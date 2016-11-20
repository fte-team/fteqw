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
// r_light.c

#include "quakedef.h"
#ifndef SERVERONLY
#include "glquake.h"
#include "shader.h"

extern cvar_t r_shadow_realtime_world, r_shadow_realtime_world_lightmaps;
extern cvar_t r_hdr_irisadaptation, r_hdr_irisadaptation_multiplier, r_hdr_irisadaptation_minvalue, r_hdr_irisadaptation_maxvalue, r_hdr_irisadaptation_fade_down, r_hdr_irisadaptation_fade_up;


int	r_dlightframecount;
int		d_lightstylevalue[256];	// 8.8 fraction of base light value

void R_UpdateLightStyle(unsigned int style, const char *stylestring, float r, float g, float b)
{
	if (style >= MAX_LIGHTSTYLES)
		return;

	if (!stylestring)
		stylestring = "";

	Q_strncpyz (cl_lightstyle[style].map,  stylestring, sizeof(cl_lightstyle[style].map));
	cl_lightstyle[style].length = Q_strlen(cl_lightstyle[style].map);
	if (!cl_lightstyle[style].length)
	{
		d_lightstylevalue[style] = 256;
		VectorSet(cl_lightstyle[style].colours, 1,1,1);
	}
	else
		VectorSet(cl_lightstyle[style].colours, r,g,b);
	cl_lightstyle[style].colourkey = (int)(cl_lightstyle[style].colours[0]*0x400) ^ (int)(cl_lightstyle[style].colours[1]*0x100000) ^ (int)(cl_lightstyle[style].colours[2]*0x40000000);
}

void Sh_CalcPointLight(vec3_t point, vec3_t light);
void R_UpdateHDR(vec3_t org)
{
	if (r_hdr_irisadaptation.ival && cl.worldmodel && !(r_refdef.flags & RDF_NOWORLDMODEL))
	{
		//fake and lame, but whatever.
		vec3_t ambient, diffuse, dir;
		float lev = 0;

#ifdef RTLIGHTS
		Sh_CalcPointLight(org, ambient);
		lev += VectorLength(ambient);


		if (!r_shadow_realtime_world.ival || r_shadow_realtime_world_lightmaps.value)
#endif
		{
			cl.worldmodel->funcs.LightPointValues(cl.worldmodel, org, ambient, diffuse, dir);
			lev += (VectorLength(ambient) + VectorLength(diffuse))/256;
		}

		lev += 0.001;	//no division by 0!
		lev = r_hdr_irisadaptation_multiplier.value / lev;
		lev = bound(r_hdr_irisadaptation_minvalue.value, lev, r_hdr_irisadaptation_maxvalue.value);
		if (lev > r_refdef.playerview->hdr_last + r_hdr_irisadaptation_fade_up.value*host_frametime)
			lev = r_refdef.playerview->hdr_last + r_hdr_irisadaptation_fade_up.value*host_frametime;
		else if (lev < r_refdef.playerview->hdr_last - r_hdr_irisadaptation_fade_down.value*host_frametime)
			lev = r_refdef.playerview->hdr_last - r_hdr_irisadaptation_fade_down.value*host_frametime;
		lev = bound(r_hdr_irisadaptation_minvalue.value, lev, r_hdr_irisadaptation_maxvalue.value);
		r_refdef.playerview->hdr_last = lev;
		r_refdef.hdr_value = lev;
	}
	else
		r_refdef.hdr_value = 1;
}

/*
==================
R_AnimateLight
==================
*/
void R_AnimateLight (void)
{
	int			i,j;
	float f;


	//if (r_lightstylescale.value > 2)
		//r_lightstylescale.value = 2;
	
//
// light animations
// 'm' is normal light, 'a' is no light, 'z' is double bright
	f = (cl.time*r_lightstylespeed.value);
	if (f < 0)
		f = 0;
	i = (int)f;
	f -= i;	//this can require updates at 1000 times a second.. Depends on your framerate of course

	for (j=0 ; j<MAX_LIGHTSTYLES ; j++)
	{
		int v1, v2, vd;
		if (!cl_lightstyle[j].length)
		{
			d_lightstylevalue[j] = ('m'-'a')*22 * r_lightstylescale.value;
			continue;
		}

		if (cl_lightstyle[j].map[0] == '=')
		{
			d_lightstylevalue[j] = atof(cl_lightstyle[j].map+1)*256*r_lightstylescale.value;
			continue;
		}

		v1 = i % cl_lightstyle[j].length;
		v1 = cl_lightstyle[j].map[v1] - 'a';

		v2 = (i+1) % cl_lightstyle[j].length;
		v2 = cl_lightstyle[j].map[v2] - 'a';

		vd = v1 - v2;
		if (!r_lightstylesmooth.ival || vd < -r_lightstylesmooth_limit.ival || vd > r_lightstylesmooth_limit.ival)
			d_lightstylevalue[j] = v1*22*r_lightstylescale.value;
		else
			d_lightstylevalue[j] = (v1*(1-f) + v2*(f))*22*r_lightstylescale.value;
	}
}

/*
=============================================================================

DYNAMIC LIGHTS BLEND RENDERING

=============================================================================
*/

void AddLightBlend (float r, float g, float b, float a2)
{
	float	a;
	float *sw_blend = r_refdef.playerview->screentint;

	r = bound(0, r, 1);
	g = bound(0, g, 1);
	b = bound(0, b, 1);

	sw_blend[3] = a = sw_blend[3] + a2*(1-sw_blend[3]);

	a2 = a2/a;

	sw_blend[0] = sw_blend[0]*(1-a2) + r*a2;
	sw_blend[1] = sw_blend[1]*(1-a2) + g*a2;
	sw_blend[2] = sw_blend[2]*(1-a2) + b*a2;
//Con_Printf("AddLightBlend(): %4.2f %4.2f %4.2f %4.6f\n", v_blend[0], v_blend[1], v_blend[2], v_blend[3]);
}

#define FLASHBLEND_VERTS 16
static float bubble_sintable[FLASHBLEND_VERTS+1], bubble_costable[FLASHBLEND_VERTS+1];

static void R_InitBubble(void)
{
	float a;
	int i;
	float *bub_sin, *bub_cos;

	bub_sin = bubble_sintable;
	bub_cos = bubble_costable;

	for (i=FLASHBLEND_VERTS ; i>=0 ; i--)
	{
		a = i/(float)FLASHBLEND_VERTS * M_PI*2;
		*bub_sin++ = sin(a);
		*bub_cos++ = cos(a);
	}
}

avec4_t flashblend_colours[FLASHBLEND_VERTS+1]; 
vecV_t flashblend_vcoords[FLASHBLEND_VERTS+1];
vec2_t flashblend_tccoords[FLASHBLEND_VERTS+1];
index_t flashblend_indexes[FLASHBLEND_VERTS*3];
index_t flashblend_fsindexes[6] = {0, 1, 2, 0, 2, 3};
mesh_t flashblend_mesh;
mesh_t flashblend_fsmesh;
shader_t *occluded_shader;
shader_t *flashblend_shader;
shader_t *lpplight_shader[LSHADER_MODES];

void R_GenerateFlashblendTexture(void)
{
	float dx, dy;
	int x, y, a;
	unsigned char pixels[32][32][4];
	for (y = 0;y < 32;y++)
	{
		dy = (y - 15.5f) * (1.0f / 16.0f);
		for (x = 0;x < 32;x++)
		{
			dx = (x - 15.5f) * (1.0f / 16.0f);
			a = (int)(((1.0f / (dx * dx + dy * dy + 0.2f)) - (1.0f / (1.0f + 0.2))) * 32.0f / (1.0f / (1.0f + 0.2)));
			a = bound(0, a, 255);
			pixels[y][x][0] = a;
			pixels[y][x][1] = a;
			pixels[y][x][2] = a;
			pixels[y][x][3] = 255;
		}
	}
	R_LoadReplacementTexture("***flashblend***", NULL, 0, pixels, 32, 32, TF_RGBA32);
}
void R_InitFlashblends(void)
{
	int i;
	R_InitBubble();
	for (i = 0; i < FLASHBLEND_VERTS; i++)
	{
		flashblend_indexes[i*3+0] = 0;
		if (i+1 == FLASHBLEND_VERTS)
			flashblend_indexes[i*3+1] = 1;
		else
			flashblend_indexes[i*3+1] = i+2;
		flashblend_indexes[i*3+2] = i+1;

		flashblend_tccoords[i+1][0] = 0.5 + bubble_sintable[i]*0.5;
		flashblend_tccoords[i+1][1] = 0.5 + bubble_costable[i]*0.5;
	}
	flashblend_tccoords[0][0] = 0.5;
	flashblend_tccoords[0][1] = 0.5;
	flashblend_mesh.numvertexes = FLASHBLEND_VERTS+1;
	flashblend_mesh.xyz_array = flashblend_vcoords;
	flashblend_mesh.st_array = flashblend_tccoords;
	flashblend_mesh.colors4f_array[0] = flashblend_colours;
	flashblend_mesh.indexes = flashblend_indexes;
	flashblend_mesh.numindexes = FLASHBLEND_VERTS*3;
	flashblend_mesh.istrifan = true;

	flashblend_fsmesh.numvertexes = 4;
	flashblend_fsmesh.xyz_array = flashblend_vcoords;
	flashblend_fsmesh.st_array = flashblend_tccoords;
	flashblend_fsmesh.colors4f_array[0] = flashblend_colours;
	flashblend_fsmesh.indexes = flashblend_fsindexes;
	flashblend_fsmesh.numindexes = 6;
	flashblend_fsmesh.istrifan = true;

	R_GenerateFlashblendTexture();

	flashblend_shader = R_RegisterShader("flashblend", SUF_NONE,
		"{\n"
			"program defaultadditivesprite\n"
			"{\n"
				"map ***flashblend***\n"	
				"blendfunc gl_one gl_one\n"
				"rgbgen vertex\n"
				"alphagen vertex\n"
				"nodepth\n"
			"}\n"
		"}\n"
		);
	occluded_shader = R_RegisterShader("flashblend_occlusiontest", SUF_NONE,
		"{\n"
			"program defaultadditivesprite\n"
			"{\n"
				"maskcolor\n"
				"maskalpha\n"
			"}\n"
		"}\n"
		);
	memset(lpplight_shader, 0, sizeof(lpplight_shader));
}

static qboolean R_BuildDlightMesh(dlight_t *light, float colscale, float radscale, int dtype)
{
	int		i, j;
//	float	a;
	vec3_t	v;
	float	rad;
	float	*bub_sin, *bub_cos;
	vec3_t colour;

	bub_sin = bubble_sintable;
	bub_cos = bubble_costable;
	rad = light->radius * radscale;

	VectorCopy(light->color, colour);

	if (light->fov)
	{
		float a = -DotProduct(light->axis[0], vpn);
		colour[0] *= a;
		colour[1] *= a;
		colour[2] *= a;
		rad *= a;
		rad *= 0.33;
	}
	if (light->style)
	{
		colscale *= d_lightstylevalue[light->style-1]/255.0f;
	}

	VectorSubtract (light->origin, r_origin, v);
	if (dtype != 1 && Length (v) < rad + r_refdef.mindist*2)
	{	// view is inside the dlight
		return false;
	}

	flashblend_colours[0][0] = colour[0]*colscale;
	flashblend_colours[0][1] = colour[1]*colscale;
	flashblend_colours[0][2] = colour[2]*colscale;
	flashblend_colours[0][3] = 1;

	VectorCopy(light->origin, flashblend_vcoords[0]);
	for (i=FLASHBLEND_VERTS ; i>0 ; i--)
	{
		for (j=0 ; j<3 ; j++)
			flashblend_vcoords[i][j] = light->origin[j] + (vright[j]*(*bub_cos) +
				+ vup[j]*(*bub_sin)) * rad;
		bub_sin++; 
		bub_cos++;
	}
	if (dtype == 0)
	{
		//flashblend 3d-ish
		VectorMA(flashblend_vcoords[0], -rad/1.5, vpn, flashblend_vcoords[0]);
	}
	else if (dtype != 1)
	{
		//prepass lights needs to be fully infront of the light. the glsl is a fullscreen-style effect, but we can benefit from early-z and scissoring
		vec3_t diff;
		VectorSubtract(r_origin, light->origin, diff);
		VectorNormalize(diff);
		for (i=0 ; i<=FLASHBLEND_VERTS ; i++)
			VectorMA(flashblend_vcoords[i], rad, diff, flashblend_vcoords[i]);
	}
	return true;
}

/*
=============
R_RenderDlights
=============
*/
void R_RenderDlights (void)
{
	int		i;
	dlight_t	*l;
	vec3_t waste1, waste2;
	unsigned int beflags = 0;
	float intensity, cscale;
	qboolean coronastyle;
	qboolean flashstyle;
	float dist;

	if (!r_coronas.value && !r_flashblend.value)
		return;

//	r_dlightframecount = r_framecount + 1;	// because the count hasn't
											//  advanced yet for this frame

	l = cl_dlights+rtlights_first;
	for (i=rtlights_first; i<rtlights_max; i++, l++)
	{
		if (!l->radius)
			continue;

		if (l->corona <= 0)
			continue;

		//dlights emitting from the local player are not visible as flashblends
		if (l->key == r_refdef.playerview->viewentity)
			continue;	//was a glow
		if (l->key == -(r_refdef.playerview->viewentity))
			continue;	//was a muzzleflash

		coronastyle = (l->flags & (LFLAG_NORMALMODE|LFLAG_REALTIMEMODE)) && r_coronas.value;
		flashstyle = ((l->flags & LFLAG_FLASHBLEND) && r_flashblend.value);

		if (!coronastyle && !flashstyle)
			continue;
		if (coronastyle && flashstyle)
			flashstyle = false;

		cscale = l->coronascale;
		intensity = l->corona;// * 0.25;
		if (coronastyle)
			intensity *= r_coronas.value;
		else
			intensity *= r_flashblend.value;
		if (intensity <= 0 || cscale <= 0)
			continue;

		//prevent the corona from intersecting with the near clip plane by just fading it away if its too close
		VectorSubtract(l->origin, r_refdef.vieworg, waste1);
		dist = VectorLength(waste1);
		if (dist < r_coronas_mindist.value+r_coronas_fadedist.value)
		{
			if (dist <= r_coronas_mindist.value)
				continue;
			intensity *= (dist-r_coronas_mindist.value) / r_coronas_fadedist.value;
		}

		/*coronas use depth testing to compute visibility*/
		if (coronastyle)
		{
			int method;
			if (!*r_coronas_occlusion.string)
				method = 4;	//default to using hardware queries where possible.
			else
				method = r_coronas_occlusion.ival;

			switch(method)
			{
			case 2:
				if (TraceLineR(r_refdef.vieworg, l->origin, waste1, waste2))
					continue;
				break;
			case 0:
				break;
			case 3:
#ifdef GLQUAKE
				if (qrenderer == QR_OPENGL)
				{
					float depth;
					vec3_t out;
					float v[4], tempv[4];
					float mvp[16];

					v[0] = l->origin[0];
					v[1] = l->origin[1];
					v[2] = l->origin[2];
					v[3] = 1;

					Matrix4_Multiply(r_refdef.m_projection, r_refdef.m_view, mvp);
					Matrix4x4_CM_Transform4(mvp, v, tempv);

					tempv[0] /= tempv[3];
					tempv[1] /= tempv[3];
					tempv[2] /= tempv[3];

					out[0] = (1+tempv[0])/2;
					out[1] = (1+tempv[1])/2;
					out[2] = (1+tempv[2])/2;

					out[0] = out[0]*r_refdef.pxrect.width + r_refdef.pxrect.x;
					out[1] = out[1]*r_refdef.pxrect.height + r_refdef.pxrect.y;
					if (tempv[3] < 0)
						out[2] *= -1;

					if (out[2] < 0)
						continue;

					//FIXME: in terms of performance, mixing reads+draws is BAD BAD BAD. SERIOUSLY BAD
					//it would be an improvement to calculate all of these at once.
					qglReadPixels(out[0], out[1], 1, 1, GL_DEPTH_COMPONENT, GL_FLOAT, &depth);
					if (depth < out[2])
						continue;
					break;
				}
#endif
				//other renderers fall through
			case 4:
#ifdef GLQUAKE
				if (qrenderer == QR_OPENGL && qglGenQueriesARB)
				{
					GLuint res;
					qboolean requery = true;
					if (r_refdef.recurse)
						requery = false;
					else if (l->coronaocclusionquery)
					{
						qglGetQueryObjectuivARB(l->coronaocclusionquery, GL_QUERY_RESULT_AVAILABLE_ARB, &res);
						if (res)
							qglGetQueryObjectuivARB(l->coronaocclusionquery, GL_QUERY_RESULT_ARB, &l->coronaocclusionresult);
						else if (!l->coronaocclusionresult)
							continue;	//query still running, nor currently visible.
						else
							requery = false;
					}
					else
					{
						qglGenQueriesARB(1, &l->coronaocclusionquery);
					}

					if (requery)
					{
						qglBeginQueryARB(GL_SAMPLES_PASSED_ARB, l->coronaocclusionquery);
						R_BuildDlightMesh (l, intensity*10, cscale*.1, coronastyle);
						BE_DrawMesh_Single(occluded_shader, &flashblend_mesh, NULL, beflags);
						qglEndQueryARB(GL_SAMPLES_PASSED_ARB);
					}

					if (!l->coronaocclusionresult)
						continue;
					break;
				}
#endif
				//other renderers fall through
			default:
			case 1:
				if (CL_TraceLine(r_refdef.vieworg, l->origin, waste1, NULL, NULL) < 1)
					continue;
				break;
			}
		}

		if (!R_BuildDlightMesh (l, intensity, cscale, coronastyle) && !coronastyle)
			AddLightBlend (l->color[0], l->color[1], l->color[2], l->radius * 0.0003);
		else
			BE_DrawMesh_Single(flashblend_shader, &flashblend_mesh, NULL, (coronastyle?BEF_FORCENODEPTH|BEF_FORCEADDITIVE:0)|beflags);
	}
}


qboolean Sh_GenerateShadowMap(dlight_t *l);
void R_GenDlightMesh(struct batch_s *batch)
{
	static mesh_t *meshptr;
	dlight_t	*l = cl_dlights + batch->surf_first;

	int lightflags = batch->surf_count;

	BE_SelectDLight(l, l->color, l->axis, lightflags);
	if (lightflags & LSHADER_SMAP)
	{
		if (!Sh_GenerateShadowMap(l))
		{
			batch->meshes = 0;
			return;
		}
		BE_SelectEntity(&r_worldentity);
		BE_SelectMode(BEM_STANDARD);
	}

	if (!R_BuildDlightMesh (l, 2, 1, 2))
	{
		int i;
		static vec2_t s[4] = {{1, -1}, {-1, -1}, {-1, 1}, {1, 1}};
		for (i = 0; i < 4; i++)
		{
			VectorMA(r_origin, 32, vpn, flashblend_vcoords[i]);
			VectorMA(flashblend_vcoords[i], s[i][0]*320, vright, flashblend_vcoords[i]);
			VectorMA(flashblend_vcoords[i], s[i][1]*320, vup, flashblend_vcoords[i]);
		}

		meshptr = &flashblend_fsmesh;
	}
	else
	{
		meshptr = &flashblend_mesh;
	}
	batch->mesh = &meshptr;
}
void R_GenDlightBatches(batch_t *batches[])
{
	int i, j, sort;
	dlight_t	*l;
	batch_t		*b;
	int lmode;
	if (!r_lightprepass)
		return;

	if (!lpplight_shader[0])
	{
		lpplight_shader[0] = R_RegisterShader("lpp_light", SUF_NONE,
						"{\n"
							"program lpp_light\n"
							"{\n"
								"map $sourcecolour\n"
								"blendfunc gl_one gl_one\n"
								"nodepthtest\n"
							"}\n"
							"surfaceparm nodlight\n"
							"lpp_light\n"
						"}\n"
					);
		lpplight_shader[LSHADER_SMAP] = R_RegisterShader("lpp_light#PCF", SUF_NONE,
						"{\n"
							"program lpp_light\n"
							"{\n"
								"map $sourcecolour\n"
								"blendfunc gl_one gl_one\n"
								"nodepthtest\n"
							"}\n"
							"surfaceparm nodlight\n"
							"lpp_light\n"
						"}\n"
					);
	}

	l = cl_dlights+rtlights_first;
	for (i=rtlights_first; i<rtlights_max; i++, l++)
	{
		if (!l->radius)
			continue;

		if (R_CullSphere(l->origin, l->radius))
			continue;

		lmode = 0;
		if (!(((i >= RTL_FIRST)?!r_shadow_realtime_world_shadows.ival:!r_shadow_realtime_dlight_shadows.ival) || l->flags & LFLAG_NOSHADOWS))
			lmode |= LSHADER_SMAP;
//		if (TEXLOADED(l->cubetexture))
//			lmode |= LSHADER_CUBE;

		b = BE_GetTempBatch();
		if (!b)
			return;

		b->flags = 0;
		b->shader = lpplight_shader[lmode];
		sort = b->shader->sort;
		b->buildmeshes = R_GenDlightMesh;
		b->ent = &r_worldentity;
		b->mesh = NULL;
		b->firstmesh = 0;
		b->meshes = 1;
		b->skin = NULL;
		b->texture = NULL;
		for (j = 0; j < MAXRLIGHTMAPS; j++)
			b->lightmap[j] = -1;
		b->surf_first = i;
		b->surf_count = lmode;
		b->flags |= BEF_NOSHADOWS;
		b->vbo = NULL;
		b->next = batches[sort];
		batches[sort] = b;
	}
}

/*
=============================================================================

DYNAMIC LIGHTS

=============================================================================
*/

/*
=============
R_PushDlights
=============
*/
void R_PushDlights (void)
{
	int		i;
	dlight_t	*l;

	r_dlightframecount = r_framecount + 1;	// because the count hasn't
											//  advanced yet for this frame

#ifdef RTLIGHTS
	/*if we're doing full rtlighting only, then don't bother calculating old-style dlights as they won't be visible anyway*/
	if (r_shadow_realtime_world.ival && r_shadow_realtime_world_lightmaps.value < 0.1)
		return;
#endif

	if (r_dynamic.ival <= 0|| !cl.worldmodel)
		return;

	if (!cl.worldmodel->nodes)
		return;

	currentmodel = cl.worldmodel;
	if (!currentmodel->funcs.MarkLights)
		return;
	
	l = cl_dlights+rtlights_first;
	for (i=rtlights_first ; i <= DL_LAST ; i++, l++)
	{
		if (!l->radius || !(l->flags & LFLAG_LIGHTMAP))
			continue;
		currentmodel->funcs.MarkLights( l, 1<<i, currentmodel->nodes );
	}
}



/////////////////////////////////////////////////////////////
//rtlight loading

#ifdef RTLIGHTS
qboolean R_ImportRTLights(char *entlump)
{
	typedef enum lighttype_e {LIGHTTYPE_MINUSX, LIGHTTYPE_RECIPX, LIGHTTYPE_RECIPXX, LIGHTTYPE_NONE, LIGHTTYPE_SUN, LIGHTTYPE_MINUSXX} lighttype_t;

	/*I'm using the DP code so I know I'll get the DP results*/
	int entnum, style, islight, skin, pflags, n;
	lighttype_t type;
	float origin[3], angles[3], radius, color[3], light[4], fadescale, lightscale, originhack[3], overridecolor[3], colourscales[3], vec[4];
	char key[256], value[8192];
	int nest;
	qboolean okay = false;

	COM_Parse(entlump);
	if (!strcmp(com_token, "Version"))
	{
		entlump = COM_Parse(entlump);
		entlump = COM_Parse(entlump);
	}

	for (entnum = 0; ;entnum++)
	{
		entlump = COM_Parse(entlump);
		if (com_token[0] != '{')
			break;

		type = LIGHTTYPE_MINUSX;
		origin[0] = origin[1] = origin[2] = 0;
		originhack[0] = originhack[1] = originhack[2] = 0;
		angles[0] = angles[1] = angles[2] = 0;
		color[0] = color[1] = color[2] = 1;
		light[0] = light[1] = light[2] = 1;light[3] = 300;
		overridecolor[0] = overridecolor[1] = overridecolor[2] = 1;
		fadescale = 1;
		lightscale = 1;
		style = 0;
		skin = 0;
		pflags = 0;
		VectorSet(colourscales, r_editlights_import_ambient.value, r_editlights_import_diffuse.value, r_editlights_import_specular.value);
		//effects = 0;
		islight = false;
		nest = 1;
		while (1)
		{
			entlump = COM_Parse(entlump);
			if (!entlump)
				break; // error
			if (com_token[0] == '{')
			{
				nest++;
				continue;
			}
			if (com_token[0] == '}')
			{
				nest--;
				if (!nest)
					break; // end of entity
				continue;
			}
			if (nest!=1)
				continue;
			if (com_token[0] == '_')
				Q_strncpyz(key, com_token + 1, sizeof(key));
			else
				Q_strncpyz(key, com_token, sizeof(key));
			while (key[strlen(key)-1] == ' ') // remove trailing spaces
				key[strlen(key)-1] = 0;
			entlump = COM_Parse(entlump);
			if (!entlump)
				break; // error
			Q_strncpyz(value, com_token, sizeof(value));

			// now that we have the key pair worked out...
			if (!strcmp("light", key))
			{
				n = sscanf(value, "%f %f %f %f", &vec[0], &vec[1], &vec[2], &vec[3]);
				if (n == 1)
				{
					// quake
					light[0] = vec[0] * (1.0f / 256.0f);
					light[1] = vec[0] * (1.0f / 256.0f);
					light[2] = vec[0] * (1.0f / 256.0f);
					light[3] = vec[0];
				}
				else if (n == 4)
				{
					// halflife
					light[0] = vec[0] * (1.0f / 255.0f);
					light[1] = vec[1] * (1.0f / 255.0f);
					light[2] = vec[2] * (1.0f / 255.0f);
					light[3] = vec[3];
				}
			}
			else if (!strcmp("delay", key))
				type = atoi(value);
			else if (!strcmp("origin", key))
				sscanf(value, "%f %f %f", &origin[0], &origin[1], &origin[2]);
			else if (!strcmp("angle", key))
				angles[0] = 0, angles[1] = atof(value), angles[2] = 0;
			else if (!strcmp("angles", key))
				sscanf(value, "%f %f %f", &angles[0], &angles[1], &angles[2]);
			else if (!strcmp("color", key))
				sscanf(value, "%f %f %f", &color[0], &color[1], &color[2]);
			else if (!strcmp("wait", key))
				fadescale = atof(value);
			else if (!strcmp("classname", key))
			{
				if (!strncmp(value, "light", 5))
				{
					islight = true;
					if (!strcmp(value, "light_fluoro"))
					{
						originhack[0] = 0;
						originhack[1] = 0;
						originhack[2] = 0;
						overridecolor[0] = 1;
						overridecolor[1] = 1;
						overridecolor[2] = 1;
					}
					if (!strcmp(value, "light_fluorospark"))
					{
						originhack[0] = 0;
						originhack[1] = 0;
						originhack[2] = 0;
						overridecolor[0] = 1;
						overridecolor[1] = 1;
						overridecolor[2] = 1;
					}
					if (!strcmp(value, "light_globe"))
					{
						originhack[0] = 0;
						originhack[1] = 0;
						originhack[2] = 0;
						overridecolor[0] = 1;
						overridecolor[1] = 0.8;
						overridecolor[2] = 0.4;
					}
					if (!strcmp(value, "light_flame_large_yellow"))
					{
						originhack[0] = 0;
						originhack[1] = 0;
						originhack[2] = 0;
						overridecolor[0] = 1;
						overridecolor[1] = 0.5;
						overridecolor[2] = 0.1;
					}
					if (!strcmp(value, "light_flame_small_yellow"))
					{
						originhack[0] = 0;
						originhack[1] = 0;
						originhack[2] = 0;
						overridecolor[0] = 1;
						overridecolor[1] = 0.5;
						overridecolor[2] = 0.1;
					}
					if (!strcmp(value, "light_torch_small_white"))
					{
						originhack[0] = 0;
						originhack[1] = 0;
						originhack[2] = 0;
						overridecolor[0] = 1;
						overridecolor[1] = 0.5;
						overridecolor[2] = 0.1;
					}
					if (!strcmp(value, "light_torch_small_walltorch"))
					{
						originhack[0] = 0;
						originhack[1] = 0;
						originhack[2] = 0;
						overridecolor[0] = 1;
						overridecolor[1] = 0.5;
						overridecolor[2] = 0.1;
					}
				}
			}
			else if (!strcmp("style", key))
				style = atoi(value);
			else if (!strcmp("skin", key))
				skin = (int)atof(value);
			else if (!strcmp("pflags", key))
				pflags = (int)atof(value);
			//else if (!strcmp("effects", key))
				//effects = (int)atof(value);

			else if (!strcmp("scale", key))
				lightscale = atof(value);
			else if (!strcmp("fade", key))
				fadescale = atof(value);

#ifdef MAP_PROC
			else if (!strcmp("nodynamicshadows", key))	//doom3
				;
			else if (!strcmp("noshadows", key))	//doom3
			{
				if (atof(value))
					pflags |= PFLAGS_NOSHADOW;
			}
			else if (!strcmp("nospecular", key))//doom3
			{
				if (atof(value))
					colourscales[2] = 0;
			}
			else if (!strcmp("nodiffuse", key))	//doom3
			{
				if (atof(value))
					colourscales[1] = 0;
			}
#endif

			else if (!strcmp("light_radius", key))
			{
				light[0] = 1;
				light[1] = 1;
				light[2] = 1;
				light[3] = atof(value);
			}
			else if (entnum == 0 && !strcmp("noautolight", key))
			{
				//tenebrae compat. don't generate rtlights automagically if the world entity specifies this.
				if (atoi(value))
				{
					okay = true;
					return okay;
				}
			}
			else if (entnum == 0 && !strcmp("lightmapbright", key))
			{
				//tenebrae compat. this overrides r_shadow_realtime_world_lightmap
			}
		}
		if (!islight)
			continue;
		if (lightscale <= 0)
			lightscale = 1;
		if (fadescale <= 0)
			fadescale = 1;
		if (color[0] == color[1] && color[0] == color[2])
		{
			color[0] *= overridecolor[0];
			color[1] *= overridecolor[1];
			color[2] *= overridecolor[2];
		}
		radius = light[3] * r_editlights_import_radius.value * lightscale / fadescale;
		color[0] = color[0] * light[0];
		color[1] = color[1] * light[1];
		color[2] = color[2] * light[2];
		switch (type)
		{
		case LIGHTTYPE_MINUSX:
			break;
		case LIGHTTYPE_RECIPX:
			radius *= 2;
			VectorScale(color, (1.0f / 16.0f), color);
			break;
		case LIGHTTYPE_RECIPXX:
			radius *= 2;
			VectorScale(color, (1.0f / 16.0f), color);
			break;
		default:
		case LIGHTTYPE_NONE:
			break;
		case LIGHTTYPE_SUN:
			break;
		case LIGHTTYPE_MINUSXX:
			break;
		}
		VectorAdd(origin, originhack, origin);
		if (radius >= 1 && !(cl.worldmodel->funcs.PointContents(cl.worldmodel, NULL, origin) & FTECONTENTS_SOLID))
		{
			dlight_t *dl = CL_AllocSlight();
			if (!dl)
				break;
			VectorCopy(origin, dl->origin);
			AngleVectors(angles, dl->axis[0], dl->axis[1], dl->axis[2]);
			VectorInverse(dl->axis[1]);
			dl->radius = radius;
			VectorCopy(color, dl->color);
			dl->flags = 0;
			dl->flags |= LFLAG_REALTIMEMODE;
			dl->flags |= (pflags & PFLAGS_CORONA)?LFLAG_FLASHBLEND:0;
			dl->flags |= (pflags & PFLAGS_NOSHADOW)?LFLAG_NOSHADOWS:0;
			dl->style = style+1;
			VectorCopy(colourscales, dl->lightcolourscales);
			if (skin >= 16)
				R_LoadNumberedLightTexture(dl, skin);

			okay = true;
		}
	}

	return okay;
}

qboolean R_LoadRTLights(void)
{
	dlight_t *dl;
	char fname[MAX_QPATH];
	char cubename[MAX_QPATH];
	char *file;
	char *end;
	int style;

	vec3_t org;
	float radius;
	vec3_t rgb;
	vec3_t avel;
	float fov;
	unsigned int flags;

	float coronascale;
	float corona;
	float ambientscale, diffusescale, specularscale;
	vec3_t angles;

	//delete all old lights, even dynamic ones
	rtlights_first = RTL_FIRST;
	rtlights_max = RTL_FIRST;

	COM_StripExtension(cl.worldmodel->name, fname, sizeof(fname));
	strncat(fname, ".rtlights", MAX_QPATH-1);

	file = COM_LoadTempFile(fname, NULL);
	if (file)
	while(1)
	{
		end = strchr(file, '\n');
		if (!end)
			end = file + strlen(file);
		if (end == file)
			break;
		*end = '\0';

		while(*file == ' ' || *file == '\t')
			file++;
		if (*file == '#')
		{
			file++;
			while(*file == ' ' || *file == '\t')
				file++;
			file = COM_Parse(file);
			if (!Q_strcasecmp(com_token, "lightmaps"))
			{
				file = COM_Parse(file);
				//foo = atoi(com_token);
			}
			else
				Con_DPrintf("Unknown directive: %s\n", com_token);
			file = end+1;
			continue;
		}
		else if (*file == '!')
		{
			flags = LFLAG_NOSHADOWS;
			file++;
		}
		else
			flags = 0;

		file = COM_Parse(file);
		org[0] = atof(com_token);
		file = COM_Parse(file);
		org[1] = atof(com_token);
		file = COM_Parse(file);
		org[2] = atof(com_token);

		file = COM_Parse(file);
		radius = atof(com_token);

		file = COM_Parse(file);
		rgb[0] = file?atof(com_token):1;
		file = COM_Parse(file);
		rgb[1] = file?atof(com_token):1;
		file = COM_Parse(file);
		rgb[2] = file?atof(com_token):1;

		file = COM_Parse(file);
		style = file?atof(com_token):0;

		file = COM_Parse(file);
		//cubemap
		Q_strncpyz(cubename, com_token, sizeof(cubename));

		file = COM_Parse(file);
		//corona
		corona = file?atof(com_token):0;

		file = COM_Parse(file);
		angles[0] = file?atof(com_token):0;
		file = COM_Parse(file);
		angles[1] = file?atof(com_token):0;
		file = COM_Parse(file);
		angles[2] = file?atof(com_token):0;

		file = COM_Parse(file);
		//corona scale
		coronascale = file?atof(com_token):0.25;

		file = COM_Parse(file);
		//ambient
		ambientscale = file?atof(com_token):0;

		file = COM_Parse(file);
		//diffuse
		diffusescale = file?atof(com_token):1;

		file = COM_Parse(file);
		//specular
		specularscale = file?atof(com_token):1;

		file = COM_Parse(file);
		flags |= file?atoi(com_token):LFLAG_REALTIMEMODE;

		fov = avel[0] = avel[1] = avel[2] = 0;
		while(file)
		{
			file = COM_Parse(file);
			if (!strncmp(com_token, "rotx=", 5))
				avel[0] = file?atof(com_token+5):0;
			else if (!strncmp(com_token, "roty=", 5))
				avel[1] = file?atof(com_token+5):0;
			else if (!strncmp(com_token, "rotz=", 5))
				avel[2] = file?atof(com_token+5):0;
			else if (!strncmp(com_token, "fov=", 4))
				fov = file?atof(com_token+4):0;
		}

		if (radius)
		{
			dl = CL_AllocSlight();
			if (!dl)
				break;

			VectorCopy(org, dl->origin);
			dl->radius = radius;
			VectorCopy(rgb, dl->color);
			dl->corona = corona;
			dl->coronascale = coronascale;
			dl->die = 0;
			dl->flags = flags;
			dl->fov = fov;
			dl->lightcolourscales[0] = ambientscale;
			dl->lightcolourscales[1] = diffusescale;
			dl->lightcolourscales[2] = specularscale;
			AngleVectorsFLU(angles, dl->axis[0], dl->axis[1], dl->axis[2]);
			VectorCopy(avel, dl->rotation);

			Q_strncpyz(dl->cubemapname, cubename, sizeof(dl->cubemapname));
			if (*dl->cubemapname)
				dl->cubetexture = R_LoadReplacementTexture(dl->cubemapname, "", IF_CUBEMAP, NULL, 0, 0, TF_INVALID);
			else
				dl->cubetexture = r_nulltex;

			dl->style = style+1;
		}
		file = end+1;
	}
	return !!file;
}

void R_SaveRTLights_f(void)
{
	dlight_t *light;
	vfsfile_t *f;
	unsigned int i;
	char fname[MAX_QPATH];
	char sysname[MAX_OSPATH];
	vec3_t ang;
	COM_StripExtension(cl.worldmodel->name, fname, sizeof(fname));
	strncat(fname, ".rtlights", MAX_QPATH-1);

	FS_CreatePath(fname, FS_GAMEONLY);
	f = FS_OpenVFS(fname, "wb", FS_GAMEONLY);
	if (!f)
	{
		Con_Printf("couldn't open %s\n", fname);
		return;
	}

//	VFS_PUTS(f, va("#lightmap %f\n", foo));

	for (light = cl_dlights+rtlights_first, i=rtlights_first; i<rtlights_max; i++, light++)
	{
		if (light->die)
			continue;
		if (!light->radius)
			continue;
		VectorAngles(light->axis[0], light->axis[2], ang);
		VFS_PUTS(f, va(
			"%s%f %f %f "
			"%f %f %f %f "
			"%i "
			"\"%s\" %f "
			"%f %f %f "
			"%f %f %f %f %i "
			"rotx=%g roty=%g rotz=%g fov=%g "
			"\n"
			,
			(light->flags & LFLAG_NOSHADOWS)?"!":"", light->origin[0], light->origin[1], light->origin[2],
			light->radius, light->color[0], light->color[1], light->color[2], 
			light->style-1,
			light->cubemapname, light->corona,
			anglemod(-ang[0]), ang[1], ang[2],
			light->coronascale, light->lightcolourscales[0], light->lightcolourscales[1], light->lightcolourscales[2], light->flags&(LFLAG_NORMALMODE|LFLAG_REALTIMEMODE|LFLAG_CREPUSCULAR),
			light->rotation[0],light->rotation[1],light->rotation[2],light->fov
			));
	}
	VFS_CLOSE(f);

	FS_NativePath(fname, FS_GAMEONLY, sysname, sizeof(sysname));
	Con_Printf("rtlights saved to %s\n", sysname);
}

void R_StaticEntityToRTLight(int i)
{
	entity_state_t *state = &cl_static_entities[i].state;
	dlight_t *dl;
	if (!(state->lightpflags&(PFLAGS_FULLDYNAMIC|PFLAGS_CORONA)))
		return;
	dl = CL_AllocSlight();
	if (!dl)
		return;
	VectorCopy(state->origin, dl->origin);
	AngleVectors(state->angles, dl->axis[0], dl->axis[1], dl->axis[2]);
	VectorInverse(dl->axis[1]);
	dl->radius = state->light[3];
	if (!dl->radius)
		dl->radius = 350;
	VectorScale(state->light, 1.0/1024, dl->color);
	if (!state->light[0] && !state->light[1] && !state->light[2])
		VectorSet(dl->color, 1, 1, 1);
	dl->flags = 0;
	dl->flags |= LFLAG_NORMALMODE|LFLAG_REALTIMEMODE;
	dl->flags |= (state->lightpflags & PFLAGS_NOSHADOW)?LFLAG_NOSHADOWS:0;
	if (state->lightpflags & PFLAGS_CORONA)
		dl->corona = 1;
	dl->style = state->lightstyle+1;
	if (state->lightpflags & PFLAGS_FULLDYNAMIC)
	{
		dl->lightcolourscales[0] = r_editlights_import_ambient.value;
		dl->lightcolourscales[1] = r_editlights_import_diffuse.value;
		dl->lightcolourscales[2] = r_editlights_import_specular.value;
	}
	else
	{	//corona-only light
		dl->lightcolourscales[0] = 0;
		dl->lightcolourscales[1] = 0;
		dl->lightcolourscales[2] = 0;
	}
	if (state->skinnum >= 16)
		R_LoadNumberedLightTexture(dl, state->skinnum);
}

void R_ReloadRTLights_f(void)
{
	int i;

	if (!cl.worldmodel)
	{
		Con_Printf("Cannot reload lights at this time\n");
		return;
	}
	rtlights_first = RTL_FIRST;
	rtlights_max = RTL_FIRST;
	if (!strcmp(Cmd_Argv(1), "bsp"))
		R_ImportRTLights(cl.worldmodel->entities);
	else if (!strcmp(Cmd_Argv(1), "rtlights"))
		R_LoadRTLights();
	else if (strcmp(Cmd_Argv(1), "none"))
	{
		R_LoadRTLights();
		if (rtlights_first == rtlights_max)
			R_ImportRTLights(cl.worldmodel->entities);
	}

	for (i = 0; i < cl.num_statics; i++)
	{
		R_StaticEntityToRTLight(i);
	}
}
#endif

/*
=============================================================================

LIGHT SAMPLING

=============================================================================
*/

mplane_t		*lightplane;
vec3_t			lightspot;

static void GLQ3_AddLatLong(qbyte latlong[2], vec3_t dir, float mag)
{
	float lat = (float)latlong[0] * (2 * M_PI)*(1.0 / 255.0);
	float lng = (float)latlong[1] * (2 * M_PI)*(1.0 / 255.0);
	dir[0] += mag * cos ( lng ) * sin ( lat );
	dir[1] += mag * sin ( lng ) * sin ( lat );
	dir[2] += mag * cos ( lat );
}

void GLQ3_LightGrid(model_t *mod, vec3_t point, vec3_t res_diffuse, vec3_t res_ambient, vec3_t res_dir)
{
	q3lightgridinfo_t *lg = (q3lightgridinfo_t *)cl.worldmodel->lightgrid;
	int index[8];
	int vi[3];
	int i, j;
	float t[8];
	vec3_t vf, vf2;
	vec3_t ambient, diffuse, direction;

	if (!lg || (!lg->lightgrid && !lg->rbspelements) || lg->numlightgridelems < 1)
	{
		if(res_ambient)
		{
			res_ambient[0] = 64;
			res_ambient[1] = 64;
			res_ambient[2] = 64;
		}

		if (res_diffuse)
		{
			res_diffuse[0] = 192;
			res_diffuse[1] = 192;
			res_diffuse[2] = 192;
		}

		if (res_dir)
		{
			res_dir[0] = 1;
			res_dir[1] = 1;
			res_dir[2] = 0.1;
		}
		return;
	}

	//If in doubt, steal someone else's code...
	//Thanks QFusion.

	for ( i = 0; i < 3; i++ )
	{
		vf[i] = (point[i] - lg->gridMins[i]) / lg->gridSize[i];
		vi[i] = (int)(vf[i]);
		vf[i] = vf[i] - floor(vf[i]);
		vf2[i] = 1.0f - vf[i];
	}

	for ( i = 0; i < 8; i++ )
	{
		//bound it properly
		index[i] =	bound(0, vi[0]+((i&1)?1:0), lg->gridBounds[0]-1) * 1                 +
					bound(0, vi[1]+((i&2)?1:0), lg->gridBounds[1]-1) * lg->gridBounds[0] +
					bound(0, vi[2]+((i&4)?1:0), lg->gridBounds[2]-1) * lg->gridBounds[3] ;
		t[i] =	((i&1)?vf[0]:vf2[0]) *
				((i&2)?vf[1]:vf2[1]) *
				((i&4)?vf[2]:vf2[2]) ;
	}

	//rbsp has a separate grid->index lookup for compression.
	if (lg->rbspindexes)
	{
		for (i = 0; i < 8; i++)
			index[i] = lg->rbspindexes[index[i]];
	}

	VectorClear(ambient);
	VectorClear(diffuse);
	VectorClear(direction);
	if (lg->rbspelements)
	{
		for (i = 0; i < 8; i++)
		{	//rbsp has up to 4 styles per grid element, which needs to be scaled by that style's current value
			float tot = 0;
			for (j = 0; j < countof(lg->rbspelements[index[i]].styles); j++)
			{
				qbyte st = lg->rbspelements[index[i]].styles[j];
				if (st != 255)
				{
					float mag = d_lightstylevalue[st] * 1.0/255 * t[i];
					//FIXME: cl_lightstyle[st].colours[rgb]
					VectorMA (ambient,      mag, lg->rbspelements[index[i]].ambient[j],   ambient);
					VectorMA (diffuse,      mag, lg->rbspelements[index[i]].diffuse[j],   diffuse);
					tot += mag;
				}
			}
			GLQ3_AddLatLong(lg->rbspelements[index[i]].direction, direction, tot);
		}
	}
	else
	{
		for (i = 0; i < 8; i++)
		{
			VectorMA (ambient,      t[i], lg->lightgrid[index[i]].ambient,   ambient);
			VectorMA (diffuse,      t[i], lg->lightgrid[index[i]].diffuse,   diffuse);
			GLQ3_AddLatLong(lg->lightgrid[index[i]].direction, direction, t[i]);
		}

		VectorScale(ambient, d_lightstylevalue[0]/255.0, ambient);
		VectorScale(diffuse, d_lightstylevalue[0]/255.0, diffuse);
		//FIXME: cl_lightstyle[0].colours[rgb]
	}

	//q3bsp has *4 overbrighting.
//	VectorScale(ambient, 4, ambient);
//	VectorScale(diffuse, 4, diffuse);

	/*ambient is the min level*/
	/*diffuse is the max level*/
	VectorCopy(ambient, res_ambient);
	if (res_diffuse)
		VectorAdd(diffuse, ambient, res_diffuse);
	if (res_dir)
		VectorCopy(direction, res_dir);
}

static int GLRecursiveLightPoint (mnode_t *node, vec3_t start, vec3_t end)
{
	int			r;
	float		front, back, frac;
	int			side;
	mplane_t	*plane;
	vec3_t		mid;
	msurface_t	*surf;
	int			s, t, ds, dt;
	int			i;
	mtexinfo_t	*tex;
	qbyte		*lightmap;
	unsigned	scale;
	int			maps;

	if (cl.worldmodel->fromgame == fg_quake2)
	{
		if (node->contents != -1)
			return -1;		// solid
	}
	else if (node->contents < 0)
		return -1;		// didn't hit anything
	
// calculate mid point

// FIXME: optimize for axial
	plane = node->plane;
	front = DotProduct (start, plane->normal) - plane->dist;
	back = DotProduct (end, plane->normal) - plane->dist;
	side = front < 0;
	
	if ( (back < 0) == side)
		return GLRecursiveLightPoint (node->children[side], start, end);
	
	frac = front / (front-back);
	mid[0] = start[0] + (end[0] - start[0])*frac;
	mid[1] = start[1] + (end[1] - start[1])*frac;
	mid[2] = start[2] + (end[2] - start[2])*frac;
	
// go down front side	
	r = GLRecursiveLightPoint (node->children[side], start, mid);
	if (r >= 0)
		return r;		// hit something
		
	if ( (back < 0) == side )
		return -1;		// didn't hit anuthing
		
// check for impact on this node
	VectorCopy (mid, lightspot);
	lightplane = plane;

	surf = cl.worldmodel->surfaces + node->firstsurface;
	for (i=0 ; i<node->numsurfaces ; i++, surf++)
	{
		if (surf->flags & SURF_DRAWTILED)
			continue;	// no lightmaps

		tex = surf->texinfo;
		
		s = DotProduct (mid, tex->vecs[0]) + tex->vecs[0][3];
		t = DotProduct (mid, tex->vecs[1]) + tex->vecs[1][3];;

		if (s < surf->texturemins[0] || t < surf->texturemins[1])
			continue;
		
		ds = s - surf->texturemins[0];
		dt = t - surf->texturemins[1];
		
		if ( ds > surf->extents[0] || dt > surf->extents[1] )
			continue;

		if (!surf->samples)
			return 0;

		ds >>= surf->lmshift;
		dt >>= surf->lmshift;

		lightmap = surf->samples;
		r = 0;
		if (lightmap)
		{
			if (cl.worldmodel->engineflags & MDLF_RGBLIGHTING)
			{
				lightmap += (dt * ((surf->extents[0]>>surf->lmshift)+1) + ds)*3;

				for (maps = 0 ; maps < MAXQ1LIGHTMAPS && surf->styles[maps] != 255 ;
						maps++)
				{
					scale = d_lightstylevalue[surf->styles[maps]];
					r += (lightmap[0]+lightmap[1]+lightmap[2]) * scale / 3;
					lightmap += ((surf->extents[0]>>surf->lmshift)+1) * ((surf->extents[1]>>surf->lmshift)+1)*3;
				}

			}
			else
			{
				lightmap += dt * ((surf->extents[0]>>surf->lmshift)+1) + ds;

				for (maps = 0 ; maps < MAXQ1LIGHTMAPS && surf->styles[maps] != 255 ;
						maps++)
				{
					scale = d_lightstylevalue[surf->styles[maps]];
					r += *lightmap * scale;
					lightmap += ((surf->extents[0]>>surf->lmshift)+1) * ((surf->extents[1]>>surf->lmshift)+1);
				}
			}
			
			r >>= 8;
		}
		
		return r;
	}

// go down back side
	return GLRecursiveLightPoint (node->children[!side], mid, end);
}



int R_LightPoint (vec3_t p)
{
	vec3_t		end;
	int			r;

	if (r_refdef.flags & 1)
		return 255;

	if (!cl.worldmodel || !cl.worldmodel->lightdata)
		return 255;

	if (cl.worldmodel->fromgame == fg_quake3)
	{
		GLQ3_LightGrid(cl.worldmodel, p, NULL, end, NULL);
		return (end[0] + end[1] + end[2])/3;
	}

	end[0] = p[0];
	end[1] = p[1];
	end[2] = p[2] - 2048;

	r = GLRecursiveLightPoint (cl.worldmodel->rootnode, p, end);
	
	if (r == -1)
		r = 0;

	return r;
}



#ifdef PEXT_LIGHTSTYLECOL

static float *GLRecursiveLightPoint3C (model_t *mod, mnode_t *node, vec3_t start, vec3_t end)
{
	static float l[6];
	float *r;
	float		front, back, frac;
	int			side;
	mplane_t	*plane;
	vec3_t		mid;
	msurface_t	*surf;
	int			s, t, ds, dt;
	int			i;
	mtexinfo_t	*tex;
	qbyte		*lightmap, *deluxmap;
	float	scale, overbright;
	int			maps;

	if (mod->fromgame == fg_quake2)
	{
		if (node->contents != -1)
			return NULL;		// solid
	}
	else if (node->contents < 0)
		return NULL;		// didn't hit anything
	
// calculate mid point

// FIXME: optimize for axial
	plane = node->plane;
	front = DotProduct (start, plane->normal) - plane->dist;
	back = DotProduct (end, plane->normal) - plane->dist;
	side = front < 0;
	
	if ( (back < 0) == side)
		return GLRecursiveLightPoint3C (mod, node->children[side], start, end);
	
	frac = front / (front-back);
	mid[0] = start[0] + (end[0] - start[0])*frac;
	mid[1] = start[1] + (end[1] - start[1])*frac;
	mid[2] = start[2] + (end[2] - start[2])*frac;
	
// go down front side	
	r = GLRecursiveLightPoint3C (mod, node->children[side], start, mid);
	if (r && r[0]+r[1]+r[2] >= 0)
		return r;		// hit something
		
	if ( (back < 0) == side )
		return NULL;		// didn't hit anuthing
		
// check for impact on this node
	VectorCopy (mid, lightspot);
	lightplane = plane;

	surf = mod->surfaces + node->firstsurface;
	for (i=0 ; i<node->numsurfaces ; i++, surf++)
	{
		if (surf->flags & SURF_DRAWTILED)
			continue;	// no lightmaps

		tex = surf->texinfo;
		
		s = DotProduct (mid, tex->vecs[0]) + tex->vecs[0][3];
		t = DotProduct (mid, tex->vecs[1]) + tex->vecs[1][3];

		if (s < surf->texturemins[0] ||
		t < surf->texturemins[1])
			continue;
		
		ds = s - surf->texturemins[0];
		dt = t - surf->texturemins[1];
		
		if ( ds > surf->extents[0] || dt > surf->extents[1] )
			continue;

		if (!surf->samples)
		{
			l[0]=0;l[1]=0;l[2]=0;
			l[3]=0;l[4]=1;l[5]=1;
			return l;
		}

		ds >>= surf->lmshift;
		dt >>= surf->lmshift;

		lightmap = surf->samples;
		l[0]=0;l[1]=0;l[2]=0;
		l[3]=0;l[4]=0;l[5]=0;
		if (lightmap)
		{
			overbright = 1/255.0f;
			if (mod->deluxdata)
			{
				if (mod->engineflags & MDLF_RGBLIGHTING)
				{
					deluxmap = surf->samples - mod->lightdata + mod->deluxdata;

					lightmap += (dt * ((surf->extents[0]>>surf->lmshift)+1) + ds)*3;
					deluxmap += (dt * ((surf->extents[0]>>surf->lmshift)+1) + ds)*3;
					for (maps = 0 ; maps < MAXQ1LIGHTMAPS && surf->styles[maps] != 255 ;
							maps++)
					{
						scale = d_lightstylevalue[surf->styles[maps]]*overbright;

						l[0] += lightmap[0] * scale * cl_lightstyle[surf->styles[maps]].colours[0];
						l[1] += lightmap[1] * scale * cl_lightstyle[surf->styles[maps]].colours[1];
						l[2] += lightmap[2] * scale * cl_lightstyle[surf->styles[maps]].colours[2];

						l[3] += (deluxmap[0]-127)*scale;
						l[4] += (deluxmap[1]-127)*scale;
						l[5] += (deluxmap[2]-127)*scale;

						lightmap += ((surf->extents[0]>>surf->lmshift)+1) *
								((surf->extents[1]>>surf->lmshift)+1) * 3;
						deluxmap += ((surf->extents[0]>>surf->lmshift)+1) *
								((surf->extents[1]>>surf->lmshift)+1) * 3;
					}

				}
				else
				{
					deluxmap = (surf->samples - mod->lightdata)*3 + mod->deluxdata;

					lightmap += (dt * ((surf->extents[0]>>surf->lmshift)+1) + ds);
					deluxmap += (dt * ((surf->extents[0]>>surf->lmshift)+1) + ds)*3;
					for (maps = 0 ; maps < MAXQ1LIGHTMAPS && surf->styles[maps] != 255 ;
							maps++)
					{
						scale = d_lightstylevalue[surf->styles[maps]]*overbright;

						l[0] += *lightmap * scale * cl_lightstyle[surf->styles[maps]].colours[0];
						l[1] += *lightmap * scale * cl_lightstyle[surf->styles[maps]].colours[1];
						l[2] += *lightmap * scale * cl_lightstyle[surf->styles[maps]].colours[2];

						l[3] += deluxmap[0]*scale;
						l[4] += deluxmap[1]*scale;
						l[5] += deluxmap[2]*scale;

						lightmap += ((surf->extents[0]>>surf->lmshift)+1) *
								((surf->extents[1]>>surf->lmshift)+1);
						deluxmap += ((surf->extents[0]>>surf->lmshift)+1) *
								((surf->extents[1]>>surf->lmshift)+1) * 3;
					}
				}

			}
			else
			{
				if (mod->engineflags & MDLF_RGBLIGHTING)
				{
					lightmap += (dt * ((surf->extents[0]>>surf->lmshift)+1) + ds)*3;
					for (maps = 0 ; maps < MAXQ1LIGHTMAPS && surf->styles[maps] != 255 ;
							maps++)
					{
						scale = d_lightstylevalue[surf->styles[maps]]*overbright;

						l[0] += lightmap[0] * scale * cl_lightstyle[surf->styles[maps]].colours[0];
						l[1] += lightmap[1] * scale * cl_lightstyle[surf->styles[maps]].colours[1];
						l[2] += lightmap[2] * scale * cl_lightstyle[surf->styles[maps]].colours[2];

						lightmap += ((surf->extents[0]>>surf->lmshift)+1) *
								((surf->extents[1]>>surf->lmshift)+1) * 3;
					}

				}
				else
				{
					lightmap += (dt * ((surf->extents[0]>>surf->lmshift)+1) + ds);
					for (maps = 0 ; maps < MAXQ1LIGHTMAPS && surf->styles[maps] != 255 ;
							maps++)
					{
						scale = d_lightstylevalue[surf->styles[maps]]*overbright;

						l[0] += *lightmap * scale * cl_lightstyle[surf->styles[maps]].colours[0];
						l[1] += *lightmap * scale * cl_lightstyle[surf->styles[maps]].colours[1];
						l[2] += *lightmap * scale * cl_lightstyle[surf->styles[maps]].colours[2];

						lightmap += ((surf->extents[0]>>surf->lmshift)+1) *
								((surf->extents[1]>>surf->lmshift)+1);
					}
				}
			}
		}
		
		return l;
	}

// go down back side
	return GLRecursiveLightPoint3C (mod, node->children[!side], mid, end);
}

#endif

void GLQ1BSP_LightPointValues(model_t *model, vec3_t point, vec3_t res_diffuse, vec3_t res_ambient, vec3_t res_dir)
{
	vec3_t		end;
	float *r;
#ifdef RTLIGHTS
	extern cvar_t r_shadow_realtime_world, r_shadow_realtime_world_lightmaps;
#endif

	if (!model->lightdata || r_fullbright.ival)
	{
		res_diffuse[0] = 0;
		res_diffuse[1] = 0;
		res_diffuse[2] = 0;
	
		res_ambient[0] = 255;
		res_ambient[1] = 255;
		res_ambient[2] = 255;

		res_dir[0] = 1;
		res_dir[1] = 1;
		res_dir[2] = 0.1;
		VectorNormalize(res_dir);
		return;
	}

	end[0] = point[0];
	end[1] = point[1];
	end[2] = point[2] - 2048;

	r = GLRecursiveLightPoint3C(model, model->rootnode, point, end);
	if (r == NULL)
	{
		res_diffuse[0] = 0;
		res_diffuse[1] = 0;
		res_diffuse[2] = 0;
	
		res_ambient[0] = 0;
		res_ambient[1] = 0;
		res_ambient[2] = 0;

		res_dir[0] = 0;
		res_dir[1] = 1;
		res_dir[2] = 1;
	}
	else
	{
		res_diffuse[0] = r[0]*2;
		res_diffuse[1] = r[1]*2;
		res_diffuse[2] = r[2]*2;

		/*bright on one side, dark on the other, but not too dark*/
		res_ambient[0] = r[0]/2;
		res_ambient[1] = r[1]/2;
		res_ambient[2] = r[2]/2;

		res_dir[0] = r[3];
		res_dir[1] = r[4];
		res_dir[2] = -r[5];
		if (!res_dir[0] && !res_dir[1] && !res_dir[2])
			res_dir[1] = res_dir[2] = 1;
		VectorNormalize(res_dir);
	}

#ifdef RTLIGHTS
	if (r_shadow_realtime_world.ival)
	{
		float lm = r_shadow_realtime_world_lightmaps.value;
		if (lm < 0) lm = 0;
		if (lm > 1) lm = 1;
		VectorScale(res_diffuse, lm, res_diffuse);
		VectorScale(res_ambient, lm, res_ambient);
	}
#endif
}

#endif
