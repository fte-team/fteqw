#include "quakedef.h"
#ifdef D3DQUAKE
#include "shader.h"

#include <d3d9.h>
extern LPDIRECT3DDEVICE9 pD3DDev9;

typedef struct
{
	shader_t	*curshader;
	texnums_t	*curtexnums;
	texid_t		curlightmap;
	texid_t		curdeluxmap;
} d3dbackend_t;

static d3dbackend_t d3dbackend;

extern int be_maxpasses;

void BE_Init(void)
{
	be_maxpasses = 1;
}

static void D3DBE_ApplyShaderBits(unsigned int bits)
{
	if (bits & SBITS_BLEND_BITS)
	{
		int src;
		int dst;

		switch(bits & SBITS_SRCBLEND_BITS)
		{
		case SBITS_SRCBLEND_ZERO:					src = D3DBLEND_ZERO; break;
		case SBITS_SRCBLEND_ONE:					src = D3DBLEND_ONE; break;
		case SBITS_SRCBLEND_DST_COLOR:				src = D3DBLEND_DESTCOLOR; break;
		case SBITS_SRCBLEND_ONE_MINUS_DST_COLOR:	src = D3DBLEND_INVDESTCOLOR; break;
		case SBITS_SRCBLEND_SRC_ALPHA:				src = D3DBLEND_SRCALPHA; break;
		case SBITS_SRCBLEND_ONE_MINUS_SRC_ALPHA:	src = D3DBLEND_INVSRCALPHA; break;
		case SBITS_SRCBLEND_DST_ALPHA:				src = D3DBLEND_DESTALPHA; break;
		case SBITS_SRCBLEND_ONE_MINUS_DST_ALPHA:	src = D3DBLEND_INVDESTALPHA; break;
		case SBITS_SRCBLEND_ALPHA_SATURATE:			src = D3DBLEND_SRCALPHASAT; break;
		default:	Sys_Error("Bad shader blend src\n"); return;
		}
		switch(bits & SBITS_DSTBLEND_BITS)
		{
		case SBITS_DSTBLEND_ZERO:					dst = D3DBLEND_ZERO; break;
		case SBITS_DSTBLEND_ONE:					dst = D3DBLEND_ONE; break;
		case SBITS_DSTBLEND_SRC_ALPHA:				dst = D3DBLEND_SRCALPHA; break;
		case SBITS_DSTBLEND_ONE_MINUS_SRC_ALPHA:	dst = D3DBLEND_INVSRCALPHA; break;
		case SBITS_DSTBLEND_DST_ALPHA:				dst = D3DBLEND_DESTALPHA; break;
		case SBITS_DSTBLEND_ONE_MINUS_DST_ALPHA:	dst = D3DBLEND_INVDESTALPHA; break;
		case SBITS_DSTBLEND_SRC_COLOR:				dst = D3DBLEND_SRCCOLOR; break;
		case SBITS_DSTBLEND_ONE_MINUS_SRC_COLOR:	dst = D3DBLEND_INVSRCCOLOR; break;
		default:	Sys_Error("Bad shader blend dst\n"); return;
		}
		IDirect3DDevice9_SetRenderState(pD3DDev9, D3DRS_ALPHABLENDENABLE, TRUE);
		IDirect3DDevice9_SetRenderState(pD3DDev9, D3DRS_SRCBLEND, src);
		IDirect3DDevice9_SetRenderState(pD3DDev9, D3DRS_DESTBLEND, dst);
	}
	else
		IDirect3DDevice9_SetRenderState(pD3DDev9, D3DRS_ALPHABLENDENABLE, FALSE);

	switch(bits & SBITS_ATEST_BITS)
	{
	case SBITS_ATEST_NONE:
		IDirect3DDevice9_SetRenderState(pD3DDev9, D3DRS_ALPHATESTENABLE, FALSE);
//		IDirect3DDevice9_SetRenderState(pD3DDev9, D3DRS_ALPHAREF, 0);
//		IDirect3DDevice9_SetRenderState(pD3DDev9, D3DRS_ALPHAFUNC, 0);
		break;
	case SBITS_ATEST_GT0:
		IDirect3DDevice9_SetRenderState(pD3DDev9, D3DRS_ALPHATESTENABLE, TRUE);
		IDirect3DDevice9_SetRenderState(pD3DDev9, D3DRS_ALPHAREF, 0);
		IDirect3DDevice9_SetRenderState(pD3DDev9, D3DRS_ALPHAFUNC, D3DCMP_GREATER);
		break;
	case SBITS_ATEST_LT128:
		IDirect3DDevice9_SetRenderState(pD3DDev9, D3DRS_ALPHATESTENABLE, TRUE);
		IDirect3DDevice9_SetRenderState(pD3DDev9, D3DRS_ALPHAREF, 128);
		IDirect3DDevice9_SetRenderState(pD3DDev9, D3DRS_ALPHAFUNC, D3DCMP_LESS);
		break;
	case SBITS_ATEST_GE128:
		IDirect3DDevice9_SetRenderState(pD3DDev9, D3DRS_ALPHATESTENABLE, TRUE);
		IDirect3DDevice9_SetRenderState(pD3DDev9, D3DRS_ALPHAREF, 128);
		IDirect3DDevice9_SetRenderState(pD3DDev9, D3DRS_ALPHAFUNC, D3DCMP_GREATEREQUAL);
		break;
	}

	if(bits & SBITS_MISC_DEPTHWRITE)
		IDirect3DDevice9_SetRenderState(pD3DDev9, D3DRS_ZWRITEENABLE, TRUE);
	else
		IDirect3DDevice9_SetRenderState(pD3DDev9, D3DRS_ZWRITEENABLE, FALSE);

	if(bits & SBITS_MISC_NODEPTHTEST)
		IDirect3DDevice9_SetRenderState(pD3DDev9, D3DRS_ZENABLE, FALSE);
	else
		IDirect3DDevice9_SetRenderState(pD3DDev9, D3DRS_ZENABLE, TRUE);

	switch(bits & (SBITS_MISC_DEPTHEQUALONLY|SBITS_MISC_DEPTHCLOSERONLY))
	{
	default:
	case 0:
		IDirect3DDevice9_SetRenderState(pD3DDev9, D3DRS_ZFUNC, D3DCMP_LESSEQUAL);
		break;
	case SBITS_MISC_DEPTHEQUALONLY:
		IDirect3DDevice9_SetRenderState(pD3DDev9, D3DRS_ZFUNC, D3DCMP_EQUAL);
		break;
	case SBITS_MISC_DEPTHCLOSERONLY:
		IDirect3DDevice9_SetRenderState(pD3DDev9, D3DRS_ZFUNC, D3DCMP_LESS);
		break;
	}
}

unsigned int workingbuffer[65536];
static void BE_DrawMeshChain_Internal(mesh_t *meshchain)
{
	float *xyz;
	unsigned int *colour;
	float *st[1];
	unsigned int fmt = 0;
	unsigned int stride;
	int i;

	if (!d3dbackend.curshader)
		return;
	if (!d3dbackend.curtexnums)
		return;

	stride = 0;
memset(workingbuffer, 0, sizeof(workingbuffer));
	xyz = (float*)(workingbuffer+stride);
	stride += 3;
	fmt |= D3DFVF_XYZ;

	colour = (unsigned int*)(workingbuffer+stride);
	stride += 1;
	fmt |= D3DFVF_DIFFUSE;

	st[0] = (float*)(workingbuffer+stride);
	stride += 2;
	fmt |= D3DFVF_TEX1;

	D3DBE_ApplyShaderBits(d3dbackend.curshader->passes[0].shaderbits);

	IDirect3DDevice9_SetFVF(pD3DDev9, fmt);

	switch(d3dbackend.curshader->passes[0].texgen)
	{
	default:
	case T_GEN_DIFFUSE:
		IDirect3DDevice9_SetTexture (pD3DDev9, 0, d3dbackend.curtexnums->base.ptr);
		break;
	case T_GEN_SINGLEMAP:
		IDirect3DDevice9_SetTexture (pD3DDev9, 0, d3dbackend.curshader->passes[0].anim_frames[0].ptr);
		break;
	}
	switch(d3dbackend.curshader->passes[1].texgen)
	{
	case T_GEN_LIGHTMAP:
		IDirect3DDevice9_SetTexture (pD3DDev9, 1, d3dbackend.curlightmap.ptr);

		IDirect3DDevice9_SetTextureStageState(pD3DDev9, 1, D3DTSS_COLORARG1, D3DTA_TEXTURE);
		IDirect3DDevice9_SetTextureStageState(pD3DDev9, 1, D3DTSS_COLORARG2, D3DTA_CURRENT);
		IDirect3DDevice9_SetTextureStageState(pD3DDev9, 1, D3DTSS_COLOROP, D3DTOP_MODULATE2X);
		break;
	default:
		IDirect3DDevice9_SetTexture (pD3DDev9, 1, NULL);
		IDirect3DDevice9_SetTextureStageState(pD3DDev9, 1, D3DTSS_COLOROP, D3DTOP_DISABLE);
		break;
	}

	if (meshchain->colors4b_array)
	{
		/*I'm guessing I ought to create a temp vertex buffer for this*/
		for (; meshchain; meshchain = meshchain->next)
		{
			for (i = 0; i < meshchain->numvertexes; i++)
			{
				VectorCopy(meshchain->xyz_array[i], (xyz+stride*i));
				*(colour+stride*i) = ((unsigned int)meshchain->colors4b_array[i][3]<<24) | ((unsigned int)meshchain->colors4b_array[i][0]<<16) | ((unsigned int)meshchain->colors4b_array[i][1]<<8) | ((unsigned int)meshchain->colors4b_array[i][2]);
				Vector2Copy(meshchain->st_array[i], (st[0]+stride*i));
			}
			IDirect3DDevice9_DrawIndexedPrimitiveUP(pD3DDev9, D3DPT_TRIANGLELIST, 0, meshchain->numvertexes, meshchain->numindexes/3, meshchain->indexes, D3DFMT_QINDEX, workingbuffer, stride*4);
		}
	}
	else
	{
		/*I'm guessing I ought to create a temp vertex buffer for this*/
		for (; meshchain; meshchain = meshchain->next)
		{
			for (i = 0; i < meshchain->numvertexes; i++)
			{
				VectorCopy(meshchain->xyz_array[i], (xyz+stride*i));
				*(colour+stride*i) = 0xffffffff;//*(unsigned int*)meshchain->colors4b_array[i];
				Vector2Copy(meshchain->st_array[i], (st[0]+stride*i));
			}
			IDirect3DDevice9_DrawIndexedPrimitiveUP(pD3DDev9, D3DPT_TRIANGLELIST, 0, meshchain->numvertexes, meshchain->numindexes/3, meshchain->indexes, D3DFMT_QINDEX, workingbuffer, stride*4);
		}
	}
}

void BE_DrawMeshChain(shader_t *shader, mesh_t *meshchain, vbo_t *vbo, texnums_t *texnums)
{
	d3dbackend.curshader = shader;
	d3dbackend.curtexnums = texnums;
	BE_DrawMeshChain_Internal(meshchain);
}

void BE_SelectMode(backendmode_t mode, unsigned int flags)
{
}

/*Generates an optimised vbo for each of the given model's textures*/
void BE_GenBrushModelVBO(model_t *mod)
{
}
/*Wipes a vbo*/
void BE_ClearVBO(vbo_t *vbo)
{
}

/*upload all lightmaps at the start to reduce lags*/
void BE_UploadAllLightmaps(void)
{
}










static void DrawSurfaceChain(msurface_t *s, shader_t *shader, vbo_t *vbo)
{	//doesn't merge surfaces, but tells gl to do each vertex arrayed surface individually, which means no vertex copying.
	int i;
	mesh_t *ml, *m;

	if (!vbo)
		return;

	d3dbackend.curshader = shader;
	d3dbackend.curtexnums = &shader->defaulttextures;
	
	ml = NULL;
	for (; s ; s=s->texturechain)
	{
		m = s->mesh;
		if (!m)	//urm.
			continue;
		if (m->numvertexes <= 1)
			continue;

		if (s->lightmaptexturenum < 0)
		{
			//pull out the surfaces with no lightmap info
			m->next = ml;
			ml = m;
		}
		else
		{
			//surfaces that do have a lightmap
			m->next = lightmap[s->lightmaptexturenum]->meshchain;
			lightmap[s->lightmaptexturenum]->meshchain = m;
		}
	}

	if (ml)
	{
		//draw the lightmapless surfaces
		BE_DrawMeshChain_Internal(ml);
	}

	//and then draw the lit chains
	for (i = 0; i < numlightmaps; i++)
	{
		if (!lightmap[i] || !lightmap[i]->meshchain)
			continue;
/*
		if (lightmap[i]->modified)
		{
			glRect_t *theRect;
			lightmap[i]->modified = false;
			theRect = &lightmap[i]->rectchange;
			GL_Bind(lightmap_textures[i]);
			qglTexSubImage2D(GL_TEXTURE_2D, 0, 0, theRect->t, 
				LMBLOCK_WIDTH, theRect->h, gl_lightmap_format, GL_UNSIGNED_BYTE,
				lightmap[i]->lightmaps+(theRect->t) *LMBLOCK_WIDTH*lightmap_bytes);
			theRect->l = LMBLOCK_WIDTH;
			theRect->t = LMBLOCK_HEIGHT;
			theRect->h = 0;
			theRect->w = 0;
			checkerror();

			if (lightmap[i]->deluxmodified)
			{
				lightmap[i]->deluxmodified = false;
				theRect = &lightmap[i]->deluxrectchange;
				GL_Bind(deluxmap_textures[i]);
				qglTexSubImage2D(GL_TEXTURE_2D, 0, 0, theRect->t, 
					LMBLOCK_WIDTH, theRect->h, GL_RGB, GL_UNSIGNED_BYTE,
					lightmap[i]->deluxmaps+(theRect->t) *LMBLOCK_WIDTH*3);
				theRect->l = LMBLOCK_WIDTH;
				theRect->t = LMBLOCK_HEIGHT;
				theRect->h = 0;
				theRect->w = 0;
				checkerror();
			}
		}
*/
		d3dbackend.curlightmap = lightmap_textures[i];
		d3dbackend.curdeluxmap = deluxmap_textures[i];
		BE_DrawMeshChain_Internal(lightmap[i]->meshchain);
		lightmap[i]->meshchain = NULL;
	}
}

static void BE_BaseTextureChain(msurface_t *first)
{
	texture_t *t, *tex;
	shader_t *shader;
	t = first->texinfo->texture;
	tex = R_TextureAnimation (t);

	//TEMP: use shader as an input parameter, not tex.
	shader = tex->shader;
	if (!shader)
	{
		shader = R_RegisterShader_Lightmap(tex->name);
		tex->shader = shader;
	}
	DrawSurfaceChain(first, shader, &t->vbo);
}

void BE_BaseEntTextures(void)
{
	extern model_t *currentmodel;
	int		i;
	entity_t *ent;

	if (!r_drawentities.ival)
		return;

	// draw sprites seperately, because of alpha blending
	for (i=0 ; i<cl_numvisedicts ; i++)
	{
		ent = &cl_visedicts[i];
		if (!ent->model)
			continue;
		if (ent->model->needload)
			continue;
//		if (!PPL_ShouldDraw())
//			continue;
		switch(ent->model->type)
		{
		case mod_brush:
//			BaseBrushTextures(ent);
			break;
		case mod_alias:
			R_DrawGAliasModel (ent, BEM_STANDARD);
			break;
		}
	}
}

void BE_SubmitMeshes (void)
{
	texture_t *t;
	msurface_t *s;
	int i;
	model_t *model = cl.worldmodel;
	unsigned int fl;

	for (i=0 ; i<model->numtextures ; i++)
	{
		t = model->textures[i];
		if (!t)
			continue;
		s = t->texturechain;
		if (!s)
			continue;

		fl = s->texinfo->texture->shader->flags;
		BE_BaseTextureChain(s);
	}

	BE_BaseEntTextures();
}

void BE_DrawWorld (qbyte *vis)
{
	RSpeedLocals();

	//make sure the world draws correctly
	r_worldentity.shaderRGBAf[0] = 1;
	r_worldentity.shaderRGBAf[1] = 1;
	r_worldentity.shaderRGBAf[2] = 1;
	r_worldentity.shaderRGBAf[3] = 1;
	r_worldentity.axis[0][0] = 1;
	r_worldentity.axis[1][1] = 1;
	r_worldentity.axis[2][2] = 1;

	BE_SelectMode(BEM_STANDARD, 0);

	RSpeedRemark();
	BE_SubmitMeshes();
	RSpeedEnd(RSPEED_WORLD);
}


#endif
