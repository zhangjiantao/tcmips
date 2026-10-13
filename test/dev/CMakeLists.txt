# Device console font-load integration test.
#
# Exercises the dev(console) → memfs → zlib chain that the Unicode console uses
# to load and decompress its bundled font asset (/tcmips/fonts/unifont.bin).
# The font payload is carried by tcmips_dev itself (STATIC_LIBRARY_OPTIONS
# -tcm-memfs=assets/), so linking tcmips_core is sufficient — no per-test
# -tcm-memfs is needed here.

add_executable(test_dev_font_autotest test_dev_font_autotest.c)
target_link_libraries(test_dev_font_autotest tcmips_core)
