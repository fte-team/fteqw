#include "quakedef.h"

#ifdef D3D11QUAKE
#include "shader.h"
#include "winquake.h"
#define COBJMACROS
#include <d3d11.h>
extern ID3D11Device *pD3DDev11;


typedef struct _D3D_SHADER_MACRO
{
    LPCSTR Name;
    LPCSTR Definition;

} D3D_SHADER_MACRO, *LPD3D_SHADER_MACRO;

typedef enum _D3D_INCLUDE_TYPE { 
  D3D_INCLUDE_LOCAL         = 0,
  D3D_INCLUDE_SYSTEM        = ( D3D_INCLUDE_LOCAL + 1 ),
  D3D_INCLUDE_FORCE_DWORD   = 0x7fffffff 
} D3D_INCLUDE_TYPE;

#undef INTERFACE
#define INTERFACE ID3DInclude
DECLARE_INTERFACE_(INTERFACE, IUnknown)
{
	STDMETHOD(Open)(THIS_ D3D_INCLUDE_TYPE IncludeType, LPCSTR pFileName, LPCVOID pParentData, LPCVOID *ppData, UINT *pBytes) PURE;
    STDMETHOD(Close)(THIS_ LPCVOID pData) PURE;
};

#undef INTERFACE
#define INTERFACE ID3DBlob
DECLARE_INTERFACE_(INTERFACE, IUnknown)
{
    STDMETHOD(QueryInterface)(THIS_ REFIID iid, LPVOID *ppv) PURE;
    STDMETHOD_(ULONG, AddRef)(THIS) PURE;
    STDMETHOD_(ULONG, Release)(THIS) PURE;
    STDMETHOD_(LPVOID, GetBufferPointer)(THIS) PURE;
    STDMETHOD_(SIZE_T, GetBufferSize)(THIS) PURE;
};
#define ID3DBlob_GetBufferPointer(b) b->lpVtbl->GetBufferPointer(b)
#define ID3DBlob_Release(b) b->lpVtbl->Release(b)
#define ID3DBlob_GetBufferSize(b) b->lpVtbl->GetBufferSize(b)
#undef INTERFACE

HRESULT (WINAPI *pD3DCompile) (
	LPCVOID pSrcData,
	SIZE_T SrcDataSize,
	LPCSTR pSourceName,
	const D3D_SHADER_MACRO *pDefines,
	ID3DInclude *pInclude,
	LPCSTR pEntrypoint,
	LPCSTR pTarget,
	UINT Flags1,
	UINT Flags2,
	ID3DBlob **ppCode,
	ID3DBlob **ppErrorMsgs
);
static dllhandle_t *shaderlib;


D3D_FEATURE_LEVEL d3dfeaturelevel;
qboolean D3D11Shader_Init(unsigned int flevel)
{
	//FIXME: if the feature level is below 10, make sure the compiler supports all the right targets etc
	int ver;
	dllfunction_t funcsold[] =
	{
		{(void**)&pD3DCompile, "D3DCompileFromMemory"},
		{NULL,NULL}
	};
	dllfunction_t funcsnew[] =
	{
		{(void**)&pD3DCompile, "D3DCompile"},
		{NULL,NULL}
	};

	for (ver = 43; ver >= 33; ver--)
	{
		shaderlib = Sys_LoadLibrary(va("D3dcompiler_%i.dll", ver), (ver>=40)?funcsnew:funcsold);
		if (shaderlib)
			break;
	}

	if (!shaderlib)
		return false;

	d3dfeaturelevel = flevel;
	return true;
}

HRESULT STDMETHODCALLTYPE d3dinclude_Close(ID3DInclude *this, LPCVOID pData)
{
	free((void*)pData);
	return S_OK;
}
HRESULT STDMETHODCALLTYPE d3dinclude_Open(ID3DInclude *this, D3D_INCLUDE_TYPE IncludeType, LPCSTR pFileName, LPCVOID pParentData, LPCVOID *ppData, UINT *pBytes)
{
	if (IncludeType == D3D_INCLUDE_SYSTEM)
	{
		if (!strcmp(pFileName, "ftedefs.h"))
		{
			static const char *defstruct =
				"cbuffer ftemodeldefs : register(b0)\n"
				"{\n"
					"matrix m_model;\n"
					"float3 e_eyepos; float e_time;\n"
					"float3 e_light_ambient; float pad1;\n"
					"float3 e_light_dir; float pad2;\n"
					"float3 e_light_mul; float pad3;\n"
					"float4 e_lmscale[4];\n"
				"};\n"
				"cbuffer fteviewdefs : register(b1)\n"
				"{\n"
					"matrix m_view;\n"
					"matrix m_projection;\n"
					"float3 v_eyepos; float v_time;\n"
				"};\n"
				"cbuffer ftelightdefs : register(b2)\n"
				"{\n"
					"matrix l_cubematrix;\n"
					"float3 l_lightposition; float padl1;\n"
					"float3 l_colour; float padl2;\n"
					"float3 l_lightcolourscale;float l_lightradius;\n"
					"float4 l_shadowmapproj;\n"
					"float2 l_shadowmapscale; float2 padl3;\n"
				"};\n"
				;
			*ppData = strdup(defstruct);
			*pBytes = strlen(*ppData);
			return S_OK;
		}
		if (!strcmp(pFileName, "sys/skeletal.h"))
		{
			static const char *defstruct =
				""
				;
			*ppData = strdup(defstruct);
			*pBytes = strlen(*ppData);
			return S_OK;
		}
	}
	else
	{
		
	}
	return E_FAIL;
}
ID3DIncludeVtbl myd3dincludetab =
{
	d3dinclude_Open,
	d3dinclude_Close
};
ID3DInclude myd3dinclude =
{
	&myd3dincludetab
};

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

void D3D11Shader_DeleteProgram(program_t *prog)
{
	ID3D11InputLayout *layout;
	ID3D11PixelShader *frag;
	ID3D11VertexShader *vert;
	int permu;
	for (permu = 0; permu < PERMUTATIONS; permu++)
	{
		vert = prog->permu[permu].handle.hlsl.vert;
		frag = prog->permu[permu].handle.hlsl.frag;
		layout = prog->permu[permu].handle.hlsl.layout;
		if (vert)
			ID3D11VertexShader_Release(vert);
		if (frag)
			ID3D11PixelShader_Release(frag);
		if (layout)
			ID3D11InputLayout_Release(layout);
	}
}

qboolean D3D11Shader_CreateProgram (program_t *prog, const char *name, int permu, char **precompilerconstants, char *vert, char *frag)
{
	char *vsformat;
	char *fsformat;
	D3D_SHADER_MACRO defines[64];
	ID3DBlob *vcode = NULL, *fcode = NULL, *errors = NULL;
	qboolean success = false;

	if (d3dfeaturelevel >= D3D_FEATURE_LEVEL_11_0)	//and 11.1
	{
		vsformat = "vs_5_0";
		fsformat = "ps_5_0";
	}
	else if (d3dfeaturelevel >= D3D_FEATURE_LEVEL_10_1)
	{
		vsformat = "vs_4_1";
		fsformat = "ps_4_1";
	}
	else if (d3dfeaturelevel >= D3D_FEATURE_LEVEL_10_0)
	{
		vsformat = "vs_4_0";
		fsformat = "ps_4_0";
	}
	else if (d3dfeaturelevel >= D3D_FEATURE_LEVEL_9_3)
	{	//dx10-compatible output for 9.3 hardware
		vsformat = "vs_4_0_level_9_3";
		fsformat = "ps_4_0_level_9_3";
	}
	else
	{	//dx10-compatible output for 9.1|9.2 hardware
		vsformat = "vs_4_0_level_9_1";
		fsformat = "ps_4_0_level_9_1";
	}

	prog->permu[permu].handle.hlsl.vert = NULL;
	prog->permu[permu].handle.hlsl.frag = NULL;
	prog->permu[permu].handle.hlsl.layout = NULL;

	if (pD3DCompile)
	{
		int consts;
		for (consts = 2; precompilerconstants[consts]; consts++)
			;
		if (consts >= sizeof(defines) / sizeof(defines[0]))
			return success;

		consts = 0;
		defines[consts].Name = NULL; /*shader type*/
		defines[consts].Definition = "1";
		consts++;

		defines[consts].Name = "ENGINE_"DISTRIBUTION;
		defines[consts].Definition = __DATE__;
		consts++;

		defines[consts].Name = Z_StrDup("LEVEL");
		defines[consts].Definition = Z_StrDup(va("0x%x", d3dfeaturelevel));
		consts++;

		for (; *precompilerconstants; precompilerconstants++)
		{
			char *t = *precompilerconstants;
			t = COM_Parse(t);
			t = COM_Parse(t);
			defines[consts].Name = Z_StrDup(com_token);
			defines[consts].Definition = t?Z_StrDup(t):NULL;
			consts++;
		}

		defines[consts].Name = NULL;
		defines[consts].Definition = NULL;

		success = true;

		defines[0].Name = "VERTEX_SHADER";
		if (FAILED(pD3DCompile(vert, strlen(vert), name, defines, &myd3dinclude, "main", vsformat, 0, 0, &vcode, &errors)))
			success = false;
		else
		{
			if (FAILED(ID3D11Device_CreateVertexShader(pD3DDev11, ID3DBlob_GetBufferPointer(vcode), ID3DBlob_GetBufferSize(vcode), NULL, (ID3D11VertexShader**)&prog->permu[permu].handle.hlsl.vert)))
				success = false;
		}
		if (errors)
		{
			char *messages = ID3DBlob_GetBufferPointer(errors);
			Con_Printf("vertex shader:\n%s", messages);
			ID3DBlob_Release(errors);
		}

		defines[0].Name = "FRAGMENT_SHADER";
		if (FAILED(pD3DCompile(frag, strlen(frag), name, defines, &myd3dinclude, "main", fsformat, 0, 0, &fcode, &errors)))
			success = false;
		else
		{
			if (FAILED(ID3D11Device_CreatePixelShader(pD3DDev11, ID3DBlob_GetBufferPointer(fcode), ID3DBlob_GetBufferSize(fcode), NULL, (ID3D11PixelShader**)&prog->permu[permu].handle.hlsl.frag)))
				success = false;
		}
		if (errors)
		{
			char *messages = ID3DBlob_GetBufferPointer(errors);
			Con_Printf("fragment shader:\n%s", messages);
			ID3DBlob_Release(errors);
		}

		while(consts-->2)
		{
			Z_Free((void*)defines[consts].Name);
			Z_Free((void*)defines[consts].Definition);
		}

		if (success)
		{
			D3D11_INPUT_ELEMENT_DESC decl[13];
			int elements = 0;
			vbovdata_t *foo = NULL;

			decl[elements].SemanticName = "POSITION";
			decl[elements].SemanticIndex = 0;
			decl[elements].Format = DXGI_FORMAT_R32G32B32_FLOAT;
			decl[elements].InputSlot = 0;
			decl[elements].AlignedByteOffset = (char*)&foo->coord[0] - (char*)NULL;
			decl[elements].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
			decl[elements].InstanceDataStepRate = 0;
			elements++;

			decl[elements].SemanticName = "TEXCOORD";
			decl[elements].SemanticIndex = 0;
			decl[elements].Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
			decl[elements].InputSlot = 0;
			decl[elements].AlignedByteOffset = (char*)&foo->tex[0] - (char*)NULL;
			decl[elements].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
			decl[elements].InstanceDataStepRate = 0;
			elements++;
			/*
			decl[elements].SemanticName = "TEXCOORD";
			decl[elements].SemanticIndex = 1;
			decl[elements].Format = DXGI_FORMAT_R32G32_FLOAT;
			decl[elements].InputSlot = 1;
			decl[elements].AlignedByteOffset = (char*)&foo->lm[0] - (char*)NULL;
			decl[elements].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
			decl[elements].InstanceDataStepRate = 0;
			elements++;
			*/
			decl[elements].SemanticName = "COLOR";
			decl[elements].SemanticIndex = 0;
			decl[elements].Format = DXGI_FORMAT_R8G8B8A8_UNORM;
			decl[elements].InputSlot = 0;
			decl[elements].AlignedByteOffset = (char*)&foo->colorsb[0] - (char*)NULL;
			decl[elements].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
			decl[elements].InstanceDataStepRate = 0;
			elements++;

			decl[elements].SemanticName = "NORMAL";
			decl[elements].SemanticIndex = 0;
			decl[elements].Format = DXGI_FORMAT_R32G32B32_FLOAT;
			decl[elements].InputSlot = 0;
			decl[elements].AlignedByteOffset = (char*)&foo->ndir[0] - (char*)NULL;
			decl[elements].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
			decl[elements].InstanceDataStepRate = 0;
			elements++;

			decl[elements].SemanticName = "TANGENT";
			decl[elements].SemanticIndex = 0;
			decl[elements].Format = DXGI_FORMAT_R32G32B32_FLOAT;
			decl[elements].InputSlot = 0;
			decl[elements].AlignedByteOffset = (char*)&foo->sdir[0] - (char*)NULL;
			decl[elements].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
			decl[elements].InstanceDataStepRate = 0;
			elements++;

			decl[elements].SemanticName = "BINORMAL";
			decl[elements].SemanticIndex = 0;
			decl[elements].Format = DXGI_FORMAT_R32G32B32_FLOAT;
			decl[elements].InputSlot = 0;
			decl[elements].AlignedByteOffset = (char*)&foo->tdir[0] - (char*)NULL;
			decl[elements].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
			decl[elements].InstanceDataStepRate = 0;
			elements++;

/*
			decl[elements].SemanticName = "BLENDWEIGHT";
			decl[elements].SemanticIndex = 0;
			decl[elements].Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
			decl[elements].InputSlot = 0;
			decl[elements].AlignedByteOffset = 0;
			decl[elements].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
			decl[elements].InstanceDataStepRate = 0;
			elements++;

			decl[elements].SemanticName = "BLENDINDICIES";
			decl[elements].SemanticIndex = 0;
			decl[elements].Format = DXGI_FORMAT_R8G8B8A8_UINT;
			decl[elements].InputSlot = 0;
			decl[elements].AlignedByteOffset = 0;
			decl[elements].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
			decl[elements].InstanceDataStepRate = 0;
			elements++;
*/
			if (FAILED(ID3D11Device_CreateInputLayout(pD3DDev11, decl, elements, ID3DBlob_GetBufferPointer(vcode), ID3DBlob_GetBufferSize(vcode), (ID3D11InputLayout**)&prog->permu[permu].handle.hlsl.layout)))
			{
				Con_Printf("HLSL Shader %s requires unsupported inputs\n", name);
				success = false;
			}
		}

		if (vcode)
			ID3DBlob_Release(vcode);
		if (fcode)
			ID3DBlob_Release(fcode);
	}
	return success;
}
int D3D11Shader_FindUniform(union programhandle_u *h, int type, char *name)
{
	return -1;
}
#endif

