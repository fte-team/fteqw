#include "quakedef.h"
#ifdef D3DQUAKE
#include "shader.h"
#include <d3d9.h>

#include <GL/gl.h>
#include "glsupp.h"

extern LPDIRECT3DDEVICE9 pD3DDev9;

//#define d3dcheck(foo) foo
#define d3dcheck(foo) do{HRESULT err = foo; if (FAILED(err)) Sys_Error("D3D reported error on line %i - error %x\n", __LINE__, err);} while(0)

#define MAX_TMUS 4

/*========================================== tables for deforms =====================================*/
#define frand() (rand()*(1.0/RAND_MAX))
#define FTABLE_SIZE		1024
#define FTABLE_CLAMP(x)	(((int)((x)*FTABLE_SIZE) & (FTABLE_SIZE-1)))
#define FTABLE_EVALUATE(table,x) (table ? table[FTABLE_CLAMP(x)] : frand()*((x)-floor(x)))

static	float	r_sintable[FTABLE_SIZE];
static	float	r_triangletable[FTABLE_SIZE];
static	float	r_squaretable[FTABLE_SIZE];
static	float	r_sawtoothtable[FTABLE_SIZE];
static	float	r_inversesawtoothtable[FTABLE_SIZE];

static float *FTableForFunc ( unsigned int func )
{
	switch (func)
	{
		case SHADER_FUNC_SIN:
			return r_sintable;

		case SHADER_FUNC_TRIANGLE:
			return r_triangletable;

		case SHADER_FUNC_SQUARE:
			return r_squaretable;

		case SHADER_FUNC_SAWTOOTH:
			return r_sawtoothtable;

		case SHADER_FUNC_INVERSESAWTOOTH:
			return r_inversesawtoothtable;
	}

	//bad values allow us to crash (so I can debug em)
	return NULL;
}

static void FTable_Init(void)
{
	unsigned int i;
	double t;
	for (i = 0; i < FTABLE_SIZE; i++)
	{
		t = (double)i / (double)FTABLE_SIZE;

		r_sintable[i] = sin(t * 2*M_PI);
		
		if (t < 0.25) 
			r_triangletable[i] = t * 4.0;
		else if (t < 0.75)
			r_triangletable[i] = 2 - 4.0 * t;
		else
			r_triangletable[i] = (t - 0.75) * 4.0 - 1.0;

		if (t < 0.5) 
			r_squaretable[i] = 1.0f;
		else
			r_squaretable[i] = -1.0f;

		r_sawtoothtable[i] = t;
		r_inversesawtoothtable[i] = 1.0 - t;
	}
}
/*================================================*/

typedef struct
{
	backendmode_t mode;
	unsigned int flags;

	float		curtime;
	entity_t    *curentity;
	shader_t	*curshader;
	texnums_t	*curtexnums;
	texid_t		curlightmap;
	texid_t		curdeluxmap;
	int			curvertdecl;
	unsigned int shaderbits;
	unsigned int lastpasscount;

	mesh_t		**meshlist;
	unsigned int nummeshes;

	D3DCOLOR	passcolour;
	qboolean	passsinglecolour;

	/*FIXME: we shouldn't lock these so much - we need to cache which batches have been submitted and set up streams separately from the vertex data*/
	IDirect3DVertexBuffer9 *dynxyz_buff;
	unsigned int dynxyz_offs;
	unsigned int dynxyz_size;

	IDirect3DVertexBuffer9 *dynst_buff[MAX_TMUS];
	unsigned int dynst_offs[MAX_TMUS];
	unsigned int dynst_size;

	IDirect3DVertexBuffer9 *dyncol_buff;
	unsigned int dyncol_offs;
	unsigned int dyncol_size;

	IDirect3DIndexBuffer9 *dynidx_buff;
	unsigned int dynidx_offs;
	unsigned int dynidx_size;
} d3dbackend_t;

#define DYNVBUFFSIZE 65536
#define DYNIBUFFSIZE 65536

static d3dbackend_t shaderstate;

extern int be_maxpasses;

enum 
{
	D3D_VDEC_COL4B = 1<<0,
	D3D_VDEC_NORMS = 1<<1,
	D3D_VDEC_ST0 = 1<<2,
	D3D_VDEC_ST1 = 1<<3,
	D3D_VDEC_ST2 = 1<<3,
	D3D_VDEC_ST3 = 1<<3,
	D3D_VDEC_MAX = 16
};
IDirect3DVertexDeclaration9 *vertexdecls[D3D_VDEC_MAX];

void BE_Init(void)
{
	D3DVERTEXELEMENT9 decl[8], declend=D3DDECL_END();
	int elements;
	int i, tmu;

	be_maxpasses = 1;
	shaderstate.curvertdecl = -1;

	FTable_Init();

	shaderstate.dynxyz_size = sizeof(vecV_t) * DYNVBUFFSIZE;
	shaderstate.dyncol_size = sizeof(byte_vec4_t) * DYNVBUFFSIZE;
	shaderstate.dynst_size = sizeof(vec2_t) * DYNVBUFFSIZE;
	shaderstate.dynidx_size = sizeof(index_t) * DYNIBUFFSIZE;
	IDirect3DDevice9_CreateVertexBuffer(pD3DDev9, shaderstate.dynxyz_size, D3DUSAGE_DYNAMIC|D3DUSAGE_WRITEONLY, 0, D3DPOOL_DEFAULT, &shaderstate.dynxyz_buff, NULL);
	for (tmu = 0; tmu < MAX_TMUS; tmu++)
		IDirect3DDevice9_CreateVertexBuffer(pD3DDev9, shaderstate.dynst_size, D3DUSAGE_DYNAMIC|D3DUSAGE_WRITEONLY, 0, D3DPOOL_DEFAULT, &shaderstate.dynst_buff[tmu], NULL);
	IDirect3DDevice9_CreateVertexBuffer(pD3DDev9, shaderstate.dyncol_size, D3DUSAGE_DYNAMIC|D3DUSAGE_WRITEONLY, 0, D3DPOOL_DEFAULT, &shaderstate.dyncol_buff, NULL);
	IDirect3DDevice9_CreateIndexBuffer(pD3DDev9, shaderstate.dynidx_size, D3DUSAGE_DYNAMIC|D3DUSAGE_WRITEONLY, D3DFMT_QINDEX, D3DPOOL_DEFAULT, &shaderstate.dynidx_buff, NULL);

	for (i = 0; i < D3D_VDEC_MAX; i++)
	{
		elements = 0;

		decl[elements].Stream = 0;
		decl[elements].Offset = 0;
		decl[elements].Type = D3DDECLTYPE_FLOAT3;
		decl[elements].Method = D3DDECLMETHOD_DEFAULT;
		decl[elements].Usage = D3DDECLUSAGE_POSITION;
		decl[elements].UsageIndex = 0;
		elements++;

		if (i & D3D_VDEC_COL4B)
		{
			decl[elements].Stream = 1;
			decl[elements].Offset = 0;
			decl[elements].Type = D3DDECLTYPE_D3DCOLOR;
			decl[elements].Method = D3DDECLMETHOD_DEFAULT;
			decl[elements].Usage = D3DDECLUSAGE_COLOR;
			decl[elements].UsageIndex = 0;
			elements++;
		}

		if (i & D3D_VDEC_NORMS)
		{
			decl[elements].Stream = 2;
			decl[elements].Offset = 0;
			decl[elements].Type = D3DDECLTYPE_FLOAT2;
			decl[elements].Method = D3DDECLMETHOD_DEFAULT;
			decl[elements].Usage = D3DDECLUSAGE_TEXCOORD;
			decl[elements].UsageIndex = 1;
			elements++;

			decl[elements].Stream = 3;
			decl[elements].Offset = 0;
			decl[elements].Type = D3DDECLTYPE_FLOAT2;
			decl[elements].Method = D3DDECLMETHOD_DEFAULT;
			decl[elements].Usage = D3DDECLUSAGE_TEXCOORD;
			decl[elements].UsageIndex = 1;
			elements++;

			decl[elements].Stream = 4;
			decl[elements].Offset = 0;
			decl[elements].Type = D3DDECLTYPE_FLOAT2;
			decl[elements].Method = D3DDECLMETHOD_DEFAULT;
			decl[elements].Usage = D3DDECLUSAGE_TEXCOORD;
			decl[elements].UsageIndex = 1;
			elements++;
		}

		for (tmu = 0; tmu < MAX_TMUS; tmu++)
		{
			if (i & (D3D_VDEC_ST0<<tmu))
			{
				decl[elements].Stream = 5+tmu;
				decl[elements].Offset = 0;
				decl[elements].Type = D3DDECLTYPE_FLOAT2;
				decl[elements].Method = D3DDECLMETHOD_DEFAULT;
				decl[elements].Usage = D3DDECLUSAGE_TEXCOORD;
				decl[elements].UsageIndex = tmu;
				elements++;
			}
		}

		decl[elements] = declend;
		elements++;

		IDirect3DDevice9_CreateVertexDeclaration(pD3DDev9, decl, &vertexdecls[i]);
	}
}

static void D3DBE_ApplyShaderBits(unsigned int bits)
{
	unsigned int delta;
	delta = bits ^ shaderstate.shaderbits;
	delta = ~0;
	if (!delta)
		return;
	shaderstate.shaderbits = bits;

	if (delta & SBITS_BLEND_BITS)
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
	}

	if (delta & SBITS_ATEST_BITS)
	{
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
	}

	if (delta & SBITS_MISC_DEPTHWRITE)
	{
		if (bits & SBITS_MISC_DEPTHWRITE)
			IDirect3DDevice9_SetRenderState(pD3DDev9, D3DRS_ZWRITEENABLE, TRUE);
		else
			IDirect3DDevice9_SetRenderState(pD3DDev9, D3DRS_ZWRITEENABLE, FALSE);
	}

	if(delta & SBITS_MISC_NODEPTHTEST)
	{
		if(bits & SBITS_MISC_NODEPTHTEST)
			IDirect3DDevice9_SetRenderState(pD3DDev9, D3DRS_ZENABLE, FALSE);
		else
			IDirect3DDevice9_SetRenderState(pD3DDev9, D3DRS_ZENABLE, TRUE);
	}

	if (delta & (SBITS_MISC_DEPTHEQUALONLY|SBITS_MISC_DEPTHCLOSERONLY))
	{
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
}

static void allocvertexbuffer(IDirect3DVertexBuffer9 *buff, unsigned int bmaxsize, unsigned int *offset, void **data, unsigned int bytes)
{
	unsigned int boff;
	if (*offset + bytes > bmaxsize)
	{
		boff = 0;
		*offset = bytes;
	}
	else
	{
		boff = *offset;
		*offset += bytes;
	}
	d3dcheck(IDirect3DVertexBuffer9_Lock(buff, boff, bytes, data, boff?D3DLOCK_NOOVERWRITE:D3DLOCK_DISCARD));
}

static unsigned int allocindexbuffer(void **dest, unsigned int entries)
{
	unsigned int bytes = entries*sizeof(index_t);
	unsigned int offset;

	if (shaderstate.dynidx_offs + bytes > DYNIBUFFSIZE)
	{
		offset = 0;
		shaderstate.dynidx_offs = 0;
	}
	else
	{
		offset = shaderstate.dynidx_offs;
		shaderstate.dynidx_offs += bytes;
	}

	d3dcheck(IDirect3DIndexBuffer9_Lock(shaderstate.dynidx_buff, offset, entries, dest, offset?D3DLOCK_NOOVERWRITE:D3DLOCK_DISCARD));
	return offset/sizeof(index_t);
}

static void SelectPassTexture(unsigned int tu, shaderpass_t *pass)
{
	switch(pass->texgen)
	{
	default:
	case T_GEN_DIFFUSE:
		IDirect3DDevice9_SetTexture (pD3DDev9, tu, shaderstate.curtexnums->base.ptr);
		break;
	case T_GEN_NORMALMAP:
		IDirect3DDevice9_SetTexture (pD3DDev9, tu, shaderstate.curtexnums->bump.ptr);
		break;
	case T_GEN_SPECULAR:
		IDirect3DDevice9_SetTexture (pD3DDev9, tu, shaderstate.curtexnums->specular.ptr);
		break;
	case T_GEN_UPPEROVERLAY:
		IDirect3DDevice9_SetTexture (pD3DDev9, tu, shaderstate.curtexnums->upperoverlay.ptr);
		break;
	case T_GEN_LOWEROVERLAY:
		IDirect3DDevice9_SetTexture (pD3DDev9, tu, shaderstate.curtexnums->loweroverlay.ptr);
		break;
	case T_GEN_FULLBRIGHT:
		IDirect3DDevice9_SetTexture (pD3DDev9, tu, shaderstate.curtexnums->fullbright.ptr);
		break;
	case T_GEN_ANIMMAP:
		IDirect3DDevice9_SetTexture (pD3DDev9, tu, pass->anim_frames[(int)(pass->anim_fps * shaderstate.curtime) % pass->anim_numframes].ptr);
		break;
		/*fixme*/
	case T_GEN_SINGLEMAP:
		IDirect3DDevice9_SetTexture (pD3DDev9, tu, pass->anim_frames[0].ptr);
		break;
	case T_GEN_DELUXMAP:
		IDirect3DDevice9_SetTexture (pD3DDev9, tu, shaderstate.curdeluxmap.ptr);
		break;
	case T_GEN_LIGHTMAP:
		IDirect3DDevice9_SetTexture (pD3DDev9, tu, shaderstate.curlightmap.ptr);
		break;

	/*case T_GEN_CURRENTRENDER:
		FIXME: no code to grab the current screen and convert to a texture
		break;*/
	case T_GEN_VIDEOMAP:
		IDirect3DDevice9_SetTexture (pD3DDev9, tu, Media_UpdateForShader(pass->cin).ptr);
		break;
	}

	/*lightmaps don't use mipmaps*/
	if (pass->flags & SHADER_PASS_NOMIPMAP)
	{
		IDirect3DDevice9_SetSamplerState(pD3DDev9, tu, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
		IDirect3DDevice9_SetSamplerState(pD3DDev9, tu, D3DSAMP_MIPFILTER, D3DTEXF_NONE);
		IDirect3DDevice9_SetSamplerState(pD3DDev9, tu, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
	}
	else
	{
		IDirect3DDevice9_SetSamplerState(pD3DDev9, tu, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
		IDirect3DDevice9_SetSamplerState(pD3DDev9, tu, D3DSAMP_MIPFILTER, D3DTEXF_LINEAR);
		IDirect3DDevice9_SetSamplerState(pD3DDev9, tu, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
	}

	switch (pass->blendmode)
	{
	case GL_DOT3_RGB_ARB:
		IDirect3DDevice9_SetTextureStageState(pD3DDev9, tu, D3DTSS_COLORARG1, D3DTA_TEXTURE);
		IDirect3DDevice9_SetTextureStageState(pD3DDev9, tu, D3DTSS_COLORARG2, D3DTA_CURRENT);
		IDirect3DDevice9_SetTextureStageState(pD3DDev9, tu, D3DTSS_COLOROP, D3DTOP_DOTPRODUCT3);
		break;
	case GL_REPLACE:
		IDirect3DDevice9_SetTextureStageState(pD3DDev9, tu, D3DTSS_COLORARG1, D3DTA_TEXTURE);
		IDirect3DDevice9_SetTextureStageState(pD3DDev9, tu, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
		break;
	case GL_ADD:
		IDirect3DDevice9_SetTextureStageState(pD3DDev9, tu, D3DTSS_COLORARG1, D3DTA_TEXTURE);
		IDirect3DDevice9_SetTextureStageState(pD3DDev9, tu, D3DTSS_COLORARG2, D3DTA_CURRENT);
		if (tu)
			IDirect3DDevice9_SetTextureStageState(pD3DDev9, tu, D3DTSS_COLOROP, D3DTOP_ADD);
		break;
	case GL_DECAL:
		IDirect3DDevice9_SetTextureStageState(pD3DDev9, tu, D3DTSS_COLORARG1, D3DTA_TEXTURE);
		IDirect3DDevice9_SetTextureStageState(pD3DDev9, tu, D3DTSS_COLORARG2, D3DTA_CURRENT);
		IDirect3DDevice9_SetTextureStageState(pD3DDev9, tu, D3DTSS_COLOROP, D3DTOP_MODULATE);
		break;
	default:
	case GL_MODULATE:
		IDirect3DDevice9_SetTextureStageState(pD3DDev9, tu, D3DTSS_COLORARG1, D3DTA_TEXTURE);
		IDirect3DDevice9_SetTextureStageState(pD3DDev9, tu, D3DTSS_COLORARG2, D3DTA_CURRENT);
		IDirect3DDevice9_SetTextureStageState(pD3DDev9, tu, D3DTSS_COLOROP, D3DTOP_MODULATE);
		break;
	}
	IDirect3DDevice9_SetTextureStageState(pD3DDev9, tu, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
	IDirect3DDevice9_SetTextureStageState(pD3DDev9, tu, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);
	IDirect3DDevice9_SetTextureStageState(pD3DDev9, tu, D3DTSS_ALPHAOP, D3DTOP_MODULATE);

	if (tu == 0)
	{
		if (shaderstate.passsinglecolour)
		{
			IDirect3DDevice9_SetTextureStageState(pD3DDev9, tu, D3DTSS_COLORARG2, D3DTA_CONSTANT);
			IDirect3DDevice9_SetTextureStageState(pD3DDev9, tu, D3DTSS_CONSTANT, shaderstate.passcolour);
		}
		else
			IDirect3DDevice9_SetTextureStageState(pD3DDev9, tu, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
	}
}

static void colourgenbyte(const shaderpass_t *pass, int cnt, const byte_vec4_t *src, byte_vec4_t *dst, const mesh_t *mesh)
{
	D3DCOLOR block;
	switch (pass->rgbgen)
	{
	case RGB_GEN_ENTITY:
		block = D3DCOLOR_COLORVALUE(shaderstate.curentity->shaderRGBAf[0], shaderstate.curentity->shaderRGBAf[1], shaderstate.curentity->shaderRGBAf[2], shaderstate.curentity->shaderRGBAf[3]);
		while((cnt)--)
		{
			((D3DCOLOR*)dst)[cnt] = block;
		}
		break;
	case RGB_GEN_ONE_MINUS_ENTITY:
		block = D3DCOLOR_COLORVALUE(1-shaderstate.curentity->shaderRGBAf[0], 1-shaderstate.curentity->shaderRGBAf[1], 1-shaderstate.curentity->shaderRGBAf[2], 1-shaderstate.curentity->shaderRGBAf[3]);
		while((cnt)--)
		{
			((D3DCOLOR*)dst)[cnt] = block;
		}
		break;
	case RGB_GEN_VERTEX:
	case RGB_GEN_EXACT_VERTEX:
		while((cnt)--)
		{
			((D3DCOLOR*)dst)[cnt] = ((D3DCOLOR*)src)[cnt];
		}
		break;
	case RGB_GEN_ONE_MINUS_VERTEX:
		while((cnt)--)
		{
			dst[cnt][0] = 255-src[cnt][0];
			dst[cnt][1] = 255-src[cnt][1];
			dst[cnt][2] = 255-src[cnt][2];
		}
		break;
	case RGB_GEN_IDENTITY_LIGHTING:
		//compensate for overbrights
		while((cnt)--)
		{
			dst[cnt][0] = 255;//shaderstate.identitylighting;
			dst[cnt][1] = 255;//shaderstate.identitylighting;
			dst[cnt][2] = 255;//shaderstate.identitylighting;
		}
		break;
	default:
	case RGB_GEN_IDENTITY:
		block = D3DCOLOR_RGBA(255, 255, 255, 255);
		while((cnt)--)
		{
			((D3DCOLOR*)dst)[cnt] = block;
		}
		break;
	case RGB_GEN_CONST:
		block = D3DCOLOR_COLORVALUE(pass->rgbgen_func.args[0], pass->rgbgen_func.args[1], pass->rgbgen_func.args[2], 1);
		while((cnt)--)
		{
			((D3DCOLOR*)dst)[cnt] = block;
		}
		break;
	case RGB_GEN_LIGHTING_DIFFUSE:
		//collect lighting details for mobile entities
		if (!mesh->normals_array)
		{
			block = D3DCOLOR_RGBA(255, 255, 255, 255);
			while((cnt)--)
			{
				((D3DCOLOR*)dst)[cnt] = block;
			}
		}
		else
		{
			R_LightArraysByte(mesh->xyz_array, dst, cnt, mesh->normals_array);
		}
		break;
	case RGB_GEN_WAVE:
		{
			float *table;
			float c;

			table = FTableForFunc(pass->rgbgen_func.type);
			c = pass->rgbgen_func.args[2] + shaderstate.curtime * pass->rgbgen_func.args[3];
			c = FTABLE_EVALUATE(table, c) * pass->rgbgen_func.args[1] + pass->rgbgen_func.args[0];
			c = bound(0.0f, c, 1.0f);
			block = D3DCOLOR_COLORVALUE(c, c, c, 1);

			while((cnt)--)
			{
				((D3DCOLOR*)dst)[cnt] = block;
			}
		}
		break;

	case RGB_GEN_TOPCOLOR:
	case RGB_GEN_BOTTOMCOLOR:
#pragma message("fix 24bit player colours")
		block = D3DCOLOR_RGBA(255, 255, 255, 255);
		while((cnt)--)
		{
			((D3DCOLOR*)dst)[cnt] = block;
		}
	//	Con_Printf("RGB_GEN %i not supported\n", pass->rgbgen);
		break;
	}
}

static void alphagenbyte(const shaderpass_t *pass, int cnt, const byte_vec4_t *src, byte_vec4_t *dst, const mesh_t *mesh)
{
	/*FIXME: Skip this if the rgbgen did it*/
	float *table;
	unsigned char t;
	float f;
	vec3_t v1, v2;

	switch (pass->alphagen)
	{
	default:
	case ALPHA_GEN_IDENTITY:
		while(cnt--)
			dst[cnt][3] = 255;
		break;

	case ALPHA_GEN_CONST:
		t = pass->alphagen_func.args[0]*255;
		while(cnt--)
			dst[cnt][3] = t;
		break;

	case ALPHA_GEN_WAVE:
		table = FTableForFunc(pass->alphagen_func.type);
		f = pass->alphagen_func.args[2] + shaderstate.curtime * pass->alphagen_func.args[3];
		f = FTABLE_EVALUATE(table, f) * pass->alphagen_func.args[1] + pass->alphagen_func.args[0];
		t = bound(0.0f, f, 1.0f)*255;
		while(cnt--)
			dst[cnt][3] = t;
		break;

	case ALPHA_GEN_PORTAL:
		//FIXME: should this be per-vert?
		VectorAdd(mesh->xyz_array[0], shaderstate.curentity->origin, v1);
		VectorSubtract(r_origin, v1, v2);
		f = VectorLength(v2) * (1.0 / 255.0);
		t = bound(0.0f, f, 1.0f)*255;

		while(cnt--)
			dst[cnt][3] = t;
		break;

	case ALPHA_GEN_VERTEX:
		if (!src)
		{
			while(cnt--)
			{
				dst[cnt][3] = 255;
			}
			break;
		}

		while(cnt--)
		{
			dst[cnt][3] = src[cnt][3];
		}
		break;

	case ALPHA_GEN_ENTITY:
		t = bound(0, shaderstate.curentity->shaderRGBAf[3], 1)*255;
		while(cnt--)
		{
			dst[cnt][3] = t;
		}
		break;

	/*FIXME: specular not supported (most noticable on q3dm0)*/
	/*case ALPHA_GEN_SPECULAR:
		{
			mat3_t axis;
			AngleVectors(shaderstate.curentity->angles, axis[0], axis[1], axis[2]);
			VectorSubtract(r_origin, shaderstate.curentity->origin, v1);

			if (!Matrix3_Compare(axis, axisDefault))
			{
				Matrix3_Multiply_Vec3(axis, v2, v2);
			}
			else
			{
				VectorCopy(v1, v2);
			}

			for (i = 0; i < cnt; i++)
			{
				VectorSubtract(v2, mesh->xyz_array[i], v1);
				f = DotProduct(v1, mesh->normals_array[i] ) * Q_rsqrt(DotProduct(v1,v1));
				f = f * f * f * f * f;
				dst[i][3] = bound (0.0f, f, 1.0f);
			}
		}
		break;*/
	}
}

static unsigned int BE_GenerateColourMods(unsigned int vertcount, const shaderpass_t *pass)
{
	unsigned int ret = 0;
	unsigned char *map;
	const mesh_t *m;
	unsigned int mno;
	qboolean usearray;

	if (pass->flags & SHADER_PASS_NOCOLORARRAY)
		usearray = false;
	else
		usearray = true;

	m = shaderstate.meshlist[0];

	if (usearray && m->colors4b_array)
	{
		shaderstate.passsinglecolour = false;
		shaderstate.passcolour = D3DCOLOR_RGBA(255,255,255,255);

		ret |= D3D_VDEC_COL4B;
		allocvertexbuffer(shaderstate.dyncol_buff, shaderstate.dyncol_size, &shaderstate.dyncol_offs, (void**)&map, vertcount*sizeof(D3DCOLOR));
		for (vertcount = 0, mno = 0; mno < shaderstate.nummeshes; mno++)
		{
			m = shaderstate.meshlist[mno];
			/*FIXME: rgba->bgra...*/
			memcpy((char*)map+vertcount*sizeof(D3DCOLOR), m->colors4b_array, m->numvertexes*sizeof(D3DCOLOR));
			vertcount += m->numvertexes;
		}
		d3dcheck(IDirect3DVertexBuffer9_Unlock(shaderstate.dyncol_buff));
		d3dcheck(IDirect3DDevice9_SetStreamSource(pD3DDev9, 1, shaderstate.dyncol_buff, shaderstate.dyncol_offs - vertcount*sizeof(D3DCOLOR), sizeof(D3DCOLOR)));
	}
	else if (usearray && m->colors4f_array)
	{
		unsigned int v;
		float *src;
		shaderstate.passsinglecolour = false;
		shaderstate.passcolour = D3DCOLOR_RGBA(255,255,255,255);

		ret |= D3D_VDEC_COL4B;
		allocvertexbuffer(shaderstate.dyncol_buff, shaderstate.dyncol_size, &shaderstate.dyncol_offs, (void**)&map, vertcount*sizeof(D3DCOLOR));
		for (vertcount = 0, mno = 0; mno < shaderstate.nummeshes; mno++)
		{
			m = shaderstate.meshlist[mno];
			src = m->colors4f_array[0];
			for (v = 0; v < m->numvertexes; v++)
			{
				map[0] = src[2]*255;
				map[1] = src[1]*255;
				map[2] = src[0]*255;
				map[3] = src[3]*255;
				/*FIXME: no clamping here*/
				map += 4;
				src += 4;
			}
			vertcount += m->numvertexes;
		}
		map -= vertcount*4;
		/*FIXME: m is wrong. its the last ent only*/
		colourgenbyte(pass, vertcount, (byte_vec4_t*)map, (byte_vec4_t*)map, m);
		alphagenbyte(pass, vertcount, (byte_vec4_t*)map, (byte_vec4_t*)map, m);
		d3dcheck(IDirect3DVertexBuffer9_Unlock(shaderstate.dyncol_buff));
		d3dcheck(IDirect3DDevice9_SetStreamSource(pD3DDev9, 1, shaderstate.dyncol_buff, shaderstate.dyncol_offs - vertcount*sizeof(D3DCOLOR), sizeof(D3DCOLOR)));
	}
	else
	{
		shaderstate.passsinglecolour = true;
		shaderstate.passcolour = D3DCOLOR_RGBA(255,255,255,255);
		colourgenbyte(pass, 1, (byte_vec4_t*)&shaderstate.passcolour, (byte_vec4_t*)&shaderstate.passcolour, m);
		alphagenbyte(pass, 1, (byte_vec4_t*)&shaderstate.passcolour, (byte_vec4_t*)&shaderstate.passcolour, m);
		/*FIXME: just because there's no rgba set, there's no reason to assume it should be a single colour (unshaded ents)*/
		d3dcheck(IDirect3DDevice9_SetStreamSource(pD3DDev9, 1, NULL, 0, 0));
	}
	return ret;
}
/*********************************************************************************************************/
/*========================================== texture coord generation =====================================*/

static void tcgen_environment(float *st, unsigned int numverts, float *xyz, float *normal) 
{
	/*
	int			i;
	vec3_t		viewer, reflected;
	float		d;

	vec3_t		rorg;


	RotateLightVector(shaderstate.curentity->axis, shaderstate.curentity->origin, r_origin, rorg);

	for (i = 0 ; i < numverts ; i++, xyz += 3, normal += 3, st += 2 ) 
	{
		VectorSubtract (rorg, xyz, viewer);
		VectorNormalizeFast (viewer);

		d = DotProduct (normal, viewer);

		reflected[0] = normal[0]*2*d - viewer[0];
		reflected[1] = normal[1]*2*d - viewer[1];
		reflected[2] = normal[2]*2*d - viewer[2];

		st[0] = 0.5 + reflected[1] * 0.5;
		st[1] = 0.5 - reflected[2] * 0.5;
	}
	*/
}

static float *tcgen(const shaderpass_t *pass, int cnt, float *dst, const mesh_t *mesh)
{
	int i;
	vecV_t *src;
	switch (pass->tcgen)
	{
	default:
	case TC_GEN_BASE:
		return (float*)mesh->st_array;
	case TC_GEN_LIGHTMAP:
		return (float*)mesh->lmst_array;
	case TC_GEN_NORMAL:
		return (float*)mesh->normals_array;
	case TC_GEN_SVECTOR:
		return (float*)mesh->snormals_array;
	case TC_GEN_TVECTOR:
		return (float*)mesh->tnormals_array;
	case TC_GEN_ENVIRONMENT:
		tcgen_environment(dst, cnt, (float*)mesh->xyz_array, (float*)mesh->normals_array);
		return dst;

//	case TC_GEN_DOTPRODUCT:
//		return mesh->st_array[0];
	case TC_GEN_VECTOR:
		src = mesh->xyz_array;
		for (i = 0; i < cnt; i++, dst += 2)
		{
			static vec3_t tc_gen_s = { 1.0f, 0.0f, 0.0f };
			static vec3_t tc_gen_t = { 0.0f, 1.0f, 0.0f };

			dst[0] = DotProduct(tc_gen_s, src[i]);
			dst[1] = DotProduct(tc_gen_t, src[i]);
		}
		return dst;
	}
}

/*src and dst can be the same address when tcmods are chained*/
static void tcmod(const tcmod_t *tcmod, int cnt, const float *src, float *dst, const mesh_t *mesh)
{
	float *table;
	float t1, t2;
	float cost, sint;
	int j;
#define R_FastSin(x) sin((x)*(2*M_PI))
	switch (tcmod->type)
	{
		case SHADER_TCMOD_ROTATE:
			cost = tcmod->args[0] * shaderstate.curtime;
			sint = R_FastSin(cost);
			cost = R_FastSin(cost + 0.25);

			for (j = 0; j < cnt; j++, dst+=2,src+=2)
			{
				t1 = cost * (src[0] - 0.5f) - sint * (src[1] - 0.5f) + 0.5f;
				t2 = cost * (src[1] - 0.5f) + sint * (src[0] - 0.5f) + 0.5f;
				dst[0] = t1;
				dst[1] = t2;
			}
			break;

		case SHADER_TCMOD_SCALE:
			t1 = tcmod->args[0];
			t2 = tcmod->args[1];

			for (j = 0; j < cnt; j++, dst+=2,src+=2)
			{
				dst[0] = src[0] * t1;
				dst[1] = src[1] * t2;
			}
			break;

		case SHADER_TCMOD_TURB:
			t1 = tcmod->args[2] + shaderstate.curtime * tcmod->args[3];
			t2 = tcmod->args[1];

			for (j = 0; j < cnt; j++, dst+=2,src+=2)
			{
				dst[0] = src[0] + R_FastSin (src[0]*t2+t1) * t2;
				dst[1] = src[1] + R_FastSin (src[1]*t2+t1) * t2;
			}
			break;

		case SHADER_TCMOD_STRETCH:
			table = FTableForFunc(tcmod->args[0]);
			t2 = tcmod->args[3] + shaderstate.curtime * tcmod->args[4];
			t1 = FTABLE_EVALUATE(table, t2) * tcmod->args[2] + tcmod->args[1];
			t1 = t1 ? 1.0f / t1 : 1.0f;
			t2 = 0.5f - 0.5f * t1;
			for (j = 0; j < cnt; j++, dst+=2,src+=2)
			{
				dst[0] = src[0] * t1 + t2;
				dst[1] = src[1] * t1 + t2;
			}
			break;

		case SHADER_TCMOD_SCROLL:
			t1 = tcmod->args[0] * shaderstate.curtime;
			t2 = tcmod->args[1] * shaderstate.curtime;

			for (j = 0; j < cnt; j++, dst += 2, src+=2)
			{
				dst[0] = src[0] + t1;
				dst[1] = src[1] + t2;
			}
			break;

		case SHADER_TCMOD_TRANSFORM:
			for (j = 0; j < cnt; j++, dst+=2, src+=2)
			{
				t1 = src[0];
				t2 = src[1];
				dst[0] = t1 * tcmod->args[0] + t2 * tcmod->args[2] + tcmod->args[4];
				dst[1] = t2 * tcmod->args[1] + t1 * tcmod->args[3] + tcmod->args[5];
			}
			break;

		default:
			break;
	}
}

static void GenerateTCMods(const shaderpass_t *pass, float *dest)
{
	mesh_t *mesh;
	unsigned int fvertex = 0, mno;
	int i;
	float *src;
	for (mno = 0; mno < shaderstate.nummeshes; mno++)
	{
		mesh = shaderstate.meshlist[mno];
		src = tcgen(pass, mesh->numvertexes, dest, mesh);
		//tcgen might return unmodified info
		if (pass->numtcmods)
		{
			tcmod(&pass->tcmods[0], mesh->numvertexes, src, dest, mesh);
			for (i = 1; i < pass->numtcmods; i++)
			{
				tcmod(&pass->tcmods[i], mesh->numvertexes, dest, dest, mesh);
			}
		}
		else if (src != dest)
		{
			memcpy(dest, src, 8*mesh->numvertexes);
		}
		dest += mesh->numvertexes*2;
	}
}

//end texture coords
/*******************************************************************************************************************/

/*does not do the draw call, does not consider indicies (except for billboard generation) */
static void BE_DrawMeshChain_SetupPass(shaderpass_t *pass, unsigned int vertcount)
{
	int vdec;
	void *map;
	int i;
	unsigned int passno = 0;

	int lastpass = pass->numMergedPasses;

	for (i = 0; i < lastpass; i++)
	{
		if (pass[i].texgen == T_GEN_UPPEROVERLAY && !TEXVALID(shaderstate.curtexnums->upperoverlay))
			continue;
		if (pass[i].texgen == T_GEN_LOWEROVERLAY && !TEXVALID(shaderstate.curtexnums->loweroverlay))
			continue;
		if (pass[i].texgen == T_GEN_FULLBRIGHT && !TEXVALID(shaderstate.curtexnums->fullbright))
			continue;
		break;
	}
	if (i == lastpass)
		return;

	passno = 0;

	/*all meshes in a chain must have the same features*/

	vdec = 0;

	/*we only use one colour, generated from the first pass*/
	vdec |= BE_GenerateColourMods(vertcount, pass);

	for (; passno < lastpass; passno++)
	{
		SelectPassTexture(passno, pass+passno);

		vdec |= D3D_VDEC_ST0<<passno;
		allocvertexbuffer(shaderstate.dynst_buff[passno], shaderstate.dynst_size, &shaderstate.dynst_offs[passno], &map, vertcount*sizeof(vec2_t));
		GenerateTCMods(pass+passno, map);
		d3dcheck(IDirect3DVertexBuffer9_Unlock(shaderstate.dynst_buff[passno]));
		d3dcheck(IDirect3DDevice9_SetStreamSource(pD3DDev9, 5+passno, shaderstate.dynst_buff[passno], shaderstate.dynst_offs[passno] - vertcount*sizeof(vec2_t), sizeof(vec2_t)));
	}
	for (; passno < shaderstate.lastpasscount; passno++)
	{
		d3dcheck(IDirect3DDevice9_SetStreamSource(pD3DDev9, 5+passno, NULL, 0, 0));
		d3dcheck(IDirect3DDevice9_SetTextureStageState(pD3DDev9, passno, D3DTSS_COLOROP, D3DTOP_DISABLE));
	}
	shaderstate.lastpasscount = lastpass;

//	if (meshchain->normals_array && 
//		meshchain->2 && 
//		meshchain->tnormals_array)
//		vdec |= D3D_VDEC_NORMS;

	if (vdec != shaderstate.curvertdecl)
	{
		shaderstate.curvertdecl = vdec;
		d3dcheck(IDirect3DDevice9_SetVertexDeclaration(pD3DDev9, vertexdecls[shaderstate.curvertdecl]));
	}

	D3DBE_ApplyShaderBits(pass->shaderbits);
}

static void BE_DrawMeshChain_Internal(void)
{
	unsigned int vertcount, idxcount, idxfirst;
	mesh_t *m;
	void *map;
	int i;
	unsigned int mno;
	unsigned int passno = 0;
	shaderpass_t *pass = shaderstate.curshader->passes;

	for (mno = 0, vertcount = 0, idxcount = 0; mno < shaderstate.nummeshes; mno++)
	{
		m = shaderstate.meshlist[mno];
		vertcount += m->numvertexes;
		idxcount += m->numindexes;
	}

	/*vertex buffers are common to all passes*/
	allocvertexbuffer(shaderstate.dynxyz_buff, shaderstate.dynxyz_size, &shaderstate.dynxyz_offs, &map, vertcount*sizeof(vecV_t));
	for (mno = 0, vertcount = 0; mno < shaderstate.nummeshes; mno++)
	{
		m = shaderstate.meshlist[mno];
		/*fixme: no tcgen*/
		memcpy((char*)map+vertcount*sizeof(vecV_t), m->xyz_array, m->numvertexes*sizeof(vecV_t));
		vertcount += m->numvertexes;
	}
	d3dcheck(IDirect3DVertexBuffer9_Unlock(shaderstate.dynxyz_buff));
	d3dcheck(IDirect3DDevice9_SetStreamSource(pD3DDev9, 0, shaderstate.dynxyz_buff, shaderstate.dynxyz_offs - vertcount*sizeof(vecV_t), sizeof(vecV_t)));

	/*so are index buffers*/
	idxfirst = allocindexbuffer(&map, idxcount);
	for (mno = 0, vertcount = 0; mno < shaderstate.nummeshes; mno++)
	{
		m = shaderstate.meshlist[mno];
		for (i = 0; i < m->numindexes; i++)
			((index_t*)map)[i] = m->indexes[i]+vertcount;
		map = (char*)map + m->numindexes*sizeof(index_t);
		vertcount += m->numvertexes;
	}
	d3dcheck(IDirect3DIndexBuffer9_Unlock(shaderstate.dynidx_buff));
	d3dcheck(IDirect3DDevice9_SetIndices(pD3DDev9, shaderstate.dynidx_buff));
	
	/*now go through and flush each pass*/
	for (passno = 0; passno < shaderstate.curshader->numpasses; passno += pass->numMergedPasses)
	{
		BE_DrawMeshChain_SetupPass(pass+passno, vertcount);
		d3dcheck(IDirect3DDevice9_DrawIndexedPrimitive(pD3DDev9, D3DPT_TRIANGLELIST, 0, 0, vertcount, idxfirst, idxcount/3));
	}
}

void BE_SelectMode(backendmode_t mode, unsigned int flags)
{
	shaderstate.mode = mode;
	shaderstate.flags = flags;
}

void _CrtCheckMemory(void);

/*Generates an optimised vbo for each of the given model's textures*/
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

//		if (GL_BuildVBO(vbo, vbovdata, vcount*pervertsize, vboedata, ecount*sizeof(index_t)))
		{
			BZ_Free(vbovdata);
			BZ_Free(vboedata);
		}
	}
	//for (i=0 ; i<mod->numsurfaces ; i++)
	//{
	//	if (!mod->surfaces[i].mark)
	//		Host_EndGame("Surfaces with bad textures detected\n");
	//}
}
/*Wipes a vbo*/
void BE_ClearVBO(vbo_t *vbo)
{
}

/*upload all lightmaps at the start to reduce lags*/
void BE_UploadLightmaps(qboolean force)
{
	int i;

	for (i = 0; i < numlightmaps; i++)
	{
		if (!lightmap[i])
			continue;

		if (force)
		{
			lightmap[i]->rectchange.l = 0;
			lightmap[i]->rectchange.t = 0;
			lightmap[i]->rectchange.w = LMBLOCK_WIDTH;
			lightmap[i]->rectchange.h = LMBLOCK_HEIGHT;
		}

		if (lightmap[i]->modified)
		{
			IDirect3DTexture9 *tex = lightmap_textures[i].ptr;
			D3DLOCKED_RECT lock;
			RECT rect;
			glRect_t *theRect = &lightmap[i]->rectchange;
			int r;
			if (tex)
			{
				lightmap[i]->modified = 0;
				rect.left = theRect->l;
				rect.right = theRect->l + theRect->w;
				rect.top = theRect->t;
				rect.bottom = theRect->t + theRect->h;

				IDirect3DTexture9_LockRect(tex, 0, &lock, &rect, 0);
				for (r = 0; r < lightmap[i]->rectchange.h; r++)
				{
					memcpy((char*)lock.pBits + r*lock.Pitch, lightmap[i]->lightmaps+(theRect->l+((r+theRect->t)*LMBLOCK_WIDTH))*lightmap_bytes, lightmap[i]->rectchange.w*lightmap_bytes);
				}
				IDirect3DTexture9_UnlockRect(tex, 0);
				theRect->l = LMBLOCK_WIDTH;
				theRect->t = LMBLOCK_HEIGHT;
				theRect->h = 0;
				theRect->w = 0;
			}
			else
				lightmap_textures[i] = R_AllocNewTexture(LMBLOCK_WIDTH, LMBLOCK_HEIGHT);
		}
	}
}

void BE_UploadAllLightmaps(void)
{
	BE_UploadLightmaps(true);
}

qboolean BE_LightCullModel(vec3_t org, model_t *model)
{
#ifdef RTLIGHTS
	if ((shaderstate.mode == BEM_LIGHT || shaderstate.mode == BEM_STENCIL))
	{
		/*true if hidden from current light*/
		/*we have no rtlight support, so mneh*/
	}
#endif
	return false;
}

batch_t *BE_GetTempBatch(void)
{
	/*FIXME: utterly evil*/
	return malloc(sizeof(batch_t));
}

static void BE_RotateForEntity (const entity_t *e, const model_t *mod)
{
	float mv[16];
	float m[16];

	m[0] = e->axis[0][0];
	m[1] = e->axis[0][1];
	m[2] = e->axis[0][2];
	m[3] = 0;

	m[4] = e->axis[1][0];
	m[5] = e->axis[1][1];
	m[6] = e->axis[1][2];
	m[7] = 0;

	m[8] = e->axis[2][0];
	m[9] = e->axis[2][1];
	m[10] = e->axis[2][2];
	m[11] = 0;

	m[12] = e->origin[0];
	m[13] = e->origin[1];
	m[14] = e->origin[2];
	m[15] = 1;

	if (e->scale != 1 && e->scale != 0)	//hexen 2 stuff
	{
		float z;
		float escale;
		escale = e->scale;
		switch(e->drawflags&SCALE_TYPE_MASKIN)
		{
		default:
		case SCALE_TYPE_UNIFORM:
			VectorScale((m+0), escale, (m+0));
			VectorScale((m+4), escale, (m+4));
			VectorScale((m+8), escale, (m+8));
			break;
		case SCALE_TYPE_XYONLY:
			VectorScale((m+0), escale, (m+0));
			VectorScale((m+4), escale, (m+4));
			break;
		case SCALE_TYPE_ZONLY:
			VectorScale((m+8), escale, (m+8));
			break;
		}
		if (mod && (e->drawflags&SCALE_TYPE_MASKIN) != SCALE_TYPE_XYONLY)
		{
			switch(e->drawflags&SCALE_ORIGIN_MASKIN)
			{
			case SCALE_ORIGIN_CENTER:
				z = ((mod->maxs[2] + mod->mins[2]) * (1-escale))/2;
				VectorMA((m+12), z, e->axis[2], (m+12));
				break;
			case SCALE_ORIGIN_BOTTOM:
				VectorMA((m+12), mod->mins[2]*(1-escale), e->axis[2], (m+12));
				break;
			case SCALE_ORIGIN_TOP:
				VectorMA((m+12), -mod->maxs[2], e->axis[2], (m+12));
				break;
			}
		}
	}
	else if (mod && !strcmp(mod->name, "progs/eyes.mdl"))
	{
		/*resize eyes, to make them easier to see*/
		m[14] -= (22 + 8);
		VectorScale((m+0), 2, (m+0));
		VectorScale((m+4), 2, (m+4));
		VectorScale((m+8), 2, (m+8));
	}
	if (mod && !ruleset_allow_larger_models.ival && mod->clampscale != 1)
	{	//possibly this should be on a per-frame basis, but that's a real pain to do
		Con_DPrintf("Rescaling %s by %f\n", mod->name, mod->clampscale);
		VectorScale((m+0), mod->clampscale, (m+0));
		VectorScale((m+4), mod->clampscale, (m+4));
		VectorScale((m+8), mod->clampscale, (m+8));
	}

	if (e->flags & Q2RF_WEAPONMODEL && r_refdef.currentplayernum>=0)
	{
		float *Matrix4_NewRotation(float a, float x, float y, float z);
		/*FIXME: no bob*/
		float iv[16];
		Matrix4_Invert(r_refdef.m_view, iv);
		Matrix4_NewRotation(90, 1, 0, 0);
		Matrix4_Multiply(iv, m, mv);
		Matrix4_Multiply(mv, Matrix4_NewRotation(-90, 1, 0, 0), iv);
		Matrix4_Multiply(iv, Matrix4_NewRotation(90, 0, 0, 1), mv);

	m[2] *= 0.1;
	m[6] *= 0.1;
	m[10] *= 0.1;
		IDirect3DDevice9_SetTransform(pD3DDev9, D3DTS_WORLD, (D3DMATRIX*)mv);
	}
	else
	{
		IDirect3DDevice9_SetTransform(pD3DDev9, D3DTS_WORLD, (D3DMATRIX*)m);
	}
}

static void BE_SubmitBatch(batch_t *batch)
{
	shaderstate.nummeshes = batch->meshes - batch->firstmesh;
	if (!shaderstate.nummeshes)
		return;
	if (shaderstate.curentity != batch->ent)
	{
		shaderstate.curentity = batch->ent;
		BE_RotateForEntity(batch->ent, batch->ent->model);
	}
	shaderstate.meshlist = batch->mesh + batch->firstmesh;
	shaderstate.curshader = batch->shader;
	shaderstate.curtexnums = batch->skin;
	if (batch->lightmap < 0)
		shaderstate.curlightmap = r_nulltex;
	else
		shaderstate.curlightmap = lightmap_textures[batch->lightmap];

	BE_DrawMeshChain_Internal();
}

void BE_DrawMesh_List(shader_t *shader, int nummeshes, mesh_t **meshlist, vbo_t *vbo, texnums_t *texnums)
{
	shaderstate.curshader = shader;
	shaderstate.curtexnums = texnums;
	shaderstate.curlightmap = r_nulltex;
	shaderstate.meshlist = meshlist;
	shaderstate.nummeshes = nummeshes;

	BE_DrawMeshChain_Internal();
}

void BE_DrawMesh_Single(shader_t *shader, mesh_t *meshchain, vbo_t *vbo, texnums_t *texnums)
{
	shaderstate.curshader = shader;
	shaderstate.curtexnums = texnums?texnums:&shader->defaulttextures;
	shaderstate.curlightmap = r_nulltex;
	shaderstate.meshlist = &meshchain;
	shaderstate.nummeshes = 1;

	BE_DrawMeshChain_Internal();
}

qboolean BE_ShouldDraw(entity_t *e)
{
	if (!r_refdef.externalview && (e->externalmodelview & (1<<r_refdef.currentplayernum)))
		return false;
	if (!Cam_DrawPlayer(r_refdef.currentplayernum, e->keynum-1))
		return false;
	return true;
}

static void BE_GenModelBatches(batch_t **batches)
{
	int		i;
	entity_t *ent;

	/*clear the batch list*/
	for (i = 0; i < SHADER_SORT_COUNT; i++)
		batches[i] = NULL;

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
		if (!BE_ShouldDraw(ent))
			continue;
		switch(ent->model->type)
		{
		case mod_brush:
			if (r_drawentities.ival == 2)
				continue;
			Surf_GenBrushBatches(batches, ent);
			break;
		case mod_alias:
			if (r_drawentities.ival == 3)
				continue;
			R_GAlias_GenerateBatches(ent, batches);
			break;
		}
	}
}

static void BE_SubmitMeshesSortList(batch_t *sortlist)
{
	batch_t *batch;
	for (batch = sortlist; batch; batch = batch->next)
	{
		if (batch->meshes == batch->firstmesh)
			continue;

		if (batch->buildmeshes)
			batch->buildmeshes(batch);
		else
		{
			batch->shader = R_TextureAnimation(batch->ent->framestate.g[FS_REG].frame[0], batch->texture)->shader;
			batch->skin = &batch->shader->defaulttextures;
		}

		if (batch->shader->flags & SHADER_NODLIGHT)
			if (shaderstate.mode == BEM_LIGHT || shaderstate.mode == BEM_SMAPLIGHT)
				continue;

		if (batch->shader->flags & SHADER_SKY)
		{
//			if (shaderstate.mode == BEM_STANDARD)
//				R_DrawSkyChain (batch);
			continue;
		}

		BE_SubmitBatch(batch);
	}
}

void BE_SubmitMeshes (qboolean drawworld, batch_t **blist)
{
	model_t *model = cl.worldmodel;
	int i;

	for (i = SHADER_SORT_PORTAL; i < SHADER_SORT_COUNT; i++)
	{
		if (drawworld)
		{
//			if (i == SHADER_SORT_PORTAL && !r_noportals.ival && !r_refdef.recurse)
//				BE_SubmitMeshesPortals(model->batches, blist[i]);

			BE_SubmitMeshesSortList(model->batches[i]);
		}
		BE_SubmitMeshesSortList(blist[i]);
	}
}

void BE_DrawWorld (qbyte *vis)
{
	batch_t *batches[SHADER_SORT_COUNT];
	RSpeedLocals();

	BE_GenModelBatches(batches);

	shaderstate.curtime = realtime;

	BE_UploadLightmaps(false);

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
	BE_SubmitMeshes(true, batches);
	RSpeedEnd(RSPEED_WORLD);

	BE_RotateForEntity(&r_worldentity, NULL);
}


#endif
