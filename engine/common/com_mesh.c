#include "quakedef.h"

#include "com_mesh.h"

qboolean		r_loadbumpmapping;
extern cvar_t dpcompat_psa_ungroup;
extern cvar_t r_noframegrouplerp;
cvar_t r_lerpmuzzlehack						= CVARF  ("r_lerpmuzzlehack", "1", CVAR_ARCHIVE);

#ifndef SERVERONLY
void Mod_UpdateCRC(void *ctx, void *data, size_t a, size_t b)
{
	char st[40];
	Q_snprintfz(st, sizeof(st), "%d", (int) a);
	if (strcmp(st, Info_ValueForKey(cls.userinfo[0], ctx)))
	{
		Info_SetValueForKey (cls.userinfo[0], ctx, st, sizeof(cls.userinfo[0]));
		if (cls.state >= ca_connected)
			CL_SendClientCommand(true, "setinfo %s %s", (char*)ctx, st);
	}
}
#endif

//Common loader function.
void Mod_DoCRC(model_t *mod, char *buffer, int buffersize)
{
#ifndef SERVERONLY
	//we've got to have this bit
	if (mod->engineflags & MDLF_DOCRC)
	{
		unsigned short crc;
		qbyte *p;
		int len;

		QCRC_Init(&crc);
		for (len = buffersize, p = buffer; len; len--, p++)
			QCRC_ProcessByte(&crc, *p);

		COM_AddWork(WG_MAIN, Mod_UpdateCRC, (mod->engineflags & MDLF_PLAYER) ? pmodel_name : emodel_name, NULL, crc, 0);

		if (!(mod->engineflags & MDLF_PLAYER))
		{	//eyes
			mod->tainted = (crc != 6967);
		}
	}
#endif
}




#ifdef _WIN32
#include <malloc.h>
#else
#include <alloca.h>
#endif

extern cvar_t gl_part_flame, r_fullbrightSkins, r_fb_models;
extern cvar_t r_noaliasshadows;
extern cvar_t r_skin_overlays;
extern cvar_t mod_md3flags;



typedef struct
{
	char *name;
	float furthestallowedextremety;	//this field is the combined max-min square, added together
									//note that while this allows you to move models about a little, you cannot resize the visible part
} clampedmodel_t;

//these should be rounded up slightly.
//really this is only to catch spiked models. This doesn't prevent more visible models, just bigger ones.
clampedmodel_t clampedmodel[] = {
	{"maps/b_bh100.bsp", 3440},
	{"progs/player.mdl", 22497},
	{"progs/eyes.mdl", 755},
	{"progs/gib1.mdl", 374},
	{"progs/gib2.mdl", 1779},
	{"progs/gib3.mdl", 2066},
	{"progs/bolt2.mdl", 1160},
	{"progs/end1.mdl", 764},
	{"progs/end2.mdl", 981},
	{"progs/end3.mdl", 851},
	{"progs/end4.mdl", 903},
	{"progs/g_shot.mdl", 3444},
	{"progs/g_nail.mdl", 2234},
	{"progs/g_nail2.mdl", 3660},
	{"progs/g_rock.mdl", 3441},
	{"progs/g_rock2.mdl", 3660},
	{"progs/g_light.mdl", 2698},
	{"progs/invisibl.mdl", 196},
	{"progs/quaddama.mdl", 2353},
	{"progs/invulner.mdl", 2746},
	{"progs/suit.mdl", 3057},
	{"progs/missile.mdl", 416},
	{"progs/grenade.mdl", 473},
	{"progs/spike.mdl", 112},
	{"progs/s_spike.mdl", 112},
	{"progs/backpack.mdl", 1117},
	{"progs/armor.mdl", 2919},
	{"progs/s_bubble.spr", 100},
	{"progs/s_explod.spr", 1000},

	//and now TF models
#ifdef warningmsg
#pragma warningmsg("FIXME: these are placeholders")
#endif
	{"progs/disp.mdl", 3000},
	{"progs/tf_flag.mdl", 3000},
	{"progs/tf_stan.mdl", 3000},
	{"progs/turrbase.mdl", 3000},
	{"progs/turrgun.mdl", 3000}
};









void Mod_AccumulateTextureVectors(vecV_t *vc, vec2_t *tc, vec3_t *nv, vec3_t *sv, vec3_t *tv, index_t *idx, int numidx)
{
	int i;
	float *v0, *v1, *v2;
	float *tc0, *tc1, *tc2;

	vec3_t d1, d2;
	float td1, td2;

	vec3_t norm, t, s;
	vec3_t temp;

	for (i = 0; i < numidx; i += 3)
	{
		//this is the stuff we're working from
		v0 = vc[idx[i+0]];
		v1 = vc[idx[i+1]];
		v2 = vc[idx[i+2]];
		tc0 = tc[idx[i+0]];
		tc1 = tc[idx[i+1]];
		tc2 = tc[idx[i+2]];

		//calc perpendicular directions
		VectorSubtract(v1, v0, d1);
		VectorSubtract(v2, v0, d2);

		//calculate s as the pependicular of the t dir
		td1 = tc1[1] - tc0[1];
		td2 = tc2[1] - tc0[1];
		s[0] = td1 * d2[0] - td2 * d1[0];
		s[1] = td1 * d2[1] - td2 * d1[1];
		s[2] = td1 * d2[2] - td2 * d1[2];

		//calculate t as the pependicular of the s dir
		td1 = tc1[0] - tc0[0];
		td2 = tc2[0] - tc0[0];
		t[0] = td1 * d2[0] - td2 * d1[0];
		t[1] = td1 * d2[1] - td2 * d1[1];
		t[2] = td1 * d2[2] - td2 * d1[2];

		//the surface might be a back face and thus textured backwards
		//calc the normal twice and compare.
		norm[0] = d2[1] * d1[2] - d2[2] * d1[1];
		norm[1] = d2[2] * d1[0] - d2[0] * d1[2];
		norm[2] = d2[0] * d1[1] - d2[1] * d1[0];
		CrossProduct(t, s, temp);
		if (DotProduct(temp, norm) < 0)
		{
			VectorNegate(s, s);
			VectorNegate(t, t);
		}

		//and we're done, accumulate the result
		VectorAdd(sv[idx[i+0]], s, sv[idx[i+0]]);
		VectorAdd(sv[idx[i+1]], s, sv[idx[i+1]]);
		VectorAdd(sv[idx[i+2]], s, sv[idx[i+2]]);

		VectorAdd(tv[idx[i+0]], t, tv[idx[i+0]]);
		VectorAdd(tv[idx[i+1]], t, tv[idx[i+1]]);
		VectorAdd(tv[idx[i+2]], t, tv[idx[i+2]]);
	}
}

void Mod_AccumulateMeshTextureVectors(mesh_t *m)
{
	Mod_AccumulateTextureVectors(m->xyz_array, m->st_array, m->normals_array, m->snormals_array, m->tnormals_array, m->indexes, m->numindexes);
}

void Mod_NormaliseTextureVectors(vec3_t *n, vec3_t *s, vec3_t *t, int v)
{
	int i;
	float f;
	vec3_t tmp;

	for (i = 0; i < v; i++)
	{
		//strip away any variance against the normal to keep it perpendicular, then normalize
		f = -DotProduct(s[i], n[i]);
		VectorMA(s[i], f, n[i], tmp);
		VectorNormalize2(tmp, s[i]);

		f = -DotProduct(t[i], n[i]);
		VectorMA(t[i], f, n[i], tmp);
		VectorNormalize2(tmp, t[i]);
	}
}



#ifdef SKELETALMODELS

/*like above, but guess the quat.w*/
static void GenMatrixPosQuat3Scale(vec3_t pos, vec3_t quat3, vec3_t scale, float result[12])
{
	vec4_t quat4;
	float term = 1 - DotProduct(quat3, quat3);
	if (term < 0)
		quat4[3] = 0;
	else
		quat4[3] = - (float) sqrt(term);
	VectorCopy(quat3, quat4);
	GenMatrixPosQuat4Scale(pos, quat4, scale, result);
}

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
#endif

#ifdef PSKMODELS
static void PSKGenMatrix(float x, float y, float z, float qx, float qy, float qz, float qw, float result[12])
{
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
#endif

/*transforms some skeletal vecV_t values*/
static void Alias_TransformVerticies_V(const float *bonepose, int vertcount, qbyte *bidx, float *weights, float *xyzin, float *fte_restrict xyzout)
{
	int i;
	const float *matrix;
	for (i = 0; i < vertcount; i++, xyzout+=sizeof(vecV_t)/sizeof(vec_t), xyzin+=sizeof(vecV_t)/sizeof(vec_t), bidx+=4, weights+=4)
	{
		matrix = &bonepose[12*bidx[0]];
		xyzout[0] = weights[0] * (xyzin[0] * matrix[0] + xyzin[1] * matrix[1] + xyzin[2] * matrix[ 2] + xyzin[3] * matrix[ 3]);
		xyzout[1] = weights[0] * (xyzin[0] * matrix[4] + xyzin[1] * matrix[5] + xyzin[2] * matrix[ 6] + xyzin[3] * matrix[ 7]);
		xyzout[2] = weights[0] * (xyzin[0] * matrix[8] + xyzin[1] * matrix[9] + xyzin[2] * matrix[10] + xyzin[3] * matrix[11]);

		if (weights[1])
		{
			matrix = &bonepose[12*bidx[1]];
			xyzout[0] += weights[1] * (xyzin[0] * matrix[0] + xyzin[1] * matrix[1] + xyzin[2] * matrix[ 2] + xyzin[3] * matrix[ 3]);
			xyzout[1] += weights[1] * (xyzin[0] * matrix[4] + xyzin[1] * matrix[5] + xyzin[2] * matrix[ 6] + xyzin[3] * matrix[ 7]);
			xyzout[2] += weights[1] * (xyzin[0] * matrix[8] + xyzin[1] * matrix[9] + xyzin[2] * matrix[10] + xyzin[3] * matrix[11]);

			if (weights[2])
			{
				matrix = &bonepose[12*bidx[2]];
				xyzout[0] += weights[2] * (xyzin[0] * matrix[0] + xyzin[1] * matrix[1] + xyzin[2] * matrix[ 2] + xyzin[3] * matrix[ 3]);
				xyzout[1] += weights[2] * (xyzin[0] * matrix[4] + xyzin[1] * matrix[5] + xyzin[2] * matrix[ 6] + xyzin[3] * matrix[ 7]);
				xyzout[2] += weights[2] * (xyzin[0] * matrix[8] + xyzin[1] * matrix[9] + xyzin[2] * matrix[10] + xyzin[3] * matrix[11]);

				if (weights[3])
				{
					matrix = &bonepose[12*bidx[3]];
					xyzout[0] += weights[3] * (xyzin[0] * matrix[0] + xyzin[1] * matrix[1] + xyzin[2] * matrix[ 2] + xyzin[3] * matrix[ 3]);
					xyzout[1] += weights[3] * (xyzin[0] * matrix[4] + xyzin[1] * matrix[5] + xyzin[2] * matrix[ 6] + xyzin[3] * matrix[ 7]);
					xyzout[2] += weights[3] * (xyzin[0] * matrix[8] + xyzin[1] * matrix[9] + xyzin[2] * matrix[10] + xyzin[3] * matrix[11]);
				}
			}
		}
	}
}

/*transforms some skeletal vecV_t values*/
static void Alias_TransformVerticies_VN(const float *bonepose, int vertcount, const qbyte *bidx, float *weights,
										const float *xyzin, float *fte_restrict xyzout,
										const float *normin, float *fte_restrict normout)
{
	int i, j;
	const float *matrix, *matrix1;
	float mat[12];
	for (i = 0; i < vertcount; i++, 
		xyzout+=sizeof(vecV_t)/sizeof(vec_t), xyzin+=sizeof(vecV_t)/sizeof(vec_t),
		normout+=sizeof(vec3_t)/sizeof(vec_t), normin+=sizeof(vec3_t)/sizeof(vec_t),
		bidx+=4, weights+=4)
	{
		matrix = &bonepose[12*bidx[0]];
		if (weights[1])
		{
			matrix1 = &bonepose[12*bidx[1]];
			for (j = 0; j < 12; j++)
				mat[j] = (weights[0] * matrix[j]) + (weights[1] * matrix1[j]);
			if (weights[2])
			{
				matrix = &bonepose[12*bidx[2]];
				for (j = 0; j < 12; j++)
					mat[j] += weights[2] * matrix[j];
				if (weights[3])
				{
					matrix = &bonepose[12*bidx[3]];
					for (j = 0; j < 12; j++)
						mat[j] += weights[3] * matrix[j];
				}
			}
			matrix = mat;
		}

		xyzout[0] = (xyzin[0] * matrix[0] + xyzin[1] * matrix[1] + xyzin[2] * matrix[ 2] + matrix[ 3]);
		xyzout[1] = (xyzin[0] * matrix[4] + xyzin[1] * matrix[5] + xyzin[2] * matrix[ 6] + matrix[ 7]);
		xyzout[2] = (xyzin[0] * matrix[8] + xyzin[1] * matrix[9] + xyzin[2] * matrix[10] + matrix[11]);

		normout[0] = (normin[0] * matrix[0] + normin[1] * matrix[1] + normin[2] * matrix[ 2]);
		normout[1] = (normin[0] * matrix[4] + normin[1] * matrix[5] + normin[2] * matrix[ 6]);
		normout[2] = (normin[0] * matrix[8] + normin[1] * matrix[9] + normin[2] * matrix[10]);
	}
}

/*transforms some skeletal vecV_t values*/
static void Alias_TransformVerticies_VNST(const float *bonepose, int vertcount, const qbyte *bidx, const float *weights,
										const float *xyzin, float *fte_restrict xyzout,
										const float *normin, float *fte_restrict normout,
										const float *sdirin, float *fte_restrict sdirout,
										const float *tdirin, float *fte_restrict tdirout)
{
	int i, j;
	const float *matrix, *matrix1;
	float mat[12];
	for (i = 0; i < vertcount; i++, bidx+=4, weights+=4)
	{
		matrix = &bonepose[12*bidx[0]];
		if (weights[1])
		{
			matrix1 = &bonepose[12*bidx[1]];
			for (j = 0; j < 12; j++)
				mat[j] = (weights[0] * matrix[j]) + (weights[1] * matrix1[j]);
			if (weights[2])
			{
				matrix = &bonepose[12*bidx[2]];
				for (j = 0; j < 12; j++)
					mat[j] += weights[2] * matrix[j];
				if (weights[3])
				{
					matrix = &bonepose[12*bidx[3]];
					for (j = 0; j < 12; j++)
						mat[j] += weights[3] * matrix[j];
				}
			}
			matrix = mat;
		}

		xyzout[0] = (xyzin[0] * matrix[0] + xyzin[1] * matrix[1] + xyzin[2] * matrix[ 2] + matrix[ 3]);
		xyzout[1] = (xyzin[0] * matrix[4] + xyzin[1] * matrix[5] + xyzin[2] * matrix[ 6] + matrix[ 7]);
		xyzout[2] = (xyzin[0] * matrix[8] + xyzin[1] * matrix[9] + xyzin[2] * matrix[10] + matrix[11]);
		xyzout+=sizeof(vecV_t)/sizeof(vec_t);
		xyzin+=sizeof(vecV_t)/sizeof(vec_t);

		normout[0] = (normin[0] * matrix[0] + normin[1] * matrix[1] + normin[2] * matrix[ 2]);
		normout[1] = (normin[0] * matrix[4] + normin[1] * matrix[5] + normin[2] * matrix[ 6]);
		normout[2] = (normin[0] * matrix[8] + normin[1] * matrix[9] + normin[2] * matrix[10]);
		normout+=sizeof(vec3_t)/sizeof(vec_t);
		normin+=sizeof(vec3_t)/sizeof(vec_t);

		sdirout[0] = (sdirin[0] * matrix[0] + sdirin[1] * matrix[1] + sdirin[2] * matrix[ 2]);
		sdirout[1] = (sdirin[0] * matrix[4] + sdirin[1] * matrix[5] + sdirin[2] * matrix[ 6]);
		sdirout[2] = (sdirin[0] * matrix[8] + sdirin[1] * matrix[9] + sdirin[2] * matrix[10]);
		sdirout+=sizeof(vec3_t)/sizeof(vec_t);
		sdirin+=sizeof(vec3_t)/sizeof(vec_t);

		tdirout[0] = (tdirin[0] * matrix[0] + tdirin[1] * matrix[1] + tdirin[2] * matrix[ 2]);
		tdirout[1] = (tdirin[0] * matrix[4] + tdirin[1] * matrix[5] + tdirin[2] * matrix[ 6]);
		tdirout[2] = (tdirin[0] * matrix[8] + tdirin[1] * matrix[9] + tdirin[2] * matrix[10]);
		tdirout+=sizeof(vec3_t)/sizeof(vec_t);
		tdirin+=sizeof(vec3_t)/sizeof(vec_t);
	}
}

static void Alias_TransformVerticies_SW(const float *bonepose, galisskeletaltransforms_t *weights, int numweights, vecV_t *xyzout, vec3_t *normout)
{
	int i;
	float *out;
	const float *matrix;
	galisskeletaltransforms_t *v = weights;
#ifndef SERVERONLY
	float *normo;
	if (normout)
	{
		for (i = 0;i < numweights;i++, v++)
		{
			out = xyzout[v->vertexindex];
			normo = normout[v->vertexindex];
			matrix = bonepose+v->boneindex*12;
			// FIXME: this can very easily be optimized with SSE or 3DNow
			out[0] += v->org[0] * matrix[0] + v->org[1] * matrix[1] + v->org[2] * matrix[ 2] + v->org[3] * matrix[ 3];
			out[1] += v->org[0] * matrix[4] + v->org[1] * matrix[5] + v->org[2] * matrix[ 6] + v->org[3] * matrix[ 7];
			out[2] += v->org[0] * matrix[8] + v->org[1] * matrix[9] + v->org[2] * matrix[10] + v->org[3] * matrix[11];

			normo[0] += v->normal[0] * matrix[0] + v->normal[1] * matrix[1] + v->normal[2] * matrix[ 2];
			normo[1] += v->normal[0] * matrix[4] + v->normal[1] * matrix[5] + v->normal[2] * matrix[ 6];
			normo[2] += v->normal[0] * matrix[8] + v->normal[1] * matrix[9] + v->normal[2] * matrix[10];
		}
	}
	else
#elif defined(_DEBUG)
	if (normout)
		Sys_Error("norms error");
#endif
	{
		for (i = 0;i < numweights;i++, v++)
		{
			out = xyzout[v->vertexindex];
			matrix = bonepose+v->boneindex*12;
			// FIXME: this can very easily be optimized with SSE or 3DNow
			out[0] += v->org[0] * matrix[0] + v->org[1] * matrix[1] + v->org[2] * matrix[ 2] + v->org[3] * matrix[ 3];
			out[1] += v->org[0] * matrix[4] + v->org[1] * matrix[5] + v->org[2] * matrix[ 6] + v->org[3] * matrix[ 7];
			out[2] += v->org[0] * matrix[8] + v->org[1] * matrix[9] + v->org[2] * matrix[10] + v->org[3] * matrix[11];
		}
	}
}

//converts one entire frame to another skeleton type
//only writes to destbuffer if absolutely needed
const float *Alias_ConvertBoneData(skeltype_t sourcetype, const float *sourcedata, size_t bonecount, galiasbone_t *bones, skeltype_t desttype, float *destbuffer, float *destbufferalt, size_t destbonecount)
{
	int i;
	if (sourcetype == desttype)
		return sourcedata;

	//everything can be converted up to SKEL_INVERSE_ABSOLUTE and back.
	//this means that everything can be converted to everything else, but it might take lots of individual transforms.
	//a->ia
	//r->a->ia
	//a->r
	//ia->ir
	//ir->ia
	//r->a->ia->ir
	//a->ia->ir

	if (bonecount > destbonecount || bonecount > MAX_BONES)
		Sys_Error("Alias_ConvertBoneData: too many bones %"PRIuSIZE">%"PRIuSIZE"\n", bonecount, destbonecount);

	//r(->a)->ia(->ir)
	if (desttype == SKEL_INVERSE_RELATIVE && sourcetype == SKEL_RELATIVE)
	{
		//for this conversion, we need absolute data.
		//this is not an efficient operation.
		sourcedata = Alias_ConvertBoneData(sourcetype, sourcedata, bonecount, bones, SKEL_ABSOLUTE, destbuffer, destbufferalt, destbonecount);
		sourcetype = SKEL_INVERSE_ABSOLUTE;
	}
	//ir->ia(->a->r)
	//ir->ia(->a)
	//a->ia(->ir)
	if ((desttype == SKEL_ABSOLUTE && sourcetype == SKEL_INVERSE_RELATIVE) ||
		(desttype == SKEL_RELATIVE && sourcetype == SKEL_INVERSE_RELATIVE) ||
		(desttype == SKEL_INVERSE_RELATIVE && sourcetype == SKEL_ABSOLUTE))
	{
		//for this conversion, we need absolute data.
		//this is not an efficient operation.
		sourcedata = Alias_ConvertBoneData(sourcetype, sourcedata, bonecount, bones, SKEL_INVERSE_ABSOLUTE, destbuffer, destbufferalt, destbonecount);
		sourcetype = SKEL_INVERSE_ABSOLUTE;
	}

	//r->a
	//r->a(->ia)
	//ir->ia
	if ((sourcetype == SKEL_RELATIVE && (desttype == SKEL_ABSOLUTE || desttype == SKEL_INVERSE_ABSOLUTE)) ||
		(sourcetype == SKEL_INVERSE_RELATIVE && desttype == SKEL_INVERSE_ABSOLUTE))
	{
		float *dest = (sourcedata == destbuffer)?destbufferalt:destbuffer;
		/*needs to be an absolute skeleton*/
		for (i = 0; i < bonecount; i++)
		{
			if (bones[i].parent >= 0)
				R_ConcatTransforms((void*)(dest + bones[i].parent*12), (void*)(sourcedata+i*12), (void*)(dest+i*12));
			else
			{
				Vector4Copy(sourcedata+i*12+0, dest+i*12+0);
				Vector4Copy(sourcedata+i*12+4, dest+i*12+4);
				Vector4Copy(sourcedata+i*12+8, dest+i*12+8);
			}
		}
		sourcedata = dest;
		if (sourcetype == SKEL_INVERSE_RELATIVE)
			sourcetype = SKEL_INVERSE_ABSOLUTE;
		else
			sourcetype = SKEL_ABSOLUTE;
	}

	//ia->a(->r)
	//ia->a
	if ((desttype == SKEL_RELATIVE || desttype == SKEL_ABSOLUTE) && sourcetype == SKEL_INVERSE_ABSOLUTE)
	{
		float iim[12];
		float *dest = (sourcedata == destbuffer)?destbufferalt:destbuffer;
		for (i = 0; i < bonecount; i++)
		{
			Matrix3x4_Invert_Simple(bones[i].inverse, iim);
			R_ConcatTransforms((void*)(sourcedata + i*12), (void*)iim, (void*)(dest + i*12));
		}
		sourcedata = dest;
		sourcetype = SKEL_ABSOLUTE;
	}

	//ia->ir
	//a->r
	if ((desttype == SKEL_RELATIVE && sourcetype == SKEL_ABSOLUTE) ||
		(desttype == SKEL_INVERSE_RELATIVE && sourcetype == SKEL_INVERSE_ABSOLUTE))
	{
		float ip[12];
		float *dest = (sourcedata == destbuffer)?destbufferalt:destbuffer;
		for (i = 0; i < bonecount; i++)
		{
			if (bones[i].parent >= 0)
			{
				Matrix3x4_Invert_Simple(sourcedata+bones[i].parent*12, ip);
				R_ConcatTransforms((void*)ip, (void*)(sourcedata+i*12), (void*)(dest+i*12));
			}
			else
			{
				Vector4Copy(sourcedata+i*12+0, dest+i*12+0);
				Vector4Copy(sourcedata+i*12+4, dest+i*12+4);
				Vector4Copy(sourcedata+i*12+8, dest+i*12+8);
			}
		}
		sourcedata = dest;
		if (sourcetype == SKEL_INVERSE_ABSOLUTE)
			sourcetype = SKEL_INVERSE_RELATIVE;
		else
			sourcetype = SKEL_RELATIVE;
	}

	//a->ia
	if (desttype == SKEL_INVERSE_ABSOLUTE && sourcetype == SKEL_ABSOLUTE)
	{
		float *dest = (sourcedata == destbuffer)?destbufferalt:destbuffer;
		for (i = 0; i < bonecount; i++)
			R_ConcatTransforms((void*)(sourcedata + i*12), (void*)(bones[i].inverse), (void*)(dest + i*12));
		sourcedata = dest;
		sourcetype = SKEL_INVERSE_ABSOLUTE;
	}
	
	if (sourcetype == SKEL_IDENTITY)
	{	//we can 'convert' identity matricies to anything. but we only want to do this when everything else is bad, because there really is no info here
		float *dest = (sourcedata == destbuffer)?destbufferalt:destbuffer;
		memset(dest, 0, bonecount*12*sizeof(float));
		for (i = 0; i < bonecount; i++)
		{	//is this right? does it matter?
			dest[i*12+0] = 1;
			dest[i*12+5] = 1;
			dest[i*12+10] = 1;
		}
		sourcedata = dest;
		sourcetype = desttype;	//panic
	}

	if (sourcetype != desttype)
		Sys_Error("Alias_ConvertBoneData: %i->%i not supported\n", (int)sourcetype, (int)desttype);

	return sourcedata;
}
/*
converts the bone data from source to dest.
uses parent bone info, so don't try to offset for a first bone.
ALWAYS writes dest. Don't force it if you don't want to waste cycles when no conversion is actually needed.
destbonecount is to catch errors, its otherwise ignored for now. no identity padding.
*/
void QDECL Alias_ForceConvertBoneData(skeltype_t sourcetype, const float *sourcedata, size_t bonecount, galiasbone_t *bones, skeltype_t desttype, float *destbuffer, size_t destbonecount)
{
	float altbuffer[MAX_BONES*12];
	const float *buf = Alias_ConvertBoneData(sourcetype, sourcedata, bonecount, bones, desttype, destbuffer, altbuffer, destbonecount);
	if (buf != destbuffer)
	{
		//Alias_ConvertBoneData successfully managed to avoid doing any work. bah.
		memcpy(destbuffer, buf, bonecount*12*sizeof(float));
	}
}

#ifndef NOLEGACY
static float Alias_CalculateSkeletalNormals(galiasinfo_t *model)
{
#ifndef SERVERONLY
	//servers don't need normals. except maybe for tracing... but hey. The normal is calculated on a per-triangle basis.

#define TriangleNormal(a,b,c,n) ( \
	(n)[0] = ((a)[1] - (b)[1]) * ((c)[2] - (b)[2]) - ((a)[2] - (b)[2]) * ((c)[1] - (b)[1]), \
	(n)[1] = ((a)[2] - (b)[2]) * ((c)[0] - (b)[0]) - ((a)[0] - (b)[0]) * ((c)[2] - (b)[2]), \
	(n)[2] = ((a)[0] - (b)[0]) * ((c)[1] - (b)[1]) - ((a)[1] - (b)[1]) * ((c)[0] - (b)[0]) \
	)
	int i, j;
	vecV_t *xyz;
	vec3_t *normals;
	int *mvert;
	float *inversepose;
	galiasinfo_t *next;
	vec3_t tn;
	vec3_t d1, d2;
	index_t *idx;
	const float *bonepose = NULL;
	float angle;
	float maxvdist = 0, d, maxbdist = 0;
	float absmatrix[MAX_BONES*12];
	float absmatrixalt[MAX_BONES*12];
	float bonedist[MAX_BONES];
	int modnum = 0;
	int bcmodnum = -1;
	int vcmodnum = -1;

	while (model)
	{
		int numbones = model->numbones;
		galisskeletaltransforms_t *v = model->ofsswtransforms;
		int numweights = model->numswtransforms;
		int numverts = model->numverts;

		next = model->nextsurf;

		xyz = Z_Malloc(numverts*sizeof(vecV_t));
		normals = Z_Malloc(numverts*sizeof(vec3_t));
		inversepose = Z_Malloc(numbones*sizeof(float)*9);
		mvert = Z_Malloc(numverts*sizeof(*mvert));

		if (bcmodnum != model->shares_bones)
		{
			galiasanimation_t *g;
			galiasbone_t *bones = model->ofsbones;
			bcmodnum = model->shares_bones;
			if (model->baseframeofs)
				bonepose = model->baseframeofs;
			else
			{
				//figure out the pose from frame0pose0
				if (!model->numanimations)
					return 0;
				g = model->ofsanimations;
				if (g->numposes < 1)
					return 0;
				bonepose = Alias_ConvertBoneData(g->skeltype, g->boneofs, numbones, bones, SKEL_ABSOLUTE, absmatrix, absmatrixalt, MAX_BONES);
			}
			/*calculate the bone sizes (assuming the bones are strung up and hanging or such)*/
			for (i = 0; i < model->numbones; i++)
			{
				vec3_t d;
				const float *b;
				b = bonepose + i*12;
				d[0] = b[3];
				d[1] = b[7];
				d[2] = b[11];
				if (bones[i].parent >= 0)
				{
					b = bonepose + bones[i].parent*12;
					d[0] -= b[3];
					d[1] -= b[7];
					d[2] -= b[11];
				}
				bonedist[i] = Length(d);
				if (bones[i].parent >= 0)
					bonedist[i] += bonedist[bones[i].parent];
				if (maxbdist < bonedist[i])
					maxbdist = bonedist[i];
			}
			for (i = 0; i < numbones; i++)
				Matrix3x4_InvertTo3x3(bonepose+i*12, inversepose+i*9);
		}

		for (i = 0; i < numweights; i++)
		{
			d = Length(v[i].org);
			if (maxvdist < d)
				maxvdist = d;
		}

		//build the actual base pose positions
		Alias_TransformVerticies_SW(bonepose, v, numweights, xyz, NULL);

		//work out which verticies are identical
		//this is needed as two verts can have same origin but different tex coords
		//without this, we end up with a seam that splits the normals each side on arms, etc
		for (i = 0; i < numverts; i++)
		{
			mvert[i] = i;
			for (j = 0; j < i; j++)
			{
				if (	xyz[i][0] == xyz[j][0]
					&&	xyz[i][1] == xyz[j][1]
					&&	xyz[i][2] == xyz[j][2])
				{
					mvert[i] = j;
					break;
				}
			}
		}

		//use that base pose to calculate the normals
		memset(normals, 0, numverts*sizeof(vec3_t));
		vcmodnum = modnum;
		idx = model->ofs_indexes;

		//calculate the triangle normal and accumulate them
		for (i = 0; i < model->numindexes; i+=3, idx+=3)
		{
			TriangleNormal(xyz[idx[0]], xyz[idx[1]], xyz[idx[2]], tn);
			//note that tn is relative to the size of the triangle

			//Imagine a cube, each side made of two triangles

			VectorSubtract(xyz[idx[1]], xyz[idx[0]], d1);
			VectorSubtract(xyz[idx[2]], xyz[idx[0]], d2);
			angle = acos(DotProduct(d1, d2)/(Length(d1)*Length(d2)));
			VectorMA(normals[mvert[idx[0]]], angle, tn, normals[mvert[idx[0]]]);

			VectorSubtract(xyz[idx[0]], xyz[idx[1]], d1);
			VectorSubtract(xyz[idx[2]], xyz[idx[1]], d2);
			angle = acos(DotProduct(d1, d2)/(Length(d1)*Length(d2)));
			VectorMA(normals[mvert[idx[1]]], angle, tn, normals[mvert[idx[1]]]);

			VectorSubtract(xyz[idx[0]], xyz[idx[2]], d1);
			VectorSubtract(xyz[idx[1]], xyz[idx[2]], d2);
			angle = acos(DotProduct(d1, d2)/(Length(d1)*Length(d2)));
			VectorMA(normals[mvert[idx[2]]], angle, tn, normals[mvert[idx[2]]]);
		}

		/*skip over each additional surface that shares the same verts*/
		for(;;)
		{
			if (next && next->shares_verts == vcmodnum)
			{
				modnum++;
				model = next;
				next = model->nextsurf;
			}
			else
				break;
		}

		//the normals are not normalized yet.
		for (i = 0; i < numverts; i++)
		{
			VectorNormalize(normals[i]);
		}

		for (i = 0; i < numweights; i++, v++)
		{
			v->normal[0] = DotProduct(normals[mvert[v->vertexindex]], inversepose+9*v->boneindex+0) * v->org[3];
			v->normal[1] = DotProduct(normals[mvert[v->vertexindex]], inversepose+9*v->boneindex+3) * v->org[3];
			v->normal[2] = DotProduct(normals[mvert[v->vertexindex]], inversepose+9*v->boneindex+6) * v->org[3];
		}

		if (model->ofs_skel_norm)
			memcpy(model->ofs_skel_norm, normals, numverts*sizeof(vec3_t));

		//FIXME: save off the xyz+normals for this base pose as an optimisation for world objects.
		Z_Free(inversepose);
		Z_Free(normals);
		Z_Free(xyz);
		Z_Free(mvert);

		model = next;
		modnum++;
	}
	return maxvdist+maxbdist;
#else
	return 0;
#endif
}
#endif
#endif





#if 1

struct
{
	int numcoords;
	vecV_t *coords;

	int numnorm;
	vec3_t *norm;

	int bonegroup;
	int vertgroup;
	entity_t *ent;

#ifdef SKELETALMODELS
	float boneposebuffer1[MAX_BONES*12];
	float boneposebuffer2[MAX_BONES*12];
	skeltype_t bonecachetype;
	const float *usebonepose;
	int bonecount;
#endif
	qboolean usebones;

	vecV_t *acoords1;
	vecV_t *acoords2;
	vec3_t *anorm;
	vec3_t *anorms;
	vec3_t *anormt;
	float lerp;

	vbo_t vbo;
	vbo_t *vbop;
} meshcache;

//#define SSE_INTRINSICS
#ifdef SSE_INTRINSICS
#include <xmmintrin.h>
#endif

#ifndef SERVERONLY
void R_LightArraysByte_BGR(const entity_t *entity, vecV_t *coords, byte_vec4_t *colours, int vertcount, vec3_t *normals)
{
	int i;
	int c;
	float l;

	byte_vec4_t ambientlightb;
	byte_vec4_t shadelightb;
	const float *lightdir = entity->light_dir;

	for (i = 0; i < 3; i++)
	{
		l = entity->light_avg[2-i]*255;
		ambientlightb[i] = bound(0, l, 255);
		l = entity->light_range[2-i]*255;
		shadelightb[i] = bound(0, l, 255);
	}

	if (ambientlightb[0] == shadelightb[0] && ambientlightb[1] == shadelightb[1] && ambientlightb[2] == shadelightb[2])
	{
		for (i = vertcount-1; i >= 0; i--)
		{
			*(int*)colours[i] = *(int*)ambientlightb;
//			colours[i][0] = ambientlightb[0];
//			colours[i][1] = ambientlightb[1];
//			colours[i][2] = ambientlightb[2];
		}
	}
	else
	{
		for (i = vertcount-1; i >= 0; i--)
		{
			l = DotProduct(normals[i], lightdir);
			c = l*shadelightb[0];
			c += ambientlightb[0];
			colours[i][0] = bound(0, c, 255);
			c = l*shadelightb[1];
			c += ambientlightb[1];
			colours[i][1] = bound(0, c, 255);
			c = l*shadelightb[2];
			c += ambientlightb[2];
			colours[i][2] = bound(0, c, 255);
		}
	}
}

void R_LightArrays(const entity_t *entity, vecV_t *coords, avec4_t *colours, int vertcount, vec3_t *normals, float scale)
{
	extern cvar_t r_vertexdlights;
	int i;
	float l;

	//float *lightdir = currententity->light_dir; //unused variable

	if (!entity->light_range[0] && !entity->light_range[1] && !entity->light_range[2])
	{
		for (i = vertcount-1; i >= 0; i--)
		{
			colours[i][0] = entity->light_avg[0];
			colours[i][1] = entity->light_avg[1];
			colours[i][2] = entity->light_avg[2];
		}
	}
	else
	{
		vec3_t la, lr;
		VectorScale(entity->light_avg, scale, la);
		VectorScale(entity->light_range, scale, lr);
#ifdef SSE_INTRINSICS
		__m128 va, vs, vl, vr;
		va = _mm_load_ps(ambientlight);
		vs = _mm_load_ps(shadelight);
		va.m128_f32[3] = 0;
		vs.m128_f32[3] = 1;
#endif
		/*dotproduct will return a value between 1 and -1, so increase the ambient to be correct for normals facing away from the light*/
		for (i = vertcount-1; i >= 0; i--)
		{
			l = DotProduct(normals[i], entity->light_dir);
	#ifdef SSE_INTRINSICS
			vl = _mm_load1_ps(&l);
			vr = _mm_mul_ss(va,vl);
			vr = _mm_add_ss(vr,vs);

			_mm_storeu_ps(colours[i], vr);
			//stomp on colour[i][3] (will be set to 1)
	#else
			colours[i][0] = l*lr[0]+la[0];
			colours[i][1] = l*lr[1]+la[1];
			colours[i][2] = l*lr[2]+la[2];
	#endif
		}
	}

	if (r_vertexdlights.ival && r_dynamic.ival > 0)
	{
		unsigned int lno, v;
		vec3_t dir, rel;
		float dot, d, a;
		//don't include world lights
		for (lno = rtlights_first; lno < RTL_FIRST; lno++)
		{
			if (cl_dlights[lno].radius)
			{
				VectorSubtract (cl_dlights[lno].origin,
								entity->origin,
								dir);
				if (Length(dir)>cl_dlights[lno].radius+256)	//far out man!
					continue;

				rel[0] = -DotProduct(dir, entity->axis[0]);
				rel[1] = -DotProduct(dir, entity->axis[1]);
				rel[2] = -DotProduct(dir, entity->axis[2]);

				for (v = 0; v < vertcount; v++)
				{
					VectorSubtract(coords[v], rel, dir);
					dot = DotProduct(dir, normals[v]);
					if (dot>0)
					{
						d = DotProduct(dir, dir);
						a = 1/d;
						if (a>0)
						{
							a *= 10000000*dot/sqrt(d);
							colours[v][0] += a*cl_dlights[lno].color[0];
							colours[v][1] += a*cl_dlights[lno].color[1];
							colours[v][2] += a*cl_dlights[lno].color[2];
						}
					}
				}
			}
		}
	}
}
static void R_LerpFrames(mesh_t *mesh, galiaspose_t *p1, galiaspose_t *p2, float lerp, float expand, float lerpcutoff)
{
	extern cvar_t r_nolerp; // r_nolightdir is unused
	float blerp = 1-lerp;
	int i;
	vecV_t *p1v = p1->ofsverts, *p2v = p2->ofsverts;
	vec3_t *p1n = p1->ofsnormals, *p2n = p2->ofsnormals;
	vec3_t *p1s = p1->ofssvector, *p2s = p2->ofssvector;
	vec3_t *p1t = p1->ofstvector, *p2t = p2->ofstvector;

	mesh->snormals_array = blerp>0.5?p2s:p1s;		//never lerp
	mesh->tnormals_array = blerp>0.5?p2t:p1t;		//never lerp
	mesh->colors4f_array[0] = NULL;	//not generated

	if (p1v == p2v || r_nolerp.value || !blerp)
	{
		mesh->normals_array = p1n;
		mesh->snormals_array = p1s;
		mesh->tnormals_array = p1t;

		if (expand)
		{
			vecV_t *oxyz = mesh->xyz_array;
			for (i = 0; i < mesh->numvertexes; i++)
			{
				oxyz[i][0] = p1v[i][0] + p1n[i][0]*expand;
				oxyz[i][1] = p1v[i][1] + p1n[i][1]*expand;
				oxyz[i][2] = p1v[i][2] + p1n[i][2]*expand;
			}
			return;
		}
		else
			mesh->xyz_array = p1v;
	}
	else
	{
		vecV_t *oxyz = mesh->xyz_array;
		vec3_t *onorm = mesh->normals_array;
		if (lerpcutoff)
		{
			vec3_t d;
			lerpcutoff *= lerpcutoff;
			for (i = 0; i < mesh->numvertexes; i++)
			{
				VectorSubtract(p2v[i], p1v[i], d);
				if (DotProduct(d, d) > lerpcutoff)
				{
					//just use the current frame if we're over the lerp threshold.
					//these verts are considered to have teleported.
					onorm[i][0] = p2n[i][0];
					onorm[i][1] = p2n[i][1];
					onorm[i][2] = p2n[i][2];

					oxyz[i][0] = p2v[i][0];
					oxyz[i][1] = p2v[i][1];
					oxyz[i][2] = p2v[i][2];
				}
				else
				{
					onorm[i][0] = p1n[i][0]*lerp + p2n[i][0]*blerp;
					onorm[i][1] = p1n[i][1]*lerp + p2n[i][1]*blerp;
					onorm[i][2] = p1n[i][2]*lerp + p2n[i][2]*blerp;

					oxyz[i][0] = p1v[i][0]*lerp + p2v[i][0]*blerp;
					oxyz[i][1] = p1v[i][1]*lerp + p2v[i][1]*blerp;
					oxyz[i][2] = p1v[i][2]*lerp + p2v[i][2]*blerp;
				}
			}
		}
		else
		{
			for (i = 0; i < mesh->numvertexes; i++)
			{
				onorm[i][0] = p1n[i][0]*lerp + p2n[i][0]*blerp;
				onorm[i][1] = p1n[i][1]*lerp + p2n[i][1]*blerp;
				onorm[i][2] = p1n[i][2]*lerp + p2n[i][2]*blerp;

				oxyz[i][0] = p1v[i][0]*lerp + p2v[i][0]*blerp;
				oxyz[i][1] = p1v[i][1]*lerp + p2v[i][1]*blerp;
				oxyz[i][2] = p1v[i][2]*lerp + p2v[i][2]*blerp;
			}
		}

		if (expand)
		{
			for (i = 0; i < mesh->numvertexes; i++)
			{
				oxyz[i][0] += onorm[i][0]*expand;
				oxyz[i][1] += onorm[i][1]*expand;
				oxyz[i][2] += onorm[i][2]*expand;
			}
		}
	}
}
#endif
#endif

#ifdef SKELETALMODELS
/*
	returns the up-to-8 skeletal bone poses to blend together.
	return value is the number of blends that are actually live.
*/
typedef struct
{
	skeltype_t	skeltype;	//the skeletal type of this bone block. all blocks should have the same result or the whole thing is unusable or whatever.
	int			firstbone;	//first bone of interest
	int			endbone;	//the first bone of the next group (ie: if first is 0, this is the count)
	float		frac[8];	//weight of this animation (1 if lerpcount is 1)
	float		*pose[8];	//pointer to the raw frame data for bone 0.
	int			lerpcount;	//number of pose+frac entries.
} skellerps_t;
static qboolean Alias_BuildSkelLerps(skellerps_t *lerps, struct framestateregion_s *fs, int numbones, galiasinfo_t *inf)
{
	unsigned int frame1;
	unsigned int frame2;
	float mlerp;	//minor lerp, poses within a group.
	int l = 0;
	galiasanimation_t *g;
	unsigned int b;
	float totalweight = 0;
#ifndef SERVERONLY
	extern cvar_t r_nolerp;
#endif

	lerps->skeltype = SKEL_IDENTITY;	//sometimes nothing else is valid.

	for (b = 0; b < FRAME_BLENDS; b++)
	{
		if (fs->lerpweight[b])
		{
			unsigned int frame = fs->frame[b];
			float time = fs->frametime[b];
			if (frame >= inf->numanimations)
				continue;//frame = (unsigned)frame%inf->groups;

			g = &inf->ofsanimations[frame];
			if (!g->numposes)
				continue;	//err...

			mlerp = time*g->rate;
			frame1=mlerp;
			frame2=frame1+1;
			mlerp-=frame1;
			if (g->loop)
			{	//loop normally.
				frame1=frame1%g->numposes;
				frame2=frame2%g->numposes;
			}
			else
			{
				frame1=(frame1>g->numposes-1)?g->numposes-1:frame1;
				frame2=(frame2>g->numposes-1)?g->numposes-1:frame2;
			}

			if (lerps->skeltype == SKEL_IDENTITY)
				lerps->skeltype = g->skeltype;
			else if (lerps->skeltype != g->skeltype)
				continue;	//oops, can't cope with mixed blend types

			if (frame1 == frame2)
				mlerp = 0;
			else if (r_noframegrouplerp.ival)
				mlerp = (mlerp>0.5)?1:0;
			lerps->frac[l] = (1-mlerp)*fs->lerpweight[b];
			if (lerps->frac[l]>0)
			{
				totalweight += lerps->frac[l];
				lerps->pose[l++] = g->boneofs + numbones*12*frame1;
			}
			lerps->frac[l] = (mlerp)*fs->lerpweight[b];
			if (lerps->frac[l]>0)
			{
				totalweight += lerps->frac[l];
				lerps->pose[l++] = g->boneofs + numbones*12*frame2;
			}
		}
	}

#ifndef SERVERONLY
	if (r_nolerp.ival && l > 1)
	{	//when lerping is completely disabled, find the strongest influence
		frame1 = 0;
		mlerp = lerps->frac[0];
		for (b = 1; b < l; b++)
		{
			if (lerps->frac[b] > mlerp)
			{
				frame1 = b;
				mlerp = lerps->frac[b];
			}
		}
		lerps->frac[0] = 1;
		lerps->pose[0] = lerps->pose[frame1];
		l = 1;
	}
	else
#endif
		if (l && totalweight != 1)
	{	//don't rescale if some animation got dropped.
		totalweight = 1 / totalweight;
		for (b = 0; b < l; b++)
		{
			lerps->frac[b] *= totalweight;
		}
	}

	lerps->lerpcount = l;
	return l > 0;
}
/*
finds the various blend info. returns number of bone blocks used.
*/
static int Alias_FindRawSkelData(galiasinfo_t *inf, framestate_t *fstate, skellerps_t *lerps, size_t firstbone, size_t lastbone)
{
	int bonegroup;
	int cbone = 0;
	int endbone;
	int numbonegroups=0;

	if (lastbone > inf->numbones)
		lastbone = inf->numbones;

	for (bonegroup = 0; bonegroup < FS_COUNT; bonegroup++)
	{
		endbone = fstate->g[bonegroup].endbone;
		if (bonegroup == FS_COUNT-1 || endbone > lastbone)
			endbone = lastbone;

		if (endbone == cbone)
			continue;

		if (!inf->numanimations || !Alias_BuildSkelLerps(lerps, &fstate->g[bonegroup], inf->numbones, inf))	//if there's no animations in this model, use the base pose instead.
		{
			if (!inf->baseframeofs)
				continue;	//nope, not happening.
			lerps->skeltype = SKEL_ABSOLUTE;
			lerps->frac[0] = 1;
			lerps->pose[0] = inf->baseframeofs;
			lerps->lerpcount = 1;
		}
		lerps->firstbone = cbone;
		lerps->endbone = endbone;
		cbone = endbone;
		numbonegroups++;
		lerps++;
	}
	return numbonegroups;
}
/*
	retrieves the raw bone data for a current frame state.
	ignores poses that don't match the desired skeltype
	ignores skeletal objects.
	return value is the lastbone argument, or less if the model simply doesn't have that many bones.
	_always_ writes into result
*/
int Alias_BlendBoneData(galiasinfo_t *inf, framestate_t *fstate, float *result, skeltype_t skeltype, int firstbone, int lastbone)
{
	skellerps_t lerps[FS_COUNT], *lerp;
	size_t bone, endbone = 0;
	size_t numgroups = Alias_FindRawSkelData(inf, fstate, lerps, firstbone, lastbone);

	float *pose, *matrix;
	int k, b;

	for (lerp = lerps; numgroups--; lerp++)
	{
		if (lerp[0].skeltype != skeltype)
			continue;	//egads, its buggy. should probably convert.

		bone = lerp->firstbone;
		endbone = lerp->endbone;
		if (lerp->lerpcount == 1 && lerp->frac[0] == 1)
			memcpy(result+bone*12, lerp->pose[0]+bone*12, (endbone-bone)*12*sizeof(float));
		else
		{
			//set up the identity matrix
			for (; bone < endbone; bone++)
			{
				pose = result + 12*bone;
				//set up the per-bone transform matrix
				matrix = lerps->pose[0] + bone*12;
				for (k = 0;k < 12;k++)
					pose[k] = matrix[k] * lerp->frac[0];
				for (b = 1;b < lerp->lerpcount;b++)
				{
					matrix = lerps->pose[b] + bone*12;

					for (k = 0;k < 12;k++)
						pose[k] += matrix[k] * lerp->frac[b];
				}
			}
		}
	}
	return endbone;
}

/*retrieves the bone data.
only writes targetbuffer if needed. the return value is the only real buffer result.
assumes that all blended types are the same. probably buggy, but meh.
*/
const float *Alias_GetBoneInformation(galiasinfo_t *inf, framestate_t *framestate, skeltype_t targettype, float *targetbuffer, float *targetbufferalt, size_t maxbufferbones)
{
	skellerps_t lerps[FS_COUNT], *lerp;
	size_t numgroups;
	size_t bone, endbone;

#ifdef SKELETALOBJECTS
	if (framestate->bonestate && framestate->bonecount >= inf->numbones)
	{
		lerps[0].skeltype = framestate->skeltype;
		lerps[0].firstbone = 0;
		lerps[0].endbone = framestate->bonecount;
		lerps[0].pose[0] = framestate->bonestate;
		lerps[0].frac[0] = 1;
		lerps[0].lerpcount = 1;
		numgroups = 1;
	}
	else
#endif
	{
		numgroups = Alias_FindRawSkelData(inf, framestate, lerps, 0, inf->numbones);
	}

	//try to return data in-place.
	if (numgroups==1 && lerps[0].lerpcount == 1)
		return Alias_ConvertBoneData(lerps[0].skeltype, lerps[0].pose[0], lerps[0].endbone, inf->ofsbones, targettype, targetbuffer, targetbufferalt, maxbufferbones);

	for (lerp = lerps; numgroups--; lerp++)
	{
		bone = lerp->firstbone;
		endbone = lerp->endbone;
		switch(lerp->lerpcount)
		{
		case 1://no blend required, data can be used as-is, once merged with the other bone groups, anyway.
			memcpy(targetbuffer+bone*12, lerp->pose[0]+bone*12, (endbone-bone)*12*sizeof(float));
			break;
		case 2:
			{
				int k;
				float *out = targetbuffer + bone*12;
				float *pose1 = lerp->pose[0] + bone*12, *pose2 = lerp->pose[1] + bone*12;
				float frac1 = lerp->frac[0], frac2 = lerp->frac[1];
				for (; bone < endbone; bone++, out+=12, pose1+=12, pose2+=12)
				{
					for (k = 0; k < 12; k++)	//please please unroll!
						out[k] = (pose1[k]*frac1) + (frac2*pose2[k]);
				}
			}
			break;
		case 3:
			{
				int k;
				float *out = targetbuffer + bone*12;
				float *pose1 = lerp->pose[0] + bone*12, *pose2 = lerp->pose[1] + bone*12, *pose3 = lerp->pose[2] + bone*12;
				float frac1 = lerp->frac[0], frac2 = lerp->frac[1], frac3 = lerp->frac[2];
				for (; bone < endbone; bone++, out+=12, pose1+=12, pose2+=12, pose3+=12)
				{
					for (k = 0; k < 12; k++)	//please please unroll!
						out[k] = (pose1[k]*frac1) + (frac2*pose2[k]) + (pose3[k]*frac3);
				}
			}
			break;
		case 4:
			{
				int k;
				float *out = targetbuffer + bone*12;
				float *pose1 = lerp->pose[0] + bone*12, *pose2 = lerp->pose[1] + bone*12, *pose3 = lerp->pose[2] + bone*12, *pose4 = lerp->pose[3] + bone*12;
				float frac1 = lerp->frac[0], frac2 = lerp->frac[1], frac3 = lerp->frac[2], frac4 = lerp->frac[3];
				for (; bone < endbone; bone++, out+=12, pose1+=12, pose2+=12, pose3+=12, pose4+=12)
				{
					for (k = 0; k < 12; k++)	//please please unroll!
						out[k] = (pose1[k]*frac1) + (frac2*pose2[k]) + (pose3[k]*frac3) + (frac4*pose4[k]);
				}
			}
			break;
		}
	}

	return Alias_ConvertBoneData(lerps[0].skeltype, targetbuffer, inf->numbones, inf->ofsbones, targettype, targetbuffer, targetbufferalt, maxbufferbones);
}

static void Alias_BuildSkeletalMesh(mesh_t *mesh, framestate_t *framestate, galiasinfo_t *inf)
{
	if (inf->ofs_skel_idx)
	{
		qbyte *fte_restrict bidx = inf->ofs_skel_idx[0];
		float *fte_restrict weight = inf->ofs_skel_weight[0];

		if (meshcache.bonecachetype != SKEL_INVERSE_ABSOLUTE)
			meshcache.usebonepose = Alias_GetBoneInformation(inf, framestate, meshcache.bonecachetype=SKEL_INVERSE_ABSOLUTE, meshcache.boneposebuffer1, meshcache.boneposebuffer2, MAX_BONES);

		if (1)
			Alias_TransformVerticies_VNST(meshcache.usebonepose, inf->numverts, bidx, weight, 
					inf->ofs_skel_xyz[0], mesh->xyz_array[0],
					inf->ofs_skel_norm[0], mesh->normals_array[0],
					inf->ofs_skel_svect[0], mesh->snormals_array[0],
					inf->ofs_skel_tvect[0], mesh->tnormals_array[0]
					);
		else
			Alias_TransformVerticies_VN(meshcache.usebonepose, inf->numverts, bidx, weight,
					inf->ofs_skel_xyz[0], mesh->xyz_array[0],
					inf->ofs_skel_norm[0], mesh->normals_array[0]
					);
	}
	else
	{
		galisskeletaltransforms_t *weights = inf->ofsswtransforms;
		int numweights = inf->numswtransforms;
		if (meshcache.bonecachetype != SKEL_ABSOLUTE)
			meshcache.usebonepose = Alias_GetBoneInformation(inf, framestate, meshcache.bonecachetype=SKEL_ABSOLUTE, meshcache.boneposebuffer1, meshcache.boneposebuffer2, MAX_BONES);
		memset(mesh->xyz_array, 0, mesh->numvertexes*sizeof(vecV_t));
		memset(mesh->normals_array, 0, mesh->numvertexes*sizeof(vec3_t));
		Alias_TransformVerticies_SW(meshcache.usebonepose, weights, numweights, mesh->xyz_array, mesh->normals_array);
	}
}

static void Alias_BuildSkeletalVPositionsPose(float *xyzout, skeltype_t bonetype, const float *bonepose, galiasinfo_t *inf)
{
	float buffer[MAX_BONES*12];
	float bufferalt[MAX_BONES*12];
	if (inf->ofs_skel_idx)
	{
		qbyte *fte_restrict bidx = inf->ofs_skel_idx[0];
		float *fte_restrict xyzin = inf->ofs_skel_xyz[0];
		float *fte_restrict weight = inf->ofs_skel_weight[0];
		bonepose = Alias_ConvertBoneData(bonetype, bonepose, inf->numbones, inf->ofsbones, SKEL_INVERSE_ABSOLUTE, buffer, bufferalt, MAX_BONES);

		Alias_TransformVerticies_V(bonepose, inf->numverts, bidx, weight, xyzin, xyzout);
	}
	else
	{
		galisskeletaltransforms_t *weights = inf->ofsswtransforms;
		int numweights = inf->numswtransforms;
		bonepose = Alias_ConvertBoneData(bonetype, bonepose, inf->numbones, inf->ofsbones, SKEL_ABSOLUTE, buffer, bufferalt, MAX_BONES);
		Alias_TransformVerticies_SW(bonepose, weights, numweights, (vecV_t*)xyzout, NULL);
	}
}

#ifndef SERVERONLY
static void Alias_DrawSkeletalBones(galiasbone_t *bones, float *bonepose, int bonecount, int basebone)
{
	scenetris_t *t;
	int flags = BEF_NODLIGHT|BEF_NOSHADOWS|BEF_LINES;
	shader_t *shader;
	int i, p;
	extern	entity_t	*currententity;
	index_t *indexes;
	vecV_t *verts;
	vec4_t *colours;
	vec2_t *texcoords;
	int numindexes = 0;
	mesh_t bonemesh, *m;
	batch_t b;

	//this shader lookup might get pricy.
	shader = R_RegisterShader("shader_draw_line", SUF_NONE,
		"{\n"
			"program defaultfill\n"
			"{\n"
				"map $whiteimage\n"
				"rgbgen exactvertex\n"
				"alphagen vertex\n"
				"blendfunc blend\n"
			"}\n"
		"}\n");

	if (cl_numstris && cl_stris[cl_numstris-1].shader == shader && cl_stris[cl_numstris-1].flags == flags)
		t = &cl_stris[cl_numstris-1];
	else
	{
		if (cl_numstris == cl_maxstris)
		{
			cl_maxstris += 8;
			cl_stris = BZ_Realloc(cl_stris, sizeof(*cl_stris)*cl_maxstris);
		}
		t = &cl_stris[cl_numstris++];
		t->shader = shader;
		t->numidx = 0;
		t->numvert = 0;
		t->firstidx = cl_numstrisidx;
		t->firstvert = cl_numstrisvert;
		t->flags = flags;
	}
	if (cl_numstrisvert + bonecount*2 > cl_maxstrisvert)
	{
		cl_maxstrisvert = cl_numstrisvert + bonecount*2;

		cl_strisvertv = BZ_Realloc(cl_strisvertv, sizeof(*cl_strisvertv)*cl_maxstrisvert);
		cl_strisvertt = BZ_Realloc(cl_strisvertt, sizeof(vec2_t)*cl_maxstrisvert);
		cl_strisvertc = BZ_Realloc(cl_strisvertc, sizeof(vec4_t)*cl_maxstrisvert);
	}
	if (cl_maxstrisidx < cl_numstrisidx+bonecount*2)
	{
		cl_maxstrisidx = cl_numstrisidx+bonecount*2;
		cl_strisidx = BZ_Realloc(cl_strisidx, sizeof(*cl_strisidx)*cl_maxstrisidx);
	}

	verts = alloca(sizeof(*verts)*bonecount);
	colours = alloca(sizeof(*colours)*bonecount);
	texcoords = alloca(sizeof(*texcoords)*bonecount);
	indexes = alloca(sizeof(*indexes)*bonecount*2);
	numindexes = 0;

	for (i = 0; i < bonecount; i++)
	{
		//fixme: transform by model matrix
		verts[i][0] = bonepose[i*12+3];
		verts[i][1] = bonepose[i*12+7];
		verts[i][2] = bonepose[i*12+11];
		texcoords[i][0] = 0;
		texcoords[i][1] = 0;
		colours[i][0] = (i < basebone)?0:1;
		colours[i][1] = (i < basebone)?0:0;
		colours[i][2] = (i < basebone)?1:0;
		colours[i][3] = 1;

		p = bones[i].parent;
		if (p < 0)
			p = i;
		indexes[numindexes++] = i;
		indexes[numindexes++] = p;
	}

	memset(&bonemesh, 0, sizeof(bonemesh));
	bonemesh.indexes = indexes;
	bonemesh.st_array = texcoords;
	bonemesh.lmst_array[0] = texcoords;
	bonemesh.xyz_array = verts;
	bonemesh.colors4f_array[0] = colours;
	bonemesh.numindexes = numindexes;
	bonemesh.numvertexes = bonecount;
	m = &bonemesh;

//FIXME: We should use the skybox clipping code and split the sphere into 6 sides.
	memset(&b, 0, sizeof(b));
	b.flags = flags;
	b.meshes = 1;
	b.firstmesh = 0;
	b.mesh = &m;
	b.ent = currententity;
	b.shader = shader;
	b.skin = NULL;
	b.texture = NULL;
	b.vbo = NULL;
	BE_SubmitBatch(&b);
}
#endif	//!SERVERONLY
#endif	//SKELETALMODELS

void Alias_FlushCache(void)
{
	meshcache.ent = NULL;
}

void Alias_Shutdown(void)
{
	if (meshcache.norm)
		BZ_Free(meshcache.norm);
	meshcache.norm = NULL;
	meshcache.numnorm = 0;

	if (meshcache.coords)
		BZ_Free(meshcache.coords);
	meshcache.coords = NULL;
	meshcache.numcoords = 0;
}

qboolean Alias_GAliasBuildMesh(mesh_t *mesh, vbo_t **vbop, galiasinfo_t *inf, int surfnum, entity_t *e, qboolean usebones)
{
	galiasanimation_t *g1, *g2;
#ifndef SERVERONLY
	extern cvar_t r_nolerp;
	float lerpcutoff;
#endif

	int frame1;
	int frame2;
	float lerp;
	float fg1time;
//	float fg2time;
#ifdef SKELETALMODELS
	qboolean bytecolours = false;
#endif

	if (!inf->numanimations)
	{
#ifdef SKELETALMODELS
		if (inf->ofs_skel_xyz && !inf->ofs_skel_weight)
		{}
		else
#endif
		{
			Con_DPrintf("Model with no frames (%s)\n", e->model->name);
			return false;
		}
	}

	if (meshcache.numnorm < inf->numverts)
	{
		if (meshcache.norm)
			BZ_Free(meshcache.norm);
		meshcache.norm = BZ_Malloc(sizeof(*meshcache.norm)*inf->numverts*3);
		meshcache.numnorm = inf->numverts;
	}
	if (meshcache.numcoords < inf->numverts)
	{
		if (meshcache.coords)
			BZ_Free(meshcache.coords);
		meshcache.coords = BZ_Malloc(sizeof(*meshcache.coords)*inf->numverts);
		meshcache.numcoords = inf->numverts;
	}

	mesh->numvertexes = inf->numverts;
	mesh->indexes = inf->ofs_indexes;
	mesh->numindexes = inf->numindexes;
	mesh->numbones = 0;

#ifndef SERVERONLY
	mesh->colors4f_array[0] = inf->ofs_rgbaf;
	mesh->colors4b_array = inf->ofs_rgbaub;
#ifdef SKELETALMODELS
	bytecolours = !!inf->ofs_rgbaub;
#endif
	mesh->st_array = inf->ofs_st_array;
#endif
	mesh->trneighbors = inf->ofs_trineighbours;

#ifdef SKELETALMODELS
	if (!inf->numbones)
		usebones = false;
	else if (inf->ofs_skel_xyz && !inf->ofs_skel_weight)
		usebones = false;
	else if (e->fatness || !inf->ofs_skel_idx || inf->numswtransforms || inf->numbones > MAX_GPU_BONES)
#endif
		usebones = false;

	if (meshcache.ent == e)
	{
		if (meshcache.vertgroup == inf->shares_verts && meshcache.ent == e && usebones == meshcache.usebones)
		{
			mesh->xyz_blendw[0] = meshcache.lerp;
			mesh->xyz_blendw[1] = 1-meshcache.lerp;
			mesh->xyz_array = meshcache.acoords1;
			mesh->xyz2_array = meshcache.acoords2;
			mesh->normals_array = meshcache.anorm;
			mesh->snormals_array = meshcache.anorms;
			mesh->tnormals_array = meshcache.anormt;
			if (vbop)
				*vbop = meshcache.vbop;

#ifndef SKELETALMODELS
			return false;
		}
	}
#else
			if (usebones)
			{
				mesh->bonenums = inf->ofs_skel_idx;
				mesh->boneweights = inf->ofs_skel_weight;
				mesh->bones = meshcache.usebonepose;
				mesh->numbones = inf->numbones;
			}
			return false;	//don't generate the new vertex positions. We still have them all.
		}
		if (meshcache.bonegroup != inf->shares_bones)
		{
			meshcache.usebonepose = NULL;
			meshcache.bonecachetype = -1;
		}
	}
	else
	{
		meshcache.usebonepose = NULL;
		meshcache.bonecachetype = -1;
	}
	meshcache.bonegroup = inf->shares_bones;
#endif
	meshcache.vertgroup = inf->shares_verts;
	meshcache.ent = e;


#ifdef _DEBUG
	if (!e->framestate.g[FS_REG].lerpweight[0] && !e->framestate.g[FS_REG].lerpweight[1] && !e->framestate.g[FS_REG].lerpweight[2] && !e->framestate.g[FS_REG].lerpweight[3])
		Con_Printf("Entity with no lerp info\n");
#endif


#ifndef SERVERONLY
	mesh->trneighbors = inf->ofs_trineighbours;
	mesh->normals_array = meshcache.norm;
	mesh->snormals_array = meshcache.norm+meshcache.numnorm;
	mesh->tnormals_array = meshcache.norm+meshcache.numnorm*2;
#endif
	mesh->xyz_array = meshcache.coords;

//we don't support meshes with one pose skeletal and annother not.
//we don't support meshes with one group skeletal and annother not.

#ifdef SKELETALMODELS
	meshcache.vbop = NULL;
	if (vbop)
		*vbop = NULL;
	if (inf->ofs_skel_xyz && !inf->ofs_skel_weight)
	{
		//if we have skeletal xyz info, but no skeletal weights, then its a partial model that cannot possibly be animated.
		meshcache.usebonepose = NULL;
		mesh->xyz_array = inf->ofs_skel_xyz;
		mesh->xyz2_array = NULL;
		mesh->normals_array = inf->ofs_skel_norm;
		mesh->snormals_array = inf->ofs_skel_svect;
		mesh->tnormals_array = inf->ofs_skel_tvect;

		if (vbop)
		{
			meshcache.vbo.indicies = inf->vboindicies;
			meshcache.vbo.indexcount = inf->numindexes;
			meshcache.vbo.vertcount = inf->numverts;
			meshcache.vbo.texcoord = inf->vbotexcoords;
			meshcache.vbo.coord = inf->vbo_skel_verts;
			memset(&meshcache.vbo.coord2, 0, sizeof(meshcache.vbo.coord2));
			meshcache.vbo.normals = inf->vbo_skel_normals;
			meshcache.vbo.svector = inf->vbo_skel_svector;
			meshcache.vbo.tvector = inf->vbo_skel_tvector;
			meshcache.vbo.colours[0] = inf->vborgba;
			meshcache.vbo.colours_bytes = bytecolours;
			memset(&meshcache.vbo.bonenums, 0, sizeof(meshcache.vbo.bonenums));
			memset(&meshcache.vbo.boneweights, 0, sizeof(meshcache.vbo.boneweights));
			meshcache.vbo.numbones = 0;
			meshcache.vbo.bones = NULL;
			if (meshcache.vbo.indicies.sysptr)
				*vbop = meshcache.vbop = &meshcache.vbo;
		}
	}
	else if (inf->numbones)
	{
		mesh->xyz2_array = NULL;	//skeltal animations blend bones, not verticies.

		if (!usebones)
		{
			if (inf->numindexes)
			{
				//software bone animation
				//there are two ways to animate a skeleton
				Alias_BuildSkeletalMesh(mesh, &e->framestate, inf);

#ifdef PEXT_FATNESS
				if (e->fatness)
				{
					int i;
					for (i = 0; i < mesh->numvertexes; i++)
					{
						VectorMA(mesh->xyz_array[i], e->fatness, mesh->normals_array[i], meshcache.coords[i]);
					}
					mesh->xyz_array = meshcache.coords;
				}
#endif
			}
			else
			{
				if (meshcache.bonecachetype != SKEL_ABSOLUTE)
					meshcache.usebonepose = Alias_GetBoneInformation(inf, &e->framestate, meshcache.bonecachetype=SKEL_ABSOLUTE, meshcache.boneposebuffer1, meshcache.boneposebuffer2, MAX_BONES);
#ifndef SERVERONLY
				if (inf->shares_bones != surfnum && qrenderer)
					Alias_DrawSkeletalBones(inf->ofsbones, (float *)meshcache.usebonepose, inf->numbones, e->framestate.g[0].endbone);
#endif
			}
		}
		else
		{
			if (meshcache.bonecachetype != SKEL_INVERSE_ABSOLUTE)
				meshcache.usebonepose = Alias_GetBoneInformation(inf, &e->framestate, meshcache.bonecachetype=SKEL_INVERSE_ABSOLUTE, meshcache.boneposebuffer1, meshcache.boneposebuffer2, MAX_BONES);

			//hardware bone animation
			mesh->xyz_array = inf->ofs_skel_xyz;
			mesh->normals_array = inf->ofs_skel_norm;
			mesh->snormals_array = inf->ofs_skel_svect;
			mesh->tnormals_array = inf->ofs_skel_tvect;

			if (vbop)
			{
				meshcache.vbo.indicies = inf->vboindicies;
				meshcache.vbo.indexcount = inf->numindexes;
				meshcache.vbo.vertcount = inf->numverts;
				meshcache.vbo.texcoord = inf->vbotexcoords;
				meshcache.vbo.coord = inf->vbo_skel_verts;
				memset(&meshcache.vbo.coord2, 0, sizeof(meshcache.vbo.coord2));
				meshcache.vbo.normals = inf->vbo_skel_normals;
				meshcache.vbo.svector = inf->vbo_skel_svector;
				meshcache.vbo.tvector = inf->vbo_skel_tvector;
				meshcache.vbo.colours[0] = inf->vborgba;
				meshcache.vbo.colours_bytes = bytecolours;
				meshcache.vbo.bonenums = inf->vbo_skel_bonenum;
				meshcache.vbo.boneweights = inf->vbo_skel_bweight;
				meshcache.vbo.numbones = inf->numbones;
				meshcache.vbo.bones = meshcache.usebonepose;
				if (meshcache.vbo.indicies.sysptr)
					*vbop = meshcache.vbop = &meshcache.vbo;
			}
		}
	}
	else
#endif
	{
		//FIXME: replace most of this logic with Alias_BuildSkelLerps

		frame1 = e->framestate.g[FS_REG].frame[0];
		frame2 = e->framestate.g[FS_REG].frame[1];
		lerp = e->framestate.g[FS_REG].lerpweight[1];	//FIXME
		fg1time = e->framestate.g[FS_REG].frametime[0];
		//fg2time = e->framestate.g[FS_REG].frametime[1];

		if (frame1 < 0)
		{
			Con_DPrintf("Negative frame (%s)\n", e->model->name);
			frame1 = 0;
		}
		if (frame2 < 0)
		{
			Con_DPrintf("Negative frame (%s)\n", e->model->name);
			frame2 = frame1;
		}
		if (frame1 >= inf->numanimations)
		{
			Con_DPrintf("Too high frame %i (%s)\n", frame1, e->model->name);
			frame1 %= inf->numanimations;
		}
		if (frame2 >= inf->numanimations)
		{
 			Con_DPrintf("Too high frame %i (%s)\n", frame2, e->model->name);
			frame2 %= inf->numanimations;
		}

		if (lerp <= 0)
			frame2 = frame1;
		else  if (lerp >= 1)
			frame1 = frame2;

		g1 = &inf->ofsanimations[frame1];
		g2 = &inf->ofsanimations[frame2];

		if (!inf->numanimations || !g1->numposes || !g2->numposes)
		{
			Con_Printf("Invalid animation data on entity with model %s\n", e->model->name);
			//no animation data. panic!
			memset(mesh, 0, sizeof(*mesh));
			*vbop = NULL;
			return false;
		}

		if (g1 == g2)	//lerping within group is only done if not changing group
		{
			lerp = fg1time*g1->rate;
			if (lerp < 0) lerp = 0;	//hrm
			frame1=lerp;
			frame2=frame1+1;
			lerp-=frame1;
			if (r_noframegrouplerp.ival)
				lerp = 0;
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

#ifndef SERVERONLY
		lerpcutoff = inf->lerpcutoff * r_lerpmuzzlehack.value;
		if (Sh_StencilShadowsActive() || e->fatness || lerpcutoff)
		{
			mesh->xyz2_array = NULL;
			mesh->xyz_blendw[0] = 1;
			mesh->xyz_blendw[1] = 0;
			R_LerpFrames(mesh,	&g1->poseofs[frame1], &g2->poseofs[frame2], 1-lerp, e->fatness, lerpcutoff);
		}
		else
#endif
		{
			galiaspose_t *p1 = &g1->poseofs[frame1];
#ifndef SERVERONLY
			galiaspose_t *p2 = &g2->poseofs[frame2];
#endif

			meshcache.vbo.indicies = inf->vboindicies;
			meshcache.vbo.indexcount = inf->numindexes;
			meshcache.vbo.vertcount = inf->numverts;
			meshcache.vbo.texcoord = inf->vbotexcoords;

#ifdef SKELETALMODELS
			memset(&meshcache.vbo.bonenums, 0, sizeof(meshcache.vbo.bonenums));
			memset(&meshcache.vbo.boneweights, 0, sizeof(meshcache.vbo.boneweights));
			meshcache.vbo.numbones = 0;
			meshcache.vbo.bones = 0;
#endif

#ifdef SERVERONLY
			mesh->xyz_array = p1->ofsverts;
			mesh->xyz2_array = NULL;
#else
			mesh->normals_array = p1->ofsnormals;
			mesh->snormals_array = p1->ofssvector;
			mesh->tnormals_array = p1->ofstvector;

			meshcache.vbo.normals = p1->vbonormals;
			meshcache.vbo.svector = p1->vbosvector;
			meshcache.vbo.tvector = p1->vbotvector;
			memset(&meshcache.vbo.colours[0], 0, sizeof(meshcache.vbo.colours[0]));

			if (p1 == p2 || r_nolerp.ival)
			{
				meshcache.vbo.coord = p1->vboverts;
				memset(&meshcache.vbo.coord2, 0, sizeof(meshcache.vbo.coord2));
				mesh->xyz_array = p1->ofsverts;
				mesh->xyz2_array = NULL;
			}
			else
			{
				meshcache.vbo.coord = p1->vboverts;
				meshcache.vbo.coord2 = p2->vboverts;
				mesh->xyz_blendw[0] = 1-lerp;
				mesh->xyz_blendw[1] = lerp;
				mesh->xyz_array = p1->ofsverts;
				mesh->xyz2_array = p2->ofsverts;
			}
#endif
			if (vbop && meshcache.vbo.indicies.sysptr)
				*vbop = meshcache.vbop = &meshcache.vbo;
		}
	}

	meshcache.vbo.vao = 0;
	meshcache.vbo.vaodynamic = ~0;
	meshcache.vbo.vaoenabled = 0;
	meshcache.acoords1 = mesh->xyz_array;
	meshcache.acoords2 = mesh->xyz2_array;
	meshcache.anorm = mesh->normals_array;
	meshcache.anorms = mesh->snormals_array;
	meshcache.anormt = mesh->tnormals_array;
	meshcache.lerp = mesh->xyz_blendw[0];
	if (vbop)
		meshcache.vbop = *vbop;

#ifdef SKELETALMODELS
	meshcache.usebones = usebones;
	if (usebones)
	{
		mesh->bonenums = inf->ofs_skel_idx;
		mesh->boneweights = inf->ofs_skel_weight;
		mesh->bones = meshcache.usebonepose;
		mesh->numbones = inf->numbones;
	}
#endif

	return true;	//to allow the mesh to be dlighted.
}






static float PlaneNearest(vec3_t normal, vec3_t mins, vec3_t maxs)
{
	float result;
#if 0
	result  = fabs(normal[0] * maxs[0]);
	result += fabs(normal[1] * maxs[1]);
	result += fabs(normal[2] * maxs[2]);
#elif 0
	result  = normal[0] * ((normal[0] > 0)?-16:16);
	result += normal[1] * ((normal[1] > 0)?-16:16);
	result += normal[2] * ((normal[2] > 0)?-24:32);
#else
	result  = normal[0] * ((normal[0] > 0)?mins[0]:maxs[0]);
	result += normal[1] * ((normal[1] > 0)?mins[1]:maxs[1]);
	result += normal[2] * ((normal[2] > 0)?mins[2]:maxs[2]);
#endif
	return result;
}

void CLQ1_DrawLine(shader_t *shader, vec3_t v1, vec3_t v2, float r, float g, float b, float a);
qboolean Mod_Trace_Trisoup(vecV_t *posedata, index_t *indexes, int numindexes, vec3_t start, vec3_t end, vec3_t mins, vec3_t maxs, trace_t *trace)
{
	qboolean impacted = false;
	int i, j;

	float *p1, *p2, *p3;
	vec3_t edge1, edge2, edge3;
	vec3_t normal;
	vec3_t edgenormal;

	float planedist;
	float diststart, distend;
	float mn,mx;
	float extend;

	float frac;

	vec3_t impactpoint;
//	shader_t *lineshader = NULL;

	for (i = 0; i < numindexes; i+=3)
	{
		p1 = posedata[indexes[i+0]];
		p2 = posedata[indexes[i+1]];
		p3 = posedata[indexes[i+2]];
/*
		VectorAdd(p1, r_refdef.pvsorigin, edge1);
		VectorAdd(p2, r_refdef.pvsorigin, edge2);
		VectorAdd(p3, r_refdef.pvsorigin, edge3);
		CLQ1_DrawLine(lineshader, edge1, edge2, 0, 0, 1, 1);
		CLQ1_DrawLine(lineshader, edge2, edge3, 0, 0, 1, 1);
		CLQ1_DrawLine(lineshader, edge3, edge1, 0, 0, 1, 1);
*/
		VectorSubtract(p1, p2, edge1);
		VectorSubtract(p3, p2, edge2);
		CrossProduct(edge1, edge2, normal);
		VectorNormalize(normal);

		//degenerate triangle
		if (!normal[0] && !normal[1] && !normal[2])
			continue;

		//debugging
//		if (normal[2] != 1)
//			continue;

#define	DIST_EPSILON	(0.03125)
#define DIST_SOLID		(3/8.0)	//the plane must be at least this thick, or player prediction will try jittering through it to correct the player's origin
		extend = PlaneNearest(normal, mins, maxs);
		planedist = DotProduct(p1, normal)-extend;
		diststart = DotProduct(start, normal);
		if (diststart/*+extend+DIST_SOLID*/ < planedist)
			continue;	//start on back side (or slightly inside).
		distend = DotProduct(end, normal);
		if (distend > planedist)
			continue;	//end on front side.

		//figure out the precise frac
		if (diststart > planedist)
		{
			//if we're not stuck inside it
			if (distend >= diststart)
				continue;	//trace moves away from or along the surface. don't block the trace if we're sliding along the front of it.
		}
		frac = (diststart - planedist) / (diststart-distend);
		if (frac >= trace->truefraction)	//already found one closer.
			continue;

		//an impact outside of the surface's bounding box (expanded by the trace bbox) is not a valid impact.
		//this solves extrusion issues.
		for (j = 0; j < 3; j++)
		{
			impactpoint[j] = start[j] + frac*(end[j] - start[j]);
			//make sure the impact point is within the triangle's bbox.
			//primarily, this serves to prevent the edge extruding off to infinity or so
			mx = mn = p1[j];
			if (mn > p2[j])
				mn = p2[j];
			if (mx < p2[j])
				mx = p2[j];
			if (mn > p3[j])
				mn = p3[j];
			if (mx < p3[j])
				mx = p3[j];
			mx-=mins[j]-DIST_EPSILON;
			mn-=maxs[j]+DIST_EPSILON;
			if (impactpoint[j] > mx)
				break;
			if (impactpoint[j] < mn)
				break;
		}
		if (j < 3)
			continue;


		//make sure the impact point is actually within the triangle
		CrossProduct(edge1, normal, edgenormal);
		VectorNormalize(edgenormal);
		if (DotProduct(impactpoint, edgenormal) > DotProduct(p2, edgenormal)-PlaneNearest(edgenormal, mins, maxs)+DIST_EPSILON)
			continue;

		CrossProduct(normal, edge2, edgenormal);
		VectorNormalize(edgenormal);
		if (DotProduct(impactpoint, edgenormal) > DotProduct(p3, edgenormal)-PlaneNearest(edgenormal, mins, maxs)+DIST_EPSILON)
			continue;

		VectorSubtract(p1, p3, edge3);
		CrossProduct(normal, edge3, edgenormal);
		VectorNormalize(edgenormal);
		if (DotProduct(impactpoint, edgenormal) > DotProduct(p1, edgenormal)-PlaneNearest(edgenormal, mins, maxs)+DIST_EPSILON)
			continue;

		//okay, its a valid impact
		trace->truefraction = frac;

		//move back from the impact point. this should keep the point slightly outside of the solid triangle.
		frac = (diststart - (planedist+DIST_EPSILON)) / (diststart-distend);
		if (frac < 0)
		{	//we're inside, apparently
			trace->startsolid = trace->allsolid = (diststart < planedist);
			trace->fraction = 0;
			VectorCopy(start, trace->endpos);
		}
		else
		{
			//we made progress
			trace->fraction = frac;
			trace->endpos[0] = start[0] + frac*(end[0] - start[0]);
			trace->endpos[1] = start[1] + frac*(end[1] - start[1]);
			trace->endpos[2] = start[2] + frac*(end[2] - start[2]);
		}
		VectorCopy(normal, trace->plane.normal);
		trace->plane.dist = planedist;
		trace->triangle_id = 1+i/3;
		impacted = true;

//		if (fabs(normal[0]) != 1 && fabs(normal[1]) != 1 && fabs(normal[2]) != 1)
//			Con_Printf("Non-axial impact\n");

/*		if (!lineshader)
			lineshader = R_RegisterShader("lineshader", SUF_NONE,
					"{\n"
					"polygonoffset\n"
					"{\n"
					"map $whiteimage\n"
					"blendfunc add\n"
					"rgbgen vertex\n"
					"alphagen vertex\n"
					"}\n"
					"}\n");
		VectorAdd(p1, r_refdef.pvsorigin, edge1);
		VectorAdd(p2, r_refdef.pvsorigin, edge2);
		VectorAdd(p3, r_refdef.pvsorigin, edge3);
		CLQ1_DrawLine(lineshader, edge1, edge2, 0, 1, 0, 1);
		CLQ1_DrawLine(lineshader, edge2, edge3, 0, 1, 0, 1);
		CLQ1_DrawLine(lineshader, edge3, edge1, 0, 1, 0, 1);
*/
	}
	return impacted;
}

//The whole reason why model loading is supported in the server.
qboolean Mod_Trace(model_t *model, int forcehullnum, int frame, vec3_t axis[3], vec3_t start, vec3_t end, vec3_t mins, vec3_t maxs, qboolean capsule, unsigned int contentsmask, trace_t *trace)
{
	galiasinfo_t *mod = Mod_Extradata(model);
	galiasanimation_t *group;
	galiaspose_t *pose;

//	float temp;

	vecV_t *posedata;
	index_t *indexes;
	int surfnum = 0;
#ifdef SKELETALMODELS
	int cursurfnum = -1;
#endif

	vec3_t start_l, end_l;

	if (axis)
	{
		start_l[0] = DotProduct(start, axis[0]);
		start_l[1] = DotProduct(start, axis[1]);
		start_l[2] = DotProduct(start, axis[2]);
		end_l[0] = DotProduct(end, axis[0]);
		end_l[1] = DotProduct(end, axis[1]);
		end_l[2] = DotProduct(end, axis[2]);
	}
	else
	{
		VectorCopy(start, start_l);
		VectorCopy(end, end_l);
	}

	trace->fraction = trace->truefraction = 1;

	for(; mod; mod = mod->nextsurf, surfnum++)
	{
		indexes = mod->ofs_indexes;
		if (!mod->numanimations)
		{
#ifdef SKELETALMODELS
			//certain models have no possibility of animation.
			//fixme: skeletal objects...
			if (mod->ofs_skel_xyz)
				posedata = mod->ofs_skel_xyz;
			else
#endif
				continue;
		}
		else
		{
			group = mod->ofsanimations;
			group += frame % mod->numanimations;
			//FIXME: no support for frame blending.
			if (!group->numposes)
				continue;
			pose = group->poseofs;
			pose += 0%group->numposes;				//FIXME: no framegroup support
			posedata = pose->ofsverts;
#ifdef SKELETALMODELS
			if (mod->numbones && mod->shares_verts != cursurfnum)
			{
				posedata = alloca(mod->numverts*sizeof(vecV_t));
				Alias_BuildSkeletalVPositionsPose((float*)posedata, group->skeltype, group->boneofs, mod);

				cursurfnum = mod->shares_verts;
			}
#endif
		}

		trace->truefraction = 1;
		if (Mod_Trace_Trisoup(posedata, indexes, mod->numindexes, start_l, end_l, mins, maxs, trace))
		{
			trace->surface_id = 1+surfnum;
			trace->bone_id = 0;
#ifdef SKELETALMODELS
			if (mod->ofs_skel_weight)
			{	//fixme: would be better to consider the distance to the vertex too. cartesian coord stuff etc.
				unsigned int v, w, i;
				float bw = 0;
				for (i = 0; i < 3; i++)
				{
					for (v = indexes[(trace->triangle_id-1)*3+i], w = 0; w < 4; w++)
					{
						if (bw < mod->ofs_skel_weight[v][w])
						{
							bw = mod->ofs_skel_weight[v][w];
							trace->bone_id = 1 + mod->ofs_skel_idx[v][w];
						}
					}
				}
			}
#endif
			if (axis)
			{
				vec3_t iaxis[3];
				vec3_t norm;
				Matrix3x3_RM_Invert_Simple((void *)axis, iaxis);
				VectorCopy(trace->plane.normal, norm);
				trace->plane.normal[0] = DotProduct(norm, iaxis[0]);
				trace->plane.normal[1] = DotProduct(norm, iaxis[1]);
				trace->plane.normal[2] = DotProduct(norm, iaxis[2]);

//				frac = traceinfo.truefraction;
				/*
				diststart = DotProduct(traceinfo.start, trace->plane.normal);
				distend = DotProduct(traceinfo.end, trace->plane.normal);
				if (diststart == distend)
					frac = 0;
				else
				{
					frac = (diststart - trace->plane.dist) / (diststart-distend);
					if (frac < 0)
						frac = 0;
					else if (frac > 1)
						frac = 1;
				}*/

				/*okay, this is where it hits this plane*/
				trace->endpos[0] = start[0] + trace->fraction*(end[0] - start[0]);
				trace->endpos[1] = start[1] + trace->fraction*(end[1] - start[1]);
				trace->endpos[2] = start[2] + trace->fraction*(end[2] - start[2]);
			}
		}
	}

	trace->allsolid = false;

	return trace->fraction != 1;
}


static void Mod_ClampModelSize(model_t *mod)
{
#ifndef SERVERONLY
	int i;

	float rad=0, axis;
	axis = (mod->maxs[0] - mod->mins[0]);
	rad += axis*axis;
	axis = (mod->maxs[1] - mod->mins[1]);
	rad += axis*axis;
	axis = (mod->maxs[2] - mod->mins[2]);
	rad += axis*axis;

	mod->tainted = false;
	if (mod->engineflags & MDLF_DOCRC)
	{
		if (!strcmp(mod->name, "progs/eyes.mdl"))
		{       //this is checked elsewhere to make sure the crc matches (this is to make sure the crc check was actually called)
			if (mod->type != mod_alias || mod->fromgame != fg_quake || mod->flags)
				mod->tainted = true;
		}
	}

	mod->clampscale = 1;
	for (i = 0; i < sizeof(clampedmodel)/sizeof(clampedmodel[0]); i++)
	{
		if (!strcmp(mod->name, clampedmodel[i].name))
		{
			if (rad > clampedmodel[i].furthestallowedextremety)
			{
				axis = clampedmodel[i].furthestallowedextremety;
				mod->clampscale = axis/rad;
				Con_DPrintf("\"%s\" will be clamped.\n", mod->name);
			}
			return;
		}
	}

//	Con_DPrintf("Don't know what size to clamp \"%s\" to (size:%f).\n", mod->name, rad);
#endif
}

#ifndef SERVERONLY
static int R_FindTriangleWithEdge (index_t *indexes, int numtris, int start, int end, int ignore)
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

void Mod_CompileTriangleNeighbours(model_t *loadmodel, galiasinfo_t *galias)
{
#ifndef SERVERONLY
	if (Sh_StencilShadowsActive())
	{
		int i, *n;
		index_t *index;
		index_t *indexes = galias->ofs_indexes;
		int numtris = galias->numindexes/3;
		int *neighbours;
		neighbours = ZG_Malloc(&loadmodel->memgroup, sizeof(int)*numtris*3);
		galias->ofs_trineighbours = neighbours;

		for (i = 0, index = indexes, n = neighbours; i < numtris; i++, index += 3, n += 3)
		{
			n[0] = R_FindTriangleWithEdge (indexes, numtris, index[1], index[0], i);
			n[1] = R_FindTriangleWithEdge (indexes, numtris, index[2], index[1], i);
			n[2] = R_FindTriangleWithEdge (indexes, numtris, index[0], index[2], i);
		}
	}
#endif
}

typedef struct
{
	unsigned int firstpose;
	unsigned int posecount;
	float fps;
	qboolean loop;
	char name[MAX_QPATH];
} frameinfo_t;
static frameinfo_t *ParseFrameInfo(char *modelname, int *numgroups)
{
	int count = 0;
	int maxcount = 0;
	char *line, *eol;
	char *file;
	frameinfo_t *frames = NULL;
	char fname[MAX_QPATH];
	char tok[64];
	size_t fsize;
	Q_snprintfz(fname, sizeof(fname), "%s.framegroups", modelname);
	line = file = COM_LoadFile(fname, 5, &fsize);
	if (!file)
		return NULL;
	while(line && *line)
	{
		eol = strchr(line, '\n');
		if (eol)
			*eol = 0;
			
		if (count == maxcount)
		{
			maxcount += 32;
			frames = realloc(frames, sizeof(*frames)*maxcount);
		}

		line = COM_ParseOut(line, tok, sizeof(tok));
		frames[count].firstpose = atoi(tok);
		line = COM_ParseOut(line, tok, sizeof(tok));
		frames[count].posecount = atoi(tok);
		line = COM_ParseOut(line, tok, sizeof(tok));
		frames[count].fps = atof(tok);
		line = COM_ParseOut(line, tok, sizeof(tok));
		if (!strcmp(tok, "true") || !strcmp(tok, "yes") || !strcmp(tok, "on"))
			frames[count].loop = true;
		else
			frames[count].loop = !!atoi(tok);
		line = COM_ParseOut(line, frames[count].name, sizeof(frames[count].name));
		if (frames[count].posecount>0 && frames[count].fps)
			count++;

		if (eol)
			line = eol+1;
		else
			break;
	}
	BZ_Free(file);

	*numgroups = count;
	return frames;
}

void Mod_DestroyMesh(galiasinfo_t *galias)
{
#ifndef SERVERONLY
	if (!qrenderer || !BE_VBO_Destroy)
		return;
	while(galias)
	{
		BE_VBO_Destroy(&galias->vbotexcoords, galias->vbomem);
		BE_VBO_Destroy(&galias->vboindicies, galias->ebomem);
		galias = galias->nextsurf;
	}
#endif
}

void Mod_GenerateMeshVBO(galiasinfo_t *galias)
//vec3_t *vc, vec2_t *tc, vec3_t *nv, vec3_t *sv, vec3_t *tv, index_t *idx, int numidx, int numverts)
{
#ifndef SERVERONLY
	int i, p;
	galiasanimation_t *group;
	galiaspose_t *pose;
	int vbospace = 0;
	vbobctx_t vboctx;

	//don't fail on dedicated servers
	if (!BE_VBO_Begin || !galias->numverts)
		return;

	group = galias->ofsanimations;

	//determine the amount of space we need for our vbos.
	if (galias->ofs_st_array)
		vbospace += sizeof(*galias->ofs_st_array) * galias->numverts;
	if (galias->ofs_rgbaf)
		vbospace += sizeof(*galias->ofs_rgbaf) * galias->numverts;
	else if (galias->ofs_rgbaub)
		vbospace += sizeof(*galias->ofs_rgbaub) * galias->numverts;
#ifdef SKELETALMODELS
	if (galias->ofs_skel_xyz)
		vbospace += sizeof(*galias->ofs_skel_xyz) * galias->numverts;
	if (galias->ofs_skel_norm)
		vbospace += sizeof(*galias->ofs_skel_norm) * galias->numverts;
	if (galias->ofs_skel_svect)
		vbospace += sizeof(*galias->ofs_skel_svect) * galias->numverts;
	if (galias->ofs_skel_tvect)
		vbospace += sizeof(*galias->ofs_skel_tvect) * galias->numverts;
	if (galias->ofs_skel_idx)
		vbospace += sizeof(*galias->ofs_skel_idx) * galias->numverts;
	if (galias->ofs_skel_weight)
		vbospace += sizeof(*galias->ofs_skel_weight) * galias->numverts;
#endif
	for (i = 0; i < galias->numanimations; i++)
	{
		if (group->poseofs)
			vbospace += group[i].numposes * galias->numverts * (sizeof(vecV_t)+sizeof(vec3_t)*3);
	}
	BE_VBO_Begin(&vboctx, vbospace);
	if (galias->ofs_st_array)
		BE_VBO_Data(&vboctx, galias->ofs_st_array, sizeof(*galias->ofs_st_array) * galias->numverts, &galias->vbotexcoords);
	if (galias->ofs_rgbaf)
		BE_VBO_Data(&vboctx, galias->ofs_rgbaf, sizeof(*galias->ofs_rgbaf) * galias->numverts, &galias->vborgba);
	else if (galias->ofs_rgbaub)
		BE_VBO_Data(&vboctx, galias->ofs_rgbaub, sizeof(*galias->ofs_rgbaub) * galias->numverts, &galias->vborgba);
#ifdef SKELETALMODELS
	if (galias->ofs_skel_xyz)
		BE_VBO_Data(&vboctx, galias->ofs_skel_xyz, sizeof(*galias->ofs_skel_xyz) * galias->numverts, &galias->vbo_skel_verts);
	if (galias->ofs_skel_norm)
		BE_VBO_Data(&vboctx, galias->ofs_skel_norm, sizeof(*galias->ofs_skel_norm) * galias->numverts, &galias->vbo_skel_normals);
	if (galias->ofs_skel_svect)
		BE_VBO_Data(&vboctx, galias->ofs_skel_svect, sizeof(*galias->ofs_skel_svect) * galias->numverts, &galias->vbo_skel_svector);
	if (galias->ofs_skel_tvect)
		BE_VBO_Data(&vboctx, galias->ofs_skel_tvect, sizeof(*galias->ofs_skel_tvect) * galias->numverts, &galias->vbo_skel_tvector);
	if (galias->ofs_skel_idx)
		BE_VBO_Data(&vboctx, galias->ofs_skel_idx, sizeof(*galias->ofs_skel_idx) * galias->numverts, &galias->vbo_skel_bonenum);
	if (galias->ofs_skel_weight)
		BE_VBO_Data(&vboctx, galias->ofs_skel_weight, sizeof(*galias->ofs_skel_weight) * galias->numverts, &galias->vbo_skel_bweight);
#endif

	for (i = 0; i < galias->numanimations; i++, group++)
	{
		pose = group->poseofs;
		if (pose)
		for (p = 0; p < group->numposes; p++, pose++)
		{
			BE_VBO_Data(&vboctx, pose->ofsverts, sizeof(*pose->ofsverts) * galias->numverts, &pose->vboverts);
			BE_VBO_Data(&vboctx, pose->ofsnormals, sizeof(*pose->ofsnormals) * galias->numverts, &pose->vbonormals);
			if (pose->ofssvector)
				BE_VBO_Data(&vboctx, pose->ofssvector, sizeof(*pose->ofssvector) * galias->numverts, &pose->vbosvector);
			if (pose->ofstvector)
				BE_VBO_Data(&vboctx, pose->ofstvector, sizeof(*pose->ofstvector) * galias->numverts, &pose->vbotvector);
		}
	}
	BE_VBO_Finish(&vboctx, galias->ofs_indexes, sizeof(*galias->ofs_indexes) * galias->numindexes, &galias->vboindicies, &galias->vbomem, &galias->ebomem);
#endif
}


//called for non-skeletal model formats.
void Mod_BuildTextureVectors(galiasinfo_t *galias)
//vec3_t *vc, vec2_t *tc, vec3_t *nv, vec3_t *sv, vec3_t *tv, index_t *idx, int numidx, int numverts)
{
#ifndef SERVERONLY
	int i, p;
	galiasanimation_t *group;
	galiaspose_t *pose;
	vecV_t *vc;
	vec3_t *nv, *sv, *tv;
	vec2_t *tc;
	index_t *idx;

	//don't fail on dedicated servers
	if (!qrenderer || !BE_VBO_Begin)
		return;

	idx = galias->ofs_indexes;
	tc = galias->ofs_st_array;
	group = galias->ofsanimations;

	for (i = 0; i < galias->numanimations; i++, group++)
	{
		pose = group->poseofs;
		for (p = 0; p < group->numposes; p++, pose++)
		{
			vc = pose->ofsverts;
			nv = pose->ofsnormals;
			if (pose->ofssvector != 0 && pose->ofstvector != 0)
			{
				sv = pose->ofssvector;
				tv = pose->ofstvector;

				Mod_AccumulateTextureVectors(vc, tc, nv, sv, tv, idx, galias->numindexes);
				Mod_NormaliseTextureVectors(nv, sv, tv, galias->numverts);
			}
		}
	}
#endif
}

#ifndef SERVERONLY
/*
=================
Mod_FloodFillSkin

Fill background pixels so mipmapping doesn't have haloes - Ed
=================
*/

typedef struct
{
	short		x, y;
} floodfill_t;

// must be a power of 2
#define FLOODFILL_FIFO_SIZE 0x1000
#define FLOODFILL_FIFO_MASK (FLOODFILL_FIFO_SIZE - 1)

#define FLOODFILL_STEP( off, dx, dy ) \
{ \
	if (pos[off] == fillcolor) \
	{ \
		pos[off] = 255; \
		fifo[inpt].x = x + (dx), fifo[inpt].y = y + (dy); \
		inpt = (inpt + 1) & FLOODFILL_FIFO_MASK; \
	} \
	else if (pos[off] != 255) fdc = pos[off]; \
}

static void Mod_FloodFillSkin( qbyte *skin, int skinwidth, int skinheight )
{
	qbyte				fillcolor = *skin; // assume this is the pixel to fill
	floodfill_t			fifo[FLOODFILL_FIFO_SIZE];
	int					inpt = 0, outpt = 0;
	int					filledcolor = -1;
	int					i;

	if (filledcolor == -1)
	{
		filledcolor = 0;
		// attempt to find opaque black
		for (i = 0; i < 256; ++i)
			if (d_8to24rgbtable[i] == (255 << 0)) // alpha 1.0
			{
				filledcolor = i;
				break;
			}
	}

	// can't fill to filled color or to transparent color (used as visited marker)
	if ((fillcolor == filledcolor) || (fillcolor == 255))
	{
		//printf( "not filling skin from %d to %d\n", fillcolor, filledcolor );
		return;
	}

	fifo[inpt].x = 0, fifo[inpt].y = 0;
	inpt = (inpt + 1) & FLOODFILL_FIFO_MASK;

	while (outpt != inpt)
	{
		int			x = fifo[outpt].x, y = fifo[outpt].y;
		int			fdc = filledcolor;
		qbyte		*pos = &skin[x + skinwidth * y];

		outpt = (outpt + 1) & FLOODFILL_FIFO_MASK;

		if (x > 0)				FLOODFILL_STEP( -1, -1, 0 );
		if (x < skinwidth - 1)	FLOODFILL_STEP( 1, 1, 0 );
		if (y > 0)				FLOODFILL_STEP( -skinwidth, 0, -1 );
		if (y < skinheight - 1)	FLOODFILL_STEP( skinwidth, 0, 1 );
		skin[x + skinwidth * y] = fdc;
	}
}

//looks for foo.md3_0.skin files, for dp compat
//also try foo_0.skin, because people appear to use that too. *sigh*.
int Mod_CountSkinFiles(char *modelname)
{
	int i;
	char skinfilename[MAX_QPATH];
	//try and add numbered skins, and then try fixed names.
	for (i = 0; ; i++)
	{
		Q_snprintfz(skinfilename, sizeof(skinfilename), "%s_%i.skin", modelname, i);
		if (!COM_FCheckExists(skinfilename))
		{
			COM_StripExtension(modelname, skinfilename, sizeof(skinfilename));
			Q_snprintfz(skinfilename+strlen(skinfilename), sizeof(skinfilename)-strlen(skinfilename), "_%i.skin", i);
			if (!COM_FCheckExists(skinfilename))
				break;
		}
	}
	return i;
}

//support for foo.md3_0.skin
shader_t *Mod_ShaderFromQ3SkinFile(galiasinfo_t *surf, char *modelname, int skinnum)
{
	shader_t *result = NULL;
	skinid_t skinid;
	skinfile_t *skinfile;
	int i;
	char *filedata;
	char skinfilename[MAX_QPATH];

	if (qrenderer == QR_NONE)
		return NULL;

	Q_snprintfz(skinfilename, sizeof(skinfilename), "%s_%i.skin", modelname, skinnum);
	filedata = FS_LoadMallocFile(skinfilename, NULL);
	if (!filedata)
	{
		COM_StripExtension(modelname, skinfilename, sizeof(skinfilename));
		Q_snprintfz(skinfilename+strlen(skinfilename), sizeof(skinfilename)-strlen(skinfilename), "_%i.skin", skinnum);
		filedata = FS_LoadMallocFile(skinfilename, NULL);
	}
	if (filedata)
	{
		skinid = Mod_ReadSkinFile(skinfilename, filedata);
		Z_Free(filedata);

		skinfile = Mod_LookupSkin(skinid);
		if (skinfile)
		{
			//check if this skinfile has a mapping.
			for (i = 0; i < skinfile->nummappings; i++)
			{
				if (!strcmp(surf->surfacename, skinfile->mappings[i].surface))
				{
					skinfile->mappings[i].shader->uses++;	//so it doesn't blow up when the skin gets freed.
					result = skinfile->mappings[i].shader;
					break;
				}
			}
			Mod_WipeSkin(skinid);
		}
	}

	return result;
}

void Mod_LoadAliasShaders(model_t *mod)
{
	qbyte *mipdata[4];
	galiasinfo_t *ai = mod->meshinfo;
	galiasskin_t *s;
	skinframe_t *f;
	int i, j;

	unsigned int loadflags;
	unsigned int imageflags;
	char basename[32];
	char alttexpath[MAX_QPATH];
	uploadfmt_t skintranstype;
	char *slash;
#ifdef HEXEN2
	if( mod->flags & MFH2_TRANSPARENT )
		skintranstype = TF_H2_T7G1;	//hexen2
	else
#endif
	 if(mod->flags & MFH2_HOLEY)
		skintranstype = TF_H2_TRANS8_0;	//hexen2
#ifdef HEXEN2
	else if(mod->flags & MFH2_SPECIAL_TRANS)
		skintranstype = TF_H2_T4A4;	//hexen2
#endif
	else
		skintranstype = TF_SOLID8;

	COM_FileBase(mod->name, basename, sizeof(basename));

	imageflags = 0;
	if (mod->engineflags & MDLF_NOTREPLACEMENTS)
	{
		ruleset_allow_sensitive_texture_replacements.flags |= CVAR_RENDERERLATCH;
		if (!ruleset_allow_sensitive_texture_replacements.ival)
			imageflags |= IF_NOREPLACE;
	}

	slash = COM_SkipPath(mod->name);
	if (slash != mod->name && slash-mod->name < sizeof(alttexpath))
	{
		slash--;
		memcpy(alttexpath, mod->name, slash-mod->name);
		Q_strncpyz(alttexpath+(slash-mod->name), ":models", sizeof(alttexpath)-(slash-mod->name));	//fuhquake compat
		slash++;
	}
	else
	{
		slash = mod->name;
		strcpy(alttexpath, "models");	//fuhquake compat
	}



	for (ai = mod->meshinfo; ai; ai = ai->nextsurf)
	{
		Mod_GenerateMeshVBO(ai);	//FIXME: shares verts
		for (i = 0, s = ai->ofsskins; i < ai->numskins; i++, s++)
		{
			for (j = 0, f = s->frame; j < s->numframes; j++, f++)
			{
				if (j == 0)
					f->shader = Mod_ShaderFromQ3SkinFile(ai, mod->name, i);
				else
					f->shader = NULL;
				if (!f->shader)
				{
					if (!f->defaultshader)
						f->shader = R_RegisterSkin(f->shadername, mod->name);
					else
						f->shader = R_RegisterShader(f->shadername, SUF_NONE, f->defaultshader);
				}

				if (f->texels)
				{
					loadflags = SHADER_HASPALETTED | SHADER_HASDIFFUSE | SHADER_HASGLOSS | SHADER_HASTOPBOTTOM;
					if (r_fb_models.ival)
						loadflags |= SHADER_HASFULLBRIGHT;
					if (r_loadbumpmapping)
						loadflags |= SHADER_HASNORMALMAP;
					mipdata[0] = f->texels;
					R_BuildLegacyTexnums(f->shader, basename, alttexpath, loadflags, imageflags, skintranstype, s->skinwidth, s->skinheight, mipdata, host_basepal);
				}
				else
					R_BuildDefaultTexnums(&f->texnums, f->shader);
			}
		}
	}
}
#endif












//Q1 model loading
#if 1
#define NUMVERTEXNORMALS	162
extern float	r_avertexnormals[NUMVERTEXNORMALS][3];
// mdltype 0 = q1, 1 = qtest, 2 = rapo/h2

static void Q1MDL_LoadPose(galiasinfo_t *galias, dmdl_t *pq1inmodel, vecV_t *verts, vec3_t *normals, vec3_t *svec, vec3_t *tvec, dtrivertx_t *pinframe, int *seamremaps, int mdltype, unsigned int bbox[6])
{
	int j;
#ifdef _DEBUG
	bbox[0] = bbox[1] = bbox[2] = ~0;
	bbox[3] = bbox[4] = bbox[5] = 0;
#endif

#ifdef HEXEN2
	if (mdltype == 2)
	{
		for (j = 0; j < galias->numverts; j++)
		{
			verts[j][0] = pinframe[seamremaps[j]].v[0]*pq1inmodel->scale[0]+pq1inmodel->scale_origin[0];
			verts[j][1] = pinframe[seamremaps[j]].v[1]*pq1inmodel->scale[1]+pq1inmodel->scale_origin[1];
			verts[j][2] = pinframe[seamremaps[j]].v[2]*pq1inmodel->scale[2]+pq1inmodel->scale_origin[2];
#ifndef SERVERONLY
			VectorCopy(r_avertexnormals[pinframe[seamremaps[j]].lightnormalindex], normals[j]);
#endif
		}
	}
	else
#endif
	{
		for (j = 0; j < pq1inmodel->numverts; j++)
		{
#ifdef _DEBUG
			bbox[0] = min(bbox[0], pinframe[j].v[0]);
			bbox[1] = min(bbox[1], pinframe[j].v[1]);
			bbox[2] = min(bbox[2], pinframe[j].v[2]);
			bbox[3] = max(bbox[3], pinframe[j].v[0]);
			bbox[4] = max(bbox[4], pinframe[j].v[1]);
			bbox[5] = max(bbox[5], pinframe[j].v[2]);
#endif
			verts[j][0] = pinframe[j].v[0]*pq1inmodel->scale[0]+pq1inmodel->scale_origin[0];
			verts[j][1] = pinframe[j].v[1]*pq1inmodel->scale[1]+pq1inmodel->scale_origin[1];
			verts[j][2] = pinframe[j].v[2]*pq1inmodel->scale[2]+pq1inmodel->scale_origin[2];
#ifndef SERVERONLY
			VectorCopy(r_avertexnormals[pinframe[j].lightnormalindex], normals[j]);
#endif
			if (seamremaps[j] != j)
			{
				VectorCopy(verts[j], verts[seamremaps[j]]);
#ifndef SERVERONLY
				VectorCopy(normals[j], normals[seamremaps[j]]);
#endif
			}
		}
	}
}
static void Q1MDL_LoadPose16(galiasinfo_t *galias, dmdl_t *pq1inmodel, vecV_t *verts, vec3_t *normals, vec3_t *svec, vec3_t *tvec, dtrivertx_t *pinframe, int *seamremaps, int mdltype)
{
	//quakeforge's MD16 format has regular 8bit stuff, trailed by an extra low-order set of the verts providing the extra 8bits of precision.
	//its worth noting that the model could be rendered using the high-order parts only, if your software renderer only supports that or whatever.
	dtrivertx_t *pinframelow =  pinframe + pq1inmodel->numverts;
	int j;
	vec3_t exscale;
	VectorScale(pq1inmodel->scale, 1.0/256, exscale);
	for (j = 0; j < pq1inmodel->numverts; j++)
	{
		verts[j][0] = pinframe[j].v[0]*pq1inmodel->scale[0] + pinframelow[j].v[0]*exscale[0] + pq1inmodel->scale_origin[0];
		verts[j][1] = pinframe[j].v[1]*pq1inmodel->scale[1] + pinframelow[j].v[1]*exscale[1] + pq1inmodel->scale_origin[1];
		verts[j][2] = pinframe[j].v[2]*pq1inmodel->scale[2] + pinframelow[j].v[2]*exscale[2] + pq1inmodel->scale_origin[2];
#ifndef SERVERONLY
		VectorCopy(r_avertexnormals[pinframe[j].lightnormalindex], normals[j]);
#endif
		if (seamremaps[j] != j)
		{
			VectorCopy(verts[j], verts[seamremaps[j]]);
#ifndef SERVERONLY
			VectorCopy(normals[j], normals[seamremaps[j]]);
#endif
		}
	}
}
static void *Q1MDL_LoadFrameGroup (galiasinfo_t *galias, dmdl_t *pq1inmodel, model_t *loadmodel, daliasframetype_t *pframetype, int *seamremaps, int mdltype)
{
	galiaspose_t *pose;
	galiasanimation_t *frame = galias->ofsanimations;
	dtrivertx_t		*pinframe;
	daliasframe_t *frameinfo;
	int				i, k;
	daliasgroup_t *ingroup;
	daliasinterval_t *intervals;
	float sinter;

	vec3_t *normals, *svec, *tvec;
	vecV_t *verts;
	int aliasframesize = (mdltype == 1) ? sizeof(daliasframe_t)-16 : sizeof(daliasframe_t);

	unsigned int bbox[6];

#ifdef SERVERONLY
	normals = NULL;
	svec = NULL;
	tvec = NULL;
#endif

	for (i = 0; i < pq1inmodel->numframes; i++)
	{
		switch(LittleLong(pframetype->type))
		{
		case ALIAS_SINGLE:
			frameinfo = (daliasframe_t*)((char *)(pframetype+1)); // qtest aliasframe is a subset
			pinframe = (dtrivertx_t*)((char*)frameinfo+aliasframesize);
#ifndef SERVERONLY
			pose = (galiaspose_t *)ZG_Malloc(&loadmodel->memgroup, sizeof(galiaspose_t) + (sizeof(vecV_t)+sizeof(vec3_t)*3)*galias->numverts);
#else
			pose = (galiaspose_t *)ZG_Malloc(&loadmodel->memgroup, sizeof(galiaspose_t) + (sizeof(vecV_t))*galias->numverts);
#endif
			frame->poseofs = pose;
			frame->numposes = 1;
			galias->numanimations++;

			if (mdltype == 1)
				frame->name[0] = '\0';
			else
				Q_strncpyz(frame->name, frameinfo->name, sizeof(frame->name));

			verts = (vecV_t *)(pose+1);
			pose->ofsverts = verts;
#ifndef SERVERONLY
			normals = (vec3_t*)&verts[galias->numverts];
			svec = &normals[galias->numverts];
			tvec = &svec[galias->numverts];
			pose->ofsnormals = normals;
			pose->ofssvector = svec;
			pose->ofstvector = tvec;
#endif

			if (mdltype & 16)
			{
				Q1MDL_LoadPose16(galias, pq1inmodel, verts, normals, svec, tvec, pinframe, seamremaps, mdltype);
				pframetype = (daliasframetype_t *)&pinframe[pq1inmodel->numverts*2];
			}
			else
			{
				Q1MDL_LoadPose(galias, pq1inmodel, verts, normals, svec, tvec, pinframe, seamremaps, mdltype, bbox);
				pframetype = (daliasframetype_t *)&pinframe[pq1inmodel->numverts];
			}

#ifdef _DEBUG
			if ((bbox[3] > frameinfo->bboxmax.v[0] || bbox[4] > frameinfo->bboxmax.v[1] || bbox[5] > frameinfo->bboxmax.v[2] ||
				bbox[0] < frameinfo->bboxmin.v[0] || bbox[1] < frameinfo->bboxmin.v[1] || bbox[2] < frameinfo->bboxmin.v[2]) && !galias->warned)
#else
			if (galias->numverts && pinframe[0].v[2] > frameinfo->bboxmax.v[2] && !galias->warned)
#endif
			{
				Con_DPrintf(CON_WARNING"%s has incorrect frame bounds\n", loadmodel->name);
				galias->warned = true;
			}


//			GL_GenerateNormals((float*)verts, (float*)normals, (int *)((char *)galias + galias->ofs_indexes), galias->numindexes/3, galias->numverts);

			break;

		case ALIAS_GROUP:
		case ALIAS_GROUP_SWAPPED: // prerelease
			ingroup = (daliasgroup_t *)(pframetype+1);

			frame->numposes = LittleLong(ingroup->numframes);
#ifdef SERVERONLY
			pose = (galiaspose_t *)ZG_Malloc(&loadmodel->memgroup, frame->numposes*(sizeof(galiaspose_t) + sizeof(vecV_t)*galias->numverts));
			verts = (vecV_t *)(pose+frame->numposes);
#else
			pose = (galiaspose_t *)ZG_Malloc(&loadmodel->memgroup, frame->numposes*(sizeof(galiaspose_t) + (sizeof(vecV_t)+sizeof(vec3_t)*3)*galias->numverts));
			verts = (vecV_t *)(pose+frame->numposes);
			normals = (vec3_t*)&verts[galias->numverts];
			svec = &normals[galias->numverts];
			tvec = &svec[galias->numverts];
#endif

			frame->poseofs = pose;
			frame->loop = true;
			galias->numanimations++;

			intervals = (daliasinterval_t *)(ingroup+1);
			sinter = LittleFloat(intervals->interval);
			if (sinter <= 0)
				sinter = 0.1;
			frame->rate = 1/sinter;

			pinframe = (dtrivertx_t *)(intervals+frame->numposes);
			for (k = 0; k < frame->numposes; k++)
			{
				pose->ofsverts = verts;
#ifndef SERVERONLY
				pose->ofsnormals = normals;
				pose->ofssvector = svec;
				pose->ofstvector = tvec;
#endif

				frameinfo = (daliasframe_t*)pinframe;
				pinframe = (dtrivertx_t *)((char *)frameinfo + aliasframesize);

				if (k == 0)
				{
					if (mdltype == 1)
						frame->name[0] = '\0';
					else
						Q_strncpyz(frame->name, frameinfo->name, sizeof(frame->name));
				}

				if (mdltype & 16)
				{
					Q1MDL_LoadPose16(galias, pq1inmodel, verts, normals, svec, tvec, pinframe, seamremaps, mdltype);
					pinframe += pq1inmodel->numverts*2;
				}
				else
				{
					Q1MDL_LoadPose(galias, pq1inmodel, verts, normals, svec, tvec, pinframe, seamremaps, mdltype, bbox);
					pinframe += pq1inmodel->numverts;
				}

#ifdef _DEBUG
				if ((bbox[3] > frameinfo->bboxmax.v[0] || bbox[4] > frameinfo->bboxmax.v[1] || bbox[5] > frameinfo->bboxmax.v[2] ||
					bbox[0] < frameinfo->bboxmin.v[0] || bbox[1] < frameinfo->bboxmin.v[1] || bbox[2] < frameinfo->bboxmin.v[2] ||
#else
				if (galias->numverts && (pinframe[0].v[2] > frameinfo->bboxmax.v[2] ||
#endif
					frameinfo->bboxmin.v[0] < ingroup->bboxmin.v[0] || frameinfo->bboxmin.v[1] < ingroup->bboxmin.v[1] || frameinfo->bboxmin.v[2] < ingroup->bboxmin.v[2] ||
					frameinfo->bboxmax.v[0] > ingroup->bboxmax.v[0] || frameinfo->bboxmax.v[1] > ingroup->bboxmax.v[1] || frameinfo->bboxmax.v[2] > ingroup->bboxmax.v[2]) && !galias->warned)
				{
					Con_DPrintf(CON_WARNING"%s has incorrect frame bounds\n", loadmodel->name);
					galias->warned = true;
				}

#ifndef SERVERONLY
				verts = (vecV_t*)&tvec[galias->numverts];
				normals = (vec3_t*)&verts[galias->numverts];
				svec = &normals[galias->numverts];
				tvec = &svec[galias->numverts];
#else
				verts = &verts[galias->numverts];
#endif
				pose++;
			}

//			GL_GenerateNormals((float*)verts, (float*)normals, (int *)((char *)galias + galias->ofs_indexes), galias->numindexes/3, galias->numverts);

			pframetype = (daliasframetype_t *)pinframe;
			break;
		default:
			Con_Printf(CON_ERROR "Bad frame type in %s\n", loadmodel->name);
			return NULL;
		}
		frame++;
	}
	return pframetype;
}

//greatly reduced version of Q1_LoadSkins
//just skips over the data
static void *Q1MDL_LoadSkins_SV (galiasinfo_t *galias, dmdl_t *pq1inmodel, daliasskintype_t *pskintype, unsigned int skintranstype)
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

#ifndef SERVERONLY
static void *Q1MDL_LoadSkins_GL (galiasinfo_t *galias, dmdl_t *pq1inmodel, model_t *loadmodel, daliasskintype_t *pskintype, uploadfmt_t skintranstype)
{
	skinframe_t *frames;
	char skinname[MAX_QPATH];
	char alttexpath[MAX_QPATH];
	int i;
	int s, t;
	float sinter;
	daliasskingroup_t *count;
	daliasskininterval_t *intervals;
	qbyte *data, *saved;
	galiasskin_t *outskin = galias->ofsskins;
	const char *slash;
	unsigned int texflags;
	const char *defaultshader = NULL;

	s = pq1inmodel->skinwidth*pq1inmodel->skinheight;
	for (i = 0; i < pq1inmodel->numskins; i++)
	{
		switch(LittleLong(pskintype->type))
		{
		case ALIAS_SKIN_SINGLE:
			outskin->skinwidth = pq1inmodel->skinwidth;
			outskin->skinheight = pq1inmodel->skinheight;

//but only preload it if we have no replacement.
			outskin->numframes=1;
			if (1 || /*!TEXVALID(texture) ||*/ (loadmodel->engineflags & MDLF_NOTREPLACEMENTS))
			{
				//we're not using 24bits
				frames = ZG_Malloc(&loadmodel->memgroup, sizeof(*frames)+s);
				saved = (qbyte*)(frames+1);
				frames[0].texels = saved;
				memcpy(saved, pskintype+1, s);
				Mod_FloodFillSkin(saved, outskin->skinwidth, outskin->skinheight);
			}
			else
			{
				frames = ZG_Malloc(&loadmodel->memgroup, sizeof(*frames));

				Q_snprintfz(skinname, sizeof(skinname), "%s_%i.lmp", slash, i);
				frames[0].texnums.base = R_LoadReplacementTexture(skinname, alttexpath, texflags, frames[0].texels, outskin->skinwidth, outskin->skinheight, skintranstype);
				if (r_fb_models.ival)
				{
					Q_snprintfz(skinname, sizeof(skinname), "%s_%i_luma.lmp", slash, i);
					frames[0].texnums.fullbright = R_LoadReplacementTexture(skinname, alttexpath, texflags, frames[0].texels, outskin->skinwidth, outskin->skinheight, TF_TRANS8_FULLBRIGHT);
				}
				if (r_loadbumpmapping)
				{
					Q_snprintfz(skinname, sizeof(skinname), "%s_%i_norm.lmp", slash, i);
					frames[0].texnums.bump = R_LoadReplacementTexture(skinname, alttexpath, texflags|IF_TRYBUMP, frames[0].texels, outskin->skinwidth, outskin->skinheight, TF_HEIGHT8PAL);
				}
				Q_snprintfz(skinname, sizeof(skinname), "%s_%i_shirt.lmp", slash, i);
				frames[0].texnums.upperoverlay = R_LoadReplacementTexture(skinname, alttexpath, texflags, NULL, outskin->skinwidth, outskin->skinheight, TF_INVALID);
				Q_snprintfz(skinname, sizeof(skinname), "%s_%i_pants.lmp", slash, i);
				frames[0].texnums.loweroverlay = R_LoadReplacementTexture(skinname, alttexpath, texflags, NULL, outskin->skinwidth, outskin->skinheight, TF_INVALID);
			}
			Q_snprintfz(frames[0].shadername, sizeof(frames[0].shadername), "%s_%i.lmp", loadmodel->name, i);
			frames[0].shader = NULL;
			frames[0].defaultshader = NULL;
			outskin->frame = frames;

			switch(skintranstype)
			{
			default:	//urk
			case TF_SOLID8:
				frames[0].defaultshader = defaultshader;
				break;
			case TF_H2_T7G1:
				frames[0].defaultshader =
					"{\n"
//						"program defaultskin\n"
						"{\n"
							"map $diffuse\n"
							"blendfunc gl_src_alpha gl_one_minus_src_alpha\n"
							"alphagen entity\n"
							"rgbgen lightingDiffuse\n"
							"depthwrite\n"
						"}\n"
					"}\n";
				break;
			case TF_H2_TRANS8_0:
				frames[0].defaultshader =
					"{\n"
						"{\n"
							"map $diffuse\n"
							"blendfunc gl_src_alpha gl_one_minus_src_alpha\n"
							"alphafunc ge128\n"
							"rgbgen lightingDiffuse\n"
							"alphagen entity\n"
							"depthwrite\n"
						"}\n"
					"}\n";
				break;
			case TF_H2_T4A4:
				frames[0].defaultshader =
					"{\n"
						"{\n"
							"map $diffuse\n"
							"blendfunc gl_one_minus_src_alpha gl_src_alpha\n"
							"alphagen entity\n"
							"rgbgen lightingDiffuse\n"
							"cull disable\n"
							"depthwrite\n"
						"}\n"
					"}\n";
				break;
			}

			pskintype = (daliasskintype_t *)((char *)(pskintype+1)+s);
			break;

		default:
			outskin->skinwidth = pq1inmodel->skinwidth;
			outskin->skinheight = pq1inmodel->skinheight;
			count = (daliasskingroup_t*)(pskintype+1);
			intervals = (daliasskininterval_t *)(count+1);
			outskin->numframes = LittleLong(count->numskins);
			data = (qbyte *)(intervals + outskin->numframes);
			frames = ZG_Malloc(&loadmodel->memgroup, sizeof(*frames)*outskin->numframes);
			outskin->frame = frames;
			sinter = LittleFloat(intervals[0].interval);
			if (sinter <= 0)
				sinter = 0.1;
			outskin->skinspeed = 1/sinter;

			for (t = 0; t < outskin->numframes; t++,data+=s)
			{
				frames[t].texels = ZG_Malloc(&loadmodel->memgroup, s);
				memcpy(frames[t].texels, data, s);
				//other engines apparently don't flood fill. because flood filling is horrible, we won't either.
				//Mod_FloodFillSkin(frames[t].texels, outskin->skinwidth, outskin->skinheight);

				Q_snprintfz(frames[t].shadername, sizeof(frames[t].shadername), "%s_%i_%i.lmp", loadmodel->name, i, t);
				frames[t].defaultshader = NULL;
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

void Mesh_HandleFramegroupsFile(model_t *mod, galiasinfo_t *galias)
{
	unsigned int numanims, a, p, g, oldnumanims = galias->numanimations, targpose;
	galiasanimation_t *o, *oldanims = galias->ofsanimations, *frame;
	frameinfo_t *framegroups = ParseFrameInfo(mod->name, &numanims);
	if (framegroups)
	{
		galias->ofsanimations = o = ZG_Malloc(&mod->memgroup, sizeof(*galias->ofsanimations) * numanims);
		for (a = 0; a < numanims; a++, o++)
		{
			o->poseofs = ZG_Malloc(&mod->memgroup, sizeof(*o->poseofs) * framegroups[a].posecount);
			for (p = 0; p < framegroups[a].posecount; p++)
			{
				targpose = framegroups[a].firstpose + p;
				for (g = 0, frame = oldanims; g < oldnumanims; g++, frame++)
				{
					if (targpose < frame->numposes)
						break;
					targpose -= frame->numposes;
				}
				if (g == oldnumanims)
					break;
				o->poseofs[p] = frame->poseofs[targpose];
			}
			o->numposes = p;
			o->rate = framegroups[a].fps;
			o->loop = framegroups[a].loop;
			Q_strncpyz(o->name, framegroups[a].name, sizeof(o->name));
		}
		galias->numanimations = numanims;
		free(framegroups);
	}
}

qboolean QDECL Mod_LoadQ1Model (model_t *mod, void *buffer, size_t fsize)
{
#ifndef SERVERONLY
	vec2_t *st_array;
	int j;
#endif
	int version;
	int i, onseams;
	dstvert_t *pinstverts;
	dtriangle_t *pinq1triangles;
	int *seamremap;
	index_t *indexes;
	daliasskintype_t *skinstart;
	uploadfmt_t skintranstype;
	qboolean sixteenbit;

	int size;
	unsigned int hdrsize;
	void *end;
	qboolean qtest = false;
#ifdef HEXEN2
	dh2triangle_t *pinh2triangles;
	qboolean rapo = false;
#endif
	galiasinfo_t *galias;
	dmdl_t *pq1inmodel = (dmdl_t *)buffer;

	hdrsize = sizeof(dmdl_t) - sizeof(int);

	mod->engineflags |= MDLF_NEEDOVERBRIGHT;

	sixteenbit = pq1inmodel->ident == LittleLong(('6'<<24)+('1'<<16)+('D'<<8)+'M');	//quakeforge's 16bit mdls

	version = LittleLong(pq1inmodel->version);
	if (version == QTESTALIAS_VERSION && !sixteenbit)
	{
		hdrsize = (size_t)&((dmdl_t*)NULL)->flags;
		qtest = true;
	}
#ifdef HEXEN2
	else if (version == 50 && !sixteenbit)
	{
		hdrsize = sizeof(dmdl_t);
		rapo = true;
	}
#endif
	else if (version != ALIAS_VERSION)
	{
		Con_Printf (CON_ERROR "%s has wrong version number (%i should be %i)\n",
				 mod->name, version, ALIAS_VERSION);
		return false;
	}

	seamremap = (int*)pq1inmodel;	//I like overloading locals.

	i = hdrsize/4 - 1;

	for (; i >= 0; i--)
		seamremap[i] = LittleLong(seamremap[i]);

	if (pq1inmodel->numframes < 1 ||
		pq1inmodel->numskins < 1 ||
		pq1inmodel->numtris < 1 ||
		pq1inmodel->numverts < 3 ||
		pq1inmodel->skinheight < 1 ||
		pq1inmodel->skinwidth < 1)
	{
		Con_Printf(CON_ERROR "Model %s has an invalid quantity\n", mod->name);
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
		+ pq1inmodel->numframes*sizeof(galiasanimation_t);

	galias = ZG_Malloc(&mod->memgroup, size);
	galias->ofsanimations = (galiasanimation_t*)(galias+1);
#ifndef SERVERONLY
	galias->ofsskins = (galiasskin_t*)(galias->ofsanimations+pq1inmodel->numframes);
#endif
	galias->nextsurf = 0;

	mod->numframes = pq1inmodel->numframes;

//skins
	skinstart = (daliasskintype_t *)((char*)pq1inmodel+hdrsize);

#ifdef HEXEN2
	if( mod->flags & MFH2_TRANSPARENT )
		skintranstype = TF_H2_T7G1;	//hexen2
	else
#endif
	 if( mod->flags & MFH2_HOLEY )
		skintranstype = TF_H2_TRANS8_0;	//hexen2
#ifdef HEXEN2
	else if( mod->flags & MFH2_SPECIAL_TRANS )
		skintranstype = TF_H2_T4A4;	//hexen2
#endif
	else
		skintranstype = TF_SOLID8;

	switch(qrenderer)
	{
	default:
#ifndef SERVERONLY
		pinstverts = (dstvert_t *)Q1MDL_LoadSkins_GL(galias, pq1inmodel, mod, skinstart, skintranstype);
		break;
#endif
	case QR_NONE:
		pinstverts = (dstvert_t *)Q1MDL_LoadSkins_SV(galias, pq1inmodel, skinstart, skintranstype);
		break;
	}

#ifdef HEXEN2
	if (rapo)
	{
		/*each triangle can use one coord and one st, for each vert, that's a lot of combinations*/
#ifdef SERVERONLY
		/*separate st + vert lists*/
		pinh2triangles = (dh2triangle_t *)&pinstverts[pq1inmodel->num_st];

		seamremap = BZ_Malloc(sizeof(*seamremap)*pq1inmodel->numtris*3);

		galias->numverts = pq1inmodel->numverts;
		galias->numindexes = pq1inmodel->numtris*3;
		indexes = ZG_Malloc(&mod->memgroup, galias->numindexes*sizeof(*indexes));
		galias->ofs_indexes = indexes;
		for (i = 0; i < pq1inmodel->numverts; i++)
			seamremap[i] = i;
		for (i = 0; i < pq1inmodel->numtris; i++)
		{
			indexes[i*3+0] = LittleShort(pinh2triangles[i].vertindex[0]);
			indexes[i*3+1] = LittleShort(pinh2triangles[i].vertindex[1]);
			indexes[i*3+2] = LittleShort(pinh2triangles[i].vertindex[2]);
		}
#else
		int t, v, k;
		int *stremap;
		/*separate st + vert lists*/
		pinh2triangles = (dh2triangle_t *)&pinstverts[pq1inmodel->num_st];

		seamremap = BZ_Malloc(sizeof(int)*pq1inmodel->numtris*6);
		stremap = seamremap + pq1inmodel->numtris*3;

		/*output the indicies as we figure out which verts we want*/
		galias->numindexes = pq1inmodel->numtris*3;
		indexes = ZG_Malloc(&mod->memgroup, galias->numindexes*sizeof(*indexes));
		galias->ofs_indexes = indexes;
		for (i = 0; i < pq1inmodel->numtris; i++)
		{
			for (j = 0; j < 3; j++)
			{
				v = LittleShort(pinh2triangles[i].vertindex[j]);
				t = LittleShort(pinh2triangles[i].stindex[j]);
				if (pinstverts[t].onseam && !pinh2triangles[i].facesfront)
					t += pq1inmodel->num_st;
			 	for (k = 0; k < galias->numverts; k++) /*big fatoff slow loop*/
				{
					if (stremap[k] == t && seamremap[k] == v)
						break;
				}
				if (k == galias->numverts)
				{
					galias->numverts++;
					stremap[k] = t;
					seamremap[k] = v;
				}
				indexes[i*3+j] = k;
			}
		}

		st_array = ZG_Malloc(&mod->memgroup, sizeof(*st_array)*(galias->numverts));
		galias->ofs_st_array = st_array;
		/*generate our st_array now we know which vertexes we want*/
		for (k = 0; k < galias->numverts; k++)
		{
			if (stremap[k] > pq1inmodel->num_st)
			{	/*onseam verts? shrink the index, and add half a texture width to the s coord*/
				st_array[k][0] = 0.5+(LittleLong(pinstverts[stremap[k]-pq1inmodel->num_st].s)+0.5)/(float)pq1inmodel->skinwidth;
				st_array[k][1] = (LittleLong(pinstverts[stremap[k]-pq1inmodel->num_st].t)+0.5)/(float)pq1inmodel->skinheight;
			}
			else
			{
				st_array[k][0] = (LittleLong(pinstverts[stremap[k]].s)+0.5)/(float)pq1inmodel->skinwidth;
				st_array[k][1] = (LittleLong(pinstverts[stremap[k]].t)+0.5)/(float)pq1inmodel->skinheight;
			}
		}
#endif
		end = &pinh2triangles[pq1inmodel->numtris];

		if (Q1MDL_LoadFrameGroup(galias, pq1inmodel, mod, (daliasframetype_t *)end, seamremap, 2) == NULL)
		{
			BZ_Free(seamremap);
			ZG_FreeGroup(&mod->memgroup);
			return false;
		}

		BZ_Free(seamremap);
	}
	else
#endif
	{
		/*onseam means +=skinwidth/2
		verticies that are marked as onseam potentially generate two output verticies.
		the triangle chooses which side based upon its 'onseam' field.
		*/

		//count number of verts that are onseam.
		for (onseams=0,i = 0; i < pq1inmodel->numverts; i++)
		{
			if (pinstverts[i].onseam)
				onseams++;
		}
		seamremap = BZ_Malloc(sizeof(*seamremap)*pq1inmodel->numverts);

		galias->numverts = pq1inmodel->numverts+onseams;

		//st
#ifndef SERVERONLY
		st_array = ZG_Malloc(&mod->memgroup, sizeof(*st_array)*(pq1inmodel->numverts+onseams));
		galias->ofs_st_array = st_array;
		for (j=pq1inmodel->numverts,i = 0; i < pq1inmodel->numverts; i++)
		{
			st_array[i][0] = (LittleLong(pinstverts[i].s)+0.5)/(float)pq1inmodel->skinwidth;
			st_array[i][1] = (LittleLong(pinstverts[i].t)+0.5)/(float)pq1inmodel->skinheight;

			if (pinstverts[i].onseam)
			{
				if (pinstverts[i].onseam != 0x20 && !galias->warned)
				{
					Con_DPrintf(CON_WARNING "Model %s has an invalid seam flag, which may crash software-rendered engines\n", mod->name);
					//1 == ALIAS_LEFT_CLIP
					galias->warned = true;
				}
				st_array[j][0] = st_array[i][0]+0.5;
				st_array[j][1] = st_array[i][1];
				seamremap[i] = j;
				j++;
			}
			else
				seamremap[i] = i;
		}
#else
		for (i = 0; i < pq1inmodel->numverts; i++)
		{
			seamremap[i] = i;
		}
#endif

		//trianglelists;
		pinq1triangles = (dtriangle_t *)&pinstverts[pq1inmodel->numverts];

		galias->numindexes = pq1inmodel->numtris*3;
		indexes = ZG_Malloc(&mod->memgroup, galias->numindexes*sizeof(*indexes));
		galias->ofs_indexes = indexes;
		for (i=0 ; i<pq1inmodel->numtris ; i++)
		{
			unsigned int v1 = LittleLong(pinq1triangles[i].vertindex[0]);
			unsigned int v2 = LittleLong(pinq1triangles[i].vertindex[1]);
			unsigned int v3 = LittleLong(pinq1triangles[i].vertindex[2]);
			if (v1 >= pq1inmodel->numverts || v2 >= pq1inmodel->numverts || v3 >= pq1inmodel->numverts)
			{
				Con_DPrintf(CON_ERROR"%s has invalid triangle (%u %u %u > %u)\n", mod->name, v1, v2, v3, pq1inmodel->numverts);
				v1 = v2 = v3 = 0;
			}
			if (!pinq1triangles[i].facesfront)
			{
				indexes[i*3+0] = seamremap[v1];
				indexes[i*3+1] = seamremap[v2];
				indexes[i*3+2] = seamremap[v3];
			}
			else
			{
				indexes[i*3+0] = v1;
				indexes[i*3+1] = v2;
				indexes[i*3+2] = v3;
			}
		}
		end = &pinq1triangles[pq1inmodel->numtris];

		//frames
		if (Q1MDL_LoadFrameGroup(galias, pq1inmodel, mod, (daliasframetype_t *)end, seamremap, (sixteenbit?16:0) | (qtest?1:0)) == NULL)
		{
			BZ_Free(seamremap);
			ZG_FreeGroup(&mod->memgroup);
			return false;
		}
		BZ_Free(seamremap);
	}


	Mod_CompileTriangleNeighbours(mod, galias);
	Mod_BuildTextureVectors(galias);

	VectorCopy (pq1inmodel->scale_origin, mod->mins);
	VectorMA (mod->mins, 255, pq1inmodel->scale, mod->maxs);

	mod->type = mod_alias;
	Mod_ClampModelSize(mod);

	Mesh_HandleFramegroupsFile(mod, galias);

	mod->meshinfo = galias;

	mod->funcs.NativeTrace = Mod_Trace;

	if (!strcmp(mod->name, "progs/v_shot.mdl"))
		galias->lerpcutoff = 20;
	else if (!strcmp(mod->name, "progs/v_shot2.mdl"))
		galias->lerpcutoff = 20;
	else if (!strcmp(mod->name, "progs/v_nail.mdl"))
		galias->lerpcutoff = 7;
	else if (!strcmp(mod->name, "progs/v_nail2.mdl"))
		galias->lerpcutoff = 6;
	else if (!strcmp(mod->name, "progs/v_rock.mdl"))
		galias->lerpcutoff = 30;
	else if (!strcmp(mod->name, "progs/v_rock2.mdl"))
		galias->lerpcutoff = 30;
	else if (!strcmp(mod->name, "progs/v_light.mdl"))
		galias->lerpcutoff = 30;
#ifdef HEXEN2
	if ((mod->flags == MF_ROCKET) && !strncmp(mod->name, "models/sflesh", 13))
		mod->flags = MFH2_ROCKET;
#endif

	return true;
}
#endif


int Mod_ReadFlagsFromMD1(char *name, int md3version)
{
	int result = 0;
	size_t fsize;
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

	pinmodel = (dmdl_t *)COM_LoadFile(fname, 5, &fsize);
	if (pinmodel)
	{
		if (fsize >= sizeof(dmdl_t) && LittleLong(pinmodel->ident) == IDPOLYHEADER)			
			if (LittleLong(pinmodel->version) == ALIAS_VERSION)
				result = LittleLong(pinmodel->flags);
		BZ_Free(pinmodel);
	}
	return result;
}

#ifdef MD2MODELS

////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//Q2 model loading

typedef struct
{
	float		scale[3];	// multiply qbyte verts by this
	float		translate[3];	// then add this
	char		name[16];	// frame name from grabbing
	dtrivertx_t	verts[1];	// variable sized
} dmd2aliasframe_t;

//static galiasinfo_t *galias;
//static md2_t *pq2inmodel;
#define Q2NUMVERTEXNORMALS	162
extern vec3_t	bytedirs[Q2NUMVERTEXNORMALS];

static void Q2MD2_LoadSkins(galiasinfo_t *galias, model_t *mod, md2_t *pq2inmodel, char *skins)
{
#ifndef SERVERONLY
	int i;
	skinframe_t *frames;
	galiasskin_t *outskin = galias->ofsskins;

	for (i = 0; i < LittleLong(pq2inmodel->num_skins); i++, outskin++)
	{
		frames = ZG_Malloc(&mod->memgroup, sizeof(*frames));
		outskin->frame = frames;
		outskin->numframes=1;

		COM_CleanUpPath(skins);	//blooming tanks.
		Q_strncpyz(frames->shadername, skins, sizeof(frames->shadername));

		outskin->skinwidth = 0;
		outskin->skinheight = 0;
		outskin->skinspeed = 0;

		skins += MD2MAX_SKINNAME;
	}
#endif
	galias->numskins = LittleLong(pq2inmodel->num_skins);

	/*
#ifndef SERVERONLY
	outskin = (galiasskin_t *)((char *)galias + galias->ofsskins);
	outskin += galias->numskins - 1;
	if (galias->numskins)
	{
		if (*(shader_t**)((char *)outskin + outskin->ofstexnums))
			return;

		galias->numskins--;
	}
#endif
	*/
}

#define MD2_MAX_TRIANGLES 4096
qboolean QDECL Mod_LoadQ2Model (model_t *mod, void *buffer, size_t fsize)
{
#ifndef SERVERONLY
	dmd2stvert_t *pinstverts;
	vec2_t *st_array;
	vec3_t *normals;
#endif
	md2_t *pq2inmodel;

	int version;
	int i, j;
	dmd2triangle_t *pintri;
	index_t *indexes;
	int numindexes;

	vec3_t min;
	vec3_t max;

	galiaspose_t *pose;
	galiasanimation_t *poutframe;
	dmd2aliasframe_t *pinframe;
	int framesize;
	vecV_t *verts;

	int		indremap[MD2_MAX_TRIANGLES*3];
	unsigned short		ptempindex[MD2_MAX_TRIANGLES*3], ptempstindex[MD2_MAX_TRIANGLES*3];

	int numverts;

	int size;
	galiasinfo_t *galias;

	mod->engineflags |= MDLF_NEEDOVERBRIGHT;

	pq2inmodel = (md2_t *)buffer;

	version = LittleLong (pq2inmodel->version);
	if (version != MD2ALIAS_VERSION)
	{
		Con_Printf (CON_ERROR "%s has wrong version number (%i should be %i)\n",
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
		Con_Printf(CON_ERROR "Model %s has an invalid quantity\n", mod->name);
		return false;
	}

	mod->flags = 0;

	mod->numframes = LittleLong(pq2inmodel->num_frames);

	size = sizeof(galiasinfo_t)
#ifndef SERVERONLY
		+ LittleLong(pq2inmodel->num_skins)*sizeof(galiasskin_t)
#endif
		+ LittleLong(pq2inmodel->num_frames)*sizeof(galiasanimation_t);

	galias = ZG_Malloc(&mod->memgroup, size);
	galias->ofsanimations = (galiasanimation_t*)(galias+1);
#ifndef SERVERONLY
	galias->ofsskins = (galiasskin_t*)(galias->ofsanimations + LittleLong(pq2inmodel->num_frames));
#endif
	galias->nextsurf = 0;

//skins
	Q2MD2_LoadSkins(galias, mod, pq2inmodel, ((char *)pq2inmodel+LittleLong(pq2inmodel->ofs_skins)));

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
	indexes = ZG_Malloc(&mod->memgroup, galias->numindexes*sizeof(*indexes));
	galias->ofs_indexes = indexes;
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

//	Con_DPrintf ( "%s: remapped %i verts to %i\n", mod->name, LittleLong(pq2inmodel->num_xyz), numverts );

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
	st_array = ZG_Malloc(&mod->memgroup, sizeof(*st_array)*(numverts));
	galias->ofs_st_array = st_array;

	for (j=0 ; j<numindexes; j++)
	{
		st_array[indexes[j]][0] = (float)(((double)LittleShort (pinstverts[ptempstindex[indremap[j]]].s) + 0.5f) /LittleLong(pq2inmodel->skinwidth));
		st_array[indexes[j]][1] = (float)(((double)LittleShort (pinstverts[ptempstindex[indremap[j]]].t) + 0.5f) /LittleLong(pq2inmodel->skinheight));
	}
#endif

	//frames
	ClearBounds ( mod->mins, mod->maxs );

	poutframe = galias->ofsanimations;
	framesize = LittleLong (pq2inmodel->framesize);
	for (i=0 ; i<LittleLong(pq2inmodel->num_frames) ; i++)
	{
		size = sizeof(galiaspose_t) + sizeof(vecV_t)*numverts;
#ifndef SERVERONLY
		size += 3*sizeof(vec3_t)*numverts;
#endif
		pose = (galiaspose_t *)ZG_Malloc(&mod->memgroup, size);
		poutframe->poseofs = pose;
		poutframe->numposes = 1;
		galias->numanimations++;

		verts = (vecV_t *)(pose+1);
		pose->ofsverts = verts;
#ifndef SERVERONLY
		normals = (vec3_t*)&verts[galias->numverts];
		pose->ofsnormals = normals;

		pose->ofssvector = &normals[galias->numverts];
		pose->ofstvector = &normals[galias->numverts*2];
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



	Mod_CompileTriangleNeighbours(mod, galias);
	Mod_BuildTextureVectors(galias);
	/*
	VectorCopy (pq2inmodel->scale_origin, mod->mins);
	VectorMA (mod->mins, 255, pq2inmodel->scale, mod->maxs);
	*/

	Mod_ClampModelSize(mod);

	mod->meshinfo = galias;
	mod->type = mod_alias;

	mod->funcs.NativeTrace = Mod_Trace;

	return true;
}

#endif









int Mod_GetNumBones(model_t *model, qboolean allowtags)
{
	if (model && model->type == mod_alias)
	{
		galiasinfo_t *inf = Mod_Extradata(model);

#ifdef SKELETALMODELS
		if (inf->numbones)
			return inf->numbones;
		else
#endif
			if (allowtags)
			return inf->numtags;
		return 0;
	}
#ifdef HALFLIFEMODELS
	if (model && model->type == mod_halflife)
		return HLMDL_GetNumBones(model);
#endif
	return 0;
}

int Mod_GetBoneRelations(model_t *model, int firstbone, int lastbone, framestate_t *fstate, float *result)
{
#ifdef SKELETALMODELS
	if (model && model->type == mod_alias)
		return Alias_BlendBoneData(Mod_Extradata(model), fstate, result, SKEL_RELATIVE, firstbone, lastbone);
#endif
#ifdef HALFLIFEMODELS
	if (model && model->type == mod_halflife)
		return HLMDL_GetBoneData(model, firstbone, lastbone, fstate, result);
#endif
	return 0;
}

galiasbone_t *Mod_GetBoneInfo(model_t *model, int *numbones)
{
#ifdef SKELETALMODELS
	galiasbone_t *bone;
	galiasinfo_t *inf;


	if (!model || model->type != mod_alias)
		return NULL;

	inf = Mod_Extradata(model);

	bone = inf->ofsbones;
	*numbones = inf->numbones;
	return bone;
#else
	*numbones = 0;
	return NULL;
#endif
}

int Mod_GetBoneParent(model_t *model, int bonenum)
{
#ifdef SKELETALMODELS
	galiasbone_t *bone;
	galiasinfo_t *inf;


	if (!model || model->type != mod_alias)
		return 0;

	inf = Mod_Extradata(model);


	bonenum--;
	if ((unsigned int)bonenum >= inf->numbones)
		return 0;	//no parent
	bone = inf->ofsbones;
	return bone[bonenum].parent+1;
#endif
	return 0;
}

char *Mod_GetBoneName(model_t *model, int bonenum)
{
#ifdef SKELETALMODELS
	galiasbone_t *bone;
	galiasinfo_t *inf;


	if (!model || model->type != mod_alias)
		return 0;

	inf = Mod_Extradata(model);


	bonenum--;
	if ((unsigned int)bonenum >= inf->numbones)
		return 0;	//no parent
	bone = inf->ofsbones;
	return bone[bonenum].name;
#endif
	return 0;
}

qboolean Mod_GetTag(model_t *model, int tagnum, framestate_t *fstate, float *result)
{
	galiasinfo_t *inf;


	if (!model || model->type != mod_alias)
		return false;

	inf = Mod_Extradata(model);
#ifdef SKELETALMODELS
	if (inf->numbones)
	{
		galiasbone_t *bone = inf->ofsbones;

		float tempmatrix[12];			//flipped between this and bonematrix
		float *matrix;	//the matrix for a single bone in a single pose.
		float m[12];	//combined interpolated version of 'matrix'.
		int b, k;	//counters

		int numbonegroups = 0;
		skellerps_t lerps[FS_COUNT], *lerp;

		if (tagnum <= 0 || tagnum > inf->numbones)
			return false;
		tagnum--;	//tagnum 0 is 'use my angles/org'

		//data comes from skeletal object, if possible
		if (!numbonegroups && fstate->bonestate)
		{
			if (tagnum >= fstate->bonecount)
				return false;

			if (fstate->skeltype == SKEL_ABSOLUTE)
			{	//can just directly read it, woo.
				memcpy(result, fstate->bonestate + 12 * tagnum, 12*sizeof(*result));
				return true;
			}

			lerps[0].pose[0] = fstate->bonestate;
			lerps[0].frac[0] = 1;
			lerps[0].lerpcount = 1;
			lerps[0].firstbone = 0;
			lerps[0].endbone = fstate->bonecount;
			numbonegroups = 1;
		}

		//try getting the data from the frame state
		if (!numbonegroups)
			numbonegroups = Alias_FindRawSkelData(inf, fstate, lerps, 0, inf->numbones);

		//try base pose?
		if (!numbonegroups && inf->baseframeofs)
		{
			lerps[0].pose[0] = inf->baseframeofs;
			lerps[0].frac[0] = 1;
			lerps[0].lerpcount = 1;
			lerps[0].firstbone = 0;
			lerps[0].endbone = inf->numbones;
			numbonegroups = 1;
		}

		//make sure it was all okay.
		if (!numbonegroups || tagnum >= lerps[numbonegroups-1].endbone)
			return false;

		//set up the identity matrix
		for (k = 0;k < 12;k++)
			result[k] = 0;
		result[0] = 1;
		result[5] = 1;
		result[10] = 1;
		while(tagnum >= 0)
		{
			for (lerp = lerps; tagnum < lerp->firstbone; lerp++)
				;
			//set up the per-bone transform matrix
			matrix = lerp->pose[0] + tagnum*12;
			for (k = 0;k < 12;k++)
				m[k] = matrix[k] * lerp->frac[0];
			for (b = 1;b < lerp->lerpcount;b++)
			{
				matrix = lerp->pose[b] + tagnum*12;
				for (k = 0;k < 12;k++)
					m[k] += matrix[k] * lerp->frac[b];
			}

			if (lerp->skeltype == SKEL_ABSOLUTE)
			{
				memcpy(result, m, sizeof(tempmatrix));
				return true;
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

		int frame1, frame2;
		//float f1time, f2time;	//tags/md3s don't support framegroups.
		float f2ness;

		frame1 = fstate->g[FS_REG].frame[0];
		frame2 = fstate->g[FS_REG].frame[1];
		//f1time = fstate->g[FS_REG].frametime[0];
		//f2time = fstate->g[FS_REG].frametime[1];
		f2ness = fstate->g[FS_REG].lerpweight[1];

		if (tagnum <= 0 || tagnum > inf->numtags)
			return false;
		if (frame1 < 0)
			return false;
		if (frame1 >= inf->numtagframes)
			frame1 = inf->numtagframes - 1;
		if (frame2 < 0 || frame2 >= inf->numtagframes)
			frame2 = frame1;
		tagnum--;	//tagnum 0 is 'use my angles/org'

		t1 = inf->ofstags;
		t1 += tagnum;
		t1 += inf->numtags*frame1;

		t2 = inf->ofstags;
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

		VectorNormalize(result);
		VectorNormalize(result+4);
		VectorNormalize(result+8);

		return true;
	}
	return false;
}

int Mod_TagNumForName(model_t *model, const char *name)
{
	int i;
	galiasinfo_t *inf;
	md3tag_t *t;

	if (!model)
		return 0;
	if (model->loadstate != MLS_LOADED)
		return 0;
#ifdef HALFLIFEMODELS
	if (model->type == mod_halflife)
		return HLMDL_BoneForName(model, name);
#endif
	if (model->type != mod_alias)
		return 0;
	inf = Mod_Extradata(model);

#ifdef SKELETALMODELS
	if (inf->numbones)
	{
		galiasbone_t *b;
		b = inf->ofsbones;
		for (i = 0; i < inf->numbones; i++)
		{
			if (!strcmp(b[i].name, name))
				return i+1;
		}
	}
#endif
	t = inf->ofstags;
	for (i = 0; i < inf->numtags; i++)
	{
		if (!strcmp(t[i].name, name))
			return i+1;
	}

	return 0;
}

int Mod_FrameNumForName(model_t *model, int surfaceidx, const char *name)
{
	galiasanimation_t *group;
	galiasinfo_t *inf;
	int i;

	if (!model)
		return -1;
#ifdef HALFLIFEMODELS
	if (model->type == mod_halflife)
		return HLMDL_FrameForName(model, name);
#endif
	if (model->type != mod_alias)
		return 0;

	inf = Mod_Extradata(model);

	while(surfaceidx-->0 && inf)
		inf = inf->nextsurf;
	if (inf)
	{
		group = inf->ofsanimations;
		for (i = 0; i < inf->numanimations; i++, group++)
		{
			if (!strcmp(group->name, name))
				return i;
		}
	}
	return -1;
}

#ifndef SERVERONLY
int Mod_SkinNumForName(model_t *model, int surfaceidx, const char *name)
{
	int i;
	galiasinfo_t *inf;
	galiasskin_t *skin;

	if (!model || model->type != mod_alias)
		return -1;
	inf = Mod_Extradata(model);

	while(surfaceidx-->0 && inf)
		inf = inf->nextsurf;
	if (inf)
	{
		skin = inf->ofsskins;
		for (i = 0; i < inf->numskins; i++, skin++)
		{
			if (!strcmp(skin->name, name))
				return i;
		}
	}
	return -1;
}
#endif

const char *Mod_FrameNameForNum(model_t *model, int surfaceidx, int num)
{
	galiasanimation_t *group;
	galiasinfo_t *inf;

	if (!model)
		return NULL;
	if (model->type == mod_alias)
	{
		inf = Mod_Extradata(model);

		while(surfaceidx-->0 && inf)
			inf = inf->nextsurf;

		if (!inf || num >= inf->numanimations)
			return NULL;
		group = inf->ofsanimations;
		return group[num].name;
	}
#ifdef HALFLIFEMODELS
	if (model->type == mod_halflife)
		return HLMDL_FrameNameForNum(model, surfaceidx, num);
#endif
	return NULL;
}

qboolean Mod_FrameInfoForNum(model_t *model, int surfaceidx, int num, char **name, int *numframes, float *duration, qboolean *loop)
{
	galiasanimation_t *group;
	galiasinfo_t *inf;

	if (!model)
		return false;
	if (model->type == mod_alias)
	{
		inf = Mod_Extradata(model);

		while(surfaceidx-->0 && inf)
			inf = inf->nextsurf;

		if (!inf || num >= inf->numanimations)
			return false;
		group = inf->ofsanimations;

		*name = group[num].name;
		*numframes = group[num].numposes;
		*loop = group[num].loop;
		*duration = group->numposes/group->rate;
		return true;
	}
#ifdef HALFLIFEMODELS
	if (model->type == mod_halflife)
		return HLMDL_FrameInfoForNum(model, surfaceidx, num, name, numframes, duration, loop);
#endif
	return false;
}

#ifndef SERVERONLY
shader_t *Mod_ShaderForSkin(model_t *model, int surfaceidx, int num)
{
	galiasinfo_t *inf;
	galiasskin_t *skin;

	if (!model || model->type != mod_alias)
	{
		if (model->type == mod_brush && surfaceidx < model->numtextures && !num)
			return model->textures[surfaceidx]->shader;
		return NULL;
	}
	inf = Mod_Extradata(model);

	while(surfaceidx-->0 && inf)
		inf = inf->nextsurf;

	if (!inf || num >= inf->numskins)
		return NULL;
	skin = inf->ofsskins;
	return skin[num].frame[0].shader;
}
#endif
const char *Mod_SkinNameForNum(model_t *model, int surfaceidx, int num)
{
#ifdef SERVERONLY
	return NULL;
#else
	galiasinfo_t *inf;
	galiasskin_t *skin;

	if (!model || model->type != mod_alias)
	{
		if (model->type == mod_brush && surfaceidx < model->numtextures && !num)
			return "";
		return NULL;
	}
	inf = Mod_Extradata(model);

	while(surfaceidx-->0 && inf)
		inf = inf->nextsurf;
	if (!inf || num >= inf->numskins)
		return NULL;
	skin = inf->ofsskins;
//	if (!*skin[num].name)
//		return skin[num].frame[0].shadername;
//	else
		return skin[num].name;
#endif
}

const char *Mod_SurfaceNameForNum(model_t *model, int num)
{
#ifdef SERVERONLY
	return NULL;
#else
	galiasinfo_t *inf;

	if (!model || model->type != mod_alias)
	{
		if (model->type == mod_brush && num < model->numtextures)
			return model->textures[num]->name;
		return NULL;
	}
	inf = Mod_Extradata(model);

	while(num-->0 && inf)
		inf = inf->nextsurf;
	if (inf)
		return inf->surfacename;
	else
		return NULL;
#endif
}

float Mod_GetFrameDuration(model_t *model, int surfaceidx, int frameno)
{
	galiasinfo_t *inf;
	galiasanimation_t *group;

	if (!model || model->type != mod_alias)
		return 0;
	inf = Mod_Extradata(model);
	
	while(surfaceidx-->0 && inf)
		inf = inf->nextsurf;

	if (inf)
	{
		group = inf->ofsanimations;
		if (frameno >= 0 && frameno < inf->numanimations)
		{
			group += frameno;
			return group->numposes/group->rate;
		}
	}
	return 0;
}


#ifdef MD3MODELS

//structures from Tenebrae
typedef struct {
	int			ident;
	int			version;

	char		name[64];

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

	char	name[64];	// polyset name

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
	char			name[64];
	int				shaderIndex;
} md3Shader_t;
//End of Tenebrae 'assistance'

qboolean QDECL Mod_LoadQ3Model(model_t *mod, void *buffer, size_t fsize)
{
#ifndef SERVERONLY
	galiasskin_t	*skin;
	skinframe_t	*frames;
	float lat, lng;
	md3St_t			*inst;
	vec3_t *normals;
	vec3_t *svector;
	vec3_t *tvector;
	vec2_t *st_array;
	md3Shader_t		*inshader;
	int externalskins;
#endif
//	int version;
	int s, i, j, d;

	index_t *indexes;

	vec3_t min;
	vec3_t max;

	galiaspose_t *pose;
	galiasinfo_t *parent, *root;
	galiasanimation_t *group;

	vecV_t *verts;

	md3Triangle_t	*intris;
	md3XyzNormal_t	*invert;


	int size;

	md3Header_t		*header;
	md3Surface_t	*surf;
	galiasinfo_t	*galias;

	header = buffer;

//	if (header->version != sdfs)
//		Sys_Error("GL_LoadQ3Model: Bad version\n");

	parent = NULL;
	root = NULL;

#ifndef SERVERONLY
	externalskins = Mod_CountSkinFiles(mod->name);
#endif

	min[0] = min[1] = min[2] = 0;
	max[0] = max[1] = max[2] = 0;

	surf = (md3Surface_t *)((qbyte *)header + LittleLong(header->ofsSurfaces));
	for (s = 0; s < LittleLong(header->numSurfaces); s++)
	{
		if (LittleLong(surf->ident) != MD3_IDENT)
			Con_Printf(CON_WARNING "Warning: md3 sub-surface doesn't match ident\n");
		size = sizeof(galiasinfo_t) + sizeof(galiasanimation_t)*LittleLong(header->numFrames);
		galias = ZG_Malloc(&mod->memgroup, size);
		galias->ofsanimations = (galiasanimation_t*)(galias+1);	//frame groups
		galias->numanimations = LittleLong(header->numFrames);
		galias->numverts = LittleLong(surf->numVerts);
		galias->numindexes = LittleLong(surf->numTriangles)*3;
		galias->shares_verts = s;
		if (parent)
			parent->nextsurf = galias;
		else
			root = galias;
		parent = galias;

		Q_strncpyz(galias->surfacename, surf->name, sizeof(galias->surfacename));

#ifndef SERVERONLY
		st_array = ZG_Malloc(&mod->memgroup, sizeof(vec2_t)*galias->numindexes);
		galias->ofs_st_array = st_array;
		inst = (md3St_t*)((qbyte*)surf + LittleLong(surf->ofsSt));
		for (i = 0; i < galias->numverts; i++)
		{
			st_array[i][0] = LittleFloat(inst[i].s);
			st_array[i][1] = LittleFloat(inst[i].t);
		}
#endif

		indexes = ZG_Malloc(&mod->memgroup, sizeof(*indexes)*galias->numindexes);
		galias->ofs_indexes = indexes;
		intris = (md3Triangle_t *)((qbyte*)surf + LittleLong(surf->ofsTriangles));
		for (i = 0; i < LittleLong(surf->numTriangles); i++)
		{
			indexes[i*3+0] = LittleLong(intris[i].indexes[0]);
			indexes[i*3+1] = LittleLong(intris[i].indexes[1]);
			indexes[i*3+2] = LittleLong(intris[i].indexes[2]);
		}

		group = (galiasanimation_t *)(galias+1);
		invert = (md3XyzNormal_t *)((qbyte*)surf + LittleLong(surf->ofsXyzNormals));
		for (i = 0; i < LittleLong(surf->numFrames); i++)
		{
			int size = sizeof(galiaspose_t) + sizeof(vecV_t)*LittleLong(surf->numVerts);
#ifndef SERVERONLY
			size += 3*sizeof(vec3_t)*LittleLong(surf->numVerts);
#endif
			pose = (galiaspose_t *)ZG_Malloc(&mod->memgroup, size);

			verts = (vecV_t*)(pose+1);
			pose->ofsverts = verts;
#ifndef SERVERONLY
			normals = (vec3_t*)(verts + LittleLong(surf->numVerts));
			pose->ofsnormals = normals;
			svector = normals + LittleLong(surf->numVerts);
			pose->ofssvector = svector;
			tvector = svector + LittleLong(surf->numVerts);
			pose->ofstvector = tvector;
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
			group->poseofs = pose;

			group++;
			invert += LittleLong(surf->numVerts);
		}

#ifndef SERVERONLY
		if (externalskins<LittleLong(surf->numShaders))
			externalskins = LittleLong(surf->numShaders);
		if (externalskins)
		{
			skin = ZG_Malloc(&mod->memgroup, (externalskins)*((sizeof(galiasskin_t)+sizeof(skinframe_t))));
			galias->ofsskins = skin;
			frames = (skinframe_t *)(skin + externalskins);
			inshader = (md3Shader_t *)((qbyte *)surf + LittleLong(surf->ofsShaders));
			for (i = 0; i < externalskins; i++)
			{
				skin->numframes = 1;
				skin->frame = &frames[i];
				skin->skinwidth = 0;
				skin->skinheight = 0;
				skin->skinspeed = 0;

				if (i >= LittleLong(surf->numShaders))
					Q_strncpyz(frames->shadername, "", sizeof(frames->shadername));	//this shouldn't be possible
				else
					Q_strncpyz(frames->shadername, inshader->name, sizeof(frames->shadername));

				inshader++;
				skin++;
			}
			galias->numskins = i;
		}
#endif

		VectorCopy(min, mod->mins);
		VectorCopy(max, mod->maxs);


		Mod_CompileTriangleNeighbours (mod, galias);
		Mod_BuildTextureVectors(galias);

		surf = (md3Surface_t *)((qbyte *)surf + LittleLong(surf->ofsEnd));
	}

	if (!root)
		root = ZG_Malloc(&mod->memgroup, sizeof(galiasinfo_t));

	root->numtagframes = LittleLong(header->numFrames);
	root->numtags = LittleLong(header->numTags);
	root->ofstags = ZG_Malloc(&mod->memgroup, LittleLong(header->numTags)*sizeof(md3tag_t)*LittleLong(header->numFrames));

	{
		md3tag_t *src;
		md3tag_t *dst;

		src = (md3tag_t *)((char*)header+LittleLong(header->ofsTags));
		dst = root->ofstags;
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

#ifndef SERVERONLY
	if (mod_md3flags.value)
		mod->flags = LittleLong(header->flags);
	else
#endif
		mod->flags = 0;
	if (!mod->flags)
		mod->flags = Mod_ReadFlagsFromMD1(mod->name, 0);

	Mod_ClampModelSize(mod);

	mod->type = mod_alias;
	mod->meshinfo = root;

	mod->funcs.NativeTrace = Mod_Trace;

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
qboolean QDECL Mod_LoadZymoticModel(model_t *mod, void *buffer, size_t fsize)
{
#ifndef SERVERONLY
	galiasskin_t *skin;
	skinframe_t *skinframe;
	int skinfiles;
	int j;
#endif

	int i;

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

	galiasanimation_t *grp;
	zymscene_t *inscene;

	int *renderlist, count;
	index_t *indexes;

	char *surfname;

	header = buffer;

	if (memcmp(header->id, "ZYMOTICMODEL", 12))
	{
		Con_Printf("Mod_LoadZymoticModel: %s, doesn't appear to BE a zymotic!\n", mod->name);
		return false;
	}

	if (BigLong(header->type) != 1)
	{
		Con_Printf("Mod_LoadZymoticModel: %s, only type 1 is supported\n", mod->name);
		return false;
	}

	for (i = 0; i < sizeof(zymtype1header_t)/4; i++)
		((int*)header)[i] = BigLong(((int*)header)[i]);

	if (!header->numverts)
	{
		Con_Printf("Mod_LoadZymoticModel: %s, no vertexes\n", mod->name);
		return false;
	}

	if (!header->numsurfaces)
	{
		Con_Printf("Mod_LoadZymoticModel: %s, no surfaces\n", mod->name);
		return false;
	}

	VectorCopy(header->mins, mod->mins);
	VectorCopy(header->maxs, mod->maxs);

	root = ZG_Malloc(&mod->memgroup, sizeof(galiasinfo_t)*header->numsurfaces);

	root->numswtransforms = header->lump_verts.length/sizeof(zymvertex_t);
	transforms = ZG_Malloc(&mod->memgroup, root->numswtransforms*sizeof(*transforms));
	root->ofsswtransforms = transforms;

	vertbonecounts = (int *)((char*)header + header->lump_vertbonecounts.start);
	intrans = (zymvertex_t *)((char*)header + header->lump_verts.start);

	vertbonecounts[0] = BigLong(vertbonecounts[0]);
	multiplier = 1.0f / vertbonecounts[0];
	for (i = 0, v=0; i < root->numswtransforms; i++)
	{
		while(!vertbonecounts[v])
		{
			v++;
			if (v == header->numverts)
			{
				Con_Printf("Mod_LoadZymoticModel: %s, too many transformations\n", mod->name);
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
		Con_Printf(CON_ERROR "%s, Vertex transforms list appears corrupt.\n", mod->name);
		return false;
	}
	if (vertbonecounts != (int *)((char*)header + header->lump_vertbonecounts.start))
	{
		Con_Printf(CON_ERROR "%s, Vertex bone counts list appears corrupt.\n", mod->name);
		return false;
	}

	root->numverts = v+1;

	root->numbones = header->numbones;
	bone = ZG_Malloc(&mod->memgroup, root->numswtransforms*sizeof(*transforms));
	inbone = (zymbone_t*)((char*)header + header->lump_bones.start);
	for (i = 0; i < root->numbones; i++)
	{
		Q_strncpyz(bone[i].name, inbone[i].name, sizeof(bone[i].name));
		bone[i].parent = BigLong(inbone[i].parent);
	}
	root->ofsbones = bone;

	renderlist = (int*)((char*)header + header->lump_render.start);
	for (i = 0;i < header->numsurfaces; i++)
	{
		count = BigLong(*renderlist++);
		count *= 3;
		indexes = ZG_Malloc(&mod->memgroup, count*sizeof(*indexes));
		root[i].ofs_indexes = indexes;
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
		Con_Printf(CON_ERROR "%s, render list appears corrupt.\n", mod->name);
		return false;
	}

	grp = ZG_Malloc(&mod->memgroup, sizeof(*grp)*header->numscenes*header->numsurfaces);
	matrix = ZG_Malloc(&mod->memgroup, header->lump_poses.length);
	inmatrix = (float*)((char*)header + header->lump_poses.start);
	for (i = 0; i < header->lump_poses.length/4; i++)
		matrix[i] = BigFloat(inmatrix[i]);
	inscene = (zymscene_t*)((char*)header + header->lump_scenes.start);
	surfname = ((char*)header + header->lump_surfnames.start);

	stcoords = ZG_Malloc(&mod->memgroup, root[0].numverts*sizeof(vec2_t));
	inst = (vec2_t *)((char *)header + header->lump_texcoords.start);
	for (i = 0; i < header->lump_texcoords.length/8; i++)
	{
		stcoords[i][0] = BigFloat(inst[i][0]);
		stcoords[i][1] = 1-BigFloat(inst[i][1]);	//hmm. upside down skin coords?
	}

#ifndef SERVERONLY
	skinfiles = Mod_CountSkinFiles(mod->name);
#endif

	for (i = 0; i < header->numsurfaces; i++, surfname+=32)
	{
		root[i].numanimations = header->numscenes;
		root[i].ofsanimations = grp;

		Q_strncpyz(root[i].surfacename, surfname, sizeof(root[i].surfacename));

#ifdef SERVERONLY
		root[i].numskins = 1;
#else
		root[i].ofs_st_array = stcoords;
		root[i].numskins = skinfiles;

		skin = ZG_Malloc(&mod->memgroup, (sizeof(galiasskin_t)+sizeof(skinframe_t*))*skinfiles);
		skinframe = (skinframe_t*)(skin+skinfiles);
		for (j = 0; j < skinfiles; j++, skinframe++)
		{
			skin[j].numframes = 1;	//non-sequenced skins.
			skin[j].frame = skinframe;

//			shaders[0] = Mod_LoadSkinFile(NULL, &root[i], j, NULL, 0, 0, NULL, skin->name);
		}

		root[i].ofsskins = skin;
#endif
	}


	for (i = 0; i < header->numscenes; i++, grp++, inscene++)
	{
		Q_strncpyz(grp->name, inscene->name, sizeof(grp->name));

		grp->skeltype = SKEL_RELATIVE;
		grp->rate = BigFloat(inscene->framerate);
		grp->loop = !(BigLong(inscene->flags) & ZYMSCENEFLAG_NOLOOP);
		grp->numposes = BigLong(inscene->length);
		grp->boneofs = matrix + BigLong(inscene->start)*12*root->numbones;
	}

	if (inscene != (zymscene_t*)((char*)header + header->lump_scenes.start+header->lump_scenes.length))
	{
		Con_Printf(CON_ERROR "%s, scene list appears corrupt.\n", mod->name);
		return false;
	}

	for (i = 0; i < header->numsurfaces-1; i++)
		root[i].nextsurf = &root[i+1];
	for (i = 1; i < header->numsurfaces; i++)
	{
		root[i].shares_verts = 0;
		root[i].numbones = root[0].numbones;
		root[i].numverts = root[0].numverts;

		root[i].ofsbones = root[0].ofsbones;
	}

	Alias_CalculateSkeletalNormals(root);

	mod->flags = Mod_ReadFlagsFromMD1(mod->name, 0);	//file replacement - inherit flags from any defunc mdl files.

	Mod_ClampModelSize(mod);


	mod->meshinfo = root;
	mod->type = mod_alias;

	mod->funcs.NativeTrace = Mod_Trace;

	return true;
}
#endif //ZYMOTICMODELS


///////////////////////////////////////////////////////////////
//psk
#ifdef PSKMODELS
/*Typedefs copied from DarkPlaces*/

typedef struct pskchunk_s
{
	// id is one of the following:
	// .psk:
	// ACTRHEAD (recordsize = 0, numrecords = 0)
	// PNTS0000 (recordsize = 12, pskpnts_t)
	// VTXW0000 (recordsize = 16, pskvtxw_t)
	// FACE0000 (recordsize = 12, pskface_t)
	// MATT0000 (recordsize = 88, pskmatt_t)
	// REFSKELT (recordsize = 120, pskboneinfo_t)
	// RAWWEIGHTS (recordsize = 12, pskrawweights_t)
	// .psa:
	// ANIMHEAD (recordsize = 0, numrecords = 0)
	// BONENAMES (recordsize = 120, pskboneinfo_t)
	// ANIMINFO (recordsize = 168, pskaniminfo_t)
	// ANIMKEYS (recordsize = 32, pskanimkeys_t)
	char id[20];
	// in .psk always 0x1e83b9
	// in .psa always 0x2e
	int version;
	int recordsize;
	int numrecords;
} pskchunk_t;

typedef struct pskpnts_s
{
	float origin[3];
} pskpnts_t;

typedef struct pskvtxw_s
{
	unsigned short pntsindex; // index into PNTS0000 chunk
	unsigned char unknown1[2]; // seems to be garbage
	float texcoord[2];
	unsigned char mattindex; // index into MATT0000 chunk
	unsigned char unknown2; // always 0?
	unsigned char unknown3[2]; // seems to be garbage
} pskvtxw_t;

typedef struct pskface_s
{
	unsigned short vtxwindex[3]; // triangle
	unsigned char mattindex; // index into MATT0000 chunk
	unsigned char unknown; // seems to be garbage
	unsigned int group; // faces seem to be grouped, possibly for smoothing?
} pskface_t;

typedef struct pskmatt_s
{
	char name[64];
	int unknown[6]; // observed 0 0 0 0 5 0
} pskmatt_t;

typedef struct pskpose_s
{
	float quat[4];
	float origin[3];
	float unknown; // probably a float, always seems to be 0
	float size[3];
} pskpose_t;

typedef struct pskboneinfo_s
{
	char name[64];
	int unknown1;
	int numchildren;
	int parent; // root bones have 0 here
	pskpose_t basepose;
} pskboneinfo_t;

typedef struct pskrawweights_s
{
	float weight;
	int pntsindex;
	int boneindex;
} pskrawweights_t;

typedef struct pskaniminfo_s
{
	char name[64];
	char group[64];
	int numbones;
	int unknown1;
	int unknown2;
	int unknown3;
	float unknown4;
	float playtime; // not really needed
	float fps; // frames per second
	int unknown5;
	int firstframe;
	int numframes;
	// firstanimkeys = (firstframe + frameindex) * numbones
} pskaniminfo_t;

typedef struct pskanimkeys_s
{
	float origin[3];
	float quat[4];
	float frametime;
} pskanimkeys_t;


qboolean QDECL Mod_LoadPSKModel(model_t *mod, void *buffer, size_t fsize)
{
	pskchunk_t *chunk;
	unsigned int pos = 0;
	unsigned int i, j;
	qboolean fail = false;
	char basename[MAX_QPATH];
	char psaname[MAX_QPATH];

	galiasinfo_t *gmdl;
#ifndef SERVERONLY
	vec2_t *stcoord;
	galiasskin_t *skin;
	skinframe_t *sframes;
#endif
	galiasbone_t *bones;
	galiasanimation_t *group;
	float *animmatrix, *basematrix;
	index_t *indexes;
	float vrad;
	int bonemap[MAX_BONES];
	char *e;
	size_t psasize;
	void *psabuffer;

	pskpnts_t *pnts = NULL;
	pskvtxw_t *vtxw = NULL;
	pskface_t *face = NULL;
	pskmatt_t *matt = NULL;
	pskboneinfo_t *boneinfo = NULL;
	pskrawweights_t *rawweights = NULL;
	unsigned int num_pnts, num_vtxw=0, num_face=0, num_matt = 0, num_boneinfo=0, num_rawweights=0;

	pskaniminfo_t *animinfo = NULL;
	pskanimkeys_t *animkeys = NULL;
	unsigned int num_animinfo=0, num_animkeys=0;

//#define PSK_GPU
#ifndef PSK_GPU
	unsigned int num_trans;
	galisskeletaltransforms_t *trans;
#else
	vecV_t *skel_xyz;
	vec3_t *skel_norm;
	byte_vec4_t *skel_idx;
	vec4_t *skel_weights;
#endif

	/*load the psk*/
	while (pos < fsize && !fail)
	{
		chunk = (pskchunk_t*)((char*)buffer + pos);
		chunk->version = LittleLong(chunk->version);
		chunk->recordsize = LittleLong(chunk->recordsize);
		chunk->numrecords = LittleLong(chunk->numrecords);

		pos += sizeof(*chunk);

		if (!strcmp("ACTRHEAD", chunk->id) && chunk->recordsize == 0 && chunk->numrecords == 0)
		{
		}
		else if (!strcmp("PNTS0000", chunk->id) && chunk->recordsize == sizeof(pskpnts_t))
		{
			num_pnts = chunk->numrecords;
			pnts = (pskpnts_t*)((char*)buffer + pos);
			pos += chunk->recordsize * chunk->numrecords;

			for (i = 0; i < num_pnts; i++)
			{
				pnts[i].origin[0] = LittleFloat(pnts[i].origin[0]);
				pnts[i].origin[1] = LittleFloat(pnts[i].origin[1]);
				pnts[i].origin[2] = LittleFloat(pnts[i].origin[2]);
			}
		}
		else if (!strcmp("VTXW0000", chunk->id) && chunk->recordsize == sizeof(pskvtxw_t))
		{
			num_vtxw = chunk->numrecords;
			vtxw = (pskvtxw_t*)((char*)buffer + pos);
			pos += chunk->recordsize * chunk->numrecords;

			for (i = 0; i < num_vtxw; i++)
			{
				vtxw[i].pntsindex = LittleShort(vtxw[i].pntsindex);
				vtxw[i].texcoord[0] = LittleFloat(vtxw[i].texcoord[0]);
				vtxw[i].texcoord[1] = LittleFloat(vtxw[i].texcoord[1]);
			}
		}
		else if (!strcmp("FACE0000", chunk->id) && chunk->recordsize == sizeof(pskface_t))
		{
			num_face = chunk->numrecords;
			face = (pskface_t*)((char*)buffer + pos);
			pos += chunk->recordsize * chunk->numrecords;

			for (i = 0; i < num_face; i++)
			{
				face[i].vtxwindex[0] = LittleShort(face[i].vtxwindex[0]);
				face[i].vtxwindex[1] = LittleShort(face[i].vtxwindex[1]);
				face[i].vtxwindex[2] = LittleShort(face[i].vtxwindex[2]);
			}
		}
		else if (!strcmp("MATT0000", chunk->id) && chunk->recordsize == sizeof(pskmatt_t))
		{
			num_matt = chunk->numrecords;
			matt = (pskmatt_t*)((char*)buffer + pos);
			pos += chunk->recordsize * chunk->numrecords;
		}
		else if (!strcmp("REFSKELT", chunk->id) && chunk->recordsize == sizeof(pskboneinfo_t))
		{
			num_boneinfo = chunk->numrecords;
			boneinfo = (pskboneinfo_t*)((char*)buffer + pos);
			pos += chunk->recordsize * chunk->numrecords;

			for (i = 0; i < num_boneinfo; i++)
			{
				boneinfo[i].parent = LittleLong(boneinfo[i].parent);
				boneinfo[i].basepose.origin[0] = LittleFloat(boneinfo[i].basepose.origin[0]);
				boneinfo[i].basepose.origin[1] = LittleFloat(boneinfo[i].basepose.origin[1]);
				boneinfo[i].basepose.origin[2] = LittleFloat(boneinfo[i].basepose.origin[2]);
				boneinfo[i].basepose.quat[0] = LittleFloat(boneinfo[i].basepose.quat[0]);
				boneinfo[i].basepose.quat[1] = LittleFloat(boneinfo[i].basepose.quat[1]);
				boneinfo[i].basepose.quat[2] = LittleFloat(boneinfo[i].basepose.quat[2]);
				boneinfo[i].basepose.quat[3] = LittleFloat(boneinfo[i].basepose.quat[3]);
				boneinfo[i].basepose.size[0] = LittleFloat(boneinfo[i].basepose.size[0]);
				boneinfo[i].basepose.size[1] = LittleFloat(boneinfo[i].basepose.size[1]);
				boneinfo[i].basepose.size[2] = LittleFloat(boneinfo[i].basepose.size[2]);

				/*not sure if this is needed, but mimic DP*/
				if (i)
				{
					boneinfo[i].basepose.quat[0] *= -1;
					boneinfo[i].basepose.quat[2] *= -1;
				}
				boneinfo[i].basepose.quat[1] *= -1;
			}
		}
		else if (!strcmp("RAWWEIGHTS", chunk->id) && chunk->recordsize == sizeof(pskrawweights_t))
		{
			num_rawweights = chunk->numrecords;
			rawweights = (pskrawweights_t*)((char*)buffer + pos);
			pos += chunk->recordsize * chunk->numrecords;

			for (i = 0; i < num_rawweights; i++)
			{
				rawweights[i].boneindex = LittleLong(rawweights[i].boneindex);
				rawweights[i].pntsindex = LittleLong(rawweights[i].pntsindex);
				rawweights[i].weight = LittleFloat(rawweights[i].weight);
			}
		}
		else
		{
			Con_Printf(CON_ERROR "%s has unsupported chunk %s of %i size with version %i.\n", mod->name, chunk->id, chunk->recordsize, chunk->version);
			fail = true;
		}
	}

	if (!num_matt)
		fail = true;

	if (!pnts || !vtxw || !face || !matt || !boneinfo || !rawweights)
		fail = true;

	/*attempt to load a psa file. don't die if we can't find one*/
	COM_StripExtension(mod->name, psaname, sizeof(psaname));
	Q_strncatz(psaname, ".psa", sizeof(psaname));
	buffer = NULL;//test
	psabuffer = COM_LoadFile(psaname, 5, &psasize);
	if (psabuffer)
	{
		pos = 0;
		while (pos < psasize && !fail)
		{
			chunk = (pskchunk_t*)((char*)psabuffer + pos);
			chunk->version = LittleLong(chunk->version);
			chunk->recordsize = LittleLong(chunk->recordsize);
			chunk->numrecords = LittleLong(chunk->numrecords);

			pos += sizeof(*chunk);

			if (!strcmp("ANIMHEAD", chunk->id) && chunk->recordsize == 0 && chunk->numrecords == 0)
			{
			}
			else if (!strcmp("BONENAMES", chunk->id) && chunk->recordsize == sizeof(pskboneinfo_t))
			{
				/*parsed purely to ensure that the bones match the main model*/
				pskboneinfo_t *animbones = (pskboneinfo_t*)((char*)psabuffer + pos);
				pos += chunk->recordsize * chunk->numrecords;
				if (num_boneinfo != chunk->numrecords)
				{
					fail = true;
					Con_Printf("PSK/PSA bone counts do not match\n");
				}
				else
				{
					for (i = 0; i < num_boneinfo; i++)
					{
						/*assumption: 1:1 mapping will be common*/
						if (!strcmp(boneinfo[i].name, animbones[i].name))
							bonemap[i] = i;
						else
						{
							/*non 1:1 mapping*/
							for (j = 0; j < chunk->numrecords; j++)
							{
								if (!strcmp(boneinfo[i].name, animbones[j].name))
								{
									bonemap[i] = j;
									break;
								}
							}
							if (j == chunk->numrecords)
							{
								fail = true;
								Con_Printf("PSK bone %s does not exist in PSA %s\n", boneinfo[i].name, basename);
								break;
							}
						}
					}
				}
			}
			else if (!strcmp("ANIMINFO", chunk->id) && chunk->recordsize == sizeof(pskaniminfo_t))
			{
				num_animinfo = chunk->numrecords;
				animinfo = (pskaniminfo_t*)((char*)psabuffer + pos);
				pos += chunk->recordsize * chunk->numrecords;

				for (i = 0; i < num_animinfo; i++)
				{
					animinfo[i].firstframe = LittleLong(animinfo[i].firstframe);
					animinfo[i].numframes = LittleLong(animinfo[i].numframes);
					animinfo[i].numbones = LittleLong(animinfo[i].numbones);
					animinfo[i].fps = LittleFloat(animinfo[i].fps);
					animinfo[i].playtime = LittleFloat(animinfo[i].playtime);
				}
			}
			else if (!strcmp("ANIMKEYS", chunk->id) && chunk->recordsize == sizeof(pskanimkeys_t))
			{
				num_animkeys = chunk->numrecords;
				animkeys = (pskanimkeys_t*)((char*)psabuffer + pos);
				pos += chunk->recordsize * chunk->numrecords;

				for (i = 0; i < num_animkeys; i++)
				{
					animkeys[i].origin[0] = LittleFloat(animkeys[i].origin[0]);
					animkeys[i].origin[1] = LittleFloat(animkeys[i].origin[1]);
					animkeys[i].origin[2] = LittleFloat(animkeys[i].origin[2]);
					animkeys[i].quat[0] = LittleFloat(animkeys[i].quat[0]);
					animkeys[i].quat[1] = LittleFloat(animkeys[i].quat[1]);
					animkeys[i].quat[2] = LittleFloat(animkeys[i].quat[2]);
					animkeys[i].quat[3] = LittleFloat(animkeys[i].quat[3]);

					/*not sure if this is needed, but mimic DP*/
					if (i%num_boneinfo)
					{
						animkeys[i].quat[0] *= -1;
						animkeys[i].quat[2] *= -1;
					}
					animkeys[i].quat[1] *= -1;
				}
			}
			else if (!strcmp("SCALEKEYS", chunk->id) && chunk->recordsize == 16)
			{
				pos += chunk->recordsize * chunk->numrecords;
			}
			else
			{
				Con_Printf(CON_ERROR "%s has unsupported chunk %s of %i size with version %i.\n", va("%s.psa", basename), chunk->id, chunk->recordsize, chunk->version);
				fail = true;
			}
		}
		if (fail)
		{
			animinfo = NULL;
			num_animinfo = 0;
			animkeys = NULL;
			num_animkeys = 0;
			fail = false;
		}
	}

	if (fail)
	{
		BZ_Free(psabuffer);
		return false;
	}

	gmdl = ZG_Malloc(&mod->memgroup, sizeof(*gmdl)*num_matt);

	/*bones!*/
	bones = ZG_Malloc(&mod->memgroup, sizeof(galiasbone_t) * num_boneinfo);
	for (i = 0; i < num_boneinfo; i++)
	{
		Q_strncpyz(bones[i].name, boneinfo[i].name, sizeof(bones[i].name));
		e = bones[i].name + strlen(bones[i].name);
		while(e > bones[i].name && e[-1] == ' ')
			*--e = 0;
		bones[i].parent = boneinfo[i].parent;
		if (i == 0 && bones[i].parent == 0)
			bones[i].parent = -1;
		else if (bones[i].parent >= i || bones[i].parent < -1)
		{
			Con_Printf("Invalid bones\n");
			break;
		}
	}

	basematrix = ZG_Malloc(&mod->memgroup, num_boneinfo*sizeof(float)*12);
	for (i = 0; i < num_boneinfo; i++)
	{
		float tmp[12];
		PSKGenMatrix(
			boneinfo[i].basepose.origin[0], boneinfo[i].basepose.origin[1], boneinfo[i].basepose.origin[2],
			boneinfo[i].basepose.quat[0],   boneinfo[i].basepose.quat[1],   boneinfo[i].basepose.quat[2], boneinfo[i].basepose.quat[3],
			tmp);
		if (bones[i].parent < 0)
			memcpy(basematrix + i*12, tmp, sizeof(float)*12);
		else
			R_ConcatTransforms((void*)(basematrix + bones[i].parent*12), (void*)tmp, (void*)(basematrix+i*12));
	}

	for (i = 0; i < num_boneinfo; i++)
	{
		Matrix3x4_Invert_Simple(basematrix+i*12, bones[i].inverse);
	}

	
#ifndef PSK_GPU
	/*expand the translations*/
	num_trans = 0;
	for (i = 0; i < num_vtxw; i++)
	{
		for (j = 0; j < num_rawweights; j++)
		{
			if (rawweights[j].pntsindex == vtxw[i].pntsindex)
			{
				num_trans++;
			}
		}
	}
	trans = ZG_Malloc(&mod->memgroup, sizeof(*trans)*num_trans);
	num_trans = 0;
	for (i = 0; i < num_vtxw; i++)
	{
//		first_trans = num_trans;
		for (j = 0; j < num_rawweights; j++)
		{
			if (rawweights[j].pntsindex == vtxw[i].pntsindex)
			{
				vec3_t tmp;
				trans[num_trans].vertexindex = i;
				trans[num_trans].boneindex = rawweights[j].boneindex;
				VectorTransform(pnts[rawweights[j].pntsindex].origin, (void*)bones[rawweights[j].boneindex].inverse, tmp);
				VectorScale(tmp, rawweights[j].weight, trans[num_trans].org);
				trans[num_trans].org[3] = rawweights[j].weight;
				num_trans++;
			}
		}
	}
#else
	skel_xyz = Hunk_Alloc(sizeof(*skel_xyz) * num_vtxw);
	skel_norm = Hunk_Alloc(sizeof(*skel_norm) * num_vtxw);
	skel_idx = Hunk_Alloc(sizeof(*skel_idx) * num_vtxw);
	skel_weights = Hunk_Alloc(sizeof(*skel_weights) * num_vtxw);
	for (i = 0; i < num_vtxw; i++)
	{
		float t;
		*(unsigned int*)skel_idx[i] = ~0;
		for (j = 0; j < num_rawweights; j++)
		{
			if (rawweights[j].pntsindex == vtxw[i].pntsindex)
			{
				int in, lin = -1;
				float liv = rawweights[j].weight;
				for (in = 0; in < 4; in++)
				{
					if (liv > skel_weights[i][in])
					{
						liv = skel_weights[i][in];
						lin = in;
						if (!liv)
							break;
					}
				}
				if (lin >= 0)
				{
					skel_idx[i][lin] = rawweights[j].boneindex;
					skel_weights[i][lin] = rawweights[j].weight;
				}
			}
		}
		t = 0;
		for (j = 0; j < 4; j++)
			t += skel_weights[i][j];
		if (t != 1)
			for (j = 0; j < 4; j++)
				skel_weights[i][j] *= 1/t;


		skel_xyz[i][0] = pnts[vtxw[i].pntsindex].origin[0];
		skel_xyz[i][1] = pnts[vtxw[i].pntsindex].origin[1];
		skel_xyz[i][2] = pnts[vtxw[i].pntsindex].origin[2];
	}
#endif

#ifndef SERVERONLY
	/*st coords, all share the same list*/
	stcoord = ZG_Malloc(&mod->memgroup, sizeof(vec2_t)*num_vtxw);
	for (i = 0; i < num_vtxw; i++)
	{
		stcoord[i][0] = vtxw[i].texcoord[0];
		stcoord[i][1] = vtxw[i].texcoord[1];
	}
#endif

	/*allocate faces in a single block, as we at least know an upper bound*/
	indexes = ZG_Malloc(&mod->memgroup, sizeof(index_t)*num_face*3);

	if (animinfo && animkeys)
	{
		int numgroups = 0;
		frameinfo_t *frameinfo = ParseFrameInfo(mod->name, &numgroups);
		if (numgroups)
		{
			/*externally supplied listing of frames. ignore all framegroups in the model and use only the pose info*/
			group = ZG_Malloc(&mod->memgroup, sizeof(galiasanimation_t)*numgroups + num_animkeys*sizeof(float)*12);
			animmatrix = (float*)(group+numgroups);
			for (j = 0; j < numgroups; j++)
			{
				/*bound check*/
				if (frameinfo[j].firstpose+frameinfo[j].posecount > num_animkeys)
					frameinfo[j].posecount = num_animkeys - frameinfo[j].firstpose;
				if (frameinfo[j].firstpose >= num_animkeys)
				{
					frameinfo[j].firstpose = 0;
					frameinfo[j].posecount = 1;
				}

				group[j].boneofs = animmatrix + 12*num_boneinfo*frameinfo[j].firstpose;
				group[j].numposes = frameinfo[j].posecount;
				if (*frameinfo[j].name)
					snprintf(group[j].name, sizeof(group[j].name), "%s", frameinfo[j].name);
				else
					snprintf(group[j].name, sizeof(group[j].name), "frame_%i", j);
				group[j].loop = frameinfo[j].loop;
				group[j].rate = frameinfo[j].fps;
				group[j].skeltype = SKEL_RELATIVE;
			}
			num_animinfo = numgroups;
		}
		else if (dpcompat_psa_ungroup.ival)
		{
			/*unpack each frame of each animation to be a separate framegroup*/
			unsigned int iframe;	/*individual frame count*/
			iframe = 0;
			for (i = 0; i < num_animinfo; i++)
				iframe += animinfo[i].numframes;
			group = ZG_Malloc(&mod->memgroup, sizeof(galiasanimation_t)*iframe + num_animkeys*sizeof(float)*12);
			animmatrix = (float*)(group+iframe);
			iframe = 0;
			for (j = 0; j < num_animinfo; j++)
			{
				for (i = 0; i < animinfo[j].numframes; i++)
				{
					group[iframe].boneofs = animmatrix + 12*num_boneinfo*(animinfo[j].firstframe+i);
					group[iframe].numposes = 1;
					snprintf(group[iframe].name, sizeof(group[iframe].name), "%s_%i", animinfo[j].name, i);
					group[iframe].loop = true;
					group[iframe].rate = animinfo[j].fps;
					group[iframe].skeltype = SKEL_RELATIVE;
					iframe++;
				}
			}
			num_animinfo = iframe;
		}
		else
		{
			/*keep each framegroup as a group*/
			group = ZG_Malloc(&mod->memgroup, sizeof(galiasanimation_t)*num_animinfo + num_animkeys*sizeof(float)*12);
			animmatrix = (float*)(group+num_animinfo);
			for (i = 0; i < num_animinfo; i++)
			{
				group[i].boneofs = animmatrix + 12*num_boneinfo*animinfo[i].firstframe;
				group[i].numposes = animinfo[i].numframes;
				Q_strncpyz(group[i].name, animinfo[i].name, sizeof(group[i].name));
				group[i].loop = true;
				group[i].rate = animinfo[i].fps;
				group[i].skeltype = SKEL_RELATIVE;
			}
		}
		for (j = 0; j < num_animkeys; j += num_boneinfo)
		{
			pskanimkeys_t *sb;
			for (i = 0; i < num_boneinfo; i++)
			{
				sb = &animkeys[j + bonemap[i]];
				PSKGenMatrix(
					sb->origin[0], sb->origin[1], sb->origin[2],
					sb->quat[0],   sb->quat[1],   sb->quat[2],   sb->quat[3],
					animmatrix + (j+i)*12);
			}
		}
	}
	else
	{
		num_animinfo = 1;
		/*build a base pose*/
		group = ZG_Malloc(&mod->memgroup, sizeof(galiasanimation_t) + num_boneinfo*sizeof(float)*12);
		animmatrix = basematrix;
		group->boneofs = animmatrix;
		group->numposes = 1;
		strcpy(group->name, "base");
		group->loop = true;
		group->rate = 10;
		group->skeltype = SKEL_ABSOLUTE;
	}

#ifndef SERVERONLY
	skin = ZG_Malloc(&mod->memgroup, num_matt * (sizeof(galiasskin_t) + sizeof(skinframe_t)));
	sframes = (skinframe_t*)(skin + num_matt);
	for (i = 0; i < num_matt; i++, skin++)
	{
		skin->frame = &sframes[i];
		skin->numframes = 1;
		skin->skinspeed = 10;
		Q_strncpyz(skin->name, matt[i].name, sizeof(skin->name));
		Q_strncpyz(sframes[i].shadername, matt[i].name, sizeof(sframes[i].shadername));
		sframes[i].shader = NULL;

		gmdl[i].ofsskins = skin;
		gmdl[i].numskins = 1;

		gmdl[i].ofs_st_array = stcoord;
		gmdl[i].numverts = num_vtxw;
#else
	for (i = 0; i < num_matt; i++)
	{
#endif

		gmdl[i].ofsanimations = group;
		gmdl[i].numanimations = num_animinfo;
		gmdl[i].baseframeofs = basematrix;

		gmdl[i].numindexes = 0;
		for (j = 0; j < num_face; j++)
		{
			if (face[j].mattindex%num_matt == i)
			{
				indexes[gmdl[i].numindexes+0] = face[j].vtxwindex[0];
				indexes[gmdl[i].numindexes+1] = face[j].vtxwindex[1];
				indexes[gmdl[i].numindexes+2] = face[j].vtxwindex[2];
				gmdl[i].numindexes += 3;
			}
		}
		gmdl[i].ofs_indexes = indexes;
		indexes += gmdl[i].numindexes;

		gmdl[i].ofsbones = bones;
		gmdl[i].numbones = num_boneinfo;

#ifndef PSK_GPU
		gmdl[i].ofsswtransforms = trans;
		gmdl[i].numswtransforms = num_trans;
#else
		gmdl[i].ofs_skel_idx = skel_idx;
		gmdl[i].ofs_skel_weight = skel_weights;
		gmdl[i].ofs_skel_xyz = skel_xyz;
		gmdl[i].ofs_skel_norm = skel_norm;
#endif

		gmdl[i].shares_verts = 0;
		gmdl[i].shares_bones = 0;
		gmdl[i].nextsurf = (i != num_matt-1)?&gmdl[i+1]:NULL;
	}

	BZ_Free(psabuffer);
	if (fail)
	{
		return false;
	}


	vrad = Alias_CalculateSkeletalNormals(gmdl);

	mod->mins[0] = mod->mins[1] = mod->mins[2] = -vrad;
	mod->maxs[0] = mod->maxs[1] = mod->maxs[2] = vrad;
	mod->radius = vrad;

	mod->flags = Mod_ReadFlagsFromMD1(mod->name, 0);	//file replacement - inherit flags from any defunc mdl files.

	Mod_ClampModelSize(mod);


	mod->meshinfo = gmdl;
	mod->type = mod_alias;
	mod->funcs.NativeTrace = Mod_Trace;
	return true;
}

#endif





//////////////////////////////////////////////////////////////
//dpm
#ifdef DPMMODELS

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

qboolean QDECL Mod_LoadDarkPlacesModel(model_t *mod, void *buffer, size_t fsize)
{
#ifndef SERVERONLY
	galiasskin_t *skin;
	skinframe_t *skinframe;
	int skinfiles;
	float *inst;
	float *outst;
#endif

	int i, j, k;

	dpmheader_t *header;
	galiasinfo_t *root, *m;
	dpmmesh_t *mesh;
	dpmvertex_t *vert;
	dpmbonevert_t *bonevert;

	galisskeletaltransforms_t *transforms;

	galiasbone_t *outbone;
	dpmbone_t *inbone;

	float *outposedata;
	galiasanimation_t *outgroups;
	float *inposedata;
	dpmframe_t *inframes;

	unsigned int *index;	index_t *outdex;	// groan...

	int numtransforms;

	header = buffer;

	if (memcmp(header->id, "DARKPLACESMODEL\0", 16))
	{
		Con_Printf(CON_ERROR "Mod_LoadDarkPlacesModel: %s, doesn't appear to be a darkplaces model!\n", mod->name);
		return false;
	}

	if (BigLong(header->type) != 2)
	{
		Con_Printf(CON_ERROR "Mod_LoadDarkPlacesModel: %s, only type 2 is supported\n", mod->name);
		return false;
	}

	for (i = 0; i < sizeof(dpmheader_t)/4; i++)
		((int*)header)[i] = BigLong(((int*)header)[i]);

	if (!header->num_bones)
	{
		Con_Printf(CON_ERROR "Mod_LoadDarkPlacesModel: %s, no bones\n", mod->name);
		return false;
	}
	if (!header->num_frames)
	{
		Con_Printf(CON_ERROR "Mod_LoadDarkPlacesModel: %s, no frames\n", mod->name);
		return false;
	}
	if (!header->num_meshs)
	{
		Con_Printf(CON_ERROR "Mod_LoadDarkPlacesModel: %s, no surfaces\n", mod->name);
		return false;
	}


	VectorCopy(header->mins, mod->mins);
	VectorCopy(header->maxs, mod->maxs);

	root = ZG_Malloc(&mod->memgroup, sizeof(galiasinfo_t)*header->num_meshs);

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
		m->shares_verts = i;
		m->shares_bones = 0;
#ifdef SERVERONLY
		transforms = ZG_Malloc(&mod->memgroup, numtransforms*sizeof(galisskeletaltransforms_t) + mesh->num_tris*3*sizeof(index_t));
#else
		outst = ZG_Malloc(&mod->memgroup, mesh->num_verts*sizeof(vec2_t) + numtransforms*sizeof(galisskeletaltransforms_t) + mesh->num_tris*3*sizeof(index_t));
		m->ofs_st_array = (vec2_t*)outst;
		m->numverts = mesh->num_verts;
		inst = (float*)((char*)buffer + mesh->ofs_texcoords);
		for (j = 0; j < mesh->num_verts; j++, outst+=2, inst+=2)
		{
			outst[0] = BigFloat(inst[0]);
			outst[1] = BigFloat(inst[1]);
		}

		transforms = (galisskeletaltransforms_t*)outst;
#endif

		//build the transform list.
		m->ofsswtransforms = transforms;
		m->numswtransforms = numtransforms;
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
#ifndef SERVERONLY
				transforms->normal[0] = BigFloat(bonevert->normal[0]);
				transforms->normal[1] = BigFloat(bonevert->normal[1]);
				transforms->normal[2] = BigFloat(bonevert->normal[2]);
#endif
			}
			//FIXME: transform these verts into the base pose, prioritise the weights, clamp to 4 weights, and then generate per-vertex arrays that are simple enough for the gpu to be happy.
			vert = (dpmvertex_t*)bonevert;
		}

		index = (unsigned int*)((char*)buffer + mesh->ofs_indices);
		outdex = (index_t *)transforms;
		m->ofs_indexes = outdex;
		m->numindexes = mesh->num_tris*3;
		for (j = 0; j < mesh->num_tris; j++, index += 3, outdex += 3)
		{
			outdex[0] = BigLong(index[2]);
			outdex[1] = BigLong(index[1]);
			outdex[2] = BigLong(index[0]);
		}
	}

	outbone = ZG_Malloc(&mod->memgroup, sizeof(galiasbone_t)*header->num_bones);
	inbone = (dpmbone_t*)((char*)buffer + header->ofs_bones);
	for (i = 0; i < header->num_bones; i++)
	{
		outbone[i].parent = BigLong(inbone[i].parent);
		if (outbone[i].parent >= i || outbone[i].parent < -1)
		{
			Con_Printf(CON_ERROR "Mod_LoadDarkPlacesModel: bad bone index in %s\n", mod->name);
			return false;
		}

		Q_strncpyz(outbone[i].name, inbone[i].name, sizeof(outbone[i].name));
		//throw away the flags.
	}

	outgroups = ZG_Malloc(&mod->memgroup, sizeof(galiasanimation_t)*header->num_frames + sizeof(float)*header->num_frames*header->num_bones*12);
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
		outgroups[i].skeltype = SKEL_RELATIVE;
		outgroups[i].boneofs = outposedata;

		inposedata = (float*)((char*)buffer + inframes[i].ofs_bonepositions);
		for (j = 0; j < header->num_bones*12; j++)
			*outposedata++ = BigFloat(*inposedata++);
	}

#ifndef SERVERONLY
	skinfiles = Mod_CountSkinFiles(mod->name);
	if (skinfiles < 1)
		skinfiles = 1;
#endif

	mesh = (dpmmesh_t*)((char*)buffer + header->ofs_meshs);
	for (i = 0; i < header->num_meshs; i++, mesh++)
	{
		m = &root[i];
		if (i < header->num_meshs-1)
			m->nextsurf = &root[i+1];
		m->shares_bones = 0;

		m->ofsbones = outbone;
		m->numbones = header->num_bones;

		m->numanimations = header->num_frames;
		m->ofsanimations = outgroups;


		Q_strncpyz(m->surfacename, mesh->shadername, sizeof(m->surfacename));

#ifdef SERVERONLY
		m->numskins = 1;
#else
		m->numskins = skinfiles;

		skin = ZG_Malloc(&mod->memgroup, (sizeof(galiasskin_t)+sizeof(skinframe_t))*skinfiles);
		skinframe = (skinframe_t*)(skin+skinfiles);
		for (j = 0; j < skinfiles; j++, skinframe++)
		{
			skin[j].numframes = 1;	//non-sequenced skins.
			skin[j].frame = skinframe;
			skin[j].skinwidth = 1;
			skin[j].skinheight = 1;
			skin[j].skinspeed = 10; /*something to avoid div by 0*/

			if (!j)
			{
				Q_strncpyz(skin[j].name, mesh->shadername, sizeof(skin[j].name));
				Q_strncpyz(skinframe->shadername, mesh->shadername, sizeof(skin[j].name));
			}
			else
			{
				Q_strncpyz(skin[j].name, "", sizeof(skin[j].name));
				Q_strncpyz(skinframe->shadername, "", sizeof(skin[j].name));
			}

		}

		m->ofsskins = skin;
#endif
	}


	Alias_CalculateSkeletalNormals(root);

	mod->flags = Mod_ReadFlagsFromMD1(mod->name, 0);	//file replacement - inherit flags from any defunc mdl files.

	Mod_ClampModelSize(mod);

	mod->meshinfo = root;
	mod->type = mod_alias;
	mod->funcs.NativeTrace = Mod_Trace;

	return true;
}
#endif	//DPMMODELS


#ifdef INTERQUAKEMODELS
#define IQM_MAGIC "INTERQUAKEMODEL"
#define IQM_VERSION1 1
#define IQM_VERSION2 2

struct iqmheader
{
	char magic[16];
	unsigned int version;
	unsigned int filesize;
	unsigned int flags;
	unsigned int num_text, ofs_text;
	unsigned int num_meshes, ofs_meshes;
	unsigned int num_vertexarrays, num_vertexes, ofs_vertexarrays;
	unsigned int num_triangles, ofs_triangles, ofs_adjacency;
	unsigned int num_joints, ofs_joints;
	unsigned int num_poses, ofs_poses;
	unsigned int num_anims, ofs_anims;
	unsigned int num_frames, num_framechannels, ofs_frames, ofs_bounds;
	unsigned int num_comment, ofs_comment;
	unsigned int num_extensions, ofs_extensions;
};

struct iqmmesh
{
	unsigned int name;
	unsigned int material;
	unsigned int first_vertex, num_vertexes;
	unsigned int first_triangle, num_triangles;
};

enum
{
	IQM_POSITION     = 0,
	IQM_TEXCOORD     = 1,
	IQM_NORMAL       = 2,
	IQM_TANGENT      = 3,
	IQM_BLENDINDEXES = 4,
	IQM_BLENDWEIGHTS = 5,
	IQM_COLOR        = 6,
	IQM_CUSTOM       = 0x10
};

enum
{
	IQM_BYTE   = 0,
	IQM_UBYTE  = 1,
	IQM_SHORT  = 2,
	IQM_USHORT = 3,
	IQM_INT    = 4,
	IQM_UINT   = 5,
	IQM_HALF   = 6,
	IQM_FLOAT  = 7,
	IQM_DOUBLE = 8,
};

struct iqmtriangle
{
	unsigned int vertex[3];
};

struct iqmjoint1
{
	unsigned int name;
	int parent;
	float translate[3], rotate[3], scale[3];
};
struct iqmjoint2
{
	unsigned int name;
	int parent;
	float translate[3], rotate[4], scale[3];
};

struct iqmpose1
{
	int parent;
	unsigned int mask;
	float channeloffset[9];
	float channelscale[9];
};
struct iqmpose2
{
	int parent;
	unsigned int mask;
	float channeloffset[10];
	float channelscale[10];
};

struct iqmanim
{
	unsigned int name;
	unsigned int first_frame, num_frames;
	float framerate;
	unsigned int flags;
};

enum
{
	IQM_LOOP = 1<<0
};

struct iqmvertexarray
{
	unsigned int type;
	unsigned int flags;
	unsigned int format;
	unsigned int size;
	unsigned int offset;
};

struct iqmbounds
{
	float bbmin[3], bbmax[3];
	float xyradius, radius;
};

/*
galisskeletaltransforms_t *IQM_ImportTransforms(int *resultcount, int inverts, float *vpos, float *tcoord, float *vnorm, float *vtang, unsigned char *vbone, unsigned char *vweight)
{
	galisskeletaltransforms_t *t, *r;
	unsigned int num_t = 0;
	unsigned int v, j;
	for (v = 0; v < inverts*4; v++)
	{
		if (vweight[v])
			num_t++;
	}
	t = r = Hunk_Alloc(sizeof(*r)*num_t);
	for (v = 0; v < inverts; v++)
	{
		for (j = 0; j < 4; j++)
		{
			if (vweight[(v<<2)+j])
			{
				t->boneindex = vbone[(v<<2)+j];
				t->vertexindex = v;
				VectorScale(vpos, vweight[(v<<2)+j]/255.0, t->org);
				VectorScale(vnorm, vweight[(v<<2)+j]/255.0, t->normal);
				t++;
			}
		}
	}
	return r;
}
*/

static qboolean IQM_ImportArray4B(qbyte *base, struct iqmvertexarray *src, byte_vec4_t *out, size_t count, unsigned int maxval)
{
	size_t i;
	unsigned int j;
	unsigned int sz = LittleLong(src->size);
	unsigned int fmt = LittleLong(src->format);
	unsigned int offset = LittleLong(src->offset);
	qboolean invalid = false;
	maxval = min(256,maxval);	//output is bytes.
	if (!offset)
	{
		sz = 0;
		fmt = IQM_UBYTE;
	}
	switch(fmt)
	{
	default:
		sz = 0;
		invalid = true;
		break;
	case IQM_BYTE:	//FIXME: should be signed, but this makes no sense for our uses
	case IQM_UBYTE:
		{
			qbyte *in = (qbyte*)(base+offset);
			/*if (sz == 4)
				memcpy(out, in, count * sizeof(*out));	//the fast path.
			else*/ for (i = 0; i < count; i++)
			{
				for (j = 0; j < 4 && j < sz; j++)
				{
					if (in[i*sz+j] >= maxval)
					{
						out[i][j] = 0;
						invalid = true;
					}
					else
						out[i][j] = in[i*sz+j];
				}
			}
		}
		break;
	case IQM_SHORT://FIXME: should be signed, but this makes no sense for our uses
	case IQM_USHORT:
		{
			unsigned short *in = (unsigned short*)(base+offset);
			for (i = 0; i < count; i++)
			{
				for (j = 0; j < 4 && j < sz; j++)
				{
					if (in[i*sz+j] >= maxval)
					{
						out[i][j] = 0;
						invalid = true;
					}
					else
						out[i][j] = in[i*sz+j];
				}
			}
		}
		break;
	case IQM_INT://FIXME: should be signed, but this makes no sense for our uses
	case IQM_UINT:
		{
			unsigned int *in = (unsigned int*)(base+offset);
			for (i = 0; i < count; i++)
			{
				for (j = 0; j < 4 && j < sz; j++)
				{
					if (in[i*sz+j] >= maxval)
					{
						out[i][j] = 0;
						invalid = true;
					}
					else
						out[i][j] = in[i*sz+j];
				}
			}
		}
		break;
	//float types don't really make sense
	}

	//if there were not enough elements, pad it.
	if (sz < 4)
	{
		for (i = 0; i < count; i++)
		{
			for (j = sz; j < 4; j++)
				out[i][j] = 0;
		}
	}

	return !invalid;
}
static void IQM_ImportArrayF(qbyte *base, struct iqmvertexarray *src, float *out, size_t e, size_t count, float *def)
{
	size_t i;
	unsigned int j;
	unsigned int sz = LittleLong(src->size);
	unsigned int fmt = LittleLong(src->format);
	unsigned int offset = LittleLong(src->offset);
	if (!offset)
	{
		sz = 0;
		fmt = IQM_FLOAT;
	}
	switch(fmt)
	{
	default:
		sz = 0;
		break;
	case IQM_BYTE:	//FIXME: should be signed
		{
			char *in = (qbyte*)(base+offset);
			for (i = 0; i < count; i++)
			{
				for (j = 0; j < e && j < sz; j++)
					out[i*e+j] = in[i*sz+j] * (1.0/127);
			}
		}
		break;
	case IQM_UBYTE:
		{
			qbyte *in = (qbyte*)(base+offset);
			for (i = 0; i < count; i++)
			{
				for (j = 0; j < e && j < sz; j++)
					out[i*e+j] = in[i*sz+j] * (1.0/255);
			}
		}
		break;
	case IQM_SHORT:
		{
			short *in = (short*)(base+offset);
			for (i = 0; i < count; i++)
			{
				for (j = 0; j < e && j < sz; j++)
					out[i*e+j] = in[i*sz+j] * (1.0/32767);
			}
		}
		break;
	case IQM_USHORT:
		{
			unsigned short *in = (unsigned short*)(base+offset);
			for (i = 0; i < count; i++)
			{
				for (j = 0; j < e && j < sz; j++)
					out[i*e+j] = in[i*sz+j] * (1.0/65535);
			}
		}
		break;
	case IQM_INT://FIXME: should be signed
	case IQM_UINT:
		{
			unsigned int *in = (unsigned int*)(base+offset);
			for (i = 0; i < count; i++)
			{
				for (j = 0; j < e && j < sz; j++)
					out[i*e+j] = in[i*sz+j];
			}
		}
		break;

	/*case IQM_HALF:
		{
			__fp16 *in = (qbyte*)(base+offset);
			for (i = 0; i < count; i++)
			{
				for (j = 0; j < e && j < sz; j++)
					out[i*e+j] = in[i*sz+j];
			}
		}
		break;*/
	case IQM_FLOAT:
		{
			float *in = (float*)(base+offset);
			if (e == sz)
				memcpy(out, in, e * sizeof(float) * count);
			else for (i = 0; i < count; i++)
			{
				for (j = 0; j < e && j < sz; j++)
					out[i*e+j] = in[i*sz+j];
			}
		}
		break;
	case IQM_DOUBLE:
		{
			double *in = (double*)(base+offset);
			for (i = 0; i < count; i++)
			{
				for (j = 0; j < e && j < sz; j++)
					out[i*e+j] = in[i*sz+j];
			}
		}
		break;
	}

	//if there were not enough elements, pad it.
	if (sz < e)
	{
		for (i = 0; i < count; i++)
		{
			for (j = sz; j < e; j++)
				out[i*e+j] = def[j];
		}
	}
}

galiasinfo_t *Mod_ParseIQMMeshModel(model_t *mod, char *buffer, size_t fsize)
{
	struct iqmheader *h = (struct iqmheader *)buffer;
	struct iqmmesh *mesh;
	struct iqmvertexarray *varray;
	struct iqmtriangle *tris;
	struct iqmanim *anim;
	unsigned int i, j, t, nt;

	char *strings;

	float *vtang = NULL;
	struct iqmvertexarray vpos = {0}, vnorm = {0}, vtcoord = {0}, vbone = {0}, vweight = {0}, vrgba = {0};
	unsigned int type, fmt, size, offset;
	unsigned short *framedata;
	vec4_t defaultcolour = {1,1,1,1};
	vec4_t defaultweight = {0,0,0,0};
	vec4_t defaultvert = {0,0,0,1};

	struct iqmbounds	*inbounds;

	int memsize;
	qbyte *obase=NULL;
	vecV_t *opos=NULL;
	vec3_t *onorm1=NULL, *onorm2=NULL, *onorm3=NULL;
	vec4_t *oweight=NULL;
	byte_vec4_t *oindex=NULL;
	float *opose=NULL,*oposebase=NULL;
	vec2_t *otcoords = NULL;
	vec4_t *orgbaf = NULL;


	galiasinfo_t *gai=NULL;
#ifndef SERVERONLY
	galiasskin_t *skin=NULL;
	skinframe_t *skinframe=NULL;
	int skinfiles;
#endif
	galiasanimation_t *fgroup=NULL;
	galiasbone_t *bones = NULL;
	index_t *idx;
	float basepose[12 * MAX_BONES];
	qboolean noweights;
	frameinfo_t *framegroups;
	int numgroups;

	if (memcmp(h->magic, IQM_MAGIC, sizeof(h->magic)))
	{
		Con_Printf("%s: format not recognised\n", mod->name);
		return NULL;
	}
	if (h->version != IQM_VERSION1 && h->version != IQM_VERSION2)
	{
		Con_Printf("%s: unsupported IQM version\n", mod->name);
		return NULL;
	}
	if (h->filesize != fsize)
	{
		Con_Printf("%s: size (%u != %"PRIuSIZE")\n", mod->name, h->filesize, fsize);
		return NULL;
	}

	varray = (struct iqmvertexarray*)(buffer + h->ofs_vertexarrays);
	for (i = 0; i < h->num_vertexarrays; i++)
	{
		type = LittleLong(varray[i].type);
		fmt = LittleLong(varray[i].format);
		size = LittleLong(varray[i].size);
		offset = LittleLong(varray[i].offset);
		if (type == IQM_POSITION)
			vpos = varray[i];
		else if (type == IQM_TEXCOORD)
			vtcoord = varray[i];
		else if (type == IQM_NORMAL)
			vnorm = varray[i];
		else if (type == IQM_TANGENT && fmt == IQM_FLOAT && size == 4) /*yup, 4, extra is side, for the bitangent*/
			vtang = (float*)(buffer + offset);
		else if (type == IQM_BLENDINDEXES)
			vbone = varray[i];
		else if (type == IQM_BLENDWEIGHTS)
			vweight = varray[i];
		else if (type == IQM_COLOR)
			vrgba = varray[i];
		else
			Con_Printf("Unrecognised iqm info (type=%i, fmt=%i, size=%i)\n", type, fmt, size);
	}

	if (!h->num_meshes)
	{
		Con_Printf("%s: IQM has no meshes\n", mod->name);
		return NULL;
	}

	//a mesh must contain vertex coords or its not much of a mesh.
	//we also require texcoords because we can.
	//we don't require normals
	//we don't require weights, but such models won't animate.
	if (h->num_vertexes > 0 && (!vpos.offset || !vtcoord.offset))
	{
		Con_Printf("%s is missing vertex array data\n", mod->name);
		return NULL;
	}
	noweights = !vbone.offset || !vweight.offset;
	if (noweights)
	{
		if (h->num_frames || h->num_anims || h->num_joints)
		{
			Con_Printf("%s: animated IQM lacks bone weights\n", mod->name);
			return NULL;
		}
	}

	if (h->num_joints > MAX_BONES)
	{
		Con_Printf("%s: IQM has %u joints, max supported is %u.\n", mod->name, h->num_joints, MAX_BONES);
		return NULL;
	}

	strings = buffer + h->ofs_text;

	/*try to completely disregard all the info the creator carefully added to their model...*/
	numgroups = 0;
	framegroups = NULL;
	if (!numgroups)
		framegroups = ParseFrameInfo(mod->name, &numgroups);
	if (!numgroups && h->num_anims)
	{
		/*use the model's framegroups*/
		numgroups = h->num_anims;
		framegroups = malloc(sizeof(*framegroups)*numgroups);

		anim = (struct iqmanim*)(buffer + h->ofs_anims);
		for (i = 0; i < numgroups; i++)
		{
			framegroups[i].firstpose = LittleLong(anim[i].first_frame);
			framegroups[i].posecount = LittleLong(anim[i].num_frames);
			framegroups[i].fps = LittleFloat(anim[i].framerate);
			framegroups[i].loop = !!(LittleLong(anim[i].flags) & IQM_LOOP);
			Q_strncpyz(framegroups[i].name, strings+anim[i].name, sizeof(fgroup[i].name));
		}
	}
	if (!numgroups)
	{	/*base frame only*/
		numgroups = 1;
		framegroups = malloc(sizeof(*framegroups));
		framegroups->firstpose = -1;
		framegroups->posecount = 1;
		framegroups->fps = 10;
		framegroups->loop = 1;
		strcpy(framegroups->name, "base");
	}

	mesh = (struct iqmmesh*)(buffer + h->ofs_meshes);

#ifndef SERVERONLY
	skinfiles = Mod_CountSkinFiles(mod->name);
	if (skinfiles < 1)
		skinfiles = 1;	//iqms have 1 skin and one skin only and always. make sure its loaded.
#endif

	/*allocate a nice big block of memory and figure out where stuff is*/
	/*run through twice, so things are consistant*/
#define dalloc(o,count) do{o = (void*)(obase+memsize); memsize += sizeof(*o)*(count);}while(0)
	for (i = 0, memsize = 0, obase = NULL; i < 2; i++)
	{
		if (i)
			obase = ZG_Malloc(&mod->memgroup, memsize);
		memsize = 0;
		dalloc(gai, h->num_meshes);
		dalloc(bones, h->num_joints);
		dalloc(opos, h->num_vertexes);
		dalloc(onorm1, h->num_vertexes);
		dalloc(onorm2, h->num_vertexes);
		dalloc(onorm3, h->num_vertexes);
		if (!noweights)
		{
			dalloc(oindex, h->num_vertexes);
			dalloc(oweight, h->num_vertexes);
		}
		else
		{
			oindex = NULL;
			oweight = NULL;
		}
#ifndef SERVERONLY
		if (vtcoord.offset)
			dalloc(otcoords, h->num_vertexes);
		else
			otcoords = NULL;
		if (vrgba.offset)
			dalloc(orgbaf, h->num_vertexes);
		else
			orgbaf = NULL;
		dalloc(skin, h->num_meshes*skinfiles);
		dalloc(skinframe, h->num_meshes*skinfiles);
#endif
		dalloc(fgroup, numgroups);
		dalloc(oposebase, 12*h->num_joints);
		dalloc(opose, 12*(h->num_poses*h->num_frames));
	}
#undef dalloc

//no code to load animations or bones
	framedata = (unsigned short*)(buffer + h->ofs_frames);

	/*Version 1 supports only normalized quaternions, version 2 uses complete quaternions. Some struct sizes change for this, otherwise functionally identical.*/
	if (h->version == IQM_VERSION1)
	{
		struct iqmpose1 *p, *ipose = (struct iqmpose1*)(buffer + h->ofs_poses);
		struct iqmjoint1 *ijoint = (struct iqmjoint1*)(buffer + h->ofs_joints);
		vec3_t pos;
		vec4_t quat;
		vec3_t scale;
		float mat[12];

		//joint info (mesh)
		for (i = 0; i < h->num_joints; i++)
		{
			Q_strncpyz(bones[i].name, strings+ijoint[i].name, sizeof(bones[i].name));
			bones[i].parent = ijoint[i].parent;

			GenMatrixPosQuat3Scale(ijoint[i].translate, ijoint[i].rotate, ijoint[i].scale, mat);

			if (ijoint[i].parent >= 0)
				Matrix3x4_Multiply(mat, &basepose[ijoint[i].parent*12], &basepose[i*12]);
			else
				memcpy(&basepose[i*12], mat, sizeof(mat));
			Matrix3x4_Invert_Simple(&basepose[i*12], bones[i].inverse);
		}

		//pose info (anim)
		for (i = 0; i < h->num_frames; i++)
		{
			for (j = 0, p = ipose; j < h->num_poses; j++, p++)
			{
				pos[0]   = p->channeloffset[0]; if (p->mask &   1) pos[0]   += *framedata++ * p->channelscale[0];
				pos[1]   = p->channeloffset[1]; if (p->mask &   2) pos[1]   += *framedata++ * p->channelscale[1];
				pos[2]   = p->channeloffset[2]; if (p->mask &   4) pos[2]   += *framedata++ * p->channelscale[2];
				quat[0]  = p->channeloffset[3]; if (p->mask &   8) quat[0]  += *framedata++ * p->channelscale[3];
				quat[1]  = p->channeloffset[4]; if (p->mask &  16) quat[1]  += *framedata++ * p->channelscale[4];
				quat[2]  = p->channeloffset[5]; if (p->mask &  32) quat[2]  += *framedata++ * p->channelscale[5];
				scale[0] = p->channeloffset[6]; if (p->mask &  64) scale[0] += *framedata++ * p->channelscale[6];
				scale[1] = p->channeloffset[7]; if (p->mask & 128) scale[1] += *framedata++ * p->channelscale[7];
				scale[2] = p->channeloffset[8]; if (p->mask & 256) scale[2] += *framedata++ * p->channelscale[8];

				quat[3] = -sqrt(max(1.0 - pow(VectorLength(quat),2), 0.0));

				GenMatrixPosQuat3Scale(pos, quat, scale, &opose[(i*h->num_poses+j)*12]);
			}
		}
	}
	else
	{
		struct iqmpose2 *p, *ipose = (struct iqmpose2*)(buffer + h->ofs_poses);
		struct iqmjoint2 *ijoint = (struct iqmjoint2*)(buffer + h->ofs_joints);
		vec3_t pos;
		vec4_t quat;
		vec3_t scale;
		float mat[12];

		//joint info (mesh)
		for (i = 0; i < h->num_joints; i++)
		{
			Q_strncpyz(bones[i].name, strings+ijoint[i].name, sizeof(bones[i].name));
			bones[i].parent = ijoint[i].parent;

			GenMatrixPosQuat4Scale(ijoint[i].translate, ijoint[i].rotate, ijoint[i].scale, mat);

			if (ijoint[i].parent >= 0)
				Matrix3x4_Multiply(mat, &basepose[ijoint[i].parent*12], &basepose[i*12]);
			else
				memcpy(&basepose[i*12], mat, sizeof(mat));
			Matrix3x4_Invert_Simple(&basepose[i*12], bones[i].inverse);
		}

		//pose info (anim)
		for (i = 0; i < h->num_frames; i++)
		{
			for (j = 0, p = ipose; j < h->num_poses; j++, p++)
			{
				pos[0]   = p->channeloffset[0]; if (p->mask &   1) pos[0]   += *framedata++ * p->channelscale[0];
				pos[1]   = p->channeloffset[1]; if (p->mask &   2) pos[1]   += *framedata++ * p->channelscale[1];
				pos[2]   = p->channeloffset[2]; if (p->mask &   4) pos[2]   += *framedata++ * p->channelscale[2];
				quat[0]  = p->channeloffset[3]; if (p->mask &   8) quat[0]  += *framedata++ * p->channelscale[3];
				quat[1]  = p->channeloffset[4]; if (p->mask &  16) quat[1]  += *framedata++ * p->channelscale[4];
				quat[2]  = p->channeloffset[5]; if (p->mask &  32) quat[2]  += *framedata++ * p->channelscale[5];
				quat[3]  = p->channeloffset[6]; if (p->mask &  64) quat[3]  += *framedata++ * p->channelscale[6];
				scale[0] = p->channeloffset[7]; if (p->mask & 128) scale[0] += *framedata++ * p->channelscale[7];
				scale[1] = p->channeloffset[8]; if (p->mask & 256) scale[1] += *framedata++ * p->channelscale[8];
				scale[2] = p->channeloffset[9]; if (p->mask & 512) scale[2] += *framedata++ * p->channelscale[9];

				GenMatrixPosQuat4Scale(pos, quat, scale, &opose[(i*h->num_poses+j)*12]);
			}
		}
	}
	//basepose
	memcpy(oposebase, basepose, sizeof(float)*12 * h->num_joints);

	//now generate the animations.
	for (i = 0; i < numgroups; i++)
	{
		if (framegroups[i].firstpose + framegroups[i].posecount > h->num_frames)
			framegroups[i].posecount = h->num_frames - framegroups[i].firstpose;
		if (framegroups[i].firstpose >= h->num_frames)
		{
			//invalid/basepose.
			fgroup[i].skeltype = SKEL_ABSOLUTE;
			fgroup[i].boneofs = oposebase;
			fgroup[i].numposes = 1;
		}
		else
		{
			fgroup[i].skeltype = SKEL_RELATIVE;
			fgroup[i].boneofs = opose + framegroups[i].firstpose*12*h->num_poses;
			fgroup[i].numposes = framegroups[i].posecount;
		}

		fgroup[i].loop = framegroups[i].loop;
		fgroup[i].rate = framegroups[i].fps;
		Q_strncpyz(fgroup[i].name, framegroups[i].name, sizeof(fgroup[i].name));
	
		if (fgroup[i].rate <= 0)
			fgroup[i].rate = 10;
	}
	free(framegroups);

	//determine the bounds
	inbounds = (struct iqmbounds*)(buffer + h->ofs_bounds);
	for (i = 0; i < h->num_frames; i++)
	{
		vec3_t mins, maxs;
		mins[0] = LittleFloat(inbounds[i].bbmin[0]);
		mins[1] = LittleFloat(inbounds[i].bbmin[1]);
		mins[2] = LittleFloat(inbounds[i].bbmin[2]);
		AddPointToBounds(mins, mod->mins, mod->maxs);
		maxs[0] = LittleFloat(inbounds[i].bbmax[0]);
		maxs[1] = LittleFloat(inbounds[i].bbmax[1]);
		maxs[2] = LittleFloat(inbounds[i].bbmax[2]);
		AddPointToBounds(maxs, mod->mins, mod->maxs);
	}

	for (i = 0; i < h->num_meshes; i++)
	{
		gai[i].nextsurf = (i == (h->num_meshes-1))?NULL:&gai[i+1];

		/*animation info*/
		gai[i].shares_bones = 0;
		gai[i].numbones = h->num_joints;
		gai[i].ofsbones = bones;
		gai[i].numanimations = numgroups;
		gai[i].ofsanimations = fgroup;
		
		offset = LittleLong(mesh[i].first_vertex);

		Q_strncpyz(gai[i].surfacename, strings+mesh[i].name, sizeof(gai[i].surfacename));

#ifndef SERVERONLY
		/*colours*/
		gai[i].ofs_rgbaf = orgbaf?(orgbaf+offset):NULL;
		gai[i].ofs_rgbaub = NULL;
		/*texture coords*/
		gai[i].ofs_st_array = (otcoords+offset);
		/*skins*/
		gai[i].numskins = skinfiles;
		gai[i].ofsskins = skin;

		for (j = 0; j < skinfiles; j++)
		{
			skin->skinwidth = 1;
			skin->skinheight = 1;
			skin->skinspeed = 10; /*something to avoid div by 0*/
			skin->numframes = 1;	//non-sequenced skins.
			skin->frame = skinframe;
			skin++;

			Q_strncpyz(skinframe->shadername, strings+mesh[i].material, sizeof(skinframe->shadername));
			skinframe++;
		}
#endif

		nt = LittleLong(mesh[i].num_triangles);
		tris = (struct iqmtriangle*)(buffer + LittleLong(h->ofs_triangles));
		tris += LittleLong(mesh[i].first_triangle);
		gai[i].numindexes = nt*3;
		idx = ZG_Malloc(&mod->memgroup, sizeof(*idx)*gai[i].numindexes);
		gai[i].ofs_indexes = idx;
		for (t = 0; t < nt; t++)
		{
			*idx++ = LittleLong(tris[t].vertex[0]) - offset;
			*idx++ = LittleLong(tris[t].vertex[1]) - offset;
			*idx++ = LittleLong(tris[t].vertex[2]) - offset;
		}

		/*verts*/
		gai[i].shares_verts = i;
		gai[i].numverts = LittleLong(mesh[i].num_vertexes);
		gai[i].ofs_skel_xyz = (opos+offset);
		gai[i].ofs_skel_norm = (onorm1+offset);
		gai[i].ofs_skel_svect = (onorm2+offset);
		gai[i].ofs_skel_tvect = (onorm3+offset);
		gai[i].ofs_skel_idx = oindex?(oindex+offset):NULL;
		gai[i].ofs_skel_weight = oweight?(oweight+offset):NULL;
	}
	if (!noweights)
	{
		if (!IQM_ImportArray4B(buffer, &vbone, oindex, h->num_vertexes, h->num_joints))
			Con_Printf(CON_WARNING "Invalid bone indexes detected inside %s\n", mod->name);
		IQM_ImportArrayF(buffer, &vweight, (float*)oweight, 4, h->num_vertexes, defaultweight);
	}

	if (otcoords)
		IQM_ImportArrayF(buffer, &vtcoord, (float*)otcoords, 2, h->num_vertexes, defaultweight);
	if (orgbaf)
		IQM_ImportArrayF(buffer, &vrgba, (float*)orgbaf, 4, h->num_vertexes, defaultcolour);

	IQM_ImportArrayF(buffer, &vnorm, (float*)onorm1, 3, h->num_vertexes, defaultcolour);
	IQM_ImportArrayF(buffer, &vpos, (float*)opos, sizeof(opos[0])/sizeof(float), h->num_vertexes, defaultvert);

	//fixme: shouldn't really be needed for an animated model
	for (i = 0; i < h->num_vertexes; i++)
		AddPointToBounds(opos[i], mod->mins, mod->maxs);

	if (vnorm.offset && vtang)
	{
		for (i = 0; i < h->num_vertexes; i++)
		{
			VectorCopy(vtang+i*4, onorm2[i]);
			if(LittleFloat(vtang[i*4 + 3]) < 0)
				CrossProduct(onorm2[i], onorm1[i], onorm3[i]);
			else
				CrossProduct(onorm1[i], onorm2[i], onorm3[i]);
		}
	}
#ifndef SERVERONLY	//hopefully dedicated servers won't need this too often...
	else if (h->num_vertexes)
	{	//make something up
		for (i = 0; i < h->num_meshes; i++)
		{
			Mod_AccumulateTextureVectors(gai[i].ofs_skel_xyz, gai[i].ofs_st_array, gai[i].ofs_skel_norm, gai[i].ofs_skel_svect, gai[i].ofs_skel_tvect, gai[i].ofs_indexes, gai[i].numindexes);
		}
		for (i = 0; i < h->num_meshes; i++)
		{
			Mod_NormaliseTextureVectors(gai[i].ofs_skel_norm, gai[i].ofs_skel_svect, gai[i].ofs_skel_tvect, gai[i].numverts);
		}
	}
#endif

	return gai;
}

qboolean Mod_ParseIQMAnim(char *buffer, galiasinfo_t *prototype, void**poseofs, galiasanimation_t *gat)
{
	return false;
}



qboolean QDECL Mod_LoadInterQuakeModel(model_t *mod, void *buffer, size_t fsize)
{
	galiasinfo_t *root;
	struct iqmheader *h = (struct iqmheader *)buffer;

	ClearBounds(mod->mins, mod->maxs);

	root = Mod_ParseIQMMeshModel(mod, buffer, fsize);
	if (!root)
	{
		return false;
	}

	mod->flags = h->flags;

	mod->radius = RadiusFromBounds(mod->mins, mod->maxs);

	Mod_ClampModelSize(mod);

	mod->meshinfo = root;
	mod->type = mod_alias;

	return true;
}
#endif





#ifdef MD5MODELS

qboolean Mod_ParseMD5Anim(model_t *mod, char *buffer, galiasinfo_t *prototype, void**poseofs, galiasanimation_t *gat)
{
#define MD5ERROR0PARAM(x) { Con_Printf(CON_ERROR x "\n"); return false; }
#define MD5ERROR1PARAM(x, y) { Con_Printf(CON_ERROR x "\n", y); return false; }
#define EXPECT(x) buffer = COM_Parse(buffer); if (strcmp(com_token, x)) MD5ERROR1PARAM("MD5ANIM: expected %s", x);
	unsigned int i, j;

	galiasanimation_t grp;

	unsigned int parent;
	unsigned int numframes;
	unsigned int numjoints;
	float framespersecond;
	unsigned int numanimatedparts;
	galiasbone_t *bonelist;

	unsigned char *boneflags;
	unsigned int *firstanimatedcomponents;

	float *animatedcomponents;
	float *baseframe;	//6 components.
	float *posedata;
	float tx, ty, tz, qx, qy, qz;
	int fac, flags;
	float f;
	char com_token[8192];

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

	firstanimatedcomponents = BZ_Malloc(sizeof(int)*numjoints);
	animatedcomponents = BZ_Malloc(sizeof(float)*numanimatedparts);
	boneflags = BZ_Malloc(sizeof(unsigned char)*numjoints);
	baseframe = BZ_Malloc(sizeof(float)*12*numjoints);

	*poseofs = posedata = ZG_Malloc(&mod->memgroup, sizeof(float)*12*numjoints*numframes);

	if (prototype->numbones)
	{
		if (prototype->numbones != numjoints)
			MD5ERROR0PARAM("MD5ANIM: number of bones doesn't match");
		bonelist = prototype->ofsbones;
	}
	else
	{
		bonelist = ZG_Malloc(&mod->memgroup, sizeof(galiasbone_t)*numjoints);
		prototype->ofsbones = bonelist;
	}

	EXPECT("hierarchy");
	EXPECT("{");
	for (i = 0; i < numjoints; i++, bonelist++)
	{
		buffer = COM_Parse(buffer);
		if (prototype->numbones)
		{
			if (strcmp(bonelist->name, com_token))
				MD5ERROR1PARAM("MD5ANIM: bone name doesn't match (%s)", com_token);
		}
		else
			Q_strncpyz(bonelist->name, com_token, sizeof(bonelist->name));
		buffer = COM_Parse(buffer);
		parent = atoi(com_token);
		if (prototype->numbones)
		{
			if (bonelist->parent != parent)
				MD5ERROR1PARAM("MD5ANIM: bone name doesn't match (%s)", com_token);
		}
		else
			bonelist->parent = parent;

		buffer = COM_Parse(buffer);
		boneflags[i] = atoi(com_token);
		buffer = COM_Parse(buffer);
		firstanimatedcomponents[i] = atoi(com_token);
	}
	EXPECT("}");

	if (!prototype->numbones)
		prototype->numbones = numjoints;

	EXPECT("bounds");
	EXPECT("{");
	for (i = 0; i < numframes; i++)
	{
		EXPECT("(");
		buffer = COM_Parse(buffer);f=atoi(com_token);
		if (f < mod->mins[0]) mod->mins[0] = f;
		buffer = COM_Parse(buffer);f=atoi(com_token);
		if (f < mod->mins[1]) mod->mins[1] = f;
		buffer = COM_Parse(buffer);f=atoi(com_token);
		if (f < mod->mins[2]) mod->mins[2] = f;
		EXPECT(")");
		EXPECT("(");
		buffer = COM_Parse(buffer);f=atoi(com_token);
		if (f > mod->maxs[0]) mod->maxs[0] = f;
		buffer = COM_Parse(buffer);f=atoi(com_token);
		if (f > mod->maxs[1]) mod->maxs[1] = f;
		buffer = COM_Parse(buffer);f=atoi(com_token);
		if (f > mod->maxs[2]) mod->maxs[2] = f;
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
			animatedcomponents[j] = atof(com_token);
		}
		EXPECT("}");

		for (j = 0; j < numjoints; j++)
		{
			fac = firstanimatedcomponents[j];
			flags = boneflags[j];

			if (flags&1)
				tx = animatedcomponents[fac++];
			else
				tx = baseframe[j*6+0];
			if (flags&2)
				ty = animatedcomponents[fac++];
			else
				ty = baseframe[j*6+1];
			if (flags&4)
				tz = animatedcomponents[fac++];
			else
				tz = baseframe[j*6+2];
			if (flags&8)
				qx = animatedcomponents[fac++];
			else
				qx = baseframe[j*6+3];
			if (flags&16)
				qy = animatedcomponents[fac++];
			else
				qy = baseframe[j*6+4];
			if (flags&32)
				qz = animatedcomponents[fac++];
			else
				qz = baseframe[j*6+5];

			GenMatrix(tx, ty, tz, qx, qy, qz, posedata+12*(j+numjoints*i));
		}
	}

	BZ_Free(firstanimatedcomponents);
	BZ_Free(animatedcomponents);
	BZ_Free(boneflags);
	BZ_Free(baseframe);

	Q_strncpyz(grp.name, "", sizeof(grp.name));
	grp.skeltype = SKEL_RELATIVE;
	grp.numposes = numframes;
	grp.rate = framespersecond;
	grp.loop = true;

	*gat = grp;
	return true;
#undef MD5ERROR0PARAM
#undef MD5ERROR1PARAM
#undef EXPECT
}

galiasinfo_t *Mod_ParseMD5MeshModel(model_t *mod, char *buffer, char *modname)
{
#define MD5ERROR0PARAM(x) { Con_Printf(CON_ERROR x "\n"); return NULL; }
#define MD5ERROR1PARAM(x, y) { Con_Printf(CON_ERROR x "\n", y); return NULL; }
#define EXPECT(x) buffer = COM_Parse(buffer); if (strcmp(com_token, x)) Sys_Error("MD5MESH: expected %s", x);
	int numjoints = 0;
	int nummeshes = 0;
	qboolean foundjoints = false;
	int i;

	galiasbone_t *bones = NULL;
	galiasanimation_t *pose = NULL;
	galiasinfo_t *inf, *root, *lastsurf;
	float *posedata;
#ifndef SERVERONLY
	galiasskin_t *skin;
	skinframe_t *frames;
#endif
	char *filestart = buffer;

	float x, y, z, qx, qy, qz;

	buffer = COM_Parse(buffer);
	if (strcmp(com_token, "MD5Version"))
		MD5ERROR0PARAM("MD5 model without MD5Version identifier first");

	buffer = COM_Parse(buffer);
	if (atoi(com_token) != 10)
		MD5ERROR0PARAM("MD5 model with unsupported MD5Version");


	root = ZG_Malloc(&mod->memgroup, sizeof(galiasinfo_t));
	lastsurf = NULL;

	for(;;)
	{
		buffer = COM_Parse(buffer);
		if (!buffer)
			break;

		if (!strcmp(com_token, "numFrames"))
		{
			void *poseofs;
			galiasanimation_t *grp = ZG_Malloc(&mod->memgroup, sizeof(galiasanimation_t));
			Mod_ParseMD5Anim(mod, filestart, root, &poseofs, grp);
			root->ofsanimations = grp;
			root->numanimations = 1;
			grp->poseofs = poseofs;
			return root;
		}
		else if (!strcmp(com_token, "commandline"))
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

			bones = ZG_Malloc(&mod->memgroup, sizeof(*bones) * numjoints);
			pose = ZG_Malloc(&mod->memgroup, sizeof(galiasanimation_t));
			posedata = ZG_Malloc(&mod->memgroup, sizeof(float)*12 * numjoints);
			pose->skeltype = SKEL_ABSOLUTE;
			pose->rate = 1;
			pose->numposes = 1;
			pose->boneofs = posedata;

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
			index_t *indexes = NULL;
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
				inf = ZG_Malloc(&mod->memgroup, sizeof(*inf));
				lastsurf->nextsurf = inf;
				lastsurf = inf;
			}

			inf->ofsbones = bones;
			inf->numbones = numjoints;
			inf->numanimations = 1;
			inf->ofsanimations = pose;
			inf->baseframeofs = pose->boneofs;

#ifndef SERVERONLY
			skin = ZG_Malloc(&mod->memgroup, sizeof(*skin));
			frames = ZG_Malloc(&mod->memgroup, sizeof(*frames));
			inf->numskins = 1;
			inf->ofsskins = skin;
			skin->numframes = 1;
			skin->skinspeed = 1;
			skin->frame = frames;
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
					//FIXME: we probably want to support multiple skins some time
					Q_strncpyz(frames[0].shadername, com_token, sizeof(frames[0].shadername));
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
					stcoord = ZG_Malloc(&mod->memgroup, sizeof(float)*2*numverts);
					inf->ofs_st_array = (vec2_t*)stcoord;
					inf->numverts = numverts;
#endif
				}
				else if (!strcmp(com_token, "vert"))
				{	//vert num ( s t ) firstweight numweights

					buffer = COM_Parse(buffer);
					num = atoi(com_token);
					if (num < 0 || num >= numverts)
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

					indexes = ZG_Malloc(&mod->memgroup, sizeof(int)*3*numtris);
					inf->ofs_indexes = indexes;
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

			trans = ZG_Malloc(&mod->memgroup, sizeof(*trans)*numusableweights);
			inf->ofsswtransforms = trans;

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
			inf->numswtransforms = vnum;

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

	Alias_CalculateSkeletalNormals(root);

	return root;
#undef MD5ERROR0PARAM
#undef MD5ERROR1PARAM
#undef EXPECT
}

qboolean QDECL Mod_LoadMD5MeshModel(model_t *mod, void *buffer, size_t fsize)
{
	galiasinfo_t *root;

	root = Mod_ParseMD5MeshModel(mod, buffer, mod->name);
	if (root == NULL)
	{
		return false;
	}


	mod->flags = Mod_ReadFlagsFromMD1(mod->name, 0);	//file replacement - inherit flags from any defunc mdl files.

	Mod_ClampModelSize(mod);


	mod->type = mod_alias;
	mod->meshinfo = root;

	mod->funcs.NativeTrace = Mod_Trace;
	return true;
}

/*
EXTERNALANIM

//File that specifies md5 model/anim stuff.

model test/imp.md5mesh

group test/idle1.md5anim
clampgroup test/idle1.md5anim
frames test/idle1.md5anim

*/
qboolean QDECL Mod_LoadCompositeAnim(model_t *mod, void *buffer, size_t fsize)
{
	int i;

	char *file;
	galiasinfo_t *root = NULL, *surf;
	int numgroups = 0;
	galiasanimation_t *grouplist = NULL;
	galiasanimation_t *newgroup = NULL;
	float **poseofs;
	char com_token[8192];

	buffer = COM_Parse(buffer);
	if (strcmp(com_token, "EXTERNALANIM"))
	{
		Con_Printf (CON_ERROR "EXTERNALANIM: header is not compleate (%s)\n", mod->name);
		return false;
	}

	buffer = COM_Parse(buffer);
	if (!strcmp(com_token, "model"))
	{
		buffer = COM_Parse(buffer);
		file = COM_LoadTempMoreFile(com_token, NULL);

		if (!file)	//FIXME: make non fatal somehow..
		{
			Con_Printf(CON_ERROR "Couldn't open %s (from %s)\n", com_token, mod->name);
			return false;
		}

		root = Mod_ParseMD5MeshModel(mod, file, mod->name);
		if (root == NULL)
		{
			return false;
		}
		newgroup = root->ofsanimations;

		grouplist = BZ_Malloc(sizeof(galiasanimation_t)*(numgroups+root->numanimations));
		memcpy(grouplist, newgroup, sizeof(galiasanimation_t)*(numgroups+root->numanimations));
		poseofs = BZ_Malloc(sizeof(galiasanimation_t)*(numgroups+root->numanimations));
		for (i = 0; i < root->numanimations; i++)
		{
			grouplist[numgroups] = newgroup[i];
			poseofs[numgroups] = newgroup[i].boneofs;
			numgroups++;
		}
	}
	else
	{
		Con_Printf (CON_ERROR "EXTERNALANIM: model must be defined immediatly after the header\n");
		return false;
	}

	for (;;)
	{
		buffer = COM_Parse(buffer);
		if (!buffer)
			break;

		if (!strcmp(com_token, "group"))
		{
			grouplist = BZ_Realloc(grouplist, sizeof(galiasanimation_t)*(numgroups+1));
			poseofs = BZ_Realloc(poseofs, sizeof(*poseofs)*(numgroups+1));
			buffer = COM_Parse(buffer);
			file = COM_LoadTempMoreFile(com_token, NULL);
			if (file)	//FIXME: make non fatal somehow..
			{
				char namebkup[MAX_QPATH];
				Q_strncpyz(namebkup, com_token, sizeof(namebkup));
				if (!Mod_ParseMD5Anim(mod, file, root, (void**)&poseofs[numgroups], &grouplist[numgroups]))
				{
					return false;
				}
				Q_strncpyz(grouplist[numgroups].name, namebkup, sizeof(grouplist[numgroups].name));
				numgroups++;
			}
		}
		else if (!strcmp(com_token, "clampgroup"))
		{
			grouplist = BZ_Realloc(grouplist, sizeof(galiasanimation_t)*(numgroups+1));
			poseofs = BZ_Realloc(poseofs, sizeof(*poseofs)*(numgroups+1));
			buffer = COM_Parse(buffer);
			file = COM_LoadTempMoreFile(com_token, NULL);
			if (file)	//FIXME: make non fatal somehow..
			{
				char namebkup[MAX_QPATH];
				Q_strncpyz(namebkup, com_token, sizeof(namebkup));
				if (!Mod_ParseMD5Anim(mod, file, root, (void**)&poseofs[numgroups], &grouplist[numgroups]))
				{
					return false;
				}
				Q_strncpyz(grouplist[numgroups].name, namebkup, sizeof(grouplist[numgroups].name));
				grouplist[numgroups].loop = false;
				numgroups++;
			}
		}
		else if (!strcmp(com_token, "frames"))
		{
			galiasanimation_t ng;
			void *np;

			buffer = COM_Parse(buffer);
			file = COM_LoadTempMoreFile(com_token, NULL);
			if (file)	//FIXME: make non fatal somehow..
			{
				char namebkup[MAX_QPATH];
				Q_strncpyz(namebkup, com_token, sizeof(namebkup));
				if (!Mod_ParseMD5Anim(mod, file, root, &np, &ng))
				{
					return false;
				}

				grouplist = BZ_Realloc(grouplist, sizeof(galiasanimation_t)*(numgroups+ng.numposes));
				poseofs = BZ_Realloc(poseofs, sizeof(*poseofs)*(numgroups+ng.numposes));

				//pull out each frame individually
				for (i = 0; i < ng.numposes; i++)
				{
					grouplist[numgroups].skeltype = ng.skeltype;
					grouplist[numgroups].loop = false;
					grouplist[numgroups].numposes = 1;
					grouplist[numgroups].rate = 24;
					poseofs[numgroups] = (float*)np + i*12*root->numbones;
					Q_snprintfz(grouplist[numgroups].name, sizeof(grouplist[numgroups].name), "%s%i", namebkup, i);
					Q_strncpyz(grouplist[numgroups].name, namebkup, sizeof(grouplist[numgroups].name));
					grouplist[numgroups].loop = false;
					numgroups++;
				}
			}
		}
		else
		{
			Con_Printf(CON_ERROR "EXTERNALANIM: unrecognised token (%s)\n", mod->name);
			return false;
		}
	}

	newgroup = grouplist;
	grouplist = ZG_Malloc(&mod->memgroup, sizeof(galiasanimation_t)*numgroups);
	for(surf = root;;)
	{
		surf->ofsanimations = grouplist;
		surf->numanimations = numgroups;
		if (!surf->nextsurf)
			break;
		surf = surf->nextsurf;
	}
	for (i = 0; i < numgroups; i++)
	{
		grouplist[i] = newgroup[i];
		grouplist[i].boneofs = poseofs[i];
	}

	mod->flags = Mod_ReadFlagsFromMD1(mod->name, 0);	//file replacement - inherit flags from any defunc mdl files.

	Mod_ClampModelSize(mod);

	mod->type = mod_alias;
	mod->meshinfo = root;

	mod->funcs.NativeTrace = Mod_Trace;
	return true;
}

#endif //MD5MODELS


void Alias_Register(void)
{
	Mod_RegisterModelFormatMagic(NULL, "Quake1 Model (mdl)",				IDPOLYHEADER,							Mod_LoadQ1Model);
	Mod_RegisterModelFormatMagic(NULL, "QuakeForge 16bit Model",			(('6'<<24)+('1'<<16)+('D'<<8)+'M'),		Mod_LoadQ1Model);
#ifdef HEXEN2
	Mod_RegisterModelFormatMagic(NULL, "Hexen2 Model (mdl)",				RAPOLYHEADER,							Mod_LoadQ1Model);
#endif
#ifdef MD2MODELS
	Mod_RegisterModelFormatMagic(NULL, "Quake2 Model (md2)",				MD2IDALIASHEADER,						Mod_LoadQ2Model);
#endif
#ifdef MD3MODELS
	Mod_RegisterModelFormatMagic(NULL, "Quake3 Model (md3)",				MD3_IDENT,								Mod_LoadQ3Model);
#endif
#ifdef HALFLIFEMODELS
	Mod_RegisterModelFormatMagic(NULL, "Half-Life Model (mdl)",				(('T'<<24)+('S'<<16)+('D'<<8)+'I'),		Mod_LoadHLModel);
#endif

#ifdef ZYMOTICMODELS
	Mod_RegisterModelFormatMagic(NULL, "Zymotic Model (zym)",				(('O'<<24)+('M'<<16)+('Y'<<8)+'Z'),		Mod_LoadZymoticModel);
#endif
#ifdef DPMMODELS
	Mod_RegisterModelFormatMagic(NULL, "DarkPlaces Model (dpm)",			(('K'<<24)+('R'<<16)+('A'<<8)+'D'),		Mod_LoadDarkPlacesModel);
#endif
#ifdef PSKMODELS
	Mod_RegisterModelFormatMagic(NULL, "Unreal Interchange Model (psk)",	('A'<<0)+('C'<<8)+('T'<<16)+('R'<<24),	Mod_LoadPSKModel);
#endif
#ifdef INTERQUAKEMODELS
	Mod_RegisterModelFormatMagic(NULL, "Inter-Quake Model (iqm)",			('I'<<0)+('N'<<8)+('T'<<16)+('E'<<24),	Mod_LoadInterQuakeModel);
#endif
#ifdef MD5MODELS
	Mod_RegisterModelFormatText(NULL, "MD5 Mesh/Anim (md5mesh)",			"MD5Version",							Mod_LoadMD5MeshModel);
	Mod_RegisterModelFormatText(NULL, "External Anim",						"EXTERNALANIM",							Mod_LoadCompositeAnim);
#endif
}
