#include "quakedef.h"

#ifdef D3DQUAKE
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
dllhandle_t *shaderlib;



void D3DShader_Init(void)
{
	dllfunction_t funcs[] =
	{
		{(void**)&pD3DXCompileShader, "D3DXCompileShader"},
		{NULL,NULL}
	};

	if (!shaderlib)
		shaderlib = Sys_LoadLibrary("d3dx9_32", funcs);
	if (!shaderlib)
		shaderlib = Sys_LoadLibrary("d3dx9_34", funcs);

	if (!shaderlib)
		return;
}

qboolean D3DShader_CreateProgram (program_t *prog, int permu, char **precompilerconstants, char *vert, char *frag)
{
	D3DXMACRO defines[64];
	LPD3DXBUFFER code = NULL, errors = NULL;
	qboolean success = false;

	prog->handle[permu].hlsl.vert = NULL;
	prog->handle[permu].hlsl.frag = NULL;

	if (pD3DXCompileShader)
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
		if (FAILED(pD3DXCompileShader(vert, strlen(vert), defines, NULL, "main", "vs_2_0", 0, &code, &errors, (LPD3DXCONSTANTTABLE*)&prog->handle[permu].hlsl.ctabv)))
			success = false;
		else
		{
			IDirect3DDevice9_CreateVertexShader(pD3DDev9, code->lpVtbl->GetBufferPointer(code), (IDirect3DVertexShader9**)&prog->handle[permu].hlsl.vert);
			code->lpVtbl->Release(code);
		}
		if (errors)
		{
			char *messages = errors->lpVtbl->GetBufferPointer(errors);
			Con_Printf("%s", messages);
			errors->lpVtbl->Release(errors);
		}

		defines[0].Name = "FRAGMENT_SHADER";
		if (FAILED(pD3DXCompileShader(frag, strlen(frag), defines, NULL, "main", "ps_2_0", 0, &code, &errors, (LPD3DXCONSTANTTABLE*)&prog->handle[permu].hlsl.ctabf)))
			success = false;
		else
		{
			IDirect3DDevice9_CreatePixelShader(pD3DDev9, code->lpVtbl->GetBufferPointer(code), (IDirect3DPixelShader9**)&prog->handle[permu].hlsl.frag);
			code->lpVtbl->Release(code);
		}
		if (errors)
		{
			char *messages = errors->lpVtbl->GetBufferPointer(errors);
			Con_Printf("%s", messages);
			errors->lpVtbl->Release(errors);
		}
	}
	return success;
}

static int D3DShader_FindUniform_(LPD3DXCONSTANTTABLE ct, char *name)
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

int D3DShader_FindUniform(union programhandle_u *h, int type, char *name)
{
	int offs;

	if (!type || type == 1)
	{
		offs = D3DShader_FindUniform_(h->hlsl.ctabv, name);
		if (offs >= 0)
			return offs;
	}
	if (!type || type == 2)
	{
		offs = D3DShader_FindUniform_(h->hlsl.ctabf, name);
		if (offs >= 0)
			return offs;
	}

	return -1;
}
#endif