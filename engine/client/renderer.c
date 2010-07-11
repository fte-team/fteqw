#include "quakedef.h"
#include "winquake.h"
#include "pr_common.h"
#include "gl_draw.h"
#include <string.h>


refdef_t	r_refdef;
vec3_t		r_origin, vpn, vright, vup;
entity_t	r_worldentity;
entity_t	*currententity;	//nnggh
model_t		*currentmodel;	//fixme: remove? or fix.
int			sh_shadowframe;	//index for msurf->shadowframe
int			r_framecount;
struct texture_s	*r_notexture_mip;


void R_InitParticleTexture (void);

qboolean vid_isfullscreen;

#define VIDCOMMANDGROUP "Video config"
#define GRAPHICALNICETIES "Graphical Nicaties"	//or eyecandy, which ever you prefer.
#ifdef PEXT_BULLETENS
#define BULLETENVARS		"BulletenBoard controls"
#endif
#define GLRENDEREROPTIONS	"GL Renderer Options"
#define SCREENOPTIONS	"Screen Options"

unsigned int	d_8to24rgbtable[256];

extern int gl_anisotropy_factor;

// callbacks used for cvars
void SCR_Viewsize_Callback (struct cvar_s *var, char *oldvalue);
void SCR_Fov_Callback (struct cvar_s *var, char *oldvalue);
#if defined(GLQUAKE)
void GL_Texturemode_Callback (struct cvar_s *var, char *oldvalue);
void GL_Texturemode2d_Callback (struct cvar_s *var, char *oldvalue);
void GL_Texture_Anisotropic_Filtering_Callback (struct cvar_s *var, char *oldvalue);
#endif

cvar_t _vid_wait_override					= CVARAF  ("vid_wait", "",
													   "_vid_wait_override", CVAR_ARCHIVE);

cvar_t _windowed_mouse						= CVARF ("_windowed_mouse","1",
													 CVAR_ARCHIVE);

cvar_t con_ocranaleds						= CVAR  ("con_ocranaleds", "2");

cvar_t d_palconvwrite						= CVAR  ("d_palconvwrite", "1");
cvar_t d_palremapsize						= CVARF ("d_palremapsize", "64",
													 CVAR_RENDERERLATCH);

cvar_t cl_cursor							= CVAR  ("cl_cursor", "");
cvar_t cl_cursorsize						= CVAR  ("cl_cursorsize", "32");
cvar_t cl_cursorbias						= CVAR  ("cl_cursorbias", "4");

cvar_t gl_nocolors							= CVAR  ("gl_nocolors", "0");
cvar_t gl_part_flame						= CVAR  ("gl_part_flame", "1");

//opengl library, blank means try default.
static cvar_t gl_driver						= CVARF ("gl_driver", "", 
													 CVAR_ARCHIVE | CVAR_RENDERERLATCH);
cvar_t gl_shadeq1_name						= CVAR  ("gl_shadeq1_name", "*");
extern cvar_t r_vertexlight;

cvar_t mod_md3flags							= CVAR  ("mod_md3flags", "1");

cvar_t r_ambient							= CVARF ("r_ambient", "0",
												CVAR_CHEAT);
cvar_t r_bloodstains						= CVAR  ("r_bloodstains", "1");
cvar_t r_bouncysparks						= CVARF ("r_bouncysparks", "0",
												CVAR_ARCHIVE);
cvar_t r_drawentities						= CVAR  ("r_drawentities", "1");
cvar_t r_drawflat							= CVARF ("r_drawflat", "0",
												CVAR_SEMICHEAT | CVAR_RENDERERCALLBACK | CVAR_SHADERSYSTEM);
cvar_t gl_miptexLevel						= CVAR  ("gl_miptexLevel", "0");
cvar_t r_drawviewmodel						= CVAR  ("r_drawviewmodel", "1");
cvar_t r_drawviewmodelinvis					= CVAR  ("r_drawviewmodelinvis", "0");
cvar_t r_dynamic							= CVARF ("r_dynamic", IFMINIMAL("0","1"),
												CVAR_ARCHIVE);
cvar_t r_fastturb							= CVARF ("r_fastturb", "0",
												CVAR_SHADERSYSTEM);
cvar_t r_fastsky							= CVARF ("r_fastsky", "0",
												CVAR_SHADERSYSTEM);
cvar_t r_fastskycolour						= CVARF ("r_fastskycolour", "0",
												CVAR_RENDERERCALLBACK|CVAR_SHADERSYSTEM);
cvar_t r_fb_bmodels							= CVARF("gl_fb_bmodels", "1",
												CVAR_SEMICHEAT|CVAR_RENDERERLATCH);
cvar_t r_fb_models							= CVARAF  ("r_fb_models", "1",
													"gl_fb_models", CVAR_SEMICHEAT|CVAR_RENDERERLATCH);
cvar_t r_skin_overlays						= SCVARF  ("r_skin_overlays", "1",
												CVAR_SEMICHEAT|CVAR_RENDERERLATCH);
cvar_t r_flashblend							= SCVARF ("gl_flashblend", "0",
												CVAR_ARCHIVE);
cvar_t r_floorcolour						= SCVARF ("r_floorcolour", "255 255 255",
												CVAR_RENDERERCALLBACK|CVAR_SHADERSYSTEM);
cvar_t r_floortexture						= SCVARF ("r_floortexture", "",
												CVAR_RENDERERCALLBACK|CVAR_SHADERSYSTEM);
cvar_t r_fullbright							= SCVARF ("r_fullbright", "0",
												CVAR_CHEAT|CVAR_SHADERSYSTEM);
cvar_t r_fullbrightSkins					= SCVARF ("r_fullbrightSkins", "1",
												CVAR_SEMICHEAT|CVAR_SHADERSYSTEM);
cvar_t r_lightmap_saturation				= SCVAR  ("r_lightmap_saturation", "1");
cvar_t r_lightstylesmooth					= SCVAR  ("r_lightstylesmooth", "0");
cvar_t r_lightstylespeed					= SCVAR  ("r_lightstylespeed", "10");
cvar_t r_loadlits							= SCVAR  ("r_loadlit", "1");
cvar_t r_menutint							= SCVARF ("r_menutint", "0.68 0.4 0.13",
												CVAR_RENDERERCALLBACK);
cvar_t r_netgraph							= SCVAR  ("r_netgraph", "0");
cvar_t r_nolerp								= SCVAR  ("r_nolerp", "0");
cvar_t r_nolightdir							= SCVAR  ("r_nolightdir", "0");
cvar_t r_novis								= SCVAR  ("r_novis", "0");
cvar_t r_part_rain							= SCVARF ("r_part_rain", "0",
												CVAR_ARCHIVE);
cvar_t r_skyboxname							= SCVARF ("r_skybox", "",
												CVAR_RENDERERCALLBACK | CVAR_SHADERSYSTEM);
cvar_t r_speeds								= SCVAR ("r_speeds", "0");
cvar_t r_stainfadeammount					= SCVAR  ("r_stainfadeammount", "1");
cvar_t r_stainfadetime						= SCVAR  ("r_stainfadetime", "1");
cvar_t r_stains								= CVARFC("r_stains", IFMINIMAL("0","0.75"),
												CVAR_ARCHIVE,
												Cvar_Limiter_ZeroToOne_Callback);
cvar_t r_wallcolour							= CVARF ("r_wallcolour", "255 255 255",
												CVAR_RENDERERCALLBACK|CVAR_SHADERSYSTEM);//FIXME: broken
cvar_t r_walltexture						= CVARF ("r_walltexture", "",
												CVAR_RENDERERCALLBACK|CVAR_SHADERSYSTEM);	//FIXME: broken
cvar_t r_wateralpha							= CVARF  ("r_wateralpha", "1",
												CVAR_SHADERSYSTEM);
cvar_t r_waterwarp							= CVARF ("r_waterwarp", "1",
												CVAR_ARCHIVE);

cvar_t r_replacemodels						= CVARF ("r_replacemodels", IFMINIMAL("","md3 md2"),
												CVAR_ARCHIVE);

//otherwise it would defeat the point.
cvar_t scr_allowsnap						= CVARF ("scr_allowsnap", "1",
												CVAR_NOTFROMSERVER);
cvar_t scr_centersbar						= CVAR  ("scr_centersbar", "0");
cvar_t scr_centertime						= CVAR  ("scr_centertime", "2");
cvar_t scr_chatmodecvar						= CVAR  ("scr_chatmode", "0");
cvar_t scr_conalpha							= CVARC ("scr_conalpha", "0.7",
												Cvar_Limiter_ZeroToOne_Callback);
cvar_t scr_consize							= CVAR  ("scr_consize", "0.5");
cvar_t scr_conspeed							= CVAR  ("scr_conspeed", "300");
// 10 - 170
cvar_t scr_fov								= CVARFC("fov", "90",
												CVAR_ARCHIVE,
												SCR_Fov_Callback);
cvar_t scr_printspeed						= SCVAR  ("scr_printspeed", "8");
cvar_t scr_showpause						= SCVAR  ("showpause", "1");
cvar_t scr_showturtle						= SCVAR  ("showturtle", "0");
cvar_t scr_turtlefps						= SCVAR  ("scr_turtlefps", "10");
cvar_t scr_sshot_compression				= SCVAR  ("scr_sshot_compression", "75");
cvar_t scr_sshot_type						= SCVAR  ("scr_sshot_type", "jpg");
cvar_t scr_viewsize							= CVARFC("viewsize", "100",
												CVAR_ARCHIVE,
												SCR_Viewsize_Callback);

cvar_t vid_conautoscale						= CVARF ("vid_conautoscale", "0",
												CVAR_ARCHIVE | CVAR_RENDERERCALLBACK);
cvar_t vid_conheight						= CVARF ("vid_conheight", "0",
												CVAR_ARCHIVE);
cvar_t vid_conwidth							= CVARF ("vid_conwidth", "0",
												CVAR_ARCHIVE | CVAR_RENDERERCALLBACK);
//see R_RestartRenderer_f for the effective default 'if (newr.renderer == -1)'.
cvar_t vid_renderer							= CVARF ("vid_renderer", "",
												CVAR_ARCHIVE | CVAR_RENDERERLATCH);

static cvar_t vid_allow_modex				= CVARF ("vid_allow_modex", "1",
												CVAR_ARCHIVE | CVAR_RENDERERLATCH);	//FIXME: remove
static cvar_t vid_bpp						= CVARF ("vid_bpp", "32",
												CVAR_ARCHIVE | CVAR_RENDERERLATCH);
static cvar_t vid_desktopsettings			= CVARF ("vid_desktopsettings", "0",
												CVAR_ARCHIVE | CVAR_RENDERERLATCH);
#ifdef NPQTV
static cvar_t vid_fullscreen_npqtv			= CVARF ("vid_fullscreen", "1",
												CVAR_ARCHIVE | CVAR_RENDERERLATCH);
cvar_t vid_fullscreen						= CVARF ("vid_fullscreen_embedded", "0",
												CVAR_ARCHIVE | CVAR_RENDERERLATCH);
#else
static cvar_t vid_fullscreen				= CVARF ("vid_fullscreen", "1",
												CVAR_ARCHIVE | CVAR_RENDERERLATCH);
#endif
cvar_t vid_height							= CVARF ("vid_height", "0",
												CVAR_ARCHIVE | CVAR_RENDERERLATCH);
cvar_t vid_multisample						= CVARF ("vid_multisample", "0",
												CVAR_ARCHIVE | CVAR_RENDERERLATCH);
static cvar_t vid_refreshrate				= CVARF ("vid_displayfrequency", "0",
												CVAR_ARCHIVE | CVAR_RENDERERLATCH);
cvar_t vid_wndalpha							= CVAR ("vid_wndalpha", "1");
//more readable defaults to match conwidth/conheight.
cvar_t vid_width							= CVARF ("vid_width", "0",
												CVAR_ARCHIVE | CVAR_RENDERERLATCH);

extern cvar_t bul_backcol;
extern cvar_t bul_forcemode;
extern cvar_t bul_norender;
extern cvar_t bul_nowater;
extern cvar_t bul_rippleamount;
extern cvar_t bul_ripplespeed;
extern cvar_t bul_scrollspeedx;
extern cvar_t bul_scrollspeedy;
extern cvar_t bul_sparkle;
extern cvar_t bul_text1;
extern cvar_t bul_text2;
extern cvar_t bul_text3;
extern cvar_t bul_text4;
extern cvar_t bul_text5;
extern cvar_t bul_text6;
extern cvar_t bul_textpalette;

extern cvar_t r_dodgytgafiles;
extern cvar_t r_dodgypcxfiles;
extern cvar_t r_drawentities;
extern cvar_t r_drawviewmodel;
extern cvar_t r_drawworld;
extern cvar_t r_fullbright;
extern cvar_t r_mirroralpha;
extern cvar_t r_netgraph;
extern cvar_t r_norefresh;
extern cvar_t r_novis;
extern cvar_t r_speeds;
extern cvar_t r_waterwarp;

extern cvar_t	r_polygonoffset_submodel_factor;
extern cvar_t	r_polygonoffset_submodel_offset;


void R_BulletenForce_f (void);

rendererstate_t currentrendererstate;

#if defined(GLQUAKE)
cvar_t	vid_gl_context_version				= SCVAR  ("vid_gl_context_version", "");
cvar_t	vid_gl_context_forwardcompatible	= SCVAR  ("vid_gl_context_breakeverything", "0");	//not useful, yet, hence the name
cvar_t	vid_gl_context_debug				= SCVAR  ("vid_gl_context_debug", "0");	//for my ati drivers, debug 1 only works if version >= 3
#endif

#if defined(GLQUAKE) || defined(D3DQUAKE)
cvar_t gl_ati_truform						= SCVAR  ("gl_ati_truform", "0");
cvar_t gl_ati_truform_type					= SCVAR  ("gl_ati_truform_type", "1");
cvar_t gl_ati_truform_tesselation			= SCVAR  ("gl_ati_truform_tesselation", "3");
cvar_t gl_blend2d							= SCVAR  ("gl_blend2d", "1");
cvar_t gl_blendsprites						= SCVAR  ("gl_blendsprites", "1");
cvar_t gl_bump								= SCVARF ("gl_bump", "0",
												CVAR_ARCHIVE | CVAR_RENDERERLATCH);
cvar_t gl_compress							= SCVARF ("gl_compress", "0",
												CVAR_ARCHIVE);
cvar_t gl_conback							= CVARFC ("gl_conback", "",
												CVAR_RENDERERCALLBACK, R2D_Conback_Callback);
cvar_t gl_contrast							= SCVAR  ("gl_contrast", "1");
cvar_t gl_detail							= SCVARF ("gl_detail", "0",
												CVAR_ARCHIVE);
cvar_t gl_detailscale						= SCVAR  ("gl_detailscale", "5");
cvar_t gl_font								= SCVARF ("gl_font", "",
												CVAR_RENDERERCALLBACK);
cvar_t gl_lateswap							= SCVAR  ("gl_lateswap", "0");
cvar_t gl_lerpimages						= SCVAR  ("gl_lerpimages", "1");
cvar_t gl_lightmap_shift					= SCVARF ("gl_lightmap_shift", "0",
												CVAR_ARCHIVE | CVAR_LATCH);
//cvar_t gl_lightmapmode						= SCVARF("gl_lightmapmode", "",
//												CVAR_ARCHIVE);
cvar_t gl_load24bit							= SCVARF ("gl_load24bit", "1",
												CVAR_ARCHIVE);

cvar_t gl_max_size							= SCVARF  ("gl_max_size", "1024", CVAR_RENDERERLATCH);
cvar_t gl_maxshadowlights					= SCVARF ("gl_maxshadowlights", "2",
												CVAR_ARCHIVE);
cvar_t gl_menutint_shader					= SCVAR  ("gl_menutint_shader", "1");

//by setting to 64 or something, you can use this as a wallhack
cvar_t gl_mindist							= SCVARF ("gl_mindist", "4",
												CVAR_CHEAT);

cvar_t gl_motionblur						= SCVARF ("gl_motionblur", "0",
												CVAR_ARCHIVE);
cvar_t gl_motionblurscale					= SCVAR  ("gl_motionblurscale", "1");
cvar_t gl_overbright						= SCVARF ("gl_overbright", "0",
												CVAR_ARCHIVE);
cvar_t gl_overbright_all					= SCVARF ("gl_overbright_all", "0",
												CVAR_ARCHIVE);
cvar_t gl_picmip							= SCVAR  ("gl_picmip", "0");
cvar_t gl_picmip2d							= SCVAR  ("gl_picmip2d", "0");
cvar_t gl_nohwblend							= SCVAR  ("gl_nohwblend","1");
cvar_t gl_savecompressedtex					= SCVAR  ("gl_savecompressedtex", "0");
cvar_t gl_schematics						= SCVAR  ("gl_schematics", "0");
cvar_t gl_skyboxdist						= SCVAR  ("gl_skyboxdist", "0");	//0 = guess.
cvar_t gl_smoothcrosshair					= SCVAR  ("gl_smoothcrosshair", "1");

#ifdef SPECULAR
cvar_t gl_specular							= SCVAR  ("gl_specular", "0");
#endif

// The callbacks are not in D3D yet (also ugly way of seperating this)
#ifdef GLQUAKE
cvar_t gl_texture_anisotropic_filtering		= CVARFC("gl_texture_anisotropic_filtering", "0",
												CVAR_ARCHIVE | CVAR_RENDERERCALLBACK,
												GL_Texture_Anisotropic_Filtering_Callback);
cvar_t gl_texturemode						= CVARFC("gl_texturemode", "GL_LINEAR_MIPMAP_NEAREST",
												CVAR_ARCHIVE | CVAR_RENDERERCALLBACK,
												GL_Texturemode_Callback);
cvar_t gl_texturemode2d						= CVARFC("gl_texturemode2d", "GL_LINEAR",
												CVAR_ARCHIVE | CVAR_RENDERERCALLBACK,
												GL_Texturemode2d_Callback);
#endif

cvar_t gl_triplebuffer						= SCVARF ("gl_triplebuffer", "1",
												CVAR_ARCHIVE);
cvar_t gl_ztrick							= SCVAR  ("gl_ztrick", "0");

cvar_t r_noportals							= SCVAR  ("r_noportals", "0");
cvar_t r_noaliasshadows						= SCVARF ("r_noaliasshadows", "0",
												CVAR_ARCHIVE);

cvar_t r_shadow_bumpscale_basetexture		= SCVAR  ("r_shadow_bumpscale_basetexture", "4");
cvar_t r_shadow_bumpscale_bumpmap			= SCVAR  ("r_shadow_bumpscale_bumpmap", "10");

cvar_t r_shadow_glsl_offsetmapping			= SCVAR  ("r_shadow_glsl_offsetmapping", "0");
cvar_t r_shadow_glsl_offsetmapping_bias		= SCVAR  ("r_shadow_glsl_offsetmapping_bias", "0.04");
cvar_t r_shadow_glsl_offsetmapping_scale	= SCVAR  ("r_shadow_glsl_offsetmapping_scale", "-0.04");

cvar_t r_shadow_realtime_world				= SCVARF ("r_shadow_realtime_world", "0", CVAR_ARCHIVE);
cvar_t r_shadow_realtime_world_shadows		= SCVARF ("r_shadow_realtime_world_shadows", "1", CVAR_ARCHIVE);
cvar_t r_shadow_realtime_dlight				= SCVARF ("r_shadow_realtime_dlight", "1", CVAR_ARCHIVE);
cvar_t r_shadow_realtime_dlight_shadows		= SCVARF ("r_shadow_realtime_dlight_shadows", "1", CVAR_ARCHIVE);
cvar_t r_shadow_realtime_world_lightmaps	= SCVARF ("r_shadow_realtime_world_lightmaps", "0.8", 0);

cvar_t r_vertexdlights						= SCVAR  ("r_vertexdlights", "0");

cvar_t vid_preservegamma					= SCVAR ("vid_preservegamma", "0");
cvar_t vid_hardwaregamma					= SCVARF ("vid_hardwaregamma", "1",
												CVAR_ARCHIVE | CVAR_RENDERERLATCH);
cvar_t vid_desktopgamma						= SCVARF ("vid_desktopgamma", "0",
												CVAR_ARCHIVE | CVAR_RENDERERLATCH);

extern cvar_t gl_dither;
extern cvar_t gl_maxdist;
extern cvar_t r_waterlayers;

#endif

#if defined(GLQUAKE) || defined(D3DQUAKE)
void GLD3DRenderer_Init(void)
{
	Cvar_Register (&gl_mindist, GLRENDEREROPTIONS);
	Cvar_Register (&gl_load24bit, GRAPHICALNICETIES);
}
#endif

#if defined(GLQUAKE)
void GLRenderer_Init(void)
{
	extern cvar_t gl_contrast;

	//gl-specific video vars
	Cvar_Register (&vid_gl_context_version, GLRENDEREROPTIONS);
	Cvar_Register (&vid_gl_context_debug, GLRENDEREROPTIONS);
	Cvar_Register (&vid_gl_context_forwardcompatible, GLRENDEREROPTIONS);

	//screen
	Cvar_Register (&gl_triplebuffer, GLRENDEREROPTIONS);

	Cvar_Register (&vid_preservegamma, GLRENDEREROPTIONS);
	Cvar_Register (&vid_hardwaregamma, GLRENDEREROPTIONS);
	Cvar_Register (&vid_desktopgamma, GLRENDEREROPTIONS);

//renderer
	Cvar_Register (&r_mirroralpha, GLRENDEREROPTIONS);
	Cvar_Register (&r_norefresh, GLRENDEREROPTIONS);


	Cvar_Register (&gl_clear, GLRENDEREROPTIONS);

	Cvar_Register (&gl_smoothmodels, GRAPHICALNICETIES);
	Cvar_Register (&gl_affinemodels, GLRENDEREROPTIONS);
	Cvar_Register (&gl_nohwblend, GLRENDEREROPTIONS);
	Cvar_Register (&r_flashblend, GLRENDEREROPTIONS);
	Cvar_Register (&gl_playermip, GLRENDEREROPTIONS);
	Cvar_Register (&gl_nocolors, GLRENDEREROPTIONS);
	Cvar_Register (&gl_finish, GLRENDEREROPTIONS);
	Cvar_Register (&gl_lateswap, GLRENDEREROPTIONS);
	Cvar_Register (&gl_lerpimages, GLRENDEREROPTIONS);

	Cvar_Register (&r_noportals, GLRENDEREROPTIONS);
	Cvar_Register (&r_noaliasshadows, GLRENDEREROPTIONS);
	Cvar_Register (&gl_maxshadowlights, GLRENDEREROPTIONS);
	Cvar_Register (&r_shadow_bumpscale_basetexture, GLRENDEREROPTIONS);
	Cvar_Register (&r_shadow_bumpscale_bumpmap, GLRENDEREROPTIONS);
	Cvar_Register (&r_shadow_realtime_world, GLRENDEREROPTIONS);
	Cvar_Register (&r_shadow_realtime_world_shadows, GLRENDEREROPTIONS);
	Cvar_Register (&r_shadow_realtime_dlight, GLRENDEREROPTIONS);
	Cvar_Register (&r_shadow_realtime_dlight_shadows, GLRENDEREROPTIONS);
	Cvar_Register (&r_shadow_realtime_world_lightmaps, GLRENDEREROPTIONS);

	Cvar_Register (&gl_keeptjunctions, GLRENDEREROPTIONS);
	Cvar_Register (&gl_reporttjunctions, GLRENDEREROPTIONS);

	Cvar_Register (&gl_ztrick, GLRENDEREROPTIONS);

	Cvar_Register (&gl_motionblur, GLRENDEREROPTIONS);
	Cvar_Register (&gl_motionblurscale, GLRENDEREROPTIONS);
	Cvar_Register (&gl_max_size, GLRENDEREROPTIONS);
	Cvar_Register (&gl_maxdist, GLRENDEREROPTIONS);
	Cvar_Register (&vid_multisample, GLRENDEREROPTIONS);

	Cvar_Register (&gl_smoothcrosshair, GRAPHICALNICETIES);

	Cvar_Register (&gl_bump, GRAPHICALNICETIES);
	Cvar_Register (&r_shadow_glsl_offsetmapping, GRAPHICALNICETIES);
	Cvar_Register (&r_shadow_glsl_offsetmapping_scale, GRAPHICALNICETIES);
	Cvar_Register (&r_shadow_glsl_offsetmapping_bias, GRAPHICALNICETIES);

	Cvar_Register (&gl_contrast, GLRENDEREROPTIONS);
#ifdef R_XFLIP
	Cvar_Register (&r_xflip, GLRENDEREROPTIONS);
#endif
	Cvar_Register (&gl_specular, GRAPHICALNICETIES);

//	Cvar_Register (&gl_lightmapmode, GLRENDEREROPTIONS);

#ifdef WATERLAYERS
	Cvar_Register (&r_waterlayers, GRAPHICALNICETIES);
#endif

	Cvar_Register (&r_polygonoffset_submodel_factor, GLRENDEREROPTIONS);
	Cvar_Register (&r_polygonoffset_submodel_offset, GLRENDEREROPTIONS);

	Cvar_Register (&gl_picmip, GLRENDEREROPTIONS);
	Cvar_Register (&gl_picmip2d, GLRENDEREROPTIONS);

	Cvar_Register (&gl_texturemode, GLRENDEREROPTIONS);
	Cvar_Register (&gl_texturemode2d, GLRENDEREROPTIONS);
	Cvar_Register (&gl_texture_anisotropic_filtering, GLRENDEREROPTIONS);
	Cvar_Register (&gl_savecompressedtex, GLRENDEREROPTIONS);
	Cvar_Register (&gl_compress, GLRENDEREROPTIONS);
	Cvar_Register (&gl_driver, GLRENDEREROPTIONS);
	Cvar_Register (&gl_detail, GRAPHICALNICETIES);
	Cvar_Register (&gl_detailscale, GRAPHICALNICETIES);
	Cvar_Register (&gl_overbright, GRAPHICALNICETIES);
	Cvar_Register (&gl_overbright_all, GRAPHICALNICETIES);
	Cvar_Register (&gl_dither, GRAPHICALNICETIES);
	Cvar_Register (&r_fb_bmodels, GRAPHICALNICETIES);

	Cvar_Register (&gl_ati_truform, GRAPHICALNICETIES);
	Cvar_Register (&gl_ati_truform_type, GRAPHICALNICETIES);
	Cvar_Register (&gl_ati_truform_tesselation, GRAPHICALNICETIES);

	Cvar_Register (&gl_skyboxdist, GLRENDEREROPTIONS);

	Cvar_Register (&r_wallcolour, GLRENDEREROPTIONS);
	Cvar_Register (&r_floorcolour, GLRENDEREROPTIONS);
	Cvar_Register (&r_walltexture, GLRENDEREROPTIONS);
	Cvar_Register (&r_floortexture, GLRENDEREROPTIONS);

	Cvar_Register (&r_vertexdlights, GLRENDEREROPTIONS);

	Cvar_Register (&gl_schematics, GLRENDEREROPTIONS);

	Cvar_Register (&r_vertexlight, GLRENDEREROPTIONS);
	Cvar_Register (&gl_shadeq1_name, GLRENDEREROPTIONS);

	Cvar_Register (&gl_blend2d, GLRENDEREROPTIONS);

	Cvar_Register (&gl_blendsprites, GLRENDEREROPTIONS);

	Cvar_Register (&gl_lightmap_shift, GLRENDEREROPTIONS);

	Cvar_Register (&gl_menutint_shader, GLRENDEREROPTIONS);

	R_BloomRegister();
}
#endif

void	R_InitTextures (void)
{
	int		x,y, m;
	qbyte	*dest;

// create a simple checkerboard texture for the default
	r_notexture_mip = Z_Malloc (sizeof(texture_t) + 16*16+8*8+4*4+2*2);

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
	currentrendererstate.renderer = NULL;
	qrenderer = QR_NONE;

	Cmd_AddCommand("setrenderer", R_SetRenderer_f);
	Cmd_AddCommand("vid_restart", R_RestartRenderer_f);

#if defined(GLQUAKE) || defined(D3DQUAKE)
	GLD3DRenderer_Init();
#endif
#if defined(GLQUAKE)
	GLRenderer_Init();
#endif

	Cvar_Register (&gl_conback, GRAPHICALNICETIES);

	Cvar_Register (&r_novis, GLRENDEREROPTIONS);

	//but register ALL vid_ commands.
	Cvar_Register (&_vid_wait_override, VIDCOMMANDGROUP);
	Cvar_Register (&_windowed_mouse, VIDCOMMANDGROUP);
	Cvar_Register (&vid_renderer, VIDCOMMANDGROUP);
	Cvar_Register (&vid_wndalpha, VIDCOMMANDGROUP);

#ifdef NPQTV
	Cvar_Register (&vid_fullscreen_npqtv, VIDCOMMANDGROUP);
#endif
	Cvar_Register (&vid_fullscreen, VIDCOMMANDGROUP);
	Cvar_Register (&vid_bpp, VIDCOMMANDGROUP);

	Cvar_Register (&vid_conwidth, VIDCOMMANDGROUP);
	Cvar_Register (&vid_conheight, VIDCOMMANDGROUP);
	Cvar_Register (&vid_conautoscale, VIDCOMMANDGROUP);

	Cvar_Register (&vid_allow_modex, VIDCOMMANDGROUP);

	Cvar_Register (&vid_width, VIDCOMMANDGROUP);
	Cvar_Register (&vid_height, VIDCOMMANDGROUP);
	Cvar_Register (&vid_refreshrate, VIDCOMMANDGROUP);

	Cvar_Register (&vid_desktopsettings, VIDCOMMANDGROUP);

	Cvar_Register (&r_skyboxname, GRAPHICALNICETIES);

	Cvar_Register(&r_dodgytgafiles, "Bug fixes");
	Cvar_Register(&r_dodgypcxfiles, "Bug fixes");
	Cvar_Register(&r_loadlits, GRAPHICALNICETIES);
	Cvar_Register(&r_lightstylesmooth, GRAPHICALNICETIES);
	Cvar_Register(&r_lightstylespeed, GRAPHICALNICETIES);

	Cvar_Register(&r_stains, GRAPHICALNICETIES);
	Cvar_Register(&r_stainfadetime, GRAPHICALNICETIES);
	Cvar_Register(&r_stainfadeammount, GRAPHICALNICETIES);

	Cvar_Register(&scr_viewsize, SCREENOPTIONS);
	Cvar_Register(&scr_fov, SCREENOPTIONS);
	Cvar_Register(&scr_chatmodecvar, SCREENOPTIONS);

	Cvar_Register (&scr_sshot_type, SCREENOPTIONS);
	Cvar_Register (&scr_sshot_compression, SCREENOPTIONS);

	Cvar_Register(&cl_cursor,	SCREENOPTIONS);
	Cvar_Register(&cl_cursorsize,	SCREENOPTIONS);
	Cvar_Register(&cl_cursorbias,	SCREENOPTIONS);


//screen
	Cvar_Register (&gl_font, GRAPHICALNICETIES);
	Cvar_Register (&scr_conspeed, SCREENOPTIONS);
	Cvar_Register (&scr_conalpha, SCREENOPTIONS);
	Cvar_Register (&scr_showturtle, SCREENOPTIONS);
	Cvar_Register (&scr_turtlefps, SCREENOPTIONS);
	Cvar_Register (&scr_showpause, SCREENOPTIONS);
	Cvar_Register (&scr_centertime, SCREENOPTIONS);
	Cvar_Register (&scr_printspeed, SCREENOPTIONS);
	Cvar_Register (&scr_allowsnap, SCREENOPTIONS);
	Cvar_Register (&scr_consize, SCREENOPTIONS);
	Cvar_Register (&scr_centersbar, SCREENOPTIONS);

	Cvar_Register(&r_bloodstains, GRAPHICALNICETIES);

	Cvar_Register(&r_fullbrightSkins, GRAPHICALNICETIES);

	Cvar_Register (&mod_md3flags, GRAPHICALNICETIES);


//renderer
	Cvar_Register (&r_fullbright, SCREENOPTIONS);
	Cvar_Register (&r_drawentities, GRAPHICALNICETIES);
	Cvar_Register (&r_drawviewmodel, GRAPHICALNICETIES);
	Cvar_Register (&r_drawviewmodelinvis, GRAPHICALNICETIES);
	Cvar_Register (&r_waterwarp, GRAPHICALNICETIES);
	Cvar_Register (&r_speeds, SCREENOPTIONS);
	Cvar_Register (&r_netgraph, SCREENOPTIONS);

	Cvar_Register (&r_dynamic, GRAPHICALNICETIES);
	Cvar_Register (&r_lightmap_saturation, GRAPHICALNICETIES);

	Cvar_Register (&r_nolerp, GRAPHICALNICETIES);
	Cvar_Register (&r_nolightdir, GRAPHICALNICETIES);

	Cvar_Register (&r_fastturb, GRAPHICALNICETIES);
	Cvar_Register (&r_fastsky, GRAPHICALNICETIES);
	Cvar_Register (&r_fastskycolour, GRAPHICALNICETIES);
	Cvar_Register (&r_wateralpha, GRAPHICALNICETIES);

	Cvar_Register (&gl_miptexLevel, GRAPHICALNICETIES);
	Cvar_Register (&r_drawflat, GRAPHICALNICETIES);
	Cvar_Register (&r_menutint, GRAPHICALNICETIES);

	Cvar_Register (&r_fb_models, GRAPHICALNICETIES);
	Cvar_Register (&r_skin_overlays, GRAPHICALNICETIES);

	Cvar_Register (&r_replacemodels, GRAPHICALNICETIES);

#ifdef PEXT_BULLETENS
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
#endif


// misc
	Cvar_Register(&con_ocranaleds, "Console controls");

	P_InitParticleSystem();
	R_InitTextures();
	RQ_Init();
}

qboolean Renderer_Started(void)
{
	return !!currentrendererstate.renderer;
}

void Renderer_Start(void)
{
	Cvar_ApplyLatches(CVAR_RENDERERLATCH);

	//renderer = none && currentrendererstate.bpp == -1 means we've never applied any mode at all
	//if we currently have none, we do actually need to apply it still
	if (qrenderer == QR_NONE && *vid_renderer.string)
	{
		Cmd_ExecuteString("vid_restart\n", RESTRICT_LOCAL);
	}
	if (!currentrendererstate.renderer)
	{	//we still failed. Try again, but use the default renderer.
		Cvar_Set(&vid_renderer, "");
		Cmd_ExecuteString("vid_restart\n", RESTRICT_LOCAL);
	}
	if (!currentrendererstate.renderer)
		Sys_Error("No renderer was set!\n");

	if (qrenderer == QR_NONE)
		Con_Printf("Use the setrenderer command to use a gui\n");
}


mpic_t	*(*Draw_SafePicFromWad)		(char *name);
mpic_t	*(*Draw_SafeCachePic)		(char *path);
void	(*Draw_Init)				(void);
void	(*Draw_Shutdown)			(void);

//void	(*Draw_TinyCharacter)		(int x, int y, unsigned int num);

void	(*Draw_Crosshair)			(void);
void	(*Draw_ScalePic)			(int x, int y, int width, int height, mpic_t *pic);
void	(*Draw_SubPic)				(int x, int y, int width, int height, mpic_t *pic, int srcx, int srcy, int srcwidth, int srcheight);
void	(*Draw_TransPicTranslate)	(int x, int y, int w, int h, qbyte *image, qbyte *translation);
void	(*Draw_ConsoleBackground)	(int firstline, int lastline, qboolean forceopaque);
void	(*Draw_EditorBackground)	(void);
void	(*Draw_TileClear)			(int x, int y, int w, int h);
void	(*Draw_Fill)				(int x, int y, int w, int h, unsigned int c);
void    (*Draw_FillRGB)				(int x, int y, int w, int h, float r, float g, float b);
void	(*Draw_FadeScreen)			(void);
void	(*Draw_BeginDisc)			(void);
void	(*Draw_EndDisc)				(void);

void	(*Draw_Image)				(float x, float y, float w, float h, float s1, float t1, float s2, float t2, mpic_t *pic);	//gl-style scaled/coloured/subpic
void	(*Draw_ImageColours)		(float r, float g, float b, float a);

void	(*R_Init)					(void);
void	(*R_DeInit)					(void);
void	(*R_RenderView)				(void);		// must set r_refdef first

void	(*R_NewMap)					(void);
void	(*R_PreNewMap)				(void);
int		(*R_LightPoint)				(vec3_t point);

void	(*R_PushDlights)			(void);
void	(*R_AddStain)				(vec3_t org, float red, float green, float blue, float radius);
void	(*R_LessenStains)			(void);

void	(*Mod_Init)					(void);
void	(*Mod_ClearAll)				(void);
struct model_s *(*Mod_ForName)		(char *name, qboolean crash);
struct model_s *(*Mod_FindName)		(char *name);
void	*(*Mod_Extradata)			(struct model_s *mod);	// handles caching
void	(*Mod_TouchModel)			(char *name);

void	(*Mod_NowLoadExternal)		(void);
void	(*Mod_Think)				(void);
//qboolean	(*Mod_GetTag)			(struct model_s *model, int tagnum, int frame, int frame2, float f2ness, float f1time, float f2time, float *transforms);
//int (*Mod_TagNumForName)			(struct model_s *model, char *name);
int (*Mod_SkinForName)				(struct model_s *model, char *name);
int (*Mod_FrameForName)				(struct model_s *model, char *name);
float (*Mod_GetFrameDuration)		(struct model_s *model, int framenum);



qboolean (*VID_Init)				(rendererstate_t *info, unsigned char *palette);
void	 (*VID_DeInit)				(void);
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

r_qrenderer_t qrenderer;
char *q_renderername = "Non-Selected renderer";



rendererinfo_t dedicatedrendererinfo = {
	//ALL builds need a 'none' renderer, as 0.
	"No renderer",
	{
		"none",
		"dedicated",
		"terminal",
		"sv"
	},
	QR_NONE,

	NULL,	//Draw_PicFromWad;	//Not supported
	NULL,	//Draw_SafeCachePic;
	NULL,	//Draw_Init;
	NULL,	//Draw_Shutdown;
	NULL,	//Draw_Crosshair;
	NULL,	//Draw_SubPic;
	NULL,	//Draw_TransPicTranslate;
	NULL,	//Draw_ConsoleBackground;
	NULL,	//Draw_EditorBackground;
	NULL,	//Draw_TileClear;
	NULL,	//Draw_Fill;
	NULL,   //Draw_FillRGB;
	NULL,	//Draw_FadeScreen;
	NULL,	//Draw_BeginDisc;
	NULL,	//Draw_EndDisc;
	NULL,	//I'm lazy.

	NULL,	//Draw_Image
	NULL,	//Draw_ImageColours

	NULL,	//R_Init;
	NULL,	//R_DeInit;
	NULL,	//R_RenderView;

	NULL,	//R_NewMap;
	NULL,	//R_PreNewMap
	NULL,	//R_LightPoint;
	NULL,	//R_PushDlights;


	NULL,	//R_AddStain;
	NULL,	//R_LessenStains;

#if defined(GLQUAKE) || defined(D3DQUAKE)
	RMod_Init,
	RMod_ClearAll,
	RMod_ForName,
	RMod_FindName,
	RMod_Extradata,
	RMod_TouchModel,

	RMod_NowLoadExternal,
	RMod_Think,

	NULL, //Mod_GetTag
	NULL, //fixme: server will need this one at some point.
	NULL,
	NULL,
	Mod_FrameDuration,

#else
#error "Need logic here!"
#endif

	NULL, //VID_Init,
	NULL, //VID_DeInit,
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
};
rendererinfo_t *pdedicatedrendererinfo = &dedicatedrendererinfo;

rendererinfo_t openglrendererinfo;
rendererinfo_t d3drendererinfo;
rendererinfo_t d3d7rendererinfo;
rendererinfo_t d3d9rendererinfo;

rendererinfo_t *rendererinfo[] =
{
#ifndef NPQTV
	&dedicatedrendererinfo,
#endif
#ifdef GLQUAKE
	&openglrendererinfo,
	&d3drendererinfo,
#endif
#ifdef D3DQUAKE
	&d3drendererinfo,
	&d3d7rendererinfo,
	&d3d9rendererinfo,
#endif
};




typedef struct vidmode_s
{
	const char *description;
	int         width, height;
} vidmode_t;

vidmode_t vid_modes[] =
{
	{ "320x200 (16:10)",   320, 200}, // CGA, MCGA
	{ "320x240 (4:3)",   320, 240}, // QVGA
	{ "400x300 (4:3)",   400, 300}, // Quarter SVGA
	{ "512x384 (4:3)",   512, 384}, // Mac LC
	{ "640x400 (16:10)",   640, 400}, // Atari ST mono, Amiga OCS NTSC Hires interlace
	{ "640x480 (4:3)",   640, 480}, // VGA, MCGA
	{ "800x600 (4:3)",   800, 600}, // SVGA
	{ "856x480 (16:9)",   856, 480}, // WVGA
	{ "960x720 (4:3)",   960, 720}, // unnamed
	{ "1024x576 (16:9)",  1024, 576}, // WSVGA
	{ "1024x640 (16:10)",  1024, 640}, // unnamed
	{ "1024x768 (4:3)",  1024, 768}, // XGA
	{ "1152x720 (16:10)",  1152, 720}, // XGA+
	{ "1152x864 (4:3)",  1152, 864}, // XGA+
	{ "1280x720 (16:9)", 1280, 720}, // WXGA min.
	{ "1280x800 (16:10)", 1280, 800}, // WXGA avg (native resolution of 17" widescreen LCDs)
	{ "1280x960 (4:3)",  1280, 960}, //SXGA-
	{ "1280x1024 (5:4)", 1280, 1024}, // SXGA (native resolution of 17-19" LCDs)
	{ "1366x768 (16:9)", 1366, 768}, // WXGA
	{ "1400x1050 (4:3)", 1400, 1050}, // SXGA+
	{ "1440x900 (16:10)", 1440, 900}, // WXGA+ (native resolution of 19" widescreen LCDs)
	{ "1440x1080 (4:3)", 1440, 1080}, // unnamed
	{ "1600x900 (16:9)", 1600, 900}, // 900p
	{ "1600x1200 (4:3)", 1600, 1200}, // UXGA (native resolution of 20"+ LCDs) //sw height is bound to 200 to 1024
	{ "1680x1050 (16:10)", 1680, 1050}, // WSXGA+ (native resolution of 22" widescreen LCDs)
	{ "1792x1344 (4:3)", 1792, 1344}, // unnamed
	{ "1800x1440 (5:4)", 1800, 1440}, // unnamed
	{ "1856x1392 (4:3)", 1856, 1392}, //unnamed
	{ "1920x1080 (16:9)", 1920, 1080}, // 1080p (native resolution of cheap 24" LCDs, which really are 23.6")
	{ "1920x1200 (16:10)", 1920, 1200}, // WUXGA (native resolution of good 24" widescreen LCDs)
	{ "1920x1440 (4:3)", 1920, 1440}, // TXGA
	{ "2048x1152 (16:9)", 2048, 1152}, // QWXGA (native resolution of 23" ultra-widescreen LCDs)
	{ "2048x1536 (4:3)", 2048, 1536}, // QXGA	//too much width will disable water warping (>1280) (but at that resolution, it's almost unnoticable)
	{ "2304x1440 (16:10)", 2304, 1440}, // (unnamed; maximum resolution of the Sony GDM-FW900 and Hewlett Packard A7217A)
	{ "2560x1600 (16:10)", 2560, 1600}, // WQXGA (maximum resolution of 30" widescreen LCDs, Dell for example)
	{ "2560x2048 (5:4)", 2560, 2048} // QSXGA
};
#define NUMVIDMODES sizeof(vid_modes)/sizeof(vid_modes[0])

qboolean M_Vid_GetMode(int num, int *w, int *h)
{
	if ((unsigned)num >= NUMVIDMODES)
		return false;

	*w = vid_modes[num].width;
	*h = vid_modes[num].height;
	return true;
}

typedef struct {
	menucombo_t *renderer;
	menucombo_t *modecombo;
	menucombo_t *conscalecombo;
	menucombo_t *bppcombo;
	menucombo_t *refreshratecombo;
	menucombo_t *vsynccombo;
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

	if (!info->bppcombo->selectedoption)
		info->bppcombo->selectedoption = 1;

	info->conscalecombo->common.ishidden = false;
}
qboolean M_VideoApply (union menuoption_s *op,struct menu_s *menu,int key)
{
	videomenuinfo_t *info = menu->data;
	int selectedbpp;

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

	selectedbpp = 16;
	switch(info->bppcombo->selectedoption)
	{
	case 0:
		if (info->renderer->selectedoption)
			selectedbpp = 16;
		else
			selectedbpp = 8;
		break;
	case 1:
		selectedbpp = 16;
		break;
	case 2:
		selectedbpp = 32;
		break;
	}

	switch(info->vsynccombo->selectedoption)
	{
	case 0:
		Cbuf_AddText(va("vid_wait %i\n", 0), RESTRICT_LOCAL);
		break;
	case 1:
		Cbuf_AddText(va("vid_wait %i\n", 1), RESTRICT_LOCAL);
		break;
	case 2:
		Cbuf_AddText(va("vid_wait %i\n", 2), RESTRICT_LOCAL);
		break;
	}


	Cbuf_AddText(va("vid_bpp %i\n", selectedbpp), RESTRICT_LOCAL);

	switch(info->refreshratecombo->selectedoption)
	{
	case 0:
		Cbuf_AddText(va("vid_displayfrequency %i\n", 0), RESTRICT_LOCAL);
		break;
	case 1:
		Cbuf_AddText(va("vid_displayfrequency %i\n", 59), RESTRICT_LOCAL);
		break;
	case 2:
		Cbuf_AddText(va("vid_displayfrequency %i\n", 60), RESTRICT_LOCAL);
		break;
	case 3:
		Cbuf_AddText(va("vid_displayfrequency %i\n", 70), RESTRICT_LOCAL);
		break;
	case 4:
		Cbuf_AddText(va("vid_displayfrequency %i\n", 72), RESTRICT_LOCAL);
		break;
	case 5:
		Cbuf_AddText(va("vid_displayfrequency %i\n", 75), RESTRICT_LOCAL);
		break;
	case 6:
		Cbuf_AddText(va("vid_displayfrequency %i\n", 85), RESTRICT_LOCAL);
		break;
	case 7:
		Cbuf_AddText(va("vid_displayfrequency %i\n", 100), RESTRICT_LOCAL);
		break;
	}

	switch(info->renderer->selectedoption)
	{
#ifdef GLQUAKE
	case 0:
		Cbuf_AddText("setrenderer gl\n", RESTRICT_LOCAL);
		break;
#endif
#ifdef D3DQUAKE
	case 1:
        Cbuf_AddText("setrenderer d3d9\n", RESTRICT_LOCAL);
        break;
#endif
	}
	M_RemoveMenu(menu);
	Cbuf_AddText("menu_video\n", RESTRICT_LOCAL);
	return true;
}
void M_Menu_Video_f (void)
{
	extern cvar_t v_contrast;
#if defined(GLQUAKE)
#endif
	static const char *modenames[128] = {"Custom"};
	static const char *rendererops[] = {
#ifdef GLQUAKE
		"OpenGL",
#endif
#ifdef D3DQUAKE
		"DirectX9",
#endif
		NULL
	};
	static const char *bppnames[] =
	{
		"8",
		"16",
		"32",
		NULL
	};
	static const char *texturefilternames[] =
	{
		"Nearest",
		"Bilinear",
		"Trilinear",
		NULL
	};

	static const char *refreshrates[] =
	{
		"0Hz (OS Driver refresh rate)",
		"59Hz (NTSC is 59.94i)",
		"60Hz",
		"70Hz",
		"72Hz", // VESA minimum setting to avoid eye damage on CRT monitors
		"75Hz",
		"85Hz",
		"100Hz",
		NULL
	};

	static const char *vsyncoptions[] =
	{
		"Off",
		"Wait for Vertical Sync",
		"Wait for Display Enable",
		NULL
	};

	videomenuinfo_t *info;
	int prefabmode;
	int prefab2dmode;
	int currentbpp;
	int currentrefreshrate;
	int currentvsync;
	int aspectratio3d;
	int aspectratio2d;
	char *aspectratio23d;
	char *aspectratio22d;
	char *rendererstring;
	static char current3dres[10]; // enough to fit 1920x1200
	static char current2dres[10]; // same as above
	static char currenthz[6]; // enough to fit 120hz
	static char currentcolordepth[6];

	extern cvar_t _vid_wait_override;
	float vidwidth = vid.pixelwidth;
	float vidheight = vid.pixelheight;

	int i, y;
	menu_t *menu = M_Options_Title(&y, sizeof(videomenuinfo_t));
	info = menu->data;

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

#if defined(GLQUAKE) && defined(D3DQUAKE)
	if (!strcmp(vid_renderer.string, "d3d9"))
		i = 1;
	else
#endif
		i = 0;

	if (vid_bpp.value >= 32)
	{
		currentbpp = 2;
		strcpy(currentcolordepth, va("%sbit (16.7m colors)",vid_bpp.string) );
	}
	else if (vid_bpp.value >= 16)
	{
		currentbpp = 1;
		strcpy(currentcolordepth, va("%sbit (65.5k colors)",vid_bpp.string) );
	}
	else
		currentbpp = 0;

	if (vid_refreshrate.value >= 100)
		currentrefreshrate = 7;
	else if (vid_refreshrate.value >= 85)
		currentrefreshrate = 6;
	else if (vid_refreshrate.value >= 75)
		currentrefreshrate = 5;
	else if (vid_refreshrate.value >= 72)
		currentrefreshrate = 4;
	else if (vid_refreshrate.value >= 70)
		currentrefreshrate = 3;
	else if (vid_refreshrate.value >= 60)
		currentrefreshrate = 2;
	else if (vid_refreshrate.value >= 59)
		currentrefreshrate = 1;
	else if (vid_refreshrate.value >= 0)
		currentrefreshrate = 0;
	else
		currentrefreshrate = 0;

	strcpy(currenthz, va("%sHz",vid_refreshrate.string) );

	aspectratio3d = (vidwidth / vidheight * 100); // times by 100 so don't have to deal with floats

	if (aspectratio3d == 125) // 1.25
		aspectratio23d = "5:4";
	else if (aspectratio3d == 160) // 1.6
		aspectratio23d = "16:10";
	else if (aspectratio3d == 133) // 1.333333
		aspectratio23d = "4:3";
	else if (aspectratio3d == 177) // 1.777778
		aspectratio23d = "16:9";
	else
	{
		aspectratio23d = "Non-standard Ratio";
		Con_Printf("Ratio: %i, width: %i, height: %i\n", aspectratio3d, vid.pixelwidth, vid.pixelheight);
	}

	aspectratio2d = (vid_conwidth.value / vid_conheight.value * 100); // times by 100 so don't have to deal with floats

	if (aspectratio2d == 125) // 1.25
		aspectratio22d = "5:4";
	else if (aspectratio2d == 160) // 1.6
		aspectratio22d = "16:10";
	else if (aspectratio2d == 133) // 1.333333
		aspectratio22d = "4:3";
	else if (aspectratio2d == 177) // 1.777778
		aspectratio22d = "16:9";
	else
		aspectratio22d = "Non-standard Ratio";

	currentvsync = _vid_wait_override.value;

	if ( stricmp(vid_renderer.string,"gl" ) == 0 )
		rendererstring = "OpenGL";
	else if ( stricmp(vid_renderer.string,"d3d7") == 0 )
		rendererstring = "DirectX 7";
	else if ( stricmp(vid_renderer.string,"d3d9") == 0 )
		rendererstring = "DirectX 9";
	else if ( stricmp(vid_renderer.string,"d3d") == 0)
		rendererstring = "DirectX";
	else if ( stricmp(vid_renderer.string,"sw") == 0)
		rendererstring = "Software";
	else
		rendererstring = "Unknown Renderer?";

	strcpy(current3dres, va("%ix%i", vid.pixelwidth, vid.pixelheight) );
	strcpy(current2dres, va("%sx%s", vid_conwidth.string, vid_conheight.string) );

	y += 40;
	MC_AddRedText(menu, 0, y, 								"           Current Renderer", false);
	MC_AddRedText(menu, 225, y, 						        rendererstring, false); y+=8;
	MC_AddRedText(menu, 0, y, 							    "        Current Color Depth", false);
	MC_AddRedText(menu, 225, y, 						 		currentcolordepth, false); y+=8;
	if ( ( vidwidth == 0) || ( vidheight == 0) )
		y+=16;
	else
	{
		MC_AddRedText(menu, 0, y, 								"             Current 3D Res", false);
		MC_AddRedText(menu, 225, y, 							  	  current3dres, false); y+=8;
		MC_AddRedText(menu, 0, y, 								"             Current 3D A/R", false);
		MC_AddRedText(menu, 225, y, 								aspectratio23d, false); y+=8;
	}

	if ( ( vid_conwidth.value == 0) || ( vid_conheight.value == 0) ) // same as 3d resolution
	{
		MC_AddRedText(menu, 0, y, 								"             Current 2D Res", false);
		MC_AddRedText(menu, 225, y, 								  current3dres, false); y+=8;
		MC_AddRedText(menu, 0, y, 								"             Current 2D A/R", false);
		MC_AddRedText(menu, 225, y, 								aspectratio23d, false); y+=8;
	}
	else
	{
		MC_AddRedText(menu, 0, y, 								"             Current 2D Res", false);
		MC_AddRedText(menu, 225, y, 								  current2dres, false); y+=8;
		MC_AddRedText(menu, 0, y, 								"             Current 2D A/R", false);
		MC_AddRedText(menu, 225, y, 								aspectratio22d, false); y+=8;
	}

	MC_AddRedText(menu, 0, y, 								"       Current Refresh Rate", false);
	MC_AddRedText(menu, 225, y, 								currenthz, false); y+=8;
 	y+=8;
	MC_AddRedText(menu, 0, y,								"      €‚ ", false); y+=8;
	y+=8;
	info->renderer = MC_AddCombo(menu,	16, y,				"         Renderer", rendererops, i);	y+=8;
	info->bppcombo = MC_AddCombo(menu,	16, y,				"      Color Depth", bppnames, currentbpp); y+=8;
	info->refreshratecombo = MC_AddCombo(menu,	16, y,		"     Refresh Rate", refreshrates, currentrefreshrate); y+=8;
	info->modecombo = MC_AddCombo(menu,	16, y,				"       Video Size", modenames, prefabmode+1);	y+=8;
	MC_AddWhiteText(menu, 16, y, 							"  3D Aspect Ratio", false); y+=8;
	info->conscalecombo = MC_AddCombo(menu,	16, y,			"          2D Size", modenames, prefab2dmode+1);	y+=8;
	MC_AddWhiteText(menu, 16, y, 							"  2D Aspect Ratio", false); y+=8;
	MC_AddCheckBox(menu,	16, y,							"       Fullscreen", &vid_fullscreen,0);	y+=8;
	y+=4;info->customwidth = MC_AddEdit(menu, 16, y,		"     Custom width", vid_width.string);	y+=8;
	y+=4;info->customheight = MC_AddEdit(menu, 16, y,		"    Custom height", vid_height.string);	y+=12;
	info->vsynccombo = MC_AddCombo(menu,	16, y,			"            VSync", vsyncoptions, currentvsync); y+=8;
	//MC_AddCheckBox(menu,	16, y,							"   Override VSync", &_vid_wait_override,0);	y+=8;
	MC_AddCheckBox(menu,	16, y,							" Desktop Settings", &vid_desktopsettings,0);	y+=8;
	y+=8;
	MC_AddCommand(menu,	16, y,								"= Apply Changes =", M_VideoApply);	y+=8;
	y+=8;
	MC_AddSlider(menu,	16, y,								"      Screen size", &scr_viewsize,	30,		120, 1);y+=8;
	MC_AddSlider(menu,	16, y,								"Console Autoscale",&vid_conautoscale, 0, 6, 0.25);	y+=8;
	MC_AddSlider(menu,	16, y,								"            Gamma", &v_gamma, 0.3, 1, 0.05);	y+=8;
	MC_AddCheckBox(menu,	16, y,							"    Desktop Gamma", &vid_desktopgamma,0);	y+=8;
	MC_AddCheckBox(menu,	16, y,							"   Hardware Gamma", &vid_hardwaregamma,0);	y+=8;
	MC_AddCheckBox(menu,	16, y,							"   Preserve Gamma", &vid_preservegamma,0);	y+=8;
	MC_AddSlider(menu,	16, y,								"         Contrast", &v_contrast, 1, 3, 0.05);	y+=8;
	y+=8;
	MC_AddCheckBox(menu,	16, y,							"      Allow ModeX", &vid_allow_modex,0);	y+=8;
	MC_AddCheckBox(menu,	16, y,							"   Windowed Mouse", &_windowed_mouse,0);	y+=8;

	menu->selecteditem = (union menuoption_s *)info->renderer;
	menu->cursoritem = (menuoption_t*)MC_AddWhiteText(menu, 152, menu->selecteditem->common.posy, NULL, false);
	menu->event = CheckCustomMode;
}

void R_SetRenderer(rendererinfo_t *ri)
{
	currentrendererstate.renderer = ri;
	if (!ri)
		ri = &dedicatedrendererinfo;

	qrenderer = ri->rtype;
	q_renderername = ri->name[0];

	Draw_SafePicFromWad		= ri->Draw_SafePicFromWad;	//Not supported
	Draw_SafeCachePic		= ri->Draw_SafeCachePic;
	Draw_Init				= ri->Draw_Init;
	Draw_Shutdown			= ri->Draw_Shutdown;
	Draw_Crosshair			= ri->Draw_Crosshair;
	Draw_SubPic				= ri->Draw_SubPic;
	Draw_TransPicTranslate	= ri->Draw_TransPicTranslate;
	Draw_ConsoleBackground	= ri->Draw_ConsoleBackground;
	Draw_EditorBackground	= ri->Draw_EditorBackground;
	Draw_TileClear			= ri->Draw_TileClear;
	Draw_Fill				= ri->Draw_Fill;
	Draw_FillRGB			= ri->Draw_FillRGB;
	Draw_FadeScreen			= ri->Draw_FadeScreen;
	Draw_BeginDisc			= ri->Draw_BeginDisc;
	Draw_EndDisc			= ri->Draw_EndDisc;
	Draw_ScalePic			= ri->Draw_ScalePic;

	Draw_Image				= ri->Draw_Image;
	Draw_ImageColours 		= ri->Draw_ImageColours;

	R_Init					= ri->R_Init;
	R_DeInit				= ri->R_DeInit;
	R_RenderView			= ri->R_RenderView;
	R_NewMap				= ri->R_NewMap;
	R_PreNewMap				= ri->R_PreNewMap;
	R_LightPoint			= ri->R_LightPoint;
	R_PushDlights			= ri->R_PushDlights;

	R_AddStain				= ri->R_AddStain;
	R_LessenStains			= ri->R_LessenStains;

	VID_Init				= ri->VID_Init;
	VID_DeInit				= ri->VID_DeInit;
	VID_LockBuffer			= ri->VID_LockBuffer;
	VID_UnlockBuffer		= ri->VID_UnlockBuffer;
	D_BeginDirectRect		= ri->D_BeginDirectRect;
	D_EndDirectRect			= ri->D_EndDirectRect;
	VID_ForceLockState		= ri->VID_ForceLockState;
	VID_ForceUnlockedAndReturnState	= ri->VID_ForceUnlockedAndReturnState;
	VID_SetPalette			= ri->VID_SetPalette;
	VID_ShiftPalette		= ri->VID_ShiftPalette;
	VID_GetRGBInfo			= ri->VID_GetRGBInfo;
	VID_SetWindowCaption	= ri->VID_SetWindowCaption;

	Mod_Init				= ri->Mod_Init;
	Mod_Think				= ri->Mod_Think;
	Mod_ClearAll			= ri->Mod_ClearAll;
	Mod_ForName				= ri->Mod_ForName;
	Mod_FindName			= ri->Mod_FindName;
	Mod_Extradata			= ri->Mod_Extradata;
	Mod_TouchModel			= ri->Mod_TouchModel;

	Mod_NowLoadExternal		= ri->Mod_NowLoadExternal;

//	Mod_GetTag				= ri->Mod_GetTag;
//	Mod_TagNumForName 		= ri->Mod_TagNumForName;
	Mod_SkinForName 		= ri->Mod_SkinForName;
	Mod_FrameForName		= ri->Mod_FrameForName;
	Mod_GetFrameDuration	= ri->Mod_GetFrameDuration;

	SCR_UpdateScreen		= ri->SCR_UpdateScreen;
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

qboolean R_ApplyRenderer_Load (rendererstate_t *newr);
void D3DSucks(void)
{
	SCR_DeInit();

	if (!R_ApplyRenderer_Load(NULL))//&currentrendererstate))
		Sys_Error("Failed to reload content after mode switch\n");
}

void R_ShutdownRenderer(void)
{

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
}

qboolean R_ApplyRenderer (rendererstate_t *newr)
{
	if (newr->bpp == -1)
		return false;
	if (!newr->renderer)
		return false;

	R_ShutdownRenderer();

	if (qrenderer == QR_NONE)
	{
		if (newr->renderer->rtype == qrenderer)
			return true;	//no point

		Sys_CloseTerminal ();
	}

	R_SetRenderer(newr->renderer);

	return R_ApplyRenderer_Load(newr);
}
qboolean R_ApplyRenderer_Load (rendererstate_t *newr)
{
	int i, j;
	extern model_t *loadmodel;
	extern int host_hunklevel;

	Cache_Flush();

	Hunk_FreeToLowMark(host_hunklevel);	//is this a good idea?

	TRACE(("dbg: R_ApplyRenderer: old renderer closed\n"));

	pmove.numphysent = 0;

	if (qrenderer != QR_NONE)	//graphics stuff only when not dedicated
	{
		qbyte *data;
#ifndef CLIENTONLY
		isDedicated = false;
#endif
		if (newr)
			Con_Printf("Setting mode %i*%i*%i*%i\n", newr->width, newr->height, newr->bpp, newr->rate);

		if (host_basepal)
			BZ_Free(host_basepal);
		if (host_colormap)
			BZ_Free(host_colormap);
		host_basepal = (qbyte *)FS_LoadMallocFile ("gfx/palette.lmp");
		if (!host_basepal)
		{
			qbyte *pcx=NULL;
			host_basepal = BZ_Malloc(768);
			pcx = COM_LoadTempFile("pics/colormap.pcx");
			if (!pcx || !ReadPCXPalette(pcx, com_filesize, host_basepal))
			{
				memcpy(host_basepal, default_quakepal, 768);
			}
			else
			{
				host_colormap = BZ_Malloc(256*VID_GRADES);
				if (ReadPCXData(pcx, com_filesize, 256, VID_GRADES, host_colormap))
					goto q2colormap;	//skip the colormap.lmp file as we already read it
			}
		}
		host_colormap = (qbyte *)FS_LoadMallocFile ("gfx/colormap.lmp");
		if (!host_colormap)
		{
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

q2colormap:

TRACE(("dbg: R_ApplyRenderer: Palette loaded\n"));

#ifdef _WIN32
#ifndef _SDL
		if (hwnd_dialog)
		{
			DestroyWindow (hwnd_dialog);
			hwnd_dialog = NULL;
		}
#endif
#endif

		if (newr)
			if (!VID_Init(newr, host_basepal))
			{
				return false;
			}
TRACE(("dbg: R_ApplyRenderer: vid applied\n"));

		W_LoadWadFile("gfx.wad");
TRACE(("dbg: R_ApplyRenderer: wad loaded\n"));
		Draw_Init();
TRACE(("dbg: R_ApplyRenderer: draw inited\n"));
		R_Init();
		R_InitParticleTexture ();
TRACE(("dbg: R_ApplyRenderer: renderer inited\n"));
		SCR_Init();
TRACE(("dbg: R_ApplyRenderer: screen inited\n"));
		Sbar_Flush();

		IN_ReInit();
	}
	else
	{
#ifdef CLIENTONLY
		Sys_Error("Tried setting dedicated mode\n");
		//we could support this, but there's no real reason to actually do so.

		//fixme: despite the checks in the setrenderer command, we can still get here via a config using vid_renderer.
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
#ifdef PEXT_BULLETENS
TRACE(("dbg: R_ApplyRenderer: initing bulletein boards\n"));
	WipeBulletenTextures();
#endif

//	host_hunklevel = Hunk_LowMark();

	if (R_PreNewMap)
	if (cl.worldmodel)
	{
		TRACE(("dbg: R_ApplyRenderer: R_PreNewMap (how handy)\n"));
		R_PreNewMap();
	}

#ifndef CLIENTONLY
	if (sv.world.worldmodel)
	{
		wedict_t *ent;
#ifdef Q2SERVER
		q2edict_t *q2ent;
#endif

TRACE(("dbg: R_ApplyRenderer: reloading server map\n"));
		sv.world.worldmodel = Mod_ForName (sv.modelname, false);
TRACE(("dbg: R_ApplyRenderer: loaded\n"));
		if (sv.world.worldmodel->needload)
		{
			SV_Error("Bsp went missing on render restart\n");
		}
TRACE(("dbg: R_ApplyRenderer: doing that funky phs thang\n"));
		SV_CalcPHS ();

TRACE(("dbg: R_ApplyRenderer: clearing world\n"));
		World_ClearWorld (&sv.world);

		if (svs.gametype == GT_PROGS)
		{
			for (i = 0; i < MAX_MODELS; i++)
			{
				if (sv.strings.model_precache[i] && *sv.strings.model_precache[i] && (!strcmp(sv.strings.model_precache[i] + strlen(sv.strings.model_precache[i]) - 4, ".bsp") || i-1 < sv.world.worldmodel->numsubmodels))
					sv.models[i] = Mod_FindName(sv.strings.model_precache[i]);
				else
					sv.models[i] = NULL;
			}

			ent = sv.world.edicts;
//			ent->v->model = PR_NewString(svprogfuncs, sv.worldmodel->name);	//FIXME: is this a problem for normal ents?
			for (i=0 ; i<sv.world.num_edicts ; i++)
			{
				ent = (wedict_t*)EDICT_NUM(svprogfuncs, i);
				if (!ent)
					continue;
				if (ent->isfree)
					continue;

				if (ent->area.prev)
				{
					ent->area.prev = ent->area.next = NULL;
					World_LinkEdict (&sv.world, ent, false);	// relink ents so touch functions continue to work.
				}
			}
		}
#ifdef Q2SERVER
		else if (svs.gametype == GT_QUAKE2)
		{
			for (i = 0; i < MAX_MODELS; i++)
			{
				if (sv.strings.configstring[Q2CS_MODELS+i] && *sv.strings.configstring[Q2CS_MODELS+i] && (!strcmp(sv.strings.configstring[Q2CS_MODELS+i] + strlen(sv.strings.configstring[Q2CS_MODELS+i]) - 4, ".bsp") || i-1 < sv.world.worldmodel->numsubmodels))
					sv.models[i] = Mod_FindName(sv.strings.configstring[Q2CS_MODELS+i]);
				else
					sv.models[i] = NULL;
			}

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
					WorldQ2_LinkEdict (&sv.world, q2ent);	// relink ents so touch functions continue to work.
				}
			}
		}
#endif
		else
			SV_UnspawnServer();
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
				if (cl_static_entities[i].ent.model == cl.model_precache[j])
				{
					staticmodelindex[i] = j;
					break;
				}
		}

		cl.worldmodel = NULL;
		cl_numvisedicts = 0;
		cl_numstrisidx = 0;
		cl_numstrisvert = 0;
		cl_numstris = 0;

TRACE(("dbg: R_ApplyRenderer: reloading ALL models\n"));
		for (i=1 ; i<MAX_MODELS ; i++)
		{
			if (!cl.model_name[i][0])
				break;

			cl.model_precache[i] = NULL;
			TRACE(("dbg: R_ApplyRenderer: reloading model %s\n", cl.model_name[i]));
			cl.model_precache[i] = Mod_ForName (cl.model_name[i], false);

			if (!cl.model_precache[i] && i == 1)
			{
				Con_Printf ("\nThe required model file '%s' could not be found.\n\n"
					, cl.model_name[i]);
				Con_Printf ("You may need to download or purchase a client "
					"pack in order to play on this server.\n\n");
				CL_Disconnect ();
#ifdef VM_UI
				UI_Reset();
#endif
				return false;
			}
		}

		for (i=0; i < MAX_VWEP_MODELS; i++)
		{
			if (*cl.model_name_vwep[i])
				cl.model_precache_vwep[i] = Mod_ForName (cl.model_name_vwep[i], false);
			else
				cl.model_precache_vwep[i] = NULL;
		}

#ifdef CSQC_DAT
		for (i=1 ; i<MAX_CSQCMODELS ; i++)
		{
			if (!cl.model_csqcname[i][0])
				break;

			cl.model_csqcprecache[i] = NULL;
			TRACE(("dbg: R_ApplyRenderer: reloading csqc model %s\n", cl.model_csqcname[i]));
			cl.model_csqcprecache[i] = Mod_ForName (cl.model_csqcname[i], false);

			if (!cl.model_csqcprecache[i])
			{
				Con_Printf ("\nThe required model file '%s' could not be found.\n\n"
					, cl.model_csqcname[i]);
				Con_Printf ("You may need to download or purchase a client "
					"pack in order to play on this server.\n\n");
				CL_Disconnect ();
#ifdef VM_UI
				UI_Reset();
#endif
				return false;
			}
		}
#endif

		loadmodel = cl.worldmodel = cl.model_precache[1];
TRACE(("dbg: R_ApplyRenderer: done the models\n"));
		if (loadmodel->needload)
		{
				CL_Disconnect ();
#ifdef VM_UI
				UI_Reset();
#endif
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
			cl_static_entities[i].ent.model = cl.model_precache[staticmodelindex[i]];
			if (cl_static_entities[i].ent.model)
				cl.worldmodel->funcs.FindTouchedLeafs(cl.worldmodel, &cl_static_entities[i].pvscache, NULL, NULL);
#pragma message("STATIC ENTITITES --- relink")
		}
	}
#ifdef VM_UI
	else
		UI_Reset();
#endif

	switch (qrenderer)
	{
	case QR_NONE:
		Con_Printf(	"\n"
					"-----------------------------\n"
					"Dedicated console created\n");
		break;

	case QR_OPENGL:
		Con_Printf(	"\n"
					"-----------------------------\n"
					"OpenGL renderer initialized\n");
		break;

	case QR_DIRECT3D:
		Con_Printf(	"\n"
					"-----------------------------\n"
					"Direct3d renderer initialized\n");
		break;
	}

	TRACE(("dbg: R_ApplyRenderer: S_Restart_f\n"));
	if (!isDedicated)
		S_DoRestart();

	TRACE(("dbg: R_ApplyRenderer: done\n"));

	if (newr)
		memcpy(&currentrendererstate, newr, sizeof(currentrendererstate));
	return true;
}

#define DEFAULT_WIDTH 640
#define DEFAULT_HEIGHT 480
#define DEFAULT_BPP 32

void R_RestartRenderer_f (void)
{
	int i, j;
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

	newr.multisample = vid_multisample.value;
	newr.bpp = vid_bpp.value;
	newr.fullscreen = vid_fullscreen.value;
	newr.rate = vid_refreshrate.value;

	Q_strncpyz(newr.glrenderer, gl_driver.string, sizeof(newr.glrenderer));

	newr.renderer = NULL;
	for (i = 0; i < sizeof(rendererinfo)/sizeof(rendererinfo[0]); i++)
	{
		if (!rendererinfo[i]->description)
			continue;	//not valid in this build. :(
		for (j = 4-1; j >= 0; j--)
		{
			if (!rendererinfo[i]->name[j])
				continue;
			if (!stricmp(rendererinfo[i]->name[j], vid_renderer.string))
			{
				newr.renderer = rendererinfo[i];
				break;
			}
		}
	}
	if (!newr.renderer)
	{
		Con_Printf("vid_renderer unset or invalid. Using default.\n");
		//gotta do this after main hunk is saved off.
#if defined(GLQUAKE)
		Cmd_ExecuteString("setrenderer gl\n", RESTRICT_LOCAL);
#elif defined(D3DQUAKE)
		Cmd_ExecuteString("setrenderer d3d9\n", RESTRICT_LOCAL);
#endif
		return;
	}

	// use desktop settings if set to 0 and not dedicated
	if (newr.renderer->rtype != QR_NONE)
	{
		int dbpp, dheight, dwidth, drate;

		if ((!newr.fullscreen && !vid_desktopsettings.value) || !Sys_GetDesktopParameters(&dwidth, &dheight, &dbpp, &drate))
		{
			// force default values for systems not supporting desktop parameters
			dwidth = DEFAULT_WIDTH;
			dheight = DEFAULT_HEIGHT;
			dbpp = DEFAULT_BPP;
			drate = 0;
		}

		if (vid_desktopsettings.value)
		{
			newr.width = dwidth;
			newr.height = dheight;
			newr.bpp = dbpp;
			newr.rate = drate;
		}
		else
		{
			if (newr.width <= 0 || newr.height <= 0)
			{
				newr.width = dwidth;
				newr.height = dheight;
			}

			if (newr.bpp <= 0)
				newr.bpp = dbpp;
		}
	}

	TRACE(("dbg: R_RestartRenderer_f renderer %i\n", newr.renderer));

	memcpy(&oldr, &currentrendererstate, sizeof(rendererstate_t));
	if (!R_ApplyRenderer(&newr))
	{
		TRACE(("dbg: R_RestartRenderer_f failed\n"));
		if (R_ApplyRenderer(&oldr))
		{
			TRACE(("dbg: R_RestartRenderer_f old restored\n"));
			Con_Printf(CON_ERROR "Video mode switch failed. Old mode restored.\n");	//go back to the old mode, the new one failed.
		}
		else
		{
			qboolean failed = true;

			if (newr.rate != 0)
			{
				Con_Printf(CON_NOTICE "Trying default refresh rate\n");
				newr.rate = 0;
				failed = !R_ApplyRenderer(&newr);
			}

			if (failed && newr.width != DEFAULT_WIDTH && newr.height != DEFAULT_HEIGHT)
			{
				Con_Printf(CON_NOTICE "Trying %i*%i\n", DEFAULT_WIDTH, DEFAULT_HEIGHT);
				newr.width = DEFAULT_WIDTH;
				newr.height = DEFAULT_HEIGHT;
				failed = !R_ApplyRenderer(&newr);
			}

			if (failed)
			{
				newr.renderer = &dedicatedrendererinfo;
				if (R_ApplyRenderer(&newr))
				{
					TRACE(("dbg: R_RestartRenderer_f going to dedicated\n"));
					Con_Printf(CON_ERROR "Video mode switch failed. Old mode wasn't supported either. Console forced.\n\nChange the following vars to something useable, and then use the setrenderer command.\n");
					Con_Printf("%s: %s\n", vid_width.name, vid_width.string);
					Con_Printf("%s: %s\n", vid_height.name, vid_height.string);
					Con_Printf("%s: %s\n", vid_bpp.name, vid_bpp.string);
					Con_Printf("%s: %s\n", vid_refreshrate.name, vid_refreshrate.string);
					Con_Printf("%s: %s\n", vid_renderer.name, vid_renderer.string);
				}
				else
					Sys_Error("Couldn't fall back to previous renderer\n");
			}
		}
	}

	Cvar_ApplyCallbacks(CVAR_RENDERERCALLBACK);
	SCR_EndLoadingPlaque();

	TRACE(("dbg: R_RestartRenderer_f success\n"));
#ifdef MENU_DAT
	MP_Init();
#endif
	CL_InitDlights();
}

void R_SetRenderer_f (void)
{
	int i, j;
	int best;
	char *param = Cmd_Argv(1);
	if (Cmd_Argc() == 1 || !stricmp(param, "help"))
	{
		Con_Printf ("\nValid setrenderer parameters are:\n");
		for (i = 0; i < sizeof(rendererinfo)/sizeof(rendererinfo[0]); i++)
		{
			if (rendererinfo[i]->description)
				Con_Printf("%s: %s\n", rendererinfo[i]->name[0], rendererinfo[i]->description);
		}
		return;
	}

	best = -1;
	for (i = 0; i < sizeof(rendererinfo)/sizeof(rendererinfo[0]); i++)
	{
		if (!rendererinfo[i]->description)
			continue;	//not valid in this build. :(
		for (j = 4-1; j >= 0; j--)
		{
			if (!rendererinfo[i]->name[j])
				continue;
			if (!stricmp(rendererinfo[i]->name[j], param))
			{
				best = i;
				break;
			}
		}
	}

#ifdef CLIENTONLY
	if (best == 0)
	{
		Con_Printf("Client-only builds cannot use dedicated modes.\n");
		return;
	}
#endif

	if (best == -1)
	{
		Con_Printf("setrenderer: parameter not supported (%s)\n", param);
		return;
	}
	else
	{
		if (Cmd_Argc() == 3)
			Cvar_Set(&vid_bpp, Cmd_Argv(2));
	}

	Cvar_Set(&vid_renderer, param);
	R_RestartRenderer_f();
}
























/*
================
R_GetSpriteFrame
================
*/
mspriteframe_t *R_GetSpriteFrame (entity_t *currententity)
{
	msprite_t		*psprite;
	mspritegroup_t	*pspritegroup;
	mspriteframe_t	*pspriteframe;
	int				i, numframes, frame;
	float			*pintervals, fullinterval, targettime, time;

	psprite = currententity->model->cache.data;
	frame = currententity->framestate.g[FS_REG].frame[0];

	if ((frame >= psprite->numframes) || (frame < 0))
	{
		Con_DPrintf ("R_DrawSprite: no such frame %d (%s)\n", frame, currententity->model->name);
		frame = 0;
	}

	if (psprite->frames[frame].type == SPR_SINGLE)
	{
		pspriteframe = psprite->frames[frame].frameptr;
	}
	else if (psprite->frames[frame].type == SPR_ANGLED)
	{
		pspritegroup = (mspritegroup_t *)psprite->frames[frame].frameptr;
		pspriteframe = pspritegroup->frames[(int)((r_refdef.viewangles[1]-currententity->angles[1])/360*8 + 0.5-4)&7];
	}
	else
	{
		pspritegroup = (mspritegroup_t *)psprite->frames[frame].frameptr;
		pintervals = pspritegroup->intervals;
		numframes = pspritegroup->numframes;
		fullinterval = pintervals[numframes-1];

		time = currententity->framestate.g[FS_REG].frametime[0];

	// when loading in Mod_LoadSpriteGroup, we guaranteed all interval values
	// are positive, so we don't have to worry about division by 0
		targettime = time - ((int)(time / fullinterval)) * fullinterval;

		for (i=0 ; i<(numframes-1) ; i++)
		{
			if (pintervals[i] > targettime)
				break;
		}

		pspriteframe = pspritegroup->frames[i];
	}

	return pspriteframe;
}

/*
void MYgluPerspective(double fovx, double fovy, double zNear, double zFar)
{
	Matrix4_Projection_Far(r_refdef.m_projection, fovx, fovy, zNear, zFar);
}

void GL_InfinatePerspective(double fovx, double fovy,
		     double zNear)
{
	// nudge infinity in just slightly for lsb slop
    float nudge = 1;// - 1.0 / (1<<23);

	double xmin, xmax, ymin, ymax;

	ymax = zNear * tan( fovy * M_PI / 360.0 );
	ymin = -ymax;

	xmax = zNear * tan( fovx * M_PI / 360.0 );
	xmin = -xmax;

	r_projection_matrix[0] = (2*zNear) / (xmax - xmin);
	r_projection_matrix[4] = 0;
	r_projection_matrix[8] = (xmax + xmin) / (xmax - xmin);
	r_projection_matrix[12] = 0;

	r_projection_matrix[1] = 0;
	r_projection_matrix[5] = (2*zNear) / (ymax - ymin);
	r_projection_matrix[9] = (ymax + ymin) / (ymax - ymin);
	r_projection_matrix[13] = 0;

	r_projection_matrix[2] = 0;
	r_projection_matrix[6] = 0;
	r_projection_matrix[10] = -1  * nudge;
	r_projection_matrix[14] = -2*zNear * nudge;

	r_projection_matrix[3] = 0;
	r_projection_matrix[7] = 0;
	r_projection_matrix[11] = -1;
	r_projection_matrix[15] = 0;
}

void GL_ParallelPerspective(double xmin, double xmax, double ymax, double ymin,
		     double znear, double zfar)
{
	r_projection_matrix[0] = 2/(xmax-xmin);
	r_projection_matrix[4] = 0;
	r_projection_matrix[8] = 0;
	r_projection_matrix[12] = (xmax+xmin)/(xmax-xmin);

	r_projection_matrix[1] = 0;
	r_projection_matrix[5] = 2/(ymax-ymin);
	r_projection_matrix[9] = 0;
	r_projection_matrix[13] = (ymax+ymin)/(ymax-ymin);

	r_projection_matrix[2] = 0;
	r_projection_matrix[6] = 0;
	r_projection_matrix[10] = -2/(zfar-znear);
	r_projection_matrix[14] = (zfar+znear)/(zfar-znear);

	r_projection_matrix[3] = 0;
	r_projection_matrix[7] = 0;
	r_projection_matrix[11] = 0;
	r_projection_matrix[15] = 1;
}
*/


/*
===============
R_TextureAnimation

Returns the proper texture for a given time and base texture
===============
*/
extern entity_t *currententity;
texture_t *R_TextureAnimation (texture_t *base)
{
	int		reletive;
	int		count;

	if (currententity->framestate.g[FS_REG].frame[0])
	{
		if (base->alternate_anims)
			base = base->alternate_anims;
	}

	if (!base->anim_total)
		return base;

	reletive = (int)(cl.time*10) % base->anim_total;

	count = 0;
	while (base->anim_min > reletive || base->anim_max <= reletive)
	{
		base = base->anim_next;
		if (!base)
			Sys_Error ("R_TextureAnimation: broken cycle");
		if (++count > 100)
			Sys_Error ("R_TextureAnimation: infinite cycle");
	}

	return base;
}





mleaf_t		*r_viewleaf, *r_oldviewleaf;
mleaf_t		*r_viewleaf2, *r_oldviewleaf2;
int		r_viewcluster, r_viewcluster2, r_oldviewcluster, r_oldviewcluster2;
int r_visframecount;
mleaf_t		*r_vischain;		// linked list of visible leafs
static qbyte	curframevis[MAX_MAP_LEAFS/8];

/*
===============
R_MarkLeaves
===============
*/
#ifdef Q3BSPS
qbyte *R_MarkLeaves_Q3 (void)
{
	static qbyte	*vis;
	int		i;

	int cluster;
	mleaf_t	*leaf;

	if (r_oldviewcluster == r_viewcluster && !r_novis.value && r_viewcluster != -1)
		return vis;

	// development aid to let you run around and see exactly where
	// the pvs ends
//		if (r_lockpvs->value)
//			return;

	r_vischain = NULL;
	r_visframecount++;
	r_oldviewcluster = r_viewcluster;

	if (r_novis.ival || r_viewcluster == -1 || !cl.worldmodel->vis )
	{
		// mark everything
		for (i=0,leaf=cl.worldmodel->leafs ; i<cl.worldmodel->numleafs ; i++, leaf++)
		{
			if (!leaf->nummarksurfaces)
			{
				continue;
			}

			leaf->visframe = r_visframecount;
			leaf->vischain = r_vischain;
			r_vischain = leaf;
		}
	}
	else
	{
		vis = CM_ClusterPVS (cl.worldmodel, r_viewcluster, curframevis, sizeof(curframevis));
		for (i=0,leaf=cl.worldmodel->leafs ; i<cl.worldmodel->numleafs ; i++, leaf++)
		{
			cluster = leaf->cluster;
			if (cluster == -1 || !leaf->nummarksurfaces)
			{
				continue;
			}
			if (vis[cluster>>3] & (1<<(cluster&7)))
			{
				leaf->visframe = r_visframecount;
				leaf->vischain = r_vischain;
				r_vischain = leaf;
			}
		}
	}
	return vis;
}
#endif

#ifdef Q2BSPS
qbyte *R_MarkLeaves_Q2 (void)
{
	static qbyte	*vis;
	mnode_t	*node;
	int		i;

	int cluster;
	mleaf_t	*leaf;

	int c;

	if (r_oldviewcluster == r_viewcluster && r_oldviewcluster2 == r_viewcluster2)
		return vis;

	r_oldviewcluster = r_viewcluster;
	r_oldviewcluster2 = r_viewcluster2;

	if (r_novis.ival == 2)
		return vis;
	r_visframecount++;
	if (r_novis.ival || r_viewcluster == -1 || !cl.worldmodel->vis)
	{
		// mark everything
		for (i=0 ; i<cl.worldmodel->numleafs ; i++)
			cl.worldmodel->leafs[i].visframe = r_visframecount;
		for (i=0 ; i<cl.worldmodel->numnodes ; i++)
			cl.worldmodel->nodes[i].visframe = r_visframecount;
		return vis;
	}

	vis = CM_ClusterPVS (cl.worldmodel, r_viewcluster, curframevis, sizeof(curframevis));
	// may have to combine two clusters because of solid water boundaries
	if (r_viewcluster2 != r_viewcluster)
	{
		vis = CM_ClusterPVS (cl.worldmodel, r_viewcluster2, NULL, sizeof(curframevis));
		c = (cl.worldmodel->numleafs+31)/32;
		for (i=0 ; i<c ; i++)
			((int *)curframevis)[i] |= ((int *)vis)[i];
		vis = curframevis;
	}

	for (i=0,leaf=cl.worldmodel->leafs ; i<cl.worldmodel->numleafs ; i++, leaf++)
	{
		cluster = leaf->cluster;
		if (cluster == -1)
			continue;
		if (vis[cluster>>3] & (1<<(cluster&7)))
		{
			node = (mnode_t *)leaf;
			do
			{
				if (node->visframe == r_visframecount)
					break;
				node->visframe = r_visframecount;
				node = node->parent;
			} while (node);
		}
	}
	return vis;
}
#endif

qbyte *R_CalcVis_Q1 (void)
{
	unsigned int i;
	static qbyte	*vis;
	r_visframecount++;
	if (r_oldviewleaf == r_viewleaf && r_oldviewleaf2 == r_viewleaf2)
	{
	}
	else
	{
		r_oldviewleaf = r_viewleaf;
		r_oldviewleaf2 = r_viewleaf2;

		if (r_novis.ival&1)
		{
			vis = curframevis;
			memset (vis, 0xff, (cl.worldmodel->numleafs+7)>>3);
		}
		else if (r_viewleaf2 && r_viewleaf2 != r_viewleaf)
		{
			int c;
			Q1BSP_LeafPVS (cl.worldmodel, r_viewleaf2, curframevis, sizeof(curframevis));
			vis = Q1BSP_LeafPVS (cl.worldmodel, r_viewleaf, NULL, sizeof(curframevis));
			c = (cl.worldmodel->numleafs+31)/32;
			for (i=0 ; i<c ; i++)
				((int *)curframevis)[i] |= ((int *)vis)[i];
			vis = curframevis;
		}
		else
			vis = Q1BSP_LeafPVS (cl.worldmodel, r_viewleaf, curframevis, sizeof(curframevis));
	}
	return vis;
}

qbyte *R_MarkLeaves_Q1 (void)
{
	qbyte	fatvis[MAX_MAP_LEAFS/8];
	static qbyte	*vis;
	mnode_t	*node;
	int		i;
	qbyte	solid[4096];

	if (((r_oldviewleaf == r_viewleaf && r_oldviewleaf2 == r_viewleaf2) && !r_novis.ival) || r_novis.ival & 2)
		return vis;

	r_visframecount++;

	r_oldviewleaf = r_viewleaf;
	r_oldviewleaf2 = r_viewleaf2;

	if (r_novis.ival)
	{
		vis = solid;
		memset (solid, 0xff, (cl.worldmodel->numleafs+7)>>3);
	}
	else if (r_viewleaf2 && r_viewleaf2 != r_viewleaf)
	{
		int c;
		Q1BSP_LeafPVS (cl.worldmodel, r_viewleaf2, fatvis, sizeof(fatvis));
		vis = Q1BSP_LeafPVS (cl.worldmodel, r_viewleaf, NULL, 0);
		c = (cl.worldmodel->numleafs+31)/32;
		for (i=0 ; i<c ; i++)
			((int *)fatvis)[i] |= ((int *)vis)[i];

		vis = fatvis;
	}
	else
		vis = Q1BSP_LeafPVS (cl.worldmodel, r_viewleaf, NULL, 0);

	for (i=0 ; i<cl.worldmodel->numleafs ; i++)
	{
		if (vis[i>>3] & (1<<(i&7)))
		{
			node = (mnode_t *)&cl.worldmodel->leafs[i+1];
			do
			{
				if (node->visframe == r_visframecount)
					break;
				node->visframe = r_visframecount;
				node = node->parent;
			} while (node);
		}
	}
	return vis;
}


mplane_t	frustum[4];


/*
=================
R_CullBox

Returns true if the box is completely outside the frustom
=================
*/
qboolean R_CullBox (vec3_t mins, vec3_t maxs)
{
	int		i;

	for (i=0 ; i<4 ; i++)
		if (BOX_ON_PLANE_SIDE (mins, maxs, &frustum[i]) == 2)
			return true;
	return false;
}

qboolean R_CullSphere (vec3_t org, float radius)
{
	//four frustrum planes all point inwards in an expanding 'cone'.
	int		i;
	float d;

	for (i=0 ; i<4 ; i++)
	{
		d = DotProduct(frustum[i].normal, org)-frustum[i].dist;
		if (d <= -radius)
			return true;
	}
	return false;
}

qboolean R_CullEntityBox(entity_t *e, vec3_t modmins, vec3_t modmaxs)
{
	int i;
	vec3_t wmin, wmax;
	float fmin, fmax;

	//convert the model's bbox to the expanded maximum size of the entity, as drawn with this model.
	//The result is an axial box, which we pass to R_CullBox

	for (i = 0; i < 3; i++)
	{
		fmin = DotProduct(modmins, e->axis[i]);
		fmax = DotProduct(modmaxs, e->axis[i]);

		if (fmin > -16)
			fmin = -16;
		if (fmax < 16)
			fmax = 16;

		if (fmin < fmax)
		{
			wmin[i] = e->origin[i]+fmin;
			wmax[i] = e->origin[i]+fmax;
		}
		else
		{       //box went inside out
			wmin[i] = e->origin[i]+fmax;
			wmax[i] = e->origin[i]+fmin;
		}
	}


	return R_CullBox(wmin, wmax);
}




int SignbitsForPlane (mplane_t *out)
{
	int	bits, j;

	// for fast box on planeside test

	bits = 0;
	for (j=0 ; j<3 ; j++)
	{
		if (out->normal[j] < 0)
			bits |= 1<<j;
	}
	return bits;
}
#if 1
void R_SetFrustum (float projmat[16], float viewmat[16])
{
	float scale;
	int i;
	float mvp[16];

	if (r_novis.ival & 4)
		return;

	Matrix4_Multiply(projmat, viewmat, mvp);

	for (i = 0; i < 4; i++)
	{
		if (i & 1)
		{
			frustum[i].normal[0]	= mvp[3] + mvp[0+i/2];
			frustum[i].normal[1]	= mvp[7] + mvp[4+i/2];
			frustum[i].normal[2]	= mvp[11] + mvp[8+i/2];
			frustum[i].dist			= mvp[15] + mvp[12+i/2];
		}
		else
		{
			frustum[i].normal[0]	= mvp[3] - mvp[0+i/2];
			frustum[i].normal[1]	= mvp[7] - mvp[4+i/2];
			frustum[i].normal[2]	= mvp[11] - mvp[8+i/2];
			frustum[i].dist			= mvp[15] - mvp[12+i/2];
		}

		scale = 1/sqrt(DotProduct(frustum[i].normal, frustum[i].normal));
		frustum[i].normal[0] *= scale;
		frustum[i].normal[1] *= scale;
		frustum[i].normal[2] *= scale;
		frustum[i].dist	*= -scale;

		frustum[i].type = PLANE_ANYZ;
		frustum[i].signbits = SignbitsForPlane (&frustum[i]);
	}
}
#else
void R_SetFrustum (void)
{
	int		i;

	if (r_novis.ival & 4)
		return;

	/*	removed - assumes fov_x == fov_y
	if (r_refdef.fov_x == 90)
	{
		// front side is visible

		VectorAdd (vpn, vright, frustum[0].normal);
		VectorSubtract (vpn, vright, frustum[1].normal);

		VectorAdd (vpn, vup, frustum[2].normal);
		VectorSubtract (vpn, vup, frustum[3].normal);
	}
	else
		*/
	{

		// rotate VPN right by FOV_X/2 degrees
		RotatePointAroundVector( frustum[0].normal, vup, vpn, -(90-r_refdef.fov_x / 2 ) );
		// rotate VPN left by FOV_X/2 degrees
		RotatePointAroundVector( frustum[1].normal, vup, vpn, 90-r_refdef.fov_x / 2 );
		// rotate VPN up by FOV_X/2 degrees
		RotatePointAroundVector( frustum[2].normal, vright, vpn, 90-r_refdef.fov_y / 2 );
		// rotate VPN down by FOV_X/2 degrees
		RotatePointAroundVector( frustum[3].normal, vright, vpn, -( 90 - r_refdef.fov_y / 2 ) );
	}

	for (i=0 ; i<4 ; i++)
	{
		frustum[i].type = PLANE_ANYZ;
		frustum[i].dist = DotProduct (r_origin, frustum[i].normal);
		frustum[i].signbits = SignbitsForPlane (&frustum[i]);
	}
}
#endif




#include "glquake.h"

//we could go for nice smooth round particles... but then we would loose a little bit of the chaotic nature of the particles.
static qbyte	dottexture[8][8] =
{
	{0,0,0,0,0,0,0,0},
	{0,0,0,1,1,0,0,0},
	{0,0,1,1,1,1,0,0},
	{0,1,1,1,1,1,1,0},
	{0,1,1,1,1,1,1,0},
	{0,0,1,1,1,1,0,0},
	{0,0,0,1,1,0,0,0},
	{0,0,0,0,0,0,0,0},
};
static qbyte	exptexture[16][16] =
{
	{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
	{0,0,0,0,1,0,0,0,1,0,0,1,0,0,0,0},
	{0,0,0,1,1,1,1,1,3,1,1,2,1,0,0,0},
	{0,0,0,1,1,1,1,4,4,4,5,4,2,1,1,0},
	{0,0,1,1,6,5,5,8,6,8,3,6,3,2,1,0},
	{0,0,1,5,6,7,5,6,8,8,8,3,3,1,0,0},
	{0,0,0,1,6,8,9,9,9,9,4,6,3,1,0,0},
	{0,0,2,1,7,7,9,9,9,9,5,3,1,0,0,0},
	{0,0,2,4,6,8,9,9,9,9,8,6,1,0,0,0},
	{0,0,2,2,3,5,6,8,9,8,8,4,4,1,0,0},
	{0,0,1,2,4,1,8,7,8,8,6,5,4,1,0,0},
	{0,1,1,1,7,8,1,6,7,5,4,7,1,0,0,0},
	{0,1,2,1,1,5,1,3,4,3,1,1,0,0,0,0},
	{0,0,0,0,0,1,1,1,1,1,0,0,0,0,0,0},
	{0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0},
	{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
};

texid_t			particletexture;	// little dot for particles
texid_t			particlecqtexture;	// little dot for particles
texid_t			explosiontexture;
texid_t			balltexture;
texid_t			beamtexture;
texid_t			ptritexture;
void R_InitParticleTexture (void)
{
#define PARTICLETEXTURESIZE 64
	int		x,y;
	float dx, dy, d;
	qbyte	data[PARTICLETEXTURESIZE*PARTICLETEXTURESIZE][4];

	//
	// particle texture
	//
	for (x=0 ; x<8 ; x++)
	{
		for (y=0 ; y<8 ; y++)
		{
			data[y*8+x][0] = 255;
			data[y*8+x][1] = 255;
			data[y*8+x][2] = 255;
			data[y*8+x][3] = dottexture[x][y]*255;
		}
	}

	particletexture = R_LoadTexture32("", 8, 8, data, IF_NOMIPMAP|IF_NOPICMIP);


	//
	// particle triangle texture
	//

	// clear to transparent white
	for (x = 0; x < 32 * 32; x++)
	{
			data[x][0] = 255;
			data[x][1] = 255;
			data[x][2] = 255;
			data[x][3] = 0;
	}
	//draw a circle in the top left.
	for (x=0 ; x<16 ; x++)
	{
		for (y=0 ; y<16 ; y++)
		{
			if ((x - 7.5) * (x - 7.5) + (y - 7.5) * (y - 7.5) <= 8 * 8)
				data[y*32+x][3] = 255;
		}
	}

	particlecqtexture = R_LoadTexture32("", 32, 32, data, IF_NOMIPMAP|IF_NOPICMIP);





	for (x=0 ; x<16 ; x++)
	{
		for (y=0 ; y<16 ; y++)
		{
			data[y*16+x][0] = 255;
			data[y*16+x][1] = 255;
			data[y*16+x][2] = 255;
			data[y*16+x][3] = exptexture[x][y]*255/9.0;
		}
	}
	explosiontexture = R_LoadTexture32("", 16, 16, data, IF_NOMIPMAP|IF_NOPICMIP);

	memset(data, 255, sizeof(data));
	for (y = 0;y < PARTICLETEXTURESIZE;y++)
	{
		dy = (y - 0.5f*PARTICLETEXTURESIZE) / (PARTICLETEXTURESIZE*0.5f-1);
		for (x = 0;x < PARTICLETEXTURESIZE;x++)
		{
			dx = (x - 0.5f*PARTICLETEXTURESIZE) / (PARTICLETEXTURESIZE*0.5f-1);
			d = 256 * (1 - (dx*dx+dy*dy));
			d = bound(0, d, 255);
			data[y*PARTICLETEXTURESIZE+x][3] = (qbyte) d;
		}
	}
	balltexture = R_LoadTexture32("", PARTICLETEXTURESIZE, PARTICLETEXTURESIZE, data, IF_NOMIPMAP|IF_NOPICMIP);

	memset(data, 255, sizeof(data));
	for (y = 0;y < PARTICLETEXTURESIZE;y++)
	{
		dy = (y - 0.5f*PARTICLETEXTURESIZE) / (PARTICLETEXTURESIZE*0.5f-1);
		d = 256 * (1 - (dy*dy));
		d = bound(0, d, 255);
		for (x = 0;x < PARTICLETEXTURESIZE;x++)
		{
			data[y*PARTICLETEXTURESIZE+x][3] = (qbyte) d;
		}
	}
	beamtexture = R_LoadTexture32("", PARTICLETEXTURESIZE, PARTICLETEXTURESIZE, data, IF_NOMIPMAP|IF_NOPICMIP);

	for (y = 0;y < PARTICLETEXTURESIZE;y++)
	{
		dy = y / (PARTICLETEXTURESIZE*0.5f-1);
		d = 256 * (1 - (dy*dy));
		d = bound(0, d, 255);
		for (x = 0;x < PARTICLETEXTURESIZE;x++)
		{
			dx = x / (PARTICLETEXTURESIZE*0.5f-1);
			d = 256 * (1 - (dx+dy));
			d = bound(0, d, 255);
			data[y*PARTICLETEXTURESIZE+x][0] = (qbyte) d;
			data[y*PARTICLETEXTURESIZE+x][1] = (qbyte) d;
			data[y*PARTICLETEXTURESIZE+x][2] = (qbyte) d;
			data[y*PARTICLETEXTURESIZE+x][3] = (qbyte) d/2;
		}
	}
	ptritexture = R_LoadTexture32("", PARTICLETEXTURESIZE, PARTICLETEXTURESIZE, data, IF_NOMIPMAP|IF_NOPICMIP);

	Cvar_ForceCallback(&r_particlesystem);
}

