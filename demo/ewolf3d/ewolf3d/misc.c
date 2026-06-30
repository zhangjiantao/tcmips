#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>
#include <fcntl.h>
              
#include "wl_def.h"

#ifndef O_BINARY
#define O_BINARY 0
#endif

#ifdef INTEGRATOR
volatile unsigned long tcount;
static volatile int tlock;

void set_TimeCount(unsigned long t)
{
    tlock = 1;
    tcount = t;
    tlock = 0;
}

unsigned long get_TimeCount(void)
{
    return tcount;
}

#define pic ((volatile uint32_t *)0x14000000)
#define pit ((volatile uint32_t *)0x13000100)

static uint32_t irqstack[4];

static void __attribute__((interrupt("irq"))) 
irqhandler (void)
{
    if (!tlock)
	tcount++;
    /* Clear interrupt.  */
    pit[3] = 1;
}

void TimerInit()
{
    register uint32_t r1 asm("r1");
    (*(volatile uint32_t *)0x18) = 0xe59ff018; /* ldr pc, 0x38 */
    (*(volatile uint32_t *)0x38) = (uint32_t)irqhandler;
    /* Enable IRQ 6.  */
    pic[2] = 1 << 6;
    /* Setup timer.  */
    pit[0] = 1000000 / 65; /* 70Hz.  */
    pit[2] = 0xe2; /* Enable periodic 32-bit timer.  */
    /* Set the IRQ stack and unmask interrupts.  */
    r1 = (uint32_t)(irqstack + 4);
    asm volatile ("mrs r0, cpsr\n\t"
		  "bic r0, r0, #0x0f\n\t"
		  "orr r0, r0, #0x02\n\t"
		  "msr cpsr_cxsf, r0\n\t"
		  "mov sp, r1\n\t"
		  "bic r0, r0, #0x8f\n\t"
		  "orr r0, r0, #0x03\n\t"
		  "msr cpsr_cxsf, r0"
		  ::"r"(r1): "r0");
}

#elif defined (LUMINARY)
#include "luminary.h"

volatile unsigned long tcount;
static volatile int tlock;

void set_TimeCount(unsigned long t)
{
    tlock = 1;
    tcount = t;
    tlock = 0;
}

unsigned long get_TimeCount(void)
{
    return tcount;
}

#define treg ((volatile int *)0x40030000)

void timer_isr()
{
    /* Clear interrupt.   */
    treg[9] = 1;
    if (!tlock)
	tcount++;
}

void TimerInit()
{
    /* Setup and enable timer.  */
    treg[0] = 0;
    treg[1] = 2;
    treg[10] = 50000000 / 70;
    treg[6] = 1;
    treg[3] = 1;

    /* Enable Timer and SSI interrupts.  */
    NVIC[0x40] |= (1 << 19) | (1 << 7);
}

#else
static struct timeval t0;
static unsigned long tc0;

void set_TimeCount(unsigned long t)
{
	tc0 = t;
	
	gettimeofday(&t0, NULL);
}

unsigned long get_TimeCount(void)
{
	struct timeval t1;
	long secs, usecs;
	unsigned long tc;
	
	gettimeofday(&t1, NULL);

	secs = t1.tv_sec - t0.tv_sec;
	usecs = t1.tv_usec - t0.tv_usec;

	if (usecs < 0) {
		usecs += 1000000;
		secs--;
	}

	tc = tc0 + secs * 70 + (usecs * 70) / 1000000;
		
	return tc;
}
#endif

unsigned long sleepuntil(unsigned long t)
{
    unsigned long now;
    while (1) {
	now = get_TimeCount();
	if (now >= t)
	    break;
#if defined(__linux__)
	usleep(10000);
#elif defined(__thumb2__)
	asm volatile ("wfi");
#elif defined(__arm__)
	asm volatile ("mcr p15, 0, r1, c7, c0, 4 @ wait for interrupt");
#endif
    }
    return now;
}

#ifndef EMBEDDED
long filelength(myint handle)
{
#ifdef __linux__
	struct stat buf;
	
	if (fstat(handle, &buf) == -1) {
		perror("filelength");
		exit(EXIT_FAILURE);
	}
	
	return buf.st_size;
#else
	return 0;
#endif
}

char *itoa(myint value, char *string, myint radix)
{
	return ltoa(value, string, radix);
}

char *ltoa(long value, char *string, myint radix)
{
	if (value < 0) {
	    string[0] = '-';
	    ultoa(-value, string + 1, radix);
	    return string;
	}
	return ultoa(value, string, radix);
}

char *ultoa(unsigned long value, char *string, myint radix)
{
	unsigned long tmp;
	char *p;
	if (value == 0) {
	    string[0] = '0';
	    string[1] = 0;
	    return string;
	}
	p = string;
	for (tmp = value; tmp != 0; tmp = tmp / 10) {
	    p++;
	}
	*p = 0;
	while (value != 0) {
	    p--;
	    *p = (value % 10) + '0';
	    value /= 10;
	}
	return string;
}

/* from Dan Olson */
static void put_dos2ansi(byte attrib)
{
	byte fore,back,blink=0,intens=0;
	
	fore = attrib&15;	// bits 0-3
	back = attrib&112; // bits 4-6
       	blink = attrib&128; // bit 7
	
	// Fix background, blink is either on or off.
	back = back>>4;

	// Fix foreground
	if (fore > 7) {
		intens = 1;
		fore-=8;
	}

	// Convert fore/back
	switch (fore) {
		case 0: // BLACK
			fore=30;
			break;
		case 1: // BLUE
			fore=34;
			break;
		case 2: // GREEN
			fore=32;
			break;
		case 3: // CYAN
			fore=36;
			break;
		case 4: // RED
			fore=31;
			break;
		case 5: // Magenta
			fore=35;
			break;
		case 6: // BROWN(yellow)
			fore=33;
			break;
		case 7: //GRAy
			fore=37;
			break;
	}
			
	switch (back) {
		case 0: // BLACK
			back=40;
			break;
		case 1: // BLUE
			back=44;
			break;
		case 2: // GREEN
			back=42;
			break;
		case 3: // CYAN
			back=46;
			break;
		case 4: // RED
			back=41;
			break;
		case 5: // Magenta
			back=45;
			break;
		case 6: // BROWN(yellow)
			back=43;
			break;
		case 7: //GRAy
			back=47;
			break;
	}
}

void DisplayTextSplash(const byte *text, myint l)
{
	myint i, x;
	
	//printf("%02X %02X %02X %02X\n", text[0], text[1], text[2], text[3]);
	text += 4;
	//printf("%02X %02X %02X %02X\n", text[0], text[1], text[2], text[3]);
	text += 2;
	
	for (x = 0; x < l; x++) {
		for (i = 0; i < 160; i += 2) {
			put_dos2ansi(text[160*x+i+2]);
			if (text[160*x+i+1] && text[160*x+i+1] != 160)
				printf("%c", text[160*x+i+1]);
			else
				printf(" ");
		}
		printf("%c[m", 27);
		printf("\n");
	}
}
#endif

/* ** */

uint16_t SwapInt16L(uint16_t i)
{
#if BYTE_ORDER == BIG_ENDIAN
	return ((uint16_t)i >> 8) | ((uint16_t)i << 8);
#else
	return i;
#endif
}

uint32_t SwapInt32L(uint32_t i)
{
#if BYTE_ORDER == BIG_ENDIAN
	return	((uint32_t)(i & 0xFF000000) >> 24) | 
		((uint32_t)(i & 0x00FF0000) >>  8) |
		((uint32_t)(i & 0x0000FF00) <<  8) | 
		((uint32_t)(i & 0x000000FF) << 24);
#else
	return i;
#endif
}

/* ** */

#ifndef ENABLE_PRECOMPILE
myint OpenWrite(const char *fn)
{
	myint fp;
	
	fp = open(fn, O_CREAT|O_WRONLY|O_TRUNC|O_BINARY, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH);
	return fp;
}

myint OpenWriteAppend(const char *fn)
{
	myint fp;
	
	fp = open(fn, O_CREAT|O_WRONLY|O_BINARY, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH);
	return fp;
}

void CloseWrite(myint fp)
{
	close(fp);
}

myint WriteSeek(myint fp, myint offset, myint whence)
{
	return lseek(fp, offset, whence);
}

myint WritePos(myint fp)
{
	return lseek(fp, 0, SEEK_CUR);
}

myint WriteInt8(myint fp, int8_t d)
{
	return write(fp, &d, 1);
}

myint WriteInt16(myint fp, int16_t d)
{
	int16_t b = SwapInt16L(d);
	
	return write(fp, &b, 2) / 2;
}

myint WriteInt32(myint fp, int32_t d)
{
	int32_t b = SwapInt32L(d);
	
	return write(fp, &b, 4) / 4;
}

myint WriteBytes(myint fp, const byte *d, myint len)
{
	return write(fp, d, len);
}


myint OpenRead(const char *fn)
{
	myint fp;
	
	fp = open(fn, O_RDONLY | O_BINARY);
	
	return fp;
}

void CloseRead(myint fp)
{
	close(fp);
}

myint ReadSeek(myint fp, myint offset, myint whence)
{
	return lseek(fp, offset, whence);
}

myint ReadLength(myint fp)
{
	return filelength(fp);
}

int8_t ReadInt8(myint fp)
{
	byte d[1];
	
	read(fp, d, 1);
	
	return d[0];
}

int16_t ReadInt16(myint fp)
{
	byte d[2];
	
	read(fp, d, 2);
	
	return (d[0]) | (d[1] << 8);
}

int32_t ReadInt32(myint fp)
{
	byte d[4];
	
	read(fp, d, 4);
	
	return (d[0]) | (d[1] << 8) | (d[2] << 16) | (d[3] << 24);
}

myint ReadBytes(myint fp, byte *d, myint len)
{
	return read(fp, d, len);
}
#endif

myshort atan2fix(fixed y, fixed x)
{
    boolean negative;
    long long quot;
    fixed tang;
    int offset;
    int res;
    if (x < 0) {
	x = -x;
	negative = true;
	offset = 180;
    } else {
	negative = false;
	offset = 0;
    }
    if (y < 0) {
	y = -y;
	negative = !negative;
	if (negative)
	    offset = 360;
    }
    if (x == 0)
      return negative ? 270 : 90;
    if (y == 0)
      return offset;
    quot = ((long long)y << 16) / x;
    tang = (fixed)quot;
    if (quot != tang) {
	/* Overflow.  */
	res = 90;
    } else {
	int low = 0;
	int high = FINEANGLES / 4 - 1;

	res = 0;
	while (low + 1 < high) {
	    res = (low + high) >> 1;
	    if (finetangent[res] < tang)
		low = res;
	    else
		high = res;
	}
	res = res / (FINEANGLES / ANGLES);
    }
    if (negative)
	res = -res;
    return res + offset;
}

fixed sinfix(int a)
{
    if (a > ANGLES / 2) {
	a -= ANGLES / 2;
	if (a > ANGLES / 4)
	  a = (ANGLES / 2) - a;
	if (a == ANGLES / 4)
	  return -GLOBAL1;
	return -(fixed)sintable[a];
    } else {
	if (a > ANGLES / 4)
	  a = (ANGLES / 2) - a;
	if (a == ANGLES / 4)
	  return GLOBAL1;
	return (fixed)sintable[a];
    }
}

fixed cosfix(int a)
{
    a += ANGLES / 4;
    if (a >= ANGLES)
	a -= ANGLES;
    return sinfix(a);
}

