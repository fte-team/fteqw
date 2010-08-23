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
// winquake.h: Win32-specific Quake header file

#ifndef WINQUAKE_H
#define WINQUAKE_H

#ifdef _WIN32

#if defined(_WIN32) && !defined(WIN32)
#define WIN32 _WIN32
#endif

#ifdef MSVCDISABLEWARNINGS 
#pragma warning( disable : 4229 )  // mgraph gets this
#endif

#define WIN32_LEAN_AND_MEAN
#define byte winbyte
#include <windows.h>
#include <winsock2.h>
#include <mmsystem.h>
#include <mmreg.h>
#define _LPCWAVEFORMATEX_DEFINED


#ifndef WM_MOUSEWHEEL
#define WM_MOUSEWHEEL                   0x020A
#endif

#ifndef SERVERONLY
#ifdef AVAIL_DDRAW
#include <ddraw.h>
#endif
#endif

#undef byte

extern	HINSTANCE	global_hInstance;
extern	int			global_nCmdShow;

extern HWND sys_parentwindow;
extern unsigned int sys_parentwidth;
extern unsigned int sys_parentheight;

#ifndef SERVERONLY

#ifdef _WIN32
LONG CDAudio_MessageHandler(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
#endif

/*
struct soundcardinfo_s {
	int snd_linear_count;	//change in asm_i386.h. MUST be first

	float pitch[MAXSOUNDCHANNELS];
	float yaw[MAXSOUNDCHANNELS];
	float dist[MAXSOUNDCHANNELS];

	void *handle;
	void *(*Lock) (soundcardinfo_t *sc);
	void (*Unlock) (soundcardinfo_t *sc, void *buffer);
	void (*Submit) (soundcardinfo_t *sc);
	void (*Shutdown) (soundcardinfo_t *sc);
	unsigned int (*GetDMAPos) (soundcardinfo_t *sc);
	void (*SetWaterDistortion) (soundcardinfo_t *sc, qboolean underwater);

#ifdef AVAIL_DSOUND
	LPDIRECTSOUND pDS;
	LPDIRECTSOUNDBUFFER pDSBuf;
	LPDIRECTSOUNDBUFFER pDSPBuf;
#endif
	HWAVEOUT hWaveOut;
	HANDLE hData;
	HGLOBAL hWaveHdr;
	HPSTR lpData;
	LPWAVEHDR lpWaveHdr;
	DWORD		mmstarttime;
	DWORD gSndBufSize;

	qboolean selfpainting;

	qboolean inactive_sound;


	int   		paintedtime;
	int			oldpaintedtime;
	int	oldsamplepos;
	int buffers;

	dma_t sn;


	char name[128];

	int snd_sent;
	int snd_completed;

channel_t   channel[MAX_CHANNELS];
int			total_chans;

#ifdef AVAIL_DSOUND
#ifdef _IKsPropertySet_
	LPKSPROPERTYSET	EaxKsPropertiesSet;
#endif
#endif

	int rawstart;
	int rawend;

	struct soundcardinfo_s *next;
};
*/


//void	VID_LockBuffer (void);
//void	VID_UnlockBuffer (void);

#endif

extern HWND			mainwindow;
extern qboolean		ActiveApp, Minimized;

extern qboolean	WinNT;

void IN_UpdateGrabs(int fullscreen, int activeapp);
void IN_RestoreOriginalMouseState (void);
void IN_SetQuakeMouseState (void);
void IN_MouseEvent (int mstate);
void IN_RawInput_Read(HANDLE in_device_handle);

extern qboolean	winsock_lib_initialized;

extern int		window_center_x, window_center_y;
extern RECT		window_rect;

extern qboolean	mouseinitialized;
extern HWND		hwnd_dialog;

//extern HANDLE	hinput, houtput;

void IN_UpdateClipCursor (void);
void CenterWindow(HWND hWndCenter, int width, int height, BOOL lefttopjustify);
void IN_TranslateKeyEvent(WPARAM wParam, LPARAM lParam, qboolean down, int pnum);

void S_BlockSound (void);
void S_UnblockSound (void);

void VID_SetDefaultMode (void);

int (PASCAL FAR *pWSAStartup)(WORD wVersionRequired, LPWSADATA lpWSAData);
int (PASCAL FAR *pWSACleanup)(void);
int (PASCAL FAR *pWSAGetLastError)(void);
SOCKET (PASCAL FAR *psocket)(int af, int type, int protocol);
int (PASCAL FAR *pioctlsocket)(SOCKET s, long cmd, u_long FAR *argp);
int (PASCAL FAR *psetsockopt)(SOCKET s, int level, int optname,
							  const char FAR * optval, int optlen);
int (PASCAL FAR *precvfrom)(SOCKET s, char FAR * buf, int len, int flags,
							struct sockaddr FAR *from, int FAR * fromlen);
int (PASCAL FAR *psendto)(SOCKET s, const char FAR * buf, int len, int flags,
						  const struct sockaddr FAR *to, int tolen);
int (PASCAL FAR *pclosesocket)(SOCKET s);
int (PASCAL FAR *pgethostname)(char FAR * name, int namelen);
struct hostent FAR * (PASCAL FAR *pgethostbyname)(const char FAR * name);
struct hostent FAR * (PASCAL FAR *pgethostbyaddr)(const char FAR * addr,
												  int len, int type);
int (PASCAL FAR *pgetsockname)(SOCKET s, struct sockaddr FAR *name,
							   int FAR * namelen);
#endif

#endif

