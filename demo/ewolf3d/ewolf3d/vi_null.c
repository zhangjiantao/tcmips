/* input/video output "/dev/null" support */

/* this file does nothing but sit there and look pretty dumb */

#include "wl_def.h"

byte *gfxbuf = NULL;

static unsigned char pal[768];

/*
==========================
=
= Quit
=
==========================
*/

void Quit(const char *error)
{
	memptr screen = NULL;

	if (!error || !*error) {
		CA_CacheGrChunk(ORDERSCREEN);
		screen = grsegs[ORDERSCREEN];
		WriteConfig();
	} else if (error) {
		CA_CacheGrChunk(ERRORSCREEN);
		screen = grsegs[ERRORSCREEN];
	}
	
	ShutdownId();
	
	if (screen) {
		/* printf("spiffy ansi screen goes here..\n"); */
	}
	
	if (error && *error) {
		fprintf(stderr, "Quit: %s\n", error);
		exit(EXIT_FAILURE);
 	}
	exit(EXIT_SUCCESS);
}

void VL_WaitVBL(myint vbls)
{
}

void VW_UpdateScreen()
{
}

/*
=======================
=
= VL_Startup
=
=======================
*/

void VL_Startup()
{
	if (gfxbuf == NULL) {
		gfxbuf = malloc(320 * 200 * 1);
	}
}

/*
=======================
=
= VL_Shutdown
=
=======================
*/

void VL_Shutdown()
{
	if (gfxbuf != NULL) {
		free(gfxbuf);
		gfxbuf = NULL;
	}
}

//===========================================================================

/*
=================
=
= VL_SetPalette
=
=================
*/

void VL_SetPalette(const byte *palette)
{
	memcpy(pal, palette, 768);
}

/*
=================
=
= VL_GetPalette
=
=================
*/

void VL_GetPalette(byte *palette)
{
	memcpy(palette, pal, 768);
}

void INL_Update()
{
}

void IN_GetMouseDelta(myint *dx, myint *dy)
{
	*dx = 0;
	*dy = 0;
}

byte IN_MouseButtons()
{
	return 0;
}

/*
===================
=
= IN_JoyButtons
=
===================
*/

byte IN_JoyButtons()
{
	return 0;
}

///////////////////////////////////////////////////////////////////////////
//
//	IN_GetJoyAbs() - Reads the absolute position of the specified joystick
//
///////////////////////////////////////////////////////////////////////////
void IN_GetJoyAbs(word joy,word *xp,word *yp)
{
	*xp = 0;
	*yp = 0;
}

///////////////////////////////////////////////////////////////////////////
//
//	INL_GetJoyDelta() - Returns the relative movement of the specified
//		joystick (from +/-127)
//
///////////////////////////////////////////////////////////////////////////
void INL_GetJoyDelta(word joy,myint *dx,myint *dy)
{
	*dx = 0;
	*dy = 0;
}

///////////////////////////////////////////////////////////////////////////
//
//	INL_GetJoyButtons() - Returns the button status of the specified
//		joystick
//
///////////////////////////////////////////////////////////////////////////
word INL_GetJoyButtons(word joy)
{
	return 0;
}

///////////////////////////////////////////////////////////////////////////
//
//      IN_SetupJoy() - Sets up thresholding values and calls INL_SetJoyScale()
//              to set up scaling values
//
///////////////////////////////////////////////////////////////////////////
void IN_SetupJoy(word joy,word minx,word maxx,word miny,word maxy)
{
}

myint main(myint argc, char *argv[])
{
	vwidth = 320;
	vheight = 200;
	
	return WolfMain(argc, argv);
}
