/*
Copyright (C) 1996-1997 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/
// in_win.c -- windows 95 mouse and joystick code
// 02/21/97 JCB Added extended DirectInput code to support external controllers.

#include "quakedef.h"
#include "winquake.h"
//#include "dosisms.h"
#ifndef WINRT
#define USINGRAWINPUT

#ifdef USINGRAWINPUT
#include "in_raw.h"
#endif

void INS_Accumulate (void);

#define AVAIL_XINPUT
#ifdef AVAIL_XINPUT
//#define AVAIL_XINPUT_DLL "xinput9_1_0.dll"
#define AVAIL_XINPUT_DLL "xinput1_3.dll"
typedef struct _XINPUT_GAMEPAD {
	WORD  wButtons;
	BYTE  bLeftTrigger;
	BYTE  bRightTrigger;
	SHORT sThumbLX;
	SHORT sThumbLY;
	SHORT sThumbRX;
	SHORT sThumbRY;
} XINPUT_GAMEPAD, *PXINPUT_GAMEPAD;
typedef struct _XINPUT_STATE {
	DWORD          dwPacketNumber;
	XINPUT_GAMEPAD Gamepad;
} XINPUT_STATE, *PXINPUT_STATE;
typedef struct _XINPUT_VIBRATION {
  WORD wLeftMotorSpeed;
  WORD wRightMotorSpeed;
} XINPUT_VIBRATION, *PXINPUT_VIBRATION;
DWORD (WINAPI *pXInputGetState)(DWORD dwUserIndex, XINPUT_STATE *pState);
DWORD (WINAPI *pXInputSetState)(DWORD dwUserIndex, XINPUT_VIBRATION *pState);
DWORD (WINAPI *pXInputGetDSoundAudioDeviceGuids)(DWORD dwUserIndex, GUID* pDSoundRenderGuid, GUID* pDSoundCaptureGuid);
enum
{
	XINPUT_GAMEPAD_DPAD_UP			= 0x0001,
	XINPUT_GAMEPAD_DPAD_DOWN		= 0x0002,
	XINPUT_GAMEPAD_DPAD_LEFT		= 0x0004,
	XINPUT_GAMEPAD_DPAD_RIGHT		= 0x0008,
	XINPUT_GAMEPAD_START			= 0x0010,
	XINPUT_GAMEPAD_BACK				= 0x0020,
	XINPUT_GAMEPAD_LEFT_THUMB		= 0x0040,
	XINPUT_GAMEPAD_RIGHT_THUMB		= 0x0080,
	XINPUT_GAMEPAD_LEFT_SHOULDER	= 0x0100,
	XINPUT_GAMEPAD_RIGHT_SHOULDER	= 0x0200,
	XINPUT_GAMEPAD_A				= 0x1000,
	XINPUT_GAMEPAD_B				= 0x2000,
	XINPUT_GAMEPAD_X				= 0x4000,
	XINPUT_GAMEPAD_Y				= 0x8000
};
static qboolean xinput_useaudio;
#endif


#ifdef AVAIL_DINPUT

#ifndef _MSC_VER
#define DIRECTINPUT_VERSION 0x0700
#endif

#include <dinput.h>

#define DINPUT_BUFFERSIZE           16
#define iDirectInputCreate(a,b,c,d)	pDirectInputCreate(a,b,c,d)

HRESULT (WINAPI *pDirectInputCreate)(HINSTANCE hinst, DWORD dwVersion,
	LPDIRECTINPUT * lplpDirectInput, LPUNKNOWN punkOuter);

#endif

#define DINPUT_VERSION_DX3 0x0300
#define DINPUT_VERSION_DX7 0x0700

// mouse variables
static cvar_t	in_dinput = CVARFD("in_dinput","0", CVAR_ARCHIVE, "Enables the use of dinput for mouse movements");
static cvar_t	in_xinput = CVARFD("in_xinput","0", CVAR_ARCHIVE, "Enables the use of xinput for controllers.\nNote that if you have a headset plugged in, that headset will be used for audio playback if no specific audio device is configured.");
static cvar_t	in_builtinkeymap = CVARF("in_builtinkeymap", "0", CVAR_ARCHIVE);
static cvar_t in_simulatemultitouch = CVAR("in_simulatemultitouch", "0");
static cvar_t	in_nonstandarddeadkeys = CVARD("in_nonstandarddeadkeys", "1", "Discard input events that result in multiple keys. Only the last key will be used. This results in behaviour that differs from eg notepad. To use a dead key, press it twice instead of the dead key followed by space.");

static cvar_t	xinput_leftvibrator = CVARFD("xinput_leftvibrator","0", CVAR_ARCHIVE, "");
static cvar_t	xinput_rightvibrator = CVARFD("xinput_rightvibrator","0", CVAR_ARCHIVE, "");

static cvar_t	m_accel_noforce = CVAR("m_accel_noforce", "0");
static cvar_t  m_threshold_noforce = CVAR("m_threshold_noforce", "0");

static cvar_t	cl_keypad = CVAR("cl_keypad", "1");

extern float multicursor_x[8], multicursor_y[8];
extern qboolean multicursor_active[8];

POINT		current_mouse_pos;

typedef struct {
	union {
		HANDLE rawinputhandle;
	} handles;
	char sysname[MAX_OSPATH];

	int qdeviceid;
} keyboard_t;

typedef struct {
	union {
		HANDLE rawinputhandle; // raw input
	} handles;
	char sysname[MAX_OSPATH];

	int numbuttons;
	int oldbuttons;
	int qdeviceid;	/*the device id controls which player slot it controls, if splitscreen splits it that way*/
} mouse_t;

static mouse_t sysmouse;

static qboolean	restore_spi;
static int		originalmouseparms[3], newmouseparms[3] = {0, 0, 0};
qboolean		mouseinitialized;
static qboolean	mouseparmsvalid, mouseactivatetoggle;
qboolean	mouseshowtoggle = 1;
unsigned int uiWheelMessage;

qboolean	mouseactive;

// joystick defines and variables
// where should defines be moved?
#define JOY_ABSOLUTE_AXIS	0x00000000		// control like a joystick
#define JOY_RELATIVE_AXIS	0x00000010		// control like a mouse, spinner, trackball
#define	JOY_MAX_AXES		6				// X, Y, Z, R, U, V


// none of these cvars are saved over a session
// this means that advanced controller configuration needs to be executed
// each time.  this avoids any problems with getting back to a default usage
// or when changing from one controller to another.  this way at least something
// works.
static cvar_t	in_joystick				= CVARAF("joystick","0", "in_joystick", CVAR_ARCHIVE);
static qboolean	joy_advancedinit;

static DWORD		joy_flags;
#define MAX_JOYSTICKS 8
static struct wjoy_s {
	qboolean			isxinput;	//xinput device
	unsigned int		id;		//windows id. device id is the index.
	unsigned int		devid;	//quake id (generally player index)
	DWORD	numbuttons;
	qboolean haspov;
	DWORD	povstate;
	DWORD	oldpovstate;
	DWORD	buttonstate;
	DWORD	oldbuttonstate;
	soundcardinfo_t *audiodev;
} wjoy[MAX_JOYSTICKS];
static int joy_count;

#ifdef AVAIL_DINPUT
static const GUID fGUID_XAxis	= {0xA36D02E0, 0xC9F3, 0x11CF, {0xBF, 0xC7, 0x44, 0x45, 0x53, 0x54, 0x00, 0x00}};
static const GUID fGUID_YAxis	= {0xA36D02E1, 0xC9F3, 0x11CF, {0xBF, 0xC7, 0x44, 0x45, 0x53, 0x54, 0x00, 0x00}};
static const GUID fGUID_ZAxis	= {0xA36D02E2, 0xC9F3, 0x11CF, {0xBF, 0xC7, 0x44, 0x45, 0x53, 0x54, 0x00, 0x00}};
static const GUID fGUID_SysMouse	= {0x6F1D2B60, 0xD5A0, 0x11CF, {0xBF, 0xC7, 0x44, 0x45, 0x53, 0x54, 0x00, 0x00}};

static const GUID fIID_IDirectInputDevice7A	= {0x57d7c6bc, 0x2356, 0x11d3, {0x8e, 0x9d, 0x00, 0xC0, 0x4f, 0x68, 0x44, 0xae}};
static const GUID fIID_IDirectInput7A		= {0x9a4cb684, 0x236d, 0x11d3, {0x8e, 0x9d, 0x00, 0xc0, 0x4f, 0x68, 0x44, 0xae}};

// devices
static LPDIRECTINPUT		g_pdi;
static LPDIRECTINPUTDEVICE	g_pMouse;
static qboolean	dinput_acquired;

static HINSTANCE hInstDI;

// current DirectInput version in use, 0 means using no DirectInput
static int dinput;

typedef struct MYDATA {
	LONG  lX;                   // X axis goes here
	LONG  lY;                   // Y axis goes here
	LONG  lZ;                   // Z axis goes here
	BYTE  bButtonA;             // One button goes here
	BYTE  bButtonB;             // Another button goes here
	BYTE  bButtonC;             // Another button goes here
	BYTE  bButtonD;             // Another button goes here
#if (DIRECTINPUT_VERSION >= DINPUT_VERSION_DX7)
	BYTE  bButtonE;             // DX7 buttons
	BYTE  bButtonF;
	BYTE  bButtonG;
	BYTE  bButtonH;
#endif
} MYDATA;

static DIOBJECTDATAFORMAT rgodf[] = {
  { &fGUID_XAxis,    FIELD_OFFSET(MYDATA, lX),       DIDFT_AXIS | DIDFT_ANYINSTANCE,   0,},
  { &fGUID_YAxis,    FIELD_OFFSET(MYDATA, lY),       DIDFT_AXIS | DIDFT_ANYINSTANCE,   0,},
  { &fGUID_ZAxis,    FIELD_OFFSET(MYDATA, lZ),       0x80000000 | DIDFT_AXIS | DIDFT_ANYINSTANCE,   0,},
  { 0,              FIELD_OFFSET(MYDATA, bButtonA), DIDFT_BUTTON | DIDFT_ANYINSTANCE, 0,},
  { 0,              FIELD_OFFSET(MYDATA, bButtonB), DIDFT_BUTTON | DIDFT_ANYINSTANCE, 0,},
  { 0,              FIELD_OFFSET(MYDATA, bButtonC), 0x80000000 | DIDFT_BUTTON | DIDFT_ANYINSTANCE, 0,},
  { 0,              FIELD_OFFSET(MYDATA, bButtonD), 0x80000000 | DIDFT_BUTTON | DIDFT_ANYINSTANCE, 0,},
#if (DIRECTINPUT_VERSION >= DINPUT_VERSION_DX7)
  { 0,              FIELD_OFFSET(MYDATA, bButtonE), 0x80000000 | DIDFT_BUTTON | DIDFT_ANYINSTANCE, 0,},
  { 0,              FIELD_OFFSET(MYDATA, bButtonF), 0x80000000 | DIDFT_BUTTON | DIDFT_ANYINSTANCE, 0,},
  { 0,              FIELD_OFFSET(MYDATA, bButtonG), 0x80000000 | DIDFT_BUTTON | DIDFT_ANYINSTANCE, 0,},
  { 0,              FIELD_OFFSET(MYDATA, bButtonH), 0x80000000 | DIDFT_BUTTON | DIDFT_ANYINSTANCE, 0,},
#endif
};

#define NUM_OBJECTS (sizeof(rgodf) / sizeof(rgodf[0]))

static DIDATAFORMAT	df = {
	sizeof(DIDATAFORMAT),       // this structure
	sizeof(DIOBJECTDATAFORMAT), // size of object data format
	DIDF_RELAXIS,               // absolute axis coordinates
	sizeof(MYDATA),             // device data size
	NUM_OBJECTS,                // number of objects
	rgodf,                      // and here they are
};

#if (DIRECTINPUT_VERSION >= DINPUT_VERSION_DX7)
// DX7 devices
static LPDIRECTINPUT7		g_pdi7;
static LPDIRECTINPUTDEVICE7	g_pMouse7;

// DX7 specific calls
#define iDirectInputCreateEx(a,b,c,d,e)	pDirectInputCreateEx(a,b,c,d,e)

static HRESULT (WINAPI *pDirectInputCreateEx)(HINSTANCE hinst,
		DWORD dwVersion, REFIID riidltf, LPVOID *ppvOut, LPUNKNOWN punkOuter);
#endif

#else
#define dinput 0
#endif

static JOYINFOEX	ji;

// raw input specific defines
#ifdef USINGRAWINPUT
// defines
#define MAX_RI_DEVICE_SIZE 128
#define INIT_RIBUFFER_SIZE (sizeof(RAWINPUTHEADER)+sizeof(RAWMOUSE))

#define RI_RAWBUTTON_MASK 0x000003E0
#define RI_INVALID_POS    0x80000000

// raw input dynamic functions
typedef INT (WINAPI *pGetRawInputDeviceList)(OUT PRAWINPUTDEVICELIST pRawInputDeviceList, IN OUT PINT puiNumDevices, IN UINT cbSize);
typedef INT(WINAPI *pGetRawInputData)(IN HRAWINPUT hRawInput, IN UINT uiCommand, OUT LPVOID pData, IN OUT PINT pcbSize, IN UINT cbSizeHeader);
typedef INT(WINAPI *pGetRawInputDeviceInfoA)(IN HANDLE hDevice, IN UINT uiCommand, OUT LPVOID pData, IN OUT PINT pcbSize);
typedef BOOL (WINAPI *pRegisterRawInputDevices)(IN PCRAWINPUTDEVICE pRawInputDevices, IN UINT uiNumDevices, IN UINT cbSize);

static pGetRawInputDeviceList _GRIDL;
static pGetRawInputData _GRID;
static pGetRawInputDeviceInfoA _GRIDIA;
static pRegisterRawInputDevices _RRID;

static keyboard_t *rawkbd;
static mouse_t *rawmice;
static int rawmicecount;
static int rawkbdcount;
static RAWINPUT *raw;
static int ribuffersize;

static cvar_t in_rawinput_mice = CVARD("in_rawinput", "0", "Enables rawinput support for mice in XP onwards. Rawinput permits independant device identification (ie: splitscreen clients can each have their own mouse)");
static cvar_t in_rawinput_keyboard = CVARD("in_rawinput_keyboard", "0", "Enables rawinput support for keyboards in XP onwards as well as just mice.");
static cvar_t in_rawinput_rdp = CVARD("in_rawinput_rdp", "0", "Activate Remote Desktop Protocol devices too.");

void INS_RawInput_MouseDeRegister(void);
int INS_RawInput_MouseRegister(void);
void INS_RawInput_KeyboardDeRegister(void);
int INS_RawInput_KeyboardRegister(void);
void INS_RawInput_DeInit(void);

#endif

// forward-referenced functions
void INS_StartupJoystick (void);
void INS_JoyMove (float *movements, int pnum);

/*
===========
Force_CenterView_f
===========
*/
void Force_CenterView_f (void)
{
	cl.playerview[0].viewangles[PITCH] = 0;
}

/*
===========
INS_UpdateClipCursor
===========
*/
void INS_UpdateClipCursor (void)
{
	if (mouseinitialized && mouseactive && !dinput)
	{
		ClipCursor (&window_rect);
	}
}


/*
===========
INS_ShowMouse
===========
*/
static void INS_ShowMouse (void)
{
	if (!mouseshowtoggle)
	{
		ShowCursor (TRUE);
		mouseshowtoggle = 1;
	}
}


/*
===========
INS_HideMouse
===========
*/
static void INS_HideMouse (void)
{
	if (mouseshowtoggle)
	{
		ShowCursor (FALSE);
		mouseshowtoggle = 0;
	}
}

//scan for an unused device id for joysticks (now that something was pressed).
static int Joy_AllocateDevID(void)
{
	extern cvar_t cl_splitscreen;
	int id = 0, j;
	for (id = 0; ; id++)
	{
		for (j = 0; j < joy_count; j++)
		{
			if (wjoy[j].devid == id)
				break;
		}
		if (j == joy_count)
		{
			if (id > cl_splitscreen.ival && !*cl_splitscreen.string)
				cl_splitscreen.ival = id;
			return id;
		}
	}
}
#ifdef USINGRAWINPUT
static int Mouse_AllocateDevID(void)
{
	extern cvar_t cl_splitscreen;
	int id = 0, j;
	for (id = 0; ; id++)
	{
		for (j = 0; j < rawmicecount; j++)
		{
			if (rawmice[j].qdeviceid == id)
				break;
		}
		if (j == rawmicecount)
		{
			if (id > cl_splitscreen.ival && !*cl_splitscreen.string)
				cl_splitscreen.ival = id;
			return id;
		}
	}
}
static int Keyboard_AllocateDevID(void)
{
	extern cvar_t cl_splitscreen;
	int id = 0, j;
	for (id = 0; ; id++)
	{
		for (j = 0; j < rawkbdcount; j++)
		{
			if (rawkbd[j].qdeviceid == id)
				break;
		}
		if (j == rawkbdcount)
		{
			if (id > cl_splitscreen.ival && !*cl_splitscreen.string)
				cl_splitscreen.ival = id;
			return id;
		}
	}
}
#endif

/*
===========
INS_ActivateMouse
===========
*/
static void INS_ActivateMouse (void)
{
	mouseactivatetoggle = true;

	if (mouseinitialized && !mouseactive)
	{
#ifdef AVAIL_DINPUT
#if (DIRECTINPUT_VERSION >= DINPUT_VERSION_DX7)
		if (dinput >= DINPUT_VERSION_DX7)
		{
			if (g_pMouse7)
			{
				if (!dinput_acquired)
				{
					IDirectInputDevice7_Acquire(g_pMouse7);
					dinput_acquired = true;
				}
			}
			else
			{
				return;
			}
		}
		else
#endif
		if (dinput)
		{
			if (g_pMouse)
			{
				if (!dinput_acquired)
				{
					IDirectInputDevice_Acquire(g_pMouse);
					dinput_acquired = true;
				}
			}
			else
			{
				return;
			}
		}
		else
#endif
		{
#ifdef USINGRAWINPUT
			if (rawmicecount > 0)
			{
				if (INS_RawInput_MouseRegister())
				{
					Con_SafePrintf("Raw input: unable to register raw input for mice, deinitializing\n");
					INS_RawInput_MouseDeRegister();
				}
			}
			if (rawkbdcount > 0)
			{
				if (INS_RawInput_KeyboardRegister())
				{
					Con_SafePrintf("Raw input: unable to register raw input for keyboard, deinitializing\n");
					INS_RawInput_KeyboardDeRegister();
				}
			}
#endif

			if (mouseparmsvalid)
			{
				SystemParametersInfo (SPI_SETMOUSE, 0, newmouseparms, 0);
				restore_spi = true;
			}

			SetCursorPos (window_center_x, window_center_y);
			SetCapture (mainwindow);
			ClipCursor (&window_rect);
		}

		mouseactive = true;
	}
}


/*
===========
INS_DeactivateMouse
===========
*/
static void INS_DeactivateMouse (void)
{
	mouseactivatetoggle = false;

	if (mouseinitialized && mouseactive)
	{
#ifdef AVAIL_DINPUT
#if (DIRECTINPUT_VERSION >= DINPUT_VERSION_DX7)
		if (dinput >= DINPUT_VERSION_DX7)
		{
			if (g_pMouse7)
			{
				if (dinput_acquired)
				{
					IDirectInputDevice_Unacquire(g_pMouse7);
					dinput_acquired = false;
				}
			}
			else
			{
				return;
			}
		}
		else
#endif
		if (dinput)
		{
			if (g_pMouse)
			{
				if (dinput_acquired)
				{
					IDirectInputDevice_Unacquire(g_pMouse);
					dinput_acquired = false;
				}
			}
		}
		else
#endif
		{
#ifdef USINGRAWINPUT
			if (rawmicecount > 0)
				INS_RawInput_MouseDeRegister();
#endif

			if (restore_spi)
				SystemParametersInfo (SPI_SETMOUSE, 0, originalmouseparms, 0);

			ClipCursor (NULL);
			ReleaseCapture ();
		}

		mouseactive = false;
	}
}

/*
===========
INS_SetQuakeMouseState
===========
*/
void INS_SetQuakeMouseState (void)
{
	if (mouseactivatetoggle)
		INS_ActivateMouse ();
	else
		INS_DeactivateMouse();
}

/*
===========
INS_RestoreOriginalMouseState
===========
*/
void INS_RestoreOriginalMouseState (void)
{
	if (mouseactivatetoggle)
	{
		INS_DeactivateMouse ();
		mouseactivatetoggle = true;
	}

// try to redraw the cursor so it gets reinitialized, because sometimes it
// has garbage after the mode switch
	ShowCursor (TRUE);
	ShowCursor (FALSE);
}




void INS_UpdateGrabs(int fullscreen, int activeapp)
{
	int grabmouse;

	if (!activeapp)
		grabmouse = false;
	else if (fullscreen || in_simulatemultitouch.ival || _windowed_mouse.value)
	{
		if (!Key_MouseShouldBeFree())
			grabmouse = true;
		else
			grabmouse = false;
	}
	else
		grabmouse = false;

	if (activeapp)
	{
		if (
#ifdef TEXTEDITOR
			!editormodal &&
#endif
			!SCR_HardwareCursorIsActive() && (fullscreen || in_simulatemultitouch.ival || _windowed_mouse.value) && (fullscreen || (current_mouse_pos.x >= window_rect.left && current_mouse_pos.y >= window_rect.top && current_mouse_pos.x <= window_rect.right && current_mouse_pos.y <= window_rect.bottom)))
		{
			INS_HideMouse();
		}
		else 
		{
			INS_ShowMouse();
			grabmouse = false;
		}
	}
	else
	{
		INS_ShowMouse();
		grabmouse = false;
	}

#ifdef HLCLIENT
	//halflife gamecode does its own mouse control... yes this is vile.
	if (grabmouse)
	{
		if (CLHL_GamecodeDoesMouse())
			grabmouse = 2;
	}

	if (grabmouse == 2)
	{
		INS_DeactivateMouse();
		CLHL_SetMouseActive(true);
		return;
	}

	CLHL_SetMouseActive(false);
#endif

	if (grabmouse)
		INS_ActivateMouse();
	else
		INS_DeactivateMouse();
}






#ifdef AVAIL_DINPUT
BOOL CALLBACK INS_EnumerateDI7Devices(LPCDIDEVICEINSTANCE inst, LPVOID parm)
{
	Con_DPrintf("EnumerateDevices found: %s\n", inst->tszProductName);

	return DIENUM_CONTINUE;
}
/*
===========
INS_InitDInput
===========
*/
int INS_InitDInput (void)
{
	HRESULT		hr;
	DIPROPDWORD	dipdw = {
		{
			sizeof(DIPROPDWORD),        // diph.dwSize
			sizeof(DIPROPHEADER),       // diph.dwHeaderSize
			0,                          // diph.dwObj
			DIPH_DEVICE,                // diph.dwHow
		},
		DINPUT_BUFFERSIZE,              // dwData
	};

	if (!hInstDI)
	{
		hInstDI = Sys_LoadLibrary("dinput.dll", NULL);

		if (hInstDI == NULL)
		{
			Con_SafePrintf ("Couldn't load dinput.dll\n");
			return 0;
		}
	}

#if (DIRECTINPUT_VERSION >= DINPUT_VERSION_DX7)
	if (!pDirectInputCreateEx)
		pDirectInputCreateEx = (void *)GetProcAddress(hInstDI,"DirectInputCreateEx");

	if (pDirectInputCreateEx) // use DirectInput 7
	{
		// register with DirectInput and get an IDirectInput to play with.
		hr = iDirectInputCreateEx(global_hInstance, DINPUT_VERSION_DX7, &fIID_IDirectInput7A, (void**)&g_pdi7, NULL);

		if (FAILED(hr))
			return 0;

		IDirectInput7_EnumDevices(g_pdi7, 0, &INS_EnumerateDI7Devices, NULL, DIEDFL_ATTACHEDONLY);

		// obtain an interface to the system mouse device.
		hr = IDirectInput7_CreateDeviceEx(g_pdi7, &fGUID_SysMouse, &fIID_IDirectInputDevice7A, (void**)&g_pMouse7, NULL);

		if (FAILED(hr)) {
			Con_SafePrintf ("Couldn't open DI7 mouse device\n");
			return 0;
		}

		// set the data format to "mouse format".
		hr = IDirectInputDevice7_SetDataFormat(g_pMouse7, &df);

		if (FAILED(hr)) {
			Con_SafePrintf ("Couldn't set DI7 mouse format\n");
			return 0;
		}

		// set the cooperativity level.
		hr = IDirectInputDevice7_SetCooperativeLevel(g_pMouse7, mainwindow,
			DISCL_EXCLUSIVE | DISCL_FOREGROUND);

		if (FAILED(hr)) {
			Con_SafePrintf ("Couldn't set DI7 coop level\n");
			return 0;
		}

		// set the buffer size to DINPUT_BUFFERSIZE elements.
		// the buffer size is a DWORD property associated with the device
		hr = IDirectInputDevice7_SetProperty(g_pMouse7, DIPROP_BUFFERSIZE, &dipdw.diph);

		if (FAILED(hr)) {
			Con_SafePrintf ("Couldn't set DI7 buffersize\n");
			return 0;
		}

		return DINPUT_VERSION_DX7;
	}
#endif

	if (!pDirectInputCreate)
	{
		pDirectInputCreate = (void *)GetProcAddress(hInstDI,"DirectInputCreateA");

		if (!pDirectInputCreate)
		{
			Con_SafePrintf ("Couldn't get DI3 proc addr\n");
			return 0;
		}
	}

// register with DirectInput and get an IDirectInput to play with.
	hr = iDirectInputCreate(global_hInstance, DINPUT_VERSION_DX3, &g_pdi, NULL);

	if (FAILED(hr))
	{
		return 0;
	}
	IDirectInput_EnumDevices(g_pdi, 0, &INS_EnumerateDI7Devices, NULL, DIEDFL_ATTACHEDONLY);

// obtain an interface to the system mouse device.
	hr = IDirectInput_CreateDevice(g_pdi, &fGUID_SysMouse, &g_pMouse, NULL);

	if (FAILED(hr))
	{
		Con_SafePrintf ("Couldn't open DI3 mouse device\n");
		return 0;
	}

// set the data format to "mouse format".
	hr = IDirectInputDevice_SetDataFormat(g_pMouse, &df);

	if (FAILED(hr))
	{
		Con_SafePrintf ("Couldn't set DI3 mouse format\n");
		return 0;
	}

// set the cooperativity level.
	hr = IDirectInputDevice_SetCooperativeLevel(g_pMouse, mainwindow,
			DISCL_EXCLUSIVE | DISCL_FOREGROUND);

	if (FAILED(hr))
	{
		Con_SafePrintf ("Couldn't set DI3 coop level\n");
		return 0;
	}


// set the buffer size to DINPUT_BUFFERSIZE elements.
// the buffer size is a DWORD property associated with the device
	hr = IDirectInputDevice_SetProperty(g_pMouse, DIPROP_BUFFERSIZE, &dipdw.diph);

	if (FAILED(hr))
	{
		Con_SafePrintf ("Couldn't set DI3 buffersize\n");
		return 0;
	}

	return DINPUT_VERSION_DX3;
}

void INS_CloseDInput (void)
{
#if (DIRECTINPUT_VERSION >= DINPUT_VERSION_DX7)
	if (g_pMouse7)
	{
		IDirectInputDevice7_Release(g_pMouse7);
		g_pMouse7 = NULL;
	}
	if (g_pdi7)
	{
		IDirectInput7_Release(g_pdi7);
		g_pdi7 = NULL;
	}
	pDirectInputCreateEx = NULL;
#endif
	if (g_pMouse)
	{
		IDirectInputDevice_Release(g_pMouse);
		g_pMouse = NULL;
	}
	if (g_pdi)
	{
		IDirectInput_Release(g_pdi);
		g_pdi = NULL;
	}
	if (hInstDI)
	{
		FreeLibrary(hInstDI);
		hInstDI = NULL;
		pDirectInputCreate = NULL;
	}

}
#endif

#ifdef USINGRAWINPUT
void INS_RawInput_MouseDeRegister(void)
{
	RAWINPUTDEVICE Rid;

	// deregister raw input
	Rid.usUsagePage = 0x01;
	Rid.usUsage = 0x02;
	Rid.dwFlags = RIDEV_REMOVE;
	Rid.hwndTarget = NULL;

	(*_RRID)(&Rid, 1, sizeof(Rid));
}

void INS_RawInput_KeyboardDeRegister(void)
{
	RAWINPUTDEVICE Rid;

	// deregister raw input
	Rid.usUsagePage = 0x01;
	Rid.usUsage = 0x06;
	Rid.dwFlags = RIDEV_REMOVE;
	Rid.hwndTarget = NULL;

	(*_RRID)(&Rid, 1, sizeof(Rid));
}

void INS_RawInput_DeInit(void)
{
	if (rawmicecount > 0)
		INS_RawInput_MouseDeRegister();
	if (rawkbdcount > 0)
		INS_RawInput_KeyboardDeRegister();
	rawmicecount = 0;
	rawkbdcount = 0;
	Z_Free(rawmice);
	rawmice = NULL;
	Z_Free(rawkbd);
	rawkbd = NULL;
	Z_Free(raw);
	raw = NULL;
	memset(multicursor_active, 0, sizeof(multicursor_active));
}
#endif

#ifdef USINGRAWINPUT
// raw input registration functions
int INS_RawInput_MouseRegister(void)
{
	// This function registers to receive the WM_INPUT messages
	RAWINPUTDEVICE Rid; // Register only for mouse messages from wm_input.

	//register to get wm_input messages
	Rid.usUsagePage = 0x01;
	Rid.usUsage = 0x02;
	Rid.dwFlags = RIDEV_NOLEGACY; // adds HID mouse and also ignores legacy mouse messages
	Rid.hwndTarget = NULL;

	// Register to receive the WM_INPUT message for any change in mouse (buttons, wheel, and movement will all generate the same message)
	if (!(*_RRID)(&Rid, 1, sizeof(Rid)))
		return 1;

	return 0;
}

int INS_RawInput_KeyboardRegister(void)
{
	RAWINPUTDEVICE Rid;

	Rid.usUsagePage = 0x01;
	Rid.usUsage = 0x06;
	Rid.dwFlags = RIDEV_NOLEGACY | RIDEV_APPKEYS | RIDEV_NOHOTKEYS; // fetch everything, disable hotkey behavior (should cvar?)
	Rid.hwndTarget = NULL;

	if (!(*_RRID)(&Rid, 1, sizeof(Rid)))
		return 1;

	return 0;
}

int INS_RawInput_Register(void)
{
	if (INS_RawInput_MouseRegister())
		return !in_rawinput_keyboard.ival || INS_RawInput_KeyboardRegister();
	return 0;
}

int INS_RawInput_IsRDPDevice(char *cDeviceString)
{
	// mouse is \\?\Root#RDP_MOU#, keyboard is \\?\Root#RDP_KBD#"
	char cRDPString[] = "\\\\?\\Root#RDP_";
	int i;

	if (strlen(cDeviceString) < strlen(cRDPString)) {
		return 0;
	}

	for (i = strlen(cRDPString) - 1; i >= 0; i--)
	{
		if (cDeviceString[i] != cRDPString[i])
			return 0;
	}

	return 1;
}

void INS_RawInput_Init(void)
{
	  // "0" to exclude, "1" to include
	PRAWINPUTDEVICELIST pRawInputDeviceList;
	int inputdevices, i, j, mtemp, ktemp;
	char dname[MAX_RI_DEVICE_SIZE];

	// Return 0 if rawinput is not available
	HMODULE user32 = LoadLibrary("user32.dll");
	if (!user32)
	{
		Con_SafePrintf("Raw input: unable to load user32.dll\n");
		return;
	}
	_RRID = (pRegisterRawInputDevices)GetProcAddress(user32,"RegisterRawInputDevices");
	if (!_RRID)
	{
		Con_SafePrintf("Raw input: function RegisterRawInputDevices is not available\n");
		return;
	}
	_GRIDL = (pGetRawInputDeviceList)GetProcAddress(user32,"GetRawInputDeviceList");
	if (!_GRIDL)
	{
		Con_SafePrintf("Raw input: function GetRawInputDeviceList is not available\n");
		return;
	}
	_GRIDIA = (pGetRawInputDeviceInfoA)GetProcAddress(user32,"GetRawInputDeviceInfoA");
	if (!_GRIDIA)
	{
		Con_SafePrintf("Raw input: function GetRawInputDeviceInfoA is not available\n");
		return;
	}
	_GRID = (pGetRawInputData)GetProcAddress(user32,"GetRawInputData");
	if (!_GRID)
	{
		Con_SafePrintf("Raw input: function GetRawInputData is not available\n");
		return;
	}

	rawmicecount = 0;
	rawmice = NULL;
	raw = NULL;
	ribuffersize = 0;

	// 1st call to GetRawInputDeviceList: Pass NULL to get the number of devices.
	if ((*_GRIDL)(NULL, &inputdevices, sizeof(RAWINPUTDEVICELIST)) != 0)
	{
		Con_SafePrintf("Raw input: unable to count raw input devices\n");
		return;
	}

	// Allocate the array to hold the DeviceList
	pRawInputDeviceList = Z_Malloc(sizeof(RAWINPUTDEVICELIST) * inputdevices);

	// 2nd call to GetRawInputDeviceList: Pass the pointer to our DeviceList and GetRawInputDeviceList() will fill the array
	if ((*_GRIDL)(pRawInputDeviceList, &inputdevices, sizeof(RAWINPUTDEVICELIST)) == -1)
	{
		Con_SafePrintf("Raw input: unable to get raw input device list\n");
		return;
	}

	// Loop through all devices and count the mice
	for (i = 0, mtemp = 0, ktemp = 0; i < inputdevices; i++)
	{
		j = MAX_RI_DEVICE_SIZE;

		// Get the device name and use it to determine if it's the RDP Terminal Services virtual device.
		if ((*_GRIDIA)(pRawInputDeviceList[i].hDevice, RIDI_DEVICENAME, dname, &j) < 0)
			dname[0] = 0;

		if (!(in_rawinput_rdp.value) && INS_RawInput_IsRDPDevice(dname)) // use rdp (cvar)
			continue;

		switch (pRawInputDeviceList[i].dwType)
		{
		case RIM_TYPEMOUSE:
			if (!in_rawinput_mice.ival)
				continue;

			mtemp++;
			break;
		case RIM_TYPEKEYBOARD:
			if (!in_rawinput_keyboard.ival)
				continue;

			ktemp++;
			break;
		default: // (RIM_TYPEHID) support joysticks?
			break;
		}
	}

	// exit out if no devices found
	if (!mtemp && !ktemp)
	{
		Con_SafePrintf("Raw input: no usable device found\n");
		return;
	}

	// Loop again and bind devices
	rawmice = (mouse_t *)Z_Malloc(sizeof(mouse_t) * mtemp);
	rawkbd = (keyboard_t *)Z_Malloc(sizeof(keyboard_t) * ktemp);
	for (i = 0; i < inputdevices; i++)
	{
		j = MAX_RI_DEVICE_SIZE;

		// Get the device name and use it to determine if it's the RDP Terminal Services virtual device.
		if ((*_GRIDIA)(pRawInputDeviceList[i].hDevice, RIDI_DEVICENAME, dname, &j) < 0)
			dname[0] = 0;

		if (!(in_rawinput_rdp.value) && INS_RawInput_IsRDPDevice(dname)) // use rdp (cvar)
			continue;

		switch (pRawInputDeviceList[i].dwType)
		{
		case RIM_TYPEMOUSE:
			if (!in_rawinput_mice.ival)
				continue;

			// set handle
			rawmice[rawmicecount].handles.rawinputhandle = pRawInputDeviceList[i].hDevice;
			Q_strncpyz(rawmice[rawmicecount].sysname, dname, sizeof(rawmice[rawmicecount].sysname));
			rawmice[rawmicecount].numbuttons = 16;
			rawmice[rawmicecount].oldbuttons = 0;
			rawmice[rawmicecount].qdeviceid = DEVID_UNSET;
			rawmicecount++;
			break;
		case RIM_TYPEKEYBOARD:
			if (!in_rawinput_keyboard.ival)
				continue;

			rawkbd[rawkbdcount].handles.rawinputhandle = pRawInputDeviceList[i].hDevice;
			Q_strncpyz(rawkbd[rawkbdcount].sysname, dname, sizeof(rawkbd[rawkbdcount].sysname));
			rawkbd[rawkbdcount].qdeviceid = DEVID_UNSET;
			rawkbdcount++;
			break;
		default:
			continue;
		}

		// print pretty message about device
		dname[MAX_RI_DEVICE_SIZE - 1] = 0;
		for (mtemp = strlen(dname); mtemp >= 0; mtemp--)
		{
			if (dname[mtemp] == '#')
			{
				dname[mtemp + 1] = 0;
				break;
			}
		}
		Con_DPrintf("Raw input type %i: [%i] %s\n", (int)pRawInputDeviceList[i].dwType, i, dname);
	}


	// free the RAWINPUTDEVICELIST
	Z_Free(pRawInputDeviceList);

	// alloc raw input buffer
	raw = BZ_Malloc(INIT_RIBUFFER_SIZE);
	ribuffersize = INIT_RIBUFFER_SIZE;

	Con_SafePrintf("Raw input: initialized with %i mice and %i keyboards\n", rawmicecount, rawkbdcount);

	return; // success
}
#endif

/*
===========
INS_StartupMouse
===========
*/
void INS_StartupMouse (void)
{
	if ( COM_CheckParm ("-nomouse") )
		return;

	mouseinitialized = true;

	//make sure it can't get stuck
	INS_DeactivateMouse ();

#ifdef AVAIL_DINPUT
	if (in_dinput.value)
	{
		dinput = INS_InitDInput ();

		if (dinput)
		{
			Con_SafePrintf ("DirectInput initialized, version %i\n", (dinput >> 8 & 0xFF));
		}
		else
		{
			Con_SafePrintf ("DirectInput not initialized\n");
		}
	}
	else
		dinput = 0;

	if (!dinput)
#endif
	{
		if (!mouseparmsvalid)
			mouseparmsvalid = SystemParametersInfo (SPI_GETMOUSE, 0, originalmouseparms, 0);

		if (mouseparmsvalid)
		{
			if ( m_accel_noforce.value )
				newmouseparms[2] = originalmouseparms[2];

			if ( m_threshold_noforce.value )
			{
				newmouseparms[0] = originalmouseparms[0];
				newmouseparms[1] = originalmouseparms[1];
			}
		}
	}

	sysmouse.numbuttons = 10;

// if a fullscreen video mode was set before the mouse was initialized,
// set the mouse state appropriately
	if (mouseactivatetoggle)
		INS_ActivateMouse ();
}


/*
===========
INS_Init
===========
*/
void INS_ReInit (void)
{
#ifdef USINGRAWINPUT
	if (in_rawinput_mice.ival || in_rawinput_keyboard.ival)
	{
		INS_RawInput_Init();
	}
#endif

	INS_StartupMouse ();
	INS_StartupJoystick ();
//	INS_ActivateMouse();
}

void INS_Init (void)
{
	//keyboard variables
	Cvar_Register (&cl_keypad, "Input Controls");

#ifdef AVAIL_DINPUT
	Cvar_Register (&in_dinput, "Input Controls");
#endif
#ifdef AVAIL_XINPUT
	Cvar_Register (&in_xinput, "Input Controls");
	Cvar_Register (&xinput_leftvibrator, "Input Controls");
	Cvar_Register (&xinput_rightvibrator, "Input Controls");
#endif
	Cvar_Register (&in_builtinkeymap, "Input Controls");
	Cvar_Register (&in_nonstandarddeadkeys, "Input Controls");
	Cvar_Register (&in_simulatemultitouch, "Input Controls");

	Cvar_Register (&m_accel_noforce, "Input Controls");
	Cvar_Register (&m_threshold_noforce, "Input Controls");

	// this looks strange but quake cmdline definitions
	// and MS documentation don't agree with each other
	if (COM_CheckParm ("-noforcemspd"))
		Cvar_Set(&m_accel_noforce, "1");

	if (COM_CheckParm ("-noforcemaccel"))
		Cvar_Set(&m_threshold_noforce, "1");

	if (COM_CheckParm ("-noforcemparms"))
	{
		Cvar_Set(&m_accel_noforce, "1");
		Cvar_Set(&m_threshold_noforce, "1");
	}

	if (COM_CheckParm ("-dinput"))
		Cvar_Set(&in_dinput, "1");

	// joystick variables
	Cvar_Register (&in_joystick, "Joystick variables");

	Cmd_AddCommand ("force_centerview", Force_CenterView_f);

	uiWheelMessage = RegisterWindowMessageA ( "MSWHEEL_ROLLMSG" );

#ifdef USINGRAWINPUT
	Cvar_Register (&in_rawinput_mice, "Input Controls");
	Cvar_Register (&in_rawinput_keyboard, "Input Controls");
	Cvar_Register (&in_rawinput_rdp, "Input Controls");
#endif
}

/*
===========
INS_Shutdown
===========
*/
void INS_Shutdown (void)
{
	INS_DeactivateMouse ();
	INS_ShowMouse ();

	mouseinitialized = false;
	mouseparmsvalid = false;

#ifdef AVAIL_DINPUT
	INS_CloseDInput();
#endif
#ifdef USINGRAWINPUT
	INS_RawInput_DeInit();
#endif
}


/*
===========
INS_MouseEvent
===========
a mouse button was pressed/released, mstate is the current set of buttons pressed.
*/
void INS_MouseEvent (int mstate)
{
	int		i;

	if (dinput && mouseactive)
		return;

#ifdef HLCLIENT
	if (CLHL_MouseEvent(mstate))
		return;
#endif

	if (1)//mouseactive || (key_dest != key_game))
	{
	// perform button actions
		for (i=0 ; i<sysmouse.numbuttons ; i++)
		{
			if ( (mstate & (1<<i)) &&
				!(sysmouse.oldbuttons & (1<<i)) )
			{
				IN_KeyEvent (sysmouse.qdeviceid, true, K_MOUSE1 + i, 0);
			}

			if ( !(mstate & (1<<i)) &&
				(sysmouse.oldbuttons & (1<<i)) )
			{
				IN_KeyEvent (sysmouse.qdeviceid, false, K_MOUSE1 + i, 0);
			}
		}

		sysmouse.oldbuttons = mstate;
	}
}

/*
===========
INS_MouseMove
===========
*/
void INS_MouseMove (float *movements, int pnum)
{
#ifdef AVAIL_DINPUT
	if (dinput && mouseactive)
	{
		DIDEVICEOBJECTDATA	od;
		DWORD				dwElements;
		HRESULT				hr;
		int xd = 0, yd = 0, zd = 0;

		for (;;)
		{
			dwElements = 1;

#if (DIRECTINPUT_VERSION >= DINPUT_VERSION_DX7)
			if (dinput >= DINPUT_VERSION_DX7)
			{
				hr = IDirectInputDevice7_GetDeviceData(g_pMouse7,
						sizeof(DIDEVICEOBJECTDATA), &od, &dwElements, 0);

				if ((hr == DIERR_INPUTLOST) || (hr == DIERR_NOTACQUIRED))
				{
					dinput_acquired = true;
					IDirectInputDevice7_Acquire(g_pMouse7);
					break;
				}
			}
			else
#endif
			{
				hr = IDirectInputDevice_GetDeviceData(g_pMouse,
						sizeof(DIDEVICEOBJECTDATA), &od, &dwElements, 0);

				if ((hr == DIERR_INPUTLOST) || (hr == DIERR_NOTACQUIRED))
				{
					dinput_acquired = true;
					IDirectInputDevice_Acquire(g_pMouse);
					break;
				}
			}

			/* Unable to read data or no data available */
			if (FAILED(hr) || dwElements == 0)
			{
				break;
			}

			/* Look at the element to see what happened */

			switch (od.dwOfs)
			{
				case DIMOFS_X:
					xd += od.dwData;
					break;

				case DIMOFS_Y:
					yd += od.dwData;
					break;

				case DIMOFS_Z:
					zd += od.dwData;
					break;

				case DIMOFS_BUTTON0:
				case DIMOFS_BUTTON1:
				case DIMOFS_BUTTON2:
				case DIMOFS_BUTTON3:
#if (DIRECTINPUT_VERSION >= DINPUT_VERSION_DX7)
				case DIMOFS_BUTTON4:
				case DIMOFS_BUTTON5:
				case DIMOFS_BUTTON6:
				case DIMOFS_BUTTON7:
#endif
					IN_KeyEvent(sysmouse.qdeviceid, (od.dwData & 0x80)?true:false, K_MOUSE1 + ((od.dwOfs - DIMOFS_BUTTON0) / (DIMOFS_BUTTON1-DIMOFS_BUTTON0)), 0);
					break;
			}
		}
		if (xd || yd || zd)
			IN_MouseMove(sysmouse.qdeviceid, false, xd, yd, zd, 0);
	}
	else
#endif
	{
		INS_Accumulate();
	}
}


/*
===========
INS_Move
===========
*/
void INS_Move (float *movements, int pnum)
{
	if (vid.activeapp && !Minimized)
	{
		INS_MouseMove (movements, pnum);
		INS_JoyMove (movements, pnum);
	}
	else
		INS_Accumulate();
}


/*
===========
INS_Accumulate
===========
potentially called multiple times per frame.
*/
void INS_Accumulate (void)
{
	if (mouseactive && !dinput)
	{
		//if you have two programs side by side that both think that they own the mouse cursor then there are certain race conditions when switching between them
		//when alt+tabbing, windows won't wait for a response, so we may have already lost focus by the time we get here, and none of our internal state will know about it.
		//fte won't grab the mouse until its actually inside the window, but other programs don't have similar delays.
		//so when switching to other quake ports, expect fte to detect the oter engine's setcursorpos as a really big mouse move.

		RECT cliprect;
		if (GetClipCursor (&cliprect) && !(
			cliprect.left >= window_rect.left && 
			cliprect.right <= window_rect.right && 
			cliprect.top >= window_rect.top && 
			cliprect.bottom <= window_rect.bottom
			))
			;	//cliprect now covers some area outside of where we asked for.
		else
#ifdef USINGRAWINPUT
		//raw input disables the system mouse, to avoid dupes
		if (!rawmicecount)
#endif
		{
			GetCursorPos (&current_mouse_pos);
			SetCursorPos (window_center_x, window_center_y);

			if (current_mouse_pos.x - window_center_x || current_mouse_pos.y - window_center_y)
				IN_MouseMove(sysmouse.qdeviceid, false, current_mouse_pos.x - window_center_x, current_mouse_pos.y - window_center_y, 0, 0);
		}
		else
		{
			// force the mouse to the center, so there's room to move (rawinput ignore this apparently)
			SetCursorPos (window_center_x, window_center_y);
		}
	}

	if (!mouseactive)
	{
		extern int window_x, window_y;
		GetCursorPos (&current_mouse_pos);

		IN_MouseMove(sysmouse.qdeviceid, true, current_mouse_pos.x-window_x, current_mouse_pos.y-window_y, 0, 0);
		return;
	}
}

#ifdef USINGRAWINPUT
void INS_RawInput_MouseRead(void)
{
	int i, tbuttons, j;
	mouse_t *mouse;

	// find mouse in our mouse list
	for (i = 0; i < rawmicecount; i++)
	{
		if (rawmice[i].handles.rawinputhandle == raw->header.hDevice)
			break;
	}

	if (i == rawmicecount) // we're not tracking this device
		return;
	mouse = &rawmice[i];

	if (mouse->qdeviceid == DEVID_UNSET)
	{
		if ((raw->data.mouse.usButtonFlags & (RI_MOUSE_BUTTON_1_DOWN|RI_MOUSE_BUTTON_2_DOWN|RI_MOUSE_BUTTON_3_DOWN|RI_MOUSE_BUTTON_4_DOWN|RI_MOUSE_BUTTON_5_DOWN|RI_MOUSE_WHEEL))
			|| (raw->data.mouse.ulRawButtons & RI_RAWBUTTON_MASK) || raw->data.mouse.lLastX || raw->data.mouse.lLastY)
		{
			mouse->qdeviceid = Mouse_AllocateDevID();
		}
		else
			return;
	}

	multicursor_active[mouse->qdeviceid&7] = 0;

	// movement
	if (raw->data.mouse.usFlags & MOUSE_MOVE_ABSOLUTE)
	{
		if (in_simulatemultitouch.ival)
		{
			multicursor_active[mouse->qdeviceid&7] = true;
			multicursor_x[mouse->qdeviceid&7] = raw->data.mouse.lLastX;
			multicursor_y[mouse->qdeviceid&7] = raw->data.mouse.lLastY;
		}
		IN_MouseMove(mouse->qdeviceid, true, raw->data.mouse.lLastX, raw->data.mouse.lLastY, 0, 0);
	}
	else // RELATIVE
	{
		if (in_simulatemultitouch.ival)
		{
			multicursor_active[mouse->qdeviceid&7] = true;
			multicursor_x[mouse->qdeviceid&7] += raw->data.mouse.lLastX;
			multicursor_y[mouse->qdeviceid&7] += raw->data.mouse.lLastY;
			multicursor_x[mouse->qdeviceid&7] = bound(0, multicursor_x[mouse->qdeviceid&7], vid.pixelwidth);
			multicursor_y[mouse->qdeviceid&7] = bound(0, multicursor_y[mouse->qdeviceid&7], vid.pixelheight);
			IN_MouseMove(mouse->qdeviceid, true, multicursor_x[mouse->qdeviceid&7], multicursor_y[mouse->qdeviceid&7], 0, 0);
		}
		else
			IN_MouseMove(mouse->qdeviceid, false, raw->data.mouse.lLastX, raw->data.mouse.lLastY, 0, 0);
	}

	// buttons
	if (raw->data.mouse.usButtonFlags & RI_MOUSE_BUTTON_1_DOWN)
		IN_KeyEvent(mouse->qdeviceid, true, K_MOUSE1, 0);
	if (raw->data.mouse.usButtonFlags & RI_MOUSE_BUTTON_1_UP)
		IN_KeyEvent(mouse->qdeviceid, false, K_MOUSE1, 0);
	if (raw->data.mouse.usButtonFlags & RI_MOUSE_BUTTON_2_DOWN)
		IN_KeyEvent(mouse->qdeviceid, true, K_MOUSE2, 0);
	if (raw->data.mouse.usButtonFlags & RI_MOUSE_BUTTON_2_UP)
		IN_KeyEvent(mouse->qdeviceid, false, K_MOUSE2, 0);
	if (raw->data.mouse.usButtonFlags & RI_MOUSE_BUTTON_3_DOWN)
		IN_KeyEvent(mouse->qdeviceid, true, K_MOUSE3, 0);
	if (raw->data.mouse.usButtonFlags & RI_MOUSE_BUTTON_3_UP)
		IN_KeyEvent(mouse->qdeviceid, false, K_MOUSE3, 0);
	if (raw->data.mouse.usButtonFlags & RI_MOUSE_BUTTON_4_DOWN)
		IN_KeyEvent(mouse->qdeviceid, true, K_MOUSE4, 0);
	if (raw->data.mouse.usButtonFlags & RI_MOUSE_BUTTON_4_UP)
		IN_KeyEvent(mouse->qdeviceid, false, K_MOUSE4, 0);
	if (raw->data.mouse.usButtonFlags & RI_MOUSE_BUTTON_5_DOWN)
		IN_KeyEvent(mouse->qdeviceid, true, K_MOUSE5, 0);
	if (raw->data.mouse.usButtonFlags & RI_MOUSE_BUTTON_5_UP)
		IN_KeyEvent(mouse->qdeviceid, false, K_MOUSE5, 0);

	// mouse wheel
	if (raw->data.mouse.usButtonFlags & RI_MOUSE_WHEEL)
	{      // If the current message has a mouse_wheel message
		if ((SHORT)raw->data.mouse.usButtonData > 0)
		{
			IN_KeyEvent(mouse->qdeviceid, true, K_MWHEELUP, 0);
			IN_KeyEvent(mouse->qdeviceid, false, K_MWHEELUP, 0);
		}
		if ((SHORT)raw->data.mouse.usButtonData < 0)
		{
			IN_KeyEvent(mouse->qdeviceid, true, K_MWHEELDOWN, 0);
			IN_KeyEvent(mouse->qdeviceid, false, K_MWHEELDOWN, 0);
		}
	}

	// extra buttons
	tbuttons = raw->data.mouse.ulRawButtons & RI_RAWBUTTON_MASK;
	for (j=6 ; j<rawmice[i].numbuttons ; j++)
	{
		if ( (tbuttons & (1<<j)) && !(rawmice[i].oldbuttons & (1<<j)) )
		{
			IN_KeyEvent (mouse->qdeviceid, true, K_MOUSE1 + j, 0);
		}

		if ( !(tbuttons & (1<<j)) && (rawmice[i].oldbuttons & (1<<j)) )
		{
			IN_KeyEvent (mouse->qdeviceid, false, K_MOUSE1 + j, 0);
		}
	}

	rawmice[i].oldbuttons &= ~RI_RAWBUTTON_MASK;
	rawmice[i].oldbuttons |= tbuttons;
}

void INS_RawInput_KeyboardRead(void)
{
	int i;
	qboolean down;
	WPARAM wParam;
	LPARAM lParam;

	for (i = 0; i < rawkbdcount; i++)
	{
		if (rawkbd[i].handles.rawinputhandle == raw->header.hDevice)
			break;
	}

	if (i == rawkbdcount) // not tracking this device
		return;

	if (rawkbd[i].qdeviceid == DEVID_UNSET)
		rawkbd[i].qdeviceid = Keyboard_AllocateDevID();

	down = !(raw->data.keyboard.Flags & RI_KEY_BREAK);
	wParam = raw->data.keyboard.VKey ;//(-down) & 0xC0000000;
	lParam = (MapVirtualKey(raw->data.keyboard.VKey, 0)<<16) | ((!!(raw->data.keyboard.Flags & RI_KEY_E0))<<24);

 	INS_TranslateKeyEvent(wParam, lParam, down, rawkbd[i].qdeviceid, true);
}

void INS_RawInput_Read(HANDLE in_device_handle)
{
	int dwSize;

	// get raw input
	if ((*_GRID)((HRAWINPUT)in_device_handle, RID_INPUT, NULL, &dwSize, sizeof(RAWINPUTHEADER)) == -1)
	{
		Con_Printf("Raw input: unable to add to get size of raw input header.\n");
		return;
	}

	if (dwSize > ribuffersize)
	{
		ribuffersize = dwSize;
		raw = (RAWINPUT *)BZ_Realloc(raw, dwSize);
	}

	if ((*_GRID)((HRAWINPUT)in_device_handle, RID_INPUT, raw, &dwSize, sizeof(RAWINPUTHEADER)) != dwSize ) {
		Con_Printf("Raw input: unable to add to get raw input header.\n");
		return;
	}

	INS_RawInput_MouseRead();
	INS_RawInput_KeyboardRead();
}
#else
void INS_RawInput_Read(HANDLE in_device_handle)
{
}
#endif

/*
===================
INS_ClearStates
===================
*/
void INS_ClearStates (void)
{

	if (mouseactive)
	{
		memset(&sysmouse, 0, sizeof(sysmouse));
		sysmouse.numbuttons = 10;
	}
}

/*
===============
INS_StartupJoystick
===============
*/
static void INS_StartupJoystickId(unsigned int id)
{
	JOYCAPS		jc;
	MMRESULT	mmr;
	struct wjoy_s *joy;
	if (joy_count == MAX_JOYSTICKS)
		return;
	joy = &wjoy[joy_count];
	memset(joy, 0, sizeof(*joy));
	joy->id = id;
	joy->devid = DEVID_UNSET;//1+joy_count;	//FIXME: this is a hack. make joysticks 1-based. this means mouse0 controls 1st player, joy0 controls 2nd player (controls wrap so joy1 controls 1st player too.

	// get the capabilities of the selected joystick
	// abort startup if command fails
	memset (&jc, 0, sizeof(jc));
	if ((mmr = joyGetDevCaps (joy->id, &jc, sizeof(jc))) != JOYERR_NOERROR)
	{
		Con_Printf ("joystick %03x not found -- invalid joystick capabilities (%x)\n", id, mmr);
		return;
	}

	// save the joystick's number of buttons and POV status
	joy->numbuttons = jc.wNumButtons;
	joy->haspov = jc.wCaps & JOYCAPS_HASPOV;

	// mark the joystick as available and advanced initialization not completed
	// this is needed as cvars are not available during initialization

	joy_advancedinit = false;

	joy_count++;
}
static void IN_XInput_SetupAudio(struct wjoy_s *joy)
{
#ifdef AVAIL_XINPUT
	char audiodevicename[MAX_QPATH];
	wchar_t mssuck[MAX_QPATH];
	static GUID GUID_NULL;
	GUID gplayback = GUID_NULL;
	GUID gcapture = GUID_NULL;

	if (joy->audiodev)
	{
		S_ShutdownCard(joy->audiodev);
		joy->audiodev = NULL;
	}

	if (!xinput_useaudio)
		return;

	if (!joy->isxinput)
		return;
	if (joy->devid == DEVID_UNSET)
		return;

	if (pXInputGetDSoundAudioDeviceGuids(joy->id, &gplayback, &gcapture) != ERROR_SUCCESS)
		return;	//probably not plugged in

	if (!memcmp(&gplayback, &GUID_NULL, sizeof(gplayback)))
		return;	//we have a controller, but no headset.

	StringFromGUID2(&gplayback, mssuck, sizeof(mssuck)/sizeof(mssuck[0]));
	narrowen(audiodevicename, sizeof(audiodevicename), mssuck);
	Con_Printf("Controller %i uses audio device %s\n", joy->id, audiodevicename);
	joy->audiodev = S_SetupDeviceSeat("DirectSound", audiodevicename, joy->devid);
#endif
}
void INS_SetupControllerAudioDevices(qboolean enabled)
{
#ifdef AVAIL_XINPUT
	int i;

	if (!pXInputGetDSoundAudioDeviceGuids)
		return;

	xinput_useaudio = enabled;
	for (i = 0; i < joy_count; i++)
		IN_XInput_SetupAudio(&wjoy[i]);
#endif
}
void INS_StartupJoystick (void)
{
	unsigned int	id, numdevs;
	MMRESULT	mmr;

 	// assume no joysticks
	joy_count = 0;

#ifdef AVAIL_XINPUT
	if (in_xinput.ival)
	{
		static dllhandle_t *xinput;
		if (!xinput)
		{
			dllfunction_t funcs[] =
			{
				{(void**)&pXInputGetState, "XInputGetState"},
				{(void**)&pXInputSetState, "XInputSetState"},
				{NULL}
			};
			xinput = Sys_LoadLibrary(AVAIL_XINPUT_DLL, funcs);

			if (xinput)
				pXInputGetDSoundAudioDeviceGuids = Sys_GetAddressForName(xinput, "XInputGetDSoundAudioDeviceGuids");
		}
		if (pXInputGetState)
		{
			for (id = 0; id < 4; id++)
			{
				if (joy_count == countof(wjoy))
					break;
				memset(&wjoy[id], 0, sizeof(wjoy[id]));
				wjoy[joy_count].isxinput = true;
				wjoy[joy_count].id = id;
				wjoy[joy_count].devid = DEVID_UNSET;//id;
				wjoy[joy_count].numbuttons = 18;	//xinput supports 16 buttons, we emulate two more with the two triggers.
				joy_count++;
			}
			Con_Printf("XInput is enabled (max %i controllers)\n", id);
		}
		else
			Con_Printf("XInput not installed\n");
	}
#endif

	// abort startup if user requests no joystick
	if (!in_joystick.ival )
		return;

	// verify joystick driver is present
	if ((numdevs = joyGetNumDevs ()) == 0)
	{
		Con_Printf ("joystick not found -- driver not present\n");
		return;
	}

	mmr = JOYERR_UNPLUGGED;

	// cycle through the joystick ids for the first valid one
	for (id=0 ; id<numdevs ; id++)
	{
		memset (&ji, 0, sizeof(ji));
		ji.dwSize = sizeof(ji);
		ji.dwFlags = JOY_RETURNCENTERED;

		if ((mmr = joyGetPosEx (id, &ji)) == JOYERR_NOERROR)
		{
			INS_StartupJoystickId(id);
		}
	}

	if (joy_count)
		Con_Printf ("found %i joysticks\n", joy_count);
	else
		Con_DPrintf ("found no joysticks\n");
}

/*
===========
INS_Commands
===========
*/
void INS_Commands (void)
{
	int		i;
	DWORD	buttonstate, povstate;
	unsigned int idx;
	struct wjoy_s *joy;

	static const int xinputjbuttons[] =
	{	
		K_GP_DPAD_UP,
		K_GP_DPAD_DOWN,
		K_GP_DPAD_LEFT,
		K_GP_DPAD_RIGHT,
		K_GP_START,
		K_GP_BACK,
		K_GP_LEFT_THUMB,
		K_GP_RIGHT_THUMB,

		K_GP_LEFT_SHOULDER,
		K_GP_RIGHT_SHOULDER,
		K_GP_GUIDE,		//officially, this is 'reserved'
		K_GP_UNKNOWN,	//reserved
		K_GP_A,
		K_GP_B,
		K_GP_X,
		K_GP_Y,

		//not part of xinput specs, but appended by us from analog triggers
		K_GP_LEFT_TRIGGER,
		K_GP_RIGHT_TRIGGER
	};
	static const int mmjbuttons[32] =
	{
		K_JOY1,
		K_JOY2,
		K_JOY3,
		K_JOY4,
		//yes, aux1-4 skipped for compat with other quake engines.
		K_AUX5,
		K_AUX6,
		K_AUX7,
		K_AUX8,
		K_AUX9,
		K_AUX10,
		K_AUX11,
		K_AUX12,
		K_AUX13,
		K_AUX14,
		K_AUX15,
		K_AUX16,
		K_AUX17,
		K_AUX18,
		K_AUX19,
		K_AUX20,
		K_AUX21,
		K_AUX22,
		K_AUX23,
		K_AUX24,
		K_AUX25,
		K_AUX26,
		K_AUX27,
		K_AUX28,
		//29-32 used for the pov stuff, so lets switch back to aux1-4 to avoid wastage
		K_AUX1,
		K_AUX2,
		K_AUX3,
		K_AUX4
	};


	for (idx = 0; idx < joy_count; idx++)
	{
		joy = &wjoy[idx];

		// loop through the joystick buttons
		// key a joystick event or auxillary event for higher number buttons for each state change
		buttonstate = joy->buttonstate;
		if (buttonstate != joy->oldbuttonstate)
		{
			if (joy->devid == DEVID_UNSET)
			{
				joy->devid = Joy_AllocateDevID();
				IN_XInput_SetupAudio(joy);
			}

			if (joy->isxinput)
			{
				for (i=0 ; i < joy->numbuttons ; i++)
				{
					if ( (buttonstate & (1<<i)) && !(joy->oldbuttonstate & (1<<i)) )
						Key_Event (joy->devid, xinputjbuttons[i], 0, true);

					if ( !(buttonstate & (1<<i)) && (joy->oldbuttonstate & (1<<i)) )
						Key_Event (joy->devid, xinputjbuttons[i], 0, false);
				}
			}
			else
			{
				for (i=0 ; i < joy->numbuttons ; i++)
				{
					if ( (buttonstate & (1<<i)) && !(joy->oldbuttonstate & (1<<i)) )
						Key_Event (joy->devid, mmjbuttons[i], 0, true);

					if ( !(buttonstate & (1<<i)) && (joy->oldbuttonstate & (1<<i)) )
						Key_Event (joy->devid, mmjbuttons[i], 0, false);
				}
			}
			joy->oldbuttonstate = buttonstate;
		}

		if (joy->haspov)
		{
			// convert POV information into 4 bits of state information
			// this avoids any potential problems related to moving from one
			// direction to another without going through the center position
			povstate = 0;
			if(joy->povstate != JOY_POVCENTERED)
			{
				if (joy->povstate == JOY_POVFORWARD)
					povstate |= 0x01;
				if (joy->povstate == JOY_POVRIGHT)
					povstate |= 0x02;
				if (joy->povstate == JOY_POVBACKWARD)
					povstate |= 0x04;
				if (joy->povstate == JOY_POVLEFT)
					povstate |= 0x08;
			}
			if (joy->oldpovstate != povstate)
			{
				if (joy->devid == DEVID_UNSET)
					joy->devid = Joy_AllocateDevID();

				// determine which bits have changed and key an auxillary event for each change
				for (i=0 ; i < 4 ; i++)
				{
					if ( (povstate & (1<<i)) && !(joy->oldpovstate & (1<<i)) )
					{
						Key_Event (joy->devid, K_AUX29 + i, 0, true);
					}

					if ( !(povstate & (1<<i)) && (joy->oldpovstate & (1<<i)) )
					{
						Key_Event (joy->devid, K_AUX29 + i, 0, false);
					}
				}
				joy->oldpovstate = povstate;
			}
		}
	}
}


/*
===============
INS_ReadJoystick
===============
*/
qboolean INS_ReadJoystick (struct wjoy_s *joy)
{
#ifdef AVAIL_XINPUT
	if (joy->isxinput)
	{
		XINPUT_STATE xistate;
		XINPUT_VIBRATION vibrator;
		HRESULT hr;
		memset(&xistate, 0, sizeof(xistate));
		hr = pXInputGetState(joy->id, &xistate);

		if (in_xinput.ival == 2)
		{
			Con_Printf("xi%i %s, b:%#x r:%u,%u l:%u,%u t:%u,%u\n", joy->id, (hr==ERROR_SUCCESS)?"success":va("%lx", hr), xistate.Gamepad.wButtons,
				xistate.Gamepad.sThumbRX, xistate.Gamepad.sThumbRY, 
				xistate.Gamepad.sThumbLX,xistate.Gamepad.sThumbLY,
				xistate.Gamepad.bLeftTrigger,xistate.Gamepad.bRightTrigger);
		}
		else if (in_xinput.ival == 3 && joy->id == 0)
		{
		//I don't have a controller to test this with, so we fake stuff.
			POINT p;
			GetCursorPos(&p);
			hr = ERROR_SUCCESS;
			xistate.Gamepad.wButtons = rand() & 0xfff0;
			xistate.Gamepad.sThumbRX = 0;//(p.x/1920.0)*0xffff - 0x8000;
			xistate.Gamepad.sThumbRY = 0;//(p.y/1080.0)*0xffff - 0x8000;
			xistate.Gamepad.sThumbLX = (p.x/1920.0)*0xffff - 0x8000;
			xistate.Gamepad.sThumbLY = (p.y/1080.0)*0xffff - 0x8000;
			xistate.Gamepad.bLeftTrigger = 0;
			xistate.Gamepad.bRightTrigger = 0;
		}

		if (hr == ERROR_SUCCESS)
		{	//ERROR_SUCCESS
			//do we care about the dwPacketNumber?
			joy->buttonstate = xistate.Gamepad.wButtons & 0xffff;

			if (xistate.Gamepad.bLeftTrigger >= 128)
				joy->buttonstate |= 0x10000;
			if (xistate.Gamepad.bRightTrigger >= 128)
				joy->buttonstate |= 0x20000;

			if (joy->devid != DEVID_UNSET)
			{
				IN_JoystickAxisEvent(joy->devid, GPAXIS_LT_RIGHT, xistate.Gamepad.sThumbLX / 32768.0);
				IN_JoystickAxisEvent(joy->devid, GPAXIS_LT_DOWN, xistate.Gamepad.sThumbLY / 32768.0);
				IN_JoystickAxisEvent(joy->devid, GPAXIS_LT_TRIGGER, xistate.Gamepad.bLeftTrigger/255.0);
				IN_JoystickAxisEvent(joy->devid, GPAXIS_RT_RIGHT, xistate.Gamepad.sThumbRX / 32768.0);
				IN_JoystickAxisEvent(joy->devid, GPAXIS_RT_DOWN, xistate.Gamepad.sThumbRY / 32768.0);
				IN_JoystickAxisEvent(joy->devid, GPAXIS_RT_TRIGGER, xistate.Gamepad.bRightTrigger/255.0);

				vibrator.wLeftMotorSpeed = xinput_leftvibrator.value * 0xffff;
				vibrator.wRightMotorSpeed = xinput_rightvibrator.value * 0xffff;
				pXInputSetState(joy->id, &vibrator);
			}
			return true;
		}
	}
	else
#endif
	{
		memset (&ji, 0, sizeof(ji));
		ji.dwSize = sizeof(ji);
		ji.dwFlags = joy_flags;

		if (joyGetPosEx (joy->id, &ji) == JOYERR_NOERROR)
		{
			joy->povstate = ji.dwPOV;
			joy->buttonstate = ji.dwButtons;
			if (joy->devid != DEVID_UNSET)
			{
				IN_JoystickAxisEvent(joy->devid, GPAXIS_LT_RIGHT, (ji.dwXpos - 32768.0) / 32768);
				IN_JoystickAxisEvent(joy->devid, GPAXIS_LT_DOWN, (ji.dwYpos - 32768.0) / 32768);
				IN_JoystickAxisEvent(joy->devid, GPAXIS_LT_AUX, (ji.dwZpos - 32768.0) / 32768);
				IN_JoystickAxisEvent(joy->devid, GPAXIS_RT_RIGHT, (ji.dwRpos - 32768.0) / 32768);
				IN_JoystickAxisEvent(joy->devid, GPAXIS_RT_DOWN, (ji.dwUpos - 32768.0) / 32768);
				IN_JoystickAxisEvent(joy->devid, GPAXIS_RT_AUX, (ji.dwVpos - 32768.0) / 32768);
			}
			return true;
		}
	}

	joy->povstate = 0;
	joy->buttonstate = 0;
	if (joy->devid != DEVID_UNSET)
	{
		IN_JoystickAxisEvent(joy->devid, 0, 0);
		IN_JoystickAxisEvent(joy->devid, 1, 0);
		IN_JoystickAxisEvent(joy->devid, 2, 0);
		IN_JoystickAxisEvent(joy->devid, 3, 0);
		IN_JoystickAxisEvent(joy->devid, 4, 0);
		IN_JoystickAxisEvent(joy->devid, 5, 0);
	}

	// read error occurred
	// turning off the joystick seems too harsh for 1 read error,
	// but what should be done?
	// Con_Printf ("INS_ReadJoystick: no response\n");
	// joy_avail = false;
	return false;
}

static void INS_JoyMovePtr (struct wjoy_s *joy, float *movements, int pnum)
{
	int wpnum;

	/*each device will be processed when its player comes to be processed*/
	wpnum = cl.splitclients;
	if (wpnum < 1)
		wpnum = 1;
	if (cl_forceseat.ival)
		wpnum = (cl_forceseat.ival-1) % wpnum; 
	else
		wpnum = joy->devid % wpnum;
	if (wpnum != pnum)
		return;


	// collect the joystick data, if possible
	if (INS_ReadJoystick (joy) != true)
	{
		return;
	}
}
/*
===========
INS_JoyMove
===========
*/
void INS_JoyMove (float *movements, int pnum)
{
	unsigned int idx;
	for (idx = 0; idx < joy_count; idx++)
	{
		INS_JoyMovePtr(&wjoy[idx], movements, pnum);
	}
}

void INS_EnumerateDevices(void *ctx, void(*callback)(void *ctx, const char *type, const char *devicename, unsigned int *qdevid))
{
	int idx;

	for (idx = 0; idx < rawkbdcount; idx++)
		callback(ctx, "keyboard", rawkbd[idx].sysname?rawkbd[idx].sysname:va("rawk%i", idx), &rawkbd[idx].qdeviceid);
	callback(ctx, "keyboard", "system", NULL);

	for (idx = 0; idx < rawmicecount; idx++)
		callback(ctx, "mouse", rawmice[idx].sysname?rawmice[idx].sysname:va("raw%i", idx), &rawmice[idx].qdeviceid);

#ifdef AVAIL_DINPUT
#if (DIRECTINPUT_VERSION >= DINPUT_VERSION_DX7)
	if (dinput >= DINPUT_VERSION_DX7 && g_pMouse7)
		callback(ctx, "mouse", "di7", NULL);
	else
#endif
		if (dinput && g_pMouse)
		callback(ctx, "mouse", "di", NULL);
#endif
	callback(ctx, "mouse", "system", NULL);

	for (idx = 0; idx < joy_count; idx++)
	{
		int odevid = wjoy[idx].devid;
		if (wjoy[idx].isxinput)
			callback(ctx, "joy", va("xi%i", wjoy[idx].id), &wjoy[idx].devid);
		else
			callback(ctx, "joy", va("mmj%i", wjoy[idx].id), &wjoy[idx].devid);
		if (odevid != wjoy[idx].devid)
			IN_XInput_SetupAudio(&wjoy[idx]);
	}
}

static qbyte        scantokey[] =
{
//	0			1			2			3			4			5			6				7
//	8			9			A			B			C			D			E				F
	0,			27,			'1',		'2',		'3',		'4',		'5',			'6',		// 0
	'7',		'8',		'9',		'0',		'-',		'=',		K_BACKSPACE,	9,			// 0
	'q',		'w',		'e',		'r',		't',		'y',		'u',			'i',		// 1
	'o',		'p',		'[',		']',		K_ENTER,	K_LCTRL,	'a',			's',		// 1
	'd',		'f',		'g',		'h',		'j',		'k',		'l',			';',		// 2
	'\'',		'`',		K_LSHIFT,	'\\',		'z',		'x',		'c',			'v',		// 2
	'b',		'n',		'm',		',',		'.',		'/',		K_RSHIFT,		K_KP_STAR,	// 3
	K_LALT,		' ',		K_CAPSLOCK,	K_F1,		K_F2,		K_F3,		K_F4,			K_F5,		// 3
	K_F6,		K_F7,		K_F8,		K_F9,		K_F10,		K_PAUSE,	K_SCRLCK,		K_KP_HOME,		// 4
	K_KP_UPARROW,K_KP_PGUP,	K_KP_MINUS,	K_KP_LEFTARROW,K_KP_5,	K_KP_RIGHTARROW,K_KP_PLUS,	K_KP_END,		// 4
	K_KP_DOWNARROW,K_KP_PGDN,K_KP_INS,	K_KP_DEL,	0,			0,			0,				K_F11,		// 5
	K_F12,		0,			0,			0,			0,			0,			0,				0,			// 5
	0,			0,			0,			0,			0,			'\\',		0,				0,			// 6
	0,			0,			0,			0,			0,			0,			0,				0,			// 6
	0,			0,			0,			0,			0,			0,			0,				0,			// 7
	0,			0,			0,			0,			0,			0,			0,				0,			// 7
//	0			1			2			3			4			5			6				7
//	8			9			A			B			C			D			E				F
	0,			0,			0,			0,			0,			0,			0,				0,			// 8
	0,			0,			0,			0,			0,			0,			0,				0,			// 8
	0,			0,			0,			0,			0,			0,			0,				0,			// 9
	0,			0,			0,			0,			0,			0,			0,				0,			// 9
	0,			0,			0,			0,			0,			0,			0,				0,			// a
	0,			0,			0,			0,			0,			0,			0,				0,			// a
	0,			0,			0,			0,			0,			0,			0,				0,			// b
	0,			0,			0,			0,			0,			0,			0,				0,			// b
	0,			0,			0,			0,			0,			0,			0,				0,			// c
	0,			0,			0,			0,			0,			0,			0,				0,			// c
	0,			0,			0,			0,			0,			0,			0,				0,			// d
	0,			0,			0,			0,			0,			0,			0,				0,			// d
	0,			0,			0,			0,			0,			0,			0,				0,			// e
	0,			0,			0,			0,			0,			0,			0,				0,			// e
	0,			0,			0,			0,			0,			0,			0,				0,			// f
	0,			0,			0,			0,			0,			0,			0,				0,			// f
//	0			1			2			3			4			5			6				7
//	8			9			A			B			C			D			E				F
	0,			27,			'1',		'2',		'3',		'4',		'5',			'6',		// 0
	'7',		'8',		'9',		'0',		'-',		'=',		K_BACKSPACE,	9,			// 0
	'q',		'w',		'e',		'r',		't',		'y',		'u',			'i',		// 1
	'o',		'p',		'[',		']',		K_KP_ENTER,	K_RCTRL,	'a',			's',		// 1
	'd',		'f',		'g',		'h',		'j',		'k',		'l',			';',		// 2
	'\'',		'`',		K_SHIFT,	'\\',		'z',		'x',		'c',			'v',		// 2
	'b',		'n',		'm',		',',		'.',		K_KP_SLASH,	K_SHIFT,		K_PRINTSCREEN,// 3
	K_RALT,		' ',		K_CAPSLOCK,	K_F1,		K_F2,		K_F3,		K_F4,			K_F5,		// 3
	K_F6,		K_F7,		K_F8,		K_F9,		K_F10,		K_KP_NUMLOCK,K_SCRLCK,		K_HOME,		// 4
	K_UPARROW,	K_PGUP,		'-',		K_LEFTARROW,0,			K_RIGHTARROW,'+',			K_END,		// 4
	K_DOWNARROW,K_PGDN,		K_INS,		K_DEL,		0,			0,			0,				K_F11,		// 5
	K_F12,		0,			0,			0,			0,			0,			0,				0,			// 5
	0,			0,			0,			0,			0,			'\\',		0,				0,			// 6
	0,			0,			0,			0,			0,			0,			0,				0,			// 6
	0,			0,			0,			0,			0,			0,			0,				0,			// 7
	0,			0,			0,			0,			0,			0,			0,				0			// 7
//	0			1			2			3			4			5			6				7
//	8			9			A			B			C			D			E				F
};

/*
=======
MapKey

Map from windows to quake keynums
=======
*/
static int MapKey (int vkey)
{
	int key;
	key = (vkey>>16)&511;

	if (key < sizeof(scantokey) / sizeof(scantokey[0]))
		key = scantokey[key];
	else
		key = 0;
	if (!cl_keypad.ival)
	{
		switch(key)
		{
		case K_KP_HOME:			return '7';
		case K_KP_UPARROW:		return '8';
		case K_KP_PGUP:			return '9';
		case K_KP_LEFTARROW:	return '4';
		case K_KP_5:			return '5';
		case K_KP_RIGHTARROW:	return '6';
		case K_KP_END:			return '1';
		case K_KP_DOWNARROW:	return '2';
		case K_KP_PGDN:			return '3';
		case K_KP_ENTER:		return K_ENTER;
		case K_KP_INS:			return '0';
		case K_KP_DEL:			return '.';
		case K_KP_SLASH:		return '/';
		case K_KP_MINUS:		return '-';
		case K_KP_PLUS:			return '+';
		case K_KP_STAR:			return '*';
		case K_KP_EQUALS:		return '=';
		}
	}
	if (key == 0)
		Con_DPrintf("key 0x%02x has no translation\n", key);
	return key;
}

void INS_TranslateKeyEvent(WPARAM wParam, LPARAM lParam, qboolean down, int qdeviceid, qboolean genkeystate)
{
	extern cvar_t in_builtinkeymap;
	int qcode;
	int unicode;
	int chars;
	extern int		keyshift[256];
	extern int		shift_down;

	qcode = MapKey(lParam);

	if (WinNT && !in_builtinkeymap.value)
	{
		BYTE	keystate[256];
		WCHAR	wchars[2];

		if (genkeystate)
		{
			extern qboolean	keydown[K_MAX];
			memset(keystate, 0, sizeof(keystate));
			//128 for held.
			//1 for toggled (ie: caps / num)
			keystate[VK_LSHIFT] = 128*!!keydown[K_LSHIFT];
			keystate[VK_RSHIFT] = 128*!!keydown[K_RSHIFT];
			keystate[VK_LCONTROL] = 128*!!keydown[K_LCTRL];
			keystate[VK_RCONTROL] = 128*!!keydown[K_RCTRL];
			keystate[VK_LMENU] = 128*!!keydown[K_LALT];
			keystate[VK_RMENU] = 128*!!keydown[K_RALT];

			//seems to matter
			keystate[VK_SHIFT] = keystate[VK_LSHIFT]|keystate[VK_RSHIFT];
			keystate[VK_CONTROL] = keystate[VK_LCONTROL]|keystate[VK_RCONTROL];
			keystate[VK_MENU] = keystate[VK_LMENU]|keystate[VK_RMENU];

			keystate[VK_NUMLOCK] = 1;	//doesn't seem to matter?
		}
		else
			GetKeyboardState(keystate);
		chars = ToUnicode(wParam, HIWORD(lParam), keystate, wchars, sizeof(wchars)/sizeof(wchars[0]), 0);
	
		if (chars > 0)
		{
			if (!in_nonstandarddeadkeys.ival)
			{
				for (unicode = 0; unicode < chars-1; unicode++)
					IN_KeyEvent(qdeviceid, down, 0, wchars[unicode]);
			}
			unicode = wchars[chars-1];
		}
		else unicode = 0;
	}
	else
	{
		unicode = (qcode < 128)?qcode:0;
		if (shift_down && unicode < K_MAX && keyshift[unicode])
			unicode = keyshift[unicode]; 
	}
	IN_KeyEvent(qdeviceid, down, qcode, unicode);
}
#endif
