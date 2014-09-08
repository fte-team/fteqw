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
#include "glquake.h"

cvar_t		baseskin = SCVAR("baseskin", "");
cvar_t		noskins = SCVAR("noskins", "0");

extern cvar_t	cl_teamskin;
extern cvar_t	cl_enemyskin;

extern cvar_t	r_fb_models;

char		allskins[128];
#define	MAX_CACHED_SKINS		256	//max_clients is 255. hopefully this will not be reached, but hey.
qwskin_t		skins[MAX_CACHED_SKINS];
int			numskins;

//returns the name
char *Skin_FindName (player_info_t *sc)
{
	int tracknum;
	char *s;
	static char name[MAX_OSPATH];

	char *skinforcing_team;

	if (allskins[0])
	{
		Q_strncpyz(name, allskins, sizeof(name));
	}
	else
	{
		s = Info_ValueForKey(sc->userinfo, "skin");
		if (s && s[0])
			Q_strncpyz(name, s, sizeof(name));
		else
			Q_strncpyz(name, baseskin.string, sizeof(name));
	}

	if (cl.spectator && (tracknum = Cam_TrackNum(&cl.playerview[0])) != -1)
		skinforcing_team = cl.players[tracknum].team;
	else if (cl.spectator)
		skinforcing_team = "spec";
	else
		skinforcing_team = cl.players[cl.playerview[0].playernum].team;

	//Don't force skins in splitscreen (it's probable that the new skin would be wrong).
	//Don't force skins in TF (where skins are forced on a class basis by the mod).
	//Don't force skins on servers that have it disabled.
	if (cl.splitclients<2 && !cl.teamfortress && !(cl.fpd & FPD_NO_FORCE_SKIN))
	{
		char *skinname = NULL;
//		player_state_t *state;
		qboolean teammate;

		teammate = (cl.teamplay && !strcmp(sc->team, skinforcing_team)) ? true : false;
/*
		if (!cl.validsequence)
			goto nopowerups;

		state = cl.frames[cl.parsecount & UPDATE_MASK].playerstate + (sc - cl.players);

		if (state->messagenum != cl.parsecount)
			goto nopowerups;

		if ((state->effects & (EF_BLUE | EF_RED)) == (EF_BLUE | EF_RED))
			skinname = teammate ? cl_teambothskin.string : cl_enemybothskin.string;
		else if (state->effects & EF_BLUE)
			skinname = teammate ? cl_teamquadskin.string : cl_enemyquadskin.string;
		else if (state->effects & EF_RED)
			skinname = teammate ? cl_teampentskin.string : cl_enemypentskin.string;

	nopowerups:
*/
		if (!skinname || !skinname[0])
			skinname = teammate ? cl_teamskin.string : cl_enemyskin.string;

		//per-player skin forcing
		if (teammate && sc->colourised && *sc->colourised->skin)
			skinname = sc->colourised->skin;

		if (skinname[0] && !strchr(skinname, '/'))	// a '/' in a skin name is deemed as a model name, so we ignore it.
			Q_strncpyz(name, skinname, sizeof(name));
	}

	if (strstr(name, "..") || *name == '.')
		Q_strncpyz(name, baseskin.string, sizeof(name));

	return name;
}

qwskin_t *Skin_Lookup (char *fullname)
{
	int i;
	qwskin_t *skin;
	char cleanname[sizeof(skin->name)];
	COM_StripExtension (fullname, cleanname, sizeof(cleanname));

	for (i=0 ; i<numskins ; i++)
	{
		if (!strcmp (cleanname, skins[i].name))
		{
			skin = &skins[i];
			Skin_Cache8 (skin);
			return skin;
		}
	}

	//FIXME: this is stupid.
	if (numskins == MAX_CACHED_SKINS)
	{	// ran out of spots, so flush everything
		Skin_Skins_f ();
	}

	skin = &skins[numskins];
	numskins++;

	memset (skin, 0, sizeof(*skin));
	Q_strncpyz(skin->name, cleanname, sizeof(skin->name));
	Skin_Cache8 (skin);
	return skin;
}
/*
================
Skin_Find

  Determines the best skin for the given scoreboard
  slot, and sets scoreboard->skin

================
*/
void Skin_Find (player_info_t *sc)
{
	qwskin_t		*skin;
	int			i;
	char		name[128], *s;

	if (allskins[0])
		s = allskins;
	else
		s = Info_ValueForKey (sc->userinfo, "skin");

	sc->model = NULL;
	sc->skinid = 0;
	sc->qwskin = NULL;

	if (!*s)
		s = baseskin.string;
	if (!*s)
		return;
//		s = "default";

	s = Skin_FindName(sc);
	COM_StripExtension (s, name, sizeof(name));


	for (i=0 ; i<numskins ; i++)
	{
		if (!strcmp (name, skins[i].name))
		{
			sc->qwskin = &skins[i];
			Skin_Cache8 (sc->qwskin);
			return;
		}
	}

	if (numskins == MAX_CACHED_SKINS)
	{	// ran out of spots, so flush everything
		Skin_Skins_f ();
		return;
	}

	skin = &skins[numskins];
	sc->qwskin = skin;
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
qbyte	*Skin_Cache8 (qwskin_t *skin)
{
	char	name[1024];
	qbyte	*raw;
	qbyte	*out, *pix;
	pcx_t	*pcx;
	int		x, y, srcw, srch;
	int		dataByte;
	int		runLength;
	int fbremap[256];
	char *skinpath;

	if (noskins.value==1) // JACK: So NOSKINS > 1 will show skins, but
		return NULL;	  // not download new ones.

	if (skin->failedload)
		return NULL;

	TEXASSIGN(skin->textures.base, r_nulltex);
	TEXASSIGN(skin->textures.loweroverlay, r_nulltex);
	TEXASSIGN(skin->textures.upperoverlay, r_nulltex);

	out = skin->skindata;
	if (out)
		return out;

	// TODO: we build a fullbright remap.. can we get rid of this?
	for (x = 0; x < vid.fullbright; x++)
		fbremap[x] = x + (256-vid.fullbright);	//fullbrights don't exist, so don't loose palette info.

//
// load the pic from disk
//
	if (strchr(skin->name, ' ')) //see if it's actually three colours
	{
		qbyte bv;
		int col[3];
		char *s;

		s = COM_Parse(skin->name);
		col[0] = atof(com_token);
		s = COM_Parse(s);
		col[1] = atof(com_token);
		s = COM_Parse(s);
		col[2] = atof(com_token);

		bv = GetPaletteIndex(col[0], col[1], col[2]);

		skin->width = 320;
		skin->height = 200;

		skin->skindata = out = BZ_Malloc(320*200);

		memset (out, bv, 320*200);

		skin->failedload = false;

		return out;
	}

	skinpath = "skins";

	//favour 24bit+recoloured skins if gl_load24bit is enabled.
	Q_snprintfz (name, sizeof(name), "%s_shirt", skin->name);
	TEXASSIGN(skin->textures.upperoverlay, R_LoadReplacementTexture(name, skinpath, 0));
	Q_snprintfz (name, sizeof(name), "%s_pants", skin->name);
	TEXASSIGN(skin->textures.loweroverlay, R_LoadReplacementTexture(name, skinpath, 0));
	if (TEXVALID(skin->textures.upperoverlay) || TEXVALID(skin->textures.loweroverlay))
	{
		TEXASSIGN(skin->textures.base, R_LoadReplacementTexture(skin->name, skinpath, IF_NOALPHA));
		if (TEXVALID(skin->textures.base))
		{
			Q_snprintfz (name, sizeof(name), "%s_luma", skin->name);
			TEXASSIGN(skin->textures.fullbright, R_LoadReplacementTexture(skin->name, skinpath, IF_NOALPHA));
			Q_snprintfz (name, sizeof(name), "%s_gloss", skin->name);
			TEXASSIGN(skin->textures.specular, R_LoadReplacementTexture(skin->name, skinpath, 0));
			skin->failedload = true;
			return NULL;
		}
	}

	Q_snprintfz (name, sizeof(name), "%s/%s.pcx", skinpath, skin->name);
	raw = COM_LoadTempFile (name);
	if (!raw)
	{
		//use 24bit skins even if gl_load24bit is failed
		if (strcmp(skin->name, baseskin.string))
		{
//			if (!gl_load24bit.value)
			{
				TEXASSIGN(skin->textures.base, R_LoadHiResTexture(skin->name, skinpath, IF_NOALPHA));
				if (TEXVALID(skin->textures.base))
				{
					Q_snprintfz (name, sizeof(name), "%s_shirt", skin->name);
					TEXASSIGN(skin->textures.upperoverlay, R_LoadHiResTexture(name, skinpath, 0));
					Q_snprintfz (name, sizeof(name), "%s_pants", skin->name);
					TEXASSIGN(skin->textures.loweroverlay, R_LoadHiResTexture(name, skinpath, 0));

					if (!TEXVALID(skin->textures.upperoverlay) && !TEXVALID(skin->textures.loweroverlay))
						Con_DPrintf("skin \"%s\" has no colourmapping info\n", skin->name);

					Q_snprintfz (name, sizeof(name), "%s_luma", skin->name);
					TEXASSIGN(skin->textures.fullbright, R_LoadHiResTexture(skin->name, skinpath, IF_NOALPHA));
					Q_snprintfz (name, sizeof(name), "%s_gloss", skin->name);
					TEXASSIGN(skin->textures.specular, R_LoadHiResTexture(skin->name, skinpath, 0));

					skin->failedload = true;
					return NULL;
				}
			}

			//if its not already the base skin, try the base (and warn if anything not base couldn't load).
			Con_Printf ("Couldn't load skin %s\n", name);
			Q_snprintfz (name, sizeof(name), "skins/%s.pcx", baseskin.string);
			raw = COM_LoadTempFile (name);
		}
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

	//check format (sizes are checked later)
	if (pcx->manufacturer != 0x0a
		|| pcx->version != 5
		|| pcx->encoding != 1
		|| pcx->bits_per_pixel != 8)
	{
		skin->failedload = true;
		Con_Printf ("Bad skin %s (unsupported format)\n", name);
		return NULL;
	}

	pcx->xmax = (unsigned short)LittleShort(pcx->xmax);
	pcx->ymax = (unsigned short)LittleShort(pcx->ymax);
	pcx->xmin = (unsigned short)LittleShort(pcx->xmin);
	pcx->ymin = (unsigned short)LittleShort(pcx->ymin);

	srcw = pcx->xmax-pcx->xmin+1;
	srch = pcx->ymax-pcx->ymin+1;

	if (srcw < 1 || srch < 1 || srcw > 320 || srch > 200)
	{
		skin->failedload = true;
		Con_Printf ("Bad skin %s (unsupported size)\n", name);
		return NULL;
	}
	skin->width = srcw;
	skin->height = srch;

	skin->skindata = out = BZ_Malloc(skin->width*skin->height);
	if (!out)
		Sys_Error ("Skin_Cache: couldn't allocate");

	pix = out;
//	memset (out, 0, skin->width*skin->height);

	dataByte = 0;	//typically black (this is in case a 0*0 file is loaded... which won't happen anyway)
	for (y=0 ; y < srch ; y++, pix += skin->width)
	{
		for (x=0 ; x < srcw ; )
		{
			if (raw - (qbyte*)pcx > com_filesize) 
			{
				BZ_Free(skin->skindata);
				skin->skindata = NULL;
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
					BZ_Free(skin->skindata);
					skin->skindata = NULL;
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
				BZ_Free(skin->skindata);
				skin->skindata = NULL;
				skin->failedload = true;
				Con_Printf ("Skin %s was malformed.  You should delete it.\n", name);
				return NULL;
			}

			if (dataByte >= 256-vid.fullbright)	//kill the fb componant
				if (!r_fb_models.ival)
					dataByte = fbremap[dataByte + vid.fullbright-256];

			while(runLength-- > 0)
				pix[x++] = dataByte;
		}

		//pad the end of the scan line with the trailing pixel
		for ( ; x < skin->width ; )
			pix[x++] = dataByte;
	}
	//pad the bottom of the skin with that final pixel
	for ( ; y < skin->height; y++, pix += skin->width)
		for (x = 0; x < skin->width; )
			pix[x++] = dataByte;

	if ( raw - (qbyte *)pcx > com_filesize)
	{
		BZ_Free(skin->skindata);
		skin->skindata = NULL;
		skin->failedload = true;
		Con_Printf ("Skin %s was malformed.  You should delete it.\n", name);
		return NULL;
	}

	skin->failedload = false;

	return out;
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

	//Con_Printf ("Checking skins...\n");
	if (cls.protocol == CP_QUAKE2)
	{
		int j;
		char *slash;
		char *skinname;
		for (i = 0; i != MAX_CLIENTS; i++)
		{
			sc = &cl.players[i];
			if (!sc->name[0])
				continue;
			skinname = Info_ValueForKey(sc->userinfo, "skin");
			slash = strchr(skinname, '/');
			if (slash)
			{
				*slash = 0;
				CL_CheckOrEnqueDownloadFile(va("players/%s/tris.md2", skinname), NULL, 0);
				for (j = 0; j < MAX_MODELS; j++)
				{
					if (cl.model_name[j][0] == '#')
						CL_CheckOrEnqueDownloadFile(va("players/%s/%s", skinname, cl.model_name[j]+1), NULL, 0);
				}
				for (j = 0; j < MAX_SOUNDS; j++)
				{
					if (cl.sound_name[j][0] == '*')
						CL_CheckOrEnqueDownloadFile(va("players/%s/%s", skinname, cl.sound_name[j]+1), NULL, 0);
				}
				*slash = '/';
				CL_CheckOrEnqueDownloadFile(va("players/%s.pcx", skinname), NULL, 0);
			}
		}
		return;
	}
	for (i = 0; i != MAX_CLIENTS; i++)
	{
		sc = &cl.players[i];
		if (!sc->name[0])
			continue;
		Skin_Find (sc);
		if (noskins.ival || !sc->qwskin)
			continue;
		if (strchr(sc->qwskin->name, ' '))	//skip over skins using a space
			continue;
		if (!*sc->qwskin->name)
			continue;
		CL_CheckOrEnqueDownloadFile(va("skins/%s.pcx", sc->qwskin->name), NULL, 0);
	}

	// now load them in for real
	for (i=0 ; i<MAX_CLIENTS ; i++)
	{
		sc = &cl.players[i];
		if (!sc->name[0] || !sc->qwskin)
			continue;
		Skin_Cache8 (sc->qwskin);
		//sc->qwskin = NULL;
	}
}

//called from a few places when some skin cheat is applied.
//flushes all player skins.
void Skin_FlushPlayers(void)
{	//wipe the skin info
	int i;
	for (i = 0; i < cl.allocated_client_slots; i++)
		cl.players[i].qwskin = NULL;

	for (i = 0; i < cl.allocated_client_slots; i++)
		CL_NewTranslation(i);
}

void Skin_FlushAll(void)
{	//wipe the skin info
	int i;
	for (i=0 ; i<numskins ; i++)
	{
		if (skins[i].skindata)
			BZ_Free(skins[i].skindata);
	}
	numskins = 0;

	Skin_FlushPlayers();
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

	if (cls.state == ca_disconnected)
	{
		Con_Printf ("Can't \"%s\", not connected\n", Cmd_Argv(0));
		return;
	}

	R_GAliasFlushSkinCache(false);
	for (i=0 ; i<numskins ; i++)
	{
		if (skins[i].skindata)
			BZ_Free(skins[i].skindata);
	}
	numskins = 0;

	Skin_NextDownload ();


//	if (Cmd_FromServer())
	{
		SCR_SetLoadingStage(LS_NONE);

		CL_SendClientCommand(true, "begin %i", cl.servercount);
	}
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


