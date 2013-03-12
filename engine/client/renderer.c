#include "quakedef.h"
#include "winquake.h"
#include "pr_common.h"
#include "gl_draw.h"
#include "shader.h"
#include "glquake.h"
#include <string.h>


refdef_t	r_refdef;
vec3_t		r_origin, vpn, vright, vup;
entity_t	r_worldentity;
entity_t	*currententity;	//nnggh
int			r_framecount;
struct texture_s	*r_notexture_mip;

r_config_t	r_config;

qboolean	r_blockvidrestart;
int r_regsequence;

int rspeeds[RSPEED_MAX];
int rquant[RQUANT_MAX];

void R_InitParticleTexture (void);

qboolean vid_isfullscreen;

#define VIDCOMMANDGROUP "Video config"
#define GRAPHICALNICETIES "Graphical Nicaties"	//or eyecandy, which ever you prefer.
#define GLRENDEREROPTIONS	"GL Renderer Options"
#define SCREENOPTIONS	"Screen Options"

unsigned int	d_8to24rgbtable[256];

extern int gl_anisotropy_factor;

// callbacks used for cvars
void SCR_Viewsize_Callback (struct cvar_s *var, char *oldvalue);
void SCR_Fov_Callback (struct cvar_s *var, char *oldvalue);
#if defined(GLQUAKE)
void GL_Mipcap_Callback (struct cvar_s *var, char *oldvalue);
void GL_Texturemode_Callback (struct cvar_s *var, char *oldvalue);
void GL_Texturemode2d_Callback (struct cvar_s *var, char *oldvalue);
void GL_Texture_Anisotropic_Filtering_Callback (struct cvar_s *var, char *oldvalue);
#endif

cvar_t _vid_wait_override					= CVARAF  ("vid_wait", "1",
													   "_vid_wait_override", CVAR_ARCHIVE);

cvar_t _windowed_mouse						= CVARF ("_windowed_mouse","1",
													 CVAR_ARCHIVE);

cvar_t con_ocranaleds						= CVAR  ("con_ocranaleds", "2");

cvar_t cl_cursor							= CVAR  ("cl_cursor", "");
cvar_t cl_cursorsize						= CVAR  ("cl_cursorsize", "32");
cvar_t cl_cursorbias						= CVAR  ("cl_cursorbias", "4");

cvar_t gl_nocolors							= CVARF  ("gl_nocolors", "0", CVAR_ARCHIVE);
cvar_t gl_part_flame						= CVARFD  ("gl_part_flame", "1", CVAR_ARCHIVE, "Enable particle emitting from models. Mainly used for torch and flame effects.");

//opengl library, blank means try default.
static cvar_t gl_driver						= CVARF ("gl_driver", "",
													 CVAR_ARCHIVE | CVAR_RENDERERLATCH);
cvar_t gl_shadeq1_name						= CVAR  ("gl_shadeq1_name", "*");
extern cvar_t r_vertexlight;

cvar_t mod_md3flags							= CVAR  ("mod_md3flags", "1");

cvar_t r_ambient							= CVARF ("r_ambient", "0",
												CVAR_CHEAT);
cvar_t r_bloodstains						= CVARF  ("r_bloodstains", "1", CVAR_ARCHIVE);
cvar_t r_bouncysparks						= CVARFD ("r_bouncysparks", "0",
												CVAR_ARCHIVE,
												"Enables particle interaction with world surfaces, allowing for bouncy particles.");
cvar_t r_drawentities						= CVAR  ("r_drawentities", "1");
cvar_t r_drawflat							= CVARF ("r_drawflat", "0",
												CVAR_ARCHIVE | CVAR_SEMICHEAT | CVAR_RENDERERCALLBACK | CVAR_SHADERSYSTEM);
cvar_t r_wireframe							= CVARF ("r_wireframe", "0",
												CVAR_CHEAT);
cvar_t gl_miptexLevel						= CVAR  ("gl_miptexLevel", "0");
cvar_t r_drawviewmodel						= CVARF  ("r_drawviewmodel", "1", CVAR_ARCHIVE);
cvar_t r_drawviewmodelinvis					= CVAR  ("r_drawviewmodelinvis", "0");
cvar_t r_dynamic							= CVARF ("r_dynamic", IFMINIMAL("0","1"),
												CVAR_ARCHIVE);
cvar_t r_fastturb							= CVARF ("r_fastturb", "0",
												CVAR_SHADERSYSTEM);
cvar_t r_fastsky							= CVARF ("r_fastsky", "0",
												CVAR_ARCHIVE | CVAR_SHADERSYSTEM);
cvar_t r_fastskycolour						= CVARF ("r_fastskycolour", "0",
												CVAR_RENDERERCALLBACK|CVAR_SHADERSYSTEM);
cvar_t r_fb_bmodels							= CVARAF("r_fb_bmodels", "1",
													"gl_fb_bmodels", CVAR_SEMICHEAT|CVAR_RENDERERLATCH);
cvar_t r_fb_models							= CVARAF  ("r_fb_models", "1",
													"gl_fb_models", CVAR_SEMICHEAT);
cvar_t r_skin_overlays						= SCVARF  ("r_skin_overlays", "1",
												CVAR_SEMICHEAT|CVAR_RENDERERLATCH);
cvar_t r_coronas							= SCVARF ("r_coronas", "0",
												CVAR_ARCHIVE);
cvar_t r_flashblend							= SCVARF ("gl_flashblend", "0",
												CVAR_ARCHIVE);
cvar_t r_flashblendscale					= SCVARF ("gl_flashblendscale", "0.35",
												CVAR_ARCHIVE);
cvar_t r_floorcolour						= CVARAF ("r_floorcolour", "64 64 128",
													"r_floorcolor", CVAR_RENDERERCALLBACK|CVAR_SHADERSYSTEM);
cvar_t r_floortexture						= SCVARF ("r_floortexture", "",
												CVAR_RENDERERCALLBACK|CVAR_SHADERSYSTEM);
cvar_t r_fullbright							= SCVARF ("r_fullbright", "0",
												CVAR_CHEAT|CVAR_SHADERSYSTEM);
cvar_t r_fullbrightSkins					= SCVARF ("r_fullbrightSkins", "0.8", /*don't default to 1, as it looks a little ugly (too bright), but don't default to 0 either because then you're handicapped in the dark*/
												CVAR_SEMICHEAT|CVAR_SHADERSYSTEM);
cvar_t r_lightmap_saturation				= SCVAR  ("r_lightmap_saturation", "1");
cvar_t r_lightstylesmooth					= CVARF  ("r_lightstylesmooth", "0", CVAR_ARCHIVE);
cvar_t r_lightstylesmooth_limit				= SCVAR  ("r_lightstylesmooth_limit", "2");
cvar_t r_lightstylespeed					= SCVAR  ("r_lightstylespeed", "10");
cvar_t r_loadlits							= CVARF  ("r_loadlit", "1", CVAR_ARCHIVE);
cvar_t r_menutint							= SCVARF ("r_menutint", "0.68 0.4 0.13",
												CVAR_RENDERERCALLBACK);
cvar_t r_netgraph							= SCVAR  ("r_netgraph", "0");
cvar_t r_nolerp								= CVARF  ("r_nolerp", "0", CVAR_ARCHIVE);
cvar_t r_noframegrouplerp					= CVARF  ("r_noframegrouplerp", "0", CVAR_ARCHIVE);
cvar_t r_nolightdir							= CVARF  ("r_nolightdir", "0", CVAR_ARCHIVE);
cvar_t r_novis								= CVARF ("r_novis", "0", CVAR_ARCHIVE);
cvar_t r_part_rain							= CVARFD ("r_part_rain", "0",
												CVAR_ARCHIVE,
												"Enable particle effects to emit off of surfaces. Mainly used for weather or lava/slime effects.");
cvar_t r_skyboxname							= SCVARF ("r_skybox", "",
												CVAR_RENDERERCALLBACK | CVAR_SHADERSYSTEM);
cvar_t r_speeds								= SCVAR ("r_speeds", "0");
cvar_t r_stainfadeammount					= SCVAR  ("r_stainfadeammount", "1");
cvar_t r_stainfadetime						= SCVAR  ("r_stainfadetime", "1");
cvar_t r_stains								= CVARFC("r_stains", IFMINIMAL("0","0.75"),
												CVAR_ARCHIVE,
												Cvar_Limiter_ZeroToOne_Callback);
cvar_t r_postprocshader						= CVARD("r_postprocshader", "", "Specifies a shader to use as a post-processing shader");
cvar_t r_wallcolour							= CVARAF ("r_wallcolour", "128 128 128",
													  "r_wallcolor", CVAR_RENDERERCALLBACK|CVAR_SHADERSYSTEM);//FIXME: broken
cvar_t r_walltexture						= CVARF ("r_walltexture", "",
												CVAR_RENDERERCALLBACK|CVAR_SHADERSYSTEM);	//FIXME: broken
cvar_t r_wateralpha							= CVARF  ("r_wateralpha", "1",
												CVAR_ARCHIVE | CVAR_SHADERSYSTEM);
cvar_t r_waterwarp							= CVARF ("r_waterwarp", "1",
												CVAR_ARCHIVE);

cvar_t r_replacemodels						= CVARF ("r_replacemodels", IFMINIMAL("","md3 md2"),
												CVAR_ARCHIVE);

//otherwise it would defeat the point.
cvar_t scr_allowsnap						= CVARF ("scr_allowsnap", "1",
												CVAR_NOTFROMSERVER);
cvar_t scr_centersbar						= CVAR  ("scr_centersbar", "2");
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

#ifdef ANDROID
cvar_t vid_conautoscale						= CVARF ("vid_conautoscale", "2",
												CVAR_ARCHIVE | CVAR_RENDERERCALLBACK);
#else
cvar_t vid_conautoscale						= CVARFD ("vid_conautoscale", "0",
												CVAR_ARCHIVE | CVAR_RENDERERCALLBACK, "Changes the 2d scale, including hud, console, and fonts. To specify an explicit font size, divide the desired 'point' size by 8 to get the scale. High values will be clamped to maintain at least a 320*200 virtual size.");
#endif
cvar_t vid_conheight						= CVARF ("vid_conheight", "0",
												CVAR_ARCHIVE);
cvar_t vid_conwidth							= CVARF ("vid_conwidth", "0",
												CVAR_ARCHIVE | CVAR_RENDERERCALLBACK);
//see R_RestartRenderer_f for the effective default 'if (newr.renderer == -1)'.
cvar_t vid_renderer							= CVARF ("vid_renderer", "",
												CVAR_ARCHIVE | CVAR_RENDERERLATCH);

cvar_t vid_bpp								= CVARF ("vid_bpp", "32",
												CVAR_ARCHIVE | CVAR_RENDERERLATCH);
cvar_t vid_desktopsettings					= CVARF ("vid_desktopsettings", "0",
												CVAR_ARCHIVE | CVAR_RENDERERLATCH);
#ifdef NACL
cvar_t vid_fullscreen						= CVARF ("vid_fullscreen", "0",
												CVAR_ARCHIVE);
#else
//these cvars will be given their names when they're registered, based upon whether -plugin was used. this means code can always use vid_fullscreen without caring, but gets saved properly.
cvar_t vid_fullscreen						= CVARAF (NULL, "1", "vid_fullscreen",
												CVAR_ARCHIVE | CVAR_RENDERERLATCH);
cvar_t vid_fullscreen_alternative			= CVARF (NULL, "1",
												CVAR_ARCHIVE);
#endif
cvar_t vid_height							= CVARF ("vid_height", "0",
												CVAR_ARCHIVE | CVAR_RENDERERLATCH);
cvar_t vid_multisample						= CVARF ("vid_multisample", "0",
												CVAR_ARCHIVE | CVAR_RENDERERLATCH);
cvar_t vid_refreshrate						= CVARF ("vid_displayfrequency", "0",
												CVAR_ARCHIVE | CVAR_RENDERERLATCH);
cvar_t vid_wndalpha							= CVAR ("vid_wndalpha", "1");
//more readable defaults to match conwidth/conheight.
cvar_t vid_width							= CVARF ("vid_width", "0",
												CVAR_ARCHIVE | CVAR_RENDERERLATCH);

cvar_t	r_stereo_separation					= CVARD("r_stereo_separation", "4", "How far your eyes are apart, in quake units. A non-zero value will enable stereoscoping rendering. You might need some of them retro 3d glasses. Hardware support is recommended, see r_stereo_context.");
cvar_t	r_stereo_method						= CVARD("r_stereo_method", "0", "Value 0 = Off.\nValue 1 = Attempt hardware acceleration. Requires vid_restart.\nValue 2 = red/cyan.\nValue 3 = red/blue. Value 4=red/green");

extern cvar_t r_dodgytgafiles;
extern cvar_t r_dodgypcxfiles;
extern cvar_t r_drawentities;
extern cvar_t r_drawviewmodel;
extern cvar_t r_drawworld;
extern cvar_t r_fullbright;
cvar_t	r_mirroralpha = SCVARF("r_mirroralpha","1", CVAR_CHEAT|CVAR_SHADERSYSTEM);
extern cvar_t r_netgraph;
extern cvar_t r_norefresh;
extern cvar_t r_novis;
extern cvar_t r_speeds;
extern cvar_t r_waterwarp;

#ifdef ANDROID
//on android, these numbers seem to be generating major weirdness, so disable these.
cvar_t	r_polygonoffset_submodel_factor = SCVAR("r_polygonoffset_submodel_factor", "0");
cvar_t	r_polygonoffset_submodel_offset = SCVAR("r_polygonoffset_submodel_offset", "0");
#else
cvar_t	r_polygonoffset_submodel_factor = SCVAR("r_polygonoffset_submodel_factor", "0.05");
cvar_t	r_polygonoffset_submodel_offset = SCVAR("r_polygonoffset_submodel_offset", "25");
#endif

cvar_t	r_polygonoffset_stencil_factor = SCVAR("r_polygonoffset_stencil_factor", "0.01");
cvar_t	r_polygonoffset_stencil_offset = SCVAR("r_polygonoffset_stencil_offset", "1");

rendererstate_t currentrendererstate;

#if defined(GLQUAKE)
cvar_t	gl_workaround_ati_shadersource		= CVARD	 ("gl_workaround_ati_shadersource", "1", "Work around ATI driver bugs in the glShaderSource function. Can safely be enabled with other drivers too.");
cvar_t	vid_gl_context_version				= SCVAR  ("vid_gl_context_version", "");
cvar_t	vid_gl_context_forwardcompatible	= SCVAR  ("vid_gl_context_forwardcompatible", "0");
cvar_t	vid_gl_context_compatibility		= SCVAR  ("vid_gl_context_compatibility", "1");
cvar_t	vid_gl_context_debug				= SCVAR  ("vid_gl_context_debug", "0");	//for my ati drivers, debug 1 only works if version >= 3
cvar_t	vid_gl_context_es2					= SCVAR  ("vid_gl_context_es2", "0"); //requires version set correctly, no debug, no compat
#endif

#if defined(GLQUAKE) || defined(D3DQUAKE)
cvar_t gl_ati_truform						= CVAR  ("gl_ati_truform", "0");
cvar_t gl_ati_truform_type					= CVAR  ("gl_ati_truform_type", "1");
cvar_t gl_ati_truform_tesselation			= CVAR  ("gl_ati_truform_tesselation", "3");
cvar_t gl_blend2d							= CVAR  ("gl_blend2d", "1");
cvar_t gl_blendsprites						= CVAR  ("gl_blendsprites", "0");
cvar_t r_deluxemapping						= CVARAF ("r_deluxemapping", "0", "r_glsl_deluxemapping",
												CVAR_ARCHIVE | CVAR_RENDERERLATCH);
cvar_t gl_compress							= CVARF ("gl_compress", "0",
												CVAR_ARCHIVE);
cvar_t gl_conback							= CVARFC ("gl_conback", "",
												CVAR_RENDERERCALLBACK, R2D_Conback_Callback);
cvar_t gl_contrast							= CVAR  ("gl_contrast", "1");
cvar_t gl_brightness						= CVAR  ("gl_brightness", "0");
cvar_t gl_detail							= CVARF ("gl_detail", "0",
												CVAR_ARCHIVE);
cvar_t gl_detailscale						= CVAR  ("gl_detailscale", "5");
cvar_t gl_font								= CVARFD ("gl_font", "",
													  CVAR_RENDERERCALLBACK, ("Specifies the font file to use. a value such as FONT:ALTFONT specifies an alternative font to be used when ^^a is used.\n"
													  "When using TTF fonts, you will likely need to scale text to at least 150% - vid_conautoscale 1.5 will do this.\n"
													  "TTF fonts may be loaded from your windows directory. \'gl_font cour:couri\' loads eg: c:\\windows\\fonts\\cour.ttf, and uses the italic version of courier for alternative text."
													  ));
cvar_t gl_lateswap							= CVAR  ("gl_lateswap", "0");
cvar_t gl_lerpimages						= CVARF  ("gl_lerpimages", "1", CVAR_ARCHIVE);
//cvar_t gl_lightmapmode						= SCVARF("gl_lightmapmode", "",
//												CVAR_ARCHIVE);
cvar_t gl_load24bit							= SCVARF ("gl_load24bit", "1",
												CVAR_ARCHIVE);

cvar_t	r_clear								= CVARAF("r_clear","0",
													 "gl_clear", 0);
cvar_t gl_max_size							= SCVARF  ("gl_max_size", "2048", CVAR_RENDERERLATCH);
cvar_t gl_maxshadowlights					= SCVARF ("gl_maxshadowlights", "2",
												CVAR_ARCHIVE);
cvar_t gl_menutint_shader					= SCVAR  ("gl_menutint_shader", "1");

//by setting to 64 or something, you can use this as a wallhack
cvar_t gl_mindist							= SCVARF ("gl_mindist", "4",
												CVAR_CHEAT);

cvar_t gl_motionblur						= SCVARF ("gl_motionblur", "0",
												CVAR_ARCHIVE);
cvar_t gl_motionblurscale					= SCVAR  ("gl_motionblurscale", "1");
cvar_t gl_overbright						= CVARFC ("gl_overbright", "1",
												CVAR_ARCHIVE,
												Surf_RebuildLightmap_Callback);
cvar_t gl_overbright_all					= SCVARF ("gl_overbright_all", "0",
												CVAR_ARCHIVE);
cvar_t gl_picmip							= CVARF  ("gl_picmip", "0", CVAR_ARCHIVE);
cvar_t gl_picmip2d							= CVARF  ("gl_picmip2d", "0", CVAR_ARCHIVE);
cvar_t gl_nohwblend							= SCVAR  ("gl_nohwblend","1");
cvar_t gl_savecompressedtex					= SCVAR  ("gl_savecompressedtex", "0");
cvar_t gl_schematics						= SCVAR  ("gl_schematics", "0");
cvar_t gl_skyboxdist						= SCVAR  ("gl_skyboxdist", "0");	//0 = guess.
cvar_t gl_smoothcrosshair					= SCVAR  ("gl_smoothcrosshair", "1");
cvar_t	gl_maxdist = SCVAR("gl_maxdist", "8192");

#ifdef SPECULAR
cvar_t gl_specular							= CVARF  ("gl_specular", "1", CVAR_ARCHIVE);
cvar_t gl_specular_fallback					= CVARF  ("gl_specular_fallback", "0.05", CVAR_ARCHIVE|CVAR_RENDERERLATCH);
#endif

// The callbacks are not in D3D yet (also ugly way of seperating this)
#ifdef GLQUAKE
cvar_t gl_texture_anisotropic_filtering		= CVARFC("gl_texture_anisotropic_filtering", "0",
												CVAR_ARCHIVE | CVAR_RENDERERCALLBACK,
												GL_Texture_Anisotropic_Filtering_Callback);
cvar_t gl_texturemode						= CVARFC("gl_texturemode", "GL_LINEAR_MIPMAP_NEAREST",
												CVAR_ARCHIVE | CVAR_RENDERERCALLBACK,
												GL_Texturemode_Callback);
cvar_t gl_mipcap							= CVARFC("d_mipcap", "0 1000",
												CVAR_ARCHIVE | CVAR_RENDERERCALLBACK,
												GL_Mipcap_Callback);
cvar_t gl_texturemode2d						= CVARFC("gl_texturemode2d", "GL_LINEAR",
												CVAR_ARCHIVE | CVAR_RENDERERCALLBACK,
												GL_Texturemode2d_Callback);
#endif

cvar_t vid_triplebuffer						= CVARAF ("vid_triplebuffer", "1",
												"gl_triplebuffer", CVAR_ARCHIVE);

cvar_t r_noportals							= SCVAR  ("r_noportals", "0");
cvar_t dpcompat_psa_ungroup					= SCVAR  ("dpcompat_psa_ungroup", "0");
cvar_t r_noaliasshadows						= SCVARF ("r_noaliasshadows", "0",
												CVAR_ARCHIVE);
cvar_t r_shadows						= SCVARF ("r_shadows", "0",
												CVAR_ARCHIVE);
cvar_t r_showbboxes							= CVARD("r_showbboxes", "0", "Debugging. Shows bounding boxes. 1=ssqc, 2=csqc. Red=solid, Green=stepping/toss/bounce, Blue=onground.");
cvar_t r_lightprepass						= CVARFD("r_lightprepass", "0", CVAR_SHADERSYSTEM, "Experimental. Attempt to use a different lighting mechanism.");

cvar_t r_shadow_bumpscale_basetexture		= SCVAR  ("r_shadow_bumpscale_basetexture", "4");
cvar_t r_shadow_bumpscale_bumpmap			= SCVAR  ("r_shadow_bumpscale_bumpmap", "10");

cvar_t r_glsl_offsetmapping					= CVARF  ("r_glsl_offsetmapping", "0", CVAR_ARCHIVE|CVAR_SHADERSYSTEM);
cvar_t r_glsl_offsetmapping_scale			= CVAR  ("r_glsl_offsetmapping_scale", "0.04");
cvar_t r_glsl_offsetmapping_reliefmapping = CVARF("r_glsl_offsetmapping_reliefmapping", "1", CVAR_ARCHIVE|CVAR_SHADERSYSTEM);
cvar_t r_glsl_turbscale						= CVARF  ("r_glsl_turbscale", "1", CVAR_ARCHIVE);

cvar_t r_shadow_realtime_world				= SCVARF ("r_shadow_realtime_world", "0", CVAR_ARCHIVE);
cvar_t r_shadow_realtime_world_shadows		= SCVARF ("r_shadow_realtime_world_shadows", "1", CVAR_ARCHIVE);
cvar_t r_shadow_realtime_world_lightmaps	= SCVARF ("r_shadow_realtime_world_lightmaps", "0", 0);
cvar_t r_shadow_realtime_dlight				= SCVARF ("r_shadow_realtime_dlight", "1", CVAR_ARCHIVE);
cvar_t r_shadow_realtime_dlight_shadows		= SCVARF ("r_shadow_realtime_dlight_shadows", "1", CVAR_ARCHIVE);
cvar_t r_shadow_realtime_dlight_ambient		= SCVAR ("r_shadow_realtime_dlight_ambient", "0");
cvar_t r_shadow_realtime_dlight_diffuse		= SCVAR ("r_shadow_realtime_dlight_diffuse", "1");
cvar_t r_shadow_realtime_dlight_specular	= SCVAR ("r_shadow_realtime_dlight_specular", "4");	//excessive, but noticable. its called stylized, okay? shiesh, some people
cvar_t r_editlights_import_radius			= SCVAR ("r_editlights_import_radius", "1");
cvar_t r_editlights_import_ambient			= SCVAR ("r_editlights_import_ambient", "0");
cvar_t r_editlights_import_diffuse			= SCVAR ("r_editlights_import_diffuse", "1");
cvar_t r_editlights_import_specular			= SCVAR ("r_editlights_import_specular", "1");	//excessive, but noticable. its called stylized, okay? shiesh, some people
cvar_t r_shadow_shadowmapping				= SCVARF ("debug_r_shadow_shadowmapping", "0", 0);
cvar_t r_sun_dir							= SCVAR ("r_sun_dir", "0.2 0.5 0.8");
cvar_t r_sun_colour							= SCVARF ("r_sun_colour", "0 0 0", CVAR_ARCHIVE);
cvar_t r_waterstyle							= CVARFD ("r_waterstyle", "1", CVAR_ARCHIVE|CVAR_SHADERSYSTEM, "Changes how water, slime, and teleporters are drawn. Possible values are:\n0: fastturb-style block colour.\n1: regular q1-style water.\n2: refraction(ripply and transparent)\n3: refraction with reflection at an angle\n4: ripplemapped without reflections (requires particle effects)\n5: ripples+reflections");
cvar_t r_lavastyle							= CVARFD ("r_lavastyle", "1", CVAR_ARCHIVE|CVAR_SHADERSYSTEM, "See r_waterstyle, but affects only lava.");

cvar_t r_vertexdlights						= SCVAR  ("r_vertexdlights", "0");

cvar_t vid_preservegamma					= SCVAR ("vid_preservegamma", "0");
cvar_t vid_hardwaregamma					= SCVARF ("vid_hardwaregamma", "1",
												CVAR_ARCHIVE | CVAR_RENDERERLATCH);
cvar_t vid_desktopgamma						= CVARFD ("vid_desktopgamma", "0",
												CVAR_ARCHIVE | CVAR_RENDERERLATCH, "Apply gamma ramps upon the desktop rather than the window.");

cvar_t r_fog_exp2							= CVARD ("r_fog_exp2", "1", "Expresses how fog fades with distance. 0 (matching DarkPlaces) is typically more realistic, while 1 (matching FitzQuake and others) is more common.");

extern cvar_t gl_dither;
cvar_t	gl_screenangle = SCVAR("gl_screenangle", "0");

#endif

#if defined(GLQUAKE) || defined(D3DQUAKE)
void GLD3DRenderer_Init(void)
{
	Cvar_Register (&gl_mindist, GLRENDEREROPTIONS);
	Cvar_Register (&gl_load24bit, GRAPHICALNICETIES);
	Cvar_Register (&gl_blendsprites, GLRENDEREROPTIONS);
}
#endif

#if defined(GLQUAKE)
void GLRenderer_Init(void)
{
	//gl-specific video vars
	Cvar_Register (&gl_workaround_ati_shadersource, GLRENDEREROPTIONS);
	Cvar_Register (&vid_gl_context_version, GLRENDEREROPTIONS);
	Cvar_Register (&vid_gl_context_debug, GLRENDEREROPTIONS);
	Cvar_Register (&vid_gl_context_forwardcompatible, GLRENDEREROPTIONS);
	Cvar_Register (&vid_gl_context_compatibility, GLRENDEREROPTIONS);
	Cvar_Register (&vid_gl_context_es2, GLRENDEREROPTIONS);

	//screen
	Cvar_Register (&vid_preservegamma, GLRENDEREROPTIONS);
	Cvar_Register (&vid_hardwaregamma, GLRENDEREROPTIONS);
	Cvar_Register (&vid_desktopgamma, GLRENDEREROPTIONS);

//renderer
	Cvar_Register (&r_norefresh, GLRENDEREROPTIONS);

	Cvar_Register (&gl_affinemodels, GLRENDEREROPTIONS);
	Cvar_Register (&gl_nohwblend, GLRENDEREROPTIONS);
	Cvar_Register (&gl_nocolors, GLRENDEREROPTIONS);
	Cvar_Register (&gl_finish, GLRENDEREROPTIONS);
	Cvar_Register (&gl_lateswap, GLRENDEREROPTIONS);
	Cvar_Register (&gl_lerpimages, GLRENDEREROPTIONS);
	Cvar_Register (&r_postprocshader, GLRENDEREROPTIONS);

	Cvar_Register (&dpcompat_psa_ungroup, GLRENDEREROPTIONS);
	Cvar_Register (&r_noframegrouplerp, GLRENDEREROPTIONS);
	Cvar_Register (&r_noportals, GLRENDEREROPTIONS);
	Cvar_Register (&r_noaliasshadows, GLRENDEREROPTIONS);
	Cvar_Register (&gl_maxshadowlights, GLRENDEREROPTIONS);
	Cvar_Register (&r_shadow_bumpscale_basetexture, GLRENDEREROPTIONS);
	Cvar_Register (&r_shadow_bumpscale_bumpmap, GLRENDEREROPTIONS);

	Cvar_Register (&gl_reporttjunctions, GLRENDEREROPTIONS);

	Cvar_Register (&gl_motionblur, GLRENDEREROPTIONS);
	Cvar_Register (&gl_motionblurscale, GLRENDEREROPTIONS);

	Cvar_Register (&gl_smoothcrosshair, GRAPHICALNICETIES);

	Cvar_Register (&r_deluxemapping, GRAPHICALNICETIES);
	Cvar_Register (&r_glsl_offsetmapping, GRAPHICALNICETIES);
	Cvar_Register (&r_glsl_offsetmapping_scale, GRAPHICALNICETIES);
	Cvar_Register (&r_glsl_offsetmapping_reliefmapping, GRAPHICALNICETIES);
	Cvar_Register (&r_glsl_turbscale, GRAPHICALNICETIES);


#ifdef R_XFLIP
	Cvar_Register (&r_xflip, GLRENDEREROPTIONS);
#endif
	Cvar_Register (&gl_specular, GRAPHICALNICETIES);
	Cvar_Register (&gl_specular_fallback, GRAPHICALNICETIES);

//	Cvar_Register (&gl_lightmapmode, GLRENDEREROPTIONS);

	Cvar_Register (&gl_picmip, GLRENDEREROPTIONS);
	Cvar_Register (&gl_picmip2d, GLRENDEREROPTIONS);

	Cvar_Register (&gl_mipcap, GLRENDEREROPTIONS);
	Cvar_Register (&gl_texturemode, GLRENDEREROPTIONS);
	Cvar_Register (&gl_texturemode2d, GLRENDEREROPTIONS);
	Cvar_Register (&gl_texture_anisotropic_filtering, GLRENDEREROPTIONS);
	Cvar_Register (&gl_savecompressedtex, GLRENDEREROPTIONS);
	Cvar_Register (&gl_compress, GLRENDEREROPTIONS);
	Cvar_Register (&gl_detail, GRAPHICALNICETIES);
	Cvar_Register (&gl_detailscale, GRAPHICALNICETIES);
	Cvar_Register (&gl_overbright, GRAPHICALNICETIES);
	Cvar_Register (&gl_overbright_all, GRAPHICALNICETIES);
	Cvar_Register (&gl_dither, GRAPHICALNICETIES);
	Cvar_Register (&r_fog_exp2, GLRENDEREROPTIONS);

	Cvar_Register (&gl_ati_truform, GRAPHICALNICETIES);
	Cvar_Register (&gl_ati_truform_type, GRAPHICALNICETIES);
	Cvar_Register (&gl_ati_truform_tesselation, GRAPHICALNICETIES);

	Cvar_Register (&gl_screenangle, GLRENDEREROPTIONS);

	Cvar_Register (&gl_skyboxdist, GLRENDEREROPTIONS);

	Cvar_Register (&r_wallcolour, GLRENDEREROPTIONS);
	Cvar_Register (&r_floorcolour, GLRENDEREROPTIONS);
	Cvar_Register (&r_walltexture, GLRENDEREROPTIONS);
	Cvar_Register (&r_floortexture, GLRENDEREROPTIONS);

	Cvar_Register (&r_vertexdlights, GLRENDEREROPTIONS);

	Cvar_Register (&gl_schematics, GLRENDEREROPTIONS);

	Cvar_Register (&r_vertexlight, GLRENDEREROPTIONS);

	Cvar_Register (&gl_blend2d, GLRENDEREROPTIONS);

	Cvar_Register (&gl_menutint_shader, GLRENDEREROPTIONS);

	R_BloomRegister();
}
#endif

void	R_InitTextures (void)
{
	int		x,y, m;
	qbyte	*dest;
	static char r_notexture_mip_mem[(sizeof(texture_t) + 16*16+8*8+4*4+2*2)];

// create a simple checkerboard texture for the default
	r_notexture_mip = (texture_t*)r_notexture_mip_mem;

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
	#ifdef AVAIL_JPEGLIB
		LibJPEG_Init();
	#endif
	#ifdef AVAIL_PNGLIB
		LibPNG_Init();
	#endif

	currentrendererstate.renderer = NULL;
	qrenderer = QR_NONE;

	r_blockvidrestart = true;
	Cmd_AddCommand("setrenderer", R_SetRenderer_f);
	Cmd_AddCommand("vid_restart", R_RestartRenderer_f);

#ifdef RTLIGHTS
	Cmd_AddCommand ("r_editlights_reload", R_ReloadRTLights_f);
	Cmd_AddCommand ("r_editlights_save", R_SaveRTLights_f);
	Cvar_Register (&r_editlights_import_radius, "Realtime Light editing/importing");
	Cvar_Register (&r_editlights_import_ambient, "Realtime Light editing/importing");
	Cvar_Register (&r_editlights_import_diffuse, "Realtime Light editing/importing");
	Cvar_Register (&r_editlights_import_specular, "Realtime Light editing/importing");

#endif
	Cmd_AddCommand("r_dumpshaders", Shader_WriteOutGenerics_f);

#if defined(GLQUAKE) || defined(D3DQUAKE)
	GLD3DRenderer_Init();
#endif
#if defined(GLQUAKE)
	GLRenderer_Init();
#endif

#ifdef SWQUAKE
	{
	extern cvar_t sw_interlace;
	extern cvar_t sw_vthread;
	extern cvar_t sw_fthreads;
	Cvar_Register(&sw_interlace, "Software Rendering Options");
	Cvar_Register(&sw_vthread, "Software Rendering Options");
	Cvar_Register(&sw_fthreads, "Software Rendering Options");
	}
#endif

	Cvar_Register (&gl_conback, GRAPHICALNICETIES);

	Cvar_Register (&r_novis, GLRENDEREROPTIONS);

	//but register ALL vid_ commands.
	Cvar_Register (&gl_driver, GLRENDEREROPTIONS);
	Cvar_Register (&_vid_wait_override, VIDCOMMANDGROUP);
	Cvar_Register (&_windowed_mouse, VIDCOMMANDGROUP);
	Cvar_Register (&vid_renderer, VIDCOMMANDGROUP);
	Cvar_Register (&vid_wndalpha, VIDCOMMANDGROUP);

#ifndef NACL
	if (COM_CheckParm("-plugin"))
	{
		vid_fullscreen.name = "vid_fullscreen_embedded";
		vid_fullscreen_alternative.name = "vid_fullscreen_standalone";
	}
	else
	{
		vid_fullscreen.name = "vid_fullscreen_standalone";
		vid_fullscreen_alternative.name = "vid_fullscreen_embedded";
	}
	Cvar_Register (&vid_fullscreen_alternative, VIDCOMMANDGROUP);
#endif
	Cvar_Register (&vid_fullscreen, VIDCOMMANDGROUP);
	Cvar_Register (&vid_bpp, VIDCOMMANDGROUP);

	Cvar_Register (&vid_conwidth, VIDCOMMANDGROUP);
	Cvar_Register (&vid_conheight, VIDCOMMANDGROUP);
	Cvar_Register (&vid_conautoscale, VIDCOMMANDGROUP);

	Cvar_Register (&vid_triplebuffer, VIDCOMMANDGROUP);
	Cvar_Register (&vid_width, VIDCOMMANDGROUP);
	Cvar_Register (&vid_height, VIDCOMMANDGROUP);
	Cvar_Register (&vid_refreshrate, VIDCOMMANDGROUP);
	Cvar_Register (&vid_multisample, GLRENDEREROPTIONS);

	Cvar_Register (&vid_desktopsettings, VIDCOMMANDGROUP);

	Cvar_Register (&r_mirroralpha, GLRENDEREROPTIONS);
	Cvar_Register (&r_skyboxname, GRAPHICALNICETIES);
	Cbuf_AddText("alias sky r_skybox\n", RESTRICT_LOCAL);	/*alternative name for users*/

	Cvar_Register(&r_dodgytgafiles, "Bug fixes");
	Cvar_Register(&r_dodgypcxfiles, "Bug fixes");
	Cvar_Register(&r_loadlits, GRAPHICALNICETIES);
	Cvar_Register(&r_lightstylesmooth, GRAPHICALNICETIES);
	Cvar_Register(&r_lightstylesmooth_limit, GRAPHICALNICETIES);
	Cvar_Register(&r_lightstylespeed, GRAPHICALNICETIES);

	Cvar_Register(&r_stains, GRAPHICALNICETIES);
	Cvar_Register(&r_stainfadetime, GRAPHICALNICETIES);
	Cvar_Register(&r_stainfadeammount, GRAPHICALNICETIES);
	Cvar_Register(&r_lightprepass, GLRENDEREROPTIONS);
	Cvar_Register (&r_coronas, GRAPHICALNICETIES);
	Cvar_Register (&r_flashblend, GRAPHICALNICETIES);
	Cvar_Register (&r_flashblendscale, GRAPHICALNICETIES);

	Cvar_Register (&r_shadow_realtime_world, GRAPHICALNICETIES);
	Cvar_Register (&r_shadow_realtime_world_shadows, GRAPHICALNICETIES);
	Cvar_Register (&r_shadow_realtime_dlight, GRAPHICALNICETIES);
	Cvar_Register (&r_shadow_realtime_dlight_ambient, GRAPHICALNICETIES);
	Cvar_Register (&r_shadow_realtime_dlight_diffuse, GRAPHICALNICETIES);
	Cvar_Register (&r_shadow_realtime_dlight_specular, GRAPHICALNICETIES);
	Cvar_Register (&r_shadow_realtime_dlight_shadows, GRAPHICALNICETIES);
	Cvar_Register (&r_shadow_realtime_world_lightmaps, GRAPHICALNICETIES);
	Cvar_Register (&r_shadow_shadowmapping, GRAPHICALNICETIES);
	Cvar_Register (&r_sun_dir, GRAPHICALNICETIES);
	Cvar_Register (&r_sun_colour, GRAPHICALNICETIES);
	Cvar_Register (&r_waterstyle, GRAPHICALNICETIES);
	Cvar_Register (&r_lavastyle, GRAPHICALNICETIES);
	Cvar_Register (&r_wireframe, GRAPHICALNICETIES);
	Cvar_Register (&r_stereo_separation, GRAPHICALNICETIES);
	Cvar_Register (&r_stereo_method, GRAPHICALNICETIES);

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
	Cvar_Register (&gl_contrast, GLRENDEREROPTIONS);
	Cvar_Register (&gl_brightness, GLRENDEREROPTIONS);


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
	Cvar_Register (&gl_shadeq1_name, GLRENDEREROPTIONS);

	Cvar_Register (&r_clear, GLRENDEREROPTIONS);
	Cvar_Register (&gl_max_size, GLRENDEREROPTIONS);
	Cvar_Register (&gl_maxdist, GLRENDEREROPTIONS);
	Cvar_Register (&gl_miptexLevel, GRAPHICALNICETIES);
	Cvar_Register (&r_drawflat, GRAPHICALNICETIES);
	Cvar_Register (&r_menutint, GRAPHICALNICETIES);

	Cvar_Register (&r_fb_bmodels, GRAPHICALNICETIES);
	Cvar_Register (&r_fb_models, GRAPHICALNICETIES);
	Cvar_Register (&r_skin_overlays, GRAPHICALNICETIES);
	Cvar_Register (&r_shadows, GRAPHICALNICETIES);

	Cvar_Register (&r_replacemodels, GRAPHICALNICETIES);

	Cvar_Register (&r_showbboxes, GLRENDEREROPTIONS);
	Cvar_Register (&r_polygonoffset_submodel_factor, GLRENDEREROPTIONS);
	Cvar_Register (&r_polygonoffset_submodel_offset, GLRENDEREROPTIONS);
	Cvar_Register (&r_polygonoffset_stencil_factor, GLRENDEREROPTIONS);
	Cvar_Register (&r_polygonoffset_stencil_offset, GLRENDEREROPTIONS);

// misc
	Cvar_Register(&con_ocranaleds, "Console controls");

	P_InitParticleSystem();
	R_InitTextures();
}

qboolean Renderer_Started(void)
{
	return !!currentrendererstate.renderer;
}

void Renderer_Start(void)
{
	r_blockvidrestart = false;
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


void	(*Draw_Init)				(void);
void	(*Draw_Shutdown)			(void);

//void	(*Draw_TinyCharacter)		(int x, int y, unsigned int num);

void	(*R_Init)					(void);
void	(*R_DeInit)					(void);
void	(*R_RenderView)				(void);		// must set r_refdef first

void	(*R_NewMap)					(void);
void	(*R_PreNewMap)				(void);

void	(*R_AddStain)				(vec3_t org, float red, float green, float blue, float radius);
void	(*R_LessenStains)			(void);

void	(*Mod_Init)					(void);
void	(*Mod_Shutdown)				(void);
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

	NULL,	//Draw_Init;
	NULL,	//Draw_Shutdown;

	NULL,	//R_LoadTexture
	NULL,	//R_LoadTexture8Pal24
	NULL,	//R_LoadTexture8Pal32
	NULL,	//R_LoadCompressed
	NULL,	//R_FindTexture
	NULL,	//R_AllocNewTexture
	NULL,	//R_Upload
	NULL,	//R_DestroyTexture

	NULL,	//R_Init;
	NULL,	//R_DeInit;
	NULL,	//R_RenderView;

	NULL,	//R_NewMap;
	NULL,	//R_PreNewMap


	NULL,	//R_AddStain;
	NULL,	//R_LessenStains;

#if defined(GLQUAKE) || defined(D3DQUAKE)
	RMod_Init,
	RMod_Shutdown,
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
	NULL, //VID_SetPalette,
	NULL, //VID_ShiftPalette,
	NULL, //VID_GetRGBInfo,


	NULL,	//set caption

	NULL,	//SCR_UpdateScreen;

	/*backend*/
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,

	""
};
rendererinfo_t *pdedicatedrendererinfo = &dedicatedrendererinfo;

#ifdef GLQUAKE
extern rendererinfo_t openglrendererinfo;
rendererinfo_t eglrendererinfo;
#endif
#ifdef D3DQUAKE
rendererinfo_t d3d9rendererinfo;
rendererinfo_t d3d11rendererinfo;
#endif
#ifdef SWQUAKE
rendererinfo_t swrendererinfo;
#endif

rendererinfo_t *rendererinfo[] =
{
#ifndef NPQTV
	&dedicatedrendererinfo,
#endif
#ifdef GLQUAKE
	&openglrendererinfo,
	&eglrendererinfo,
#endif
#ifdef D3DQUAKE
	&d3d9rendererinfo,
	&d3d11rendererinfo,
#endif
#ifdef SWQUAKE
	&swrendererinfo,
#endif
};


void R_SetRenderer(rendererinfo_t *ri)
{
	currentrendererstate.renderer = ri;
	if (!ri)
		ri = &dedicatedrendererinfo;

	qrenderer = ri->rtype;
	q_renderername = ri->name[0];

	Draw_Init				= ri->Draw_Init;
	Draw_Shutdown			= ri->Draw_Shutdown;

	R_Init					= ri->R_Init;
	R_DeInit				= ri->R_DeInit;
	R_RenderView			= ri->R_RenderView;
	R_NewMap				= ri->R_NewMap;
	R_PreNewMap				= ri->R_PreNewMap;

	R_AddStain				= ri->R_AddStain;
	R_LessenStains			= ri->R_LessenStains;

	VID_Init				= ri->VID_Init;
	VID_DeInit				= ri->VID_DeInit;
	VID_SetPalette			= ri->VID_SetPalette;
	VID_ShiftPalette		= ri->VID_ShiftPalette;
	VID_GetRGBInfo			= ri->VID_GetRGBInfo;
	VID_SetWindowCaption	= ri->VID_SetWindowCaption;

	Mod_Init				= ri->Mod_Init;
	Mod_Shutdown			= ri->Mod_Shutdown;
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

qbyte default_quakepal[768] =
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

	P_Shutdown();
	if (Mod_Shutdown)
		Mod_Shutdown();

	IN_Shutdown();

	if (R_DeInit)
	{
		TRACE(("dbg: R_ApplyRenderer: R_DeInit\n"));
		R_DeInit();
	}

	if (Draw_Shutdown)
		Draw_Shutdown();

	if (VID_DeInit)
	{
		TRACE(("dbg: R_ApplyRenderer: VID_DeInit\n"));
		VID_DeInit();
	}

	TRACE(("dbg: R_ApplyRenderer: SCR_DeInit\n"));
	SCR_DeInit();

	COM_FlushTempoaryPacks();

	W_Shutdown();
	if (h2playertranslations)
		BZ_Free(h2playertranslations);
	h2playertranslations = NULL;
	if (host_basepal)
		BZ_Free(host_basepal);
	host_basepal = NULL;
	Surf_ClearLightmaps();

	RQ_Shutdown();

	S_Shutdown();
}

void R_GenPaletteLookup(void)
{
	int r,g,b,i;
	unsigned char *pal = host_basepal;
	for (i=0 ; i<256 ; i++)
	{
		r = pal[0];
		g = pal[1];
		b = pal[2];
		pal += 3;

		d_8to24rgbtable[i] = (255<<24) + (r<<0) + (g<<8) + (b<<16);
	}
	d_8to24rgbtable[255] &= 0xffffff;	// 255 is transparent
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

	memset(&r_config, 0, sizeof(r_config));

	if (qrenderer != QR_NONE)	//graphics stuff only when not dedicated
	{
		qbyte *data;
#ifndef CLIENTONLY
		isDedicated = false;
#endif
		if (newr)
			Con_Printf("Setting mode %i*%i*%i*%i %s\n", newr->width, newr->height, newr->bpp, newr->rate, newr->renderer->description);

		if (host_basepal)
			BZ_Free(host_basepal);
		host_basepal = (qbyte *)FS_LoadMallocFile ("gfx/palette.lmp");
		if (!host_basepal)
			host_basepal = (qbyte *)FS_LoadMallocFile ("wad/playpal");
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
				//if (ReadPCXData(pcx, com_filesize, 256, VID_GRADES, colormap))
				goto q2colormap;	//skip the colormap.lmp file as we already read it
			}
		}

		{
			qbyte *colormap = (qbyte *)FS_LoadMallocFile ("gfx/colormap.lmp");
			if (!colormap)
			{
				vid.fullbright=0;
			}
			else
			{
				j = VID_GRADES-1;
				data = colormap + j*256;
				vid.fullbright = 0;
				for (i = 255; i >= 0; i--)
				{
					if (colormap[i] == data[i])
						vid.fullbright++;
					else
						break;
				}
			}
			BZ_Free(colormap);
		}

		R_GenPaletteLookup();

		if (h2playertranslations)
			BZ_Free(h2playertranslations);
		h2playertranslations = FS_LoadMallocFile ("gfx/player.lmp");

		if (vid.fullbright < 2)
			vid.fullbright = 0;	//transparent colour doesn't count.

q2colormap:

TRACE(("dbg: R_ApplyRenderer: Palette loaded\n"));

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
		RQ_Init();
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
	Cvar_ForceCallback(&r_particlesystem);

	CL_InitDlights();

TRACE(("dbg: R_ApplyRenderer: starting on client state\n"));
	if (cl.worldmodel)
	{
		cl.worldmodel = NULL;
		CL_ClearEntityLists();	//shouldn't really be needed, but we're paranoid

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
		for (i=1 ; i<MAX_CSMODELS ; i++)
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
			cl_static_entities[i].ent.model = NULL;
			if (cl_static_entities[i].mdlidx < 0)
			{
				if (cl_static_entities[i].mdlidx > -MAX_CSMODELS)
					cl_static_entities[i].ent.model = cl.model_csqcprecache[-cl_static_entities[i].mdlidx];
			}
			else
			{
				if (cl_static_entities[i].mdlidx < MAX_MODELS)
					cl_static_entities[i].ent.model = cl.model_precache[cl_static_entities[i].mdlidx];
			}
		}

		Skin_FlushAll();

#ifdef CSQC_DAT
		CSQC_RendererRestarted();
#endif
	}
	else
	{
#ifdef VM_UI
		UI_Reset();
#endif
	}

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

	case QR_DIRECT3D9:
		Con_Printf(	"\n"
					"-----------------------------\n"
					"Direct3d9 renderer initialized\n");
		break;
	case QR_DIRECT3D11:
		Con_Printf(	"\n"
					"-----------------------------\n"
					"Direct3d11 renderer initialized\n");
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
	if (r_blockvidrestart)
	{
		Con_Printf("Ignoring vid_restart from config\n");
		return;
	}

	M_Shutdown();
	memset(&newr, 0, sizeof(newr));

TRACE(("dbg: R_RestartRenderer_f\n"));

	Media_CaptureDemoEnd();

	Cvar_ApplyLatches(CVAR_RENDERERLATCH);

	newr.width = vid_width.value;
	newr.height = vid_height.value;

	newr.triplebuffer = vid_triplebuffer.value;
	newr.multisample = vid_multisample.value;
	newr.bpp = vid_bpp.value;
	newr.fullscreen = vid_fullscreen.value;
	newr.rate = vid_refreshrate.value;
	newr.stereo = (r_stereo_method.ival == 1);

	if (!*_vid_wait_override.string || _vid_wait_override.value < 0)
		newr.wait = -1;
	else
		newr.wait = _vid_wait_override.value;

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
		int i;
		if (*vid_renderer.string)
			Con_Printf("vid_renderer unsupported. Using default.\n");
		else
			Con_DPrintf("vid_renderer unset. Using default.\n");

		//gotta do this after main hunk is saved off.
		for (i = 0; i < sizeof(rendererinfo)/sizeof(rendererinfo[0]); i++)
		{
			if (rendererinfo[i]->name[0] && stricmp(rendererinfo[i]->name[0], "none"))
			{
				Cmd_ExecuteString(va("setrenderer %s\n", rendererinfo[i]->name[0]), RESTRICT_LOCAL);
				break;
			}
		}
		return;
	}

	// use desktop settings if set to 0 and not dedicated
	if (newr.renderer->rtype != QR_NONE)
	{
		int dbpp, dheight, dwidth, drate;
		extern qboolean isPlugin;

		if ((!newr.fullscreen && !vid_desktopsettings.value && !isPlugin) || !Sys_GetDesktopParameters(&dwidth, &dheight, &dbpp, &drate))
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
					Con_Printf("%s: %s\n", gl_driver.name, gl_driver.string);
				}
				else
					Sys_Error("Couldn't fall back to previous renderer\n");
			}
		}
	}

	Cvar_ApplyCallbacks(CVAR_RENDERERCALLBACK);
	SCR_EndLoadingPlaque();

	TRACE(("dbg: R_RestartRenderer_f success\n"));
	M_Reinit();
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
texture_t *R_TextureAnimation (int frame, texture_t *base)
{
	int		reletive;
	int		count;

	if (frame)
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




unsigned int	r_viewcontents;
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
	mnode_t *node;

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

#if 1
			for (node = (mnode_t*)leaf; node; node = node->parent)
			{
				if (node->visframe == r_visframecount)
					break;
				node->visframe = r_visframecount;
			}
#else
			leaf->visframe = r_visframecount;
			leaf->vischain = r_vischain;
			r_vischain = leaf;
#endif
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
#if 1
				for (node = (mnode_t*)leaf; node; node = node->parent)
				{
					if (node->visframe == r_visframecount)
						break;
					node->visframe = r_visframecount;
				}
#else
				leaf->visframe = r_visframecount;
				leaf->vischain = r_vischain;
				r_vischain = leaf;
#endif
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

	if (r_refdef.forcevis)
	{
		vis = r_refdef.forcedvis;

		r_oldviewcluster = 0;
		r_oldviewcluster2 = 0;
	}
	else
	{
		if (r_oldviewcluster == r_viewcluster && r_oldviewcluster2 == r_viewcluster2)
			return vis;

		r_oldviewcluster = r_viewcluster;
		r_oldviewcluster2 = r_viewcluster2;

		if (r_novis.ival == 2)
			return vis;

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
	}

	r_visframecount++;

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
	static qbyte	fatvis[2][MAX_MAP_LEAFS/8];
	static qbyte	*cvis[2];
	qbyte *vis;
	mnode_t	*node;
	int		i;
	qboolean portal = r_refdef.recurse;

	//for portals to work, we need two sets of any pvs caches
	//this means lights can still check pvs at the end of the frame despite recursing in the mean time
	//however, we still need to invalidate the cache because we only have one 'visframe' field in nodes.

	if (r_refdef.forcevis)
	{
		vis = cvis[portal] = r_refdef.forcedvis;

		r_oldviewleaf = NULL;
		r_oldviewleaf2 = NULL;
	}
	else
	{
		if (!portal)
		{
			if (((r_oldviewleaf == r_viewleaf && r_oldviewleaf2 == r_viewleaf2) && !r_novis.ival) || r_novis.ival & 2)
				return cvis[portal];

			r_oldviewleaf = r_viewleaf;
			r_oldviewleaf2 = r_viewleaf2;
		}
		else
		{
			r_oldviewleaf = NULL;
			r_oldviewleaf2 = NULL;
		}

		if (r_novis.ival)
		{
			vis = cvis[portal] = fatvis[portal];
			memset (fatvis[portal], 0xff, (cl.worldmodel->numleafs+7)>>3);

			r_oldviewleaf = NULL;
			r_oldviewleaf2 = NULL;
		}
		else if (r_viewleaf2 && r_viewleaf2 != r_viewleaf)
		{
			int c;
			Q1BSP_LeafPVS (cl.worldmodel, r_viewleaf2, fatvis[portal], sizeof(fatvis[portal]));
			vis = cvis[portal] = Q1BSP_LeafPVS (cl.worldmodel, r_viewleaf, NULL, 0);
			c = (cl.worldmodel->numleafs+31)/32;
			for (i=0 ; i<c ; i++)
				((int *)fatvis[portal])[i] |= ((int *)vis)[i];

			vis = cvis[portal] = fatvis[portal];
		}
		else
		{
			vis = cvis[portal] = Q1BSP_LeafPVS (cl.worldmodel, r_viewleaf, fatvis[portal], sizeof(fatvis[portal]));
		}
	}

	r_visframecount++;

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


mplane_t	frustum[FRUSTUMPLANES];


/*
=================
R_CullBox

Returns true if the box is completely outside the frustom
=================
*/
qboolean R_CullBox (vec3_t mins, vec3_t maxs)
{
	int		i;

	for (i=0 ; i<FRUSTUMPLANES ; i++)
		if (BOX_ON_PLANE_SIDE (mins, maxs, &frustum[i]) == 2)
			return true;
	return false;
}

qboolean R_CullSphere (vec3_t org, float radius)
{
	//four frustrum planes all point inwards in an expanding 'cone'.
	int		i;
	float d;

	for (i=0 ; i<FRUSTUMPLANES ; i++)
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

#if 1
	float mrad = 0, v;

	if (e->axis[0][0]==1 && e->axis[0][1]==0 && e->axis[0][2]==0 &&
		e->axis[1][0]==0 && e->axis[1][1]==1 && e->axis[1][2]==0 &&
		e->axis[2][0]==0 && e->axis[2][1]==0 && e->axis[2][2]==1)
	{
		for (i = 0; i < 3; i++)
		{
			wmin[i] = e->origin[i]+modmins[i]*e->scale;
			wmax[i] = e->origin[i]+modmaxs[i]*e->scale;
		}
	}
	else
	{
		for (i = 0; i < 3; i++)
		{
			v = fabs(modmins[i]);
			if (mrad < v)
				mrad = v;
			v = fabs(modmaxs[i]);
			if (mrad < v)
				mrad = v;
		}
		mrad *= e->scale;
		for (i = 0; i < 3; i++)
		{
			wmin[i] = e->origin[i]-mrad;
			wmax[i] = e->origin[i]+mrad;
		}
	}
#else
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
#endif

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

	if (r_refdef.recurse)
		return;

#if FRUSTUMPLANES > 4
	//do far plane
	//fog will not logically not actually reach 0, though precision issues will force it. we cut off at an exponant of -500
	if (r_refdef.gfog_rgbd[3] 
#ifdef TERRAIN
	&& cl.worldmodel && cl.worldmodel->terrain
#else
	&& 0
#endif
		)
	{
		float culldist;
		float fog;
		extern cvar_t r_fog_exp2;

		/*Documentation: the GLSL/GL will do this maths:
		float dist = 1024;
		if (r_fog_exp2.ival)
			fog = pow(2, -r_refdef.gfog_rgbd[3] * r_refdef.gfog_rgbd[3] * dist * dist * 1.442695);
		else
			fog = pow(2, -r_refdef.gfog_rgbd[3] * dist * 1.442695);
		*/

		//the fog factor cut-off where its pointless to allow it to get closer to 0 (0 is technically infinite)
		fog = 2/255.0f;

		//figure out the eyespace distance required to reach that fog value
		culldist = log(fog);
		if (r_fog_exp2.ival)
			culldist = sqrt(culldist / (-r_refdef.gfog_rgbd[3] * r_refdef.gfog_rgbd[3]));
		else
			culldist = culldist / (-r_refdef.gfog_rgbd[3]);
		//anything drawn beyond this point is fully obscured by fog

		frustum[4].normal[0] = mvp[3] - mvp[2];
		frustum[4].normal[1] = mvp[7] - mvp[6];
		frustum[4].normal[2] = mvp[11] - mvp[10];
		frustum[4].dist      = mvp[15] - mvp[14];

		scale = 1/sqrt(DotProduct(frustum[4].normal, frustum[4].normal));
		frustum[4].normal[0] *= scale;
		frustum[4].normal[1] *= scale;
		frustum[4].normal[2] *= scale;
//		frustum[4].dist *= scale;
		frustum[4].dist	= DotProduct(r_origin, frustum[4].normal)-culldist;

		frustum[4].type      = PLANE_ANYZ;
		frustum[4].signbits  = SignbitsForPlane (&frustum[4]);
	}
	else
	{	
		frustum[4].normal[0] = mvp[3] - mvp[2];
		frustum[4].normal[1] = mvp[7] - mvp[6];
		frustum[4].normal[2] = mvp[11] - mvp[10];
		frustum[4].dist      = mvp[15] - mvp[14];

		scale = 1/sqrt(DotProduct(frustum[4].normal, frustum[4].normal));
		frustum[4].normal[0] *= scale;
		frustum[4].normal[1] *= scale;
		frustum[4].normal[2] *= scale;
		frustum[4].dist *= -scale;

		frustum[4].type      = PLANE_ANYZ;
		frustum[4].signbits  = SignbitsForPlane (&frustum[4]);
	}
#endif
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

	TEXASSIGN(particletexture, R_LoadTexture32("", 8, 8, data, IF_NOMIPMAP|IF_NOPICMIP));


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
	explosiontexture = R_LoadTexture32("fte_fuzzyparticle", 16, 16, data, IF_NOMIPMAP|IF_NOPICMIP);

	for (x=0 ; x<16 ; x++)
	{
		for (y=0 ; y<16 ; y++)
		{
			data[y*16+x][0] = exptexture[x][y]*255/9.0;
			data[y*16+x][1] = exptexture[x][y]*255/9.0;
			data[y*16+x][2] = exptexture[x][y]*255/9.0;
			data[y*16+x][3] = exptexture[x][y]*255/9.0;
		}
	}
	R_LoadTexture32("fte_bloodparticle", 16, 16, data, IF_NOMIPMAP|IF_NOPICMIP);

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
	balltexture = R_LoadTexture32("balltexture", PARTICLETEXTURESIZE, PARTICLETEXTURESIZE, data, IF_NOMIPMAP|IF_NOPICMIP);

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
}

