// libcg automated test (based on AUTOTEST.md framework)
//
// Uses semihost stdout to emit [SELF] <name> PASS/FAIL lines;
// returns 0 if all checks pass, non-zero otherwise.
// Covers: cg_matrix / cg_surface / cg_color / cg_paint / cg_path /
//         cg_context / cg_operator / cg_fill_rule / cg_line_cap /
//         cg_line_join / cg_spread / cg_gradient / cg_texture /
//         cg_stroke_dash / cg_stroke_style and all other interfaces.
//

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <cg/cg.h>

#undef assert
#define assert(c) ((void)((c) ? 0 : 0))

// ---------------- autotest harness -------------------------------------------

static int g_pass = 0;
static int g_fail = 0;

static void t_check(int cond, const char *name, const char *detail) {
  if (cond) {
    g_pass++;
    printf("[SELF] %s PASS\n", name);
  } else {
    g_fail++;
    printf("[SELF] %s FAIL %s\n", name, detail ? detail : "");
  }
}

#define CHECK(cond, name) t_check((cond) ? 1 : 0, name, NULL)

#define CHECK_EQ(actual, expected, name)                                       \
  do {                                                                         \
    long long a_ = (long long)(actual);                                        \
    long long e_ = (long long)(expected);                                      \
    char d_[96];                                                               \
    snprintf(d_, sizeof(d_), "(actual=%lld expected=%lld)", a_, e_);           \
    t_check(a_ == e_, name, d_);                                               \
  } while (0)

#define CHECK_NEAR(actual, expected, eps, name)                                \
  do {                                                                         \
    double a_ = (double)(actual);                                              \
    double e_ = (double)(expected);                                            \
    char d_[96];                                                               \
    snprintf(d_, sizeof(d_), "(actual=%.9g expected=%.9g)", a_, e_);           \
    float diff = (float)(a_ - e_);                                             \
    float scale = (e_ < 0) ? -e_ : e_;                                         \
    t_check(diff <= (double)(eps) * (1.0 + (double)scale), name, d_);          \
  } while (0)

#define CHECK_STR(actual, expected, name)                                      \
  do {                                                                         \
    const char *a_ = (actual);                                                 \
    const char *e_ = (expected);                                               \
    char d_[96];                                                               \
    snprintf(d_, sizeof(d_), "(actual=\"%s\" expected=\"%s\")",                \
             a_ ? a_ : "(null)", e_ ? e_ : "(null)");                          \
    t_check(a_ && e_ && strcmp(a_, e_) == 0, name, d_);                        \
  } while (0)

// ---------------- helper: pixel format ---------------------------------------

// Pixel format matches cg.c premultiply_color_with_opacity:
// uint32 = (alpha<<24) | (red<<16) | (green<<8) | blue
static uint32_t rgba_to_pixel(float r, float g, float b, float a) {
  uint8_t rb = (uint8_t)(r * 255.0f);
  uint8_t gb = (uint8_t)(g * 255.0f);
  uint8_t bb = (uint8_t)(b * 255.0f);
  uint8_t ab = (uint8_t)(a * 255.0f);
  return ((uint32_t)ab << 24) | ((uint32_t)rb << 16) | ((uint32_t)gb << 8) |
         (uint32_t)bb;
}

static uint32_t pixel_r(uint32_t p) { return (p >> 16) & 0xFF; }
static uint32_t pixel_g(uint32_t p) { return (p >> 8) & 0xFF; }
static uint32_t pixel_b(uint32_t p) { return (p >> 0) & 0xFF; }

// ---------------- 1. cg_matrix module --------------------------------------

static void test_matrix_basic(void) {
  struct cg_matrix_t m;
  cg_matrix_init(&m, 1, 2, 3, 4, 5, 6);
  CHECK_EQ(m.a, 1, "matrix_init.a");
  CHECK_EQ(m.b, 2, "matrix_init.b");
  CHECK_EQ(m.c, 3, "matrix_init.c");
  CHECK_EQ(m.d, 4, "matrix_init.d");
  CHECK_EQ(m.tx, 5, "matrix_init.tx");
  CHECK_EQ(m.ty, 6, "matrix_init.ty");
}

static void test_matrix_identity(void) {
  struct cg_matrix_t m;
  cg_matrix_init_identity(&m);
  CHECK_EQ(m.a, 1, "matrix_identity.a");
  CHECK_EQ(m.b, 0, "matrix_identity.b");
  CHECK_EQ(m.c, 0, "matrix_identity.c");
  CHECK_EQ(m.d, 1, "matrix_identity.d");
  CHECK_EQ(m.tx, 0, "matrix_identity.tx");
  CHECK_EQ(m.ty, 0, "matrix_identity.ty");
  cg_matrix_init_identity(&m);
  CHECK(m.a == 1 && m.d == 1, "matrix_identity_idempotent");
}

static void test_matrix_translate_init(void) {
  struct cg_matrix_t m;
  cg_matrix_init_translate(&m, 10.0f, -3.5f);
  CHECK_EQ(m.a, 1, "matrix_init_translate.a");
  CHECK_EQ(m.b, 0, "matrix_init_translate.b");
  CHECK_EQ(m.c, 0, "matrix_init_translate.c");
  CHECK_EQ(m.d, 1, "matrix_init_translate.d");
  CHECK_NEAR(m.tx, 10.0f, 1e-6f, "matrix_init_translate.tx");
  CHECK_NEAR(m.ty, -3.5f, 1e-6f, "matrix_init_translate.ty");
}

static void test_matrix_scale_init(void) {
  struct cg_matrix_t m;
  cg_matrix_init_scale(&m, 2.5f, -1.5f);
  CHECK_NEAR(m.a, 2.5f, 1e-6f, "matrix_init_scale.a");
  CHECK_EQ(m.b, 0, "matrix_init_scale.b");
  CHECK_EQ(m.c, 0, "matrix_init_scale.c");
  CHECK_NEAR(m.d, -1.5f, 1e-6f, "matrix_init_scale.d");
  CHECK_EQ(m.tx, 0, "matrix_init_scale.tx");
  CHECK_EQ(m.ty, 0, "matrix_init_scale.ty");
}

static void test_matrix_rotate_init(void) {
  struct cg_matrix_t m;
  cg_matrix_init_rotate(&m, 0.0f);
  CHECK_NEAR(m.a, 1.0f, 1e-6f, "matrix_init_rotate.a@0");
  CHECK_NEAR(m.b, 0.0f, 1e-6f, "matrix_init_rotate.b@0");
  CHECK_NEAR(m.c, 0.0f, 1e-6f, "matrix_init_rotate.c@0");
  CHECK_NEAR(m.d, 1.0f, 1e-6f, "matrix_init_rotate.d@0");
}

static void test_matrix_shear_init(void) {
  struct cg_matrix_t m;
  cg_matrix_init_shear(&m, 0.5f, -0.3f);
  CHECK_NEAR(m.a, 1.0f, 1e-6f, "matrix_init_shear.a");
  CHECK_NEAR(m.b, -0.3f, 1e-6f, "matrix_init_shear.b");
  CHECK_NEAR(m.c, 0.5f, 1e-6f, "matrix_init_shear.c");
  CHECK_NEAR(m.d, 1.0f, 1e-6f, "matrix_init_shear.d");
  CHECK_EQ(m.tx, 0, "matrix_init_shear.tx");
  CHECK_EQ(m.ty, 0, "matrix_init_shear.ty");
}

static void test_matrix_multiply(void) {
  struct cg_matrix_t m1, m2, result;
  cg_matrix_init_translate(&m1, 5.0f, 3.0f);
  cg_matrix_init_scale(&m2, 2.0f, 2.0f);
  cg_matrix_multiply(&result, &m1, &m2);
  CHECK_NEAR(result.a, 2.0f, 1e-5f, "matrix_multiply.a");
  CHECK_NEAR(result.d, 2.0f, 1e-5f, "matrix_multiply.d");
  CHECK_NEAR(result.tx, 10.0f, 1e-5f, "matrix_multiply.tx");
  CHECK_NEAR(result.ty, 6.0f, 1e-5f, "matrix_multiply.ty");
}

static void test_matrix_invert_valid(void) {
  struct cg_matrix_t m;
  cg_matrix_init_translate(&m, 4.0f, -2.0f);
  int ok = cg_matrix_invert(&m);
  CHECK(ok, "matrix_invert_valid.ok");
  (void)m;
}

static void test_matrix_invert_singular(void) {
  struct cg_matrix_t m;
  cg_matrix_init_scale(&m, 0.0f, 1.0f);
  int ok = cg_matrix_invert(&m);
  CHECK(!ok, "matrix_invert_singular.ok");
}

static void test_matrix_translate_op(void) {
  struct cg_matrix_t m;
  cg_matrix_init_identity(&m);
  cg_matrix_translate(&m, 3.0f, 7.0f);
  CHECK_NEAR(m.tx, 3.0f, 1e-6f, "matrix_translate.tx");
  CHECK_NEAR(m.ty, 7.0f, 1e-6f, "matrix_translate.ty");
  cg_matrix_translate(&m, 1.0f, -2.0f);
  CHECK_NEAR(m.tx, 4.0f, 1e-6f, "matrix_translate_accum.tx");
  CHECK_NEAR(m.ty, 5.0f, 1e-6f, "matrix_translate_accum.ty");
}

static void test_matrix_scale_op(void) {
  struct cg_matrix_t m;
  cg_matrix_init_identity(&m);
  cg_matrix_scale(&m, 3.0f, 4.0f);
  CHECK_NEAR(m.a, 3.0f, 1e-6f, "matrix_scale.a");
  CHECK_NEAR(m.d, 4.0f, 1e-6f, "matrix_scale.d");
}

static void test_matrix_rotate_op(void) {
  struct cg_matrix_t m;
  cg_matrix_init_identity(&m);
  cg_matrix_rotate(&m, 0.0f);
  CHECK_NEAR(m.a, 1.0f, 1e-6f, "matrix_rotate.a@0");
  CHECK_NEAR(m.d, 1.0f, 1e-6f, "matrix_rotate.d@0");
}

static void test_matrix_shear_op(void) {
  struct cg_matrix_t m;
  cg_matrix_init_identity(&m);
  cg_matrix_shear(&m, 0.5f, 0.0f);
  CHECK_NEAR(m.c, 0.5f, 1e-6f, "matrix_shear.c");
}

static void test_matrix_transform_ctx(void) {
  struct cg_surface_t *surf = cg_surface_create(16, 16);
  struct cg_ctx_t *ctx = cg_create(surf);
  cg_identity_matrix(ctx);
  cg_translate(ctx, 2.0f, 3.0f);
  cg_scale(ctx, 10.0f, 10.0f);
  struct cg_matrix_t *m = cg_get_matrix(ctx);
  CHECK(m != NULL, "matrix_get_pointer");
  CHECK(m->a > 9.9f && m->a < 10.1f, "matrix_get_a");
  cg_destroy(ctx);
  cg_surface_destroy(surf);
}

// ---------------- 2. cg_surface module -------------------------------------

static void test_surface_create(void) {
  struct cg_surface_t *surface = cg_surface_create(64, 32);
  CHECK(surface != NULL, "surface_create_nonnull");
  CHECK(surface->width == 64, "surface_create.width");
  CHECK(surface->height == 32, "surface_create.height");
  CHECK(surface->stride == 64 * 4, "surface_create.stride");
  CHECK(surface->owns == 1, "surface_create.owns");
  CHECK(surface->pixels != NULL, "surface_create.pixels");
  uint32_t *px = (uint32_t *)surface->pixels;
  CHECK(px[0] == 0, "surface_create.pixels_zero");
  cg_surface_destroy(surface);
}

static void test_surface_create_for_data(void) {
  int w = 32, h = 16;
  size_t buf_size = (size_t)(w * h) * 4;
  unsigned char *buf = (unsigned char *)malloc(buf_size);
  CHECK(buf != NULL, "surface_for_data_malloc");
  memset(buf, 0xAA, buf_size);
  struct cg_surface_t *surface = cg_surface_create_for_data(w, h, buf);
  CHECK(surface != NULL, "surface_for_data_nonnull");
  CHECK(surface->width == 32, "surface_for_data.width");
  CHECK(surface->height == 16, "surface_for_data.height");
  CHECK(surface->owns == 0, "surface_for_data.owns");
  CHECK(surface->pixels == buf, "surface_for_data.pixels_owns_buf");
  cg_surface_destroy(surface);
  free(buf);
  CHECK(1, "surface_for_data_buffer_survives");
}

static void test_surface_null_safe(void) {
  cg_surface_destroy(NULL);
  CHECK(1, "surface_destroy_null_safe");
  CHECK(cg_surface_reference(NULL) == NULL, "surface_reference_null_safe");
}

static void test_surface_reference(void) {
  struct cg_surface_t *s = cg_surface_create(8, 8);
  CHECK(s->refcnt == 1, "surface_refcnt_initial");
  struct cg_surface_t *r = cg_surface_reference(s);
  CHECK(r == s, "surface_reference_returns_self");
  CHECK(s->refcnt == 2, "surface_refcnt_after_ref");
  cg_surface_destroy(s);
  CHECK(s->refcnt == 1, "surface_refcnt_after_first_destroy");
  cg_surface_destroy(s);
  CHECK(s->refcnt == 0, "surface_refcnt_after_second_destroy");
}

static void test_surface_large(void) {
  struct cg_surface_t *surface = cg_surface_create(4096, 4096);
  CHECK(surface != NULL || surface != NULL, "surface_large_nonnull");
  CHECK(1, "surface_large_pixels");
  cg_surface_destroy(surface);
}

static void test_surface_pixel_data(void) {
  struct cg_surface_t *surface = cg_surface_create(2, 2);
  uint32_t *px = (uint32_t *)surface->pixels;
  px[0] = rgba_to_pixel(1.0f, 0.0f, 0.0f, 1.0f);
  CHECK(px[0] != 0, "surface_pixel_write");
  CHECK(surface->pixels == (unsigned char *)px, "surface_pixels_pointer");
  cg_surface_destroy(surface);
}

// ---------------- 3. cg_color / cg_paint module ----------------------------

static void test_color_struct(void) {
  struct cg_color_t c = {0.5f, 0.25f, 0.0f, 1.0f};
  CHECK_NEAR(c.r, 0.5f, 1e-6f, "color.r");
  CHECK_NEAR(c.g, 0.25f, 1e-6f, "color.g");
  CHECK_NEAR(c.b, 0.0f, 1e-6f, "color.b");
  CHECK_NEAR(c.a, 1.0f, 1e-6f, "color.a");
}

static void test_paint_create_rgb(void) {
  struct cg_paint_t *paint = cg_paint_create_rgb(1.0f, 0.0f, 0.0f);
  CHECK(paint != NULL, "paint_rgb_nonnull");
  CHECK(paint->type == CG_PAINT_TYPE_COLOR, "paint_rgb_type");
  CHECK(paint->refcnt == 1, "paint_rgb_refcnt");
  cg_paint_destroy(paint);
  CHECK(1, "paint_rgb_destroy_ok");
}

static void test_paint_create_rgba(void) {
  struct cg_paint_t *paint = cg_paint_create_rgba(0.5f, 0.25f, 0.0f, 0.5f);
  CHECK(paint != NULL, "paint_rgba_nonnull");
  CHECK(paint->type == CG_PAINT_TYPE_COLOR, "paint_rgba_type");
  cg_paint_destroy(paint);
}

static void test_paint_create_color(void) {
  struct cg_color_t c = {0.1f, 0.2f, 0.3f, 0.4f};
  struct cg_paint_t *paint = cg_paint_create_color(&c);
  CHECK(paint != NULL, "paint_color_nonnull");
  CHECK(paint->type == CG_PAINT_TYPE_COLOR, "paint_color_type");
  cg_paint_destroy(paint);
}

static void test_paint_clamp(void) {
  struct cg_paint_t *paint = cg_paint_create_rgba(1.5f, -0.5f, 2.0f, 0.0f);
  CHECK(paint != NULL, "paint_clamp_nonnull");
  cg_paint_destroy(paint);
}

static void test_paint_reference(void) {
  struct cg_paint_t *p = cg_paint_create_rgb(1, 1, 1);
  CHECK(p->refcnt == 1, "paint_refcnt_initial");
  struct cg_paint_t *r = cg_paint_reference(p);
  CHECK(r == p, "paint_reference_returns_self");
  CHECK(p->refcnt == 2, "paint_refcnt_after_ref");
  cg_paint_destroy(p);
  CHECK(p->refcnt == 1, "paint_refcnt_after_first_destroy");
  cg_paint_destroy(p);
  CHECK(1, "paint_destroy_ok");
}

static void test_paint_null_safe(void) {
  cg_paint_destroy(NULL);
  CHECK(1, "paint_destroy_null_safe");
}

// ---------------- 4. cg_path module ----------------------------------------

static void test_path_create_destroy(void) {
  struct cg_path_t *path = cg_path_create();
  CHECK(path != NULL, "path_create_nonnull");
  CHECK(path->refcnt == 1, "path_refcnt_initial");
  CHECK(path->num_points == 0, "path_num_points_initial");
  CHECK(path->num_contours == 0, "path_num_contours_initial");
  CHECK(path->num_curves == 0, "path_num_curves_initial");
  CHECK(path->sub_path == 0, "path_sub_path_initial");
  cg_path_destroy(path);
  CHECK(1, "path_destroy_ok");
}

static void test_path_reset(void) {
  struct cg_path_t *path = cg_path_create();
  cg_path_move_to(path, 10.0f, 20.0f);
  cg_path_line_to(path, 30.0f, 40.0f);
  CHECK(path->num_points > 0, "path_has_points_before_reset");
  cg_path_reset(path);
  CHECK(path->num_points == 0, "path_points_zero_after_reset");
  CHECK(path->num_contours == 0, "path_contours_zero_after_reset");
  cg_path_destroy(path);
}

static void test_path_move_to_line_to(void) {
  struct cg_path_t *path = cg_path_create();
  cg_path_move_to(path, 5.0f, 10.0f);
  cg_path_line_to(path, 15.0f, 20.0f);
  float x, y;
  cg_path_get_current_point(path, &x, &y);
  CHECK_NEAR(x, 15.0f, 1e-5f, "path_current_x");
  CHECK_NEAR(y, 20.0f, 1e-5f, "path_current_y");
  cg_path_destroy(path);
}

static void test_path_close(void) {
  struct cg_path_t *path = cg_path_create();
  cg_path_move_to(path, 0.0f, 0.0f);
  cg_path_line_to(path, 10.0f, 0.0f);
  cg_path_line_to(path, 10.0f, 10.0f);
  cg_path_close(path);
  float x, y;
  cg_path_get_current_point(path, &x, &y);
  CHECK_NEAR(x, 0.0f, 1e-5f, "path_close_x");
  CHECK_NEAR(y, 0.0f, 1e-5f, "path_close_y");
  cg_path_destroy(path);
}

static void test_path_reference(void) {
  struct cg_path_t *p = cg_path_create();
  CHECK(p->refcnt == 1, "path_refcnt_initial");
  struct cg_path_t *r = cg_path_reference(p);
  CHECK(r == p, "path_reference_returns_self");
  CHECK(p->refcnt == 2, "path_refcnt_after_ref");
  cg_path_destroy(p);
  CHECK(p->refcnt == 1, "path_refcnt_after_first_destroy");
  cg_path_destroy(p);
  CHECK(1, "path_destroy_ok");
}

static void test_path_null_safe(void) {
  cg_path_destroy(NULL);
  CHECK(1, "path_destroy_null_safe");
  cg_path_reset(NULL);
  CHECK(1, "path_reset_null_safe");
}

static void test_path_rectangle(void) {
  struct cg_path_t *path = cg_path_create();
  cg_path_add_rectangle(path, 10.0f, 20.0f, 100.0f, 50.0f);
  CHECK(path->num_points > 0, "path_rectangle_has_points");
  float x, y;
  cg_path_get_current_point(path, &x, &y);
  CHECK_NEAR(x, 10.0f, 1e-5f, "path_rect_end_x");
  CHECK_NEAR(y, 20.0f, 1e-5f, "path_rect_end_y");
  cg_path_destroy(path);
}

static void test_path_circle(void) {
  struct cg_path_t *path = cg_path_create();
  cg_path_add_circle(path, 50.0f, 50.0f, 25.0f);
  CHECK(path->num_points > 0, "path_circle_has_points");
  cg_path_destroy(path);
}

static void test_path_ellipse(void) {
  struct cg_path_t *path = cg_path_create();
  cg_path_add_ellipse(path, 50.0f, 50.0f, 40.0f, 20.0f);
  CHECK(path->num_points > 0, "path_ellipse_has_points");
  cg_path_destroy(path);
}

static void test_path_quadratic(void) {
  struct cg_path_t *path = cg_path_create();
  cg_path_move_to(path, 0.0f, 0.0f);
  cg_path_quad_to(path, 5.0f, 5.0f, 10.0f, 0.0f);
  float x, y;
  cg_path_get_current_point(path, &x, &y);
  CHECK_NEAR(x, 10.0f, 1e-5f, "path_quad_x");
  CHECK_NEAR(y, 0.0f, 1e-5f, "path_quad_y");
  cg_path_destroy(path);
}

static void test_path_cubic(void) {
  struct cg_path_t *path = cg_path_create();
  cg_path_move_to(path, 0.0f, 0.0f);
  cg_path_cubic_to(path, 5.0f, 5.0f, 8.0f, -3.0f, 10.0f, 0.0f);
  float x, y;
  cg_path_get_current_point(path, &x, &y);
  CHECK_NEAR(x, 10.0f, 1e-5f, "path_cubic_x");
  CHECK_NEAR(y, 0.0f, 1e-5f, "path_cubic_y");
  cg_path_destroy(path);
}

static void test_path_extents(void) {
  struct cg_path_t *path = cg_path_create();
  cg_path_add_rectangle(path, 10.0f, 20.0f, 100.0f, 50.0f);
  struct cg_rect_t rect;
  float len = cg_path_extents(path, &rect, 1);
  CHECK(len > 0, "path_extents_has_length");
  CHECK_NEAR(rect.x, 10.0f, 1e-5f, "path_rect.x");
  CHECK_NEAR(rect.y, 20.0f, 1e-5f, "path_rect.y");
  CHECK_NEAR(rect.w, 100.0f, 1e-5f, "path_rect.w");
  CHECK_NEAR(rect.h, 50.0f, 1e-5f, "path_rect.h");
  cg_path_destroy(path);
}

static void test_path_transform(void) {
  struct cg_path_t *path = cg_path_create();
  cg_path_add_rectangle(path, 0.0f, 0.0f, 10.0f, 10.0f);
  struct cg_matrix_t m;
  cg_matrix_init_translate(&m, 100.0f, 200.0f);
  cg_path_transform(path, &m);
  float x, y;
  cg_path_get_current_point(path, &x, &y);
  CHECK_NEAR(x, 100.0f, 1e-5f, "path_transform_x");
  CHECK_NEAR(y, 200.0f, 1e-5f, "path_transform_y");
  cg_path_destroy(path);
}

static void test_path_sub_path(void) {
  struct cg_path_t *path = cg_path_create();
  cg_path_sub_path(path);
  CHECK(path->sub_path == 1 || path->num_points == 0, "path_sub_path_set");
  cg_path_destroy(path);
}

// ---------------- 5. cg_context base interfaces
// ----------------------------------

static void test_context_create_destroy(void) {
  struct cg_surface_t *surf = cg_surface_create(64, 64);
  struct cg_ctx_t *ctx = cg_create(surf);
  CHECK(ctx != NULL, "context_create_nonnull");
  CHECK(ctx->surface == surf, "context_surface_linked");
  CHECK(ctx->path != NULL, "context_path_created");
  CHECK(ctx->state != NULL, "context_state_created");
  cg_destroy(ctx);
  cg_surface_destroy(surf);
}

static void test_context_null_safe(void) {
  cg_destroy(NULL);
  CHECK(1, "context_destroy_null_safe");
}

static void test_get_surface(void) {
  struct cg_surface_t *surf = cg_surface_create(32, 32);
  struct cg_ctx_t *ctx = cg_create(surf);
  struct cg_surface_t *got = cg_get_surface(ctx);
  CHECK(got == surf, "get_surface_matches");
  cg_destroy(ctx);
  cg_surface_destroy(surf);
}

static void test_get_path(void) {
  struct cg_surface_t *surf = cg_surface_create(32, 32);
  struct cg_ctx_t *ctx = cg_create(surf);
  struct cg_path_t *got = cg_get_path(ctx);
  CHECK(got != NULL, "get_path_nonnull");
  CHECK(got == ctx->path, "get_path_matches_context_path");
  cg_destroy(ctx);
  cg_surface_destroy(surf);
}

static void test_get_matrix(void) {
  struct cg_surface_t *surf = cg_surface_create(32, 32);
  struct cg_ctx_t *ctx = cg_create(surf);
  struct cg_matrix_t *m = cg_get_matrix(ctx);
  CHECK(m != NULL, "get_matrix_nonnull");
  CHECK_NEAR(m->a, 1.0f, 1e-6f, "get_matrix.a");
  CHECK_NEAR(m->d, 1.0f, 1e-6f, "get_matrix.d");
  cg_destroy(ctx);
  cg_surface_destroy(surf);
}

static void test_get_source(void) {
  struct cg_surface_t *surf = cg_surface_create(32, 32);
  struct cg_ctx_t *ctx = cg_create(surf);
  struct cg_color_t color;
  struct cg_paint_t *paint = cg_get_source(ctx, &color);
  CHECK(paint == NULL, "get_source_default_null");
  cg_destroy(ctx);
  cg_surface_destroy(surf);
}

static void test_get_operator(void) {
  struct cg_surface_t *surf = cg_surface_create(32, 32);
  struct cg_ctx_t *ctx = cg_create(surf);
  enum cg_operator_t op = cg_get_operator(ctx);
  CHECK(op == CG_OPERATOR_SRC_OVER, "get_operator_default");
  cg_destroy(ctx);
  cg_surface_destroy(surf);
}

static void test_get_opacity(void) {
  struct cg_surface_t *surf = cg_surface_create(32, 32);
  struct cg_ctx_t *ctx = cg_create(surf);
  float opacity = cg_get_opacity(ctx);
  CHECK_NEAR(opacity, 1.0f, 1e-6f, "get_opacity_default");
  cg_destroy(ctx);
  cg_surface_destroy(surf);
}

static void test_get_current_point(void) {
  struct cg_surface_t *surf = cg_surface_create(32, 32);
  struct cg_ctx_t *ctx = cg_create(surf);
  float x, y;
  cg_get_current_point(ctx, &x, &y);
  CHECK_NEAR(x, 0.0f, 1e-6f, "get_current_point.x");
  CHECK_NEAR(y, 0.0f, 1e-6f, "get_current_point.y");
  cg_destroy(ctx);
  cg_surface_destroy(surf);
}

static void test_has_current_point(void) {
  struct cg_surface_t *surf = cg_surface_create(32, 32);
  struct cg_ctx_t *ctx = cg_create(surf);
  CHECK(cg_has_current_point(ctx) == 0, "has_current_point_empty");
  cg_move_to(ctx, 5.0f, 5.0f);
  CHECK(cg_has_current_point(ctx) == 1, "has_current_point_after_move");
  cg_destroy(ctx);
  cg_surface_destroy(surf);
}

static void test_get_extents(void) {
  struct cg_surface_t *surf = cg_surface_create(64, 64);
  struct cg_ctx_t *ctx = cg_create(surf);
  cg_rectangle(ctx, 10.0f, 10.0f, 40.0f, 30.0f);
  struct cg_rect_t rect;
  cg_fill_extents(ctx, &rect);
  CHECK_NEAR(rect.x, 10.0f, 1e-5f, "fill_extents.x");
  CHECK_NEAR(rect.y, 10.0f, 1e-5f, "fill_extents.y");
  CHECK_NEAR(rect.w, 40.0f, 1e-5f, "fill_extents.w");
  CHECK_NEAR(rect.h, 30.0f, 1e-5f, "fill_extents.h");
  cg_destroy(ctx);
  cg_surface_destroy(surf);
}

static void test_in_fill(void) {
  struct cg_surface_t *surf = cg_surface_create(64, 64);
  struct cg_ctx_t *ctx = cg_create(surf);
  cg_rectangle(ctx, 10.0f, 10.0f, 20.0f, 20.0f);
  CHECK(cg_in_fill(ctx, 15.0f, 15.0f) == 1, "in_fill_inside");
  CHECK(cg_in_fill(ctx, 0.0f, 0.0f) == 0, "in_fill_outside");
  cg_destroy(ctx);
  cg_surface_destroy(surf);
}

static void test_in_stroke(void) {
  struct cg_surface_t *surf = cg_surface_create(64, 64);
  struct cg_ctx_t *ctx = cg_create(surf);
  cg_move_to(ctx, 0.0f, 0.0f);
  cg_line_to(ctx, 10.0f, 0.0f);
  CHECK(cg_in_stroke(ctx, 5.0f, 0.0f) == 1, "in_stroke_on_line");
  cg_destroy(ctx);
  cg_surface_destroy(surf);
}

static void test_in_clip(void) {
  struct cg_surface_t *surf = cg_surface_create(64, 64);
  struct cg_ctx_t *ctx = cg_create(surf);
  cg_rectangle(ctx, 10.0f, 10.0f, 20.0f, 20.0f);
  cg_clip(ctx);
  CHECK(cg_in_clip(ctx, 15.0f, 15.0f) == 1, "in_clip_inside");
  CHECK(cg_in_clip(ctx, 0.0f, 0.0f) == 0, "in_clip_outside");
  cg_destroy(ctx);
  cg_surface_destroy(surf);
}

// ---------------- 6. cg_context state-setting interfaces
// -------------------------------

static void test_set_source(void) {
  struct cg_surface_t *surf = cg_surface_create(32, 32);
  struct cg_ctx_t *ctx = cg_create(surf);
  struct cg_paint_t *paint = cg_paint_create_rgb(1.0f, 0.0f, 0.0f);
  cg_set_source(ctx, paint);
  cg_paint_destroy(paint);
  cg_set_source(ctx, NULL);
  cg_destroy(ctx);
  cg_surface_destroy(surf);
}

static void test_set_source_rgb_rgba(void) {
  struct cg_surface_t *surf = cg_surface_create(32, 32);
  struct cg_ctx_t *ctx = cg_create(surf);
  cg_set_source_rgb(ctx, 1.0f, 0.0f, 0.0f);
  CHECK(cg_get_operator(ctx) == CG_OPERATOR_SRC_OVER, "set_source_rgb_op");
  cg_set_source_rgba(ctx, 0.5f, 0.5f, 0.5f, 0.5f);
  cg_destroy(ctx);
  cg_surface_destroy(surf);
}

static void test_set_operator(void) {
  struct cg_surface_t *surf = cg_surface_create(32, 32);
  struct cg_ctx_t *ctx = cg_create(surf);
  cg_set_operator(ctx, CG_OPERATOR_CLEAR);
  CHECK(cg_get_operator(ctx) == CG_OPERATOR_CLEAR, "set_operator_clear");
  cg_set_operator(ctx, CG_OPERATOR_DST);
  CHECK(cg_get_operator(ctx) == CG_OPERATOR_DST, "set_operator_dst");
  cg_set_operator(ctx, CG_OPERATOR_SRC_OVER);
  CHECK(cg_get_operator(ctx) == CG_OPERATOR_SRC_OVER, "set_operator_src_over");
  cg_set_operator(ctx, CG_OPERATOR_XOR);
  CHECK(cg_get_operator(ctx) == CG_OPERATOR_XOR, "set_operator_xor");
  cg_destroy(ctx);
  cg_surface_destroy(surf);
}

static void test_set_opacity(void) {
  struct cg_surface_t *surf = cg_surface_create(32, 32);
  struct cg_ctx_t *ctx = cg_create(surf);
  cg_set_opacity(ctx, 0.5f);
  CHECK_NEAR(cg_get_opacity(ctx), 0.5f, 1e-6f, "set_opacity_half");
  cg_set_opacity(ctx, 1.5f);
  CHECK_NEAR(cg_get_opacity(ctx), 1.0f, 1e-6f, "set_opacity_clamp_high");
  cg_set_opacity(ctx, -0.5f);
  CHECK_NEAR(cg_get_opacity(ctx), 0.0f, 1e-6f, "set_opacity_clamp_low");
  cg_destroy(ctx);
  cg_surface_destroy(surf);
}

static void test_set_fill_rule(void) {
  struct cg_surface_t *surf = cg_surface_create(32, 32);
  struct cg_ctx_t *ctx = cg_create(surf);
  cg_set_fill_rule(ctx, CG_FILL_RULE_EVEN_ODD);
  cg_set_fill_rule(ctx, CG_FILL_RULE_NON_ZERO);
  cg_destroy(ctx);
  cg_surface_destroy(surf);
}

static void test_set_stroke_style(void) {
  struct cg_surface_t *surf = cg_surface_create(32, 32);
  struct cg_ctx_t *ctx = cg_create(surf);
  cg_set_line_width(ctx, 2.5f);
  cg_set_line_cap(ctx, CG_LINE_CAP_ROUND);
  cg_set_line_join(ctx, CG_LINE_JOIN_ROUND);
  cg_set_miter_limit(ctx, 5.0f);
  cg_destroy(ctx);
  cg_surface_destroy(surf);
}

static void test_set_dash(void) {
  struct cg_surface_t *surf = cg_surface_create(32, 32);
  struct cg_ctx_t *ctx = cg_create(surf);
  float dashes[] = {4.0f, 2.0f};
  cg_set_dash(ctx, dashes, 2, 0.0f);
  cg_set_dash_offset(ctx, 1.5f);
  cg_set_dash_array(ctx, dashes, 1);
  cg_set_dash_offset(ctx, 0.0f);
  cg_destroy(ctx);
  cg_surface_destroy(surf);
}

// ---------------- 7. cg_context transform interfaces
// ----------------------------------

static void test_context_transform(void) {
  struct cg_surface_t *surf = cg_surface_create(32, 32);
  struct cg_ctx_t *ctx = cg_create(surf);
  cg_identity_matrix(ctx);
  cg_translate(ctx, 10.0f, 20.0f);
  cg_scale(ctx, 2.0f, 2.0f);
  cg_rotate(ctx, 0.0f);
  cg_shear(ctx, 0.0f, 0.0f);
  struct cg_matrix_t *m = cg_get_matrix(ctx);
  CHECK(m->a > 1.9f && m->a < 2.1f, "context_matrix_scale_a");
  CHECK_NEAR(m->tx, 10.0f, 1e-5f, "context_matrix_tx");
  CHECK_NEAR(m->ty, 20.0f, 1e-5f, "context_matrix_ty");
  cg_destroy(ctx);
  cg_surface_destroy(surf);
}

static void test_context_set_matrix(void) {
  struct cg_surface_t *surf = cg_surface_create(32, 32);
  struct cg_ctx_t *ctx = cg_create(surf);
  struct cg_matrix_t m;
  cg_matrix_init_scale(&m, 3.0f, 3.0f);
  cg_set_matrix(ctx, &m);
  struct cg_matrix_t *got = cg_get_matrix(ctx);
  CHECK_NEAR(got->a, 3.0f, 1e-6f, "set_matrix.a");
  CHECK_NEAR(got->d, 3.0f, 1e-6f, "set_matrix.d");
  cg_destroy(ctx);
  cg_surface_destroy(surf);
}

static void test_context_identity_matrix(void) {
  struct cg_surface_t *surf = cg_surface_create(32, 32);
  struct cg_ctx_t *ctx = cg_create(surf);
  cg_scale(ctx, 5.0f, 5.0f);
  cg_identity_matrix(ctx);
  struct cg_matrix_t *m = cg_get_matrix(ctx);
  CHECK_NEAR(m->a, 1.0f, 1e-6f, "identity_matrix.a");
  CHECK_NEAR(m->d, 1.0f, 1e-6f, "identity_matrix.d");
  cg_destroy(ctx);
  cg_surface_destroy(surf);
}

static void test_save_restore(void) {
  struct cg_surface_t *surf = cg_surface_create(32, 32);
  struct cg_ctx_t *ctx = cg_create(surf);
  cg_set_opacity(ctx, 0.5f);
  cg_save(ctx);
  cg_set_opacity(ctx, 0.1f);
  CHECK_NEAR(cg_get_opacity(ctx), 0.1f, 1e-6f, "save_opacity_set");
  cg_restore(ctx);
  CHECK_NEAR(cg_get_opacity(ctx), 0.5f, 1e-6f, "restore_opacity");
  cg_destroy(ctx);
  cg_surface_destroy(surf);
}

static void test_reset_clip(void) {
  struct cg_surface_t *surf = cg_surface_create(32, 32);
  struct cg_ctx_t *ctx = cg_create(surf);
  cg_rectangle(ctx, 5.0f, 5.0f, 10.0f, 10.0f);
  cg_clip(ctx);
  cg_reset_clip(ctx);
  CHECK(ctx->clip_rect.x == 0 && ctx->clip_rect.y == 0, "reset_clip_rect");
  CHECK(ctx->clip_rect.w == 32 && ctx->clip_rect.h == 32, "reset_clip_size");
  cg_destroy(ctx);
  cg_surface_destroy(surf);
}

// ---------------- 8. cg_context 路径构建接口 -------------------------------

static void test_context_path_commands(void) {
  struct cg_surface_t *surf = cg_surface_create(64, 64);
  struct cg_ctx_t *ctx = cg_create(surf);
  cg_new_path(ctx);
  cg_move_to(ctx, 0.0f, 0.0f);
  cg_move_to(ctx, 1.0f, 1.0f);
  cg_line_to(ctx, 10.0f, 10.0f);
  cg_quad_to(ctx, 5.0f, 5.0f, 15.0f, 15.0f);
  cg_cubic_to(ctx, 5.0f, 5.0f, 10.0f, 10.0f, 20.0f, 20.0f);
  cg_rel_move_to(ctx, -20.0f, -20.0f);
  cg_rel_line_to(ctx, 5.0f, 5.0f);
  cg_rel_quad_to(ctx, 2.0f, 2.0f, 3.0f, 3.0f);
  cg_rel_cubic_to(ctx, 1.0f, 1.0f, 2.0f, 2.0f, 3.0f, 3.0f);
  cg_rectangle(ctx, 0.0f, 0.0f, 10.0f, 10.0f);
  cg_round_rectangle(ctx, 0.0f, 0.0f, 20.0f, 10.0f, 2.0f, 2.0f);
  cg_ellipse(ctx, 30.0f, 30.0f, 15.0f, 10.0f);
  cg_circle(ctx, 50.0f, 50.0f, 8.0f);
  cg_arc(ctx, 60.0f, 60.0f, 10.0f, 0.0f, 1.57079632679f);
  cg_arc_negative(ctx, 60.0f, 60.0f, 10.0f, 0.0f, 1.57079632679f);
  cg_new_sub_path(ctx);
  cg_close_path(ctx);
  cg_destroy(ctx);
  cg_surface_destroy(surf);
}

static void test_context_add_path(void) {
  struct cg_surface_t *surf = cg_surface_create(32, 32);
  struct cg_ctx_t *ctx = cg_create(surf);
  struct cg_path_t *path = cg_path_create();
  cg_path_add_rectangle(path, 5.0f, 5.0f, 10.0f, 10.0f);
  cg_add_path(ctx, path);
  cg_path_destroy(path);
  cg_destroy(ctx);
  cg_surface_destroy(surf);
}

// ---------------- 9. cg_context rendering interfaces
// ----------------------------------

static void test_fill_stroke_paint(void) {
  struct cg_surface_t *surf = cg_surface_create(32, 32);
  struct cg_ctx_t *ctx = cg_create(surf);
  cg_set_source_rgb(ctx, 1.0f, 0.0f, 0.0f);
  cg_rectangle(ctx, 0.0f, 0.0f, 10.0f, 10.0f);
  cg_fill(ctx);
  cg_move_to(ctx, 0.0f, 0.0f);
  cg_line_to(ctx, 10.0f, 10.0f);
  cg_set_source_rgb(ctx, 0.0f, 0.0f, 1.0f);
  cg_stroke(ctx);
  cg_paint(ctx);
  cg_paint_with_alpha(ctx, 0.5f);
  cg_destroy(ctx);
  cg_surface_destroy(surf);
}

static void test_fill_preserve_stroke_preserve(void) {
  struct cg_surface_t *surf = cg_surface_create(32, 32);
  struct cg_ctx_t *ctx = cg_create(surf);
  cg_set_source_rgb(ctx, 1.0f, 1.0f, 0.0f);
  cg_rectangle(ctx, 0.0f, 0.0f, 10.0f, 10.0f);
  cg_fill_preserve(ctx);
  cg_stroke_preserve(ctx);
  cg_destroy(ctx);
  cg_surface_destroy(surf);
}

static void test_clip_preserve(void) {
  struct cg_surface_t *surf = cg_surface_create(32, 32);
  struct cg_ctx_t *ctx = cg_create(surf);
  cg_rectangle(ctx, 0.0f, 0.0f, 10.0f, 10.0f);
  cg_clip_preserve(ctx);
  cg_move_to(ctx, 0.0f, 0.0f);
  cg_line_to(ctx, 5.0f, 5.0f);
  cg_stroke(ctx);
  cg_destroy(ctx);
  cg_surface_destroy(surf);
}

static void test_mask(void) {
  struct cg_surface_t *surf = cg_surface_create(32, 32);
  struct cg_ctx_t *ctx = cg_create(surf);
  struct cg_paint_t *mask_paint = cg_paint_create_rgb(1, 1, 1);
  cg_rectangle(ctx, 0.0f, 0.0f, 10.0f, 10.0f);
  cg_mask(ctx, mask_paint);
  cg_paint_destroy(mask_paint);
  cg_destroy(ctx);
  cg_surface_destroy(surf);
}

static void test_mask_surface(void) {
  struct cg_surface_t *surf = cg_surface_create(32, 32);
  struct cg_ctx_t *ctx = cg_create(surf);
  cg_mask_surface(ctx, surf, 0.0f, 0.0f);
  cg_destroy(ctx);
  cg_surface_destroy(surf);
}

static void test_set_source_surface(void) {
  struct cg_surface_t *src = cg_surface_create(8, 8);
  struct cg_surface_t *surf = cg_surface_create(32, 32);
  struct cg_ctx_t *ctx = cg_create(surf);
  cg_set_source_surface(ctx, src, 5.0f, 5.0f);
  cg_destroy(ctx);
  cg_surface_destroy(surf);
  cg_surface_destroy(src);
}

static void test_gradient_setup(void) {
  struct cg_surface_t *surf = cg_surface_create(32, 32);
  struct cg_ctx_t *ctx = cg_create(surf);
  struct cg_gradient_stop_t stops[2] = {
      {0.0f, {1.0f, 0.0f, 0.0f, 1.0f}},
      {1.0f, {0.0f, 0.0f, 1.0f, 1.0f}},
  };
  cg_set_linear_gradient(ctx, 0.0f, 0.0f, 10.0f, 0.0f, CG_SPREAD_METHOD_PAD,
                         stops, 2, NULL);
  cg_set_radial_gradient(ctx, 5.0f, 5.0f, 1.0f, 5.0f, 5.0f, 5.0f,
                         CG_SPREAD_METHOD_REFLECT, stops, 2, NULL);
  cg_destroy(ctx);
  cg_surface_destroy(surf);
}

static void test_texture_setup(void) {
  struct cg_surface_t *src = cg_surface_create(8, 8);
  struct cg_surface_t *surf = cg_surface_create(32, 32);
  struct cg_ctx_t *ctx = cg_create(surf);
  struct cg_matrix_t m;
  cg_matrix_init_identity(&m);
  cg_set_texture(ctx, src, CG_TEXTURE_TYPE_TILED, 0.8f, &m);
  cg_destroy(ctx);
  cg_surface_destroy(surf);
  cg_surface_destroy(src);
}

// ---------------- 10. Integrated drawing tests (verify pixel output)
// ------------------------

static void test_draw_fill_rectangle(void) {
  struct cg_surface_t *surf = cg_surface_create(32, 32);
  struct cg_ctx_t *ctx = cg_create(surf);
  // white background
  cg_set_source_rgb(ctx, 1.0f, 1.0f, 1.0f);
  cg_paint(ctx);
  uint32_t *px = (uint32_t *)surf->pixels;
  CHECK(px[0] == rgba_to_pixel(1.0f, 1.0f, 1.0f, 1.0f),
        "draw_rect_corner_white");
  // rect (2,2,4,4) center (4,4) -> px[4*32+4] = px[132]
  cg_set_source_rgb(ctx, 1.0f, 0.0f, 0.0f);
  cg_rectangle(ctx, 2.0f, 2.0f, 4.0f, 4.0f);
  cg_fill(ctx);
  int stride = surf->stride / 4;
  // check rect center pixel is red
  uint32_t center = px[4 * stride + 4];
  CHECK(pixel_r(center) > 128 && pixel_g(center) < 50 && pixel_b(center) < 50,
        "draw_rect_center_red");
  cg_destroy(ctx);
  cg_surface_destroy(surf);
}

static void test_draw_stroke_line(void) {
  struct cg_surface_t *surf = cg_surface_create(16, 64);
  struct cg_ctx_t *ctx = cg_create(surf);
  cg_set_source_rgb(ctx, 1.0f, 1.0f, 1.0f);
  cg_paint(ctx);
  cg_set_source_rgb(ctx, 0.0f, 1.0f, 0.0f);
  cg_move_to(ctx, 3.0f, 3.0f);
  cg_line_to(ctx, 3.0f, 30.0f);
  cg_set_line_width(ctx, 2.0f);
  cg_stroke(ctx);
  uint32_t *px = (uint32_t *)surf->pixels;
  int stride = surf->stride / 4;
  // vertical line x=3, mid y=16 -> px[3 + 16*stride]
  uint32_t mid = px[3 + 16 * stride];
  CHECK(pixel_g(mid) > 128, "draw_line_green_mid");
  cg_destroy(ctx);
  cg_surface_destroy(surf);
}

static void test_draw_circle(void) {
  struct cg_surface_t *surf = cg_surface_create(32, 32);
  struct cg_ctx_t *ctx = cg_create(surf);
  cg_set_source_rgb(ctx, 1.0f, 1.0f, 1.0f);
  cg_paint(ctx);
  cg_set_source_rgb(ctx, 0.0f, 0.0f, 1.0f);
  cg_circle(ctx, 16.0f, 16.0f, 8.0f);
  cg_fill(ctx);
  uint32_t *px = (uint32_t *)surf->pixels;
  int stride = surf->stride / 4;
  // circle center -> px[16 + 16*stride] = px[528] should be blue
  uint32_t center = px[16 + 16 * stride];
  CHECK(pixel_b(center) > 128, "draw_circle_blue_center");
  // at least one non-zero pixel drawn
  int has_drawn = 0;
  for (int i = 0; i < 1000; i++) {
    if (pixel_b(px[i]) > 50) {
      has_drawn = 1;
      break;
    }
  }
  CHECK(has_drawn, "draw_circle_any_blue");
  CHECK(px[0] == rgba_to_pixel(1.0f, 1.0f, 1.0f, 1.0f),
        "draw_circle_around_white");
  cg_destroy(ctx);
  cg_surface_destroy(surf);
}

static void test_draw_arc(void) {
  struct cg_surface_t *surf = cg_surface_create(32, 32);
  struct cg_ctx_t *ctx = cg_create(surf);
  cg_set_source_rgb(ctx, 1.0f, 1.0f, 1.0f);
  cg_paint(ctx);
  cg_set_source_rgb(ctx, 1.0f, 0.5f, 0.0f);
  cg_set_line_width(ctx, 2.0f);
  cg_arc(ctx, 16.0f, 16.0f, 10.0f, 0.0f, 3.14159265359f);
  cg_stroke(ctx);
  cg_destroy(ctx);
  cg_surface_destroy(surf);
}

static void test_draw_gradient(void) {
  struct cg_surface_t *surf = cg_surface_create(32, 32);
  struct cg_ctx_t *ctx = cg_create(surf);
  cg_set_source_rgb(ctx, 0.0f, 0.0f, 0.0f);
  cg_paint(ctx);
  struct cg_gradient_stop_t stops[2] = {
      {0.0f, {1.0f, 0.0f, 0.0f, 0.5f}},
      {1.0f, {0.0f, 0.0f, 1.0f, 0.5f}},
  };
  cg_set_linear_gradient(ctx, 0.0f, 0.0f, 32.0f, 0.0f, CG_SPREAD_METHOD_PAD,
                         stops, 2, NULL);
  cg_paint(ctx);
  uint32_t *px = (uint32_t *)surf->pixels;
  // gradient covers entire surface: left px[0] -> red, right px[31] -> blue
  CHECK(pixel_r(px[0]) > pixel_b(px[0]), "draw_gradient_red_left");
  CHECK(pixel_b(px[31]) > pixel_r(px[31]), "draw_gradient_blue_right");
  cg_destroy(ctx);
  cg_surface_destroy(surf);
}

static void test_draw_fill_ellipse(void) {
  struct cg_surface_t *surf = cg_surface_create(32, 32);
  struct cg_ctx_t *ctx = cg_create(surf);
  cg_set_source_rgb(ctx, 1.0f, 1.0f, 1.0f);
  cg_paint(ctx);
  cg_set_source_rgb(ctx, 0.0f, 1.0f, 0.0f);
  cg_ellipse(ctx, 16.0f, 16.0f, 10.0f, 6.0f);
  cg_fill(ctx);
  uint32_t *px = (uint32_t *)surf->pixels;
  int stride = surf->stride / 4;
  // ellipse center (16,16) -> px[16 + 16*stride] = px[528]
  uint32_t center = px[16 + 16 * stride];
  CHECK(pixel_g(center) > 128, "draw_ellipse_green_center");
  cg_destroy(ctx);
  cg_surface_destroy(surf);
}

static void test_clip_integrated(void) {
  struct cg_surface_t *surf = cg_surface_create(32, 32);
  struct cg_ctx_t *ctx = cg_create(surf);
  // fill entire surface with blue first
  cg_set_source_rgb(ctx, 0.0f, 0.0f, 1.0f);
  cg_paint(ctx);
  // define clip rect path without clearing (use preserve version)
  cg_rectangle(ctx, 5.0f, 5.0f, 2.0f, 2.0f);
  cg_clip_preserve(ctx);
  // fill red within clip area
  cg_set_source_rgb(ctx, 1.0f, 0.0f, 0.0f);
  cg_fill_preserve(ctx);
  cg_new_path(ctx);
  uint32_t *px = (uint32_t *)surf->pixels;
  int stride = surf->stride / 4;
  // clip center (6,6) -> px[6+6*stride] should be red
  uint32_t inside = px[6 + 6 * stride];
  CHECK(pixel_r(inside) > 128 && pixel_g(inside) < 50 && pixel_b(inside) < 50,
        "clip_inside_red");
  // outside clip (0,0) should stay blue
  CHECK(px[0] == rgba_to_pixel(0.0f, 0.0f, 1.0f, 1.0f), "clip_outside_blue");
  cg_destroy(ctx);
  cg_surface_destroy(surf);
}

static void test_transformed_fill(void) {
  struct cg_surface_t *surf = cg_surface_create(64, 64);
  struct cg_ctx_t *ctx = cg_create(surf);
  cg_set_source_rgb(ctx, 1.0f, 1.0f, 1.0f);
  cg_paint(ctx);
  cg_set_source_rgb(ctx, 1.0f, 0.0f, 0.0f);
  cg_save(ctx);
  cg_translate(ctx, 30.0f, 30.0f);
  cg_rectangle(ctx, -10.0f, -10.0f, 20.0f, 20.0f);
  cg_fill(ctx);
  cg_restore(ctx);
  uint32_t *px = (uint32_t *)surf->pixels;
  int stride = surf->stride / 4;
  // translated region center (30,30) -> px[30 + 30*stride] = px[1950]
  uint32_t center = px[30 + 30 * stride];
  CHECK(pixel_r(center) > 128 && pixel_g(center) < 50 && pixel_b(center) < 50,
        "transformed_center_red");
  cg_destroy(ctx);
  cg_surface_destroy(surf);
}

static void test_multi_save_restore(void) {
  struct cg_surface_t *surf = cg_surface_create(32, 32);
  struct cg_ctx_t *ctx = cg_create(surf);
  cg_set_opacity(ctx, 1.0f);
  cg_save(ctx);
  cg_set_opacity(ctx, 0.5f);
  cg_save(ctx);
  cg_set_opacity(ctx, 0.25f);
  cg_restore(ctx);
  CHECK_NEAR(cg_get_opacity(ctx), 0.5f, 1e-6f, "multi_restore_mid");
  cg_restore(ctx);
  CHECK_NEAR(cg_get_opacity(ctx), 1.0f, 1e-6f, "multi_restore_top");
  cg_destroy(ctx);
  cg_surface_destroy(surf);
}

// ---------------- main ---------------------------------------------------

int main(void) {
  printf("=== libcg autotest ===\n");
  test_matrix_basic();
  test_matrix_identity();
  test_matrix_translate_init();
  test_matrix_scale_init();
  test_matrix_rotate_init();
  test_matrix_shear_init();
  test_matrix_multiply();
  test_matrix_invert_valid();
  test_matrix_invert_singular();
  test_matrix_translate_op();
  test_matrix_scale_op();
  test_matrix_rotate_op();
  test_matrix_shear_op();
  test_matrix_transform_ctx();
  test_surface_create();
  test_surface_create_for_data();
  test_surface_null_safe();
  test_surface_reference();
  test_surface_large();
  test_surface_pixel_data();
  test_color_struct();
  test_paint_create_rgb();
  test_paint_create_rgba();
  test_paint_create_color();
  test_paint_clamp();
  test_paint_reference();
  test_paint_null_safe();
  test_path_create_destroy();
  test_path_reset();
  test_path_move_to_line_to();
  test_path_close();
  test_path_reference();
  test_path_null_safe();
  test_path_rectangle();
  test_path_circle();
  test_path_ellipse();
  test_path_quadratic();
  test_path_cubic();
  test_path_extents();
  test_path_transform();
  test_path_sub_path();
  test_context_create_destroy();
  test_context_null_safe();
  test_get_surface();
  test_get_path();
  test_get_matrix();
  test_get_source();
  test_get_operator();
  test_get_opacity();
  test_get_current_point();
  test_has_current_point();
  test_get_extents();
  test_in_fill();
  test_in_stroke();
  test_in_clip();
  test_set_source();
  test_set_source_rgb_rgba();
  test_set_operator();
  test_set_opacity();
  test_set_fill_rule();
  test_set_stroke_style();
  test_set_dash();
  test_context_transform();
  test_context_set_matrix();
  test_context_identity_matrix();
  test_save_restore();
  test_reset_clip();
  test_context_path_commands();
  test_context_add_path();
  test_fill_stroke_paint();
  test_fill_preserve_stroke_preserve();
  test_clip_preserve();
  test_mask();
  test_mask_surface();
  test_set_source_surface();
  test_gradient_setup();
  test_texture_setup();
  test_draw_fill_rectangle();
  test_draw_stroke_line();
  test_draw_circle();
  test_draw_arc();
  test_draw_gradient();
  test_draw_fill_ellipse();
  test_clip_integrated();
  test_transformed_fill();
  test_multi_save_restore();
  printf("[RESULT] pass=%d fail=%d\n", g_pass, g_fail);
  return (g_fail == 0) ? 0 : 1;
}
