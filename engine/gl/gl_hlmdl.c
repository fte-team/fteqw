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
extern char loadname[];
qboolean Mod_LoadHLModel (model_t *mod, void *buffer)
{
    /*~~*/
    int i;

	hlmodelcache_t *model;
	hlmdl_header_t *header;
	hlmdl_header_t *texheader;
	hlmdl_tex_t	*tex;
	hlmdl_bone_t	*bones;
	hlmdl_bonecontroller_t	*bonectls;
	shader_t **shaders;
	void *texmem = NULL;
    /*~~*/


	//checksum the model

	if (mod->engineflags & MDLF_DOCRC)
	{
		unsigned short crc;
		qbyte *p;
		int len;
		char st[40];

		QCRC_Init(&crc);
		for (len = com_filesize, p = buffer; len; len--, p++)
			QCRC_ProcessByte(&crc, *p);

		sprintf(st, "%d", (int) crc);
		Info_SetValueForKey (cls.userinfo[0],
			(mod->engineflags & MDLF_PLAYER) ? pmodel_name : emodel_name,
			st, sizeof(cls.userinfo[0]));

		if (cls.state >= ca_connected)
		{
			CL_SendClientCommand(true, "setinfo %s %d",
				(mod->engineflags & MDLF_PLAYER) ? pmodel_name : emodel_name,
				(int)crc);
		}
	}

	//load the model into hunk
	model = ZG_Malloc(&mod->memgroup, sizeof(hlmodelcache_t));

	header = ZG_Malloc(&mod->memgroup, com_filesize);
	memcpy(header, buffer, com_filesize);

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
		char texmodelname[MAX_QPATH];
		COM_StripExtension(mod->name, texmodelname, sizeof(texmodelname));
		//no textures? eesh. They must be stored externally.
		texheader = texmem = (hlmdl_header_t*)FS_LoadMallocFile(va("%st.mdl", texmodelname));
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

	model->header = (char *)header - (char *)model;
	model->texheader = (char *)texheader - (char *)model;
	model->textures = (char *)tex - (char *)model;
	model->bones = (char *)bones - (char *)model;
	model->bonectls = (char *)bonectls - (char *)model;

	shaders = ZG_Malloc(&mod->memgroup, texheader->numtextures*sizeof(shader_t));
	model->shaders = (char *)shaders - (char *)model;
    for(i = 0; i < texheader->numtextures; i++)
    {
		shaders[i] = R_RegisterSkin(va("%s_%i.tga", mod->name, i), mod->name);
		shaders[i]->defaulttextures.base = R_LoadTexture8Pal24("", tex[i].w, tex[i].h, (qbyte *) texheader + tex[i].offset, (qbyte *) texheader + tex[i].w * tex[i].h + tex[i].offset, IF_NOALPHA|IF_NOGAMMA);
    }

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
	return (void*)((char*)mc + mc->header);
}
#endif

int HLMod_FrameForName(model_t *mod, char *name)
{
	int i;
	hlmdl_header_t *h;
	hlmdl_sequencelist_t *seqs;
	hlmodelcache_t *mc;
	if (!mod || mod->type != mod_halflife)
		return -1;	//halflife models only, please

	mc = Mod_Extradata(mod);

	h = (hlmdl_header_t *)((char *)mc + mc->header);
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

	h = (hlmdl_header_t *)((char *)mc + mc->header);
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

  note, while ender may be proud of this function, it lacks the fact that interpolating eular angles is not as acurate as interpolating quaternions.
  it is faster though.
 */
void HL_CalculateBones
(
    int				offset,
    int				frame,
	float			lerpfrac,
    vec4_t			adjust,
    hlmdl_bone_t	*bone,
    hlmdl_anim_t	*animation,
    float			*destination
)
{
    /*~~~~~~~~~~*/
    int		i;
    vec3_t	angle;
	float lerpifrac = 1-lerpfrac;
	float t;
    /*~~~~~~~~~~*/

    /* For each vector */
    for(i = 0; i < 3; i++)
    {
        /*~~~~~~~~~~~~~~~*/
        int o = i + offset;        /* Take the value offset - allows quaternion & vector in one function */
        /*~~~~~~~~~~~~~~~*/

        angle[i] = bone->value[o];	/* Take the bone value */

        if(animation->offset[o] != 0)
        {
            /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
            int					tempframe = frame;
            hlmdl_animvalue_t	*animvalue = (hlmdl_animvalue_t *) ((qbyte *) animation + animation->offset[o]);
            /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

            /* find values including the required frame */
            while(animvalue->num.total <= tempframe)
            {
                tempframe -= animvalue->num.total;
                animvalue += animvalue->num.valid + 1;
            }
            if(animvalue->num.valid > tempframe)
            {
                if(animvalue->num.valid > (tempframe + 1))
				{
					//we can lerp that
                    t = animvalue[tempframe + 1].value * lerpifrac + lerpfrac * animvalue[tempframe + 2].value;
				}
                else
                    t = animvalue[animvalue->num.valid].value;
                angle[i] = bone->value[o] + t * bone->scale[o];
            }
            else
            {
                if(animvalue->num.total < tempframe + 1)
                {
                    angle[i] +=
                        (animvalue[animvalue->num.valid].value * lerpifrac +
                         lerpfrac * animvalue[animvalue->num.valid + 2].value) *
                        bone->scale[o];
                }
                else
                {
                    angle[i] += animvalue[animvalue->num.valid].value * bone->scale[o];
                }
            }
        }

        if(bone->bonecontroller[o] != -1)
		{	/* Add the programmable offset. */
            angle[i] += adjust[bone->bonecontroller[o]];
        }
    }

    if(offset < 3)
    {
        VectorCopy(angle, destination);			/* Just a standard vector */
    }
    else
    {
        QuaternionGLAngle(angle, destination);	/* A quaternion */
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
    static vec3_t			positions[2];
    static vec4_t			quaternions[2], blended;

	int frame;

    hlmdl_sequencelist_t	*sequence = (hlmdl_sequencelist_t *) ((qbyte *) model->header + model->header->seqindex) +
										 ((unsigned int)seqnum>=model->header->numseq?0:seqnum);
    hlmdl_sequencedata_t	*sequencedata = (hlmdl_sequencedata_t *)
                                         ((qbyte *) model->header + model->header->seqgroups) +
                                         sequence->seqindex;
    hlmdl_anim_t			*animation = (hlmdl_anim_t *)
                                ((qbyte *) model->header + sequencedata->data + sequence->index);
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

	frametime *= sequence->timing;
	if (frametime < 0)
		frametime = 0;

	frame = (int)frametime;
	frametime -= frame;

	if (!sequence->numframes)
		return;
    if(frame >= sequence->numframes)
	{
		if (sequence->loop)
			frame %= sequence->numframes;
		else
			frame = sequence->numframes-1;
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
			HL_CalculateBones(0, frame, frametime, model->adjust, model->bones + i, animation + i, positions[0]);
			HL_CalculateBones(3, frame, frametime, model->adjust, model->bones + i, animation + i, quaternions[0]);

			HL_CalculateBones(3, frame, frametime, model->adjust, model->bones + i, animation + i + model->header->numbones, quaternions[1]);

			QuaternionSlerp(quaternions[0], quaternions[1], subblendfrac, blended);
			QuaternionGLMatrix(blended[0], blended[1], blended[2], blended[3], matrix);
			matrix[0][3] = positions[0][0];
			matrix[1][3] = positions[0][1];
			matrix[2][3] = positions[0][2];

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
			/*
			 * There are two vector offsets in the structure. The first seems to be the
			 * positions of the bones, the second the quats of the bone matrix itself. We
			 * convert it inside the routine - Inconsistant, but hey.. so's the whole model
			 * format.
			 */
			HL_CalculateBones(0, frame, frametime, model->adjust, model->bones + i, animation + i, positions[0]);
			HL_CalculateBones(3, frame, frametime, model->adjust, model->bones + i, animation + i, quaternions[0]);

			QuaternionGLMatrix(quaternions[0][0], quaternions[0][1], quaternions[0][2], quaternions[0][3], matrix);
			matrix[0][3] = positions[0][0];
			matrix[1][3] = positions[0][1];
			matrix[2][3] = positions[0][2];

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

			norm[vert][1] = 1;
		

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
	short					*skins;
	int batchid = 0;
	static mesh_t bmesh, *mptr = &bmesh;

	//general model
	model.header	= (hlmdl_header_t *)			((char *)modelc + modelc->header);
	model.texheader	= (hlmdl_header_t *)			((char *)modelc + modelc->texheader);
	model.textures	= (hlmdl_tex_t *)				((char *)modelc + modelc->textures);
	model.bones		= (hlmdl_bone_t *)				((char *)modelc + modelc->bones);
	model.bonectls	= (hlmdl_bonecontroller_t *)	((char *)modelc + modelc->bonectls);
	model.shaders	= (shader_t **)					((char *)modelc + modelc->shaders);

	skins = (short *) ((qbyte *) model.texheader + model.texheader->skins);

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
            /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/


			if (batches)
			{
				shader_t *shader;
				int sort;

				b = BE_GetTempBatch();
				if (!b)
					return;

				shader = model.shaders[skins[mesh->skinindex]];
				b->buildmeshes = R_HL_BuildMesh;
				b->ent = rent;
				b->mesh = NULL;
				b->firstmesh = 0;
				b->meshes = 1;
				b->skin = &shader->defaulttextures;
				b->texture = NULL;
				b->shader = shader;
				b->lightmap[0] = -1;
				b->lightmap[1] = -1;
				b->lightmap[2] = -1;
				b->lightmap[3] = -1;
				b->surf_first = batchid;
				b->flags = 0;
				sort = shader->sort;
				//fixme: we probably need to force some blend modes based on the surface flags.
				if (rent->flags & RF_FORCECOLOURMOD)
					b->flags |= BEF_FORCECOLOURMOD;
				if (rent->flags & Q2RF_ADDITIVE)
				{
					b->flags |= BEF_FORCEADDITIVE;
					if (sort < SHADER_SORT_ADDITIVE)
						sort = SHADER_SORT_ADDITIVE;
				}
				if (rent->flags & Q2RF_TRANSLUCENT)
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
					tex_w = 1.0f / model.textures[skins[mesh->skinindex]].w;
					tex_h = 1.0f / model.textures[skins[mesh->skinindex]].h;
	
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
