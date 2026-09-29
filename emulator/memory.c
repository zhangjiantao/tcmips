/*
 * Shared Guest Memory — Definition
 *
 * See memory.h for the rationale behind keeping the address space separate
 * from any single interpreter.
 *
 * Also owns the guest section-layout bookkeeping: the 4 KB TCM image header
 * declares where the linker placed .text/.data/.bss, and the RSP stub needs
 * those addresses to answer GDB's 'qOffsets' query.  The parse and the
 * public accessor (memory_get_image_info) live here; memory.h forwards the
 * accessor declaration to consumers.
 */

#include "tcm_format.h"

#include "memory.h"
#include "platform.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

uint8_t g_emu_memory[TCM_MEMORY_SIZE] __attribute__((aligned(16))) BSS_SECTION;

/* ── Section layout parsed from the TCM image header ─────────────────── */

static uint32_t g_image_size;
static uint32_t g_text_base, g_data_base, g_bss_base;
static bool g_section_info_valid;

bool memory_get_image_info(uint32_t *image_size, uint32_t *text_base,
                           uint32_t *data_base, uint32_t *bss_base) {
  if (image_size)
    *image_size = g_image_size;

  if (!g_section_info_valid)
    return false;

  if (text_base)
    *text_base = g_text_base;
  if (data_base)
    *data_base = g_data_base;
  if (bss_base)
    *bss_base = g_bss_base;
  return true;
}

void memory_init(const uint8_t *image, uint32_t size) {
  g_section_info_valid = false;
  g_text_base = g_data_base = g_bss_base = 0;
  g_image_size = size;

  memset(g_emu_memory, 0, TCM_MEMORY_SIZE);

  /* Recover the section layout declared by the linker in the 4 KB header
   * (used by the RSP stub to answer GDB's 'qOffsets').  Only trust the
   * fields after the magic check, and only accept a sane layout: the 4 KB
   * header zone must precede .text, and text/data/bss must be ordered and
   * reside inside guest RAM.  Anything dubious disables the export instead
   * of feeding garbage to the debugger. */
  if (image != NULL && size >= sizeof(TCMImageHeader)) {
    const TCMImageHeader *img = (const TCMImageHeader *)image;
    if (img->w_magic == TCM_MAGIC) {
      uint32_t text = img->w_textbase;
      uint32_t data = img->w_database;
      uint32_t bss = img->w_bssbase;
      if (text >= TCM_HEADER_SIZE && text <= data && data <= bss &&
          bss < TCM_MEMORY_SIZE) {
        g_text_base = text;
        g_data_base = data;
        g_bss_base = bss;
        g_section_info_valid = true;
      } else {
        fprintf(stderr,
                "memory: refusing TCM header section bases "
                "(text=0x%08x data=0x%08x bss=0x%08x)\n",
                text, data, bss);
      }
    } else {
      fprintf(stderr,
              "memory: image header magic 0x%08x != 0x%08x (no section "
              "bases available)\n",
              img->w_magic, (unsigned)TCM_MAGIC);
    }
  }

  memcpy(g_emu_memory, image, size);
}
