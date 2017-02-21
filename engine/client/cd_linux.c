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

#ifndef HAVE_CDPLAYER
	//nothing
#elif defined(__CYGWIN__)
#include "cd_null.c"
#else


#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/file.h>
#include <sys/types.h>
#include <fcntl.h>
#include <string.h>
#include <time.h>
#include <errno.h>

#include <linux/cdrom.h>

static int cdfile = -1;
static char cd_dev[64] = "/dev/cdrom";
static qboolean playing;

void CDAudio_Eject(void)
{
	if (cdfile == -1)
		return; // no cd init'd

	if ( ioctl(cdfile, CDROMEJECT) == -1 ) 
		Con_DPrintf("ioctl cdromeject failed\n");
}


void CDAudio_CloseDoor(void)
{
	if (cdfile == -1)
		return; // no cd init'd

	if ( ioctl(cdfile, CDROMCLOSETRAY) == -1 ) 
		Con_DPrintf("ioctl cdromclosetray failed\n");
}

int CDAudio_GetAudioDiskInfo(void)
{
	struct cdrom_tochdr tochdr;

	if (cdfile == -1)
		return -1;

	if ( ioctl(cdfile, CDROMREADTOCHDR, &tochdr) == -1 ) 
    {
      Con_DPrintf("ioctl cdromreadtochdr failed\n");
	  return -1;
    }

	if (tochdr.cdth_trk0 < 1)
	{
		Con_DPrintf("CDAudio: no music tracks\n");
		return -1;
	}

	return tochdr.cdth_trk1;
}


void CDAudio_Play(int track)
{
	struct cdrom_tocentry entry;
	struct cdrom_ti ti;

	if (cdfile == -1)
		return;

	// don't try to play a non-audio track
	entry.cdte_track = track;
	entry.cdte_format = CDROM_MSF;
    if ( ioctl(cdfile, CDROMREADTOCENTRY, &entry) == -1 )
	{
		Con_DPrintf("ioctl cdromreadtocentry failed\n");
		return;
	}
	if (entry.cdte_ctrl == CDROM_DATA_TRACK)
	{
		Con_Printf("CDAudio: track %i is not audio\n", track);
		return;
	}

	ti.cdti_trk0 = track;
	ti.cdti_trk1 = track;
	ti.cdti_ind0 = 1;
	ti.cdti_ind1 = 99;

	if ( ioctl(cdfile, CDROMPLAYTRKIND, &ti) == -1 ) 
    {
		Con_DPrintf("ioctl cdromplaytrkind failed\n");
		return;
    }

	if ( ioctl(cdfile, CDROMRESUME) == -1 ) 
		Con_DPrintf("ioctl cdromresume failed\n");

	playing = true;

	if (!bgmvolume.value)
		CDAudio_Pause ();
}


void CDAudio_Stop(void)
{
	if (cdfile == -1)
		return;

	if ( ioctl(cdfile, CDROMSTOP) == -1 )
		Con_DPrintf("ioctl cdromstop failed (%d)\n", errno);
}

void CDAudio_Pause(void)
{
	if (cdfile == -1)
		return;

	if ( ioctl(cdfile, CDROMPAUSE) == -1 ) 
		Con_DPrintf("ioctl cdrompause failed\n");
}


void CDAudio_Resume(void)
{
	if (cdfile == -1)
		return;
	
	if ( ioctl(cdfile, CDROMRESUME) == -1 ) 
		Con_DPrintf("ioctl cdromresume failed\n");
}

void CDAudio_Update(void)
{
	struct cdrom_subchnl subchnl;
	static time_t lastchk;

	if (playing && lastchk < time(NULL))
	{
		lastchk = time(NULL) + 2; //two seconds between checks
		subchnl.cdsc_format = CDROM_MSF;
		if (ioctl(cdfile, CDROMSUBCHNL, &subchnl) == -1 )
		{
			Con_DPrintf("ioctl cdromsubchnl failed\n");
			playing = false;
			return;
		}
		if (subchnl.cdsc_audiostatus != CDROM_AUDIO_PLAY &&
			subchnl.cdsc_audiostatus != CDROM_AUDIO_PAUSED)
		{
			playing = false;
			Media_EndedTrack();
		}
	}
}

qboolean CDAudio_Startup(void)
{
	int i;

	if (cdfile != -1)
		return true;

	if ((i = COM_CheckParm("-cddev")) != 0 && i < com_argc - 1)
	{
		Q_strncpyz(cd_dev, com_argv[i + 1], sizeof(cd_dev));
		cd_dev[sizeof(cd_dev) - 1] = 0;
	}

	if ((cdfile = open(cd_dev, O_RDONLY)) == -1)
	{
		Con_Printf("CDAudio_Init: open of \"%s\" failed (%i)\n", cd_dev, errno);
		cdfile = -1;
		return false;
	}

	return true;
}

void CDAudio_Init(void)
{
}


void CDAudio_Shutdown(void)
{
	if (cdfile == -1)
		return;
	CDAudio_Stop();
	close(cdfile);
	cdfile = -1;
}
#endif
