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

#include "qwsvdef.h"
#include "pr_common.h"
#ifndef CLIENTONLY

void SV_CleanupEnts(void);

extern cvar_t sv_nailhack;
extern cvar_t sv_cullentities_trace;
extern cvar_t sv_cullplayers_trace;

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

	if (sv.world.worldmodel->fromgame == fg_quake3)
		longs = CM_ClusterSize(mod);
	else
		longs = (CM_NumClusters(mod)+7)/8;
	longs = (longs+(sizeof(long)-1))/sizeof(long);

	// convert leafs to clusters
	for (i=0 ; i<count ; i++)
		leafs[i] = CM_LeafCluster(mod, leafs[i]);

	CM_ClusterPVS(mod, leafs[0], resultbuf, buffersize);


	if (!add)
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

//=============================================================================

// because there can be a lot of nails, there is a special
// network protocol for them
#define	MAX_NAILS	32
edict_t	*nails[MAX_NAILS];
int		numnails;
int		nailcount = 0;
extern	int	sv_nailmodel, sv_supernailmodel, sv_playermodel;

qboolean demonails;

static edict_t *csqcent[MAX_EDICTS];
static int csqcnuments;

qboolean SV_AddNailUpdate (edict_t *ent)
{
	if (ent->v->modelindex != sv_nailmodel
		&& ent->v->modelindex != sv_supernailmodel)
		return false;
	if (sv_nailhack.value)
		return false;

	demonails = false;

	if (numnails == MAX_NAILS)
		return true;

	nails[numnails] = ent;
	numnails++;
	return true;
}

qboolean SV_DemoNailUpdate (int i)
{
	demonails = true;

	if (numnails == MAX_NAILS)
		return true;
	nails[numnails] = (edict_t *)i;
	numnails++;
	return true;
}

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

		bits[0] = x;
		bits[1] = (x>>8) | (y<<4);
		bits[2] = (y>>4);
		bits[3] = z;
		bits[4] = (z>>8) | (p<<4);
		bits[5] = yaw;

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
void SV_EmitCSQCUpdate(client_t *client, sizebuf_t *msg)
{
#ifdef PEXT_CSQC
	qbyte messagebuffer[1024];
	int en;
	int currentsequence = client->netchan.outgoing_sequence;
	unsigned short mask;
	globalvars_t *pr_globals;
	edict_t *ent;
	qboolean writtenheader = false;

	//we don't check that we got some already - because this is delta compressed!

	if (!(client->csqcactive) || !svprogfuncs)
		return;

	pr_globals = PR_globals(svprogfuncs, PR_CURRENT);

	//FIXME: prioritise the list of csqc ents somehow

	csqcmsgbuffer.data = messagebuffer;
	csqcmsgbuffer.maxsize = sizeof(messagebuffer);
	csqcmsgbuffer.packing = msg->packing;
	csqcmsgbuffer.prim = msg->prim;

	for (en = 0; en < csqcnuments; en++)
	{
		ent = csqcent[en];

		if (ent->xv->SendFlags)
		{
			ent->xv->SendFlags = 0;
			ent->xv->Version+=1;
		}

		//prevent mishaps with entities being respawned and things.
		if ((int)ent->xv->Version < sv.csqcentversion[ent->entnum])
			ent->xv->Version = sv.csqcentversion[ent->entnum];
		else
			sv.csqcentversion[ent->entnum] = (int)ent->xv->Version;

		//If it's not changed, don't send
		if (client->csqcentversions[ent->entnum] == sv.csqcentversion[ent->entnum])
			continue;

		csqcmsgbuffer.cursize = 0;
		csqcmsgbuffer.currentbit = 0;
		//Ask CSQC to write a buffer for it.
		G_INT(OFS_PARM0) = EDICT_TO_PROG(svprogfuncs, client->edict);
		G_FLOAT(OFS_PARM1) = 0xffffff;	//psudo compatibility with SendFlags (fte doesn't support properly)
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
			if (!writtenheader)
			{
				writtenheader=true;
				if (client->protocol != SCP_QUAKEWORLD)
					MSG_WriteByte(msg, svcdp_csqcentities);
				else
					MSG_WriteByte(msg, svcfte_csqcentities);
			}
			MSG_WriteShort(msg, ent->entnum);
			if (sv.csqcdebug)	//optional extra.
			{
				if (!csqcmsgbuffer.cursize)
					Con_Printf("Warning: empty csqc packet on %s\n", svprogfuncs->stringtable+ent->v->classname);
				MSG_WriteShort(msg, csqcmsgbuffer.cursize);
			}
			//FIXME: Add a developer mode to write the length of each entity.
			SZ_Write(msg, csqcmsgbuffer.data, csqcmsgbuffer.cursize);

//			Con_Printf("Sending update packet %i\n", ent->entnum);
		}
		else if (sv.csqcentversion[ent->entnum] && !((int)ent->xv->pvsflags & PVSF_NOREMOVE))
		{	//Don't want to send, but they have it already
			if (!writtenheader)
			{
				writtenheader=true;
				MSG_WriteByte(msg, svcfte_csqcentities);
			}

			mask = (unsigned)ent->entnum | 0x8000;
			MSG_WriteShort(msg, mask);
//			Con_Printf("Sending remove 2 packet\n");
		}
		client->csqcentversions[ent->entnum] = sv.csqcentversion[ent->entnum];
		client->csqcentsequence[ent->entnum] = currentsequence;
	}
	//now remove any out dated ones
	for (en = 1; en < sv.world.num_edicts; en++)
	{
		ent = EDICT_NUM(svprogfuncs, en);
		if (client->csqcentversions[en] > 0 && (client->csqcentversions[en] != sv.csqcentversion[en]) && !((int)ent->xv->pvsflags & PVSF_NOREMOVE))
		{
		//	if (!ent->isfree)
		//		continue;

			if (msg->cursize + 5 >= msg->maxsize)	//try removing next frame instead.
			{
			}
			else
			{
				if (!writtenheader)
				{
					writtenheader=true;
					MSG_WriteByte(msg, svcfte_csqcentities);
				}

//				Con_Printf("Sending remove packet %i\n", en);
				mask = (unsigned)en | 0x8000;
				MSG_WriteShort(msg, mask);

				client->csqcentversions[en] = 0;
				client->csqcentsequence[en] = currentsequence;
			}
		}
	}
	if (writtenheader)
		MSG_WriteShort(msg, 0);	//a 0 means no more.

	csqcnuments = 0;

	//prevent the qc from trying to use it at inopertune times.
	csqcmsgbuffer.maxsize = 0;
	csqcmsgbuffer.data = NULL;
#endif
}

#ifdef PEXT_CSQC
void SV_CSQC_DroppedPacket(client_t *client, int sequence)
{
	int i;

	if (!(client->csqcactive))	//we don't need this, but it might be a little faster.
		return;

	for (i = 0; i < sv.world.num_edicts; i++)
		if (client->csqcentsequence[i] == sequence)
			client->csqcentversions[i]--;	//do that update thang (but later).
}
void SV_CSQC_DropAll(client_t *client)
{
	int i;
	if (!(client->csqcactive))	//we don't need this, but it might be a little faster.
		return;

	for (i = 0; i < sv.world.num_edicts; i++)
		client->csqcentversions[i]--;	//do that update thang (but later).
}
#endif

//=============================================================================


/*
==================
SV_WriteDelta

Writes part of a packetentities message.
Can delta from either a baseline or a previous packet_entity
==================
*/
void SV_WriteDelta (entity_state_t *from, entity_state_t *to, sizebuf_t *msg, qboolean force, unsigned int protext)
{
#ifdef PROTOCOLEXTENSIONS
	int evenmorebits=0;
#endif
	int		bits;
	int		i;
	int fromeffects;
	coorddata coordd[3];
	coorddata angled[3];

	static entity_state_t defaultbaseline;
	if (from == &((edict_t*)NULL)->baseline)
		from = &defaultbaseline;

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
	if ((to->effects&0xff00) != (fromeffects&0xff00) && protext & PEXT_DPFLAGS)
		evenmorebits |= U_EFFECTS16;

	if (to->modelindex != from->modelindex)
	{
		bits |= U_MODEL;
		if (to->modelindex > 255)
		{
			if (protext & PEXT_MODELDBL)
				evenmorebits |= U_MODELDBL;
			else
				return;
		}
	}

#ifdef PROTOCOLEXTENSIONS
#ifdef U_SCALE
	if ( to->scale != from->scale && protext & PEXT_SCALE)
		evenmorebits |= U_SCALE;
#endif
#ifdef U_TRANS
	if ( to->trans != from->trans && protext & PEXT_TRANS)
		evenmorebits |= U_TRANS;
#endif
#ifdef U_FATNESS
	if ( to->fatness != from->fatness && protext & PEXT_FATNESS)
		evenmorebits |= U_FATNESS;
#endif

	if ( to->hexen2flags != from->hexen2flags && protext & PEXT_HEXEN2)
		evenmorebits |= U_DRAWFLAGS;
	if ( to->abslight != from->abslight && protext & PEXT_HEXEN2)
		evenmorebits |= U_ABSLIGHT;

	if ((to->colormod[0]!=from->colormod[0]||to->colormod[1]!=from->colormod[1]||to->colormod[2]!=from->colormod[2]) && protext & PEXT_COLOURMOD)
		evenmorebits |= U_COLOURMOD;

	if (to->glowsize != from->glowsize)
		to->dpflags |= 2; // RENDER_GLOWTRAIL

	if (to->dpflags != from->dpflags && protext & PEXT_DPFLAGS)
		evenmorebits |= U_DPFLAGS;

	if ((to->tagentity != from->tagentity || to->tagindex != from->tagindex) && protext & PEXT_DPFLAGS)
		evenmorebits |= U_TAGINFO;

	if ((to->light[0] != from->light[0] || to->light[1] != from->light[1] || to->light[2] != from->light[2] || to->light[3] != from->light[3] || to->lightstyle != from->lightstyle || to->lightpflags != from->lightstyle) && protext & PEXT_DPFLAGS)
		evenmorebits |= U_LIGHT;
#endif

	if (to->flags & U_SOLID)
		bits |= U_SOLID;

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
				SV_Error ("Entity number >= 2048");
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
	if (bits & U_FRAME)
		MSG_WriteByte (msg, to->frame);
	if (bits & U_COLORMAP)
		MSG_WriteByte (msg, to->colormap);
	if (bits & U_SKIN)
		MSG_WriteByte (msg, to->skinnum);
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
		MSG_WriteShort (msg, to->tagentity);
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

/*
=============
SV_EmitPacketEntities

Writes a delta update of a packet_entities_t to the message.

=============
*/
void SV_EmitPacketEntities (client_t *client, packet_entities_t *to, sizebuf_t *msg)
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
			SV_WriteDelta (&from->entities[oldindex], &to->entities[newindex], msg, false, client->fteprotocolextensions);
#else
			SV_WriteDelta (&from->entities[oldindex], &to->entities[newindex], msg, false);
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
			SV_WriteDelta (&ent->baseline, &to->entities[newindex], msg, true, client->fteprotocolextensions);
#else
			SV_WriteDelta (&ent->baseline, &to->entities[newindex], msg, true);
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
				MSG_WriteShort (msg, oldnum | U_REMOVE|U_MOREBITS);
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
				MSG_WriteShort (msg, oldnum | U_REMOVE);

			oldindex++;
			continue;
		}
	}

	if (newindex > to->max_entities)
		Con_Printf("Exceeded max entities\n");

	MSG_WriteShort (msg, 0);	// end of packetentities
}
#ifdef NQPROT

// reset all entity fields (typically used if status changed)
#define E5_FULLUPDATE (1<<0)
// E5_ORIGIN32=0: short[3] = s->origin[0] * 8, s->origin[1] * 8, s->origin[2] * 8
// E5_ORIGIN32=1: float[3] = s->origin[0], s->origin[1], s->origin[2]
#define E5_ORIGIN (1<<1)
// E5_ANGLES16=0: byte[3] = s->angle[0] * 256 / 360, s->angle[1] * 256 / 360, s->angle[2] * 256 / 360
// E5_ANGLES16=1: short[3] = s->angle[0] * 65536 / 360, s->angle[1] * 65536 / 360, s->angle[2] * 65536 / 360
#define E5_ANGLES (1<<2)
// E5_MODEL16=0: byte = s->modelindex
// E5_MODEL16=1: short = s->modelindex
#define E5_MODEL (1<<3)
// E5_FRAME16=0: byte = s->frame
// E5_FRAME16=1: short = s->frame
#define E5_FRAME (1<<4)
// byte = s->skin
#define E5_SKIN (1<<5)
// E5_EFFECTS16=0 && E5_EFFECTS32=0: byte = s->effects
// E5_EFFECTS16=1 && E5_EFFECTS32=0: short = s->effects
// E5_EFFECTS16=0 && E5_EFFECTS32=1: int = s->effects
// E5_EFFECTS16=1 && E5_EFFECTS32=1: int = s->effects
#define E5_EFFECTS (1<<6)
// bits >= (1<<8)
#define E5_EXTEND1 (1<<7)

// byte = s->renderflags
#define E5_FLAGS (1<<8)
// byte = bound(0, s->alpha * 255, 255)
#define E5_ALPHA (1<<9)
// byte = bound(0, s->scale * 16, 255)
#define E5_SCALE (1<<10)
// flag
#define E5_ORIGIN32 (1<<11)
// flag
#define E5_ANGLES16 (1<<12)
// flag
#define E5_MODEL16 (1<<13)
// byte = s->colormap
#define E5_COLORMAP (1<<14)
// bits >= (1<<16)
#define E5_EXTEND2 (1<<15)

// short = s->tagentity
// byte = s->tagindex
#define E5_ATTACHMENT (1<<16)
// short[4] = s->light[0], s->light[1], s->light[2], s->light[3]
// byte = s->lightstyle
// byte = s->lightpflags
#define E5_LIGHT (1<<17)
// byte = s->glowsize
// byte = s->glowcolor
#define E5_GLOW (1<<18)
// short = s->effects
#define E5_EFFECTS16 (1<<19)
// int = s->effects
#define E5_EFFECTS32 (1<<20)
// flag
#define E5_FRAME16 (1<<21)
// unused
#define E5_COLORMOD (1<<22)
// bits >= (1<<24)
#define E5_EXTEND3 (1<<23)

// unused
#define E5_UNUSED24 (1<<24)
// unused
#define E5_UNUSED25 (1<<25)
// unused
#define E5_UNUSED26 (1<<26)
// unused
#define E5_UNUSED27 (1<<27)
// unused
#define E5_UNUSED28 (1<<28)
// unused
#define E5_UNUSED29 (1<<29)
// unused
#define E5_UNUSED30 (1<<30)
// bits2 > 0
#define E5_EXTEND4 (1<<31)

void SVDP_EmitEntityDelta(entity_state_t *from, entity_state_t *to, sizebuf_t *msg, qboolean isnew)
{
	int bits;
	if (!isnew && !memcmp(from, to, sizeof(entity_state_t)))
	{
		return;	//didn't change
	}

	bits = 0;
	if (isnew)
	{
		bits |= E5_FULLUPDATE;
	}

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

	if ((bits & E5_ORIGIN) && (/*!(to->flags & RENDER_LOWPRECISION) ||*/ to->origin[0] < -4096 || to->origin[0] >= 4096 || to->origin[1] < -4096 || to->origin[1] >= 4096 || to->origin[2] < -4096 || to->origin[2] >= 4096))
		bits |= E5_ORIGIN32;
	if ((bits & E5_ANGLES)/* && !(to->flags & RENDER_LOWPRECISION)*/)
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
		MSG_WriteByte(msg, to->colormap);
	if (bits & E5_ATTACHMENT)
	{
		MSG_WriteShort(msg, to->tagentity);
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
}

entity_state_t defaultstate;
void SVDP_EmitEntitiesUpdate (client_t *client, packet_entities_t *to, sizebuf_t *msg)
{
	edict_t	*ent;
	client_frame_t	*fromframe;
	packet_entities_t *from;
	int		oldindex, newindex;
	int		oldnum, newnum;
	int		oldmax;

	client->netchan.incoming_sequence++;

	// this is the frame that we are going to delta update from
	fromframe = &client->frameunion.frames[(client->netchan.incoming_sequence-2) & UPDATE_MASK];
	from = &fromframe->entities;
	oldmax = from->num_entities;

//	Con_Printf ("frame %i\n", client->netchan.incoming_sequence);

	MSG_WriteByte(msg, svcdp_entities);
	MSG_WriteLong(msg, 0);
	if (client->protocol == SCP_DARKPLACES7)
		MSG_WriteLong(msg, client->last_sequence);

	for (newindex = 0; newindex < to->num_entities; newindex++)
		to->entities[newindex].bitmask = 0;
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
			SVDP_EmitEntityDelta (&from->entities[oldindex], &to->entities[newindex], msg, false);
			oldindex++;
			newindex++;
			continue;
		}

		if (newnum < oldnum)
		{	// this is a new entity, send it from the baseline... as far as dp understands it...
			ent = EDICT_NUM(svprogfuncs, newnum);
//Con_Printf ("baseline %i\n", newnum);
			SVDP_EmitEntityDelta (&defaultstate, &to->entities[newindex], msg, true);
			newindex++;
			continue;
		}

		if (newnum > oldnum)
		{	// the old entity isn't present in the new message
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
	vec3_t size;
	int diff;
	int best;
	int hullnum, i;

	if (sv.world.worldmodel->fromgame != fg_quake)
	{
		VectorSubtract (maxs, mins, size);
		return size[2];	//clients are expected to decide themselves.
	}

	if (h2hull)
		return h2hull-1 | (mins[2]?0:128);


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

		if (!ent->isself || ent->fteext & PEXT_SPLITSCREEN)
		{
#ifdef PEXT_SCALE	//this is graphics, not physics
			if (ent->fteext & PEXT_SCALE)
			{
				if (ent->scale && ent->scale != 1) pflags |= PF_SCALE_Z;
			}
#endif
#ifdef PEXT_TRANS
			if (ent->fteext & PEXT_TRANS)
			{
				if (ent->transparency) pflags |= PF_TRANS_Z;
			}
#endif
#ifdef PEXT_FATNESS
			if (ent->fteext & PEXT_FATNESS)
			{
				if (ent->fatness) pflags |= PF_FATNESS_Z;
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
					pm_type = SV_PMTypeForClient (ent->cl);
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
				default:
					Sys_Error("SV_WritePlayersToClient: unexpected pm_type");
					pm_code=0;
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
				cmd.angles[0] = (int)(ent->angles[0] * 65535/360.0f);
				cmd.angles[1] = (int)(ent->angles[1] * 65535/360.0f);
				cmd.angles[2] = (int)(ent->angles[2] * 65535/360.0f);
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
					MSG_WriteShort (msg, ent->velocity[i]);
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
		if (pflags & PF_SCALE_Z)
			MSG_WriteByte (msg, ent->scale*50);
#endif
#ifdef PEXT_TRANS
		if (pflags & PF_TRANS_Z)
			MSG_WriteByte (msg, (qbyte)(ent->transparency*255));
#endif
#ifdef PEXT_FATNESS
		if (pflags & PF_FATNESS_Z)
			MSG_WriteChar (msg, ent->fatness*10);
#endif
#ifdef PEXT_HULLSIZE	//shrunken or crouching in halflife levels. (possibly enlarged)
		if (pflags & PF_HULLSIZE_Z)
			MSG_WriteChar (msg, hullnumber + (ent->onladder?128:0));	//physics.
#endif
}
#endif


qboolean Cull_Traceline(edict_t *viewer, edict_t *seen)
{
	int i;
	trace_t tr;
	vec3_t start;
	vec3_t end;

	if (seen->v->solid == SOLID_BSP)
		return false;	//bsp ents are never culled this way

	//stage 1: check against their origin
	VectorAdd(viewer->v->origin, viewer->v->view_ofs, start);
	tr.fraction = 1;
	if (!sv.world.worldmodel->funcs.Trace (sv.world.worldmodel, 1, 0, NULL, start, seen->v->origin, vec3_origin, vec3_origin, &tr))
		return false;	//wasn't blocked

	//stage 2: check against their bbox
	for (i = 0; i < 8; i++)
	{
		end[0] = seen->v->origin[0] + ((i&1)?seen->v->mins[0]:seen->v->maxs[0]);
		end[1] = seen->v->origin[1] + ((i&2)?seen->v->mins[1]:seen->v->maxs[1]);
		end[2] = seen->v->origin[2] + ((i&4)?seen->v->mins[2]+0.1:seen->v->maxs[2]);

		tr.fraction = 1;
		if (!sv.world.worldmodel->funcs.Trace (sv.world.worldmodel, 1, 0, NULL, start, end, vec3_origin, vec3_origin, &tr))
			return false;	//this trace went through, so don't cull
	}

	return true;
}


/*
=============
SV_WritePlayersToClient

=============
*/
void SV_WritePlayersToClient (client_t *client, client_frame_t *frame, edict_t *clent, qbyte *pvs, sizebuf_t *msg)
{
	qboolean isbot;
	int			j;
	client_t	*cl;
	edict_t		*ent, *vent;
	int			pflags;

	demo_frame_t *demo_frame;
	demo_client_t *dcl;
#define DF_DEAD		(1<<8)
#define DF_GIB		(1<<9)

	if (clent == NULL)	//write to demo file. (no PVS)
	{
		demo_frame = &demo.frames[demo.parsecount&DEMO_FRAMES_MASK];
		for (j=0,cl=svs.clients, dcl = demo_frame->clients; j<MAX_CLIENTS ; j++,cl++, dcl++)
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
			if (cl->viewent && ent == clent)
				vent = EDICT_NUM(svprogfuncs, cl->viewent);
			else
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
			continue;
		}
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

		for (i=0 ; i<MAX_CLIENTS ; i++)
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
		clst.playernum = MAX_CLIENTS-1;
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
	for (j=0,cl=svs.clients ; j<sv.allocated_client_slots ; j++,cl++)
	{
		if (cl->state != cs_spawned && !(cl->state == cs_free && cl->name[0]))	//this includes bots, and nq bots
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
			if (!sv.world.worldmodel->funcs.EdictInFatPVS(sv.world.worldmodel, &((wedict_t*)ent)->pvsinfo, pvs))
				continue;

			if (!((int)clent->xv->dimension_see & ((int)ent->xv->dimension_seen | (int)ent->xv->dimension_ghost)))
				continue;	//not in this dimension - sorry...
			if (sv_cullplayers_trace.value || sv_cullentities_trace.value)
				if (Cull_Traceline(clent, ent))
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
					if (client->spec_track)
					{
						clst.spectator = 2;
						clst.mins = svs.clients[client->spec_track-1].edict->v->mins;
						clst.maxs = svs.clients[client->spec_track-1].edict->v->maxs;
						clst.health = svs.clients[client->spec_track-1].edict->v->health;
						clst.weaponframe = svs.clients[client->spec_track-1].edict->v->weaponframe;
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
		pflags = 0;
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
			ClientReliable_FinishWrite(client);
		}
	}
}


void SVNQ_EmitEntityState(sizebuf_t *msg, entity_state_t *ent)
{
	entity_state_t *baseline = &EDICT_NUM(svprogfuncs, ent->number)->baseline;

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

	if (baseline->colormap != ent->colormap && ent->colormap>=0)
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


	if (0)
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
#ifdef PARANOID
	MSG_WriteByte (msg,(bits | NQU_SIGNAL) & 0xFF); //gets caught on 'range error'
#else
	MSG_WriteByte (msg,bits | NQU_SIGNAL);
#endif

	if (bits & NQU_MOREBITS)	MSG_WriteByte (msg, bits>>8);
	if (bits & DPU_EXTEND1)		MSG_WriteByte (msg, bits>>16);
	if (bits & DPU_EXTEND2)		MSG_WriteByte (msg, bits>>24);

	if (bits & NQU_LONGENTITY)
		MSG_WriteShort (msg,ent->number);
	else
		MSG_WriteByte (msg,ent->number);

	if (bits & NQU_MODEL)		MSG_WriteByte (msg,	ent->modelindex);
	if (bits & NQU_FRAME)		MSG_WriteByte (msg, ent->frame);
	if (bits & NQU_COLORMAP)	MSG_WriteByte (msg, ent->colormap);
	if (bits & NQU_SKIN)		MSG_WriteByte (msg, ent->skinnum);
	if (bits & NQU_EFFECTS)		MSG_WriteByte (msg, eff & 0x00ff);
	if (bits & NQU_ORIGIN1)		MSG_WriteCoord (msg, ent->origin[0]);
	if (bits & NQU_ANGLE1)		MSG_WriteAngle(msg, ent->angles[0]);
	if (bits & NQU_ORIGIN2)		MSG_WriteCoord (msg, ent->origin[1]);
	if (bits & NQU_ANGLE2)		MSG_WriteAngle(msg, ent->angles[1]);
	if (bits & NQU_ORIGIN3)		MSG_WriteCoord (msg, ent->origin[2]);
	if (bits & NQU_ANGLE3)		MSG_WriteAngle(msg, ent->angles[2]);

	if (bits & DPU_ALPHA)		MSG_WriteByte(msg, ent->trans*255);
	if (bits & DPU_SCALE)		MSG_WriteByte(msg, ent->scale*16);
	if (bits & DPU_EFFECTS2)	MSG_WriteByte(msg, eff >> 8);
	if (bits & DPU_GLOWSIZE)	MSG_WriteByte(msg, glowsize);
	if (bits & DPU_GLOWCOLOR)	MSG_WriteByte(msg, glowcolor);
	if (bits & DPU_COLORMOD)	MSG_WriteByte(msg, colourmod);
	if (bits & DPU_FRAME2)		MSG_WriteByte(msg, (int)ent->frame >> 8);
	if (bits & DPU_MODEL2)		MSG_WriteByte(msg, (int)ent->modelindex >> 8);
}

typedef struct gibfilter_s {
	struct gibfilter_s *next;
	int modelindex;
	int minframe;
	int maxframe;
} gibfilter_t;
gibfilter_t *gibfilter;
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
	gibfilter_t *gf;
	while(gibfilter)
	{
		gf = gibfilter;
		gibfilter = gibfilter->next;

		Z_Free(gf);
	}

	if (svs.gametype != GT_PROGS && svs.gametype != GT_Q1QVM)
		return;

	file = COM_LoadStackFile("gibfiltr.cfg", buffer, sizeof(buffer));
	if (!file)
	{
		Con_DPrintf("gibfiltr.cfg file was not found. Using defaults\n");
		SV_GibFilterAdd("progs/gib1.mdl", -1, -1, false);
		SV_GibFilterAdd("progs/gib2.mdl", -1, -1, false);
		SV_GibFilterAdd("progs/gib3.mdl", -1, -1, false);
		SV_GibFilterAdd("progs/h_player.mdl", -1, -1, false);
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
{//fixme: this doesn't add
	int		leafnum;
	leafnum = CM_PointLeafnum (mod, org);
	clientarea = CM_LeafArea (mod, leafnum);

	return SV_Q2BSP_FatPVS (mod, org, buffer, buffersize, add);
}

qboolean Q2BSP_EdictInFatPVS(model_t *mod, pvscache_t *ent, qbyte *pvs)
{
	int i,l;
	if (!CM_AreasConnected (mod, clientarea, ent->areanum))
	{	// doors can legally straddle two areas, so
		// we may need to check another one
		if (!ent->areanum2
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

			if (e >= 1 && e <= MAX_CLIENTS)
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

void SV_Snapshot_BuildStateQ1(entity_state_t *state, edict_t *ent, client_t *client)
{
//builds an entity_state from an entity
//note that client can be null, for building baselines.

	int i;

#ifdef Q2SERVER
	state->modelindex2 = 0;
	state->modelindex3 = 0;
	state->modelindex4 = 0;
	state->event = 0;
	state->solid = 0;
	state->sound = 0;
	state->renderfx = 0;
	state->old_origin[0] = 0;
	state->old_origin[1] = 0;
	state->old_origin[2] = 0;
#endif

	state->dpflags = 0;
	if (ent->xv->viewmodelforclient)
	{	//this ent would have been filtered out by now if its not ours
		//if ent->viewmodelforclient == client then:
		state->dpflags |= RENDER_VIEWMODEL;
	}
	if (ent->xv->exteriormodeltoclient && client)
	{
		if (ent->xv->exteriormodeltoclient == EDICT_TO_PROG(svprogfuncs, client->edict))
			state->dpflags |= RENDER_EXTERIORMODEL;
		//everyone else sees it normally.
	}

	if (ent->v->movetype == MOVETYPE_STEP)
		state->dpflags |= RENDER_STEP;

	state->number = NUM_FOR_EDICT(svprogfuncs, ent);
	state->flags = 0;
	VectorCopy (ent->v->origin, state->origin);
	VectorCopy (ent->v->angles, state->angles);
	state->modelindex = ent->v->modelindex;
	state->frame = ent->v->frame;
	state->colormap = ent->v->colormap;
	state->skinnum = ent->v->skin;
	state->effects = ent->v->effects;
	state->hexen2flags = ent->xv->drawflags;
	state->abslight = (int)(ent->xv->abslight*255) & 255;
	state->tagentity = ent->xv->tag_entity;
	state->tagindex = ent->xv->tag_index;

	state->light[0] = ent->xv->color[0]*255;
	state->light[1] = ent->xv->color[1]*255;
	state->light[2] = ent->xv->color[2]*255;
	state->light[3] = ent->xv->light_lev;
	state->lightstyle = ent->xv->style;
	state->lightstyle = ent->xv->style;
	state->lightpflags = ent->xv->pflags;

	if ((int)ent->v->flags & FL_CLASS_DEPENDENT && client->playerclass)	//hexen2 wierdness.
	{
		char modname[MAX_QPATH];
		Q_strncpyz(modname, sv.strings.model_precache[state->modelindex], sizeof(modname));
		if (strlen(modname)>5)
		{
			modname[strlen(modname)-5] = client->playerclass+'0';
			state->modelindex = SV_ModelIndex(modname);
		}
	}

	if (state->effects & 0x00400000)	//DP's EF_LOWPRECISION
		state->effects &= ~0x00400000;	//we don't support it, nor does dp any more. strip it.

	if (state->effects & EF_FULLBRIGHT)	//wrap the field for fte clients (this is horrible)
	{
		state->hexen2flags |= MLS_FULLBRIGHT;
	}

	if (progstype != PROG_QW && state->effects && client && ISQWCLIENT(client))	//don't send extra nq effects to a qw client.
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
		else
		{
			if (state->effects & NQEF_NODRAW)
				state->modelindex = 0;
		}

		if (state->number <= sv.allocated_client_slots) // clear only client ents
			state->effects &= ~ (QWEF_FLAG1|QWEF_FLAG2);
	}

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
	state->glowsize = ent->xv->glow_size*0.25;
	state->glowcolour = ent->xv->glow_color;
	if (ent->xv->glow_trail)
		state->dpflags |= RENDER_GLOWTRAIL;


#ifdef PEXT_SCALE
	state->scale = ent->xv->scale*16;
	if (!ent->xv->scale)
		state->scale = 1*16;
#endif
#ifdef PEXT_TRANS
	state->trans = ent->xv->alpha*255;
	if (!ent->xv->alpha)
		state->trans = 255;

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
	state->fatness = ent->xv->fatness*16;
#endif
}

void SV_Snapshot_BuildQ1(client_t *client, packet_entities_t *pack, qbyte *pvs, edict_t *clent, qboolean ignorepvs)
{
//pvs and clent can be null, but only if the other is also null
	int e, i;
	edict_t *ent;
	entity_state_t	*state;
#define DEPTHOPTIMISE
#ifdef DEPTHOPTIMISE
	vec3_t org;
	float distances[MAX_EXTENDED_PACKET_ENTITIES];
	float dist;
#endif
	globalvars_t *pr_globals = PR_globals(svprogfuncs, PR_CURRENT);
	int pvsflags;

	if (client->viewent
 #ifdef NQPROT
  && ISQWCLIENT(client)
  #endif
		)	//this entity is watching from outside themselves. The client is tricked into thinking that they themselves are in the view ent, and a new dummy ent (the old them) must be spawned.

	{

//FIXME: this hack needs cleaning up
#ifdef DEPTHOPTIMISE
		distances[0] = 0;
#endif
		state = &pack->entities[pack->num_entities];
		pack->num_entities++;

		SV_Snapshot_BuildStateQ1(state, clent, client);

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



#ifdef NQPROT
	for (e=(ISQWCLIENT(client)?sv.allocated_client_slots+1:1) ; e<sv.world.num_edicts ; e++)
#else
	for (e=sv.allocated_client_slots+1 ; e<sv.num_edicts ; e++)
#endif
	{
		ent = EDICT_NUM(svprogfuncs, e);

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

		if (ent->xv->viewmodelforclient)
		{
			if (ent->xv->viewmodelforclient != (clent?EDICT_TO_PROG(svprogfuncs, clent):0))
				continue;
			pvsflags = PVSF_IGNOREPVS;
		}
		else if (ent == clent)
		{
			pvsflags = PVSF_IGNOREPVS;
		}
		else
		{
			// ignore ents without visible models
			if (!ent->xv->SendEntity && (!ent->v->modelindex || !*PR_GetString(svprogfuncs, ent->v->model)) && !((int)ent->xv->pflags & PFLAGS_FULLDYNAMIC))
				continue;

			pvsflags = ent->xv->pvsflags;
			if (pvs && ent != clent)	//self doesn't get a pvs test, to cover teleporters
			{
				if ((int)ent->v->effects & EF_NODEPTHTEST)
				{
				}
				else if ((pvsflags & PVSF_MODE_MASK) < PVSF_USEPHS)
				{
					//branch out to the pvs testing.
					if (ent->xv->tag_entity)
					{
						edict_t *p = ent;
						int c = 10;
						while(p->xv->tag_entity&&c-->0)
						{
							p = EDICT_NUM(svprogfuncs, p->xv->tag_entity);
						}
						if (!sv.world.worldmodel->funcs.EdictInFatPVS(sv.world.worldmodel, &((wedict_t*)p)->pvsinfo, pvs))
							continue;
					}
					else
					{
						if (!sv.world.worldmodel->funcs.EdictInFatPVS(sv.world.worldmodel, &((wedict_t*)ent)->pvsinfo, pvs))
							continue;
					}
				}
				else if ((pvsflags & PVSF_MODE_MASK) == PVSF_USEPHS && sv.world.worldmodel->fromgame == fg_quake)
				{
					int leafnum;
					unsigned char *mask;
					if (sv.phs)
					{
						leafnum = sv.world.worldmodel->funcs.LeafnumForPoint(sv.world.worldmodel, host_client->edict->v->origin);
						mask = sv.phs + leafnum * 4*((sv.world.worldmodel->numleafs+31)>>5);

						leafnum = sv.world.worldmodel->funcs.LeafnumForPoint (sv.world.worldmodel, ent->v->origin)-1;
						if ( !(mask[leafnum>>3] & (1<<(leafnum&7)) ) )
						{
							continue;
						}
					}
				}

				if (client->gibfilter && SV_GibFilter(ent))
					continue;
			}
		}

		//DP_SV_NODRAWONLYTOCLIENT
		if (ent->xv->nodrawtoclient)	//DP extension.
			if (ent->xv->nodrawtoclient == EDICT_TO_PROG(svprogfuncs, client->edict))
				continue;
		//DP_SV_DRAWONLYTOCLIENT
		if (ent->xv->drawonlytoclient)
			if (ent->xv->drawonlytoclient != EDICT_TO_PROG(svprogfuncs, client->edict))
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


		if (!ignorepvs && ent != clent && (pvsflags & PVSF_MODE_MASK)==PVSF_NORMALPVS && !((unsigned int)ent->v->effects & (EF_DIMLIGHT|EF_BLUE|EF_RED|EF_BRIGHTLIGHT|EF_BRIGHTFIELD|EF_NODEPTHTEST)))
		{	//more expensive culling
			if ((e <= sv.allocated_client_slots && sv_cullplayers_trace.value) || sv_cullentities_trace.value)
				if (Cull_Traceline(clent, ent))
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
		if (!ISDPCLIENT(client))
		{
			if (e >= client->max_net_ents)
				continue;
#ifdef PEXT_MODELDBL
			if (ent->v->modelindex >= 256 && !(client->fteprotocolextensions & PEXT_MODELDBL))
				continue;
#endif
		}

#ifdef DEPTHOPTIMISE
		if (clent)
		{
			//find distance based upon absolute mins/maxs so bsps are treated fairly.
			//org = clentorg + -0.5*(max+min)
			VectorAdd(ent->v->absmin, ent->v->absmax, org);
			VectorMA(clent->v->origin, -0.5, org, org);
			dist = DotProduct(org, org);	//Length

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
		SV_Snapshot_BuildStateQ1(state, ent, client);
	}
}

qbyte *SV_Snapshot_SetupPVS(client_t *client, qbyte *pvs, unsigned int pvsbufsize)
{
	vec3_t org;
	int leavepvs = false;

	for (; client; client = client->controlled)
	{
		if (client->viewent)
		{
			edict_t *e = PROG_TO_EDICT(svprogfuncs, client->viewent);
			VectorAdd (e->v->origin, client->edict->v->view_ofs, org);
		}
		else
			VectorAdd (client->edict->v->origin, client->edict->v->view_ofs, org);
		sv.world.worldmodel->funcs.FatPVS(sv.world.worldmodel, org, pvs, pvsbufsize, leavepvs);
		leavepvs = true;

#ifdef PEXT_VIEW2
		if (client->edict->xv->view2)	//add a second view point to the pvs
			sv.world.worldmodel->funcs.FatPVS(sv.world.worldmodel, PROG_TO_EDICT(svprogfuncs, client->edict->xv->view2)->v->origin, pvs, pvsbufsize, leavepvs);
#endif
	}

	return pvs;
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
	qbyte pvsbuf[(MAX_MAP_LEAFS+7)>>3];
	qbyte *pvs;
	SV_Snapshot_Clear(pack);
	pvs = SV_Snapshot_SetupPVS(client, pvsbuf, sizeof(pvsbuf));
	SV_Snapshot_BuildQ1(client, pack, pvs, client->edict, false);
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
	qbyte	*pvs;
	packet_entities_t	*pack;
	edict_t	*clent;
	client_frame_t	*frame;
	qbyte pvsbuffer[(MAX_MAP_LEAFS+7)/8];


	// this is the frame we are creating
	frame = &client->frameunion.frames[client->netchan.incoming_sequence & UPDATE_MASK];
	if (!sv.paused)
		memset(frame->playerpresent, 0, sizeof(frame->playerpresent));

	// find the client's PVS
	if (ignorepvs)
	{
		clent = NULL;
		pvs = NULL;
	}
	else
	{
		clent = client->edict;
#ifdef HLSERVER
		if (svs.gametype == GT_HALFLIFE)
			pvs = SVHL_Snapshot_SetupPVS(client, pvsbuffer, sizeof(pvsbuffer));
		else
#endif
			pvs = SV_Snapshot_SetupPVS(client, pvsbuffer, sizeof(pvsbuffer));
	}

	host_client = client;
	pack = &frame->entities;
	SV_Snapshot_Clear(pack);

	// send over the players in the PVS
	if (svs.gametype != GT_HALFLIFE)
		SV_WritePlayersToClient (client, frame, clent, pvs, msg);

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
			SVHL_Snapshot_Build(client, pack, pvs, clent, ignorepvs);
		else
#endif
			SV_Snapshot_BuildQ1(client, pack, pvs, clent, ignorepvs);
	}

#ifdef NQPROT
	if (ISNQCLIENT(client))
	{
		if (client->protocol == SCP_DARKPLACES6 || client->protocol == SCP_DARKPLACES7)
		{
			SVDP_EmitEntitiesUpdate(client, pack, msg);
			SV_EmitCSQCUpdate(client, msg);
			return;
		}
		else
		{
			for (e = 0; e < pack->num_entities; e++)
			{
				if (msg->cursize + 32 > msg->maxsize)
					break;
				SVNQ_EmitEntityState(msg, &pack->entities[e]);
			}
			client->netchan.incoming_sequence++;
			return;
		}
	}
#endif

	// encode the packet entities as a delta from the
	// last packetentities acknowledged by the client

	SV_EmitPacketEntities (client, pack, msg);

	SV_EmitCSQCUpdate(client, msg);

	// now add the specialized nail update
	SV_EmitNailUpdate (msg, ignorepvs);
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
			MSG_WriteShort(&sv.multicast, e);
			VectorCopy(ent->v->origin, org);
			if (progstype == PROG_H2)
				org[2] += 24;
			SV_Multicast(org, MULTICAST_PVS);
		}
	}
	needcleanup=0;
}
#endif

