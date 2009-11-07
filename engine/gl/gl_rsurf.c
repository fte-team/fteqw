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

extern cvar_t gl_bump;


void BE_ClearVBO(vbo_t *vbo)
{
	int vboh[7];
	int i, j;
	vboh[0] = vbo->vboe;
	vboh[1] = vbo->vbocoord;
	vboh[2] = vbo->vbotexcoord;
	vboh[3] = vbo->vbolmcoord;
	vboh[4] = vbo->vbonormals;
	vboh[5] = vbo->vbosvector;
	vboh[6] = vbo->vbotvector;

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
	memset(vbo, 0, sizeof(*vbo));
}

static qboolean GL_BuildVBO(vbo_t *vbo, void *vdata, int vsize, void *edata, int elementsize)
{
	unsigned int vbos[2];

	if (!qglGenBuffersARB)
		return false;

	qglGenBuffersARB(2, vbos);
	GL_SelectVBO(vbos[0]);
	qglBufferDataARB(GL_ARRAY_BUFFER_ARB, vsize, vdata, GL_STATIC_DRAW_ARB);
	GL_SelectEBO(vbos[1]);
	qglBufferDataARB(GL_ELEMENT_ARRAY_BUFFER_ARB, elementsize, edata, GL_STATIC_DRAW_ARB);

	if (qglGetError())
	{
		GL_SelectVBO(0);
		GL_SelectEBO(0);
		qglDeleteBuffersARB(2, vbos);
		return false;
	}

	//opengl ate our data, fixup the vbo arrays to point to the vbo instead of the raw data

	if (vbo->indicies)
	{
		vbo->vboe = vbos[1];
		vbo->indicies = (index_t*)((char*)vbo->indicies - (char*)edata);
	}
	if (vbo->coord)
	{
		vbo->vbocoord = vbos[0];
		vbo->coord = (vecV_t*)((char*)vbo->coord - (char*)vdata);
	}
	if (vbo->texcoord)
	{
		vbo->vbotexcoord = vbos[0];
		vbo->texcoord = (vec2_t*)((char*)vbo->texcoord - (char*)vdata);
	}
	if (vbo->lmcoord)
	{
		vbo->vbolmcoord = vbos[0];
		vbo->lmcoord = (vec2_t*)((char*)vbo->lmcoord - (char*)vdata);
	}
	if (vbo->normals)
	{
		vbo->vbonormals = vbos[0];
		vbo->normals = (vec3_t*)((char*)vbo->normals - (char*)vdata);
	}
	if (vbo->svector)
	{
		vbo->vbosvector = vbos[0];
		vbo->svector = (vec3_t*)((char*)vbo->svector - (char*)vdata);
	}
	if (vbo->tvector)
	{
		vbo->vbotvector = vbos[0];
		vbo->tvector = (vec3_t*)((char*)vbo->tvector - (char*)vdata);
	}
	if (vbo->colours4f)
	{
		vbo->vbocolours = vbos[0];
		vbo->colours4f = (vec4_t*)((char*)vbo->colours4f - (char*)vdata);
	}

	return true;
}

void BE_GenBrushModelVBO(model_t *mod)
{
	unsigned int maxvboverts;
	unsigned int maxvboelements;

	unsigned int t;
	unsigned int i;
	unsigned int v;
	unsigned int vcount, ecount;
	unsigned int pervertsize;	//erm, that name wasn't intentional
	unsigned int meshes;

	vbo_t *vbo;
	char *vboedata;
	mesh_t *m;
	char *vbovdata;

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

		vbovdata = BZ_Malloc(maxvboverts*pervertsize);
		vboedata = BZ_Malloc(maxvboelements*sizeof(index_t));

		vbo->coord = (vecV_t*)(vbovdata);
		vbo->texcoord = (vec2_t*)((char*)vbo->coord+maxvboverts*sizeof(*vbo->coord));
		vbo->lmcoord = (vec2_t*)((char*)vbo->texcoord+maxvboverts*sizeof(*vbo->texcoord));
		vbo->normals = (vec3_t*)((char*)vbo->lmcoord+maxvboverts*sizeof(*vbo->lmcoord));
		vbo->svector = (vec3_t*)((char*)vbo->normals+maxvboverts*sizeof(*vbo->normals));
		vbo->tvector = (vec3_t*)((char*)vbo->svector+maxvboverts*sizeof(*vbo->svector));
		vbo->colours4f = (vec4_t*)((char*)vbo->tvector+maxvboverts*sizeof(*vbo->tvector));
		vbo->indicies = (index_t*)vboedata;

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
				vbo->indicies[ecount++] = vcount + m->indexes[v];
			for (v = 0; v < m->numvertexes; v++)
			{
				vbo->coord[vcount+v][0] = m->xyz_array[v][0];
				vbo->coord[vcount+v][1] = m->xyz_array[v][1];
				vbo->coord[vcount+v][2] = m->xyz_array[v][2];
				if (m->st_array)
				{
					vbo->texcoord[vcount+v][0] = m->st_array[v][0];
					vbo->texcoord[vcount+v][1] = m->st_array[v][1];
				}
				if (m->lmst_array)
				{
					vbo->lmcoord[vcount+v][0] = m->lmst_array[v][0];
					vbo->lmcoord[vcount+v][1] = m->lmst_array[v][1];
				}
				if (m->normals_array)
				{
					vbo->normals[vcount+v][0] = m->normals_array[v][0];
					vbo->normals[vcount+v][1] = m->normals_array[v][1];
					vbo->normals[vcount+v][2] = m->normals_array[v][2];
				}
				if (m->snormals_array)
				{
					vbo->svector[vcount+v][0] = m->snormals_array[v][0];
					vbo->svector[vcount+v][1] = m->snormals_array[v][1];
					vbo->svector[vcount+v][2] = m->snormals_array[v][2];
				}
				if (m->tnormals_array)
				{
					vbo->tvector[vcount+v][0] = m->tnormals_array[v][0];
					vbo->tvector[vcount+v][1] = m->tnormals_array[v][1];
					vbo->tvector[vcount+v][2] = m->tnormals_array[v][2];
				}
				if (m->colors4f_array)
				{
					vbo->colours4f[vcount+v][0] = m->colors4f_array[v][0];
					vbo->colours4f[vcount+v][1] = m->colors4f_array[v][1];
					vbo->colours4f[vcount+v][2] = m->colors4f_array[v][2];
					vbo->colours4f[vcount+v][3] = m->colors4f_array[v][3];
				}
			}
			vcount += v;
		}

		if (GL_BuildVBO(vbo, vbovdata, vcount*pervertsize, vboedata, ecount*sizeof(index_t)))
		{
			BZ_Free(vbovdata);
			BZ_Free(vboedata);
		}
	}
	for (i=0 ; i<mod->numsurfaces ; i++)
	{
		if (!mod->surfaces[i].mark)
			Host_EndGame("Surfaces with bad textures detected\n");
	}
}

void BE_UploadAllLightmaps(void)
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
		GL_Bind(lightmap_textures[i]);
		qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		qglTexImage2D (GL_TEXTURE_2D, 0, lightmap_bytes
		, LMBLOCK_WIDTH, LMBLOCK_HEIGHT, 0, 
		((lightmap_bytes==3)?GL_RGB:GL_LUMINANCE), GL_UNSIGNED_BYTE, lightmap[i]->lightmaps);

		if (gl_bump.ival)
		{
			lightmap[i]->deluxmodified = false;
			lightmap[i]->deluxrectchange.l = LMBLOCK_WIDTH;
			lightmap[i]->deluxrectchange.t = LMBLOCK_HEIGHT;
			lightmap[i]->deluxrectchange.w = 0;
			lightmap[i]->deluxrectchange.h = 0;
			GL_Bind(deluxmap_textures[i]);
			qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			qglTexImage2D (GL_TEXTURE_2D, 0, 3
			, LMBLOCK_WIDTH, LMBLOCK_HEIGHT, 0, 
			GL_RGB, GL_UNSIGNED_BYTE, lightmap[i]->deluxmaps);
		}
	}
}
#endif
