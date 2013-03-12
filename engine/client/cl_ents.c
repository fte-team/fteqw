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
#include "shader.h"

extern	cvar_t	cl_predict_players;
extern	cvar_t	cl_predict_players_frac;
extern	cvar_t	cl_lerp_players;
extern	cvar_t	cl_solid_players;
extern	cvar_t	cl_item_bobbing;

extern	cvar_t	r_rocketlight;
extern	cvar_t	r_lightflicker;
extern	cvar_t	cl_r2g;
extern	cvar_t	r_powerupglow;
extern	cvar_t	v_powerupshell;
extern	cvar_t	cl_nolerp;
extern	cvar_t	cl_nolerp_netquake;
extern	cvar_t	r_torch;
extern  cvar_t r_shadows;
extern	cvar_t	r_showbboxes;

extern	cvar_t	cl_gibfilter, cl_deadbodyfilter;
extern int cl_playerindex;

static struct predicted_player
{
	int flags;
	qboolean active;
	vec3_t origin;	// predicted origin

	vec3_t	oldo;
	vec3_t	olda;
	vec3_t	oldv;
	qboolean predict;
	player_state_t *oldstate;
} predicted_players[MAX_CLIENTS];

static void CL_LerpNetFrameState(int fsanim, framestate_t *fs, lerpents_t *le);
qboolean CL_PredictPlayer(lerpents_t *le, entity_state_t *state, int sequence);
void CL_PlayerFrameUpdated(player_state_t *plstate, entity_state_t *state, int sequence);


extern int cl_playerindex, cl_h_playerindex, cl_rocketindex, cl_grenadeindex, cl_gib1index, cl_gib2index, cl_gib3index;

qboolean CL_FilterModelindex(int modelindex, int frame)
{
	if (modelindex == cl_playerindex)
	{
		if (cl_deadbodyfilter.ival == 2)
		{
			if (frame >= 41 && frame <= 102)
				return true;
		}
		else if (cl_deadbodyfilter.ival)
		{
			if (frame == 49 || frame == 60 || frame == 69 || frame == 84 || frame == 93 || frame == 102)
				return true;
		}
	}

	if (cl_gibfilter.ival && (
			modelindex == cl_h_playerindex ||
			modelindex == cl_gib1index ||
			modelindex == cl_gib2index ||
			modelindex == cl_gib3index))
		return true;
	return false;
}

//============================================================

void CL_FreeDlights(void)
{
#ifdef RTLIGHTS
	int i;
	if (cl_dlights)
		for (i = 0; i < rtlights_max; i++)
			if (cl_dlights[i].worldshadowmesh)
				SH_FreeShadowMesh(cl_dlights[i].worldshadowmesh);
#endif

	rtlights_max = cl_maxdlights = 0;
	BZ_Free(cl_dlights);
	cl_dlights = NULL;
}
void CL_InitDlights(void)
{
	rtlights_max = cl_maxdlights = RTL_FIRST;
	cl_dlights = BZ_Realloc(cl_dlights, sizeof(*cl_dlights)*cl_maxdlights);
	memset(cl_dlights, 0, sizeof(*cl_dlights)*cl_maxdlights);
}

static void CL_ClearDlight(dlight_t *dl, int key)
{
	void *sm;
	sm = dl->worldshadowmesh;
	memset (dl, 0, sizeof(*dl));
	dl->rebuildcache = true;
	dl->worldshadowmesh = sm;
	dl->axis[0][0] = 1;
	dl->axis[1][1] = 1;
	dl->axis[2][2] = 1;
	dl->key = key;
	dl->flags = LFLAG_DYNAMIC;
	dl->color[0] = 1;
	dl->color[1] = 1;
	dl->color[2] = 1;
	dl->corona = r_flashblend.value * 0.25;
	dl->coronascale = r_flashblendscale.value;
	dl->lightcolourscales[0] = r_shadow_realtime_dlight_ambient.value;
	dl->lightcolourscales[1] = r_shadow_realtime_dlight_diffuse.value;
	dl->lightcolourscales[2] = r_shadow_realtime_dlight_specular.value;
//	if (r_shadow_realtime_dlight_shadowmap.value)
//		dl->flags |= LFLAG_SHADOWMAP;
}

dlight_t *CL_AllocSlight(void)
{
	dlight_t	*dl;
	if (rtlights_max == cl_maxdlights)
	{
		cl_maxdlights = rtlights_max+8;
		cl_dlights = BZ_Realloc(cl_dlights, sizeof(*cl_dlights)*cl_maxdlights);
		memset(&cl_dlights[rtlights_max], 0, sizeof(*cl_dlights)*(cl_maxdlights-rtlights_max));
	}
	dl = &cl_dlights[rtlights_max++];

	CL_ClearDlight(dl, 0);
	dl->corona = 0;
	return dl;
}

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
		dl = cl_dlights+rtlights_first;
		for (i=rtlights_first ; i<RTL_FIRST ; i++, dl++)
		{
			if (dl->key == key)
			{
				CL_ClearDlight(dl, key);
				return dl;
			}
		}
	}

	//default to the first
	dl = &cl_dlights[rtlights_first?rtlights_first-1:0];
	//try and find one that is free
	for (i=RTL_FIRST; i > rtlights_first && i > 0; )
	{
		i--;
		if (!cl_dlights[i].radius)
		{
			dl = &cl_dlights[i];
			break;
		}
	}
	if (rtlights_first > dl - cl_dlights)
		rtlights_first = dl - cl_dlights;

	CL_ClearDlight(dl, key);
	return dl;
}

/*
===============
CL_NewDlight
===============
*/
dlight_t *CL_NewDlight (int key, const vec3_t org, float radius, float time,
				   float r, float g, float b)
{
	dlight_t	*dl;

	dl = CL_AllocDlight (key);
	VectorCopy(org, dl->origin);
	dl->radius = radius;
	dl->die = cl.time + time;
	dl->color[0] = r;
	dl->color[1] = g;
	dl->color[2] = b;

	return dl;
}

dlight_t *CL_NewDlightRGB(int key, const vec3_t org, float radius, float time,
				   float r, float g, float b)
{
	return CL_NewDlight(key, org, radius, time, r*5, g*5, b*5);
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

	dl = cl_dlights+rtlights_first;
	for (i=rtlights_first ; i<RTL_FIRST ; i++, dl++)
	{
		if (!dl->radius)
		{
			continue;
		}

		if (!dl->die)
		{
			continue;
		}

		if (dl->die < (float)cl.time)
		{
			if (i==rtlights_first)
				rtlights_first++;
			dl->radius = 0;
			continue;
		}

		dl->radius -= host_frametime*dl->decay;
		if (dl->radius < 0)
		{
			if (i==rtlights_first)
				rtlights_first++;
			dl->radius = 0;
			continue;
		}

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
//int	bitcounts[32];	/// just for protocol profiling
void CLQW_ParseDelta (entity_state_t *from, entity_state_t *to, int bits, qboolean new)
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
//	for (i=0 ; i<16 ; i++)
//		if (bits&(1<<i))
//			bitcounts[i]++;

#ifdef PROTOCOLEXTENSIONS
	if (bits & U_EVENMORE && (cls.fteprotocolextensions & (PEXT_SCALE|PEXT_TRANS|PEXT_FATNESS|PEXT_HEXEN2|PEXT_COLOURMOD|PEXT_DPFLAGS|PEXT_MODELDBL|PEXT_ENTITYDBL|PEXT_ENTITYDBL2)))
		morebits = MSG_ReadByte ();
	if (morebits & U_YETMORE)
		morebits |= MSG_ReadByte()<<8;
#endif

	if ((morebits & U_ENTITYDBL) && (cls.fteprotocolextensions & PEXT_ENTITYDBL))
		to->number += 512;
	if ((morebits & U_ENTITYDBL2) && (cls.fteprotocolextensions & PEXT_ENTITYDBL2))
		to->number += 1024;

	if (bits & U_MODEL)
	{
		to->modelindex = MSG_ReadByte ();
		if (morebits & U_MODELDBL && (cls.fteprotocolextensions & PEXT_MODELDBL))
			to->modelindex += 256;
	}
	else if (morebits & U_MODELDBL && (cls.fteprotocolextensions & PEXT_MODELDBL))
		to->modelindex = MSG_ReadShort();

	if (bits & U_FRAME)
		to->frame = MSG_ReadByte ();

	if (bits & U_COLORMAP)
		to->colormap = MSG_ReadByte();

	if (bits & U_SKIN)
	{
		to->skinnum = MSG_ReadByte();
		if (to->skinnum >= 256-32) /*final 32 skins are taken as a content value instead*/
			to->skinnum = (char)to->skinnum;
	}

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

	to->solid = ES_SOLID_BSP;
	if (bits & U_SOLID)
	{
		//doesn't mean anything. solidity is infered instead.
	}

#ifdef PEXT_SCALE
	if ((morebits & U_SCALE) && (cls.fteprotocolextensions & PEXT_SCALE))
		to->scale = MSG_ReadByte();
#endif
#ifdef PEXT_TRANS
	if ((morebits & U_TRANS) && (cls.fteprotocolextensions & PEXT_TRANS))
		to->trans = MSG_ReadByte();
#endif
#ifdef PEXT_FATNESS
	if ((morebits & U_FATNESS) && (cls.fteprotocolextensions & PEXT_FATNESS))
		to->fatness = MSG_ReadChar();
#endif

	if ((morebits & U_DRAWFLAGS) && (cls.fteprotocolextensions & PEXT_HEXEN2))
		to->hexen2flags = MSG_ReadByte();
	if ((morebits & U_ABSLIGHT) && (cls.fteprotocolextensions & PEXT_HEXEN2))
		to->abslight = MSG_ReadByte();

	if ((morebits & U_COLOURMOD) && (cls.fteprotocolextensions & PEXT_COLOURMOD))
	{
		to->colormod[0] = MSG_ReadByte();
		to->colormod[1] = MSG_ReadByte();
		to->colormod[2] = MSG_ReadByte();
	}

	if (morebits & U_DPFLAGS)// && cls.fteprotocolextensions & PEXT_DPFLAGS)
	{
		// these are bits for the 'flags' field of the entity_state_t

		i = MSG_ReadByte();
		to->dpflags = i;
	}
	if (!(cls.fteprotocolextensions & PEXT_DPFLAGS))
	{
		if (to->frame)
			to->dpflags |= RENDER_STEP;
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

		CLQW_ParseDelta (&olde, &newe, word, true);
	}
}

void CLFTE_ReadDelta(unsigned int entnum, entity_state_t *news, entity_state_t *olds, entity_state_t *baseline)
{
	unsigned int bits;
	
	bits = MSG_ReadByte();
	if (bits & UF_EXTEND1)
		bits |= MSG_ReadByte()<<8;
	if (bits & UF_EXTEND2)
		bits |= MSG_ReadByte()<<16;
	if (bits & UF_EXTEND3)
		bits |= MSG_ReadByte()<<24;

	if (cl_shownet.ival >= 3)
		Con_Printf("%3i: Update %4i 0x%x\n", msg_readcount, entnum, bits);

	if (bits & UF_RESET)
	{
//		Con_Printf("%3i: Reset %i @ %i\n", msg_readcount, entnum, cls.netchan.incoming_sequence);
		*news = *baseline;
	}
	else if (!olds)
	{
		/*reset got lost, probably the data will be filled in later - FIXME: we should probably ignore this entity*/
		Con_DPrintf("New entity without reset\n");
		*news = nullentitystate;
//		*news = *baseline;
	}
	else
		*news = *olds;
	news->number = entnum;
	
	if (bits & UF_FRAME)
	{
		if (bits & UF_16BIT)
			news->frame = MSG_ReadShort();
		else
			news->frame = MSG_ReadByte();
	}

	if (bits & UF_ORIGINXY)
	{
		news->origin[0] = MSG_ReadCoord();
		news->origin[1] = MSG_ReadCoord();
	}
	if (bits & UF_ORIGINZ)
		news->origin[2] = MSG_ReadCoord();

	if (bits & UF_PREDINFO)
	{
		/*predicted stuff gets more precise angles*/
		if (bits & UF_ANGLESXZ)
		{
			news->angles[0] = MSG_ReadAngle16();
			news->angles[2] = MSG_ReadAngle16();
		}
		if (bits & UF_ANGLESY)
			news->angles[1] = MSG_ReadAngle16();
	}
	else
	{
		if (bits & UF_ANGLESXZ)
		{
			news->angles[0] = MSG_ReadAngle();
			news->angles[2] = MSG_ReadAngle();
		}
		if (bits & UF_ANGLESY)
			news->angles[1] = MSG_ReadAngle();
	}

	if ((bits & (UF_EFFECTS | UF_EFFECTS2)) == (UF_EFFECTS | UF_EFFECTS2))
		news->effects = MSG_ReadLong();
	else if (bits & UF_EFFECTS2)
		news->effects = MSG_ReadShort();
	else if (bits & UF_EFFECTS)
		news->effects = MSG_ReadByte();

	news->u.q1.movement[0] = 0;
	news->u.q1.movement[1] = 0;
	news->u.q1.movement[2] = 0;
	news->u.q1.velocity[0] = 0;
	news->u.q1.velocity[1] = 0;
	news->u.q1.velocity[2] = 0;
	if (bits & UF_PREDINFO)
	{
		unsigned int predbits;
		predbits = MSG_ReadByte();

		if (predbits & UFP_FORWARD)
			news->u.q1.movement[0] = MSG_ReadShort();
		else
			news->u.q1.movement[0] = 0;
		if (predbits & UFP_SIDE)
			news->u.q1.movement[1] = MSG_ReadShort();
		else
			news->u.q1.movement[1] = 0;
		if (predbits & UFP_UP)
			news->u.q1.movement[2] = MSG_ReadShort();
		else
			news->u.q1.movement[2] = 0;
		if (predbits & UFP_MOVETYPE)
			news->u.q1.pmovetype = MSG_ReadByte();
		if (predbits & UFP_VELOCITYXY)
		{
			news->u.q1.velocity[0] = MSG_ReadShort();
			news->u.q1.velocity[1] = MSG_ReadShort();
		}
		else
		{
			news->u.q1.velocity[0] = 0;
			news->u.q1.velocity[1] = 0;
		}
		if (predbits & UFP_VELOCITYZ)
			news->u.q1.velocity[2] = MSG_ReadShort();
		else
			news->u.q1.velocity[2] = 0;
		if (predbits & UFP_MSEC)
			news->u.q1.msec = MSG_ReadByte();
		else
			news->u.q1.msec = 0;
		if (predbits & UFP_WEAPONFRAME)
		{
			news->u.q1.weaponframe = MSG_ReadByte();
			if (news->u.q1.weaponframe & 0x80)
				news->u.q1.weaponframe = (news->u.q1.weaponframe & 127) | (MSG_ReadByte()<<7);
		}
	}
	else
		news->u.q1.msec = 0;

	if (bits & UF_MODEL)
	{
		if (bits & UF_16BIT)
			news->modelindex = MSG_ReadShort();
		else
			news->modelindex = MSG_ReadByte();
	}
	if (bits & UF_SKIN)
	{
		if (bits & UF_16BIT)
			news->skinnum = MSG_ReadShort();
		else
			news->skinnum = MSG_ReadByte();
	}
	if (bits & UF_COLORMAP)
		news->colormap = MSG_ReadByte();

	if (bits & UF_SOLID)
		news->solid = MSG_ReadShort();

	if (bits & UF_FLAGS)
		news->dpflags = MSG_ReadByte();

	if (bits & UF_ALPHA)
		news->trans = MSG_ReadByte();
	if (bits & UF_SCALE)
		news->scale = MSG_ReadByte();
	if (bits & UF_ABSLIGHT)
		news->abslight = MSG_ReadByte();
	if (bits & UF_DRAWFLAGS)
		news->hexen2flags = MSG_ReadByte();
	if (bits & UF_TAGINFO)
	{
		news->tagentity = MSGCL_ReadEntity();
		news->tagindex = MSG_ReadByte();
	}
	if (bits & UF_LIGHT)
	{
		news->light[0] = MSG_ReadShort();
		news->light[1] = MSG_ReadShort();
		news->light[1] = MSG_ReadShort();
		news->light[1] = MSG_ReadShort();
		news->lightstyle = MSG_ReadByte();
		news->lightpflags = MSG_ReadByte();
	}
	if (bits & UF_TRAILEFFECT)
		news->u.q1.traileffectnum = MSG_ReadShort();

	if (bits & UF_COLORMOD)
	{
		news->colormod[0] = MSG_ReadByte();
		news->colormod[1] = MSG_ReadByte();
		news->colormod[2] = MSG_ReadByte();
	}
	if (bits & UF_GLOW)
	{
		news->glowsize = MSG_ReadByte();
		news->glowcolour = MSG_ReadByte();
		news->glowmod[0] = MSG_ReadByte();
		news->glowmod[1] = MSG_ReadByte();
		news->glowmod[2] = MSG_ReadByte();
	}
	if (bits & UF_FATNESS)
		news->fatness = MSG_ReadByte();
	if (bits & UF_MODELINDEX2)
	{
		if (bits & UF_16BIT)
			news->modelindex2 = MSG_ReadShort();
		else
			news->modelindex2 = MSG_ReadByte();
	}
	if (bits & UF_GRAVITYDIR)
	{
		news->u.q1.gravitydir[0] = MSG_ReadByte();
		news->u.q1.gravitydir[1] = MSG_ReadByte();
	}
}

void CLFTE_ParseBaseline(entity_state_t *es, qboolean numberisimportant)
{
	int entnum = 0;
	if (numberisimportant)
		entnum = MSGCL_ReadEntity();
	CLFTE_ReadDelta(entnum, es, &nullentitystate, &nullentitystate);
}

/*
Note: strictly speaking, you don't need multiple frames, just two and flip between them.
FTE retains the full 64 frames because its interpolation will go multiple packets back in time to cover packet loss.
*/
void CLFTE_ParseEntities(void)
{
	int			oldpacket, newpacket;
	packet_entities_t	*oldp, *newp, nullp;
	unsigned int newnum, oldnum;
	int			oldindex;
	qboolean	isvalid = false;
	entity_state_t *e;
	qboolean removeflag;

//	int i;
//	for (i = cl.validsequence+1; i < cls.netchan.incoming_sequence; i++)
//	{
//		Con_Printf("CL: Dropped %i\n", i);
//	}

	newpacket = cls.netchan.incoming_sequence&UPDATE_MASK;
	oldpacket = cl.validsequence&UPDATE_MASK;
	newp = &cl.frames[newpacket].packet_entities;
	oldp = &cl.frames[oldpacket].packet_entities;
	cl.frames[newpacket].invalid = true;


	if (!cl.validsequence || cls.netchan.incoming_sequence-cl.validsequence >= UPDATE_BACKUP-1)
	{
		oldp = &nullp;
		oldp->num_entities = 0;
		oldp->max_entities = 0;
	}
	else
		isvalid = true;

	newp->servertime = MSG_ReadFloat();

	cl.oldgametime = cl.gametime;
	cl.oldgametimemark = cl.gametimemark;
	cl.gametime = newp->servertime;
	cl.gametimemark = realtime;

	/*clear all entities*/
	newp->num_entities = 0;
	oldindex = 0;
	while(1)
	{
		//high bit means remove, second high bit means 22bit index
		newnum = (unsigned short)(short)MSG_ReadShort();
		removeflag = !!(newnum & 0x8000);
		if (newnum & 0x4000)
			newnum = (newnum & 0x3fff) | (MSG_ReadByte()<<14);
		else
			newnum &= ~0x8000;

		if ((!newnum && !removeflag) || msg_badread)
		{
			/*reached the end, don't forget old entities*/
			while(oldindex < oldp->num_entities)
			{
				if (newp->num_entities >= newp->max_entities)
				{
					newp->max_entities = newp->num_entities+1;
					newp->entities = BZ_Realloc(newp->entities, sizeof(entity_state_t)*newp->max_entities);
				}
				newp->entities[newp->num_entities++] = oldp->entities[oldindex++];
			}
			break;
		}

		oldnum = (oldindex >= oldp->num_entities) ? 0xffffffff : oldp->entities[oldindex].number;

		/*if we skipped some, then they were unchanged*/
		while (newnum > oldnum)
		{
			if (newp->num_entities >= newp->max_entities)
			{
				newp->max_entities = newp->num_entities+1;
				newp->entities = BZ_Realloc(newp->entities, sizeof(entity_state_t)*newp->max_entities);
			}
			newp->entities[newp->num_entities++] = oldp->entities[oldindex++];

			oldnum = (oldindex >= oldp->num_entities) ? 0xffffffff : oldp->entities[oldindex].number;
		}

		if (removeflag)
		{
			if (cl_shownet.ival >= 3)
				Con_Printf("%3i: Remove %i @ %i\n", msg_readcount, newnum, cls.netchan.incoming_sequence);

			if (!newnum)
			{
				/*removal of world - means forget all entities*/
				if (cl_shownet.ival >= 3)
					Con_Printf("%3i: Reset all\n", msg_readcount);
				newp->num_entities = 0;
				isvalid = true;
				continue;
			}

			if (oldnum == newnum)
				oldindex++;
			continue;
		}
		else
		{
			if (!CL_CheckBaselines(newnum))
				Host_EndGame("CL_ParsePacketEntities: check baselines failed with size %i", newnum);

			if (newp->num_entities >= newp->max_entities)
			{
				newp->max_entities = newp->num_entities+1;
				newp->entities = BZ_Realloc(newp->entities, sizeof(entity_state_t)*newp->max_entities);
			}

			if (oldnum == newnum)
				CLFTE_ReadDelta(newnum, &newp->entities[newp->num_entities++], &oldp->entities[oldindex++], cl_baselines + newnum);
			else
				CLFTE_ReadDelta(newnum, &newp->entities[newp->num_entities++], NULL, cl_baselines + newnum);
		}
	}

	for (oldindex = 0; oldindex < newp->num_entities; oldindex++)
	{
		e = newp->entities + oldindex;
		if (e->number > cl.allocated_client_slots)
			break;

		/*update the prediction info if needed*/
//		if (e->u.q1.pmovetype)
		{
			frame_t *fram;
			fram = &cl.frames[cls.netchan.incoming_sequence & UPDATE_MASK];
			CL_PlayerFrameUpdated(&fram->playerstate[e->number-1], e, cls.netchan.incoming_sequence);
		}
	}

	if (isvalid)
	{
		cl.oldvalidsequence = cl.validsequence;
		cl.validsequence = cls.netchan.incoming_sequence;
		cl.ackedinputsequence = cl.validsequence;
		cl.frames[newpacket].invalid = false;
	}
	else
	{
		newp->num_entities = 0;
		cl.validsequence = 0;
	}

	/*ackedinputsequence is updated when we have new player prediction info*/
	cl.ackedinputsequence = cls.netchan.incoming_sequence;
}

/*
==================
CL_ParsePacketEntities

An svc_packetentities has just been parsed, deal with the
rest of the data stream.
==================
*/
void CLQW_ParsePacketEntities (qboolean delta)
{
	int			oldpacket, newpacket;
	packet_entities_t	*oldp, *newp, dummy;
	int			oldindex, newindex;
	int			word, newnum, oldnum;
	qboolean	full;
	int		from;

	newpacket = cls.netchan.incoming_sequence&UPDATE_MASK;
	newp = &cl.frames[newpacket].packet_entities;
	cl.frames[newpacket].invalid = false;

	if (cls.protocol == CP_QUAKEWORLD && cls.demoplayback == DPB_MVD)
	{
		extern float olddemotime;	//time from the most recent demo packet
		cl.oldgametime = cl.gametime;
		cl.oldgametimemark = cl.gametimemark;
		cl.gametime = olddemotime;
		cl.gametimemark = realtime;
	}
	else if (!(cls.fteprotocolextensions & PEXT_ACCURATETIMINGS) && cls.protocol == CP_QUAKEWORLD)
	{
		cl.oldgametime = cl.gametime;
		cl.oldgametimemark = cl.gametimemark;
		cl.gametime = realtime;//cl.frames[newpacket].senttime - cl.frames[(newpacket-1)&UPDATE_MASK].senttime;
		cl.gametimemark = realtime;
	}

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
			CLQW_ParseDelta (cl_baselines + newnum, &newp->entities[newindex], word, true);
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
			CLQW_ParseDelta (&oldp->entities[oldindex], &newp->entities[newindex], word, false);
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
		s->colormod[0] = (256)/8;
		s->colormod[1] = (256)/8;
		s->colormod[2] = (256)/8;
		s->solid = ES_SOLID_BSP;
//		s->active = true;
	}
	if (bits & E5_FLAGS)
	{
		int i = MSG_ReadByte();
		s->dpflags = i;
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
		s->tagentity = MSGCL_ReadEntity();
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
	if (bits & E5_GLOWMOD)
	{
		s->glowmod[0] = MSG_ReadByte();
		s->glowmod[1] = MSG_ReadByte();
		s->glowmod[2] = MSG_ReadByte();
	}
}

int cl_latestframenum;
void CLDP_ParseDarkPlaces5Entities(void)	//the things I do.. :o(
{
	//the incoming entities do not come in in any order. :(
	//well, they come in in order of priorities, but that's not useful to us.
	//I guess this means we'll have to go slowly.

	//dp deltas update in-place
	//this gets in the way of tracking multiple frames, and thus doesn't match fte too well


	packet_entities_t	*pack, oldpack;
	static packet_entities_t	newpack;

	entity_state_t		*to, *from;
	unsigned short read;
	int oldi, newi, lowesti, lowestv, newremaining;
	qboolean remove;

	cl_latestframenum = MSG_ReadLong(); /*server sequence to be acked*/

	if (cls.protocol_nq >= CPNQ_DP7)
		cl.ackedinputsequence = MSG_ReadLong(); /*client input sequence which has been acked*/

	cl.frames[(cls.netchan.incoming_sequence)&UPDATE_MASK].receivedtime = realtime;
	pack = &cl.frames[(cls.netchan.incoming_sequence)&UPDATE_MASK].packet_entities;
	pack->servertime = cl.gametime;
	oldpack = *pack;
	oldi = 0;

	newpack.num_entities = 0;
	for (read = MSG_ReadShort(); read!=0x8000; read = MSG_ReadShort())
	{
		if (msg_badread)
			Host_EndGame("Corrupt entitiy message packet\n");
		remove = !!(read&0x8000);
		read&=~0x8000;

		if (read >= MAX_EDICTS)
			Host_EndGame("Too many entities.\n");

		from = &defaultstate;

		for (oldi=0 ; oldi<oldpack.num_entities ; oldi++)
		{
			if (read == oldpack.entities[oldi].number)
			{
				from = &oldpack.entities[oldi];
				from->inactiveflag |= 1;	//so we don't copy it.
				break;
			}
		}

		if (remove)
		{
			continue;
		}

		if (newpack.num_entities==newpack.max_entities)
		{
			newpack.max_entities = newpack.num_entities+16;
			newpack.entities = BZ_Realloc(newpack.entities, sizeof(entity_state_t)*newpack.max_entities);
		}

		to = &newpack.entities[newpack.num_entities];
		newpack.num_entities++;

		memcpy(to, from, sizeof(*to));
		to->number = read;
		DP5_ParseDelta(to);
		to->inactiveflag &= ~1;
	}

	/*we're writing into the old one, clear it out prematurely (to make the malloc below trigger, and free it at the end)*/
	pack->max_entities = 0;
	pack->entities = NULL;

	//make sure there's enough space for both lists
	if (oldpack.num_entities + newpack.num_entities>=pack->max_entities)
	{
		pack->max_entities = oldpack.num_entities + newpack.num_entities;
		pack->entities = BZ_Realloc(pack->entities, sizeof(entity_state_t)*pack->max_entities);
	}
	pack->num_entities = 0;

	//we're read all the new states, so have current info
	//merge the packets, sorting the new ones (so the output is always sorted)
	for (oldi = 0, lowesti=0, lowestv = 0, newremaining = newpack.num_entities; newremaining || oldi < oldpack.num_entities; )
	{
		if (oldi == oldpack.num_entities)
			from = NULL;
		else
		{
			from = &oldpack.entities[oldi];
			if (from->inactiveflag & 1)
			{
				oldi++;
				continue;
			}
		}

		if (newremaining && !lowestv)
		{
			lowestv = 0x7ffffffe;
			for(newi = 0; newi < newpack.num_entities; newi++)
			{
				if (newpack.entities[newi].inactiveflag & 1)
					continue;
				if (newpack.entities[newi].number < lowestv)
				{
					lowestv = newpack.entities[newi].number;
					lowesti = newi;
				}
			}
		}

		/*use the new packet instead if we need to*/
		if (!from || (from->number > lowestv && lowestv))
		{
			from = &newpack.entities[lowesti];
			from->inactiveflag |= 1;
			lowestv = 0;	/*find the next oldest*/
			newremaining--;
		}
		else
			oldi++;

		to = &pack->entities[pack->num_entities];
		pack->num_entities++;
		memcpy(to, from, sizeof(*to));
		to->inactiveflag &= ~1;
	}

	BZ_Free(oldpack.entities);
}

void CLNQ_ParseEntity(unsigned int bits)
{
	int i;
	int num;
	entity_state_t		*state;//, *from;
	entity_state_t	*base;
	static float lasttime;
	packet_entities_t	*pack;

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
		num = MSGCL_ReadEntity ();
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

//	from = CL_FindOldPacketEntity(num);	//this could be optimised.

	if (!CL_CheckBaselines(num))
		Host_EndGame("CLNQ_ParseEntity: check baselines failed with size %i", num);
	base = cl_baselines + num;
	memcpy(state, base, sizeof(*state));

	state->number = num;
	state->solid = ES_SOLID_BSP;

	state->dpflags = (bits & NQU_NOLERP)?RENDER_STEP:0;

	if (bits & NQU_MODEL)
		state->modelindex = MSG_ReadByte ();

	if (bits & NQU_FRAME)
		state->frame = MSG_ReadByte();

	if (bits & NQU_COLORMAP)
		state->colormap = MSG_ReadByte();

	if (bits & NQU_SKIN)
		state->skinnum = MSG_ReadByte();

	if (bits & NQU_EFFECTS)
		state->effects = MSG_ReadByte();

	if (bits & NQU_ORIGIN1)
		state->origin[0] = MSG_ReadCoord ();
	if (bits & NQU_ANGLE1)
		state->angles[0] = MSG_ReadAngle();

	if (bits & NQU_ORIGIN2)
		state->origin[1] = MSG_ReadCoord ();
	if (bits & NQU_ANGLE2)
		state->angles[1] = MSG_ReadAngle();

	if (bits & NQU_ORIGIN3)
		state->origin[2] = MSG_ReadCoord ();
	if (bits & NQU_ANGLE3)
		state->angles[2] = MSG_ReadAngle();

	if (cls.protocol_nq == CPNQ_FITZ666)
	{
		if (bits & FITZU_ALPHA)
			state->trans = MSG_ReadByte();

		if (bits & RMQU_SCALE)
			state->scale = MSG_ReadByte();

		if (bits & FITZU_FRAME2)
			state->frame = (state->frame & 0xff) | (MSG_ReadByte() << 8);

		if (bits & FITZU_MODEL2)
			state->modelindex = (state->modelindex & 0xff) | (MSG_ReadByte() << 8);

		if (bits & FITZU_LERPFINISH)
			MSG_ReadByte();
	}
	else
	{
		if (bits & DPU_ALPHA)
			state->trans = MSG_ReadByte();

		if (bits & DPU_SCALE)
			state->scale = MSG_ReadByte();

		if (bits & DPU_EFFECTS2)
			state->effects |= MSG_ReadByte() << 8;

		if (bits & DPU_GLOWSIZE)
			state->glowsize = MSG_ReadByte();

		if (bits & DPU_GLOWCOLOR)
			state->glowcolour = MSG_ReadByte();

		if (bits & DPU_COLORMOD)
		{
			i = MSG_ReadByte(); // follows format RRRGGGBB
			state->colormod[0] = (qbyte)(((i >> 5) & 7) * (32.0f / 7.0f));
			state->colormod[1] = (qbyte)(((i >> 2) & 7) * (32.0f / 7.0f));
			state->colormod[2] = (qbyte)((i & 3) * (32.0f / 3.0f));
		}

		if (bits & DPU_FRAME2)
			state->frame |= MSG_ReadByte() << 8;

		if (bits & DPU_MODEL2)
			state->modelindex |= MSG_ReadByte() << 8;
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

void CL_RotateAroundTag(entity_t *ent, int entnum, int parenttagent, int parenttagnum)
{
	entity_state_t *ps;
	float *org=NULL, *ang=NULL;
	vec3_t axis[3];
	float transform[12], parent[12], result[12], old[12], temp[12];

	int model;
	framestate_t fstate;

	if (parenttagent > cl.maxlerpents)
	{
		Con_Printf("tag entity out of range!\n");
		return;
	}

	memset(&fstate, 0, sizeof(fstate));

	//for visibility checks
	ent->keynum = parenttagent;

	ps = CL_FindPacketEntity(parenttagent);
	if (ps)
	{
		if (ps->tagentity)
			CL_RotateAroundTag(ent, entnum, ps->tagentity, ps->tagindex);

		org = ps->origin;
		ang = ps->angles;
		model = ps->modelindex;

		CL_LerpNetFrameState(FS_REG, &fstate, &cl.lerpents[parenttagent]);
	}
	else
	{
		extern int parsecountmod;
//		Con_Printf("tagent %i\n", tagent);
		if (parenttagent <= MAX_CLIENTS && parenttagent > 0)
		{
			if (parenttagent == cl.playernum[0]+1)
			{
				org = cl.playerview[0].simorg;
				ang = cl.playerview[0].simangles;
			}
			else
			{
				org = cl.frames[parsecountmod].playerstate[parenttagent-1].origin;
				ang = cl.frames[parsecountmod].playerstate[parenttagent-1].viewangles;
			}
			model = cl.frames[parsecountmod].playerstate[parenttagent-1].modelindex;

			CL_LerpNetFrameState(FS_REG, &fstate, &cl.lerpplayers[parenttagent-1]);
		}
		else
		{
			CL_LerpNetFrameState(FS_REG, &fstate, &cl.lerpents[parenttagent]);
			model = 0;
		}
	}

	if (ang)
	{
		ang[0]*=-1;
		AngleVectors(ang, axis[0], axis[1], axis[2]);
		ang[0]*=-1;
		VectorInverse(axis[1]);

//		fstate.g[FS_REG].lerpfrac = CL_EntLerpFactor(tagent);
//		fstate.g[FS_REG].frametime[0] = cl.time - cl.lerpents[tagent].framechange;
//		fstate.g[FS_REG].frametime[1] = cl.time - cl.lerpents[tagent].oldframechange;

		if (Mod_GetTag(cl.model_precache[model], parenttagnum, &fstate, transform))
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

	if (cl_numvisedicts == cl_maxvisedicts)
	{
		return;		// object list is full
	}
	ent = &cl_visedicts[cl_numvisedicts];
	cl_numvisedicts++;

	*ent = *in;
}
void V_ClearEntity(entity_t *e)
{
	memset(e, 0, sizeof(*e));
	e->playerindex = -1;
	e->topcolour = TOP_DEFAULT;
	e->bottomcolour = BOTTOM_DEFAULT;
	e->h2playerclass = 0;
}
entity_t *V_AddEntity(entity_t *in)
{
	entity_t *ent;

	if (cl_numvisedicts == cl_maxvisedicts)
	{
		return NULL;		// object list is full
	}
	ent = &cl_visedicts[cl_numvisedicts];
	cl_numvisedicts++;

	*ent = *in;

	ent->angles[0]*=-1;
	AngleVectors(ent->angles, ent->axis[0], ent->axis[1], ent->axis[2]);
	VectorInverse(ent->axis[1]);
	ent->angles[0]*=-1;

	return ent;
}
/*
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
*/
int V_AddLight (int entsource, vec3_t org, float quant, float r, float g, float b)
{
	return CL_NewDlightRGB (entsource, org, quant, -0.1, r, g, b) - cl_dlights;
}

void CLQ1_AddOrientedHalfSphere(shader_t *shader, float radius, float gap, float *matrix, float r, float g, float b, float a)
{
	//use simple algo
	//a series of cylinders that gets progressively narrower
	int latsteps = 16;
	int lngsteps = 16;
	float cradius;
	int v, i, j;
	scenetris_t *t;
	vec3_t corner;
	float x,y;
	int flags = BEF_NODLIGHT|BEF_NOSHADOWS;

	if (!r && !g && !b)
		return;

	/*reuse the previous trigroup if its the same shader*/
	if (cl_numstris && cl_stris[cl_numstris-1].shader == shader && cl_stris[cl_numstris-1].flags == flags)
		t = &cl_stris[cl_numstris-1];
	else
	{
		if (cl_numstris == cl_maxstris)
		{
			cl_maxstris += 8;
			cl_stris = BZ_Realloc(cl_stris, sizeof(*cl_stris)*cl_maxstris);
		}
		t = &cl_stris[cl_numstris++];
		t->shader = shader;
		t->numidx = 0;
		t->numvert = 0;
		t->firstidx = cl_numstrisidx;
		t->firstvert = cl_numstrisvert;
		t->flags = flags;
	}

	if (cl_numstrisvert + latsteps*lngsteps > cl_maxstrisvert)
	{
		cl_maxstrisvert = cl_numstrisvert + latsteps*lngsteps;

		cl_strisvertv = BZ_Realloc(cl_strisvertv, sizeof(*cl_strisvertv)*cl_maxstrisvert);
		cl_strisvertt = BZ_Realloc(cl_strisvertt, sizeof(vec2_t)*cl_maxstrisvert);
		cl_strisvertc = BZ_Realloc(cl_strisvertc, sizeof(vec4_t)*cl_maxstrisvert);
	}
	if (cl_maxstrisidx < cl_numstrisidx+latsteps*(lngsteps-1)*6)
	{
		cl_maxstrisidx = cl_numstrisidx+latsteps*(lngsteps-1)*6 + 64;
		cl_strisidx = BZ_Realloc(cl_strisidx, sizeof(*cl_strisidx)*cl_maxstrisidx);
	}

	for (i = 0; i < latsteps; i++)
	{
		x = sin(i * 2 * M_PI / latsteps);
		y = cos(i * 2 * M_PI / latsteps);
		for (j = 0; j < lngsteps; j++)
		{
			v = i*lngsteps + j;
			cradius = sin(j * 0.5 * M_PI / (lngsteps-1))*radius;
			corner[0] = x*cradius;
			corner[1] = y*cradius;
			corner[2] = (cos(j * 0.5 * M_PI / (lngsteps-1))*-radius) - gap;
			Matrix3x4_RM_Transform3(matrix, corner, cl_strisvertv[cl_numstrisvert+v]);

			cl_strisvertt[cl_numstrisvert+v][0] = 0;
			cl_strisvertt[cl_numstrisvert+v][1] = 0;

			cl_strisvertc[cl_numstrisvert+v][0] = r;
			cl_strisvertc[cl_numstrisvert+v][1] = g;
			cl_strisvertc[cl_numstrisvert+v][2] = b;
			cl_strisvertc[cl_numstrisvert+v][3] = a;
		}
	}

	if (radius < 0)
	{
		for (i = 0; i < lngsteps-1; i++)
		{
			v = latsteps-1;
			for (v = 0; v < latsteps-1; v++)
			{
				cl_strisidx[cl_numstrisidx++] = cl_numstrisvert+lngsteps	+ v*lngsteps + i;
				cl_strisidx[cl_numstrisidx++] = cl_numstrisvert+0			+ v*lngsteps + i;
				cl_strisidx[cl_numstrisidx++] = cl_numstrisvert+1			+ v*lngsteps + i;
				cl_strisidx[cl_numstrisidx++] = cl_numstrisvert+lngsteps+1	+ v*lngsteps + i;
				cl_strisidx[cl_numstrisidx++] = cl_numstrisvert+lngsteps	+ v*lngsteps + i;
				cl_strisidx[cl_numstrisidx++] = cl_numstrisvert+1			+ v*lngsteps + i;
			}
			cl_strisidx[cl_numstrisidx++] = cl_numstrisvert					+ i;
			cl_strisidx[cl_numstrisidx++] = cl_numstrisvert+0				+ v*lngsteps + i;
			cl_strisidx[cl_numstrisidx++] = cl_numstrisvert+1				+ v*lngsteps + i;
			cl_strisidx[cl_numstrisidx++] = cl_numstrisvert+1				+ i;
			cl_strisidx[cl_numstrisidx++] = cl_numstrisvert					+ i;
			cl_strisidx[cl_numstrisidx++] = cl_numstrisvert+1				+ v*lngsteps + i;
		}
	}
	else
	{
		for (i = 0; i < lngsteps-1; i++)
		{
			v = latsteps-1;
			for (v = 0; v < latsteps-1; v++)
			{
				cl_strisidx[cl_numstrisidx++] = cl_numstrisvert+0			+ v*lngsteps + i;
				cl_strisidx[cl_numstrisidx++] = cl_numstrisvert+lngsteps	+ v*lngsteps + i;
				cl_strisidx[cl_numstrisidx++] = cl_numstrisvert+1			+ v*lngsteps + i;
				cl_strisidx[cl_numstrisidx++] = cl_numstrisvert+lngsteps	+ v*lngsteps + i;
				cl_strisidx[cl_numstrisidx++] = cl_numstrisvert+lngsteps+1	+ v*lngsteps + i;
				cl_strisidx[cl_numstrisidx++] = cl_numstrisvert+1			+ v*lngsteps + i;
			}
			cl_strisidx[cl_numstrisidx++] = cl_numstrisvert+0				+ v*lngsteps + i;
			cl_strisidx[cl_numstrisidx++] = cl_numstrisvert					+ i;
			cl_strisidx[cl_numstrisidx++] = cl_numstrisvert+1				+ v*lngsteps + i;
			cl_strisidx[cl_numstrisidx++] = cl_numstrisvert					+ i;
			cl_strisidx[cl_numstrisidx++] = cl_numstrisvert+1				+ i;
			cl_strisidx[cl_numstrisidx++] = cl_numstrisvert+1				+ v*lngsteps + i;
		}
	}

	t->numvert += lngsteps*latsteps;
	t->numidx = cl_numstrisidx - t->firstidx;
	cl_numstrisvert += lngsteps*latsteps;
}

void CLQ1_AddOrientedCylinder(shader_t *shader, float radius, float height, qboolean capsule, float *matrix, float r, float g, float b, float a)
{
	int sides = 16;
	int v;
	scenetris_t *t;
	vec3_t corner;
	int flags = BEF_NODLIGHT|BEF_NOSHADOWS;

	if (!r && !g && !b)
		return;

	radius *= 0.5;
	height *= 0.5;

	if (capsule)
		height -= radius;

	if (height > 0)
	{
		/*reuse the previous trigroup if its the same shader*/
		if (cl_numstris && cl_stris[cl_numstris-1].shader == shader && cl_stris[cl_numstris-1].flags == flags)
			t = &cl_stris[cl_numstris-1];
		else
		{
			if (cl_numstris == cl_maxstris)
			{
				cl_maxstris += 8;
				cl_stris = BZ_Realloc(cl_stris, sizeof(*cl_stris)*cl_maxstris);
			}
			t = &cl_stris[cl_numstris++];
			t->shader = shader;
			t->numidx = 0;
			t->numvert = 0;
			t->firstidx = cl_numstrisidx;
			t->firstvert = cl_numstrisvert;
			t->flags = flags;
		}

		if (cl_numstrisvert + sides*2 > cl_maxstrisvert)
		{
			cl_maxstrisvert = cl_numstrisvert + sides*2;

			cl_strisvertv = BZ_Realloc(cl_strisvertv, sizeof(*cl_strisvertv)*cl_maxstrisvert);
			cl_strisvertt = BZ_Realloc(cl_strisvertt, sizeof(vec2_t)*cl_maxstrisvert);
			cl_strisvertc = BZ_Realloc(cl_strisvertc, sizeof(vec4_t)*cl_maxstrisvert);
		}
		if (cl_maxstrisidx < cl_numstrisidx+sides*6)
		{
			cl_maxstrisidx = cl_numstrisidx+sides*6 + 64;
			cl_strisidx = BZ_Realloc(cl_strisidx, sizeof(*cl_strisidx)*cl_maxstrisidx);
		}


		for (v = 0; v < sides*2; v++)
		{
			corner[0] = sin((v>>1) * 2 * M_PI / sides)*radius;
			corner[1] = cos((v>>1) * 2 * M_PI / sides)*radius;
			corner[2] = (v & 1)?height:-height;
			Matrix3x4_RM_Transform3(matrix, corner, cl_strisvertv[cl_numstrisvert+v]);

			cl_strisvertt[cl_numstrisvert+v][0] = 0;
			cl_strisvertt[cl_numstrisvert+v][1] = 0;

			cl_strisvertc[cl_numstrisvert+v][0] = r;
			cl_strisvertc[cl_numstrisvert+v][1] = g;
			cl_strisvertc[cl_numstrisvert+v][2] = b;
			cl_strisvertc[cl_numstrisvert+v][3] = a;
		}
		for (v = 0; v < sides-1; v++)
		{
			cl_strisidx[cl_numstrisidx++] = cl_numstrisvert+2 + v*2;
			cl_strisidx[cl_numstrisidx++] = cl_numstrisvert+1 + v*2;
			cl_strisidx[cl_numstrisidx++] = cl_numstrisvert+0 + v*2;
			cl_strisidx[cl_numstrisidx++] = cl_numstrisvert+3 + v*2;
			cl_strisidx[cl_numstrisidx++] = cl_numstrisvert+1 + v*2;
			cl_strisidx[cl_numstrisidx++] = cl_numstrisvert+2 + v*2;
		}
		cl_strisidx[cl_numstrisidx++] = cl_numstrisvert+0;
		cl_strisidx[cl_numstrisidx++] = cl_numstrisvert+1 + v*2;
		cl_strisidx[cl_numstrisidx++] = cl_numstrisvert+0 + v*2;
		cl_strisidx[cl_numstrisidx++] = cl_numstrisvert+1;
		cl_strisidx[cl_numstrisidx++] = cl_numstrisvert+1 + v*2;
		cl_strisidx[cl_numstrisidx++] = cl_numstrisvert+0;

		if (!capsule)
		{
			for (v = 4; v < sides*2; v+=2)
			{
				cl_strisidx[cl_numstrisidx++] = cl_numstrisvert+v;
				cl_strisidx[cl_numstrisidx++] = cl_numstrisvert+(v-2);
				cl_strisidx[cl_numstrisidx++] = cl_numstrisvert+0;

				cl_strisidx[cl_numstrisidx++] = cl_numstrisvert+1;
				cl_strisidx[cl_numstrisidx++] = cl_numstrisvert+(v-2)+1;
				cl_strisidx[cl_numstrisidx++] = cl_numstrisvert+v+1;
			}
		}

		t->numvert += sides*2;
		t->numidx = cl_numstrisidx - t->firstidx;
		cl_numstrisvert += sides*2;
	}

	if (capsule)
	{
		CLQ1_AddOrientedHalfSphere(shader, radius, height, matrix, r, g, b, a);
		CLQ1_AddOrientedHalfSphere(shader, -radius, -height, matrix, r, g, b, a);
	}
}
void CLQ1_DrawLine(shader_t *shader, vec3_t v1, vec3_t v2, float r, float g, float b, float a)
{
	scenetris_t *t;
	int flags = BEF_NODLIGHT|BEF_NOSHADOWS|BEF_LINES;

	if (cl_numstris && cl_stris[cl_numstris-1].shader == shader && cl_stris[cl_numstris-1].flags == flags)
		t = &cl_stris[cl_numstris-1];
	else
	{
		if (cl_numstris == cl_maxstris)
		{
			cl_maxstris += 8;
			cl_stris = BZ_Realloc(cl_stris, sizeof(*cl_stris)*cl_maxstris);
		}
		t = &cl_stris[cl_numstris++];
		t->shader = shader;
		t->numidx = 0;
		t->numvert = 0;
		t->firstidx = cl_numstrisidx;
		t->firstvert = cl_numstrisvert;
		t->flags = flags;
	}
	if (cl_numstrisvert + 2 > cl_maxstrisvert)
	{
		cl_maxstrisvert = cl_numstrisvert + 2;

		cl_strisvertv = BZ_Realloc(cl_strisvertv, sizeof(*cl_strisvertv)*cl_maxstrisvert);
		cl_strisvertt = BZ_Realloc(cl_strisvertt, sizeof(vec2_t)*cl_maxstrisvert);
		cl_strisvertc = BZ_Realloc(cl_strisvertc, sizeof(vec4_t)*cl_maxstrisvert);
	}
	if (cl_maxstrisidx < cl_numstrisidx+2)
	{
		cl_maxstrisidx = cl_numstrisidx+2;
		cl_strisidx = BZ_Realloc(cl_strisidx, sizeof(*cl_strisidx)*cl_maxstrisidx);
	}

	VectorCopy(v1, cl_strisvertv[cl_numstrisvert+0]);
	cl_strisvertt[cl_numstrisvert+0][0] = 0;
	cl_strisvertt[cl_numstrisvert+0][1] = 0;
	cl_strisvertc[cl_numstrisvert+0][0] = r;
	cl_strisvertc[cl_numstrisvert+0][1] = g;
	cl_strisvertc[cl_numstrisvert+0][2] = b;
	cl_strisvertc[cl_numstrisvert+0][3] = a;

	VectorCopy(v2, cl_strisvertv[cl_numstrisvert+1]);
	cl_strisvertt[cl_numstrisvert+1][0] = 0;
	cl_strisvertt[cl_numstrisvert+1][1] = 0;
	cl_strisvertc[cl_numstrisvert+1][0] = r;
	cl_strisvertc[cl_numstrisvert+1][1] = g;
	cl_strisvertc[cl_numstrisvert+1][2] = b;
	cl_strisvertc[cl_numstrisvert+1][3] = a;

	cl_strisidx[cl_numstrisidx++] = cl_numstrisvert-t->firstvert+0;
	cl_strisidx[cl_numstrisidx++] = cl_numstrisvert-t->firstvert+1;

	t->numvert += 2;
	t->numidx = cl_numstrisidx - t->firstidx;
	cl_numstrisvert += 2;
}
void CLQ1_AddOrientedCube(shader_t *shader, vec3_t mins, vec3_t maxs, float *matrix, float r, float g, float b, float a)
{
	int v;
	scenetris_t *t;
	vec3_t corner;
	int flags = BEF_NODLIGHT|BEF_NOSHADOWS;

	if (!r && !g && !b)
		return;

	/*reuse the previous trigroup if its the same shader*/
	if (cl_numstris && cl_stris[cl_numstris-1].shader == shader && cl_stris[cl_numstris-1].flags == flags && cl_stris[cl_numstris-1].numvert + 8 <= MAX_INDICIES)
		t = &cl_stris[cl_numstris-1];
	else
	{
		if (cl_numstris == cl_maxstris)
		{
			cl_maxstris += 8;
			cl_stris = BZ_Realloc(cl_stris, sizeof(*cl_stris)*cl_maxstris);
		}
		t = &cl_stris[cl_numstris++];
		t->shader = shader;
		t->numidx = 0;
		t->numvert = 0;
		t->firstidx = cl_numstrisidx;
		t->firstvert = cl_numstrisvert;
		t->flags = flags;
	}


	if (cl_numstrisvert + 8 > cl_maxstrisvert)
	{
		cl_maxstrisvert = cl_numstrisvert + 8 + 1024;

		cl_strisvertv = BZ_Realloc(cl_strisvertv, sizeof(*cl_strisvertv)*cl_maxstrisvert);
		cl_strisvertt = BZ_Realloc(cl_strisvertt, sizeof(vec2_t)*cl_maxstrisvert);
		cl_strisvertc = BZ_Realloc(cl_strisvertc, sizeof(vec4_t)*cl_maxstrisvert);
	}
	if (cl_maxstrisidx < cl_numstrisidx+6*6)
	{
		cl_maxstrisidx = cl_numstrisidx + 6*6 + 1024;
		cl_strisidx = BZ_Realloc(cl_strisidx, sizeof(*cl_strisidx)*cl_maxstrisidx);
	}


	for (v = 0; v < 8; v++)
	{
		corner[0] = (v & 1)?mins[0]:maxs[0];
		corner[1] = (v & 2)?mins[1]:maxs[1];
		corner[2] = (v & 4)?mins[2]:maxs[2];
		if (matrix)
			Matrix3x4_RM_Transform3(matrix, corner, cl_strisvertv[cl_numstrisvert+v]);
		else
			VectorCopy(corner, cl_strisvertv[cl_numstrisvert+v]);

		cl_strisvertt[cl_numstrisvert+v][0] = 0;
		cl_strisvertt[cl_numstrisvert+v][1] = 0;

		cl_strisvertc[cl_numstrisvert+v][0] = r;
		cl_strisvertc[cl_numstrisvert+v][1] = g;
		cl_strisvertc[cl_numstrisvert+v][2] = b;
		cl_strisvertc[cl_numstrisvert+v][3] = a;
	}

	/*top*/
	cl_strisidx[cl_numstrisidx++] = cl_numstrisvert+2 - t->firstvert;
	cl_strisidx[cl_numstrisidx++] = cl_numstrisvert+1 - t->firstvert;
	cl_strisidx[cl_numstrisidx++] = cl_numstrisvert+0 - t->firstvert;
	cl_strisidx[cl_numstrisidx++] = cl_numstrisvert+3 - t->firstvert;
	cl_strisidx[cl_numstrisidx++] = cl_numstrisvert+1 - t->firstvert;
	cl_strisidx[cl_numstrisidx++] = cl_numstrisvert+2 - t->firstvert;

	/*bottom*/
	cl_strisidx[cl_numstrisidx++] = cl_numstrisvert+4 - t->firstvert;
	cl_strisidx[cl_numstrisidx++] = cl_numstrisvert+5 - t->firstvert;
	cl_strisidx[cl_numstrisidx++] = cl_numstrisvert+6 - t->firstvert;
	cl_strisidx[cl_numstrisidx++] = cl_numstrisvert+6 - t->firstvert;
	cl_strisidx[cl_numstrisidx++] = cl_numstrisvert+5 - t->firstvert;
	cl_strisidx[cl_numstrisidx++] = cl_numstrisvert+7 - t->firstvert;

	/*'left'*/
	cl_strisidx[cl_numstrisidx++] = cl_numstrisvert+5 - t->firstvert;
	cl_strisidx[cl_numstrisidx++] = cl_numstrisvert+4 - t->firstvert;
	cl_strisidx[cl_numstrisidx++] = cl_numstrisvert+0 - t->firstvert;
	cl_strisidx[cl_numstrisidx++] = cl_numstrisvert+1 - t->firstvert;
	cl_strisidx[cl_numstrisidx++] = cl_numstrisvert+5 - t->firstvert;
	cl_strisidx[cl_numstrisidx++] = cl_numstrisvert+0 - t->firstvert;

	/*right*/
	cl_strisidx[cl_numstrisidx++] = cl_numstrisvert+2 - t->firstvert;
	cl_strisidx[cl_numstrisidx++] = cl_numstrisvert+6 - t->firstvert;
	cl_strisidx[cl_numstrisidx++] = cl_numstrisvert+7 - t->firstvert;
	cl_strisidx[cl_numstrisidx++] = cl_numstrisvert+2 - t->firstvert;
	cl_strisidx[cl_numstrisidx++] = cl_numstrisvert+7 - t->firstvert;
	cl_strisidx[cl_numstrisidx++] = cl_numstrisvert+3 - t->firstvert;

	/*urm, the other way*/
	cl_strisidx[cl_numstrisidx++] = cl_numstrisvert+2 - t->firstvert;
	cl_strisidx[cl_numstrisidx++] = cl_numstrisvert+4 - t->firstvert;
	cl_strisidx[cl_numstrisidx++] = cl_numstrisvert+6 - t->firstvert;
	cl_strisidx[cl_numstrisidx++] = cl_numstrisvert+4 - t->firstvert;
	cl_strisidx[cl_numstrisidx++] = cl_numstrisvert+2 - t->firstvert;
	cl_strisidx[cl_numstrisidx++] = cl_numstrisvert+0 - t->firstvert;

	/*and its oposite*/
	cl_strisidx[cl_numstrisidx++] = cl_numstrisvert+7 - t->firstvert;
	cl_strisidx[cl_numstrisidx++] = cl_numstrisvert+5 - t->firstvert;
	cl_strisidx[cl_numstrisidx++] = cl_numstrisvert+3 - t->firstvert;
	cl_strisidx[cl_numstrisidx++] = cl_numstrisvert+1 - t->firstvert;
	cl_strisidx[cl_numstrisidx++] = cl_numstrisvert+3 - t->firstvert;
	cl_strisidx[cl_numstrisidx++] = cl_numstrisvert+5 - t->firstvert;

	t->numvert += 8;
	t->numidx = cl_numstrisidx - t->firstidx;
	cl_numstrisvert += 8;
}
#include "pr_common.h"
void CLQ1_AddVisibleBBoxes(void)
{
	world_t *w;
	wedict_t *e;
	int i;
	shader_t *s;
	extern world_t csqc_world;
	vec3_t min, max, size;

	switch(r_showbboxes.ival & 3)
	{
	default:
		return;

	#ifndef CLIENTONLY
	case 1:
		w = &sv.world;
		break;
	#endif
	#ifdef CSQC_DAT
	case 2:
		w = &csqc_world;
		break;
	#endif
	case 3:
		{
			frame_t *frame;
			packet_entities_t *pak;
			entity_state_t *state;
			model_t *mod;
			s = R_RegisterShader("bboxshader",
				"{\n"
				"polygonoffset\n"
				"{\n"
				"map $whiteimage\n"
				"blendfunc add\n"
				"rgbgen vertex\n"
				"alphagen vertex\n"
				"}\n"
				"}\n");
			frame = &cl.frames[cl.parsecount & UPDATE_MASK];
			pak = &frame->packet_entities;

			for (i=0 ; i<pak->num_entities ; i++)
			{
				state = &pak->entities[i];

				if (!state->solid && !state->skinnum)
					continue;

				if (state->solid == ES_SOLID_BSP)
				{	/*bsp model size*/
					if (state->modelindex <= 0)
						continue;
					if (!cl.model_precache[state->modelindex])
						continue;
					/*this makes non-inline bsp objects non-solid for prediction*/
					if ((*cl.model_precache[state->modelindex]->name == '*' || cl.model_precache[state->modelindex]->numsubmodels) && cl.model_precache[state->modelindex]->hulls[1].firstclipnode)
					{
						mod = cl.model_precache[state->modelindex];
						VectorAdd(state->origin, mod->mins, min);
						VectorAdd(state->origin, mod->maxs, max);
						CLQ1_AddOrientedCube(s, min, max, NULL, 0.1, 0, 0, 1);
					}
				}
				else
				{
					/*don't bother with angles*/
					max[0] = max[1] = 8*(state->solid & 31);
					min[0] = min[1] = -max[0];
					min[2] = -8*((state->solid>>5) & 31);
					max[2] = 8*((state->solid>>10) & 63) - 32;
					VectorAdd(state->origin, min, min);
					VectorAdd(state->origin, max, max);
					CLQ1_AddOrientedCube(s, min, max, NULL, 0.1, 0, 0, 1);
				}
			}
		}
		return;
	}

	if (!w->progs)
		return;
	
	s = R_RegisterShader("bboxshader",
		"{\n"
		"polygonoffset\n"
		"{\n"
		"map $whiteimage\n"
		"blendfunc add\n"
		"rgbgen vertex\n"
		"alphagen vertex\n"
		"}\n"
		"}\n");
	for (i = 1; i < w->num_edicts; i++)
	{
		e = WEDICT_NUM(w->progs, i);
		if (e->isfree)
			continue;

		if (r_showbboxes.ival & 4)
		{
			//shows the hulls instead

			/*mins is easy*/
			VectorAdd(e->v->origin, e->v->mins, min);

			/*maxs is weeeeird*/
			VectorSubtract (e->v->maxs, e->v->mins, size);
			if (size[0] < 3)
				VectorCopy(min, max);
			else if (size[0] <= 32)
			{
				max[0] = min[0] + 32;
				max[1] = min[1] + 32;
				max[2] = min[2] + 56;
			}
			else
			{
				max[0] = min[0] + 64;
				max[1] = min[1] + 64;
				max[2] = min[2] + 88;
			}
		}
		else
		{
			VectorCopy(e->v->absmin, min);
			VectorCopy(e->v->absmax, max);
		}
		CLQ1_AddOrientedCube(s, min, max, NULL, (e->v->solid || e->v->movetype)?0.1:0, (e->v->movetype == MOVETYPE_STEP || e->v->movetype == MOVETYPE_TOSS || e->v->movetype == MOVETYPE_BOUNCE)?0.1:0, ((int)e->v->flags & (FL_ONGROUND | ((e->v->movetype == MOVETYPE_STEP)?FL_FLY:0)))?0.1:0, 1);
	}
}

void CL_AddDecal(shader_t *shader, vec3_t origin, vec3_t up, vec3_t side, vec3_t rgbvalue, float alphavalue)
{
	int num, v;
	vec3_t tang;
	float radius = 1;
	float *verts;
	float tx, ty, tz;
	scenetris_t *t;
	float l, s;

	VectorNegate(up, up);
	CrossProduct(up, side, tang);

	s = sqrt(DotProduct(side, side));
	l = sqrt(DotProduct(tang, tang));

	VectorScale(tang, s/l, tang);

	num = Q1BSP_ClipDecal(origin, up, side, tang, 2, &verts);

	if (!num)
		return;
	num*=3;

	VectorScale(tang, 0.5/(s*s), tang);
	VectorScale(side, 0.5/(s*s), side);
	l = sqrt(DotProduct(up, up));
	VectorScale(up, 1/(l*l), up);

	tx = DotProduct(origin, tang) + 0.5;
	ty = DotProduct(origin, side) + 0.5;
	tz = DotProduct(origin, up);

	/*reuse the previous trigroup if its the same shader*/
	if (cl_numstris && cl_stris[cl_numstris-1].shader == shader && cl_stris[cl_numstris-1].flags == (BEF_NODLIGHT|BEF_NOSHADOWS))
		t = &cl_stris[cl_numstris-1];
	else
	{
		if (cl_numstris == cl_maxstris)
		{
			cl_maxstris += 8;
			cl_stris = BZ_Realloc(cl_stris, sizeof(*cl_stris)*cl_maxstris);
		}
		t = &cl_stris[cl_numstris++];
		t->shader = shader;
		t->numidx = 0;
		t->numvert = 0;
		t->flags = BEF_NODLIGHT|BEF_NOSHADOWS;
		t->firstidx = cl_numstrisidx;
		t->firstvert = cl_numstrisvert;
	}


	if (cl_numstrisvert + num > cl_maxstrisvert)
	{
		cl_maxstrisvert = cl_numstrisvert + num;

		cl_strisvertv = BZ_Realloc(cl_strisvertv, sizeof(*cl_strisvertv)*cl_maxstrisvert);
		cl_strisvertt = BZ_Realloc(cl_strisvertt, sizeof(vec2_t)*cl_maxstrisvert);
		cl_strisvertc = BZ_Realloc(cl_strisvertc, sizeof(vec4_t)*cl_maxstrisvert);
	}
	if (cl_maxstrisidx < cl_numstrisidx+num)
	{
		cl_maxstrisidx = cl_numstrisidx+num + 64;
		cl_strisidx = BZ_Realloc(cl_strisidx, sizeof(*cl_strisidx)*cl_maxstrisidx);
	}


	for (v = 0; v < num; v++)
	{
		VectorCopy(verts, cl_strisvertv[cl_numstrisvert+v]);
		cl_strisvertt[cl_numstrisvert+v][0] = (DotProduct(verts, tang) - tx);
		cl_strisvertt[cl_numstrisvert+v][1] = -(DotProduct(verts, side) - ty);
		cl_strisvertc[cl_numstrisvert+v][0] = rgbvalue[0];
		cl_strisvertc[cl_numstrisvert+v][1] = rgbvalue[1];
		cl_strisvertc[cl_numstrisvert+v][2] = rgbvalue[2];
		cl_strisvertc[cl_numstrisvert+v][3] = alphavalue * (1-(DotProduct(verts, up) - tz));
		verts+=3;
	}
	for (v = 0; v < num; v++)
	{
		cl_strisidx[cl_numstrisidx++] = cl_numstrisvert+v - t->firstvert;
	}

	t->numvert += num;
	t->numidx += num;
	cl_numstrisvert += num;
}

void CLQ1_AddShadow(entity_t *ent)
{
	float radius;
	vec3_t shadoworg;
	vec3_t eang;
	vec3_t axis[3];
	float tx, ty, tz;
	float *verts;
	shader_t *s;
	int v, num;
	scenetris_t *t;

	if (!r_shadows.value || !ent->model || ent->model->type != mod_alias)
		return;

	s = R_RegisterShader("shadowshader",
		"{\n"
		"polygonoffset\n"
		"{\n"
		"map $diffuse\n"
		"blendfunc blend\n"
		"rgbgen vertex\n"
		"alphagen vertex\n"
		"}\n"
		"}\n");
	TEXASSIGN(s->defaulttextures.base, balltexture);

	tx = ent->model->maxs[0] - ent->model->mins[0];
	ty = ent->model->maxs[1] - ent->model->mins[1];

	if (tx > ty)
		radius = tx;
	else
		radius = ty;
	radius/=2;

	shadoworg[0] = ent->origin[0];
	shadoworg[1] = ent->origin[1];
	shadoworg[2] = ent->origin[2] + ent->model->mins[2];

	eang[0] = 0;
	eang[1] = ent->angles[1];
	eang[2] = 0;
	AngleVectors(eang, axis[0], axis[1], axis[2]);
	VectorNegate(axis[2], axis[2]);

	num = Q1BSP_ClipDecal(shadoworg, axis[2], axis[1], axis[0], radius, &verts);

	if (!num)
		return;
	num*=3;

	tx = DotProduct(shadoworg, axis[1]) + 0.5*radius;
	ty = DotProduct(shadoworg, axis[0]) + 0.5*radius;
	tz = DotProduct(shadoworg, axis[2]);

	/*reuse the previous trigroup if its the same shader*/
	if (cl_numstris && cl_stris[cl_numstris-1].shader == s && cl_stris[cl_numstris-1].flags == (BEF_NODLIGHT|BEF_NOSHADOWS))
		t = &cl_stris[cl_numstris-1];
	else
	{
		if (cl_numstris == cl_maxstris)
		{
			cl_maxstris += 8;
			cl_stris = BZ_Realloc(cl_stris, sizeof(*cl_stris)*cl_maxstris);
		}
		t = &cl_stris[cl_numstris++];
		t->shader = s;
		t->flags = BEF_NODLIGHT|BEF_NOSHADOWS;
		t->numidx = 0;
		t->numvert = 0;
		t->firstidx = cl_numstrisidx;
		t->firstvert = cl_numstrisvert;
	}


	if (cl_numstrisvert + num > cl_maxstrisvert)
	{
		cl_maxstrisvert = cl_numstrisvert + num;

		cl_strisvertv = BZ_Realloc(cl_strisvertv, sizeof(*cl_strisvertv)*cl_maxstrisvert);
		cl_strisvertt = BZ_Realloc(cl_strisvertt, sizeof(vec2_t)*cl_maxstrisvert);
		cl_strisvertc = BZ_Realloc(cl_strisvertc, sizeof(vec4_t)*cl_maxstrisvert);
	}
	if (cl_maxstrisidx < cl_numstrisidx+num)
	{
		cl_maxstrisidx = cl_numstrisidx+num + 64;
		cl_strisidx = BZ_Realloc(cl_strisidx, sizeof(*cl_strisidx)*cl_maxstrisidx);
	}


	for (v = 0; v < num; v++)
	{
		VectorCopy(verts, cl_strisvertv[cl_numstrisvert+v]);
		cl_strisvertt[cl_numstrisvert+v][0] = (DotProduct(verts, axis[1]) - tx)/radius;
		cl_strisvertt[cl_numstrisvert+v][1] = -(DotProduct(verts, axis[0]) - ty)/radius;
		cl_strisvertc[cl_numstrisvert+v][0] = 0;
		cl_strisvertc[cl_numstrisvert+v][1] = 0;
		cl_strisvertc[cl_numstrisvert+v][2] = 0;
		cl_strisvertc[cl_numstrisvert+v][3] = r_shadows.value * (1-((DotProduct(verts, axis[2]) - tz)/(radius/2)));
		verts+=3;
	}
	for (v = 0; v < num; v++)
	{
		cl_strisidx[cl_numstrisidx++] = cl_numstrisvert+v - t->firstvert;
	}

	t->numvert += num;
	t->numidx += num;
	cl_numstrisvert += num;
}
void CLQ1_AddPowerupShell(entity_t *ent, qboolean viewweap, unsigned int effects)
{
	entity_t *shell;
	if (!(effects & (EF_BLUE | EF_RED)) || !v_powerupshell.value || !ent)
		return;

	if (cl_numvisedicts == cl_maxvisedicts)
		return;		// object list is full
	shell = &cl_visedicts[cl_numvisedicts++];

	*shell = *ent;

	/*view weapons are much closer to the screen, the scales don't work too well, so use a different shader with a smaller expansion*/
	if (viewweap)
	{
		shell->forcedshader = R_RegisterShader("powerups/shellweapon",
				"{\n"
					"program defaultpowerupshell\n"
					"sort additive\n"
					"deformVertexes wave 100 sin 0.5 0 0 0\n"
					"noshadows\n"
					"surfaceparm nodlight\n"
					"{\n"
						"map $whiteimage\n"
						"rgbgen entity\n"
						"alphagen entity\n"
						"blendfunc src_alpha one\n"
					"}\n"
				"}\n"
			);
	}
	else
	{
		shell->forcedshader = R_RegisterShader("powerups/shell",
				"{\n"
					"program defaultpowerupshell\n"
					"sort additive\n"
					"deformVertexes wave 100 sin 3 0 0 0\n"
					"noshadows\n"
					"surfaceparm nodlight\n"
					"{\n"
						"map $whiteimage\n"
						"rgbgen entity\n"
						"alphagen entity\n"
						"blendfunc src_alpha one\n"
					"}\n"
				"}\n"
			);
	}
	shell->shaderRGBAf[0] *= (effects & EF_RED)?1:0;
	shell->shaderRGBAf[1] *= 0;//(effects & EF_GREEN)?1:0;
	shell->shaderRGBAf[2] *= (effects & EF_BLUE)?1:0;
	shell->shaderRGBAf[3] *= v_powerupshell.value;
	/*let the shader do all the work*/
	shell->flags &= ~Q2RF_TRANSLUCENT|Q2RF_ADDITIVE;
}

static void CL_LerpNetFrameState(int fsanim, framestate_t *fs, lerpents_t *le)
{
	fs->g[fsanim].frame[0] = le->newframe;
	fs->g[fsanim].frame[1] = le->oldframe;

	fs->g[fsanim].frametime[0] = cl.servertime - le->newframestarttime;
	fs->g[fsanim].frametime[1] = cl.servertime - le->oldframestarttime;

	fs->g[fsanim].lerpfrac = 1-(fs->g[fsanim].frametime[0]) / le->framelerpdeltatime;
	fs->g[fsanim].lerpfrac = bound(0, fs->g[FS_REG].lerpfrac, 1);
}

static void CL_UpdateNetFrameLerpState(qboolean force, unsigned int curframe, lerpents_t *le)
{
	if (force || curframe != le->newframe)
	{
		le->framelerpdeltatime = bound(0, cl.servertime - le->newframestarttime, 0.1);	//clamp to 10 tics per second

		if (!force)
		{
			le->oldframe = le->newframe;
			le->oldframestarttime = le->newframestarttime;
		}
		else
		{
			le->oldframe = curframe;
			le->oldframestarttime = cl.servertime;
		}
		le->newframe = curframe;
		le->newframestarttime = cl.servertime;
	}
}

void CL_ClearLerpEntsParticleState(void)
{
	int i;
	for (i = 0; i < cl.maxlerpents; i++)
	{
		pe->DelinkTrailstate(&(cl.lerpents[i].trailstate));
		pe->DelinkTrailstate(&(cl.lerpents[i].emitstate));
	}
}

void CL_LinkStaticEntities(void *pvs)
{
	int i;
	entity_t *ent, *stat;
	model_t		*clmodel;
	extern cvar_t r_drawflame, gl_part_flame;

	if (r_drawflame.ival < 0)
		return;

	if (!cl.worldmodel)
		return;

	for (i = 0; i < cl.num_statics; i++)
	{
		if (cl_numvisedicts == cl_maxvisedicts)
			break;
		stat = &cl_static_entities[i].ent;

		clmodel = stat->model;
		if (!clmodel || clmodel->needload)
			continue;

		if ((!r_drawflame.ival) && (clmodel->engineflags & MDLF_FLAME))
			continue;

		/*pvs test*/
		if (pvs && !cl.worldmodel->funcs.EdictInFatPVS(cl.worldmodel, &cl_static_entities[i].pvscache, pvs))
			continue;

		ent = &cl_visedicts[cl_numvisedicts++];
		*ent = *stat;
		ent->framestate.g[FS_REG].frametime[0] = cl.time;
		ent->framestate.g[FS_REG].frametime[1] = cl.time;

	// emit particles for statics (we don't need to cheat check statics)
		if (clmodel->particleeffect >= 0 && gl_part_flame.ival)
		{
			// TODO: this is ugly.. assumes ent is in static entities, and subtracts
			// pointer math to get an index to use in cl_static emit
			// there needs to be a cleaner method for this
			P_EmitEffect(ent->origin, clmodel->particleeffect, &cl_static_entities[i].emit);
		}

//  FIXME: no effects on static ents
//		CLQ1_AddPowerupShell(ent, false, stat->effects);
	}
}

/*
===============
CL_LinkPacketEntities

===============
*/
void R_FlameTrail(vec3_t start, vec3_t end, float seperation);

/*
Interpolates the two packets by the given time, writes its results into the lerpentities array.
*/
static void CL_TransitionPacketEntities(int newsequence, packet_entities_t *newpack, packet_entities_t *oldpack, float servertime)
{
	lerpents_t		*le;
	entity_state_t		*snew, *sold;
	int					i;
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

	cl.lerpentssequence = newsequence;

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
				oldpnum++;
				if (sold->number > snew->number)
					sold = NULL;	//woo, it's a new entity.
				break;
			}

#ifdef RAGDOLL
			le = &cl.lerpents[sold->number];
			if (le->skeletalobject)
				rag_removedeltaent(le);
#endif
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
		le->sequence = newsequence;
		le->entstate = snew;

		VectorSubtract(snew->origin, sold->origin, move);
		if (DotProduct(move, move) > 200*200 || snew->modelindex != sold->modelindex)
		{
			sold = snew;	//teleported?
			VectorClear(move);
		}

		VectorCopy(le->origin, le->lastorigin);
		if (snew->u.q1.pmovetype && CL_PredictPlayer(le, snew, newsequence))
		{
			if (sold == snew)
			{
				/*keep trails correct*/
				le->isnew = true;
				VectorCopy(le->origin, le->lastorigin);
			}
		}
		else if (sold == snew)
		{
			//new this frame (or we noticed something changed significantly)
			VectorCopy(snew->origin, le->origin);
			VectorCopy(snew->angles, le->angles);

			VectorCopy(snew->origin, le->oldorigin);
			VectorCopy(snew->angles, le->oldangle);
			VectorCopy(snew->origin, le->neworigin);
			VectorCopy(snew->angles, le->newangle);

			le->orglerpdeltatime = 0.1;
			le->orglerpstarttime = oldpack->servertime;

			le->isnew = true;
			VectorCopy(le->origin, le->lastorigin);
		}
		else
		{
			if (snew->dpflags & RENDER_STEP)
			{
				float lfrac;
				//ignore the old packet entirely, except for maybe its time.
				if (!VectorEquals(le->neworigin, snew->origin) || !VectorEquals(le->newangle, snew->angles))
				{
					le->orglerpdeltatime = bound(0, oldpack->servertime - le->orglerpstarttime, 0.11);	//clamp to 10 tics per second
					le->orglerpstarttime = oldpack->servertime;

					VectorCopy(le->neworigin, le->oldorigin);
					VectorCopy(le->newangle, le->oldangle);
 
					VectorCopy(snew->origin, le->neworigin);
					VectorCopy(snew->angles, le->newangle);
				}

				lfrac = (servertime - le->orglerpstarttime) / le->orglerpdeltatime;
				lfrac = bound(0, lfrac, 1);
				for (i = 0; i < 3; i++)
				{
					le->origin[i] = le->oldorigin[i] + lfrac*(le->neworigin[i] - le->oldorigin[i]);

					a1 = le->oldangle[i];
					a2 = le->newangle[i];
					if (a1 - a2 > 180)
						a1 -= 360;
					if (a1 - a2 < -180)
						a1 += 360;
					le->angles[i] = a1 + lfrac * (a2 - a1);
				}
			}
			else
			{
				//lerp based purely on the packet times,
				for (i = 0; i < 3; i++)
				{
					le->origin[i] = sold->origin[i] + frac*(move[i]);

					a1 = sold->angles[i];
					a2 = snew->angles[i];
					if (a1 - a2 > 180)
						a1 -= 360;
					if (a1 - a2 < -180)
						a1 += 360;
					le->angles[i] = a1 + frac * (a2 - a1);
				}
				VectorCopy(le->origin, le->neworigin);
				VectorCopy(le->angles, le->newangle);
				le->orglerpdeltatime = 0.1;
				le->orglerpstarttime = oldpack->servertime;
			}
		}

		CL_UpdateNetFrameLerpState(sold == snew, snew->frame, le);
	}
}

static qboolean CL_ChooseInterpolationFrames(int *newf, int *oldf, float servertime)
{
	int i;
	float newtime = 0;
	*oldf = -1;
	*newf = -1;

	//choose the two packets.
	//we should be picking the packet just after the server time, and the one just before
	for (i = cls.netchan.incoming_sequence; i >= cls.netchan.incoming_sequence-UPDATE_MASK; i--)
	{
		if (cl.frames[i&UPDATE_MASK].receivedtime < 0 || cl.frames[i&UPDATE_MASK].invalid)
			continue;	//packetloss/choke, it's really only a problem for the oldframe, but...

		if (cl.frames[i&UPDATE_MASK].packet_entities.servertime >= servertime)
		{
			if (cl.frames[i&UPDATE_MASK].packet_entities.servertime)
			{
				if (!newtime || newtime != cl.frames[i&UPDATE_MASK].packet_entities.servertime)	//if it's a duplicate, pick the latest (so just-shot rockets are still present)
				{
					newtime = cl.frames[i&UPDATE_MASK].packet_entities.servertime;
					*newf = i;
				}
			}
		}
		else if (newtime)
		{
			if (cl.frames[i&UPDATE_MASK].packet_entities.servertime != newtime)
			{	//it does actually lerp, and isn't an identical frame.
				*oldf = i;
				break;
			}
		}
	}

	if (*newf == -1)
	{
		/*
		This can happen if the client's predicted time is greater than the most recently received packet.
		This should of course not happen...
		*/
		Con_DPrintf("Warning: No lerp-to frame packet\n");

		/*just grab the most recent frame that is valid*/
		for (i = cls.netchan.incoming_sequence; i >= cls.netchan.incoming_sequence-UPDATE_MASK; i--)
		{
			if (cl.frames[i&UPDATE_MASK].receivedtime < 0 || cl.frames[i&UPDATE_MASK].invalid)
				continue;	//packetloss/choke, it's really only a problem for the oldframe, but...
			*oldf = *newf = i;
			return true;
		}
		return false;
	}
	else if (*oldf == -1)	//can happen at map start, and really laggy games, but really shouldn't in a normal game
	{
		*oldf = *newf;
	}
	return true;
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
		return !cl_nolerp_netquake.ival;
#endif
	if (cl_nolerp.ival == 2 && cls.gamemode != GAME_DEATHMATCH)
		return true;
	return !cl_nolerp.ival;
}

/*fills in cl.lerpents and cl.currentpackentities*/
void CL_TransitionEntities (void)
{
	packet_entities_t	*packnew, *packold;
	int newf, newff, oldf;
	qboolean nolerp;
	float servertime;

	if (cls.protocol == CP_QUAKEWORLD && (cls.demoplayback == DPB_MVD || cls.demoplayback == DPB_EZTV))
	{
		nolerp = false;
	}
	else
	{
		nolerp = !CL_MayLerp() && cls.demoplayback != DPB_MVD && cls.demoplayback != DPB_EZTV;
	}

	//force our emulated time to as late as we can, if we're not using interpolation, which has the effect of disabling all interpolation
	if (nolerp)
		servertime = cl.frames[cls.netchan.incoming_sequence&UPDATE_MASK].packet_entities.servertime;
	else
		servertime = cl.servertime;

//	servertime -= 0.1;

	/*make sure we have some info for it, on failure keep the info from the last frame (its possible that the frame data can be changed by a network packet, but mneh, but chances are if there's no info then there are NO packets at all)*/
	if (!CL_ChooseInterpolationFrames(&newf, &oldf, servertime))
		return;

	newff = newf;
	newf&=UPDATE_MASK;
	oldf&=UPDATE_MASK;
	/*transition the ents and stuff*/
	packnew = &cl.frames[newf].packet_entities;
	packold = &cl.frames[oldf].packet_entities;

	CL_TransitionPacketEntities(newff, packnew, packold, servertime);
	cl.currentpacktime = servertime;
	cl.currentpackentities = packnew;


	/*and transition players too*/
	{
		float frac, a1, a2;
		int i, p;
		vec3_t move;
		lerpents_t *le;
		player_state_t *pnew, *pold;
		if (!cl_lerp_players.ival && !(cls.demoplayback == DPB_MVD || cls.demoplayback == DPB_EZTV))
		{
			newf = newff = oldf = cl.parsecount;
			newf&=UPDATE_MASK;
			oldf&=UPDATE_MASK;
		}
		if (packnew->servertime == packold->servertime)
			frac = 1; //lerp totally into the new
		else
			frac = (servertime-packold->servertime)/(packnew->servertime-packold->servertime);
		pnew = &cl.frames[newf].playerstate[0];
		pold = &cl.frames[oldf].playerstate[0];
		for (p = 0; p < cl.allocated_client_slots; p++, pnew++, pold++)
		{
			if (pnew->messagenum != newff)
			{
				continue;
			}
		
			le = &cl.lerpplayers[p];
			VectorSubtract(pnew->predorigin, pold->predorigin, move);

			if (DotProduct(move, move) > 120*120)
				frac = 1;

			//lerp based purely on the packet times,
			for (i = 0; i < 3; i++)
			{
				le->origin[i] = pold->predorigin[i] + frac*(move[i]);

				a1 = SHORT2ANGLE(pold->command.angles[i]);
				a2 = SHORT2ANGLE(pnew->command.angles[i]);
				if (a1 - a2 > 180)
					a1 -= 360;
				if (a1 - a2 < -180)
					a1 += 360;
				le->angles[i] = a1 + frac * (a2 - a1);
			}
			le->orglerpdeltatime = 0.1;
			le->orglerpstarttime = packold->servertime;
		}
	}
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
	static int flickertime;
	static int flicker;
	int trailef;

	pack = cl.currentpackentities;
	if (!pack)
		return;

	i = cl.currentpacktime*20;
	if (flickertime != i)
	{
		flickertime = i;
		flicker = rand();
	}

	autorotate = anglemod(100*cl.currentpacktime);

#ifdef CSQC_DAT
	CSQC_DeltaStart(cl.currentpacktime);
#endif

	for (newpnum=0 ; newpnum<pack->num_entities ; newpnum++)
	{
		state = &pack->entities[newpnum];



		if (cl_numvisedicts == cl_maxvisedicts)
		{
			break;
		}

#ifdef CSQC_DAT
		if (CSQC_DeltaUpdate(state))
			continue;
#endif

		ent = &cl_visedicts[cl_numvisedicts];
		ent->playerindex = -1;
		ent->topcolour = TOP_DEFAULT;
		ent->bottomcolour = BOTTOM_DEFAULT;
		ent->h2playerclass = 0;
		ent->light_known = 0;
		ent->forcedshader = NULL;

		le = &cl.lerpents[state->number];

		memset(&ent->framestate, 0, sizeof(ent->framestate));

		VectorCopy(le->origin, ent->origin);

		//bots or powerup glows. items always glow, bots can be disabled
		if (state->modelindex != cl_playerindex || r_powerupglow.ival)
		if (state->effects & (EF_BLUE | EF_RED | EF_BRIGHTLIGHT | EF_DIMLIGHT))
		{
			vec3_t colour;
			float radius;
			colour[0] = 0;
			colour[1] = 0;
			colour[2] = 0;
			radius = 0;

			if (state->effects & EF_BRIGHTLIGHT)
			{
				radius = max(radius,400);
				colour[0] += 2.0;
				colour[1] += 1.0;
				colour[2] += 0.5;
			}
			if (state->effects & EF_DIMLIGHT)
			{
				radius = max(radius,200);
				colour[0] += 2.0;
				colour[1] += 1.0;
				colour[2] += 0.5;
			}
			if (state->effects & EF_BLUE)
			{
				radius = max(radius,200);
				colour[0] += 0.5;
				colour[1] += 0.5;
				colour[2] += 3.0;
			}
			if (state->effects & EF_RED)
			{
				radius = max(radius,200);
				colour[0] += 3.0;
				colour[1] += 0.5;
				colour[2] += 0.5;
			}

			if (radius)
			{
				radius += r_lightflicker.value?((flicker + state->number)&31):0;
				CL_NewDlight(state->number, state->origin, radius, 0.1, colour[0], colour[1], colour[2]);
			}
		}
		if (state->lightpflags & PFLAGS_FULLDYNAMIC)
		{
			vec3_t colour;
			if (!state->light[0] && !state->light[1] && !state->light[2])
			{
				colour[0] = colour[1] = colour[2] = 1;
			}
			else
			{
				colour[0] = state->light[0]/1024.0f;
				colour[1] = state->light[1]/1024.0f;
				colour[2] = state->light[2]/1024.0f;
			}
			dl = CL_NewDlight(state->number, state->origin, state->light[3]?state->light[3]:350, 0.1, colour[0], colour[1], colour[2]);
			dl->corona = (state->lightpflags & PFLAGS_CORONA)?1:0;
			dl->coronascale = 0.25;
			dl->flags &= ~LFLAG_FLASHBLEND;
			dl->flags |= (state->lightpflags & PFLAGS_NOSHADOW)?LFLAG_NOSHADOWS:0;
			if (state->skinnum)
			{
				VectorCopy(angles, ent->angles);
				angles[0]*=-1;	//pflags matches alias models.
				AngleVectors(angles, dl->axis[0], dl->axis[1], dl->axis[2]);
				VectorInverse(dl->axis[1]);
				snprintf(dl->cubemapname, sizeof(dl->cubemapname), "cubemaps/%i", state->skinnum);
				dl->cubetexture = R_LoadReplacementTexture(dl->cubemapname, "", IF_CUBEMAP);
			}
		}

		// if set to invisible, skip
		if (state->modelindex<1)
			continue;

		if (CL_FilterModelindex(state->modelindex, state->frame))
			continue;

		model = cl.model_precache[state->modelindex];
		if (!model)
		{
			Con_DPrintf("Bad modelindex (%i)\n", state->modelindex);
			continue;
		}

		cl_numvisedicts++;

		ent->externalmodelview = 0;
		ent->forcedshader = NULL;

		ent->keynum = state->number;

		if (cl_r2g.value && state->modelindex == cl_rocketindex && cl_rocketindex && cl_grenadeindex)
			ent->model = cl.model_precache[cl_grenadeindex];
		else
			ent->model = model;

		ent->flags = 0;
		if (state->dpflags & RENDER_VIEWMODEL)
			ent->flags |= Q2RF_WEAPONMODEL|Q2RF_MINLIGHT|Q2RF_DEPTHHACK;
		if (state->dpflags & RENDER_EXTERIORMODEL)
			ent->flags |= Q2RF_EXTERNALMODEL;
		if (state->effects & NQEF_ADDITIVE)
			ent->flags |= Q2RF_ADDITIVE;
		if (state->effects & EF_NODEPTHTEST)
			ent->flags |= RF_NODEPTHTEST;
		if (state->effects & DPEF_NOSHADOW)
			ent->flags |= RF_NOSHADOW;
		if (state->trans != 0xff)
			ent->flags |= Q2RF_TRANSLUCENT;

		/*FIXME: pay attention to tags instead, so nexuiz can work with splitscreen*/
		if (ent->flags & Q2RF_EXTERNALMODEL)
			ent->externalmodelview = ~0;

/*		if (le->origin[2] < r_refdef.waterheight != le->lastorigin[2] < r_refdef.waterheight)
		{
			P_RunParticleEffectTypeString(le->origin, NULL, 1, "te_watertransition");
		}
*/
		// set colormap
		if (state->dpflags & RENDER_COLORMAPPED)
		{
			ent->topcolour    = (state->colormap>>4) & 0xf;
			ent->bottomcolour = (state->colormap>>0) & 0xf;
		}
		else if (state->colormap > 0 && state->colormap <= MAX_CLIENTS)
		{
			ent->topcolour    = cl.players[state->colormap-1].ttopcolor;
			ent->bottomcolour = cl.players[state->colormap-1].tbottomcolor;
		}

		// set skin
		ent->skinnum = state->skinnum;

		ent->abslight = state->abslight;
		ent->drawflags = state->hexen2flags;

		CL_LerpNetFrameState(FS_REG, &ent->framestate, le);

#ifdef PEXT_SCALE
		//set scale
		ent->scale = state->scale/16.0;
#endif
		ent->shaderRGBAf[0] = (state->colormod[0]*8.0f)/256;
		ent->shaderRGBAf[1] = (state->colormod[1]*8.0f)/256;
		ent->shaderRGBAf[2] = (state->colormod[2]*8.0f)/256;
		ent->shaderRGBAf[3] = state->trans/255.0f;

#ifdef PEXT_FATNESS
		//set trans
		ent->fatness = state->fatness/16.0;
#endif

		// rotate binary objects locally
		if (model && model->flags & MF_ROTATE)
		{
			angles[0] = 0;
			angles[1] = autorotate;
			angles[2] = 0;

			if (cl_item_bobbing.value)
				ent->origin[2] += 5+sin(cl.time*3+(state->origin[0]+state->origin[1])/8)*5.5;	//don't let it into the ground
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

		/*if this entity is in a player's slot...*/
		if (ent->keynum <= cl.allocated_client_slots)
		{
			if (!cl.nolocalplayer[0])
				ent->keynum += MAX_EDICTS;
		}

		if (state->tagentity)
		{	//ent is attached to a tag, rotate this ent accordingly.
			CL_RotateAroundTag(ent, state->number, state->tagentity, state->tagindex);
		}

		CLQ1_AddShadow(ent);
		CLQ1_AddPowerupShell(ent, false, state->effects);

		if (r_torch.ival && ent->keynum <= cl.allocated_client_slots)
		{
			dlight_t *dl;
			dl = CL_NewDlight(ent->keynum, ent->origin, 300, r_torch.ival, 0.9, 0.9, 0.6);
			dl->flags |= LFLAG_SHADOWMAP|LFLAG_FLASHBLEND;
			dl->fov = 90;
			angles[0] *= 3;
//			angles[1] += sin(realtime)*8;
//			angles[0] += cos(realtime*1.13)*5;
			AngleVectors(angles, dl->axis[0], dl->axis[1], dl->axis[2]);

			VectorMA(dl->origin, 16, dl->axis[0], dl->origin);
		}

#ifdef RAGDOLL
		if (ent->model->dollinfo)
			rag_updatedeltaent(ent, le);
#endif

		// add automatic particle trails
		if (!model || (!(model->flags&~MF_ROTATE) && model->particletrail<0 && model->particleeffect<0 && state->u.q1.traileffectnum==0))
			continue;

		if (!cls.allow_anyparticles && !(model->flags & ~MF_ROTATE))
			continue;

		if (le->isnew)
		{
			le->isnew = false;
			pe->DelinkTrailstate(&(cl.lerpents[state->number].trailstate));
			pe->DelinkTrailstate(&(cl.lerpents[state->number].emitstate));
			continue;		// not in last message
		}

		VectorCopy(le->lastorigin, old_origin);
		for (i=0 ; i<3 ; i++)
		{
			if ( abs(old_origin[i] - ent->origin[i]) > 128)
			{	// no trail if too far
				VectorCopy (ent->origin, old_origin);
				break;
			}
		}

		trailef = model->particletrail;
		if (state->u.q1.traileffectnum)
			trailef = CL_TranslateParticleFromServer(state->u.q1.traileffectnum);

		if (trailef == P_INVALID || pe->ParticleTrail (old_origin, ent->origin, trailef, ent->keynum, &(le->trailstate)))
			if (model->traildefaultindex >= 0)
				pe->ParticleTrailIndex(old_origin, ent->origin, model->traildefaultindex, 0, &(le->trailstate));

		{
			extern cvar_t gl_part_flame;
			if (model->particleeffect != P_INVALID && cls.allow_anyparticles && gl_part_flame.ival)
			{
				P_EmitEffect (ent->origin, model->particleeffect, &(le->emitstate));
			}
		}

		//dlights are not so customisable.
		if (r_rocketlight.value)
		{
			float rad = 0;
			vec3_t dclr;

			dclr[0] = 2.0;
			dclr[1] = 1.0;
			dclr[2] = 0.25;

			if (model->flags & MF_ROCKET)
			{
#ifdef warningmsg
#pragma warningmsg("Replace this flag on load for hexen2 models")
#endif
				if (strncmp(model->name, "models/sflesh", 13))
				{	//hmm. hexen spider gibs...
					rad = 200;
					rad += r_lightflicker.value?((flicker + state->number)&31):0;
				}
			}
			else if (model->flags & MFH2_FIREBALL)
			{
				rad = 120 - (rand() % 20);
			}
			else if (model->flags & MFH2_ACIDBALL)
			{
				rad = 120 - (rand() % 20);
				dclr[0] = 0.1;
				dclr[1] = 0.2;
			}
			else if (model->flags & MFH2_SPIT)
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
				memcpy(dl->axis, ent->axis, sizeof(dl->axis));
				VectorCopy (ent->origin, dl->origin);
				dl->die = (float)cl.time;
				if (model->flags & MF_ROCKET)
					dl->origin[2] += 1; // is this even necessary
				dl->radius = rad * r_rocketlight.value;
				VectorCopy(dclr, dl->color);
			}


		}
	}
#ifdef CSQC_DAT
	CSQC_DeltaEnd();
#endif

	CLQ1_AddVisibleBBoxes();
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
		pr->angles[0] = 360*(((int)bits[4]>>4)/16.0f + 1/32.0f);
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
		if (cl_numvisedicts == cl_maxvisedicts)
			break;		// object list is full
		ent = &cl_visedicts[cl_numvisedicts];
		cl_numvisedicts++;
		ent->light_known = 0;
		ent->keynum = 0;

		if (pr->modelindex < 1)
			continue;

		ent->forcedshader = NULL;
		ent->model = cl.model_precache[pr->modelindex];
		ent->skinnum = 0;
		memset(&ent->framestate, 0, sizeof(ent->framestate));
		ent->flags = 0;
		ent->playerindex = -1;
		ent->topcolour = TOP_DEFAULT;
		ent->bottomcolour = BOTTOM_DEFAULT;
		ent->h2playerclass = 0;
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

void CL_ParseClientdata (void);
void CL_MVDUpdateSpectator(void)
{
	player_state_t *self, *oldself;
	int s;
	for (s = 0; s < cl.splitclients; s++)
	{
		self = &cl.frames[cl.parsecount & UPDATE_MASK].playerstate[cl.playernum[s]];
		oldself = &cl.frames[(cls.netchan.outgoing_sequence - 1) & UPDATE_MASK].playerstate[cl.playernum[s]];
//		cl.frames[cl.parsecount & UPDATE_MASK].senttime = cl.frames[(cls.netchan.outgoing_sequence - 1) & UPDATE_MASK].senttime;

//		self->messagenum = cl.parsecount;

//		VectorCopy(oldself->origin, self->origin);
//		VectorCopy(oldself->velocity, self->velocity);
//		VectorCopy(oldself->viewangles, self->viewangles);
	}

	CL_ParseClientdata();
}


void CL_ParsePlayerinfo (void)
{
	int			msec;
	unsigned int			flags;
	player_info_t	*info;
	player_state_t	*state, *oldstate;
	int			num;
	int			i;
	int newf;
	vec3_t		org, dist;

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

		VectorSubtract(state->origin, prevstate->origin, dist);
		VectorScale(dist, 1/(cl.frames[parsecountmod].packet_entities.servertime - cl.frames[oldparsecountmod].packet_entities.servertime), state->velocity);
		VectorCopy (state->origin, state->predorigin);

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

		VectorSet(state->szmins, -16, -16, -24);
		VectorSet(state->szmaxs, 16, 16, 32);
		state->scale = 1;
		state->alpha = 255;
		state->fatness = 0;

		state->colourmod[0] = 32;
		state->colourmod[1] = 32;
		state->colourmod[2] = 32;

		state->gravitydir[0] = 0;
		state->gravitydir[1] = 0;
		state->gravitydir[2] = -1;

		state->pm_type = PM_NORMAL;

		TP_ParsePlayerInfo(oldstate, state, info);


		//can't CL_SetStatInt as we don't know if its actually us or not
		cl.players[num].stats[STAT_WEAPONFRAME] = state->weaponframe;
		cl.players[num].statsf[STAT_WEAPONFRAME] = state->weaponframe;
		for (i = 0; i < cl.splitclients; i++)
		{
			if (spec_track[i] == num)
			{
				cl.playerview[i].stats[STAT_WEAPONFRAME] = state->weaponframe;
				cl.playerview[i].statsf[STAT_WEAPONFRAME] = state->weaponframe;
			}
		}

		if (cl.splitclients < MAX_SPLITS && !cl.players[num].spectator)
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
					Cam_Lock(i, num);
				}
			}
		}
		return;
	}

	flags = (unsigned short)MSG_ReadShort ();

	if (cls.fteprotocolextensions & (PEXT_HULLSIZE|PEXT_TRANS|PEXT_SCALE|PEXT_FATNESS))
		if (flags & PF_EXTRA_PFS)
			flags |= MSG_ReadByte()<<16;

	state->flags = flags;

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
	{
		msec = 0;
		state->state_time = parsecounttime;
	}

	if (flags & PF_COMMAND)
	{
		MSG_ReadDeltaUsercmd (&nullcmd, &state->command);

		state->viewangles[0] = state->command.angles[0] * (360.0/65536);
		state->viewangles[1] = state->command.angles[1] * (360.0/65536);
		state->viewangles[2] = state->command.angles[2] * (360.0/65536);

		if (!(cls.z_ext & Z_EXT_VWEP))
			state->command.impulse = 0;
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

	VectorSet(state->szmins, -16, -16, -24);
	VectorSet(state->szmaxs, 16, 16, 32);
	state->scale = 1;
	state->alpha = 255;
	state->fatness = 0;

	state->gravitydir[0] = 0;
	state->gravitydir[1] = 0;
	state->gravitydir[2] = -1;

#ifdef PEXT_SCALE
	if (flags & PF_SCALE && cls.fteprotocolextensions & PEXT_SCALE)
		state->scale = (float)MSG_ReadByte()/50;
#endif
#ifdef PEXT_TRANS
	if (flags & PF_TRANS && cls.fteprotocolextensions & PEXT_TRANS)
		state->alpha = MSG_ReadByte();
#endif
#ifdef PEXT_FATNESS
	if (flags & PF_FATNESS && cls.fteprotocolextensions & PEXT_FATNESS)
		state->fatness = (float)MSG_ReadChar();
#endif
#ifdef PEXT_HULLSIZE
	if ((cls.fteprotocolextensions & PEXT_HULLSIZE) && (flags & PF_HULLSIZE_Z))
	{
		int num;
		num = MSG_ReadByte();

		if (!cl.worldmodel || cl.worldmodel->fromgame != fg_quake)
		{
			VectorScale(state->szmins, num/56.0f, state->szmins);
			VectorScale(state->szmaxs, num/56.0f, state->szmaxs);
		}
		else
		{
			VectorCopy(cl.worldmodel->hulls[num&(MAX_MAP_HULLSM-1)].clip_mins, state->szmins);
			VectorCopy(cl.worldmodel->hulls[num&(MAX_MAP_HULLSM-1)].clip_maxs, state->szmaxs);
		}
		if (num & 128)
		{	//this hack is for hexen2.
			state->szmaxs[2] -= state->szmins[2];
			state->szmins[2] = 0;
		}
	}
	//should be passed to player move func.
#endif

	if (cls.fteprotocolextensions & PEXT_COLOURMOD && (flags & PF_COLOURMOD))
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
				else if (pm_code == PMC_WALLWALK)
					state->pm_type = PM_WALLWALK;
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

	TP_ParsePlayerInfo(oldstate, state, info);

	//can't CL_SetStatInt as we don't know if its actually us or not
	for (i = 0; i < cl.splitclients; i++)
	{
		if (spec_track[i] == num)
		{
			cl.playerview[i].stats[STAT_WEAPONFRAME] = state->weaponframe;
			cl.playerview[i].statsf[STAT_WEAPONFRAME] = state->weaponframe;
		}
	}

	if (cl.worldmodel && cl_lerp_players.ival)
	{
		player_state_t exact;
		msec += cls.latency*1000;
//		msec = 1000*((realtime - cls.latency + 0.02) - state->state_time);
		// predict players movement
		if (msec > 255)
			msec = 255;
		state->command.msec = msec;

		CL_SetSolidEntities();
		CL_SetSolidPlayers();
		CL_PredictUsercmd (0, num+1, state, &exact, &state->command);	//uses player 0's maxspeed/grav...
		VectorCopy (exact.origin, state->predorigin);
	}
	else
		VectorCopy (state->origin, state->predorigin);
}

/*
void CL_ParseClientPersist(void)
{
	player_info_t	*info;
	int flags;
	flags = MSG_ReadShort();
	info = &cl.players[lastplayerinfo];
	if (flags & 1)
		info->vweapindex = MSG_ReadShort();
}
*/

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

void CL_AddVWeapModel(entity_t *player, model_t *model)
{
	entity_t	*newent;
	vec3_t	angles;
	if (!model)
		return;
	newent = CL_NewTempEntity ();

	newent->keynum = player->keynum;

	VectorCopy(player->origin, newent->origin);
	VectorCopy(player->angles, newent->angles);
	newent->skinnum = player->skinnum;
	newent->model = model;
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
vec3_t nametagorg[MAX_CLIENTS];
qboolean nametagseen[MAX_CLIENTS];
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
	qboolean		predictplayers;
	model_t			*model;
	static int		flickertime;
	static int		flicker;
	float			predictmsmult = 1000*cl_predict_players_frac.value;

	if (!cl.worldmodel || cl.worldmodel->needload)
		return;

	playertime = realtime - cls.latency + 0.02;
	if (playertime > realtime)
		playertime = realtime;

	frame = &cl.frames[cl.validsequence&UPDATE_MASK];
	fromf = &cl.frames[cl.oldvalidsequence&UPDATE_MASK];

	predictplayers = cl_predict_players.ival;
	if (cls.demoplayback == DPB_MVD || cls.demoplayback == DPB_EZTV)
		predictplayers = false;

	for (j=0, info=cl.players, state=frame->playerstate ; j < MAX_CLIENTS
		; j++, info++, state++)
	{
		nametagseen[j] = false;

		if (state->messagenum != cl.validsequence)
		{
#ifdef CSQC_DAT
			CSQC_DeltaPlayer(j, NULL);
#endif
			continue;	// not present this frame
		}

		CL_UpdateNetFrameLerpState(false, state->frame, &cl.lerpplayers[j]);

#ifdef CSQC_DAT
		if (CSQC_DeltaPlayer(j, state))
			continue;
#endif

		if (info->spectator)
			continue;

		//the extra modelindex check is to stop lame mods from using vweps with rings
		if (state->command.impulse && cl.model_precache_vwep[0] && state->modelindex == cl_playerindex)
			model = cl.model_precache_vwep[0];
		else
			model = cl.model_precache[state->modelindex];

		// spawn light flashes, even ones coming from invisible objects
		if (r_powerupglow.value && !(r_powerupglow.value == 2 && j == cl.playernum[0])
			&& (state->effects & (EF_BLUE|EF_RED|EF_BRIGHTLIGHT|EF_DIMLIGHT)))
		{
			vec3_t colour;
			float radius;
			colour[0] = 0;
			colour[1] = 0;
			colour[2] = 0;
			radius = 0;

			if (state->effects & EF_BRIGHTLIGHT)
			{
				radius = max(radius,400);
				colour[0] += 0.2;
				colour[1] += 0.1;
				colour[2] += 0.05;
			}
			if (state->effects & EF_DIMLIGHT)
			{
				radius = max(radius,200);
				colour[0] += 2.0;
				colour[1] += 1.0;
				colour[2] += 0.5;
			}
			if (state->effects & EF_BLUE)
			{
				radius = max(radius,200);
				colour[0] += 0.5;
				colour[1] += 0.5;
				colour[2] += 3.0;
			}
			if (state->effects & EF_RED)
			{
				radius = max(radius,200);
				colour[0] += 5.0;
				colour[1] += 0.5;
				colour[2] += 0.5;
			}

			if (radius)
			{
				vec3_t org;
				VectorCopy(state->origin, org);
				//make the light appear at the predicted position rather than anywhere else.
				for (pnum = 0; pnum < cl.splitclients; pnum++)
					if (cl.playernum[pnum] == j)
						VectorCopy(cl.playerview[pnum].simorg, org);
				if (model)
				{
					org[2] += model->mins[2];
					org[2] += 32;
				}
				if (r_lightflicker.value)
				{
					pnum = realtime*20;
					if (flickertime != pnum)
					{
						flickertime = pnum;
						flicker = rand();
					}
					radius += (flicker+j)&31;
				}
				CL_NewDlight(j+1, org, radius, 0.1, colour[0], colour[1], colour[2])->flags &= ~LFLAG_FLASHBLEND;
			}
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
		if (cl_numvisedicts == cl_maxvisedicts)
			break;		// object list is full
		ent = &cl_visedicts[cl_numvisedicts];
		cl_numvisedicts++;
		ent->light_known = 0;
		ent->keynum = j+1;
		ent->flags = 0;
		ent->model = model;
		ent->forcedshader = NULL;

		ent->skinnum = state->skinnum;

		CL_LerpNetFrameState(FS_REG, &ent->framestate,	&cl.lerpplayers[j]);

		// set colormap
		ent->playerindex = j;
		ent->topcolour	  = info->ttopcolor;
		ent->bottomcolour = info->tbottomcolor;
		ent->h2playerclass = info->h2playerclass;

#ifdef PEXT_SCALE
		ent->scale = state->scale;
#endif
		ent->shaderRGBAf[0] = state->colourmod[0]/32.0f;
		ent->shaderRGBAf[1] = state->colourmod[1]/32.0f;
		ent->shaderRGBAf[2] = state->colourmod[2]/32.0f;
		ent->shaderRGBAf[3] = state->alpha/255.0f;
		if (state->alpha != 255)
			ent->flags |= Q2RF_TRANSLUCENT;

		ent->fatness = state->fatness;
		//
		// angles
		//
		angles[PITCH] = -state->viewangles[PITCH]/3;
		angles[YAW] = state->viewangles[YAW];
		angles[ROLL] = 0;
		angles[ROLL] = V_CalcRoll (angles, state->velocity)*4;

		ent->externalmodelview = 0;
		// the player object gets added with flags | 2
		for (pnum = 0; pnum < cl.splitclients; pnum++)
		{
			if (j == (cl.viewentity[pnum]?cl.viewentity[pnum]:cl.playernum[pnum]))
			{
				ent->flags |= Q2RF_EXTERNALMODEL;
				ent->externalmodelview |= (1<<pnum);
			}
			if (j == cl.playernum[pnum])
			{
/*				if (cl.spectator)
				{
					cl_numvisedicts--;
					continue;
				}
*/				angles[0] = -1*cl.playerview[pnum].viewangles[0] / 3;
				angles[1] = cl.playerview[pnum].viewangles[1];
				angles[2] = cl.playerview[pnum].viewangles[2];
				ent->origin[0] = cl.playerview[pnum].simorg[0];
				ent->origin[1] = cl.playerview[pnum].simorg[1];
				ent->origin[2] = cl.playerview[pnum].simorg[2]+cl.crouch[pnum];
				break;
			}
		}

		VectorCopy(angles, ent->angles);
		angles[0]*=-1;
		AngleVectors(angles, ent->axis[0], ent->axis[1], ent->axis[2]);
		VectorInverse(ent->axis[1]);

		// only predict half the move to minimize overruns
		msec = predictmsmult*(playertime - state->state_time);

		if (pnum < cl.splitclients)
		{	//this is a local player
		}
		else if (cl_lerp_players.ival || (cls.demoplayback==DPB_MVD || cls.demoplayback == DPB_EZTV))
		{
			lerpents_t *le = &cl.lerpplayers[j];
			VectorCopy (le->origin, ent->origin);

			VectorCopy(le->angles, ent->angles);
			ent->angles[0] /= 3;
			AngleVectors(ent->angles, ent->axis[0], ent->axis[1], ent->axis[2]);
			VectorInverse(ent->axis[1]);
		}
		else if (msec <= 0 || (!predictplayers))
		{
			VectorCopy (state->origin, ent->origin);
//Con_DPrintf ("nopredict\n");
		}
		else
		{
			// predict players movement
			if (msec > 250)
				msec = 250;
			state->command.msec = msec;
//Con_DPrintf ("predict: %i\n", msec);

			oldphysent = pmove.numphysent;
			CL_SetSolidPlayers ();
			CL_PredictUsercmd (0, j+1, state, &exact, &state->command);	//uses player 0's maxspeed/grav...
			pmove.numphysent = oldphysent;
			VectorCopy (exact.origin, ent->origin);
		}

		VectorCopy(ent->origin, nametagorg[j]);
		nametagseen[j] = true;

		if (state->effects & QWEF_FLAG1)
			CL_AddFlagModels (ent, 0);
		else if (state->effects & QWEF_FLAG2)
			CL_AddFlagModels (ent, 1);
		if (state->command.impulse)
			CL_AddVWeapModel (ent, cl.model_precache_vwep[state->command.impulse]);

		CLQ1_AddShadow(ent);
		CLQ1_AddPowerupShell(ent, false, state->effects);

		if (r_torch.ival)
		{
			dlight_t *dl;
			dl = CL_NewDlight(j+1, ent->origin, 300, r_torch.ival, 0.5, 0.5, 0.2);
			dl->flags |= LFLAG_SHADOWMAP|LFLAG_FLASHBLEND;
			dl->fov = 60;
			angles[0] *= 3;
			angles[1] += sin(realtime)*8;
			angles[0] += cos(realtime*1.13)*5;
			AngleVectors(angles, dl->axis[0], dl->axis[1], dl->axis[2]);
		}
	}
}

void CL_LinkViewModel(void)
{
	entity_t	ent;

	unsigned int plnum;
	player_state_t *plstate;
	static struct model_s *oldmodel[MAX_SPLITS];
	static float lerptime[MAX_SPLITS];
	static float frameduration[MAX_SPLITS];
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

	if (!r_drawentities.ival)
		return;

	if ((cl.playerview[r_refdef.currentplayernum].stats[STAT_ITEMS] & IT_INVISIBILITY) && r_drawviewmodelinvis.value <= 0)
		return;

	if (cl.playerview[r_refdef.currentplayernum].stats[STAT_HEALTH] <= 0)
		return;

	if (r_drawviewmodel.value > 0 && r_drawviewmodel.value < 1)
		alpha = r_drawviewmodel.value;
	else
		alpha = 1;

	if ((cl.playerview[r_refdef.currentplayernum].stats[STAT_ITEMS] & IT_INVISIBILITY)
		&& r_drawviewmodelinvis.value > 0
		&& r_drawviewmodelinvis.value < 1)
		alpha *= r_drawviewmodelinvis.value;

	if (alpha <= 0)
		return;

	V_ClearEntity(&ent);

	ent.model = cl.viewent[r_refdef.currentplayernum].model;
	if (!ent.model)
		return;

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
	if (alpha != 1)
	{
		ent.flags |= Q2RF_TRANSLUCENT;
	}

#ifdef HLCLIENT
	if (!CLHL_AnimateViewEntity(&ent))
#endif
	{
		ent.framestate.g[FS_REG].frame[0] = cl.viewent[r_refdef.currentplayernum].framestate.g[FS_REG].frame[0];
		ent.framestate.g[FS_REG].frame[1] = oldframe[r_refdef.currentplayernum];

		if (ent.framestate.g[FS_REG].frame[0] != prevframe[r_refdef.currentplayernum])
		{
			oldframe[r_refdef.currentplayernum] = ent.framestate.g[FS_REG].frame[1] = prevframe[r_refdef.currentplayernum];

			frameduration[r_refdef.currentplayernum] = (realtime - lerptime[r_refdef.currentplayernum]);
			if (frameduration[r_refdef.currentplayernum] < 0.01)//no faster than 100 times a second... to avoid divide by zero
				frameduration[r_refdef.currentplayernum] = 0.01;
			if (frameduration[r_refdef.currentplayernum] > 0.2)	//no slower than 5 times a second
				frameduration[r_refdef.currentplayernum] = 0.2;
			lerptime[r_refdef.currentplayernum] = realtime;
		}
		prevframe[r_refdef.currentplayernum] = ent.framestate.g[FS_REG].frame[0];

		if (ent.model != oldmodel[r_refdef.currentplayernum])
		{
			oldmodel[r_refdef.currentplayernum] = ent.model;
			oldframe[r_refdef.currentplayernum] = ent.framestate.g[FS_REG].frame[1] = ent.framestate.g[FS_REG].frame[0];
			frameduration[r_refdef.currentplayernum] = 0.1;
			lerptime[r_refdef.currentplayernum] = realtime;
		}
		ent.framestate.g[FS_REG].lerpfrac = 1-(realtime-lerptime[r_refdef.currentplayernum])/frameduration[r_refdef.currentplayernum];
		ent.framestate.g[FS_REG].lerpfrac = bound(0, ent.framestate.g[FS_REG].lerpfrac, 1);
	}

	ent.flags |= Q2RF_WEAPONMODEL|Q2RF_DEPTHHACK|RF_NOSHADOW;

	plnum = -1;
	if (cl.spectator)
		plnum = Cam_TrackNum(r_refdef.currentplayernum);
	if (plnum == -1)
		plnum = cl.playernum[r_refdef.currentplayernum];
	plstate = &cl.frames[parsecountmod].playerstate[plnum];

	CLQ1_AddPowerupShell(V_AddEntity(&ent), true, plstate?plstate->effects:0);

	if (alpha < 1 && qrenderer == QR_OPENGL)
	{
		ent.forcedshader = 	R_RegisterShader("viewmodeldepthmask",
				"{\n"
					"noshadows\n"
					"surfaceparm nodlight\n"
					"{\n"
						"map $whiteimage\n"
						"maskcolor\n"
						"depthwrite\n"
					"}\n"
				"}\n"
				);
		ent.shaderRGBAf[3] = 1;
		ent.flags &= ~Q2RF_TRANSLUCENT;
		V_AddEntity(&ent);
		ent.forcedshader = NULL;
		ent.shaderRGBAf[3] = alpha;
		ent.flags |= Q2RF_TRANSLUCENT;
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
	physent_t			*pent;

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

		if (!state->solid && !state->skinnum)
			continue;

		if (state->solid == ES_SOLID_BSP)
		{	/*bsp model size*/
			if (state->modelindex <= 0)
				continue;
			if (!cl.model_precache[state->modelindex])
				continue;
			/*this makes non-inline bsp objects non-solid for prediction*/
			if ((*cl.model_precache[state->modelindex]->name == '*' || cl.model_precache[state->modelindex]->numsubmodels) && cl.model_precache[state->modelindex]->hulls[1].firstclipnode)
			{
				pent = &pmove.physents[pmove.numphysent];
				memset(pent, 0, sizeof(physent_t));
				pent->model = cl.model_precache[state->modelindex];
				VectorCopy (state->angles, pent->angles);
				pent->angles[0]*=-1;
			}
			else
				continue;
		}
		else
		{
			pent = &pmove.physents[pmove.numphysent];
			memset(pent, 0, sizeof(physent_t));
			pent->info = state->number;
			/*don't bother with angles*/
			pent->maxs[0] = pent->maxs[1] = 8*(state->solid & 31);
			pent->mins[0] = pent->mins[1] = -pent->maxs[0];
			pent->mins[2] = -8*((state->solid>>5) & 31);
			pent->maxs[2] = 8*((state->solid>>10) & 63) - 32;
		}
		if (++pmove.numphysent == MAX_PHYSENTS)
			break;
		VectorCopy(state->origin, pent->origin);
		pent->info = state->number;

		switch((int)state->skinnum)
		{
		case 0:
			break;
		case Q1CONTENTS_LADDER:
			pent->nonsolid = true;
			pent->forcecontentsmask = FTECONTENTS_LADDER;
			break;
		case Q1CONTENTS_SKY:
			pent->nonsolid = true;
			pent->forcecontentsmask = FTECONTENTS_SKY;
			break;
		case Q1CONTENTS_LAVA:
			pent->nonsolid = true;
			pent->forcecontentsmask = FTECONTENTS_LAVA;
			break;
		case Q1CONTENTS_SLIME:
			pent->nonsolid = true;
			pent->forcecontentsmask = FTECONTENTS_SLIME;
			break;
		case Q1CONTENTS_WATER:
			pent->nonsolid = true;
			pent->forcecontentsmask = FTECONTENTS_WATER;
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
	float predictmsmult = 1000*cl_predict_players_frac.value;

	int s;

	playertime = realtime - cls.latency + 0.02;
	if (playertime > realtime)
		playertime = realtime;

	if (cl_nopred.value || /*cls.demoplayback ||*/ cl.paused || cl.worldmodel->needload)
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

		// note that the local player is special, since he moves locally
		// we use his last predicted postition
		for (s = 0; s < cl.splitclients; s++)
		{
			if (j == cl.playernum[s])
			{
				VectorCopy(cl.frames[cls.netchan.outgoing_sequence&UPDATE_MASK].playerstate[cl.playernum[s]].origin,
					pplayer->origin);
				break;
			}
		}
		if (s == cl.splitclients)
		{
			// only predict half the move to minimize overruns
			msec = predictmsmult*(playertime - state->state_time);
			if (msec <= 0 ||
				!cl_predict_players.ival ||
				!dopred)
			{
				VectorCopy (state->origin, pplayer->origin);
	//Con_DPrintf ("nopredict\n");
			}
			else
			{
				// predict players movement
				if (msec > 250)
					msec = 250;
				state->command.msec = msec;
	//Con_DPrintf ("predict: %i\n", msec);

				CL_PredictUsercmd (0, j+1, state, &exact, &state->command);
				VectorCopy (exact.origin, pplayer->origin);
			}

			if (cl.spectator)
			{
//				if (!Cam_DrawPlayer(0, j))
//					VectorCopy(pplayer->origin, cl.simorg[0]);

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
void CL_SetSolidPlayers (void)
{
	int		j;
	extern	vec3_t	player_mins;
	extern	vec3_t	player_maxs;
	struct predicted_player *pplayer;
	physent_t *pent;

	if (!cl_solid_players.ival)
		return;

	pent = pmove.physents + pmove.numphysent;

	if (pmove.numphysent == MAX_PHYSENTS)	//too many.
		return;

	for (j=0, pplayer = predicted_players; j < MAX_CLIENTS;	j++, pplayer++)
	{
		if (!pplayer->active)
			continue;	// not present this frame

		if (pplayer->flags & PF_DEAD)
			continue; // dead players aren't solid

		memset(pent, 0, sizeof(physent_t));
		VectorCopy(pplayer->origin, pent->origin);
		pent->info = j+1;
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
void CL_ClearEntityLists(void)
{
	if (cl_numvisedicts == cl_maxvisedicts)
	{
		int newnum = cl_maxvisedicts + 32;
		entity_t *n = BZ_Realloc(cl_visedicts, newnum * sizeof(*n));
		if (n)
		{
			cl_visedicts = n;
			cl_maxvisedicts = newnum;
		}
	}
	cl_numvisedicts = 0;
	cl_numstrisidx = 0;
	cl_numstrisvert = 0;
	cl_numstris = 0;
}
void CL_FreeVisEdicts(void)
{
	BZ_Free(cl_visedicts);
	cl_visedicts = NULL;
	cl_maxvisedicts = 0;
	cl_numvisedicts = 0;
}
/*
static void CL_WaterSplashes(void)
{
	int i;
	entity_t *ent;
	vec3_t org;

	static unsigned int ltime;
	unsigned int ntime = cl.time*1000;
	if (ntime - ltime < 200)
		return;
	ltime = ntime;

	for (i = 0; i < cl_numvisedicts; i++)
	{
		ent = &cl_visedicts[i];

		if (ent->model)
		{
			if (ent->origin[2] + ent->model->mins[2] < r_refdef.waterheight &&
				ent->origin[2] + ent->model->maxs[2] > r_refdef.waterheight)
			{
				org[0] = ent->origin[0];
				org[1] = ent->origin[1];
				org[2] = r_refdef.waterheight;
				P_RunParticleEffectTypeString(org, NULL, 1, "te_watertransition");
			}
		}
	}
}
*/
void CL_EmitEntities (void)
{
	if (cls.state != ca_active)
		return;

	CL_DecayLights ();

#ifdef Q2CLIENT
	if (cls.protocol == CP_QUAKE2)
	{
		CL_ClearEntityLists();
		CLQ2_AddEntities();
		return;
	}
#endif
	if (!cl.validsequence)
		return;

	CL_ClearEntityLists();

	CL_LinkPlayers ();
	CL_LinkPacketEntities ();
	CL_LinkProjectiles ();
	CL_UpdateTEnts ();

//	CL_WaterSplashes();
}











void CL_ClearPredict(void)
{
	memset(predicted_players, 0, sizeof(predicted_players));
}

