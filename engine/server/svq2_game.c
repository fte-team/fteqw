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
	char searchpath[MAX_OSPATH];
	void *iterator;
	int o;
	const char *gamename[] = {
#ifdef _DEBUG
		"debug/game" ARCH_CPU_POSTFIX ARCH_DL_POSTFIX,
#endif
#if defined(__linux__) && defined(__i386__)
		"game" "i386" ARCH_DL_POSTFIX,	//compat is often better than consistancy
#endif
		"game" ARCH_CPU_POSTFIX ARCH_DL_POSTFIX,
		"game" ARCH_DL_POSTFIX,
		NULL
	};

	void *ret;

#ifdef _DEBUG
	Con_DPrintf("Searching for %s\n", gamename[1]);
#else
	Con_DPrintf("Searching for %s\n", gamename[0]);
#endif

	iterator = NULL;
	while(COM_IteratePaths(&iterator, searchpath, sizeof(searchpath)))
	{
		for (o = 0; gamename[o]; o++)
		{
			snprintf(name, sizeof(name), "%s%s", searchpath, gamename[o]);

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
static void VARGS PFQ2_dprintf (char *fmt, ...)
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
static void VARGS PFQ2_cprintf (q2edict_t *ent, int level, char *fmt, ...)
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
static void VARGS PFQ2_centerprintf (q2edict_t *ent, char *fmt, ...)
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
static void VARGS PFQ2_error (char *fmt, ...)
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
static void VARGS PFQ2_Configstring (int i, char *val)
{
	if (i < 0 || i >= Q2MAX_CONFIGSTRINGS)
		Sys_Error ("configstring: bad index %i\n", i);

	if (!val)
		val = "";

	strcpy(sv.strings.configstring[i], val);

	if (i == Q2CS_NAME)
		Q_strncpyz(sv.mapname, val, sizeof(sv.name));

/*
	//work out range
	if (i >= Q2CS_LIGHTS && i < Q2CS_LIGHTS+Q2MAX_LIGHTSTYLES)
	{
		j = i - Q2CS_LIGHTS;
		if (j < MAX_LIGHTSTYLES)
		{
			if (sv.lightstyles[j])
				Z_Free(sv.lightstyles[j]);
			sv.lightstyles[j] = Z_Malloc(strlen(val)+1);
			strcpy(sv.lightstyles[j], val);
		}
	}
	else if (i >= Q2CS_MODELS && i < Q2CS_MODELS+Q2MAX_MODELS)
	{
		Q_strncpyS(sv.model_precache[i-Q2CS_MODELS], val, MAX_QPATH-1);
	}
	else if (i >= Q2CS_SOUNDS && i < Q2CS_SOUNDS+Q2MAX_SOUNDS)
	{
		Q_strncpyS(sv.sound_precache[i-Q2CS_SOUNDS], val, MAX_QPATH-1);
	}
	else if (i >= Q2CS_IMAGES && i < Q2CS_IMAGES+Q2MAX_IMAGES)
	{
		Q_strncpyS(sv.image_precache[i-Q2CS_IMAGES], val, MAX_QPATH-1);
	}
	else if (i == Q2CS_STATUSBAR)
	{
		if (sv.statusbar)
			Z_Free(sv.statusbar);
		sv.statusbar = Z_Malloc(strlen(val)+1);
		strcpy(sv.statusbar, val);
	}
	else if (i == Q2CS_NAME)
	{
		Q_strncpyz(sv.mapname, val, sizeof(sv.name));
	}
	else
	{
		Con_Printf("Ignoring configstring %i\n", i);
	}
*/

	if (sv.state != ss_loading)
	{	// send the update to everyone
		SZ_Clear (&sv.q2multicast);
		MSG_WriteChar (&sv.q2multicast, svcq2_configstring);
		MSG_WriteShort (&sv.q2multicast, i);
		MSG_WriteString (&sv.q2multicast, val);

		SV_Multicast (vec3_origin, MULTICAST_ALL_R);
	}
}

static int SVQ2_FindIndex (char *name, int start, int max, qboolean create)
{
	int		i;
	int stringlength = MAX_QPATH;
	char *strings = sv.strings.configstring[start];
	strings += stringlength;
	
	if (!name || !name[0])
		return 0;

	for (i=1 ; i<max && strings[0] ; i++, strings+=stringlength)
		if (!strcmp(strings, name))
			return i;

	if (!create)
		return 0;

	if (i == max)
		Sys_Error ("*Index: overflow");

	PFQ2_Configstring(start + i, name);

	return i;
}


static int VARGS SVQ2_ModelIndex (char *name)
{
	return SVQ2_FindIndex (name, Q2CS_MODELS, Q2MAX_MODELS, true);
}

static int VARGS SVQ2_SoundIndex (char *name)
{
	return SVQ2_FindIndex (name, Q2CS_SOUNDS, Q2MAX_SOUNDS, true);
}

static int VARGS SVQ2_ImageIndex (char *name)
{
	return SVQ2_FindIndex (name, Q2CS_IMAGES, Q2MAX_IMAGES, true);
}

/*
=================
PF_setmodel

Also sets mins and maxs for inline bmodels
=================
*/
static void VARGS PFQ2_setmodel (q2edict_t *ent, char *name)
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
		mod = Mod_FindName (name);
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
static void VARGS PFQ2_WriteString (char *s) {MSG_WriteString (&sv.q2multicast, s);}
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
	mask = CM_ClusterPVS (sv.world.worldmodel, cluster, NULL, 0);

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
	mask = CM_ClusterPHS (sv.world.worldmodel, cluster);

	leafnum = CM_PointLeafnum (sv.world.worldmodel, p2);
	cluster = CM_LeafCluster (sv.world.worldmodel, leafnum);
	area2 = CM_LeafArea (sv.world.worldmodel, leafnum);
	if ( mask && (!(mask[cluster>>3] & (1<<(cluster&7)) ) ) )
		return false;		// more than one bounce away
	if (!CM_AreasConnected (sv.world.worldmodel, area1, area2))
		return false;		// a door blocks hearing

	return true;
}

qboolean VARGS PFQ2_AreasConnected(int area1, int area2)
{
	//FIXME: requires q2/q3 bsp
	return CM_AreasConnected(sv.world.worldmodel, area1, area2);
}


#define	Q2SND_VOLUME		(1<<0)		// a byte
#define	Q2SND_ATTENUATION	(1<<1)		// a byte
#define	Q2SND_POS			(1<<2)		// three coordinates
#define	Q2SND_ENT			(1<<3)		// a short 0-2: channel, 3-12: entity
#define	Q2SND_OFFSET		(1<<4)		// a byte, msec offset from frame start

#define Q2DEFAULT_SOUND_PACKET_VOLUME	1.0
#define Q2DEFAULT_SOUND_PACKET_ATTENUATION 1.0



#define	Q2ATTN_NONE				0	// full volume the entire level
#define Q2ATTN_NORM				1/*
#define Q2CHAN_AUTO   0
#define Q2CHAN_WEAPON 1
#define Q2CHAN_VOICE  2
#define Q2CHAN_ITEM   3
#define Q2CHAN_BODY   4*/
#define Q2CHAN_NO_PHS_ADD		8
#define	Q2CHAN_RELIABLE			16

void VARGS SVQ2_StartSound (vec3_t origin, q2edict_t *entity, int channel,
					int soundindex, float volume,
					float attenuation, float timeofs)
{       
	int			sendchan;
    int			flags;
    int			i;
	int			ent;
	vec3_t		origin_v;
	qboolean	use_phs;

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
	}

	MSG_WriteByte (&sv.q2multicast, svcq2_sound);
	MSG_WriteByte (&sv.q2multicast, flags);
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
		MSG_WriteCoord (&sv.q2multicast, origin[0]);
		MSG_WriteCoord (&sv.q2multicast, origin[1]);
		MSG_WriteCoord (&sv.q2multicast, origin[2]);
	}

	// if the sound doesn't attenuate,send it to everyone
	// (global radio chatter, voiceovers, etc)
	if (attenuation == Q2ATTN_NONE)
		use_phs = false;

	if (channel & Q2CHAN_RELIABLE)
	{
		if (use_phs)
			SV_Multicast (origin, MULTICAST_PHS_R);
		else
			SV_Multicast (origin, MULTICAST_ALL_R);
	}
	else
	{
		if (use_phs)
			SV_Multicast (origin, MULTICAST_PHS);
		else
			SV_Multicast (origin, MULTICAST_ALL);
	}
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
//	memcpy(&ret, &tr, sizeof(q2trace_t));
	return ret;
}

static int VARGS SVQ2_PointContents (vec3_t p)
{
	q2trace_t tr = SVQ2_Trace(p, vec3_origin, vec3_origin, p, NULL, ~0);
	return tr.contents;
//	return CM_PointContents(p, 0);
}

static cvar_t *VARGS Q2Cvar_Get (char *var_name, char *value, int flags)
{
	cvar_t *var = Cvar_Get(var_name, value, flags, "Quake2 game variables");
	if (!var)
	{
		Con_Printf("Q2Cvar_Get: variable %s not creatable\n", var_name);
		return NULL;
	}
	return var;
}

cvar_t *VARGS Q2Cvar_Set (char *var_name, char *value)
{
	cvar_t *var = Cvar_FindVar(var_name);
	if (!var)
	{
		Con_Printf("Q2Cvar_Set: variable %s not found\n", var_name);
		return NULL;
	}
	return Cvar_Set(var, value);
}
cvar_t *VARGS Q2Cvar_ForceSet (char *var_name, char *value)
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

static void VARGS AddCommandString(char *command)
{
	Cbuf_AddText(command, RESTRICT_LOCAL);
}

/*
===============
SV_InitGameProgs

Init the game subsystem for a new map
===============
*/

void VARGS Q2SCR_DebugGraph(float value, int color)
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

static model_t *SVQ2_GetCModel(world_t *w, int modelindex)
{
	if ((unsigned int)modelindex < MAX_MODELS)
		return sv.models[modelindex];
	else
		return NULL;
}

void SVQ2_InitWorld(void)
{
	sv.world.Get_CModel = SVQ2_GetCModel;
}

qboolean SVQ2_InitGameProgs(void)
{
	extern cvar_t maxclients;
	volatile static game_import_t	import;	//volatile because msvc sucks
	if (COM_CheckParm("-noq2dll"))
	{
		SVQ2_ShutdownGameProgs();
		return false;
	}

	// unload anything we have now
	if (sv.world.worldmodel && (sv.world.worldmodel->fromgame == fg_quake || sv.world.worldmodel->fromgame == fg_halflife))	//we don't support q1 or hl maps yet... If ever.
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

	import.TagMalloc			= Z_TagMalloc;
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
	import.SetAreaPortalState	= CMQ2_SetAreaPortalState;
	import.AreasConnected		= PFQ2_AreasConnected;

	if (sv.world.worldmodel && (sv.world.worldmodel->fromgame == fg_quake || sv.world.worldmodel->fromgame == fg_halflife))
	{
		return false;
		/*
		import.linkentity			= SVQ2_Q1BSP_LinkEdict;
		import.unlinkentity			= SVQ2_Q1BSP_UnlinkEdict;
		import.BoxEdicts			= SVQ2_Q1BSP_AreaEdicts;
		import.trace				= SVQ2_Q1BSP_Trace;
		import.pointcontents		= SVQ2_Q1BSP_PointContents;
		import.setmodel				= PFQ2_Q1BSP_setmodel;
		import.inPVS				= PFQ2_Q1BSP_inPVS;
		import.inPHS				= PFQ2_Q1BSP_inPHS;
		import.Pmove				= Q2_Pmove;

		import.AreasConnected		= PFQ2_Q1BSP_AreasConnected;
		import.SetAreaPortalState	= CMQ2_Q1BSP_SetAreaPortalState;
	*/
	}

	ge = (game_export_t *)SVQ2_GetGameAPI ((game_import_t*)&import);

	if (!ge)
		return false;
	if (ge->apiversion != Q2GAME_API_VERSION)
	{
		Con_Printf("game is version %i, not %i", ge->apiversion, Q2GAME_API_VERSION);
		SVQ2_UnloadGame ();
		return false;
	}

	if (!deathmatch.value && !coop.value)
		maxclients.value = 1;
	if (maxclients.value > MAX_CLIENTS)
		Cvar_SetValue(&maxclients, MAX_CLIENTS);

	svq2_maxclients = maxclients.value;
	maxclients.flags |= CVAR_LATCH;
	deathmatch.flags |= CVAR_LATCH;
	coop.flags |= CVAR_LATCH;

	SVQ2_InitWorld();
	ge->Init ();
	return true;
}

#endif
