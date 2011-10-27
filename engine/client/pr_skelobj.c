/*
Copyright (C) 2011 Id Software, Inc.

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
/*
this file deals with qc builtins to apply custom skeletal blending (skeletal objects extension), as well as the logic required to perform realtime ragdoll, if I ever implement that.
*/

#include "quakedef.h"

#ifdef CSQC_DAT

#include "pr_common.h"

#define MAX_SKEL_OBJECTS 1024

/*this is the description of the ragdoll, it is how the doll flops around*/
typedef struct doll_s
{
	char *name;
	struct doll_s *next;

	int numbodies;
	struct
	{
		int joint;
		char *name;
	} body[32];
//	struct
//	{

//	};
} doll_t;

enum
{
	BF_ACTIVE, /*used to avoid traces if doll is stationary*/
	BF_INSOLID
};
typedef struct {
	int jointo;	/*multiple of 12*/
	int flags;
	vec3_t vel;
} body_t;

/*this is the skeletal object*/
typedef struct {
	int inuse;

	model_t *model;
	enum
	{
		SKOT_HBLEND,
		SKOT_ABLEND,
		SKOT_ARAG
	} type;

	unsigned int numbones;
	float *bonematrix;
/*
	unsigned int numbodies;
	body_t *body;
	doll_t *doll;
	*/
} skelobject_t;

static doll_t *dolllist;
static skelobject_t skelobjects[MAX_SKEL_OBJECTS];
static int numskelobjectsused;

static qboolean pendingkill; /*states that there is a skel waiting to be killed*/
#if 0
doll_t *rag_loaddoll(char *fname)
{
	doll_t *d;
	void *fptr = NULL;
	int fsize;

	for (d = dolllist; d; d = d->next)
	{
		if (!strcmp(d->name, fname))
			return d;
	}

	fsize = FS_LoadFile(fname, &fptr);
	if (!fptr)
		return NULL;
	FS_FreeFile(fptr);
}

void skel_integrate(progfuncs_t *prinst, skelobject_t *sko, float ft)
{
	unsigned int p;
	trace_t t;
	vec3_t npos, opos;
	world_t *w = prinst->parms->user;
	body_t *b;
	float gravity = 800;

	for (p = 0, b = sko->body; p < sko->numbodies; p++, b++)
	{
		/*handle gravity*/
		b->vel[2] = b->vel[2] - gravity * ft / 2;

		opos[0] = sko->bonematrix[b->jointo + 3 ];
		opos[1] = sko->bonematrix[b->jointo + 7 ];
		opos[2] = sko->bonematrix[b->jointo + 11];
		npos[0] = opos[0] + b->vel[0]*ft;
		npos[1] = opos[1] + b->vel[1]*ft;
		npos[2] = opos[2] + b->vel[2]*ft;
		t = World_Move(w, opos, vec3_origin, vec3_origin, npos, MOVE_NOMONSTERS, w->edicts);
		sko->bonematrix[b->jointo + 3 ] = t.endpos[0];
		sko->bonematrix[b->jointo + 7 ] = t.endpos[1];
		sko->bonematrix[b->jointo + 11] = t.endpos[2];

		/*handle gravity again to compensate for framerate*/
		b->vel[2] = b->vel[2] - gravity * ft / 2;
	}
	
	/*draw points*/
	for (p = 0, b = sko->body; p < sko->numbodies; p++, b++)
	{
		opos[0] = sko->bonematrix[b->jointo + 3 ];
		opos[1] = sko->bonematrix[b->jointo + 7 ];
		opos[2] = sko->bonematrix[b->jointo + 11];
		P_RunParticleEffectTypeString(opos, b->vel, 1, "ragdolltest");
	}
}
#endif

/*destroys all skeletons*/
void skel_reset(progfuncs_t *prinst)
{
	while (numskelobjectsused > 0)
	{
		numskelobjectsused--;
		skelobjects[numskelobjectsused].numbones = 0;
		skelobjects[numskelobjectsused].inuse = false;
	}
}

/*deletes any skeletons marked for deletion*/
void skel_dodelete(progfuncs_t *prinst)
{
	int skelidx;
	if (!pendingkill)
		return;

	pendingkill = false;
	for (skelidx = 0; skelidx < numskelobjectsused; skelidx++)
	{
		if (skelobjects[skelidx].inuse == 2)
			skelobjects[skelidx].inuse = 0;
	}

	while (numskelobjectsused && !skelobjects[numskelobjectsused-1].inuse)
		numskelobjectsused--;
}

skelobject_t *skel_get(progfuncs_t *prinst, int skelidx, int bonecount)
{
	if (skelidx == 0)
	{
		//allocation
		if (!bonecount)
			return NULL;

		for (skelidx = 0; skelidx < numskelobjectsused; skelidx++)
		{
			if (!skelobjects[skelidx].inuse && skelobjects[skelidx].numbones == bonecount)
				return &skelobjects[skelidx];
		}

		for (skelidx = 0; skelidx <= numskelobjectsused; skelidx++)
		{
			if (!skelobjects[skelidx].inuse && !skelobjects[skelidx].numbones)
			{
				skelobjects[skelidx].numbones = bonecount;
				/*so bone matrix list can be mmapped some day*/
				skelobjects[skelidx].bonematrix = (float*)PR_AddString(prinst, "", sizeof(float)*12*bonecount);
				if (skelidx <= numskelobjectsused)
				{
					numskelobjectsused = skelidx + 1;
					skelobjects[skelidx].model = NULL;
					skelobjects[skelidx].inuse = 1;
				}
				return &skelobjects[skelidx];
			}
		}

		return NULL;
	}
	else
	{
		skelidx--;
		if ((unsigned int)skelidx >= numskelobjectsused)
			return NULL;
		if (skelobjects[skelidx].inuse != 1)
			return NULL;
		if (bonecount && skelobjects[skelidx].numbones != bonecount)
			return NULL;
		return &skelobjects[skelidx];
	}
}

void skel_lookup(progfuncs_t *prinst, int skelidx, framestate_t *out)
{
	skelobject_t *sko = skel_get(prinst, skelidx, 0);
	if (sko && sko->inuse)
	{
		out->bonecount = sko->numbones;
		out->bonestate = sko->bonematrix;
	}
}

//float(float modelindex) skel_create (FTE_CSQC_SKELETONOBJECTS)
void QCBUILTIN PF_skel_create (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	world_t *w = prinst->parms->user;

	int numbones;
	skelobject_t *skelobj;
	model_t *model;
	int midx;
	int type;
	char *afname;

	midx = G_FLOAT(OFS_PARM0);

	if (*prinst->callargc > 1)
		afname = PR_GetStringOfs(prinst, OFS_PARM1);
	else
		afname = "";

	//default to failure
	G_FLOAT(OFS_RETURN) = 0;

	model = w->Get_CModel(w, midx);
	if (!model)
		return; //no model set, can't get a skeleton

	type = SKOT_HBLEND;
	numbones = Mod_GetNumBones(model, type != SKOT_HBLEND);
	if (!numbones)
	{
//		isabs = true;
//		numbones = Mod_GetNumBones(model, isabs);
//		if (!numbones)
			return;	//this isn't a skeletal model.
	}

	skelobj = skel_get(prinst, 0, numbones);
	if (!skelobj)
		return;	//couldn't get one, ran out of memory or something?

	skelobj->model = model;
	skelobj->type = type;

	G_FLOAT(OFS_RETURN) = (skelobj - skelobjects) + 1;
}

//float(float skel, entity ent, float modelindex, float retainfrac, float firstbone, float lastbone) skel_build (FTE_CSQC_SKELETONOBJECTS)
void QCBUILTIN PF_skel_build(progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	world_t *w = prinst->parms->user;
	int skelidx = G_FLOAT(OFS_PARM0);
	wedict_t *ent = (wedict_t*)G_EDICT(prinst, OFS_PARM1);
	int midx = G_FLOAT(OFS_PARM2);
	float retainfrac = G_FLOAT(OFS_PARM3);
	int firstbone = G_FLOAT(OFS_PARM4)-1;
	int lastbone = G_FLOAT(OFS_PARM5)-1;
	float addition = 1?G_FLOAT(OFS_PARM6):1-retainfrac;

	int i, j;
	int numbones;
	framestate_t fstate;
	skelobject_t *skelobj;
	model_t *model;

	//default to failure
	G_FLOAT(OFS_RETURN) = 0;

	model = w->Get_CModel(w, midx);
	if (!model)
		return; //invalid model, can't get a skeleton

	w->Get_FrameState(w, ent, &fstate);

	//heh... don't copy.
	fstate.bonecount = 0;
	fstate.bonestate = NULL;

	numbones = Mod_GetNumBones(model, false);
	if (!numbones)
	{
		return;	//this isn't a skeletal model.
	}

	skelobj = skel_get(prinst, skelidx, 0);
	if (!skelobj)
		return;	//couldn't get one, ran out of memory or something?

	if (lastbone < 0)
		lastbone = numbones;
	if (lastbone > numbones)
		lastbone = numbones;
	if (firstbone < 0)
		firstbone = 0;

	if (retainfrac == 0)
	{
		/*replace everything*/
		if (addition == 1)
			Mod_GetBoneRelations(model, firstbone, lastbone, &fstate, skelobj->bonematrix);
		else
		{
			//scale new
			float relationsbuf[MAX_BONES*12];
			Mod_GetBoneRelations(model, firstbone, lastbone, &fstate, relationsbuf);
			for (i = firstbone; i < lastbone; i++)
			{
				for (j = 0; j < 12; j++)
					skelobj->bonematrix[i*12+j] = addition*relationsbuf[i*12+j];
			}
		}
	}
	else
	{
		if (retainfrac != 1)
		{
			//rescale the existing bones
			for (i = firstbone; i < lastbone; i++)
			{
				for (j = 0; j < 12; j++)
					skelobj->bonematrix[i*12+j] *= retainfrac;
			}
		}
		if (addition == 1)
		{
			//just add
			float relationsbuf[MAX_BONES*12];
			Mod_GetBoneRelations(model, firstbone, lastbone, &fstate, relationsbuf);
			for (i = firstbone; i < lastbone; i++)
			{
				for (j = 0; j < 12; j++)
					skelobj->bonematrix[i*12+j] += relationsbuf[i*12+j];
			}
		}
		else if (addition)
		{
			//add+scale
			float relationsbuf[MAX_BONES*12];
			Mod_GetBoneRelations(model, firstbone, lastbone, &fstate, relationsbuf);
			for (i = firstbone; i < lastbone; i++)
			{
				for (j = 0; j < 12; j++)
					skelobj->bonematrix[i*12+j] += addition*relationsbuf[i*12+j];
			}
		}
	}

	G_FLOAT(OFS_RETURN) = (skelobj - skelobjects) + 1;
}

//float(float skel) skel_get_numbones (FTE_CSQC_SKELETONOBJECTS)
void QCBUILTIN PF_skel_get_numbones (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	int skelidx = G_FLOAT(OFS_PARM0);
	skelobject_t *skelobj;

	skelobj = skel_get(prinst, skelidx, 0);

	if (!skelobj)
		G_FLOAT(OFS_RETURN) = 0;
	else
		G_FLOAT(OFS_RETURN) = skelobj->numbones;
}

//string(float skel, float bonenum) skel_get_bonename (FTE_CSQC_SKELETONOBJECTS) (returns tempstring)
void QCBUILTIN PF_skel_get_bonename (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	int skelidx = G_FLOAT(OFS_PARM0);
	int boneidx = G_FLOAT(OFS_PARM1);
	skelobject_t *skelobj;

	skelobj = skel_get(prinst, skelidx, 0);

	if (!skelobj)
		G_INT(OFS_RETURN) = 0;
	else
	{
		RETURN_TSTRING(Mod_GetBoneName(skelobj->model, boneidx));
	}
}

//float(float skel, float bonenum) skel_get_boneparent (FTE_CSQC_SKELETONOBJECTS)
void QCBUILTIN PF_skel_get_boneparent (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	int skelidx = G_FLOAT(OFS_PARM0);
	int boneidx = G_FLOAT(OFS_PARM1);
	skelobject_t *skelobj;

	skelobj = skel_get(prinst, skelidx, 0);

	if (!skelobj)
		G_FLOAT(OFS_RETURN) = 0;
	else
		G_FLOAT(OFS_RETURN) = Mod_GetBoneParent(skelobj->model, boneidx);
}

//float(float skel, string tagname) skel_find_bone (FTE_CSQC_SKELETONOBJECTS)
void QCBUILTIN PF_skel_find_bone (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	int skelidx = G_FLOAT(OFS_PARM0);
	char *bname = PR_GetStringOfs(prinst, OFS_PARM1);
	skelobject_t *skelobj;

	skelobj = skel_get(prinst, skelidx, 0);
	if (!skelobj)
		G_FLOAT(OFS_RETURN) = 0;
	else
		G_FLOAT(OFS_RETURN) = Mod_TagNumForName(skelobj->model, bname);
}

static void bonemat_fromqcvectors(float *out, const float vx[3], const float vy[3], const float vz[3], const float t[3])
{
	out[0] = vx[0];
	out[1] = -vy[0];
	out[2] = vz[0];
	out[3] = t[0];
	out[4] = vx[1];
	out[5] = -vy[1];
	out[6] = vz[1];
	out[7] = t[1];
	out[8] = vx[2];
	out[9] = -vy[2];
	out[10] = vz[2];
	out[11] = t[2];
}
static void bonemat_toqcvectors(const float *in, float vx[3], float vy[3], float vz[3], float t[3])
{
	vx[0] = in[0];
	vx[1] = in[4];
	vx[2] = in[8];
	vy[0] = -in[1];
	vy[1] = -in[5];
	vy[2] = -in[9];
	vz[0] = in[2];
	vz[1] = in[6];
	vz[2] = in[10];
	t [0] = in[3];
	t [1] = in[7];
	t [2] = in[11];
}

static void bonematident_toqcvectors(float vx[3], float vy[3], float vz[3], float t[3])
{
	vx[0] = 1;
	vx[1] = 0;
	vx[2] = 0;
	vy[0] = -0;
	vy[1] = -1;
	vy[2] = -0;
	vz[0] = 0;
	vz[1] = 0;
	vz[2] = 1;
	t [0] = 0;
	t [1] = 0;
	t [2] = 0;
}

//vector(float skel, float bonenum) skel_get_bonerel (FTE_CSQC_SKELETONOBJECTS) (sets v_forward etc)
void QCBUILTIN PF_skel_get_bonerel (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	world_t *w = prinst->parms->user;
	int skelidx = G_FLOAT(OFS_PARM0);
	int boneidx = G_FLOAT(OFS_PARM1)-1;
	skelobject_t *skelobj = skel_get(prinst, skelidx, 0);
	if (!skelobj || (unsigned int)boneidx >= skelobj->numbones)
		bonematident_toqcvectors(w->g.v_forward, w->g.v_right, w->g.v_up, G_VECTOR(OFS_RETURN));
	else if (skelobj->type!=SKOT_HBLEND)
	{
		//FIXME
		bonematident_toqcvectors(w->g.v_forward, w->g.v_right, w->g.v_up, G_VECTOR(OFS_RETURN));
	}
	else
		bonemat_toqcvectors(skelobj->bonematrix+12*boneidx, w->g.v_forward, w->g.v_right, w->g.v_up, G_VECTOR(OFS_RETURN));
}

//vector(float skel, float bonenum) skel_get_boneabs (FTE_CSQC_SKELETONOBJECTS) (sets v_forward etc)
void QCBUILTIN PF_skel_get_boneabs (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	world_t *w = prinst->parms->user;
	int skelidx = G_FLOAT(OFS_PARM0);
	int boneidx = G_FLOAT(OFS_PARM1)-1;
	float workingm[12], tempmatrix[3][4];
	int i;
	skelobject_t *skelobj = skel_get(prinst, skelidx, 0);

	if (!skelobj || (unsigned int)boneidx >= skelobj->numbones)
		bonematident_toqcvectors(w->g.v_forward, w->g.v_right, w->g.v_up, G_VECTOR(OFS_RETURN));
	else if (skelobj->type != SKOT_HBLEND)
	{
		//can just copy it out
		bonemat_toqcvectors(skelobj->bonematrix + boneidx*12, w->g.v_forward, w->g.v_right, w->g.v_up, G_VECTOR(OFS_RETURN));
	}
	else
	{
		//we need to work out the abs position

		//testme

		//set up an identity matrix
		for (i = 0;i < 12;i++)
			workingm[i] = 0;
		workingm[0] = 1;
		workingm[5] = 1;
		workingm[10] = 1;

		while(boneidx >= 0)
		{
			//copy out the previous working matrix, so we don't stomp on it
			memcpy(tempmatrix, workingm, sizeof(tempmatrix));
			R_ConcatTransforms((void*)(skelobj->bonematrix + boneidx*12), (void*)tempmatrix, (void*)workingm);

			boneidx = Mod_GetBoneParent(skelobj->model, boneidx+1)-1;
		}
		bonemat_toqcvectors(workingm, w->g.v_forward, w->g.v_right, w->g.v_up, G_VECTOR(OFS_RETURN));
	}
}

//void(float skel, float bonenum, vector org) skel_set_bone (FTE_CSQC_SKELETONOBJECTS) (reads v_forward etc)
void QCBUILTIN PF_skel_set_bone (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	world_t *w = prinst->parms->user;
	int skelidx = G_FLOAT(OFS_PARM0);
	unsigned int boneidx = G_FLOAT(OFS_PARM1)-1;
	float *matrix[3];
	skelobject_t *skelobj;
	float *bone;

	if (*prinst->callargc > 5)
	{
		matrix[0] = G_VECTOR(OFS_PARM3);
		matrix[1] = G_VECTOR(OFS_PARM4);
		matrix[2] = G_VECTOR(OFS_PARM5);
	}
	else
	{
		matrix[0] = w->g.v_forward;
		matrix[1] = w->g.v_right;
		matrix[2] = w->g.v_up;
	}

	skelobj = skel_get(prinst, skelidx, 0);
	if (!skelobj || boneidx >= skelobj->numbones)
		return;

	bone = skelobj->bonematrix+12*boneidx;
	bonemat_fromqcvectors(skelobj->bonematrix+12*boneidx, matrix[0], matrix[1], matrix[2], G_VECTOR(OFS_PARM2));
}

//void(float skel, float bonenum, vector org [, vector fwd, vector right, vector up]) skel_mul_bone (FTE_CSQC_SKELETONOBJECTS) (reads v_forward etc)
void QCBUILTIN PF_skel_mul_bone (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	world_t *w = prinst->parms->user;
	int skelidx = G_FLOAT(OFS_PARM0);
	int boneidx = G_FLOAT(OFS_PARM1)-1;
	float temp[3][4];
	float mult[3][4];
	skelobject_t *skelobj;
	if (*prinst->callargc > 5)
		bonemat_fromqcvectors((float*)mult, G_VECTOR(OFS_PARM3), G_VECTOR(OFS_PARM4), G_VECTOR(OFS_PARM5), G_VECTOR(OFS_PARM2));
	else
		bonemat_fromqcvectors((float*)mult, w->g.v_forward, w->g.v_right, w->g.v_up, G_VECTOR(OFS_PARM2));

	skelobj = skel_get(prinst, skelidx, 0);
	if (!skelobj || boneidx >= skelobj->numbones)
		return;
//testme
	Vector4Copy(skelobj->bonematrix+12*boneidx+0, temp[0]);
	Vector4Copy(skelobj->bonematrix+12*boneidx+4, temp[1]);
	Vector4Copy(skelobj->bonematrix+12*boneidx+8, temp[2]);
	R_ConcatTransforms(mult, temp, (float(*)[4])(skelobj->bonematrix+12*boneidx));
}

//void(float skel, float startbone, float endbone, vector org) skel_mul_bone (FTE_CSQC_SKELETONOBJECTS) (reads v_forward etc)
void QCBUILTIN PF_skel_mul_bones (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	world_t *w = prinst->parms->user;
	int skelidx = G_FLOAT(OFS_PARM0);
	unsigned int startbone = G_FLOAT(OFS_PARM1)-1;
	unsigned int endbone = G_FLOAT(OFS_PARM2)-1;
	float temp[3][4];
	float mult[3][4];
	skelobject_t *skelobj;
	if (*prinst->callargc > 6)
		bonemat_fromqcvectors((float*)mult, G_VECTOR(OFS_PARM4), G_VECTOR(OFS_PARM5), G_VECTOR(OFS_PARM6), G_VECTOR(OFS_PARM3));
	else
		bonemat_fromqcvectors((float*)mult, w->g.v_forward, w->g.v_right, w->g.v_up, G_VECTOR(OFS_PARM3));

	skelobj = skel_get(prinst, skelidx, 0);
	if (!skelobj)
		return;

	if (startbone == -1)
		startbone = 0;
//testme
	while(startbone < endbone && startbone < skelobj->numbones)
	{
		Vector4Copy(skelobj->bonematrix+12*startbone+0, temp[0]);
		Vector4Copy(skelobj->bonematrix+12*startbone+4, temp[1]);
		Vector4Copy(skelobj->bonematrix+12*startbone+8, temp[2]);
		R_ConcatTransforms(mult, temp, (float(*)[4])(skelobj->bonematrix+12*startbone));
	}
}

//void(float skeldst, float skelsrc, float startbone, float entbone) skel_copybones (FTE_CSQC_SKELETONOBJECTS)
void QCBUILTIN PF_skel_copybones (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	int skeldst = G_FLOAT(OFS_PARM0);
	int skelsrc = G_FLOAT(OFS_PARM1);
	int startbone = G_FLOAT(OFS_PARM2)-1;
	int endbone = G_FLOAT(OFS_PARM3)-1;

	skelobject_t *skelobjdst;
	skelobject_t *skelobjsrc;

	skelobjdst = skel_get(prinst, skeldst, 0);
	skelobjsrc = skel_get(prinst, skelsrc, 0);
	if (!skelobjdst || !skelobjsrc)
		return;
	if (skelobjsrc->type != skelobjdst->type)
		return;

	if (startbone == -1)
		startbone = 0;
//testme
	while(startbone < endbone && startbone < skelobjdst->numbones && startbone < skelobjsrc->numbones)
	{
		Vector4Copy(skelobjsrc->bonematrix+12*startbone+0, skelobjdst->bonematrix+12*startbone+0);
		Vector4Copy(skelobjsrc->bonematrix+12*startbone+4, skelobjdst->bonematrix+12*startbone+4);
		Vector4Copy(skelobjsrc->bonematrix+12*startbone+8, skelobjdst->bonematrix+12*startbone+8);
	}
}

//void(float skel) skel_delete (FTE_CSQC_SKELETONOBJECTS)
void QCBUILTIN PF_skel_delete (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	int skelidx = G_FLOAT(OFS_PARM0);
	skelobject_t *skelobj;

	skelobj = skel_get(prinst, skelidx, 0);
	if (skelobj)
	{
		skelobj->inuse = 2;	//2 means don't reuse yet.
		pendingkill = true;
	}
}
#endif

