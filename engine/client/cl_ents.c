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
// cl_ents.c -- entity parsing and management

#include "quakedef.h"

extern	cvar_t	cl_predict_players;
extern	cvar_t	cl_predict_players2;
extern	cvar_t	cl_solid_players;
extern	cvar_t	gl_part_inferno;
extern	cvar_t	cl_item_bobbing;
extern int cl_playerindex;

static struct predicted_player {
	int flags;
	int frame;
	int oldframe;
	float lerptime;
	qboolean active;
	vec3_t origin;	// predicted origin
} predicted_players[MAX_CLIENTS];

float newlerprate;

//============================================================

/*
===============
CL_AllocDlight

===============
*/
dlight_t *CL_AllocDlight (int key)
{
	int		i;
	dlight_t	*dl;

// first look for an exact key match
	if (key)
	{
		dl = cl_dlights;
		for (i=0 ; i<MAX_DLIGHTS ; i++, dl++)
		{
			if (dl->key == key)
			{
				memset (dl, 0, sizeof(*dl));
				dl->key = key;
				return dl;
			}
		}
	}

// then look for anything else
	dl = cl_dlights;
	for (i=0 ; i<MAX_DLIGHTS ; i++, dl++)
	{
		if (dl->die < cl.time)
		{
			memset (dl, 0, sizeof(*dl));
			dl->key = key;
			return dl;
		}
	}

	dl = &cl_dlights[0];
	memset (dl, 0, sizeof(*dl));
	dl->key = key;
	return dl;
}

/*
===============
CL_NewDlight
===============
*/
dlight_t *CL_NewDlight (int key, float x, float y, float z, float radius, float time,
				   int type)
{
	dlight_t	*dl;

	dl = CL_AllocDlight (key);
	dl->origin[0] = x;
	dl->origin[1] = y;
	dl->origin[2] = z;
	dl->radius = radius;
	dl->die = cl.time + time;
	if (type == 0) {
		dl->color[0] = 0.2;
		dl->color[1] = 0.1;
		dl->color[2] = 0.05;
	} else if (type == 1) {
		dl->color[0] = 0.05;
		dl->color[1] = 0.05;
		dl->color[2] = 0.3;
	} else if (type == 2) {
		dl->color[0] = 0.5;
		dl->color[1] = 0.05;
		dl->color[2] = 0.05;
	} else if (type == 3) {
		dl->color[0]=0.5;
		dl->color[1] = 0.05;
		dl->color[2] = 0.4;
	}

	return dl;
}
void CL_NewDlightRGB (int key, float x, float y, float z, float radius, float time,
				   float r, float g, float b)
{
	dlight_t	*dl;

	dl = CL_AllocDlight (key);
	dl->origin[0] = x;
	dl->origin[1] = y;
	dl->origin[2] = z;
	dl->radius = radius;
	dl->die = cl.time + time;
	dl->color[0] = r;
	dl->color[1] = g;
	dl->color[2] = b;
}


/*
===============
CL_DecayLights

===============
*/
void CL_DecayLights (void)
{
	int			i;
	dlight_t	*dl;

	if (cl.paused)	//DON'T DO IT!!!
		return;

	dl = cl_dlights;
	for (i=0 ; i<MAX_DLIGHTS ; i++, dl++)
	{
		if (dl->die < cl.time || !dl->radius)
			continue;
		
		dl->radius -= host_frametime*dl->decay;
		if (dl->radius < 0)
			dl->radius = 0;

		if (dl->channelfade[0])
		{
			dl->color[0] -= host_frametime*dl->channelfade[0]; 
			if (dl->color[0] < 0)
				dl->color[0] = 0;
		}

		if (dl->channelfade[1])
		{
			dl->color[1] -= host_frametime*dl->channelfade[1]; 
			if (dl->color[1] < 0)
				dl->color[1] = 0;
		}

		if (dl->channelfade[2])
		{
			dl->color[2] -= host_frametime*dl->channelfade[2]; 
			if (dl->color[2] < 0)
				dl->color[2] = 0;
		}
#ifdef Q2CLIENT	//FIXME: Why? why just q2 clients?
		if (cls.q2server)
			if (!dl->decay)
				dl->radius = 0;
#endif
	}
}


/*
=========================================================================

PACKET ENTITY PARSING / LINKING

=========================================================================
*/

/*
==================
CL_ParseDelta

Can go from either a baseline or a previous packet_entity
==================
*/
int	bitcounts[32];	/// just for protocol profiling
void CL_ParseDelta (entity_state_t *from, entity_state_t *to, int bits, qboolean new)
{
	int			i;
#ifdef PROTOCOLEXTENSIONS
	int morebits=0;
#endif
	vec3_t move;

	// set everything to the state we are delta'ing from
	*to = *from;

	to->number = bits & 511;
	bits &= ~511;

	if (bits & U_MOREBITS)
	{	// read in the low order bits
		i = MSG_ReadByte ();
		bits |= i;
	}

	// count the bits for net profiling
	for (i=0 ; i<16 ; i++)
		if (bits&(1<<i))
			bitcounts[i]++;

	to->flags = bits;

#ifdef PROTOCOLEXTENSIONS
	if (bits & U_EVENMORE && cls.fteprotocolextensions)
		morebits = MSG_ReadByte ();
	if (morebits & U_YETMORE)
		morebits |= MSG_ReadByte()<<8;
#endif

	if (bits & U_MODEL)
		to->modelindex = MSG_ReadByte ();

	if (bits & U_FRAME)
		to->frame = MSG_ReadByte ();

	if (bits & U_COLORMAP)
		to->colormap = MSG_ReadByte();

	if (bits & U_SKIN)
		to->skinnum = MSG_ReadByte();

	if (bits & U_EFFECTS)
		to->effects = MSG_ReadByte();

	if (bits & U_ORIGIN1)
		to->origin[0] = MSG_ReadCoord ();
		
	if (bits & U_ANGLE1)
		to->angles[0] = MSG_ReadAngle ();

	if (bits & U_ORIGIN2)
		to->origin[1] = MSG_ReadCoord ();
		
	if (bits & U_ANGLE2)
		to->angles[1] = MSG_ReadAngle ();

	if (bits & U_ORIGIN3)
		to->origin[2] = MSG_ReadCoord ();
		
	if (bits & U_ANGLE3)
		to->angles[2] = MSG_ReadAngle ();

	if (bits & U_SOLID)
	{
		// FIXME
	}

#ifdef PEXT_SCALE
	if (morebits & U_SCALE && cls.fteprotocolextensions & PEXT_SCALE)
		to->scale = (float)MSG_ReadByte() / 100.0;
#endif
#ifdef PEXT_TRANS
	if (morebits & U_TRANS && cls.fteprotocolextensions & PEXT_TRANS)
		to->trans = (float)MSG_ReadByte() / 255;
#endif
#ifdef PEXT_FATNESS
	if (morebits & U_FATNESS && cls.fteprotocolextensions & PEXT_FATNESS)
		to->trans = (float)MSG_ReadChar() / 2;
#endif

	if (morebits & U_DRAWFLAGS && cls.fteprotocolextensions & PEXT_HEXEN2)
		to->drawflags = MSG_ReadByte();
	if (morebits & U_ABSLIGHT && cls.fteprotocolextensions & PEXT_HEXEN2)
		to->abslight = MSG_ReadByte();

	if (morebits & U_ENTITYDBL)
		to->number += 512;
	if (morebits & U_ENTITYDBL2)
		to->number += 1024;
	if (morebits & U_MODELDBL)
		to->modelindex += 256;

	VectorSubtract(to->origin, from->origin, move);

#ifdef HALFLIFEMODELS
	if (to->frame != from->frame)
		cl.lerpents[to->number].framechange = cl.time;	//marked for hl models
#endif
	if (to->modelindex != from->modelindex || to->number != from->number || VectorLength(move)>128)	//model changed... or entity changed...
	{
#ifdef HALFLIFEMODELS
		cl.lerpents[to->number].framechange = cl.time;	//marked for hl models
#endif
		cl.lerpents[to->number].lerptime = -10;

		if (!new)
			return;
		move[0] = 1;	//make sure it enters the next block.
	}
	if (to->frame != from->frame || move[0] || move[1] || move[2])
	{
		cl.lerpents[to->number].origin[0] = from->origin[0];
		cl.lerpents[to->number].origin[1] = from->origin[1];
		cl.lerpents[to->number].origin[2] = from->origin[2];

		cl.lerpents[to->number].angles[0] = from->angles[0];
		cl.lerpents[to->number].angles[1] = from->angles[1];
		cl.lerpents[to->number].angles[2] = from->angles[2];
//we have three sorts of movement.
//1: stepping monsters. These have frames and tick at 10fps.
//2: physics. Objects moving acording to gravity.
//3: both. This is really awkward. And I'm really lazy.
		cl.lerpents[to->number].lerprate = cl.time-cl.lerpents[to->number].lerptime;	//time per update
		cl.lerpents[to->number].frame = from->frame;
		cl.lerpents[to->number].lerptime = cl.time;

		if (cl.lerpents[to->number].lerprate>0.5)
			cl.lerpents[to->number].lerprate=0.1;

		//store this off for new ents to use.
		if (new)
			cl.lerpents[to->number].lerptime = newlerprate;
		if (to->frame == from->frame && !new) //(h2 runs at 20fps)
			newlerprate = cl.time-cl.lerpents[to->number].lerptime;
	}
}


/*
=================
FlushEntityPacket
=================
*/
void FlushEntityPacket (void)
{
	int			word;
	entity_state_t	olde, newe;

	Con_DPrintf ("FlushEntityPacket\n");

	memset (&olde, 0, sizeof(olde));

	cl.validsequence = 0;		// can't render a frame
	cl.frames[cls.netchan.incoming_sequence&UPDATE_MASK].invalid = true;

	// read it all, but ignore it
	while (1)
	{
		word = (unsigned short)MSG_ReadShort ();
		if (msg_badread)
		{	// something didn't parse right...
			Host_EndGame ("msg_badread in packetentities");
			return;
		}

		if (!word)
			break;	// done

		CL_ParseDelta (&olde, &newe, word, true);
	}
}

/*
==================
CL_ParsePacketEntities

An svc_packetentities has just been parsed, deal with the
rest of the data stream.
==================
*/
void CL_ParsePacketEntities (qboolean delta)
{
	int			oldpacket, newpacket;
	packet_entities_t	*oldp, *newp, dummy;
	int			oldindex, newindex;
	int			word, newnum, oldnum;
	qboolean	full;
	qbyte		from;

	newpacket = cls.netchan.incoming_sequence&UPDATE_MASK;
	newp = &cl.frames[newpacket].packet_entities;
	cl.frames[newpacket].invalid = false;

	if (delta)
	{
		from = MSG_ReadByte ();

		oldpacket = cl.frames[newpacket].delta_sequence;

		if (cls.demoplayback == DPB_MVD)
			from = oldpacket = cls.netchan.incoming_sequence - 1;

		if ( (from&UPDATE_MASK) != (oldpacket&UPDATE_MASK) )
			Con_DPrintf ("WARNING: from mismatch\n");
	}
	else
		oldpacket = -1;

	full = false;
	if (oldpacket != -1)
	{
		if (cls.netchan.outgoing_sequence - oldpacket >= UPDATE_BACKUP-1)
		{	// we can't use this, it is too old
			FlushEntityPacket ();
			return;
		}
		cl.oldvalidsequence = cl.validsequence;
		cl.validsequence = cls.netchan.incoming_sequence;
		oldp = &cl.frames[oldpacket&UPDATE_MASK].packet_entities;
	}
	else
	{	// this is a full update that we can start delta compressing from now
		oldp = &dummy;
		dummy.num_entities = 0;
		cl.oldvalidsequence = cl.validsequence;
		cl.validsequence = cls.netchan.incoming_sequence;
		full = true;
	}

	oldindex = 0;
	newindex = 0;
	newp->num_entities = 0;

	while (1)
	{
		word = (unsigned short)MSG_ReadShort ();
		if (msg_badread)
		{	// something didn't parse right...
			Host_EndGame ("msg_badread in packetentities");
			return;
		}

		if (!word)
		{
			while (oldindex < oldp->num_entities)
			{	// copy all the rest of the entities from the old packet
//Con_Printf ("copy %i\n", oldp->entities[oldindex].number);
				if (newindex >= newp->max_entities)
				{
					newp->max_entities = newindex+1;
					newp->entities = BZ_Realloc(newp->entities, sizeof(entity_state_t)*newp->max_entities);
				}
				if (oldindex >= oldp->max_entities)
					Host_EndGame("Old packet entity too big\n");
				newp->entities[newindex] = oldp->entities[oldindex];
				newindex++;
				oldindex++;
			}
			break;
		}
		newnum = word&511;

		if (word & U_MOREBITS)
		{
			int oldpos = msg_readcount;
			int excessive;
			excessive = MSG_ReadByte();
			if (excessive & U_EVENMORE)
			{
				excessive = MSG_ReadByte();
				if (excessive & U_ENTITYDBL)
					newnum += 512;
				if (excessive & U_ENTITYDBL2)
					newnum += 1024;
			}			

			msg_readcount = oldpos;//undo the read...
		}
		oldnum = oldindex >= oldp->num_entities ? 9999 : oldp->entities[oldindex].number;

		while (newnum > oldnum)
		{
			if (full)
			{
				Con_Printf ("WARNING: oldcopy on full update");
				FlushEntityPacket ();
				return;
			}

//Con_Printf ("copy %i\n", oldnum);
			// copy one of the old entities over to the new packet unchanged
			if (newindex >= newp->max_entities)
			{
				newp->max_entities = newindex+1;
				newp->entities = BZ_Realloc(newp->entities, sizeof(entity_state_t)*newp->max_entities);
			}
			if (oldindex >= oldp->max_entities)
					Host_EndGame("Old packet entity too big\n");
			newp->entities[newindex] = oldp->entities[oldindex];
			newindex++;
			oldindex++;
			oldnum = oldindex >= oldp->num_entities ? 9999 : oldp->entities[oldindex].number;
		}

		if (newnum < oldnum)
		{	// new from baseline
//Con_Printf ("baseline %i\n", newnum);
			if (word & U_REMOVE)
			{
				if (full)
				{
					cl.validsequence = 0;
					Con_Printf ("WARNING: U_REMOVE on full update\n");
					FlushEntityPacket ();
					return;
				}
				continue;
			}
			if (newindex >= newp->max_entities)
			{
				newp->max_entities = newindex+1;
				newp->entities = BZ_Realloc(newp->entities, sizeof(entity_state_t)*newp->max_entities);
			}

			CL_ParseDelta (&cl_baselines[newnum], &newp->entities[newindex], word, true);
			newindex++;
			continue;
		}

		if (newnum == oldnum)
		{	// delta from previous
			if (full)
			{
				cl.validsequence = 0;
				Con_Printf ("WARNING: delta on full update");
			}
			if (word & U_REMOVE)
			{
				oldindex++;
				continue;
			}

			if (newindex >= newp->max_entities)
			{
				newp->max_entities = newindex+1;
				newp->entities = BZ_Realloc(newp->entities, sizeof(entity_state_t)*newp->max_entities);
			}

//Con_Printf ("delta %i\n",newnum);
			CL_ParseDelta (&oldp->entities[oldindex], &newp->entities[newindex], word, false);
			newindex++;
			oldindex++;
		}

	}

	newp->num_entities = newindex;
}

#ifdef NQPROT
entity_state_t *CL_FindOldPacketEntity(int num)
{
	int					pnum;
	entity_state_t		*s1;
	packet_entities_t	*pack;
	if (!cls.netchan.incoming_sequence)
		return NULL;
	pack = &cl.frames[(cls.netchan.incoming_sequence-1)&UPDATE_MASK].packet_entities;

	for (pnum=0 ; pnum<pack->num_entities ; pnum++)
	{
		s1 = &pack->entities[pnum];

		if (num == s1->number)
			return s1;
	}
	return NULL;
}

void CLNQ_ParseEntity(int bits)
{
	int i;
	int num, pnum;
	entity_state_t		*state, *from;	
	entity_state_t	*base;
	static float lasttime;
	packet_entities_t	*pack;
	cl.validsequence=1;
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

	if (cls.signon == 4 - 1)
	{	// first update is the final signon stage
		cls.signon = 4;
		CLNQ_SignonReply ();
	}	
	pack = &cl.frames[cls.netchan.incoming_sequence&UPDATE_MASK].packet_entities;	


	if (bits & NQU_MOREBITS)
	{
		i = MSG_ReadByte ();
		bits |= (i<<8);
	}

	if (bits & NQU_LONGENTITY)	
		num = MSG_ReadShort ();
	else
		num = MSG_ReadByte ();

//	state = CL_FindPacketEntity(num);
//	if (!state)
	{
//		if ((int)(lasttime*100) != (int)(realtime*100))
//			pack->num_entities=0;
//		else
			if (pack->num_entities==pack->max_entities)
		{
			pack->max_entities = pack->num_entities+1;
			pack->entities = BZ_Realloc(pack->entities, sizeof(entity_state_t)*pack->max_entities);
		}
		lasttime = realtime;
		state = &pack->entities[pack->num_entities++];
	}

	from = CL_FindOldPacketEntity(num);

	base = &cl_baselines[num];

	state->number = num;

	if (bits & NQU_MODEL)	
		state->modelindex = MSG_ReadByte ();
	else
		state->modelindex = base->modelindex;

	if (bits & NQU_FRAME)
		state->frame = MSG_ReadByte();
	else
		state->frame = base->frame;

	if (bits & NQU_COLORMAP)
		state->colormap = MSG_ReadByte();
	else
		state->colormap = 0;

	if (bits & NQU_SKIN)
		state->skinnum = MSG_ReadByte();
	else
		state->skinnum = base->skinnum;

	if (bits & NQU_EFFECTS)
		state->effects = MSG_ReadByte();
	else
		state->effects = base->effects;

	if (bits & NQU_ORIGIN1)
		state->origin[0] = MSG_ReadCoord ();
	else
		state->origin[0] = base->origin[0];
	if (bits & NQU_ANGLE1)
		state->angles[0] = MSG_ReadAngle();
	else
		state->angles[0] = base->angles[0];

	if (bits & NQU_ORIGIN2)
		state->origin[1] = MSG_ReadCoord ();
	else
		state->origin[1] = base->origin[1];
	if (bits & NQU_ANGLE2)
		state->angles[1] = MSG_ReadAngle();
	else
		state->angles[1] = base->angles[1];

	if (bits & NQU_ORIGIN3)
		state->origin[2] = MSG_ReadCoord ();
	else
		state->origin[2] = base->origin[2];
	if (bits & NQU_ANGLE3)
		state->angles[2] = MSG_ReadAngle();
	else
		state->angles[2] = base->angles[2];

	if (cls.demoplayback != DPB_NONE)
		for (pnum = 0; pnum < cl.splitclients; pnum++)
			if (num == cl.viewentity[pnum])
			{
				state->angles[0] = cl.viewangles[pnum][0]/-3;
				state->angles[1] = cl.viewangles[pnum][1];
				state->angles[2] = cl.viewangles[pnum][2];
			}


	if (!from || state->modelindex != from->modelindex || state->number != from->number)	//model changed... or entity changed...
		cl.lerpents[state->number].lerptime = -10;
	else if (state->frame != from->frame || state->origin[0] != from->origin[0] || state->origin[1] != from->origin[1] || state->origin[2] != from->origin[2])
	{
		cl.lerpents[state->number].origin[0] = from->origin[0];
		cl.lerpents[state->number].origin[1] = from->origin[1];
		cl.lerpents[state->number].origin[2] = from->origin[2];

		cl.lerpents[state->number].angles[0] = from->angles[0];
		cl.lerpents[state->number].angles[1] = from->angles[1];
		cl.lerpents[state->number].angles[2] = from->angles[2];
//we have three sorts of movement.
//1: stepping monsters. These have frames and tick at 10fps.
//2: physics. Objects moving acording to gravity.
//3: both. This is really awkward. And I'm really lazy.
		cl.lerpents[state->number].lerprate = cl.time-cl.lerpents[state->number].lerptime;	//time per update
		cl.lerpents[state->number].frame = from->frame;
		cl.lerpents[state->number].lerptime = cl.time;

		if (cl.lerpents[state->number].lerprate>0.5)
			cl.lerpents[state->number].lerprate=0.1;

		//store this off for new ents to use.
//		if (new)
//			cl.lerpents[state->number].lerptime = newlerprate;
//		else
		if (state->frame == from->frame)
			newlerprate = cl.time-cl.lerpents[state->number].lerptime;
	}


/*
	if (num == cl.viewentity)
	{
		cl.frames[cls.netchan.incoming_sequence&UPDATE_MASK].playerstate[cl.playernum[0]].velocity[0] = state->origin[0] - cl.frames[cls.netchan.incoming_sequence&UPDATE_MASK].playerstate[cl.playernum[0]].origin[0];
		cl.frames[cls.netchan.incoming_sequence&UPDATE_MASK].playerstate[cl.playernum[0]].velocity[1] = state->origin[1] - cl.frames[cls.netchan.incoming_sequence&UPDATE_MASK].playerstate[cl.playernum[0]].origin[1];
		cl.frames[cls.netchan.incoming_sequence&UPDATE_MASK].playerstate[cl.playernum[0]].velocity[2] = state->origin[2] - cl.frames[cls.netchan.incoming_sequence&UPDATE_MASK].playerstate[cl.playernum[0]].origin[2];
		VectorCopy(state->origin, cl.frames[cls.netchan.incoming_sequence&UPDATE_MASK].playerstate[cl.playernum[0]].origin);
	}*/
}
#endif
#ifdef PEXT_SETVIEW
entity_state_t *CL_FindPacketEntity(int num)
{
	int					pnum;
	entity_state_t		*s1;
	packet_entities_t	*pack;
	pack = &cl.frames[cls.netchan.incoming_sequence&UPDATE_MASK].packet_entities;

	for (pnum=0 ; pnum<pack->num_entities ; pnum++)
	{
		s1 = &pack->entities[pnum];

		if (num == s1->number)
			return s1;
	}
	return NULL;
}
#endif

void CL_RotateAroundTag(entity_t *ent, int num)
{
	entity_state_t *ps;
	float *org=NULL, *ang=NULL;

	ps = CL_FindPacketEntity(cl.lerpents[num].tagent);
	if (ps)
	{
		org = ps->origin;
		ang = ps->angles;
	}
	else
	{
		extern int parsecountmod;
		if (cl.lerpents[num].tagent <= MAX_CLIENTS && cl.lerpents[num].tagent > 0)
		{
			if (cl.lerpents[num].tagent-1 == cl.playernum[0])
			{
				org = cl.simorg[0];
				ang = cl.simangles[0];
			}
			else
			{
				org = cl.frames[parsecountmod].playerstate[cl.lerpents[num].tagent-1].origin;
				ang = cl.frames[parsecountmod].playerstate[cl.lerpents[num].tagent-1].viewangles;
			}
		}
	}

	if (org)
		VectorAdd(ent->origin, org, ent->origin);
	if (ang)
	{
		if (ps)
			ent->angles[0]+=ang[0];
		else
			ent->angles[0]+=-ang[0]/3;
		ent->angles[1]+=ang[1];
		ent->angles[2]+=ang[2];
	}

	ent->keynum = cl.lerpents[num].tagent;
}
/*
===============
CL_LinkPacketEntities

===============
*/
void R_FlameTrail(vec3_t start, vec3_t end, float seperation);
void CL_LinkPacketEntities (void)
{
	entity_t			*ent;
	packet_entities_t	*pack;
	entity_state_t		*s1;
	float				f;
	model_t				*model;
	vec3_t				old_origin;
	float				autorotate;
	int					i;
	int					pnum, spnum;
	dlight_t			*dl;

	pack = &cl.frames[cls.netchan.incoming_sequence&UPDATE_MASK].packet_entities;

	autorotate = anglemod(100*cl.time);

	for (pnum=0 ; pnum<pack->num_entities ; pnum++)
	{
		s1 = &pack->entities[pnum];

		// spawn light flashes, even ones coming from invisible objects
		if ((s1->effects & (EF_BLUE | EF_RED)) == (EF_BLUE | EF_RED))
			CL_NewDlight (s1->number, s1->origin[0], s1->origin[1], s1->origin[2], 200 + (rand()&31), 0.1, 3);
		else if (s1->effects & EF_BLUE)
			CL_NewDlight (s1->number, s1->origin[0], s1->origin[1], s1->origin[2], 200 + (rand()&31), 0.1, 1);
		else if (s1->effects & EF_RED)
			CL_NewDlight (s1->number, s1->origin[0], s1->origin[1], s1->origin[2], 200 + (rand()&31), 0.1, 2);
		else if (s1->effects & EF_BRIGHTLIGHT)
			CL_NewDlight (s1->number, s1->origin[0], s1->origin[1], s1->origin[2] + 16, 400 + (rand()&31), 0.1, 0);
		else if (s1->effects & EF_DIMLIGHT)
			CL_NewDlight (s1->number, s1->origin[0], s1->origin[1], s1->origin[2], 200 + (rand()&31), 0.1, 0);

		// if set to invisible, skip
		if (!s1->modelindex)
			continue;

		for (spnum = 0; spnum < cl.splitclients; spnum++)
		{
			if (s1->number == cl.viewentity[spnum])
			{
	float	a1, a2;
				cl.simvel[spnum][0] = 0;
				cl.simvel[spnum][1] = 0;
				cl.simvel[spnum][2] = 0;


	//			f = 1-(realtime-cl.lerpents[s1->number].lerptime)*10;
				f = 1-(cl.time-cl.lerpents[s1->number].lerptime)/cl.lerpents[s1->number].lerprate;
				if (f<0)f=0;
				if (f>1)f=1;

				for (i=0 ; i<3 ; i++)
					cl.simorg[spnum][i] = r_refdef.vieworg[i] = s1->origin[i] + 
							f * (cl.lerpents[s1->number].origin[i] - s1->origin[i]);

				for (i=0 ; i<3 ; i++)
				{
					a1 = cl.lerpents[s1->number].angles[i];
					a2 = s1->angles[i];
					if (a1 - a2 > 180)
						a1 -= 360;
					if (a1 - a2 < -180)
						a1 += 360;
					cl.simangles[spnum][i] = a2 + f * (a1 - a2);
				}
				cl.simangles[spnum][0] = cl.simangles[spnum][0]*-3;

				/*for (i=0 ; i<3 ; i++)
				{
					a1 = cl.lerpents[s1->number].angles[i];
					a2 = s1->angles[i];
					if (a1 - a2 > 180)
						a1 -= 360;
					if (a1 - a2 < -180)
						a1 += 360;
					cl.simangles[i] = a2 + f * (a1 - a2);
				}*/
			}
		}

		// create a new entity
		if (cl_numvisedicts == MAX_VISEDICTS)
			break;		// object list is full

		model = cl.model_precache[s1->modelindex];
		if (!model)
		{
			Con_DPrintf("Bad modelindex\n");
			continue;
		}

		/*if (qrenderer == QR_OPENGL && model->type == mod_sprite)	//more efficient strcmping - there arn't that many sprites.
		{
			if (gl_part_inferno.value && (model->numframes == 6 || gl_part_inferno.value==2) && !strcmp(model->name, "progs/s_explod.spr"))
			{
				VectorCopy (s1->origin, old_origin);
				for (i=0 ; i<cl_oldnumvisedicts ; i++)
				{
					if (cl_oldvisedicts[i].keynum == s1->number)
					{
						VectorCopy (cl_oldvisedicts[i].origin, old_origin);
						break;
					}
				}

				for (i=0 ; i<3 ; i++)
					if ( abs(old_origin[i] - s1->origin[i]) > 128)
					{	// no trail if too far
						VectorCopy (s1->origin, old_origin);
						break;
					}

				
				R_FlameTrail(old_origin, s1->origin, (float)s1->frame/model->numframes);
				model = NULL;
			}
		}*/
 
		ent = &cl_visedicts[cl_numvisedicts];
		cl_numvisedicts++;
		ent->visframe = 0;

		ent->keynum = s1->number;
		ent->model = model;//Mod_ForName("progs/tris.md2", true);//model;

		ent->flags = 0;

		// set colormap
		if (s1->colormap && (s1->colormap <= MAX_CLIENTS) 
			&& (gl_nocolors.value == -1 || (ent->model && s1->modelindex == cl_playerindex)))
		{
			ent->colormap = cl.players[s1->colormap-1].translations;
			ent->scoreboard = &cl.players[s1->colormap-1];
		}
		else
		{
			ent->colormap = vid.colormap;
			ent->scoreboard = NULL;
		}

		// set skin
		ent->skinnum = s1->skinnum;

		ent->abslight = s1->abslight;
		ent->drawflags = s1->drawflags;

		// set frame
		ent->frame = s1->frame;
		ent->oldframe = cl.lerpents[s1->number].frame;

		if (!cl.lerpents[s1->number].lerprate)
		{
			ent->lerptime = 0;
		}
		else
		{
			ent->lerptime = 1-(cl.time-cl.lerpents[s1->number].lerptime)/cl.lerpents[s1->number].lerprate;
		}
			
		if (ent->lerptime<0)ent->lerptime=0;
		if (ent->lerptime>1)ent->lerptime=1;

		f = ent->lerptime;

//		f = (sin(realtime)+1)/2;

#ifdef PEXT_SCALE
		//set scale
		ent->scale = s1->scale;
		if (!ent->scale)
			ent->scale=1;
#endif
#ifdef PEXT_TRANS
		//set trans
		ent->alpha = s1->trans;
		if (!ent->alpha)
			ent->alpha=1;
#endif
#ifdef PEXT_FATNESS
		//set trans
		ent->fatness = s1->fatness;
#endif

		// calculate origin
		for (i=0 ; i<3 ; i++)
			ent->origin[i] = s1->origin[i] + 
			f * (cl.lerpents[s1->number].origin[i] - s1->origin[i]);

		// rotate binary objects locally
		if (model && model->flags & EF_ROTATE)
		{
			ent->angles[0] = 0;
			ent->angles[1] = autorotate;
			ent->angles[2] = 0;

			if (cl_item_bobbing.value)
				ent->origin[2] += 5+sin(cl.time*3)*5;	//don't let it into the ground
		}
		else
		{
			float	a1, a2;

			for (i=0 ; i<3 ; i++)
			{
				a1 = cl.lerpents[s1->number].angles[i];
				a2 = s1->angles[i];
				if (a1 - a2 > 180)
					a1 -= 360;
				if (a1 - a2 < -180)
					a1 += 360;
				ent->angles[i] = a2 + f * (a1 - a2);
			}
		}

		if (cl.lerpents[s1->number].tagent)
		{	//ent is attached to a tag, rotate this ent accordingly.
			CL_RotateAroundTag(ent, s1->number);
		}

		// add automatic particle trails
		if (!model || (!(model->flags&~EF_ROTATE) && model->particletrail<0))
			continue;

		if (!cls.allow_anyparticles && !(model->flags & ~EF_ROTATE))
			continue;

		// scan the old entity display list for a matching
		for (i=0 ; i<cl_oldnumvisedicts ; i++)
		{
			if (cl_oldvisedicts[i].keynum == ent->keynum)
			{
				VectorCopy (cl_oldvisedicts[i].origin, old_origin);
				break;
			}
		}
		if (i == cl_oldnumvisedicts)
		{
			cl.lerpents[s1->number].trailstate.lastdist = 0;
			continue;		// not in last message
		}

		for (i=0 ; i<3 ; i++)
			if ( abs(old_origin[i] - ent->origin[i]) > 128)
			{	// no trail if too far
				VectorCopy (ent->origin, old_origin);
				break;
			}

		if (model->particletrail>=0)
			R_RocketTrail (old_origin, ent->origin, model->particletrail, &cl.lerpents[s1->number].trailstate);

		//dlights are not customisable.
		if (model->flags & EF_ROCKET)
		{
			if (strncmp(model->name, "models/sflesh", 13))
			{	//hmm. hexen spider gibs...
				dl = CL_AllocDlight (s1->number);
				VectorCopy (ent->origin, dl->origin);
				dl->radius = 200;
				dl->die = cl.time + 0.1;
				dl->color[0] = 0.20;
				dl->color[1] = 0.1;
				dl->color[2] = 0.05;
			}
		}
		else if (model->flags & EF_FIREBALL)
		{
			dl = CL_AllocDlight (i);
			VectorCopy (ent->origin, dl->origin);
			dl->radius = 120 - (rand() % 20);
			dl->die = cl.time + 0.01;
		}
		else if (model->flags & EF_ACIDBALL)
		{
			dl = CL_AllocDlight (i);
			VectorCopy (ent->origin, dl->origin);
			dl->radius = 120 - (rand() % 20);
			dl->die = cl.time + 0.01;
		}
		else if (model->flags & EF_SPIT)
		{
			dl = CL_AllocDlight (i);
			VectorCopy (ent->origin, dl->origin);
			dl->radius = -120 - (rand() % 20);
			dl->die = cl.time + 0.05;
		}
	}
}


/*
=========================================================================

PROJECTILE PARSING / LINKING

=========================================================================
*/

typedef struct
{
	int		modelindex;
	vec3_t	origin;
	vec3_t	angles;
} projectile_t;

#define	MAX_PROJECTILES	32
projectile_t	cl_projectiles[MAX_PROJECTILES];
int				cl_num_projectiles;

extern int cl_spikeindex;

void CL_ClearProjectiles (void)
{
	cl_num_projectiles = 0;
}

/*
=====================
CL_ParseProjectiles

Nails are passed as efficient temporary entities
=====================
*/
void CL_ParseProjectiles (int modelindex)
{
	int		i, c, j;
	qbyte	bits[6];
	projectile_t	*pr;

	c = MSG_ReadByte ();
	for (i=0 ; i<c ; i++)
	{
		for (j=0 ; j<6 ; j++)
			bits[j] = MSG_ReadByte ();

		if (cl_num_projectiles == MAX_PROJECTILES)
			continue;

		pr = &cl_projectiles[cl_num_projectiles];
		cl_num_projectiles++;

		pr->modelindex = modelindex;
		pr->origin[0] = ( ( bits[0] + ((bits[1]&15)<<8) ) <<1) - 4096;
		pr->origin[1] = ( ( (bits[1]>>4) + (bits[2]<<4) ) <<1) - 4096;
		pr->origin[2] = ( ( bits[3] + ((bits[4]&15)<<8) ) <<1) - 4096;
		pr->angles[0] = 360*(bits[4]>>4)/16;
		pr->angles[1] = 360*bits[5]/256;
	}
}

/*
=============
CL_LinkProjectiles

=============
*/
void CL_LinkProjectiles (void)
{
	int		i;
	projectile_t	*pr;
	entity_t		*ent;

	for (i=0, pr=cl_projectiles ; i<cl_num_projectiles ; i++, pr++)
	{
		// grab an entity to fill in
		if (cl_numvisedicts == MAX_VISEDICTS)
			break;		// object list is full
		ent = &cl_visedicts[cl_numvisedicts];
		cl_numvisedicts++;
		ent->keynum = 0;

		if (pr->modelindex < 1)
			continue;
		ent->model = cl.model_precache[pr->modelindex];
		ent->skinnum = 0;
		ent->frame = 0;
		ent->colormap = vid.colormap;
		ent->scoreboard = NULL;
#ifdef PEXT_SCALE
		ent->scale = 1;
#endif
#ifdef PEXT_TRANS
		ent->alpha = 1;
#endif
		VectorCopy (pr->origin, ent->origin);
		VectorCopy (pr->angles, ent->angles);
	}
}

//========================================

extern	int		cl_spikeindex, cl_playerindex, cl_flagindex;

entity_t *CL_NewTempEntity (void);



#define DF_ORIGIN	1
#define DF_ANGLES		(1<<3)
#define DF_EFFECTS		(1<<6)
#define DF_SKINNUM		(1<<7)
#define DF_DEAD			(1<<8)
#define DF_GIB			(1<<9)
#define DF_WEAPONFRAME	(1<<10)
#define DF_MODEL		(1<<11)
static int MVD_TranslateFlags(int src)
{
	int dst = 0;

	if (src & DF_EFFECTS)
		dst |= PF_EFFECTS;
	if (src & DF_SKINNUM)
		dst |= PF_SKINNUM;
	if (src & DF_DEAD)
		dst |= PF_DEAD;
	if (src & DF_GIB)
		dst |= PF_GIB;
	if (src & DF_WEAPONFRAME)
		dst |= PF_WEAPONFRAME;
	if (src & DF_MODEL)
		dst |= PF_MODEL;

	return dst;
}

/*
===================
CL_ParsePlayerinfo
===================
*/
extern int parsecountmod;
extern double parsecounttime;
int lastplayerinfo;
void CL_ParsePlayerinfo (void)
{
	int			msec;
	unsigned int			flags;
	player_info_t	*info;
	player_state_t	*state;
	int			num;
	int			i;
	int new;
	vec3_t		org;

	lastplayerinfo = num = MSG_ReadByte ();
	if (num >= MAX_CLIENTS)
		Host_EndGame ("CL_ParsePlayerinfo: bad num");

	info = &cl.players[num];

	state = &cl.frames[parsecountmod].playerstate[num];

	if (cls.demoplayback == DPB_MVD)
	{
		player_state_t	*prevstate, dummy;
		if (!cl.parsecount || info->prevcount > cl.parsecount || cl.parsecount - info->prevcount >= UPDATE_BACKUP - 1)
		{
			memset(&dummy, 0, sizeof(dummy));
			prevstate = &dummy;
		}
		else
		{
			prevstate = &cl.frames[info->prevcount & UPDATE_MASK].playerstate[num];
		}
		memcpy(state, prevstate, sizeof(player_state_t));
		info->prevcount = cl.parsecount;

/*		if (cls.findtrack && info->stats[STAT_HEALTH] > 0)
		{
			extern int ideal_track;
			autocam = CAM_TRACK;
			Cam_Lock(num);
			ideal_track = num;
			cls.findtrack = false;
		}
*/
		flags = MSG_ReadShort ();
		state->flags = MVD_TranslateFlags(flags);

		state->messagenum = cl.parsecount;
		state->command.msec = 0;

		state->frame = MSG_ReadByte ();

		state->state_time = parsecounttime;
		state->command.msec = 0;

		for (i = 0; i < 3; i++)
		{
			if (flags & (DF_ORIGIN << i))
				state->origin[i] = MSG_ReadCoord ();
		}

		for (i = 0; i < 3; i++)
		{
			if (flags & (DF_ANGLES << i))
			{
				state->command.angles[i] = MSG_ReadShort();
			}
			state->viewangles[i] = state->command.angles[i] * (360.0/65536);
		}

		if (flags & DF_MODEL)
			state->modelindex = MSG_ReadByte ();

		if (flags & DF_SKINNUM)
			state->skinnum = MSG_ReadByte ();

		if (flags & DF_EFFECTS)
			state->effects = MSG_ReadByte ();

		if (flags & DF_WEAPONFRAME)
			state->weaponframe = MSG_ReadByte ();

		state->hullnum = 1;
		state->scale = 1;
		state->trans = 1;
		state->fatness = 0;

		state->pm_type = PM_NORMAL;

		return;
	}

	flags = state->flags = (unsigned short)MSG_ReadShort ();

	if (cls.z_ext & Z_EXT_PM_TYPE)
		if (flags & PF_EXTRA_PFS)
			flags |= MSG_ReadByte()<<16;

	state->messagenum = cl.parsecount;
	org[0] = MSG_ReadCoord ();
	org[1] = MSG_ReadCoord ();
	org[2] = MSG_ReadCoord ();

	VectorCopy(org, state->origin);

	new = MSG_ReadByte ();
	if (state->frame != new)
	{
//		state->lerpstarttime = realtime;
		state->frame = new;
	}

	// the other player's last move was likely some time
	// before the packet was sent out, so accurately track
	// the exact time it was valid at
	if (flags & PF_MSEC)
	{
		msec = MSG_ReadByte ();
		state->state_time = parsecounttime - msec*0.001;
	}
	else
		state->state_time = parsecounttime;

	if (flags & PF_COMMAND)
	{
		MSG_ReadDeltaUsercmd (&nullcmd, &state->command);

		state->viewangles[0] = state->command.angles[0] * (360.0/65536);
		state->viewangles[1] = state->command.angles[1] * (360.0/65536);
		state->viewangles[2] = state->command.angles[2] * (360.0/65536);
	}

	for (i=0 ; i<3 ; i++)
	{
		if (flags & (PF_VELOCITY1<<i) )
			state->velocity[i] = MSG_ReadShort();
		else
			state->velocity[i] = 0;
	}
	if (flags & PF_MODEL)
		state->modelindex = MSG_ReadByte ();
	else
		state->modelindex = cl_playerindex;

	if (flags & PF_SKINNUM)
	{
		state->skinnum = MSG_ReadByte ();
		if (state->skinnum & (1<<7) && (flags & PF_MODEL))
		{
			state->modelindex+=256;
			state->skinnum -= (1<<7);
		}
	}
	else
		state->skinnum = 0;

	if (flags & PF_EFFECTS)
		state->effects = MSG_ReadByte ();
	else
		state->effects = 0;

	if (flags & PF_WEAPONFRAME)
		state->weaponframe = MSG_ReadByte ();
	else
		state->weaponframe = 0;

	state->hullnum = 1;
	state->scale = 1;
	state->trans = 1;
	state->fatness = 0;

	if (cls.z_ext & Z_EXT_PM_TYPE)
	{
		int pm_code;

#ifdef PEXT_SCALE
		if (flags & PF_SCALE_Z && cls.fteprotocolextensions & PEXT_SCALE)
			state->scale = (float)MSG_ReadByte() / 100;
#endif
#ifdef PEXT_TRANS
		if (flags & PF_TRANS_Z && cls.fteprotocolextensions & PEXT_TRANS)
			state->trans = (float)MSG_ReadByte() / 255;
#endif
#ifdef PEXT_FATNESS
		if (flags & PF_FATNESS_Z && cls.fteprotocolextensions & PEXT_FATNESS)
			state->fatness = (float)MSG_ReadChar() / 2;
#endif
#ifdef PEXT_HULLSIZE
		if (cls.fteprotocolextensions & PEXT_HULLSIZE)
		{
			if (flags & PF_HULLSIZE_Z)
				state->hullnum = MSG_ReadByte();
		}
		else
			state->hullnum = 1;
	//should be passed to player move func.
#endif

		pm_code = (flags&PF_PMC_MASK) >> PF_PMC_SHIFT;
		if (pm_code == PMC_NORMAL || pm_code == PMC_NORMAL_JUMP_HELD)
		{
			if (flags & PF_DEAD)
				state->pm_type = PM_DEAD;
			else
			{
				state->pm_type = PM_NORMAL;
				state->jump_held = (pm_code == PMC_NORMAL_JUMP_HELD);
			}
		}
		else if (pm_code == PMC_OLD_SPECTATOR)
			state->pm_type = PM_OLD_SPECTATOR;
		else
		{
			if (cls.z_ext & Z_EXT_PM_TYPE_NEW)
			{
				if (pm_code == PMC_SPECTATOR)
					state->pm_type = PM_SPECTATOR;
				else if (pm_code == PMC_FLY)
					state->pm_type = PM_FLY;
				else if (pm_code == PMC_NONE)
					state->pm_type = PM_NONE;
				else if (pm_code == PMC_FREEZE)
					state->pm_type = PM_FREEZE;
				else {
					// future extension?
					goto guess_pm_type;
				}
			}
			else
			{
				// future extension?
				goto guess_pm_type;
			}
		}
	}
	else
	{
#ifdef PEXT_SCALE
		if (flags & PF_SCALE_NOZ && cls.fteprotocolextensions & PEXT_SCALE)
			state->scale = (float)MSG_ReadByte() / 100;
#endif
#ifdef PEXT_TRANS
		if (flags & PF_TRANS_NOZ && cls.fteprotocolextensions & PEXT_TRANS)
			state->trans = (float)MSG_ReadByte() / 255;
#endif
#ifdef PEXT_FATNESS
		if (flags & PF_FATNESS_NOZ && cls.fteprotocolextensions & PEXT_FATNESS)
			state->fatness = (float)MSG_ReadChar() / 2;
#endif
#ifdef PEXT_HULLSIZE
		if (flags & PF_HULLSIZE_NOZ && cls.fteprotocolextensions & PEXT_HULLSIZE)
			state->hullnum = MSG_ReadByte();
		//should be passed to player move func.
#endif

guess_pm_type:
		if (cl.players[num].spectator)
			state->pm_type = PM_OLD_SPECTATOR;
		else if (flags & PF_DEAD)
			state->pm_type = PM_DEAD;
		else
			state->pm_type = PM_NORMAL;
	}

}

void CL_ParseClientPersist(void)
{
	player_info_t	*info;
	int flags;
	flags = MSG_ReadShort();
	info = &cl.players[lastplayerinfo];
	if (flags & 1)
		info->vweapindex = MSG_ReadShort();
}


/*
================
CL_AddFlagModels

Called when the CTF flags are set
================
*/
void CL_AddFlagModels (entity_t *ent, int team)
{
	int		i;
	float	f;
	vec3_t	v_forward, v_right, v_up;
	entity_t	*newent;

	if (cl_flagindex == -1)
		return;

	f = 14;
	if (ent->frame >= 29 && ent->frame <= 40) {
		if (ent->frame >= 29 && ent->frame <= 34) { //axpain
			if      (ent->frame == 29) f = f + 2; 
			else if (ent->frame == 30) f = f + 8;
			else if (ent->frame == 31) f = f + 12;
			else if (ent->frame == 32) f = f + 11;
			else if (ent->frame == 33) f = f + 10;
			else if (ent->frame == 34) f = f + 4;
		} else if (ent->frame >= 35 && ent->frame <= 40) { // pain
			if      (ent->frame == 35) f = f + 2; 
			else if (ent->frame == 36) f = f + 10;
			else if (ent->frame == 37) f = f + 10;
			else if (ent->frame == 38) f = f + 8;
			else if (ent->frame == 39) f = f + 4;
			else if (ent->frame == 40) f = f + 2;
		}
	} else if (ent->frame >= 103 && ent->frame <= 118) {
		if      (ent->frame >= 103 && ent->frame <= 104) f = f + 6;  //nailattack
		else if (ent->frame >= 105 && ent->frame <= 106) f = f + 6;  //light 
		else if (ent->frame >= 107 && ent->frame <= 112) f = f + 7;  //rocketattack
		else if (ent->frame >= 112 && ent->frame <= 118) f = f + 7;  //shotattack
	}

	newent = CL_NewTempEntity ();
	newent->model = cl.model_precache[cl_flagindex];
	newent->skinnum = team;

	AngleVectors (ent->angles, v_forward, v_right, v_up);
	v_forward[2] = -v_forward[2]; // reverse z component
	for (i=0 ; i<3 ; i++)
		newent->origin[i] = ent->origin[i] - f*v_forward[i] + 22*v_right[i];
	newent->origin[2] -= 16;

	VectorCopy (ent->angles, newent->angles)
	newent->angles[2] -= 45;
}

void CL_AddVWeapModel(entity_t *player, int model)
{
	entity_t	*newent;
	newent = CL_NewTempEntity ();

	VectorCopy(player->origin, newent->origin);
	VectorCopy(player->angles, newent->angles);
	newent->skinnum = player->skinnum;
	newent->model = cl.model_precache[model];
	newent->frame = player->frame;
}

void CL_ParseAttachment(void)
{
	int e = (unsigned short)MSG_ReadShort();
	int o = (unsigned short)MSG_ReadShort();
	int i = (unsigned short)MSG_ReadShort();
	cl.lerpents[e].tagent = o;
	cl.lerpents[e].tagindex = i;
}

/*
=============
CL_LinkPlayers

Create visible entities in the correct position
for all current players
=============
*/
void CL_LinkPlayers (void)
{
	int pnum;
	int				j;
	player_info_t	*info;
	player_state_t	*state;
	player_state_t	exact;
	double			playertime;
	entity_t		*ent;
	int				msec;
	frame_t			*frame;
	int				oldphysent;

	playertime = realtime - cls.latency + 0.02;
	if (playertime > realtime)
		playertime = realtime;

	frame = &cl.frames[cl.parsecount&UPDATE_MASK];

	for (j=0, info=cl.players, state=frame->playerstate ; j < MAX_CLIENTS 
		; j++, info++, state++)
	{
		if (state->messagenum != cl.parsecount)
			continue;	// not present this frame

		// spawn light flashes, even ones coming from invisible objects
		if (!r_flashblend.value || j != cl.playernum[0])
		{
			if ((state->effects & (EF_BLUE | EF_RED)) == (EF_BLUE | EF_RED))
				CL_NewDlight (j+1, state->origin[0], state->origin[1], state->origin[2], 200 + (rand()&31), 0.1, 3)->noppl = (j != cl.playernum[0]);
			else if (state->effects & EF_BLUE)
				CL_NewDlight (j+1, state->origin[0], state->origin[1], state->origin[2], 200 + (rand()&31), 0.1, 1)->noppl = (j != cl.playernum[0]);
			else if (state->effects & EF_RED)
				CL_NewDlight (j+1, state->origin[0], state->origin[1], state->origin[2], 200 + (rand()&31), 0.1, 2)->noppl = (j != cl.playernum[0]);
			else if (state->effects & EF_BRIGHTLIGHT)
				CL_NewDlight (j+1, state->origin[0], state->origin[1], state->origin[2] + 16, 400 + (rand()&31), 0.1, 0)->noppl = (j != cl.playernum[0]);
			else if (state->effects & EF_DIMLIGHT)
				CL_NewDlight (j+1, state->origin[0], state->origin[1], state->origin[2], 200 + (rand()&31), 0.1, 0)->noppl = (j != cl.playernum[0]);
		}

		if (!state->modelindex)
			continue;
/*
		if (!Cam_DrawPlayer(j))
			continue;
*/
		// grab an entity to fill in
		if (cl_numvisedicts == MAX_VISEDICTS)
			break;		// object list is full
		ent = &cl_visedicts[cl_numvisedicts];
		cl_numvisedicts++;
		ent->keynum = j+1;
		ent->flags = 0;

		ent->model = cl.model_precache[state->modelindex];
		ent->skinnum = state->skinnum;		

		ent->frame = state->frame;
		ent->oldframe = state->oldframe;
		if (state->lerpstarttime)
		{
			ent->lerptime = 1-(realtime - state->lerpstarttime)*10;
			if (ent->lerptime < 0)
				ent->lerptime = 0;
		}
		else
			ent->lerptime = 0;

		ent->colormap = info->translations;
		if (state->modelindex == cl_playerindex)
			ent->scoreboard = info;		// use custom skin
		else
			ent->scoreboard = NULL;

#ifdef PEXT_SCALE
		ent->scale = state->scale;
		if (!ent->scale)
			ent->scale = 1;
#endif
#ifdef PEXT_TRANS
		ent->alpha = state->trans;
		if (!ent->alpha)
			ent->alpha = 1;
#endif

		//
		// angles
		//
		ent->angles[PITCH] = -state->viewangles[PITCH]/3;
		ent->angles[YAW] = state->viewangles[YAW];
		ent->angles[ROLL] = 0;
		ent->angles[ROLL] = V_CalcRoll (ent->angles, state->velocity)*4;

		// the player object gets added with flags | 2
		for (pnum = 0; pnum < cl.splitclients; pnum++)
		{
			if (j == cl.playernum[pnum])
			{
				if (cl.spectator)
				{
					cl_numvisedicts--;
					continue;
				}
				ent->angles[0] = -1*cl.viewangles[pnum][0] / 3;
				ent->angles[1] = cl.viewangles[pnum][1];
				ent->angles[2] = cl.viewangles[pnum][2];
				ent->origin[0] = cl.simorg[pnum][0];
				ent->origin[1] = cl.simorg[pnum][1];
				ent->origin[2] = cl.simorg[pnum][2]+cl.crouch[pnum];
				ent->flags |= 2;
				break;
			}
		}

		// only predict half the move to minimize overruns
		msec = 500*(playertime - state->state_time);
		if (pnum < cl.splitclients)
		{
		}
		else if (msec <= 0 || (!cl_predict_players.value && !cl_predict_players2.value))
		{
			VectorCopy (state->origin, ent->origin);
//Con_DPrintf ("nopredict\n");
		}
		else
		{
			// predict players movement
			if (msec > 255)
				msec = 255;
			state->command.msec = msec;
//Con_DPrintf ("predict: %i\n", msec);

			oldphysent = pmove.numphysent;
			CL_SetSolidPlayers (j);
			CL_PredictUsercmd (0, state, &exact, &state->command);	//uses player 0's maxspeed/grav...
			pmove.numphysent = oldphysent;
			VectorCopy (exact.origin, ent->origin);
		}
		if (cl.worldmodel->fromgame == fg_halflife)
			ent->origin[2]-=12;

		if (state->effects & EF_FLAG1)
			CL_AddFlagModels (ent, 0);
		else if (state->effects & EF_FLAG2)
			CL_AddFlagModels (ent, 1);
		else if (info->vweapindex)
			CL_AddVWeapModel (ent, info->vweapindex);

	}
}

//======================================================================

/*
===============
CL_SetSolid

Builds all the pmove physents for the current frame
===============
*/
void CL_SetSolidEntities (void)
{
	int		i;
	frame_t	*frame;
	packet_entities_t	*pak;
	entity_state_t		*state;

	pmove.physents[0].model = cl.worldmodel;
	VectorCopy (vec3_origin, pmove.physents[0].origin);
	pmove.physents[0].info = 0;
	pmove.numphysent = 1;

	frame = &cl.frames[parsecountmod];
	pak = &frame->packet_entities;

	for (i=0 ; i<pak->num_entities ; i++)
	{
		state = &pak->entities[i];

		if (!state->modelindex)
			continue;
		if (!cl.model_precache[state->modelindex])
			continue;
		if (*cl.model_precache[state->modelindex]->name == '*' || cl.model_precache[state->modelindex]->numsubmodels)
		if ( cl.model_precache[state->modelindex]->hulls[1].firstclipnode 
			|| cl.model_precache[state->modelindex]->clipbox )
		{
			pmove.physents[pmove.numphysent].model = cl.model_precache[state->modelindex];
			VectorCopy (state->origin, pmove.physents[pmove.numphysent].origin);
			VectorCopy (state->angles, pmove.physents[pmove.numphysent].angles);
			pmove.physents[pmove.numphysent].angles[0]*=-1;
			if (++pmove.numphysent == MAX_PHYSENTS)
				break;
		}
	}

}

/*
===
Calculate the new position of players, without other player clipping

We do this to set up real player prediction.
Players are predicted twice, first without clipping other players,
then with clipping against them.
This sets up the first phase.
===
*/
void CL_SetUpPlayerPrediction(qboolean dopred)
{
	int				j;
	player_state_t	*state;
	player_state_t	exact;
	double			playertime;
	int				msec;
	frame_t			*frame;
	struct predicted_player *pplayer;
	extern cvar_t cl_nopred;

	playertime = realtime - cls.latency + 0.02;
	if (playertime > realtime)
		playertime = realtime;

	if (cl_nopred.value)
		return;

	frame = &cl.frames[cl.parsecount&UPDATE_MASK];

	for (j=0, pplayer = predicted_players, state=frame->playerstate; 
		j < MAX_CLIENTS;
		j++, pplayer++, state++)
	{

		pplayer->active = false;

		if (state->messagenum != cl.parsecount)
			continue;	// not present this frame

		if (!state->modelindex)
			continue;

		pplayer->active = true;
		pplayer->flags = state->flags;

		if (pplayer->frame != state->frame)
		{
			state->oldframe = pplayer->oldframe = pplayer->frame;
			state->lerpstarttime = pplayer->lerptime = realtime;
			pplayer->frame = state->frame;
		}
		else
		{
			state->lerpstarttime = pplayer->lerptime;
			state->oldframe = pplayer->oldframe;
		}

		// note that the local player is special, since he moves locally
		// we use his last predicted postition
		if (j == cl.playernum[0])
		{
			VectorCopy(cl.frames[cls.netchan.outgoing_sequence&UPDATE_MASK].playerstate[cl.playernum[0]].origin,
				pplayer->origin);
		}
		else
		{
			// only predict half the move to minimize overruns
			msec = 500*(playertime - state->state_time);
			if (msec <= 0 ||
				(!cl_predict_players.value && !cl_predict_players2.value) ||
				!dopred)
			{
				VectorCopy (state->origin, pplayer->origin);
	//Con_DPrintf ("nopredict\n");
			}
			else
			{
				// predict players movement
				if (msec > 255)
					msec = 255;
				state->command.msec = msec;
	//Con_DPrintf ("predict: %i\n", msec);

				CL_PredictUsercmd (0, state, &exact, &state->command);
				VectorCopy (exact.origin, pplayer->origin);
			}

			if (cl.spectator)
			{
				if (!Cam_DrawPlayer(0, j))
					VectorCopy(pplayer->origin, cl.simorg[0]);
				
			}
		}
	}
}

/*
===============
CL_SetSolid

Builds all the pmove physents for the current frame
Note that CL_SetUpPlayerPrediction() must be called first!
pmove must be setup with world and solid entity hulls before calling
(via CL_PredictMove)
===============
*/
void CL_SetSolidPlayers (int playernum)
{
	int		j;
	extern	vec3_t	player_mins;
	extern	vec3_t	player_maxs;
	struct predicted_player *pplayer;
	physent_t *pent;

	if (!cl_solid_players.value)
		return;

	pent = pmove.physents + pmove.numphysent;

	if (pmove.numphysent == MAX_PHYSENTS)	//too many.
		return;

	for (j=0, pplayer = predicted_players; j < MAX_CLIENTS;	j++, pplayer++) {

		if (!pplayer->active)
			continue;	// not present this frame

		// the player object never gets added
		if (j == playernum)
			continue;

		if (pplayer->flags & PF_DEAD)
			continue; // dead players aren't solid

		pent->model = 0;
		VectorCopy(pplayer->origin, pent->origin);
		pent->angles[0] = pent->angles[1] = pent->angles[2] = 0;	//don't bother rotating - only useful with bsps
		VectorCopy(player_mins, pent->mins);
		VectorCopy(player_maxs, pent->maxs);
		if (++pmove.numphysent == MAX_PHYSENTS)	//we just hit 88 miles per hour.
			break;
		pent++;
	}
}

/*
===============
CL_EmitEntities

Builds the visedicts array for cl.time

Made up of: clients, packet_entities, nails, and tents
===============
*/
void CL_EmitEntities (void)
{
	if (cls.state != ca_active)
		return;
#ifdef Q2CLIENT
	if (cls.q2server)
	{
		CLQ2_AddEntities();
		return;
	}
#endif
	if (!cl.validsequence)
		return;

	cl_oldnumvisedicts = cl_numvisedicts;
	cl_oldvisedicts = cl_visedicts;
	if (cl_visedicts == cl_visedicts_list[0])
		cl_visedicts = cl_visedicts_list[1];
	else
		cl_visedicts = cl_visedicts_list[0];
//	cl_oldvisedicts = cl_visedicts_list[(cls.netchan.incoming_sequence-1)&1];
//	cl_visedicts = cl_visedicts_list[cls.netchan.incoming_sequence&1];

	cl_numvisedicts = 0;

	CL_LinkPlayers ();
	CL_LinkPacketEntities ();
	CL_LinkProjectiles ();
	CL_UpdateTEnts ();
}












void MVD_Interpolate(void)
{
	cls.netchan.outgoing_sequence = cl.parsecount+1;
}

		/*

int	mvd_fixangle;

static float MVD_AdjustAngle(float current, float ideal, float fraction) {
	float move;

	move = ideal - current;
	if (move >= 180)
		move -= 360;
	else if (move <= -180)
		move += 360;

	return current + fraction * move;
}

static void MVD_InitInterpolation(void) {
	player_state_t *state, *oldstate;
	int i, tracknum;
	frame_t	*frame, *oldframe;
	vec3_t dist;
	struct predicted_player *pplayer;

	if (!cl.validsequence)
		 return;

//	if (nextdemotime <= olddemotime)
//		return;

	frame = &cl.frames[cl.parsecount & UPDATE_MASK];
	oldframe = &cl.frames[(cl.parsecount-1) & UPDATE_MASK];

	// clients
	for (i = 0; i < MAX_CLIENTS; i++) {
		pplayer = &predicted_players[i];
		state = &frame->playerstate[i];
		oldstate = &oldframe->playerstate[i];

		if (pplayer->predict) {
			VectorCopy(pplayer->oldo, oldstate->origin);
			VectorCopy(pplayer->olda, oldstate->command.angles);
			VectorCopy(pplayer->oldv, oldstate->velocity);
		}

		pplayer->predict = false;

		tracknum = Cam_TrackNum();
		if ((mvd_fixangle & 1) << i) {
			if (i == tracknum) {
				VectorCopy(cl.viewangles, state->command.angles);
				VectorCopy(cl.viewangles, state->viewangles);
			}

			// no angle interpolation
			VectorCopy(state->command.angles, oldstate->command.angles);

			mvd_fixangle &= ~(1 << i);
		}

		// we dont interpolate ourself if we are spectating
		if (i == cl.playernum && cl.spectator)
			continue;

		memset(state->velocity, 0, sizeof(state->velocity));

		if (state->messagenum != cl.parsecount)
			continue;	// not present this frame

		if (oldstate->messagenum != cl.oldparsecount || !oldstate->messagenum)
			continue;	// not present last frame

		if (!ISDEAD(state->frame) && ISDEAD(oldstate->frame))
			continue;

		VectorSubtract(state->origin, oldstate->origin, dist);
		if (DotProduct(dist, dist) > 22500)
			continue;

		VectorScale(dist, 1 / (nextdemotime - olddemotime), pplayer->oldv);

		VectorCopy(state->origin, pplayer->oldo);
		VectorCopy(state->command.angles, pplayer->olda);

		pplayer->oldstate = oldstate;
		pplayer->predict = true;
	}

	// nails
	for (i = 0; i < cl_num_projectiles; i++) {
		if (!cl.int_projectiles[i].interpolate)
			continue;

		VectorCopy(cl.int_projectiles[i].origin, cl_projectiles[i].origin);
	}
}

void MVD_Interpolate(void)
{
	int i, j;
	float f;
	frame_t	*frame, *oldframe;
	player_state_t *state, *oldstate, *self, *oldself;
	entity_state_t *oldents;
	struct predicted_player *pplayer;
	static float old;

	self = &cl.frames[cl.parsecount & UPDATE_MASK].playerstate[cl.playernum];
	oldself = &cl.frames[(cls.netchan.outgoing_sequence - 1) & UPDATE_MASK].playerstate[cl.playernum];

	self->messagenum = cl.parsecount;

	VectorCopy(oldself->origin, self->origin);
	VectorCopy(oldself->velocity, self->velocity);
	VectorCopy(oldself->viewangles, self->viewangles);

	if (old != nextdemotime) {
		old = nextdemotime;
		MVD_InitInterpolation();
	}

	CL_ParseClientdata();

	cls.netchan.outgoing_sequence = cl.parsecount + 1;

	if (!cl.validsequence)
		return;

	if (nextdemotime <= olddemotime)
		return;

	frame = &cl.frames[cl.parsecount & UPDATE_MASK];
	oldframe = &cl.frames[cl.oldparsecount & UPDATE_MASK];
	oldents = oldframe->packet_entities.entities;

	f = bound(0, (cls.demotime - olddemotime) / (nextdemotime - olddemotime), 1);

	// interpolate nails
	for (i = 0; i < cl_num_projectiles; i++)	{
		if (!cl.int_projectiles[i].interpolate)
			continue;

		for (j = 0; j < 3; j++) {
			cl_projectiles[i].origin[j] = cl_oldprojectiles[cl.int_projectiles[i].oldindex].origin[j] +
				f * (cl.int_projectiles[i].origin[j] - cl_oldprojectiles[cl.int_projectiles[i].oldindex].origin[j]);
		}
	}

	// interpolate clients
	for (i = 0; i < MAX_CLIENTS; i++) {
		pplayer = &predicted_players[i];
		state = &frame->playerstate[i];
		oldstate = &oldframe->playerstate[i];

		if (pplayer->predict) {
			for (j = 0; j < 3; j++) {
				state->viewangles[j] = MVD_AdjustAngle(oldstate->command.angles[j], pplayer->olda[j], f);
				state->origin[j] = oldstate->origin[j] + f * (pplayer->oldo[j] - oldstate->origin[j]);
				state->velocity[j] = oldstate->velocity[j] + f * (pplayer->oldv[j] - oldstate->velocity[j]);
			}
		}
	}
}

void CL_ClearPredict(void) {
	memset(predicted_players, 0, sizeof(predicted_players));
	mvd_fixangle = 0;
}
*/

