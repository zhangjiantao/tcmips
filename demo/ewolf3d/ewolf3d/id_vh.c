#include "wl_def.h"

#ifndef ENABLE_PRECOMPILE
pictabletype pictable[NUMPICS];
#endif

myint px, py;
byte fontcolor, backcolor;
myint fontnumber;

#ifndef LUMINARY
myint xfrac, yfrac;

boolean	screenfaded;

#ifndef SKIPFADE
static byte palette1[256][3], palette2[256][3];
#endif

/* ======================================================================== */

#ifndef SKIPFADE
void VL_FillPalette(myint red, myint green, myint blue)
{
	byte pal[768];
	myint i;
	
	for (i = 0; i < 256; i++) {
		pal[i*3+0] = red;
		pal[i*3+1] = green;
		pal[i*3+2] = blue;
	}
	VL_SetPalette(pal);
	VW_UpdateScreen();
}
#endif

/*
=================
=
= VL_FadeOut
=
= Fades the current palette to the given color in the given number of steps
=
=================
*/

void VL_FadeOut(myint start, myint end, myint red, myint green, myint blue, myint steps)
{
#ifndef SKIPFADE
	myint i,j,orig,delta;
	byte *origptr, *newptr;

	VL_GetPalette(&palette1[0][0]);
	memcpy(palette2, palette1, 768);

/* fade through intermediate frames */
	for (i = 0; i < steps; i++)
	{
		origptr = &palette1[start][0];
		newptr = &palette2[start][0];
		for (j=start;j<=end;j++)
		{
			orig = *origptr++;
			delta = red-orig;
			*newptr++ = orig + delta * i / steps;
			orig = *origptr++;
			delta = green-orig;
			*newptr++ = orig + delta * i / steps;
			orig = *origptr++;
			delta = blue-orig;
			*newptr++ = orig + delta * i / steps;
		}

		VL_SetPalette(&palette2[0][0]);
		VW_UpdateScreen();
	}

/* final color */
	VL_FillPalette(red, green, blue);
#endif

	screenfaded = true;
}

/*
=================
=
= VL_FadeIn
=
=================
*/

void VL_FadeIn(myint start, myint end, const byte *palette, myint steps)
{
#ifdef SKIPFADE
#ifndef EMBEDDED
	VL_SetPalette(palette);
#endif
#else
	myint i, j, delta;

	VL_GetPalette(&palette1[0][0]);
	memcpy(&palette2[0][0],&palette1[0][0],sizeof(palette1));

	start *= 3;
	end = end*3+2;

/* fade through intermediate frames */
	for (i = 0; i < steps; i++)
	{
		for (j = start;j <= end; j++)
		{
			delta = palette[j]-palette1[0][j];
			palette2[0][j] = palette1[0][j] + delta * i / steps;
		}

		VL_SetPalette(&palette2[0][0]);
		VW_UpdateScreen();
	}

/* final color */
	VL_SetPalette(palette);
#endif
	screenfaded = false;
}
#endif

#ifndef EMBEDDED
void VL_CacheScreen(myint chunk)
{
	CA_CacheGrChunk(chunk);
	VL_MemToScreen(CA_GetChunk(chunk), 320, 200, 0, 0);
	CA_UnCacheGrChunk(chunk);
}

void VL_DeModeXize(byte *buf, myint width, myint height)
{
	byte *mem, *ptr, *destline;
	myint plane, x, y;
	
	if (width & 3) {
		printf("Not divisible by 4?\n");
		return;
	}
	
	MM_GetPtr((memptr)&mem, width * height);
	
	ptr = buf;

	for (plane = 0; plane < 4; plane++) {
		destline = mem;
		for (y = 0; y < height; y++) {
			for (x = 0; x < width / 4; x++)
				*(destline + x*4 + plane) = *ptr++;
			destline += width;
		}
	}

	memcpy(buf, mem, width * height);
	
	MM_FreePtr((memptr)&mem);
}

/*
=================
=
= VL_Plot
=
=================
*/

static void VL_Plot(myint x, myint y, myint color)
{
	myint xend, yend, xs, ys;
	
	xend = x + 1;
	yend = y + 1;
	
	x *= xfrac;
	y *= yfrac;
	xend *= xfrac;
	yend *= yfrac;
	
	x >>= 16;
	y >>= 16;
	xend >>= 16;
	yend >>= 16;
	
	for (xs = x; xs < xend; xs++)
		for (ys = y; ys < yend; ys++)
			*(gfxbuf + ys * vwidth + xs) = color;
}

void VW_Plot(myint x, myint y, myint color)
{
	VL_Plot(x, y, color);
}

/* 
font is:
height is a word
256 word offsets from start
256 byte widths
data
*/
void VW_DrawPropString(const char *string)
{
	byte *font;
	myint width, step, height, x, xs, y;
	byte *source, *ptrs;
	byte ch;

	font = CA_GetChunk(STARTFONT+fontnumber);
	height = font[0] | (font[1] << 8);

	xs = 0;
	
	while ((ch = *string++) != 0) {
		width = step = font[2 + 512 + ch];
		source = font+font[2+ch*2+0]+(font[2+ch*2+1]<<8);
		for (x = 0; x < width; x++) {
			ptrs = source;
			for (y = 0; y < height; y++) {
				if (*ptrs)
					VL_Plot(px+xs, py+y, fontcolor);
				ptrs += step;
			}
			xs++;
			source++;
		}
	}
}

void VW_MeasurePropString(const char *string, word *width, word *height)
{
	myint w, mw;
	byte *font = CA_GetChunk(STARTFONT+fontnumber);
	
	w = 0;
	mw = 0;
	*height = font[0] | (font[1] << 8);
	for (;*string; string++) {
		if (*string == '\n') {
			if (mw < w)
				mw = w;

			w = 0;
			*height += font[0] | (font[1] << 8);
		} else {
			w += font[2 + 512 + *(byte *)string]; 
		}
	}
	if (mw < w)
		mw = w;
		
	*width = mw;
}

void VWB_DrawTile8(myint x, myint y, myint tile)
{
	VL_MemToScreen(CA_GetChunk(STARTTILE8)+(tile*64), 8, 8, x, y);
}

void VWB_DrawPic(myint x, myint y, myint chunknum)
{
	myint picnum = chunknum - STARTPICS;
	myint width, height;

	width = pictable[picnum].width;
	height = pictable[picnum].height;

	VL_MemToScreen(CA_GetChunk(chunknum), width, height, x, y);
}

/*
=================
=
= VL_Hlin
=
=================
*/

void VL_Hlin(unsigned x, unsigned y, unsigned width, unsigned color)
{
/*
	myint xend, yend;
	myint w, h;
	byte *ptr;
	
	xend = x + width;
	yend = y + 1;
	
	x *= xfrac;
	y *= yfrac;
	xend *= xfrac;
	yend *= yfrac;
	
	w = (xend - x) >> 16;
	h = (yend - y) >> 16;

	ptr = gfxbuf + vwidth * (y >> 16) + (x >> 16);
	
	while (h--) {
		memset(ptr, color, w);
		ptr += vwidth;
	}
*/

	unsigned myint w;
	
	for (w = 0; w < width; w++)
		VL_Plot(x+w, y, color);
		
}

/*
=================
=
= VL_Vlin
=
=================
*/

void VL_Vlin(myint x, myint y, myint height, myint color)
{
/*
	myint xend, yend;
	myint w, h;
	byte *ptr;
	
	xend = x + 1;
	yend = y + height;
	
	x *= xfrac;
	y *= yfrac;
	xend *= xfrac;
	yend *= yfrac;
	
	w = (xend - x) >> 16;
	h = (yend - y) >> 16;

	ptr = gfxbuf + vwidth * (y >> 16) + (x >> 16);
	
	while (h--) {
		memset(ptr, color, w);
		ptr += vwidth;
	}
*/
	myint h;
	
	for (h = 0; h < height; h++)
		VL_Plot(x, y+h, color);
}

/*
=================
=
= VL_Bar
=
=================
*/

void VW_Bar(myint x, myint y, myint width, myint height, myint color)
{
	myint w, h;
	byte *ptr;
	
	x *= xfrac;
	y *= yfrac;

	w = (width * xfrac) >> 16;
	h = (height * yfrac) >> 16;
	
	ptr = gfxbuf + vwidth * (y >> 16) + (x >> 16);
	
	while (h--) {
		memset(ptr, color, w);
		ptr += vwidth;
	}
}
#endif

void VL_Bar(myint x, myint y, myint width, myint height, myint color)
{
#ifndef ENABLE_COLOR
	byte *ptr = gfxbuf4 + vpitch * y + (x >> 1);
	byte c;

	c = pal4bit[color];
	c |= c << 4;
	while (height--) {
		memset(ptr, c, width >> 1);
		ptr += vpitch;
	}
#else
	byte *ptr = gfxbuf + vpitch * y + x;
	while (height--) {
		memset(ptr, color, width);
		ptr += vpitch;
	}
#endif
}

#ifndef EMBEDDED
/*
=================
=
= VL_MemToScreen
=
= Draws a block of data to the screen.
=
=================
*/

void VL_MemToScreen(const byte *source, myint width, myint height, myint x, myint y)
{
	myint w, h;
	
	for (w = 0; w < width; w++)
		for (h = 0; h < height; h++)
			VL_Plot(x+w, y+h, source[h*width+w]);
}
#endif

void VW_Startup()
{
	VL_Startup();
	
#ifndef EMBEDDED
	xfrac = (vwidth << 16) / 320;
	yfrac = (vheight << 16) / 200;
#endif
}

#ifndef EMBEDDED
void VW_Shutdown()
{
	VL_Shutdown();
}
#endif
