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

void SV_CleanupEnts(void);

extern qboolean pr_udc_exteffect_enabled;

extern cvar_t sv_nailhack;

/*
=============================================================================

The PVS must include a small area around the client to allow head bobbing
or other small motion on the client side.  Otherwise, a bob might cause an
entity that should be visible to not show up, especially when the bob
crosses a waterline.

=============================================================================
*/

int needcleanup;

int		fatbytes;
qbyte	fatpvs[(MAX_MAP_LEAFS+1)/4];



#ifdef Q2BSPS
void SV_Q2BSP_FatPVS (vec3_t org)
{
	int		leafs[64];
	int		i, j, count;
	int		longs;
	qbyte	*src;
	vec3_t	mins, maxs;

	for (i=0 ; i<3 ; i++)
	{
		mins[i] = org[i] - 8;
		maxs[i] = org[i] + 8;
	}

	count = CM_BoxLeafnums (mins, maxs, leafs, 64, NULL);
	if (count < 1)
		Sys_Error ("SV_Q2FatPVS: count < 1");

	if (sv.worldmodel->fromgame == fg_quake3)
		longs = CM_ClusterSize();
	else
		longs = (CM_NumClusters()+31)>>5;

	// convert leafs to clusters
	for (i=0 ; i<count ; i++)
		leafs[i] = CM_LeafCluster(leafs[i]);

	CM_ClusterPVS(leafs[0], fatpvs);


//	memcpy (fatpvs, CM_ClusterPVS(leafs[0]), longs<<2);
	// or in all the other leaf bits
	for (i=1 ; i<count ; i++)
	{
		for (j=0 ; j<i ; j++)
			if (leafs[i] == leafs[j])
				break;
		if (j != i)
			continue;		// already have the cluster we want
		src = CM_ClusterPVS(leafs[i], NULL);
		for (j=0 ; j<longs ; j++)
			((long *)fatpvs)[j] |= ((long *)src)[j];
	}
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

#ifdef PEXT_LIGHTUPDATES
edict_t	*light[MAX_NAILS];
int		numlight;
extern	int sv_lightningmodel;
#endif

qboolean SV_AddNailUpdate (edict_t *ent)
{
	if (ent->v.modelindex != sv_nailmodel
		&& ent->v.modelindex != sv_supernailmodel)
		return false;
	if (sv_nailhack.value)
		return false;

	demonails = true;

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

#ifdef PEXT_LIGHTUPDATES
qboolean SV_AddLightUpdate (edict_t *ent)
{
	if (ent->v.modelindex != sv_lightningmodel)
		return false;
	if (numlight == MAX_NAILS)
		return true;
	light[numnails] = ent;
	numlight++;
	return true;
}
#endif

void SV_EmitNailUpdate (sizebuf_t *msg, qboolean recorder)
{
	qbyte	bits[6];	// [48 bits] xyzpy 12 12 12 4 8 
	int		n, i;
	edict_t	*ent;
	int		x, y, z, p, yaw;

#ifdef PEXT_LIGHTUPDATES
	if (numlight)
	{
		MSG_WriteByte (msg, svc_lightnings);
		MSG_WriteByte (msg, numlight);

		for (n=0 ; n<numlight ; n++)
		{
			ent = light[n];
			x = (int)(ent->v.origin[0]+4096)>>1;
			y = (int)(ent->v.origin[1]+4096)>>1;
			z = (int)(ent->v.origin[2]+4096)>>1;
			p = (int)(16*ent->v.angles[0]/360)&15;
			yaw = (int)(256*ent->v.angles[1]/360)&255;

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
#endif
	if (!numnails)
		return;

	if (recorder)
		MSG_WriteByte (msg, svc_nails2);
	else
		MSG_WriteByte (msg, svc_nails);

	MSG_WriteByte (msg, numnails);

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
	for (n=0 ; n<numnails ; n++)
	{
		ent = nails[n];
		if (recorder) {
			if (!ent->v.colormap) {
				if (!((++nailcount)&255)) nailcount++;
				ent->v.colormap = nailcount&255;
			}

			MSG_WriteByte (msg, (qbyte)ent->v.colormap);
		}
		x = (int)(ent->v.origin[0]+4096)>>1;
		y = (int)(ent->v.origin[1]+4096)>>1;
		z = (int)(ent->v.origin[2]+4096)>>1;
		p = (int)(16*ent->v.angles[0]/360)&15;
		yaw = (int)(256*ent->v.angles[1]/360)&255;

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
	float	miss;

// send an update
	bits = 0;

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

	for (i=0 ; i<3 ; i++)
	{
		miss = to->origin[i] - from->origin[i];
		if ( miss < -0.1 || miss > 0.1 )
		{
			bits |= U_ORIGIN1<<i;
#ifdef PEXT_ORIGINDBL
			if (protext & PEXT_ORIGINDBL && to->origin[i] >= 4096 || to->origin[i] <= -4096)
				bits |= PF_ORIGINDBL;
#endif
		}
	}

	if ( to->angles[0] != from->angles[0] )
		bits |= U_ANGLE1;

	if ( to->angles[1] != from->angles[1] )
		bits |= U_ANGLE2;

	if ( to->angles[2] != from->angles[2] )
		bits |= U_ANGLE3;

	if ( to->colormap != from->colormap )
		bits |= U_COLORMAP;

	if ( to->skinnum != from->skinnum )
		bits |= U_SKIN;

	if ( to->frame != from->frame )
		bits |= U_FRAME;

	if ( (to->effects&255) != (from->effects&255) )
		bits |= U_EFFECTS;

	if ( to->modelindex != from->modelindex )
	{
		bits |= U_MODEL;
		if (to->modelindex > 255)
			evenmorebits |= U_MODELDBL;
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

	if ( to->drawflags != from->drawflags && protext & PEXT_HEXEN2)
		evenmorebits |= U_DRAWFLAGS;
	if ( to->abslight != from->abslight && protext & PEXT_HEXEN2)
		evenmorebits |= U_ABSLIGHT;

	if (evenmorebits&0xff00)
		evenmorebits |= U_YETMORE;
	if (evenmorebits&0x00ff)
		bits |= U_EVENMORE;
	if (bits & 511)
		bits |= U_MOREBITS;
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

	if (!bits && !force)
		return;		// nothing to send!
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
		MSG_WriteByte (msg, to->effects);
	if (bits & U_ORIGIN1)
		MSG_WriteCoord (msg, to->origin[0]);
	if (bits & U_ANGLE1)
		MSG_WriteAngle(msg, to->angles[0]);
	if (bits & U_ORIGIN2)
		MSG_WriteCoord (msg, to->origin[1]);
	if (bits & U_ANGLE2)
		MSG_WriteAngle(msg, to->angles[1]);
	if (bits & U_ORIGIN3)
		MSG_WriteCoord (msg, to->origin[2]);
	if (bits & U_ANGLE3)
		MSG_WriteAngle(msg, to->angles[2]);

#ifdef U_SCALE
	if (evenmorebits & U_SCALE)
		MSG_WriteByte (msg, (qbyte)(to->scale*100.0));
#endif
#ifdef U_TRANS
	if (evenmorebits & U_TRANS)
		MSG_WriteByte (msg, (qbyte)(to->trans*255));
#endif
#ifdef U_FATNESS
	if (evenmorebits & U_FATNESS)
		MSG_WriteChar (msg, to->fatness*2);
#endif

	if (evenmorebits & U_DRAWFLAGS)
		MSG_WriteByte (msg, to->drawflags);
	if (evenmorebits & U_ABSLIGHT)
		MSG_WriteByte (msg, to->abslight);
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
		fromframe = &client->frames[client->delta_sequence & UPDATE_MASK];
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
			ent = EDICT_NUM(svprogfuncs, newnum);
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
			MSG_WriteShort (msg, oldnum | U_REMOVE);
			oldindex++;
			continue;
		}
	}

	MSG_WriteShort (msg, 0);	// end of packetentities
}


int SV_HullNumForPlayer(int h2hull, float *mins, float *maxs)
{
	vec3_t size;
	int diff;
	int best;
	int hullnum, i;

	if (sv.worldmodel->fromgame == fg_quake2 || sv.worldmodel->fromgame == fg_quake3)
	{
		VectorSubtract (maxs, mins, size);
		return size[2];	//clients are expected to decide themselves.
	}

	if (h2hull)
		return h2hull-1;


	hullnum = 0;
	best = 8192;
	//x/y pos/neg are assumed to be the same magnitute.
	//y pos/height are assumed to be different from all the others.
	for (i = 0; i < MAX_MAP_HULLSM; i++)
	{
#define sq(x) ((x)*(x))
		diff = sq(sv.worldmodel->hulls[i].clip_maxs[2] - maxs[2]) +
			sq(sv.worldmodel->hulls[i].clip_mins[2] - mins[2]) +
			sq(sv.worldmodel->hulls[i].clip_maxs[0] - maxs[0]) +
			sq(sv.worldmodel->hulls[i].clip_mins[0] - mins[0]);
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
	int modelindex2;
	int frame;
	int weaponframe;
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

		if (ent->spectator == 2 && ent->weaponframe)
			pflags |= PF_WEAPONFRAME;

		if (!ent->isself || ent->fteext & PEXT_SPLITSCREEN)
		{
#ifdef PEXT_SCALE	//this is graphics, not physics
			if (ent->fteext & PEXT_SCALE)
			{
				if (ent->scale) pflags |= (zext&Z_EXT_PM_TYPE)?PF_SCALE_Z:PF_SCALE_NOZ;
			}
#endif
#ifdef PEXT_TRANS
			if (ent->fteext & PEXT_TRANS)
			{
				if (ent->transparency) pflags |= (zext&Z_EXT_PM_TYPE)?PF_TRANS_Z:PF_TRANS_NOZ;
			}
#endif
#ifdef PEXT_FATNESS
			if (ent->fteext & PEXT_FATNESS)
			{
				if (ent->fatness) pflags |= (zext&Z_EXT_PM_TYPE)?PF_FATNESS_Z:PF_FATNESS_NOZ;
			}
#endif
		}
#ifdef PEXT_HULLSIZE
		if (ent->fteext & PEXT_HULLSIZE)
		{
			hullnumber = SV_HullNumForPlayer(ent->hull, ent->mins, ent->maxs);
			if (hullnumber != 1)
				pflags |= (zext&Z_EXT_PM_TYPE)?PF_HULLSIZE_Z:PF_HULLSIZE_NOZ;
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
				pm_code = (ent->zext & Z_EXT_PM_TYPE_NEW)?PM_SPECTATOR:PMC_OLD_SPECTATOR;//(ent->spectator && ent->isself) ? PMC_OLD_SPECTATOR : PMC_NORMAL;
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
			MSG_WriteCoord (msg, ent->origin[i]+(sv.demostatevalid?1:0));

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
				cmd.angles[0] = ent->angles[0] * 65535/360.0f;
				cmd.angles[1] = ent->angles[1] * 65535/360.0f;
				cmd.angles[2] = ent->angles[2] * 65535/360.0f;
			}

			if (ent->health <= 0)
			{	// don't show the corpse looking around...
				cmd.angles[0] = 0;
				cmd.angles[1] = ent->angles[1]*65535/360;
				cmd.angles[0] = 0;
			}

			cmd.buttons = 0;	// never send buttons
			cmd.impulse = 0;	// never send impulses

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

		if (zext&Z_EXT_PM_TYPE)
		{
#ifdef PEXT_SCALE
			if (pflags & PF_SCALE_Z)
				MSG_WriteByte (msg, ent->scale*100);
#endif
#ifdef PEXT_TRANS
			if (pflags & PF_TRANS_Z)
				MSG_WriteByte (msg, (qbyte)(ent->transparency*255));
#endif
#ifdef PEXT_FATNESS
			if (pflags & PF_FATNESS_Z)
				MSG_WriteChar (msg, ent->fatness*2);
#endif
#ifdef PEXT_HULLSIZE	//shrunken or crouching in halflife levels. (possibly enlarged)
			if (pflags & PF_HULLSIZE_Z)
				MSG_WriteChar (msg, hullnumber + (ent->onladder?128:0));	//physics.
#endif
		}
		else
		{
#ifdef PEXT_SCALE
			if (pflags & PF_SCALE_NOZ)
				MSG_WriteByte (msg, ent->scale*100);
#endif
#ifdef PEXT_TRANS
			if (pflags & PF_TRANS_NOZ)
				MSG_WriteByte (msg, (qbyte)(ent->transparency*255));
#endif
#ifdef PEXT_FATNESS
			if (pflags & PF_FATNESS_NOZ)
				MSG_WriteChar (msg, ent->fatness*2);
#endif
#ifdef PEXT_HULLSIZE	//shrunken or crouching in halflife levels. (possibly enlarged)
			if (pflags & PF_HULLSIZE_NOZ)
				MSG_WriteChar (msg, hullnumber + (ent->onladder?128:0));	//physics.
#endif
		}
}
#endif


#define EFNQ_DARKLIGHT	16
#define EFNQ_DARKFIELD	32
#define EFNQ_LIGHT		64

#define EFQW_DARKLIGHT	256
#define EFQW_DARKFIELD	512
#define EFQW_LIGHT		1024

void SV_RemoveEffect(client_t *to, edict_t *ent, int seefmask)
{
	specialenteffects_t *prev = NULL;
	specialenteffects_t *ef;
	int en = NUM_FOR_EDICT(svprogfuncs, ent);
	for (ef = to->enteffects; ef; ef = ef->next)
	{
		if (ef->entnum == en && ef->efnum & seefmask)
		{
			if (prev)
				prev->next = ef->next;
			else
				to->enteffects = ef->next;
			Z_Free(ef);

			if (ef->efnum & seefmask & 1>>SEEF_BRIGHTFIELD)
			{
				ClientReliableWrite_Begin(to, svc_temp_entity, 4);
				ClientReliableWrite_Byte(to, TE_SEEF_BRIGHTFIELD);
				ClientReliableWrite_Short(to, en|0x8000);
			}
			if (ef->efnum & seefmask & 1>>SEEF_DARKLIGHT)
			{
				ClientReliableWrite_Begin(to, svc_temp_entity, 4);
				ClientReliableWrite_Byte(to, SEEF_DARKLIGHT);
				ClientReliableWrite_Short(to, en|0x8000);
			}
			if (ef->efnum & seefmask & 1>>SEEF_DARKFIELD)
			{
				ClientReliableWrite_Begin(to, svc_temp_entity, 4);
				ClientReliableWrite_Byte(to, SEEF_DARKFIELD);
				ClientReliableWrite_Short(to, en|0x8000);
			}
			if (ef->efnum & seefmask & 1>>SEEF_LIGHT)
			{
				ClientReliableWrite_Begin(to, svc_temp_entity, 4);
				ClientReliableWrite_Byte(to, SEEF_LIGHT);
				ClientReliableWrite_Short(to, en|0x8000);
			}
			return;
		}
		prev = ef;
	}
}

void SV_AddEffect(client_t *to, edict_t *ent, int seefno)
{
	specialenteffects_t *prev = NULL;
	specialenteffects_t *ef;
	int en = NUM_FOR_EDICT(svprogfuncs, ent);

	for (ef = to->enteffects; ef; ef = ef->next)
	{
		if (ef->entnum == en && ef->efnum == 1<<seefno)
		{
			if (ef->colour != ent->v.seefcolour || ef->offset != ent->v.seefoffset || ef->size[0] != ent->v.seefsizex || ef->size[1] != ent->v.seefsizey || ef->size[2] != ent->v.seefsizez || ef->die < sv.time)
			{
				if (prev)
					prev->next = ef->next;
				else
					to->enteffects = ef->next;
				Z_Free(ef);
				ef = NULL;
				break;
			}
			return;	//still the same state.
		}
		prev = ef;
	}

	ef = Z_Malloc(sizeof(specialenteffects_t));
	ef->die = sv.time + 10;
	ef->next = to->enteffects;
	to->enteffects = ef;
	ef->efnum = 1<<seefno;
	ef->entnum = en;
	ef->colour = ent->v.seefcolour;
	if (!ef->colour)
		ef->colour = 111;
	ef->offset = ent->v.seefoffset;
	ef->size[0] = ent->v.seefsizex;
	if (!ef->size[0])
		ef->offset = 64;
	ef->size[1] = ent->v.seefsizey;
	if (!ef->size[1])
		ef->offset = 64;
	ef->size[2] = ent->v.seefsizez;
	if (!ef->size[2])
		ef->offset = 64;

	ClientReliableWrite_Begin(to, svc_temp_entity, 20);
	ClientReliableWrite_Byte(to, TE_SEEF_BRIGHTFIELD+seefno);
	ClientReliableWrite_Short(to, en);
	switch(seefno)
	{
	case SEEF_BRIGHTFIELD:
		ClientReliableWrite_Coord(to, ef->size[0]);
		ClientReliableWrite_Coord(to, ef->size[1]);
		ClientReliableWrite_Coord(to, ef->size[2]);
		ClientReliableWrite_Char (to, ef->offset);
		ClientReliableWrite_Byte (to, ef->colour);
		break;
	case SEEF_DARKFIELD:
		ClientReliableWrite_Byte (to, ef->colour);
		break;
	case SEEF_DARKLIGHT:
	case SEEF_LIGHT:
		ClientReliableWrite_Coord(to, ef->size[0]);
		ClientReliableWrite_Coord(to, ef->size[1]);
		break;
	}
}

void SV_SendExtraEntEffects(client_t *to, edict_t *ent)
{
	int removeeffects = 0;
	if (pr_udc_exteffect_enabled)
	{
		if (to->fteprotocolextensions & PEXT_SEEF1)
		{
			if (progstype != PROG_QW)
			{
				if ((int)ent->v.effects & (EF_BRIGHTFIELD|EFNQ_DARKLIGHT|EFNQ_DARKFIELD|EFNQ_LIGHT) || to->enteffects)
				{
					if ((int)ent->v.effects & EF_BRIGHTFIELD)
						SV_AddEffect(to, ent, SEEF_BRIGHTFIELD);
					else
						removeeffects |= 1<<SEEF_BRIGHTFIELD;

					if ((int)ent->v.effects & EFNQ_DARKLIGHT)
						SV_AddEffect(to, ent, SEEF_DARKLIGHT);
					else
						removeeffects |= 1<<SEEF_DARKLIGHT;

					if ((int)ent->v.effects & EFNQ_DARKFIELD)
						SV_AddEffect(to, ent, SEEF_DARKFIELD);
					else
						removeeffects |= 1<<SEEF_DARKFIELD;

					if ((int)ent->v.effects & EFNQ_LIGHT)
						SV_AddEffect(to, ent, SEEF_LIGHT);
					else
						removeeffects |= 1<<SEEF_LIGHT;
				}
			}
			else
			{
				if ((int)ent->v.effects & (EF_BRIGHTFIELD|EFQW_DARKLIGHT|EFQW_DARKFIELD|EFQW_LIGHT) || to->enteffects)
				{
					if ((int)ent->v.effects & EF_BRIGHTFIELD)
						SV_AddEffect(to, ent, SEEF_BRIGHTFIELD);
					else
						removeeffects |= 1<<SEEF_BRIGHTFIELD;

					if ((int)ent->v.effects & EFQW_DARKLIGHT)
						SV_AddEffect(to, ent, SEEF_DARKLIGHT);
					else
						removeeffects |= 1<<SEEF_DARKLIGHT;

					if ((int)ent->v.effects & EFQW_DARKFIELD)
						SV_AddEffect(to, ent, SEEF_DARKFIELD);
					else
						removeeffects |= 1<<SEEF_DARKFIELD;

					if ((int)ent->v.effects & EFQW_LIGHT)
						SV_AddEffect(to, ent, SEEF_LIGHT);
					else
						removeeffects |= 1<<SEEF_LIGHT;
				}
			}
			if (to->enteffects)
				SV_RemoveEffect(to, ent, removeeffects);
		}
	}
}
/*
=============
SV_WritePlayersToClient

=============
*/
void SV_WritePlayersToClient (client_t *client, edict_t *clent, qbyte *pvs, sizebuf_t *msg)
{
	qboolean isbot;
	int			i, j;
	client_t	*cl;
	edict_t		*ent, *vent;
	int			pflags;

	demo_frame_t *demo_frame;
	demo_client_t *dcl;
#define DF_DEAD		(1<<8)
#define DF_GIB		(1<<9)

	if (clent == NULL)	//write to demo file. (no pov)
	{
		demo_frame = &demo.frames[demo.parsecount&DEMO_FRAMES_MASK];
		for (j=0,cl=svs.clients, dcl = demo_frame->clients; j<MAX_CLIENTS ; j++,cl++, dcl++)
		{
			if (cl->state != cs_spawned)
				continue;

			if (sv.demostatevalid)
			{
				if (client != cl)
					continue;
			}

			ent = cl->edict;
			if (cl->viewent && ent == clent)
				vent = EDICT_NUM(svprogfuncs, cl->viewent);
			else
				vent = ent;

			if (progstype != PROG_QW)
			{
				if ((int)ent->v.effects & EF_MUZZLEFLASH)
				{
					if (needcleanup < (j+1))
					{
						needcleanup = (j+1);
						MSG_WriteByte(&sv.multicast, svc_muzzleflash);
						MSG_WriteShort(&sv.multicast, (j+1));
						SV_Multicast(ent->v.origin, MULTICAST_PVS);				
					}
				}
			}

			if (cl->spectator)
				continue;

			dcl->parsecount = demo.parsecount;

			VectorCopy(vent->v.origin, dcl->info.origin);
			VectorCopy(vent->v.angles, dcl->info.angles);
			dcl->info.angles[0] *= -3;
			dcl->info.angles[2] = 0; // no roll angle

			if (ent->v.health <= 0)
			{	// don't show the corpse looking around...
				dcl->info.angles[0] = 0;
				dcl->info.angles[1] = vent->v.angles[1];
				dcl->info.angles[2] = 0;
			}

			if (ent != vent)
			{
				dcl->info.model = 0;	//invisible.
				dcl->info.effects = 0;
			}
			else
			{
				dcl->info.skinnum = ent->v.skin;
				dcl->info.effects = ent->v.effects;
				dcl->info.weaponframe = ent->v.weaponframe;
				dcl->info.model = ent->v.modelindex;
			}
			dcl->sec = sv.time - cl->localtime;
			dcl->frame = ent->v.frame;
			dcl->flags = 0;
			dcl->cmdtime = cl->localtime;
			dcl->fixangle = demo.fixangle[j];
			demo.fixangle[j] = 0;

			if (ent->v.health <= 0)
				dcl->flags |= DF_DEAD;
			if (ent->v.mins[2] != -24)
				dcl->flags |= DF_GIB;
			continue;
		}
		return;
	}



#ifdef NQPROT
	if (client->nqprot)
		return;
#endif

	for (j=0,cl=svs.clients ; j<sv.allocated_client_slots ; j++,cl++)
	{
		isbot = !cl->state && cl->name[0];
		if (!sv.demostatevalid)
			if (cl->state != cs_spawned)	//this includes bots
				if (!isbot || progstype != PROG_NQ)	//unless they're NQ bots...
					continue;

		ent = cl->edict;
		if (cl->viewent && ent == clent)
		{
			vent = EDICT_NUM(svprogfuncs, cl->viewent);
			if (!vent)
				vent = ent;
		}
		else
			vent = ent;

		if (sv.demostatevalid)	//this is a demo
		{
				// ZOID visibility tracking
	/*		if (ent != clent &&
				!(client->spec_track && client->spec_track - 1 == j)) 
			{
				if (cl->spectator)
					continue;

				// ignore if not touching a PV leaf
				for (i=0 ; i < ent->num_leafs ; i++)
					if (pvs[ent->leafnums[i] >> 3] & (1 << (ent->leafnums[i]&7) ))
						break;
				if (i == ent->num_leafs)
				{
					continue;		// not visible
				}
			}
*/
			if (ent == clent)
				i = svs.clients[j].spec_track-1;
			else
				i = j;
			if (i>=0&&*sv.recordedplayer[i].userinfo)
			{
				usercmd_t cmd;
				vec3_t ang;
				vec3_t org;
				extern vec3_t player_mins, player_maxs;
				clstate_t clst;
				clst.playernum = i;
				clst.onladder = 0;
				clst.lastcmd = &cmd;
				clst.modelindex = sv.demostate[i+1].modelindex;
				clst.modelindex2 = 0;
				clst.frame = sv.demostate[i+1].frame;
				clst.weaponframe = sv.recordedplayer[i].weaponframe;
				clst.angles = ang;
				clst.origin = org;
				clst.hull = 1;
				clst.velocity = sv.recordedplayer[i].velocity;
				clst.effects = sv.demostate[i+1].effects;
				clst.skin = sv.demostate[i+1].skinnum;
				clst.mins = player_mins;
				clst.maxs = player_maxs;
				clst.scale = sv.demostate[i+1].scale;
				clst.transparency = sv.demostate[i+1].trans;
				clst.fatness = sv.demostate[i+1].fatness;
				clst.localtime = sv.time;//sv.recordedplayer[j].updatetime;
				clst.health = sv.recordedplayer[i].stats[STAT_HEALTH];
				clst.spectator = 0;
				clst.isself = false;
				clst.fteext = client->fteprotocolextensions;
				clst.zext = client->zquake_extensions;
				clst.cl = NULL;

				VectorMA(sv.demostate[i+1].angles, realtime - sv.recordedplayer[i].updatetime, sv.recordedplayer[i].avelocity, ang);
				VectorMA(sv.demostate[i+1].origin, realtime - sv.recordedplayer[i].updatetime, sv.recordedplayer[i].velocity, org);
				ang[0] *= -3;

//				ang[0] = ang[1] = ang[2] = 0;

				memset(&cmd, 0, sizeof(cmd));
				cmd.angles[0] = ang[0]*65535/360.0f;
				cmd.angles[1] = ang[1]*65535/360.0f;
				cmd.angles[2] = ang[2]*65535/360.0f;
				cmd.msec = 50;
				{vec3_t f, r, u, v;
				vec_t VectorNormalize2 (vec3_t, vec3_t);
				AngleVectors(ang, f, r, u);
				VectorCopy(sv.recordedplayer[i].velocity, v);
				cmd.forwardmove = DotProduct(f, v);
				cmd.sidemove = DotProduct(r, v);
				cmd.upmove = DotProduct(u, v);
				}
				clst.lastcmd=NULL;

				clst.spectator = 1;
				if (i == j)
					clst.spectator = 2;

				SV_WritePlayerToClient(msg, &clst);
			}
//			if (ent != clent)
				continue;
		}

		if (progstype != PROG_QW)
		{
			if (progstype == PROG_H2 && (int)ent->v.effects & EF_NODRAW && ent != clent)
				continue;

			if ((int)ent->v.effects & EF_MUZZLEFLASH)
			{
				if (needcleanup < (j+1))
				{
					needcleanup = (j+1);
					MSG_WriteByte(&sv.multicast, svc_muzzleflash);
					MSG_WriteShort(&sv.multicast, (j+1));
					SV_Multicast(ent->v.origin, MULTICAST_PVS);				
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
			for (i=0 ; i < ent->num_leafs ; i++)
				if (pvs[ent->leafnums[i] >> 3] & (1 << (ent->leafnums[i]&7) ))
					break;
			if (i == ent->num_leafs)
			{
				continue;		// not visible
			}

			if (!((int)clent->v.dimension_see & ((int)ent->v.dimension_seen | (int)ent->v.dimension_ghost)))
				continue;	//not in this dimension - sorry...
		}

		{
			clstate_t clst;
			clst.playernum = j;
			clst.onladder = (int)ent->v.fteflags&FF_LADDER;
			clst.lastcmd = &cl->lastcmd;
			clst.modelindex = vent->v.modelindex;
			clst.modelindex2 = vent->v.vweapmodelindex;
			clst.frame = vent->v.frame;
			clst.weaponframe = ent->v.weaponframe;
			clst.angles = ent->v.angles;
			clst.origin = vent->v.origin;
			clst.velocity = vent->v.velocity;
			clst.effects = ent->v.effects;

			if (((int)vent->v.effects & EF_NODRAW) && progstype == PROG_H2)
			{
				clst.effects = 0;
				clst.modelindex = 0;
			}

			clst.skin = vent->v.skin;
			clst.mins = vent->v.mins;
			clst.hull = vent->v.hull;
			clst.maxs = vent->v.maxs;
			clst.scale = vent->v.scale;
			clst.transparency = vent->v.alpha;

			//QSG_DIMENSION_PLANES - if the only shared dimensions are ghost dimensions, Set half alpha.
			if (((int)clent->v.dimension_see & (int)ent->v.dimension_ghost))
				if (!((int)clent->v.dimension_see & ((int)ent->v.dimension_seen & ~(int)ent->v.dimension_ghost)) )
				{
					if (ent->v.dimension_ghost_alpha)
						clst.transparency *= ent->v.dimension_ghost_alpha;
					else
						clst.transparency *= 0.5;
				}

			clst.fatness = vent->v.fatness;
			clst.localtime = cl->localtime;
			clst.health = ent->v.health;
			clst.spectator = 0;
			clst.fteext = client->fteprotocolextensions;
			clst.zext = client->zquake_extensions;
			clst.cl = cl;

			if (ent != vent)
				clst.modelindex = 0;

			if (sv.demostatevalid)
				clst.health = 100;

			if (sv.demostatevalid)
				clst.playernum = MAX_CLIENTS-1;

			clst.isself = false;
			if ((cl == client || cl->controller == client) && !sv.demostatevalid)
			{
				clst.isself = true;
				clst.spectator = 0;
				if (client->spectator)
				{
					clst.spectator = 2;
					if (client->spec_track)
					{
						clst.mins = svs.clients[client->spec_track-1].edict->v.mins;
						clst.maxs = svs.clients[client->spec_track-1].edict->v.maxs;
						clst.health = svs.clients[client->spec_track-1].edict->v.health;
					}
					else
						clst.health = 1;
				}
			}
			else if (client->spectator || sv.demostatevalid)
			{
				clst.health=100;
				if (client->spec_track && client->spec_track -1 == j)
				{
					if (sv.demostatevalid)
					{
						clst.origin = sv.demostate[client->spec_track].origin;
						clst.velocity = sv.recordedplayer[client->spec_track-1].velocity;
						clst.angles = sv.demostate[client->spec_track].angles;
					}
					clst.spectator = 2;
				}
				else
					clst.spectator = 1;
			}
			if (isbot)
			{
				clst.lastcmd = NULL;
				clst.velocity = NULL;
			}
			SV_WritePlayerToClient(msg, &clst);
		}

//FIXME: Name flags
		//player is visible, now would be a good time to update what the player is like.
		pflags = 0;
		if (client->fteprotocolextensions & PEXT_VWEAP && client->otherclientsknown[j].vweap != ent->v.vweapmodelindex)
		{
			pflags |= 1;
			client->otherclientsknown[j].vweap = ent->v.vweapmodelindex;
		}
		if (pflags)
		{
			ClientReliableWrite_Begin(client, svc_ftesetclientpersist, 10);
			ClientReliableWrite_Short(client, pflags);
			if (pflags & 1)
				ClientReliableWrite_Short(client, client->otherclientsknown[j].vweap);
			ClientReliable_FinishWrite(client);
		}
		if (!sv.demostatevalid)
		{
			SV_SendExtraEntEffects(client, cl->edict);
		}
	}
}


void SVNQ_EmitEntity(sizebuf_t *msg, edict_t *ent, int entnum)
{
#define	NQU_MOREBITS	(1<<0)
#define	NQU_ORIGIN1	(1<<1)
#define	NQU_ORIGIN2	(1<<2)
#define	NQU_ORIGIN3	(1<<3)
#define	NQU_ANGLE2	(1<<4)
#define	NQU_NOLERP	(1<<5)		// don't interpolate movement
#define	NQU_FRAME		(1<<6)
#define NQU_SIGNAL	(1<<7)		// just differentiates from other updates

// svc_update can pass all of the fast update bits, plus more
#define	NQU_ANGLE1	(1<<8)
#define	NQU_ANGLE3	(1<<9)
#define	NQU_MODEL		(1<<10)
#define	NQU_COLORMAP	(1<<11)
#define	NQU_SKIN		(1<<12)
#define	NQU_EFFECTS	(1<<13)
#define	NQU_LONGENTITY	(1<<14)

// LordHavoc's: protocol extension
#define DPU_EXTEND1		(1<<15)
// LordHavoc: first extend byte
#define DPU_DELTA			(1<<16) // no data, while this is set the entity is delta compressed (uses previous frame as a baseline, meaning only things that have changed from the previous frame are sent, except for the forced full update every half second)
#define DPU_ALPHA			(1<<17) // 1 byte, 0.0-1.0 maps to 0-255, not sent if exactly 1, and the entity is not sent if <=0 unless it has effects (model effects are checked as well)
#define DPU_SCALE			(1<<18) // 1 byte, scale / 16 positive, not sent if 1.0
#define DPU_EFFECTS2		(1<<19) // 1 byte, this is .effects & 0xFF00 (second byte)
#define DPU_GLOWSIZE		(1<<20) // 1 byte, encoding is float/4.0, unsigned, not sent if 0
#define DPU_GLOWCOLOR		(1<<21) // 1 byte, palette index, default is 254 (white), this IS used for darklight (allowing colored darklight), however the particles from a darklight are always black, not sent if default value (even if glowsize or glowtrail is set)
// LordHavoc: colormod feature has been removed, because no one used it
#define DPU_COLORMOD		(1<<22) // 1 byte, 3 bit red, 3 bit green, 2 bit blue, this lets you tint an object artifically, so you could make a red rocket, or a blue fiend...
#define DPU_EXTEND2		(1<<23) // another byte to follow
// LordHavoc: second extend byte
#define DPU_GLOWTRAIL		(1<<24) // leaves a trail of particles (of color .glowcolor, or black if it is a negative glowsize)
#define DPU_VIEWMODEL		(1<<25) // attachs the model to the view (origin and angles become relative to it), only shown to owner, a more powerful alternative to .weaponmodel and such
#define DPU_FRAME2		(1<<26) // 1 byte, this is .frame & 0xFF00 (second byte)
#define DPU_MODEL2		(1<<27) // 1 byte, this is .modelindex & 0xFF00 (second byte)
#define DPU_EXTERIORMODEL	(1<<28) // causes this model to not be drawn when using a first person view (third person will draw it, first person will not)
#define DPU_UNUSED29		(1<<29) // future expansion
#define DPU_UNUSED30		(1<<30) // future expansion
#define DPU_EXTEND3		(1<<31) // another byte to follow, future expansion

int i, eff;	
float miss;
unsigned int bits=0;
eval_t *val;

int glowsize, glowcolor;

	if (ent->v.modelindex >= 256)	//as much as protocols can handle
		return;

	if (entnum >= 768)		//too many for a conventional nq client.
		return;

	for (i=0 ; i<3 ; i++)
	{				
		miss = ent->v.origin[i] - ent->baseline.origin[i];
		if ( miss < -0.1 || miss > 0.1 )
			bits |= NQU_ORIGIN1<<i;
	}

	if (ent->v.angles[0] != ent->baseline.angles[0] )
		bits |= NQU_ANGLE1;
		
	if (ent->v.angles[1] != ent->baseline.angles[1] )
		bits |= NQU_ANGLE2;
		
	if (ent->v.angles[2] != ent->baseline.angles[2] )
		bits |= NQU_ANGLE3;
		
	if ((ent->v.movetype == MOVETYPE_STEP || (ent->v.movetype == MOVETYPE_PUSH)) && (bits & (U_ANGLE1|U_ANGLE2|U_ANGLE3)))
		bits |= NQU_NOLERP;	// don't mess up the step animation

	if (ent->baseline.colormap != ent->v.colormap && ent->v.colormap>=0)
		bits |= NQU_COLORMAP;

	if (ent->baseline.skinnum != ent->v.skin)
		bits |= NQU_SKIN;

	if (ent->baseline.frame != ent->v.frame)
		bits |= NQU_FRAME;

	eff = ent->v.effects;

	if ((ent->baseline.effects & 0x00ff) != ((int)eff & 0x00ff))
		bits |= NQU_EFFECTS;	

	if (/*ent->baseline.modelindex !=*/ ent->v.modelindex)
		bits |= NQU_MODEL;

	if (entnum >= 256)
		bits |= NQU_LONGENTITY;


//	if (usedpextensions)
	{
		if (ent->baseline.trans != ent->v.alpha)
			if (!(ent->baseline.trans == 1 && !ent->v.alpha))
				bits |= DPU_ALPHA;
		if (ent->baseline.scale != ent->v.scale)
			bits |= DPU_SCALE;

		if ((ent->baseline.effects&0xff00) != ((int)eff & 0xff00))
			bits |= DPU_EFFECTS2;


		val = svprogfuncs->GetEdictFieldValue(svprogfuncs, ent, "glow_size", NULL);	//ouch.. null...
		if (val)
			glowsize = val->_float*0.25f;
		else
			glowsize = 0;
		val = svprogfuncs->GetEdictFieldValue(svprogfuncs, ent, "glow_color", NULL);	//ouch.. null...
		if (val)
			glowcolor = val->_float;
		else
			glowcolor = 0;
		
		if (0 != glowsize)
			bits |= DPU_GLOWSIZE;
		if (0 != glowcolor)
			bits |= DPU_GLOWCOLOR;
	}


	if (bits & 0xFF00)
		bits |= NQU_MOREBITS;
	if (bits & 0xFF0000)
		bits |= DPU_EXTEND1;
	if (bits & 0xFF000000)
		bits |= DPU_EXTEND2;


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
		MSG_WriteShort (msg,entnum);
	else
		MSG_WriteByte (msg,entnum);

	if (bits & NQU_MODEL)		MSG_WriteByte (msg,	ent->v.modelindex);
	if (bits & NQU_FRAME)		MSG_WriteByte (msg, ent->v.frame);
	if (bits & NQU_COLORMAP)	MSG_WriteByte (msg, ent->v.colormap);
	if (bits & NQU_SKIN)		MSG_WriteByte (msg, ent->v.skin);
	if (bits & NQU_EFFECTS)		MSG_WriteByte (msg, eff & 0x00ff);		
	if (bits & NQU_ORIGIN1)		MSG_WriteCoord (msg, ent->v.origin[0]);		
	if (bits & NQU_ANGLE1)		MSG_WriteAngle(msg, ent->v.angles[0]);
	if (bits & NQU_ORIGIN2)		MSG_WriteCoord (msg, ent->v.origin[1]);
	if (bits & NQU_ANGLE2)		MSG_WriteAngle(msg, ent->v.angles[1]);
	if (bits & NQU_ORIGIN3)		MSG_WriteCoord (msg, ent->v.origin[2]);
	if (bits & NQU_ANGLE3)		MSG_WriteAngle(msg, ent->v.angles[2]);

	if (bits & DPU_ALPHA)		MSG_WriteByte(msg, ent->v.alpha*255);
	if (bits & DPU_SCALE)		MSG_WriteByte(msg, ent->v.scale*16);
	if (bits & DPU_EFFECTS2)	MSG_WriteByte(msg, eff >> 8);
	if (bits & DPU_GLOWSIZE)	MSG_WriteByte(msg, glowsize);
	if (bits & DPU_GLOWCOLOR)	MSG_WriteByte(msg, glowcolor);
//	if (bits & DPU_COLORMOD)	MSG_WriteByte(msg, colormod);
	if (bits & DPU_FRAME2)		MSG_WriteByte(msg, (int)ent->v.frame >> 8);
	if (bits & DPU_MODEL2)		MSG_WriteByte(msg, (int)ent->v.modelindex >> 8);
}

typedef struct gibfilter_s {
	struct gibfilter_s *next;
	int modelindex;
	int minframe;
	int maxframe;
} gibfilter_t;
gibfilter_t *gibfilter;
void SV_GibFilterAdd(char *modelname, int min, int max)
{
	int i;
	gibfilter_t *gf;	

	for (i=1; *sv.model_precache[i] ; i++)
		if (!strcmp(sv.model_precache[i], modelname))
			break;
	if (!*sv.model_precache[i])
	{
		Con_Printf("Filtered model \"%s\" was not precached\n", modelname);
		return;	//model not in use.
	}

	gf = Z_Malloc(sizeof(gibfilter_t));
	gf->modelindex = i;
	gf->minframe = ((min==-1)?0:min);
	gf->maxframe = ((max==-1)?255:max);
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

	file = COM_LoadStackFile("gibfiltr.cfg", buffer, sizeof(buffer));
	if (!file)
	{
		Con_Printf("gibfiltr.cfg file was not found. The gib filter will be disabled\n");
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
		SV_GibFilterAdd(com_token, min, max);
	}
}
qboolean SV_GibFilter(edict_t	*ent)
{
	int indx = ent->v.modelindex;
	int frame = ent->v.frame;
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

void Q2BSP_FatPVS(vec3_t org, qboolean add)
{
	int		leafnum;
	leafnum = CM_PointLeafnum (org);
	clientarea = CM_LeafArea (leafnum);

	SV_Q2BSP_FatPVS (org);
}

qboolean Q2BSP_EdictInFatPVS(edict_t *ent)
{
	int i,l;
	if (!CM_AreasConnected (clientarea, ent->areanum))
	{	// doors can legally straddle two areas, so
		// we may need to check another one
		if (!ent->areanum2
			|| !CM_AreasConnected (clientarea, ent->areanum2))
			return false;		// blocked by a door
	}

	if (ent->num_leafs == -1)
	{	// too many leafs for individual check, go by headnode
		if (!CM_HeadnodeVisible (ent->headnode, fatpvs))
			return false;
	}
	else
	{	// check individual leafs
		for (i=0 ; i < ent->num_leafs ; i++)
		{
			l = ent->leafnums[i];
			if (fatpvs[l >> 3] & (1 << (l&7) ))
				break;
		}
		if (i == ent->num_leafs)
			return false;		// not visible
	}
	return true;
}
#endif
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
#define DEPTHOPTIMISE
#ifdef DEPTHOPTIMISE
	float distances[MAX_EXTENDED_PACKET_ENTITIES];
	float dist;
#endif

	int		e, i;
	qbyte	*pvs;
	vec3_t	org;
	edict_t	*ent;
	packet_entities_t	*pack;
	entity_state_t *dement;
	edict_t	*clent;
	client_frame_t	*frame;
	entity_state_t	*state;
#ifdef NQPROT
	int nqprot = client->nqprot;
#endif

	client_t *split;

	// this is the frame we are creating
	frame = &client->frames[client->netchan.incoming_sequence & UPDATE_MASK];

	// find the client's PVS

	if (!ignorepvs)
	{
		clent = client->edict;
		VectorAdd (clent->v.origin, clent->v.view_ofs, org);

		sv.worldmodel->funcs.FatPVS(org, false);

#ifdef PEXT_VIEW2	
		if (clent->v.view2)
			sv.worldmodel->funcs.FatPVS(PROG_TO_EDICT(svprogfuncs, clent->v.view2)->v.origin, true);
#endif
		for (split = client->controlled; split; split = split->controlled)
			sv.worldmodel->funcs.FatPVS(split->edict->v.origin, true);
/*
		if (sv.worldmodel->fromgame == fg_doom)
		{
		}
		else
#ifdef Q2BSPS
			if (sv.worldmodel->fromgame == fg_quake2 || sv.worldmodel->fromgame == fg_quake3)
		{
			leafnum = CM_PointLeafnum (org);
			clientarea = CM_LeafArea (leafnum);
			clientcluster = CM_LeafCluster (leafnum);

			SV_Q2BSP_FatPVS (org);
		}
		else
#endif
		{
			SV_Q1BSP_FatPVS (org);

#ifdef PEXT_VIEW2	
			if (clent->v.view2)
				SV_Q1BSP_AddToFatPVS (PROG_TO_EDICT(svprogfuncs, clent->v.view2)->v.origin, sv.worldmodel->nodes);	//add a little more...
#endif
			for (split = client->controlled; split; split = split->controlled)
				SV_Q1BSP_AddToFatPVS (split->edict->v.origin, sv.worldmodel->nodes);	//add a little more...
		}
*/
	}
	else
		clent = NULL;

	pvs = fatpvs;

	// send over the players in the PVS
	SV_WritePlayersToClient (client, clent, pvs, msg);	
	
	// put other visible entities into either a packet_entities or a nails message
	pack = &frame->entities;
	pack->num_entities = 0;

	numnails = 0;
#ifdef PEXT_LIGHTUPDATES
	numlight = 0;
#endif

	if (sv.demostatevalid)	//generate info from demo stats
	{
		for (e=1, dement=&sv.demostate[e] ; e<sv.num_edicts ; e++, dement++)
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
			if (dement->modelindex >= 256 && !(client->fteprotocolextensions & PEXT_MODELDBL))
				continue;

			state = &pack->entities[pack->num_entities];
			pack->num_entities++;

			state->number = e;
			state->flags = 0;
			VectorCopy (dement->origin, state->origin);
			VectorCopy (dement->angles, state->angles);
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
			if (!state->trans)
				state->trans = 1;
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

		// encode the packet entities as a delta from the
		// last packetentities acknowledged by the client
		SV_EmitPacketEntities (client, pack, msg);

		// now add the specialized nail update
		SV_EmitNailUpdate (msg, ignorepvs);

		return;
	}

	if (client->viewent
 #ifdef NQPROT
  && !nqprot
  #endif
  )	//this entity is watching from outside themselves. The client is tricked into thinking that they themselves are in the view ent, and a new dummy ent (the old them) must be spawned.
	{
		distances[0] = 0;
		state = &pack->entities[pack->num_entities];
		pack->num_entities++;

		state->number = client - svs.clients + 1;
		state->flags = 0;
		VectorCopy (clent->v.origin, state->origin);
		VectorCopy (clent->v.angles, state->angles);
		state->modelindex = clent->v.modelindex;
		state->frame = clent->v.frame;
		state->colormap = clent->v.colormap;
		state->skinnum = clent->v.skin;
		state->effects = clent->v.effects;
		state->drawflags = clent->v.drawflags;
		state->abslight = clent->v.abslight;

#ifdef PEXT_SCALE
		state->scale = clent->v.scale;
#endif
#ifdef PEXT_TRANS
		state->trans = clent->v.alpha;
		if (!state->trans)
			state->trans = 1;
#endif
#ifdef PEXT_FATNESS
		state->fatness = clent->v.fatness;
#endif

		if (state->effects & EF_FLAG1)
		{
			memcpy(&pack->entities[pack->num_entities], state, sizeof(*state));
			state = &pack->entities[pack->num_entities];
			pack->num_entities++;
			state->modelindex = SV_ModelIndex("progs/flag.mdl");
			state->frame = 0;
			state->number++;
			state->skinnum = 0;
		}
		else if (state->effects & EF_FLAG2)
		{
			memcpy(&pack->entities[pack->num_entities], state, sizeof(*state));
			state = &pack->entities[pack->num_entities];
			pack->num_entities++;
			state->modelindex = SV_ModelIndex("progs/flag.mdl");
			state->frame = 0;
			state->number++;
			state->skinnum = 1;
		}
	}

#ifdef NQPROT
	for (e=(nqprot?1:sv.allocated_client_slots+1) ; e<sv.num_edicts ; e++)
#else
	for (e=sv.allocated_client_slots+1 ; e<sv.num_edicts ; e++)
#endif
	{
		ent = EDICT_NUM(svprogfuncs, e);

		// ignore ents without visible models
		if (!ent->v.modelindex || !*PR_GetString(svprogfuncs, ent->v.model))
			continue;

		if (progstype != PROG_QW)
		{
			if (progstype == PROG_H2 && (int)ent->v.effects & EF_NODRAW)
				continue;
			if ((int)ent->v.effects & EF_MUZZLEFLASH)
			{
				if (needcleanup < e)
				{
					needcleanup = e;
					MSG_WriteByte(&sv.multicast, svc_muzzleflash);
					MSG_WriteShort(&sv.multicast, e);
					SV_Multicast(ent->v.origin, MULTICAST_PVS);				
				}
			}
		}

		if (!ignorepvs)
		{
			if (ent->tagent)
			{
				edict_t *p = ent;
				int c = 10;
				while(p->tagent&&c-->0)
				{
					p = EDICT_NUM(svprogfuncs, p->tagent);
				}
				if (!sv.worldmodel->funcs.EdictInFatPVS(p))
					continue;
			}
			else
			{
				if (!sv.worldmodel->funcs.EdictInFatPVS(ent))
					continue;
			}
			/*
#ifdef Q2BSPS
			if (sv.worldmodel->fromgame == fg_quake2 || sv.worldmodel->fromgame == fg_quake3)
			{//quake2 vising logic
				// check area
				if (!CM_AreasConnected (clientarea, ent->areanum))
				{	// doors can legally straddle two areas, so
					// we may need to check another one
					if (!ent->areanum2
						|| !CM_AreasConnected (clientarea, ent->areanum2))
						continue;		// blocked by a door
				}

				if (ent->num_leafs == -1)
				{	// too many leafs for individual check, go by headnode
					if (!CM_HeadnodeVisible (ent->headnode, fatpvs))
						continue;
				}
				else
				{	// check individual leafs
					for (i=0 ; i < ent->num_leafs ; i++)
					{
						l = ent->leafnums[i];
						if (fatpvs[l >> 3] & (1 << (l&7) ))
							break;
					}
					if (i == ent->num_leafs)
						continue;		// not visible
				}
			}
			else
#endif
				if (sv.worldmodel->fromgame == fg_doom)
				{
				}
			else
			{//quake1 vising logic
				// ignore if not touching a PV leaf
				for (i=0 ; i < ent->num_leafs ; i++)
					if (pvs[ent->leafnums[i] >> 3] & (1 << (ent->leafnums[i]&7) ))
						break;
					
				if (i == ent->num_leafs)		// not visible
				{
					continue;
				}
			}
			*/
		}





		if (client->gibfilter && SV_GibFilter(ent))
			continue;

//		if (strstr(sv.model_precache[(int)ent->v.modelindex], "gib"))
//			continue;

		//QC code doesn't want some clients to see some ents.
#define SF_OWNERSEEONLY 1
#define SF_OWNERDONTSEE 2
#define SF_OWNERTEAMONLY 4
#define SF_OWNERTEAMDONTSEE 8		
		if (ent->v.sendflags && !ignorepvs)	//hmm
		{
			if ((int)ent->v.sendflags & SF_OWNERSEEONLY)
			{
				if (PROG_TO_EDICT(svprogfuncs, ent->v.owner) != clent)
					continue;
			}
			if ((int)ent->v.sendflags & SF_OWNERDONTSEE)
			{
				if (PROG_TO_EDICT(svprogfuncs, ent->v.owner) == clent)
					continue;
			}
			if ((int)ent->v.sendflags & SF_OWNERTEAMONLY)
			{
				if (ent->v.team != clent->v.team)
					continue;
			}
			if ((int)ent->v.sendflags & SF_OWNERTEAMDONTSEE)
			{
				if (ent->v.team == clent->v.team)
					continue;
			}
		}
		if (ent->v.nodrawtoclient)	//DP extension.
			if (ent->v.nodrawtoclient == EDICT_TO_PROG(svprogfuncs, client->edict))
				continue;
		if (ent->v.drawonlytoclient)
			if (ent->v.drawonlytoclient != EDICT_TO_PROG(svprogfuncs, client->edict))
			{
				client_t *split;
				for (split = client->controlled; split; split=split->controlled)
				{
					if (split->edict->v.view2 == EDICT_TO_PROG(svprogfuncs, ent))
						break;
				}
				if (!split)
					continue;
			}

		//QSG_DIMENSION_PLANES
		if (client->edict)
			if (!((int)client->edict->v.dimension_see & ((int)ent->v.dimension_seen | (int)ent->v.dimension_ghost)))
				continue;	//not in this dimension - sorry...

#ifdef NQPROT
		if (nqprot)
		{
			SVNQ_EmitEntity(msg, ent, e);			
			continue;
		}
#endif
		if (SV_AddNailUpdate (ent))
			continue;	// added to the special update list
#ifdef PEXT_LIGHTUPDATES
		if (client->fteprotocolextensions & PEXT_LIGHTUPDATES)
			if (SV_AddLightUpdate (ent))
				continue;
#endif

		//the entity would mess up the client and possibly disconnect them.
		//FIXME: add an option to drop clients... entity fog could be killed in this way.
		if (e >= 512 && !(client->fteprotocolextensions & PEXT_ENTITYDBL))
			continue;
		if (e >= 1024 && !(client->fteprotocolextensions & PEXT_ENTITYDBL2))
			continue;
		if (ent->v.modelindex >= 256 && !(client->fteprotocolextensions & PEXT_MODELDBL))
			continue;

#ifdef DEPTHOPTIMISE
		if (clent)
		{
			//find distance based upon absolute mins/maxs so bsps are treated fairly.
			VectorAdd(ent->v.absmin, ent->v.absmax, org);
			VectorMA(clent->v.origin, -0.5, org, org);
			dist = Length(org);

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

		state->number = e;
		state->flags = 0;
		VectorCopy (ent->v.origin, state->origin);
		VectorCopy (ent->v.angles, state->angles);
		state->modelindex = ent->v.modelindex;
		state->frame = ent->v.frame;
		state->colormap = ent->v.colormap;
		state->skinnum = ent->v.skin;
		state->effects = ent->v.effects;
		state->drawflags = ent->v.drawflags;
		state->abslight = (int)(ent->v.abslight*255) & 255;
		if ((int)ent->v.flags & FL_CLASS_DEPENDENT && client->playerclass)
		{
			char modname[MAX_QPATH];
			Q_strncpyz(modname, sv.model_precache[state->modelindex], sizeof(modname));
			if (strlen(modname)>5)
			{
				modname[strlen(modname)-5] = client->playerclass+'0';
				state->modelindex = SV_ModelIndex(modname);
			}
		}
		if (progstype == PROG_H2 && ent->v.solid == SOLID_BSP)
			state->angles[0]*=-1;

		if (state->effects & EF_FULLBRIGHT)
		{
			state->abslight = 255;
			state->drawflags |= MLS_ABSLIGHT;
		}
		if (progstype != PROG_QW)	//don't send extra nq effects to a qw client.
			state->effects &= EF_BRIGHTLIGHT | EF_DIMLIGHT;

#ifdef PEXT_SCALE
		state->scale = ent->v.scale;
#endif
#ifdef PEXT_TRANS
		state->trans = ent->v.alpha;
		if (!state->trans)
			state->trans = 1;

		//QSG_DIMENSION_PLANES - if the only shared dimensions are ghost dimensions, Set half alpha.
		if (client->edict)
			if (((int)client->edict->v.dimension_see & (int)ent->v.dimension_ghost))
				if (!((int)client->edict->v.dimension_see & ((int)ent->v.dimension_seen & ~(int)ent->v.dimension_ghost)) )
				{
					if (ent->v.dimension_ghost_alpha)
						state->trans *= ent->v.dimension_ghost_alpha;
					else
						state->trans *= 0.5;
				}
#endif
#ifdef PEXT_FATNESS
		state->fatness = ent->v.fatness;
#endif
	}
#ifdef NQPROT
	if (nqprot)
		return;
#endif

	if (!sv.demostatevalid)
	{
		for (i = 0; i < pack->num_entities; i++)
		{
			SV_SendExtraEntEffects(client, EDICT_NUM(svprogfuncs, pack->entities[i].number));
		}
	}

	// encode the packet entities as a delta from the
	// last packetentities acknowledged by the client

	SV_EmitPacketEntities (client, pack, msg);

	// now add the specialized nail update
	SV_EmitNailUpdate (msg, ignorepvs);
}

void SV_CleanupEnts(void)
{
	int		e;
	edict_t	*ent;

	if (!needcleanup)
		return;

	for (e=1 ; e<=needcleanup ; e++)
	{
		ent = EDICT_NUM(svprogfuncs, e);
		if ((int)ent->v.effects & EF_MUZZLEFLASH)
			ent->v.effects = (int)ent->v.effects & ~EF_MUZZLEFLASH;
	}
	needcleanup=0;
}
