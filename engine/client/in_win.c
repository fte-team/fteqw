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

void INS_Accumulate (void);

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
static cvar_t	m_filter = CVAR("m_filter","0");
static cvar_t  m_accel = CVAR("m_accel", "0");
static cvar_t	in_dinput = CVARF("in_dinput","0", CVAR_ARCHIVE);
static cvar_t	in_builtinkeymap = CVARF("in_builtinkeymap", "0", CVAR_ARCHIVE);
static cvar_t in_simulatemultitouch = CVAR("in_simulatemultitouch", "0");

static cvar_t	m_accel_noforce = CVAR("m_accel_noforce", "0");
static cvar_t  m_threshold_noforce = CVAR("m_threshold_noforce", "0");

static cvar_t	cl_keypad = CVAR("cl_keypad", "1");
extern cvar_t cl_forcesplitclient;

extern float multicursor_x[8], multicursor_y[8];
extern qboolean multicursor_active[8];

typedef struct {
	union {
		HANDLE rawinputhandle;
	} handles;

	int qdeviceid;
} keyboard_t;

typedef struct {
	union {
		HANDLE rawinputhandle; // raw input
	} handles;

	int numbuttons;
	int oldbuttons;
	int qdeviceid;	/*the device id controls which player slot it controls, if splitscreen splits it that way*/
} mouse_t;

static mouse_t sysmouse;

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

static DWORD	dwAxisFlags[JOY_MAX_AXES] =
{
	JOY_RETURNX, JOY_RETURNY, JOY_RETURNZ, JOY_RETURNR, JOY_RETURNU, JOY_RETURNV
};

static DWORD	dwAxisMap[JOY_MAX_AXES];
static DWORD	dwControlMap[JOY_MAX_AXES];
static PDWORD	pdwRawValue[JOY_MAX_AXES];

// none of these cvars are saved over a session
// this means that advanced controller configuration needs to be executed
// each time.  this avoids any problems with getting back to a default usage
// or when changing from one controller to another.  this way at least something
// works.
static cvar_t	in_joystick				= CVARF("joystick","0", CVAR_ARCHIVE);
static cvar_t	joy_name				= CVAR("joyname", "joystick");
static cvar_t	joy_advanced			= CVAR("joyadvanced", "0");
static cvar_t	joy_advaxisx			= CVAR("joyadvaxisx", "0");
static cvar_t	joy_advaxisy			= CVAR("joyadvaxisy", "0");
static cvar_t	joy_advaxisz			= CVAR("joyadvaxisz", "0");
static cvar_t	joy_advaxisr			= CVAR("joyadvaxisr", "0");
static cvar_t	joy_advaxisu			= CVAR("joyadvaxisu", "0");
static cvar_t	joy_advaxisv			= CVAR("joyadvaxisv", "0");
static cvar_t	joy_forwardthreshold	= CVAR("joyforwardthreshold", "0.15");
static cvar_t	joy_sidethreshold		= CVAR("joysidethreshold", "0.15");
static cvar_t	joy_pitchthreshold		= CVAR("joypitchthreshold", "0.15");
static cvar_t	joy_yawthreshold		= CVAR("joyyawthreshold", "0.15");
static cvar_t	joy_forwardsensitivity	= CVAR("joyforwardsensitivity", "-1.0");
static cvar_t	joy_sidesensitivity		= CVAR("joysidesensitivity", "-1.0");
static cvar_t	joy_pitchsensitivity	= CVAR("joypitchsensitivity", "1.0");
static cvar_t	joy_yawsensitivity		= CVAR("joyyawsensitivity", "-1.0");
static cvar_t	joy_wwhack1				= CVAR("joywwhack1", "0.0");
static cvar_t	joy_wwhack2				= CVAR("joywwhack2", "0.0");

static qboolean	joy_avail, joy_advancedinit, joy_haspov;
static DWORD		joy_oldbuttonstate, joy_oldpovstate;

static int			joy_id;
static DWORD		joy_flags;
static DWORD		joy_numbuttons;

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

static cvar_t in_rawinput = CVARD("in_rawinput", "0", "Enables rawinput support for mice in XP onwards. Rawinput permits independant device identification (ie: splitscreen clients can each have their own mouse)");
static cvar_t in_rawinput_keyboard = CVARD("in_rawinput_keyboard", "0", "Enables rawinput support for keyboards in XP onwards as well as just mice. Requires in_rawinput to be set.");
static cvar_t in_rawinput_rdp = CVARD("in_rawinput_rdp", "0", "Activate Remote Desktop Protocol devices too.");

void INS_RawInput_MouseDeRegister(void);
int INS_RawInput_MouseRegister(void);
void INS_RawInput_KeyboardDeRegister(void);
int INS_RawInput_KeyboardRegister(void);
void INS_RawInput_DeInit(void);

#endif

// forward-referenced functions
void INS_StartupJoystick (void);
void Joy_AdvancedUpdate_f (void);
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
	else if (fullscreen || in_simulatemultitouch.ival)
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
		INS_HideMouse();
	else
		INS_ShowMouse();

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
BOOL CALLBACK INS_EnumerateDevices(LPCDIDEVICEINSTANCE inst, LPVOID parm)
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

		IDirectInput7_EnumDevices(g_pdi7, 0, &INS_EnumerateDevices, NULL, DIEDFL_ATTACHEDONLY);

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
	IDirectInput_EnumDevices(g_pdi, 0, &INS_EnumerateDevices, NULL, DIEDFL_ATTACHEDONLY);

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
	Rid.usUsage = 0x02;
	Rid.dwFlags = RIDEV_REMOVE;
	Rid.hwndTarget = NULL;

	(*_RRID)(&Rid, 1, sizeof(Rid));
}

void INS_RawInput_DeInit(void)
{
	if (rawmicecount > 0)
	{
		INS_RawInput_MouseDeRegister();
		Z_Free(rawmice);
		rawmicecount = 0;
	}
	if (rawkbdcount > 0)
	{
		INS_RawInput_KeyboardDeRegister();
		Z_Free(rawkbd);
		rawkbdcount = 0;
	}
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

		if (!(in_rawinput_rdp.value) && INS_RawInput_IsRDPDevice(dname)) // use rdp (cvar)
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

		if (!(in_rawinput_rdp.value) && INS_RawInput_IsRDPDevice(dname)) // use rdp (cvar)
			continue;

		switch (pRawInputDeviceList[i].dwType)
		{
		case RIM_TYPEMOUSE:
			// set handle
			rawmice[rawmicecount].handles.rawinputhandle = pRawInputDeviceList[i].hDevice;
			rawmice[rawmicecount].numbuttons = 10;
			rawmice[rawmicecount].qdeviceid = rawmicecount;
			rawmicecount++;
			break;
		case RIM_TYPEKEYBOARD:
			if (!in_rawinput_keyboard.ival)
				continue;

			rawkbd[rawkbdcount].handles.rawinputhandle = pRawInputDeviceList[i].hDevice;
			rawkbd[rawkbdcount].qdeviceid = rawkbdcount;
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
	if (in_rawinput.value)
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

	Cvar_Register (&in_dinput, "Input Controls");
	Cvar_Register (&in_builtinkeymap, "Input Controls");
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
INS_Shutdown
===========
*/
void INS_Shutdown (void)
{
	mouseinitialized = false;

	INS_DeactivateMouse ();
	INS_ShowMouse ();

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
				IN_KeyEvent (0, true, K_MOUSE1 + i, 0);
			}

			if ( !(mstate & (1<<i)) &&
				(sysmouse.oldbuttons & (1<<i)) )
			{
				IN_KeyEvent (0, false, K_MOUSE1 + i, 0);
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
	extern int window_x, window_y;

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
	if (ActiveApp && !Minimized)
	{
		INS_MouseMove (movements, pnum);
		if (pnum == 1 || cl.splitclients<2)
			INS_JoyMove (movements, pnum);
	}
}


/*
===========
INS_Accumulate
===========
potentially called multiple times per frame.
*/
void INS_Accumulate (void)
{
	static POINT		current_pos;	//static to avoid bugs in vista with largeaddressaware (this is fixed in win7). fixed exe base address prevents this from going above 2gb.

	if (mouseactive && !dinput)
	{
#ifdef USINGRAWINPUT
		//raw input disables the system mouse, to avoid dupes
		if (!rawmicecount)
#endif
		{
			GetCursorPos (&current_pos);

			IN_MouseMove(sysmouse.qdeviceid, false, current_pos.x - window_center_x, current_pos.y - window_center_y, 0, 0);
		}

	// force the mouse to the center, so there's room to move (rawinput ignore this apparently)
		SetCursorPos (window_center_x, window_center_y);
	}

	if (!mouseactive)
	{
		extern int window_x, window_y;
		GetCursorPos (&current_pos);

		IN_MouseMove(sysmouse.qdeviceid, true, current_pos.x-window_x, current_pos.y-window_y, 0, 0);
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

	down = !(raw->data.keyboard.Flags & RI_KEY_BREAK);
	wParam = (-down) & 0xC0000000;
	lParam = MapVirtualKey(raw->data.keyboard.VKey, 0)<<16;

 	INS_TranslateKeyEvent(wParam, lParam, down, rawkbd[i].qdeviceid);
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
void INS_StartupJoystick (void)
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
//		Con_Printf ("joystick not found -- no valid joysticks (%x)\n", mmr);
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
static PDWORD RawValuePointer (int axis)
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

	// called once by INS_ReadJoystick and by user whenever an update is needed
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
INS_Commands
===========
*/
void INS_Commands (void)
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
INS_ReadJoystick
===============
*/
qboolean INS_ReadJoystick (void)
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
		// Con_Printf ("INS_ReadJoystick: no response\n");
		// joy_avail = false;
		return false;
	}
}


/*
===========
INS_JoyMove
===========
*/
void INS_JoyMove (float *movements, int pnum)
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
	if (INS_ReadJoystick () != true)
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
						cl.playerview[pnum].viewanglechange[PITCH] -= (fAxisValue * joy_pitchsensitivity.value) * aspeed * cl_pitchspeed.value;
					}
					else
					{
						cl.playerview[pnum].viewanglechange[PITCH] += (fAxisValue * joy_pitchsensitivity.value) * aspeed * cl_pitchspeed.value;
					}
					V_StopPitchDrift(&cl.playerview[pnum]);
				}
				else
				{
					// no pitch movement
					// disable pitch return-to-center unless requested by user
					// *** this code can be removed when the lookspring bug is fixed
					// *** the bug always has the lookspring feature on
					if(lookspring.value == 0.0)
						V_StopPitchDrift(&cl.playerview[pnum]);
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
						cl.playerview[pnum].viewanglechange[YAW] += (fAxisValue * joy_yawsensitivity.value) * aspeed * cl_yawspeed.value;
					}
					else
					{
						cl.playerview[pnum].viewanglechange[YAW] += (fAxisValue * joy_yawsensitivity.value) * speed * 180.0;
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
						cl.playerview[pnum].viewanglechange[PITCH] += (fAxisValue * joy_pitchsensitivity.value) * aspeed * cl_pitchspeed.value;
					}
					else
					{
						cl.playerview[pnum].viewanglechange[PITCH] += (fAxisValue * joy_pitchsensitivity.value) * speed * 180.0;
					}
					V_StopPitchDrift(&cl.playerview[pnum]);
				}
				else
				{
					// no pitch movement
					// disable pitch return-to-center unless requested by user
					// *** this code can be removed when the lookspring bug is fixed
					// *** the bug always has the lookspring feature on
					if(lookspring.value == 0.0)
						V_StopPitchDrift(&cl.playerview[pnum]);
				}
			}
			break;

		default:
			break;
		}
	}

	CL_ClampPitch(pnum);
}

static qbyte        scantokey[] =
{
//  0           1			2			3			4			5			6				7
//  8           9			A			B			C			D			E				F
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
//  0           1			2			3			4			5			6				7
//  8           9			A			B			C			D			E				F
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
//  0           1			2			3			4			5			6				7
//  8           9			A			B			C			D			E				F
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
//  0           1			2			3			4			5			6				7
//  8           9			A			B			C			D			E				F
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

void INS_TranslateKeyEvent(WPARAM wParam, LPARAM lParam, qboolean down, int qdeviceid)
{
	extern cvar_t in_builtinkeymap;
	int qcode;
	int unicode;
	extern int		keyshift[256];
	extern int		shift_down;

	qcode = MapKey(lParam);

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
		else unicode = 0;
	}
	else
	{
		unicode = (qcode < 128)?qcode:0;
		if (shift_down && unicode < K_MAX && keyshift[unicode])
			unicode = keyshift[unicode]; 
	}

	Key_Event (qdeviceid, qcode, unicode, down);
}
