#include "quakedef.h"

#include <SDL.h>

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

static SDL_CD	*cddevice;


static void CDAudio_Eject(void)
{
	if (SDL_CDEject(cddevice))
		Con_DPrintf("SDL_CDEject failed\n");
}


static void CDAudio_CloseDoor(void)
{
	Con_Printf("SDL does not support this\n");
}


static int CDAudio_GetAudioDiskInfo(void)
{
	cdValid = false;

	switch (SDL_CDStatus(cddevice))
	{
	case CD_ERROR:
		Con_Printf("SDL_CDStatus returned error\n");
	case CD_TRAYEMPTY:
		cdValid = false;
		return 1;
	default:
		break;
	}

	cdValid = true;
	maxTrack = cddevice->numtracks;

	return 0;
}


void CDAudio_Play(int track, qboolean looping)
{
	if (!enabled)
		return;

	if (!cdValid)
	{
		CDAudio_GetAudioDiskInfo();
		if (!cdValid)
			return;
	}

	track = remap[track];

	if (track < 1 || track > maxTrack)
	{
		Con_DPrintf("CDAudio: Bad track number %u.\n", track);
		return;
	}

	if (playing)
	{
		if (playTrack == track)
			return;
		CDAudio_Stop();
	}

	if (SDL_CDPlayTracks(cddevice, track, 0, track+1, 0))
	{
		Con_Printf("CDAudio: track %i is not audio\n", track);
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
	if (!enabled)
		return;

	if (!playing)
		return;

	if (SDL_CDStop(cddevice))
		Con_DPrintf("CDAudio: SDL_CDStop failed");

	wasPlaying = false;
	playing = false;
}


void CDAudio_Pause(void)
{
	if (!enabled)
		return;

	if (!playing)
		return;

	if (SDL_CDPause(cddevice))
		Con_DPrintf("CDAudio: SDL_CDPause failed");

	wasPlaying = playing;
	playing = false;
}


void CDAudio_Resume(void)
{
	if (!enabled)
		return;

	if (!cdValid)
		return;

	if (!wasPlaying)
		return;

	if (SDL_CDResume(cddevice))
	{
		Con_DPrintf("CDAudio: SDL_CDResume failed\n");
		return;
	}
	playing = true;
}


static void CD_f (void)
{
	char	*command;
	int		ret;
	int		n;

	if (!initialized)
		return;

	if (Cmd_Argc() < 2)
		return;

	command = Cmd_Argv (1);

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

	if (Q_strcasecmp(command, "play") == 0)
	{
		CDAudio_Play((qbyte)Q_atoi(Cmd_Argv (2)), false);
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

	if (!cdValid)
	{
		CDAudio_GetAudioDiskInfo();
		if (!cdValid)
		{
			Con_Printf("No CD in player.\n");
			return;
		}
	}

	if (Q_strcasecmp(command, "loop") == 0)
	{
		CDAudio_Play((qbyte)Q_atoi(Cmd_Argv (2)), true);
		return;
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
			Con_DPrintf("Unexpected MM_MCINOTIFY type (%i)\n", wParam);
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

	if (cdvolume && !var->value)
		CDAudio_Pause ();
	else if (!cdvolume && var->value)
		CDAudio_Resume ();
}

void CDAudio_Update(void)
{
}


int CDAudio_Init(void)
{
	int	n;

#if		0	// QW
	if (cls.state == ca_dedicated)
		return -1;
#endif
	if (COM_CheckParm("-nocdaudio"))
		return -1;

	SDL_InitSubSystem(SDL_INIT_CDROM|SDL_INIT_NOPARACHUTE);

	if(!SDL_CDNumDrives())
	{
		Con_DPrintf("CDAudio_Init: No CD drives\n");
		return -1;
	}

	cddevice = SDL_CDOpen(0);
	if (!cddevice)
	{
		Con_Printf("CDAudio_Init: SDL_CDOpen failed\n");
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

	Cmd_AddCommand ("cd", CD_f);

	Cvar_Hook(&bgmvolume, BGMVolume_Callback);
//	Con_Printf("CD Audio Initialized\n");

	return 0;
}


void CDAudio_Shutdown(void)
{
	if (!initialized)
		return;
	CDAudio_Stop();

	SDL_CDClose(cddevice);
	cddevice = NULL;
	initialized = false;

	Cvar_Unhook(&bgmvolume);
}
