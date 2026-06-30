#ifndef __ID_VH_H__
#define __ID_VH_H__

#define WHITE			15
#define BLACK			0

typedef struct
{
	myshort width, height;
} pictabletype;

#ifdef ENABLE_PRECOMPILE
extern const pictabletype pictable[NUMPICS];
#else
extern pictabletype pictable[NUMPICS];
#endif

extern byte fontcolor, backcolor;
extern myint fontnumber;
extern myint px, py;

#define SETFONTCOLOR(f, b) { fontcolor = f; backcolor = b; }

void VW_UpdateScreen();

void VWB_DrawTile8(myint x, myint y, myint tile);
void VWB_DrawPic(myint x, myint y, myint chunknum);

extern boolean screenfaded;

#define VW_Hlin(x,z,y,c)	VL_Hlin(x,y,(z)-(x)+1,c)
#define VW_Vlin(y,z,x,c)	VL_Vlin(x,y,(z)-(y)+1,c)
#define VW_WaitVBL		VL_WaitVBL
#ifdef LUMINARY
#define VW_FadeIn()		do {} while (0)
#define VW_FadeOut()		do {} while (0)
#else
#define VW_FadeIn()		VL_FadeIn(0,255,gamepal,30);
#define VW_FadeOut()		VL_FadeOut(0,255,0,0,0,30);
#endif
void	VW_MeasurePropString(const char *string, word *width, word *height);

void VW_DrawPropString(const char *string);

void VL_FadeOut(myint start, myint end, myint red, myint green, myint blue, myint steps);
void VL_FadeIn(myint start, myint end, const byte *palette, myint steps);

void VL_CacheScreen(myint chunk);

void VW_Bar(myint x, myint y, myint width, myint height, myint color);

void VW_Startup();
void VW_Shutdown();

void VL_FillPalette(myint red, myint green, myint blue);
void VW_Plot(myint x, myint y, myint color);
void VL_Hlin(unsigned x, unsigned y, unsigned width, unsigned color);
void VL_Vlin(myint x, myint y, myint height, myint color);
void VL_Bar(myint x, myint y, myint width, myint height, myint color);

void VL_MemToScreen(const byte *source, myint width, myint height, myint x, myint y);

void VL_DeModeXize(byte *buf, myint width, myint height);

#endif
