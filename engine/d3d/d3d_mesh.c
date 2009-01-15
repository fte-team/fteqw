#include "quakedef.h"
#ifdef D3DQUAKE
#include "d3dquake.h"

#include "com_mesh.h"

#ifdef _WIN32
#include <malloc.h>
#else
#include <alloca.h>
#endif

extern cvar_t r_fullbrightSkins;
extern cvar_t r_vertexdlights;


typedef struct {
	float x, y, z;
	float s, t;
} meshvert_t;

typedef struct {
	float x, y, z;
	unsigned int colour;
	float s, t;
} meshcolouredvert_t;

void D3D_DrawMesh(mesh_t *mesh)
{
	pD3DDev->lpVtbl->SetTextureStageState(pD3DDev, 0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
	pD3DDev->lpVtbl->SetTextureStageState(pD3DDev, 0, D3DTSS_COLORARG2, D3DTA_CURRENT);
	pD3DDev->lpVtbl->SetTextureStageState(pD3DDev, 0, D3DTSS_COLOROP, D3DTOP_MODULATE);
	{
		int v;
		vec3_t *xyz;
		byte_vec4_t *colour;
		vec2_t *wm;
		xyz = mesh->xyz_array;
		wm = mesh->st_array;
		colour = mesh->colors_array;

		if (colour)
		{
			meshcolouredvert_t *meshvert = alloca(sizeof(meshcolouredvert_t)*mesh->numvertexes);

			for (v = 0; v < mesh->numvertexes; v++, xyz++, wm++, colour++)
			{
				meshvert[v].x = (*xyz)[0];
				meshvert[v].y = (*xyz)[1];
				meshvert[v].z = (*xyz)[2];
				meshvert[v].colour = *(unsigned int*)colour;
				meshvert[v].s = (*wm)[0];
				meshvert[v].t = (*wm)[1];
			}

			pD3DDev->lpVtbl->DrawIndexedPrimitive(pD3DDev, D3DPT_TRIANGLELIST, D3DFVF_XYZ|D3DFVF_DIFFUSE|D3DFVF_TEX1, meshvert, mesh->numvertexes, mesh->indexes, mesh->numindexes, 0);
		}
		else
		{
			meshvert_t *meshvert = alloca(sizeof(meshvert_t)*mesh->numvertexes);

			for (v = 0; v < mesh->numvertexes; v++, xyz++, wm++)
			{
				meshvert[v].x = (*xyz)[0];
				meshvert[v].y = (*xyz)[1];
				meshvert[v].z = (*xyz)[2];
				meshvert[v].s = (*wm)[0];
				meshvert[v].t = (*wm)[1];
			}

			pD3DDev->lpVtbl->DrawIndexedPrimitive(pD3DDev, D3DPT_TRIANGLELIST, D3DFVF_XYZ|D3DFVF_TEX1, meshvert, mesh->numvertexes, mesh->indexes, mesh->numindexes, 0);
		}
	}
}




hashtable_t skincolourmapped;

void d3d_GAliasFlushSkinCache(void)
{
	int i;
	bucket_t *b;
	for (i = 0; i < skincolourmapped.numbuckets; i++)
	{
		while((b = skincolourmapped.bucket[i]))
		{
			skincolourmapped.bucket[i] = b->next;
			BZ_Free(b->data);
		}
	}
	if (skincolourmapped.bucket)
		BZ_Free(skincolourmapped.bucket);
	skincolourmapped.bucket = NULL;
	skincolourmapped.numbuckets = 0;
}

static galiastexnum_t *D3D7_ChooseSkin(galiasinfo_t *inf, char *modelname, int surfnum, entity_t *e)
{
	galiasskin_t *skins;
	galiastexnum_t *texnums;
	int frame;

	int tc, bc;
	int local;

	if (!gl_nocolors.value)
	{
		if (e->scoreboard)
		{
			if (!e->scoreboard->skin)
				Skin_Find(e->scoreboard);
			tc = e->scoreboard->ttopcolor;
			bc = e->scoreboard->tbottomcolor;

			//colour forcing
			if (cl.splitclients<2 && !(cl.fpd & FPD_NO_FORCE_COLOR))	//no colour/skin forcing in splitscreen.
			{
				if (cl.teamplay && cl.spectator)
				{
					local = Cam_TrackNum(0);
					if (local < 0)
						local = cl.playernum[0];
				}
				else
					local = cl.playernum[0];
				if (cl.teamplay && !strcmp(e->scoreboard->team, cl.players[local].team))
				{
					if (cl_teamtopcolor>=0)
						tc = cl_teamtopcolor;
					if (cl_teambottomcolor>=0)
						bc = cl_teambottomcolor;
				}
				else
				{
					if (cl_enemytopcolor>=0)
						tc = cl_enemytopcolor;
					if (cl_enemybottomcolor>=0)
						bc = cl_enemybottomcolor;
				}
			}
		}
		else
		{
			tc = 1;
			bc = 1;
		}

		if (tc != 1 || bc != 1 || (e->scoreboard && e->scoreboard->skin))
		{
			int			inwidth, inheight;
			int			tinwidth, tinheight;
			char *skinname;
			qbyte	*original;
			galiascolourmapped_t *cm;
			char hashname[512];

			if (e->scoreboard && e->scoreboard->skin && !gl_nocolors.value)
			{
				snprintf(hashname, sizeof(hashname), "%s$%s$%i", modelname, e->scoreboard->skin->name, surfnum);
				skinname = hashname;
			}
			else if (surfnum)
			{
				snprintf(hashname, sizeof(hashname), "%s$%i", modelname, surfnum);
				skinname = hashname;
			}
			else
				skinname = modelname;

			if (!skincolourmapped.numbuckets)
			{
				void *buckets = BZ_Malloc(Hash_BytesForBuckets(256));
				memset(buckets, 0, Hash_BytesForBuckets(256));
				Hash_InitTable(&skincolourmapped, 256, buckets);
			}

			for (cm = Hash_Get(&skincolourmapped, skinname); cm; cm = Hash_GetNext(&skincolourmapped, skinname, cm))
			{
				if (cm->tcolour == tc && cm->bcolour == bc && cm->skinnum == e->skinnum)
				{
					return &cm->texnum;
				}
			}

			if (!inf->numskins)
			{
				skins = NULL;
				texnums = NULL;
			}
			else
			{
				skins = (galiasskin_t*)((char *)inf + inf->ofsskins);
				if (!skins->texnums)
				{
					skins = NULL;
					texnums = NULL;
				}
				else
				{
					if (e->skinnum >= 0 && e->skinnum < inf->numskins)
						skins += e->skinnum;
					texnums = (galiastexnum_t*)((char *)skins + skins->ofstexnums);
				}
			}

			//colourmap isn't present yet.
			cm = BZ_Malloc(sizeof(*cm));
			Q_strncpyz(cm->name, skinname, sizeof(cm->name));
			Hash_Add(&skincolourmapped, cm->name, cm, &cm->bucket);
			cm->tcolour = tc;
			cm->bcolour = bc;
			cm->skinnum = e->skinnum;
			cm->texnum.fullbright = 0;
			cm->texnum.base = 0;
#ifdef Q3SHADERS
			cm->texnum.shader = NULL;
#endif

			if (!texnums)
			{	//load just the skin (q2)
/*				if (e->scoreboard && e->scoreboard->skin)
				{
					if (cls.protocol == CP_QUAKE2)
					{
						original = Skin_Cache32(e->scoreboard->skin);
						if (original)
						{
							inwidth = e->scoreboard->skin->width;
							inheight = e->scoreboard->skin->height;
							cm->texnum.base = cm->texnum.fullbright = GL_LoadTexture32(e->scoreboard->skin->name, inwidth, inheight, (unsigned int*)original, true, false);
							return &cm->texnum;
						}
					}
					else
					{
						original = Skin_Cache8(e->scoreboard->skin);
						if (original)
						{
							inwidth = e->scoreboard->skin->width;
							inheight = e->scoreboard->skin->height;
							cm->texnum.base = cm->texnum.fullbright = GL_LoadTexture(e->scoreboard->skin->name, inwidth, inheight, original, true, false);
							return &cm->texnum;
						}
					}
				
					cm->texnum.base = Mod_LoadHiResTexture(e->scoreboard->skin->name, "skins", true, false, true);
					return &cm->texnum;
				}
*/
				return NULL;
			}

			cm->texnum.bump = texnums[cm->skinnum].bump;	//can't colour bumpmapping
			if (cls.protocol != CP_QUAKE2 && ((!texnums || !strcmp(modelname, "progs/player.mdl")) && e->scoreboard && e->scoreboard->skin))
			{
				original = Skin_Cache8(e->scoreboard->skin);
				inwidth = e->scoreboard->skin->width;
				inheight = e->scoreboard->skin->height;
			}
			else
			{
				original = NULL;
				inwidth = 0;
				inheight = 0;
			}
			if (!original)
			{
				if (skins->ofstexels)
				{
					original = (qbyte *)skins + skins->ofstexels;
					inwidth = skins->skinwidth;
					inheight = skins->skinheight;
				}
				else
				{
					original = NULL;
					inwidth = 0;
					inheight = 0;
				}
			}
			tinwidth = skins->skinwidth;
			tinheight = skins->skinheight;
			if (original)
			{
				int i, j;
				qbyte	translate[256];
				unsigned translate32[256];
				static unsigned	pixels[512*512];
				unsigned	*out;
				unsigned	frac, fracstep;

				unsigned	scaled_width, scaled_height;
				qbyte		*inrow;

				texnums = &cm->texnum;

				texnums->base = 0;
				texnums->fullbright = 0;

				if (gl_max_size.value <= 0)
					gl_max_size.value = 512;

				scaled_width = gl_max_size.value < 512 ? gl_max_size.value : 512;
				scaled_height = gl_max_size.value < 512 ? gl_max_size.value : 512;

				for (i=0 ; i<256 ; i++)
					translate[i] = i;

				tc<<=4;
				bc<<=4;

				for (i=0 ; i<16 ; i++)
				{
					if (tc < 128)	// the artists made some backwards ranges.  sigh.
						translate[TOP_RANGE+i] = tc+i;
					else
						translate[TOP_RANGE+i] = tc+15-i;

					if (bc < 128)
						translate[BOTTOM_RANGE+i] = bc+i;
					else
						translate[BOTTOM_RANGE+i] = bc+15-i;
				}


				for (i=0 ; i<256 ; i++)
					translate32[i] = d_8to24rgbtable[translate[i]];

				out = pixels;
				fracstep = tinwidth*0x10000/scaled_width;
				for (i=0 ; i<scaled_height ; i++, out += scaled_width)
				{
					inrow = original + inwidth*(i*inheight/scaled_height);
					frac = fracstep >> 1;
					for (j=0 ; j<scaled_width ; j+=4)
					{
						out[j] = translate32[inrow[frac>>16]] | 0xff000000;
						frac += fracstep;
						out[j+1] = translate32[inrow[frac>>16]] | 0xff000000;
						frac += fracstep;
						out[j+2] = translate32[inrow[frac>>16]] | 0xff000000;
						frac += fracstep;
						out[j+3] = translate32[inrow[frac>>16]] | 0xff000000;
						frac += fracstep;
					}
				}
				texnums->base = D3D7_LoadTexture_32 ("", pixels, scaled_width, scaled_height, 0);
/*				texnums->base = texture_extension_number++;
				GL_Bind(texnums->base);
				qglTexImage2D (GL_TEXTURE_2D, 0, gl_solid_format, scaled_width, scaled_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);

				qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
				qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
*/

				//now do the fullbrights.
				out = pixels;
				fracstep = tinwidth*0x10000/scaled_width;
				for (i=0 ; i<scaled_height ; i++, out += scaled_width)
				{
					inrow = original + inwidth*(i*inheight/scaled_height);
					frac = fracstep >> 1;
					for (j=0 ; j<scaled_width ; j+=1)
					{
						if (inrow[frac>>16] < 255-vid.fullbright)
							((char *) (&out[j]))[3] = 0;	//alpha 0
						frac += fracstep;
					}
				}
/*				texnums->fullbright = texture_extension_number++;
				GL_Bind(texnums->fullbright);
				qglTexImage2D (GL_TEXTURE_2D, 0, gl_alpha_format, scaled_width, scaled_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);

				qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
				qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
*/
			}
			else
			{
				skins = (galiasskin_t*)((char *)inf + inf->ofsskins);
				if (e->skinnum >= 0 && e->skinnum < inf->numskins)
					skins += e->skinnum;

				if (!inf->numskins || !skins->texnums)
					return NULL;

				frame = cl.time*skins->skinspeed;
				frame = frame%skins->texnums;
				texnums = (galiastexnum_t*)((char *)skins + skins->ofstexnums + frame*sizeof(galiastexnum_t));
				memcpy(&cm->texnum, texnums, sizeof(cm->texnum));
			}
			return &cm->texnum;
		}
	}

	if (!inf->numskins)
		return NULL;

	skins = (galiasskin_t*)((char *)inf + inf->ofsskins);
	if (e->skinnum >= 0 && e->skinnum < inf->numskins)
		skins += e->skinnum;
	else
	{
		Con_DPrintf("Skin number out of range\n");
		if (!inf->numskins)
			return NULL;
	}

	if (!skins->texnums)
		return NULL;

	frame = cl.time*skins->skinspeed;
	frame = frame%skins->texnums;
	texnums = (galiastexnum_t*)((char *)skins + skins->ofstexnums + frame*sizeof(galiastexnum_t));

	return texnums;
}


extern vec3_t shadevector;
extern vec3_t ambientlight;
extern vec3_t shadelight;

static void LotsOfLightDirectionHacks(entity_t *e, model_t *m, vec3_t lightaxis[3])
{
	int i;
	vec3_t dist;
	float add;
	qboolean nolightdir;
	vec3_t lightdir;


	if (!(r_refdef.flags & Q2RDF_NOWORLDMODEL))
	{
		if (e->flags & Q2RF_WEAPONMODEL)
			cl.worldmodel->funcs.LightPointValues(cl.worldmodel, r_refdef.vieworg, shadelight, ambientlight, lightdir);
		else
			cl.worldmodel->funcs.LightPointValues(cl.worldmodel, e->origin, shadelight, ambientlight, lightdir);
	}
	else
	{
		ambientlight[0] = ambientlight[1] = ambientlight[2] = shadelight[0] = shadelight[1] = shadelight[2] = 255;
		lightdir[0] = 0;
		lightdir[1] = 1;
		lightdir[2] = 1;
	}

	if (!r_vertexdlights.value)
	{
		for (i=0 ; i<dlights_running ; i++)
		{
			if (cl_dlights[i].radius)
			{
				VectorSubtract (e->origin,
								cl_dlights[i].origin,
								dist);
				add = cl_dlights[i].radius - Length(dist);

				if (add > 0) {
					add*=5;
					ambientlight[0] += add * cl_dlights[i].color[0];
					ambientlight[1] += add * cl_dlights[i].color[1];
					ambientlight[2] += add * cl_dlights[i].color[2];
					//ZOID models should be affected by dlights as well
					shadelight[0] += add * cl_dlights[i].color[0];
					shadelight[1] += add * cl_dlights[i].color[1];
					shadelight[2] += add * cl_dlights[i].color[2];
				}
			}
		}
	}
	else
	{
	}

	for (i = 0; i < 3; i++)	//clamp light so it doesn't get vulgar.
	{
		if (ambientlight[i] > 128)
			ambientlight[i] = 128;
		if (ambientlight[i] + shadelight[i] > 192)
			shadelight[i] = 192 - ambientlight[i];
	}

	if (e->flags & Q2RF_WEAPONMODEL)
	{
		for (i = 0; i < 3; i++)
		{
			if (ambientlight[i] < 24)
				ambientlight[i] = shadelight[i] = 24;
		}
	}

//MORE HUGE HACKS! WHEN WILL THEY CEASE!
	// clamp lighting so it doesn't overbright as much
	// ZOID: never allow players to go totally black
	nolightdir = false;
	if (m->engineflags & MDLF_PLAYER)
	{
		float fb = r_fullbrightSkins.value;
		if (fb > cls.allow_fbskins)
			fb = cls.allow_fbskins;
		if (fb < 0)
			fb = 0;
		if (fb)
		{
			extern cvar_t r_fb_models;

			if (fb >= 1 && r_fb_models.value)
			{
				ambientlight[0] = ambientlight[1] = ambientlight[2] = 4096;
				shadelight[0] = shadelight[1] = shadelight[2] = 4096;
				nolightdir = true;
			}
			else
			{
				for (i = 0; i < 3; i++)
				{
					ambientlight[i] = max(ambientlight[i], 8 + fb * 120);
					shadelight[i] = max(shadelight[i], 8 + fb * 120);
				}
			}
		}
		for (i = 0; i < 3; i++)
		{
			if (ambientlight[i] < 8)
				ambientlight[i] = shadelight[i] = 8;
		}
	}
	if (m->engineflags & MDLF_FLAME)
	{
		shadelight[0] = shadelight[1] = shadelight[2] = 4096;
		ambientlight[0] = ambientlight[1] = ambientlight[2] = 4096;
		nolightdir = true;
	}
	else
	{
		for (i = 0; i < 3; i++)
		{
			if (ambientlight[i] > 128)
				ambientlight[i] = 128;

			shadelight[i] /= 200.0/255;
			ambientlight[i] /= 200.0/255;
		}
	}

	if ((e->drawflags & MLS_MASKIN) == MLS_ABSLIGHT)
	{
		shadelight[0] = shadelight[1] = shadelight[2] = e->abslight;
		ambientlight[0] = ambientlight[1] = ambientlight[2] = 0;
	}
	if ((e->drawflags & MLS_MASKIN) == MLS_FULLBRIGHT || (e->flags & Q2RF_FULLBRIGHT))
	{
		shadelight[0] = shadelight[1] = shadelight[2] = 255;
		ambientlight[0] = ambientlight[1] = ambientlight[2] = 0;
		nolightdir = true;
	}

//#define SHOWLIGHTDIR
	{	//lightdir is absolute, shadevector is relative
		shadevector[0] = DotProduct(lightdir, e->axis[0]);
		shadevector[1] = DotProduct(lightdir, e->axis[1]);
		shadevector[2] = DotProduct(lightdir, e->axis[2]);

		if (e->flags & Q2RF_WEAPONMODEL)
		{
			vec3_t temp;
			temp[0] = DotProduct(shadevector, vpn);
			temp[1] = DotProduct(shadevector, vright);
			temp[2] = DotProduct(shadevector, vup);

			VectorCopy(temp, shadevector);
		}

		VectorNormalize(shadevector);

		VectorCopy(shadevector, lightaxis[2]);
		VectorVectors(lightaxis[2], lightaxis[1], lightaxis[0]);
		VectorInverse(lightaxis[1]);
	}

	if (e->flags & Q2RF_GLOW)
	{
		shadelight[0] += sin(cl.time)*0.25;
		shadelight[1] += sin(cl.time)*0.25;
		shadelight[2] += sin(cl.time)*0.25;
	}

	//d3d is bgra
	//ogl is rgba
	//so switch em and use the gl code
	add = shadelight[0];
	shadelight[0] = shadelight[2];
	shadelight[2] = add;

	add = ambientlight[0];
	ambientlight[0] = ambientlight[2];
	ambientlight[2] = add;
}


qboolean R_GAliasBuildMesh(mesh_t *mesh, galiasinfo_t *inf, int frame1, int frame2, float lerp, float alpha, float fg1time, float fg2time, qboolean nolightdir);
//draws currententity
void D3D7_DrawAliasModel(void)
{
	mesh_t mesh;
	extern entity_t *currententity;
	entity_t *e = currententity;
	galiasinfo_t *inf;
	model_t *m;
	galiastexnum_t *skin;
	int i;

	if (r_secondaryview && e->flags & Q2RF_WEAPONMODEL)
		return;

	{
		extern int cl_playerindex;
		if (e->scoreboard && e->model == cl.model_precache[cl_playerindex])
		{
			m = e->scoreboard->model;
			if (!m || m->type != mod_alias)
				m = e->model;
		}
		else
			m = e->model;
	}

	if (!(e->flags & Q2RF_WEAPONMODEL))
		if (R_CullEntityBox (e, m->mins, m->maxs))
			return;


	inf = GLMod_Extradata (m);

	if (!inf)
		return;


	LotsOfLightDirectionHacks(e, m, mesh.lightaxis);


	{
		float matrix[16];

		if (e->flags & Q2RF_WEAPONMODEL && r_refdef.currentplayernum>=0)
		{	//view weapons need to be rotated onto the screen first
			float view[16];
			float ent[16];
			Matrix4_ModelMatrixFromAxis(view, cl.viewent[r_refdef.currentplayernum].axis[0], cl.viewent[r_refdef.currentplayernum].axis[1], cl.viewent[r_refdef.currentplayernum].axis[2], cl.viewent[r_refdef.currentplayernum].origin);
			Matrix4_ModelMatrixFromAxis(ent, e->axis[0], e->axis[1], e->axis[2], e->origin);
			Matrix4_Multiply(view, ent, matrix);
		}
		else
		{
			Matrix4_ModelMatrixFromAxis(matrix, e->axis[0], e->axis[1], e->axis[2], e->origin);
		}
		pD3DDev->lpVtbl->SetTransform(pD3DDev, D3DTRANSFORMSTATE_WORLD, (D3DMATRIX*)matrix);
	}

pD3DDev->lpVtbl->SetTextureStageState(pD3DDev, 0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
pD3DDev->lpVtbl->SetTextureStageState(pD3DDev, 0, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
pD3DDev->lpVtbl->SetTextureStageState(pD3DDev, 0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);

if (e->flags & Q2RF_DEPTHHACK)
{	//apply the depth hack to stop things from poking into walls.
	//(basically moving it closer to the screen)
	D3DVIEWPORT7 viewport;
	pD3DDev->lpVtbl->GetViewport(pD3DDev, &viewport);
	viewport.dvMinZ = 0;
	viewport.dvMaxZ = 0.3;
	pD3DDev->lpVtbl->SetViewport(pD3DDev, &viewport);
}

	for(i = 0;; i++)
	{
		Alias_GAliasBuildMesh(&mesh, inf, e, e->shaderRGBAf[3], false);

		skin = D3D7_ChooseSkin(inf, m->name, e->skinnum, e);
		if (!skin)
			pD3DDev->lpVtbl->SetTexture(pD3DDev, 0, NULL);
		else
			pD3DDev->lpVtbl->SetTexture(pD3DDev, 0, skin->base);
		D3D_DrawMesh(&mesh);

		if (inf->nextsurf == 0)
			break;
		inf = (galiasinfo_t*)((char*)inf + inf->nextsurf);
	}

if (e->flags & Q2RF_DEPTHHACK)
{
	D3DVIEWPORT7 viewport;
	pD3DDev->lpVtbl->GetViewport(pD3DDev, &viewport);
	viewport.dvMinZ = 0;
	viewport.dvMaxZ = 1;
	pD3DDev->lpVtbl->SetViewport(pD3DDev, &viewport);
}

	{
		float matrix[16];
		Matrix4_Identity(matrix);
		pD3DDev->lpVtbl->SetTransform(pD3DDev, D3DTRANSFORMSTATE_WORLD, (D3DMATRIX*)matrix);
	}
}
#endif
