#include <SDL.h>

#include <ctype.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

// Memory functions are thin wrappers over picolibc malloc/free.
// When TCM_SDL_DEBUG is defined, SDL_malloc/free/calloc/realloc count
// live allocations so tests can detect leaks.
#ifdef TCM_SDL_DEBUG
static int s_sdl2_alloc_count = 0;

void *SDL_malloc(size_t size) {
  void *p = malloc(size);
  if (p)
    s_sdl2_alloc_count++;
  return p;
}

void *SDL_calloc(size_t nmemb, size_t size) {
  void *p = calloc(nmemb, size);
  if (p)
    s_sdl2_alloc_count++;
  return p;
}

void *SDL_realloc(void *mem, size_t size) {
  if (!mem && size > 0)
    s_sdl2_alloc_count++; /* behaves like malloc */
  else if (mem && size == 0)
    s_sdl2_alloc_count--; /* behaves like free */
  /* realloc to non-zero on non-NULL: net count unchanged */
  return realloc(mem, size);
}

void SDL_free(void *mem) {
  if (mem)
    s_sdl2_alloc_count--;
  free(mem);
}

size_t tcmips_sdl2_debug_alloc_count(void) {
  /* Report the raw counter without clamping.  A negative counter (double-free
     / over-free) wraps to a large unsigned value and thus fails any
     `live == 0` leak assertion, instead of being silently masked to zero. */
  return (size_t)s_sdl2_alloc_count;
}

void tcmips_sdl2_debug_alloc_count_reset(void) { s_sdl2_alloc_count = 0; }
#else
void *SDL_malloc(size_t size) { return malloc(size); }

void *SDL_calloc(size_t nmemb, size_t size) { return calloc(nmemb, size); }

void *SDL_realloc(void *mem, size_t size) { return realloc(mem, size); }

void SDL_free(void *mem) { free(mem); }
#endif

#ifdef TCM_SDL_DEBUG
void *SDL_aligned_alloc(size_t alignment, size_t size) {
  void *p = aligned_alloc(alignment, size);
  if (p)
    s_sdl2_alloc_count++;
  return p;
}

void SDL_aligned_free(void *mem) {
  if (mem)
    s_sdl2_alloc_count--;
  free(mem);
}
#else
void *SDL_aligned_alloc(size_t alignment, size_t size) {
  return aligned_alloc(alignment, size);
}

void SDL_aligned_free(void *mem) { free(mem); }
#endif

void SDL_GetMemoryFunctions(SDL_malloc_func *malloc_func,
                            SDL_calloc_func *calloc_func,
                            SDL_realloc_func *realloc_func,
                            SDL_free_func *free_func) {
  if (malloc_func)
    *malloc_func = SDL_malloc;
  if (calloc_func)
    *calloc_func = SDL_calloc;
  if (realloc_func)
    *realloc_func = SDL_realloc;
  if (free_func)
    *free_func = SDL_free;
}

void SDL_GetOriginalMemoryFunctions(SDL_malloc_func *malloc_func,
                                    SDL_calloc_func *calloc_func,
                                    SDL_realloc_func *realloc_func,
                                    SDL_free_func *free_func) {
  SDL_GetMemoryFunctions(malloc_func, calloc_func, realloc_func, free_func);
}

int SDL_SetMemoryFunctions(SDL_malloc_func malloc_func,
                           SDL_calloc_func calloc_func,
                           SDL_realloc_func realloc_func,
                           SDL_free_func free_func) {
  (void)malloc_func;
  (void)calloc_func;
  (void)realloc_func;
  (void)free_func;
  return 0;
}

char *SDL_getenv(const char *name) { return getenv(name); }
int SDL_setenv(const char *name, const char *value, int overwrite) {
  return setenv(name, value, overwrite);
}

int SDL_abs(int x) { return abs(x); }
int SDL_isalpha(int x) { return isalpha(x); }
int SDL_isalnum(int x) { return isalnum(x); }
int SDL_isblank(int x) { return isblank(x); }
int SDL_iscntrl(int x) { return iscntrl(x); }
int SDL_isdigit(int x) { return isdigit(x); }
int SDL_isxdigit(int x) { return isxdigit(x); }
int SDL_ispunct(int x) { return ispunct(x); }
int SDL_isspace(int x) { return isspace(x); }
int SDL_isupper(int x) { return isupper(x); }
int SDL_islower(int x) { return islower(x); }
int SDL_isprint(int x) { return isprint(x); }
int SDL_isgraph(int x) { return isgraph(x); }
int SDL_toupper(int x) { return toupper(x); }
int SDL_tolower(int x) { return tolower(x); }

void *SDL_memset(void *dst, int c, size_t len) { return memset(dst, c, len); }
void *SDL_memcpy(void *dst, const void *src, size_t len) {
  return memcpy(dst, src, len);
}
void *SDL_memmove(void *dst, const void *src, size_t len) {
  return memmove(dst, src, len);
}
int SDL_memcmp(const void *s1, const void *s2, size_t len) {
  return memcmp(s1, s2, len);
}

size_t SDL_strlen(const char *s) { return strlen(s); }
size_t SDL_strlcpy(char *dst, const char *src, size_t maxlen) {
  if (!dst || !src || maxlen == 0)
    return 0;
  size_t srclen = SDL_strlen(src);
  if (srclen < maxlen) {
    memcpy(dst, src, srclen + 1);
  } else {
    memcpy(dst, src, maxlen - 1);
    dst[maxlen - 1] = '\0';
  }
  return srclen;
}
size_t SDL_utf8strlcpy(char *dst, const char *src, size_t dst_bytes) {
  return SDL_strlcpy(dst, src, dst_bytes);
}
size_t SDL_strlcat(char *dst, const char *src, size_t maxlen) {
  size_t dstlen = SDL_strlen(dst);
  if (dstlen >= maxlen)
    return dstlen;
  return dstlen + SDL_strlcpy(dst + dstlen, src, maxlen - dstlen);
}
char *SDL_strdup(const char *s) { return s ? strdup(s) : NULL; }
char *SDL_strrev(char *str) {
  if (!str)
    return NULL;
  size_t len = SDL_strlen(str);
  for (size_t i = 0; i < len / 2; ++i) {
    char t = str[i];
    str[i] = str[len - 1 - i];
    str[len - 1 - i] = t;
  }
  return str;
}
char *SDL_strupr(char *str) {
  if (!str)
    return NULL;
  for (char *p = str; *p; ++p)
    *p = (char)toupper((unsigned char)*p);
  return str;
}
char *SDL_strlwr(char *str) {
  if (!str)
    return NULL;
  for (char *p = str; *p; ++p)
    *p = (char)tolower((unsigned char)*p);
  return str;
}
char *SDL_strchr(const char *s, int c) { return strchr(s, c); }
char *SDL_strrchr(const char *s, int c) { return (char *)strrchr(s, c); }
char *SDL_strstr(const char *haystack, const char *needle) {
  return (char *)strstr(haystack, needle);
}
char *SDL_strcasestr(const char *haystack, const char *needle) {
  return (char *)strcasestr(haystack, needle);
}
int SDL_strcmp(const char *s1, const char *s2) { return strcmp(s1, s2); }
int SDL_strncmp(const char *s1, const char *s2, size_t maxlen) {
  return strncmp(s1, s2, maxlen);
}
int SDL_strcasecmp(const char *s1, const char *s2) {
  return strcasecmp(s1, s2);
}
int SDL_strncasecmp(const char *s1, const char *s2, size_t maxlen) {
  return strncasecmp(s1, s2, maxlen);
}

size_t SDL_wcslen(const wchar_t *s) {
  const wchar_t *p = s;
  while (p && *p)
    ++p;
  return (size_t)(p - s);
}
size_t SDL_wcslcpy(wchar_t *dst, const wchar_t *src, size_t maxlen) {
  if (!dst || !src || maxlen == 0)
    return src ? SDL_wcslen(src) : 0;
  size_t i = 0;
  for (; i < maxlen - 1 && src[i]; ++i)
    dst[i] = src[i];
  dst[i] = L'\0';
  return SDL_wcslen(src);
}
size_t SDL_wcslcat(wchar_t *dst, const wchar_t *src, size_t maxlen) {
  if (!dst || !src || maxlen == 0)
    return 0;
  size_t dstlen = SDL_wcslen(dst);
  if (dstlen >= maxlen)
    return dstlen + SDL_wcslen(src);
  size_t i = 0;
  for (; dstlen + i < maxlen - 1 && src[i]; ++i)
    dst[dstlen + i] = src[i];
  dst[dstlen + i] = L'\0';
  return dstlen + SDL_wcslen(src);
}
wchar_t *SDL_wcsdup(const wchar_t *s) {
  if (!s)
    return NULL;
  size_t len = SDL_wcslen(s) + 1;
  wchar_t *p = (wchar_t *)SDL_malloc(len * sizeof(wchar_t));
  if (p)
    SDL_memcpy(p, s, len * sizeof(wchar_t));
  return p;
}
wchar_t *SDL_wcsstr(const wchar_t *haystack, const wchar_t *needle) {
  if (!haystack || !needle)
    return NULL;
  if (*needle == L'\0')
    return (wchar_t *)haystack;
  size_t n = SDL_wcslen(needle);
  for (const wchar_t *p = haystack; *p; ++p) {
    if (*p == *needle && SDL_wcsncmp(p, needle, n) == 0) {
      return (wchar_t *)p;
    }
  }
  return NULL;
}
int SDL_wcscmp(const wchar_t *s1, const wchar_t *s2) {
  if (!s1 || !s2)
    return s1 ? 1 : (s2 ? -1 : 0);
  while (*s1 && *s1 == *s2) {
    ++s1;
    ++s2;
  }
  return (int)(*s1 - *s2);
}
int SDL_wcsncmp(const wchar_t *s1, const wchar_t *s2, size_t maxlen) {
  if (maxlen == 0)
    return 0;
  if (!s1 || !s2)
    return s1 ? 1 : (s2 ? -1 : 0);
  while (maxlen && *s1 && *s1 == *s2) {
    ++s1;
    ++s2;
    --maxlen;
  }
  if (maxlen == 0)
    return 0;
  return (int)(*s1 - *s2);
}
static int tcmips_sdl2_wchar_tolower(wchar_t c) {
  return (c >= L'A' && c <= L'Z') ? (c - L'A' + L'a') : c;
}
int SDL_wcscasecmp(const wchar_t *s1, const wchar_t *s2) {
  if (!s1 || !s2)
    return s1 ? 1 : (s2 ? -1 : 0);
  while (*s1 &&
         tcmips_sdl2_wchar_tolower(*s1) == tcmips_sdl2_wchar_tolower(*s2)) {
    ++s1;
    ++s2;
  }
  return (int)(tcmips_sdl2_wchar_tolower(*s1) - tcmips_sdl2_wchar_tolower(*s2));
}
int SDL_wcsncasecmp(const wchar_t *s1, const wchar_t *s2, size_t maxlen) {
  if (maxlen == 0)
    return 0;
  if (!s1 || !s2)
    return s1 ? 1 : (s2 ? -1 : 0);
  while (maxlen && *s1 &&
         tcmips_sdl2_wchar_tolower(*s1) == tcmips_sdl2_wchar_tolower(*s2)) {
    ++s1;
    ++s2;
    --maxlen;
  }
  if (maxlen == 0)
    return 0;
  return (int)(tcmips_sdl2_wchar_tolower(*s1) - tcmips_sdl2_wchar_tolower(*s2));
}

char *SDL_strtokr(char *s1, const char *s2, char **saveptr) {
  return strtok_r(s1, s2, saveptr);
}

size_t SDL_utf8strlen(const char *s) {
  size_t len = 0;
  while (*s) {
    if ((*s & 0xC0) != 0x80)
      ++len;
    ++s;
  }
  return len;
}
size_t SDL_utf8strnlen(const char *s, size_t bytes) {
  size_t len = 0;
  while (bytes && *s) {
    if ((*s & 0xC0) != 0x80)
      ++len;
    ++s;
    --bytes;
  }
  return len;
}

static char *tcmips_sdl2_ltoa_helper(long long value, char *str, int radix,
                                     int is_unsigned) {
  if (!str || radix < 2 || radix > 36)
    return str;
  unsigned long long u;
  int neg = 0;
  if (!is_unsigned && value < 0) {
    neg = 1;
    u = (unsigned long long)(-value);
  } else {
    u = (unsigned long long)value;
  }
  char *p = str;
  do {
    int digit = (int)(u % (unsigned long long)radix);
    *p++ = (char)(digit < 10 ? '0' + digit : 'a' + digit - 10);
    u /= (unsigned long long)radix;
  } while (u != 0);
  if (neg)
    *p++ = '-';
  *p = '\0';
  // Reverse in place.
  char *start = str, *end = p - 1;
  while (start < end) {
    char t = *start;
    *start = *end;
    *end = t;
    ++start;
    --end;
  }
  return str;
}

char *SDL_itoa(int value, char *str, int radix) {
  return tcmips_sdl2_ltoa_helper((long long)value, str, radix, 0);
}
char *SDL_uitoa(unsigned int value, char *str, int radix) {
  return tcmips_sdl2_ltoa_helper((long long)value, str, radix, 1);
}
char *SDL_ltoa(long value, char *str, int radix) {
  return tcmips_sdl2_ltoa_helper((long long)value, str, radix, 0);
}
char *SDL_ultoa(unsigned long value, char *str, int radix) {
  return tcmips_sdl2_ltoa_helper((long long)value, str, radix, 1);
}
char *SDL_lltoa(Sint64 value, char *str, int radix) {
  return tcmips_sdl2_ltoa_helper((long long)value, str, radix, 0);
}
char *SDL_ulltoa(Uint64 value, char *str, int radix) {
  return tcmips_sdl2_ltoa_helper((long long)value, str, radix, 1);
}

int SDL_atoi(const char *s) { return atoi(s); }
double SDL_atof(const char *s) { return atof(s); }
long SDL_strtol(const char *s, char **end, int base) {
  return strtol(s, end, base);
}
unsigned long SDL_strtoul(const char *s, char **end, int base) {
  return strtoul(s, end, base);
}
Sint64 SDL_strtoll(const char *s, char **end, int base) {
  return (Sint64)strtoll(s, end, base);
}
Uint64 SDL_strtoull(const char *s, char **end, int base) {
  return (Uint64)strtoull(s, end, base);
}
double SDL_strtod(const char *s, char **end) { return strtod(s, end); }

int SDL_sscanf(const char *s, const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  int n = vsscanf(s, fmt, ap);
  va_end(ap);
  return n;
}

int SDL_vsscanf(const char *s, const char *fmt, va_list ap) {
  return vsscanf(s, fmt, ap);
}

int SDL_snprintf(char *str, size_t size, const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  int n = vsnprintf(str, size, fmt, ap);
  va_end(ap);
  return n;
}

int SDL_vsnprintf(char *str, size_t size, const char *fmt, va_list ap) {
  return vsnprintf(str, size, fmt, ap);
}

int SDL_vasprintf(char **strp, const char *fmt, va_list ap) {
  return vasprintf(strp, fmt, ap);
}
int SDL_asprintf(char **strp, const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  int n = vasprintf(strp, fmt, ap);
  va_end(ap);
  return n;
}

SDL_iconv_t SDL_iconv_open(const char *tocode, const char *fromcode) {
  (void)tocode;
  (void)fromcode;
  return (SDL_iconv_t)-1;
}

int SDL_iconv_close(SDL_iconv_t cd) {
  (void)cd;
  return 0;
}

size_t SDL_iconv(SDL_iconv_t cd, const char **inbuf, size_t *inbytesleft,
                 char **outbuf, size_t *outbytesleft) {
  (void)cd;
  (void)inbuf;
  (void)inbytesleft;
  (void)outbuf;
  (void)outbytesleft;
  return (size_t)-1;
}

char *SDL_iconv_string(const char *tocode, const char *fromcode,
                       const char *inbuf, size_t inbytesleft) {
  (void)tocode;
  (void)fromcode;
  (void)inbuf;
  (void)inbytesleft;
  return NULL;
}

Uint16 SDL_crc16(Uint16 crc, const void *data, size_t len) {
  (void)crc;
  (void)data;
  (void)len;
  return 0;
}

Uint32 SDL_crc32(Uint32 crc, const void *data, size_t len) {
  (void)crc;
  (void)data;
  (void)len;
  return 0;
}

// Trig wrappers
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmacro-redefined"
#include <math.h>
#pragma GCC diagnostic pop
double SDL_acos(double x) { return acos(x); }
float SDL_acosf(float x) { return acosf(x); }
double SDL_asin(double x) { return asin(x); }
float SDL_asinf(float x) { return asinf(x); }
double SDL_atan(double x) { return atan(x); }
float SDL_atanf(float x) { return atanf(x); }
double SDL_atan2(double y, double x) { return atan2(y, x); }
float SDL_atan2f(float y, float x) { return atan2f(y, x); }
double SDL_ceil(double x) { return ceil(x); }
float SDL_ceilf(float x) { return ceilf(x); }
double SDL_copysign(double x, double y) { return copysign(x, y); }
float SDL_copysignf(float x, float y) { return copysignf(x, y); }
double SDL_cos(double x) { return cos(x); }
float SDL_cosf(float x) { return cosf(x); }
double SDL_exp(double x) { return exp(x); }
float SDL_expf(float x) { return expf(x); }
double SDL_fabs(double x) { return fabs(x); }
float SDL_fabsf(float x) { return fabsf(x); }
double SDL_floor(double x) { return floor(x); }
float SDL_floorf(float x) { return floorf(x); }
double SDL_fmod(double x, double y) { return fmod(x, y); }
float SDL_fmodf(float x, float y) { return fmodf(x, y); }
double SDL_log(double x) { return log(x); }
float SDL_logf(float x) { return logf(x); }
double SDL_log10(double x) { return log10(x); }
float SDL_log10f(float x) { return log10f(x); }
double SDL_pow(double x, double y) { return pow(x, y); }
float SDL_powf(float x, float y) { return powf(x, y); }
double SDL_round(double x) { return round(x); }
float SDL_roundf(float x) { return roundf(x); }
long SDL_lround(double x) { return lround(x); }
long SDL_lroundf(float x) { return lroundf(x); }
double SDL_scalbn(double x, int n) { return scalbn(x, n); }
float SDL_scalbnf(float x, int n) { return scalbnf(x, n); }
double SDL_sin(double x) { return sin(x); }
float SDL_sinf(float x) { return sinf(x); }
double SDL_sqrt(double x) { return sqrt(x); }
float SDL_sqrtf(float x) { return sqrtf(x); }
double SDL_tan(double x) { return tan(x); }
float SDL_tanf(float x) { return tanf(x); }
double SDL_trunc(double x) { return trunc(x); }
float SDL_truncf(float x) { return truncf(x); }
