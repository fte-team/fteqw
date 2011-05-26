#include "quakedef.h"

#ifdef D3DQUAKE
#include "shader.h"
#include "shader.h"
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

#define LPD3DXINCLUDE void *
#define LPD3DXCONSTANTTABLE void *

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
		return;
}

union programhandle_u D3DShader_CreateProgram (char **precompilerconstants, char *vert, char *frag)
{
	union programhandle_u ret;
	D3DXMACRO defines[64];
	LPD3DXBUFFER code = NULL, errors = NULL;
	memset(&ret, 0, sizeof(ret));

	if (pD3DXCompileShader)
	{
		int consts;
		for (consts = 2; precompilerconstants[consts]; consts++)
		{
		}
		if (consts >= sizeof(defines) / sizeof(defines[0]))
			return ret;

		consts = 0;
		defines[consts].Name = NULL;
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

		defines[0].Name = "VERTEX_SHADER";
		if (!FAILED(pD3DXCompileShader(vert, strlen(vert), defines, NULL, "main", "vs_2_0", 0, &code, &errors, NULL)))
		{
			IDirect3DDevice9_CreateVertexShader(pD3DDev9, code->lpVtbl->GetBufferPointer(code), (IDirect3DVertexShader9**)&ret.hlsl.vert);
			code->lpVtbl->Release(code);
		}
		if (errors)
		{
			char *messages = errors->lpVtbl->GetBufferPointer(errors);
			Con_Printf("%s", messages);
			errors->lpVtbl->Release(errors);
		}

		defines[0].Name = "FRAGMENT_SHADER";
		if (!FAILED(pD3DXCompileShader(frag, strlen(frag), defines, NULL, "main", "ps_2_0", 0, &code, &errors, NULL)))
		{
			IDirect3DDevice9_CreatePixelShader(pD3DDev9, code->lpVtbl->GetBufferPointer(code), (IDirect3DPixelShader9**)&ret.hlsl.frag);
			code->lpVtbl->Release(code);
		}
		if (errors)
		{
			char *messages = errors->lpVtbl->GetBufferPointer(errors);
			Con_Printf("%s", messages);
			errors->lpVtbl->Release(errors);
		}
	}

	return ret;
}
#endif