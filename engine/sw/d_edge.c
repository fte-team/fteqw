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
// d_edge.c

#include "quakedef.h"
#include "d_local.h"

static int	miplevel;

float		scale_for_mip;
int			screenwidth;
int			ubasestep, errorterm, erroradjustup, erroradjustdown;
int			vstartscan;
int r_wallindex, r_floorindex, r_skycolorindex;

// FIXME: should go away
extern void			R_RotateBmodel (void);
extern void			R_TransformFrustum (void);

vec3_t		transformed_modelorg;

/*
==============
D_DrawPoly

==============
*/
void D_DrawPoly (void)
{
// this driver takes spans, not polygons
}


/*
=============
D_MipLevelForScale
=============
*/
int D_MipLevelForScale (float scale)
{
	int		lmiplevel;

	if (scale >= d_scalemip[0] )
		lmiplevel = 0;
	else if (scale >= d_scalemip[1] )
		lmiplevel = 1;
	else if (scale >= d_scalemip[2] )
		lmiplevel = 2;
	else
		lmiplevel = 3;

	if (lmiplevel < d_minmip)
		lmiplevel = d_minmip;

	return lmiplevel;
}

/*
==============
D_DrawSolidSurface
==============
*/

// FIXME: clean this up
void D_DrawSolidSurface (surf_t *surf, int color)
{
	espan_t	*span;
	qbyte	*pdest;
	int		u, u2, pix;
	
	if (r_pixbytes == 4)
	{
		unsigned int	*p32dest;
		pix = d_8to32table[color];
		for (span=surf->spans ; span ; span=span->pnext)
		{
			p32dest = (unsigned int *)d_viewbuffer + screenwidth*span->v;
			u = span->u;
			u2 = span->u + span->count - 1;
			p32dest[u] = pix;

			for ( ; u <= u2 ; u++)
				p32dest[u] = pix;
		}
	}
	else if (r_pixbytes == 2)
	{
		unsigned short	*p16dest;
		pix = vid.colormap16[color];
		for (span=surf->spans ; span ; span=span->pnext)
		{
			p16dest = (unsigned short *)d_viewbuffer + screenwidth*span->v;
			u = span->u;
			u2 = span->u + span->count - 1;
			p16dest[u] = pix;

			for ( ; u <= u2 ; u++)
				p16dest[u] = pix;
		}
	}
	else
	{
		pix = (color<<24) | (color<<16) | (color<<8) | color;
		for (span=surf->spans ; span ; span=span->pnext)
		{
			pdest = (qbyte *)d_viewbuffer + screenwidth*span->v;
			u = span->u;
			u2 = span->u + span->count - 1;
			((qbyte *)pdest)[u] = pix;

			if (u2 - u < 8)
			{
				for (u++ ; u <= u2 ; u++)
					((qbyte *)pdest)[u] = pix;
			}
			else
			{
				for (u++ ; u & 3 ; u++)
					((qbyte *)pdest)[u] = pix;

				u2 -= 3;
				for ( ; u <= u2 ; u+=4)
					*(int *)((qbyte *)pdest + u) = pix;
				u2 += 3;
				for ( ; u <= u2 ; u++)
					((qbyte *)pdest)[u] = pix;
			}
		}
	}
}


/*
==============
D_CalcGradients
==============
*/
void D_CalcGradients (msurface_t *pface)
{
	mplane_t	*pplane;
	float		mipscale;
	vec3_t		p_temp1;
	vec3_t		p_saxis, p_taxis;
	float		t;

	pplane = pface->plane;

	mipscale = 1.0 / (float)(1 << miplevel);

	TransformVector (pface->texinfo->vecs[0], p_saxis);
	TransformVector (pface->texinfo->vecs[1], p_taxis);

	t = xscaleinv * mipscale;
	d_sdivzstepu = p_saxis[0] * t;
	d_tdivzstepu = p_taxis[0] * t;

	t = yscaleinv * mipscale;
	d_sdivzstepv = -p_saxis[1] * t;
	d_tdivzstepv = -p_taxis[1] * t;

	d_sdivzorigin = p_saxis[2] * mipscale - xcenter * d_sdivzstepu -
			ycenter * d_sdivzstepv;
	d_tdivzorigin = p_taxis[2] * mipscale - xcenter * d_tdivzstepu -
			ycenter * d_tdivzstepv;

	VectorScale (transformed_modelorg, mipscale, p_temp1);

	t = 0x10000*mipscale;
	sadjust = ((fixed16_t)(DotProduct (p_temp1, p_saxis) * 0x10000 + 0.5)) -
			((pface->texturemins[0] << 16) >> miplevel)
			+ pface->texinfo->vecs[0][3]*t;
	tadjust = ((fixed16_t)(DotProduct (p_temp1, p_taxis) * 0x10000 + 0.5)) -
			((pface->texturemins[1] << 16) >> miplevel)
			+ pface->texinfo->vecs[1][3]*t;

//
// -1 (-epsilon) so we never wander off the edge of the texture
//
	bbextents = ((pface->extents[0] << 16) >> miplevel) - 1;
	bbextentt = ((pface->extents[1] << 16) >> miplevel) - 1;
}

void SWR_Drawflat_Callback(struct cvar_s *var, char *oldvalue)
{
	D_FlushCaches();
}

void SWR_Floorcolour_Callback(struct cvar_s *var, char *oldvalue)
{
	r_floorindex = fbremapidx(SCR_StringToPalIndex(var->string, 255));
	D_FlushCaches();
}

void SWR_Wallcolour_Callback(struct cvar_s *var, char *oldvalue)
{
	r_wallindex = fbremapidx(SCR_StringToPalIndex(var->string, 255));
	D_FlushCaches();
}

void SWR_Fastskycolour_Callback(struct cvar_s *var, char *oldvalue)
{
	r_skycolorindex = SCR_StringToPalIndex(var->string, 255);
}

/*
==============
D_DrawSurfaces
==============
*/
void D_DrawSurfaces (void)
{
	void D_DrawSpans32 (espan_t *pspan);
	surf_t			*s;
	msurface_t		*pface;
	surfcache_t		*pcurrentcache;
	vec3_t			world_transformed_modelorg;
	vec3_t			local_modelorg;
	extern int r_dosirds;
	extern cvar_t r_fastsky, r_fastskycolour, r_drawflat;

	currententity = &r_worldentity;
	TransformVector (modelorg, transformed_modelorg);
	VectorCopy (transformed_modelorg, world_transformed_modelorg);

	if (r_dosirds)	//depth only
	{
		for (s = &surfaces[1] ; s<surface_p ; s++)
		{
			if (!s->spans)
				continue;

			d_zistepu = s->d_zistepu;
			d_zistepv = s->d_zistepv;
			d_ziorigin = s->d_ziorigin;
			D_DrawZSpans (s->spans);
		}
	}
	else
	{
		for (s = &surfaces[1] ; s<surface_p ; s++)
		{
			if (!s->spans)
				continue;

			r_drawnpolycount++;

			d_zistepu = s->d_zistepu;
			d_zistepv = s->d_zistepv;
			d_ziorigin = s->d_ziorigin;

			if (s->flags & SURF_DRAWSKY)
			{
				if (r_fastsky.value || r_worldentity.model->fromgame != fg_quake)
				{
					D_DrawSolidSurface (s, r_skycolorindex & 0xFF);
				}
				else
				{
					if (!r_skymade)
					{
						R_MakeSky ();
					}

					if (r_pixbytes == 4)
						D_DrawSkyScans32 (s->spans);
					else if (r_pixbytes == 2)
						D_DrawSkyScans16 (s->spans);
					else
						D_DrawSkyScans8 (s->spans);
				}
				D_DrawZSpans (s->spans);
			}
			else if (s->flags & SURF_DRAWSKYBOX)
			{
				pface = s->data;
				miplevel = 0;
				if (!pface->texinfo->texture)
				{
					d_zistepu = 0;
					d_zistepv = 0;
					d_ziorigin = -0.9;

					D_DrawSolidSurface (s, r_skycolorindex & 0xFF);
					D_DrawZSpans (s->spans);
					continue;
				}
				cacheblock = (qbyte *)pface->texinfo->texture + pface->texinfo->texture->offsets[0];
				cachewidth = 256;
				cacheheight = 256;

				d_zistepu = s->d_zistepu;
				d_zistepv = s->d_zistepv;
				d_ziorigin = s->d_ziorigin;

				D_CalcGradients (pface);

				if (r_pixbytes == 2)
					D_DrawSpans16From8(s->spans);
				else
					(*d_drawspans) (s->spans);

			// set up a gradient for the background surface that places it
			// effectively at infinity distance from the viewpoint
				d_zistepu = 0;
				d_zistepv = 0;
				d_ziorigin = -0.9;

				D_DrawZSpans (s->spans);
			}
			else if (s->flags & SURF_DRAWBACKGROUND)
			{
			// set up a gradient for the background surface that places it
			// effectively at infinity distance from the viewpoint
				d_zistepu = 0;
				d_zistepv = 0;
				d_ziorigin = -0.9;

				D_DrawSolidSurface (s, (int)r_clearcolor.value & 0xFF);
				D_DrawZSpans (s->spans);
			}
			else if (s->flags & SURF_DRAWTURB)
			{
				pface = s->data;
				miplevel = 0;
				cacheblock = (pixel_t *)
						((qbyte *)pface->texinfo->texture +
						pface->texinfo->texture->offsets[0]);
				cachewidth = 64;
				cacheheight = 64;

				if (s->insubmodel)
				{
				// FIXME: we don't want to do all this for every polygon!
				// TODO: store once at start of frame
					currententity = s->entity;	//FIXME: make this passed in to
												// R_RotateBmodel ()
					VectorSubtract (r_origin, currententity->origin,
							local_modelorg);
					TransformVector (local_modelorg, transformed_modelorg);

					R_RotateBmodel ();	// FIXME: don't mess with the frustum,
										// make entity passed in
				}

				D_CalcGradients (pface);

				if (r_pixbytes == 4)
					Turbulent32 (s->spans);
				else if (r_pixbytes == 2)
					Turbulent16 (s->spans);
				else
					Turbulent8 (s->spans);
				D_DrawZSpans (s->spans);

				if (s->insubmodel)
				{
				//
				// restore the old drawing state
				// FIXME: we don't want to do this every time!
				// TODO: speed up
				//
					currententity = &r_worldentity;
					VectorCopy (world_transformed_modelorg,
								transformed_modelorg);
					VectorCopy (base_vpn, vpn);
					VectorCopy (base_vup, vup);
					VectorCopy (base_vright, vright);
					VectorCopy (base_modelorg, modelorg);
					R_TransformFrustum ();
				}
			}
			else
			{
				if (s->insubmodel)
				{
				// FIXME: we don't want to do all this for every polygon!
				// TODO: store once at start of frame
					currententity = s->entity;	//FIXME: make this passed in to
												// R_RotateBmodel ()
					VectorSubtract (r_origin, currententity->origin, local_modelorg);
					TransformVector (local_modelorg, transformed_modelorg);

					R_RotateBmodel ();	// FIXME: don't mess with the frustum,
										// make entity passed in
				}

				pface = s->data;

				if (pface->flags & SURF_BULLETEN)
				{
					miplevel = 0;
					if (pface->cachespots[miplevel])
						pface->cachespots[miplevel]->texture = NULL;
				}
				else
				{
					miplevel = D_MipLevelForScale (s->nearzi * scale_for_mip
											* pface->texinfo->mipadjust);
				}

			// FIXME: make this passed in to D_CacheSurface
				pcurrentcache = D_CacheSurface (pface, miplevel);

				cacheblock = (pixel_t *)pcurrentcache->data;
				cachewidth = pcurrentcache->width;

				cacheheight = pcurrentcache->height;

//				if (s->entity == &r_worldentity)	//temporary
//				{
				D_CalcGradients (pface);
				(*d_drawspans) (s->spans);

				if (d_drawspans != D_DrawSpans32)
					D_DrawZSpans (s->spans);
//				}

				if (s->insubmodel)
				{
				//
				// restore the old drawing state
				// FIXME: we don't want to do this every time!
				// TODO: speed up
				//
					VectorCopy (world_transformed_modelorg,
								transformed_modelorg);
					VectorCopy (base_vpn, vpn);
					VectorCopy (base_vup, vup);
					VectorCopy (base_vright, vright);
					VectorCopy (base_modelorg, modelorg);
					R_TransformFrustum ();
					currententity = &r_worldentity;
				}
			}
		}
	}
}

