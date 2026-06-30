#include "wl_def.h"

/*
=============================================================================

							STATICS

=============================================================================
*/


statobj_t statobjlist[MAXSTATS], *laststatobj;

struct
{
	myshort	picnum:10;
	stat_t	type:6;
} static const statinfo[] =
{
{SPR_STAT_0},					// puddle          spr1v
{SPR_STAT_1,block},				// Green Barrel    "
{SPR_STAT_2,block},				// Table/chairs    "
{SPR_STAT_3,block},				// Floor lamp      "
{SPR_STAT_4},					// Chandelier      "
{SPR_STAT_5,block},				// Hanged man      "
{SPR_STAT_6,bo_alpo},			// Bad food        "
{SPR_STAT_7,block},				// Red pillar      "
{SPR_STAT_8,block},				// Tree            spr2v
{SPR_STAT_9},					// Skeleton flat   "
{SPR_STAT_10,block},			// Sink            " (SOD:gibs)
{SPR_STAT_11,block},			// Potted plant    "
{SPR_STAT_12,block},			// Urn             "
{SPR_STAT_13,block},			// Bare table      "
{SPR_STAT_14},					// Ceiling light   "

#ifndef SPEAR
{SPR_STAT_15},					// Kitchen stuff   "
#else
{SPR_STAT_15,block},			// Gibs!
#endif

{SPR_STAT_16,block},			// suit of armor   spr3v
{SPR_STAT_17,block},			// Hanging cage    "
{SPR_STAT_18,block},			// SkeletoninCage  "
{SPR_STAT_19},					// Skeleton relax  "
{SPR_STAT_20,bo_key1},			// Key 1           "
{SPR_STAT_21,bo_key2},			// Key 2           "
{SPR_STAT_22,block},			// stuff				(SOD:gibs)
{SPR_STAT_23},					// stuff
{SPR_STAT_24,bo_food}, 			// Good food       spr4v
{SPR_STAT_25,bo_firstaid},		// First aid       "
{SPR_STAT_26,bo_clip},			// Clip            "
{SPR_STAT_27,bo_machinegun},	// Machine gun     "
{SPR_STAT_28,bo_chaingun},		// Gatling gun     "
{SPR_STAT_29,bo_cross},			// Cross           "
{SPR_STAT_30,bo_chalice},		// Chalice         "
{SPR_STAT_31,bo_bible},			// Bible           "
{SPR_STAT_32,bo_crown},			// crown           spr5v
{SPR_STAT_33,bo_fullheal},		// one up          "
{SPR_STAT_34,bo_gibs},			// gibs            "
{SPR_STAT_35,block},			// barrel          "
{SPR_STAT_36,block},			// well            "
{SPR_STAT_37,block},			// Empty well      "
{SPR_STAT_38,bo_gibs},			// Gibs 2          "
{SPR_STAT_39,block},			// flag				"

#ifndef SPEAR
{SPR_STAT_40,block},			// Call Apogee		spr7v
#else
{SPR_STAT_40},					// Red light
#endif

{SPR_STAT_41},					// junk            "
{SPR_STAT_42},					// junk 		   "
{SPR_STAT_43},					// junk            "

#ifndef SPEAR
{SPR_STAT_44},					// pots            "
#else
{SPR_STAT_44,block},			// Gibs!
#endif

{SPR_STAT_45,block},			// stove           " (SOD:gibs)
{SPR_STAT_46,block},			// spears          " (SOD:gibs)
{SPR_STAT_47},					// vines			"

#ifdef SPEAR
{SPR_STAT_48,block},			// marble pillar
{SPR_STAT_49,bo_25clip},		// bonus 25 clip
{SPR_STAT_50,block},			// truck
{SPR_STAT_51,bo_spear},			// SPEAR OF DESTINY!
#endif

{SPR_STAT_26,bo_clip2},			// Clip            "
{-1}							// terminator
};

/*
===============
=
= InitStaticList
=
===============
*/

void InitStaticList()
{
	laststatobj = &statobjlist[0];
}

/*
===============
=
= SpawnStatic
=
===============
*/

void SpawnStatic(myint tilex, myint tiley, myint type)
{
	laststatobj->shapenum = statinfo[type].picnum;
	laststatobj->tilex = tilex;
	laststatobj->tiley = tiley;

	switch (statinfo[type].type)
	{
	case block:
		set_wall_at(tilex, tiley, 1);	// consider it a blocking tile
	case dressing:
		laststatobj->is_bonus = 0;
		break;

	case	bo_cross:
	case	bo_chalice:
	case	bo_bible:
	case	bo_crown:
	case	bo_fullheal:
#ifdef ENABLE_STATS
		if (!loadedgame)
			gamestate.treasuretotal++;
#endif

	case	bo_firstaid:
	case	bo_key1:
	case	bo_key2:
	case	bo_key3:
	case	bo_key4:
	case	bo_clip:
	case	bo_25clip:
	case	bo_machinegun:
	case	bo_chaingun:
	case	bo_food:
	case	bo_alpo:
	case	bo_gibs:
	case	bo_spear:
		laststatobj->is_bonus = true;
		laststatobj->itemnumber = statinfo[type].type;
		break;
	default: 
		break;
	}

	laststatobj++;

	if (laststatobj == &statobjlist[MAXSTATS])
		Quit ("Too many static objects!\n");
}

/*
===============
=
= PlaceItemType
=
= Called during game play to drop actors' items.  It finds the proper
= item number based on the item type (bo_???).  If there are no free item
= spots, nothing is done.
=
===============
*/

void PlaceItemType(myint itemtype, myint tilex, myint tiley)
{
	myint type;
	statobj_t *spot;

//
// find the item number
//
	for (type = 0; ; type++)
	{
		if (statinfo[type].picnum == -1) /* end of list */
			Quit("PlaceItemType: couldn't find type!");
		if (statinfo[type].type == itemtype)
			break;
	}

//
// find a spot in statobjlist to put it in
//
	for (spot=&statobjlist[0] ; ; spot++)
	{
		if (spot==laststatobj)
		{
			if (spot == &statobjlist[MAXSTATS])
				return;	/* no free spots */
			laststatobj++;	/* space at end */
			break;
		}

		if (spot->shapenum == -1) /* -1 is a free spot */
			break;
	}
//
// place it
//
	spot->shapenum = statinfo[type].picnum;
	spot->tilex = tilex;
	spot->tiley = tiley;
	spot->is_bonus = 1;
	spot->itemnumber = statinfo[type].type;
}

/*
=============================================================================

							DOORS

doorobjlist[] holds most of the information for the doors

doorposition[] holds the amount the door is open, ranging from 0 to 0xffff
	this is directly accessed by AsmRefresh during rendering

The number of doors is limited to 64 because a spot in tilemap holds the
	door number in the low 6 bits, with the high bit meaning a door center
	and bit 6 meaning a door side tile

Open doors conect two areas, so sounds will travel between them and sight
	will be checked when the player is in a connected area.

Areaconnect is incremented/decremented by each door. If >0 they connect

Every time a door opens or closes the areabyplayer matrix gets recalculated.
	An area is true if it connects with the player's current spot.

=============================================================================
*/

/* Must fit in a 10-bit field.  */
#define OPENTICS	300

doorobj_t	doorobjlist[MAXDOORS],*lastdoorobj;
myint			doornum;

umyshort	doorposition[MAXDOORS];	// leading edge of door 0=closed
					// 0xffff = fully open
uint64_t	areabyplayer;


/*
==============
=
= ConnectAreas
=
= Scans outward from playerarea, marking all connected areas
=
==============
*/

void ConnectAreas(boolean reset)
{
    int door;
    boolean again;
    int area1, area2;
    boolean by1, by2;
    ms0 *map;

    if (reset) {
	areabyplayer = 0;
	setareabyplayer(player->areanumber);
    }
    do {
	again = false;
	for (door = 0; door < doornum; door++) {
	    if (doorposition[door] == 0)
		continue;
	    map = (mapseg0 + farmapylookup(doorobjlist[door].tiley)
		    +doorobjlist[door].tilex);

	    if (doorobjlist[door].vertical) {
		area1 =	*(map+1);
		area2 =	*(map-1);
	    } else {
		area1 =	*(map-mapwidth);
		area2 =	*(map+mapwidth);
	    }
	    area1 -= AREATILE;
	    area2 -= AREATILE;
	    by1 = getareabyplayer(area1);
	    by2 = getareabyplayer(area2);
	    if (by1 ^ by2) {
		again = true;
		if (!by1)
		    setareabyplayer(area1);
		if (!by2)
		    setareabyplayer(area2);
	    }
	}
    } while (again);
}

void InitAreas()
{
	areabyplayer = 0;
	setareabyplayer(player->areanumber);
}

/*
===============
=
= InitDoorList
=
===============
*/

void InitDoorList()
{
	areabyplayer = 0;

	lastdoorobj = &doorobjlist[0];
	doornum = 0;
}

/*
===============
=
= SpawnDoor
=
===============
*/

void SpawnDoor(myint tilex, myint tiley, boolean vertical, myint lock)
{
	ms0 *map;

	if (doornum == 64)
		Quit ("64+ doors on level!");

	doorposition[doornum] = 0;		// doors start out fully closed
	lastdoorobj->tilex = tilex;
	lastdoorobj->tiley = tiley;
	lastdoorobj->vertical = vertical;
	lastdoorobj->lock = lock;
	lastdoorobj->action = dr_closed;

	set_door_actor(tilex, tiley, doornum);	// consider it a solid wall

//
// make the door tile a special tile, and mark the adjacent tiles
// for door sides
//
	tilemap[tilex][tiley] = doornum | 0x80;
	map = (mapseg0 + farmapylookup(tiley)+tilex);
	if (vertical)
	{
		*map = *(map-1);                        // set area number
		tilemap[tilex][tiley-1] |= 0x40;
		tilemap[tilex][tiley+1] |= 0x40;
	}
	else
	{
		*map = *(map-mapwidth);					// set area number
		tilemap[tilex-1][tiley] |= 0x40;
		tilemap[tilex+1][tiley] |= 0x40;
	}

	doornum++;
	lastdoorobj++;
}

/*
=====================
=
= OpenDoor
=
=====================
*/

void OpenDoor(myint door)
{
	if (doorobjlist[door].action == dr_open)
		doorobjlist[door].ticcount = 0;			// reset open time
	else
		doorobjlist[door].action = dr_opening;	// start it opening
}

/*
=====================
=
= CloseDoor
=
=====================
*/

void CloseDoor(myint door)
{
	myint tilex, tiley, area;
	objtype *check;

//
// don't close on anything solid
//
	tilex = doorobjlist[door].tilex;
	tiley = doorobjlist[door].tiley;

	if (any_actor_at(tilex, tiley))
		return;

	if (player->tilex == tilex && player->tiley == tiley)
		return;

	if (doorobjlist[door].vertical)
	{
		if (player->tiley == tiley)
		{
			if (((player->x+MINDIST) >>TILESHIFT) == tilex)
				return;
			if (((player->x-MINDIST) >>TILESHIFT) == tilex)
				return;
		}

		if (obj_actor_at(tilex-1, tiley))
			check = &objlist[get_actor_at(tilex-1, tiley)];
		else
			check = NULL;

		if (check && ((check->x+MINDIST) >> TILESHIFT) == tilex)
			return;
		
		if (obj_actor_at(tilex+1, tiley))
			check = &objlist[get_actor_at(tilex+1, tiley)];
		else
			check = NULL;

		if (check && ((check->x-MINDIST) >> TILESHIFT) == tilex)
			return;
	}
	else if (!doorobjlist[door].vertical)
	{
		if (player->tilex == tilex)
		{
			if ( ((player->y+MINDIST) >>TILESHIFT) == tiley )
				return;
			if ( ((player->y-MINDIST) >>TILESHIFT) == tiley )
				return;
		}
		
		if (obj_actor_at(tilex, tiley-1))
			check = &objlist[get_actor_at(tilex, tiley-1)];
		else
			check = NULL;

		if (check && ((check->y+MINDIST) >> TILESHIFT) == tiley )
			return;
		
		if (obj_actor_at(tilex, tiley+1))
			check = &objlist[get_actor_at(tilex, tiley+1)];
		else
			check = NULL;
		
		if (check && ((check->y-MINDIST) >> TILESHIFT) == tiley )
			return;
	}


//
// play door sound if in a connected area
//
	area = *(mapseg0 + farmapylookup(doorobjlist[door].tiley)
			+doorobjlist[door].tilex)-AREATILE;
	if (getareabyplayer(area))
	{
		PlaySoundLocTile(CLOSEDOORSND,doorobjlist[door].tilex,doorobjlist[door].tiley);	// JAB
	}

	doorobjlist[door].action = dr_closing;
//
// make the door space solid
//
	set_door_actor(tilex, tiley, door);
}

/*
=====================
=
= OperateDoor
=
= The player wants to change the door's direction
=
=====================
*/

void OperateDoor(myint door)
{
	myint	lock;

	lock = doorobjlist[door].lock;
	if (lock >= dr_lock1 && lock <= dr_lock4)
	{
		if ( ! (gamestate.keys & (1 << (lock-dr_lock1) ) ) )
		{
			SD_PlaySound (NOWAYSND);		// locked
			return;
		}
	}

	switch (doorobjlist[door].action)
	{
	case dr_closed:
	case dr_closing:
		OpenDoor (door);
		break;
	case dr_open:
	case dr_opening:
		CloseDoor (door);
		break;
	}
}

/*
===============
=
= DoorOpen
=
= Close the door after three seconds
=
===============
*/

void DoorOpen (myint door)
{
	if ( (doorobjlist[door].ticcount += tics) >= OPENTICS)
		CloseDoor (door);
}



/*
===============
=
= DoorOpening
=
===============
*/

void DoorOpening(myint door)
{
	long	position;

	position = doorposition[door];
	if (!position)
	{
	//
	// door is just starting to open, so connect the areas
	//
		// Open the door slightly.
		doorposition[door] = 1;
		ConnectAreas (false);
#ifdef ENABLE_AUDIO
		if (getareabyplayer(area1))
		{
			PlaySoundLocTile(OPENDOORSND,doorobjlist[door].tilex,doorobjlist[door].tiley);	// JAB
		}
#endif
	}

//
// slide the door by an adaptive amount
//
	position += tics<<10;
	if (position >= 0xffff)
	{
	//
	// door is all the way open
	//
		position = 0xffff;
		doorobjlist[door].ticcount = 0;
		doorobjlist[door].action = dr_open;
		clear_actor(doorobjlist[door].tilex, doorobjlist[door].tiley);
	}

	doorposition[door] = position;
}


/*
===============
=
= DoorClosing
=
===============
*/

void DoorClosing(myint door)
{
	long	position;
	myint		tilex,tiley;

	tilex = doorobjlist[door].tilex;
	tiley = doorobjlist[door].tiley;

	if (obj_actor_at(tilex, tiley)
	|| (player->tilex == tilex && player->tiley == tiley) )
	{			// something got inside the door
		OpenDoor(door);
		return;
	};

	position = doorposition[door];

//
// slide the door by an adaptive amount
//
	position -= tics<<10;
	if (position <= 0)
	{
	//
	// door is closed all the way, so disconnect the areas
	//
		doorposition[door] = 0;

		doorobjlist[door].action = dr_closed;

		ConnectAreas (true);
	} else {
		doorposition[door] = position;
	}
}

/*
=====================
=
= MoveDoors
=
= Called from PlayLoop
=
=====================
*/

void MoveDoors()
{
	myint door;

	if (gamestate.victoryflag)		// don't move door during victory sequence
		return;

	for (door = 0 ; door < doornum ; door++)
		switch(doorobjlist[door].action)
		{
		case dr_open:
			DoorOpen (door);
			break;

		case dr_opening:
			DoorOpening(door);
			break;

		case dr_closing:
			DoorClosing(door);
			break;
		default:
			break;
		}
}


/*
=============================================================================

						PUSHABLE WALLS

=============================================================================
*/

umyshort pwallstate;
umyshort pwallpos; // amount a pushable wall has been moved (0-63)
umyshort pwallx, pwally;
myint pwalldir;

/*
===============
=
= PushWall
=
===============
*/

void PushWall(myint checkx, myint checky, myint dir)
{
	myint oldtile;

	if (pwallstate)
		return;

	oldtile = tilemap[checkx][checky];
	if (!oldtile)
		return;

	switch (dir)
	{
	case di_north:
		if (any_actor_at(checkx, checky-1))
		{
			SD_PlaySound(NOWAYSND);
			return;
		}
		actorat[checkx][checky-1] = tilemap[checkx][checky-1] = oldtile;
		break;

	case di_east:
		if (any_actor_at(checkx+1, checky))
		{
			SD_PlaySound(NOWAYSND);
			return;
		}
		actorat[checkx+1][checky] = tilemap[checkx+1][checky] = oldtile;
		break;

	case di_south:
		if (any_actor_at(checkx, checky+1))
		{
			SD_PlaySound(NOWAYSND);
			return;
		}
		actorat[checkx][checky+1] = tilemap[checkx][checky+1] = oldtile;
		break;

	case di_west:
		if (any_actor_at(checkx-1, checky))
		{
			SD_PlaySound(NOWAYSND);
			return;
		}
		actorat[checkx-1][checky] = tilemap[checkx-1][checky] = oldtile;
		break;
	}

#ifdef ENABLE_STATS
	gamestate.secretcount++;
#endif
	pwallx = checkx;
	pwally = checky;
	pwalldir = dir;
	pwallstate = 1;
	pwallpos = 0;
	tilemap[pwallx][pwally] |= 0xc0;
	setmapspecial(pwallx, pwally, 0);

	SD_PlaySound(PUSHWALLSND);
}



/*
=================
=
= MovePWalls
=
=================
*/

void MovePWalls()
{
	myint oldblock, oldtile;

	if (!pwallstate)
		return;

	oldblock = pwallstate/128;

	pwallstate += tics;

	if (pwallstate/128 != oldblock)
	{
	// block crossed into a new block
		oldtile = tilemap[pwallx][pwally] & 63;

		//
		// the tile can now be walked into
		//
		tilemap[pwallx][pwally] = 0;
		clear_actor(pwallx, pwally);
		*(mapseg0+farmapylookup(pwally)+pwallx) = player->areanumber+AREATILE;

		//
		// see if it should be pushed farther
		//
		if (pwallstate>256)
		{
		//
		// the block has been pushed two tiles
		//
			pwallstate = 0;
			return;
		}
		else
		{
			switch (pwalldir)
			{
			case di_north:
				pwally--;
				if (any_actor_at(pwallx, pwally-1))
				{
					pwallstate = 0;
					return;
				}
				actorat[pwallx][pwally-1] =
				tilemap[pwallx][pwally-1] = oldtile;
				break;

			case di_east:
				pwallx++;
				if (any_actor_at(pwallx+1, pwally))
				{
					pwallstate = 0;
					return;
				}
				actorat[pwallx+1][pwally] =
				tilemap[pwallx+1][pwally] = oldtile;
				break;

			case di_south:
				pwally++;
				if (any_actor_at(pwallx, pwally+1))
				{
					pwallstate = 0;
					return;
				}
				actorat[pwallx][pwally+1] =
				tilemap[pwallx][pwally+1] = oldtile;
				break;

			case di_west:
				pwallx--;
				if (any_actor_at(pwallx-1, pwally))
				{
					pwallstate = 0;
					return;
				}
				actorat[pwallx-1][pwally] =
				tilemap[pwallx-1][pwally] = oldtile;
				break;
			}

			tilemap[pwallx][pwally] = oldtile | 0xc0;
		}
	}

	pwallpos = (pwallstate/2)&63;

}
