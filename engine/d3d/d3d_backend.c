#include "quakedef.h"
#include "glquake.h"
#include "gl_draw.h"
#ifdef D3D9QUAKE
#include "shader.h"
#if !defined(HMONITOR_DECLARED) && (WINVER < 0x0500)
    #define HMONITOR_DECLARED
    DECLARE_HANDLE(HMONITOR);
#endif
#include <d3d9.h>

/*
Things to improve:
	mmapping:
		this code is fairly simple. the caller gives a series of batches/meshes, and this code pushes the data to the gpu in its entirety before moving on to the next batch
		for the world, this is fast enough where its pretty much all vboed (except for weird shaders).
		but for lightning beams and models etc, we're mmapping new data regions in the dynamic vbo for every single quad.
		not just one mmap, but one mmap for every single individual attribute that is submitted
		by using a single buffer for all dynamic data, and filling that buffer before flushing anything to the hardware we ought to get higher framerates
		especially if this means D3DLOCK_NOOVERWRITE checks etc are not doing weird slow things.
		This won't affect the world too much, but should stop everything else destroying the framerate.

		long story short, make this batch properly, and use D3DLOCK_DISCARD only.

	models:
		models should already be in a buffer (with different offsets for different poses). This is a problem with gl too.

	hlsl:
		we don't really use this as much as we should. Should be possible to use as a fast path to avoid having to do any weird shader/dynamic stuff.
		currently used for water(required to mimic software rendering) and sky(because its muuuch faster than a skydome).
		hlsl programs ought to have per-hlsl vertex declarations, and only one stream source.
*/






extern LPDIRECT3DDEVICE9 pD3DDev9;

//#define d3dcheck(foo) foo
#define d3dcheck(foo) do{HRESULT err = foo; if (FAILED(err)) Sys_Error("D3D reported error on backend line %i - error 0x%x\n", __LINE__, (unsigned int)err);} while(0)

#define MAX_TMUS 4

extern float d3d_trueprojection[16];

static void BE_RotateForEntity (const entity_t *e, const model_t *mod);

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
	const entity_t	*curentity;
	const dlight_t	*curdlight;
	vec3_t		curdlight_colours;
	shader_t	*curshader;
	texnums_t	*curtexnums;
	texid_t		curlightmap;
	texid_t		curdeluxmap;
	int			curvertdecl;
	unsigned int shaderbits;
	unsigned int curcull;
	float depthbias;
	float depthfactor;
	float m_model[16];
	unsigned int lastpasscount;
	vbo_t *batchvbo;

	shader_t	*shader_rtlight;
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

typedef struct
{
	vecV_t coord;
	vec2_t tex;
	vec2_t lm;
	vec3_t ndir;
	vec3_t sdir;
	vec3_t tdir;
	byte_vec4_t colorsb;
} vbovdata_t;

#define DYNVBUFFSIZE 65536
#define DYNIBUFFSIZE 65536

static d3dbackend_t shaderstate;

extern int be_maxpasses;

enum
{
	D3D_VDEC_COL4B = 1<<0,
	D3D_VDEC_ST0 = 1<<1,
	D3D_VDEC_ST1 = 1<<2,
	D3D_VDEC_ST2 = 1<<3,
	D3D_VDEC_ST3 = 1<<4,
	D3D_VDEC_NORM = 1<<5,
	D3D_VDEC_SKEL = 1<<6,
	D3D_VDEC_MAX = 1<<7,

};
#define STRM_VERT	0
#define STRM_COL	1
#define STRM_TC0	2
#define STRM_TC1	3
#define STRM_TC2	4
#define STRM_TC3	5
#define STRM_NORM	6
#define STRM_NORMS	7
#define STRM_NORMT	8
#define STRM_BONENUM	9
#define STRM_BONEWEIGHT	10
#define STRM_MAX 11
static IDirect3DVertexDeclaration9 *vertexdecls[D3D_VDEC_MAX];

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

static void BE_ApplyShaderBits(unsigned int bits)
{
	unsigned int delta;

	if (shaderstate.flags & (BEF_FORCEADDITIVE|BEF_FORCETRANSPARENT|BEF_FORCENODEPTH|BEF_FORCEDEPTHTEST|BEF_FORCEDEPTHWRITE))
	{
		if (shaderstate.flags & BEF_FORCEADDITIVE)
			bits = (bits & ~(SBITS_MISC_DEPTHWRITE|SBITS_BLEND_BITS|SBITS_ATEST_BITS))
						| (SBITS_SRCBLEND_SRC_ALPHA | SBITS_DSTBLEND_ONE);
		else if (shaderstate.flags & BEF_FORCETRANSPARENT)
		{
			if ((bits & SBITS_BLEND_BITS) == (SBITS_SRCBLEND_ONE|SBITS_DSTBLEND_ZERO) || !(bits & SBITS_BLEND_BITS)) 	/*if transparency is forced, clear alpha test bits*/
				bits = (bits & ~(SBITS_MISC_DEPTHWRITE|SBITS_BLEND_BITS|SBITS_ATEST_BITS))
							| (SBITS_SRCBLEND_SRC_ALPHA | SBITS_DSTBLEND_ONE_MINUS_SRC_ALPHA);
		}

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

	if (delta & (SBITS_MASK_BITS))
	{
		IDirect3DDevice9_SetRenderState(pD3DDev9, D3DRS_COLORWRITEENABLE,
				((bits&SBITS_MASK_RED)?0:D3DCOLORWRITEENABLE_RED) |
				((bits&SBITS_MASK_GREEN)?0:D3DCOLORWRITEENABLE_GREEN) |
				((bits&SBITS_MASK_BLUE)?0:D3DCOLORWRITEENABLE_BLUE) |
				((bits&SBITS_MASK_ALPHA)?0:D3DCOLORWRITEENABLE_ALPHA));
	}
}

void D3D9BE_Reset(qboolean before)
{
	int i, tmu;
	if (before)
	{
		IDirect3DDevice9_SetVertexDeclaration(pD3DDev9, NULL);
		shaderstate.curvertdecl = 0;
		for (i = 0; i < STRM_MAX; i++)
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
		D3DVERTEXELEMENT9 decl[13], declend=D3DDECL_END();
		int elements;

		for (i = 0; i < D3D_VDEC_MAX; i++)
		{
			elements = 0;
			decl[elements].Stream = STRM_VERT;
			decl[elements].Offset = 0;
			decl[elements].Type = D3DDECLTYPE_FLOAT3;
			decl[elements].Method = D3DDECLMETHOD_DEFAULT;
			decl[elements].Usage = D3DDECLUSAGE_POSITION;
			decl[elements].UsageIndex = 0;
			elements++;

			if (i & D3D_VDEC_COL4B)
			{
				decl[elements].Stream = STRM_COL;
				decl[elements].Offset = 0;
				decl[elements].Type = D3DDECLTYPE_D3DCOLOR;
				decl[elements].Method = D3DDECLMETHOD_DEFAULT;
				decl[elements].Usage = D3DDECLUSAGE_COLOR;
				decl[elements].UsageIndex = 0;
				elements++;
			}

			if (i & D3D_VDEC_NORM)
			{
				decl[elements].Stream = STRM_NORM;
				decl[elements].Offset = 0;
				decl[elements].Type = D3DDECLTYPE_FLOAT3;
				decl[elements].Method = D3DDECLMETHOD_DEFAULT;
				decl[elements].Usage = D3DDECLUSAGE_NORMAL;
				decl[elements].UsageIndex = 0;
				elements++;

				decl[elements].Stream = STRM_NORMS;
				decl[elements].Offset = 0;
				decl[elements].Type = D3DDECLTYPE_FLOAT3;
				decl[elements].Method = D3DDECLMETHOD_DEFAULT;
				decl[elements].Usage = D3DDECLUSAGE_TANGENT;
				decl[elements].UsageIndex = 0;
				elements++;

				decl[elements].Stream = STRM_NORMT;
				decl[elements].Offset = 0;
				decl[elements].Type = D3DDECLTYPE_FLOAT3;
				decl[elements].Method = D3DDECLMETHOD_DEFAULT;
				decl[elements].Usage = D3DDECLUSAGE_BINORMAL;
				decl[elements].UsageIndex = 0;
				elements++;
			}

			for (tmu = 0; tmu < MAX_TMUS; tmu++)
			{
				if (i & (D3D_VDEC_ST0<<tmu))
				{
					decl[elements].Stream = STRM_TC0+tmu;
					decl[elements].Offset = 0;
					decl[elements].Type = D3DDECLTYPE_FLOAT2;
					decl[elements].Method = D3DDECLMETHOD_DEFAULT;
					decl[elements].Usage = D3DDECLUSAGE_TEXCOORD;
					decl[elements].UsageIndex = tmu;
					elements++;
				}
			}

			if (i & D3D_VDEC_SKEL)
			{
				decl[elements].Stream = STRM_BONEWEIGHT;
				decl[elements].Offset = 0;
				decl[elements].Type = D3DDECLTYPE_FLOAT3;
				decl[elements].Method = D3DDECLMETHOD_DEFAULT;
				decl[elements].Usage = D3DDECLUSAGE_BLENDWEIGHT;
				decl[elements].UsageIndex = 0;
				elements++;

				decl[elements].Stream = STRM_BONENUM;
				decl[elements].Offset = 0;
				decl[elements].Type = D3DDECLTYPE_FLOAT3;
				decl[elements].Method = D3DDECLMETHOD_DEFAULT;
				decl[elements].Usage = D3DDECLUSAGE_BLENDINDICES;
				decl[elements].UsageIndex = 0;
				elements++;
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

		for (i = 0; i < MAX_TMUS; i++)
		{
			shaderstate.tmuflags[i] = ~0;
			BE_ApplyTMUState(i, 0);
		}

		/*force all state to change, thus setting a known state*/
		shaderstate.shaderbits = ~0;
		BE_ApplyShaderBits(0);
	}
}

static const char LIGHTPASS_SHADER[] = "\
{\n\
	program rtlight\n\
	{\n\
		map $diffuse\n\
		blendfunc add\n\
	}\n\
	{\n\
		map $normalmap\n\
	}\n\
	{\n\
		map $specular\n\
	}\n\
}";

void D3D9BE_Init(void)
{
	be_maxpasses = MAX_TMUS;
	memset(&shaderstate, 0, sizeof(shaderstate));
	shaderstate.curvertdecl = -1;

	FTable_Init();

	shaderstate.dynxyz_size = sizeof(vecV_t) * DYNVBUFFSIZE;
	shaderstate.dyncol_size = sizeof(byte_vec4_t) * DYNVBUFFSIZE;
	shaderstate.dynst_size = sizeof(vec2_t) * DYNVBUFFSIZE;
	shaderstate.dynidx_size = sizeof(index_t) * DYNIBUFFSIZE;

	D3D9BE_Reset(false);

	shaderstate.shader_rtlight = R_RegisterShader("rtlight", LIGHTPASS_SHADER);

	R_InitFlashblends();
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

	d3dcheck(IDirect3DIndexBuffer9_Lock(shaderstate.dynidx_buff, offset, (unsigned int)entries, dest, offset?D3DLOCK_NOOVERWRITE:D3DLOCK_DISCARD));
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
	int last;
	extern texid_t missing_texture;

	switch(pass->texgen)
	{
	default:
	case T_GEN_DIFFUSE:
		if (shaderstate.curtexnums->base.ptr)
			BindTexture(tu, shaderstate.curtexnums->base.ptr);
		else
			BindTexture(tu, missing_texture.ptr);
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

	if (tu == 0)
	{
		if (shaderstate.passsinglecolour)
		{
			IDirect3DDevice9_SetTextureStageState(pD3DDev9, tu, D3DTSS_CONSTANT, shaderstate.passcolour);
			last = D3DTA_CONSTANT;
		}
		else
		{
			last = D3DTA_DIFFUSE;
		}
	}
	else
		last = D3DTA_CURRENT;
	switch (pass->blendmode)
	{
	case PBM_DOTPRODUCT:
		IDirect3DDevice9_SetTextureStageState(pD3DDev9, tu, D3DTSS_COLORARG1, last);
		IDirect3DDevice9_SetTextureStageState(pD3DDev9, tu, D3DTSS_COLORARG2, D3DTA_TEXTURE);
		IDirect3DDevice9_SetTextureStageState(pD3DDev9, tu, D3DTSS_COLOROP, D3DTOP_DOTPRODUCT3);

//		IDirect3DDevice9_SetTextureStageState(pD3DDev9, tu, D3DTSS_ALPHAARG1, last);
		IDirect3DDevice9_SetTextureStageState(pD3DDev9, tu, D3DTSS_ALPHAARG2, D3DTA_TEXTURE);
		IDirect3DDevice9_SetTextureStageState(pD3DDev9, tu, D3DTSS_ALPHAOP, D3DTOP_SELECTARG2);
		break;
	case PBM_REPLACE:
		IDirect3DDevice9_SetTextureStageState(pD3DDev9, tu, D3DTSS_COLORARG1, last);
		IDirect3DDevice9_SetTextureStageState(pD3DDev9, tu, D3DTSS_COLORARG2, D3DTA_TEXTURE);
		IDirect3DDevice9_SetTextureStageState(pD3DDev9, tu, D3DTSS_COLOROP, D3DTOP_SELECTARG2);

		if (shaderstate.flags & (BEF_FORCETRANSPARENT | BEF_FORCEADDITIVE))
		{
			IDirect3DDevice9_SetTextureStageState(pD3DDev9, tu, D3DTSS_ALPHAARG1, last);
			IDirect3DDevice9_SetTextureStageState(pD3DDev9, tu, D3DTSS_ALPHAARG2, D3DTA_TEXTURE);
			IDirect3DDevice9_SetTextureStageState(pD3DDev9, tu, D3DTSS_ALPHAOP, D3DTOP_MODULATE);
		}
		else
		{
	//		IDirect3DDevice9_SetTextureStageState(pD3DDev9, tu, D3DTSS_ALPHAARG1, last);
			IDirect3DDevice9_SetTextureStageState(pD3DDev9, tu, D3DTSS_ALPHAARG2, D3DTA_TEXTURE);
			IDirect3DDevice9_SetTextureStageState(pD3DDev9, tu, D3DTSS_ALPHAOP, D3DTOP_SELECTARG2);
		}
		break;
	case PBM_ADD:
		if (tu == 0)
			goto forcemod;
		IDirect3DDevice9_SetTextureStageState(pD3DDev9, tu, D3DTSS_COLORARG1, last);
		IDirect3DDevice9_SetTextureStageState(pD3DDev9, tu, D3DTSS_COLORARG2, D3DTA_TEXTURE);
		IDirect3DDevice9_SetTextureStageState(pD3DDev9, tu, D3DTSS_COLOROP, D3DTOP_ADD);

		IDirect3DDevice9_SetTextureStageState(pD3DDev9, tu, D3DTSS_ALPHAARG1, last);
		IDirect3DDevice9_SetTextureStageState(pD3DDev9, tu, D3DTSS_ALPHAARG2, D3DTA_TEXTURE);
		IDirect3DDevice9_SetTextureStageState(pD3DDev9, tu, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);
		break;
	case PBM_DECAL:
		if (!tu)
			goto forcemod;
		IDirect3DDevice9_SetTextureStageState(pD3DDev9, tu, D3DTSS_COLORARG1, last);
		IDirect3DDevice9_SetTextureStageState(pD3DDev9, tu, D3DTSS_COLORARG2, D3DTA_TEXTURE);
		IDirect3DDevice9_SetTextureStageState(pD3DDev9, tu, D3DTSS_COLOROP, D3DTOP_BLENDTEXTUREALPHA);

//		IDirect3DDevice9_SetTextureStageState(pD3DDev9, tu, D3DTSS_ALPHAARG1, last);
		IDirect3DDevice9_SetTextureStageState(pD3DDev9, tu, D3DTSS_ALPHAARG2, D3DTA_TEXTURE);
		IDirect3DDevice9_SetTextureStageState(pD3DDev9, tu, D3DTSS_ALPHAOP, D3DTOP_SELECTARG2);
		break;
	case PBM_OVERBRIGHT:
		IDirect3DDevice9_SetTextureStageState(pD3DDev9, tu, D3DTSS_COLORARG1, last);
		IDirect3DDevice9_SetTextureStageState(pD3DDev9, tu, D3DTSS_COLORARG2, D3DTA_TEXTURE);
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
		IDirect3DDevice9_SetTextureStageState(pD3DDev9, tu, D3DTSS_ALPHAARG1, last);
		IDirect3DDevice9_SetTextureStageState(pD3DDev9, tu, D3DTSS_ALPHAARG2, D3DTA_TEXTURE);
		IDirect3DDevice9_SetTextureStageState(pD3DDev9, tu, D3DTSS_ALPHAOP, D3DTOP_MODULATE);
		break;
	default:
	case PBM_MODULATE:
	forcemod:
		IDirect3DDevice9_SetTextureStageState(pD3DDev9, tu, D3DTSS_COLORARG1, last);
		IDirect3DDevice9_SetTextureStageState(pD3DDev9, tu, D3DTSS_COLORARG2, D3DTA_TEXTURE);
		IDirect3DDevice9_SetTextureStageState(pD3DDev9, tu, D3DTSS_COLOROP, D3DTOP_MODULATE);

		IDirect3DDevice9_SetTextureStageState(pD3DDev9, tu, D3DTSS_ALPHAARG1, last);
		IDirect3DDevice9_SetTextureStageState(pD3DDev9, tu, D3DTSS_ALPHAARG2, D3DTA_TEXTURE);
		IDirect3DDevice9_SetTextureStageState(pD3DDev9, tu, D3DTSS_ALPHAOP, D3DTOP_MODULATE);
		break;
	}
}

static void colourgenbyte(const shaderpass_t *pass, int cnt, byte_vec4_t *srcb, vec4_t *srcf, byte_vec4_t *dst, const mesh_t *mesh)
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
	case RGB_GEN_VERTEX_LIGHTING:
	case RGB_GEN_VERTEX_EXACT:
		if (srcb)
		{
			while((cnt)--)
			{
				qbyte r, g, b;
				r=srcb[cnt][0];
				g=srcb[cnt][1];
				b=srcb[cnt][2];
				dst[cnt][0] = b;
				dst[cnt][1] = g;
				dst[cnt][2] = r;
			}
		}
		else if (srcf)
		{
			while((cnt)--)
			{
				int r, g, b;
				r=srcf[cnt][0]*255;
				g=srcf[cnt][1]*255;
				b=srcf[cnt][2]*255;
				dst[cnt][0] = bound(0, b, 255);
				dst[cnt][1] = bound(0, g, 255);
				dst[cnt][2] = bound(0, r, 255);
			}
		}
		else
			goto identity;
		break;
	case RGB_GEN_ONE_MINUS_VERTEX:
		if (srcb)
		{
			while((cnt)--)
			{
				qbyte r, g, b;
				r=255-srcb[cnt][0];
				g=255-srcb[cnt][1];
				b=255-srcb[cnt][2];
				dst[cnt][0] = b;
				dst[cnt][1] = g;
				dst[cnt][2] = r;
			}
		}
		else if (srcf)
		{
			while((cnt)--)
			{
				int r, g, b;
				r=255-srcf[cnt][0]*255;
				g=255-srcf[cnt][1]*255;
				b=255-srcf[cnt][2]*255;
				dst[cnt][0] = bound(0, b, 255);
				dst[cnt][1] = bound(0, g, 255);
				dst[cnt][2] = bound(0, r, 255);
			}
		}
		else
			goto identity;
		break;
	case RGB_GEN_IDENTITY_LIGHTING:
		//compensate for overbrights
		block = D3DCOLOR_RGBA(255, 255, 255, 255); //shaderstate.identitylighting
		while((cnt)--)
		{
			((D3DCOLOR*)dst)[cnt] = block;
		}
		break;
	default:
	identity:
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
			R_LightArraysByte_BGR(shaderstate.curentity , mesh->xyz_array, dst, cnt, mesh->normals_array);
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
#ifdef warningmsg
#pragma warningmsg("fix 24bit player colours")
#endif
		block = D3DCOLOR_RGBA(255, 255, 255, 255);
		while((cnt)--)
		{
			((D3DCOLOR*)dst)[cnt] = block;
		}
	//	Con_Printf("RGB_GEN %i not supported\n", pass->rgbgen);
		break;
	}
}

static void alphagenbyte(const shaderpass_t *pass, int cnt, byte_vec4_t *srcb, vec4_t *srcf, byte_vec4_t *dst, const mesh_t *mesh)
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
		if (srcb)
		{
			while(cnt--)
			{
				dst[cnt][3] = srcb[cnt][3];
			}
		}
		else if (srcf)
		{
			while(cnt--)
			{
				dst[cnt][3] = bound(0, srcf[cnt][3]*255, 255);
			}
		}
		else
		{
			while(cnt--)
			{
				dst[cnt][3] = 255;
			}
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

			if (!Matrix3_Compare(shaderstate.curentity->axis, (void *)axisDefault))
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
		colourgenbyte(pass, 1, (byte_vec4_t*)&shaderstate.passcolour, NULL, (byte_vec4_t*)&shaderstate.passcolour, m);
		alphagenbyte(pass, 1, (byte_vec4_t*)&shaderstate.passcolour, NULL, (byte_vec4_t*)&shaderstate.passcolour, m);
		/*FIXME: just because there's no rgba set, there's no reason to assume it should be a single colour (unshaded ents)*/
		d3dcheck(IDirect3DDevice9_SetStreamSource(pD3DDev9, STRM_COL, NULL, 0, 0));
	}
	else
	{
		shaderstate.passsinglecolour = false;

		ret |= D3D_VDEC_COL4B;
		if (shaderstate.batchvbo && (m->colors4f_array &&
						((pass->rgbgen == RGB_GEN_VERTEX_LIGHTING) ||
						(pass->rgbgen == RGB_GEN_VERTEX_EXACT) ||
						(pass->rgbgen == RGB_GEN_ONE_MINUS_VERTEX)) &&
						(pass->alphagen == ALPHA_GEN_VERTEX)))
		{
			d3dcheck(IDirect3DDevice9_SetStreamSource(pD3DDev9, STRM_COL, shaderstate.batchvbo->colours.d3d.buff, shaderstate.batchvbo->colours.d3d.offs, sizeof(vbovdata_t)));
		}
		else
		{
			allocvertexbuffer(shaderstate.dyncol_buff, shaderstate.dyncol_size, &shaderstate.dyncol_offs, (void**)&map, vertcount*sizeof(D3DCOLOR));
			for (vertcount = 0, mno = 0; mno < shaderstate.nummeshes; mno++)
			{
				m = shaderstate.meshlist[mno];
				colourgenbyte(pass, m->numvertexes, m->colors4b_array, m->colors4f_array, (byte_vec4_t*)map, m);
				alphagenbyte(pass, m->numvertexes, m->colors4b_array, m->colors4f_array, (byte_vec4_t*)map, m);
				map += m->numvertexes*4;
				vertcount += m->numvertexes;
			}
			d3dcheck(IDirect3DVertexBuffer9_Unlock(shaderstate.dyncol_buff));
			d3dcheck(IDirect3DDevice9_SetStreamSource(pD3DDev9, STRM_COL, shaderstate.dyncol_buff, shaderstate.dyncol_offs - vertcount*sizeof(D3DCOLOR), sizeof(D3DCOLOR)));
		}
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
	unsigned int mno;
	// unsigned int fvertex = 0; //unused variable
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
			memcpy(dest, src, sizeof(vec2_t)*mesh->numvertexes);
		}
		dest += mesh->numvertexes*2;
	}
}

//end texture coords
/*******************************************************************************************************************/

static void deformgen(const deformv_t *deformv, int cnt, vecV_t *src, vecV_t *dst, const mesh_t *mesh)
{
	float *table;
	int j, k;
	float args[4];
	float deflect;
	switch (deformv->type)
	{
	default:
	case DEFORMV_NONE:
		if (src != dst)
			memcpy(dst, src, sizeof(*src)*cnt);
		break;

	case DEFORMV_WAVE:
		if (!mesh->normals_array)
		{
			if (src != dst)
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
		if (src != dst)
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
				Matrix3_Multiply_Vec3((void *)result, tv, quad[j]);
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
		if (shaderstate.batchvbo && pass[passno].tcgen == TC_GEN_BASE && !pass[passno].numtcmods)
			d3dcheck(IDirect3DDevice9_SetStreamSource(pD3DDev9, STRM_TC0+tmu, shaderstate.batchvbo->texcoord.d3d.buff, shaderstate.batchvbo->texcoord.d3d.offs, sizeof(vbovdata_t)));
		else if (shaderstate.batchvbo && pass[passno].tcgen == TC_GEN_LIGHTMAP && !pass[passno].numtcmods)
			d3dcheck(IDirect3DDevice9_SetStreamSource(pD3DDev9, STRM_TC0+tmu, shaderstate.batchvbo->lmcoord[0].d3d.buff, shaderstate.batchvbo->lmcoord[0].d3d.offs, sizeof(vbovdata_t)));
		else
		{
			allocvertexbuffer(shaderstate.dynst_buff[tmu], shaderstate.dynst_size, &shaderstate.dynst_offs[tmu], &map, vertcount*sizeof(vec2_t));
			GenerateTCMods(pass+passno, map);
			d3dcheck(IDirect3DVertexBuffer9_Unlock(shaderstate.dynst_buff[tmu]));
			d3dcheck(IDirect3DDevice9_SetStreamSource(pD3DDev9, STRM_TC0+tmu, shaderstate.dynst_buff[tmu], shaderstate.dynst_offs[tmu] - vertcount*sizeof(vec2_t), sizeof(vec2_t)));
		}
		tmu++;
	}
	/*deactivate any extras*/
	for (; tmu < shaderstate.lastpasscount; )
	{
		d3dcheck(IDirect3DDevice9_SetStreamSource(pD3DDev9, STRM_TC0+tmu, NULL, 0, 0));
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

	BE_ApplyShaderBits(pass->shaderbits);
	return true;
}

static void BE_SubmitMeshChain(int idxfirst)
{
	int startv, starti, endv, endi;
	int m;
	mesh_t *mesh;

//	if (shaderstate.batchvbo)
//	IDirect3DDevice9_DrawIndexedPrimitive(pD3DDev9, D3DPT_TRIANGLELIST, 0, 0, shaderstate.batchvbo->vertcount, idxfirst, shaderstate.batchvbo->indexcount/3);

	for (m = 0, mesh = shaderstate.meshlist[0]; m < shaderstate.nummeshes; )
	{
		startv = mesh->vbofirstvert;
		starti = mesh->vbofirstelement;

		endv = startv+mesh->numvertexes;
		endi = starti+mesh->numindexes;

		//find consecutive surfaces
		for (++m; m < shaderstate.nummeshes; m++)
		{
			mesh = shaderstate.meshlist[m];
			if (endi == mesh->vbofirstelement)
			{
				endv = mesh->vbofirstvert+mesh->numvertexes;
				endi = mesh->vbofirstelement+mesh->numindexes;
			}
			else
			{
				break;
			}
		}

		IDirect3DDevice9_DrawIndexedPrimitive(pD3DDev9, D3DPT_TRIANGLELIST, 0, startv, endv - startv, idxfirst + starti, (endi-starti)/3);
		RQuantAdd(RQUANT_DRAWS, 1);
 	}
}

static void BE_ApplyUniforms(program_t *prog, int permu)
{
	int i;
	IDirect3DDevice9_SetVertexShader(pD3DDev9, prog->handle[permu].hlsl.vert);
	IDirect3DDevice9_SetPixelShader(pD3DDev9, prog->handle[permu].hlsl.frag);
	for (i = 0; i < prog->numparams; i++)
	{
		switch (prog->parm[i].type)
		{
		case SP_M_PROJECTION:
			IDirect3DDevice9_SetVertexShaderConstantF(pD3DDev9, prog->parm[i].handle[permu], d3d_trueprojection, 4);
			break;
		case SP_M_VIEW:
			IDirect3DDevice9_SetVertexShaderConstantF(pD3DDev9, prog->parm[i].handle[permu], r_refdef.m_view, 4);
			break;
//		case SP_M_MODEL:
//			IDirect3DDevice9_SetVertexShaderConstantF(pD3DDev9, prog->parm[i].handle[permu], r_refdef.m_view, 4);
//			break;

		case SP_V_EYEPOS:
			IDirect3DDevice9_SetPixelShaderConstantF(pD3DDev9, prog->parm[i].handle[permu], r_origin, 1);
			break;
		case SP_E_EYEPOS:
			{
				vec4_t t2;
				float m16[16];
				Matrix4x4_CM_ModelMatrixFromAxis(m16, shaderstate.curentity->axis[0], shaderstate.curentity->axis[1], shaderstate.curentity->axis[2], shaderstate.curentity->origin);
				Matrix4x4_CM_Transform3(m16, r_origin, t2);
				IDirect3DDevice9_SetPixelShaderConstantF(pD3DDev9, prog->parm[i].handle[permu], t2, 1);
			}
			break;
		case SP_E_TIME:
			{
				vec4_t t1 = {shaderstate.curtime};
				IDirect3DDevice9_SetPixelShaderConstantF(pD3DDev9, prog->parm[i].handle[permu], t1, 1);
			}
			break;
		case SP_M_MODELVIEWPROJECTION:
			{
				float mv[16], mvp[16];
				Matrix4_Multiply(r_refdef.m_view, shaderstate.m_model, mv);
				Matrix4_Multiply(d3d_trueprojection, mv, mvp);
				IDirect3DDevice9_SetVertexShaderConstantF(pD3DDev9, prog->parm[i].handle[permu], mvp, 4);
			}
			break;

		case SP_LIGHTPOSITION:
			{
				/*light position in model space*/
				float inv[16];
				vec3_t t2;
				qboolean Matrix4_Invert(const float *m, float *out);

				Matrix4_Invert(shaderstate.m_model, inv);
				Matrix4x4_CM_Transform3(inv, shaderstate.curdlight->origin, t2);
				IDirect3DDevice9_SetVertexShaderConstantF(pD3DDev9, prog->parm[i].handle[permu], t2, 3);
				break;
			}

		case SP_LIGHTRADIUS:
			IDirect3DDevice9_SetPixelShaderConstantF(pD3DDev9, prog->parm[i].handle[permu], &shaderstate.curdlight->radius, 1);
			break;
		case SP_LIGHTCOLOUR:
			IDirect3DDevice9_SetPixelShaderConstantF(pD3DDev9, prog->parm[i].handle[permu], shaderstate.curdlight_colours, 3);
			break;

		case SP_E_COLOURS:
		case SP_E_COLOURSIDENT:
		case SP_E_TOPCOLOURS:
		case SP_E_BOTTOMCOLOURS:
		case SP_E_L_DIR:
		case SP_E_L_MUL:
		case SP_E_L_AMBIENT:

		case SP_M_ENTBONES:
		case SP_M_MODEL:
		case SP_M_MODELVIEW:

		case SP_RENDERTEXTURESCALE:

		case SP_FIRSTIMMEDIATE:
		case SP_CONSTI:
		case SP_CONSTF:
		case SP_CVARI:
		case SP_CVARF:
		case SP_CVAR3F:
		case SP_TEXTURE:
			Con_Printf("shader property %i not implemented\n", prog->parm[i].type);
			break;
		}
	}
}

static void BE_RenderMeshProgram(shader_t *s, unsigned int vertcount, unsigned int idxfirst, unsigned int idxcount)
{
	int vdec = D3D_VDEC_ST0|D3D_VDEC_NORM;
	int passno;
	int perm = 0;

	program_t *p = s->prog;

	if (TEXVALID(shaderstate.curtexnums->bump) && p->handle[perm|PERMUTATION_BUMPMAP].hlsl.vert)
		perm |= PERMUTATION_BUMPMAP;
	if (TEXVALID(shaderstate.curtexnums->specular) && p->handle[perm|PERMUTATION_SPECULAR].hlsl.vert)
		perm |= PERMUTATION_SPECULAR;
	if (TEXVALID(shaderstate.curtexnums->fullbright) && p->handle[perm|PERMUTATION_FULLBRIGHT].hlsl.vert)
		perm |= PERMUTATION_FULLBRIGHT;
	if (p->handle[perm|PERMUTATION_UPPERLOWER].hlsl.vert && (TEXVALID(shaderstate.curtexnums->upperoverlay) || TEXVALID(shaderstate.curtexnums->loweroverlay)))
		perm |= PERMUTATION_UPPERLOWER;
	if (r_refdef.gfog_rgbd[3] && p->handle[perm|PERMUTATION_FOG].hlsl.vert)
		perm |= PERMUTATION_FOG;
//	if (r_glsl_offsetmapping.ival && TEXVALID(shaderstate.curtexnums->bump) && p->handle[perm|PERMUTATION_OFFSET.hlsl.vert)
//		perm |= PERMUTATION_OFFSET;

	BE_ApplyUniforms(p, perm);


	BE_ApplyShaderBits(s->passes->shaderbits);

	/*activate tmus*/
	for (passno = 0; passno < s->numpasses; passno++)
	{
		SelectPassTexture(passno, s->passes+passno);
	}
	/*deactivate any extras*/
	for (; passno < shaderstate.lastpasscount; passno++)
	{
		BindTexture(passno, NULL);
		d3dcheck(IDirect3DDevice9_SetTextureStageState(pD3DDev9, passno, D3DTSS_COLOROP, D3DTOP_DISABLE));
	}
	shaderstate.lastpasscount = passno;

	/*colours*/
	if (vdec & D3D_VDEC_COL4B)
	{
		if (shaderstate.batchvbo)
			d3dcheck(IDirect3DDevice9_SetStreamSource(pD3DDev9, STRM_COL, shaderstate.batchvbo->colours.d3d.buff, shaderstate.batchvbo->colours.d3d.offs, sizeof(vbovdata_t)));
		else
		{
			int mno,v;
			void *map;
			mesh_t *m;
			allocvertexbuffer(shaderstate.dynst_buff[0], shaderstate.dynst_size, &shaderstate.dynst_offs[0], &map, vertcount*sizeof(byte_vec4_t));
			for (mno = 0, vertcount = 0; mno < shaderstate.nummeshes; mno++)
			{
				byte_vec4_t *dest = (byte_vec4_t*)((char*)map+vertcount*sizeof(byte_vec4_t));
				m = shaderstate.meshlist[mno];
				if (m->colors4f_array)
				{
					for (v = 0; v < m->numvertexes; v++)
					{
						dest[v][0] = bound(0, m->colors4f_array[v][0] * 255, 255);
						dest[v][1] = bound(0, m->colors4f_array[v][1] * 255, 255);
						dest[v][2] = bound(0, m->colors4f_array[v][2] * 255, 255);
						dest[v][3] = bound(0, m->colors4f_array[v][3] * 255, 255);
					}
				}
				else if (m->colors4b_array)
					memcpy(dest, m->colors4b_array, m->numvertexes*sizeof(byte_vec4_t));
				else
					memset(dest, 0, m->numvertexes*sizeof(byte_vec4_t));
				vertcount += m->numvertexes;
			}
			d3dcheck(IDirect3DVertexBuffer9_Unlock(shaderstate.dynst_buff[0]));
			d3dcheck(IDirect3DDevice9_SetStreamSource(pD3DDev9, STRM_TC0, shaderstate.dynst_buff[0], shaderstate.dynst_offs[0] - vertcount*sizeof(byte_vec4_t), sizeof(byte_vec4_t)));
		}
	}

	/*texture coords*/
	if (vdec & D3D_VDEC_ST0)
	{
		if (shaderstate.batchvbo)
			d3dcheck(IDirect3DDevice9_SetStreamSource(pD3DDev9, STRM_TC0, shaderstate.batchvbo->texcoord.d3d.buff, shaderstate.batchvbo->texcoord.d3d.offs, sizeof(vbovdata_t)));
		else
		{
			int mno;
			void *map;
			mesh_t *m;
			allocvertexbuffer(shaderstate.dynst_buff[0], shaderstate.dynst_size, &shaderstate.dynst_offs[0], &map, vertcount*sizeof(vec2_t));
			for (mno = 0, vertcount = 0; mno < shaderstate.nummeshes; mno++)
			{
				vec2_t *dest = (vec2_t*)((char*)map+vertcount*sizeof(vec2_t));
				m = shaderstate.meshlist[mno];
				memcpy(dest, m->st_array, m->numvertexes*sizeof(vec2_t));
				vertcount += m->numvertexes;
			}
			d3dcheck(IDirect3DVertexBuffer9_Unlock(shaderstate.dynst_buff[0]));
			d3dcheck(IDirect3DDevice9_SetStreamSource(pD3DDev9, STRM_TC0, shaderstate.dynst_buff[0], shaderstate.dynst_offs[0] - vertcount*sizeof(vec2_t), sizeof(vec2_t)));
		}
	}
	/*lm coords*/
	if (vdec & D3D_VDEC_ST1)
	{
		if (shaderstate.batchvbo)
			d3dcheck(IDirect3DDevice9_SetStreamSource(pD3DDev9, STRM_TC1, shaderstate.batchvbo->lmcoord[0].d3d.buff, shaderstate.batchvbo->lmcoord[0].d3d.offs, sizeof(vbovdata_t)));
		else
		{
			int mno;
			void *map;
			mesh_t *m;
			allocvertexbuffer(shaderstate.dynst_buff[1], shaderstate.dynst_size, &shaderstate.dynst_offs[1], &map, vertcount*sizeof(vec2_t));
			for (mno = 0, vertcount = 0; mno < shaderstate.nummeshes; mno++)
			{
				vec2_t *dest = (vec2_t*)((char*)map+vertcount*sizeof(vec2_t));
				m = shaderstate.meshlist[mno];
				memcpy(dest, m->lmst_array, m->numvertexes*sizeof(vec2_t));
				vertcount += m->numvertexes;
			}
			d3dcheck(IDirect3DVertexBuffer9_Unlock(shaderstate.dynst_buff[1]));
			d3dcheck(IDirect3DDevice9_SetStreamSource(pD3DDev9, STRM_TC1, shaderstate.dynst_buff[1], shaderstate.dynst_offs[1] - vertcount*sizeof(vec2_t), sizeof(vec2_t)));
		}
	}

	/*normals/tangents/bitangents*/
	if (vdec & D3D_VDEC_NORM)
	{
		if (shaderstate.batchvbo)
		{
			d3dcheck(IDirect3DDevice9_SetStreamSource(pD3DDev9, STRM_NORM, shaderstate.batchvbo->normals.d3d.buff, shaderstate.batchvbo->normals.d3d.offs, sizeof(vbovdata_t)));
			d3dcheck(IDirect3DDevice9_SetStreamSource(pD3DDev9, STRM_NORMS, shaderstate.batchvbo->svector.d3d.buff, shaderstate.batchvbo->svector.d3d.offs, sizeof(vbovdata_t)));
			d3dcheck(IDirect3DDevice9_SetStreamSource(pD3DDev9, STRM_NORMT, shaderstate.batchvbo->tvector.d3d.buff, shaderstate.batchvbo->tvector.d3d.offs, sizeof(vbovdata_t)));
		}
		else
		{
		/*FIXME*/
		vdec &= ~D3D_VDEC_NORM;
		}
	}

	/*bone weights+indexes*/
	if (vdec & D3D_VDEC_SKEL)
	{
		/*FIXME*/
		vdec &= ~D3D_VDEC_NORM;
	}

	if (vdec != shaderstate.curvertdecl)
	{
		shaderstate.curvertdecl = vdec;
		d3dcheck(IDirect3DDevice9_SetVertexDeclaration(pD3DDev9, vertexdecls[shaderstate.curvertdecl]));
	}

//	IDirect3DDevice9_SetVertexShaderConstantF(pD3DDev9,
	BE_SubmitMeshChain(idxfirst);

	IDirect3DDevice9_SetVertexShader(pD3DDev9, NULL);
	IDirect3DDevice9_SetPixelShader(pD3DDev9, NULL);
}

void D3D9BE_Cull(unsigned int cullflags)
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
	extern cvar_t r_polygonoffset_submodel_factor;
	float pushdepth;
//	float pushfactor;

	D3D9BE_Cull(shaderstate.curshader->flags & (SHADER_CULL_FRONT | SHADER_CULL_BACK));
	pushdepth = (shaderstate.curshader->polyoffset.factor + ((shaderstate.flags & BEF_PUSHDEPTH)?r_polygonoffset_submodel_factor.value:0))/0xffff;
	if (pushdepth != shaderstate.depthbias)
	{
		shaderstate.depthbias = pushdepth;
		IDirect3DDevice9_SetRenderState(pD3DDev9, D3DRS_DEPTHBIAS, *(DWORD*)&shaderstate.depthbias);
	}
//	pushdepth = shaderstate.curshader->polyoffset.unit/-1;// + ((shaderstate.flags & BEF_PUSHDEPTH)?8:0);
//	pushfactor = shaderstate.curshader->polyoffset.factor/-1;
//	if (pushfactor != shaderstate.depthfactor)
//	{
//		shaderstate.depthfactor = pushfactor;
//		IDirect3DDevice9_SetRenderState(pD3DDev9, D3DRS_SLOPESCALEDEPTHBIAS, *(DWORD*)&shaderstate.depthfactor);
//	}

	if (shaderstate.batchvbo)
	{
		vertcount = shaderstate.batchvbo->vertcount;
		idxcount = shaderstate.batchvbo->indexcount;
	}
	else
	{
		for (mno = 0, vertcount = 0, idxcount = 0; mno < shaderstate.nummeshes; mno++)
		{
			m = shaderstate.meshlist[mno];
			vertcount += m->numvertexes;
			idxcount += m->numindexes;
		}
	}

	/*vertex buffers are common to all passes*/
	if (shaderstate.batchvbo && !shaderstate.curshader->numdeforms)
	{
		d3dcheck(IDirect3DDevice9_SetStreamSource(pD3DDev9, STRM_VERT, shaderstate.batchvbo->coord.d3d.buff, shaderstate.batchvbo->coord.d3d.offs, sizeof(vbovdata_t)));
	}
	else
	{
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
		d3dcheck(IDirect3DDevice9_SetStreamSource(pD3DDev9, STRM_VERT, shaderstate.dynxyz_buff, shaderstate.dynxyz_offs - vertcount*sizeof(vecV_t), sizeof(vecV_t)));
	}

	/*so are index buffers*/
	if (shaderstate.batchvbo)
	{
		d3dcheck(IDirect3DDevice9_SetIndices(pD3DDev9, shaderstate.batchvbo->indicies.d3d.buff));
		idxfirst = 0;
	}
	else
	{
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
	}

	switch (shaderstate.mode)
	{
	case BEM_LIGHT:
		if (shaderstate.shader_rtlight->prog)
			BE_RenderMeshProgram(shaderstate.shader_rtlight, vertcount, idxfirst, idxcount);
		break;
	case BEM_DEPTHONLY:
		shaderstate.lastpasscount = 0;
		i = 0;
		if (i != shaderstate.curvertdecl)
		{
			shaderstate.curvertdecl = i;
			d3dcheck(IDirect3DDevice9_SetVertexDeclaration(pD3DDev9, vertexdecls[shaderstate.curvertdecl]));
		}
		IDirect3DDevice9_SetRenderState(pD3DDev9, D3DRS_COLORWRITEENABLE, 0);
		/*deactivate any extras*/
		for (passno = 0; passno < shaderstate.lastpasscount; )
		{
			d3dcheck(IDirect3DDevice9_SetStreamSource(pD3DDev9, STRM_TC0+passno, NULL, 0, 0));
			BindTexture(passno, NULL);
			d3dcheck(IDirect3DDevice9_SetTextureStageState(pD3DDev9, passno, D3DTSS_COLOROP, D3DTOP_DISABLE));
			passno++;
		}
		shaderstate.lastpasscount = 0;
		BE_SubmitMeshChain(idxfirst);
		IDirect3DDevice9_SetRenderState(pD3DDev9, D3DRS_COLORWRITEENABLE, D3DCOLORWRITEENABLE_RED|D3DCOLORWRITEENABLE_GREEN|D3DCOLORWRITEENABLE_BLUE|D3DCOLORWRITEENABLE_ALPHA);
		break;
	default:
	case BEM_STANDARD:
		if (shaderstate.curshader->prog)
		{
			BE_RenderMeshProgram(shaderstate.curshader, vertcount, idxfirst, idxcount);
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
				BE_SubmitMeshChain(idxfirst);
//				d3dcheck(IDirect3DDevice9_DrawIndexedPrimitive(pD3DDev9, D3DPT_TRIANGLELIST, 0, 0, vertcount, idxfirst, idxcount/3));
			}
		}
		break;
	}
}

void D3D9BE_SelectMode(backendmode_t mode)
{
	shaderstate.mode = mode;

	if (mode == BEM_STENCIL)
		BE_ApplyShaderBits(SBITS_MASK_BITS);
}

void D3D9BE_SelectDLight(dlight_t *dl, vec3_t colour)
{
	shaderstate.curdlight = dl;
	VectorCopy(colour, shaderstate.curdlight_colours);
}

void D3D9BE_SelectEntity(entity_t *ent)
{
	shaderstate.curentity = ent;
	BE_RotateForEntity(ent, ent->model);
}

#if 1
static void D3D9BE_GenBatchVBOs(vbo_t **vbochain, batch_t *firstbatch, batch_t *stopbatch)
{
	int maxvboelements;
	int maxvboverts;
	int vert = 0, idx = 0;
	batch_t *batch;
	vbo_t *vbo;
	int i, j;
	mesh_t *m;
	IDirect3DVertexBuffer9 *vbuff;
	IDirect3DIndexBuffer9 *ebuff;
	index_t *vboedata;
	vbovdata_t *vbovdata;

	vbo = Z_Malloc(sizeof(*vbo));

	maxvboverts = 0;
	maxvboelements = 0;
	for(batch = firstbatch; batch != stopbatch; batch = batch->next)
	{
		for (i=0 ; i<batch->maxmeshes ; i++)
		{
			m = batch->mesh[i];
			maxvboelements += m->numindexes;
			maxvboverts += m->numvertexes;
		}
	}

	IDirect3DDevice9_CreateIndexBuffer(pD3DDev9, sizeof(index_t) * maxvboelements, 0, D3DFMT_QINDEX, D3DPOOL_MANAGED, &ebuff, NULL);
	IDirect3DDevice9_CreateVertexBuffer(pD3DDev9, sizeof(*vbovdata) * maxvboverts, D3DUSAGE_WRITEONLY, 0, D3DPOOL_MANAGED, &vbuff, NULL);

	vbovdata = NULL;
	vbo->coord.d3d.buff = vbuff;
	vbo->coord.d3d.offs = (quintptr_t)&vbovdata->coord;
	vbo->texcoord.d3d.buff = vbuff;
	vbo->texcoord.d3d.offs = (quintptr_t)&vbovdata->tex;
	vbo->lmcoord[0].d3d.buff = vbuff;
	vbo->lmcoord[0].d3d.offs = (quintptr_t)&vbovdata->lm;
	vbo->normals.d3d.buff = vbuff;
	vbo->normals.d3d.offs = (quintptr_t)&vbovdata->ndir;
	vbo->svector.d3d.buff = vbuff;
	vbo->svector.d3d.offs = (quintptr_t)&vbovdata->sdir;
	vbo->tvector.d3d.buff = vbuff;
	vbo->tvector.d3d.offs = (quintptr_t)&vbovdata->tdir;
	vbo->colours.d3d.buff = vbuff;
	vbo->colours.d3d.offs = (quintptr_t)&vbovdata->colorsb;
	vbo->indicies.d3d.buff = ebuff;
	vbo->indicies.d3d.offs = 0;

	IDirect3DIndexBuffer9_Lock(ebuff, 0, sizeof(index_t) * maxvboelements, &vboedata, D3DLOCK_DISCARD);
	IDirect3DVertexBuffer9_Lock(vbuff, 0, sizeof(*vbovdata) * maxvboverts, &vbovdata, D3DLOCK_DISCARD);

	for(batch = firstbatch; batch != stopbatch; batch = batch->next)
	{
		batch->vbo = vbo;
		for (j=0 ; j<batch->maxmeshes ; j++)
		{
			m = batch->mesh[j];
			m->vbofirstvert = vert;
			for (i = 0; i < m->numvertexes; i++)
			{
				VectorCopy(m->xyz_array[i],			vbovdata->coord);
				vbovdata->coord[3] = 1;
				Vector2Copy(m->st_array[i],			vbovdata->tex);
				Vector2Copy(m->lmst_array[0][i],		vbovdata->lm);
				VectorCopy(m->normals_array[i],		vbovdata->ndir);
				VectorCopy(m->snormals_array[i],	vbovdata->sdir);
				VectorCopy(m->tnormals_array[i],	vbovdata->tdir);
				Vector4Scale(m->colors4f_array[i],	255, vbovdata->colorsb);

				vbovdata++;
			}

			m->vbofirstelement = idx;
			for (i = 0; i < m->numindexes; i++)
			{
				*vboedata++ = vert + m->indexes[i];
			}
			idx += m->numindexes;
			vert += m->numvertexes;
		}
	}

	IDirect3DIndexBuffer9_Unlock(ebuff);
	IDirect3DVertexBuffer9_Unlock(vbuff);

	vbo->next = *vbochain;
	*vbochain = vbo;
}

void D3D9BE_GenBrushModelVBO(model_t *mod)
{
	unsigned int vcount;


	batch_t *batch, *fbatch;
	int sortid;
	int i;

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
				D3D9BE_GenBatchVBOs(&mod->vbos, fbatch, batch);
				fbatch = batch;
				vcount = 0;
			}

			for (i = 0; i < batch->maxmeshes; i++)
				vcount += batch->mesh[i]->numvertexes;
		}

		D3D9BE_GenBatchVBOs(&mod->vbos, fbatch, batch);
	}
}
#else
/*Generates an optimised vbo for each of the given model's textures*/
void D3D9BE_GenBrushModelVBO(model_t *mod)
{
#if 1
	unsigned int maxvboverts;
	unsigned int maxvboelements;

	unsigned int t;
	unsigned int i;
	unsigned int v;
	unsigned int vcount, ecount;
	unsigned int pervertsize;	//erm, that name wasn't intentional
	unsigned int meshes;

	vbo_t *vbo;
	index_t *vboedata;
	mesh_t *m;
	char *vbovdata;
	IDirect3DVertexBuffer9 *vbuff;
	IDirect3DIndexBuffer9 *ebuff;

	vecV_t *coord;
	vec2_t *texcoord;
	vec2_t *lmcoord;
	vec3_t *normals;
	vec3_t *svector;
	vec3_t *tvector;
	byte_vec4_t *colours;

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
		if (maxvboverts > (1u<<(sizeof(index_t)*8))-1)
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

		IDirect3DDevice9_CreateIndexBuffer(pD3DDev9, sizeof(index_t) * maxvboelements, 0, D3DFMT_QINDEX, D3DPOOL_MANAGED, &ebuff, NULL);
		IDirect3DDevice9_CreateVertexBuffer(pD3DDev9, pervertsize * maxvboverts, D3DUSAGE_WRITEONLY, 0, D3DPOOL_MANAGED, &vbuff, NULL);

		IDirect3DIndexBuffer9_Lock(ebuff, 0, sizeof(index_t) * maxvboelements, &vboedata, D3DLOCK_DISCARD);
		IDirect3DVertexBuffer9_Lock(vbuff, 0, pervertsize * maxvboverts, &vbovdata, D3DLOCK_DISCARD);

		vbo->vertdata = vbuff;
		vbo->indexcount = maxvboelements;
		vbo->vertcount = maxvboverts;

		vbo->coord.d3d.buff = vbuff;
		vbo->coord.d3d.offs = 0;
		vbo->texcoord.d3d.buff = vbuff;
		vbo->texcoord.d3d.offs = vbo->coord.d3d.offs+maxvboverts*sizeof(vecV_t);
		vbo->lmcoord.d3d.buff = vbuff;
		vbo->lmcoord.d3d.offs = vbo->texcoord.d3d.offs+maxvboverts*sizeof(vec2_t);
		vbo->normals.d3d.buff = vbuff;
		vbo->normals.d3d.offs = vbo->lmcoord.d3d.offs+maxvboverts*sizeof(vec2_t);
		vbo->svector.d3d.buff = vbuff;
		vbo->svector.d3d.offs = vbo->normals.d3d.offs+maxvboverts*sizeof(vec3_t);
		vbo->tvector.d3d.buff = vbuff;
		vbo->tvector.d3d.offs = vbo->svector.d3d.offs+maxvboverts*sizeof(vec3_t);
		vbo->colours.d3d.buff = vbuff;
		vbo->colours.d3d.offs = vbo->tvector.d3d.offs+maxvboverts*sizeof(vec3_t);
		vbo->indicies.d3d.buff = ebuff;
		vbo->indicies.d3d.offs = 0;

		coord = (void*)(vbovdata + vbo->coord.d3d.offs);
		texcoord = (void*)(vbovdata + vbo->texcoord.d3d.offs);
		lmcoord = (void*)(vbovdata + vbo->lmcoord.d3d.offs);
		normals = (void*)(vbovdata + vbo->normals.d3d.offs);
		svector = (void*)(vbovdata + vbo->svector.d3d.offs);
		tvector = (void*)(vbovdata + vbo->tvector.d3d.offs);
		colours = (void*)(vbovdata + vbo->colours.d3d.offs);

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
				vboedata[ecount++] = vcount + m->indexes[v];
			for (v = 0; v < m->numvertexes; v++)
			{
				coord[vcount+v][0] = m->xyz_array[v][0];
				coord[vcount+v][1] = m->xyz_array[v][1];
				coord[vcount+v][2] = m->xyz_array[v][2];
				coord[vcount+v][3] = 1;
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
				if (m->colors4b_array)
				{
					colours[vcount+v][0] = m->colors4b_array[v][0];
					colours[vcount+v][1] = m->colors4b_array[v][1];
					colours[vcount+v][2] = m->colors4b_array[v][2];
					colours[vcount+v][3] = m->colors4b_array[v][3];
				}
				if (m->colors4f_array)
				{
					colours[vcount+v][0] = m->colors4f_array[v][0] * 255;
					colours[vcount+v][1] = m->colors4f_array[v][1] * 255;
					colours[vcount+v][2] = m->colors4f_array[v][2] * 255;
					colours[vcount+v][3] = m->colors4f_array[v][3] * 255;
				}
			}
			vcount += v;
		}

		IDirect3DIndexBuffer9_Unlock(ebuff);
		IDirect3DVertexBuffer9_Unlock(vbuff);
	}
#endif
}
#endif
/*Wipes a vbo*/
void D3D9BE_ClearVBO(vbo_t *vbo)
{
	IDirect3DVertexBuffer9 *vbuff = vbo->coord.d3d.buff;
	IDirect3DIndexBuffer9 *ebuff = vbo->indicies.d3d.buff;
	if (vbuff)
		IDirect3DVertexBuffer9_Release(vbuff);
	if (ebuff)
		IDirect3DIndexBuffer9_Release(ebuff);
	vbo->coord.d3d.buff = NULL;
	vbo->indicies.d3d.buff = NULL;

	free(vbo);
}

/*upload all lightmaps at the start to reduce lags*/
static void BE_UploadLightmaps(qboolean force)
{
	int i;
	lightmapinfo_t *lm;

	for (i = 0; i < numlightmaps; i++)
	{
		lm = lightmap[i];
		if (!lm)
			continue;

		if (force)
		{
			lm->rectchange.l = 0;
			lm->rectchange.t = 0;
			lm->rectchange.w = lm->width;
			lm->rectchange.h = lm->height;
		}

		if (lightmap[i]->modified)
		{
			IDirect3DTexture9 *tex = lm->lightmap_texture.ptr;
			D3DLOCKED_RECT lock;
			RECT rect;
			glRect_t *theRect = &lm->rectchange;
			int r;
			if (!tex)
			{
				lm->lightmap_texture = R_AllocNewTexture("***lightmap***", lm->width, lm->height, 0);
				tex = lm->lightmap_texture.ptr;
				if (!tex)
					continue;
			}
			
			lm->modified = 0;
			rect.left = theRect->l;
			rect.right = theRect->l + theRect->w;
			rect.top = theRect->t;
			rect.bottom = theRect->t + theRect->h;

			IDirect3DTexture9_LockRect(tex, 0, &lock, &rect, 0);
			for (r = 0; r < lightmap[i]->rectchange.h; r++)
			{
				memcpy((char*)lock.pBits + r*lock.Pitch, lightmap[i]->lightmaps+(theRect->l+((r+theRect->t)*lm->width))*lightmap_bytes, lightmap[i]->rectchange.w*lightmap_bytes);
			}
			IDirect3DTexture9_UnlockRect(tex, 0);
			theRect->l = lm->width;
			theRect->t = lm->height;
			theRect->h = 0;
			theRect->w = 0;
		}
	}
}

void D3D9BE_UploadAllLightmaps(void)
{
	BE_UploadLightmaps(true);
}

qboolean D3D9BE_LightCullModel(vec3_t org, model_t *model)
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

batch_t *D3D9BE_GetTempBatch(void)
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
	float *m = shaderstate.m_model;

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
		/*FIXME: no bob*/
		float iv[16];
		Matrix4_Invert(r_refdef.m_view, iv);
		Matrix4x4_CM_NewRotation(90, 1, 0, 0);
		Matrix4_Multiply(iv, m, mv);
		Matrix4_Multiply(mv, Matrix4x4_CM_NewRotation(-90, 1, 0, 0), iv);
		Matrix4_Multiply(iv, Matrix4x4_CM_NewRotation(90, 0, 0, 1), mv);

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

void D3D9BE_SubmitBatch(batch_t *batch)
{
	shaderstate.nummeshes = batch->meshes - batch->firstmesh;
	if (!shaderstate.nummeshes)
		return;
	if (shaderstate.curentity != batch->ent)
	{
		BE_RotateForEntity(batch->ent, batch->ent->model);
		shaderstate.curtime = r_refdef.time - shaderstate.curentity->shaderTime;
	}
	shaderstate.batchvbo = batch->vbo;
	shaderstate.meshlist = batch->mesh + batch->firstmesh;
	shaderstate.curshader = batch->shader;
	shaderstate.curtexnums = batch->skin;
	shaderstate.flags = batch->flags;
	if (batch->lightmap[0] < 0)
		shaderstate.curlightmap = r_nulltex;
	else
		shaderstate.curlightmap = lightmap[batch->lightmap[0]]->lightmap_texture;

	BE_DrawMeshChain_Internal();
}

void D3D9BE_DrawMesh_List(shader_t *shader, int nummeshes, mesh_t **meshlist, vbo_t *vbo, texnums_t *texnums, unsigned int beflags)
{
	shaderstate.batchvbo = vbo;
	shaderstate.curshader = shader;
	shaderstate.curtexnums = texnums;
	shaderstate.curlightmap = r_nulltex;
	shaderstate.meshlist = meshlist;
	shaderstate.nummeshes = nummeshes;
	shaderstate.flags = beflags;

	BE_DrawMeshChain_Internal();
}

void D3D9BE_DrawMesh_Single(shader_t *shader, mesh_t *meshchain, vbo_t *vbo, texnums_t *texnums, unsigned int beflags)
{
	shaderstate.batchvbo = vbo;
	shaderstate.curtime = realtime;
	shaderstate.curshader = shader;
	shaderstate.curtexnums = texnums?texnums:&shader->defaulttextures;
	shaderstate.curlightmap = r_nulltex;
	shaderstate.meshlist = &meshchain;
	shaderstate.nummeshes = 1;
	shaderstate.flags = beflags;

	BE_DrawMeshChain_Internal();
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
			if (!batch->shader->prog)
			{
				if (shaderstate.mode == BEM_STANDARD)
					R_DrawSkyChain (batch);
				continue;
			}
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
static void R_RenderScene(void)
{
	IDirect3DDevice9_SetTransform(pD3DDev9, D3DTS_PROJECTION, (D3DMATRIX*)d3d_trueprojection);
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
	Matrix4x4_CM_ModelViewMatrixFromAxis(r_refdef.m_view, vpn, vright, vup, r_refdef.vieworg);
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

	IDirect3DDevice9_SetTransform(pD3DDev9, D3DTS_PROJECTION, (D3DMATRIX*)d3d_trueprojection);
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
				BE_SelectMode(BEM_DEPTHONLY);
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
				BE_SelectMode(BEM_STANDARD);

				R_DrawPortal(batch, worldlist);

				/*clear depth again*/
				IDirect3DDevice9_Clear(pD3DDev9, 0, NULL, D3DCLEAR_ZBUFFER, D3DCOLOR_XRGB(0,0,0), 1, 0);
			}
		}
	}
}

void D3D9BE_SubmitMeshes (qboolean drawworld, batch_t **blist, int first, int stop)
{
	model_t *model = cl.worldmodel;
	int i;

	for (i = first; i < stop; i++)
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

#ifdef RTLIGHTS
void D3D9BE_BaseEntTextures(void)
{
	batch_t *batches[SHADER_SORT_COUNT];
	BE_GenModelBatches(batches);
	D3D9BE_SubmitMeshes(false, batches, SHADER_SORT_PORTAL, SHADER_SORT_DECAL);
	BE_SelectEntity(&r_worldentity);
}

void D3D9BE_RenderShadowBuffer(unsigned int numverts, IDirect3DVertexBuffer9 *vbuf, unsigned int numindicies, IDirect3DIndexBuffer9 *ibuf)
{
	IDirect3DDevice9_SetStreamSource(pD3DDev9, STRM_VERT, vbuf, 0, sizeof(vecV_t));
	IDirect3DDevice9_SetIndices(pD3DDev9, ibuf);

	if (0 != shaderstate.curvertdecl)
	{
		shaderstate.curvertdecl = 0;
		d3dcheck(IDirect3DDevice9_SetVertexDeclaration(pD3DDev9, vertexdecls[shaderstate.curvertdecl]));
	}

	IDirect3DDevice9_DrawIndexedPrimitive(pD3DDev9, D3DPT_TRIANGLELIST, 0, 0, numverts, 0, numindicies/3);
}
#endif

void D3D9BE_DrawWorld (qboolean drawworld, qbyte *vis)
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

	if (drawworld)
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

		BE_SelectMode(BEM_STANDARD);

		RSpeedRemark();
		D3D9BE_SubmitMeshes(true, batches, SHADER_SORT_PORTAL, SHADER_SORT_DECAL);
		RSpeedEnd(RSPEED_WORLD);

#ifdef RTLIGHTS
		if (vis)
		{
			RSpeedRemark();
			D3D9BE_SelectEntity(&r_worldentity);
			Sh_DrawLights(vis);
			RSpeedEnd(RSPEED_STENCILSHADOWS);
		}
#endif

		D3D9BE_SubmitMeshes(true, batches, SHADER_SORT_DECAL, SHADER_SORT_COUNT);
	}
	else
	{
		RSpeedRemark();
		D3D9BE_SubmitMeshes(false, batches, SHADER_SORT_PORTAL, SHADER_SORT_COUNT);
		RSpeedEnd(RSPEED_DRAWENTITIES);
	}

	R_RenderDlights ();

	BE_RotateForEntity(&r_worldentity, NULL);
}


#endif
