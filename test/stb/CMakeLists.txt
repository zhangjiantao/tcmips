# CMakeLists.txt for stb AUTOTESTs.
#
# Uses fixed committed test assets (assets/), no build-time generation:
#   assets/test_image.png      — 4x4 RGB PNG for stb_image / stb_image_resize2
#   assets/geforce-bold.ttf    — TTF font for stb_truetype
#
# Six test executables:
#   test_stb_image_autotest       — stbi_load / stbi_load_from_memory / stbi_image_free
#   test_stb_image_resize2_autotest — stbir_resize_uint8 scaling
#   test_stb_truetype_autotest      — stbtt_InitFont / metrics / glyph rendering
#   test_stb_ds_autotest            — dynamic arrays & hash maps (stb_ds.h macros)
#   test_stb_sprintf_autotest       — stbsp_snprintf / stbsp_vsnprintf
#   test_stb_rect_pack_autotest     — stbrp_pack_rects (rectangle packing)
#

# --- stb_image: decode PNG from memory, verify dimensions & pixels ------------
add_executable(test_stb_image_autotest test_stb_image_autotest.c)
target_link_libraries(test_stb_image_autotest tcmips_stb tcmips_core)
target_link_options(test_stb_image_autotest PRIVATE
    -tcm-memfs=${CMAKE_CURRENT_SOURCE_DIR}/assets)

# --- stb_image_resize2: scale source image to multiple target sizes -----------
add_executable(test_stb_image_resize2_autotest test_stb_image_resize2_autotest.c)
target_link_libraries(test_stb_image_resize2_autotest tcmips_stb tcmips_core)
target_link_options(test_stb_image_resize2_autotest PRIVATE
    -tcm-memfs=${CMAKE_CURRENT_SOURCE_DIR}/assets)

# --- stb_truetype: load TTF, query metrics, render glyph bitmap ---------------
add_executable(test_stb_truetype_autotest test_stb_truetype_autotest.c)
target_link_libraries(test_stb_truetype_autotest tcmips_stb tcmips_core)
target_link_options(test_stb_truetype_autotest PRIVATE
    -tcm-memfs=${CMAKE_CURRENT_SOURCE_DIR}/assets)

# --- stb_ds: dynamic arrays and hash tables -----------------------------------
add_executable(test_stb_ds_autotest test_stb_ds_autotest.c)
target_link_libraries(test_stb_ds_autotest tcmips_stb tcmips_core)

# --- stb_sprintf: format strings ------------------------------------------------
add_executable(test_stb_sprintf_autotest test_stb_sprintf_autotest.c)
target_link_libraries(test_stb_sprintf_autotest tcmips_stb tcmips_core)

# --- stb_rect_pack: 2D rectangle bin packing ----------------------------------
add_executable(test_stb_rect_pack_autotest test_stb_rect_pack_autotest.c)
target_link_libraries(test_stb_rect_pack_autotest tcmips_stb tcmips_core)


# Note: AUTOTEST_TARGETS is aggregated in test/CMakeLists.txt (parent scope).
# test/stb targets are added there after add_subdirectory(stb).