#include "wl_def.h"

#include "SDL/SDL.h"

static SDL_Surface *surface;
static unsigned myint sdl_palettemode;

byte framebuffer[128 * 64 / 2];

extern void keyboard_handler(myint code, myint press);
extern boolean InternalKeyboard[NumCodes];

myint main (myint argc, char *argv[])
{
	return WolfMain(argc, argv);
}

void DisplayTextSplash(byte *text);

/*
==========================
=
= Quit
=
==========================
*/

void Quit(const char *error)
{
#ifndef EMBEDDED
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
		//DisplayTextSplash(screen);
	}
#endif
	
	if (error && *error) {
		fprintf(stderr, "Quit: %s\n", error);
		exit(EXIT_FAILURE);
 	}
	exit(EXIT_SUCCESS);
}

void VL_WaitVBL(myint vbls)
{
	unsigned long last = get_TimeCount() + vbls;
	while (last > get_TimeCount()) ;
}

void VW_UpdateScreen()
{
#ifdef EMBEDDED
	byte *out;
	byte *in;
	int x;
       	int y;
	byte c;

	out = surface->pixels;
	in = framebuffer;
	for (y = 0; y < 64; y++)
	  {
	    for (x = 0; x < 64; x++)
	      {
		c = *(in++);
		out[(x << 1)] = c >> 4;
		out[(x << 1) + 1] = c & 0xf;
	      }
	    out += surface->pitch;
	  }
#endif
	SDL_Flip(surface);

#ifndef EMBEDDED
	gfxbuf = surface->pixels;
#endif
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
	const SDL_VideoInfo *vinfo;
	myint flags;
	
#ifndef EMBEDDED
	vwidth = 320;
	vheight = 200;
	
	if (MS_CheckParm("x2")) {
		vwidth *= 2;
		vheight *= 2;
	} else if (MS_CheckParm("x3")) {
		vwidth *= 3;
		vheight *= 3;
	}
#endif
	
	if (SDL_Init(SDL_INIT_VIDEO|SDL_INIT_NOPARACHUTE) < 0) {
		Quit("Couldn't init SDL");
	}

	vinfo = SDL_GetVideoInfo();
	sdl_palettemode = (vinfo->vfmt->BitsPerPixel == 8) ? (SDL_PHYSPAL|SDL_LOGPAL) : SDL_LOGPAL;
	
	flags = SDL_SWSURFACE|SDL_HWPALETTE|SDL_DOUBLEBUF;
#ifndef EMBEDDED
	if (MS_CheckParm("fullscreen"))
		flags |= SDL_FULLSCREEN;
#endif
		
	surface = SDL_SetVideoMode(vwidth, vheight, 8, flags);
		
	if (surface == NULL) {
		SDL_Quit();
		Quit("(SDL) Couldn't set video mode");
	}
#ifndef EMBEDDED
	gfxbuf = surface->pixels;
#else
{
	SDL_Color colors[256];
	myint i;
	
	for (i = 0; i < 15; i++)
	{
		colors[i].r = i << 4;
		colors[i].g = i << 4;
		colors[i].b = i << 4;
	}
	SDL_SetPalette(surface, sdl_palettemode, colors, 0, 16);
}

#endif
	
	if (surface->flags & SDL_FULLSCREEN)
		SDL_ShowCursor(0);
	
	SDL_WM_SetCaption(GAMENAME, GAMENAME);	
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
	SDL_Quit();
}

/* ======================================================================== */

#ifndef EMBEDDED
/*
=================
=
= VL_SetPalette
=
=================
*/

void VL_SetPalette(const byte *palette)
{
	SDL_Color colors[256];
	myint i;
	
	VL_WaitVBL(1);
	
	for (i = 0; i < 256; i++)
	{
		colors[i].r = palette[i*3+0] << 2;
		colors[i].g = palette[i*3+1] << 2;
		colors[i].b = palette[i*3+2] << 2;
	}
	SDL_SetPalette(surface, sdl_palettemode, colors, 0, 256);
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
	myint i;
	for (i=0;i<256;i++)
	{
		palette[i*3+0] = surface->format->palette->colors[i].r >> 2;
		palette[i*3+1] = surface->format->palette->colors[i].g >> 2;
		palette[i*3+2] = surface->format->palette->colors[i].b >> 2;
	}
}
#endif

static myint XKeysymToScancode(unsigned myint keysym)
{
	switch (keysym) {
		case SDLK_KP_ENTER:
		case SDLK_RETURN:
			return sc_Enter;
		case SDLK_ESCAPE:
			return sc_Escape;
		case SDLK_SPACE:
			return sc_Space;
		case SDLK_BACKSPACE:
			return sc_BackSpace;
		case SDLK_TAB:
			return sc_Tab;
		case SDLK_LALT:
		case SDLK_RALT:
			return sc_Alt;
		case SDLK_LCTRL:
		case SDLK_RCTRL:
			return sc_Control;
		case SDLK_CAPSLOCK:
			return sc_CapsLock;
		case SDLK_LSHIFT:
			return sc_LShift;
		case SDLK_RSHIFT:
			return sc_RShift;
		
		case SDLK_UP:
		case SDLK_KP8:
			return sc_UpArrow;
		case SDLK_DOWN:
		case SDLK_KP2:
			return sc_DownArrow;
		case SDLK_LEFT:
		case SDLK_KP4:
			return sc_LeftArrow;
		case SDLK_RIGHT:
		case SDLK_KP6:
			return sc_RightArrow;
		case SDLK_HOME:
			return sc_Home;
		case SDLK_END:
			return sc_End;
		case SDLK_PAGEUP:
			return sc_PgUp;
		case SDLK_PAGEDOWN:
			return sc_PgDn;
			
		case SDLK_INSERT:
			return sc_Insert;
		case SDLK_DELETE:
			return sc_Delete;
		case SDLK_F1:
			return sc_F1;
		case SDLK_F2:
			return sc_F2;
		case SDLK_F3:
			return sc_F3;
		case SDLK_F4:
			return sc_F4;
		case SDLK_F5:
			return sc_F5;
		case SDLK_F6:
			return sc_F6;
		case SDLK_F7:
			return sc_F7;
		case SDLK_F8:
			return sc_F8;
		case SDLK_F9:
			return sc_F9;
		case SDLK_F10:
			return sc_F10;
		case SDLK_F11:
			return sc_F11;
		case SDLK_F12:
			return sc_F12;	
		
		case SDLK_1:
			return sc_1;
		case SDLK_2:
			return sc_2;
		case SDLK_3:
			return sc_3;
		case SDLK_4:
			return sc_4;
		case SDLK_5:
			return sc_5;
		case SDLK_6:
			return sc_6;
		case SDLK_7:
			return sc_7;
		case SDLK_8:
			return sc_8;
		case SDLK_9:
			return sc_9;
		case SDLK_0:
			return sc_0;
		
		case SDLK_a:
			return sc_A;
		case SDLK_b:
			return sc_B;
		case SDLK_c:
			return sc_C;
		case SDLK_d:
			return sc_D;
		case SDLK_e:
			return sc_E;
		case SDLK_f:
			return sc_F;
		case SDLK_g:
			return sc_G;
		case SDLK_h:
			return sc_H;
		case SDLK_i:
			return sc_I;
		case SDLK_j:
			return sc_J;
		case SDLK_k:
			return sc_K;
		case SDLK_l:
			return sc_L;
		case SDLK_m:
			return sc_M;
		case SDLK_n:
			return sc_N;
		case SDLK_o:
			return sc_O;
		case SDLK_p:
			return sc_P;
		case SDLK_q:
			return sc_Q;
		case SDLK_r:
			return sc_R;
		case SDLK_s:
			return sc_S;
		case SDLK_t:
			return sc_T;
		case SDLK_u:
			return sc_U;
		case SDLK_v:
			return sc_V;
		case SDLK_w:
			return sc_W;
		case SDLK_x:
			return sc_X;
		case SDLK_y:
			return sc_Y;
		case SDLK_z:
			return sc_Z;
		
		case SDLK_PAUSE:
			return 0xE1;
		default:
			return sc_None;
	}
}

void INL_Update()
{
	SDL_Event event;
		
	if (SDL_PollEvent(&event)) {
		do {
			switch(event.type) {
				case SDL_KEYDOWN:
					keyboard_handler(XKeysymToScancode(event.key.keysym.sym), 1);
					break;
				case SDL_KEYUP:
					keyboard_handler(XKeysymToScancode(event.key.keysym.sym), 0);
					break;
				case SDL_QUIT:
					Quit(NULL);
					break;
				default:
					break;
			}
		} while (SDL_PollEvent(&event));
	}
	
#ifndef EMBEDDED
	if (InternalKeyboard[sc_Alt] && 
	(!DebouncedKeyboard[sc_Return] && InternalKeyboard[sc_Return])) {
		SDL_GrabMode gm;
		
		SDL_WM_ToggleFullScreen(surface);
		
		gm = SDL_WM_GrabInput(SDL_GRAB_QUERY);
		if (gm == SDL_GRAB_OFF && !(surface->flags & SDL_FULLSCREEN))
			SDL_ShowCursor(1);
		else
			SDL_ShowCursor(0);
	}
	
	if (InternalKeyboard[sc_Control] &&
	(!DebouncedKeyboard[sc_G] && InternalKeyboard[sc_G])) {
		SDL_GrabMode gm;
		
		gm = SDL_WM_GrabInput(SDL_GRAB_QUERY);
		SDL_WM_GrabInput((gm == SDL_GRAB_ON) ? SDL_GRAB_OFF : SDL_GRAB_ON);
		
		gm = SDL_WM_GrabInput(SDL_GRAB_QUERY);
		if (gm == SDL_GRAB_OFF && !(surface->flags & SDL_FULLSCREEN))
			SDL_ShowCursor(1);
		else
			SDL_ShowCursor(0);
	}
#endif
	
	/* ctrl-z for iconify window? */
}

void IN_GetMouseDelta(myint *dx, myint *dy)
{
	myint x, y;
	
	SDL_GetRelativeMouseState(&x, &y);
	
	if (dx)
		*dx = x;
	if (dy)
		*dy = y;
}

byte IN_MouseButtons()
{
	Uint8 state;
	byte retr;
	
	state = SDL_GetMouseState(NULL, NULL);
	
	retr = 0;
	
	if (state & SDL_BUTTON(SDL_BUTTON_LEFT)) {
		retr |= 1;
	}
	
	if (state & SDL_BUTTON(SDL_BUTTON_RIGHT)) {
		retr |= 2;
	}
	
	if (state & SDL_BUTTON(SDL_BUTTON_MIDDLE)) {
		retr |= 4;
	}
	
	return retr;
}

#ifndef EMBEDDED
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
#endif

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
