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
#include "pr_common.h"
#ifndef CLIENTONLY

extern cvar_t sv_nailhack;
extern cvar_t sv_cullentities_trace;
extern cvar_t sv_cullplayers_trace;
extern cvar_t sv_nopvs;

#define SV_PVS_CAMERAS 16
typedef struct
{
	int numents;
	edict_t *ent[SV_PVS_CAMERAS];	//ents in this list are always sent, even if the server thinks that they are invisible.
	vec3_t org[SV_PVS_CAMERAS];

	qbyte pvs[(MAX_MAP_LEAFS+7)>>3];
} pvscamera_t;

static void *AllocateBoneSpace(packet_entities_t *pack, unsigned char bonecount, unsigned int *allocationpos)
{
	size_t space = bonecount * sizeof(short)*7;
	void *r;
	if (pack->bonedatacur + space > pack->bonedatamax)
	{	//expand the storage as needed. messy, but whatever.
		pack->bonedatamax = pack->bonedatacur + space;
		pack->bonedata = BZ_Realloc(pack->bonedata, pack->bonedatamax);
	}
	r = pack->bonedata + pack->bonedatacur;
	*allocationpos = pack->bonedatacur;
	pack->bonedatacur += space;
	return r;
}

/*
=============================================================================

The PVS must include a small area around the client to allow head bobbing
or other small motion on the client side.  Otherwise, a bob might cause an
entity that should be visible to not show up, especially when the bob
crosses a waterline.

=============================================================================
*/

int needcleanup;

//int		fatbytes;
int glowsize, glowcolor; // made it a global variable, to suppress msvc warning.

#ifdef Q2BSPS
unsigned int  SV_Q2BSP_FatPVS (model_t *mod, vec3_t org, qbyte *resultbuf, unsigned int buffersize, qboolean add)
{
	int		leafs[64];
	int		i, j, count;
	unsigned int		longs;
	qbyte	*src;
	vec3_t	mins, maxs;

	for (i=0 ; i<3 ; i++)
	{
		mins[i] = org[i] - 8;
		maxs[i] = org[i] + 8;
	}

	count = CM_BoxLeafnums (mod, mins, maxs, leafs, 64, NULL);
	if (count < 1)
		Sys_Error ("SV_Q2FatPVS: count < 1");

	if (mod->fromgame == fg_quake3)
		longs = CM_ClusterSize(mod);
	else
		longs = (CM_NumClusters(mod)+7)/8;
	longs = (longs+(sizeof(long)-1))/sizeof(long);

	// convert leafs to clusters
	for (i=0 ; i<count ; i++)
		leafs[i] = CM_LeafCluster(mod, leafs[i]);

//	CM_ClusterPVS(mod, leafs[0], resultbuf, buffersize);


	if (count == 1 && leafs[0] == -1)
	{
		memset(resultbuf, 0xff, longs<<2);
		i = count;
	}
	else if (!add)
	{
		memcpy (resultbuf, CM_ClusterPVS(mod, leafs[0], NULL, 0), longs<<2);
		i = 1;
	}
	else
		i = 0;
	// or in all the other leaf bits
	for ( ; i<count ; i++)
	{
		for (j=0 ; j<i ; j++)
			if (leafs[i] == leafs[j])
				break;
		if (j != i)
			continue;		// already have the cluster we want
		src = CM_ClusterPVS(mod, leafs[i], NULL, 0);
		for (j=0 ; j<longs ; j++)
			((long *)resultbuf)[j] |= ((long *)src)[j];
	}
	return longs*sizeof(long);
}
#endif


void SVFTE_ExpandFrames(client_t *client, int require)
{
	client_frame_t *newframes;
	char *ptr;
	int i;
	int maxents = require * 2;	/*this is the max number of ents updated per frame. we can't track more, so...*/
	if (maxents > client->max_net_ents)
		maxents = client->max_net_ents;
	ptr = Z_Malloc(	sizeof(client_frame_t)*UPDATE_BACKUP+
					sizeof(*client->pendingdeltabits)*client->max_net_ents+
					sizeof(*client->pendingcsqcbits)*client->max_net_ents+
					sizeof(newframes[i].resend)*maxents*UPDATE_BACKUP);
	newframes = (void*)ptr;
	memcpy(newframes, client->frameunion.frames, sizeof(client_frame_t)*UPDATE_BACKUP);
	ptr += sizeof(client_frame_t)*UPDATE_BACKUP;
	memcpy(ptr, client->pendingdeltabits, sizeof(*client->pendingdeltabits)*client->max_net_ents);
	client->pendingdeltabits = (void*)ptr;
	ptr += sizeof(*client->pendingdeltabits)*client->max_net_ents;
	memcpy(ptr, client->pendingcsqcbits, sizeof(*client->pendingcsqcbits)*client->max_net_ents);
	client->pendingcsqcbits = (void*)ptr;
	ptr += sizeof(*client->pendingcsqcbits)*client->max_net_ents;
	for (i = 0; i < UPDATE_BACKUP; i++)
	{
		newframes[i].entities.max_entities = maxents;
		newframes[i].resend = (void*)ptr;
		memcpy(newframes[i].resend, client->frameunion.frames[i].resend, sizeof(newframes[i].resend)*client->frameunion.frames[i].entities.num_entities);
		newframes[i].senttime = realtime;
	}
	Z_Free(client->frameunion.frames);
	client->frameunion.frames = newframes;
}

//=============================================================================

// because there can be a lot of nails, there is a special
// network protocol for them
#define	MAX_NAILS	32
edict_t	*nails[MAX_NAILS];
int		numnails;
int		nailcount = 0;
extern	int	sv_nailmodel, sv_supernailmodel, sv_playermodel;

#ifdef SERVER_DEMO_PLAYBACK
qboolean demonails;
#endif

static edict_t *csqcent[MAX_EDICTS];
static int csqcnuments;

qboolean SV_AddNailUpdate (edict_t *ent)
{
	if (ent->v->modelindex != sv_nailmodel
		&& ent->v->modelindex != sv_supernailmodel)
		return false;
	if (sv_nailhack.value || (host_client->fteprotocolextensions2 & PEXT2_REPLACEMENTDELTAS))
		return false;

#ifdef SERVER_DEMO_PLAYBACK
	demonails = false;
#endif

	if (numnails == MAX_NAILS)
		return true;

	nails[numnails] = ent;
	numnails++;
	return true;
}
#ifdef SERVER_DEMO_PLAYBACK
qboolean SV_DemoNailUpdate (int i)
{
	demonails = true;

	if (numnails == MAX_NAILS)
		return true;
	nails[numnails] = (edict_t *)i;
	numnails++;
	return true;
}
#endif

void SV_EmitNailUpdate (sizebuf_t *msg, qboolean recorder)
{
	qbyte	bits[6];	// [48 bits] xyzpy 12 12 12 4 8
	int		n, i;
	edict_t	*ent;
	int		x, y, z, p, yaw;

	if (!numnails)
		return;

	if (recorder)
		MSG_WriteByte (msg, svc_nails2);
	else
		MSG_WriteByte (msg, svc_nails);

	MSG_WriteByte (msg, numnails);

#ifdef SERVER_DEMO_PLAYBACK
	if (demonails)
	{
		for (n=0 ; n<numnails ; n++)
		{
			i = (int)(nails[n]);
			if (recorder) {
				if (!sv.demospikes[i].id) {
					if (!((++nailcount)&255)) nailcount++;
					sv.demospikes[i].id = nailcount&255;
				}

				MSG_WriteByte (msg, (qbyte)sv.demospikes[i].id);
			}
			x = (int)(sv.demospikes[i].org[0]+4096)>>1;
			y = (int)(sv.demospikes[i].org[1]+4096)>>1;
			z = (int)(sv.demospikes[i].org[2]+4096)>>1;
			p = (int)(sv.demospikes[i].pitch)&15;
			yaw = (int)(sv.demospikes[i].yaw)&255;

			bits[0] = x;
			bits[1] = (x>>8) | (y<<4);
			bits[2] = (y>>4);
			bits[3] = z;
			bits[4] = (z>>8) | (p<<4);
			bits[5] = yaw;

			for (i=0 ; i<6 ; i++)
				MSG_WriteByte (msg, bits[i]);
		}

		return;
	}
#endif
	for (n=0 ; n<numnails ; n++)
	{
		ent = nails[n];
		if (recorder) {
			if (!ent->v->colormap) {
				if (!((++nailcount)&255)) nailcount++;
				ent->v->colormap = nailcount&255;
			}

			MSG_WriteByte (msg, (qbyte)ent->v->colormap);
		}
		x = (int)(ent->v->origin[0]+4096)>>1;
		y = (int)(ent->v->origin[1]+4096)>>1;
		z = (int)(ent->v->origin[2]+4096)>>1;
		p = (int)(16*ent->v->angles[0]/360)&15;
		yaw = (int)(256*ent->v->angles[1]/360)&255;

		bits[0] = x					& 0xff;
		bits[1] = ((x>>8) | (y<<4))	& 0xff;
		bits[2] = (y>>4)			& 0xff;
		bits[3] = z					& 0xff;
		bits[4] = ((z>>8) | (p<<4))	& 0xff;
		bits[5] = yaw				& 0xff;

		for (i=0 ; i<6 ; i++)
			MSG_WriteByte (msg, bits[i]);
	}
}





//=============================================================================

//this is the bit of the code that sends the csqc entity deltas out.
//whenever the entity in question has a newer version than we sent to the client, we need to resend.

//So, we track the outgoing sequence that an entity was sent in, and the version.
//Upon detection of a dropped packet, we resend all entities who were last sent in that packet.
//When an entities' last sent version doesn't match the current version, we send.
static qboolean SV_AddCSQCUpdate (client_t *client, edict_t *ent)
{
#ifndef PEXT_CSQC
	return false;
#else
	if (!(client->csqcactive))
		return false;

	if (!ent->xv->SendEntity)
		return false;

	csqcent[csqcnuments++] = ent;

	return true;
#endif
}
sizebuf_t csqcmsgbuffer;

static void SV_EmitDeltaEntIndex(sizebuf_t *msg, unsigned int entnum, qboolean remove, qboolean big)
{
	unsigned int rflag = remove?0x8000:0;
	if (big)
	{
		if (entnum >= 0x4000)
		{
			MSG_WriteShort(msg, (entnum&0x3fff) | 0x4000 | rflag);
			MSG_WriteByte(msg, entnum >> 14);
		}
		else
			MSG_WriteShort(msg, entnum | rflag);
	}
	else
		MSG_WriteShort(msg, entnum | rflag);
}
void SV_EmitCSQCUpdate(client_t *client, sizebuf_t *msg, qbyte svcnumber)
{
#ifdef PEXT_CSQC
	qbyte messagebuffer[1024];
	int en;
	int currentsequence = client->netchan.outgoing_sequence;
	globalvars_t *pr_globals;
	edict_t *ent;
	qboolean writtenheader = false;
	int viewerent;
	int entnum;
	int lognum = client->frameunion.frames[currentsequence & UPDATE_MASK].entities.num_entities;

	struct resendinfo_s *resend = client->frameunion.frames[currentsequence & UPDATE_MASK].resend;
	int maxlog = client->frameunion.frames[currentsequence & UPDATE_MASK].entities.max_entities;

	//we don't check that we got some already - because this is delta compressed!

	if (!client->csqcactive || !svprogfuncs || !client->pendingcsqcbits)
		return;

	pr_globals = PR_globals(svprogfuncs, PR_CURRENT);

	if (client->edict)
		viewerent = EDICT_TO_PROG(svprogfuncs, client->edict);
	else
		viewerent = 0; /*for mvds, its as if world is looking*/

	//FIXME: prioritise the list of csqc ents somehow

	csqcmsgbuffer.data = messagebuffer;
	csqcmsgbuffer.maxsize = sizeof(messagebuffer);
	csqcmsgbuffer.packing = msg->packing;
	csqcmsgbuffer.prim = msg->prim;

	for (en = 0, entnum = 0; en < csqcnuments; en++, entnum++)
	{
		ent = csqcent[en];

		//add any entity removes on ents leading up to this entity
		for (; entnum < ent->entnum; entnum++)
		{
			if (client->pendingcsqcbits[entnum] & (SENDFLAGS_PRESENT|SENDFLAGS_REMOVED))
			{
				if (!(client->pendingcsqcbits[entnum] & SENDFLAGS_REMOVED))
				{	//while the entity has NOREMOVE, only remove it if the remove is a resend
					if ((int)EDICT_NUM(svprogfuncs, en)->xv->pvsflags & PVSF_NOREMOVE)
						continue;
				}
				if (msg->cursize + 5 >= msg->maxsize)
					break;	//we're overflowing, try removing next frame instead.

				if (lognum > maxlog)
				{
					SVFTE_ExpandFrames(client, lognum+1);
					break;
				}
				resend[lognum].entnum = entnum;
				resend[lognum].bits = 0;
				resend[lognum].flags = SENDFLAGS_REMOVED;
				lognum++;
			
				if (!writtenheader)
				{
					writtenheader=true;
					MSG_WriteByte(msg, svcnumber);
				}

				SV_EmitDeltaEntIndex(msg, entnum, true, client->fteprotocolextensions2 & PEXT2_REPLACEMENTDELTAS);
//				Con_Printf("Sending remove 2 packet\n");

				client->pendingcsqcbits[entnum] = 0;
			}
		}

		if (client->pendingcsqcbits[entnum] == SENDFLAGS_PRESENT)
			continue;	//nothing changed

		if (client->pendingcsqcbits[entnum] & SENDFLAGS_REMOVED)
		{
			//we lost a remove, but it got readded since.
			//make sure all is resent
			client->pendingcsqcbits[entnum] = SENDFLAGS_USABLE;
		}

		if (!(client->pendingcsqcbits[entnum] & SENDFLAGS_PRESENT))
			client->pendingcsqcbits[entnum] = SENDFLAGS_USABLE;	//this entity appears new. make sure its fully transmitted.

		csqcmsgbuffer.cursize = 0;
		csqcmsgbuffer.currentbit = 0;
		//Ask CSQC to write a buffer for it.
		G_INT(OFS_PARM0) = viewerent;
		G_FLOAT(OFS_PARM1) = (int)(client->pendingcsqcbits[entnum] & 0xffffff);

		pr_global_struct->self = EDICT_TO_PROG(svprogfuncs, ent);
		PR_ExecuteProgram(svprogfuncs, ent->xv->SendEntity);
		if (G_INT(OFS_RETURN))	//0 means not to tell the client about it.
		{
			if (msg->cursize + csqcmsgbuffer.cursize+5 >= msg->maxsize)
			{
				if (csqcmsgbuffer.cursize < 32)
					break;
				continue;
			}

			if (lognum > maxlog)
			{
				SVFTE_ExpandFrames(client, lognum+1);
				break;
			}
			resend[lognum].entnum = entnum;
			resend[lognum].bits = 0;
			resend[lognum].flags = SENDFLAGS_PRESENT|client->pendingcsqcbits[entnum];
			lognum++;

			if (!writtenheader)
			{
				writtenheader=true;
				MSG_WriteByte(msg, svcnumber);
			}
			SV_EmitDeltaEntIndex(msg, entnum, false, client->fteprotocolextensions2 & PEXT2_REPLACEMENTDELTAS);
			if (sv.csqcdebug)	//optional extra length prefix.
			{
				if (!csqcmsgbuffer.cursize)
					Con_Printf("Warning: empty csqc packet on %s\n", PR_GetString(svprogfuncs, ent->v->classname));
				MSG_WriteShort(msg, csqcmsgbuffer.cursize);
			}
			SZ_Write(msg, csqcmsgbuffer.data, csqcmsgbuffer.cursize);

			client->pendingcsqcbits[entnum] = SENDFLAGS_PRESENT;
//			Con_Printf("Sending update packet %i\n", ent->entnum);
		}
		else if ((client->pendingcsqcbits[entnum] & SENDFLAGS_PRESENT) && !((int)ent->xv->pvsflags & PVSF_NOREMOVE))
		{	//Don't want to send, but they have it already


			if (msg->cursize + 5 >= msg->maxsize)
				break;	//we're overflowing, try removing next frame instead.

			if (lognum > maxlog)
			{
				SVFTE_ExpandFrames(client, lognum+1);
				break;
			}
			resend[lognum].entnum = entnum;
			resend[lognum].bits = 0;
			resend[lognum].flags = SENDFLAGS_REMOVED;
			lognum++;
		
			if (!writtenheader)
			{
				writtenheader=true;
				MSG_WriteByte(msg, svcnumber);
			}

			SV_EmitDeltaEntIndex(msg, entnum, true, client->fteprotocolextensions2 & PEXT2_REPLACEMENTDELTAS);
//				Con_Printf("Sending remove 2 packet\n");

			client->pendingcsqcbits[entnum] = 0;
		}
	}

	//and now tail entities
	for(; entnum < client->max_net_ents && entnum < sv.world.num_edicts; entnum++)
	{
		if (client->pendingcsqcbits[entnum] & (SENDFLAGS_PRESENT|SENDFLAGS_REMOVED))
		{
			if (!(client->pendingcsqcbits[entnum] & SENDFLAGS_REMOVED))
			{	//while the entity has NOREMOVE, only remove it if the remove is a resend
				if ((int)EDICT_NUM(svprogfuncs, en)->xv->pvsflags & PVSF_NOREMOVE)
					continue;
			}
			if (msg->cursize + 5 >= msg->maxsize)
				break;	//we're overflowing, try removing next frame instead.

			if (lognum > maxlog)
			{
				SVFTE_ExpandFrames(client, lognum+1);
				break;
			}
			resend[lognum].entnum = entnum;
			resend[lognum].bits = 0;
			resend[lognum].flags = SENDFLAGS_REMOVED;
			lognum++;

			if (!writtenheader)
			{
				writtenheader=true;
				MSG_WriteByte(msg, svcnumber);
			}

			SV_EmitDeltaEntIndex(msg, entnum, true, client->fteprotocolextensions2 & PEXT2_REPLACEMENTDELTAS);
//				Con_Printf("Sending remove 2 packet\n");

			client->pendingcsqcbits[entnum] = 0;
		}
	}

	if (writtenheader)
		MSG_WriteShort(msg, 0);	//a 0 means no more.

	csqcnuments = 0;

	client->frameunion.frames[currentsequence & UPDATE_MASK].entities.num_entities = lognum;

	//prevent the qc from trying to use it at inopertune times.
	csqcmsgbuffer.maxsize = 0;
	csqcmsgbuffer.data = NULL;
#endif
}

void SV_CSQC_DroppedPacket(client_t *client, int sequence)
{
	int i;
	if (!client->frameunion.frames)
	{
		Con_Printf("Server bug: No frames!\n");
		return;
	}

	//skip it if we never generated that frame, to avoid pulling in stale data
	if (client->frameunion.frames[sequence & UPDATE_MASK].sequence != sequence)
	{
//		Con_Printf("SV: Stale %i\n", sequence);
		return;
	}

	//lost entities need flagging for a resend
	if (client->fteprotocolextensions2 & PEXT2_REPLACEMENTDELTAS)
	{
		struct resendinfo_s *resend = client->frameunion.frames[sequence & UPDATE_MASK].resend;
//		Con_Printf("SV: Resend %i\n", sequence);
		i = client->frameunion.frames[sequence & UPDATE_MASK].entities.num_entities;
		while (i > 0)
		{
			i--;

//			if (f[i] & UF_RESET)
//				Con_Printf("Resend %i @ %i\n", i, sequence);
//			if (f[i] & UF_REMOVE)
//				Con_Printf("Remove %i @ %i\n", i, sequence);
			client->pendingdeltabits[resend[i].entnum] |= resend[i].bits;
			client->pendingcsqcbits[resend[i].entnum] |= resend[i].flags;
		}
		client->frameunion.frames[sequence & UPDATE_MASK].entities.num_entities = 0;
	}
	//lost stats do too
	if (client->fteprotocolextensions2 & PEXT2_REPLACEMENTDELTAS)
	{
		client_t *sp;
		unsigned short *n = client->frameunion.frames[sequence & UPDATE_MASK].resendstats;
		i = client->frameunion.frames[sequence & UPDATE_MASK].numresendstats;
		while(i-->0)
		{
			unsigned short s = n[i];
			if (s & 0xf000)
			{
				sp = client;
				while (s & 0xf000)
				{
					s -= 0x1000;
					sp = sp->controlled;
				}
				sp->pendingstats[s>>5u] |= 1u << (s & 0x1fu);
			}
			else
				client->pendingstats[s>>5u] |= 1u << (s & 0x1fu);
		}
		client->frameunion.frames[sequence & UPDATE_MASK].numresendstats = 0;
	}
}

void SV_AckEntityFrame(client_t *cl, int framenum)
{
	//any not acked yet are assumed to be lost.
	//this may result in packetloss if the client received multiple packets between sending two outgoing packets.
	int frame = cl->lastsequence_acknowledged+1;
	if (framenum > cl->lastsequence_acknowledged)
		cl->lastsequence_acknowledged = framenum;
	if (framenum > frame + UPDATE_BACKUP)
		framenum = frame + UPDATE_BACKUP;

#ifdef PEXT_CSQC
	for (; frame < framenum; frame++)
		SV_CSQC_DroppedPacket(cl, frame);
#endif
}

void SV_ReplaceEntityFrame(client_t *cl, int framenum)
{
	//this packet is about to be overwritten, we can't track more.
	//we might as well pretend that it got acked, we can't track pings that far back anyway, just make sure it gets flagged as dropped.
	//due to how qw sequences are controlled by the client, the server may have skipped some frames. try to handle those too.
	int frame = cl->lastsequence_acknowledged+1;
	framenum -= UPDATE_BACKUP;
	if (framenum > cl->lastsequence_acknowledged)
		cl->lastsequence_acknowledged = framenum;
	if (framenum > frame + UPDATE_BACKUP)
		framenum = frame + UPDATE_BACKUP;

#ifdef PEXT_CSQC
	for (; frame <= framenum; frame++)
		SV_CSQC_DroppedPacket(cl, frame);
#endif
}

/*
void SV_CSQC_DropAll(client_t *client)
{
	int i;

	if (client->fteprotocolextensions2 & PEXT2_REPLACEMENTDELTAS)
	{
//		Con_Printf("Reset all\n");
		client->pendingdeltabits[0] = UF_REMOVE;
	}

	if (client->csqcactive)	//we don't need this, but it might be a little faster.
	{
		//FIXME: handle any needed removes
		for (i = 0; i < sv.world.num_edicts; i++)
			client->pendingcsqcbits[i] |= SENDFLAGS_USABLE;	//resend all
	}

	//we don't know which stats were on the wire, resend all. :(
	memset(client->pendingstats, 0xff, sizeof(client->pendingstats));
}
*/
//=============================================================================


/*
==================
SV_WriteDelta

Writes part of a packetentities message.
Can delta from either a baseline or a previous packet_entity
==================
*/
void SVQW_WriteDelta (entity_state_t *from, entity_state_t *to, sizebuf_t *msg, qboolean force, unsigned int protext)
{
#ifdef PROTOCOLEXTENSIONS
	int evenmorebits=0;
#endif
	int		bits;
	int		i;
	int fromeffects;
	coorddata coordd[3];
	coorddata angled[3];

	if (from == &((edict_t*)NULL)->baseline)
		from = &nullentitystate;

// send an update
	bits = 0;

	if (msg->prim.coordsize == 2)
	{
		for (i=0 ; i<3 ; i++)
		{
			coordd[i] = MSG_ToCoord(to->origin[i], msg->prim.coordsize);
			if (MSG_ToCoord(from->origin[i], msg->prim.coordsize).b4 != coordd[i].b4)
				bits |= U_ORIGIN1<<i;
			else
				to->origin[i] = from->origin[i];
		}
	}
	else
	{
		for (i=0 ; i<3 ; i++)
		{
			coordd[i] = MSG_ToCoord(to->origin[i], msg->prim.coordsize);
			if (to->origin[i] != from->origin[i])
				bits |= U_ORIGIN1<<i;
		}
	}

	angled[0] = MSG_ToAngle(to->angles[0], msg->prim.anglesize);
	if (MSG_ToAngle(from->angles[0], msg->prim.anglesize).b4 != angled[0].b4)
		bits |= U_ANGLE1;
	else
		to->angles[0] = from->angles[0];

	angled[1] = MSG_ToAngle(to->angles[1], msg->prim.anglesize);
	if (MSG_ToAngle(from->angles[1], msg->prim.anglesize).b4 != angled[1].b4)
		bits |= U_ANGLE2;
	else
		to->angles[1] = from->angles[1];

	angled[2] = MSG_ToAngle(to->angles[2], msg->prim.anglesize);
	if (MSG_ToAngle(from->angles[2], msg->prim.anglesize).b4 != angled[2].b4)
		bits |= U_ANGLE3;
	else
		to->angles[2] = from->angles[2];

	if ( to->colormap != from->colormap )
		bits |= U_COLORMAP;

	if ( to->skinnum != from->skinnum )
		bits |= U_SKIN;

	if ( to->frame != from->frame )
		bits |= U_FRAME;


	if (force && !(protext & PEXT_SPAWNSTATIC2))
		fromeffects = 0;	//force is true if we're going from baseline
	else					//old quakeworld protocols do not include effects in the baseline
		fromeffects = from->effects;	//so old clients will see the effects baseline as 0
	if ((to->effects&0x00ff) != (fromeffects&0x00ff))
		bits |= U_EFFECTS;
	if ((to->effects&0xff00) != (fromeffects&0xff00) && (protext & PEXT_DPFLAGS))
		evenmorebits |= U_EFFECTS16;

	if (to->modelindex != from->modelindex)
	{
		bits |= U_MODEL;
		if (to->modelindex > 255)
		{
			if (protext & PEXT_MODELDBL)
			{
				if (to->modelindex > 512)
					bits &= ~U_MODEL;
				evenmorebits |= U_MODELDBL;
			}
			else
				return;
		}
	}

#ifdef PROTOCOLEXTENSIONS
#ifdef U_SCALE
	if ( to->scale != from->scale && (protext & PEXT_SCALE))
		evenmorebits |= U_SCALE;
#endif
#ifdef U_TRANS
	if ( to->trans != from->trans && (protext & PEXT_TRANS))
		evenmorebits |= U_TRANS;
#endif
#ifdef U_FATNESS
	if ( to->fatness != from->fatness && (protext & PEXT_FATNESS))
		evenmorebits |= U_FATNESS;
#endif

	if ( to->hexen2flags != from->hexen2flags && (protext & PEXT_HEXEN2))
		evenmorebits |= U_DRAWFLAGS;
	if ( to->abslight != from->abslight && (protext & PEXT_HEXEN2))
		evenmorebits |= U_ABSLIGHT;

	if ((to->colormod[0]!=from->colormod[0]||to->colormod[1]!=from->colormod[1]||to->colormod[2]!=from->colormod[2]) && (protext & PEXT_COLOURMOD))
		evenmorebits |= U_COLOURMOD;

	if (to->glowsize != from->glowsize)
		to->dpflags |= RENDER_GLOWTRAIL;

	if (to->dpflags != from->dpflags && (protext & PEXT_DPFLAGS))
		evenmorebits |= U_DPFLAGS;

	if ((to->tagentity != from->tagentity || to->tagindex != from->tagindex) && (protext & PEXT_DPFLAGS))
		evenmorebits |= U_TAGINFO;

	if ((to->light[0] != from->light[0] || to->light[1] != from->light[1] || to->light[2] != from->light[2] || to->light[3] != from->light[3] || to->lightstyle != from->lightstyle || to->lightpflags != from->lightstyle) && (protext & PEXT_DPFLAGS))
		evenmorebits |= U_LIGHT;
#endif

//	if (to->solid)
//		bits |= U_SOLID;

	if (msg->cursize + 40 > msg->maxsize)
	{	//not enough space in the buffer, don't send the entity this frame. (not sending means nothing changes, and it takes no bytes!!)
		*to = *from;
		return;
	}

	//
	// write the message
	//
	if (!to->number)
		SV_Error ("Unset entity number");

	if (!bits && !evenmorebits && !force)
		return;		// nothing to send!

#ifdef PROTOCOLEXTENSIONS
	if (to->number >= 512)
	{
		if (to->number >= 1024)
		{
			if (to->number >= 1024+512)
				evenmorebits |= U_ENTITYDBL;

			evenmorebits |= U_ENTITYDBL2;
			if (to->number >= 2048)
				return;
		}
		else
			evenmorebits |= U_ENTITYDBL;
	}

	if (evenmorebits&0xff00)
		evenmorebits |= U_YETMORE;
	if (evenmorebits&0x00ff)
		bits |= U_EVENMORE;
	if (bits & 511)
		bits |= U_MOREBITS;
#endif

	i = (to->number&511) | (bits&~511);
	if (i & U_REMOVE)
		Sys_Error ("U_REMOVE");
	MSG_WriteShort (msg, i);

	if (bits & U_MOREBITS)
		MSG_WriteByte (msg, bits&255);
#ifdef PROTOCOLEXTENSIONS
	if (bits & U_EVENMORE)
		MSG_WriteByte (msg, evenmorebits&255);
	if (evenmorebits & U_YETMORE)
		MSG_WriteByte (msg, (evenmorebits>>8)&255);
#endif

	if (bits & U_MODEL)
		MSG_WriteByte (msg,	to->modelindex&255);
	else if (evenmorebits & U_MODELDBL)
		MSG_WriteShort(msg, to->modelindex);

	if (bits & U_FRAME)
		MSG_WriteByte (msg, to->frame);
	if (bits & U_COLORMAP)
		MSG_WriteByte (msg, to->colormap);
	if (bits & U_SKIN)
		MSG_WriteByte (msg, to->skinnum&0xff);
	if (bits & U_EFFECTS)
		MSG_WriteByte (msg, to->effects&0x00ff);
	if (bits & U_ORIGIN1)
		SZ_Write(msg, &coordd[0], msg->prim.coordsize);
	if (bits & U_ANGLE1)
		SZ_Write(msg, &angled[0], msg->prim.anglesize);
	if (bits & U_ORIGIN2)
		SZ_Write(msg, &coordd[1], msg->prim.coordsize);
	if (bits & U_ANGLE2)
		SZ_Write(msg, &angled[1], msg->prim.anglesize);
	if (bits & U_ORIGIN3)
		SZ_Write(msg, &coordd[2], msg->prim.coordsize);
	if (bits & U_ANGLE3)
		SZ_Write(msg, &angled[2], msg->prim.anglesize);

#ifdef U_SCALE
	if (evenmorebits & U_SCALE)
		MSG_WriteByte (msg, (qbyte)(to->scale));
#endif
#ifdef U_TRANS
	if (evenmorebits & U_TRANS)
		MSG_WriteByte (msg, (qbyte)(to->trans));
#endif
#ifdef U_FATNESS
	if (evenmorebits & U_FATNESS)
		MSG_WriteChar (msg, to->fatness);
#endif

	if (evenmorebits & U_DRAWFLAGS)
		MSG_WriteByte (msg, to->hexen2flags);
	if (evenmorebits & U_ABSLIGHT)
		MSG_WriteByte (msg, to->abslight);

	if (evenmorebits & U_COLOURMOD)
	{
		MSG_WriteByte (msg, to->colormod[0]);
		MSG_WriteByte (msg, to->colormod[1]);
		MSG_WriteByte (msg, to->colormod[2]);
	}

	if (evenmorebits & U_DPFLAGS)
		MSG_WriteByte (msg, to->dpflags);

	if (evenmorebits & U_TAGINFO)
	{
		MSG_WriteEntity (msg, to->tagentity);
		MSG_WriteShort (msg, to->tagindex);
	}

	if (evenmorebits & U_LIGHT)
	{
		MSG_WriteShort (msg, to->light[0]);
		MSG_WriteShort (msg, to->light[1]);
		MSG_WriteShort (msg, to->light[2]);
		MSG_WriteShort (msg, to->light[3]);
		MSG_WriteByte (msg, to->lightstyle);
		MSG_WriteByte (msg, to->lightpflags);
	}

	if (evenmorebits & U_EFFECTS16)
		MSG_WriteByte (msg, (to->effects&0xff00)>>8);
}

/*special flags which are slightly more compact. these are 'wasted' as part of the delta itself*/
#define UF_REMOVE		UF_16BIT	/*says we removed the entity in this frame*/
#define UF_MOVETYPE		UF_EFFECTS2	/*this flag isn't present in the header itself*/
#define UF_RESET2		UF_EXTEND1	/*so new ents are reset 3 times to avoid weird baselines*/
//#define UF_UNUSED		UF_EXTEND2	/**/
#define UF_WEAPONFRAME_OLD	UF_EXTEND2
#define UF_VIEWANGLES	UF_EXTEND3	/**/

static unsigned int SVFTE_DeltaPredCalcBits(entity_state_t *from, entity_state_t *to)
{
	unsigned int bits = 0;
	if (from && from->u.q1.pmovetype != to->u.q1.pmovetype)
		bits |= UFP_MOVETYPE;

	if (to->u.q1.movement[0])
		bits |= UFP_FORWARD;
	if (to->u.q1.movement[1])
		bits |= UFP_SIDE;
	if (to->u.q1.movement[2])
		bits |= UFP_UP;
	if (to->u.q1.velocity[0])
		bits |= UFP_VELOCITYXY;
	if (to->u.q1.velocity[1])
		bits |= UFP_VELOCITYXY;
	if (to->u.q1.velocity[2])
		bits |= UFP_VELOCITYZ;
	if (to->u.q1.msec)
		bits |= UFP_MSEC;

	return bits;
}

static unsigned int SVFTE_DeltaCalcBits(entity_state_t *from, qbyte *frombonedata, entity_state_t *to, qbyte *tobonedata)
{
	unsigned int bits = 0;

	if (from->u.q1.pmovetype != to->u.q1.pmovetype)
		bits |= UF_PREDINFO|UF_MOVETYPE;
	if (from->u.q1.weaponframe != to->u.q1.weaponframe)
		bits |= UF_PREDINFO|UF_WEAPONFRAME_OLD;
	if (to->u.q1.pmovetype)
	{
		if (SVFTE_DeltaPredCalcBits(from, to))
			bits |= UF_PREDINFO;

		/*moving players get extra data forced upon them which is not deltatracked*/
		if ((bits & UF_PREDINFO) && (from->u.q1.velocity[0] || from->u.q1.velocity[1] || from->u.q1.velocity[2]))
		{
			/*if we've got player movement then write the origin anyway*/
			bits |= UF_ORIGINXY | UF_ORIGINZ;
			/*and force angles too, if its not us*/
			if (host_client != svs.clients + to->number-1)
				bits |= UF_ANGLESXZ | UF_ANGLESY;
		}
	}

	if (to->origin[0] != from->origin[0])
		bits |= UF_ORIGINXY;
	if (to->origin[1] != from->origin[1])
		bits |= UF_ORIGINXY;
	if (to->origin[2] != from->origin[2])
		bits |= UF_ORIGINZ;

	if (to->angles[0] != from->angles[0])
		bits |= UF_ANGLESXZ;
	if (to->angles[1] != from->angles[1])
		bits |= UF_ANGLESY;
	if (to->angles[2] != from->angles[2])
		bits |= UF_ANGLESXZ;


	if (to->modelindex != from->modelindex)
		bits |= UF_MODEL;
	if (to->frame != from->frame)
		bits |= UF_FRAME;
	if (to->skinnum != from->skinnum)
		bits |= UF_SKIN;
	if (to->colormap != from->colormap)
		bits |= UF_COLORMAP;
	if (to->effects != from->effects)
		bits |= UF_EFFECTS;
	if (to->dpflags != from->dpflags)
		bits |= UF_FLAGS;
	if (to->solidsize != from->solidsize)
		bits |= UF_SOLID;

	if (to->scale != from->scale)
		bits |= UF_SCALE;
	if (to->trans != from->trans)
		bits |= UF_ALPHA;
	if (to->fatness != from->fatness)
		bits |= UF_FATNESS;

	if (to->hexen2flags != from->hexen2flags || to->abslight != from->abslight)
		bits |= UF_DRAWFLAGS;

	if (to->bonecount != from->bonecount || (to->bonecount && memcmp(frombonedata+from->boneoffset, tobonedata+to->boneoffset, to->bonecount*sizeof(short)*7)))
		bits |= UF_BONEDATA;
	if (!to->bonecount && (to->basebone != from->basebone || to->baseframe != from->baseframe))
		bits |= UF_BONEDATA;

	if (to->colormod[0]!=from->colormod[0]||to->colormod[1]!=from->colormod[1]||to->colormod[2]!=from->colormod[2])
		bits |= UF_COLORMOD;

	if (to->glowsize!=from->glowsize||to->glowcolour!=from->glowcolour||to->glowmod[0]!=from->glowmod[0]||to->glowmod[1]!=from->glowmod[1]||to->glowmod[2]!=from->glowmod[2])
		bits |= UF_GLOW;

	if (to->tagentity != from->tagentity || to->tagindex != from->tagindex)
		bits |= UF_TAGINFO;

	if (to->light[0] != from->light[0] || to->light[1] != from->light[1] || to->light[2] != from->light[2] || to->light[3] != from->light[3] || to->lightstyle != from->lightstyle || to->lightpflags != from->lightpflags)
		bits |= UF_LIGHT;

	if (to->u.q1.traileffectnum != from->u.q1.traileffectnum)
		bits |= UF_TRAILEFFECT;

	if (to->modelindex2 != from->modelindex2)
		bits |= UF_MODELINDEX2;

	if (to->u.q1.gravitydir[0] != from->u.q1.gravitydir[0] || to->u.q1.gravitydir[1] != from->u.q1.gravitydir[1])
		bits |= UF_GRAVITYDIR;

	return bits;
}

static void SVFTE_WriteUpdate(unsigned int bits, entity_state_t *state, sizebuf_t *msg, unsigned int pext2, qbyte *boneptr)
{
	unsigned int predbits = 0;
	if (bits & UF_MOVETYPE)
	{
		bits &= ~UF_MOVETYPE;
		predbits |= UFP_MOVETYPE;
	}
	if (pext2 & PEXT2_PREDINFO)
	{
		if (bits & UF_VIEWANGLES)
		{
			bits &= ~UF_VIEWANGLES;
			bits |= UF_PREDINFO;
			predbits |= UFP_VIEWANGLE;
		}
	}
	else
	{
		if (bits & UF_VIEWANGLES)
		{
			bits &= ~UF_VIEWANGLES;
			bits |= UF_PREDINFO;
		}
		if (bits & UF_WEAPONFRAME_OLD)
		{
			bits &= ~UF_WEAPONFRAME_OLD;
			predbits |= UFP_WEAPONFRAME_OLD;
		}
	}

	if (!(pext2 & PEXT2_NEWSIZEENCODING))	//was added at the same time
		bits &= ~UF_BONEDATA;

	/*check if we need more precision*/
	if ((bits & UF_MODEL) && state->modelindex > 255)
		bits |= UF_16BIT;
	if ((bits & UF_SKIN) && state->skinnum > 255)
		bits |= UF_16BIT;
	if ((bits & UF_FRAME) && state->frame > 255)
		bits |= UF_16BIT;

	/*convert effects bits to higher lengths if needed*/
	if (bits & UF_EFFECTS)
	{
		if (state->effects & 0xffff0000) /*both*/
			bits |= UF_EFFECTS | UF_EFFECTS2;
		else if (state->effects & 0x0000ff00) /*2 only*/
			bits = (bits & ~UF_EFFECTS) | UF_EFFECTS2;
	}
	if (bits & 0xff000000)
		bits |= UF_EXTEND3;
	if (bits & 0x00ff0000)
		bits |= UF_EXTEND2;
	if (bits & 0x0000ff00)
		bits |= UF_EXTEND1;

	MSG_WriteByte(msg, (bits>>0) & 0xff);
	if (bits & UF_EXTEND1)
		MSG_WriteByte(msg, (bits>>8) & 0xff);
	if (bits & UF_EXTEND2)
		MSG_WriteByte(msg, (bits>>16) & 0xff);
	if (bits & UF_EXTEND3)
		MSG_WriteByte(msg, (bits>>24) & 0xff);

	if (bits & UF_FRAME)
	{
		if (bits & UF_16BIT)
			MSG_WriteShort(msg, state->frame);
		else
			MSG_WriteByte(msg, state->frame);
	}
	if (bits & UF_ORIGINXY)
	{
		MSG_WriteCoord(msg, state->origin[0]);
		MSG_WriteCoord(msg, state->origin[1]);
	}
	if (bits & UF_ORIGINZ)
		MSG_WriteCoord(msg, state->origin[2]);

	if ((bits & UF_PREDINFO) && !(pext2 & PEXT2_PREDINFO))
	{	/*if we have pred info, use more precise angles*/
		if (bits & UF_ANGLESXZ)
		{
			MSG_WriteAngle16(msg, state->angles[0]);
			MSG_WriteAngle16(msg, state->angles[2]);
		}
		if (bits & UF_ANGLESY)
			MSG_WriteAngle16(msg, state->angles[1]);
	}
	else
	{
		if (bits & UF_ANGLESXZ)
		{
			MSG_WriteAngle(msg, state->angles[0]);
			MSG_WriteAngle(msg, state->angles[2]);
		}
		if (bits & UF_ANGLESY)
			MSG_WriteAngle(msg, state->angles[1]);
	}

	if ((bits & (UF_EFFECTS|UF_EFFECTS2)) == (UF_EFFECTS|UF_EFFECTS2))
		MSG_WriteLong(msg, state->effects);
	else if (bits & UF_EFFECTS2)
		MSG_WriteShort(msg, state->effects);
	else if (bits & UF_EFFECTS)
		MSG_WriteByte(msg, state->effects);

	if (bits & UF_PREDINFO)
	{
		/*movetype is set above somewhere*/
		predbits |= SVFTE_DeltaPredCalcBits(NULL, state);

		MSG_WriteByte(msg, predbits);
		if (predbits & UFP_FORWARD)
			MSG_WriteShort(msg, state->u.q1.movement[0]);
		if (predbits & UFP_SIDE)
			MSG_WriteShort(msg, state->u.q1.movement[1]);
		if (predbits & UFP_UP)
			MSG_WriteShort(msg, state->u.q1.movement[2]);
		if (predbits & UFP_MOVETYPE)
			MSG_WriteByte(msg, state->u.q1.pmovetype);
		if (predbits & UFP_VELOCITYXY)
		{
			MSG_WriteShort(msg, state->u.q1.velocity[0]);
			MSG_WriteShort(msg, state->u.q1.velocity[1]);
		}
		if (predbits & UFP_VELOCITYZ)
			MSG_WriteShort(msg, state->u.q1.velocity[2]);
		if (predbits & UFP_MSEC)
			MSG_WriteByte(msg, state->u.q1.msec);
		if (pext2 & PEXT2_PREDINFO)
		{
			if (predbits & UFP_VIEWANGLE)
			{	/*if we have pred info, use more precise angles*/
				if (bits & UF_ANGLESXZ)
				{
					MSG_WriteShort(msg, state->u.q1.vangle[0]);
					MSG_WriteShort(msg, state->u.q1.vangle[2]);
				}
				if (bits & UF_ANGLESY)
					MSG_WriteShort(msg, state->u.q1.vangle[1]);
			}
		}
		else
		{
			if (predbits & UFP_WEAPONFRAME_OLD)
			{
				if (state->u.q1.weaponframe > 127)
				{
					MSG_WriteByte(msg, 128 | (state->u.q1.weaponframe & 127));
					MSG_WriteByte(msg, state->u.q1.weaponframe>>7);
				}
				else
					MSG_WriteByte(msg, state->u.q1.weaponframe);
			}
		}
	}

	if (bits & UF_MODEL)
	{
		if (bits & UF_16BIT)
			MSG_WriteShort(msg, state->modelindex);
		else
			MSG_WriteByte(msg, state->modelindex);
	}
	if (bits & UF_SKIN)
	{
		if (bits & UF_16BIT)
			MSG_WriteShort(msg, state->skinnum);
		else
			MSG_WriteByte(msg, state->skinnum);
	}
	if (bits & UF_COLORMAP)
		MSG_WriteByte(msg, state->colormap & 0xff);

	if (bits & UF_SOLID)
	{
		if (pext2 & PEXT2_NEWSIZEENCODING)
		{
			if (!state->solidsize)
				MSG_WriteByte(msg, 0);
			else if (state->solidsize == ES_SOLID_BSP)
				MSG_WriteByte(msg, 1);
			else if (state->solidsize == ES_SOLID_HULL1)
				MSG_WriteByte(msg, 2);
			else if (state->solidsize == ES_SOLID_HULL2)
				MSG_WriteByte(msg, 3);
			else if (!ES_SOLID_HAS_EXTRA_BITS(state->solidsize))
			{
				MSG_WriteByte(msg, 16);
				MSG_WriteSize16(msg, state->solidsize);
			}
			else
			{
				MSG_WriteByte(msg, 32);
				MSG_WriteLong(msg, state->solidsize);
			}
		}
		else
			MSG_WriteSize16(msg, state->solidsize);
	}

	if (bits & UF_FLAGS)
		MSG_WriteByte(msg, state->dpflags);

	if (bits & UF_ALPHA)
		MSG_WriteByte(msg, state->trans);
	if (bits & UF_SCALE)
		MSG_WriteByte(msg, state->scale);
	if (bits & UF_BONEDATA)
	{
		short *bonedata;
		int i;
		qbyte bfl = 0;
		if (state->bonecount && boneptr)
			bfl |= 0x80;
		if (state->basebone || state->baseframe)
			bfl |= 0x40;
		MSG_WriteByte(msg, bfl);
		if (bfl & 0x80)
		{
			//this is NOT finalized
			MSG_WriteByte(msg, state->bonecount);
			bonedata = (short*)(boneptr + state->boneoffset);
			for (i = 0; i < state->bonecount*7; i++)
				MSG_WriteShort(msg, bonedata[i]);
		}
		if (bfl & 0x40)
		{
			MSG_WriteByte(msg, state->basebone);
			MSG_WriteShort(msg, state->baseframe);
		}
	}
	if (bits & UF_DRAWFLAGS)
	{
		MSG_WriteByte(msg, state->hexen2flags);
		if ((state->hexen2flags & MLS_MASK) == MLS_ABSLIGHT)
			MSG_WriteByte(msg, state->abslight);
	}
	if (bits & UF_TAGINFO)
	{
		MSG_WriteEntity(msg, state->tagentity);
		MSG_WriteByte(msg, state->tagindex);
	}
	if (bits & UF_LIGHT)
	{
		MSG_WriteShort (msg, state->light[0]);
		MSG_WriteShort (msg, state->light[1]);
		MSG_WriteShort (msg, state->light[2]);
		MSG_WriteShort (msg, state->light[3]);
		MSG_WriteByte (msg, state->lightstyle);
		MSG_WriteByte (msg, state->lightpflags);
	}
	if (bits & UF_TRAILEFFECT)
		MSG_WriteShort(msg, state->u.q1.traileffectnum);

	if (bits & UF_COLORMOD)
	{
		MSG_WriteByte(msg, state->colormod[0]);
		MSG_WriteByte(msg, state->colormod[1]);
		MSG_WriteByte(msg, state->colormod[2]);
	}
	if (bits & UF_GLOW)
	{
		MSG_WriteByte(msg, state->glowsize);
		MSG_WriteByte(msg, state->glowcolour);
		MSG_WriteByte(msg, state->glowmod[0]);
		MSG_WriteByte(msg, state->glowmod[1]);
		MSG_WriteByte(msg, state->glowmod[2]);
	}
	if (bits & UF_FATNESS)
		MSG_WriteByte(msg, state->fatness);
	if (bits & UF_MODELINDEX2)
	{
		if (bits & UF_16BIT)
			MSG_WriteShort(msg, state->modelindex2);
		else
			MSG_WriteByte(msg, state->modelindex2);
	}

	if (bits & UF_GRAVITYDIR)
	{
		MSG_WriteByte(msg, state->u.q1.gravitydir[0]);
		MSG_WriteByte(msg, state->u.q1.gravitydir[1]);
	}
}

/*dump out the delta from baseline (used for baselines and statics, so has no svc)*/
void SVFTE_EmitBaseline(entity_state_t *to, qboolean numberisimportant, sizebuf_t *msg, unsigned int pext2)
{
	unsigned int bits;
	if (numberisimportant)
		MSG_WriteEntity(msg, to->number);
	bits = UF_RESET | SVFTE_DeltaCalcBits(&nullentitystate, NULL, to, NULL);
	SVFTE_WriteUpdate(bits, to, msg, pext2, NULL);
}

/*SVFTE_EmitPacketEntities
Writes changed entities to the client.
Changed ent states will be tracked, even if they're not sent just yet, dropped packets will also re-flag dropped delta bits
Only what changed is tracked, via bitmask, its previous value is never tracked.
*/
void SVFTE_EmitPacketEntities(client_t *client, packet_entities_t *to, sizebuf_t *msg)
{
	edict_t *e;
	entity_state_t *o, *n;
	unsigned int i;
	unsigned int j;
	unsigned int bits;
	struct resendinfo_s *resend;
	unsigned int outno, outmax;
	int sequence;
	qbyte *oldbonedata;
	unsigned int maxbonedatasize;

	if (ISNQCLIENT(client))
		sequence = client->netchan.outgoing_unreliable;
	else
		sequence = client->netchan.incoming_sequence;

	if (!client->pendingdeltabits)
		return;
	
	if (client->delta_sequence < 0)
		client->pendingdeltabits[0] = UF_REMOVE;

	//if we're clearing the list and starting from scratch, just wipe all lingering state
	if (client->pendingdeltabits[0] & UF_REMOVE)
	{
		for (j = 0; j < client->sentents.num_entities; j++)
		{
			client->sentents.entities[j].number = 0;
			client->pendingdeltabits[j] = 0;
		}
		client->pendingdeltabits[0] = UF_REMOVE;
	}

	//expand client's entstate list
	if (to->num_entities)
	{
		j = to->entities[to->num_entities-1].number+1;
		if (j > client->sentents.max_entities)
		{
			client->sentents.entities = BZ_Realloc(client->sentents.entities, sizeof(*client->sentents.entities) * j);
			memset(&client->sentents.entities[client->sentents.max_entities], 0, sizeof(client->sentents.entities[0]) * (j - client->sentents.max_entities));
			client->sentents.max_entities = j;
		}
		while(j > client->sentents.num_entities)
		{
			client->sentents.entities[client->sentents.num_entities].number = 0;
			client->sentents.num_entities++;
		}
	}

	//orphan and regenerate
	oldbonedata = client->sentents.bonedata;
	maxbonedatasize = client->sentents.bonedatamax;
	if (client->sentents.bonedatacur)
	{
		client->sentents.bonedata = BZ_Malloc(maxbonedatasize);
		client->sentents.bonedatacur = 0;
		client->sentents.bonedatamax = maxbonedatasize;
	}
	else
	{
		client->sentents.bonedata = NULL;
		client->sentents.bonedatacur = 0;
		client->sentents.bonedatamax = 0;
	}

	/*figure out the entitys+bits that changed (removed and active)*/
	for (i = 0, j = 0; i < to->num_entities; i++)
	{
		n = &to->entities[i];
		/*gaps are dead entities*/
		for (; j < n->number; j++)
		{
			o = &client->sentents.entities[j];
			if (o->number)
			{
				e = EDICT_NUM(svprogfuncs, o->number);
				if (!((int)e->xv->pvsflags & PVSF_NOREMOVE))
				{
					client->pendingdeltabits[j] = UF_REMOVE;
					o->number = 0; /*dead*/
					o->bonecount = 0; /*don't waste cycles*/
				}
				else if (o->bonecount)
				{
					short *srcbdata = (short*)(oldbonedata + o->boneoffset);
					short *bonedata = AllocateBoneSpace(&client->sentents, o->bonecount, &o->boneoffset);
					memcpy(bonedata, srcbdata, sizeof(short)*7*o->bonecount);
				}
			}
		}

		o = &client->sentents.entities[j];
		if (!o->number)
		{
			/*flag it for reset, we can add the extra bits later once we get around to sending it*/
			client->pendingdeltabits[j] = UF_RESET | UF_RESET2;
		}
		else
		{
			//its valid, make sure we don't have a stale/resent remove, and do a cheap reset due to uncertainty.
			if (client->pendingdeltabits[j] & UF_REMOVE)
				client->pendingdeltabits[j] = (client->pendingdeltabits[j] & ~UF_REMOVE) | UF_RESET2;
			client->pendingdeltabits[j] |= SVFTE_DeltaCalcBits(o, oldbonedata, n, to->bonedata);
			//even if prediction is disabled, we want to force velocity info to be sent for the local player. This is used by view bob and things.
			if (client->edict && j == client->edict->entnum && (n->u.q1.velocity[0] || n->u.q1.velocity[1] || n->u.q1.velocity[2]))
				client->pendingdeltabits[j] |= UF_PREDINFO;

			//spectators(and mvds) should be told the actual view angles of the person they're trying to track
			if (j <= sv.allocated_client_slots && (!client->edict || j == client->spec_track))
//				if (client->pendingentbits[j])
				{
					if (o->u.q1.vangle[0] != n->u.q1.vangle[0] || o->u.q1.vangle[2] != n->u.q1.vangle[2])
						client->pendingdeltabits[j] |= UF_ANGLESXZ;
					if (o->u.q1.vangle[1] != n->u.q1.vangle[1])
						client->pendingdeltabits[j] |= UF_ANGLESY;
					client->pendingdeltabits[j] |= UF_VIEWANGLES;
				}
		}
		*o = *n;
		if (o->bonecount)
		{
			short *bonedata = AllocateBoneSpace(&client->sentents, o->bonecount, &o->boneoffset);
			short *srcbdata = (short*)(to->bonedata + n->boneoffset);
			memcpy(bonedata, srcbdata, sizeof(short)*7*o->bonecount);
		}
		j++;
	}

	/*gaps are dead entities*/
	for (; j < client->sentents.num_entities; j++)
	{
		o = &client->sentents.entities[j];
		if (o->number)
		{
			e = EDICT_NUM(svprogfuncs, o->number);
			if (!((int)e->xv->pvsflags & PVSF_NOREMOVE))
			{
				client->pendingdeltabits[j] = UF_REMOVE;
				o->number = 0; /*dead*/
				o->bonecount = 0; /*don't waste cycles*/
			}
			else if (o->bonecount)
			{
				short *srcbdata = (short*)(oldbonedata + o->boneoffset);
				short *bonedata = AllocateBoneSpace(&client->sentents, o->bonecount, &o->boneoffset);
				memcpy(bonedata, srcbdata, sizeof(short)*7*o->bonecount);
			}
		}
	}

	Z_Free(oldbonedata);

	/*cache frame info*/
	resend = client->frameunion.frames[sequence & UPDATE_MASK].resend;
	outno = 0;
	outmax = client->frameunion.frames[sequence & UPDATE_MASK].entities.max_entities;

	/*start writing the packet*/
	MSG_WriteByte (msg, svcfte_updateentities);
	if (ISNQCLIENT(client) && (client->fteprotocolextensions2 & PEXT2_PREDINFO))
	{
		MSG_WriteLong(msg, client->last_sequence);
	}
//	Con_Printf("Gen sequence %i\n", sequence);
	MSG_WriteFloat(msg, sv.world.physicstime);

	if (client->pendingdeltabits[0] & UF_REMOVE)
	{
		SV_EmitDeltaEntIndex(msg, 0, true, true);
		resend[outno].bits = UF_REMOVE;
		resend[outno].flags = 0;
		resend[outno++].entnum = 0;

		client->pendingdeltabits[0] &= ~UF_REMOVE;
	}
	for(j = 1; j < client->sentents.num_entities; j++)
	{
		bits = client->pendingdeltabits[j];
		if (!(bits & ~UF_RESET2))	//skip while there's nothing to send (skip reset2 if there's no other changes, its only to reduce chances of the client getting 'new' entities containing just an origin)*/
			continue;
		if (msg->cursize + 50 > msg->maxsize)
			break; /*give up if it gets full. FIXME: bone data is HUGE.*/
		if (outno >= outmax)
		{	//expand the frames. may need some copying...
			SVFTE_ExpandFrames(client, outno+1);
			break;
		}
		client->pendingdeltabits[j] = 0;

		if (bits & UF_REMOVE)
		{	//if reset is set, then reset was set eroneously.
			SV_EmitDeltaEntIndex(msg, j, true, true);
			resend[outno].bits = UF_REMOVE;
//			Con_Printf("REMOVE %i @ %i\n", j, sequence);
		}
		else if (client->sentents.entities[j].number) /*only send a new copy of the ent if they actually have one already*/
		{
			if (bits & UF_RESET2)
			{
				/*if reset2, then this is the second packet sent to the client and should have a forced reset (but which isn't tracked)*/
				resend[outno].bits = bits & ~UF_RESET2;
				bits = UF_RESET | SVFTE_DeltaCalcBits(&EDICT_NUM(svprogfuncs, j)->baseline, NULL, &client->sentents.entities[j], client->sentents.bonedata);
//				Con_Printf("RESET2 %i @ %i\n", j, sequence);
			}
			else if (bits & UF_RESET)
			{
				/*flag the entity for the next packet, so we always get two resets when it appears, to reduce the effects of packetloss on seeing rockets etc*/
				client->pendingdeltabits[j] = UF_RESET2;
				bits = UF_RESET | SVFTE_DeltaCalcBits(&EDICT_NUM(svprogfuncs, j)->baseline, NULL, &client->sentents.entities[j], client->sentents.bonedata);
				resend[outno].bits = UF_RESET;
//				Con_Printf("RESET %i @ %i\n", j, sequence);
			}
			else
				resend[outno].bits = bits;

			SV_EmitDeltaEntIndex(msg, j, false, true);
			SVFTE_WriteUpdate(bits, &client->sentents.entities[j], msg, client->fteprotocolextensions2, client->sentents.bonedata);
		}
		resend[outno].flags = 0;
		resend[outno++].entnum = j;
	}
	MSG_WriteShort(msg, 0);

	client->frameunion.frames[sequence & UPDATE_MASK].entities.num_entities = outno;
	client->frameunion.frames[sequence & UPDATE_MASK].sequence = sequence;
}

/*
=============
SVQW_EmitPacketEntities

Writes a delta update of a packet_entities_t to the message.

deltaing is performed from one set of entity states directly to the next
=============
*/
void SVQW_EmitPacketEntities (client_t *client, packet_entities_t *to, sizebuf_t *msg)
{
	edict_t	*ent;
	client_frame_t	*fromframe;
	packet_entities_t *from;
	int		oldindex, newindex;
	int		oldnum, newnum;
	int		oldmax;

	// this is the frame that we are going to delta update from
	if (client->delta_sequence != -1)
	{
		fromframe = &client->frameunion.frames[client->delta_sequence & UPDATE_MASK];
		from = &fromframe->entities;
		oldmax = from->num_entities;

		MSG_WriteByte (msg, svc_deltapacketentities);
		MSG_WriteByte (msg, client->delta_sequence);
	}
	else
	{
		oldmax = 0;	// no delta update
		from = NULL;

		MSG_WriteByte (msg, svc_packetentities);
	}

	newindex = 0;
	oldindex = 0;
//Con_Printf ("---%i to %i ----\n", client->delta_sequence & UPDATE_MASK
//			, client->netchan.outgoing_sequence & UPDATE_MASK);
	while (newindex < to->num_entities || oldindex < oldmax)
	{
		newnum = newindex >= to->num_entities ? 9999 : to->entities[newindex].number;
		oldnum = oldindex >= oldmax ? 9999 : from->entities[oldindex].number;

		if (newnum == oldnum)
		{	// delta update from old position
//Con_Printf ("delta %i\n", newnum);
#ifdef PROTOCOLEXTENSIONS
			SVQW_WriteDelta (&from->entities[oldindex], &to->entities[newindex], msg, false, client->fteprotocolextensions);
#else
			SVQW_WriteDelta (&from->entities[oldindex], &to->entities[newindex], msg, false);
#endif
			oldindex++;
			newindex++;
			continue;
		}

		if (newnum < oldnum)
		{	// this is a new entity, send it from the baseline
			if (svprogfuncs)
				ent = EDICT_NUM(svprogfuncs, newnum);
			else
				ent = NULL;
//Con_Printf ("baseline %i\n", newnum);
#ifdef PROTOCOLEXTENSIONS
			SVQW_WriteDelta (&ent->baseline, &to->entities[newindex], msg, true, client->fteprotocolextensions);
#else
			SVQW_WriteDelta (&ent->baseline, &to->entities[newindex], msg, true);
#endif
			newindex++;
			continue;
		}

		if (newnum > oldnum)
		{	// the old entity isn't present in the new message
//Con_Printf ("remove %i\n", oldnum);
			if (oldnum >= 512)
			{
				//yup, this is expensive.
				MSG_WriteShort (msg, (oldnum&511) | U_REMOVE|U_MOREBITS);
				MSG_WriteByte (msg, U_EVENMORE);
				if (oldnum >= 1024)
				{
					if (oldnum >= 1024+512)
						MSG_WriteByte (msg, U_ENTITYDBL|U_ENTITYDBL2);
					else
						MSG_WriteByte (msg, U_ENTITYDBL2);
				}
				else
					MSG_WriteByte (msg, U_ENTITYDBL);
			}
			else
				MSG_WriteShort (msg, (oldnum&511) | U_REMOVE);

			oldindex++;
			continue;
		}
	}

	if (newindex > to->max_entities)
		Con_Printf("Exceeded max entities\n");

	MSG_WriteShort (msg, 0);	// end of packetentities
}
#ifdef NQPROT
unsigned int SVDP_CalcDelta(entity_state_t *from, qbyte *frombonedatabase, entity_state_t *to, qbyte *tobonedatabase)
{
	unsigned int bits = 0;
	//E5_FULLUPDATE is handled elsewhere
	//E5_EXTEND* is handled elsewhere
	if (!VectorEquals(from->origin, to->origin))
		bits |= E5_ORIGIN;
	if (!VectorEquals(from->angles, to->angles))
		bits |= E5_ANGLES;
	if (from->modelindex != to->modelindex)
		bits |= E5_MODEL;
	if (from->frame != to->frame)
		bits |= E5_FRAME;
	if (from->skinnum != to->skinnum)
		bits |= E5_SKIN;
	if (from->effects != to->effects)
		bits |= E5_EFFECTS;
	if (from->dpflags != to->dpflags)
		bits |= E5_FLAGS;
	if (from->trans != to->trans)
		bits |= E5_ALPHA;
	if (from->scale != to->scale)
		bits |= E5_SCALE;
	if (from->colormap != to->colormap)
		bits |= E5_COLORMAP;
	if (from->tagentity != to->tagentity || from->tagindex != to->tagindex)
		bits |= E5_ATTACHMENT;
	if (from->light[0] != to->light[0] || from->light[1] != to->light[1] || from->light[2] != to->light[2] || from->light[3] != to->light[3] || from->lightstyle != to->lightstyle || from->lightpflags != to->lightpflags)
		bits |= E5_LIGHT;
	if (from->glowsize != to->glowsize || from->glowcolour != to->glowcolour)
		bits |= E5_GLOW;
	if (from->colormod[0] != to->colormod[0] || from->colormod[1] != to->colormod[1] || from->colormod[2] != to->colormod[2])
		bits |= E5_COLORMOD;
	if (from->glowmod[0] != to->glowmod[0] || from->glowmod[1] != to->glowmod[1] || from->glowmod[2] != to->glowmod[2])
		bits |= E5_GLOWMOD;
	if (to->bonecount != from->bonecount || (to->bonecount && (!frombonedatabase || memcmp(frombonedatabase+from->boneoffset, tobonedatabase+to->boneoffset, to->bonecount*sizeof(short)*7))))
		if (to->bonecount)
			bits |= E5_COMPLEXANIMATION;
	if (to->u.q1.traileffectnum != from->u.q1.traileffectnum)
		bits |= E5_TRAILEFFECTNUM;

	if ((bits & E5_ORIGIN) && (!(to->dpflags & RENDER_LOWPRECISION) || to->origin[0] < -4096 || to->origin[0] >= 4096 || to->origin[1] < -4096 || to->origin[1] >= 4096 || to->origin[2] < -4096 || to->origin[2] >= 4096))
		bits |= E5_ORIGIN32;
	if ((bits & E5_ANGLES) && !(to->dpflags & RENDER_LOWPRECISION))
		bits |= E5_ANGLES16;
	if ((bits & E5_MODEL) && to->modelindex >= 256)
		bits |= E5_MODEL16;
	if ((bits & E5_FRAME) && to->frame >= 256)
		bits |= E5_FRAME16;
	if (bits & E5_EFFECTS)
	{
		if (to->effects >= 65536)
			bits |= E5_EFFECTS32;
		else if (to->effects >= 256)
			bits |= E5_EFFECTS16;
	}

	return bits;
}

void SVDP_EmitEntityDelta(entity_state_t *from, entity_state_t *to, sizebuf_t *msg, qboolean isnew, qbyte *bonedatabase)
{
	int bits;

	bits = 0;
	if (isnew)
		bits |= E5_FULLUPDATE;

	//FIXME: this stuff should be outside of this function
	//with the whole nack stuff
	bits = SVDP_CalcDelta(from, NULL, to, bonedatabase);

	if (bits >= 256)
		bits |= E5_EXTEND1;
	if (bits >= 65536)
		bits |= E5_EXTEND2;
	if (bits >= 16777216)
		bits |= E5_EXTEND3;

	if (!bits)
		return;

	MSG_WriteShort(msg, to->number);
	MSG_WriteByte(msg, bits & 0xFF);
	if (bits & E5_EXTEND1)
		MSG_WriteByte(msg, (bits >> 8) & 0xFF);
	if (bits & E5_EXTEND2)
		MSG_WriteByte(msg, (bits >> 16) & 0xFF);
	if (bits & E5_EXTEND3)
		MSG_WriteByte(msg, (bits >> 24) & 0xFF);
	if (bits & E5_FLAGS)
		MSG_WriteByte(msg, to->dpflags);
	if (bits & E5_ORIGIN)
	{
		if (bits & E5_ORIGIN32)
		{
			MSG_WriteFloat(msg, to->origin[0]);
			MSG_WriteFloat(msg, to->origin[1]);
			MSG_WriteFloat(msg, to->origin[2]);
		}
		else
		{
			MSG_WriteShort(msg, to->origin[0]*8);
			MSG_WriteShort(msg, to->origin[1]*8);
			MSG_WriteShort(msg, to->origin[2]*8);
		}
	}
	if (bits & E5_ANGLES)
	{
		if (bits & E5_ANGLES16)
		{
			MSG_WriteAngle16(msg, to->angles[0]);
			MSG_WriteAngle16(msg, to->angles[1]);
			MSG_WriteAngle16(msg, to->angles[2]);
		}
		else
		{
			MSG_WriteAngle8(msg, to->angles[0]);
			MSG_WriteAngle8(msg, to->angles[1]);
			MSG_WriteAngle8(msg, to->angles[2]);
		}
	}
	if (bits & E5_MODEL)
	{
		if (bits & E5_MODEL16)
			MSG_WriteShort(msg, to->modelindex);
		else
			MSG_WriteByte(msg, to->modelindex);
	}
	if (bits & E5_FRAME)
	{
		if (bits & E5_FRAME16)
			MSG_WriteShort(msg, to->frame);
		else
			MSG_WriteByte(msg, to->frame);
	}
	if (bits & E5_SKIN)
		MSG_WriteByte(msg, to->skinnum);
	if (bits & E5_EFFECTS)
	{
		if (bits & E5_EFFECTS32)
			MSG_WriteLong(msg, to->effects);
		else if (bits & E5_EFFECTS16)
			MSG_WriteShort(msg, to->effects);
		else
			MSG_WriteByte(msg, to->effects);
	}
	if (bits & E5_ALPHA)
		MSG_WriteByte(msg, to->trans);
	if (bits & E5_SCALE)
		MSG_WriteByte(msg, to->scale);
	if (bits & E5_COLORMAP)
		MSG_WriteByte(msg, to->colormap&0xff);
	if (bits & E5_ATTACHMENT)
	{
		MSG_WriteEntity(msg, to->tagentity);
		MSG_WriteByte(msg, to->tagindex);
	}
	if (bits & E5_LIGHT)
	{
		MSG_WriteShort(msg, to->light[0]);
		MSG_WriteShort(msg, to->light[1]);
		MSG_WriteShort(msg, to->light[2]);
		MSG_WriteShort(msg, to->light[3]);
		MSG_WriteByte(msg, to->lightstyle);
		MSG_WriteByte(msg, to->lightpflags);
	}
	if (bits & E5_GLOW)
	{
		MSG_WriteByte(msg, to->glowsize);
		MSG_WriteByte(msg, to->glowcolour);
	}
	if (bits & E5_COLORMOD)
	{
		MSG_WriteByte(msg, to->colormod[0]);
		MSG_WriteByte(msg, to->colormod[1]);
		MSG_WriteByte(msg, to->colormod[2]);
	}
	if (bits & E5_GLOWMOD)
	{
		MSG_WriteByte(msg, to->glowmod[0]);
		MSG_WriteByte(msg, to->glowmod[1]);
		MSG_WriteByte(msg, to->glowmod[2]);
	}
	if (bits & E5_COMPLEXANIMATION)
	{
		short *bonedata = (short*)(bonedatabase+to->boneoffset);
		int i;
		MSG_WriteByte(msg, 4);
		MSG_WriteShort(msg, to->modelindex);
		MSG_WriteByte(msg, to->bonecount);
		for (i = 0; i < to->bonecount*7; i++)
			MSG_WriteShort(msg, bonedata[i]);
	}
	if (bits & E5_TRAILEFFECTNUM)
		MSG_WriteShort(msg, to->u.q1.traileffectnum);
}

void SVDP_EmitEntitiesUpdate (client_t *client, packet_entities_t *to, sizebuf_t *msg)
{
	packet_entities_t *from;
	int		oldindex, newindex;
	int		oldnum, newnum;
	int		oldmax;

	// this is the frame that we are going to delta update from
	if (!client->netchan.incoming_sequence)
	{
		oldmax = 0;
		from = NULL;
	}
	else
	{
		client_frame_t	*fromframe = &client->frameunion.frames[(client->netchan.incoming_sequence-1) & UPDATE_MASK];
		from = &fromframe->entities;
		oldmax = from->num_entities;
	}

//	Con_Printf ("frame %i\n", client->netchan.incoming_sequence);

	MSG_WriteByte(msg, svcdp_entities);
	MSG_WriteLong(msg, client->netchan.incoming_sequence);	//sequence for the client to ack (any bits sent in unacked frames will be re-queued)
	if (client->protocol == SCP_DARKPLACES7)
		MSG_WriteLong(msg, client->last_sequence);			//movement sequence that we are acking.

	client->netchan.incoming_sequence++;

	//add in the bitmasks of dropped packets.

	newindex = 0;
	oldindex = 0;
//Con_Printf ("---%i to %i ----\n", client->delta_sequence & UPDATE_MASK
//			, client->netchan.outgoing_sequence & UPDATE_MASK);
	while (newindex < to->num_entities || oldindex < oldmax)
	{
		newnum = newindex >= to->num_entities ? 0x7fff : to->entities[newindex].number;
		oldnum = oldindex >= oldmax ? 0x7fff : from->entities[oldindex].number;

		if (newnum == oldnum)
		{	// delta update from old position
//Con_Printf ("delta %i\n", newnum);
			SVDP_EmitEntityDelta (&from->entities[oldindex], &to->entities[newindex], msg, false, to->bonedata);
			oldindex++;
			newindex++;
			continue;
		}

		if (newnum < oldnum)
		{	// this is a new entity, send it from the baseline... as far as dp understands it...
//Con_Printf ("baseline %i\n", newnum);
			SVDP_EmitEntityDelta (&nullentitystate, &to->entities[newindex], msg, true, to->bonedata);
			newindex++;
			continue;
		}

		if (newnum > oldnum)
		{	// the old entity isn't present in the new message
//	Con_Printf("sRemove %i\n", oldnum);
			MSG_WriteShort(msg, oldnum | 0x8000);
			oldindex++;
			continue;
		}
	}

	MSG_WriteShort(msg, 0x8000);
}
#endif


int SV_HullNumForPlayer(int h2hull, float *mins, float *maxs)
{
	int diff;
	int best;
	int hullnum, i;

	if (sv.world.worldmodel->fromgame != fg_quake)
	{
		return -mins[2] + 32;	//clients are expected to decide themselves.
	}

	if (h2hull)
		return (h2hull-1) | (mins[2]?0:128);


	hullnum = 0;
	best = 8192;
	//x/y pos/neg are assumed to be the same magnitute.
	//y pos/height are assumed to be different from all the others.
	for (i = 0; i < MAX_MAP_HULLSM; i++)
	{
#define sq(x) ((x)*(x))
		diff = sq(sv.world.worldmodel->hulls[i].clip_maxs[2] - maxs[2]) +
			sq(sv.world.worldmodel->hulls[i].clip_mins[2] - mins[2]) +
			sq(sv.world.worldmodel->hulls[i].clip_maxs[0] - maxs[0]) +
			sq(sv.world.worldmodel->hulls[i].clip_mins[0] - mins[0]);
		if (diff < best)
		{
			best = diff;
			hullnum=i;
		}
	}
	return hullnum;
}

#if 1
typedef struct {
	int playernum;
	qboolean onladder;
	usercmd_t	*lastcmd;
	int modelindex;
	int frame;
	int weaponframe;
	int vw_index;
	float *angles;
	float *origin;
	float *velocity;
	int effects;
	int skin;
	float *mins;
	float *maxs;
	float scale;
	float transparency;
	float fatness;
	float localtime;
	int health;
	int spectator;	//0=send to a player. 1=non-tracked player, to a spec. 2=tracked player, to a spec(or self)
	qboolean isself;
	int fteext;
	int zext;
	int hull;
	client_t *cl;
} clstate_t;
void SV_WritePlayerToClient(sizebuf_t *msg, clstate_t *ent)
{
	usercmd_t	cmd;
	int msec;
	int hullnumber;
	int i;
	int pflags;
	int pm_type, pm_code;
	int zext = ent->zext;

		pflags = PF_MSEC | PF_COMMAND;

		if (ent->modelindex != sv_playermodel)
			pflags |= PF_MODEL;

		if (ent->velocity)
			for (i=0 ; i<3 ; i++)
				if (ent->velocity[i])
					pflags |= PF_VELOCITY1<<i;
		if (ent->effects)
			pflags |= PF_EFFECTS;
		if (ent->skin || ent->modelindex>=256)
			pflags |= PF_SKINNUM;
		if (ent->health <= 0)
			pflags |= PF_DEAD;
		if (progstype == PROG_QW)
		{
			if (ent->mins[2] != -24)
				pflags |= PF_GIB;
		}
		else if (progstype == PROG_H2)
		{
//			if (ent->maxs[2] != 56)
//				pflags |= PF_GIB;
		}
		else
		{
			if (ent->mins[2] != -24)
				pflags |= PF_GIB;
		}

		if (ent->isself)
		{
			if (ent->spectator)
				pflags &= PF_VELOCITY1 | PF_VELOCITY2 | PF_VELOCITY3 | PF_DEAD | PF_GIB;
			else
			{	// don't send a lot of data on personal entity
				pflags &= ~(PF_MSEC|PF_COMMAND);
				if (ent->weaponframe)
					pflags |= PF_WEAPONFRAME;
			}
		}

		if (ent->spectator == 2 && ent->weaponframe)	//it's not us, but we are spectating, so we need the correct weaponframe
			pflags |= PF_WEAPONFRAME;

		if (!ent->isself || (ent->fteext & PEXT_SPLITSCREEN))
		{
#ifdef PEXT_SCALE	//this is graphics, not physics
			if (ent->fteext & PEXT_SCALE)
			{
				if (ent->scale && ent->scale != 1) pflags |= PF_SCALE;
			}
#endif
#ifdef PEXT_TRANS
			if (ent->fteext & PEXT_TRANS)
			{
				if (ent->transparency) pflags |= PF_TRANS;
			}
#endif
#ifdef PEXT_FATNESS
			if (ent->fteext & PEXT_FATNESS)
			{
				if (ent->fatness) pflags |= PF_FATNESS;
			}
#endif
		}
#ifdef PEXT_HULLSIZE
		if (ent->fteext & PEXT_HULLSIZE)
		{
			hullnumber = SV_HullNumForPlayer(ent->hull, ent->mins, ent->maxs);
			if (hullnumber != 1)
				pflags |= PF_HULLSIZE_Z;
		}
		else
			hullnumber=1;
#endif

		if (zext&Z_EXT_PM_TYPE)
		{
			if (ent->cl)
			{
				if (ent->cl->viewent)
					pm_type = PMC_NONE;
				else
					pm_type = SV_PMTypeForClient (ent->cl, ent->cl->edict);
				switch (pm_type)
				{
				case PM_NORMAL:		// Z_EXT_PM_TYPE protocol extension
					if (ent->cl->jump_held)
						pm_code = PMC_NORMAL_JUMP_HELD;	// encode pm_type and jump_held into pm_code
					else
						pm_code = PMC_NORMAL;
					break;
				case PM_OLD_SPECTATOR:
					pm_code = PMC_OLD_SPECTATOR;
					break;
				case PM_SPECTATOR:	// Z_EXT_PM_TYPE_NEW protocol extension
					pm_code = PMC_SPECTATOR;
					break;
				case PM_FLY:
					pm_code = PMC_FLY;
					break;
				case PM_DEAD:
					pm_code = PMC_NORMAL;
					break;
				case PM_NONE:
					pm_code = PMC_NONE;
					break;
				case PM_WALLWALK:
					pm_code = PMC_WALLWALK;
					break;
				default:
//					Sys_Error("SV_WritePlayersToClient: unexpected pm_type");
					pm_code=PMC_NORMAL;
					break;
				}
			}
			else
				pm_code = (ent->zext & Z_EXT_PM_TYPE_NEW)?PMC_SPECTATOR:PMC_OLD_SPECTATOR;//(ent->spectator && ent->isself) ? PMC_OLD_SPECTATOR : PMC_NORMAL;
			pflags |= pm_code << PF_PMC_SHIFT;
		}

		if (pflags & 0xff0000)
			pflags |= PF_EXTRA_PFS;

		MSG_WriteByte (msg, svc_playerinfo);
		MSG_WriteByte (msg, ent->playernum);
		MSG_WriteShort (msg, pflags&0xffff);

		if (pflags & PF_EXTRA_PFS)
		{
			MSG_WriteByte(msg, (pflags&0xff0000)>>16);
		}
		//we need to tell the client that it's moved, as it's own origin might not be natural

		for (i=0 ; i<3 ; i++)
			MSG_WriteCoord (msg, ent->origin[i]);

		MSG_WriteByte (msg, ent->frame);

		if (pflags & PF_MSEC)
		{
			msec = 1000*(sv.time - ent->localtime);
			if (msec < 0)
				msec = 0;
			if (msec > 255)
				msec = 255;
			MSG_WriteByte (msg, msec);
		}

		if (pflags & PF_COMMAND)
		{
			if (ent->lastcmd)
				cmd = *ent->lastcmd;
			else
			{
				memset(&cmd, 0, sizeof(cmd));
				cmd.angles[0] = (short)(ent->angles[0] * 65535/360.0f);
				cmd.angles[1] = (short)(ent->angles[1] * 65535/360.0f);
				cmd.angles[2] = (short)(ent->angles[2] * 65535/360.0f);
			}

			if (ent->health <= 0)
			{	// don't show the corpse looking around...
				cmd.angles[0] = 0;
				cmd.angles[1] = (int)(ent->angles[1]*65535/360);
				cmd.angles[2] = 0;
			}

			cmd.buttons = 0;	// never send buttons
			if (ent->zext & Z_EXT_VWEP)
				cmd.impulse = ent->vw_index;	// never send impulses
			else
				cmd.impulse = 0;

			MSG_WriteDeltaUsercmd (msg, &nullcmd, &cmd);
		}

		if (ent->velocity)
		{
			for (i=0 ; i<3 ; i++)
				if (pflags & (PF_VELOCITY1<<i) )
					MSG_WriteShort (msg, (short)(ent->velocity[i]));
		}
		else
		{
			for (i=0 ; i<3 ; i++)
				if (pflags & (PF_VELOCITY1<<i) )
					MSG_WriteShort (msg, 0);
		}

		if (pflags & PF_MODEL)
		{
			MSG_WriteByte (msg, ent->modelindex);
		}

		if (pflags & PF_SKINNUM)
			MSG_WriteByte (msg, ent->skin | (((pflags & PF_MODEL)&&(ent->modelindex>=256))<<7));

		if (pflags & PF_EFFECTS)
			MSG_WriteByte (msg, ent->effects);

		if (pflags & PF_WEAPONFRAME)
			MSG_WriteByte (msg, ent->weaponframe);

#ifdef PEXT_SCALE
		if (pflags & PF_SCALE)
			MSG_WriteByte (msg, ent->scale*50);
#endif
#ifdef PEXT_TRANS
		if (pflags & PF_TRANS)
			MSG_WriteByte (msg, (qbyte)(ent->transparency*255));
#endif
#ifdef PEXT_FATNESS
		if (pflags & PF_FATNESS)
			MSG_WriteChar (msg, ent->fatness);
#endif
#ifdef PEXT_HULLSIZE	//shrunken or crouching in halflife levels. (possibly enlarged)
		if (pflags & PF_HULLSIZE_Z)
			MSG_WriteChar (msg, hullnumber + (ent->onladder?128:0));	//physics.
#endif
}
#endif


qboolean Cull_Traceline(pvscamera_t *cameras, edict_t *seen)
{
	int i;
	trace_t tr;
	vec3_t end;
	int c;

	if (seen->v->solid == SOLID_BSP)
		return false;	//bsp ents are never culled this way

	//stage 1: check against their origin
	for (c = 0; c < cameras->numents; c++)
	{
		tr.fraction = 1;
		if (!sv.world.worldmodel->funcs.NativeTrace (sv.world.worldmodel, 1, 0, NULL, cameras->org[c], seen->v->origin, vec3_origin, vec3_origin, false, FTECONTENTS_SOLID, &tr))
			return false;	//wasn't blocked
	}

	//stage 2: check against their bbox
	for (c = 0; c < cameras->numents; c++)
	{
		for (i = 0; i < 8; i++)
		{
			end[0] = seen->v->origin[0] + ((i&1)?seen->v->mins[0]:seen->v->maxs[0]);
			end[1] = seen->v->origin[1] + ((i&2)?seen->v->mins[1]:seen->v->maxs[1]);
			end[2] = seen->v->origin[2] + ((i&4)?seen->v->mins[2]+0.1:seen->v->maxs[2]);

			tr.fraction = 1;
			if (!sv.world.worldmodel->funcs.NativeTrace (sv.world.worldmodel, 1, 0, NULL, cameras->org[c], end, vec3_origin, vec3_origin, false, FTECONTENTS_SOLID, &tr))
				return false;	//this trace went through, so don't cull
		}
	}

	return true;
}

void SV_WritePlayersToMVD (client_t *client, client_frame_t *frame, sizebuf_t *msg)
{
	int			j;
	client_t	*cl;
	edict_t		*ent, *vent;
//	int			pflags;

	demo_frame_t *demo_frame;
	demo_client_t *dcl;

	demo_frame = &demo.frames[demo.parsecount&DEMO_FRAMES_MASK];
	for (j=0,cl=svs.clients, dcl = demo_frame->clients; j < svs.allocated_client_slots ; j++,cl++, dcl++)
	{
		if (cl->state != cs_spawned)
			continue;

#ifdef SERVER_DEMO_PLAYBACK
		if (sv.demostatevalid)
		{
			if (client != cl)
				continue;
		}
#endif

		ent = cl->edict;
		vent = ent;

		if (progstype != PROG_QW)
		{
			if ((int)ent->v->effects & EF_MUZZLEFLASH)
			{
				if (needcleanup < (j+1))
				{
					needcleanup = (j+1);
				}
			}
		}

		if (SV_AddCSQCUpdate(client, ent))
			continue;

		if (cl->spectator)
			continue;

		dcl->parsecount = demo.parsecount;

		VectorCopy(vent->v->origin, dcl->info.origin);
		VectorCopy(vent->v->angles, dcl->info.angles);
		dcl->info.angles[0] *= -3;
		dcl->info.angles[2] = 0; // no roll angle

		if (ent->v->health <= 0)
		{	// don't show the corpse looking around...
			dcl->info.angles[0] = 0;
			dcl->info.angles[1] = vent->v->angles[1];
			dcl->info.angles[2] = 0;
		}

		if (ent != vent)
		{
			dcl->info.model = 0;	//invisible.
			dcl->info.effects = 0;
		}
		else
		{
			dcl->info.skinnum = ent->v->skin;
			dcl->info.effects = ent->v->effects;
			dcl->info.weaponframe = ent->v->weaponframe;
			dcl->info.model = ent->v->modelindex;
		}
		dcl->sec = sv.time - cl->localtime;
		dcl->frame = ent->v->frame;
		dcl->flags = 0;
		dcl->cmdtime = cl->localtime;
		dcl->fixangle = demo.fixangle[j];
		demo.fixangle[j] = 0;

		if (ent->v->health <= 0)
			dcl->flags |= DF_DEAD;
		if (ent->v->mins[2] != -24)
			dcl->flags |= DF_GIB;
	}
}

/*
=============
SV_WritePlayersToClient

=============
*/
void SV_WritePlayersToClient (client_t *client, client_frame_t *frame, edict_t *clent, pvscamera_t *cameras, sizebuf_t *msg)
{
	qboolean isbot;
	int			j;
	client_t	*cl;
	edict_t		*ent, *vent;
//	int			pflags;

	if (client->state < cs_spawned)
	{
		Con_Printf("SV_WritePlayersToClient: not spawned yet\n");
		return;
	}

#ifdef NQPROT
	if (!ISQWCLIENT(client))
		return;
#endif

#ifdef SERVER_DEMO_PLAYBACK
	if (sv.demostatevalid)	//this is a demo
	{
		usercmd_t cmd;
		vec3_t ang;
		vec3_t org;
		vec3_t vel;
		float lerp;
		float a1, a2;
		int i;
		extern vec3_t player_mins, player_maxs;
		clstate_t clst;
		extern float olddemotime, nextdemotime;

		for (i=0 ; i<svs.allocated_client_slots ; i++)
		{
			//FIXME: Add PVS stuff.

			if (*sv.recordedplayer[i].userinfo)	//if the client was active
			{
				clst.playernum = i;
				clst.onladder = 0;
				clst.lastcmd = &cmd;
				clst.modelindex = sv.demostate[i+1].modelindex;
				if (!clst.modelindex)
					continue;
				clst.frame = sv.demostate[i+1].frame;
				clst.weaponframe = sv.recordedplayer[i].weaponframe;
				clst.angles = ang;
				clst.origin = org;
				clst.hull = 1;
				clst.velocity = vel;
				clst.effects = sv.demostate[i+1].effects;
				clst.skin = sv.demostate[i+1].skinnum;
				clst.mins = player_mins;
				clst.maxs = player_maxs;
				clst.scale = sv.demostate[i+1].scale;
				clst.transparency = sv.demostate[i+1].trans;
				clst.fatness = sv.demostate[i+1].fatness;
				clst.localtime = sv.time;//sv.recordedplayer[j].updatetime;
				clst.health = sv.recordedplayer[i].stats[STAT_HEALTH];
				clst.spectator = 2;	//so that weaponframes work properly.
				clst.isself = false;
				clst.fteext = 0;//client->fteprotocolextensions;
				clst.zext = 0;//client->zquake_extensions;
				clst.cl = NULL;
				clst.vw_index = 0;

				lerp = (realtime - olddemotime) / (nextdemotime - olddemotime);
				if (lerp < 0)
					lerp = 0;
				if (lerp > 1)
					lerp = 1;
				for (j = 0; j < 3; j++)
				{
					a1 = (360.0f/256)*sv.recordedplayer[i].oldang[j];
					a2 = (360.0f/256)*sv.demostate[i+1].angles[j];
					a2 = a2 - a1;
					if (a2 > 180)
						a2-=360;
					if (a2 < -180)
						a2+=360;
					ang[j] = (a1 + (a2)*lerp);

					org[j] = sv.recordedplayer[i].oldorg[j] + (sv.demostate[i+1].origin[j] - sv.recordedplayer[i].oldorg[j])*lerp;

					vel[j] = (-sv.recordedplayer[i].oldorg[j] + sv.demostate[i+1].origin[j])*(nextdemotime - olddemotime);
				}

				ang[0] *= -3;

//				ang[0] = ang[1] = ang[2] = 0;

				memset(&cmd, 0, sizeof(cmd));
				cmd.angles[0] = ang[0]*65535/360.0f;
				cmd.angles[1] = ang[1]*65535/360.0f;
				cmd.angles[2] = ang[2]*65535/360.0f;
				cmd.msec = 50;
					{vec3_t f, r, u, v;
				AngleVectors(ang, f, r, u);
				VectorCopy(vel, v);
				cmd.forwardmove = DotProduct(f, v);
				cmd.sidemove = DotProduct(r, v);
				cmd.upmove = DotProduct(u, v);
					}
				clst.lastcmd=NULL;

				SV_WritePlayerToClient(msg, &clst);
			}
		}

		//now build the spectator's thingie

		memset(&clst, 0, sizeof(clst));

		clst.fteext = 0;//client->fteprotocolextensions;
		clst.zext = 0;//client->zquake_extensions;
		clst.vw_index = 0;
		clst.playernum = svs.allocated_client_slots-1;
		clst.isself = true;
		clst.modelindex = 0;
		clst.hull = 1;
		clst.frame = 0;
		clst.localtime = sv.time;
		clst.mins = player_mins;
		clst.maxs = player_maxs;

		clst.angles = vec3_origin;	//not needed, as the client knows better than us anyway.
		clst.origin = client->specorigin;
		clst.velocity = client->specvelocity;

		for (client = client; client; client = client->controller)
		{
			clst.health = 100;

			if (client->spec_track)
			{
				clst.weaponframe = sv.recordedplayer[client->spec_track-1].weaponframe;
				clst.spectator = 2;
			}
			else
			{
				clst.weaponframe = 0;
				clst.spectator = 1;
			}

			SV_WritePlayerToClient(msg, &clst);

			clst.playernum--;
		}
		return;
	}
#endif
	for (j=0,cl=svs.clients ; j<sv.allocated_client_slots && j < client->max_net_clients; j++,cl++)
	{
		if (cl->state != cs_spawned && !(cl->state == cs_free && cl->name[0]))	//this includes bots, and nq bots
			continue;

		if ((client->penalties & BAN_BLIND) && client != cl)
			continue;


		isbot = (!cl->name[0] || cl->protocol == SCP_BAD);
		ent = cl->edict;
		if (cl->viewent && ent == clent)
		{
			vent = EDICT_NUM(svprogfuncs, cl->viewent);
			if (!vent)
				vent = ent;
		}
		else
			vent = ent;






		if (progstype != PROG_QW)
		{
			if (progstype == PROG_H2 && (int)ent->v->effects & H2EF_NODRAW && ent != clent)
				continue;

			if ((int)ent->v->effects & EF_MUZZLEFLASH)
			{
				if (needcleanup < (j+1))
				{
					needcleanup = (j+1);
				}
			}
		}

		// ZOID visibility tracking
		if (ent != clent &&
			!(client->spec_track && client->spec_track - 1 == j))
		{
			if (cl->spectator)
				continue;

			// ignore if not touching a PV leaf
			if (cameras && !sv.world.worldmodel->funcs.EdictInFatPVS(sv.world.worldmodel, &ent->pvsinfo, cameras->pvs))
				continue;

			if (!((int)clent->xv->dimension_see & ((int)ent->xv->dimension_seen | (int)ent->xv->dimension_ghost)))
				continue;	//not in this dimension - sorry...
			if (cameras && (sv_cullplayers_trace.value || sv_cullentities_trace.value))
				if (Cull_Traceline(cameras, ent))
					continue;
		}

		if (SV_AddCSQCUpdate(client, ent))
			continue;

		{
			clstate_t clst;
			clst.playernum = j;
			clst.onladder = (int)ent->xv->pmove_flags&PMF_LADDER;
			clst.lastcmd = &cl->lastcmd;
			clst.modelindex = vent->v->modelindex;
			clst.frame = vent->v->frame;
			clst.weaponframe = ent->v->weaponframe;
			clst.angles = ent->v->angles;
			clst.origin = vent->v->origin;
			clst.velocity = vent->v->velocity;
			clst.effects = ent->v->effects;
			clst.vw_index = ent->xv->vw_index;

			if (progstype == PROG_H2 && ((int)vent->v->effects & H2EF_NODRAW))
			{
				clst.effects = 0;
				clst.modelindex = 0;
			}

			clst.skin = vent->v->skin;
			clst.mins = vent->v->mins;
			clst.hull = vent->xv->hull;
			clst.maxs = vent->v->maxs;
			clst.scale = vent->xv->scale;
			clst.transparency = vent->xv->alpha;

			//QSG_DIMENSION_PLANES - if the only shared dimensions are ghost dimensions, Set half alpha.
			if (((int)clent->xv->dimension_see & (int)ent->xv->dimension_ghost))
				if (!((int)clent->xv->dimension_see & ((int)ent->xv->dimension_seen & ~(int)ent->xv->dimension_ghost)) )
				{
					if (ent->xv->dimension_ghost_alpha)
						clst.transparency *= ent->xv->dimension_ghost_alpha;
					else
						clst.transparency *= 0.5;
				}

			clst.fatness = vent->xv->fatness;
			clst.localtime = cl->localtime;
			clst.health = ent->v->health;
			clst.spectator = 0;
			clst.fteext = client->fteprotocolextensions;
			clst.zext = client->zquake_extensions;
			clst.cl = cl;

			if (ent != vent || host_client->viewent == j+1)
				clst.modelindex = 0;

#ifdef SERVER_DEMO_PLAYBACK
			if (sv.demostatevalid)
				clst.health = 100;
#endif

			clst.isself = false;
			if ((cl == client || cl->controller == client))
			{
				clst.isself = true;
				clst.spectator = 0;
				if (client->spectator)
				{
					if (client->spec_track > 0)
					{
						edict_t *s = EDICT_NUM(svprogfuncs, client->spec_track);

						clst.spectator = 2;
						clst.mins = s->v->mins;
						clst.maxs = s->v->maxs;
						clst.health = s->v->health;
						clst.weaponframe = s->v->weaponframe;
					}
					else
					{
						clst.spectator = 1;
						clst.health = 1;
					}
				}
			}
			else if (client->spectator)
			{
				clst.health=100;
				if (client->spec_track == j+1)
					clst.spectator = 2;
				else
					clst.spectator = 1;
			}
			if (isbot)
			{
				clst.lastcmd = NULL;
				clst.velocity = NULL;
				clst.localtime = sv.time;
				VectorCopy(clst.origin, frame->playerpositions[j]);
			}
			else
			{
				VectorMA(clst.origin, (sv.time - clst.localtime), clst.velocity, frame->playerpositions[j]);
			}
			frame->playerpresent[j] = true;
			SV_WritePlayerToClient(msg, &clst);
		}

//FIXME: Name flags
		//player is visible, now would be a good time to update what the player is like.
/*		pflags = 0;
#ifdef PEXT_VWEAP
		if (client->fteprotocolextensions & PEXT_VWEAP && client->otherclientsknown[j].vweap != ent->xv->vweapmodelindex)
		{
			pflags |= 1;
			client->otherclientsknown[j].vweap = ent->xv->vweapmodelindex;
		}
#endif
		if (pflags)
		{
			ClientReliableWrite_Begin(client, svc_ftesetclientpersist, 10);
			ClientReliableWrite_Short(client, pflags);
			if (pflags & 1)
				ClientReliableWrite_Short(client, client->otherclientsknown[j].vweap);
		}
*/
	}
}

#ifdef NQPROT
void SVNQ_EmitEntityState(sizebuf_t *msg, entity_state_t *ent)
{
	edict_t *ed = EDICT_NUM(svprogfuncs, ent->number);
	entity_state_t *baseline = &ed->baseline;

int i, eff;
float miss;
unsigned int bits=0;

int glowsize=0, glowcolor=0, colourmod=0;

	for (i=0 ; i<3 ; i++)
	{
		miss = ent->origin[i] - baseline->origin[i];
		if ( miss < -0.1 || miss > 0.1 )
			bits |= NQU_ORIGIN1<<i;
	}

	if (ent->angles[0] != baseline->angles[0] )
		bits |= NQU_ANGLE1;

	if (ent->angles[1] != baseline->angles[1] )
		bits |= NQU_ANGLE2;

	if (ent->angles[2] != baseline->angles[2] )
		bits |= NQU_ANGLE3;

	if (ent->dpflags & RENDER_STEP)
		bits |= NQU_NOLERP;	// don't mess up the step animation

	if (baseline->colormap != ent->colormap)
		bits |= NQU_COLORMAP;

	if (baseline->skinnum != ent->skinnum)
		bits |= NQU_SKIN;

	if (baseline->frame != ent->frame)
		bits |= NQU_FRAME;

	eff = ent->effects;

	if ((baseline->effects & 0x00ff) != ((int)eff & 0x00ff))
		bits |= NQU_EFFECTS;

	if (baseline->modelindex != ent->modelindex)
		bits |= NQU_MODEL;

	if (ent->number >= 256)
		bits |= NQU_LONGENTITY;


	if (host_client->protocol == SCP_FITZ666)
	{
		if (baseline->trans != ent->trans)
			bits |= FITZU_ALPHA;

		if (baseline->scale != ent->scale)
			bits |= RMQU_SCALE;

		if ((baseline->frame&0xff00) != (ent->frame&0xff00))
			bits |= FITZU_FRAME2;

		if ((baseline->modelindex&0xff00) != (ent->modelindex&0xff00))
			bits |= FITZU_MODEL2;

		if (baseline->dpflags & RENDER_STEP)
			bits |= FITZU_LERPFINISH;
	}
	else if (host_client->protocol == SCP_BJP3)
	{
	}
	else if (0)
	{
#if 0
		if (baseline.trans != ent->xv->alpha)
			if (!(baseline.trans == 1 && !ent->xv->alpha))
				bits |= DPU_ALPHA;
		if (baseline.scale != ent->xv->scale)
		{
			if (ent->xv->scale != 0 || ent->baseline.scale != 1)
				bits |= DPU_SCALE;
		}

		if (ent->v->modelindex >= 256)	//as much as protocols can handle
			bits |= DPU_MODEL2;

		if ((baseline.effects&0xff00) != ((int)eff & 0xff00))
			bits |= DPU_EFFECTS2;

		if (ent->xv->exteriormodeltoclient == EDICT_TO_PROG(svprogfuncs, host_client->edict))
			bits |= DPU_EXTERIORMODEL;
		if (ent->xv->viewmodelforclient == EDICT_TO_PROG(svprogfuncs, host_client->edict))
			bits |= DPU_VIEWMODEL;


		glowsize = ent->xv->glow_size*0.25f;
		glowcolor = ent->xv->glow_color;

		colourmod = ((int)bound(0, ent->xv->colormod[0] * (7.0f / 32.0f), 7) << 5) | ((int)bound(0, ent->xv->colormod[1] * (7.0f / 32.0f), 7) << 2) | ((int)bound(0, ent->xv->colormod[2] * (3.0f / 32.0f), 3) << 0);

		if (0 != glowsize)
			bits |= DPU_GLOWSIZE;
		if (0 != glowcolor)
			bits |= DPU_GLOWCOLOR;

		if (0 != colourmod)
			bits |= DPU_COLORMOD;
#endif
	}
	else
	{
		if (ent->modelindex >= 256)	//as much as protocols can handle
			return;
		if (ent->number >= 600)		//too many for a conventional nq client.
			return;
	}


	if (bits & 0xFF000000)
		bits |= DPU_EXTEND2;
	if (bits & 0xFF0000)
		bits |= DPU_EXTEND1;
	if (bits & 0xFF00)
		bits |= NQU_MOREBITS;


//
// write the message
//
	MSG_WriteByte (msg,(bits | NQU_SIGNAL) & 0xff); //gets caught on 'range error'

	if (bits & NQU_MOREBITS)	MSG_WriteByte (msg, (bits>>8)&0xff);
	if (bits & DPU_EXTEND1)		MSG_WriteByte (msg, (bits>>16)&0xff);
	if (bits & DPU_EXTEND2)		MSG_WriteByte (msg, (bits>>24)&0xff);

	if (bits & NQU_LONGENTITY)
		MSG_WriteShort (msg,ent->number);
	else
		MSG_WriteByte (msg,ent->number);

	if (bits & NQU_MODEL)
	{
		if (host_client->protocol == SCP_BJP3)
			MSG_WriteShort(msg,	ent->modelindex & 0xffff);
		else
			MSG_WriteByte (msg,	ent->modelindex & 0xff);
	}
	if (bits & NQU_FRAME)		MSG_WriteByte (msg, ent->frame & 0xff);
	if (bits & NQU_COLORMAP)	MSG_WriteByte (msg, ent->colormap & 0xff);
	if (bits & NQU_SKIN)		MSG_WriteByte (msg, ent->skinnum & 0xff);
	if (bits & NQU_EFFECTS)		MSG_WriteByte (msg, eff & 0x00ff);
	if (bits & NQU_ORIGIN1)		MSG_WriteCoord (msg, ent->origin[0]);
	if (bits & NQU_ANGLE1)		MSG_WriteAngle(msg, ent->angles[0]);
	if (bits & NQU_ORIGIN2)		MSG_WriteCoord (msg, ent->origin[1]);
	if (bits & NQU_ANGLE2)		MSG_WriteAngle(msg, ent->angles[1]);
	if (bits & NQU_ORIGIN3)		MSG_WriteCoord (msg, ent->origin[2]);
	if (bits & NQU_ANGLE3)		MSG_WriteAngle(msg, ent->angles[2]);

	if (host_client->protocol == SCP_FITZ666)
	{
		if (bits & FITZU_ALPHA)		MSG_WriteByte(msg, ent->trans);
		if (bits & RMQU_SCALE)		MSG_WriteByte(msg, ent->scale);
		if (bits & FITZU_FRAME2)	MSG_WriteByte(msg, ent->frame>>8);
		if (bits & FITZU_MODEL2)	MSG_WriteByte(msg, ent->modelindex>>8);
		if (bits & FITZU_LERPFINISH)MSG_WriteByte(msg, bound(0, (int)((ed->v->nextthink - sv.world.physicstime) * 255), 255));
	}
	else if (host_client->protocol == SCP_BJP3)
	{
	}
	else
	{
		if (bits & DPU_ALPHA)		MSG_WriteByte(msg, ent->trans);
		if (bits & DPU_SCALE)		MSG_WriteByte(msg, ent->scale);
		if (bits & DPU_EFFECTS2)	MSG_WriteByte(msg, eff >> 8);
		if (bits & DPU_GLOWSIZE)	MSG_WriteByte(msg, glowsize);
		if (bits & DPU_GLOWCOLOR)	MSG_WriteByte(msg, glowcolor);
		if (bits & DPU_COLORMOD)	MSG_WriteByte(msg, colourmod);
		if (bits & DPU_FRAME2)		MSG_WriteByte(msg, ent->frame >> 8);
		if (bits & DPU_MODEL2)		MSG_WriteByte(msg, ent->modelindex >> 8);
	}
}
#endif

typedef struct gibfilter_s {
	struct gibfilter_s *next;
	int modelindex;
	int minframe;
	int maxframe;
} gibfilter_t;
gibfilter_t *gibfilter;
void SV_GibFilterPurge(void)
{
	gibfilter_t *gf;
	while(gibfilter)
	{
		gf = gibfilter;
		gibfilter = gibfilter->next;

		Z_Free(gf);
	}
}

void SV_GibFilterAdd(char *modelname, int min, int max, qboolean allowwarn)
{
	int i;
	gibfilter_t *gf;

	for (i=1; sv.strings.model_precache[i] ; i++)
		if (!strcmp(sv.strings.model_precache[i], modelname))
			break;
	if (!sv.strings.model_precache[i])
	{
		if (allowwarn)
			Con_Printf("Filtered model \"%s\" was not precached\n", modelname);
		return;	//model not in use.
	}

	gf = Z_Malloc(sizeof(gibfilter_t));
	gf->modelindex = i;
	gf->minframe = ((min==-1)?0:min);
	gf->maxframe = ((max==-1)?0x80000000:max);
	gf->next = gibfilter;
	gibfilter = gf;
}
void SV_GibFilterInit(void)
{
	char buffer[2048];
	char *file;
	int min, max;

	SV_GibFilterPurge();

	if (svs.gametype != GT_PROGS && svs.gametype != GT_Q1QVM)
		return;

	file = COM_LoadStackFile("gibfiltr.cfg", buffer, sizeof(buffer), NULL);
	if (!file)
	{
		Con_DPrintf("gibfiltr.cfg file was not found. Using defaults\n");
		SV_GibFilterAdd("progs/gib1.mdl", -1, -1, false);
		SV_GibFilterAdd("progs/gib2.mdl", -1, -1, false);
		SV_GibFilterAdd("progs/gib3.mdl", -1, -1, false);
		SV_GibFilterAdd("progs/h_player.mdl", -1, -1, false);
//		SV_GibFilterAdd("progs/player.mdl", 49, 49, false);
//		SV_GibFilterAdd("progs/player.mdl", 60, 60, false);
//		SV_GibFilterAdd("progs/player.mdl", 69, 69, false);
//		SV_GibFilterAdd("progs/player.mdl", 84, 84, false);
//		SV_GibFilterAdd("progs/player.mdl", 93, 93, false);
//		SV_GibFilterAdd("progs/player.mdl", 102, 102, false);
		return;
	}
	while(file)
	{
		file = COM_Parse(file);
		if (!file)
		{
			return;
		}
		min = atoi(com_token);
		file = COM_Parse(file);	//handles nulls nicly
		max = atoi(com_token);
		file = COM_Parse(file);
		if (!file)
		{
			Con_Printf("Sudden ending to gibfiltr.cfg\n");
			return;
		}
		SV_GibFilterAdd(com_token, min, max, true);
	}
}
qboolean SV_GibFilter(edict_t	*ent)
{
	int indx = ent->v->modelindex;
	int frame = ent->v->frame;
	gibfilter_t *gf;

	for (gf = gibfilter; gf; gf=gf->next)
	{
		if (gf->modelindex == indx)
			if (frame >= gf->minframe && frame <= gf->maxframe)
				return true;
	}

	return false;
}


#ifdef Q2BSPS
static int		clientarea;

unsigned int Q2BSP_FatPVS(model_t *mod, vec3_t org, qbyte *buffer, unsigned int buffersize, qboolean add)
{//fixme: this doesn't add areas
	int		leafnum;
	leafnum = CM_PointLeafnum (mod, org);
	clientarea = CM_LeafArea (mod, leafnum);

	return SV_Q2BSP_FatPVS (mod, org, buffer, buffersize, add);
}

qboolean Q2BSP_EdictInFatPVS(model_t *mod, pvscache_t *ent, qbyte *pvs)
{
	int i,l;
	int nullarea = (mod->fromgame == fg_quake2)?0:-1;
	if (clientarea == ent->areanum)
	{
		if (clientarea == nullarea)
			return false;
	}
	else  if (!CM_AreasConnected (mod, clientarea, ent->areanum))
	{	// doors can legally straddle two areas, so
		// we may need to check another one
		if (ent->areanum2 == nullarea
			|| !CM_AreasConnected (mod, clientarea, ent->areanum2))
			return false;		// blocked by a door
	}

	if (ent->num_leafs == -1)
	{	// too many leafs for individual check, go by headnode
		if (!CM_HeadnodeVisible (mod, ent->headnode, pvs))
			return false;
	}
	else
	{	// check individual leafs
		for (i=0 ; i < ent->num_leafs ; i++)
		{
			l = ent->leafnums[i];
			if (pvs[l >> 3] & (1 << (l&7) ))
				break;
		}
		if (i == ent->num_leafs)
			return false;		// not visible
	}
	return true;
}
#endif

#ifdef SERVER_DEMO_PLAYBACK
static void SV_Snapshot_Build_Playback(client_t *client, packet_entities_t *pack)
{
	int e;
	entity_state_t	*state;
	mvdentity_state_t *dement;
		for (e=1, dement=&sv.demostate[e] ; e<=sv.demomaxents ; e++, dement++)
		{
			if (!dement->modelindex)
				continue;

			if (e >= 1 && e <= svs.allocated_client_slots)
				continue;

			if (pack->num_entities == pack->max_entities)
				continue;	// all full

			//the entity would mess up the client and possibly disconnect them.
			//FIXME: add an option to drop clients... entity fog could be killed in this way.
			if (e >= 512 && !(client->fteprotocolextensions & PEXT_ENTITYDBL))
				continue;
			if (e >= 1024 && !(client->fteprotocolextensions & PEXT_ENTITYDBL2))
				continue;
//			if (dement->modelindex >= 256 && !(client->fteprotocolextensions & PEXT_MODELDBL))
//				continue;

			state = &pack->entities[pack->num_entities];
			pack->num_entities++;

			state->number = e;
			state->flags = EF_DIMLIGHT;
			VectorCopy (dement->origin, state->origin);
			state->angles[0] = dement->angles[0]*360.0f/256;
			state->angles[1] = dement->angles[1]*360.0f/256;
			state->angles[2] = dement->angles[2]*360.0f/256;
			state->modelindex = dement->modelindex;
			state->frame = dement->frame;
			state->colormap = dement->colormap;
			state->skinnum = dement->skinnum;
			state->effects = dement->effects;

#ifdef PEXT_SCALE
			state->scale = dement->scale;
#endif
#ifdef PEXT_TRANS
			state->trans = dement->trans;
#endif
#ifdef PEXT_FATNESS
			state->fatness = dement->fatness;
#endif
		}

		for (e = 0; e < sv.numdemospikes; e++)
		{
			if (SV_DemoNailUpdate (e))
				continue;
		}
}
#endif

void SV_Snapshot_BuildStateQ1(entity_state_t *state, edict_t *ent, client_t *client, packet_entities_t *pack)
{
//builds an entity_state from an entity
//note that client can be null, for building baselines.

	int i;
	state->number = NUM_FOR_EDICT(svprogfuncs, ent);

	state->u.q1.msec = 0;
	state->u.q1.pmovetype = 0;
	state->u.q1.movement[0] = 0;
	state->u.q1.movement[1] = 0;
	state->u.q1.movement[2] = 0;
	state->u.q1.velocity[0] = 0;
	state->u.q1.velocity[1] = 0;
	state->u.q1.velocity[2] = 0;

	VectorCopy (ent->v->origin, state->origin);
	VectorCopy (ent->v->angles, state->angles);

	state->u.q1.weaponframe = 0;
	if ((state->number-1) < (unsigned int)sv.allocated_client_slots && (client == &svs.clients[state->number-1] || client == svs.clients[state->number-1].controller || (client && (!client->edict || client->spec_track == state->number))))
		if (!client || !(client->fteprotocolextensions2 & PEXT2_PREDINFO))
			state->u.q1.weaponframe = ent->v->weaponframe;
	if ((state->number-1) < (unsigned int)sv.allocated_client_slots && ent->v->movetype && client)
	{
		client_t *cl = &svs.clients[state->number-1];
		if (cl->isindependant)
		{
			state->u.q1.pmovetype = ent->v->movetype;
			if (cl != client && client)
			{	/*only generate movement values if the client doesn't already know them...*/
				state->u.q1.movement[0] = ent->xv->movement[0];
				state->u.q1.movement[1] = ent->xv->movement[1];
				state->u.q1.movement[2] = ent->xv->movement[2];
				state->u.q1.msec = bound(0, 1000*(sv.time - cl->localtime), 255);
			}

			state->u.q1.velocity[0] = ent->v->velocity[0] * 8;
			state->u.q1.velocity[1] = ent->v->velocity[1] * 8;
			state->u.q1.velocity[2] = ent->v->velocity[2] * 8;
		}
		else if (ent == cl->edict)
		{
			state->u.q1.velocity[0] = ent->v->velocity[0] * 8;
			state->u.q1.velocity[1] = ent->v->velocity[1] * 8;
			state->u.q1.velocity[2] = ent->v->velocity[2] * 8;
		}

		//fixme: deal with fixangles
		if (client->fteprotocolextensions2 & PEXT2_PREDINFO)
		{
			state->u.q1.vangle[0] = ANGLE2SHORT(ent->v->v_angle[0]);
			state->u.q1.vangle[1] = ANGLE2SHORT(ent->v->v_angle[1]);
			state->u.q1.vangle[2] = ANGLE2SHORT(ent->v->v_angle[2]);
		}
		else
		{
			if (client->fteprotocolextensions2 & PEXT2_REPLACEMENTDELTAS)
			if (state->u.q1.pmovetype && (state->u.q1.pmovetype != MOVETYPE_TOSS && state->u.q1.pmovetype != MOVETYPE_BOUNCE))
			{
				state->angles[0] = ent->v->v_angle[0]/-3.0;
				state->angles[1] = ent->v->v_angle[1];
				state->angles[2] = ent->v->v_angle[2];
			}
		}
	}

	if (client && client->edict && (ent->v->owner == client->edict->entnum))
		state->solidsize = 0;
	else if (ent->v->solid == SOLID_BSP || (ent->v->skin < 0 && ent->v->modelindex))
		state->solidsize = ES_SOLID_BSP;
	else if (ent->v->solid == SOLID_BBOX || ent->v->solid == SOLID_SLIDEBOX || ent->v->skin < 0)
		state->solidsize = ent->solidsize;
	else
		state->solidsize = 0;

	state->dpflags = 0;
	if (ent->xv->viewmodelforclient)
	{	//this ent would have been filtered out by now if its not ours
		//if ent->viewmodelforclient == client then:
		state->dpflags |= RENDER_VIEWMODEL;
	}
	state->colormap = ent->v->colormap;
	if (state->colormap >= 1024)
		state->dpflags |= RENDER_COLORMAPPED;
	else if (client && state->colormap > client->max_net_clients)
		state->colormap = 0;

	if (ent->xv->exteriormodeltoclient && client)
	{
		if (ent->xv->exteriormodeltoclient == EDICT_TO_PROG(svprogfuncs, client->edict))
			state->dpflags |= RENDER_EXTERIORMODEL;
		//everyone else sees it normally.
	}

	if (0)//ent->xv->basebone < 0)
	{
		if (ent->xv->skeletonindex && pack)
		{
			framestate_t fs;
			fs.skeltype = SKEL_IDENTITY;
			fs.bonecount = 0;
			skel_lookup(sv.world.progs, ent->xv->skeletonindex, &fs);
			if (fs.skeltype == SKEL_RELATIVE && fs.bonecount)
			{
				Bones_To_PosQuat4(fs.bonecount, fs.bonestate, AllocateBoneSpace(pack, state->bonecount = fs.bonecount, &state->boneoffset));
				state->dpflags |= RENDER_COMPLEXANIMATION;
			}
		}
	}
	else
	{
		state->basebone = ent->xv->basebone;
		state->baseframe = ent->xv->baseframe;
	}

	if (!ent->v->movetype || ent->v->movetype == MOVETYPE_STEP)
		state->dpflags |= RENDER_STEP;

	state->modelindex = ent->v->modelindex;
	state->modelindex2 = ent->xv->vw_index;
	state->frame = ent->v->frame;
	state->skinnum = ent->v->skin;
	state->effects = ent->v->effects;
	state->effects |= (int)ent->xv->modelflags<<24;
	state->hexen2flags = ent->xv->drawflags;
	state->abslight = (int)(ent->xv->abslight*255) & 255;
	state->tagentity = ent->xv->tag_entity;
	state->tagindex = ent->xv->tag_index;

	state->light[0] = ent->xv->color[0]*1024;
	state->light[1] = ent->xv->color[1]*1024;
	state->light[2] = ent->xv->color[2]*1024;
	state->light[3] = ent->xv->light_lev;
	state->lightstyle = ent->xv->style;
	state->lightpflags = ent->xv->pflags;
	state->u.q1.traileffectnum = ent->xv->traileffectnum;

	if (ent->xv->gravitydir[2] == -1)
	{
		state->u.q1.gravitydir[0] = 0;
		state->u.q1.gravitydir[1] = 0;
	}
	else if ((!ent->xv->gravitydir[0] && !ent->xv->gravitydir[1] && !ent->xv->gravitydir[2]))// || (ent->xv->gravitydir[2] == -1))
	{
		vec3_t ang;
		if (sv.world.g.defaultgravitydir[2] == -1)
		{
			state->u.q1.gravitydir[0] = 0;
			state->u.q1.gravitydir[1] = 0;
		}
		else
		{
			vectoangles(sv.world.g.defaultgravitydir, ang);
			state->u.q1.gravitydir[0] = ((ang[0]/360) * 256) - 192;
			state->u.q1.gravitydir[1] = (ang[1]/360) * 256;
		}
	}
	else
	{
		vec3_t ang;
		vectoangles(ent->xv->gravitydir, ang);
		state->u.q1.gravitydir[0] = ((ang[0]/360) * 256) - 192;
		state->u.q1.gravitydir[1] = (ang[1]/360) * 256;
	}

	if (((int)ent->v->flags & FL_CLASS_DEPENDENT) && client && client->playerclass)	//hexen2 wierdness.
	{
		char modname[MAX_QPATH];
		Q_strncpyz(modname, sv.strings.model_precache[state->modelindex], sizeof(modname));
		if (strlen(modname)>5)
		{
			modname[strlen(modname)-5] = client->playerclass+'0';
			state->modelindex = SV_ModelIndex(modname);
		}
	}

	if (state->effects & DPEF_LOWPRECISION)
		state->effects &= ~DPEF_LOWPRECISION;	//we don't support it, nor does dp any more. strip it.

	if (state->effects & EF_FULLBRIGHT)	//wrap the field for fte clients (this is horrible)
		state->hexen2flags |= MLS_FULLBRIGHT;

#ifdef NQPROT
	if (progstype != PROG_QW)
	{
		if (progstype == PROG_TENEBRAE)
		{
			//tenebrae has some hideous hacks
			if (!strcmp(sv.strings.model_precache[state->modelindex], "progs/w_light.spr") ||
				!strcmp(sv.strings.model_precache[state->modelindex], "progs/b_light.spr") ||
				!strcmp(sv.strings.model_precache[state->modelindex], "progs/s_light.spr") ||
				!strcmp(sv.strings.model_precache[state->modelindex], "progs/flame.mdl") ||
				!strcmp(sv.strings.model_precache[state->modelindex], "progs/flame2.mdl"))
			{
				//fixme: add some default colours
				state->lightpflags |= PFLAGS_FULLDYNAMIC;
				if (!state->light[3])
					state->light[3] = 350;
			}

			if (!strcmp (sv.strings.model_precache[state->modelindex], "progs/lavaball.mdl"))
			{
				state->lightpflags |= PFLAGS_FULLDYNAMIC;
				state->skinnum = 17;
				state->light[3] = 270;
			}
		}
		if (state->effects && client && ISQWCLIENT(client))	//don't send extra nq effects to a qw client.
		{
			//EF_NODRAW doesn't draw the model.
			//The client still needs to know about it though, as it might have other effects on it.
			if (progstype == PROG_H2)
			{
				if (state->effects == H2EF_NODRAW)
				{
					//actually, H2 is pretty lame about this
					state->effects = 0;
					state->modelindex = 0;
					state->frame = 0;
					state->colormap = 0;
					state->abslight = 0;
					state->skinnum = 0;
					state->hexen2flags = 0;
				}
			}
			else if (progstype == PROG_TENEBRAE)
			{
				if (state->effects & 16)	//tenebrae's EF_FULLDYNAMIC
				{
					state->effects &= ~16;
					state->lightpflags |= PFLAGS_FULLDYNAMIC;
				}
				if (state->effects & 32)	//tenebrae's EF_GREEN
				{
					state->effects &= ~32;
					state->effects |= EF_GREEN;
				}
			}
			else
			{
				if (state->effects & NQEF_NODRAW)
					state->modelindex = 0;
			}

			if (state->number <= sv.allocated_client_slots) // clear only client ents
				state->effects &= ~ (QWEF_FLAG1|QWEF_FLAG2);

			if ((state->effects & EF_DIMLIGHT) && !(state->effects & (EF_RED|EF_BLUE)))
			{
				int it = ent->v->items;
				state->effects &= ~EF_DIMLIGHT;
				if ((it & (IT_INVULNERABILITY|IT_QUAD)) == (IT_INVULNERABILITY|IT_QUAD))
					state->effects |= EF_RED|EF_BLUE;
				else if (it & IT_INVULNERABILITY)
					state->effects |= EF_RED;
				else if (it & IT_QUAD)
					state->effects |= EF_BLUE;
				else
					state->effects |= EF_DIMLIGHT;
			}
		}
	}
#endif

	if (!ent->xv->colormod[0] && !ent->xv->colormod[1] && !ent->xv->colormod[2])
	{
		state->colormod[0] = (256)/8;
		state->colormod[1] = (256)/8;
		state->colormod[2] = (256)/8;
	}
	else
	{
		i = ent->xv->colormod[0]*(256/8); state->colormod[0] = bound(0, i, 255);
		i = ent->xv->colormod[1]*(256/8); state->colormod[1] = bound(0, i, 255);
		i = ent->xv->colormod[2]*(256/8); state->colormod[2] = bound(0, i, 255);
	}
	if (!ent->xv->glowmod[0] && !ent->xv->glowmod[1] && !ent->xv->glowmod[2])
	{
		state->glowmod[0] = (256/8);
		state->glowmod[1] = (256/8);
		state->glowmod[2] = (256/8);
	}
	else
	{
		state->glowmod[0] = ent->xv->glowmod[0]*(256/8);
		state->glowmod[1] = ent->xv->glowmod[1]*(256/8);
		state->glowmod[2] = ent->xv->glowmod[2]*(256/8);
	}
	state->glowsize = ent->xv->glow_size*0.25;
	state->glowcolour = ent->xv->glow_color;
	if (ent->xv->glow_trail)
		state->dpflags |= RENDER_GLOWTRAIL;


#ifdef PEXT_SCALE
	if (!ent->xv->scale)
		state->scale = 1*16;
	else
		state->scale = bound(1, ent->xv->scale*16, 255);

#endif
#ifdef PEXT_TRANS
	if (!ent->xv->alpha)
		state->trans = 255;
	else
		state->trans = bound(1, ent->xv->alpha*255, 255);

	//QSG_DIMENSION_PLANES - if the only shared dimensions are ghost dimensions, Set half alpha.
	if (client && client->edict)
	{
		if (((int)client->edict->xv->dimension_see & (int)ent->xv->dimension_ghost))
			if (!((int)client->edict->xv->dimension_see & ((int)ent->xv->dimension_seen & ~(int)ent->xv->dimension_ghost)) )
			{
				if (ent->xv->dimension_ghost_alpha)
					state->trans *= ent->xv->dimension_ghost_alpha;
				else
					state->trans *= 0.5;
			}
	}
#endif
#ifdef PEXT_FATNESS
	state->fatness = ent->xv->fatness;
#endif

#pragma warningmsg("TODO: Fix attachments for more vanilla clients")
}

void SV_Snapshot_BuildQ1(client_t *client, packet_entities_t *pack, pvscamera_t *cameras, edict_t *clent)
{
//pvs and clent can be null, but only if the other is also null
	int e, i;
	edict_t *ent, *tracecullent;	//tracecullent is different from ent because attached models cull the parent instead. also, null for entities which are not culled.
	entity_state_t	*state;
#define DEPTHOPTIMISE
#ifdef DEPTHOPTIMISE
	vec3_t org;
	static float distances[32768];
	float dist;
#endif
	globalvars_t *pr_globals = PR_globals(svprogfuncs, PR_CURRENT);
	int pvsflags;
	int limit;
	int c, maxc = cameras?cameras->numents:0;

	//this entity is watching from outside themselves. The client is tricked into thinking that they themselves are in the view ent, and a new dummy ent (the old them) must be spawned.
	if (client->viewent && ISQWCLIENT(client))
	{
//FIXME: this hack needs cleaning up
#ifdef DEPTHOPTIMISE
		distances[0] = 0;
#endif
		state = &pack->entities[pack->num_entities];
		pack->num_entities++;

		SV_Snapshot_BuildStateQ1(state, clent, client, pack);

		state->number = client - svs.clients + 1;

		//yeah, I doubt anyone will need this
		if (progstype == PROG_QW)
		{
			if ((int)clent->v->effects & QWEF_FLAG1)
			{
				memcpy(&pack->entities[pack->num_entities], state, sizeof(*state));
				state = &pack->entities[pack->num_entities];
				pack->num_entities++;
				state->modelindex = SV_ModelIndex("progs/flag.mdl");
				state->frame = 0;
				state->number++;	//yeek
				state->skinnum = 0;
			}
			else if ((int)clent->v->effects & QWEF_FLAG2)
			{
				memcpy(&pack->entities[pack->num_entities], state, sizeof(*state));
				state = &pack->entities[pack->num_entities];
				pack->num_entities++;
				state->modelindex = SV_ModelIndex("progs/flag.mdl");
				state->frame = 0;
				state->number++;	//yeek
				state->skinnum = 1;
			}
		}
	}


	/*legacy qw clients get their players separately*/
	if (ISQWCLIENT(client) && !(client->fteprotocolextensions2 & PEXT2_REPLACEMENTDELTAS))
		e = min(sv.allocated_client_slots+1, client->max_net_clients);
	else
		e = 1;

	limit = sv.world.num_edicts;
	if (client->max_net_ents < limit)
	{
		limit = client->max_net_ents;
		if (!(client->plimitwarned & PLIMIT_ENTITIES))
		{
			client->plimitwarned |= PLIMIT_ENTITIES;
			SV_ClientPrintf(client, PRINT_HIGH, "WARNING: Your client's network protocol only supports %i entities. Please upgrade or enable extensions.\n", client->max_net_ents);
		}
	}

	if (client->penalties & BAN_BLIND)
	{
		e = client->edict->entnum;
		limit = e+1;
	}

	for ( ; e<limit ; e++)
	{
		ent = EDICT_NUM(svprogfuncs, e);
		if (ED_ISFREE(ent))
			continue;

		if (ent->xv->customizeentityforclient)
		{
			pr_global_struct->self = EDICT_TO_PROG(svprogfuncs, ent);
			pr_global_struct->other = (clent?EDICT_TO_PROG(svprogfuncs, clent):0);
			PR_ExecuteProgram(svprogfuncs, ent->xv->customizeentityforclient);
			if(!G_FLOAT(OFS_RETURN))
				continue;
		}

		if (progstype != PROG_QW)
		{
//			if (progstype == PROG_H2)
//				if (ent->v->effects == H2EF_NODRAW)
//					continue;
			if ((int)ent->v->effects & EF_MUZZLEFLASH)
			{
				if (needcleanup < e)
				{
					needcleanup = e;
				}
			}
		}

		pvsflags = ent->xv->pvsflags;
		for (c = 0; c < maxc; c++)
		{
			if (ent == cameras->ent[c])
				break;
		}
		if (c < maxc)
			tracecullent = NULL;
		else if (ent->xv->viewmodelforclient)
		{
			if (ent->xv->viewmodelforclient != (clent?EDICT_TO_PROG(svprogfuncs, clent):0))
				continue;
			tracecullent = NULL;
		}
		else
		{
			// ignore ents without visible models
			if (!ent->xv->SendEntity && (!ent->v->modelindex || !*PR_GetString(svprogfuncs, ent->v->model)) && !((int)ent->xv->pflags & PFLAGS_FULLDYNAMIC) && ent->v->skin >= 0)
				continue;

			if (cameras)	//self doesn't get a pvs test, to cover teleporters
			{
				if ((int)ent->v->effects & EF_NODEPTHTEST)
					tracecullent = NULL;
				else if ((pvsflags & PVSF_MODE_MASK) < PVSF_USEPHS)
				{
					//branch out to the pvs testing.
					if (ent->xv->tag_entity)
					{
						int c = 10;
						tracecullent = ent;
						while(tracecullent->xv->tag_entity&&c-->0)
						{
							tracecullent = EDICT_NUM(svprogfuncs, tracecullent->xv->tag_entity);
						}
						if (tracecullent == clent)
							tracecullent = NULL;
						else if (tracecullent->xv->viewmodelforclient)
						{
							//special hack so viewmodelforclient on the root of the tagged entity overrides pvs
							if (tracecullent->xv->viewmodelforclient != (clent?EDICT_TO_PROG(svprogfuncs, clent):0))
								continue;
							tracecullent = NULL;	//don't tracecull
						}
						else
						{
							if (!sv.world.worldmodel->funcs.EdictInFatPVS(sv.world.worldmodel, &((wedict_t*)tracecullent)->pvsinfo, cameras->pvs))
								continue;
						}
					}
					else
					{
						if (!sv.world.worldmodel->funcs.EdictInFatPVS(sv.world.worldmodel, &((wedict_t*)ent)->pvsinfo, cameras->pvs))
							continue;
						tracecullent = ent;
					}
				}
				else if ((pvsflags & PVSF_MODE_MASK) == PVSF_USEPHS && sv.world.worldmodel->fromgame == fg_quake)
				{
					int cluster;
					unsigned char *mask;
					qbyte *phs = sv.world.worldmodel->phs;
					if (phs)
					{
						//FIXME: this lookup should be cachable or something.
						if (client->edict)
							cluster = sv.world.worldmodel->funcs.ClusterForPoint(sv.world.worldmodel, client->edict->v->origin);
						else
							cluster = -1;	//mvd
						if (cluster >= 0)
						{
							mask = phs + cluster * 4*((sv.world.worldmodel->numclusters+31)>>5);

							cluster = sv.world.worldmodel->funcs.ClusterForPoint (sv.world.worldmodel, ent->v->origin);
							if (cluster >= 0 && !(mask[cluster>>3] & (1<<(cluster&7)) ) )
							{
								continue;
							}
						}
					}
					tracecullent = NULL;
				}
				else
					tracecullent = NULL;

				if (client->gibfilter && SV_GibFilter(ent))
					continue;
			}
			else
				tracecullent = NULL;
		}

		//DP_SV_NODRAWONLYTOCLIENT
		if (ent->xv->nodrawtoclient)	//DP extension.
			if (client->edict && ent->xv->nodrawtoclient == EDICT_TO_PROG(svprogfuncs, client->edict))
				continue;
		//DP_SV_DRAWONLYTOCLIENT
		if (ent->xv->drawonlytoclient)
			if (!client->edict || ent->xv->drawonlytoclient != EDICT_TO_PROG(svprogfuncs, client->edict))
			{
				client_t *split;
				for (split = client->controlled; split; split=split->controlled)
				{
					if (split->edict->xv->view2 == EDICT_TO_PROG(svprogfuncs, ent))
						break;
				}
				if (!split)
					continue;
			}

		//QSG_DIMENSION_PLANES
		if (client->edict)
			if (!((int)client->edict->xv->dimension_see & ((int)ent->xv->dimension_seen | (int)ent->xv->dimension_ghost)))
				continue;	//not in this dimension - sorry...


		if (cameras && tracecullent && !((unsigned int)ent->v->effects & (EF_DIMLIGHT|EF_BLUE|EF_RED|EF_BRIGHTLIGHT|EF_BRIGHTFIELD|EF_NODEPTHTEST)))
		{	//more expensive culling
			if ((e <= sv.allocated_client_slots && sv_cullplayers_trace.value) || sv_cullentities_trace.value)
				if (Cull_Traceline(cameras, tracecullent))
					continue;
		}

		//EXT_CSQC
		if (SV_AddCSQCUpdate(client, ent))	//csqc took it.
			continue;

		if (ISQWCLIENT(client))
		{
			if (SV_AddNailUpdate (ent))
				continue;	// added to the special update list
		}

		//the entity would mess up the client and possibly disconnect them.
		//FIXME: add an option to drop clients... entity fog could be killed in this way.
		if (e >= client->max_net_ents)
			continue;
		if (ent->v->modelindex >= client->maxmodels)
			continue;
#ifdef DEPTHOPTIMISE
		if (clent)
		{
			//find distance based upon absolute mins/maxs so bsps are treated fairly.
			//org = clentorg + -0.5*(max+min)
			VectorAdd(ent->v->absmin, ent->v->absmax, org);
			VectorMA(clent->v->origin, -0.5, org, org);
			dist = DotProduct(org, org);	//Length

//			if (dist > 1024*1024)
//				continue;

			// add to the packetentities
			if (pack->num_entities == pack->max_entities)
			{
				float furthestdist = -1;
				int best=-1;
				for (i = 0; i < pack->max_entities; i++)
					if (furthestdist < distances[i])
					{
						furthestdist = distances[i];
						best = i;
					}

				if (furthestdist > dist && best != -1)
				{
					state = &pack->entities[best];
	//				Con_Printf("Dropping ent %s\n", sv.model_precache[state->modelindex]);
					memmove(&distances[best], &distances[best+1], sizeof(*distances)*(pack->num_entities-best-1));
					memmove(state, state+1, sizeof(*state)*(pack->num_entities-best-1));

					best = pack->num_entities-1;

					distances[best] = dist;
					state = &pack->entities[best];
				}
				else
					continue;	// all full
			}
			else
			{
				state = &pack->entities[pack->num_entities];
				distances[pack->num_entities] = dist;
				pack->num_entities++;
			}
		}
		else
#endif
		{
			// add to the packetentities
			if (pack->num_entities == pack->max_entities)
				continue;	// all full
			else
			{
				state = &pack->entities[pack->num_entities];
				pack->num_entities++;
			}
		}

		//its not a nail or anything, pack it up and ship it on
		SV_Snapshot_BuildStateQ1(state, ent, client, pack);
	}
}

void SV_AddCameraEntity(pvscamera_t *cameras, edict_t *ent, vec3_t viewofs)
{
	int i;
	vec3_t org;

	for (i = 0; i < cameras->numents; i++)
	{
		if (cameras->ent[i] == ent)
			return;	//don't add the same ent multiple times (.view2 or portals that can see themselves through other portals).
	}

	if (viewofs)
		VectorAdd (ent->v->origin, viewofs, org);
	else
		VectorCopy (ent->v->origin, org);

	sv.world.worldmodel->funcs.FatPVS(sv.world.worldmodel, org, cameras->pvs, sizeof(cameras->pvs), cameras->numents!=0);
	if (cameras->numents < SV_PVS_CAMERAS)
	{
		cameras->ent[cameras->numents] = ent;
		VectorCopy(org, cameras->org[cameras->numents]);
		cameras->numents++;
	}
}

void SV_Snapshot_SetupPVS(client_t *client, pvscamera_t *camera)
{
	camera->numents = 0;
	for (; client; client = client->controlled)
	{
		if (client->viewent)	//svc_viewentity hack
			SV_AddCameraEntity(camera, EDICT_NUM(svprogfuncs, client->viewent), client->edict->v->view_ofs);
		else
			SV_AddCameraEntity(camera, client->edict, client->edict->v->view_ofs);

		//spectators should always see their targetted player
		if (client->spec_track)
			SV_AddCameraEntity(camera, EDICT_NUM(svprogfuncs, client->spec_track), client->edict->v->view_ofs);

		//view2 support should always see the extra entity
		if (client->edict->xv->view2)
			SV_AddCameraEntity(camera, PROG_TO_EDICT(svprogfuncs, client->edict->xv->view2), NULL);
	}
}

void SV_Snapshot_Clear(packet_entities_t *pack)
{
	pack->num_entities = 0;

	csqcnuments = 0;
	numnails = 0;
}

/*
=============
SVQ3Q1_BuildEntityPacket

Builds a temporary q1 style entity packet for a q3 client
=============
*/
void SVQ3Q1_BuildEntityPacket(client_t *client, packet_entities_t *pack)
{
	pvscamera_t cameras;
	SV_Snapshot_Clear(pack);
	SV_Snapshot_SetupPVS(client, &cameras);
	SV_Snapshot_BuildQ1(client, pack, &cameras, client->edict);
}

/*
=============
SV_WriteEntitiesToClient

Encodes the current state of the world as
a svc_packetentities messages and possibly
a svc_nails message and
svc_playerinfo messages
=============
*/
void SV_WriteEntitiesToClient (client_t *client, sizebuf_t *msg, qboolean ignorepvs)
{
	int		e;
	packet_entities_t	*pack;
	edict_t	*clent;
	client_frame_t	*frame;
	pvscamera_t camerasbuf, *cameras = &camerasbuf;

	// this is the frame we are creating
	frame = &client->frameunion.frames[client->netchan.incoming_sequence & UPDATE_MASK];
	if (!sv.paused)
		memset(frame->playerpresent, 0, sizeof(frame->playerpresent));

	// find the client's PVS
	if (ignorepvs)
	{	//mvd...
		clent = NULL;
		cameras = NULL;
	}
	else
	{
		clent = client->edict;
		if (sv_nopvs.ival)
			cameras = NULL;
#ifdef HLSERVER
		else if (svs.gametype == GT_HALFLIFE)
			SVHL_Snapshot_SetupPVS(client, cameras->pvs, sizeof(cameras->pvs));
#endif
		else 
			SV_Snapshot_SetupPVS(client, cameras);
	}

	host_client = client;
	if (client->fteprotocolextensions2 & PEXT2_REPLACEMENTDELTAS)
	{
		pack = &svs.entstatebuffer;
		if (pack->max_entities < client->max_net_ents)
		{
			pack->max_entities = client->max_net_ents;
			pack->entities = BZ_Realloc(pack->entities, sizeof(*pack->entities) * pack->max_entities);
			memset(pack->entities, 0, sizeof(entity_state_t) * pack->max_entities);
		}
	}
	else
		pack = &frame->entities;
	SV_Snapshot_Clear(pack);

	if (!pack->entities)
		return;

	// put other visible entities into either a packet_entities or a nails message
#ifdef SERVER_DEMO_PLAYBACK
	if (sv.demostatevalid)	//generate info from demo stats
	{
		SV_Snapshot_Build_Playback(client, pack);
	}
	else
#endif
	{
#ifdef HLSERVER
		if (svs.gametype == GT_HALFLIFE)
			SVHL_Snapshot_Build(client, pack, cameras->pvs, clent, ignorepvs);
		else
#endif
			SV_Snapshot_BuildQ1(client, pack, cameras, clent);
	}

#ifdef NQPROT
	if (ISNQCLIENT(client))
	{
		if (client->fteprotocolextensions2 & PEXT2_REPLACEMENTDELTAS)
		{
			SVFTE_EmitPacketEntities(client, pack, msg);
			client->netchan.incoming_sequence++;
		}
		else if (client->protocol == SCP_DARKPLACES6 || client->protocol == SCP_DARKPLACES7)
			SVDP_EmitEntitiesUpdate(client, pack, msg);
		else
		{
			for (e = 0; e < pack->num_entities; e++)
			{
				if (pack->entities[e].number > sv.allocated_client_slots)
					break;
				if (msg->cursize + 32 > msg->maxsize)
					break;
				SVNQ_EmitEntityState(msg, &pack->entities[e]);
			}
			for (; e < pack->num_entities; e++)
			{
				if (msg->cursize + 32 + client->datagram.cursize > msg->maxsize)
					break;
				SVNQ_EmitEntityState(msg, &pack->entities[e]);
			}
			client->netchan.incoming_sequence++;
		}
		SV_EmitCSQCUpdate(client, msg, svcdp_csqcentities);
		return;
	}
#endif

	// encode the packet entities as a delta from the
	// last packetentities acknowledged by the client

	if (client->fteprotocolextensions2 & PEXT2_REPLACEMENTDELTAS)
	{
		SVFTE_EmitPacketEntities(client, pack, msg);
	}
	else
	{
#ifdef QUAKESTATS
		// Z_EXT_TIME protocol extension
		// every now and then, send an update so that extrapolation
		// on client side doesn't stray too far off
		if (ISQWCLIENT(client))
		{
			if (client->fteprotocolextensions & PEXT_ACCURATETIMINGS && sv.world.physicstime - client->nextservertimeupdate > 0)
			{	//the fte pext causes the server to send out accurate timings, allowing for perfect interpolation.
				MSG_WriteByte (msg, svcqw_updatestatlong);
				MSG_WriteByte (msg, STAT_TIME);
				MSG_WriteLong (msg, (int)(sv.world.physicstime * 1000));

				client->nextservertimeupdate = sv.world.physicstime;
			}
			else if (client->zquake_extensions & Z_EXT_SERVERTIME && sv.world.physicstime - client->nextservertimeupdate > 0)
			{	//the zquake ext causes the server to send out peridoic timings, allowing for moderatly accurate game time.
				MSG_WriteByte (msg, svcqw_updatestatlong);
				MSG_WriteByte (msg, STAT_TIME);
				MSG_WriteLong (msg, (int)(sv.world.physicstime * 1000));

				client->nextservertimeupdate = sv.world.physicstime+10;
			}
		}
#endif

		// send over the players in the PVS
		if (svs.gametype != GT_HALFLIFE)
		{
			if (client == &demo.recorder)
				SV_WritePlayersToMVD(client, frame, msg);
			else
				SV_WritePlayersToClient (client, frame, clent, cameras, msg);
		}

		SVQW_EmitPacketEntities (client, pack, msg);
	}

	SV_EmitCSQCUpdate(client, msg, svcfte_csqcentities);

	// now add the specialized nail update
	SV_EmitNailUpdate (msg, ignorepvs);
}

//just goes and makes sure each client tracks all the right SendFlags.
void SV_ProcessSendFlags(client_t *c)
{
	edict_t *ent;
	unsigned int e, h = 0;
	if (!c->csqcactive || !c->pendingcsqcbits)
		return;
	for (e=1 ; e<sv.world.num_edicts && e < c->max_net_ents; e++)
	{
		ent = EDICT_NUM(svprogfuncs, e);
		if (ED_ISFREE(ent))
			continue;
		if (ent->xv->SendFlags)
		{
			c->pendingcsqcbits[e] |= (int)ent->xv->SendFlags & SENDFLAGS_USABLE;
			h = e;
		}
	}
	needcleanup = max(needcleanup, h);
}
void SV_CleanupEnts(void)
{
	int		e;
	edict_t	*ent;
	vec3_t org;

	if (!needcleanup)
		return;

	for (e=1 ; e<=needcleanup ; e++)
	{
		ent = EDICT_NUM(svprogfuncs, e);
		if ((int)ent->v->effects & EF_MUZZLEFLASH)
		{
			ent->v->effects = (int)ent->v->effects & ~EF_MUZZLEFLASH;

			MSG_WriteByte(&sv.multicast, svc_muzzleflash);
			MSG_WriteEntity(&sv.multicast, e);
			VectorCopy(ent->v->origin, org);
			if (progstype == PROG_H2)
				org[2] += 24;
			SV_Multicast(org, MULTICAST_PVS);
		}
		ent->xv->SendFlags = 0;

#ifndef NOLEGACY
		//this is legacy code. we'll just have to live with the slight delay.
		//FIXME: check if Version exists and do it earlier.
		if ((int)ent->xv->Version != sv.csqcentversion[ent->entnum])
		{
			ent->xv->SendFlags = SENDFLAGS_USABLE;
			sv.csqcentversion[ent->entnum] = (int)ent->xv->Version;
		}
#endif
	}
	needcleanup=0;
}
#endif

