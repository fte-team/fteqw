#include "quakedef.h"

#ifdef HALFLIFEMODELS

#include "shader.h"
/*
 +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
    Half-Life Model Renderer (Experimental) Copyright (C) 2001 James 'Ender' Brown [ender@quakesrc.org] This program is
    free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation; either version 2 of the License, or (at your option) any later version.
    This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied
    warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more
    details. You should have received a copy of the GNU General Public License along with this program; if not, write
    to the Free Software Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA. fromquake.h -

	render.c - apart from calculations (mostly range checking or value conversion code is a mix of standard Quake 1
	meshing, and vertex deforms. The rendering loop uses standard Quake 1 drawing, after SetupBones deforms the vertex.
 +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++



  Also, please note that it won't do all hl models....
  Nor will it work 100%
 */
#include "model_hl.h"

void QuaternionGLMatrix(float x, float y, float z, float w, vec4_t *GLM)
{
    GLM[0][0] = 1 - 2 * y * y - 2 * z * z;
    GLM[1][0] = 2 * x * y + 2 * w * z;
    GLM[2][0] = 2 * x * z - 2 * w * y;
    GLM[0][1] = 2 * x * y - 2 * w * z;
    GLM[1][1] = 1 - 2 * x * x - 2 * z * z;
    GLM[2][1] = 2 * y * z + 2 * w * x;
    GLM[0][2] = 2 * x * z + 2 * w * y;
    GLM[1][2] = 2 * y * z - 2 * w * x;
    GLM[2][2] = 1 - 2 * x * x - 2 * y * y;
}

/*
 =======================================================================================================================
    QuaternionGLAngle - Convert a GL angle to a quaternion matrix
 =======================================================================================================================
 */
void QuaternionGLAngle(const vec3_t angles, vec4_t quaternion)
{
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
    float	yaw = angles[2] * 0.5;
    float	pitch = angles[1] * 0.5;
    float	roll = angles[0] * 0.5;
    float	siny = sin(yaw);
    float	cosy = cos(yaw);
    float	sinp = sin(pitch);
    float	cosp = cos(pitch);
    float	sinr = sin(roll);
    float	cosr = cos(roll);
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

    quaternion[0] = sinr * cosp * cosy - cosr * sinp * siny;
    quaternion[1] = cosr * sinp * cosy + sinr * cosp * siny;
    quaternion[2] = cosr * cosp * siny - sinr * sinp * cosy;
    quaternion[3] = cosr * cosp * cosy + sinr * sinp * siny;
}

matrix3x4 transform_matrix[MAX_BONES];	/* Vertex transformation matrix */

void GL_Draw_HL_AliasFrame(short *order, vec3_t *transformed, float tex_w, float tex_h);

/*
 =======================================================================================================================
    Mod_LoadHLModel - read in the model's constituent parts
 =======================================================================================================================
 */
qboolean QDECL Mod_LoadHLModel (model_t *mod, void *buffer, size_t fsize)
{
    /*~~*/
    int i;

	hlmodelcache_t *model;
	hlmdl_header_t *header;
	hlmdl_header_t *texheader;
	hlmdl_tex_t	*tex;
	hlmdl_bone_t	*bones;
	hlmdl_bonecontroller_t	*bonectls;
	struct hlmodelshaders_s *shaders;
	void *texmem = NULL;
    /*~~*/


	//load the model into hunk
	model = ZG_Malloc(&mod->memgroup, sizeof(hlmodelcache_t));

	header = ZG_Malloc(&mod->memgroup, fsize);
	memcpy(header, buffer, fsize);

#if defined(HLSERVER) && (defined(__powerpc__) || defined(__ppc__))
//this is to let bigfoot know when he comes to port it all... And I'm lazy.
#ifdef warningmsg
#pragma warningmsg("-----------------------------------------")
#pragma warningmsg("FIXME: No byteswapping on halflife models")
#pragma warningmsg("-----------------------------------------")
#endif
#endif

	if (header->version != 10)
	{
		Con_Printf(CON_ERROR "Cannot load model %s - unknown version %i\n", mod->name, header->version);
		return false;
	}

	if (header->numcontrollers > MAX_BONE_CONTROLLERS)
	{
		Con_Printf(CON_ERROR "Cannot load model %s - too many controllers %i\n", mod->name, header->numcontrollers);
		return false;
	}
	if (header->numbones > MAX_BONES)
	{
		Con_Printf(CON_ERROR "Cannot load model %s - too many bones %i\n", mod->name, header->numbones);
		return false;
	}

	texheader = NULL;
	if (!header->numtextures)
	{
		size_t fz;
		char texmodelname[MAX_QPATH];
		COM_StripExtension(mod->name, texmodelname, sizeof(texmodelname));
		Q_strncatz(texmodelname, "t.mdl", sizeof(texmodelname));
		//no textures? eesh. They must be stored externally.
		texheader = texmem = (hlmdl_header_t*)FS_LoadMallocFile(texmodelname, &fz);
		if (texheader)
		{
			if (texheader->version != 10)
				texheader = NULL;
		}
	}

	if (!texheader)
		texheader = header;
	else
		header->numtextures = texheader->numtextures;

	tex = (hlmdl_tex_t *) ((qbyte *) texheader + texheader->textures);
    bones = (hlmdl_bone_t *) ((qbyte *) header + header->boneindex);
    bonectls = (hlmdl_bonecontroller_t *) ((qbyte *) header + header->controllerindex);


/*	won't work - doesn't know exact sizes.

	header = Hunk_Alloc(sizeof(hlmdl_header_t));
	memcpy(header, (hlmdl_header_t *) buffer, sizeof(hlmdl_header_t));

	tex = Hunk_Alloc(sizeof(hlmdl_tex_t)*header->numtextures);
	memcpy(tex, (hlmdl_tex_t *) buffer, sizeof(hlmdl_tex_t)*header->numtextures);

	bones = Hunk_Alloc(sizeof(hlmdl_bone_t)*header->numtextures);
	memcpy(bones, (hlmdl_bone_t *) buffer, sizeof(hlmdl_bone_t)*header->numbones);

	bonectls = Hunk_Alloc(sizeof(hlmdl_bonecontroller_t)*header->numcontrollers);
	memcpy(bonectls, (hlmdl_bonecontroller_t *) buffer, sizeof(hlmdl_bonecontroller_t)*header->numcontrollers);
*/

	model->header = header;
	model->bones = bones;
	model->bonectls = bonectls;

	shaders = ZG_Malloc(&mod->memgroup, texheader->numtextures*sizeof(shader_t));
	model->shaders = shaders;
    for(i = 0; i < texheader->numtextures; i++)
    {
		Q_snprintfz(shaders[i].name, sizeof(shaders[i].name), "%s_%i.tga", mod->name, i);
		memset(&shaders[i].defaulttex, 0, sizeof(shaders[i].defaulttex));
		shaders[i].defaulttex.base = Image_GetTexture(shaders[i].name, "", IF_NOALPHA, (qbyte *) texheader + tex[i].offset, (qbyte *) texheader + tex[i].w * tex[i].h + tex[i].offset, tex[i].w, tex[i].h, TF_8PAL24);
		shaders[i].w = tex[i].w;
		shaders[i].h = tex[i].h;
    }

	model->numskins = texheader->numtextures;
	model->skins = ZG_Malloc(&mod->memgroup, model->numskins*sizeof(*model->skins));
	memcpy(model->skins, (short *) ((qbyte *) texheader + texheader->skins), model->numskins*sizeof(*model->skins));


	if (texmem)
		Z_Free(texmem);

	mod->type = mod_halflife;
	mod->meshinfo = model;
	return true;
}

#ifdef HLSERVER
void *Mod_GetHalfLifeModelData(model_t *mod)
{
	hlmodelcache_t *mc;
	if (!mod || mod->type != mod_halflife)
		return NULL;	//halflife models only, please

	mc = Mod_Extradata(mod);
	return (void*)mc->header;
}
#endif

int HLMod_FrameForName(model_t *mod, const char *name)
{
	int i;
	hlmdl_header_t *h;
	hlmdl_sequencelist_t *seqs;
	hlmodelcache_t *mc;
	if (!mod || mod->type != mod_halflife)
		return -1;	//halflife models only, please

	mc = Mod_Extradata(mod);

	h = mc->header;
	seqs = (hlmdl_sequencelist_t*)((char*)h+h->seqindex);

	for (i = 0; i < h->numseq; i++)
	{
		if (!strcmp(seqs[i].name, name))
			return i;
	}
	return -1;
}

int HLMod_BoneForName(model_t *mod, char *name)
{
	int i;
	hlmdl_header_t *h;
	hlmdl_bone_t *bones;
	hlmodelcache_t *mc;
	if (!mod || mod->type != mod_halflife)
		return -1;	//halflife models only, please

	mc = Mod_Extradata(mod);

	h = mc->header;
	bones = (hlmdl_bone_t*)((char*)h+h->boneindex);

	for (i = 0; i < h->numbones; i++)
	{
		if (!strcmp(bones[i].name, name))
			return i+1;
	}
	return 0;
}

/*
 =======================================================================================================================
    HL_CalculateBones - calculate bone positions - quaternion+vector in one function
 =======================================================================================================================
 */
void HL_CalculateBones
(
	int				frame,
	vec4_t			adjust,
	hlmdl_bone_t	*bone,
	hlmdl_anim_t	*animation,
	float			*organg
)
{
	int		i;

	/* For each vector */
	for(i = 0; i < 6; i++)
	{
		organg[i] = bone->value[i];	/* Take the bone value */

		if(animation->offset[i] != 0)
		{
			/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
			int					tempframe;
			hlmdl_animvalue_t	*animvalue = (hlmdl_animvalue_t *) ((qbyte *) animation + animation->offset[i]);
			/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

			/* find values including the required frame */
			tempframe = frame;
			while(animvalue->num.total <= tempframe)
			{
				tempframe -= animvalue->num.total;
				animvalue += animvalue->num.valid + 1;
			}
			if (tempframe >= animvalue->num.valid)
				tempframe = animvalue->num.valid;
			else
				tempframe += 1;

			organg[i] += animvalue[tempframe].value * bone->scale[i];
		}

		if(bone->bonecontroller[i] != -1)
		{	/* Add the programmable offset. */
			organg[i] += adjust[bone->bonecontroller[i]];
		}
	}
}

/*
 =======================================================================================================================
    HL_CalcBoneAdj - Calculate the adjustment values for the programmable controllers
 =======================================================================================================================
 */
void HL_CalcBoneAdj(hlmodel_t *model)
{
	/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
	int						i;
	float					value;
	hlmdl_bonecontroller_t	*control = (hlmdl_bonecontroller_t *)
									  ((qbyte *) model->header + model->header->controllerindex);
	/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

	for(i = 0; i < model->header->numcontrollers; i++)
	{
		/*~~~~~~~~~~~~~~~~~~~~~*/
		int j = control[i].index;
		/*~~~~~~~~~~~~~~~~~~~~~*/

		if(control[i].type & 0x8000)
		{
			value = model->controller[j] + control[i].start;
		}
		else
		{
			value = (model->controller[j]+1)*0.5;	//shifted to give a valid range between -1 and 1, with 0 being mid-range.
			if(value < 0)
				value = 0;
			else if(value > 1.0)
				value = 1.0;
			value = (1.0 - value) * control[i].start + value * control[i].end;
		}

		/* Rotational controllers need their values converted */
		if(control[i].type >= 0x0008 && control[i].type <= 0x0020)
			model->adjust[i] = M_PI * value / 180;
		else
			model->adjust[i] = value;
	}
}

/*
 =======================================================================================================================
    HL_SetupBones - determine where vertex should be using bone movements
 =======================================================================================================================
 */
void QuaternionSlerp( const vec4_t p, vec4_t q, float t, vec4_t qt );
void HL_SetupBones(hlmodel_t *model, int seqnum, int firstbone, int lastbone, float subblendfrac, float frametime)
{
	/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
	int						i;
	float					matrix[3][4];
	vec3_t					organg1[2];
	vec3_t					organg2[2];
	vec3_t					organgb[2];
	vec4_t					quat1, quat2, quatb;

	int frame1, frame2;

	hlmdl_sequencelist_t	*sequence = (hlmdl_sequencelist_t *) ((qbyte *) model->header + model->header->seqindex) +
										 ((unsigned int)seqnum>=model->header->numseq?0:seqnum);
	hlmdl_sequencedata_t	*sequencedata = (hlmdl_sequencedata_t *)
										 ((qbyte *) model->header + model->header->seqgroups) +
										 sequence->seqindex;
	hlmdl_anim_t			*animation;
	/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

	if (sequencedata->name[32])
	{
		size_t fz;
		if (sequence->seqindex >= MAX_ANIM_GROUPS)
		{
			Sys_Error("Too many animation sequence cache groups\n");
			return;
		}
		if (!model->animcache[sequence->seqindex])
			model->animcache[sequence->seqindex] = FS_LoadMallocGroupFile(model->memgroup, sequencedata->name+32, &fz);
		if (!model->animcache[sequence->seqindex] || model->animcache[sequence->seqindex]->magic != *(int*)"IDSQ" || model->animcache[sequence->seqindex]->version != 10)
		{
			Sys_Error("Unable to load %s\n", sequencedata->name+32);
			return;
		}
		animation = (hlmdl_anim_t *)((qbyte*)model->animcache[sequence->seqindex] + sequence->index);
	}
	else
		animation = (hlmdl_anim_t *) ((qbyte *) model->header + sequencedata->data + sequence->index);

	frametime *= sequence->timing;
	if (frametime < 0)
		frametime = 0;

	frame1 = (int)frametime;
	frametime -= frame1;
	frame2 = frame1+1;

	if (!sequence->numframes)
		return;
	if(frame1 >= sequence->numframes)
	{
		if (sequence->loop)
			frame1 %= sequence->numframes;
		else
			frame1 = sequence->numframes-1;
	}
	if(frame2 >= sequence->numframes)
	{
		if (sequence->loop)
			frame2 %= sequence->numframes;
		else
			frame2 = sequence->numframes-1;
	}

	if (frame2 < frame1)
	{
		i = frame2;
		frame2 = frame1;
		frame1 = i;
		frametime = 1-frametime;
	}

	if (lastbone > model->header->numbones)
		lastbone = model->header->numbones;



	HL_CalcBoneAdj(model);	/* Deal with programmable controllers */

	/*FIXME:this is useless*/
	/*
	if(sequence->motiontype & 0x0001)
		positions[sequence->motionbone][0] = 0.0;
	if(sequence->motiontype & 0x0002)
		positions[sequence->motionbone][1] = 0.0;
	if(sequence->motiontype & 0x0004)
		positions[sequence->motionbone][2] = 0.0;
		*/

	/*
	this is hellish.
	a hl model blends:
		4 controllers (on a player, it seems each one of them twists a separate bone in the chest)
		a mouth (not used on players)
		its a sequence (to be smooth we need to blend between two frames in the sequence)
		up to four source animations (ironically used to pitch up/down)
		alternate sequence (walking+firing)
		frame2 (quake expectations.)

		this is madness, quite frankly.

		luckily...
		controllers and mouth control the entire thing. they should be interpolated outside, and have no affect on blending here
		alternate sequences replace. we can just call this function twice (so long as bone ranges are incremental).
		autoanimating sequence is handled inside HL_CalculateBones (sequences are weird and it has to be handled there anyway)

		this means we only have sources and alternate frames left to cope with.

		FIXME: we don't handle frame2.
	*/

	if (sequence->hasblendseq>1)
	{
		if (subblendfrac < 0)
			subblendfrac = 0;
		if (subblendfrac > 1)
			subblendfrac = 1;
		for(i = firstbone; i < lastbone; i++)
		{
			//calc first blend (writes organgb+quatb)
			HL_CalculateBones(frame1, model->adjust, model->bones + i, animation + i, organgb[0]);
			QuaternionGLAngle(organgb[1], quatb);	/* A quaternion */
			if (frame1 != frame2)
			{
				HL_CalculateBones(frame2, model->adjust, model->bones + i, animation + i, organg2[0]);
				QuaternionGLAngle(organg2[1], quat2);	/* A quaternion */

				QuaternionSlerp(quatb, quat2, frametime, quatb);
				VectorInterpolate(organgb[0], frametime, organg2[0], organgb[0]);
			}

			//calc first blend (writes organg1+quat1)
			HL_CalculateBones(frame1, model->adjust, model->bones + i, animation + i + model->header->numbones, organg1[0]);
			QuaternionGLAngle(organg1[1], quat1);	/* A quaternion */
			if (frame1 != frame2)
			{
				HL_CalculateBones(frame2, model->adjust, model->bones + i, animation + i + model->header->numbones, organg2[0]);
				QuaternionGLAngle(organg2[1], quat2);	/* A quaternion */

				QuaternionSlerp(quat1, quat2, frametime, quat1);
				VectorInterpolate(organg1[0], frametime, organg2[0], organg1[0]);
			}

			//blend the two
			QuaternionSlerp(quatb, quat1, subblendfrac, quat1);
			FloatInterpolate(organgb[0][0], subblendfrac, organg1[0][0], matrix[0][3]);
			FloatInterpolate(organgb[0][0], subblendfrac, organg1[0][1], matrix[1][3]);
			FloatInterpolate(organgb[0][0], subblendfrac, organg1[0][2], matrix[2][3]);

			/* If we have a parent, take the addition. Otherwise just copy the values */
			if(model->bones[i].parent>=0)
			{
				R_ConcatTransforms(transform_matrix[model->bones[i].parent], matrix, transform_matrix[i]);
			}
			else
			{
				memcpy(transform_matrix[i], matrix, 12 * sizeof(float));
			}
		}
	}
	else
	{
		for(i = firstbone; i < lastbone; i++)
		{
			HL_CalculateBones(frame1, model->adjust, model->bones + i, animation + i, organg1[0]);
			QuaternionGLAngle(organg1[1], quat1);	/* A quaternion */
			if (frame1 != frame2)
			{
				HL_CalculateBones(frame2, model->adjust, model->bones + i, animation + i, organg2[0]);
				QuaternionGLAngle(organg2[1], quat2);	/* A quaternion */

				//lerp the quats properly rather than poorly lerping eular angles.
				QuaternionSlerp(quat1, quat2, frametime, quat1);
				VectorInterpolate(organg1[0], frametime, organg2[0], organg1[0]);
			}

			//we probably ought to keep them as quats or something.
			QuaternionGLMatrix(quat1[0], quat1[1], quat1[2], quat1[3], matrix);
			matrix[0][3] = organg1[0][0];
			matrix[1][3] = organg1[0][1];
			matrix[2][3] = organg1[0][2];

			/* If we have a parent, take the addition. Otherwise just copy the values */
			if(model->bones[i].parent>=0)
			{
				R_ConcatTransforms(transform_matrix[model->bones[i].parent], matrix, transform_matrix[i]);
			}
			else
			{
				memcpy(transform_matrix[i], matrix, 12 * sizeof(float));
			}
		}
	}
}

void R_HL_BuildFrame(hlmodel_t *model, hlmdl_model_t *amodel, entity_t *curent, short *order, float tex_s, float tex_t, mesh_t *mesh)
{
	static vecV_t xyz[2048];
	static vec3_t norm[2048];
	static vec2_t st[2048];
	static byte_vec4_t vc[2048];
	static index_t index[4096];
	int count;
	int b;
	int cbone;
	int bgroup;
	int lastbone;
	int v, i;
	vec3_t *verts;
	qbyte *bone;
	vec3_t transformed[2048];

	int idx = 0;
	int vert = 0;

	mesh->xyz_array = xyz;
	mesh->st_array = st;
	mesh->normals_array = norm;	//for lighting algos to not crash
	mesh->snormals_array = norm; //for rtlighting
	mesh->tnormals_array = norm; //for rtlighting
	mesh->indexes = index;
	mesh->colors4b_array = vc;

	for (b = 0; b < MAX_BONE_CONTROLLERS; b++)
		model->controller[b] = curent->framestate.bonecontrols[b];

//	Con_Printf("%s %i\n", sequence->name, sequence->unknown1[0]);

	cbone = 0;
	for (bgroup = 0; bgroup < FS_COUNT; bgroup++)
	{
		lastbone = curent->framestate.g[bgroup].endbone;
		if (bgroup == FS_COUNT-1)
			lastbone = model->header->numbones;
		if (cbone >= lastbone)
			continue;
		HL_SetupBones(model, curent->framestate.g[bgroup].frame[0], cbone, lastbone, (curent->framestate.g[bgroup].subblendfrac+1)*0.5, curent->framestate.g[bgroup].frametime[0]);	/* Setup the bones */
		cbone = lastbone;
	}


	verts = (vec3_t *) ((qbyte *) model->header + amodel->vertindex);
	bone = ((qbyte *) model->header + amodel->vertinfoindex);
	for(v = 0; v < amodel->numverts; v++)
	{
		VectorTransform(verts[v], (void *)transform_matrix[bone[v]], transformed[v]);
	}

	for(;;)
	{
		count = *order++;	/* get the vertex count and primitive type */
		if(!count) break;	/* done */

		if(count < 0)
		{
			count = -count;

			//emit (count-2)*3 indicies as a fan



			for (i = 0; i < count-2; i++)
			{
				index[idx++] = vert + 0;
				index[idx++] = vert + i+1;
				index[idx++] = vert + i+2;
			}
		}
		else
		{
			//emit (count-2)*3 indicies as a strip

			for (i = 0; ; )
			{
				if (i == count-2)
					break;
				index[idx++] = vert + i;
				index[idx++] = vert + i+1;
				index[idx++] = vert + i+2;
				i++;

				if (i == count-2)
					break;
				index[idx++] = vert + i;
				index[idx++] = vert + i+2;
				index[idx++] = vert + i+1;
				i++;
			}
		}

		do
		{
			VectorCopy(transformed[order[0]], xyz[vert]);

			//FIXME: what's order[1]?

			/* texture coordinates come from the draw list */
			st[vert][0] = order[2] * tex_s;
			st[vert][1] = order[3] * tex_t;

			/*fixme: build vertex normals in the base pose and transform them using the same bone matricies (just discard the origin part)*/
			norm[vert][0] = 1;
			norm[vert][1] = 1;
			norm[vert][2] = 1;
		
			vc[vert][0] = 255;
			vc[vert][1] = 255;
			vc[vert][2] = 255;
			vc[vert][3] = 255;

			order += 4;
			vert++;
		} while(--count);
	}

	mesh->numindexes = idx;
	mesh->numvertexes = vert;
}

void R_HalfLife_WalkMeshes(entity_t *rent, batch_t *b, batch_t **batches);
void R_HL_BuildMesh(struct batch_s *b)
{
	R_HalfLife_WalkMeshes(b->ent, b, NULL);
}

void R_HalfLife_WalkMeshes(entity_t *rent, batch_t *b, batch_t **batches)
{
	hlmodelcache_t *modelc = Mod_Extradata(rent->model);
	hlmodel_t model;
	int						body, m;
	int batchid = 0;
	static mesh_t bmesh, *mptr = &bmesh;

	//general model
	model.header	= modelc->header;
	model.bones		= modelc->bones;
	model.bonectls	= modelc->bonectls;
	model.shaders	= modelc->shaders;
	model.animcache = modelc->animcache;
	model.memgroup  = &rent->model->memgroup;

	for (body = 0; body < model.header->numbodyparts; body++)
	{
		/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
		hlmdl_bodypart_t	*bodypart = (hlmdl_bodypart_t *) ((qbyte *) model.header + model.header->bodypartindex) + body;
		int					bodyindex = (0 / bodypart->base) % bodypart->nummodels;
		hlmdl_model_t		*amodel = (hlmdl_model_t *) ((qbyte *) model.header + bodypart->modelindex) + bodyindex;
		/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/


		/* Draw each mesh */
		for(m = 0; m < amodel->nummesh; m++)
		{
			/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
			hlmdl_mesh_t	*mesh = (hlmdl_mesh_t *) ((qbyte *) model.header + amodel->meshindex) + m;
			float			tex_w;
			float			tex_h;
			struct hlmodelshaders_s *s;
			/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

			if (mesh->skinindex >= modelc->numskins)
				continue;

			s = &model.shaders[modelc->skins[mesh->skinindex]];

			if (batches)
			{
				int sort, j;

				b = BE_GetTempBatch();
				if (!b)
					return;

				if (!s->shader)
				{
					s->shader = R_RegisterSkin(s->name, rent->model->name);
					R_BuildDefaultTexnums(&s->defaulttex, s->shader);
				}
				b->buildmeshes = R_HL_BuildMesh;
				b->ent = rent;
				b->mesh = NULL;
				b->firstmesh = 0;
				b->meshes = 1;
				b->skin = NULL;
				b->texture = NULL;
				b->shader = s->shader;
				for (j = 0; j < MAXRLIGHTMAPS; j++)
					b->lightmap[j] = -1;
				b->surf_first = batchid;
				b->flags = 0;
				sort = b->shader->sort;
				//fixme: we probably need to force some blend modes based on the surface flags.
				if (rent->flags & RF_FORCECOLOURMOD)
					b->flags |= BEF_FORCECOLOURMOD;
				if (rent->flags & RF_ADDITIVE)
				{
					b->flags |= BEF_FORCEADDITIVE;
					if (sort < SHADER_SORT_ADDITIVE)
						sort = SHADER_SORT_ADDITIVE;
				}
				if (rent->flags & RF_TRANSLUCENT)
				{
					b->flags |= BEF_FORCETRANSPARENT;
					if (SHADER_SORT_PORTAL < sort && sort < SHADER_SORT_BLEND)
						sort = SHADER_SORT_BLEND;
				}
				if (rent->flags & RF_NODEPTHTEST)
				{
					b->flags |= BEF_FORCENODEPTH;
					if (sort < SHADER_SORT_NEAREST)
						sort = SHADER_SORT_NEAREST;
				}
				if (rent->flags & RF_NOSHADOW)
					b->flags |= BEF_NOSHADOWS;
				b->vbo = NULL;
				b->next = batches[sort];
				batches[sort] = b;
			}
				else
			{
				if (batchid == b->surf_first)
				{
					tex_w = 1.0f / s->w;
					tex_h = 1.0f / s->h;
	
					b->mesh = &mptr;
					R_HL_BuildFrame(&model, amodel, b->ent, (short *) ((qbyte *) model.header + mesh->index), tex_w, tex_h, b->mesh[0]);
					return;
				}
			}

			batchid++;
		}
	}
}

qboolean R_CalcModelLighting(entity_t *e, model_t *clmodel);

void R_HalfLife_GenerateBatches(entity_t *e, batch_t **batches)
{
	R_CalcModelLighting(e, e->model);
	R_HalfLife_WalkMeshes(e, NULL, batches);
}

#endif
