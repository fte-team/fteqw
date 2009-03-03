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
#include "particles.h"

extern	cvar_t	cl_predict_players;
extern	cvar_t	cl_predict_players2;
extern	cvar_t	cl_solid_players;
extern	cvar_t	cl_item_bobbing;

extern	cvar_t	r_rocketlight;
extern	cvar_t	r_lightflicker;
extern	cvar_t	cl_r2g;
extern	cvar_t	r_powerupglow;
extern	cvar_t	v_powerupshell;
extern	cvar_t	cl_nolerp;
extern	cvar_t	cl_nolerp_netquake;

extern	cvar_t	cl_gibfilter, cl_deadbodyfilter;
extern int cl_playerindex;

static struct predicted_player {
	int flags;
	qboolean active;
	vec3_t origin;	// predicted origin

	vec3_t	oldo;
	vec3_t	olda;
	vec3_t	oldv;
	qboolean predict;
	player_state_t *oldstate;
} predicted_players[MAX_CLIENTS];

extern int cl_playerindex, cl_h_playerindex, cl_rocketindex, cl_grenadeindex, cl_gib1index, cl_gib2index, cl_gib3index;

qboolean CL_FilterModelindex(int modelindex, int frame)
{
	if (modelindex == cl_playerindex)
	{
		if (cl_deadbodyfilter.value == 2)
		{
			if (frame >= 41 && frame <= 102)
				return true;
		}
		else if (cl_deadbodyfilter.value)
		{
			if (frame == 49 || frame == 60 || frame == 69 || frame == 84 || frame == 93 || frame == 102)
				return true;
		}
	}

	if (cl_gibfilter.value && (
			modelindex == cl_h_playerindex ||
			modelindex == cl_gib1index ||
			modelindex == cl_gib2index ||
			modelindex == cl_gib3index))
		return true;
	return false;
}

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
		for (i=0 ; i<dlights_running ; i++, dl++)
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
	if (dlights_running < MAX_DLIGHTS)
	{
		dl = &cl_dlights[dlights_running];
		memset (dl, 0, sizeof(*dl));
		dl->key = key;
		dlights_running++;
		if (dlights_software < MAX_SWLIGHTS)
			dlights_software++;
		return dl;
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
	dl->die = (float)cl.time + time;
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
dlight_t *CL_NewDlightRGB (int key, float x, float y, float z, float radius, float time,
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

	return dl;
}


/*
===============
CL_DecayLights

===============
*/
void CL_DecayLights (void)
{
	int			i;
	int lastrunning = -1;
	dlight_t	*dl;

	if (cl.paused)	//DON'T DO IT!!!
		return;

	dl = cl_dlights;
	for (i=0 ; i<dlights_running ; i++, dl++)
	{
		if (!dl->radius)
			continue;

		if (dl->die < (float)cl.time)
		{
			dl->radius = 0;
			continue;
		}

		dl->radius -= host_frametime*dl->decay;
		if (dl->radius < 0)
		{
			dl->radius = 0;
			continue;
		}
		lastrunning = i;

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
	}
	dlights_running = lastrunning+1;
	dlights_software = dlights_running;
	if (dlights_software > MAX_SWLIGHTS)
		dlights_software = MAX_SWLIGHTS;
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
		to->effects = (to->effects&0xff00)|MSG_ReadByte();

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
		to->scale = MSG_ReadByte();
#endif
#ifdef PEXT_TRANS
	if (morebits & U_TRANS && cls.fteprotocolextensions & PEXT_TRANS)
		to->trans = MSG_ReadByte();
#endif
#ifdef PEXT_FATNESS
	if (morebits & U_FATNESS && cls.fteprotocolextensions & PEXT_FATNESS)
		to->fatness = MSG_ReadChar();
#endif

	if (morebits & U_DRAWFLAGS && cls.fteprotocolextensions & PEXT_HEXEN2)
		to->hexen2flags = MSG_ReadByte();
	if (morebits & U_ABSLIGHT && cls.fteprotocolextensions & PEXT_HEXEN2)
		to->abslight = MSG_ReadByte();

	if (morebits & U_COLOURMOD && cls.fteprotocolextensions & PEXT_COLOURMOD)
	{
		to->colormod[0] = MSG_ReadByte();
		to->colormod[1] = MSG_ReadByte();
		to->colormod[2] = MSG_ReadByte();
	}

	if (morebits & U_ENTITYDBL)
		to->number += 512;
	if (morebits & U_ENTITYDBL2)
		to->number += 1024;
	if (morebits & U_MODELDBL)
		to->modelindex += 256;

	if (morebits & U_DPFLAGS)// && cls.fteprotocolextensions & PEXT_DPFLAGS)
	{
		// these are bits for the 'flags' field of the entity_state_t

		i = MSG_ReadByte();
		to->flags = 0;
		if (i & RENDER_VIEWMODEL)
			to->flags |= Q2RF_WEAPONMODEL|Q2RF_MINLIGHT|Q2RF_DEPTHHACK;
		if (i & RENDER_EXTERIORMODEL)
			to->flags |= Q2RF_EXTERNALMODEL;
	}
	if (morebits & U_TAGINFO)
	{
		to->tagentity = MSG_ReadShort();
		to->tagindex = MSG_ReadShort();
	}
	if (morebits & U_LIGHT)
	{
		to->light[0] = MSG_ReadShort();
		to->light[1] = MSG_ReadShort();
		to->light[2] = MSG_ReadShort();
		to->light[3] = MSG_ReadShort();
		to->lightstyle = MSG_ReadByte();
		to->lightpflags = MSG_ReadByte();
	}

	if (morebits & U_EFFECTS16)
		to->effects = (to->effects&0x00ff)|(MSG_ReadByte()<<8);
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
	int		from;

	if (!(cls.fteprotocolextensions & PEXT_ACCURATETIMINGS))
	{
		cl.oldgametime = cl.gametime;
		cl.oldgametimemark = cl.gametimemark;
		cl.gametime = realtime;
		cl.gametimemark = realtime;
	}

	newpacket = cls.netchan.incoming_sequence&UPDATE_MASK;
	newp = &cl.frames[newpacket].packet_entities;
	cl.frames[newpacket].invalid = false;

	newp->servertime = cl.gametime;

	if (delta)
	{
		from = MSG_ReadByte ();

//		Con_Printf("%i %i from %i\n", cls.netchan.outgoing_sequence, cls.netchan.incoming_sequence, from);

		oldpacket = cl.frames[newpacket].delta_sequence;
		if (cls.demoplayback == DPB_MVD || cls.demoplayback == DPB_EZTV)
			from = oldpacket = cls.netchan.incoming_sequence - 1;

		if (cls.netchan.outgoing_sequence - cls.netchan.incoming_sequence >= UPDATE_BACKUP - 1) {
			// there are no valid frames left, so drop it
			FlushEntityPacket ();
			cl.validsequence = 0;
			return;
		}

		if ((from & UPDATE_MASK) != (oldpacket & UPDATE_MASK)) {
			Con_DPrintf ("WARNING: from mismatch\n");
//			FlushEntityPacket ();
//			cl.validsequence = 0;
//			return;
		}

		if (cls.netchan.outgoing_sequence - oldpacket >= UPDATE_BACKUP - 1)
		{
			// we can't use this, it is too old
			FlushEntityPacket ();
			// don't clear cl.validsequence, so that frames can still be rendered;
			// it is possible that a fresh packet will be received before
			// (outgoing_sequence - incoming_sequence) exceeds UPDATE_BACKUP - 1
			return;
		}

		oldp = &cl.frames[oldpacket & UPDATE_MASK].packet_entities;
		full = false;
	}
	else
	{	// this is a full update that we can start delta compressing from now
		oldp = &dummy;
		dummy.num_entities = 0;
		full = true;
	}

	cl.oldvalidsequence = cl.validsequence;
	cl.validsequence = cls.netchan.incoming_sequence;

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
			{	//really read the extra entity number if required
				if (word & U_MOREBITS)
					if (MSG_ReadByte() & U_EVENMORE)
						MSG_ReadByte();

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

			if (!CL_CheckBaselines(newnum))
				Host_EndGame("CL_ParsePacketEntities: check baselines failed with size %i", newnum);
			CL_ParseDelta (cl_baselines + newnum, &newp->entities[newindex], word, true);
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
				if (word & U_MOREBITS)
					if (MSG_ReadByte() & U_EVENMORE)
						MSG_ReadByte();
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


entity_state_t *CL_FindOldPacketEntity(int num)
{
	int					pnum;
	entity_state_t		*s1;
	packet_entities_t	*pack;
	if (!cl.validsequence)
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
#ifdef NQPROT

entity_state_t defaultstate;
void DP5_ParseDelta(entity_state_t *s)
{
	int bits;
	bits = MSG_ReadByte();
	if (bits & E5_EXTEND1)
	{
		bits |= MSG_ReadByte() << 8;
		if (bits & E5_EXTEND2)
		{
			bits |= MSG_ReadByte() << 16;
			if (bits & E5_EXTEND3)
				bits |= MSG_ReadByte() << 24;
		}
	}

	if (bits & E5_ALLUNUSED)
	{
		Host_EndGame("Detected 'unused' bits in DP5+ entity delta - %x (%x)\n", bits, (bits & E5_ALLUNUSED));
	}

	if (bits & E5_FULLUPDATE)
	{
		int num;
		num = s->number;
		*s = defaultstate;
		s->trans = 255;
		s->scale = 16;
		s->number = num;
//		s->active = true;
	}
	if (bits & E5_FLAGS)
	{
		int i = MSG_ReadByte();
		s->flags = 0;
		if (i & RENDER_VIEWMODEL)
			s->flags |= Q2RF_WEAPONMODEL|Q2RF_MINLIGHT|Q2RF_DEPTHHACK;
		if  (i & RENDER_EXTERIORMODEL)
			s->flags |= Q2RF_EXTERNALMODEL;
	}
	if (bits & E5_ORIGIN)
	{
		if (bits & E5_ORIGIN32)
		{
			s->origin[0] = MSG_ReadFloat();
			s->origin[1] = MSG_ReadFloat();
			s->origin[2] = MSG_ReadFloat();
		}
		else
		{
			s->origin[0] = MSG_ReadShort()*(1/8.0f);
			s->origin[1] = MSG_ReadShort()*(1/8.0f);
			s->origin[2] = MSG_ReadShort()*(1/8.0f);
		}
	}
	if (bits & E5_ANGLES)
	{
		if (bits & E5_ANGLES16)
		{
			s->angles[0] = MSG_ReadAngle16();
			s->angles[1] = MSG_ReadAngle16();
			s->angles[2] = MSG_ReadAngle16();
		}
		else
		{
			s->angles[0] = MSG_ReadChar() * (360.0/256);
			s->angles[1] = MSG_ReadChar() * (360.0/256);
			s->angles[2] = MSG_ReadChar() * (360.0/256);
		}
	}
	if (bits & E5_MODEL)
	{
		if (bits & E5_MODEL16)
			s->modelindex = (unsigned short) MSG_ReadShort();
		else
			s->modelindex = MSG_ReadByte();
	}
	if (bits & E5_FRAME)
	{
		if (bits & E5_FRAME16)
			s->frame = (unsigned short) MSG_ReadShort();
		else
			s->frame = MSG_ReadByte();
	}
	if (bits & E5_SKIN)
		s->skinnum = MSG_ReadByte();
	if (bits & E5_EFFECTS)
	{
		if (bits & E5_EFFECTS32)
			s->effects = (unsigned int) MSG_ReadLong();
		else if (bits & E5_EFFECTS16)
			s->effects = (unsigned short) MSG_ReadShort();
		else
			s->effects = MSG_ReadByte();
	}
	if (bits & E5_ALPHA)
		s->trans = MSG_ReadByte();
	if (bits & E5_SCALE)
		s->scale = MSG_ReadByte();
	if (bits & E5_COLORMAP)
		s->colormap = MSG_ReadByte();
	if (bits & E5_ATTACHMENT)
	{
		s->tagentity = MSG_ReadShort();
		s->tagindex = MSG_ReadByte();
	}
	if (bits & E5_LIGHT)
	{
		s->light[0] = MSG_ReadShort();
		s->light[1] = MSG_ReadShort();
		s->light[2] = MSG_ReadShort();
		s->light[3] = MSG_ReadShort();
		s->lightstyle = MSG_ReadByte();
		s->lightpflags = MSG_ReadByte();
	}
	if (bits & E5_GLOW)
	{
		s->glowsize = MSG_ReadByte();
		s->glowcolour = MSG_ReadByte();
	}
	if (bits & E5_COLORMOD)
	{
		s->colormod[0] = MSG_ReadByte();
		s->colormod[1] = MSG_ReadByte();
		s->colormod[2] = MSG_ReadByte();
	}
}

int cl_latestframenum;
void CLNQ_ParseDarkPlaces5Entities(void)	//the things I do.. :o(
{
	//the incoming entities do not come in in any order. :(
	//well, they come in in order of priorities, but that's not useful to us.
	//I guess this means we'll have to go slowly.

	packet_entities_t	*pack, *oldpack;

	entity_state_t		*to, *from;
	unsigned short read;
	int oldi;
	qboolean remove;

	cl_latestframenum = MSG_ReadLong();

	if (nq_dp_protocol >=7)
		cl.ackedinputsequence = MSG_ReadLong();

	pack = &cl.frames[(cls.netchan.incoming_sequence)&UPDATE_MASK].packet_entities;
	pack->servertime = cl.gametime;
	oldpack = &cl.frames[(cls.netchan.incoming_sequence-1)&UPDATE_MASK].packet_entities;

	from = oldpack->entities;
	oldi = 0;
	pack->num_entities = 0;

	for (oldi = 0; oldi < oldpack->num_entities; oldi++)
	{
		from = &oldpack->entities[oldi];
		from->flags &= ~0x80000000;
	}

	for (read = MSG_ReadShort(); read!=0x8000; read = MSG_ReadShort())
	{
		if (msg_badread)
			Host_EndGame("Corrupt entitiy message packet\n");
		remove = !!(read&0x8000);
		read&=~0x8000;

		if (read >= MAX_EDICTS)
			Host_EndGame("Too many entities.\n");

		from = &defaultstate;

		for (oldi=0 ; oldi<oldpack->num_entities ; oldi++)
		{
			if (read == oldpack->entities[oldi].number)
			{
				from = &oldpack->entities[oldi];
				from->flags |= 0x80000000;	//so we don't copy it.
				break;
			}
		}

		if (remove)
		{
			continue;
		}

		if (pack->num_entities==pack->max_entities)
		{
			pack->max_entities = pack->num_entities+16;
			pack->entities = BZ_Realloc(pack->entities, sizeof(entity_state_t)*pack->max_entities);
		}

		to = &pack->entities[pack->num_entities];
		pack->num_entities++;
		memcpy(to, from, sizeof(*to));
		to->number = read;
		DP5_ParseDelta(to);
		to->flags &= ~0x80000000;
	}

	//the pack has all the new ones in it, now copy the old ones in that wern't removed (or changed).
	for (oldi = 0; oldi < oldpack->num_entities; oldi++)
	{
		from = &oldpack->entities[oldi];
		if (from->flags & 0x80000000)
			continue;

		if (pack->num_entities==pack->max_entities)
		{
			pack->max_entities = pack->num_entities+16;
			pack->entities = BZ_Realloc(pack->entities, sizeof(entity_state_t)*pack->max_entities);
		}

		to = &pack->entities[pack->num_entities];
		pack->num_entities++;

		from = &oldpack->entities[oldi];

		memcpy(to, from, sizeof(*to));
	}
}

void CLNQ_ParseEntity(unsigned int bits)
{
	int i;
	int num, pnum;
	entity_state_t		*state, *from;
	entity_state_t	*base;
	static float lasttime;
	packet_entities_t	*pack;

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
	if (bits & DPU_EXTEND1)
	{
		i = MSG_ReadByte ();
		bits |= (i<<16);
	}
	if (bits & DPU_EXTEND2)
	{
		i = MSG_ReadByte ();
		bits |= (i<<24);
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
			memset(pack->entities + pack->num_entities, 0, sizeof(entity_state_t));
		}
		lasttime = realtime;
		state = &pack->entities[pack->num_entities++];
	}

	from = CL_FindOldPacketEntity(num);	//this could be optimised.

	if (!CL_CheckBaselines(num))
		Host_EndGame("CLNQ_ParseEntity: check baselines failed with size %i", num);
	base = cl_baselines + num;

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
		state->colormap = base->colormap;

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

	if (bits & DPU_ALPHA)
		i = MSG_ReadByte();
	else
		i = -1;

#ifdef PEXT_TRANS
	if (i == -1)
		state->trans = base->trans;
	else
		state->trans = i;
#endif

	if (bits & DPU_SCALE)
		i = MSG_ReadByte();
	else
		i = -1;

#ifdef PEXT_SCALE
	if (i == -1)
		state->scale = base->scale;
	else
		state->scale = i;
#endif

	if (bits & DPU_EFFECTS2)
		state->effects |= MSG_ReadByte() << 8;

	if (bits & DPU_GLOWSIZE)
		state->glowsize = MSG_ReadByte();
	else
		state->glowsize = base->glowsize;

	if (bits & DPU_GLOWCOLOR)
		state->glowcolour = MSG_ReadByte();
	else
		state->glowcolour = base->glowcolour;

	if (bits & DPU_COLORMOD)
	{
		i = MSG_ReadByte(); // follows format RRRGGGBB
		state->colormod[0] = (qbyte)(((i >> 5) & 7) * (32.0f / 7.0f));
		state->colormod[1] = (qbyte)(((i >> 2) & 7) * (32.0f / 7.0f));
		state->colormod[2] = (qbyte)((i & 3) * (32.0f / 3.0f));
	}
	else
	{
		state->colormod[0] = base->colormod[0];
		state->colormod[1] = base->colormod[1];
		state->colormod[2] = base->colormod[2];
	}

	if (bits & DPU_FRAME2)
		state->frame |= MSG_ReadByte() << 8;

	if (bits & DPU_MODEL2)
		state->modelindex |= MSG_ReadByte() << 8;

	if (cls.demoplayback != DPB_NONE)
		for (pnum = 0; pnum < cl.splitclients; pnum++)
			if (num == cl.viewentity[pnum])
			{
				state->angles[0] = cl.viewangles[pnum][0]/-3;
				state->angles[1] = cl.viewangles[pnum][1];
				state->angles[2] = cl.viewangles[pnum][2];
			}
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

//return 0 to 1
//1 being entirly new frame.
float CL_LerpEntityFrac(float lerprate, float lerptime)
{
	float f;
	if (!lerprate)
	{
		return 0;
	}
	else
	{
		f = 1-(cl.time-lerptime)/lerprate;
	}

	if (f<0)f=0;
	if (f>1)f=1;

	return f;
}

float CL_EntLerpFactor(int entnum)
{
	float f;
	if (cl.lerpents[entnum].lerprate<=0)
		return 0;
	else
		f = 1-(cl.time-cl.lerpents[entnum].lerptime)/cl.lerpents[entnum].lerprate;
	if (f<0)
		f=0;
	if (f>1)
		f=1;
	return f;
}

void CL_RotateAroundTag(entity_t *ent, int num, int tagent, int tagnum)
{
	entity_state_t *ps;
	float *org=NULL, *ang=NULL;
	vec3_t axis[3];
	float transform[12], parent[12], result[12], old[12], temp[12];

	int model;
	framestate_t fstate;

	if (tagent > cl.maxlerpents)
	{
		Con_Printf("tag entity out of range!\n");
		return;
	}

	memset(&fstate, 0, sizeof(fstate));

	fstate.g[FS_REG].frame[1] = cl.lerpents[tagent].frame;

	ent->keynum = tagent;

	ps = CL_FindPacketEntity(tagent);
	if (ps)
	{
		if (ps->tagentity)
			CL_RotateAroundTag(ent, num, ps->tagentity, ps->tagindex);

		org = ps->origin;
		ang = ps->angles;
		model = ps->modelindex;
		fstate.g[FS_REG].frame[0] = ps->frame;
	}
	else
	{
		extern int parsecountmod;
//		Con_Printf("tagent %i\n", tagent);
		if (tagent <= MAX_CLIENTS && tagent > 0)
		{
			if (tagent-1 == cl.playernum[0])
			{
				org = cl.simorg[0];
				ang = cl.simangles[0];
			}
			else
			{
				org = cl.frames[parsecountmod].playerstate[tagent-1].origin;
				ang = cl.frames[parsecountmod].playerstate[tagent-1].viewangles;
			}
			model = cl.frames[parsecountmod].playerstate[tagent-1].modelindex;
			fstate.g[FS_REG].frame[0] = cl.frames[parsecountmod].playerstate[tagent-1].frame;
		}
		else
			model = 0;
	}

	if (ang)
	{
		ang[0]*=-1;
		AngleVectors(ang, axis[0], axis[1], axis[2]);
		ang[0]*=-1;
		VectorInverse(axis[1]);

		fstate.g[FS_REG].lerpfrac = CL_EntLerpFactor(tagent);
		fstate.g[FS_REG].frametime[0] = cl.time - cl.lerpents[tagent].framechange;
		fstate.g[FS_REG].frametime[1] = cl.time - cl.lerpents[tagent].oldframechange;
		if (Mod_GetTag(cl.model_precache[model], tagnum, &fstate, transform))
		{
			old[0] = ent->axis[0][0];
			old[1] = ent->axis[1][0];
			old[2] = ent->axis[2][0];
			old[3] = ent->origin[0];
			old[4] = ent->axis[0][1];
			old[5] = ent->axis[1][1];
			old[6] = ent->axis[2][1];
			old[7] = ent->origin[1];
			old[8] = ent->axis[0][2];
			old[9] = ent->axis[1][2];
			old[10] = ent->axis[2][2];
			old[11] = ent->origin[2];

			parent[0] = axis[0][0];
			parent[1] = axis[1][0];
			parent[2] = axis[2][0];
			parent[3] = org[0];
			parent[4] = axis[0][1];
			parent[5] = axis[1][1];
			parent[6] = axis[2][1];
			parent[7] = org[1];
			parent[8] = axis[0][2];
			parent[9] = axis[1][2];
			parent[10] = axis[2][2];
			parent[11] = org[2];

			R_ConcatTransforms((void*)old, (void*)parent, (void*)temp);
			R_ConcatTransforms((void*)temp, (void*)transform, (void*)result);

			ent->axis[0][0] = result[0];
			ent->axis[1][0] = result[1];
			ent->axis[2][0] = result[2];
			ent->origin[0] = result[3];
			ent->axis[0][1] = result[4];
			ent->axis[1][1] = result[5];
			ent->axis[2][1] = result[6];
			ent->origin[1] = result[7];
			ent->axis[0][2] = result[8];
			ent->axis[1][2] = result[9];
			ent->axis[2][2] = result[10];
			ent->origin[2] = result[11];
		}
		else	//hrm.
		{
			old[0] = ent->axis[0][0];
			old[1] = ent->axis[1][0];
			old[2] = ent->axis[2][0];
			old[3] = ent->origin[0];
			old[4] = ent->axis[0][1];
			old[5] = ent->axis[1][1];
			old[6] = ent->axis[2][1];
			old[7] = ent->origin[1];
			old[8] = ent->axis[0][2];
			old[9] = ent->axis[1][2];
			old[10] = ent->axis[2][2];
			old[11] = ent->origin[2];

			parent[0] = axis[0][0];
			parent[1] = axis[1][0];
			parent[2] = axis[2][0];
			parent[3] = org[0];
			parent[4] = axis[0][1];
			parent[5] = axis[1][1];
			parent[6] = axis[2][1];
			parent[7] = org[1];
			parent[8] = axis[0][2];
			parent[9] = axis[1][2];
			parent[10] = axis[2][2];
			parent[11] = org[2];

			R_ConcatTransforms((void*)old, (void*)parent, (void*)result);

			ent->axis[0][0] = result[0];
			ent->axis[1][0] = result[1];
			ent->axis[2][0] = result[2];
			ent->origin[0] = result[3];
			ent->axis[0][1] = result[4];
			ent->axis[1][1] = result[5];
			ent->axis[2][1] = result[6];
			ent->origin[1] = result[7];
			ent->axis[0][2] = result[8];
			ent->axis[1][2] = result[9];
			ent->axis[2][2] = result[10];
			ent->origin[2] = result[11];
		}
	}
}

void V_AddAxisEntity(entity_t *in)
{
	entity_t *ent;

	if (cl_numvisedicts == MAX_VISEDICTS)
	{
		Con_Printf("Visedict list is full!\n");
		return;		// object list is full
	}
	ent = &cl_visedicts[cl_numvisedicts];
	cl_numvisedicts++;

	*ent = *in;
}
void V_AddEntity(entity_t *in)
{
	entity_t *ent;

	if (cl_numvisedicts == MAX_VISEDICTS)
	{
		Con_Printf("Visedict list is full!\n");
		return;		// object list is full
	}
	ent = &cl_visedicts[cl_numvisedicts];
	cl_numvisedicts++;

	*ent = *in;

	ent->angles[0]*=-1;
	AngleVectors(ent->angles, ent->axis[0], ent->axis[1], ent->axis[2]);
	VectorInverse(ent->axis[1]);
	ent->angles[0]*=-1;
}

void VQ2_AddLerpEntity(entity_t *in)	//a convienience function
{
	entity_t *ent;
	float fwds, back;
	int i;

	if (cl_numvisedicts == MAX_VISEDICTS)
		return;		// object list is full
	ent = &cl_visedicts[cl_numvisedicts];
	cl_numvisedicts++;

	*ent = *in;

	fwds = ent->framestate.g[FS_REG].lerpfrac;
	back = 1 - ent->framestate.g[FS_REG].lerpfrac;
	for (i = 0; i < 3; i++)
	{
		ent->origin[i] = in->origin[i]*fwds + in->oldorigin[i]*back;
	}

	ent->framestate.g[FS_REG].lerpfrac = back;

	ent->angles[0]*=-1;
	AngleVectors(ent->angles, ent->axis[0], ent->axis[1], ent->axis[2]);
	VectorInverse(ent->axis[1]);
	ent->angles[0]*=-1;
}

void V_AddLight (vec3_t org, float quant, float r, float g, float b)
{
	CL_NewDlightRGB (0, org[0], org[1], org[2], quant, -0.1, r, g, b);
}

/*
===============
CL_LinkPacketEntities

===============
*/
void R_FlameTrail(vec3_t start, vec3_t end, float seperation);
#define DECENTLERP
#ifdef DECENTLERP

void CL_TransitionPacketEntities(packet_entities_t *newpack, packet_entities_t *oldpack, float servertime)
{
	lerpents_t		*le;
	entity_state_t		*snew, *sold;
	int					i, j;
	int					oldpnum, newpnum;

	vec3_t move;

	float a1, a2;

	float frac;
	/*
		seeing as how dropped packets cannot be filled in due to the reliable networking stuff,
		We can simply detect changes and lerp towards them
	*/

	//we have two index-sorted lists of entities
	//we figure out which ones are new,
	//we don't care about old, as our caller will use the lerpents array we fill, and the entity numbers from the 'new' packet.

	if (newpack->servertime == oldpack->servertime)
		frac = 1; //lerp totally into the new
	else
		frac = (servertime-oldpack->servertime)/(newpack->servertime-oldpack->servertime);

	oldpnum=0;
	for (newpnum=0 ; newpnum<newpack->num_entities ; newpnum++)
	{
		snew = &newpack->entities[newpnum];

		sold = NULL;
		for ( ; oldpnum<oldpack->num_entities ; oldpnum++)
		{
			sold = &oldpack->entities[oldpnum];
			if (sold->number >= snew->number)
			{
				if (sold->number > snew->number)
					sold = NULL;	//woo, it's a new entity.
				break;
			}
		}
		if (!sold)	//I'm lazy
			sold = snew;

		if (snew->number >= cl.maxlerpents)
		{
			int newmaxle = snew->number+16;
			cl.lerpents = BZ_Realloc(cl.lerpents, newmaxle*sizeof(lerpents_t));
			memset(cl.lerpents + cl.maxlerpents, 0, sizeof(lerpents_t)*(newmaxle - cl.maxlerpents));
			cl.maxlerpents = newmaxle;
		}
		le = &cl.lerpents[snew->number];

		VectorSubtract(snew->origin, sold->origin, move);
		if (DotProduct(move, move) > 200*200 || snew->modelindex != sold->modelindex)
		{
			sold = snew;	//teleported?
			VectorClear(move);
		}

		for (i = 0; i < 3; i++)
		{
			le->origin[i] = sold->origin[i] + frac*(move[i]);

			for (j = 0; j < 3; j++)
			{
				a1 = sold->angles[i];
				a2 = snew->angles[i];
				if (a1 - a2 > 180)
					a1 -= 360;
				if (a1 - a2 < -180)
					a1 += 360;
				le->angles[i] = a1 + frac * (a2 - a1);
			}
		}

		if (snew == sold || (sold->frame != le->frame && sold->frame != snew->frame) || snew->modelindex != sold->modelindex)
		{
			le->oldframechange = le->framechange;
			le->framechange = newpack->servertime;

			le->frame = sold->frame;
		}
	}
}

packet_entities_t *CL_ProcessPacketEntities(float *servertime, qboolean nolerp)
{
	packet_entities_t	*packnew, *packold;
	int					i;
	//, spnum;

	if (nolerp)
	{	//force our emulated time to as late as we can.
		//this will disable all position interpolation
		*servertime = cl.frames[cls.netchan.incoming_sequence&UPDATE_MASK].packet_entities.servertime;
//		Con_DPrintf("No lerp\n");
	}

	packnew = NULL;
	packold = NULL;
	//choose the two packets.
	//we should be picking the packet just after the server time, and the one just before
	for (i = cls.netchan.incoming_sequence; i >= cls.netchan.incoming_sequence-UPDATE_MASK; i--)
	{
		if (cl.frames[i&UPDATE_MASK].receivedtime < 0 || cl.frames[i&UPDATE_MASK].invalid)
			continue;	//packetloss/choke, it's really only a problem for the oldframe, but...

		if (cl.frames[i&UPDATE_MASK].packet_entities.servertime >= *servertime)
		{
			if (cl.frames[i&UPDATE_MASK].packet_entities.servertime)
			{
				if (!packnew || packnew->servertime != cl.frames[i&UPDATE_MASK].packet_entities.servertime)	//if it's a duplicate, pick the latest (so just-shot rockets are still present)
					packnew = &cl.frames[i&UPDATE_MASK].packet_entities;
			}
		}
		else if (packnew)
		{
			if (cl.frames[i&UPDATE_MASK].packet_entities.servertime != packnew->servertime)
			{	//it does actually lerp, and isn't an identical frame.
				packold = &cl.frames[i&UPDATE_MASK].packet_entities;
				break;
			}
		}
	}

	//Note, hacking this to return anyway still needs the lerpent array to be valid for all contained entities.

	if (!packnew)	//should never happen
	{
		Con_DPrintf("Warning: No lerp-to frame packet\n");
		return NULL;
	}
	if (!packold)	//can happem at map start, and really laggy games, but really shouldn't in a normal game
	{
//		Con_DPrintf("Warning: No lerp-from frame packet\n");
		packold = packnew;
	}

	CL_TransitionPacketEntities(packnew, packold, *servertime);

//	Con_DPrintf("%f %f %f %f %f %f\n", packnew->servertime, *servertime, packold->servertime, cl.gametime, cl.oldgametime, cl.servertime);

//	if (packold->servertime < oldoldtime)
//		Con_Printf("Spike screwed up\n");
//	oldoldtime = packold->servertime;

	return packnew;
}

qboolean CL_MayLerp(void)
{
	//force lerping when playing low-framerate demos.
	if (cls.demoplayback == DPB_MVD || cls.demoplayback == DPB_EZTV)
		return true;
#ifdef NQPROT
	if (cls.demoplayback == DPB_NETQUAKE)
		return true;

	if (cls.protocol == CP_NETQUAKE)	//this includes DP protocols.
		return !cl_nolerp_netquake.value;
#endif
	return !cl_nolerp.value;
}

void CL_LinkPacketEntities (void)
{
	entity_t			*ent;
	packet_entities_t	*pack;
	entity_state_t		*state;
	lerpents_t		*le;
	model_t				*model;
	vec3_t				old_origin;
	float				autorotate;
	int					i;
	int					newpnum;
	//, spnum;
	dlight_t			*dl;
	vec3_t				angles;
	int flicker;
	qboolean nolerp;

	float servertime;

	CL_CalcClientTime();
	servertime = cl.servertime;

	nolerp = !CL_MayLerp() && cls.demoplayback != DPB_MVD && cls.demoplayback != DPB_EZTV;
#ifdef NQPROT
	nolerp = nolerp && cls.demoplayback != DPB_NETQUAKE;
#endif
	pack = CL_ProcessPacketEntities(&servertime, nolerp);
	if (!pack)
		return;

	servertime = cl.servertime;


	autorotate = anglemod(100*servertime);

	for (newpnum=0 ; newpnum<pack->num_entities ; newpnum++)
	{
		state = &pack->entities[newpnum];



		if (cl_numvisedicts == MAX_VISEDICTS)
		{
			Con_Printf("Too many visible entities\n");
			break;
		}
		ent = &cl_visedicts[cl_numvisedicts];
#ifdef Q3SHADERS
		ent->forcedshader = NULL;
#endif

		le = &cl.lerpents[state->number];

		memset(&ent->framestate, 0, sizeof(ent->framestate));

		if (le->framechange == le->oldframechange)
			ent->framestate.g[FS_REG].lerpfrac = 0;
		else
		{
			ent->framestate.g[FS_REG].lerpfrac = 1-(servertime - le->framechange) / (le->framechange - le->oldframechange);
			if (ent->framestate.g[FS_REG].lerpfrac > 1)
				ent->framestate.g[FS_REG].lerpfrac = 1;
			else if (ent->framestate.g[FS_REG].lerpfrac < 0)
			{
				ent->framestate.g[FS_REG].lerpfrac = 0;
				//le->oldframechange = le->framechange;
			}
		}


		VectorCopy(le->origin, ent->origin);

		//bots or powerup glows. items always glow, powerups can be disabled
		if (state->modelindex != cl_playerindex || r_powerupglow.value)
		{
			flicker = r_lightflicker.value?(rand()&31):0;
			// spawn light flashes, even ones coming from invisible objects
			if ((state->effects & (EF_BLUE | EF_RED)) == (EF_BLUE | EF_RED))
				CL_NewDlight (state->number, state->origin[0], state->origin[1], state->origin[2], 200 + flicker, 0, 3);
			else if (state->effects & EF_BLUE)
				CL_NewDlight (state->number, state->origin[0], state->origin[1], state->origin[2], 200 + flicker, 0, 1);
			else if (state->effects & EF_RED)
				CL_NewDlight (state->number, state->origin[0], state->origin[1], state->origin[2], 200 + flicker, 0, 2);
			else if (state->effects & EF_BRIGHTLIGHT)
				CL_NewDlight (state->number, state->origin[0], state->origin[1], state->origin[2] + 16, 400 + flicker, 0, 0);
			else if (state->effects & EF_DIMLIGHT)
				CL_NewDlight (state->number, state->origin[0], state->origin[1], state->origin[2], 200 + flicker, 0, 0);
		}
		if (state->light[3])
		{
			CL_NewDlightRGB (state->number, state->origin[0], state->origin[1], state->origin[2], state->light[3], 0, state->light[0]/1024.0f, state->light[1]/1024.0f, state->light[2]/1024.0f);
		}

		// if set to invisible, skip
		if (state->modelindex<1)
			continue;

		// create a new entity
		if (cl_numvisedicts == MAX_VISEDICTS)
			break;		// object list is full

		if (CL_FilterModelindex(state->modelindex, state->frame))
			continue;

		model = cl.model_precache[state->modelindex];
		if (!model)
		{
			Con_DPrintf("Bad modelindex (%i)\n", state->modelindex);
			continue;
		}

		cl_numvisedicts++;

#ifdef Q3SHADERS
		ent->forcedshader = NULL;
#endif

		ent->visframe = 0;

		ent->keynum = state->number;

		if (cl_r2g.value && state->modelindex == cl_rocketindex && cl_rocketindex && cl_grenadeindex)
			ent->model = cl.model_precache[cl_grenadeindex];
		else
			ent->model = model;

		ent->flags = state->flags;
		if (state->effects & NQEF_ADDATIVE)
			ent->flags |= Q2RF_ADDATIVE;
		if (state->effects & EF_NODEPTHTEST)
			ent->flags |= RF_NODEPTHTEST;

		// set colormap
		if (state->colormap && (state->colormap <= MAX_CLIENTS)
			&& (gl_nocolors.value == -1 || (ent->model/* && state->modelindex == cl_playerindex*/)))
		{
			// TODO: DP colormap/colormod extension?
#ifdef SWQUAKE
			ent->palremap = cl.players[state->colormap-1].palremap;
#endif
			ent->scoreboard = &cl.players[state->colormap-1];
		}
		else
		{
#ifdef SWQUAKE
			ent->palremap = D_IdentityRemap();
#endif
			ent->scoreboard = NULL;
		}

		// set skin
		ent->skinnum = state->skinnum;

		ent->abslight = state->abslight;
		ent->drawflags = state->hexen2flags;

		// set frame
		ent->framestate.g[FS_REG].frame[0] = state->frame;
		ent->framestate.g[FS_REG].frame[1] = le->frame;

		ent->framestate.g[FS_REG].frametime[0] = cl.servertime - le->framechange;
		ent->framestate.g[FS_REG].frametime[1] = cl.servertime - le->oldframechange;

//		f = (sin(realtime)+1)/2;

#ifdef PEXT_SCALE
		//set scale
		ent->scale = state->scale/16.0;
#endif
		ent->shaderRGBAf[0] = (state->colormod[0]*8.0f)/255;
		ent->shaderRGBAf[1] = (state->colormod[1]*8.0f)/255;
		ent->shaderRGBAf[2] = (state->colormod[2]*8.0f)/255;
		ent->shaderRGBAf[3] = state->trans/255.0f;
#ifdef PEXT_FATNESS
		//set trans
		ent->fatness = state->fatness/2.0;
#endif

		// rotate binary objects locally
		if (model && model->flags & EF_ROTATE)
		{
			angles[0] = 0;
			angles[1] = autorotate;
			angles[2] = 0;

			if (cl_item_bobbing.value)
				ent->origin[2] += 5+sin(cl.time*3)*5;	//don't let it into the ground
		}
		else
		{
			for (i=0 ; i<3 ; i++)
			{
				angles[i] = le->angles[i];
			}
		}

		VectorCopy(angles, ent->angles);
		if (model && model->type == mod_alias)
			angles[0]*=-1;	//carmack screwed up when he added alias models - they pitch the wrong way.
		AngleVectors(angles, ent->axis[0], ent->axis[1], ent->axis[2]);
		VectorInverse(ent->axis[1]);

		if (ent->keynum <= MAX_CLIENTS
#ifdef NQPROT
			&& cls.protocol == CP_QUAKEWORLD
#endif
			)
			ent->keynum += MAX_EDICTS;

		if (state->tagentity)
		{	//ent is attached to a tag, rotate this ent accordingly.
			CL_RotateAroundTag(ent, state->number, state->tagentity, state->tagindex);
		}

		// add automatic particle trails
		if (!model || (!(model->flags&~EF_ROTATE) && model->particletrail<0 && model->particleeffect<0))
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
			pe->DelinkTrailstate(&(cl.lerpents[state->number].trailstate));
			pe->DelinkTrailstate(&(cl.lerpents[state->number].emitstate));
			continue;		// not in last message
		}

		for (i=0 ; i<3 ; i++)
		{
			if ( abs(old_origin[i] - ent->origin[i]) > 128)
			{	// no trail if too far
				VectorCopy (ent->origin, old_origin);
				break;
			}
		}

		if (model->particletrail >= 0)
		{
			if (pe->ParticleTrail (old_origin, ent->origin, model->particletrail, &(le->trailstate)))
				pe->ParticleTrailIndex(old_origin, ent->origin, model->traildefaultindex, 0, &(le->trailstate));
		}

		{
			extern cvar_t gl_part_flame;
			if (cls.allow_anyparticles && gl_part_flame.value)
			{
				P_EmitEffect (ent->origin, model->particleeffect, &(le->emitstate));
			}
		}

		//dlights are not so customisable.
		if (r_rocketlight.value)
		{
			float rad = 0;
			vec3_t dclr;

			dclr[0] = 0.20;
			dclr[1] = 0.10;
			dclr[2] = 0;

			if (model->flags & EF_ROCKET)
			{
				if (strncmp(model->name, "models/sflesh", 13))
				{	//hmm. hexen spider gibs...
					rad = 200;
					dclr[2] = 0.05;
				}
			}
			else if (model->flags & EFH2_FIREBALL)
			{
				rad = 120 - (rand() % 20);
			}
			else if (model->flags & EFH2_ACIDBALL)
			{
				rad = 120 - (rand() % 20);
			}
			else if (model->flags & EFH2_SPIT)
			{
				// as far as I can tell this effect inverses the light...
				dclr[0] = -dclr[0];
				dclr[0] = -dclr[1];
				dclr[0] = -dclr[2];
				rad = 120 - (rand() % 20);
			}

			if (rad)
			{
				dl = CL_AllocDlight (state->number);
				VectorCopy (ent->origin, dl->origin);
				dl->die = (float)cl.time;
				if (model->flags & EF_ROCKET)
					dl->origin[2] += 1; // is this even necessary
				dl->radius = rad * r_rocketlight.value;
				VectorCopy(dclr, dl->color);
			}


		}
	}
}
#else

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
	int					pnum;
	//, spnum;
	dlight_t			*dl;
	vec3_t				angles;
	int flicker;

	pack = &cl.frames[cls.netchan.incoming_sequence&UPDATE_MASK].packet_entities;

	autorotate = anglemod(100*cl.time);

	for (pnum=0 ; pnum<pack->num_entities ; pnum++)
	{
		s1 = &pack->entities[pnum];

		if (cl_numvisedicts == MAX_VISEDICTS)
		{
			Con_Printf("Too many visible entities\n");
			break;
		}
		ent = &cl_visedicts[cl_numvisedicts];
#ifdef Q3SHADERS
		ent->forcedshader = NULL;
#endif

		if (CL_MayLerp())
		{
			//figure out the lerp factor
			if (cl.lerpents[s1->number].lerprate<=0)
				f = 0;
			else
				f = (cl.servertime-cl.lerpents[s1->number].lerptime)/cl.lerpents[s1->number].lerprate;//(cl.gametime-cl.oldgametime);//1-(cl.time-cl.lerpents[s1->number].lerptime)/cl.lerpents[s1->number].lerprate;
			if (f<0)
				f=0;
			if (f>1)
				f=1;
		}
		else
			f = 1;

		ent->lerpfrac = 1-(cl.servertime-cl.lerpents[s1->number].lerptime)/cl.lerpents[s1->number].lerprate;
		if (ent->lerpfrac<0)
			ent->lerpfrac=0;
		if (ent->lerpfrac>1)
			ent->lerpfrac=1;

//		if (s1->modelindex == 87 && !cl.paused)
//			Con_Printf("%f %f\n", f, cl.lerpents[s1->number].lerptime-cl.servertime);

		// calculate origin
		for (i=0 ; i<3 ; i++)
			ent->origin[i] = cl.lerpents[s1->number].origin[i] +
			f * (s1->origin[i] - cl.lerpents[s1->number].origin[i]);

		//bots or powerup glows. items always glow, powerups can be disabled
		if (s1->modelindex != cl_playerindex || r_powerupglow.value)
		{
			flicker = r_lightflicker.value?(rand()&31):0;
			// spawn light flashes, even ones coming from invisible objects
			if ((s1->effects & (EF_BLUE | EF_RED)) == (EF_BLUE | EF_RED))
				CL_NewDlight (s1->number, s1->origin[0], s1->origin[1], s1->origin[2], 200 + flicker, 0, 3);
			else if (s1->effects & EF_BLUE)
				CL_NewDlight (s1->number, s1->origin[0], s1->origin[1], s1->origin[2], 200 + flicker, 0, 1);
			else if (s1->effects & EF_RED)
				CL_NewDlight (s1->number, s1->origin[0], s1->origin[1], s1->origin[2], 200 + flicker, 0, 2);
			else if (s1->effects & EF_BRIGHTLIGHT)
				CL_NewDlight (s1->number, s1->origin[0], s1->origin[1], s1->origin[2] + 16, 400 + flicker, 0, 0);
			else if (s1->effects & EF_DIMLIGHT)
				CL_NewDlight (s1->number, s1->origin[0], s1->origin[1], s1->origin[2], 200 + flicker, 0, 0);
		}
		if (s1->light[3])
		{
			CL_NewDlightRGB (s1->number, s1->origin[0], s1->origin[1], s1->origin[2], s1->light[3], 0, s1->light[0]/1024.0f, s1->light[1]/1024.0f, s1->light[2]/1024.0f);
		}

		// if set to invisible, skip
		if (s1->modelindex<1)
			continue;

		// create a new entity
		if (cl_numvisedicts == MAX_VISEDICTS)
			break;		// object list is full

		if (CL_FilterModelindex(s1->modelindex, s1->frame))
			continue;

		model = cl.model_precache[s1->modelindex];
		if (!model)
		{
			Con_DPrintf("Bad modelindex (%i)\n", s1->modelindex);
			continue;
		}

		cl_numvisedicts++;

#ifdef Q3SHADERS
		ent->forcedshader = NULL;
#endif

		ent->visframe = 0;

		ent->keynum = s1->number;

		if (cl_r2g.value && s1->modelindex == cl_rocketindex && cl_rocketindex && cl_grenadeindex)
			ent->model = cl.model_precache[cl_grenadeindex];
		else
			ent->model = model;

		ent->flags = s1->flags;
		if (s1->effects & NQEF_ADDATIVE)
			ent->flags |= Q2RF_ADDATIVE;
		if (s1->effects & EF_NODEPTHTEST)
			ent->flags |= RF_NODEPTHTEST;

		// set colormap
		if (s1->colormap && (s1->colormap <= MAX_CLIENTS)
			&& (gl_nocolors.value == -1 || (ent->model/* && s1->modelindex == cl_playerindex*/)))
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
		ent->drawflags = s1->hexen2flags;

		// set frame
		ent->frame = s1->frame;
		ent->oldframe = cl.lerpents[s1->number].frame;

		ent->frame1time = cl.servertime - cl.lerpents[s1->number].framechange;
		ent->frame2time = cl.servertime - cl.lerpents[s1->number].oldframechange;

//		f = (sin(realtime)+1)/2;

#ifdef PEXT_SCALE
		//set scale
		ent->scale = s1->scale/16.0;
#endif
#ifdef PEXT_TRANS
		//set trans
		ent->alpha = s1->trans/255.0;
#endif
#ifdef PEXT_FATNESS
		//set trans
		ent->fatness = s1->fatness/2.0;
#endif

		// rotate binary objects locally
		if (model && model->flags & EF_ROTATE)
		{
			angles[0] = 0;
			angles[1] = autorotate;
			angles[2] = 0;

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
				angles[i] = a1 + f * (a2 - a1);
			}
		}

		VectorCopy(angles, ent->angles);
		angles[0]*=-1;
		AngleVectors(angles, ent->axis[0], ent->axis[1], ent->axis[2]);
		VectorInverse(ent->axis[1]);

		if (ent->keynum <= MAX_CLIENTS
#ifdef NQPROT
			&& cls.protocol == CP_QUAKEWORLD
#endif
			)
			ent->keynum += MAX_EDICTS;

		if (s1->tagentity)
		{	//ent is attached to a tag, rotate this ent accordingly.
			CL_RotateAroundTag(ent, s1->number, s1->tagentity, s1->tagindex);
		}

		// add automatic particle trails
		if (!model || (!(model->flags&~EF_ROTATE) && model->particletrail<0 && model->particleeffect<0))
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
			P_DelinkTrailstate(&(cl.lerpents[s1->number].trailstate));
			P_DelinkTrailstate(&(cl.lerpents[s1->number].emitstate));
			continue;		// not in last message
		}

		for (i=0 ; i<3 ; i++)
		{
			if ( abs(old_origin[i] - ent->origin[i]) > 128)
			{	// no trail if too far
				VectorCopy (ent->origin, old_origin);
				break;
			}
		}

		if (model->particletrail >= 0)
		{
			if (P_ParticleTrail (old_origin, ent->origin, model->particletrail, &cl.lerpents[s1->number].trailstate))
				P_ParticleTrailIndex(old_origin, ent->origin, model->traildefaultindex, 0, &cl.lerpents[s1->number].trailstate);
		}

		{
			extern cvar_t gl_part_flame;
			if (cls.allow_anyparticles && gl_part_flame.value)
			{
				P_EmitEffect (ent->origin, model->particleeffect, &(cl.lerpents[s1->number].emitstate));
			}
		}


		//dlights are not so customisable.
		if (r_rocketlight.value)
		{
			float rad = 0;
			vec3_t dclr;

			dclr[0] = 0.20;
			dclr[1] = 0.10;
			dclr[2] = 0;

			if (model->flags & EF_ROCKET)
			{
				if (strncmp(model->name, "models/sflesh", 13))
				{	//hmm. hexen spider gibs...
					rad = 200;
					dclr[2] = 0.05;
				}
			}
			else if (model->flags & EF_FIREBALL)
			{
				rad = 120 - (rand() % 20);
			}
			else if (model->flags & EF_ACIDBALL)
			{
				rad = 120 - (rand() % 20);
			}
			else if (model->flags & EF_SPIT)
			{
				// as far as I can tell this effect inverses the light...
				dclr[0] = -dclr[0];
				dclr[1] = -dclr[1];
				dclr[2] = -dclr[2];
				rad = 120 - (rand() % 20);
			}

			if (rad)
			{
				dl = CL_AllocDlight (s1->number);
				VectorCopy (ent->origin, dl->origin);
				dl->die = (float)cl.time;
				if (model->flags & EF_ROCKET)
					dl->origin[2] += 1; // is this even necessary
				dl->radius = rad * r_rocketlight.value;
				VectorCopy(dclr, dl->color);
			}


		}
	}
}
#endif

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
void CL_ParseProjectiles (int modelindex, qboolean nails2)
{
	int		i, c, j;
	qbyte	bits[6];
	projectile_t	*pr;

	c = MSG_ReadByte ();
	for (i=0 ; i<c ; i++)
	{
		if (nails2)
			MSG_ReadByte();
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
		pr->angles[0] = 360*((int)bits[4]>>4)/16.0f;
		pr->angles[1] = 360*(int)bits[5]/256.0f;
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
#ifdef Q3SHADERS
		ent->forcedshader = NULL;
#endif
		ent->model = cl.model_precache[pr->modelindex];
		ent->skinnum = 0;
		memset(&ent->framestate, 0, sizeof(ent->framestate));
		ent->flags = 0;
#ifdef SWQUAKE
		ent->palremap = D_IdentityRemap();
#endif
		ent->scoreboard = NULL;
#ifdef PEXT_SCALE
		ent->scale = 1;
#endif

		ent->shaderRGBAf[0] = 1;
		ent->shaderRGBAf[1] = 1;
		ent->shaderRGBAf[2] = 1;
		ent->shaderRGBAf[3] = 1;
		
		VectorCopy (pr->origin, ent->origin);
		VectorCopy (pr->angles, ent->angles);

		ent->angles[0]*=-1;
		AngleVectors(ent->angles, ent->axis[0], ent->axis[1], ent->axis[2]);
		VectorInverse(ent->axis[1]);
		ent->angles[0]*=-1;
	}
}

//========================================

extern	int		cl_spikeindex, cl_playerindex, cl_flagindex, cl_rocketindex, cl_grenadeindex;

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
extern int parsecountmod, oldparsecountmod;
extern double parsecounttime;
int lastplayerinfo;
void CL_ParsePlayerinfo (void)
{
	int			msec;
	unsigned int			flags;
	player_info_t	*info;
	player_state_t	*state, *oldstate;
	int			num;
	int			i;
	int newf;
	vec3_t		org;

	lastplayerinfo = num = MSG_ReadByte ();
	if (num >= MAX_CLIENTS)
		Host_EndGame ("CL_ParsePlayerinfo: bad num");

	info = &cl.players[num];

	oldstate = &cl.frames[oldparsecountmod].playerstate[num];
	state = &cl.frames[parsecountmod].playerstate[num];

	if (cls.demoplayback == DPB_MVD || cls.demoplayback == DPB_EZTV)
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

		if (cls.findtrack && info->stats[STAT_HEALTH] > 0)
		{
//			extern int ideal_track;
			autocam[0] = CAM_TRACK;
			Cam_Lock(0, num);
//			ideal_track = num;
			cls.findtrack = false;
		}

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
		state->scale = 1*16;
		state->alpha = 255;
		state->fatness = 0;

		state->colourmod[0] = 32;
		state->colourmod[1] = 32;
		state->colourmod[2] = 32;

		state->pm_type = PM_NORMAL;

		TP_ParsePlayerInfo(oldstate, state, info);

		if (cl.splitclients < MAX_SPLITS)
		{
			extern cvar_t cl_splitscreen;
			if (cl.splitclients < cl_splitscreen.value+1)
			{
				for (i = 0; i < cl.splitclients; i++)
					if (autocam[i] && spec_track[i] == num)
						return;

				if (i == cl.splitclients)
				{
					autocam[cl.splitclients] = CAM_TRACK;
					spec_track[cl.splitclients] = num;
					cl.splitclients++;
				}
			}
		}
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

	newf = MSG_ReadByte ();
	if (state->frame != newf)
	{
//		state->lerpstarttime = realtime;
		state->frame = newf;
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

	if (cl.worldmodel && cl.worldmodel->fromgame == fg_quake)
		state->hullnum = 1;
	else
		state->hullnum = 56;
	state->scale = 1*16;
	state->alpha = 255;
	state->fatness = 0;

#ifdef PEXT_SCALE
	if (flags & PF_SCALE_Z && cls.fteprotocolextensions & PEXT_SCALE)
		state->scale = (float)MSG_ReadByte() / 100;
#endif
#ifdef PEXT_TRANS
	if (flags & PF_TRANS_Z && cls.fteprotocolextensions & PEXT_TRANS)
		state->alpha = MSG_ReadByte();
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
	//should be passed to player move func.
#endif

	if (cls.fteprotocolextensions & PEXT_COLOURMOD && flags & PF_COLOURMOD)
	{
		state->colourmod[0] = MSG_ReadByte();
		state->colourmod[1] = MSG_ReadByte();
		state->colourmod[2] = MSG_ReadByte();
	}
	else
	{
		state->colourmod[0] = 32;
		state->colourmod[1] = 32;
		state->colourmod[2] = 32;
	}

	if (cls.z_ext & Z_EXT_PM_TYPE)
	{
		int pm_code;

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
guess_pm_type:
		if (cl.players[num].spectator)
			state->pm_type = PM_OLD_SPECTATOR;
		else if (flags & PF_DEAD)
			state->pm_type = PM_DEAD;
		else
			state->pm_type = PM_NORMAL;
	}

	if (cl.lerpplayers[num].frame != state->frame)
	{
		cl.lerpplayers[num].oldframechange = cl.lerpplayers[num].framechange;
		cl.lerpplayers[num].framechange = cl.time;
		cl.lerpplayers[num].frame = state->frame;

		//don't care about position interpolation.
	}

	TP_ParsePlayerInfo(oldstate, state, info);
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
	vec3_t	angles;
	float offs = 0;

	if (cl_flagindex == -1)
		return;

	for (i = 0; i < 2; i++)
	{
		f = 14;
		if (ent->framestate.g[FS_REG].frame[i] >= 29 && ent->framestate.g[FS_REG].frame[i] <= 40) {
			if (ent->framestate.g[FS_REG].frame[i] >= 29 && ent->framestate.g[FS_REG].frame[i] <= 34) { //axpain
				if      (ent->framestate.g[FS_REG].frame[i] == 29) f = f + 2;
				else if (ent->framestate.g[FS_REG].frame[i] == 30) f = f + 8;
				else if (ent->framestate.g[FS_REG].frame[i] == 31) f = f + 12;
				else if (ent->framestate.g[FS_REG].frame[i] == 32) f = f + 11;
				else if (ent->framestate.g[FS_REG].frame[i] == 33) f = f + 10;
				else if (ent->framestate.g[FS_REG].frame[i] == 34) f = f + 4;
			} else if (ent->framestate.g[FS_REG].frame[i] >= 35 && ent->framestate.g[FS_REG].frame[i] <= 40) { // pain
				if      (ent->framestate.g[FS_REG].frame[i] == 35) f = f + 2;
				else if (ent->framestate.g[FS_REG].frame[i] == 36) f = f + 10;
				else if (ent->framestate.g[FS_REG].frame[i] == 37) f = f + 10;
				else if (ent->framestate.g[FS_REG].frame[i] == 38) f = f + 8;
				else if (ent->framestate.g[FS_REG].frame[i] == 39) f = f + 4;
				else if (ent->framestate.g[FS_REG].frame[i] == 40) f = f + 2;
			}
		} else if (ent->framestate.g[FS_REG].frame[i] >= 103 && ent->framestate.g[FS_REG].frame[i] <= 118) {
			if      (ent->framestate.g[FS_REG].frame[i] >= 103 && ent->framestate.g[FS_REG].frame[i] <= 104) f = f + 6;  //nailattack
			else if (ent->framestate.g[FS_REG].frame[i] >= 105 && ent->framestate.g[FS_REG].frame[i] <= 106) f = f + 6;  //light
			else if (ent->framestate.g[FS_REG].frame[i] >= 107 && ent->framestate.g[FS_REG].frame[i] <= 112) f = f + 7;  //rocketattack
			else if (ent->framestate.g[FS_REG].frame[i] >= 112 && ent->framestate.g[FS_REG].frame[i] <= 118) f = f + 7;  //shotattack
		}

		offs += f + ((i==0)?(ent->framestate.g[FS_REG].lerpfrac):(1-ent->framestate.g[FS_REG].lerpfrac));
	}

	newent = CL_NewTempEntity ();
	newent->model = cl.model_precache[cl_flagindex];
	newent->skinnum = team;

	AngleVectors (ent->angles, v_forward, v_right, v_up);
	v_forward[2] = -v_forward[2]; // reverse z component
	for (i=0 ; i<3 ; i++)
		newent->origin[i] = ent->origin[i] - offs*v_forward[i] + 22*v_right[i];
	newent->origin[2] -= 16;

	VectorCopy (ent->angles, newent->angles);
	newent->angles[2] -= 45;

	VectorCopy(newent->angles, angles);
	angles[0]*=-1;
	AngleVectors(angles, newent->axis[0], newent->axis[1], newent->axis[2]);
	VectorInverse(newent->axis[1]);
}

void CL_AddVWeapModel(entity_t *player, int model)
{
	entity_t	*newent;
	vec3_t	angles;
	newent = CL_NewTempEntity ();

	VectorCopy(player->origin, newent->origin);
	VectorCopy(player->angles, newent->angles);
	newent->skinnum = player->skinnum;
	newent->model = cl.model_precache[model];
	newent->framestate = player->framestate;

	VectorCopy(newent->angles, angles);
	angles[0]*=-1;
	AngleVectors(angles, newent->axis[0], newent->axis[1], newent->axis[2]);
	VectorInverse(newent->axis[1]);
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
	frame_t			*fromf;
	int				oldphysent;
	vec3_t			angles;
	float			*org;

	playertime = realtime - cls.latency + 0.02;
	if (playertime > realtime)
		playertime = realtime;

	frame = &cl.frames[cl.validsequence&UPDATE_MASK];
	fromf = &cl.frames[cl.oldvalidsequence&UPDATE_MASK];

	for (j=0, info=cl.players, state=frame->playerstate ; j < MAX_CLIENTS
		; j++, info++, state++)
	{
		if (state->messagenum != cl.validsequence)
			continue;	// not present this frame

		// spawn light flashes, even ones coming from invisible objects
		if (r_powerupglow.value && !(r_powerupglow.value == 2 && j == cl.playernum[0]))
		{
			org = (j == cl.playernum[0]) ? cl.simorg[0] : state->origin;

			if ((state->effects & (EF_BLUE | EF_RED)) == (EF_BLUE | EF_RED))
				CL_NewDlight (j+1, org[0], org[1], org[2], 200 + (r_lightflicker.value?(rand()&31):0), 0.1, 3)->noppl = (j != cl.playernum[0]);
			else if (state->effects & EF_BLUE)
				CL_NewDlight (j+1, org[0], org[1], org[2], 200 + (r_lightflicker.value?(rand()&31):0), 0.1, 1)->noppl = (j != cl.playernum[0]);
			else if (state->effects & EF_RED)
				CL_NewDlight (j+1, org[0], org[1], org[2], 200 + (r_lightflicker.value?(rand()&31):0), 0.1, 2)->noppl = (j != cl.playernum[0]);
			else if (state->effects & EF_BRIGHTLIGHT)
				CL_NewDlight (j+1, org[0], org[1], org[2] + 16, 400 + (r_lightflicker.value?(rand()&31):0), 0.1, 0)->noppl = (j != cl.playernum[0]);
			else if (state->effects & EF_DIMLIGHT)
				CL_NewDlight (j+1, org[0], org[1], org[2], 200 + (r_lightflicker.value?(rand()&31):0), 0.1, 0)->noppl = (j != cl.playernum[0]);
		}

		if (state->modelindex < 1)
			continue;

		if (CL_FilterModelindex(state->modelindex, state->frame))
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

#ifdef Q3SHADERS
		ent->forcedshader = NULL;
#endif

		ent->model = cl.model_precache[state->modelindex];
		ent->skinnum = state->skinnum;

		ent->framestate.g[FS_REG].frametime[0] = cl.time - cl.lerpplayers[j].framechange;
		ent->framestate.g[FS_REG].frametime[1] = cl.time - cl.lerpplayers[j].oldframechange;

		if (ent->framestate.g[FS_REG].frame[0] != cl.lerpplayers[j].frame)
		{
			ent->framestate.g[FS_REG].frame[1] = ent->framestate.g[FS_REG].frame[0];
			ent->framestate.g[FS_REG].frame[0] = cl.lerpplayers[j].frame;
		}

		ent->framestate.g[FS_REG].lerpfrac = 1-(realtime - cl.lerpplayers[j].framechange)*10;
		if (ent->framestate.g[FS_REG].lerpfrac > 1)
			ent->framestate.g[FS_REG].lerpfrac = 1;
		else if (ent->framestate.g[FS_REG].lerpfrac < 0)
		{
			ent->framestate.g[FS_REG].lerpfrac = 0;
			//state->lerpstarttime = 0;
		}

#ifdef SWQUAKE
		ent->palremap = info->palremap;
#endif
		if (state->modelindex == cl_playerindex)
			ent->scoreboard = info;		// use custom skin
		else
			ent->scoreboard = NULL;

#ifdef PEXT_SCALE
		ent->scale = state->scale/16.0f;
#endif
		ent->shaderRGBAf[0] = state->colourmod[0]/32;
		ent->shaderRGBAf[1] = state->colourmod[1]/32;
		ent->shaderRGBAf[2] = state->colourmod[2]/32;
		ent->shaderRGBAf[3] = state->alpha/255;

		ent->fatness = state->fatness/2;
		//
		// angles
		//
		angles[PITCH] = -state->viewangles[PITCH]/3;
		angles[YAW] = state->viewangles[YAW];
		angles[ROLL] = 0;
		angles[ROLL] = V_CalcRoll (angles, state->velocity)*4;

		// the player object gets added with flags | 2
		for (pnum = 0; pnum < cl.splitclients; pnum++)
		{
			if (j == cl.playernum[pnum])
			{
/*				if (cl.spectator)
				{
					cl_numvisedicts--;
					continue;
				}
*/				angles[0] = -1*cl.viewangles[pnum][0] / 3;
				angles[1] = cl.viewangles[pnum][1];
				angles[2] = cl.viewangles[pnum][2];
				ent->origin[0] = cl.simorg[pnum][0];
				ent->origin[1] = cl.simorg[pnum][1];
				ent->origin[2] = cl.simorg[pnum][2]+cl.crouch[pnum];
				ent->flags |= Q2RF_EXTERNALMODEL;
				break;
			}
		}

		VectorCopy(angles, ent->angles);
		angles[0]*=-1;
		AngleVectors(angles, ent->axis[0], ent->axis[1], ent->axis[2]);
		VectorInverse(ent->axis[1]);

		// only predict half the move to minimize overruns
		msec = 500*(playertime - state->state_time);
		/*
		if (1)
		{
			float f;
			int i;
			f = (cl.gametime-cl.servertime)/(cl.gametime-cl.oldgametime);
			if (f<0)
				f=0;
			if (f>1)
				f=1;

			for (i=0 ; i<3 ; i++)
			{
				ent->origin[i] = state->origin[i] +
							f * (fromf->playerstate[j].origin[i] - state->origin[i]);
			}

		}
		else 
			*/
		if (pnum < cl.splitclients)
		{	//this is a local player
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

		if (state->effects & QWEF_FLAG1)
			CL_AddFlagModels (ent, 0);
		else if (state->effects & QWEF_FLAG2)
			CL_AddFlagModels (ent, 1);
		else if (info->vweapindex)
			CL_AddVWeapModel (ent, info->vweapindex);

	}
}

#ifdef Q3SHADERS	//fixme: do better.
#include "shader.h"
#endif

void CL_LinkViewModel(void)
{
	entity_t	ent;
//	float		ambient[4], diffuse[4];
//	int			j;
//	int			lnum;
//	vec3_t		dist;
//	float		add;
//	dlight_t	*dl;
//	int			ambientlight, shadelight;

	static struct model_s *oldmodel[MAX_SPLITS];
	static float lerptime[MAX_SPLITS];
	static int prevframe[MAX_SPLITS];
	static int oldframe[MAX_SPLITS];
	float alpha;

	extern cvar_t cl_gunx, cl_guny, cl_gunz;
	extern cvar_t cl_gunanglex, cl_gunangley, cl_gunanglez;

#ifdef SIDEVIEWS
	extern qboolean r_secondaryview;
	if (r_secondaryview==1)
		return;
#endif

	if (r_drawviewmodel.value <= 0 || !Cam_DrawViewModel(r_refdef.currentplayernum))
		return;

#ifdef Q2CLIENT
	if (cls.protocol == CP_QUAKE2)
		return;
#endif

	if (!r_drawentities.value)
		return;

	if ((cl.stats[r_refdef.currentplayernum][STAT_ITEMS] & IT_INVISIBILITY) && r_drawviewmodelinvis.value <= 0)
		return;

	if (cl.stats[r_refdef.currentplayernum][STAT_HEALTH] <= 0)
		return;

	memset(&ent, 0, sizeof(ent));

	ent.model = cl.viewent[r_refdef.currentplayernum].model;
	if (!ent.model)
		return;

	if (r_drawviewmodel.value > 0 && r_drawviewmodel.value < 1)
		alpha = r_drawviewmodel.value;
	else
		alpha = 1;

	if ((cl.stats[r_refdef.currentplayernum][STAT_ITEMS] & IT_INVISIBILITY)
		&& r_drawviewmodelinvis.value > 0
		&& r_drawviewmodelinvis.value < 1)
		alpha *= r_drawviewmodelinvis.value;

#ifdef PEXT_SCALE
	ent.scale = 1;
#endif

	ent.origin[0] = cl_gunz.value;
	ent.origin[1] = -cl_gunx.value;
	ent.origin[2] = -cl_guny.value;

	ent.angles[0] = cl_gunanglex.value;
	ent.angles[1] = cl_gunangley.value;
	ent.angles[2] = cl_gunanglez.value;

	ent.shaderRGBAf[0] = 1;
	ent.shaderRGBAf[1] = 1;
	ent.shaderRGBAf[2] = 1;
	ent.shaderRGBAf[3] = alpha;

	ent.framestate.g[FS_REG].frame[0] = cl.viewent[r_refdef.currentplayernum].framestate.g[FS_REG].frame[0];
	ent.framestate.g[FS_REG].frame[1] = oldframe[r_refdef.currentplayernum];

	if (ent.framestate.g[FS_REG].frame[0] != prevframe[r_refdef.currentplayernum])
	{
		oldframe[r_refdef.currentplayernum] = ent.framestate.g[FS_REG].frame[1] = prevframe[r_refdef.currentplayernum];
		lerptime[r_refdef.currentplayernum] = realtime;
	}
	prevframe[r_refdef.currentplayernum] = ent.framestate.g[FS_REG].frame[0];

	if (ent.model != oldmodel[r_refdef.currentplayernum])
	{
		oldmodel[r_refdef.currentplayernum] = ent.model;
		oldframe[r_refdef.currentplayernum] = ent.framestate.g[FS_REG].frame[1] = ent.framestate.g[FS_REG].frame[0];
		lerptime[r_refdef.currentplayernum] = realtime;
	}
	ent.framestate.g[FS_REG].lerpfrac = 1-(realtime-lerptime[r_refdef.currentplayernum])*10;
	ent.framestate.g[FS_REG].lerpfrac = bound(0, ent.framestate.g[FS_REG].lerpfrac, 1);

#define	Q2RF_VIEWERMODEL		2		// don't draw through eyes, only mirrors
#define	Q2RF_WEAPONMODEL		4		// only draw through eyes
#define	Q2RF_DEPTHHACK			16		// for view weapon Z crunching

	ent.flags = Q2RF_WEAPONMODEL|Q2RF_DEPTHHACK;

	V_AddEntity(&ent);

	if (!v_powerupshell.value)
		return;

	if (cl.stats[r_refdef.currentplayernum][STAT_ITEMS] & IT_QUAD)
	{
#ifdef Q3SHADERS
		if (v_powerupshell.value == 2)
		{
			ent.forcedshader = R_RegisterCustom("powerups/quadWeapon", Shader_DefaultSkinShell);
			V_AddEntity(&ent);
		}
		else
#endif
			ent.flags |= Q2RF_SHELL_BLUE;
	}
	if (cl.stats[r_refdef.currentplayernum][STAT_ITEMS] & IT_INVULNERABILITY)
	{
#ifdef Q3SHADERS
		if (v_powerupshell.value == 2)
		{
			ent.forcedshader = R_RegisterCustom("powerups/regen", Shader_DefaultSkinShell);
			ent.fatness = -2.5;
			V_AddEntity(&ent);
		}
		else
#endif
			ent.flags |= Q2RF_SHELL_RED;
	}

	if (!(ent.flags & (Q2RF_SHELL_RED|Q2RF_SHELL_GREEN|Q2RF_SHELL_BLUE)))
		return;

	ent.fatness = 0.5;
	ent.shaderRGBAf[3] /= 10;
#ifdef Q3SHADERS	//fixme: do better.
	//fixme: this is woefully gl specific. :(
	if (qrenderer == QR_OPENGL)
	{
		extern void Shader_DefaultSkinShell(char *shortname, shader_t *s);
		ent.shaderRGBAf[0] = (!!(ent.flags & Q2RF_SHELL_RED));
		ent.shaderRGBAf[1] = (!!(ent.flags & Q2RF_SHELL_GREEN));
		ent.shaderRGBAf[2] = (!!(ent.flags & Q2RF_SHELL_BLUE));
		ent.forcedshader = R_RegisterCustom("q2/shell", Shader_DefaultSkinShell);
	}
#endif

	V_AddEntity(&ent);
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

	memset(&pmove.physents[0], 0, sizeof(physent_t));
	pmove.physents[0].model = cl.worldmodel;
	VectorClear (pmove.physents[0].origin);
	pmove.physents[0].info = 0;
	pmove.numphysent = 1;

	frame = &cl.frames[parsecountmod];
	pak = &frame->packet_entities;

	for (i=0 ; i<pak->num_entities ; i++)
	{
		state = &pak->entities[i];

		if (state->modelindex <= 0)
			continue;
		if (!cl.model_precache[state->modelindex])
			continue;
		if (*cl.model_precache[state->modelindex]->name == '*' || cl.model_precache[state->modelindex]->numsubmodels)
		if ( cl.model_precache[state->modelindex]->hulls[1].firstclipnode
			|| cl.model_precache[state->modelindex]->clipbox )
		{
			memset(&pmove.physents[pmove.numphysent], 0, sizeof(physent_t));
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

	if (cl_nopred.value || cls.demoplayback)
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

		/*
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
		*/

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

		memset(pent, 0, sizeof(physent_t));
		VectorCopy(pplayer->origin, pent->origin);
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
void CL_SwapEntityLists(void)
{
	cl_oldnumvisedicts = cl_numvisedicts;
	cl_oldvisedicts = cl_visedicts;
	if (cl_visedicts == cl_visedicts_list[0])
		cl_visedicts = cl_visedicts_list[1];
	else
		cl_visedicts = cl_visedicts_list[0];
//	cl_oldvisedicts = cl_visedicts_list[(cls.netchan.incoming_sequence-1)&1];
//	cl_visedicts = cl_visedicts_list[cls.netchan.incoming_sequence&1];

	cl_numvisedicts = 0;
}

void CL_EmitEntities (void)
{
	if (cls.state != ca_active)
		return;

	CL_DecayLights ();

#ifdef Q2CLIENT
	if (cls.protocol == CP_QUAKE2)
	{
		CLQ2_AddEntities();
		return;
	}
#endif
	if (!cl.validsequence)
		return;

	CL_SwapEntityLists();

	CL_LinkPlayers ();
	CL_LinkPacketEntities ();
	CL_LinkProjectiles ();
	CL_UpdateTEnts ();
	CL_LinkViewModel ();
}










void CL_ParseClientdata (void);
/*
void MVD_Interpolate(void)
{
	player_state_t *self, *oldself;

	CL_ParseClientdata();

	self = &cl.frames[cl.parsecount & UPDATE_MASK].playerstate[cl.playernum[0]];
	oldself = &cl.frames[(cls.netchan.outgoing_sequence-1) & UPDATE_MASK].playerstate[cl.playernum[0]];
	self->messagenum = cl.parsecount;
	VectorCopy(oldself->origin, self->origin);
	VectorCopy(oldself->velocity, self->velocity);
	VectorCopy(oldself->viewangles, self->viewangles);


	cls.netchan.outgoing_sequence = cl.parsecount+1;
}

*/

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

extern float nextdemotime;
extern float olddemotime;

static void MVD_InitInterpolation(void)
{
	player_state_t *state, *oldstate;
	int i, tracknum;
	frame_t	*frame, *oldframe;
	vec3_t dist;
	struct predicted_player *pplayer;

#define ISDEAD(i) ( (i) >= 41 && (i) <= 102 )

	if (!cl.validsequence)
		 return;

//	if (nextdemotime <= olddemotime)
//		return;

	frame = &cl.frames[cl.parsecount & UPDATE_MASK];
	oldframe = &cl.frames[(cl.parsecount-1) & UPDATE_MASK];

	// clients
	for (i = 0; i < MAX_CLIENTS; i++)
	{
		pplayer = &predicted_players[i];
		state = &frame->playerstate[i];
		oldstate = &oldframe->playerstate[i];

		if (pplayer->predict)
		{
			VectorCopy(pplayer->oldo, oldstate->origin);
			VectorCopy(pplayer->olda, oldstate->command.angles);
			VectorCopy(pplayer->oldv, oldstate->velocity);
		}

		pplayer->predict = false;

		tracknum = spec_track[0];
		if ((mvd_fixangle & 1) << i)
		{
			if (i == tracknum)
			{
				state->command.angles[0] = (state->viewangles[0] = cl.viewangles[0][0])*65535/360;
				state->command.angles[1] = (state->viewangles[1] = cl.viewangles[0][1])*65535/360;
				state->command.angles[2] = (state->viewangles[2] = cl.viewangles[0][2])*65535/360;
			}

			// no angle interpolation
			VectorCopy(state->command.angles, oldstate->command.angles);

			mvd_fixangle &= ~(1 << i);
		}

		// we dont interpolate ourself if we are spectating
		if (i == cl.playernum[0] && cl.spectator)
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
/*
	// nails
	for (i = 0; i < cl_num_projectiles; i++)
	{
		if (!cl.int_projectiles[i].interpolate)
			continue;

		VectorCopy(cl.int_projectiles[i].origin, cl_projectiles[i].origin);
	}
*/
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
	extern float demtime;

	self = &cl.frames[cl.parsecount & UPDATE_MASK].playerstate[cl.playernum[0]];
	oldself = &cl.frames[(cls.netchan.outgoing_sequence - 1) & UPDATE_MASK].playerstate[cl.playernum[0]];

	self->messagenum = cl.parsecount;

	VectorCopy(oldself->origin, self->origin);
	VectorCopy(oldself->velocity, self->velocity);
	VectorCopy(oldself->viewangles, self->viewangles);

	if (old != nextdemotime)
	{
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

	f = (demtime - olddemotime) / (nextdemotime - olddemotime);
	f = bound(0, f, 1);

	// interpolate nails
/*	for (i = 0; i < cl_num_projectiles; i++)
	{
		if (!cl.int_projectiles[i].interpolate)
			continue;

		for (j = 0; j < 3; j++)
		{
			cl_projectiles[i].origin[j] = cl_oldprojectiles[cl.int_projectiles[i].oldindex].origin[j] +
				f * (cl.int_projectiles[i].origin[j] - cl_oldprojectiles[cl.int_projectiles[i].oldindex].origin[j]);
		}
	}
*/

	// interpolate clients
	for (i = 0; i < MAX_CLIENTS; i++)
	{
		pplayer = &predicted_players[i];
		state = &frame->playerstate[i];
		oldstate = &oldframe->playerstate[i];

		if (pplayer->predict)
		{
			for (j = 0; j < 3; j++)
			{
				state->viewangles[j] = MVD_AdjustAngle(oldstate->command.angles[j]/65535.0f*360, pplayer->olda[j]/65535.0f*360, f);
				state->origin[j] = oldstate->origin[j] + f * (pplayer->oldo[j] - oldstate->origin[j]);
				state->velocity[j] = oldstate->velocity[j] + f * (pplayer->oldv[j] - oldstate->velocity[j]);
			}
		}

		if (cl.lerpplayers[i].frame != state->frame)
		{
			cl.lerpplayers[i].oldframechange = cl.lerpplayers[i].framechange;
			cl.lerpplayers[i].framechange = demtime;
			cl.lerpplayers[i].frame = state->frame;
		}
	}
}

void CL_ClearPredict(void)
{
	memset(predicted_players, 0, sizeof(predicted_players));
	mvd_fixangle = 0;
}

