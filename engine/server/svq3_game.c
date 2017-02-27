#include "quakedef.h"

//An implementation of a Q3 server...
//requires qvm implementation and existing q3 client stuff (or at least the overlapping stuff in q3common.c).

#ifdef Q3SERVER


#define USEBOTLIB

#ifdef USEBOTLIB


#define fileHandle_t int
#define fsMode_t int
#define pc_token_t void
#include "botlib.h"

#define Z_TAG_BOTLIB 221726

static botlib_export_t *FTE_GetBotLibAPI(int apiVersion, botlib_import_t *import)
{	//a stub that will prevent botlib from loading.
#ifdef BOTLIB_STATIC
	return GetBotLibAPI(apiVersion, import);
#else
	static void *botlib;
	static botlib_export_t *(QDECL *pGetBotLibAPI)(int apiVersion, botlib_import_t *import);

	dllfunction_t funcs[] =
	{
		{(void**)&pGetBotLibAPI, "GetBotLibAPI"},
		{NULL}
	};
	if (!botlib)
		botlib = Sys_LoadLibrary("botlib", funcs);
	if (!botlib)
		return NULL;
	return pGetBotLibAPI(apiVersion, import);
#endif
}

botlib_export_t *botlib;
#endif



#include "clq3defs.h"
#include "q3g_public.h"

static vm_t *q3gamevm;


#define fs_key 0

#define MAX_CONFIGSTRINGS 1024
static char *svq3_configstrings[MAX_CONFIGSTRINGS];

static q3sharedEntity_t *q3_entarray;
static int	numq3entities;
static int sizeofq3gentity;
static q3playerState_t *q3playerstates;
static int sizeofGameClient;

static int q3_num_snapshot_entities;
static int q3_next_snapshot_entities;
static q3entityState_t *q3_snapshot_entities;
static q3entityState_t *q3_baselines;
extern cvar_t sv_pure;

#define NUM_FOR_GENTITY(ge) (((char*)ge - (char*)q3_entarray) / sizeofq3gentity)
#define NUM_FOR_SENTITY(se) (se - q3_sentities)
#define GENTITY_FOR_NUM(num) ((q3sharedEntity_t*)((char *)q3_entarray + sizeofq3gentity*(num)))
#define SENTITY_FOR_NUM(num) ((q3serverEntity_t*)((char *)q3_sentities + sizeof(q3serverEntity_t)*(num)))
#define SENTITY_FOR_GENTITY(ge) (SENTITY_FOR_NUM(NUM_FOR_GENTITY(ge)))
#define GENTITY_FOR_SENTITY(se) (GENTITY_FOR_NUM(NUM_FOR_SENTITY(se)))

static qboolean BoundsIntersect (vec3_t mins1, vec3_t maxs1, vec3_t mins2, vec3_t maxs2);

void SVQ3_CreateBaseline(void);
void SVQ3_ClientThink(client_t *cl);
void SVQ3Q1_ConvertEntStateQ1ToQ3(entity_state_t *q1, q3entityState_t *q3);

const char *mapentspointer;

#define	Q3SOLID_BMODEL	0xffffff

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

static void Q3G_UnlinkEntity(q3sharedEntity_t *ent)
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

static model_t *Q3G_GetCModel(unsigned int modelindex)
{
	//0 is world
	//1 == *1
	//this is not how quake's precaches normally work.
	modelindex++;
	if ((unsigned int)modelindex < MAX_PRECACHE_MODELS)
	{
		if (!sv.models[modelindex])
		{
			if (modelindex == 1)
				sv.models[modelindex] = sv.world.worldmodel;
			else
				sv.models[modelindex] = Mod_ForName(Mod_FixName(va("*%i", modelindex-1), sv.modelname), MLV_WARN);
		}

		if (sv.models[modelindex]->loadstate == MLS_LOADING)
			COM_WorkerPartialSync(sv.models[modelindex], &sv.models[modelindex]->loadstate, MLS_LOADING);
		if (sv.models[modelindex]->loadstate == MLS_LOADED)
			return sv.models[modelindex];
	}
	return NULL;
}

static void Q3G_LinkEntity(q3sharedEntity_t *ent)
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
	if (sv.world.worldmodel->type == mod_heightmap)
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
		num_leafs = CM_BoxLeafnums(sv.world.worldmodel, ent->r.absmin, ent->r.absmax,
			leafs, MAX_TOTAL_ENT_LEAFS, &topnode);

		if(!num_leafs)
			return;

		// set areas
		for(i=0; i<num_leafs; i++)
		{
			clusters[i] = CM_LeafCluster(sv.world.worldmodel, leafs[i]);
			area = CM_LeafArea(sv.world.worldmodel, leafs[i]);
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
	node = sv.world.areanodes;
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
	InsertLinkBefore((link_t *)&sent->area, &node->edicts);
}

static int SVQ3_EntitiesInBoxNode(areanode_t *node, vec3_t mins, vec3_t maxs, int *list, int maxcount)
{
	link_t		*l, *next;
	q3serverEntity_t		*sent;
	q3sharedEntity_t		*gent;

	int linkcount = 0;

	//work out who they are first.
	for (l = node->edicts.next ; l != &node->edicts ; l = next)
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

static int SVQ3_EntitiesInBox(vec3_t mins, vec3_t maxs, int *list, int maxcount)
{
	if (maxcount < 0)
		return 0;
	return SVQ3_EntitiesInBoxNode(sv.world.areanodes, mins, maxs, list, maxcount);
}

#define	ENTITYNUM_NONE		(MAX_GENTITIES-1)
#define	ENTITYNUM_WORLD		(MAX_GENTITIES-2)
static void SVQ3_Trace(q3trace_t *result, vec3_t start, vec3_t mins, vec3_t maxs, vec3_t end, int entnum, int contentmask, qboolean capsule)
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

	sv.world.worldmodel->funcs.NativeTrace(sv.world.worldmodel, 0, NULLFRAMESTATE, NULL, start, end, mins, maxs, capsule, contentmask, &tr);
	result->allsolid = tr.allsolid;
	result->contents = tr.contents;
	VectorCopy(tr.endpos, result->endpos);
	result->entityNum = (tr.fraction==1.0)?ENTITYNUM_NONE:ENTITYNUM_WORLD;
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
			mmins[i] = start[i]+mins[i]-1;
			mmaxs[i] = end[i]+maxs[i]+1;
		}
		else
		{
			mmins[i] = end[i]+mins[i]-1;
			mmaxs[i] = start[i]+maxs[i]+1;
		}
	}

	if (entnum == -1)
		ourowner = -1;
	else if (entnum != ENTITYNUM_WORLD)
	{
		ourowner = GENTITY_FOR_NUM(entnum)->r.ownerNum;
		if (ourowner == ENTITYNUM_NONE)
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
			if (contactlist[i] == entnum)
				continue;
			if (es->r.ownerNum == entnum)
				continue;
			if (es->r.ownerNum == ourowner)
				continue;
		}

		if (es->r.bmodel)
		{
			mod = Q3G_GetCModel(es->s.modelindex);
			if (!mod)
				continue;
			World_TransformedTrace(mod, 0, 0, start, end, mins, maxs, capsule, &tr, es->r.currentOrigin, es->r.currentAngles, contentmask);
		}
		else
		{
			if (es->r.svFlags & SVF_CAPSULE)
				mod = CM_TempBoxModel(es->r.mins, es->r.maxs);
			else
				mod = CM_TempBoxModel(es->r.mins, es->r.maxs);
			World_TransformedTrace(mod, 0, 0, start, end, mins, maxs, capsule, &tr, es->r.currentOrigin, vec3_origin, contentmask);
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

static int SVQ3_PointContents(vec3_t pos, int entnum)
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
	cont = sv.world.worldmodel->funcs.NativeContents (sv.world.worldmodel, 0, 0, NULL, pos, vec3_origin, vec3_origin);

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
			if (contactlist[i] == entnum)
				continue;	// don't clip against the pass entity
			if (es->r.ownerNum == entnum)
				continue;	// don't clip against own missiles
			if (es->r.ownerNum == ourowner)
				continue;	// don't clip against other missiles from our owner
		}

		if (es->r.bmodel)
		{
			mod = Q3G_GetCModel(es->s.modelindex);
			if (!mod)
				continue;
			World_TransformedTrace(mod, 0, 0, pos, pos, vec3_origin, vec3_origin, false, &tr, es->r.currentOrigin, es->r.currentAngles, 0xffffffff);
		}
		else
		{
			mod = CM_TempBoxModel(es->r.mins, es->r.maxs);
			World_TransformedTrace(mod, 0, 0, pos, pos, vec3_origin, vec3_origin, false, &tr, es->r.currentOrigin, vec3_origin, 0xffffffff);
		}

		cont |= tr.contents;
	}
	return cont;
}

static int SVQ3_Contact(vec3_t mins, vec3_t maxs, q3sharedEntity_t *ent, qboolean capsule)
{
	model_t *mod;
	trace_t tr;
	float *ang;

	if (ent->r.bmodel)
	{
		ang = ent->r.currentAngles;
		mod = Q3G_GetCModel(ent->s.modelindex);
	}
	else
	{
		ang = vec3_origin;
		mod = CM_TempBoxModel(ent->r.mins, ent->r.maxs);
	}

	if (!mod || !mod->funcs.NativeTrace)
		return false;

	World_TransformedTrace(mod, 0, 0, vec3_origin, vec3_origin, mins, maxs, capsule, &tr, ent->r.currentOrigin, ang, 0xffffffff);

	if (tr.startsolid)
		return true;
	return false;
}

static void SVQ3_SetBrushModel(q3sharedEntity_t *ent, char *modelname)
{
	int modelindex;
	model_t *mod;
	if (!modelname || *modelname != '*')
		SV_Error("SVQ3_SetBrushModel: not an inline model");
	modelindex = atoi(modelname+1);
	mod = Q3G_GetCModel(modelindex);
	if (mod)
	{
		VectorCopy(mod->mins, ent->r.mins);
		VectorCopy(mod->maxs, ent->r.maxs);
	}
	else
	{
		VectorCopy(vec3_origin, ent->r.mins);
		VectorCopy(vec3_origin, ent->r.maxs);
	}
	ent->r.bmodel = true;
	ent->r.contents = -1;
	ent->s.modelindex = modelindex;

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
static qboolean SVQ3_GetUserCmd(int clientnumber, q3usercmd_t *ucmd)
{
	usercmd_t *cmd;

	if (clientnumber < 0 || clientnumber >= sv.allocated_client_slots)
		SV_Error("SVQ3_GetUserCmd: Client out of range");

	cmd = &svs.clients[clientnumber].lastcmd;
	ucmd->angles[0] = cmd->angles[0];
	ucmd->angles[1] = cmd->angles[1];
	ucmd->angles[2] = cmd->angles[2];
	ucmd->serverTime = cmd->servertime;
	ucmd->forwardmove = (signed char)cmd->forwardmove;
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
		for (i = 0; i < sv.allocated_client_slots; i++)
		{
			if (svs.clients[i].state>=cs_connected)
			{
				SVQ3_SendServerCommand(&svs.clients[i], str);	//go for consistancy.
			}
		}
		return;
	}

	cl->num_server_commands++;
	Q_strncpyz(cl->server_commands[cl->num_server_commands & TEXTCMD_MASK], str, sizeof(cl->server_commands[0]));
}

void SVQ3_SendConfigString(client_t *dest, int num, char *string)
{
	int len = strlen(string);
#define CONFIGSTRING_MAXCHUNK (1024-24)
	if (len > CONFIGSTRING_MAXCHUNK)
	{
		char *cmd;
		char buf[CONFIGSTRING_MAXCHUNK+1];
		int off = 0;
		for (;;)
		{
			int chunk = len - off;
			if (chunk > CONFIGSTRING_MAXCHUNK)
				chunk = CONFIGSTRING_MAXCHUNK;
			//split it up into multiple commands.
			if (!off)
				cmd = "bcs0";	//initial chunk
			else if (off + chunk == len)
				cmd = "bcs2";	//terminator
			else
				cmd = "bcs1";	//mid chunk
			memcpy(buf, string+off, chunk);
			buf[chunk] = 0;
			SVQ3_SendServerCommand(dest, va("%s %i \"%s\"\n", cmd, num, buf));
			off += chunk;
			if (off == len)
				break;
		}
	}
	else
		SVQ3_SendServerCommand(dest, va("cs %i \"%s\"\n", num, string));
}

void SVQ3_SetConfigString(int num, char *string)
{
	int len;
	if (!string)
		string = "";
	len = strlen(string);
	if (svq3_configstrings[num])
		Z_Free(svq3_configstrings[num]);
	svq3_configstrings[num] = Z_Malloc(len+1);
	strcpy(svq3_configstrings[num], string);

	SVQ3_SendConfigString(NULL, num, string);
}

static int FloatAsInt(float f)
{
	return *(int*)&f;
}

static int SVQ3_BotGetConsoleMessage( int client, char *buf, int size )
{
	//retrieves server->client commands that were sent to a bot
	client_t	*cl;
	int			index;

	if ((unsigned)client >= sv.allocated_client_slots)
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
static int SVQ3_BotGetSnapshotEntity(int client, int entnum)
{
	//fixme: does the bot actually use this?...
	return -1;
}

static void SVQ3_Adjust_Area_Portal_State(q3sharedEntity_t *ge, qboolean open)
{
	q3serverEntity_t *se = SENTITY_FOR_GENTITY(ge);
	if (se->areanum == -1 || se->areanum2 == -1) //not linked properly.
		return;
	CMQ3_SetAreaPortalState(sv.world.worldmodel, se->areanum, se->areanum2, open);
}

#define VALIDATEPOINTER(o,l) if ((int)o + l >= mask || VM_POINTER(o) < offset) SV_Error("Call to game trap %u passes invalid pointer\n", (unsigned int)fn);	//out of bounds.
static qintptr_t Q3G_SystemCalls(void *offset, unsigned int mask, qintptr_t fn, const qintptr_t *arg)
{
	int ret = 0;
	switch(fn)
	{
	case G_PRINT:		// ( const char *string );
		Con_Printf("%s", (char*)VM_POINTER(arg[0]));
		break;
	case G_ERROR:		// ( const char *string );
		SV_Error("Q3 Game error: %s", (char*)VM_POINTER(arg[0]));
		break;
	case G_MILLISECONDS:
		return Sys_DoubleTime()*1000;

	case G_CVAR_REGISTER:// ( vmCvar_t *vmCvar, const char *varName, const char *defaultValue, int flags );
		if (arg[0])
			VALIDATEPOINTER(arg[0], sizeof(q3vmcvar_t));
		return VMQ3_Cvar_Register(VM_POINTER(arg[0]), VM_POINTER(arg[1]), VM_POINTER(arg[2]), VM_LONG(arg[3]));
	case G_CVAR_UPDATE:// ( vmCvar_t *vmCvar );
		VALIDATEPOINTER(arg[0], sizeof(q3vmcvar_t));
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
		if ((unsigned)VM_LONG(arg[0]) < sv.allocated_client_slots)
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

		Con_DPrintf("Game dispatching %s\n", (char*)VM_POINTER(arg[1]));
		if (VM_LONG(arg[0]) == -1)
		{	//broadcast
			SVQ3_SendServerCommand(NULL, VM_POINTER(arg[1]));
		}
		else
		{
			int i = VM_LONG(arg[0]);
			if (i < 0 || i >= sv.allocated_client_slots)
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
		SV_ExtractFromUserinfo(&svs.clients[VM_LONG(arg[0])], false);
		break;

	case G_LINKENTITY:		// ( gentity_t *ent );													30
		Q3G_LinkEntity(VM_POINTER(arg[0]));
		break;
	case G_UNLINKENTITY:		// ( gentity_t *ent );												31
		Q3G_UnlinkEntity(VM_POINTER(arg[0]));
		break;
	case G_TRACE:	// ( trace_t *results, const vec3_t start, const vec3_t mins, const vec3_t maxs, const vec3_t end, int passEntityNum, int contentmask );
		VALIDATEPOINTER(arg[0], sizeof(q3trace_t));
		SVQ3_Trace(VM_POINTER(arg[0]), VM_POINTER(arg[1]), VM_POINTER(arg[2]), VM_POINTER(arg[3]), VM_POINTER(arg[4]), VM_LONG(arg[5]), VM_LONG(arg[6]), false);
		break;
	case G_TRACECAPSULE:	// ( trace_t *results, const vec3_t start, const vec3_t mins, const vec3_t maxs, const vec3_t end, int passEntityNum, int contentmask );
		VALIDATEPOINTER(arg[0], sizeof(q3trace_t));
		SVQ3_Trace(VM_POINTER(arg[0]), VM_POINTER(arg[1]), VM_POINTER(arg[2]), VM_POINTER(arg[3]), VM_POINTER(arg[4]), VM_LONG(arg[5]), VM_LONG(arg[6]), true);
		break;
	case G_ENTITY_CONTACT:
			// ( const vec3_t mins, const vec3_t maxs, const gentity_t *ent );	33
	// perform an exact check against inline brush models of non-square shape
		return SVQ3_Contact(VM_POINTER(arg[0]), VM_POINTER(arg[1]), VM_POINTER(arg[2]), false);
	case G_ENTITY_CONTACTCAPSULE:
		return SVQ3_Contact(VM_POINTER(arg[0]), VM_POINTER(arg[1]), VM_POINTER(arg[2]), true);

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
			Q_strncpyS(dst, src, arg[2]);
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
			if ((unsigned)i >= sv.allocated_client_slots)
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
		Con_Printf("builtin %i is not implemented\n", (int)fn);
	}
	return ret;
}

static int Q3G_SystemCallsVM(void *offset, quintptr_t mask, int fn, const int *arg)
{
	qintptr_t args[13];

	args[0]=arg[0];
	args[1]=arg[1];
	args[2]=arg[2];
	args[3]=arg[3];
	args[4]=arg[4];
	args[5]=arg[5];
	args[6]=arg[6];
	args[7]=arg[7];
	args[8]=arg[8];
	args[9]=arg[9];
	args[10]=arg[10];
	args[11]=arg[11];
	args[12]=arg[12];

	return Q3G_SystemCalls(offset, mask, fn, args);
}

static qintptr_t EXPORT_FN Q3G_SystemCallsNative(qintptr_t arg, ...)
{
	qintptr_t args[13];
	va_list argptr;

	va_start(argptr, arg);
	args[0]=va_arg(argptr, qintptr_t);
	args[1]=va_arg(argptr, qintptr_t);
	args[2]=va_arg(argptr, qintptr_t);
	args[3]=va_arg(argptr, qintptr_t);
	args[4]=va_arg(argptr, qintptr_t);
	args[5]=va_arg(argptr, qintptr_t);
	args[6]=va_arg(argptr, qintptr_t);
	args[7]=va_arg(argptr, qintptr_t);
	args[8]=va_arg(argptr, qintptr_t);
	args[9]=va_arg(argptr, qintptr_t);
	args[10]=va_arg(argptr, qintptr_t);
	args[11]=va_arg(argptr, qintptr_t);
	args[12]=va_arg(argptr, qintptr_t);
	va_end(argptr);

	return Q3G_SystemCalls(NULL, ~0, arg, args);
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
static void VARGS BL_Print(int l, char *fmt, ...)
{
	va_list		argptr;
	char		text[1024];

	va_start (argptr, fmt);
	vsnprintf (text, sizeof(text), fmt, argptr);
	va_end (argptr);

	Con_Printf("%s", text);
}

static int botlibmemoryavailable;
static int QDECL BL_AvailableMemory(void)
{
	return botlibmemoryavailable;
}
static void *QDECL BL_Malloc(int size)
{
	int *mem;
	botlibmemoryavailable-=size;

	mem = (int *)Z_TagMalloc(size+sizeof(int), Z_TAG_BOTLIB);
	mem[0] = size;

	return (void *)(mem + 1);
}
static void QDECL BL_Free(void *mem)
{
	int *memref = ((int *)mem) - 1;
	botlibmemoryavailable+=memref[0];
	Z_TagFree(memref);
}
static void *QDECL BL_HunkMalloc(int size)
{
	return BL_Malloc(size);//Hunk_AllocName(size, "botlib");
}

static int QDECL BL_FOpenFile(const char *name, fileHandle_t *handle, fsMode_t mode)
{
	return VM_fopen((char*)name, (int*)handle, mode, Z_TAG_BOTLIB);
}
static int QDECL BL_FRead(void *buffer, int len, fileHandle_t f)
{
	return VM_FRead(buffer, len, (int)f, Z_TAG_BOTLIB);
}
static int QDECL BL_FWrite(const void *buffer, int len, fileHandle_t f)
{
	return VM_FWrite(buffer, len, (int)f, Z_TAG_BOTLIB);
}
static void QDECL BL_FCloseFile(fileHandle_t f)
{
	VM_fclose((int)f, Z_TAG_BOTLIB);
}
static int QDECL BL_Seek(fileHandle_t f, long offset, int seektype)
{	// on success, apparently returns 0
	return VM_FSeek((int)f, offset, seektype, Z_TAG_BOTLIB)?0:-1;
}
static const char *QDECL BL_BSPEntityData(void)
{
	return Mod_GetEntitiesString(sv.world.worldmodel);
}
static void QDECL BL_Trace(bsp_trace_t *trace, vec3_t start, vec3_t mins, vec3_t maxs, vec3_t end, int passent, int contentmask)
{
	q3trace_t tr;
	SVQ3_Trace(&tr, start, mins, maxs, end, passent, contentmask, false);

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
static int QDECL BL_PointContents(vec3_t point)
{
	return SVQ3_PointContents(point, -1);
}

static int QDECL BL_inPVS(vec3_t p1, vec3_t p2)
{
	return true;// FIXME: :(
}

static void QDECL BL_EntityTrace(bsp_trace_t *trace, vec3_t start, vec3_t mins, vec3_t maxs, vec3_t end, int entnum, int contentmask)
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

static void QDECL BL_BSPModelMinsMaxsOrigin(int modelnum, vec3_t angles, vec3_t outmins, vec3_t outmaxs, vec3_t origin)
{
	model_t *mod;
	vec3_t mins, maxs;
	float max;
	int	i;

	mod = Q3G_GetCModel(modelnum);
	if (mod)
	{
		VectorCopy(mod->mins, mins);
		VectorCopy(mod->maxs, maxs);
	}
	else
	{
		VectorCopy(vec3_origin, mins);
		VectorCopy(vec3_origin, maxs);
	}

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
static void QDECL BL_BotClientCommand(int clientnum, char *command)
{
	Cmd_TokenizeString(command, false, false);
	VM_Call(q3gamevm, GAME_CLIENT_COMMAND, clientnum);
}

static int QDECL BL_DebugLineCreate(void) {return 0;}
static void QDECL BL_DebugLineDelete(int line) {}
static void QDECL BL_DebugLineShow(int line, vec3_t start, vec3_t end, int color) {}
static int QDECL BL_DebugPolygonCreate(int color, int numPoints, vec3_t *points) {return 0;}
static void QDECL BL_DebugPolygonDelete(int id) {}

#endif

static void SV_InitBotLib(void)
{
	cvar_t *bot_enable = Cvar_Get("bot_enable", "1", 0, "Q3 compatability");

#ifdef USEBOTLIB
	botlib_import_t import;

	Cvar_Set(Cvar_Get("sv_mapChecksum", "0", 0, "Q3 compatability"), va("%i", sv.world.worldmodel->checksum));

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
	import.FS_Write = BL_FWrite;
	import.FS_FCloseFile = BL_FCloseFile;
	import.FS_Seek = BL_Seek;

	import.DebugLineCreate = BL_DebugLineCreate;
	import.DebugLineDelete = BL_DebugLineDelete;
	import.DebugLineShow = BL_DebugLineShow;
	import.DebugPolygonCreate= BL_DebugPolygonCreate;
	import.DebugPolygonDelete = BL_DebugPolygonDelete;

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
#else

//make sure it's switched off.
	Cvar_ForceSet(bot_enable, "0");
	bot_enable->flags |= CVAR_NOSET;
#endif
}

qboolean SVQ3_InitGame(void)
{
	int i;
	char buffer[8192];
	char *str;
	char sysinfo[8192];

	if (sv.world.worldmodel->type == mod_heightmap)
	{
	}
	else
	{
		if (sv.world.worldmodel->fromgame == fg_quake || sv.world.worldmodel->fromgame == fg_halflife || sv.world.worldmodel->fromgame == fg_quake2)
			return false;	//always fail on q1bsp
	}

	if (*pr_ssqc_progs.string)	//don't load q3 gamecode if we're explicitally told to load a progs.
		return false;


	SVQ3_ShutdownGame();

	q3gamevm = VM_Create("vm/qagame", com_nogamedirnativecode.ival?NULL:Q3G_SystemCallsNative, Q3G_SystemCallsVM);

	if (!q3gamevm)
		return false;

	//q3 needs mapname (while qw has map serverinfo)
	{
		cvar_t *mapname = Cvar_Get("mapname", "", CVAR_SERVERINFO, "Q3 compatability");
		Cvar_Set(mapname, svs.name);
	}

	SV_InitBotLib();

	World_ClearWorld(&sv.world, false);

	q3_sentities = Z_Malloc(sizeof(q3serverEntity_t)*MAX_GENTITIES);

	/*qw serverinfo settings are not normally visible in the q3 serverinfo*/
	strcpy(buffer, svs.info);
	Info_SetValueForKey(buffer, "map", "", sizeof(buffer));
	Info_SetValueForKey(buffer, "maxclients", "", sizeof(buffer));
	Info_SetValueForKey(buffer, "mapname", svs.name, sizeof(buffer));
	Info_SetValueForKey(buffer, "sv_maxclients", "32", sizeof(buffer));
	Info_SetValueForKey(buffer, "sv_pure", "", sizeof(buffer));
	SVQ3_SetConfigString(0, buffer);

	Cvar_Set(Cvar_Get("sv_running", "0", 0, "Q3 compatability"), "1");

	sysinfo[0] = '\0';
	Info_SetValueForKey(sysinfo, "sv_serverid", va("%i", svs.spawncount), MAX_SERVERINFO_STRING);

	str = FS_GetPackHashes(buffer, sizeof(buffer), false);
	Info_SetValueForKey(sysinfo, "sv_paks", str, MAX_SERVERINFO_STRING);

	str = FS_GetPackNames(buffer, sizeof(buffer), false, false);
	Info_SetValueForKey(sysinfo, "sv_pakNames", str, MAX_SERVERINFO_STRING);

	str = FS_GetPackHashes(buffer, sizeof(buffer), true);
	Info_SetValueForKey(sysinfo, "sv_referencedPaks", str, MAX_SERVERINFO_STRING);

	str = FS_GetPackNames(buffer, sizeof(buffer), true, false);
	Info_SetValueForKey(sysinfo, "sv_referencedPakNames", str, MAX_SERVERINFO_STRING);

	Info_SetValueForKey(sysinfo, "sv_pure", sv_pure.string, MAX_SERVERINFO_STRING);

	SVQ3_SetConfigString(1, sysinfo);


	mapentspointer = Mod_GetEntitiesString(sv.world.worldmodel);
	VM_Call(q3gamevm, GAME_INIT, 0, (int)rand(), false);

	CM_InitBoxHull();

	SVQ3_CreateBaseline();

	q3_num_snapshot_entities = 32 * Q3UPDATE_BACKUP * 32;
	if (q3_snapshot_entities)
		BZ_Free(q3_snapshot_entities);
	q3_next_snapshot_entities = 0;
	q3_snapshot_entities = BZ_Malloc(sizeof( q3entityState_t ) * q3_num_snapshot_entities);


		// run a few frames to allow everything to settle
	for (i = 0; i < 3; i++)
	{
		SVQ3_RunFrame();
		sv.time += 0.1;
	}

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
	sv.spawned_client_slots++;
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
	q3entityState_t	*oldent, *newent;
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
			if (svs.gametype == GT_QUAKE3)
				MSGQ3_WriteDeltaEntity( msg, &q3_baselines[newnum], newent, true );
			else
			{
				q3entityState_t q3base;
				edict_t *e;
				e = EDICT_NUM(svprogfuncs, newnum);
				SVQ3Q1_ConvertEntStateQ1ToQ3(&e->baseline, &q3base);
				MSGQ3_WriteDeltaEntity( msg, &q3base, newent, true );
			}
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

	// this is the frame we are transmitting
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
	MSG_WriteBits(msg, snap->serverTime, 32);
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


static int clientNum;
static int clientarea;
static qbyte		*bitvector;

static int VARGS SVQ3_QsortEntityStates( const void *arg1, const void *arg2 )
{
	const q3entityState_t *s1 = *(const q3entityState_t **)arg1;
	const q3entityState_t *s2 = *(const q3entityState_t **)arg2;


	if( s1->number > s2->number )
	{
		return 1;
	}

	if( s1->number < s2->number )
	{
		return -1;
	}

	SV_Error("SV_QsortEntityStates: duplicated entity");

	return 0;

}

static qboolean SVQ3_EntityIsVisible(q3client_frame_t *snap, q3sharedEntity_t *ent)
{
	q3serverEntity_t *sent;
	int i;
	int l;

	if (!ent->r.linked)
	{
		return false; // not active entity
	}

	if (ent->r.svFlags & SVF_NOCLIENT )
	{
		return false; // set to invisible
	}

	if (ent->r.svFlags & SVF_CLIENTMASK)
	{
		if (clientNum > 32)
		{
			SV_Error("SVF_CLIENTMASK: clientNum > 32" );
		}
		if (ent->r.singleClient & (1 << (clientNum & 7)))
		{
			return true;
		}
		return false;
	}

	if (ent->r.svFlags & SVF_SINGLECLIENT)
	{
		if ( ent->r.singleClient == clientNum)
		{
			return true;
		}
		return false;
	}

	if (ent->r.svFlags & SVF_NOTSINGLECLIENT)
	{
		if (ent->r.singleClient == clientNum)
		{
			return false;
		}
		// FIXME: fall through
	}

	if (ent->r.svFlags & SVF_BROADCAST)
	{
		return true;
	}

	//
	// ignore if not touching a PV leaf
	//
	sent = SENTITY_FOR_GENTITY( ent );

	// check area
	if (sent->areanum < 0 || !(snap->areabits[sent->areanum >> 3] & (1 << (sent->areanum & 7))))
	{
		// doors can legally straddle two areas, so
		// we may need to check another one
		if (sent->areanum2 < 0 || !(snap->areabits[sent->areanum2 >> 3] & (1 << (sent->areanum2 & 7))))
		{
			return false;		// blocked by a door
		}
	}

/*
	// check area
	if( !CM_AreasConnected( clientarea, sent->areanum ) )
	{
		// doors can legally straddle two areas, so
		// we may need to check another one
		if( !CM_AreasConnected( clientarea, sent->areanum2 ) )
		{
			return false;		// blocked by a door
		}
	}
*/


	if (sent->num_clusters == -1)
	{
		// too many leafs for individual check, go by headnode
		if (!CM_HeadnodeVisible(sv.world.worldmodel, sent->headnode, bitvector))
		{
			return false;
		}
	}
	else
	{
		// check individual leafs
		for (i=0; i < sent->num_clusters; i++)
		{
			l = sent->clusternums[i];
			if (bitvector[l >> 3] & (1 << (l & 7)))
			{
				break;
			}
		}
		if (i == sent->num_clusters)
		{
			return false;		// not visible
		}
	}

	return true;
}

q3playerState_t *SVQ3Q1_BuildPlayerState(client_t *client)
{
	static q3playerState_t state;
	extern cvar_t sv_gravity;

	memset(&state, 0, sizeof(state));

#ifdef warningmsg
#pragma warningmsg("qwoverq3: other things will need to be packed into here.")
#endif

	state.commandTime = client->lastcmd.servertime;

	state.pm_type = client->edict->v->movetype;
	state.origin[0] = client->edict->v->origin[0];
	state.origin[1] = client->edict->v->origin[1];
	state.origin[2] = client->edict->v->origin[2];
	state.velocity[0] = client->edict->v->velocity[0];
	state.velocity[1] = client->edict->v->velocity[1];
	state.velocity[2] = client->edict->v->velocity[2];

	client->maxspeed = client->edict->xv->maxspeed;
	if (!client->maxspeed)
		client->maxspeed = sv_maxspeed.value;
	client->entgravity = client->edict->xv->gravity * sv_gravity.value;
	if (!client->entgravity)
		client->entgravity = sv_gravity.value;
	if (client->edict->xv->hasted)
		client->maxspeed *= client->edict->xv->hasted;
	state.speed = client->maxspeed;
	state.gravity = client->entgravity;

	state.viewangles[0] = client->edict->v->angles[0];
	state.viewangles[1] = client->edict->v->angles[1];
	state.viewangles[2] = client->edict->v->angles[2];

	state.clientNum = client - svs.clients;
	state.weapon = client->edict->v->weapon;

	state.stats[0] = client->edict->v->health;
	state.stats[2] = (int)client->edict->v->items&1023;
	state.stats[3] = client->edict->v->armorvalue;
	state.stats[4] = client->edict->v->angles[1];
//	state.stats[6] = client->edict->v->max_health;
	state.persistant[0] = client->edict->v->frags;
	state.ammo[0] = client->edict->v->currentammo;
	state.ammo[1] = client->edict->v->ammo_shells;
	state.ammo[2] = client->edict->v->ammo_nails;
	state.ammo[3] = client->edict->v->ammo_rockets;
	state.ammo[4] = client->edict->v->ammo_cells;
	return &state;
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
	static qbyte pvsbuffer[(MAX_MAP_LEAFS+7)>>3];

	if (!q3_snapshot_entities)
	{
		q3_num_snapshot_entities = 32 * Q3UPDATE_BACKUP * 32;
		q3_next_snapshot_entities = 0;
		q3_snapshot_entities = BZ_Malloc(sizeof( q3entityState_t ) * q3_num_snapshot_entities);
	}

	clientNum = client - svs.clients;
	if (svs.gametype == GT_QUAKE3)
	{
		clent = GENTITY_FOR_NUM( clientNum );
		ps = PS_FOR_NUM( clientNum );
	}
	else
	{
		clent = NULL;
		ps = SVQ3Q1_BuildPlayerState(client);
	}

	// this is the frame we are creating
	snap = &client->frameunion.q3frames[client->netchan.outgoing_sequence & Q3UPDATE_MASK];

	snap->serverTime = sv.time*1000;//svs.levelTime; // save it for ping calc later
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

	clientarea = CM_PointLeafnum(sv.world.worldmodel, org);
	bitvector = sv.world.worldmodel->funcs.ClusterPVS(sv.world.worldmodel, CM_LeafCluster(sv.world.worldmodel, clientarea), pvsbuffer, sizeof(pvsbuffer));
	clientarea = CM_LeafArea(sv.world.worldmodel, clientarea);
/*
	if (client->areanum != clientarea)
	{
		Com_Printf( "%s entered area %i\n", client->name, clientarea);
		client->areanum = clientarea;
	}
*/

	// calculate the visible areas
	snap->areabytes = CM_WriteAreaBits(sv.world.worldmodel, snap->areabits, clientarea, false);

	// grab the current playerState_t
	memcpy(&snap->ps, ps, sizeof(snap->ps));

	// build up the list of visible entities
	snap->num_entities = 0;
	snap->first_entity = q3_next_snapshot_entities;

	if (svs.gametype == GT_QUAKE3)
	{
	// check for SVF_PORTAL entities first
		for( i=0 ; i<numq3entities ; i++)
		{
			unsigned int c;
			qbyte *merge;
			ent = GENTITY_FOR_NUM(i);

			if(ent == clent )
				continue;
			if(!(ent->r.svFlags & SVF_PORTAL))
				continue;
			if(!SVQ3_EntityIsVisible(snap, ent))
				continue;

			// merge PVS if portal
			portalarea = CM_PointLeafnum(sv.world.worldmodel, ent->s.origin2);
			//merge pvs bits so we can see other ents through it
			merge = sv.world.worldmodel->funcs.ClusterPVS(sv.world.worldmodel, CM_LeafCluster(sv.world.worldmodel, portalarea), NULL, 0);
			c = (sv.world.worldmodel->numclusters+31)/32;
			while (c-->0)
				((int *)bitvector)[c] |= ((int *)merge)[c];
			//and merge areas, so we can see the world too (client will calc its own pvs)
			portalarea = CM_LeafArea(sv.world.worldmodel, portalarea);
			CM_WriteAreaBits(sv.world.worldmodel, snap->areabits, portalarea, true);
		}

		// add all visible entities
		for (i=0 ; i<numq3entities ; i++)
		{
			ent = GENTITY_FOR_NUM(i);

			if (ent == clent)
				continue;
			if (!SVQ3_EntityIsVisible(snap, ent))
				continue;

			if (ent->s.number != i)
			{
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
	}
	else
	{	//our q1->q3 converter
		packet_entities_t pack;
		entity_state_t packentities[64];
		q3entityState_t q3packentities[64];
		pack.entities = packentities;
		pack.max_entities = sizeof(packentities)/sizeof(packentities[0]);
		//get the q1 code to generate a packet
		SVQ3Q1_BuildEntityPacket(client, &pack);
		for (i = 0; i < pack.num_entities; i++)
		{	//map the packet fields to q3.
			SVQ3Q1_ConvertEntStateQ1ToQ3(&pack.entities[i], &q3packentities[i]);
			entityStates[snap->num_entities++] = &q3packentities[i];
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

void SVQ3Q1_ConvertEntStateQ1ToQ3(entity_state_t *q1, q3entityState_t *q3)
{
#ifdef warningmsg
#pragma warningmsg("qwoverq3: This _WILL_ need extending")
#endif
	q3->number = q1->number;

	q3->pos.trTime = 0;
	q3->pos.trBase[0] = 0;
	q3->pos.trBase[1] = 0;
	q3->pos.trDelta[0] = 0;
	q3->pos.trDelta[1] = 0;
	q3->pos.trBase[2] = 0;
	q3->apos.trBase[1] = 0;
	q3->pos.trDelta[2] = 0;
	q3->apos.trBase[0] = 0;
	q3->event = 0;
	q3->angles2[1] = 0;
	q3->eType = 0;
	q3->torsoAnim = q1->skinnum;
	q3->eventParm = 0;
	q3->legsAnim = 0;
	q3->groundEntityNum = 0;
	q3->pos.trType = 0;
	q3->eFlags = 0;
	q3->otherEntityNum = 0;
	q3->weapon = 0;
	q3->clientNum = q1->colormap;
	q3->angles[1] = q1->angles[0];
	q3->pos.trDuration = 0;
	q3->apos.trType = 0;
	q3->origin[0] = q1->origin[0];
	q3->origin[1] = q1->origin[1];
	q3->origin[2] = q1->origin[2];
	q3->solid = q1->solidsize;
	q3->powerups = q1->effects;
	q3->modelindex = q1->modelindex;
	q3->otherEntityNum2 = 0;
	q3->loopSound = 0;
	q3->generic1 = q1->trans;
	q3->origin2[2] = 0;//q1->old_origin[2];
	q3->origin2[0] = 0;//q1->old_origin[0];
	q3->origin2[1] = 0;//q1->old_origin[1];
	q3->modelindex2 = 0;//q1->modelindex2;
	q3->angles[0] = q1->angles[0];
	q3->time = 0;
	q3->apos.trTime = 0;
	q3->apos.trDuration = 0;
	q3->apos.trBase[2] = 0;
	q3->apos.trDelta[0] = 0;
	q3->apos.trDelta[1] = 0;
	q3->apos.trDelta[2] = 0;
	q3->time2 = 0;
	q3->angles[2] = q1->angles[2];
	q3->angles2[0] = 0;
	q3->angles2[2] = 0;
	q3->constantLight = q1->abslight;
	q3->frame = q1->frame;

#if 0

//these are the things I've not packed in to the above structure yet.
#if defined(Q2CLIENT) || defined(Q2SERVER)
	int             renderfx;               //q2
	qbyte           modelindex3;    //q2
	qbyte           modelindex4;    //q2
#endif
	qbyte glowsize;
	qbyte glowcolour;
	qbyte   scale;
	char    fatness;
	qbyte   hexen2flags;
	qbyte   dpflags;
	qbyte   colormod[3];//multiply this by 8 to read as 0 to 1...
	qbyte lightstyle;
	qbyte lightpflags;
	unsigned short light[4];
	unsigned short tagentity;
	unsigned short tagindex;
#endif
}

void SVQ3Q1_SendGamestateConfigstrings(sizebuf_t *msg)
{
	const int cs_models = 32;
	const int cs_sounds = cs_models + 256;
	const int cs_players = cs_sounds + 256;

	int i, j;

	const char *str;
	char sysinfo[MAX_SERVERINFO_STRING];
	const char *cfgstr[MAX_CONFIGSTRINGS];

	//an empty crc string means we let the client use any
	//but then it doesn't download our qwoverq3 thing.
	char *refpackcrcs = "";//"-1309355180 0 0 0 0 0 0 0 0 0";
	char *refpacknames = "baseq3/pak0 baseq3/pak1 baseq3/pak2 baseq3/pak3 baseq3/pak4 baseq3/pak5 baseq3/pak6 baseq3/pak7 baseq3/pak8 fte/qwoverq3";

	memset((void*)cfgstr, 0, sizeof(cfgstr));

	sysinfo[0] = 0;
	Info_SetValueForKey(sysinfo, "sv_serverid", va("%i", svs.spawncount), sizeof(sysinfo));

	str = refpackcrcs;//FS_GetPackHashes(buffer, sizeof(buffer), false);
	Info_SetValueForKey(sysinfo, "sv_paks", str, sizeof(sysinfo));

	str = refpacknames;//FS_GetPackNames(buffer, sizeof(buffer), false);
	Info_SetValueForKey(sysinfo, "sv_pakNames", str, sizeof(sysinfo));

	str = refpackcrcs;//FS_GetPackHashes(buffer, sizeof(buffer), true);
	Info_SetValueForKey(sysinfo, "sv_referencedPaks", str, sizeof(sysinfo));

	str = refpacknames;//FS_GetPackNames(buffer, sizeof(buffer), true);
	Info_SetValueForKey(sysinfo, "sv_referencedPakNames", str, sizeof(sysinfo));

//Con_Printf("Sysinfo: %s\n", sysinfo);
	str = "0";
	Info_SetValueForKey(sysinfo, "sv_pure", str, sizeof(sysinfo));

	str = "qwoverq3";
	Info_SetValueForKey(sysinfo, "fs_game", str, sizeof(sysinfo));

	cfgstr[0] = svs.info;
	cfgstr[1] = sysinfo;

	cfgstr[2] = NULL;//sv.cdtrack;
	cfgstr[3] = sv.mapname;
	cfgstr[20] = "QuakeWorld-Over-Q3";	//you can get the gamedir out of the serverinfo

	//add in 32 clients
	for (i = 0; i < sv.allocated_client_slots; i++)
	{
		cfgstr[cs_players+i] = svs.clients[i].userinfo;
	}

	//fill up the last half with sound/model names
	//note that we're limited to only 256 models/sounds
	for (i = 2; i < 256; i++)
	{
		cfgstr[cs_models+i] = sv.strings.model_precache[i];
	}
	for (i = 0; i < 256; i++)
	{
		cfgstr[cs_sounds+i] = sv.strings.sound_precache[i];
	}

	// write configstrings
	for (i = 0; i < MAX_CONFIGSTRINGS; i++)
	{
		str = cfgstr[i];
		if (!str || !*str)
			continue;

		MSG_WriteBits(msg, svcq3_configstring, 8);
		MSG_WriteBits(msg, i, 16);
		for (j = 0; str[j]; j++)
			MSG_WriteBits(msg, str[j], 8);
		MSG_WriteBits(msg, 0, 8);
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

	switch (svs.gametype)
	{
	case GT_QUAKE3:
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
				if (!q3_baselines[i].number)
					continue;

			MSG_WriteBits(&msg, svcq3_baseline, 8);
			MSGQ3_WriteDeltaEntity( &msg, NULL, &q3_baselines[i], true );
		}
		break;
	case GT_PROGS:
	case GT_Q1QVM:
		SVQ3Q1_SendGamestateConfigstrings(&msg);

		for (i = sv.allocated_client_slots+1; i < sv.world.num_edicts; i++)
		{
			edict_t *e = EDICT_NUM(svprogfuncs, i);
			if (e->baseline.modelindex)
			{
				q3entityState_t q3base;
				SVQ3Q1_ConvertEntStateQ1ToQ3(&e->baseline, &q3base);
				MSG_WriteBits(&msg, svcq3_baseline, 8);
				MSGQ3_WriteDeltaEntity(&msg, NULL, &q3base, true);
			}
		}
		break;
	// warning: enumeration value GT_? not handled in switch
	case GT_HALFLIFE:
	case GT_QUAKE2:
	case GT_MAX:
		break;
	}

	// write svc_eom command
	MSG_WriteBits(&msg, svcq3_eom, 8);

	MSG_WriteBits(&msg, client - svs.clients, 32);
	MSG_WriteBits(&msg, fs_key, 32);

	// end of message marker
	MSG_WriteBits(&msg, svcq3_eom, 8);

	// send the datagram
	SVQ3_Netchan_Transmit(client, msg.cursize, msg.data);

	// calculate client->sendTime
//	SV_RateDrop(client, msg.cursize);

	client->state = cs_connected;
	client->gamestatesequence = client->last_sequence;
}

void SVQ3_WriteServerCommandsToClient(client_t *client, sizebuf_t *msg)
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

	SVQ3_BuildClientSnapshot(client);

	MSG_WriteBits(&msg, client->last_client_command_num, 32);

	// write pending serverCommands
	SVQ3_WriteServerCommandsToClient(client, &msg);

	// send over all the relevant entityState_t
	// and the playerState_t
	SVQ3_WriteSnapshotToClient(client, &msg);

	// SV_WriteDownloadToClient(client, &msg);

	// end of message marker
	MSG_WriteBits(&msg, svcq3_eom, 8);

	SVQ3_Netchan_Transmit(client, msg.cursize, msg.data);
}




client_t *SVQ3_FindEmptyPlayerSlot(void)
{
	extern cvar_t maxclients;
	int pcount = 0;
	int i;
	for (i = 0; i < sv.allocated_client_slots; i++)
	{
		if (svs.clients[i].state)
			pcount++;
	}
	//in q3, spectators are not special
	if (pcount >= maxclients.value)
		return NULL;
	for (i = 0; i < sv.allocated_client_slots; i++)
	{
		if (!svs.clients[i].state)
			return &svs.clients[i];
	}
	return NULL;
}
client_t *SVQ3_FindExistingPlayerByIP(netadr_t *na, int qport)
{
	int i;
	for (i = 0; i < sv.allocated_client_slots; i++)
	{
		if (svs.clients[i].state && NET_CompareAdr(&svs.clients[i].netchan.remote_address, na))
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
	bitmask = (serverid ^ lastSequence ^ client->challenge) & 0xff;
	string = client->server_commands[lastServerCommandNum & TEXTCMD_MASK];

#ifndef Q3_NOENCRYPT
	// decrypt the packet
	for(i=msg_readcount+12,j=0; i<net_message.cursize; i++,j++)
	{
		if(!string[j])
		{
			j = 0; // another way around
		}
		c = string[j];
		if(c > 127 || c == '%')
		{
			c = '.';
		}
		bitmask ^= c << (i & 1);
		net_message.data[i] ^= bitmask;
	}
#endif

	return true;
}

void SVQ3_Netchan_Transmit(client_t *client, int length, qbyte *data)
{
	qbyte		buffer[MAX_OVERALLMSGLEN];
	qbyte		bitmask;
	qbyte		c;
	int			i, j;
	char		*string;

	// calculate bitmask
	bitmask = (client->netchan.outgoing_sequence ^ client->challenge) & 0xff;
	string = client->last_client_command;

#ifndef Q3_NOENCRYPT
	//first four bytes are not encrypted.
	for(i=0; i<4 ; i++)
		buffer[i] = data[i];
	// encrypt the packet
	for(j=0 ; i<length ; i++, j++)
	{
		if(!string[j])
		{
			j = 0; // another way around
		}
		c = string[j];
		if (c > 127 || c == '%')
		{
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
	Netchan_TransmitQ3(&client->netchan, length, buffer);
}

int StringKey(const char *string, int length);
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

	if(delta)
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

	if(client->state < cs_connected)
		return; // was dropped

	// calculate key for usercmd decryption
	string = client->server_commands[client->last_server_command_num & TEXTCMD_MASK];
	key = client->last_sequence ^ fs_key ^ StringKey(string, 32);

	// read delta sequenced usercmds
	from = &nullcmd;
	from->servertime = client->lastcmd.servertime;
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
		if (svs.gametype == GT_QUAKE3)
			SVQ3_ClientBegin(client);
		else
		{
			sv_player = host_client->edict;
			SV_Begin_Core(client);
		}
		client->state = cs_spawned;

		client->lastcmd.servertime = sv.time*1000;
		break;
	case cs_spawned:
		// run G_ClientThink() on each usercmd
		if (svs.gametype != GT_QUAKE3)
		{
			sv_player = host_client->edict;
			SV_PreRunCmd();
		}
		for(i=0,to=commands; i<cmdCount; i++, to++)
		{
			if(to->servertime <= client->lastcmd.servertime)
			{
				continue;
			}
			if (to->servertime-10 > sv.time*1000)	//10 ms allows some server latency...
			{
				Con_Printf("ignoring command from the future...\n");
				continue;
			}

			memcpy(&client->lastcmd, to, sizeof(client->lastcmd));
			if (svs.gametype == GT_QUAKE3)
				SVQ3_ClientThink(client);
			else if (svs.gametype == GT_PROGS || svs.gametype == GT_Q1QVM)
			{
				usercmd_t temp;
				temp = client->lastcmd;
#ifdef warningmsg
#pragma warningmsg("qwoverq3: you need to be aware of this if you're making a compatible cgame")
#endif
				//if you read the q3 code, you'll see that the speed value used is 64 for walking, and 127 for running (full speed).
				//so we map full to full here.
				temp.sidemove *= client->maxspeed/127.0f;
				temp.forwardmove *= client->maxspeed/127.0f;
				temp.upmove *= client->maxspeed/127.0f;

				temp.buttons &= ~2;
				if (temp.buttons & 64)
					temp.buttons |= 2;
				SV_RunCmd(&temp, false);
			}
		}
		if (svs.gametype != GT_QUAKE3)
			SV_PostRunCmd();
		break;
	default:
		break; // outdated usercmd packet
	}

}

void SVQ3_UpdateUserinfo_f(client_t *cl)
{
	Q_strncpyz(cl->userinfo, Cmd_Argv(1), sizeof(cl->userinfo));

	SV_ExtractFromUserinfo (cl, true);

	if (svs.gametype == GT_QUAKE3)
		VM_Call(q3gamevm, GAME_CLIENT_USERINFO_CHANGED, (int)(cl-svs.clients));
}

static void SVQ3_Drop_f(client_t *cl)
{
	SV_DropClient(cl);
}

//part of the sv_pure mechanism. verifies the client's pack list and kicks if they're wrong.
//safe to ignore, if you're okay with potential cheats.
static void SVQ3_ClientPacks_f(client_t *cl)
{
}

static void SVQ3_Download_f(client_t *cl)
{
	//clients might end up waiting for the download which will never come.
	//kick them so that doesn't happen. downloads are not supported at this time. not even reporting failure! :s
	SV_DropClient(cl);
//	short 0
//	long -1
}
static void SVQ3_NextDL_f(client_t *cl)
{
	//send next chunk
}
static void SVQ3_StopDL_f(client_t *cl)
{
	//abort/close current download, if any
}
static void SVQ3_DoneDL_f(client_t *cl)
{
	//send new gamestate
}

typedef struct ucmd_s
{
	char	*name;
	void	(*func)(client_t *);
} ucmd_t;

static const ucmd_t ucmds[] =
{
	{"userinfo",		SVQ3_UpdateUserinfo_f},
	{"disconnect",		SVQ3_Drop_f},

	{"cp",				SVQ3_ClientPacks_f},
	{"download",		SVQ3_Download_f},
	{"nextdl",			SVQ3_NextDL_f},
	{"stopdl",			SVQ3_StopDL_f},
	{"donedl",			SVQ3_DoneDL_f},

	{NULL,				NULL}
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
		Con_Printf("Client %s lost %i clientCommands\n", client->name, commandNum - client->last_client_command_num);
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

	if (svs.gametype == GT_QUAKE3)
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
			if (client->last_sequence - client->gamestatesequence < 100)
			{
				return; // don't resend gameState too frequently
			}

			Con_DPrintf("%s : dropped gamestate, resending\n", client->name);
		}
		client->lastcmd.servertime = sv.time*1000;
		SVQ3_SendGameState(client);
		return;
	}

	client->send_message = true;


	//
	// parse the message
	//
	while(1)
	{
		if(client->state < cs_connected)
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
qboolean SVQ3_HandleClient(void)
{
	int i;
	int qport;

	if (net_message.cursize<6)
		return false;	//urm. :/

	MSG_BeginReading(msg_nullnetprim);
	MSG_ReadBits(32);
	qport = (unsigned short)MSG_ReadBits(16);

	for (i = 0; i < sv.allocated_client_slots; i++)
	{
		if (svs.clients[i].state < cs_connected)
			continue;
		if (svs.clients[i].netchan.qport != qport)
			continue;
		if (!NET_CompareBaseAdr(&svs.clients[i].netchan.remote_address, &net_from))
			continue;
		if (!ISQ3CLIENT(&svs.clients[i]))
			continue;

		//found them.
		break;
	}
	if (i == sv.allocated_client_slots)
		return false;	//nope

	if (!SVQ3_Netchan_Process(&svs.clients[i]))
	{
		return true; // wasn't accepted for some reason
	}

	SVQ3_ParseClientMessage(&svs.clients[i]);
	return true;
}
void SVQ3_NewMapConnects(void)
{
	int i;
	qintptr_t ret;
	for (i = 0; i < sv.allocated_client_slots; i++)
	{
		if (svs.clients[i].state < cs_connected)
			continue;

		ret = VM_Call(q3gamevm, GAME_CLIENT_CONNECT, i, false, false);
		if (ret)
		{
			SV_DropClient(&svs.clients[i]);
		}
		else
		{
			//FIXME: make sure bots get the right GAME_CLIENT_BEGIN
		}
	}
}

void SVQ3_DirectConnect(void)	//Actually connect the client, use up a slot, and let the gamecode know of it.
{
	//this is only called when running q3 gamecode
	char *reason;
	client_t *cl;
	char *userinfo = NULL;
	qintptr_t ret;
	int challenge;
	int qport;
	char adr[MAX_ADR_SIZE];

	if (net_message.cursize < 13)
		return;
#ifdef HUFFNETWORK
	Huff_DecryptPacket(&net_message, 12);
#endif


	Cmd_TokenizeString((char*)net_message.data+4, false, false);
	userinfo = Cmd_Argv(1);
	qport = atoi(Info_ValueForKey(userinfo, "qport"));
	challenge = atoi(Info_ValueForKey(userinfo, "challenge"));

	cl = SVQ3_FindExistingPlayerByIP(&net_from, qport);	//use a duplicate first.
	if (!cl)
		cl = SVQ3_FindEmptyPlayerSlot();

#ifdef HUFFNETWORK
	if (!Huff_CompressionCRC(HUFFCRC_QUAKE3))
	{
		reason = "Could not set up compression.";
		userinfo = NULL;
	}
	else
#endif
		if (!cl)
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
			Q_strncpyz(cl->userinfo, userinfo, sizeof(cl->userinfo));
			reason = NET_AdrToString(adr, sizeof(adr), &net_from);
			Info_SetValueForStarKey(cl->userinfo, "ip", reason, sizeof(cl->userinfo));

			ret = VM_Call(q3gamevm, GAME_CLIENT_CONNECT, (int)(cl-svs.clients), false, false);
			if (!ret)
				reason = NULL;
			else
				reason = (char*)VM_MemoryBase(q3gamevm)+ret;
		}
	}

	if (reason)
	{
		Con_Printf("%s\n", reason);
		reason = va("\377\377\377\377print\n%s", reason);
		NET_SendPacket (NS_SERVER, strlen(reason), reason, &net_from);
		return;
	}

	cl->protocol = SCP_QUAKE3;
	cl->state = cs_connected;
	cl->name = cl->namebuf;
	cl->team = cl->teambuf;
	SV_ExtractFromUserinfo(cl, true);
	Netchan_Setup(NS_SERVER, &cl->netchan, &net_from, qport);
	cl->netchan.outgoing_sequence = 1;

	cl->challenge = challenge;
	cl->userid = (cl - svs.clients)+1;

	cl->gamestatesequence = -1;

	NET_SendPacket (NS_SERVER, 19, "\377\377\377\377connectResponse", &net_from);

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
	memset(&cl->netchan.remote_address, 0, sizeof(cl->netchan.remote_address));

	return cl - svs.clients;
}

void SVQ3_DropClient(client_t *cl)
{
	if (q3gamevm)
		VM_Call(q3gamevm, GAME_CLIENT_DISCONNECT, (int)(cl-svs.clients));
}

#endif
