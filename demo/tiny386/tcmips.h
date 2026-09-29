#ifndef FS_HOOK_H
#define FS_HOOK_H

#define _POSIX_MONOTONIC_CLOCK
#include <time.h>

#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

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