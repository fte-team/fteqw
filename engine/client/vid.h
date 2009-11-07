/*
Copyright (C) 1996-1997 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/
// vid.h -- video driver defs

#define VID_CBITS	6
#define VID_GRADES	(1 << VID_CBITS)

// a pixel can be one, two, or four bytes
typedef qbyte pixel_t;

typedef enum {QR_NONE, QR_OPENGL, QR_DIRECT3D} r_qrenderer_t;

typedef struct {
	//you are not allowed to make anything not work if it's not based on these vars...
	int width;
	int height;
	qboolean fullscreen;
	int bpp;
	int rate;
	int multisample;	//for opengl antialiasing (which requires context stuff)
	char glrenderer[MAX_QPATH];
	r_qrenderer_t renderer;
} rendererstate_t;

typedef struct vrect_s
{
	int				x,y,width,height;
	struct vrect_s	*pnext;
} vrect_t;

typedef struct
{
	pixel_t			*colormap;		// 256 * VID_GRADES size
	int				fullbright;		// index of first fullbright color

	unsigned		width;		
	unsigned		height;
	float			aspect;		// width / height -- < 0 is taller than wide
	int				numpages;
	int				recalc_refdef;	// if true, recalc vid-based stuff

FTE_DEPRECATED	pixel_t			*conbuffer;
FTE_DEPRECATED	int				conrowbytes;
FTE_DEPRECATED	unsigned		conwidth;
FTE_DEPRECATED	unsigned		conheight;

	unsigned		pixelwidth;
	unsigned		pixelheight;
} viddef_t;

extern	viddef_t	vid;				// global video state

extern unsigned int	d_8to24rgbtable[256];

#ifdef GLQUAKE
void	GLVID_SetPalette (unsigned char *palette);
// called at startup and after any gamma correction

void	GLVID_ShiftPalette (unsigned char *palette);
// called for bonus and pain flashes, and for underwater color changes

qboolean GLVID_Init (rendererstate_t *info, unsigned char *palette);
// Called at startup to set up translation tables, takes 256 8 bit RGB values
// the palette data will go away after the call, so it must be copied off if
// the video driver will need it again

void	GLVID_Shutdown (void);
// Called at shutdown

void GLVID_Crashed(void);

void	GLVID_Update (vrect_t *rects);
// flushes the given rectangles from the view buffer to the screen

int GLVID_SetMode (rendererstate_t *info, unsigned char *palette);
// sets the mode; only used by the Quake engine for resetting to mode 0 (the
// base mode) on memory allocation failures

void GLVID_LockBuffer (void);
void GLVID_UnlockBuffer (void);

int GLVID_ForceUnlockedAndReturnState (void);
void GLVID_ForceLockState (int lk);

qboolean GLVID_Is8bit();

void GLD_BeginDirectRect (int x, int y, qbyte *pbitmap, int width, int height);
void GLD_EndDirectRect (int x, int y, int width, int height);
char *GLVID_GetRGBInfo(int prepadbytes, int *truewidth, int *trueheight);
void GLVID_SetCaption(char *caption);
#endif
