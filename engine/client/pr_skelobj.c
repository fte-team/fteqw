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

/*
skeletal objects are just a set of bone poses.
they are separate from the entity they are attached to, and must be created/destroyed separately.
typically the bones are all stored relative to their parent.
qc must use skel_build to copy animation data from the file format into the skeletal object for rendering, but can build different bones using different animations or can override explicit bones.

ragdoll file is a description of the joints in the ragdoll.
a skeletal object, built from a doll instead of a model, has a series of physics objects created at key points (ie: not face).
these objects are absolute
qc must build the skeletal object still, which fills the skeletal object from the physics objects instead of animation data, for bones that have solid objects.
*/

#include "quakedef.h"

#if defined(CSQC_DAT) || !defined(CLIENTONLY)
#define RAGDOLL

#include "pr_common.h"
#include "com_mesh.h"

#define MAX_SKEL_OBJECTS 1024

#ifdef RAGDOLL
/*this is the description of the ragdoll, it is how the doll flops around*/
typedef struct doll_s
{
	char *name;
	int uses;
	model_t *model;
	struct doll_s *next;

	int numbodies;
	int numjoints;
	int numbones;
	struct
	{
		char name[32];
		int bone;
	} *body;
	odejointinfo_t *joint;
	struct
	{
		//easy lookup table for bone->body.
		//most of these will be -1, which means 'import from animation object'
		int bodyidx;
	} *bone;
} doll_t;

enum
{
	BF_ACTIVE, /*used to avoid traces if doll is stationary*/
	BF_INSOLID
};
typedef struct
{
	odebody_t odebody;

	int ownerent;	/*multiple of 12*/
	int flags;

	float moment[12];
	float matrix[12];
} body_t;
#endif

/*this is the skeletal object*/
typedef struct skelobject_s
{
	int inuse;

	model_t *model;
	world_t *world; /*be it ssqc or csqc*/
	enum
	{
		SKOT_RELATIVE,	//relative to parent
		SKOT_ABSOLUTE	//relative to model
	} type;

	unsigned int numbones;
	float *bonematrix;

#ifdef RAGDOLL
	struct skelobject_s *animsource;
	unsigned int numbodies;
	body_t *body;
	int numjoints;
	odejoint_t *joint;
	doll_t *doll;
#endif
} skelobject_t;

static doll_t *dolllist;
static skelobject_t skelobjects[MAX_SKEL_OBJECTS];
static int numskelobjectsused;

static void bonemat_fromidentity(float *out)
{
	out[0] = 1;
	out[1] = 0;
	out[2] = 0;
	out[3] = 0;

	out[4] = 0;
	out[5] = 1;
	out[6] = 0;
	out[7] = 0;

	out[8] = 0;
	out[9] = 0;
	out[10] = 1;
	out[11] = 0;
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
static void bonemat_fromentity(world_t *w, wedict_t *ed, float *trans)
{
	vec3_t d[3], a;
	model_t *mod;
	mod = w->Get_CModel(w, ed->v->modelindex);
	if (!mod || mod->type == mod_alias)
		a[0] = -ed->v->angles[0];
	else
		a[0] = ed->v->angles[0];
	a[1] = ed->v->angles[1];
	a[2] = ed->v->angles[2];
	AngleVectors(a, d[0], d[1], d[2]);
	bonemat_fromqcvectors(trans, d[0], d[1], d[2], ed->v->origin);
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


static qboolean pendingkill; /*states that there is a skel waiting to be killed*/
#ifdef RAGDOLL
void rag_uninstanciate(skelobject_t *sko);
int rag_finddollbody(doll_t *d, char *bodyname)
{
	int i;
	for (i = 0; i < d->numbodies; i++)
	{
		if (!strcmp(d->body[i].name, bodyname))
			return i;
	}
	return -1;
}
int rag_finddolljoint(doll_t *d, char *name)
{
	int i;
	for (i = 0; i < d->numjoints; i++)
	{
		if (!strcmp(d->joint[i].name, name))
			return i;
	}
	return -1;
}
doll_t *rag_loaddoll(model_t *mod, char *fname, int numbones)
{
	doll_t *d;
	void *fptr = NULL;
	char *file;
	int fsize;
	int i;
	char *cmd;
	galiasbone_t *bones;
	int errors = 0;

	for (d = dolllist; d; d = d->next)
	{
		if (d->model == mod)
			if (!strcmp(d->name, fname))
				return d;
	}

	bones = Mod_GetBoneInfo(mod);
	if (!bones)
	{
		//model not skeletal.
		return NULL;
	}

	fsize = FS_LoadFile(fname, &fptr);
	if (!fptr)
		return NULL;

	d = malloc(sizeof(*d));
	d->next = dolllist;
	dolllist = d;
	d->name = strdup(fname);
	d->model = mod;
	d->numbodies = 0;
	d->body = NULL;
	d->numjoints = 0;
	d->uses = 0;
	d->joint = NULL;
	d->numbones = numbones;
	d->bone = malloc(sizeof(*d->bone) * d->numbones);
	for (i = 0; i < d->numbones; i++)
		d->bone[i].bodyidx = -1;
	file = fptr;
	while(file && *file)
	{
		file = Cmd_TokenizeString(file, false, false);
		cmd = Cmd_Argv(0);

		if (!stricmp(cmd, "body"))
		{
			int boneidx;
			boneidx = Mod_TagNumForName(d->model, Cmd_Argv(2))-1;
			if (boneidx >= 0)
			{
				d->body = realloc(d->body, sizeof(*d->body)*(d->numbodies+1));
				Q_strncpyz(d->body[d->numbodies].name, Cmd_Argv(1), sizeof(d->body[d->numbodies].name));
				d->bone[boneidx].bodyidx = d->numbodies;
				d->body[d->numbodies].bone = boneidx;
				d->numbodies++;
			}
			else if (!errors++)
				Con_Printf("Unable to create body \"%s\" because bone \"%s\" does not exist in \"%s\"\n", Cmd_Argv(1), Cmd_Argv(2), mod->name);
		}
		else if (!stricmp(cmd, "joint"))
		{
			odejointinfo_t *joint;
			char *name;
			d->joint = realloc(d->joint, sizeof(*d->joint)*(d->numjoints+1));
			joint = &d->joint[d->numjoints];
			memset(joint, 0, sizeof(*joint));
			Q_strncpyz(joint->name, Cmd_Argv(1), sizeof(joint->name));
			name = Cmd_Argv(2);
			joint->body1 = *name?rag_finddollbody(d, name):-1;
			if (*name && joint->body1 < 0 && !errors++)
			{
				Con_Printf("Joint \"%s\" joints invalid body \"%s\" in \"%s\"\n", joint->name, name, fname);
				continue;
			}
			name = Cmd_Argv(3);
			joint->body2 = *name?rag_finddollbody(d, name):-1;
			if (*name && (joint->body2 < 0 || joint->body2 == joint->body1) && !errors++)
			{
				if (joint->body2 == joint->body1)
					Con_Printf("Joint \"%s\" joints body \"%s\" to itself in \"%s\"\n", joint->name, name, fname);
				else
					Con_Printf("Joint \"%s\" joints invalid body \"%s\" in \"%s\"\n", joint->name, name, fname);
				continue;
			}
			joint->orgmatrix[0] = 1;
			joint->orgmatrix[4] = 1;
			joint->orgmatrix[8] = 1;
			joint->bonepivot = d->body[(joint->body2 >= 0)?joint->body2:joint->body1].bone;	//default the pivot object to the bone of the second object.

			joint->ERP = 0.4;
			joint->ERP2 = 0.4;
			joint->CFM = 0.1;
			joint->CFM2 = 0.1;

			if (joint->body1 >= 0 || joint->body2 >= 0)
				d->numjoints++;
			else if (!errors++)
				Con_Printf("Joint property \"%s\" not recognised in \"%s\"\n", joint->name, fname);
		}
		else if (!stricmp(cmd, "setjoint"))
		{
			int j = rag_finddolljoint(d, Cmd_Argv(1));
			if (j >= 0)
			{
				odejointinfo_t *joint = &d->joint[j];
				char *prop = Cmd_Argv(2);
				char *val = Cmd_Argv(3);
				if (!stricmp(prop, "type"))
				{
					if (!stricmp(val, "fixed"))
						joint->type = JOINTTYPE_FIXED;
					else if (!stricmp(val, "point"))
						joint->type = JOINTTYPE_POINT;
					else if (!stricmp(val, "hinge"))
						joint->type = JOINTTYPE_HINGE;
					else if (!stricmp(val, "slider"))
						joint->type = JOINTTYPE_SLIDER;
					else if (!stricmp(val, "universal"))
						joint->type = JOINTTYPE_UNIVERSAL;
					else if (!stricmp(val, "hinge2"))
						joint->type = JOINTTYPE_HINGE2;
				}
				else if (!stricmp(prop, "ERP"))
					joint->ERP = atof(val);
				else if (!stricmp(prop, "ERP2"))
					joint->ERP2 = atof(val);
				else if (!stricmp(prop, "CFM"))
					joint->CFM = atof(val);
				else if (!stricmp(prop, "CFM2"))
					joint->CFM2 = atof(val);
				else if (!stricmp(prop, "FMax"))
					joint->FMax = atof(val);
				else if (!stricmp(prop, "FMax2"))
					joint->FMax2 = atof(val);
				else if (!stricmp(prop, "HiStop"))
					joint->HiStop = atof(val);
				else if (!stricmp(prop, "HiStop2"))
					joint->HiStop2 = atof(val);
				else if (!stricmp(prop, "LoStop"))
					joint->LoStop = atof(val);
				else if (!stricmp(prop, "LoStop2"))
					joint->LoStop2 = atof(val);
				else if (!stricmp(prop, "origin") || !stricmp(prop, "pivot"))
				{
					//the origin is specified in base-frame model space
					//we need to make it relative to the joint's bodies
					float omat[12] = {	1,	0,	0,	atoi(Cmd_Argv(3)),
										0,	1,	0,	atoi(Cmd_Argv(4)),
										0,	0,	1,	atoi(Cmd_Argv(5))};
					char *bone = Cmd_Argv(6);
					i = Mod_TagNumForName(d->model, Cmd_Argv(2))-1;
					joint->bonepivot = i;
					Matrix3x4_Multiply(omat, bones[i].inverse, joint->orgmatrix);
				}
				else if (!errors++)
					Con_Printf("Joint property \"%s\" not recognised in \"%s\"\n", prop, fname);
			}
			else if (!errors++)
				Con_Printf("Joint \"%s\" not yet defined in \"%s\"\n", Cmd_Argv(1), fname);
		}
	}
	FS_FreeFile(fptr);
	return d;
}
void rag_freedoll(doll_t *doll)
{
	free(doll->bone);
	free(doll->body);
	free(doll->joint);
	free(doll);
}

void rag_flushdolls(void)
{
	doll_t *d, **link;
	for (link = &dolllist; *link; )
	{
		d = *link;
		if (!d->uses)
		{
			*link = d->next;
			rag_freedoll(d);
		}
		else
			link = &(*link)->next;
	}
}

void skel_integrate(progfuncs_t *prinst, skelobject_t *sko, skelobject_t *skelobjsrc, float ft, float mmat[12])
{
#if 0
	trace_t t;
	vec3_t npos, opos, wnpos, wopos;
	vec3_t move;
	float wantmat[12];
	world_t *w = prinst->parms->user;
	body_t *b;
	float gravity = 800;
	int bone, bno;
	int boffs;
	galiasbone_t *boneinfo = Mod_GetBoneInfo(sko->model);
	if (!boneinfo)
		return;

	for (bone = 0, bno = 0, b = sko->body; bone < sko->numbones; bone++)
	{
		boffs = bone*12;
		/*if this bone is positioned using a physical body...*/
		if (bno < sko->numbodies && b->jointo == boffs)
		{
			if (skelobjsrc)
			{
				/*attempt to move to target*/
				if (boneinfo[bone].parent >= 0)
				{
					Matrix3x4_Multiply(skelobjsrc->bonematrix+boffs, sko->bonematrix+12*boneinfo[bone].parent, wantmat);
				}
				else
				{
					Vector4Copy(skelobjsrc->bonematrix+boffs+0, wantmat+0);
					Vector4Copy(skelobjsrc->bonematrix+boffs+4, wantmat+4);
					Vector4Copy(skelobjsrc->bonematrix+boffs+8, wantmat+8);
				}
				b->vel[0] = (wantmat[3 ] - sko->bonematrix[b->jointo + 3 ])/ft;
				b->vel[1] = (wantmat[7 ] - sko->bonematrix[b->jointo + 7 ])/ft;
				b->vel[2] = (wantmat[11] - sko->bonematrix[b->jointo + 11])/ft;
			}
			else
			{
				/*handle gravity*/
				b->vel[2] = b->vel[2] - gravity * ft / 2;
			}

			VectorScale(b->vel, ft, move);

			opos[0] = sko->bonematrix[b->jointo + 3 ];
			opos[1] = sko->bonematrix[b->jointo + 7 ];
			opos[2] = sko->bonematrix[b->jointo + 11];
			npos[0] = opos[0] + move[0];
			npos[1] = opos[1] + move[1];
			npos[2] = opos[2] + move[2];

			Matrix3x4_RM_Transform3(mmat, opos, wopos);
			Matrix3x4_RM_Transform3(mmat, npos, wnpos);

			t = World_Move(w, wopos, vec3_origin, vec3_origin, wnpos, MOVE_NOMONSTERS, w->edicts);
			if (t.startsolid)
				t.fraction = 1;
			else if (t.fraction < 1)
			{
				/*clip the velocity*/
				float backoff = -DotProduct (b->vel, t.plane.normal) * 1.9; /*teehee, bouncy corpses*/
				VectorMA(b->vel, backoff, t.plane.normal, b->vel);
			}
			if (skelobjsrc)
			{
				VectorCopy(wantmat+0, sko->bonematrix+b->jointo+0);
				VectorCopy(wantmat+4, sko->bonematrix+b->jointo+4);
				VectorCopy(wantmat+8, sko->bonematrix+b->jointo+8);
			}

			sko->bonematrix[b->jointo + 3 ] += move[0] * t.fraction;
			sko->bonematrix[b->jointo + 7 ] += move[1] * t.fraction;
			sko->bonematrix[b->jointo + 11] += move[2] * t.fraction;

			if (!skelobjsrc)
				b->vel[2] = b->vel[2] - gravity * ft / 2;

			b++;
			bno++;
		}
		else if (skelobjsrc)
		{
			/*directly copy animation skeleton*/
			if (boneinfo[bone].parent >= 0)
			{
				Matrix3x4_Multiply(skelobjsrc->bonematrix+boffs, sko->bonematrix+12*boneinfo[bone].parent, sko->bonematrix+boffs);
			}
			else
			{
				Vector4Copy(skelobjsrc->bonematrix+boffs+0, sko->bonematrix+boffs+0);
				Vector4Copy(skelobjsrc->bonematrix+boffs+4, sko->bonematrix+boffs+4);
				Vector4Copy(skelobjsrc->bonematrix+boffs+8, sko->bonematrix+boffs+8);
			}
		}
		else
		{
			/*retain the old relation*/
			/*FIXME*/
		}
	}
#endif
	/*debugging*/
#if 0
	/*draw points*/
	for (bone = 0; p < sko->numbones; bone++)
	{
		opos[0] = sko->bonematrix[bone*12 + 3 ];
		opos[1] = sko->bonematrix[bone*12 + 7 ];
		opos[2] = sko->bonematrix[bone*12 + 11];
		Matrix3x4_RM_Transform3(mmat, opos, wopos);
		P_RunParticleEffectTypeString(wopos, vec3_origin, 1, "ragdolltest");
	}
#endif
}
#endif

/*destroys all skeletons*/
void skel_reset(progfuncs_t *prinst)
{
	int i;

	for (i = 0; i < numskelobjectsused; i++)
	{
		if (skelobjects[i].world = prinst->parms->user)
		{
			rag_uninstanciate(&skelobjects[i]);
			skelobjects[i].numbones = 0;
			skelobjects[i].inuse = false;
			skelobjects[i].bonematrix = NULL;
		}
	}

	while (numskelobjectsused && !skelobjects[numskelobjectsused-1].inuse)
		numskelobjectsused--;
	rag_flushdolls();
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
		{
			rag_uninstanciate(&skelobjects[skelidx]);
			skelobjects[skelidx].inuse = 0;
		}
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
			if (!skelobjects[skelidx].inuse && skelobjects[skelidx].numbones == bonecount && skelobjects[skelidx].world == prinst->parms->user)
				return &skelobjects[skelidx];
		}

		for (skelidx = 0; skelidx <= MAX_SKEL_OBJECTS; skelidx++)
		{
			if (!skelobjects[skelidx].inuse &&
				(!skelobjects[skelidx].numbones || skelobjects[skelidx].numbones == bonecount) &&
				(!skelobjects[skelidx].world || skelobjects[skelidx].world == prinst->parms->user))
			{
				if (!skelobjects[skelidx].numbones)
				{
					skelobjects[skelidx].numbones = bonecount;
					skelobjects[skelidx].bonematrix = (float*)PR_AddString(prinst, "", sizeof(float)*12*bonecount);
				}
				skelobjects[skelidx].world = prinst->parms->user;
				if (numskelobjectsused <= skelidx)
					numskelobjectsused = skelidx + 1;
				skelobjects[skelidx].model = NULL;
				skelobjects[skelidx].inuse = 1;
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
		out->boneabs = sko->type;
		out->bonecount = sko->numbones;
		out->bonestate = sko->bonematrix;
	}
}

void QCBUILTIN PF_skel_mmap(progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	int skelidx = G_FLOAT(OFS_PARM0);
	skelobject_t *sko = skel_get(prinst, skelidx, 0);
	if (!sko || sko->world != prinst->parms->user)
		G_INT(OFS_RETURN) = 0;
	else
		G_INT(OFS_RETURN) = (char*)sko->bonematrix - prinst->stringtable;
}

//may not poke the skeletal object bone data.
void rag_uninstanciate(skelobject_t *sko)
{
	int i;
	if (!sko->doll)
		return;

	for (i = 0; i < sko->numbodies; i++)
	{
		World_ODE_RagDestroyBody(sko->world, &sko->body[i].odebody);
	}
	free(sko->body);
	sko->body = NULL;
	sko->numbodies = 0;

	for (i = 0; i < sko->numjoints; i++)
	{
		World_ODE_RagDestroyJoint(sko->world, &sko->joint[i]);
	}
	free(sko->joint);
	sko->joint = NULL;
	sko->numjoints = 0;

	sko->doll->uses--;
	sko->doll = NULL;
}
qboolean rag_instanciate(skelobject_t *sko, doll_t *doll, float *emat, wedict_t *ent)
{
	int i;
	vec3_t org, porg;
	float *bmat;
	float bodymat[12], worldmat[12];
	vec3_t aaa2[3];
	galiasbone_t *bones = Mod_GetBoneInfo(sko->model);
	int bone;
	odebody_t *body1, *body2;
	odejointinfo_t *j;
	sko->numbodies = doll->numbodies;
	sko->body = malloc(sizeof(*sko->body) * sko->numbodies);
	sko->doll = doll;
	doll->uses++;
	for (i = 0; i < sko->numbodies; i++)
	{
		memset(&sko->body[i], 0, sizeof(sko->body[i]));

		bone = doll->body[i].bone;
		bmat = sko->bonematrix + bone*12;
		R_ConcatTransforms((void*)emat, (void*)bmat, (void*)bodymat);
		if (!World_ODE_RagCreateBody(sko->world, &sko->body[i].odebody, bodymat, ent))
			return false;
	}
	sko->numjoints = doll->numjoints;
	sko->joint = malloc(sizeof(*sko->joint) * sko->numjoints);
	memset(sko->joint, 0, sizeof(*sko->joint) * sko->numjoints);
	for(i = 0; i < sko->numjoints; i++)
	{
		j = &doll->joint[i];
		body1 = j->body1>=0?&sko->body[j->body1].odebody:NULL;
		body2 = j->body2>=0?&sko->body[j->body2].odebody:NULL;

		bone = j->bonepivot;
		bmat = sko->bonematrix + bone*12;

		R_ConcatTransforms(bmat, j->orgmatrix, bodymat);
		R_ConcatTransforms((void*)emat, (void*)bodymat, (void*)worldmat);
		aaa2[0][0] = worldmat[3];
		aaa2[0][1] = worldmat[3+4];
		aaa2[0][2] = worldmat[3+8];
//		P_RunParticleEffectTypeString(aaa2[0], vec3_origin, 1, "te_spike");

		aaa2[1][0] = 1;
		aaa2[1][1] = 0;
		aaa2[1][2] = 0;
		
		aaa2[2][0] = 0;
		aaa2[2][1] = 1;
		aaa2[2][2] = 0;

//		VectorCopy(j->offset, aaa2[0]);	//fixme: transform these vectors into world space, and transform to match the current positions of the bones.
//		VectorCopy(j->axis, aaa2[1]);
//		VectorCopy(j->axis2, aaa2[2]);
		World_ODE_RagCreateJoint(sko->world, &sko->joint[i], j, body1, body2, aaa2);
	}
	return true;
}
void rag_derive(skelobject_t *sko, skelobject_t *asko, float *emat)
{
	doll_t *doll = sko->doll;
	float *bmat = sko->bonematrix;
	float *amat = asko?asko->bonematrix:NULL;
	galiasbone_t *bones = Mod_GetBoneInfo(sko->model);
	int i;
	float invemat[12];
	float bodymat[12];

	Matrix3x4_Invert(emat, invemat);

	for (i = 0; i < doll->numbones; i++)
	{
		if (doll->bone[i].bodyidx >= 0)
		{
			World_ODE_RagMatrixFromBody(sko->world, &sko->body[doll->bone[i].bodyidx].odebody, bodymat);
			//that body matrix is in world space, so transform to model space for our result
			R_ConcatTransforms((void*)invemat, (void*)bodymat, (void*)((float*)bmat+i*12));
		}
		else if (amat)
		{
			//this bone has no joint object, use the anim sko's relative pose info instead
			if (bones[i].parent >= 0)
				R_ConcatTransforms((void*)(bmat + bones[i].parent*12), (void*)((float*)amat+i*12), (void*)((float*)bmat+i*12));
			else
				memcpy((void*)((float*)bmat+i*12), (void*)((float*)amat+i*12), sizeof(float)*12);
		}
	}
}

//update a skeletal object to track its ragdoll/apply a ragdoll to a skeletal object.
void QCBUILTIN PF_skel_ragedit(progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	//do we want to be able to generate a ragdoll object with this function too?
#ifdef RAGDOLL
	wedict_t *wed = (wedict_t*)G_EDICT(prinst, OFS_PARM0);
	char *ragname = PR_GetStringOfs(prinst, OFS_PARM1);
	int parentskel = G_FLOAT(OFS_PARM2);
	int skelidx = wed->xv->skeletonindex;
	skelobject_t *sko, *psko;
	doll_t *doll;
	int i;
	float emat[12];
	extern cvar_t temp1;

	{
		vec3_t d[3], a;
		//fixme: respond to renderflags&USEAXIS? scale?
		a[0] = wed->v->angles[0] * -1; /*mod_alias bug*/
		a[1] = wed->v->angles[1];
		a[2] = wed->v->angles[2];
		AngleVectors(a, d[0], d[1], d[2]);
		bonemat_fromqcvectors(emat, d[0], d[1], d[2], wed->v->origin);
		skelidx = wed->xv->skeletonindex;
	}

	G_FLOAT(OFS_RETURN) = 0;

	//the parent skeletal object must be relative, if specified.
	psko = skel_get(prinst, parentskel, 0);
	if (psko && psko->type != SKOT_RELATIVE)
		return;

	sko = skel_get(prinst, skelidx, 0);
	if (!sko)
	{
		Con_DPrintf("PF_skel_ragedit: invalid skeletal object\n");
		return;
	}

	if (!sko->world->ode.ode)
	{
		Con_DPrintf("PF_skel_ragedit: ODE not enabled\n");
		return;
	}

	if (*ragname)
	{
		if (sko->doll && !strcmp(sko->doll->name, ragname))
			doll = sko->doll;
		else
			doll = rag_loaddoll(sko->model, ragname, sko->numbones);
		if (!doll)
		{
			Con_DPrintf("PF_skel_ragedit: invalid doll\n");
			return;
		}
	}
	else
	{
		/*no doll name makes it revert to a normal skeleton*/
		rag_uninstanciate(sko);
		G_FLOAT(OFS_RETURN) = 1;
		return;
	}

	if (sko->type != SKOT_ABSOLUTE)
	{
		float tmp[12];
		float *bmat = sko->bonematrix;
		galiasbone_t *bones = Mod_GetBoneInfo(sko->model);
		for (i = 0; i < sko->numbones; i++)
		{
			//bones without parents are technically already absolute
			if (bones[i].parent >= 0)
			{
				//write to a tmp to avoid premature clobbering
				R_ConcatTransforms((void*)(bmat + bones[i].parent*12), (void*)((float*)bmat+i*12), (void*)tmp);
				memcpy((void*)(bmat+i*12), tmp, sizeof(tmp));
			}
		}
		sko->type = SKOT_ABSOLUTE;
	}

	if (sko->doll != doll || temp1.ival)
	{
		rag_uninstanciate(sko);
		if (!rag_instanciate(sko, doll, emat, wed))
		{
			rag_uninstanciate(sko);
			Con_DPrintf("PF_skel_ragedit: unable to instanciate objects\n");
			G_FLOAT(OFS_RETURN) = 0;
			return;
		}
	}

	rag_derive(sko, psko, emat);
//	skel_integrate(prinst, sko, psko, host_frametime, emat);

	G_FLOAT(OFS_RETURN) = 1;
#endif
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
	int i;

	midx = G_FLOAT(OFS_PARM0);
	type = (*prinst->callargc > 1)?G_FLOAT(OFS_PARM1):SKOT_RELATIVE;

	//default to failure
	G_FLOAT(OFS_RETURN) = 0;

	model = w->Get_CModel(w, midx);
	if (!model)
		return; //no model set, can't get a skeleton

	numbones = Mod_GetNumBones(model, type != SKOT_RELATIVE);
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

	/*
	for (i = 0; i < numbones; i++)
	{
		galiasbone_t *bones = Mod_GetBoneInfo(skelobj->model);
		Matrix3x4_Invert_Simple(bones[i].inverse, skelobj->bonematrix + i*12);
	}
	skelobj->type = SKOT_ABSOLUTE;
	*/

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
	float addition = (*prinst->callargc>6)?G_FLOAT(OFS_PARM6):1-retainfrac;

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

	if (skelobj->type != SKOT_RELATIVE)
	{
		if (firstbone > 0 || lastbone < skelobj->numbones || retainfrac)
		{
			Con_Printf("skel_build on non-relative skeleton\n");
			return;
		}
		skelobj->type = SKOT_RELATIVE;	//entire model will get replaced, convert it.
	}

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

//vector(float skel, float bonenum) skel_get_bonerel (FTE_CSQC_SKELETONOBJECTS) (sets v_forward etc)
void QCBUILTIN PF_skel_get_bonerel (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	world_t *w = prinst->parms->user;
	int skelidx = G_FLOAT(OFS_PARM0);
	int boneidx = G_FLOAT(OFS_PARM1)-1;
	skelobject_t *skelobj = skel_get(prinst, skelidx, 0);
	if (!skelobj || (unsigned int)boneidx >= skelobj->numbones)
		bonematident_toqcvectors(w->g.v_forward, w->g.v_right, w->g.v_up, G_VECTOR(OFS_RETURN));
	else if (skelobj->type!=SKOT_RELATIVE)
	{
		float tmp[12];
		float invparent[12];
		int parent;
		/*invert the parent, multiply that against the child, we now know the transform required to go from parent to child. woo.*/
		parent = Mod_GetBoneParent(skelobj->model, boneidx+1)-1;
		Matrix3x4_Invert(skelobj->bonematrix+12*parent, invparent);
		Matrix3x4_Multiply(invparent, skelobj->bonematrix+12*boneidx, tmp);
		bonemat_toqcvectors(tmp, w->g.v_forward, w->g.v_right, w->g.v_up, G_VECTOR(OFS_RETURN));
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
	else if (skelobj->type != SKOT_RELATIVE)
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

//void(entity ent, float bonenum, vector org, optional fwd, right, up) skel_set_bone_world (FTE_CSQC_SKELETONOBJECTS2) (reads v_forward etc)
void QCBUILTIN PF_skel_set_bone_world (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	world_t *w = prinst->parms->user;
	wedict_t *ent = G_WEDICT(prinst, OFS_PARM0);
	unsigned int boneidx = G_FLOAT(OFS_PARM1)-1;
	float *matrix[3];
	skelobject_t *skelobj;
	float *bone;
	float childworld[12], parentinv[12];

	/*sort out the parameters*/
	if (*prinst->callargc == 4)
	{
		vec3_t d[3], a;
		a[0] = G_VECTOR(OFS_PARM3)[0] * -1; /*mod_alias bug*/
		a[1] = G_VECTOR(OFS_PARM3)[1];
		a[2] = G_VECTOR(OFS_PARM3)[2];
		AngleVectors(a, d[0], d[1], d[2]);
		bonemat_fromqcvectors(childworld, d[0], d[1], d[2], G_VECTOR(OFS_PARM2));
	}
	else
	{
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
		bonemat_fromqcvectors(childworld, matrix[0], matrix[1], matrix[2], G_VECTOR(OFS_PARM2));
	}

	/*make sure the skeletal object is correct*/
	skelobj = skel_get(prinst, ent->xv->skeletonindex, 0);
	if (!skelobj || boneidx >= skelobj->numbones)
		return;

	/*get the inverse of the parent matrix*/
	{
		float parentabs[12];
		float parentw[12];
		float parentent[12];
		framestate_t fstate;
		w->Get_FrameState(w, ent, &fstate);
		if (skelobj->type == SKOT_ABSOLUTE || !Mod_GetTag(skelobj->model, Mod_GetBoneParent(skelobj->model, boneidx+1), &fstate, parentabs))
		{
			bonemat_fromentity(w, ent, parentw);
		}
		else
		{
			bonemat_fromentity(w, ent, parentent);
			Matrix3x4_Multiply(parentabs, parentent, parentw);
		}
		Matrix3x4_Invert(parentw, parentinv);
	}

	/*calc the result*/
	bone = skelobj->bonematrix+12*boneidx;
	Matrix3x4_Multiply(childworld, parentinv, bone);
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
	bonemat_fromqcvectors(bone, matrix[0], matrix[1], matrix[2], G_VECTOR(OFS_PARM2));
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
	if (endbone == -1)
		endbone = skelobj->numbones;
	else if (endbone > skelobj->numbones)
		endbone = skelobj->numbones;

	while(startbone < endbone)
	{
		Vector4Copy(skelobj->bonematrix+12*startbone+0, temp[0]);
		Vector4Copy(skelobj->bonematrix+12*startbone+4, temp[1]);
		Vector4Copy(skelobj->bonematrix+12*startbone+8, temp[2]);
		R_ConcatTransforms(mult, temp, (float(*)[4])(skelobj->bonematrix+12*startbone));

		startbone++;
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
	if (startbone == -1)
		startbone = 0;
	if (endbone == -1)
		endbone = skelobjdst->numbones;
	if (endbone > skelobjdst->numbones)
		endbone = skelobjdst->numbones;
	if (endbone > skelobjsrc->numbones)
		endbone = skelobjsrc->numbones;

	if (skelobjsrc->type == skelobjdst->type)
	{
		while(startbone < endbone)
		{
			Vector4Copy(skelobjsrc->bonematrix+12*startbone+0, skelobjdst->bonematrix+12*startbone+0);
			Vector4Copy(skelobjsrc->bonematrix+12*startbone+4, skelobjdst->bonematrix+12*startbone+4);
			Vector4Copy(skelobjsrc->bonematrix+12*startbone+8, skelobjdst->bonematrix+12*startbone+8);

			startbone++;
		}
	}
	else if (skelobjsrc->type == SKOT_RELATIVE && skelobjdst->type == SKOT_ABSOLUTE)
	{
		/*copy from relative to absolute*/

		galiasbone_t *boneinfo = Mod_GetBoneInfo(skelobjsrc->model);
		if (!boneinfo)
			return;
		while(startbone < endbone)
		{
			if (boneinfo[startbone].parent >= 0)
			{
				Matrix3x4_Multiply(skelobjsrc->bonematrix+12*startbone, skelobjdst->bonematrix+12*boneinfo[startbone].parent, skelobjdst->bonematrix+12*startbone);
			}
			else
			{
				Vector4Copy(skelobjsrc->bonematrix+12*startbone+0, skelobjdst->bonematrix+12*startbone+0);
				Vector4Copy(skelobjsrc->bonematrix+12*startbone+4, skelobjdst->bonematrix+12*startbone+4);
				Vector4Copy(skelobjsrc->bonematrix+12*startbone+8, skelobjdst->bonematrix+12*startbone+8);
			}

			startbone++;
		}
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
		skelobj->model = NULL;
		pendingkill = true;
	}
}

//vector(entity ent, float tag) gettaginfo (DP_MD3_TAGSINFO)
void QCBUILTIN PF_gettaginfo (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	world_t *w = prinst->parms->user;
	wedict_t *ent = G_WEDICT(prinst, OFS_PARM0);
	int tagnum = G_FLOAT(OFS_PARM1);

	int modelindex = ent->v->modelindex;

	model_t *mod = w->Get_CModel(w, modelindex);

	float transent[12];
	float transforms[12];
	float result[12];

	framestate_t fstate;

	w->Get_FrameState(w, ent, &fstate);

	if (!Mod_GetTag(mod, tagnum, &fstate, transforms))
	{
		bonemat_fromidentity(transforms);
	}

	if (ent->xv->tag_entity)
	{
#ifdef warningmsg
		#pragma warningmsg("PF_gettaginfo: This function doesn't honour attachments")
#endif
		Con_Printf("bug: PF_gettaginfo doesn't support attachments\n");
	}

	bonemat_fromentity(w, ent, transent);
	R_ConcatTransforms((void*)transent, (void*)transforms, (void*)result);

	bonemat_toqcvectors(result, w->g.v_forward, w->g.v_right, w->g.v_up, G_VECTOR(OFS_RETURN));
}

//vector(entity ent, string tagname) gettagindex (DP_MD3_TAGSINFO)
void QCBUILTIN PF_gettagindex (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	world_t *w = prinst->parms->user;
	wedict_t *ent = G_WEDICT(prinst, OFS_PARM0);
	char *tagname = PR_GetStringOfs(prinst, OFS_PARM1);
	model_t *mod = *tagname?w->Get_CModel(w, ent->v->modelindex):NULL;
	if (mod)
		G_FLOAT(OFS_RETURN) = Mod_TagNumForName(mod, tagname);
	else
		G_FLOAT(OFS_RETURN) = 0;
}

const char *Mod_FrameNameForNum(model_t *model, int num);
const char *Mod_SkinNameForNum(model_t *model, int num);

//string(float modidx, float framenum) frametoname
void QCBUILTIN PF_frametoname (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	world_t *w = prinst->parms->user;
	unsigned int modelindex = G_FLOAT(OFS_PARM0);
	unsigned int skinnum = G_FLOAT(OFS_PARM1);
	model_t *mod = w->Get_CModel(w, modelindex);
	const char *n = Mod_FrameNameForNum(mod, skinnum);

	if (n)
		RETURN_TSTRING(n);
	else
		G_INT(OFS_RETURN) = 0;	//null string (which is also empty in qc)
}

//string(float modidx, float skinnum) skintoname
void QCBUILTIN PF_skintoname (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	world_t *w = prinst->parms->user;
	unsigned int modelindex = G_FLOAT(OFS_PARM0);
	unsigned int skinnum = G_FLOAT(OFS_PARM1);
	model_t *mod = w->Get_CModel(w, modelindex);
	const char *n = Mod_SkinNameForNum(mod, skinnum);

	if (n)
		RETURN_TSTRING(n);
	else
		G_INT(OFS_RETURN) = 0;	//null string (which is also empty in qc)
}

void QCBUILTIN PF_frameforname (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	world_t *w = prinst->parms->user;
	unsigned int modelindex = G_FLOAT(OFS_PARM0);
	char *str = PF_VarString(prinst, 1, pr_globals);
	model_t *mod = w->Get_CModel(w, modelindex);

	if (mod && Mod_FrameForName)
		G_FLOAT(OFS_RETURN) = Mod_FrameForName(mod, str);
	else
		G_FLOAT(OFS_RETURN) = -1;
}
void QCBUILTIN PF_frameduration (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	world_t *w = prinst->parms->user;
	unsigned int modelindex = G_FLOAT(OFS_PARM0);
	unsigned int framenum = G_FLOAT(OFS_PARM1);
	model_t *mod = w->Get_CModel(w, modelindex);

	if (mod && Mod_GetFrameDuration)
		G_FLOAT(OFS_RETURN) = Mod_GetFrameDuration(mod, framenum);
	else
		G_FLOAT(OFS_RETURN) = 0;
}
void QCBUILTIN PF_skinforname (progfuncs_t *prinst, struct globalvars_s *pr_globals)
{
#ifndef SERVERONLY
	world_t *w = prinst->parms->user;
	unsigned int modelindex = G_FLOAT(OFS_PARM0);
	char *str = PF_VarString(prinst, 1, pr_globals);
	model_t *mod = w->Get_CModel(w, modelindex);

	if (mod && Mod_SkinForName)
		G_FLOAT(OFS_RETURN) = Mod_SkinForName(mod, str);
	else
#endif
		G_FLOAT(OFS_RETURN) = -1;
}
#endif

