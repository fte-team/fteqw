#include "quakedef.h"
#ifdef D3DQUAKE
#include "shader.h"

#include <d3d9.h>
LPDIRECT3DDEVICE9 pD3DDev9;

extern int be_maxpasses;

void BE_Init(void)
{
	be_maxpasses = 1;
}

static D3DBE_ApplyShaderBits(unsigned int bits)
{
	if (bits & SBITS_BLEND_BITS)
	{
		int src;
		int dst;

		switch(bits & SBITS_SRCBLEND_BITS)
		{
		case SBITS_SRCBLEND_ZERO:					dst = D3DBLEND_ZERO; break;
		case SBITS_SRCBLEND_ONE:					dst = D3DBLEND_ONE; break;
		case SBITS_SRCBLEND_DST_COLOR:				src = D3DBLEND_DESTCOLOR; break;
		case SBITS_SRCBLEND_ONE_MINUS_DST_COLOR:	src = D3DBLEND_INVDESTCOLOR; break;
		case SBITS_SRCBLEND_SRC_ALPHA:				src = D3DBLEND_SRCALPHA; break;
		case SBITS_SRCBLEND_ONE_MINUS_SRC_ALPHA:	src = D3DBLEND_INVSRCALPHA; break;
		case SBITS_SRCBLEND_DST_ALPHA:				src = D3DBLEND_DESTALPHA; break;
		case SBITS_SRCBLEND_ONE_MINUS_DST_ALPHA:	src = D3DBLEND_INVDESTALPHA; break;
		case SBITS_SRCBLEND_ALPHA_SATURATE:			src = D3DBLEND_SRCALPHASAT; break;
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
void BE_DrawMeshChain(shader_t *shader, mesh_t *meshchain, vbo_t *vbo, texnums_t *texnums)
{
	float *xyz;
	unsigned int *colour;
	float *st[1];
	unsigned int fmt = 0;
	unsigned int stride;
	int i;

	if (!shader)
		return;
	if (!texnums)
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

	D3DBE_ApplyShaderBits(shader->passes[0].shaderbits);

	IDirect3DDevice9_SetFVF(pD3DDev9, fmt);

	switch(shader->passes[0].texgen)
	{
	case T_GEN_DIFFUSE:
		IDirect3DDevice9_SetTexture (pD3DDev9, 0, texnums->base.ptr);
		break;
	case T_GEN_SINGLEMAP:
		IDirect3DDevice9_SetTexture (pD3DDev9, 0, shader->passes[0].anim_frames[0].ptr);
		break;
	}

	/*I'm guessing I ought to create a temp vertex buffer for this*/
	for (i = 0; i < meshchain->numvertexes; i++)
	{
		VectorCopy(meshchain->xyz_array[i], (xyz+stride*i));
		*(colour+stride*i) = 0xffffffff;//*(unsigned int*)meshchain->colors4b_array[i];
		Vector2Copy(meshchain->st_array[i], (st[0]+stride*i));
	}
	IDirect3DDevice9_DrawIndexedPrimitiveUP(pD3DDev9, D3DPT_TRIANGLELIST, 0, meshchain->numvertexes, meshchain->numindexes/3, meshchain->indexes, D3DFMT_QINDEX, workingbuffer, stride*4);
}

void BE_SelectMode(backendmode_t mode, unsigned int flags)
{
}

void BE_ClearVBO(vbo_t *vbo)
{
}
#endif
