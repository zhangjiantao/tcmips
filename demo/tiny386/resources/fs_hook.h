#ifndef FS_HOOK_H
#define FS_HOOK_H

#define _POSIX_MONOTONIC_CLOCK
#include <time.h>

#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

void *my_rom_fopen(const char *path, const char *mode);
size_t my_rom_fread(void *ptr, size_t size, size_t count, void *stream);
size_t my_rom_fwrite(const void *ptr, size_t size, size_t count, void *stream);
int my_rom_fseek(void *stream, long offset, int whence);
long my_rom_ftell(void *stream);
int my_rom_fclose(void *stream);
int my_rom_feof(void *stream);
int my_rom_fseeko(void *stream, int64_t offset, int whence);
int64_t my_rom_ftello(void *stream);

#ifdef __cplusplus
}
#endif

#undef fopen
#undef fread
#undef fwrite
#undef fseek
#undef ftell
#undef fclose
#undef feof
#undef rewind
#undef fseeko
#undef fseeko64
#undef ftello
#undef ftello64

#define fopen my_rom_fopen
#define fread my_rom_fread
#define fwrite my_rom_fwrite
#define fseek my_rom_fseek
#define ftell my_rom_ftell
#define fclose my_rom_fclose
#define feof my_rom_feof
#define fseeko my_rom_fseeko
#define fseeko64 my_rom_fseeko
#define ftello my_rom_ftello
#define ftello64 my_rom_ftello

#define rewind(stream) my_rom_fseek((stream), 0, 0)

#ifdef NDEBUG
static inline void baremetal_assert_failed(const char *expr, const char *file,
                                           int line) {
  printf("\n[ASSERT FAILED]\n");
  printf("  Expression: %s\n", expr);
  printf("  Location  : %s:%d\n", file, line);
  printf("  Status    : System Halted.\n\n");
  exit(-1);
}

#undef assert
#define assert(expr)                                                           \
  do {                                                                         \
    if (!(expr)) {                                                             \
      baremetal_assert_failed(#expr, __FILE__, __LINE__);                      \
    }                                                                          \
  } while (0)
#endif

#include <ne2000.h>
#define NE2000State void
#define ne2000_ioport_write(...) (void)0
#define ne2000_ioport_read(...) 0xff
#define ne2000_reset_ioport_write(...) (void)0
#define ne2000_reset_ioport_read(...) 0xff
#define ne2000_asic_ioport_write(...) (void)0
#define ne2000_asic_ioport_read(...) 0xff
#define ne2000_step(...) (void)0
#define isa_ne2000_init(...) NULL

#include <adlib.h>
#define AdlibState void
#define adlib_write(...) (void)0
#define adlib_read(...) 0xff
#define adlib_callback(...) (void)0
#define adlib_free(...) (void)0
#define adlib_new(...) NULL

#include <sb16.h>
#define SB16State void
#define sb16_dsp_read(...) 0xff
#define sb16_dsp_write(...) (void)0
#define sb16_mixer_read(...) 0xff
#define sb16_mixer_write_indexb(...) (void)0
#define sb16_mixer_write_datab(...) (void)0
#define sb16_audio_callback(...) (void)0
#define sb16_new(...) NULL

#include <pcspk.h>
#define PCSpkState void
#define pcspk_ioport_read(...) 0xff
#define pcspk_ioport_write(...) (void)0
#define pcspk_callback(...) (void)0
#define pcspk_get_active_out(...) 0
#define pcspk_init(...) NULL
#endif // FS_HOOK_H