/*
 * gfx_gl33.c - OpenGL 3.3 core backend.
 *
 * The only user-pass surface is an FBO: a pass target of {0} renders into an
 * internal window-sized texture, and end_frame presents it with a fullscreen
 * triangle draw that flips v (never a blit - blit sRGB semantics diverge
 * across drivers; texture decode and fragment-output encode do not).  The
 * canonical y-up clip space is reached by negating y in the injected rc_clip
 * and inverting front-face winding globally; viewport and scissor pass
 * through unconverted (see the design derivation).
 *
 * Vertex layout state lives in a VAO cache keyed on (pipeline, vertex buffer
 * handles, offsets), populated lazily at draw time and evicted on buffer or
 * pipeline destruction.  Playback keeps a small shadow of GL state (program,
 * per-unit textures and samplers, UBO ranges) for redundancy filtering.
 */

#include "gfx_gl33_internal.h"

#include <stdio.h>

#include "richc/arena.h"
#include "richc/hash.h"
#include "richc/ops.h"

/* ---- VAO cache ---- */

typedef struct gl_vao_key {
    rc_gfx_pipeline pipeline;
    rc_gfx_buffer   vbufs[RC_GFX_MAX_VERTEX_BUFFERS];
    uint32_t        vbuf_offsets[RC_GFX_MAX_VERTEX_BUFFERS];
} gl_vao_key;

// the key is hashed and compared as raw bytes, so it must stay padding-free
_Static_assert(sizeof(gl_vao_key) == (2u + 2u * RC_GFX_MAX_VERTEX_BUFFERS + RC_GFX_MAX_VERTEX_BUFFERS) * sizeof(uint32_t),
               "gl_vao_key must have no padding");

typedef struct gl_vao_entry {
    gl_vao_key key;
    GLuint     vao;
    GLuint     element_buffer;   /* element binding stored in the VAO */
} gl_vao_entry;

#define RC_ARRAY_TYPE gl_vao_entry
#define RC_ARRAY_NAME gl_vao_entry
#include "richc/template/array.h"

#define RC_TRIE_KEY_TYPE   gl_vao_key
#define RC_TRIE_VALUE_TYPE uint32_t
#define RC_TRIE_HASH(k)    ((uint64_t)rc_hash_bytes(&(k), (uint32_t)sizeof(gl_vao_key)))
#define RC_TRIE_EQUAL(a, b) (memcmp(&(a), &(b), sizeof(gl_vao_key)) == 0)
#define RC_TRIE_NAME       gl_vao_trie
#include "richc/template/hash_trie.h"

/* ---- limits of the flat binding space ---- */

#define GL_MAX_UBO_UNITS_ (RC_GFX_MAX_BIND_GROUPS * RC_GFX_MAX_BINDINGS_PER_GROUP)
#define GL_MAX_TEX_UNITS_ (RC_GFX_MAX_BIND_GROUPS * RC_GFX_MAX_BINDINGS_PER_GROUP)

/* ---- backend state ---- */

static struct {
    rc_gfx_features features;
    rc_gfx_limits   limits;
    bool has_s3tc;
    bool has_s3tc_srgb;
    bool has_bptc;

    GLuint ring_buffer;

    GLuint present_program;         /* hardware-encode / passthrough variant */
    GLuint present_program_encode;  /* manual sRGB encode variant */
    GLuint empty_vao;

    /* swapchain target */
    rc_vec2i              sc_size;
    rc_gfx_texture_format sc_format;
    rc_gfx_texture_format sc_depth_format;   /* NONE => colour-only */
    uint32_t              sc_samples;
    GLuint                sc_texture;
    GLuint                sc_fbo;
    GLuint                sc_depth_rbo;      /* 0 when sc_depth_format is NONE */
    GLuint                sc_msaa_rbo;
    GLuint                sc_msaa_fbo;

    /* VAO cache: the entries array is authoritative (for eviction sweeps);
     * the trie maps key -> entry index */
    rc_array_gl_vao_entry vao_entries;
    gl_vao_trie           vao_trie;
    gl_vao_trie_pool      vao_trie_pool;

    /* GL state shadow for redundancy filtering */
    struct {
        GLuint    program;
        GLuint    vao;
        GLuint    ubo_buffer[GL_MAX_UBO_UNITS_];
        GLintptr  ubo_offset[GL_MAX_UBO_UNITS_];
        GLsizeiptr ubo_size[GL_MAX_UBO_UNITS_];
        GLuint    texture[GL_MAX_TEX_UNITS_];
        GLuint    sampler[GL_MAX_TEX_UNITS_];
        GLenum    active_unit;
    } cache;

    /* playback state, valid between pass_begin and pass_end */
    struct {
        bool     in_pass;
        bool     target_is_swapchain;
        rc_vec2i target_size;
        uint32_t color_count;
        rc_gfx_texture_format color_formats[RC_GFX_MAX_COLOR_ATTACHMENTS];
        rc_gfx_texture_format depth_format;
        uint32_t sample_count;
        rc_gfx_render_target target;        /* {0} for the swapchain */
        rc_gfx_pass_desc pass;              /* store ops applied at pass_end */

        rc_gfx_pipeline_obj        *pipeline;
        rc_gfx_shader_obj          *shader;
        rc_gfx_pipeline_layout_obj *layout;
        rc_gfx_pipeline pipeline_handle;
        bool     pipeline_dirty;
        rc_gfx_bind_group groups[RC_GFX_MAX_BIND_GROUPS];
        uint32_t group_offsets[RC_GFX_MAX_BIND_GROUPS][RC_GFX_MAX_BINDINGS_PER_GROUP];
        uint32_t group_offset_counts[RC_GFX_MAX_BIND_GROUPS];
        uint32_t groups_dirty;              /* bitmask */
        rc_gfx_buffer vbufs[RC_GFX_MAX_VERTEX_BUFFERS];
        uint32_t vbuf_offsets[RC_GFX_MAX_VERTEX_BUFFERS];
        bool     vbufs_dirty;
        rc_gfx_buffer index_buffer;
        rc_gfx_index_format index_format;
        uint32_t index_offset;
        uint32_t stencil_ref;
    } play;
} gl;

/* ---- small state helpers ---- */

static void gl_use_program(GLuint program)
{
    if (gl.cache.program != program) {
        glUseProgram(program);
        gl.cache.program = program;
    }
}

static void gl_bind_vao(GLuint vao)
{
    if (gl.cache.vao != vao) {
        glBindVertexArray(vao);
        gl.cache.vao = vao;
    }
}

static void gl_active_unit(uint32_t unit)
{
    GLenum e = GL_TEXTURE0 + unit;
    if (gl.cache.active_unit != e) {
        glActiveTexture(e);
        gl.cache.active_unit = e;
    }
}

static void gl_bind_texture_unit(uint32_t unit, GLenum target, GLuint texture)
{
    RC_ASSERT(unit < GL_MAX_TEX_UNITS_);
    if (gl.cache.texture[unit] != texture) {
        gl_active_unit(unit);
        glBindTexture(target, texture);
        gl.cache.texture[unit] = texture;
    }
}

static void gl_bind_sampler_unit(uint32_t unit, GLuint sampler)
{
    RC_ASSERT(unit < GL_MAX_TEX_UNITS_);
    if (gl.cache.sampler[unit] != sampler) {
        glBindSampler(unit, sampler);
        gl.cache.sampler[unit] = sampler;
    }
}

static void gl_bind_ubo_range(uint32_t unit, GLuint buffer, GLintptr offset, GLsizeiptr size)
{
    RC_ASSERT(unit < GL_MAX_UBO_UNITS_);
    if (gl.cache.ubo_buffer[unit] != buffer
        || gl.cache.ubo_offset[unit] != offset
        || gl.cache.ubo_size[unit] != size) {
        glBindBufferRange(GL_UNIFORM_BUFFER, unit, buffer, offset, size);
        gl.cache.ubo_buffer[unit] = buffer;
        gl.cache.ubo_offset[unit] = offset;
        gl.cache.ubo_size[unit] = size;
    }
}

/*
 * Deleting a GL object detaches it from every binding it occupied, and a
 * later glGen* may hand the freed name straight back - so any shadow entry
 * still carrying the name would make the redundancy filter skip a bind that
 * GL no longer has.  Scrub the shadow whenever an object is deleted.
 */
static void gl_cache_forget_texture(GLuint name)
{
    for (uint32_t unit = 0; unit < GL_MAX_TEX_UNITS_; unit += 1) {
        if (gl.cache.texture[unit] == name) {
            gl.cache.texture[unit] = 0;
        }
    }
}

static void gl_cache_forget_sampler(GLuint name)
{
    for (uint32_t unit = 0; unit < GL_MAX_TEX_UNITS_; unit += 1) {
        if (gl.cache.sampler[unit] == name) {
            gl.cache.sampler[unit] = 0;
        }
    }
}

static void gl_cache_forget_buffer(GLuint name)
{
    for (uint32_t unit = 0; unit < GL_MAX_UBO_UNITS_; unit += 1) {
        if (gl.cache.ubo_buffer[unit] == name) {
            gl.cache.ubo_buffer[unit] = 0;
            gl.cache.ubo_offset[unit] = 0;
            gl.cache.ubo_size[unit] = 0;
        }
    }
}

static void gl_cache_forget_program(GLuint name)
{
    if (gl.cache.program == name) {
        gl.cache.program = 0;
    }
}

/* ---- shader compilation ---- */

/*
 * The preambles absorb every backend convention: rc_clip negates y (the
 * present draw flips the image back), and RC_STORAGE_LOAD hides the TBO
 * texelFetch behind the portable macro.  Depth has two variants: with
 * ARB_clip_control the init-time glClipControl(GL_LOWER_LEFT, GL_ZERO_TO_ONE)
 * makes GL consume canonical [0,1] z natively; without it rc_clip rewrites z
 * to GL's [-1,1] as 2z - w, which costs reverse-Z precision at distance
 * (catastrophic cancellation where z << w) but keeps user math identical.
 * The clip origin stays GL_LOWER_LEFT in both cases so the y path never
 * varies.  The trailing #line 1 keeps driver error messages pointing at user
 * source.
 */
static const char gl_vs_preamble_native[] =
    "#version 330 core\n"
    "#define RC_GFX_BACKEND_GL33 1\n"
    "#define RC_STORAGE_LOAD(name, i) texelFetch(name, int(i))\n"
    "vec4 rc_clip(vec4 p) { return vec4(p.x, -p.y, p.z, p.w); }\n"
    "#line 1\n";

static const char gl_vs_preamble_remap[] =
    "#version 330 core\n"
    "#define RC_GFX_BACKEND_GL33 1\n"
    "#define RC_STORAGE_LOAD(name, i) texelFetch(name, int(i))\n"
    "vec4 rc_clip(vec4 p) { return vec4(p.x, -p.y, 2.0 * p.z - p.w, p.w); }\n"
    "#line 1\n";

static const char gl_fs_preamble[] =
    "#version 330 core\n"
    "#define RC_GFX_BACKEND_GL33 1\n"
    "#define RC_STORAGE_LOAD(name, i) texelFetch(name, int(i))\n"
    "#line 1\n";

static GLuint gl_compile_stage(GLenum stage, const char *preamble, rc_str source)
{
    GLuint shader = glCreateShader(stage);
    const GLchar *strings[2] = {preamble, source.data};
    GLint lengths[2] = {(GLint)strlen(preamble), (GLint)source.len};
    glShaderSource(shader, 2, strings, lengths);
    glCompileShader(shader);

    GLint ok = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[2048];
        glGetShaderInfoLog(shader, (GLsizei)sizeof(log), NULL, log);
        fprintf(stderr, "rc_gfx: %s shader compile failed:\n%s\n",
                stage == GL_VERTEX_SHADER ? "vertex" : "fragment", log);
        RC_PANIC(false);
    }
    return shader;
}

static GLuint gl_link_program(rc_str vs_source, rc_str fs_source,
                              const char *vs_preamble, const char *fs_preamble)
{
    GLuint vs = gl_compile_stage(GL_VERTEX_SHADER, vs_preamble, vs_source);
    GLuint fs = gl_compile_stage(GL_FRAGMENT_SHADER, fs_preamble, fs_source);
    GLuint program = glCreateProgram();
    glAttachShader(program, vs);
    glAttachShader(program, fs);
    glLinkProgram(program);
    glDeleteShader(vs);
    glDeleteShader(fs);

    GLint ok = GL_FALSE;
    glGetProgramiv(program, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[2048];
        glGetProgramInfoLog(program, (GLsizei)sizeof(log), NULL, log);
        fprintf(stderr, "rc_gfx: program link failed:\n%s\n", log);
        RC_PANIC(false);
    }
    return program;
}

/* ---- init / shutdown ---- */

static bool gl_has_extension(const char *name)
{
    GLint count = 0;
    glGetIntegerv(GL_NUM_EXTENSIONS, &count);
    for (GLint i = 0; i < count; i += 1) {
        const char *ext = (const char *)glGetStringi(GL_EXTENSIONS, (GLuint)i);
        if (ext != NULL && strcmp(ext, name) == 0) {
            return true;
        }
    }
    return false;
}

static const char gl_present_vs[] =
    "#version 330 core\n"
    "out vec2 v_uv;\n"
    "void main() {\n"
    "    vec2 pos = vec2(float((gl_VertexID << 1) & 2), float(gl_VertexID & 2));\n"
    "    v_uv = vec2(pos.x, 1.0 - pos.y);\n"   /* the one place the y flip is materialised */
    "    gl_Position = vec4(pos * 2.0 - 1.0, 0.0, 1.0);\n"
    "}\n";

static const char gl_present_fs[] =
    "#version 330 core\n"
    "uniform sampler2D u_image;\n"
    "in vec2 v_uv;\n"
    "out vec4 o_color;\n"
    "void main() { o_color = texture(u_image, v_uv); }\n";

/* Exact piecewise encode, for the rare default framebuffer that reports no
 * sRGB encoding of its own. */
static const char gl_present_fs_encode[] =
    "#version 330 core\n"
    "uniform sampler2D u_image;\n"
    "in vec2 v_uv;\n"
    "out vec4 o_color;\n"
    "vec3 encode(vec3 l) {\n"
    "    vec3 lo = l * 12.92;\n"
    "    vec3 hi = 1.055 * pow(l, vec3(1.0 / 2.4)) - 0.055;\n"
    "    return mix(lo, hi, step(vec3(0.0031308), l));\n"
    "}\n"
    "void main() {\n"
    "    vec4 c = texture(u_image, v_uv);\n"
    "    o_color = vec4(encode(c.rgb), c.a);\n"
    "}\n";

rc_gfx_features rc_gfx_backend_init(uint32_t ring_size_total, uint32_t *out_ring_buffer)
{
    gl.has_s3tc = gl_has_extension("GL_EXT_texture_compression_s3tc");
    gl.has_s3tc_srgb = gl.has_s3tc && gl_has_extension("GL_EXT_texture_sRGB");
    gl.has_bptc = gl_has_extension("GL_ARB_texture_compression_bptc");
    bool has_aniso = gl_has_extension("GL_EXT_texture_filter_anisotropic")
                  || gl_has_extension("GL_ARB_texture_filter_anisotropic");
    bool has_clip_control = GLAD_GL_ARB_clip_control && glClipControl != NULL;

    // global state set once; everything else is per-pipeline or per-pass
    glEnable(GL_FRAMEBUFFER_SRGB);        // format-driven, so a no-op on non-sRGB targets
    glFrontFace(GL_CW);                   // canonical CCW + rc_clip y negation
    glDepthRange(0.0, 1.0);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glEnable(GL_SCISSOR_TEST);            // always on; passes set the full-target rect
    glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);
    if (has_clip_control) {
        // consume canonical [0,1] depth natively, preserving the reverse-Z
        // precision; the origin stays LOWER_LEFT so the rc_clip y negation is
        // identical whether or not the extension is present
        glClipControl(GL_LOWER_LEFT, GL_ZERO_TO_ONE);
    }

    GLint encoding = 0;
    glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER, GL_BACK_LEFT,
        GL_FRAMEBUFFER_ATTACHMENT_COLOR_ENCODING, &encoding);

    gl.features = (rc_gfx_features) {
        .storage_buffers_via_tbo = true,
        .native_depth_zero_to_one = has_clip_control,
        .anisotropic_filtering = has_aniso,
        .srgb_default_framebuffer = encoding == GL_SRGB,
        .timer_queries = true,
    };

    GLint v = 0;
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &v);
    gl.limits.max_texture_size_2d = (uint32_t)v;
    glGetIntegerv(GL_MAX_3D_TEXTURE_SIZE, &v);
    gl.limits.max_texture_size_3d = (uint32_t)v;
    glGetIntegerv(GL_MAX_CUBE_MAP_TEXTURE_SIZE, &v);
    gl.limits.max_texture_size_cube = (uint32_t)v;
    glGetIntegerv(GL_MAX_ARRAY_TEXTURE_LAYERS, &v);
    gl.limits.max_texture_array_layers = (uint32_t)v;
    glGetIntegerv(GL_MAX_COLOR_ATTACHMENTS, &v);
    gl.limits.max_color_attachments = (uint32_t)rc_min_i32(v, RC_GFX_MAX_COLOR_ATTACHMENTS);
    glGetIntegerv(GL_MAX_VERTEX_ATTRIBS, &v);
    gl.limits.max_vertex_attributes = (uint32_t)rc_min_i32(v, RC_GFX_MAX_VERTEX_ATTRIBUTES);
    glGetIntegerv(GL_MAX_UNIFORM_BLOCK_SIZE, &v);
    gl.limits.max_uniform_buffer_range = (uint32_t)v;
    glGetIntegerv(GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT, &v);
    gl.limits.uniform_buffer_offset_alignment = (uint32_t)v;
    glGetIntegerv(GL_MAX_SAMPLES, &v);
    gl.limits.max_msaa_samples = (uint32_t)v;
    if (has_aniso) {
        glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &gl.limits.max_anisotropy);
    } else {
        gl.limits.max_anisotropy = 1.0f;
    }

    // uniform ring: one buffer, RC_GFX_FRAMES_IN_FLIGHT rotating regions
    glGenBuffers(1, &gl.ring_buffer);
    glBindBuffer(GL_COPY_WRITE_BUFFER, gl.ring_buffer);
    glBufferData(GL_COPY_WRITE_BUFFER, ring_size_total, NULL, GL_STREAM_DRAW);
    *out_ring_buffer = gl.ring_buffer;

    // present programs and the attribute-less VAO for the fullscreen triangle
    gl.present_program = gl_link_program(
        rc_str_make(gl_present_vs, (uint32_t)sizeof(gl_present_vs) - 1),
        rc_str_make(gl_present_fs, (uint32_t)sizeof(gl_present_fs) - 1), "", "");
    gl.present_program_encode = gl_link_program(
        rc_str_make(gl_present_vs, (uint32_t)sizeof(gl_present_vs) - 1),
        rc_str_make(gl_present_fs_encode, (uint32_t)sizeof(gl_present_fs_encode) - 1), "", "");
    gl_use_program(gl.present_program);
    glUniform1i(glGetUniformLocation(gl.present_program, "u_image"), 0);
    gl_use_program(gl.present_program_encode);
    glUniform1i(glGetUniformLocation(gl.present_program_encode, "u_image"), 0);
    glGenVertexArrays(1, &gl.empty_vao);

    return gl.features;
}

void rc_gfx_backend_shutdown(void)
{
    for (uint32_t i = 0; i < gl.vao_entries.num; i += 1) {
        glDeleteVertexArrays(1, &rc_array_gl_vao_entry_at(&gl.vao_entries, i)->vao);
    }
    glDeleteVertexArrays(1, &gl.empty_vao);
    glDeleteProgram(gl.present_program);
    glDeleteProgram(gl.present_program_encode);
    glDeleteBuffers(1, &gl.ring_buffer);
    glDeleteTextures(1, &gl.sc_texture);
    glDeleteFramebuffers(1, &gl.sc_fbo);
    glDeleteRenderbuffers(1, &gl.sc_depth_rbo);
    glDeleteRenderbuffers(1, &gl.sc_msaa_rbo);
    glDeleteFramebuffers(1, &gl.sc_msaa_fbo);
    memset(&gl, 0, sizeof(gl));
}

rc_gfx_limits rc_gfx_backend_limits(void)
{
    return gl.limits;
}

rc_str rc_gfx_backend_name(void)
{
    return RC_STR("OpenGL 3.3");
}

uint32_t rc_gfx_backend_format_caps(rc_gfx_texture_format fmt)
{
    uint32_t all = RC_GFX_FORMAT_CAP_SAMPLE | RC_GFX_FORMAT_CAP_FILTER | RC_GFX_FORMAT_CAP_RENDER
                 | RC_GFX_FORMAT_CAP_BLEND | RC_GFX_FORMAT_CAP_MSAA | RC_GFX_FORMAT_CAP_RESOLVE;
    switch (fmt) {
    case RC_GFX_TEXTURE_FORMAT_NONE:
        return 0;
    case RC_GFX_TEXTURE_FORMAT_R8_UINT:
    case RC_GFX_TEXTURE_FORMAT_RGBA8_UINT:
    case RC_GFX_TEXTURE_FORMAT_R16_UINT:
    case RC_GFX_TEXTURE_FORMAT_RG16_UINT:
    case RC_GFX_TEXTURE_FORMAT_R32_UINT:
    case RC_GFX_TEXTURE_FORMAT_RGBA32_UINT:
        // integer formats: no filtering, no blending
        return RC_GFX_FORMAT_CAP_SAMPLE | RC_GFX_FORMAT_CAP_RENDER | RC_GFX_FORMAT_CAP_MSAA;
    case RC_GFX_TEXTURE_FORMAT_R32F:
    case RC_GFX_TEXTURE_FORMAT_RG32F:
    case RC_GFX_TEXTURE_FORMAT_RGBA32F:
        // conservatively unfilterable, matching the portable (WebGPU) baseline
        return all & ~(uint32_t)RC_GFX_FORMAT_CAP_FILTER;
    case RC_GFX_TEXTURE_FORMAT_DEPTH16_UNORM:
    case RC_GFX_TEXTURE_FORMAT_DEPTH24_PLUS:
    case RC_GFX_TEXTURE_FORMAT_DEPTH32F:
    case RC_GFX_TEXTURE_FORMAT_DEPTH24_PLUS_STENCIL8:
    case RC_GFX_TEXTURE_FORMAT_DEPTH32F_STENCIL8:
        return RC_GFX_FORMAT_CAP_SAMPLE | RC_GFX_FORMAT_CAP_RENDER | RC_GFX_FORMAT_CAP_MSAA;
    case RC_GFX_TEXTURE_FORMAT_BC1_RGBA_UNORM:
    case RC_GFX_TEXTURE_FORMAT_BC3_RGBA_UNORM:
        return gl.has_s3tc ? (RC_GFX_FORMAT_CAP_SAMPLE | RC_GFX_FORMAT_CAP_FILTER) : 0;
    case RC_GFX_TEXTURE_FORMAT_BC1_RGBA_SRGB:
    case RC_GFX_TEXTURE_FORMAT_BC3_RGBA_SRGB:
        return gl.has_s3tc_srgb ? (RC_GFX_FORMAT_CAP_SAMPLE | RC_GFX_FORMAT_CAP_FILTER) : 0;
    case RC_GFX_TEXTURE_FORMAT_BC4_R_UNORM:
    case RC_GFX_TEXTURE_FORMAT_BC5_RG_UNORM:
        // RGTC is core in GL 3.0
        return RC_GFX_FORMAT_CAP_SAMPLE | RC_GFX_FORMAT_CAP_FILTER;
    case RC_GFX_TEXTURE_FORMAT_BC6H_RGB_FLOAT:
    case RC_GFX_TEXTURE_FORMAT_BC7_RGBA_UNORM:
    case RC_GFX_TEXTURE_FORMAT_BC7_RGBA_SRGB:
        return gl.has_bptc ? (RC_GFX_FORMAT_CAP_SAMPLE | RC_GFX_FORMAT_CAP_FILTER) : 0;
    default:
        return all;
    }
}

/* ---- frame lifecycle ---- */

void rc_gfx_backend_begin_frame(rc_vec2i size, rc_gfx_texture_format format,
                                rc_gfx_texture_format depth_format, uint32_t sample_count)
{
    if (size.x == gl.sc_size.x && size.y == gl.sc_size.y
        && format == gl.sc_format && depth_format == gl.sc_depth_format
        && sample_count == gl.sc_samples) {
        return;
    }
    gl_cache_forget_texture(gl.sc_texture);
    glDeleteTextures(1, &gl.sc_texture);
    glDeleteFramebuffers(1, &gl.sc_fbo);
    glDeleteRenderbuffers(1, &gl.sc_depth_rbo);
    glDeleteRenderbuffers(1, &gl.sc_msaa_rbo);
    glDeleteFramebuffers(1, &gl.sc_msaa_fbo);
    gl.sc_depth_rbo = 0;
    gl.sc_msaa_rbo = 0;
    gl.sc_msaa_fbo = 0;

    rc_gfx_gl_format glfmt = rc_gfx_gl_format_get(format);
    glGenTextures(1, &gl.sc_texture);
    gl_bind_texture_unit(0, GL_TEXTURE_2D, gl.sc_texture);
    glTexImage2D(GL_TEXTURE_2D, 0, (GLint)glfmt.internal_format, size.x, size.y, 0,
                 glfmt.format, glfmt.type, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    // the optional depth buffer lives as a renderbuffer on whichever FBO the
    // user's passes render into (the MSAA one when multisampling); the resolve
    // and present paths are colour-only, so no other FBO carries depth
    if (depth_format != RC_GFX_TEXTURE_FORMAT_NONE) {
        rc_gfx_gl_format gldepth = rc_gfx_gl_format_get(depth_format);
        glGenRenderbuffers(1, &gl.sc_depth_rbo);
        glBindRenderbuffer(GL_RENDERBUFFER, gl.sc_depth_rbo);
        if (sample_count > 1) {
            glRenderbufferStorageMultisample(GL_RENDERBUFFER, (GLsizei)sample_count,
                                             gldepth.internal_format, size.x, size.y);
        } else {
            glRenderbufferStorage(GL_RENDERBUFFER, gldepth.internal_format, size.x, size.y);
        }
    }
    GLenum depth_attachment = rc_gfx_texture_format_is_stencil(depth_format)
        ? GL_DEPTH_STENCIL_ATTACHMENT
        : GL_DEPTH_ATTACHMENT;

    glGenFramebuffers(1, &gl.sc_fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, gl.sc_fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, gl.sc_texture, 0);
    if (gl.sc_depth_rbo != 0 && sample_count <= 1) {
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, depth_attachment, GL_RENDERBUFFER, gl.sc_depth_rbo);
    }
    RC_PANIC(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);

    if (sample_count > 1) {
        glGenRenderbuffers(1, &gl.sc_msaa_rbo);
        glBindRenderbuffer(GL_RENDERBUFFER, gl.sc_msaa_rbo);
        glRenderbufferStorageMultisample(GL_RENDERBUFFER, (GLsizei)sample_count,
                                         glfmt.internal_format, size.x, size.y);
        glGenFramebuffers(1, &gl.sc_msaa_fbo);
        glBindFramebuffer(GL_FRAMEBUFFER, gl.sc_msaa_fbo);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, gl.sc_msaa_rbo);
        if (gl.sc_depth_rbo != 0) {
            glFramebufferRenderbuffer(GL_FRAMEBUFFER, depth_attachment, GL_RENDERBUFFER, gl.sc_depth_rbo);
        }
        RC_PANIC(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);
    }

    gl.sc_size = size;
    gl.sc_format = format;
    gl.sc_depth_format = depth_format;
    gl.sc_samples = sample_count;
}

/* Raw raster state for internal draws (present); the pipeline shadow is
 * invalidated so the next user pipeline reapplies everything. */
static void gl_raw_raster_state(void)
{
    glDisable(GL_BLEND);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_STENCIL_TEST);
    glDisable(GL_CULL_FACE);
    glDisable(GL_SAMPLE_ALPHA_TO_COVERAGE);
    glDisable(GL_POLYGON_OFFSET_FILL);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    gl.play.pipeline = NULL;
    gl.play.pipeline_handle = (rc_gfx_pipeline) {0};
}

void rc_gfx_backend_end_frame(rc_gfx_color_space color_space)
{
    RC_ASSERT(!gl.play.in_pass);

    // a multisampled swapchain resolves into the single-sample final target
    // first, so the present draw always samples a plain 2D texture
    if (gl.sc_samples > 1) {
        glBindFramebuffer(GL_READ_FRAMEBUFFER, gl.sc_msaa_fbo);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, gl.sc_fbo);
        glScissor(0, 0, gl.sc_size.x, gl.sc_size.y);
        glBlitFramebuffer(0, 0, gl.sc_size.x, gl.sc_size.y,
                          0, 0, gl.sc_size.x, gl.sc_size.y,
                          GL_COLOR_BUFFER_BIT, GL_NEAREST);
    }

    // present: sample the final target into the default framebuffer, flipping
    // v in the vertex shader.  Encoding: sRGB colour space uses the hardware
    // encode when the default framebuffer reports sRGB, the shader encode
    // otherwise; LINEAR colour space disables the write-side encode entirely
    // so the presented bytes are the rendered bytes.
    bool manual_encode = color_space == RC_GFX_COLOR_SPACE_SRGB && !gl.features.srgb_default_framebuffer;
    if (color_space == RC_GFX_COLOR_SPACE_LINEAR) {
        glDisable(GL_FRAMEBUFFER_SRGB);
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, gl.sc_size.x, gl.sc_size.y);
    glScissor(0, 0, gl.sc_size.x, gl.sc_size.y);
    gl_raw_raster_state();
    gl_use_program(manual_encode ? gl.present_program_encode : gl.present_program);
    gl_bind_texture_unit(0, GL_TEXTURE_2D, gl.sc_texture);
    gl_bind_sampler_unit(0, 0);
    gl_bind_vao(gl.empty_vao);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    if (color_space == RC_GFX_COLOR_SPACE_LINEAR) {
        glEnable(GL_FRAMEBUFFER_SRGB);
    }
}

/* ---- buffers ---- */

static GLenum gl_buffer_usage_hint(rc_gfx_buffer_update_mode update)
{
    switch (update) {
    case RC_GFX_BUFFER_UPDATE_IMMUTABLE: return GL_STATIC_DRAW;
    case RC_GFX_BUFFER_UPDATE_DYNAMIC:   return GL_DYNAMIC_DRAW;
    case RC_GFX_BUFFER_UPDATE_STREAM:    return GL_STREAM_DRAW;
    default:
        RC_UNREACHABLE();
    }
}

void rc_gfx_backend_make_buffer(rc_gfx_buffer_obj *obj, rc_view_bytes data)
{
    glGenBuffers(1, &obj->gl_buffer);
    // COPY_WRITE_BUFFER for all data operations: it never disturbs VAO
    // element bindings or the vertex bind point
    glBindBuffer(GL_COPY_WRITE_BUFFER, obj->gl_buffer);
    glBufferData(GL_COPY_WRITE_BUFFER, obj->size, NULL, gl_buffer_usage_hint(obj->update));
    if (data.data != NULL && data.num > 0) {
        glBufferSubData(GL_COPY_WRITE_BUFFER, 0, data.num, data.data);
    }
}

void rc_gfx_backend_update_buffer(rc_gfx_buffer_obj *obj, uint32_t offset, rc_view_bytes data)
{
    glBindBuffer(GL_COPY_WRITE_BUFFER, obj->gl_buffer);
    glBufferSubData(GL_COPY_WRITE_BUFFER, (GLintptr)offset, (GLsizeiptr)data.num, data.data);
}

void rc_gfx_backend_flush_uniforms(uint32_t offset, rc_view_bytes data)
{
    glBindBuffer(GL_COPY_WRITE_BUFFER, gl.ring_buffer);
    glBufferSubData(GL_COPY_WRITE_BUFFER, (GLintptr)offset, (GLsizeiptr)data.num, data.data);
}

/* ---- textures ---- */

static GLenum gl_texture_target(rc_gfx_texture_dim dim, uint32_t sample_count)
{
    if (sample_count > 1) {
        return GL_TEXTURE_2D_MULTISAMPLE;
    }
    switch (dim) {
    case RC_GFX_TEXTURE_DIM_2D:       return GL_TEXTURE_2D;
    case RC_GFX_TEXTURE_DIM_2D_ARRAY: return GL_TEXTURE_2D_ARRAY;
    case RC_GFX_TEXTURE_DIM_3D:       return GL_TEXTURE_3D;
    case RC_GFX_TEXTURE_DIM_CUBE:     return GL_TEXTURE_CUBE_MAP;
    default:
        RC_UNREACHABLE();
    }
}

static rc_vec2i gl_mip_size(rc_vec2i size, uint32_t mip)
{
    return rc_vec2i_make(rc_max_i32(1, size.x >> mip), rc_max_i32(1, size.y >> mip));
}

void rc_gfx_backend_make_texture(rc_gfx_texture_obj *obj, const rc_gfx_texture_desc *desc)
{
    rc_gfx_gl_format glfmt = rc_gfx_gl_format_get(obj->format);
    bool compressed = rc_gfx_texture_format_is_compressed(obj->format);
    obj->gl_target = gl_texture_target(obj->dim, obj->sample_count);

    glGenTextures(1, &obj->gl_texture);
    gl_bind_texture_unit(0, obj->gl_target, obj->gl_texture);

    if (obj->sample_count > 1) {
        glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, (GLsizei)obj->sample_count,
                                glfmt.internal_format, obj->size.x, obj->size.y, GL_TRUE);
        gl.cache.texture[0] = 0;   // multisample targets rebind loosely; drop the shadow
        return;
    }

    glTexParameteri(obj->gl_target, GL_TEXTURE_MAX_LEVEL, (GLint)(obj->mip_count - 1));

    const rc_view_bytes *sub = desc->data.subresources;
    for (uint32_t mip = 0; mip < obj->mip_count; mip += 1) {
        rc_vec2i size = gl_mip_size(obj->size, mip);
        switch (obj->dim) {
        case RC_GFX_TEXTURE_DIM_2D: {
            const void *data = sub != NULL ? sub[mip].data : NULL;
            if (compressed) {
                RC_ASSERT(data != NULL);   // compressed textures need initial data on GL 3.3
                glCompressedTexImage2D(GL_TEXTURE_2D, (GLint)mip, glfmt.internal_format,
                                       size.x, size.y, 0, (GLsizei)sub[mip].num, data);
            } else {
                glTexImage2D(GL_TEXTURE_2D, (GLint)mip, (GLint)glfmt.internal_format,
                             size.x, size.y, 0, glfmt.format, glfmt.type, data);
            }
            break;
        }
        case RC_GFX_TEXTURE_DIM_CUBE: {
            for (uint32_t face = 0; face < 6; face += 1) {
                const void *data = sub != NULL ? sub[face * obj->mip_count + mip].data : NULL;
                GLenum target = GL_TEXTURE_CUBE_MAP_POSITIVE_X + face;
                if (compressed) {
                    RC_ASSERT(data != NULL);
                    glCompressedTexImage2D(target, (GLint)mip, glfmt.internal_format,
                                           size.x, size.y, 0,
                                           (GLsizei)sub[face * obj->mip_count + mip].num, data);
                } else {
                    glTexImage2D(target, (GLint)mip, (GLint)glfmt.internal_format,
                                 size.x, size.y, 0, glfmt.format, glfmt.type, data);
                }
            }
            break;
        }
        case RC_GFX_TEXTURE_DIM_2D_ARRAY: {
            RC_ASSERT(!compressed);
            glTexImage3D(GL_TEXTURE_2D_ARRAY, (GLint)mip, (GLint)glfmt.internal_format,
                         size.x, size.y, (GLsizei)obj->depth, 0, glfmt.format, glfmt.type, NULL);
            if (sub != NULL) {
                for (uint32_t slice = 0; slice < obj->depth; slice += 1) {
                    glTexSubImage3D(GL_TEXTURE_2D_ARRAY, (GLint)mip, 0, 0, (GLint)slice,
                                    size.x, size.y, 1, glfmt.format, glfmt.type,
                                    sub[slice * obj->mip_count + mip].data);
                }
            }
            break;
        }
        case RC_GFX_TEXTURE_DIM_3D: {
            RC_ASSERT(!compressed);
            GLsizei depth = (GLsizei)rc_max_i32(1, (int32_t)obj->depth >> mip);
            const void *data = sub != NULL ? sub[mip].data : NULL;
            glTexImage3D(GL_TEXTURE_3D, (GLint)mip, (GLint)glfmt.internal_format,
                         size.x, size.y, depth, 0, glfmt.format, glfmt.type, data);
            break;
        }
        default:
            RC_UNREACHABLE();
        }
    }
}

void rc_gfx_backend_update_texture(rc_gfx_texture_obj *obj, uint32_t mip, uint32_t slice,
                                   rc_box2i region, rc_view_bytes data)
{
    rc_gfx_gl_format glfmt = rc_gfx_gl_format_get(obj->format);
    rc_vec2i pos = rc_box2i_min(region);
    rc_vec2i size = rc_box2i_size(region);
    gl_bind_texture_unit(0, obj->gl_target, obj->gl_texture);
    switch (obj->dim) {
    case RC_GFX_TEXTURE_DIM_2D:
        glTexSubImage2D(GL_TEXTURE_2D, (GLint)mip, pos.x, pos.y, size.x, size.y,
                        glfmt.format, glfmt.type, data.data);
        break;
    case RC_GFX_TEXTURE_DIM_CUBE:
        glTexSubImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + slice, (GLint)mip,
                        pos.x, pos.y, size.x, size.y, glfmt.format, glfmt.type, data.data);
        break;
    case RC_GFX_TEXTURE_DIM_2D_ARRAY:
    case RC_GFX_TEXTURE_DIM_3D:
        glTexSubImage3D(obj->gl_target, (GLint)mip, pos.x, pos.y, (GLint)slice,
                        size.x, size.y, 1, glfmt.format, glfmt.type, data.data);
        break;
    default:
        RC_UNREACHABLE();
    }
}

void rc_gfx_backend_generate_mipmaps(rc_gfx_texture_obj *obj)
{
    gl_bind_texture_unit(0, obj->gl_target, obj->gl_texture);
    glGenerateMipmap(obj->gl_target);
}

uint32_t rc_gfx_backend_make_tbo(uint32_t gl_buffer, rc_gfx_texture_format texel_format)
{
    rc_gfx_gl_format glfmt = rc_gfx_gl_format_get(texel_format);
    GLuint tbo = 0;
    glGenTextures(1, &tbo);
    gl_bind_texture_unit(0, GL_TEXTURE_BUFFER, tbo);
    glTexBuffer(GL_TEXTURE_BUFFER, glfmt.internal_format, gl_buffer);
    gl.cache.texture[0] = 0;   // target mismatch with later 2D binds; drop the shadow
    return tbo;
}

/* ---- samplers ---- */

void rc_gfx_backend_make_sampler(rc_gfx_sampler_obj *obj, const rc_gfx_sampler_desc *desc)
{
    glGenSamplers(1, &obj->gl_sampler);
    GLuint s = obj->gl_sampler;
    glSamplerParameteri(s, GL_TEXTURE_MIN_FILTER,
                        (GLint)rc_gfx_gl_min_filter(desc->min_filter, desc->mip_filter));
    glSamplerParameteri(s, GL_TEXTURE_MAG_FILTER,
                        desc->mag_filter == RC_GFX_FILTER_NEAREST ? GL_NEAREST : GL_LINEAR);
    glSamplerParameteri(s, GL_TEXTURE_WRAP_S, (GLint)rc_gfx_gl_address(desc->address_u));
    glSamplerParameteri(s, GL_TEXTURE_WRAP_T, (GLint)rc_gfx_gl_address(desc->address_v));
    glSamplerParameteri(s, GL_TEXTURE_WRAP_R, (GLint)rc_gfx_gl_address(desc->address_w));
    glSamplerParameterf(s, GL_TEXTURE_MIN_LOD, desc->lod_min);
    glSamplerParameterf(s, GL_TEXTURE_MAX_LOD, desc->lod_max != 0.0f ? desc->lod_max : 1000.0f);
    if (desc->compare != RC_GFX_COMPARE_ALWAYS) {
        glSamplerParameteri(s, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
        glSamplerParameteri(s, GL_TEXTURE_COMPARE_FUNC, (GLint)rc_gfx_gl_compare(desc->compare));
    }
    if (desc->max_anisotropy > 1 && gl.features.anisotropic_filtering) {
        float amount = rc_min_f32((float)desc->max_anisotropy, gl.limits.max_anisotropy);
        glSamplerParameterf(s, GL_TEXTURE_MAX_ANISOTROPY_EXT, amount);
    }
    float border[4] = {desc->border_color.x, desc->border_color.y,
                       desc->border_color.z, desc->border_color.w};
    glSamplerParameterfv(s, GL_TEXTURE_BORDER_COLOR, border);
}

/* ---- shaders ---- */

void rc_gfx_backend_make_shader(rc_gfx_shader_obj *obj, const rc_gfx_shader_desc *desc)
{
    const char *vs_preamble = gl.features.native_depth_zero_to_one
        ? gl_vs_preamble_native
        : gl_vs_preamble_remap;
    obj->gl_program = gl_link_program(desc->vs_source, desc->fs_source,
                                      vs_preamble, gl_fs_preamble);
    GLuint program = obj->gl_program;

    for (uint32_t i = 0; i < obj->block_count; i += 1) {
        const rc_gfx_shader_uniform_block *block = &desc->uniform_blocks[i];
        char name[128];
        const char *cname = rc_str_as_cstr(block->glsl_name, name, (uint32_t)sizeof(name));
        GLuint index = glGetUniformBlockIndex(program, cname);
        RC_ASSERT(index != GL_INVALID_INDEX);
        obj->blocks[i].gl_block_index = index;

        if (rc_gfx_internal_validation()) {
            // std140 validation: the GL-computed block layout must match the
            // C struct, or uniform writes silently corrupt (see the design)
            GLint data_size = 0;
            glGetActiveUniformBlockiv(program, index, GL_UNIFORM_BLOCK_DATA_SIZE, &data_size);
            // a C struct smaller than the std140 layout is the padding bug
            // this exists to catch; larger is only over-allocation (drivers
            // may round the reported size up, so equality would be too strict)
            RC_ASSERT(block->size >= (uint32_t)data_size);
            for (uint32_t m = 0; m < block->member_count; m += 1) {
                char member_name[128];
                const char *cmember = rc_str_as_cstr(block->members[m].name, member_name,
                                                     (uint32_t)sizeof(member_name));
                GLuint uniform_index = GL_INVALID_INDEX;
                glGetUniformIndices(program, 1, &cmember, &uniform_index);
                RC_ASSERT(uniform_index != GL_INVALID_INDEX);
                GLint offset = -1;
                glGetActiveUniformsiv(program, 1, &uniform_index, GL_UNIFORM_OFFSET, &offset);
                RC_ASSERT((uint32_t)offset == block->members[m].offset);
            }
        }
    }

    for (uint32_t i = 0; i < obj->pair_count; i += 1) {
        char name[128];
        const char *cname = rc_str_as_cstr(desc->texture_samplers[i].glsl_name, name,
                                           (uint32_t)sizeof(name));
        // -1 when the uniform was optimised out; resolve skips it
        obj->pairs[i].gl_location = glGetUniformLocation(program, cname);
    }
}

/* Index of the entry with the given binding within a group layout. */
static uint32_t gl_layout_entry_index(const rc_gfx_bind_group_layout_obj *group, uint32_t binding)
{
    for (uint32_t e = 0; e < group->entry_count; e += 1) {
        if (group->entries[e].binding == binding) {
            return e;
        }
    }
    return RC_INDEX_NONE;
}

void rc_gfx_backend_resolve_shader(rc_gfx_shader_obj *shd, const rc_gfx_pipeline_layout_obj *layout)
{
    GLuint program = shd->gl_program;
    for (uint32_t i = 0; i < shd->block_count; i += 1) {
        const rc_gfx_shader_block_info *block = &shd->blocks[i];
        RC_ASSERT(block->group < layout->group_count);
        rc_gfx_bind_group_layout_obj *group = rc_gfx_resolve_bind_group_layout(layout->groups[block->group]);
        uint32_t entry = gl_layout_entry_index(group, block->binding);
        RC_ASSERT(entry != RC_INDEX_NONE);
        glUniformBlockBinding(program, block->gl_block_index, layout->flat_unit[block->group][entry]);
    }
    gl_use_program(program);
    for (uint32_t i = 0; i < shd->pair_count; i += 1) {
        const rc_gfx_shader_pair_info *pair = &shd->pairs[i];
        if (pair->gl_location < 0) {
            continue;
        }
        RC_ASSERT(pair->texture_group < layout->group_count);
        rc_gfx_bind_group_layout_obj *group = rc_gfx_resolve_bind_group_layout(layout->groups[pair->texture_group]);
        uint32_t entry = gl_layout_entry_index(group, pair->texture_binding);
        RC_ASSERT(entry != RC_INDEX_NONE);
        RC_ASSERT(group->entries[entry].type == RC_GFX_BINDING_TEXTURE
                  || group->entries[entry].type == RC_GFX_BINDING_STORAGE_BUFFER_READ);
        glUniform1i(pair->gl_location, (GLint)layout->flat_unit[pair->texture_group][entry]);
    }
}

/* ---- render targets ---- */

static void gl_attach(GLenum attachment, const rc_gfx_attachment *desc_attachment)
{
    rc_gfx_texture_obj *texture = rc_gfx_resolve_texture(desc_attachment->texture);
    switch (texture->dim) {
    case RC_GFX_TEXTURE_DIM_2D:
        glFramebufferTexture2D(GL_FRAMEBUFFER, attachment,
                               texture->sample_count > 1 ? GL_TEXTURE_2D_MULTISAMPLE : GL_TEXTURE_2D,
                               texture->gl_texture, (GLint)desc_attachment->mip);
        break;
    case RC_GFX_TEXTURE_DIM_CUBE:
        glFramebufferTexture2D(GL_FRAMEBUFFER, attachment,
                               GL_TEXTURE_CUBE_MAP_POSITIVE_X + desc_attachment->slice,
                               texture->gl_texture, (GLint)desc_attachment->mip);
        break;
    case RC_GFX_TEXTURE_DIM_2D_ARRAY:
    case RC_GFX_TEXTURE_DIM_3D:
        glFramebufferTextureLayer(GL_FRAMEBUFFER, attachment, texture->gl_texture,
                                  (GLint)desc_attachment->mip, (GLint)desc_attachment->slice);
        break;
    default:
        RC_UNREACHABLE();
    }
}

void rc_gfx_backend_make_render_target(rc_gfx_render_target_obj *obj,
                                       const rc_gfx_render_target_desc *desc)
{
    static const GLenum draw_buffers[RC_GFX_MAX_COLOR_ATTACHMENTS] = {
        GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2, GL_COLOR_ATTACHMENT3,
    };

    glGenFramebuffers(1, &obj->gl_fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, obj->gl_fbo);
    for (uint32_t i = 0; i < obj->color_count; i += 1) {
        gl_attach(GL_COLOR_ATTACHMENT0 + i, &desc->colors[i]);
    }
    if (obj->color_count > 0) {
        glDrawBuffers((GLsizei)obj->color_count, draw_buffers);
    } else {
        glDrawBuffer(GL_NONE);
        glReadBuffer(GL_NONE);
    }
    if (obj->depth_format != RC_GFX_TEXTURE_FORMAT_NONE) {
        GLenum attachment = rc_gfx_texture_format_is_stencil(obj->depth_format)
            ? GL_DEPTH_STENCIL_ATTACHMENT
            : GL_DEPTH_ATTACHMENT;
        gl_attach(attachment, &desc->depth_stencil);
    }
    RC_PANIC(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);

    if (obj->resolve_mask != 0) {
        glGenFramebuffers(1, &obj->gl_resolve_fbo);
        glBindFramebuffer(GL_FRAMEBUFFER, obj->gl_resolve_fbo);
        for (uint32_t i = 0; i < obj->color_count; i += 1) {
            if (obj->resolve_mask & (1u << i)) {
                gl_attach(GL_COLOR_ATTACHMENT0 + i, &desc->resolves[i]);
            }
        }
        RC_PANIC(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);
    }
}

/* ---- releases and cache eviction ---- */

void rc_gfx_backend_release(rc_gfx_release_kind kind, uint32_t name)
{
    if (name == 0) {
        return;
    }
    GLuint gl_name = name;
    switch (kind) {
    case RC_GFX_RELEASE_BUFFER:
        gl_cache_forget_buffer(gl_name);
        glDeleteBuffers(1, &gl_name);
        break;
    case RC_GFX_RELEASE_TEXTURE:
        gl_cache_forget_texture(gl_name);
        glDeleteTextures(1, &gl_name);
        break;
    case RC_GFX_RELEASE_SAMPLER:
        gl_cache_forget_sampler(gl_name);
        glDeleteSamplers(1, &gl_name);
        break;
    case RC_GFX_RELEASE_PROGRAM:
        gl_cache_forget_program(gl_name);
        glDeleteProgram(gl_name);
        break;
    case RC_GFX_RELEASE_FBO:
        glDeleteFramebuffers(1, &gl_name);   // FBO bindings are not shadowed
        break;
    default:
        RC_UNREACHABLE();
    }
}

/* Remove one VAO cache entry by array index (delete GL object, fix the trie
 * for the swap-removed last element). */
static void gl_vao_cache_remove(uint32_t index)
{
    gl_vao_entry entry = rc_array_gl_vao_entry_get(&gl.vao_entries, index);
    glDeleteVertexArrays(1, &entry.vao);
    if (gl.cache.vao == entry.vao) {
        gl_bind_vao(0);
    }
    bool deleted = gl_vao_trie_delete(gl.vao_trie, &gl.vao_trie_pool, entry.key);
    RC_ASSERT(deleted);
    (void)deleted;

    uint32_t last = gl.vao_entries.num - 1;
    if (index != last) {
        gl_vao_entry moved = rc_array_gl_vao_entry_get(&gl.vao_entries, last);
        rc_array_gl_vao_entry_set(&gl.vao_entries, index, moved);
        uint32_t node = gl_vao_trie_find(gl.vao_trie, &gl.vao_trie_pool, moved.key);
        RC_ASSERT(node != RC_INDEX_NONE);
        gl_vao_trie_value_set(&gl.vao_trie_pool, node, index);
    }
    rc_array_gl_vao_entry_pop_n(&gl.vao_entries, 1);
}

static bool gl_vao_key_uses_buffer(const gl_vao_key *key, rc_gfx_buffer buffer)
{
    for (uint32_t i = 0; i < RC_GFX_MAX_VERTEX_BUFFERS; i += 1) {
        if (rc_genpool_handle_equal(key->vbufs[i].h, buffer.h)) {
            return true;
        }
    }
    return false;
}

void rc_gfx_backend_evict_buffer(rc_gfx_buffer buffer)
{
    for (uint32_t i = 0; i < gl.vao_entries.num; ) {
        if (gl_vao_key_uses_buffer(&rc_array_gl_vao_entry_at(&gl.vao_entries, i)->key, buffer)) {
            gl_vao_cache_remove(i);   // swap-remove; recheck the same index
        } else {
            i += 1;
        }
    }
}

void rc_gfx_backend_evict_pipeline(rc_gfx_pipeline pipeline)
{
    for (uint32_t i = 0; i < gl.vao_entries.num; ) {
        if (rc_genpool_handle_equal(rc_array_gl_vao_entry_at(&gl.vao_entries, i)->key.pipeline.h, pipeline.h)) {
            gl_vao_cache_remove(i);
        } else {
            i += 1;
        }
    }
}

/* ---- pipeline state application ---- */

static void gl_apply_pipeline(void)
{
    const rc_gfx_pipeline_obj *pip = gl.play.pipeline;
    gl_use_program(pip->gl_program);

    // blend: GL 3.3 has per-target enables but a single blend function, so
    // target 0's state stands for all (validated at pipeline creation)
    const rc_gfx_blend_state *blend = &pip->colors[0].blend;
    bool any_blend = false;
    for (uint32_t i = 0; i < pip->color_count; i += 1) {
        if (pip->colors[i].blend.enabled) {
            glEnablei(GL_BLEND, i);
            any_blend = true;
        } else {
            glDisablei(GL_BLEND, i);
        }
        uint8_t mask = pip->colors[i].write_mask;
        glColorMaski(i,
                     (mask & RC_GFX_COLOR_MASK_DISABLE_R) ? GL_FALSE : GL_TRUE,
                     (mask & RC_GFX_COLOR_MASK_DISABLE_G) ? GL_FALSE : GL_TRUE,
                     (mask & RC_GFX_COLOR_MASK_DISABLE_B) ? GL_FALSE : GL_TRUE,
                     (mask & RC_GFX_COLOR_MASK_DISABLE_A) ? GL_FALSE : GL_TRUE);
    }
    if (any_blend) {
        glBlendFuncSeparate(rc_gfx_gl_blend_factor(blend->src_factor),
                            rc_gfx_gl_blend_factor(blend->dst_factor),
                            rc_gfx_gl_blend_factor(blend->src_alpha_factor),
                            rc_gfx_gl_blend_factor(blend->dst_alpha_factor));
        glBlendEquationSeparate(rc_gfx_gl_blend_op(blend->op),
                                rc_gfx_gl_blend_op(blend->alpha_op));
    }

    // depth
    const rc_gfx_depth_stencil_state *ds = &pip->depth_stencil;
    bool depth_active = ds->format != RC_GFX_TEXTURE_FORMAT_NONE
                     && (ds->depth_write || ds->depth_compare != RC_GFX_COMPARE_ALWAYS);
    if (depth_active) {
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(rc_gfx_gl_compare(ds->depth_compare));
        glDepthMask(ds->depth_write ? GL_TRUE : GL_FALSE);
    } else {
        glDisable(GL_DEPTH_TEST);
        glDepthMask(GL_FALSE);
    }

    // stencil
    if (ds->stencil_enabled) {
        glEnable(GL_STENCIL_TEST);
        GLuint read_mask = ds->stencil_read_mask != 0 ? ds->stencil_read_mask : 0xFFu;
        GLuint write_mask = ds->stencil_write_mask != 0 ? ds->stencil_write_mask : 0xFFu;
        GLint ref = (GLint)gl.play.stencil_ref;
        glStencilFuncSeparate(GL_FRONT, rc_gfx_gl_compare(ds->stencil_front.compare), ref, read_mask);
        glStencilFuncSeparate(GL_BACK, rc_gfx_gl_compare(ds->stencil_back.compare), ref, read_mask);
        glStencilOpSeparate(GL_FRONT,
                            rc_gfx_gl_stencil_op(ds->stencil_front.fail_op),
                            rc_gfx_gl_stencil_op(ds->stencil_front.depth_fail_op),
                            rc_gfx_gl_stencil_op(ds->stencil_front.pass_op));
        glStencilOpSeparate(GL_BACK,
                            rc_gfx_gl_stencil_op(ds->stencil_back.fail_op),
                            rc_gfx_gl_stencil_op(ds->stencil_back.depth_fail_op),
                            rc_gfx_gl_stencil_op(ds->stencil_back.pass_op));
        glStencilMaskSeparate(GL_FRONT_AND_BACK, write_mask);
    } else {
        glDisable(GL_STENCIL_TEST);
    }

    // depth bias
    if (ds->depth_bias != 0.0f || ds->depth_bias_slope_scale != 0.0f) {
        glEnable(GL_POLYGON_OFFSET_FILL);
        glPolygonOffset(ds->depth_bias_slope_scale, ds->depth_bias);
    } else {
        glDisable(GL_POLYGON_OFFSET_FILL);
    }

    // raster
    if (pip->cull == RC_GFX_CULL_NONE) {
        glDisable(GL_CULL_FACE);
    } else {
        glEnable(GL_CULL_FACE);
        glCullFace(pip->cull == RC_GFX_CULL_FRONT ? GL_FRONT : GL_BACK);
    }
    // canonical winding is inverted globally by the rc_clip y negation
    glFrontFace(pip->front_face == RC_GFX_FRONT_FACE_CCW ? GL_CW : GL_CCW);

    if (pip->alpha_to_coverage) {
        glEnable(GL_SAMPLE_ALPHA_TO_COVERAGE);
    } else {
        glDisable(GL_SAMPLE_ALPHA_TO_COVERAGE);
    }

    gl.play.pipeline_dirty = false;
}

static void gl_apply_bind_group(uint32_t group_index)
{
    rc_gfx_bind_group group_handle = gl.play.groups[group_index];
    RC_ASSERT(!rc_genpool_handle_is_null(group_handle.h));   // draw with a group the pipeline layout needs unset
    rc_gfx_bind_group_obj *group = rc_gfx_resolve_bind_group(group_handle);
    const rc_gfx_pipeline_layout_obj *layout = gl.play.layout;
    RC_ASSERT(rc_genpool_handle_equal(group->layout.h, layout->groups[group_index].h));   // same layout object
    RC_ASSERT(gl.play.group_offset_counts[group_index] == group->dynamic_count);

    uint32_t next_offset = 0;
    for (uint32_t e = 0; e < group->entry_count; e += 1) {
        const rc_gfx_bind_entry_resolved *entry = &group->entries[e];
        uint32_t unit = layout->flat_unit[group_index][e];
        switch (entry->type) {
        case RC_GFX_BINDING_UNIFORM_BUFFER: {
            uint32_t offset = entry->offset;
            if (entry->has_dynamic_offset) {
                uint32_t dyn = gl.play.group_offsets[group_index][next_offset];
                next_offset += 1;
                RC_ASSERT(dyn % RC_GFX_UNIFORM_ALIGN == 0);
                offset += dyn;
            }
            gl_bind_ubo_range(unit, entry->gl_object, (GLintptr)offset, (GLsizeiptr)entry->size);
            break;
        }
        case RC_GFX_BINDING_TEXTURE:
            gl_bind_texture_unit(unit, entry->gl_target, entry->gl_object);
            break;
        case RC_GFX_BINDING_STORAGE_BUFFER_READ:
            gl_bind_texture_unit(unit, GL_TEXTURE_BUFFER, entry->gl_object);
            break;
        case RC_GFX_BINDING_SAMPLER:
        case RC_GFX_BINDING_COMPARISON_SAMPLER:
            // bound onto the paired texture's unit by gl_apply_sampler_pairs
            break;
        default:
            RC_UNREACHABLE();
        }
    }
}

/* Find the sampler GL name a pair refers to in the currently bound groups. */
static GLuint gl_pair_sampler(const rc_gfx_shader_pair_info *pair)
{
    if (pair->sampler_group >= gl.play.layout->group_count) {
        return 0;
    }
    rc_gfx_bind_group group_handle = gl.play.groups[pair->sampler_group];
    if (rc_genpool_handle_is_null(group_handle.h)) {
        return 0;
    }
    rc_gfx_bind_group_obj *group = rc_gfx_resolve_bind_group(group_handle);
    for (uint32_t e = 0; e < group->entry_count; e += 1) {
        const rc_gfx_bind_entry_resolved *entry = &group->entries[e];
        if (entry->binding == pair->sampler_binding
            && (entry->type == RC_GFX_BINDING_SAMPLER
                || entry->type == RC_GFX_BINDING_COMPARISON_SAMPLER)) {
            return entry->gl_object;
        }
    }
    return 0;
}

static void gl_apply_sampler_pairs(void)
{
    const rc_gfx_shader_obj *shader = gl.play.shader;
    const rc_gfx_pipeline_layout_obj *layout = gl.play.layout;
    for (uint32_t i = 0; i < shader->pair_count; i += 1) {
        const rc_gfx_shader_pair_info *pair = &shader->pairs[i];
        if (pair->gl_location < 0) {
            continue;
        }
        rc_gfx_bind_group_layout_obj *group = rc_gfx_resolve_bind_group_layout(layout->groups[pair->texture_group]);
        uint32_t entry = gl_layout_entry_index(group, pair->texture_binding);
        RC_ASSERT(entry != RC_INDEX_NONE);
        uint32_t unit = layout->flat_unit[pair->texture_group][entry];
        gl_bind_sampler_unit(unit, gl_pair_sampler(pair));
    }
}

/* ---- VAO cache lookup ---- */

static GLuint gl_vao_get(uint32_t *out_entry_index)
{
    const rc_gfx_pipeline_obj *pip = gl.play.pipeline;
    gl_vao_key key = {.pipeline = gl.play.pipeline_handle};
    for (uint32_t b = 0; b < RC_GFX_MAX_VERTEX_BUFFERS; b += 1) {
        if (pip->vbuf_mask & (1u << b)) {
            key.vbufs[b] = gl.play.vbufs[b];
            key.vbuf_offsets[b] = gl.play.vbuf_offsets[b];
            RC_ASSERT(!rc_genpool_handle_is_null(key.vbufs[b].h));   // draw without a vertex buffer the layout needs
        }
    }

    uint32_t node = gl_vao_trie_find(gl.vao_trie, &gl.vao_trie_pool, key);
    if (node != RC_INDEX_NONE) {
        *out_entry_index = gl_vao_trie_value_get(&gl.vao_trie_pool, node);
        return rc_array_gl_vao_entry_at(&gl.vao_entries, *out_entry_index)->vao;
    }

    GLuint vao = 0;
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);
    gl.cache.vao = vao;
    for (uint32_t i = 0; i < pip->attr_count; i += 1) {
        const rc_gfx_vertex_attribute *attr = &pip->attrs[i];
        rc_gfx_buffer_obj *buffer = rc_gfx_resolve_buffer(key.vbufs[attr->buffer_index]);
        RC_ASSERT(buffer->usage & RC_GFX_BUFFER_USAGE_VERTEX);
        rc_gfx_gl_vertex_format fmt = rc_gfx_gl_vertex_format_get(attr->format);
        glBindBuffer(GL_ARRAY_BUFFER, buffer->gl_buffer);
        glEnableVertexAttribArray(attr->location);
        const void *pointer = (const void *)(uintptr_t)(attr->offset + key.vbuf_offsets[attr->buffer_index]);
        if (fmt.integer) {
            glVertexAttribIPointer(attr->location, fmt.size, fmt.type,
                                   (GLsizei)pip->strides[attr->buffer_index], pointer);
        } else {
            glVertexAttribPointer(attr->location, fmt.size, fmt.type, fmt.normalized,
                                  (GLsizei)pip->strides[attr->buffer_index], pointer);
        }
        glVertexAttribDivisor(attr->location, pip->divisors[attr->buffer_index]);
    }

    uint32_t index = rc_array_gl_vao_entry_push(&gl.vao_entries, (gl_vao_entry) {
        .key = key,
        .vao = vao,
    }, rc_gfx_internal_arena());
    gl_vao_trie_add(&gl.vao_trie, &gl.vao_trie_pool, key, index, rc_gfx_internal_arena());
    *out_entry_index = index;
    return vao;
}

/* ---- draw flush ---- */

static void gl_flush_draw_state(bool indexed)
{
    RC_ASSERT(gl.play.in_pass);
    RC_ASSERT(gl.play.pipeline != NULL);   // draw before set_pipeline

    if (gl.play.pipeline_dirty) {
        gl_apply_pipeline();
    }
    const rc_gfx_pipeline_layout_obj *layout = gl.play.layout;
    bool groups_changed = false;
    for (uint32_t g = 0; g < layout->group_count; g += 1) {
        if (gl.play.groups_dirty & (1u << g)) {
            gl_apply_bind_group(g);
            groups_changed = true;
        }
    }
    gl.play.groups_dirty = 0;
    if (groups_changed || gl.play.shader->pair_count > 0) {
        gl_apply_sampler_pairs();
    }

    if (gl.play.pipeline->attr_count > 0) {
        uint32_t entry_index = 0;
        GLuint vao = gl_vao_get(&entry_index);
        gl_bind_vao(vao);
        if (indexed) {
            RC_ASSERT(!rc_genpool_handle_is_null(gl.play.index_buffer.h));   // draw_indexed before set_index_buffer
            rc_gfx_buffer_obj *index_buffer = rc_gfx_resolve_buffer(gl.play.index_buffer);
            RC_ASSERT(index_buffer->usage & RC_GFX_BUFFER_USAGE_INDEX);
            gl_vao_entry *entry = rc_array_gl_vao_entry_at(&gl.vao_entries, entry_index);
            if (entry->element_buffer != index_buffer->gl_buffer) {
                // element binding is VAO state; track it per cache entry
                glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, index_buffer->gl_buffer);
                entry->element_buffer = index_buffer->gl_buffer;
            }
        }
    } else {
        gl_bind_vao(gl.empty_vao);
        RC_ASSERT(!indexed);   // an attribute-less pipeline cannot draw indexed
    }
}

static void gl_play_draw(const rc_gfx_draw_desc *desc)
{
    gl_flush_draw_state(false);
    RC_ASSERT(desc->first_instance == 0);   // base_instance needs GL 4.2
    GLenum primitive = rc_gfx_gl_primitive(gl.play.pipeline->primitive);
    uint32_t instances = desc->instance_count != 0 ? desc->instance_count : 1;
    if (instances > 1) {
        glDrawArraysInstanced(primitive, (GLint)desc->first_vertex,
                              (GLsizei)desc->vertex_count, (GLsizei)instances);
    } else {
        glDrawArrays(primitive, (GLint)desc->first_vertex, (GLsizei)desc->vertex_count);
    }
}

static void gl_play_draw_indexed(const rc_gfx_draw_indexed_desc *desc)
{
    gl_flush_draw_state(true);
    RC_ASSERT(desc->first_instance == 0);   // base_instance needs GL 4.2
    RC_ASSERT(gl.play.pipeline->index_format == gl.play.index_format);
    GLenum primitive = rc_gfx_gl_primitive(gl.play.pipeline->primitive);
    GLenum type = gl.play.index_format == RC_GFX_INDEX_FORMAT_U16 ? GL_UNSIGNED_SHORT : GL_UNSIGNED_INT;
    uint32_t index_size = gl.play.index_format == RC_GFX_INDEX_FORMAT_U16 ? 2 : 4;
    const void *pointer = (const void *)(uintptr_t)(gl.play.index_offset + desc->first_index * index_size);
    uint32_t instances = desc->instance_count != 0 ? desc->instance_count : 1;
    if (instances > 1) {
        glDrawElementsInstancedBaseVertex(primitive, (GLsizei)desc->index_count, type, pointer,
                                          (GLsizei)instances, desc->base_vertex);
    } else {
        glDrawElementsBaseVertex(primitive, (GLsizei)desc->index_count, type, pointer,
                                 desc->base_vertex);
    }
}

/* ---- pass playback ---- */

static void gl_play_pass_begin(const rc_gfx_pass_desc *desc)
{
    RC_ASSERT(!gl.play.in_pass);
    gl.play.in_pass = true;
    gl.play.pass = *desc;
    gl.play.target = desc->target;

    GLuint fbo = 0;
    if (rc_genpool_handle_is_null(desc->target.h)) {
        gl.play.target_is_swapchain = true;
        gl.play.target_size = gl.sc_size;
        gl.play.color_count = 1;
        gl.play.color_formats[0] = gl.sc_format;
        gl.play.depth_format = gl.sc_depth_format;
        gl.play.sample_count = gl.sc_samples;
        fbo = gl.sc_samples > 1 ? gl.sc_msaa_fbo : gl.sc_fbo;
    } else {
        rc_gfx_render_target_obj *target = rc_gfx_resolve_render_target(desc->target);
        gl.play.target_is_swapchain = false;
        gl.play.target_size = target->size;
        gl.play.color_count = target->color_count;
        for (uint32_t i = 0; i < target->color_count; i += 1) {
            gl.play.color_formats[i] = target->color_formats[i];
        }
        gl.play.depth_format = target->depth_format;
        gl.play.sample_count = target->sample_count;
        fbo = target->gl_fbo;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glViewport(0, 0, gl.play.target_size.x, gl.play.target_size.y);
    glDepthRange(0.0, 1.0);
    glScissor(0, 0, gl.play.target_size.x, gl.play.target_size.y);

    // clears honour the write masks and scissor; open everything first
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glDepthMask(GL_TRUE);
    glStencilMaskSeparate(GL_FRONT_AND_BACK, 0xFFu);

    for (uint32_t i = 0; i < gl.play.color_count; i += 1) {
        if (desc->colors[i].load_op == RC_GFX_LOAD_OP_CLEAR) {
            // linear values; the hardware encodes on write for sRGB targets
            float value[4] = {desc->colors[i].clear_value.x, desc->colors[i].clear_value.y,
                              desc->colors[i].clear_value.z, desc->colors[i].clear_value.w};
            glClearBufferfv(GL_COLOR, (GLint)i, value);
        }
    }
    if (gl.play.depth_format != RC_GFX_TEXTURE_FORMAT_NONE) {
        bool has_stencil = rc_gfx_texture_format_is_stencil(gl.play.depth_format);
        bool clear_depth = desc->depth_stencil.depth_load_op == RC_GFX_LOAD_OP_CLEAR;
        bool clear_stencil = has_stencil && desc->depth_stencil.stencil_load_op == RC_GFX_LOAD_OP_CLEAR;
        if (clear_depth && clear_stencil) {
            glClearBufferfi(GL_DEPTH_STENCIL, 0, desc->depth_stencil.depth_clear_value,
                            desc->depth_stencil.stencil_clear_value);
        } else if (clear_depth) {
            glClearBufferfv(GL_DEPTH, 0, &desc->depth_stencil.depth_clear_value);
        } else if (clear_stencil) {
            GLint stencil = desc->depth_stencil.stencil_clear_value;
            glClearBufferiv(GL_STENCIL, 0, &stencil);
        }
    }

    // per-pass encoder state starts clean; the pipeline shadow is stale after
    // the mask resets above, so the next set_pipeline reapplies in full
    gl.play.pipeline = NULL;
    gl.play.shader = NULL;
    gl.play.layout = NULL;
    gl.play.pipeline_handle = (rc_gfx_pipeline) {0};
    gl.play.pipeline_dirty = false;
    gl.play.groups_dirty = 0;
    memset(gl.play.groups, 0, sizeof(gl.play.groups));
    memset(gl.play.vbufs, 0, sizeof(gl.play.vbufs));
    memset(gl.play.vbuf_offsets, 0, sizeof(gl.play.vbuf_offsets));
    gl.play.index_buffer = (rc_gfx_buffer) {0};
    gl.play.index_format = RC_GFX_INDEX_FORMAT_NONE;
    gl.play.index_offset = 0;
    gl.play.stencil_ref = 0;
}

static void gl_play_pass_end(void)
{
    RC_ASSERT(gl.play.in_pass);

    // store-op RESOLVE: blit each resolved attachment into the resolve FBO.
    // Both sides are our own FBOs with identical formats, which sidesteps the
    // cross-encoding blit ambiguity entirely.
    if (!gl.play.target_is_swapchain) {
        rc_gfx_render_target_obj *target = rc_gfx_resolve_render_target(gl.play.target);
        for (uint32_t i = 0; i < gl.play.color_count; i += 1) {
            if (gl.play.pass.colors[i].store_op != RC_GFX_STORE_OP_RESOLVE) {
                continue;
            }
            RC_ASSERT(target->resolve_mask & (1u << i));   // RESOLVE needs a resolve attachment
            static const GLenum draw_buffers[RC_GFX_MAX_COLOR_ATTACHMENTS] = {
                GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2, GL_COLOR_ATTACHMENT3,
            };
            glBindFramebuffer(GL_READ_FRAMEBUFFER, target->gl_fbo);
            glReadBuffer(GL_COLOR_ATTACHMENT0 + i);
            glBindFramebuffer(GL_DRAW_FRAMEBUFFER, target->gl_resolve_fbo);
            glDrawBuffers(1, &draw_buffers[i]);
            glBlitFramebuffer(0, 0, gl.play.target_size.x, gl.play.target_size.y,
                              0, 0, gl.play.target_size.x, gl.play.target_size.y,
                              GL_COLOR_BUFFER_BIT, GL_NEAREST);
        }
        // restore the read buffer convention for the main FBO
        glBindFramebuffer(GL_FRAMEBUFFER, target->gl_fbo);
        if (target->color_count > 0) {
            glReadBuffer(GL_COLOR_ATTACHMENT0);
        }
    } else {
        for (uint32_t i = 0; i < gl.play.color_count; i += 1) {
            RC_ASSERT(gl.play.pass.colors[i].store_op != RC_GFX_STORE_OP_RESOLVE);
        }
    }
    // store-op DISCARD would be glInvalidateFramebuffer, which is GL 4.3;
    // a no-op is the correct GL 3.3 fallback

    gl.play.in_pass = false;
}

static void gl_play_set_pipeline(rc_gfx_pipeline handle)
{
    rc_gfx_pipeline_obj *pip = rc_gfx_resolve_pipeline(handle);
    if (rc_gfx_internal_validation()) {
        // the formats and sample count baked into the pipeline must match the
        // pass's render target - the D3D12/Vulkan PSO contract, checked here
        RC_ASSERT(pip->color_count == gl.play.color_count);
        for (uint32_t i = 0; i < pip->color_count; i += 1) {
            RC_ASSERT(pip->colors[i].format == gl.play.color_formats[i]);
        }
        RC_ASSERT(pip->depth_stencil.format == gl.play.depth_format);
        RC_ASSERT(pip->sample_count == gl.play.sample_count);
    }

    rc_gfx_pipeline_layout_obj *layout = rc_gfx_resolve_pipeline_layout(pip->layout);
    if (gl.play.layout == NULL || gl.play.layout->mapping_hash != layout->mapping_hash) {
        // the flat unit assignment changed: every bound group must reapply
        for (uint32_t g = 0; g < RC_GFX_MAX_BIND_GROUPS; g += 1) {
            if (!rc_genpool_handle_is_null(gl.play.groups[g].h)) {
                gl.play.groups_dirty |= 1u << g;
            }
        }
    }
    gl.play.pipeline = pip;
    gl.play.pipeline_handle = handle;
    gl.play.shader = rc_gfx_resolve_shader(pip->shader);
    gl.play.layout = layout;
    gl.play.pipeline_dirty = true;
}

/* ---- submit ---- */

void rc_gfx_backend_submit(rc_view_bytes stream)
{
    rc_gfx_cmd_reader r = {.p = stream.data, .end = stream.data + stream.num};
    while (!rc_gfx_cmd_reader_done(&r)) {
        rc_gfx_op op = (rc_gfx_op)rc_gfx_cmd_read_u32(&r);
        switch (op) {
        case RC_GFX_OP_PASS_BEGIN: {
            rc_gfx_pass_desc desc;
            rc_gfx_cmd_read(&r, &desc, (uint32_t)sizeof(desc));
            gl_play_pass_begin(&desc);
            break;
        }
        case RC_GFX_OP_PASS_END: {
            gl_play_pass_end();
            break;
        }
        case RC_GFX_OP_SET_PIPELINE: {
            rc_gfx_cmd_set_pipeline cmd;
            rc_gfx_cmd_read(&r, &cmd, (uint32_t)sizeof(cmd));
            gl_play_set_pipeline(cmd.pipeline);
            break;
        }
        case RC_GFX_OP_SET_BIND_GROUP: {
            rc_gfx_cmd_set_bind_group cmd;
            rc_gfx_cmd_read(&r, &cmd, (uint32_t)sizeof(cmd));
            RC_ASSERT(cmd.offset_count <= RC_GFX_MAX_BINDINGS_PER_GROUP);
            gl.play.groups[cmd.group_index] = cmd.group;
            gl.play.group_offset_counts[cmd.group_index] = cmd.offset_count;
            for (uint32_t i = 0; i < cmd.offset_count; i += 1) {
                gl.play.group_offsets[cmd.group_index][i] = rc_gfx_cmd_read_u32(&r);
            }
            gl.play.groups_dirty |= 1u << cmd.group_index;
            break;
        }
        case RC_GFX_OP_SET_VERTEX_BUFFER: {
            rc_gfx_cmd_set_vertex_buffer cmd;
            rc_gfx_cmd_read(&r, &cmd, (uint32_t)sizeof(cmd));
            gl.play.vbufs[cmd.slot] = cmd.buffer;
            gl.play.vbuf_offsets[cmd.slot] = cmd.offset;
            break;
        }
        case RC_GFX_OP_SET_INDEX_BUFFER: {
            rc_gfx_cmd_set_index_buffer cmd;
            rc_gfx_cmd_read(&r, &cmd, (uint32_t)sizeof(cmd));
            gl.play.index_buffer = cmd.buffer;
            gl.play.index_format = (rc_gfx_index_format)cmd.format;
            gl.play.index_offset = cmd.offset;
            break;
        }
        case RC_GFX_OP_SET_VIEWPORT: {
            rc_gfx_cmd_set_viewport cmd;
            rc_gfx_cmd_read(&r, &cmd, (uint32_t)sizeof(cmd));
            // canonical top-left viewport passes through unconverted (see the
            // y derivation in the design)
            rc_vec2i pos = rc_box2i_min(cmd.rect);
            rc_vec2i size = rc_box2i_size(cmd.rect);
            glViewport(pos.x, pos.y, size.x, size.y);
            glDepthRange(cmd.min_depth, cmd.max_depth);
            break;
        }
        case RC_GFX_OP_SET_SCISSOR: {
            rc_gfx_cmd_set_scissor cmd;
            rc_gfx_cmd_read(&r, &cmd, (uint32_t)sizeof(cmd));
            rc_vec2i pos = rc_box2i_min(cmd.rect);
            rc_vec2i size = rc_box2i_size(cmd.rect);
            glScissor(pos.x, pos.y, size.x, size.y);
            break;
        }
        case RC_GFX_OP_SET_BLEND_CONSTANT: {
            rc_gfx_cmd_set_blend_constant cmd;
            rc_gfx_cmd_read(&r, &cmd, (uint32_t)sizeof(cmd));
            glBlendColor(cmd.color.x, cmd.color.y, cmd.color.z, cmd.color.w);
            break;
        }
        case RC_GFX_OP_SET_STENCIL_REFERENCE: {
            rc_gfx_cmd_set_stencil_reference cmd;
            rc_gfx_cmd_read(&r, &cmd, (uint32_t)sizeof(cmd));
            gl.play.stencil_ref = cmd.reference;
            if (gl.play.pipeline != NULL && gl.play.pipeline->depth_stencil.stencil_enabled) {
                gl.play.pipeline_dirty = true;   // reference feeds glStencilFuncSeparate
            }
            break;
        }
        case RC_GFX_OP_DRAW: {
            rc_gfx_draw_desc desc;
            rc_gfx_cmd_read(&r, &desc, (uint32_t)sizeof(desc));
            gl_play_draw(&desc);
            break;
        }
        case RC_GFX_OP_DRAW_INDEXED: {
            rc_gfx_draw_indexed_desc desc;
            rc_gfx_cmd_read(&r, &desc, (uint32_t)sizeof(desc));
            gl_play_draw_indexed(&desc);
            break;
        }
        case RC_GFX_OP_PUSH_DEBUG_GROUP: {
            rc_str label;
            rc_gfx_cmd_read(&r, &label, (uint32_t)sizeof(label));
            // GL_KHR_debug entry points are absent from the vendored loader;
            // debug groups are no-ops on this backend
            break;
        }
        case RC_GFX_OP_POP_DEBUG_GROUP: {
            break;
        }
        default:
            RC_UNREACHABLE();
        }
    }
}
