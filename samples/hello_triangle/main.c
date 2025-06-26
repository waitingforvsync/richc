#include "richc/app/app.h"
#include "richc/math/mat44f.h"
#include <GL/glew.h>
#include <stdio.h>


//////////////////////////////////////////////////

static bool compile_shader(GLuint s) {
    glCompileShader(s);
    GLint st;
    glGetShaderiv(s, GL_COMPILE_STATUS, &st);
    if (st != GL_TRUE) {
        char msg[512];
        glGetShaderInfoLog(s, 512, 0, msg);
        fprintf(stderr, "Shader error: %s\n", msg);
        return false;
    }
    return true;
}

static GLuint compile_shaders(const GLchar *vs_src, const GLchar *fs_src) {
    GLuint vs = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vs, 1, &vs_src, 0);

    GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fs, 1, &fs_src, 0);

    if (!(compile_shader(vs) && compile_shader(fs))) {
        exit(1);
    }

    GLuint p = glCreateProgram();
    glAttachShader(p, vs);
    glAttachShader(p, fs);
    glLinkProgram(p);
    return p;
}

#define SHADER(...) "#version 330 core\n" #__VA_ARGS__

//////////////////////////////////////////////////////

// Define the triangle vertex shader
static const char *triangle_vs = SHADER(
    layout(location = 0) in vec2 in_pos;
    layout(location = 1) in vec4 in_col;

    uniform mat4 u_mvp;
    out vec4 col;

    void main() {
        col = in_col;
        gl_Position = u_mvp * vec4(in_pos, 0.0, 1.0);
    }
);


// Define the triangle fragment shader
static const char *triangle_fs = SHADER(
    in vec4 col;
    out vec4 frag_color;

    void main() {
        frag_color = col;
    }
);


//////////////////////////////////////////////////////





typedef struct app_state_t {
    float angle;

    GLuint tri_prog;
    GLuint tri_vao;
    GLuint tri_vbo;
    GLuint tri_ebo;
    GLint tri_mvp;
} app_state_t;


static void on_init_render(void *context, int32_t width, int32_t height) {
    app_state_t *state = (app_state_t *)context;

    state->tri_prog = compile_shaders(triangle_vs, triangle_fs);

    typedef struct {
        vec2f_t pos;
        vec4f_t col;
    } tri_vertex_data_t;

    tri_vertex_data_t tri_verts[] = {
        {{0, -350}, {1, 0, 0, 1}},
        {{-350, 350}, {0, 1, 0, 1}},
        {{350, 350}, {0, 0, 1, 1}}
    };

    GLuint tri_indices[] = {0, 1, 2};

    glGenVertexArrays(1, &state->tri_vao);
    glGenBuffers(1, &state->tri_vbo);
    glGenBuffers(1, &state->tri_ebo);

    glBindVertexArray(state->tri_vao);
    glBindBuffer(GL_ARRAY_BUFFER, state->tri_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof tri_verts, tri_verts, GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, state->tri_ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof tri_indices, tri_indices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(tri_vertex_data_t), (void *)offsetof(tri_vertex_data_t, pos));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(tri_vertex_data_t), (void *)offsetof(tri_vertex_data_t, col));
    glBindVertexArray(0);

    state->tri_mvp = glGetUniformLocation(state->tri_prog, "u_mvp");
}


static void on_update(void *context, float delta_time) {
    app_state_t *state = (app_state_t *)context;
    state->angle += delta_time;
}


static void on_render(void *context, int32_t width, int32_t height) {
    app_state_t *state = (app_state_t *)context;

    glViewport(0, 0, width, height);
    glEnable(GL_FRAMEBUFFER_SRGB);

    glClearColor(0.15f, 0.2f, 0.3f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    glUseProgram(state->tri_prog);

    glUniformMatrix4fv(state->tri_mvp, 1, GL_FALSE,
        mat44f_as_floats(
            (mat44f_t[]) {
                mat44f_mul(
                    mat44f_make_ortho(0, width, 0, height, -1, 1),
                    mat44f_mul(
                        mat44f_make_translation((vec3f_t) {width / 2, height / 2, 0}),
                        mat44f_from_mat22f(mat22f_make_rotation(state->angle))
                    )
                )
            }
        )
    );

    glBindVertexArray(state->tri_vao);
    glDrawElements(GL_TRIANGLES, 3, GL_UNSIGNED_INT, 0);
}


int main(void) {
    app_state_t state = {
        .angle = 0.0f
    };

    app_run(&(app_desc_t) {
        .width = 1024,
        .height = 800,
        .title = "Sample window",
        .resizable = true,
        .srgb = true,
        .context = &state,
        .callbacks = {
            .on_init_render = on_init_render,
            .on_update = on_update,
            .on_render = on_render
        }
    });

    return 0;
}
