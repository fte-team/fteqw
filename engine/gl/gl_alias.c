#include "quakedef.h"
#include "glquake.h"
#include "shader.h"
#include "hash.h"

//FIXME
typedef struct
{
	float		scale[3];	// multiply qbyte verts by this
	float		translate[3];	// then add this
	char		name[16];	// frame name from grabbing
	dtrivertx_t	verts[1];	// variable sized
} dmd2aliasframe_t;



// entity_state_t->renderfx flags
#define	Q2RF_MINLIGHT			1		// allways have some light (viewmodel)
#define	Q2RF_VIEWERMODEL		2		// don't draw through eyes, only mirrors
#define	Q2RF_WEAPONMODEL		4		// only draw through eyes
#define	Q2RF_FULLBRIGHT			8		// allways draw full intensity
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




extern cvar_t gl_part_flame, gl_part_torch, r_fullbrightSkins, r_fb_models;
extern cvar_t r_noaliasshadows;
void R_TorchEffect (vec3_t pos, int type);
void GLMod_FloodFillSkin( qbyte *skin, int skinwidth, int skinheight );



extern char	loadname[32];	// for hunk tags


int numTempColours;
byte_vec4_t *tempColours;

int numTempVertexCoords;
vec4_t *tempVertexCoords;

int numTempNormals;
vec3_t *tempNormals;

extern cvar_t gl_ati_truform;

typedef struct {
	int ofs_indexes;
	int numindexes;

	int ofs_trineighbours;

	int numskins;
	int ofsskins;


	int numverts;

	int ofs_st_array;

	int groups;
	int groupofs;

	int nextsurf;
} galiasinfo_t;

//frame is an index into this
typedef struct {
	int numposes;
	float rate;
	int poseofs;
} galiasgroup_t;

typedef struct {
	int ofsverts;
	int ofsnormals;

	vec3_t		scale;
	vec3_t		scale_origin;
} galiaspose_t;

//we can't be bothered with animating skins.
//We'll load up to four of them but after that you're on your own
typedef struct {
	int skinwidth;
	int skinheight;
	int ofstexels;	//this is 8bit for frame 0 only. only valid in q1 models without replacement textures, used for colourising player skins.
	float skinspeed;
	int texnums;
	int ofstexnums;
} galiasskin_t;

typedef struct {
	int base;
	int bump;
	int fullbright;

	shader_t *shader;
} galiastexnum_t;

typedef struct {
	char name[MAX_QPATH];
	galiastexnum_t texnum;
	int colour;
	int skinnum;
	bucket_t bucket;
} galiascolourmapped_t;

static hashtable_t skincolourmapped;

static vec3_t shadevector;
static vec3_t shadelight, ambientlight;
static void R_LerpFrames(mesh_t *mesh, galiaspose_t *p1, galiaspose_t *p2, float lerp, qbyte alpha, float expand)
{
	extern cvar_t r_nolerp, r_nolightdir;
	float blerp = 1-lerp;
	int i;
	float l;
	int temp;
	vec3_t *p1v, *p2v;
	vec3_t *p1n, *p2n;
	p1v = (vec3_t *)((char *)p1 + p1->ofsverts);
	p2v = (vec3_t *)((char *)p2 + p2->ofsverts);

	p1n = (vec3_t *)((char *)p1 + p1->ofsnormals);
	p2n = (vec3_t *)((char *)p2 + p2->ofsnormals);

	if (p1v == p2v || r_nolerp.value)
	{
		mesh->normals_array = (vec3_t*)((char *)p1 + p1->ofsnormals);
		if (r_nolightdir.value)
		{
			for (i = 0; i < mesh->numvertexes; i++)
			{
				mesh->xyz_array[i][0] = p1v[i][0];
				mesh->xyz_array[i][1] = p1v[i][1];
				mesh->xyz_array[i][2] = p1v[i][2];

				mesh->colors_array[i][0] = ambientlight[0]+shadelight[0];
				mesh->colors_array[i][1] = ambientlight[1]+shadelight[1];
				mesh->colors_array[i][2] = ambientlight[2]+shadelight[2];
				mesh->colors_array[i][3] = alpha;
			}
		}
		else
		{
			for (i = 0; i < mesh->numvertexes; i++)
			{
				mesh->xyz_array[i][0] = p1v[i][0];
				mesh->xyz_array[i][1] = p1v[i][1];
				mesh->xyz_array[i][2] = p1v[i][2];

				l = DotProduct(mesh->normals_array[i], shadevector);

				temp = l*ambientlight[0]+shadelight[0];
				if (temp < 0) temp = 0;
				else if (temp > 255) temp = 255;
				mesh->colors_array[i][0] = temp;

				temp = l*ambientlight[1]+shadelight[1];
				if (temp < 0) temp = 0;
				else if (temp > 255) temp = 255;
				mesh->colors_array[i][1] = temp;

				temp = l*ambientlight[2]+shadelight[2];
				if (temp < 0) temp = 0;
				else if (temp > 255) temp = 255;
				mesh->colors_array[i][2] = temp;

				mesh->colors_array[i][3] = alpha;
			}
		}
	}
	else
	{
		if (r_nolightdir.value)
		{
			for (i = 0; i < mesh->numvertexes; i++)
			{
				mesh->normals_array[i][0] = p1n[i][0]*lerp + p2n[i][0]*blerp;
				mesh->normals_array[i][1] = p1n[i][1]*lerp + p2n[i][1]*blerp;
				mesh->normals_array[i][2] = p1n[i][2]*lerp + p2n[i][2]*blerp;

				mesh->xyz_array[i][0] = p1v[i][0]*lerp + p2v[i][0]*blerp;
				mesh->xyz_array[i][1] = p1v[i][1]*lerp + p2v[i][1]*blerp;
				mesh->xyz_array[i][2] = p1v[i][2]*lerp + p2v[i][2]*blerp;

				mesh->colors_array[i][0] = ambientlight[0];
				mesh->colors_array[i][1] = ambientlight[1];
				mesh->colors_array[i][2] = ambientlight[2];
				mesh->colors_array[i][3] = alpha;
			}
		}
		else
		{
			for (i = 0; i < mesh->numvertexes; i++)
			{
				mesh->normals_array[i][0] = p1n[i][0]*lerp + p2n[i][0]*blerp;
				mesh->normals_array[i][1] = p1n[i][1]*lerp + p2n[i][1]*blerp;
				mesh->normals_array[i][2] = p1n[i][2]*lerp + p2n[i][2]*blerp;

				mesh->xyz_array[i][0] = p1v[i][0]*lerp + p2v[i][0]*blerp;
				mesh->xyz_array[i][1] = p1v[i][1]*lerp + p2v[i][1]*blerp;
				mesh->xyz_array[i][2] = p1v[i][2]*lerp + p2v[i][2]*blerp;

				l = DotProduct(mesh->normals_array[i], shadevector);
				temp = l*ambientlight[0]+shadelight[0];
				if (temp < 0) temp = 0;
				else if (temp > 255) temp = 255;
				mesh->colors_array[i][0] = temp;

				temp = l*ambientlight[1]+shadelight[1];
				if (temp < 0) temp = 0;
				else if (temp > 255) temp = 255;
				mesh->colors_array[i][1] = temp;

				temp = l*ambientlight[2]+shadelight[2];
				if (temp < 0) temp = 0;
				else if (temp > 255) temp = 255;
				mesh->colors_array[i][2] = temp;

				mesh->colors_array[i][3] = alpha;
			}
		}
	}
	if (expand)
	{
		for (i = 0; i < mesh->numvertexes; i++)
		{
			mesh->xyz_array[i][0] += mesh->normals_array[i][0]*expand;
			mesh->xyz_array[i][1] += mesh->normals_array[i][1]*expand;
			mesh->xyz_array[i][2] += mesh->normals_array[i][2]*expand;
		}
	}
}

static void R_GAliasAddDlights(mesh_t *mesh, vec3_t org, vec3_t angles)
{
	int l, v;
	vec3_t rel;
	vec3_t dir;
	vec3_t axis[3];
	float dot, d, a, f;
	AngleVectors(angles, axis[0], axis[1], axis[2]);
	for (l=0 ; l<MAX_DLIGHTS ; l++)
	{
		if (cl_dlights[l].die >= cl.time)
		{
			VectorSubtract (cl_dlights[l].origin,
							org,
							dir);
			if (Length(dir)>cl_dlights[l].radius+mesh->radius)	//far out man!
				continue;

			rel[0] = -DotProduct(dir, axis[0]);
			rel[1] = DotProduct(dir, axis[1]);	//quake's crazy.
			rel[2] = -DotProduct(dir, axis[2]);
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
						f = mesh->colors_array[v][0] + a*cl_dlights[l].color[0];
						if (f > 255)
							f = 255;
						else if (f < 0)
							f = 0;
						mesh->colors_array[v][0] = f;

						f = mesh->colors_array[v][1] + a*cl_dlights[l].color[1];
						if (f > 255)
							f = 255;
						else if (f < 0)
							f = 0;
						mesh->colors_array[v][1] = f;

						f = mesh->colors_array[v][2] + a*cl_dlights[l].color[2];
						if (f > 255)
							f = 255;
						else if (f < 0)
							f = 0;
						mesh->colors_array[v][2] = f;
					}
//					else
//						mesh->colors_array[v][1] =255;
				}
//				else
//					mesh->colors_array[v][2] =255;
			}
		}
	}
}

static void R_GAliasBuildMesh(mesh_t *mesh, galiasinfo_t *inf, int frame1, int frame2, float lerp, float alpha)
{
	galiasgroup_t *g1, *g2;
	if (!inf->groups)
	{
		Con_DPrintf("Model with no frames (%s)\n", currententity->model->name);
		return;
	}
	if (frame1 < 0)
	{
		Con_DPrintf("Negative frame (%s)\n", currententity->model->name);
		frame1 = 0;
	}
	if (frame2 < 0)
	{
		Con_DPrintf("Negative frame (%s)\n", currententity->model->name);
		frame2 = frame1;
	}
	if (frame1 >= inf->groups)
	{
		Con_DPrintf("Too high frame %i (%s)\n", frame1, currententity->model->name);
		frame1 = 0;
	}
	if (frame2 >= inf->groups)
	{
		Con_DPrintf("Too high frame %i (%s)\n", frame2, currententity->model->name);
		frame2 = frame1;
	}

	if (lerp <= 0)
		frame2 = frame1;
	if (lerp >= 1)
		frame1 = frame2;

	if (numTempColours < inf->numverts)
	{
		if (tempColours)
			BZ_Free(tempColours);
		tempColours = BZ_Malloc(sizeof(*tempColours)*inf->numverts);
		numTempColours = inf->numverts;
	}
	if (numTempNormals < inf->numverts)
	{
		if (tempNormals)
			BZ_Free(tempNormals);
		tempNormals = BZ_Malloc(sizeof(*tempNormals)*inf->numverts);
		numTempNormals = inf->numverts;
	}
	if (numTempVertexCoords < inf->numverts)
	{
		if (tempVertexCoords)
			BZ_Free(tempVertexCoords);
		tempVertexCoords = BZ_Malloc(sizeof(*tempVertexCoords)*inf->numverts);
		numTempVertexCoords = inf->numverts;
	}

	mesh->indexes = (index_t*)((char *)inf + inf->ofs_indexes);
	mesh->numindexes = inf->numindexes;
	mesh->st_array = (vec2_t*)((char *)inf + inf->ofs_st_array);
	mesh->lmst_array = NULL;
	mesh->colors_array = tempColours;
	mesh->normals_array = tempNormals;
	mesh->xyz_array = tempVertexCoords;
	mesh->numvertexes = inf->numverts;
	mesh->trneighbors = (int *)((char *)inf + inf->ofs_trineighbours);

	g1 = (galiasgroup_t*)((char *)inf + inf->groupofs + sizeof(galiasgroup_t)*frame1);
	g2 = (galiasgroup_t*)((char *)inf + inf->groupofs + sizeof(galiasgroup_t)*frame2);

	if (g1 == g2)	//lerping within group is only done if not changing group
	{
		lerp = cl.time*g1->rate;
		frame1=lerp;
		frame2=frame1+1;
		lerp-=frame1;
		frame1=frame1%g1->numposes;
		frame2=frame2%g1->numposes;
		
	}
	else	//don't bother with a four way lerp
	{
		frame1=0;
		frame2=0;
	}

	R_LerpFrames(mesh,	(galiaspose_t *)((char *)g1 + g1->poseofs + sizeof(galiaspose_t)*frame1),
						(galiaspose_t *)((char *)g2 + g2->poseofs + sizeof(galiaspose_t)*frame2),
						1-lerp, (qbyte)(alpha*255), currententity->fatness);//20*sin(cl.time));
}

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

static galiastexnum_t *GL_ChooseSkin(galiasinfo_t *inf, char *modelname, entity_t *e)
{
	galiasskin_t *skins;
	galiastexnum_t *texnums;
	int frame;

	int tc, bc;

	if (e->scoreboard)
	{
		if (!e->scoreboard->skin && !gl_nocolors.value)
			Skin_Find(e->scoreboard);
		tc = e->scoreboard->topcolor;
		bc = e->scoreboard->bottomcolor;

		//colour forcing
		if (!cl.splitclients && !(cl.fpd & FPD_NO_FORCE_COLOR))	//no colour/skin forcing in splitscreen.
		{
			if (cl.teamplay && !strcmp(e->scoreboard->team, cl.players[cl.playernum[0]].team))
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

	if (!gl_nocolors.value && (tc != 1 || bc != 1 || (e->scoreboard && e->scoreboard->skin)))
	{
		int			inwidth, inheight;
		int			tinwidth, tinheight;
		char *skinname;
		qbyte	*original;
		int cc;
		galiascolourmapped_t *cm;
		cc = (tc<<4)|bc;

		if (!strstr(modelname, "player"))
			skinname = modelname;
		else
		{
			if (e->scoreboard && e->scoreboard->skin && !gl_nocolors.value)
				skinname = e->scoreboard->skin->name;
			else
				skinname = modelname;
		}

		if (!skincolourmapped.numbuckets)
			Hash_InitTable(&skincolourmapped, 256, BZ_Malloc(Hash_BytesForBuckets(256)));

		for (cm = Hash_Get(&skincolourmapped, skinname); cm; cm = Hash_GetNext(&skincolourmapped, skinname, cm))
		{
			if (cm->colour == cc && cm->skinnum == e->skinnum)
			{
				return &cm->texnum;
			}
		}

		skins = (galiasskin_t*)((char *)inf + inf->ofsskins);
		if (!skins->texnums)
			return NULL;
		if (e->skinnum >= 0 && e->skinnum < inf->numskins)
			skins += e->skinnum;
		texnums = (galiastexnum_t*)((char *)skins + skins->ofstexnums);


		//colourmap isn't present yet.
		cm = BZ_Malloc(sizeof(*cm));
		Q_strncpyz(cm->name, skinname, sizeof(cm->name));
		Hash_Add2(&skincolourmapped, cm->name, cm, &cm->bucket);
		cm->colour = cc;
		cm->skinnum = e->skinnum;
		cm->texnum.fullbright = 0;
		cm->texnum.base = 0;
		cm->texnum.bump = texnums[cm->skinnum].bump;	//can't colour bumpmapping
		if (skins->ofstexels)
		{
			original = (qbyte *)skins + skins->ofstexels;
			inwidth = skins->skinwidth;
			inheight = skins->skinheight;
		}
		else if (skinname!=modelname && e->scoreboard && e->scoreboard->skin)
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
				inrow = original + inwidth*(i*tinheight/scaled_height);
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
			texnums->base = texture_extension_number++;
			GL_Bind(texnums->base);
			glTexImage2D (GL_TEXTURE_2D, 0, gl_solid_format, scaled_width, scaled_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);

			glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);


			//now do the fullbrights.
			out = pixels;
			fracstep = tinwidth*0x10000/scaled_width;
			for (i=0 ; i<scaled_height ; i++, out += scaled_width)
			{
				inrow = original + inwidth*(i*tinheight/scaled_height);
				frac = fracstep >> 1;
				for (j=0 ; j<scaled_width ; j+=1)
				{
					if (inrow[frac>>16] < 255-vid.fullbright)
						((char *) (&out[j]))[3] = 0;	//alpha 0
					frac += fracstep;
				}
			}
			texnums->fullbright = texture_extension_number++;
			GL_Bind(texnums->fullbright);
			glTexImage2D (GL_TEXTURE_2D, 0, gl_alpha_format, scaled_width, scaled_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);

			glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		}
		else
		{
			skins = (galiasskin_t*)((char *)inf + inf->ofsskins);
			if (e->skinnum >= 0 && e->skinnum < inf->numskins)
				skins += e->skinnum;

			if (!skins->texnums)
				return NULL;

			frame = cl.time*skins->skinspeed;
			frame = frame%skins->texnums;
			texnums = (galiastexnum_t*)((char *)skins + skins->ofstexnums + frame*sizeof(galiastexnum_t));
			memcpy(&cm->texnum, texnums, sizeof(cm->texnum));
		}
		return &cm->texnum;
	}

	skins = (galiasskin_t*)((char *)inf + inf->ofsskins);
	if (e->skinnum >= 0 && e->skinnum < inf->numskins)
		skins += e->skinnum;

	if (!skins->texnums)
		return NULL;

	frame = cl.time*skins->skinspeed;
	frame = frame%skins->texnums;
	texnums = (galiastexnum_t*)((char *)skins + skins->ofstexnums + frame*sizeof(galiastexnum_t));
	
	return texnums;
}


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
	vec4_t *input = mesh->xyz_array;
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
	vec4_t *verts = mesh->xyz_array;
	index_t *indexes = mesh->indexes;
	int *neighbours = mesh->trneighbors;
	int numtris = mesh->numindexes/3;

	glBegin(GL_TRIANGLES);
	for (t = 0; t < numtris; t++)
	{
		if (triangleFacing[t])
		{
			//draw front
			glVertex3fv(verts[indexes[t*3+0]]);
			glVertex3fv(verts[indexes[t*3+1]]);
			glVertex3fv(verts[indexes[t*3+2]]);

			//draw back
			glVertex3fv(proj[indexes[t*3+1]]);
			glVertex3fv(proj[indexes[t*3+0]]);
			glVertex3fv(proj[indexes[t*3+2]]);

			//draw side caps
			if (neighbours[t*3+0] < 0 || !triangleFacing[neighbours[t*3+0]])
			{
				glVertex3fv(verts[indexes[t*3+1]]);
				glVertex3fv(verts[indexes[t*3+0]]);
				glVertex3fv(proj [indexes[t*3+0]]);
				glVertex3fv(verts[indexes[t*3+1]]);
				glVertex3fv(proj [indexes[t*3+0]]);
				glVertex3fv(proj [indexes[t*3+1]]);
			}

			if (neighbours[t*3+1] < 0 || !triangleFacing[neighbours[t*3+1]])
			{
				glVertex3fv(verts[indexes[t*3+2]]);
				glVertex3fv(verts[indexes[t*3+1]]);
				glVertex3fv(proj [indexes[t*3+1]]);
				glVertex3fv(verts[indexes[t*3+2]]);
				glVertex3fv(proj [indexes[t*3+1]]);
				glVertex3fv(proj [indexes[t*3+2]]);
			}

			if (neighbours[t*3+2] < 0 || !triangleFacing[neighbours[t*3+2]])
			{
				glVertex3fv(verts[indexes[t*3+0]]);
				glVertex3fv(verts[indexes[t*3+2]]);
				glVertex3fv(proj [indexes[t*3+2]]);
				glVertex3fv(verts[indexes[t*3+0]]);
				glVertex3fv(proj [indexes[t*3+2]]);
				glVertex3fv(proj [indexes[t*3+0]]);
			}
		}
	}
	glEnd();
}

void GL_DrawAliasMesh (mesh_t *mesh, int texnum)
{
	extern int gldepthfunc;
#ifdef Q3SHADERS
	R_UnlockArrays();
#endif

	glDepthFunc(gldepthfunc);
	glDepthMask(1);
	
	GL_Bind(texnum);
	qglCullFace ( GL_FRONT );

	GL_TexEnv(GL_MODULATE);

	glVertexPointer(3, GL_FLOAT, 16, mesh->xyz_array);
	glEnableClientState( GL_VERTEX_ARRAY );

	if (mesh->normals_array)
	{
		glNormalPointer(GL_FLOAT, 0, mesh->normals_array);
		glEnableClientState( GL_NORMAL_ARRAY );
	}

	if (mesh->colors_array)
	{
		glColorPointer(4, GL_UNSIGNED_BYTE, 0, mesh->colors_array);
		glEnableClientState( GL_COLOR_ARRAY );
	}
	else
		glDisableClientState( GL_COLOR_ARRAY );

	glEnableClientState( GL_TEXTURE_COORD_ARRAY );
	glTexCoordPointer(2, GL_FLOAT, 0, mesh->st_array);

	glDrawElements(GL_TRIANGLES, mesh->numindexes, GL_UNSIGNED_INT, mesh->indexes);

	glDisableClientState( GL_VERTEX_ARRAY );
	glDisableClientState( GL_COLOR_ARRAY );
	glDisableClientState( GL_NORMAL_ARRAY );
	glDisableClientState( GL_TEXTURE_COORD_ARRAY );

#ifdef Q3SHADERS
	R_IBrokeTheArrays();
#endif
}

void R_DrawGAliasModel (entity_t *e)
{
	model_t *clmodel = e->model;
	vec3_t mins, maxs;
	vec3_t dist;
	vec_t add;
	int i;
	galiasinfo_t *inf;
	mesh_t mesh;
	galiastexnum_t *skin;
	float entScale;
	vec3_t lightdir;

	int pervertexdlights = 1;

	float	tmatrix[3][4];

	currententity = e;

	if (e->flags & Q2RF_VIEWERMODEL && e->keynum == cl.playernum[r_refdef.currentplayernum]+1)
		return;

	VectorAdd (e->origin, clmodel->mins, mins);
	VectorAdd (e->origin, clmodel->maxs, maxs);

	if (!(e->flags & Q2RF_WEAPONMODEL))
		if (R_CullBox (mins, maxs))
			return;

	if (!(r_refdef.flags & 1))	//RDF_NOWORLDMODEL
	{
		cl.worldmodel->funcs.LightPointValues(e->origin, shadelight, ambientlight, lightdir);
	}
	else
	{
		ambientlight[0] = ambientlight[1] = ambientlight[2] = shadelight[0] = shadelight[1] = shadelight[2] = 255;
		lightdir[0] = 0;
		lightdir[1] = 1;
		lightdir[2] = 1;
	}

	if (!pervertexdlights)
	{
		for (i=0 ; i<MAX_DLIGHTS ; i++)
		{
			if (cl_dlights[i].die >= cl.time)
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
	if (!strcmp(clmodel->name, "progs/player.mdl"))
	{
		float fb = r_fullbrightSkins.value;
		if (fb > cls.allow_fbskins)
			fb = cls.allow_fbskins;
		if (fb < 0)
			fb = 0;
		if (fb)
		{
			for (i = 0; i < 3; i++)
			{
				ambientlight[i] = max(ambientlight[i], 8 + fb * 120);
				shadelight[i] = max(shadelight[i], 8 + fb * 120);
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

	if ((e->drawflags & MLS_MASKIN) == MLS_ABSLIGHT)
	{
		shadelight[0] = shadelight[1] = shadelight[2] = e->abslight;
		ambientlight[0] = ambientlight[1] = ambientlight[2] = 0;
	}

//#define SHOWLIGHTDIR
	{	//lightdir is absolute, shadevector is relative
		vec3_t entaxis[3];
		e->angles[0]*=-1;
		AngleVectors(e->angles, entaxis[0], entaxis[1], entaxis[2]);
		e->angles[0]*=-1;
		entaxis[1][0]*=-1;
		entaxis[1][1]*=-1;
		entaxis[1][2]*=-1;
		shadevector[0] = DotProduct(lightdir, entaxis[0]);
		shadevector[1] = DotProduct(lightdir, entaxis[1]);
		shadevector[2] = DotProduct(lightdir, entaxis[2]);
		VectorNormalize(shadevector);

		VectorCopy(shadevector, mesh.lightaxis[2]);
		VectorVectors(mesh.lightaxis[2], mesh.lightaxis[1], mesh.lightaxis[0]);
		mesh.lightaxis[0][0]*=-1;
		mesh.lightaxis[0][1]*=-1;
		mesh.lightaxis[0][2]*=-1;
	}
	/*
	an = e->angles[1]/180*M_PI;
	shadevector[0] = cos(-an);
	shadevector[1] = sin(-an);
	shadevector[2] = 1;
	VectorNormalize (shadevector);
	*/

	GL_DisableMultitexture();
	GL_TexEnv(GL_MODULATE);
	if (gl_smoothmodels.value)
		glShadeModel (GL_SMOOTH);
	if (gl_affinemodels.value)
		glHint (GL_PERSPECTIVE_CORRECTION_HINT, GL_FASTEST);

	glDisable (GL_ALPHA_TEST);

	if (e->flags & Q2RF_DEPTHHACK)
		glDepthRange (gldepthmin, gldepthmin + 0.3*(gldepthmax-gldepthmin));

//	glColor3f( 1,1,1);
	if ((e->model->flags & EF_SPECIAL_TRANS))	//hexen2 flags.
	{
		glEnable (GL_BLEND);
		glBlendFunc (GL_ONE_MINUS_SRC_ALPHA, GL_SRC_ALPHA);
//		glColor3f( 1,1,1);
		glDisable( GL_CULL_FACE );
	}
	else if (e->drawflags & DRF_TRANSLUCENT)
	{
		glEnable (GL_BLEND);
		glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		e->alpha = r_wateralpha.value;
//		glColor4f( 1,1,1,r_wateralpha.value);
	}
	else if ((e->model->flags & EF_TRANSPARENT))
	{
		glEnable (GL_BLEND);
		glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
//		glColor3f( 1,1,1);
	}
	else if ((e->model->flags & EF_HOLEY))
	{
		glEnable (GL_ALPHA_TEST);
//		glEnable (GL_BLEND);
		glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

//		glColor3f( 1,1,1);
	}
	else if (e->alpha < 1)
	{
		glEnable(GL_BLEND);
		glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	}
	else
	{
		glDisable(GL_BLEND);
		glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	}
	//	glEnable (GL_ALPHA_TEST);

	glPushMatrix();
	R_RotateForEntity(e);

	if (e->scale != 1 && e->scale != 0)	//hexen 2 stuff
	{
		vec3_t scale;
		vec3_t scale_origin;
		float xyfact, zfact;
		scale[0] = (clmodel->maxs[0]-clmodel->mins[0])/255;
		scale[1] = (clmodel->maxs[1]-clmodel->mins[1])/255;
		scale[2] = (clmodel->maxs[2]-clmodel->mins[2])/255;
		scale_origin[0] = clmodel->mins[0];
		scale_origin[1] = clmodel->mins[1];
		scale_origin[2] = clmodel->mins[2];

/*		glScalef(	1/scale[0],
					1/scale[1],
					1/scale[2]);
		glTranslatef (	-scale_origin[0],
						-scale_origin[1],
						-scale_origin[2]);
*/

		if(e->scale != 0 && e->scale != 1)
		{
			entScale = (float)e->scale;
			switch(e->drawflags&SCALE_TYPE_MASKIN)
			{
			default:
			case SCALE_TYPE_UNIFORM:
				tmatrix[0][0] = scale[0]*entScale;
				tmatrix[1][1] = scale[1]*entScale;
				tmatrix[2][2] = scale[2]*entScale;
				xyfact = zfact = (entScale-1.0)*127.95;
				break;
			case SCALE_TYPE_XYONLY:
				tmatrix[0][0] = scale[0]*entScale;
				tmatrix[1][1] = scale[1]*entScale;
				tmatrix[2][2] = scale[2];
				xyfact = (entScale-1.0)*127.95;
				zfact = 1.0;
				break;
			case SCALE_TYPE_ZONLY:
				tmatrix[0][0] = scale[0];
				tmatrix[1][1] = scale[1];
				tmatrix[2][2] = scale[2]*entScale;
				xyfact = 1.0;
				zfact = (entScale-1.0)*127.95;
				break;
			}
			switch(currententity->drawflags&SCALE_ORIGIN_MASKIN)
			{
			default:
			case SCALE_ORIGIN_CENTER:
				tmatrix[0][3] = scale_origin[0]-scale[0]*xyfact;
				tmatrix[1][3] = scale_origin[1]-scale[1]*xyfact;
				tmatrix[2][3] = scale_origin[2]-scale[2]*zfact;
				break;
			case SCALE_ORIGIN_BOTTOM:
				tmatrix[0][3] = scale_origin[0]-scale[0]*xyfact;
				tmatrix[1][3] = scale_origin[1]-scale[1]*xyfact;
				tmatrix[2][3] = scale_origin[2];
				break;
			case SCALE_ORIGIN_TOP:
				tmatrix[0][3] = scale_origin[0]-scale[0]*xyfact;
				tmatrix[1][3] = scale_origin[1]-scale[1]*xyfact;
				tmatrix[2][3] = scale_origin[2]-scale[2]*zfact*2.0;
				break;
			}
		}
		else
		{
			tmatrix[0][0] = scale[0];
			tmatrix[1][1] = scale[1];
			tmatrix[2][2] = scale[2];
			tmatrix[0][3] = scale_origin[0];
			tmatrix[1][3] = scale_origin[1];
			tmatrix[2][3] = scale_origin[2];
		}

/*		if(clmodel->flags&EF_ROTATE)
		{ // Floating motion
			tmatrix[2][3] += sin(currententity->origin[0]
				+currententity->origin[1]+(cl.time*3))*5.5;
		}*/

		glTranslatef (tmatrix[0][3],tmatrix[1][3],tmatrix[2][3]);
		glScalef (tmatrix[0][0],tmatrix[1][1],tmatrix[2][2]);
		
		glScalef(	1/scale[0],
					1/scale[1],
					1/scale[2]);
		glTranslatef (	-scale_origin[0],
						-scale_origin[1],
						-scale_origin[2]);
	}

	inf = GLMod_Extradata (clmodel);
	if (qglPNTrianglesfATI && gl_ati_truform.value)
		glEnable(GL_PN_TRIANGLES_ATI);

	memset(&mesh, 0, sizeof(mesh));
	while(inf)
	{
		R_GAliasBuildMesh(&mesh, inf, e->frame, e->oldframe, e->lerptime, e->alpha);
		if (pervertexdlights)
			R_GAliasAddDlights(&mesh, e->origin, e->angles);
		skin = GL_ChooseSkin(inf, clmodel->name, e);
		c_alias_polys += mesh.numindexes/3;

		if (!skin)
		{
			glEnable(GL_TEXTURE_2D);
			GL_DrawAliasMesh(&mesh, 1);
		}
#ifdef Q3SHADERS
		else if (skin->shader)
		{
			meshbuffer_t mb;

			mb.entity = &r_worldentity;
			mb.shader = skin->shader;
			mb.fog = NULL;
			mb.mesh = &mesh;
			mb.infokey = currententity->keynum;
			mb.dlightbits = 0;

			R_IBrokeTheArrays();

			R_PushMesh(&mesh, skin->shader->features | MF_NONBATCHED | MF_COLORS);

			R_RenderMeshBuffer ( &mb, false );
		}
#endif
		else
		{
			glEnable(GL_TEXTURE_2D);
//			if (skin->bump)
//				GL_DrawMeshBump(&mesh, skin->base, 0, skin->bump, 0);
//			else
				GL_DrawAliasMesh(&mesh, skin->base);

			if (skin->fullbright && r_fb_models.value && cls.allow_luma)
			{
				mesh.colors_array = NULL;
				glEnable(GL_BLEND);
				glColor4f(1, 1, 1, e->alpha*r_fb_models.value);
				c_alias_polys += mesh.numindexes/3;
				GL_DrawAliasMesh(&mesh, skin->fullbright);
			}
	
		}
		if (inf->nextsurf)
			inf = (galiasinfo_t*)((char *)inf + inf->nextsurf);
		else
			inf = NULL;
	}

	if (qglPNTrianglesfATI && gl_ati_truform.value)
		glDisable(GL_PN_TRIANGLES_ATI);

#ifdef SHOWLIGHTDIR	//testing
	glDisable(GL_TEXTURE_2D);
	glBegin(GL_LINES);
	glColor3f(1,0,0);
	glVertex3f(	0,
				0,
				0);
	glVertex3f(	100*mesh.lightaxis[0][0],
				100*mesh.lightaxis[0][1],
				100*mesh.lightaxis[0][2]);

glColor3f(0,1,0);
	glVertex3f(	0,
				0,
				0);
	glVertex3f(	100*mesh.lightaxis[1][0],
				100*mesh.lightaxis[1][1],
				100*mesh.lightaxis[1][2]);

glColor3f(0,0,1);
	glVertex3f(	0,
				0,
				0);
	glVertex3f(	100*mesh.lightaxis[2][0],
				100*mesh.lightaxis[2][1],
				100*mesh.lightaxis[2][2]);
	glEnd();
	glEnable(GL_TEXTURE_2D);
#endif

	glPopMatrix();

	glDisable(GL_BLEND);

	glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	GL_TexEnv(GL_REPLACE);

	glEnable(GL_TEXTURE_2D);

	glShadeModel (GL_FLAT);
	if (gl_affinemodels.value)
		glHint (GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);	

	if (e->flags & Q2RF_DEPTHHACK)
		glDepthRange (gldepthmin, gldepthmax);

	if ((currententity->model->flags & EF_SPECIAL_TRANS) && gl_cull.value)
		glEnable( GL_CULL_FACE );
	if ((currententity->model->flags & EF_HOLEY))
		glDisable( GL_ALPHA_TEST );

#ifdef SHOWLIGHTDIR	//testing
	glDisable(GL_TEXTURE_2D);
	glColor3f(1,1,1);
	glBegin(GL_LINES);
	glVertex3f(	currententity->origin[0],
				currententity->origin[1],
				currententity->origin[2]);
	glVertex3f(	currententity->origin[0]+100*lightdir[0],
				currententity->origin[1]+100*lightdir[1],
				currententity->origin[2]+100*lightdir[2]);
	glEnd();
	glEnable(GL_TEXTURE_2D);
#endif
}

void R_DrawGAliasModelLighting (entity_t *e)
{
	model_t *clmodel = e->model;
	vec3_t mins, maxs;
	vec3_t dist;
	vec_t add, an;
	vec3_t lightdir;
	int i;
	galiasinfo_t *inf;
	mesh_t mesh;

	if (e->flags & Q2RF_VIEWERMODEL)
		return;

	//Total insanity with r_shadows 2...
//	if (!strcmp (clmodel->name, "progs/flame2.mdl"))
//		CL_NewDlight (e, e->origin[0]-1, e->origin[1]+1, e->origin[2]+24, 200 + (rand()&31), host_frametime*2, 3);

//	if (!strcmp (clmodel->name, "progs/armor.mdl"))
//		CL_NewDlight (e->keynum, e->origin[0]-1, e->origin[1]+1, e->origin[2]+25, 200 + (rand()&31), host_frametime*2, 3);

	VectorAdd (e->origin, clmodel->mins, mins);
	VectorAdd (e->origin, clmodel->maxs, maxs);

	if (!(e->flags & Q2RF_WEAPONMODEL))
		if (R_CullBox (mins, maxs))
			return;

	if (!(r_refdef.flags & 1))	//RDF_NOWORLDMODEL
	{
		cl.worldmodel->funcs.LightPointValues(e->origin, shadelight, ambientlight, lightdir);
	}
	else
	{
		ambientlight[0] = ambientlight[1] = ambientlight[2] = shadelight[0] = shadelight[1] = shadelight[2] = 255;
		lightdir[0] = 0;
		lightdir[1] = 1;
		lightdir[2] = 1;
	}

	if (e->flags & 4)
	{
		if (ambientlight[0] < 24)
			ambientlight[0] = shadelight[0] = 24;
		if (ambientlight[1] < 24)
			ambientlight[1] = shadelight[1] = 24;
		if (ambientlight[2] < 24)
			ambientlight[2] = shadelight[2] = 24;
	}

	for (i=0 ; i<MAX_DLIGHTS ; i++)
	{
		if (cl_dlights[i].die >= cl.time)
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

//MORE HUGE HACKS! WHEN WILL THEY CEASE!
	// clamp lighting so it doesn't overbright as much
	// ZOID: never allow players to go totally black
	if (!strcmp(clmodel->name, "progs/player.mdl"))
	{
		for (i = 0; i < 3; i++)
			if (ambientlight[i] < 8)
				ambientlight[i] = shadelight[i] = 8;
	}
	for (i = 0; i < 3; i++)
	{
		if (ambientlight[i] > 128)
			ambientlight[i] = 128;

		shadelight[i] /= 200.0/255;
		ambientlight[i] /= 200.0/255;
	}

	an = e->angles[1]/180*M_PI;
	shadevector[0] = cos(-an);
	shadevector[1] = sin(-an);
	shadevector[2] = 1;
	VectorNormalize (shadevector);

	GL_DisableMultitexture();
	GL_TexEnv(GL_MODULATE);
	if (gl_smoothmodels.value)
		glShadeModel (GL_SMOOTH);
	if (gl_affinemodels.value)
		glHint (GL_PERSPECTIVE_CORRECTION_HINT, GL_FASTEST);


	if (e->flags & Q2RF_DEPTHHACK)
		glDepthRange (gldepthmin, gldepthmin + 0.3*(gldepthmax-gldepthmin));

	glPushMatrix();
	R_RotateForEntity(e);
	inf = GLMod_Extradata (clmodel);
	if (gl_ati_truform.value)
		glEnable(GL_PN_TRIANGLES_ATI);
	while(inf)
	{
		R_GAliasBuildMesh(&mesh, inf, e->frame, e->oldframe, e->lerptime, e->alpha);
		mesh.colors_array = NULL;
#ifdef Q3SHADERS
#else
		GL_DrawMesh(&mesh, NULL, 0, 0);
#endif

		if (inf->nextsurf)
			inf = (galiasinfo_t*)((char *)inf + inf->nextsurf);
		else
			inf = NULL;
	}
	glPopMatrix();
	if (gl_ati_truform.value)
		glDisable(GL_PN_TRIANGLES_ATI);

	GL_TexEnv(GL_REPLACE);

	glShadeModel (GL_FLAT);
	if (gl_affinemodels.value)
		glHint (GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);	

	if (e->flags & Q2RF_DEPTHHACK)
		glDepthRange (gldepthmin, gldepthmax);
}

//returns result in the form of the result vector
void RotateLightVector(vec3_t angles, vec3_t origin, vec3_t lightpoint, vec3_t result)
{
	vec3_t f, r, u, offs;

	angles[0]*=-1;
	AngleVectors(angles, f, r, u);
	angles[0]*=-1;

	offs[0] = lightpoint[0] - origin[0];
	offs[1] = lightpoint[1] - origin[1];
	offs[2] = lightpoint[2] - origin[2];

	result[0] = DotProduct (offs, f);
	result[1] = -DotProduct (offs, r);
	result[2] = DotProduct (offs, u);
}

//FIXME: Be less agressive.
//This function will have to be called twice (for geforce cards), with the same data, so do the building once and rendering twice.
void R_DrawGAliasShadowVolume(entity_t *e, vec3_t lightpos, float radius)
{
	model_t *clmodel = e->model;
	galiasinfo_t *inf;
	mesh_t mesh;
	vec3_t lightorg;

	if (!strcmp (clmodel->name, "progs/flame2.mdl"))
		return;
	if (!strncmp (clmodel->name, "progs/bolt", 10))
		return;
	if (r_noaliasshadows.value)
		return;

	RotateLightVector(e->angles, e->origin, lightpos, lightorg);

	if (Length(lightorg) > radius + clmodel->radius)
		return;

	glPushMatrix();
	R_RotateForEntity(e);


	inf = GLMod_Extradata (clmodel);
	while(inf)
	{
		if (inf->ofs_trineighbours)
		{
			R_GAliasBuildMesh(&mesh, inf, e->frame, e->oldframe, e->lerptime, e->alpha);
			R_CalcFacing(&mesh, lightorg);
			R_ProjectShadowVolume(&mesh, lightorg);
			R_DrawShadowVolume(&mesh);
		}

		if (inf->nextsurf)
			inf = (galiasinfo_t*)((char *)inf + inf->nextsurf);
		else
			inf = NULL;
	}

	glPopMatrix();
}






static int R_FindTriangleWithEdge ( int *indexes, int numtris, int start, int end, int ignore)
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

static void R_BuildTriangleNeighbours ( int *neighbours, int *indexes, int numtris )
{
	int i, *n;
	int *index;

	for (i = 0, index = indexes, n = neighbours; i < numtris; i++, index += 3, n += 3)
	{
		n[0] = R_FindTriangleWithEdge (indexes, numtris, index[1], index[0], i);
		n[1] = R_FindTriangleWithEdge (indexes, numtris, index[2], index[1], i);
		n[2] = R_FindTriangleWithEdge (indexes, numtris, index[0], index[2], i);
	}
}





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



//Q1 model loading
#if 1
static galiasinfo_t *galias;
static model_t *loadmodel;
static dmdl_t *pq1inmodel;
#define NUMVERTEXNORMALS	162
extern float	r_avertexnormals[NUMVERTEXNORMALS][3];
static void *Q1_LoadFrameGroup (daliasframetype_t *pframetype, int *seamremaps)
{
	galiaspose_t *pose;
	galiasgroup_t *frame;
	dtrivertx_t		*pinframe;
	int				i, j, k;
	daliasgroup_t *ingroup;
	daliasinterval_t *intervals;

	vec3_t *normals;
	vec3_t *verts;

	frame = (galiasgroup_t*)((char *)galias + galias->groupofs);

	for (i = 0; i < pq1inmodel->numframes; i++)
	{
		switch(LittleLong(pframetype->type))
		{
		case ALIAS_SINGLE:
			pinframe = (dtrivertx_t*)((char *)(pframetype+1)+sizeof(daliasframe_t));
			pose = (galiaspose_t *)Hunk_Alloc(sizeof(galiaspose_t) + sizeof(vec3_t)*2*galias->numverts);
			frame->poseofs = (char *)pose - (char *)frame;
			frame->numposes = 1;
			galias->groups++;

			verts = (vec3_t *)(pose+1);
			normals = &verts[galias->numverts];
			pose->ofsverts = (char *)verts - (char *)pose;
			pose->ofsnormals = (char *)normals - (char *)pose;

			for (j = 0; j < pq1inmodel->numverts; j++)
			{
				verts[j][0] = pinframe[j].v[0]*pq1inmodel->scale[0]+pq1inmodel->scale_origin[0];
				verts[j][1] = pinframe[j].v[1]*pq1inmodel->scale[1]+pq1inmodel->scale_origin[1];
				verts[j][2] = pinframe[j].v[2]*pq1inmodel->scale[2]+pq1inmodel->scale_origin[2];

				VectorCopy(r_avertexnormals[pinframe[j].lightnormalindex], normals[j]);

				if (seamremaps[j] != j)
				{
					VectorCopy(verts[j], verts[seamremaps[j]]);
					VectorCopy(normals[j], normals[seamremaps[j]]);
				}
			}

//			GL_GenerateNormals((float*)verts, (float*)normals, (int *)((char *)galias + galias->ofs_indexes), galias->numindexes/3, galias->numverts);

			pframetype = (daliasframetype_t *)&pinframe[pq1inmodel->numverts];
			break;
		case ALIAS_GROUP:
			ingroup = (daliasgroup_t *)(pframetype+1);

			pose = (galiaspose_t *)Hunk_Alloc(ingroup->numframes*(sizeof(galiaspose_t) + sizeof(vec3_t)*2*galias->numverts));
			frame->poseofs = (char *)pose - (char *)frame;
			frame->numposes = LittleLong(ingroup->numframes);
			galias->groups++;

			verts = (vec3_t *)(pose+frame->numposes);
			normals = &verts[galias->numverts];

			intervals = (daliasinterval_t *)(ingroup+1);
			frame->rate = 1/LittleFloat(intervals->interval);

			pinframe = (dtrivertx_t *)(intervals+frame->numposes);
			for (k = 0; k < frame->numposes; k++)
			{
				pose->ofsverts = (char *)verts - (char *)pose;
				pose->ofsnormals = (char *)normals - (char *)pose;

				pinframe = (dtrivertx_t *)((char *)pinframe + sizeof(daliasframe_t));
				for (j = 0; j < pq1inmodel->numverts; j++)
				{
					verts[j][0] = pinframe[j].v[0]*pq1inmodel->scale[0]+pq1inmodel->scale_origin[0];
					verts[j][1] = pinframe[j].v[1]*pq1inmodel->scale[1]+pq1inmodel->scale_origin[1];
					verts[j][2] = pinframe[j].v[2]*pq1inmodel->scale[2]+pq1inmodel->scale_origin[2];

					VectorCopy(r_avertexnormals[pinframe[j].lightnormalindex], normals[j]);
					if (seamremaps[j] != j)
					{
						VectorCopy(verts[j], verts[seamremaps[j]]);
						VectorCopy(normals[j], normals[seamremaps[j]]);
					}
				}
				verts = &normals[galias->numverts];
				normals = &verts[galias->numverts];
				pose++;

				pinframe += pq1inmodel->numverts;
			}

//			GL_GenerateNormals((float*)verts, (float*)normals, (int *)((char *)galias + galias->ofs_indexes), galias->numindexes/3, galias->numverts);

			pframetype = (daliasframetype_t *)pinframe;
			break;
		default:
			Sys_Error("Bad frame type\n");
		}
		frame++;
	}
	return pframetype;
}

static void *Q1_LoadSkins (daliasskintype_t *pskintype, qboolean alpha)
{
	extern int gl_bumpmappingpossible;
	galiastexnum_t *texnums;
	char skinname[MAX_QPATH];
	int i;
	int s, t;
	int *count;
	float *intervals;
	qbyte *data, *saved;
	galiasskin_t *outskin = (galiasskin_t *)((char *)galias + galias->ofsskins);

	int texture;
	int fbtexture;

	s = pq1inmodel->skinwidth*pq1inmodel->skinheight;
	for (i = 0; i < pq1inmodel->numskins; i++)
	{
		switch(LittleLong(pskintype->type))
		{
		case ALIAS_SKIN_SINGLE:
			outskin->skinwidth = pq1inmodel->skinwidth;
			outskin->skinheight = pq1inmodel->skinheight;

			sprintf(skinname, "%s_%i", loadname, i);
			texture = Mod_LoadReplacementTexture(skinname, true, false, true);
			if (!texture)
			{
				sprintf(skinname, "textures/models/%s_%i", loadname, i);
				texture = Mod_LoadReplacementTexture(skinname, true, false, true);
				if (texture && r_fb_models.value)
				{
					sprintf(skinname, "textures/models/%s_%i_luma", loadname, i);
					fbtexture = Mod_LoadReplacementTexture(skinname, true, true, true);
				}
				else
					fbtexture = 0;
			}
			else if (texture && r_fb_models.value)
			{
				sprintf(skinname, "%s_%i_luma", loadname, i);
				fbtexture = Mod_LoadReplacementTexture(skinname, true, true, true);
			}
			else
				fbtexture = 0;

			if (!texture)
			{
				texnums = Hunk_Alloc(sizeof(*texnums)+s);
				saved = (qbyte*)(texnums+1);
				outskin->ofstexels = (qbyte *)(saved) - (qbyte *)outskin;
				memcpy(saved, pskintype+1, s);
				GLMod_FloodFillSkin(saved, outskin->skinwidth, outskin->skinheight);
				sprintf(skinname, "%s_%i", loadname, i);
				texture = GL_LoadTexture(skinname, outskin->skinwidth, outskin->skinheight, saved, true, alpha);
				if (r_fb_models.value)
				{
					sprintf(skinname, "%s_%i_luma", loadname, i);
					fbtexture = GL_LoadTextureFB(skinname, outskin->skinwidth, outskin->skinheight, saved, true, true);
				}
			}
			else
			{
				texnums = Hunk_Alloc(sizeof(*texnums));
				outskin->ofstexels = 0;
			}
			outskin->texnums=1;

			outskin->ofstexnums = (char *)texnums - (char *)outskin;

			texnums->base = texture;
			texnums->fullbright = fbtexture;

			pskintype = (daliasskintype_t *)((char *)(pskintype+1)+s);
			break;

		default:
			outskin->skinwidth = pq1inmodel->skinwidth;
			outskin->skinheight = pq1inmodel->skinheight;
			count = (int *)(pskintype+1);
			intervals = (float *)(count+1);
			outskin->texnums = LittleLong(*count);
			data = (qbyte *)(intervals + outskin->texnums);
			texnums = Hunk_Alloc(sizeof(*texnums)*outskin->texnums);
			outskin->ofstexnums = (char *)texnums - (char *)outskin;
			outskin->ofstexels = 0;
			for (t = 0; t < outskin->texnums; t++,data+=s, texnums++)
			{
				sprintf(skinname, "%s_%i%c", loadname, i, t+'a');
				texture = Mod_LoadReplacementTexture(skinname, true, false, true);
				if (texture)
				{
					texnums->base = texture;
					if (r_fb_models.value)
					{
						sprintf(skinname, "%s_%i%c_luma", loadname, i, t+'a');
						texnums->fullbright = Mod_LoadReplacementTexture(skinname, true, true, true);
					}
				}
				else
				{
					if (t == 0)
					{
						saved = Hunk_Alloc(s);
						outskin->ofstexels = (qbyte *)(saved) - (qbyte *)outskin;
					}
					else
						saved = BZ_Malloc(s);
					memcpy(saved, pskintype+1, s);
					GLMod_FloodFillSkin(saved, outskin->skinwidth, outskin->skinheight);
					sprintf(skinname, "%s_%i%c", loadname, i, t+'a');
					texnums->base = GL_LoadTexture(skinname, outskin->skinwidth, outskin->skinheight, saved, true, alpha);

					if (gl_bumpmappingpossible)
					{
						char name[MAX_QPATH];
						COM_StripExtension(skinname, name);	//go for the normalmap
						strcat(name, "_norm");
						texnums->bump = Mod_LoadHiResTexture(name, true, true, false);
						if (!texnums->bump)
						{
							strcpy(name, loadmodel->name);
							COM_StripExtension(COM_SkipPath(skinname), COM_SkipPath(name));
							strcat(name, "_norm");
							texnums->bump = Mod_LoadHiResTexture(name, true, true, false);
							if (!texnums->bump)
							{
								COM_StripExtension(skinname, name);	//bother, go for heightmap and convert
								strcat(name, "_bump");
								texnums->bump = Mod_LoadBumpmapTexture(name);
								if (!texnums->bump)
								{
									strcpy(name, loadmodel->name);
									strcpy(COM_SkipPath(name), COM_SkipPath(skinname));	//eviile eh?
									COM_StripExtension(name, name);
									strcat(name, "_bump");
									texnums->bump = Mod_LoadBumpmapTexture(name);
								}
							}
						}
					}

					if (r_fb_models.value)
					{
						sprintf(skinname, "%s_%i%c_luma", loadname, i, t+'a');
						texnums->fullbright = GL_LoadTextureFB(skinname, outskin->skinwidth, outskin->skinheight, saved, true, true);
					}

					if (t != 0)	//only keep the first.
						BZ_Free(saved);
				}
			}	
			pskintype = (daliasskintype_t *)data;
			break;
		}
		outskin++;
	}
	galias->numskins=pq1inmodel->numskins;
	return pskintype;
}

void GL_LoadQ1Model (model_t *mod, void *buffer)
{
	vec2_t *st_array;
	int hunkstart, hunkend, hunktotal;
	int version;
	int i, j, onseams;
	dstvert_t *pinstverts;
	dtriangle_t *pintriangles;
	int *seamremap;
	index_t *indexes;

	int size;

	loadmodel=mod;

	//we've got to have this bit
	if (!strcmp(loadmodel->name, "progs/player.mdl") ||
		!strcmp(loadmodel->name, "progs/eyes.mdl"))
	{
		unsigned short crc;
		qbyte *p;
		int len;
		char st[40];

		CRC_Init(&crc);
		for (len = com_filesize, p = buffer; len; len--, p++)
			CRC_ProcessByte(&crc, *p);
	
		sprintf(st, "%d", (int) crc);
		Info_SetValueForKey (cls.userinfo, 
			!strcmp(loadmodel->name, "progs/player.mdl") ? pmodel_name : emodel_name,
			st, MAX_INFO_STRING);

		if (cls.state >= ca_connected) {
			MSG_WriteByte (&cls.netchan.message, clc_stringcmd);
			sprintf(st, "setinfo %s %d", 
				!strcmp(loadmodel->name, "progs/player.mdl") ? pmodel_name : emodel_name,
				(int)crc);
			SZ_Print (&cls.netchan.message, st);
		}
	}
	
	hunkstart = Hunk_LowMark ();

	pq1inmodel = (dmdl_t *)buffer;

	version = LittleLong (pq1inmodel->version);
	if (version != ALIAS_VERSION)
		Sys_Error ("%s has wrong version number (%i should be %i)",
				 mod->name, version, ALIAS_VERSION);

	if (pq1inmodel->numframes < 1 ||
		pq1inmodel->numskins < 1 ||
		pq1inmodel->numtris < 1 ||
		pq1inmodel->numverts < 3 ||
		pq1inmodel->skinheight < 1 ||
		pq1inmodel->skinwidth < 1)
		Sys_Error("Model %s has an invalid quantity\n", mod->name);

	mod->flags = LittleLong (pq1inmodel->flags);

	size = sizeof(galiasinfo_t)
		+ pq1inmodel->numframes*sizeof(galiasgroup_t)
		+ pq1inmodel->numskins*sizeof(galiasskin_t);

	galias = Hunk_Alloc(size);
	galias->groupofs = sizeof(*galias);
	galias->ofsskins = sizeof(*galias)+pq1inmodel->numframes*sizeof(galiasgroup_t);
	galias->nextsurf = 0;

//skins
	if( mod->flags & EF_HOLEY )
		pinstverts = (dstvert_t *)Q1_LoadSkins((daliasskintype_t *)(pq1inmodel+1), 3);
	else if( mod->flags & EF_TRANSPARENT )
		pinstverts = (dstvert_t *)Q1_LoadSkins((daliasskintype_t *)(pq1inmodel+1), 2);
	else if( mod->flags & EF_SPECIAL_TRANS )
		pinstverts = (dstvert_t *)Q1_LoadSkins((daliasskintype_t *)(pq1inmodel+1), 4);
	else
		pinstverts = (dstvert_t *)Q1_LoadSkins((daliasskintype_t *)(pq1inmodel+1), 0);

//	pinstverts = (dstvert_t *)Q1_LoadSkins((daliasskintype_t *)(pq1inmodel+1));

	//count number of verts that are onseam.
	for (onseams=0,i = 0; i < pq1inmodel->numverts; i++)
	{
		if (pinstverts[i].onseam)
			onseams++;
	}
	seamremap = BZ_Malloc(sizeof(int)*pq1inmodel->numverts);

	galias->numverts = pq1inmodel->numverts+onseams;
	//st
	st_array = Hunk_Alloc(sizeof(*st_array)*(pq1inmodel->numverts+onseams));
	galias->ofs_st_array = (char *)st_array - (char *)galias;
	for (j=pq1inmodel->numverts,i = 0; i < pq1inmodel->numverts; i++)
	{
		st_array[i][0] = LittleLong(pinstverts[i].s)/(float)pq1inmodel->skinwidth;
		st_array[i][1] = LittleLong(pinstverts[i].t)/(float)pq1inmodel->skinheight;

		if (pinstverts[i].onseam)
		{
			st_array[j][0] = st_array[i][0]+0.5;
			st_array[j][1] = st_array[i][1];
			seamremap[i] = j;
			j++;
		}
		else
			seamremap[i] = i;
	}

	//trianglelists;
	pintriangles = (dtriangle_t *)&pinstverts[pq1inmodel->numverts];

	galias->numindexes = pq1inmodel->numtris*3;
	indexes = Hunk_Alloc(galias->numindexes*sizeof(*indexes));
	galias->ofs_indexes = (char *)indexes - (char *)galias;
	for (i=0 ; i<pq1inmodel->numtris ; i++)
	{
		if (!pintriangles[i].facesfront)
		{
			indexes[i*3+0] = seamremap[LittleLong(pintriangles[i].vertindex[0])];
			indexes[i*3+1] = seamremap[LittleLong(pintriangles[i].vertindex[1])];
			indexes[i*3+2] = seamremap[LittleLong(pintriangles[i].vertindex[2])];
		}
		else
		{
			indexes[i*3+0] = LittleLong(pintriangles[i].vertindex[0]);
			indexes[i*3+1] = LittleLong(pintriangles[i].vertindex[1]);
			indexes[i*3+2] = LittleLong(pintriangles[i].vertindex[2]);
		}
	}

	//frames
	Q1_LoadFrameGroup((daliasframetype_t *)&pintriangles[pq1inmodel->numtris], seamremap);
	BZ_Free(seamremap);

	if (r_shadows.value)
	{
		int *neighbours;
		neighbours = Hunk_Alloc(sizeof(int)*3*pq1inmodel->numtris);
		galias->ofs_trineighbours = (qbyte *)neighbours - (qbyte *)galias;
		R_BuildTriangleNeighbours(neighbours, indexes, pq1inmodel->numtris);
	}

	VectorCopy (pq1inmodel->scale_origin, mod->mins);
	VectorMA (mod->mins, 255, pq1inmodel->scale, mod->maxs);
//
// move the complete, relocatable alias model to the cache
//	
	hunkend = Hunk_LowMark ();
	Hunk_Alloc(0);
	hunktotal = hunkend - hunkstart;
	
	Cache_Alloc (&mod->cache, hunktotal, loadname);
	mod->type = mod_alias;
	if (!mod->cache.data)
	{
		Hunk_FreeToLowMark (hunkstart);
		return;
	}
	memcpy (mod->cache.data, galias, hunktotal);

	Hunk_FreeToLowMark (hunkstart);

}
#endif


int Mod_ReadFlagsFromMD1(char *name, int md3version)
{
	dmdl_t				*pinmodel;
	char fname[MAX_QPATH];
	COM_StripExtension(name, fname);
	COM_DefaultExtension(fname, ".mdl");

	if (strcmp(name, fname))	//md3 renamed as mdl
	{
		COM_StripExtension(name, fname);	//seeing as the md3 is named over the mdl,
		COM_DefaultExtension(fname, ".md1");//read from a file with md1 (one, not an ell)
		return 0;
	}

	pinmodel = (dmdl_t *)COM_LoadTempFile(fname);

	if (!pinmodel)	//not found
		return 0;

	if (pinmodel->ident != IDPOLYHEADER)
		return 0;
	if (pinmodel->version != ALIAS_VERSION)
		return 0;
	return pinmodel->flags;
}

#ifdef MD2MODELS

////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//Q2 model loading

static galiasinfo_t *galias;
static model_t *loadmodel;
static md2_t *pq2inmodel;
#define Q2NUMVERTEXNORMALS	162
extern vec3_t	bytedirs[Q2NUMVERTEXNORMALS];

static void Q2_LoadSkins(char *skins)
{
	int i;
	galiastexnum_t *texnums;
	galiasskin_t *outskin = (galiasskin_t *)((char *)galias + galias->ofsskins);

	for (i = 0; i < pq2inmodel->num_skins; i++)
	{
		texnums = Hunk_Alloc(sizeof(*texnums));
		outskin->ofstexnums = (char *)texnums - (char *)outskin;
		outskin->texnums=1;

		texnums->base = Mod_LoadReplacementTexture(skins, true, false, true);
		outskin->skinwidth = 0;
		outskin->skinheight = 0;
		outskin->skinspeed = 0;

		skins += MD2MAX_SKINNAME;
	}
}

#define MD2_MAX_TRIANGLES 4096
void GL_LoadQ2Model (model_t *mod, void *buffer)
{
	int hunkstart, hunkend, hunktotal;
	int version;
	int i, j;
	dmd2stvert_t *pinstverts;
	dmd2triangle_t *pintri;
	index_t *indexes;
	int numindexes;

	vec3_t min;
	vec3_t max;

	galiaspose_t *pose;
	galiasgroup_t *poutframe;
	dmd2aliasframe_t *pinframe;
	int framesize;
	vec3_t *verts;
	vec3_t *normals;
	vec2_t *st_array;
	
	int		indremap[MD2_MAX_TRIANGLES*3];
	unsigned short		ptempindex[MD2_MAX_TRIANGLES*3], ptempstindex[MD2_MAX_TRIANGLES*3];

	int numverts;

	int size;


	loadmodel=mod;

	hunkstart = Hunk_LowMark ();

	pq2inmodel = (md2_t *)buffer;

	version = LittleLong (pq2inmodel->version);
	if (version != MD2ALIAS_VERSION)
		Sys_Error ("%s has wrong version number (%i should be %i)",
				 mod->name, version, MD2ALIAS_VERSION);

	if (pq2inmodel->num_frames < 1 ||
		pq2inmodel->num_skins < 0 ||
		pq2inmodel->num_tris < 1 ||
		pq2inmodel->num_xyz < 3 ||
		pq2inmodel->num_st < 3 ||
		pq2inmodel->skinheight < 1 ||
		pq2inmodel->skinwidth < 1)
		Sys_Error("Model %s has an invalid quantity\n", mod->name);

	mod->flags = 0;

	size = sizeof(galiasinfo_t)
		+ pq2inmodel->num_frames*sizeof(galiasgroup_t)
		+ pq2inmodel->num_skins*sizeof(galiasskin_t);

	galias = Hunk_Alloc(size);
	galias->groupofs = sizeof(*galias);
	galias->ofsskins = sizeof(*galias)+pq2inmodel->num_frames*sizeof(galiasgroup_t);
	galias->nextsurf = 0;

//skins
	Q2_LoadSkins(((char *)pq2inmodel+pq2inmodel->ofs_skins));

	//trianglelists;
	pintri = (dmd2triangle_t *)((char *)pq2inmodel + pq2inmodel->ofs_tris);


	for (i=0 ; i<pq2inmodel->num_tris ; i++, pintri++)
	{
		for (j=0 ; j<3 ; j++)
		{
			ptempindex[i*3+j] = ( unsigned short )LittleShort ( pintri->xyz_index[j] );
			ptempstindex[i*3+j] = ( unsigned short )LittleShort ( pintri->st_index[j] );
		}
	}

	numindexes = galias->numindexes = pq2inmodel->num_tris*3;
	indexes = Hunk_Alloc(galias->numindexes*sizeof(*indexes));
	galias->ofs_indexes = (char *)indexes - (char *)galias;
	memset ( indremap, -1, sizeof(indremap) );
	numverts=0;

	for ( i = 0; i < numindexes; i++ )
	{
		if ( indremap[i] != -1 ) {
			continue;
		}

		for ( j = 0; j < numindexes; j++ )
		{
			if ( j == i ) {
				continue;
			}

			if ( (ptempindex[i] == ptempindex[j]) && (ptempstindex[i] == ptempstindex[j]) ) {
				indremap[j] = i;
			}
		}
	}

	// count unique vertexes
	for ( i = 0; i < numindexes; i++ )
	{
		if ( indremap[i] != -1 ) {
			continue;
		}

		indexes[i] = numverts++;
		indremap[i] = i;
	}

	Con_DPrintf ( "%s: remapped %i verts to %i\n", mod->name, pq2inmodel->num_xyz, numverts );

	galias->numverts = numverts;

	// remap remaining indexes
	for ( i = 0; i < numindexes; i++ ) 
	{
		if ( indremap[i] != i ) {
			indexes[i] = indexes[indremap[i]];
		}
	}

// s and t vertices
	pinstverts = ( dmd2stvert_t * ) ( ( qbyte * )pq2inmodel + LittleLong (pq2inmodel->ofs_st) );
	st_array = Hunk_Alloc(sizeof(*st_array)*(numverts));
	galias->ofs_st_array = (char *)st_array - (char *)galias;

	for (j=0 ; j<numindexes; j++)
	{
		st_array[indexes[j]][0] = (float)(((double)LittleShort (pinstverts[ptempstindex[indremap[j]]].s) + 0.5f) /pq2inmodel->skinwidth);
		st_array[indexes[j]][1] = (float)(((double)LittleShort (pinstverts[ptempstindex[indremap[j]]].t) + 0.5f) /pq2inmodel->skinheight);
	}

	//frames
	ClearBounds ( mod->mins, mod->maxs );

	poutframe = (galiasgroup_t*)((char *)galias + galias->groupofs);
	framesize = LittleLong (pq2inmodel->framesize);
	for (i=0 ; i<pq2inmodel->num_frames ; i++)
	{
		pose = (galiaspose_t *)Hunk_Alloc(sizeof(galiaspose_t) + sizeof(vec3_t)*2*numverts);
		poutframe->poseofs = (char *)pose - (char *)poutframe;
		poutframe->numposes = 1;
		galias->groups++;

		verts = (vec3_t *)(pose+1);
		normals = &verts[galias->numverts];
		pose->ofsverts = (char *)verts - (char *)pose;
		pose->ofsnormals = (char *)normals - (char *)pose;


		pinframe = ( dmd2aliasframe_t * )( ( qbyte * )pq2inmodel + LittleLong (pq2inmodel->ofs_frames) + i * framesize );

		for (j=0 ; j<3 ; j++)
		{
			pose->scale[j] = LittleFloat (pinframe->scale[j]);
			pose->scale_origin[j] = LittleFloat (pinframe->translate[j]);
		}

		for (j=0 ; j<numindexes; j++)
		{
			// verts are all 8 bit, so no swapping needed
			verts[indexes[j]][0] = pose->scale_origin[0]+pose->scale[0]*pinframe->verts[ptempindex[indremap[j]]].v[0];
			verts[indexes[j]][1] = pose->scale_origin[1]+pose->scale[1]*pinframe->verts[ptempindex[indremap[j]]].v[1];
			verts[indexes[j]][2] = pose->scale_origin[2]+pose->scale[2]*pinframe->verts[ptempindex[indremap[j]]].v[2];
			VectorCopy(bytedirs[pinframe->verts[ptempindex[indremap[j]]].lightnormalindex], normals[indexes[j]]);
		}

//		Mod_AliasCalculateVertexNormals ( numindexes, poutindex, numverts, poutvertex, qfalse );

		VectorCopy ( pose->scale_origin, min );
		VectorMA ( pose->scale_origin, 255, pose->scale, max );

//		poutframe->radius = RadiusFromBounds ( min, max );

//		mod->radius = max ( mod->radius, poutframe->radius );
		AddPointToBounds ( min, mod->mins, mod->maxs );
		AddPointToBounds ( max, mod->mins, mod->maxs );

//		GL_GenerateNormals((float*)verts, (float*)normals, indexes, numindexes/3, numverts);

		poutframe++;
	}




	if (r_shadows.value)
	{
		int *neighbours;
		neighbours = Hunk_Alloc(sizeof(int)*3*pq2inmodel->num_tris);
		galias->ofs_trineighbours = (qbyte *)neighbours - (qbyte *)galias;
		R_BuildTriangleNeighbours(neighbours, indexes, pq2inmodel->num_tris);
	}

	/*
	VectorCopy (pq2inmodel->scale_origin, mod->mins);
	VectorMA (mod->mins, 255, pq2inmodel->scale, mod->maxs);
	*/
//
// move the complete, relocatable alias model to the cache
//	
	hunkend = Hunk_LowMark ();
	Hunk_Alloc(0);
	hunktotal = hunkend - hunkstart;
	
	Cache_Alloc (&mod->cache, hunktotal, loadname);
	mod->type = mod_alias;
	if (!mod->cache.data)
	{
		Hunk_FreeToLowMark (hunkstart);
		return;
	}
	memcpy (mod->cache.data, galias, hunktotal);

	Hunk_FreeToLowMark (hunkstart);

}

#endif




























#ifdef MD3MODELS

//structures from Tenebrae
typedef struct {
	int			ident;
	int			version;

	char		name[MAX_QPATH];

	int			flags;	//Does anyone know what these are?

	int			numFrames;
	int			numTags;			
	int			numSurfaces;

	int			numSkins;

	int			ofsFrames;
	int			ofsTags;
	int			ofsSurfaces;
	int			ofsEnd;
} md3Header_t;

//then has header->numFrames of these at header->ofs_Frames
typedef struct md3Frame_s {
	vec3_t		bounds[2];
	vec3_t		localOrigin;
	float		radius;
	char		name[16];
} md3Frame_t;

//there are header->numSurfaces of these at header->ofsSurfaces, following from ofsEnd
typedef struct {
	int		ident;				// 

	char	name[MAX_QPATH];	// polyset name

	int		flags;
	int		numFrames;			// all surfaces in a model should have the same

	int		numShaders;			// all surfaces in a model should have the same
	int		numVerts;

	int		numTriangles;
	int		ofsTriangles;

	int		ofsShaders;			// offset from start of md3Surface_t
	int		ofsSt;				// texture coords are common for all frames
	int		ofsXyzNormals;		// numVerts * numFrames

	int		ofsEnd;				// next surface follows
} md3Surface_t;

//at surf+surf->ofsXyzNormals
typedef struct {
	short		xyz[3];
	qbyte		latlong[2];
} md3XyzNormal_t;

//surf->numTriangles at surf+surf->ofsTriangles
typedef struct {
	int			indexes[3];
} md3Triangle_t;

//surf->numVerts at surf+surf->ofsSt
typedef struct {
	float		s;
	float		t;
} md3St_t;

typedef struct {
	char			name[MAX_QPATH];
	int				shaderIndex;
} md3Shader_t;
//End of Tenebrae 'assistance'

typedef struct {
	char name[MAX_QPATH];
	vec3_t org;
	float ang[3][3];
} md3tag_t;


void GL_LoadQ3Model(model_t *mod, void *buffer)
{
	extern qboolean gl_bumpmappingpossible;
	int hunkstart, hunkend, hunktotal;
//	int version;
	int s, i, j, d;

	index_t *indexes;

	vec3_t min;
	vec3_t max;

	galiaspose_t *pose;
	galiasinfo_t *parent, *root;
	galiasgroup_t *group;

	galiasskin_t	*skin;
	galiastexnum_t	*texnum;

	vec3_t *verts;
	vec3_t *normals;
	vec2_t *st_array;

	float lat, lng;

	md3St_t			*inst;
	md3Triangle_t	*intris;
	md3XyzNormal_t	*invert;
	md3Shader_t		*inshader;


	int size;

	md3Header_t		*header;
	md3Surface_t	*surf;


	loadmodel=mod;

	hunkstart = Hunk_LowMark ();

	header = buffer;

//	if (header->version != sdfs)
//		Sys_Error("GL_LoadQ3Model: Bad version\n");

	if (header->numSurfaces < 1)
	{
		mod->type = mod_alias;
		return;
	}


	parent = NULL;
	root = NULL;

	min[0] = min[1] = min[2] = 0;
	max[0] = max[1] = max[2] = 0;

	surf = (md3Surface_t *)((qbyte *)header + header->ofsSurfaces);
	for (s = 0; s < header->numSurfaces; s++)
	{
		size = sizeof(galiasinfo_t) + sizeof(galiasgroup_t)*header->numFrames;
		galias = Hunk_Alloc(size);
		galias->groupofs = sizeof(*galias);	//frame groups
		galias->groups = header->numFrames;
		galias->numverts = surf->numVerts;
		galias->numindexes = surf->numTriangles*3;
		galias->numskins = 1;
		if (parent)
			parent->nextsurf = (qbyte *)galias - (qbyte *)parent;
		else
			root = galias;
		parent = galias;

		st_array = Hunk_Alloc(sizeof(vec2_t)*galias->numindexes);
		galias->ofs_st_array = (qbyte*)st_array - (qbyte*)galias;

		inst = (md3St_t*)((qbyte*)surf + surf->ofsSt);
		for (i = 0; i < galias->numverts; i++)
		{
			st_array[i][0] = inst[i].s;
			st_array[i][1] = inst[i].t;
		}

		indexes = Hunk_Alloc(sizeof(*indexes)*galias->numindexes);
		galias->ofs_indexes = (qbyte*)indexes - (qbyte*)galias;
		intris = (md3Triangle_t *)((qbyte*)surf + surf->ofsTriangles);
		for (i = 0; i < surf->numTriangles; i++)
		{
			indexes[i*3+0] = intris[i].indexes[0];
			indexes[i*3+1] = intris[i].indexes[1];
			indexes[i*3+2] = intris[i].indexes[2];
		}

		group = (galiasgroup_t *)(galias+1);
		invert = (md3XyzNormal_t *)((qbyte*)surf + surf->ofsXyzNormals);
		for (i = 0; i < surf->numFrames; i++)
		{
			pose = (galiaspose_t *)Hunk_Alloc(sizeof(galiaspose_t) + sizeof(vec3_t)*2*surf->numVerts);
			normals = (vec3_t*)(pose+1);
			verts = normals + surf->numVerts;

			pose->ofsnormals = (qbyte*)normals - (qbyte*)pose;
			pose->ofsverts = (qbyte*)verts - (qbyte*)pose;

			for (j = 0; j < surf->numVerts; j++)
			{
				lat = (float)invert[j].latlong[0] * (2 * M_PI)*(1.0 / 255.0);
				lng = (float)invert[j].latlong[1] * (2 * M_PI)*(1.0 / 255.0);
				normals[j][0] = cos ( lng ) * sin ( lat );
				normals[j][1] = sin ( lng ) * sin ( lat );
				normals[j][2] = cos ( lat );

				for (d = 0; d < 3; d++)
				{
					verts[j][d] = invert[j].xyz[d]/64.0f;
					if (verts[j][d]<min[d])
						min[d] = verts[j][d];
					if (verts[j][d]>max[d])
						max[d] = verts[j][d];
				}
			}

			pose->scale[0] = 1;
			pose->scale[1] = 1;
			pose->scale[2] = 1;
			
			pose->scale_origin[0] = 0;
			pose->scale_origin[1] = 0;
			pose->scale_origin[2] = 0;

			group->numposes = 1;
			group->rate = 1;
			group->poseofs = (qbyte*)pose - (qbyte*)group;

			group++;
			invert += surf->numVerts;
		}

		if (surf->numShaders)
		{
#ifndef Q3SHADERS
			char name[1024];
#endif
			skin = Hunk_Alloc(surf->numShaders*(sizeof(galiasskin_t)+sizeof(galiastexnum_t)));
			galias->ofsskins = (qbyte *)skin - (qbyte *)galias;
			texnum = (galiastexnum_t *)(skin + surf->numShaders);
			inshader = (md3Shader_t *)((qbyte *)surf + surf->ofsShaders);
			for (i = 0; i < surf->numShaders; i++)
			{
				skin->texnums = 1;
				skin->ofstexnums = (qbyte *)texnum - (qbyte *)skin;
				skin->ofstexels = 0;
				skin->skinwidth = 0;
				skin->skinheight = 0;
				skin->skinspeed = 0;
#ifdef Q3SHADERS
				texnum->shader = R_RegisterSkin(inshader->name);
#else

				texnum->base = Mod_LoadHiResTexture(inshader->name, true, true, true);
				if (!texnum->base)
				{
					strcpy(name, loadmodel->name);
					strcpy(COM_SkipPath(name), COM_SkipPath(inshader->name));	//eviile eh?
					texnum->base = Mod_LoadHiResTexture(name, true, true, true);
				}

				texnum->bump = 0;
				if (gl_bumpmappingpossible)
				{
					COM_StripExtension(inshader->name, name);	//go for the normalmap
					strcat(name, "_norm");
					texnum->bump = Mod_LoadHiResTexture(name, true, true, false);
					if (!texnum->bump)
					{
						strcpy(name, loadmodel->name);
						COM_StripExtension(COM_SkipPath(inshader->name), COM_SkipPath(name));
						strcat(name, "_norm");
						texnum->bump = Mod_LoadHiResTexture(name, true, true, false);
						if (!texnum->bump)
						{
							COM_StripExtension(inshader->name, name);	//bother, go for heightmap and convert
							strcat(name, "_bump");
							texnum->bump = Mod_LoadBumpmapTexture(name);
							if (!texnum->bump)
							{
								strcpy(name, loadmodel->name);
								strcpy(COM_SkipPath(name), COM_SkipPath(inshader->name));	//eviile eh?
								COM_StripExtension(name, name);
								strcat(name, "_bump");
								texnum->bump = Mod_LoadBumpmapTexture(name);
							}
						}
					}
				}
				if (r_fb_models.value)
				{
					COM_StripExtension(inshader->name, name);	//go for the normalmap
					strcat(name, "_luma");
					texnum->fullbright = Mod_LoadHiResTexture(name, true, true, true);
					if (!texnum->base)
					{
						strcpy(name, loadmodel->name);
						strcpy(COM_SkipPath(name), COM_SkipPath(inshader->name));	//eviile eh?
						COM_StripExtension(name, name);
						strcat(name, "_luma");
						texnum->fullbright = Mod_LoadBumpmapTexture(name);
					}
				}
#endif

				skin++;
				texnum++;
			}
		}

		VectorCopy(min, loadmodel->mins);
		VectorCopy(max, loadmodel->maxs);



		if (r_shadows.value)
		{
			int *neighbours;
			neighbours = Hunk_Alloc(sizeof(int)*3*surf->numTriangles);
			galias->ofs_trineighbours = (qbyte *)neighbours - (qbyte *)galias;
			R_BuildTriangleNeighbours(neighbours, indexes, surf->numTriangles);
		}
		surf = (md3Surface_t *)((qbyte *)surf + surf->ofsEnd);
	}

//
// move the complete, relocatable alias model to the cache
//	

	hunkend = Hunk_LowMark ();

	mod->flags = Mod_ReadFlagsFromMD1(mod->name, 0);

	Hunk_Alloc(0);
	hunktotal = hunkend - hunkstart;
	
	Cache_Alloc (&mod->cache, hunktotal, loadname);
	mod->type = mod_alias;
	if (!mod->cache.data)
	{
		Hunk_FreeToLowMark (hunkstart);
		return;
	}
	memcpy (mod->cache.data, root, hunktotal);

	Hunk_FreeToLowMark (hunkstart);
}
#endif
