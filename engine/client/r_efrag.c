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
// r_efrag.c

#include "quakedef.h"

extern int			r_framecount;

mnode_t	*r_pefragtopnode;


//===========================================================================

/*
===============================================================================

					ENTITY FRAGMENT FUNCTIONS

===============================================================================
*/

efrag_t		**lastlink;

vec3_t		r_emins, r_emaxs;

entity_t	*r_addent;


/*
================
R_RemoveEfrags

Call when removing an object from the world or moving it to another position
================
*/
void R_RemoveEfrags (entity_t *ent)
{
	efrag_t		*ef, *old, *walk, **prev;

	ef = ent->efrag;

	while (ef)
	{
		prev = &ef->leaf->efrags;
		while (1)
		{
			walk = *prev;
			if (!walk)
				break;
			if (walk == ef)
			{	// remove this fragment
				*prev = ef->leafnext;
				break;
			}
			else
				prev = &walk->leafnext;
		}

		old = ef;
		ef = ef->entnext;

	// put it on the free list
		old->entnext = cl.free_efrags;
		cl.free_efrags = old;
	}

	ent->efrag = NULL;
}

/*
===================
R_SplitEntityOnNode
===================
*/
void R_Q1Q2BSP_SplitEntityOnNode (mnode_t *node)
{
	efrag_t		*ef;
	mplane_t	*splitplane;
	mleaf_t		*leaf;
	int			sides;

	if (cl.worldmodel->fromgame == fg_quake2 || cl.worldmodel->fromgame == fg_quake3)
	{
		if (node->contents & Q2CONTENTS_SOLID)
		{
			return;
		}
	}
	else
	{
		if (node->contents == Q1CONTENTS_SOLID)
		{
			return;
		}
	}

// add an efrag if the node is a leaf

	if ( node->contents < 0)
	{
		if (!r_pefragtopnode)
			r_pefragtopnode = node;

		leaf = (mleaf_t *)node;

// grab an efrag off the free list
		ef = cl.free_efrags;
		if (!ef)
		{
			Con_Printf ("Too many efrags!\n");
			return;		// no free fragments...
		}
		cl.free_efrags = cl.free_efrags->entnext;

		ef->entity = r_addent;

// add the entity link
		*lastlink = ef;
		lastlink = &ef->entnext;
		ef->entnext = NULL;

// set the leaf links
		ef->leaf = leaf;
		ef->leafnext = leaf->efrags;
		leaf->efrags = ef;

		return;
	}

// NODE_MIXED

	splitplane = node->plane;
	sides = BOX_ON_PLANE_SIDE(r_emins, r_emaxs, splitplane);

	if (sides == 3)
	{
	// split on this plane
	// if this is the first splitter of this bmodel, remember it
		if (!r_pefragtopnode)
			r_pefragtopnode = node;
	}

// recurse down the contacted sides
	if (sides & 1)
		R_Q1Q2BSP_SplitEntityOnNode (node->children[0]);

	if (sides & 2)
		R_Q1Q2BSP_SplitEntityOnNode (node->children[1]);
}

/*
===========
R_AddEfrags
===========
*/
void R_AddEfrags (entity_t *ent)
{
	model_t		*entmodel;
	int			i;

	if (!ent->model)
		return;

	if (ent == &r_worldentity)
		return;		// never add the world

	r_addent = ent;

	lastlink = &ent->efrag;
	r_pefragtopnode = NULL;

	entmodel = ent->model;

	for (i=0 ; i<3 ; i++)
	{
		r_emins[i] = ent->origin[i] + entmodel->mins[i];
		r_emaxs[i] = ent->origin[i] + entmodel->maxs[i];
	}

	if (cl.worldmodel->nodes)
		R_Q1Q2BSP_SplitEntityOnNode (cl.worldmodel->nodes);

	ent->topnode = r_pefragtopnode;
}


/*
================
R_StoreEfrags

// FIXME: a lot of this goes away with edge-based
================
*/
void R_StoreEfrags (efrag_t **ppefrag)
{
	entity_t	*pent;
	model_t		*clmodel;
	efrag_t		*pefrag;

	extern cvar_t gl_part_flame;
	extern cvar_t r_drawflame;

	while ((pefrag = *ppefrag) != NULL)
	{
		pent = pefrag->entity;
		clmodel = pent->model;

		if ((!r_drawflame.value) && (clmodel->engineflags & MDLF_FLAME))
			break;

//		switch (clmodel->type)
//		{
//		case mod_alias:
//		case mod_brush:
//		case mod_sprite:
//			pent = pefrag->entity;

			if ((pent->visframe != r_framecount) &&
				(cl_numvisedicts < MAX_VISEDICTS))
			{
				pent->framestate.g[FS_REG].frametime[0] = cl.time;
				pent->framestate.g[FS_REG].frametime[1] = cl.time;
				cl_visedicts[cl_numvisedicts++] = *pent;

			// mark that we've recorded this entity for this frame
				pent->visframe = r_framecount;

			// emit particles for statics (we don't need to cheat check statics)
				if (clmodel->particleeffect >= 0 && gl_part_flame.value)
				{
					// TODO: this is ugly.. assumes ent is in static entities, and subtracts
					// pointer math to get an index to use in cl_static emit
					// there needs to be a cleaner method for this
					int i = (int)(pent - cl_static_entities);
					P_EmitEffect(pent->origin, clmodel->particleeffect, &(cl_static_emit[i]));
				}
			}

			ppefrag = &pefrag->leafnext;
//			break;

//		default:
//			Con_Printf ("R_StoreEfrags: Bad entity type %d\n", clmodel->type);
//		}
	}
}


