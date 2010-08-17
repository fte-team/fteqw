
//a note about dedicated servers:
//In the server-side gamecode, a couple of q1 extensions require knowing something about models.
//So we load models serverside, if required.

//things we need:
//tag/bone names and indexes so we can have reasonable modding with tags. :)
//tag/bone positions so we can shoot from the actual gun or other funky stuff
//vertex positions so we can trace against the mesh rather than the bbox.

//we use the gl renderer's model code because it supports more sorts of models than the sw renderer. Sad but true.




#include "quakedef.h"
#ifdef GLQUAKE
	#include "glquake.h"
#endif
#if defined(GLQUAKE) || defined(D3DQUAKE)

#ifdef _WIN32
	#include <malloc.h>
#else
	#include <alloca.h>
#endif

#define MAX_BONES 256

#include "com_mesh.h"

//FIXME
typedef struct
{
	float		scale[3];	// multiply qbyte verts by this
	float		translate[3];	// then add this
	char		name[16];	// frame name from grabbing
	dtrivertx_t	verts[1];	// variable sized
} dmd2aliasframe_t;



// entity_state_t->renderfx flags
#define	Q2RF_MINLIGHT			1		// always have some light (viewmodel)
#define	Q2RF_VIEWERMODEL		2		// don't draw through eyes, only mirrors
#define	Q2RF_WEAPONMODEL		4		// only draw through eyes
#define	Q2RF_FULLBRIGHT			8		// always draw full intensity
#define	Q2RF_DEPTHHACK			16		// for view weapon Z crunching
#define	Q2RF_TRANSLUCENT		32
#define	Q2RF_FRAMELERP			64
#define Q2RF_BEAM				128
#define	Q2RF_CUSTOMSKIN			256		// skin is an index in image_precache
#define	Q2RF_GLOW				512		// pulse lighting for bonus items
#define Q2RF_SHELL_RED			1024
#define	Q2RF_SHELL_GREEN		2048
#define Q2RF_SHELL_BLUE			4096

//ROGUE
#define Q2RF_IR_VISIBLE			0x00008000		// 32768
#define	Q2RF_SHELL_DOUBLE		0x00010000		// 65536
#define	Q2RF_SHELL_HALF_DAM		0x00020000
#define Q2RF_USE_DISGUISE		0x00040000
//ROGUE




extern cvar_t gl_part_flame, r_fullbrightSkins, r_fb_models;
extern cvar_t r_noaliasshadows;
void R_TorchEffect (vec3_t pos, int type);
void GLMod_FloodFillSkin( qbyte *skin, int skinwidth, int skinheight );



extern char	loadname[32];	// for hunk tags


int numTempColours;
byte_vec4_t *tempColours;

int numTempVertexCoords;
vec3_t *tempVertexCoords;

int numTempNormals;
vec3_t *tempNormals;

extern cvar_t gl_ati_truform;
extern cvar_t r_vertexdlights;
extern cvar_t mod_md3flags;
extern cvar_t r_skin_overlays;

#ifndef SERVERONLY
static hashtable_t skincolourmapped;
extern avec3_t shadevector, shadelight, ambientlight; 

//changes vertex lighting values
#if 0
static void R_GAliasApplyLighting(mesh_t *mesh, vec3_t org, vec3_t angles, float *colormod)
{
	int l, v;
	vec3_t rel;
	vec3_t dir;
	float dot, d, a;

	if (mesh->colors4f_array)
	{
		float l;
		int temp;
		int i;
		avec4_t *colours = mesh->colors4f_array;
		vec3_t *normals = mesh->normals_array;
		vec3_t ambient, shade;
		qbyte alphab = bound(0, colormod[3], 1);
		if (!mesh->normals_array)
		{
			mesh->colors4f_array = NULL;
			return;
		}

		VectorCopy(ambientlight, ambient);
		VectorCopy(shadelight, shade);

		for (i = 0; i < 3; i++)
		{
			ambient[i] *= colormod[i];
			shade[i] *= colormod[i];
		}


		for (i = mesh->numvertexes-1; i >= 0; i--)
		{
			l = DotProduct(normals[i], shadevector);

			temp = l*ambient[0]+shade[0];
			colours[i][0] = temp;

			temp = l*ambient[1]+shade[1];
			colours[i][1] = temp;

			temp = l*ambient[2]+shade[2];
			colours[i][2] = temp;

			colours[i][3] = alphab;
		}
	}

	if (r_vertexdlights.value && mesh->colors4f_array)
	{
		//don't include world lights
		for (l=rtlights_first ; l<RTL_FIRST; l++)
		{
			if (cl_dlights[l].radius)
			{
				VectorSubtract (cl_dlights[l].origin,
								org,
								dir);
				if (Length(dir)>cl_dlights[l].radius+mesh->radius)	//far out man!
					continue;

				rel[0] = -DotProduct(dir, currententity->axis[0]);
				rel[1] = -DotProduct(dir, currententity->axis[1]);	//quake's crazy.
				rel[2] = -DotProduct(dir, currententity->axis[2]);
	/*
				glBegin(GL_LINES);
				glVertex3f(0,0,0);
				glVertex3f(rel[0],rel[1],rel[2]);
				glEnd();
	*/
				for (v = 0; v < mesh->numvertexes; v++)
				{
					VectorSubtract(mesh->xyz_array[v], rel, dir);
					dot = DotProduct(dir, mesh->normals_array[v]);
					if (dot>0)
					{
						d = DotProduct(dir, dir);
						a = 1/d;
						if (a>0)
						{
							a *= 10000000*dot/sqrt(d);
							mesh->colors4f_array[v][0] += a*cl_dlights[l].color[0];
							mesh->colors4f_array[v][1] += a*cl_dlights[l].color[1];
							mesh->colors4f_array[v][2] += a*cl_dlights[l].color[2];
						}
	//					else
	//						mesh->colors4f_array[v][1] = 1;
					}
	//				else
	//					mesh->colors4f_array[v][2] = 1;
				}
			}
		}
	}
}
#endif

void GL_GAliasFlushSkinCache(void)
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

static texnums_t *GL_ChooseSkin(galiasinfo_t *inf, char *modelname, int surfnum, entity_t *e)
{
	galiasskin_t *skins;
	texnums_t *texnums;
	int frame;
	unsigned int subframe;

	unsigned int tc, bc;
	qboolean forced;

	if (e->skinnum >= 100 && e->skinnum < 110)
	{
		shader_t *s;
		s = R_RegisterSkin(va("gfx/skin%d.lmp", e->skinnum));
		if (!TEXVALID(s->defaulttextures.base))
			s->defaulttextures.base = R_LoadHiResTexture(va("gfx/skin%d.lmp", e->skinnum), NULL, 0);
		s->defaulttextures.shader = s;
		return &s->defaulttextures;
	}


	if ((e->model->engineflags & MDLF_NOTREPLACEMENTS) && !ruleset_allow_sensative_texture_replacements.ival)
		forced = true;
	else
		forced = false;

	if (!gl_nocolors.ival || forced)
	{
		if (e->scoreboard)
		{
			if (!e->scoreboard->skin)
				Skin_Find(e->scoreboard);
			tc = e->scoreboard->ttopcolor;
			bc = e->scoreboard->tbottomcolor;
		}
		else
		{
			tc = 1;
			bc = 1;
		}

		if (forced || tc != 1 || bc != 1 || (e->scoreboard && e->scoreboard->skin))
		{
			int			inwidth, inheight;
			int			tinwidth, tinheight;
			char *skinname;
			qbyte	*original;
			galiascolourmapped_t *cm;
			char hashname[512];

//			if (e->scoreboard->skin->cachedbpp 

	/*		if (cls.protocol == CP_QUAKE2)
			{
				if (e->scoreboard && e->scoreboard->skin)
					snprintf(hashname, sizeof(hashname), "%s$%s$%i", modelname, e->scoreboard->skin->name, surfnum);
				else
					snprintf(hashname, sizeof(hashname), "%s$%i", modelname, surfnum);
				skinname = hashname;
			}
			else */
			{
				if (e->scoreboard && e->scoreboard->skin)
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
			}

			if (!skincolourmapped.numbuckets)
			{
				void *buckets = BZ_Malloc(Hash_BytesForBuckets(256));
				memset(buckets, 0, Hash_BytesForBuckets(256));
				Hash_InitTable(&skincolourmapped, 256, buckets);
			}

			if (!inf->numskins)
			{
				skins = NULL;
				subframe = 0;
				texnums = NULL;
			}
			else
			{
				skins = (galiasskin_t*)((char *)inf + inf->ofsskins);
				if (!skins->texnums)
				{
					skins = NULL;
					subframe = 0;
					texnums = NULL;
				}
				else
				{
					if (e->skinnum >= 0 && e->skinnum < inf->numskins)
						skins += e->skinnum;

					subframe = cl.time*skins->skinspeed;
					subframe = subframe%skins->texnums;

					texnums = (texnums_t*)((char *)skins + skins->ofstexnums + subframe*sizeof(texnums_t));
				}
			}

			for (cm = Hash_Get(&skincolourmapped, skinname); cm; cm = Hash_GetNext(&skincolourmapped, skinname, cm))
			{
				if (cm->tcolour == tc && cm->bcolour == bc && cm->skinnum == e->skinnum && cm->subframe == subframe)
				{
					return &cm->texnum;
				}
			}

			//colourmap isn't present yet.
			cm = BZ_Malloc(sizeof(*cm));
			Q_strncpyz(cm->name, skinname, sizeof(cm->name));
			Hash_Add(&skincolourmapped, cm->name, cm, &cm->bucket);
			cm->tcolour = tc;
			cm->bcolour = bc;
			cm->skinnum = e->skinnum;
			cm->subframe = subframe;
			cm->texnum.fullbright = r_nulltex;
			cm->texnum.base = r_nulltex;
			cm->texnum.loweroverlay = r_nulltex;
			cm->texnum.upperoverlay = r_nulltex;
			cm->texnum.shader = texnums?texnums->shader:R_RegisterSkin(skinname);

			if (!texnums)
			{	//load just the skin
				if (e->scoreboard && e->scoreboard->skin)
				{
					if (cls.protocol == CP_QUAKE2)
					{
						original = Skin_Cache32(e->scoreboard->skin);
						if (original)
						{
							inwidth = e->scoreboard->skin->width;
							inheight = e->scoreboard->skin->height;
							cm->texnum.base = R_LoadTexture32(e->scoreboard->skin->name, inwidth, inheight, (unsigned int*)original, IF_NOALPHA|IF_NOGAMMA);
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
							cm->texnum.base = R_LoadTexture8(e->scoreboard->skin->name, inwidth, inheight, original, IF_NOALPHA|IF_NOGAMMA, 1);
							return &cm->texnum;
						}
					}

					if (TEXVALID(e->scoreboard->skin->tex_base))
					{
						texnums = &cm->texnum;
						texnums->loweroverlay = e->scoreboard->skin->tex_lower;
						texnums->upperoverlay = e->scoreboard->skin->tex_upper;
						texnums->base = e->scoreboard->skin->tex_base;
						return texnums;
					}
				
					cm->texnum.base = R_LoadHiResTexture(e->scoreboard->skin->name, "skins", IF_NOALPHA);
					return &cm->texnum;
				}
				return NULL;
			}

			cm->texnum.bump = texnums[cm->skinnum].bump;	//can't colour bumpmapping
			if (cls.protocol != CP_QUAKE2 && ((!texnums || !strcmp(modelname, "progs/player.mdl")) && e->scoreboard && e->scoreboard->skin))
			{
				original = Skin_Cache8(e->scoreboard->skin);
				inwidth = e->scoreboard->skin->width;
				inheight = e->scoreboard->skin->height;

				if (!original && TEXVALID(e->scoreboard->skin->tex_base))
				{
					texnums = &cm->texnum;
					texnums->loweroverlay = e->scoreboard->skin->tex_lower;
					texnums->upperoverlay = e->scoreboard->skin->tex_upper;
					texnums->base = e->scoreboard->skin->tex_base;
					return texnums;
				}
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
				unsigned translate32[256];
				static unsigned	pixels[512*512];
				unsigned	*out;
				unsigned	frac, fracstep;

				unsigned	scaled_width, scaled_height;
				qbyte		*inrow;

				texnums = &cm->texnum;

				texnums->base = r_nulltex;
				texnums->fullbright = r_nulltex;

				scaled_width = gl_max_size.value < 512 ? gl_max_size.value : 512;
				scaled_height = gl_max_size.value < 512 ? gl_max_size.value : 512;

				//handle the case of an external skin being smaller than the texture that its meant to replace
				//(to support the evil hackage of the padding on the outside of common qw skins)
				if (tinwidth > inwidth)
					tinwidth = inwidth;
				if (tinheight > inheight)
					tinheight = inheight;

				//don't make scaled width any larger than it needs to be
				for (i = 0; i < 10; i++)
				{
					scaled_width = (1<<i);
					if (scaled_width >= tinwidth)
						break;	//its covered
				}
				if (scaled_width > gl_max_size.value)
					scaled_width = gl_max_size.value;	//whoops, we made it too big

				for (i = 0; i < 10; i++)
				{
					scaled_height = (1<<i);
					if (scaled_height >= tinheight)
						break;	//its covered
				}
				if (scaled_height > gl_max_size.value)
					scaled_height = gl_max_size.value;	//whoops, we made it too big

				{
					for (i=0 ; i<256 ; i++)
						translate32[i] = d_8to24rgbtable[i];

					for (i = 0; i < 16; i++)
					{
						if (tc >= 16)
						{
							//assumption: row 0 is pure white.
							*((unsigned char*)&translate32[TOP_RANGE+i]+0) = (((tc&0xff0000)>>16)**((unsigned char*)&d_8to24rgbtable[i]+0))>>8;
							*((unsigned char*)&translate32[TOP_RANGE+i]+1) = (((tc&0x00ff00)>> 8)**((unsigned char*)&d_8to24rgbtable[i]+1))>>8;
							*((unsigned char*)&translate32[TOP_RANGE+i]+2) = (((tc&0x0000ff)>> 0)**((unsigned char*)&d_8to24rgbtable[i]+2))>>8;
							*((unsigned char*)&translate32[TOP_RANGE+i]+3) = 0xff;
						}
						else
						{
							if (tc < 8)
								translate32[TOP_RANGE+i] = d_8to24rgbtable[(tc<<4)+i];
							else
								translate32[TOP_RANGE+i] = d_8to24rgbtable[(tc<<4)+15-i];
						}
						if (bc >= 16)
						{
							*((unsigned char*)&translate32[BOTTOM_RANGE+i]+0) = (((bc&0xff0000)>>16)**((unsigned char*)&d_8to24rgbtable[i]+0))>>8;
							*((unsigned char*)&translate32[BOTTOM_RANGE+i]+1) = (((bc&0x00ff00)>> 8)**((unsigned char*)&d_8to24rgbtable[i]+1))>>8;
							*((unsigned char*)&translate32[BOTTOM_RANGE+i]+2) = (((bc&0x0000ff)>> 0)**((unsigned char*)&d_8to24rgbtable[i]+2))>>8;
							*((unsigned char*)&translate32[BOTTOM_RANGE+i]+3) = 0xff;
						}
						else
						{
							if (bc < 8)
								translate32[BOTTOM_RANGE+i] = d_8to24rgbtable[(bc<<4)+i];
							else
								translate32[BOTTOM_RANGE+i] = d_8to24rgbtable[(bc<<4)+15-i];
						}
					}
				}

				out = pixels;
				fracstep = tinwidth*0x10000/scaled_width;
				for (i=0 ; i<scaled_height ; i++, out += scaled_width)
				{
					inrow = original + inwidth*(i*inheight/scaled_height);
					frac = fracstep >> 1;
					for (j=0 ; j<scaled_width ; j+=4)
					{
						out[j] = translate32[inrow[frac>>16]];
						frac += fracstep;
						out[j+1] = translate32[inrow[frac>>16]];
						frac += fracstep;
						out[j+2] = translate32[inrow[frac>>16]];
						frac += fracstep;
						out[j+3] = translate32[inrow[frac>>16]];
						frac += fracstep;
					}
				}
				texnums->base = R_AllocNewTexture(scaled_width, scaled_height);
				R_Upload(texnums->base, "", TF_RGBX32, pixels, NULL, scaled_width, scaled_height, IF_NOMIPMAP);

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
				texnums->fullbright = R_AllocNewTexture(scaled_width, scaled_height);
				R_Upload(texnums->fullbright, "", TF_RGBA32, pixels, NULL, scaled_width, scaled_height, IF_NOMIPMAP);
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
				texnums = (texnums_t*)((char *)skins + skins->ofstexnums + frame*sizeof(texnums_t));
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
	texnums = (texnums_t*)((char *)skins + skins->ofstexnums + frame*sizeof(texnums_t));

	return texnums;
}

#if defined(RTLIGHTS) && defined(GLQUAKE)
static int numFacing;
static qbyte *triangleFacing;
static void R_CalcFacing(mesh_t *mesh, vec3_t lightpos)
{
	float *v1, *v2, *v3;
	vec3_t d1, d2, norm;

	int i;

	index_t *indexes = mesh->indexes;
	int numtris = mesh->numindexes/3;


	if (numFacing < numtris)
	{
		if (triangleFacing)
			BZ_Free(triangleFacing);
		triangleFacing = BZ_Malloc(sizeof(*triangleFacing)*numtris);
		numFacing = numtris;
	}

	for (i = 0; i < numtris; i++, indexes+=3)
	{
		v1 = (float *)(mesh->xyz_array + indexes[0]);
		v2 = (float *)(mesh->xyz_array + indexes[1]);
		v3 = (float *)(mesh->xyz_array + indexes[2]);

		VectorSubtract(v1, v2, d1);
		VectorSubtract(v3, v2, d2);
		CrossProduct(d1, d2, norm);

		triangleFacing[i] = (( lightpos[0] - v1[0] ) * norm[0] + ( lightpos[1] - v1[1] ) * norm[1] + ( lightpos[2] - v1[2] ) * norm[2]) > 0;
	}
}

#define PROJECTION_DISTANCE 30000
static int numProjectedShadowVerts;
static vec3_t *ProjectedShadowVerts;
static void R_ProjectShadowVolume(mesh_t *mesh, vec3_t lightpos)
{
	int numverts = mesh->numvertexes;
	int i;
	vecV_t *input = mesh->xyz_array;
	vec3_t *projected;
	if (numProjectedShadowVerts < numverts)
	{
		if (ProjectedShadowVerts)
			BZ_Free(ProjectedShadowVerts);
		ProjectedShadowVerts = BZ_Malloc(sizeof(*ProjectedShadowVerts)*numverts);
		numProjectedShadowVerts = numverts;
	}
	projected = ProjectedShadowVerts;
	for (i = 0; i < numverts; i++)
	{
		projected[i][0] = input[i][0] + (input[i][0]-lightpos[0])*PROJECTION_DISTANCE;
		projected[i][1] = input[i][1] + (input[i][1]-lightpos[1])*PROJECTION_DISTANCE;
		projected[i][2] = input[i][2] + (input[i][2]-lightpos[2])*PROJECTION_DISTANCE;
	}
}

static void R_DrawShadowVolume(mesh_t *mesh)
{
	int t;
	vec3_t *proj = ProjectedShadowVerts;
	vecV_t *verts = mesh->xyz_array;
	index_t *indexes = mesh->indexes;
	int *neighbours = mesh->trneighbors;
	int numtris = mesh->numindexes/3;

	qglBegin(GL_TRIANGLES);
	for (t = 0; t < numtris; t++)
	{
		if (triangleFacing[t])
		{
			//draw front
			qglVertex3fv(verts[indexes[t*3+0]]);
			qglVertex3fv(verts[indexes[t*3+1]]);
			qglVertex3fv(verts[indexes[t*3+2]]);

			//draw back
			qglVertex3fv(proj[indexes[t*3+1]]);
			qglVertex3fv(proj[indexes[t*3+0]]);
			qglVertex3fv(proj[indexes[t*3+2]]);

			//draw side caps
			if (neighbours[t*3+0] < 0 || !triangleFacing[neighbours[t*3+0]])
			{
				qglVertex3fv(verts[indexes[t*3+1]]);
				qglVertex3fv(verts[indexes[t*3+0]]);
				qglVertex3fv(proj [indexes[t*3+0]]);
				qglVertex3fv(verts[indexes[t*3+1]]);
				qglVertex3fv(proj [indexes[t*3+0]]);
				qglVertex3fv(proj [indexes[t*3+1]]);
			}

			if (neighbours[t*3+1] < 0 || !triangleFacing[neighbours[t*3+1]])
			{
				qglVertex3fv(verts[indexes[t*3+2]]);
				qglVertex3fv(verts[indexes[t*3+1]]);
				qglVertex3fv(proj [indexes[t*3+1]]);
				qglVertex3fv(verts[indexes[t*3+2]]);
				qglVertex3fv(proj [indexes[t*3+1]]);
				qglVertex3fv(proj [indexes[t*3+2]]);
			}

			if (neighbours[t*3+2] < 0 || !triangleFacing[neighbours[t*3+2]])
			{
				qglVertex3fv(verts[indexes[t*3+0]]);
				qglVertex3fv(verts[indexes[t*3+2]]);
				qglVertex3fv(proj [indexes[t*3+2]]);
				qglVertex3fv(verts[indexes[t*3+0]]);
				qglVertex3fv(proj [indexes[t*3+2]]);
				qglVertex3fv(proj [indexes[t*3+0]]);
			}
		}
	}
	qglEnd();
}
#endif

//true if no shading is to be used.
static qboolean R_CalcModelLighting(entity_t *e, model_t *clmodel, unsigned int rmode)
{
	vec3_t lightdir;
	int i;
	vec3_t dist;
	float add;

	if (clmodel->engineflags & MDLF_FLAME)
	{
		shadelight[0] = shadelight[1] = shadelight[2] = 4096;
		ambientlight[0] = ambientlight[1] = ambientlight[2] = 4096;
		return true;
	}
	if ((e->drawflags & MLS_MASKIN) == MLS_FULLBRIGHT || (e->flags & Q2RF_FULLBRIGHT))
	{
		shadelight[0] = shadelight[1] = shadelight[2] = 255;
		ambientlight[0] = ambientlight[1] = ambientlight[2] = 0;
		return true;
	}

	//shortcut here, no need to test bsp lights or world lights when there's realtime lighting going on.
	if (rmode == BEM_DEPTHDARK || rmode == BEM_DEPTHONLY)
	{
		shadelight[0] = shadelight[1] = shadelight[2] = 0;
		ambientlight[0] = ambientlight[1] = ambientlight[2] = 0;
		return true;
	}

	if (!(r_refdef.flags & Q2RDF_NOWORLDMODEL))
	{
		if (e->flags & Q2RF_WEAPONMODEL)
		{
			cl.worldmodel->funcs.LightPointValues(cl.worldmodel, r_refdef.vieworg, shadelight, ambientlight, lightdir);
			for (i = 0; i < 3; i++)
			{	/*viewmodels may not be pure black*/
				if (ambientlight[i] < 24)
					ambientlight[i] = 24;
			}
		}
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

	if (!r_vertexdlights.ival && r_dynamic.ival)
	{
		//don't do world lights, although that might be funny
		for (i=rtlights_first; i<RTL_FIRST; i++)
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

	for (i = 0; i < 3; i++)	//clamp light so it doesn't get vulgar.
	{
		if (ambientlight[i] > 128)
			ambientlight[i] = 128;
		if (shadelight[i] > 192)
			shadelight[i] = 192;
	}

//MORE HUGE HACKS! WHEN WILL THEY CEASE!
	// clamp lighting so it doesn't overbright as much
	// ZOID: never allow players to go totally black
	if (clmodel->engineflags & MDLF_PLAYER)
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
				return true;
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


	for (i = 0; i < 3; i++)
	{
		if (ambientlight[i] > 128)
			ambientlight[i] = 128;

		shadelight[i] /= 200.0/255;
		ambientlight[i] /= 200.0/255;
	}

	if ((e->model->flags & EF_ROTATE) && cl.hexen2pickups)
	{
		shadelight[0] = shadelight[1] = shadelight[2] = 
		ambientlight[0] = ambientlight[1] = ambientlight[2] = 128+sin(cl.servertime*4)*64;
	}
	if ((e->drawflags & MLS_MASKIN) == MLS_ABSLIGHT)
	{
		shadelight[0] = shadelight[1] = shadelight[2] = e->abslight;
		ambientlight[0] = ambientlight[1] = ambientlight[2] = e->abslight;
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
			temp[1] = -DotProduct(shadevector, vright);
			temp[2] = DotProduct(shadevector, vup);

			VectorCopy(temp, shadevector);
		}

		VectorNormalize(shadevector);

	}

	shadelight[0] *= 1/255.0f;
	shadelight[1] *= 1/255.0f;
	shadelight[2] *= 1/255.0f;
	ambientlight[0] *= 1/255.0f;
	ambientlight[1] *= 1/255.0f;
	ambientlight[2] *= 1/255.0f;

	if (e->flags & Q2RF_GLOW)
	{
		shadelight[0] += sin(cl.time)*0.25;
		shadelight[1] += sin(cl.time)*0.25;
		shadelight[2] += sin(cl.time)*0.25;
	}
	return false;
}

static shader_t reskinnedmodelshader;
void R_DrawGAliasModel (entity_t *e, unsigned int rmode)
{
	model_t *clmodel;
	galiasinfo_t *inf;
	mesh_t mesh;
	texnums_t *skin;

	vec3_t saveorg;
	int surfnum;
	int bef;

	qboolean needrecolour;
	qboolean nolightdir;

	shader_t *shader;

//	if (e->flags & Q2RF_VIEWERMODEL && e->keynum == cl.playernum[r_refdef.currentplayernum]+1)
//		return;

	if (r_refdef.externalview && e->flags & Q2RF_WEAPONMODEL)
		return;

	{
		extern int cl_playerindex;
		if (e->scoreboard && e->model == cl.model_precache[cl_playerindex])
		{
			clmodel = e->scoreboard->model;
			if (!clmodel || clmodel->type != mod_alias)
				clmodel = e->model;
		}
		else
			clmodel = e->model;
	}

	if (clmodel->tainted)
	{
		if (!ruleset_allow_modified_eyes.ival && !strcmp(clmodel->name, "progs/eyes.mdl"))
			return;
	}

	if (!(e->flags & Q2RF_WEAPONMODEL))
	{
		if (R_CullEntityBox (e, clmodel->mins, clmodel->maxs))
			return;
#ifdef RTLIGHTS
		if (BE_LightCullModel(e->origin, clmodel))
			return;
	}
	else
	{
		if (BE_LightCullModel(r_origin, clmodel))
			return;
#endif
	}

	nolightdir = R_CalcModelLighting(e, clmodel, rmode);

	if (gl_affinemodels.ival)
		qglHint (GL_PERSPECTIVE_CORRECTION_HINT, GL_FASTEST);

	if (e->flags & Q2RF_DEPTHHACK)
		qglDepthRange (gldepthmin, gldepthmin + 0.3*(gldepthmax-gldepthmin));

	bef = BEF_FORCEDEPTHTEST;
	if (e->flags & Q2RF_ADDITIVE)
	{
		bef |= BEF_FORCEADDITIVE;
	}
	else if (e->drawflags & DRF_TRANSLUCENT)	//hexen2
	{
		bef |= BEF_FORCETRANSPARENT;
		e->shaderRGBAf[3] = r_wateralpha.value;
	}
	else if ((e->model->flags & EFH2_SPECIAL_TRANS))	//hexen2 flags.
	{
		//BEFIXME: this needs to generate the right sort of default instead
		//(alpha blend+disable cull)
	}
	else if ((e->model->flags & EFH2_TRANSPARENT))
	{
		//BEFIXME: make sure the shader generator works
	}
	else if ((e->model->flags & EFH2_HOLEY))
	{
		//BEFIXME: this needs to generate the right sort of default instead
		//(alpha test)
	}
	else if (e->shaderRGBAf[3] < 1 && cls.protocol != CP_QUAKE3)
		bef |= BEF_FORCETRANSPARENT;
	BE_SelectMode(rmode, bef);


	qglPushMatrix();
	R_RotateForEntity(e, clmodel);

	inf = RMod_Extradata (clmodel);
	if (qglPNTrianglesfATI && gl_ati_truform.ival)
		qglEnable(GL_PN_TRIANGLES_ATI);

	if (clmodel == cl.model_precache_vwep[0])
	{
		extern int cl_playerindex;
		clmodel = cl.model_precache[cl_playerindex];
	}

	if (e->flags & Q2RF_WEAPONMODEL)
	{
		VectorCopy(currententity->origin, saveorg);
		VectorCopy(r_refdef.vieworg, currententity->origin);
	}

	memset(&mesh, 0, sizeof(mesh));
	for(surfnum=0; inf; ((inf->nextsurf)?(inf = (galiasinfo_t*)((char *)inf + inf->nextsurf)):(inf=NULL)), surfnum++)
	{
		needrecolour = Alias_GAliasBuildMesh(&mesh, inf, e, e->shaderRGBAf[3], nolightdir);

		shader = currententity->forcedshader;
		skin = GL_ChooseSkin(inf, clmodel->name, surfnum, e);

		if (!shader)
		{
			if (skin && skin->shader)
				shader = skin->shader;
			else
			{
				shader = &reskinnedmodelshader;
				skin = &shader->defaulttextures;
				reskinnedmodelshader.numpasses = 1;
				reskinnedmodelshader.passes[0].flags = 0;
				reskinnedmodelshader.passes[0].numMergedPasses = 1;
				reskinnedmodelshader.passes[0].anim_frames[0] = skin->base;
				if (nolightdir || !mesh.normals_array || !mesh.colors4f_array)
				{
					reskinnedmodelshader.passes[0].rgbgen = RGB_GEN_IDENTITY_LIGHTING;
					reskinnedmodelshader.passes[0].flags |= SHADER_PASS_NOCOLORARRAY;
				}
				else
					reskinnedmodelshader.passes[0].rgbgen = RGB_GEN_LIGHTING_DIFFUSE;
				reskinnedmodelshader.passes[0].alphagen = (e->shaderRGBAf[3]<1)?ALPHA_GEN_ENTITY:ALPHA_GEN_IDENTITY;
				reskinnedmodelshader.passes[0].shaderbits |= SBITS_MISC_DEPTHWRITE;
				reskinnedmodelshader.passes[0].blendmode = GL_MODULATE;
				reskinnedmodelshader.passes[0].texgen = T_GEN_DIFFUSE;

				reskinnedmodelshader.flags = SHADER_CULL_FRONT;
			}
		}

		BE_DrawMesh_Single(shader, &mesh, NULL, skin);
	}

	if (e->flags & Q2RF_WEAPONMODEL)
		VectorCopy(saveorg, currententity->origin);

	if (qglPNTrianglesfATI && gl_ati_truform.ival)
		qglDisable(GL_PN_TRIANGLES_ATI);

	qglPopMatrix();

	if (gl_affinemodels.value)
		qglHint (GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);

	if (e->flags & Q2RF_DEPTHHACK)
		qglDepthRange (gldepthmin, gldepthmax);

	BE_SelectMode(rmode, 0);
}

//returns the rotated offset of the two points in result
void RotateLightVector(const vec3_t *axis, const vec3_t origin, const vec3_t lightpoint, vec3_t result)
{
	vec3_t offs;

	offs[0] = lightpoint[0] - origin[0];
	offs[1] = lightpoint[1] - origin[1];
	offs[2] = lightpoint[2] - origin[2];

	result[0] = DotProduct (offs, axis[0]);
	result[1] = DotProduct (offs, axis[1]);
	result[2] = DotProduct (offs, axis[2]);
}

#if defined(RTLIGHTS) && defined(GLQUAKE)
void GL_LightMesh (mesh_t *mesh, vec3_t lightpos, vec3_t colours, float radius)
{
	vec3_t dir;
	int i;
	float dot, d, f, a;

	vecV_t *xyz = mesh->xyz_array;
	vec3_t *normals = mesh->normals_array;
	vec4_t *out = mesh->colors4f_array;

	if (!out)
		return;	//urm..

	if (normals)
	{
		for (i = 0; i < mesh->numvertexes; i++)
		{
			VectorSubtract(lightpos, xyz[i], dir);
			dot = DotProduct(dir, normals[i]);
			if (dot > 0)
			{
				d = DotProduct(dir, dir)/radius;
				a = 1/d;
				if (a>0)
				{
					a *= dot/sqrt(d);
					f = a*colours[0];
					out[i][0] = f;

					f = a*colours[1];
					out[i][1] = f;

					f = a*colours[2];
					out[i][2] = f;
				}
				else
				{
					out[i][0] = 0;
					out[i][1] = 0;
					out[i][2] = 0;
				}
			}
			else
			{
				out[i][0] = 0;
				out[i][1] = 0;
				out[i][2] = 0;
			}
			out[i][3] = 1;
		}
	}
	else
	{
		for (i = 0; i < mesh->numvertexes; i++)
		{
			VectorSubtract(lightpos, xyz[i], dir);
			out[i][0] = colours[0];
			out[i][1] = colours[1];
			out[i][2] = colours[2];
			out[i][3] = 1;
		}
	}
}

//courtesy of DP
void R_BuildBumpVectors(const float *v0, const float *v1, const float *v2, const float *tc0, const float *tc1, const float *tc2, float *svector3f, float *tvector3f, float *normal3f)
{
	float f, tangentcross[3], v10[3], v20[3], tc10[2], tc20[2];
	// 79 add/sub/negate/multiply (1 cycle), 1 compare (3 cycle?), total cycles not counting load/store/exchange roughly 82 cycles
	// 6 add, 28 subtract, 39 multiply, 1 compare, 50% chance of 6 negates

	// 6 multiply, 9 subtract
	VectorSubtract(v1, v0, v10);
	VectorSubtract(v2, v0, v20);
	normal3f[0] = v10[1] * v20[2] - v10[2] * v20[1];
	normal3f[1] = v10[2] * v20[0] - v10[0] * v20[2];
	normal3f[2] = v10[0] * v20[1] - v10[1] * v20[0];
	// 12 multiply, 10 subtract
	tc10[1] = tc1[1] - tc0[1];
	tc20[1] = tc2[1] - tc0[1];
	svector3f[0] = tc10[1] * v20[0] - tc20[1] * v10[0];
	svector3f[1] = tc10[1] * v20[1] - tc20[1] * v10[1];
	svector3f[2] = tc10[1] * v20[2] - tc20[1] * v10[2];
	tc10[0] = tc1[0] - tc0[0];
	tc20[0] = tc2[0] - tc0[0];
	tvector3f[0] = tc10[0] * v20[0] - tc20[0] * v10[0];
	tvector3f[1] = tc10[0] * v20[1] - tc20[0] * v10[1];
	tvector3f[2] = tc10[0] * v20[2] - tc20[0] * v10[2];
	// 12 multiply, 4 add, 6 subtract
	f = DotProduct(svector3f, normal3f);
	svector3f[0] -= f * normal3f[0];
	svector3f[1] -= f * normal3f[1];
	svector3f[2] -= f * normal3f[2];
	f = DotProduct(tvector3f, normal3f);
	tvector3f[0] -= f * normal3f[0];
	tvector3f[1] -= f * normal3f[1];
	tvector3f[2] -= f * normal3f[2];
	// if texture is mapped the wrong way (counterclockwise), the tangents
	// have to be flipped, this is detected by calculating a normal from the
	// two tangents, and seeing if it is opposite the surface normal
	// 9 multiply, 2 add, 3 subtract, 1 compare, 50% chance of: 6 negates
	CrossProduct(tvector3f, svector3f, tangentcross);
	if (DotProduct(tangentcross, normal3f) < 0)
	{
		VectorNegate(svector3f, svector3f);
		VectorNegate(tvector3f, tvector3f);
	}
}

//courtesy of DP
void R_AliasGenerateTextureVectors(mesh_t *mesh, float *normal3f, float *svector3f, float *tvector3f)
{
	int i;
	float sdir[3], tdir[3], normal[3], *v;
	index_t *e;
	float *vertex3f = (float*)mesh->xyz_array;
	float *texcoord2f = (float*)mesh->st_array;
	// clear the vectors
//	if (svector3f)
		memset(svector3f, 0, mesh->numvertexes * sizeof(float[3]));
//	if (tvector3f)
		memset(tvector3f, 0, mesh->numvertexes * sizeof(float[3]));
//	if (normal3f)
		memset(normal3f, 0, mesh->numvertexes * sizeof(float[3]));
	// process each vertex of each triangle and accumulate the results
	for (e = mesh->indexes; e < mesh->indexes+mesh->numindexes; e += 3)
	{
		R_BuildBumpVectors(vertex3f + e[0] * 3, vertex3f + e[1] * 3, vertex3f + e[2] * 3, texcoord2f + e[0] * 2, texcoord2f + e[1] * 2, texcoord2f + e[2] * 2, sdir, tdir, normal);
//		if (!areaweighting)
//		{
//			VectorNormalize(sdir);
//			VectorNormalize(tdir);
//			VectorNormalize(normal);
//		}
//		if (svector3f)
			for (i = 0;i < 3;i++)
				VectorAdd(svector3f + e[i]*3, sdir, svector3f + e[i]*3);
//		if (tvector3f)
			for (i = 0;i < 3;i++)
				VectorAdd(tvector3f + e[i]*3, tdir, tvector3f + e[i]*3);
//		if (normal3f)
			for (i = 0;i < 3;i++)
				VectorAdd(normal3f + e[i]*3, normal, normal3f + e[i]*3);
	}
	// now we could divide the vectors by the number of averaged values on
	// each vertex...  but instead normalize them
	// 4 assignments, 1 divide, 1 sqrt, 2 adds, 6 multiplies
	if (svector3f)
		for (i = 0, v = svector3f;i < mesh->numvertexes;i++, v += 3)
			VectorNormalize(v);
	// 4 assignments, 1 divide, 1 sqrt, 2 adds, 6 multiplies
	if (tvector3f)
		for (i = 0, v = tvector3f;i < mesh->numvertexes;i++, v += 3)
			VectorNormalize(v);
	// 4 assignments, 1 divide, 1 sqrt, 2 adds, 6 multiplies
	if (normal3f)
		for (i = 0, v = normal3f;i < mesh->numvertexes;i++, v += 3)
			VectorNormalize(v);

}


void R_AliasGenerateVertexLightDirs(mesh_t *mesh, vec3_t lightdir, vec3_t *results, vec3_t *normal3f, vec3_t *svector3f, vec3_t *tvector3f)
{
	int i;
	R_AliasGenerateTextureVectors(mesh, (float*)normal3f, (float*)svector3f, (float*)tvector3f);

	for (i = 0; i < mesh->numvertexes; i++)
	{
		results[i][0] = -DotProduct(lightdir, tvector3f[i]);
		results[i][1] = -DotProduct(lightdir, svector3f[i]);
		results[i][2] = -DotProduct(lightdir, normal3f[i]);
	}
}

//FIXME: Be less agressive.
//This function will have to be called twice (for geforce cards), with the same data, so do the building once and rendering twice.
void R_DrawGAliasShadowVolume(entity_t *e, vec3_t lightpos, float radius)
{
	model_t *clmodel = e->model;
	galiasinfo_t *inf;
	mesh_t mesh;
	vec3_t lightorg;

	if (clmodel->engineflags & (MDLF_FLAME | MDLF_BOLT))
		return;
	if (r_noaliasshadows.ival)
		return;

//	if (e->shaderRGBAf[3] < 0.5)
//		return;

	RotateLightVector(e->axis, e->origin, lightpos, lightorg);

	if (Length(lightorg) > radius + clmodel->radius)
		return;

	qglPushMatrix();
	R_RotateForEntity(e, clmodel);


	inf = RMod_Extradata (clmodel);
	while(inf)
	{
		if (inf->ofs_trineighbours)
		{
			Alias_GAliasBuildMesh(&mesh, inf, e, 1, true);
			R_CalcFacing(&mesh, lightorg);
			R_ProjectShadowVolume(&mesh, lightorg);
			R_DrawShadowVolume(&mesh);
		}

		if (inf->nextsurf)
			inf = (galiasinfo_t*)((char *)inf + inf->nextsurf);
		else
			inf = NULL;
	}

	qglPopMatrix();
}
#endif





#if 0
static int R_FindTriangleWithEdge ( index_t *indexes, int numtris, index_t start, index_t end, int ignore)
{
	int i;
	int match, count;

	count = 0;
	match = -1;

	for (i = 0; i < numtris; i++, indexes += 3)
	{
		if ( (indexes[0] == start && indexes[1] == end)
			|| (indexes[1] == start && indexes[2] == end)
			|| (indexes[2] == start && indexes[0] == end) ) {
			if (i != ignore)
				match = i;
			count++;
		} else if ( (indexes[1] == start && indexes[0] == end)
			|| (indexes[2] == start && indexes[1] == end)
			|| (indexes[0] == start && indexes[2] == end) ) {
			count++;
		}
	}

	// detect edges shared by three triangles and make them seams
	if (count > 2)
		match = -1;

	return match;
}
#endif

#if 0
static void R_BuildTriangleNeighbours ( int *neighbours, index_t *indexes, int numtris )
{
	int i, *n;
	index_t *index;

	for (i = 0, index = indexes, n = neighbours; i < numtris; i++, index += 3, n += 3)
	{
		n[0] = R_FindTriangleWithEdge (indexes, numtris, index[1], index[0], i);
		n[1] = R_FindTriangleWithEdge (indexes, numtris, index[2], index[1], i);
		n[2] = R_FindTriangleWithEdge (indexes, numtris, index[0], index[2], i);
	}
}
#endif




#if 0
void GL_GenerateNormals(float *orgs, float *normals, int *indicies, int numtris, int numverts)
{
	vec3_t d1, d2;
	vec3_t norm;
	int t, i, v1, v2, v3;
	int tricounts[MD2MAX_VERTS];
	vec3_t combined[MD2MAX_VERTS];
	int triremap[MD2MAX_VERTS];
	if (numverts > MD2MAX_VERTS)
		return;	//not an issue, you just loose the normals.

	memset(triremap, 0, numverts*sizeof(triremap[0]));

	v2=0;
	for (i = 0; i < numverts; i++)	//weld points
	{
		for (v1 = 0; v1 < v2; v1++)
		{
			if (orgs[i*3+0] == combined[v1][0] &&
				orgs[i*3+1] == combined[v1][1] &&
				orgs[i*3+2] == combined[v1][2])
			{
				triremap[i] = v1;
				break;
			}
		}
		if (v1 == v2)
		{
			combined[v1][0] = orgs[i*3+0];
			combined[v1][1] = orgs[i*3+1];
			combined[v1][2] = orgs[i*3+2];
			v2++;

			triremap[i] = v1;
		}
	}
	memset(tricounts, 0, v2*sizeof(tricounts[0]));
	memset(combined, 0, v2*sizeof(*combined));

	for (t = 0; t < numtris; t++)
	{
		v1 = triremap[indicies[t*3]];
		v2 = triremap[indicies[t*3+1]];
		v3 = triremap[indicies[t*3+2]];

		VectorSubtract((orgs+v2*3), (orgs+v1*3), d1);
		VectorSubtract((orgs+v3*3), (orgs+v1*3), d2);
		CrossProduct(d1, d2, norm);
		VectorNormalize(norm);

		VectorAdd(norm, combined[v1], combined[v1]);
		VectorAdd(norm, combined[v2], combined[v2]);
		VectorAdd(norm, combined[v3], combined[v3]);

		tricounts[v1]++;
		tricounts[v2]++;
		tricounts[v3]++;
	}

	for (i = 0; i < numverts; i++)
	{
		if (tricounts[triremap[i]])
		{
			VectorScale(combined[triremap[i]], 1.0f/tricounts[triremap[i]], normals+i*3);
		}
	}
}
#endif
#endif

#endif	// defined(GLQUAKE)
