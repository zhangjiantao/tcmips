#include "wl_def.h"

/*
=============================================================================

					GLOBAL VARIABLES

=============================================================================
*/

//
// configuration variables
//
#ifdef ENABLE_MOUSE
boolean		MousePresent;
#endif
#ifdef ENABLE_JOYSTICK
boolean		JoysPresent[MaxJoys];
#endif

// 	Global variables
uint32_t	Keyboard[NumCodes >> 5];
boolean		Paused;
char		LastASCII;
ScanCode	LastScan;
unsigned long	LastEventTime;

KeyboardDef KbdDefs = {sc_Control, sc_Alt, sc_Home, sc_UpArrow, sc_PgUp, sc_LeftArrow, sc_RightArrow, sc_End, sc_DownArrow, sc_PgDn};

ControlType	Controls[MaxPlayers];

/*
=============================================================================

					LOCAL VARIABLES

=============================================================================
*/
#ifndef EMBEDDED
static const byte ASCIINames[] =		// Unshifted ASCII for scan codes
					{
//	 0   1   2   3   4   5   6   7   8   9   A   B   C   D   E   F
	 0  ,27 ,'1','2','3','4','5','6','7','8','9','0','-','=',8  ,9  ,	// 0
	'q','w','e','r','t','y','u','i','o','p','[',']',13 ,0  ,'a','s',	// 1
	'd','f','g','h','j','k','l',';',39 ,'`',0  ,92 ,'z','x','c','v',	// 2
	'b','n','m',',','.','/',0  ,'*',0  ,' ',0  ,0  ,0  ,0  ,0  ,0  ,	// 3
	0  ,0  ,0  ,0  ,0  ,0  ,0  ,'7','8','9','-','4','5','6','+','1',	// 4
	'2','3','0',127,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,	// 5
	0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,	// 6
	0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0		// 7
					},
					 ShiftNames[] =		// Shifted ASCII for scan codes
					{
//	 0   1   2   3   4   5   6   7   8   9   A   B   C   D   E   F
	0  ,27 ,'!','@','#','$','%','^','&','*','(',')','_','+',8  ,9  ,	// 0
	'Q','W','E','R','T','Y','U','I','O','P','{','}',13 ,0  ,'A','S',	// 1
	'D','F','G','H','J','K','L',':',34 ,'~',0  ,'|','Z','X','C','V',	// 2
	'B','N','M','<','>','?',0  ,'*',0  ,' ',0  ,0  ,0  ,0  ,0  ,0  ,	// 3
	0  ,0  ,0  ,0  ,0  ,0  ,0  ,'7','8','9','-','4','5','6','+','1',	// 4
	'2','3','0',127,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,	// 5
	0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,	// 6
	0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0   	// 7
					};
#endif

static	boolean		IN_Started;
#ifndef EMBEDDED
static	boolean		CapsLock;
#endif
static	ScanCode	CurCode,LastCode;

static	const Direction	DirTable[] =		// Quick lookup for total direction
					{
						dir_NorthWest,	dir_North,	dir_NorthEast,
						dir_West,		dir_None,	dir_East,
						dir_SouthWest,	dir_South,	dir_SouthEast
					};

#ifndef KBD_ONLY
static boolean btnstate[8];
#endif

#define	GetKey(code) \
  ((Keyboard[(code >> 5)] & (1 << (code & 31))) != 0)
#define	SetKey(code) \
  Keyboard[(code >> 5)] |= (1 << (code & 31))
#define	ClearKey(code) \
  Keyboard[(code >> 5)] &= ~(1 << (code & 31))
void keyboard_handler(myint code, myint press)
{
	byte k;
#ifndef EMBEDDED
	byte c = 0;
#endif

	LastEventTime = get_TimeCount();

 	k = code;

	if (k == 0xe1)	// Handle Pause key
		Paused = true;
	else
	{
		if (press == 0)	
		{
			ClearKey(k);
		}
		else			// Make code
		{
			LastCode = CurCode;
			CurCode = LastScan = k;
			
			SetKey(k);

#ifndef EMBEDDED
			if (k == sc_CapsLock)
			{
				CapsLock ^= true;
			}

			if (GetKey(sc_LShift) || GetKey(sc_RShift))	// If shifted
			{
				c = ShiftNames[k];
				if ((c >= 'A') && (c <= 'Z') && CapsLock)
					c += 'a' - 'A';
			}
			else
			{
				c = ASCIINames[k];
				if ((c >= 'a') && (c <= 'z') && CapsLock)
					c -= 'a' - 'A';
			}
			
			if (c)
				LastASCII = c;
#endif
		}
	}
}

#ifndef EMBEDDED
///////////////////////////////////////////////////////////////////////////
//
//	IN_UserInput() - Waits for the specified delay time (in ticks) or the
//		user pressing a key or a mouse button. If the clear flag is set, it
//		then either clears the key or waits for the user to let the mouse
//		button up.
//
///////////////////////////////////////////////////////////////////////////
boolean IN_UserInput(longword delay)
{
	longword	lasttime;

	lasttime = get_TimeCount();
	
	IN_StartAck();
	do {
		if (IN_CheckAck())
			return true;
	} while ( (get_TimeCount() - lasttime) < delay );
	
	return false;
}
#endif

//===========================================================================

///////////////////////////////////////////////////////////////////////////
//
//	INL_StartKbd() - Sets up my keyboard stuff for use
//
///////////////////////////////////////////////////////////////////////////
static void INL_StartKbd(void)
{
	IN_ClearKeysDown();
}

#ifndef EMBEDDED
///////////////////////////////////////////////////////////////////////////
//
//	INL_ShutKbd() - Restores keyboard control to the BIOS
//
///////////////////////////////////////////////////////////////////////////
static void INL_ShutKbd(void)
{
}

///////////////////////////////////////////////////////////////////////////
//
//	INL_ShutMouse() - Cleans up after the mouse
//
///////////////////////////////////////////////////////////////////////////
static void INL_ShutMouse(void)
{
}
#endif

#ifdef ENABLE_JOYSTICK
///////////////////////////////////////////////////////////////////////////
//
//	INL_StartJoy() - Detects & auto-configures the specified joystick
//					The auto-config assumes the joystick is centered
//
///////////////////////////////////////////////////////////////////////////
static boolean INL_StartJoy(word joy)
{
	return false;
}

///////////////////////////////////////////////////////////////////////////
//
//	INL_ShutJoy() - Cleans up the joystick stuff
//
///////////////////////////////////////////////////////////////////////////
static void INL_ShutJoy(word joy)
{
	JoysPresent[joy] = false;
}
#endif


///////////////////////////////////////////////////////////////////////////
//
//	IN_Startup() - Starts up the Input Mgr
//
///////////////////////////////////////////////////////////////////////////
void IN_Startup(void)
{
#ifdef ENABLE_JOYSTICK
	boolean	checkjoys;
	word	i;
#endif

	if (IN_Started)
		return;

#ifndef EMBEDDED
	checkjoys = true;
	
	if (MS_CheckParm("nojoy"))
		checkjoys = false;
	if (MS_CheckParm("nomouse"))
		MousePresent = false;
	else
		MousePresent = true;
#endif

	INL_StartKbd();

#ifdef ENABLE_JOYSTICK
	for (i = 0;i < MaxJoys;i++)
		JoysPresent[i] = checkjoys ? INL_StartJoy(i) : false;
#endif

	IN_Started = true;
}

#ifndef EMBEDDED
///////////////////////////////////////////////////////////////////////////
//
//	IN_Shutdown() - Shuts down the Input Mgr
//
///////////////////////////////////////////////////////////////////////////
void IN_Shutdown(void)
{
#ifdef ENABLE_JOYSTICK
	word i;
#endif

	if (!IN_Started)
		return;

	INL_ShutMouse();
#ifdef ENABLE_JOYSTICK
	for (i = 0;i < MaxJoys;i++)
		INL_ShutJoy(i);
#endif
	INL_ShutKbd();

	IN_Started = false;
}
#endif

///////////////////////////////////////////////////////////////////////////
//
//	IN_ClearKeysDown() - Clears the keyboard array
//
///////////////////////////////////////////////////////////////////////////
void IN_ClearKeysDown(void)
{
	LastScan = sc_None;
	LastASCII = key_None;
	memset(Keyboard, 0, sizeof(Keyboard));
}

#ifndef EMBEDDED
///////////////////////////////////////////////////////////////////////////
//
//	IN_ReadControl() - Reads the device associated with the specified
//		player and fills in the control info struct
//
///////////////////////////////////////////////////////////////////////////
void IN_ReadControl(myint player,ControlInfo *info)
{
			boolean		realdelta = false;
			word		buttons;
			myint			dx,dy;
			Motion		mx,my;
			ControlType	type;
			KeyboardDef	*def;

	dx = dy = 0;
	mx = my = motion_None;
	buttons = 0;

IN_CheckAck();

		switch (type = Controls[player])
		{
		case ctrl_Keyboard:
			def = &KbdDefs;

#ifndef EMBEDDED
			if (GetKey(def->upleft))
				mx = motion_Left,my = motion_Up;
			else if (GetKey(def->upright))
				mx = motion_Right,my = motion_Up;
			else if (GetKey(def->downleft))
				mx = motion_Left,my = motion_Down;
			else if (GetKey(def->downright))
				mx = motion_Right,my = motion_Down;
#endif

			if (GetKey(def->up))
				my = motion_Up;
			else if (GetKey(def->down))
				my = motion_Down;

			if (GetKey(def->left))
				mx = motion_Left;
			else if (GetKey(def->right))
				mx = motion_Right;

			if (GetKey(def->button0))
				buttons += 1 << 0;
			if (GetKey(def->button1))
				buttons += 1 << 1;
			realdelta = false;
			break;
#ifdef ENABLE_JOYSTICK
		case ctrl_Joystick1:
		case ctrl_Joystick2:
			INL_GetJoyDelta(type - ctrl_Joystick,&dx,&dy);
			buttons = INL_GetJoyButtons(type - ctrl_Joystick);
			realdelta = true;
			break;
#endif
#ifdef ENABLE_MOUSE
		case ctrl_Mouse:
			IN_GetMouseDelta(&dx,&dy);
			buttons = IN_MouseButtons();
			realdelta = true;
			break;
#endif
		}

	if (realdelta)
	{
		mx = (dx < 0)? motion_Left : ((dx > 0)? motion_Right : motion_None);
		my = (dy < 0)? motion_Up : ((dy > 0)? motion_Down : motion_None);
	}
	else
	{
		dx = mx * 127;
		dy = my * 127;
	}

	info->x = dx;
	info->xaxis = mx;
	info->y = dy;
	info->yaxis = my;
	info->button0 = buttons & (1 << 0);
	info->button1 = buttons & (1 << 1);
	info->button2 = buttons & (1 << 2);
	info->button3 = buttons & (1 << 3);
	info->dir = DirTable[((my + 1) * 3) + (mx + 1)];
}
#endif

#ifndef EMBEDDED
///////////////////////////////////////////////////////////////////////////
//
//	IN_Ack() - waits for a button or key press.  If a button is down, upon
// calling, it must be released for it to be recognized
//
///////////////////////////////////////////////////////////////////////////

void IN_StartAck(void)
{
#ifndef KBD_ONLY
	unsigned	i,buttons;
#endif


//
// get initial state of everything
//
	IN_ClearKeysDown();
#ifndef KBD_ONLY
	memset (btnstate,0,sizeof(btnstate));

	buttons = IN_JoyButtons () << 4;
	if (MousePresent)
		buttons |= IN_MouseButtons ();

	for (i=0;i<8;i++,buttons>>=1)
		if (buttons&1)
			btnstate[i] = true;
#endif
}

boolean IN_CheckAck()
{	
#ifndef KBD_ONLY
	unsigned i, buttons;
#endif
	
	INL_Update();
		
	if (LastScan)
		return true;

#ifndef KBD_ONLY
	buttons = IN_JoyButtons () << 4;
	if (MousePresent)
		buttons |= IN_MouseButtons ();

	for (i=0;i<8;i++,buttons>>=1)
		if ( buttons&1 )
		{
			if (!btnstate[i])
				return true;
		}
		else
			btnstate[i]=false;
#endif

	return false;
}

void IN_Ack()
{
	IN_StartAck();

	while(!IN_CheckAck()) ;
}
#endif
