#include "wl_def.h"
#include <unistd.h>
#include <stdint.h>
#include <fcntl.h>

#ifdef __thumb2__
#define SEMI_SWI "bkpt 0xab"
#else
#define SEMI_SWI "swi 0x123456"
#endif

void __aeabi_unwind_cpp_pr0()
{
}

int atoi(const char *p)
{
    int negative = 0;
    int i;
    if (*p == '-') {
	negative = 1;
	p++;
    }
    i = 0;
    while (*p) {
	i = (i * 10) + (*p - '0');
	p++;
    }
    return i;
}

void abort()
{
  while(1);
}

void *memset(void *s, int c, size_t n)
{
    char *p = s;
    while (n--)
	*(p++) = c;
    return s;
}

int memcmp(const void *s1, const void *s2, size_t n)
{
    const byte *p1 = s1;
    const byte *p2 = s2;
    while (n && *p1 == *p2) {
	p1++;
	p2++;
	n--;
    }
    if (n)
	return (int)*p2 - (int)*p1;
    return 0;
}

int strncmp(const char *s1, const char *s2, size_t n)
{
    int i;

    i = 0;
    while (i < n && *s1 && *s2 && *s1 != *s2) {
	s1++;
	s2++;
    }
    return (int)*s2 - (int)*s1;
}

/* Only used for file timestamps.  */
time_t time(time_t *p)
{
  return 0;
}

char *strcpy(char *dest, const char *src)
{
    char *p = dest;
    while (*src) {
	*(p++) = *(src++);
    }
    *p = 0;
    return dest;
}

size_t strlen(const char *s)
{
    const char *p = s;
    while (*p)
	p++;
    return p - s;
}

char *strcat(char *dest, const char *src)
{
    char *p = dest + strlen(dest);
    strcpy(p, src);
    return dest;
}

void *memcpy(void *dest, const void *src, size_t n)
{
    char *a = dest;
    const char *b = src;
    while (n--)
	*(a++) = *(b++);
    return dest;
}

#ifndef ENABLE_PRECOMPILE
static uint32_t angel(int reason, uint32_t *args)
{
    register uint32_t r0 asm ("r0");
    register uint32_t r1 asm ("r1");

    r0 = reason;
    r1 = (uint32_t)args;
    asm volatile (SEMI_SWI
		  : "=r" (r0), "=r"(r1)
		  : "0"(r0), "1"(r1)
		  : "r2", "r3", "lr", "ip", "cc", "memory");
    return r0;
}

int open(const char *filename, int flags, ...)
{
    uint32_t block[3];
    if (flags & O_CREAT)
	Quit("Can't creat\n");
#ifdef O_BINARY
    flags &= ~O_BINARY;
#endif
    if (flags != O_RDONLY)
	Quit("Can only open readonly\n");
    block[0] = (uint32_t)filename;
    block[1] = 1;
    block[2] = strlen(filename);
    return angel(1, block);
}

ssize_t read(int fd, void *buf, size_t count)
{
    uint32_t block[3];
    int rc;

    block[0] = fd;
    block[1] = (uint32_t)buf;
    block[2] = count;
    rc = angel(6, block);
    if (rc)
	Quit("Read Failed\n");
    return count - rc;
}

ssize_t write(int fd, const void *buf, size_t count)
{
    uint32_t block[3];
    int rc;

    block[0] = fd;
    block[1] = (uint32_t)buf;
    block[2] = count;
    rc = angel(5, block);
    if (rc)
	Quit("Write Failed\n");
    return count - rc;
}

int close(int fd)
{
    uint32_t arg = fd;
    angel(2, &arg);
    return 0;
}

off_t lseek(int fd, off_t ptr, int whence)
{
    uint32_t block[2];
    int rc;
    if (whence != SEEK_SET)
	Quit("Bad Seek whence\n");
    block[0] = fd;
    block[1] = ptr;
    rc = angel(10, block);
    if (rc)
	return -1;
    return ptr;
}
#endif
