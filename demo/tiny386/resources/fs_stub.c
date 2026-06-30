//
// Created by zhangjiantao on 2026/6/4.
//

#include "stdlib.h"
#include "string.h"

#include "resources.h"

typedef struct {
  uint8_t *buffer;
  size_t size;
  size_t pos;
  int is_writable;
} ROM_FILE;

typedef struct {
  const char *filename;
  uint8_t *buffer;
  size_t size;
  int is_writable;
} ROM_FileMap;

static const ROM_FileMap g_RomFileMap[] = {
    {"bios.bin", g_res_BIOS_BIN, g_res_BIOS_BIN_len, 0},
    {"vgabios.bin", g_res_VGABIOS_BIN, g_res_VGABIOS_BIN_len, 0},

#ifdef TCM_TINY386_WIN95
    {"micro95.img", g_res_MICRO95_IMG, g_res_MICRO95_IMG_len, 1},
#endif
};

#define ROM_FILE_COUNT (sizeof(g_RomFileMap) / sizeof(ROM_FileMap))
#define MAX_OPEN_FILES 32
static ROM_FILE g_VirtualFiles[MAX_OPEN_FILES];

void *my_rom_fopen(const char *path, const char *mode) {
  if (!path)
    return NULL;

  const char *filename = strrchr(path, '/');
  if (filename) {
    filename++;
  } else {
    filename = path;
  }

  for (size_t i = 0; i < ROM_FILE_COUNT; i++) {
    if (strcasecmp(filename, g_RomFileMap[i].filename) == 0) {
      for (int j = 0; j < MAX_OPEN_FILES; j++) {
        if (g_VirtualFiles[j].buffer == NULL) {
          g_VirtualFiles[j].buffer = g_RomFileMap[i].buffer;
          g_VirtualFiles[j].size = g_RomFileMap[i].size;
          g_VirtualFiles[j].pos = 0;
          return &g_VirtualFiles[j];
        }
      }
    }
  }
  return NULL;
}

size_t my_rom_fread(void *ptr, size_t size, size_t count, void *stream) {
  ROM_FILE *f = (ROM_FILE *)stream;
  if (!f || !f->buffer || size == 0 || count == 0 || !ptr)
    return 0;

  size_t bytes_to_read = size * count;
  if (f->pos + bytes_to_read > f->size) {
    bytes_to_read = f->size - f->pos;
  }

  if (bytes_to_read > 0) {
    memcpy(ptr, f->buffer + f->pos, bytes_to_read);
    f->pos += bytes_to_read;
  }

  return bytes_to_read / size;
}

size_t my_rom_fwrite(const void *ptr, size_t size, size_t count, void *stream) {
  ROM_FILE *f = (ROM_FILE *)stream;
  if (!f || !f->buffer || size == 0 || count == 0 || !ptr)
    return 0;
  if (!f->is_writable)
    return 0;

  size_t bytes_to_write = size * count;
  if (f->pos + bytes_to_write > f->size) {
    bytes_to_write = f->size - f->pos;
  }

  if (bytes_to_write > 0) {
    memcpy(f->buffer + f->pos, ptr, bytes_to_write);
    f->pos += bytes_to_write;
  }
  return bytes_to_write / size;
}

int my_rom_fseek(void *stream, long offset, int whence) {
  ROM_FILE *f = (ROM_FILE *)stream;
  if (!f || !f->buffer)
    return -1;

  long current_pos = (long)f->pos;
  long file_size = (long)f->size;
  long new_pos = 0;

  if (whence == 0) { // SEEK_SET
    new_pos = offset;
  } else if (whence == 1) { // SEEK_CUR
    new_pos = current_pos + offset;
  } else if (whence == 2) { // SEEK_END
    new_pos = file_size + offset;
  } else {
    return -1;
  }

  if (new_pos < 0 || new_pos > file_size)
    return -1;

  f->pos = (size_t)new_pos;
  return 0;
}

long my_rom_ftell(void *stream) {
  ROM_FILE *f = (ROM_FILE *)stream;
  return f ? (long)f->pos : -1;
}

int my_rom_fseeko(void *stream, int64_t offset, int whence) {
  ROM_FILE *f = (ROM_FILE *)stream;
  if (!f || !f->buffer)
    return -1;

  long regular_offset = (long)offset;
  return my_rom_fseek(stream, regular_offset, whence);
}

int64_t my_rom_ftello(void *stream) { return (int64_t)my_rom_ftell(stream); }

int my_rom_fclose(void *stream) {
  ROM_FILE *f = (ROM_FILE *)stream;
  if (f) {
    f->buffer = NULL;
    f->size = 0;
    f->pos = 0;
  }
  return 0;
}

int my_rom_feof(void *stream) {
  ROM_FILE *f = (ROM_FILE *)stream;
  if (!f || !f->buffer) {
    return 1;
  }
  return (f->pos >= f->size) ? 1 : 0;
}
