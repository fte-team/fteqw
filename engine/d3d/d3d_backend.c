#include "quakedef.h"
#ifdef D3DQUAKE
#include "shader.h"
#if !defined(HMONITOR_DECLARED) && (WINVER < 0x0500)
    #define HMONITOR_DECLARED
    DECLARE_HANDLE(HMONITOR);
#endif
#include <d3d9.h>

extern LPDIRECT3DDEVICE9 pD3DDev9;

//#define d3dcheck(foo) foo
#define d3dcheck(foo) do{HRESULT err = foo; if (FAILED(err)) Sys_Error("D3D reported error on backend line %i - error 0x%x\n", __LINE__, err);} while(0)

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

typedef vec3_t mat3_t[3];
static mat3_t axisDefault={{1, 0, 0},
					{0, 1, 0},
					{0, 0, 1}};

static void Matrix3_Transpose (mat3_t in, mat3_t out)
{
	out[0][0] = in[0][0];
	out[1][1] = in[1][1];
	out[2][2] = in[2][2];

	out[0][1] = in[1][0];
	out[0][2] = in[2][0];
	out[1][0] = in[0][1];
	out[1][2] = in[2][1];
	out[2][0] = in[0][2];
	out[2][1] = in[1][2];
}
static void Matrix3_Multiply_Vec3 (const mat3_t a, const vec3_t b, vec3_t product)
{
	product[0] = a[0][0]*b[0] + a[0][1]*b[1] + a[0][2]*b[2];
	product[1] = a[1][0]*b[0] + a[1][1]*b[1] + a[1][2]*b[2];
	product[2] = a[2][0]*b[0] + a[2][1]*b[1] + a[2][2]*b[2];
}

static int Matrix3_Compare(const mat3_t in, const mat3_t out)
{
	return !memcmp(in, out, sizeof(mat3_t));
}

/*================================================*/

typedef struct
{
	backendmode_t mode;
	unsigned int flags;

	float		curtime;
	const entity_t    *curentity;
	shader_t	*curshader;
	texnums_t	*curtexnums;
	texid_t		curlightmap;
	texid_t		curdeluxmap;
	int			curvertdecl;
	unsigned int shaderbits;
	unsigned int curcull;
	unsigned int lastpasscount;

	texid_t		curtex[MAX_TMUS];
	unsigned int tmuflags[MAX_TMUS];

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

	unsigned int wbatch;
	unsigned int maxwbatches;
	batch_t *wbatches;
} d3dbackend_t;

#define DYNVBUFFSIZE 65536
#define DYNIBUFFSIZE 65536

static d3dbackend_t shaderstate;

extern int be_maxpasses;

enum 
{
	D3D_VDEC_COL4B = 1<<0,
	//D3D_VDEC_NORMS = 1<<1,
	D3D_VDEC_ST0 = 1<<1,
	D3D_VDEC_ST1 = 1<<2,
	D3D_VDEC_ST2 = 1<<3,
	D3D_VDEC_ST3 = 1<<4,
	D3D_VDEC_MAX = 1<<5
};
IDirect3DVertexDeclaration9 *vertexdecls[D3D_VDEC_MAX];


void D3DBE_Reset(qboolean before)
{
	int i, tmu;
	if (before)
	{
		IDirect3DDevice9_SetVertexDeclaration(pD3DDev9, NULL);
		shaderstate.curvertdecl = 0;
		for (i = 0; i < 5+MAX_TMUS; i++)
			IDirect3DDevice9_SetStreamSource(pD3DDev9, i, NULL, 0, 0);
		IDirect3DDevice9_SetIndices(pD3DDev9, NULL);

		if (shaderstate.dynxyz_buff)
			IDirect3DVertexBuffer9_Release(shaderstate.dynxyz_buff);
		shaderstate.dynxyz_buff = NULL;
		for (tmu = 0; tmu < MAX_TMUS; tmu++)
		{
			if (shaderstate.dynst_buff[tmu])
				IDirect3DVertexBuffer9_Release(shaderstate.dynst_buff[tmu]);
			shaderstate.dynst_buff[tmu] = NULL;
		}
		if (shaderstate.dyncol_buff)
			IDirect3DVertexBuffer9_Release(shaderstate.dyncol_buff);
		shaderstate.dyncol_buff = NULL;
		if (shaderstate.dynidx_buff)
			IDirect3DIndexBuffer9_Release(shaderstate.dynidx_buff);
		shaderstate.dynidx_buff = NULL;

		for (i = 0; i < D3D_VDEC_MAX; i++)
		{
			if (vertexdecls[i])
				IDirect3DVertexDeclaration9_Release(vertexdecls[i]);
			vertexdecls[i] = NULL;
		}
	}
	else
	{
		D3DVERTEXELEMENT9 decl[8], declend=D3DDECL_END();
		int elements;

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

/*			if (i & D3D_VDEC_NORMS)
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
*/
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

		IDirect3DDevice9_CreateVertexBuffer(pD3DDev9, shaderstate.dynxyz_size, D3DUSAGE_DYNAMIC|D3DUSAGE_WRITEONLY, 0, D3DPOOL_DEFAULT, &shaderstate.dynxyz_buff, NULL);
		for (tmu = 0; tmu < MAX_TMUS; tmu++)
			IDirect3DDevice9_CreateVertexBuffer(pD3DDev9, shaderstate.dynst_size, D3DUSAGE_DYNAMIC|D3DUSAGE_WRITEONLY, 0, D3DPOOL_DEFAULT, &shaderstate.dynst_buff[tmu], NULL);
		IDirect3DDevice9_CreateVertexBuffer(pD3DDev9, shaderstate.dyncol_size, D3DUSAGE_DYNAMIC|D3DUSAGE_WRITEONLY, 0, D3DPOOL_DEFAULT, &shaderstate.dyncol_buff, NULL);
		IDirect3DDevice9_CreateIndexBuffer(pD3DDev9, shaderstate.dynidx_size, D3DUSAGE_DYNAMIC|D3DUSAGE_WRITEONLY, D3DFMT_QINDEX, D3DPOOL_DEFAULT, &shaderstate.dynidx_buff, NULL);
	}
}

static void BE_ApplyTMUState(unsigned int tu, unsigned int flags)
{
	if ((flags ^ shaderstate.tmuflags[tu]) & SHADER_PASS_CLAMP)
	{
		shaderstate.tmuflags[tu] ^= SHADER_PASS_CLAMP;
		if (flags & SHADER_PASS_CLAMP)
		{
			IDirect3DDevice9_SetSamplerState(pD3DDev9, tu, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
			IDirect3DDevice9_SetSamplerState(pD3DDev9, tu, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);
			IDirect3DDevice9_SetSamplerState(pD3DDev9, tu, D3DSAMP_ADDRESSW, D3DTADDRESS_CLAMP);
		}
		else
		{
			IDirect3DDevice9_SetSamplerState(pD3DDev9, tu, D3DSAMP_ADDRESSU, D3DTADDRESS_WRAP);
			IDirect3DDevice9_SetSamplerState(pD3DDev9, tu, D3DSAMP_ADDRESSV, D3DTADDRESS_WRAP);
			IDirect3DDevice9_SetSamplerState(pD3DDev9, tu, D3DSAMP_ADDRESSW, D3DTADDRESS_WRAP);
		}
	}
	if ((flags ^ shaderstate.tmuflags[tu]) & SHADER_PASS_NOMIPMAP)
	{
		shaderstate.tmuflags[tu] ^= SHADER_PASS_NOMIPMAP;
		/*lightmaps don't use mipmaps*/
		if (flags & SHADER_PASS_NOMIPMAP)
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
	}
}

static void D3DBE_ApplyShaderBits(unsigned int bits)
{
	unsigned int delta;

	if (shaderstate.flags & ~BEF_PUSHDEPTH)
	{
		if (shaderstate.flags & BEF_FORCEADDITIVE)
			bits = (bits & ~(SBITS_MISC_DEPTHWRITE|SBITS_BLEND_BITS|SBITS_ATEST_BITS))
						| (SBITS_SRCBLEND_ONE | SBITS_DSTBLEND_ONE);
		else if ((shaderstate.flags & BEF_FORCETRANSPARENT) && !(bits & SBITS_BLEND_BITS)) 	/*if transparency is forced, clear alpha test bits*/
			bits = (bits & ~(SBITS_MISC_DEPTHWRITE|SBITS_BLEND_BITS|SBITS_ATEST_BITS))
						| (SBITS_SRCBLEND_SRC_ALPHA | SBITS_DSTBLEND_ONE_MINUS_SRC_ALPHA);

		if (shaderstate.flags & BEF_FORCENODEPTH) 	/*EF_NODEPTHTEST dp extension*/
			bits |= SBITS_MISC_NODEPTHTEST;
		else
		{
			if (shaderstate.flags & BEF_FORCEDEPTHTEST)
				bits &= ~SBITS_MISC_NODEPTHTEST;
			if (shaderstate.flags & BEF_FORCEDEPTHWRITE)
				bits |= SBITS_MISC_DEPTHWRITE;
		}
	}

	delta = bits ^ shaderstate.shaderbits;
	if (!delta)
		return;
	shaderstate.shaderbits = bits;

	if (delta & SBITS_BLEND_BITS)
	{
		if (bits & SBITS_BLEND_BITS)
		{
			D3DBLEND src;
			D3DBLEND dst;

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

void D3DBE_Init(void)
{
	unsigned int i;
	be_maxpasses = MAX_TMUS;
	memset(&shaderstate, 0, sizeof(shaderstate));
	shaderstate.curvertdecl = -1;

	FTable_Init();

	shaderstate.dynxyz_size = sizeof(vecV_t) * DYNVBUFFSIZE;
	shaderstate.dyncol_size = sizeof(byte_vec4_t) * DYNVBUFFSIZE;
	shaderstate.dynst_size = sizeof(vec2_t) * DYNVBUFFSIZE;
	shaderstate.dynidx_size = sizeof(index_t) * DYNIBUFFSIZE;

	D3DBE_Reset(false);

	/*force all state to change, thus setting a known state*/
	shaderstate.shaderbits = ~0;
	D3DBE_ApplyShaderBits(0);

	for (i = 0; i < MAX_TMUS; i++)
	{
		shaderstate.tmuflags[i] = ~0;
		BE_ApplyTMUState(i, 0);
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

static void BindTexture(unsigned int tu, void *id)
{
	if (shaderstate.curtex[tu].ptr != id)
	{
		shaderstate.curtex[tu].ptr = id;
		IDirect3DDevice9_SetTexture (pD3DDev9, tu, id);
	}
}

static void SelectPassTexture(unsigned int tu, shaderpass_t *pass)
{
	switch(pass->texgen)
	{
	default:
	case T_GEN_DIFFUSE:
		BindTexture(tu, shaderstate.curtexnums->base.ptr);
		break;
	case T_GEN_NORMALMAP:
		BindTexture( tu, shaderstate.curtexnums->bump.ptr);
		break;
	case T_GEN_SPECULAR:
		BindTexture(tu, shaderstate.curtexnums->specular.ptr);
		break;
	case T_GEN_UPPEROVERLAY:
		BindTexture(tu, shaderstate.curtexnums->upperoverlay.ptr);
		break;
	case T_GEN_LOWEROVERLAY:
		BindTexture(tu, shaderstate.curtexnums->loweroverlay.ptr);
		break;
	case T_GEN_FULLBRIGHT:
		BindTexture(tu, shaderstate.curtexnums->fullbright.ptr);
		break;
	case T_GEN_ANIMMAP:
		BindTexture(tu, pass->anim_frames[(int)(pass->anim_fps * shaderstate.curtime) % pass->anim_numframes].ptr);
		break;
	case T_GEN_SINGLEMAP:
		BindTexture(tu, pass->anim_frames[0].ptr);
		break;
	case T_GEN_DELUXMAP:
		BindTexture(tu, shaderstate.curdeluxmap.ptr);
		break;
	case T_GEN_LIGHTMAP:
		BindTexture(tu, shaderstate.curlightmap.ptr);
		break;

	/*case T_GEN_CURRENTRENDER:
		FIXME: no code to grab the current screen and convert to a texture
		break;*/
	case T_GEN_VIDEOMAP:
		BindTexture(tu, Media_UpdateForShader(pass->cin).ptr);
		break;
	}

	BE_ApplyTMUState(tu, pass->flags);

	switch (pass->blendmode)
	{
	case PBM_DOTPRODUCT:
		IDirect3DDevice9_SetTextureStageState(pD3DDev9, tu, D3DTSS_COLORARG1, D3DTA_TEXTURE);
		IDirect3DDevice9_SetTextureStageState(pD3DDev9, tu, D3DTSS_COLORARG2, D3DTA_CURRENT);
		IDirect3DDevice9_SetTextureStageState(pD3DDev9, tu, D3DTSS_COLOROP, D3DTOP_DOTPRODUCT3);

		IDirect3DDevice9_SetTextureStageState(pD3DDev9, tu, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
//		IDirect3DDevice9_SetTextureStageState(pD3DDev9, tu, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);
		IDirect3DDevice9_SetTextureStageState(pD3DDev9, tu, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);
		break;
	case PBM_REPLACE:
		IDirect3DDevice9_SetTextureStageState(pD3DDev9, tu, D3DTSS_COLORARG1, D3DTA_TEXTURE);
		IDirect3DDevice9_SetTextureStageState(pD3DDev9, tu, D3DTSS_COLOROP, D3DTOP_SELECTARG1);

		if (shaderstate.flags & (BEF_FORCETRANSPARENT | BEF_FORCEADDITIVE))
		{
			IDirect3DDevice9_SetTextureStageState(pD3DDev9, tu, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
			IDirect3DDevice9_SetTextureStageState(pD3DDev9, tu, D3DTSS_ALPHAARG2, D3DTA_CURRENT);
			IDirect3DDevice9_SetTextureStageState(pD3DDev9, tu, D3DTSS_ALPHAOP, D3DTOP_MODULATE);
		}
		else
		{
			IDirect3DDevice9_SetTextureStageState(pD3DDev9, tu, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
	//		IDirect3DDevice9_SetTextureStageState(pD3DDev9, tu, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);
			IDirect3DDevice9_SetTextureStageState(pD3DDev9, tu, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);
		}
		break;
	case PBM_ADD:
		IDirect3DDevice9_SetTextureStageState(pD3DDev9, tu, D3DTSS_COLORARG1, D3DTA_TEXTURE);
		IDirect3DDevice9_SetTextureStageState(pD3DDev9, tu, D3DTSS_COLORARG2, D3DTA_CURRENT);
		IDirect3DDevice9_SetTextureStageState(pD3DDev9, tu, D3DTSS_COLOROP, D3DTOP_BLENDTEXTUREALPHA);
		shaderstate.passcolour &= 0xff000000;

		IDirect3DDevice9_SetTextureStageState(pD3DDev9, tu, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
		IDirect3DDevice9_SetTextureStageState(pD3DDev9, tu, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);
		IDirect3DDevice9_SetTextureStageState(pD3DDev9, tu, D3DTSS_ALPHAOP, D3DTOP_MODULATE);
		break;
	case PBM_DECAL:
		if (!tu)
			goto forcemod;
		IDirect3DDevice9_SetTextureStageState(pD3DDev9, tu, D3DTSS_COLORARG1, D3DTA_TEXTURE);
		IDirect3DDevice9_SetTextureStageState(pD3DDev9, tu, D3DTSS_COLORARG2, D3DTA_CURRENT);
		IDirect3DDevice9_SetTextureStageState(pD3DDev9, tu, D3DTSS_COLOROP, D3DTOP_BLENDTEXTUREALPHA);

		IDirect3DDevice9_SetTextureStageState(pD3DDev9, tu, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
//		IDirect3DDevice9_SetTextureStageState(pD3DDev9, tu, D3DTSS_ALPHAARG2, D3DTA_CURRENT);
		IDirect3DDevice9_SetTextureStageState(pD3DDev9, tu, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);
		break;
	case PBM_OVERBRIGHT:
		IDirect3DDevice9_SetTextureStageState(pD3DDev9, tu, D3DTSS_COLORARG1, D3DTA_TEXTURE);
		IDirect3DDevice9_SetTextureStageState(pD3DDev9, tu, D3DTSS_COLORARG2, D3DTA_CURRENT);
		{
			extern cvar_t gl_overbright;
			switch (gl_overbright.ival)
			{
			case 1:
				IDirect3DDevice9_SetTextureStageState(pD3DDev9, tu, D3DTSS_COLOROP, D3DTOP_MODULATE2X);
				break;
			case 2:
			case 3:
				IDirect3DDevice9_SetTextureStageState(pD3DDev9, tu, D3DTSS_COLOROP, D3DTOP_MODULATE4X);
				break;
			default:
				IDirect3DDevice9_SetTextureStageState(pD3DDev9, tu, D3DTSS_COLOROP, D3DTOP_MODULATE);
				break;
			}
		}
		IDirect3DDevice9_SetTextureStageState(pD3DDev9, tu, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
		IDirect3DDevice9_SetTextureStageState(pD3DDev9, tu, D3DTSS_ALPHAARG2, D3DTA_CURRENT);
		IDirect3DDevice9_SetTextureStageState(pD3DDev9, tu, D3DTSS_ALPHAOP, D3DTOP_MODULATE);
		break;
	default:
	case PBM_MODULATE:
	forcemod:
		IDirect3DDevice9_SetTextureStageState(pD3DDev9, tu, D3DTSS_COLORARG1, D3DTA_TEXTURE);
		IDirect3DDevice9_SetTextureStageState(pD3DDev9, tu, D3DTSS_COLORARG2, D3DTA_CURRENT);
		IDirect3DDevice9_SetTextureStageState(pD3DDev9, tu, D3DTSS_COLOROP, D3DTOP_MODULATE);

		IDirect3DDevice9_SetTextureStageState(pD3DDev9, tu, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
		IDirect3DDevice9_SetTextureStageState(pD3DDev9, tu, D3DTSS_ALPHAARG2, D3DTA_CURRENT);
		IDirect3DDevice9_SetTextureStageState(pD3DDev9, tu, D3DTSS_ALPHAOP, D3DTOP_MODULATE);
		break;
	}

	if (tu == 0)
	{
		if (shaderstate.passsinglecolour)
		{
			IDirect3DDevice9_SetTextureStageState(pD3DDev9, tu, D3DTSS_COLORARG2, D3DTA_CONSTANT);
			IDirect3DDevice9_SetTextureStageState(pD3DDev9, tu, D3DTSS_ALPHAARG2, D3DTA_CONSTANT);
			IDirect3DDevice9_SetTextureStageState(pD3DDev9, tu, D3DTSS_CONSTANT, shaderstate.passcolour);
		}
		else
		{
			IDirect3DDevice9_SetTextureStageState(pD3DDev9, tu, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
			IDirect3DDevice9_SetTextureStageState(pD3DDev9, tu, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);
		}
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
		if (!src)
		{
			while((cnt)--)
			{
				dst[cnt][0] = 255;//shaderstate.identitylighting;
				dst[cnt][1] = 255;//shaderstate.identitylighting;
				dst[cnt][2] = 255;//shaderstate.identitylighting;
			}
		}
		else
		{
			while((cnt)--)
			{
				qbyte r, g, b;
				r=src[cnt][0];
				g=src[cnt][1];
				b=src[cnt][2];
				dst[cnt][0] = b;
				dst[cnt][1] = g;
				dst[cnt][2] = r;
			}
		}
		break;
	case RGB_GEN_ONE_MINUS_VERTEX:
		while((cnt)--)
		{
			qbyte r, g, b;
			r=255-src[cnt][0];
			g=255-src[cnt][1];
			b=255-src[cnt][2];
			dst[cnt][0] = b;
			dst[cnt][1] = g;
			dst[cnt][2] = r;
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
			R_LightArraysByte_BGR(mesh->xyz_array, dst, cnt, mesh->normals_array);
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
		if (shaderstate.flags & BEF_FORCETRANSPARENT)
		{
			f = shaderstate.curentity->shaderRGBAf[3];
			if (f < 0)
				t = 0;
			else if (f >= 1)
				t = 255;
			else
				t = f*255;
			while(cnt--)
				dst[cnt][3] = t;
		}
		else
		{
			while(cnt--)
				dst[cnt][3] = 255;
		}
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

	case ALPHA_GEN_SPECULAR:
		{
			int i;
			VectorSubtract(r_origin, shaderstate.curentity->origin, v1);

			if (!Matrix3_Compare(shaderstate.curentity->axis, axisDefault))
			{
				Matrix3_Multiply_Vec3(shaderstate.curentity->axis, v2, v2);
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
				dst[i][3] = bound (0.0f, (int)(f*255), 255);
			}
		}
		break;
	}
}

static unsigned int BE_GenerateColourMods(unsigned int vertcount, const shaderpass_t *pass)
{
	unsigned int ret = 0;
	unsigned char *map;
	const mesh_t *m;
	unsigned int mno;

	m = shaderstate.meshlist[0];

	if (pass->flags & SHADER_PASS_NOCOLORARRAY)
	{
		shaderstate.passsinglecolour = true;
		shaderstate.passcolour = D3DCOLOR_RGBA(255,255,255,255);
		colourgenbyte(pass, 1, (byte_vec4_t*)&shaderstate.passcolour, (byte_vec4_t*)&shaderstate.passcolour, m);
		alphagenbyte(pass, 1, (byte_vec4_t*)&shaderstate.passcolour, (byte_vec4_t*)&shaderstate.passcolour, m);
		/*FIXME: just because there's no rgba set, there's no reason to assume it should be a single colour (unshaded ents)*/
		d3dcheck(IDirect3DDevice9_SetStreamSource(pD3DDev9, 1, NULL, 0, 0));
	}
	else
	{
		unsigned int v;
		int c;
		float *src;

		shaderstate.passsinglecolour = false;

		ret |= D3D_VDEC_COL4B;
		allocvertexbuffer(shaderstate.dyncol_buff, shaderstate.dyncol_size, &shaderstate.dyncol_offs, (void**)&map, vertcount*sizeof(D3DCOLOR));
		if (m->colors4b_array)
		{
			for (vertcount = 0, mno = 0; mno < shaderstate.nummeshes; mno++)
			{
				m = shaderstate.meshlist[mno];
				colourgenbyte(pass, m->numvertexes, (byte_vec4_t*)m->colors4b_array, (byte_vec4_t*)map, m);
				alphagenbyte(pass, m->numvertexes, (byte_vec4_t*)m->colors4b_array, (byte_vec4_t*)map, m);
				map += m->numvertexes*4;
				vertcount += m->numvertexes;
			}
		}
		else if (m->colors4f_array &&
						((pass->rgbgen == RGB_GEN_VERTEX) ||
						(pass->rgbgen == RGB_GEN_EXACT_VERTEX) ||
						(pass->rgbgen == RGB_GEN_ONE_MINUS_VERTEX) ||
						(pass->alphagen == ALPHA_GEN_VERTEX)))
		{
			for (vertcount = 0, mno = 0; mno < shaderstate.nummeshes; mno++)
			{
				m = shaderstate.meshlist[mno];
				src = m->colors4f_array[0];
				for (v = 0; v < m->numvertexes; v++)
				{
					c = src[0]*255;
					map[0] = bound(0, c, 255);
					c = src[1]*255;
					map[1] = bound(0, c, 255);
					c = src[2]*255;
					map[2] = bound(0, c, 255);
					c = src[3]*255;
					map[3] = bound(0, c, 255);
					map += 4;
					src += 4;
				}
				vertcount += m->numvertexes;
			}
			map -= vertcount*4;
			/*FIXME: m is wrong. its the last ent only*/
			colourgenbyte(pass, vertcount, (byte_vec4_t*)map, (byte_vec4_t*)map, m);
			alphagenbyte(pass, vertcount, (byte_vec4_t*)map, (byte_vec4_t*)map, m);
		}
		else
		{
			for (vertcount = 0, mno = 0; mno < shaderstate.nummeshes; mno++)
			{
				m = shaderstate.meshlist[mno];
				colourgenbyte(pass, m->numvertexes, NULL, (byte_vec4_t*)map, m);
				alphagenbyte(pass, m->numvertexes, NULL, (byte_vec4_t*)map, m);
				map += m->numvertexes*4;
				vertcount += m->numvertexes;
			}
		}
		d3dcheck(IDirect3DVertexBuffer9_Unlock(shaderstate.dyncol_buff));
		d3dcheck(IDirect3DDevice9_SetStreamSource(pD3DDev9, 1, shaderstate.dyncol_buff, shaderstate.dyncol_offs - vertcount*sizeof(D3DCOLOR), sizeof(D3DCOLOR)));
	}
	return ret;
}
/*********************************************************************************************************/
/*========================================== texture coord generation =====================================*/

static void tcgen_environment(float *st, unsigned int numverts, float *xyz, float *normal) 
{
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

	case TC_GEN_DOTPRODUCT:
		return dst;//mesh->st_array[0];
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

static void deformgen(const deformv_t *deformv, int cnt, const vecV_t *src, vecV_t *dst, const mesh_t *mesh)
{
	float *table;
	int j, k;
	float args[4];
	float deflect;
	switch (deformv->type)
	{
	default:
	case DEFORMV_NONE:
		if (src != (const avec4_t*)dst)
			memcpy(dst, src, sizeof(*src)*cnt);
		break;

	case DEFORMV_WAVE:
		if (!mesh->normals_array)
		{
			if (src != (const avec4_t*)dst)
				memcpy(dst, src, sizeof(*src)*cnt);
			return;
		}
		args[0] = deformv->func.args[0];
		args[1] = deformv->func.args[1];
		args[3] = deformv->func.args[2] + deformv->func.args[3] * shaderstate.curtime;
		table = FTableForFunc(deformv->func.type);

		for ( j = 0; j < cnt; j++ )
		{
			deflect = deformv->args[0] * (src[j][0]+src[j][1]+src[j][2]) + args[3];
			deflect = FTABLE_EVALUATE(table, deflect) * args[1] + args[0];

			// Deflect vertex along its normal by wave amount
			VectorMA(src[j], deflect, mesh->normals_array[j], dst[j]);
		}
		break;

	case DEFORMV_NORMAL:
		//normal does not actually move the verts, but it does change the normals array
		//we don't currently support that.
		if (src != (const avec4_t*)dst)
			memcpy(dst, src, sizeof(*src)*cnt);
/*
		args[0] = deformv->args[1] * shaderstate.curtime;

		for ( j = 0; j < cnt; j++ )
		{
			args[1] = normalsArray[j][2] * args[0];

			deflect = deformv->args[0] * R_FastSin(args[1]);
			normalsArray[j][0] *= deflect;
			deflect = deformv->args[0] * R_FastSin(args[1] + 0.25);
			normalsArray[j][1] *= deflect;
			VectorNormalizeFast(normalsArray[j]);
		}
*/		break;

	case DEFORMV_MOVE:
		table = FTableForFunc(deformv->func.type);
		deflect = deformv->func.args[2] + shaderstate.curtime * deformv->func.args[3];
		deflect = FTABLE_EVALUATE(table, deflect) * deformv->func.args[1] + deformv->func.args[0];

		for ( j = 0; j < cnt; j++ )
			VectorMA(src[j], deflect, deformv->args, dst[j]);
		break;

	case DEFORMV_BULGE:
		args[0] = deformv->args[0]/(2*M_PI);
		args[1] = deformv->args[1];
		args[2] = shaderstate.curtime * deformv->args[2]/(2*M_PI);

		for (j = 0; j < cnt; j++)
		{
			deflect = R_FastSin(mesh->st_array[j][0]*args[0] + args[2])*args[1];
			dst[j][0] = src[j][0]+deflect*mesh->normals_array[j][0];
			dst[j][1] = src[j][1]+deflect*mesh->normals_array[j][1];
			dst[j][2] = src[j][2]+deflect*mesh->normals_array[j][2];
		}
		break;

	case DEFORMV_AUTOSPRITE:
		if (mesh->numindexes < 6)
			break;

		for (j = 0; j < cnt-3; j+=4, src+=4, dst+=4)
		{
			vec3_t mid, d;
			float radius;
			mid[0] = 0.25*(src[0][0] + src[1][0] + src[2][0] + src[3][0]);
			mid[1] = 0.25*(src[0][1] + src[1][1] + src[2][1] + src[3][1]);
			mid[2] = 0.25*(src[0][2] + src[1][2] + src[2][2] + src[3][2]);
			VectorSubtract(src[0], mid, d);
			radius = 2*VectorLength(d);

			for (k = 0; k < 4; k++)
			{
				dst[k][0] = mid[0] + radius*((mesh->st_array[k][0]-0.5)*r_refdef.m_view[0+0]-(mesh->st_array[k][1]-0.5)*r_refdef.m_view[0+1]);
				dst[k][1] = mid[1] + radius*((mesh->st_array[k][0]-0.5)*r_refdef.m_view[4+0]-(mesh->st_array[k][1]-0.5)*r_refdef.m_view[4+1]);
				dst[k][2] = mid[2] + radius*((mesh->st_array[k][0]-0.5)*r_refdef.m_view[8+0]-(mesh->st_array[k][1]-0.5)*r_refdef.m_view[8+1]);
			}
		}
		break;

	case DEFORMV_AUTOSPRITE2:
		if (mesh->numindexes < 6)
			break;

		for (k = 0; k < mesh->numindexes; k += 6)
		{
			int long_axis, short_axis;
			vec3_t axis;
			float len[3];
			mat3_t m0, m1, m2, result;
			float *quad[4];
			vec3_t rot_centre, tv;

			quad[0] = (float *)(dst + mesh->indexes[k+0]);
			quad[1] = (float *)(dst + mesh->indexes[k+1]);
			quad[2] = (float *)(dst + mesh->indexes[k+2]);

			for (j = 2; j >= 0; j--)
			{
				quad[3] = (float *)(dst + mesh->indexes[k+3+j]);
				if (!VectorEquals (quad[3], quad[0]) && 
					!VectorEquals (quad[3], quad[1]) &&
					!VectorEquals (quad[3], quad[2]))
				{
					break;
				}
			}

			// build a matrix were the longest axis of the billboard is the Y-Axis
			VectorSubtract(quad[1], quad[0], m0[0]);
			VectorSubtract(quad[2], quad[0], m0[1]);
			VectorSubtract(quad[2], quad[1], m0[2]);
			len[0] = DotProduct(m0[0], m0[0]);
			len[1] = DotProduct(m0[1], m0[1]);
			len[2] = DotProduct(m0[2], m0[2]);

			if ((len[2] > len[1]) && (len[2] > len[0]))
			{
				if (len[1] > len[0])
				{
					long_axis = 1;
					short_axis = 0;
				}
				else
				{
					long_axis = 0;
					short_axis = 1;
				}
			}
			else if ((len[1] > len[2]) && (len[1] > len[0]))
			{
				if (len[2] > len[0])
				{
					long_axis = 2;
					short_axis = 0;
				}
				else
				{
					long_axis = 0;
					short_axis = 2;
				}
			}
			else //if ( (len[0] > len[1]) && (len[0] > len[2]) )
			{
				if (len[2] > len[1])
				{
					long_axis = 2;
					short_axis = 1;
				}
				else
				{
					long_axis = 1;
					short_axis = 2;
				}
			}

			if (DotProduct(m0[long_axis], m0[short_axis]))
			{
				VectorNormalize2(m0[long_axis], axis);
				VectorCopy(axis, m0[1]);

				if (axis[0] || axis[1])
				{
					VectorVectors(m0[1], m0[2], m0[0]);
				}
				else
				{
					VectorVectors(m0[1], m0[0], m0[2]);
				}
			}
			else
			{
				VectorNormalize2(m0[long_axis], axis);
				VectorNormalize2(m0[short_axis], m0[0]);
				VectorCopy(axis, m0[1]);
				CrossProduct(m0[0], m0[1], m0[2]);
			}

			for (j = 0; j < 3; j++)
				rot_centre[j] = (quad[0][j] + quad[1][j] + quad[2][j] + quad[3][j]) * 0.25;

			if (shaderstate.curentity)
			{
				VectorAdd(shaderstate.curentity->origin, rot_centre, tv);
			}
			else
			{
				VectorCopy(rot_centre, tv);
			}
			VectorSubtract(r_origin, tv, tv);

			// filter any longest-axis-parts off the camera-direction
			deflect = -DotProduct(tv, axis);

			VectorMA(tv, deflect, axis, m1[2]);
			VectorNormalizeFast(m1[2]);
			VectorCopy(axis, m1[1]);
			CrossProduct(m1[1], m1[2], m1[0]);

			Matrix3_Transpose(m1, m2);
			Matrix3_Multiply(m2, m0, result);

			for (j = 0; j < 4; j++)
			{
				VectorSubtract(quad[j], rot_centre, tv);
				Matrix3_Multiply_Vec3(result, tv, quad[j]);
				VectorAdd(rot_centre, quad[j], quad[j]);
			}
		}
		break;

//	case DEFORMV_PROJECTION_SHADOW:
//		break;
	}
}



/*does not do the draw call, does not consider indicies (except for billboard generation) */
static qboolean BE_DrawMeshChain_SetupPass(shaderpass_t *pass, unsigned int vertcount)
{
	int vdec;
	void *map;
	int i;
	unsigned int passno = 0, tmu;

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
		return false;

	/*all meshes in a chain must have the same features*/

	vdec = 0;

	/*we only use one colour, generated from the first pass*/
	vdec |= BE_GenerateColourMods(vertcount, pass);

	tmu = 0;
	/*activate tmus*/
	for (passno = 0; passno < lastpass; passno++)
	{
		if (pass[passno].texgen == T_GEN_UPPEROVERLAY && !TEXVALID(shaderstate.curtexnums->upperoverlay))
			continue;
		if (pass[passno].texgen == T_GEN_LOWEROVERLAY && !TEXVALID(shaderstate.curtexnums->loweroverlay))
			continue;
		if (pass[passno].texgen == T_GEN_FULLBRIGHT && !TEXVALID(shaderstate.curtexnums->fullbright))
			continue;

		SelectPassTexture(tmu, pass+passno);

		vdec |= D3D_VDEC_ST0<<tmu;
		allocvertexbuffer(shaderstate.dynst_buff[tmu], shaderstate.dynst_size, &shaderstate.dynst_offs[tmu], &map, vertcount*sizeof(vec2_t));
		GenerateTCMods(pass+passno, map);
		d3dcheck(IDirect3DVertexBuffer9_Unlock(shaderstate.dynst_buff[tmu]));
		d3dcheck(IDirect3DDevice9_SetStreamSource(pD3DDev9, 5+tmu, shaderstate.dynst_buff[tmu], shaderstate.dynst_offs[tmu] - vertcount*sizeof(vec2_t), sizeof(vec2_t)));
		tmu++;
	}
	/*deactivate any extras*/
	for (; tmu < shaderstate.lastpasscount; )
	{
		d3dcheck(IDirect3DDevice9_SetStreamSource(pD3DDev9, 5+tmu, NULL, 0, 0));
		BindTexture(tmu, NULL);
		d3dcheck(IDirect3DDevice9_SetTextureStageState(pD3DDev9, tmu, D3DTSS_COLOROP, D3DTOP_DISABLE));
		tmu++;
	}
	shaderstate.lastpasscount = tmu;

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
	return true;
}

static void BE_Cull(unsigned int cullflags)
{
	cullflags |= r_refdef.flipcull;
	if (shaderstate.curcull != cullflags)
	{
		shaderstate.curcull = cullflags;
		if (shaderstate.curcull & 1)
		{
			if (shaderstate.curcull & SHADER_CULL_FRONT)
				IDirect3DDevice9_SetRenderState(pD3DDev9, D3DRS_CULLMODE, D3DCULL_CW);
			else if (shaderstate.curcull & SHADER_CULL_BACK)
				IDirect3DDevice9_SetRenderState(pD3DDev9, D3DRS_CULLMODE, D3DCULL_CCW);
			else
				IDirect3DDevice9_SetRenderState(pD3DDev9, D3DRS_CULLMODE, D3DCULL_NONE);
		}
		else
		{
			if (shaderstate.curcull & SHADER_CULL_FRONT)
				IDirect3DDevice9_SetRenderState(pD3DDev9, D3DRS_CULLMODE, D3DCULL_CCW);
			else if (shaderstate.curcull & SHADER_CULL_BACK)
				IDirect3DDevice9_SetRenderState(pD3DDev9, D3DRS_CULLMODE, D3DCULL_CW);
			else
				IDirect3DDevice9_SetRenderState(pD3DDev9, D3DRS_CULLMODE, D3DCULL_NONE);
		}
	}
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

	BE_Cull(shaderstate.curshader->flags & (SHADER_CULL_FRONT | SHADER_CULL_BACK));

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
		vecV_t *dest = (vecV_t*)((char*)map+vertcount*sizeof(vecV_t));
		m = shaderstate.meshlist[mno];
		deformgen(&shaderstate.curshader->deforms[0], m->numvertexes, m->xyz_array, dest, m);
		for (i = 1; i < shaderstate.curshader->numdeforms; i++)
		{
			deformgen(&shaderstate.curshader->deforms[i], m->numvertexes, dest, dest, m);
		}
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

	if (shaderstate.mode == BEM_DEPTHONLY)
	{
		IDirect3DDevice9_SetRenderState(pD3DDev9, D3DRS_COLORWRITEENABLE, 0);
		/*deactivate any extras*/
		for (passno = 0; passno < shaderstate.lastpasscount; )
		{
			d3dcheck(IDirect3DDevice9_SetStreamSource(pD3DDev9, 5+passno, NULL, 0, 0));
			BindTexture(passno, NULL);
			d3dcheck(IDirect3DDevice9_SetTextureStageState(pD3DDev9, passno, D3DTSS_COLOROP, D3DTOP_DISABLE));
			passno++;
		}
		shaderstate.lastpasscount = 0;
		d3dcheck(IDirect3DDevice9_DrawIndexedPrimitive(pD3DDev9, D3DPT_TRIANGLELIST, 0, 0, vertcount, idxfirst, idxcount/3));
		IDirect3DDevice9_SetRenderState(pD3DDev9, D3DRS_COLORWRITEENABLE, D3DCOLORWRITEENABLE_RED|D3DCOLORWRITEENABLE_GREEN|D3DCOLORWRITEENABLE_BLUE|D3DCOLORWRITEENABLE_ALPHA);
	}
	else
	{
		/*now go through and flush each pass*/
		for (passno = 0; passno < shaderstate.curshader->numpasses; passno += pass->numMergedPasses)
		{
			if (!BE_DrawMeshChain_SetupPass(pass+passno, vertcount))
				continue;
	#ifdef BENCH
			shaderstate.bench.draws++;
			if (shaderstate.bench.clamp && shaderstate.bench.clamp < shaderstate.bench.draws)
				continue;
	#endif
			d3dcheck(IDirect3DDevice9_DrawIndexedPrimitive(pD3DDev9, D3DPT_TRIANGLELIST, 0, 0, vertcount, idxfirst, idxcount/3));
		}
	}
}

void D3DBE_SelectMode(backendmode_t mode, unsigned int flags)
{
	shaderstate.mode = mode;
	shaderstate.flags = flags;
}

/*Generates an optimised vbo for each of the given model's textures*/
void D3DBE_GenBrushModelVBO(model_t *mod)
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
void D3DBE_ClearVBO(vbo_t *vbo)
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

void D3DBE_UploadAllLightmaps(void)
{
	BE_UploadLightmaps(true);
}

qboolean D3DBE_LightCullModel(vec3_t org, model_t *model)
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

batch_t *D3DBE_GetTempBatch(void)
{
	if (shaderstate.wbatch >= shaderstate.maxwbatches)
	{
		shaderstate.wbatch++;
		return NULL;
	}
	return &shaderstate.wbatches[shaderstate.wbatch++];
}

static void BE_RotateForEntity (const entity_t *e, const model_t *mod)
{
	float mv[16];
	float m[16];

	shaderstate.curentity = e;

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

		IDirect3DDevice9_SetTransform(pD3DDev9, D3DTS_WORLD, (D3DMATRIX*)mv);
	}
	else
	{
		IDirect3DDevice9_SetTransform(pD3DDev9, D3DTS_WORLD, (D3DMATRIX*)m);
	}

	{
	D3DVIEWPORT9 vport;
	IDirect3DDevice9_GetViewport(pD3DDev9, &vport);
	vport.MaxZ = (e->flags & Q2RF_DEPTHHACK)?0.333:1;
	IDirect3DDevice9_SetViewport(pD3DDev9, &vport);
	}
}

void D3DBE_SubmitBatch(batch_t *batch)
{
	shaderstate.nummeshes = batch->meshes - batch->firstmesh;
	if (!shaderstate.nummeshes)
		return;
	if (shaderstate.curentity != batch->ent)
	{
		BE_RotateForEntity(batch->ent, batch->ent->model);
		shaderstate.curtime = r_refdef.time - shaderstate.curentity->shaderTime;
	}
	shaderstate.meshlist = batch->mesh + batch->firstmesh;
	shaderstate.curshader = batch->shader;
	shaderstate.curtexnums = batch->skin;
	shaderstate.flags = batch->flags;
	if (batch->lightmap < 0)
		shaderstate.curlightmap = r_nulltex;
	else
		shaderstate.curlightmap = lightmap_textures[batch->lightmap];

	BE_DrawMeshChain_Internal();
}

void D3DBE_DrawMesh_List(shader_t *shader, int nummeshes, mesh_t **meshlist, vbo_t *vbo, texnums_t *texnums)
{
	shaderstate.curshader = shader;
	shaderstate.curtexnums = texnums;
	shaderstate.curlightmap = r_nulltex;
	shaderstate.meshlist = meshlist;
	shaderstate.nummeshes = nummeshes;

	BE_DrawMeshChain_Internal();
}

void D3DBE_DrawMesh_Single(shader_t *shader, mesh_t *meshchain, vbo_t *vbo, texnums_t *texnums)
{
	shaderstate.curtime = realtime;
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



#ifdef Q3CLIENT

//q3 lightning gun
static void R_DrawLightning(entity_t *e)
{
	vec3_t v;
	vec3_t dir, cr;
	float scale = e->scale;
	float length;

	vecV_t points[4];
	vec2_t texcoords[4] = {{0, 0}, {0, 1}, {1, 1}, {1, 0}};
	static index_t indexarray[6] = {0, 1, 2, 0, 2, 3};

	mesh_t mesh;

	if (!e->forcedshader)
		return;

	if (!scale)
		scale = 10;


	VectorSubtract(e->origin, e->oldorigin, dir);
	length = Length(dir);

	//this seems to be about right.
	texcoords[2][0] = length/128;
	texcoords[3][0] = length/128;

	VectorSubtract(r_refdef.vieworg, e->origin, v);
	CrossProduct(v, dir, cr);
	VectorNormalize(cr);

	VectorMA(e->origin, -scale/2, cr, points[0]);
	VectorMA(e->origin, scale/2, cr, points[1]);

	VectorSubtract(r_refdef.vieworg, e->oldorigin, v);
	CrossProduct(v, dir, cr);
	VectorNormalize(cr);

	VectorMA(e->oldorigin, scale/2, cr, points[2]);
	VectorMA(e->oldorigin, -scale/2, cr, points[3]);

	memset(&mesh, 0, sizeof(mesh));
	mesh.vbofirstelement = 0;
	mesh.vbofirstvert = 0;
	mesh.xyz_array = points;
	mesh.indexes = indexarray;
	mesh.numindexes = sizeof(indexarray)/sizeof(indexarray[0]);
	mesh.colors4f_array = NULL;
	mesh.lmst_array = NULL;
	mesh.normals_array = NULL;
	mesh.numvertexes = 4;
	mesh.st_array = texcoords;
	BE_DrawMesh_Single(e->forcedshader, &mesh, NULL, NULL);
}
//q3 railgun beam
static void R_DrawRailCore(entity_t *e)
{
	vec3_t v;
	vec3_t dir, cr;
	float scale = e->scale;
	float length;

	mesh_t mesh;
	vecV_t points[4];
	vec2_t texcoords[4] = {{0, 0}, {0, 1}, {1, 1}, {1, 0}};
	static index_t indexarray[6] = {0, 1, 2, 0, 2, 3};
	vec4_t colors[4];

	if (!e->forcedshader)
		return;

	if (!scale)
		scale = 10;


	VectorSubtract(e->origin, e->oldorigin, dir);
	length = Length(dir);

	//this seems to be about right.
	texcoords[2][0] = length/128;
	texcoords[3][0] = length/128;

	VectorSubtract(r_refdef.vieworg, e->origin, v);
	CrossProduct(v, dir, cr);
	VectorNormalize(cr);

	VectorMA(e->origin, -scale/2, cr, points[0]);
	VectorMA(e->origin, scale/2, cr, points[1]);

	VectorSubtract(r_refdef.vieworg, e->oldorigin, v);
	CrossProduct(v, dir, cr);
	VectorNormalize(cr);

	VectorMA(e->oldorigin, scale/2, cr, points[2]);
	VectorMA(e->oldorigin, -scale/2, cr, points[3]);

	Vector4Copy(e->shaderRGBAf, colors[0]);
	Vector4Copy(e->shaderRGBAf, colors[1]);
	Vector4Copy(e->shaderRGBAf, colors[2]);
	Vector4Copy(e->shaderRGBAf, colors[3]);

	memset(&mesh, 0, sizeof(mesh));
	mesh.vbofirstelement = 0;
	mesh.vbofirstvert = 0;
	mesh.xyz_array = points;
	mesh.indexes = indexarray;
	mesh.numindexes = sizeof(indexarray)/sizeof(indexarray[0]);
	mesh.colors4f_array = (vec4_t*)colors;
	mesh.lmst_array = NULL;
	mesh.normals_array = NULL;
	mesh.numvertexes = 4;
	mesh.st_array = texcoords;

	BE_DrawMesh_Single(e->forcedshader, &mesh, NULL, NULL);
}
#endif
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

		if (!BE_ShouldDraw(ent))
			continue;

		switch(ent->rtype)
		{
		case RT_MODEL:
		default:
			if (!ent->model)
				continue;
			if (ent->model->needload)
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
			case mod_sprite:
				break;
			}
			break;
		case RT_SPRITE:
			//RQ_AddDistReorder(GLR_DrawSprite, currententity, NULL, currententity->origin);
			break;

#ifdef Q3CLIENT
		case RT_BEAM:
		case RT_RAIL_RINGS:
		case RT_LIGHTNING:
			R_DrawLightning(ent);
			continue;
		case RT_RAIL_CORE:
			R_DrawRailCore(ent);
			continue;
#endif

		case RT_POLY:
			/*not implemented*/
			break;
		case RT_PORTALSURFACE:
			/*nothing*/
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
		else if (batch->texture)
		{
			batch->shader = R_TextureAnimation(batch->ent->framestate.g[FS_REG].frame[0], batch->texture)->shader;
			batch->skin = &batch->shader->defaulttextures;
		}

		if (batch->shader->flags & SHADER_NODLIGHT)
			if (shaderstate.mode == BEM_LIGHT || shaderstate.mode == BEM_SMAPLIGHT)
				continue;

		if (batch->shader->flags & SHADER_SKY)
		{
			if (shaderstate.mode == BEM_STANDARD)
				R_DrawSkyChain (batch);
			continue;
		}

		BE_SubmitBatch(batch);
	}
}


/*generates a new modelview matrix, as well as vpn vectors*/
static void R_MirrorMatrix(plane_t *plane)
{
	float mirror[16];
	float view[16];
	float result[16];

	vec3_t pnorm;
	VectorNegate(plane->normal, pnorm);

	mirror[0] = 1-2*pnorm[0]*pnorm[0];
	mirror[1] = -2*pnorm[0]*pnorm[1];
	mirror[2] = -2*pnorm[0]*pnorm[2];
	mirror[3] = 0;

	mirror[4] = -2*pnorm[1]*pnorm[0];
	mirror[5] = 1-2*pnorm[1]*pnorm[1];
	mirror[6] = -2*pnorm[1]*pnorm[2] ;
	mirror[7] = 0;

	mirror[8]  = -2*pnorm[2]*pnorm[0];
	mirror[9]  = -2*pnorm[2]*pnorm[1];
	mirror[10] = 1-2*pnorm[2]*pnorm[2];
	mirror[11] = 0;

	mirror[12] = -2*pnorm[0]*plane->dist;
	mirror[13] = -2*pnorm[1]*plane->dist;
	mirror[14] = -2*pnorm[2]*plane->dist;
	mirror[15] = 1;

	view[0] = vpn[0];
	view[1] = vpn[1];
	view[2] = vpn[2];
	view[3] = 0;

	view[4] = -vright[0];
	view[5] = -vright[1];
	view[6] = -vright[2];
	view[7] = 0;

	view[8]  = vup[0];
	view[9]  = vup[1];
	view[10] = vup[2];
	view[11] = 0;

	view[12] = r_refdef.vieworg[0];
	view[13] = r_refdef.vieworg[1];
	view[14] = r_refdef.vieworg[2];
	view[15] = 1;

	VectorMA(r_refdef.vieworg, 0.25, plane->normal, r_refdef.pvsorigin);

	Matrix4_Multiply(mirror, view, result);

	vpn[0] = result[0];
	vpn[1] = result[1];
	vpn[2] = result[2];

	vright[0] = -result[4];
	vright[1] = -result[5];
	vright[2] = -result[6];

	vup[0] = result[8];
	vup[1] = result[9];
	vup[2] = result[10];

	r_refdef.vieworg[0] = result[12];
	r_refdef.vieworg[1] = result[13];
	r_refdef.vieworg[2] = result[14];
}
static entity_t *R_NearestPortal(plane_t *plane)
{
	int i;
	entity_t *best = NULL;
	float dist, bestd = 0;
	for (i = 0; i < cl_numvisedicts; i++)
	{
		if (cl_visedicts[i].rtype == RT_PORTALSURFACE)
		{
			dist = DotProduct(cl_visedicts[i].origin, plane->normal)-plane->dist;
			dist = fabs(dist);
			if (dist < 64 && (!best || dist < bestd))
				best = &cl_visedicts[i];
		}
	}
	return best;
}

static void TransformCoord(vec3_t in, vec3_t planea[3], vec3_t planeo, vec3_t viewa[3], vec3_t viewo, vec3_t result)
{
	int		i;
	vec3_t	local;
	vec3_t	transformed;
	float	d;

	local[0] = in[0] - planeo[0];
	local[1] = in[1] - planeo[1];
	local[2] = in[2] - planeo[2];

	VectorClear(transformed);
	for ( i = 0 ; i < 3 ; i++ )
	{
		d = DotProduct(local, planea[i]);
		VectorMA(transformed, d, viewa[i], transformed);
	}

	result[0] = transformed[0] + viewo[0];
	result[1] = transformed[1] + viewo[1];
	result[2] = transformed[2] + viewo[2];
}
static void TransformDir(vec3_t in, vec3_t planea[3], vec3_t viewa[3], vec3_t result)
{
	int		i;
	float	d;
	vec3_t tmp;

	VectorCopy(in, tmp);

	VectorClear(result);
	for ( i = 0 ; i < 3 ; i++ )
	{
		d = DotProduct(tmp, planea[i]);
		VectorMA(result, d, viewa[i], result);
	}
}
static R_RenderScene(void)
{
	IDirect3DDevice9_SetTransform(pD3DDev9, D3DTS_PROJECTION, (D3DMATRIX*)r_refdef.m_projection);
	IDirect3DDevice9_SetTransform(pD3DDev9, D3DTS_VIEW, (D3DMATRIX*)r_refdef.m_view);
	R_SetFrustum (r_refdef.m_projection, r_refdef.m_view);
	Surf_DrawWorld();
}

static void R_DrawPortal(batch_t *batch, batch_t **blist)
{
	entity_t *view;
	float glplane[4];
	plane_t plane;
	refdef_t oldrefdef;
	mesh_t *mesh = batch->mesh[batch->firstmesh];
	int sort;

	if (r_refdef.recurse)
		return;

	VectorCopy(mesh->normals_array[0], plane.normal);
	plane.dist = DotProduct(mesh->xyz_array[0], plane.normal);

	//if we're too far away from the surface, don't draw anything
	if (batch->shader->flags & SHADER_AGEN_PORTAL)
	{
		/*there's a portal alpha blend on that surface, that fades out after this distance*/
		if (DotProduct(r_refdef.vieworg, plane.normal)-plane.dist > batch->shader->portaldist)
			return;
	}
	//if we're behind it, then also don't draw anything.
	if (DotProduct(r_refdef.vieworg, plane.normal)-plane.dist < 0)
		return;

	view = R_NearestPortal(&plane);
	//if (!view)
	//	return;

	oldrefdef = r_refdef;
	r_refdef.recurse = true;

	r_refdef.externalview = true;

	if (!view || VectorCompare(view->origin, view->oldorigin))
	{
		r_refdef.flipcull ^= true;
		R_MirrorMatrix(&plane);
	}
	else
	{
		float d;
		vec3_t paxis[3], porigin, vaxis[3], vorg;
		void PerpendicularVector( vec3_t dst, const vec3_t src );

		/*calculate where the surface is meant to be*/
		VectorCopy(mesh->normals_array[0], paxis[0]);
		PerpendicularVector(paxis[1], paxis[0]);
		CrossProduct(paxis[0], paxis[1], paxis[2]);
		d = DotProduct(view->origin, plane.normal) - plane.dist;
		VectorMA(view->origin, -d, paxis[0], porigin);

		/*grab the camera origin*/
		VectorNegate(view->axis[0], vaxis[0]);
		VectorNegate(view->axis[1], vaxis[1]);
		VectorCopy(view->axis[2], vaxis[2]);
		VectorCopy(view->oldorigin, vorg);

		VectorCopy(vorg, r_refdef.pvsorigin);

		/*rotate it a bit*/
		RotatePointAroundVector(vaxis[1], vaxis[0], view->axis[1], sin(realtime)*4);
		CrossProduct(vaxis[0], vaxis[1], vaxis[2]);

		TransformCoord(oldrefdef.vieworg, paxis, porigin, vaxis, vorg, r_refdef.vieworg);
		TransformDir(vpn, paxis, vaxis, vpn);
		TransformDir(vright, paxis, vaxis, vright);
		TransformDir(vup, paxis, vaxis, vup);
	}
	Matrix4_ModelViewMatrixFromAxis(r_refdef.m_view, vpn, vright, vup, r_refdef.vieworg);
	VectorAngles(vpn, vup, r_refdef.viewangles);
	VectorCopy(r_refdef.vieworg, r_origin);

/*FIXME: the batch stuff should be done in renderscene*/

	/*fixup the first mesh index*/
	for (sort = 0; sort < SHADER_SORT_COUNT; sort++)
	for (batch = blist[sort]; batch; batch = batch->next)
	{
		batch->firstmesh = batch->meshes;
	}

	/*FIXME: can we get away with stenciling the screen?*/
	/*Add to frustum culling instead of clip planes?*/
	glplane[0] = plane.normal[0];
	glplane[1] = plane.normal[1];
	glplane[2] = plane.normal[2];
	glplane[3] = -plane.dist;
	IDirect3DDevice9_SetClipPlane(pD3DDev9, 0, glplane);
	IDirect3DDevice9_SetRenderState(pD3DDev9, D3DRS_CLIPPLANEENABLE, D3DCLIPPLANE0);
	R_RenderScene();
	IDirect3DDevice9_SetRenderState(pD3DDev9, D3DRS_CLIPPLANEENABLE, 0);

	for (sort = 0; sort < SHADER_SORT_COUNT; sort++)
	for (batch = blist[sort]; batch; batch = batch->next)
	{
		batch->firstmesh = 0;
	}
	r_refdef = oldrefdef;

	/*broken stuff*/
	AngleVectors (r_refdef.viewangles, vpn, vright, vup);
	VectorCopy (r_refdef.vieworg, r_origin);

	IDirect3DDevice9_SetTransform(pD3DDev9, D3DTS_PROJECTION, (D3DMATRIX*)r_refdef.m_projection);
	IDirect3DDevice9_SetTransform(pD3DDev9, D3DTS_VIEW, (D3DMATRIX*)r_refdef.m_view);
	R_SetFrustum (r_refdef.m_projection, r_refdef.m_view);
}

static void BE_SubmitMeshesPortals(batch_t **worldlist, batch_t *dynamiclist)
{
	batch_t *batch, *old;
	int i;
	/*attempt to draw portal shaders*/
	if (shaderstate.mode == BEM_STANDARD)
	{
		for (i = 0; i < 2; i++)
		{
			for (batch = i?dynamiclist:worldlist[SHADER_SORT_PORTAL]; batch; batch = batch->next)
			{
				if (batch->meshes == batch->firstmesh)
					continue;

				if (batch->buildmeshes)
					batch->buildmeshes(batch);
				else
					batch->shader = R_TextureAnimation(batch->ent->framestate.g[FS_REG].frame[0], batch->texture)->shader;


				/*draw already-drawn portals as depth-only, to ensure that their contents are not harmed*/
				BE_SelectMode(BEM_DEPTHONLY, 0);
				for (old = worldlist[SHADER_SORT_PORTAL]; old && old != batch; old = old->next)
				{
					if (old->meshes == old->firstmesh)
						continue;
					BE_SubmitBatch(old);
				}
				if (!old)
				{
					for (old = dynamiclist; old != batch; old = old->next)
					{
						if (old->meshes == old->firstmesh)
							continue;
						BE_SubmitBatch(old);
					}
				}
				BE_SelectMode(BEM_STANDARD, 0);

				R_DrawPortal(batch, worldlist);

				/*clear depth again*/
				IDirect3DDevice9_Clear(pD3DDev9, 0, NULL, D3DCLEAR_ZBUFFER, D3DCOLOR_XRGB(0,0,0), 1, 0);
			}
		}
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
			if (i == SHADER_SORT_PORTAL /*&& !r_noportals.ival*/ && !r_refdef.recurse)
				BE_SubmitMeshesPortals(model->batches, blist[i]);

			BE_SubmitMeshesSortList(model->batches[i]);
		}
		BE_SubmitMeshesSortList(blist[i]);
	}
}

void D3DBE_DrawWorld (qbyte *vis)
{
	batch_t *batches[SHADER_SORT_COUNT];
	RSpeedLocals();

	shaderstate.curentity = NULL;

	if (!r_refdef.recurse)
	{
		if (shaderstate.wbatch > shaderstate.maxwbatches)
		{
			int newm = shaderstate.wbatch;
			shaderstate.wbatches = BZ_Realloc(shaderstate.wbatches, newm * sizeof(*shaderstate.wbatches));
			memset(shaderstate.wbatches + shaderstate.maxwbatches, 0, (newm - shaderstate.maxwbatches) * sizeof(*shaderstate.wbatches));
			shaderstate.maxwbatches = newm;
		}
		shaderstate.wbatch = 0;
	}

	BE_GenModelBatches(batches);

	if (vis)
	{
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
	}
	else
	{
		RSpeedRemark();
		BE_SubmitMeshes(false, batches);
		RSpeedEnd(RSPEED_DRAWENTITIES);
	}

	BE_RotateForEntity(&r_worldentity, NULL);
}


#endif
