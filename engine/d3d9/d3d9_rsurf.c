#include "quakedef.h"
#ifdef D3DQUAKE
#include "winquake.h"
#include "d3d9quake.h"

int numlightmaps;

mvertex_t *r_pcurrentvertbase;

#define LMBLOCK_WIDTH 128
#define LMBLOCK_HEIGHT LMBLOCK_WIDTH

LPDIRECT3DTEXTURE9 *lightmap_d3d9textures;
LPDIRECT3DTEXTURE9 *deluxmap_d3d9textures;
lightmapinfo_t **lightmap;



void D3D9_BuildSurfaceDisplayList (msurface_t *fa)
{
	int			i, lindex, lnumverts;
	medge_t		*pedges, *r_pedge;
	int			vertpage;
	float		*vec;
	float		s, t;
	int	lm;

	int size;
	mesh_t *mesh;

// reconstruct the polygon
	pedges = currentmodel->edges;
	lnumverts = fa->numedges;
	vertpage = 0;


	if (lnumverts<3)
		return;	//q3 map.

	size = sizeof(mesh_t) + sizeof(index_t)*(lnumverts-2)*3 + (sizeof(vec3_t) + sizeof(vec3_t) + 2*sizeof(vec2_t) + sizeof(byte_vec4_t))*lnumverts;

	fa->mesh = mesh = Hunk_Alloc(size);
	mesh->xyz_array = (vec3_t*)(mesh + 1);
	mesh->normals_array = (vec3_t*)(mesh->xyz_array + lnumverts);
	mesh->st_array = (vec2_t*)(mesh->normals_array + lnumverts);
	mesh->lmst_array = (vec2_t*)(mesh->st_array + lnumverts);
	mesh->colors_array = (byte_vec4_t*)(mesh->lmst_array + lnumverts);
	mesh->indexes = (index_t*)(mesh->colors_array + lnumverts);

	mesh->numindexes = (lnumverts-2)*3;
	mesh->numvertexes = lnumverts;
	mesh->patchWidth = mesh->patchHeight = 1;

	for (i=0 ; i<lnumverts-2 ; i++)
	{
		mesh->indexes[i*3] = 0;
		mesh->indexes[i*3+1] = i+1;
		mesh->indexes[i*3+2] = i+2;
	}

	for (i=0 ; i<lnumverts ; i++)
	{
		lindex = currentmodel->surfedges[fa->firstedge + i];

		if (lindex > 0)
		{
			r_pedge = &pedges[lindex];
			vec = r_pcurrentvertbase[r_pedge->v[0]].position;
		}
		else
		{
			r_pedge = &pedges[-lindex];
			vec = r_pcurrentvertbase[r_pedge->v[1]].position;
		}

		s = DotProduct (vec, fa->texinfo->vecs[0]) + fa->texinfo->vecs[0][3];
		t = DotProduct (vec, fa->texinfo->vecs[1]) + fa->texinfo->vecs[1][3];

		VectorCopy (vec, mesh->xyz_array[i]);
		mesh->xyz_array[i][3] = 1;
		mesh->st_array[i][0] = s/fa->texinfo->texture->width;
		mesh->st_array[i][1] = t/fa->texinfo->texture->height;

		s -= fa->texturemins[0];
		lm = s*fa->light_t;
		s += fa->light_s*16;
		s += 8;
		s /= LMBLOCK_WIDTH*16;

		t -= fa->texturemins[1];
		lm += t;
		t += fa->light_t*16;
		t += 8;
		t /= LMBLOCK_HEIGHT*16;

		mesh->lmst_array[i][0] = s;
		mesh->lmst_array[i][1] = t;

		if (fa->flags & SURF_PLANEBACK)
			VectorNegate(fa->plane->normal, mesh->normals_array[i]);
		else
			VectorCopy(fa->plane->normal, mesh->normals_array[i]);

		mesh->colors_array[i][0] = 255;
		mesh->colors_array[i][1] = 255;
		mesh->colors_array[i][2] = 255;
		mesh->colors_array[i][3] = 255;
	}
}







#define MAX_LIGHTMAP_SIZE LMBLOCK_WIDTH

vec3_t			blocknormals[MAX_LIGHTMAP_SIZE*MAX_LIGHTMAP_SIZE];
unsigned		blocklights[MAX_LIGHTMAP_SIZE*MAX_LIGHTMAP_SIZE];

unsigned		greenblklights[MAX_LIGHTMAP_SIZE*MAX_LIGHTMAP_SIZE];
unsigned		blueblklights[MAX_LIGHTMAP_SIZE*MAX_LIGHTMAP_SIZE];

void D3D9R_BuildLightMap (msurface_t *surf, qbyte *dest, qbyte *deluxdest, stmap *stainsrc, int shift)
{
	int			smax, tmax;
	int			t;
	int			i, j, size;
	qbyte		*lightmap;
	unsigned	scale;
	int			maps;
	unsigned	*bl;
	qboolean isstained;
	extern cvar_t r_ambient;
	extern cvar_t gl_lightmap_shift;

	unsigned	*blg;
	unsigned	*blb;

	int r, g, b;
	int cr, cg, cb;

	int stride = LMBLOCK_WIDTH*lightmap_bytes;

	shift += 7; // increase to base value
	surf->cached_dlight = (surf->dlightframe == r_framecount);

	smax = (surf->extents[0]>>4)+1;
	tmax = (surf->extents[1]>>4)+1;
	size = smax*tmax;
	lightmap = surf->samples;

	if (size > MAX_LIGHTMAP_SIZE*MAX_LIGHTMAP_SIZE)
	{	//fixme: fill in?
		Con_Printf("Lightmap too large\n");
		return;
	}
	
//	if (currentmodel->deluxdata)
//		GLR_BuildDeluxMap(surf, deluxdest);



	if (true)
	{
		// set to full bright if no light data
		if (r_fullbright.value>0)	//not qw
		{
			for (i=0 ; i<size ; i++)
			{
				blocklights[i] = r_fullbright.value*255*256;
				greenblklights[i] = r_fullbright.value*255*256;
				blueblklights[i] = r_fullbright.value*255*256;
			}
//			if (r_fullbright.value < 1)
			{
//				if (surf->dlightframe == r_framecount)
//					GLR_AddDynamicLightsColours (surf);
			}
			goto store;
		}
		if (!currentmodel->lightdata)
		{
			for (i=0 ; i<size ; i++)
			{
				blocklights[i] = 255*256;
				greenblklights[i] = 255*256;
				blueblklights[i] = 255*256;
			}
//				if (surf->dlightframe == r_framecount)
//					GLR_AddDynamicLightsColours (surf);
			goto store;
		}

// clear to no light
		t = r_ambient.value*255;
		for (i=0 ; i<size ; i++)
		{
			blocklights[i] = t;
			greenblklights[i] = t;
			blueblklights[i] = t;
		}

// add all the lightmaps
		if (lightmap)
		{
			if (currentmodel->fromgame == fg_quake3)	//rgb
			{
			/*	for (maps = 0 ; maps < MAXLIGHTMAPS && surf->styles[maps] != 255 ;
					 maps++)	//no light styles in q3 apparently.
				{
					scale = d_lightstylevalue[surf->styles[maps]];
					surf->cached_light[maps] = scale;	// 8.8 fraction
					surf->cached_colour[maps] = cl_lightstyle[surf->styles[maps]].colour;
				}
					 */
				for (i = 0; i < tmax; i++)	//q3 maps store thier light in a block fashion, q1/q2/hl store it in a linear fashion.
				{
					for (j = 0; j < smax; j++)
					{
						blocklights[i*smax+j]		= 255*lightmap[(i*LMBLOCK_WIDTH+j)*3];
						greenblklights[i*smax+j]	= 255*lightmap[(i*LMBLOCK_WIDTH+j)*3+1];
						blueblklights[i*smax+j]		= 255*lightmap[(i*LMBLOCK_WIDTH+j)*3+2];
					}
				}
//				memset(blocklights, 255, sizeof(blocklights));
			}
			else if (currentmodel->engineflags & MDLF_RGBLIGHTING)	//rgb
			{				
				for (maps = 0 ; maps < MAXLIGHTMAPS && surf->styles[maps] != 255 ;
					 maps++)
				{
					scale = d_lightstylevalue[surf->styles[maps]];
					surf->cached_light[maps] = scale;	// 8.8 fraction
					surf->cached_colour[maps] = cl_lightstyle[surf->styles[maps]].colour;


					if (cl_lightstyle[surf->styles[maps]].colour == 7)	//hopefully a faster alternative.
					{
						for (i=0 ; i<size ; i++)
						{
							blocklights[i]		+= lightmap[i*3  ] * scale;
							greenblklights[i]	+= lightmap[i*3+1] * scale;
							blueblklights[i]	+= lightmap[i*3+2] * scale;
						}
					}
					else
					{
						if (cl_lightstyle[surf->styles[maps]].colour & 1)
							for (i=0 ; i<size ; i++)
								blocklights[i]		+= lightmap[i*3  ] * scale;
						if (cl_lightstyle[surf->styles[maps]].colour & 2)
							for (i=0 ; i<size ; i++)
								greenblklights[i]	+= lightmap[i*3+1] * scale;
						if (cl_lightstyle[surf->styles[maps]].colour & 4)
							for (i=0 ; i<size ; i++)
								blueblklights[i]	+= lightmap[i*3+2] * scale;
					}
					lightmap += size*3;	// skip to next lightmap
				}
			}
			else
				for (maps = 0 ; maps < MAXLIGHTMAPS && surf->styles[maps] != 255 ;
					 maps++)
				{
					scale = d_lightstylevalue[surf->styles[maps]];
					surf->cached_light[maps] = scale;	// 8.8 fraction
					surf->cached_colour[maps] = cl_lightstyle[surf->styles[maps]].colour;

					if (cl_lightstyle[surf->styles[maps]].colour == 7)	//hopefully a faster alternative.
					{
						for (i=0 ; i<size ; i++)
						{
							blocklights[i]		+= lightmap[i] * scale;
							greenblklights[i]	+= lightmap[i] * scale;
							blueblklights[i]	+= lightmap[i] * scale;
						}
					}
					else
					{
						if (cl_lightstyle[surf->styles[maps]].colour & 1)
							for (i=0 ; i<size ; i++)
								blocklights[i] += lightmap[i] * scale;
						if (cl_lightstyle[surf->styles[maps]].colour & 2)
							for (i=0 ; i<size ; i++)
								greenblklights[i] += lightmap[i] * scale;
						if (cl_lightstyle[surf->styles[maps]].colour & 4)
							for (i=0 ; i<size ; i++)
								blueblklights[i] += lightmap[i] * scale;
					}
					lightmap += size;	// skip to next lightmap
				}
		}

// add all the dynamic lights
//		if (surf->dlightframe == r_framecount)
//			GLR_AddDynamicLightsColours (surf);
	}
	else
	{
	// set to full bright if no light data
		if (r_fullbright.value || !currentmodel->lightdata)
		{
			for (i=0 ; i<size ; i++)
				blocklights[i] = 255*256;
			goto store;
		}

	// clear to no light
		for (i=0 ; i<size ; i++)
			blocklights[i] = 0;

	// add all the lightmaps
		if (lightmap)
		{
			if (currentmodel->engineflags & MDLF_RGBLIGHTING)	//rgb
				for (maps = 0 ; maps < MAXLIGHTMAPS && surf->styles[maps] != 255 ;
					 maps++)
				{
					scale = d_lightstylevalue[surf->styles[maps]]/3;
					surf->cached_light[maps] = scale;	// 8.8 fraction
					surf->cached_colour[maps] = cl_lightstyle[surf->styles[maps]].colour;
					for (i=0 ; i<size ; i++)
						blocklights[i] += (lightmap[i*3]+lightmap[i*3+1]+lightmap[i*3+2]) * scale;
					lightmap += size*3;	// skip to next lightmap
				}

			else
				for (maps = 0 ; maps < MAXLIGHTMAPS && surf->styles[maps] != 255 ;
					 maps++)
				{
					scale = d_lightstylevalue[surf->styles[maps]];
					surf->cached_light[maps] = scale;	// 8.8 fraction
					surf->cached_colour[maps] = cl_lightstyle[surf->styles[maps]].colour;
					for (i=0 ; i<size ; i++)
						blocklights[i] += lightmap[i] * scale;
					lightmap += size;	// skip to next lightmap
				}
		}

	// add all the dynamic lights
//		if (surf->dlightframe == r_framecount)
//			GLR_AddDynamicLights (surf);
	}

// bound, invert, and shift
store:
#define INVERTLIGHTMAPS
#ifdef INVERTLIGHTMAPS
	switch (lightmap_bytes)
	{
	case 4:
		stride -= (smax<<2);
		bl = blocklights;
		blg = greenblklights;
		blb = blueblklights;

//		if (!r_stains.value)
			isstained = false;
//		else
//			isstained = surf->stained;

/*		if (!gl_lightcomponantreduction.value)
		{
			for (i=0 ; i<tmax ; i++, dest += stride)
			{
				for (j=0 ; j<smax ; j++)
				{
					t = *bl++;
					t >>= 7;
					if (t > 255)
						dest[0] = 0;
					else if (t < 0)
						dest[0] = 256;
					else
						dest[0] = (255-t);				

					t = *blg++;
					t >>= 7;
					if (t > 255)
						dest[1] = 0;
					else if (t < 0)
						dest[1] = 256;
					else
						dest[1] = (255-t);

					t = *blb++;
					t >>= 7;
					if (t > 255)
						dest[2] = 0;
					else if (t < 0)
						dest[2] = 256;
					else
						dest[2] = (255-t);

					dest[3] = 0;//(dest[0]+dest[1]+dest[2])/3;
					dest += 4;
				}
			}
		}
		else
*/		{
		stmap *stain;		
			for (i=0 ; i<tmax ; i++, dest += stride)
			{
				stain = stainsrc + i*LMBLOCK_WIDTH*3;
				for (j=0 ; j<smax ; j++)
				{
					r = *bl++;
					g = *blg++;
					b = *blb++;

					r >>= shift;
					g >>= shift;
					b >>= shift;	

					if (isstained)	// merge in stain
					{
						r = (127+r*(*stain++)) >> 8;
						g = (127+g*(*stain++)) >> 8;
						b = (127+b*(*stain++)) >> 8;
					}

					cr = 0;
					cg = 0;
					cb = 0;

					if (r > 255)	//ak too much red
					{
						cr -= (255-r)/2;
						cg += (255-r)/4;	//reduce it, and indicate to drop the others too.
						cb += (255-r)/4;
						r = 255;
					}
//					else if (r < 0)					
//						r = 0;				
					
					if (g > 255)
					{					
						cr += (255-g)/4;
						cg -= (255-g)/2;
						cb += (255-g)/4;
						g = 255;
					}
//					else if (g < 0)				
//						g = 0;					

					if (b > 255)
					{
						cr += (255-b)/4;
						cg += (255-b)/4;
						cb -= (255-b)/2;
						b = 255;
					}
//					else if (b < 0)
//						b = 0;
				/*
					if ((r+cr) > 255)
						dest[2] = 0;	//inverse lighting
					else if ((r+cr) < 0)
						dest[2] = 255;
					else
						dest[2] = 255-(r+cr);

					if ((g+cg) > 255)
						dest[1] = 0;
					else if ((g+cg) < 0)
						dest[1] = 255;
					else
						dest[1] = 255-(g+cg);

					if ((b+cb) > 255)
						dest[0] = 0;
					else if ((b+cb) < 0)
						dest[0] = 255;
					else
						dest[0] = 255-(b+cb);
/*/
					if ((r+cr) > 255)
						dest[2] = 255;	//non-inverse lighting
					else if ((r+cr) < 0)
						dest[2] = 0;
					else
						dest[2] = (r+cr);

					if ((g+cg) > 255)
						dest[1] = 255;
					else if ((g+cg) < 0)
						dest[1] = 0;
					else
						dest[1] = (g+cg);

					if ((b+cb) > 255)
						dest[0] = 255;
					else if ((b+cb) < 0)
						dest[0] = 0;
					else
						dest[0] = (b+cb);
//*/



					dest[3] = (dest[0]+dest[1]+dest[2])/3;	//alpha?!?!
					dest += 4;					
				}
			}
		}		
		break;

	case 3:
		stride -= smax*3;
		bl = blocklights;
		blg = greenblklights;
		blb = blueblklights;

//		if (!r_stains.value)
			isstained = false;
//		else
//			isstained = surf->stained;

/*		if (!gl_lightcomponantreduction.value)
		{
			for (i=0 ; i<tmax ; i++, dest += stride)
			{
				for (j=0 ; j<smax ; j++)
				{
					t = *bl++;
					t >>= 7;
					if (t > 255)
						dest[0] = 0;
					else if (t < 0)
						dest[0] = 256;
					else
						dest[0] = (255-t);				

					t = *blg++;
					t >>= 7;
					if (t > 255)
						dest[1] = 0;
					else if (t < 0)
						dest[1] = 256;
					else
						dest[1] = (255-t);

					t = *blb++;
					t >>= 7;
					if (t > 255)
						dest[2] = 0;
					else if (t < 0)
						dest[2] = 256;
					else
						dest[2] = (255-t);

					dest += 3;
				}
			}
		}
		else
*/		{
		stmap *stain;		
			for (i=0 ; i<tmax ; i++, dest += stride)
			{
				stain = stainsrc + i*LMBLOCK_WIDTH*3;
				for (j=0 ; j<smax ; j++)
				{
					r = *bl++;
					g = *blg++;
					b = *blb++;

					r >>= shift;
					g >>= shift;
					b >>= shift;	
					
					if (isstained)	// merge in stain
					{
						r = (127+r*(*stain++)) >> 8;
						g = (127+g*(*stain++)) >> 8;
						b = (127+b*(*stain++)) >> 8;
					}

					cr = 0;
					cg = 0;
					cb = 0;

					if (r > 255)	//ak too much red
					{
						cr -= (255-r)/2;
						cg += (255-r)/4;	//reduce it, and indicate to drop the others too.
						cb += (255-r)/4;
						r = 255;
					}
//					else if (r < 0)					
//						r = 0;				
					
					if (g > 255)
					{					
						cr += (255-g)/4;
						cg -= (255-g)/2;
						cb += (255-g)/4;
						g = 255;
					}
//					else if (g < 0)				
//						g = 0;					

					if (b > 255)
					{
						cr += (255-b)/4;
						cg += (255-b)/4;
						cb -= (255-b)/2;
						b = 255;
					}
//					else if (b < 0)
//						b = 0;
				/*
					if ((r+cr) > 255)
						dest[2] = 0;	//inverse lighting
					else if ((r+cr) < 0)
						dest[2] = 255;
					else
						dest[2] = 255-(r+cr);

					if ((g+cg) > 255)
						dest[1] = 0;
					else if ((g+cg) < 0)
						dest[1] = 255;
					else
						dest[1] = 255-(g+cg);

					if ((b+cb) > 255)
						dest[0] = 0;
					else if ((b+cb) < 0)
						dest[0] = 255;
					else
						dest[0] = 255-(b+cb);
/*/
					if ((r+cr) > 255)
						dest[2] = 255;	//non-inverse lighting
					else if ((r+cr) < 0)
						dest[2] = 0;
					else
						dest[2] = (r+cr);

					if ((g+cg) > 255)
						dest[1] = 255;
					else if ((g+cg) < 0)
						dest[1] = 0;
					else
						dest[1] = (g+cg);

					if ((b+cb) > 255)
						dest[0] = 255;
					else if ((b+cb) < 0)
						dest[0] = 0;
					else
						dest[0] = (b+cb);
// */
					dest += 3;	
				}
			}
		}		
		break;
	default:
		Sys_Error ("Bad lightmap format");
	}
#else
	switch (lightmap_bytes)
	{
	case 4:
		stride -= (smax<<2);
		bl = blocklights;
		blg = greenblklights;
		blb = blueblklights;

//		if (!r_stains.value)
			isstained = false;
//		else
//			isstained = surf->stained;

/*		if (!gl_lightcomponantreduction.value)
		{
			for (i=0 ; i<tmax ; i++, dest += stride)
			{
				for (j=0 ; j<smax ; j++)
				{
					t = *bl++;
					t >>= 7;
					if (t > 255)
						dest[0] = 0;
					else if (t < 0)
						dest[0] = 256;
					else
						dest[0] = (255-t);				

					t = *blg++;
					t >>= 7;
					if (t > 255)
						dest[1] = 0;
					else if (t < 0)
						dest[1] = 256;
					else
						dest[1] = (255-t);

					t = *blb++;
					t >>= 7;
					if (t > 255)
						dest[2] = 0;
					else if (t < 0)
						dest[2] = 256;
					else
						dest[2] = (255-t);

					dest[3] = 0;//(dest[0]+dest[1]+dest[2])/3;
					dest += 4;
				}
			}
		}
		else
*/		{
		stmap *stain;		
			for (i=0 ; i<tmax ; i++, dest += stride)
			{
				stain = stainsrc + i*LMBLOCK_WIDTH*3;
				for (j=0 ; j<smax ; j++)
				{
					r = *bl++;
					g = *blg++;
					b = *blb++;

					r >>= shift;
					g >>= shift;
					b >>= shift;	

					if (isstained)	// merge in stain
					{
						r = (127+r*(*stain++)) >> 8;
						g = (127+g*(*stain++)) >> 8;
						b = (127+b*(*stain++)) >> 8;
					}

					cr = 0;
					cg = 0;
					cb = 0;

					if (r > 255)	//ak too much red
					{
						cr -= (255-r)/2;
						cg += (255-r)/4;	//reduce it, and indicate to drop the others too.
						cb += (255-r)/4;
						r = 255;
					}
//					else if (r < 0)					
//						r = 0;				
					
					if (g > 255)
					{					
						cr += (255-g)/4;
						cg -= (255-g)/2;
						cb += (255-g)/4;
						g = 255;
					}
//					else if (g < 0)				
//						g = 0;					

					if (b > 255)
					{
						cr += (255-b)/4;
						cg += (255-b)/4;
						cb -= (255-b)/2;
						b = 255;
					}
//					else if (b < 0)
//						b = 0;
				//*
					if ((r+cr) > 255)
						dest[2] = 0;	//inverse lighting
					else if ((r+cr) < 0)
						dest[2] = 255;
					else
						dest[2] = 255-(r+cr);

					if ((g+cg) > 255)
						dest[1] = 0;
					else if ((g+cg) < 0)
						dest[1] = 255;
					else
						dest[1] = 255-(g+cg);

					if ((b+cb) > 255)
						dest[0] = 0;
					else if ((b+cb) < 0)
						dest[0] = 255;
					else
						dest[0] = 255-(b+cb);
/*/
					if ((r+cr) > 255)
						dest[0] = 255;	//non-inverse lighting
					else if ((r+cr) < 0)
						dest[0] = 0;
					else
						dest[0] = (r+cr);

					if ((g+cg) > 255)
						dest[1] = 255;
					else if ((g+cg) < 0)
						dest[1] = 0;
					else
						dest[1] = (g+cg);

					if ((b+cb) > 255)
						dest[2] = 255;
					else if ((b+cb) < 0)
						dest[2] = 0;
					else
						dest[2] = (b+cb);
*/



					dest[3] = (dest[0]+dest[1]+dest[2])/3;	//alpha?!?!					
					dest += 4;					
				}
			}
		}		
		break;

	case 3:
		stride -= smax*3;
		bl = blocklights;
		blg = greenblklights;
		blb = blueblklights;

//		if (!r_stains.value)
			isstained = false;
//		else
//			isstained = surf->stained;

/*		if (!gl_lightcomponantreduction.value)
		{
			for (i=0 ; i<tmax ; i++, dest += stride)
			{
				for (j=0 ; j<smax ; j++)
				{
					t = *bl++;
					t >>= 7;
					if (t > 255)
						dest[0] = 255;
					else if (t < 0)
						dest[0] = 0;
					else
						dest[0] = t;				

					t = *blg++;
					t >>= 7;
					if (t > 255)
						dest[1] = 255;
					else if (t < 0)
						dest[1] = 0;
					else
						dest[1] = t;

					t = *blb++;
					t >>= 7;
					if (t > 255)
						dest[2] = 255;
					else if (t < 0)
						dest[2] = 0;
					else
						dest[2] = t;

					dest += 3;
				}
			}
		}
		else
*/		{
		stmap *stain;		
			for (i=0 ; i<tmax ; i++, dest += stride)
			{
				stain = stainsrc + i*LMBLOCK_WIDTH*3;
				for (j=0 ; j<smax ; j++)
				{
					r = *bl++;
					g = *blg++;
					b = *blb++;

					r >>= shift;
					g >>= shift;
					b >>= shift;	

					if (isstained)	// merge in stain
					{
						r = (127+r*(*stain++)) >> 8;
						g = (127+g*(*stain++)) >> 8;
						b = (127+b*(*stain++)) >> 8;
					}

					cr = 0;
					cg = 0;
					cb = 0;

					if (r > 255)	//ak too much red
					{
						cr -= (255-r)/2;
						cg += (255-r)/4;	//reduce it, and indicate to drop the others too.
						cb += (255-r)/4;
						r = 255;
					}
//					else if (r < 0)					
//						r = 0;				
					
					if (g > 255)
					{					
						cr += (255-g)/4;
						cg -= (255-g)/2;
						cb += (255-g)/4;
						g = 255;
					}
//					else if (g < 0)				
//						g = 0;					

					if (b > 255)
					{
						cr += (255-b)/4;
						cg += (255-b)/4;
						cb -= (255-b)/2;
						b = 255;
					}
//					else if (b < 0)
//						b = 0;
				//*
					if ((r+cr) > 255)
						dest[2] = 255;	//inverse lighting
					else if ((r+cr) < 0)
						dest[2] = 0;
					else
						dest[2] = (r+cr);

					if ((g+cg) > 255)
						dest[1] = 255;
					else if ((g+cg) < 0)
						dest[1] = 0;
					else
						dest[1] = (g+cg);

					if ((b+cb) > 255)
						dest[0] = 255;
					else if ((b+cb) < 0)
						dest[0] = 0;
					else
						dest[0] = (b+cb);
/*/
					if ((r+cr) > 255)
						dest[0] = 255;	//non-inverse lighting
					else if ((r+cr) < 0)
						dest[0] = 0;
					else
						dest[0] = (r+cr);

					if ((g+cg) > 255)
						dest[1] = 255;
					else if ((g+cg) < 0)
						dest[1] = 0;
					else
						dest[1] = (g+cg);

					if ((b+cb) > 255)
						dest[2] = 255;
					else if ((b+cb) < 0)
						dest[2] = 0;
					else
						dest[2] = (b+cb);
// */
					dest += 3;	
				}
			}
		}		
		break;
	default:
		Sys_Error ("Bad lightmap format");
	}
#endif
}



int D3D9_FillBlock (int texnum, int w, int h, int x, int y)
{
	int		i, l;
	while (texnum >= numlightmaps)	//allocate 4 more lightmap slots. not much memory usage, but we don't want any caps here.
	{
		lightmap = BZ_Realloc(lightmap, sizeof(*lightmap)*(numlightmaps+4));
		lightmap_d3d9textures = BZ_Realloc(lightmap_d3d9textures, sizeof(*lightmap_d3d9textures)*(numlightmaps+4));
//		lightmap_textures[numlightmaps+0] = texture_extension_number++;
//		lightmap_textures[numlightmaps+1] = texture_extension_number++;
//		lightmap_textures[numlightmaps+2] = texture_extension_number++;
//		lightmap_textures[numlightmaps+3] = texture_extension_number++;

		deluxmap_d3d9textures = BZ_Realloc(deluxmap_d3d9textures, sizeof(*deluxmap_d3d9textures)*(numlightmaps+4));
//		deluxmap_textures[numlightmaps+0] = texture_extension_number++;
//		deluxmap_textures[numlightmaps+1] = texture_extension_number++;
//		deluxmap_textures[numlightmaps+2] = texture_extension_number++;
//		deluxmap_textures[numlightmaps+3] = texture_extension_number++;
		numlightmaps+=4;
	}
	for (i = texnum; i >= 0; i--)
	{
		if (!lightmap[i])
		{
			lightmap[i] = BZ_Malloc(sizeof(*lightmap[i]));
			for (l=0 ; l<LMBLOCK_HEIGHT ; l++)
			{
				lightmap[i]->allocated[l] = LMBLOCK_HEIGHT;
			}

			//maybe someone screwed with my lightmap...
			memset(lightmap[i]->lightmaps, 255, LMBLOCK_HEIGHT*LMBLOCK_HEIGHT*3);
//			if (cl.worldmodel->lightdata)
//				memcpy(lightmap[i]->lightmaps, cl.worldmodel->lightdata+3*LMBLOCK_HEIGHT*LMBLOCK_HEIGHT*i, LMBLOCK_HEIGHT*LMBLOCK_HEIGHT*3);

		}
		else
			break;
	}
	return texnum;
}

int D3D9_AllocBlock (int w, int h, int *x, int *y)
{
	int		i, j;
	int		best, best2;
	int		texnum;

	for (texnum=0 ;  ; texnum++)
	{
		if (texnum == numlightmaps)	//allocate 4 more lightmap slots. not much memory usage, but we don't want any caps here.
		{
			lightmap = BZ_Realloc(lightmap, sizeof(*lightmap)*(numlightmaps+4));
			lightmap_d3d9textures = BZ_Realloc(lightmap_d3d9textures, sizeof(*lightmap_d3d9textures)*(numlightmaps+4));
//			lightmap_textures[numlightmaps+0] = texture_extension_number++;
//			lightmap_textures[numlightmaps+1] = texture_extension_number++;
//			lightmap_textures[numlightmaps+2] = texture_extension_number++;
//			lightmap_textures[numlightmaps+3] = texture_extension_number++;

			deluxmap_d3d9textures = BZ_Realloc(deluxmap_d3d9textures, sizeof(*deluxmap_d3d9textures)*(numlightmaps+4));
//			deluxmap_textures[numlightmaps+0] = texture_extension_number++;
//			deluxmap_textures[numlightmaps+1] = texture_extension_number++;
//			deluxmap_textures[numlightmaps+2] = texture_extension_number++;
//			deluxmap_textures[numlightmaps+3] = texture_extension_number++;
			numlightmaps+=4;
		}
		if (!lightmap[texnum])
		{
			lightmap[texnum] = Z_Malloc(sizeof(*lightmap[texnum]));
			// reset stainmap since it now starts at 255
			memset(lightmap[texnum]->stainmaps, 255, sizeof(lightmap[texnum]->stainmaps));
		}


		best = LMBLOCK_HEIGHT;

		for (i=0 ; i<LMBLOCK_WIDTH-w ; i++)
		{
			best2 = 0;

			for (j=0 ; j<w ; j++)
			{
				if (lightmap[texnum]->allocated[i+j] >= best)
					break;
				if (lightmap[texnum]->allocated[i+j] > best2)
					best2 = lightmap[texnum]->allocated[i+j];
			}
			if (j == w)
			{	// this is a valid spot
				*x = i;
				*y = best = best2;
			}
		}

		if (best + h > LMBLOCK_HEIGHT)
			continue;

		for (i=0 ; i<w ; i++)
			lightmap[texnum]->allocated[*x + i] = best + h;

		return texnum;
	}

	Sys_Error ("AllocBlock: full");
	return 0;
}


void D3D9_CreateSurfaceLightmap (msurface_t *surf, int shift)
{
	int		smax, tmax;
	qbyte	*base, *luxbase; stmap *stainbase;

	if (surf->flags & (SURF_DRAWSKY|SURF_DRAWTURB))
		surf->lightmaptexturenum = -1;
	if (surf->texinfo->flags & TEX_SPECIAL)
		surf->lightmaptexturenum = -1;
	if (surf->lightmaptexturenum<0)
		return;

	smax = (surf->extents[0]>>4)+1;
	tmax = (surf->extents[1]>>4)+1;

	if (smax > LMBLOCK_WIDTH || tmax > LMBLOCK_HEIGHT || smax < 0 || tmax < 0)
	{	//whoa, buggy.
		surf->lightmaptexturenum = -1;
		return;
	}

	if (currentmodel->fromgame == fg_quake3)
		D3D9_FillBlock(surf->lightmaptexturenum, smax, tmax, surf->light_s, surf->light_t);
	else
		surf->lightmaptexturenum = D3D9_AllocBlock (smax, tmax, &surf->light_s, &surf->light_t);
	base = lightmap[surf->lightmaptexturenum]->lightmaps;
	base += (surf->light_t * LMBLOCK_WIDTH + surf->light_s) * lightmap_bytes;

	luxbase = lightmap[surf->lightmaptexturenum]->deluxmaps;
	luxbase += (surf->light_t * LMBLOCK_WIDTH + surf->light_s) * lightmap_bytes;

	stainbase = lightmap[surf->lightmaptexturenum]->stainmaps;
	stainbase += (surf->light_t * LMBLOCK_WIDTH + surf->light_s) * 3;
	
	D3D9R_BuildLightMap (surf, base, luxbase, stainbase, shift);
}


void D3D9R_RenderDynamicLightmaps (msurface_t *fa, int shift)
{
	qbyte		*base, *luxbase;
	stmap *stainbase;
	int			maps;
	glRect_t    *theRect;
	int smax, tmax;

	if (!fa->mesh)
		return;

//	c_brush_polys++;

	if (fa->lightmaptexturenum<0)
		return;

	if (fa->flags & ( SURF_DRAWSKY | SURF_DRAWTURB) )
		return;

	if (fa->texinfo->flags & (SURF_SKY|SURF_TRANS33|SURF_TRANS66|SURF_WARP))
		return;

	if (fa->texinfo->flags & (TEX_SPECIAL))
	{
		if (cl.worldmodel->fromgame == fg_halflife)
			return;	//some textures do this.
	}
	
//	fa->polys->chain = lightmap[fa->lightmaptexturenum]->polys;
//	lightmap[fa->lightmaptexturenum]->polys = fa->polys;

	// check for lightmap modification
//	if (cl.worldmodel->fromgame != fg_quake3)	//no lightstyles on q3 maps
	{
		for (maps = 0 ; maps < MAXLIGHTMAPS && fa->styles[maps] != 255 ;
			 maps++)
			if (d_lightstylevalue[fa->styles[maps]] != fa->cached_light[maps]
	#ifdef PEXT_LIGHTSTYLECOL
				|| cl_lightstyle[fa->styles[maps]].colour != fa->cached_colour[maps]
	#endif
				)
			{
				goto dynamic;
			}
	}

	if (fa->dlightframe == r_framecount	// dynamic this frame
		|| fa->cached_dlight)			// dynamic previously
	{
		RSpeedLocals();
dynamic:
		RSpeedRemark();

		lightmap[fa->lightmaptexturenum]->modified = true;

		smax = (fa->extents[0]>>4)+1;
		tmax = (fa->extents[1]>>4)+1;

		theRect = &lightmap[fa->lightmaptexturenum]->rectchange;
		if (fa->light_t < theRect->t) {
			if (theRect->h)
				theRect->h += theRect->t - fa->light_t;
			theRect->t = fa->light_t;
		}
		if (fa->light_s < theRect->l) {
			if (theRect->w)
				theRect->w += theRect->l - fa->light_s;
			theRect->l = fa->light_s;
		}
		if ((theRect->w + theRect->l) < (fa->light_s + smax))
			theRect->w = (fa->light_s-theRect->l)+smax;
		if ((theRect->h + theRect->t) < (fa->light_t + tmax))
			theRect->h = (fa->light_t-theRect->t)+tmax;

		lightmap[fa->lightmaptexturenum]->deluxmodified = true;
		theRect = &lightmap[fa->lightmaptexturenum]->deluxrectchange;
		if (fa->light_t < theRect->t) {
			if (theRect->h)
				theRect->h += theRect->t - fa->light_t;
			theRect->t = fa->light_t;
		}
		if (fa->light_s < theRect->l) {
			if (theRect->w)
				theRect->w += theRect->l - fa->light_s;
			theRect->l = fa->light_s;
		}

		if ((theRect->w + theRect->l) < (fa->light_s + smax))
			theRect->w = (fa->light_s-theRect->l)+smax;
		if ((theRect->h + theRect->t) < (fa->light_t + tmax))
			theRect->h = (fa->light_t-theRect->t)+tmax;


		base = lightmap[fa->lightmaptexturenum]->lightmaps;
		base += fa->light_t * LMBLOCK_WIDTH * lightmap_bytes + fa->light_s * lightmap_bytes;
		luxbase = lightmap[fa->lightmaptexturenum]->deluxmaps;
		luxbase += fa->light_t * LMBLOCK_WIDTH * lightmap_bytes + fa->light_s * lightmap_bytes;
		stainbase = lightmap[fa->lightmaptexturenum]->stainmaps;
		stainbase += (fa->light_t * LMBLOCK_WIDTH + fa->light_s) * 3;
		D3D9R_BuildLightMap (fa, base, luxbase, stainbase, shift);

		RSpeedEnd(RSPEED_DYNAMIC);
	}
}



LPDIRECT3DTEXTURE9 D3D9_NewLightmap(void)
{
	LPDIRECT3DTEXTURE9 newsurf;
	IDirect3DDevice9_CreateTexture(pD3DDev9, LMBLOCK_WIDTH, LMBLOCK_WIDTH, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &newsurf, NULL);

	if (!newsurf)
		return NULL;

	return newsurf;
}

void D3D9_BuildLightmaps (void)
{
	D3DLOCKED_RECT desc;

	int		i, j;
	model_t	*m;
	int shift;

	r_framecount = 1;		// no dlightcache
/*
	for (i = 0; i < numlightmaps; i++)
	{
		if (!lightmap[i])
			break;
		BZ_Free(lightmap[i]);
		lightmap[i] = NULL;
	}

	if (cl.worldmodel->fromgame == fg_doom)
		return;	//no lightmaps.

	if ((cl.worldmodel->engineflags & MDLF_RGBLIGHTING) || cl.worldmodel->deluxdata || r_loadlits.value)
		gl_lightmap_format = GL_RGB;
	else
		gl_lightmap_format = GL_LUMINANCE;


	if (cl.worldmodel->fromgame == fg_quake3 && gl_lightmap_format != GL_RGB && gl_lightmap_format != GL_RGBA)
		gl_lightmap_format = GL_RGB;


	switch (gl_lightmap_format)
	{
	case GL_RGBA:
		lightmap_bytes = 4;
		break;
	case GL_RGB:
		lightmap_bytes = 3;
		break;
	case GL_LUMINANCE:
	case GL_INTENSITY:
	case GL_ALPHA:
		lightmap_bytes = 1;
		break;
	}
*/
	for (j=1 ; j<MAX_MODELS ; j++)
	{
		m = cl.model_precache[j];
		if (!m)
			break;
		if (m->name[0] == '*')
			continue;

		r_pcurrentvertbase = m->vertexes;
		currentmodel = m;
		shift = 0;//GLR_LightmapShift(currentmodel);

		for (i=0 ; i<m->numsurfaces ; i++)
		{
			D3D9_CreateSurfaceLightmap (m->surfaces + i, shift);
			P_EmitSkyEffectTris(m, &m->surfaces[i]);
			if (m->surfaces[i].mesh)	//there are some surfaces that have a display list already (the subdivided ones)
				continue;
			D3D9_BuildSurfaceDisplayList (m->surfaces + i);
		}
	}

	//
	// upload all lightmaps that were filled
	//
	for (i=0 ; i<numlightmaps ; i++)
	{
		if (!lightmap[i])
			break;		// no more used
		lightmap[i]->modified = false;
		lightmap[i]->rectchange.l = LMBLOCK_WIDTH;
		lightmap[i]->rectchange.t = LMBLOCK_HEIGHT;
		lightmap[i]->rectchange.w = 0;
		lightmap[i]->rectchange.h = 0;

		if (!lightmap_d3d9textures[i])
		{
			lightmap_d3d9textures[i] = D3D9_NewLightmap();

			if (!lightmap_d3d9textures[i])
			{
				Con_Printf("Couldn't create new lightmap\n");
				return;
			}
		}

		IDirect3DTexture9_LockRect(lightmap_d3d9textures[i], 0, &desc, NULL, DDLOCK_NOSYSLOCK|DDLOCK_WAIT|DDLOCK_WRITEONLY|DDLOCK_DISCARDCONTENTS);
		memcpy(desc.pBits, lightmap[i]->lightmaps, sizeof(lightmap[i]->lightmaps));
	/*	memset(desc.lpSurface, 0, sizeof(lightmap[i]->lightmaps));
		{
			int i;
			unsigned char *c;
			c = desc.lpSurface;
			for (i = 0; i < sizeof(lightmap[i]->lightmaps); i++)
				*c++ = rand();
		}*/
		IDirect3DTexture9_UnlockRect(lightmap_d3d9textures[i], 0);

		if (deluxmap_d3d9textures[i])
		{
			IDirect3DTexture9_LockRect(deluxmap_d3d9textures[i], 0, &desc, NULL, DDLOCK_NOSYSLOCK|DDLOCK_WAIT|DDLOCK_WRITEONLY|DDLOCK_DISCARDCONTENTS);
			memcpy(desc.pBits, lightmap[i]->lightmaps, sizeof(lightmap[i]->deluxmaps));
			IDirect3DTexture9_UnlockRect(deluxmap_d3d9textures[i], 0);
		}
	}

}






void D3D9_DrawSkyMesh(int pass, int texture, void *verts, int numverts, void *indicies, int numelements)
{
	if (pass == 0)
	{
		IDirect3DDevice9_SetRenderState(pD3DDev9, D3DRS_ALPHATESTENABLE, FALSE );

		IDirect3DDevice9_SetTextureStageState(pD3DDev9, 0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
		IDirect3DDevice9_SetTextureStageState(pD3DDev9, 0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);

		IDirect3DDevice9_SetTextureStageState(pD3DDev9, 0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
		IDirect3DDevice9_SetTextureStageState(pD3DDev9, 0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);

		IDirect3DDevice9_SetRenderState(pD3DDev9, D3DRS_SRCBLEND,  D3DBLEND_SRCALPHA);
		IDirect3DDevice9_SetRenderState(pD3DDev9, D3DRS_DESTBLEND,  D3DBLEND_INVSRCALPHA);

		IDirect3DDevice9_SetTexture(pD3DDev9, 0, (LPDIRECT3DBASETEXTURE9)texture);
		IDirect3DDevice9_SetFVF(pD3DDev9, D3DFVF_XYZ|D3DFVF_TEX1);
		IDirect3DDevice9_DrawIndexedPrimitiveUP(pD3DDev9, D3DPT_TRIANGLELIST, 0, numverts, numelements, indicies, D3DFMT_QINDEX, verts, 20);
	}

	if (pass == 1)
	{
		IDirect3DDevice9_SetRenderState(pD3DDev9, D3DRS_ALPHABLENDENABLE, TRUE);

		IDirect3DDevice9_SetTextureStageState(pD3DDev9, 0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
		IDirect3DDevice9_SetTextureStageState(pD3DDev9, 0, D3DTSS_COLORARG2, D3DTA_CURRENT);
		IDirect3DDevice9_SetTextureStageState(pD3DDev9, 0, D3DTSS_COLOROP, D3DTOP_MODULATE);

		IDirect3DDevice9_SetTextureStageState(pD3DDev9, 0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
		IDirect3DDevice9_SetTextureStageState(pD3DDev9, 0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);

		IDirect3DDevice9_SetTexture(pD3DDev9, 0, (LPDIRECT3DBASETEXTURE9)texture);
		IDirect3DDevice9_SetFVF(pD3DDev9, D3DFVF_XYZ|D3DFVF_TEX1);
		IDirect3DDevice9_DrawIndexedPrimitiveUP(pD3DDev9, D3DPT_TRIANGLELIST, 0, numverts, numelements, indicies, D3DFMT_QINDEX, verts, 20);




		IDirect3DDevice9_SetRenderState(pD3DDev9, D3DRS_ALPHABLENDENABLE, FALSE);
	}
}

#endif
