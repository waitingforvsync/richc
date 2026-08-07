/*
 * gfx/texture.h - GPU textures and samplers.
 *
 * Types
 * -----
 *   rc_gfx_texture_dim          - 2D (default), 2D_ARRAY, 3D, CUBE
 *   rc_gfx_texture_sample_type  - FLOAT (default), UNFILTERABLE_FLOAT, DEPTH,
 *                                 SINT, UINT
 *   rc_gfx_texture_usage        - bit flags: SAMPLED, RENDER_ATTACHMENT,
 *                                 COPY_SRC, COPY_DST
 *   rc_gfx_filter               - NEAREST (default), LINEAR
 *   rc_gfx_address              - CLAMP_TO_EDGE (default), REPEAT,
 *                                 MIRROR_REPEAT, CLAMP_TO_BORDER
 *   rc_gfx_texture_data         - per-subresource initial contents
 *   rc_gfx_texture_desc, rc_gfx_sampler_desc
 *
 * Functions
 * ---------
 *   rc_gfx_texture_make(desc) / rc_gfx_texture_destroy(tex)
 *   rc_gfx_texture_update(tex, mip, slice, region, data)
 *   rc_gfx_texture_generate_mipmaps(tex)
 *   rc_gfx_mip_count(size)
 *   rc_gfx_texture_from_image(img, srgb, mipmaps)
 *   rc_gfx_sampler_make(desc) / rc_gfx_sampler_destroy(smp)
 *
 * Pixel data is supplied with rows top-first, tightly packed to the format's
 * block size.  Samplers are separate objects at the API level on every
 * backend; the GL backend re-combines them with textures internally.
 */

#ifndef RC_GFX_TEXTURE_H_
#define RC_GFX_TEXTURE_H_

#include "richc/bytes.h"
#include "richc/gfx/gfx.h"
#include "richc/image/image.h"
#include "richc/math/vec4f.h"

/* ---- enums ---- */

typedef enum rc_gfx_texture_dim {
    RC_GFX_TEXTURE_DIM_2D = 0, RC_GFX_TEXTURE_DIM_2D_ARRAY,
    RC_GFX_TEXTURE_DIM_3D, RC_GFX_TEXTURE_DIM_CUBE,
} rc_gfx_texture_dim;

typedef enum rc_gfx_texture_sample_type {
    RC_GFX_TEXTURE_SAMPLE_FLOAT = 0,       /* filterable */
    RC_GFX_TEXTURE_SAMPLE_UNFILTERABLE_FLOAT,
    RC_GFX_TEXTURE_SAMPLE_DEPTH,
    RC_GFX_TEXTURE_SAMPLE_SINT, RC_GFX_TEXTURE_SAMPLE_UINT,
} rc_gfx_texture_sample_type;

/* Bit flags describing how a texture may be used. */
enum rc_gfx_texture_usage {
    RC_GFX_TEXTURE_USAGE_SAMPLED           = 1u << 0,
    RC_GFX_TEXTURE_USAGE_RENDER_ATTACHMENT = 1u << 1,
    RC_GFX_TEXTURE_USAGE_COPY_SRC          = 1u << 2,
    RC_GFX_TEXTURE_USAGE_COPY_DST          = 1u << 3,
};

typedef enum rc_gfx_filter {
    RC_GFX_FILTER_NEAREST = 0, RC_GFX_FILTER_LINEAR
} rc_gfx_filter;

typedef enum rc_gfx_address {
    RC_GFX_ADDRESS_CLAMP_TO_EDGE = 0, RC_GFX_ADDRESS_REPEAT,
    RC_GFX_ADDRESS_MIRROR_REPEAT, RC_GFX_ADDRESS_CLAMP_TO_BORDER,
} rc_gfx_address;

/* ---- textures ---- */

typedef struct rc_gfx_texture_data {
    /* subresources[slice * mip_count + mip] */
    const rc_view_bytes *subresources;
    uint32_t             count;
} rc_gfx_texture_data;

typedef struct rc_gfx_texture_desc {
    rc_gfx_texture_dim    dim;          /* default 2D */
    rc_gfx_texture_format format;       /* required */
    rc_vec2i              size;         /* width, height; required */
    uint32_t              depth;        /* 3D depth or array layers; default 1 */
    uint32_t              mip_count;    /* 0 => 1; use rc_gfx_mip_count() for a full chain */
    uint32_t              sample_count; /* 0 or 1 => no MSAA */
    uint32_t              usage;        /* rc_gfx_texture_usage flags; default SAMPLED */
    rc_gfx_texture_data   data;         /* optional initial contents */
    rc_str                label;
} rc_gfx_texture_desc;

rc_gfx_texture rc_gfx_texture_make(const rc_gfx_texture_desc *desc);
void           rc_gfx_texture_destroy(rc_gfx_texture tex);

/* Update a region of one mip of one slice.  Uncompressed formats only; the
 * data is tightly packed rows covering exactly the region. */
void rc_gfx_texture_update(rc_gfx_texture tex, uint32_t mip, uint32_t slice,
                           rc_box2i region, rc_view_bytes data);

/*
 * Generate mips 1..mip_count-1 from mip 0 on the GPU.  Convenience only: for
 * sRGB formats the GL spec requires linear-space downsampling but drivers have
 * historically diverged, so where quality matters generate mips on the CPU in
 * linear space and upload them explicitly.
 */
void rc_gfx_texture_generate_mipmaps(rc_gfx_texture tex);

/* Number of mips in a full chain for a given base size. */
uint32_t rc_gfx_mip_count(rc_vec2i size);

/*
 * Bridge from the CPU image type in the same layer.  srgb selects RGBA8_SRGB
 * vs RGBA8_UNORM (or R8_UNORM for single-channel images, which are never
 * sRGB); RGB8 images are widened to RGBA on upload.  mipmaps requests a full
 * chain generated on the GPU (see the rc_gfx_texture_generate_mipmaps caveat).
 */
rc_gfx_texture rc_gfx_texture_from_image(rc_image img, bool srgb, bool mipmaps);

/* ---- samplers ---- */

typedef struct rc_gfx_sampler_desc {
    rc_gfx_filter   min_filter;      /* default NEAREST */
    rc_gfx_filter   mag_filter;
    rc_gfx_filter   mip_filter;
    rc_gfx_address  address_u;       /* default CLAMP_TO_EDGE */
    rc_gfx_address  address_v;
    rc_gfx_address  address_w;
    float           lod_min;
    float           lod_max;         /* 0 => 1000.0f */
    uint32_t        max_anisotropy;                    /* 0 or 1 => off */
    rc_gfx_compare  compare;                           /* ALWAYS => not a comparison sampler */
    rc_vec4f        border_color;                      /* linear */
    rc_str          label;
} rc_gfx_sampler_desc;

rc_gfx_sampler rc_gfx_sampler_make(const rc_gfx_sampler_desc *desc);
void           rc_gfx_sampler_destroy(rc_gfx_sampler smp);

#endif /* RC_GFX_TEXTURE_H_ */
