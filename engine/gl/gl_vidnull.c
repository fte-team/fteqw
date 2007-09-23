#include "quakedef.h"

qbyte vid_curpal[768];

#ifdef IN_XFLIP
cvar_t in_xflip = {"in_xflip", "0"};
#endif

qboolean GLVID_Init(rendererstate_t *info, unsigned char *palette)
{
	return false;
}

void GLVID_DeInit()
{
}

void GLVID_ShiftPalette(unsigned char *p)
{
}

void GLVID_SetPalette(unsigned char *palette)
{
}

void Sys_SendKeyEvents(void)
{
}

void GLD_BeginDirectRect(int x, int y, qbyte *pbitmap, int width, int height)
{
}

void GLD_EndDirectRect(int x, int y, int width, int height)
{
}

void GL_BeginRendering (int *x, int *y, int *width, int *height)
{
	*x = *y = 0;
	*width = 640;
	*height = 480;
}

void GL_EndRendering (void)
{
}

void GL_DoSwap(void)
{
}

void GLVID_LockBuffer (void)
{
}

void GLVID_UnlockBuffer (void)
{
}

int GLVID_ForceUnlockedAndReturnState (void)
{
	return 0;
}

void GLVID_ForceLockState (int lk)
{
}

void GLVID_HandlePause (qboolean pause)
{
}

void GLVID_SetCaption(char *text)
{
}

/*** Input ***/

void IN_ReInit(void)
{
}

void IN_Init(void)
{
#ifdef IN_XFLIP
	Cvar_Register (&in_xflip, "Input Controls");
#endif
}

void IN_Shutdown(void)
{
}

void IN_Commands(void)
{
}

void IN_Move(usercmd_t *cmd, int pnum)
{
}

