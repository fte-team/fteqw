
//a note about dedicated servers:
//In the server-side gamecode, a couple of q1 extensions require knowing something about models.
//So we load models serverside, if required.

//things we need:
//tag/bone names and indexes so we can have reasonable modding with tags. :)
//tag/bone positions so we can shoot from the actual gun or other funky stuff
//vertex positions so we can trace against the mesh rather than the bbox.

//we use the gl renderer's model code because it supports more sorts of models than the sw renderer. Sad but true.




#include "quakedef.h"
#ifdef RGLQUAKE
#include "glquake.h"
#endif
#if defined(RGLQUAKE) || defined(SERVERONLY)
#include "shader.h"
#include "hash.h"

#if defined(ZYMOTICMODELS) || defined(MD5MODELS)
#define SKELETALMODELS

#include "malloc.h"
#endif

#define MAX_BONES 256

static model_t *loadmodel;

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

typedef struct {
	int ofs_indexes;
	int numindexes;

	int ofs_trineighbours;

	int numskins;
#ifndef SERVERONLY
	int ofsskins;
#endif

	qboolean sharesverts;	//used with models with two shaders using the same vertex - use last mesh's verts
	qboolean sharesbones;	//use last mesh's bones (please, never set this on the first mesh!)

	int numverts;

#ifndef SERVERONLY
	int ofs_st_array;
#endif

	int groups;
	int groupofs;

	int nextsurf;

#ifdef SKELETALMODELS
	int numbones;
	int ofsbones;
	int numtransforms;
	int ofstransforms;
#endif

//these exist only in the root mesh.
	int numtagframes;
	int numtags;
	int ofstags;
} galiasinfo_t;

//frame is an index into this
typedef struct {
#ifdef SKELETALMODELS
	qboolean isheirachical;	//for models with transforms, states that bones need to be transformed from thier parent.
							//this is actually bad, and can result in bones shortening as they interpolate.
#endif
	qboolean loop;
	int numposes;
	float rate;
	int poseofs;
	char name[64];
} galiasgroup_t;

typedef struct {
	int ofsverts;
#ifndef SERVERONLY
	int ofsnormals;
#endif

	vec3_t		scale;
	vec3_t		scale_origin;
} galiaspose_t;

#ifdef SKELETALMODELS
typedef struct {
	char name[32];
	int parent;
} galiasbone_t;

typedef struct {
	//skeletal poses refer to this.
	int vertexindex;
	int boneindex;
	vec4_t org;
} galisskeletaltransforms_t;
#endif

//we can't be bothered with animating skins.
//We'll load up to four of them but after that you're on your own
#ifndef SERVERONLY
typedef struct {
	int skinwidth;
	int skinheight;
	int ofstexels;	//this is 8bit for frame 0 only. only valid in q1 models without replacement textures, used for colourising player skins.
	float skinspeed;
	int texnums;
	int ofstexnums;
	char name [MAX_QPATH];
} galiasskin_t;

typedef struct {
	int base;
	int bump;
	int fullbright;

#ifdef Q3SHADERS
	shader_t *shader;
#endif
} galiastexnum_t;

typedef struct {
	char name[MAX_QPATH];
	galiastexnum_t texnum;
	int colour;
	int skinnum;
	bucket_t bucket;
} galiascolourmapped_t;
#endif

#ifdef SKELETALMODELS
static void R_LerpBones(float *plerp, float **pose, int poses, galiasbone_t *bones, int bonecount, float bonepose[MAX_BONES][12]);
static void R_TransformVerticies(float bonepose[MAX_BONES][12], galisskeletaltransforms_t *weights, int numweights, float *xyzout);
#endif

void Mod_DoCRC(model_t *mod, char *buffer, int buffersize)
{
#ifndef SERVERONLY
	//we've got to have this bit
	if (loadmodel->engineflags & MDLF_DOCRC)
	{
		unsigned short crc;
		qbyte *p;
		int len;
		char st[40];

		QCRC_Init(&crc);
		for (len = buffersize, p = buffer; len; len--, p++)
			QCRC_ProcessByte(&crc, *p);

		sprintf(st, "%d", (int) crc);
		Info_SetValueForKey (cls.userinfo,
			(loadmodel->engineflags & MDLF_PLAYER) ? pmodel_name : emodel_name,
			st, MAX_INFO_STRING);

		if (cls.state >= ca_connected)
		{
			CL_SendClientCommand(true, "setinfo %s %d",
				(loadmodel->engineflags & MDLF_PLAYER) ? pmodel_name : emodel_name,
				(int)crc);
		}
	}
#endif
}
qboolean GLMod_Trace(model_t *model, int forcehullnum, int frame, vec3_t start, vec3_t end, vec3_t mins, vec3_t maxs, trace_t *trace)
{
	galiasinfo_t *mod = Mod_Extradata(model);
	galiasgroup_t *group;
	galiaspose_t *pose;
	int i;

	float *p1, *p2, *p3;
	vec3_t edge1, edge2, edge3;
	vec3_t normal;
	vec3_t edgenormal;

	float planedist;
	float diststart, distend;

	float frac;
//	float temp;

	vec3_t impactpoint;

	float *posedata;
	int *indexes;

	while(mod)
	{
		indexes = (int*)((char*)mod + mod->ofs_indexes);
		group = (galiasgroup_t*)((char*)mod + mod->groupofs);
		pose = (galiaspose_t*)((char*)&group[0] + group[0].poseofs);
		posedata = (float*)((char*)pose + pose->ofsverts);
#ifdef SKELETALMODELS
		if (mod->numbones && !mod->sharesverts)
		{
			float bonepose[MAX_BONES][12];
			posedata = alloca(mod->numverts*sizeof(vec3_t));
			frac = 1;
			if (group->isheirachical)
			{
				if (!mod->sharesbones)
					R_LerpBones(&frac, (float**)posedata, 1, (galiasbone_t*)((char*)mod + mod->ofsbones), mod->numbones, bonepose);
				R_TransformVerticies(bonepose, (galisskeletaltransforms_t*)((char*)mod + mod->ofstransforms), mod->numtransforms, posedata);
			}
			else
				R_TransformVerticies((void*)posedata, (galisskeletaltransforms_t*)((char*)mod + mod->ofstransforms), mod->numtransforms, posedata);
		}
#endif

		for (i = 0; i < mod->numindexes; i+=3)
		{
			p1 = posedata + 3*indexes[i+0];
			p2 = posedata + 3*indexes[i+1];
			p3 = posedata + 3*indexes[i+2];

			VectorSubtract(p1, p2, edge1);
			VectorSubtract(p3, p2, edge2);
			CrossProduct(edge1, edge2, normal);

			planedist = DotProduct(p1, normal);
			diststart = DotProduct(start, normal);
			if (diststart <= planedist)
				continue;	//start on back side.
			distend = DotProduct(end, normal);
			if (distend >= planedist)
				continue;	//end on front side (as must start - doesn't cross).

			frac = (diststart - planedist) / (diststart-distend);

			if (frac >= trace->fraction)	//already found one closer.
				continue;

			impactpoint[0] = start[0] + frac*(end[0] - start[0]);
			impactpoint[1] = start[1] + frac*(end[1] - start[1]);
			impactpoint[2] = start[2] + frac*(end[2] - start[2]);

//			temp = DotProduct(impactpoint, normal)-planedist;

			CrossProduct(edge1, normal, edgenormal);
//			temp = DotProduct(impactpoint, edgenormal)-DotProduct(p2, edgenormal);
			if (DotProduct(impactpoint, edgenormal) > DotProduct(p2, edgenormal))
				continue;

			CrossProduct(normal, edge2, edgenormal);
			if (DotProduct(impactpoint, edgenormal) > DotProduct(p3, edgenormal))
				continue;

			VectorSubtract(p1, p3, edge3);
			CrossProduct(normal, edge3, edgenormal);
			if (DotProduct(impactpoint, edgenormal) > DotProduct(p1, edgenormal))
				continue;

			trace->fraction = frac;
			VectorCopy(impactpoint, trace->endpos);
			VectorCopy(normal, trace->plane.normal);
		}

		if (mod->nextsurf)
			mod = (galiasinfo_t*)((char*)mod + mod->nextsurf);
		else
			mod = NULL;
	}

	trace->allsolid = false;

	return trace->fraction != 1;
}

#ifndef SERVERONLY
static hashtable_t skincolourmapped;

static vec3_t shadevector;
static vec3_t shadelight, ambientlight;
static void R_LerpFrames(mesh_t *mesh, galiaspose_t *p1, galiaspose_t *p2, float lerp, qbyte alpha, float expand, qboolean nolightdir)
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
		mesh->xyz_array = p1v;
		if (r_nolightdir.value || nolightdir)
		{
			mesh->colors_array = NULL;
		}
		else
		{
			for (i = 0; i < mesh->numvertexes; i++)
			{
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
		if (r_nolightdir.value || nolightdir)
		{
			mesh->colors_array = NULL;
			for (i = 0; i < mesh->numvertexes; i++)
			{
				mesh->normals_array[i][0] = p1n[i][0]*lerp + p2n[i][0]*blerp;
				mesh->normals_array[i][1] = p1n[i][1]*lerp + p2n[i][1]*blerp;
				mesh->normals_array[i][2] = p1n[i][2]*lerp + p2n[i][2]*blerp;

				mesh->xyz_array[i][0] = p1v[i][0]*lerp + p2v[i][0]*blerp;
				mesh->xyz_array[i][1] = p1v[i][1]*lerp + p2v[i][1]*blerp;
				mesh->xyz_array[i][2] = p1v[i][2]*lerp + p2v[i][2]*blerp;
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
		if (mesh->xyz_array == p1v)
		{
			mesh->xyz_array = tempVertexCoords;
			for (i = 0; i < mesh->numvertexes; i++)
			{
				mesh->xyz_array[i][0] = p1v[i][0] + mesh->normals_array[i][0]*expand;
				mesh->xyz_array[i][1] = p1v[i][1] + mesh->normals_array[i][1]*expand;
				mesh->xyz_array[i][2] = p1v[i][2] + mesh->normals_array[i][2]*expand;
			}

		}
		else
		{
			for (i = 0; i < mesh->numvertexes; i++)
			{
				mesh->xyz_array[i][0] += mesh->normals_array[i][0]*expand;
				mesh->xyz_array[i][1] += mesh->normals_array[i][1]*expand;
				mesh->xyz_array[i][2] += mesh->normals_array[i][2]*expand;
			}
		}
	}
}
#endif
#ifdef SKELETALMODELS
static void R_LerpBones(float *plerp, float **pose, int poses, galiasbone_t *bones, int bonecount, float bonepose[MAX_BONES][12])
{
	int i, k, b;
	float *matrix, m[12];

	if (poses == 1)
	{
		// vertex weighted skeletal
		// interpolate matrices and concatenate them to their parents
		for (i = 0;i < bonecount;i++)
		{
			matrix = pose[0] + i*12;

			if (bones[i].parent >= 0)
				R_ConcatTransforms((void*)bonepose[bones[i].parent], (void*)matrix, (void*)bonepose[i]);
			else
				for (k = 0;k < 12;k++)	//parentless
					bonepose[i][k] = matrix[k];
		}
	}
	else
	{
		// vertex weighted skeletal
		// interpolate matrices and concatenate them to their parents
		for (i = 0;i < bonecount;i++)
		{
			for (k = 0;k < 12;k++)
				m[k] = 0;
			for (b = 0;b < poses;b++)
			{
				matrix = pose[b] + i*12;

				for (k = 0;k < 12;k++)
					m[k] += matrix[k] * plerp[b];
			}
			if (bones[i].parent >= 0)
				R_ConcatTransforms((void*)bonepose[bones[i].parent], (void*)m, (void*)bonepose[i]);
			else
				for (k = 0;k < 12;k++)	//parentless
					bonepose[i][k] = m[k];
		}
	}
}
static void R_TransformVerticies(float bonepose[MAX_BONES][12], galisskeletaltransforms_t *weights, int numweights, float *xyzout)
{
	int i;
	float *out, *matrix;

	galisskeletaltransforms_t *v = weights;
	for (i = 0;i < numweights;i++, v++)
	{
		out = xyzout + v->vertexindex * 3;
		matrix = bonepose[v->boneindex];
		// FIXME: this can very easily be optimized with SSE or 3DNow
		out[0] += v->org[0] * matrix[0] + v->org[1] * matrix[1] + v->org[2] * matrix[ 2] + v->org[3] * matrix[ 3];
		out[1] += v->org[0] * matrix[4] + v->org[1] * matrix[5] + v->org[2] * matrix[ 6] + v->org[3] * matrix[ 7];
		out[2] += v->org[0] * matrix[8] + v->org[1] * matrix[9] + v->org[2] * matrix[10] + v->org[3] * matrix[11];
	}
}
#ifndef SERVERONLY
static void R_BuildSkeletalMesh(mesh_t *mesh, float *plerp, float **pose, int poses, galiasbone_t *bones, int bonecount, galisskeletaltransforms_t *weights, int numweights, qboolean usehierarchy)
{
	float bonepose[MAX_BONES][12];

	int i, k, l;

	if (usehierarchy)
		R_LerpBones(plerp, pose, poses, bones, bonecount, bonepose);
	else
	{
		if (poses == 1)
			memcpy(bonepose, pose[0], sizeof(float)*12*bonecount);
		else if (poses == 2)
		{
			for (i = 0; i < bonecount*12; i++)
			{
				((float*)bonepose)[i] = pose[0][i]*plerp[0] + pose[1][i]*plerp[1];
			}
		}
		else
		{
			for (i = 0; i < bonecount; i++)
			{
				for (l = 0; l < 12; l++)
					bonepose[i][l] = 0;
				for (k = 0; k < poses; k++)
				{
					for (l = 0; l < 12; l++)
						bonepose[i][l] += pose[k][i*12+l] * plerp[k];
				}
			}
		}
	}

	// blend the vertex bone weights
//	memset(outhead, 0, mesh->numvertexes * sizeof(mesh->xyz_array[0]));

	for (i = 0; i < mesh->numvertexes; i++)
	{
		mesh->normals_array[i][0] = 0;
		mesh->normals_array[i][1] = 0;
		mesh->normals_array[i][2] = 1;
/*
		mesh->colors_array[i][0] = ambientlight[0];
		mesh->colors_array[i][1] = ambientlight[1];
		mesh->colors_array[i][2] = ambientlight[2];
		mesh->colors_array[i][3] = 255;//alpha;
*/
/*
		mesh->xyz_array[i][0] = 0;
		mesh->xyz_array[i][1] = 0;
		mesh->xyz_array[i][2] = 0;
		mesh->xyz_array[i][3] = 1;
		*/
	}
	mesh->colors_array = NULL;

	memset(mesh->xyz_array, 0, mesh->numvertexes*sizeof(vec3_t));
	R_TransformVerticies(bonepose, weights, numweights, (float*)mesh->xyz_array);




#if 0	//draws the bones
	qglColor3f(1, 0, 0);
	{
		int i;
		int p;
		vec3_t org, dest;

		qglBegin(GL_LINES);
		for (i = 0; i < bonecount; i++)
		{
			p = bones[i].parent;
			if (p < 0)
				p = 0;
			qglVertex3f(bonepose[i][3], bonepose[i][7], bonepose[i][11]);
			qglVertex3f(bonepose[p][3], bonepose[p][7], bonepose[p][11]);
		}
		qglEnd();
		qglBegin(GL_LINES);
		for (i = 0; i < bonecount; i++)
		{
			p = bones[i].parent;
			if (p < 0)
				p = 0;
			org[0] = bonepose[i][3]; org[1] = bonepose[i][7]; org[2] = bonepose[i][11];
			qglVertex3fv(org);
			qglVertex3f(bonepose[p][3], bonepose[p][7], bonepose[p][11]);
			dest[0] = org[0]+bonepose[i][0];dest[1] = org[1]+bonepose[i][1];dest[2] = org[2]+bonepose[i][2];
			qglVertex3fv(org);
			qglVertex3fv(dest);
			qglVertex3fv(dest);
			qglVertex3f(bonepose[p][3], bonepose[p][7], bonepose[p][11]);
			dest[0] = org[0]+bonepose[i][4];dest[1] = org[1]+bonepose[i][5];dest[2] = org[2]+bonepose[i][6];
			qglVertex3fv(org);
			qglVertex3fv(dest);
			qglVertex3fv(dest);
			qglVertex3f(bonepose[p][3], bonepose[p][7], bonepose[p][11]);
			dest[0] = org[0]+bonepose[i][8];dest[1] = org[1]+bonepose[i][9];dest[2] = org[2]+bonepose[i][10];
			qglVertex3fv(org);
			qglVertex3fv(dest);
			qglVertex3fv(dest);
			qglVertex3f(bonepose[p][3], bonepose[p][7], bonepose[p][11]);
		}
		qglEnd();

//		mesh->numindexes = 0;	//don't draw this mesh, as that would obscure the bones. :(
	}
#endif
}
#endif
#endif

#ifndef SERVERONLY

void R_LightArrays(byte_vec4_t *colours, int vertcount, vec3_t *normals)
{
	int i;
	float l;
	int temp;

	for (i = vertcount-1; i >= 0; i--)
	{
		l = DotProduct(normals[i], shadevector);

		temp = l*ambientlight[0]+shadelight[0];
		if (temp < 0) temp = 0;
		else if (temp > 255) temp = 255;
		colours[i][0] = temp;

		temp = l*ambientlight[1]+shadelight[1];
		if (temp < 0) temp = 0;
		else if (temp > 255) temp = 255;
		colours[i][1] = temp;

		temp = l*ambientlight[2]+shadelight[2];
		if (temp < 0) temp = 0;
		else if (temp > 255) temp = 255;
		colours[i][2] = temp;
	}
}

//changes vertex lighting values
static void R_GAliasApplyLighting(mesh_t *mesh, vec3_t org, vec3_t angles, float *colormod)
{
	int l, v;
	vec3_t rel;
	vec3_t dir;
	float dot, d, a, f;

	if (mesh->colors_array)
	{
		float l;
		int temp;
		int i;
		byte_vec4_t *colours = mesh->colors_array;
		vec3_t *normals = mesh->normals_array;
		vec3_t ambient, shade;
		qbyte alphab = bound(0, colormod[3]*255, 255);
		if (!mesh->normals_array)
		{
			mesh->colors_array = NULL;
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
			if (temp < 0) temp = 0;
			else if (temp > 255) temp = 255;
			colours[i][0] = temp;

			temp = l*ambient[1]+shade[1];
			if (temp < 0) temp = 0;
			else if (temp > 255) temp = 255;
			colours[i][1] = temp;

			temp = l*ambient[2]+shade[2];
			if (temp < 0) temp = 0;
			else if (temp > 255) temp = 255;
			colours[i][2] = temp;

			colours[i][3] = alphab;
		}
	}

	if (r_vertexdlights.value && mesh->colors_array)
	{
		for (l=0 ; l<dlights_running ; l++)
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
}

static qboolean R_GAliasBuildMesh(mesh_t *mesh, galiasinfo_t *inf, int frame1, int frame2, float lerp, float alpha, float fg1time, float fg2time, qboolean nolightdir)
{
	galiasgroup_t *g1, *g2;

	if (!inf->groups)
	{
		Con_DPrintf("Model with no frames (%s)\n", currententity->model->name);
		return false;
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
		frame1 %= inf->groups;
	}
	if (frame2 >= inf->groups)
	{
		Con_DPrintf("Too high frame %i (%s)\n", frame2, currententity->model->name);
		frame2 = frame1;
	}

	if (lerp <= 0)
		frame2 = frame1;
	else  if (lerp >= 1)
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

	mesh->numvertexes = inf->numverts;
	mesh->indexes = (index_t*)((char *)inf + inf->ofs_indexes);
	mesh->numindexes = inf->numindexes;

	if (inf->sharesverts)
		return false;	//don't generate the new vertex positions. We still have them all.

#ifndef SERVERONLY
	mesh->st_array = (vec2_t*)((char *)inf + inf->ofs_st_array);
	mesh->lmst_array = NULL;
	mesh->colors_array = tempColours;
	mesh->trneighbors = (int *)((char *)inf + inf->ofs_trineighbours);
	mesh->normals_array = tempNormals;
#endif
	mesh->xyz_array = tempVertexCoords;

	g1 = (galiasgroup_t*)((char *)inf + inf->groupofs + sizeof(galiasgroup_t)*frame1);
	g2 = (galiasgroup_t*)((char *)inf + inf->groupofs + sizeof(galiasgroup_t)*frame2);

//we don't support meshes with one pose skeletal and annother not.
//we don't support meshes with one group skeletal and annother not.

#ifdef SKELETALMODELS
	if (inf->numbones)
	{
		int l=0;
		float plerp[4];
		float *pose[4];
		float mlerp;	//minor lerp, poses within a group.
		qboolean hirachy;

		if (g1->isheirachical != g2->isheirachical || lerp < 0)
			lerp = 0;
		hirachy = g1->isheirachical;

		mlerp = (fg1time)*g1->rate;
		frame1=mlerp;
		frame2=frame1+1;
		mlerp-=frame1;
		if (g1->loop)
		{
			frame1=frame1%g1->numposes;
			frame2=frame2%g1->numposes;
		}
		else
		{
			frame1=(frame1>g1->numposes-1)?g1->numposes-1:frame1;
			frame2=(frame2>g1->numposes-1)?g1->numposes-1:frame2;
		}

		plerp[l] = (1-mlerp)*(1-lerp);
		if (plerp[l]>0)
			pose[l++] = (float *)((char *)g1 + g1->poseofs + sizeof(float)*inf->numbones*12*frame1);
		plerp[l] = (mlerp)*(1-lerp);
		if (plerp[l]>0)
			pose[l++] = (float *)((char *)g1 + g1->poseofs + sizeof(float)*inf->numbones*12*frame2);

		if (lerp)
		{
			mlerp = (fg2time)*g2->rate;
			frame1=mlerp;
			frame2=frame1+1;
			mlerp-=frame1;
			if (g2->loop)
			{
				frame1=frame1%g2->numposes;
				frame2=frame2%g2->numposes;
			}
			else
			{
				frame1=(frame1>g2->numposes-1)?g2->numposes-1:frame1;
				frame2=(frame2>g2->numposes-1)?g2->numposes-1:frame2;
			}

			plerp[l] = (1-mlerp)*(lerp);
			if (plerp[l]>0)
				pose[l++] = (float *)((char *)g2 + g2->poseofs + sizeof(float)*inf->numbones*12*frame1);
			plerp[l] = (mlerp)*(lerp);
			if (plerp[l]>0)
				pose[l++] = (float *)((char *)g2 + g2->poseofs + sizeof(float)*inf->numbones*12*frame2);
		}
/*
		pose[0] = (float *)((char *)g1 + g1->poseofs);
		plerp[0] = 1;
		plerp[1] = 0;
		plerp[3] = 0;
		plerp[4] = 0;
		l = 1;
*/
		R_BuildSkeletalMesh(mesh, plerp, pose, l, (galiasbone_t *)((char*)inf+inf->ofsbones), inf->numbones, (galisskeletaltransforms_t *)((char*)inf+inf->ofstransforms), inf->numtransforms, hirachy);
		return false;
	}
#endif

	if (g1 == g2)	//lerping within group is only done if not changing group
	{
		lerp = fg1time*g1->rate;
		if (lerp < 0) lerp = 0;	//hrm
		frame1=lerp;
		frame2=frame1+1;
		lerp-=frame1;
		if (g1->loop)
		{
			frame1=frame1%g1->numposes;
			frame2=frame2%g1->numposes;
		}
		else
		{
			frame1=(frame1>g1->numposes-1)?g1->numposes-1:frame1;
			frame2=(frame2>g1->numposes-1)?g1->numposes-1:frame2;
		}
	}
	else	//don't bother with a four way lerp. Yeah, this will produce jerkyness with models with just framegroups.
	{
		frame1=0;
		frame2=0;
	}

	R_LerpFrames(mesh,	(galiaspose_t *)((char *)g1 + g1->poseofs + sizeof(galiaspose_t)*frame1),
						(galiaspose_t *)((char *)g2 + g2->poseofs + sizeof(galiaspose_t)*frame2),
						1-lerp, (qbyte)(alpha*255), currententity->fatness, nolightdir);

	return true;	//to allow the mesh to be dlighted.
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

static galiastexnum_t *GL_ChooseSkin(galiasinfo_t *inf, char *modelname, int surfnum, entity_t *e)
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
			tc = e->scoreboard->topcolor;
			bc = e->scoreboard->bottomcolor;

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
			int cc;
			galiascolourmapped_t *cm;
			char hashname[512];
			cc = (tc<<4)|bc;

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
				Hash_InitTable(&skincolourmapped, 256, BZ_Malloc(Hash_BytesForBuckets(256)));

			for (cm = Hash_Get(&skincolourmapped, skinname); cm; cm = Hash_GetNext(&skincolourmapped, skinname, cm))
			{
				if (cm->colour == cc && cm->skinnum == e->skinnum)
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
					return NULL;
				if (e->skinnum >= 0 && e->skinnum < inf->numskins)
					skins += e->skinnum;
				texnums = (galiastexnum_t*)((char *)skins + skins->ofstexnums);
			}

			//colourmap isn't present yet.
			cm = BZ_Malloc(sizeof(*cm));
			Q_strncpyz(cm->name, skinname, sizeof(cm->name));
			Hash_Add(&skincolourmapped, cm->name, cm, &cm->bucket);
			cm->colour = cc;
			cm->skinnum = e->skinnum;
			cm->texnum.fullbright = 0;
			cm->texnum.base = 0;

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
				qglTexImage2D (GL_TEXTURE_2D, 0, gl_solid_format, scaled_width, scaled_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);

				qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
				qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);


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
				texnums->fullbright = texture_extension_number++;
				GL_Bind(texnums->fullbright);
				qglTexImage2D (GL_TEXTURE_2D, 0, gl_alpha_format, scaled_width, scaled_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);

				qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
				qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
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
	vec3_t *input = mesh->xyz_array;
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
	vec3_t *verts = mesh->xyz_array;
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

void GL_DrawAliasMesh_Sketch (mesh_t *mesh)
{
	int i;
	extern int gldepthfunc;
#ifdef Q3SHADERS
	R_UnlockArrays();
#endif

	qglDepthFunc(gldepthfunc);
	qglDepthMask(1);

	if (gldepthmin == 0.5)
		qglCullFace ( GL_BACK );
	else
		qglCullFace ( GL_FRONT );

	GL_TexEnv(GL_MODULATE);

	qglDisable(GL_TEXTURE_2D);

	qglVertexPointer(3, GL_FLOAT, 0, mesh->xyz_array);
	qglEnableClientState( GL_VERTEX_ARRAY );

	if (mesh->normals_array && qglNormalPointer)	//d3d wrapper doesn't support normals, and this is only really needed for truform
	{
		qglNormalPointer(GL_FLOAT, 0, mesh->normals_array);
		qglEnableClientState( GL_NORMAL_ARRAY );
	}

	qglColor3f(1,1,1);
/*	if (mesh->colors_array)
	{
		qglColorPointer(4, GL_UNSIGNED_BYTE, 0, mesh->colors_array);
		qglEnableClientState( GL_COLOR_ARRAY );
	}
	else
*/		qglDisableClientState( GL_COLOR_ARRAY );

	qglDrawElements(GL_TRIANGLES, mesh->numindexes, GL_UNSIGNED_INT, mesh->indexes);

	qglDisableClientState( GL_VERTEX_ARRAY );
	qglDisableClientState( GL_COLOR_ARRAY );
	qglDisableClientState( GL_NORMAL_ARRAY );

	if (mesh->colors_array)
		qglColor4ub(0, 0, 0, mesh->colors_array[0][3]);
	else
		qglColor3f(0, 0, 0);
	qglBegin(GL_LINES);
	for (i = 0; i < mesh->numindexes; i+=3)
	{
		float *v1, *v2, *v3;
		int n;
		v1 = mesh->xyz_array[mesh->indexes[i+0]];
		v2 = mesh->xyz_array[mesh->indexes[i+1]];
		v3 = mesh->xyz_array[mesh->indexes[i+2]];
		for (n = 0; n < 3; n++)	//rember we do this triangle AND the neighbours
		{
			qglVertex3f(v1[0]+0.5*(rand()/(float)RAND_MAX-0.5),
						v1[1]+0.5*(rand()/(float)RAND_MAX-0.5),
						v1[2]+0.5*(rand()/(float)RAND_MAX-0.5));
			qglVertex3f(v2[0]+0.5*(rand()/(float)RAND_MAX-0.5),
						v2[1]+0.5*(rand()/(float)RAND_MAX-0.5),
						v2[2]+0.5*(rand()/(float)RAND_MAX-0.5));

			qglVertex3f(v2[0]+0.5*(rand()/(float)RAND_MAX-0.5),
						v2[1]+0.5*(rand()/(float)RAND_MAX-0.5),
						v2[2]+0.5*(rand()/(float)RAND_MAX-0.5));
			qglVertex3f(v3[0]+0.5*(rand()/(float)RAND_MAX-0.5),
						v3[1]+0.5*(rand()/(float)RAND_MAX-0.5),
						v3[2]+0.5*(rand()/(float)RAND_MAX-0.5));

			qglVertex3f(v3[0]+0.5*(rand()/(float)RAND_MAX-0.5),
						v3[1]+0.5*(rand()/(float)RAND_MAX-0.5),
						v3[2]+0.5*(rand()/(float)RAND_MAX-0.5));
			qglVertex3f(v1[0]+0.5*(rand()/(float)RAND_MAX-0.5),
						v1[1]+0.5*(rand()/(float)RAND_MAX-0.5),
						v1[2]+0.5*(rand()/(float)RAND_MAX-0.5));
		}
	}
	qglEnd();

#ifdef Q3SHADERS
	R_IBrokeTheArrays();
#endif
}

//called from sprite code.
/*
void GL_KnownState(void)
{
	extern int gldepthfunc;
	qglDepthFunc(gldepthfunc);
	qglDepthMask(1);
	if (gldepthmin == 0.5)
		qglCullFace ( GL_BACK );
	else
		qglCullFace ( GL_FRONT );

	GL_TexEnv(GL_MODULATE);

	qglEnable (GL_BLEND);
	qglBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}
*/

void GL_DrawAliasMesh (mesh_t *mesh, int texnum)
{
	extern int gldepthfunc;
#ifdef Q3SHADERS
	R_UnlockArrays();
#endif

	qglDepthFunc(gldepthfunc);
	qglDepthMask(1);

	GL_Bind(texnum);
	if (gldepthmin == 0.5)
		qglCullFace ( GL_BACK );
	else
		qglCullFace ( GL_FRONT );

	GL_TexEnv(GL_MODULATE);

	qglVertexPointer(3, GL_FLOAT, 0, mesh->xyz_array);
	qglEnableClientState( GL_VERTEX_ARRAY );

	if (mesh->normals_array && qglNormalPointer)	//d3d wrapper doesn't support normals, and this is only really needed for truform
	{
		qglNormalPointer(GL_FLOAT, 0, mesh->normals_array);
		qglEnableClientState( GL_NORMAL_ARRAY );
	}

	if (mesh->colors_array)
	{
		qglColorPointer(4, GL_UNSIGNED_BYTE, 0, mesh->colors_array);
		qglEnableClientState( GL_COLOR_ARRAY );
	}
	else
		qglDisableClientState( GL_COLOR_ARRAY );

	qglEnableClientState( GL_TEXTURE_COORD_ARRAY );
	qglTexCoordPointer(2, GL_FLOAT, 0, mesh->st_array);

	qglDrawRangeElements(GL_TRIANGLES, 0, mesh->numvertexes, mesh->numindexes, GL_UNSIGNED_INT, mesh->indexes);

	qglDisableClientState( GL_VERTEX_ARRAY );
	qglDisableClientState( GL_COLOR_ARRAY );
	qglDisableClientState( GL_NORMAL_ARRAY );
	qglDisableClientState( GL_TEXTURE_COORD_ARRAY );

#ifdef Q3SHADERS
	R_IBrokeTheArrays();
#endif
}

qboolean R_CullEntityBox(entity_t *e, vec3_t modmins, vec3_t modmaxs)
{
	int i;
	vec3_t wmin, wmax;
	float fmin, fmax;

	//convert the model's bbox to the expanded maximum size of the entity, as drawn with this model.
	//The result is an axial box, which we pass to R_CullBox

	for (i = 0; i < 3; i++)
	{
		fmin = DotProduct(modmins, e->axis[i]);
		fmax = DotProduct(modmaxs, e->axis[i]);

		if (fmin > -16)
			fmin = -16;
		if (fmax < 16)
			fmax = 16;

		if (fmin < fmax)
		{
			wmin[i] = e->origin[i]+fmin;
			wmax[i] = e->origin[i]+fmax;
		}
		else
		{       //box went inside out
			wmin[i] = e->origin[i]+fmax;
			wmax[i] = e->origin[i]+fmin;
		}
	}


	return R_CullBox(wmin, wmax);
}

#ifdef Q3SHADERS
mfog_t *CM_FogForOrigin(vec3_t org);
#endif
void R_DrawGAliasModel (entity_t *e)
{
	extern cvar_t r_drawflat;
	model_t *clmodel;
	vec3_t dist;
	vec_t add;
	int i;
	galiasinfo_t *inf;
	mesh_t mesh;
	galiastexnum_t *skin;
	float entScale;
	vec3_t lightdir;

	vec3_t saveorg;
#ifdef Q3SHADERS
	mfog_t *fog;
#endif
	int surfnum;

	float	tmatrix[3][4];

	qboolean needrecolour;
	qboolean nolightdir;

	currententity = e;

//	if (e->flags & Q2RF_VIEWERMODEL && e->keynum == cl.playernum[r_refdef.currentplayernum]+1)
//		return;

	if (r_secondaryview && e->flags & Q2RF_WEAPONMODEL)
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

	if (!(e->flags & Q2RF_WEAPONMODEL))
		if (R_CullEntityBox (e, clmodel->mins, clmodel->maxs))
			return;

	if (!(r_refdef.flags & Q2RDF_NOWORLDMODEL))
	{
		if (e->flags & Q2RF_WEAPONMODEL)
			cl.worldmodel->funcs.LightPointValues(r_refdef.vieworg, shadelight, ambientlight, lightdir);
		else
			cl.worldmodel->funcs.LightPointValues(e->origin, shadelight, ambientlight, lightdir);
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
	if (clmodel->engineflags & MDLF_FLAME)
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

		VectorCopy(shadevector, mesh.lightaxis[2]);
		VectorVectors(mesh.lightaxis[2], mesh.lightaxis[1], mesh.lightaxis[0]);
		VectorInverse(mesh.lightaxis[1]);
	}

	if (e->flags & Q2RF_GLOW)
	{
		shadelight[0] += sin(cl.time)*0.25;
		shadelight[1] += sin(cl.time)*0.25;
		shadelight[2] += sin(cl.time)*0.25;
	}

/*
	VectorClear(ambientlight);
	VectorClear(shadelight);
*/

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
		qglShadeModel (GL_SMOOTH);
	if (gl_affinemodels.value)
		qglHint (GL_PERSPECTIVE_CORRECTION_HINT, GL_FASTEST);

	qglDisable (GL_ALPHA_TEST);

	if (e->flags & Q2RF_DEPTHHACK)
		qglDepthRange (gldepthmin, gldepthmin + 0.3*(gldepthmax-gldepthmin));

//	glColor3f( 1,1,1);
	if (e->flags & Q2RF_ADDATIVE)
	{
		qglEnable (GL_BLEND);
		qglBlendFunc(GL_ONE, GL_ONE);
	}
	else if ((e->model->flags & EF_SPECIAL_TRANS))	//hexen2 flags.
	{
		qglEnable (GL_BLEND);
		qglBlendFunc (GL_ONE_MINUS_SRC_ALPHA, GL_SRC_ALPHA);
//		glColor3f( 1,1,1);
		qglDisable( GL_CULL_FACE );
	}
	else if (e->drawflags & DRF_TRANSLUCENT)
	{
		qglEnable (GL_BLEND);
		qglBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		e->shaderRGBAf[3] = r_wateralpha.value;
	}
	else if ((e->model->flags & EF_TRANSPARENT))
	{
		qglEnable (GL_BLEND);
		qglBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	}
	else if ((e->model->flags & EF_HOLEY))
	{
		qglEnable (GL_ALPHA_TEST);
//		qglEnable (GL_BLEND);
		qglBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	}
	else if (e->shaderRGBAf[3] < 1)
	{
		qglEnable(GL_BLEND);
		qglBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	}
	else
	{
		qglDisable(GL_BLEND);
		qglBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	}
	//	qglEnable (GL_ALPHA_TEST);

	qglPushMatrix();
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

/*		qglScalef(	1/scale[0],
					1/scale[1],
					1/scale[2]);
		qglTranslatef (	-scale_origin[0],
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

		qglTranslatef (tmatrix[0][3],tmatrix[1][3],tmatrix[2][3]);
		qglScalef (tmatrix[0][0],tmatrix[1][1],tmatrix[2][2]);

		qglScalef(	1/scale[0],
					1/scale[1],
					1/scale[2]);
		qglTranslatef (	-scale_origin[0],
						-scale_origin[1],
						-scale_origin[2]);
	}

	inf = GLMod_Extradata (clmodel);
	if (qglPNTrianglesfATI && gl_ati_truform.value)
		qglEnable(GL_PN_TRIANGLES_ATI);

	if (e->flags & Q2RF_WEAPONMODEL)
	{
		VectorCopy(currententity->origin, saveorg);
		VectorCopy(r_refdef.vieworg, currententity->origin);
	}

#ifdef Q3SHADERS
	fog = CM_FogForOrigin(currententity->origin);
#endif

	qglColor4f(shadelight[0]/255, shadelight[1]/255, shadelight[2]/255, e->shaderRGBAf[3]);

	memset(&mesh, 0, sizeof(mesh));
	for(surfnum=0; inf; ((inf->nextsurf)?(inf = (galiasinfo_t*)((char *)inf + inf->nextsurf)):(inf=NULL)), surfnum++)
	{
		needrecolour = R_GAliasBuildMesh(&mesh, inf, e->frame, e->oldframe, e->lerpfrac, e->shaderRGBAf[3], e->frame1time, e->frame2time, nolightdir);

		c_alias_polys += mesh.numindexes/3;

		if (r_drawflat.value == 2)
		{
			if (needrecolour)
				R_GAliasApplyLighting(&mesh, e->origin, e->angles, e->shaderRGBAf);
			GL_DrawAliasMesh_Sketch(&mesh);
			continue;
		}
#ifdef Q3SHADERS
		else if (currententity->forcedshader)
		{
			meshbuffer_t mb;

			R_IBrokeTheArrays();

			mb.entity = &r_worldentity;
			mb.shader = currententity->forcedshader;
			mb.fog = fog;
			mb.mesh = &mesh;
			mb.infokey = -1;//currententity->keynum;
			mb.dlightbits = 0;

			R_PushMesh(&mesh, mb.shader->features | MF_NONBATCHED | MF_COLORS);

			R_RenderMeshBuffer ( &mb, false );

			continue;
		}
#endif

		skin = GL_ChooseSkin(inf, clmodel->name, surfnum, e);

		if (!skin || ((void*)skin->base == NULL
#ifdef Q3SHADERS
			&& skin->shader == NULL
#endif
			))
		{
			if (needrecolour)
				R_GAliasApplyLighting(&mesh, e->origin, e->angles, e->shaderRGBAf);
			GL_DrawAliasMesh_Sketch(&mesh);
		}
#ifdef Q3SHADERS
		else if (skin->shader)
		{
			meshbuffer_t mb;
			int olddst = skin->shader->numpasses?skin->shader->passes[0].blenddst:0;

			if (e->flags & Q2RF_ADDATIVE && skin->shader->numpasses)
			{	//hack the shader into submition.
				skin->shader->passes[0].blenddst = GL_ONE;
				skin->shader->passes[0].flags &= ~SHADER_PASS_DEPTHWRITE;
			}

			mb.entity = &r_worldentity;
			mb.shader = skin->shader;
			mb.fog = fog;
			mb.mesh = &mesh;
			mb.infokey = -1;//currententity->keynum;
			mb.dlightbits = 0;

			R_IBrokeTheArrays();

			R_PushMesh(&mesh, skin->shader->features | MF_NONBATCHED | MF_COLORS);

			R_RenderMeshBuffer ( &mb, false );

			if (e->flags & Q2RF_ADDATIVE && skin->shader->numpasses)
			{	//hack the shader into submition.
				skin->shader->passes[0].blenddst = olddst;
			}
		}
#endif
		else
		{
			if (needrecolour)
				R_GAliasApplyLighting(&mesh, e->origin, e->angles, e->shaderRGBAf);

			qglEnable(GL_TEXTURE_2D);
//			if (skin->bump)
//				GL_DrawMeshBump(&mesh, skin->base, 0, skin->bump, 0);
//			else
				GL_DrawAliasMesh(&mesh, skin->base);

			if (skin->fullbright && r_fb_models.value && cls.allow_luma)
			{
				mesh.colors_array = NULL;
				qglEnable(GL_BLEND);
				qglColor4f(e->shaderRGBAf[0], e->shaderRGBAf[1], e->shaderRGBAf[2], e->shaderRGBAf[3]*r_fb_models.value);
				c_alias_polys += mesh.numindexes/3;

				qglBlendFunc (GL_SRC_ALPHA, GL_ONE);
				GL_DrawAliasMesh(&mesh, skin->fullbright);
				qglBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
			}
#ifdef Q3BSPS
			if (fog)
			{
				meshbuffer_t mb;
				shader_t dummyshader = {0};

				R_IBrokeTheArrays();

				mb.entity = currententity;
				mb.shader = &dummyshader;
				mb.fog = fog;
				mb.mesh = &mesh;
				mb.infokey = -1;//currententity->keynum;
				mb.dlightbits = 0;

				R_PushMesh(&mesh, mb.shader->features | MF_NONBATCHED | MF_COLORS);

				R_RenderMeshBuffer ( &mb, false );

		
				R_ClearArrays();
			}
#endif
		}
	}

	if (e->flags & Q2RF_WEAPONMODEL)
		VectorCopy(saveorg, currententity->origin);

	if (qglPNTrianglesfATI && gl_ati_truform.value)
		qglDisable(GL_PN_TRIANGLES_ATI);

#ifdef SHOWLIGHTDIR	//testing
	qglDisable(GL_TEXTURE_2D);
	qglBegin(GL_LINES);
	qglColor3f(1,0,0);
	qglVertex3f(	0,
				0,
				0);
	qglVertex3f(	100*mesh.lightaxis[0][0],
				100*mesh.lightaxis[0][1],
				100*mesh.lightaxis[0][2]);

qglColor3f(0,1,0);
	qglVertex3f(	0,
				0,
				0);
	qglVertex3f(	100*mesh.lightaxis[1][0],
				100*mesh.lightaxis[1][1],
				100*mesh.lightaxis[1][2]);

qglColor3f(0,0,1);
	qglVertex3f(	0,
				0,
				0);
	qglVertex3f(	100*mesh.lightaxis[2][0],
				100*mesh.lightaxis[2][1],
				100*mesh.lightaxis[2][2]);
	qglEnd();
	qglEnable(GL_TEXTURE_2D);
#endif

	qglPopMatrix();

	qglDisable(GL_BLEND);

	qglBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	GL_TexEnv(GL_REPLACE);

	qglEnable(GL_TEXTURE_2D);

	qglShadeModel (GL_FLAT);
	if (gl_affinemodels.value)
		qglHint (GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);

	if (e->flags & Q2RF_DEPTHHACK)
		qglDepthRange (gldepthmin, gldepthmax);

	if ((currententity->model->flags & EF_SPECIAL_TRANS) && gl_cull.value)
		qglEnable( GL_CULL_FACE );
	if ((currententity->model->flags & EF_HOLEY))
		qglDisable( GL_ALPHA_TEST );

#ifdef SHOWLIGHTDIR	//testing
	qglDisable(GL_TEXTURE_2D);
	qglColor3f(1,1,1);
	qglBegin(GL_LINES);
	qglVertex3f(	currententity->origin[0],
				currententity->origin[1],
				currententity->origin[2]);
	qglVertex3f(	currententity->origin[0]+100*lightdir[0],
				currententity->origin[1]+100*lightdir[1],
				currententity->origin[2]+100*lightdir[2]);
	qglEnd();
	qglEnable(GL_TEXTURE_2D);
#endif
}

//returns result in the form of the result vector
void RotateLightVector(vec3_t *axis, vec3_t origin, vec3_t lightpoint, vec3_t result)
{
	vec3_t offs;

	offs[0] = lightpoint[0] - origin[0];
	offs[1] = lightpoint[1] - origin[1];
	offs[2] = lightpoint[2] - origin[2];

	result[0] = DotProduct (offs, axis[0]);
	result[1] = DotProduct (offs, axis[1]);
	result[2] = DotProduct (offs, axis[2]);
}

void GL_LightMesh (mesh_t *mesh, vec3_t lightpos, vec3_t colours, float radius)
{
	vec3_t dir;
	int i;
	float dot, d, f, a;
	vec3_t bcolours;

	vec3_t *xyz = mesh->xyz_array;
	vec3_t *normals = mesh->normals_array;
	byte_vec4_t *out = mesh->colors_array;

	bcolours[0] = colours[0]*255;
	bcolours[1] = colours[1]*255;
	bcolours[2] = colours[2]*255;

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
					f = a*bcolours[0];
					if (f > 255)
						f = 255;
					else if (f < 0)
						f = 0;
					out[i][0] = f;

					f = a*bcolours[1];
					if (f > 255)
						f = 255;
					else if (f < 0)
						f = 0;
					out[i][1] = f;

					f = a*bcolours[2];
					if (f > 255)
						f = 255;
					else if (f < 0)
						f = 0;
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
			out[i][3] = 255;
		}
	}
	else
	{
		if (bcolours[0] > 255)
			bcolours[0] = 255;
		if (bcolours[1] > 255)
			bcolours[1] = 255;
		if (bcolours[2] > 255)
			bcolours[2] = 255;
		for (i = 0; i < mesh->numvertexes; i++)
		{
			VectorSubtract(lightpos, xyz[i], dir);
			out[i][0] = bcolours[0];
			out[i][1] = bcolours[1];
			out[i][2] = bcolours[2];
			out[i][3] = 255;
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
	int *e;
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


void R_DrawMeshBumpmap(mesh_t *mesh, galiastexnum_t *skin, vec3_t lightdir)
{
	extern int gldepthfunc;
	static vec3_t *lightdirs;
	static int maxlightdirs;
	extern int normalisationCubeMap;

#ifdef Q3SHADERS
	R_UnlockArrays();
#endif


	//(bumpmap dot cubemap)*texture

	//why no luma?
	//that's thrown on last.

	//why a cubemap?
	//we need to pass colours as a normal somehow
	//we could use the fragment colour for it, however, we then wouldn't be able to colour the light.
	//so we use a cubemap, which has the added advantage of normalizing the light dir for us.

	//the bumpmap we use is tangent-space (so I'm told)
	qglDepthFunc(gldepthfunc);
	qglDepthMask(0);
	if (gldepthmin == 0.5)
		qglCullFace ( GL_BACK );
	else
		qglCullFace ( GL_FRONT );

	qglEnable(GL_BLEND);

	qglVertexPointer(3, GL_FLOAT, 0, mesh->xyz_array);
	qglEnableClientState( GL_VERTEX_ARRAY );

	if (mesh->normals_array && qglNormalPointer)	//d3d wrapper doesn't support normals, and this is only really needed for truform
	{
		qglNormalPointer(GL_FLOAT, 0, mesh->normals_array);
		qglEnableClientState( GL_NORMAL_ARRAY );
	}

	if (mesh->colors_array)
	{
		qglColorPointer(4, GL_UNSIGNED_BYTE, 0, mesh->colors_array);
		qglEnableClientState( GL_COLOR_ARRAY );
	}
	else
		qglDisableClientState( GL_COLOR_ARRAY );


	if (maxlightdirs < mesh->numvertexes)
	{
		maxlightdirs = mesh->numvertexes;
		lightdirs = BZ_Malloc(sizeof(vec3_t)*maxlightdirs*4);
	}

	R_AliasGenerateVertexLightDirs(mesh, lightdir,
				lightdirs + maxlightdirs*0,
				lightdirs + maxlightdirs*1,
				lightdirs + maxlightdirs*2,
				lightdirs + maxlightdirs*3);

	GL_MBind(mtexid0, skin->bump);
	GL_TexEnv(GL_REPLACE);
	qglEnableClientState(GL_TEXTURE_COORD_ARRAY);
	qglTexCoordPointer(2, GL_FLOAT, 0, mesh->st_array);
	qglEnable(GL_TEXTURE_2D);

	GL_SelectTexture(mtexid1);
	GL_BindType(GL_TEXTURE_CUBE_MAP_ARB, normalisationCubeMap);
	qglEnable(GL_TEXTURE_CUBE_MAP_ARB);
	qglTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB_ARB, GL_TEXTURE);
	qglTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_RGB_ARB, GL_PREVIOUS_ARB);
	qglTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB_ARB, GL_DOT3_RGB_ARB);
	GL_TexEnv(GL_COMBINE_ARB);

	qglEnableClientState(GL_TEXTURE_COORD_ARRAY);
	qglTexCoordPointer(3, GL_FLOAT, 0, lightdirs);

	if (gl_mtexarbable>=3)
	{
		GL_MBind(mtexid0+2, skin->base);
		qglEnable(GL_TEXTURE_2D);
	}
	else
	{	//we don't support 3tmus, so draw the bumps, and multiply the rest over the top
		qglDrawElements(GL_TRIANGLES, mesh->numindexes, GL_UNSIGNED_INT, mesh->indexes);
		qglDisable(GL_TEXTURE_CUBE_MAP_ARB);
		GL_MBind(mtexid0, skin->base);
	}
	GL_TexEnv(GL_MODULATE);
	qglEnableClientState(GL_TEXTURE_COORD_ARRAY);
	qglTexCoordPointer(2, GL_FLOAT, 0, mesh->st_array);

	qglDrawElements(GL_TRIANGLES, mesh->numindexes, GL_UNSIGNED_INT, mesh->indexes);




//	GL_SelectTexture(mtexid2);
	qglDisableClientState( GL_TEXTURE_COORD_ARRAY );
	qglDisable(GL_TEXTURE_2D);

	GL_SelectTexture(mtexid1);
	qglDisable(GL_TEXTURE_CUBE_MAP_ARB);
	qglDisableClientState( GL_TEXTURE_COORD_ARRAY );
	GL_TexEnv(GL_MODULATE);

	GL_SelectTexture(mtexid0);
	qglEnable(GL_TEXTURE_2D);
	qglDisableClientState( GL_TEXTURE_COORD_ARRAY );

	qglDisableClientState( GL_VERTEX_ARRAY );
	qglDisableClientState( GL_COLOR_ARRAY );
	qglDisableClientState( GL_NORMAL_ARRAY );

#ifdef Q3SHADERS
	R_IBrokeTheArrays();
#endif
}

void R_DrawGAliasModelLighting (entity_t *e, vec3_t lightpos, vec3_t colours, float radius)
{
#if 0	//glitches, no attenuation... :(

	model_t *clmodel = e->model;
	vec3_t mins, maxs;
	vec3_t lightdir;
	galiasinfo_t *inf;
	galiastexnum_t *tex;
	mesh_t mesh;
	int surfnum;
	extern cvar_t r_nolightdir;

	if (e->flags & Q2RF_VIEWERMODEL)
		return;
	if (r_nolightdir.value)	//are you crazy?
		return;

	//Total insanity with r_shadows 2...
//	if (!strcmp (clmodel->name, "progs/flame2.mdl"))
//		CL_NewDlight (e, e->origin[0]-1, e->origin[1]+1, e->origin[2]+24, 200 + (rand()&31), host_frametime*2, 3);

//	if (!strcmp (clmodel->name, "progs/armor.mdl"))
//		CL_NewDlight (e->keynum, e->origin[0]-1, e->origin[1]+1, e->origin[2]+25, 200 + (rand()&31), host_frametime*2, 3);

	VectorAdd (e->origin, clmodel->mins, mins);
	VectorAdd (e->origin, clmodel->maxs, maxs);

//	if (!(e->flags & Q2RF_WEAPONMODEL))
//		if (R_CullBox (mins, maxs))
//			return;


	RotateLightVector(e->axis, e->origin, lightpos, lightdir);


	GL_DisableMultitexture();
	GL_TexEnv(GL_MODULATE);
	if (gl_smoothmodels.value)
		qglShadeModel (GL_SMOOTH);
	if (gl_affinemodels.value)
		qglHint (GL_PERSPECTIVE_CORRECTION_HINT, GL_FASTEST);


	if (e->flags & Q2RF_DEPTHHACK)
		qglDepthRange (gldepthmin, gldepthmin + 0.3*(gldepthmax-gldepthmin));

	qglColor3f(colours[0], colours[1], colours[2]);
	qglColor4f(1, 1, 1, 1);

	qglPushMatrix();
	R_RotateForEntity(e);
	inf = GLMod_Extradata (clmodel);
	if (gl_ati_truform.value)
		qglEnable(GL_PN_TRIANGLES_ATI);
	qglEnable(GL_TEXTURE_2D);

	qglEnable(GL_POLYGON_OFFSET_FILL);

		GL_TexEnv(GL_REPLACE);
//	qglDisable(GL_STENCIL_TEST);
	qglEnable(GL_BLEND);
	qglDisable(GL_ALPHA_TEST);	//if you used an alpha channel where you shouldn't have, more fool you.
	qglBlendFunc(GL_ONE, GL_ONE);
//	qglDepthFunc(GL_ALWAYS);
	for(surfnum=0;inf;surfnum++)
	{
		R_GAliasBuildMesh(&mesh, inf, e->frame, e->oldframe, e->lerpfrac, e->alpha, e->frame1time, e->frame2time, false);
		mesh.colors_array = tempColours;

		tex = GL_ChooseSkin(inf, clmodel->name, surfnum, e);

		if (tex->bump && e->alpha==1)
		{
			R_DrawMeshBumpmap(&mesh, tex, lightdir);
		}
		else
		{
			GL_LightMesh(&mesh, lightdir, colours, radius);
			GL_DrawAliasMesh(&mesh, tex->base);
		}

		if (inf->nextsurf)
			inf = (galiasinfo_t*)((char *)inf + inf->nextsurf);
		else
			inf = NULL;
	}
	currententity->fatness=0;
	qglPopMatrix();
	if (gl_ati_truform.value)
		qglDisable(GL_PN_TRIANGLES_ATI);

	GL_TexEnv(GL_REPLACE);

	qglShadeModel (GL_FLAT);
	if (gl_affinemodels.value)
		qglHint (GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);
	qglDisable(GL_POLYGON_OFFSET_FILL);

	if (e->flags & Q2RF_DEPTHHACK)
		qglDepthRange (gldepthmin, gldepthmax);

	qglBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	qglDisable(GL_BLEND);
	qglDisable(GL_TEXTURE_2D);

	R_IBrokeTheArrays();
#endif
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
	if (r_noaliasshadows.value)
		return;

	if (e->shaderRGBAf[3] < 0.5)
		return;

	RotateLightVector(e->axis, e->origin, lightpos, lightorg);

	if (Length(lightorg) > radius + clmodel->radius)
		return;

	qglPushMatrix();
	R_RotateForEntity(e);


	inf = GLMod_Extradata (clmodel);
	while(inf)
	{
		if (inf->ofs_trineighbours)
		{
			R_GAliasBuildMesh(&mesh, inf, e->frame, e->oldframe, e->lerpfrac, 1, e->frame1time, e->frame2time, true);
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


char **skinfilelist;
int skinfilecount;

static qboolean VARGS TryAddSkin(char *skinname, ...)
{
	va_list		argptr;
	char		string[MAX_QPATH];

	//make sure we don't add it twice
	int i;


	va_start (argptr, skinname);
	vsnprintf (string,sizeof(string)-1, skinname,argptr);
	va_end (argptr);
	string[MAX_QPATH-1] = '\0';

	for (i = 0; i < skinfilecount; i++)
	{
		if (!strcmp(skinfilelist[i], string))
			return true;	//already added
	}

	if (!COM_FCheckExists(string))
		return false;

	skinfilelist = BZ_Realloc(skinfilelist, sizeof(*skinfilelist)*(skinfilecount+1));
	skinfilelist[skinfilecount] = Z_Malloc(strlen(string)+1);
	strcpy(skinfilelist[skinfilecount], string);
	skinfilecount++;
	return true;
}

int GL_EnumerateSkins(char *name, int size, void *param)
{
	TryAddSkin(name);
	return true;
}

int GL_BuildSkinFileList(char *modelname)
{
	int i;
	char skinfilename[MAX_QPATH];

	//flush the old list
	for (i = 0; i < skinfilecount; i++)
	{
		Z_Free(skinfilelist[i]);
		skinfilelist[i] = NULL;
	}
	skinfilecount=0;

	COM_StripExtension(modelname, skinfilename, sizeof(skinfilename));

	//try and add numbered skins, and then try fixed names.
	for (i = 0; ; i++)
	{
		if (!TryAddSkin("%s_%i.skin", modelname, i))
		{
			if (i == 0)
			{
				if (!TryAddSkin("%s_default.skin", skinfilename, i))
					break;
			}
			else if (i == 1)
			{
				if (!TryAddSkin("%s_blue.skin", skinfilename, i))
					break;
			}
			else if (i == 2)
			{
				if (!TryAddSkin("%s_red.skin", skinfilename, i))
					break;
			}
			else if (i == 3)
			{
				if (!TryAddSkin("%s_green.skin", skinfilename, i))
					break;
			}
			else if (i == 4)
			{
				if (!TryAddSkin("%s_yellow.skin", skinfilename, i))
					break;
			}
			else
				break;
		}
	}

//	if (strstr(modelname, "lower") || strstr(modelname, "upper") || strstr(modelname, "head"))
//	{
		COM_EnumerateFiles(va("%s_*.skin", modelname), GL_EnumerateSkins, NULL);
		COM_EnumerateFiles(va("%s_*.skin", skinfilename), GL_EnumerateSkins, NULL);
//	}
//	else
//		COM_EnumerateFiles("*.skin", GL_EnumerateSkins, NULL);

	return skinfilecount;
}

//This is a hack. It uses an assuption about q3 player models.
void GL_ParseQ3SkinFile(char *out, char *surfname, char *modelname, int skinnum, char *skinfilename)
{
	const char *f = NULL, *p;
	int len;

	if (skinnum >= skinfilecount)
		return;

	if (skinfilename)
		strcpy(skinfilename, skinfilelist[skinnum]);

	f = COM_LoadTempFile2(skinfilelist[skinnum]);

	while(f)
	{
		f = COM_ParseToken(f,NULL);
		if (!f)
			return;
		if (!strcmp(com_token, "replace"))
		{
			f = COM_ParseToken(f, NULL);

			len = strlen(com_token);

			//copy surfname -> out, until we meet the part we need to replace
			while(*surfname)
			{
				if (!strncmp(com_token, surfname, len))
					//found it
				{
					surfname+=len;
					f = COM_ParseToken(f, NULL);
					p = com_token;
					while(*p)	//copy the replacement
						*out++ = *p++;

					while(*surfname)	//copy the remaining
						*out++ = *surfname++;
					*out++ = '\0';	//we didn't find it.
					return;
				}
				*out++ = *surfname++;
			}
			*out++ = '\0';	//we didn't find it.
			return;
		}
		else
		{
			while(*f == ' ' || *f == '\t')
				f++;
			if (*f == ',')
			{
				if (!strcmp(com_token, surfname))
				{
					f++;
					COM_ParseToken(f, NULL);
					strcpy(out, com_token);
					return;
				}
			}
		}

		p = strchr(f, '\n');
		if (!p)
			f = f+strlen(f);
		else
			f = p+1;
		if (!*f)
			break;
	}
}

void GL_LoadSkinFile(galiastexnum_t *texnum, char *surfacename, int skinnumber, unsigned char *rawdata, int width, int height, unsigned char *palette)
{
	char shadername[MAX_QPATH];
	Q_strncpyz(shadername, surfacename, sizeof(shadername));

	GL_ParseQ3SkinFile(shadername, surfacename, loadmodel->name, skinnumber, NULL);

#ifdef Q3SHADERS
	texnum->shader = R_RegisterSkin(shadername);
#endif

	texnum->base = Mod_LoadHiResTexture(shadername, "models", true, true, true);
}

#endif //SERVERONLY


//Q1 model loading
#if 1
static galiasinfo_t *galias;
static dmdl_t *pq1inmodel;
#define NUMVERTEXNORMALS	162
extern float	r_avertexnormals[NUMVERTEXNORMALS][3];
static void *QTest_LoadFrameGroup (daliasframetype_t *pframetype, int *seamremaps)
{
	galiaspose_t *pose;
	galiasgroup_t *frame;
	dtrivertx_t		*pinframe;
	qtestaliasframe_t *frameinfo;
	int				i, j;

	vec3_t *normals;
	vec3_t *verts;

	frame = (galiasgroup_t*)((char *)galias + galias->groupofs);

	for (i = 0; i < pq1inmodel->numframes; i++)
	{
		switch(LittleLong(pframetype->type))
		{
		case ALIAS_SINGLE:
			frameinfo = (qtestaliasframe_t*)((char *)(pframetype+1));
			pinframe = (dtrivertx_t*)((char*)frameinfo+sizeof(qtestaliasframe_t));
			pose = (galiaspose_t *)Hunk_Alloc(sizeof(galiaspose_t) + sizeof(vec3_t)*2*galias->numverts);
			frame->poseofs = (char *)pose - (char *)frame;
			frame->numposes = 1;
			galias->groups++;

			frame->name[0] = '\0';

			verts = (vec3_t *)(pose+1);
			normals = &verts[galias->numverts];
			pose->ofsverts = (char *)verts - (char *)pose;
#ifndef SERVERONLY
			pose->ofsnormals = (char *)normals - (char *)pose;
#endif

			for (j = 0; j < pq1inmodel->numverts; j++)
			{
				verts[j][0] = pinframe[j].v[0]*pq1inmodel->scale[0]+pq1inmodel->scale_origin[0];
				verts[j][1] = pinframe[j].v[1]*pq1inmodel->scale[1]+pq1inmodel->scale_origin[1];
				verts[j][2] = pinframe[j].v[2]*pq1inmodel->scale[2]+pq1inmodel->scale_origin[2];
#ifndef SERVERONLY
				VectorCopy(r_avertexnormals[pinframe[j].lightnormalindex], normals[j]);
#endif
				if (seamremaps[j] != j)
				{
					VectorCopy(verts[j], verts[seamremaps[j]]);
					VectorCopy(normals[j], normals[seamremaps[j]]);
				}
			}

//			GL_GenerateNormals((float*)verts, (float*)normals, (int *)((char *)galias + galias->ofs_indexes), galias->numindexes/3, galias->numverts);

			pframetype = (daliasframetype_t *)&pinframe[pq1inmodel->numverts];
			break;
		default:
			Con_Printf(S_ERROR "Bad frame type for QTest model in %s\n", loadmodel->name);
			return NULL;
		}
		frame++;
	}
	return pframetype;
}

static void *Q1_LoadFrameGroup (daliasframetype_t *pframetype, int *seamremaps)
{
	galiaspose_t *pose;
	galiasgroup_t *frame;
	dtrivertx_t		*pinframe;
	daliasframe_t *frameinfo;
	int				i, j, k;
	daliasgroup_t *ingroup;
	daliasinterval_t *intervals;
	float sinter;

	vec3_t *normals;
	vec3_t *verts;

	frame = (galiasgroup_t*)((char *)galias + galias->groupofs);

	for (i = 0; i < pq1inmodel->numframes; i++)
	{
		switch(LittleLong(pframetype->type))
		{
		case ALIAS_SINGLE:
			frameinfo = (daliasframe_t*)((char *)(pframetype+1));
			pinframe = (dtrivertx_t*)((char*)frameinfo+sizeof(daliasframe_t));
			pose = (galiaspose_t *)Hunk_Alloc(sizeof(galiaspose_t) + sizeof(vec3_t)*2*galias->numverts);
			frame->poseofs = (char *)pose - (char *)frame;
			frame->numposes = 1;
			galias->groups++;

			Q_strncpyz(frame->name, frameinfo->name, sizeof(frame->name));

			verts = (vec3_t *)(pose+1);
			normals = &verts[galias->numverts];
			pose->ofsverts = (char *)verts - (char *)pose;
#ifndef SERVERONLY
			pose->ofsnormals = (char *)normals - (char *)pose;
#endif

			for (j = 0; j < pq1inmodel->numverts; j++)
			{
				verts[j][0] = pinframe[j].v[0]*pq1inmodel->scale[0]+pq1inmodel->scale_origin[0];
				verts[j][1] = pinframe[j].v[1]*pq1inmodel->scale[1]+pq1inmodel->scale_origin[1];
				verts[j][2] = pinframe[j].v[2]*pq1inmodel->scale[2]+pq1inmodel->scale_origin[2];
#ifndef SERVERONLY
				VectorCopy(r_avertexnormals[pinframe[j].lightnormalindex], normals[j]);
#endif
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
		case ALIAS_GROUP_SWAPPED: // prerelease
			ingroup = (daliasgroup_t *)(pframetype+1);

			pose = (galiaspose_t *)Hunk_Alloc(LittleLong(ingroup->numframes)*(sizeof(galiaspose_t) + sizeof(vec3_t)*2*galias->numverts));
			frame->poseofs = (char *)pose - (char *)frame;
			frame->numposes = LittleLong(ingroup->numframes);
			frame->loop = true;
			galias->groups++;

			verts = (vec3_t *)(pose+frame->numposes);
			normals = &verts[galias->numverts];

			intervals = (daliasinterval_t *)(ingroup+1);
			sinter = LittleFloat(intervals->interval);
			if (sinter <= 0)
				sinter = 0.1;
			frame->rate = 1/sinter;

			pinframe = (dtrivertx_t *)(intervals+frame->numposes);
			for (k = 0; k < frame->numposes; k++)
			{
				pose->ofsverts = (char *)verts - (char *)pose;
#ifndef SERVERONLY
				pose->ofsnormals = (char *)normals - (char *)pose;
#endif

				frameinfo = (daliasframe_t*)pinframe;
				pinframe = (dtrivertx_t *)((char *)frameinfo + sizeof(daliasframe_t));

				if (k == 0)
					Q_strncpyz(frame->name, frameinfo->name, sizeof(frame->name));

				for (j = 0; j < pq1inmodel->numverts; j++)
				{
					verts[j][0] = pinframe[j].v[0]*pq1inmodel->scale[0]+pq1inmodel->scale_origin[0];
					verts[j][1] = pinframe[j].v[1]*pq1inmodel->scale[1]+pq1inmodel->scale_origin[1];
					verts[j][2] = pinframe[j].v[2]*pq1inmodel->scale[2]+pq1inmodel->scale_origin[2];
#ifndef SERVERONLY
					VectorCopy(r_avertexnormals[pinframe[j].lightnormalindex], normals[j]);
#endif
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
			Con_Printf(S_ERROR "Bad frame type in %s\n", loadmodel->name);
			return NULL;
		}
		frame++;
	}
	return pframetype;
}

#ifdef SERVERONLY //greatly reduced version of Q1_LoadSkins (too many #ifdefs
static void *Q1_LoadSkins (daliasskintype_t *pskintype, qboolean alpha)
{
	int i;
	int s;
	int *count;
	float *intervals;
	qbyte *data;

	s = pq1inmodel->skinwidth*pq1inmodel->skinheight;
	for (i = 0; i < pq1inmodel->numskins; i++)
	{
		switch(LittleLong(pskintype->type))
		{
		case ALIAS_SKIN_SINGLE:
			pskintype = (daliasskintype_t *)((char *)(pskintype+1)+s);
			break;

		default:
			count = (int *)(pskintype+1);
			intervals = (float *)(count+1);
			data = (qbyte *)(intervals + LittleLong(*count));
			data += s*LittleLong(*count);
			pskintype = (daliasskintype_t *)data;
			break;
		}
	}
	galias->numskins=pq1inmodel->numskins;
	return pskintype;
}
#else
static void *Q1_LoadSkins (daliasskintype_t *pskintype, qboolean alpha)
{
	extern cvar_t gl_bump;
	galiastexnum_t *texnums;
	char skinname[MAX_QPATH];
	int i;
	int s, t;
	float sinter;
	daliasskingroup_t *count;
	daliasskininterval_t *intervals;
	qbyte *data, *saved;
	galiasskin_t *outskin = (galiasskin_t *)((char *)galias + galias->ofsskins);

	int texture;
	int fbtexture;
	int bumptexture;

	s = pq1inmodel->skinwidth*pq1inmodel->skinheight;
	for (i = 0; i < pq1inmodel->numskins; i++)
	{
		switch(LittleLong(pskintype->type))
		{
		case ALIAS_SKIN_SINGLE:
			outskin->skinwidth = pq1inmodel->skinwidth;
			outskin->skinheight = pq1inmodel->skinheight;

			//LH's naming scheme ("models" is likly to be ignored)
			fbtexture = 0;
			bumptexture = 0;
			snprintf(skinname, sizeof(skinname), "%s_%i.", loadmodel->name, i);
			texture = Mod_LoadReplacementTexture(skinname, "models", true, false, true);
			if (texture)
			{
				snprintf(skinname, sizeof(skinname), "%s_%i_luma.", loadmodel->name, i);
				fbtexture = Mod_LoadReplacementTexture(skinname, "models", true, false, true);
				if (gl_bump.value)
				{
					snprintf(skinname, sizeof(skinname), "%s_%i_bump", loadmodel->name, i);
					bumptexture = Mod_LoadBumpmapTexture(skinname, "models");
				}
			}
			else
			{
				snprintf(skinname, sizeof(skinname), "%s_%i", loadname, i);
				texture = Mod_LoadReplacementTexture(skinname, "models", true, false, true);
				if (texture && r_fb_models.value)
				{
					snprintf(skinname, sizeof(skinname), "%s_%i_luma", loadname, i);
					fbtexture = Mod_LoadReplacementTexture(skinname, "models", true, true, true);
				}
				if (texture && gl_bump.value)
				{
					snprintf(skinname, sizeof(skinname), "%s_%i_bump", loadname, i);
					bumptexture = Mod_LoadBumpmapTexture(skinname, "models");
				}
			}

//but only preload it if we have no replacement.
			if (!texture)
			{
				//we're not using 24bits
				texnums = Hunk_Alloc(sizeof(*texnums)+s);
				saved = (qbyte*)(texnums+1);
				outskin->ofstexels = (qbyte *)(saved) - (qbyte *)outskin;
				memcpy(saved, pskintype+1, s);
				GLMod_FloodFillSkin(saved, outskin->skinwidth, outskin->skinheight);

//the extra underscore is to stop
				snprintf(skinname, sizeof(skinname), "%s__%i", loadname, i);
				texture = GL_LoadTexture(skinname, outskin->skinwidth, outskin->skinheight, saved, true, alpha);
				if (r_fb_models.value)
				{
					snprintf(skinname, sizeof(skinname), "%s__%i_luma", loadname, i);
					fbtexture = GL_LoadTextureFB(skinname, outskin->skinwidth, outskin->skinheight, saved, true, true);
				}
				if (gl_bump.value)
				{
					snprintf(skinname, sizeof(skinname), "%s__%i_bump", loadname, i);
					bumptexture = GL_LoadTexture8Bump(skinname, outskin->skinwidth, outskin->skinheight, saved, true, true);
				}
			}
			else
				texnums = Hunk_Alloc(sizeof(*texnums));
			outskin->texnums=1;

			outskin->ofstexnums = (char *)texnums - (char *)outskin;

#ifdef Q3SHADERS
			if (cls.allow_shaders)
			{
				sprintf(skinname, "%s_%i", loadname, i);
				texnums->shader = R_RegisterCustom (skinname, NULL);
			}
#endif


			texnums->base = texture;
			texnums->fullbright = fbtexture;
			texnums->bump = bumptexture;

			pskintype = (daliasskintype_t *)((char *)(pskintype+1)+s);
			break;

		default:
			outskin->skinwidth = pq1inmodel->skinwidth;
			outskin->skinheight = pq1inmodel->skinheight;
			count = (daliasskingroup_t*)(pskintype+1);
			intervals = (daliasskininterval_t *)(count+1);
			outskin->texnums = LittleLong(count->numskins);
			data = (qbyte *)(intervals + outskin->texnums);
			texnums = Hunk_Alloc(sizeof(*texnums)*outskin->texnums);
			outskin->ofstexnums = (char *)texnums - (char *)outskin;
			outskin->ofstexels = 0;
			sinter = LittleFloat(intervals[0].interval);
			if (sinter <= 0)
				sinter = 0.1;
			outskin->skinspeed = 1/sinter;

			for (t = 0; t < outskin->texnums; t++,data+=s, texnums++)
			{
				texture = 0;
				fbtexture = 0;

				//LH naming scheme
				if (!texture)
				{
					sprintf(skinname, "%s_%i_%i.", loadmodel->name, i, t);
					texture = Mod_LoadReplacementTexture(skinname, "models", true, false, true);
				}
				if (!fbtexture && r_fb_models.value)
				{
					sprintf(skinname, "%s_%i_%i_luma.", loadmodel->name, i, t);
					fbtexture = Mod_LoadReplacementTexture(skinname, "models", true, true, true);
				}

				//Fuhquake naming scheme
				if (!texture)
				{
					sprintf(skinname, "%s_%i_%i", loadname, i, t);
					texture = Mod_LoadReplacementTexture(skinname, "models", true, false, true);
				}
				if (!fbtexture && r_fb_models.value)
				{
					sprintf(skinname, "%s_%i_%i_luma", loadname, i, t);
					fbtexture = Mod_LoadReplacementTexture(skinname, "models", true, true, true);
				}

				if (!texture || (!fbtexture && r_fb_models.value))
				{
					if (t == 0)
					{
						saved = Hunk_Alloc(s);
						outskin->ofstexels = (qbyte *)(saved) - (qbyte *)outskin;
					}
					else
						saved = BZ_Malloc(s);
					memcpy(saved, data, s);
					GLMod_FloodFillSkin(saved, outskin->skinwidth, outskin->skinheight);
					if (!texture)
					{
						sprintf(skinname, "%s_%i_%i", loadname, i, t);
						texture = GL_LoadTexture(skinname, outskin->skinwidth, outskin->skinheight, saved, true, alpha);
					}


					if (!fbtexture && r_fb_models.value)
					{
						sprintf(skinname, "%s_%i_%i_luma", loadname, i, t);
						fbtexture = GL_LoadTextureFB(skinname, outskin->skinwidth, outskin->skinheight, saved, true, true);
					}

					if (t != 0)	//only keep the first.
						BZ_Free(saved);
				}

#ifdef Q3SHADERS
				if (cls.allow_shaders)
				{
					sprintf(skinname, "%s_%i_%i", loadname, i, t);
					texnums->shader = R_RegisterCustom (skinname, NULL);
				}
#endif

				texnums->base = texture;
				texnums->fullbright = fbtexture;
			}
			pskintype = (daliasskintype_t *)data;
			break;
		}
		outskin++;
	}
	galias->numskins=pq1inmodel->numskins;
	return pskintype;
}
#endif

qboolean GL_LoadQ1Model (model_t *mod, void *buffer)
{
#ifndef SERVERONLY
	vec2_t *st_array;
	int j;
#endif
	int hunkstart, hunkend, hunktotal;
	int version;
	int i, onseams;
	dstvert_t *pinstverts;
	dtriangle_t *pintriangles;
	int *seamremap;
	index_t *indexes;
	qboolean qtest = false;

	int size;

	loadmodel=mod;

	Mod_DoCRC(loadmodel, buffer, com_filesize);

	hunkstart = Hunk_LowMark ();

	pq1inmodel = (dmdl_t *)buffer;

	loadmodel->engineflags |= MDLF_NEEDOVERBRIGHT;

	version = LittleLong(pq1inmodel->version);
	if (version == QTESTALIAS_VERSION)
		qtest = true;
	else if (version != ALIAS_VERSION)
	{
		Con_Printf (S_ERROR "%s has wrong version number (%i should be %i)\n",
				 mod->name, version, ALIAS_VERSION);
		return false;
	}

	seamremap = (int*)pq1inmodel;	//I like overloading locals.
	
	if (qtest)
		i = sizeof(dmdl_t)/4 - sizeof(int)*2 - 1;
	else
		i = sizeof(dmdl_t)/4 - 1;

	for (; i >= 0; i--)
		seamremap[i] = LittleLong(seamremap[i]);

	if (pq1inmodel->numframes < 1 ||
		pq1inmodel->numskins < 1 ||
		pq1inmodel->numtris < 1 ||
		pq1inmodel->numverts < 3 ||
		pq1inmodel->skinheight < 1 ||
		pq1inmodel->skinwidth < 1)
	{
		Con_Printf(S_ERROR "Model %s has an invalid quantity\n", mod->name);
		return false;
	}

	if (qtest)
		mod->flags = 0; // Qtest has no flags in header
	else
		mod->flags = pq1inmodel->flags;

	size = sizeof(galiasinfo_t)
#ifndef SERVERONLY
		+ pq1inmodel->numskins*sizeof(galiasskin_t)
#endif
		+ pq1inmodel->numframes*sizeof(galiasgroup_t);

	galias = Hunk_Alloc(size);
	galias->groupofs = sizeof(*galias);
#ifndef SERVERONLY
	galias->ofsskins = sizeof(*galias)+pq1inmodel->numframes*sizeof(galiasgroup_t);
#endif
	galias->nextsurf = 0;

//skins
	if (qtest)
		pinstverts = (dstvert_t *)Q1_LoadSkins((daliasskintype_t *)((char *)buffer + sizeof(dmdl_t) - sizeof(int)*2), 0);
	else if( mod->flags & EF_HOLEY )
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
#ifndef SERVERONLY
	st_array = Hunk_Alloc(sizeof(*st_array)*(pq1inmodel->numverts+onseams));
	galias->ofs_st_array = (char *)st_array - (char *)galias;
	for (j=pq1inmodel->numverts,i = 0; i < pq1inmodel->numverts; i++)
	{
		st_array[i][0] = (LittleLong(pinstverts[i].s)+0.5)/(float)pq1inmodel->skinwidth;
		st_array[i][1] = (LittleLong(pinstverts[i].t)+0.5)/(float)pq1inmodel->skinheight;

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
#endif

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
	if (qtest)
	{
		if (QTest_LoadFrameGroup((daliasframetype_t *)&pintriangles[pq1inmodel->numtris], seamremap) == NULL)
		{
			BZ_Free(seamremap);
			Hunk_FreeToLowMark (hunkstart);
			return false;
		}
	}
	else
	{
		if (Q1_LoadFrameGroup((daliasframetype_t *)&pintriangles[pq1inmodel->numtris], seamremap) == NULL)
		{
			BZ_Free(seamremap);
			Hunk_FreeToLowMark (hunkstart);
			return false;
		}
	}
	BZ_Free(seamremap);


#ifndef SERVERONLY
	if (r_shadows.value)
	{
		int *neighbours;
		neighbours = Hunk_Alloc(sizeof(int)*3*pq1inmodel->numtris);
		galias->ofs_trineighbours = (qbyte *)neighbours - (qbyte *)galias;
		R_BuildTriangleNeighbours(neighbours, indexes, pq1inmodel->numtris);
	}
#endif
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
		return false;
	}
	memcpy (mod->cache.data, galias, hunktotal);

	Hunk_FreeToLowMark (hunkstart);

	mod->funcs.Trace = GLMod_Trace;

	return true;
}
#endif


int Mod_ReadFlagsFromMD1(char *name, int md3version)
{
	dmdl_t				*pinmodel;
	char fname[MAX_QPATH];
	COM_StripExtension(name, fname, sizeof(fname));
	COM_DefaultExtension(fname, ".mdl", sizeof(fname));

	if (strcmp(name, fname))	//md3 renamed as mdl
	{
		COM_StripExtension(name, fname, sizeof(fname));	//seeing as the md3 is named over the mdl,
		COM_DefaultExtension(fname, ".md1", sizeof(fname));//read from a file with md1 (one, not an ell)
		return 0;
	}

	pinmodel = (dmdl_t *)COM_LoadTempFile(fname);

	if (!pinmodel)	//not found
		return 0;

	if (LittleLong(pinmodel->ident) != IDPOLYHEADER)
		return 0;
	if (LittleLong(pinmodel->version) != ALIAS_VERSION)
		return 0;
	return LittleLong(pinmodel->flags);
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
#ifndef SERVERONLY
	int i;
	galiastexnum_t *texnums;
	galiasskin_t *outskin = (galiasskin_t *)((char *)galias + galias->ofsskins);

	for (i = 0; i < LittleLong(pq2inmodel->num_skins); i++, outskin++)
	{
		texnums = Hunk_Alloc(sizeof(*texnums));
		outskin->ofstexnums = (char *)texnums - (char *)outskin;
		outskin->texnums=1;

		COM_CleanUpPath(skins);	//blooming tanks.
		texnums->base = Mod_LoadReplacementTexture(skins, "models", true, false, true);
		texnums->shader = R_RegisterCustom(skins, NULL);
		if (!texnums->base && !texnums->shader)
			Con_Printf("Couldn't load %s\n", skins);
		outskin->skinwidth = 0;
		outskin->skinheight = 0;
		outskin->skinspeed = 0;

		skins += MD2MAX_SKINNAME;
	}
#endif
	galias->numskins = LittleLong(pq2inmodel->num_skins);

#ifndef SERVERONLY
	outskin = (galiasskin_t *)((char *)galias + galias->ofsskins);
	outskin += galias->numskins - 1;
	if (galias->numskins)
	{
		texnums = (galiastexnum_t*)((char *)outskin +outskin->ofstexnums);
		if (texnums->base)
			return;
		if (texnums->shader)
			return;

		galias->numskins--;
	}
#endif
}

#define MD2_MAX_TRIANGLES 4096
qboolean GL_LoadQ2Model (model_t *mod, void *buffer)
{
#ifndef SERVERONLY
	dmd2stvert_t *pinstverts;
	vec2_t *st_array;
	vec3_t *normals;
#endif

	int hunkstart, hunkend, hunktotal;
	int version;
	int i, j;
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

	int		indremap[MD2_MAX_TRIANGLES*3];
	unsigned short		ptempindex[MD2_MAX_TRIANGLES*3], ptempstindex[MD2_MAX_TRIANGLES*3];

	int numverts;

	int size;


	loadmodel=mod;

	loadmodel->engineflags |= MDLF_NEEDOVERBRIGHT;

	Mod_DoCRC(mod, buffer, com_filesize);

	hunkstart = Hunk_LowMark ();

	pq2inmodel = (md2_t *)buffer;

	version = LittleLong (pq2inmodel->version);
	if (version != MD2ALIAS_VERSION)
	{
		Con_Printf (S_ERROR "%s has wrong version number (%i should be %i)\n",
				 mod->name, version, MD2ALIAS_VERSION);
		return false;
	}

	if (LittleLong(pq2inmodel->num_frames) < 1 ||
		LittleLong(pq2inmodel->num_skins) < 0 ||
		LittleLong(pq2inmodel->num_tris) < 1 ||
		LittleLong(pq2inmodel->num_xyz) < 3 ||
		LittleLong(pq2inmodel->num_st) < 3 ||
		LittleLong(pq2inmodel->skinheight) < 1 ||
		LittleLong(pq2inmodel->skinwidth) < 1)
	{
		Con_Printf(S_ERROR "Model %s has an invalid quantity\n", mod->name);
		return false;
	}

	mod->flags = 0;

	loadmodel->numframes = LittleLong(pq2inmodel->num_frames);

	size = sizeof(galiasinfo_t)
#ifndef SERVERONLY
		+ LittleLong(pq2inmodel->num_skins)*sizeof(galiasskin_t)
#endif
		+ LittleLong(pq2inmodel->num_frames)*sizeof(galiasgroup_t);

	galias = Hunk_Alloc(size);
	galias->groupofs = sizeof(*galias);
#ifndef SERVERONLY
	galias->ofsskins = sizeof(*galias)+LittleLong(pq2inmodel->num_frames)*sizeof(galiasgroup_t);
#endif
	galias->nextsurf = 0;

//skins
	Q2_LoadSkins(((char *)pq2inmodel+LittleLong(pq2inmodel->ofs_skins)));

	//trianglelists;
	pintri = (dmd2triangle_t *)((char *)pq2inmodel + LittleLong(pq2inmodel->ofs_tris));


	for (i=0 ; i<LittleLong(pq2inmodel->num_tris) ; i++, pintri++)
	{
		for (j=0 ; j<3 ; j++)
		{
			ptempindex[i*3+j] = ( unsigned short )LittleShort ( pintri->xyz_index[j] );
			ptempstindex[i*3+j] = ( unsigned short )LittleShort ( pintri->st_index[j] );
		}
	}

	numindexes = galias->numindexes = LittleLong(pq2inmodel->num_tris)*3;
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

	Con_DPrintf ( "%s: remapped %i verts to %i\n", mod->name, LittleLong(pq2inmodel->num_xyz), numverts );

	galias->numverts = numverts;

	// remap remaining indexes
	for ( i = 0; i < numindexes; i++ )
	{
		if ( indremap[i] != i ) {
			indexes[i] = indexes[indremap[i]];
		}
	}

// s and t vertices
#ifndef SERVERONLY
	pinstverts = ( dmd2stvert_t * ) ( ( qbyte * )pq2inmodel + LittleLong (pq2inmodel->ofs_st) );
	st_array = Hunk_Alloc(sizeof(*st_array)*(numverts));
	galias->ofs_st_array = (char *)st_array - (char *)galias;

	for (j=0 ; j<numindexes; j++)
	{
		st_array[indexes[j]][0] = (float)(((double)LittleShort (pinstverts[ptempstindex[indremap[j]]].s) + 0.5f) /LittleLong(pq2inmodel->skinwidth));
		st_array[indexes[j]][1] = (float)(((double)LittleShort (pinstverts[ptempstindex[indremap[j]]].t) + 0.5f) /LittleLong(pq2inmodel->skinheight));
	}
#endif

	//frames
	ClearBounds ( mod->mins, mod->maxs );

	poutframe = (galiasgroup_t*)((char *)galias + galias->groupofs);
	framesize = LittleLong (pq2inmodel->framesize);
	for (i=0 ; i<LittleLong(pq2inmodel->num_frames) ; i++)
	{
		pose = (galiaspose_t *)Hunk_Alloc(sizeof(galiaspose_t) + sizeof(vec3_t)*numverts
#ifndef SERVERONLY
			+ sizeof(vec3_t)*numverts
#endif
			);
		poutframe->poseofs = (char *)pose - (char *)poutframe;
		poutframe->numposes = 1;
		galias->groups++;

		verts = (vec3_t *)(pose+1);
		pose->ofsverts = (char *)verts - (char *)pose;
#ifndef SERVERONLY
		normals = &verts[galias->numverts];
		pose->ofsnormals = (char *)normals - (char *)pose;
#endif


		pinframe = ( dmd2aliasframe_t * )( ( qbyte * )pq2inmodel + LittleLong (pq2inmodel->ofs_frames) + i * framesize );
		Q_strncpyz(poutframe->name, pinframe->name, sizeof(poutframe->name));

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
#ifndef SERVERONLY
			VectorCopy(bytedirs[pinframe->verts[ptempindex[indremap[j]]].lightnormalindex], normals[indexes[j]]);
#endif
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



#ifndef SERVERONLY
	if (r_shadows.value)
	{
		int *neighbours;
		neighbours = Hunk_Alloc(sizeof(int)*3*LittleLong(pq2inmodel->num_tris));
		galias->ofs_trineighbours = (qbyte *)neighbours - (qbyte *)galias;
		R_BuildTriangleNeighbours(neighbours, indexes, LittleLong(pq2inmodel->num_tris));
	}
#endif
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
		return false;
	}
	memcpy (mod->cache.data, galias, hunktotal);

	Hunk_FreeToLowMark (hunkstart);

	mod->funcs.Trace = GLMod_Trace;

	return true;
}

#endif
















typedef struct {
	char name[MAX_QPATH];
	vec3_t org;
	float ang[3][3];
} md3tag_t;



#ifndef SERVERONLY
qboolean GLMod_GetTag(model_t *model, int tagnum, int frame1, int frame2, float f2ness, float f1time, float f2time, float *result)
#else
qboolean Mod_GetTag(model_t *model, int tagnum, int frame1, int frame2, float f2ness, float f1time, float f2time, float *result)
#endif
{
	galiasinfo_t *inf;


	if (!model || model->type != mod_alias)
		return false;

	inf = Mod_Extradata(model);
#ifdef SKELETALMODELS
	if (inf->numbones)
	{
		galiasbone_t *bone;
		galiasgroup_t *g1, *g2;

		float tempmatrix[12];			//flipped between this and bonematrix
		float *matrix;	//the matrix for a single bone in a single pose.
		float m[12];	//combined interpolated version of 'matrix'.
		int b, k;	//counters

		float *pose[4];	//the per-bone matricies (one for each pose)
		float plerp[4];	//the ammount of that pose to use (must combine to 1)
		int numposes = 0;

		if (tagnum <= 0 || tagnum > inf->numbones)
			return false;
		tagnum--;	//tagnum 0 is 'use my angles/org'

		if (frame1 < 0 || frame1 >= inf->groups)
			return false;
		if (frame2 < 0 || frame2 >= inf->groups)
		{
			f2ness = 0;
			frame2 = frame1;
		}

		bone = (galiasbone_t*)((char*)inf + inf->ofsbones);
//the higher level merges old/new anims, but we still need to blend between automated frame-groups.
		g1 = (galiasgroup_t*)((char *)inf + inf->groupofs + sizeof(galiasgroup_t)*frame1);
		g2 = (galiasgroup_t*)((char *)inf + inf->groupofs + sizeof(galiasgroup_t)*frame2);

		f1time *= g1->rate;
		frame1 = (int)f1time%g1->numposes;
		frame2 = ((int)f1time+1)%g1->numposes;
		f1time = f1time - (int)f1time;
		pose[numposes] = (float *)((char *)g1 + g1->poseofs + sizeof(float)*inf->numbones*12*frame1);
		plerp[numposes] = (1-f1time) * (1-f2ness);
		numposes++;
		if (frame1 != frame2)
		{
			pose[numposes] = (float *)((char *)g1 + g1->poseofs + sizeof(float)*inf->numbones*12*frame2);
			plerp[numposes] = f1time * (1-f2ness);
			numposes++;
		}
		if (f2ness)
		{
			f2time *= g2->rate;
			frame1 = (int)f2time%g2->numposes;
			frame2 = ((int)f2time+1)%g2->numposes;
			f2time = f2time - (int)f2time;
			pose[numposes] = (float *)((char *)g2 + g2->poseofs + sizeof(float)*inf->numbones*12*frame1);
			plerp[numposes] = (1-f2time) * f2ness;
			numposes++;
			if (frame1 != frame2)
			{
				pose[numposes] = (float *)((char *)g2 + g2->poseofs + sizeof(float)*inf->numbones*12*frame2);
				plerp[numposes] = f2time * f2ness;
				numposes++;
			}
		}

		//set up the identity matrix
		for (k = 0;k < 12;k++)
			result[k] = 0;
		result[0] = 1;
		result[5] = 1;
		result[10] = 1;
		while(tagnum >= 0)
		{
			//set up the per-bone transform matrix
			for (k = 0;k < 12;k++)
				m[k] = 0;
			for (b = 0;b < numposes;b++)
			{
				matrix = pose[b] + tagnum*12;

				for (k = 0;k < 12;k++)
					m[k] += matrix[k] * plerp[b];
			}

			memcpy(tempmatrix, result, sizeof(tempmatrix));
			R_ConcatTransforms((void*)m, (void*)tempmatrix, (void*)result);

			tagnum = bone[tagnum].parent;
		}

		return true;
	}
#endif
	if (inf->numtags)
	{
		md3tag_t *t1, *t2;

		if (tagnum <= 0 || tagnum > inf->numtags)
			return false;
		if (frame1 < 0)
			return false;
		if (frame1 >= inf->numtagframes)
			frame1 = inf->numtagframes - 1;
		if (frame2 < 0 || frame2 >= inf->numtagframes)
			frame2 = frame1;
		tagnum--;	//tagnum 0 is 'use my angles/org'

		t1 = (md3tag_t*)((char*)inf + inf->ofstags);
		t1 += tagnum;
		t1 += inf->numtags*frame1;

		t2 = (md3tag_t*)((char*)inf + inf->ofstags);
		t2 += tagnum;
		t2 += inf->numtags*frame2;

		if (t1 == t2)
		{
			result[0]	= t1->ang[0][0];
			result[1]	= t1->ang[0][1];
			result[2]	= t1->ang[0][2];
			result[3]	= t1->org[0];
			result[4]	= t1->ang[1][0];
			result[5]	= t1->ang[1][1];
			result[6]	= t1->ang[1][2];
			result[7]	= t1->org[1];
			result[8]	= t1->ang[2][0];
			result[9]	= t1->ang[2][1];
			result[10]	= t1->ang[2][2];
			result[11]	= t1->org[2];
		}
		else
		{
			float f1ness = 1-f2ness;
			result[0]	= t1->ang[0][0]*f1ness	+ t2->ang[0][0]*f2ness;
			result[1]	= t1->ang[0][1]*f1ness	+ t2->ang[0][1]*f2ness;
			result[2]	= t1->ang[0][2]*f1ness	+ t2->ang[0][2]*f2ness;
			result[3]	= t1->org[0]*f1ness		+ t2->org[0]*f2ness;
			result[4]	= t1->ang[1][0]*f1ness	+ t2->ang[1][0]*f2ness;
			result[5]	= t1->ang[1][1]*f1ness	+ t2->ang[1][1]*f2ness;
			result[6]	= t1->ang[1][2]*f1ness	+ t2->ang[1][2]*f2ness;
			result[7]	= t1->org[1]*f1ness		+ t2->org[1]*f2ness;
			result[8]	= t1->ang[2][0]*f1ness	+ t2->ang[2][0]*f2ness;
			result[9]	= t1->ang[2][1]*f1ness	+ t2->ang[2][1]*f2ness;
			result[10]	= t1->ang[2][2]*f1ness	+ t2->ang[2][2]*f2ness;
			result[11]	= t1->org[2]*f1ness		+ t2->org[2]*f2ness;
		}
		return true;
	}
	return false;
}

#ifndef SERVERONLY
int GLMod_TagNumForName(model_t *model, char *name)
#else
int Mod_TagNumForName(model_t *model, char *name)
#endif
{
	int i;
	galiasinfo_t *inf;
	md3tag_t *t;

	if (!model || model->type != mod_alias)
		return 0;
	inf = Mod_Extradata(model);

#ifdef SKELETALMODELS
	if (inf->numbones)
	{
		galiasbone_t *b;
		b = (galiasbone_t*)((char*)inf + inf->ofsbones);
		for (i = 0; i < inf->numbones; i++)
		{
			if (!strcmp(b[i].name, name))
				return i+1;
		}
	}
#endif
	t = (md3tag_t*)((char*)inf + inf->ofstags);
	for (i = 0; i < inf->numtags; i++)
	{
		if (!strcmp(t[i].name, name))
			return i+1;
	}

	return 0;
}
#ifndef SERVERONLY
int GLMod_SkinNumForName(model_t *model, char *name)
{
	int i;
	galiasinfo_t *inf;
	galiasskin_t *skin;

	if (!model || model->type != mod_alias)
		return -1;
	inf = Mod_Extradata(model);

	skin = (galiasskin_t*)((char*)inf+inf->ofsskins);
	for (i = 0; i < inf->numskins; i++, skin++)
	{
		if (!strcmp(skin->name, name))
			return i;
	}

	return -1;
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

qboolean GL_LoadQ3Model(model_t *mod, void *buffer)
{
#ifndef SERVERONLY
	galiasskin_t	*skin;
	galiastexnum_t	*texnum;
	float lat, lng;
	md3St_t			*inst;
	vec3_t *normals;
	vec2_t *st_array;
	md3Shader_t		*inshader;
#endif
	int hunkstart, hunkend, hunktotal;
//	int version;
	int s, i, j, d;

	index_t *indexes;

	vec3_t min;
	vec3_t max;

	galiaspose_t *pose;
	galiasinfo_t *parent, *root;
	galiasgroup_t *group;

	vec3_t *verts;

	md3Triangle_t	*intris;
	md3XyzNormal_t	*invert;


	int size;
	int externalskins;

	md3Header_t		*header;
	md3Surface_t	*surf;


	loadmodel=mod;

	Mod_DoCRC(mod, buffer, com_filesize);

	hunkstart = Hunk_LowMark ();

	header = buffer;

//	if (header->version != sdfs)
//		Sys_Error("GL_LoadQ3Model: Bad version\n");

	parent = NULL;
	root = NULL;

#ifndef SERVERONLY
	externalskins = GL_BuildSkinFileList(mod->name);
#else
	externalskins = 0;
#endif

	min[0] = min[1] = min[2] = 0;
	max[0] = max[1] = max[2] = 0;

	surf = (md3Surface_t *)((qbyte *)header + LittleLong(header->ofsSurfaces));
	for (s = 0; s < LittleLong(header->numSurfaces); s++)
	{
		if (LittleLong(surf->ident) != MD3_IDENT)
			Con_Printf(S_WARNING "Warning: md3 sub-surface doesn't match ident\n");
		size = sizeof(galiasinfo_t) + sizeof(galiasgroup_t)*LittleLong(header->numFrames);
		galias = Hunk_Alloc(size);
		galias->groupofs = sizeof(*galias);	//frame groups
		galias->groups = LittleLong(header->numFrames);
		galias->numverts = LittleLong(surf->numVerts);
		galias->numindexes = LittleLong(surf->numTriangles)*3;
		if (parent)
			parent->nextsurf = (qbyte *)galias - (qbyte *)parent;
		else
			root = galias;
		parent = galias;

#ifndef SERVERONLY
		st_array = Hunk_Alloc(sizeof(vec2_t)*galias->numindexes);
		galias->ofs_st_array = (qbyte*)st_array - (qbyte*)galias;
		inst = (md3St_t*)((qbyte*)surf + LittleLong(surf->ofsSt));
		for (i = 0; i < galias->numverts; i++)
		{
			st_array[i][0] = LittleFloat(inst[i].s);
			st_array[i][1] = LittleFloat(inst[i].t);
		}
#endif

		indexes = Hunk_Alloc(sizeof(*indexes)*galias->numindexes);
		galias->ofs_indexes = (qbyte*)indexes - (qbyte*)galias;
		intris = (md3Triangle_t *)((qbyte*)surf + LittleLong(surf->ofsTriangles));
		for (i = 0; i < LittleLong(surf->numTriangles); i++)
		{
			indexes[i*3+0] = LittleLong(intris[i].indexes[0]);
			indexes[i*3+1] = LittleLong(intris[i].indexes[1]);
			indexes[i*3+2] = LittleLong(intris[i].indexes[2]);
		}

		group = (galiasgroup_t *)(galias+1);
		invert = (md3XyzNormal_t *)((qbyte*)surf + LittleLong(surf->ofsXyzNormals));
		for (i = 0; i < LittleLong(surf->numFrames); i++)
		{
			pose = (galiaspose_t *)Hunk_Alloc(sizeof(galiaspose_t) + sizeof(vec3_t)*LittleLong(surf->numVerts)
#ifndef SERVERONLY
				+ sizeof(vec3_t)*LittleLong(surf->numVerts)
#endif
				);

			verts = (vec3_t*)(pose+1);
			pose->ofsverts = (qbyte*)verts - (qbyte*)pose;
#ifndef SERVERONLY
			normals = verts + LittleLong(surf->numVerts);
			pose->ofsnormals = (qbyte*)normals - (qbyte*)pose;
#endif

			for (j = 0; j < LittleLong(surf->numVerts); j++)
			{
#ifndef SERVERONLY
				lat = (float)invert[j].latlong[0] * (2 * M_PI)*(1.0 / 255.0);
				lng = (float)invert[j].latlong[1] * (2 * M_PI)*(1.0 / 255.0);
				normals[j][0] = cos ( lng ) * sin ( lat );
				normals[j][1] = sin ( lng ) * sin ( lat );
				normals[j][2] = cos ( lat );
#endif
				for (d = 0; d < 3; d++)
				{
					verts[j][d] = LittleShort(invert[j].xyz[d])/64.0f;
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

			snprintf(group->name, sizeof(group->name)-1, "frame%i", i);

			group->numposes = 1;
			group->rate = 1;
			group->poseofs = (qbyte*)pose - (qbyte*)group;

			group++;
			invert += LittleLong(surf->numVerts);
		}

#ifndef SERVERONLY
		if (externalskins<LittleLong(surf->numShaders))
			externalskins = LittleLong(surf->numShaders);
		if (externalskins)
		{
#ifndef Q3SHADERS
			char name[1024];
			extern int gl_bumpmappingpossible;
#endif
			char shadname[1024];

			skin = Hunk_Alloc((LittleLong(surf->numShaders)+externalskins)*((sizeof(galiasskin_t)+sizeof(galiastexnum_t))));
			galias->ofsskins = (qbyte *)skin - (qbyte *)galias;
			texnum = (galiastexnum_t *)(skin + LittleLong(surf->numShaders)+externalskins);
			inshader = (md3Shader_t *)((qbyte *)surf + LittleLong(surf->ofsShaders));
			for (i = 0; i < externalskins; i++)
			{
				skin->texnums = 1;
				skin->ofstexnums = (qbyte *)texnum - (qbyte *)skin;
				skin->ofstexels = 0;
				skin->skinwidth = 0;
				skin->skinheight = 0;
				skin->skinspeed = 0;

				shadname[0] = '\0';

				GL_ParseQ3SkinFile(shadname, surf->name, loadmodel->name, i, skin->name);

				if (!*shadname)
				{
					if (i >= LittleLong(surf->numShaders))
						strcpy(shadname, "missingskin");	//this shouldn't be possible
					else
						strcpy(shadname, inshader->name);

					Q_strncpyz(skin->name, shadname, sizeof(skin->name));
				}

#ifdef Q3SHADERS
				if (qrenderer)
				{
					texnum->shader = R_RegisterSkin(shadname);

					if (r_shadows.value)	//real-time shadows requires a texture to lighten the model with, even if it has a shader.
										//fixme: this should be read from the shader.
						texnum->base = Mod_LoadHiResTexture(shadname, "models", true, true, true);
				}
#else

				texnum->base = Mod_LoadHiResTexture(shadname, "models", true, true, true);
				if (!texnum->base)
				{
					strcpy(name, loadmodel->name);
					strcpy(COM_SkipPath(name), COM_SkipPath(shadname));	//eviile eh?
					texnum->base = Mod_LoadHiResTexture(name, "models", true, true, true);
				}

				texnum->bump = 0;
				if (gl_bumpmappingpossible)
				{
					COM_StripExtension(shadname, name, sizeof(name));	//go for the normalmap
					strcat(name, "_norm");
					texnum->bump = Mod_LoadHiResTexture(name, "models", true, true, false);
					if (!texnum->bump)
					{
						strcpy(name, loadmodel->name);
						COM_StripExtension(COM_SkipPath(shadname), COM_SkipPath(name), sizeof(name));
						strcat(name, "_norm");
						texnum->bump = Mod_LoadHiResTexture(name, "models", true, true, false);
						if (!texnum->bump)
						{
							COM_StripExtension(shadname, name, sizeof(name));	//bother, go for heightmap and convert
							strcat(name, "_bump");
							texnum->bump = Mod_LoadBumpmapTexture(name, "models");
							if (!texnum->bump)
							{
								strcpy(name, loadmodel->name);
								strcpy(COM_SkipPath(name), COM_SkipPath(shadname));	//eviile eh?
								COM_StripExtension(name, name, sizeof(name));
								strcat(name, "_bump");
								texnum->bump = Mod_LoadBumpmapTexture(name, "models");
							}
						}
					}
				}
				if (r_fb_models.value)
				{
					COM_StripExtension(shadname, name, sizeof(name));	//go for the normalmap
					strcat(name, "_luma");
					texnum->fullbright = Mod_LoadHiResTexture(name, "models", true, true, true);
					if (!texnum->base)
					{
						strcpy(name, loadmodel->name);
						strcpy(COM_SkipPath(name), COM_SkipPath(shadname));	//eviile eh?
						COM_StripExtension(name, name, sizeof(name));
						strcat(name, "_luma");
						texnum->fullbright = Mod_LoadBumpmapTexture(name, "models");
					}
				}
#endif

				inshader++;
				skin++;
				texnum++;
			}
			galias->numskins = i;
		}
#endif

		VectorCopy(min, loadmodel->mins);
		VectorCopy(max, loadmodel->maxs);


#ifndef SERVERONLY
		if (r_shadows.value)
		{
			int *neighbours;
			neighbours = Hunk_Alloc(sizeof(int)*3*LittleLong(surf->numTriangles));
			galias->ofs_trineighbours = (qbyte *)neighbours - (qbyte *)galias;
			R_BuildTriangleNeighbours(neighbours, indexes, LittleLong(surf->numTriangles));
		}
#endif
		surf = (md3Surface_t *)((qbyte *)surf + LittleLong(surf->ofsEnd));
	}

	if (!root)
		root = Hunk_Alloc(sizeof(galiasinfo_t));

	root->numtagframes = LittleLong(header->numFrames);
	root->numtags = LittleLong(header->numTags);
	root->ofstags = (char*)Hunk_Alloc(LittleLong(header->numTags)*sizeof(md3tag_t)*LittleLong(header->numFrames)) - (char*)root;

	{
		md3tag_t *src;
		md3tag_t *dst;

		src = (md3tag_t *)(((unsigned int)header)+LittleLong(header->ofsTags));
		dst = (md3tag_t *)(((unsigned int)root)+root->ofstags);
		for(i=0;i<LittleLong(header->numTags)*LittleLong(header->numFrames);i++)
		{
			memcpy(dst->name, src->name, sizeof(dst->name));
			for(j=0;j<3;j++)
			{
				dst->org[j] = LittleFloat(src->org[j]);
			}

			for(j=0;j<3;j++)
			{
				for(s=0;s<3;s++)
				{
					dst->ang[j][s] = LittleFloat(src->ang[j][s]);
				}
			}

			src++;
			dst++;
		}
	}

//
// move the complete, relocatable alias model to the cache
//

	hunkend = Hunk_LowMark ();
#ifndef SERVERONLY
	if (mod_md3flags.value)
		mod->flags = LittleLong(header->flags);
	else
#endif
		mod->flags = 0;
	if (!mod->flags)
		mod->flags = Mod_ReadFlagsFromMD1(mod->name, 0);

	Hunk_Alloc(0);
	hunktotal = hunkend - hunkstart;

	Cache_Alloc (&mod->cache, hunktotal, loadname);
	mod->type = mod_alias;
	if (!mod->cache.data)
	{
		Hunk_FreeToLowMark (hunkstart);
		return false;
	}
	memcpy (mod->cache.data, root, hunktotal);

	Hunk_FreeToLowMark (hunkstart);

	mod->funcs.Trace = GLMod_Trace;

	return true;
}
#endif



#ifdef ZYMOTICMODELS


typedef struct zymlump_s
{
	int start;
	int length;
} zymlump_t;

typedef struct zymtype1header_s
{
	char id[12]; // "ZYMOTICMODEL", length 12, no termination
	int type; // 0 (vertex morph) 1 (skeletal pose) or 2 (skeletal scripted)
	int filesize; // size of entire model file
	float mins[3], maxs[3], radius; // for clipping uses
	int numverts;
	int numtris;
	int numsurfaces;
	int numbones; // this may be zero in the vertex morph format (undecided)
	int numscenes; // 0 in skeletal scripted models

// skeletal pose header
	// lump offsets are relative to the file
	zymlump_t lump_scenes; // zymscene_t scene[numscenes]; // name and other information for each scene (see zymscene struct)
	zymlump_t lump_poses; // float pose[numposes][numbones][6]; // animation data
	zymlump_t lump_bones; // zymbone_t bone[numbones];
	zymlump_t lump_vertbonecounts; // int vertbonecounts[numvertices]; // how many bones influence each vertex (separate mainly to make this compress better)
	zymlump_t lump_verts; // zymvertex_t vert[numvertices]; // see vertex struct
	zymlump_t lump_texcoords; // float texcoords[numvertices][2];
	zymlump_t lump_render; // int renderlist[rendersize]; // sorted by shader with run lengths (int count), shaders are sequentially used, each run can be used with glDrawElements (each triangle is 3 int indices)
	zymlump_t lump_surfnames; // char shadername[numsurfaces][32]; // shaders used on this model
	zymlump_t lump_trizone; // byte trizone[numtris]; // see trizone explanation
} zymtype1header_t;

typedef struct zymbone_s
{
	char name[32];
	int flags;
	int parent; // parent bone number
} zymbone_t;

typedef struct zymscene_s
{
	char name[32];
	float mins[3], maxs[3], radius; // for clipping
	float framerate; // the scene will animate at this framerate (in frames per second)
	int flags;
	int start, length; // range of poses
} zymscene_t;
#define ZYMSCENEFLAG_NOLOOP 1

typedef struct zymvertex_s
{
	int bonenum;
	float origin[3];
} zymvertex_t;

//this can generate multiple meshes (one for each shader).
//but only one set of transforms are ever generated.
qboolean GLMod_LoadZymoticModel(model_t *mod, void *buffer)
{
#ifndef SERVERONLY
	galiasskin_t *skin;
	galiastexnum_t *texnum;
	int skinfiles;
	int j;
#endif

	int i;
	int hunkstart, hunkend, hunktotal;

	zymtype1header_t *header;
	galiasinfo_t *root;

	galisskeletaltransforms_t *transforms;
	zymvertex_t	*intrans;

	galiasbone_t *bone;
	zymbone_t *inbone;
	int v;
	float multiplier;
	float *matrix, *inmatrix;

	vec2_t *stcoords;
	vec2_t *inst;

	int *vertbonecounts;

	galiasgroup_t *grp;
	zymscene_t *inscene;

	int *renderlist, count;
	index_t *indexes;

	char *surfname;


	loadmodel=mod;

	Mod_DoCRC(mod, buffer, com_filesize);

	hunkstart = Hunk_LowMark ();

	header = buffer;

	if (memcmp(header->id, "ZYMOTICMODEL", 12))
	{
		Con_Printf("GLMod_LoadZymoticModel: %s, doesn't appear to BE a zymotic!\n", mod->name);
		return false;
	}

	if (BigLong(header->type) != 1)
	{
		Con_Printf("GLMod_LoadZymoticModel: %s, only type 1 is supported\n", mod->name);
		return false;
	}

	for (i = 0; i < sizeof(zymtype1header_t)/4; i++)
		((int*)header)[i] = BigLong(((int*)header)[i]);

	if (!header->numverts)
	{
		Con_Printf("GLMod_LoadZymoticModel: %s, no vertexes\n", mod->name);
		return false;
	}

	if (!header->numsurfaces)
	{
		Con_Printf("GLMod_LoadZymoticModel: %s, no surfaces\n", mod->name);
		return false;
	}

	VectorCopy(header->mins, mod->mins);
	VectorCopy(header->maxs, mod->maxs);

	root = Hunk_AllocName(sizeof(galiasinfo_t)*header->numsurfaces, loadname);

	root->numtransforms = header->lump_verts.length/sizeof(zymvertex_t);
	transforms = Hunk_Alloc(root->numtransforms*sizeof(*transforms));
	root->ofstransforms = (char*)transforms - (char*)root;

	vertbonecounts = (int *)((char*)header + header->lump_vertbonecounts.start);
	intrans = (zymvertex_t *)((char*)header + header->lump_verts.start);

	vertbonecounts[0] = BigLong(vertbonecounts[0]);
	multiplier = 1.0f / vertbonecounts[0];
	for (i = 0, v=0; i < root->numtransforms; i++)
	{
		while(!vertbonecounts[v])
		{
			v++;
			if (v == header->numverts)
			{
				Con_Printf("GLMod_LoadZymoticModel: %s, too many transformations\n", mod->name);
				Hunk_FreeToLowMark(hunkstart);
				return false;
			}
			vertbonecounts[v] = BigLong(vertbonecounts[v]);
			multiplier = 1.0f / vertbonecounts[v];
		}
		transforms[i].vertexindex = v;
		transforms[i].boneindex = BigLong(intrans[i].bonenum);
		transforms[i].org[0] = multiplier*BigFloat(intrans[i].origin[0]);
		transforms[i].org[1] = multiplier*BigFloat(intrans[i].origin[1]);
		transforms[i].org[2] = multiplier*BigFloat(intrans[i].origin[2]);
		transforms[i].org[3] = multiplier*1;
		vertbonecounts[v]--;
	}
	if (intrans != (zymvertex_t *)((char*)header + header->lump_verts.start))
	{
		Con_Printf(S_ERROR "%s, Vertex transforms list appears corrupt.\n", mod->name);
		Hunk_FreeToLowMark(hunkstart);
		return false;
	}
	if (vertbonecounts != (int *)((char*)header + header->lump_vertbonecounts.start))
	{
		Con_Printf(S_ERROR "%s, Vertex bone counts list appears corrupt.\n", mod->name);
		Hunk_FreeToLowMark(hunkstart);
		return false;
	}

	root->numverts = v+1;

	root->numbones = header->numbones;
	bone = Hunk_Alloc(root->numtransforms*sizeof(*transforms));
	inbone = (zymbone_t*)((char*)header + header->lump_bones.start);
	for (i = 0; i < root->numbones; i++)
	{
		Q_strncpyz(bone[i].name, inbone[i].name, sizeof(bone[i].name));
		bone[i].parent = BigLong(inbone[i].parent);
	}
	root->ofsbones = (char *)bone - (char *)root;

	renderlist = (int*)((char*)header + header->lump_render.start);
	for (i = 0;i < header->numsurfaces; i++)
	{
		count = BigLong(*renderlist++);
		count *= 3;
		indexes = Hunk_Alloc(count*sizeof(*indexes));
		root[i].ofs_indexes = (char *)indexes - (char*)&root[i];
		root[i].numindexes = count;
		while(count)
		{	//invert
			indexes[count-1] = BigLong(renderlist[count-3]);
			indexes[count-2] = BigLong(renderlist[count-2]);
			indexes[count-3] = BigLong(renderlist[count-1]);
			count-=3;
		}
		renderlist += root[i].numindexes;
	}
	if (renderlist != (int*)((char*)header + header->lump_render.start + header->lump_render.length))
	{
		Con_Printf(S_ERROR "%s, render list appears corrupt.\n", mod->name);
		Hunk_FreeToLowMark(hunkstart);
		return false;
	}

	grp = Hunk_Alloc(sizeof(*grp)*header->numscenes*header->numsurfaces);
	matrix = Hunk_Alloc(header->lump_poses.length);
	inmatrix = (float*)((char*)header + header->lump_poses.start);
	for (i = 0; i < header->lump_poses.length/4; i++)
		matrix[i] = BigFloat(inmatrix[i]);
	inscene = (zymscene_t*)((char*)header + header->lump_scenes.start);
	surfname = ((char*)header + header->lump_surfnames.start);

	stcoords = Hunk_Alloc(root[0].numverts*sizeof(vec2_t));
	inst = (vec2_t *)((char *)header + header->lump_texcoords.start);
	for (i = 0; i < header->lump_texcoords.length/8; i++)
	{
		stcoords[i][0] = BigFloat(inst[i][0]);
		stcoords[i][1] = 1-BigFloat(inst[i][1]);	//hmm. upside down skin coords?
	}

#ifndef SERVERONLY
	skinfiles = GL_BuildSkinFileList(loadmodel->name);
	if (skinfiles < 1)
		skinfiles = 1;
#endif

	for (i = 0; i < header->numsurfaces; i++, surfname+=32)
	{
		root[i].groups = header->numscenes;
		root[i].groupofs = (char*)grp - (char*)&root[i];

#ifdef SERVERONLY
		root[i].numskins = 1;
#else
		root[i].ofs_st_array = (char*)stcoords - (char*)&root[i];
		root[i].numskins = skinfiles;

		skin = Hunk_Alloc((sizeof(galiasskin_t)+sizeof(galiastexnum_t))*skinfiles);
		texnum = (galiastexnum_t*)(skin+skinfiles);
		for (j = 0; j < skinfiles; j++, texnum++)
		{
			skin[j].texnums = 1;	//non-sequenced skins.
			skin[j].ofstexnums = (char *)texnum - (char *)&skin[j];

			GL_LoadSkinFile(texnum, surfname, j, NULL, 0, 0, NULL);
		}

		root[i].ofsskins = (char *)skin - (char *)&root[i];
#endif
	}


	for (i = 0; i < header->numscenes; i++, grp++, inscene++)
	{
		Q_strncpyz(grp->name, inscene->name, sizeof(grp->name));

		grp->isheirachical = 1;
		grp->rate = BigFloat(inscene->framerate);
		grp->loop = !(BigLong(inscene->flags) & ZYMSCENEFLAG_NOLOOP);
		grp->numposes = BigLong(inscene->length);
		grp->poseofs = (char*)matrix  - (char*)grp;
		grp->poseofs += BigLong(inscene->start)*12*sizeof(float)*root->numbones;
	}

	if (inscene != (zymscene_t*)((char*)header + header->lump_scenes.start+header->lump_scenes.length))
	{
		Con_Printf(S_ERROR "%s, scene list appears corrupt.\n", mod->name);
		Hunk_FreeToLowMark(hunkstart);
		return false;
	}

	for (i = 0; i < header->numsurfaces-1; i++)
		root[i].nextsurf = sizeof(galiasinfo_t);
	for (i = 1; i < header->numsurfaces; i++)
	{
		root[i].sharesverts = true;
		root[i].numbones = root[0].numbones;
		root[i].numverts = root[0].numverts;

		root[i].ofsbones = root[0].ofsbones;

		root[i-1].nextsurf = sizeof(*root);
	}

//
// move the complete, relocatable alias model to the cache
//

	hunkend = Hunk_LowMark ();

	mod->flags = Mod_ReadFlagsFromMD1(mod->name, 0);	//file replacement - inherit flags from any defunc mdl files.

	Hunk_Alloc(0);
	hunktotal = hunkend - hunkstart;

	Cache_Alloc (&mod->cache, hunktotal, loadname);
	mod->type = mod_alias;
	if (!mod->cache.data)
	{
		Hunk_FreeToLowMark (hunkstart);
		return false;
	}
	memcpy (mod->cache.data, root, hunktotal);

	Hunk_FreeToLowMark (hunkstart);


	mod->funcs.Trace = GLMod_Trace;

	return true;
}









//////////////////////////////////////////////////////////////
//dpm


// header for the entire file
typedef struct dpmheader_s
{
	char id[16]; // "DARKPLACESMODEL\0", length 16
	unsigned int type; // 2 (hierarchical skeletal pose)
	unsigned int filesize; // size of entire model file
	float mins[3], maxs[3], yawradius, allradius; // for clipping uses

	// these offsets are relative to the file
	unsigned int num_bones;
	unsigned int num_meshs;
	unsigned int num_frames;
	unsigned int ofs_bones; // dpmbone_t bone[num_bones];
	unsigned int ofs_meshs; // dpmmesh_t mesh[num_meshs];
	unsigned int ofs_frames; // dpmframe_t frame[num_frames];
} dpmheader_t;

// there may be more than one of these
typedef struct dpmmesh_s
{
	// these offsets are relative to the file
	char shadername[32]; // name of the shader to use
	unsigned int num_verts;
	unsigned int num_tris;
	unsigned int ofs_verts; // dpmvertex_t vert[numvertices]; // see vertex struct
	unsigned int ofs_texcoords; // float texcoords[numvertices][2];
	unsigned int ofs_indices; // unsigned int indices[numtris*3]; // designed for glDrawElements (each triangle is 3 unsigned int indices)
	unsigned int ofs_groupids; // unsigned int groupids[numtris]; // the meaning of these values is entirely up to the gamecode and modeler
} dpmmesh_t;

// if set on a bone, it must be protected from removal
#define DPMBONEFLAG_ATTACHMENT 1

// one per bone
typedef struct dpmbone_s
{
	// name examples: upperleftarm leftfinger1 leftfinger2 hand, etc
	char name[32];
	// parent bone number
	signed int parent;
	// flags for the bone
	unsigned int flags;
} dpmbone_t;

// a bonepose matrix is intended to be used like this:
// (n = output vertex, v = input vertex, m = matrix, f = influence)
// n[0] = v[0] * m[0][0] + v[1] * m[0][1] + v[2] * m[0][2] + f * m[0][3];
// n[1] = v[0] * m[1][0] + v[1] * m[1][1] + v[2] * m[1][2] + f * m[1][3];
// n[2] = v[0] * m[2][0] + v[1] * m[2][1] + v[2] * m[2][2] + f * m[2][3];
typedef struct dpmbonepose_s
{
	float matrix[3][4];
} dpmbonepose_t;

// immediately followed by bone positions for the frame
typedef struct dpmframe_s
{
	// name examples: idle_1 idle_2 idle_3 shoot_1 shoot_2 shoot_3, etc
	char name[32];
	float mins[3], maxs[3], yawradius, allradius;
	int ofs_bonepositions; // dpmbonepose_t bonepositions[bones];
} dpmframe_t;

// one or more of these per vertex
typedef struct dpmbonevert_s
{
	float origin[3]; // vertex location (these blend)
	float influence; // influence fraction (these must add up to 1)
	float normal[3]; // surface normal (these blend)
	unsigned int bonenum; // number of the bone
} dpmbonevert_t;

// variable size, parsed sequentially
typedef struct dpmvertex_s
{
	unsigned int numbones;
	// immediately followed by 1 or more dpmbonevert_t structures
} dpmvertex_t;

qboolean GLMod_LoadDarkPlacesModel(model_t *mod, void *buffer)
{
#ifndef SERVERONLY
	galiasskin_t *skin;
	galiastexnum_t *texnum;
	int skinfiles;
	float *inst;
	float *outst;
#endif

	int i, j, k;
	int hunkstart, hunkend, hunktotal;

	dpmheader_t *header;
	galiasinfo_t *root, *m;
	dpmmesh_t *mesh;
	dpmvertex_t *vert;
	dpmbonevert_t *bonevert;

	galisskeletaltransforms_t *transforms;

	galiasbone_t *outbone;
	dpmbone_t *inbone;

	float *outposedata;
	galiasgroup_t *outgroups;
	float *inposedata;
	dpmframe_t *inframes;

	unsigned int *index;	index_t *outdex;	// groan...

	int numtransforms;
	int numverts;


	loadmodel=mod;

	Mod_DoCRC(mod, buffer, com_filesize);

	hunkstart = Hunk_LowMark ();

	header = buffer;

	if (memcmp(header->id, "DARKPLACESMODEL\0", 16))
	{
		Con_Printf(S_ERROR "GLMod_LoadDarkPlacesModel: %s, doesn't appear to be a darkplaces model!\n", mod->name);
		return false;
	}

	if (BigLong(header->type) != 2)
	{
		Con_Printf(S_ERROR "GLMod_LoadDarkPlacesModel: %s, only type 2 is supported\n", mod->name);
		return false;
	}

	for (i = 0; i < sizeof(dpmheader_t)/4; i++)
		((int*)header)[i] = BigLong(((int*)header)[i]);

	if (!header->num_bones)
	{
		Con_Printf(S_ERROR "GLMod_LoadDarkPlacesModel: %s, no bones\n", mod->name);
		return false;
	}
	if (!header->num_frames)
	{
		Con_Printf(S_ERROR "GLMod_LoadDarkPlacesModel: %s, no frames\n", mod->name);
		return false;
	}
	if (!header->num_meshs)
	{
		Con_Printf(S_ERROR "GLMod_LoadDarkPlacesModel: %s, no surfaces\n", mod->name);
		return false;
	}


	VectorCopy(header->mins, mod->mins);
	VectorCopy(header->maxs, mod->maxs);

	root = Hunk_AllocName(sizeof(galiasinfo_t)*header->num_meshs, loadname);

	mesh = (dpmmesh_t*)((char*)buffer + header->ofs_meshs);
	for (i = 0; i < header->num_meshs; i++, mesh++)
	{
		//work out how much memory we need to allocate

		mesh->num_verts = BigLong(mesh->num_verts);
		mesh->num_tris = BigLong(mesh->num_tris);
		mesh->ofs_verts = BigLong(mesh->ofs_verts);
		mesh->ofs_texcoords = BigLong(mesh->ofs_texcoords);
		mesh->ofs_indices = BigLong(mesh->ofs_indices);
		mesh->ofs_groupids = BigLong(mesh->ofs_groupids);


		numverts = mesh->num_verts;
		numtransforms = 0;
		//count and byteswap the transformations
		vert = (dpmvertex_t*)((char *)buffer+mesh->ofs_verts);
		for (j = 0; j < mesh->num_verts; j++)
		{
			vert->numbones = BigLong(vert->numbones);
			numtransforms += vert->numbones;
			bonevert = (dpmbonevert_t*)(vert+1);
			vert = (dpmvertex_t*)(bonevert+vert->numbones);
		}

		m = &root[i];
#ifdef SERVERONLY
		transforms = Hunk_AllocName(numtransforms*sizeof(galisskeletaltransforms_t) + mesh->num_tris*3*sizeof(index_t), loadname);
#else
		outst = Hunk_AllocName(numverts*sizeof(vec2_t) + numtransforms*sizeof(galisskeletaltransforms_t) + mesh->num_tris*3*sizeof(index_t), loadname);
		m->ofs_st_array = (char*)outst - (char*)m;
		m->numverts = mesh->num_verts;
		inst = (float*)((char*)buffer + mesh->ofs_texcoords);
		for (j = 0; j < numverts; j++, outst+=2, inst+=2)
		{
			outst[0] = BigFloat(inst[0]);
			outst[1] = BigFloat(inst[1]);
		}

		transforms = (galisskeletaltransforms_t*)outst;
#endif

		//build the transform list.
		m->ofstransforms = (char*)transforms - (char*)m;
		m->numtransforms = numtransforms;
		vert = (dpmvertex_t*)((char *)buffer+mesh->ofs_verts);
		for (j = 0; j < mesh->num_verts; j++)
		{
			bonevert = (dpmbonevert_t*)(vert+1);
			for (k = 0; k < vert->numbones; k++, bonevert++, transforms++)
			{
				transforms->boneindex = BigLong(bonevert->bonenum);
				transforms->vertexindex = j;
				transforms->org[0] = BigFloat(bonevert->origin[0]);
				transforms->org[1] = BigFloat(bonevert->origin[1]);
				transforms->org[2] = BigFloat(bonevert->origin[2]);
				transforms->org[3] = BigFloat(bonevert->influence);
				//do nothing with the normals. :(
			}
			vert = (dpmvertex_t*)bonevert;
		}

		index = (index_t*)((char*)buffer + mesh->ofs_indices);
		outdex = (index_t *)transforms;
		m->ofs_indexes = (char*)outdex - (char*)m;
		m->numindexes = mesh->num_tris*3;
		for (j = 0; j < m->numindexes; j++)
		{
			*outdex++ = BigLong(*index++);
		}
	}

	outbone = Hunk_Alloc(sizeof(galiasbone_t)*header->num_bones);
	inbone = (dpmbone_t*)((char*)buffer + header->ofs_bones);
	for (i = 0; i < header->num_bones; i++)
	{
		outbone[i].parent = BigLong(inbone[i].parent);
		if (outbone[i].parent >= i || outbone[i].parent < -1)
		{
			Con_Printf(S_ERROR "GLMod_LoadDarkPlacesModel: bad bone index in %s\n", mod->name);
			Hunk_FreeToLowMark(hunkstart);
			return false;
		}

		Q_strncpyz(outbone[i].name, inbone[i].name, sizeof(outbone[i].name));
		//throw away the flags.
	}

	outgroups = Hunk_Alloc(sizeof(galiasgroup_t)*header->num_frames + sizeof(float)*header->num_frames*header->num_bones*12);
	outposedata = (float*)(outgroups+header->num_frames);

	inframes = (dpmframe_t*)((char*)buffer + header->ofs_frames);
	for (i = 0; i < header->num_frames; i++)
	{
		inframes[i].ofs_bonepositions = BigLong(inframes[i].ofs_bonepositions);
		inframes[i].allradius = BigLong(inframes[i].allradius);
		inframes[i].yawradius = BigLong(inframes[i].yawradius);
		inframes[i].mins[0] = BigLong(inframes[i].mins[0]);
		inframes[i].mins[1] = BigLong(inframes[i].mins[1]);
		inframes[i].mins[2] = BigLong(inframes[i].mins[2]);
		inframes[i].maxs[0] = BigLong(inframes[i].maxs[0]);
		inframes[i].maxs[1] = BigLong(inframes[i].maxs[1]);
		inframes[i].maxs[2] = BigLong(inframes[i].maxs[2]);

		Q_strncpyz(outgroups[i].name, inframes[i].name, sizeof(outgroups[i].name));

		outgroups[i].rate = 10;
		outgroups[i].numposes = 1;
		outgroups[i].isheirachical = true;
		outgroups[i].poseofs = (char*)outposedata - (char*)&outgroups[i];

		inposedata = (float*)((char*)buffer + inframes[i].ofs_bonepositions);
		for (j = 0; j < header->num_bones*12; j++)
			*outposedata++ = BigFloat(*inposedata++);
	}

#ifndef SERVERONLY
	skinfiles = GL_BuildSkinFileList(loadmodel->name);
	if (skinfiles < 1)
		skinfiles = 1;
#endif

	mesh = (dpmmesh_t*)((char*)buffer + header->ofs_meshs);
	for (i = 0; i < header->num_meshs; i++, mesh++)
	{
		m = &root[i];
		if (i < header->num_meshs-1)
			m->nextsurf = sizeof(galiasinfo_t);
		m->sharesbones = true;

		m->ofsbones = (char*)outbone-(char*)m;
		m->numbones = header->num_bones;

		m->groups = header->num_frames;
		m->groupofs = (char*)outgroups - (char*)m;



#ifdef SERVERONLY
		m->numskins = 1;
#else
		m->numskins = skinfiles;

		skin = Hunk_Alloc((sizeof(galiasskin_t)+sizeof(galiastexnum_t))*skinfiles);
		texnum = (galiastexnum_t*)(skin+skinfiles);
		for (j = 0; j < skinfiles; j++, texnum++)
		{
			skin[j].texnums = 1;	//non-sequenced skins.
			skin[j].ofstexnums = (char *)texnum - (char *)&skin[j];

			GL_LoadSkinFile(texnum, mesh->shadername, j, NULL, 0, 0, NULL);
		}

		m->ofsskins = (char *)skin - (char *)m;
#endif
	}
	root[0].sharesbones = false;




//
// move the complete, relocatable alias model to the cache
//
	hunkend = Hunk_LowMark ();

	mod->flags = Mod_ReadFlagsFromMD1(mod->name, 0);	//file replacement - inherit flags from any defunc mdl files.

	Hunk_Alloc(0);
	hunktotal = hunkend - hunkstart;

	Cache_Alloc (&mod->cache, hunktotal, loadname);
	mod->type = mod_alias;
	if (!mod->cache.data)
	{
		Hunk_FreeToLowMark (hunkstart);
		return false;
	}
	memcpy (mod->cache.data, root, hunktotal);

	Hunk_FreeToLowMark (hunkstart);


	mod->funcs.Trace = GLMod_Trace;

	return true;
}







#endif	//ZYMOTICMODELS

#ifdef MD5MODELS

static void GenMatrix(float x, float y, float z, float qx, float qy, float qz, float result[12])
{
	float qw;
	{	//figure out qw
		float term = 1 - (qx*qx) - (qy*qy) - (qz*qz);
		if (term < 0)
			qw = 0;
		else
			qw = - (float) sqrt(term);
	}

	{	//generate the matrix
		/*
		float xx      = qx * qx;
		float xy      = qx * qy;
		float xz      = qx * qz;
		float xw      = qx * qw;
		float yy      = qy * qy;
		float yz      = qy * qz;
		float yw      = qy * qw;
		float zz      = qz * qz;
		float zw      = qz * qw;
		result[0*4+0]  = 1 - 2 * ( yy + zz );
		result[0*4+1]  =     2 * ( xy - zw );
		result[0*4+2]  =     2 * ( xz + yw );
		result[0*4+3]  =     x;
		result[1*4+0]  =     2 * ( xy + zw );
		result[1*4+1]  = 1 - 2 * ( xx + zz );
		result[1*4+2]  =     2 * ( yz - xw );
		result[1*4+3]  =     y;
		result[2*4+0]  =     2 * ( xz - yw );
		result[2*4+1]  =     2 * ( yz + xw );
		result[2*4+2] = 1 - 2 * ( xx + yy );
		result[2*4+3]  =     z;
		*/

		   float xx, xy, xz, xw, yy, yz, yw, zz, zw;
		   float x2, y2, z2;
		   x2 = qx + qx;
		   y2 = qy + qy;
		   z2 = qz + qz;

		   xx = qx * x2;   xy = qx * y2;   xz = qx * z2;
		   yy = qy * y2;   yz = qy * z2;   zz = qz * z2;
		   xw = qw * x2;   yw = qw * y2;   zw = qw * z2;

		   result[0*4+0] = 1.0f - (yy + zz);
		   result[1*4+0] = xy + zw;
		   result[2*4+0] = xz - yw;

		   result[0*4+1] = xy - zw;
		   result[1*4+1] = 1.0f - (xx + zz);
		   result[2*4+1] = yz + xw;

		   result[0*4+2] = xz + yw;
		   result[1*4+2] = yz - xw;
		   result[2*4+2] = 1.0f - (xx + yy);

		   result[0*4+3]  =     x;
		   result[1*4+3]  =     y;
		   result[2*4+3]  =     z;
	}
}

galiasinfo_t *GLMod_ParseMD5MeshModel(char *buffer)
{
#define MD5ERROR0PARAM(x) { Con_Printf(S_ERROR x "\n"); return NULL; }
#define MD5ERROR1PARAM(x, y) { Con_Printf(S_ERROR x "\n", y); return NULL; }
#define EXPECT(x) buffer = COM_Parse(buffer); if (strcmp(com_token, x)) Sys_Error("MD5MESH: expected %s", x);
	int numjoints = 0;
	int nummeshes = 0;
	qboolean foundjoints = false;
	int i;

	galiasbone_t *bones = NULL;
	galiasgroup_t *pose = NULL;
	galiasinfo_t *inf, *root, *lastsurf;
	float *posedata;
#ifndef SERVERONLY
	galiasskin_t *skin;
	galiastexnum_t *texnum;
#endif

	float x, y, z, qx, qy, qz;



	buffer = COM_Parse(buffer);
	if (strcmp(com_token, "MD5Version"))
		MD5ERROR0PARAM("MD5 model without MD5Version identifier first");

	buffer = COM_Parse(buffer);
	if (atoi(com_token) != 10)
		MD5ERROR0PARAM("MD5 model with unsupported MD5Version");


	root = Hunk_Alloc(sizeof(galiasinfo_t));
	lastsurf = NULL;

	for(;;)
	{
		buffer = COM_Parse(buffer);
		if (!buffer)
			break;

		if (!strcmp(com_token, "commandline"))
		{	//we don't need this
			buffer = strchr(buffer, '\"');
			buffer = strchr((char*)buffer+1, '\"')+1;
//			buffer = COM_Parse(buffer);
		}
		else if (!strcmp(com_token, "numJoints"))
		{
			if (numjoints)
				MD5ERROR0PARAM("MD5MESH: numMeshes was already declared");
			buffer = COM_Parse(buffer);
			numjoints = atoi(com_token);
			if (numjoints <= 0)
				MD5ERROR0PARAM("MD5MESH: Needs some joints");
		}
		else if (!strcmp(com_token, "numMeshes"))
		{
			if (nummeshes)
				MD5ERROR0PARAM("MD5MESH: numMeshes was already declared");
			buffer = COM_Parse(buffer);
			nummeshes = atoi(com_token);
			if (nummeshes <= 0)
				MD5ERROR0PARAM("MD5MESH: Needs some meshes");
		}
		else if (!strcmp(com_token, "joints"))
		{
			if (foundjoints)
				MD5ERROR0PARAM("MD5MESH: Duplicate joints section");
			foundjoints=true;
			if (!numjoints)
				MD5ERROR0PARAM("MD5MESH: joints section before (or without) numjoints");

			bones = Hunk_Alloc(sizeof(*bones) * numjoints);
			pose = Hunk_Alloc(sizeof(galiasgroup_t));
			posedata = Hunk_Alloc(sizeof(float)*12 * numjoints);
			pose->isheirachical = false;
			pose->rate = 1;
			pose->numposes = 1;
			pose->poseofs = (char*)posedata - (char*)pose;

			Q_strncpyz(pose->name, "base", sizeof(pose->name));

			EXPECT("{");
			//"name" parent (x y z) (s t u)
			//stu are a normalized quaternion, which we will convert to a 3*4 matrix for no apparent reason

			for (i = 0; i < numjoints; i++)
			{
				buffer = COM_Parse(buffer);
				Q_strncpyz(bones[i].name, com_token, sizeof(bones[i].name));
				buffer = COM_Parse(buffer);
				bones[i].parent = atoi(com_token);
				if (bones[i].parent >= i)
					MD5ERROR0PARAM("MD5MESH: joints parent's must be lower");
				if ((bones[i].parent < 0 && i) || (!i && bones[i].parent!=-1))
					MD5ERROR0PARAM("MD5MESH: Only the root joint may have a negative parent");

				EXPECT("(");
				buffer = COM_Parse(buffer);
				x = atof(com_token);
				buffer = COM_Parse(buffer);
				y = atof(com_token);
				buffer = COM_Parse(buffer);
				z = atof(com_token);
				EXPECT(")");
				EXPECT("(");
				buffer = COM_Parse(buffer);
				qx = atof(com_token);
				buffer = COM_Parse(buffer);
				qy = atof(com_token);
				buffer = COM_Parse(buffer);
				qz = atof(com_token);
				EXPECT(")");
				GenMatrix(x, y, z, qx, qy, qz, posedata+i*12);
			}
			EXPECT("}");
		}
		else if (!strcmp(com_token, "mesh"))
		{
			int numverts = 0;
			int numweights = 0;
			int numtris = 0;

			int num;
			int vnum;

			int numusableweights = 0;
			int *firstweightlist = NULL;
			int *numweightslist = NULL;

			galisskeletaltransforms_t *trans;
#ifndef SERVERONLY
			float *stcoord = NULL;
#endif
			int *indexes = NULL;
			float w;

			vec4_t *rawweight = NULL;
			int *rawweightbone = NULL;


			if (!nummeshes)
				MD5ERROR0PARAM("MD5MESH: mesh section before (or without) nummeshes");
			if (!foundjoints || !bones || !pose)
				MD5ERROR0PARAM("MD5MESH: mesh must come after joints");

			if (!lastsurf)
			{
				lastsurf = root;
				inf = root;
			}
			else
			{
				inf = Hunk_Alloc(sizeof(*inf));
				lastsurf->nextsurf = (char*)inf - (char*)lastsurf;
				lastsurf = inf;
			}

			inf->ofsbones = (char*)bones - (char*)inf;
			inf->numbones = numjoints;
			inf->groups = 1;
			inf->groupofs = (char*)pose - (char*)inf;

#ifndef SERVERONLY
			skin = Hunk_Alloc(sizeof(*skin));
			texnum = Hunk_Alloc(sizeof(*texnum));
			inf->numskins = 1;
			inf->ofsskins = (char*)skin - (char*)inf;
			skin->texnums = 1;
			skin->skinspeed = 1;
			skin->ofstexnums = (char*)texnum - (char*)skin;
#endif
			EXPECT("{");
			for(;;)
			{
				buffer = COM_Parse(buffer);
				if (!buffer)
					MD5ERROR0PARAM("MD5MESH: unexpected eof");

				if (!strcmp(com_token, "shader"))
				{
					buffer = COM_Parse(buffer);
#ifndef SERVERONLY
	//				texnum->shader = R_RegisterSkin(com_token);
					texnum->base = Mod_LoadHiResTexture(com_token, "models", true, true, true);
#endif
				}
				else if (!strcmp(com_token, "numverts"))
				{
					if (numverts)
						MD5ERROR0PARAM("MD5MESH: numverts was already specified");
					buffer = COM_Parse(buffer);
					numverts = atoi(com_token);
					if (numverts < 0)
						MD5ERROR0PARAM("MD5MESH: numverts cannot be negative");

					firstweightlist = Z_Malloc(sizeof(*firstweightlist) * numverts);
					numweightslist = Z_Malloc(sizeof(*numweightslist) * numverts);
#ifndef SERVERONLY
					stcoord = Hunk_Alloc(sizeof(float)*2*numverts);
					inf->ofs_st_array = (char*)stcoord - (char*)inf;
					inf->numverts = numverts;
#endif
				}
				else if (!strcmp(com_token, "vert"))
				{	//vert num ( s t ) firstweight numweights

					buffer = COM_Parse(buffer);
					num = atoi(com_token);
					if (num < 0 || num >= numverts || !indexes)
						MD5ERROR0PARAM("MD5MESH: vertex out of range");

					EXPECT("(");
					buffer = COM_Parse(buffer);
#ifndef SERVERONLY
					if (!stcoord)
						MD5ERROR0PARAM("MD5MESH: vertex out of range");
					stcoord[num*2+0] = atof(com_token);
#endif
					buffer = COM_Parse(buffer);
#ifndef SERVERONLY
					stcoord[num*2+1] = atof(com_token);
#endif
					EXPECT(")");
					buffer = COM_Parse(buffer);
					firstweightlist[num] = atoi(com_token);
					buffer = COM_Parse(buffer);
					numweightslist[num] = atoi(com_token);

					numusableweights += numweightslist[num];
				}
				else if (!strcmp(com_token, "numtris"))
				{
					if (numtris)
						MD5ERROR0PARAM("MD5MESH: numtris was already specified");
					buffer = COM_Parse(buffer);
					numtris = atoi(com_token);
					if (numtris < 0)
						MD5ERROR0PARAM("MD5MESH: numverts cannot be negative");

					indexes = Hunk_Alloc(sizeof(int)*3*numtris);
					inf->ofs_indexes = (char*)indexes - (char*)inf;
					inf->numindexes = numtris*3;
				}
				else if (!strcmp(com_token, "tri"))
				{
					buffer = COM_Parse(buffer);
					num = atoi(com_token);
					if (num < 0 || num >= numtris)
						MD5ERROR0PARAM("MD5MESH: vertex out of range");

					buffer = COM_Parse(buffer);
					indexes[num*3+0] = atoi(com_token);
					buffer = COM_Parse(buffer);
					indexes[num*3+1] = atoi(com_token);
					buffer = COM_Parse(buffer);
					indexes[num*3+2] = atoi(com_token);
				}
				else if (!strcmp(com_token, "numweights"))
				{
					if (numweights)
						MD5ERROR0PARAM("MD5MESH: numweights was already specified");
					buffer = COM_Parse(buffer);
					numweights = atoi(com_token);

					rawweight = Z_Malloc(sizeof(*rawweight)*numweights);
					rawweightbone = Z_Malloc(sizeof(*rawweightbone)*numweights);
				}
				else if (!strcmp(com_token, "weight"))
				{
					//weight num bone scale ( x y z )
					buffer = COM_Parse(buffer);
					num = atoi(com_token);
					if (num < 0 || num >= numweights)
						MD5ERROR0PARAM("MD5MESH: weight out of range");

					buffer = COM_Parse(buffer);
					rawweightbone[num] = atoi(com_token);
					if (rawweightbone[num] < 0 || rawweightbone[num] >= numjoints)
						MD5ERROR0PARAM("MD5MESH: weight specifies bad bone");
					buffer = COM_Parse(buffer);
					w = atof(com_token);

					EXPECT("(");
					buffer = COM_Parse(buffer);
					rawweight[num][0] = w*atof(com_token);
					buffer = COM_Parse(buffer);
					rawweight[num][1] = w*atof(com_token);
					buffer = COM_Parse(buffer);
					rawweight[num][2] = w*atof(com_token);
					EXPECT(")");
					rawweight[num][3] = w;
				}
				else if (!strcmp(com_token, "}"))
					break;
				else
					MD5ERROR1PARAM("MD5MESH: Unrecognised token inside mesh (%s)", com_token);

			}

			trans = Hunk_Alloc(sizeof(*trans)*numusableweights);
			inf->ofstransforms = (char*)trans - (char*)inf;

			for (num = 0, vnum = 0; num < numverts; num++)
			{
				if (numweightslist[num] <= 0)
					MD5ERROR0PARAM("MD5MESH: weights not set on vertex");
				while(numweightslist[num])
				{
					trans[vnum].vertexindex = num;
					trans[vnum].boneindex = rawweightbone[firstweightlist[num]];
					trans[vnum].org[0] = rawweight[firstweightlist[num]][0];
					trans[vnum].org[1] = rawweight[firstweightlist[num]][1];
					trans[vnum].org[2] = rawweight[firstweightlist[num]][2];
					trans[vnum].org[3] = rawweight[firstweightlist[num]][3];
					vnum++;
					firstweightlist[num]++;
					numweightslist[num]--;
				}
			}
			inf->numtransforms = vnum;

			if (firstweightlist)
				Z_Free(firstweightlist);
			if (numweightslist)
				Z_Free(numweightslist);
			if (rawweight)
				Z_Free(rawweight);
			if (rawweightbone)
				Z_Free(rawweightbone);
		}
		else
			MD5ERROR1PARAM("Unrecognised token in MD5 model (%s)", com_token);
	}

	if (!lastsurf)
		MD5ERROR0PARAM("MD5MESH: No meshes");

	return root;
#undef MD5ERROR0PARAM
#undef MD5ERROR1PARAM
#undef EXPECT
}

qboolean GLMod_LoadMD5MeshModel(model_t *mod, void *buffer)
{
	galiasinfo_t *root;
	int hunkstart, hunkend, hunktotal;


	loadmodel=mod;

	Mod_DoCRC(mod, buffer, com_filesize);

	hunkstart = Hunk_LowMark ();


	root = GLMod_ParseMD5MeshModel(buffer);
	if (root == NULL)
	{
		Hunk_FreeToLowMark(hunkstart);
		return false;
	}


	hunkend = Hunk_LowMark ();

	mod->flags = Mod_ReadFlagsFromMD1(mod->name, 0);	//file replacement - inherit flags from any defunc mdl files.

	Hunk_Alloc(0);
	hunktotal = hunkend - hunkstart;

	Cache_Alloc (&mod->cache, hunktotal, loadname);
	mod->type = mod_alias;
	if (!mod->cache.data)
	{
		Hunk_FreeToLowMark (hunkstart);
		return false;
	}
	memcpy (mod->cache.data, root, hunktotal);

	Hunk_FreeToLowMark (hunkstart);


	mod->funcs.Trace = GLMod_Trace;
	return true;
}

qboolean GLMod_ParseMD5Anim(char *buffer, galiasinfo_t *prototype, void**poseofs, galiasgroup_t *gat)
{
#define MD5ERROR0PARAM(x) { Con_Printf(S_ERROR x "\n"); return false; }
#define MD5ERROR1PARAM(x, y) { Con_Printf(S_ERROR x "\n", y); return false; }
#define EXPECT(x) buffer = COM_Parse(buffer); if (strcmp(com_token, x)) MD5ERROR1PARAM("MD5ANIM: expected %s", x);
	unsigned int i, j;

	galiasgroup_t grp;

	unsigned int parent;
	unsigned int numframes;
	unsigned int numjoints;
	float framespersecond;
	unsigned int numanimatedparts;
	galiasbone_t *bonelist;

	unsigned char *boneflags;
	unsigned int *firstanimatedcomponants;

	float *animatedcomponants;
	float *baseframe;	//6 componants.
	float *posedata;
	float tx, ty, tz, qx, qy, qz;
	int fac, flags;
	float f;

	EXPECT("MD5Version");
	EXPECT("10");

	EXPECT("commandline");
	buffer = COM_Parse(buffer);

	EXPECT("numFrames");
	buffer = COM_Parse(buffer);
	numframes = atoi(com_token);

	EXPECT("numJoints");
	buffer = COM_Parse(buffer);
	numjoints = atoi(com_token);

	EXPECT("frameRate");
	buffer = COM_Parse(buffer);
	framespersecond = atof(com_token);

	EXPECT("numAnimatedComponents");
	buffer = COM_Parse(buffer);
	numanimatedparts = atoi(com_token);

	firstanimatedcomponants = BZ_Malloc(sizeof(int)*numjoints);
	animatedcomponants = BZ_Malloc(sizeof(float)*numanimatedparts);
	boneflags = BZ_Malloc(sizeof(unsigned char)*numjoints);
	baseframe = BZ_Malloc(sizeof(float)*12*numjoints);

	*poseofs = posedata = Hunk_Alloc(sizeof(float)*12*numjoints*numframes);

	if (prototype)
	{
		if (prototype->numbones != numjoints)
			MD5ERROR0PARAM("MD5ANIM: number of bones doesn't match");
		bonelist = (galiasbone_t *)((char*)prototype + prototype->ofsbones);
	}
	else
	{
		bonelist = Hunk_Alloc(sizeof(galiasbone_t)*numjoints);
		prototype->ofsbones = (char*)bonelist - (char*)prototype;
		prototype->numbones = numjoints;
	}

	EXPECT("hierarchy");
	EXPECT("{");
	for (i = 0; i < numjoints; i++, bonelist++)
	{
		buffer = COM_Parse(buffer);
		if (prototype)
		{
			if (strcmp(bonelist->name, com_token))
				MD5ERROR1PARAM("MD5ANIM: bone name doesn't match (%s)", com_token);
		}
		else
			Q_strncpyz(bonelist->name, com_token, sizeof(bonelist->name));
		buffer = COM_Parse(buffer);
		parent = atoi(com_token);
		if (prototype)
		{
			if (bonelist->parent != parent)
				MD5ERROR1PARAM("MD5ANIM: bone name doesn't match (%s)", com_token);
		}
		else
			bonelist->parent = parent;

		buffer = COM_Parse(buffer);
		boneflags[i] = atoi(com_token);
		buffer = COM_Parse(buffer);
		firstanimatedcomponants[i] = atoi(com_token);
	}
	EXPECT("}");

	EXPECT("bounds");
	EXPECT("{");
	for (i = 0; i < numframes; i++)
	{
		EXPECT("(");
		buffer = COM_Parse(buffer);f=atoi(com_token);
		if (f < loadmodel->mins[0]) loadmodel->mins[0] = f;
		buffer = COM_Parse(buffer);f=atoi(com_token);
		if (f < loadmodel->mins[1]) loadmodel->mins[1] = f;
		buffer = COM_Parse(buffer);f=atoi(com_token);
		if (f < loadmodel->mins[2]) loadmodel->mins[2] = f;
		EXPECT(")");
		EXPECT("(");
		buffer = COM_Parse(buffer);f=atoi(com_token);
		if (f > loadmodel->maxs[0]) loadmodel->maxs[0] = f;
		buffer = COM_Parse(buffer);f=atoi(com_token);
		if (f > loadmodel->maxs[1]) loadmodel->maxs[1] = f;
		buffer = COM_Parse(buffer);f=atoi(com_token);
		if (f > loadmodel->maxs[2]) loadmodel->maxs[2] = f;
		EXPECT(")");
	}
	EXPECT("}");

	EXPECT("baseframe");
	EXPECT("{");
	for (i = 0; i < numjoints; i++)
	{
		EXPECT("(");
		buffer = COM_Parse(buffer);
		baseframe[i*6+0] = atof(com_token);
		buffer = COM_Parse(buffer);
		baseframe[i*6+1] = atof(com_token);
		buffer = COM_Parse(buffer);
		baseframe[i*6+2] = atof(com_token);
		EXPECT(")");
		EXPECT("(");
		buffer = COM_Parse(buffer);
		baseframe[i*6+3] = atof(com_token);
		buffer = COM_Parse(buffer);
		baseframe[i*6+4] = atof(com_token);
		buffer = COM_Parse(buffer);
		baseframe[i*6+5] = atof(com_token);
		EXPECT(")");
	}
	EXPECT("}");

	for (i = 0; i < numframes; i++)
	{
		EXPECT("frame");
		EXPECT(va("%i", i));
		EXPECT("{");
		for (j = 0; j < numanimatedparts; j++)
		{
			buffer = COM_Parse(buffer);
			animatedcomponants[j] = atof(com_token);
		}
		EXPECT("}");

		for (j = 0; j < numjoints; j++)
		{
			fac = firstanimatedcomponants[j];
			flags = boneflags[j];

			if (flags&1)
				tx = animatedcomponants[fac++];
			else
				tx = baseframe[j*6+0];
			if (flags&2)
				ty = animatedcomponants[fac++];
			else
				ty = baseframe[j*6+1];
			if (flags&4)
				tz = animatedcomponants[fac++];
			else
				tz = baseframe[j*6+2];
			if (flags&8)
				qx = animatedcomponants[fac++];
			else
				qx = baseframe[j*6+3];
			if (flags&16)
				qy = animatedcomponants[fac++];
			else
				qy = baseframe[j*6+4];
			if (flags&32)
				qz = animatedcomponants[fac++];
			else
				qz = baseframe[j*6+5];

			GenMatrix(tx, ty, tz, qx, qy, qz, posedata+12*(j+numjoints*i));
		}
	}

	BZ_Free(firstanimatedcomponants);
	BZ_Free(animatedcomponants);
	BZ_Free(boneflags);
	BZ_Free(baseframe);

	Q_strncpyz(grp.name, "", sizeof(grp.name));
	grp.isheirachical = true;
	grp.numposes = numframes;
	grp.rate = framespersecond;
	grp.loop = true;

	*gat = grp;
	return true;
#undef MD5ERROR0PARAM
#undef MD5ERROR1PARAM
#undef EXPECT
}

/*
EXTERNALANIM

//File what specifies md5 model/anim stuff.

model test/imp.md5mesh

group test/idle1.md5anim
clampgroup test/idle1.md5anim
frames test/idle1.md5anim

*/
qboolean GLMod_LoadCompositeAnim(model_t *mod, void *buffer)
{
	int i;

	char *file;
	galiasinfo_t *root = NULL;
	int numgroups = 0;
	galiasgroup_t *grouplist = NULL;
	galiasgroup_t *newgroup = NULL;
	void **poseofs;
	int hunkstart, hunkend, hunktotal;


	loadmodel=mod;

	Mod_DoCRC(mod, buffer, com_filesize);

	hunkstart = Hunk_LowMark ();




	buffer = COM_Parse(buffer);
	if (strcmp(com_token, "EXTERNALANIM"))
	{
		Con_Printf (S_ERROR "EXTERNALANIM: header is not compleate (%s)\n", mod->name);
		return false;
	}

	buffer = COM_Parse(buffer);
	if (!strcmp(com_token, "model"))
	{
		buffer = COM_Parse(buffer);
		file = COM_LoadTempFile2(com_token);

		if (!file)	//FIXME: make non fatal somehow..
		{
			Con_Printf(S_ERROR "Couldn't open %s (from %s)\n", com_token, mod->name);
			Hunk_FreeToLowMark(hunkstart);
			return false;
		}

		root = GLMod_ParseMD5MeshModel(file);
		if (root == NULL)
		{
			Hunk_FreeToLowMark(hunkstart);
			return false;
		}
		newgroup = (galiasgroup_t*)((char*)root + root->groupofs);

		grouplist = BZ_Malloc(sizeof(galiasgroup_t)*(numgroups+root->groups));
		memcpy(grouplist, newgroup, sizeof(galiasgroup_t)*(numgroups+root->groups));
		poseofs = BZ_Malloc(sizeof(galiasgroup_t)*(numgroups+root->groups));
		for (i = 0; i < root->groups; i++)
		{
			grouplist[numgroups] = newgroup[i];
			poseofs[numgroups] = (char*)&newgroup[i] + newgroup[i].poseofs;
			numgroups++;
		}
	}
	else
	{
		Con_Printf (S_ERROR "EXTERNALANIM: model must be defined immediatly after the header\n");
		return false;
	}

	for (;;)
	{
		buffer = COM_Parse(buffer);
		if (!buffer)
			break;

		if (!strcmp(com_token, "group"))
		{
			grouplist = BZ_Realloc(grouplist, sizeof(galiasgroup_t)*(numgroups+1));
			poseofs = BZ_Realloc(poseofs, sizeof(*poseofs)*(numgroups+1));
			buffer = COM_Parse(buffer);
			file = COM_LoadTempFile2(com_token);
			if (file)	//FIXME: make non fatal somehow..
			{
				char namebkup[MAX_QPATH];
				Q_strncpyz(namebkup, com_token, sizeof(namebkup));
				if (!GLMod_ParseMD5Anim(file, root, &poseofs[numgroups], &grouplist[numgroups]))
				{
					Hunk_FreeToLowMark(hunkstart);
					return false;
				}
				Q_strncpyz(grouplist[numgroups].name, namebkup, sizeof(grouplist[numgroups].name));
				numgroups++;
			}
		}
		else if (!strcmp(com_token, "clampgroup"))
		{
			Con_Printf(S_ERROR "EXTERNALANIM: clampgroup not yet supported (%s)\n", mod->name);
			Hunk_FreeToLowMark(hunkstart);
			return false;
		}
		else if (!strcmp(com_token, "frames"))
		{
			Con_Printf (S_ERROR "EXTERNALANIM: frames not yet supported (%s)\n", mod->name);
			Hunk_FreeToLowMark(hunkstart);
			return false;
		}
		else
		{
			Con_Printf(S_ERROR "EXTERNALANIM: unrecognised token (%s)\n", mod->name);
			Hunk_FreeToLowMark(hunkstart);
			return false;
		}
	}

	newgroup = grouplist;
	grouplist = Hunk_Alloc(sizeof(galiasgroup_t)*numgroups);
	for(;;)
	{
		root->groupofs = (char*)grouplist - (char*)root;
		root->groups = numgroups;
		if (!root->nextsurf)
			break;
		root = (galiasinfo_t*)((char*)root + root->nextsurf);
	}
	for (i = 0; i < numgroups; i++)
	{
		grouplist[i] = newgroup[i];
		grouplist[i].poseofs = (char*)poseofs[i] - (char*)&grouplist[i];
	}


	hunkend = Hunk_LowMark ();

	mod->flags = Mod_ReadFlagsFromMD1(mod->name, 0);	//file replacement - inherit flags from any defunc mdl files.

	Hunk_Alloc(0);
	hunktotal = hunkend - hunkstart;

	Cache_Alloc (&mod->cache, hunktotal, loadname);
	mod->type = mod_alias;
	if (!mod->cache.data)
	{
		Hunk_FreeToLowMark (hunkstart);
		return false;
	}
	memcpy (mod->cache.data, root, hunktotal);

	Hunk_FreeToLowMark (hunkstart);


	mod->funcs.Trace = GLMod_Trace;
	return true;
}

#endif //MD5MODELS

#endif	// defined(RGLQUAKE) || defined(SERVERONLY)
