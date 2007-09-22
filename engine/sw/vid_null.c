#include "quakedef.h"

qbyte vid_curpal[768];

#ifdef IN_XFLIP
cvar_t in_xflip = {"in_xflip", "0"};
#endif

qboolean SWVID_Init(rendererstate_t *info, unsigned char *palette)
{
	return false;
}

void SWVID_ShiftPalette(unsigned char *p)
{
}

void SWVID_SetPalette(unsigned char *palette)
{
}

void SWVID_Shutdown(void)
{
}

void SWVID_Update(vrect_t *rects)
{
}

void Sys_SendKeyEvents(void)
{
}

void SWD_BeginDirectRect(int x, int y, qbyte *pbitmap, int width, int height)
{
}

void SWD_EndDirectRect(int x, int y, int width, int height)
{
}

void SWVID_LockBuffer (void)
{
}

void SWVID_UnlockBuffer (void)
{
}

int SWVID_ForceUnlockedAndReturnState (void)
{
	return 0;
}

void SWVID_ForceLockState (int lk)
{
}

void SWVID_HandlePause (qboolean pause)
{
}

void SWVID_SetCaption(char *text)
{
}

/*** Input ***/

void IN_ReInit(void)
{
}

void IN_Init(void)
{
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

