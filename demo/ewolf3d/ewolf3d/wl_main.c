#include "wl_def.h"

/*
=============================================================================

						   WOLFENSTEIN 3-D

					  An Id Software production

						   by John Carmack

=============================================================================
*/

#ifndef EMBEDDED
char str[80], str2[20];
#endif

#ifndef EMBEDDED
myint viewwidth, sviewheight;
myint xoffset, yoffset;
myint viewsize;

myint centerx;
myint shootdelta;			/* pixels away from centerx a target can be */
myint viewwidthwin, viewheightwin; /* for borders */
myint vwidth, vheight, vpitch; /* size of screen */
#endif

boolean startgame,loadedgame;
#ifdef MOUSE_ENABLED
myint mouseadjustment;
#endif

long frameon;
long lasttimecount;
fixed viewsin, viewcos;
fixed viewx, viewy;		/* the focal point */
#ifndef EMBEDDED
myshort pixelangle[MAXVIEWWIDTH];
//int32_t finetangent[FINEANGLES/4];
myshort horizwall[MAXWALLTILES], vertwall_table[MAXWALLTILES];
#endif

#ifndef EMBEDDED
char configname[13] = "config." GAMEEXT;

myint _argc;
char **_argv;
#endif

/*
========================
=
= FixedByFrac (FixedMul)
=
= multiply two 16/16 bit, 2's complement fixed point numbers
=
========================
*/
#ifdef NOASM
#if defined(__INTEL_COMPILER)
typedef _int64 int64_t;
#endif
#endif

/*
=====================
=
= CalcTics
=
=====================
*/

void CalcTics()
{
	myint newtime;
	myint ticcount;
	
#ifdef ENABLE_DEMO
	if (demoplayback || demorecord)
		ticcount = DEMOTICS - 1; /* [70/4] 17.5 Hz */
	else
#endif
		ticcount = 0 + 1; /* 35 Hz */
	
	newtime = sleepuntil(lasttimecount + ticcount);
	tics = newtime - lasttimecount;
	
	lasttimecount = newtime;
	
#ifdef ENABLE_DEMO
	if (demoplayback || demorecord)
		tics = DEMOTICS;
	else
#endif
	if (tics > MAXTICS)
		tics = MAXTICS;

#ifdef LUMINARY
	/* Start screensaver after 30 seconds.  */
	if (newtime - LastEventTime > 70 * 30) {
	    VL_ScreenSaver();
	}
#endif
}

/* ======================================================================== */

#ifndef EMBEDDED
static void DiskFlopAnim(myint x, myint y)
{
	static char which = 0;
	
	if (!x && !y)
		return;
	
	VWB_DrawPic(x, y, C_DISKLOADING1PIC+which);
	VW_UpdateScreen();
	
	which ^= 1;
}

static int32_t CalcFileChecksum(myint fd, myint len)
{
	int8_t buf[32];
	
	int32_t cs;
	myint i, j;
	int8_t c1;
	
	cs = 0;
	c1 = ReadInt8(fd);
	
	len--;
	while (len > 0) {
		i = 32;
		if (len < i) {
			i = len;
		}
		
		ReadBytes(fd, (byte *)buf, i);
		
		for (j = 0; j < i; j++) {
			cs += c1 ^ buf[j];
			c1 = buf[j];
		}
		
		len -= 32;
	}
	
	return cs;
}
#endif

myint WriteConfig()
{
#ifndef EMBEDDED
#ifdef ENABLE_CONTROLS
	myint i;
#endif
	myint fd;
	int32_t cs;
	
	fd = OpenWrite(configname);
	
	if (fd != -1) {
		WriteBytes(fd, (byte *)GAMEHDR, 8);	/* WOLF3D, 0, 0 */
		WriteBytes(fd, (byte *)CFGTYPE, 4);	/* CFG, 0 */
		WriteInt32(fd, 0x00000000);		/* Version (integer) */
		WriteBytes(fd, (byte *)GAMETYPE, 4);	/* XXX, 0 */
		WriteInt32(fd, time(NULL));		/* Time */
		WriteInt32(fd, 0x00000000);		/* Padding */
		WriteInt32(fd, 0x00000000);		/* Checksum (placeholder) */
	
#ifdef ENABLE_HIGHSCORES
		for (i = 0; i < 7; i++) { /* MaxScores = 7 */
			WriteBytes(fd, (byte *)Scores[i].name, 58);
			WriteInt32(fd, Scores[i].score);
			WriteInt32(fd, Scores[i].completed);
			WriteInt32(fd, Scores[i].episode);
		}
#endif
		
		WriteInt32(fd, viewsize);

		/* sound config, etc. (to be done) */
		WriteInt32(fd, 0); /* padding */
		WriteInt32(fd, 0); /* padding */
		WriteInt32(fd, 0); /* padding */
		WriteInt32(fd, 0); /* padding */
		WriteInt32(fd, 0); /* padding */
		WriteInt32(fd, 0); /* padding */
		WriteInt32(fd, 0); /* padding */
		WriteInt32(fd, 0); /* padding */
			
#ifdef ENABLE_CONTROLS
		/* direction keys */	
		for (i = 0; i < 4; i++) {
			WriteInt32(fd, dirscan[i]);
		}
			
		/* other game keys */
		for (i = 0; i < 8; i++) { /* NUMBUTTONS = 8 */
			WriteInt32(fd, buttonscan[i]);
		}
			
		/* mouse enabled */
		WriteInt8(fd, mouseenabled);
			
		/* mouse buttons */
		for (i = 0; i < 4; i++) {
			WriteInt32(fd, buttonmouse[i]);
		}
#endif
			
		/* mouse adjustment */
		WriteInt32(fd, mouseadjustment);
			
		/* joystick (to be done) */
		WriteInt32(fd, -1);

		CloseWrite(fd);
		
		fd = OpenRead(configname);
		ReadSeek(fd, 32, SEEK_SET);
		cs = CalcFileChecksum(fd, ReadLength(fd) - 32);
		CloseRead(fd);
		
		fd = OpenWriteAppend(configname);
		WriteSeek(fd, 28, SEEK_SET);
		WriteInt32(fd, cs);
		
		CloseWrite(fd);
	}
	
#endif
	return 0;
}

static void SetDefaults()
{
#ifndef EMBEDDED
	viewsize = 15;
#endif
	
#ifdef MOUSE_ENABLED
	mouseenabled = false;
	mouseadjustment = 5;
#endif

#ifdef JOYSTICK_ENABLED
	joystickenabled = false;
	joypadenabled = false;
	joystickport = 0;
#endif

#ifdef ENABLE_AUDIO
	SD_SetMusicMode(smm_AdLib);
	SD_SetSoundMode(sdm_AdLib);
	SD_SetDigiDevice(sds_SoundBlaster);
#endif
}

myint ReadConfig()
{
#ifndef EMBEDDED
	myint fd, configokay;
	char buf[8];
	int32_t version, v;
#ifdef ENABLE_CONTROLS
	myint i;
#endif
	
	configokay = 0;
	
	fd = OpenRead(configname);
	
	if (fd != -1) {
		SetDefaults();
		
		ReadBytes(fd, (byte *)buf, 8);
		if (strncmp(buf, GAMEHDR, 8))
			goto configend;
		
		ReadBytes(fd, (byte *)buf, 4);
		if (strncmp(buf, CFGTYPE, 4))
			goto configend;
		
		version = ReadInt32(fd);
		if (version != 0xFFFFFFFF && version != 0x00000000)
			goto configend;
		
		ReadBytes(fd, (byte *)buf, 4);
		if (strncmp(buf, GAMETYPE, 4))
			goto configend;
		
		ReadInt32(fd);	/* skip over time */
		ReadInt32(fd);	/* skip over padding */
		
		v = ReadInt32(fd);	/* get checksum */
		if (v != CalcFileChecksum(fd, ReadLength(fd) - 32))
			goto configend;
		
		ReadSeek(fd, 32, SEEK_SET);
		
#ifdef ENABLE_HIGHSCORES
		for (i = 0; i < 7; i++) { /* MaxScores = 7 */
			ReadBytes(fd, (byte *)Scores[i].name, 58);
			Scores[i].score = ReadInt32(fd);
			Scores[i].completed = ReadInt32(fd);
			Scores[i].episode = ReadInt32(fd);
		}
#endif
		
		viewsize = ReadInt32(fd);
		
		/* load the new data */
		if (version == 0x00000000) {
			/* sound config, etc. */
			ReadInt32(fd); /* padding */
			ReadInt32(fd); /* padding */
			ReadInt32(fd); /* padding */
			ReadInt32(fd); /* padding */
			ReadInt32(fd); /* padding */
			ReadInt32(fd); /* padding */
			ReadInt32(fd); /* padding */
			ReadInt32(fd); /* padding */
			
#ifdef ENABLE_CONTROLS
			/* direction keys */	
			for (i = 0; i < 4; i++) {
				dirscan[i] = ReadInt32(fd);
			}
			
			/* other game keys */
			for (i = 0; i < 8; i++) { /* NUMBUTTONS = 8 */
				buttonscan[i] = ReadInt32(fd);
			}
			
			/* mouse enabled */
			mouseenabled = ReadInt8(fd);
			
			/* mouse buttons */
			for (i = 0; i < 4; i++) {
				buttonmouse[i] = ReadInt32(fd);
			}
#endif
			
			/* mouse adjustment */
			mouseadjustment = ReadInt32(fd);
			
			/* unimplemented joystick */
			v = ReadInt32(fd);
			if (v != 0xFFFFFFFF) {
			}
		}
#ifndef EMBEDDED
#ifdef UPLOAD		
		MainMenu[readthis].active = 1;
		MainItems.curpos = 0;
#endif
#endif

		configokay = 1;
	} 
	
configend:	
	
	if (fd != -1)
		CloseRead(fd);
	
	if (!configokay) {
		SetDefaults();
	}
#else
	SetDefaults();
#endif
	
	return 0;
}

#ifndef EMBEDDED
myint SaveTheGame(const char *fn, const char *tag, myint dx, myint dy)
{
	objtype *ob;
	myint fd, i, x, y;
	int32_t cs;
	
	fd = OpenWrite(fn);
	
	if (fd != -1) {
		WriteBytes(fd, (byte *)GAMEHDR, 8);
		WriteBytes(fd, (byte *)SAVTYPE, 4);
		WriteInt32(fd, 0x00000000); /* write version */
		WriteBytes(fd, (byte *)GAMETYPE, 4);
	
		WriteInt32(fd, time(NULL));
		WriteInt32(fd, 0x00000000);
	
		WriteInt32(fd, 0x00000000); /* write checksum (placeholder) */
	
		WriteBytes(fd, (byte *)tag, 32); /* write savegame name */
	
		DiskFlopAnim(dx, dy);
	
		WriteInt32(fd, gamestate_difficulty);
		WriteInt32(fd, gamestate.mapon);
		WriteInt32(fd, gamestate.oldscore);
		WriteInt32(fd, gamestate.score);
		WriteInt32(fd, gamestate.nextextra);
		WriteInt32(fd, gamestate.lives);
		WriteInt32(fd, gamestate.health);
		WriteInt32(fd, gamestate.ammo);
		WriteInt32(fd, gamestate.keys);
		WriteInt32(fd, gamestate.bestweapon);
		WriteInt32(fd, gamestate.weapon);
		WriteInt32(fd, gamestate.chosenweapon);
		WriteInt32(fd, gamestate.faceframe);
		WriteInt32(fd, gamestate.attackframe);
		WriteInt32(fd, gamestate.attackcount);
		WriteInt32(fd, gamestate.weaponframe);
		WriteInt32(fd, gamestate_episode);
		WriteInt32(fd, gamestate.secretcount);
		WriteInt32(fd, gamestate.treasurecount);
		WriteInt32(fd, gamestate.killcount);
		WriteInt32(fd, gamestate.secrettotal);
		WriteInt32(fd, gamestate.treasuretotal);
		WriteInt32(fd, gamestate.killtotal);
		WriteInt32(fd, gamestate.TimeCount);
		WriteInt32(fd, gamestate.killx);
		WriteInt32(fd, gamestate.killy);
		WriteInt8(fd, gamestate.victoryflag);
	
		DiskFlopAnim(dx, dy);
	
#ifdef SPEAR
		for (i = 0; i < 20; i++) {
#else
		for (i = 0; i < 8; i++) {
#endif
			WriteInt32(fd, LevelRatios[i].kill);
			WriteInt32(fd, LevelRatios[i].secret);
			WriteInt32(fd, LevelRatios[i].treasure);
			WriteInt32(fd, LevelRatios[i].time);
		}
	
		DiskFlopAnim(dx, dy);
	
		WriteBytes(fd, (byte *)tilemap, 64*64); /* MAPSIZE * MAPSIZE */
	
		DiskFlopAnim(dx, dy);
	
		for (x = 0; x < 64; x++)
			for (y = 0; y < 64; y++)
				WriteInt32(fd, actorat[x][y]);
	
		DiskFlopAnim(dx, dy);
			
		WriteBytes(fd, (byte *)&areabyplayer, 8);
	
		for (ob = player; ob; ob = obj_next(ob)) {
			DiskFlopAnim(dx, dy);
			
			WriteInt32(fd, obj_id(ob));
			WriteInt32(fd, ob->active);
			WriteInt32(fd, ob->ticcount);
			WriteInt32(fd, ob->obclass);
			WriteInt32(fd, ob->state);
			WriteInt8(fd,  ob->flags);
			WriteInt32(fd, ob->distance);
			WriteInt32(fd, ob->dir);
			WriteInt32(fd, ob->x);
			WriteInt32(fd, ob->y);
			WriteInt32(fd, ob->tilex);
			WriteInt32(fd, ob->tiley);
			WriteInt8(fd,  ob->areanumber);
			WriteInt32(fd, ob->viewx);
			WriteInt32(fd, ob->viewheight);
			WriteInt32(fd, ob->transx);
			WriteInt32(fd, ob->transy);
			WriteInt32(fd, ob->angle);
			WriteInt32(fd, ob->hitpoints);
			WriteInt32(fd, ob->speed);
			WriteInt32(fd, ob->temp1);
			WriteInt32(fd, ob->temp2);
			//WriteInt32(fd, ob->temp3);
		}	
		
		WriteInt32(fd, 0xFFFFFFFF); /* end of actor list */
		
		DiskFlopAnim(dx, dy);
		
		WriteInt32(fd, laststatobj - statobjlist); /* ptr offset */

		for (i = 0; i < 400; i++) { /* MAXSTATS */
			WriteInt8(fd,  statobjlist[i].tilex);
			WriteInt8(fd,  statobjlist[i].tiley);
			WriteInt32(fd, statobjlist[i].shapenum);
			WriteInt8(fd,  statobjlist[i].is_bonus);
			WriteInt8(fd,  statobjlist[i].itemnumber);
		}
	
		DiskFlopAnim(dx, dy);
	
		for (i = 0; i < 64; i++) { /* MAXDOORS */
			WriteInt32(fd, doorposition[i]);
		}
	
		DiskFlopAnim(dx, dy);
	
		for (i = 0; i < 64; i++) { /* MAXDOORS */
			WriteInt8(fd,  doorobjlist[i].tilex);
			WriteInt8(fd,  doorobjlist[i].tiley);
			WriteInt8(fd,  doorobjlist[i].vertical);
			WriteInt8(fd,  doorobjlist[i].lock);
			WriteInt8(fd,  doorobjlist[i].action);
			WriteInt32(fd, doorobjlist[i].ticcount);
		}
	
		DiskFlopAnim(dx, dy);
	
		WriteInt32(fd, pwallstate);
		WriteInt32(fd, pwallx);
		WriteInt32(fd, pwally);
		WriteInt32(fd, pwalldir);
		WriteInt32(fd, pwallpos);

		DiskFlopAnim(dx, dy);

		CloseWrite(fd);

		fd = OpenRead(fn);
		ReadSeek(fd, 64, SEEK_SET);
		cs = CalcFileChecksum(fd, ReadLength(fd) - 64);
		CloseRead(fd);
		
		fd = OpenWriteAppend(fn);
		WriteSeek(fd, 28, SEEK_SET);
		WriteInt32(fd, cs);
		
		CloseWrite(fd);
	} else {
		Message(STR_NOSPACE1"\n"
			STR_NOSPACE2);
			
		IN_ClearKeysDown();
		IN_Ack();
		
		return -1;
	}
	
	return 0;
}

myint ReadSaveTag(const char *fn, const char *tag)
{
	char buf[8];
	myint fd;
	int32_t v;
	
	fd = OpenRead(fn);
	if (fd == -1)
		goto rstfail;
	
	ReadBytes(fd, (byte *)buf, 8);
	if (strncmp(buf, GAMEHDR, 8))
		goto rstfail;
	
	ReadBytes(fd, (byte *)buf, 4);
	if (strncmp(buf, SAVTYPE, 4))
		goto rstfail;
	
	v = ReadInt32(fd);
	if (v != 0xFFFFFFFF && v != 0x00000000) /* -1 and 0 are the same */
		goto rstfail;
	
	ReadBytes(fd, (byte *)buf, 4);
	if (strncmp(buf, GAMETYPE, 4))
		goto rstfail;
	
	ReadInt32(fd);
	ReadInt32(fd);
	
	v = ReadInt32(fd); /* get checksum */
	
	ReadSeek(fd, 64, SEEK_SET);
	if (v != CalcFileChecksum(fd, ReadLength(fd) - 64))
		goto rstfail;
	
	ReadSeek(fd, 32, SEEK_SET);
	ReadBytes(fd, (byte *)tag, 32);
		
	CloseRead(fd);
	
	return 0;
rstfail:
	if (fd != -1)
		CloseRead(fd);
	
	return -1;
}

myint LoadTheGame(const char *fn, myint dx, myint dy)
{
	char buf[8];
	myint fd, i, x, y, id;
	int32_t v;
	
	fd = OpenRead(fn);

	if (fd == -1)
		goto loadfail;
	
	ReadBytes(fd, (byte *)buf, 8);
	if (strncmp(buf, GAMEHDR, 8))
		goto loadfail;
	
	ReadBytes(fd, (byte *)buf, 4);
	if (strncmp(buf, SAVTYPE, 4))
		goto loadfail;
	
	v = ReadInt32(fd);
	if (v != 0xFFFFFFFF && v != 0x00000000) /* -1 and 0 are the same */
		goto loadfail;
	
	ReadBytes(fd, (byte *)buf, 4);
	if (strncmp(buf, GAMETYPE, 4))
		goto loadfail;
	
	ReadInt32(fd);
	ReadInt32(fd);
	
	v = ReadInt32(fd); /* get checksum */
	
	ReadSeek(fd, 64, SEEK_SET);
	if (v != CalcFileChecksum(fd, ReadLength(fd) - 64))
		goto loadfail;
	
	ReadSeek(fd, 64, SEEK_SET);
	
	DiskFlopAnim(dx, dy);
	
	gamestate_difficulty	= ReadInt32(fd);
	gamestate.mapon		= ReadInt32(fd);
	gamestate.oldscore	= ReadInt32(fd);
	gamestate.score		= ReadInt32(fd);
	gamestate.nextextra	= ReadInt32(fd);
	gamestate.lives		= ReadInt32(fd);
	gamestate.health	= ReadInt32(fd);
	gamestate.ammo		= ReadInt32(fd);
	gamestate.keys		= ReadInt32(fd);
	gamestate.bestweapon	= ReadInt32(fd);
	gamestate.weapon	= ReadInt32(fd);
	gamestate.chosenweapon	= ReadInt32(fd);
	gamestate.faceframe	= ReadInt32(fd);
	gamestate.attackframe	= ReadInt32(fd);
	gamestate.attackcount	= ReadInt32(fd);
	gamestate.weaponframe	= ReadInt32(fd);
	gamestate_episode	= ReadInt32(fd);
	gamestate.secretcount	= ReadInt32(fd);
	gamestate.treasurecount	= ReadInt32(fd);
	gamestate.killcount	= ReadInt32(fd);
	gamestate.secrettotal	= ReadInt32(fd);
	gamestate.treasuretotal = ReadInt32(fd);
	gamestate.killtotal	= ReadInt32(fd);
	gamestate.TimeCount	= ReadInt32(fd);
	gamestate.killx		= ReadInt32(fd);
	gamestate.killy		= ReadInt32(fd);
	gamestate.victoryflag	= ReadInt8(fd);
	
	DiskFlopAnim(dx, dy);
	
#ifdef SPEAR
	for (i = 0; i < 20; i++) {
#else
	for (i = 0; i < 8; i++) {
#endif
		LevelRatios[i].kill	= ReadInt32(fd);
		LevelRatios[i].secret	= ReadInt32(fd);
		LevelRatios[i].treasure	= ReadInt32(fd);
		LevelRatios[i].time	= ReadInt32(fd);
	}
	
	DiskFlopAnim(dx, dy);
	
	SetupGameLevel();
	
	DiskFlopAnim(dx, dy);
	
	ReadBytes(fd, (byte *)tilemap, 64*64); /* MAPSIZE * MAPSIZE */
	
	DiskFlopAnim(dx, dy);
	
	for (x = 0; x < 64; x++)
		for (y = 0; y < 64; y++)
			actorat[x][y] = ReadInt32(fd);
	
	DiskFlopAnim(dx, dy);
			
	//FIXME: This is not endian safe.  */
	ReadBytes(fd, (byte *)&areabyplayer, 8);
	
	DiskFlopAnim(dx, dy);
	
	InitActorList();
	
	DiskFlopAnim(dx, dy);
	
	/* player ptr already set up */
	id			= ReadInt32(fd); /* get id */
	player->active		= ReadInt32(fd);
	player->ticcount	= ReadInt32(fd);
	player->obclass		= ReadInt32(fd);
	player->state		= ReadInt32(fd);
	player->flags		= ReadInt8(fd);
	player->distance	= ReadInt32(fd);
	player->dir		= ReadInt32(fd);
	player->x		= ReadInt32(fd);
	player->y		= ReadInt32(fd);
	player->tilex		= ReadInt32(fd);
	player->tiley		= ReadInt32(fd);
	player->areanumber	= ReadInt8(fd);
	player->viewx		= ReadInt32(fd);
	player->viewheight	= ReadInt32(fd);
	player->transx		= ReadInt32(fd);
	player->transy		= ReadInt32(fd);
	player->angle		= ReadInt32(fd);
	player->hitpoints	= ReadInt32(fd);
	player->speed		= ReadInt32(fd);
	player->temp1		= ReadInt32(fd);
	player->temp2		= ReadInt32(fd);
	//player->temp3		= ReadInt32(fd);
	
	/* update the id */
	for (x = 0; x < 64; x++)
		for (y = 0; y < 64; y++)
			if (actorat[x][y] == (id | 0x8000))
				actorat[x][y] = obj_id(player) | 0x8000;

	while (1) {
		DiskFlopAnim(dx, dy);
		
		id			= ReadInt32(fd);
		
		if (id == 0xFFFFFFFF)
			break;
		
		GetNewActor();
		
		new->active		= ReadInt32(fd);
		new->ticcount		= ReadInt32(fd);
		new->obclass		= ReadInt32(fd);
		new->state		= ReadInt32(fd);
		new->flags		= ReadInt8(fd);
		new->distance		= ReadInt32(fd);
		new->dir		= ReadInt32(fd);
		new->x			= ReadInt32(fd);
		new->y			= ReadInt32(fd);
		new->tilex		= ReadInt32(fd);
		new->tiley		= ReadInt32(fd);
		new->areanumber		= ReadInt8(fd);
		new->viewx		= ReadInt32(fd);
		new->viewheight		= ReadInt32(fd);
		new->transx		= ReadInt32(fd);
		new->transy		= ReadInt32(fd);
		new->angle		= ReadInt32(fd);
		new->hitpoints		= ReadInt32(fd);
		new->speed		= ReadInt32(fd);
		new->temp1		= ReadInt32(fd);
		new->temp2		= ReadInt32(fd);
		//new->temp3		= ReadInt32(fd);
		
		for (x = 0; x < 64; x++)
			for (y = 0; y < 64; y++)
				if (actorat[x][y] == (id | 0x8000))
					actorat[x][y] = obj_id(new) | 0x8000;
	}
	
	DiskFlopAnim(dx, dy);
	
	laststatobj = statobjlist + ReadInt32(fd); /* ptr offset */
	for (i = 0; i < 400; i++) { /* MAXSTATS */
		statobjlist[i].tilex		= ReadInt8(fd);
		statobjlist[i].tiley		= ReadInt8(fd);
		statobjlist[i].shapenum		= ReadInt32(fd);
		statobjlist[i].is_bonus		= ReadInt8(fd);
		statobjlist[i].itemnumber	= ReadInt8(fd);
	}
	
	DiskFlopAnim(dx, dy);
	
	for (i = 0; i < 64; i++) { /* MAXDOORS */
		doorposition[i] 		= ReadInt32(fd);
	}
	
	DiskFlopAnim(dx, dy);
	
	for (i = 0; i < 64; i++) { /* MAXDOORS */
		doorobjlist[i].tilex	= ReadInt8(fd);
		doorobjlist[i].tiley	= ReadInt8(fd);
		doorobjlist[i].vertical = ReadInt8(fd);
		doorobjlist[i].lock	= ReadInt8(fd);
		doorobjlist[i].action	= ReadInt8(fd);
		doorobjlist[i].ticcount	= ReadInt32(fd);
	}
	
	DiskFlopAnim(dx, dy);
	
	pwallstate 	= ReadInt32(fd);
	pwallx		= ReadInt32(fd);
	pwally		= ReadInt32(fd);
	pwalldir	= ReadInt32(fd);
	pwallpos	= ReadInt32(fd);

	DiskFlopAnim(dx, dy);
	
	CloseRead(fd);	
	
	return 0;
	
loadfail:
	if (fd != -1)
		CloseRead(fd);
		
	Message(STR_SAVECHT1"\n"
		STR_SAVECHT2"\n"
		STR_SAVECHT3"\n"
		STR_SAVECHT4);
			
	IN_ClearKeysDown();
	IN_Ack();
	
	NewGame(1, 0);
	return -1;
}
#endif

/* ======================================================================== */

#if !defined(EMBEDDED) || defined(ENABLE_DEMO)
/*
=================
=
= MS_CheckParm
=
=================
*/

myint MS_CheckParm(const char *check)
{
	myint i;
	char *parm;

	for (i = 1; i < _argc; i++) {
		parm = _argv[i];

		while (!isalpha(*parm))		// skip - / \ etc.. in front of parm
			if (!*parm++)
				break;		// hit end of string without an alphanum

		if (!stricmp(check, parm))
			return i;
	}
	return 0;
}
#endif

/* ======================================================================== */

/*
==================
=
= BuildTables
=
==================
*/

#ifndef EMBEDDED
void ShowViewSize(myint width)
{
	myint oldwidth,oldheight;

	oldwidth = viewwidthwin;
	oldheight = viewheightwin;

	viewwidthwin = width*16;
	viewheightwin = width_to_height(width*16);
	DrawPlayBorder();

	viewheightwin = oldheight;
	viewwidthwin = oldwidth;
}
#endif

#ifndef EMBEDDED
void NewViewSize(myint width)
{
	if (width > (MAXVIEWWIDTH / 16))
		width = MAXVIEWWIDTH / 16;
	if (width < 4)
		width = 4;	
	
	width *= vwidth / 320;
	
	if ((width*16) > vwidth)
		width = vwidth / 16;
	
	if (width_to_height(width*16) > (vheight - 40*vheight/200))
		width = (vheight - 40*vheight/200)/8;
	
	viewwidthwin = width*16*320/vwidth;
	viewheightwin = width_to_height(width*16)*320/vwidth;
	viewsize = width*320/vwidth;
	
	viewwidth = width*16;
	sviewheight = width_to_height(width*16);
	
	centerx = viewwidth/2-1;
	shootdelta = viewwidth/10;
	
	yoffset = (vheight-STATUSLINES*vheight/200-sviewheight)/2;
	xoffset = (vwidth-viewwidth)/2;
	vpitch = vwidth;
//
// calculate trace angles and projection constants
//
	CalcProjection(FOCALLENGTH);

}
#endif

//===========================================================================

#ifdef ENABLE_AUDIO
#ifndef SPEARDEMO

#ifndef SPEAR
CP_iteminfo MusicItems={CTL_X,CTL_Y,6,0,32};
CP_itemtype MusicMenu[]=
{
	{1,"Get Them!",0},
	{1,"Searching",0},
	{1,"P.O.W.",0},
	{1,"Suspense",0},
	{1,"War March",0},
	{1,"Around The Corner!",0},

	{1,"Nazi Anthem",0},
	{1,"Lurking...",0},
	{1,"Going After Hitler",0},
	{1,"Pounding Headache",0},
	{1,"Into the Dungeons",0},
	{1,"Ultimate Conquest",0},

	{1,"Kill the S.O.B.",0},
	{1,"The Nazi Rap",0},
	{1,"Twelfth Hour",0},
	{1,"Zero Hour",0},
	{1,"Ultimate Conquest",0},
	{1,"Wolfpack",0}
};
#else
CP_iteminfo MusicItems={CTL_X,CTL_Y-20,9,0,32};
CP_itemtype MusicMenu[]=
{
	{1,"Funky Colonel Bill",0},
	{1,"Death To The Nazis",0},
	{1,"Tiptoeing Around",0},
	{1,"Is This THE END?",0},
	{1,"Evil Incarnate",0},
	{1,"Jazzin' Them Nazis",0},
	{1,"Puttin' It To The Enemy",0},
	{1,"The SS Gonna Get You",0},
	{1,"Towering Above",0}
};
#endif

static const myint songs[] =
{
#ifndef SPEAR
	GETTHEM_MUS,
	SEARCHN_MUS,
	POW_MUS,
	SUSPENSE_MUS,
	WARMARCH_MUS,
	CORNER_MUS,

	NAZI_OMI_MUS,
	PREGNANT_MUS,
	GOINGAFT_MUS,
	HEADACHE_MUS,
	DUNGEON_MUS,
	ULTIMATE_MUS,

	INTROCW3_MUS,
	NAZI_RAP_MUS,
	TWELFTH_MUS,
	ZEROHOUR_MUS,
	ULTIMATE_MUS,
	PACMAN_MUS
#else
	XFUNKIE_MUS,             // 0
	XDEATH_MUS,              // 2
	XTIPTOE_MUS,             // 4
	XTHEEND_MUS,             // 7
	XEVIL_MUS,               // 17
	XJAZNAZI_MUS,            // 18
	XPUTIT_MUS,              // 21
	XGETYOU_MUS,             // 22
	XTOWER2_MUS              // 23
#endif
};
		
void DoJukebox()
{
	myint which,lastsong=-1;
	unsigned start;

	IN_ClearKeysDown();
	if (!AdLibPresent && !SoundBlasterPresent)
		return;
	
	MenuFadeOut();

#if !defined(SPEAR) || !defined(UPLOAD)
	start = (US_RndT() % 3) * 6;
#else
	start = 0;
#endif

	CA_CacheGrChunk(STARTFONT+1);
#ifdef SPEAR
	CacheLump(BACKDROP_LUMP_START, BACKDROP_LUMP_END);
#else
	CacheLump(CONTROLS_LUMP_START, CONTROLS_LUMP_END);
#endif
	CA_LoadAllSounds();

	fontnumber=1;
	ClearMScreen();
	VWB_DrawPic(112,184,C_MOUSELBACKPIC);
	DrawStripes(10);
	SETFONTCOLOR (TEXTCOLOR,BKGDCOLOR);

#ifndef SPEAR
	DrawWindow (CTL_X-2,CTL_Y-6,280,13*7,BKGDCOLOR);
#else
	DrawWindow (CTL_X-2,CTL_Y-26,280,13*10,BKGDCOLOR);
#endif

	DrawMenu (&MusicItems,&MusicMenu[start]);

	SETFONTCOLOR (READHCOLOR,BKGDCOLOR);
	PrintY = 15;
	WindowX = 0;
	WindowY = 320;
	US_CPrint("Robert's Jukebox");

	SETFONTCOLOR (TEXTCOLOR,BKGDCOLOR);
	VW_UpdateScreen();
	MenuFadeIn();

	do
	{
		which = HandleMenu(&MusicItems,&MusicMenu[start],NULL);
		if (which>=0)
		{
			if (lastsong >= 0)
				MusicMenu[start+lastsong].active = 1;

			StartCPMusic(songs[start + which]);
			MusicMenu[start+which].active = 2;
			DrawMenu (&MusicItems,&MusicMenu[start]);
			VW_UpdateScreen();
			lastsong = which;
		}
	} while(which>=0);

	MenuFadeOut();
	IN_ClearKeysDown();
#ifdef SPEAR
	UnCacheLump(BACKDROP_LUMP_START, BACKDROP_LUMP_END);
#else
	UnCacheLump(CONTROLS_LUMP_START, CONTROLS_LUMP_END);
#endif
}
#endif
#endif

/* ======================================================================== */

#ifndef EMBEDDED
/*
==========================
=
= ShutdownId
=
= Shuts down all ID_?? managers
=
==========================
*/

void ShutdownId()
{
	US_Shutdown();
#ifdef ENABLE_AUDIO
	SD_Shutdown();
#endif
	IN_Shutdown();
	VW_Shutdown();
	CA_Shutdown();
#ifdef ENABLE_COLOR
	PM_Shutdown();
#endif
	MM_Shutdown();
}
#endif

/*
=====================
=
= NewGame
=
= Set up new game to start from the beginning
=
=====================
*/

void NewGame(myint difficulty, myint episode)
{
	memset(&gamestate, 0, sizeof(gamestate));
	
#ifndef EMBEDDED
	gamestate_difficulty = difficulty;
	gamestate_episode = episode;
#endif
	gamestate.weapon = gamestate.bestweapon
		= gamestate.chosenweapon = wp_pistol;
	gamestate.health = 100;
	gamestate.ammo = STARTAMMO;
	gamestate.lives = 3;
	gamestate.nextextra = EXTRAPOINTS;

	startgame = true;
}

/*
==========================
=
= InitGame
=
= Load a few things right away
=
==========================
*/

void InitGame()
{
#ifndef EMBEDDED
	myint i;
#endif

	MM_Startup(); 
#ifdef ENABLE_COLOR
	PM_Startup();
#endif
	CA_Startup();
	VW_Startup();
	IN_Startup();
#ifdef ENABLE_AUDIO
	SD_Startup();
#endif
	US_Startup();
	
//
// build some tables
//

	ReadConfig();

#ifndef EMBEDDED
/* load in and lock down some basic chunks */

	CA_CacheGrChunk(STARTFONT);
	CA_CacheGrChunk(STARTTILE8);
	for (i = LATCHPICS_LUMP_START; i <= LATCHPICS_LUMP_END; i++)
		CA_CacheGrChunk(i);
#endif
			
#ifndef EMBEDDED
	NewViewSize(viewsize);
#endif


//
// initialize variables
//
	InitRedShifts();

#ifndef EMBEDDED
	IN_CheckAck();
#endif
//
// HOLDING DOWN 'M' KEY?
//
#ifdef ENABLE_AUDIO
#ifndef SPEARDEMO
	if (IN_KeyDown(sc_M))
		DoJukebox();
#endif
#endif
}

/*
=====================
=
= DemoLoop
=
=====================
*/

void DemoLoop()
{
#ifdef ENABLE_DEMO
	static myint LastDemo;
	
	myint i;
//
// main game cycle
//

	LastDemo = 0;
	
	StartCPMusic(INTROSONG);

	i = MS_CheckParm("playdemo");
	if (i && ((i+1) < _argc)) {
		i++;
		for (; i < _argc; i++) {
			if (_argv[i][0] == '-')
				break;
			IN_ClearKeysDown();
			if (PlayDemoFromFile(_argv[i]))
				IN_UserInput(3 * 70);
		}
		VW_FadeOut();
	}
	
	if (MS_CheckParm("demotest")) {
	#ifndef SPEARDEMO
		for (i = 0; i < 4; i++)
			PlayDemo(i);
	#else
		PlayDemo(0);
	#endif
	}
#endif
	
	while (1)
	{
		VW_FadeOut();

#if 0
		if (IN_KeyDown(sc_Tab) && MS_CheckParm("debugmode"))
			RecordDemo();
		else
			US_ControlPanel(0);
#else
		NewGame(gd_medium, 0);
		startgame = 1;
#endif

		if (startgame || loadedgame)
		{
			GameLoop();
			VW_FadeOut();
			StartCPMusic(INTROSONG);
		}
	}
}


/* ======================================================================== */


/*
==========================
=
= WolfMain
=
==========================
*/

myint WolfMain(myint argc, char *argv[])
{
#ifndef EMBEDDED
	_argc = argc;
	_argv = argv;
#endif

	if (SPR_TOTAL >= 512) {
	    /* statstruct:shapenum is a 10-bit signed field.  */
	    Quit("Too many sprites\n");
	}

#ifndef EMBEDDED
	if (MS_CheckParm("version")) {
		printf("Game: %s\n", GAMENAME);
		Quit(NULL);
	}
		
	printf("Now Loading %s\n", GAMENAME);
		
	CheckForEpisodes();
#endif

	InitGame();

	DemoLoop();

	Quit("Demo loop exited???");
	
	return 0;
}
