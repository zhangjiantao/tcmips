#ifndef __MISC_H__
#define __MISC_H__

extern myint _argc;
extern char **_argv;

void SavePCX256ToFile(const unsigned char *buf, myint width, myint height, const unsigned char *pal, const char *name);
void SavePCXRGBToFile(const unsigned char *buf, myint width, myint height, const char *name);

void set_TimeCount(unsigned long t);
unsigned long get_TimeCount(void);
unsigned long sleepuntil(unsigned long t);

long filelength(myint handle);

#ifndef stricmp
#define stricmp strcasecmp
#endif

#ifndef strnicmp
#define strnicmp strncasecmp
#endif

char *itoa(myint value, char *string, myint radix);
char *ltoa(long value, char *string, myint radix);
char *ultoa(unsigned long value, char *string, myint radix);

uint16_t SwapInt16L(uint16_t i);
uint32_t SwapInt32L(uint32_t i);

extern myint OpenWrite(const char *fn);
extern myint OpenWriteAppend(const char *fn);
extern void CloseWrite(myint fp);

extern myint WriteSeek(myint fp, myint offset, myint whence);
extern myint WritePos(myint fp);

extern myint WriteInt8(myint fp, int8_t d);
extern myint WriteInt16(myint fp, int16_t d);
extern myint WriteInt32(myint fp, int32_t d);
extern myint WriteBytes(myint fp, const byte *d, myint len);

extern myint OpenRead(const char *fn);
extern void CloseRead(myint fp);

extern myint ReadSeek(myint fp, myint offset, myint whence);
extern myint ReadLength(myint fp);

extern int8_t ReadInt8(myint fp);
extern int16_t ReadInt16(myint fp);
extern int32_t ReadInt32(myint fp);
extern myint ReadBytes(myint fp, byte *d, myint len);

extern myshort atan2fix(fixed x, fixed y);

static __inline__ uint16_t SwapInt16(uint16_t i)
{
	return ((uint16_t)i >> 8) | ((uint16_t)i << 8);
}

static __inline__ uint32_t SwapInt32(uint32_t i)
{
	return	((uint32_t)(i & 0xFF000000) >> 24) |
		((uint32_t)(i & 0x00FF0000) >>  8) |
		((uint32_t)(i & 0x0000FF00) <<  8) |
		((uint32_t)(i & 0x000000FF) << 24);
}

#ifdef LUMINARY
extern const int oled_height;
extern const uint8_t oled_init_strings[];
#endif

#endif
