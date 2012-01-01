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
#if defined(GLQUAKE) || defined(D3DQUAKE)
#include "glquake.h"
#include "shader.h"

extern cvar_t r_shadow_realtime_world, r_shadow_realtime_world_lightmaps;


int	r_dlightframecount;
int		d_lightstylevalue[256];	// 8.8 fraction of base light value

/*
==================
R_AnimateLight
==================
*/
void R_AnimateLight (void)
{
	int			i,j;
	float f;
	
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
			d_lightstylevalue[j] = 256;
			cl_lightstyle[j].colour = 7;
			continue;
		}
		v1 = i % cl_lightstyle[j].length;
		v1 = cl_lightstyle[j].map[v1] - 'a';

		v2 = (i+1) % cl_lightstyle[j].length;
		v2 = cl_lightstyle[j].map[v2] - 'a';

		vd = v1 - v2;
		if (!r_lightstylesmooth.ival || vd < -r_lightstylesmooth_limit.ival || vd > r_lightstylesmooth_limit.ival)
			d_lightstylevalue[j] = v1*22;
		else
			d_lightstylevalue[j] = (v1*(1-f) + v2*(f))*22;
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
shader_t *flashblend_shader;
shader_t *lpplight_shader;

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
	R_LoadTexture32("***flashblend***", 32, 32, pixels, 0);
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
	flashblend_mesh.colors4f_array = flashblend_colours;
	flashblend_mesh.indexes = flashblend_indexes;
	flashblend_mesh.numindexes = FLASHBLEND_VERTS*3;
	flashblend_mesh.istrifan = true;

	flashblend_fsmesh.numvertexes = 4;
	flashblend_fsmesh.xyz_array = flashblend_vcoords;
	flashblend_fsmesh.st_array = flashblend_tccoords;
	flashblend_fsmesh.colors4f_array = flashblend_colours;
	flashblend_fsmesh.indexes = flashblend_fsindexes;
	flashblend_fsmesh.numindexes = 6;
	flashblend_fsmesh.istrifan = true;

	R_GenerateFlashblendTexture();

	flashblend_shader = R_RegisterShader("flashblend", 
		"{\n"
			"program defaultadditivesprite\n"
			"{\n"
				"map ***flashblend***\n"	
				"blendfunc gl_one gl_one\n"
				"rgbgen vertex\n"
				"alphagen vertex\n"
			"}\n"
		"}\n"
		);
	lpplight_shader = NULL;
}

static qboolean R_BuildDlightMesh(dlight_t *light, float colscale, float radscale, qboolean expand)
{
	int		i, j;
//	float	a;
	vec3_t	v;
	float	rad;
	float	*bub_sin, *bub_cos;
	vec3_t colour;
	extern cvar_t gl_mindist;

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
	if (Length (v) < rad + gl_mindist.value*2)
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
	if (!expand)
		VectorMA(flashblend_vcoords[0], -rad/1.5, vpn, flashblend_vcoords[0]);
	else
	{
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
	float intensity;

	if (r_coronas.value)
		beflags |= BEF_FORCENODEPTH;

//	r_dlightframecount = r_framecount + 1;	// because the count hasn't
											//  advanced yet for this frame

	l = cl_dlights+rtlights_first;
	for (i=rtlights_first; i<rtlights_max; i++, l++)
	{
		if (!l->radius)
			continue;

		if (l->corona <= 0)
			continue;

		if (l->flags & LFLAG_FLASHBLEND)
		{
			//dlights emitting from the local player are not visible as flashblends
			if (l->key == cl.playernum[r_refdef.currentplayernum]+1)
				continue;	//was a glow
			if (l->key == -(cl.playernum[r_refdef.currentplayernum]+1))
				continue;	//was a muzzleflash
		}

		intensity = l->corona * 0.25;
		if (r_flashblend.value && (l->flags & LFLAG_FLASHBLEND))
			intensity = l->corona; /*intensity is already in the corona value...*/
		else
			intensity = l->corona * r_coronas.value;
		if (intensity <= 0)
			continue;

		/*coronas use depth testing to compute visibility*/
		if (r_coronas.value)
		{
			if (TraceLineN(r_refdef.vieworg, l->origin, waste1, waste2))
				continue;
		}

		if (!R_BuildDlightMesh (l, intensity, l->coronascale, false) && r_flashblend.value)
			AddLightBlend (l->color[0], l->color[1], l->color[2], l->radius * 0.0003);
		else
			BE_DrawMesh_Single(flashblend_shader, &flashblend_mesh, NULL, &flashblend_shader->defaulttextures, beflags);
	}
}


void R_GenDlightMesh(struct batch_s *batch)
{
	static mesh_t *meshptr;
	dlight_t	*l = cl_dlights + batch->surf_first;

	BE_SelectDLight(l, l->color);

	if (!R_BuildDlightMesh (l, 2, 1, true))
	{
		int i;
		static vec2_t s[4] = {{1, -1}, {-1, -1}, {-1, 1}, {1, 1}};
		batch->flags |= BEF_FORCENODEPTH;
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
	int i, sort;
	dlight_t	*l;
	batch_t		*b;
	if (!lpplight_shader)
		lpplight_shader = R_RegisterShader("lpp_light", 
						"{\n"
							"program lpp_light\n"
							"{\n"
								"map $sourcecolour\n"
								"blendfunc gl_one gl_one\n"
							"}\n"
							"surfaceparm nodlight\n"
							"lpp_light\n"
						"}\n"
					);

	l = cl_dlights+rtlights_first;
	for (i=rtlights_first; i<rtlights_max; i++, l++)
	{
		if (!l->radius)
			continue;

		if (R_CullSphere(l->origin, l->radius))
			continue;

		b = BE_GetTempBatch();
		if (!b)
			return;

		b->flags = 0;
		sort = lpplight_shader->sort;
		b->buildmeshes = R_GenDlightMesh;
		b->ent = &r_worldentity;
		b->mesh = NULL;
		b->firstmesh = 0;
		b->meshes = 1;
		b->skin = &lpplight_shader->defaulttextures;
		b->texture = NULL;
		b->shader = lpplight_shader;
		b->lightmap = -1;
		b->surf_first = i;
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
	if (r_shadow_realtime_world.value && r_shadow_realtime_world_lightmaps.value < 0.1)
		return;
#endif

	if (!r_dynamic.ival || !cl.worldmodel)
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
void R_ImportRTLights(char *entlump)
{
	typedef enum lighttype_e {LIGHTTYPE_MINUSX, LIGHTTYPE_RECIPX, LIGHTTYPE_RECIPXX, LIGHTTYPE_NONE, LIGHTTYPE_SUN, LIGHTTYPE_MINUSXX} lighttype_t;

	/*I'm using the DP code so I know I'll get the DP results*/
	int entnum, style, islight, skin, pflags, effects, n;
	lighttype_t type;
	float origin[3], angles[3], radius, color[3], light[4], fadescale, lightscale, originhack[3], overridecolor[3], vec[4];
	char key[256], value[8192];
	int nest;

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
		effects = 0;
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
			else if (!strcmp("effects", key))
				effects = (int)atof(value);

			else if (!strcmp("scale", key))
				lightscale = atof(value);
			else if (!strcmp("fade", key))
				fadescale = atof(value);

			else if (!strcmp("light_radius", key))
			{
				light[0] = 1;
				light[1] = 1;
				light[2] = 1;
				light[3] = atof(value);
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
		radius = light[3] * 1/*r_editlights_quakelightsizescale*/ * lightscale / fadescale;
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
		if (radius >= 1)
		{
			dlight_t *dl = CL_AllocSlight();
			if (!dl)
				break;
			VectorCopy(origin, dl->origin);
			AngleVectors(angles, dl->axis[0], dl->axis[1], dl->axis[2]);
			dl->radius = radius;
			VectorCopy(color, dl->color);
			dl->flags = 0;
			dl->flags |= LFLAG_REALTIMEMODE;
			dl->flags |= (pflags & PFLAGS_CORONA)?LFLAG_FLASHBLEND:0;
			dl->flags |= (pflags & PFLAGS_NOSHADOW)?LFLAG_NOSHADOWS:0;
			dl->style = style+1;

			//FIXME: cubemaps if skin >= 16
		}
	}
}

void R_LoadRTLights(void)
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

	file = COM_LoadTempFile(fname);
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
		if (*file == '!')
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
		//corrona scale
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
			dl->lightcolourscales[0] = ambientscale;
			dl->lightcolourscales[1] = diffusescale;
			dl->lightcolourscales[2] = specularscale;
			AngleVectors(angles, dl->axis[0], dl->axis[1], dl->axis[2]);

			Q_strncpyz(dl->cubemapname, cubename, sizeof(dl->cubemapname));
			if (*dl->cubemapname)
				dl->cubetexture = R_LoadReplacementTexture(dl->cubemapname, "", IF_CUBEMAP);
			else
				dl->cubetexture = r_nulltex;

			dl->style = style+1;
		}
		file = end+1;
	}
}

void R_SaveRTLights_f(void)
{
	dlight_t *light;
	vfsfile_t *f;
	unsigned int i;
	char fname[MAX_QPATH];
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
			"\n"
			,
			(light->flags & LFLAG_NOSHADOWS)?"!":"", light->origin[0], light->origin[1], light->origin[2],
			light->radius, light->color[0], light->color[1], light->color[2], 
			light->style-1,
			light->cubemapname, light->corona,
			ang[0], ang[1], ang[2],
			light->coronascale, light->lightcolourscales[0], light->lightcolourscales[1], light->lightcolourscales[2], light->flags&(LFLAG_NORMALMODE|LFLAG_REALTIMEMODE|LFLAG_CREPUSCULAR)
			));
	}
	VFS_CLOSE(f);
	Con_Printf("rtlights saved to %s\n", fname);
}

void R_ReloadRTLights_f(void)
{
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
}
#endif

/*
=============================================================================

LIGHT SAMPLING

=============================================================================
*/

mplane_t		*lightplane;
vec3_t			lightspot;

void GLQ3_LightGrid(model_t *mod, vec3_t point, vec3_t res_diffuse, vec3_t res_ambient, vec3_t res_dir)
{
	q3lightgridinfo_t *lg = (q3lightgridinfo_t *)cl.worldmodel->lightgrid;
	int index[8];
	int vi[3];
	int i, j;
	float t[8], direction_uv[3];
	vec3_t vf, vf2;
	vec3_t ambient, diffuse;

	if (res_dir)
	{
		res_dir[0] = 1;
		res_dir[1] = 1;
		res_dir[2] = 0.1;
	}

	if (!lg || !lg->lightgrid)
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

	index[0] = vi[2]*lg->gridBounds[3] + vi[1]*lg->gridBounds[0] + vi[0];
	index[1] = index[0] + lg->gridBounds[0];
	index[2] = index[0] + lg->gridBounds[3];
	index[3] = index[2] + lg->gridBounds[0];

	index[4] = index[0]+(index[0]<(lg->numlightgridelems-1));
	index[5] = index[1]+(index[1]<(lg->numlightgridelems-1));
	index[6] = index[2]+(index[2]<(lg->numlightgridelems-1));
	index[7] = index[3]+(index[3]<(lg->numlightgridelems-1));

	for ( i = 0; i < 8; i++ )
	{
		if ( index[i] < 0 || index[i] >= (lg->numlightgridelems) )
		{
			res_ambient[0] = 255;	//out of the map
			res_ambient[1] = 255;
			res_ambient[2] = 255;
			return;
		}
	}

	t[0] = vf2[0] * vf2[1] * vf2[2];
	t[1] = vf[0] * vf2[1] * vf2[2];
	t[2] = vf2[0] * vf[1] * vf2[2];
	t[3] = vf[0] * vf[1] * vf2[2];
	t[4] = vf2[0] * vf2[1] * vf[2];
	t[5] = vf[0] * vf2[1] * vf[2];
	t[6] = vf2[0] * vf[1] * vf[2];
	t[7] = vf[0] * vf[1] * vf[2];

	for ( j = 0; j < 3; j++ )
	{
		ambient[j] = 0;
		diffuse[j] = 0;

		for ( i = 0; i < 4; i++ )
		{
			ambient[j] += t[i*2] * lg->lightgrid[ index[i]].ambient[j];
			ambient[j] += t[i*2+1] * lg->lightgrid[ index[i+4]].ambient[j];

			diffuse[j] += t[i*2] * lg->lightgrid[ index[i]].diffuse[j];
			diffuse[j] += t[i*2+1] * lg->lightgrid[ index[i+4]].diffuse[j];
		}
	}

	for ( j = 0; j < 2; j++ )
	{
		direction_uv[j] = 0;

		for ( i = 0; i < 4; i++ )
		{
			direction_uv[j] += t[i*2] * lg->lightgrid[ index[i]].direction[j];
			direction_uv[j] += t[i*2+1] * lg->lightgrid[ index[i+4]].direction[j];
		}

		direction_uv[j] = anglemod ( direction_uv[j] );
	}

	VectorScale(ambient, 4, ambient);
	VectorScale(diffuse, 4, diffuse);

	/*ambient is the min level*/
	/*diffuse is the max level*/
	VectorCopy(ambient, res_ambient);
	if (res_diffuse)
		VectorAdd(diffuse, ambient, res_diffuse);
	if (res_dir)
	{
		vec3_t right, left;
		direction_uv[2] = 0;
		AngleVectors(direction_uv, res_dir, right, left);
	}
}

int GLRecursiveLightPoint (mnode_t *node, vec3_t start, vec3_t end)
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

		ds >>= 4;
		dt >>= 4;

		lightmap = surf->samples;
		r = 0;
		if (lightmap)
		{
			if (cl.worldmodel->engineflags & MDLF_RGBLIGHTING)
			{
				lightmap += (dt * ((surf->extents[0]>>4)+1) + ds)*3;

				for (maps = 0 ; maps < MAXLIGHTMAPS && surf->styles[maps] != 255 ;
						maps++)
				{
					scale = d_lightstylevalue[surf->styles[maps]];
					r += (lightmap[0]+lightmap[1]+lightmap[2]) * scale / 3;
					lightmap += ((surf->extents[0]>>4)+1) *
							((surf->extents[1]>>4)+1)*3;
				}

			}
			else
			{
				lightmap += dt * ((surf->extents[0]>>4)+1) + ds;

				for (maps = 0 ; maps < MAXLIGHTMAPS && surf->styles[maps] != 255 ;
						maps++)
				{
					scale = d_lightstylevalue[surf->styles[maps]];
					r += *lightmap * scale;
					lightmap += ((surf->extents[0]>>4)+1) *
							((surf->extents[1]>>4)+1);
				}
			}
			
			r >>= 8;
		}
		
		return r;
	}

// go down back side
	return GLRecursiveLightPoint (node->children[!side], mid, end);
}



int GLR_LightPoint (vec3_t p)
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

	r = GLRecursiveLightPoint (cl.worldmodel->nodes, p, end);
	
	if (r == -1)
		r = 0;

	return r;
}



#ifdef PEXT_LIGHTSTYLECOL

float *GLRecursiveLightPoint3C (mnode_t *node, vec3_t start, vec3_t end)
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
	float	scale;
	int			maps;

	if (cl.worldmodel->fromgame == fg_quake2)
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
		return GLRecursiveLightPoint3C (node->children[side], start, end);
	
	frac = front / (front-back);
	mid[0] = start[0] + (end[0] - start[0])*frac;
	mid[1] = start[1] + (end[1] - start[1])*frac;
	mid[2] = start[2] + (end[2] - start[2])*frac;
	
// go down front side	
	r = GLRecursiveLightPoint3C (node->children[side], start, mid);
	if (r && r[0]+r[1]+r[2] >= 0)
		return r;		// hit something
		
	if ( (back < 0) == side )
		return NULL;		// didn't hit anuthing
		
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

		ds >>= 4;
		dt >>= 4;

		lightmap = surf->samples;
		l[0]=0;l[1]=0;l[2]=0;
		l[3]=0;l[4]=0;l[5]=0;
		if (lightmap)
		{
			if (cl.worldmodel->deluxdata)
			{
				if (cl.worldmodel->engineflags & MDLF_RGBLIGHTING)
				{
					deluxmap = surf->samples - cl.worldmodel->lightdata + cl.worldmodel->deluxdata;

					lightmap += (dt * ((surf->extents[0]>>4)+1) + ds)*3;
					deluxmap += (dt * ((surf->extents[0]>>4)+1) + ds)*3;
					for (maps = 0 ; maps < MAXLIGHTMAPS && surf->styles[maps] != 255 ;
							maps++)
					{
						scale = d_lightstylevalue[surf->styles[maps]]/256.0f;

						if (cl_lightstyle[surf->styles[maps]].colour & 1)
							l[0] += lightmap[0] * scale;
						if (cl_lightstyle[surf->styles[maps]].colour & 2)
							l[1] += lightmap[1] * scale;
						if (cl_lightstyle[surf->styles[maps]].colour & 4)
							l[2] += lightmap[2] * scale;

						l[3] += (deluxmap[0]-127)*scale;
						l[4] += (deluxmap[1]-127)*scale;
						l[5] += (deluxmap[2]-127)*scale;

						lightmap += ((surf->extents[0]>>4)+1) *
								((surf->extents[1]>>4)+1) * 3;
						deluxmap += ((surf->extents[0]>>4)+1) *
								((surf->extents[1]>>4)+1) * 3;
					}

				}
				else
				{
					deluxmap = (surf->samples - cl.worldmodel->lightdata)*3 + cl.worldmodel->deluxdata;

					lightmap += (dt * ((surf->extents[0]>>4)+1) + ds);
					deluxmap += (dt * ((surf->extents[0]>>4)+1) + ds)*3;
					for (maps = 0 ; maps < MAXLIGHTMAPS && surf->styles[maps] != 255 ;
							maps++)
					{
						scale = d_lightstylevalue[surf->styles[maps]]/256.0f;

						if (cl_lightstyle[surf->styles[maps]].colour & 1)
							l[0] += *lightmap * scale;
						if (cl_lightstyle[surf->styles[maps]].colour & 2)
							l[1] += *lightmap * scale;
						if (cl_lightstyle[surf->styles[maps]].colour & 4)
							l[2] += *lightmap * scale;

						l[3] += deluxmap[0]*scale;
						l[4] += deluxmap[1]*scale;
						l[5] += deluxmap[2]*scale;

						lightmap += ((surf->extents[0]>>4)+1) *
								((surf->extents[1]>>4)+1);
						deluxmap += ((surf->extents[0]>>4)+1) *
								((surf->extents[1]>>4)+1) * 3;
					}
				}

			}
			else
			{
				if (cl.worldmodel->engineflags & MDLF_RGBLIGHTING)
				{
					lightmap += (dt * ((surf->extents[0]>>4)+1) + ds)*3;
					for (maps = 0 ; maps < MAXLIGHTMAPS && surf->styles[maps] != 255 ;
							maps++)
					{
						scale = d_lightstylevalue[surf->styles[maps]]/256.0f;

						if (cl_lightstyle[surf->styles[maps]].colour & 1)
							l[0] += lightmap[0] * scale;
						if (cl_lightstyle[surf->styles[maps]].colour & 2)
							l[1] += lightmap[1] * scale;
						if (cl_lightstyle[surf->styles[maps]].colour & 4)
							l[2] += lightmap[2] * scale;

						lightmap += ((surf->extents[0]>>4)+1) *
								((surf->extents[1]>>4)+1) * 3;
					}

				}
				else
				{
					lightmap += (dt * ((surf->extents[0]>>4)+1) + ds);
					for (maps = 0 ; maps < MAXLIGHTMAPS && surf->styles[maps] != 255 ;
							maps++)
					{
						scale = d_lightstylevalue[surf->styles[maps]]/256.0f;

						if (cl_lightstyle[surf->styles[maps]].colour & 1)
							l[0] += *lightmap * scale;
						if (cl_lightstyle[surf->styles[maps]].colour & 2)
							l[1] += *lightmap * scale;
						if (cl_lightstyle[surf->styles[maps]].colour & 4)
							l[2] += *lightmap * scale;

						lightmap += ((surf->extents[0]>>4)+1) *
								((surf->extents[1]>>4)+1);
					}
				}
			}
		}
		
		return l;
	}

// go down back side
	return GLRecursiveLightPoint3C (node->children[!side], mid, end);
}

#endif

void GLQ1BSP_LightPointValues(model_t *model, vec3_t point, vec3_t res_diffuse, vec3_t res_ambient, vec3_t res_dir)
{
	vec3_t		end;
	float *r;
	extern cvar_t r_shadow_realtime_world, r_shadow_realtime_world_lightmaps;

	if (!cl.worldmodel->lightdata || r_fullbright.ival)
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

	r = GLRecursiveLightPoint3C(model->nodes, point, end);
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
		res_diffuse[0] = r[0];
		res_diffuse[1] = r[1];
		res_diffuse[2] = r[2];

		/*bright on one side, dark on the other, but not too dark*/
		res_ambient[0] = r[0]/3;
		res_ambient[1] = r[1]/3;
		res_ambient[2] = r[2]/3;

		res_dir[0] = r[3];
		res_dir[1] = r[4];
		res_dir[2] = -r[5];
		if (!res_dir[0] && !res_dir[1] && !res_dir[2])
			res_dir[1] = res_dir[2] = 1;
		VectorNormalize(res_dir);
	}

	if (r_shadow_realtime_world.ival)
	{
		VectorScale(res_diffuse, r_shadow_realtime_world_lightmaps.value, res_diffuse);
		VectorScale(res_ambient, r_shadow_realtime_world_lightmaps.value, res_ambient);
	}
}

#endif
