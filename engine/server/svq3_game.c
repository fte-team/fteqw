#include "quakedef.h"

#ifdef Q3SERVER

#include "clq3defs.h"
#include "q3g_public.h"

vm_t *q3gamevm;


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

void SVQ3_CreateBaseline(void);

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

	if(!ent->r.linked)
		return;		// not linked in anywhere

	sent = SENTITY_FOR_GENTITY(ent);
	if (sent->area.next)
		RemoveLink(&sent->area);
	sent->area.prev = sent->area.next = NULL;

	ent->r.linked = false;
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

	if(ent->r.linked)
		Q3G_UnlinkEntity(ent);	// unlink from old position

	// encode the size into the entity_state for client prediction
	if(ent->r.bmodel)
	{
		ent->s.solid = Q3SOLID_BMODEL;
	}
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

	//origin = (ent->r.svFlags & SVF_USE_CURRENT_ORIGIN) ? ent->r.currentOrigin : ent->s.origin;
	//angles = (ent->r.svFlags & SVF_USE_CURRENT_ORIGIN) ? ent->r.currentAngles : ent->s.angles;

	// FIXME - always use currentOrigin?
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

	sent = SENTITY_FOR_GENTITY(ent);

// link to PVS leafs
	sent->num_clusters = 0;
	sent->areanum = -1;
	sent->areanum2 = -1;

	//get all leafs, including solids
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
	InsertLinkBefore((link_t *)sent, &node->solid_edicts);
}


int SVQ3_EntitiesInBoxNode(areanode_t *node, vec3_t mins, vec3_t maxs, int *list, int maxcount)
{
	link_t		*l, *next;
	q3serverEntity_t		*sent;
	q3sharedEntity_t		*gent;

	int linkcount = 0, ln;
	float d1, d2;

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

#define	ENTITYNUM_WORLD		(MAX_GENTITIES-2)
void SVQ3_Trace(q3trace_t *result, vec3_t start, vec3_t mins, vec3_t maxs, vec3_t end, int entnum, int contentmask)
{
	int contactlist[128];
	trace_t tr;
	vec3_t mmins, mmaxs;
	int i, contactcount;
	q3entityShared_t *es;
	model_t *mod;

	if (!mins)
		mins = vec3_origin;
	if (!maxs)
		maxs = vec3_origin;

	sv.worldmodel->funcs.Trace(sv.worldmodel, 0, 0, start, end, mins, maxs, &tr);
	result->allsolid = tr.allsolid;
	result->contents = tr.contents;
	VectorCopy(tr.endpos, result->endpos);
	result->entityNum = ENTITYNUM_WORLD;
	result->fraction = tr.fraction;
	result->plane = tr.plane;
	result->startsolid = tr.startsolid;
//	if (tr.surface)
//		result->surfaceFlags = tr.surface->flags;
//	else
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

	for (i = SVQ3_EntitiesInBox(mmins, mmaxs, contactlist, sizeof(contactlist)/sizeof(contactlist[0]))-1; i >= 0; i--)
	{
		if (contactlist[i] == entnum)
			continue;	//don't collide with self.

		es = GENTITY_FOR_NUM(contactlist[i]);
		if (es->bmodel)
			mod = Mod_ForName(va("*%i", es->s.modelindex), true);
		else
			mod = CM_TempBoxModel(es->mins, es->maxs);
		tr = CM_TransformedBoxTrace(mod, start, mins, mmins, mmaxs, contentmask, es->currentOrigin, es->currentAngles);
		if (tr.fraction < result->fraction)
		{
			result->allsolid = tr.allsolid;
			result->contents = tr.contents;
			VectorCopy(tr.endpos, result->endpos);
			result->entityNum = contactlist[i];
			result->fraction = tr.fraction;
			result->plane = tr.plane;
			result->startsolid = tr.startsolid;
//			if (tr.surface)
//				result->surfaceFlags = tr.surface->flags;
//			else
				result->surfaceFlags = 0;
		}
	}
}

int SVQ3_Contact(vec3_t mins, vec3_t maxs, q3entityShared_t *ent)
{
/*	model_t *mod;
	vec3_t org;
	trace_t tr;

	VectorSubtract(maxs, mins, org);
	VectorMA(mins, 0.5, org, org);
	mod = Mod_ForName(va("*%i", ent->s.modelindex), false);
	mod->funcs.Trace(mod, 0, 0, org, org, mins, maxs, &tr);

	if (tr.startsolid)*/
		return true;
//	return false;
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
	Q_strncpyz(cl->server_commands[cl->num_server_commands], str, sizeof(cl->server_commands[0]));
}

void SVQ3_SetConfigString(int num, char *string)
{
	if (svq3_configstrings[num])
		Z_Free(svq3_configstrings[num]);
	svq3_configstrings[num] = Z_Malloc(strlen(string)+1);
	strcpy(svq3_configstrings[num], string);

	SVQ3_SendServerCommand( NULL, va("cs %i \"%s\"\n", num, string));
}

#define VALIDATEPOINTER(o,l) if ((int)o + l >= mask || VM_POINTER(o) < offset) SV_Error("Call to game trap %i passes invalid pointer\n", fn);	//out of bounds.
long Q3G_SystemCallsEx(void *offset, unsigned int mask, int fn, const long *arg)
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
		{
			vmcvar_t *vmc;
			cvar_t *var;
			vmc = VM_POINTER(arg[0]);
			var = Cvar_Get(VM_POINTER(arg[1]), VM_POINTER(arg[2]), 0/*VM_LONG(arg[3])*/, "Q3-Game-Code created");
			if (!vmc)	//qvm doesn't need to retreive it
				break;
			vmc->handle = (char *)var - (char *)offset;

			vmc->integer = var->value;
			vmc->value = var->value;
			vmc->modificationCount = var->modified;
			Q_strncpyz(vmc->string, var->string, sizeof(vmc->string));
		}
		break;
	case G_CVAR_UPDATE:// ( vmCvar_t *vmCvar );
		VALIDATEPOINTER(arg[0], sizeof(vmcvar_t));
		{
			cvar_t *var;
			vmcvar_t *vmc;
			vmc = VM_POINTER(arg[0]);
			var = (cvar_t *)((int)vmc->handle + (char *)offset);
			if (!var || !vmc->handle)
				return false;

			vmc->integer = var->value;
			vmc->value = var->value;
			vmc->modificationCount = var->modified;
			Q_strncpyz(vmc->string, var->string, sizeof(vmc->string));
		}
		break;

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

	case G_ARGC:		//8
		return Cmd_Argc();
	case G_ARGV:				//9
		VALIDATEPOINTER(arg[1], arg[2]);
		Q_strncpyz(VM_POINTER(arg[1]), Cmd_Argv(VM_LONG(arg[0])), VM_LONG(arg[2]));
		break;

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

	case G_GET_USERINFO://int num, char *buffer, int bufferSize										20
		if (VM_OOB(arg[1], arg[2]))
			return 0;
		Q_strncpyz(VM_POINTER(arg[1]), svs.clients[VM_LONG(arg[0])].userinfo, VM_LONG(arg[2]));
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
		VALIDATEPOINTER(arg[2], sizeof(int*)*VM_LONG(arg[3]));
		return SVQ3_EntitiesInBox(VM_POINTER(arg[0]), VM_POINTER(arg[1]), VM_POINTER(arg[2]), VM_LONG(arg[3]));
		break;
	case G_POINT_CONTENTS:
		return sv.worldmodel->funcs.PointContents(sv.worldmodel, VM_POINTER(arg[0]));
		break;
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

	case G_REAL_TIME:																			//	42
Con_Printf("builtin %i is not implemented\n", fn);
		return 0;
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
		memset(VM_POINTER(arg[0]), arg[1], arg[2]);
		break;
	case G_MEMCPY:
		VALIDATEPOINTER(arg[0], arg[2]);
		memmove(VM_POINTER(arg[0]), VM_POINTER(arg[1]), arg[2]);
		break;
	case G_STRNCPY:
		VALIDATEPOINTER(arg[0], arg[2]);
		Q_strncpyS(VM_POINTER(arg[0]), VM_POINTER(arg[1]), arg[2]);
		break;
	case G_SIN:
		VM_FLOAT(ret)=(float)sin(VM_FLOAT(arg[0]));
		break;
	case G_COS:
		VM_FLOAT(ret)=(float)cos(VM_FLOAT(arg[0]));
		break;
	case G_ACOS:
		VM_FLOAT(ret)=(float)acos(VM_FLOAT(arg[0]));
		break;
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

	default:
		Con_Printf("builtin %i is not implemented\n", fn);
	}
	return ret;
}


int EXPORT_FN Q3G_SystemCalls(int arg, ...)
{
	long args[9];
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
	va_end(argptr);

	return Q3G_SystemCallsEx(NULL, ~0, arg, args);
}

void SVQ3_ShutdownGame(void)
{
	int i;
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
}

qboolean SVQ3_InitGame(void)
{
	int i;
	//clear out the configstrings
	for (i = 0; i < MAX_CONFIGSTRINGS; i++)
	{
		if (svq3_configstrings[i])
		{
			Z_Free(svq3_configstrings[i]);
			svq3_configstrings[i] = NULL;
		}
	}

	q3gamevm = VM_Create(NULL, "vm/qagame", Q3G_SystemCalls, Q3G_SystemCallsEx);

	if (!q3gamevm)
		return false;

	SV_ClearWorld();

	q3_sentities = Z_Malloc(sizeof(q3serverEntity_t)*MAX_GENTITIES);

	svq3_configstrings[0] = Z_Malloc(strlen(svs.info)+1 + 9+strlen(sv.name)+16);
	strcpy(svq3_configstrings[0], svs.info);
	Info_SetValueForKey(svq3_configstrings[0], "mapname", sv.name, MAX_SERVERINFO_STRING); 

	svq3_configstrings[1] = Z_Malloc(32);
	Info_SetValueForKey(svq3_configstrings[1], "sv_serverid", va("%i", svs.spawncount), MAX_SERVERINFO_STRING); 

	mapentspointer = sv.worldmodel->entities;
	VM_Call(q3gamevm, GAME_INIT, 0, rand(), false);

	SVQ3_CreateBaseline();

	q3_num_snapshot_entities = 32 * Q3UPDATE_BACKUP * 32;
	if (q3_snapshot_entities)
		BZ_Free(q3_snapshot_entities);
	q3_next_snapshot_entities = 0;
	q3_snapshot_entities = BZ_Malloc(sizeof( q3entityState_t ) * q3_num_snapshot_entities);

	return true;
}

void SVQ3_RunFrame(void)
{
	VM_Call(q3gamevm, GAME_RUN_FRAME, sv.framenum*100);
}

void SVQ3_ClientCommand(client_t *cl)
{
	VM_Call(q3gamevm, GAME_CLIENT_COMMAND, cl-svs.clients);
}

void SVQ3_ClientBegin(client_t *cl)
{
	VM_Call(q3gamevm, GAME_CLIENT_BEGIN, cl-svs.clients);
}

void SVQ3_ClientThink(client_t *cl)
{
	VM_Call(q3gamevm, GAME_CLIENT_THINK, cl-svs.clients);
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
			newnum = 99999;
		}
		else
		{
			newent = &q3_snapshot_entities[(to->first_entity + newindex) % q3_num_snapshot_entities];
			newnum = newent->number;
		}

		if(oldindex >= from_num_entities)
		{
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
	snap = &client->q3frames[client->netchan.outgoing_sequence & Q3UPDATE_MASK];

	if(client->state < cs_spawned)
	{
		// not fully in game yet
		delta = 0;
		oldsnap = NULL;
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
		oldsnap = &client->q3frames[client->delta_sequence & Q3UPDATE_MASK];

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
	MSG_WriteBits(msg, sv.framenum*100, 32);
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
//	MSG_WriteBits( msg, ENTITYNUM_NONE, GENTITYNUM_BITS );

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
	snap = &client->q3frames[client->netchan.outgoing_sequence & Q3UPDATE_MASK];

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
	bitvector = sv.worldmodel->funcs.LeafPVS(sv.worldmodel, sv.worldmodel->funcs.LeafnumForPoint(sv.worldmodel, org), clientarea);
	clientarea = CM_LeafArea(sv.worldmodel, clientarea);
/*
	if( client->areanum != clientarea ) {
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

	
	/*
	for (i = 0; i < snap->areabytes;i++)
	{	//fix areabits, q2->q3 style..
		snap->areabits[i]^=255;
	}
	*/
}


//writes initial gamestate
void SVQ3_SendGameState(client_t *client)
{
	sizebuf_t		msg;
	char			buffer[MAX_OVERALLMSGLEN];
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
		memcpy(to, from, sizeof(*to));
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
			if(to->servertime <= client->lastcmd.servertime )
				continue;

			memcpy( &client->lastcmd, to, sizeof(client->lastcmd));
			SVQ3_ClientThink(client);
		}
		break;
	default:
		break; // outdated usercmd packet
	}

}

void SVQ3_UpdateUserinfo_f(void)
{
	Q_strncpyz( host_client->userinfo, Cmd_Argv(1), sizeof(host_client->userinfo) );

	SV_ExtractFromUserinfo (host_client);

	VM_Call(q3gamevm, GAME_CLIENT_USERINFO_CHANGED, host_client-svs.clients);
}


typedef struct ucmd_s {
	char	*name;
	void	(*func)( client_t * );
} ucmd_t;

static const ucmd_t ucmds[] = {
	{ "userinfo",		SVQ3_UpdateUserinfo_f},
	{ "disconnect",		NULL},//SV_Disconnect_f },

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

	Con_Printf("ClientCommand %i: %s\n", commandNum, buffer);

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
	int c, last;

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
			SV_DropClient(client);
			return;
		}

		last = c;
		c = MSG_ReadBits(8);
		if (c == clcq3_eom)
		{
			break;
		}
				
		switch(c)
		{
		default:
			Con_Printf("corrupted packet from %s\n", client->name);
			SV_DropClient(client);
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
		Con_Printf( S_COLOR_YELLOW"WARNING: Junk at end of packet for client %s\n", client->name );
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

void SVQ3_DirectConnect(void)	//Actually connect the client, use up a slot, and let the gamecode know of it.
{
	char *reason;
	client_t *cl;
	char *userinfo = NULL;
	int ret;
	int challenge = 0;
	if (net_message.cursize < 13)
		return;
	Huff_DecryptPacket(&net_message, 12);

	cl = SVQ3_FindEmptyPlayerSlot();

	if (!cl)
	{
		reason = "Server is full.";
		userinfo = NULL;
	}
	else
	{
		if (cl->q3frames)
			BZ_Free(cl->q3frames);
		memset(cl, 0, sizeof(*cl));
		Cmd_TokenizeString(net_message.data+4, false, false);
		userinfo = Cmd_Argv(1);
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
			reason = NET_AdrToString(net_from);
			Info_SetValueForStarKey(cl->userinfo, "ip", reason, sizeof(cl->userinfo));

			ret = VM_Call(q3gamevm, GAME_CLIENT_CONNECT, cl-svs.clients, false, false);
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
	Netchan_Setup(NS_SERVER, &cl->netchan, net_from, atoi(Info_ValueForKey(userinfo, "qport")));
	cl->netchan.outgoing_sequence = 1;

	cl->challenge = challenge;
	cl->userid = (cl - svs.clients)+1;

	cl->gamestatesequence = -1;

	NET_SendPacket (NS_SERVER, 19, "\377\377\377\377connectResponse", net_from);

	Huff_PreferedCompressionCRC();

	cl->q3frames = BZ_Malloc(Q3UPDATE_BACKUP*sizeof(*cl->q3frames));
}

void SVQ3_DropClient(client_t *cl)
{
	if (q3gamevm)
		VM_Call(q3gamevm, GAME_CLIENT_DISCONNECT, cl-svs.clients);
}

#endif
