# Backup 64-bit integer soft-ops (no soft-float), kept only as an alternative
# to compiler-rt (tcmips_clang_rt). Not wired into tcmips_core or any default
# link path — used only on demand.
add_library(tcmips_gcc_rt arith64.c)