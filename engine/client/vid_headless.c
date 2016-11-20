#include "quakedef.h"
#ifndef SERVERONLY
#include "gl_draw.h"
#include "shader.h"

#ifdef _WIN32
#include "winquake.h"
#include "resource.h"
#else
#include <unistd.h>
#endif

static void Headless_Draw_Init(void)
{
	R2D_Init();
}
static void Headless_Draw_Shutdown(void)
{
	Shader_Shutdown();
}
static void		Headless_IMG_UpdateFiltering	(image_t *imagelist, int filtermip[3], int filterpic[3], int mipcap[2], float anis)
{
}
static qboolean	Headless_IMG_LoadTextureMips	(texid_t tex, struct pendingtextureinfo *mips)
{
	return true;
}
static void		Headless_IMG_DestroyTexture		(texid_t tex)
{
}
static void	Headless_R_Init					(void)
{
}
static void	Headless_R_DeInit					(void)
{
}
static void	Headless_R_RenderView				(void)
{
}
#ifdef _WIN32
//tray icon crap, so the user can still restore the game.
LRESULT CALLBACK HeadlessWndProc(HWND wnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
	extern cvar_t vid_renderer;
	switch(msg)
	{
	case WM_USER:
		switch(LOWORD(lparam))
		{
		case WM_CONTEXTMENU:
		case WM_USER+0:
		case WM_RBUTTONUP:
			if (!Q_strcasecmp(vid_renderer.string, "headless"))
				Cbuf_AddText("vid_renderer \"\";vid_restart\n", RESTRICT_LOCAL);
			else
				Cbuf_AddText("vid_restart\n", RESTRICT_LOCAL);
			break;
		default:
			break;
		}
		return 0;
	default:
		if (WinNT)
			return DefWindowProcW(wnd, msg, wparam, lparam);
		else
			return DefWindowProcA(wnd, msg, wparam, lparam);
	}
}
#endif

static qboolean Headless_VID_Init				(rendererstate_t *info, unsigned char *palette)
{
#ifdef _WIN32
	//tray icon crap, so the user can still restore the game.
	extern HWND	mainwindow;
	extern HINSTANCE	global_hInstance;
	if (WinNT)
	{
		WNDCLASSW wc;
		NOTIFYICONDATAW data;

		//Shell_NotifyIcon requires a window to provide events etc.
		wc.style         = 0;
		wc.lpfnWndProc   = (WNDPROC)HeadlessWndProc;
		wc.cbClsExtra    = 0;
		wc.cbWndExtra    = 0;
		wc.hInstance     = global_hInstance;
		wc.hIcon         = LoadIcon (global_hInstance, MAKEINTRESOURCE (IDI_ICON1));
		wc.hCursor       = LoadCursor (NULL,IDC_ARROW);
		wc.hbrBackground = NULL;
		wc.lpszMenuName  = 0;
		wc.lpszClassName = L"FTEHeadlessClass";
		RegisterClassW(&wc);

		mainwindow = CreateWindowExW(0L, wc.lpszClassName, L"FTE QuakeWorld", 0, 0, 0, 0, 0, NULL, NULL, global_hInstance, NULL);
		data.cbSize = sizeof(data);
		data.hWnd = mainwindow;
		data.uID = 0;
		data.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
		data.uCallbackMessage = WM_USER;
		data.hIcon = wc.hIcon;
		wcscpy(data.szTip, L"Right-click to restore");
		if (pShell_NotifyIconW)
			pShell_NotifyIconW(NIM_ADD, &data);
	}
	else
	{
		WNDCLASSA wc;
		NOTIFYICONDATAA data;

		//Shell_NotifyIcon requires a window to provide events etc.
		wc.style         = 0;
		wc.lpfnWndProc   = (WNDPROC)HeadlessWndProc;
		wc.cbClsExtra    = 0;
		wc.cbWndExtra    = 0;
		wc.hInstance     = global_hInstance;
		wc.hIcon         = LoadIcon (global_hInstance, MAKEINTRESOURCE (IDI_ICON1));
		wc.hCursor       = LoadCursor (NULL,IDC_ARROW);
		wc.hbrBackground = NULL;
		wc.lpszMenuName  = 0;
		wc.lpszClassName = "FTEHeadlessClass";
		RegisterClassA(&wc);

		mainwindow = CreateWindowExA(0L, wc.lpszClassName, "FTE QuakeWorld", 0, 0, 0, 0, 0, NULL, NULL, global_hInstance, NULL);
		data.cbSize = sizeof(data);
		data.hWnd = mainwindow;
		data.uID = 0;
		data.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
		data.uCallbackMessage = WM_USER;
		data.hIcon = wc.hIcon;
		//fixme: proper multibyte
		Q_strncpyz(data.szTip, "Right-click to restore", sizeof(data.szTip));
		Shell_NotifyIconA(NIM_ADD, &data);
	}
#endif

	memset(&sh_config, 0, sizeof(sh_config));
	return true;
}
static void	 Headless_VID_DeInit				(void)
{
#ifdef _WIN32
	//tray icon crap, so the user can still restore the game.
	//FIXME: remove tray icon. win95 won't do this automagically.
	extern HWND	mainwindow;
	DestroyWindow(mainwindow);
	mainwindow = NULL;
#endif
}
static void	Headless_VID_SwapBuffers			(void)
{
}
static qboolean Headless_VID_ApplyGammaRamps		(unsigned int gammarampsize, unsigned short *ramps)
{
	return false;
}
static void	Headless_VID_SetWindowCaption		(const char *msg)
{
}
static char	*Headless_VID_GetRGBInfo			(int *truevidwidth, int *truevidheight, enum uploadfmt *fmt)
{
	*fmt = TF_INVALID;
	*truevidwidth = *truevidheight = 0;
	return NULL;
}
static qboolean	Headless_SCR_UpdateScreen			(void)
{
	if (!cls.timedemo)
	{
#ifdef _WIN32
		Sleep(100);
#else
		usleep(100*1000);
#endif
	}
	return true;
}
static void	Headless_BE_SelectMode	(backendmode_t mode)
{
}
static void	Headless_BE_DrawMesh_List	(shader_t *shader, int nummeshes, struct mesh_s **mesh, struct vbo_s *vbo, struct texnums_s *texnums, unsigned int be_flags)
{
}
static void	Headless_BE_DrawMesh_Single	(shader_t *shader, struct mesh_s *meshchain, struct vbo_s *vbo, unsigned int be_flags)
{
}
static void	Headless_BE_SubmitBatch	(struct batch_s *batch)
{
}
static struct batch_s *Headless_BE_GetTempBatch	(void)
{
	return NULL;
}
static void	Headless_BE_DrawWorld	(struct batch_s **worldbatches)
{
}
static void	Headless_BE_Init	(void)
{
}
static void Headless_BE_GenBrushModelVBO	(struct model_s *mod)
{
}
static void Headless_BE_ClearVBO	(struct vbo_s *vbo)
{
}
static void Headless_BE_UploadAllLightmaps	(void)
{
}
static void Headless_BE_SelectEntity	(struct entity_s *ent)
{
}
static qboolean Headless_BE_SelectDLight	(struct dlight_s *dl, vec3_t colour, vec3_t axis[3], unsigned int lmode)
{
	return false;
}
static void Headless_BE_Scissor	(srect_t *rect)
{
}
static qboolean Headless_BE_LightCullModel	(vec3_t org, struct model_s *model)
{
	return false;
}
static void Headless_BE_VBO_Begin	(vbobctx_t *ctx, size_t maxsize)
{
}
static void Headless_BE_VBO_Data	(vbobctx_t *ctx, void *data, size_t size, vboarray_t *varray)
{
}
static void Headless_BE_VBO_Finish	(vbobctx_t *ctx, void *edata, size_t esize, vboarray_t *earray, void **vbomem, void **ebomem)
{
}
static void Headless_BE_VBO_Destroy	(vboarray_t *vearray, void *mem)
{
}
static void Headless_BE_RenderToTextureUpdate2d	(qboolean destchanged)
{
}

rendererinfo_t headlessrenderer =
{
	"Headless",
	{"headless"},
	QR_HEADLESS,

	Headless_Draw_Init,
	Headless_Draw_Shutdown,
	Headless_IMG_UpdateFiltering,
	Headless_IMG_LoadTextureMips,
	Headless_IMG_DestroyTexture,
	Headless_R_Init,
	Headless_R_DeInit,
	Headless_R_RenderView,
	Headless_VID_Init,
	Headless_VID_DeInit,
	Headless_VID_SwapBuffers,
	Headless_VID_ApplyGammaRamps,
	NULL,
	NULL,
	NULL,
	Headless_VID_SetWindowCaption,
	Headless_VID_GetRGBInfo,
	Headless_SCR_UpdateScreen,
	Headless_BE_SelectMode,
	Headless_BE_DrawMesh_List,
	Headless_BE_DrawMesh_Single,
	Headless_BE_SubmitBatch,
	Headless_BE_GetTempBatch,
	Headless_BE_DrawWorld,
	Headless_BE_Init,
	Headless_BE_GenBrushModelVBO,
	Headless_BE_ClearVBO,
	Headless_BE_UploadAllLightmaps,
	Headless_BE_SelectEntity,
	Headless_BE_SelectDLight,
	Headless_BE_Scissor,
	Headless_BE_LightCullModel,
	Headless_BE_VBO_Begin,
	Headless_BE_VBO_Data,
	Headless_BE_VBO_Finish,
	Headless_BE_VBO_Destroy,
	Headless_BE_RenderToTextureUpdate2d,
	""
};
#endif
