#include "qwsvdef.h"

/*
I think globals.maxentities is the hard cap, rather than current max like in q1.
*/

#ifdef HLSERVER

#include "svhl_gcapi.h"

#include "crc.h"
#include "model_hl.h"

#define ignore(s) Con_Printf("Fixme: " s "\n")
#define notimp(l) Con_Printf("halflife sv builtin not implemented on line %i\n", l)


dllhandle_t *hlgamecode;
SVHL_Globals_t SVHL_Globals;
SVHL_GameFuncs_t SVHL_GameFuncs;

#define MAX_HL_EDICTS 2048
hledict_t *SVHL_Edict;
int SVHL_NumActiveEnts;

int lastusermessage;




string_t GHL_AllocString(char *string)
{
	char *news;
	news = Hunk_Alloc(strlen(string)+1);
	memcpy(news, string, strlen(string)+1);
	return news - SVHL_Globals.stringbase;
}
int GHL_PrecacheModel(char *name)
{
	int		i;

	if (name[0] <= ' ')
	{
		Con_Printf ("precache_model: empty string\n");
		return 0;
	}

	for (i=1 ; i<MAX_MODELS ; i++)
	{
		if (!sv.strings.model_precache[i])
		{
			if (strlen(name)>=MAX_QPATH-1)	//probably safest to keep this.
			{
				SV_Error ("Precache name too long");
				return 0;
			}
			name = sv.strings.model_precache[i] = SVHL_Globals.stringbase+GHL_AllocString(name);

			if (!strcmp(name + strlen(name) - 4, ".bsp"))
				sv.models[i] = Mod_FindName(name);

			if (sv.state != ss_loading)
			{
				MSG_WriteByte(&sv.reliable_datagram, svcfte_precache);
				MSG_WriteShort(&sv.reliable_datagram, i);
				MSG_WriteString(&sv.reliable_datagram, name);
#ifdef NQPROT
				MSG_WriteByte(&sv.nqreliable_datagram, svcdp_precache);
				MSG_WriteShort(&sv.nqreliable_datagram, i);
				MSG_WriteString(&sv.nqreliable_datagram, name);
#endif
			}

			return i;
		}
		if (!strcmp(sv.strings.model_precache[i], name))
		{
			return i;
		}
	}
	SV_Error ("GHL_precache_model: overflow");
	return 0;
}
int GHL_PrecacheSound(char *name)
{
	int		i;

	if (name[0] <= ' ')
	{
		Con_Printf ("precache_sound: empty string\n");
		return 0;
	}

	for (i=1 ; i<MAX_SOUNDS ; i++)
	{
		if (!*sv.strings.sound_precache[i])
		{
			if (strlen(name)>=MAX_QPATH-1)	//probably safest to keep this.
			{
				SV_Error ("Precache name too long");
				return 0;
			}
			strcpy(sv.strings.sound_precache[i], name);
			name = sv.strings.sound_precache[i];

			if (sv.state != ss_loading)
			{
				MSG_WriteByte(&sv.reliable_datagram, svcfte_precache);
				MSG_WriteShort(&sv.reliable_datagram, -i);
				MSG_WriteString(&sv.reliable_datagram, name);
#ifdef NQPROT
				MSG_WriteByte(&sv.nqreliable_datagram, svcdp_precache);
				MSG_WriteShort(&sv.nqreliable_datagram, -i);
				MSG_WriteString(&sv.nqreliable_datagram, name);
#endif
			}

			return i;
		}
		if (!strcmp(sv.strings.sound_precache[i], name))
		{
			return i;
		}
	}
	SV_Error ("GHL_precache_sound: overflow");
	return 0;
}
void GHL_SetModel(hledict_t *ed, char *modelname)
{
	model_t *mod;
	int mdlidx = GHL_PrecacheModel(modelname);
	ed->v.modelindex = mdlidx;
	ed->v.model = sv.strings.model_precache[mdlidx] - SVHL_Globals.stringbase;

	mod = sv.models[mdlidx];
	if (mod)
	{
		VectorCopy(mod->mins, ed->v.mins);
		VectorCopy(mod->maxs, ed->v.maxs);
		VectorSubtract(mod->maxs, mod->mins, ed->v.size);
	}
	SVHL_LinkEdict(ed, false);
}
unk GHL_ModelIndex(unk){notimp(__LINE__);}
int GHL_ModelFrames(int midx)
{
	//returns the number of frames(sequences I assume) this model has
	ignore("ModelFrames");
	return 1;
}
void GHL_SetSize(hledict_t *ed, float *mins, float *maxs)
{
	VectorCopy(mins, ed->v.mins);
	VectorCopy(maxs, ed->v.maxs);
	SVHL_LinkEdict(ed, false);
}
void GHL_ChangeLevel(char *nextmap, char *startspot)
{
	Cbuf_AddText(va("changelevel %s %s@%f@%f@%f\n", nextmap, startspot, SVHL_Globals.landmark[0], SVHL_Globals.landmark[1], SVHL_Globals.landmark[2]), RESTRICT_PROGS);
}
unk GHL_GetSpawnParms(unk){notimp(__LINE__);}
unk GHL_SaveSpawnParms(unk){notimp(__LINE__);}
float GHL_VecToYaw(float *inv)
{
	vec3_t outa;

	VectorAngles(inv, NULL, outa);
	return outa[1];
}
void GHL_VecToAngles(float *inv, float *outa)
{
	VectorAngles(inv, NULL, outa);
}
unk GHL_MoveToOrigin(unk){notimp(__LINE__);}
unk GHL_ChangeYaw(unk){notimp(__LINE__);}
unk GHL_ChangePitch(unk){notimp(__LINE__);}
hledict_t *GHL_FindEntityByString(hledict_t *last, char *field, char *value)
{
	hledict_t *ent;
	int i;
	int ofs;
	string_t str;
	if (!strcmp(field, "targetname"))
		ofs = (char*)&((hledict_t *)NULL)->v.targetname - (char*)NULL;
	else if (!strcmp(field, "classname"))
		ofs = (char*)&((hledict_t *)NULL)->v.classname - (char*)NULL;
	else
	{
		Con_Printf("Fixme: Look for %s=%s\n", field, value);
		return NULL;
	}
	
	if (last)
		i=last-SVHL_Edict+1;
	else
		i = 0;
	for (; i<SVHL_Globals.maxentities; i++)
	{
		ent = &SVHL_Edict[i];
		if (ent->isfree)
			continue;
		str = *(string_t*)((char*)ent+ofs);
		if (str && !strcmp(SVHL_Globals.stringbase+str, value))
			return ent;
	}
	return SVHL_Edict;
}
unk GHL_GetEntityIllum(unk){notimp(__LINE__);}
hledict_t *GHL_FindEntityInSphere(hledict_t *last, float *org, float radius)
{
	int i, j;
	vec3_t eorg;
	hledict_t *ent;

	radius = radius*radius;

	if (last)
		i=last-SVHL_Edict+1;
	else
		i = 0;
	for (; i<SVHL_Globals.maxentities; i++)
	{
		ent = &SVHL_Edict[i];
		if (ent->isfree)
			continue;
		if (!ent->v.solid)
			continue;
		for (j=0; j<3; j++)
			eorg[j] = org[j] - (ent->v.origin[j] + (ent->v.mins[j] + ent->v.maxs[j])*0.5);
		if (DotProduct(eorg,eorg) > radius)
			continue;

		//its close enough
		return ent;
	}
	return NULL;
}
hledict_t *GHL_FindClientInPVS(hledict_t *ed)
{
	qbyte	*viewerpvs;
	int best = 0, i;
	float bestdist = 99999999;	//HL maps are limited in size anyway
	float d;
	int leafnum;
	vec3_t ofs;
	hledict_t *other;

	//fixme: we need to track some state
	//a different client should be returned each call _per ent_ (so it can be used once per frame)

	viewerpvs = sv.worldmodel->funcs.LeafPVS(sv.worldmodel, sv.worldmodel->funcs.LeafnumForPoint(sv.worldmodel, ed->v.origin), NULL);

	for (i = 0; i < MAX_CLIENTS; i++)
	{
		if (svs.clients[i].state == cs_spawned)
		{
			other = &SVHL_Edict[i+1];
			if (ed == other)
				continue;	//ignore yourself.
			if (svs.clients[i].spectator)
				continue;	//ignore spectators

			leafnum = sv.worldmodel->funcs.LeafnumForPoint(sv.worldmodel, other->v.origin)-1;/*pvs is 1 based, leafs are 0 based*/
			if (viewerpvs[leafnum>>3] & (1<<(leafnum&7)))
			{
				VectorSubtract(ed->v.origin, other->v.origin, ofs);
				d = DotProduct(ofs, ofs);

				if (d < bestdist)
				{
					bestdist = d;
					best = i+1;
				}
			}
		}
	}

	if (best)
		return &SVHL_Edict[best];
	return NULL;
}
unk GHL_EntitiesInPVS(unk){notimp(__LINE__);}
void GHL_MakeVectors(float *angles)
{
	AngleVectors(angles, SVHL_Globals.v_forward, SVHL_Globals.v_right, SVHL_Globals.v_up);
}
void GHL_AngleVectors(float *angles, float *forward, float *right, float *up)
{
	AngleVectors(angles, forward, right, up);
}

///////////////////////////////////////////////////////////

hledict_t *GHL_CreateEntity(void)
{	
	int i;
	static int spawnnumber;
	spawnnumber++;

	for (i = sv.allocated_client_slots; i < SVHL_NumActiveEnts; i++)
	{
		if (SVHL_Edict[i].isfree)
		{
			if (SVHL_Edict[i].freetime > sv.time)
				continue;

			memset(&SVHL_Edict[i], 0, sizeof(SVHL_Edict[i]));
			SVHL_Edict[i].spawnnumber = spawnnumber;
			SVHL_Edict[i].v.edict = &SVHL_Edict[i];
			return &SVHL_Edict[i];
		}
	}
	if (i < MAX_HL_EDICTS)
	{
		SVHL_NumActiveEnts++;
		memset(&SVHL_Edict[i], 0, sizeof(SVHL_Edict[i]));
		SVHL_Edict[i].spawnnumber = spawnnumber;
		SVHL_Edict[i].v.edict = &SVHL_Edict[i];
		return &SVHL_Edict[i];
	}
	SV_Error("Ran out of free edicts");
	return NULL;
}
void GHL_RemoveEntity(hledict_t *ed)
{
	SVHL_UnlinkEdict(ed);
	ed->isfree = true;
	ed->freetime = sv.time+2;
}
hledict_t *GHL_CreateNamedEntity(string_t name)
{
	void (*spawnfunc)(hlentvars_t *evars);
	hledict_t *ed;
	ed = GHL_CreateEntity();
	if (!ed)
		return NULL;
	ed->v.classname = name;

	spawnfunc = (void(*)(hlentvars_t*))GetProcAddress((HINSTANCE)hlgamecode, name+SVHL_Globals.stringbase);
	if (spawnfunc)
		spawnfunc(&ed->v);
	return ed;
}
void *GHL_PvAllocEntPrivateData(hledict_t *ed, long quant)
{
	ed->moddata = Z_Malloc(quant);
	return ed->moddata;
}
unk GHL_PvEntPrivateData(unk)
{
	notimp(__LINE__);
}
unk GHL_FreeEntPrivateData(unk)
{
	notimp(__LINE__);
}
unk GHL_GetVarsOfEnt(unk)
{
	notimp(__LINE__);
}
hledict_t *GHL_PEntityOfEntOffset(int ednum)
{
	return (hledict_t *)(ednum + (char*)SVHL_Edict);
}
int GHL_EntOffsetOfPEntity(hledict_t *ed)
{
	return (char*)ed - (char*)SVHL_Edict;
}
int GHL_IndexOfEdict(hledict_t *ed)
{
	return ed - SVHL_Edict;
}
hledict_t *GHL_PEntityOfEntIndex(int idx)
{
	return &SVHL_Edict[idx];
}
unk GHL_FindEntityByVars(unk)
{
	notimp(__LINE__);
}

///////////////////////////////////////////////////////

unk GHL_MakeStatic(unk){notimp(__LINE__);}
unk GHL_EntIsOnFloor(unk){notimp(__LINE__);}
int GHL_DropToFloor(hledict_t *ed)
{
	vec3_t top;
	vec3_t bottom;
	trace_t tr;
	VectorCopy(ed->v.origin, top);
	VectorCopy(ed->v.origin, bottom);
	top[2] += 1;
	bottom[2] -= 256;
	tr = SVHL_Move(top, ed->v.mins, ed->v.maxs, bottom, 0, 0, ed);
	VectorCopy(tr.endpos, ed->v.origin);
	return tr.fraction != 0 && tr.fraction != 1;
}
int GHL_WalkMove(hledict_t *ed, float yaw, float dist, int mode)
{
	ignore("walkmove");
	return 1;
}
void GHL_SetOrigin(hledict_t *ed, float *neworg)
{
	VectorCopy(neworg, ed->v.origin);
	SVHL_LinkEdict(ed, false);
}
void GHL_EmitSound(hledict_t *ed, int chan, char *soundname, float vol, float atten, int flags, int pitch)
{
	SV_StartSound(ed-SVHL_Edict, ed->v.origin, ~0, chan, soundname, vol*255, atten);
}
void GHL_EmitAmbientSound(hledict_t *ed, float *org, char *soundname, float vol, float atten, unsigned int flags, int pitch)
{
	SV_StartSound(0, org, ~0, 0, soundname, vol*255, atten);
}
void GHL_TraceLine(float *start, float *end, int flags, hledict_t *ignore, hltraceresult_t *result)
{
	trace_t t;
	
	t = SVHL_Move(start, vec3_origin, vec3_origin, end, flags, 0, ignore);

	result->allsolid = t.allsolid;
	result->startsolid = t.startsolid;
	result->inopen = t.inopen;
	result->inwater = t.inwater;
	result->fraction = t.fraction;
	VectorCopy(t.endpos, result->endpos);
	result->planedist = t.plane.dist;
	VectorCopy(t.plane.normal, result->planenormal);
	result->touched = t.ent;
	if (!result->touched)
		result->touched = &SVHL_Edict[0];
	result->hitgroup = 0;
}
unk GHL_TraceToss(unk){notimp(__LINE__);}
unk GHL_TraceMonsterHull(unk){notimp(__LINE__);}
void GHL_TraceHull(float *start, float *end, int flags, int hullnum, hledict_t *ignore, hltraceresult_t *result)
{
	trace_t t;

	t = SVHL_Move(start, sv.worldmodel->hulls[hullnum].clip_mins, sv.worldmodel->hulls[hullnum].clip_maxs, end, flags, 0, ignore);

	result->allsolid = t.allsolid;
	result->startsolid = t.startsolid;
	result->inopen = t.inopen;
	result->inwater = t.inwater;
	result->fraction = t.fraction;
	VectorCopy(t.endpos, result->endpos);
	result->planedist = t.plane.dist;
	VectorCopy(t.plane.normal, result->planenormal);
	result->touched = t.ent;
	result->hitgroup = 0;
}
unk GHL_TraceModel(unk){notimp(__LINE__);}
char *GHL_TraceTexture(hledict_t *againstent, vec3_t start, vec3_t end)
{
	trace_t tr;
	sv.worldmodel->funcs.Trace(sv.worldmodel, 0, 0, start, end, vec3_origin, vec3_origin, &tr);
	return tr.surface->name;
}
unk GHL_TraceSphere(unk){notimp(__LINE__);}
unk GHL_GetAimVector(unk){notimp(__LINE__);}
void GHL_ServerCommand(char *cmd)
{
	Cbuf_AddText(cmd, RESTRICT_PROGS);
}
void GHL_ServerExecute(void)
{
	Cbuf_ExecuteLevel(RESTRICT_PROGS);
}
unk GHL_ClientCommand(unk){notimp(__LINE__);}
unk GHL_ParticleEffect(unk){notimp(__LINE__);}
void GHL_LightStyle(int stylenum, char *stylestr)
{
	PF_applylightstyle(stylenum, stylestr, 7);
}
int GHL_DecalIndex(char *decalname)
{
	Con_Printf("Fixme: precache decal %s\n", decalname);
	return 0;
}
int GHL_PointContents(float *org)
{
	return Q1CONTENTS_EMPTY;
}

int svhl_messagedest;
vec3_t svhl_messageorigin;
hledict_t *svhl_messageent;
void GHL_MessageBegin(int dest, int type, float *org, hledict_t *ent)
{
	svhl_messagedest = dest;
	if (org)
		VectorCopy(org, svhl_messageorigin);
	else
		VectorClear(svhl_messageorigin);
	svhl_messageent = ent;

	if (sv.multicast.cursize)
	{
		Con_Printf("MessageBegin called without MessageEnd\n");
		SZ_Clear (&sv.multicast);
	}
	MSG_WriteByte(&sv.multicast, svcfte_cgamepacket);
	MSG_WriteShort(&sv.multicast, 0);
	MSG_WriteByte(&sv.multicast, type);
}
void GHL_MessageEnd(unk)
{
	unsigned short len;
	client_t *cl;

	if (!sv.multicast.cursize)
	{
		Con_Printf("MessageEnd called without MessageBegin\n");
		return;
	}

	//update the length
	len = sv.multicast.cursize - 3;
	sv.multicast.data[1] = len&0xff;
	sv.multicast.data[2] = len>>8;

	switch(svhl_messagedest)
	{
	case MSG_BROADCAST:
		SZ_Write(&sv.datagram, sv.multicast.data, sv.multicast.cursize);
		break;
	case MSG_ONE:
		cl = &svs.clients[svhl_messageent - SVHL_Edict - 1];
		if (cl->state >= cs_spawned)
		{
			ClientReliableCheckBlock(cl, sv.multicast.cursize);
			ClientReliableWrite_SZ(cl, sv.multicast.data, sv.multicast.cursize);
			ClientReliable_FinishWrite(cl);
		}
		break;
	case MSG_ALL:
		SZ_Write(&sv.reliable_datagram, sv.multicast.data, sv.multicast.cursize);
		break;
	case MSG_MULTICAST:
		SV_Multicast(svhl_messageorigin, MULTICAST_PVS);
		break;
	case MSG_MULTICAST+1:
		SV_Multicast(svhl_messageorigin, MULTICAST_PHS);
		break;
	default:
		Con_Printf("GHL_MessageEnd: dest type %i not supported\n", svhl_messagedest);
		break;
	}

	SZ_Clear (&sv.multicast);
}
void GHL_WriteByte(int value)
{
	MSG_WriteByte(&sv.multicast, value);
}
void GHL_WriteChar(int value)
{
	MSG_WriteChar(&sv.multicast, value);
}
void GHL_WriteShort(int value)
{
	MSG_WriteShort(&sv.multicast, value);
}
void GHL_WriteLong(int value)
{
	MSG_WriteLong(&sv.multicast, value);
}
void GHL_WriteAngle(float value)
{
	MSG_WriteAngle8(&sv.multicast, value);
}
void GHL_WriteCoord(float value)
{
	coorddata i = MSG_ToCoord(value, 2);
	SZ_Write (&sv.multicast, (void*)&i, 2);
}
void GHL_WriteString(char *string)
{
	MSG_WriteString(&sv.multicast, string);
}
void GHL_WriteEntity(int entnum)
{
	MSG_WriteShort(&sv.multicast, entnum);
}


void GHL_AlertMessage(int level, char *fmt, ...)
{
	va_list		argptr;
	char		string[1024];

	va_start (argptr, fmt);
	vsnprintf (string,sizeof(string)-1, fmt,argptr);
	va_end (argptr);

	Con_Printf("%s\n", string);
}
void GHL_EngineFprintf(FILE *f, char *fmt, ...)
{
	SV_Error("Halflife gamecode tried to use EngineFprintf\n");
}
unk GHL_SzFromIndex(unk){notimp(__LINE__);}
void *GHL_GetModelPtr(hledict_t *ed)
{
#ifdef SERVERONLY
	return NULL;
#else
	if (!ed->v.modelindex)
		return NULL;
	if (!sv.models[ed->v.modelindex])
		sv.models[ed->v.modelindex] = Mod_ForName(sv.strings.model_precache[ed->v.modelindex], false);
	return Mod_GetHalfLifeModelData(sv.models[ed->v.modelindex]);
#endif
}
int GHL_RegUserMsg(char *msgname, int msgsize)
{
	//we use 1 as the code to choose others.
	if (lastusermessage <= 1)
		return -1;

	SV_FlushSignon ();

	//for new clients
	MSG_WriteByte(&sv.signon, svcfte_cgamepacket);
	MSG_WriteShort(&sv.signon, strlen(msgname)+3);
	MSG_WriteByte(&sv.signon, 1);
	MSG_WriteByte(&sv.signon, lastusermessage);
	MSG_WriteString(&sv.signon, msgname);

	//and if the client is already spawned...
	MSG_WriteByte(&sv.reliable_datagram, svcfte_cgamepacket);
	MSG_WriteShort(&sv.reliable_datagram, strlen(msgname)+3);
	MSG_WriteByte(&sv.reliable_datagram, 1);
	MSG_WriteByte(&sv.reliable_datagram, lastusermessage);
	MSG_WriteString(&sv.reliable_datagram, msgname);

	return lastusermessage--;
}
unk GHL_AnimationAutomove(unk){notimp(__LINE__);}
unk GHL_GetBonePosition(unk){notimp(__LINE__);}

hlintptr_t GHL_FunctionFromName(char *name)
{
	return (hlintptr_t)Sys_GetAddressForName(hlgamecode, name);
}
char *GHL_NameForFunction(hlintptr_t function)
{
	return Sys_GetNameForAddress(hlgamecode, (void*)function);
}

unk GHL_ClientPrintf(unk)
{
//	SV_ClientPrintf(
	notimp(__LINE__);
}
void GHL_ServerPrint(char *msg)
{
	Con_Printf("%s", msg);	
}
char *GHL_Cmd_Args(void)
{
	return Cmd_Args();
}
char *GHL_Cmd_Argv(int arg)
{
	return Cmd_Argv(arg);
}
int GHL_Cmd_Argc(unk)
{
	return Cmd_Argc();
}
unk GHL_GetAttachment(unk){notimp(__LINE__);}
void GHL_CRC32_Init(hlcrc_t *crc)
{
	unsigned short crc16 = *crc;
	QCRC_Init(&crc16);
	*crc = crc16;
}
void GHL_CRC32_ProcessBuffer(hlcrc_t *crc, qbyte *p, int len)
{
	unsigned short crc16 = *crc;
	while(len-->0)
	{
		QCRC_ProcessByte(&crc16, *p++);
	}
	*crc = crc16;
}
void GHL_CRC32_ProcessByte(hlcrc_t *crc, qbyte b)
{
	unsigned short crc16 = *crc;
	QCRC_ProcessByte(&crc16, b);
	*crc = crc16;
}
hlcrc_t GHL_CRC32_Final(hlcrc_t crc)
{
	unsigned short crc16 = crc;
	return QCRC_Value(crc16);
}
long GHL_RandomLong(long minv, long maxv)
{
	return minv + frandom()*(maxv-minv);
}
float GHL_RandomFloat(float minv, float maxv)
{
	return minv + frandom()*(maxv-minv);
}
unk GHL_SetView(unk){notimp(__LINE__);}
unk GHL_Time(unk){notimp(__LINE__);}
unk GHL_CrosshairAngle(unk){notimp(__LINE__);}
void *GHL_LoadFileForMe(char *name, int *size_out)
{
	int fsize;
	void *fptr;
	fsize = FS_LoadFile(name, &fptr);
	if (size_out)
		*size_out = fsize;
	if (fsize == -1)
		return NULL;
	return fptr;
}
void GHL_FreeFile(void *fptr)
{
	FS_FreeFile(fptr);
}
unk GHL_EndSection(unk){notimp(__LINE__);}
#include <sys/stat.h>
int GHL_CompareFileTime(char *fname1, char *fname2, int *result)
{
	flocation_t loc1, loc2;
	struct stat stat1, stat2;
	//results:
	//1 = f1 is newer
	//0 = equal age
	//-1 = f2 is newer
	//at least I think that's what it means.
	*result = 0;
	if (!FS_FLocateFile(fname1, FSLFRT_IFFOUND, &loc1) || !FS_FLocateFile(fname2, FSLFRT_IFFOUND, &loc2))
		return 0;

	if (stat(loc1.rawname, &stat1) || stat(loc2.rawname, &stat2))
		return 0;

	if (stat1.st_mtime > stat2.st_mtime)
		*result = 1;
	else if (stat1.st_mtime < stat2.st_mtime)
		*result = -1;
	else
		*result = 0;

	return 1;
}
void GHL_GetGameDir(char *gamedir)
{
	extern char gamedirfile[];
	//warning: the output buffer size is not specified!
	Q_strncpyz(gamedir, gamedirfile, MAX_QPATH);
}
unk GHL_Cvar_RegisterVariable(unk){notimp(__LINE__);}
unk GHL_FadeClientVolume(unk){notimp(__LINE__);}
unk GHL_SetClientMaxspeed(unk)
{
	notimp(__LINE__);
}
unk GHL_CreateFakeClient(unk){notimp(__LINE__);}
unk GHL_RunPlayerMove(unk){notimp(__LINE__);}
int GHL_NumberOfEntities(void)
{
	return 0;
}
char *GHL_GetInfoKeyBuffer(hledict_t *ed)
{
	if (!ed)
		return svs.info;

	return svs.clients[ed - SVHL_Edict - 1].userinfo;
}
char *GHL_InfoKeyValue(char *infostr, char *key)
{
	return Info_ValueForKey(infostr, key);
}
unk GHL_SetKeyValue(unk){notimp(__LINE__);}
unk GHL_SetClientKeyValue(unk){notimp(__LINE__);}
unk GHL_IsMapValid(unk){notimp(__LINE__);}
unk GHL_StaticDecal(unk){notimp(__LINE__);}
unk GHL_PrecacheGeneric(unk){notimp(__LINE__);}
int GHL_GetPlayerUserId(hledict_t *ed)
{
	unsigned int clnum = (ed - SVHL_Edict) - 1;
	if (clnum >= sv.allocated_client_slots)
		return -1;
	return svs.clients[clnum].userid;
}
unk GHL_BuildSoundMsg(unk){notimp(__LINE__);}

int GHL_IsDedicatedServer(void)
{
#ifdef SERVERONLY
	return 1;
#else
	return qrenderer == QR_NONE;
#endif
}

hlcvar_t *hlcvar_malloced;
hlcvar_t *hlcvar_stored;
void SVHL_UpdateCvar(cvar_t *var)
{
	if (!var->hlcvar)
		return;	//nothing to do
	var->hlcvar->string = var->string;
	var->hlcvar->value = var->value;
}

void SVHL_FreeCvars(void)
{
	cvar_t *nc;
	hlcvar_t *n;
	//forget all
	while (hlcvar_malloced)
	{
		n = hlcvar_malloced;
		hlcvar_malloced = n->next;

		nc = Cvar_FindVar(n->name);
		if (nc)
			nc->hlcvar = NULL;
		Z_Free(n);
	}

	while (hlcvar_stored)
	{
		n = hlcvar_stored;
		hlcvar_stored = n->next;

		nc = Cvar_FindVar(n->name);
		if (nc)
			nc->hlcvar = NULL;
	}
}
void SVHL_FreeCvar(hlcvar_t *var)
{
	cvar_t *nc;
	hlcvar_t **ref;
	//unlink (free if it was malloced)

	ref = &hlcvar_malloced;
	while (*ref)
	{
		if (*ref == var)
		{
			(*ref) = (*ref)->next;

			nc = Cvar_FindVar(var->name);
			if (nc)
				nc->hlcvar = NULL;
			Z_Free(var);
			return;
		}
		ref = &(*ref)->next;
	}

	ref = &hlcvar_stored;
	while (*ref)
	{
		if (*ref == var)
		{
			(*ref) = (*ref)->next;

			nc = Cvar_FindVar(var->name);
			if (nc)
				nc->hlcvar = NULL;
			return;
		}
		ref = &(*ref)->next;
	}
}

hlcvar_t *GHL_CVarGetPointer(char *varname)
{
	cvar_t *var;
	hlcvar_t *hlvar;
	var = Cvar_Get(varname, "", 0, "HalfLife");
	if (!var)
	{
		Con_Printf("Not giving cvar \"%s\" to game\n", varname);
		return NULL;
	}
	hlvar = var->hlcvar;
	if (!hlvar)
	{
		hlvar = var->hlcvar = Z_Malloc(sizeof(hlcvar_t));
		hlvar->name = var->name;
		hlvar->string = var->string;
		hlvar->value = var->value;

		hlvar->next = hlcvar_malloced;
		hlcvar_malloced = hlvar;
	}
	return hlvar;
}
void GHL_CVarRegister(hlcvar_t *hlvar)
{
	cvar_t *var;
	var = Cvar_Get(hlvar->name, hlvar->string, 0, "HalfLife");
	if (!var)
	{
		Con_Printf("Not giving cvar \"%s\" to game\n", hlvar->name);
		return;
	}
	if (var->hlcvar)
	{
		SVHL_FreeCvar(var->hlcvar);

		hlvar->next = hlcvar_stored;
		hlcvar_stored = hlvar;
	}
	var->hlcvar = hlvar;
}
float GHL_CVarGetFloat(char *vname)
{
	cvar_t *var = Cvar_FindVar(vname);
	if (var)
		return var->value;
	Con_Printf("cvar %s does not exist\n", vname);
	return 0;
}
char *GHL_CVarGetString(char *vname)
{
	cvar_t *var = Cvar_FindVar(vname);
	if (var)
		return var->string;
	Con_Printf("cvar %s does not exist\n", vname);
	return "";
}
void GHL_CVarSetFloat(char *vname, float value)
{
	cvar_t *var = Cvar_FindVar(vname);
	if (var)
		Cvar_SetValue(var, value);
	else
		Con_Printf("cvar %s does not exist\n", vname);
}
void GHL_CVarSetString(char *vname, char *value)
{
	cvar_t *var = Cvar_FindVar(vname);
	if (var)
		Cvar_Set(var, value);
	else
		Con_Printf("cvar %s does not exist\n", vname);
}

unk GHL_GetPlayerWONId(unk){notimp(__LINE__);}
unk GHL_Info_RemoveKey(unk){notimp(__LINE__);}
unk GHL_GetPhysicsKeyValue(unk){notimp(__LINE__);}
unk GHL_SetPhysicsKeyValue(unk){notimp(__LINE__);}
unk GHL_GetPhysicsInfoString(unk){notimp(__LINE__);}
unsigned short GHL_PrecacheEvent(int eventtype, char *eventname)
{
	Con_Printf("Fixme: GHL_PrecacheEvent: %s\n", eventname);
	return 0;
}
void GHL_PlaybackEvent(int flags, hledict_t *ent, unsigned short eventidx, float delay, float *origin, float *angles, float f1, float f2, int i1, int i2, int b1, int b2)
{
	ignore("GHL_PlaybackEvent not implemented");
}
unk GHL_SetFatPVS(unk){notimp(__LINE__);}
unk GHL_SetFatPAS(unk){notimp(__LINE__);}
unk GHL_CheckVisibility(unk){notimp(__LINE__);}
unk GHL_DeltaSetField(unk){notimp(__LINE__);}
unk GHL_DeltaUnsetField(unk){notimp(__LINE__);}
unk GHL_DeltaAddEncoder(unk){notimp(__LINE__);}
unk GHL_GetCurrentPlayer(unk){notimp(__LINE__);}
unk GHL_CanSkipPlayer(unk){notimp(__LINE__);}
unk GHL_DeltaFindField(unk){notimp(__LINE__);}
unk GHL_DeltaSetFieldByIndex(unk){notimp(__LINE__);}
unk GHL_DeltaUnsetFieldByIndex(unk){notimp(__LINE__);}
unk GHL_SetGroupMask(unk){notimp(__LINE__);}
unk GHL_CreateInstancedBaseline(unk){notimp(__LINE__);}
unk GHL_Cvar_DirectSet(unk){notimp(__LINE__);}
unk GHL_ForceUnmodified(unk){notimp(__LINE__);}
unk GHL_GetPlayerStats(unk){notimp(__LINE__);}
unk GHL_AddServerCommand(unk){notimp(__LINE__);}
unk GHL_Voice_GetClientListening(unk){notimp(__LINE__);}
qboolean GHL_Voice_SetClientListening(int listener, int sender, int shouldlisten)
{
	return false;
}
unk GHL_GetPlayerAuthId(unk){notimp(__LINE__);}
unk GHL_SequenceGet(unk){notimp(__LINE__);}
unk GHL_SequencePickSentence(unk){notimp(__LINE__);}
unk GHL_GetFileSize(unk){notimp(__LINE__);}
unk GHL_GetApproxWavePlayLen(unk){notimp(__LINE__);}
unk GHL_IsCareerMatch(unk){notimp(__LINE__);}
unk GHL_GetLocalizedStringLength(unk){notimp(__LINE__);}
unk GHL_RegisterTutorMessageShown(unk){notimp(__LINE__);}
unk GHL_GetTimesTutorMessageShown(unk){notimp(__LINE__);}
unk GHL_ProcessTutorMessageDecayBuffer(unk){notimp(__LINE__);}
unk GHL_ConstructTutorMessageDecayBuffer(unk){notimp(__LINE__);}
unk GHL_ResetTutorMessageDecayData(unk){notimp(__LINE__);}
unk GHL_QueryClientCvarValue(unk){notimp(__LINE__);}
unk GHL_QueryClientCvarValue2(unk){notimp(__LINE__);}




//====================================================================================================





SVHL_Builtins_t SVHL_Builtins =
{
	GHL_PrecacheModel,
	GHL_PrecacheSound,
	GHL_SetModel,
	GHL_ModelIndex,
	GHL_ModelFrames,
	GHL_SetSize,
	GHL_ChangeLevel,
	GHL_GetSpawnParms,
	GHL_SaveSpawnParms,
	GHL_VecToYaw,
	GHL_VecToAngles,
	GHL_MoveToOrigin,
	GHL_ChangeYaw,
	GHL_ChangePitch,
	GHL_FindEntityByString,
	GHL_GetEntityIllum,
	GHL_FindEntityInSphere,
	GHL_FindClientInPVS,
	GHL_EntitiesInPVS,
	GHL_MakeVectors,
	GHL_AngleVectors,
	GHL_CreateEntity,
	GHL_RemoveEntity,
	GHL_CreateNamedEntity,
	GHL_MakeStatic,
	GHL_EntIsOnFloor,
	GHL_DropToFloor,
	GHL_WalkMove,
	GHL_SetOrigin,
	GHL_EmitSound,
	GHL_EmitAmbientSound,
	GHL_TraceLine,
	GHL_TraceToss,
	GHL_TraceMonsterHull,
	GHL_TraceHull,
	GHL_TraceModel,
	GHL_TraceTexture,
	GHL_TraceSphere,
	GHL_GetAimVector,
	GHL_ServerCommand,
	GHL_ServerExecute,
	GHL_ClientCommand,
	GHL_ParticleEffect,
	GHL_LightStyle,
	GHL_DecalIndex,
	GHL_PointContents,
	GHL_MessageBegin,
	GHL_MessageEnd,
	GHL_WriteByte,
	GHL_WriteChar,
	GHL_WriteShort,
	GHL_WriteLong,
	GHL_WriteAngle,
	GHL_WriteCoord,
	GHL_WriteString,
	GHL_WriteEntity,
	GHL_CVarRegister,
	GHL_CVarGetFloat,
	GHL_CVarGetString,
	GHL_CVarSetFloat,
	GHL_CVarSetString,
	GHL_AlertMessage,
	GHL_EngineFprintf,
	GHL_PvAllocEntPrivateData,
	GHL_PvEntPrivateData,
	GHL_FreeEntPrivateData,
	GHL_SzFromIndex,
	GHL_AllocString,
	GHL_GetVarsOfEnt,
	GHL_PEntityOfEntOffset,
	GHL_EntOffsetOfPEntity,
	GHL_IndexOfEdict,
	GHL_PEntityOfEntIndex,
	GHL_FindEntityByVars,
	GHL_GetModelPtr,
	GHL_RegUserMsg,
	GHL_AnimationAutomove,
	GHL_GetBonePosition,
	GHL_FunctionFromName,
	GHL_NameForFunction,
	GHL_ClientPrintf,
	GHL_ServerPrint,
	GHL_Cmd_Args,
	GHL_Cmd_Argv,
	GHL_Cmd_Argc,
	GHL_GetAttachment,
	GHL_CRC32_Init,
	GHL_CRC32_ProcessBuffer,
	GHL_CRC32_ProcessByte,
	GHL_CRC32_Final,
	GHL_RandomLong,
	GHL_RandomFloat,
	GHL_SetView,
	GHL_Time,
	GHL_CrosshairAngle,
	GHL_LoadFileForMe,
	GHL_FreeFile,
	GHL_EndSection,
	GHL_CompareFileTime,
	GHL_GetGameDir,
	GHL_Cvar_RegisterVariable,
	GHL_FadeClientVolume,
	GHL_SetClientMaxspeed,
	GHL_CreateFakeClient,
	GHL_RunPlayerMove,
	GHL_NumberOfEntities,
	GHL_GetInfoKeyBuffer,
	GHL_InfoKeyValue,
	GHL_SetKeyValue,
	GHL_SetClientKeyValue,
	GHL_IsMapValid,
	GHL_StaticDecal,
	GHL_PrecacheGeneric,
	GHL_GetPlayerUserId,
	GHL_BuildSoundMsg,
	GHL_IsDedicatedServer,
#if HALFLIFE_API_VERSION > 138
	GHL_CVarGetPointer,
	GHL_GetPlayerWONId,
	GHL_Info_RemoveKey,
	GHL_GetPhysicsKeyValue,
	GHL_SetPhysicsKeyValue,
	GHL_GetPhysicsInfoString,
	GHL_PrecacheEvent,
	GHL_PlaybackEvent,
	GHL_SetFatPVS,
	GHL_SetFatPAS,
	GHL_CheckVisibility,
	GHL_DeltaSetField,
	GHL_DeltaUnsetField,
	GHL_DeltaAddEncoder,
	GHL_GetCurrentPlayer,
	GHL_CanSkipPlayer,
	GHL_DeltaFindField,
	GHL_DeltaSetFieldByIndex,
	GHL_DeltaUnsetFieldByIndex,
	GHL_SetGroupMask,
	GHL_CreateInstancedBaseline,
	GHL_Cvar_DirectSet,
	GHL_ForceUnmodified,
	GHL_GetPlayerStats,
	GHL_AddServerCommand,
	GHL_Voice_GetClientListening,
	GHL_Voice_SetClientListening,
	GHL_GetPlayerAuthId,
	GHL_SequenceGet,
	GHL_SequencePickSentence,
	GHL_GetFileSize,
	GHL_GetApproxWavePlayLen,
	GHL_IsCareerMatch,
	GHL_GetLocalizedStringLength,
	GHL_RegisterTutorMessageShown,
	GHL_GetTimesTutorMessageShown,
	GHL_ProcessTutorMessageDecayBuffer,
	GHL_ConstructTutorMessageDecayBuffer,
	GHL_ResetTutorMessageDecayData,
	GHL_QueryClientCvarValue,
	GHL_QueryClientCvarValue2, 
#endif

	0xdeadbeef
};

void SV_ReadLibListDotGam(void)
{
	char key[1024];
	char value[1024];
	char *file;
	char *s;

	Info_SetValueForStarKey(svs.info, "*gamedll", "", sizeof(svs.info));
	Info_SetValueForStarKey(svs.info, "*cldll", "", sizeof(svs.info));

	file = COM_LoadTempFile("liblist.gam");
	if (!file)
		return;

	Info_SetValueForStarKey(svs.info, "*cldll", "1", sizeof(svs.info));

	while ((file = COM_ParseOut(file, key, sizeof(key))))
	{
		file = COM_ParseOut(file, value, sizeof(value));

		while((s = strchr(value, '\\')))
			*s = '/';

		if (!strcmp(key, "gamedll"
#ifdef __linux__
			"_linux"
#endif
			))
			Info_SetValueForStarKey(svs.info, "*gamedll", value, sizeof(svs.info));
		if (!strcmp(key, "cldll"))
			Info_SetValueForStarKey(svs.info, "*cldll", atoi(value)?"1":"", sizeof(svs.info));
	}
}

int SVHL_InitGame(void)
{
	char *gamedll;
	char *path;
	char fullname[MAX_OSPATH];
	void (*GiveFnptrsToDll) (funcs, globals);
	int (*GetEntityAPI)(SVHL_GameFuncs_t *pFunctionTable, int apivers);

	dllfunction_t hlgamefuncs[] =
	{
		{(void**)&GiveFnptrsToDll, "GiveFnptrsToDll"},
		{(void**)&GetEntityAPI, 	"GetEntityAPI"},
		{NULL, NULL}
	};

	if (sizeof(long) != sizeof(void*))
	{
		Con_Printf("sizeof(long)!=sizeof(ptr): Cannot run halflife gamecode on this platform\n");
		return 0;
	}

	SV_ReadLibListDotGam();

	if (hlgamecode)
	{
		SVHL_Edict = Hunk_Alloc(sizeof(*SVHL_Edict) * MAX_HL_EDICTS);
		SVHL_Globals.maxentities = MAX_HL_EDICTS;	//I think this is correct
		return 1;
	}

	gamedll = Info_ValueForKey(svs.info, "*gamedll");
	path = NULL;
	while((path = COM_NextPath (path)))
	{
		if (!path)
			return 0;		// couldn't find one anywhere
		snprintf (fullname, sizeof(fullname), "%s/%s", path, gamedll);
		hlgamecode = Sys_LoadLibrary(fullname, hlgamefuncs);
		if (hlgamecode)
			break;
	}

	if (!hlgamecode)
		return 0;

	SVHL_Edict = Hunk_Alloc(sizeof(*SVHL_Edict) * MAX_HL_EDICTS);
	SVHL_Globals.maxentities = MAX_HL_EDICTS;	//I think this is correct
	GiveFnptrsToDll(&SVHL_Builtins, &SVHL_Globals);

	if (!GetEntityAPI(&SVHL_GameFuncs, HALFLIFE_API_VERSION))
	{
		Con_Printf(CON_ERROR "Error: %s is incompatible\n", fullname);
		Sys_CloseLibrary(hlgamecode);
		return 0;
	}

	SVHL_GameFuncs.GameDLLInit();
	return 1;
}

void SVHL_SaveLevelCache(void)
{

}

void SVHL_SetupGame(void)
{
	lastusermessage = 255;
	//called on new map
}

void SVHL_SpawnEntities(char *entstring)
{
	char key[256];
	char value[1024];
	char classname[1024];
	hlfielddef_t fdef;
	hledict_t *ed, *world;
	extern cvar_t	coop;	//who'd have thought it, eh?
	char *ts;
	int i;

	//initialise globals
	SVHL_Globals.stringbase = "";
	SVHL_Globals.maxclients = MAX_CLIENTS;
	SVHL_Globals.deathmatch = deathmatch.value;
	SVHL_Globals.coop = coop.value;
	SVHL_Globals.serverflags = 0;
	SVHL_Globals.mapname = GHL_AllocString(sv.name);

	SVHL_NumActiveEnts = 0;


	//touch world.
	world = GHL_CreateNamedEntity(GHL_AllocString("worldspawn"));
	world->v.solid = SOLID_BSP;
	GHL_SetModel(world, sv.modelname);

	//spawn player ents
	sv.allocated_client_slots = 0;
	for (i = 0; i < SVHL_Globals.maxclients; i++)
	{
		sv.allocated_client_slots++;
		ed = GHL_CreateNamedEntity(GHL_AllocString("player"));
	}
	for (i = 0; i < sv.allocated_client_slots; i++)
	{
		SVHL_Edict[i].isfree = true;
	}
	sv.allocated_client_slots = i;

	//precache the inline models (and touch them).
	sv.strings.model_precache[0] = "";
	sv.strings.model_precache[1] = sv.modelname;	//the qvm doesn't have access to this array
	for (i=1 ; i<sv.worldmodel->numsubmodels ; i++)
	{
		sv.strings.model_precache[1+i] = localmodels[i];
		sv.models[i+1] = Mod_ForName (localmodels[i], false);
	}

	while (entstring)
	{
		entstring = COM_ParseOut(entstring, key, sizeof(key));
		if (strcmp(key, "{"))
			break;

		*classname = 0;

		ts = entstring;
		while (ts)
		{
			ts = COM_ParseOut(ts, key, sizeof(key));
			if (!strcmp(key, "}"))
				break;
			ts = COM_ParseOut(ts, value, sizeof(value));

			if (!strcmp(key, "classname"))
			{
				memcpy(classname, value, strlen(value)+1);
				break;
			}
		}

		if (world)
		{
			if (strcmp(classname, "worldspawn"))
				SV_Error("first entity is not worldspawn");
			ed = world;
			world = NULL;
		}
		else
			ed = GHL_CreateNamedEntity(GHL_AllocString(classname));

		while (entstring)
		{
			entstring = COM_ParseOut(entstring, key, sizeof(key));
			if (!strcmp(key, "}"))
				break;
			entstring = COM_ParseOut(entstring, value, sizeof(value));

			if (*key == '_')
				continue;

			if (!strcmp(key, "classname"))
				memcpy(classname, value, strlen(value)+1);

			fdef.handled = false;
			if (!*classname)
				fdef.classname = NULL;
			else
				fdef.classname = classname;
			fdef.key = key;
			fdef.value = value;
			SVHL_GameFuncs.DispatchKeyValue(ed, &fdef);
			if (!fdef.handled)
			{
				if (!strcmp(key, "angle"))
				{
					float a = atof(value);
					sprintf(value, "%f %f %f", 0.0f, a, 0.0f);
					strcpy(key, "angles");
					SVHL_GameFuncs.DispatchKeyValue(ed, &fdef);
				}
				if (!fdef.handled)
					Con_Printf("Bad field on %s, %s\n", classname, key);
			}
		}
		SVHL_GameFuncs.DispatchSpawn(ed);
	}

	SVHL_GameFuncs.ServerActivate(SVHL_Edict, SVHL_NumActiveEnts, sv.allocated_client_slots);
}

void SVHL_ShutdownGame(void)
{
	SVHL_FreeCvars();

	//gametype changed, or server shutdown
	Sys_CloseLibrary(hlgamecode);
	hlgamecode = NULL;

	SVHL_Edict = NULL;
	memset(&SVHL_Globals, 0, sizeof(SVHL_Globals));
	memset(&SVHL_GameFuncs, 0, sizeof(SVHL_GameFuncs));
	memset(&SVHL_GameFuncsEx, 0, sizeof(SVHL_GameFuncsEx));
}

qboolean HLSV_ClientCommand(client_t *client)
{
	hledict_t *ed = &SVHL_Edict[client - svs.clients + 1];
	if (!hlgamecode)
		return false;
	SVHL_GameFuncs.ClientCommand(ed);
	return true;
}

qboolean SVHL_ClientConnect(client_t *client, netadr_t adr, char rejectmessage[128])
{
	char ipadr[256];
	NET_AdrToString(ipadr, sizeof(ipadr), adr);
	strcpy(rejectmessage, "Rejected by gamecode");

	if (!SVHL_GameFuncs.ClientConnect(&SVHL_Edict[client-svs.clients+1], client->name, ipadr, rejectmessage))
		return false;

	return true;
}

void SVHL_BuildStats(client_t *client, int *si, float *sf, char **ss)
{
	hledict_t *ed = &SVHL_Edict[client - svs.clients + 1];

	si[STAT_HEALTH] = ed->v.health;
	si[STAT_VIEWHEIGHT] = ed->v.view_ofs[2];
	si[STAT_WEAPON] = SV_ModelIndex(SVHL_Globals.stringbase+ed->v.vmodelindex);
	si[STAT_ITEMS] = ed->v.weapons;
}

void SVHL_PutClientInServer(client_t *client)
{
	hledict_t *ed = &SVHL_Edict[client - svs.clients + 1];
	ed->isfree = false;
	SVHL_GameFuncs.ClientPutInServer(&SVHL_Edict[client-svs.clients+1]);
}

void SVHL_DropClient(client_t *drop)
{
	hledict_t *ed = &SVHL_Edict[drop - svs.clients + 1];
	SVHL_GameFuncs.ClientDisconnect(&SVHL_Edict[drop-svs.clients+1]);
	ed->isfree = true;
}

void SVHL_RunCmdR(hledict_t *ed, usercmd_t *ucmd)
{
	int i;
	hledict_t *other;
extern cvar_t temp1;
extern vec3_t	player_mins;
extern vec3_t	player_maxs;

	// chop up very long commands
	if (ucmd->msec > 50)
	{
		usercmd_t cmd = *ucmd;

		cmd.msec = ucmd->msec/2;
		SVHL_RunCmdR (ed, &cmd);
		cmd.msec = ucmd->msec/2 + (ucmd->msec&1);	//give them back their msec.
		cmd.impulse = 0;
		SVHL_RunCmdR (ed, &cmd);
		return;
	}

	host_frametime = ucmd->msec * 0.001;
	host_frametime *= sv.gamespeed;
	if (host_frametime > 0.1)
		host_frametime = 0.1;

	pmove.cmd = *ucmd;
	pmove.pm_type = temp1.value;//PM_NORMAL;//FLY;
	pmove.numphysent = 1;
	pmove.physents[0].model = sv.worldmodel;
	pmove.physents[0].info = 0;

	if (ed->v.flags & (1<<24))
	{
		pmove.cmd.forwardmove = 0;
		pmove.cmd.sidemove = 0;
		pmove.cmd.upmove = 0;
	}

	{
		hledict_t *list[256];
		int count;
		physent_t *pe;
		vec3_t	pmove_mins, pmove_maxs;

		for (i = 0; i < 3; i++)
		{
			pmove_mins[i] = pmove.origin[i] - 256;
			pmove_maxs[i] = pmove.origin[i] + 256;
		}

		count = SVHL_AreaEdicts(pmove_mins, pmove_maxs, list, sizeof(list)/sizeof(list[0]));
		for (i = 0; i < count; i++)
		{
			other = list[i];
			if (other == ed)
				continue;
			if (other->v.owner == ed)
				continue;
			if (other->v.flags & (1<<23))	//has monsterclip flag set, so ignore it
				continue;

			pe = &pmove.physents[pmove.numphysent];
			if (other->v.modelindex)
			{
				pe->model = sv.models[other->v.modelindex];
				if (pe->model && pe->model->type != mod_brush)
					pe->model = NULL;
			}
			else
				pe->model = NULL;
			pmove.numphysent++;
			pe->info = other - SVHL_Edict;
			VectorCopy(other->v.origin, pe->origin);
			VectorCopy(other->v.mins, pe->mins);
			VectorCopy(other->v.maxs, pe->maxs);
			VectorCopy(other->v.angles, pe->angles);

			if (other->v.solid == SOLID_NOT || other->v.solid == SOLID_TRIGGER)
				pe->nonsolid = true;
			else
				pe->nonsolid = false;

			switch(other->v.skin)
			{
			case Q1CONTENTS_EMPTY:
				pe->forcecontentsmask = FTECONTENTS_EMPTY;
				break;
			case Q1CONTENTS_SOLID:
				pe->forcecontentsmask = FTECONTENTS_SOLID;
				break;
			case Q1CONTENTS_WATER:
				pe->forcecontentsmask = FTECONTENTS_WATER;
				break;
			case Q1CONTENTS_SLIME:
				pe->forcecontentsmask = FTECONTENTS_SLIME;
				break;
			case Q1CONTENTS_LAVA:
				pe->forcecontentsmask = FTECONTENTS_LAVA;
				break;
			case Q1CONTENTS_SKY:
				pe->forcecontentsmask = FTECONTENTS_SKY;
				break;
			case -16:
				pe->forcecontentsmask = FTECONTENTS_LADDER;
				break;
			default:
				pe->forcecontentsmask = 0;
				break;
			}
		}
	}

	VectorCopy(ed->v.mins, player_mins);
	VectorCopy(ed->v.maxs, player_maxs);

	VectorCopy(ed->v.origin, pmove.origin);
	VectorCopy(ed->v.velocity, pmove.velocity);
	if (ed->v.flags & (1<<22))
	{
		VectorCopy(ed->v.basevelocity, pmove.basevelocity);
	}
	else
		VectorClear(pmove.basevelocity);

	PM_PlayerMove(sv.gamespeed);

	VectorCopy(pmove.origin, ed->v.origin);
	VectorCopy(pmove.velocity, ed->v.velocity);

	if (pmove.onground)
	{
		ed->v.flags |= FL_ONGROUND;
		ed->v.groundentity = &SVHL_Edict[pmove.physents[pmove.groundent].info];
	}
	else
		ed->v.flags &= ~FL_ONGROUND;

	for (i = 0; i < pmove.numtouch; i++)
	{
		other = &SVHL_Edict[pmove.physents[pmove.touchindex[i]].info];
		SVHL_GameFuncs.DispatchTouch(other, ed);
	}

	SVHL_LinkEdict(ed, true);
}

void SVHL_RunCmd(client_t *cl, usercmd_t *ucmd)
{
	hledict_t *ed = &SVHL_Edict[cl - svs.clients + 1];

#if HALFLIFE_API_VERSION >= 140
	ed->v.buttons = ucmd->buttons;
#else
	//assume they're not running halflife cgame
	ed->v.buttons = 0;

	if (ucmd->buttons & 1)
		ed->v.buttons |= (1<<0);	//shoot
	if (ucmd->buttons & 2)
		ed->v.buttons |= (1<<1);	//jump
	if (ucmd->buttons & 8)
		ed->v.buttons |= (1<<2);	//duck
	if (ucmd->forwardmove > 0)
		ed->v.buttons |= (1<<3);	//forward
	if (ucmd->forwardmove < 0)
		ed->v.buttons |= (1<<4);	//back
	if (ucmd->buttons & 4)
		ed->v.buttons |= (1<<5);	//use
	//ed->v.buttons |= (1<<6);	//cancel
	//ed->v.buttons |= (1<<7);	//turn left
	//ed->v.buttons |= (1<<8);	//turn right
	if (ucmd->sidemove > 0)
		ed->v.buttons |= (1<<9);	//move left
	if (ucmd->sidemove < 0)
		ed->v.buttons |= (1<<10);	//move right
	//ed->v.buttons |= (1<<11);	//shoot2
	//ed->v.buttons |= (1<<12);	//run
	if (ucmd->buttons & 16)
		ed->v.buttons |= (1<<13);	//reload
	//ed->v.buttons |= (1<<14);	//alt1
	//ed->v.buttons |= (1<<15);	//alt2
#endif

	if (ucmd->impulse)
		ed->v.impulse = ucmd->impulse;
	ed->v.v_angle[0] = SHORT2ANGLE(ucmd->angles[0]);
	ed->v.v_angle[1] = SHORT2ANGLE(ucmd->angles[1]);
	ed->v.v_angle[2] = SHORT2ANGLE(ucmd->angles[2]);

	ed->v.angles[0] = 0;
	ed->v.angles[1] = SHORT2ANGLE(ucmd->angles[1]);
	ed->v.angles[2] = SHORT2ANGLE(ucmd->angles[2]);

	SVHL_GameFuncs.PlayerPreThink(ed);
	SVHL_RunCmdR(ed, ucmd);

	if (ed->v.nextthink && ed->v.nextthink < sv.time)
	{
		ed->v.nextthink = 0;
		SVHL_GameFuncs.DispatchThink(ed);
	}

	SVHL_GameFuncs.PlayerPostThink(ed);
}


void SVHL_RunPlayerCommand(client_t *cl, usercmd_t *oldest, usercmd_t *oldcmd, usercmd_t *newcmd)
{
	hledict_t *e = &SVHL_Edict[cl - svs.clients + 1];

	SVHL_Globals.time = sv.time;
	if (net_drop < 20)
	{
		while (net_drop > 2)
		{
			SVHL_RunCmd (cl, &cl->lastcmd);
			net_drop--;
		}
		if (net_drop > 1)
			SVHL_RunCmd (cl, oldest);
		if (net_drop > 0)
			SVHL_RunCmd (cl, oldcmd);
	}
	SVHL_RunCmd (cl, newcmd);
}

void SVHL_Snapshot_Build(client_t *client, packet_entities_t *pack, qbyte *pvs, edict_t *clent, qboolean ignorepvs)
{
	hledict_t *e;
	entity_state_t *s;
	int i;

	pack->servertime = sv.time;
	pack->num_entities = 0;

	for (i = 1; i < MAX_HL_EDICTS; i++)
	{
		e = &SVHL_Edict[i];
		if (!e)
			break;
		if (e->isfree)
			continue;

		if (!e->v.modelindex || !e->v.model)
			continue;
		if (e->v.effects & 128)
			continue;

		if (pack->num_entities == pack->max_entities)
			break;

		s = &pack->entities[pack->num_entities++];

		s->number = i;
		s->modelindex = e->v.modelindex;
		s->frame = e->v.sequence1;
		s->effects = e->v.effects;
		s->skinnum = e->v.skin;
		VectorCopy(e->v.angles, s->angles);
		VectorCopy(e->v.origin, s->origin);
	}
}

void SVHL_Snapshot_SetupPVS(client_t *client, qbyte *pvs, unsigned int pvsbufsize)
{
}

#endif
