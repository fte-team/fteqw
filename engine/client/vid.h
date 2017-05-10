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

typedef enum
{
	QR_NONE,		//server-style operation (no rendering).
	QR_HEADLESS,	//no window/rendering at all (system tray only)
	QR_OPENGL,		//gl+gles+etc.
	QR_DIRECT3D9,	//
	QR_DIRECT3D11,	//
	QR_SOFTWARE,	//not worth using
	QR_VULKAN,		//
	QR_DIRECT3D12,	//no implementation
	QR_METAL,		//no implementation
	QR_DIRECT3D8	//
} r_qrenderer_t;

typedef struct {
	//you are not allowed to make anything not work if it's not based on these vars...
	int width;
	int height;
	int fullscreen;	//0 = windowed. 1 = fullscreen (mode changes). 2 = borderless+maximized
	qboolean stereo;
	qboolean srgb;
	int bpp;
	int rate;
	int wait;	//-1 = default, 0 = off, 1 = on, 2 = every other
	int multisample;	//for opengl antialiasing (which requires context stuff)
	int triplebuffer;
	char subrenderer[MAX_QPATH];
	struct rendererinfo_s *renderer;
} rendererstate_t;
#ifndef SERVERONLY
extern rendererstate_t currentrendererstate;
#endif

typedef struct vrect_s
{
	float				x,y,width,height;
} vrect_t;
typedef struct
{
	int x;
	int y;
	int width;
	int height;
	int maxheight;	//vid.pixelheight or so
} pxrect_t;

typedef struct
{
	qboolean		activeapp;
	qboolean		isminimized;	//can omit rendering as it won't be seen anyway.
	int				fullbright;		// index of first fullbright color
	int				gammarampsize;		//typically 256. but can be up to 1024 (yay 10-bit hardware that's crippled to only actually use 8)

	unsigned		fbvwidth; /*virtual 2d width of the current framebuffer image*/
	unsigned		fbvheight; /*virtual 2d height*/
	unsigned		fbpwidth; /*physical 2d width of the current framebuffer image*/
	unsigned		fbpheight; /*physical 2d height*/
	struct image_s	*framebuffer; /*the framebuffer fbo (set by democapture)*/

	unsigned		width; /*virtual 2d screen width*/
	unsigned		height; /*virtual 2d screen height*/

	int				numpages;

	unsigned		rotpixelwidth; /*width after rotation in pixels*/
	unsigned		rotpixelheight; /*pixel after rotation in pixels*/
	unsigned		pixelwidth; /*true height in pixels*/
	unsigned		pixelheight; /*true width in pixels*/

	float			dpi_x;
	float			dpi_y;
} viddef_t;

extern	viddef_t	vid;				// global video state

extern unsigned int	d_8to24rgbtable[256];
extern unsigned int	d_8to24bgrtable[256];

#ifdef GLQUAKE
//called when gamma ramps need to be reapplied
qboolean GLVID_ApplyGammaRamps (unsigned int size, unsigned short *ramps);

qboolean GLVID_Init (rendererstate_t *info, unsigned char *palette);
// Called at startup to set up translation tables, takes 256 8 bit RGB values
// the palette data will go away after the call, so it must be copied off if
// the video driver will need it again

void	GLVID_Shutdown (void);
// Called at shutdown

void GLVID_Crashed(void);

void	GLVID_Update (vrect_t *rects);
// flushes the given rectangles from the view buffer to the screen

void GLVID_SwapBuffers(void);
enum uploadfmt;
char *GLVID_GetRGBInfo(int *bytestride, int *truewidth, int *trueheight, enum uploadfmt *fmt);
void GLVID_SetCaption(const char *caption);
#endif
