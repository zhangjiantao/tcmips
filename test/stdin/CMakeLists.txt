# Semihost stdin integration test.
#
# The guest reads its standard input through tcm_syscall_semihost_stdin.  The
# host emulator supplies the data via a `-stdin=<file>` argument at run time
# (NOT via --tcm-memfs); we generate that file here so a driver can reference
# a stable path.
#
# IMPORTANT for the bare regression loop (running every autotest/*.tcm with no
# extra args): without `-stdin=` the emulator reports "not wired", so this test
# prints a greppable "[SKIP] stdin-wired skipped" line and exits 0.  That is an
# expected environment skip, NOT a failure.  To actually exercise the input
# path, run:
#   TCMIPSEmulator test_stdin_autotest.tcm -hold=0 -mode=fast \
#       -stdin=<build>/test/stdin/stdin.txt

add_executable(test_stdin_autotest test_stdin_autotest.c)
target_link_libraries(test_stdin_autotest tcmips_core)

# Generate the known-content stdin payload used by the runtime driver.  The
# guest expects exactly "hello stdin\n42\n" (see test_stdin_autotest.c).
set(TCM_STDIN_DATA "${CMAKE_CURRENT_BINARY_DIR}/stdin.txt")
file(WRITE "${TCM_STDIN_DATA}" "hello stdin\n42\n")
