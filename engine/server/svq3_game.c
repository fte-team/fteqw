#include "quakedef.h"

//An implementation of a Q3 server...
//requires qvm implementation and existing q3 client stuff (or at least the overlapping stuff in q3common.c).

#ifdef Q3SERVER

float RadiusFromBounds (vec3_t mins, vec3_t maxs);


//#define USEBOTLIB

#ifdef USEBOTLIB



#ifdef _WIN32
#define QDECL __cdecl
#else
#define QDECL
#endif
#define fileHandle_t int
#define fsMode_t int
#define pc_token_t void
#include "botlib.h"

#define Z_TAG_BOTLIB 221726

#ifdef _WIN32
	#if 0
		#pragma comment (lib, "botlib.lib")
		#define FTE_GetBotLibAPI GetBotLibAPI
	#else
		#ifndef WIN32_LEAN_AND_MEAN
		#define WIN32_LEAN_AND_MEAN
		#endif

		#include <windows.h>
		botlib_export_t *FTE_GetBotLibAPI( int apiVersion, botlib_import_t *import )
		{
			botlib_export_t *(QDECL *pGetBotLibAPI)( int apiVersion, botlib_import_t *import );

			static HINSTANCE hmod;
			if (!hmod)
				hmod = LoadLibrary("botlib.dll");
			if (!hmod)
				return NULL;

			pGetBotLibAPI = (void*)GetProcAddress(hmod, "GetBotLibAPI");

			if (!pGetBotLibAPI)
				return NULL;
			return pGetBotLibAPI(apiVersion, import);
		}
	#endif

#elif defined(__linux__)

	#include "dlfcn.h"
	botlib_export_t *FTE_GetBotLibAPI( int apiVersion, botlib_import_t *import )
	{
		botlib_export_t *(*QDECL pGetBotLibAPI)( int apiVersion, botlib_import_t *import );
		void *handle;
		handle = dlopen("botlib.so", RTLD_LAZY);
		if (!handle)
			return NULL;

		pGetBotLibAPI = dlsym(handle, "GetBotLibAPI");
		if (!pGetBotLibAPI)
			return NULL;
		return pGetBotLibAPI(apiVersion, import);
	}
#else
	botlib_export_t *FTE_GetBotLibAPI(int apiVersion, botlib_import_t *import)
	{	//a stub that will prevent botlib from loading.
		return NULL;
	}
#endif

botlib_export_t *botlib;




int COM_Compress( char *data_p ) {
	char *in, *out;
	int c;
	qboolean newline = false, whitespace = false;

	in = out = data_p;
	if (in) {
		while ((c = *in) != 0) {
			// skip double slash comments
			if ( c == '/' && in[1] == '/' ) {
				while (*in && *in != '\n') {
					in++;
				}
			// skip /* */ comments
			} else if ( c == '/' && in[1] == '*' ) {
				while ( *in && ( *in != '*' || in[1] != '/' ) ) 
					in++;
				if ( *in ) 
					in += 2;
                        // record when we hit a newline
                        } else if ( c == '\n' || c == '\r' ) {
                            newline = true;
                            in++;
                        // record when we hit whitespace
                        } else if ( c == ' ' || c == '\t') {
                            whitespace = true;
                            in++;
                        // an actual token
			} else {
                            // if we have a pending newline, emit it (and it counts as whitespace)
                            if (newline) {
                                *out++ = '\n';
                                newline = false;
                                whitespace = false;
                            } if (whitespace) {
                                *out++ = ' ';
                                whitespace = false;
                            }
                            
                            // copy quoted strings unmolested
                            if (c == '"') {
                                    *out++ = c;
                                    in++;
                                    while (1) {
                                        c = *in;
                                        if (c && c != '"') {
                                            *out++ = c;
                                            in++;
                                        } else {
                                            break;
                                        }
                                    }
                                    if (c == '"') {
                                        *out++ = c;
                                        in++;
                                    }
                            } else {
                                *out = c;
                                out++;
                                in++;
                            }
			}
		}
	}
	*out = 0;
	return out - data_p;
}

void Com_Memset (void* dest, const int val, const size_t count)
{
	memset(dest, val, count);
}
void Com_Memcpy (void* dest, const void* src, const size_t count)
{
	memcpy(dest, src, count);
}
int Q_stricmp(char *a, char *b)
{
	return stricmp(a, b);
}
#if _MSC_VER < 700
int _ftol2 (float f)
{
	return (int)f;
}
#endif
void QDECL Com_Error( int level, const char *error, ... )
{
	va_list		argptr;
	char		text[1024];

	va_start (argptr, error);
	vsprintf (text, error, argptr);
	va_end (argptr);
	Sys_Error("%s", text);
}
void QDECL Com_Printf( const char *error, ... )
{
	va_list		argptr;
	char		text[1024];

	va_start (argptr, error);
	vsprintf (text, error, argptr);
	va_end (argptr);

	Con_Printf("%s", text);
}

void QDECL Com_sprintf( char *dest, int size, const char *fmt, ...)
{
	int		len;
	va_list		argptr;
	char	bigbuffer[32000];	// big, but small enough to fit in PPC stack

	va_start (argptr,fmt);
	len = vsprintf (bigbuffer,fmt,argptr);
	va_end (argptr);
	if ( len >= sizeof( bigbuffer ) ) {
		Com_Error( 0, "Com_sprintf: overflowed bigbuffer" );
	}
	if (len >= size) {
		Com_Printf ("Com_sprintf: overflow of %i in %i\n", len, size);
#ifdef	_DEBUG
		__asm {
			int 3;
		}
#endif
	}
	Q_strncpyz (dest, bigbuffer, size );
}
#endif



#include "clq3defs.h"
#include "q3g_public.h"

static vm_t *q3gamevm;


#define fs_key 0

#define MAX_CONFIGSTRINGS 1024
char *svq3_configstrings[MAX_CONFIGSTRINGS];

q3sharedEntity_t *q3_entarray;
int	numq3entities;
int sizeofq3gentity;
q3playerState_t *q3playerstates;
int sizeofGameClient;

int q3_num_snapshot_entities;
int q3_next_snapshot_entities;
q3entityState_t *q3_snapshot_entities;
q3entityState_t *q3_baselines;

#define NUM_FOR_GENTITY(ge) (((char*)ge - (char*)q3_entarray) / sizeofq3gentity)
#define NUM_FOR_SENTITY(se) (se - q3_sentities)
#define GENTITY_FOR_NUM(num) ((q3sharedEntity_t*)((char *)q3_entarray + sizeofq3gentity*(num)))
#define SENTITY_FOR_NUM(num) ((q3serverEntity_t*)((char *)q3_sentities + sizeof(q3serverEntity_t)*(num)))
#define SENTITY_FOR_GENTITY(ge) (SENTITY_FOR_NUM(NUM_FOR_GENTITY(ge)))
#define GENTITY_FOR_SENTITY(se) (GENTITY_FOR_NUM(NUM_FOR_SENTITY(se)))

static qboolean BoundsIntersect (vec3_t mins1, vec3_t maxs1, vec3_t mins2, vec3_t maxs2);

void SVQ3_CreateBaseline(void);
void SVQ3_ClientThink(client_t *cl);

char *mapentspointer;

#define	Q3SOLID_BMODEL	0xffffff

#define	Q3CONTENTS_SOLID		Q2CONTENTS_SOLID	// should never be on a brush, only in game
#define	Q3CONTENTS_BODY			0x2000000	// should never be on a brush, only in game

#define PS_FOR_NUM(n) ((q3playerState_t *)((qbyte *)q3playerstates + sizeofGameClient*(n)))

#define clamp(v,min,max) v = (v>max)?max:((v < min)?min:v)
#define Q_rint(x) (int)((x > 0)?(x + 0.5f):(x-0.5f))



// entity->svFlags
// the server does not know how to interpret most of the values
// in entityStates (level eType), so the game must explicitly flag
// special server behaviors
#define	SVF_NOCLIENT			0x00000001	// don't send entity to clients, even if it has effects

// TTimo
// https://zerowing.idsoftware.com/bugzilla/show_bug.cgi?id=551
#define SVF_CLIENTMASK 0x00000002

#define SVF_BOT					0x00000008	// set if the entity is a bot
#define	SVF_BROADCAST			0x00000020	// send to all connected clients
#define	SVF_PORTAL				0x00000040	// merge a second pvs at origin2 into snapshots
#define	SVF_USE_CURRENT_ORIGIN	0x00000080	// entity->r.currentOrigin instead of entity->s.origin
											// for link position (missiles and movers)
#define SVF_SINGLECLIENT		0x00000100	// only send to a single client (entityShared_t->singleClient)
#define SVF_NOSERVERINFO		0x00000200	// don't send CS_SERVERINFO updates to this client
											// so that it can be updated for ping tools without
											// lagging clients
#define SVF_CAPSULE				0x00000400	// use capsule for collision detection instead of bbox
#define SVF_NOTSINGLECLIENT		0x00000800	// send entity to everyone but one client
											// (entityShared_t->singleClient)





typedef struct {
	link_t area;
	qboolean linked;
	int areanum;
	int areanum2;
	int headnode;
	int num_clusters;
	int clusternums[MAX_ENT_CLUSTERS];
} q3serverEntity_t;
q3serverEntity_t *q3_sentities;

void Q3G_UnlinkEntity(q3sharedEntity_t *ent)
{
	q3serverEntity_t *sent;

	ent->r.linked = false;

	sent = SENTITY_FOR_GENTITY(ent);

	if(!sent->linked)
	{
		return;		// not linked in anywhere
	}

	if (sent->area.prev == NULL || sent->area.next == NULL)
		SV_Error("Null entity links in linked entity\n");

	RemoveLink(&sent->area);
	sent->area.prev = sent->area.next = NULL;

	sent->linked = false;
}

#define MAX_TOTAL_ENT_LEAFS		256


void Q3G_LinkEntity(q3sharedEntity_t *ent)
{
	areanode_t	*node;
	q3serverEntity_t	*sent;
	int			leafs[MAX_TOTAL_ENT_LEAFS];
	int			clusters[MAX_TOTAL_ENT_LEAFS];
	int			num_leafs;
	int			i, j, k;
	int			area;
	int			topnode;
	const float		*origin;
	const float		*angles;

	sent = SENTITY_FOR_GENTITY(ent);

	if(sent->linked)
		Q3G_UnlinkEntity(ent);	// unlink from old position

	// encode the size into the entity_state for client prediction
	if(ent->r.bmodel)
		ent->s.solid = Q3SOLID_BMODEL;
	else if(ent->r.contents & (Q3CONTENTS_BODY|Q3CONTENTS_SOLID))
	{
		// assume that x/y are equal and symetric
		i = ent->r.maxs[0];
		clamp(i, 1, 255);

		// z is not symetric
		j = -ent->r.mins[2];
		clamp(j, 1, 255);

		// and z maxs can be negative...
		k = ent->r.maxs[2]+32;
		clamp(k, 1, 255);

		ent->s.solid = (((k << 8) | j) << 8) | i;
	}
	else
		ent->s.solid = 0;

	// always use currentOrigin
	origin = ent->r.currentOrigin;
	angles = ent->r.currentAngles;

	// set the abs box
	if(ent->r.bmodel && (angles[0] || angles[1] || angles[2]))
	{
		// expand for rotation
		float		max, v;
		int			i;

		max = 0;
		for(i=0; i<3; i++)
		{
			v = fabs(ent->r.mins[i]);
			if(v > max)
				max = v;
			v = fabs(ent->r.maxs[i]);
			if(v > max)
				max = v;
		}
		for(i=0; i<3; i++)
		{
			ent->r.absmin[i] = origin[i] - max;
			ent->r.absmax[i] = origin[i] + max;
		}
	}
	else
	{
		// normal
		VectorAdd(origin, ent->r.mins, ent->r.absmin);	
		VectorAdd(origin, ent->r.maxs, ent->r.absmax);
	}

	// because movement is clipped an epsilon away from an actual edge,
	// we must fully check even when bounding boxes don't quite touch
	ent->r.absmin[0] -= 1;
	ent->r.absmin[1] -= 1;
	ent->r.absmin[2] -= 1;
	ent->r.absmax[0] += 1;
	ent->r.absmax[1] += 1;
	ent->r.absmax[2] += 1;

// link to PVS leafs
	sent->num_clusters = 0;
	sent->areanum = -1;
	sent->areanum2 = -1;

	//get all leafs, including solids
	if (sv.worldmodel->type == mod_heightmap)
	{
		sent->areanum = 0;
		num_leafs = 1;
		sent->num_clusters = -1;
		sent->headnode = 0;
		clusters[0] = 0;
		topnode = 0;
	}
	else
	{
		num_leafs = CM_BoxLeafnums(sv.worldmodel, ent->r.absmin, ent->r.absmax,
			leafs, MAX_TOTAL_ENT_LEAFS, &topnode);

		if(!num_leafs)
			return;

		// set areas
		for(i=0; i<num_leafs; i++)
		{
			clusters[i] = CM_LeafCluster(sv.worldmodel, leafs[i]);
			area = CM_LeafArea(sv.worldmodel, leafs[i]);
			if(area >= 0)
			{
				// doors may legally straggle two areas,
				// but nothing should ever need more than that
				if(sent->areanum >= 0 && sent->areanum != area)
				{
					if(sent->areanum2 >= 0 && sent->areanum2 != area && sv.state == ss_loading)
						Con_DPrintf("Object touching 3 areas at %f %f %f\n", ent->r.absmin[0], ent->r.absmin[1], ent->r.absmin[2]);

					sent->areanum2 = area;
				}
				else
					sent->areanum = area;
			}
		}
	}

	if(num_leafs >= MAX_TOTAL_ENT_LEAFS)
	{
		// assume we missed some leafs, and mark by headnode
		sent->num_clusters = -1;
		sent->headnode = topnode;
	}
	else
	{
		sent->num_clusters = 0;
		for(i=0; i<num_leafs; i++)
		{
			if(clusters[i] == -1)
				continue;		// not a visible leaf

			for(j=0 ; j<i ; j++)
			{
				if(clusters[j] == clusters[i])
					break;
			}
			if(j == i)
			{
				if(sent->num_clusters == MAX_ENT_CLUSTERS)
				{
					// assume we missed some leafs, and mark by headnode
					sent->num_clusters = -1;
					sent->headnode = topnode;
					break;
				}

				sent->clusternums[sent->num_clusters++] = clusters[i];
			}
		}
	}

	ent->r.linkcount++;
	ent->r.linked = true;
	sent->linked = true;

// find the first node that the ent's box crosses
	node = sv_areanodes;
	while(1)
	{
		if(node->axis == -1)
			break;

		if(ent->r.absmin[node->axis] > node->dist)
			node = node->children[0];
		else if(ent->r.absmax[node->axis] < node->dist)
			node = node->children[1];
		else
			break;		// crosses the node
	}
	// link it in	
	InsertLinkBefore((link_t *)&sent->area, &node->solid_edicts);
}

int SVQ3_EntitiesInBoxNode(areanode_t *node, vec3_t mins, vec3_t maxs, int *list, int maxcount)
{
	link_t		*l, *next;
	q3serverEntity_t		*sent;
	q3sharedEntity_t		*gent;

	int linkcount = 0;

	//work out who they are first.
	for (l = node->solid_edicts.next ; l != &node->solid_edicts ; l = next)
	{
		if (maxcount == linkcount)
			return linkcount;

		next = l->next;
		sent = Q3EDICT_FROM_AREA(l);
		gent = GENTITY_FOR_SENTITY(sent);

		if (!BoundsIntersect(mins, maxs, gent->r.absmin, gent->r.absmax))
			continue;

		list[linkcount++] = NUM_FOR_GENTITY(gent);
	}

	if (node->axis >= 0)
	{
	
		if ( maxs[node->axis] > node->dist )
			linkcount += SVQ3_EntitiesInBoxNode(node->children[0], mins, maxs, list+linkcount, maxcount-linkcount);
		if ( mins[node->axis] < node->dist )
			linkcount += SVQ3_EntitiesInBoxNode(node->children[1], mins, maxs, list+linkcount, maxcount-linkcount);
	}

	return linkcount;
}

int SVQ3_EntitiesInBox(vec3_t mins, vec3_t maxs, int *list, int maxcount)
{
	if (maxcount < 0)
		return 0;
	return SVQ3_EntitiesInBoxNode(sv_areanodes, mins, maxs, list, maxcount);
}

model_t *SVQ3_ModelForEntity(q3sharedEntity_t *es)
{
	if (es->r.bmodel)
	{
		return Mod_ForName(va("*%i", es->s.modelindex), false);
	}
	else
	{
		return CM_TempBoxModel(es->r.mins, es->r.maxs);
	}
}

#define	ENTITYNUM_WORLD		(MAX_GENTITIES-2)
void SVQ3_Trace(q3trace_t *result, vec3_t start, vec3_t mins, vec3_t maxs, vec3_t end, int entnum, int contentmask)
{
	int contactlist[128];
	trace_t tr;
	vec3_t mmins, mmaxs;
	int i;
	q3sharedEntity_t *es;
	model_t *mod;
	int ourowner;

	if (!mins)
		mins = vec3_origin;
	if (!maxs)
		maxs = vec3_origin;

	sv.worldmodel->funcs.NativeTrace(sv.worldmodel, 0, 0, start, end, mins, maxs, contentmask, &tr);
	result->allsolid = tr.allsolid;
	result->contents = tr.contents;
	VectorCopy(tr.endpos, result->endpos);
	result->entityNum = ENTITYNUM_WORLD;
	result->fraction = tr.fraction;
	result->plane = tr.plane;
	result->startsolid = tr.startsolid;
	if (tr.surface)
		result->surfaceFlags = tr.surface->flags;
	else
		result->surfaceFlags = 0;

	for (i = 0; i < 3; i++)
	{
		if (start[i] < end[i])
		{
			mmins[i] = start[i]+mins[i];
			mmaxs[i] = end[i]+maxs[i];
		}
		else
		{
			mmins[i] = end[i]+mins[i];
			mmaxs[i] = start[i]+maxs[i];
		}
	}

	if (entnum == -1)
		ourowner = -1;
	else if ( entnum != ENTITYNUM_WORLD )
	{
		ourowner = GENTITY_FOR_NUM(entnum)->r.ownerNum;
		if (ourowner == ENTITYNUM_WORLD)
			ourowner = -1;
	}
	else 
		ourowner = -1;


	for (i = SVQ3_EntitiesInBox(mmins, mmaxs, contactlist, sizeof(contactlist)/sizeof(contactlist[0]))-1; i >= 0; i--)
	{
		if (contactlist[i] == entnum)
			continue;	//don't collide with self.

		es = GENTITY_FOR_NUM(contactlist[i]);
		if (!(es->r.contents & contentmask))
			continue;

		if (entnum != ENTITYNUM_WORLD)
		{
//			if (contactlist[i] == entnum)
//				continue;	// don't clip against the pass entity
//			if (es->r.ownerNum == entnum)
//				continue;	// don't clip against own missiles
//			if (es->r.ownerNum == ourowner)
//				continue;	// don't clip against other missiles from our owner
		}

		if (es->r.bmodel)
		{
			mod = Mod_ForName(va("*%i", es->s.modelindex), false);
			if (mod->needload)
				continue;
			tr = CM_TransformedBoxTrace(mod, start, end, mins, maxs, contentmask, es->r.currentOrigin, vec3_origin);
		}
		else
		{
			mod = CM_TempBoxModel(es->r.mins, es->r.maxs);
			tr = CM_TransformedBoxTrace(mod, start, end, mins, maxs, contentmask, es->r.currentOrigin, es->r.currentAngles);
//			mod->funcs.Trace(mod, 0, 0, start, end, mins, maxs, &tr);
		}
		if (tr.fraction < result->fraction)
		{
			result->allsolid = tr.allsolid;
			result->contents = tr.contents;
			VectorCopy(tr.endpos, result->endpos);
			result->entityNum = contactlist[i];
			result->fraction = tr.fraction;
			result->plane = tr.plane;
			result->startsolid |= tr.startsolid;
//			if (tr.surface)
//				result->surfaceFlags = tr.surface->flags;
//			else
				result->surfaceFlags = 0;
		}
	}
}

int SVQ3_PointContents(vec3_t pos, int entnum)
{
	int contactlist[128];
	trace_t tr;
	int i;
	q3sharedEntity_t *es;
	model_t *mod;
	int ourowner;

	int cont;


//	sv.worldmodel->funcs.Trace(sv.worldmodel, 0, 0, pos, pos, vec3_origin, vec3_origin, &tr);
//	tr = CM_BoxTrace(sv.worldmodel, pos, pos, vec3_origin, vec3_origin, 0);
	cont = sv.worldmodel->funcs.NativeContents (sv.worldmodel, 0, 0, pos, vec3_origin, vec3_origin);

	if ((unsigned)entnum >= MAX_GENTITIES)
		ourowner = -1;
	else if ( entnum != ENTITYNUM_WORLD )
	{
		ourowner = GENTITY_FOR_NUM(entnum)->r.ownerNum;
		if (ourowner == ENTITYNUM_WORLD)
			ourowner = -1;
	}
	else 
		ourowner = -1;

	for (i = SVQ3_EntitiesInBox(pos, pos, contactlist, sizeof(contactlist)/sizeof(contactlist[0]))-1; i >= 0; i--)
	{
		if (contactlist[i] == entnum)
			continue;	//don't collide with self.

		es = GENTITY_FOR_NUM(contactlist[i]);

		if (entnum != ENTITYNUM_WORLD)
		{
//			if (contactlist[i] == entnum)
//				continue;	// don't clip against the pass entity
//			if (es->r.ownerNum == entnum)
//				continue;	// don't clip against own missiles
//			if (es->r.ownerNum == ourowner)
//				continue;	// don't clip against other missiles from our owner
		}

		if (es->r.bmodel)
		{
			mod = Mod_ForName(va("*%i", es->s.modelindex), false);
			if (mod->needload)
				continue;
			tr = CM_TransformedBoxTrace(mod, pos, pos, vec3_origin, vec3_origin, 0xffffffff, es->r.currentOrigin, vec3_origin);
		}
		else
		{
			mod = CM_TempBoxModel(es->r.mins, es->r.maxs);
			tr = CM_TransformedBoxTrace(mod, pos, pos, vec3_origin, vec3_origin, 0xffffffff, es->r.currentOrigin, es->r.currentAngles);
//			mod->funcs.Trace(mod, 0, 0, start, end, mins, maxs, &tr);
		}

		cont |= tr.contents;
	}
	return cont;
}

int SVQ3_Contact(vec3_t mins, vec3_t maxs, q3sharedEntity_t *ent)
{
	model_t *mod;
	trace_t tr;

	if (!ent->s.modelindex || ent->r.bmodel)
		mod = CM_TempBoxModel(ent->r.mins, ent->r.maxs);
	else
		mod = Mod_ForName(va("*%i", ent->s.modelindex), false);

	if (mod->needload || !mod->funcs.Trace)
		return false;

	mod->funcs.Trace(mod, 0, 0, vec3_origin, vec3_origin, mins, maxs, &tr);

	if (tr.startsolid)
		return true;
	return false;
}

void SVQ3_SetBrushModel(q3sharedEntity_t *ent, char *modelname)
{
	model_t *mod;
	mod = Mod_ForName(modelname, false);
	VectorCopy(mod->mins, ent->r.mins);
	VectorCopy(mod->maxs, ent->r.maxs);
	ent->r.bmodel = true;
	ent->r.contents = -1;
	ent->s.modelindex = atoi(modelname+1);

	Q3G_LinkEntity( ent );
}

static qboolean BoundsIntersect (vec3_t mins1, vec3_t maxs1, vec3_t mins2, vec3_t maxs2)
{
	return (mins1[0] <= maxs2[0] && mins1[1] <= maxs2[1] && mins1[2] <= maxs2[2] &&
		 maxs1[0] >= mins2[0] && maxs1[1] >= mins2[1] && maxs1[2] >= mins2[2]);
}

typedef struct {
	int				serverTime;
	int				angles[3];
	int 			buttons;
	qbyte			weapon;           // weapon 
	signed char	forwardmove, rightmove, upmove;
} q3usercmd_t;
#define CMD_MASK Q3UPDATE_MASK
qboolean SVQ3_GetUserCmd(int clientnumber, q3usercmd_t *ucmd)
{
	usercmd_t *cmd;

	if (clientnumber < 0 || clientnumber >= MAX_CLIENTS)
		SV_Error("SVQ3_GetUserCmd: Client out of range");

	cmd = &svs.clients[clientnumber].lastcmd;
	ucmd->angles[0] = cmd->angles[0];
	ucmd->angles[1] = cmd->angles[1];
	ucmd->angles[2] = cmd->angles[2];
	ucmd->serverTime = cmd->servertime;
	ucmd->forwardmove = cmd->forwardmove;
	ucmd->rightmove = cmd->sidemove;
	ucmd->upmove = cmd->upmove;
	ucmd->buttons = cmd->buttons;
	ucmd->weapon = cmd->weapon;

	return true;
}

void SVQ3_SendServerCommand(client_t *cl, char *str)
{
	if (!cl)
	{	//broadcast
		int i;
		for (i = 0; i < MAX_CLIENTS; i++)
		{
			if (svs.clients[i].state>cs_zombie)
			{
				SVQ3_SendServerCommand(&svs.clients[i], str);	//go for consistancy.
			}
		}
		return;
	}
	
	cl->num_server_commands++;
	Q_strncpyz(cl->server_commands[cl->num_server_commands & TEXTCMD_MASK], str, sizeof(cl->server_commands[0]));
}

void SVQ3_SetConfigString(int num, char *string)
{
	if (!string)
		string = "";
	if (svq3_configstrings[num])
		Z_Free(svq3_configstrings[num]);
	svq3_configstrings[num] = Z_Malloc(strlen(string)+1);
	strcpy(svq3_configstrings[num], string);

	SVQ3_SendServerCommand( NULL, va("cs %i \"%s\"\n", num, string));
}

int FloatAsInt(float f)
{
	return *(int*)&f;
}

int SVQ3_BotGetConsoleMessage( int client, char *buf, int size )
{
	//retrieves server->client commands that were sent to a bot
	client_t	*cl;
	int			index;

	if ((unsigned)client >= MAX_CLIENTS)
		return false;

	cl = &svs.clients[client];
//	cl->lastPacketTime = svs.time;

	if (cl->last_server_command_num == cl->num_server_commands)
		return false;

	cl->last_server_command_num++;
	index = cl->last_server_command_num & TEXTCMD_MASK;

	if ( !cl->server_commands[index][0] )
		return false;

	Q_strncpyz( buf, cl->server_commands[index], size );
	return true;
}
int SVQ3_BotGetSnapshotEntity(int client, int entnum)
{
	//fixme: does the bot actually use this?...
	return -1;
}

void SVQ3_Adjust_Area_Portal_State(q3sharedEntity_t *ge, qboolean open)
{
	q3serverEntity_t *se = SENTITY_FOR_GENTITY(ge);
	CMQ3_SetAreaPortalState(se->areanum, se->areanum2, open);
}

#define VALIDATEPOINTER(o,l) if ((int)o + l >= mask || VM_POINTER(o) < offset) SV_Error("Call to game trap %i passes invalid pointer\n", fn);	//out of bounds.
int Q3G_SystemCallsEx(void *offset, unsigned int mask, int fn, const int *arg)
{
	int ret = 0;
	switch(fn)
	{
	case G_PRINT:		// ( const char *string );
		Con_Printf("%s", VM_POINTER(arg[0]));
		break;
	case G_ERROR:		// ( const char *string );
		SV_Error("Q3 Game error: %s", VM_POINTER(arg[0]));
		break;
	case G_MILLISECONDS:
		return Sys_DoubleTime()*1000;

	case G_CVAR_REGISTER:// ( vmCvar_t *vmCvar, const char *varName, const char *defaultValue, int flags );
		if (arg[0])
			VALIDATEPOINTER(arg[0], sizeof(vmcvar_t));
		return VMQ3_Cvar_Register(VM_POINTER(arg[0]), VM_POINTER(arg[1]), VM_POINTER(arg[2]), VM_LONG(arg[3]));
	case G_CVAR_UPDATE:// ( vmCvar_t *vmCvar );
		VALIDATEPOINTER(arg[0], sizeof(vmcvar_t));
		return VMQ3_Cvar_Update(VM_POINTER(arg[0]));

	case G_CVAR_SET:// ( const char *var_name, const char *value );
		{
			cvar_t *var;
			var = Cvar_FindVar(VM_POINTER(arg[0]));
			if (var)
				Cvar_Set(var, VM_POINTER(arg[1]));	//set it
			else
				Cvar_Get(VM_POINTER(arg[0]), VM_POINTER(arg[1]), 0, "Q3-Game-Code created");	//create one
		}
		break;
	case G_CVAR_VARIABLE_INTEGER_VALUE:// ( const char *var_name );
		{
			cvar_t *var;
			var = Cvar_Get(VM_POINTER(arg[0]), "0", 0, "Q3-Game-Code created");
			if (var)
				return var->value;
		}
		break;
	case G_CVAR_VARIABLE_STRING_BUFFER:// ( const char *var_name, char *buffer, int bufsize );
		{
			cvar_t *var;
			var = Cvar_FindVar(VM_POINTER(arg[0]));
			if (!VM_LONG(arg[2]))
				return 0;
			else if (!var)
			{
				VALIDATEPOINTER(arg[1], 1);
				*(char *)VM_POINTER(arg[1]) = '\0';
				return -1;
			}
			else
			{
				VALIDATEPOINTER(arg[1], arg[2]);
				Q_strncpyz(VM_POINTER(arg[1]), var->string, VM_LONG(arg[2]));
			}
		}
		break;

	case G_BOT_FREE_CLIENT:
	case G_DROP_CLIENT:
		if ((unsigned)VM_LONG(arg[0]) < MAX_CLIENTS)
			SV_DropClient(&svs.clients[VM_LONG(arg[0])]);
		break;

	case G_BOT_ALLOCATE_CLIENT:
		return SVQ3_AddBot();

	case G_ARGC:		//8
		return Cmd_Argc();
	case G_ARGV:				//9
		VALIDATEPOINTER(arg[1], arg[2]);
		Q_strncpyz(VM_POINTER(arg[1]), Cmd_Argv(VM_LONG(arg[0])), VM_LONG(arg[2]));
		break;

	case G_SEND_CONSOLE_COMMAND:
		Cbuf_AddText(VM_POINTER(arg[1]), RESTRICT_SERVER);
		return 0;

	
	case G_FS_FOPEN_FILE: //fopen
		if ((int)arg[1] + 4 >= mask || VM_POINTER(arg[1]) < offset)
			break;	//out of bounds.
		VM_LONG(ret) = VM_fopen(VM_POINTER(arg[0]), VM_POINTER(arg[1]), VM_LONG(arg[2]), 0);
		break;

	case G_FS_READ:	//fread
		if ((int)arg[0] + VM_LONG(arg[1]) >= mask || VM_POINTER(arg[0]) < offset)
			break;	//out of bounds.

		VM_FRead(VM_POINTER(arg[0]), VM_LONG(arg[1]), VM_LONG(arg[2]), 0);
		break;
	case G_FS_WRITE:	//fwrite
		break;
	case G_FS_FCLOSE_FILE:	//fclose
		VM_fclose(VM_LONG(arg[0]), 0);
		break;

	case G_FS_GETFILELIST:	//fs listing
		if ((int)arg[2] + arg[3] >= mask || VM_POINTER(arg[2]) < offset)
			break;	//out of bounds.
		return VM_GetFileList(VM_POINTER(arg[0]), VM_POINTER(arg[1]), VM_POINTER(arg[2]), VM_LONG(arg[3]));

	case G_LOCATE_GAME_DATA:		// ( gentity_t *gEnts, int numGEntities, int sizeofGEntity_t,	15
	//							playerState_t *clients, int sizeofGameClient );

		if (VM_OOB(arg[0], arg[1]*arg[2]) || VM_OOB(arg[3], arg[4]*MAX_CLIENTS))
			SV_Error("Gamedata is out of bounds\n");
		q3_entarray = VM_POINTER(arg[0]);
		numq3entities = VM_LONG(arg[1]);
		sizeofq3gentity = VM_LONG(arg[2]);
		q3playerstates = VM_POINTER(arg[3]);
		sizeofGameClient = VM_LONG(arg[4]);

		if (numq3entities > MAX_GENTITIES)
			SV_Error("Gamecode specifies too many entities");
		break;

	case G_SEND_SERVER_COMMAND:		// ( int clientNum, const char *fmt, ... );						17

		Con_DPrintf("Game dispatching %s\n", VM_POINTER(arg[1]));
		if (VM_LONG(arg[0]) == -1)
		{	//broadcast
			SVQ3_SendServerCommand(NULL, VM_POINTER(arg[1]));
		}
		else
		{
			int i = VM_LONG(arg[0]);
			if (i < 0 || i >= MAX_CLIENTS)
				return false;
			SVQ3_SendServerCommand(&svs.clients[i], VM_POINTER(arg[1]));
		}
		break;

	case G_SET_CONFIGSTRING:	// ( int num, const char *string );									18
		if (arg[0] < 0 || arg[0] >= MAX_CONFIGSTRINGS)
			return 0;
		SVQ3_SetConfigString(arg[0], VM_POINTER(arg[1]));
		break;
	case G_GET_CONFIGSTRING:	// ( int num, char *buffer, int bufferSize );						19
		if (arg[0] < 0 || arg[0] >= MAX_CONFIGSTRINGS || !arg[2])
			return 0;
		VALIDATEPOINTER(arg[1], arg[2]);
		if (svq3_configstrings[arg[0]])
			Q_strncpyz(VM_POINTER(arg[1]), svq3_configstrings[arg[0]], arg[2]);
		else
			*(char*)VM_POINTER(arg[1]) = '\0';
		break;

	case G_GET_SERVERINFO:
		{
			char *dest = VM_POINTER(arg[0]);
			int length = VM_LONG(arg[1]);
			Q_strncpyz(dest, svs.info, length);
		}
		return true;
	case G_GET_USERINFO://int num, char *buffer, int bufferSize										20
		if (VM_OOB(arg[1], arg[2]))
			return 0;
		if ((unsigned)VM_LONG(arg[0]) >= sv.allocated_client_slots)
			return 0;
		Q_strncpyz(VM_POINTER(arg[1]), svs.clients[VM_LONG(arg[0])].userinfo, VM_LONG(arg[2]));
		break;

	case G_SET_USERINFO://int num, char *buffer										20
		Q_strncpyz(svs.clients[VM_LONG(arg[0])].userinfo, VM_POINTER(arg[1]), sizeof(svs.clients[0].userinfo));
		SV_ExtractFromUserinfo(&svs.clients[VM_LONG(arg[0])]);
		break;

	case G_LINKENTITY:		// ( gentity_t *ent );													30
		Q3G_LinkEntity(VM_POINTER(arg[0]));
		break;
	case G_UNLINKENTITY:		// ( gentity_t *ent );												31
		Q3G_UnlinkEntity(VM_POINTER(arg[0]));
		break;
	case G_TRACE:	// ( trace_t *results, const vec3_t start, const vec3_t mins, const vec3_t maxs, const vec3_t end, int passEntityNum, int contentmask );
		VALIDATEPOINTER(arg[0], sizeof(q3trace_t));
		SVQ3_Trace(VM_POINTER(arg[0]), VM_POINTER(arg[1]), VM_POINTER(arg[2]), VM_POINTER(arg[3]), VM_POINTER(arg[4]), VM_LONG(arg[5]), VM_LONG(arg[6]));
		break;
	case G_ENTITY_CONTACT:
			// ( const vec3_t mins, const vec3_t maxs, const gentity_t *ent );	33
	// perform an exact check against inline brush models of non-square shape
		return SVQ3_Contact(VM_POINTER(arg[0]), VM_POINTER(arg[1]), VM_POINTER(arg[2]));
		break;
	case G_ENTITIES_IN_BOX:	// ( const vec3_t mins, const vec3_t maxs, gentity_t **list, int maxcount );	32
	// EntitiesInBox will return brush models based on their bounding box,
	// so exact determination must still be done with EntityContact
		VALIDATEPOINTER(arg[2], sizeof(int)*VM_LONG(arg[3]));
		return SVQ3_EntitiesInBox(VM_POINTER(arg[0]), VM_POINTER(arg[1]), VM_POINTER(arg[2]), VM_LONG(arg[3]));

	case G_ADJUST_AREA_PORTAL_STATE:
		SVQ3_Adjust_Area_Portal_State(VM_POINTER(arg[0]), arg[1]);
		break;
	case G_POINT_CONTENTS:
		return SVQ3_PointContents(VM_POINTER(arg[0]), -1);
	case G_SET_BRUSH_MODEL:	//ent, name
		VALIDATEPOINTER(arg[0], sizeof(q3sharedEntity_t));
		SVQ3_SetBrushModel(VM_POINTER(arg[0]), VM_POINTER(arg[1]));
		break;
	case G_GET_USERCMD:		// ( int clientNum, usercmd_t *cmd )									36
		VALIDATEPOINTER(arg[1], sizeof(q3usercmd_t));
		SVQ3_GetUserCmd(VM_LONG(arg[0]), VM_POINTER(arg[1]));
		break;
	case G_GET_ENTITY_TOKEN:	// qboolean ( char *buffer, int bufferSize )						37
		mapentspointer = COM_ParseOut(mapentspointer, VM_POINTER(arg[0]), arg[1]);
		return !!mapentspointer;

	case G_REAL_TIME:																			//	41
		VM_FLOAT(ret) = realtime;
		return ret;
	case G_SNAPVECTOR:
		{
			float *fp = (float *)VM_POINTER( arg[0] );
			VALIDATEPOINTER(arg[0], sizeof(vec3_t));
			fp[0] = Q_rint(fp[0]);
			fp[1] = Q_rint(fp[1]);
			fp[2] = Q_rint(fp[2]);
		}
		break;
	// standard Q3
	case G_MEMSET:
		VALIDATEPOINTER(arg[0], arg[2]);
		{
			void *dst = VM_POINTER(arg[0]);
			memset(dst, arg[1], arg[2]);
		}
		break;
	case G_MEMCPY:
		VALIDATEPOINTER(arg[0], arg[2]);
		{
			void *dst = VM_POINTER(arg[0]);
			void *src = VM_POINTER(arg[1]);
			memmove(dst, src, arg[2]);
		}
		break;
	case G_STRNCPY:
		VALIDATEPOINTER(arg[0], arg[2]);
		{
			void *dst = VM_POINTER(arg[0]);
			void *src = VM_POINTER(arg[1]);
			Q_strncpyS(src, dst, arg[2]);
		}
		break;
	case G_SIN:
		VM_FLOAT(ret)=(float)sin(VM_FLOAT(arg[0]));
		break;
	case G_COS:
		VM_FLOAT(ret)=(float)cos(VM_FLOAT(arg[0]));
		break;
//	case G_ACOS:
//		VM_FLOAT(ret)=(float)acos(VM_FLOAT(arg[0]));
//		break;
	case G_ATAN2:
		VM_FLOAT(ret)=(float)atan2(VM_FLOAT(arg[0]), VM_FLOAT(arg[1]));
		break;
	case G_SQRT:
		VM_FLOAT(ret)=(float)sqrt(VM_FLOAT(arg[0]));
		break;
	case G_FLOOR:
		VM_FLOAT(ret)=(float)floor(VM_FLOAT(arg[0]));
		break;
	case G_CEIL:
		VM_FLOAT(ret)=(float)ceil(VM_FLOAT(arg[0]));
		break;

#ifdef USEBOTLIB
	case BOTLIB_SETUP:
		return botlib->BotLibSetup();
	case BOTLIB_SHUTDOWN:
		return botlib->BotLibShutdown();
	case BOTLIB_LIBVAR_SET:
		return botlib->BotLibVarSet(VM_POINTER(arg[0]), VM_POINTER(arg[1]));
	case BOTLIB_LIBVAR_GET:
		VALIDATEPOINTER(arg[1], arg[2]);
		return botlib->BotLibVarGet(VM_POINTER(arg[0]), VM_POINTER(arg[1]), VM_LONG(arg[2]));
	case BOTLIB_PC_ADD_GLOBAL_DEFINE:
		return botlib->PC_AddGlobalDefine(VM_POINTER(arg[0]));
	case BOTLIB_START_FRAME:
		return botlib->BotLibStartFrame(VM_FLOAT(arg[0]));
	case BOTLIB_LOAD_MAP:
		return botlib->BotLibLoadMap(VM_POINTER(arg[0]));

	case BOTLIB_UPDATENTITY:
		return botlib->BotLibUpdateEntity(VM_LONG(arg[0]), VM_POINTER(arg[1]));
	case BOTLIB_TEST:
		return botlib->Test(VM_LONG(arg[0]), VM_POINTER(arg[1]), VM_POINTER(arg[2]), VM_POINTER(arg[3]));
	case BOTLIB_GET_SNAPSHOT_ENTITY:
		return SVQ3_BotGetSnapshotEntity(VM_LONG(arg[0]), VM_LONG(arg[1]));
	case BOTLIB_GET_CONSOLE_MESSAGE:
		VALIDATEPOINTER(arg[1], arg[2]);
		return SVQ3_BotGetConsoleMessage(arg[0], VM_POINTER(arg[1]), VM_LONG(arg[2]));
	case BOTLIB_USER_COMMAND:
		{
			q3usercmd_t *uc = VM_POINTER(arg[1]);
			int i = VM_LONG(arg[0]);
			if ((unsigned)i >= MAX_CLIENTS)
				return 1;
			svs.clients[i].lastcmd.angles[0] = uc->angles[0];
			svs.clients[i].lastcmd.angles[1] = uc->angles[1];
			svs.clients[i].lastcmd.angles[2] = uc->angles[2];
			svs.clients[i].lastcmd.upmove = uc->upmove;
			svs.clients[i].lastcmd.sidemove = uc->rightmove;
			svs.clients[i].lastcmd.forwardmove = uc->forwardmove;
			svs.clients[i].lastcmd.servertime = uc->serverTime;
			svs.clients[i].lastcmd.weapon = uc->weapon;
			svs.clients[i].lastcmd.buttons = uc->buttons;
			SVQ3_ClientThink(&svs.clients[i]);
		}
		return 0;


	case BOTLIB_AAS_ENABLE_ROUTING_AREA:
		return botlib->aas.AAS_EnableRoutingArea(VM_LONG(arg[0]), VM_LONG(arg[1]));
	case BOTLIB_AAS_BBOX_AREAS:
		//FIXME: validatepointer arg2
		return botlib->aas.AAS_BBoxAreas(VM_POINTER(arg[0]), VM_POINTER(arg[1]), VM_POINTER(arg[2]), VM_LONG(arg[3]));
	case BOTLIB_AAS_AREA_INFO:
		return botlib->aas.AAS_AreaInfo(VM_LONG(arg[0]), VM_POINTER(arg[1]));
	case BOTLIB_AAS_ENTITY_INFO:
		botlib->aas.AAS_EntityInfo(VM_LONG(arg[0]), VM_POINTER(arg[1]));
		return 0;

	case BOTLIB_AAS_INITIALIZED:
		return botlib->aas.AAS_Initialized();
	case BOTLIB_AAS_PRESENCE_TYPE_BOUNDING_BOX:
		botlib->aas.AAS_PresenceTypeBoundingBox(VM_LONG(arg[0]), VM_POINTER(arg[1]), VM_POINTER(arg[2]));
		return 0;
	case BOTLIB_AAS_TIME:
		return FloatAsInt(botlib->aas.AAS_Time());

	case BOTLIB_AAS_POINT_AREA_NUM:
		return botlib->aas.AAS_PointAreaNum(VM_POINTER(arg[0]));
	case BOTLIB_AAS_TRACE_AREAS:
		return botlib->aas.AAS_TraceAreas(VM_POINTER(arg[0]), VM_POINTER(arg[1]), VM_POINTER(arg[2]), VM_POINTER(arg[3]), VM_LONG(arg[4]));

	case BOTLIB_AAS_POINT_CONTENTS:
		return botlib->aas.AAS_PointContents(VM_POINTER(arg[0]));
	case BOTLIB_AAS_NEXT_BSP_ENTITY:
		return botlib->aas.AAS_NextBSPEntity(VM_LONG(arg[0]));
	case BOTLIB_AAS_VALUE_FOR_BSP_EPAIR_KEY:
		VALIDATEPOINTER(arg[2], arg[3]);
		return botlib->aas.AAS_ValueForBSPEpairKey(VM_LONG(arg[0]), VM_POINTER(arg[1]), VM_POINTER(arg[2]), VM_LONG(arg[3]));
	case BOTLIB_AAS_VECTOR_FOR_BSP_EPAIR_KEY:
		VALIDATEPOINTER(arg[2], sizeof(vec3_t));
		return botlib->aas.AAS_VectorForBSPEpairKey(VM_LONG(arg[0]), VM_POINTER(arg[1]), VM_POINTER(arg[2]));
	case BOTLIB_AAS_FLOAT_FOR_BSP_EPAIR_KEY:
		VALIDATEPOINTER(arg[2], sizeof(float));
		return botlib->aas.AAS_FloatForBSPEpairKey(VM_LONG(arg[0]), VM_POINTER(arg[1]), VM_POINTER(arg[2]));
	case BOTLIB_AAS_INT_FOR_BSP_EPAIR_KEY:
		VALIDATEPOINTER(arg[2], sizeof(int));
		return botlib->aas.AAS_IntForBSPEpairKey(VM_LONG(arg[0]), VM_POINTER(arg[1]), VM_POINTER(arg[2]));

	case BOTLIB_AAS_AREA_REACHABILITY:
		return botlib->aas.AAS_AreaReachability(VM_LONG(arg[0]));

	case BOTLIB_AAS_AREA_TRAVEL_TIME_TO_GOAL_AREA:
		return botlib->aas.AAS_AreaTravelTimeToGoalArea(VM_LONG(arg[0]), VM_POINTER(arg[1]), VM_LONG(arg[2]), VM_LONG(arg[3]));

	case BOTLIB_AAS_SWIMMING:
		return botlib->aas.AAS_Swimming(VM_POINTER(arg[0]));
	case BOTLIB_AAS_PREDICT_CLIENT_MOVEMENT:
		return botlib->aas.AAS_PredictClientMovement(VM_POINTER(arg[0]),
											VM_LONG(arg[1]), VM_POINTER(arg[2]),
											VM_LONG(arg[3]), VM_LONG(arg[4]),
											VM_POINTER(arg[5]), VM_POINTER(arg[6]),
											VM_LONG(arg[7]),
											VM_LONG(arg[8]), VM_FLOAT(arg[9]),
											VM_LONG(arg[10]), VM_LONG(arg[11]), VM_LONG(arg[12]));

	case BOTLIB_EA_SAY:
		botlib->ea.EA_Say(VM_LONG(arg[0]), VM_POINTER(arg[1]));
		return 0;
	case BOTLIB_EA_SAY_TEAM:
		botlib->ea.EA_SayTeam(VM_LONG(arg[0]), VM_POINTER(arg[1]));
		return 0;
	case BOTLIB_EA_COMMAND:
		botlib->ea.EA_Command(VM_LONG(arg[0]), VM_POINTER(arg[1]));
		return 0;

	case BOTLIB_EA_ACTION:
		botlib->ea.EA_Action(VM_LONG(arg[0]), VM_LONG(arg[1]));
		return 0;
	case BOTLIB_EA_GESTURE:
		botlib->ea.EA_Gesture(VM_LONG(arg[0]));
		return 0;
	case BOTLIB_EA_TALK:
		botlib->ea.EA_Talk(VM_LONG(arg[0]));
		return 0;
	case BOTLIB_EA_ATTACK:
		botlib->ea.EA_Attack(VM_LONG(arg[0]));
		return 0;
	case BOTLIB_EA_USE:
		botlib->ea.EA_Use(VM_LONG(arg[0]));
		return 0;
	case BOTLIB_EA_RESPAWN:
		botlib->ea.EA_Respawn(VM_LONG(arg[0]));
		return 0;
	case BOTLIB_EA_CROUCH:
		botlib->ea.EA_Crouch(VM_LONG(arg[0]));
		return 0;
	case BOTLIB_EA_MOVE_UP:
		botlib->ea.EA_MoveUp(VM_LONG(arg[0]));
		return 0;
	case BOTLIB_EA_MOVE_DOWN:
		botlib->ea.EA_MoveDown(VM_LONG(arg[0]));
		return 0;
	case BOTLIB_EA_MOVE_FORWARD:
		botlib->ea.EA_MoveForward(VM_LONG(arg[0]));
		return 0;
	case BOTLIB_EA_MOVE_BACK:
		botlib->ea.EA_MoveBack(VM_LONG(arg[0]));
		return 0;
	case BOTLIB_EA_MOVE_LEFT:
		botlib->ea.EA_MoveLeft(VM_LONG(arg[0]));
		return 0;
	case BOTLIB_EA_MOVE_RIGHT:
		botlib->ea.EA_MoveRight(VM_LONG(arg[0]));
		return 0;

	case BOTLIB_EA_SELECT_WEAPON:
		botlib->ea.EA_SelectWeapon(VM_LONG(arg[0]), VM_LONG(arg[1]));
		return 0;
	case BOTLIB_EA_JUMP:
		botlib->ea.EA_Jump(VM_LONG(arg[0]));
		return 0;
	case BOTLIB_EA_DELAYED_JUMP:
		botlib->ea.EA_DelayedJump(VM_LONG(arg[0]));
		return 0;
	case BOTLIB_EA_MOVE:
		botlib->ea.EA_Move(VM_LONG(arg[0]), VM_POINTER(arg[1]), VM_FLOAT(arg[2]));
		return 0;
	case BOTLIB_EA_VIEW:
		botlib->ea.EA_View(VM_LONG(arg[0]), VM_POINTER(arg[1]));
		return 0;

	case BOTLIB_EA_END_REGULAR:
		botlib->ea.EA_EndRegular(VM_LONG(arg[0]), VM_FLOAT(arg[1]));
		return 0;
	case BOTLIB_EA_GET_INPUT:
		botlib->ea.EA_GetInput(VM_LONG(arg[0]), VM_FLOAT(arg[1]), VM_POINTER(arg[2]));
		return 0;
	case BOTLIB_EA_RESET_INPUT:
		botlib->ea.EA_ResetInput(VM_LONG(arg[0]));
		return 0;


	case BOTLIB_AI_LOAD_CHARACTER:
		return botlib->ai.BotLoadCharacter(VM_POINTER(arg[0]), VM_FLOAT(arg[1]));
	case BOTLIB_AI_FREE_CHARACTER:
		botlib->ai.BotFreeCharacter(arg[0]);
		return 0;
	case BOTLIB_AI_CHARACTERISTIC_FLOAT:
		return FloatAsInt(botlib->ai.Characteristic_Float(arg[0], arg[1]));
	case BOTLIB_AI_CHARACTERISTIC_BFLOAT:
		return FloatAsInt(botlib->ai.Characteristic_BFloat(arg[0], arg[1], VM_FLOAT(arg[2]), VM_FLOAT(arg[3])));
	case BOTLIB_AI_CHARACTERISTIC_INTEGER:
		return botlib->ai.Characteristic_Integer(arg[0], arg[1]);
	case BOTLIB_AI_CHARACTERISTIC_BINTEGER:
		return botlib->ai.Characteristic_BInteger(arg[0], arg[1], arg[2], arg[3]);
	case BOTLIB_AI_CHARACTERISTIC_STRING:
		VALIDATEPOINTER(arg[2], arg[3]);
		botlib->ai.Characteristic_String(arg[0], arg[1], VM_POINTER(arg[2]), arg[3]);
		return 0;

	case BOTLIB_AI_ALLOC_CHAT_STATE:
		return botlib->ai.BotAllocChatState();
	case BOTLIB_AI_FREE_CHAT_STATE:
		botlib->ai.BotFreeChatState(arg[0]);
		return 0;

	case BOTLIB_AI_QUEUE_CONSOLE_MESSAGE:
		botlib->ai.BotQueueConsoleMessage(arg[0], arg[1], VM_POINTER(arg[2]));
		return 0;
	case BOTLIB_AI_REMOVE_CONSOLE_MESSAGE:
		botlib->ai.BotRemoveConsoleMessage(arg[0], arg[1]);
		return 0;
	case BOTLIB_AI_NEXT_CONSOLE_MESSAGE:
		return botlib->ai.BotNextConsoleMessage(arg[0], VM_POINTER(arg[1]));
	case BOTLIB_AI_NUM_CONSOLE_MESSAGE:
		return botlib->ai.BotNumConsoleMessages(arg[0]);
	case BOTLIB_AI_INITIAL_CHAT:
		botlib->ai.BotInitialChat(arg[0], VM_POINTER(arg[1]), arg[2], VM_POINTER(arg[3]), VM_POINTER(arg[4]), VM_POINTER(arg[5]), VM_POINTER(arg[6]), VM_POINTER(arg[7]), VM_POINTER(arg[8]), VM_POINTER(arg[9]), VM_POINTER(arg[10]));
		return 0;
	case BOTLIB_AI_REPLY_CHAT:
		return botlib->ai.BotReplyChat(arg[0], VM_POINTER(arg[1]), arg[2], arg[3], VM_POINTER(arg[4]), VM_POINTER(arg[5]), VM_POINTER(arg[6]), VM_POINTER(arg[7]), VM_POINTER(arg[8]), VM_POINTER(arg[9]), VM_POINTER(arg[10]), VM_POINTER(arg[11]));
	case BOTLIB_AI_CHAT_LENGTH:
		return botlib->ai.BotChatLength(arg[0]);
	case BOTLIB_AI_ENTER_CHAT:
		botlib->ai.BotEnterChat(arg[0], arg[1], arg[2]);
		return 0;
	case BOTLIB_AI_STRING_CONTAINS:
		return botlib->ai.StringContains(VM_POINTER(arg[0]), VM_POINTER(arg[1]), arg[2]);
	case BOTLIB_AI_FIND_MATCH:
		return botlib->ai.BotFindMatch(VM_POINTER(arg[0]), VM_POINTER(arg[1]), arg[2]);
	case BOTLIB_AI_MATCH_VARIABLE:
		botlib->ai.BotMatchVariable(VM_POINTER(arg[0]), arg[1], VM_POINTER(arg[2]), arg[3]);
		return 0;
	case BOTLIB_AI_UNIFY_WHITE_SPACES:
		botlib->ai.UnifyWhiteSpaces(VM_POINTER(arg[0]));
		return 0;
	case BOTLIB_AI_REPLACE_SYNONYMS:
		botlib->ai.BotReplaceSynonyms(VM_POINTER(arg[0]), arg[1]);
		return 0;
	case BOTLIB_AI_LOAD_CHAT_FILE:
		return botlib->ai.BotLoadChatFile(arg[0], VM_POINTER(arg[1]), VM_POINTER(arg[2]));
	case BOTLIB_AI_SET_CHAT_GENDER:
		botlib->ai.BotSetChatGender(arg[0], arg[1]);
		return 0;
	case BOTLIB_AI_SET_CHAT_NAME:
		botlib->ai.BotSetChatName(arg[0], VM_POINTER(arg[1]), arg[2]);
		return 0;



	case BOTLIB_AI_RESET_GOAL_STATE:
		botlib->ai.BotResetGoalState(VM_LONG(arg[0]));
		return 0;
	case BOTLIB_AI_RESET_AVOID_GOALS:
		botlib->ai.BotResetAvoidGoals(VM_LONG(arg[0]));
		return 0;
	case BOTLIB_AI_PUSH_GOAL:
		botlib->ai.BotPushGoal(VM_LONG(arg[0]), VM_POINTER(arg[1]));
		return 0;
	case BOTLIB_AI_POP_GOAL:
		botlib->ai.BotPopGoal(VM_LONG(arg[0]));
		return 0;

	case BOTLIB_AI_EMPTY_GOAL_STACK:
		botlib->ai.BotEmptyGoalStack(VM_LONG(arg[0]));
		return 0;
	case BOTLIB_AI_DUMP_AVOID_GOALS:
		botlib->ai.BotDumpAvoidGoals(VM_LONG(arg[0]));
		return 0;
	case BOTLIB_AI_DUMP_GOAL_STACK:
		botlib->ai.BotDumpGoalStack(VM_LONG(arg[0]));
		return 0;

	case BOTLIB_AI_GOAL_NAME:
		VALIDATEPOINTER(arg[1], arg[2]);
		botlib->ai.BotGoalName(VM_LONG(arg[0]), VM_POINTER(arg[1]), VM_LONG(arg[2]));
		return 0;

	case BOTLIB_AI_GET_TOP_GOAL:
		//FIXME: validatepointer ?
		return botlib->ai.BotGetTopGoal(VM_LONG(arg[0]), VM_POINTER(arg[1]));
	case BOTLIB_AI_GET_SECOND_GOAL:
		//FIXME: validatepointer ?
		return botlib->ai.BotGetSecondGoal(VM_LONG(arg[0]), VM_POINTER(arg[1]));
	case BOTLIB_AI_CHOOSE_LTG_ITEM:
		//FIXME: validatepointer ?
		return botlib->ai.BotChooseLTGItem(VM_LONG(arg[0]), VM_POINTER(arg[1]), VM_POINTER(arg[2]), VM_LONG(arg[3]));
	case BOTLIB_AI_CHOOSE_NBG_ITEM:
		//FIXME: validatepointer ?
		return botlib->ai.BotChooseNBGItem(VM_LONG(arg[0]), VM_POINTER(arg[1]), VM_POINTER(arg[2]), VM_LONG(arg[3]), VM_POINTER(arg[4]), VM_FLOAT(arg[5]));	
	case BOTLIB_AI_TOUCHING_GOAL:
		return botlib->ai.BotTouchingGoal(VM_POINTER(arg[0]), VM_POINTER(arg[1]));
	case BOTLIB_AI_ITEM_GOAL_IN_VIS_BUT_NOT_VISIBLE:
		return botlib->ai.BotItemGoalInVisButNotVisible(arg[0], VM_POINTER(arg[1]), VM_POINTER(arg[2]), VM_POINTER(arg[3]));

	case BOTLIB_AI_GET_LEVEL_ITEM_GOAL:
		return botlib->ai.BotGetLevelItemGoal(arg[0], VM_POINTER(arg[1]), VM_POINTER(arg[2]));
	case BOTLIB_AI_AVOID_GOAL_TIME:
		return botlib->ai.BotAvoidGoalTime(arg[0], arg[1]);
	case BOTLIB_AI_INIT_LEVEL_ITEMS:
		botlib->ai.BotInitLevelItems();
		return 0;

	case BOTLIB_AI_UPDATE_ENTITY_ITEMS:
		botlib->ai.BotUpdateEntityItems();
		return 0;

	case BOTLIB_AI_LOAD_ITEM_WEIGHTS:
		return botlib->ai.BotLoadItemWeights(arg[0], VM_POINTER(arg[1]));

	case BOTLIB_AI_FREE_ITEM_WEIGHTS:
		botlib->ai.BotFreeItemWeights(arg[0]);
		return 0;

	case BOTLIB_AI_SAVE_GOAL_FUZZY_LOGIC:
		botlib->ai.BotSaveGoalFuzzyLogic(arg[0], VM_POINTER(arg[1]));
		return 0;
	case BOTLIB_AI_ALLOC_GOAL_STATE:
		return botlib->ai.BotAllocGoalState(VM_LONG(arg[0]));
	case BOTLIB_AI_FREE_GOAL_STATE:
		botlib->ai.BotFreeGoalState(VM_LONG(arg[0]));
		return 0;

	case BOTLIB_AI_RESET_MOVE_STATE:
		botlib->ai.BotResetMoveState(VM_LONG(arg[0]));
		return 0;
	case BOTLIB_AI_MOVE_TO_GOAL:
		botlib->ai.BotMoveToGoal(VM_POINTER(arg[0]), VM_LONG(arg[1]), VM_POINTER(arg[2]), VM_LONG(arg[3]));
		return 0;
	case BOTLIB_AI_MOVE_IN_DIRECTION:
		return botlib->ai.BotMoveInDirection(VM_LONG(arg[0]), VM_POINTER(arg[1]), VM_FLOAT(arg[2]), VM_LONG(arg[3]));

	case BOTLIB_AI_RESET_AVOID_REACH:
		botlib->ai.BotUpdateEntityItems();
		return 0;

	case BOTLIB_AI_RESET_LAST_AVOID_REACH:
		botlib->ai.BotResetLastAvoidReach(arg[0]);
		return 0;
	case BOTLIB_AI_REACHABILITY_AREA:
		return botlib->ai.BotReachabilityArea(VM_POINTER(arg[0]), arg[1]);

	case BOTLIB_AI_MOVEMENT_VIEW_TARGET:
		return botlib->ai.BotMovementViewTarget(arg[0], VM_POINTER(arg[1]), arg[2], VM_FLOAT(arg[3]), VM_POINTER(arg[4]));

	case BOTLIB_AI_ALLOC_MOVE_STATE:
		return botlib->ai.BotAllocMoveState();
	case BOTLIB_AI_FREE_MOVE_STATE:
		botlib->ai.BotFreeMoveState(VM_LONG(arg[0]));
		return 0;
	case BOTLIB_AI_INIT_MOVE_STATE:
		//FIXME: validatepointer?
		botlib->ai.BotInitMoveState(VM_LONG(arg[0]), VM_POINTER(arg[1]));
		return 0;

	case BOTLIB_AI_CHOOSE_BEST_FIGHT_WEAPON:
		return botlib->ai.BotChooseBestFightWeapon(VM_LONG(arg[0]), VM_POINTER(arg[1]));
	case BOTLIB_AI_GET_WEAPON_INFO:
		botlib->ai.BotGetWeaponInfo(VM_LONG(arg[0]), VM_LONG(arg[1]), VM_POINTER(arg[2]));
		return 0;
	case BOTLIB_AI_LOAD_WEAPON_WEIGHTS:
		return botlib->ai.BotLoadWeaponWeights(VM_LONG(arg[0]), VM_POINTER(arg[1]));
	case BOTLIB_AI_ALLOC_WEAPON_STATE:
		return botlib->ai.BotAllocWeaponState();
	case BOTLIB_AI_FREE_WEAPON_STATE:
		botlib->ai.BotFreeWeaponState(VM_LONG(arg[0]));
		return 0;
	case BOTLIB_AI_RESET_WEAPON_STATE:
		botlib->ai.BotResetWeaponState(VM_LONG(arg[0]));
		return 0;

	case BOTLIB_AI_GENETIC_PARENTS_AND_CHILD_SELECTION:
		return botlib->ai.GeneticParentsAndChildSelection(VM_LONG(arg[0]), VM_POINTER(arg[1]), VM_POINTER(arg[2]), VM_POINTER(arg[3]), VM_POINTER(arg[4]));
	case BOTLIB_AI_INTERBREED_GOAL_FUZZY_LOGIC:
		botlib->ai.BotInterbreedGoalFuzzyLogic(VM_LONG(arg[0]), VM_LONG(arg[1]), VM_LONG(arg[2]));
		return 0;
	case BOTLIB_AI_MUTATE_GOAL_FUZZY_LOGIC:
		botlib->ai.BotMutateGoalFuzzyLogic(VM_LONG(arg[0]), VM_FLOAT(arg[1]));
		return 0;
	case BOTLIB_AI_GET_NEXT_CAMP_SPOT_GOAL:
		return botlib->ai.BotGetNextCampSpotGoal(VM_LONG(arg[0]), VM_POINTER(arg[1]));
	case BOTLIB_AI_GET_MAP_LOCATION_GOAL:
		return botlib->ai.BotGetMapLocationGoal(VM_POINTER(arg[0]), VM_POINTER(arg[1]));
	case BOTLIB_AI_NUM_INITIAL_CHATS:
		return botlib->ai.BotNumInitialChats(VM_LONG(arg[0]), VM_POINTER(arg[1]));
	case BOTLIB_AI_GET_CHAT_MESSAGE:
		VALIDATEPOINTER(arg[1], arg[2]);
		botlib->ai.BotGetChatMessage(VM_LONG(arg[0]), VM_POINTER(arg[1]), VM_LONG(arg[2]));
		return 0;
	case BOTLIB_AI_REMOVE_FROM_AVOID_GOALS:
		botlib->ai.BotRemoveFromAvoidGoals(VM_LONG(arg[0]), VM_LONG(arg[1]));
		return 0;
	case BOTLIB_AI_PREDICT_VISIBLE_POSITION:
		return botlib->ai.BotPredictVisiblePosition(VM_POINTER(arg[0]), VM_LONG(arg[1]), VM_POINTER(arg[2]), VM_LONG(arg[3]), VM_POINTER(arg[4]));

	case BOTLIB_AI_SET_AVOID_GOAL_TIME:
		botlib->ai.BotSetAvoidGoalTime(VM_LONG(arg[0]), VM_LONG(arg[1]), VM_FLOAT(arg[2]));
		return 0;
		
	case BOTLIB_AI_ADD_AVOID_SPOT:
		botlib->ai.BotAddAvoidSpot(VM_LONG(arg[0]), VM_POINTER(arg[1]), VM_FLOAT(arg[2]), VM_LONG(arg[3]));
		return 0;

	case BOTLIB_AAS_ALTERNATIVE_ROUTE_GOAL:
		return botlib->aas.AAS_AlternativeRouteGoals(VM_POINTER(arg[0]), arg[1], VM_POINTER(arg[2]), arg[3], arg[4],
							VM_POINTER(arg[5]), arg[6], arg[7]);
	case BOTLIB_AAS_PREDICT_ROUTE:
		return botlib->aas.AAS_PredictRoute(VM_POINTER(arg[0]), arg[1], VM_POINTER(arg[2]), arg[3], arg[4], arg[5], arg[6],
							arg[7], arg[8], arg[9], arg[10]);
	case BOTLIB_AAS_POINT_REACHABILITY_AREA_INDEX:
		return botlib->aas.AAS_PointReachabilityAreaIndex(VM_POINTER(arg[0]));

  	case BOTLIB_PC_LOAD_SOURCE:
		if (!botlib)
		{
			SV_Error("Botlib is not installed (trap BOTLIB_PC_LOAD_SOURCE)\n");
		}
		return botlib->PC_LoadSourceHandle(VM_POINTER(arg[0]));
	case BOTLIB_PC_FREE_SOURCE:
		return botlib->PC_FreeSourceHandle(VM_LONG(arg[0]));
	case BOTLIB_PC_READ_TOKEN:
		//fixme: validatepointer
		return botlib->PC_ReadTokenHandle(VM_LONG(arg[0]), VM_POINTER(arg[1]));
	case BOTLIB_PC_SOURCE_FILE_AND_LINE:
		//fixme: validatepointer
		return botlib->PC_SourceFileAndLine(VM_LONG(arg[0]), VM_POINTER(arg[1]), VM_POINTER(arg[2]));

#endif

//	notimplemented:
	default:
		Con_Printf("builtin %i is not implemented\n", fn);
	}
	return ret;
}


int EXPORT_FN Q3G_SystemCalls(int arg, ...)
{
	int args[13];
	va_list argptr;

	va_start(argptr, arg);
	args[0]=va_arg(argptr, int);
	args[1]=va_arg(argptr, int);
	args[2]=va_arg(argptr, int);
	args[3]=va_arg(argptr, int);
	args[4]=va_arg(argptr, int);
	args[5]=va_arg(argptr, int);
	args[6]=va_arg(argptr, int);
	args[7]=va_arg(argptr, int);
	args[8]=va_arg(argptr, int);
	args[9]=va_arg(argptr, int);
	args[10]=va_arg(argptr, int);
	args[11]=va_arg(argptr, int);
	args[12]=va_arg(argptr, int);
	va_end(argptr);

	return Q3G_SystemCallsEx(NULL, ~0, arg, args);
}

void SVQ3_ShutdownGame(void)
{
	int i;
	if (!q3gamevm)
		return;
#ifdef USEBOTLIB
	if (botlib)
	{	//it crashes otherwise, probably due to our huck clearage
		botlib->BotLibShutdown();
		Z_FreeTags(Z_TAG_BOTLIB);
	}
#endif

	for (i = 0; i < MAX_CONFIGSTRINGS; i++)
	{
		if (svq3_configstrings[i])
		{
			Z_Free(svq3_configstrings[i]);
			svq3_configstrings[i] = NULL;
		}
	}

	Z_Free(q3_sentities);
	q3_sentities = NULL;
	BZ_Free(q3_snapshot_entities);
	q3_snapshot_entities = NULL;

	VM_Destroy(q3gamevm);
	q3gamevm = NULL;

	Cvar_Set(Cvar_Get("sv_running", "0", 0, "Q3 compatability"), "0");
}

#ifdef USEBOTLIB
void VARGS BL_Print(int l, char *fmt, ...)
{
	va_list		argptr;
	char		text[1024];

	va_start (argptr, fmt);
	vsprintf (text, fmt, argptr);
	va_end (argptr);

	Con_Printf("%s", text);
}

int botlibmemoryavailable;
int BL_AvailableMemory(void)
{
	return botlibmemoryavailable;
}
void *BL_Malloc(int size)
{
	int *mem;
	botlibmemoryavailable-=size;

	mem = (int *)Z_TagMalloc(size+sizeof(int), Z_TAG_BOTLIB);
	mem[0] = size;

	return (void *)(mem + 1);
}
void BL_Free(void *mem)
{
	int *memref = ((int *)mem) - 1;
	botlibmemoryavailable+=memref[0];
	Z_Free(memref);
}
void *BL_HunkMalloc(int size)
{
	return BL_Malloc(size);//Hunk_AllocName(size, "botlib");
}

int BL_FOpenFile(const char *name, fileHandle_t *handle, fsMode_t mode)
{
	return VM_fopen((char*)name, (int*)handle, mode, Z_TAG_BOTLIB);
}
int BL_FRead( void *buffer, int len, fileHandle_t f )
{
	return VM_FRead(buffer, len, (int)f, Z_TAG_BOTLIB);
}
//int BL_FWrite( const void *buffer, int len, fileHandle_t f )
//{
//	return VM_FWrite(buffer, len, f, Z_TAG_BOTLIB);
//}	
void BL_FCloseFile( fileHandle_t f )
{
	VM_fclose((int)f, Z_TAG_BOTLIB);
}
//int BL_Seek( fileHandle_t f )
//{
//	VM_fseek(f, Z_TAG_BOTLIB)
//}
char *BL_BSPEntityData(void)
{
	return sv.worldmodel->entities;
}
void BL_Trace(bsp_trace_t *trace, vec3_t start, vec3_t mins, vec3_t maxs, vec3_t end, int passent, int contentmask)
{
	q3trace_t tr;
	SVQ3_Trace(&tr, start, mins, maxs, end, passent, contentmask);

	trace->allsolid = tr.allsolid;
	trace->startsolid = tr.startsolid;
	trace->fraction = tr.fraction;
	VectorCopy(tr.endpos, trace->endpos);
	trace->plane = tr.plane;
	trace->exp_dist = 0;
	trace->sidenum = 0;
	//trace->surface.name
	//trace->surface.flags
	trace->surface.value = tr.surfaceFlags;
	trace->contents = 0;//tr.contents;
	trace->ent = tr.entityNum;
}
int BL_PointContents(vec3_t point)
{
	return SVQ3_PointContents(point, -1);
}

int BL_inPVS(vec3_t p1, vec3_t p2)
{
	return true;// FIXME: :(
}

void BL_EntityTrace(bsp_trace_t *trace, vec3_t start, vec3_t mins, vec3_t maxs, vec3_t end, int entnum, int contentmask)
{
	trace->allsolid = 0;//tr.allsolid;
	trace->startsolid = 0;//tr.startsolid;
	trace->fraction = 1;//tr.fraction;
	VectorCopy(end, trace->endpos);
//	trace->plane = tr.plane;
	trace->exp_dist = 0;
	trace->sidenum = 0;
	//trace->surface.name
	//trace->surface.flags
//	trace->surface.value = tr.surfaceFlags;
	trace->contents = 0;//tr.contents;
//	trace->ent = tr.entityNum;
}

void BL_BSPModelMinsMaxsOrigin(int modelnum, vec3_t angles, vec3_t outmins, vec3_t outmaxs, vec3_t origin)
{
	model_t *mod;
	vec3_t mins, maxs;
	float max;
	int	i;

	mod = Mod_ForName(va("*%i", modelnum), false);
	VectorCopy(mod->mins, mins);
	VectorCopy(mod->maxs, maxs);

	//if the model is rotated
	if ((angles[0] || angles[1] || angles[2]))
	{
		// expand for rotation
		max = RadiusFromBounds(mins, maxs);
		for (i = 0; i < 3; i++)
		{
			mins[i] = -max;
			maxs[i] = max;
		}
	}
	if (outmins)
		VectorCopy(mins, outmins);
	if (outmaxs)
		VectorCopy(maxs, outmaxs);
	if (origin)
		VectorClear(origin);
}
void BL_BotClientCommand(int clientnum, char *command)
{
	Cmd_TokenizeString(command, false, false);
	VM_Call(q3gamevm, GAME_CLIENT_COMMAND, clientnum);
}

#endif

void SV_InitBotLib()
{
	cvar_t *bot_enable = Cvar_Get("bot_enable", "1", 0, "Q3 compatability");

#ifdef USEBOTLIB
	botlib_import_t import;

	Cvar_Set(Cvar_Get("sv_mapChecksum", "0", 0, "Q3 compatability"), va("%i", sv.worldmodel->checksum));

	memset(&import, 0, sizeof(import));
	import.Print = BL_Print;
	import.Trace = BL_Trace;
	import.EntityTrace = BL_EntityTrace;
	import.PointContents = BL_PointContents;
	import.inPVS = BL_inPVS;
	import.BSPEntityData = BL_BSPEntityData;
	import.BSPModelMinsMaxsOrigin = BL_BSPModelMinsMaxsOrigin;
	import.BotClientCommand = BL_BotClientCommand;
	import.GetMemory = BL_Malloc;
	import.FreeMemory = BL_Free;
	import.AvailableMemory = BL_AvailableMemory;
	import.HunkAlloc = BL_HunkMalloc;
	import.FS_FOpenFile = BL_FOpenFile;
	import.FS_Read = BL_FRead;
//	import.FS_Write = BL_FWrite;
	import.FS_FCloseFile = BL_FCloseFile;
//	import.FS_Seek = BL_Seek;
//	import.DebugLineCreate
//	import.DebugLineDelete
//	import.DebugLineShow
//
//	import.DebugPolygonCreate
//	import.DebugPolygonDelete

//	Z_FreeTags(Z_TAG_BOTLIB);
	botlibmemoryavailable = 1024*1024*16;
	if (bot_enable->value)
		botlib = FTE_GetBotLibAPI(BOTLIB_API_VERSION, &import);
	else
		botlib = NULL;
	if (!botlib)
	{
		bot_enable->flags |= CVAR_LATCH;
		Cvar_ForceSet(bot_enable, "0");
	}
	else
	{
		cvar_t *mapname = Cvar_Get("mapname", "", CVAR_SERVERINFO, "Q3 compatability");
		Cvar_Set(mapname, sv.name);
	}
#else

//make sure it's switched off.
	Cvar_ForceSet(bot_enable, "0");
	bot_enable->flags |= CVAR_NOSET;
#endif
}

qboolean SVQ3_InitGame(void)
{
	char buffer[8192];
	char *str;
	char sysinfo[8192];
	extern cvar_t progs;
	cvar_t *sv_pure;

	if (sv.worldmodel->type == mod_heightmap)
	{
	}
	else
	{
		if (sv.worldmodel->fromgame == fg_quake || sv.worldmodel->fromgame == fg_quake2)
			return false;	//always fail on q1bsp
	}

	if (*progs.string)	//don't load q3 gamecode if we're explicitally told to load a progs.
		return false;


	SVQ3_ShutdownGame();

	q3gamevm = VM_Create(NULL, "vm/qagame", Q3G_SystemCalls, Q3G_SystemCallsEx);

	if (!q3gamevm)
		return false;

	SV_InitBotLib();

	SV_ClearWorld();

	q3_sentities = Z_Malloc(sizeof(q3serverEntity_t)*MAX_GENTITIES);

	strcpy(buffer, svs.info);
	Info_SetValueForKey(buffer, "map", "", sizeof(buffer)); 
	Info_SetValueForKey(buffer, "maxclients", "", sizeof(buffer)); 
	Info_SetValueForKey(buffer, "mapname", sv.name, sizeof(buffer)); 
	Info_SetValueForKey(buffer, "sv_maxclients", "32", sizeof(buffer)); 
	SVQ3_SetConfigString(0, buffer);

	Cvar_Set(Cvar_Get("sv_running", "0", 0, "Q3 compatability"), "1");

	sysinfo[0] = '\0';
	Info_SetValueForKey(sysinfo, "sv_serverid", va("%i", svs.spawncount), MAX_SERVERINFO_STRING); 
	
	str = FS_GetPackHashes(buffer, sizeof(buffer), false);
	Info_SetValueForKey(sysinfo, "sv_paks", str, MAX_SERVERINFO_STRING);

	str = FS_GetPackNames(buffer, sizeof(buffer), false);
	Info_SetValueForKey(sysinfo, "sv_pakNames", str, MAX_SERVERINFO_STRING);
	
	str = FS_GetPackHashes(buffer, sizeof(buffer), true);
	Info_SetValueForKey(sysinfo, "sv_referencedPaks", str, MAX_SERVERINFO_STRING);
	
	str = FS_GetPackNames(buffer, sizeof(buffer), true);
	Info_SetValueForKey(sysinfo, "sv_referencedPakNames", str, MAX_SERVERINFO_STRING);

	sv_pure = Cvar_Get("sv_pure", "1", 0, "Q3 compatability");
	Info_SetValueForKey(sysinfo, "sv_pure", sv_pure->string, MAX_SERVERINFO_STRING);

	SVQ3_SetConfigString(1, sysinfo);


	mapentspointer = sv.worldmodel->entities;
	VM_Call(q3gamevm, GAME_INIT, 0, (int)rand(), false);

	CM_InitBoxHull();

	SVQ3_CreateBaseline();

	q3_num_snapshot_entities = 32 * Q3UPDATE_BACKUP * 32;
	if (q3_snapshot_entities)
		BZ_Free(q3_snapshot_entities);
	q3_next_snapshot_entities = 0;
	q3_snapshot_entities = BZ_Malloc(sizeof( q3entityState_t ) * q3_num_snapshot_entities);

#ifdef USEBOTLIB
	if (botlib)
		VM_Call(q3gamevm, BOTAI_START_FRAME, (int)(sv.time*1000));
#endif

	return true;
}

void SVQ3_RunFrame(void)
{
	VM_Call(q3gamevm, GAME_RUN_FRAME, (int)(sv.time*1000));
#ifdef USEBOTLIB
	if (botlib)
		VM_Call(q3gamevm, BOTAI_START_FRAME, (int)(sv.time*1000));
#endif
}

void SVQ3_ClientCommand(client_t *cl)
{
	VM_Call(q3gamevm, GAME_CLIENT_COMMAND, (int)(cl-svs.clients));
}

void SVQ3_ClientBegin(client_t *cl)
{
	VM_Call(q3gamevm, GAME_CLIENT_BEGIN, (int)(cl-svs.clients));
}

void SVQ3_ClientThink(client_t *cl)
{
	VM_Call(q3gamevm, GAME_CLIENT_THINK, (int)(cl-svs.clients));
}

qboolean SVQ3_Command(void)
{
	if (!q3gamevm)
		return false;

	return VM_Call(q3gamevm, GAME_CONSOLE_COMMAND);
}

qboolean SVQ3_ConsoleCommand(void)
{
	if (!q3gamevm)
		return false;
	Cmd_ShiftArgs(1, false);
	VM_Call(q3gamevm, GAME_CONSOLE_COMMAND);
	return true;
}

void SVQ3_Netchan_Transmit( client_t *client, int length, qbyte *data );

void SVQ3_CreateBaseline(void)
{
	q3sharedEntity_t	*ent;
	int				entnum;	

	if (q3_baselines)
		Z_Free(q3_baselines);

	q3_baselines = Z_Malloc(sizeof(q3entityState_t)*MAX_GENTITIES);
	for(entnum=0; entnum<numq3entities; entnum++)
	{
		ent = GENTITY_FOR_NUM(entnum);

		if(!ent->r.linked)
			continue;

		// FIXME - is this check correct?
		if(ent->r.svFlags & (SVF_NOCLIENT|/*SVF_CLIENTMASK|*/SVF_SINGLECLIENT))
			continue;

		if (ent->s.number < 0)
			continue;	//hey!

		//
		// take current state as baseline
		//
		memcpy(&q3_baselines[entnum], &ent->s, sizeof(q3_baselines[0]));
	}
}

//Writes the entities to the clients
void SVQ3_EmitPacketEntities(client_t *client, q3client_frame_t *from, q3client_frame_t *to, sizebuf_t *msg)
{
	q3entityState_t	*oldent = {0}, *newent = {0};
	int				oldindex, newindex;
	int				oldnum, newnum;
	int				from_num_entities;

	if(!from )
	{
		from_num_entities = 0;
	}
	else
	{
		from_num_entities = from->num_entities;
	}

	newindex = 0;
	oldindex = 0;
	while(newindex < to->num_entities || oldindex < from_num_entities)
	{
		if(newindex >= to->num_entities)
		{
			newent = NULL;
			newnum = 99999;
		}
		else
		{
			newent = &q3_snapshot_entities[(to->first_entity + newindex) % q3_num_snapshot_entities];
			newnum = newent->number;
		}

		if(oldindex >= from_num_entities)
		{
			oldent = NULL;
			oldnum = 99999;
		}
		else
		{
			oldent = &q3_snapshot_entities[(from->first_entity + oldindex) % q3_num_snapshot_entities];
			oldnum = oldent->number;
		}

		if(newnum == oldnum)
		{
			// delta update from old position
			// because the force parm is false, this will not result
			// in any bytes being emited if the entity has not changed at all
			MSGQ3_WriteDeltaEntity(msg, oldent, newent, false);
			oldindex++;
			newindex++;
			continue;
		}

		if(newnum < oldnum)
		{
			// this is a new entity, send it from the baseline
			MSGQ3_WriteDeltaEntity( msg, &q3_baselines[newnum], newent, true );
			newindex++;
			continue;
		}

		if(newnum > oldnum)
		{
			// the old entity isn't present in the new message
			MSGQ3_WriteDeltaEntity( msg, oldent, NULL, true );
			oldindex++;
			continue;
		}
	}
	
	MSG_WriteBits(msg, ENTITYNUM_NONE, GENTITYNUM_BITS); // end of packetentities
}

void SVQ3_WriteSnapshotToClient(client_t *client, sizebuf_t *msg)
{
	q3client_frame_t	*oldsnap;
	q3client_frame_t	*snap;
	int				delta;
	int				i;

	// this is a frame we are creating
	snap = &client->frameunion.q3frames[client->netchan.outgoing_sequence & Q3UPDATE_MASK];

	if(client->state < cs_spawned)
	{
		// not fully in game yet
		delta = 0;
		oldsnap = NULL;
		return;
	}
	else if(client->delta_sequence < 0)
	{
		// client is asking for a retransmit
		delta = 0;
		oldsnap = NULL;
	}
	else if(client->netchan.outgoing_sequence - client->delta_sequence >= Q3UPDATE_BACKUP - 3)
	{
		// client hasn't gotten a good message through in a long time
		Con_DPrintf( "%s: Delta request from out of date packet.\n", client->name );
		delta = 0;
		oldsnap = NULL;
	}
	else
	{
		// we have a valid message to delta from
		delta = client->netchan.outgoing_sequence - client->delta_sequence;
		oldsnap = &client->frameunion.q3frames[client->delta_sequence & Q3UPDATE_MASK];

		if(oldsnap->first_entity <= q3_next_snapshot_entities - q3_num_snapshot_entities)
		{
			// oldsnap entities are too old
			Con_DPrintf("%s: Delta request from out of date entities.\n", client->name);
			delta = 0;
			oldsnap = NULL;
		}
	}

//	if( client->surpressCount ) {
//		snap->snapFlags |= SNAPFLAG_RATE_DELAYED;
//		client->surpressCount = 0;
//	}
	
	// write snapshot header
	MSG_WriteBits(msg, svcq3_snapshot, 8);
	MSG_WriteBits(msg, (int)(sv.time*1000), 32);
	MSG_WriteBits(msg, delta, 8); // what we are delta'ing from
	
	// write snapFlags
	MSG_WriteBits(msg, snap->flags, 8);

	// send over the areabits
	MSG_WriteBits(msg, snap->areabytes, 8);
	for (i = 0; i < snap->areabytes; i++)
		MSG_WriteBits(msg, snap->areabits[i], 8);

	// delta encode the playerstate
	MSGQ3_WriteDeltaPlayerstate(msg, oldsnap ? &oldsnap->ps : NULL, &snap->ps);

	// delta encode the entities
	SVQ3_EmitPacketEntities(client, oldsnap, snap, msg);

//	while( msg.cursize < sv_padPackets->integer ) { // FIXME?
//	for( i=0 ; i<sv_padPackets->integer ; i++ )
//	{
//		MSG_WriteByte( msg, svcq3_nop );
//	}
}


int clientNum;
int clientarea;
qbyte *areabits;
qbyte		*bitvector;

static int VARGS SVQ3_QsortEntityStates( const void *arg1, const void *arg2 ) {
	const q3entityState_t *s1 = *(const q3entityState_t **)arg1;
	const q3entityState_t *s2 = *(const q3entityState_t **)arg2;

	if( s1->number > s2->number ) {
		return 1;
	}

	if( s1->number < s2->number ) {
		return -1;
	}

	SV_Error("SV_QsortEntityStates: duplicated entity");

	return 0;
	
}

static qboolean SVQ3_EntityIsVisible( q3sharedEntity_t *ent ) {
	q3serverEntity_t *sent;
	int i;
	int l;

	if( !ent->r.linked ) {
		return false; // not active entity
	}

	if( ent->r.svFlags & SVF_NOCLIENT ) {
		return false; // set to invisible
	}

	if( ent->r.svFlags & SVF_CLIENTMASK ) {
		if( clientNum > 32 ) {
			SV_Error("SVF_CLIENTMASK: clientNum > 32" );
		}
		if( ent->r.singleClient & (1 << (clientNum & 7)) ) {
			return true;
		}
		return false;
	}

	if( ent->r.svFlags & SVF_SINGLECLIENT ) {
		if( ent->r.singleClient == clientNum ) {
			return true;
		}
		return false;
	}

	if( ent->r.svFlags & SVF_NOTSINGLECLIENT ) {
		if( ent->r.singleClient == clientNum ) {
			return false;
		}
		// FIXME: fall through
	}

	if( ent->r.svFlags & SVF_BROADCAST ) {
		return true;
	}

	//
	// ignore if not touching a PV leaf
	//
	sent = SENTITY_FOR_GENTITY( ent );
	
	// check area
	if( sent->areanum < 0 || !(areabits[sent->areanum >> 3] & (1 << (sent->areanum & 7))) ) {
		// doors can legally straddle two areas, so
		// we may need to check another one
		if( sent->areanum2 < 0 || !(areabits[sent->areanum2 >> 3] & (1 << (sent->areanum2 & 7))) ) {
			return false;		// blocked by a door
		}
	}

/*
	// check area
	if( !CM_AreasConnected( clientarea, sent->areanum ) ) {
		// doors can legally straddle two areas, so
		// we may need to check another one
		if( !CM_AreasConnected( clientarea, sent->areanum2 ) ) {
			return false;		// blocked by a door
		}
	}
*/

			
	if( sent->num_clusters == -1 ) {
		// too many leafs for individual check, go by headnode
		if( !CM_HeadnodeVisible(sv.worldmodel, sent->headnode, bitvector ) ) {
			return false;
		}
	} else {
		// check individual leafs
		for( i=0 ; i < sent->num_clusters ; i++ ) {
			l = sent->clusternums[i];
			if( bitvector[l >> 3] & (1 << (l & 7) ) ) {
				break;
			}
		}
		if( i == sent->num_clusters ) {
			return false;		// not visible
		}
	}
	
	return true;
}

void SVQ3_BuildClientSnapshot( client_t *client )
{
	q3entityState_t		*entityStates[MAX_ENTITIES_IN_SNAPSHOT];
	vec3_t				org;
	q3sharedEntity_t		*ent;
	q3sharedEntity_t		*clent;
	q3client_frame_t	*snap;
	q3entityState_t		*es;
	q3playerState_t		*ps;
	int					portalarea;
	int					i;

	clientNum = client - svs.clients;
	clent = GENTITY_FOR_NUM( clientNum );
	ps = PS_FOR_NUM( clientNum );

	// this is the frame we are creating
	snap = &client->frameunion.q3frames[client->netchan.outgoing_sequence & Q3UPDATE_MASK];

	snap->serverTime = Sys_DoubleTime()*1000;//svs.levelTime; // save it for ping calc later
	snap->flags = 0;

	if( client->state < cs_spawned )
	{
		// not in game yet
		memcpy(&snap->ps, ps, sizeof(snap->ps));
		snap->flags |= SNAPFLAG_NOT_ACTIVE;
		snap->areabytes = 1;
		snap->areabits[0] = 0;
		snap->num_entities = 0;
		snap->first_entity = q3_next_snapshot_entities;
		return;
	}

	// find the client's PVS
	VectorCopy( ps->origin, org );
	org[2] += ps->viewheight;

	clientarea = CM_PointLeafnum(sv.worldmodel, org);
	bitvector = sv.worldmodel->funcs.LeafPVS(sv.worldmodel, sv.worldmodel->funcs.LeafnumForPoint(sv.worldmodel, org), NULL);
	clientarea = CM_LeafArea(sv.worldmodel, clientarea);
/*
	if (client->areanum != clientarea)
	{
		Com_Printf( "%s entered area %i\n", client->name, clientarea);
		client->areanum = clientarea;
	}
*/

	// calculate the visible areas
	areabits = snap->areabits;
	snap->areabytes = CM_WriteAreaBits(sv.worldmodel, areabits, clientarea);

	// grab the current playerState_t
	memcpy( &snap->ps, ps, sizeof( snap->ps ) );

	// build up the list of visible entities
	snap->num_entities = 0;
	snap->first_entity = q3_next_snapshot_entities;

	// check for SVF_PORTAL entities first
	for( i=0 ; i<numq3entities ; i++ )
	{
		ent = GENTITY_FOR_NUM( i );

		if( ent == clent )
		{
			continue;
		}

		if( !(ent->r.svFlags & SVF_PORTAL) )
		{
			continue;
		}

		if( !SVQ3_EntityIsVisible( ent ) )
		{
			continue;
		}

		// merge PVS if portal 
		portalarea = CM_PointLeafnum(sv.worldmodel, ent->s.origin2);
		portalarea = CM_LeafArea(sv.worldmodel, portalarea);

//		CM_MergePVS ( ent->s.origin2 );

//		CM_MergeAreaBits( snap->areabits, portalarea );
	}

	// add all visible entities
	for( i=0 ; i<numq3entities ; i++ )
	{
		ent = GENTITY_FOR_NUM( i );

		if( ent == clent ) {
			continue;
		}

		if( !SVQ3_EntityIsVisible( ent ) )
		{
			continue;
		}
		
		if( ent->s.number != i ) {
			Con_DPrintf( "FIXING ENT->S.NUMBER!!!\n" );
			ent->s.number = i;
		}

		entityStates[snap->num_entities++] = &ent->s;

		if( snap->num_entities >= MAX_ENTITIES_IN_SNAPSHOT )
		{
			Con_DPrintf( "MAX_ENTITIES_IN_SNAPSHOT\n" );
			break;
		}
	
	}

	if( q3_next_snapshot_entities + snap->num_entities >= 0x7FFFFFFE )
	{
		SV_Error("q3_next_snapshot_entities wrapped");
	}

	// find duplicated entities
	qsort( entityStates, snap->num_entities, sizeof( entityStates[0] ), SVQ3_QsortEntityStates );

	// add them to the circular snapshotEntities array
	for( i=0 ; i<snap->num_entities ; i++ )
	{
		es = &q3_snapshot_entities[q3_next_snapshot_entities % q3_num_snapshot_entities];
		memcpy( es, entityStates[i], sizeof( *es ) );

		q3_next_snapshot_entities++;
	}

	
	
	for (i = 0; i < snap->areabytes;i++)
	{	//fix areabits, q2->q3 style..
		snap->areabits[i]^=255;
	}
	
}


//writes initial gamestate
void SVQ3_SendGameState(client_t *client)
{
	sizebuf_t		msg;
	unsigned char			buffer[MAX_OVERALLMSGLEN];
	int				i;
	int				j;
	char			*configString;

	Con_DPrintf( "SV_SendClientGameState() for %s\n", client->name );

	memset(&msg, 0, sizeof(msg));
	msg.maxsize =  sizeof(buffer);
	msg.data = buffer;
	msg.packing = SZ_HUFFMAN;

	// write last clientCommand number we have processed
	MSG_WriteBits(&msg, client->last_client_command_num, 32);

	MSG_WriteBits(&msg, svcq3_gamestate, 8 );
	MSG_WriteBits(&msg, client->num_client_commands, 32);

	// write configstrings
	for( i=0; i<MAX_CONFIGSTRINGS; i++ )
	{
		configString = svq3_configstrings[i];
		if( !configString )
			continue;

		MSG_WriteBits( &msg, svcq3_configstring, 8);
		MSG_WriteBits( &msg, i, 16 );
		for (j = 0; configString[j]; j++)
			MSG_WriteBits(&msg, configString[j], 8);
		MSG_WriteBits(&msg, 0, 8);
	}

	// write baselines
	for( i=0; i<MAX_GENTITIES; i++ )
	{
		if(i && !q3_baselines[i].number)
			continue; // FIXME: is this correct?

		MSG_WriteBits(&msg, svcq3_baseline, 8);
		MSGQ3_WriteDeltaEntity( &msg, NULL, &q3_baselines[i], true );
	}

	// write svc_eom command
	MSG_WriteBits(&msg, svcq3_eom, 8);

	MSG_WriteBits(&msg, client - svs.clients, 32);
	MSG_WriteBits(&msg, fs_key, 32);

	// end of message marker
	MSG_WriteBits(&msg, svcq3_eom, 8);

	// send the datagram
	SVQ3_Netchan_Transmit( client, msg.cursize, msg.data );

	// calculate client->sendTime
//	SV_RateDrop( client, msg.cursize );

	client->state = cs_connected;
	client->gamestatesequence = client->last_sequence;
}

void SVQ3_WriteServerCommandsToClient( client_t *client, sizebuf_t *msg )
{
	int	i;
	int j, len;
	char *str;

	for(i=client->last_server_command_num+1; i<=client->num_server_commands; i++)
	{
		MSG_WriteBits(msg, svcq3_serverCommand, 8);
		MSG_WriteBits(msg, i, 32);
		str = client->server_commands[i & TEXTCMD_MASK];
		len = strlen(str);
		for (j = 0; j <= len; j++)
			MSG_WriteBits(msg, str[j], 8);
	}
}

void SVQ3_SendMessage(client_t *client)
{
	qbyte		buffer[MAX_OVERALLMSGLEN];
	sizebuf_t	msg;
	memset(&msg, 0, sizeof(msg));
	msg.maxsize =  sizeof(buffer);
	msg.data = buffer;
	msg.packing = SZ_HUFFMAN;

	SVQ3_BuildClientSnapshot( client );

	MSG_WriteBits(&msg, client->last_client_command_num, 32);

	// write pending serverCommands
	SVQ3_WriteServerCommandsToClient(client, &msg);

	// send over all the relevant entityState_t
	// and the playerState_t
	SVQ3_WriteSnapshotToClient( client, &msg );

	// SV_WriteDownloadToClient( client, &msg );

	// end of message marker
	MSG_WriteBits(&msg, svcq3_eom, 8);

	SVQ3_Netchan_Transmit( client, msg.cursize, msg.data );
}




client_t *SVQ3_FindEmptyPlayerSlot(void)
{
	int i;
	for (i = 0; i < MAX_CLIENTS; i++)
	{
		if (!svs.clients[i].state)
			return &svs.clients[i];
	}
	return NULL;
}
client_t *SVQ3_FindExistingPlayerByIP(netadr_t na, int qport)
{
	int i;
	for (i = 0; i < MAX_CLIENTS; i++)
	{
		if (svs.clients[i].state && NET_CompareAdr(svs.clients[i].netchan.remote_address, na))
			return &svs.clients[i];
	}
	return NULL;
}

qboolean Netchan_ProcessQ3 (netchan_t *chan);
static qboolean SVQ3_Netchan_Process(client_t *client)
{
	int		serverid;
	int		lastSequence;
	int		lastServerCommandNum;
	qbyte	bitmask;
	qbyte	c;
	int		i, j;
	char	*string;
	int		bit;
	int		readcount;

	if (!Netchan_ProcessQ3(&client->netchan))
	{
		return false;
	}

	// archive buffer state
	bit = net_message.currentbit;
	readcount = msg_readcount;
	net_message.packing = SZ_HUFFMAN;

	serverid = MSG_ReadBits(32);
	lastSequence = MSG_ReadBits(32);
	lastServerCommandNum = MSG_ReadBits(32);

	// restore buffer state
	net_message.currentbit = bit;
	msg_readcount = readcount;
	net_message.packing = SZ_RAWBYTES;

	// calculate bitmask
	bitmask = serverid ^ lastSequence ^ client->challenge;
	string = client->server_commands[lastServerCommandNum & TEXTCMD_MASK];

#ifndef Q3_NOENCRYPT
	// decrypt the packet
	for( i=msg_readcount+12,j=0 ; i<net_message.cursize ; i++,j++ )
	{
		if( !string[j] )
		{
			j = 0; // another way around
		}
		c = string[j];
		if( c > 127 || c == '%' )
		{
			c = '.';
		}
		bitmask ^= c << (i & 1);
		net_message.data[i] ^= bitmask;
	}
#endif

	return true;
}

void SVQ3_Netchan_Transmit( client_t *client, int length, qbyte *data )
{
	qbyte		buffer[MAX_OVERALLMSGLEN];
	qbyte		bitmask;
	qbyte		c;
	int			i, j;
	char		*string;

	// calculate bitmask
	bitmask = client->netchan.outgoing_sequence ^ client->challenge;
	string = client->last_client_command;

#ifndef Q3_NOENCRYPT
	//first four bytes are not encrypted.
	for( i=0; i<4 ; i++)
		buffer[i] = data[i];
	// encrypt the packet
	for( j=0 ; i<length ; i++, j++ )
	{
		if( !string[j] )
		{
			j = 0; // another way around
		}
		c = string[j];
		if( c > 127 || c == '%' ) {
			c = '.';
		}
		bitmask ^= c << (i & 1);
		buffer[i] = data[i]^bitmask;
	}
#else
	for( i=0 ; i<length ; i++ )
		buffer[i] = data[i];
#endif

	// deliver the message
	Netchan_TransmitQ3( &client->netchan, length, buffer);
}

int StringKey( const char *string, int length );
#define MAX_PACKET_USERCMDS 64
void SVQ3_ParseUsercmd(client_t *client, qboolean delta)
{
	static usercmd_t	nullcmd;
	usercmd_t			commands[MAX_PACKET_USERCMDS];
	usercmd_t			*from;
	usercmd_t			*to;
	int					i;
	int					key;
	int					cmdCount;
	char *string;
	
	if( delta )
	{
		client->delta_sequence = client->last_sequence;
//		client->snapLatency[client->last_sequence & (LATENCY_COUNTS-1)] = Sys_Milliseconds()/*svs.levelTime*/ - client->snapshots[client->last_sequence & UPDATE_MASK].serverTime;
	}
	else
	{
		client->delta_sequence = -1; // client is asking for retransmit
//		client->snapLatency[client->last_sequence & (LATENCY_COUNTS-1)] = -1;
	}
	
	// read number of usercmds in a packet
	cmdCount = MSG_ReadBits(8);
	if(cmdCount < 1)
		SV_DropClient(client);
	else if(cmdCount > MAX_PACKET_USERCMDS)
		SV_DropClient(client);

	if(client->state <= cs_zombie)
		return; // was dropped

	// calculate key for usercmd decryption
	string = client->server_commands[client->last_server_command_num & TEXTCMD_MASK];
	key = client->last_sequence ^ fs_key ^ StringKey(string, 32);

	// read delta sequenced usercmds
	from = &nullcmd;
	for(i=0, to=commands; i<cmdCount; i++, to++)
	{
		MSG_Q3_ReadDeltaUsercmd(key, from, to);
		from = to;
	}

	switch(client->state)
	{
	case cs_connected:
		// transition from CS_PRIMED to CS_ACTIVE
		memcpy(&client->lastcmd, &commands[cmdCount-1], sizeof(client->lastcmd));
		SVQ3_ClientBegin(client);
		client->state = cs_spawned;
		break;
	case cs_spawned:
		// run G_ClientThink() on each usercmd
		for(i=0,to=commands; i<cmdCount; i++, to++)
		{
//			if(to->servertime <= client->lastcmd.servertime )
//				continue;

			memcpy( &client->lastcmd, to, sizeof(client->lastcmd));
			SVQ3_ClientThink(client);
		}
		break;
	default:
		break; // outdated usercmd packet
	}

}

void SVQ3_UpdateUserinfo_f(client_t *cl)
{
	Q_strncpyz( cl->userinfo, Cmd_Argv(1), sizeof(cl->userinfo) );

	SV_ExtractFromUserinfo (cl);

	VM_Call(q3gamevm, GAME_CLIENT_USERINFO_CHANGED, (int)(cl-svs.clients));
}

void SVQ3_Drop_f(client_t *cl)
{
	SV_DropClient(cl);
}

typedef struct ucmd_s {
	char	*name;
	void	(*func)( client_t * );
} ucmd_t;

static const ucmd_t ucmds[] = {
	{ "userinfo",		SVQ3_UpdateUserinfo_f},
	{ "disconnect",		SVQ3_Drop_f},//SV_Disconnect_f },

	// TODO
	{ "cp",				NULL },
	{ "download",		NULL },
	{ "nextdl",			NULL },
	{ "stopdl",			NULL },
	{ "donedl",			NULL },

	{ NULL,				NULL }
};
void SVQ3_ParseClientCommand(client_t *client)
{
	int commandNum;
	char *command;
	const ucmd_t	*u;
	char buffer[2048];
	int i;

	commandNum = MSG_ReadBits(32);
	for (i = 0; ; i++)
	{
		buffer[i] = MSG_ReadBits(8);
		if (!buffer[i])
			break;
	}
	command = buffer;

	if(commandNum <= client->last_client_command_num)
		return; // we have already received this command

//	Con_Printf("ClientCommand %i: %s\n", commandNum, buffer);

//	Con_DPrintf("clientCommand: %s : %i : %s\n", client->name, commandNum, Com_TranslateLinefeeds(command));

	client->last_client_command_num++;

	if(commandNum > client->last_client_command_num)
	{
		Con_Printf("Client %s lost %i clientCommands\n", commandNum - client->last_client_command_num);
		SV_DropClient(client);
		return;
	}

	// copy current command for netchan encryption
	Q_strncpyz(client->last_client_command, command, sizeof(client->last_client_command));

	Cmd_TokenizeString(command, false, false);

	// check for server private commands first
	for(u=ucmds; u->name; u++)
	{
		if(!stricmp(Cmd_Argv(0), u->name))
		{
			if(u->func)
				u->func(client);
			break;
		}
	}

	// TODO - flood protection

	if(!u->name && sv.state == ss_active)
		SVQ3_ClientCommand(client);
}

void SVQ3_ParseClientMessage(client_t *client)
{
	int serverid;	//sorta like the level number.
	int c;

	host_client = client;

	// remaining data is compressed
	net_message.packing = SZ_HUFFMAN;
	net_message.currentbit = msg_readcount*8;

	// read serverid
	serverid = MSG_ReadBits(32);

	// read last server message sequence client received
	client->last_sequence = MSG_ReadBits(32);
	if( client->last_sequence < 0 )
	{
		return; // this shouldn't happen
	}

	// read last server command number client received
	client->last_server_command_num = MSG_ReadBits(32);
	if( client->last_server_command_num <= client->num_server_commands - TEXTCMD_BACKUP )
		client->last_server_command_num = client->num_server_commands - TEXTCMD_BACKUP + 1;
	else if( client->last_server_command_num > client->num_server_commands )
		client->last_server_command_num = client->num_server_commands;

	// check if message is from a previous level
	if( serverid != svs.spawncount )
	{
		if(client->gamestatesequence>=0)
		{
			if( client->last_sequence - client->gamestatesequence < 100 )
				return; // don't resend gameState too frequently

			Con_DPrintf( "%s : dropped gamestate, resending\n", client->name );
		}
		SVQ3_SendGameState( client );
		return;
	}

	client->send_message = true;


	//
	// parse the message
	//
	while(1)
	{
		if(client->state <= cs_zombie)
			return; // parsed command caused client to disconnect

		if(msg_readcount > net_message.cursize)
		{
			Con_Printf("corrupted packet from %s\n", client->name);
			client->drop = true;
			return;
		}

		c = MSG_ReadBits(8);
		if (c == clcq3_eom)
		{
			break;
		}
				
		switch(c)
		{
		default:
			Con_Printf("corrupted packet from %s\n", client->name);
			client->drop = true;
			return;
		case clcq3_nop:
			break;
		case clcq3_move:
			SVQ3_ParseUsercmd(client, true);
			break;
		case clcq3_nodeltaMove:
			SVQ3_ParseUsercmd(client, false);
			break;
		case clcq3_clientCommand:
			SVQ3_ParseClientCommand(client);
			break;
		}
	}

	if (msg_readcount != net_message.cursize)
	{
		Con_Printf(CON_WARNING "WARNING: Junk at end of packet for client %s\n", client->name );
	}
};
void SVQ3_HandleClient(void)
{
	int i;
	int qport;

	if (net_message.cursize<6)
		return;	//urm. :/

	MSG_BeginReading();
	MSG_ReadBits(32);
	qport = (unsigned short)MSG_ReadBits(16);

	for (i = 0; i < MAX_CLIENTS; i++)
	{
		if (svs.clients[i].state <= cs_zombie)
			continue;
		if (svs.clients[i].netchan.qport != qport)
			continue;
		if (!NET_CompareBaseAdr(svs.clients[i].netchan.remote_address, net_from))
			continue;

		//found them.
		break;
	}
	if (i == MAX_CLIENTS)
		return;	//nope

	if (!SVQ3_Netchan_Process(&svs.clients[i]))
	{
		return; // wasn't accepted for some reason
	}

	SVQ3_ParseClientMessage(&svs.clients[i]);
}

bannedips_t *SV_BannedAddress (netadr_t *a);
void SVQ3_DirectConnect(void)	//Actually connect the client, use up a slot, and let the gamecode know of it.
{
	char *reason;
	client_t *cl;
	char *userinfo = NULL;
	int ret;
	int challenge;
	int qport;
	bannedips_t *banip;
	char adr[MAX_ADR_SIZE];

	if (net_message.cursize < 13)
		return;
	Huff_DecryptPacket(&net_message, 12);


	Cmd_TokenizeString((char*)net_message.data+4, false, false);
	userinfo = Cmd_Argv(1);
	qport = atoi(Info_ValueForKey(userinfo, "qport"));
	challenge = atoi(Info_ValueForKey(userinfo, "challenge"));

	cl = SVQ3_FindExistingPlayerByIP(net_from, qport);	//use a duplicate first.
	if (!cl)
		cl = SVQ3_FindEmptyPlayerSlot();

	banip = SV_BannedAddress(&net_from);

	if (banip)
	{
		if (banip->reason[0])
			reason = banip->reason;
		else
			reason = "Banned.";
		userinfo = NULL;
	}
	else if (!cl)
	{
		reason = "Server is full.";
		userinfo = NULL;
	}
	else
	{
		if (cl->frameunion.q3frames)
			BZ_Free(cl->frameunion.q3frames);
		memset(cl, 0, sizeof(*cl));
		challenge = atoi(Info_ValueForKey(userinfo, "challenge"));

		if (net_from.type != NA_LOOPBACK && !SV_ChallengePasses(challenge))
			reason = "Invalid challenge";
		else
		{
#ifndef SERVERONLY
			if (net_from.type == NA_LOOPBACK)
				cls.challenge = challenge = 500;
#endif
			Q_strncpyz(cl->userinfo, userinfo, sizeof(cl->userinfo));
			reason = NET_AdrToString(adr, sizeof(adr), net_from);
			Info_SetValueForStarKey(cl->userinfo, "ip", reason, sizeof(cl->userinfo));

			ret = VM_Call(q3gamevm, GAME_CLIENT_CONNECT, (int)(cl-svs.clients), false, false);
			if (!ret)
				reason = NULL;
			else
				reason = (char*)VM_MemoryBase(q3gamevm)+ret;	//this is going to stop q3 dll gamecode at 64bits.
		}
	}

	if (reason)
	{
		Con_Printf("%s\n", reason);
		reason = va("\377\377\377\377print\n%s", reason);
		NET_SendPacket (NS_SERVER, strlen(reason), reason, net_from);
		return;
	}

	cl->protocol = SCP_QUAKE3;
	cl->state = cs_connected;
	cl->name = cl->namebuf;
	cl->team = cl->teambuf;
	SV_ExtractFromUserinfo(cl);
	Netchan_Setup(NS_SERVER, &cl->netchan, net_from, qport);
	cl->netchan.outgoing_sequence = 1;

	cl->challenge = challenge;
	cl->userid = (cl - svs.clients)+1;

	cl->gamestatesequence = -1;

	NET_SendPacket (NS_SERVER, 19, "\377\377\377\377connectResponse", net_from);

	Huff_PreferedCompressionCRC();

	cl->frameunion.q3frames = BZ_Malloc(Q3UPDATE_BACKUP*sizeof(*cl->frameunion.q3frames));
}

int SVQ3_AddBot(void)
{
	client_t *cl;

	cl = SVQ3_FindEmptyPlayerSlot();
	if (!cl)
		return -1;	//failure, no slots

	cl->protocol = SCP_BAD;
	cl->state = cs_connected;
	cl->name = cl->namebuf;
	cl->team = cl->teambuf;

	cl->challenge = 0;
	cl->userid = (cl - svs.clients)+1;
	cl->state = cs_spawned;

	return cl - svs.clients;
}

void SVQ3_DropClient(client_t *cl)
{
	if (q3gamevm)
		VM_Call(q3gamevm, GAME_CLIENT_DISCONNECT, (int)(cl-svs.clients));
}

#endif
