#include "font_sdf.h"

#include <math.h>

#include "richc/arena.h"
#include "richc/math/box2f.h"
#include "richc/math/solve.h"
#include "richc/ops.h"

// Per-edge precompute, independent of the sample point.  For a quadratic the
// distance derivative is the cubic
//   (B.B) t^3 + 3(A.B) t^2 + (2 A.A + M.B) t + M.A = 0,
// with A = p1 - p0, B = p2 - 2 p1 + p0, M = p0 - p; the p-independent pieces are
// kept here.  bbox is the edge's pixel bounding box, used to skip distant edges.
typedef struct sdf_edge {
    rc_vec2f p0, p1, p2;
    bool     is_quad;
    rc_box2f bbox;
    rc_vec2f a;        // p1 - p0
    rc_vec2f b;        // p2 - 2 p1 + p0
    float    bb;       // B . B
    float    ab3;      // 3 (A . B)
    float    aa2;      // 2 (A . A)
} sdf_edge;

#define RC_ARRAY_TYPE sdf_edge
#define RC_ARRAY_NAME sdf_edge
#include "richc/template/array.h"

static sdf_edge sdf_edge_make(font_edge e)
{
    rc_vec2f a = rc_vec2f_sub(e.p1, e.p0);
    rc_vec2f b = rc_vec2f_add(rc_vec2f_sub(e.p2, rc_vec2f_scalar_mul(e.p1, 2.0f)), e.p0);
    rc_box2f bbox = rc_box2f_make(e.p0, e.p2);
    if (e.is_quad)
        bbox = rc_box2f_expand(bbox, e.p1);   // the control point can extend it
    return (sdf_edge) {
        .p0 = e.p0, .p1 = e.p1, .p2 = e.p2, .is_quad = e.is_quad,
        .bbox = bbox,
        .a = a, .b = b,
        .bb = rc_vec2f_dot(b, b),
        .ab3 = 3.0f * rc_vec2f_dot(a, b),
        .aa2 = 2.0f * rc_vec2f_dot(a, a),
    };
}

// Lower bound on the squared distance from p to the edge's bbox (0 if inside).
static float sdf_bbox_dist2(const sdf_edge *e, rc_vec2f p)
{
    rc_vec2f mn = rc_box2f_min(e->bbox);
    rc_vec2f mx = rc_box2f_max(e->bbox);
    float dx = rc_max_f32(rc_max_f32(mn.x - p.x, p.x - mx.x), 0.0f);
    float dy = rc_max_f32(rc_max_f32(mn.y - p.y, p.y - mx.y), 0.0f);
    return dx * dx + dy * dy;
}

static float sdf_line_dist2(rc_vec2f a, rc_vec2f b, rc_vec2f p)
{
    rc_vec2f ab = rc_vec2f_sub(b, a);
    float len2 = rc_vec2f_dot(ab, ab);
    float t = len2 > 0.0f ? rc_vec2f_dot(rc_vec2f_sub(p, a), ab) / len2 : 0.0f;
    t = t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t);
    rc_vec2f q = rc_vec2f_add(a, rc_vec2f_scalar_mul(ab, t));
    rc_vec2f d = rc_vec2f_sub(q, p);
    return rc_vec2f_dot(d, d);
}

// B(t) = p0 + 2t A + t^2 B.
static rc_vec2f sdf_quad_eval(const sdf_edge *e, float t)
{
    return rc_vec2f_add3(e->p0,
                         rc_vec2f_scalar_mul(e->a, 2.0f * t),
                         rc_vec2f_scalar_mul(e->b, t * t));
}

static float sdf_quad_dist2_at(const sdf_edge *e, rc_vec2f p, float t)
{
    rc_vec2f d = rc_vec2f_sub(sdf_quad_eval(e, t), p);
    return rc_vec2f_dot(d, d);
}

static float sdf_quad_dist2(const sdf_edge *e, rc_vec2f p)
{
    rc_vec2f m = rc_vec2f_sub(e->p0, p);
    // endpoints are always candidates
    float best = rc_min_f32(sdf_quad_dist2_at(e, p, 0.0f), sdf_quad_dist2_at(e, p, 1.0f));
    rc_cubic_roots r = rc_solve_cubic(e->bb, e->ab3,
                                      e->aa2 + rc_vec2f_dot(m, e->b),
                                      rc_vec2f_dot(m, e->a));
    for (int i = 0; i < r.num_roots; ++i) {
        float t = r.root[i];
        if (t > 0.0f && t < 1.0f)
            best = rc_min_f32(best, sdf_quad_dist2_at(e, p, t));
    }
    return best;
}

// Nonzero-winding contribution of one edge to a +x ray from p (half-open in y).
static int sdf_line_winding(rc_vec2f a, rc_vec2f b, rc_vec2f p)
{
    bool ca = a.y <= p.y;
    bool cb = b.y <= p.y;
    if (ca == cb)
        return 0;
    float t  = (p.y - a.y) / (b.y - a.y);
    float xc = a.x + t * (b.x - a.x);
    if (xc <= p.x)
        return 0;
    return ca ? 1 : -1;
}

static int sdf_quad_winding(const sdf_edge *e, rc_vec2f p)
{
    // By(t) = p0.y + 2 A.y t + B.y t^2
    float u = e->b.y;
    float v = e->a.y;
    rc_quadratic_roots r = rc_solve_quadratic(u, 2.0f * v, e->p0.y - p.y);
    int wn = 0;
    for (int i = 0; i < r.num_roots; ++i) {
        float t = r.root[i];
        if (t < 0.0f || t >= 1.0f)            // [0, 1): start-inclusive
            continue;
        rc_vec2f q = sdf_quad_eval(e, t);
        if (q.x <= p.x)
            continue;
        float dydt = v + u * t;               // sign of dBy/dt (up vs down)
        if (dydt > 0.0f)      ++wn;
        else if (dydt < 0.0f) --wn;
    }
    return wn;
}

rc_image font_sdf_render(rc_view_font_edge edges, rc_vec2i size, rc_vec2f origin,
                         float spread, rc_arena *arena, rc_arena scratch)
{
    RC_ASSERT(arena && arena->base != scratch.base);
    RC_ASSERT(size.x > 0 && size.y > 0);
    RC_ASSERT(spread > 0.0f);

    uint32_t n = edges.num;
    rc_array_sdf_edge es = rc_array_sdf_edge_make(n, &scratch);
    for (uint32_t k = 0; k < n; ++k)
        rc_array_sdf_edge_push(&es, sdf_edge_make(rc_view_font_edge_get(edges, k)), &scratch);

    rc_image img = rc_image_make(size, RC_PIXEL_FORMAT_R8, arena);
    float inv = 128.0f / spread;
    float cap2 = spread * spread;

    for (int32_t j = 0; j < size.y; ++j) {
        float py = origin.y - (float)j;
        for (int32_t i = 0; i < size.x; ++i) {
            rc_vec2f p = {origin.x + (float)i, py};
            float best2 = cap2;        // distances beyond spread saturate anyway
            int   wn = 0;
            for (uint32_t k = 0; k < n; ++k) {
                const sdf_edge *e = rc_array_sdf_edge_at(&es, k);
                if (e->is_quad) wn += sdf_quad_winding(e, p);
                else            wn += sdf_line_winding(e->p0, e->p2, p);

                if (sdf_bbox_dist2(e, p) >= best2)
                    continue;
                float d2 = e->is_quad ? sdf_quad_dist2(e, p)
                                      : sdf_line_dist2(e->p0, e->p2, p);
                if (d2 < best2)
                    best2 = d2;
            }
            float d  = sqrtf(best2);
            float sd = (wn != 0) ? d : -d;
            float v  = 128.0f + sd * inv;
            int   b  = (int)lrintf(v);
            b = b < 0 ? 0 : (b > 255 ? 255 : b);
            rc_image_set_pixel_r8(img, i, j, (uint32_t)b);
        }
    }
    return img;
}
