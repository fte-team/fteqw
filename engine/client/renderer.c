#include "quakedef.h"
#include "winquake.h"
#ifdef RGLQUAKE
#include "gl_draw.h"
#endif
#ifdef SWQUAKE
#include "sw_draw.h"
#endif


qboolean vid_isfullscreen;

//move to headers

void GLR_SetSky (char *name, float rotate, vec3_t axis);
qboolean GLR_CheckSky(void);
void GLR_AddStain(vec3_t org, float red, float green, float blue, float radius);
void GLR_LessenStains(void);

void SWR_SetSky (char *name, float rotate, vec3_t axis);
qboolean SWR_CheckSky(void);
void SWR_AddStain(vec3_t org, float red, float green, float blue, float radius);
void SWR_LessenStains(void);

void GLVID_DeInit (void);
void GLR_DeInit (void);
void GLSCR_DeInit (void);

void SWVID_Shutdown (void);
void SWR_DeInit (void);
void SWSCR_DeInit (void);

int SWR_LightPoint (vec3_t p);
int GLR_LightPoint (vec3_t p);
void GLR_PreNewMap(void);

#define VIDCOMMANDGROUP "Video config"
#define GRAPHICALNICETIES "Graphical Nicaties"	//or eyecandy, which ever you prefer.
#define BULLETENVARS		"BulletenBoard controls"
#define GLRENDEREROPTIONS	"GL Renderer Options"
#define SWRENDEREROPTIONS	"SW Renderer Options"
#define SCREENOPTIONS	"Screen Options"


unsigned short	d_8to16table[256];
unsigned int	d_8to24bgrtable[256];
unsigned int	d_8to24rgbtable[256];
unsigned int	*d_8to32table = d_8to24bgrtable;	//palette lookups while rendering r_pixbytes=4


//

cvar_t	r_drawviewmodel = {"r_drawviewmodel","1"};
cvar_t	r_netgraph = {"r_netgraph","0"};
cvar_t	r_speeds = {"r_speeds","0", NULL, CVAR_CHEAT};
cvar_t	r_waterwarp = {"r_waterwarp","1"};
cvar_t	r_drawentities = {"r_drawentities","1"};
cvar_t	r_fullbright = {"r_fullbright","0", NULL, CVAR_CHEAT};
cvar_t	r_ambient = {"r_ambient", "0", NULL, CVAR_CHEAT};
#if defined(SWQUAKE)
cvar_t	r_draworder = {"r_draworder","0", NULL, CVAR_CHEAT};
cvar_t	r_timegraph = {"r_timegraph","0"};
cvar_t	r_zgraph = {"r_zgraph","0"};
cvar_t	r_graphheight = {"r_graphheight","15"};
cvar_t	r_clearcolor = {"r_clearcolor","218"};
cvar_t	r_aliasstats = {"r_polymodelstats","0"};
cvar_t	r_dspeeds = {"r_dspeeds","0"};
//cvar_t	r_drawflat = {"r_drawflat", "0", NULL, CVAR_CHEAT};
cvar_t	r_reportsurfout = {"r_reportsurfout", "0"};
cvar_t	r_maxsurfs = {"r_maxsurfs", "0"};
cvar_t	r_numsurfs = {"r_numsurfs", "0"};
cvar_t	r_reportedgeout = {"r_reportedgeout", "0"};
cvar_t	r_maxedges = {"r_maxedges", "0"};
cvar_t	r_numedges = {"r_numedges", "0"};
cvar_t	r_aliastransbase = {"r_aliastransbase", "200"};
cvar_t	r_aliastransadj = {"r_aliastransadj", "100"};
cvar_t	d_smooth = {"d_smooth", "0"};
#endif
cvar_t	gl_skyboxdist = {"gl_skyboxdist", "2300"};

cvar_t	r_vertexdlights = {"r_vertexdlights", "1"};

extern	cvar_t	r_dodgytgafiles;

cvar_t	r_nolerp = {"r_nolerp", "0"};
cvar_t	r_nolightdir = {"r_nolightdir", "0"};

cvar_t	r_sirds = {"r_sirds", "0", NULL, CVAR_SEMICHEAT};//whack in a value of 2 and you get easily visible players.

cvar_t	r_loadlits = {"r_loadlit", "1"};

cvar_t r_stains = {"r_stains", "1", NULL, CVAR_ARCHIVE};
cvar_t r_stainfadetime = {"r_stainfadetime", "1"};
cvar_t r_stainfadeammount = {"r_stainfadeammount", "1"};

cvar_t		_windowed_mouse = {"_windowed_mouse","1"};
cvar_t		vid_wait = {"vid_wait","0"};
cvar_t		vid_nopageflip = {"vid_nopageflip","0", NULL, CVAR_ARCHIVE|CVAR_RENDERERLATCH};
cvar_t		_vid_wait_override = {"_vid_wait_override", "0", NULL, CVAR_ARCHIVE|CVAR_RENDERERLATCH};

static cvar_t		vid_stretch = {"vid_stretch","1", NULL, CVAR_ARCHIVE|CVAR_RENDERERLATCH};
//cvar_t		_windowed_mouse = {"_windowed_mouse","1", CVAR_ARCHIVE};
static cvar_t	gl_driver = {"gl_driver","", NULL, CVAR_ARCHIVE|CVAR_RENDERERLATCH};	//opengl library
cvar_t	vid_renderer = {"vid_renderer", "", NULL, CVAR_ARCHIVE|CVAR_RENDERERLATCH};

static cvar_t	vid_bpp = {"vid_bpp", "32", NULL, CVAR_ARCHIVE|CVAR_RENDERERLATCH};
static cvar_t	vid_allow_modex = {"vid_allow_modex", "1", NULL, CVAR_ARCHIVE|CVAR_RENDERERLATCH};
static cvar_t	vid_fullscreen = {"vid_fullscreen", "1", NULL, CVAR_ARCHIVE|CVAR_RENDERERLATCH};
static cvar_t	vid_width = {"vid_width", "640", NULL, CVAR_ARCHIVE|CVAR_RENDERERLATCH};	//more readable defaults to match conwidth/conheight.
static cvar_t	vid_height = {"vid_height", "480", NULL, CVAR_ARCHIVE|CVAR_RENDERERLATCH};
static cvar_t	vid_refreshrate = {"vid_displayfrequency", "0", NULL, CVAR_ARCHIVE|CVAR_RENDERERLATCH};

cvar_t	gl_fontedgeclamp = {"gl_fontedgeclamp", "0"};	//gl blends. Set this to 1 to stop the outside of your conchars from being visible
cvar_t	gl_font = {"gl_font", ""};
cvar_t	gl_conback = {"gl_conback", ""};
cvar_t	gl_smoothfont = {"gl_smoothfont", "1"};
cvar_t	gl_part_flame = {"gl_part_flame", "1"};
cvar_t	gl_part_torch = {"gl_part_torch", "1"};
cvar_t	r_part_rain = {"r_part_rain", "0"};

cvar_t	r_bouncysparks = {"r_bouncysparks", "0"};

cvar_t	r_fullbrightSkins = {"r_fullbrightSkins", "1",	NULL,	CVAR_SEMICHEAT};
cvar_t	r_fb_models	= {"gl_fb_models", "1", NULL, CVAR_SEMICHEAT|CVAR_RENDERERLATCH};	//as it can highlight the gun a little... ooo nooo....
cvar_t	r_fb_bmodels	= {"gl_fb_bmodels", "1", NULL, CVAR_SEMICHEAT|CVAR_RENDERERLATCH};	//as it can highlight the gun a little... ooo nooo....


cvar_t	gl_nocolors = {"gl_nocolors","0"};
cvar_t		gl_load24bit = {"gl_load24bit", "1"};
cvar_t		vid_conwidth = {"vid_conwidth", "640"};
cvar_t		vid_conheight = {"vid_conheight", "480"};
cvar_t		gl_nobind = {"gl_nobind", "0"};
cvar_t		gl_max_size = {"gl_max_size", "1024"};
cvar_t		gl_picmip = {"gl_picmip", "0"};
cvar_t		gl_picmip2d = {"gl_picmip2d", "0"};
cvar_t		r_drawdisk = {"r_drawdisk", "1"};
cvar_t		gl_compress = {"gl_compress", "0"};
cvar_t		gl_savecompressedtex = {"gl_savecompressedtex", "0"};
extern cvar_t gl_dither;
extern	cvar_t	gl_maxdist;

#ifdef SPECULAR
cvar_t		gl_specular = {"gl_specular", "0"};
#endif
cvar_t		gl_waterripples = {"gl_waterripples", "0"};
cvar_t		gl_detail = {"gl_detail", "0", NULL, CVAR_ARCHIVE};
cvar_t		r_shadows = {"r_shadows", "0", NULL, CVAR_ARCHIVE|CVAR_RENDERERLATCH};
cvar_t		r_shadow_realtime_world = {"r_shadow_realtime_world", "0", NULL, CVAR_CHEAT};
cvar_t		r_noaliasshadows = {"r_noaliasshadows", "0", NULL, CVAR_ARCHIVE};
cvar_t		gl_maxshadowlights = {"gl_maxshadowlights", "2", NULL, CVAR_ARCHIVE};
cvar_t		gl_bump = {"gl_bump", "0", NULL, CVAR_ARCHIVE|CVAR_RENDERERLATCH};
cvar_t		gl_lightmapmode = {"gl_lightmapmode", "", NULL, CVAR_ARCHIVE};

cvar_t		gl_ati_truform = {"gl_ati_truform", "0"};
cvar_t		gl_ati_truform_type = {"gl_ati_truform_type", "1"};
cvar_t		gl_ati_truform_tesselation = {"gl_ati_truform_tesselation", "3"};

cvar_t		gl_lateswap = {"gl_lateswap", "0"};

cvar_t			scr_sshot_type = {"scr_sshot_type", "jpg"};


cvar_t			con_height = {"con_height", "50"};
cvar_t          scr_viewsize = {"viewsize","100", NULL, CVAR_ARCHIVE};
cvar_t          scr_fov = {"fov","90", NULL, CVAR_ARCHIVE}; // 10 - 170
cvar_t          scr_conspeed = {"scr_conspeed","300"};
cvar_t          scr_centertime = {"scr_centertime","2"};
cvar_t          scr_showram = {"showram","1"};
cvar_t          scr_showturtle = {"showturtle","0"};
cvar_t          scr_showpause = {"showpause","1"};
cvar_t          scr_printspeed = {"scr_printspeed","8"};
cvar_t			scr_allowsnap = {"scr_allowsnap", "1", NULL, CVAR_NOTFROMSERVER};	//otherwise it would defeat the point.

cvar_t			scr_chatmodecvar = {"scr_chatmode", "0"};

#ifdef Q3SHADERS
cvar_t 			gl_shadeq3 = {"gl_shadeq3", "1"};	//use if you want.
extern cvar_t r_vertexlight;
cvar_t			gl_shadeq1 = {"gl_shadeq1", "0", NULL, CVAR_CHEAT};	//FIXME: :(
cvar_t			gl_shadeq1_name = {"gl_shadeq1_name", "*"};
#endif

cvar_t r_bloodstains = {"r_bloodstains", "1"};

extern	cvar_t	r_norefresh;
extern	cvar_t	r_drawentities;
extern	cvar_t	r_drawworld;
extern	cvar_t	r_drawviewmodel;
extern	cvar_t	r_speeds;
extern	cvar_t	r_waterwarp;
extern	cvar_t	r_fullbright;
extern	cvar_t	r_lightmap;
extern	cvar_t	r_shadows;
extern	cvar_t	r_mirroralpha;
extern	cvar_t	r_wateralpha;
cvar_t	r_dynamic = {"r_dynamic","1"};
cvar_t	r_flashblend = {"gl_flashblend","0"};
extern	cvar_t	r_novis;
extern	cvar_t	r_netgraph;

cvar_t r_drawflat = {"r_drawflat","0", NULL, CVAR_SEMICHEAT};
cvar_t r_wallcolour = {"r_wallcolour","0 0 1"};
cvar_t r_floorcolour = {"r_floorcolour","0.5 0.5 1"};

cvar_t r_transtables = {"r_transtables","2"};
cvar_t r_transtablehalf = {"r_transtablehalf", "1"};
cvar_t r_transtablewrite = {"r_transtablewrite", "1"};
cvar_t r_palconvbits = {"r_palconvbits", "565"};
cvar_t r_palconvwrite = {"r_palconvwrite", "1"};

extern cvar_t bul_text1;
extern cvar_t bul_text2;
extern cvar_t bul_text3;
extern cvar_t bul_text4;
extern cvar_t bul_text5;
extern cvar_t bul_text6;

extern cvar_t bul_scrollspeedx;
extern cvar_t bul_scrollspeedy;
extern cvar_t bul_backcol;
extern cvar_t bul_textpalette;
extern cvar_t bul_norender;
extern cvar_t bul_sparkle;
extern cvar_t bul_forcemode;
extern cvar_t bul_ripplespeed;
extern cvar_t bul_rippleamount;
extern cvar_t bul_nowater;
void R_BulletenForce_f (void);

rendererstate_t currentrendererstate;

cvar_t	gl_skyboxname = {"r_skybox", ""};
cvar_t	r_fastsky = {"r_fastsky", "0"};
cvar_t	r_fastskycolour = {"r_fastskycolour", "0"};

#if defined(RGLQUAKE)
cvar_t gl_schematics = {"gl_schematics","0"};
cvar_t	gl_ztrick = {"gl_ztrick","1"};
cvar_t	gl_lerpimages = {"gl_lerpimages", "1"};
extern cvar_t r_waterlayers;
cvar_t			gl_triplebuffer = {"gl_triplebuffer", "1", NULL, CVAR_ARCHIVE};
cvar_t			gl_subdivide_size = {"gl_subdivide_size", "128", NULL, CVAR_ARCHIVE};
cvar_t			gl_subdivide_water = {"gl_subdivide_water", "0", NULL, CVAR_ARCHIVE};
cvar_t			vid_hardwaregamma = {"vid_hardwaregamma", "1", NULL, CVAR_ARCHIVE};
void GLRenderer_Init(void)
{
	extern cvar_t gl_contrast;
	//screen
	Cvar_Register (&gl_triplebuffer, GLRENDEREROPTIONS);

	Cvar_Register (&vid_hardwaregamma, GLRENDEREROPTIONS);

	//model
	Cvar_Register (&gl_subdivide_size, GLRENDEREROPTIONS);
	Cvar_Register (&gl_subdivide_water, GLRENDEREROPTIONS);

//renderer
	Cvar_Register (&r_novis, GLRENDEREROPTIONS);
	Cvar_Register (&r_wateralpha, GLRENDEREROPTIONS);
	Cvar_Register (&r_mirroralpha, GLRENDEREROPTIONS);
	Cvar_Register (&r_lightmap, GLRENDEREROPTIONS);
	Cvar_Register (&r_norefresh, GLRENDEREROPTIONS);

	Cvar_Register (&r_shadow_realtime_world, GLRENDEREROPTIONS);

	Cvar_Register (&gl_clear, GLRENDEREROPTIONS);
 
	Cvar_Register (&gl_cull, GLRENDEREROPTIONS);
	Cvar_Register (&gl_smoothmodels, GRAPHICALNICETIES);
	Cvar_Register (&gl_affinemodels, GLRENDEREROPTIONS);
	Cvar_Register (&gl_polyblend, GLRENDEREROPTIONS);
	Cvar_Register (&r_flashblend, GLRENDEREROPTIONS);
	Cvar_Register (&gl_playermip, GLRENDEREROPTIONS);
	Cvar_Register (&gl_nocolors, GLRENDEREROPTIONS);
	Cvar_Register (&gl_finish, GLRENDEREROPTIONS);
	Cvar_Register (&gl_lateswap, GLRENDEREROPTIONS);
	Cvar_Register (&gl_lerpimages, GLRENDEREROPTIONS);

	Cvar_Register (&r_shadows, GLRENDEREROPTIONS);
	Cvar_Register (&r_noaliasshadows, GLRENDEREROPTIONS);
	Cvar_Register (&gl_maxshadowlights, GLRENDEREROPTIONS);

	Cvar_Register (&gl_part_flame, GRAPHICALNICETIES);
	Cvar_Register (&gl_part_torch, GRAPHICALNICETIES);

	Cvar_Register (&gl_keeptjunctions, GLRENDEREROPTIONS);
	Cvar_Register (&gl_reporttjunctions, GLRENDEREROPTIONS);

	Cvar_Register (&gl_ztrick, GLRENDEREROPTIONS);

	Cvar_Register (&gl_max_size, GLRENDEREROPTIONS);
	Cvar_Register (&gl_maxdist, GLRENDEREROPTIONS);
	Cvar_Register (&vid_conwidth, GLRENDEREROPTIONS);
	Cvar_Register (&vid_conheight, GLRENDEREROPTIONS);

	Cvar_Register (&gl_fontedgeclamp, GRAPHICALNICETIES);
	Cvar_Register (&gl_font, GRAPHICALNICETIES);
	Cvar_Register (&gl_conback, GRAPHICALNICETIES);
	Cvar_Register (&gl_smoothfont, GRAPHICALNICETIES);

	Cvar_Register (&gl_bump, GRAPHICALNICETIES);
	Cvar_Register (&gl_contrast, GLRENDEREROPTIONS);
#ifdef R_XFLIP
	Cvar_Register (&r_xflip, GLRENDEREROPTIONS);
#endif
	Cvar_Register (&gl_load24bit, GRAPHICALNICETIES);
	Cvar_Register (&gl_specular, GRAPHICALNICETIES);

	Cvar_Register (&gl_lightmapmode, GLRENDEREROPTIONS);

#ifdef WATERLAYERS
	Cvar_Register (&r_waterlayers, GRAPHICALNICETIES);
#endif
	Cvar_Register (&gl_waterripples, GRAPHICALNICETIES);

	Cvar_Register (&gl_nobind, GLRENDEREROPTIONS);
	Cvar_Register (&gl_max_size, GLRENDEREROPTIONS);
	Cvar_Register (&gl_picmip, GLRENDEREROPTIONS);
	Cvar_Register (&gl_picmip2d, GLRENDEREROPTIONS);
	Cvar_Register (&r_drawdisk, GLRENDEREROPTIONS);

	Cvar_Register (&gl_savecompressedtex, GLRENDEREROPTIONS);
	Cvar_Register (&gl_compress, GLRENDEREROPTIONS);
	Cvar_Register (&gl_driver, GLRENDEREROPTIONS);
	Cvar_Register (&gl_detail, GRAPHICALNICETIES);
	Cvar_Register (&gl_dither, GRAPHICALNICETIES);
	Cvar_Register (&r_fb_models, GRAPHICALNICETIES);
	Cvar_Register (&r_fb_bmodels, GRAPHICALNICETIES);

	Cvar_Register (&gl_ati_truform, GRAPHICALNICETIES);
	Cvar_Register (&gl_ati_truform_type, GRAPHICALNICETIES);
	Cvar_Register (&gl_ati_truform_tesselation, GRAPHICALNICETIES);

	Cvar_Register (&gl_skyboxdist, GLRENDEREROPTIONS);

	Cvar_Register (&r_wallcolour, GLRENDEREROPTIONS);
	Cvar_Register (&r_floorcolour, GLRENDEREROPTIONS);
	Cvar_Register (&r_vertexdlights, GLRENDEREROPTIONS);

	Cvar_Register (&gl_schematics, GLRENDEREROPTIONS);
#ifdef Q3SHADERS
	Cvar_Register (&r_vertexlight, GLRENDEREROPTIONS);
	Cvar_Register (&gl_shadeq1, GLRENDEREROPTIONS);
	Cvar_Register (&gl_shadeq1_name, GLRENDEREROPTIONS);
	Cvar_Register (&gl_shadeq3, GLRENDEREROPTIONS);
#endif
}
#endif
#if defined(SWQUAKE)
extern cvar_t d_subdiv16;
extern cvar_t d_mipcap;
extern cvar_t d_mipscale;
extern cvar_t d_smooth;
void SWRenderer_Init(void)
{
	Cvar_Register (&d_subdiv16, SWRENDEREROPTIONS);
	Cvar_Register (&d_mipcap, SWRENDEREROPTIONS);
	Cvar_Register (&d_mipscale, SWRENDEREROPTIONS);
	Cvar_Register (&d_smooth, SWRENDEREROPTIONS);

	Cvar_Register (&r_maxsurfs, SWRENDEREROPTIONS);
	Cvar_Register (&r_numsurfs, SWRENDEREROPTIONS);
	Cvar_Register (&r_maxedges, SWRENDEREROPTIONS);
	Cvar_Register (&r_numedges, SWRENDEREROPTIONS);
	Cvar_Register (&r_sirds, SWRENDEREROPTIONS);

	Cvar_Register (&r_aliastransbase, SWRENDEREROPTIONS);
	Cvar_Register (&r_aliastransadj, SWRENDEREROPTIONS);
	Cvar_Register (&r_reportedgeout, SWRENDEREROPTIONS);
	Cvar_Register (&r_aliasstats, SWRENDEREROPTIONS);
	Cvar_Register (&r_clearcolor, SWRENDEREROPTIONS);

	Cvar_Register (&r_timegraph, SWRENDEREROPTIONS);
	Cvar_Register (&r_draworder, SWRENDEREROPTIONS);
	Cvar_Register (&r_zgraph, SWRENDEREROPTIONS);
	Cvar_Register (&r_graphheight, SWRENDEREROPTIONS);
	Cvar_Register (&r_aliasstats, SWRENDEREROPTIONS);
	Cvar_Register (&r_dspeeds, SWRENDEREROPTIONS);
	Cvar_Register (&r_ambient, SWRENDEREROPTIONS);
	Cvar_Register (&r_ambient, SWRENDEREROPTIONS);
	Cvar_Register (&r_reportsurfout, SWRENDEREROPTIONS);

	Cvar_Register (&r_transtables, SWRENDEREROPTIONS);
	Cvar_Register (&r_transtablewrite, SWRENDEREROPTIONS);
	Cvar_Register (&r_transtablehalf, SWRENDEREROPTIONS);
	Cvar_Register (&r_palconvbits, SWRENDEREROPTIONS);
	Cvar_Register (&r_palconvwrite, SWRENDEREROPTIONS);
}
#endif


void	R_InitTextures (void)
{
	int		x,y, m;
	qbyte	*dest;
	
// create a simple checkerboard texture for the default
	r_notexture_mip = BZ_Malloc (sizeof(texture_t) + 16*16+8*8+4*4+2*2);
	
	r_notexture_mip->pixbytes = 1;
	r_notexture_mip->width = r_notexture_mip->height = 16;
	r_notexture_mip->offsets[0] = sizeof(texture_t);
	r_notexture_mip->offsets[1] = r_notexture_mip->offsets[0] + 16*16;
	r_notexture_mip->offsets[2] = r_notexture_mip->offsets[1] + 8*8;
	r_notexture_mip->offsets[3] = r_notexture_mip->offsets[2] + 4*4;
	
	for (m=0 ; m<4 ; m++)
	{
		dest = (qbyte *)r_notexture_mip + r_notexture_mip->offsets[m];
		for (y=0 ; y< (16>>m) ; y++)
			for (x=0 ; x< (16>>m) ; x++)
			{
				if (  (y< (8>>m) ) ^ (x< (8>>m) ) )
					*dest++ = 0;
				else
					*dest++ = 0xff;
			}
	}	
}



void R_SetRenderer_f (void);

void Renderer_Init(void)
{
	currentrendererstate.bpp = -1;	//no previous.

	currentrendererstate.renderer = -1;

	Cmd_AddCommand("setrenderer", R_SetRenderer_f);
	Cmd_AddCommand("vid_restart", R_RestartRenderer_f);

#if defined(RGLQUAKE)
	GLRenderer_Init();
#endif
#if defined(SWQUAKE)
	SWRenderer_Init();
#endif


	//but register ALL vid_ commands.
	Cvar_Register (&vid_wait, VIDCOMMANDGROUP);
	Cvar_Register (&vid_nopageflip, VIDCOMMANDGROUP);
	Cvar_Register (&_vid_wait_override, VIDCOMMANDGROUP);
	Cvar_Register (&vid_stretch, VIDCOMMANDGROUP);
	Cvar_Register (&_windowed_mouse, VIDCOMMANDGROUP);
	Cvar_Register (&vid_renderer, VIDCOMMANDGROUP);

	Cvar_Register (&_windowed_mouse, VIDCOMMANDGROUP);
	Cvar_Register (&vid_fullscreen, VIDCOMMANDGROUP);
//	Cvar_Register (&vid_stretch, VIDCOMMANDGROUP);
	Cvar_Register (&vid_bpp, VIDCOMMANDGROUP);

	Cvar_Register (&vid_allow_modex, VIDCOMMANDGROUP);

	Cvar_Register (&vid_width, VIDCOMMANDGROUP);
	Cvar_Register (&vid_height, VIDCOMMANDGROUP);
	Cvar_Register (&vid_refreshrate, VIDCOMMANDGROUP);

	Cvar_Register (&gl_skyboxname, GRAPHICALNICETIES);

	Cvar_Register(&r_dodgytgafiles, "Bug fixes");
	Cvar_Register(&r_loadlits, GRAPHICALNICETIES);

	Cvar_Register(&r_stains, GRAPHICALNICETIES);
	Cvar_Register(&r_stainfadetime, GRAPHICALNICETIES);
	Cvar_Register(&r_stainfadeammount, GRAPHICALNICETIES);

	Cvar_Register(&scr_viewsize, SCREENOPTIONS);
	Cvar_Register(&scr_fov, SCREENOPTIONS);
	Cvar_Register(&scr_conspeed, SCREENOPTIONS);
	Cvar_Register(&scr_centertime, SCREENOPTIONS);
	Cvar_Register(&scr_showram, SCREENOPTIONS);
	Cvar_Register(&scr_showturtle, SCREENOPTIONS);
	Cvar_Register(&scr_showpause, SCREENOPTIONS);
	Cvar_Register(&scr_printspeed, SCREENOPTIONS);
	Cvar_Register(&scr_allowsnap, SCREENOPTIONS);
	Cvar_Register(&scr_chatmodecvar, SCREENOPTIONS);

	Cvar_Register (&scr_sshot_type, SCREENOPTIONS);


//screen
	Cvar_Register (&scr_fov, SCREENOPTIONS);
	Cvar_Register (&scr_viewsize, SCREENOPTIONS);
	Cvar_Register (&scr_conspeed, SCREENOPTIONS);
	Cvar_Register (&scr_showram, SCREENOPTIONS);
	Cvar_Register (&scr_showturtle, SCREENOPTIONS);
	Cvar_Register (&scr_showpause, SCREENOPTIONS);
	Cvar_Register (&scr_centertime, SCREENOPTIONS);
	Cvar_Register (&scr_printspeed, SCREENOPTIONS);
	Cvar_Register (&scr_allowsnap, SCREENOPTIONS);
	Cvar_Register (&con_height, SCREENOPTIONS);

	Cvar_Register(&r_bloodstains, GRAPHICALNICETIES);

	Cvar_Register(&r_fullbrightSkins, GRAPHICALNICETIES);


//renderer
	Cvar_Register (&r_fullbright, SCREENOPTIONS);
	Cvar_Register (&r_drawentities, GRAPHICALNICETIES);
	Cvar_Register (&r_drawviewmodel, GRAPHICALNICETIES);
	Cvar_Register (&r_waterwarp, GRAPHICALNICETIES);
	Cvar_Register (&r_speeds, SCREENOPTIONS);
	Cvar_Register (&r_netgraph, SCREENOPTIONS);

	Cvar_Register (&r_dynamic, GRAPHICALNICETIES);

	Cvar_Register (&r_nolerp, GRAPHICALNICETIES);
	Cvar_Register (&r_nolightdir, GRAPHICALNICETIES);

	Cvar_Register (&r_fastsky, GRAPHICALNICETIES);
	Cvar_Register (&r_fastskycolour, GRAPHICALNICETIES);

	Cvar_Register (&r_drawflat, GRAPHICALNICETIES);

//bulletens
	Cvar_Register(&bul_nowater, BULLETENVARS);
	Cvar_Register(&bul_rippleamount, BULLETENVARS);
	Cvar_Register(&bul_ripplespeed, BULLETENVARS);
	Cvar_Register(&bul_forcemode, BULLETENVARS);
	Cvar_Register(&bul_sparkle, BULLETENVARS);
	Cvar_Register(&bul_textpalette, BULLETENVARS);
	Cvar_Register(&bul_scrollspeedy, BULLETENVARS);
	Cvar_Register(&bul_scrollspeedx, BULLETENVARS);
	Cvar_Register(&bul_backcol, BULLETENVARS);

	Cvar_Register(&bul_text6,	BULLETENVARS);	//reverse order, to get forwards ordered console vars.
	Cvar_Register(&bul_text5,	BULLETENVARS);
	Cvar_Register(&bul_text4,	BULLETENVARS);
	Cvar_Register(&bul_text3,	BULLETENVARS);
	Cvar_Register(&bul_text2,	BULLETENVARS);
	Cvar_Register(&bul_text1,	BULLETENVARS);

	Cvar_Register(&bul_norender,	BULLETENVARS);	//find this one first...

	Cmd_AddCommand("bul_make",	R_BulletenForce_f);	

	R_InitParticles();
	R_InitTextures();
	RQ_Init();
}


mpic_t	*(*Draw_PicFromWad)			(char *name);
mpic_t	*(*Draw_SafePicFromWad)		(char *name);
mpic_t	*(*Draw_CachePic)			(char *path);
mpic_t	*(*Draw_SafeCachePic)		(char *path);
void	(*Draw_Init)				(void);
void	(*Draw_ReInit)				(void);
void	(*Draw_Character)			(int x, int y, unsigned int num);
void	(*Draw_ColouredCharacter)	(int x, int y, unsigned int num);
void	(*Draw_String)				(int x, int y, const qbyte *str);
void	(*Draw_Alt_String)			(int x, int y, const qbyte *str);
void	(*Draw_Crosshair)			(void);
void	(*Draw_DebugChar)			(qbyte num);
void	(*Draw_Pic)					(int x, int y, mpic_t *pic);
void	(*Draw_ScalePic)			(int x, int y, int width, int height, mpic_t *pic);
void	(*Draw_SubPic)				(int x, int y, mpic_t *pic, int srcx, int srcy, int width, int height);
void	(*Draw_TransPic)			(int x, int y, mpic_t *pic);
void	(*Draw_TransPicTranslate)	(int x, int y, int w, int h, qbyte *image, qbyte *translation);
void	(*Draw_ConsoleBackground)	(int lines);
void	(*Draw_EditorBackground)	(int lines);
void	(*Draw_TileClear)			(int x, int y, int w, int h);
void	(*Draw_Fill)				(int x, int y, int w, int h, int c);
void	(*Draw_FadeScreen)			(void);
void	(*Draw_BeginDisc)			(void);
void	(*Draw_EndDisc)				(void);

void	(*Draw_Image)							(float x, float y, float w, float h, float s1, float t1, float s2, float t2, mpic_t *pic);	//gl-style scaled/coloured/subpic 
void	(*Draw_ImageColours)		(float r, float g, float b, float a);

void	(*R_Init)					(void);
void	(*R_DeInit)					(void);
void	(*R_ReInit)					(void);
void	(*R_RenderView)				(void);		// must set r_refdef first

void	(*R_InitSky)				(struct texture_s *mt);	// called at level load
qboolean	(*R_CheckSky)			(void);
void	(*R_SetSky)					(char *name, float rotate, vec3_t axis);

void	(*R_NewMap)					(void);
void	(*R_PreNewMap)				(void);
int		(*R_LightPoint)				(vec3_t point);

void	(*R_PushDlights)			(void);
void	(*R_AddStain)				(vec3_t org, float red, float green, float blue, float radius);
void	(*R_LessenStains)			(void);

void (*Media_ShowFrameBGR_24_Flip)	(qbyte *framedata, int inwidth, int inheight);	//input is bottom up...
void (*Media_ShowFrameRGBA_32)		(qbyte *framedata, int inwidth, int inheight);	//top down
void (*Media_ShowFrame8bit)			(qbyte *framedata, int inwidth, int inheight, qbyte *palette);	//paletted topdown (framedata is 8bit indexes into palette)

void	(*Mod_Init)					(void);
void	(*Mod_ClearAll)				(void);
struct model_s *(*Mod_ForName)		(char *name, qboolean crash);
struct model_s *(*Mod_FindName)		(char *name);
void	*(*Mod_Extradata)			(struct model_s *mod);	// handles caching
void	(*Mod_TouchModel)			(char *name);

struct mleaf_s *(*Mod_PointInLeaf)	(float *p, struct model_s *model);
qbyte	*(*Mod_Q1LeafPVS)			(struct mleaf_s *leaf, struct model_s *model, qbyte *buffer);
void	(*Mod_NowLoadExternal)		(void);
void	(*Mod_Think)				(void);
void	(*Mod_GetTag)				(struct model_s *model, int tagnum, int frame, float **org, float **axis);
int (*Mod_TagNumForName)			(struct model_s *model, char *name);



qboolean (*VID_Init)				(rendererstate_t *info, unsigned char *palette);
void	 (*VID_DeInit)				(void);
void	(*VID_HandlePause)			(qboolean pause);
void	(*VID_LockBuffer)			(void);
void	(*VID_UnlockBuffer)			(void);
void	(*D_BeginDirectRect)		(int x, int y, qbyte *pbitmap, int width, int height);
void	(*D_EndDirectRect)			(int x, int y, int width, int height);
void	(*VID_ForceLockState)		(int lk);
int		(*VID_ForceUnlockedAndReturnState) (void);
void	(*VID_SetPalette)			(unsigned char *palette);
void	(*VID_ShiftPalette)			(unsigned char *palette);
char	*(*VID_GetRGBInfo)			(int prepad, int *truevidwidth, int *truevidheight);
void	(*VID_SetWindowCaption)		(char *msg);

void	(*SCR_UpdateScreen)			(void);

r_qrenderer_t qrenderer=QR_NONE;
char *q_renderername = "Non-Selected renderer";



struct {
	char *name[4];
	r_qrenderer_t rtype;

	mpic_t	*(*Draw_PicFromWad)			(char *name);
	mpic_t	*(*Draw_SafePicFromWad)			(char *name);
	mpic_t	*(*Draw_CachePic)			(char *path);
	mpic_t	*(*Draw_SafeCachePic)		(char *path);
	void	(*Draw_Init)				(void);
	void	(*Draw_ReInit)				(void);
	void	(*Draw_Character)			(int x, int y, unsigned int num);
	void	(*Draw_ColouredCharacter)	(int x, int y, unsigned int num);
	void	(*Draw_String)				(int x, int y, const qbyte *str);
	void	(*Draw_Alt_String)			(int x, int y, const qbyte *str);
	void	(*Draw_Crosshair)			(void);
	void	(*Draw_DebugChar)			(qbyte num);
	void	(*Draw_Pic)					(int x, int y, mpic_t *pic);
	void	(*Draw_ScalePic)			(int x, int y, int width, int height, mpic_t *pic);
	void	(*Draw_SubPic)				(int x, int y, mpic_t *pic, int srcx, int srcy, int width, int height);
	void	(*Draw_TransPic)			(int x, int y, mpic_t *pic);
	void	(*Draw_TransPicTranslate)	(int x, int y, int w, int h, qbyte *pic, qbyte *translation);
	void	(*Draw_ConsoleBackground)	(int lines);
	void	(*Draw_EditorBackground)	(int lines);
	void	(*Draw_TileClear)			(int x, int y, int w, int h);
	void	(*Draw_Fill)				(int x, int y, int w, int h, int c);
	void	(*Draw_FadeScreen)			(void);
	void	(*Draw_BeginDisc)			(void);
	void	(*Draw_EndDisc)				(void);

	void	(*Draw_Image)				(float x, float y, float w, float h, float s1, float t1, float s2, float t2, mpic_t *pic);	//gl-style scaled/coloured/subpic 
	void	(*Draw_ImageColours)		(float r, float g, float b, float a);

	void	(*R_Init)					(void);
	void	(*R_DeInit)					(void);
	void	(*R_ReInit)					(void);
	void	(*R_RenderView)				(void);		// must set r_refdef first

	void	(*R_InitSky)				(struct texture_s *mt);	// called at level load
	qboolean	(*R_CheckSky)			(void);
	void	(*R_SetSky)					(char *name, float rotate, vec3_t axis);

	void	(*R_NewMap)					(void);
	void	(*R_PreNewMap)				(void);
	int		(*R_LightPoint)				(vec3_t point);

	void	(*R_PushDlights)			(void);
	void	(*R_AddStain)				(vec3_t org, float red, float green, float blue, float radius);
	void	(*R_LessenStains)			(void);

	void (*Media_ShowFrameBGR_24_Flip)	(qbyte *framedata, int inwidth, int inheight);	//input is bottom up...
	void (*Media_ShowFrameRGBA_32)		(qbyte *framedata, int inwidth, int inheight);	//top down
	void (*Media_ShowFrame8bit)			(qbyte *framedata, int inwidth, int inheight, qbyte *palette);	//paletted topdown (framedata is 8bit indexes into palette)

	void	(*Mod_Init)					(void);
	void	(*Mod_ClearAll)				(void);
	struct model_s *(*Mod_ForName)		(char *name, qboolean crash);
	struct model_s *(*Mod_FindName)		(char *name);
	void	*(*Mod_Extradata)			(struct model_s *mod);	// handles caching
	void	(*Mod_TouchModel)			(char *name);

	struct mleaf_s *(*Mod_PointInLeaf)	(float *p, struct model_s *model);
	qbyte	*(*Mod_Q1LeafPVS)			(struct mleaf_s *leaf, struct model_s *model, qbyte *buffer);
	void	(*Mod_NowLoadExternal)		(void);
	void	(*Mod_Think)				(void);
	void	(*Mod_GetTag)				(struct model_s *model, int tagnum, int frame, float **org, float **axis);
	int (*Mod_TagNumForName)			(struct model_s *model, char *name);


	qboolean (*VID_Init)				(rendererstate_t *info, unsigned char *palette);
	void	 (*VID_DeInit)				(void);
	void	(*VID_HandlePause)			(qboolean pause);
	void	(*VID_LockBuffer)			(void);
	void	(*VID_UnlockBuffer)			(void);
	void	(*D_BeginDirectRect)		(int x, int y, qbyte *pbitmap, int width, int height);
	void	(*D_EndDirectRect)			(int x, int y, int width, int height);
	void	(*VID_ForceLockState)		(int lk);
	int		(*VID_ForceUnlockedAndReturnState) (void);
	void	(*VID_SetPalette)			(unsigned char *palette);
	void	(*VID_ShiftPalette)			(unsigned char *palette);
	char	*(*VID_GetRGBInfo)			(int prepad, int *truevidwidth, int *truevidheight);
	void	(*VID_SetWindowCaption)		(char *msg);

	void	(*SCR_UpdateScreen)			(void);

	char *alignment;
} rendererinfo[] = {
	{	//ALL builds need a 'none' renderer, as 0.
		{
			"none",
			"dedicated",
			"terminal",
			"sw"
		},
		QR_NONE,




		NULL,	//Draw_PicFromWad;
		NULL,	//Draw_PicFromWad;	//Not supported
		NULL,	//Draw_CachePic;
		NULL,	//Draw_SafeCachePic;
		NULL,	//Draw_Init;
		NULL,	//Draw_Init;
		NULL,	//Draw_Character;
		NULL,	//Draw_ColouredCharacter;
		NULL,	//Draw_String;
		NULL,	//Draw_Alt_String;
		NULL,	//Draw_Crosshair;
		NULL,	//Draw_DebugChar;
		NULL,	//Draw_Pic;
		NULL,	//Draw_SubPic;
		NULL,	//Draw_TransPic;
		NULL,	//Draw_TransPicTranslate;
		NULL,	//Draw_ConsoleBackground;
		NULL,	//Draw_EditorBackground;
		NULL,	//Draw_TileClear;
		NULL,	//Draw_Fill;
		NULL,	//Draw_FadeScreen;
		NULL,	//Draw_BeginDisc;
		NULL,	//Draw_EndDisc;
		NULL,	//I'm lazy.

		NULL,	//Draw_Image
		NULL,	//Draw_ImageColours

		NULL,	//R_Init;
		NULL,	//R_DeInit;
		NULL,	//R_ReInit;
		NULL,	//R_RenderView;
		NULL,	//R_InitSky;
		NULL,	//R_CheckSky;
		NULL,	//R_SetSky;

		NULL,	//R_NewMap;
		NULL,	//R_PreNewMap
		NULL,	//R_LightPoint;
		NULL,	//R_PushDlights;


		NULL,	//R_AddStain;
		NULL,	//R_LessenStains;

		NULL,	//Media_ShowFrameBGR_24_Flip;
		NULL,	//Media_ShowFrameRGBA_32;
		NULL,	//Media_ShowFrame8bit;

#ifdef SWQUAKE
		SWMod_Init,
		SWMod_ClearAll,
		SWMod_ForName,
		SWMod_FindName,
		SWMod_Extradata,
		SWMod_TouchModel,

		SWMod_PointInLeaf,
		SWMod_LeafPVS,
		SWMod_NowLoadExternal,
		SWMod_Think,
#elif defined(RGLQUAKE)
		GLMod_Init,
		GLMod_ClearAll,
		GLMod_ForName,
		GLMod_FindName,
		GLMod_Extradata,
		GLMod_TouchModel,

		GLMod_PointInLeaf,
		GLMod_LeafPVS,
		GLMod_NowLoadExternal,
		GLMod_Think,
#else
		#error "No renderer in client build"
#endif

		NULL, //Mod_GetTag
		NULL, //fixme: server will need this one at some point.

		NULL, //VID_Init,
		NULL, //VID_DeInit,
		NULL, //VID_HandlePause,
		NULL, //VID_LockBuffer,
		NULL, //VID_UnlockBuffer,
		NULL, //D_BeginDirectRect,
		NULL, //D_EndDirectRect,
		NULL, //VID_ForceLockState,
		NULL, //VID_ForceUnlockedAndReturnState,
		NULL, //VID_SetPalette,
		NULL, //VID_ShiftPalette,
		NULL, //VID_GetRGBInfo,


		NULL,	//set caption

		NULL,	//SCR_UpdateScreen;

		""
	}
#ifdef SWQUAKE
	,
	{
		{
			"sw",
			"software",
		},
		QR_SOFTWARE,
		
		SWDraw_PicFromWad,
		SWDraw_PicFromWad,	//Not supported
		SWDraw_CachePic,
		SWDraw_SafeCachePic,
		SWDraw_Init,
		SWDraw_Init,
		SWDraw_Character,
		SWDraw_ColouredCharacter,
		SWDraw_String,
		SWDraw_Alt_String,
		SWDraw_Crosshair,
		SWDraw_DebugChar,
		SWDraw_Pic,
		NULL,//SWDraw_ScaledPic,
		SWDraw_SubPic,
		SWDraw_TransPic,
		SWDraw_TransPicTranslate,
		SWDraw_ConsoleBackground,
		SWDraw_EditorBackground,
		SWDraw_TileClear,
		SWDraw_Fill,
		SWDraw_FadeScreen,
		SWDraw_BeginDisc,
		SWDraw_EndDisc,

		SWDraw_Image,
		SWDraw_ImageColours,

		SWR_Init,
		SWR_DeInit,
		NULL,//SWR_ReInit,
		SWR_RenderView,

		SWR_InitSky,
		SWR_CheckSky,
		SWR_SetSky,

		SWR_NewMap,
		NULL,
		SWR_LightPoint,
		SWR_PushDlights,

		SWR_AddStain,
		SWR_LessenStains,

		MediaSW_ShowFrameBGR_24_Flip,
		MediaSW_ShowFrameRGBA_32,
		MediaSW_ShowFrame8bit,

		SWMod_Init,
		SWMod_ClearAll,
		SWMod_ForName,
		SWMod_FindName,
		SWMod_Extradata,
		SWMod_TouchModel,

		SWMod_PointInLeaf,
		SWMod_LeafPVS,
		SWMod_NowLoadExternal,
		SWMod_Think,

		NULL,	//Mod_GetTag
		NULL,	//Mod_TagForName

		SWVID_Init,
		SWVID_Shutdown,
		SWVID_HandlePause,
		SWVID_LockBuffer,
		SWVID_UnlockBuffer,
		SWD_BeginDirectRect,
		SWD_EndDirectRect,
		SWVID_ForceLockState,
		SWVID_ForceUnlockedAndReturnState,
		SWVID_SetPalette,
		SWVID_ShiftPalette,
		SWVID_GetRGBInfo,

		NULL,
		
		SWSCR_UpdateScreen,

		""
	}
#else
	,
	{
		{
			NULL
		}
	}
#endif
#ifdef RGLQUAKE
	,
	{
		{
			"gl",
			"opengl",
			"hardware",
		},
		QR_SOFTWARE,
		

		GLDraw_PicFromWad,
		GLDraw_SafePicFromWad,
		GLDraw_CachePic,
		GLDraw_SafeCachePic,
		GLDraw_Init,
		GLDraw_ReInit,
		GLDraw_Character,
		GLDraw_ColouredCharacter,
		GLDraw_String,
		GLDraw_Alt_String,
		GLDraw_Crosshair,
		GLDraw_DebugChar,
		GLDraw_Pic,
		GLDraw_ScalePic,
		GLDraw_SubPic,
		GLDraw_TransPic,
		GLDraw_TransPicTranslate,
		GLDraw_ConsoleBackground,
		GLDraw_EditorBackground,
		GLDraw_TileClear,
		GLDraw_Fill,
		GLDraw_FadeScreen,
		GLDraw_BeginDisc,
		GLDraw_EndDisc,

		GLDraw_Image,
		GLDraw_ImageColours,

		GLR_Init,
		GLR_DeInit,
		GLR_ReInit,
		GLR_RenderView,


		GLR_InitSky,
		GLR_CheckSky,
		GLR_SetSky,

		GLR_NewMap,
		GLR_PreNewMap,
		GLR_LightPoint,
		GLR_PushDlights,


		GLR_AddStain,
		GLR_LessenStains,

		MediaGL_ShowFrameBGR_24_Flip,
		MediaGL_ShowFrameRGBA_32,
		MediaGL_ShowFrame8bit,
	

		GLMod_Init,
		GLMod_ClearAll,
		GLMod_ForName,
		GLMod_FindName,
		GLMod_Extradata,
		GLMod_TouchModel,

		GLMod_PointInLeaf,
		GLMod_LeafPVS,
		GLMod_NowLoadExternal,
		GLMod_Think,

		GLMod_GetTag,
		GLMod_TagNumForName,

		GLVID_Init,
		GLVID_DeInit,
		GLVID_HandlePause,
		GLVID_LockBuffer,
		GLVID_UnlockBuffer,
		GLD_BeginDirectRect,
		GLD_EndDirectRect,
		GLVID_ForceLockState,
		GLVID_ForceUnlockedAndReturnState,
		GLVID_SetPalette,
		GLVID_ShiftPalette,
		GLVID_GetRGBInfo,

		NULL,	//setcaption

	
		GLSCR_UpdateScreen,

		""
	}
#else
	,
	{
		{
		NULL
		}
	}
#endif
};





typedef struct vidmode_s
{
	const char *description;
	int         width, height;
} vidmode_t;

vidmode_t vid_modes[] =
{
	{ "320x200",   320, 200},
	{ "320x240",   320, 240},
	{ "400x300",   400, 300},
	{ "512x384",   512, 384},
	{ "640x480",   640, 480},
	{ "800x600",   800, 600},
	{ "960x720",   960, 720},
	{ "1024x768",  1024, 768},
	{ "1152x864",  1152, 864},
	{ "1280x960",  1280, 960},
	{ "1600x1200", 1600, 1200},	//sw height is bound to 200 to 1024
	{ "2048x1536", 2048, 1536}	//too much width will disable water warping (>1280) (but at that resolution, it's almost unnoticable)
};
#define NUMVIDMODES sizeof(vid_modes)/sizeof(vid_modes[0])


typedef struct {
	menucombo_t *renderer;
	menucombo_t *modecombo;
	menucombo_t *conscalecombo;
	menuedit_t *customwidth;
	menuedit_t *customheight;
} videomenuinfo_t;

menuedit_t *MC_AddEdit(menu_t *menu, int x, int y, char *text, char *def);
void CheckCustomMode(struct menu_s *menu)
{
	videomenuinfo_t *info = menu->data;
	if (info->modecombo->selectedoption && info->conscalecombo->selectedoption)
	{	//hide the custom options
		info->customwidth->common.ishidden = true;
		info->customheight->common.ishidden = true;
	}
	else
	{
		info->customwidth->common.ishidden = false;
		info->customheight->common.ishidden = false;
	}

#ifdef SWQUAKE
	if (info->renderer->selectedoption < 2)
		info->conscalecombo->common.ishidden = true;
	else
#endif
		info->conscalecombo->common.ishidden = false;
}
qboolean M_VideoApply (union menuoption_s *op,struct menu_s *menu,int key)
{
	videomenuinfo_t *info = menu->data;
	if (key != K_ENTER)
		return false;

	if (info->modecombo->selectedoption)
	{	//set a prefab
		Cbuf_AddText(va("vid_width %i\n", vid_modes[info->modecombo->selectedoption-1].width), RESTRICT_LOCAL);
		Cbuf_AddText(va("vid_height %i\n", vid_modes[info->modecombo->selectedoption-1].height), RESTRICT_LOCAL);
	}
	else
	{	//use the custom one
		Cbuf_AddText(va("vid_width %s\n", info->customwidth->text), RESTRICT_LOCAL);
		Cbuf_AddText(va("vid_height %s\n", info->customheight->text), RESTRICT_LOCAL);
	}
	
	if (info->conscalecombo->selectedoption)	//I am aware that this handicaps the menu a bit, but it should be easier for n00bs.
	{	//set a prefab
		Cbuf_AddText(va("vid_conwidth %i\n", vid_modes[info->conscalecombo->selectedoption-1].width), RESTRICT_LOCAL);
		Cbuf_AddText(va("vid_conheight %i\n", vid_modes[info->conscalecombo->selectedoption-1].height), RESTRICT_LOCAL);
	}
	else
	{	//use the custom one
		Cbuf_AddText(va("vid_conwidth %s\n", info->customwidth->text), RESTRICT_LOCAL);
		Cbuf_AddText(va("vid_conheight %s\n", info->customheight->text), RESTRICT_LOCAL);
	}

	switch(info->renderer->selectedoption)
	{
#ifdef SWQUAKE
	case 0:
		Cbuf_AddText("setrenderer sw 8\n", RESTRICT_LOCAL);
		break;
	case 1:
		Cbuf_AddText("setrenderer sw 32\n", RESTRICT_LOCAL);
		break;
	case 2:
#else
	case 0:
#endif
		Cbuf_AddText("setrenderer gl\n", RESTRICT_LOCAL);
		break;
#ifdef SWQUAKE
	case 3:
#else
	case 1:
#endif
		Cbuf_AddText("setrenderer d3d\n", RESTRICT_LOCAL);
		break;
	}
	M_RemoveMenu(menu);
	Cbuf_AddText("menu_video\n", RESTRICT_LOCAL);
	return true;
}
void M_Menu_Video_f (void)
{
	extern cvar_t r_stains, v_contrast;
#if defined(SWQUAKE)
	extern cvar_t d_smooth;
#endif
	extern cvar_t r_bouncysparks;
	static const char *modenames[128] = {"Custom"};
	static const char *rendererops[] = {
#ifdef SWQUAKE
		"8bit Software",
		"32bit Software",
#endif
#ifdef RGLQUAKE
		"OpenGL",
#ifdef AVAIL_DX7
		"Direct3D",
#endif
#endif
		NULL
	};
	videomenuinfo_t *info;
	menu_t *menu;
	int prefabmode;
	int prefab2dmode;

	int i, y;
	prefabmode = -1;
	prefab2dmode = -1;
	for (i = 0; i < sizeof(vid_modes)/sizeof(vidmode_t); i++)
	{
		if (vid_modes[i].width == vid_width.value && vid_modes[i].height == vid_height.value)
			prefabmode = i;
		if (vid_modes[i].width == vid_conwidth.value && vid_modes[i].height == vid_conheight.value)
			prefab2dmode = i;
		modenames[i+1] = vid_modes[i].description;
	}
	modenames[i+1] = NULL;

	key_dest = key_menu;
	m_state = m_complex;
	m_entersound = true;

	menu = M_CreateMenu(sizeof(videomenuinfo_t));
	info = menu->data;

#ifdef SWQUAKE
	if (qrenderer == QR_SOFTWARE && vid_bpp.value >= 32)
		i = 1;
	else
#endif
#if defined(SWQUAKE) && defined(RGLQUAKE)
	if (qrenderer == QR_OPENGL)
	{
#ifdef AVAIL_DX7
		if (!strcmp(vid_renderer.string, "d3d"))
			i = 3;
		else
#endif
			i = 2;
	}
	else
#endif
#if defined(RGLQUAKE) && defined(AVAIL_DX7)
		 if (!strcmp(vid_renderer.string, "d3d"))
		i = 1;
	else
#endif
		i = 0;

	MC_AddCenterPicture(menu, 4, "vidmodes");

	y = 32;
	info->renderer = MC_AddCombo(menu,	16, y,				"   Renderer     ", rendererops, i);	y+=8;
	info->modecombo = MC_AddCombo(menu,	16, y,					"   Video Size   ", modenames, prefabmode+1);	y+=8;
	info->conscalecombo = MC_AddCombo(menu,	16, y,					"      2d Size   ", modenames, prefab2dmode+1);	y+=8;
	MC_AddCheckBox(menu,	16, y,							"   Fullscreen   ", &vid_fullscreen,0);	y+=8;
	y+=4;info->customwidth = MC_AddEdit(menu, 16, y,		"   Custom width ", vid_width.string);	y+=8;
	y+=4;info->customheight = MC_AddEdit(menu, 16, y,		"   Custom height", vid_height.string);	y+=12;
	y+=8;
	MC_AddCommand(menu,	16, y,								"           Apply", M_VideoApply);	y+=8;
	y+=8;
	MC_AddCheckBox(menu,	16, y,							"      Stain maps", &r_stains,0);	y+=8;
	MC_AddCheckBox(menu,	16, y,							"   Bouncy sparks", &r_bouncysparks,0);	y+=8;
	MC_AddCheckBox(menu,	16, y,							"            Rain", &r_part_rain,0);	y+=8;
#if defined(SWQUAKE)
	MC_AddCheckBox(menu,	16, y,							"    SW Smoothing", &d_smooth,0);	y+=8;
#endif
#ifdef RGLQUAKE
	MC_AddCheckBox(menu,	16, y,							"  GL Bumpmapping", &gl_bump,0);	y+=8;
#endif
	MC_AddCheckBox(menu,	16, y,							"  Dynamic lights", &r_dynamic,0);	y+=8;
	MC_AddSlider(menu,	16, y,								"     Screen size", &scr_viewsize,	30,		120);y+=8;
	MC_AddSlider(menu,	16, y,								"           Gamma", &v_gamma, 0.3, 1);	y+=8;
	MC_AddSlider(menu,	16, y,								"        Contrast", &v_contrast, 1, 3);	y+=8;

	menu->cursoritem = (menuoption_t*)MC_AddWhiteText(menu, 152, 32, NULL, false);
	menu->selecteditem = (union menuoption_s *)info->renderer;
	menu->event = CheckCustomMode;
}


void R_SetRenderer(int wanted)
{
	q_renderername = rendererinfo[wanted].name[0];

	Draw_PicFromWad			= rendererinfo[wanted].Draw_PicFromWad;
	Draw_SafePicFromWad		= rendererinfo[wanted].Draw_SafePicFromWad;	//Not supported
	Draw_CachePic			= rendererinfo[wanted].Draw_CachePic;
	Draw_SafeCachePic		= rendererinfo[wanted].Draw_SafeCachePic;
	Draw_Init				= rendererinfo[wanted].Draw_Init;
	Draw_ReInit				= rendererinfo[wanted].Draw_Init;
	Draw_Character			= rendererinfo[wanted].Draw_Character;
	Draw_ColouredCharacter	= rendererinfo[wanted].Draw_ColouredCharacter;
	Draw_String				= rendererinfo[wanted].Draw_String;
	Draw_Alt_String			= rendererinfo[wanted].Draw_Alt_String;
	Draw_Crosshair			= rendererinfo[wanted].Draw_Crosshair;
	Draw_DebugChar			= rendererinfo[wanted].Draw_DebugChar;
	Draw_Pic				= rendererinfo[wanted].Draw_Pic;
	Draw_SubPic				= rendererinfo[wanted].Draw_SubPic;
	Draw_TransPic			= rendererinfo[wanted].Draw_TransPic;
	Draw_TransPicTranslate	= rendererinfo[wanted].Draw_TransPicTranslate;
	Draw_ConsoleBackground	= rendererinfo[wanted].Draw_ConsoleBackground;
	Draw_EditorBackground	= rendererinfo[wanted].Draw_EditorBackground;
	Draw_TileClear			= rendererinfo[wanted].Draw_TileClear;
	Draw_Fill				= rendererinfo[wanted].Draw_Fill;
	Draw_FadeScreen			= rendererinfo[wanted].Draw_FadeScreen;
	Draw_BeginDisc			= rendererinfo[wanted].Draw_BeginDisc;
	Draw_EndDisc			= rendererinfo[wanted].Draw_EndDisc;
	Draw_ScalePic			= rendererinfo[wanted].Draw_ScalePic;

	Draw_Image				= rendererinfo[wanted].Draw_Image;
	Draw_ImageColours 		= rendererinfo[wanted].Draw_ImageColours;

	R_Init					= rendererinfo[wanted].R_Init;
	R_DeInit				= rendererinfo[wanted].R_DeInit;
	R_RenderView			= rendererinfo[wanted].R_RenderView;
	R_NewMap				= rendererinfo[wanted].R_NewMap;
	R_PreNewMap				= rendererinfo[wanted].R_PreNewMap;
	R_LightPoint			= rendererinfo[wanted].R_LightPoint;
	R_PushDlights			= rendererinfo[wanted].R_PushDlights;
	R_InitSky				= rendererinfo[wanted].R_InitSky;
	R_CheckSky				= rendererinfo[wanted].R_CheckSky;
	R_SetSky				= rendererinfo[wanted].R_SetSky;

	R_AddStain				= rendererinfo[wanted].R_AddStain;
	R_LessenStains			= rendererinfo[wanted].R_LessenStains;

	VID_Init				= rendererinfo[wanted].VID_Init;
	VID_DeInit				= rendererinfo[wanted].VID_DeInit;
	VID_HandlePause			= rendererinfo[wanted].VID_HandlePause;
	VID_LockBuffer			= rendererinfo[wanted].VID_LockBuffer;
	VID_UnlockBuffer		= rendererinfo[wanted].VID_UnlockBuffer;
	D_BeginDirectRect		= rendererinfo[wanted].D_BeginDirectRect;
	D_EndDirectRect			= rendererinfo[wanted].D_EndDirectRect;
	VID_ForceLockState		= rendererinfo[wanted].VID_ForceLockState;
	VID_ForceUnlockedAndReturnState	= rendererinfo[wanted].VID_ForceUnlockedAndReturnState;
	VID_SetPalette			= rendererinfo[wanted].VID_SetPalette;
	VID_ShiftPalette		= rendererinfo[wanted].VID_ShiftPalette;
	VID_GetRGBInfo			= rendererinfo[wanted].VID_GetRGBInfo;

	Media_ShowFrame8bit			= rendererinfo[wanted].Media_ShowFrame8bit;
	Media_ShowFrameRGBA_32		= rendererinfo[wanted].Media_ShowFrameRGBA_32;
	Media_ShowFrameBGR_24_Flip	= rendererinfo[wanted].Media_ShowFrameBGR_24_Flip;

	Mod_Init				= rendererinfo[wanted].Mod_Init;
	Mod_Think				= rendererinfo[wanted].Mod_Think;
	Mod_ClearAll			= rendererinfo[wanted].Mod_ClearAll;
	Mod_ForName				= rendererinfo[wanted].Mod_ForName;
	Mod_FindName			= rendererinfo[wanted].Mod_FindName;
	Mod_Extradata			= rendererinfo[wanted].Mod_Extradata;
	Mod_TouchModel			= rendererinfo[wanted].Mod_TouchModel;

	Mod_PointInLeaf			= rendererinfo[wanted].Mod_PointInLeaf;
	Mod_Q1LeafPVS			= rendererinfo[wanted].Mod_Q1LeafPVS;
	Mod_NowLoadExternal		= rendererinfo[wanted].Mod_NowLoadExternal;

	Mod_GetTag				= rendererinfo[wanted].Mod_GetTag;
	Mod_TagNumForName 		= rendererinfo[wanted].Mod_TagNumForName;


	
	SCR_UpdateScreen		= rendererinfo[wanted].SCR_UpdateScreen;

	qrenderer = wanted;
}

static qbyte default_quakepal[768] =
{
0,0,0,15,15,15,31,31,31,47,47,47,63,63,63,75,75,75,91,91,91,107,107,107,123,123,123,139,139,139,155,155,155,171,171,171,187,187,187,203,203,203,219,219,219,235,235,235,15,11,7,23,15,11,31,23,11,39,27,15,47,35,19,55,43,23,63,47,23,75,55,27,83,59,27,91,67,31,99,75,31,107,83,31,115,87,31,123,95,35,131,103,35,143,111,35,11,11,15,19,19,27,27,27,39,39,39,51,47,47,63,55,55,75,63,63,87,71,71,103,79,79,115,91,91,127,99,99,
139,107,107,151,115,115,163,123,123,175,131,131,187,139,139,203,0,0,0,7,7,0,11,11,0,19,19,0,27,27,0,35,35,0,43,43,7,47,47,7,55,55,7,63,63,7,71,71,7,75,75,11,83,83,11,91,91,11,99,99,11,107,107,15,7,0,0,15,0,0,23,0,0,31,0,0,39,0,0,47,0,0,55,0,0,63,0,0,71,0,0,79,0,0,87,0,0,95,0,0,103,0,0,111,0,0,119,0,0,127,0,0,19,19,0,27,27,0,35,35,0,47,43,0,55,47,0,67,
55,0,75,59,7,87,67,7,95,71,7,107,75,11,119,83,15,131,87,19,139,91,19,151,95,27,163,99,31,175,103,35,35,19,7,47,23,11,59,31,15,75,35,19,87,43,23,99,47,31,115,55,35,127,59,43,143,67,51,159,79,51,175,99,47,191,119,47,207,143,43,223,171,39,239,203,31,255,243,27,11,7,0,27,19,0,43,35,15,55,43,19,71,51,27,83,55,35,99,63,43,111,71,51,127,83,63,139,95,71,155,107,83,167,123,95,183,135,107,195,147,123,211,163,139,227,179,151,
171,139,163,159,127,151,147,115,135,139,103,123,127,91,111,119,83,99,107,75,87,95,63,75,87,55,67,75,47,55,67,39,47,55,31,35,43,23,27,35,19,19,23,11,11,15,7,7,187,115,159,175,107,143,163,95,131,151,87,119,139,79,107,127,75,95,115,67,83,107,59,75,95,51,63,83,43,55,71,35,43,59,31,35,47,23,27,35,19,19,23,11,11,15,7,7,219,195,187,203,179,167,191,163,155,175,151,139,163,135,123,151,123,111,135,111,95,123,99,83,107,87,71,95,75,59,83,63,
51,67,51,39,55,43,31,39,31,23,27,19,15,15,11,7,111,131,123,103,123,111,95,115,103,87,107,95,79,99,87,71,91,79,63,83,71,55,75,63,47,67,55,43,59,47,35,51,39,31,43,31,23,35,23,15,27,19,11,19,11,7,11,7,255,243,27,239,223,23,219,203,19,203,183,15,187,167,15,171,151,11,155,131,7,139,115,7,123,99,7,107,83,0,91,71,0,75,55,0,59,43,0,43,31,0,27,15,0,11,7,0,0,0,255,11,11,239,19,19,223,27,27,207,35,35,191,43,
43,175,47,47,159,47,47,143,47,47,127,47,47,111,47,47,95,43,43,79,35,35,63,27,27,47,19,19,31,11,11,15,43,0,0,59,0,0,75,7,0,95,7,0,111,15,0,127,23,7,147,31,7,163,39,11,183,51,15,195,75,27,207,99,43,219,127,59,227,151,79,231,171,95,239,191,119,247,211,139,167,123,59,183,155,55,199,195,55,231,227,87,127,191,255,171,231,255,215,255,255,103,0,0,139,0,0,179,0,0,215,0,0,255,0,0,255,243,147,255,247,199,255,255,255,159,91,83
};
qbyte default_conchar[11356] =
{
#include "lhfont.h"
};

qboolean R_ApplyRenderer (rendererstate_t *newr)
{
	int i, j;
	extern model_t *loadmodel;
	extern int host_hunklevel;

	if (newr->bpp == -1)
		return false;

	CL_AllowIndependantSendCmd(false);	//FIXME: figure out exactly which parts are going to affect the model loading.

	IN_Shutdown();

	if (R_DeInit)
	{
		TRACE(("dbg: R_ApplyRenderer: R_DeInit\n"));
		R_DeInit();
	}

	if (VID_DeInit)
	{
		TRACE(("dbg: R_ApplyRenderer: VID_DeInit\n"));
		VID_DeInit();
	}

	TRACE(("dbg: R_ApplyRenderer: SCR_DeInit\n"));
	SCR_DeInit();

	COM_FlushTempoaryPacks();

	S_Shutdown();

	if (qrenderer == QR_NONE || qrenderer==-1)
	{
		if (newr->renderer == QR_NONE && qrenderer != -1)
			return true;	//no point

		Sys_CloseTerminal ();
	}

	R_SetRenderer(newr->renderer);

	Cache_Flush();

	Hunk_FreeToLowMark(host_hunklevel);	//is this a good idea?

	TRACE(("dbg: R_ApplyRenderer: old renderer closed\n"));

	gl_skyboxname.modified = true;

	pmove.numphysent = 0;

	if (qrenderer)	//graphics stuff only when not dedicated
	{
		qbyte *data;
#ifndef CLIENTONLY
		isDedicated = false;
#endif
		v_gamma.modified = true;	//force the gamma to be reset

		Con_Printf("Setting mode %i*%i*%i*%i\n", newr->width, newr->height, newr->bpp, newr->rate);

		if (host_basepal)
			BZ_Free(host_basepal);
		host_basepal = (qbyte *)COM_LoadMallocFile ("gfx/palette.lmp");
		if (!host_basepal)
		{
			qbyte *pcx=NULL;
			host_basepal = BZ_Malloc(768);
			pcx = COM_LoadTempFile("pics/colormap.pcx");
			if (!pcx || !ReadPCXPalette(pcx, com_filesize, host_basepal))
			{
				memcpy(host_basepal, default_quakepal, 768);
			}
		}
		if (host_colormap)
			BZ_Free(host_colormap);
		host_colormap = (qbyte *)COM_LoadMallocFile ("gfx/colormap.lmp");
		if (!host_colormap)
		{
#ifdef SWQUAKE
			float f;
			data = host_colormap = BZ_Malloc(256*VID_GRADES+sizeof(int));
			//let's try making one. this is probably caused by running out of baseq2.
			for (j = 0; j < VID_GRADES; j++)
			{
				f = 1 - ((float)j/VID_GRADES);
				for (i = 0; i < 256-vid.fullbright; i++)
				{
					data[i] = GetPalette(host_basepal[i*3+0]*f, host_basepal[i*3+1]*f, host_basepal[i*3+2]*f);
				}
				for (; i < 256; i++)
					data[i] = i;
				data+=256;
			}
#endif		//glquake doesn't really care.

			vid.fullbright=0;
		}
		else
		{
			j = VID_GRADES-1;
			data = host_colormap + j*256;
			vid.fullbright=0;
			for (i = 255; i >= 0; i--)
			{
				if (host_colormap[i] == data[i])
					vid.fullbright++;
				else
					break;
			}
		}

		if (vid.fullbright < 2)
			vid.fullbright = 0;	//transparent colour doesn't count.

TRACE(("dbg: R_ApplyRenderer: Palette loaded\n"));

		if (!VID_Init(newr, host_basepal))
		{
			return false;
		}
TRACE(("dbg: R_ApplyRenderer: vid applied\n"));

#ifdef RGLQUAKE
		if (qrenderer == QR_OPENGL)
			GLV_UpdatePalette();
#endif

TRACE(("dbg: R_ApplyRenderer: done palette\n"));

		v_gamma.modified = true;	//force the gamma to be reset
		W_LoadWadFile("gfx.wad");
TRACE(("dbg: R_ApplyRenderer: wad loaded\n"));
		Draw_Init();
TRACE(("dbg: R_ApplyRenderer: draw inited\n"));
		R_Init();
TRACE(("dbg: R_ApplyRenderer: renderer inited\n"));
		SCR_Init();
TRACE(("dbg: R_ApplyRenderer: screen inited\n"));
		Sbar_Flush();

		IN_Init();
	}
	else
	{
#ifdef CLIENTONLY
		Host_Error("Tried setting dedicated mode\n");
#else
TRACE(("dbg: R_ApplyRenderer: isDedicated = true\n"));
		isDedicated = true;
		if (cls.state)
		{
			int os = sv.state;
			sv.state = ss_dead;	//prevents server from being killed off too.
			CL_Disconnect();
			sv.state = os;
		}
		Sys_InitTerminal();
		Con_PrintToSys();
#endif
	}
TRACE(("dbg: R_ApplyRenderer: initing mods\n"));
	Mod_Init();
TRACE(("dbg: R_ApplyRenderer: initing bulletein boards\n"));
	WipeBulletenTextures();

//	host_hunklevel = Hunk_LowMark();

	if (R_PreNewMap)
	if (cl.worldmodel)
	{
		TRACE(("dbg: R_ApplyRenderer: R_PreNewMap (how handy)\n"));
		R_PreNewMap();
	}

#ifndef CLIENTONLY
	if (sv.worldmodel)
	{
		edict_t *ent;
#ifdef Q2SERVER
		q2edict_t *q2ent;
#endif

TRACE(("dbg: R_ApplyRenderer: reloading server map\n"));
		sv.worldmodel = Mod_ForName (sv.modelname, false);
TRACE(("dbg: R_ApplyRenderer: loaded\n"));
		if (sv.worldmodel->needload)
		{
			SV_Error("Bsp went missing on render restart\n");
		}
TRACE(("dbg: R_ApplyRenderer: doing that funky phs thang\n"));
		SV_CalcPHS ();

		for (i = 0; i < MAX_MODELS; i++)
		{
			if (*sv.model_precache[i] && (!strcmp(sv.model_precache[i] + strlen(sv.model_precache[i]) - 4, ".bsp") || i-1 < sv.worldmodel->numsubmodels))
				sv.models[i] = Mod_FindName(sv.model_precache[i]);
			else
				sv.models[i] = NULL;
		}
TRACE(("dbg: R_ApplyRenderer: clearing world\n"));
		SV_ClearWorld ();

		if (svprogfuncs)
		{
			ent = sv.edicts;
			ent->v.model = PR_SetString(svprogfuncs, sv.worldmodel->name);	//FIXME: is this a problem for normal ents?
			for (i=0 ; i<sv.num_edicts ; i++)
			{
				ent = EDICT_NUM(svprogfuncs, i);
				if (!ent)
					continue;
				if (ent->isfree)
					continue;

				if (ent->area.prev)
				{
					ent->area.prev = ent->area.next = NULL;
					SV_LinkEdict (ent, false);	// relink ents so touch functions continue to work.
				}
			}
		}
#ifdef Q2SERVER
		else if (ge && ge->edicts)
		{
			q2ent = ge->edicts;
			
			for (i=0 ; i<ge->num_edicts ; i++, q2ent = (q2edict_t *)((char *)q2ent + ge->edict_size))
			{
				if (!q2ent)
					continue;
				if (!q2ent->inuse)
					continue;

				if (q2ent->area.prev)
				{
					q2ent->area.prev = q2ent->area.next = NULL;
					SVQ2_LinkEdict (q2ent);	// relink ents so touch functions continue to work.
				}
			}
		}
#endif
	}
#endif
#ifdef PLUGINS
	Plug_ResChanged();
#endif

TRACE(("dbg: R_ApplyRenderer: starting on client state\n"));
	if (cl.worldmodel)
	{
		int staticmodelindex[MAX_STATIC_ENTITIES];		

		for (i = 0; i < cl.num_statics; i++)	//static entities contain pointers to the model index.
		{
			staticmodelindex[i] = 0;
			for (j = 1; j < MAX_MODELS; j++)
				if (cl_static_entities[i].model == cl.model_precache[j])
					staticmodelindex[i] = j;
		}

		cl.worldmodel = NULL;
		cl_numvisedicts=0;
TRACE(("dbg: R_ApplyRenderer: reloading ALL models\n"));
		for (i=1 ; i<MAX_MODELS ; i++)
		{
			if (!cl.model_name[i][0])
				break;

			cl.model_precache[i] = NULL;
			TRACE(("dbg: R_ApplyRenderer: reloading model %s\n", cl.model_name[i]));
			cl.model_precache[i] = Mod_ForName (cl.model_name[i], false);

			if (!cl.model_precache[i])
			{
				Con_Printf ("\nThe required model file '%s' could not be found or downloaded.\n\n"
					, cl.model_name[i]);
				Con_Printf ("You may need to download or purchase a client "
					"pack in order to play on this server.\n\n");
				CL_Disconnect ();
				UI_Reset();
				return false;
			}

			S_ExtraUpdate();
		}

		loadmodel = cl.worldmodel = cl.model_precache[1];
TRACE(("dbg: R_ApplyRenderer: done the models\n"));
		if (loadmodel->needload)
		{
				CL_Disconnect ();
				UI_Reset();
				memcpy(&currentrendererstate, newr, sizeof(currentrendererstate));
				return true;
		}

TRACE(("dbg: R_ApplyRenderer: checking any wad textures\n"));
		Mod_NowLoadExternal();
TRACE(("dbg: R_ApplyRenderer: R_NewMap\n"));
		R_NewMap();
TRACE(("dbg: R_ApplyRenderer: efrags\n"));
		for (i = 0; i < cl.num_statics; i++)	//make the static entities reappear.
		{
			cl_static_entities[i].model = cl.model_precache[staticmodelindex[i]];
			if (staticmodelindex[i])	//make sure it's worthwhile.
			{
				R_AddEfrags(&cl_static_entities[i]);
			}
		}
	}
	else
		UI_Reset();

	switch (qrenderer)
	{
	case QR_NONE:
		Con_Printf(	"\n"
					"-----------------------------\n"
					"Dedicated console created\n");
		break;

	case QR_SOFTWARE:
		Con_Printf(	"\n"
					"-----------------------------\n"
					"Software renderer initialized\n");
		break;

	case QR_OPENGL:
		Con_Printf(	"\n"
					"-----------------------------\n"
					"OpenGL renderer initialized\n");
		break;
	}
	
	if (!isDedicated)
		S_Restart_f();

	memcpy(&currentrendererstate, newr, sizeof(currentrendererstate));
	return true;
}

void R_RestartRenderer_f (void)
{
	rendererstate_t oldr;
	rendererstate_t newr;
#ifdef MENU_DAT
	MP_Shutdown();
#endif
	memset(&newr, 0, sizeof(newr));

TRACE(("dbg: R_RestartRenderer_f\n"));

	Media_CaptureDemoEnd();

	Cvar_ApplyLatches(CVAR_RENDERERLATCH);

	newr.width = vid_width.value;
	newr.height = vid_height.value;

	newr.allow_modex = vid_allow_modex.value;

	newr.bpp = vid_bpp.value;
	newr.fullscreen = vid_fullscreen.value;
	newr.rate = vid_refreshrate.value;
	Q_strncpyz(newr.glrenderer, gl_driver.string, sizeof(newr.glrenderer));

	if (!*vid_renderer.string)
	{
		//gotta do this after main hunk is saved off.
#if defined(RGLQUAKE) && defined(SWQUAKE)
		Cmd_ExecuteString("setrenderer sw 8\n", RESTRICT_LOCAL);
		Cbuf_AddText("menu_video\n", RESTRICT_LOCAL);

#elif defined(RGLQUAKE)
		Cmd_ExecuteString("setrenderer gl\n", RESTRICT_LOCAL);
#else
		Cmd_ExecuteString("setrenderer sw\n", RESTRICT_LOCAL);
#endif
		return;
	}

#ifdef SWQUAKE
	if (!stricmp(vid_renderer.string, "sw") || !stricmp(vid_renderer.string, "software"))
		newr.renderer = QR_SOFTWARE;
	else
#endif
#ifdef RGLQUAKE
		if (!stricmp(vid_renderer.string, "gl") || !stricmp(vid_renderer.string, "opengl"))
		newr.renderer = QR_OPENGL;
	else
#endif
#if defined(RGLQUAKE) && defined(AVAIL_DX7)
		if (!stricmp(vid_renderer.string, "d3d") || !stricmp(vid_renderer.string, "dx"))
	{
		newr.renderer = QR_OPENGL;	//direct3d is done via a gl->d3d wrapper.
		Q_strncpyz(newr.glrenderer, "d3d", sizeof(newr.glrenderer));
	}
	else
#endif
#ifndef CLIENTONLY
		if (!stricmp(vid_renderer.string, "sv") || !stricmp(vid_renderer.string, "dedicated"))
		newr.renderer = QR_NONE;
	else
#endif
#if defined(SWQUAKE)
		newr.renderer = QR_SOFTWARE;
#elif defined(RGLQUAKE)
		newr.renderer = QR_OPENGL;
#else
#error "no default renderer"
#endif

	TRACE(("dbg: R_RestartRenderer_f renderer %i\n", newr.renderer));

	memcpy(&oldr, &currentrendererstate, sizeof(rendererstate_t));
	if (!R_ApplyRenderer(&newr))
	{
		TRACE(("dbg: R_RestartRenderer_f failed\n"));
		if (R_ApplyRenderer(&oldr))
		{
			TRACE(("dbg: R_RestartRenderer_f old restored\n"));
			Con_Printf("^1Video mode switch failed. Old mode restored.\n");	//go back to the old mode, the new one failed.
		}
		else
		{
			newr.renderer = QR_NONE;
			if (R_ApplyRenderer(&newr))
			{
				TRACE(("dbg: R_RestartRenderer_f going to dedicated\n"));
				Con_Printf("\n================================\n");
				Con_Printf("^1Video mode switch failed. Old mode wasn't supported either. Console forced.\nChange vid_mode to a compatable mode, and then use the setrenderer command.\n");
				Con_Printf("================================\n\n");
			}
			else
				Sys_Error("Couldn't fall back to previous renderer\n");
		}
	}
	SCR_EndLoadingPlaque();

	TRACE(("dbg: R_RestartRenderer_f success\n"));
#ifdef MENU_DAT
	MP_Init();
#endif
}

void R_SetRenderer_f (void)
{
	if (!strcmp(Cmd_Argv(1), "help"))
	{
		Con_Printf ("\nValid commands are:\n"
#ifdef SWQUAKE
					"%s SW 8 will set 8bit software rendering\n"
					"%s SW 32 will set 32 bit software rendering\n"
#endif //SWQUAKE
#ifdef RGLQUAKE
					"%s GL will use the default OpenGL on your pc\n"
					"%s GL 3dfxgl will use a 3dfx minidriver (not supplied)\n"
	#ifdef AVAIL_DX7
					"%s D3D will use direct3d rendering\n"
	#endif
#endif
					"\n"
#ifdef SWQUAKE
					,Cmd_Argv(0),Cmd_Argv(0)
#endif
#ifdef RGLQUAKE
					,Cmd_Argv(0),Cmd_Argv(0)
	#ifdef AVAIL_DX7
					,Cmd_Argv(0)
	#endif
#endif		
					);
		return;
	}
	else if (!stricmp(Cmd_Argv(1), "dedicated"))
	{
		Cvar_Set(&vid_renderer, "sv");
		R_RestartRenderer_f();
	}
	else if (!stricmp(Cmd_Argv(1), "SW") || !stricmp(Cmd_Argv(1), "Software"))
	{
#ifndef SWQUAKE
		Con_Printf("Software rendering is not supported in this binary\n");
#else
		if (Cmd_Argc() >= 3)	//set vid_use32bit accordingly.
		{
			switch(atoi(Cmd_Argv(2)))
			{
			default:
				Con_Printf ("The parameter you specified is not linked to the software renderer.");
				return;
			case 32:
				Cvar_Set(&vid_bpp, "32");
				break;
			case 8:
				Cvar_Set(&vid_bpp, "8");
				break;
			}
		}
		Cvar_Set(&vid_renderer, "sw");
		
		R_RestartRenderer_f();

#endif
	}
	else if (!stricmp(Cmd_Argv(1), "GL") || !stricmp(Cmd_Argv(1), "OpenGL"))
	{
#ifndef RGLQUAKE
		Con_Printf("OpenGL rendering is not supported in this binary\n");
#else
		if (Cmd_Argc() == 3)	//set gl_driver accordingly.
			Cvar_Set(&gl_driver, Cmd_Argv(2));
		
		Cvar_ForceSet(&vid_renderer, "gl");

		if (vid_bpp.value == 8)
			Cvar_Set(&vid_bpp, "16");

		R_RestartRenderer_f();
#endif
	}
	else if (!stricmp(Cmd_Argv(1), "D3D") || !stricmp(Cmd_Argv(1), "DX"))
	{
#if defined(RGLQUAKE) && defined(AVAIL_DX7)
		Cvar_Set(&vid_renderer, "d3d");

		if (vid_bpp.value == 8)
			Cvar_Set(&vid_bpp, "16");

		R_RestartRenderer_f();
#else
		Con_Printf("Direct3D rendering is not supported in this binary\n");
#endif
	}
	else if (Cmd_Argc() < 2)
	{
		Con_Printf ("%s: Switch to a different renderer\n\ttype %s help for more info.\n", Cmd_Argv(0), Cmd_Argv(0));
		return;
	}
	else
	{
		Con_Printf ("%s: Parameters are bad.\n\ttype %s help for more info.\n", Cmd_Argv(0), Cmd_Argv(0));
		return;
	}
}







