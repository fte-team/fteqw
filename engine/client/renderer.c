#include "quakedef.h"
#include "winquake.h"
#include "pr_common.h"
#include "gl_draw.h"
#include "shader.h"
#include "glquake.h"
#include <string.h>


#define DEFAULT_WIDTH 640
#define DEFAULT_HEIGHT 480
#define DEFAULT_BPP 32

refdef_t	r_refdef;
vec3_t		r_origin, vpn, vright, vup;
entity_t	r_worldentity;
entity_t	*currententity;	//nnggh
int			r_framecount;
struct texture_s	*r_notexture_mip;

int	r_blockvidrestart;	//'block' is a bit of a misnomer. 0=filesystem, configs, cinematics, video are all okay as they are. 1=starting up, waiting for filesystem, will restart after. 2=configs execed, but still need cinematics. 3=video will be restarted without any other init needed
int r_regsequence;

int rspeeds[RSPEED_MAX];
int rquant[RQUANT_MAX];

void R_InitParticleTexture (void);
void R_RestartRenderer (rendererstate_t *newr);

qboolean vid_isfullscreen;

#define VIDCOMMANDGROUP "Video config"
#define GRAPHICALNICETIES "Graphical Nicaties"	//or eyecandy, which ever you prefer.
#define GLRENDEREROPTIONS	"GL Renderer Options"
#define SCREENOPTIONS	"Screen Options"

#define VKRENDEREROPTIONS	"Vulkan-Specific Renderer Options"

unsigned int	d_8to24rgbtable[256];
unsigned int	d_8to24bgrtable[256];

extern int gl_anisotropy_factor;

// callbacks used for cvars
void QDECL SCR_Viewsize_Callback (struct cvar_s *var, char *oldvalue);
void QDECL SCR_Fov_Callback (struct cvar_s *var, char *oldvalue);
void QDECL Image_TextureMode_Callback (struct cvar_s *var, char *oldvalue);
void QDECL R_SkyBox_Changed (struct cvar_s *var, char *oldvalue)
{
	Shader_NeedReload(false);
}
void R_ForceSky_f(void)
{
	if (Cmd_Argc() < 2)
	{
		extern cvar_t r_skyboxname;
		if (*r_skyboxname.string)
			Con_Printf("Current user skybox is %s\n", r_skyboxname.string);
		else if (*cl.skyname)
			Con_Printf("Current per-map skybox is %s\n", cl.skyname);
		else
			Con_Printf("no skybox forced.\n");
	}
	else
	{
		R_SetSky(Cmd_Argv(1));
	}
}

#ifdef FTE_TARGET_WEB	//webgl sucks too much to get a stable framerate without vsync.
cvar_t vid_vsync							= CVARAF  ("vid_vsync", "1",
													   "vid_wait", CVAR_ARCHIVE);
#else
cvar_t vid_vsync							= CVARAF  ("vid_vsync", "0",
													   "vid_wait", CVAR_ARCHIVE);
#endif

cvar_t _windowed_mouse						= CVARF ("_windowed_mouse","1",
													 CVAR_ARCHIVE);

cvar_t con_ocranaleds						= CVAR  ("con_ocranaleds", "2");

cvar_t cl_cursor							= CVAR  ("cl_cursor", "");
cvar_t cl_cursorscale						= CVAR  ("cl_cursor_scale", "0.2");
cvar_t cl_cursorbiasx						= CVAR  ("cl_cursor_bias_x", "7.5");
cvar_t cl_cursorbiasy						= CVAR  ("cl_cursor_bias_y", "0.8");

cvar_t gl_nocolors							= CVARF  ("gl_nocolors", "0", CVAR_ARCHIVE);
cvar_t gl_part_flame						= CVARFD  ("gl_part_flame", "1", CVAR_ARCHIVE, "Enable particle emitting from models. Mainly used for torch and flame effects.");

//opengl library, blank means try default.
static cvar_t gl_driver						= CVARF ("gl_driver", "",
													 CVAR_ARCHIVE | CVAR_RENDERERLATCH);
cvar_t gl_shadeq1_name						= CVARD  ("gl_shadeq1_name", "*", "Rename all surfaces from quake1 bsps using this pattern for the purposes of shader names.");
extern cvar_t r_vertexlight;
extern cvar_t r_forceprogramify;
extern cvar_t dpcompat_nopremulpics;

cvar_t mod_md3flags							= CVARD  ("mod_md3flags", "1", "The flags field of md3s was never officially defined. If this is set to 1, the flags will be treated identically to mdl files. Otherwise they will be ignored. Naturally, this is required to provide rotating pickups in quake.");

cvar_t r_ambient							= CVARF ("r_ambient", "0",
													CVAR_CHEAT);
cvar_t r_bloodstains						= CVARF  ("r_bloodstains", "1", CVAR_ARCHIVE);
cvar_t r_bouncysparks						= CVARFD ("r_bouncysparks", "1",
													CVAR_ARCHIVE,
													"Enables particle interaction with world surfaces, allowing for bouncy particles, stains, and decals.");
cvar_t r_drawentities						= CVAR  ("r_drawentities", "1");
cvar_t r_max_gpu_bones						= CVARD  ("r_max_gpu_bones", "", "Specifies the maximum number of bones that can be handled on the GPU. If empty, will guess.");
cvar_t r_drawflat							= CVARAF ("r_drawflat", "0", "gl_textureless",
													CVAR_ARCHIVE | CVAR_SEMICHEAT | CVAR_RENDERERCALLBACK | CVAR_SHADERSYSTEM);
cvar_t r_lightmap							= CVARF ("r_lightmap", "0",
													CVAR_ARCHIVE | CVAR_SEMICHEAT | CVAR_RENDERERCALLBACK | CVAR_SHADERSYSTEM);
cvar_t r_wireframe							= CVARFD ("r_wireframe", "0",
													CVAR_CHEAT, "Developer feature where everything is drawn with wireframe over the top. Only active where cheats are permitted.");
cvar_t r_wireframe_smooth					= CVAR ("r_wireframe_smooth", "0");
cvar_t r_refract_fbo						= CVARD ("r_refract_fbo", "1", "Use an fbo for refraction. If 0, just renders as a portal and uses a copy of the current framebuffer.");
cvar_t r_refractreflect_scale				= CVARD ("r_refractreflect_scale", "0.5", "Use a different scale for refraction and reflection. Because $reasons.");
cvar_t gl_miptexLevel						= CVAR  ("gl_miptexLevel", "0");
cvar_t r_drawviewmodel						= CVARF  ("r_drawviewmodel", "1", CVAR_ARCHIVE);
cvar_t r_drawviewmodelinvis					= CVAR  ("r_drawviewmodelinvis", "0");
cvar_t r_dynamic							= CVARFD ("r_dynamic", IFMINIMAL("0","1"),
													  CVAR_ARCHIVE, "-1: the engine will bypass dlights completely, allowing for better batching.\n0: no standard dlights at all.\n1: coloured dlights will be used, they may show through walls. These are not realtime things.\n2: The dlights will be forced to monochrome (this does not affect coronas/flashblends/rtlights attached to the same light).");
cvar_t r_fastturb							= CVARF ("r_fastturb", "0",
													CVAR_SHADERSYSTEM);
cvar_t r_fastsky							= CVARF ("r_fastsky", "0",
													CVAR_ARCHIVE);
cvar_t r_fastskycolour						= CVARF ("r_fastskycolour", "0",
													CVAR_RENDERERCALLBACK|CVAR_SHADERSYSTEM);
cvar_t r_fb_bmodels							= CVARAF("r_fb_bmodels", "1",
													"gl_fb_bmodels", CVAR_SEMICHEAT|CVAR_RENDERERLATCH);
cvar_t r_fb_models							= CVARAFD  ("r_fb_models", "1",
													"gl_fb_models", CVAR_SEMICHEAT, "Enables the use of lumas on models. Note that if ruleset_allow_fbmodels is enabled, then all models are unconditionally fullbright in deathmatch, because cheaters would set up their models like that anyway, hurrah for beating them at their own game. QuakeWorld players suck.");
cvar_t r_skin_overlays						= CVARF  ("r_skin_overlays", "1",
													CVAR_SEMICHEAT|CVAR_RENDERERLATCH);
cvar_t r_globalskin_first					= CVARFD  ("r_globalskin_first", "100", CVAR_RENDERERLATCH, "Specifies the first .skin value that is a global skin. Entities within this range will use the shader/image called 'gfx/skinSKIN.lmp' instead of their regular skin. See also: r_globalskin_count.");
cvar_t r_globalskin_count					= CVARFD  ("r_globalskin_count", "10", CVAR_RENDERERLATCH, "Specifies how many globalskins there are.");
cvar_t r_coronas							= CVARFD ("r_coronas", "0",	CVAR_ARCHIVE, "Draw coronas on realtime lights. Overrides glquake-esque flashblends.");
cvar_t r_coronas_occlusion					= CVARFD ("r_coronas_occlusion", "", CVAR_ARCHIVE, "Specifies that coronas should be occluded more carefully.\n0: No occlusion, at all.\n1: BSP occlusion only (simple tracelines).\n2: non-bsp occlusion also (complex tracelines).\n3: Depthbuffer reads (forces synchronisation).\n4: occlusion queries.");
cvar_t r_coronas_mindist					= CVARFD ("r_coronas_mindist", "128", CVAR_ARCHIVE, "Coronas closer than this will be invisible, preventing near clip plane issues.");
cvar_t r_coronas_fadedist					= CVARFD ("r_coronas_fadedist", "256", CVAR_ARCHIVE, "Coronas will fade out over this distance.");

cvar_t r_flashblend							= CVARF ("gl_flashblend", "0",
													CVAR_ARCHIVE);
cvar_t r_flashblendscale					= CVARF ("gl_flashblendscale", "0.35",
													CVAR_ARCHIVE);
cvar_t r_floorcolour						= CVARAF ("r_floorcolour", "64 64 128",
													"r_floorcolor", CVAR_RENDERERCALLBACK|CVAR_SHADERSYSTEM);
//cvar_t r_floortexture						= SCVARF ("r_floortexture", "",
//												CVAR_RENDERERCALLBACK|CVAR_SHADERSYSTEM);
cvar_t r_fullbright							= CVARFD ("r_fullbright", "0",
												CVAR_CHEAT|CVAR_SHADERSYSTEM, "Ignore world lightmaps, drawing *everything* fully lit.");
cvar_t r_fullbrightSkins					= CVARFD	("r_fullbrightSkins", "0.8", /*don't default to 1, as it looks a little ugly (too bright), but don't default to 0 either because then you're handicapped in the dark*/
												CVAR_SEMICHEAT|CVAR_SHADERSYSTEM, "Force the use of fullbright skins on other players. No more hiding in the dark.");
cvar_t r_lightmap_saturation				= CVAR	("r_lightmap_saturation", "1");
cvar_t r_lightstylesmooth					= CVARF	("r_lightstylesmooth", "0", CVAR_ARCHIVE);
cvar_t r_lightstylesmooth_limit				= CVAR	("r_lightstylesmooth_limit", "2");
cvar_t r_lightstylespeed					= CVAR	("r_lightstylespeed", "10");
cvar_t r_lightstylescale					= CVAR	("r_lightstylescale", "1");
cvar_t r_lightmap_scale						= CVARFD ("r_shadow_realtime_nonworld_lightmaps", "1", 0, "Scaler for lightmaps used when not using realtime world lighting. Probably broken.");
cvar_t r_hdr_irisadaptation					= CVARF	("r_hdr_irisadaptation", "0", CVAR_ARCHIVE);
cvar_t r_hdr_irisadaptation_multiplier		= CVAR	("r_hdr_irisadaptation_multiplier", "2");
cvar_t r_hdr_irisadaptation_minvalue		= CVAR	("r_hdr_irisadaptation_minvalue", "0.5");
cvar_t r_hdr_irisadaptation_maxvalue		= CVAR	("r_hdr_irisadaptation_maxvalue", "4");
cvar_t r_hdr_irisadaptation_fade_down		= CVAR	("r_hdr_irisadaptation_fade_down", "0.5");
cvar_t r_hdr_irisadaptation_fade_up			= CVAR	("r_hdr_irisadaptation_fade_up", "0.1");
cvar_t r_loadlits							= CVARF	("r_loadlit", "1", CVAR_ARCHIVE);
cvar_t r_menutint							= CVARF	("r_menutint", "0.68 0.4 0.13",
												CVAR_RENDERERCALLBACK);
cvar_t r_netgraph							= CVAR	("r_netgraph", "0");
extern cvar_t r_lerpmuzzlehack;
cvar_t r_nolerp								= CVARF	("r_nolerp", "0", CVAR_ARCHIVE);
cvar_t r_noframegrouplerp					= CVARF	("r_noframegrouplerp", "0", CVAR_ARCHIVE);
cvar_t r_nolightdir							= CVARF	("r_nolightdir", "0", CVAR_ARCHIVE);
cvar_t r_novis								= CVARF	("r_novis", "0", CVAR_ARCHIVE);
cvar_t r_part_rain							= CVARFD ("r_part_rain", "0",
												CVAR_ARCHIVE,
												"Enable particle effects to emit off of surfaces. Mainly used for weather or lava/slime effects.");
cvar_t r_skyboxname							= CVARFC ("r_skybox", "",
												CVAR_RENDERERCALLBACK | CVAR_SHADERSYSTEM, R_SkyBox_Changed);
cvar_t r_softwarebanding_cvar				= CVARFD ("r_softwarebanding", "0", CVAR_SHADERSYSTEM, "Utilise the Quake colormap in order to emulate 8bit software rendering. This results in banding as well as other artifacts that some believe adds character. Also forces nearest sampling on affected surfaces (palette indicies do not interpolate well).");
qboolean r_softwarebanding;
cvar_t r_speeds								= CVAR ("r_speeds", "0");
cvar_t r_stainfadeammount					= CVAR  ("r_stainfadeammount", "1");
cvar_t r_stainfadetime						= CVAR  ("r_stainfadetime", "1");
cvar_t r_stains								= CVARFC("r_stains", IFMINIMAL("0","0"),
												CVAR_ARCHIVE,
												Cvar_Limiter_ZeroToOne_Callback);
cvar_t r_renderscale						= CVARD("r_renderscale", "1", "Provides a way to enable subsampling or super-sampling");
cvar_t r_fxaa								= CVARD("r_fxaa", "0", "Runs a post-procesing pass to strip the jaggies.");
cvar_t r_postprocshader						= CVARD("r_postprocshader", "", "Specifies a custom shader to use as a post-processing shader");
cvar_t r_wallcolour							= CVARAF ("r_wallcolour", "128 128 128",
													  "r_wallcolor", CVAR_RENDERERCALLBACK|CVAR_SHADERSYSTEM);//FIXME: broken
//cvar_t r_walltexture						= CVARF ("r_walltexture", "",
//												CVAR_RENDERERCALLBACK|CVAR_SHADERSYSTEM);	//FIXME: broken
cvar_t r_wateralpha							= CVARF  ("r_wateralpha", "1",
												CVAR_ARCHIVE | CVAR_SHADERSYSTEM);
cvar_t r_lavaalpha							= CVARF  ("r_lavaalpha", "",
												CVAR_ARCHIVE | CVAR_SHADERSYSTEM);
cvar_t r_slimealpha							= CVARF  ("r_slimealpha", "",
												CVAR_ARCHIVE | CVAR_SHADERSYSTEM);
cvar_t r_telealpha							= CVARF  ("r_telealpha", "",
												CVAR_ARCHIVE | CVAR_SHADERSYSTEM);
cvar_t r_waterwarp							= CVARFD ("r_waterwarp", "1",
												CVAR_ARCHIVE, "Enables fullscreen warp, preferably via glsl. -1 specifies to force the fov warp fallback instead which can give a smidge more performance.");

cvar_t r_replacemodels						= CVARFD ("r_replacemodels", IFMINIMAL("","md3 md2"),
												CVAR_ARCHIVE, "A list of filename extensions to attempt to use instead of mdl.");

cvar_t gl_lightmap_nearest					= CVARFD ("gl_lightmap_nearest", "0", CVAR_ARCHIVE, "Use nearest sampling for lightmaps. This will give a more blocky look. Meaningless when gl_lightmap_average is enabled.");
cvar_t gl_lightmap_average					= CVARFD ("gl_lightmap_average", "0", CVAR_ARCHIVE, "Determine lightmap values based upon the center of the polygon. This will give a more buggy look, quite probably.");

//otherwise it would defeat the point.
cvar_t scr_allowsnap						= CVARF ("scr_allowsnap", "1",
												CVAR_NOTFROMSERVER);
cvar_t scr_centersbar						= CVAR  ("scr_centersbar", "2");
cvar_t scr_centertime						= CVAR  ("scr_centertime", "2");
cvar_t scr_logcenterprint					= CVARD  ("con_logcenterprint", "1", "Specifies whether to print centerprints on the console.\n0: never\n1: single-player or coop only.\n2: always.\n");
cvar_t scr_chatmodecvar						= CVAR  ("scr_chatmode", "0");
cvar_t scr_conalpha							= CVARC ("scr_conalpha", "0.7",
												Cvar_Limiter_ZeroToOne_Callback);
cvar_t scr_consize							= CVAR  ("scr_consize", "0.5");
cvar_t scr_conspeed							= CVAR  ("scr_conspeed", "2000");
// 10 - 170
cvar_t scr_fov								= CVARFCD("fov", "90", CVAR_ARCHIVE, SCR_Fov_Callback,
												"field of vision, 1-170 degrees, standard fov is 90, nquake defaults to 108.");
cvar_t scr_printspeed						= CVAR  ("scr_printspeed", "16");
cvar_t scr_showpause						= CVAR  ("showpause", "1");
cvar_t scr_showturtle						= CVAR  ("showturtle", "0");
cvar_t scr_turtlefps						= CVAR  ("scr_turtlefps", "10");
cvar_t scr_sshot_compression				= CVAR  ("scr_sshot_compression", "75");
cvar_t scr_sshot_type						= CVAR  ("scr_sshot_type", "png");
cvar_t scr_sshot_prefix						= CVAR  ("scr_sshot_prefix", "screenshots/fte-"); 
cvar_t scr_viewsize							= CVARFC("viewsize", "100", CVAR_ARCHIVE, SCR_Viewsize_Callback);

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
cvar_t vid_renderer							= CVARFD ("vid_renderer", "",
													 CVAR_ARCHIVE | CVAR_RENDERERLATCH, "Specifies which backend is used. Values that might work are: sv (dedicated server), headless (null renderer), vk (vulkan), gl (opengl), egl (opengl es), d3d9 (direct3d 9), d3d11 (direct3d 11, with default hardware rendering), d3d11 warp (direct3d 11, with software rendering).");
cvar_t vid_renderer_opts					= CVARFD ("_vid_renderer_opts", "", CVAR_NOSET, "The possible video renderer apis, in \"value\" \"description\" pairs, for gamecode to read.");

cvar_t vid_bpp								= CVARFD ("vid_bpp", "0",
												CVAR_ARCHIVE | CVAR_RENDERERLATCH, "The number of colour bits to request from the renedering context");
cvar_t vid_desktopsettings					= CVARFD ("vid_desktopsettings", "0",
												CVAR_ARCHIVE | CVAR_RENDERERLATCH, "Ignore the values of vid_width and vid_height, and just use the same settings that are used for the desktop.");
#ifdef NACL
cvar_t vid_fullscreen						= CVARF ("vid_fullscreen", "0",
												CVAR_ARCHIVE);
#else
//these cvars will be given their names when they're registered, based upon whether -plugin was used. this means code can always use vid_fullscreen without caring, but gets saved properly.
cvar_t vid_fullscreen						= CVARAFD (NULL, "1", "vid_fullscreen",
												CVAR_ARCHIVE | CVAR_RENDERERLATCH, "Whether to use fullscreen or not. A value of 2 specifies fullscreen windowed (aka borderless window) mode.");
cvar_t vid_fullscreen_alternative			= CVARFD (NULL, "1",
												CVAR_ARCHIVE, "Whether to use fullscreen or not. This cvar is saved to your config but not otherwise used in this operating mode.");
#endif
cvar_t vid_height							= CVARFD ("vid_height", "0",
												CVAR_ARCHIVE | CVAR_RENDERERLATCH, "The screen height to attempt to use, in physical pixels. 0 means use desktop resolution.");
cvar_t vid_multisample						= CVARFD ("vid_multisample", "0",
													CVAR_ARCHIVE, "The number of samples to use for Multisample AntiAliasing (aka: msaa). A value of 1 explicitly disables it.");
cvar_t vid_refreshrate						= CVARF ("vid_displayfrequency", "0",
												CVAR_ARCHIVE | CVAR_RENDERERLATCH);
cvar_t vid_srgb								= CVARFD ("vid_srgb", "0",
													  CVAR_ARCHIVE, "0: Off. Colour blending will be wrong.\n1: Only the framebuffer should use sRGB colourspace, textures and colours will be assumed to be linear. This has the effect of brightening the screen.\n2: Use sRGB extensions/support to ensure that the sh");
cvar_t vid_wndalpha							= CVARD ("vid_wndalpha", "1", "When running windowed, specifies the window's transparency level.");
#if defined(_WIN32) && defined(MULTITHREAD)
cvar_t vid_winthread						= CVARFD ("vid_winthread", "", CVAR_ARCHIVE|CVAR_RENDERERLATCH, "When enabled, window messages will be handled by a separate thread. This allows the game to continue rendering when Microsoft Windows blocks while resizing etc.");
#endif
//more readable defaults to match conwidth/conheight.
cvar_t vid_width							= CVARFD ("vid_width", "0",
												CVAR_ARCHIVE | CVAR_RENDERERLATCH, "The screen width to attempt to use, in physical pixels. 0 means use desktop resolution.");
cvar_t vid_dpi_x							= CVARFD ("vid_dpi_x", "0", CVAR_NOSET, "For mods that need to determine the physical screen size (like with touchscreens). 0 means unknown");
cvar_t vid_dpi_y							= CVARFD ("vid_dpi_y", "0", CVAR_NOSET, "For mods that need to determine the physical screen size (like with touchscreens). 0 means unknown");

cvar_t	r_stereo_separation					= CVARD("r_stereo_separation", "4", "How far apart your eyes are, in quake units. A non-zero value will enable stereoscoping rendering. You might need some of them retro 3d glasses. Hardware support is recommended, see r_stereo_context.");
cvar_t	r_stereo_convergence				= CVARD("r_stereo_convergence", "0", "Nudges the angle of each eye inwards when using stereoscopic rendering.");
cvar_t	r_stereo_method						= CVARFD("r_stereo_method", "0", CVAR_ARCHIVE, "Value 0 = Off.\nValue 1 = Attempt hardware acceleration. Requires vid_restart.\nValue 2 = red/cyan.\nValue 3 = red/blue.\nValue 4=red/green.\nValue 5=eye strain.");

extern cvar_t r_dodgytgafiles;
extern cvar_t r_dodgypcxfiles;
extern cvar_t r_dodgymiptex;
extern char *r_defaultimageextensions;
extern cvar_t r_imageexensions;
extern cvar_t r_image_downloadsizelimit;
extern cvar_t r_drawentities;
extern cvar_t r_drawviewmodel;
extern cvar_t r_drawworld;
extern cvar_t r_fullbright;
cvar_t	r_mirroralpha = CVARFD("r_mirroralpha","1", CVAR_CHEAT|CVAR_SHADERSYSTEM|CVAR_RENDERERLATCH, "Specifies how the default shader is generated for the 'window02_1' texture. Values less than 1 will turn it into a mirror.");
extern cvar_t r_netgraph;
cvar_t	r_norefresh = CVAR("r_norefresh","0");
extern cvar_t r_novis;
extern cvar_t r_speeds;
extern cvar_t r_waterwarp;

cvar_t	r_polygonoffset_submodel_factor = CVAR("r_polygonoffset_submodel_factor", "0");
cvar_t	r_polygonoffset_submodel_offset = CVAR("r_polygonoffset_submodel_offset", "0");
cvar_t	r_polygonoffset_shadowmap_offset = CVAR("r_polygonoffset_shadowmap_factor", "0.05");
cvar_t	r_polygonoffset_shadowmap_factor = CVAR("r_polygonoffset_shadowmap_offset", "0");

cvar_t	r_polygonoffset_stencil_factor = CVAR("r_polygonoffset_stencil_factor", "0.01");
cvar_t	r_polygonoffset_stencil_offset = CVAR("r_polygonoffset_stencil_offset", "1");

rendererstate_t currentrendererstate;

#if defined(GLQUAKE)
cvar_t	gl_workaround_ati_shadersource		= CVARD	 ("gl_workaround_ati_shadersource", "1", "Work around ATI driver bugs in the glShaderSource function. Can safely be enabled with other drivers too.");
cvar_t	vid_gl_context_version				= CVARD  ("vid_gl_context_version", "", "Specifies the version of OpenGL to try to create.");
cvar_t	vid_gl_context_forwardcompatible	= CVARD  ("vid_gl_context_forwardcompatible", "0", "Requests an opengl context with no depricated features enabled.");
cvar_t	vid_gl_context_compatibility		= CVARD  ("vid_gl_context_compatibility", "1", "Requests an OpenGL context with fixed-function backwards compat.");
cvar_t	vid_gl_context_debug				= CVARD  ("vid_gl_context_debug", "0", "Requests a debug opengl context. This provides better error oreporting.");	//for my ati drivers, debug 1 only works if version >= 3
cvar_t	vid_gl_context_es					= CVARD  ("vid_gl_context_es", "0", "Requests an OpenGLES context. Be sure to set vid_gl_context_version to 2 or so."); //requires version set correctly, no debug, no compat
cvar_t	vid_gl_context_robustness			= CVARD	("vid_gl_context_robustness", "1", "Attempt to enforce extra buffer protection in the gl driver, but can be slower with pre-gl3 hardware.");
cvar_t	vid_gl_context_selfreset			= CVARD	("vid_gl_context_selfreset", "1", "Upon hardware failure, have the engine create a new context instead of depending on the drivers to restore everything. This can help to avoid graphics drivers randomly killing your game, and can help reduce memory requirements.");
cvar_t	vid_gl_context_noerror				= CVARD	("vid_gl_context_noerror", "", "Disables OpenGL's error checks for a small performance speedup. May cause segfaults if stuff wasn't properly implemented/tested.");
#endif

#if 1
cvar_t r_tessellation						= CVARAFD  ("r_tessellation", "0", "gl_ati_truform", CVAR_SHADERSYSTEM, "Enables+controls the use of blinn tessellation on the fallback shader for meshes, equivelent to a shader with 'program defaultskin#TESS'. This will look stupid unless the meshes were actually designed for it and have suitable vertex normals.");
cvar_t gl_ati_truform_type					= CVAR  ("gl_ati_truform_type", "1");
cvar_t r_tessellation_level					= CVAR  ("r_tessellation_level", "5");
cvar_t gl_blend2d							= CVAR  ("gl_blend2d", "1");
cvar_t gl_blendsprites						= CVARD  ("gl_blendsprites", "0", "Blend sprites instead of alpha testing them");
cvar_t r_deluxmapping_cvar					= CVARAFD ("r_deluxmapping", "0", "r_deluxemapping",	//fixme: rename to r_glsl_deluxmapping once configs catch up
												CVAR_ARCHIVE, "Enables bumpmapping based upon precomputed light directions.\n0=off\n1=use if available\n2=auto-generate (if possible)");
qboolean r_deluxmapping;
cvar_t r_shaderblobs						= CVARD ("r_shaderblobs", "0", "If enabled, can massively accelerate vid restarts / loading (especially with the d3d renderer). Can cause issues when upgrading engine versions, so this is disabled by default.");
cvar_t gl_compress							= CVARFD ("gl_compress", "0", CVAR_ARCHIVE, "Enable automatic texture compression even for textures which are not pre-compressed.");
cvar_t gl_conback							= CVARFCD ("gl_conback", "",
												CVAR_RENDERERCALLBACK, R2D_Conback_Callback, "Specifies which conback shader/image to use. The Quake fallback is gfx/conback.lmp");
//cvar_t gl_detail							= CVARF ("gl_detail", "0",
//												CVAR_ARCHIVE);
//cvar_t gl_detailscale						= CVAR  ("gl_detailscale", "5");
cvar_t gl_font								= CVARFD ("gl_font", "",
													  CVAR_RENDERERCALLBACK|CVAR_ARCHIVE, ("Specifies the font file to use. a value such as FONT:ALTFONT specifies an alternative font to be used when ^^a is used.\n"
													  "When using TTF fonts, you will likely need to scale text to at least 150% - vid_conautoscale 1.5 will do this.\n"
													  "TTF fonts may be loaded from your windows directory. \'gl_font cour?col=1,1,1:couri?col=0,1,0\' loads eg: c:\\windows\\fonts\\cour.ttf, and uses the italic version of courier for alternative text, with specific colour tints."
													  ));
cvar_t gl_lateswap							= CVAR  ("gl_lateswap", "0");
cvar_t gl_lerpimages						= CVARFD  ("gl_lerpimages", "1", CVAR_ARCHIVE, "Enables smoother resampling for images which are not power-of-two, when the drivers do not support non-power-of-two textures.");
//cvar_t gl_lightmapmode						= SCVARF("gl_lightmapmode", "",
//												CVAR_ARCHIVE);
cvar_t gl_load24bit							= CVARF ("gl_load24bit", "1",
												CVAR_ARCHIVE);

cvar_t	r_clear								= CVARAF("r_clear","0",
													 "gl_clear", 0);
cvar_t gl_max_size							= CVARFD  ("gl_max_size", "8192", CVAR_RENDERERLATCH, "Specifies the maximum texture size that the engine may use. Textures larger than this will be downsized. Clamped by the value the driver supports.");
cvar_t gl_menutint_shader					= CVARD  ("gl_menutint_shader", "1", "Controls the use of GLSL to desaturate the background when drawing the menu, like quake's dos software renderer used to do before the ugly dithering of winquake.");

//by setting to 64 or something, you can use this as a wallhack
cvar_t gl_mindist							= CVARAD ("gl_mindist", "1", "r_nearclip",
												"Distance to the near clip plane. Smaller values may damage depth precision, high values can potentialy be used to see through walls...");

cvar_t gl_motionblur						= CVARF ("gl_motionblur", "0",
												CVAR_ARCHIVE);
cvar_t gl_motionblurscale					= CVAR  ("gl_motionblurscale", "1");
cvar_t gl_overbright						= CVARFC ("gl_overbright", "1",
												CVAR_ARCHIVE,
												Surf_RebuildLightmap_Callback);
cvar_t gl_overbright_all					= CVARF ("gl_overbright_all", "0",
												CVAR_ARCHIVE);
cvar_t gl_picmip							= CVARFD  ("gl_picmip", "0", CVAR_ARCHIVE, "Reduce world/model texture sizes by some exponential factor.");
cvar_t gl_picmip2d							= CVARFD  ("gl_picmip2d", "0", CVAR_ARCHIVE, "Reduce hud/menu texture sizes by some exponential factor.");
cvar_t gl_nohwblend							= CVARD  ("gl_nohwblend","1", "If 1, don't use hardware gamma ramps for transient effects that change each frame (does not affect long-term effects like holding quad or underwater tints).");
cvar_t gl_savecompressedtex					= CVARD  ("gl_savecompressedtex", "0", "Write out a copy of textures in a compressed format. The driver will do the compression on the fly, thus this setting is likely inferior to software which does not care so much about compression times.");
//cvar_t gl_schematics						= CVARD  ("gl_schematics", "0", "Gimmick rendering mode that draws the length of various world edges.");
cvar_t gl_skyboxdist						= CVARD  ("gl_skyboxdist", "0", "The distance of the skybox. If 0, the engine will determine it based upon the far clip plane distance.");	//0 = guess.
cvar_t gl_smoothcrosshair					= CVAR  ("gl_smoothcrosshair", "1");
cvar_t	gl_maxdist							= CVARD	("gl_maxdist", "0", "The distance of the far clip plane. If set to 0, some fancy maths will be used to place it at an infinite distance.");

#ifdef SPECULAR
cvar_t gl_specular							= CVARF  ("gl_specular", "0.3", CVAR_ARCHIVE);
cvar_t gl_specular_fallback					= CVARF  ("gl_specular_fallback", "0.05", CVAR_ARCHIVE|CVAR_RENDERERLATCH);
cvar_t gl_specular_fallbackexp				= CVARF  ("gl_specular_fallbackexp", "1", CVAR_ARCHIVE|CVAR_RENDERERLATCH);
#endif

// The callbacks are not in D3D yet (also ugly way of seperating this)
cvar_t gl_texture_anisotropic_filtering		= CVARFC("gl_texture_anisotropic_filtering", "0",
												CVAR_ARCHIVE | CVAR_RENDERERCALLBACK,
												Image_TextureMode_Callback);
cvar_t gl_texturemode						= CVARFCD("gl_texturemode", "GL_LINEAR_MIPMAP_LINEAR",
												CVAR_ARCHIVE | CVAR_RENDERERCALLBACK | CVAR_SAVE, Image_TextureMode_Callback,
												"Specifies how world/model textures appear. Typically 3 letters eg lln.\nFirst letter can be l(inear) or n(earest) and says how to sample from the mip (when downsampling).\nThe middle letter can . to disable mipmaps, or l or n to describe whether to blend between mipmaps.\nThe third letter says what to do when the texture is too low resolution and is thus the most noticable with low resolution textures, a n will make it look like lego, while an l will keep it smooth.");
cvar_t gl_mipcap							= CVARAFC("d_mipcap", "0 1000", "gl_miptexLevel",
												CVAR_ARCHIVE | CVAR_RENDERERCALLBACK,
												Image_TextureMode_Callback);
cvar_t gl_texturemode2d						= CVARFCD("gl_texturemode2d", "GL_LINEAR",
												CVAR_ARCHIVE | CVAR_RENDERERCALLBACK, Image_TextureMode_Callback,
												"Specifies how 2d images are sampled. format is a 3-tupple ");
cvar_t r_font_linear						= CVARF("r_font_linear", "1", CVAR_RENDERERLATCH);

cvar_t vid_triplebuffer						= CVARAFD ("vid_triplebuffer", "1", "gl_triplebuffer", CVAR_ARCHIVE, "Specifies whether the hardware is forcing tripplebuffering on us, this is the number of extra page swaps required before old data has been completely overwritten.");

cvar_t r_portalrecursion					= CVARD  ("r_portalrecursion", "1", "The number of portals the camera is allowed to recurse through.");
cvar_t r_portaldrawplanes					= CVARD  ("r_portaldrawplanes", "0", "Draw front and back planes in portals. Debug feature.");
cvar_t r_portalonly							= CVARD  ("r_portalonly", "0", "Don't draw things which are not portals. Debug feature.");
cvar_t dpcompat_psa_ungroup					= CVAR  ("dpcompat_psa_ungroup", "0");
cvar_t r_noaliasshadows						= CVARF ("r_noaliasshadows", "0", CVAR_ARCHIVE);
cvar_t r_shadows							= CVARFD ("r_shadows", "0",	CVAR_ARCHIVE, "Draw basic blob shadows underneath entities without using realtime lighting.");
cvar_t r_showbboxes							= CVARD("r_showbboxes", "0", "Debugging. Shows bounding boxes. 1=ssqc, 2=csqc. Red=solid, Green=stepping/toss/bounce, Blue=onground.");
cvar_t r_showfields							= CVARD("r_showfields", "0", "Debugging. Shows entity fields boxes (entity closest to crosshair). 1=ssqc, 2=csqc.");
cvar_t r_showshaders						= CVARD("r_showshaders", "0", "Debugging. Shows the name of the (worldmodel) shader being pointed to.");
cvar_t r_lightprepass_cvar					= CVARFD("r_lightprepass", "0", CVAR_SHADERSYSTEM, "Experimental. Attempt to use a different lighting mechanism.");
int r_lightprepass;

cvar_t r_shadow_bumpscale_basetexture		= CVARD  ("r_shadow_bumpscale_basetexture", "0", "bumpyness scaler for generation of fallback normalmap textures from models");
cvar_t r_shadow_bumpscale_bumpmap			= CVARD  ("r_shadow_bumpscale_bumpmap", "4", "bumpyness scaler for _bump textures");

cvar_t r_shadow_heightscale_basetexture		= CVARD  ("r_shadow_heightscale_basetexture", "0", "scaler for generation of height maps from legacy paletted content.");
cvar_t r_shadow_heightscale_bumpmap			= CVARD  ("r_shadow_heightscale_bumpmap", "1", "height scaler for 8bit _bump textures");

cvar_t r_glsl_offsetmapping					= CVARFD  ("r_glsl_offsetmapping", "0", CVAR_ARCHIVE|CVAR_SHADERSYSTEM, "Enables the use of paralax mapping, adding fake depth to textures.");
cvar_t r_glsl_offsetmapping_scale			= CVAR  ("r_glsl_offsetmapping_scale", "0.04");
cvar_t r_glsl_offsetmapping_reliefmapping	= CVARFD("r_glsl_offsetmapping_reliefmapping", "0", CVAR_ARCHIVE|CVAR_SHADERSYSTEM, "Changes the paralax sampling mode to be a bit nicer, but noticably more expensive at high resolutions. r_glsl_offsetmapping must be set.");
cvar_t r_glsl_turbscale_reflect				= CVARFD  ("r_glsl_turbscale_reflect", "1", CVAR_ARCHIVE, "Controls the strength of the water reflection ripples (used by the altwater glsl code).");
cvar_t r_glsl_turbscale_refract				= CVARFD  ("r_glsl_turbscale_refract", "1", CVAR_ARCHIVE, "Controls the strength of the underwater ripples (used by the altwater glsl code).");

cvar_t r_fastturbcolour						= CVARFD ("r_fastturbcolour", "0.1 0.2 0.3", CVAR_ARCHIVE, "The colour to use for water surfaces draw with r_waterstyle 0.\n");
cvar_t r_waterstyle							= CVARFD ("r_waterstyle", "1", CVAR_ARCHIVE|CVAR_SHADERSYSTEM, "Changes how water, and teleporters are drawn. Possible values are:\n0: fastturb-style block colour.\n1: regular q1-style water.\n2: refraction(ripply and transparent)\n3: refraction with reflection at an angle\n4: ripplemapped without reflections (requires particle effects)\n5: ripples+reflections");
cvar_t r_slimestyle							= CVARFD ("r_slimestyle", "", CVAR_ARCHIVE|CVAR_SHADERSYSTEM, "See r_waterstyle, but affects only slime. If empty, defers to r_waterstyle.");
cvar_t r_lavastyle							= CVARFD ("r_lavastyle", "1", CVAR_ARCHIVE|CVAR_SHADERSYSTEM, "See r_waterstyle, but affects only lava. If empty, defers to r_waterstyle.");
cvar_t r_telestyle							= CVARFD ("r_telestyle", "1", CVAR_ARCHIVE|CVAR_SHADERSYSTEM, "See r_waterstyle, but affects only teleporters. If empty, defers to r_waterstyle.");

cvar_t r_vertexdlights						= CVARD	("r_vertexdlights", "0", "Determine model lighting with respect to nearby dlights. Poor-man's rtlights.");

cvar_t vid_preservegamma					= CVARD ("vid_preservegamma", "0", "Restore initial hardware gamma ramps when quitting.");
cvar_t vid_hardwaregamma					= CVARFD ("vid_hardwaregamma", "1",
												CVAR_ARCHIVE | CVAR_RENDERERLATCH, "Use hardware gamma ramps. 0=ugly texture-based gamma, 1=glsl(windowed) or hardware(fullscreen), 2=always glsl, 3=always hardware gamma (disabled if hardware doesn't support).");
cvar_t vid_desktopgamma						= CVARFD ("vid_desktopgamma", "0",
												CVAR_ARCHIVE | CVAR_RENDERERLATCH, "Apply gamma ramps upon the desktop rather than the window.");

cvar_t r_fog_exp2							= CVARD ("r_fog_exp2", "1", "Expresses how fog fades with distance. 0 (matching DarkPlaces's default) is typically more realistic, while 1 (matching FitzQuake and others) is more common.");

extern cvar_t gl_dither;
cvar_t	gl_screenangle = CVAR("gl_screenangle", "0");
#endif

#ifdef VKQUAKE
cvar_t vk_stagingbuffers					= CVARD ("vk_stagingbuffers",			"", "Configures which dynamic buffers are copied into gpu memory for rendering, instead of reading from shared memory. Empty for default settings.\nAccepted chars are u, e, v, 0.");
cvar_t vk_submissionthread					= CVARD	("vk_submissionthread",			"", "Execute submits+presents on a thread dedicated to executing them. This may be a significant speedup on certain drivers.");
cvar_t vk_debug								= CVARD	("vk_debug",					"0", "Register a debug handler to display driver/layer messages. 2 enables the standard validation layers.");
cvar_t vk_dualqueue							= CVARD ("vk_dualqueue",				"", "Attempt to use a separate queue for presentation. Blank for default.");
cvar_t vk_busywait							= CVARD ("vk_busywait",					"", "Force busy waiting until the GPU finishes doing its thing.");
cvar_t vk_nv_glsl_shader					= CVARD	("vk_loadglsl",					"", "Enable direct loading of glsl, where supported by drivers. Do not use in combination with vk_debug 2 (vk_debug should be 1 if you want to see any glsl compile errors). Don't forget to do a vid_restart after.");
cvar_t vk_nv_dedicated_allocation			= CVARD	("vk_nv_dedicated_allocation",	"", "Flag vulkan memory allocations as dedicated, where applicable.");
//cvar_t vk_khr_dedicated_allocation		= CVARD	("vk_khr_dedicated_allocation",	"", "Flag vulkan memory allocations as dedicated, where applicable.");
cvar_t vk_khr_push_descriptor				= CVARD	("vk_khr_push_descriptor",		"", "Enables better descriptor streaming.");
#endif

#if defined(D3DQUAKE)
void GLD3DRenderer_Init(void)
{
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
	Cvar_Register (&vid_gl_context_es, GLRENDEREROPTIONS);
	Cvar_Register (&vid_gl_context_robustness, GLRENDEREROPTIONS);
	Cvar_Register (&vid_gl_context_selfreset, GLRENDEREROPTIONS);
	Cvar_Register (&vid_gl_context_noerror, GLRENDEREROPTIONS);

//renderer

	Cvar_Register (&gl_affinemodels, GLRENDEREROPTIONS);
	Cvar_Register (&gl_nohwblend, GLRENDEREROPTIONS);
	Cvar_Register (&gl_nocolors, GLRENDEREROPTIONS);
	Cvar_Register (&gl_finish, GLRENDEREROPTIONS);
	Cvar_Register (&gl_lateswap, GLRENDEREROPTIONS);
	Cvar_Register (&gl_lerpimages, GLRENDEREROPTIONS);

	Cvar_Register (&dpcompat_psa_ungroup, GLRENDEREROPTIONS);
	Cvar_Register (&r_lerpmuzzlehack, GLRENDEREROPTIONS);
	Cvar_Register (&r_noframegrouplerp, GLRENDEREROPTIONS);
	Cvar_Register (&r_portalrecursion, GLRENDEREROPTIONS);
	Cvar_Register (&r_portaldrawplanes, GLRENDEREROPTIONS);
	Cvar_Register (&r_portalonly, GLRENDEREROPTIONS);
	Cvar_Register (&r_noaliasshadows, GLRENDEREROPTIONS);

	Cvar_Register (&gl_motionblur, GLRENDEREROPTIONS);
	Cvar_Register (&gl_motionblurscale, GLRENDEREROPTIONS);

	Cvar_Register (&gl_smoothcrosshair, GRAPHICALNICETIES);

	Cvar_Register (&r_deluxmapping_cvar, GRAPHICALNICETIES);

#ifdef R_XFLIP
	Cvar_Register (&r_xflip, GLRENDEREROPTIONS);
#endif

//	Cvar_Register (&gl_lightmapmode, GLRENDEREROPTIONS);

	Cvar_Register (&gl_picmip, GLRENDEREROPTIONS);
	Cvar_Register (&gl_picmip2d, GLRENDEREROPTIONS);

	Cvar_Register (&r_shaderblobs, GLRENDEREROPTIONS);
	Cvar_Register (&gl_savecompressedtex, GLRENDEREROPTIONS);
	Cvar_Register (&gl_compress, GLRENDEREROPTIONS);
//	Cvar_Register (&gl_detail, GRAPHICALNICETIES);
//	Cvar_Register (&gl_detailscale, GRAPHICALNICETIES);
	Cvar_Register (&gl_overbright, GRAPHICALNICETIES);
	Cvar_Register (&gl_overbright_all, GRAPHICALNICETIES);
	Cvar_Register (&gl_dither, GRAPHICALNICETIES);
	Cvar_Register (&r_fog_exp2, GLRENDEREROPTIONS);

	Cvar_Register (&r_tessellation, GRAPHICALNICETIES);
	Cvar_Register (&gl_ati_truform_type, GRAPHICALNICETIES);
	Cvar_Register (&r_tessellation_level, GRAPHICALNICETIES);

	Cvar_Register (&gl_screenangle, GLRENDEREROPTIONS);

	Cvar_Register (&gl_skyboxdist, GLRENDEREROPTIONS);

	Cvar_Register (&r_wallcolour, GLRENDEREROPTIONS);
	Cvar_Register (&r_floorcolour, GLRENDEREROPTIONS);
//	Cvar_Register (&r_walltexture, GLRENDEREROPTIONS);
//	Cvar_Register (&r_floortexture, GLRENDEREROPTIONS);

	Cvar_Register (&r_vertexdlights, GLRENDEREROPTIONS);

//	Cvar_Register (&gl_schematics, GLRENDEREROPTIONS);

	Cvar_Register (&r_vertexlight, GLRENDEREROPTIONS);

	Cvar_Register (&gl_blend2d, GLRENDEREROPTIONS);

	Cvar_Register (&gl_menutint_shader, GLRENDEREROPTIONS);

	Cvar_Register (&gl_lightmap_nearest, GLRENDEREROPTIONS);
	Cvar_Register (&gl_lightmap_average, GLRENDEREROPTIONS);
}
#endif

void	R_InitTextures (void)
{
	int		x,y, m;
	qbyte	*dest;
	static FTE_ALIGN(4) char r_notexture_mip_mem[(sizeof(texture_t) + 16*16)];

// create a simple checkerboard texture for the default
	r_notexture_mip = (texture_t*)r_notexture_mip_mem;

	r_notexture_mip->width = r_notexture_mip->height = 16;

	for (m=0 ; m<1 ; m++)
	{
		dest = (qbyte *)(r_notexture_mip+1);
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

static int QDECL ShowFileList (const char *name, qofs_t flags, time_t mtime, void *parm, searchpathfuncs_t *spath)
{
	//ignore non-diffuse texture filenames, because they're annoying as heck.
	if (!strstr(name, "_pants.") && !strstr(name, "_shirt.") && !strstr(name, "_upper.") && !strstr(name, "_lower.") && !strstr(name, "_bump.") && !strstr(name, "_norm.") && !strstr(name, "_gloss.") && !strstr(name, "_luma."))
	{
		Con_Printf("%s\n", name);
	}
	return true;
}
void R_ListConfigs_f(void)
{
	COM_EnumerateFiles("*.cfg", ShowFileList, NULL);
	COM_EnumerateFiles("configs/*.cfg", ShowFileList, NULL);
}
void R_ListFonts_f(void)
{
	COM_EnumerateFiles("charsets/*.*", ShowFileList, NULL);
	COM_EnumerateFiles("textures/charsets/*.*", ShowFileList, NULL);
}
void R_ListSkins_f(void)
{
	COM_EnumerateFiles("skins/*.*", ShowFileList, NULL);
}
void R_ListSkyBoxes_f(void)
{
	//FIXME: this demonstrates why we need a nicer result printer.
	COM_EnumerateFiles("env/*rt.*", ShowFileList, NULL);
	COM_EnumerateFiles("env/*px.*", ShowFileList, NULL);
	COM_EnumerateFiles("env/*posx.*", ShowFileList, NULL);
	COM_EnumerateFiles("gfx/env/*rt.*", ShowFileList, NULL);
	COM_EnumerateFiles("gfx/env/*px.*", ShowFileList, NULL);
	COM_EnumerateFiles("gfx/env/*posx.*", ShowFileList, NULL);
	COM_EnumerateFiles("textures/env/*rt.*", ShowFileList, NULL);
	COM_EnumerateFiles("textures/env/*px.*", ShowFileList, NULL);
	COM_EnumerateFiles("textures/env/*posx.*", ShowFileList, NULL);
	COM_EnumerateFiles("textures/gfx/env/*rt.*", ShowFileList, NULL);
	COM_EnumerateFiles("textures/gfx/env/*px.*", ShowFileList, NULL);
	COM_EnumerateFiles("textures/gfx/env/*posx.*", ShowFileList, NULL);
}


void R_SetRenderer_f (void);
void R_ReloadRenderer_f (void);

void R_ToggleFullscreen_f(void)
{
	double time;
	rendererstate_t newr;

	if (currentrendererstate.renderer == NULL)
	{
		Con_Printf("vid_toggle: no renderer currently set\n");
		return;
	}
	//vid toggle makes no sense with these two...
	if (currentrendererstate.renderer->rtype == QR_HEADLESS || currentrendererstate.renderer->rtype == QR_NONE)
		return;

	Cvar_ApplyLatches(CVAR_RENDERERLATCH);

	newr = currentrendererstate;
	if (newr.fullscreen)
		newr.fullscreen = 0;	//if we're currently any sort of fullscreen then go windowed
	else if (vid_fullscreen.ival)
		newr.fullscreen = vid_fullscreen.ival;	//if we're normally meant to be fullscreen, use that
	else
		newr.fullscreen = 2;	//otherwise use native resolution
	if (newr.fullscreen)
	{
		int dbpp, dheight, dwidth, drate;
		if (!Sys_GetDesktopParameters(&dwidth, &dheight, &dbpp, &drate))
		{
			dwidth = DEFAULT_WIDTH;
			dheight = DEFAULT_HEIGHT;
			dbpp = DEFAULT_BPP;
			drate = 0;
		}

		if (newr.fullscreen == 1 && vid_width.ival>0)
			newr.width = vid_width.ival;
		else
			newr.width = dwidth;
		if (newr.fullscreen == 1 && vid_height.ival>0)
			newr.height = vid_height.ival;
		else
			newr.height = dheight;
	}
	else
	{
		newr.width = DEFAULT_WIDTH;
		newr.height = DEFAULT_HEIGHT;
	}

	time = Sys_DoubleTime();
	R_RestartRenderer(&newr);
	Con_DPrintf("main thread video restart took %f secs\n", Sys_DoubleTime() - time);
//	COM_WorkerFullSync();
//	Con_Printf("full video restart took %f secs\n", Sys_DoubleTime() - time);
}

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
	Cmd_AddCommand("vid_reload", R_ReloadRenderer_f);
	Cmd_AddCommand("vid_toggle", R_ToggleFullscreen_f);

#ifdef RTLIGHTS
	Cmd_AddCommand ("r_editlights_reload", R_ReloadRTLights_f);
	Cmd_AddCommand ("r_editlights_save", R_SaveRTLights_f);
	Cvar_Register (&r_editlights_import_radius, "Realtime Light editing/importing");
	Cvar_Register (&r_editlights_import_ambient, "Realtime Light editing/importing");
	Cvar_Register (&r_editlights_import_diffuse, "Realtime Light editing/importing");
	Cvar_Register (&r_editlights_import_specular, "Realtime Light editing/importing");

#endif
	Cmd_AddCommand("r_dumpshaders", Shader_WriteOutGenerics_f);
	Cmd_AddCommand("r_remapshader", Shader_RemapShader_f);
	Cmd_AddCommand("r_showshader", Shader_ShowShader_f);

#if defined(D3DQUAKE)
	GLD3DRenderer_Init();
#endif
#if defined(GLQUAKE)
	GLRenderer_Init();
#endif

#if defined(GLQUAKE) || defined(VKQUAKE)
	R_BloomRegister();
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
	Cvar_Register (&vid_vsync, VIDCOMMANDGROUP);
	Cvar_Register (&vid_wndalpha, VIDCOMMANDGROUP);
#if defined(_WIN32) && defined(MULTITHREAD)
	Cvar_Register (&vid_winthread, VIDCOMMANDGROUP);
#endif
	Cvar_Register (&_windowed_mouse, VIDCOMMANDGROUP);
	Cvar_Register (&vid_renderer, VIDCOMMANDGROUP);
	vid_renderer_opts.enginevalue = 
#ifdef GLQUAKE
		"gl \"OpenGL\" "
#endif
#ifdef VKQUAKE
		"vk \"Vulkan\" "
#endif
#ifdef D3D8QUAKE
//		"d3d8 \"Direct3D 8\" "
#endif
#ifdef D3D9QUAKE
		"d3d9 \"Direct3D 9\" "
#endif
#ifdef D3D11QUAKE
		"d3d11 \"Direct3D 11\" "
#endif
#ifdef SWQUAKE
		"sw \"Software Rendering\" "
#endif
		"";
	Cvar_Register (&vid_renderer_opts, VIDCOMMANDGROUP);

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
	Cvar_Register (&vid_srgb, GLRENDEREROPTIONS);
	Cvar_Register (&vid_dpi_x, GLRENDEREROPTIONS);
	Cvar_Register (&vid_dpi_y, GLRENDEREROPTIONS);

	Cvar_Register (&vid_desktopsettings, VIDCOMMANDGROUP);
	Cvar_Register (&vid_preservegamma, GLRENDEREROPTIONS);
	Cvar_Register (&vid_hardwaregamma, GLRENDEREROPTIONS);
	Cvar_Register (&vid_desktopgamma, GLRENDEREROPTIONS);


	Cvar_Register (&r_norefresh, GLRENDEREROPTIONS);
	Cvar_Register (&r_mirroralpha, GLRENDEREROPTIONS);
	Cvar_Register (&r_softwarebanding_cvar, GRAPHICALNICETIES);

	Cvar_Register (&r_skyboxname, GRAPHICALNICETIES);
	Cmd_AddCommand("sky", R_ForceSky_f);	//QS compat
	Cmd_AddCommand("loadsky", R_ForceSky_f);//DP compat

	Cvar_Register(&r_dodgytgafiles, "Hacky bug workarounds");
	Cvar_Register(&r_dodgypcxfiles, "Hacky bug workarounds");
	Cvar_Register(&r_dodgymiptex, "Hacky bug workarounds");
	r_imageexensions.enginevalue = r_defaultimageextensions;
	Cvar_Register(&r_imageexensions, GRAPHICALNICETIES);
	r_imageexensions.callback(&r_imageexensions, NULL);
	Cvar_Register(&r_image_downloadsizelimit, GRAPHICALNICETIES);
	Cvar_Register(&r_loadlits, GRAPHICALNICETIES);
	Cvar_Register(&r_lightstylesmooth, GRAPHICALNICETIES);
	Cvar_Register(&r_lightstylesmooth_limit, GRAPHICALNICETIES);
	Cvar_Register(&r_lightstylespeed, GRAPHICALNICETIES);
	Cvar_Register(&r_lightstylescale, GRAPHICALNICETIES);
	Cvar_Register(&r_lightmap_scale, GRAPHICALNICETIES);

	Cvar_Register(&r_hdr_irisadaptation, GRAPHICALNICETIES);
	Cvar_Register(&r_hdr_irisadaptation_multiplier, GRAPHICALNICETIES);
	Cvar_Register(&r_hdr_irisadaptation_minvalue, GRAPHICALNICETIES);
	Cvar_Register(&r_hdr_irisadaptation_maxvalue, GRAPHICALNICETIES);
	Cvar_Register(&r_hdr_irisadaptation_fade_down, GRAPHICALNICETIES);
	Cvar_Register(&r_hdr_irisadaptation_fade_up, GRAPHICALNICETIES);

	Cvar_Register(&r_stains, GRAPHICALNICETIES);
	Cvar_Register(&r_stainfadetime, GRAPHICALNICETIES);
	Cvar_Register(&r_stainfadeammount, GRAPHICALNICETIES);
	Cvar_Register(&r_lightprepass_cvar, GLRENDEREROPTIONS);
	Cvar_Register (&r_coronas, GRAPHICALNICETIES);
	Cvar_Register (&r_coronas_occlusion, GRAPHICALNICETIES);
	Cvar_Register (&r_coronas_mindist, GRAPHICALNICETIES);
	Cvar_Register (&r_coronas_fadedist, GRAPHICALNICETIES);
	Cvar_Register (&r_flashblend, GRAPHICALNICETIES);
	Cvar_Register (&r_flashblendscale, GRAPHICALNICETIES);
	Cvar_Register (&gl_specular, GRAPHICALNICETIES);
	Cvar_Register (&gl_specular_fallback, GRAPHICALNICETIES);
	Cvar_Register (&gl_specular_fallbackexp, GRAPHICALNICETIES);

	Sh_RegisterCvars();

	Cvar_Register (&r_fastturbcolour, GRAPHICALNICETIES);
	Cvar_Register (&r_waterstyle, GRAPHICALNICETIES);
	Cvar_Register (&r_lavastyle, GRAPHICALNICETIES);
	Cvar_Register (&r_slimestyle, GRAPHICALNICETIES);
	Cvar_Register (&r_telestyle, GRAPHICALNICETIES);
	Cvar_Register (&r_wireframe, GRAPHICALNICETIES);
	Cvar_Register (&r_wireframe_smooth, GRAPHICALNICETIES);
	Cvar_Register (&r_refract_fbo, GRAPHICALNICETIES);
	Cvar_Register (&r_refractreflect_scale, GRAPHICALNICETIES);
	Cvar_Register (&r_postprocshader, GRAPHICALNICETIES);
	Cvar_Register (&r_fxaa, GRAPHICALNICETIES);
	Cvar_Register (&r_renderscale, GRAPHICALNICETIES);
	Cvar_Register (&r_stereo_separation, GRAPHICALNICETIES);
	Cvar_Register (&r_stereo_convergence, GRAPHICALNICETIES);
	Cvar_Register (&r_stereo_method, GRAPHICALNICETIES);

	Cvar_Register (&r_shadow_bumpscale_basetexture, GRAPHICALNICETIES);
	Cvar_Register (&r_shadow_bumpscale_bumpmap, GRAPHICALNICETIES);
	Cvar_Register (&r_shadow_heightscale_basetexture, GRAPHICALNICETIES);
	Cvar_Register (&r_shadow_heightscale_bumpmap, GRAPHICALNICETIES);

	Cvar_Register (&r_glsl_offsetmapping, GRAPHICALNICETIES);
	Cvar_Register (&r_glsl_offsetmapping_scale, GRAPHICALNICETIES);
	Cvar_Register (&r_glsl_offsetmapping_reliefmapping, GRAPHICALNICETIES);
	Cvar_Register (&r_glsl_turbscale_reflect, GRAPHICALNICETIES);
	Cvar_Register (&r_glsl_turbscale_refract, GRAPHICALNICETIES);

	Cvar_Register(&scr_viewsize, SCREENOPTIONS);
	Cvar_Register(&scr_fov, SCREENOPTIONS);
//	Cvar_Register(&scr_chatmodecvar, SCREENOPTIONS);

	Cvar_Register (&scr_sshot_type, SCREENOPTIONS);
	Cvar_Register (&scr_sshot_compression, SCREENOPTIONS);
	Cvar_Register (&scr_sshot_prefix, SCREENOPTIONS);

	Cvar_Register(&cl_cursor,	SCREENOPTIONS);
	Cvar_Register(&cl_cursorscale,	SCREENOPTIONS);
	Cvar_Register(&cl_cursorbiasx,	SCREENOPTIONS);
	Cvar_Register(&cl_cursorbiasy,	SCREENOPTIONS);


//screen
	Cvar_Register (&gl_font, GRAPHICALNICETIES);
	Cvar_Register (&scr_conspeed, SCREENOPTIONS);
	Cvar_Register (&scr_conalpha, SCREENOPTIONS);
	Cvar_Register (&scr_showturtle, SCREENOPTIONS);
	Cvar_Register (&scr_turtlefps, SCREENOPTIONS);
	Cvar_Register (&scr_showpause, SCREENOPTIONS);
	Cvar_Register (&scr_centertime, SCREENOPTIONS);
	Cvar_Register (&scr_logcenterprint, SCREENOPTIONS);
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
	Cvar_Register (&r_lavaalpha, GRAPHICALNICETIES);
	Cvar_Register (&r_slimealpha, GRAPHICALNICETIES);
	Cvar_Register (&r_telealpha, GRAPHICALNICETIES);
	Cvar_Register (&gl_shadeq1_name, GLRENDEREROPTIONS);

	Cvar_Register (&gl_mindist, GLRENDEREROPTIONS);
	Cvar_Register (&gl_load24bit, GRAPHICALNICETIES);
	Cvar_Register (&gl_blendsprites, GLRENDEREROPTIONS);

	Cvar_Register (&r_clear, GLRENDEREROPTIONS);
	Cvar_Register (&gl_max_size, GLRENDEREROPTIONS);
	Cvar_Register (&gl_maxdist, GLRENDEREROPTIONS);
	Cvar_Register (&gl_miptexLevel, GRAPHICALNICETIES);
	Cvar_Register (&gl_texturemode, GLRENDEREROPTIONS);
	Cvar_Register (&gl_texturemode2d, GLRENDEREROPTIONS);
	Cvar_Register (&r_font_linear, GLRENDEREROPTIONS);
	Cvar_Register (&gl_mipcap, GLRENDEREROPTIONS);
	Cvar_Register (&gl_texture_anisotropic_filtering, GLRENDEREROPTIONS);
	Cvar_Register (&r_max_gpu_bones, GRAPHICALNICETIES);
	Cvar_Register (&r_drawflat, GRAPHICALNICETIES);
	Cvar_Register (&r_lightmap, GRAPHICALNICETIES);
	Cvar_Register (&r_menutint, GRAPHICALNICETIES);

	Cvar_Register (&r_fb_bmodels, GRAPHICALNICETIES);
	Cvar_Register (&r_fb_models, GRAPHICALNICETIES);
	Cvar_Register (&r_skin_overlays, GRAPHICALNICETIES);
	Cvar_Register (&r_globalskin_first, GRAPHICALNICETIES);
	Cvar_Register (&r_globalskin_count, GRAPHICALNICETIES);
	Cvar_Register (&r_shadows, GRAPHICALNICETIES);

	Cvar_Register (&r_replacemodels, GRAPHICALNICETIES);

	Cvar_Register (&r_showbboxes, GLRENDEREROPTIONS);
	Cvar_Register (&r_showfields, GLRENDEREROPTIONS);
	Cvar_Register (&r_showshaders, GLRENDEREROPTIONS);
#ifndef NOLEGACY
	Cvar_Register (&r_polygonoffset_submodel_factor, GLRENDEREROPTIONS);
	Cvar_Register (&r_polygonoffset_submodel_offset, GLRENDEREROPTIONS);
#endif
	Cvar_Register (&r_polygonoffset_shadowmap_factor, GLRENDEREROPTIONS);
	Cvar_Register (&r_polygonoffset_shadowmap_offset, GLRENDEREROPTIONS);
	Cvar_Register (&r_polygonoffset_stencil_factor, GLRENDEREROPTIONS);
	Cvar_Register (&r_polygonoffset_stencil_offset, GLRENDEREROPTIONS);

	Cvar_Register (&r_forceprogramify, GLRENDEREROPTIONS);
	Cvar_Register (&dpcompat_nopremulpics, GLRENDEREROPTIONS);
#ifdef VKQUAKE
	Cvar_Register (&vk_stagingbuffers,			VKRENDEREROPTIONS);
	Cvar_Register (&vk_submissionthread,		VKRENDEREROPTIONS);
	Cvar_Register (&vk_debug,					VKRENDEREROPTIONS);
	Cvar_Register (&vk_dualqueue,				VKRENDEREROPTIONS);
	Cvar_Register (&vk_busywait,				VKRENDEREROPTIONS);

	Cvar_Register (&vk_nv_glsl_shader,			VKRENDEREROPTIONS);
	Cvar_Register (&vk_nv_dedicated_allocation,	VKRENDEREROPTIONS);
//	Cvar_Register (&vk_khr_dedicated_allocation,VKRENDEREROPTIONS);
	Cvar_Register (&vk_khr_push_descriptor,		VKRENDEREROPTIONS);
#endif

// misc
	Cvar_Register(&con_ocranaleds, "Console controls");

	Cmd_AddCommand ("listfonts", R_ListFonts_f);
	Cmd_AddCommand ("listskins", R_ListSkins_f);
	Cmd_AddCommand ("listskyboxes", R_ListSkyBoxes_f);
	Cmd_AddCommand ("listconfigs", R_ListConfigs_f);

	P_InitParticleSystem();
	R_InitTextures();
}

qboolean Renderer_Started(void)
{
	return !r_blockvidrestart && !!currentrendererstate.renderer;
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

void	(*R_Init)					(void);
void	(*R_DeInit)					(void);
void	(*R_RenderView)				(void);		// must set r_refdef first

qboolean (*VID_Init)				(rendererstate_t *info, unsigned char *palette);
void	 (*VID_DeInit)				(void);
char	*(*VID_GetRGBInfo)			(int *stride, int *truevidwidth, int *truevidheight, enum uploadfmt *fmt);
void	(*VID_SetWindowCaption)		(const char *msg);

qboolean (*SCR_UpdateScreen)			(void);

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

	NULL,	//IMG_UpdateFiltering
	NULL,	//IMG_LoadTextureMips
	NULL,	//IMG_DestroyTexture

	NULL,	//R_Init;
	NULL,	//R_DeInit;
	NULL,	//R_RenderView;

	NULL, //VID_Init,
	NULL, //VID_DeInit,
	NULL, //VID_SwapBuffers
	NULL, //VID_ApplyGammaRamps,
	NULL,
	NULL,
	NULL,
	NULL,	//set caption
	NULL, //VID_GetRGBInfo,

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
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,

	NULL,

	""
};

#ifdef GLQUAKE
extern rendererinfo_t openglrendererinfo;
#ifdef USE_EGL
extern rendererinfo_t eglrendererinfo;
#endif
extern rendererinfo_t rpirendererinfo;
rendererinfo_t waylandrendererinfo;
rendererinfo_t fbdevrendererinfo;
#endif
#ifdef D3D8QUAKE
extern rendererinfo_t d3d8rendererinfo;
#endif
#ifdef D3D9QUAKE
extern rendererinfo_t d3d9rendererinfo;
#endif
#ifdef D3D11QUAKE
extern rendererinfo_t d3d11rendererinfo;
#endif
#ifdef SWQUAKE
extern rendererinfo_t swrendererinfo;
#endif
#ifdef VKQUAKE
extern rendererinfo_t vkrendererinfo;
//rendererinfo_t headlessvkrendererinfo;
extern rendererinfo_t nvvkrendererinfo;
#endif
#ifdef HEADLESSQUAKE
extern rendererinfo_t headlessrenderer;
#endif

rendererinfo_t *rendererinfo[] =
{
#ifdef GLQUAKE
#ifdef FTE_RPI
	&rpirendererinfo,
#endif
	&openglrendererinfo,
#ifdef USE_EGL
	&eglrendererinfo,
#endif
	&waylandrendererinfo,
	&fbdevrendererinfo,
#endif
#ifdef D3D9QUAKE
	&d3d9rendererinfo,
#endif
#ifdef D3D11QUAKE
	&d3d11rendererinfo,
#endif
#ifdef SWQUAKE
	&swrendererinfo,
#endif
#ifdef VKQUAKE
	&vkrendererinfo,
	#if defined(_WIN32) && defined(GLQUAKE)
		&nvvkrendererinfo,
	#endif
#endif
#ifdef D3D8QUAKE
	&d3d8rendererinfo,
#endif
#ifndef NPQTV
	&dedicatedrendererinfo,
#endif
#ifdef HEADLESSQUAKE
	&headlessrenderer,
#ifdef VKQUAKE
	//&headlessvkrendererinfo,
#endif
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

	VID_Init				= ri->VID_Init;
	VID_DeInit				= ri->VID_DeInit;
	VID_GetRGBInfo			= ri->VID_GetRGBInfo;
	VID_SetWindowCaption	= ri->VID_SetWindowCaption;

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

qboolean R_ApplyRenderer_Load (rendererstate_t *newr);
void D3DSucks(void)
{
	SCR_DeInit();

	if (!R_ApplyRenderer_Load(NULL))//&currentrendererstate))
		Sys_Error("Failed to reload content after mode switch\n");
}

void R_ShutdownRenderer(qboolean devicetoo)
{
	//make sure the worker isn't still loading stuff
	COM_WorkerFullSync();

	CL_AllowIndependantSendCmd(false);	//FIXME: figure out exactly which parts are going to affect the model loading.

	Skin_FlushAll();

	P_Shutdown();
	Mod_Shutdown(false);

	IN_Shutdown();

	if (R_DeInit)
	{
		TRACE(("dbg: R_ApplyRenderer: R_DeInit\n"));
		R_DeInit();
	}

	if (Draw_Shutdown)
		Draw_Shutdown();

	if (VID_DeInit && devicetoo)
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

	if (devicetoo)
		S_Shutdown(false);
	else
		S_StopAllSounds (true);
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
		d_8to24bgrtable[i] = (255<<24) + (b<<0) + (g<<8) + (r<<16);
	}
	d_8to24rgbtable[255] &= 0xffffff;	// 255 is transparent
	d_8to24bgrtable[255] &= 0xffffff;	// 255 is transparent
}

qboolean R_ApplyRenderer (rendererstate_t *newr)
{
	double time;
	if (newr->bpp == -1)
		return false;
	if (!newr->renderer)
		return false;

	COM_WorkerFullSync();

	time = Sys_DoubleTime();

#ifndef NOBUILTINMENUS
	M_RemoveAllMenus(true);
#endif
	Media_CaptureDemoEnd();
	R_ShutdownRenderer(true);
	Con_DPrintf("video shutdown took %f seconds\n", Sys_DoubleTime() - time);

	if (qrenderer == QR_NONE)
	{
		if (newr->renderer->rtype == qrenderer && currentrendererstate.renderer)
		{
			R_SetRenderer(newr->renderer);
			return true;	//no point
		}

		Sys_CloseTerminal ();
	}

	time = Sys_DoubleTime();
	R_SetRenderer(newr->renderer);
	Con_DPrintf("video startup took %f seconds\n", Sys_DoubleTime() - time);

	return R_ApplyRenderer_Load(newr);
}
qboolean R_ApplyRenderer_Load (rendererstate_t *newr)
{
	int i, j;
	double start = Sys_DoubleTime();

	COM_WorkerFullSync();

	Cache_Flush();
	COM_FlushFSCache(false, true);	//make sure the fs cache is built if needed. there's lots of loading here.

	TRACE(("dbg: R_ApplyRenderer: old renderer closed\n"));

	pmove.numphysent = 0;
	pmove.physents[0].model = NULL;

	vid.dpi_x = 0;
	vid.dpi_y = 0;

#ifndef CLIENTONLY
	sv.world.lastcheckpvs = NULL;
#endif

	if (qrenderer != QR_NONE)	//graphics stuff only when not dedicated
	{
		size_t sz;
		qbyte *data;
#ifndef CLIENTONLY
		isDedicated = false;
#endif
		if (newr)
			Con_TPrintf("Setting mode %i*%i*%i*%i %s\n", newr->width, newr->height, newr->bpp, newr->rate, newr->renderer->description);

		vid.fullbright=0;

		if (host_basepal)
			BZ_Free(host_basepal);
		host_basepal = (qbyte *)FS_LoadMallocFile ("gfx/palette.lmp", &sz);
		if (!host_basepal)
			host_basepal = (qbyte *)FS_LoadMallocFile ("wad/playpal", &sz);
		if (!host_basepal || sz != 768)
		{
			qbyte *pcx=NULL;
			if (host_basepal)
				Z_Free(host_basepal);
			host_basepal = BZ_Malloc(768);
			pcx = COM_LoadTempFile("pics/colormap.pcx", &sz);
			if (!pcx || !ReadPCXPalette(pcx, sz, host_basepal))
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
			qbyte *colormap = (qbyte *)FS_LoadMallocFile ("gfx/colormap.lmp", NULL);
			if (colormap)
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

#ifdef HEXEN2
		if (h2playertranslations)
			BZ_Free(h2playertranslations);
		h2playertranslations = FS_LoadMallocFile ("gfx/player.lmp", NULL);
#endif

		if (vid.fullbright < 2)
			vid.fullbright = 0;	//transparent colour doesn't count.

q2colormap:
		R_GenPaletteLookup();

TRACE(("dbg: R_ApplyRenderer: Palette loaded\n"));

		if (newr)
		{
			vid.gammarampsize = 256;	//make a guess.
			if (!VID_Init(newr, host_basepal))
			{
				return false;
			}
		}
TRACE(("dbg: R_ApplyRenderer: vid applied\n"));

		r_softwarebanding = false;
		r_deluxmapping = false;
		r_lightprepass = false;

		W_LoadWadFile("gfx.wad");
TRACE(("dbg: R_ApplyRenderer: wad loaded\n"));
		Image_Init();
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

		Cvar_ForceCallback(&v_gamma);
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
	Mod_Init(false);

//	host_hunklevel = Hunk_LowMark();

	Cvar_ForceSetValue(&vid_dpi_x, vid.dpi_x);
	Cvar_ForceSetValue(&vid_dpi_y, vid.dpi_y);


	TRACE(("dbg: R_ApplyRenderer: R_PreNewMap (how handy)\n"));
	Surf_PreNewMap();

#ifndef CLIENTONLY
	if (sv.world.worldmodel)
	{
TRACE(("dbg: R_ApplyRenderer: reloading server map\n"));
		sv.world.worldmodel = Mod_ForName (sv.modelname, MLV_WARNSYNC);
TRACE(("dbg: R_ApplyRenderer: loaded\n"));
		if (sv.world.worldmodel->loadstate == MLS_LOADING)
			COM_WorkerPartialSync(sv.world.worldmodel, &sv.world.worldmodel->loadstate, MLS_LOADING);
TRACE(("dbg: R_ApplyRenderer: doing that funky phs thang\n"));
		SV_CalcPHS ();

TRACE(("dbg: R_ApplyRenderer: clearing world\n"));

		if (sv.world.worldmodel->loadstate != MLS_LOADED)
			SV_UnspawnServer();
		else if (svs.gametype == GT_PROGS)
		{
			for (i = 0; i < MAX_PRECACHE_MODELS; i++)
			{
				if (sv.strings.model_precache[i] && *sv.strings.model_precache[i] && (!strcmp(sv.strings.model_precache[i] + strlen(sv.strings.model_precache[i]) - 4, ".bsp") || i-1 < sv.world.worldmodel->numsubmodels))
					sv.models[i] = Mod_FindName(Mod_FixName(sv.strings.model_precache[i], sv.strings.model_precache[1]));
				else
					sv.models[i] = NULL;
			}

			World_ClearWorld (&sv.world, true);
//			ent = sv.world.edicts;
//			ent->v->model = PR_NewString(svprogfuncs, sv.worldmodel->name);	//FIXME: is this a problem for normal ents?
		}
#ifdef Q2SERVER
		else if (svs.gametype == GT_QUAKE2)
		{
			q2edict_t *q2ent;
			for (i = 0; i < Q2MAX_MODELS; i++)
			{
				if (sv.strings.configstring[Q2CS_MODELS+i] && *sv.strings.configstring[Q2CS_MODELS+i] && (!strcmp(sv.strings.configstring[Q2CS_MODELS+i] + strlen(sv.strings.configstring[Q2CS_MODELS+i]) - 4, ".bsp") || i-1 < sv.world.worldmodel->numsubmodels))
					sv.models[i] = Mod_FindName(Mod_FixName(sv.strings.configstring[Q2CS_MODELS+i], sv.modelname));
				else
					sv.models[i] = NULL;
			}
			for (; i < MAX_PRECACHE_MODELS; i++)
			{
				if (sv.strings.q2_extramodels[i] && *sv.strings.q2_extramodels[i] && (!strcmp(sv.strings.q2_extramodels[i] + strlen(sv.strings.q2_extramodels[i]) - 4, ".bsp") || i-1 < sv.world.worldmodel->numsubmodels))
					sv.models[i] = Mod_FindName(Mod_FixName(sv.strings.q2_extramodels[i], sv.modelname));
				else
					sv.models[i] = NULL;
			}

			World_ClearWorld (&sv.world, false);
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
#ifdef Q3SERVER
		else if (svs.gametype == GT_QUAKE3)
		{
			memset(&sv.models, 0, sizeof(sv.models));
			sv.models[1] = Mod_FindName(sv.modelname);
			//traditionally a q3 server can just keep hold of its world cmodel and nothing is harmed.
			//this means we just need to reload the worldmodel and all is fine...
			//there are some edge cases however, like lingering pointers refering to entities.
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

	if (newr)
		memcpy(&currentrendererstate, newr, sizeof(currentrendererstate));

	TRACE(("dbg: R_ApplyRenderer: S_Restart_f\n"));
	if (!isDedicated)
		S_DoRestart(true);

#ifdef Q3SERVER
	if (svs.gametype == GT_QUAKE3)
	{
		cl.worldmodel = NULL;
		CG_Stop();
		CG_Start();
		if (cl.worldmodel)
			Surf_NewMap();
	}
	else
#endif
	if (cl.worldmodel)
	{
		cl.worldmodel = NULL;
		CL_ClearEntityLists();	//shouldn't really be needed, but we're paranoid

		//FIXME: this code should not be here. call CL_LoadModels instead? that does csqc loading etc though. :s
TRACE(("dbg: R_ApplyRenderer: reloading ALL models\n"));
		for (i=1 ; i<MAX_PRECACHE_MODELS ; i++)
		{
			if (!cl.model_name[i][0])
				break;

			TRACE(("dbg: R_ApplyRenderer: reloading model %s\n", cl.model_name[i]));

#ifdef Q2CLIENT	//skip vweps
			if (cls.protocol == CP_QUAKE2 && *cl.model_name[i] == '#')
				cl.model_precache[i] = NULL;
			else
#endif
				if (i == 1)
					cl.model_precache[i] = Mod_ForName (cl.model_name[i], MLV_SILENT);
				else
					cl.model_precache[i] = Mod_FindName (Mod_FixName(cl.model_name[i], cl.model_name[1]));
		}

		for (i=0; i < MAX_VWEP_MODELS; i++)
		{
			if (*cl.model_name_vwep[i])
				cl.model_precache_vwep[i] = Mod_ForName (cl.model_name_vwep[i], MLV_SILENT);
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
			cl.model_csqcprecache[i] = Mod_ForName (Mod_FixName(cl.model_csqcname[i], cl.model_name[1]), MLV_SILENT);
		}
#endif

		//fixme: worldmodel could be ssqc or csqc.
		cl.worldmodel = cl.model_precache[1];

		if (cl.worldmodel && cl.worldmodel->loadstate == MLS_LOADING)
			COM_WorkerPartialSync(cl.worldmodel, &cl.worldmodel->loadstate, MLS_LOADING);

TRACE(("dbg: R_ApplyRenderer: done the models\n"));
		if (!cl.worldmodel || cl.worldmodel->loadstate != MLS_LOADED)
		{
//				Con_Printf ("\nThe required model file '%s' could not be found.\n\n", cl.model_name[i]);
//				Con_Printf ("You may need to download or purchase a client pack in order to play on this server.\n\n");

				CL_Disconnect ();
#ifdef VM_UI
				UI_Reset();
#endif
				if (newr)
					memcpy(&currentrendererstate, newr, sizeof(currentrendererstate));
				return true;
		}

TRACE(("dbg: R_ApplyRenderer: checking any wad textures\n"));
		Mod_NowLoadExternal(cl.worldmodel);

		for (i = 0; i < cl.num_statics; i++)	//make the static entities reappear.
			cl_static_entities[i].ent.model = NULL;

TRACE(("dbg: R_ApplyRenderer: Surf_NewMap\n"));
		Surf_NewMap();
TRACE(("dbg: R_ApplyRenderer: efrags\n"));

//		Skin_FlushAll();
		Skin_FlushPlayers();

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

	if (newr && qrenderer != QR_NONE)
	{
		Con_TPrintf("%s renderer initialized\n", newr->renderer->description);
	}

	TRACE(("dbg: R_ApplyRenderer: done\n"));


	Con_DPrintf("video restart took %f seconds\n", Sys_DoubleTime() - start);
	return true;
}

void R_ReloadRenderer_f (void)
{
	float time = Sys_DoubleTime();
	if (qrenderer == QR_NONE || qrenderer == QR_HEADLESS)
		return;	//don't bother reloading the renderer if its not actually rendering anything anyway.
	R_ShutdownRenderer(false);
	Con_DPrintf("teardown = %f\n", Sys_DoubleTime() - time);
	//reloads textures without destroying video context.
	R_ApplyRenderer_Load(NULL);
}

//use Cvar_ApplyLatches(CVAR_RENDERERLATCH) beforehand.
qboolean R_BuildRenderstate(rendererstate_t *newr, char *rendererstring)
{
	int i, j;

	memset(newr, 0, sizeof(*newr));

	newr->width = vid_width.value;
	newr->height = vid_height.value;

	newr->triplebuffer = vid_triplebuffer.value;
	newr->multisample = vid_multisample.value;
	newr->bpp = vid_bpp.value;
	newr->fullscreen = vid_fullscreen.value;
	newr->rate = vid_refreshrate.value;
	newr->stereo = (r_stereo_method.ival == 1);
	newr->srgb = vid_srgb.ival;

	if (com_installer)
	{
		newr->fullscreen = false;
		newr->width = 640;
		newr->height = 480;
	}

	if (!*vid_vsync.string || vid_vsync.value < 0)
		newr->wait = -1;
	else
		newr->wait = vid_vsync.value;

	newr->renderer = NULL;

	rendererstring = COM_Parse(rendererstring);
	if (!*com_token)
	{
		for (i = 0; i < sizeof(rendererinfo)/sizeof(rendererinfo[0]); i++)
		{
			if (rendererinfo[i]->name[0] && stricmp(rendererinfo[i]->name[0], "none"))
			{
				newr->renderer = rendererinfo[i];
				break;
			}
		}
	}
	else if (!strcmp(com_token, "random"))
	{
		int count;
		for (i = 0, count = 0; i < sizeof(rendererinfo)/sizeof(rendererinfo[0]); i++)
		{
			if (!rendererinfo[i]->description)
				continue;	//not valid in this build. :(
			if (rendererinfo[i]->rtype == QR_NONE		||	//dedicated servers are not useful
				rendererinfo[i]->rtype == QR_HEADLESS	||	//headless appears buggy
				rendererinfo[i]->rtype == QR_SOFTWARE	)	//software is just TOO buggy/limited for us to care.
				continue;
			count++;
		}
		count = rand()%count;
		for (i = 0; i < sizeof(rendererinfo)/sizeof(rendererinfo[0]); i++)
		{
			if (!rendererinfo[i]->description)
				continue;	//not valid in this build. :(
			if (rendererinfo[i]->rtype == QR_NONE		||
				rendererinfo[i]->rtype == QR_HEADLESS	||
				rendererinfo[i]->rtype == QR_SOFTWARE	)
				continue;
			if (!count--)
			{
				newr->renderer = rendererinfo[i];
				Con_Printf("randomly selected renderer: %s\n", rendererinfo[i]->description);
				break;
			}
		}
	}
	else
	{
		for (i = 0; i < sizeof(rendererinfo)/sizeof(rendererinfo[0]); i++)
		{
			if (!rendererinfo[i]->description)
				continue;	//not valid in this build. :(
			for (j = 4-1; j >= 0; j--)
			{
				if (!rendererinfo[i]->name[j])
					continue;
				if (!stricmp(rendererinfo[i]->name[j], com_token))
				{
					newr->renderer = rendererinfo[i];
					break;
				}
			}
		}
	}

	rendererstring = COM_Parse(rendererstring);
	if (*com_token)
		Q_strncpyz(newr->subrenderer, com_token, sizeof(newr->subrenderer));
	else if (newr->renderer && newr->renderer->rtype == QR_OPENGL)
		Q_strncpyz(newr->subrenderer, gl_driver.string, sizeof(newr->subrenderer));

	// use desktop settings if set to 0 and not dedicated
	if (newr->renderer && newr->renderer->rtype != QR_NONE)
	{
		extern int isPlugin;

		if (vid_desktopsettings.value)
		{
			newr->width = 0;
			newr->height = 0;
			newr->bpp = 0;
			newr->rate = 0;
		}

		if (newr->width <= 0 || newr->height <= 0 || newr->bpp <= 0)
		{
			int dbpp, dheight, dwidth, drate;
			
			if (!newr->fullscreen || isPlugin || !Sys_GetDesktopParameters(&dwidth, &dheight, &dbpp, &drate))
			{
				dwidth = DEFAULT_WIDTH;
				dheight = DEFAULT_HEIGHT;
				dbpp = DEFAULT_BPP;
				drate = 0;
			}

			if (newr->width <= 0)
				newr->width = dwidth;
			if (newr->height <= 0)
				newr->height = dheight;
			if (newr->bpp <= 0)
				newr->bpp = dbpp;
		}
	}

#ifdef CLIENTONLY
	if (newr->renderer && newr->renderer->rtype == QR_NONE)
	{
		Con_Printf("Client-only builds cannot use dedicated modes.\n");
		return false;
	}
#endif

	return newr->renderer != NULL;
}

void R_RestartRenderer (rendererstate_t *newr)
{
	rendererstate_t oldr;
	if (r_blockvidrestart)
	{
		Con_Printf("Ignoring vid_restart from config\n");
		return;
	}

	TRACE(("dbg: R_RestartRenderer_f renderer %i\n", newr.renderer));

	memcpy(&oldr, &currentrendererstate, sizeof(rendererstate_t));
	if (!R_ApplyRenderer(newr))
	{
		TRACE(("dbg: R_RestartRenderer_f failed\n"));
		if (R_ApplyRenderer(&oldr))
		{
			TRACE(("dbg: R_RestartRenderer_f old restored\n"));
			Con_Printf(CON_ERROR "Video mode switch failed. Old mode restored.\n");	//go back to the old mode, the new one failed.
		}
		else
		{
			int i;
			qboolean failed = true;
			rendererinfo_t *skip = newr->renderer;

			if (newr->rate != 0)
			{
				Con_Printf(CON_NOTICE "Trying default refresh rate\n");
				newr->rate = 0;
				failed = !R_ApplyRenderer(newr);
			}

			if (failed && newr->width != DEFAULT_WIDTH && newr->height != DEFAULT_HEIGHT)
			{
				Con_Printf(CON_NOTICE "Trying %i*%i\n", DEFAULT_WIDTH, DEFAULT_HEIGHT);
				newr->width = DEFAULT_WIDTH;
				newr->height = DEFAULT_HEIGHT;
				failed = !R_ApplyRenderer(newr);
			}

			for (i = 0; failed && i < sizeof(rendererinfo)/sizeof(rendererinfo[0]); i++)
			{
				newr->renderer = rendererinfo[i];
				if (newr->renderer && newr->renderer != skip && newr->renderer->rtype != QR_HEADLESS)
				{
					Con_Printf(CON_NOTICE "Trying %s\n", newr->renderer->description);
					failed = !R_ApplyRenderer(newr);
				}
			}

			//if we ended up resorting to our last choice (dedicated) then print some informative message about it
			//fixme: on unixy systems, we should make sure we're actually printing to something (ie: that we're not running via some x11 shortcut with our stdout redirected to /dev/nul
			if (!failed && newr->renderer == &dedicatedrendererinfo)
			{
				Con_Printf(CON_ERROR "Video mode switch failed. Console forced.\n\nPlease change the following vars to something useable, and then use the setrenderer command.\n");
				Con_Printf("%s: %s\n", vid_width.name, vid_width.string);
				Con_Printf("%s: %s\n", vid_height.name, vid_height.string);
				Con_Printf("%s: %s\n", vid_bpp.name, vid_bpp.string);
				Con_Printf("%s: %s\n", vid_refreshrate.name, vid_refreshrate.string);
				Con_Printf("%s: %s\n", vid_renderer.name, vid_renderer.string);
				Con_Printf("%s: %s\n", gl_driver.name, gl_driver.string);
			}

			if (failed)
				Sys_Error("Unable to initialise any video mode\n");
		}
	}

	Cvar_ApplyCallbacks(CVAR_RENDERERCALLBACK);
	SCR_EndLoadingPlaque();

	TRACE(("dbg: R_RestartRenderer_f success\n"));
//	M_Reinit();
}

void R_RestartRenderer_f (void)
{
	double time;
	rendererstate_t newr;

	Cvar_ApplyLatches(CVAR_RENDERERLATCH);
	if (!R_BuildRenderstate(&newr, vid_renderer.string))
	{
		Con_Printf("vid_renderer \"%s\" unsupported. Using default.\n", vid_renderer.string);

		//gotta do this after main hunk is saved off.
		Cmd_ExecuteString("setrenderer \"\"\n", RESTRICT_LOCAL);
		return;
	}

	time = Sys_DoubleTime();
	R_RestartRenderer(&newr);
	Con_DPrintf("main thread video restart took %f secs\n", Sys_DoubleTime() - time);
//	COM_WorkerFullSync();
//	Con_Printf("full video restart took %f secs\n", Sys_DoubleTime() - time);
}

void R_SetRenderer_f (void)
{
	int i;
	char *param = Cmd_Argv(1);
	rendererstate_t newr;

	if (Cmd_Argc() == 1 || !stricmp(param, "help"))
	{
		Con_Printf ("\nValid setrenderer parameters are:\n");
		for (i = 0; i < sizeof(rendererinfo)/sizeof(rendererinfo[0]); i++)
		{
			if (rendererinfo[i]->description)
				Con_Printf("^[%s\\type\\/setrenderer %s^]^7: %s%s\n", rendererinfo[i]->name[0], rendererinfo[i]->name[0], rendererinfo[i]->description, (currentrendererstate.renderer == rendererinfo[i])?" ^2(current)":"");
		}
		return;
	}

	Cvar_ApplyLatches(CVAR_RENDERERLATCH);
	if (!R_BuildRenderstate(&newr, param))
	{
		Con_Printf("setrenderer: parameter not supported (%s)\n", param);
		return;
	}
	else
	{
		if (Cmd_Argc() == 3)
			Cvar_Set(&vid_bpp, Cmd_Argv(2));
	}

	if (newr.renderer->rtype != QR_HEADLESS && !strstr(param, "headless"))	//don't save headless in the vid_renderer cvar via the setrenderer command. 'setrenderer headless;vid_restart' can then do what is most sane.
		Cvar_Set(&vid_renderer, param);

	if (!r_blockvidrestart)
		R_RestartRenderer(&newr);
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

	psprite = currententity->model->meshinfo;
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
		float f = DotProduct(vpn,currententity->axis[0]);
		float r = DotProduct(vright,currententity->axis[0]);
		int dir = (atan2(r, f)+1.125*M_PI)*(4/M_PI);

		pspritegroup = (mspritegroup_t *)psprite->frames[frame].frameptr;
//		pspriteframe = pspritegroup->frames[(int)((r_refdef.viewangles[1]-currententity->angles[1])/360*pspritegroup->numframes + 0.5-4)%pspritegroup->numframes];
		//int dir = (int)((r_refdef.viewangles[1]-currententity->angles[1])/360*8 + 8 + 0.5-4)&7;
		pspriteframe = pspritegroup->frames[dir&7];
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

texture_t *R_TextureAnimation_Q2 (texture_t *base)
{
	int		reletive;
	int		frame;
	
	if (!base->anim_total)
		return base;

	//this is only ever used on world. everything other than rtlights have proper batches.
	frame = cl.time*2;	//q2 is lame

	reletive = frame % base->anim_total;

	while (reletive --> 0)
	{
		base = base->anim_next;
		if (!base)
			Sys_Error ("R_TextureAnimation: broken cycle");
	}

	return base;
}




unsigned int	r_viewcontents;
//mleaf_t		*r_viewleaf, *r_oldviewleaf;
//mleaf_t		*r_viewleaf2, *r_oldviewleaf2;
int		r_viewcluster, r_viewcluster2, r_oldviewcluster, r_oldviewcluster2;
int r_visframecount;
mleaf_t		*r_vischain;		// linked list of visible leafs
static pvsbuffer_t	curframevis[R_MAX_RECURSE];

/*
===============
R_MarkLeaves
===============
*/
#ifdef Q3BSPS
qbyte *R_MarkLeaves_Q3 (void)
{
	static qbyte	*cvis[R_MAX_RECURSE];
	qbyte *vis;
	int		i;

	int cluster;
	mleaf_t	*leaf;
	mnode_t *node;
	int portal = r_refdef.recurse;

	if (!portal)
	{
		if (r_oldviewcluster == r_viewcluster && !r_novis.value && r_viewcluster != -1)
			return cvis[portal];
	}

	// development aid to let you run around and see exactly where
	// the pvs ends
//		if (r_lockpvs->value)
//			return;

	r_vischain = NULL;
	r_visframecount++;
	r_oldviewcluster = r_viewcluster;

	if (r_novis.ival || r_viewcluster == -1 || !cl.worldmodel->vis )
	{
		vis = NULL;
		// mark everything
		for (i=0,leaf=cl.worldmodel->leafs ; i<cl.worldmodel->numleafs ; i++, leaf++)
		{
//			if (!leaf->nummarksurfaces)
//			{
//				continue;
//			}

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
		vis = CM_ClusterPVS (cl.worldmodel, r_viewcluster, &curframevis[portal], PVM_FAST);
		for (i=0,leaf=cl.worldmodel->leafs ; i<cl.worldmodel->numleafs ; i++, leaf++)
		{
			cluster = leaf->cluster;
			if (cluster == -1)// || !leaf->nummarksurfaces)
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
		cvis[portal] = vis;
	}
	return vis;
}
#endif

#ifdef Q2BSPS
qbyte *R_MarkLeaves_Q2 (void)
{
	static qbyte	*cvis[R_MAX_RECURSE];
	mnode_t	*node;
	int		i;

	int cluster;
	mleaf_t	*leaf;
	qbyte *vis;

	int portal = r_refdef.recurse;

	if (r_refdef.forcevis)
	{
		vis = cvis[portal] = r_refdef.forcedvis;

		r_oldviewcluster = -1;
		r_oldviewcluster2 = -1;
	}
	else
	{
		vis = cvis[portal];
		if (!portal)
		{
			if (r_oldviewcluster == r_viewcluster && r_oldviewcluster2 == r_viewcluster2)
				return vis;

			r_oldviewcluster = r_viewcluster;
			r_oldviewcluster2 = r_viewcluster2;
		}
		else
		{
			r_oldviewcluster = -1;
			r_oldviewcluster2 = -1;
		}

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

		if (r_viewcluster2 != r_viewcluster)	// may have to combine two clusters because of solid water boundaries
		{
			vis = CM_ClusterPVS (cl.worldmodel, r_viewcluster, &curframevis[portal], PVM_REPLACE);
			vis = CM_ClusterPVS (cl.worldmodel, r_viewcluster2, &curframevis[portal], PVM_MERGE);
		}
		else
			vis = CM_ClusterPVS (cl.worldmodel, r_viewcluster, &curframevis[portal], PVM_FAST);
		cvis[portal] = vis;
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

#ifdef Q1BSPS
#if 0
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
			c = (cl.worldmodel->numclusters+31)/32;
			for (i=0 ; i<c ; i++)
				((int *)curframevis)[i] |= ((int *)vis)[i];
			vis = curframevis;
		}
		else
			vis = Q1BSP_LeafPVS (cl.worldmodel, r_viewleaf, curframevis, sizeof(curframevis));
	}
	return vis;
}
#endif

qbyte *R_MarkLeaves_Q1 (qboolean getvisonly)
{
	static qbyte	*cvis[R_MAX_RECURSE];
	qbyte *vis;
	mnode_t	*node;
	int		i;
	int portal = r_refdef.recurse;

	//for portals to work, we need two sets of any pvs caches
	//this means lights can still check pvs at the end of the frame despite recursing in the mean time
	//however, we still need to invalidate the cache because we only have one 'visframe' field in nodes.

	if (r_refdef.forcevis)
	{
		vis = cvis[portal] = r_refdef.forcedvis;

		r_oldviewcluster = -1;
		r_oldviewcluster2 = -1;
	}
	else
	{
		if (!portal)
		{
			if (((r_oldviewcluster == r_viewcluster && r_oldviewcluster2 == r_viewcluster2) && !r_novis.ival) || r_novis.ival & 2)
				return cvis[portal];

			r_oldviewcluster = r_viewcluster;
			r_oldviewcluster2 = r_viewcluster2;
		}
		else
		{
			r_oldviewcluster = -1;
			r_oldviewcluster2 = -1;
		}

		if (r_novis.ival)
		{
			vis = cvis[portal] = curframevis[portal].buffer;
			memset (curframevis[portal].buffer, 0xff, curframevis[portal].buffersize);

			r_oldviewcluster = -1;
			r_oldviewcluster2 = -1;
		}
		else
		{
			if (r_viewcluster2 != -1 && r_viewcluster2 != r_viewcluster)
			{
				vis = cvis[portal] = cl.worldmodel->funcs.ClusterPVS(cl.worldmodel, r_viewcluster, &curframevis[portal], PVM_REPLACE);
				vis = cvis[portal] = cl.worldmodel->funcs.ClusterPVS(cl.worldmodel, r_viewcluster2, &curframevis[portal], PVM_MERGE);
			}
			else
				vis = cvis[portal] = cl.worldmodel->funcs.ClusterPVS(cl.worldmodel, r_viewcluster, &curframevis[portal], PVM_FAST);
		}
	}

	r_visframecount++;

	if (getvisonly)
		return vis;
	else if (r_viewcluster == -1)
	{
		//to improve spectating, when the camera is in a wall, we ignore any sky leafs.
		//this prevents seeing the upwards-facing sky surfaces within the sky volumes.
		//this will not affect inwards facing sky, so sky will basically appear as though it is identical to solid brushes.
		for (i=0 ; i<cl.worldmodel->numclusters ; i++)
		{
			if (vis[i>>3] & (1<<(i&7)))
			{
				if (cl.worldmodel->leafs[i+1].contents == Q1CONTENTS_SKY)
					continue;
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
	}
	else
	{
		for (i=0 ; i<cl.worldmodel->numclusters ; i++)
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
	}
	return vis;
}
#endif

/*
=================
R_CullBox

Returns true if the box is completely outside the frustom
=================
*/
qboolean R_CullBox (vec3_t mins, vec3_t maxs)
{
	//this isn't very precise.
	//checking each plane individually can be problematic
	//if you have a large object behind the view, it can cross multiple planes, and be infront of each one at some point, yet should still be outside the view.
	//this is quite noticable with terrain where the potential height of a section is essentually infinite.
	//note that this is not a concern for spheres, just boxes.
	int		i;

	for (i = 0; i < r_refdef.frustum_numplanes; i++)
		if (BOX_ON_PLANE_SIDE (mins, maxs, &r_refdef.frustum[i]) == 2)
			return true;
	return false;
}

qboolean R_CullSphere (vec3_t org, float radius)
{
	//four frustrum planes all point inwards in an expanding 'cone'.
	int		i;
	float d;

	for (i = 0; i < r_refdef.frustum_numplanes; i++)
	{
		d = DotProduct(r_refdef.frustum[i].normal, org)-r_refdef.frustum[i].dist;
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
			r_refdef.frustum[i].normal[0]	= mvp[3] + mvp[0+i/2];
			r_refdef.frustum[i].normal[1]	= mvp[7] + mvp[4+i/2];
			r_refdef.frustum[i].normal[2]	= mvp[11] + mvp[8+i/2];
			r_refdef.frustum[i].dist		= mvp[15] + mvp[12+i/2];
		}
		else
		{
			r_refdef.frustum[i].normal[0]	= mvp[3] - mvp[0+i/2];
			r_refdef.frustum[i].normal[1]	= mvp[7] - mvp[4+i/2];
			r_refdef.frustum[i].normal[2]	= mvp[11] - mvp[8+i/2];
			r_refdef.frustum[i].dist		= mvp[15] - mvp[12+i/2];
		}

		scale = 1/sqrt(DotProduct(r_refdef.frustum[i].normal, r_refdef.frustum[i].normal));
		r_refdef.frustum[i].normal[0]	*= scale;
		r_refdef.frustum[i].normal[1]	*= scale;
		r_refdef.frustum[i].normal[2]	*= scale;
		r_refdef.frustum[i].dist		*= -scale;

		r_refdef.frustum[i].type = PLANE_ANYZ;
		r_refdef.frustum[i].signbits = SignbitsForPlane (&r_refdef.frustum[i]);
	}

	r_refdef.frustum_numplanes = 4;

	r_refdef.frustum[r_refdef.frustum_numplanes].normal[0] = mvp[3] - mvp[2];
	r_refdef.frustum[r_refdef.frustum_numplanes].normal[1] = mvp[7] - mvp[6];
	r_refdef.frustum[r_refdef.frustum_numplanes].normal[2] = mvp[11] - mvp[10];
	r_refdef.frustum[r_refdef.frustum_numplanes].dist      = mvp[15] - mvp[14];

	scale = 1/sqrt(DotProduct(r_refdef.frustum[r_refdef.frustum_numplanes].normal, r_refdef.frustum[r_refdef.frustum_numplanes].normal));
	r_refdef.frustum[r_refdef.frustum_numplanes].normal[0] *= scale;
	r_refdef.frustum[r_refdef.frustum_numplanes].normal[1] *= scale;
	r_refdef.frustum[r_refdef.frustum_numplanes].normal[2] *= scale;
	r_refdef.frustum[r_refdef.frustum_numplanes].dist *= -scale;

	r_refdef.frustum[r_refdef.frustum_numplanes].type      = PLANE_ANYZ;
	r_refdef.frustum[r_refdef.frustum_numplanes].signbits  = SignbitsForPlane (&r_refdef.frustum[4]);

	r_refdef.frustum_numplanes++;

	r_refdef.frustum_numworldplanes = r_refdef.frustum_numplanes;

	//do far plane
	//fog will logically not actually reach 0, though precision issues will force it. we cut off at an exponant of -500
	if (r_refdef.globalfog.density)
	{
		float culldist;
		float fog;
		extern cvar_t r_fog_exp2;

		/*Documentation: the GLSL/GL will do this maths:
		float dist = 1024;
		if (r_fog_exp2.ival)
			fog = pow(2, -r_refdef.globalfog.density * r_refdef.globalfog.density * dist * dist * 1.442695);
		else
			fog = pow(2, -r_refdef.globalfog.density * dist * 1.442695);
		*/

		//the fog factor cut-off where its pointless to allow it to get closer to 0 (0 is technically infinite)
		fog = 2/255.0f;

		//figure out the eyespace distance required to reach that fog value
		culldist = log(fog);
		if (r_fog_exp2.ival)
			culldist = sqrt(culldist / (-r_refdef.globalfog.density * r_refdef.globalfog.density));
		else
			culldist = culldist / (-r_refdef.globalfog.density);
		//anything drawn beyond this point is fully obscured by fog

		r_refdef.frustum[r_refdef.frustum_numplanes].normal[0] = mvp[3] - mvp[2];
		r_refdef.frustum[r_refdef.frustum_numplanes].normal[1] = mvp[7] - mvp[6];
		r_refdef.frustum[r_refdef.frustum_numplanes].normal[2] = mvp[11] - mvp[10];
		r_refdef.frustum[r_refdef.frustum_numplanes].dist      = mvp[15] - mvp[14];

		scale = 1/sqrt(DotProduct(r_refdef.frustum[r_refdef.frustum_numplanes].normal, r_refdef.frustum[r_refdef.frustum_numplanes].normal));
		r_refdef.frustum[r_refdef.frustum_numplanes].normal[0] *= -scale;
		r_refdef.frustum[r_refdef.frustum_numplanes].normal[1] *= -scale;
		r_refdef.frustum[r_refdef.frustum_numplanes].normal[2] *= -scale;
//		r_refdef.frustum[r_refdef.frustum_numplanes].dist *= scale;
		r_refdef.frustum[r_refdef.frustum_numplanes].dist	= DotProduct(r_origin, r_refdef.frustum[r_refdef.frustum_numplanes].normal)-culldist;

		r_refdef.frustum[r_refdef.frustum_numplanes].type      = PLANE_ANYZ;
		r_refdef.frustum[r_refdef.frustum_numplanes].signbits  = SignbitsForPlane (&r_refdef.frustum[r_refdef.frustum_numplanes]);
		r_refdef.frustum_numplanes++;
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

	TEXASSIGN(particletexture, R_LoadTexture32("dotparticle", 8, 8, data, IF_NOMIPMAP|IF_NOPICMIP));


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
	particlecqtexture = Image_GetTexture("classicparticle", "particles", IF_NOMIPMAP|IF_NOPICMIP, data, NULL, 32, 32, TF_RGBA32);

	//draw a square in the top left. still a triangle.
	for (x=0 ; x<16 ; x++)
	{
		for (y=0 ; y<16 ; y++)
		{
			data[y*32+x][3] = 255;
		}
	}
	Image_GetTexture("classicparticle_square", "particles", IF_NOMIPMAP|IF_NOPICMIP, data, NULL, 32, 32, TF_RGBA32);


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
	explosiontexture = Image_GetTexture("fte_fuzzyparticle", "particles", IF_NOMIPMAP|IF_NOPICMIP, data, NULL, 16, 16, TF_RGBA32);

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
	Image_GetTexture("fte_bloodparticle", "particles", IF_NOMIPMAP|IF_NOPICMIP, data, NULL, 16, 16, TF_RGBA32);

	for (x=0 ; x<16 ; x++)
	{
		for (y=0 ; y<16 ; y++)
		{
			data[y*16+x][0] = min(255, exptexture[x][y]*255/9.0);
			data[y*16+x][1] = min(255, exptexture[x][y]*255/5.0);
			data[y*16+x][2] = min(255, exptexture[x][y]*255/5.0);
			data[y*16+x][3] = 255;
		}
	}
	Image_GetTexture("fte_blooddecal", "particles", IF_NOMIPMAP|IF_NOPICMIP, data, NULL, 16, 16, TF_RGBA32);

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
	beamtexture = R_LoadTexture32("beamparticle", PARTICLETEXTURESIZE, PARTICLETEXTURESIZE, data, IF_NOMIPMAP|IF_NOPICMIP);

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
	ptritexture = R_LoadTexture32("ptritexture", PARTICLETEXTURESIZE, PARTICLETEXTURESIZE, data, IF_NOMIPMAP|IF_NOPICMIP);
}

