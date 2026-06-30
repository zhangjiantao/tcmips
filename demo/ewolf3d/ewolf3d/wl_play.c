#include "wl_def.h"

/*
=============================================================================

						 GLOBAL VARIABLES

=============================================================================
*/

boolean		madenoise;		// true when shooting or screaming

exit_t		playstate;

objtype 	objlist[MAXACTORS],*new,*obj,*player,*lastobj,
			*objfreelist,*killerobj;

#ifndef EMBEDDED
myint		DebugOk;
boolean		singlestep,godmode,noclip;
#endif

byte		tilemap[MAPSIZE][MAPSIZE];	// wall values only
mapbitmap		spotvis;
mapbitmap		objactor;
byte		actorat[MAPSIZE][MAPSIZE];

myint tics;

//
// control info
//
#ifdef ENABLE_MOUSE
boolean		mouseenabled;
#endif
#ifdef ENABLE_JOYSTICK
boolean joystickenabled,joypadenabled;
myint			joystickport;
byte			buttonjoy[4]={bt_attack,bt_strafe,bt_use,bt_run};
#endif
CONTROL_CONST
byte			dirscan[4] = {sc_UpArrow,sc_RightArrow,sc_DownArrow,sc_LeftArrow};
CONTROL_CONST
byte			buttonscan[NUMBUTTONS] =
			{sc_Control,sc_Alt,sc_RShift,sc_Space,sc_1,sc_2,sc_3,sc_4};
#ifdef ENABLE_MOUSE
CONTROL_CONST
byte			buttonmouse[4]={bt_attack,bt_strafe,bt_use,bt_nobutton};
#endif

boolean		buttonheld[NUMBUTTONS];

#ifdef ENABLE_DEMO
boolean		demorecord,demoplayback;
byte		*demoptr, *lastdemoptr;
memptr		demobuffer;
#endif

//
// curent user input
//
myint		controlx,controly;	/* range from -100 to 100 per tic */
boolean		buttonstate[NUMBUTTONS];

static void RemoveObj(objtype *gone);

/*
=============================================================================

						 LOCAL VARIABLES

=============================================================================
*/

#ifdef ENABLE_MUSIC
/* LIST OF SONGS FOR EACH VERSION */
static const myint songs[]=
{
#ifndef SPEAR
/* Episode One */
 GETTHEM_MUS,
 SEARCHN_MUS,
 POW_MUS,
 SUSPENSE_MUS,
 GETTHEM_MUS,
 SEARCHN_MUS,
 POW_MUS,
 SUSPENSE_MUS,
 WARMARCH_MUS,	/* Boss level */
 CORNER_MUS,	/* Secret level */

/* Episode Two */
 NAZI_OMI_MUS,
 PREGNANT_MUS,
 GOINGAFT_MUS,
 HEADACHE_MUS,
 NAZI_OMI_MUS,
 PREGNANT_MUS,
 HEADACHE_MUS,
 GOINGAFT_MUS,
 WARMARCH_MUS,	/* Boss level */
 DUNGEON_MUS,	/* Secret level */

/*  Episode Three */
 INTROCW3_MUS,
 NAZI_RAP_MUS,
 TWELFTH_MUS,
 ZEROHOUR_MUS,
 INTROCW3_MUS,
 NAZI_RAP_MUS,
 TWELFTH_MUS,
 ZEROHOUR_MUS,
 ULTIMATE_MUS,	/* Boss level */
 PACMAN_MUS,	/* Secret level */

/* Episode Four */
 GETTHEM_MUS,
 SEARCHN_MUS,
 POW_MUS,
 SUSPENSE_MUS,
 GETTHEM_MUS,
 SEARCHN_MUS,
 POW_MUS,
 SUSPENSE_MUS,
 WARMARCH_MUS,	/* Boss level */
 CORNER_MUS,	/* Secret level */

/* Episode Five */
 NAZI_OMI_MUS,
 PREGNANT_MUS,
 GOINGAFT_MUS,
 HEADACHE_MUS,
 NAZI_OMI_MUS,
 PREGNANT_MUS,
 HEADACHE_MUS,
 GOINGAFT_MUS,
 WARMARCH_MUS,	/* Boss level */
 DUNGEON_MUS,	/* Secret level */

 /* Episode Six */
 INTROCW3_MUS,
 NAZI_RAP_MUS,
 TWELFTH_MUS,
 ZEROHOUR_MUS,
 INTROCW3_MUS,
 NAZI_RAP_MUS,
 TWELFTH_MUS,
 ZEROHOUR_MUS,
 ULTIMATE_MUS,	/* Boss level */
 FUNKYOU_MUS	/* Secret level */
#else

 //////////////////////////////////////////////////////////////
 //
 // SPEAR OF DESTINY TRACKS
 //
 //////////////////////////////////////////////////////////////
 XTIPTOE_MUS,
 XFUNKIE_MUS,
 XDEATH_MUS,
 XGETYOU_MUS,
 ULTIMATE_MUS,	/* Trans Gr”sse */

 DUNGEON_MUS,
 GOINGAFT_MUS,
 POW_MUS,
 TWELFTH_MUS,
 ULTIMATE_MUS,	/* Barnacle Wilhelm BOSS */

 NAZI_OMI_MUS,
 GETTHEM_MUS,
 SUSPENSE_MUS,
 SEARCHN_MUS,
 ZEROHOUR_MUS,
 ULTIMATE_MUS,	/* Super Mutant BOSS */

 XPUTIT_MUS,
 ULTIMATE_MUS,	/* Death Knight BOSS */

 XJAZNAZI_MUS,	/* Secret level */
 XFUNKIE_MUS,	/* Secret level */

 XEVIL_MUS	/* Angel of Death BOSS */

#endif
};
#endif


/*
=============================================================================

						  USER CONTROL

=============================================================================
*/


#define BASEMOVE		35
#define RUNMOVE			70

/*
===================
=
= PollKeyboardButtons
=
===================
*/

void PollKeyboardButtons()
{
	myint i;

	for (i=0;i<NUMBUTTONS;i++)
		if (IN_KeyDown(buttonscan[i]))
			buttonstate[i] = true;
}


#ifdef ENABLE_MOUSE
/*
===================
=
= PollMouseButtons
=
===================
*/

void PollMouseButtons()
{
	myint	buttons;

	buttons = IN_MouseButtons();

	if (buttons&1)
		buttonstate[buttonmouse[0]] = true;
	if (buttons&2)
		buttonstate[buttonmouse[1]] = true;
	if (buttons&4)
		buttonstate[buttonmouse[2]] = true;
}
#endif


#ifdef ENABLE_JOYSTICK
/*
===================
=
= PollJoystickButtons
=
===================
*/

void PollJoystickButtons()
{
	myint	buttons;

	buttons = IN_JoyButtons();

	if (joystickport && !joypadenabled)
	{
		if (buttons&4)
			buttonstate[buttonjoy[0]] = true;
		if (buttons&8)
			buttonstate[buttonjoy[1]] = true;
	}
	else
	{
		if (buttons&1)
			buttonstate[buttonjoy[0]] = true;
		if (buttons&2)
			buttonstate[buttonjoy[1]] = true;
		if (joypadenabled)
		{
			if (buttons&4)
				buttonstate[buttonjoy[2]] = true;
			if (buttons&8)
				buttonstate[buttonjoy[3]] = true;
		}
	}
}
#endif

/*
===================
=
= PollKeyboardMove
=
===================
*/

void PollKeyboardMove()
{
	if (buttonstate[bt_run])
	{
		if (IN_KeyDown(dirscan[di_north]))
			controly -= RUNMOVE*tics;
		if (IN_KeyDown(dirscan[di_south]))
			controly += RUNMOVE*tics;
		if (IN_KeyDown(dirscan[di_west]))
			controlx -= RUNMOVE*tics;
		if (IN_KeyDown(dirscan[di_east]))
			controlx += RUNMOVE*tics;
	}
	else
	{
		if (IN_KeyDown(dirscan[di_north]))
			controly -= BASEMOVE*tics;
		if (IN_KeyDown(dirscan[di_south]))
			controly += BASEMOVE*tics;
		if (IN_KeyDown(dirscan[di_west]))
			controlx -= BASEMOVE*tics;
		if (IN_KeyDown(dirscan[di_east]))
			controlx += BASEMOVE*tics;
	}
}


#ifdef ENABLE_MOUSE
/*
===================
=
= PollMouseMove
=
===================
*/

void PollMouseMove()
{
	myint mousexmove = 0, mouseymove = 0;

	IN_GetMouseDelta(&mousexmove, &mouseymove);
	
	controlx += mousexmove*10/(13-mouseadjustment);
	controly += mouseymove*20/(13-mouseadjustment);
}
#endif

#ifdef ENABLE_JOYSTICK
/*
===================
=
= PollJoystickMove
=
===================
*/

void PollJoystickMove()
{
	myint joyx, joyy;

	INL_GetJoyDelta(joystickport,&joyx,&joyy);

	if (buttonstate[bt_run])
	{
		if (joyx > 64)
			controlx += RUNMOVE*tics;
		else if (joyx < -64)
			controlx -= RUNMOVE*tics;
		if (joyy > 64)
			controly += RUNMOVE*tics;
		else if (joyy < -64)
			controly -= RUNMOVE*tics;
	}
	else
	{
		if (joyx > 64)
			controlx += BASEMOVE*tics;
		else if (joyx < -64)
			controlx -= BASEMOVE*tics;
		if (joyy > 64)
			controly += BASEMOVE*tics;
		else if (joyy < -64)
			controly -= BASEMOVE*tics;
	}
}
#endif

void UpdateInput()
{
//
// get button states
//
	PollKeyboardButtons();

#ifdef ENABLE_MOUSE
	if (mouseenabled)
		PollMouseButtons();
#endif

#ifdef ENABLE_JOYSTICK
	if (joystickenabled)
		PollJoystickButtons();
#endif

//
// get movements
//
	PollKeyboardMove();

#ifdef ENABLE_MOUSE
	if (mouseenabled)
		PollMouseMove();
#endif

#ifdef ENABLE_JOYSTICK
	if (joystickenabled)
		PollJoystickMove();
#endif
}

/*
===================
=
= PollControls
=
= Gets user or demo input, call once each frame
=
= controlx		set between -100 and 100 per tic
= controly
= buttonheld[]	the state of the buttons LAST frame
= buttonstate[]	the state of the buttons THIS frame
=
===================
*/

void PollControls()
{
	myint max, min;
#ifdef ENABLE_DEMO
	myint i;
	byte buttonbits;
#endif
	static boolean shoothack;

	controlx = 0;
	controly = 0;
	if (buttonstate[bt_attack])
	    shoothack = true;
	memcpy(buttonheld, buttonstate, sizeof(buttonstate));
	memset(buttonstate, 0, sizeof(buttonstate));

#ifdef ENABLE_DEMO
	if (demoplayback) {
	//
	// read commands from demo buffer
	//
		buttonbits = *demoptr++;
		for (i = 0; i < NUMBUTTONS; i++)
		{
			buttonstate[i] = buttonbits&1;
			buttonbits >>= 1;
		}

		controlx = (signed char)*demoptr++;
		controly = (signed char)*demoptr++;

		if (demoptr == lastdemoptr)
			playstate = ex_completed;		// demo is done

		controlx *= (myint)tics;
		controly *= (myint)tics;

		return;
	}
#endif

	/* Update keys */
#ifdef EMBEDDED
	INL_Update();
#else
	IN_CheckAck(); 
#endif
	
	UpdateInput();
	
#ifdef EMBEDDED
	if (shoothack) {
	    if (buttonstate[bt_use]) {
		buttonstate[bt_attack] = true;
	    } else {
		shoothack = false;
	    }
	}
#endif
//
// bound movement to a maximum
//
	max = 100*tics;
	min = -max;
	if (controlx > max)
		controlx = max;
	else if (controlx < min)
		controlx = min;

	if (controly > max)
		controly = max;
	else if (controly < min)
		controly = min;

#ifdef ENABLE_DEMO
	if (demorecord)
	{
	//
	// save info out to demo buffer
	//
		controlx /= (myint)tics;
		controly /= (myint)tics;

		buttonbits = 0;

		for (i=NUMBUTTONS-1;i>=0;i--)
		{
			buttonbits <<= 1;
			if (buttonstate[i])
				buttonbits |= 1;
		}

		*demoptr++ = buttonbits;
		*demoptr++ = controlx;
		*demoptr++ = controly;

		if (demoptr >= lastdemoptr)
			Quit("Demo buffer overflowed!");

		controlx *= (myint)tics;
		controly *= (myint)tics;
	}
#endif
}



//==========================================================================



///////////////////////////////////////////////////////////////////////////
//
//	CenterWindow() - Generates a window of a given width & height in the
//		middle of the screen
//
///////////////////////////////////////////////////////////////////////////

#define MAXX	320
#define MAXY	160

void CenterWindow(word w,word h)
{
#ifndef EMBEDDED
	US_DrawWindow(((MAXX / 8) - w) / 2,((MAXY / 8) - h) / 2,w,h);
#endif
}

//===========================================================================


/*
=====================
=
= CheckKeys
=
=====================
*/

void CheckKeys()
{
	byte	scan;

	if (screenfaded	// don't do anything with a faded screen
#ifdef ENABLE_DEMO
	      || demoplayback
#endif
	      )
		return;

	scan = LastScan;

#ifndef EMBEDDED
	//
	// SECRET CHEAT CODE: TAB-G-F10
	//
	if (IN_KeyDown(sc_Tab) && IN_KeyDown(sc_G) && IN_KeyDown(sc_F10)) {
		WindowH = 160;
		if (godmode)
		{
			Message ("God mode OFF");
			SD_PlaySound (NOBONUSSND);
		}
		else
		{
			Message ("God mode ON");
			SD_PlaySound (ENDBONUS2SND);
		}

		IN_Ack();
		godmode ^= 1;
		DrawPlayBorderSides();
		IN_ClearKeysDown();
		return;
	}

	//
	// SECRET CHEAT CODE: 'MLI'
	//
	if (IN_KeyDown(sc_M) &&	IN_KeyDown(sc_L) && IN_KeyDown(sc_I)) {
		gamestate.health = 100;
		gamestate.ammo = 99;
		gamestate.keys = 3;
		gamestate.score = 0;
		gamestate.TimeCount += 42000L;
		GiveWeapon(wp_chaingun);

		DrawWeapon();
		DrawHealth();
		DrawKeys();
		DrawAmmo();
		DrawScore();

		ClearMemory();
		CA_CacheGrChunk(STARTFONT+1);
		ClearSplitVWB();

		Message(STR_CHEATER);

		CA_UnCacheGrChunk(STARTFONT+1);
		IN_ClearKeysDown();
		IN_Ack();

		DrawPlayBorder();
	}

	//
	// OPEN UP DEBUG KEYS
	//
	if (IN_KeyDown(sc_BackSpace) && IN_KeyDown(sc_LShift) &&
		IN_KeyDown(sc_Alt) && MS_CheckParm("debugmode")) {
	 ClearMemory();
	 CA_CacheGrChunk(STARTFONT+1);
	 ClearSplitVWB();

	 Message(STR_DEBUG);
	 
	 CA_UnCacheGrChunk(STARTFONT+1);
	 IN_ClearKeysDown();
	 IN_Ack();

	 DrawPlayBorderSides();
	 DebugOk=1;
	}

	//
	// TRYING THE KEEN CHEAT CODE!
	//
	if (IN_KeyDown(sc_B) && IN_KeyDown(sc_A) && IN_KeyDown(sc_T)) {
		ClearMemory();
		CA_CacheGrChunk(STARTFONT+1);
		ClearSplitVWB();

		Message(STR_KEEN);

		CA_UnCacheGrChunk(STARTFONT+1);
		IN_ClearKeysDown();
		IN_Ack();

		DrawPlayBorder();
	}

	if (Paused) {
		VWB_DrawPic(128, 64, PAUSEDPIC);
		VW_UpdateScreen();
		
		SD_MusicOff();
		IN_Ack();
		IN_ClearKeysDown();
		SD_MusicOn();
		
		IN_GetMouseDelta(NULL, NULL); // Clear accumulated mouse movement
		
		Paused = false;		
		return;
	}


//
// F1-F7/ESC to enter control panel
//
	if (
		scan == sc_F10 ||
		scan == sc_F9 ||
		scan == sc_F7 ||
		scan == sc_F8)			// pop up quit dialog
	{
		ClearMemory();
		ClearSplitVWB();
		US_ControlPanel(scan);
	
		DrawPlayBorderSides();
		VW_UpdateScreen();
		
		if (scan == sc_F9)
			StartMusic();

		SETFONTCOLOR(0,15);
		IN_ClearKeysDown();
		
		lasttimecount = get_TimeCount();
		
		return;
	}

	if ( (scan >= sc_F1 && scan <= sc_F9) || scan == sc_Escape)
	{
		StopMusic();
		ClearMemory();
		VW_FadeOut();

		US_ControlPanel(scan);

		SETFONTCOLOR(0,15);
		IN_ClearKeysDown();
		DrawPlayScreen();
		VW_UpdateScreen();
		
		if (!startgame && !loadedgame)
		{
			VW_FadeIn();
			StartMusic();
		}
		if (loadedgame)
			playstate = ex_abort;
		lasttimecount = get_TimeCount();
		
		IN_GetMouseDelta(NULL, NULL); // Clear accumulated mouse movement
		
		return;
	}

//
// TAB-? debug keys
//
	if (IN_KeyDown(sc_Tab) && DebugOk) {
		CA_CacheGrChunk (STARTFONT);
		fontnumber=0;
		SETFONTCOLOR(0,15);
		DebugKeys();
		lasttimecount = get_TimeCount();
		
		IN_GetMouseDelta(NULL, NULL); // Clear accumulated mouse movement
		
		return;
	}
#endif

}


//===========================================================================

/*
#############################################################################

				  The objlist data structure

#############################################################################

objlist containt structures for every actor currently playing.  The structure
is accessed as a linked list starting at *player, ending when ob->next ==
NULL.  GetNewObj inserts a new object at the end of the list, meaning that
if an actor spawn another actor, the new one WILL get to think and react the
same frame.  RemoveObj unlinks the given object and returns it to the free
list, but does not damage the objects ->next pointer, so if the current object
removes itself, a linked list following loop can still safely get to the
next element.

<backwardly linked free list>

#############################################################################
*/


/*
=========================
=
= InitActorList
=
= Call to clear out the actor object lists returning them all to the free
= list.  Allocates a special spot for the player.
=
=========================
*/

void InitActorList()
{
	myint	i;

//
// init the actor lists
//
	for (i = 0; i < MAXACTORS; i++)
	{
		//objlist[i].id = i;
		objlist[i].prev = i+1;
		objlist[i].next = INVALID_OBJID;
	}

	objlist[MAXACTORS-1].prev = INVALID_OBJID;

	objfreelist = &objlist[0];
	lastobj = NULL;

/* give the player the first free spots */
	GetNewActor();
	player = new;

}

//===========================================================================

/*
=========================
=
= GetNewActor
=
= Sets the global variable new to point to a free spot in objlist.
= The free spot is inserted at the end of the liked list
=
=========================
*/

void GetNewActor()
{
	if (!objfreelist)
		Quit("GetNewActor: No free spots in objlist!");
	
	new = objfreelist;
	objfreelist = obj_prev(new);
	
	memset(new, 0, sizeof(*new));
	
	if (lastobj)
	  {
		lastobj->next = obj_id(new);
		new->prev = obj_id(lastobj);
	  }
	else
	  new->prev = INVALID_OBJID;
	new->next = INVALID_OBJID;

	new->active = ac_no;
	lastobj = new;
}

//===========================================================================

/*
=========================
=
= RemoveObj
=
= Add the given object back into the free list, and unlink it from its
= neighbors
=
=========================
*/

static void RemoveObj(objtype *gone)
{
	if (gone == player)
		Quit("RemoveObj: Tried to remove the player!");

	gone->state = s_none;

//
// fix the next object's back link
//
	if (gone == lastobj)
		lastobj = obj_prev(gone);
	else
		obj_next(gone)->prev = gone->prev;

//
// fix the previous object's forward link
//
	if (gone->prev != INVALID_OBJID)
	    obj_prev(gone)->next = gone->next;

//
// add it back in to the free list
//
	if (objfreelist)
	  gone->prev = obj_id(objfreelist);
	else
	  gone->prev = INVALID_OBJID;
	objfreelist = gone;

}

#ifdef ENABLE_MUSIC
/*
=============================================================================

						MUSIC STUFF

=============================================================================
*/


/*
=================
=
= StopMusic
=
=================
*/

void StopMusic()
{
	FreeMusic();
}

/*
=================
=
= StartMusic
=
=================
*/

void StartMusic()
{
	StartCPMusic(songs[gamestate.mapon+gamestate_episode*10]);
}
#endif


/*
=============================================================================

					PALETTE SHIFTING STUFF

=============================================================================
*/

#ifndef ENABLE_FLASHES

void InitRedShifts()
{
}

#else

#define NUMREDSHIFTS	6
#define REDSTEPS		8

#define NUMWHITESHIFTS	3
#define WHITESTEPS		20
#define WHITETICS		6


static byte	redshifts[NUMREDSHIFTS][768];
static byte	whiteshifts[NUMREDSHIFTS][768];

myint	damagecount,bonuscount;
boolean	palshifted;

/*
=====================
=
= InitRedShifts
=
=====================
*/

void InitRedShifts()
{
	byte *workptr;
	const byte *baseptr;

	myint	i,j,delta;


//
// fade through intermediate frames
//
	for (i=1;i<=NUMREDSHIFTS;i++)
	{
		workptr = (byte *)&redshifts[i-1][0];
		baseptr = gamepal;

		for (j=0;j<=255;j++)
		{
			delta = 64-*baseptr;
			*workptr++ = *baseptr++ + delta * i / REDSTEPS;
			delta = -*baseptr;
			*workptr++ = *baseptr++ + delta * i / REDSTEPS;
			delta = -*baseptr;
			*workptr++ = *baseptr++ + delta * i / REDSTEPS;
		}
	}

	for (i=1;i<=NUMWHITESHIFTS;i++)
	{
		workptr = (byte *)&whiteshifts[i-1][0];
		baseptr = gamepal;

		for (j=0;j<=255;j++)
		{
			delta = 64-*baseptr;
			*workptr++ = *baseptr++ + delta * i / WHITESTEPS;
			delta = 62-*baseptr;
			*workptr++ = *baseptr++ + delta * i / WHITESTEPS;
			delta = 0-*baseptr;
			*workptr++ = *baseptr++ + delta * i / WHITESTEPS;
		}
	}
}
#endif


#ifdef ENABLE_FLASHES
/*
=====================
=
= ClearPaletteShifts
=
=====================
*/

void ClearPaletteShifts()
{
	bonuscount = damagecount = 0;

}
#endif


/*
=====================
=
= StartBonusFlash
=
=====================
*/

void StartBonusFlash()
{
#ifdef ENABLE_FLASHES
	bonuscount = NUMWHITESHIFTS*WHITETICS;		// white shift palette
#endif

}


/*
=====================
=
= StartDamageFlash
=
=====================
*/

void StartDamageFlash(myint damage)
{
#ifdef ENABLE_FLASHES
	damagecount += damage;
#endif

}


/*
=====================
=
= UpdatePaletteShifts
=
=====================
*/

#ifdef ENABLE_FLASHES
void UpdatePaletteShifts()
{
	myint red, white;

	if (bonuscount)
	{
		white = bonuscount/WHITETICS +1;
		if (white>NUMWHITESHIFTS)
			white = NUMWHITESHIFTS;
		bonuscount -= tics;
		if (bonuscount < 0)
			bonuscount = 0;
	}
	else
		white = 0;


	if (damagecount)
	{
		red = damagecount/10 +1;
		if (red>NUMREDSHIFTS)
			red = NUMREDSHIFTS;

		damagecount -= tics;
		if (damagecount < 0)
			damagecount = 0;
	}
	else
		red = 0;

	if (red)
	{
		VL_SetPalette(redshifts[red-1]);
		palshifted = true;
	}
	else if (white)
	{
		VL_SetPalette(whiteshifts[white-1]);
		palshifted = true;
	}
	else if (palshifted)
	{
		VL_SetPalette(gamepal);		// back to normal
		palshifted = false;
	}

}


/*
=====================
=
= FinishPaletteShifts
=
= Resets palette to normal if needed
=
=====================
*/

void FinishPaletteShifts()
{
	if (palshifted)
	{
		palshifted = 0;
		VL_SetPalette(gamepal);
	}
}
#endif


/*
=============================================================================

						CORE PLAYLOOP

=============================================================================
*/


/*
=====================
=
= DoActor
=
=====================
*/

static void DoActor(objtype *ob)
{
	void (*think)(objtype *);

	if (!ob->active && !getareabyplayer(ob->areanumber))
		return;

	if (!(ob->flags & (FL_NONMARK|FL_NEVERMARK)))
		clear_actor(ob->tilex, ob->tiley);

//
// non transitional object
//

	if (!ob->ticcount)
	{
		think =	THINK_FN(ob);
		if (think)
		{
			think(ob);
			if (ob->state == s_none)
			{
				RemoveObj (ob);
				return;
			}
		}

		if (ob->flags&FL_NEVERMARK)
			return;

		if ((ob->flags&FL_NONMARK) && any_actor_at(ob->tilex, ob->tiley))
			return;

		move_actor(ob);
		return;
	}

//
// transitional object
//
	ob->ticcount-=tics;
	while (ob->ticcount <= 0)
	{
		think = ACTION_FN(ob);	// end of state action
		if (think)
		{
			think(ob);
			if (ob->state == s_none)
			{
				RemoveObj(ob);
				return;
			}
		}

		ob->state = gamestates[ob->state].next;

		if (ob->state == s_none)
		{
			RemoveObj(ob);
			return;
		}

		if (!gamestates[ob->state].tictime)
		{
			ob->ticcount = 0;
			goto think;
		}

		ob->ticcount += gamestates[ob->state].tictime;
	}

think:
	//
	// think
	//
	think =	THINK_FN(ob);
	if (think)
	{
		think(ob);
		if (ob->state == s_none)
		{
			RemoveObj(ob);
			return;
		}
	}

	if (ob->flags&FL_NEVERMARK)
		return;

	if ((ob->flags&FL_NONMARK) && any_actor_at(ob->tilex, ob->tiley))
		return;

	move_actor(ob);
}

//==========================================================================


/*
===================
=
= PlayLoop
=
===================
*/

long funnyticount;

void PlayLoop()
{
	playstate = lasttimecount = 0;
	
	frameon = 0;
	anglefrac = 0;
#ifndef EMBEDDED
	facecount = 0;
#endif
	funnyticount = 0;
	
	memset (buttonstate,0,sizeof(buttonstate));
#ifdef ENABLE_FLASHES
	ClearPaletteShifts();
#endif

#ifdef ENABLE_MOUSE
	IN_GetMouseDelta(NULL, NULL); // Clear accumulated mouse movement
#endif
		
#ifdef ENABLE_DEMO
	if (demoplayback)
		IN_StartAck();
#endif

	set_TimeCount(0);
	
	do
	{
		/* get timing info for last frame */
		CalcTics();
		
		/* handle input */
		PollControls();

//
// actor thinking
//
		madenoise = false;

		MoveDoors();
		MovePWalls();

		for (obj = player; obj; obj = obj_next(obj))
			DoActor(obj);
		
#ifdef ENABLE_FLASHES
		UpdatePaletteShifts();
#endif

		ThreeDRefresh();

		//
		// MAKE FUNNY FACE IF BJ DOESN'T MOVE FOR AWHILE
		//
		#ifdef SPEAR
		funnyticount += tics;
		if (funnyticount > 30l*70)
		{
			funnyticount = 0;
			StatusDrawPic (17,4,BJWAITING1PIC+(US_RndT()&1));
			facecount = 0;
		}
		#endif

		gamestate.TimeCount += tics;

#ifdef ENABE_AUDIO
 		UpdateSoundLoc(player->x, player->y, player->angle);
#endif

		if (screenfaded)
			VW_FadeIn();

		CheckKeys();

//
// debug aids
//
#ifndef EMBEDDED
		if (singlestep)
		{
			VW_WaitVBL(14);
			lasttimecount = get_TimeCount();
		}
#endif

#ifdef ENABLE_DEMO
		if (demoplayback)
		{
			if (IN_CheckAck())
			{
				IN_ClearKeysDown();
				playstate = ex_abort;
			}
		}
#endif

	} while (!playstate && !startgame);

#ifdef ENABLE_FLASHES
	if (playstate != ex_died)
		FinishPaletteShifts();
#endif
}
