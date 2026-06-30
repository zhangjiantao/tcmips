#ifndef __ID_HEADS_H__
#define __ID_HEADS_H__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <time.h>
#include <inttypes.h>
#ifndef __arm__
#include <glob.h>
#endif
#include <ctype.h>
#ifndef EMBEDDED
#include <math.h>
#endif

#ifdef __cplusplus
typedef bool boolean;
#else
#define true 1
#define false 0
typedef unsigned char boolean;
#endif

#define PACKED __attribute__((packed))

#include "version.h"

/* ------------------------------------------------------------------------ */

#ifndef SPEAR

#ifndef UPLOAD

#ifdef WOLF92
#  include "gfxv_wl6_92.h"
#else
#  include "gfxv_wl6.h"
#endif
  
#else
#include "gfxv_wl1.h"
#endif

#include "audiowl6.h"

#else /* SPEAR */

#ifndef SPEARDEMO
#include "gfxv_sod.h"
#else /* SPEARDEMO */
#include "gfxv_sdm.h"
#endif /* SPEARDEMO */

#include "audiosod.h"

#endif /* SPEAR */

/* ---------------- */

typedef uint8_t		byte;
typedef uint16_t	word;
typedef uint32_t	longword;
typedef uint32_t	dword;

typedef int32_t fixed;

typedef void * memptr;

#define myint int
#define myshort short
typedef unsigned short umyshort;

#include "misc.h"

#include "vi_comm.h"
#include "sd_comm.h"

#include "id_ca.h"
#include "id_vh.h"
#include "id_us.h"

extern const byte gamepal[];

myint MS_CheckParm(const char *string);
#if defined(LUMINARY) && !defined(DEBUG)
#define Quit(msg) do {} while (0)
#else
void Quit(const char *error);
#endif

#define TickBase	70	/* 70Hz per tick */

#undef PI
#define PI		3.1415926535897932384626433832795028841971693993751058209749445920f

#define	MAXTICS		10
#define DEMOTICS	4

extern myint tics;

#define mapwidth	64
#define mapheight	64

#endif
