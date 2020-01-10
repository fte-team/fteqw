#include "quakedef.h"

#define Q2NUM_FOR_EDICT(ent) (((char *)ent - (char *)ge->edicts) / ge->edict_size)

#ifndef Q2SERVER
qboolean SVQ2_InitGameProgs(void)
{
	return false;
}
#else
game_export_t	*ge;
int svq2_maxclients;

dllhandle_t *q2gamedll;
void SVQ2_UnloadGame (void)
{
	if (q2gamedll)
		Sys_CloseLibrary(q2gamedll);
	q2gamedll = NULL;
}
void *SVQ2_GetGameAPI (void *parms)
{
	void *(VARGS *GetGameAPI)(void *);
	dllfunction_t funcs[] =
	{
		{(void**)&GetGameAPI, "GetGameAPI"},
		{NULL,NULL}
	};

	char name[MAX_OSPATH];
	char syspath[MAX_OSPATH];
	char gamepath[MAX_OSPATH];
	void *iterator;
	int o;
	const char *gamename[] = {
		"",	//binarydir/q2gameCPU_gamedir.ext
		"",	//homedir/q2gameCPU_gamedir.ext
		"",	//basedir/q2gameCPU_gamedir.ext
		"",	//binarydir/libgame_gamedir.ext
#ifdef _DEBUG
		"debug/game" ARCH_CPU_POSTFIX ARCH_DL_POSTFIX,
#endif
#if defined(__linux__) && defined(__i386__)
		"game" "i386" ARCH_DL_POSTFIX,	//compat is often better than consistancy
#endif
		"game" ARCH_CPU_POSTFIX ARCH_DL_POSTFIX,
		"game" ARCH_DL_POSTFIX,
#if defined(__linux__)	//FTE doesn't provide gamecode. Borrow someone else's. Lets just hope that its installed.
//		"/usr/lib/yamagi-quake2/%s/game.so",
#endif
		NULL
	};
	void *ret;

#ifdef _DEBUG
	Con_DPrintf("Searching for %s\n", gamename[3]);
#else
	Con_DPrintf("Searching for %s\n", gamename[2]);
#endif

	iterator = NULL;
	while(COM_IteratePaths(&iterator, syspath, sizeof(syspath), gamepath, sizeof(gamepath)))
	{
		for (o = 0; gamename[o]; o++)
		{
			if (o == 0)
			{	//nice and specific
				if (!host_parms.binarydir)
					continue;
				Q_snprintfz(name, sizeof(name), "%sq2game"ARCH_CPU_POSTFIX"_%s"ARCH_DL_POSTFIX, host_parms.binarydir, gamepath);
			}
			else if (o == 1)
			{	//nice and specific
				if (!*com_homepath)
					continue;
				Q_snprintfz(name, sizeof(name), "%sq2game"ARCH_CPU_POSTFIX"_%s"ARCH_DL_POSTFIX, com_homepath, gamepath);
			}
			else if (o == 2)
			{	//nice and specific
				if (!*com_gamepath)
					continue;
				Q_snprintfz(name, sizeof(name), "%sq2game"ARCH_CPU_POSTFIX"_%s"ARCH_DL_POSTFIX, com_gamepath, gamepath);
			}
			else if (o == 3)
			{	//because some people don't like knowing what cpu arch they're compiling for
				if (!host_parms.binarydir)
					continue;
				Q_snprintfz(name, sizeof(name), "%slibgame_%s"ARCH_DL_POSTFIX, host_parms.binarydir, gamepath);
			}
			else if (*gamename[o] == '/')
			{	//system path. o.O
				if (com_nogamedirnativecode.ival)	//just in case they match.
					continue;
				Q_snprintfz(name, sizeof(name), gamename[o], gamepath);
			}
			else
			{	//gamedir paths as specified above.
				if (com_nogamedirnativecode.ival)
					continue;
				Q_snprintfz(name, sizeof(name), "%s%s", syspath, gamename[o]);
			}

			q2gamedll = Sys_LoadLibrary(name, funcs);
			if (q2gamedll)
			{
				ret = GetGameAPI(parms);
				if (ret)
				{
					return ret;
				}

				Sys_CloseLibrary(q2gamedll);
				q2gamedll = 0;
			}
		}
	}

#ifdef _WIN64
	//if we found 32bit q2 gamecode that cannot be loaded, print out a warning about it.
	//this should make it a little obvious when people try using 64bit builds to run q2.
	if (COM_FCheckExists("gamex86.dll"))
		Con_Printf(CON_ERROR "WARNING: 32bit q2 gamecode found, but it cannot be used in a 64bit process.\nIf you wish to run this q2 mod, you will need to use a 32bit engine build.\n");
#endif

	return NULL;
}

/*
===============
PF_Unicast

Sends the contents of the mutlicast buffer to a single client
===============
*/
static void VARGS PFQ2_Unicast (q2edict_t *ent, qboolean reliable)
{
	int		p;
	client_t	*client;

	if (!ent)
		return;

	p = Q2NUM_FOR_EDICT(ent);
	if (p < 1 || p > svs.allocated_client_slots)
		return;

	client = svs.clients + (p-1);

	if (client->state < cs_connected)
		return;

	if (client->controller)
	{
		client_t	*peer;
		for (p = 0, peer = client->controller; peer; peer = peer->controlled, p++)
		{
			if (peer == client)
				break;
		}
		client = client->controller;

		//svcq2_playerinfo is not normally valid except within the svc_frame message.
		//this means its 'free' to repurpose for things like splitscreen. woot.
		MSG_WriteShort(&sv.q2multicast, 0);
		memmove(sv.q2multicast.data+2, sv.q2multicast.data, sv.q2multicast.cursize-2);
		sv.q2multicast.data[0] = svcq2_playerinfo;
		sv.q2multicast.data[1] = p;
	}

	if (reliable)
		SZ_Write (&client->netchan.message, sv.q2multicast.data, sv.q2multicast.cursize);
	else
		SZ_Write (&client->datagram, sv.q2multicast.data, sv.q2multicast.cursize);

	SZ_Clear (&sv.q2multicast);
}


/*
===============
PF_dprintf

Debug print to server console
===============
*/
static void VARGS PFQ2_dprintf (const char *fmt, ...)
{
	char		msg[1024];
	va_list		argptr;
	
	va_start (argptr,fmt);
	vsnprintf (msg, sizeof(msg), fmt, argptr);
	va_end (argptr);

	Con_Printf ("%s", msg);
}


/*
===============
PF_cprintf

Print to a single client
===============
*/
static void VARGS PFQ2_cprintf (q2edict_t *ent, int level, const char *fmt, ...)
{
	char		msg[1024];
	va_list		argptr;
	int			n=0;

	if (ent)
	{
		n = Q2NUM_FOR_EDICT(ent);
		if (n < 1 || n > svs.allocated_client_slots)
		{
			Sys_Error ("cprintf to a non-client");
			return;
		}
		
		if (svs.clients[n-1].state < cs_connected)
		{
			Sys_Error ("cprintf to a disconnected client");
			return;
		}
	}

	va_start (argptr,fmt);
	vsnprintf (msg, sizeof(msg), fmt, argptr);
	va_end (argptr);

	if (ent)
		SV_ClientPrintf (svs.clients+(n-1), level, "%s", msg);
	else
		Con_Printf ("%s", msg);
}


/*
===============
PF_centerprintf

centerprint to a single client
===============
*/
static void VARGS PFQ2_centerprintf (q2edict_t *ent, const char *fmt, ...)
{
	char		msg[1024];
	va_list		argptr;
	int			n;
	
	n = Q2NUM_FOR_EDICT(ent);
	if (n < 1 || n > svs.allocated_client_slots)
		return;	// Com_Error (ERR_DROP, "centerprintf to a non-client");

	if (svs.clients[n-1].state < cs_connected)
		return;

	va_start (argptr,fmt);
	vsprintf (msg, fmt, argptr);
	va_end (argptr);

	MSG_WriteByte (&sv.q2multicast,svcq2_centerprint);
	MSG_WriteString (&sv.q2multicast,msg);
	PFQ2_Unicast (ent, true);
}


/*
===============
PF_error

Abort the server with a game error
===============
*/
static void VARGS PFQ2_error (const char *fmt, ...)
{
	char		msg[1024];
	va_list		argptr;
	
	va_start (argptr,fmt);
	vsnprintf (msg, sizeof(msg), fmt, argptr);
	va_end (argptr);

	SV_Error("Game Error: %s", msg);
}

/*
===============
PF_Configstring

===============
*/
void VARGS PFQ2_Configstring (int i, const char *val)
{
	if (i < 0 || i >= Q2MAX_CONFIGSTRINGS)
		Sys_Error ("configstring: bad index %i\n", i);

	if (!val)
		val = "";

	Z_Free((char*)sv.strings.configstring[i]);
	sv.strings.configstring[i] = Z_StrDup(val);

	if (i == Q2CS_NAME)
		Q_strncpyz(sv.mapname, val, sizeof(sv.mapname));

	if (sv.state != ss_loading)
	{	// send the update to everyone
		SZ_Clear (&sv.q2multicast);
		MSG_WriteChar (&sv.q2multicast, svcq2_configstring);
		MSG_WriteShort (&sv.q2multicast, i);
		MSG_WriteString (&sv.q2multicast, val);

		SV_Multicast (vec3_origin, MULTICAST_ALL_R);
	}
}

static int SVQ2_FindIndex (const char *name, int start, int max, int overflowtype)
{
	int		i;
	const char *strings;
	
	if (!name || !name[0])
		return 0;

	for (i=1 ; i<max ; i++)
	{
		strings = sv.strings.configstring[start+i];
		if (!strings || !*strings)
			break;
		if (!strcmp(strings, name))
			return i;
	}

	if (i == max)
	{
		if (overflowtype)
		{
			const char **overflowstrings;
			switch(overflowtype)
			{
			case 1:
				overflowstrings = sv.strings.q2_extramodels;
				max = countof(sv.strings.q2_extramodels);
				i++;	//do not allow 255 to be allocated, ever. just live with the gap.
				start = 0x8000;
				break;
			case 2:
				overflowstrings = sv.strings.q2_extrasounds;
				max = countof(sv.strings.q2_extrasounds);
				start = 0x8000|0x4000;
				break;
			default:
				overflowstrings = NULL;	//ssh
				max = i;
				break;
			}

			for ( ; i<max ; i++)
			{
				strings = overflowstrings[i];
				if (!strings || !*strings)
				{
					overflowstrings[i] = Z_StrDup(name);
					if (sv.state != ss_loading)
					{
						SZ_Clear (&sv.q2multicast);
						MSG_WriteChar (&sv.q2multicast, svcq2_configstring);
						MSG_WriteShort (&sv.q2multicast, start+i);
						MSG_WriteString (&sv.q2multicast, name);
						SV_Multicast (vec3_origin, MULTICAST_ALL_R);
					}
					return i;
				}
				if (!strcmp(strings, name))
					return i;
			}
		}
		Sys_Error ("*Index: overflow");
	}

	PFQ2_Configstring(start + i, name);

	return i;
}


static int VARGS SVQ2_ModelIndex (const char *name)
{
	//model 255 is special for players. don't use it.
	return SVQ2_FindIndex (name, Q2CS_MODELS, Q2MAX_MODELS-1, 1);
}

static int VARGS SVQ2_SoundIndex (const char *name)
{
	return SVQ2_FindIndex (name, Q2CS_SOUNDS, Q2MAX_SOUNDS, 2);
}

static int VARGS SVQ2_ImageIndex (const char *name)
{
	return SVQ2_FindIndex (name, Q2CS_IMAGES, Q2MAX_IMAGES, 0);
}

/*
=================
PF_setmodel

Also sets mins and maxs for inline bmodels
=================
*/
static void VARGS PFQ2_setmodel (q2edict_t *ent, const char *name)
{
	int		i;
	model_t	*mod;

	if (!name)
	{
		Con_Printf (CON_ERROR "ERROR: PF_setmodel: NULL\n");
		ent->s.modelindex = 0;
		return;
	}

	i = SVQ2_ModelIndex (name);
		
//	ent->model = name;
	ent->s.modelindex = i;

// if it is an inline model, get the size information for it
	if (name[0] == '*')
	{
		mod = Mod_FindName (Mod_FixName(name, sv.modelname));
		if (mod->loadstate == MLS_LOADING)
			COM_WorkerPartialSync(mod, &mod->loadstate, MLS_LOADING);	//wait for it if needed
		VectorCopy (mod->mins, ent->mins);
		VectorCopy (mod->maxs, ent->maxs);
		WorldQ2_LinkEdict (&sv.world, ent);
	}

}

/*
static qboolean	PFQ2_Q1BSP_AreasConnected (int area1, int area2)
{
	return true;
}

static qboolean	CMQ2_Q1BSP_SetAreaPortalState (int portalnum, qboolean open)
{
	return true;
}*/

static void VARGS PFQ2_WriteChar (int c) {MSG_WriteChar (&sv.q2multicast, c & 0xff);}
static void VARGS PFQ2_WriteByte (int c) {MSG_WriteByte (&sv.q2multicast, c & 0xff);}
static void VARGS PFQ2_WriteShort (int c) {MSG_WriteShort (&sv.q2multicast, c & 0xffff);}
static void VARGS PFQ2_WriteLong (int c) {MSG_WriteLong (&sv.q2multicast, c);}
static void VARGS PFQ2_WriteFloat (float f) {MSG_WriteFloat (&sv.q2multicast, f);}
static void VARGS PFQ2_WriteString (const char *s) {MSG_WriteString (&sv.q2multicast, s);}
static void VARGS PFQ2_WriteAngle (float f) {MSG_WriteAngle (&sv.q2multicast, f);}
static void VARGS PFQ2_WritePos (vec3_t pos) {	MSG_WriteCoord (&sv.q2multicast, pos[0]);
									MSG_WriteCoord (&sv.q2multicast, pos[1]);
									MSG_WriteCoord (&sv.q2multicast, pos[2]);
								}
static void VARGS PFQ2_WriteDir (vec3_t dir)	{MSG_WriteDir (&sv.q2multicast, dir);}

/*
=================
PF_inPVS

Also checks portalareas so that doors block sight
=================
*/
static qboolean VARGS PFQ2_inPVS (vec3_t p1, vec3_t p2)
{
	int		leafnum;
	int		cluster;
	int		area1, area2;
	qbyte	*mask;

	//FIXME: requires q2/q3 bsp
	leafnum = CM_PointLeafnum (sv.world.worldmodel, p1);
	cluster = CM_LeafCluster (sv.world.worldmodel, leafnum);
	area1 = CM_LeafArea (sv.world.worldmodel, leafnum);
	mask = CM_ClusterPVS (sv.world.worldmodel, cluster, NULL, PVM_FAST);

	leafnum = CM_PointLeafnum (sv.world.worldmodel, p2);
	cluster = CM_LeafCluster (sv.world.worldmodel, leafnum);
	area2 = CM_LeafArea (sv.world.worldmodel, leafnum);
	if ( mask && (!(mask[cluster>>3] & (1<<(cluster&7)) ) ) )
		return false;
	if (!CM_AreasConnected (sv.world.worldmodel, area1, area2))
		return false;		// a door blocks sight
	return true;
}


/*
=================
PF_inPHS

Also checks portalareas so that doors block sound
=================
*/
static qboolean VARGS PFQ2_inPHS (vec3_t p1, vec3_t p2)
{
	int		leafnum;
	int		cluster;
	int		area1, area2;
	qbyte	*mask;

	//FIXME: requires q2/q3 bsp
	leafnum = CM_PointLeafnum (sv.world.worldmodel, p1);
	cluster = CM_LeafCluster (sv.world.worldmodel, leafnum);
	area1 = CM_LeafArea (sv.world.worldmodel, leafnum);
	mask = CM_ClusterPHS (sv.world.worldmodel, cluster, NULL);

	leafnum = CM_PointLeafnum (sv.world.worldmodel, p2);
	cluster = CM_LeafCluster (sv.world.worldmodel, leafnum);
	area2 = CM_LeafArea (sv.world.worldmodel, leafnum);
	if ( mask && (!(mask[cluster>>3] & (1<<(cluster&7)) ) ) )
		return false;		// more than one bounce away
	if (!CM_AreasConnected (sv.world.worldmodel, area1, area2))
		return false;		// a door blocks hearing

	return true;
}

qboolean VARGS PFQ2_AreasConnected(unsigned int area1, unsigned int area2)
{
	//FIXME: requires q2/q3 bsp
	return CM_AreasConnected(sv.world.worldmodel, area1, area2);
}




#define	Q2ATTN_NONE				0	// full volume the entire level
#define Q2ATTN_NORM				1/*
#define Q2CHAN_AUTO   0
#define Q2CHAN_WEAPON 1
#define Q2CHAN_VOICE  2
#define Q2CHAN_ITEM   3
#define Q2CHAN_BODY   4*/
#define Q2CHAN_NO_PHS_ADD		8
#define	Q2CHAN_RELIABLE			16

static void VARGS SVQ2_StartSound (vec3_t origin, q2edict_t *entity, int channel,
					int soundindex, float volume,
					float attenuation, float timeofs)
{       
	int			sendchan;
    int			flags;
    int			i;
	int			ent;
	vec3_t		origin_v;
	qboolean	use_phs;
	unsigned int needext = 0;

	if (volume < 0 || volume > 1.0)
		Sys_Error ("SV_StartSound: volume = %f", volume);

	if (attenuation < 0 || attenuation > 4)
		Sys_Error ("SV_StartSound: attenuation = %f", attenuation);

//	if (channel < 0 || channel > 15)
//		Sys_Error ("SV_StartSound: channel = %i", channel);

	if (timeofs < 0 || timeofs > 0.255)
		Sys_Error ("SV_StartSound: timeofs = %f", timeofs);

	ent = Q2NUM_FOR_EDICT(entity);

	if (channel & Q2CHAN_NO_PHS_ADD)	// no PHS flag
		use_phs = false;
	else
		use_phs = true;

	sendchan = (ent<<3) | (channel&7);

	flags = 0;
	if (volume != Q2DEFAULT_SOUND_PACKET_VOLUME)
		flags |= Q2SND_VOLUME;
	if (attenuation != Q2DEFAULT_SOUND_PACKET_ATTENUATION)
		flags |= Q2SND_ATTENUATION;
	if (soundindex > 0xff)
	{
		flags |= Q2SND_LARGEIDX;
		needext |= PEXT_SOUNDDBL;
	}

	// the client doesn't know that bmodels have weird origins
	// the origin can also be explicitly set
	if ( (entity->svflags & SVF_NOCLIENT)
		|| (entity->solid == Q2SOLID_BSP) 
		|| origin )
		flags |= Q2SND_POS;

	// always send the entity number for channel overrides
	flags |= Q2SND_ENT;

	if (timeofs)
		flags |= Q2SND_OFFSET;

	// use the entity origin unless it is a bmodel or explicitly specified
	if (!origin)
	{
		origin = origin_v;
		if (entity->solid == Q2SOLID_BSP)
		{
			for (i=0 ; i<3 ; i++)
				origin_v[i] = entity->s.origin[i]+0.5*(entity->mins[i]+entity->maxs[i]);
		}
		else
		{
			VectorCopy (entity->s.origin, origin_v);
		}

		if (flags & Q2SND_POS)
		{
			for (i=0 ; i<3 ; i++)
				if (-32768/8.0 > origin_v[i] || origin_v[i] > 32767/8.0)
				{
					flags |= Q2SND_LARGEPOS;
					needext |= PEXT_FLOATCOORDS;
					break;
				}
		}
	}

	MSG_WriteByte (&sv.q2multicast, svcq2_sound);
	MSG_WriteByte (&sv.q2multicast, flags);
	if (flags & Q2SND_LARGEIDX)
		MSG_WriteShort (&sv.q2multicast, soundindex);
	else
		MSG_WriteByte (&sv.q2multicast, soundindex);

	if (flags & Q2SND_VOLUME)
		MSG_WriteByte (&sv.q2multicast, volume*255);
	if (flags & Q2SND_ATTENUATION)
		MSG_WriteByte (&sv.q2multicast, attenuation*64);
	if (flags & Q2SND_OFFSET)
		MSG_WriteByte (&sv.q2multicast, timeofs*1000);

	if (flags & Q2SND_ENT)
		MSG_WriteShort (&sv.q2multicast, sendchan);

	if (flags & Q2SND_POS)
	{
		if (flags & Q2SND_LARGEPOS)
		{
			MSG_WriteFloat (&sv.q2multicast, origin[0]);
			MSG_WriteFloat (&sv.q2multicast, origin[1]);
			MSG_WriteFloat (&sv.q2multicast, origin[2]);
		}
		else
		{
			MSG_WriteCoord (&sv.q2multicast, origin[0]);
			MSG_WriteCoord (&sv.q2multicast, origin[1]);
			MSG_WriteCoord (&sv.q2multicast, origin[2]);
		}
	}

	// if the sound doesn't attenuate,send it to everyone
	// (global radio chatter, voiceovers, etc)
	if (attenuation == Q2ATTN_NONE)
		use_phs = false;

	if (channel & Q2CHAN_RELIABLE)
		SV_MulticastProtExt(origin, use_phs?MULTICAST_PHS_R:MULTICAST_ALL_R, FULLDIMENSIONMASK, needext, 0);
	else
		SV_MulticastProtExt(origin, use_phs?MULTICAST_PHS:MULTICAST_ALL, FULLDIMENSIONMASK, needext, 0);
}  

static void VARGS PFQ2_StartSound (q2edict_t *entity, int channel, int sound_num, float volume,
    float attenuation, float timeofs)
{
	if (!entity)
		return;
	SVQ2_StartSound (NULL, entity, channel, sound_num, volume, attenuation, timeofs);
}

static q2trace_t VARGS SVQ2_Trace (vec3_t start, vec3_t mins, vec3_t maxs, vec3_t end, q2edict_t *passedict, int contentmask)
{
	q2trace_t ret;
	trace_t tr;
	if (!mins)
		mins = vec3_origin;
	if (!maxs)
		maxs = vec3_origin;
	tr = WorldQ2_Move(&sv.world, start, mins, maxs, end, contentmask, passedict);
	ret.allsolid = tr.allsolid;
	ret.startsolid = tr.startsolid;
	ret.contents = tr.contents;
	ret.surface = tr.surface;
	ret.fraction = tr.fraction;
	VectorCopy(tr.endpos, ret.endpos);
	memset(&ret.plane, 0, sizeof(ret.plane));
	VectorCopy(tr.plane.normal, ret.plane.normal);
	ret.plane.dist = tr.plane.dist;
	ret.ent = tr.ent;
	return ret;
}

static int VARGS SVQ2_PointContents (vec3_t p)
{
	q2trace_t tr = SVQ2_Trace(p, vec3_origin, vec3_origin, p, NULL, ~0);
	return tr.contents;
//	return CM_PointContents(p, 0);
}

static cvar_t *VARGS Q2Cvar_Get (const char *var_name, const char *value, int flags)
{
	cvar_t *var;
	//q2 gamecode knows about these flags. anything else is probably a bug, or 3rd-party extension.
	flags &= (CVAR_NOSET|CVAR_SERVERINFO|CVAR_USERINFO|CVAR_ARCHIVE|CVAR_LATCH);

	if (!strcmp(var_name, "gamedir"))
		var_name = "fs_gamedir";

	var = Cvar_Get(var_name, value, flags, "Quake2 game variables");
	if (!var)
	{
		Con_Printf("Q2Cvar_Get: variable %s not creatable\n", var_name);
		return NULL;
	}

	//allow this to change all < cvar_latch values.
	//this allows q2 dlls to apply different flags to a cvar without destroying our important ones (like cheat).
	flags |= var->flags;
	if (flags != var->flags)
	{
		var->flags = flags;
		Cvar_Set(var, var->string);
	}
	return var;
}

cvar_t *VARGS Q2Cvar_Set (const char *var_name, const char *value)
{
	cvar_t *var = Cvar_FindVar(var_name);
	if (!var)
	{
		Con_Printf("Q2Cvar_Set: variable %s not found\n", var_name);
		return NULL;
	}
	return Cvar_Set(var, value);
}
cvar_t *VARGS Q2Cvar_ForceSet (const char *var_name, const char *value)
{
	cvar_t *var = Cvar_FindVar(var_name);
	if (!var)
	{
		Con_Printf("Q2Cvar_Set: variable %s not found\n", var_name);
		return NULL;
	}
	return Cvar_ForceSet(var, value);
}

//==============================================

/*
===============
SV_ShutdownGameProgs

Called when either the entire server is being killed, or
it is changing to a different game directory.
===============
*/
void VARGS SVQ2_ShutdownGameProgs (void)
{
	if (!ge)
		return;
	ge->Shutdown ();
	SVQ2_UnloadGame ();
	ge = NULL;
}

static void VARGS AddCommandString(const char *command)
{
	Cbuf_AddText(command, RESTRICT_LOCAL);
}

/*
===============
SV_InitGameProgs

Init the game subsystem for a new map
===============
*/

static void VARGS Q2SCR_DebugGraph(float value, int color)
{return;}

static void	VARGS SVQ2_LinkEdict (q2edict_t *ent)
{
	WorldQ2_LinkEdict(&sv.world, ent);
}
static void	VARGS SVQ2_UnlinkEdict (q2edict_t *ent)
{
	WorldQ2_UnlinkEdict(&sv.world, ent);
}
static int	VARGS SVQ2_AreaEdicts (vec3_t mins, vec3_t maxs, q2edict_t **list,	int maxcount, int areatype)
{
	return WorldQ2_AreaEdicts(&sv.world, mins, maxs, list, maxcount, areatype);
}

static model_t *QDECL SVQ2_GetCModel(world_t *w, int modelindex)
{
	if ((unsigned int)modelindex < MAX_PRECACHE_MODELS)
		return sv.models[modelindex];
	else
		return NULL;
}

void SVQ2_InitWorld(void)
{
	World_ClearWorld_Nodes (&sv.world, false);
	sv.world.Get_CModel = SVQ2_GetCModel;
}

static void QDECL PFQ2_SetAreaPortalState(unsigned int p, qboolean s)
{
	CMQ2_SetAreaPortalState(sv.world.worldmodel, p, s);
}

static void *VARGS ZQ2_TagMalloc(int size, int tag)
{
	return Z_TagMalloc(size, tag);
}
qboolean SVQ2_InitGameProgs(void)
{
	extern cvar_t maxclients;
	static volatile game_import_t	import;	//volatile because msvc sucks
	if (COM_CheckParm("-noq2dll"))
	{
		SVQ2_ShutdownGameProgs();
		return false;
	}

	if (ge)
	{
		SVQ2_InitWorld();
		return true;
	}

	// calc the imports. 
	import.multicast			= SV_Multicast;
	import.unicast				= PFQ2_Unicast;
	import.bprintf				= SV_BroadcastPrintf;
	import.dprintf				= PFQ2_dprintf;
	import.cprintf				= PFQ2_cprintf;
	import.centerprintf			= PFQ2_centerprintf;
	import.error				= PFQ2_error;

	import.linkentity			= SVQ2_LinkEdict;
	import.unlinkentity			= SVQ2_UnlinkEdict;
	import.BoxEdicts			= SVQ2_AreaEdicts;
	import.trace				= SVQ2_Trace;
	import.pointcontents		= SVQ2_PointContents;
	import.setmodel				= PFQ2_setmodel;
	import.inPVS				= PFQ2_inPVS;
	import.inPHS				= PFQ2_inPHS;
	import.Pmove				= Q2_Pmove;

	import.modelindex			= SVQ2_ModelIndex;
	import.soundindex			= SVQ2_SoundIndex;
	import.imageindex			= SVQ2_ImageIndex;

	import.configstring			= PFQ2_Configstring;
	import.sound				= PFQ2_StartSound;
	import.positioned_sound		= SVQ2_StartSound;

	import.WriteChar			= PFQ2_WriteChar;
	import.WriteByte			= PFQ2_WriteByte;
	import.WriteShort			= PFQ2_WriteShort;
	import.WriteLong			= PFQ2_WriteLong;
	import.WriteFloat			= PFQ2_WriteFloat;
	import.WriteString			= PFQ2_WriteString;
	import.WritePosition		= PFQ2_WritePos;
	import.WriteDir				= PFQ2_WriteDir;
	import.WriteAngle			= PFQ2_WriteAngle;

	import.TagMalloc			= ZQ2_TagMalloc;
	import.TagFree				= Z_TagFree;
	import.FreeTags				= Z_FreeTags;

	import.cvar					= Q2Cvar_Get;
	import.cvar_set				= Q2Cvar_Set;
	import.cvar_forceset		= Q2Cvar_ForceSet;

	import.argc					= Cmd_Argc;
	import.argv					= Cmd_Argv;
	import.args					= Cmd_Args;
	import.AddCommandString		= AddCommandString;

	import.DebugGraph			= Q2SCR_DebugGraph;
	import.SetAreaPortalState	= PFQ2_SetAreaPortalState;
	import.AreasConnected		= PFQ2_AreasConnected;

	ge = (game_export_t *)SVQ2_GetGameAPI ((game_import_t*)&import);

	if (!ge)
		return false;
	if (ge->apiversion != Q2GAME_API_VERSION)
	{
		Con_Printf("game is version %i, not %i", ge->apiversion, Q2GAME_API_VERSION);
		SVQ2_UnloadGame ();
		return false;
	}

	//Q2 gamecode depends upon maxclients being set+locked in order to know how many player slots there actually are. It crashes when its wrong.
	if (!deathmatch.value && !coop.value)
		svq2_maxclients = 1;
	else
		svq2_maxclients = maxclients.ival;
	if (Cvar_VariableValue("cl_splitscreen"))
		svq2_maxclients = max(svq2_maxclients, MAX_SPLITS);
	if (svq2_maxclients > MAX_CLIENTS)
		svq2_maxclients = MAX_CLIENTS;
	if (svq2_maxclients != maxclients.value)
		Cvar_SetValue(&maxclients, svq2_maxclients);

	maxclients.flags |= CVAR_LATCH;
	deathmatch.flags |= CVAR_LATCH;
	coop.flags |= CVAR_LATCH;

	SVQ2_InitWorld();
	ge->Init ();
	return true;
}

#endif
