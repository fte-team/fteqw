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
// r_main.c

#include "quakedef.h"
#include "r_local.h"
#include "sw_draw.h"

int		r_viewcluster, r_viewcluster2, r_oldviewcluster, r_oldviewcluster2;

extern cvar_t r_netgraph;
extern cvar_t r_sirds;

//define	PASSAGES

void		*colormap;
vec3_t		viewlightvec;
alight_t	r_viewlighting = {128, 192, viewlightvec};
float		r_time1;
int			r_numallocatededges;
qboolean	r_drawpolys;
qboolean	r_drawculledpolys;
qboolean	r_worldpolysbacktofront;
qboolean	r_recursiveaffinetriangles = true;
int			r_pixbytes = 1;
float		r_aliasuvscale = 1.0;
int			r_outofsurfaces;
int			r_outofedges;

qboolean	r_dowarp, r_dowarpold, r_viewchanged;

int			numbtofpolys;
btofpoly_t	*pbtofpolys;
mvertex_t	*r_pcurrentvertbase;

int			c_surf;
int			r_maxsurfsseen, r_maxedgesseen, r_cnumsurfs;
qboolean	r_surfsonstack;
int			r_clipflags;

qbyte		*r_warpbuffer;

qboolean	r_fov_greater_than_90;

entity_t	r_worldentity;

//
// view origin
//
vec3_t	vup, base_vup;
vec3_t	vpn, base_vpn;
vec3_t	vright, base_vright;
vec3_t	r_origin;

//
// screen size info
//
refdef_t	r_refdef;
float		xcenter, ycenter;
float		xscale, yscale;
float		xscaleinv, yscaleinv;
float		xscaleshrink, yscaleshrink;
float		aliasxscale, aliasyscale, aliasxcenter, aliasycenter;

int		screenwidth;

float	pixelAspect;
float	screenAspect;
float	verticalFieldOfView;
float	xOrigin, yOrigin;

float r_wateralphaval;

mplane_t	screenedge[4];

//colour bits (for 16 bit rendering)
int redbits, redshift;
int greenbits, greenshift;
int bluebits, blueshift;


//
// refresh flags
//
int		r_framecount = 1;	// so frame counts initialized to 0 don't match
int		r_visframecount;
int		d_spanpixcount;
int		r_polycount;
int		r_drawnpolycount;
int		r_wholepolycount;

int			*pfrustum_indexes[4];
int			r_frustum_indexes[4*6];

int		reinit_surfcache = 1;	// if 1, surface cache is currently empty and
								// must be reinitialized for current cache size

mleaf_t		*r_viewleaf, *r_oldviewleaf;

texture_t	*r_notexture_mip;

float		r_aliastransition, r_resfudge;

int		d_lightstylevalue[256];	// 8.8 fraction of base light value

float	dp_time1, dp_time2, db_time1, db_time2, rw_time1, rw_time2;
float	se_time1, se_time2, de_time1, de_time2, dv_time1, dv_time2;

void R_MarkLeaves (void);
extern cvar_t	r_zgraph;
extern cvar_t	r_aliastransbase;
extern cvar_t	r_aliastransadj;
extern cvar_t	r_fixmodelsbyclip;
/*
cvar_t	r_draworder = {"r_draworder","0"};
cvar_t	r_speeds = {"r_speeds","0"};
cvar_t	r_timegraph = {"r_timegraph","0"};
cvar_t	r_netgraph = {"r_netgraph","0"};
cvar_t	r_graphheight = {"r_graphheight","15"};
cvar_t	r_clearcolor = {"r_clearcolor","218"};
cvar_t	r_waterwarp = {"r_waterwarp","1"};
cvar_t	r_fullbright = {"r_fullbright","0"};
cvar_t	r_drawentities = {"r_drawentities","1"};
cvar_t	r_drawviewmodel = {"r_drawviewmodel","1"};
cvar_t	r_aliasstats = {"r_polymodelstats","0"};
cvar_t	r_dspeeds = {"r_dspeeds","0"};
cvar_t	r_drawflat = {"r_drawflat", "0"};
cvar_t	r_ambient = {"r_ambient", "0"};
cvar_t	r_reportsurfout = {"r_reportsurfout", "0"};
cvar_t	r_maxsurfs = {"r_maxsurfs", "0"};
cvar_t	r_numsurfs = {"r_numsurfs", "0"};
cvar_t	r_reportedgeout = {"r_reportedgeout", "0"};
cvar_t	r_maxedges = {"r_maxedges", "0"};
cvar_t	r_numedges = {"r_numedges", "0"};
*/
extern cvar_t r_loadlits;

extern cvar_t r_stains;
extern cvar_t r_stainfadetime;
extern cvar_t r_stainfadeammount;

qboolean r_usinglits;

#ifdef FISH
extern cvar_t	ffov;
#endif
extern cvar_t	scr_fov;

void CreatePassages (void);
void SetVisibilityByPassages (void);

void R_NetGraph (void);
void R_ZGraph (void);

/*
==================
R_InitTextures
==================
*
void	SWR_InitTextures (void)
{
	int		x,y, m;
	byte	*dest;
	
// create a simple checkerboard texture for the default
	r_notexture_mip = Hunk_AllocName (sizeof(texture_t) + 16*16+8*8+4*4+2*2, "notexture");
	
	r_notexture_mip->width = r_notexture_mip->height = 16;
	r_notexture_mip->offsets[0] = sizeof(texture_t);
	r_notexture_mip->offsets[1] = r_notexture_mip->offsets[0] + 16*16;
	r_notexture_mip->offsets[2] = r_notexture_mip->offsets[1] + 8*8;
	r_notexture_mip->offsets[3] = r_notexture_mip->offsets[2] + 4*4;
	
	for (m=0 ; m<4 ; m++)
	{
		dest = (byte *)r_notexture_mip + r_notexture_mip->offsets[m];
		for (y=0 ; y< (16>>m) ; y++)
			for (x=0 ; x< (16>>m) ; x++)
			{
				if (  (y< (8>>m) ) ^ (x< (8>>m) ) )
					*dest++ = 0;
				else
					*dest++ = 0xff;
			}
	}	
}*/

// callback declares
extern cvar_t crosshaircolor, r_skyboxname, r_menutint, v_contrast;
extern cvar_t r_floorcolour, r_wallcolour, r_drawflat, r_fastskycolour;
void SWCrosshaircolor_Callback(struct cvar_s *var, char *oldvalue);
void SWR_Skyboxname_Callback(struct cvar_s *var, char *oldvalue);
void SWR_Menutint_Callback(struct cvar_s *var, char *oldvalue);
void SWV_Gamma_Callback(struct cvar_s *var, char *oldvalue);
void SWR_Floorcolour_Callback(struct cvar_s *var, char *oldvalue);
void SWR_Wallcolour_Callback(struct cvar_s *var, char *oldvalue);
void SWR_Drawflat_Callback(struct cvar_s *var, char *oldvalue);
void SWR_Fastskycolour_Callback(struct cvar_s *var, char *oldvalue);

void SWR_DeInit (void)
{
	Cmd_RemoveCommand ("timerefresh");	
	Cmd_RemoveCommand ("pointfile");

	Cvar_Unhook(&crosshaircolor);
	Cvar_Unhook(&r_skyboxname);
	Cvar_Unhook(&r_menutint);
	Cvar_Unhook(&v_gamma);
	Cvar_Unhook(&v_contrast);
	Cvar_Unhook(&r_floorcolour);
	Cvar_Unhook(&r_wallcolour);
	Cvar_Unhook(&r_drawflat);
	Cvar_Unhook(&r_fastskycolour);

	SWDraw_Shutdown();
	D_Shutdown();
}

/*
===============
R_Init
===============
*/
void SWR_Init (void)
{
	//int		dummy;
	
// get stack position so we can guess if we are going to overflow
	//r_stack_start = (qbyte *)&dummy;
	
	R_InitTurb ();
	
	Cmd_AddRemCommand ("timerefresh", SWR_TimeRefresh_f);	

	Cvar_Hook(&crosshaircolor, SWCrosshaircolor_Callback);
	Cvar_Hook(&r_skyboxname, SWR_Skyboxname_Callback);
	Cvar_Hook(&r_menutint, SWR_Menutint_Callback);
	Cvar_Hook(&v_gamma, SWV_Gamma_Callback);
	Cvar_Hook(&v_contrast, SWV_Gamma_Callback);
	Cvar_Hook(&r_floorcolour, SWR_Floorcolour_Callback);
	Cvar_Hook(&r_wallcolour, SWR_Wallcolour_Callback);
	Cvar_Hook(&r_drawflat, SWR_Drawflat_Callback);
	Cvar_Hook(&r_fastskycolour, SWR_Fastskycolour_Callback);

	if (!r_maxedges.value)
		Cvar_SetValue (&r_maxedges, (float)NUMSTACKEDGES);
	if (!r_maxsurfs.value)
		Cvar_SetValue (&r_maxsurfs, (float)NUMSTACKSURFACES);

	view_clipplanes[0].leftedge = true;
	view_clipplanes[1].rightedge = true;
	view_clipplanes[1].leftedge = view_clipplanes[2].leftedge =
			view_clipplanes[3].leftedge = false;
	view_clipplanes[0].rightedge = view_clipplanes[2].rightedge =
			view_clipplanes[3].rightedge = false;

	r_refdef.xOrigin = XCENTERING;
	r_refdef.yOrigin = YCENTERING;

// TODO: collect 386-specific code in one place
#if	id386
	Sys_MakeCodeWriteable ((long)R_EdgeCodeStart,
					     (long)R_EdgeCodeEnd - (long)R_EdgeCodeStart);
#endif	// id386

	D_Init ();
}


/*
===============
R_NewMap
===============
*/
void SWR_NewMap (void)
{
	extern cvar_t host_mapname;
	char namebuf[MAX_OSPATH];
	int		i;

	memset (&r_worldentity, 0, sizeof(r_worldentity));
	AngleVectors(r_worldentity.angles, r_worldentity.axis[0], r_worldentity.axis[1], r_worldentity.axis[2]);
	VectorInverse(r_worldentity.axis[1]);
	r_worldentity.model = cl.worldmodel;

// clear out efrags in case the level hasn't been reloaded
// FIXME: is this one short?
	for (i=0 ; i<cl.worldmodel->numleafs ; i++)
		cl.worldmodel->leafs[i].efrags = NULL;
		 	
	r_viewleaf = NULL;
	P_ClearParticles ();

	r_cnumsurfs = r_maxsurfs.value;

	if (r_cnumsurfs <= MINSURFACES)
		r_cnumsurfs = MINSURFACES;

	if (r_cnumsurfs > NUMSTACKSURFACES)
	{
		surfaces = Hunk_AllocName (r_cnumsurfs * sizeof(surf_t), "surfaces");
		surface_p = surfaces;
		surf_max = &surfaces[r_cnumsurfs];
		r_surfsonstack = false;
	// surface 0 doesn't really exist; it's just a dummy because index 0
	// is used to indicate no edge attached to surface
		surfaces--;
		R_SurfacePatch ();
	}
	else
	{
		r_surfsonstack = true;
	}

	r_maxedgesseen = 0;
	r_maxsurfsseen = 0;

	r_numallocatededges = r_maxedges.value;

	if (r_numallocatededges < MINEDGES)
		r_numallocatededges = MINEDGES;

	if (r_numallocatededges <= NUMSTACKEDGES)
	{
		auxedges = NULL;
	}
	else
	{
		auxedges = Hunk_AllocName (r_numallocatededges * sizeof(edge_t),
								   "edges");
	}

	COM_StripExtension(COM_SkipPath(cl.worldmodel->name), namebuf, sizeof(namebuf));
	Cvar_Set(&host_mapname, namebuf);

	r_dowarpold = false;
	r_viewchanged = false;
#ifdef SWSTAINS
	SWR_BuildLightmaps();
#endif

	R_WipeDecals();

	R_InitSkyBox();
#ifdef VM_UI
	UI_Reset();
#endif
	TP_NewMap();
}


/*
===============
R_SetVrect
===============
*/
void R_SetVrect (vrect_t *pvrectin, vrect_t *pvrect, int lineadj)
{
	int		h;
	float	size;
	qboolean full = false;

#ifdef SIDEVIEWS
	if (r_secondaryview==1)
		return;

	if (!r_dowarp && !r_dowarpold)
		return;
#endif

	if (scr_viewsize.value >= 100.0) {
		size = 100.0;
		full = true;
	} else
		size = scr_viewsize.value;

	if (cl.intermission)
	{
		full = true;
		size = 100.0;
		lineadj = 0;
	}
	size /= 100.0;

	if (!cl_sbar.value && full)
		h = pvrectin->height;
	else
		h = pvrectin->height - lineadj;

//	h = (!cl_sbar.value && size==1.0) ? pvrectin->height : (pvrectin->height - lineadj);
//	h = pvrectin->height - lineadj;
	if (full)
		pvrect->width = pvrectin->width;
	else
		pvrect->width = pvrectin->width * size;
	if (pvrect->width < 96)
	{
		size = 96.0 / pvrectin->width;
		pvrect->width = 96;	// min for icons
	}
	pvrect->width &= ~7;
	pvrect->height = pvrectin->height * size;
	if (cl_sbar.value || !full) {
		if (pvrect->height > pvrectin->height - lineadj)
			pvrect->height = pvrectin->height - lineadj;
	} else
		if (pvrect->height > pvrectin->height)
			pvrect->height = pvrectin->height;

	pvrect->height &= ~1;

	pvrect->x = (pvrectin->width - pvrect->width)/2;
	if (full)
		pvrect->y = 0;
	else
		pvrect->y = (h - pvrect->height)/2;
}


/*
===============
R_ViewChanged

Called every time the vid structure or r_refdef changes.
Guaranteed to be called before the first refresh
===============
*/
void SWR_ViewChanged (vrect_t *pvrect, int lineadj, float aspect)
{
	int		i;
	float	res_scale;

	r_viewchanged = true;

	R_SetVrect (pvrect, &r_refdef.vrect, lineadj);

	r_refdef.horizontalFieldOfView = 2.0 * tan (r_refdef.fov_x/360*M_PI);
	r_refdef.fvrectx = (float)r_refdef.vrect.x;
	r_refdef.fvrectx_adj = (float)r_refdef.vrect.x - 0.5;
	r_refdef.vrect_x_adj_shift20 = (r_refdef.vrect.x<<20) + (1<<19) - 1;
	r_refdef.fvrecty = (float)r_refdef.vrect.y;
	r_refdef.fvrecty_adj = (float)r_refdef.vrect.y - 0.5;
	r_refdef.vrectright = r_refdef.vrect.x + r_refdef.vrect.width;
	r_refdef.vrectright_adj_shift20 = (r_refdef.vrectright<<20) + (1<<19) - 1;
	r_refdef.fvrectright = (float)r_refdef.vrectright;
	r_refdef.fvrectright_adj = (float)r_refdef.vrectright - 0.5;
	r_refdef.vrectrightedge = (float)r_refdef.vrectright - 0.99;
	r_refdef.vrectbottom = r_refdef.vrect.y + r_refdef.vrect.height;
	r_refdef.fvrectbottom = (float)r_refdef.vrectbottom;
	r_refdef.fvrectbottom_adj = (float)r_refdef.vrectbottom - 0.5;

	r_refdef.aliasvrect.x = (int)(r_refdef.vrect.x * r_aliasuvscale);
	r_refdef.aliasvrect.y = (int)(r_refdef.vrect.y * r_aliasuvscale);
	r_refdef.aliasvrect.width = (int)(r_refdef.vrect.width * r_aliasuvscale);
	r_refdef.aliasvrect.height = (int)(r_refdef.vrect.height * r_aliasuvscale);
	r_refdef.aliasvrectright = r_refdef.aliasvrect.x +
			r_refdef.aliasvrect.width;
	r_refdef.aliasvrectbottom = r_refdef.aliasvrect.y +
			r_refdef.aliasvrect.height;


#ifdef FISH	
	if (ffov.value && cls.allow_fish)
		pixelAspect = (float)r_refdef.vrect.height/(float)r_refdef.vrect.width;
	else
#endif	
		pixelAspect = aspect;
	xOrigin = r_refdef.xOrigin;
	yOrigin = r_refdef.yOrigin;
#ifdef FISH	
	if (ffov.value && cls.allow_fish)
		screenAspect = 1;
	else
#endif
		screenAspect = r_refdef.vrect.width*pixelAspect /
			r_refdef.vrect.height;
// 320*200 1.0 pixelAspect = 1.6 screenAspect
// 320*240 1.0 pixelAspect = 1.3333 screenAspect
// proper 320*200 pixelAspect = 0.8333333

	verticalFieldOfView = r_refdef.horizontalFieldOfView / screenAspect;

// values for perspective projection
// if math were exact, the values would range from 0.5 to to range+0.5
// hopefully they wll be in the 0.000001 to range+.999999 and truncate
// the polygon rasterization will never render in the first row or column
// but will definately render in the [range] row and column, so adjust the
// buffer origin to get an exact edge to edge fill
	xcenter = ((float)r_refdef.vrect.width * XCENTERING) +
			r_refdef.vrect.x - 0.5;
	aliasxcenter = xcenter * r_aliasuvscale;
	ycenter = ((float)r_refdef.vrect.height * YCENTERING) +
			r_refdef.vrect.y - 0.5;
	aliasycenter = ycenter * r_aliasuvscale;

	xscale = r_refdef.vrect.width / r_refdef.horizontalFieldOfView;
	aliasxscale = xscale * r_aliasuvscale;
	xscaleinv = 1.0 / xscale;
	yscale = xscale * pixelAspect;
	aliasyscale = yscale * r_aliasuvscale;
	yscaleinv = 1.0 / yscale;
	xscaleshrink = (r_refdef.vrect.width-6)/r_refdef.horizontalFieldOfView;
	yscaleshrink = xscaleshrink*pixelAspect;

// left side clip
	screenedge[0].normal[0] = -1.0 / (xOrigin*r_refdef.horizontalFieldOfView);
	screenedge[0].normal[1] = 0;
	screenedge[0].normal[2] = 1;
	screenedge[0].type = PLANE_ANYZ;
	
// right side clip
	screenedge[1].normal[0] =
			1.0 / ((1.0-xOrigin)*r_refdef.horizontalFieldOfView);
	screenedge[1].normal[1] = 0;
	screenedge[1].normal[2] = 1;
	screenedge[1].type = PLANE_ANYZ;
	
// top side clip
	screenedge[2].normal[0] = 0;
	screenedge[2].normal[1] = -1.0 / (yOrigin*verticalFieldOfView);
	screenedge[2].normal[2] = 1;
	screenedge[2].type = PLANE_ANYZ;
	
// bottom side clip
	screenedge[3].normal[0] = 0;
	screenedge[3].normal[1] = 1.0 / ((1.0-yOrigin)*verticalFieldOfView);
	screenedge[3].normal[2] = 1;	
	screenedge[3].type = PLANE_ANYZ;
	
	for (i=0 ; i<4 ; i++)
		VectorNormalize (screenedge[i].normal);

	res_scale = sqrt ((double)(r_refdef.vrect.width * r_refdef.vrect.height) /
			          (320.0 * 152.0)) *
			(2.0 / r_refdef.horizontalFieldOfView);
	r_aliastransition = r_aliastransbase.value * res_scale;
	r_resfudge = r_aliastransadj.value * res_scale;

	if (scr_fov.value <= 90.0)
		r_fov_greater_than_90 = false;
	else
		r_fov_greater_than_90 = true;

// TODO: collect 386-specific code in one place
#if id386
	if (r_pixbytes == 1)
	{
		Sys_MakeCodeWriteable ((long)R_Surf8Start,
						     (long)R_Surf8End - (long)R_Surf8Start);
		colormap = vid.colormap;
		R_Surf8Patch ();
	}
	else
	{
		Sys_MakeCodeWriteable ((long)R_Surf16Start,
						     (long)R_Surf16End - (long)R_Surf16Start);
		colormap = vid.colormap16;
		R_Surf16Patch ();
	}
#endif	// id386

	D_ViewChanged ();	//make sure gamma changes and the like take affect.
}


/*
===============
R_MarkLeaves
===============
*/
qbyte *SWMod_LeafPVS (model_t *model, mleaf_t *leaf, qbyte *buffer);
void SWR_MarkLeaves (void)
{
	qbyte	*vis;
	mnode_t	*node;
	int		i;

#ifdef Q2BSPS
	if (cl.worldmodel->fromgame == fg_quake2)
	{
		qbyte	fatvis[MAX_MAP_LEAFS/8];
		int c;
		mleaf_t	*leaf;
		int		cluster;

//		if (r_oldviewcluster == r_viewcluster && r_oldviewcluster2 == r_viewcluster2)
//			return;

		r_visframecount++;

		r_oldviewcluster = r_viewcluster;
		r_oldviewcluster2 = r_viewcluster2;

		if (/*r_novis.value || */r_viewcluster == -1 || !cl.worldmodel->vis)
		{
			// mark everything
			for (i=0 ; i<cl.worldmodel->numleafs ; i++)
				cl.worldmodel->leafs[i].visframe = r_visframecount;
			for (i=0 ; i<cl.worldmodel->numnodes ; i++)
				cl.worldmodel->nodes[i].visframe = r_visframecount;
			return;
		}

		vis = CM_ClusterPVS (cl.worldmodel, r_viewcluster, NULL);
		// may have to combine two clusters because of solid water boundaries
		if (r_viewcluster2 != r_viewcluster)
		{
			memcpy (fatvis, vis, (cl.worldmodel->numleafs+7)/8);
			vis = CM_ClusterPVS (cl.worldmodel, r_viewcluster2, NULL);//, cl.worldmodel);
			c = (cl.worldmodel->numleafs+31)/32;
			for (i=0 ; i<c ; i++)
				((int *)fatvis)[i] |= ((int *)vis)[i];
			vis = fatvis;
		}
		
		for (i=0,leaf=cl.worldmodel->leafs ; i<cl.worldmodel->numleafs ; i++, leaf++)
		{
			cluster = leaf->cluster;
			if (cluster == -1)
				continue;
			if (vis[cluster>>3] & (1<<(cluster&7)))
			{
				node = (mnode_t *)leaf;
				do
				{
					if (node->visframe == r_visframecount)
						break;
					node->visframe = r_visframecount;
					node = node->parent;
				} while (node);
			}
		}
		return;
	}
#endif

	if (r_oldviewleaf == r_viewleaf)
		return;
	
	r_visframecount++;
	r_oldviewleaf = r_viewleaf;

	vis = SWMod_LeafPVS (cl.worldmodel, r_viewleaf, NULL);
		
	for (i=0 ; i<cl.worldmodel->numleafs ; i++)
	{
		if (vis[i>>3] & (1<<(i&7)))
		{
			node = (mnode_t *)&cl.worldmodel->leafs[i+1];
			do
			{
				if (node->visframe == r_visframecount)
					break;
				node->visframe = r_visframecount;
				node = node->parent;
			} while (node);
		}
	}
}

//temporary
void SWR_DrawBeam(entity_t *e)
{
	particle_t p;
	vec3_t o1, o2;
	vec3_t dir;
	int len;
	VectorSubtract(e->origin, e->oldorigin, dir);
	VectorCopy(e->oldorigin, o1);
	len = VectorNormalize(dir);
	p.alpha = 1;
	p.color = 15;
	for (; len>=0; len--)
	{
		VectorAdd(o1, dir, o2);
		D_DrawSparkTrans (&p, o1, o2, 0);
		VectorCopy(o2, o1);
	}
}

/*
=============
R_DrawEntitiesOnList
=============
*/
void SWR_DrawEntitiesOnList (void)
{
	extern cvar_t gl_part_flame;
	int			i, j;
	int			lnum;
	alight_t	lighting;
// FIXME: remove and do real lighting
	float		lightvec[3] = {-1, 0, 0};
	vec3_t		dist;
	float		add;

	if (!r_drawentities.value)
		return;

	for (i=0 ; i<cl_numvisedicts ; i++)
	{
		currententity = &cl_visedicts[i];

		{
			if (currententity->keynum == (cl.viewentity[r_refdef.currentplayernum]?cl.viewentity[r_refdef.currentplayernum]:(cl.playernum[r_refdef.currentplayernum]+1)))
				continue;
//			if (cl.viewentity[r_refdef.currentplayernum] && currententity->keynum == cl.viewentity[r_refdef.currentplayernum])
//				continue;
			if (!Cam_DrawPlayer(0, currententity->keynum-1))
				continue;
		}

		if (currententity->flags & Q2RF_BEAM)
		{
			SWR_DrawBeam(currententity);
			continue;
		}
		if (!currententity->model)
			continue;

		if (cls.allow_anyparticles || currententity->visframe)	//allowed or static
		{
			if (gl_part_flame.value)
			{
				if (currententity->model->engineflags & MDLF_ENGULPHS)
					continue;
			}
		}

		switch (currententity->model->type)
		{
		case mod_sprite:
			VectorCopy (currententity->origin, r_entorigin);
			VectorSubtract (r_origin, r_entorigin, modelorg);
			R_DrawSprite ();
			break;

		case mod_alias:
			VectorCopy (currententity->origin, r_entorigin);
			VectorSubtract (r_origin, r_entorigin, modelorg);

		// see if the bounding box lets us trivially reject, also sets
		// trivial accept status
			if (R_AliasCheckBBox ())
			{
				float *org;
				extern cvar_t r_fullbrightSkins;
				extern cvar_t r_fb_models;
				float fb = r_fullbrightSkins.value;
				if (fb > cls.allow_fbskins)
					fb = cls.allow_fbskins;
				if (fb < 0)
					fb = 0;

				if (currententity->flags & Q2RF_WEAPONMODEL)
					org = cl.viewent[r_refdef.currentplayernum].origin;
				else
					org = currententity->origin;

				if ((currententity->drawflags & MLS_MASKIN) == MLS_FULLBRIGHT
					|| (currententity->flags & Q2RF_FULLBRIGHT)
					|| (currententity->model->engineflags & MDLF_FLAME))
				{
					lighting.ambientlight = 4096;
					lighting.shadelight = 4096;
					lighting.plightvec = lightvec;
				}
				else if ((currententity->drawflags & MLS_MASKIN) == MLS_ABSLIGHT)
				{
					lighting.shadelight = currententity->abslight;
					lighting.ambientlight = 0;
					lighting.plightvec = lightvec;
				}
				else if (fb >= 1 && r_fb_models.value)
				{
					lighting.ambientlight = 4096;
					lighting.shadelight = 4096;
					lighting.plightvec = lightvec;
				}
				else
				{
					j = SWR_LightPoint (org);
		
					lighting.ambientlight = j+fb * 120;
					lighting.shadelight = j+fb * 120;

					lighting.plightvec = lightvec;

					for (lnum=0 ; lnum<dlights_running ; lnum++)
					{
						if (cl_dlights[lnum].radius)
						{
							VectorSubtract (org,
											cl_dlights[lnum].origin,
											dist);
							add = cl_dlights[lnum].radius - Length(dist);
		
							if (add > 0)
								lighting.ambientlight += add;
						}
					}
	
				// clamp lighting so it doesn't overbright as much
					if (lighting.ambientlight > 128)
						lighting.ambientlight = 128;
					if (lighting.ambientlight + lighting.shadelight > 192)
						lighting.shadelight = 192 - lighting.ambientlight;
				}

				R_AliasDrawModel (&lighting);
			}

			break;

		default:
			break;
		}
	}
}

/*
=============
R_BmodelCheckBBox
=============
*/
int R_BmodelCheckBBox (model_t *clmodel, float *minmaxs)
{
	int			i, *pindex, clipflags;
	vec3_t		acceptpt, rejectpt;
	double		d;

	clipflags = 0;

	if (currententity->angles[0] || currententity->angles[1]
		|| currententity->angles[2])
	{
		for (i=0 ; i<4 ; i++)
		{
			d = DotProduct (currententity->origin, view_clipplanes[i].normal);
			d -= view_clipplanes[i].dist;

			if (d <= -clmodel->radius)
				return BMODEL_FULLY_CLIPPED;

			if (d <= clmodel->radius)
				clipflags |= (1<<i);
		}
	}
	else
	{
		for (i=0 ; i<4 ; i++)
		{
		// generate accept and reject points
		// FIXME: do with fast look-ups or integer tests based on the sign bit
		// of the floating point values

			pindex = pfrustum_indexes[i];

			rejectpt[0] = minmaxs[pindex[0]];
			rejectpt[1] = minmaxs[pindex[1]];
			rejectpt[2] = minmaxs[pindex[2]];
			
			d = DotProduct (rejectpt, view_clipplanes[i].normal);
			d -= view_clipplanes[i].dist;

			if (d <= 0)
				return BMODEL_FULLY_CLIPPED;

			acceptpt[0] = minmaxs[pindex[3+0]];
			acceptpt[1] = minmaxs[pindex[3+1]];
			acceptpt[2] = minmaxs[pindex[3+2]];

			d = DotProduct (acceptpt, view_clipplanes[i].normal);
			d -= view_clipplanes[i].dist;

			if (d <= 0)
				clipflags |= (1<<i);
		}
	}

	return clipflags;
}

mnode_t *R_FindTopnode (vec3_t mins, vec3_t maxs)
{
	mplane_t	*splitplane;
	int			sides;
	mnode_t *node;

	node = cl.worldmodel->nodes;

	while (1)
	{
		if (node->visframe != r_visframecount)
			return NULL;		// not visible at all
		
		if (node->contents != -1)
		{
			if (node->contents != Q2CONTENTS_SOLID)
				return	node; // we've reached a non-solid leaf, so it's
							//  visible and not BSP clipped
			return NULL;	// in solid, so not visible
		}
		
		splitplane = node->plane;
		sides = BOX_ON_PLANE_SIDE(mins, maxs, splitplane);
		
		if (sides == 3)
			return node;	// this is the splitter
		
	// not split yet; recurse down the contacted side
		if (sides & 1)
			node = node->children[0];
		else
			node = node->children[1];
	}
}
void RotatedBBox (vec3_t mins, vec3_t maxs, vec3_t angles, vec3_t tmins, vec3_t tmaxs)
{
	vec3_t	tmp, v;
	int		i, j;
	vec3_t	forward, right, up;

	if (!angles[0] && !angles[1] && !angles[2])
	{
		VectorCopy (mins, tmins);
		VectorCopy (maxs, tmaxs);
		return;
	}

	for (i=0 ; i<3 ; i++)
	{
		tmins[i] = 99999;
		tmaxs[i] = -99999;
	}

	AngleVectors (angles, forward, right, up);

	for ( i = 0; i < 8; i++ )
	{
		if ( i & 1 )
			tmp[0] = mins[0];
		else
			tmp[0] = maxs[0];

		if ( i & 2 )
			tmp[1] = mins[1];
		else
			tmp[1] = maxs[1];

		if ( i & 4 )
			tmp[2] = mins[2];
		else
			tmp[2] = maxs[2];


		VectorScale (forward, tmp[0], v);
		VectorMA (v, -tmp[1], right, v);
		VectorMA (v, tmp[2], up, v);

		for (j=0 ; j<3 ; j++)
		{
			if (v[j] < tmins[j])
				tmins[j] = v[j];
			if (v[j] > tmaxs[j])
				tmaxs[j] = v[j];
		}
	}
}


/*
=============
R_DrawBEntitiesOnList
=============
*/
void R_DrawBEntitiesOnList (void)
{
	int			i, j, k, clipflags;
	vec3_t		oldorigin;
	model_t		*clmodel;
	float		minmaxs[6];
		vec3_t		mins, maxs;
			mnode_t		*topnode;

	model_t *currentmodel;

	if (!r_drawentities.value)
		return;

	VectorCopy (modelorg, oldorigin);
	insubmodel = true;
	r_dlightframecount = r_framecount;

	for (i=0 ; i<cl_numvisedicts ; i++)
	{
		currententity = &cl_visedicts[i];

		if (!currententity->model)
			continue;
		if (currententity->flags & Q2RF_BEAM)
			continue;

		switch (currententity->model->type)
		{
		case mod_brush:
			if (cl.worldmodel->fromgame == fg_quake2)
			{
				currentmodel = currententity->model;
				if (!currentmodel)
					continue;
				if (currentmodel->nummodelsurfaces == 0)
					continue;	// clip brush only
		//		if ( currententity->flags & RF_BEAM )
		//			continue;
		//		if (currentmodel->type != mod_brush)
		//			continue;
			// see if the bounding box lets us trivially reject, also sets
			// trivial accept status
				RotatedBBox (currentmodel->mins, currentmodel->maxs,
					currententity->angles, mins, maxs);
				VectorAdd (mins, currententity->origin, minmaxs);
				VectorAdd (maxs, currententity->origin, (minmaxs+3));

				clipflags = R_BmodelCheckBBox (currentmodel, minmaxs);
				if (clipflags == BMODEL_FULLY_CLIPPED)
					continue;	// off the edge of the screen

				topnode = R_FindTopnode (minmaxs, minmaxs+3);
				if (!topnode)
					continue;	// no part in a visible leaf

				VectorCopy (currententity->origin, r_entorigin);
				VectorSubtract (r_origin, r_entorigin, modelorg);

				r_pcurrentvertbase = currentmodel->vertexes;

			// FIXME: stop transforming twice
				R_RotateBmodel ();

			// calculate dynamic lighting for bmodel
//				R_PushDlights (currentmodel);

				currententity->topnode = r_pefragtopnode = topnode;
				if (topnode->contents == -1)
				{
				// not a leaf; has to be clipped to the world BSP
					r_clipflags = clipflags;
					R_DrawSolidClippedSubmodelPolygons (currentmodel);
				}
				else
				{
				// falls entirely in one leaf, so we just put all the
				// edges in the edge list and let 1/z sorting handle
				// drawing order
					R_DrawSubmodelPolygons (currentmodel, clipflags);//, topnode);
				}
				r_pefragtopnode = NULL;

			// put back world rotation and frustum clipping		
			// FIXME: R_RotateBmodel should just work off base_vxx
				VectorCopy (base_vpn, vpn);
				VectorCopy (base_vup, vup);
				VectorCopy (base_vright, vright);
				VectorCopy (oldorigin, modelorg);
				R_TransformFrustum ();
			}
			else	//q1/hl levels
			{
				clmodel = currententity->model;

			// see if the bounding box lets us trivially reject, also sets
			// trivial accept status
				for (j=0 ; j<3 ; j++)
				{
					minmaxs[j] = currententity->origin[j] +
							clmodel->mins[j];
					minmaxs[3+j] = currententity->origin[j] +
							clmodel->maxs[j];
				}

				clipflags = R_BmodelCheckBBox (clmodel, minmaxs);

				if (clipflags != BMODEL_FULLY_CLIPPED)
				{
					VectorCopy (currententity->origin, r_entorigin);
					VectorSubtract (r_origin, r_entorigin, modelorg);
				// FIXME: is this needed?
					VectorCopy (modelorg, r_worldmodelorg);
			
					r_pcurrentvertbase = clmodel->vertexes;
			
				// FIXME: stop transforming twice
					R_RotateBmodel ();

				// calculate dynamic lighting for bmodel if it's not an
				// instanced model
					if (clmodel->firstmodelsurface != 0)
					{
						for (k=0 ; k<dlights_software ; k++)
						{
							if ((cl_dlights[k].die < cl.time) ||
								(!cl_dlights[k].radius))
							{
								continue;
							}

							SWR_MarkLights (&cl_dlights[k], 1<<k,
								clmodel->nodes + clmodel->hulls[0].firstclipnode);
						}
					}

				// if the driver wants polygons, deliver those. Z-buffering is on
				// at this point, so no clipping to the world tree is needed, just
				// frustum clipping
					if (r_drawpolys | r_drawculledpolys)
					{
						R_ZDrawSubmodelPolys (clmodel);
					}
					else
					{
						if (cl.worldmodel->fromgame == fg_quake2)
						{
							r_pefragtopnode = R_FindTopnode (minmaxs, minmaxs+3);
							if (r_pefragtopnode)
							{
								currententity->topnode = r_pefragtopnode;
			
								if (r_pefragtopnode->contents == -1)
								{
								// not a leaf; has to be clipped to the world BSP
									r_clipflags = clipflags;
									R_DrawSolidClippedSubmodelPolygons (clmodel);
								}
								else
								{
								// falls entirely in one leaf, so we just put all the
								// edges in the edge list and let 1/z sorting handle
								// drawing order
									R_DrawSubmodelPolygons (clmodel, clipflags);
								}
			
								currententity->topnode = NULL;
							}
						}
						else
						{
							r_pefragtopnode = NULL;

							for (j=0 ; j<3 ; j++)
							{
								r_emins[j] = minmaxs[j];
								r_emaxs[j] = minmaxs[3+j];
							}

							R_Q1BSP_SplitEntityOnNode2 (cl.worldmodel->nodes);

							if (r_pefragtopnode)
							{
								currententity->topnode = r_pefragtopnode;
			
								if (r_pefragtopnode->contents >= 0)
								{
								// not a leaf; has to be clipped to the world BSP
									r_clipflags = clipflags;
									R_DrawSolidClippedSubmodelPolygons (clmodel);
								}
								else
								{
								// falls entirely in one leaf, so we just put all the
								// edges in the edge list and let 1/z sorting handle
								// drawing order
									R_DrawSubmodelPolygons (clmodel, clipflags);
								}
			
								currententity->topnode = NULL;
							}
						}
					}

				// put back world rotation and frustum clipping		
				// FIXME: R_RotateBmodel should just work off base_vxx
					VectorCopy (base_vpn, vpn);
					VectorCopy (base_vup, vup);
					VectorCopy (base_vright, vright);
					VectorCopy (base_modelorg, modelorg);
					VectorCopy (oldorigin, modelorg);
					R_TransformFrustum ();
				}
			}
			break;

		default:
			break;
		}
	}

	insubmodel = false;
}


/*
================
R_EdgeDrawing
================
*/
void R_EdgeDrawing (void)
{
	edge_t	ledges[NUMSTACKEDGES +
				((CACHE_SIZE - 1) / sizeof(edge_t)) + 1];
	surf_t	lsurfs[NUMSTACKSURFACES +
				((CACHE_SIZE - 1) / sizeof(surf_t)) + 1];

	if (auxedges)
	{
		r_edges = auxedges;
	}
	else
	{
		r_edges =  (edge_t *)
				(((long)&ledges[0] + CACHE_SIZE - 1) & ~(CACHE_SIZE - 1));
	}

	if (r_surfsonstack)
	{
		surfaces =  (surf_t *)
				(((long)&lsurfs[0] + CACHE_SIZE - 1) & ~(CACHE_SIZE - 1));
		surf_max = &surfaces[r_cnumsurfs];
	// surface 0 doesn't really exist; it's just a dummy because index 0
	// is used to indicate no edge attached to surface
		surfaces--;
		R_SurfacePatch ();
	}

	R_BeginEdgeFrame ();

	if (r_dspeeds.value)
	{
		rw_time1 = Sys_DoubleTime ();
	}

	R_RenderWorld ();

	if (r_drawculledpolys)
		R_ScanEdges ();

// only the world can be drawn back to front with no z reads or compares, just
// z writes, so have the driver turn z compares on now
	D_TurnZOn ();

	if (r_dspeeds.value)
	{
		rw_time2 = Sys_DoubleTime ();
		db_time1 = rw_time2;
	}

	R_DrawBEntitiesOnList ();

	if (r_dspeeds.value)
	{
		db_time2 = Sys_DoubleTime ();
		se_time1 = db_time2;
	}

	if (!r_dspeeds.value)
	{
		S_ExtraUpdate ();	// don't let sound get messed up if going slow
	}
	
	if (!(r_drawpolys | r_drawculledpolys))
		R_ScanEdges ();

	SWR_DrawAlphaSurfaces();
}


void R_ApplySIRDAlgorithum(void);
qboolean r_dosirds = true;
/*
================
R_RenderView

r_refdef must be set before the first call
================
*/
void SWR_RenderView (void)
{
	qbyte	warpbuffer[WARP_WIDTH * WARP_HEIGHT];

	r_warpbuffer = warpbuffer;
	r_dosirds = r_sirds.value;

#ifdef FISH
	if (ffov.value && cls.allow_fish)	//THAT's HORRIBLE!
		r_dosirds = false;
#endif

	if (r_timegraph.value || r_speeds.value || r_dspeeds.value)
		r_time1 = Sys_DoubleTime ();

	SWR_SetupFrame ();

	if (r_refdef.flags & 1)
	{
		D_ClearDepth();
		SWR_DrawEntitiesOnList ();
		return;
	}

#ifdef PASSAGES
SetVisibilityByPassages ();
#else
	SWR_MarkLeaves ();	// done here so we know if we're in water
#endif

// make FDIV fast. This reduces timing precision after we've been running for a
// while, so we don't do it globally.  This also sets chop mode, and we do it
// here so that setup stuff like the refresh area calculations match what's
// done in screen.c
	Sys_LowFPPrecision ();

	if (!r_worldentity.model || !cl.worldmodel)
		Sys_Error ("R_RenderView: NULL worldmodel");
		
	if (!r_dspeeds.value)
	{
		S_ExtraUpdate ();	// don't let sound get messed up if going slow
	}
	
	R_EdgeDrawing ();


	if (!r_dspeeds.value)
	{
		VID_UnlockBuffer ();
		S_ExtraUpdate ();	// don't let sound get messed up if going slow
		VID_LockBuffer ();
	}
	
	if (r_dspeeds.value)
	{
		se_time2 = Sys_DoubleTime ();
		de_time1 = se_time2;
	}

	SWR_DrawEntitiesOnList ();

	if (r_dspeeds.value)
	{
		de_time2 = Sys_DoubleTime ();
		dv_time1 = de_time2;
	}

	if (r_dspeeds.value)
	{
		dv_time2 = Sys_DoubleTime ();
		dp_time1 = Sys_DoubleTime ();
	}

	P_DrawParticles ();

	if (r_dspeeds.value)
		dp_time2 = Sys_DoubleTime ();

	if (r_dosirds)
	{
		R_ApplySIRDAlgorithum();
	}
	else if (r_dowarp)
		D_WarpScreen ();

	V_SetContentsColor (r_viewleaf->contents);

	if (r_timegraph.value)
		R_TimeGraph ();

	if (r_netgraph.value)
		SWR_NetGraph ();

	if (r_zgraph.value)
		R_ZGraph ();

	if (r_aliasstats.value)
		R_PrintAliasStats ();
		
	if (r_speeds.value)
		R_PrintTimes ();

	if (r_dspeeds.value)
		R_PrintDSpeeds ();

	if (r_reportsurfout.value && r_outofsurfaces)
		Con_Printf ("Short %d surfaces\n", r_outofsurfaces);

	if (r_reportedgeout.value && r_outofedges)
		Con_Printf ("Short roughly %d edges\n", r_outofedges * 2 / 3);

// back to high floating-point precision
	Sys_HighFPPrecision ();
}

/*
void SWR_RenderView (void)
{
	int		dummy;
	int		delta;
	
	delta = (qbyte *)&dummy - r_stack_start;
	if (delta < -10000 || delta > 10000)
		Sys_Error ("R_RenderView: called without enough stack");

	if ( Hunk_LowMark() & 3 )
		Sys_Error ("Hunk is missaligned");

	if ( (long)(&dummy) & 3 )
		Sys_Error ("Stack is missaligned");

	if ( (long)(&r_warpbuffer) & 3 )
		Sys_Error ("Globals are missaligned");

	SWR_RenderView_ ();
}
*/

/*
================
R_InitTurb
================
*/
void R_InitTurb (void)
{
	int		i;
	
	for (i=0 ; i<SINTABLESIZE ; i++)
	{
		sintable[i] = AMP + sin(i*3.14159*2/CYCLE)*AMP;
		intsintable[i] = AMP2 + sin(i*3.14159*2/CYCLE)*AMP2;	// AMP2, not 20
	}
}



































/*
** Start Added by Lewey
**
** This is where the real SIRDS code is
*/

//width of the repeating pattern. Increasing this will
//increase the quality of the SIRD by giving it more
//height levels.
//
//Make sure: ((R_SIRDw % 3) == 0)
//			&& (((R_SIRDw / 3) % R_SIRDExponents) == 0)
#define R_SIRDw 144

//height of the repeating pattern (not really important)
#define R_SIRDh 50

//maximum offset. This is the max number of pixels
//an item can be moved due to it's height, this is
//is also obviously then the number of different
//height layers you can have. A large R_SIRDw will
//make it harder and harder to see the image, a larger
//ratio of R_SIRDw (i.e. less than 3) will eventually
//cause your eyes to be unable to see the pattern.
#define R_SIRDmaxDiff (R_SIRDw / 3)

//the number of lower powers to ignore
#define R_SIRDIgnoreExponents	5

//the number of exponents (after ignored ones) to have different
//height values (ones after it are rounded to the max difference)
#define R_SIRDExponents			6

//the number of height levels each exponent is given
#define R_SIRDstepsPerExponent (R_SIRDmaxDiff / R_SIRDExponents)

//this is the z value of the sky, which logically should be 0, but
//for implimentation reasons is made very high. Not my doing by the
//way. If you move to a different platform, you may need to change this
#define R_SIRD_ZofSky 0x8ccc

//this is the number of random numbers
//defined in "rand1k.h"
#define R_SIRDnumRand 103

//this hold the background pattern
qbyte *r_SIRDBackground;//[R_SIRDw * R_SIRDh];

//these are the actual random numbers
qbyte *r_SIRDrandValues;//[R_SIRDnumRand];

#include "d_local.h"
void InitSIRD(void)
{
	int i;
	if (!r_SIRDBackground)
	{
		r_SIRDBackground = BZ_Malloc(R_SIRDw * R_SIRDh + R_SIRDnumRand);
		r_SIRDrandValues = r_SIRDBackground + R_SIRDw * R_SIRDh;

		for (i = 0; i < R_SIRDnumRand; i++)
			r_SIRDrandValues[i] = rand();
	}
}

void CloseSIRD(void)
{
	if (r_SIRDBackground)
		BZ_Free(r_SIRDBackground);
}

//Only used if id386 is false, this acts as a
// reverse bit-scanner, and uses a sort of binary
// search to find the index of the highest set bit.
//You could also expand the loop 4 times to remove
// the 'while'
#if	!(id386 && defined(_MSC_VER))
static int UShortLog(int val)
{
	int mask = 0xff00;
	int p = 0;
	int b = 8;
	while (b)
	{
		if (val & mask)
		{
			p += b;
			b >>= 1;
			mask &= (mask << b);
		}
		else
		{
			mask &= (mask << (b >> 1));
			mask >>= b;
			b >>= 1;
		}
	}
	return p;
}
#endif

static int R_SIRDZFunc(int sub)
{
	int e;

	//special case the sky.
	if (sub == 	R_SIRD_ZofSky)
		return 0;

#if	id386 && defined(_MSC_VER)
	e = sub;
	//calculate the log (base 2) of the number. In other
	//words the index of the highest set bit. bsr is undefined
	//if it's input is 0, so special case that.
	if (e!=0)
	{
		__asm 
		{
			mov ebx, e
			bsr eax, ebx
			mov e, eax
		}
	}
#else
	e = UShortLog(sub);
#endif

	//clip the exponent
	if (e < R_SIRDIgnoreExponents)
		return 0;

	// based on the power, shift the z so that
	// it's as high as it can get while still staying
	// under 0x100
	if (e > 8)
	{
		sub >>= (e-8);
	}
	else
	{
		if (e < 8)
		{
			sub <<= (8-e);
		}
	}

	// Lower the power of the number, this helps scaling and removes
	// small z values.
	e -= R_SIRDIgnoreExponents;

	// contruct the height value. The power is used as the primary calculator,
	// and then the extra bits are used to offset. In this way you
	// get more detail than just the log of the z value, and it works
	// as a pretty good approximation of it.
	e *= R_SIRDstepsPerExponent;
	e += ((sub * R_SIRDstepsPerExponent) >> 8);

	//make sure we stay under maximum height.
	return ((e<=R_SIRDmaxDiff)? e : R_SIRDmaxDiff );
}

#if 0
void R_ApplyFog(void)
{
	// test code for fog, the real implementation should use a lookup table
	qbyte *pbuf;
	short *zbuf;
	extern short *d_pzbuffer;
	int y, x;
	float v;

	for (y=0 ; y<vid.height ; y++)
	{
		pbuf = (qbyte *)(vid.buffer + vid.rowbytes*y);
		zbuf = d_pzbuffer + (vid.width*y);

		for (x=0 ; x<vid.width ; x++)
		{
			if (!zbuf[x])
				D_SetTransLevel(1.0f, BM_ADD);
			else
			{
				v = 64.0f / zbuf[x];
				v = bound(0, v, 1);
				D_SetTransLevel(v, BM_ADD);
			}

			pbuf[x] = AddBlend(pbuf[x], 74);
		}
	}
}
#endif

void R_ApplySIRDAlgorithum(void)
{
	unsigned short* curz, *oldz;
	unsigned short cz, lastz;
	qbyte* curp;
	qbyte* curbp, j;
	int x, y, i, zinc, k;

	//note of interest: I've made this static so that
	//if you like you could make it not static and see
	//what would happen if you didn't change the background
	static int ji = 0;

	InitSIRD();

	if (cl.paused)
		ji = 0;

	//create the background image to tile
	//basically done by shifting the values around
	//each time and xoring them with a randomly
	//selected pixel
	j = 0;
	for (i=0; i<R_SIRDw * R_SIRDh; i++)
	{
		if ((i%R_SIRDnumRand)==0)
		{
			ji++; 
			ji %= R_SIRDnumRand;
			j = r_SIRDrandValues[r_SIRDrandValues[ji] % R_SIRDnumRand];
		}
		r_SIRDBackground[i] = r_SIRDrandValues[ (i%R_SIRDnumRand) ] ^ j;
	}

	//if we are under water:
	if ((r_dowarp) && (vid.width != WARP_WIDTH))
	{
		//the rendering is only in the top left
		//WARP_WIDTH by WARP_HEIGHT area, so scale the z-values
		//to span over the whole screen


		//why are we going backwards? so that we don't write over the
		//values before we read from them

		zinc = ((WARP_WIDTH * 0x10000) / vid.width);
		for (y=vid.height-1; y>=0; y--)
		{
			curz = (d_pzbuffer + (vid.width * y));
			oldz = (d_pzbuffer + (vid.width * ((y*WARP_HEIGHT)/vid.height) ));
			k = (zinc * (vid.width-1));

			for (x=vid.width-1; x>=0; x--)
			{
				curz[x] = oldz[k >> 16];
				k -= zinc;
			}
		}
	}


	//SIRDify each line
	for (y=0; y<vid.height; y++)
	{
		curp = (vid.buffer + (vid.rowbytes * y));
		curz = (d_pzbuffer + (vid.width * y ));

#ifdef _DEBUG
		if (r_dosirds == 2)
		{
			//if we are just drawing the height map
			//this lets you see which layers are used to
			//create the SIRD
			//
			//NOTE: even though it may sort of look like
			//a grey-scale height map, that is merely a
			//coincidence because of how the colours are
			//organized in the pallette.

			lastz = 0;
			cz = 0;
			for (x=0; x<vid.width; x++)
			{
				if (lastz != *curz)
				{
					lastz = *curz;
					cz = R_SIRDZFunc(*curz);
				}

				*curp = cz;

				curp++;
				curz++;
			}
		}
		else
#endif
		{
			// draw the SIRD

			// copy the background into the left most column
			curbp = &(r_SIRDBackground[ R_SIRDw * (y % R_SIRDh) ]);
			for (x=0; x<R_SIRDw; x++)
			{
				*curp = *curbp;
				curp++;
				curbp++; 
			}

			lastz = 0;
			cz = 0;
			curz += R_SIRDw;
			curbp = curp - R_SIRDw;

			// now calculate the SIRD
			for (x=R_SIRDw; x<vid.width; x++)
			{
				//only call the z-function with a new
				//value, it is slow so this saves quite
				//some time.
				if (lastz != *curz)
				{
					lastz = *curz;

					//convert from z to height offset
					cz = R_SIRDZFunc(lastz);

					//the "height offset" used in making SIRDS
					//can be considered an adjustment of the 
					//frequency of repetition in the pattern.
					//so here we are copying from bp to p, and so
					//it simply increases or decreases the distance
					//between the two.
					curbp = (curp - R_SIRDw + cz);
				}

				*curp = *curbp;

				curp++;
				curbp++;
				curz++;
			}
		}
	}
}

/*
** End Added by Lewey
*/
