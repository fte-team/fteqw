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



void D3D11Shader_Init(void)
{
	dllfunction_t funcs[] =
	{
		{(void**)&pD3DCompile, "D3DCompileFromMemory"},
		{NULL,NULL}
	};

	if (!shaderlib)
		shaderlib = Sys_LoadLibrary("D3dcompiler_34.dll", funcs);

	if (!shaderlib)
		return;
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
				"};\n"
				"cbuffer fteviewdefs : register(b1)\n"
				"{\n"
					"matrix m_view;\n"
					"matrix m_projection;\n"
					"float3 v_eyepos; float v_time;\n"
				"};\n"
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

qboolean D3D11Shader_CreateProgram (program_t *prog, const char *name, int permu, char **precompilerconstants, char *vert, char *frag)
{
	D3D_SHADER_MACRO defines[64];
	ID3DBlob *vcode = NULL, *fcode = NULL, *errors = NULL;
	qboolean success = false;

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

		for (; *precompilerconstants; precompilerconstants++)
		{
			defines[consts].Name = NULL;
			defines[consts].Definition = NULL;
			consts++;
		}

		defines[consts].Name = NULL;
		defines[consts].Definition = NULL;

		success = true;

		defines[0].Name = "VERTEX_SHADER";
		if (FAILED(pD3DCompile(vert, strlen(vert), name, defines, &myd3dinclude, "main", "vs_4_0", 0, 0, &vcode, &errors)))
			success = false;
		else
		{
			if (FAILED(ID3D11Device_CreateVertexShader(pD3DDev11, vcode->lpVtbl->GetBufferPointer(vcode), vcode->lpVtbl->GetBufferSize(vcode), NULL, (ID3D11VertexShader**)&prog->permu[permu].handle.hlsl.vert)))
				success = false;
		}
		if (errors)
		{
			char *messages = errors->lpVtbl->GetBufferPointer(errors);
			Con_Printf("%s", messages);
			errors->lpVtbl->Release(errors);
		}

		defines[0].Name = "FRAGMENT_SHADER";
		if (FAILED(pD3DCompile(frag, strlen(frag), name, defines, &myd3dinclude, "main", "ps_4_0", 0, 0, &fcode, &errors)))
			success = false;
		else
		{
			if (FAILED(ID3D11Device_CreatePixelShader(pD3DDev11, fcode->lpVtbl->GetBufferPointer(fcode), fcode->lpVtbl->GetBufferSize(fcode), NULL, (ID3D11PixelShader**)&prog->permu[permu].handle.hlsl.frag)))
				success = false;
		}
		if (errors)
		{
			char *messages = errors->lpVtbl->GetBufferPointer(errors);
			Con_Printf("%s", messages);
			errors->lpVtbl->Release(errors);
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
			if (FAILED(ID3D11Device_CreateInputLayout(pD3DDev11, decl, elements, vcode->lpVtbl->GetBufferPointer(vcode), vcode->lpVtbl->GetBufferSize(vcode), (ID3D11InputLayout**)&prog->permu[permu].handle.hlsl.layout)))
			{
				Con_Printf("HLSL Shader %s requires unsupported inputs\n", name);
				success = false;
			}
		}

		if (vcode)
			vcode->lpVtbl->Release(vcode);
		if (fcode)
			fcode->lpVtbl->Release(fcode);
	}
	return success;
}
/*
static int D3D11Shader_FindUniform_(LPD3DXCONSTANTTABLE ct, char *name)
{
	if (ct)
	{
		UINT dc = 1;
		D3DXCONSTANT_DESC d;
		if (!FAILED(ct->lpVtbl->GetConstantDesc(ct, name, &d, &dc)))
			return d.RegisterIndex;
	}
	return -1;
}
*/
int D3D11Shader_FindUniform(union programhandle_u *h, int type, char *name)
{
#if 0
	int offs;

	if (!type || type == 1)
	{
		offs = D3D11Shader_FindUniform_(h->hlsl.ctabv, name);
		if (offs >= 0)
			return offs;
	}
	if (!type || type == 2)
	{
		offs = D3D11Shader_FindUniform_(h->hlsl.ctabf, name);
		if (offs >= 0)
			return offs;
	}
#endif
	return -1;
}
#endif

