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

#include "quakedef.h"

cvar_t		baseskin = {"baseskin", "base"};
cvar_t		noskins = {"noskins", "0"};

extern cvar_t	r_fb_models;

char		allskins[128];
#define	MAX_CACHED_SKINS		128
skin_t		skins[MAX_CACHED_SKINS];
int			numskins;

/*
================
Skin_Find

  Determines the best skin for the given scoreboard
  slot, and sets scoreboard->skin

================
*/
void Skin_Find (player_info_t *sc)
{
	skin_t		*skin;
	int			i;
	char		name[128], *s, *mn;
	model_t		*model;

	mn = Info_ValueForKey (sc->userinfo, "model");
	while(s = strchr(mn, '/'))
		*mn = '\0';

	if (allskins[0])
		s = allskins;
	else
	{
		s = Info_ValueForKey (sc->userinfo, "skin");
		if (!s[0])
			s = baseskin.string;
	}

	if (*mn)
		mn = va("%s/%s", mn, s);

	if (strstr (mn, "..") || *mn == '.')
		mn = "base";

	COM_StripExtension (mn, name);

	s = strchr(name, '/');
	if (s)
	{
		*s = '\0';
		model = Mod_ForName(va("models/players/%s.mdl", name), false);
		if (model->type == mod_dummy)
			model = NULL;
		*s = '/';
	}
	else
		model = NULL;

	sc->model = model;

	for (i=0 ; i<numskins ; i++)
	{
		if (!strcmp (name, skins[i].name))
		{
			sc->skin = &skins[i];
			Skin_Cache8 (sc->skin);
			return;
		}
	}

	if (numskins == MAX_CACHED_SKINS)
	{	// ran out of spots, so flush everything
		Skin_Skins_f ();
		return;
	}

	skin = &skins[numskins];
	sc->skin = skin;
	numskins++;

	memset (skin, 0, sizeof(*skin));
	Q_strncpyz(skin->name, name, sizeof(skin->name));
}


/*
==========
Skin_Cache

Returns a pointer to the skin bitmap, or NULL to use the default
==========
*/
qbyte	*Skin_Cache8 (skin_t *skin)
{
	char	name[1024];
	qbyte	*raw;
	qbyte	*out, *pix;
	pcx_t	*pcx;
	int		x, y;
	int		dataByte;
	int		runLength;
	int fbremap[256];

	if (cls.downloadtype == dl_skin)
		return NULL;		// use base until downloaded

	if (noskins.value==1) // JACK: So NOSKINS > 1 will show skins, but
		return NULL;	  // not download new ones.

	if (skin->failedload)
		return NULL;

	out = Cache_Check (&skin->cache);
	if (out)
		return out;

#ifdef SWQUAKE
	if (qrenderer == QR_SOFTWARE && r_pixbytes == 1)	//only time FB has to exist... (gl can be disabled)
	{
		for (x = 0; x < vid.fullbright; x++)
			fbremap[x] = GetPalette(host_basepal[((x+256-vid.fullbright)*3)], host_basepal[((x+256-vid.fullbright)*3)+1], host_basepal[((x+256-vid.fullbright)*3)+2]);
	}
	else
#endif
	{
		for (x = 0; x < vid.fullbright; x++)
			fbremap[x] = x + (256-vid.fullbright);	//fullbrights don't exist, so don't loose palette info.
	}

//
// load the pic from disk
//
//	sprintf (name, "players/male/%s.pcx", skin->name);
	sprintf (name, "skins/%s.pcx", skin->name);
	raw = COM_LoadTempFile (name);
	if (!raw)
	{
		Con_Printf ("Couldn't load skin %s\n", name);
		sprintf (name, "skins/%s.pcx", baseskin.string);
		raw = COM_LoadTempFile (name);
		if (!raw)
		{
			skin->failedload = true;
			return NULL;
		}
	}

//
// parse the PCX file
//
	pcx = (pcx_t *)raw;
	raw = (qbyte *)(pcx+1);

	if (pcx->manufacturer != 0x0a
		|| pcx->version != 5
		|| pcx->encoding != 1
		|| pcx->bits_per_pixel != 8
		|| pcx->xmax >= 320
		|| pcx->ymax >= 200)
	{
		skin->failedload = true;
		Con_Printf ("Bad skin %s\n", name);
		return NULL;
	}
	skin->width = 320;
	skin->height = 200;
	skin->cachedbpp = 8;
	
	out = Cache_Alloc (&skin->cache, 320*200, skin->name);
	if (!out)
		Sys_Error ("Skin_Cache: couldn't allocate");

	pix = out;
	memset (out, 0, 320*200);

	for (y=0 ; y<pcx->ymax ; y++, pix += 320)
	{
		for (x=0 ; x<=pcx->xmax ; )
		{
			if (raw - (qbyte*)pcx > com_filesize) 
			{
				Cache_Free (&skin->cache);
				skin->failedload = true;
				Con_Printf ("Skin %s was malformed.  You should delete it.\n", name);
				return NULL;
			}
			dataByte = *raw++;

			if((dataByte & 0xC0) == 0xC0)
			{
				runLength = dataByte & 0x3F;
				if (raw - (qbyte*)pcx > com_filesize) 
				{
					Cache_Free (&skin->cache);
					skin->failedload = true;
					Con_Printf ("Skin %s was malformed.  You should delete it.\n", name);
					return NULL;
				}
				dataByte = *raw++;
			}
			else
				runLength = 1;

			// skin sanity check
			if (runLength + x > pcx->xmax + 2) {
				Cache_Free (&skin->cache);
				skin->failedload = true;
				Con_Printf ("Skin %s was malformed.  You should delete it.\n", name);
				return NULL;
			}

			if (dataByte >= 256-vid.fullbright)	//kill the fb componant
				if (!r_fb_models.value)
					dataByte = fbremap[dataByte + vid.fullbright-256];

			while(runLength-- > 0)
				pix[x++] = dataByte;
		}

	}

	if ( raw - (qbyte *)pcx > com_filesize)
	{
		Cache_Free (&skin->cache);
		skin->failedload = true;
		Con_Printf ("Skin %s was malformed.  You should delete it.\n", name);
		return NULL;
	}

	skin->failedload = false;

	return out;
}

qbyte	*Skin_Cache32 (skin_t *skin)
{
	char	name[1024];
	qbyte	*raw;
	qbyte	*out, *pix;

	if (cls.downloadtype == dl_skin)
		return NULL;		// use base until downloaded

	if (noskins.value==1) // JACK: So NOSKINS > 1 will show skins, but
		return NULL;	  // not download new ones.

	if (skin->failedload)
		return NULL;

	out = Cache_Check (&skin->cache);
	if (out)
		return out;

//
// load the pic from disk
//
	sprintf (name, "skins/%s.tga", skin->name);
	raw = COM_LoadTempFile (name);
	if (raw)
	{
		pix = ReadTargaFile(raw, com_filesize, &skin->width, &skin->height, false);
		if (pix)
		{
			out = Cache_Alloc(&skin->cache, skin->width*skin->height*4, name);
			memcpy(out, pix, skin->width*skin->height*4);
			BZ_Free(pix);
		}
	}
#ifdef AVAIL_PNGLIB
	sprintf (name, "skins/%s.png", skin->name);
	raw = COM_LoadTempFile (name);
	if (raw)
	{
		pix = ReadPNGFile(raw, com_filesize, &skin->width, &skin->height);
		if (pix)
		{
			out = Cache_Alloc(&skin->cache, skin->width*skin->height*4, name);
			memcpy(out, pix, skin->width*skin->height*4);
			BZ_Free(pix);
		}
	}
#endif
#ifdef AVAIL_JPEGLIB
	sprintf (name, "skins/%s.jpeg", skin->name);
	raw = COM_LoadTempFile (name);
	if (raw)
	{
		pix = ReadJPEGFile(raw, com_filesize, &skin->width, &skin->height);
		if (pix)
		{
			out = Cache_Alloc(&skin->cache, skin->width*skin->height*4, name);
			memcpy(out, pix, skin->width*skin->height*4);
			BZ_Free(pix);
		}
	}
	sprintf (name, "skins/%s.jpg", skin->name);	//jpegs are gready with 2 extensions...
	raw = COM_LoadTempFile (name);
	if (raw)
	{
		pix = ReadJPEGFile(raw, com_filesize, &skin->width, &skin->height);
		if (pix)
		{
			out = Cache_Alloc(&skin->cache, skin->width*skin->height*4, name);
			memcpy(out, pix, skin->width*skin->height*4);
			BZ_Free(pix);
		}
	}
#endif

	skin->failedload = true;
	return NULL;
}

/*
=================
Skin_NextDownload
=================
*/
void Skin_NextDownload (void)
{
	player_info_t	*sc;
	int			i;

	if (cls.downloadnumber == 0)
		Con_Printf ("Checking skins...\n");
	cls.downloadtype = dl_skin;

	for ( 
		; cls.downloadnumber != MAX_CLIENTS
		; cls.downloadnumber++)
	{
		sc = &cl.players[cls.downloadnumber];
		if (!sc->name[0])
			continue;
		Skin_Find (sc);
		if (noskins.value)
			continue;
		if (!CL_CheckOrDownloadFile(va("skins/%s.pcx", sc->skin->name), false))
			return;		// started a download
	}

	cls.downloadtype = dl_none;

	// now load them in for real
	for (i=0 ; i<MAX_CLIENTS ; i++)
	{
		sc = &cl.players[i];
		if (!sc->name[0])
			continue;
		Skin_Cache8 (sc->skin);
#ifdef RGLQUAKE
		sc->skin = NULL;
#endif
	}

	if (cls.state != ca_active)
	{	// get next signon phase
		MSG_WriteByte (&cls.netchan.message, clc_stringcmd);
		MSG_WriteString (&cls.netchan.message,
			va("begin %i", cl.servercount));
		Cache_Report ();		// print remaining memory
	}
}


/*
==========
Skin_Skins_f

Refind all skins, downloading if needed.
==========
*/
void	Skin_Skins_f (void)
{
	int		i;

	for (i=0 ; i<numskins ; i++)
	{
		if (skins[i].cache.data)
			Cache_Free (&skins[i].cache);
	}
	numskins = 0;

	cls.downloadnumber = 0;
	cls.downloadtype = dl_skin;
	Skin_NextDownload ();

#ifdef VM_CG
	CG_Stop();
	CG_Start();
#endif
}


/*
==========
Skin_AllSkins_f

Sets all skins to one specific one
==========
*/
void	Skin_AllSkins_f (void)
{
	strcpy (allskins, Cmd_Argv(1));
	Skin_Skins_f ();
}

void Skin_FlushSkin(char *name)
{
int i;
	char sname[16]="";
	if (strncmp(name, "skins/", 6))
		return;
	Q_strncpyz(sname, (name + 6), strlen(name+6)-3);
	for (i=0 ; i<numskins ; i++)
	{
		if (!strcmp(skins[i].name, sname))
			skins[i].failedload = false;
	}
}

