#include "quakedef.h"

#ifdef _WIN32
//not really needed, but nice none-the-less.
#include "winquake.h"
LONG CDAudio_MessageHandler(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	return 1;
}
#endif

void CDAudio_Shutdown(void)
{
}

void CDAudio_Update(void)
{
}

int CDAudio_Init(void)
{
	return -1;
}

void CDAudio_Play(int track, qboolean looping)
{
}

void CDAudio_Stop(void)
{
}

void CDAudio_Pause(void)
{
}

void CDAudio_Resume(void)
{
}
