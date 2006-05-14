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
// r_misc.c

#include "quakedef.h"
#include "r_local.h"
extern int		r_viewcluster, r_viewcluster2, r_oldviewcluster, r_oldviewcluster2;

/*
===============
R_CheckVariables
===============
*/
void R_CheckVariables (void)
{
}


/*
============
Show

Debugging use
============
*/
void Show (void)
{
	vrect_t	vr;

	vr.x = vr.y = 0;
	vr.width = vid.width;
	vr.height = vid.height;
	vr.pnext = NULL;
	SWVID_Update (&vr);
}

/*
====================
R_TimeRefresh_f

For program optimization
====================
*/
void SWR_TimeRefresh_f (void)
{
	int			i;
	float		start, stop, time;
	int			startangle;
	vrect_t		vr;

	startangle = r_refdef.viewangles[1];
	
	start = Sys_DoubleTime ();
	for (i=0 ; i<128 ; i++)
	{
		r_refdef.viewangles[1] = i/128.0*360.0;

		VID_LockBuffer ();

		R_RenderView ();

		VID_UnlockBuffer ();

		vr.x = r_refdef.vrect.x;
		vr.y = r_refdef.vrect.y;
		vr.width = r_refdef.vrect.width;
		vr.height = r_refdef.vrect.height;
		vr.pnext = NULL;
		SWVID_Update (&vr);
	}
	stop = Sys_DoubleTime ();
	time = stop-start;
	Con_Printf ("%f seconds (%f fps)\n", time, 128/time);
	
	r_refdef.viewangles[1] = startangle;
}

/*
================
R_LineGraph

Only called by R_DisplayTime
================
*/
void R_LineGraph (int x, int y, int h)
{
	int		i;
	qbyte	*dest;
	unsigned short *dest16;
	unsigned int *dest32;
	int		s;
	unsigned int color;

// FIXME: should be disabled on no-buffer adapters, or should be in the driver
	
//	x += r_refdef.vrect.x;
//	y += r_refdef.vrect.y;
	s = r_graphheight.value;

	if (h>s)
		h = s;

	if (h == 10000)
		color = 0x6f;	// yellow
	else if (h == 9999)
		color = 0x4f;	// red
	else if (h == 9998)
		color = 0xd0;	// blue
	else
		color = 0xff;	// pink
		
	switch (r_pixbytes)
	{
	case 2: // 16 bpp
		dest16 = (unsigned short*)vid.buffer + vid.rowbytes*y + x;

		// use constant colors instead?
		color = d_8to16table[color];
		
		for (i=0 ; i<h ; i++, dest16 -= vid.rowbytes*2)
		{
			dest16[0] = color;
		//	*(dest-vid.rowbytes) = 0x30;
		}
		break;
	case 4: // 32 bpp
		dest32 = (unsigned int*)vid.buffer + vid.rowbytes*y + x;

		// use constant colors instead?
		color = d_8to32table[color];
		
		for (i=0 ; i<h ; i++, dest32 -= vid.rowbytes*2)
		{
			dest32[0] = color;
		//	*(dest-vid.rowbytes) = 0x30;
		}
		break;
	default: // 8 bpp
		dest = vid.buffer + vid.rowbytes*y + x;
				
		for (i=0 ; i<h ; i++, dest -= vid.rowbytes*2)
		{
			dest[0] = color;
		//	*(dest-vid.rowbytes) = 0x30;
		}
	}
}

/*
==============
R_TimeGraph

Performance monitoring tool
==============
*/
#define	MAX_TIMINGS		100
int		graphval;
void R_TimeGraph (void)
{
	static	int		timex;
	int		a;
	float	r_time2;
	static qbyte	r_timings[MAX_TIMINGS];
	int		x;
	
	r_time2 = Sys_DoubleTime ();

	a = (r_time2-r_time1)/0.01;
//a = fabs(mouse_y * 0.05);
//a = (int)((r_refdef.vieworg[2] + 1024)/1)%(int)r_graphheight.value;
//a = (int)((pmove.velocity[2] + 500)/10);
//a = fabs(velocity[0])/20;
//a = ((int)fabs(origin[0])/8)%20;
//a = (cl.idealpitch + 30)/5;
//a = (int)(cl.simangles[YAW] * 64/360) & 63;
a = graphval;

	r_timings[timex] = a;
	a = timex;

	if (r_refdef.vrect.width <= MAX_TIMINGS)
		x = r_refdef.vrect.width-1;
	else
		x = r_refdef.vrect.width -
				(r_refdef.vrect.width - MAX_TIMINGS)/2;
	do
	{
		R_LineGraph (x, r_refdef.vrect.height-2, r_timings[a]);
		if (x==0)
			break;		// screen too small to hold entire thing
		x--;
		a--;
		if (a == -1)
			a = MAX_TIMINGS-1;
	} while (a != timex);

	timex = (timex+1)%MAX_TIMINGS;
}

/*
==============
R_NetGraph
==============
*/
void SWR_NetGraph (void)
{
	int		a, x, y, y2, w, i;
	int lost;
	char st[80];

	if (vid.width - 16 <= NET_TIMINGS)
		w = vid.width - 16;
	else
		w = NET_TIMINGS;

	x =	-(int)((vid.width - 320)>>1);
	y = vid.height - sb_lines - 24 - (int)r_graphheight.value*2 - 2;

	M_DrawTextBox (x, y, (w+7)/8, ((int)r_graphheight.value*2+7)/8 + 1);
	y2 = y + 8;
	y = vid.height - sb_lines - 8 - 2;

	x = 8;
	lost = CL_CalcNet();
	for (a=NET_TIMINGS-w ; a<w ; a++)
	{
		i = (cls.netchan.outgoing_sequence-a) & NET_TIMINGSMASK;
		R_LineGraph (x+w-1-a, y, packet_latency[i]);
	}
	sprintf(st, "%3i%% packet loss", lost);
	Draw_String(8, y2, st);
}

/*
==============
R_ZGraph
==============
*/
void R_ZGraph (void)
{
	int		a, x, w, i;
	static	int	height[256];

	if (r_refdef.vrect.width <= 256)
		w = r_refdef.vrect.width;
	else
		w = 256;

	height[r_framecount&255] = ((int)r_origin[2]) & 31;

	x = 0;
	for (a=0 ; a<w ; a++)
	{
		i = (r_framecount-a) & 255;
		R_LineGraph (x+w-1-a, r_refdef.vrect.height-2, height[i]);
	}
}

/*
=============
R_PrintTimes
=============
*/
void R_PrintTimes (void)
{
	float	r_time2;
	float		ms;

	r_time2 = Sys_DoubleTime ();

	ms = 1000* (r_time2 - r_time1);
	
	Con_Printf ("%5.1f ms %3i/%3i/%3i poly %3i surf\n",
				ms, c_faceclip, r_polycount, r_drawnpolycount, c_surf);
	c_surf = 0;
}


/*
=============
R_PrintDSpeeds
=============
*/
void R_PrintDSpeeds (void)
{
	float	ms, dp_time, r_time2, rw_time, db_time, se_time, de_time, dv_time;

	r_time2 = Sys_DoubleTime ();

	dp_time = (dp_time2 - dp_time1) * 1000;
	rw_time = (rw_time2 - rw_time1) * 1000;
	db_time = (db_time2 - db_time1) * 1000;
	se_time = (se_time2 - se_time1) * 1000;
	de_time = (de_time2 - de_time1) * 1000;
	dv_time = (dv_time2 - dv_time1) * 1000;
	ms = (r_time2 - r_time1) * 1000;

	Con_Printf ("%3i %4.1fp %3iw %4.1fb %3is %4.1fe %4.1fv\n",
				(int)ms, dp_time, (int)rw_time, db_time, (int)se_time, de_time,
				dv_time);
}


/*
=============
R_PrintAliasStats
=============
*/
void R_PrintAliasStats (void)
{
	Con_Printf ("%3i polygon model drawn\n", r_amodels_drawn);
}


void WarpPalette (void)
{
	int		i,j;
	qbyte	newpalette[768];
	int		basecolor[3];
	
	basecolor[0] = 130;
	basecolor[1] = 80;
	basecolor[2] = 50;

// pull the colors halfway to bright brown
	for (i=0 ; i<256 ; i++)
	{
		for (j=0 ; j<3 ; j++)
		{
			newpalette[i*3+j] = (host_basepal[i*3+j] + basecolor[j])/2;
		}
	}
	
	VID_ShiftPalette (newpalette);
}


/*
===================
R_TransformFrustum
===================
*/
void R_TransformFrustum (void)
{
	int		i;
	vec3_t	v, v2;
	
	for (i=0 ; i<4 ; i++)
	{
		v[0] = screenedge[i].normal[2];
		v[1] = -screenedge[i].normal[0];
		v[2] = screenedge[i].normal[1];

		v2[0] = v[1]*vright[0] + v[2]*vup[0] + v[0]*vpn[0];
		v2[1] = v[1]*vright[1] + v[2]*vup[1] + v[0]*vpn[1];
		v2[2] = v[1]*vright[2] + v[2]*vup[2] + v[0]*vpn[2];

		VectorCopy (v2, view_clipplanes[i].normal);

		view_clipplanes[i].dist = DotProduct (modelorg, v2);
	}
}


#if !id386

/*
================
TransformVector
================
*/
void TransformVector (vec3_t in, vec3_t out)
{
	out[0] = DotProduct(in,vright);
	out[1] = DotProduct(in,vup);
	out[2] = DotProduct(in,vpn);
}

#endif


/*
================
R_TransformPlane
================
*/
void R_TransformPlane (mplane_t *p, float *normal, float *dist)
{
	float	d;
	
	d = DotProduct (r_origin, p->normal);
	*dist = p->dist - d;
// TODO: when we have rotating entities, this will need to use the view matrix
	TransformVector (p->normal, normal);
}


/*
===============
R_SetUpFrustumIndexes
===============
*/
void R_SetUpFrustumIndexes (void)
{
	int		i, j, *pindex;

	pindex = r_frustum_indexes;

	for (i=0 ; i<4 ; i++)
	{
		for (j=0 ; j<3 ; j++)
		{
			if (view_clipplanes[i].normal[j] < 0)
			{
				pindex[j] = j;
				pindex[j+3] = j+3;
			}
			else
			{
				pindex[j] = j+3;
				pindex[j+3] = j;
			}
		}

	// FIXME: do just once at start
		pfrustum_indexes[i] = pindex;
		pindex += 6;
	}
}


/*
===============
R_SetupFrame
===============
*/
void SWR_SetupFrame (void)
{
	static mleaf_t fakeleaf;
	extern int r_dosirds;

	extern int scr_chatmode;
	extern float r_wateralphaval;

	int				edgecount;
	vrect_t			vrect;
	float			w, h;


// don't allow cheats in multiplayer
	r_wateralphaval = r_wateralpha.value;
	if (!cls.allow_watervis)
		r_wateralphaval = 1;

	if (r_numsurfs.value)
	{
		if ((surface_p - surfaces) > r_maxsurfsseen)
			r_maxsurfsseen = surface_p - surfaces;

		Con_Printf ("Used %d of %d surfs; %d max\n", surface_p - surfaces,
				surf_max - surfaces, r_maxsurfsseen);
	}

	if (r_numedges.value)
	{
		edgecount = edge_p - r_edges;

		if (edgecount > r_maxedgesseen)
			r_maxedgesseen = edgecount;

		Con_Printf ("Used %d of %d edges; %d max\n", edgecount,
				r_numallocatededges, r_maxedgesseen);
	}

	r_refdef.ambientlight = r_ambient.value;

	if (r_refdef.ambientlight < 0)
		r_refdef.ambientlight = 0;

//	if (!sv.active)
		r_draworder.value = 0;	// don't let cheaters look behind walls
		
	R_CheckVariables ();
	
	SWR_AnimateLight ();

	r_framecount++;

	numbtofpolys = 0;

// debugging
#if 0
r_refdef.vieworg[0]=  80;
r_refdef.vieworg[1]=      64;
r_refdef.vieworg[2]=      40;
r_refdef.viewangles[0]=    0;
r_refdef.viewangles[1]=    46.763641357;
r_refdef.viewangles[2]=    0;
#endif

// build the transformation matrix for the given view angles
	VectorCopy (r_refdef.vieworg, modelorg);
	VectorCopy (r_refdef.vieworg, r_origin);

	AngleVectors (r_refdef.viewangles, vpn, vright, vup);

	if (r_refdef.flags & 1)
	{
		r_oldviewleaf = r_viewleaf = &fakeleaf;	//so we can use quake1 rendering routines for q2 bsps.
		r_viewleaf->contents = Q1CONTENTS_EMPTY;
	}
#ifdef Q2BSPS
	else if (cl.worldmodel->fromgame == fg_quake2)
	{
		mleaf_t	*leaf;

		r_viewleaf = &fakeleaf;	//so we can use quake1 rendering routines for q2 bsps.
		r_viewleaf->contents = Q1CONTENTS_EMPTY;

		r_oldviewcluster = r_viewcluster;
		r_oldviewcluster2 = r_viewcluster2;
		leaf = SWMod_PointInLeaf (cl.worldmodel, r_origin);
		r_viewcluster = r_viewcluster2 = leaf->cluster;

		// check above and below so crossing solid water doesn't draw wrong
		if (!leaf->contents)
		{	// look down a bit
			vec3_t	temp;

			VectorCopy (r_origin, temp);
			temp[2] -= 16;
			leaf = SWMod_PointInLeaf (cl.worldmodel, temp);
			if ( !(leaf->contents & Q2CONTENTS_SOLID) &&
				(leaf->cluster != r_viewcluster2) )
				r_viewcluster2 = leaf->cluster;
		}
		else
		{	// look up a bit
			vec3_t	temp;

			VectorCopy (r_origin, temp);
			temp[2] += 16;
			leaf = SWMod_PointInLeaf (cl.worldmodel, temp);
			if ( !(leaf->contents & Q2CONTENTS_SOLID) &&
				(leaf->cluster != r_viewcluster2) )
				r_viewcluster2 = leaf->cluster;
		}
	}
#endif
	else
	{
// current viewleaf
		r_oldviewleaf = r_viewleaf;
		r_viewleaf = SWMod_PointInLeaf (cl.worldmodel, r_origin);
	}

	r_dowarpold = r_dowarp;
#ifdef SIDEVIEWS
	r_dowarp = r_waterwarp.value && (r_viewleaf->contents <= Q1CONTENTS_WATER) && !r_secondaryview;
#else
	r_dowarp = r_waterwarp.value && (r_viewleaf->contents <= Q1CONTENTS_WATER);
#endif
	if (vid.width > MAXWIDTH || r_pixbytes != 1 || scr_chatmode)
		r_dowarp = 0;
	if (r_dosirds)
		r_dowarp = 0;

	if ((r_dowarp != r_dowarpold) || r_viewchanged)
	{
		if (r_dowarp)
		{
			if ((vid.width <= vid.maxwarpwidth) &&
				(vid.height <= vid.maxwarpheight))
			{
				vrect.x = 0;
				vrect.y = 0;
				vrect.width = vid.width;
				vrect.height = vid.height;

				SWR_ViewChanged (&vrect, sb_lines, vid.aspect);
			}
			else
			{
				w = vid.width;
				h = vid.height;

				if (w > vid.maxwarpwidth)
				{
					h *= (float)vid.maxwarpwidth / w;
					w = vid.maxwarpwidth;
				}

				if (h > vid.maxwarpheight)
				{
					h = vid.maxwarpheight;
					w *= (float)vid.maxwarpheight / h;
				}

				vrect.x = 0;
				vrect.y = 0;
				vrect.width = (int)w;
				vrect.height = (int)h;

				SWR_ViewChanged (&vrect,
							   (int)((float)sb_lines * (h/(float)vid.height)),
							   vid.aspect * (h / w) *
								 ((float)vid.width / (float)vid.height));
			}
		}
		else
		{
			vrect.x = 0;
			vrect.y = 0;
			vrect.width = vid.width;
			vrect.height = vid.height;

			SWR_ViewChanged (&vrect, sb_lines, vid.aspect);
		}

		r_viewchanged = false;
	}

// start off with just the four screen edge clip planes
	R_TransformFrustum ();

// save base values
	VectorCopy (vpn, base_vpn);
	VectorCopy (vright, base_vright);
	VectorCopy (vup, base_vup);
	VectorCopy (modelorg, base_modelorg);

	R_SetSkyFrame ();

	R_SetUpFrustumIndexes ();

	r_cache_thrash = false;

// clear frame counts
	c_faceclip = 0;
	d_spanpixcount = 0;
	r_polycount = 0;
	r_drawnpolycount = 0;
	r_wholepolycount = 0;
	r_amodels_drawn = 0;
	r_outofsurfaces = 0;
	r_outofedges = 0;

	D_SetupFrame ();
}

