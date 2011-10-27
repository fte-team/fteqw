/*
Copyright (C) 1996-1997 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the included (GNU.txt) GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/
// Quake is a trademark of Id Software, Inc., (c) 1996 Id Software, Inc. All
// rights reserved.

#include "quakedef.h"
#include "winquake.h"

#if defined(_MSC_VER) && (_MSC_VER < 1300)
#define DWORD_PTR DWORD
#endif

extern	HWND	mainwindow;
extern	cvar_t	bgmvolume;

static qboolean cdValid = false;
static qboolean	playing = false;
static qboolean	wasPlaying = false;
static qboolean	initialized = false;
static qboolean	enabled = false;
static qboolean playLooping = false;
static qbyte 	remap[100];
static qbyte		playTrack;
static qbyte		maxTrack;

UINT	wDeviceID;


static void CDAudio_Eject(void)
{
	DWORD	dwReturn;

	dwReturn = mciSendCommand(wDeviceID, MCI_SET, MCI_SET_DOOR_OPEN, (DWORD_PTR)NULL);
    if (dwReturn)
		Con_DPrintf("MCI_SET_DOOR_OPEN failed (%i)\n", (int)dwReturn);
}


static void CDAudio_CloseDoor(void)
{
	DWORD	dwReturn;

	dwReturn = mciSendCommand(wDeviceID, MCI_SET, MCI_SET_DOOR_CLOSED, (DWORD_PTR)NULL);
    if (dwReturn)
		Con_DPrintf("MCI_SET_DOOR_CLOSED failed (%i)\n", (int)dwReturn);
}


static int CDAudio_GetAudioDiskInfo(void)
{
	DWORD				dwReturn;
	MCI_STATUS_PARMS	mciStatusParms;

	cdValid = false;
	if (!initialized)
		return -1;

	mciStatusParms.dwItem = MCI_STATUS_READY;
    dwReturn = mciSendCommand(wDeviceID, MCI_STATUS, MCI_STATUS_ITEM | MCI_WAIT, (DWORD_PTR) (LPVOID) &mciStatusParms);
	if (dwReturn)
	{
		Con_DPrintf("CDAudio: drive ready test - get status failed\n");
		return -1;
	}
	if (!mciStatusParms.dwReturn)
	{
		Con_DPrintf("CDAudio: drive not ready\n");
		return -1;
	}

	mciStatusParms.dwItem = MCI_STATUS_NUMBER_OF_TRACKS;
    dwReturn = mciSendCommand(wDeviceID, MCI_STATUS, MCI_STATUS_ITEM | MCI_WAIT, (DWORD_PTR) (LPVOID) &mciStatusParms);
	if (dwReturn)
	{
		Con_DPrintf("CDAudio: get tracks - status failed\n");
		return -1;
	}
	if (mciStatusParms.dwReturn < 1)
	{
		Con_DPrintf("CDAudio: no music tracks\n");
		return -1;
	}

	cdValid = true;
	maxTrack = mciStatusParms.dwReturn;

	return 0;
}

void CDAudio_Play(int track, qboolean looping)
{
	DWORD				dwReturn;
    MCI_PLAY_PARMS		mciPlayParms;
	MCI_STATUS_PARMS	mciStatusParms;

#ifndef NOMEDIA
	if (Media_FakeTrack(track, looping))
	{
		if (playing)
			CDAudio_Stop();
		return;
	}
#endif

	if (!enabled)
	{
		return;
	}

	if (!cdValid)
	{
		CDAudio_GetAudioDiskInfo();
		if (!cdValid)
		{
			return;
		}
	}

	if (!initialized)
		return;

	track = remap[track];

	if (track < 1 || track > maxTrack)
	{
		Con_DPrintf("CDAudio: Bad track number %u.\n", track);
		return;
	}

	// don't try to play a non-audio track
	mciStatusParms.dwItem = MCI_CDA_STATUS_TYPE_TRACK;
	mciStatusParms.dwTrack = track;
    dwReturn = mciSendCommand(wDeviceID, MCI_STATUS, MCI_STATUS_ITEM | MCI_TRACK | MCI_WAIT, (DWORD_PTR) (LPVOID) &mciStatusParms);
	if (dwReturn)
	{
		Con_DPrintf("MCI_STATUS failed (%i)\n", (int)dwReturn);
		return;
	}
	if (mciStatusParms.dwReturn != MCI_CDA_TRACK_AUDIO)
	{
		Con_Printf("CDAudio: track %i is not audio\n", track);
		return;
	}

	// get the length of the track to be played
	mciStatusParms.dwItem = MCI_STATUS_LENGTH;
	mciStatusParms.dwTrack = track;
    dwReturn = mciSendCommand(wDeviceID, MCI_STATUS, MCI_STATUS_ITEM | MCI_TRACK | MCI_WAIT, (DWORD_PTR) (LPVOID) &mciStatusParms);
	if (dwReturn)
	{
		Con_DPrintf("MCI_STATUS failed (%i)\n", (int)dwReturn);
		return;
	}

	if (playing)
	{
		if (playTrack == track)
			return;
		CDAudio_Stop();
	}

    mciPlayParms.dwFrom = MCI_MAKE_TMSF(track, 0, 0, 0);
	mciPlayParms.dwTo = (mciStatusParms.dwReturn << 8) | track;
    mciPlayParms.dwCallback = (DWORD_PTR)mainwindow;
    dwReturn = mciSendCommand(wDeviceID, MCI_PLAY, MCI_NOTIFY | MCI_FROM | MCI_TO, (DWORD_PTR)(LPVOID) &mciPlayParms);
	if (dwReturn)
	{
		Con_DPrintf("CDAudio: MCI_PLAY failed (%i)\n", (int)dwReturn);
		return;
	}

	playLooping = looping;
	playTrack = track;
	playing = true;

	if (!bgmvolume.value)
		CDAudio_Pause ();

	return;
}


void CDAudio_Stop(void)
{
	DWORD	dwReturn;

	if (!enabled)
		return;

	if (!playing)
		return;

	dwReturn = mciSendCommand(wDeviceID, MCI_STOP, 0, (DWORD_PTR)NULL);
    if (dwReturn)
		Con_DPrintf("MCI_STOP failed (%i)", (int)dwReturn);

	wasPlaying = false;
	playing = false;
}


void CDAudio_Pause(void)
{
	DWORD				dwReturn;
	MCI_GENERIC_PARMS	mciGenericParms;

	if (!enabled)
		return;

	if (!playing)
		return;

	mciGenericParms.dwCallback = (DWORD_PTR)mainwindow;
	dwReturn = mciSendCommand(wDeviceID, MCI_PAUSE, 0, (DWORD_PTR)(LPVOID) &mciGenericParms);
    if (dwReturn)
		Con_DPrintf("MCI_PAUSE failed (%i)", (int)dwReturn);

	wasPlaying = playing;
	playing = false;
}


void CDAudio_Resume(void)
{
	DWORD			dwReturn;
    MCI_PLAY_PARMS	mciPlayParms;

	if (!enabled)
		return;

	if (!cdValid)
		return;

	if (!wasPlaying)
		return;

	if (!bgmvolume.value)
		return;

    mciPlayParms.dwFrom = MCI_MAKE_TMSF(playTrack, 0, 0, 0);
    mciPlayParms.dwTo = MCI_MAKE_TMSF(playTrack + 1, 0, 0, 0);
    mciPlayParms.dwCallback = (DWORD_PTR)mainwindow;
    dwReturn = mciSendCommand(wDeviceID, MCI_PLAY, MCI_TO | MCI_NOTIFY, (DWORD_PTR)(LPVOID) &mciPlayParms);
	if (dwReturn)
	{
		Con_DPrintf("CDAudio: MCI_PLAY failed (%i)\n", (int)dwReturn);
		return;
	}
	playing = true;
}


static void CD_f (void)
{
	char	*command;
	int		ret;
	int		n;

	if (Cmd_Argc() < 2)
		return;

	command = Cmd_Argv (1);


	if (Q_strcasecmp(command, "play") == 0)
	{
		CDAudio_Play((qbyte)Q_atoi(Cmd_Argv (2)), false);
		return;
	}

	if (Q_strcasecmp(command, "loop") == 0)
	{
		CDAudio_Play((qbyte)Q_atoi(Cmd_Argv (2)), true);
		return;
	}


	if (!initialized)
	{
		Con_Printf("No cd drive detected\n");
		return;
	}

	if (Q_strcasecmp(command, "on") == 0)
	{
		enabled = true;
		return;
	}

	if (Q_strcasecmp(command, "off") == 0)
	{
		if (playing)
			CDAudio_Stop();
		enabled = false;
		return;
	}

	if (Q_strcasecmp(command, "reset") == 0)
	{
		enabled = true;
		if (playing)
			CDAudio_Stop();
		for (n = 0; n < 100; n++)
			remap[n] = n;
		CDAudio_GetAudioDiskInfo();
		return;
	}

	if (Q_strcasecmp(command, "remap") == 0)
	{
		ret = Cmd_Argc() - 2;
		if (ret <= 0)
		{
			for (n = 1; n < 100; n++)
				if (remap[n] != n)
					Con_Printf("  %u -> %u\n", n, remap[n]);
			return;
		}
		for (n = 1; n <= ret; n++)
			remap[n] = Q_atoi(Cmd_Argv (n+1));
		return;
	}

	if (Q_strcasecmp(command, "close") == 0)
	{
		CDAudio_CloseDoor();
		return;
	}

	if (!cdValid)
	{
		CDAudio_GetAudioDiskInfo();
		if (!cdValid)
		{
			Con_Printf("No CD in player.\n");
			return;
		}
	}

	if (Q_strcasecmp(command, "stop") == 0)
	{
		CDAudio_Stop();
		return;
	}

	if (Q_strcasecmp(command, "pause") == 0)
	{
		CDAudio_Pause();
		return;
	}

	if (Q_strcasecmp(command, "resume") == 0)
	{
		CDAudio_Resume();
		return;
	}

	if (Q_strcasecmp(command, "eject") == 0)
	{
		if (playing)
			CDAudio_Stop();
		CDAudio_Eject();
		cdValid = false;
		return;
	}

	if (Q_strcasecmp(command, "info") == 0)
	{
		Con_Printf("%u tracks\n", maxTrack);
		if (playing)
			Con_Printf("Currently %s track %u\n", playLooping ? "looping" : "playing", playTrack);
		else if (wasPlaying)
			Con_Printf("Paused %s track %u\n", playLooping ? "looping" : "playing", playTrack);
		return;
	}
}

LONG CDAudio_MessageHandler(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	if (lParam != wDeviceID)
		return 1;

	switch (wParam)
	{
		case MCI_NOTIFY_SUCCESSFUL:
			if (playing)
			{
				playing = false;
				if (playLooping)
					CDAudio_Play(playTrack, true);
			}
			break;

		case MCI_NOTIFY_ABORTED:
		case MCI_NOTIFY_SUPERSEDED:
			break;

		case MCI_NOTIFY_FAILURE:
			Con_DPrintf("MCI_NOTIFY_FAILURE\n");
			CDAudio_Stop ();
			cdValid = false;
			break;

		default:
			Con_DPrintf("Unexpected MM_MCINOTIFY type (%i)\n", (int)wParam);
			return 1;
	}

	return 0;
}

void BGMVolume_Callback(struct cvar_s *var, char *oldvalue)
{
	int cdvolume;

	if (!enabled)
		return;

	cdvolume = atof(oldvalue);

	if (cdvolume && !var->ival)
		CDAudio_Pause ();
	else if (!cdvolume && var->ival)
		CDAudio_Resume ();
}

void CDAudio_Update(void)
{

}


int CDAudio_Init(void)
{
	DWORD	dwReturn;
	MCI_OPEN_PARMS	mciOpenParms;
    MCI_SET_PARMS	mciSetParms;
	int				n;

	Cmd_AddCommand ("cd", CD_f);

#if		0	// QW
	if (cls.state == ca_dedicated)
		return -1;
#endif
	if (COM_CheckParm("-nocdaudio"))
		return -1;

	mciOpenParms.lpstrDeviceType = "cdaudio";
	dwReturn = mciSendCommand(0, MCI_OPEN, MCI_OPEN_TYPE | MCI_OPEN_SHAREABLE, (DWORD_PTR) (LPVOID) &mciOpenParms);
	if (dwReturn)
	{
		Con_Printf("CDAudio_Init: MCI_OPEN failed (%i)\n", (int)dwReturn);
		return -1;
	}
	wDeviceID = mciOpenParms.wDeviceID;

    // Set the time format to track/minute/second/frame (TMSF).
    mciSetParms.dwTimeFormat = MCI_FORMAT_TMSF;
	dwReturn = mciSendCommand(wDeviceID, MCI_SET, MCI_SET_TIME_FORMAT, (DWORD_PTR)(LPVOID) &mciSetParms);
    if (dwReturn)
    {
		Con_Printf("MCI_SET_TIME_FORMAT failed (%i)\n", (int)dwReturn);
        mciSendCommand(wDeviceID, MCI_CLOSE, 0, (DWORD_PTR)NULL);
		return -1;
    }

	for (n = 0; n < 100; n++)
		remap[n] = n;
	initialized = true;
	enabled = true;

	if (CDAudio_GetAudioDiskInfo())
	{
		Con_Printf("CDAudio_Init: No CD in player.\n");
		cdValid = false;
		enabled = false;
	}

	Cvar_Hook(&bgmvolume, BGMVolume_Callback);
//	Con_Printf("CD Audio Initialized\n");

	return 0;
}


void CDAudio_Shutdown(void)
{
	if (!initialized)
		return;
	CDAudio_Stop();
	if (mciSendCommand(wDeviceID, MCI_CLOSE, MCI_WAIT, (DWORD_PTR)NULL))
		Con_DPrintf("CDAudio_Shutdown: MCI_CLOSE failed\n");

	Cvar_Unhook(&bgmvolume);
}
