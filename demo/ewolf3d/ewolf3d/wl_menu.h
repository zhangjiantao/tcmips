#ifndef __WL_MENU_H__
#define __WL_MENU_H__

#ifdef SPEAR

#define BORDCOLOR	0x99
#define BORD2COLOR	0x93
#define DEACTIVE	0x9b
#define BKGDCOLOR	0x9d
//#define STRIPE		0x9c

#define MenuFadeOut()	VL_FadeOut(0,255,0,0,51,10)

#else

#define BORDCOLOR	0x29
#define BORD2COLOR	0x23
#define DEACTIVE	0x2b
#define BKGDCOLOR	0x2d
#define STRIPE		0x2c

#define MenuFadeOut()	VL_FadeOut(0,255,43,0,0,10)

#endif

#define READCOLOR	0x4a
#define READHCOLOR	0x47
#define VIEWCOLOR	0x7f
#define TEXTCOLOR	0x17
#define HIGHLIGHT	0x13
#define MenuFadeIn()	VL_FadeIn(0,255,gamepal,10)


#define MENUSONG	WONDERIN_MUS

#ifndef SPEAR
#define INTROSONG	NAZI_NOR_MUS
#else
#define INTROSONG	XTOWER2_MUS
#endif

#define SENSITIVE	60
#define CENTER		SENSITIVE*2

#define MENU_X	76
#define MENU_Y	55
#define MENU_W	178
#if defined(UPLOAD)
#define MENU_H	13*10+6
#else
#define MENU_H	13*9+6
#endif

#define SM_X	48
#define SM_W	250

#define SM_Y1	20
#define SM_H1	4*13-7
#define SM_Y2	SM_Y1+5*13
#define SM_H2	4*13-7
#define SM_Y3	SM_Y2+5*13
#define SM_H3	3*13-7

#define CTL_X	24
#define CTL_Y	70
#define CTL_W	284
#define CTL_H	13*7-7

#define LSM_X	85
#define LSM_Y	55
#define LSM_W	175
#define LSM_H	10*13+10

#define NM_X	50
#define NM_Y	100
#define NM_W	225
#define NM_H	13*4+15

#define NE_X	10
#define NE_Y	23
#define NE_W	320-NE_X*2
#define NE_H	200-NE_Y*2

#define CST_X		20
#define CST_Y		48
#define CST_START	60
#define CST_SPC		60


//
// TYPEDEFS
//
typedef const struct {
	myshort x,y,amount,indent;
	signed char *curpos;
	byte *active;
} CP_iteminfo;

typedef void (* MenuFunc)(myint temp1);

typedef const struct {
	byte iactive;
	char string[35];
	// FIXME: Make this an index.
	MenuFunc routine;
} CP_itemtype;

typedef struct {
	boolean allowed[4];
} CustomCtrls;

extern CP_itemtype MainMenu[], NewEMenu[];

//
// FUNCTION PROTOTYPES
//
void SetupControlPanel(void);
void CleanupControlPanel(void);

void DrawMenu(CP_iteminfo *item_i,CP_itemtype *items);
myint HandleMenu(CP_iteminfo *item_i, CP_itemtype *items, void (*routine)(myint w));
void ClearMScreen(void);
void DrawWindow(myint x,myint y,myint w,myint h,myint wcolor);
void DrawOutline(myint x,myint y,myint w,myint h,myint color1,myint color2);
void WaitKeyUp(void);
void ReadAnyControl(ControlInfo *ci);
void TicDelay(myint count);
#ifdef EMBEDDED
#define CacheLump(s, e) do {} while(0)
#define UnCacheLump(s, e) do {} while(0)
#define StartCPMusic(s) do {} while(0)
#define Confirm(s) 1
#define Message(s) do {} while(0)
#else
void CacheLump(myint lumpstart,myint lumpend);
void UnCacheLump(myint lumpstart,myint lumpend);
void StartCPMusic(myint song);
myint  Confirm(const char *string);
void Message(const char *string);
#endif
void CheckPause(void);
void ShootSnd(void);
void FreeMusic(void);
void BossKey(void);

void DrawGun(CP_iteminfo *item_i,CP_itemtype *items,myint x,myint *y,myint which,myint basey,void (*routine)(myint w));
void DrawHalfStep(myint x,myint y);
void EraseGun(CP_iteminfo *item_i,CP_itemtype *items,myint x,myint y,myint which);
void SetMenuTextColor(byte active,myint hlight);
void DrawMenuGun(CP_iteminfo *iteminfo);
void DrawStripes(myint y);

void DefineMouseBtns(void);
void DefineJoyBtns(void);
void DefineKeyBtns(void);
void DefineKeyMove(void);
void EnterCtrlData(myint index,const CustomCtrls *cust,void (*DrawRtn)(myint),void (*PrintRtn)(myint),myint type);

void DrawMainMenu(void);
void DrawSoundMenu(void);
void DrawLoadSaveScreen(myint loadsave);
void DrawNewEpisode(void);
void DrawNewGame(void);
void DrawChangeView(myint view);
void DrawMouseSens(void);
void DrawCtlScreen(void);
void DrawCustomScreen(void);
void DrawLSAction(myint which);
void DrawCustMouse(myint hilight);
void DrawCustJoy(myint hilight);
void DrawCustKeybd(myint hilight);
void DrawCustKeys(myint hilight);
void PrintCustMouse(myint i);
void PrintCustJoy(myint i);
void PrintCustKeybd(myint i);
void PrintCustKeys(myint i);

void PrintLSEntry(myint w,myint color);
void TrackWhichGame(myint w);
void DrawNewGameDiff(myint w);
void FixupCustom(myint w);

void CP_NewGame(void);
void CP_Sound(void);
myint  CP_LoadGame(myint quick);
myint  CP_SaveGame(myint quick);
void CP_Control(void);
void CP_ChangeView(void);
void CP_ExitOptions(void);
void CP_Quit(void);
void CP_ViewScores(void);
myint  CP_EndGame(void);
myint  CP_CheckQuick(unsigned scancode);
void CustomControls(void);
void MouseSensitivity(void);

void CheckForEpisodes(void);

//
// VARIABLES
//

typedef enum
{
	newgame,
	soundmenu,
	control,
	loadgame,
	savegame,
	changeview,
#ifdef UPLOAD
	readthis,
#endif
	viewscores,
	backtodemo,
	quit
} menuitems;

//
// WL_INTER
//
typedef struct {
	myshort kill,secret,treasure;
	myshort time;
} LRstruct;

extern LRstruct LevelRatios[];

void Write(myint x,myint y,const char *string);
myint GetYorN(myint x,myint y,myint pic);

#endif
