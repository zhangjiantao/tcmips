//
// Copyright(C) 1993-1996 Id Software, Inc.
// Copyright(C) 2005-2014 Simon Howard
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// DESCRIPTION:
//	WAD I/O functions.
//

#include <stdio.h>

#include "config.h"

#include "doomtype.h"
#include "m_argv.h"

#include "w_file.h"

#include "string.h"

extern wad_file_class_t stdc_wad_file;

/*
#ifdef _WIN32
extern wad_file_class_t win32_wad_file;
#endif
*/

#ifdef HAVE_MMAP
extern wad_file_class_t posix_wad_file;
#endif 

static wad_file_class_t *wad_file_classes[] = 
{
/*
#ifdef _WIN32
    &win32_wad_file,
#endif
*/
#ifdef HAVE_MMAP
    &posix_wad_file,
#endif
    &stdc_wad_file,
};

extern const unsigned char doom1_wad[];
extern const unsigned int doom1_wad_len;

static wad_file_t g_virtual_wad;

wad_file_t *W_OpenFile(char *path)
{
  g_virtual_wad.file_class->OpenFile = &W_OpenFile;
  g_virtual_wad.length   = doom1_wad_len;
  g_virtual_wad.mapped = (byte *)doom1_wad;
  return &g_virtual_wad;
}

void W_CloseFile(wad_file_t *wad)
{
    //wad->file_class->CloseFile(wad);
}

size_t W_Read(wad_file_t *wad, unsigned int offset,
              void *buffer, size_t buffer_len)
{
  if (!wad || !wad->mapped) return 0;

  size_t bytes_read = buffer_len;
  if (offset + buffer_len > wad->length) {
    bytes_read = wad->length - offset;

  }
  memcpy(buffer, wad->mapped + offset, bytes_read);
  return bytes_read;
}

