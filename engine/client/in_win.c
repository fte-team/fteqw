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

#define USINGRAWINPUT

#ifdef USINGRAWINPUT
#include "in_raw.h"
#endif

#ifdef AVAIL_DINPUT

#ifndef _MSC_VER
#define DIRECTINPUT_VERSION 0x0500
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
cvar_t	m_filter = SCVAR("m_filter","0");
cvar_t  m_accel = SCVAR("m_accel", "0");
cvar_t	m_forcewheel = SCVAR("m_forcewheel", "1");
cvar_t	m_forcewheel_threshold = SCVAR("m_forcewheel_threshold", "32");
cvar_t	in_dinput = SCVARF("in_dinput","0", CVAR_ARCHIVE);
cvar_t	in_builtinkeymap = SCVARF("in_builtinkeymap", "1", CVAR_ARCHIVE);

cvar_t	m_accel_noforce = SCVAR("m_accel_noforce", "0");
cvar_t  m_threshold_noforce = SCVAR("m_threshold_noforce", "0");

cvar_t	cl_keypad = SCVAR("cl_keypad", "0");
extern cvar_t cl_forcesplitclient;

typedef struct {
	union {
		HANDLE rawinputhandle;
	} handles;

	int playerid;
} keyboard_t;

typedef struct {
	union {
		HANDLE rawinputhandle; // raw input
	} handles;

	int numbuttons;
	int playerid;

	volatile int buttons;
	volatile int oldbuttons;
	volatile int wheeldelta;

	volatile int delta[2];
	int old_delta[2];
	int accum[2];

	int pos[2];
} mouse_t;

mouse_t sysmouse;

static qboolean	restore_spi;
static int		originalmouseparms[3], newmouseparms[3] = {0, 0, 0};
qboolean		mouseinitialized;
static qboolean	mouseparmsvalid, mouseactivatetoggle;
static qboolean	mouseshowtoggle = 1;
static qboolean	dinput_acquired;
unsigned int uiWheelMessage;

qboolean	mouseactive;

// joystick defines and variables
// where should defines be moved?
#define JOY_ABSOLUTE_AXIS	0x00000000		// control like a joystick
#define JOY_RELATIVE_AXIS	0x00000010		// control like a mouse, spinner, trackball
#define	JOY_MAX_AXES		6				// X, Y, Z, R, U, V
#define JOY_AXIS_X			0
#define JOY_AXIS_Y			1
#define JOY_AXIS_Z			2
#define JOY_AXIS_R			3
#define JOY_AXIS_U			4
#define JOY_AXIS_V			5

enum _ControlList
{
	AxisNada = 0, AxisForward, AxisLook, AxisSide, AxisTurn
};

DWORD	dwAxisFlags[JOY_MAX_AXES] =
{
	JOY_RETURNX, JOY_RETURNY, JOY_RETURNZ, JOY_RETURNR, JOY_RETURNU, JOY_RETURNV
};

DWORD	dwAxisMap[JOY_MAX_AXES];
DWORD	dwControlMap[JOY_MAX_AXES];
PDWORD	pdwRawValue[JOY_MAX_AXES];

#ifdef IN_XFLIP
cvar_t	in_xflip = SCVAR("in_xflip", "0");
#endif

// none of these cvars are saved over a session
// this means that advanced controller configuration needs to be executed
// each time.  this avoids any problems with getting back to a default usage
// or when changing from one controller to another.  this way at least something
// works.
cvar_t	in_joystick				= SCVARF("joystick","0", CVAR_ARCHIVE);
cvar_t	joy_name				= SCVAR("joyname", "joystick");
cvar_t	joy_advanced			= SCVAR("joyadvanced", "0");
cvar_t	joy_advaxisx			= SCVAR("joyadvaxisx", "0");
cvar_t	joy_advaxisy			= SCVAR("joyadvaxisy", "0");
cvar_t	joy_advaxisz			= SCVAR("joyadvaxisz", "0");
cvar_t	joy_advaxisr			= SCVAR("joyadvaxisr", "0");
cvar_t	joy_advaxisu			= SCVAR("joyadvaxisu", "0");
cvar_t	joy_advaxisv			= SCVAR("joyadvaxisv", "0");
cvar_t	joy_forwardthreshold	= SCVAR("joyforwardthreshold", "0.15");
cvar_t	joy_sidethreshold		= SCVAR("joysidethreshold", "0.15");
cvar_t	joy_pitchthreshold		= SCVAR("joypitchthreshold", "0.15");
cvar_t	joy_yawthreshold		= SCVAR("joyyawthreshold", "0.15");
cvar_t	joy_forwardsensitivity	= SCVAR("joyforwardsensitivity", "-1.0");
cvar_t	joy_sidesensitivity		= SCVAR("joysidesensitivity", "-1.0");
cvar_t	joy_pitchsensitivity	= SCVAR("joypitchsensitivity", "1.0");
cvar_t	joy_yawsensitivity		= SCVAR("joyyawsensitivity", "-1.0");
cvar_t	joy_wwhack1				= SCVAR("joywwhack1", "0.0");
cvar_t	joy_wwhack2				= SCVAR("joywwhack2", "0.0");

qboolean	joy_avail, joy_advancedinit, joy_haspov;
DWORD		joy_oldbuttonstate, joy_oldpovstate;

int			joy_id;
DWORD		joy_flags;
DWORD		joy_numbuttons;

#ifdef AVAIL_DINPUT
static const GUID fGUID_XAxis	= {0xA36D02E0, 0xC9F3, 0x11CF, {0xBF, 0xC7, 0x44, 0x45, 0x53, 0x54, 0x00, 0x00}};
static const GUID fGUID_YAxis	= {0xA36D02E1, 0xC9F3, 0x11CF, {0xBF, 0xC7, 0x44, 0x45, 0x53, 0x54, 0x00, 0x00}};
static const GUID fGUID_ZAxis	= {0xA36D02E2, 0xC9F3, 0x11CF, {0xBF, 0xC7, 0x44, 0x45, 0x53, 0x54, 0x00, 0x00}};
static const GUID fGUID_SysMouse	= {0x6F1D2B60, 0xD5A0, 0x11CF, {0xBF, 0xC7, 0x44, 0x45, 0x53, 0x54, 0x00, 0x00}};

static const GUID fIID_IDirectInputDevice7A	= {0x57d7c6bc, 0x2356, 0x11d3, {0x8e, 0x9d, 0x00, 0xC0, 0x4f, 0x68, 0x44, 0xae}};
static const GUID fIID_IDirectInput7A		= {0x9a4cb684, 0x236d, 0x11d3, {0x8e, 0x9d, 0x00, 0xc0, 0x4f, 0x68, 0x44, 0xae}};

// devices
LPDIRECTINPUT		g_pdi;
LPDIRECTINPUTDEVICE	g_pMouse;

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
LPDIRECTINPUT7		g_pdi7;
LPDIRECTINPUTDEVICE7	g_pMouse7;

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

pGetRawInputDeviceList _GRIDL;
pGetRawInputData _GRID;
pGetRawInputDeviceInfoA _GRIDIA;
pRegisterRawInputDevices _RRID;

keyboard_t *rawkbd;
mouse_t *rawmice;
int rawmicecount;
int rawkbdcount;
RAWINPUT *raw;
int ribuffersize;

cvar_t in_rawinput = SCVAR("in_rawinput", "0");
cvar_t in_rawinput_keyboard = SCVAR("in_rawinput_keyboard", "0");
cvar_t in_rawinput_rdp = SCVAR("in_rawinput_rdp", "0");

void IN_RawInput_MouseDeRegister(void);
int IN_RawInput_MouseRegister(void);
void IN_RawInput_KeyboardDeRegister(void);
int IN_RawInput_KeyboardRegister(void);
void IN_RawInput_DeInit(void);

#endif

// forward-referenced functions
void IN_StartupJoystick (void);
void Joy_AdvancedUpdate_f (void);
void IN_JoyMove (float *movements, int pnum);

/*
===========
Force_CenterView_f
===========
*/
void Force_CenterView_f (void)
{
	cl.viewangles[0][PITCH] = 0;
}

/*
===========
IN_UpdateClipCursor
===========
*/
void IN_UpdateClipCursor (void)
{

	if (mouseinitialized && mouseactive && !dinput)
	{
		ClipCursor (&window_rect);
	}
}


/*
===========
IN_ShowMouse
===========
*/
static void IN_ShowMouse (void)
{

	if (!mouseshowtoggle)
	{
		ShowCursor (TRUE);
		mouseshowtoggle = 1;
	}
}


/*
===========
IN_HideMouse
===========
*/
static void IN_HideMouse (void)
{

	if (mouseshowtoggle)
	{
		ShowCursor (FALSE);
		mouseshowtoggle = 0;
	}
}


/*
===========
IN_ActivateMouse
===========
*/
static void IN_ActivateMouse (void)
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
				if (IN_RawInput_MouseRegister())
				{
					Con_SafePrintf("Raw input: unable to register raw input for mice, deinitializing\n");
					IN_RawInput_MouseDeRegister();
				}
			}
			if (rawkbdcount > 0)
			{
				if (IN_RawInput_KeyboardRegister())
				{
					Con_SafePrintf("Raw input: unable to register raw input for keyboard, deinitializing\n");
					IN_RawInput_KeyboardDeRegister();
				}
			}
#endif

			if (mouseparmsvalid)
				restore_spi = SystemParametersInfo (SPI_SETMOUSE, 0, newmouseparms, 0);

			SetCursorPos (window_center_x, window_center_y);
			SetCapture (mainwindow);
			ClipCursor (&window_rect);
		}

		mouseactive = true;
	}
}


/*
===========
IN_DeactivateMouse
===========
*/
static void IN_DeactivateMouse (void)
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
				IN_RawInput_MouseDeRegister();
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
IN_SetQuakeMouseState
===========
*/
void IN_SetQuakeMouseState (void)
{
	if (mouseactivatetoggle)
		IN_ActivateMouse ();
	else
		IN_DeactivateMouse();
}

/*
===========
IN_RestoreOriginalMouseState
===========
*/
void IN_RestoreOriginalMouseState (void)
{
	if (mouseactivatetoggle)
	{
		IN_DeactivateMouse ();
		mouseactivatetoggle = true;
	}

// try to redraw the cursor so it gets reinitialized, because sometimes it
// has garbage after the mode switch
	ShowCursor (TRUE);
	ShowCursor (FALSE);
}




void IN_UpdateGrabs(int fullscreen, int activeapp)
{
	int grabmouse;

	if (!activeapp)
		grabmouse = false;
	else if (fullscreen)
		grabmouse = true;
	else if (_windowed_mouse.value)
	{
		if (!Key_MouseShouldBeFree())
			grabmouse = true;
		else
			grabmouse = false;
	}
	else
		grabmouse = false;

	//visiblity
	if (grabmouse)
		IN_HideMouse();
	else
		IN_ShowMouse();

#ifdef HLCLIENT
	//halflife gamecode does its own mouse control... yes this is vile.
	if (grabmouse)
	{
		if (CLHL_GamecodeDoesMouse())
			grabmouse = 2;
	}

	if (grabmouse == 2)
	{
		IN_DeactivateMouse();
		CLHL_SetMouseActive(true);
		return;
	}

	CLHL_SetMouseActive(false);
#endif

	if (grabmouse)
		IN_ActivateMouse();
	else
		IN_DeactivateMouse();
}






#ifdef AVAIL_DINPUT
BOOL CALLBACK IN_EnumerateDevices(LPCDIDEVICEINSTANCE inst, LPVOID parm)
{
	Con_DPrintf("EnumerateDevices found: %s\n", inst->tszProductName);

	return DIENUM_CONTINUE;
}
/*
===========
IN_InitDInput
===========
*/
int IN_InitDInput (void)
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
		hInstDI = LoadLibrary("dinput.dll");

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
		hr = iDirectInputCreateEx(global_hInstance, DINPUT_VERSION_DX7, &fIID_IDirectInput7A, &g_pdi7, NULL);

		if (FAILED(hr))
			return 0;

		IDirectInput7_EnumDevices(g_pdi7, 0, &IN_EnumerateDevices, NULL, DIEDFL_ATTACHEDONLY);

		// obtain an interface to the system mouse device.
		hr = IDirectInput7_CreateDeviceEx(g_pdi7, &fGUID_SysMouse, &fIID_IDirectInputDevice7A, &g_pMouse7, NULL);

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
	IDirectInput_EnumDevices(g_pdi, 0, &IN_EnumerateDevices, NULL, DIEDFL_ATTACHEDONLY);

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

void IN_CloseDInput (void)
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
void IN_RawInput_MouseDeRegister(void)
{
	RAWINPUTDEVICE Rid;

	// deregister raw input
	Rid.usUsagePage = 0x01;
	Rid.usUsage = 0x02;
	Rid.dwFlags = RIDEV_REMOVE;
	Rid.hwndTarget = NULL;

	(*_RRID)(&Rid, 1, sizeof(Rid));
}

void IN_RawInput_KeyboardDeRegister(void)
{
	RAWINPUTDEVICE Rid;

	// deregister raw input
	Rid.usUsagePage = 0x01;
	Rid.usUsage = 0x02;
	Rid.dwFlags = RIDEV_REMOVE;
	Rid.hwndTarget = NULL;

	(*_RRID)(&Rid, 1, sizeof(Rid));
}

void IN_RawInput_DeInit(void)
{
	if (rawmicecount > 0)
	{
		IN_RawInput_MouseDeRegister();
		Z_Free(rawmice);
		rawmicecount = 0;
	}
	if (rawkbdcount > 0)
	{
		IN_RawInput_KeyboardDeRegister();
		Z_Free(rawkbd);
		rawkbdcount = 0;
	}
}
#endif

#ifdef USINGRAWINPUT
// raw input registration functions
int IN_RawInput_MouseRegister(void)
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

int IN_RawInput_KeyboardRegister(void)
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

int IN_RawInput_Register(void)
{
	if (IN_RawInput_MouseRegister())
		return !in_rawinput_keyboard.ival || IN_RawInput_KeyboardRegister();
	return 0;
}

int IN_RawInput_IsRDPDevice(char *cDeviceString)
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

void IN_RawInput_Init(void)
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
		Con_SafePrintf("Raw input: function RegisterRawInputDevices could not be registered\n");
		return;
	}
	_GRIDL = (pGetRawInputDeviceList)GetProcAddress(user32,"GetRawInputDeviceList");
	if (!_GRIDL)
	{
		Con_SafePrintf("Raw input: function GetRawInputDeviceList could not be registered\n");
		return;
	}
	_GRIDIA = (pGetRawInputDeviceInfoA)GetProcAddress(user32,"GetRawInputDeviceInfoA");
	if (!_GRIDIA)
	{
		Con_SafePrintf("Raw input: function GetRawInputDeviceInfoA could not be registered\n");
		return;
	}
	_GRID = (pGetRawInputData)GetProcAddress(user32,"GetRawInputData");
	if (!_GRID)
	{
		Con_SafePrintf("Raw input: function GetRawInputData could not be registered\n");
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

		if (!(in_rawinput_rdp.value) && IN_RawInput_IsRDPDevice(dname)) // use rdp (cvar)
			continue;

		switch (pRawInputDeviceList[i].dwType)
		{
		case RIM_TYPEMOUSE:
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

		if (!(in_rawinput_rdp.value) && IN_RawInput_IsRDPDevice(dname)) // use rdp (cvar)
			continue;

		switch (pRawInputDeviceList[i].dwType)
		{
		case RIM_TYPEMOUSE:
			// set handle
			rawmice[rawmicecount].handles.rawinputhandle = pRawInputDeviceList[i].hDevice;
			rawmice[rawmicecount].numbuttons = 10;
			rawmice[rawmicecount].pos[0] = RI_INVALID_POS;
			rawmice[rawmicecount].playerid = rawmicecount;
			rawmicecount++;
			break;
		case RIM_TYPEKEYBOARD:
			if (!in_rawinput_keyboard.ival)
				continue;

			rawkbd[rawkbdcount].handles.rawinputhandle = pRawInputDeviceList[i].hDevice;
			rawkbd[rawkbdcount].playerid = rawmicecount;
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
		Con_SafePrintf("Raw input type %i: [%i] %s\n", (int)pRawInputDeviceList[i].dwType, i, dname);
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
IN_StartupMouse
===========
*/
void IN_StartupMouse (void)
{
	if ( COM_CheckParm ("-nomouse") )
		return;

	mouseinitialized = true;

	//make sure it can't get stuck
	IN_DeactivateMouse ();

#ifdef AVAIL_DINPUT
	if (in_dinput.value)
	{
		dinput = IN_InitDInput ();

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
		IN_ActivateMouse ();
}


/*
===========
IN_Init
===========
*/
void IN_ReInit (void)
{
#ifdef USINGRAWINPUT
	if (in_rawinput.value)
	{
		IN_RawInput_Init();
	}
#endif

	IN_StartupMouse ();
	IN_StartupJoystick ();
//	IN_ActivateMouse();
}

void IN_Init (void)
{
	//keyboard variables
	Cvar_Register (&cl_keypad, "Input Controls");

	// mouse variables
	Cvar_Register (&m_filter, "Input Controls");
	Cvar_Register (&m_accel, "Input Controls");
	Cvar_Register (&m_forcewheel, "Input Controls");
	Cvar_Register (&m_forcewheel_threshold, "Input Controls");

	Cvar_Register (&in_dinput, "Input Controls");
	Cvar_Register (&in_builtinkeymap, "Input Controls");

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

#ifdef IN_XFLIP
	Cvar_Register (&in_xflip, "Input stuff");
#endif

	// joystick variables
	Cvar_Register (&in_joystick, "Joystick variables");

	Cvar_Register (&joy_name, "Joystick variables");
	Cvar_Register (&joy_advanced, "Joystick variables");
	Cvar_Register (&joy_advaxisx, "Joystick variables");
	Cvar_Register (&joy_advaxisy, "Joystick variables");
	Cvar_Register (&joy_advaxisz, "Joystick variables");
	Cvar_Register (&joy_advaxisr, "Joystick variables");
	Cvar_Register (&joy_advaxisu, "Joystick variables");
	Cvar_Register (&joy_advaxisv, "Joystick variables");
	Cvar_Register (&joy_forwardthreshold, "Joystick variables");
	Cvar_Register (&joy_sidethreshold, "Joystick variables");
	Cvar_Register (&joy_pitchthreshold, "Joystick variables");
	Cvar_Register (&joy_yawthreshold, "Joystick variables");
	Cvar_Register (&joy_forwardsensitivity, "Joystick variables");
	Cvar_Register (&joy_sidesensitivity, "Joystick variables");
	Cvar_Register (&joy_pitchsensitivity, "Joystick variables");
	Cvar_Register (&joy_yawsensitivity, "Joystick variables");
	Cvar_Register (&joy_wwhack1, "Joystick variables");
	Cvar_Register (&joy_wwhack2, "Joystick variables");

	Cmd_AddCommand ("force_centerview", Force_CenterView_f);
	Cmd_AddCommand ("joyadvancedupdate", Joy_AdvancedUpdate_f);

	uiWheelMessage = RegisterWindowMessage ( "MSWHEEL_ROLLMSG" );

#ifdef USINGRAWINPUT
	Cvar_Register (&in_rawinput, "Input Controls");
	Cvar_Register (&in_rawinput_keyboard, "Input Controls");
	Cvar_Register (&in_rawinput_rdp, "Input Controls");
#endif
}

/*
===========
IN_Shutdown
===========
*/
void IN_Shutdown (void)
{
	mouseinitialized = false;

	IN_DeactivateMouse ();
	IN_ShowMouse ();

	mouseparmsvalid = false;

#ifdef AVAIL_DINPUT
	IN_CloseDInput();
#endif
#ifdef USINGRAWINPUT
	IN_RawInput_DeInit();
#endif
}


/*
===========
IN_MouseEvent
===========
*/
void IN_MouseEvent (int mstate)
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
				Key_Event (0, K_MOUSE1 + i, 0, true);
			}

			if ( !(mstate & (1<<i)) &&
				(sysmouse.oldbuttons & (1<<i)) )
			{
				Key_Event (0, K_MOUSE1 + i, 0, false);
			}
		}

		sysmouse.oldbuttons = mstate;
	}
}

static void ProcessMouse(mouse_t *mouse, float *movements, int pnum)
{
	extern int mousecursor_x, mousecursor_y;
	extern int mousemove_x, mousemove_y;

	int mx, my;
	double mouse_x, mouse_y, mouse_deltadist;
	int mfwt;

	int i;

	int wpnum;
	wpnum = cl.splitclients;
	if (wpnum < 1)
		wpnum = 1;
	if (cl_forcesplitclient.ival)
		wpnum = (cl_forcesplitclient.ival-1) % wpnum;
	else
		wpnum = mouse->playerid % wpnum;
	if (wpnum != pnum)
		return;

	// perform button actions
	for (i=0 ; i<mouse->numbuttons ; i++)
	{
		if ( (mouse->buttons & (1<<i)) &&
			!(mouse->oldbuttons & (1<<i)) )
		{
			Key_Event (pnum, K_MOUSE1 + i, 0, true);
		}

		if ( !(mouse->buttons & (1<<i)) &&
			(mouse->oldbuttons & (1<<i)) )
		{
			Key_Event (pnum, K_MOUSE1 + i, 0, false);
		}
	}
	mouse->oldbuttons = mouse->buttons;

	if (m_forcewheel.value)
	{
		mfwt = (int)m_forcewheel_threshold.value;
		if (mfwt)
		{
			while(mouse->wheeldelta <= -mfwt)
			{
				Key_Event (pnum, K_MWHEELUP, 0, true);
				Key_Event (pnum, K_MWHEELUP, 0, false);
				mouse->wheeldelta += mfwt;
			}

			while(mouse->wheeldelta >= mfwt)
			{
				Key_Event (pnum, K_MWHEELDOWN, 0, true);
				Key_Event (pnum, K_MWHEELDOWN, 0, false);
				mouse->wheeldelta -= mfwt;
			}
		}

		if (m_forcewheel.value < 2)
			mouse->wheeldelta = 0;
	}

	mx = mouse->delta[0];
	mouse->delta[0]=0;
	my = mouse->delta[1];
	mouse->delta[1]=0;


#ifdef IN_XFLIP
	if(in_xflip.value) mx *= -1;
#endif

	mousemove_x += mx;
	mousemove_y += my;

	if (Key_MouseShouldBeFree())
	{
		mousecursor_x += mx;
		mousecursor_y += my;

		if (mousecursor_y<0)
			mousecursor_y=0;
		if (mousecursor_x<0)
			mousecursor_x=0;

		if (mousecursor_x >= vid.width)
			mousecursor_x = vid.width - 1;

		if (mousecursor_y >= vid.height)
			mousecursor_y = vid.height - 1;
		mx=my=0;
	}
#ifdef VM_UI
	else
	{
		if (UI_MousePosition(mx, my))
		{
			mx = 0;
			my = 0;
		}
	}
#endif

#ifdef PEXT_CSQC
	if (CSQC_MouseMove(mx, my))
	{
		mx = 0;
		my = 0;
	}
#endif

	if (m_filter.value)
	{
		double fraction = bound(0, m_filter.value, 2) * 0.5;
		mouse_x = (mx*(1-fraction) + mouse->old_delta[0]*fraction);
		mouse_y = (my*(1-fraction) + mouse->old_delta[1]*fraction);
	}
	else
	{
		mouse_x = mx;
		mouse_y = my;
	}

	mouse->old_delta[0] = mx;
	mouse->old_delta[1] = my;

	if (m_accel.value) {
		mouse_deltadist = sqrt(mx*mx + my*my);
		mouse_x *= (mouse_deltadist*m_accel.value + sensitivity.value*in_sensitivityscale);
		mouse_y *= (mouse_deltadist*m_accel.value + sensitivity.value*in_sensitivityscale);
	} else {
		mouse_x *= sensitivity.value*in_sensitivityscale;
		mouse_y *= sensitivity.value*in_sensitivityscale;
	}

	if (cl.stats[pnum][STAT_VIEWZOOM])
	{
		mouse_x *= cl.stats[pnum][STAT_VIEWZOOM]/255.0f;
		mouse_y *= cl.stats[pnum][STAT_VIEWZOOM]/255.0f;
	}


	if (!movements)
	{
		return;
	}

//	if (cl.paused)
//		return;

// add mouse X/Y movement to cmd
	if ( (in_strafe.state[pnum] & 1) || (lookstrafe.value && (in_mlook.state[pnum] & 1) ))
		movements[1] += m_side.value * mouse_x;
	else
	{
//		if ((int)((cl.viewangles[pnum][PITCH]+89.99)/180) & 1)
//			mouse_x *= -1;
		cl.viewangles[pnum][YAW] -= m_yaw.value * mouse_x;
	}

	if (in_mlook.state[pnum] & 1)
		V_StopPitchDrift (pnum);

	if ( (in_mlook.state[pnum] & 1) && !(in_strafe.state[pnum] & 1))
	{
		cl.viewangles[pnum][PITCH] += m_pitch.value * mouse_y;

		CL_ClampPitch(pnum);
	}
	else
	{
		if ((in_strafe.state[pnum] & 1) && noclip_anglehack)
			movements[2] -= m_forward.value * mouse_y;
		else
			movements[0] -= m_forward.value * mouse_y;
	}

}


/*
===========
IN_MouseMove
===========
*/
void IN_MouseMove (float *movements, int pnum)
{
	POINT		current_pos;

	extern int mousecursor_x, mousecursor_y;
	extern int window_x, window_y;

	if (!mouseactive)
	{
		GetCursorPos (&current_pos);
		mousecursor_x = current_pos.x-window_x;
		mousecursor_y = current_pos.y-window_y;

		mousecursor_x *= vid.width/(float)vid.pixelwidth;
		mousecursor_y *= vid.height/(float)vid.pixelheight;

#ifdef VM_UI
		if (!Key_MouseShouldBeFree())
			UI_MousePosition(mousecursor_x, mousecursor_y);
#endif

		return;
	}

#ifdef AVAIL_DINPUT
	if (dinput)
	{
		DIDEVICEOBJECTDATA	od;
		DWORD				dwElements;
		HRESULT				hr;

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
					sysmouse.delta[0] += od.dwData;
					break;

				case DIMOFS_Y:
					sysmouse.delta[1] += od.dwData;
					break;

				case DIMOFS_Z:
					if (m_forcewheel.value >= 2)
						sysmouse.wheeldelta -= (signed int)od.dwData;
					else if (m_forcewheel.value)
					{
						int mfwt = (int)m_forcewheel_threshold.value;

						if ((signed int)od.dwData > mfwt)
							sysmouse.wheeldelta -= mfwt;
						else if ((signed int)od.dwData < -mfwt)
							sysmouse.wheeldelta += mfwt;
					}
					break;

				case DIMOFS_BUTTON0:
					if (od.dwData & 0x80)
						sysmouse.buttons |= 1;
					else
						sysmouse.buttons &= ~1;
					break;

				case DIMOFS_BUTTON1:
					if (od.dwData & 0x80)
						sysmouse.buttons |= (1 << 1);
					else
						sysmouse.buttons &= ~(1 << 1);
					break;

				case DIMOFS_BUTTON2:
					if (od.dwData & 0x80)
						sysmouse.buttons |= (1 << 2);
					else
						sysmouse.buttons &= ~(1 << 2);
					break;

				case DIMOFS_BUTTON3:
					if (od.dwData & 0x80)
						sysmouse.buttons |= (1 << 3);
					else
						sysmouse.buttons &= ~(1 << 3);
					break;

#if (DIRECTINPUT_VERSION >= DINPUT_VERSION_DX7)
				case DIMOFS_BUTTON4:
					if (od.dwData & 0x80)
						sysmouse.buttons |= (1 << 4);
					else
						sysmouse.buttons &= ~(1 << 4);
					break;

				case DIMOFS_BUTTON5:
					if (od.dwData & 0x80)
						sysmouse.buttons |= (1 << 5);
					else
						sysmouse.buttons &= ~(1 << 5);
					break;

				case DIMOFS_BUTTON6:
					if (od.dwData & 0x80)
						sysmouse.buttons |= (1 << 6);
					else
						sysmouse.buttons &= ~(1 << 6);
					break;

				case DIMOFS_BUTTON7:
					if (od.dwData & 0x80)
						sysmouse.buttons |= (1 << 7);
					else
						sysmouse.buttons &= ~(1 << 7);
					break;
#endif
			}
		}
	}
	else
#endif
	{
		IN_Accumulate();

		sysmouse.buttons = sysmouse.oldbuttons;	//don't do it!!! Our buttons are event driven. We don't want to merge em and forget do we now?
	}

#ifdef USINGRAWINPUT
	if (rawmicecount)
	{
		int x;
		for (x = 0; x < rawmicecount; x++)
		{
			ProcessMouse(rawmice + x, movements, pnum);
		}
	}
#endif

	ProcessMouse(&sysmouse,		movements, pnum);
}


/*
===========
IN_Move
===========
*/
void IN_Move (float *movements, int pnum)
{

	if (ActiveApp && !Minimized)
	{
		IN_MouseMove (movements, pnum);
		if (pnum == 1 || cl.splitclients<2)
			IN_JoyMove (movements, pnum);
	}
}


/*
===========
IN_Accumulate
===========
*/
void IN_Accumulate (void)
{
	if (mouseactive && !dinput)
	{
#ifdef USINGRAWINPUT
		if (rawmicecount)
		{
		}
		else
#endif
		{
			POINT		current_pos;

			GetCursorPos (&current_pos);

			sysmouse.delta[0] += current_pos.x - window_center_x;
			sysmouse.delta[1] += current_pos.y - window_center_y;
		}

	// force the mouse to the center, so there's room to move
		SetCursorPos (window_center_x, window_center_y);
	}
}

#ifdef USINGRAWINPUT
void IN_RawInput_MouseRead(void)
{
	int pnum;
	int i, tbuttons, j;

	// find mouse in our mouse list
	for (i = 0; i < rawmicecount; i++)
	{
		if (rawmice[i].handles.rawinputhandle == raw->header.hDevice)
			break;
	}

	if (i == rawmicecount) // we're not tracking this device
		return;

	pnum = cl.splitclients;
	if (pnum < 1)
		pnum = 1;
	if (cl_forcesplitclient.ival)
		pnum = (cl_forcesplitclient.ival-1) % pnum;
	else
		pnum = rawmice[i].playerid % pnum;

	// movement
	if (raw->data.mouse.usFlags & MOUSE_MOVE_ABSOLUTE)
	{
		if (rawmice[i].pos[0] != RI_INVALID_POS)
		{
			rawmice[i].delta[0] += raw->data.mouse.lLastX - rawmice[i].pos[0];
			rawmice[i].delta[1] += raw->data.mouse.lLastY - rawmice[i].pos[1];
		}
		rawmice[i].pos[0] = raw->data.mouse.lLastX;
		rawmice[i].pos[1] = raw->data.mouse.lLastY;
	}
	else // RELATIVE
	{
		rawmice[i].delta[0] += raw->data.mouse.lLastX;
		rawmice[i].delta[1] += raw->data.mouse.lLastY;
		rawmice[i].pos[0] = RI_INVALID_POS;
	}

	// buttons
	if (raw->data.mouse.usButtonFlags & RI_MOUSE_BUTTON_1_DOWN)
		Key_Event(pnum, K_MOUSE1, 0, true);
	if (raw->data.mouse.usButtonFlags & RI_MOUSE_BUTTON_1_UP)
		Key_Event(pnum, K_MOUSE1, 0, false);
	if (raw->data.mouse.usButtonFlags & RI_MOUSE_BUTTON_2_DOWN)
		Key_Event(pnum, K_MOUSE2, 0, true);
	if (raw->data.mouse.usButtonFlags & RI_MOUSE_BUTTON_2_UP)
		Key_Event(pnum, K_MOUSE2, 0, false);
	if (raw->data.mouse.usButtonFlags & RI_MOUSE_BUTTON_3_DOWN)
		Key_Event(pnum, K_MOUSE3, 0, true);
	if (raw->data.mouse.usButtonFlags & RI_MOUSE_BUTTON_3_UP)
		Key_Event(pnum, K_MOUSE3, 0, false);
	if (raw->data.mouse.usButtonFlags & RI_MOUSE_BUTTON_4_DOWN)
		Key_Event(pnum, K_MOUSE4, 0, true);
	if (raw->data.mouse.usButtonFlags & RI_MOUSE_BUTTON_4_UP)
		Key_Event(pnum, K_MOUSE4, 0, false);
	if (raw->data.mouse.usButtonFlags & RI_MOUSE_BUTTON_5_DOWN)
		Key_Event(pnum, K_MOUSE5, 0, true);
	if (raw->data.mouse.usButtonFlags & RI_MOUSE_BUTTON_5_UP)
		Key_Event(pnum, K_MOUSE5, 0, false);

	// mouse wheel
	if (raw->data.mouse.usButtonFlags & RI_MOUSE_WHEEL)
	{      // If the current message has a mouse_wheel message
		if ((SHORT)raw->data.mouse.usButtonData > 0)
		{
			Key_Event(pnum, K_MWHEELUP, 0, true);
			Key_Event(pnum, K_MWHEELUP, 0, false);
		}
		if ((SHORT)raw->data.mouse.usButtonData < 0)
		{
			Key_Event(pnum, K_MWHEELDOWN, 0, true);
			Key_Event(pnum, K_MWHEELDOWN, 0, false);
		}
	}

	// extra buttons
	tbuttons = raw->data.mouse.ulRawButtons & RI_RAWBUTTON_MASK;
	for (j=6 ; j<rawmice[i].numbuttons ; j++)
	{
		if ( (tbuttons & (1<<j)) && !(rawmice[i].buttons & (1<<j)) )
		{
			Key_Event (pnum, K_MOUSE1 + j, 0, true);
		}

		if ( !(tbuttons & (1<<j)) && (rawmice[i].buttons & (1<<j)) )
		{
			Key_Event (pnum, K_MOUSE1 + j, 0, false);
		}

	}

	rawmice[i].buttons &= ~RI_RAWBUTTON_MASK;
	rawmice[i].buttons |= tbuttons;
}

void IN_RawInput_KeyboardRead(void)
{
	int i;
	int pnum;
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

	down = !(raw->data.keyboard.Flags & RI_KEY_BREAK);
	wParam = (-down) & 0xC0000000;
	lParam = MapVirtualKey(raw->data.keyboard.VKey, 0)<<16;

	pnum = cl.splitclients;
	if (pnum < 1)
		pnum = 1;
	if (cl_forcesplitclient.ival)
		pnum = (cl_forcesplitclient.ival-1) % pnum;
	else
		pnum = rawkbd[i].playerid % pnum;

 	IN_TranslateKeyEvent(wParam, lParam, down, pnum);
}

void IN_RawInput_Read(HANDLE in_device_handle)
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

	IN_RawInput_MouseRead();
	IN_RawInput_KeyboardRead();
}
#else
void IN_RawInput_Read(HANDLE in_device_handle)
{
}
#endif

/*
===================
IN_ClearStates
===================
*/
void IN_ClearStates (void)
{

	if (mouseactive)
	{
		memset(&sysmouse, 0, sizeof(sysmouse));
		sysmouse.numbuttons = 10;
	}
}


/*
===============
IN_StartupJoystick
===============
*/
void IN_StartupJoystick (void)
{
	int			numdevs;
	JOYCAPS		jc;
	MMRESULT	mmr;

 	// assume no joystick
	joy_avail = false;

	// abort startup if user requests no joystick
	if ( COM_CheckParm ("-nojoy") )
		return;

	// verify joystick driver is present
	if ((numdevs = joyGetNumDevs ()) == 0)
	{
		Con_Printf ("joystick not found -- driver not present\n");
		return;
	}

	mmr = JOYERR_UNPLUGGED;

	// cycle through the joystick ids for the first valid one
	for (joy_id=0 ; joy_id<numdevs ; joy_id++)
	{
		memset (&ji, 0, sizeof(ji));
		ji.dwSize = sizeof(ji);
		ji.dwFlags = JOY_RETURNCENTERED;

		if ((mmr = joyGetPosEx (joy_id, &ji)) == JOYERR_NOERROR)
			break;
	}

	// abort startup if we didn't find a valid joystick
	if (mmr != JOYERR_NOERROR)
	{
		Con_Printf ("joystick not found -- no valid joysticks (%x)\n", mmr);
		return;
	}

	// get the capabilities of the selected joystick
	// abort startup if command fails
	memset (&jc, 0, sizeof(jc));
	if ((mmr = joyGetDevCaps (joy_id, &jc, sizeof(jc))) != JOYERR_NOERROR)
	{
		Con_Printf ("joystick not found -- invalid joystick capabilities (%x)\n", mmr);
		return;
	}

	// save the joystick's number of buttons and POV status
	joy_numbuttons = jc.wNumButtons;
	joy_haspov = jc.wCaps & JOYCAPS_HASPOV;

	// old button and POV states default to no buttons pressed
	joy_oldbuttonstate = joy_oldpovstate = 0;

	// mark the joystick as available and advanced initialization not completed
	// this is needed as cvars are not available during initialization

	joy_avail = true;
	joy_advancedinit = false;

	Con_Printf ("joystick detected\n");
}


/*
===========
RawValuePointer
===========
*/
PDWORD RawValuePointer (int axis)
{
	switch (axis)
	{
	case JOY_AXIS_X:
		return &ji.dwXpos;
	case JOY_AXIS_Y:
		return &ji.dwYpos;
	case JOY_AXIS_Z:
		return &ji.dwZpos;
	case JOY_AXIS_R:
		return &ji.dwRpos;
	case JOY_AXIS_U:
		return &ji.dwUpos;
	case JOY_AXIS_V:
		return &ji.dwVpos;
	}
	return 0;	//compiler shut up.
}


/*
===========
Joy_AdvancedUpdate_f
===========
*/
void Joy_AdvancedUpdate_f (void)
{

	// called once by IN_ReadJoystick and by user whenever an update is needed
	// cvars are now available
	int	i;
	DWORD dwTemp;

	// initialize all the maps
	for (i = 0; i < JOY_MAX_AXES; i++)
	{
		dwAxisMap[i] = AxisNada;
		dwControlMap[i] = JOY_ABSOLUTE_AXIS;
		pdwRawValue[i] = RawValuePointer(i);
	}

	if( joy_advanced.value == 0.0)
	{
		// default joystick initialization
		// 2 axes only with joystick control
		dwAxisMap[JOY_AXIS_X] = AxisTurn;
		// dwControlMap[JOY_AXIS_X] = JOY_ABSOLUTE_AXIS;
		dwAxisMap[JOY_AXIS_Y] = AxisForward;
		// dwControlMap[JOY_AXIS_Y] = JOY_ABSOLUTE_AXIS;
	}
	else
	{
		if (Q_strcmp (joy_name.string, "joystick") != 0)
		{
			// notify user of advanced controller
			Con_Printf ("\n%s configured\n\n", joy_name.string);
		}

		// advanced initialization here
		// data supplied by user via joy_axisn cvars
		dwTemp = (DWORD) joy_advaxisx.value;
		dwAxisMap[JOY_AXIS_X] = dwTemp & 0x0000000f;
		dwControlMap[JOY_AXIS_X] = dwTemp & JOY_RELATIVE_AXIS;
		dwTemp = (DWORD) joy_advaxisy.value;
		dwAxisMap[JOY_AXIS_Y] = dwTemp & 0x0000000f;
		dwControlMap[JOY_AXIS_Y] = dwTemp & JOY_RELATIVE_AXIS;
		dwTemp = (DWORD) joy_advaxisz.value;
		dwAxisMap[JOY_AXIS_Z] = dwTemp & 0x0000000f;
		dwControlMap[JOY_AXIS_Z] = dwTemp & JOY_RELATIVE_AXIS;
		dwTemp = (DWORD) joy_advaxisr.value;
		dwAxisMap[JOY_AXIS_R] = dwTemp & 0x0000000f;
		dwControlMap[JOY_AXIS_R] = dwTemp & JOY_RELATIVE_AXIS;
		dwTemp = (DWORD) joy_advaxisu.value;
		dwAxisMap[JOY_AXIS_U] = dwTemp & 0x0000000f;
		dwControlMap[JOY_AXIS_U] = dwTemp & JOY_RELATIVE_AXIS;
		dwTemp = (DWORD) joy_advaxisv.value;
		dwAxisMap[JOY_AXIS_V] = dwTemp & 0x0000000f;
		dwControlMap[JOY_AXIS_V] = dwTemp & JOY_RELATIVE_AXIS;
	}

	// compute the axes to collect from DirectInput
	joy_flags = JOY_RETURNCENTERED | JOY_RETURNBUTTONS | JOY_RETURNPOV;
	for (i = 0; i < JOY_MAX_AXES; i++)
	{
		if (dwAxisMap[i] != AxisNada)
		{
			joy_flags |= dwAxisFlags[i];
		}
	}
}


/*
===========
IN_Commands
===========
*/
void IN_Commands (void)
{
	int		i, key_index;
	DWORD	buttonstate, povstate;

	if (!joy_avail)
	{
		return;
	}


	// loop through the joystick buttons
	// key a joystick event or auxillary event for higher number buttons for each state change
	buttonstate = ji.dwButtons;
	for (i=0 ; i < joy_numbuttons ; i++)
	{
		if ( (buttonstate & (1<<i)) && !(joy_oldbuttonstate & (1<<i)) )
		{
			key_index = (i < 4) ? K_JOY1 : K_AUX1;
			Key_Event (0, key_index + i, 0, true);
		}

		if ( !(buttonstate & (1<<i)) && (joy_oldbuttonstate & (1<<i)) )
		{
			key_index = (i < 4) ? K_JOY1 : K_AUX1;
			Key_Event (0, key_index + i, 0, false);
		}
	}
	joy_oldbuttonstate = buttonstate;

	if (joy_haspov)
	{
		// convert POV information into 4 bits of state information
		// this avoids any potential problems related to moving from one
		// direction to another without going through the center position
		povstate = 0;
		if(ji.dwPOV != JOY_POVCENTERED)
		{
			if (ji.dwPOV == JOY_POVFORWARD)
				povstate |= 0x01;
			if (ji.dwPOV == JOY_POVRIGHT)
				povstate |= 0x02;
			if (ji.dwPOV == JOY_POVBACKWARD)
				povstate |= 0x04;
			if (ji.dwPOV == JOY_POVLEFT)
				povstate |= 0x08;
		}
		// determine which bits have changed and key an auxillary event for each change
		for (i=0 ; i < 4 ; i++)
		{
			if ( (povstate & (1<<i)) && !(joy_oldpovstate & (1<<i)) )
			{
				Key_Event (0, K_AUX29 + i, 0, true);
			}

			if ( !(povstate & (1<<i)) && (joy_oldpovstate & (1<<i)) )
			{
				Key_Event (0, K_AUX29 + i, 0, false);
			}
		}
		joy_oldpovstate = povstate;
	}
}


/*
===============
IN_ReadJoystick
===============
*/
qboolean IN_ReadJoystick (void)
{

	memset (&ji, 0, sizeof(ji));
	ji.dwSize = sizeof(ji);
	ji.dwFlags = joy_flags;

	if (joyGetPosEx (joy_id, &ji) == JOYERR_NOERROR)
	{
		// this is a hack -- there is a bug in the Logitech WingMan Warrior DirectInput Driver
		// rather than having 32768 be the zero point, they have the zero point at 32668
		// go figure -- anyway, now we get the full resolution out of the device
		if (joy_wwhack1.value != 0.0)
		{
			ji.dwUpos += 100;
		}
		return true;
	}
	else
	{
		// read error occurred
		// turning off the joystick seems too harsh for 1 read error,
		// but what should be done?
		// Con_Printf ("IN_ReadJoystick: no response\n");
		// joy_avail = false;
		return false;
	}
}


/*
===========
IN_JoyMove
===========
*/
void IN_JoyMove (float *movements, int pnum)
{
	float	speed, aspeed;
	float	fAxisValue, fTemp;
	int		i;

	// complete initialization if first time in
	// this is needed as cvars are not available at initialization time
	if( joy_advancedinit != true )
	{
		Joy_AdvancedUpdate_f();
		joy_advancedinit = true;
	}

	// verify joystick is available and that the user wants to use it
	if (!joy_avail || !in_joystick.value)
	{
		return;
	}

	// collect the joystick data, if possible
	if (IN_ReadJoystick () != true)
	{
		return;
	}

	if (in_speed.state[pnum] & 1)
		speed = cl_movespeedkey.value;
	else
		speed = 1;
	aspeed = speed * host_frametime;

	// loop through the axes
	for (i = 0; i < JOY_MAX_AXES; i++)
	{
		// get the floating point zero-centered, potentially-inverted data for the current axis
		fAxisValue = (float) *pdwRawValue[i];
		// move centerpoint to zero
		fAxisValue -= 32768.0;

		if (joy_wwhack2.value != 0.0)
		{
			if (dwAxisMap[i] == AxisTurn)
			{
				// this is a special formula for the Logitech WingMan Warrior
				// y=ax^b; where a = 300 and b = 1.3
				// also x values are in increments of 800 (so this is factored out)
				// then bounds check result to level out excessively high spin rates
				fTemp = 300.0 * pow(abs(fAxisValue) / 800.0, 1.3);
				if (fTemp > 14000.0)
					fTemp = 14000.0;
				// restore direction information
				fAxisValue = (fAxisValue > 0.0) ? fTemp : -fTemp;
			}
		}

		// convert range from -32768..32767 to -1..1
		fAxisValue /= 32768.0;

		switch (dwAxisMap[i])
		{
		case AxisForward:
			if ((joy_advanced.value == 0.0) && (in_mlook.state[pnum] & 1))
			{
				// user wants forward control to become look control
				if (fabs(fAxisValue) > joy_pitchthreshold.value)
				{
					// if mouse invert is on, invert the joystick pitch value
					// only absolute control support here (joy_advanced is false)
					if (m_pitch.value < 0.0)
					{
						cl.viewangles[pnum][PITCH] -= (fAxisValue * joy_pitchsensitivity.value) * aspeed * cl_pitchspeed.value;
					}
					else
					{
						cl.viewangles[pnum][PITCH] += (fAxisValue * joy_pitchsensitivity.value) * aspeed * cl_pitchspeed.value;
					}
					V_StopPitchDrift(pnum);
				}
				else
				{
					// no pitch movement
					// disable pitch return-to-center unless requested by user
					// *** this code can be removed when the lookspring bug is fixed
					// *** the bug always has the lookspring feature on
					if(lookspring.value == 0.0)
						V_StopPitchDrift(pnum);
				}
			}
			else
			{
				// user wants forward control to be forward control
				if (fabs(fAxisValue) > joy_forwardthreshold.value)
				{
					movements[0] += (fAxisValue * joy_forwardsensitivity.value) * speed * cl_forwardspeed.value;
				}
			}
			break;

		case AxisSide:
			if (fabs(fAxisValue) > joy_sidethreshold.value)
			{
				movements[1] += (fAxisValue * joy_sidesensitivity.value) * speed * cl_sidespeed.value;
			}
			break;

		case AxisTurn:
			if ((in_strafe.state[pnum] & 1) || (lookstrafe.value && (in_mlook.state[pnum] & 1)))
			{
				// user wants turn control to become side control
				if (fabs(fAxisValue) > joy_sidethreshold.value)
				{
					movements[2] -= (fAxisValue * joy_sidesensitivity.value) * speed * cl_sidespeed.value;
				}
			}
			else
			{
				// user wants turn control to be turn control
				if (fabs(fAxisValue) > joy_yawthreshold.value)
				{
					if(dwControlMap[i] == JOY_ABSOLUTE_AXIS)
					{
						cl.viewangles[pnum][YAW] += (fAxisValue * joy_yawsensitivity.value) * aspeed * cl_yawspeed.value;
					}
					else
					{
						cl.viewangles[pnum][YAW] += (fAxisValue * joy_yawsensitivity.value) * speed * 180.0;
					}

				}
			}
			break;

		case AxisLook:
			if (in_mlook.state[pnum] & 1)
			{
				if (fabs(fAxisValue) > joy_pitchthreshold.value)
				{
					// pitch movement detected and pitch movement desired by user
					if(dwControlMap[i] == JOY_ABSOLUTE_AXIS)
					{
						cl.viewangles[pnum][PITCH] += (fAxisValue * joy_pitchsensitivity.value) * aspeed * cl_pitchspeed.value;
					}
					else
					{
						cl.viewangles[pnum][PITCH] += (fAxisValue * joy_pitchsensitivity.value) * speed * 180.0;
					}
					V_StopPitchDrift(pnum);
				}
				else
				{
					// no pitch movement
					// disable pitch return-to-center unless requested by user
					// *** this code can be removed when the lookspring bug is fixed
					// *** the bug always has the lookspring feature on
					if(lookspring.value == 0.0)
						V_StopPitchDrift(pnum);
				}
			}
			break;

		default:
			break;
		}
	}

	CL_ClampPitch(pnum);
}

static qbyte        scantokey[128] =
					{
//  0           1       2       3       4       5       6       7
//  8           9       A       B       C       D       E       F
	0  ,		27,		'1',		'2',		'3',	'4',		'5',			'6',
	'7',		'8',	'9',		'0',		'-',	'=',		K_BACKSPACE,	9,			// 0
	'q',		'w',	'e',		'r',		't',	'y',		'u',			'i',
	'o',		'p',	'[',		']',		13 ,	K_CTRL,		'a',			's',		// 1
	'd',		'f',	'g',		'h',		'j',	'k',		'l',			';',
	'\'',		'`',	K_SHIFT,	'\\',		'z',	'x',		'c',			'v',		// 2
	'b',		'n',	'm',		',',		'.',	'/',		K_SHIFT,		'*',
	K_ALT,		' ',	K_CAPSLOCK,	K_F1,		K_F2,	K_F3,		K_F4,			K_F5,		// 3
	K_F6,		K_F7,	K_F8,		K_F9,		K_F10,	K_PAUSE,	K_SCRLCK,		K_HOME,
	K_UPARROW,	K_PGUP,	'-',		K_LEFTARROW,'5',	K_RIGHTARROW,'+',			K_END,		// 4
	K_DOWNARROW,K_PGDN,	K_INS,		K_DEL,		0,      0,			0,				K_F11,
	K_F12,		0,		0,			0,			0,		0,			0,				0,			// 5
	0,			0,		0,			0,			0,		0,			0,				0,
	0,			0,		0,			0,			0,		0,			0,				0,			// 6
	0,			0,		0,			0,			0,		0,			0,				0,
	0,			0,		0,			0,			0,		0,			0,				0			// 7
					};
/*
static qbyte        shiftscantokey[128] =
					{
//  0           1       2       3       4       5       6       7
//  8           9       A       B       C       D       E       F
	0  ,    27,     '!',    '@',    '#',    '$',    '%',    '^',
	'&',    '*',    '(',    ')',    '_',    '+',    K_BACKSPACE, 9, // 0
	'Q',    'W',    'E',    'R',    'T',    'Y',    'U',    'I',
	'O',    'P',    '{',    '}',    13 ,    K_CTRL,'A',  'S',      // 1
	'D',    'F',    'G',    'H',    'J',    'K',    'L',    ':',
	'"' ,    '~',    K_SHIFT,'|',  'Z',    'X',    'C',    'V',      // 2
	'B',    'N',    'M',    '<',    '>',    '?',    K_SHIFT,'*',
	K_ALT,' ',   K_CAPSLOCK  ,    K_F1, K_F2, K_F3, K_F4, K_F5,   // 3
	K_F6, K_F7, K_F8, K_F9, K_F10, K_PAUSE  ,    K_SCRLCK  , K_HOME,
	K_UPARROW,K_PGUP,'_',K_LEFTARROW,'%',K_RIGHTARROW,'+',K_END, //4
	K_DOWNARROW,K_PGDN,K_INS,K_DEL,0,0,             0,              K_F11,
	K_F12,  0  ,    0  ,    0  ,    0  ,    0  ,    0  ,    0,        // 5
	0  ,    0  ,    0  ,    0  ,    0  ,    0  ,    0  ,    0,
	0  ,    0  ,    0  ,    0  ,    0  ,    0  ,    0  ,    0,        // 6
	0  ,    0  ,    0  ,    0  ,    0  ,    0  ,    0  ,    0,
	0  ,    0  ,    0  ,    0  ,    0  ,    0  ,    0  ,    0         // 7
					};
*/

/*
=======
MapKey

Map from windows to quake keynums
=======
*/
static int MapKey (int vkey)
{
	int key;
	key = (vkey>>16)&255;

	if (cl_keypad.value)
	{
		switch (key)
		{
		case 0x1c:
			if ((vkey>>24)&1)	//not compleatly seperate
				return K_KP_ENTER;
			break;
		case 0x47:
			if (!((vkey>>24)&1))	//not compleatly seperate
				return K_KP_HOME;
			break;
		case 0x48:
			if (!((vkey>>24)&1))	//not compleatly seperate
				return K_KP_UPARROW;
			break;
		case 0x49:
			if (!((vkey>>24)&1))	//not compleatly seperate
				return K_KP_PGUP;
			break;
		case 0x4b:
			if (!((vkey>>24)&1))	//not compleatly seperate
				return K_KP_LEFTARROW;
			break;
		case 0x4c:
			return K_KP_5;
		case 0x4d:
			if (!((vkey>>24)&1))	//not compleatly seperate
				return K_KP_RIGHTARROW;
			break;
		case 0x4f:
			if (!((vkey>>24)&1))	//not compleatly seperate
				return K_KP_END;
			break;
		case 0x50:
			if (!((vkey>>24)&1))	//not compleatly seperate
				return K_KP_DOWNARROW;
			break;
		case 0x51:
			if (!((vkey>>24)&1))	//not compleatly seperate
				return K_KP_PGDN;
			break;
		case 0x52:
			if (!((vkey>>24)&1))	//not compleatly seperate
				return K_KP_INS;
			break;
		case 0x53:
			if (!((vkey>>24)&1))	//not compleatly seperate
				return K_KP_DEL;
			break;
		case 0x35:
			if ((vkey>>24)&1)	//not compleatly seperate
				return K_KP_SLASH;
			break;
		case 0x4a:
			return K_KP_MINUS;
		case 0x4e:
			return K_KP_PLUS;
		case 0x45:
			if ((vkey>>24)&1)	//not compleatly seperate
				return K_KP_NUMLOCK;
			break;
		case 0x37:
			return K_KP_STAR;
//		case 0x
//			return K_KP_EQUALS;
		}
	}
	if (key > 127)
		return 0;
	if (scantokey[key] == 0)
		Con_DPrintf("key 0x%02x has no translation\n", key);
	return scantokey[key];
}

void IN_TranslateKeyEvent(WPARAM wParam, LPARAM lParam, qboolean down, int pnum)
{
	extern cvar_t in_builtinkeymap;
	int qcode;
	int unicode;

	qcode = MapKey(lParam);
	unicode = (qcode < 128)?qcode:0;

	if (WinNT && !in_builtinkeymap.value)
	{
		BYTE	keystate[256];
		WCHAR	wchars[2];
		GetKeyboardState(keystate);
		if (ToUnicode(wParam, HIWORD(lParam), keystate, wchars, sizeof(wchars)/sizeof(wchars[0]), 0) > 0)
		{
			//ignore if more, its probably a compose and > 65535 anyway. we can't represent that.
//			if (!wchars[1])
				unicode = wchars[0];
		}
	}

	Key_Event (pnum, qcode, unicode, down);
}
