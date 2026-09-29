# TCMIPS hardware configuration constants — single source of truth.
#
# Shared across guest cross-build, emulator host build, and downstream projects.
# Include with: include(cmake/config.cmake)
# All values use CACHE STRING + FORCE for global override consistency.
#
# Layout diagram (top = low addr, bottom = high addr):
#  | Region       | Size          | Direction    | Description                    |
#  |--------------|---------------|--------------|--------------------------------|
#  | Low Mem      | -             | -            | Boot stub + header + app image |
#  | Heap         | (remaining)   | Low->High    | After app image, grows up      |
#  | Guard Page   | 4KB           | Static       | Between Heap and Stack         |
#  | Stack        | 256KB         | High->Low    | From StackTop, grows down      |
#  | ASCII VRAM   | 1MB           | Static       | At StackTop                    |
#  | Pixel VRAM   | 3MB           | Static       | Top of memory                  |
# See docs/TCMIPS-MemoryLayout.md for full details.

# All arithmetic uses OUTPUT_FORMAT HEXADECIMAL for C-style hex output:
# TCM_VRAM_PIXEL_ADDR = TCM_MEMORY_SIZE − TCM_VRAM_PIXEL_SIZE
# TCM_VRAM_ASCII_ADDR = TCM_VRAM_PIXEL_ADDR − TCM_VRAM_ASCII_SIZE
# TCM_STACK_TOP_ADDR  = TCM_VRAM_ASCII_ADDR
# TCM_STACK_LIMIT_ADDR = TCM_STACK_TOP_ADDR − TCM_STACK_SIZE
# TCM_GUARD_PAGE_ADDR = TCM_STACK_LIMIT_ADDR − TCM_GUARD_PAGE_SIZE
# TCM_HEAP_LIMIT_ADDR = TCM_GUARD_PAGE_ADDR

# ── Configurable: total memory size ────────────────────────────────────

set(TCM_MEMORY_SIZE 0x04000000 CACHE STRING "Total size of TCM main memory" FORCE)

# ── Fixed region sizes (independent of TCM_MEMORY_SIZE) ────────────────

set(TCM_VRAM_ASCII_SIZE 0x100000 CACHE STRING "VRAM size for ASCII character display mode" FORCE)
set(TCM_VRAM_PIXEL_SIZE 0x300000 CACHE STRING "VRAM size for raw pixel framebuffer mode" FORCE)
set(TCM_STACK_SIZE 0x40000 CACHE STRING "Stack size for TCM" FORCE)
set(TCM_GUARD_PAGE_SIZE 0x1000 CACHE STRING "Guard page size for TCM" FORCE)

# ── Dynamically computed addresses (derived from TCM_MEMORY_SIZE) ──────

math(EXPR TCM_VRAM_PIXEL_ADDR "${TCM_MEMORY_SIZE} - ${TCM_VRAM_PIXEL_SIZE}" OUTPUT_FORMAT HEXADECIMAL)
math(EXPR TCM_VRAM_ASCII_ADDR "${TCM_VRAM_PIXEL_ADDR} - ${TCM_VRAM_ASCII_SIZE}" OUTPUT_FORMAT HEXADECIMAL)

set(TCM_STACK_TOP_ADDR "${TCM_VRAM_ASCII_ADDR}" CACHE STRING "Stack top pointer (Initial SP)" FORCE)

math(EXPR TCM_STACK_LIMIT_ADDR "${TCM_STACK_TOP_ADDR} - ${TCM_STACK_SIZE}" OUTPUT_FORMAT HEXADECIMAL)

math(EXPR TCM_GUARD_PAGE_ADDR "${TCM_STACK_LIMIT_ADDR} - ${TCM_GUARD_PAGE_SIZE}" OUTPUT_FORMAT HEXADECIMAL)

set(TCM_HEAP_LIMIT_ADDR "${TCM_GUARD_PAGE_ADDR}" CACHE STRING "Maximum limit address for Heap" FORCE)
