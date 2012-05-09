/*
Copyright (C) 1996-1997 Id Software, Inc.

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
// r_surf.c: surface-related refresh code

#include "quakedef.h"
#if defined(GLQUAKE)
#include "glquake.h"
#include "shader.h"
#include "renderque.h"
#include <math.h>

void GLBE_ClearVBO(vbo_t *vbo)
{
	int vboh[7];
	int i, j;
	vboh[0] = vbo->indicies.gl.vbo;
	vboh[1] = vbo->coord.gl.vbo;
	vboh[2] = vbo->texcoord.gl.vbo;
	vboh[3] = vbo->lmcoord.gl.vbo;
	vboh[4] = vbo->normals.gl.vbo;
	vboh[5] = vbo->svector.gl.vbo;
	vboh[6] = vbo->tvector.gl.vbo;

	for (i = 0; i < 7; i++)
	{
		if (!vboh[i])
			continue;
		for (j = 0; j < 7; j++)
		{
			if (vboh[j] == vboh[i])
				break;	//already freed by one of the other ones
		}
		if (j == 7)
			qglDeleteBuffersARB(1, &vboh[i]);
	}
	if (vbo->vertdata)
		BZ_Free(vbo->vertdata);
	BZ_Free(vbo->meshlist);
	memset(vbo, 0, sizeof(*vbo));
}

void GLBE_SetupVAO(vbo_t *vbo, unsigned int vaodynamic);

static qboolean GL_BuildVBO(vbo_t *vbo, void *vdata, int vsize, void *edata, int elementsize, unsigned int vaodynamic)
{
	unsigned int vbos[2];

	if (!qglGenBuffersARB)
		return false;

	qglGenBuffersARB(1+(elementsize>0), vbos);

	//opengl ate our data, fixup the vbo arrays to point to the vbo instead of the raw data

	if (vbo->indicies.gl.addr && elementsize)
	{
		vbo->indicies.gl.vbo = vbos[1];
		vbo->indicies.gl.addr = (index_t*)((char*)vbo->indicies.gl.addr - (char*)edata);
	}
	if (vbo->coord.gl.addr)
	{
		vbo->coord.gl.vbo = vbos[0];
		vbo->coord.gl.addr = (vecV_t*)((char*)vbo->coord.gl.addr - (char*)vdata);
	}
	if (vbo->texcoord.gl.addr)
	{
		vbo->texcoord.gl.vbo = vbos[0];
		vbo->texcoord.gl.addr = (vec2_t*)((char*)vbo->texcoord.gl.addr - (char*)vdata);
	}
	if (vbo->lmcoord.gl.addr)
	{
		vbo->lmcoord.gl.vbo = vbos[0];
		vbo->lmcoord.gl.addr = (vec2_t*)((char*)vbo->lmcoord.gl.addr - (char*)vdata);
	}
	if (vbo->normals.gl.addr)
	{
		vbo->normals.gl.vbo = vbos[0];
		vbo->normals.gl.addr = (vec3_t*)((char*)vbo->normals.gl.addr - (char*)vdata);
	}
	if (vbo->svector.gl.addr)
	{
		vbo->svector.gl.vbo = vbos[0];
		vbo->svector.gl.addr = (vec3_t*)((char*)vbo->svector.gl.addr - (char*)vdata);
	}
	if (vbo->tvector.gl.addr)
	{
		vbo->tvector.gl.vbo = vbos[0];
		vbo->tvector.gl.addr = (vec3_t*)((char*)vbo->tvector.gl.addr - (char*)vdata);
	}
	if (vbo->colours.gl.addr)
	{
		vbo->colours.gl.vbo = vbos[0];
		vbo->colours.gl.addr = (vec4_t*)((char*)vbo->colours.gl.addr - (char*)vdata);
	}

	GLBE_SetupVAO(vbo, vaodynamic);
	
	qglBufferDataARB(GL_ARRAY_BUFFER_ARB, vsize, vdata, GL_STATIC_DRAW_ARB);
	if (elementsize>0)
	{
		qglBufferDataARB(GL_ELEMENT_ARRAY_BUFFER_ARB, elementsize, edata, GL_STATIC_DRAW_ARB);
	}

	return true;
}

void *allocbuf(char **p, int elements, int elementsize)
{
	void *ret;
	*p += elementsize - 1;
	*p -= (size_t)*p & (elementsize-1);
	ret = *p;
	*p += elements*elementsize;
	return ret;
}

void GLBE_GenBatchVBOs(vbo_t **vbochain, batch_t *firstbatch, batch_t *stopbatch)
{
	unsigned int maxvboverts;
	unsigned int maxvboelements;

	unsigned int i;
	unsigned int v;
	unsigned int vcount, ecount;
	unsigned int pervertsize;	//erm, that name wasn't intentional
	unsigned int meshes;

	vbo_t *vbo;
	mesh_t *m;
	char *p;

	vecV_t *coord;
	vec2_t *texcoord;
	vec2_t *lmcoord;
	vec3_t *normals;
	vec3_t *svector;
	vec3_t *tvector;
	vec4_t *colours;
	index_t *indicies;
	batch_t *batch;


	vbo = Z_Malloc(sizeof(*vbo));

	maxvboverts = 0;
	maxvboelements = 0;
	meshes = 0;
	for(batch = firstbatch; batch != stopbatch; batch = batch->next)
	{
		for (i=0 ; i<batch->meshes ; i++)
		{
			m = batch->mesh[i];
			meshes++;
			maxvboelements += m->numindexes;
			maxvboverts += m->numvertexes;
		}
	}
	if (maxvboverts > MAX_INDICIES)
		Sys_Error("Building a vbo with too many verticies\n");


	vcount = 0;
	ecount = 0;

	pervertsize =	sizeof(vecV_t)+	//coord
				sizeof(vec2_t)+	//tex
				sizeof(vec2_t)+	//lm
				sizeof(vec3_t)+	//normal
				sizeof(vec3_t)+	//sdir
				sizeof(vec3_t)+	//tdir
				sizeof(vec4_t);	//colours

	vbo->vertdata = BZ_Malloc((maxvboverts+1)*pervertsize + (maxvboelements+1)*sizeof(index_t));

	p = vbo->vertdata;

	vbo->coord.gl.addr = allocbuf(&p, maxvboverts, sizeof(vecV_t));
	vbo->texcoord.gl.addr = allocbuf(&p, maxvboverts, sizeof(vec2_t));
	vbo->lmcoord.gl.addr = allocbuf(&p, maxvboverts, sizeof(vec2_t));
	vbo->normals.gl.addr = allocbuf(&p, maxvboverts, sizeof(vec3_t));
	vbo->svector.gl.addr = allocbuf(&p, maxvboverts, sizeof(vec3_t));
	vbo->tvector.gl.addr = allocbuf(&p, maxvboverts, sizeof(vec3_t));
	vbo->colours.gl.addr = allocbuf(&p, maxvboverts, sizeof(vec4_t));
	vbo->indicies.gl.addr = allocbuf(&p, maxvboelements, sizeof(index_t));

	coord = vbo->coord.gl.addr;
	texcoord = vbo->texcoord.gl.addr;
	lmcoord = vbo->lmcoord.gl.addr;
	normals = vbo->normals.gl.addr;
	svector = vbo->svector.gl.addr;
	tvector = vbo->tvector.gl.addr;
	colours = vbo->colours.gl.addr;
	indicies = vbo->indicies.gl.addr;

	//vbo->meshcount = meshes;
	//vbo->meshlist = BZ_Malloc(meshes*sizeof(*vbo->meshlist));

	meshes = 0;


	for(batch = firstbatch; batch != stopbatch; batch = batch->next)
	{
		batch->vbo = vbo;
		for (i=0 ; i<batch->meshes ; i++)
		{
			m = batch->mesh[i];

//			surf->mark = &vbo->meshlist[meshes++];
//			*surf->mark = NULL;

			m->vbofirstvert = vcount;
			m->vbofirstelement = ecount;
			for (v = 0; v < m->numindexes; v++)
				indicies[ecount++] = vcount + m->indexes[v];
			for (v = 0; v < m->numvertexes; v++)
			{
				coord[vcount+v][0] = m->xyz_array[v][0];
				coord[vcount+v][1] = m->xyz_array[v][1];
				coord[vcount+v][2] = m->xyz_array[v][2];
				if (m->st_array)
				{
					texcoord[vcount+v][0] = m->st_array[v][0];
					texcoord[vcount+v][1] = m->st_array[v][1];
				}
				if (m->lmst_array)
				{
					lmcoord[vcount+v][0] = m->lmst_array[v][0];
					lmcoord[vcount+v][1] = m->lmst_array[v][1];
				}
				if (m->normals_array)
				{
					normals[vcount+v][0] = m->normals_array[v][0];
					normals[vcount+v][1] = m->normals_array[v][1];
					normals[vcount+v][2] = m->normals_array[v][2];
				}
				if (m->snormals_array)
				{
					svector[vcount+v][0] = m->snormals_array[v][0];
					svector[vcount+v][1] = m->snormals_array[v][1];
					svector[vcount+v][2] = m->snormals_array[v][2];
				}
				if (m->tnormals_array)
				{
					tvector[vcount+v][0] = m->tnormals_array[v][0];
					tvector[vcount+v][1] = m->tnormals_array[v][1];
					tvector[vcount+v][2] = m->tnormals_array[v][2];
				}
				if (m->colors4f_array)
				{
					colours[vcount+v][0] = m->colors4f_array[v][0];
					colours[vcount+v][1] = m->colors4f_array[v][1];
					colours[vcount+v][2] = m->colors4f_array[v][2];
					colours[vcount+v][3] = m->colors4f_array[v][3];
				}
			}
			vcount += v;
		}
	}

	if (GL_BuildVBO(vbo, vbo->vertdata, vcount*pervertsize, indicies, ecount*sizeof(index_t), 0))
	{
		BZ_Free(vbo->vertdata);
		vbo->vertdata = NULL;
	}

	vbo->next = *vbochain;
	*vbochain = vbo;
}

void GLBE_GenBrushModelVBO(model_t *mod)
{
	unsigned int vcount;


	batch_t *batch, *fbatch;
	int sortid;

	fbatch = NULL;
	vcount = 0;
	for (sortid = 0; sortid < SHADER_SORT_COUNT; sortid++)
	{
		if (!mod->batches[sortid])
			continue;

		for (fbatch = batch = mod->batches[sortid]; batch != NULL; batch = batch->next)
		{
			//firstmesh got reused as the number of verticies in each batch
			if (vcount + batch->firstmesh > MAX_INDICIES)
			{
				GLBE_GenBatchVBOs(&mod->vbos, fbatch, batch);
				fbatch = batch;
				vcount = 0;
			}
			vcount += batch->firstmesh;
		}
		
		GLBE_GenBatchVBOs(&mod->vbos, fbatch, batch);
	}
#if 0
	if (!mod->numsurfaces)
		return;

	for (t = 0; t < mod->numtextures; t++)
	{
		if (!mod->textures[t])
			continue;
		vbo = &mod->textures[t]->vbo;
		BE_ClearVBO(vbo);

		maxvboverts = 0;
		maxvboelements = 0;
		meshes = 0;
		for (i=0 ; i<mod->numsurfaces ; i++)
		{
			if (mod->surfaces[i].texinfo->texture != mod->textures[t])
				continue;
			m = mod->surfaces[i].mesh;
			if (!m)
				continue;

			meshes++;
			maxvboelements += m->numindexes;
			maxvboverts += m->numvertexes;
		}
#if sizeof_index_t == 2
		if (maxvboverts > (1<<(sizeof(index_t)*8))-1)
			continue;
#endif
		if (!maxvboverts)
			continue;

		//fixme: stop this from leaking!
		vcount = 0;
		ecount = 0;

		pervertsize =	sizeof(vecV_t)+	//coord
					sizeof(vec2_t)+	//tex
					sizeof(vec2_t)+	//lm
					sizeof(vec3_t)+	//normal
					sizeof(vec3_t)+	//sdir
					sizeof(vec3_t)+	//tdir
					sizeof(vec4_t);	//colours

		vbo->vertdata = BZ_Malloc((maxvboverts+1)*pervertsize + (maxvboelements+1)*sizeof(index_t));

		p = vbo->vertdata;

		vbo->coord.gl.addr = allocbuf(&p, maxvboverts, sizeof(vecV_t));
		vbo->texcoord.gl.addr = allocbuf(&p, maxvboverts, sizeof(vec2_t));
		vbo->lmcoord.gl.addr = allocbuf(&p, maxvboverts, sizeof(vec2_t));
		vbo->normals.gl.addr = allocbuf(&p, maxvboverts, sizeof(vec3_t));
		vbo->svector.gl.addr = allocbuf(&p, maxvboverts, sizeof(vec3_t));
		vbo->tvector.gl.addr = allocbuf(&p, maxvboverts, sizeof(vec3_t));
		vbo->colours.gl.addr = allocbuf(&p, maxvboverts, sizeof(vec4_t));
		vbo->indicies.gl.addr = allocbuf(&p, maxvboelements, sizeof(index_t));

		coord = vbo->coord.gl.addr;
		texcoord = vbo->texcoord.gl.addr;
		lmcoord = vbo->lmcoord.gl.addr;
		normals = vbo->normals.gl.addr;
		svector = vbo->svector.gl.addr;
		tvector = vbo->tvector.gl.addr;
		colours = vbo->colours.gl.addr;
		indicies = vbo->indicies.gl.addr;

		vbo->meshcount = meshes;
		vbo->meshlist = BZ_Malloc(meshes*sizeof(*vbo->meshlist));

		meshes = 0;

		for (i=0 ; i<mod->numsurfaces ; i++)
		{
			if (mod->surfaces[i].texinfo->texture != mod->textures[t])
				continue;
			m = mod->surfaces[i].mesh;
			if (!m)
				continue;

			mod->surfaces[i].mark = &vbo->meshlist[meshes++];
			*mod->surfaces[i].mark = NULL;

			m->vbofirstvert = vcount;
			m->vbofirstelement = ecount;
			for (v = 0; v < m->numindexes; v++)
				indicies[ecount++] = vcount + m->indexes[v];
			for (v = 0; v < m->numvertexes; v++)
			{
				coord[vcount+v][0] = m->xyz_array[v][0];
				coord[vcount+v][1] = m->xyz_array[v][1];
				coord[vcount+v][2] = m->xyz_array[v][2];
				if (m->st_array)
				{
					texcoord[vcount+v][0] = m->st_array[v][0];
					texcoord[vcount+v][1] = m->st_array[v][1];
				}
				if (m->lmst_array)
				{
					lmcoord[vcount+v][0] = m->lmst_array[v][0];
					lmcoord[vcount+v][1] = m->lmst_array[v][1];
				}
				if (m->normals_array)
				{
					normals[vcount+v][0] = m->normals_array[v][0];
					normals[vcount+v][1] = m->normals_array[v][1];
					normals[vcount+v][2] = m->normals_array[v][2];
				}
				if (m->snormals_array)
				{
					svector[vcount+v][0] = m->snormals_array[v][0];
					svector[vcount+v][1] = m->snormals_array[v][1];
					svector[vcount+v][2] = m->snormals_array[v][2];
				}
				if (m->tnormals_array)
				{
					tvector[vcount+v][0] = m->tnormals_array[v][0];
					tvector[vcount+v][1] = m->tnormals_array[v][1];
					tvector[vcount+v][2] = m->tnormals_array[v][2];
				}
				if (m->colors4f_array)
				{
					colours[vcount+v][0] = m->colors4f_array[v][0];
					colours[vcount+v][1] = m->colors4f_array[v][1];
					colours[vcount+v][2] = m->colors4f_array[v][2];
					colours[vcount+v][3] = m->colors4f_array[v][3];
				}
			}
			vcount += v;
		}

		if (GL_BuildVBO(vbo, vbo->vertdata, vcount*pervertsize, indicies, ecount*sizeof(index_t), 0))
		{
			BZ_Free(vbo->vertdata);
			vbo->vertdata = NULL;
		}
	}
#endif
}

void GLBE_UploadAllLightmaps(void)
{
	int i;
	//
	// upload all lightmaps that were filled
	//
	for (i=0 ; i<numlightmaps ; i++)
	{
		if (!lightmap[i])
			break;		// no more used
		lightmap[i]->rectchange.l = LMBLOCK_WIDTH;
		lightmap[i]->rectchange.t = LMBLOCK_HEIGHT;
		lightmap[i]->rectchange.w = 0;
		lightmap[i]->rectchange.h = 0;
		if (!lightmap[i]->modified)
			continue;
		lightmap[i]->modified = false;
		GL_MTBind(0, GL_TEXTURE_2D, lightmap_textures[i]);
		qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		switch (lightmap_bytes)
		{
		case 4:
			qglTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA,
				LMBLOCK_WIDTH, LMBLOCK_WIDTH, 0, (lightmap_bgra?GL_BGRA_EXT:GL_RGBA), GL_UNSIGNED_INT_8_8_8_8_REV,
				lightmap[i]->lightmaps);
			break;
		case 3:
			qglTexImage2D(GL_TEXTURE_2D, 0, GL_RGB,
				LMBLOCK_WIDTH, LMBLOCK_WIDTH, 0, (lightmap_bgra?GL_BGR_EXT:GL_RGB), GL_UNSIGNED_BYTE,
				lightmap[i]->lightmaps);
			break;
		case 1:
			qglTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE,
				LMBLOCK_WIDTH, LMBLOCK_WIDTH, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE,
				lightmap[i]->lightmaps);
			break;
		}
		if (r_deluxemapping.ival)
		{
			lightmap[i]->deluxmodified = false;
			lightmap[i]->deluxrectchange.l = LMBLOCK_WIDTH;
			lightmap[i]->deluxrectchange.t = LMBLOCK_HEIGHT;
			lightmap[i]->deluxrectchange.w = 0;
			lightmap[i]->deluxrectchange.h = 0;
			GL_MTBind(0, GL_TEXTURE_2D, deluxmap_textures[i]);
			qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			qglTexImage2D (GL_TEXTURE_2D, 0, 3
					, LMBLOCK_WIDTH, LMBLOCK_HEIGHT, 0,
					GL_RGB, GL_UNSIGNED_BYTE, lightmap[i]->deluxmaps);
		}
	}
}
#endif
