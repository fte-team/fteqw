#include "quakedef.h"

#ifndef CLIENTONLY

#define Q2EDICT_NUM(i) (q2edict_t*)((char *)ge->edicts+i*ge->edict_size)

#ifndef Q2SERVER
void SV_WriteFrameToClient (client_t *client, sizebuf_t *msg)
{
}
void SV_BuildClientFrame (client_t *client)
{
}
#else

q2entity_state_t *svs_client_entities;//[Q2UPDATE_BACKUP*MAX_PACKET_ENTITIES];
int svs_num_client_entities;
int svs_next_client_entities;

q2entity_state_t	sv_baselines[Q2MAX_EDICTS];

/*
=============================================================================

Encode a client frame onto the network channel

=============================================================================
*/

/*
==================
MSG_WriteDeltaEntity

Writes part of a packetentities message.
Can delta from either a baseline or a previous packet_entity
==================
*/
void MSGQ2_WriteDeltaEntity (q2entity_state_t *from, q2entity_state_t *to, sizebuf_t *msg, qboolean force, qboolean newentity)
{
	int		bits;

	if (!to->number)
		Sys_Error ("Unset entity number");
	if (to->number >= Q2MAX_EDICTS)
		Sys_Error ("Entity number >= MAX_EDICTS");

// send an update
	bits = 0;

	if (to->number >= 256)
		bits |= Q2U_NUMBER16;		// number8 is implicit otherwise

	if (to->origin[0] != from->origin[0])
		bits |= Q2U_ORIGIN1;
	if (to->origin[1] != from->origin[1])
		bits |= Q2U_ORIGIN2;
	if (to->origin[2] != from->origin[2])
		bits |= Q2U_ORIGIN3;

	if ( to->angles[0] != from->angles[0] )
		bits |= Q2U_ANGLE1;		
	if ( to->angles[1] != from->angles[1] )
		bits |= Q2U_ANGLE2;
	if ( to->angles[2] != from->angles[2] )
		bits |= Q2U_ANGLE3;
		
	if ( to->skinnum != from->skinnum )
	{
		if ((unsigned)to->skinnum < 256)
			bits |= Q2U_SKIN8;
		else if ((unsigned)to->skinnum < 0x10000)
			bits |= Q2U_SKIN16;
		else
			bits |= (Q2U_SKIN8|Q2U_SKIN16);
	}
		
	if ( to->frame != from->frame )
	{
		if (to->frame < 256)
			bits |= Q2U_FRAME8;
		else
			bits |= Q2U_FRAME16;
	}

	if ( to->effects != from->effects )
	{
		if (to->effects < 256)
			bits |= Q2U_EFFECTS8;
		else if (to->effects < 0x8000)
			bits |= Q2U_EFFECTS16;
		else
			bits |= Q2U_EFFECTS8|Q2U_EFFECTS16;
	}
	
	if ( to->renderfx != from->renderfx )
	{
		if (to->renderfx < 256)
			bits |= Q2U_RENDERFX8;
		else if (to->renderfx < 0x8000)
			bits |= Q2U_RENDERFX16;
		else
			bits |= Q2U_RENDERFX8|Q2U_RENDERFX16;
	}
	
	if ( to->solid != from->solid )
		bits |= Q2U_SOLID;

	// event is not delta compressed, just 0 compressed
	if ( to->event  )
		bits |= Q2U_EVENT;
	
	if ( to->modelindex != from->modelindex )
	{
		bits |= Q2U_MODEL;
		if (to->modelindex > 0xff)
			bits |= Q2UX_INDEX16;
	}
	if ( to->modelindex2 != from->modelindex2 )
	{
		bits |= Q2U_MODEL2;
		if (to->modelindex2 > 0xff)
			bits |= Q2UX_INDEX16;
	}
	if ( to->modelindex3 != from->modelindex3 )
	{
		bits |= Q2U_MODEL3;
		if (to->modelindex3 > 0xff)
			bits |= Q2UX_INDEX16;
	}
	if ( to->modelindex4 != from->modelindex4 )
	{
		bits |= Q2U_MODEL4;
		if (to->modelindex4 > 0xff)
			bits |= Q2UX_INDEX16;
	}

	if ( to->sound != from->sound )
	{
		bits |= Q2U_SOUND;
		if (to->sound > 0xff)
			bits |= Q2UX_INDEX16;
	}

	if (newentity || (to->renderfx & Q2RF_BEAM))
		bits |= Q2U_OLDORIGIN;

	//
	// write the message
	//
	if (!bits && !force)
		return;		// nothing to send!

	//----------

	if (bits & 0xff000000)
		bits |= Q2U_MOREBITS3 | Q2U_MOREBITS2 | Q2U_MOREBITS1;
	else if (bits & 0x00ff0000)
		bits |= Q2U_MOREBITS2 | Q2U_MOREBITS1;
	else if (bits & 0x0000ff00)
		bits |= Q2U_MOREBITS1;

	MSG_WriteByte (msg,	bits&255 );

	if (bits & 0xff000000)
	{
		MSG_WriteByte (msg,	(bits>>8)&255 );
		MSG_WriteByte (msg,	(bits>>16)&255 );
		MSG_WriteByte (msg,	(bits>>24)&255 );
	}
	else if (bits & 0x00ff0000)
	{
		MSG_WriteByte (msg,	(bits>>8)&255 );
		MSG_WriteByte (msg,	(bits>>16)&255 );
	}
	else if (bits & 0x0000ff00)
	{
		MSG_WriteByte (msg,	(bits>>8)&255 );
	}

	//----------

	if (bits & Q2U_NUMBER16)
		MSG_WriteShort (msg, to->number);
	else
		MSG_WriteByte (msg,	to->number);

	if (bits & Q2U_MODEL)
	{
		if (bits & Q2UX_INDEX16)
			MSG_WriteShort	(msg,	to->modelindex);
		else
			MSG_WriteByte	(msg,	to->modelindex);
	}
	if (bits & Q2U_MODEL2)
	{
		if (bits & Q2UX_INDEX16)
			MSG_WriteShort	(msg,	to->modelindex2);
		else
			MSG_WriteByte	(msg,	to->modelindex2);
	}
	if (bits & Q2U_MODEL3)
	{
		if (bits & Q2UX_INDEX16)
			MSG_WriteShort	(msg,	to->modelindex3);
		else
			MSG_WriteByte	(msg,	to->modelindex3);
	}
	if (bits & Q2U_MODEL4)
	{
		if (bits & Q2UX_INDEX16)
			MSG_WriteShort	(msg,	to->modelindex4);
		else
			MSG_WriteByte	(msg,	to->modelindex4);
	}

	if (bits & Q2U_FRAME8)
		MSG_WriteByte (msg, to->frame);
	if (bits & Q2U_FRAME16)
		MSG_WriteShort (msg, to->frame);

	if ((bits & Q2U_SKIN8) && (bits & Q2U_SKIN16))		//used for laser colors
		MSG_WriteLong (msg, to->skinnum);
	else if (bits & Q2U_SKIN8)
		MSG_WriteByte (msg, to->skinnum);
	else if (bits & Q2U_SKIN16)
		MSG_WriteShort (msg, to->skinnum);


	if ( (bits & (Q2U_EFFECTS8|Q2U_EFFECTS16)) == (Q2U_EFFECTS8|Q2U_EFFECTS16) )
		MSG_WriteLong (msg, to->effects);
	else if (bits & Q2U_EFFECTS8)
		MSG_WriteByte (msg, to->effects);
	else if (bits & Q2U_EFFECTS16)
		MSG_WriteShort (msg, to->effects);

	if ( (bits & (Q2U_RENDERFX8|Q2U_RENDERFX16)) == (Q2U_RENDERFX8|Q2U_RENDERFX16) )
		MSG_WriteLong (msg, to->renderfx);
	else if (bits & Q2U_RENDERFX8)
		MSG_WriteByte (msg, to->renderfx);
	else if (bits & Q2U_RENDERFX16)
		MSG_WriteShort (msg, to->renderfx);

	if (bits & Q2U_ORIGIN1)
		MSG_WriteCoord (msg, to->origin[0]);		
	if (bits & Q2U_ORIGIN2)
		MSG_WriteCoord (msg, to->origin[1]);
	if (bits & Q2U_ORIGIN3)
		MSG_WriteCoord (msg, to->origin[2]);

	if (bits & Q2U_ANGLE1)
		MSG_WriteAngle(msg, to->angles[0]);
	if (bits & Q2U_ANGLE2)
		MSG_WriteAngle(msg, to->angles[1]);
	if (bits & Q2U_ANGLE3)
		MSG_WriteAngle(msg, to->angles[2]);

	if (bits & Q2U_OLDORIGIN)
	{
		MSG_WriteCoord (msg, to->old_origin[0]);
		MSG_WriteCoord (msg, to->old_origin[1]);
		MSG_WriteCoord (msg, to->old_origin[2]);
	}

	if (bits & Q2U_SOUND)
	{
		if (bits & Q2UX_INDEX16)
			MSG_WriteShort	(msg,	to->sound);
		else
			MSG_WriteByte	(msg,	to->sound);
	}

	if (bits & Q2U_EVENT)
		MSG_WriteByte (msg, to->event);
	if (bits & Q2U_SOLID)
	{
		if (msg->prim.flags & NPQ2_SOLID32)
			MSG_WriteLong(msg, to->solid);
		else
			MSG_WriteSize16(msg, to->solid);
	}
}


/*
=============
SV_EmitPacketEntities

Writes a delta update of an entity_state_t list to the message.
=============
*/
void SVQ2_EmitPacketEntities (q2client_frame_t *from, q2client_frame_t *to, sizebuf_t *msg)
{
	q2entity_state_t	*oldent, *newent;
	int		oldindex, newindex;
	int		oldnum, newnum;
	int		from_num_entities;
	int		bits;

	MSG_WriteByte (msg, svcq2_packetentities);

	if (!from)
		from_num_entities = 0;
	else
		from_num_entities = from->num_entities;

	newindex = 0;
	oldindex = 0;
	while (newindex < to->num_entities || oldindex < from_num_entities)
	{
		if (newindex >= to->num_entities)
		{
			newent = NULL;	//shh compiler, shh...
			newnum = 9999;
		}
		else
		{
			newent = &svs_client_entities[(to->first_entity+newindex)%svs_num_client_entities];
			newnum = newent->number;
		}

		if (oldindex >= from_num_entities)
		{
			oldent = NULL;	//shh compiler, shh...
			oldnum = 9999;
		}
		else
		{
			oldent = &svs_client_entities[(from->first_entity+oldindex)%svs_num_client_entities];
			oldnum = oldent->number;
		}

		if (newnum == oldnum)
		{	// delta update from old position
			// because the force parm is false, this will not result
			// in any bytes being emited if the entity has not changed at all
			// note that players are always 'newentities', this updates their oldorigin always
			// and prevents warping
			if (msg->cursize+128 > msg->maxsize)
				memcpy(newent, oldent, sizeof(*newent));	//too much data, so set the ent up as the same as the old, so it's sent next frame
			else
				MSGQ2_WriteDeltaEntity (oldent, newent, msg, false, newent->number <= svs.allocated_client_slots);
			oldindex++;
			newindex++;
			continue;
		}

		if (newnum < oldnum)
		{	// this is a new entity, send it from the baseline

			if (msg->cursize+128 > msg->maxsize)
			{	//might cause the packet to overflow
				//so strip out this ent, we can add it next frame if it's still relevent
				to->num_entities--;
				memmove(newent, newent+1, sizeof(*newent) * (to->num_entities-newindex));
			}
			else
			{
				MSGQ2_WriteDeltaEntity (&sv_baselines[newnum], newent, msg, true, true);
				newindex++;
			}
			continue;
		}

		if (newnum > oldnum)
		{	// the old entity isn't present in the new message
			bits = Q2U_REMOVE;
			if (oldnum >= 256)
				bits |= Q2U_NUMBER16 | Q2U_MOREBITS1;

			MSG_WriteByte (msg,	bits&255 );
			if (bits & 0x0000ff00)
				MSG_WriteByte (msg,	(bits>>8)&255 );

			if (bits & Q2U_NUMBER16)
				MSG_WriteShort (msg, oldnum);
			else
				MSG_WriteByte (msg, oldnum);

			oldindex++;
			continue;
		}
	}

	MSG_WriteShort (msg, 0);	// end of packetentities

#if 0
	if (numprojs)
		SV_EmitProjectileUpdate(msg);
#endif
}



/*
=============
SV_WritePlayerstateToClient

=============
*/
void SVQ2_WritePlayerstateToClient (unsigned int pext, int seat, int extflags, q2client_frame_t *from, q2client_frame_t *to, sizebuf_t *msg)
{
	int				i;
	int				pflags;
	q2player_state_t	*ps, *ops;
	q2player_state_t	dummy;
	int				statbits;

	ps = &to->ps[seat];
	if (!from)
	{
		memset (&dummy, 0, sizeof(dummy));
		ops = &dummy;
	}
	else
		ops = &from->ps[seat];

	//
	// determine what needs to be sent
	//
	pflags = 0;

	if (pext & PEXT_SPLITSCREEN)
		if (!from || from->clientnum[seat] != to->clientnum[seat])
			pflags |= Q2PS_CLIENTNUM;

	if (ps->pmove.pm_type != ops->pmove.pm_type)
		pflags |= Q2PS_M_TYPE;

	if (ps->pmove.origin[0] != ops->pmove.origin[0]
		|| ps->pmove.origin[1] != ops->pmove.origin[1]
		|| ps->pmove.origin[2] != ops->pmove.origin[2] )
		pflags |= Q2PS_M_ORIGIN;

	if (ps->pmove.velocity[0] != ops->pmove.velocity[0]
		|| ps->pmove.velocity[1] != ops->pmove.velocity[1]
		|| ps->pmove.velocity[2] != ops->pmove.velocity[2] )
		pflags |= Q2PS_M_VELOCITY;

	if (ps->pmove.pm_time != ops->pmove.pm_time)
		pflags |= Q2PS_M_TIME;

	if (ps->pmove.pm_flags != ops->pmove.pm_flags)
		pflags |= Q2PS_M_FLAGS;

	if (ps->pmove.gravity != ops->pmove.gravity)
		pflags |= Q2PS_M_GRAVITY;

	if (ps->pmove.delta_angles[0] != ops->pmove.delta_angles[0]
		|| ps->pmove.delta_angles[1] != ops->pmove.delta_angles[1]
		|| ps->pmove.delta_angles[2] != ops->pmove.delta_angles[2] )
		pflags |= Q2PS_M_DELTA_ANGLES;


	if (ps->viewoffset[0] != ops->viewoffset[0]
		|| ps->viewoffset[1] != ops->viewoffset[1]
		|| ps->viewoffset[2] != ops->viewoffset[2] )
		pflags |= Q2PS_VIEWOFFSET;

	if (ps->viewangles[0] != ops->viewangles[0]
		|| ps->viewangles[1] != ops->viewangles[1]
		|| ps->viewangles[2] != ops->viewangles[2] )
		pflags |= Q2PS_VIEWANGLES;

	if (ps->kick_angles[0] != ops->kick_angles[0]
		|| ps->kick_angles[1] != ops->kick_angles[1]
		|| ps->kick_angles[2] != ops->kick_angles[2] )
		pflags |= Q2PS_KICKANGLES;

	if (ps->blend[0] != ops->blend[0]
		|| ps->blend[1] != ops->blend[1]
		|| ps->blend[2] != ops->blend[2]
		|| ps->blend[3] != ops->blend[3] )
		pflags |= Q2PS_BLEND;

	if (ps->fov != ops->fov)
		pflags |= Q2PS_FOV;

	if (ps->rdflags != ops->rdflags)
		pflags |= Q2PS_RDFLAGS;

	if (ps->gunframe != ops->gunframe)
		pflags |= Q2PS_WEAPONFRAME;

	if (ps->gunindex != ops->gunindex)
		pflags |= Q2PS_WEAPONINDEX;

	if (pext & PEXT_MODELDBL)
	{
		if ((pflags & Q2PS_WEAPONINDEX) && ps->gunindex > 0xff)
			pflags |= Q2PS_INDEX16;
		if ((pflags & Q2PS_WEAPONFRAME) && ps->gunframe > 0xff)
			pflags |= Q2PS_INDEX16;
	}

	if (pflags > 0xffff)
		pflags |= Q2PS_EXTRABITS;

	//
	// write it
	//
	MSG_WriteByte (msg, svcq2_playerinfo);
	MSG_WriteShort (msg, pflags&0xffff);
	if (pflags & Q2PS_EXTRABITS)
		MSG_WriteByte (msg, pflags>>16);

	//
	// write the pmove_state_t
	//
	if (pflags & Q2PS_M_TYPE)
		MSG_WriteByte (msg, ps->pmove.pm_type);

	if (pflags & Q2PS_M_ORIGIN)
	{
		MSG_WriteShort (msg, ps->pmove.origin[0]);
		MSG_WriteShort (msg, ps->pmove.origin[1]);
		MSG_WriteShort (msg, ps->pmove.origin[2]);
	}

	if (pflags & Q2PS_M_VELOCITY)
	{
		MSG_WriteShort (msg, ps->pmove.velocity[0]);
		MSG_WriteShort (msg, ps->pmove.velocity[1]);
		MSG_WriteShort (msg, ps->pmove.velocity[2]);
	}

	if (pflags & Q2PS_M_TIME)
		MSG_WriteByte (msg, ps->pmove.pm_time);

	if (pflags & Q2PS_M_FLAGS)
		MSG_WriteByte (msg, ps->pmove.pm_flags);

	if (pflags & Q2PS_M_GRAVITY)
		MSG_WriteShort (msg, ps->pmove.gravity);

	if (pflags & Q2PS_M_DELTA_ANGLES)
	{
		MSG_WriteShort (msg, ps->pmove.delta_angles[0]);
		MSG_WriteShort (msg, ps->pmove.delta_angles[1]);
		MSG_WriteShort (msg, ps->pmove.delta_angles[2]);
	}

	//
	// write the rest of the player_state_t
	//
	if (pflags & Q2PS_VIEWOFFSET)
	{
		MSG_WriteChar (msg, ps->viewoffset[0]*4);
		MSG_WriteChar (msg, ps->viewoffset[1]*4);
		MSG_WriteChar (msg, ps->viewoffset[2]*4);
	}


	if (pflags & Q2PS_VIEWANGLES)
	{
		MSG_WriteAngle16 (msg, ps->viewangles[0]);
		MSG_WriteAngle16 (msg, ps->viewangles[1]);
		MSG_WriteAngle16 (msg, ps->viewangles[2]);
	}

	if (pflags & Q2PS_KICKANGLES)
	{
		MSG_WriteChar (msg, ps->kick_angles[0]*4);
		MSG_WriteChar (msg, ps->kick_angles[1]*4);
		MSG_WriteChar (msg, ps->kick_angles[2]*4);
	}

	if (pflags & Q2PS_WEAPONINDEX)
	{
		if (pflags & Q2PS_INDEX16)
			MSG_WriteShort(msg, ps->gunindex);
		else
			MSG_WriteByte (msg, ps->gunindex);
	}

	if (pflags & Q2PS_WEAPONFRAME)
	{
		if (pflags & Q2PS_INDEX16)
			MSG_WriteShort (msg, ps->gunframe);
		else
			MSG_WriteByte (msg, ps->gunframe);
		MSG_WriteChar (msg, ps->gunoffset[0]*4);
		MSG_WriteChar (msg, ps->gunoffset[1]*4);
		MSG_WriteChar (msg, ps->gunoffset[2]*4);
		MSG_WriteChar (msg, ps->gunangles[0]*4);
		MSG_WriteChar (msg, ps->gunangles[1]*4);
		MSG_WriteChar (msg, ps->gunangles[2]*4);
	}

	if (pflags & Q2PS_BLEND)
	{
		MSG_WriteByte (msg, ps->blend[0]*255);
		MSG_WriteByte (msg, ps->blend[1]*255);
		MSG_WriteByte (msg, ps->blend[2]*255);
		MSG_WriteByte (msg, ps->blend[3]*255);
	}

	if (pflags & Q2PS_FOV)
		MSG_WriteByte (msg, ps->fov);
	if (pflags & Q2PS_RDFLAGS)
		MSG_WriteByte (msg, ps->rdflags);

	// send stats
	statbits = 0;
	for (i=0 ; i<Q2MAX_STATS ; i++)
		if (ps->stats[i] != ops->stats[i])
			statbits |= 1<<i;
	MSG_WriteLong (msg, statbits);
	for (i=0 ; i<Q2MAX_STATS ; i++)
		if (statbits & (1<<i) )
			MSG_WriteShort (msg, ps->stats[i]);

	if ((extflags & Q2PSX_CLIENTNUM) || (pflags & Q2PS_CLIENTNUM))
		MSG_WriteByte(msg, to->clientnum[seat]);
}


/*
==================
SV_WriteFrameToClient
==================
*/
void SVQ2_WriteFrameToClient (client_t *client, sizebuf_t *msg)
{
	q2client_frame_t		*frame, *oldframe;
	int					lastframe;
	client_t			*split;
	int seat;
	int extflags = 0;

//Com_Printf ("%i -> %i\n", client->lastframe, sv.framenum);
	// this is the frame we are creating
	frame = &client->frameunion.q2frames[sv.framenum & Q2UPDATE_MASK];

	if (client->delta_sequence <= 0)
	{	// client is asking for a retransmit
		oldframe = NULL;
		lastframe = -1;
	}
	else if (sv.framenum - client->delta_sequence >= (Q2UPDATE_BACKUP - 3) )
	{	// client hasn't gotten a good message through in a long time
//		Com_Printf ("%s: Delta request from out-of-date packet.\n", client->name);
		oldframe = NULL;
		lastframe = -1;
	}
	else
	{	// we have a valid message to delta from
		oldframe = &client->frameunion.q2frames[client->delta_sequence & Q2UPDATE_MASK];
		lastframe = client->delta_sequence;
	}

	MSG_WriteByte (msg, svcq2_frame);
	MSG_WriteLong (msg, sv.framenum);
	MSG_WriteLong (msg, lastframe);	// what we are delta'ing from
	MSG_WriteByte (msg, client->chokecount&0xff);	// rate dropped packets
	extflags |= Q2PSX_OLD;
	client->chokecount = 0;

	// send over the areabits
	MSG_WriteByte (msg, frame->areabytes);
	SZ_Write (msg, frame->areabits, frame->areabytes);

	// delta encode the playerstate
	for (split = client, seat = 0; split; split = split->controlled, seat++)
		SVQ2_WritePlayerstateToClient (client->fteprotocolextensions, seat, extflags, oldframe, frame, msg);

	// delta encode the entities
	SVQ2_EmitPacketEntities (oldframe, frame, msg);
}


/*
=============================================================================

Build a client frame structure

=============================================================================
*/

/*
=============
SV_BuildClientFrame

Decides which entities are going to be visible to the client, and
copies off the playerstat and areabits.
=============
*/
void SVQ2_Ents_Init(void);
void SVQ2_BuildClientFrame (client_t *client)
{
	int		e, i;
	vec3_t org[MAX_SPLITS];
	int		clientarea[MAX_SPLITS];
	q2edict_t	*clent[MAX_SPLITS];
	client_t	*split;
	q2edict_t	*ent;
	q2client_frame_t	*frame;
	q2entity_state_t	*state;
	int		l;
	int		seat;
	int		c_fullsend;
	pvsbuffer_t	clientpvs;
	qbyte	*clientphs = NULL;
	int		seats;

	if (client->state < cs_spawned)
		return;

	SVQ2_Ents_Init();

	clientpvs.buffer = alloca(clientpvs.buffersize=sv.world.worldmodel->pvsbytes);

#if 0
	numprojs = 0; // no projectiles yet
#endif

	// this is the frame the client will be acking (EVIL HACKS!)
	frame = &client->frameunion.q2frames[client->netchan.outgoing_sequence & Q2UPDATE_MASK];

	frame->senttime = realtime*1000; // save it for ping calc later

	// this is the frame we are creating
	frame = &client->frameunion.q2frames[sv.framenum & Q2UPDATE_MASK];

	// grab the current player_state_t
	for (seat = 0, split = client; split; split = split->controlled, seat++)
	{
		int		clientcluster;
		int		leafnum;

		clent[seat] = split->q2edict;
		frame->clientnum[seat] = split - svs.clients;

		if (!split->q2edict->client)
		{	//shouldn't happen
			VectorClear(org[seat]);
			clientarea[seat] = 0;
			memset(&frame->ps[seat], 0, sizeof(frame->ps[seat]));
			frame->ps[seat].pmove.pm_type = Q2PM_FREEZE;
			continue;
		}

		// find the client's PVS
		for (i=0 ; i<3 ; i++)
			org[seat][i] = clent[seat]->client->ps.pmove.origin[i]*0.125 + clent[seat]->client->ps.viewoffset[i];

		leafnum = CM_PointLeafnum (sv.world.worldmodel, org[seat]);
		clientarea[seat] = CM_LeafArea (sv.world.worldmodel, leafnum);
		clientcluster = CM_LeafCluster (sv.world.worldmodel, leafnum);

		// calculate the visible areas
		frame->areabytes = CM_WriteAreaBits (sv.world.worldmodel, frame->areabits, clientarea[seat], seat != 0);

		sv.world.worldmodel->funcs.FatPVS(sv.world.worldmodel, org[seat], &clientpvs, seat!=0);
		if (seat==0)	//FIXME
			clientphs = CM_ClusterPHS (sv.world.worldmodel, clientcluster, NULL);

		frame->ps[seat] = clent[seat]->client->ps;
		if (sv.paused)
			frame->ps[seat].pmove.pm_type = Q2PM_FREEZE;
	}
	seats = seat;
	for (; seat < MAX_SPLITS; seat++)
	{
		memset(&frame->ps[seat], 0, sizeof(frame->ps[seat]));
		frame->clientnum[seat] = 0xff;	//invalid
	}

	// build up the list of visible entities
	frame->num_entities = 0;
	frame->first_entity = svs_next_client_entities;

	c_fullsend = 0;

	for (e=1 ; e<ge->num_edicts ; e++)
	{
		ent = Q2EDICT_NUM(e);

		// ignore ents without visible models
		if (ent->svflags & SVF_NOCLIENT)
			continue;

		// ignore ents without visible models unless they have an effect
		if (!ent->s.modelindex && !ent->s.effects && !ent->s.sound
			&& !ent->s.event)
			continue;

		for (seat = 0; seat < seats; seat++)
		{
			// ignore if not touching a PV leaf
			if (ent != clent[seat])
			{
				// check area
				if (!CM_AreasConnected (sv.world.worldmodel, clientarea[seat], ent->areanum))
				{	// doors can legally straddle two areas, so
					// we may need to check another one
					if (!ent->areanum2
						|| !CM_AreasConnected (sv.world.worldmodel, clientarea[seat], ent->areanum2))
						continue;		// blocked by a door
				}

				// beams just check one point for PHS
				if (ent->s.renderfx & Q2RF_BEAM)
				{
					l = ent->clusternums[0];
					if ( !(clientphs[l >> 3] & (1 << (l&7) )) )
						continue;
				}
				else
				{
					// FIXME: if an ent has a model and a sound, but isn't
					// in the PVS, only the PHS, clear the model

					if (ent->num_clusters == -1)
					{	// too many leafs for individual check, go by headnode
						if (!CM_HeadnodeVisible (sv.world.worldmodel, ent->headnode, clientpvs.buffer))
							continue;
						c_fullsend++;
					}
					else
					{	// check individual leafs
						for (i=0 ; i < ent->num_clusters ; i++)
						{
							l = ent->clusternums[i];
							if (clientpvs.buffer[l >> 3] & (1 << (l&7) ))
								break;
						}
						if (i == ent->num_clusters)
							continue;		// not visible
					}

					if (!ent->s.modelindex)
					{	// don't send sounds if they will be attenuated away
						vec3_t	delta;
						float	len;

						VectorSubtract (org[seat], ent->s.origin, delta);
						len = Length (delta);
						if (len > 400)
							continue;
					}
				}
			}
			break;
		}
		if (seat == seats)
			continue;	//not visible to any seat
		seat = 0; //FIXME

		// add it to the circular client_entities array
		state = &svs_client_entities[svs_next_client_entities%svs_num_client_entities];
		if (ent->s.number != e)
		{
			Con_DPrintf ("FIXING ENT->S.NUMBER!!!\n");
			ent->s.number = e;
		}
		*state = ent->s;

		// don't mark players missiles as solid
		if (ent->owner == clent[seat])
			state->solid = 0;

		svs_next_client_entities++;
		frame->num_entities++;
	}
}

void SVQ2_BuildBaselines(void)
{
	unsigned int e;
	q2edict_t	*ent;
	q2entity_state_t	*base;

	if (!ge)
		return;

	for (e=1 ; e<ge->num_edicts ; e++)
	{
		ent = Q2EDICT_NUM(e);
		base = &ent->s;

		if (base->modelindex || base->sound || base->effects)
			sv_baselines[e] = *base;
	}
}

void SVQ2_Ents_Init(void)
{
	extern cvar_t	maxclients;
	if (!svs_client_entities)
	{
		svs_num_client_entities = maxclients.value*Q2UPDATE_BACKUP*64;
		svs_client_entities = Z_Malloc (sizeof(entity_state_t)*svs_num_client_entities);
	}
}
void SVQ2_Ents_Shutdown(void)
{
	if (svs_client_entities)
	{
		Z_Free(svs_client_entities);
		svs_client_entities = NULL;
		svs_num_client_entities = 0;
	}
}
#endif

#endif
