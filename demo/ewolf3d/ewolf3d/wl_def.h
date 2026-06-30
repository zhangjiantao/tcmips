#ifndef __WL_DEF_H__
#define __WL_DEF_H__

#include "id_heads.h"

#include "wl_menu.h"
#include "foreign.h"

#ifdef EMBEDDED
#define vwidth 128
#define vpitch 64
#define vheight 64
#else
extern myint vwidth, vheight, vpitch; /* size of screen */
#endif

#define NOASM

/*
=============================================================================

						 GLOBAL CONSTANTS

=============================================================================
*/

#define MAXACTORS		150		// max number of nazis, etc / map
#define MAXSTATS		400		// max number of lamps, bonus, etc
#define MAXDOORS		64		// max number of sliding doors
#define MAXWALLTILES		64		// max number of wall tiles
#define WALLCBLOCK		16		// Number of compression blocks
#define WALLBLOCKSIZE		((64 * 64) / WALLCBLOCK)

//
// tile constants
//

#define	ICONARROWS		90
#define PUSHABLETILE		98
#define EXITTILE		99		// at end of castle
#define AREATILE		107		// first of NUMAREAS floor tiles
#define NUMAREAS		37
#define ELEVATORTILE		21
#define AMBUSHTILE		106
#define	ALTELEVATORTILE		107

#define width_to_height(x)	(x >> 1)

//----------------

#define EXTRAPOINTS	40000

#define RUNSPEED   	6000

#define PLAYERSIZE	MINDIST		/* player radius */
#define MINACTORDIST	0x10000		/* minimum dist from player center */
					/* to any actor center */
#define TILESHIFT	16
#define UNSIGNEDSHIFT	(TILESHIFT-8)
#define GLOBAL1		(1<<TILESHIFT)
#define TILEGLOBAL  	GLOBAL1
#define VIEWGLOBAL	GLOBAL1

#define ANGLES		360		/* must be divisible by 4 */
#define ANGLEQUAD	(ANGLES/4)
#define FINEANGLES	3600

#define MINDIST		0x5800

//#define MAXVIEWWIDTH	1280
#define MAXVIEWWIDTH 128

#define MAPSIZE		64		/* maps are 64*64 */

#define STATUSLINES	40

#define STARTAMMO	8

// object flag values

#define FL_SHOOTABLE	1
#define FL_NEVERMARK	4
#define FL_VISABLE		8
#define FL_ATTACKMODE	16
#define FL_FIRSTATTACK	32
#define FL_AMBUSH		64
#define FL_NONMARK		128

//
// sprite constants
//

enum PACKED {
		SPR_DEMO,
		SPR_DEATHCAM,
//
// static sprites
//
		SPR_STAT_0,SPR_STAT_1,SPR_STAT_2,SPR_STAT_3,
		SPR_STAT_4,SPR_STAT_5,SPR_STAT_6,SPR_STAT_7,

		SPR_STAT_8,SPR_STAT_9,SPR_STAT_10,SPR_STAT_11,
		SPR_STAT_12,SPR_STAT_13,SPR_STAT_14,SPR_STAT_15,

		SPR_STAT_16,SPR_STAT_17,SPR_STAT_18,SPR_STAT_19,
		SPR_STAT_20,SPR_STAT_21,SPR_STAT_22,SPR_STAT_23,

		SPR_STAT_24,SPR_STAT_25,SPR_STAT_26,SPR_STAT_27,
		SPR_STAT_28,SPR_STAT_29,SPR_STAT_30,SPR_STAT_31,

		SPR_STAT_32,SPR_STAT_33,SPR_STAT_34,SPR_STAT_35,
		SPR_STAT_36,SPR_STAT_37,SPR_STAT_38,SPR_STAT_39,

		SPR_STAT_40,SPR_STAT_41,SPR_STAT_42,SPR_STAT_43,
		SPR_STAT_44,SPR_STAT_45,SPR_STAT_46,SPR_STAT_47,

#ifdef SPEAR
		SPR_STAT_48,SPR_STAT_49,SPR_STAT_50,SPR_STAT_51,
#endif

//
// guard
//
		SPR_GRD_S_1,SPR_GRD_S_2,SPR_GRD_S_3,SPR_GRD_S_4,
		SPR_GRD_S_5,SPR_GRD_S_6,SPR_GRD_S_7,SPR_GRD_S_8,

		SPR_GRD_W1_1,SPR_GRD_W1_2,SPR_GRD_W1_3,SPR_GRD_W1_4,
		SPR_GRD_W1_5,SPR_GRD_W1_6,SPR_GRD_W1_7,SPR_GRD_W1_8,

		SPR_GRD_W2_1,SPR_GRD_W2_2,SPR_GRD_W2_3,SPR_GRD_W2_4,
		SPR_GRD_W2_5,SPR_GRD_W2_6,SPR_GRD_W2_7,SPR_GRD_W2_8,

		SPR_GRD_W3_1,SPR_GRD_W3_2,SPR_GRD_W3_3,SPR_GRD_W3_4,
		SPR_GRD_W3_5,SPR_GRD_W3_6,SPR_GRD_W3_7,SPR_GRD_W3_8,

		SPR_GRD_W4_1,SPR_GRD_W4_2,SPR_GRD_W4_3,SPR_GRD_W4_4,
		SPR_GRD_W4_5,SPR_GRD_W4_6,SPR_GRD_W4_7,SPR_GRD_W4_8,

		SPR_GRD_PAIN_1,SPR_GRD_DIE_1,SPR_GRD_DIE_2,SPR_GRD_DIE_3,
		SPR_GRD_PAIN_2,SPR_GRD_DEAD,

		SPR_GRD_SHOOT1,SPR_GRD_SHOOT2,SPR_GRD_SHOOT3,

//
// dogs
//
		SPR_DOG_W1_1,SPR_DOG_W1_2,SPR_DOG_W1_3,SPR_DOG_W1_4,
		SPR_DOG_W1_5,SPR_DOG_W1_6,SPR_DOG_W1_7,SPR_DOG_W1_8,

		SPR_DOG_W2_1,SPR_DOG_W2_2,SPR_DOG_W2_3,SPR_DOG_W2_4,
		SPR_DOG_W2_5,SPR_DOG_W2_6,SPR_DOG_W2_7,SPR_DOG_W2_8,

		SPR_DOG_W3_1,SPR_DOG_W3_2,SPR_DOG_W3_3,SPR_DOG_W3_4,
		SPR_DOG_W3_5,SPR_DOG_W3_6,SPR_DOG_W3_7,SPR_DOG_W3_8,

		SPR_DOG_W4_1,SPR_DOG_W4_2,SPR_DOG_W4_3,SPR_DOG_W4_4,
		SPR_DOG_W4_5,SPR_DOG_W4_6,SPR_DOG_W4_7,SPR_DOG_W4_8,

		SPR_DOG_DIE_1,SPR_DOG_DIE_2,SPR_DOG_DIE_3,SPR_DOG_DEAD,
		SPR_DOG_JUMP1,SPR_DOG_JUMP2,SPR_DOG_JUMP3,



//
// ss
//
		SPR_SS_S_1,SPR_SS_S_2,SPR_SS_S_3,SPR_SS_S_4,
		SPR_SS_S_5,SPR_SS_S_6,SPR_SS_S_7,SPR_SS_S_8,

		SPR_SS_W1_1,SPR_SS_W1_2,SPR_SS_W1_3,SPR_SS_W1_4,
		SPR_SS_W1_5,SPR_SS_W1_6,SPR_SS_W1_7,SPR_SS_W1_8,

		SPR_SS_W2_1,SPR_SS_W2_2,SPR_SS_W2_3,SPR_SS_W2_4,
		SPR_SS_W2_5,SPR_SS_W2_6,SPR_SS_W2_7,SPR_SS_W2_8,

		SPR_SS_W3_1,SPR_SS_W3_2,SPR_SS_W3_3,SPR_SS_W3_4,
		SPR_SS_W3_5,SPR_SS_W3_6,SPR_SS_W3_7,SPR_SS_W3_8,

		SPR_SS_W4_1,SPR_SS_W4_2,SPR_SS_W4_3,SPR_SS_W4_4,
		SPR_SS_W4_5,SPR_SS_W4_6,SPR_SS_W4_7,SPR_SS_W4_8,

		SPR_SS_PAIN_1,SPR_SS_DIE_1,SPR_SS_DIE_2,SPR_SS_DIE_3,
		SPR_SS_PAIN_2,SPR_SS_DEAD,

		SPR_SS_SHOOT1,SPR_SS_SHOOT2,SPR_SS_SHOOT3,

//
// mutant
//
		SPR_MUT_S_1,SPR_MUT_S_2,SPR_MUT_S_3,SPR_MUT_S_4,
		SPR_MUT_S_5,SPR_MUT_S_6,SPR_MUT_S_7,SPR_MUT_S_8,

		SPR_MUT_W1_1,SPR_MUT_W1_2,SPR_MUT_W1_3,SPR_MUT_W1_4,
		SPR_MUT_W1_5,SPR_MUT_W1_6,SPR_MUT_W1_7,SPR_MUT_W1_8,

		SPR_MUT_W2_1,SPR_MUT_W2_2,SPR_MUT_W2_3,SPR_MUT_W2_4,
		SPR_MUT_W2_5,SPR_MUT_W2_6,SPR_MUT_W2_7,SPR_MUT_W2_8,

		SPR_MUT_W3_1,SPR_MUT_W3_2,SPR_MUT_W3_3,SPR_MUT_W3_4,
		SPR_MUT_W3_5,SPR_MUT_W3_6,SPR_MUT_W3_7,SPR_MUT_W3_8,

		SPR_MUT_W4_1,SPR_MUT_W4_2,SPR_MUT_W4_3,SPR_MUT_W4_4,
		SPR_MUT_W4_5,SPR_MUT_W4_6,SPR_MUT_W4_7,SPR_MUT_W4_8,

		SPR_MUT_PAIN_1,SPR_MUT_DIE_1,SPR_MUT_DIE_2,SPR_MUT_DIE_3,
		SPR_MUT_PAIN_2,SPR_MUT_DIE_4,SPR_MUT_DEAD,

		SPR_MUT_SHOOT1,SPR_MUT_SHOOT2,SPR_MUT_SHOOT3,SPR_MUT_SHOOT4,

//
// officer
//
		SPR_OFC_S_1,SPR_OFC_S_2,SPR_OFC_S_3,SPR_OFC_S_4,
		SPR_OFC_S_5,SPR_OFC_S_6,SPR_OFC_S_7,SPR_OFC_S_8,

		SPR_OFC_W1_1,SPR_OFC_W1_2,SPR_OFC_W1_3,SPR_OFC_W1_4,
		SPR_OFC_W1_5,SPR_OFC_W1_6,SPR_OFC_W1_7,SPR_OFC_W1_8,

		SPR_OFC_W2_1,SPR_OFC_W2_2,SPR_OFC_W2_3,SPR_OFC_W2_4,
		SPR_OFC_W2_5,SPR_OFC_W2_6,SPR_OFC_W2_7,SPR_OFC_W2_8,

		SPR_OFC_W3_1,SPR_OFC_W3_2,SPR_OFC_W3_3,SPR_OFC_W3_4,
		SPR_OFC_W3_5,SPR_OFC_W3_6,SPR_OFC_W3_7,SPR_OFC_W3_8,

		SPR_OFC_W4_1,SPR_OFC_W4_2,SPR_OFC_W4_3,SPR_OFC_W4_4,
		SPR_OFC_W4_5,SPR_OFC_W4_6,SPR_OFC_W4_7,SPR_OFC_W4_8,

		SPR_OFC_PAIN_1,SPR_OFC_DIE_1,SPR_OFC_DIE_2,SPR_OFC_DIE_3,
		SPR_OFC_PAIN_2,SPR_OFC_DIE_4,SPR_OFC_DEAD,

		SPR_OFC_SHOOT1,SPR_OFC_SHOOT2,SPR_OFC_SHOOT3,

#ifndef SPEAR
//
// ghosts
//
		SPR_BLINKY_W1,SPR_BLINKY_W2,SPR_PINKY_W1,SPR_PINKY_W2,
		SPR_CLYDE_W1,SPR_CLYDE_W2,SPR_INKY_W1,SPR_INKY_W2,

//
// hans
//
		SPR_BOSS_W1,SPR_BOSS_W2,SPR_BOSS_W3,SPR_BOSS_W4,
		SPR_BOSS_SHOOT1,SPR_BOSS_SHOOT2,SPR_BOSS_SHOOT3,SPR_BOSS_DEAD,

		SPR_BOSS_DIE1,SPR_BOSS_DIE2,SPR_BOSS_DIE3,

//
// schabbs
//
		SPR_SCHABB_W1,SPR_SCHABB_W2,SPR_SCHABB_W3,SPR_SCHABB_W4,
		SPR_SCHABB_SHOOT1,SPR_SCHABB_SHOOT2,

		SPR_SCHABB_DIE1,SPR_SCHABB_DIE2,SPR_SCHABB_DIE3,SPR_SCHABB_DEAD,
		SPR_HYPO1,SPR_HYPO2,SPR_HYPO3,SPR_HYPO4,

//
// fake
//
		SPR_FAKE_W1,SPR_FAKE_W2,SPR_FAKE_W3,SPR_FAKE_W4,
		SPR_FAKE_SHOOT,SPR_FIRE1,SPR_FIRE2,

		SPR_FAKE_DIE1,SPR_FAKE_DIE2,SPR_FAKE_DIE3,SPR_FAKE_DIE4,
		SPR_FAKE_DIE5,SPR_FAKE_DEAD,

//
// hitler
//
		SPR_MECHA_W1,SPR_MECHA_W2,SPR_MECHA_W3,SPR_MECHA_W4,
		SPR_MECHA_SHOOT1,SPR_MECHA_SHOOT2,SPR_MECHA_SHOOT3,SPR_MECHA_DEAD,

		SPR_MECHA_DIE1,SPR_MECHA_DIE2,SPR_MECHA_DIE3,

		SPR_HITLER_W1,SPR_HITLER_W2,SPR_HITLER_W3,SPR_HITLER_W4,
		SPR_HITLER_SHOOT1,SPR_HITLER_SHOOT2,SPR_HITLER_SHOOT3,SPR_HITLER_DEAD,

		SPR_HITLER_DIE1,SPR_HITLER_DIE2,SPR_HITLER_DIE3,SPR_HITLER_DIE4,
		SPR_HITLER_DIE5,SPR_HITLER_DIE6,SPR_HITLER_DIE7,

//
// giftmacher
//
		SPR_GIFT_W1,SPR_GIFT_W2,SPR_GIFT_W3,SPR_GIFT_W4,
		SPR_GIFT_SHOOT1,SPR_GIFT_SHOOT2,

		SPR_GIFT_DIE1,SPR_GIFT_DIE2,SPR_GIFT_DIE3,SPR_GIFT_DEAD,
#endif
//
// Rocket, smoke and small explosion
//
		SPR_ROCKET_1,SPR_ROCKET_2,SPR_ROCKET_3,SPR_ROCKET_4,
		SPR_ROCKET_5,SPR_ROCKET_6,SPR_ROCKET_7,SPR_ROCKET_8,

		SPR_SMOKE_1,SPR_SMOKE_2,SPR_SMOKE_3,SPR_SMOKE_4,
		SPR_BOOM_1,SPR_BOOM_2,SPR_BOOM_3,

//
// Angel of Death's DeathSparks(tm)
//
#ifdef SPEAR
		SPR_HROCKET_1,SPR_HROCKET_2,SPR_HROCKET_3,SPR_HROCKET_4,
		SPR_HROCKET_5,SPR_HROCKET_6,SPR_HROCKET_7,SPR_HROCKET_8,

		SPR_HSMOKE_1,SPR_HSMOKE_2,SPR_HSMOKE_3,SPR_HSMOKE_4,
		SPR_HBOOM_1,SPR_HBOOM_2,SPR_HBOOM_3,

		SPR_SPARK1,SPR_SPARK2,SPR_SPARK3,SPR_SPARK4,
#endif

#ifndef SPEAR
//
// gretel
//
		SPR_GRETEL_W1,SPR_GRETEL_W2,SPR_GRETEL_W3,SPR_GRETEL_W4,
		SPR_GRETEL_SHOOT1,SPR_GRETEL_SHOOT2,SPR_GRETEL_SHOOT3,SPR_GRETEL_DEAD,

		SPR_GRETEL_DIE1,SPR_GRETEL_DIE2,SPR_GRETEL_DIE3,

//
// fat face
//
		SPR_FAT_W1,SPR_FAT_W2,SPR_FAT_W3,SPR_FAT_W4,
		SPR_FAT_SHOOT1,SPR_FAT_SHOOT2,SPR_FAT_SHOOT3,SPR_FAT_SHOOT4,

		SPR_FAT_DIE1,SPR_FAT_DIE2,SPR_FAT_DIE3,SPR_FAT_DEAD,

//
// bj
//
		SPR_BJ_W1,SPR_BJ_W2,SPR_BJ_W3,SPR_BJ_W4,
		SPR_BJ_JUMP1,SPR_BJ_JUMP2,SPR_BJ_JUMP3,SPR_BJ_JUMP4,
#else
//
// THESE ARE FOR 'SPEAR OF DESTINY'
//

//
// Trans Grosse
//
		SPR_TRANS_W1,SPR_TRANS_W2,SPR_TRANS_W3,SPR_TRANS_W4,
		SPR_TRANS_SHOOT1,SPR_TRANS_SHOOT2,SPR_TRANS_SHOOT3,SPR_TRANS_DEAD,

		SPR_TRANS_DIE1,SPR_TRANS_DIE2,SPR_TRANS_DIE3,

//
// Wilhelm
//
		SPR_WILL_W1,SPR_WILL_W2,SPR_WILL_W3,SPR_WILL_W4,
		SPR_WILL_SHOOT1,SPR_WILL_SHOOT2,SPR_WILL_SHOOT3,SPR_WILL_SHOOT4,

		SPR_WILL_DIE1,SPR_WILL_DIE2,SPR_WILL_DIE3,SPR_WILL_DEAD,

//
// UberMutant
//
		SPR_UBER_W1,SPR_UBER_W2,SPR_UBER_W3,SPR_UBER_W4,
		SPR_UBER_SHOOT1,SPR_UBER_SHOOT2,SPR_UBER_SHOOT3,SPR_UBER_SHOOT4,

		SPR_UBER_DIE1,SPR_UBER_DIE2,SPR_UBER_DIE3,SPR_UBER_DIE4,
		SPR_UBER_DEAD,

//
// Death Knight
//
		SPR_DEATH_W1,SPR_DEATH_W2,SPR_DEATH_W3,SPR_DEATH_W4,
		SPR_DEATH_SHOOT1,SPR_DEATH_SHOOT2,SPR_DEATH_SHOOT3,SPR_DEATH_SHOOT4,

		SPR_DEATH_DIE1,SPR_DEATH_DIE2,SPR_DEATH_DIE3,SPR_DEATH_DIE4,
		SPR_DEATH_DIE5,SPR_DEATH_DIE6,SPR_DEATH_DEAD,

//
// Ghost
//
		SPR_SPECTRE_W1,SPR_SPECTRE_W2,SPR_SPECTRE_W3,SPR_SPECTRE_W4,
		SPR_SPECTRE_F1,SPR_SPECTRE_F2,SPR_SPECTRE_F3,SPR_SPECTRE_F4,

//
// Angel of Death
//
		SPR_ANGEL_W1,SPR_ANGEL_W2,SPR_ANGEL_W3,SPR_ANGEL_W4,
		SPR_ANGEL_SHOOT1,SPR_ANGEL_SHOOT2,SPR_ANGEL_TIRED1,SPR_ANGEL_TIRED2,

		SPR_ANGEL_DIE1,SPR_ANGEL_DIE2,SPR_ANGEL_DIE3,SPR_ANGEL_DIE4,
		SPR_ANGEL_DIE5,SPR_ANGEL_DIE6,SPR_ANGEL_DIE7,SPR_ANGEL_DEAD,

#endif

//
// player attack frames
//
		SPR_KNIFEREADY,SPR_KNIFEATK1,SPR_KNIFEATK2,SPR_KNIFEATK3,
		SPR_KNIFEATK4,

		SPR_PISTOLREADY,SPR_PISTOLATK1,SPR_PISTOLATK2,SPR_PISTOLATK3,
		SPR_PISTOLATK4,

		SPR_MACHINEGUNREADY,SPR_MACHINEGUNATK1,SPR_MACHINEGUNATK2,SPR_MACHINEGUNATK3,
		SPR_MACHINEGUNATK4,

		SPR_CHAINREADY,SPR_CHAINATK1,SPR_CHAINATK2,SPR_CHAINATK3,
		SPR_CHAINATK4, SPR_NULLSPRITE, SPR_TOTAL
};


/*
=============================================================================

						   GLOBAL TYPES

=============================================================================
*/

typedef uint64_t mapbitmap[MAPSIZE];
static inline boolean getmapbit(mapbitmap m, int x, int y)
{
  return (m[x] & (1ull << y)) != 0;
}
static inline void setmapbit(mapbitmap m, int x, int y)
{
  m[x] |= (1ull << y);
}
static inline void clearmapbit(mapbitmap m, int x, int y)
{
  m[x] &= ~(1ull << y);
}

typedef enum PACKED {
	di_north,
	di_east,
	di_south,
	di_west
} controldir_t;

typedef enum PACKED {
	dr_normal,
	dr_lock1,
	dr_lock2,
	dr_lock3,
	dr_lock4,
	dr_elevator
} door_t;

typedef enum PACKED {
	ac_badobject = -1,
	ac_no,
	ac_yes
} activetype;

typedef enum PACKED {
	nothing,
	playerobj,
	inertobj,
	guardobj,
	officerobj,
	ssobj,
	dogobj,
	bossobj,
	schabbobj,
	fakeobj,
	mechahitlerobj,
	mutantobj,
	needleobj,
	fireobj,
	bjobj,
	ghostobj,
	realhitlerobj,
	gretelobj,
	giftobj,
	fatobj,
	rocketobj,

	spectreobj,
	angelobj,
	transobj,
	uberobj,
	willobj,
	deathobj,
	hrocketobj,
	sparkobj
} classtype;

typedef enum PACKED {
	dressing,
	block,
	bo_gibs,
	bo_alpo,
	bo_firstaid,
	bo_key1,
	bo_key2,
	bo_key3,
	bo_key4,
	bo_cross,
	bo_chalice,
	bo_bible,
	bo_crown,
	bo_clip,
	bo_clip2,
	bo_machinegun,
	bo_chaingun,
	bo_food,
	bo_fullheal,
	bo_25clip,
	bo_spear
} stat_t;

typedef enum PACKED {
	east,
	northeast,
	north,
	northwest,
	west,
	southwest,
	south,
	southeast,
	nodir
} dirtype;

#define NUMENEMIES		22
typedef enum PACKED {
	en_guard,
	en_officer,
	en_ss,
	en_dog,
	en_boss,
	en_schabbs,
	en_fake,
	en_hitler,
	en_mutant,
	en_blinky,
	en_clyde,
	en_pinky,
	en_inky,
	en_gretel,
	en_gift,
	en_fat,
	en_spectre,
	en_angel,
	en_trans,
	en_uber,
	en_will,
	en_death
} enemy_t;

//---------------------
//
// trivial actor structure
//
//---------------------

typedef struct statstruct
{
	byte	tilex,tiley;
	myshort	shapenum:10;		/* if shapenum == -1 the obj has been removed */
	stat_t	itemnumber:5;
	boolean is_bonus:1;
} statobj_t;

//---------------------
//
// door actor structure
//
//---------------------

typedef struct doorstruct
{
	byte	tilex,tiley;
	boolean	vertical:1;
	door_t	lock:3;
	enum PACKED {dr_open,dr_closed,dr_opening,dr_closing} action :2;
	myshort	ticcount:10;
} doorobj_t;

//--------------------
//
// thinking actor structure
//
//--------------------

typedef struct objstruct
{
	activetype	active:2;
	dirtype		dir:4;
	classtype	obclass:5;
	myshort		state; /* stateenum */
	myint		ticcount;

	int32_t		distance;	/* if negative, wait for that door to open */

	fixed 		x,y;
	byte	tilex,tiley;
	byte		areanumber;
	byte		flags;		/* FL_SHOOTABLE, etc */


	myshort	 	viewx;
	umyshort	viewheight;
	fixed		transx, transy;		/* in global coord */

	myshort 		angle;
	myshort		hitpoints;
	int32_t		speed;

	byte		temp1;
	signed char temp2;
	byte next;
	byte prev;
} objtype;

typedef void (*action_fn)(objtype *ob);

typedef struct statestruct
{
	boolean	rotate:2;
	myshort shapenum:10; /* a shapenum of -1 means get from ob->temp1 */
	myshort tictime:10;
	byte think:6;
	byte action:6;
	myshort next:10; /* stateenum */
} statetype;

#define THINK_FN(ob) AllActions[gamestates[ob->state].think]
#define ACTION_FN(ob) AllActions[gamestates[ob->state].action]

#define NUMBUTTONS	8
enum PACKED {
	bt_nobutton=-1,
	bt_attack=0,
	bt_strafe,
	bt_run,
	bt_use,
	bt_readyknife,
	bt_readypistol,
	bt_readymachinegun,
	bt_readychaingun
};


#define NUMWEAPONS	4
typedef enum PACKED {
	wp_none = -1,
	wp_knife,
	wp_pistol,
	wp_machinegun,
	wp_chaingun
} weapontype;


enum PACKED {
	gd_baby,
	gd_easy,
	gd_medium,
	gd_hard
};

//---------------
//
// gamestate structure
//
//---------------

typedef	struct
{
#ifndef EMBEDDED
#define gamestate_difficulty gamestate.difficulty
	myint		difficulty;
#else
#define gamestate_difficulty gd_medium
#endif
	myint		mapon;
	long		oldscore,score,nextextra;
	myint		lives;
	myint		health;
	myint		ammo;
	myint		keys;
	weapontype	bestweapon,weapon,chosenweapon;

	myint		faceframe;
	myint		attackframe,attackcount,weaponframe;

#ifndef EMBEDDED
#define gamestate_episode gamestate.episode
	myint		episode;
#else
#define gamestate_episode 0
#endif
#ifdef ENABLE_STATS
	myint		secretcount,treasurecount,killcount;
	myint		secrettotal,treasuretotal;
	myint killtotal;
#endif
	long		TimeCount;
	long		killx,killy;
	boolean		victoryflag;		// set during victory animations
} gametype;


typedef	enum PACKED {
	ex_stillplaying,
	ex_completed,
	ex_died,
	ex_warped,
	ex_resetgame,
	ex_loadedgame,
	ex_victorious,
	ex_abort,
	ex_demodone,
	ex_secretlevel
} exit_t;


/*
=============================================================================

						 WL_MAIN DEFINITIONS

=============================================================================
*/

extern	char str[80], str2[20];

#define FOCALLENGTH     0x5800		/* in global coordinates */

#ifdef EMBEDDED
#define viewwidth 128
#define sviewheight 64
#define xoffset 0
#define yoffset 0
#define centerx (viewwidth/2-1)
#define shootdelta (viewwidth/10)
#else
extern myint viewwidth, sviewheight;
extern myint viewwidthwin, viewheightwin;
extern myint xoffset, yoffset;

extern	myint			centerx;
extern	myint			shootdelta;
#endif

extern	boolean         startgame,loadedgame;
extern	myint		mouseadjustment;

/* math tables */
#ifdef EMBEDDED
// FIXME:  This only needs to be half size.
extern const myshort pixelangle[MAXVIEWWIDTH];
#else
extern myshort pixelangle[MAXVIEWWIDTH];
#endif
extern const fixed finetangent[FINEANGLES/4];
extern const uint16_t sintable[];
fixed sinfix(int a);
fixed cosfix(int a);

extern char configname[13];

void CalcProjection(long focal);
void NewGame(myint difficulty,myint episode);
void NewViewSize(myint width);
void ShowViewSize(myint width);

myint LoadTheGame(const char *fn, myint x, myint y);
myint SaveTheGame(const char *fn, const char *tag, myint x, myint y);
myint ReadSaveTag(const char *fn, const char *tag);

void ShutdownId();
myint WriteConfig();

myint WolfMain(myint argc, char *argv[]);

/*
=============================================================================

						 WL_GAME DEFINITIONS

=============================================================================
*/


extern	boolean		ingame;
extern	gametype	gamestate;
extern	myint		doornum;

extern	char		demoname[13];

extern	long		spearx,speary;
extern	unsigned	spearangle;
extern	boolean		spearflag;

#define ms_pushable 1
#define ms_exit 2
#define ms_arrow 8

extern byte mapspecial[(MAPSIZE * MAPSIZE) >> 1];
static inline int getmapspecial(int x, int y)
{
  int n = (x << 5) + (y >> 1);
  if (y & 1)
    return mapspecial[n] & 0xf;
  else
    return mapspecial[n] >> 4;
}

static inline void setmapspecial(int x, int y, int newval)
{
  int n = (x << 5) + (y >> 1);
  uint8_t val;
  val = mapspecial[n];
  if (y & 1)
    val = (val & 0xf0) | newval;
  else
    val = (val & 0xf) | (newval << 4);
  mapspecial[n] = val;
}

void 	ScanInfoPlane (void);
void	SetupGameLevel (void);
void 	DrawPlayScreen (void);
void 	GameLoop (void);
void ClearMemory (void);
void PlayDemo(myint demonumber);
myint PlayDemoFromFile(const char *demoname);
void RecordDemo();
void DrawHighScores();
void DrawPlayBorder();
void DrawPlayBorderSides();
void DrawStatusBar();

#define	PlaySoundLocTile(s,tx,ty)	PlaySoundLocGlobal(s,(myint)((tx<<6)|(ty)), (tx << TILESHIFT) + (1 << (TILESHIFT - 1)), (ty << TILESHIFT) + (1L << (TILESHIFT - 1)))
#define	PlaySoundLocActor(s,ob)		PlaySoundLocGlobal(s,(int)(long)ob,(ob)->x,(ob)->y)

/*
=============================================================================

						 WL_PLAY DEFINITIONS

=============================================================================
*/

extern	long		funnyticount;		// FOR FUNNY BJ FACE

extern	exit_t		playstate;

extern	boolean		madenoise;

#define INVALID_OBJID 0xff
#define obj_id(ob) ((ob) - objlist)
#define obj_prev(ob) (((ob)->prev == INVALID_OBJID) ? NULL : &objlist[ob->prev])
#define obj_next(ob) (((ob)->next == INVALID_OBJID) ? NULL : &objlist[ob->next])

extern	objtype 	objlist[MAXACTORS],*new,*obj,*player,*lastobj,
					*objfreelist,*killerobj;
extern	statobj_t	statobjlist[MAXSTATS],*laststatobj;
extern	doorobj_t	doorobjlist[MAXDOORS],*lastdoorobj;

#define farmapylookup(y) (y << 6)
extern	byte		tilemap[MAPSIZE][MAPSIZE];	// wall values only

extern mapbitmap spotvis;
#define getspotvis(x, y) getmapbit(spotvis, x, y)
#define setspotvis(x, y) setmapbit(spotvis, x, y)

extern mapbitmap objactor;
#define getactorflag(x, y) getmapbit(objactor, x, y)
#define setactorflag(x, y) setmapbit(objactor, x, y)
#define clearactorflag(x, y) clearmapbit(objactor, x, y)

extern	byte		actorat[MAPSIZE][MAPSIZE];

/* Record actor location.  */
#define move_actor(o) do { \
    setactorflag((o)->tilex, (o)->tiley); \
    actorat[(o)->tilex][(o)->tiley] = obj_id(o); \
    } while (0)

/* Record Door location.  */
#define set_door_actor(x, y, doornum) actorat[x][y] = doornum | 0x80
/* Record wall location.  */
#define set_wall_at(x, y, tile) actorat[x][y] = tile
/* Clear location.  */
#define clear_actor(x, y) do { \
    actorat[x][y] = 0; \
    clearactorflag(x, y); \
    } while (0)
/* nonzero if something other than a door is at given location.  */
#define obj_actor_at(x, y) getactorflag(x, y)
/* nonzero if a wall is at given location.  */
#define wall_actor_at(x, y) (actorat[x][y] && actorat[x][y] < 128 \
			    && !getactorflag(x, y))
/* nonzero if a door or wall is at given location.  */
#define solid_actor_at(x, y) (actorat[x][y] && !getactorflag(x, y))
/* zero if given location is empty.  */
#define any_actor_at(x, y) (actorat[x][y] != 0 || getactorflag(x, y))
/* The id of the actor at given location.  */
#define get_actor_at(x, y) \
  (obj_actor_at(x, y) ? actorat[x][y] & 0xff : actorat[x][y] & 0x7f)

extern	boolean		singlestep,godmode,noclip;

//
// control info
//
#ifdef ENABLE_CONTROLS
#define CONTROL_CONST
#else
#define CONTROL_CONST const
#endif
extern	boolean		mouseenabled,joystickenabled,joypadenabled;
extern	myint			joystickport;
extern	CONTROL_CONST byte			dirscan[4];
extern	CONTROL_CONST byte	buttonscan[NUMBUTTONS];
extern	CONTROL_CONST byte	buttonmouse[4];
extern	byte			buttonjoy[4];

extern	boolean		buttonheld[NUMBUTTONS];

extern	myint viewsize;

//
// curent user input
//
extern	myint			controlx,controly;		// range from -100 to 100
extern	boolean		buttonstate[NUMBUTTONS];

#ifdef ENABLE_DEMO
extern	boolean		demorecord,demoplayback;
extern	byte		*demoptr, *lastdemoptr;
extern	memptr		demobuffer;
#endif


void	InitRedShifts (void);
void 	FinishPaletteShifts (void);

void	CenterWindow(word w,word h);
void 	InitActorList (void);
void 	GetNewActor (void);
#ifdef ENABLE_MUSIC
void 	StopMusic(void);
void 	StartMusic(void);
#else
#define StopMusic() do {} while (0)
#define StartMusic() do {} while (0)
#endif
void	PlayLoop (void);
void StartDamageFlash (myint damage);
void StartBonusFlash (void);

/*
=============================================================================

							WL_INTER

=============================================================================
*/

void IntroScreen (void);
void PreloadGraphics(void);
void LevelCompleted (void);
void CheckHighScore (long score,word other);
void Victory (void);
void ClearSplitVWB (void);
void PG13();

/*
=============================================================================

							WL_DEBUG

=============================================================================
*/

myint DebugKeys (void);
void PicturePause (void);

/*
=============================================================================

						 WL_DRAW DEFINITIONS

=============================================================================
*/

extern long lasttimecount;
extern long frameon;

/* refresh variables */
extern fixed viewx, viewy;			/* the focal point */
extern fixed viewsin, viewcos;

#define horizwall(x) ((x)-1)*2
#define vertwall(x) ((x)-1)*2+1

void BuildTables();
void CalcTics();
void ThreeDRefresh();

void FizzleFade(boolean abortable, myint frames, myint color);

/*
=============================================================================

						 WL_STATE DEFINITIONS

=============================================================================
*/

#define SPDPATROL	512
#define SPDDOG		1500

void	SpawnNewObj(unsigned tilex, unsigned tiley, myint state); /* stateenum */ 
void	NewState(objtype *ob, myint state); /* stateenum */

boolean TryWalk (objtype *ob);
void 	SelectChaseDir (objtype *ob);
void 	SelectDodgeDir (objtype *ob);
void	SelectRunDir (objtype *ob);
void	MoveObj (objtype *ob, long move);
boolean SightPlayer (objtype *ob);

void	KillActor (objtype *ob);
void	DamageActor (objtype *ob, unsigned damage);

boolean CheckLine (objtype *ob);
boolean	CheckSight (objtype *ob);

/*
=============================================================================

						 WL_AGENT DEFINITIONS

=============================================================================
*/

//
// player state info
//
extern	long		thrustspeed;
extern	unsigned	plux,pluy;		// player coordinates scaled to unsigned

extern	myint			anglefrac;
extern	myint			facecount;

void	SpawnPlayer (myint tilex, myint tiley, myint dir);
void	DrawHealth (void);
void	TakeDamage (myint points,objtype *attacker);
void	HealSelf (myint points);
void	DrawLevel (void);
void	DrawLives (void);
void	GiveExtraMan (void);
void	DrawScore (void);
void	GivePoints (long points);
void	GiveWeapon (myint weapon);
void	DrawAmmo (void);
void	GiveAmmo (myint ammo);
void	GiveKey (myint key);
void	GetBonus (statobj_t *check);

#ifdef EMBEDDED
#define DrawKeys() do {} while(0)
#define DrawWeapon() do {} while(0)
#define DrawFace() do {} while(0)
#define StatusDrawPic(x,y,picnum) do {} while(0)
#else
void	DrawKeys (void);
void	DrawWeapon (void);
void 	DrawFace (void);
void StatusDrawPic(unsigned x, unsigned y, unsigned picnum);

#endif
void	Thrust (myint angle, long speed);

/*
=============================================================================

						 WL_ACT1 DEFINITIONS

=============================================================================
*/

extern	doorobj_t	doorobjlist[MAXDOORS],*lastdoorobj;
extern	myint			doornum;

extern	umyshort	doorposition[MAXDOORS],pwallstate;

extern	uint64_t	areabyplayer;
static inline boolean getareabyplayer(int i)
{
  return (areabyplayer & (1ull << i)) != 0;
}
static inline void setareabyplayer(int i)
{
  areabyplayer |= (1ull << i);
}

extern umyshort	pwallstate;
extern umyshort	pwallpos;			// amount a pushable wall has been moved (0-63)
extern umyshort	pwallx,pwally;
extern myint			pwalldir;


void InitDoorList (void);
void InitStaticList (void);
void SpawnStatic (myint tilex, myint tiley, myint type);
void SpawnDoor (myint tilex, myint tiley, boolean vertical, myint lock);
void MoveDoors (void);
void MovePWalls (void);
void OpenDoor (myint door);
void PlaceItemType (myint itemtype, myint tilex, myint tiley);
void PushWall (myint checkx, myint checky, myint dir);
void OperateDoor (myint door);
void InitAreas (void);

/*
=============================================================================

						 WL_ACT2 DEFINITIONS

=============================================================================
*/

void A_DeathScream(objtype *ob);

void SpawnBJVictory(void);

void SpawnStand (enemy_t which, myint tilex, myint tiley, myint dir);
void SpawnPatrol (enemy_t which, myint tilex, myint tiley, myint dir);

void US_ControlPanel(byte);

void SpawnDeadGuard (myint tilex, myint tiley);
void SpawnBoss (myint tilex, myint tiley);
void SpawnGretel (myint tilex, myint tiley);
void SpawnTrans (myint tilex, myint tiley);
void SpawnUber (myint tilex, myint tiley);
void SpawnWill (myint tilex, myint tiley);
void SpawnDeath (myint tilex, myint tiley);
void SpawnAngel (myint tilex, myint tiley);
void SpawnSpectre (myint tilex, myint tiley);
void SpawnGhosts (myint which, myint tilex, myint tiley);
void SpawnSchabbs (myint tilex, myint tiley);
void SpawnGift (myint tilex, myint tiley);
void SpawnFat (myint tilex, myint tiley);
void SpawnFakeHitler (myint tilex, myint tiley);
void SpawnHitler (myint tilex, myint tiley);

/*
=============================================================================

						 WL_TEXT DEFINITIONS

=============================================================================
*/

extern void HelpScreens();
extern void EndText();

#include "wl_act3.h"

/* FixedByFrac */
//fixed FixedByFrac(fixed a, fixed b);
static inline fixed FixedByFrac(fixed a, fixed b)
{
	int64_t ra = a;
	int64_t rb = b;
	int64_t r;
	
	r = ra * rb;
	r >>= TILESHIFT;
	return (fixed)r;
}

#ifndef NOASM
#define FixedByFrac(x, y) \
__extension__  \
({ unsigned long z; \
 asm("imull %2; shrdl $16, %%edx, %%eax" : "=a" (z) : "a" (x), "q" (y) : "%edx"); \
 z; \
})
#endif

#endif
