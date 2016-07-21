#include "quakedef.h"

#ifdef D3D9QUAKE
#include "shader.h"
#include "winquake.h"
#if !defined(HMONITOR_DECLARED) && (WINVER < 0x0500)
    #define HMONITOR_DECLARED
    DECLARE_HANDLE(HMONITOR);
#endif
#include <d3d9.h>
extern LPDIRECT3DDEVICE9 pD3DDev9;

typedef struct {
  LPCSTR Name;
  LPCSTR Definition;
} D3DXMACRO;

#define D3DXHANDLE void *
#define LPD3DXINCLUDE void *

#undef INTERFACE
#define INTERFACE d3dxbuffer
DECLARE_INTERFACE_(d3dxbuffer,IUnknown)
{
	STDMETHOD(QueryInterface)(THIS_ REFIID,PVOID*) PURE;
	STDMETHOD_(ULONG,AddRef)(THIS) PURE;
	STDMETHOD_(ULONG,Release)(THIS) PURE;

	STDMETHOD_(LPVOID,GetBufferPointer)(THIS) PURE;
	STDMETHOD_(SIZE_T,GetBufferSize)(THIS) PURE;
};
typedef struct d3dxbuffer *LPD3DXBUFFER;

typedef enum _D3DXREGISTER_SET 
{ 
    D3DXRS_BOOL, 
    D3DXRS_INT4, 
    D3DXRS_FLOAT4, 
    D3DXRS_SAMPLER, 
    D3DXRS_FORCE_DWORD = 0x7fffffff 
} D3DXREGISTER_SET, *LPD3DXREGISTER_SET; 
typedef enum _D3DXPARAMETER_CLASS 
{ 
    D3DXPC_SCALAR, 
    D3DXPC_VECTOR, 
    D3DXPC_MATRIX_ROWS, 
    D3DXPC_MATRIX_COLUMNS, 
    D3DXPC_OBJECT, 
    D3DXPC_STRUCT, 
    D3DXPC_FORCE_DWORD = 0x7fffffff 
} D3DXPARAMETER_CLASS, *LPD3DXPARAMETER_CLASS;
typedef enum _D3DXPARAMETER_TYPE 
{ 
    D3DXPT_VOID, 
    D3DXPT_BOOL, 
    D3DXPT_INT, 
    D3DXPT_FLOAT, 
    D3DXPT_STRING, 
    D3DXPT_TEXTURE, 
    D3DXPT_TEXTURE1D, 
    D3DXPT_TEXTURE2D, 
    D3DXPT_TEXTURE3D, 
    D3DXPT_TEXTURECUBE, 
    D3DXPT_SAMPLER, 
    D3DXPT_SAMPLER1D, 
    D3DXPT_SAMPLER2D, 
    D3DXPT_SAMPLER3D, 
    D3DXPT_SAMPLERCUBE, 
    D3DXPT_PIXELSHADER, 
    D3DXPT_VERTEXSHADER, 
    D3DXPT_PIXELFRAGMENT, 
    D3DXPT_VERTEXFRAGMENT,
} D3DXPARAMETER_TYPE, *LPD3DXPARAMETER_TYPE;
typedef struct _D3DXCONSTANT_DESC 
{ 
    LPCSTR Name;                        // Constant name 

    D3DXREGISTER_SET RegisterSet;       // Register set 
    UINT RegisterIndex;                 // Register index 
    UINT RegisterCount;                 // Number of registers occupied 
 
    D3DXPARAMETER_CLASS Class;          // Class 
    D3DXPARAMETER_TYPE Type;            // Component type 
 
    UINT Rows;                          // Number of rows 
    UINT Columns;                       // Number of columns 
    UINT Elements;                      // Number of array elements 
    UINT StructMembers;                 // Number of structure member sub-parameters 
 
    UINT Bytes;                         // Data size, in bytes 
    LPCVOID DefaultValue;               // Pointer to default value 
 
} D3DXCONSTANT_DESC, *LPD3DXCONSTANT_DESC; 
typedef struct _D3DXCONSTANTTABLE_DESC 
{ 
    LPCSTR Creator;                     // Creator string 
    DWORD Version;                      // Shader version 
    UINT Constants;                     // Number of constants 
 
} D3DXCONSTANTTABLE_DESC, *LPD3DXCONSTANTTABLE_DESC;
 
#undef INTERFACE
#define INTERFACE d3dxconstanttable
DECLARE_INTERFACE_(d3dxconstanttable,IUnknown)
{
	STDMETHOD(QueryInterface)(THIS_ REFIID,PVOID*) PURE;
	STDMETHOD_(ULONG,AddRef)(THIS) PURE;
	STDMETHOD_(ULONG,Release)(THIS) PURE;

	STDMETHOD_(LPVOID,GetBufferPointer)(THIS) PURE;
	STDMETHOD_(SIZE_T,GetBufferSize)(THIS) PURE;

    STDMETHOD(GetDesc)(THIS_ D3DXCONSTANTTABLE_DESC *pDesc) PURE; 
    STDMETHOD(GetConstantDesc)(THIS_ D3DXHANDLE hConstant, D3DXCONSTANT_DESC *pConstantDesc, UINT *pCount) PURE; 
 
    STDMETHOD_(D3DXHANDLE, GetConstant)(THIS_ D3DXHANDLE hConstant, UINT Index) PURE; 
    STDMETHOD_(D3DXHANDLE, GetConstantByName)(THIS_ D3DXHANDLE hConstant, LPCSTR pName) PURE; 
	STDMETHOD_(D3DXHANDLE, GetConstantElement)(THIS_ D3DXHANDLE hConstant, UINT Index) PURE;

	/*more stuff not included here cos I don't need it*/
};
typedef struct d3dxconstanttable *LPD3DXCONSTANTTABLE;


HRESULT (WINAPI *pD3DXCompileShader) (
	LPCSTR pSrcData,
	UINT SrcDataLen,
	const D3DXMACRO *pDefines,
	LPD3DXINCLUDE pInclude,
	LPCSTR pEntrypoint,
	LPCSTR pTarget,
	UINT Flags,
	LPD3DXBUFFER *ppCode,
	LPD3DXBUFFER *ppErrorMsgs,
	LPD3DXCONSTANTTABLE *constants
);
static dllhandle_t *shaderlib;

#ifndef IUnknown_Release
#define IUnknown_Release(This)	\
    (This)->lpVtbl -> Release(This)
#endif

static qboolean D3D9Shader_CreateProgram (program_t *prog, const char *sname, unsigned int permu, int ver, const char **precompilerconstants, const char *vert, const char *tcs, const char *tes, const char *geom, const char *frag, qboolean silent, vfsfile_t *blobfile)
{
	D3DXMACRO defines[64];
	LPD3DXBUFFER code = NULL, errors = NULL;
	qboolean success = false;
	char defbuf[2048];
	char *defbufe;

	if (geom || tcs || tes)
	{
		Con_Printf("geometry and tessellation shaders are not availale in d3d9 (%s)\n", sname);
		return false;
	}

	prog->permu[permu].h.hlsl.vert = NULL;
	prog->permu[permu].h.hlsl.frag = NULL;

	if (pD3DXCompileShader)
	{
		int consts;
		for (consts = 0; precompilerconstants[consts]; consts++)
			;
		consts+=2;
		if (consts >= sizeof(defines) / sizeof(defines[0]))
			return success;

		consts = 0;
		defines[consts].Name = NULL; /*shader type*/
		defines[consts].Definition = "1";
		consts++;

		defines[consts].Name = "ENGINE_"DISTRIBUTION;
		defines[consts].Definition = __DATE__;
		consts++;

		for (defbufe = defbuf; *precompilerconstants; precompilerconstants++)
		{
			defines[consts].Definition = COM_ParseOut(*precompilerconstants+7, defbufe, defbuf+sizeof(defbuf) - defbufe-1);
			defines[consts].Name = defbufe;
			defbufe += strlen(defbufe)+1;
			consts++;
		}

		defines[consts].Name = NULL;
		defines[consts].Definition = NULL;

		success = true;

		defines[0].Name = "VERTEX_SHADER";
		if (FAILED(pD3DXCompileShader(vert, strlen(vert), defines, NULL, "main", "vs_2_0", 0, &code, &errors, (LPD3DXCONSTANTTABLE*)&prog->permu[permu].h.hlsl.ctabv)))
			success = false;
		else
		{
			IDirect3DDevice9_CreateVertexShader(pD3DDev9, code->lpVtbl->GetBufferPointer(code), (IDirect3DVertexShader9**)&prog->permu[permu].h.hlsl.vert);
			code->lpVtbl->Release(code);
		}
		if (errors)
		{
			char *messages = errors->lpVtbl->GetBufferPointer(errors);
			Con_Printf("Error compiling vertex shader %s:\n%s", sname, messages);
			errors->lpVtbl->Release(errors);
		}

		defines[0].Name = "FRAGMENT_SHADER";
		if (FAILED(pD3DXCompileShader(frag, strlen(frag), defines, NULL, "main", "ps_2_0", 0, &code, &errors, (LPD3DXCONSTANTTABLE*)&prog->permu[permu].h.hlsl.ctabf)))
			success = false;
		else
		{
			IDirect3DDevice9_CreatePixelShader(pD3DDev9, code->lpVtbl->GetBufferPointer(code), (IDirect3DPixelShader9**)&prog->permu[permu].h.hlsl.frag);
			code->lpVtbl->Release(code);
		}
		if (errors)
		{
			char *messages = errors->lpVtbl->GetBufferPointer(errors);
			Con_Printf("Error compiling pixel shader %s:\n%s", sname, messages);
			errors->lpVtbl->Release(errors);
		}
	}
	return success;
}

static int D3D9Shader_FindUniform_(LPD3DXCONSTANTTABLE ct, const char *name)
{
	if (ct)
	{
		UINT dc = 1;
		D3DXCONSTANT_DESC d;
		if (!FAILED(ct->lpVtbl->GetConstantDesc(ct, (void*)name, &d, &dc)))
			return d.RegisterIndex;
	}
	return -1;
}

static int D3D9Shader_FindUniform(union programhandle_u *h, int type, const char *name)
{
	int offs;

	if (!type || type == 1)
	{
		offs = D3D9Shader_FindUniform_(h->hlsl.ctabv, name);
		if (offs >= 0)
			return offs;
	}
	if (!type || type == 2)
	{
		offs = D3D9Shader_FindUniform_(h->hlsl.ctabf, name);
		if (offs >= 0)
			return offs;
	}

	return -1;
}

static void D3D9Shader_ProgAutoFields(program_t *prog, const char *progname, cvar_t **cvarrefs, char **cvarnames, int *cvartypes)
{
	struct programpermu_s *pp;
	unsigned int i, p;
	int uniformloc;

	static const char *defaultsamplers[] =
	{
		"s_shadowmap",
		"s_projectionmap",
		"s_diffuse",
		"s_normalmap",
		"s_specular",
		"s_upper",
		"s_lower",
		"s_fullbright",
		"s_paletted",
		"s_reflectcube",
		"s_reflectmask",
		"s_lightmap",
		"s_deluxmap"
#if MAXRLIGHTMAPS > 1
		,"s_lightmap1"
		,"s_lightmap2"
		,"s_lightmap3"
		,"s_deluxmap1"
		,"s_deluxmap2"
		,"s_deluxmap3"
#endif
	};
#define ALTLIGHTMAPSAMP 13
#define ALTDELUXMAPSAMP 16

	prog->nofixedcompat = true;
	prog->defaulttextures = 0;
	prog->numsamplers = 0;

	for (p = 0; p < PERMUTATIONS; p++)
	{
		int maxparms = 0;
		pp = &prog->permu[p];
		pp->parm = NULL;
		pp->numparms = 0;
		if (!pp->h.loaded)
			continue;
		IDirect3DDevice9_SetVertexShader(pD3DDev9, pp->h.hlsl.vert);
		IDirect3DDevice9_SetPixelShader(pD3DDev9, pp->h.hlsl.frag);

		for (i = 0; shader_unif_names[i].name; i++)
		{
			uniformloc = D3D9Shader_FindUniform(&pp->h, 0, shader_unif_names[i].name);
			if (uniformloc != -1)
			{
				if (pp->numparms == maxparms)
				{
					maxparms = maxparms?maxparms*2:8;
					pp->parm = BZ_Realloc(pp->parm, sizeof(*pp->parm) * maxparms);
				}
				pp->parm[pp->numparms].handle = uniformloc;
				pp->parm[pp->numparms].type = shader_unif_names[i].ptype;
				pp->numparms++;
			}
		}

		for (i = 0; cvarnames[i]; i++)
		{
			if (!cvarrefs[i])
				continue;
			//just directly sets uniforms. can't cope with cvars dynamically changing.
			cvarrefs[i]->flags |= CVAR_SHADERSYSTEM;
		
			uniformloc = D3D9Shader_FindUniform(&prog->permu[p].h, 1, va("cvar_%s", cvarnames[i]));
			if (uniformloc != -1)
			{
				if (cvartypes[i] == SP_CVARI)
				{
					int v[4] = {cvarrefs[i]->ival, 0, 0, 0};
					IDirect3DDevice9_SetVertexShaderConstantI(pD3DDev9, 0, v, 1);
				}
				else
					IDirect3DDevice9_SetVertexShaderConstantF(pD3DDev9, 0, cvarrefs[i]->vec4, 1);
			}
			uniformloc = D3D9Shader_FindUniform(&prog->permu[p].h, 2, va("cvar_%s", cvarnames[i]));
			if (uniformloc != -1)
			{
				if (cvartypes[i] == SP_CVARI)
				{
					int v[4] = {cvarrefs[i]->ival, 0, 0, 0};
					IDirect3DDevice9_SetPixelShaderConstantI(pD3DDev9, 0, v, 1);
				}
				else
					IDirect3DDevice9_SetPixelShaderConstantF(pD3DDev9, 0, cvarrefs[i]->vec4, 1);
			}
		}

		/*set texture uniforms*/
		for (i = 0; i < 8; i++)
		{
			uniformloc = D3D9Shader_FindUniform(&pp->h, 2, va("s_t%i", i));
			if (uniformloc != -1)
			{
				int v[4] = {i};
				IDirect3DDevice9_SetPixelShader(pD3DDev9, pp->h.hlsl.frag);
				IDirect3DDevice9_SetPixelShaderConstantI(pD3DDev9, 0, v, 1);

				if (prog->numsamplers < i+1)
					prog->numsamplers = i+1;
			}
		}

		for (i = 0; i < sizeof(defaultsamplers)/sizeof(defaultsamplers[0]); i++)
		{
			//figure out which ones are needed.
			if (prog->defaulttextures & (1u<<i))
				continue;	//don't spam
			uniformloc = D3D9Shader_FindUniform(&pp->h, 2, defaultsamplers[i]);
			if (uniformloc != -1)
				prog->defaulttextures |= (1u<<i);
		}
	}

	//multiple lightmaps is kinda hacky. if any are set, all must be. 
	if (prog->defaulttextures & ((1u<<(ALTLIGHTMAPSAMP+0)) | (1u<<(ALTLIGHTMAPSAMP+1)) | (1u<<(ALTLIGHTMAPSAMP+2))))
		prog->defaulttextures |=((1u<<(ALTLIGHTMAPSAMP+0)) | (1u<<(ALTLIGHTMAPSAMP+1)) | (1u<<(ALTLIGHTMAPSAMP+2)));
	if (prog->defaulttextures & ((1u<<(ALTDELUXMAPSAMP+0)) | (1u<<(ALTDELUXMAPSAMP+1)) | (1u<<(ALTDELUXMAPSAMP+2))))
		prog->defaulttextures |=((1u<<(ALTDELUXMAPSAMP+0)) | (1u<<(ALTDELUXMAPSAMP+1)) | (1u<<(ALTDELUXMAPSAMP+2)));

	if (prog->defaulttextures)
	{
		unsigned int sampnum;
		/*set default texture uniforms*/
		for (p = 0; p < PERMUTATIONS; p++)
		{
			if (!prog->permu[p].h.loaded)
				continue;
			sampnum = prog->numsamplers;
			for (i = 0; i < sizeof(defaultsamplers)/sizeof(defaultsamplers[0]); i++)
			{
				if (prog->defaulttextures & (1u<<i))
				{
					uniformloc = D3D9Shader_FindUniform(&prog->permu[p].h, 2, defaultsamplers[i]);
					if (uniformloc != -1)
					{
						int v[4] = {sampnum};
						IDirect3DDevice9_SetPixelShader(pD3DDev9, prog->permu[p].h.hlsl.frag);
						IDirect3DDevice9_SetPixelShaderConstantI(pD3DDev9, 0, v, 1);
					}
					sampnum++;
				}
			}
		}
	}
	IDirect3DDevice9_SetVertexShader(pD3DDev9, NULL);
	IDirect3DDevice9_SetPixelShader(pD3DDev9, NULL);
}

void D3D9Shader_DeleteProg(program_t *prog)
{
	unsigned int permu;
	for (permu = 0; permu < countof(prog->permu); permu++)
	{
		if (prog->permu[permu].h.hlsl.vert)
		{
			IDirect3DVertexShader9 *vs = prog->permu[permu].h.hlsl.vert;
			prog->permu[permu].h.hlsl.vert = NULL;
			IDirect3DVertexShader9_Release(vs);
		}
		if (prog->permu[permu].h.hlsl.frag)
		{
			IDirect3DPixelShader9 *fs = prog->permu[permu].h.hlsl.frag;
			prog->permu[permu].h.hlsl.frag = NULL;
			IDirect3DPixelShader9_Release(fs);
		}
		if (prog->permu[permu].h.hlsl.ctabv)
		{
			LPD3DXCONSTANTTABLE vct = prog->permu[permu].h.hlsl.ctabv;
			prog->permu[permu].h.hlsl.ctabv = NULL;
			IUnknown_Release(vct);
		}
		if (prog->permu[permu].h.hlsl.ctabf)
		{
			LPD3DXCONSTANTTABLE fct = prog->permu[permu].h.hlsl.ctabf;
			prog->permu[permu].h.hlsl.ctabf = NULL;
			IUnknown_Release(fct);
		}
	}
}

void D3D9Shader_Init(void)
{
	D3DCAPS9 caps;

	dllfunction_t funcs[] =
	{
		{(void**)&pD3DXCompileShader, "D3DXCompileShader"},
		{NULL,NULL}
	};

	if (!shaderlib)
		shaderlib = Sys_LoadLibrary("d3dx9_32", funcs);
	if (!shaderlib)
		shaderlib = Sys_LoadLibrary("d3dx9_34", funcs);

	memset(&sh_config, 0, sizeof(sh_config));
	sh_config.minver = 9;
	sh_config.maxver = 9;
	sh_config.blobpath = "hlsl/%s.blob";
	sh_config.progpath = shaderlib?"hlsl/%s.hlsl":NULL;
	sh_config.shadernamefmt = "%s_hlsl9";

	sh_config.progs_supported	= !!shaderlib;
	sh_config.progs_required	= false;

	sh_config.pDeleteProg		= D3D9Shader_DeleteProg;
	sh_config.pLoadBlob			= NULL;//D3D9Shader_LoadBlob;
	sh_config.pCreateProgram	= D3D9Shader_CreateProgram;
	sh_config.pProgAutoFields	= D3D9Shader_ProgAutoFields;

	sh_config.tex_env_combine		= 1;
	sh_config.nv_tex_env_combine4	= 1;
	sh_config.env_add				= 1;

	//FIXME: check caps
	sh_config.texfmt[PTI_RGBX8] = true;	//fixme: shouldn't support
	sh_config.texfmt[PTI_RGBA8] = true;	//fixme: shouldn't support
	sh_config.texfmt[PTI_BGRX8] = true;
	sh_config.texfmt[PTI_BGRA8] = true;
	sh_config.texfmt[PTI_RGB565] = true;
	sh_config.texfmt[PTI_ARGB1555] = true;
	sh_config.texfmt[PTI_ARGB4444] = true;

	sh_config.can_mipcap		= true;	//at creation time, I think.

	IDirect3DDevice9_GetDeviceCaps(pD3DDev9, &caps);

	if (caps.TextureCaps & D3DPTEXTURECAPS_POW2)
	{	//this flag is a LIMITATION, not a capability.
		sh_config.texture_non_power_of_two = false;
		sh_config.texture_non_power_of_two_pic = !!(caps.TextureCaps & D3DPTEXTURECAPS_NONPOW2CONDITIONAL);	//pic npot is supported if both flags are set.
	}
	else
	{	//modern cards support npot
		sh_config.texture_non_power_of_two_pic = true;
		sh_config.texture_non_power_of_two = true;
	}

	sh_config.texture_maxsize = min(caps.MaxTextureWidth, caps.MaxTextureHeight);
}
#endif

