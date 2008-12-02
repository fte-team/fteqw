
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

#ifdef _WIN32
	#include <malloc.h>
#else
	#include <alloca.h>
#endif

#define MAX_BONES 256

#ifndef SERVERONLY
	static model_t *loadmodel;
#endif

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

#ifdef SKELETALMODELS
static void R_LerpBones(float *plerp, float **pose, int poses, galiasbone_t *bones, int startingbone, int bonecount, float bonepose[MAX_BONES][12]);
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

		if (!(loadmodel->engineflags & MDLF_PLAYER))
		{	//eyes
			loadmodel->tainted = (crc != 6967);
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
#pragma message("this just uses frame 0")
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
					R_LerpBones(&frac, (float**)posedata, 1, (galiasbone_t*)((char*)mod + mod->ofsbones), 0, mod->numbones, bonepose);
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
static void R_LerpBones(float *plerp, float **pose, int poses, galiasbone_t *bones, int startingbone, int bonecount, float bonepose[MAX_BONES][12])
{
	int i, k, b;
	float *matrix, *matrix2, m[12];

	if (poses == 1)
	{
		// vertex weighted skeletal
		// interpolate matrices and concatenate them to their parents
		matrix = pose[0] + startingbone*12;
		for (i = startingbone;i < bonecount;i++)
		{
			if (bones[i].parent >= 0)
				R_ConcatTransforms((void*)bonepose[bones[i].parent], (void*)matrix, (void*)bonepose[i]);
			else
				for (k = 0;k < 12;k++)	//parentless
					bonepose[i][k] = matrix[k];
			
			matrix += 12;
		}
	}
	else if (poses == 2)
	{
		// vertex weighted skeletal
		// interpolate matrices and concatenate them to their parents
		matrix = pose[0] + startingbone*12;
		matrix2 = pose[1] + startingbone*12;
		for (i = startingbone;i < bonecount;i++)
		{
			//only two poses, blend the matricies to generate a temp matrix
			for (k = 0;k < 12;k++)
				m[k] = (matrix[k] * plerp[0]) + (matrix2[k] * plerp[1]);
			matrix += 12;
			matrix2 += 12;

			if (bones[i].parent >= 0)
				R_ConcatTransforms((void*)bonepose[bones[i].parent], (void*)m, (void*)bonepose[i]);
			else
				for (k = 0;k < 12;k++)	//parentless
					bonepose[i][k] = m[k];
		}
	}
	else
	{
		// vertex weighted skeletal
		// interpolate matrices and concatenate them to their parents
		for (i = startingbone;i < bonecount;i++)
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

static int R_BuildSkeletonLerps(float plerp[4], float *pose[4], int numbones, galiasgroup_t *g1, galiasgroup_t *g2, float lerpfrac, float fg1time, float fg2time)
{
	int frame1;
	int frame2;
	float mlerp;	//minor lerp, poses within a group.
	int l = 0;
	
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

	plerp[l] = (1-mlerp)*(1-lerpfrac);
	if (plerp[l]>0)
		pose[l++] = (float *)((char *)g1 + g1->poseofs + sizeof(float)*numbones*12*frame1);
	plerp[l] = (mlerp)*(1-lerpfrac);
	if (plerp[l]>0)
		pose[l++] = (float *)((char *)g1 + g1->poseofs + sizeof(float)*numbones*12*frame2);

	if (lerpfrac)
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

		plerp[l] = (1-mlerp)*(lerpfrac);
		if (plerp[l]>0)
			pose[l++] = (float *)((char *)g2 + g2->poseofs + sizeof(float)*numbones*12*frame1);
		plerp[l] = (mlerp)*(lerpfrac);
		if (plerp[l]>0)
			pose[l++] = (float *)((char *)g2 + g2->poseofs + sizeof(float)*numbones*12*frame2);
	}

	return l;
}

//writes into bonepose
static void R_BuildSkeleton(galiasinfo_t *inf, entity_t *e, float bonepose[MAX_BONES][12])
{
	int i, k;

	int l=0;
	float plerp[4];
	float *pose[4];
	float baseplerp[4];
	float *basepose[4];
	qboolean hirachy;

	int numposes, basenumposes;
	int basebone = e->basebone;
	int frame1 = e->frame1;
	int frame2 = e->frame2;
	float lerpfrac = e->lerpfrac;
	float baselerpfrac = e->baselerpfrac;

	galiasgroup_t *g1, *g2, *bg1, *bg2;

	galiasbone_t *bones = (galiasbone_t *)((char*)inf+inf->ofsbones);

	g1 = (galiasgroup_t*)((char *)inf + inf->groupofs + sizeof(galiasgroup_t)*frame1);
	g2 = (galiasgroup_t*)((char *)inf + inf->groupofs + sizeof(galiasgroup_t)*frame2);

	if (basebone < 0)
		basebone = 0;
	if (basebone > inf->numbones)
		basebone = inf->numbones;

	if (basebone)
	{
		bg1 = (galiasgroup_t*)((char *)inf + inf->groupofs + sizeof(galiasgroup_t)*e->baseframe1);
		bg2 = (galiasgroup_t*)((char *)inf + inf->groupofs + sizeof(galiasgroup_t)*e->baseframe2);

		if (!bg1->isheirachical || !g1->isheirachical)
		{
			//mixing not supported
			basebone = 0;
			bg1 = g1;
			bg2 = g2;
		}
	}
	else
	{
		bg1 = g1;
		bg2 = g2;
	}

	if (g1->isheirachical != g2->isheirachical || lerpfrac < 0)
		lerpfrac = 0;
	hirachy = g1->isheirachical;


	numposes = R_BuildSkeletonLerps(plerp, pose, inf->numbones, g1, g2, lerpfrac, e->frame1time, e->frame2time);

	if (hirachy)
	{
		if (basebone)
		{
			basenumposes = R_BuildSkeletonLerps(baseplerp, basepose, inf->numbones, bg1, bg2, baselerpfrac, e->baseframe1time, e->baseframe2time);
			R_LerpBones(baseplerp, basepose, basenumposes, bones, 0, basebone, bonepose);
		}
		R_LerpBones(plerp, pose, numposes, bones, basebone, inf->numbones, bonepose);
	}
	else
	{
		//this is not hierachal, using base frames is not a good idea.
		//just blend the poses here
		if (numposes == 1)
			memcpy(bonepose, pose[0], sizeof(float)*12*inf->numbones);
		else if (numposes == 2)
		{
			for (i = 0; i < inf->numbones*12; i++)
			{
				((float*)bonepose)[i] = pose[0][i]*plerp[0] + pose[1][i]*plerp[1];
			}
		}
		else
		{
			for (i = 0; i < inf->numbones; i++)
			{
				for (l = 0; l < 12; l++)
					bonepose[i][l] = 0;
				for (k = 0; k < numposes; k++)
				{
					for (l = 0; l < 12; l++)
						bonepose[i][l] += pose[k][i*12+l] * plerp[k];
				}
			}
		}
	}
}

#ifndef SERVERONLY
static void R_BuildSkeletalMesh(mesh_t *mesh, float bonepose[MAX_BONES][12], galisskeletaltransforms_t *weights, int numweights)
{
	int i;


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

static qboolean R_GAliasBuildMesh(mesh_t *mesh, galiasinfo_t *inf, 
									entity_t *e,
								  float alpha, qboolean nolightdir)
{
	galiasgroup_t *g1, *g2;

	int frame1 = e->frame1;
	int frame2 = e->frame2;
	float lerp = e->lerpfrac;
	float fg1time = e->frame1time;
	float fg2time = e->frame2time;

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
		float bonepose[MAX_BONES][12];
		R_BuildSkeleton(inf, e, bonepose);
		R_BuildSkeletalMesh(mesh, bonepose, (galisskeletaltransforms_t *)((char*)inf+inf->ofstransforms), inf->numtransforms);
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

	unsigned int tc, bc;
	qboolean forced;

	if ((e->model->engineflags & MDLF_NOTREPLACEMENTS) && !ruleset_allow_sensative_texture_replacements.value)
		forced = true;
	else
		forced = false;

	if (!gl_nocolors.value || forced)
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
					return NULL;
				if (e->skinnum >= 0 && e->skinnum < inf->numskins)
					skins += e->skinnum;
				texnums = (galiastexnum_t*)((char *)skins + skins->ofstexnums);
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
								translate32[BOTTOM_RANGE+i] = d_8to24rgbtable[(tc<<4)+15-i];
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

	qglDrawElements(GL_TRIANGLES, mesh->numindexes, GL_INDEX_TYPE, mesh->indexes);

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

	qglDrawRangeElements(GL_TRIANGLES, 0, mesh->numvertexes, mesh->numindexes, GL_INDEX_TYPE, mesh->indexes);

	qglDisableClientState( GL_VERTEX_ARRAY );
	qglDisableClientState( GL_COLOR_ARRAY );
	qglDisableClientState( GL_NORMAL_ARRAY );
	qglDisableClientState( GL_TEXTURE_COORD_ARRAY );

#ifdef Q3SHADERS
	R_IBrokeTheArrays();
#endif
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

	if (clmodel->tainted)
	{
		if (!ruleset_allow_modified_eyes.value && !strcmp(clmodel->name, "progs/eyes.mdl"))
			return;
	}

	if (!(e->flags & Q2RF_WEAPONMODEL))
		if (R_CullEntityBox (e, clmodel->mins, clmodel->maxs))
			return;

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

	if ((e->model->flags & EF_ROTATE) && cl.hexen2pickups)
	{
		shadelight[0] = shadelight[1] = shadelight[2] = 
		ambientlight[0] = ambientlight[1] = ambientlight[2] = 128+sin(cl.time*4)*64;
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
	else if ((e->model->flags & EFH2_SPECIAL_TRANS))	//hexen2 flags.
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
	else if ((e->model->flags & EFH2_TRANSPARENT))
	{
		qglEnable (GL_BLEND);
		qglBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	}
	else if ((e->model->flags & EFH2_HOLEY))
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

	if (!ruleset_allow_larger_models.value && clmodel->clampscale != 1)
	{	//possibly this should be on a per-frame basis, but that's a real pain to do
		Con_DPrintf("Rescaling %s by %f\n", clmodel->name, clmodel->clampscale);
		qglScalef(clmodel->clampscale, clmodel->clampscale, clmodel->clampscale);
	}

	inf = GLMod_Extradata (clmodel);
	if (qglPNTrianglesfATI && gl_ati_truform.value)
		qglEnable(GL_PN_TRIANGLES_ATI);

	if (e->flags & Q2RF_WEAPONMODEL)
	{
		VectorCopy(currententity->origin, saveorg);
		VectorCopy(r_refdef.vieworg, currententity->origin);
	}

#if defined(Q3SHADERS) && defined(Q2BSPS)
	fog = CM_FogForOrigin(currententity->origin);
#endif

	qglColor4f(shadelight[0]/255, shadelight[1]/255, shadelight[2]/255, e->shaderRGBAf[3]);

	memset(&mesh, 0, sizeof(mesh));
	for(surfnum=0; inf; ((inf->nextsurf)?(inf = (galiasinfo_t*)((char *)inf + inf->nextsurf)):(inf=NULL)), surfnum++)
	{
		needrecolour = R_GAliasBuildMesh(&mesh, inf, e, e->shaderRGBAf[3], nolightdir);

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

			if (skin->loweroverlay && r_skin_overlays.value)
			{
				qglEnable(GL_BLEND);
				qglBlendFunc (GL_SRC_ALPHA, GL_ONE);
				mesh.colors_array = NULL;
				if (e->scoreboard)
				{
					int c = e->scoreboard->tbottomcolor;
					if (c >= 16)
						qglColor4f(shadelight[0]/255, shadelight[1]/255, shadelight[2]/255, e->shaderRGBAf[3]);
					else if (c >= 8)
						qglColor4f(host_basepal[c*16*3]/255.0f, host_basepal[c*16*3+1]/255.0f, host_basepal[c*16*3+2]/255.0f, e->shaderRGBAf[3]);
					else
						qglColor4f(host_basepal[15+c*16*3]/255.0f, host_basepal[15+c*16*3+1]/255.0f, host_basepal[15+c*16*3+2]/255.0f, e->shaderRGBAf[3]);
				}
				c_alias_polys += mesh.numindexes/3;
				GL_DrawAliasMesh(&mesh, skin->loweroverlay);
			}
			if (skin->upperoverlay && r_skin_overlays.value)
			{
				qglEnable(GL_BLEND);
				qglBlendFunc (GL_SRC_ALPHA, GL_ONE);
				mesh.colors_array = NULL;
				if (e->scoreboard)
				{
					int c = e->scoreboard->ttopcolor;
					if (c >= 16)
						qglColor4f(shadelight[0]/255, shadelight[1]/255, shadelight[2]/255, e->shaderRGBAf[3]);
					else if (c >= 8)
						qglColor4f(host_basepal[c*16*3]/255.0f, host_basepal[c*16*3+1]/255.0f, host_basepal[c*16*3+2]/255.0f, e->shaderRGBAf[3]);
					else
						qglColor4f(host_basepal[15+c*16*3]/255.0f, host_basepal[15+c*16*3+1]/255.0f, host_basepal[15+c*16*3+2]/255.0f, e->shaderRGBAf[3]);
				}
				c_alias_polys += mesh.numindexes/3;
				GL_DrawAliasMesh(&mesh, skin->upperoverlay);
			}
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

	if ((currententity->model->flags & EFH2_SPECIAL_TRANS) && gl_cull.value)
		qglEnable( GL_CULL_FACE );
	if ((currententity->model->flags & EFH2_HOLEY))
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
		qglDrawElements(GL_TRIANGLES, mesh->numindexes, GL_INDEX_TYPE, mesh->indexes);
		qglDisable(GL_TEXTURE_CUBE_MAP_ARB);
		GL_MBind(mtexid0, skin->base);
	}
	GL_TexEnv(GL_MODULATE);
	qglEnableClientState(GL_TEXTURE_COORD_ARRAY);
	qglTexCoordPointer(2, GL_FLOAT, 0, mesh->st_array);

	qglDrawElements(GL_TRIANGLES, mesh->numindexes, GL_INDEX_TYPE, mesh->indexes);




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
			R_GAliasBuildMesh(&mesh, inf, e, 1, true);
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

#endif	// defined(RGLQUAKE) || defined(SERVERONLY)

