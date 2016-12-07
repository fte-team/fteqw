//read menu.h

#include "quakedef.h"
#include "winquake.h"


static const char *res4x3[] =
{
	"640x480",
	"800x600",
	"960x720",
	"1024x768",
	"1152x864",
	"1280x960",
	"1440x1080",
	"1600x1200",
//	"1792x1344",
//	"1856x1392",
	"1920x1440",
	"2048x1536",
	NULL
};
static const char *res5x4[] =
{
	"1280x1024",
	"1800x1440",
	"2560x2048",
	NULL
};
static const char *res16x9[] =
{
	"856x480",
	"1024x576",
	"1280x720",
	"1366x768",
	"1600x900",
	"1920x1080",
	"2048x1152",
	"2560x1440",
	"3840x2160",
	"4096x2304",
	NULL
};
static const char *res16x10[] =
{
	"1024x640",
	"1152x720",
	"1280x800",
	"1440x900",
	"1680x1050",
	"1920x1200",
	"2304x1440",
	"2560x1600",
	NULL
};
#define ASPECT_RATIOS 4
static const char **resaspects[ASPECT_RATIOS] =
{
	res4x3,
	res5x4,
	res16x9,
	res16x10
};
#define ASPECT_LIST "4:3", "5:4", "16:9", "16:10",

qboolean M_Vid_GetMode(int num, int *w, int *h)
{
	int i;

	for (i = 0; i < 4; i++)
	{
		const char **v = resaspects[i];
		while (*v && num)
		{
			v++;
			num--;
		}
		if (*v)
		{
			const char *c = *v;
			const char *s = strchr(c, 'x');
			if (s)
			{
				*w = atoi(c);
				*h = atoi(s + 1);
				return true;
			}
			return false;
		}
	}
	return false;
}



#ifndef NOBUILTINMENUS

extern qboolean forcesaveprompt;
extern cvar_t pr_debugger;

menu_t *M_Options_Title(int *y, int infosize)
{
	struct menu_s *menu;
	*y = 32;

	Key_Dest_Add(kdm_emenu);
	m_state = m_complex;

	menu = M_CreateMenu(infosize);

	switch(M_GameType())
	{
	case MGT_QUAKE2:	//q2...
		MC_AddCenterPicture(menu, 4, 24, "pics/m_banner_options");
		*y += 32;
		break;
#ifdef HEXEN2
	case MGT_HEXEN2://h2
		MC_AddPicture(menu, 16, 0, 35, 176, "gfx/menu/hplaque.lmp");
		MC_AddCenterPicture(menu, 0, 60, "gfx/menu/title3.lmp");
		*y += 32;
		break;
#endif
	default: //q1
		MC_AddPicture(menu, 16, 4, 32, 144, "gfx/qplaque.lmp");
		MC_AddCenterPicture(menu, 4, 24, "gfx/p_option.lmp");
		break;
	}
	return menu;
}

//these are awkward/strange
qboolean M_Options_AlwaysRun (menucheck_t *option, struct menu_s *menu, chk_set_t set)
{
	if (M_GameType() == MGT_QUAKE2)
	{
		extern cvar_t cl_run;
		//quake2 mods have a nasty tendancy to hack at the various cvars, which breaks everything
		if (set != CHK_CHECKED)
			Cvar_SetValue(&cl_run, !cl_run.ival);
		return cl_run.ival;
	}
	else
	{
		//for better compat with other quake engines, we just ignore the cl_run cvar, at least for the menu.
		if (set == CHK_CHECKED)
			return cl_forwardspeed.value > 200;
		else if (cl_forwardspeed.value > 200)
		{
			Cvar_SetValue (&cl_forwardspeed, 200);
			if (*cl_backspeed.string)
				Cvar_SetValue (&cl_backspeed, 200);
			return false;
		}
		else
		{
			Cvar_SetValue (&cl_forwardspeed, 400);
			if (*cl_backspeed.string)
				Cvar_SetValue (&cl_backspeed, 400);
			return true;
		}
	}
}
qboolean M_Options_InvertMouse (menucheck_t *option, struct menu_s *menu, chk_set_t set)
{
	if (set == CHK_CHECKED)
		return m_pitch.value < 0;
	else
	{
		Cvar_SetValue (&m_pitch, -m_pitch.value);
		return m_pitch.value < 0;
	}
}

#ifdef HAVEAUTOUPDATE
static void M_Options_Remove(menu_t *m)
{
	menucombo_t *c = m->data;
	if (c)
		Sys_SetAutoUpdateSetting(c->selectedoption);
}
#endif

//options menu.
void M_Menu_Options_f (void)
{
	extern cvar_t crosshair, r_projection;
#ifndef CLIENTONLY
	extern cvar_t sv_autosave;
#endif
	int y;

#ifdef HAVEAUTOUPDATE
#define HAVEAUTOUPDATE
	menuoption_t *updatecbo;
	static const char *autoupopts[] = {
		"Revert",
		"Off",
		"Tested(Recommended)",
		"Untested(Latest)",
		NULL
	};
#endif
	static const char *projections[] = {
		"Regular",
		"Stereographic",
		"Fisheye",
		"Panoramic",
		"Lambert Azimuthal Equal-Area",
		"Equirectangular",
		NULL
	};
	static const char *projectionvalues[] = {
		"0",
		"1",
		"2",
		"3",
		"4",
		"5",
		NULL
	};

#ifndef CLIENTONLY
	static const char *autosaveopts[] = {
		"Off",
		"30 secs",
		"60 secs",
		"90 secs",
		"120 secs",
		"5 mins",
		NULL
	};
	static const char *autosavevals[] = {
		"0",
		"0.5",
		"1",
		"1.5",
		"2",
		"5",
		NULL
	};
#endif

	menubulk_t bulk[] = {
		MB_CONSOLECMD("Customize controls", "menu_keys\n", "Modify keyboard and mouse inputs."),
		MB_CONSOLECMD("Go to console", "toggleconsole\nplay misc/menu2.wav\n", "Open up the engine console."),
		MB_CONSOLECMD("Reset to defaults", "cvarreset *\nexec default.cfg\nplay misc/menu2.wav\n", "Reloads the default configuration."),
		MB_CONSOLECMD("Save all settings", "cfg_save\n", "Writes changed settings out to a config file."),
		MB_SPACING(4),
		MB_COMBOCVAR("View Projection", r_projection, projections, projectionvalues, NULL),
		MB_SLIDER("Field of View", scr_fov, 70, 360, 5, NULL),
		MB_SLIDER("Mouse Speed", sensitivity, 1, 10, 0.2, NULL),
		MB_SLIDER("Crosshair", crosshair, 0, 22, 1, NULL), // move this to hud setup?
		MB_CHECKBOXFUNC("Always Run", M_Options_AlwaysRun, 0, "Set movement to run at fastest speed by default."),
		MB_CHECKBOXFUNC("Invert Mouse", M_Options_InvertMouse, 0, "Invert vertical mouse movement."),
		MB_CHECKBOXCVAR("Lookspring", lookspring, 0),
		MB_CHECKBOXCVAR("Lookstrafe", lookstrafe, 0),
		MB_CHECKBOXCVAR("Windowed Mouse", _windowed_mouse, 0),
#ifdef HAVEAUTOUPDATE
		MB_COMBORETURN("Auto Update", autoupopts, Sys_GetAutoUpdateSetting(), updatecbo, "This downloads engine updates from the internet, when a new build is available."),
#endif
#ifndef CLIENTONLY
		MB_COMBOCVAR("Auto Save", sv_autosave, autosaveopts, autosavevals, NULL),
#endif
		MB_SPACING(4),
		// removed hud options (cl_sbar, cl_hudswap, old-style chat, old-style msg)
		MB_CONSOLECMD("Video Options", "menu_video\n", "Set video resolution, color depth, refresh rate, and anti-aliasing options."),
		MB_CONSOLECMD("Graphics Presets", "fps_preset\n", "Choose a different graphical preset to use."),
		MB_CONSOLECMD("Audio Options", "menu_audio\n", "Set audio quality and speaker setup options."),
		MB_SPACING(4),
		MB_CONSOLECMD("FPS Options", "menu_fps\n", "Set model filtering and graphical profile options."),
		MB_CONSOLECMD("Rendering Options", "menu_render\n", "Set rendering options such as water warp and tinting effects."),
		MB_CONSOLECMD("Lighting Options", "menu_lighting\n", "Set options for level lighting and dynamic lights."),
#ifdef GLQUAKE
		MB_CONSOLECMD("Texture Options", "menu_textures\n", "Set options for texture detail and effects."),
#endif
#ifndef MINIMAL
		MB_CONSOLECMD("Particle Options", "menu_particles\n", "Set particle effect options."),
#endif
#ifdef TEXTEDITOR
		//this option is a bit strange in q2.
//		MB_CHECKBOXCVAR("QC Debugger", pr_debugger, 0),
#endif
		// removed downloads (is this still appropriate?)
		// removed teamplay
		// removed singleplayer cheats (move this to single player menu)
		MB_END()
	};
	menu_t *menu = M_Options_Title(&y, 0);
	static menuresel_t resel;
	y = MC_AddBulk(menu, &resel, bulk, 16, 216, y);

#ifdef PLUGINS
	if (Cmd_Exists("ezhud_nquake"))
	{
		extern cvar_t plug_sbar;
		static const char *hudplugopts[] = {
			"Never",
			"Deathmatch",
			"Single Player/Coop",
			"Always",
			NULL
		};
		static const char *hudplugvalues[] = {
			"0",
			"1",
			"2",
			"3",
			NULL
		};
		MC_AddCvarCombo(menu, 16, 216, y, "Use Hud Plugin", &plug_sbar, hudplugopts, hudplugvalues);			y += 8;
	}
#endif

#ifdef HAVEAUTOUPDATE
	menu->data = updatecbo;
	menu->remove = M_Options_Remove;
#endif
}

#ifndef __CYGWIN__
typedef struct {
	int cursorpos;
	menuoption_t *cursoritem;

	menutext_t *speaker[MAXSOUNDCHANNELS];
	menutext_t *testsoundsource;

	soundcardinfo_t *card;
} audiomenuinfo_t;

qboolean M_Audio_Key (int key, struct menu_s *menu)
{
	int i;
	audiomenuinfo_t *info = menu->data;
	soundcardinfo_t *sc;
	for (sc = sndcardinfo; sc; sc = sc->next)
	{
		if (sc == info->card)
			break;
	}
	if (!sc)
	{
		M_RemoveMenu(menu);
		return true;
	}


	if (key == K_DOWNARROW)
	{
		info->testsoundsource->common.posy+=10;
	}
	if (key == K_UPARROW)
	{
		info->testsoundsource->common.posy-=10;
	}
	if (key == K_RIGHTARROW)
	{
		info->testsoundsource->common.posx+=10;
	}
	if (key == K_LEFTARROW)
	{
		info->testsoundsource->common.posx-=10;
	}
	if (key >= '0' && key <= '5')
	{
		i = key - '0';

		sc->speakerdir[i][0] = (info->testsoundsource->common.posy-200/2)/-50.0;
		sc->speakerdir[i][1] = (info->testsoundsource->common.posx-320/2)/-50.0;
		sc->speakerdir[i][2] = 0;

		sc->dist[i] = VectorLength(sc->speakerdir[i]);
	}

	menu->selecteditem = NULL;

	return false;
}

void M_Audio_StartSound (struct menu_s *menu)
{
	int i;
	vec3_t org;
	audiomenuinfo_t *info = menu->data;
	soundcardinfo_t *sc;
	vec3_t mat[4];

	static float lasttime;

	for (sc = sndcardinfo; sc; sc = sc->next)
	{
		if (sc == info->card)
			break;
	}
	if (!sc)
	{
		M_RemoveMenu(menu);
		return;
	}

	for (i = 0; i < sc->sn.numchannels; i++)
	{
		info->speaker[i]->common.posx = 320/2 + sc->speakerdir[i][1] * 50;
		info->speaker[i]->common.posy = 200/2 - sc->speakerdir[i][0] * 50;
	}
	for (; i < 6; i++)
		info->speaker[i]->common.posy = -100;

	if (lasttime+0.5 < Sys_DoubleTime())
	{
		S_GetListenerInfo(0, mat[0], mat[1], mat[2], mat[3]);

		lasttime = Sys_DoubleTime();
		org[0] = mat[0][0] + 2*(mat[2][0]*(info->testsoundsource->common.posx-320/2) - mat[1][0]*(info->testsoundsource->common.posy-200/2));
		org[1] = mat[0][1] + 2*(mat[2][1]*(info->testsoundsource->common.posx-320/2) - mat[1][1]*(info->testsoundsource->common.posy-200/2));
		org[2] = mat[0][2] + 2*(mat[2][2]*(info->testsoundsource->common.posx-320/2) - mat[1][2]*(info->testsoundsource->common.posy-200/2));
		S_StartSound(0, 0, S_PrecacheSound("player/pain3.wav"), org, NULL, 1, 4, 0, 0, 0);
	}
}

void M_Menu_Audio_Speakers_f (void)
{
	int i;
	audiomenuinfo_t *info;
	menu_t *menu;

	Key_Dest_Add(kdm_emenu);
	m_state = m_complex;

	menu = M_CreateMenu(sizeof(audiomenuinfo_t));
	info = menu->data;
	menu->key = M_Audio_Key;
	menu->predraw = M_Audio_StartSound;

	for (i = 0; i < 6; i++)
		info->speaker[i] = MC_AddBufferedText(menu, 0, 0, 0, va("%i", i), false, true);

	info->testsoundsource = MC_AddBufferedText(menu, 320/2, 320/2, 200/2, "X", false, true);

	info->card = sndcardinfo;

	menu->selecteditem = NULL;
}

struct audiomenuinfo
{
	char **outdevnames;
	char **outdevdescs;
#ifdef VOICECHAT
	char **capdevnames;
	char **capdevdescs;
#endif
};
void M_Menu_Audio_Remove(menu_t *menu)
{
	int i;
	struct audiomenuinfo *info = menu->data;
	for (i = 0; info->outdevnames[i]; i++)
		Z_Free(info->outdevnames[i]);
	Z_Free(info->outdevnames);
	for (i = 0; info->outdevdescs[i]; i++)
		Z_Free(info->outdevdescs[i]);
	Z_Free(info->outdevdescs);
#ifdef VOICECHAT
	for (i = 0; info->capdevnames[i]; i++)
		Z_Free(info->capdevnames[i]);
	Z_Free(info->capdevnames);
	for (i = 0; info->capdevdescs[i]; i++)
		Z_Free(info->capdevdescs[i]);
	Z_Free(info->capdevdescs);
#endif
}
struct audiomenuinfo *M_Menu_Audio_Setup(menu_t *menu)
{
#ifdef VOICECHAT
	extern cvar_t snd_voip_capturedevice_opts;
#endif
	extern cvar_t snd_device_opts;
	int pairs, i;
	struct audiomenuinfo *info = menu->data;
	menu->remove = M_Menu_Audio_Remove;

	Cmd_TokenizeString(snd_device_opts.string?snd_device_opts.string:"", false, false);
	pairs = Cmd_Argc()/2;
	info->outdevnames = BZ_Malloc((pairs+1)*sizeof(char*));
	info->outdevdescs = BZ_Malloc((pairs+1)*sizeof(char*));
	for (i = 0; i < pairs; i++)
	{
		info->outdevnames[i] = Z_StrDup(Cmd_Argv(i*2+0));
		info->outdevdescs[i] = Z_StrDup(Cmd_Argv(i*2+1));
	}
	info->outdevnames[i] = NULL;
	info->outdevdescs[i] = NULL;
#ifdef VOICECHAT
	Cmd_TokenizeString(snd_voip_capturedevice_opts.string?snd_voip_capturedevice_opts.string:"", false, false);
	pairs = Cmd_Argc()/2;
	info->capdevnames = BZ_Malloc((pairs+1)*sizeof(char*));
	info->capdevdescs = BZ_Malloc((pairs+1)*sizeof(char*));
	for (i = 0; i < pairs; i++)
	{
		info->capdevnames[i] = Z_StrDup(Cmd_Argv(i*2+0));
		info->capdevdescs[i] = Z_StrDup(Cmd_Argv(i*2+1));
	}
	info->capdevnames[i] = NULL;
	info->capdevdescs[i] = NULL;
#endif
	return info;
}

void M_Menu_Audio_f (void)
{
	int y;
	menu_t *menu = M_Options_Title(&y, sizeof(struct audiomenuinfo));
	struct audiomenuinfo *info = M_Menu_Audio_Setup(menu);
	extern cvar_t nosound, snd_leftisright, snd_device, snd_khz, snd_speakers, ambient_level, bgmvolume, snd_playersoundvolume, ambient_fade, cl_staticsounds, snd_inactive, _snd_mixahead, snd_doppler;
//	extern cvar_t snd_noextraupdate, snd_eax, precache;
#ifdef VOICECHAT
	extern cvar_t snd_voip_capturedevice, snd_voip_play, snd_voip_send, snd_voip_test, snd_voip_micamp, snd_voip_vad_threshhold, snd_voip_ducking, snd_voip_noisefilter, snd_voip_codec;
#endif

	static const char *soundqualityoptions[] = {
		"11025 Hz",
		"22050 Hz",
		"44100 Hz",
		"48000 Hz",
		NULL
	};

	static const char *soundqualityvalues[] = {
		"11",
		"22",
		"44",
		"48",
		NULL
	};

	static const char *speakeroptions[] = {
		"Mono",
		"Stereo",
		"Quad",
		"5.1",
		NULL
	};

	static const char *speakervalues[] = {
		"1",
		"2",
		"4",
		"6",
		NULL
	};
#ifdef VOICECHAT
	static const char *voipcodecoptions[] = {
		"Speex (ez-compat)",
//		"Raw (11025)",
		"Opus (external)",
		"Speex (Narrow)",
		"Speex (Wide)",
		NULL
	};
	static const char *voipcodecvalue[] = {
		"0",
//		"1",
		"2",
		"3",
		"4",
		NULL
	};

	static const char *voipsendoptions[] = {
		"Push To Talk",
		"Voice Activation",
		"Continuous",
		NULL
	};
	static const char *voipsendvalue[] = {
		"0",
		"1",
		"2",
		NULL
	};
#endif
	menubulk_t bulk[] = {
		MB_REDTEXT("Sound Options", true),
		MB_TEXT("^Ue080^Ue081^Ue081^Ue081^Ue081^Ue081^Ue081^Ue081^Ue081^Ue081^Ue081^Ue081^Ue082", true),
		MB_SPACING(8),
		MB_CONSOLECMD("Restart Sound", "snd_restart\n", "Restart audio systems and apply set options."),
		MB_SPACING(4),
		MB_COMBOCVAR("Output Device", snd_device, (const char**)info->outdevdescs, (const char**)info->outdevnames, "Choose which audio driver and device to use."),
		MB_SLIDER("Volume", volume, 0, 1, 0.1, NULL),
		MB_COMBOCVAR("Speaker Setup", snd_speakers, speakeroptions, speakervalues, NULL),
		MB_COMBOCVAR("Frequency", snd_khz, soundqualityoptions, soundqualityvalues, NULL),
		MB_CHECKBOXCVAR("Low Quality (8-bit)", loadas8bit, 0),
		MB_CHECKBOXCVAR("Flip Speakers", snd_leftisright, 0),
		MB_SLIDER("Mixahead", _snd_mixahead, 0, 1, 0.05, NULL),
		MB_CHECKBOXCVAR("Disable All Sounds", nosound, 0),
		MB_SPACING(4),

		MB_SLIDER("Player Sound Volume", snd_playersoundvolume, 0, 1, 0.1, NULL),
		MB_SLIDER("Ambient Volume", ambient_level, 0, 1, 0.1, NULL),
		MB_SLIDER("Ambient Fade", ambient_fade, 0, 1000, 1, NULL),
		MB_CHECKBOXCVAR("Static Sounds", cl_staticsounds, 0),
		MB_SLIDER("Music Volume", bgmvolume, 0, 1, 0.1, NULL),
		MB_SLIDER("Doppler Factor", snd_doppler, 0, 10, 0.1, NULL),
		// removed music buffer
		// removed precache
		// removed eax2
		// remove no extra update
		MB_CHECKBOXCVAR("Sound While Inactive", snd_inactive, 0),
		MB_SPACING(4),

#ifdef VOICECHAT
		MB_REDTEXT("Voice Options", true),
		MB_TEXT("^Ue080^Ue081^Ue081^Ue081^Ue081^Ue081^Ue081^Ue081^Ue081^Ue081^Ue081^Ue081^Ue082", true),
		MB_COMBOCVAR("Microphone Device", snd_voip_capturedevice, (const char**)info->capdevdescs, (const char**)info->capdevnames, NULL),
		MB_SLIDER("Voice Volume", snd_voip_play, 0, 2, 0.1, NULL),
		MB_CHECKBOXCVAR("Microphone Test", snd_voip_test, 0),
		MB_SLIDER("Microphone Volume", snd_voip_micamp, 0, 2, 0.1, NULL),
		MB_COMBOCVAR("Activation Mode", snd_voip_send, voipsendoptions, voipsendvalue, NULL),
		MB_SLIDER("Act. Threshhold", snd_voip_vad_threshhold, 0, 30, 1, NULL),
		MB_CHECKBOXCVAR("Audio Ducking", snd_voip_ducking, 0),
		MB_CHECKBOXCVAR("Noise Cancelation", snd_voip_noisefilter, 0),
		MB_COMBOCVAR("Codec", snd_voip_codec, voipcodecoptions, voipcodecvalue, NULL),
#endif

		//MB_CONSOLECMD("Speaker Test", "menu_speakers\n", "Test speaker setup output."),
		MB_END()
	};
	static menuresel_t resel;
	MC_AddBulk(menu, &resel, bulk, 16, 216, y);
}

#else
void M_Menu_Audio_f (void)
{
	Con_Printf("No sound in cygwin\n");
}
#endif



void M_Menu_Particles_f (void)
{
	menu_t *menu;
	extern cvar_t r_bouncysparks, r_part_rain, gl_part_flame, r_grenadetrail, r_rockettrail, r_part_rain_quantity, r_particledesc, r_particle_tracelimit, r_part_contentswitch, r_bloodstains;
//	extern cvar_t r_part_sparks_trifan, r_part_sparks_textured, r_particlesystem;

/*	static const char *psystemopts[] =
	{
		"Classic",
		"Script",
		"None",
		NULL
	};
	static const char *psystemvals[] =
	{
		"classic",
		"script",
		"null",
		NULL
	};
*/
	static const char *pdescopts[] =
	{
		"Classic",
		"Faithful",
		"High FPS",
		"Fancy",
		"Fancy+LG",
		"Snazzy",
		"Bare bones",
		NULL
	};
	static const char *pdescvals[] =
	{
		"classic",
		"faithful",
		"highfps",
		"spikeset",
		"spikeset tsshaft",
		"spikeset high tsshaft",
		"minimal",
		NULL
	};

	static const char *trailopts[] =
	{
		"Disable",
		"Default",
		"Swap",
		"Alternate",
		"Blood",
		"Zombie",
		"Scrag",
		"Knight",
		"Vore",
		"Rail",
		NULL
	};
	static const char *trailvals[] = { "0", "1", "2", "3", "4", "5", "6", "7", "8", "9", NULL };

	int y;
	menubulk_t bulk[] = {
		MB_REDTEXT("Particle Options", true),
		MB_TEXT("^Ue080^Ue081^Ue081^Ue081^Ue081^Ue081^Ue081^Ue081^Ue081^Ue081^Ue081^Ue081^Ue081^Ue081^Ue081^Ue082", true),
//		MB_COMBOCVAR("Particle System", r_particlesystem, psystemopts, psystemvals, "Selects particle system to use. Classic is standard Quake particles, script is FTE style scripted particles, and none disables particles entirely."),
		MB_COMBOCVAR("Particle Set", r_particledesc, pdescopts, pdescvals, "Selects particle set to use with the scripted particle system."),
		MB_SPACING(4),
		MB_COMBOCVAR("Rocket Trail", r_rockettrail, trailopts, trailvals, "Chooses effect to replace rocket trails."),
		MB_COMBOCVAR("Grenade Trail", r_grenadetrail, trailopts, trailvals, "Chooses effect to replace grenade trails."),
		MB_SPACING(4),
		// removed texture sparks
		// removed trifan sparks
		MB_CHECKBOXCVAR("Particle Physics", r_bouncysparks, 0),
		MB_CHECKBOXCVAR("Particle Stains", r_bloodstains, 0),
		MB_CHECKBOXCVAR("Content Switching", r_part_contentswitch, 0),
		MB_CHECKBOXCVAR("Surface Emitting", r_part_rain, 0),
		MB_SLIDER("Surface Quantity", r_part_rain_quantity, 0, 10, 1, NULL),
		MB_CHECKBOXCVAR("Model Emitting", gl_part_flame, 0),
		MB_SLIDER("Trace Limit", r_particle_tracelimit, 0, 2000, 100, NULL),
		// removed particle beams
		MB_END()
	};
	static menuresel_t resel;

	menu = M_Options_Title(&y, 0);
	MC_AddBulk(menu, &resel, bulk, 16, 200, y);
}

const char *presetname[] =
{
	"286",		//everything turned off to make it as fast as possible, even if you're crippled without it
	"Fast",		//typical deathmatch settings.
	"Vanilla",	//vanilla effects enabled, no content replacement.
	"Normal",	//content replacement enabled
	"Nice",		//potentially expensive, but not painful
	"Realtime",	//everything on
	NULL
};
#define PRESET_NUM 6

// this is structured like this for a possible future feature
// also don't include cvars that need a restart here
const char *presetexec[] =
{
	// 286 options (also the first commands to be execed in the chain)
	"seta m_preset_chosen 1;"
	"seta gl_texturemode nn;"
	"seta gl_texturemode2d n;"
	"seta gl_blendsprites 0;"
	"seta r_particlesystem null;"
	"seta r_particledesc \"\";"
	"seta r_part_classic_square 0;"
	"seta r_part_classic_expgrav 10;"
	"seta r_part_classic_opaque 0;"
	"seta r_stains 0;"
	"seta r_drawflat 1;"
	"seta r_lightmap 0;"
	"seta r_nolerp 1;"
	"seta r_nolightdir 1;"
	"seta r_dynamic 0;"
	"seta r_bloom 0;"
	"seta r_softwarebanding 0;"
	"seta d_mipcap 0 1000;"
	"seta gl_affinemodels 0;"
	"seta gl_polyblend 0;"
	"seta gl_flashblend 0;"
	"seta gl_specular 0;"
	"seta r_deluxemapping 0;"
	"seta r_loadlit 0;"
	"seta r_fastsky 1;"
	"seta r_drawflame 0;"
	"seta r_waterstyle 1;"
	"seta r_lavastyle 1;"		//defer to water
//	"seta r_slimestyle \"\";"	//defer to water
	"seta r_coronas 0;"
	"seta r_shadow_realtime_dlight 0;"
	"seta r_shadow_realtime_world 0;"
	"seta r_glsl_offsetmapping 0;"
//	"seta gl_detail 0;"
	"seta gl_load24bit 0;"
	"seta r_replacemodels \"\";"
	"seta r_waterwarp 0;"
	"seta r_lightstylesmooth 0;"
	"seta r_part_density 0.25;"
	"seta cl_nolerp 1;"
	"seta r_lerpmuzzlehack 0;"
	"seta v_gunkick 0;"
	"seta cl_rollangle 0;"
	"seta cl_bob 0;"
	"seta cl_sbar 0;"
	"seta sv_nqplayerphysics 0;"	//server settings in a preset might be bad.
	"seta cl_demoreel 0;"
	"seta cl_gibfilter 1;"
	"if cl_deadbodyfilter == 0 then seta cl_deadbodyfilter 1;"		//as useful as 2 is, some mods use death frames for crouching etc.
	"seta gl_simpleitems 1;"

	, // fast options
	"gl_texturemode ln;"
	"gl_texturemode2d n;"
#ifdef MINIMAL
	"r_particlesystem classic;"
#else
	"r_particlesystem script;"
#endif
	"r_particledesc classic;"
	"r_drawflat 0;"
	"r_nolerp 0;"
	"gl_flashblend 1;"
	"r_loadlit 1;"
	"r_fastsky 0;"
	"r_waterstyle 1;"
	"r_lavastyle 1;"
	"r_nolightdir 0;"
	"seta gl_simpleitems 0;"

	, //vanilla options.
	"r_part_density 1;"
	"gl_polyblend 1;"
	"r_dynamic 1;"
	"gl_flashblend 0;"
	"cl_nolerp 0;"	//projectiles lerped at least.
	"r_waterwarp 1;"
	"r_drawflame 1;"
	"v_gunkick 1;"
	"cl_rollangle 2.0;"
	"cl_bob 0.02;"
	//these things are perhaps a little extreme
	"r_loadlit 0;"
	"r_nolerp 1;"
	"r_softwarebanding 1;"		//ugly software banding.
	"d_mipcap 0 2;"				//gl without anisotropic filtering favours too-distant mips too often, so lets just pretend it doesn't exist. should probably mess with lod instead or something
	"gl_affinemodels 1;"
	"gl_texturemode nnl;"		//yup, we went there.
	"gl_texturemode2d n.l;"		//yeah, 2d too.
	"r_part_classic_square 1;"	//blocky baby!
	"r_part_classic_expgrav 1;"	//vanillaery
	"r_part_classic_opaque 1;"
//	"r_particlesystem script;"	//q2 or hexen2 particle effects need to be loadable
	"cl_sbar 1;"				//its a style thing
	"sv_nqplayerphysics 1;"		//gb wanted this
	"cl_demoreel 1;"			//yup, arcadey
	//"d_mipcap \"0 3\";"		//logically correct, but will fuck up on ATI drivers if increased mid-map, because ATI will just ignore any levels that are not currently enabled.
	"seta cl_gibfilter 0;"
	"seta cl_deadbodyfilter 0;"

	, // normal (faithful) options, but with content replacement thrown in
//#ifdef MINIMAL
//	"r_particlesystem classic;"
//#else
//	"r_particlesystem script;"
//	"r_particledesc classic;"
//#endif
	"r_part_classic_square 0;"
	"r_part_classic_expgrav 10;"	//gives a slightly more dynamic feel to them
	"r_part_classic_opaque 0;"
	"gl_load24bit 1;"
	"r_replacemodels \"md3 md2\";"
	"r_coronas 1;"
	"r_softwarebanding 0;"
	"d_mipcap 0 1000;"
	"gl_affinemodels 0;"
	"r_lerpmuzzlehack 1;"
	"gl_texturemode ln;"
	"gl_texturemode2d l;"
	"cl_sbar 0;"
	"sv_nqplayerphysics 0;"
	"cl_demoreel 0;"
	"r_loadlit 1;"
	"r_nolerp 0;"

	, // nice options
	"r_stains 0.75;"
	"gl_texturemode ll;"
#ifndef MINIMAL
//	"r_particlesystem script;"
	"r_particledesc \"high tsshaft\";"
#endif
	"gl_specular 1;"
//	"r_loadlit 2;"
	"r_waterstyle 2;"
	"gl_blendsprites 1;"
//	"r_fastsky -1;"
	"r_shadow_realtime_dlight 1;"
//	"gl_detail 1;"
	"r_lightstylesmooth 1;"
	"r_deluxemapping 1;"
	"gl_texture_anisotropic_filtering 4;"

	, // realtime options
	"r_bloom 1;"
	"r_deluxemapping 0;"	//won't be seen anyway
	"r_particledesc \"high tsshaft\";"
	"r_waterstyle 3;"
	"r_glsl_offsetmapping 1;"
	"r_shadow_realtime_world 1;"
	"gl_texture_anisotropic_filtering 16;"
};

typedef struct fpsmenuinfo_s
{
	menucombo_t *preset;
} fpsmenuinfo_t;

static void ApplyPreset (int presetnum)
{
	int i;
	//this function is written backwards, to ensure things work properly in configs etc.

	// TODO: work backwards and only set cvars once
	Cbuf_InsertText("\nfs_restart\nvid_reload\n", RESTRICT_LOCAL, true);
	for (i = presetnum; i >= 0; i--)
	{
		Cbuf_InsertText(presetexec[i], RESTRICT_LOCAL, true);
	}
	forcesaveprompt = true;
}

void M_Menu_Preset_f (void)
{
	menu_t *menu;
	int y;
	menubulk_t bulk[] =
	{
		MB_REDTEXT("Please Choose Preset", true),
		MB_TEXT("^Ue080^Ue081^Ue081^Ue081^Ue081^Ue081^Ue081^Ue081^Ue081^Ue081^Ue081^Ue081^Ue081^Ue081^Ue081^Ue081^Ue082", true),
		MB_CONSOLECMD("simple  (untextured)",	"fps_preset 286;menupop\n",			"Lacks textures, particles, pretty much everything."),
		MB_CONSOLECMD("fast    (deathmatch)",	"fps_preset fast;menupop\n",		"Fullscreen effects off to give consistant framerates"),
		MB_CONSOLECMD("vanilla  (softwarey)",	"fps_preset vanilla;menupop\n",		"This is for purists! Party like its 1995! No sanity spared!"),
		MB_CONSOLECMD("normal    (faithful)",	"fps_preset normal;menupop\n",		"An updated but still faithful appearance, using content replacements where applicable"),
		MB_CONSOLECMD("nice       (dynamic)",	"fps_preset nice;menupop\n",		"For people who like nice things, but still want to actually play"),
#ifdef RTLIGHTS
		MB_CONSOLECMD("realtime    (all on)",	"fps_preset realtime;menupop\n",	"For people who value pretty over fast/smooth. Not viable for deathmatch."),
#endif
		MB_END()
	};
	static menuresel_t resel;
	int item, bias = 0;
	extern cvar_t r_drawflat;
	menu = M_Options_Title(&y, 0);
	MC_AddBulk(menu, &resel, bulk, 16, 216, y);
	menu->selecteditem = menu->options;
	//bottoms up!
#ifdef RTLIGHTS
	if (r_shadow_realtime_world.ival)
		item = 1;	//realtime
	else
#else
	bias = 1;
#endif
		if (r_deluxemapping_cvar.ival)
		item = 2;	//nice
	else if (gl_load24bit.ival)
		item = 3;	//normal
	else if (r_softwarebanding_cvar.ival)
		item = 4;
	else if (!r_drawflat.ival)
		item = 5;
	else
		item = 6;
	item -= bias;
	while (item --> 0)
		menu->selecteditem = menu->selecteditem->common.next;
	menu->cursoritem->common.posy = menu->selecteditem->common.posy;
}

void FPS_Preset_f (void)
{
	char *presetfname;
	char *arg = Cmd_Argv(1);
	int i;

	if (!*arg)
	{
		M_Menu_Preset_f();
		return;
	}

	presetfname = va("configs/preset_%s.cfg", arg);
	if (COM_FCheckExists(presetfname))
	{
		char buffer[MAX_OSPATH];
		COM_QuotedString(presetfname, buffer, sizeof(buffer), false);
		Cbuf_AddText(va("\nexec %s\nfs_restart\n", buffer), RESTRICT_LOCAL);
		return;
	}

	for (i = 0; i < PRESET_NUM; i++)
	{
		if (!stricmp(presetname[i], arg))
		{
			ApplyPreset(i);
			return;
		}
	}

	Con_Printf("Preset %s not recognised\n", arg);
	Con_Printf("Valid presests:\n");
	for (i = 0; i < PRESET_NUM; i++)
		Con_Printf("%s\n", presetname[i]);
}

qboolean M_PresetApply (union menuoption_s *op, struct menu_s *menu, int key)
{
	fpsmenuinfo_t *info = (fpsmenuinfo_t*)menu->data;

	if (key != K_ENTER && key != K_KP_ENTER && key != K_MOUSE1)
		return false;

	Cbuf_AddText("fps_preset ", RESTRICT_LOCAL);
	Cbuf_AddText(info->preset->options[info->preset->selectedoption], RESTRICT_LOCAL);
	Cbuf_AddText("\n", RESTRICT_LOCAL);

	return true;
}

void M_Menu_FPS_f (void)
{
	static const char *fpsopts[] =
	{
		"Disabled",
		"Average FPS",
		"Worst FPS",
		"Best FPS",
		"Immediate FPS",
		"Average MSEC",
		"Worst MSEC",
		"Best MSEC",
		"Immediate MSEC",
		NULL
	};
	static const char *fpsvalues[] = {"0", "1", "2", "3", "4", "-1", "-2", "-3", "-4", NULL};
	static const char *entlerpopts[] =
	{
		"Enabled (always)",
		"Disabled",
		"Enabled (SP only)",
		NULL
	};
	static const char *playerlerpopts[] =
	{
		"Disabled",
		"Enabled",
		NULL
	};
	static const char *bodyopts[] =
	{
		"Disabled",
		"Ground",
		"All",
		NULL
	};
	static const char *values_0_1_2[] = {"0", "1", "2", NULL};
	static const char *values_0_1[] = {"0", "1", NULL};

	menu_t *menu;
	fpsmenuinfo_t *info;

	extern cvar_t v_contentblend, show_fps, cl_r2g, cl_gibfilter, cl_expsprite, cl_deadbodyfilter, cl_lerp_players, cl_nolerp;
	static menuresel_t resel;
	int y;
	menu = M_Options_Title(&y, sizeof(fpsmenuinfo_t));
	info = (fpsmenuinfo_t *)menu->data;

	/*lerping is here because I'm not sure where else to put it. if they care about framerate that much then they'll want to disable interpolation to get as up-to-date stuff as possible*/

	{
		menubulk_t bulk[] =
		{
			MB_REDTEXT("FPS Options", true),
			MB_TEXT("^Ue080^Ue081^Ue081^Ue081^Ue081^Ue081^Ue081^Ue081^Ue081^Ue081^Ue082", true),
			MB_COMBORETURN("Preset", presetname, 2, info->preset, "Select a builtin configuration of graphical settings."),
			MB_CMD("Apply", M_PresetApply, "Applies selected preset."),
			MB_SPACING(4),
			MB_COMBOCVAR("Show FPS", show_fps, fpsopts, fpsvalues, "Display FPS or frame millisecond values on screen. Settings except immediate are for values across 1 second."),
			MB_COMBOCVAR("Player lerping", cl_lerp_players, playerlerpopts, values_0_1, "Smooth movement of other players, but will increase effective latency. Does not affect all network protocols."),
			MB_COMBOCVAR("Entity lerping", cl_nolerp, entlerpopts, values_0_1_2, "Smooth movement of entities, but will increase effective latency."),
			MB_CHECKBOXCVAR("Content Blend", v_contentblend, 0),
			MB_CHECKBOXCVAR("Gib Filter", cl_gibfilter, 0),
			MB_COMBOCVAR("Dead Body Filter", cl_deadbodyfilter, bodyopts, values_0_1_2, "Selects which dead player frames to filter out in rendering. Ground frames are those of the player lying on the ground, and all frames include all used in the player dying animation."),
			MB_CHECKBOXCVAR("Explosion Sprite", cl_expsprite, 0),
			MB_CHECKBOXCVAR("Rockets to Grenades", cl_r2g, 0),
			MB_EDITCVAR("Skybox", "r_skybox"),
			MB_END()
		};
		MC_AddBulk(menu, &resel, bulk, 16, 216, y);
	}
}

void M_Menu_Render_f (void)
{
	static const char *warpopts[] =
	{
		"Disabled",
		"FOV Warp",
		"Shader",
		NULL
	};
	static const char *warpvalues[] =
	{
		"0",
		"-1",
		"1",
		NULL
	};
	static const char *logcenteropts[] = {"Off", "Singleplayer", "Always", NULL};
	static const char *logcentervalues[] = {"0", "1", "2", NULL};

	menu_t *menu;
	extern cvar_t r_novis, cl_item_bobbing, r_waterwarp, r_nolerp, r_noframegrouplerp, r_fastsky, gl_nocolors, gl_lerpimages, r_wateralpha, r_drawviewmodel, gl_cshiftenabled, r_hdr_irisadaptation, scr_logcenterprint;
#ifdef GLQUAKE
	extern cvar_t r_bloom;
#endif
	static menuresel_t resel;

	int y;
	menubulk_t bulk[] =
	{
		MB_REDTEXT("Rendering Options", true),
		MB_TEXT("^Ue080^Ue081^Ue081^Ue081^Ue081^Ue081^Ue081^Ue081^Ue081^Ue081^Ue081^Ue081^Ue081^Ue081^Ue081^Ue081^Ue082", true),
		MB_CHECKBOXCVAR("Disable VIS", r_novis, 0),
		MB_CHECKBOXCVAR("Fast Sky", r_fastsky, 0),
		MB_CHECKBOXCVAR("Disable Model Lerp", r_nolerp, 0),
		MB_CHECKBOXCVAR("Disable Framegroup Lerp", r_noframegrouplerp, 0),
		MB_CHECKBOXCVAR("Lerp Images", gl_lerpimages, 0),
		MB_COMBOCVAR("Water Warp", r_waterwarp, warpopts, warpvalues, NULL),
		MB_SLIDER("Water Alpha", r_wateralpha, 0, 1, 0.1, NULL),
		MB_SLIDER("Viewmodel Alpha", r_drawviewmodel, 0, 1, 0.1, NULL),
		MB_CHECKBOXCVAR("Poly Blending", gl_cshiftenabled, 0),
		MB_CHECKBOXCVAR("Disable Colormap", gl_nocolors, 0),
		MB_COMBOCVAR("Log Centerprints", scr_logcenterprint, logcenteropts, logcentervalues, "Display centerprints in the console also."),
#ifdef GLQUAKE
		MB_CHECKBOXCVAR("Bloom", r_bloom, 0),
#endif
		MB_CHECKBOXCVAR("HDR", r_hdr_irisadaptation, 0),
		MB_CHECKBOXCVAR("Model Bobbing", cl_item_bobbing, 0),
		MB_END()
	};
	menu = M_Options_Title(&y, 0);
	MC_AddBulk(menu, &resel, bulk, 16, 216, y);
}

#ifdef GLQUAKE
void M_Menu_Textures_f (void)
{
	static const char *texturefilternames[] =
	{
		"Nearest",
		"Bilinear",
		"Trilinear",
		NULL
	};
	static const char *texturefiltervalues[] =
	{
		"GL_NEAREST_MIPMAP_NEAREST",
		"GL_LINEAR_MIPMAP_NEAREST",
		"GL_LINEAR_MIPMAP_LINEAR",
		NULL
	};

	static const char *texture2dfilternames[] =
	{
		"Nearest",
		"Linear",
		NULL
	};
	static const char *texture2dfiltervalues[] =
	{
		"GL_NEAREST",
		"GL_LINEAR",
		NULL
	};


	static const char *anisotropylevels[] =
	{
		"Off",
		"2x",
		"4x",
		"8x",
		"16x",
		NULL
	};
	static const char *anisotropyvalues[] =
	{
		"1",
		"2",
		"4",
		"8",
		"16",
		NULL
	};

	static const char *texturesizeoptions[] =
	{
		"128",
		"196",
		"256",
		"384",
		"512",
		"768",
		"1024",
		"2048",
		"4096",
		"8192",
		NULL
	};

	extern cvar_t gl_load24bit, gl_specular, gl_compress, gl_picmip, gl_picmip2d, gl_max_size, r_drawflat, r_glsl_offsetmapping;
	extern cvar_t gl_texture_anisotropic_filtering, gl_texturemode, gl_texturemode2d, gl_mipcap;
	int y;
	menubulk_t bulk[] =
	{
		MB_REDTEXT("Texture Options", true),
		MB_TEXT("^Ue080^Ue081^Ue081^Ue081^Ue081^Ue081^Ue081^Ue081^Ue081^Ue081^Ue081^Ue081^Ue081^Ue081^Ue082", true),
		MB_CHECKBOXCVAR("Load Replacements", gl_load24bit, 0),
		MB_CHECKBOXCVAR("Simple Texturing", r_drawflat, 0),
		MB_COMBOCVAR("3D Filter Mode", gl_texturemode, texturefilternames, texturefiltervalues, "Chooses the texture filtering method used for 3D objects."),
		MB_COMBOCVAR("2D Filter Mode", gl_texturemode2d, texture2dfilternames, texture2dfiltervalues, "Chooses the texture filtering method used for HUD, menus, and other 2D assets."),
		MB_COMBOCVAR("Anisotropy", gl_texture_anisotropic_filtering, anisotropylevels, anisotropyvalues, NULL),
		MB_SPACING(4),
		MB_CHECKBOXCVAR("Software-style Rendering", r_softwarebanding_cvar, 0),
		MB_CHECKBOXCVAR("Deluxemapping", r_deluxemapping_cvar, 0),
		MB_CHECKBOXCVAR("Specular Highlights", gl_specular, 0),
//		MB_CHECKBOXCVAR("Detail Textures", gl_detail, 0),
		MB_CHECKBOXCVAR("offsetmapping", r_glsl_offsetmapping, 0),
		MB_SPACING(4),
		MB_CHECKBOXCVAR("Texture Compression", gl_compress, 0), // merge the save compressed tex options into here?
		MB_SLIDER("3D Picmip", gl_picmip, 0, 16, 1, NULL),
		MB_SLIDER("2D Picmip", gl_picmip2d, 0, 16, 1, NULL),
		MB_SLIDER("World Mipcap", gl_mipcap, 0, 3, 1, NULL),
		MB_COMBOCVAR("Max Texture Size", gl_max_size, texturesizeoptions, texturesizeoptions, NULL),
		MB_END()
	};
	menu_t *menu = M_Options_Title(&y, 0);
	static menuresel_t resel;
	MC_AddBulk(menu, &resel, bulk, 16, 216, y);
}
#endif

typedef struct {
	menucombo_t *lightcombo;
	menucombo_t *dlightcombo;
} lightingmenuinfo_t;

qboolean M_VideoApplyShadowLighting (union menuoption_s *op,struct menu_s *menu,int key)
{
	lightingmenuinfo_t *info = (lightingmenuinfo_t*)menu->data;

	if (key != K_ENTER && key != K_KP_ENTER && key != K_MOUSE1)
		return false;

#ifdef RTLIGHTS
	{
		char *cvarsrw = "0";
		char *cvarsrws = "0";
		char *cvarv = "0";
		switch (info->lightcombo->selectedoption)
		{
		case 1:
			cvarsrw = "1";
			break;
		case 2:
			cvarsrw = "1";
			cvarsrws = "1";
			break;
		case 3:
			cvarv = "1";
			break;
		}
#ifdef MINIMAL
		Cbuf_AddText(va("r_shadow_realtime_world %s;r_shadow_realtime_world_shadows %s\n", cvarsrw, cvarsrws), RESTRICT_LOCAL);
#else
		Cbuf_AddText(va("r_vertexlight %s;r_shadow_realtime_world %s;r_shadow_realtime_world_shadows %s\n", cvarv, cvarsrw, cvarsrws), RESTRICT_LOCAL);
#endif
	}
#endif

	{
		char *cvard = "0";
		char *cvarvd = "0";
		char *cvarsrd = "0";
		char *cvarsrds = "0";
		switch (info->dlightcombo->selectedoption)
		{
		case 1:
			cvard = "1";
			break;
		case 2:
			cvarsrd = "1";
			break;
		case 3:
			cvarsrd = "1";
			cvarsrds = "1";
			break;
		case 4:
			cvard = "-1";
			break;
		case 5:
			cvard = "1";
			cvarvd = "1";
			break;
		}
#ifdef RTLIGHTS
#ifdef MINIMAL
		Cbuf_AddText(va("r_shadow_realtime_dlight %s;r_shadow_realtime_dlight_shadows %s;r_dynamic %s\n", cvarsrd, cvarsrds, cvard), RESTRICT_LOCAL);
#else
		Cbuf_AddText(va("r_shadow_realtime_dlight %s;r_shadow_realtime_dlight_shadows %s;r_dynamic %s;r_vertexdlight %s\n", cvarsrd, cvarsrds, cvard, cvarvd), RESTRICT_LOCAL);
#endif
#else
#ifdef MINIMAL
		Cbuf_AddText(va("r_dynamic %s\n", cvard), RESTRICT_LOCAL);
#else
		Cbuf_AddText(va("r_dynamic %s;r_vertexdlight %s\n", cvard, cvarvd), RESTRICT_LOCAL);
#endif
#endif
		(void)cvarsrd, (void)cvarsrds, (void)cvard, (void)cvarvd;
	}

	Cbuf_AddText("vid_restart\n", RESTRICT_LOCAL);

	M_RemoveMenu(menu);
	Cbuf_AddText("menu_lighting\n", RESTRICT_LOCAL);
	return true;
}

void M_Menu_Lighting_f (void)
{
#ifndef MINIMAL
	extern cvar_t r_vertexlight;
#endif
	extern cvar_t r_stains, r_shadows, r_loadlits;
	extern cvar_t r_lightstylesmooth, r_nolightdir;
#ifdef RTLIGHTS
	extern cvar_t r_dynamic, r_shadow_realtime_world, r_shadow_realtime_dlight, r_shadow_realtime_dlight_shadows;
#ifndef MINIMAL
	extern cvar_t r_vertexdlights;
#endif
#endif
	extern cvar_t r_fb_models, r_rocketlight, r_powerupglow;
	extern cvar_t v_powerupshell, r_explosionlight;
	//extern cvar_t r_fb_bmodels, r_shadow_realtime_world_lightmaps, r_lightstylespeed;

	static const char *lightingopts[] =
	{
		"Standard",
#ifdef RTLIGHTS
		"Realtime",
		"RT+Shadows",
#ifndef MINIMAL
		"Vertex",
#endif
#endif
		NULL
	};
	static const char *dlightopts[] =
	{
		"None",
		"Standard",
#ifdef RTLIGHTS
		"Realtime",
		"RT+Shadows",
		"Threaded Lightmaps",
#ifndef MINIMAL
		"Vertex",
#endif
#endif
		NULL
	};

	static const char *loadlitopts[] =
	{
		"Disabled",
		"Enabled",
		"Generate",
		NULL
	};
	static const char *loadlitvalues[] =
	{
		"0",
		"1",
		"2",
		NULL
	};

	static const char *fbopts[] =
	{
		"Disabled",
		"Enabled",
		"Traced",
		NULL
	};
	static const char *fbvalues[] =
	{
		"0",
		"1",
		"2",
		NULL
	};

	static const char *powerupopts[] =
	{
		"Disabled",
		"Enabled",
		"Non-Self",
		NULL
	};
	static const char *powerupvalues[] =
	{
		"0",
		"1",
		"2",
		NULL
	};

	static const char *fb_models_opts[] =
	{
		"Disabled",
		"Entire model",
		"If textured",
		NULL
	};
	static const char *fb_models_values[] =
	{
		"0",
		"1",
		"2",
		NULL
	};

	int y;
	menu_t *menu = M_Options_Title(&y, sizeof(lightingmenuinfo_t));

	int lightselect, dlightselect;

#ifdef RTLIGHTS
	if (r_shadow_realtime_world.ival)
	{
		if (r_shadow_realtime_world_shadows.ival)
			lightselect = 2;
		else
			lightselect = 1;
	}
	else
#endif
#ifndef MINIMAL
	if (r_vertexlight.ival)
		lightselect = 3;
	else
#endif
		lightselect = 0;

#ifdef RTLIGHTS
	if (r_shadow_realtime_dlight.ival)
	{
		if (r_shadow_realtime_dlight_shadows.ival)
			dlightselect = 3;
		else
			dlightselect = 2;
	}
	else if (r_dynamic.ival < 0)
		dlightselect = 4;
#ifndef MINIMAL
	else if (r_vertexdlights.ival)
		dlightselect = 5;
#endif
	else
#endif
	if (r_dynamic.ival > 0)
		dlightselect = 1;
	else
		dlightselect = 0;

	{
		lightingmenuinfo_t *info = menu->data;
		menubulk_t bulk[] =
		{
			MB_REDTEXT("Lighting Options", true),
			MB_TEXT("^Ue080^Ue081^Ue081^Ue081^Ue081^Ue081^Ue081^Ue081^Ue081^Ue081^Ue081^Ue081^Ue081^Ue081^Ue081^Ue082", true),
			MB_COMBORETURN("Lighting Mode", lightingopts, lightselect, info->lightcombo, "Selects method used for world lighting. Realtime lighting requires appropriate realtime lighting files for maps."),
			MB_COMBORETURN("Dynamic Lighting Mode", dlightopts, dlightselect, info->dlightcombo, "Selects method used for dynamic lighting such as explosion lights and muzzle flashes."),
#ifdef RTLIGHTS
			MB_CHECKBOXCVARTIP("Soft Shadows", r_shadow_shadowmapping, 0, "Enables softer shadows instead of course-edged pixelated shadows."),
			MB_CMD("Apply Lighting", M_VideoApplyShadowLighting, "Applies set lighting modes and restarts video."),
			MB_SPACING(4),
#endif
			MB_COMBOCVAR("LIT Loading", r_loadlits, loadlitopts, loadlitvalues, "Determines if the engine should use external colored lighting for maps. The generated setting will cause the engine to generate colored lighting for maps that don't have the associated data."),
			MB_CHECKBOXCVAR("Lightstyle Lerp", r_lightstylesmooth, 0),
			MB_SPACING(4),
			MB_COMBOCVAR("Flash Blend", r_flashblend, fbopts, fbvalues, "Disables or enables the spherical light effect for dynamic lights. Traced means the sphere effect will be line of sight checked before displaying the effect."),
			MB_SLIDER("Explosion Light", r_explosionlight, 0, 1, 0.1, NULL),
			MB_SLIDER("Rocket Light", r_rocketlight, 0, 1, 0.1, NULL),
			MB_COMBOCVAR("Powerup Glow", r_powerupglow, powerupopts, powerupvalues, "Disables or enables the dynamic light effect for powerups. Non-self will disable the light only for the current player."),
			MB_CHECKBOXCVAR("Powerup Shell", v_powerupshell, 0),
			MB_SPACING(4),
			MB_SLIDER("Blob Shadows", r_shadows, 0, 1, 0.05, "Small blobs underneath monsters and players, to add depth to the scene without excessive rendering."),
			MB_SLIDER("Stains", r_stains, 0, 1, 0.05, "Allows discolouration of world surfaces, commonly used for blood trails."),
			MB_CHECKBOXCVARTIP("No Light Direction", r_nolightdir, 0, "Disables shading calculations for uniform light levels on models from all directions."),
			MB_COMBOCVAR("Model Fullbrights", r_fb_models, fb_models_opts, fb_models_values, "Affects loading of fullbrights on models/polymeshes."),
			MB_END()
		};
		static menuresel_t resel;
		MC_AddBulk(menu, &resel, bulk, 16, 216, y);
	}
}


typedef struct {
menucombo_t *skillcombo;
menucombo_t *mapcombo;
} singleplayerinfo_t;

#ifndef CLIENTONLY
static const char *maplist_q1[] =
{
	"start",
	"e1m1",
	"e1m2",
	"e1m3",
	"e1m4",
	"e1m5",
	"e1m6",
	"e1m7",
	"e1m8",
	"e2m1",
	"e2m2",
	"e2m3",
	"e2m4",
	"e2m5",
	"e2m6",
	"e2m7",
	"e3m1",
	"e3m2",
	"e3m3",
	"e3m4",
	"e3m5",
	"e3m6",
	"e3m7",
	"e4m1",
	"e4m2",
	"e4m3",
	"e4m4",
	"e4m5",
	"e4m6",
	"e4m7",
	"e4m8",
	"end"
};
static const char *mapoptions_q1[] =
{
	"Start (Introduction)",
	"E1M1 (The Slipgate Complex)",
	"E1M2 (Castle Of The Damned)",
	"E1M3 (The Necropolis)",
	"E1M4 (The Grisly Grotto)",
	"E1M5 (Gloom Keep)",
	"E1M6 (The Door To Chthon)",
	"E1M7 (The House Of Chthon)",
	"E1M8 (Ziggarat Vertigo)",
	"E2M1 (The Installation)",
	"E2M2 (The Ogre Citadel)",
	"E2M3 (The Crypt Of Decay)",
	"E2M4 (The Ebon Fortress)",
	"E2M5 (The Wizard's Manse)",
	"E2M6 (The Dismal Oubliette",
	"E2M7 (The Underearth)",
	"E3M1 (Termination Central)",
	"E3M2 (The Vaults Of Zin)",
	"E3M3 (The Tomb Of Terror)",
	"E3M4 (Satan's Dark Delight)",
	"E3M5 (The Wind Tunnels)",
	"E3M6 (Chambers Of Torment)",
	"E3M7 (Tha Haunted Halls)",
	"E4M1 (The Sewage System)",
	"E4M2 (The Tower Of Despair)",
	"E4M3 (The Elder God Shrine)",
	"E4M4 (The Palace Of Hate)",
	"E4M5 (Hell's Atrium)",
	"E4M6 (The Pain Maze)",
	"E4M7 (Azure Agony)",
	"E4M8 (The Nameless City)",
	"End (Shub-Niggurath's Pit)",
	NULL
};

#ifdef Q2CLIENT
static const char *maplist_q2[] =
{
	"base1",
	"base2",
	"base3",
	"train",
	"bunk1",
	"ware1",
	"ware2",
	"jail1",
	"jail2",
	"jail3",
	"jail4",
	"jail5",
	"security",
	"mintro",
	"mine1",
	"mine2",
	"mine3",
	"mine4",
	"fact1",
	"fact3",
	"fact2",
	"power1",
	"power2",
	"cool1",
	"waste1",
	"waste2",
	"waste3",
	"biggun",
	"hangar1",
	"space",
	"lab",
	"hangar2",
	"command",
	"strike",
	"city1",
	"city2",
	"city3",
	"boss1",
	"boss2"
};
static const char *mapoptions_q2[] =
{
	"base1 (Unit 1 Base Unit: Outer Base)",
	"base2 (Unit 1 Base Unit: Installation)",
	"base3 (Unit 1 Base Unit: Comm Center)",
	"train (Unit 1 Base Unit: Lost Station)",
	"bunk1 (Unit 2 Warehouse Unit: Ammo Depot)",
	"ware1 (Unit 2 Warehouse Unit: Supply Station)",
	"ware2 (Unit 2 Warehouse Unit: Warehouse)",
	"jail1 (Unit 3 Jail Unit: Main Gate)",
	"jail2 (Unit 3 Jail Unit: Destination Center)",
	"jail3 (Unit 3 Jail Unit: Security Compex)",
	"jail4 (Unit 3 Jail Unit: Torture Chambers)",
	"jail5 (Unit 3 Jail Unit: Guard House)",
	"security (Unit 3 Jail Unit: Grid Control)",
	"mintro (Unit 4 Mine Unit: Mine Entrance)",
	"mine1 (Unit 4 Mine Unit: Upper Mines)",
	"mine2 (Unit 4 Mine Unit: Borehole)",
	"mine3 (Unit 4 Mine Unit: Drilling Area)",
	"mine4 (Unit 4 Mine Unit: Lower Mines)",
	"fact1 (Unit 5 Factory Unit: Receiving Center)",
	"fact3 (Unit 5 Factory Unit: Sudden Death)",
	"fact2 (Unit 5 Factory Unit: Processing Plant)",
	"power1 (Unit 6 Power Unit/Big Gun: Power Plant)",
	"power2 (Unit 6 Power Unit/Big Gun: The Reactor)",
	"cool1 (Unit 6 Power Unit/Big Gun: Cooling Facility)",
	"waste1 (Unit 6 Power Unit/Big Gun: Toxic Waste Dump)",
	"waste2 (Unit 6 Power Unit/Big Gun: Pumping Station 1)",
	"waste3 (Unit 6 Power Unit/Big Gun: Pumping Station 2)",
	"biggun (Unit 6 Power Unit/Big Gun: Big Gun)",
	"hangar1 (Unit 7 Hangar Unit: Outer Hangar)",
	"space (Unit 7 Hangar Unit: Comm Satelite)",
	"lab (Unit 7 Hangar Unit: Research Lab)",
	"hangar2 (Unit 7 Hangar Unit: Inner Hangar)",
	"command (Unit 7 Hangar Unit: Launch Command)",
	"strike (Unit 7 Hangar Unit: Outlands)",
	"city1 (Unit 8 City Unit: Outer Courts)",
	"city2 (Unit 8 City Unit: Lower Palace)",
	"city3 (Unit 8 City Unit: Upper Palace)",
	"boss1 (Unit 9 Boss Levels: Inner Chamber)",
	"boss2 (Unit 9 Boss Levels: Final Showdown)",
	NULL
};
#endif
#endif

qboolean M_Apply_SP_Cheats (union menuoption_s *op,struct menu_s *menu,int key)
{
	singleplayerinfo_t *info = menu->data;

	if (key != K_ENTER && key != K_KP_ENTER && key != K_MOUSE1)
		return false;

	switch(info->skillcombo->selectedoption)
	{
	case 0:
		Cbuf_AddText("skill 0\n", RESTRICT_LOCAL);
		break;
	case 1:
		Cbuf_AddText("skill 1\n", RESTRICT_LOCAL);
		break;
	case 2:
		Cbuf_AddText("skill 2\n", RESTRICT_LOCAL);
		break;
	case 3:
		Cbuf_AddText("skill 3\n", RESTRICT_LOCAL);
		break;
	}

#ifndef CLIENTONLY
	if ((unsigned int)info->mapcombo->selectedoption >= sizeof(maplist_q1)/sizeof(maplist_q1[0]))
		Cbuf_AddText(va("map %s\n", maplist_q1[info->mapcombo->selectedoption]), RESTRICT_LOCAL);
#endif

	M_RemoveMenu(menu);
	Cbuf_AddText("menu_spcheats\n", RESTRICT_LOCAL);
	return true;
}


void M_Menu_Singleplayer_Cheats_Quake (void)
{
	#ifndef CLIENTONLY
	static const char *skilloptions[] =
	{
		"Easy",
		"Normal",
		"Hard",
		"Nightmare",
		"None Set",
		NULL
	};
	int currentskill;
	int currentmap;
	extern cvar_t sv_gravity, sv_cheats, sv_maxspeed, skill;
	extern cvar_t host_mapname;
	#endif
	singleplayerinfo_t *info;
	int cursorpositionY;
	int y;
	menu_t *menu = M_Options_Title(&y, sizeof(*info));
	info = menu->data;

	cursorpositionY = (y + 24);

	#ifndef CLIENTONLY
	if ( !*skill.string )
		currentskill = 4; // no skill selected
	else
		currentskill = skill.value;

	for (currentmap = sizeof(maplist_q1)/sizeof(maplist_q1[0]) - 1; currentmap > 0; currentmap--)
		if (!strcmp(host_mapname.string, maplist_q1[currentmap]))
			break;
	/*anything that doesn't match will end up with 0*/
	#endif

	MC_AddRedText(menu, 16, 170, y, 			"     Quake Singleplayer Cheats", false); y+=8;
	MC_AddWhiteText(menu, 16, 170, y,		"     ^Ue080^Ue081^Ue081^Ue081^Ue081^Ue081^Ue081^Ue081^Ue081^Ue081^Ue081^Ue081^Ue081^Ue081^Ue081^Ue081^Ue081^Ue081^Ue081^Ue081^Ue081^Ue081^Ue081^Ue082 ", false); y+=8;
	y+=8;
	#ifndef CLIENTONLY
	info->skillcombo = MC_AddCombo(menu,16,170, y,	"Difficulty", skilloptions, currentskill);	y+=8;
	info->mapcombo = MC_AddCombo(menu,16,170, y,	"Map", mapoptions_q1, currentmap);	y+=8;
	MC_AddCheckBox(menu,	16, 170, y,		"Cheats", &sv_cheats,0);	y+=8;
	#endif
	#ifdef TEXTEDITOR
	MC_AddCheckBox(menu,	16, 170, y,		"Debugger", &pr_debugger, 0); y+=8;
	#endif
	MC_AddConsoleCommand(menu, 16, 170, y,	"     Toggle Godmode", "god\n"); y+=8;
	MC_AddConsoleCommand(menu, 16, 170, y,	"     Toggle Flymode", "fly\n"); y+=8;
	MC_AddConsoleCommand(menu, 16, 170, y,	"      Toggle Noclip", "noclip\n"); y+=8;
	MC_AddConsoleCommand(menu, 16, 170, y,	"        Quad Damage", "impulse 255\n"); y+=8;
	#ifndef CLIENTONLY
	MC_AddSlider(menu,	16, 170, y,			"Gravity", &sv_gravity,0,800,25);	y+=8;
	#endif
	MC_AddSlider(menu,	16, 170, y,			"Forward Speed", &cl_forwardspeed,0,1000,50);	y+=8;
	MC_AddSlider(menu,	16, 170, y,			"Side Speed", &cl_sidespeed,0,1000,50);	y+=8;
	MC_AddSlider(menu,	16, 170, y,			"Back Speed", &cl_backspeed,0,1000,50);	y+=8;
	#ifndef CLIENTONLY
	MC_AddSlider(menu,	16, 170, y,			"Max Movement Speed", &sv_maxspeed,0,1000,50);	y+=8;
	#endif
	MC_AddConsoleCommand(menu, 16, 170, y,	" Silver & Gold Keys", "impulse 13\nimpulse 14\n"); y+=8;
	MC_AddConsoleCommand(menu, 16, 170, y,	"All Weapons & Items", "impulse 9\n"); y+=8;
	MC_AddConsoleCommand(menu, 16, 170, y,	"No Enemy Targetting", "notarget\n"); y+=8;
	#ifndef CLIENTONLY
	MC_AddConsoleCommand(menu, 16, 170, y,   "Restart Map", "restart\n"); y+=8;
	#else
	MC_AddConsoleCommand(menu, 16, 170, y,   "Suicide", "kill\n"); y+=8;
	#endif

	y+=8;
	MC_AddCommand(menu,	16, 170, y,			"Apply Changes", M_Apply_SP_Cheats);	y+=8;

	menu->selecteditem = (union menuoption_s *)info->skillcombo;
	menu->cursoritem = (menuoption_t*)MC_AddWhiteText(menu, 170, 0, cursorpositionY, NULL, false);
}

#ifdef Q2CLIENT
// Quake 2

typedef struct {
menucombo_t *skillcombo;
menucombo_t *mapcombo;
} singleplayerq2info_t;

qboolean M_Apply_SP_Cheats_Q2 (union menuoption_s *op,struct menu_s *menu,int key)
{
	singleplayerq2info_t *info = menu->data;

	if (key != K_ENTER && key != K_KP_ENTER && key != K_MOUSE1)
		return false;

	switch(info->skillcombo->selectedoption)
	{
	case 0:
		Cbuf_AddText("skill 0\n", RESTRICT_LOCAL);
		break;
	case 1:
		Cbuf_AddText("skill 1\n", RESTRICT_LOCAL);
		break;
	case 2:
		Cbuf_AddText("skill 2\n", RESTRICT_LOCAL);
		break;
	}

	if ((unsigned int)info->mapcombo->selectedoption >= sizeof(maplist_q2)/sizeof(maplist_q2[0]))
		Cbuf_AddText(va("map %s\n", maplist_q2[info->mapcombo->selectedoption]), RESTRICT_LOCAL);

	M_RemoveMenu(menu);
	Cbuf_AddText("menu_spcheats\n", RESTRICT_LOCAL);
	return true;
}

void M_Menu_Singleplayer_Cheats_Quake2 (void)
{

	static const char *skilloptions[] =
	{
		"Easy",
		"Normal",
		"Hard",
		"None Set",
		NULL
	};

	singleplayerq2info_t *info;
	int cursorpositionY;
	#ifndef CLIENTONLY
	int currentskill;
	int currentmap;
	extern cvar_t sv_gravity, sv_cheats, sv_maxspeed, skill;
	extern cvar_t host_mapname;
	#endif
	int y;
	menu_t *menu = M_Options_Title(&y, sizeof(*info));
	info = menu->data;

	cursorpositionY = (y + 24);

	#ifndef CLIENTONLY
	if ( !*skill.string )
		currentskill = 3; // no skill selected
	else
		currentskill = skill.value;

	for (currentmap = sizeof(maplist_q2)/sizeof(maplist_q2[0]) - 1; currentmap > 0; currentmap--)
		if (!strcmp(host_mapname.string, maplist_q2[currentmap]))
			break;
	/*anything that doesn't match will end up with 0*/
	#endif

	MC_AddRedText(menu, 16, 170, y, 		"Quake2 Singleplayer Cheats", false); y+=8;
	MC_AddWhiteText(menu, 16, 170, y,		"^Ue080^Ue081^Ue081^Ue081^Ue081^Ue081^Ue081^Ue081^Ue081^Ue081^Ue081^Ue081^Ue081^Ue081^Ue081^Ue081^Ue081^Ue081^Ue081^Ue081^Ue081^Ue081^Ue081^Ue081^Ue082", false); y+=8;
	y+=8;
	#ifndef CLIENTONLY
	info->skillcombo = MC_AddCombo(menu,16,170, y,	"Difficulty", skilloptions, currentskill);	y+=8;
	info->mapcombo = MC_AddCombo(menu,16,170, y,	"Map", mapoptions_q2, currentmap);	y+=8;
	MC_AddCheckBox(menu,	16, 170, y,		"Cheats", &sv_cheats,0);	y+=8;
	#endif
	MC_AddConsoleCommand(menu, 16, 170, y,	"Toggle Godmode", "god\n"); y+=8;
	MC_AddConsoleCommand(menu, 16, 170, y,	"Toggle Noclip", "noclip\n"); y+=8;
	#ifndef CLIENTONLY
	MC_AddSlider(menu,	16, 170, y,			"Gravity", &sv_gravity,0,850,25);	y+=8;
	#endif
	MC_AddSlider(menu,	16, 170, y,			"Forward Speed", &cl_forwardspeed,0,1000,50);	y+=8;
	MC_AddSlider(menu,	16, 170, y,			"Side Speed", &cl_sidespeed,0,1000,50);	y+=8;
	MC_AddSlider(menu,	16, 170, y,			"Back Speed", &cl_backspeed,0,1000,50);	y+=8;
	#ifndef CLIENTONLY
	MC_AddSlider(menu,	16, 170, y,			"Max Movement Speed", &sv_maxspeed,0,1000,50);	y+=8;
	#endif
	MC_AddConsoleCommand(menu, 16, 170, y,	"Unlimited Ammo", "dmflags 8192\n"); y+=8;
	MC_AddConsoleCommand(menu, 16, 170, y,	"Quad Damage", "give quad damage\n"); y+=8;
	MC_AddConsoleCommand(menu, 16, 170, y,	"Blue & Red Key", "give blue key\ngive red key\n"); y+=8;
	MC_AddConsoleCommand(menu, 16, 170, y,	"Pyramid Key", "give pyramid key\n"); y+=8;
	MC_AddConsoleCommand(menu, 16, 170, y,	"All Weapons & Items", "give all\n"); y+=8;
	MC_AddConsoleCommand(menu, 16, 170, y,	"Data Spinner", "give data spinner\n"); y+=8;
	MC_AddConsoleCommand(menu, 16, 170, y,	"Power Cube", "give power cube\n"); y+=8;
	MC_AddConsoleCommand(menu, 16, 170, y,	"Data CD", "give data cd\n"); y+=8;
	MC_AddConsoleCommand(menu, 16, 170, y,	"Ammo Pack", "give ammo pack\n"); y+=8;
	MC_AddConsoleCommand(menu, 16, 170, y,	"Bandolier", "give bandolier\n"); y+=8;
	MC_AddConsoleCommand(menu, 16, 170, y,	"Adrenaline", "give adrenaline\n"); y+=8;
	MC_AddConsoleCommand(menu, 16, 170, y,	"Ancient Head", "give ancient head\n"); y+=8;
	MC_AddConsoleCommand(menu, 16, 170, y,	"Environment Suit", "give environment suit\n"); y+=8;
	MC_AddConsoleCommand(menu, 16, 170, y,	"Rebreather", "give rebreather\n"); y+=8;
	MC_AddConsoleCommand(menu, 16, 170, y,	"Invulnerability", "give invulnerability\n"); y+=8;
	MC_AddConsoleCommand(menu, 16, 170, y,	"Silencer", "give silencer\n"); y+=8;
	MC_AddConsoleCommand(menu, 16, 170, y,	"Power Shield", "give power shield\n"); y+=8;
	MC_AddConsoleCommand(menu, 16, 170, y,	"Commander's Head", "give commander's head\n"); y+=8;
	MC_AddConsoleCommand(menu, 16, 170, y,	"Security Pass", "give security pass\n"); y+=8;
	MC_AddConsoleCommand(menu, 16, 170, y,	"Airstrike Marker", "give airstrike marker\n"); y+=8;
	#ifndef CLIENTONLY
	MC_AddConsoleCommand(menu, 16, 170, y,   "Restart Map", va("restart\n")); y+=8;
	#endif

	y+=8;
	MC_AddCommand(menu,	16, 170, y,			"Apply Changes", M_Apply_SP_Cheats_Q2);	y+=8;

	menu->selecteditem = (union menuoption_s *)info->skillcombo;
	menu->cursoritem = (menuoption_t*)MC_AddWhiteText(menu, 170, 0, cursorpositionY, NULL, false);
}
#endif	// Quake 2

#ifdef HEXEN2 // Hexen 2
typedef struct {
menucombo_t *skillcombo;
menucombo_t *mapcombo;
} singleplayerh2info_t;

qboolean M_Apply_SP_Cheats_H2 (union menuoption_s *op,struct menu_s *menu,int key)
{
	singleplayerh2info_t *info = menu->data;

	if (key != K_ENTER && key != K_KP_ENTER && key != K_MOUSE1)
		return false;

	switch(info->skillcombo->selectedoption)
	{
	case 0:
		Cbuf_AddText("skill 0\n", RESTRICT_LOCAL);
		break;
	case 1:
		Cbuf_AddText("skill 1\n", RESTRICT_LOCAL);
		break;
	case 2:
		Cbuf_AddText("skill 2\n", RESTRICT_LOCAL);
		break;
	case 3:
		Cbuf_AddText("skill 3\n", RESTRICT_LOCAL);
		break;
	}

	switch(info->mapcombo->selectedoption)
	{
	case 0:
		Cbuf_AddText("map demo1\n", RESTRICT_LOCAL);
		break;
	case 1:
		Cbuf_AddText("map demo2\n", RESTRICT_LOCAL);
		break;
	case 2:
		Cbuf_AddText("map demo3\n", RESTRICT_LOCAL);
		break;
	case 3:
		Cbuf_AddText("map village1\n", RESTRICT_LOCAL);
		break;
	case 4:
		Cbuf_AddText("map village2\n", RESTRICT_LOCAL);
		break;
	case 5:
		Cbuf_AddText("map village3\n", RESTRICT_LOCAL);
		break;
	case 6:
		Cbuf_AddText("map village4\n", RESTRICT_LOCAL);
		break;
	case 7:
		Cbuf_AddText("map village5\n", RESTRICT_LOCAL);
		break;
	case 8:
		Cbuf_AddText("map rider1a\n", RESTRICT_LOCAL);
		break;
	case 9:
		Cbuf_AddText("map meso1\n", RESTRICT_LOCAL);
		break;
	case 10:
		Cbuf_AddText("map meso2\n", RESTRICT_LOCAL);
		break;
	case 11:
		Cbuf_AddText("map meso3\n", RESTRICT_LOCAL);
		break;
	case 12:
		Cbuf_AddText("map meso4\n", RESTRICT_LOCAL);
		break;
	case 13:
		Cbuf_AddText("map meso5\n", RESTRICT_LOCAL);
		break;
	case 14:
		Cbuf_AddText("map meso6\n", RESTRICT_LOCAL);
		break;
	case 15:
		Cbuf_AddText("map meso8\n", RESTRICT_LOCAL);
		break;
	case 16:
		Cbuf_AddText("map meso9\n", RESTRICT_LOCAL);
		break;
	case 17:
		Cbuf_AddText("map egypt1\n", RESTRICT_LOCAL);
		break;
	case 18:
		Cbuf_AddText("map egypt2\n", RESTRICT_LOCAL);
		break;
	case 19:
		Cbuf_AddText("map egypt3\n", RESTRICT_LOCAL);
		break;
	case 20:
		Cbuf_AddText("map egypt4\n", RESTRICT_LOCAL);
		break;
	case 21:
		Cbuf_AddText("map egypt5\n", RESTRICT_LOCAL);
		break;
	case 22:
		Cbuf_AddText("map egypt6\n", RESTRICT_LOCAL);
		break;
	case 23:
		Cbuf_AddText("map egypt7\n", RESTRICT_LOCAL);
		break;
	case 24:
		Cbuf_AddText("map rider2c\n", RESTRICT_LOCAL);
		break;
	case 25:
		Cbuf_AddText("map romeric1\n", RESTRICT_LOCAL);
		break;
	case 26:
		Cbuf_AddText("map romeric2\n", RESTRICT_LOCAL);
		break;
	case 27:
		Cbuf_AddText("map romeric3\n", RESTRICT_LOCAL);
		break;
	case 28:
		Cbuf_AddText("map romeric4\n", RESTRICT_LOCAL);
		break;
	case 29:
		Cbuf_AddText("map romeric5\n", RESTRICT_LOCAL);
		break;
	case 30:
		Cbuf_AddText("map romeric6\n", RESTRICT_LOCAL);
		break;
	case 31:
		Cbuf_AddText("map romeric7\n", RESTRICT_LOCAL);
		break;
	case 32:
		Cbuf_AddText("map castle4\n", RESTRICT_LOCAL);
		break;
	case 33:
		Cbuf_AddText("map castle5\n", RESTRICT_LOCAL);
		break;
	case 34:
		Cbuf_AddText("map cath\n", RESTRICT_LOCAL);
		break;
	case 35:
		Cbuf_AddText("map tower\n", RESTRICT_LOCAL);
		break;
	case 36:
		Cbuf_AddText("map eidolon\n", RESTRICT_LOCAL);
		break;
	}

	M_RemoveMenu(menu);
	Cbuf_AddText("menu_spcheats\n", RESTRICT_LOCAL);
	return true;
}


void M_Menu_Singleplayer_Cheats_Hexen2 (void)
{

	static const char *skilloptions[] =
	{
		"Easy",
		"Normal",
		"Hard",
		"Nightmare",
		"None Set",
		NULL
	};

	static const char *mapoptions[] =
	{
		"demo1 (Blackmarsh: Hub 1 Blackmarsh)",
		"demo2 (Barbican: Hub 1 Blackmarsh)",
		"demo3 (The Mill: Hub 1 Blackmarsh)",
		"village1 (King's Court: Hub 1 Blackmarsh)",
		"village3 (Stables: Hub 1 Blackmarsh)",
		"village2 (Inner Courtyard: Hub 1 Blackmarsh)",
		"village4 (Palance Entrance: Hub 1 Blackmarsh)",
		"village5 (The Forgotten Chapel: Hub 1 Blackmarsh)",
		"rider1a (Famine's Domain: Hub 1 Blackmarsh)",
		"meso1 (Palance of Columns: Hub 2 Mazaera)",
		"meso2 (Plaza of the Sun: Hub 2 Mazaera)",
		"meso3 (Square of the Stream: Hub 2 Mazaera)",
		"meso4 (Tomb of the High Priest: Hub 2 Mazaera)",
		"meso5 (Obelisk of the Moon: Hub 2 Mazaera)",
		"meso6 (Court of 1000 Warriors: Hub 2 Mazaera)",
		"meso8 (Bridge of Stars: Hub 2 Mazaera)",
		"meso9 (Well of Souls: Hub 2 Mazaera)",
		"egypt1 (Temple of Horus: Hub 3 Thysis)",
		"egypt2 (Ancient Tempor of Nefertum: Hub 3 Thysis)",
		"egypt3 (Tempor of Nefertum: Hub 3 Thysis)",
		"egypt4 (Palace of the Pharaoh: Hub 3 Thysis",
		"egypt5 (Pyramid of Anubus: Hub 3 Thysis)",
		"egypt6 (Temple of Light: Hub 3 Thysis)",
		"egypt7 (Shrine of Naos: Hub 3 Thysis)",
		"rider2c (Pestilence's Lair: Hub 3 Thysis)",
		"romeric1 (The Hall of Heroes: Hub 4 Septimus)",
		"romeric2 (Gardens of Athena: Hub 4 Septimus)",
		"romeric3 (Forum of Zeus: Hub 4 Septimus)",
		"romeric4 (Baths of Demetrius: Hub 4 Septimus)",
		"romeric5 (Temple of Mars: Hub 4 Septimus)",
		"romeric6 (Coliseum of War: Hub 4 Septimus)",
		"romeric7 (Reflecting Pool: Hub 4 Septimus)",
		"castle4 (The Underhalls: Hub 5 Return to Blackmarsh)",
		"castle5 (Eidolon's Ordeal: Hub 5 Return to Blackmarsh)",
		"cath (Cathedral: Hub 5 Return to Blackmarsh)",
		"tower (Tower of the Dark Mage: Hub 5 Return to Blackmarsh)",
		"eidolon (Eidolon's Lair: Hub 5 Return to Blackmarsh)",
		NULL
	};

	singleplayerh2info_t *info;
	int cursorpositionY;
	int currentmap;
	#ifndef CLIENTONLY
		int currentskill;
		extern cvar_t sv_gravity, sv_cheats, sv_maxspeed, skill;
	#endif
	extern cvar_t host_mapname;
	int y;
	menu_t *menu = M_Options_Title(&y, sizeof(*info));
	info = menu->data;

	cursorpositionY = (y + 24);

	#ifndef CLIENTONLY
	if ( !*skill.string )
		currentskill = 4; // no skill selected
	else
		currentskill = skill.value;
	#endif

	if ( strcmp ( host_mapname.string, "" ) == 0)
		currentmap = 0;
	else if ( stricmp ( host_mapname.string, "demo1" ) == 0 )
		currentmap = 0;
	else if ( stricmp ( host_mapname.string, "demo2" ) == 0 )
		currentmap = 1;
	else if ( stricmp ( host_mapname.string, "demo3" ) == 0 )
		currentmap = 2;
	else if ( stricmp ( host_mapname.string, "village1" ) == 0 )
		currentmap = 3;
	else if ( stricmp ( host_mapname.string, "village2" ) == 0 )
		currentmap = 4;
	else if ( stricmp ( host_mapname.string, "village3" ) == 0 )
		currentmap = 5;
	else if ( stricmp ( host_mapname.string, "village4" ) == 0 )
		currentmap = 6;
	else if ( stricmp ( host_mapname.string, "village5" ) == 0 )
		currentmap = 7;
	else if ( stricmp ( host_mapname.string, "rider1a" ) == 0 )
		currentmap = 8;
	else if ( stricmp ( host_mapname.string, "meso1" ) == 0 )
		currentmap = 9;
	else if ( stricmp ( host_mapname.string, "meso2" ) == 0 )
		currentmap = 10;
	else if ( stricmp ( host_mapname.string, "meso3" ) == 0 )
		currentmap = 11;
	else if ( stricmp ( host_mapname.string, "meso4" ) == 0 )
		currentmap = 12;
	else if ( stricmp ( host_mapname.string, "meso5" ) == 0 )
		currentmap = 13;
	else if ( stricmp ( host_mapname.string, "meso6" ) == 0 )
		currentmap = 14;
	else if ( stricmp ( host_mapname.string, "meso8" ) == 0 )
		currentmap = 15;
	else if ( stricmp ( host_mapname.string, "meso9" ) == 0 )
		currentmap = 16;
	else if ( stricmp ( host_mapname.string, "egypt1" ) == 0 )
		currentmap = 17;
	else if ( stricmp ( host_mapname.string, "egypt2" ) == 0 )
		currentmap = 18;
	else if ( stricmp ( host_mapname.string, "egypt3" ) == 0 )
		currentmap = 19;
	else if ( stricmp ( host_mapname.string, "egypt4" ) == 0 )
		currentmap = 20;
	else if ( stricmp ( host_mapname.string, "egypt5" ) == 0 )
		currentmap = 21;
	else if ( stricmp ( host_mapname.string, "egypt6" ) == 0 )
		currentmap = 22;
	else if ( stricmp ( host_mapname.string, "egypt7" ) == 0 )
		currentmap = 23;
	else if ( stricmp ( host_mapname.string, "rider2c" ) == 0 )
		currentmap = 24;
	else if ( stricmp ( host_mapname.string, "romeric1" ) == 0 )
		currentmap = 25;
	else if ( stricmp ( host_mapname.string, "romeric2" ) == 0 )
		currentmap = 26;
	else if ( stricmp ( host_mapname.string, "romeric3" ) == 0 )
		currentmap = 27;
	else if ( stricmp ( host_mapname.string, "romeric4" ) == 0 )
		currentmap = 28;
	else if ( stricmp ( host_mapname.string, "romeric5" ) == 0 )
		currentmap = 29;
	else if ( stricmp ( host_mapname.string, "romeric6" ) == 0 )
		currentmap = 30;
	else if ( stricmp ( host_mapname.string, "romeric7" ) == 0 )
		currentmap = 31;
	else if ( stricmp ( host_mapname.string, "castle4" ) == 0 )
		currentmap = 32;
	else if ( stricmp ( host_mapname.string, "castle5" ) == 0 )
		currentmap = 33;
	else if ( stricmp ( host_mapname.string, "cath" ) == 0 )
		currentmap = 34;
	else if ( stricmp ( host_mapname.string, "tower" ) == 0 )
		currentmap = 35;
	else if ( stricmp ( host_mapname.string, "eidolon" ) == 0 )
		currentmap = 36;
	else
		currentmap = 0;

		MC_AddRedText(menu, 16, 170, y, 		"Hexen2 Singleplayer Cheats", false); y+=8;
		MC_AddWhiteText(menu, 16, 170, y,		"^Ue080^Ue081^Ue081^Ue081^Ue081^Ue081^Ue081^Ue081^Ue081^Ue081^Ue081^Ue081^Ue081^Ue081^Ue081^Ue081^Ue081^Ue081^Ue081^Ue081^Ue081^Ue081^Ue081^Ue081^Ue082 ", false); y+=8;
		y+=8;
		#ifndef CLIENTONLY
		info->skillcombo = MC_AddCombo(menu,16,170, y,	"Difficulty", skilloptions, currentskill);	y+=8;
		#endif
		info->mapcombo = MC_AddCombo(menu,16,170, y,	"Map", mapoptions, currentmap);	y+=8;
		#ifndef CLIENTONLY
		MC_AddCheckBox(menu,	16, 170, y,		"Cheats", &sv_cheats,0);	y+=8;
		#endif
		MC_AddConsoleCommand(menu, 16, 170, y,	"Toggle Godmode", "god\n"); y+=8;
		MC_AddConsoleCommand(menu, 16, 170, y,	"Toggle Flymode", "fly\n"); y+=8;
		MC_AddConsoleCommand(menu, 16, 170, y,	"Toggle Noclip", "noclip\n"); y+=8;
		#ifndef CLIENTONLY
		MC_AddSlider(menu,	16, 170, y,			"Gravity", &sv_gravity,0,800,25);	y+=8;
		#endif
		MC_AddSlider(menu,	16, 170, y,			"Forward Speed", &cl_forwardspeed,0,1000,50);	y+=8;
		MC_AddSlider(menu,	16, 170, y,			"Side Speed", &cl_sidespeed,0,1000,50);	y+=8;
		MC_AddSlider(menu,	16, 170, y,			"Back Speed", &cl_backspeed,0,1000,50);	y+=8;
		#ifndef CLIENTONLY
		MC_AddSlider(menu,	16, 170, y,			"Max Movement Speed", &sv_maxspeed,0,1000,50);	y+=8;
		#endif
		MC_AddConsoleCommand(menu, 16, 170, y,	"Sheep Transformation", "impulse 14\n"); y+=8;
		MC_AddConsoleCommand(menu, 16, 170, y,	"Change To Paladin (lvl3+)", "impulse 171\n"); y+=8;
		MC_AddConsoleCommand(menu, 16, 170, y,	"Change To Crusader (lvl3+)", "impulse 172\n"); y+=8;
		MC_AddConsoleCommand(menu, 16, 170, y,	"Change to Necromancer (lvl3+)", "impulse 173\n"); y+=8;
		MC_AddConsoleCommand(menu, 16, 170, y,	"Change to Assassin (lvl3+)", "impulse 174\n"); y+=8;
		MC_AddConsoleCommand(menu, 16, 170, y,	"Remove Monsters", "impulse 35\n"); y+=8;
		MC_AddConsoleCommand(menu, 16, 170, y,	"Freeze Monsters", "impulse 36\n"); y+=8;
		MC_AddConsoleCommand(menu, 16, 170, y,	"Unfreeze Monsters", "impulse 37\n"); y+=8;
		MC_AddConsoleCommand(menu, 16, 170, y,	"Increase Level By 1", "impulse 40\n"); y+=8;
		MC_AddConsoleCommand(menu, 16, 170, y,	"Increase Experience", "impulse 41\n"); y+=8;
		MC_AddConsoleCommand(menu, 16, 170, y,	"Display Co-ordinates", "impulse 42\n"); y+=8;
		MC_AddConsoleCommand(menu, 16, 170, y,	"All Weapons & Mana", "impulse 9\n"); y+=8;
		MC_AddConsoleCommand(menu, 16, 170, y,	"All Weapons & Mana & Items", "impulse 43\n"); y+=8;
		MC_AddConsoleCommand(menu, 16, 170, y,	"No Enemy Targetting", "notarget\n"); y+=8;
		MC_AddConsoleCommand(menu, 16, 170, y,	"Enable Crosshair", "crosshair 1\n"); y+=8;
		MC_AddConsoleCommand(menu, 16, 170, y,	"20 Of Each Artifact", "impulse 299\n"); y+=8;
		MC_AddConsoleCommand(menu, 16, 170, y,  "Restart Map", "impulse 99\n"); y+=8;

		y+=8;
		MC_AddCommand(menu,	16, 170, y,			"Apply Changes", M_Apply_SP_Cheats_H2);	y+=8;

	menu->selecteditem = (union menuoption_s *)info->skillcombo;
	menu->cursoritem = (menuoption_t*)MC_AddWhiteText(menu, 250, 0, cursorpositionY, NULL, false);
}
#endif

void M_Menu_Singleplayer_Cheats_f (void)
{
	switch(M_GameType())
	{
	case MGT_QUAKE1:
		M_Menu_Singleplayer_Cheats_Quake();
		break;
#ifdef Q2CLIENT
	case MGT_QUAKE2:
		M_Menu_Singleplayer_Cheats_Quake2();
		break;
#endif
#ifdef HEXEN2
	case MGT_HEXEN2:
		M_Menu_Singleplayer_Cheats_Hexen2();
		break;
#endif
	}
}

// video mode options
#if defined(D3DQUAKE) && defined(GLQUAKE)
#define MULTIRENDERER // allow options for selecting renderer
#endif

typedef struct {
	menucombo_t *dispmode;
	menucombo_t *resmode;
	menuedit_t *width;
	menuedit_t *height;
	menuedit_t *bpp;
	menuedit_t *hz;
	menucombo_t *bppfixed;
	menucombo_t *hzfixed;
	menucombo_t *res2dmode;
	menucombo_t *scale;
	menuedit_t *width2d;
	menuedit_t *height2d;
	menucombo_t *ressize[ASPECT_RATIOS];
	menucombo_t *res2dsize[ASPECT_RATIOS];
} videomenuinfo_t;

void CheckCustomMode(struct menu_s *menu)
{
	int i, sel;
	videomenuinfo_t *info = (videomenuinfo_t*)menu->data;

	// hide all display controls
	info->resmode->common.ishidden = true;
	info->width->common.ishidden = true;
	info->height->common.ishidden = true;
	info->bpp->common.ishidden = true;
	info->hz->common.ishidden = true;
	info->bppfixed->common.ishidden = true;
	info->hzfixed->common.ishidden = true;
	for (i = 0; i < ASPECT_RATIOS; i++)
		info->ressize[i]->common.ishidden = true;
	if (info->dispmode->selectedoption != 2)
	{
		info->resmode->common.ishidden = false;
		sel = info->resmode->selectedoption;
		if (sel < ASPECT_RATIOS)
		{
			// unhide appropriate aspect ratio combo and restricted bpp/hz combos
			info->bppfixed->common.ishidden = false;
			info->hzfixed->common.ishidden = false;
			info->ressize[sel]->common.ishidden = false;
		}
		else if (sel == (ASPECT_RATIOS + 1))
		{ // unhide custom entries for custom option
			info->width->common.ishidden = false;
			info->height->common.ishidden = false;
			info->bpp->common.ishidden = false;
			info->hz->common.ishidden = false;
		}
	}
	// hide all 2d display controls
	info->width2d->common.ishidden = true;
	info->height2d->common.ishidden = true;
	info->scale->common.ishidden = true;
	for (i = 0; i < ASPECT_RATIOS; i++)
		info->res2dsize[i]->common.ishidden = true;
	sel = info->res2dmode->selectedoption;
	if (sel < ASPECT_RATIOS) // unhide appropriate aspect ratio combo
		info->res2dsize[sel]->common.ishidden = false;
	else if (sel == (ASPECT_RATIOS + 1)) // unhide scale option
		info->scale->common.ishidden = false;
	else if (sel == (ASPECT_RATIOS + 2)) // unhide custom entries for custom option
	{
		info->width2d->common.ishidden = false;
		info->height2d->common.ishidden = false;
	}
}

int M_MatchModes(int width, int height, int *outres)
{
	int i;
	int ratio = -1;

	// find closest resolution for each ratio
	for (i = 0; i < ASPECT_RATIOS; i++)
	{
		const char **v = resaspects[i];
		outres[i] = 0;
		// search through each string in ratio array
		while (*v)
		{
			const char *c = *v;
			int w = atoi(c);
			if (width <= w)
			{
				if (width == w)
				{
					// if we match height as well we have a direct resolution match
					// so record ratio index
					const char *s = strchr(c, 'x');
					if (s)
					{
						int h = atoi(s + 1);
						if (height == h)
							ratio = i;
					}
				}
				break;
			}
			outres[i]++;
			v++;
		}
	}

	return ratio;
}

qboolean M_VideoApply (union menuoption_s *op, struct menu_s *menu, int key)
{
	extern cvar_t vid_desktopsettings;
	videomenuinfo_t *info = (videomenuinfo_t*)menu->data;

	if (key != K_ENTER && key != K_KP_ENTER && key != K_MOUSE1)
		return false;

	// force update display options
	{
		int w = 0, h = 0;
		const char *wc = NULL;
		const char *hc = NULL;
		const char *bc = "32";
		const char *fc = "0";
		const char *dc = "0";

		switch (info->resmode->selectedoption)
		{
		case ASPECT_RATIOS: // Desktop
			dc = "1";
			break;
		case ASPECT_RATIOS + 1: // Custom
			wc = info->width->text;
			hc = info->height->text;
			bc = info->bpp->text;
			fc = info->hz->text;
			break;
		default: // Aspects
			{
				menucombo_t *c = info->ressize[info->resmode->selectedoption];
				const char *res = c->options[c->selectedoption];
				const char *x = strchr(res, 'x');

				w = atoi(res);
				h = atoi(x + 1);

				bc = info->bppfixed->values[info->bppfixed->selectedoption];
				fc = info->hzfixed->values[info->hzfixed->selectedoption];
			}
		}

		if (!wc)
			Cvar_SetValue(info->width->cvar, w);
		else
			Cvar_Set(info->width->cvar, wc);
		if (!hc)
			Cvar_SetValue(info->height->cvar, h);
		else
			Cvar_Set(info->height->cvar, hc);
		Cvar_Set(info->bpp->cvar, bc);
		Cvar_Set(info->hz->cvar, fc);
		Cvar_Set(&vid_desktopsettings, dc);
	}

	// force update 2d options
	{
		int w = 0, h = 0;
		const char *wc = NULL;
		const char *hc = NULL;
		const char *sc = "0";

		switch (info->res2dmode->selectedoption)
		{
		case ASPECT_RATIOS: // Default
			break;
		case ASPECT_RATIOS + 1: // Scale
			sc = info->scale->values[info->scale->selectedoption];
			break;
		case ASPECT_RATIOS + 2: // Custom
			wc = info->width2d->text;
			hc = info->height2d->text;
			break;
		default: // Aspects
			{
				menucombo_t *c = info->res2dsize[info->res2dmode->selectedoption];
				const char *res = c->options[c->selectedoption];
				const char *x = strchr(res, 'x');

				w = atoi(res);
				h = atoi(x + 1);
			}
		}

		if (!wc)
			Cvar_SetValue(info->width2d->cvar, w);
		else
			Cvar_Set(info->width2d->cvar, wc);
		if (!hc)
			Cvar_SetValue(info->height2d->cvar, h);
		else
			Cvar_Set(info->height2d->cvar, hc);
		Cvar_Set(info->scale->cvar, sc);
	}

	// restart video to apply latched cvars
	M_RemoveMenu(menu);
	Cbuf_AddText("vid_restart\nmenu_video\n", RESTRICT_LOCAL);
	return true;
}

void M_Menu_Video_f (void)
{
	extern cvar_t v_contrast, vid_conwidth, vid_conheight;
//	extern cvar_t vid_width, vid_height, vid_preservegamma, vid_hardwaregamma, vid_desktopgamma;
	extern cvar_t vid_fullscreen, vid_desktopsettings, vid_conautoscale;
	extern cvar_t vid_bpp, vid_refreshrate, vid_multisample;

	extern cvar_t vid_renderer;
	static const char *rendererops[] =
	{
#ifdef GLQUAKE
		"OpenGL",
#endif
#ifdef VKQUAKE
		"Vulkan (Experimental)",
#endif
#ifdef D3D9QUAKE
		"Direct3D 9 (limited)",
#endif
#ifdef D3D11QUAKE
		"Direct3D 11 Hardware (Experimental)",
		"Direct3D 11 Software (Experimental)",
#endif
#ifdef SWQUAKE
		"Software Rendering (unusable)",
#endif
#if defined(_WIN32) && !defined(CLIENTONLY)	//win32 because other systems probably won't display a console if started via some shortcut
		"Dedicated Server",
#endif
#if 0
		"Headless",
#endif
		NULL
	};
	static const char *renderervalues[] =
	{
#ifdef GLQUAKE
		"gl",
#endif
#ifdef VKQUAKE
		"vk",
#endif
#ifdef D3D9QUAKE
		"d3d9",
#endif
#ifdef D3D11QUAKE
		"d3d11",
		"d3d11 warp",
#endif
#ifdef SWQUAKE
		"sw",
#endif
#if defined(_WIN32) && !defined(CLIENTONLY)
		"sv",
#endif
#if 0
		"headless",
#endif
		NULL
	};

	static const char *fullscreenopts[] = {
		"Windowed",
		"Fullscreen",
		"Borderless Windowed",
		NULL
	};
	static const char *fullscreenvalues[] = {"0", "1", "2", NULL};

	static const char *aaopts[] = {
		"1x",
		"2x",
		"4x",
		"6x",
		"8x",
		NULL
	};
	static const char *aavalues[] = {"0", "2", "4", "6", "8", NULL};

	static const char *resmodeopts[] = {
		ASPECT_LIST
		"Desktop",
		"Custom",
		NULL
	};

	static const char *bppopts[] =
	{
		"16-bit",
		"32-bit",
		NULL
	};
	static const char *bppvalues[] = {"16", "32", NULL};

	static const char *refreshopts[] =
	{
		"Default",
		"59Hz",
		"60Hz",
		"70Hz",
		"72Hz",
		"75Hz",
		"85Hz",
		"100Hz",
		"120Hz",
		NULL
	};
	static const char *refreshvalues[] = {"", "59", "60", "70", "72", "75", "85", "100", "120", NULL};

	static const char *res2dmodeopts[] = {
		ASPECT_LIST
		"Default",
		"Scale",
		"Custom",
		NULL
	};

	static const char *scaleopts[] = {
		"1x",
		"1.5x",
		"2x",
		"2.5x",
		"3x",
		"4x",
		"5x",
		"6x",
		NULL
	};
	static const char *scalevalues[] = { "1", "1.5", "2", "2.5", "3", "4", "5", "6", NULL};
/*
	static const char *vsyncoptions[] =
	{
		"Off",
		"Wait for Vertical Sync",
		"Wait for Display Enable",
		NULL
	};
	extern cvar_t vid_vsync;
*/
	videomenuinfo_t *info;
	static char current3dres[32]; // enough to fit 1920x1200

	static menuresel_t resel;
	int y;
	int resmodechoice, res2dmodechoice;
	int reschoices[ASPECT_RATIOS], res2dchoices[ASPECT_RATIOS];
	menu_t *menu = M_Options_Title(&y, sizeof(videomenuinfo_t));
	info = (videomenuinfo_t*)menu->data;

	snprintf(current3dres, sizeof(current3dres), "Current: %ix%i", vid.pixelwidth, vid.pixelheight);
	resmodechoice = M_MatchModes(vid.pixelwidth, vid.pixelheight, reschoices);
	if (vid_desktopsettings.ival)
		resmodechoice = ASPECT_RATIOS;
	else if (resmodechoice < 0)
		resmodechoice = ASPECT_RATIOS + 1;
	res2dmodechoice = M_MatchModes(vid.pixelwidth, vid.pixelheight, res2dchoices);
	if (vid_conautoscale.ival >= 1)
		res2dmodechoice = ASPECT_RATIOS + 1;
	else if (!vid_conwidth.ival && !vid_conheight.ival)
		res2dmodechoice = ASPECT_RATIOS;
	else if (res2dmodechoice < 0)
		res2dmodechoice = ASPECT_RATIOS + 2;

	{
		menubulk_t bulk[] =
		{
			MB_REDTEXT("Video Options", true),
			MB_TEXT("^Ue080^Ue081^Ue081^Ue081^Ue081^Ue081^Ue081^Ue081^Ue081^Ue081^Ue081^Ue081^Ue082", true),
			MB_COMBOCVAR("Renderer", vid_renderer, rendererops, renderervalues, NULL),
			MB_COMBOCVARRETURN("Display Mode", vid_fullscreen, fullscreenopts, fullscreenvalues, info->dispmode, vid_fullscreen.description),
			MB_COMBOCVAR("Anti-aliasing", vid_multisample, aaopts, aavalues, NULL),
			MB_REDTEXT(current3dres, true),
			MB_COMBORETURN("Aspect", resmodeopts, resmodechoice, info->resmode, "Select method for determining or configuring display options. The desktop option will attempt to use the width, height, color depth, and refresh from your operating system's desktop environment."),
			// aspect entries
			MB_COMBORETURN("Size", resaspects[0], reschoices[0], info->ressize[0], "Select resolution for display."),
			MB_SPACING(-8),
			MB_COMBORETURN("Size", resaspects[1], reschoices[1], info->ressize[1], "Select resolution for display."),
			MB_SPACING(-8),
			MB_COMBORETURN("Size", resaspects[2], reschoices[2], info->ressize[2], "Select resolution for display."),
			MB_SPACING(-8),
			MB_COMBORETURN("Size", resaspects[3], reschoices[3], info->ressize[3], "Select resolution for display."),
			MB_COMBOCVARRETURN("Color Depth", vid_bpp, bppopts, bppvalues, info->bppfixed, vid_bpp.description),
			MB_COMBOCVARRETURN("Refresh Rate", vid_refreshrate, refreshopts, refreshvalues, info->hzfixed, vid_refreshrate.description),
			MB_SPACING(-24), // really hacky...
			// custom entries
			MB_EDITCVARSLIMRETURN("Width", "vid_width", info->width),
			MB_EDITCVARSLIMRETURN("Height", "vid_height", info->height),
			MB_EDITCVARSLIMRETURN("Color Depth", "vid_bpp", info->bpp),
			MB_EDITCVARSLIMRETURN("Refresh Rate", "vid_displayfrequency", info->hz),
			MB_SPACING(4),
			MB_COMBORETURN("2D Mode", res2dmodeopts, res2dmodechoice, info->res2dmode, "Select method for determining or configuring 2D resolution and scaling. The default option matches the current display resolution, and the scale option scales by a factor of the display resolution."),
			// scale entry
			MB_COMBOCVARRETURN("Amount", vid_conautoscale, scaleopts, scalevalues, info->scale, NULL),
			MB_SPACING(-8),
			// 2d aspect entries
			MB_COMBORETURN("Size", resaspects[0], res2dchoices[0], info->res2dsize[0], "Select resolution for 2D rendering."),
			MB_SPACING(-8),
			MB_COMBORETURN("Size", resaspects[1], res2dchoices[1], info->res2dsize[1], "Select resolution for 2D rendering."),
			MB_SPACING(-8),
			MB_COMBORETURN("Size", resaspects[2], res2dchoices[2], info->res2dsize[2], "Select resolution for 2D rendering."),
			MB_SPACING(-8),
			MB_COMBORETURN("Size", resaspects[3], res2dchoices[3], info->res2dsize[3], "Select resolution for 2D rendering."),
			MB_SPACING(-8),
			// 2d custom entries
			MB_EDITCVARSLIMRETURN("Width", "vid_conwidth", info->width2d),
			MB_EDITCVARSLIMRETURN("Height", "vid_conheight", info->height2d),
			MB_SPACING(4),
			MB_CMD("Apply Settings", M_VideoApply, "Restart video and apply renderer, display, and 2D resolution options."),
			MB_SPACING(4),
			MB_SLIDER("View Size", scr_viewsize, 30, 120, 10, NULL),
			MB_SLIDER("Gamma", v_gamma, 1.5, 0.25, -0.05, NULL),
			MB_SLIDER("Contrast", v_contrast, 0.8, 3, 0.05, NULL),
			MB_END()
		};
		MC_AddBulk(menu, &resel, bulk, 16, 200, y);
	}

	/*
	y += 8;
	MC_AddRedText(menu, 200, y, current3dres, false); y+=8;

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
	//MC_AddCheckBox(menu,	16, y,							"   Override VSync", &vid_vsync,0);	y+=8;
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
	MC_AddCheckBox(menu,	16, y,							"   Windowed Mouse", &_windowed_mouse,0);	y+=8;

	menu->selecteditem = (union menuoption_s *)info->renderer;
	menu->cursoritem = (menuoption_t*)MC_AddWhiteText(menu, 152, menu->selecteditem->common.posy, NULL, false);
	*/
	menu->predraw = CheckCustomMode;
}

#ifndef MINIMAL
typedef struct
{
	enum {
		MV_NONE,
		MV_BONES,
		MV_SHADER,
		MV_TEXTURE
	} mode;
	int surfaceidx;
	int skingroup;
	int framegroup;
	double framechangetime;
	double skinchangetime;
	float pitch;
	float yaw;
	float dist;
	char modelname[MAX_QPATH];
	char forceshader[MAX_QPATH];

	char shaderfile[MAX_QPATH];
	char *shadertext;
} modelview_t;

static unsigned int genhsv(float h_, float s, float v)
{
	float r=0, g=1, b=0;

	float h = h_ - floor(h_);

	int i = floor(h * 6);
	float f = h * 6 - i;
	float p = v * (1 - s);
	float q = v * (1 - f * s);
	float t = v * (1 - (1 - f) * s);
	switch(i)
	{
		case 0: r = v, g = t, b = p; break;
		case 1: r = q, g = v, b = p; break;
		case 2: r = p, g = v, b = t; break;
		case 3: r = p, g = q, b = v; break;
		case 4: r = t, g = p, b = v; break;
		case 5: r = v, g = p, b = q; break;
	}

	return 0xff000000 |
		((int)(r*255)<<16) |
		((int)(g*255)<<8) |
		((int)(b*255)<<0);
}

#include "com_mesh.h"
#ifdef SKELETALMODELS
static void M_BoneDisplay(entity_t *e, galiasbone_t *b, int *y, int depth, int parent, int first, int last)
{
	int i;
	for (i = first; i < last;  i++)
	{
		if (b[i].parent == parent)
		{
			float result[12];
			memset(result, 0, sizeof(result));
			if (Mod_GetTag(e->model, i+1, &e->framestate, result))
				Draw_FunString(depth*16, *y, va("%i: %s (%g %g %g)", i, b[i].name, result[3], result[7], result[11]));
			else
				Draw_FunString(depth*16, *y, va("%i: %s", i, b[i].name));
			*y += 8;
			M_BoneDisplay(e, b, y, depth+1, i, i+1, last);
		}
	}
}
#endif
static void M_ModelViewerDraw(int x, int y, struct menucustom_s *c, struct menu_s *m)
{
	static playerview_t pv;
	entity_t ent;
	vec3_t fwd, rgt, up;
	const char *fname;
	shader_t *shader;
	vec2_t fs = {8,8};
	float bones[12*MAX_BONES];

	modelview_t *mods = c->dptr;

	if (R2D_Flush)
		R2D_Flush();

	memset(&pv, 0, sizeof(pv));

	CL_DecayLights ();
	CL_ClearEntityLists();
	V_ClearRefdef(&pv);
	r_refdef.drawsbar = false;
	V_CalcRefdef(&pv);
	r_refdef.grect.width = vid.width;
	r_refdef.grect.height = vid.height;
	r_refdef.grect.x = 0;
	r_refdef.grect.y = 0;
	r_refdef.time = realtime;

	r_refdef.flags = RDF_NOWORLDMODEL;

	r_refdef.afov = 60;
	r_refdef.fov_x = 0;
	r_refdef.fov_y = 0;
	r_refdef.dirty |= RDFD_FOV;

	VectorClear(r_refdef.viewangles);
	r_refdef.viewangles[0] = mods->pitch;
	r_refdef.viewangles[1] = mods->yaw;
	AngleVectors(r_refdef.viewangles, fwd, rgt, up);
	VectorScale(fwd, -mods->dist, r_refdef.vieworg);

	memset(&ent, 0, sizeof(ent));
	ent.scale = 1;
	ent.model = Mod_ForName(mods->modelname, MLV_WARN);
	if (!ent.model)
		return;	//panic!
	ent.origin[2] -= (ent.model->maxs[2]-ent.model->mins[2]) * 0.5 + ent.model->mins[2];
	Vector4Set(ent.shaderRGBAf, 1, 1, 1, 1);
	if (strstr(mods->modelname, "player"))
	{
		ent.bottomcolour	= genhsv(realtime*0.1 + 0, 1, 1);
		ent.topcolour		= genhsv(realtime*0.1 + 0.5, 1, 1);
	}
	else
	{
		ent.topcolour = TOP_DEFAULT;
		ent.bottomcolour = BOTTOM_DEFAULT;
	}
//	ent.fatness = sin(realtime)*5;
	ent.playerindex = -1;
	ent.skinnum = mods->skingroup;
	ent.shaderTime = 0;//realtime;
	ent.framestate.g[FS_REG].lerpweight[0] = 1;
	ent.framestate.g[FS_REG].frame[0] = mods->framegroup;
	ent.framestate.g[FS_REG].frametime[0] = ent.framestate.g[FS_REG].frametime[1] = realtime - mods->framechangetime;
	ent.customskin = Mod_RegisterSkinFile(va("%s_0.skin", mods->modelname));

//	ent.framestate.bonecount = Mod_GetNumBones(ent.model, false);
	ent.framestate.bonestate = bones;
	ent.framestate.bonecount = Mod_GetBoneRelations(ent.model, 0, MAX_BONES, &ent.framestate, ent.framestate.bonestate);
	ent.framestate.skeltype = SKEL_RELATIVE;

	ent.light_avg[0] = ent.light_avg[1] = ent.light_avg[2] = 0.66;
	ent.light_range[0] = ent.light_range[1] = ent.light_range[2] = 0.33;
	ent.light_dir[0] = 0; ent.light_dir[1] = 1; ent.light_dir[2] = 0;
	ent.light_known = 2;

	V_AddEntity(&ent);

	V_ApplyRefdef();
	R_RenderView();

	y = 0;

	fname = Mod_SurfaceNameForNum(ent.model, mods->surfaceidx);
	if (!fname)
		fname = "Unknown Surface";
	Draw_FunString(0, y, va("Surf %i: %s", mods->surfaceidx, fname));
	y+=8;
	
	{
		char *fname;
		int numframes = 0;
		float duration = 0;
		qboolean loop = false;
		if (!Mod_FrameInfoForNum(ent.model, mods->surfaceidx, mods->framegroup, &fname, &numframes, &duration, &loop))
			fname = "Unknown Frame";
		Draw_FunString(0, y, va("Frame%i: %s (%i poses, %f/%f secs, %s)", mods->framegroup, fname, numframes, ent.framestate.g[FS_REG].frametime[0], duration, loop?"looped":"unlooped"));
		y+=8;
	}
	fname = Mod_SkinNameForNum(ent.model, mods->surfaceidx, mods->skingroup);
	if (!fname)
		fname = "Unknown Skin";
	shader = Mod_ShaderForSkin(ent.model, mods->surfaceidx, mods->skingroup);
	Draw_FunString(0, y, va("Skin %i: (%s) %s", mods->skingroup, fname, shader?shader->name:"NO SHADER"));
	y+=8;

	switch(mods->mode)
	{
	case MV_NONE:
		R_DrawTextField(r_refdef.grect.x, r_refdef.grect.y+y, r_refdef.grect.width, r_refdef.grect.height-y, 
			va("arrows: pitch/rotate\n"
			"w: zoom in\n"
			"s: zoom out\n"
			"m: mode\n"
			"r: reset times\n"
			"home: skin-=1\n"
			"end: skin+=1\n"
			"pgup: frame+=1\n"
			"pgdn: frame-=1\n"
			"mins: %g %g %g, maxs: %g %g %g\n", ent.model->mins[0], ent.model->mins[1], ent.model->mins[2], ent.model->maxs[0], ent.model->maxs[1], ent.model->maxs[2])
			, CON_WHITEMASK, CPRINT_TALIGN|CPRINT_LALIGN, font_default, fs);
		break;
	case MV_BONES:
#ifdef SKELETALMODELS
		{
			int bonecount;
			galiasbone_t *b = Mod_GetBoneInfo(ent.model, &bonecount);
			if (b && bonecount)
			{
				Draw_FunString(0, y, va("Bones: "));
				y+=8;
				M_BoneDisplay(&ent, b, &y, 0, -1, 0, bonecount);
			}
			else
				R_DrawTextField(r_refdef.grect.x, r_refdef.grect.y+y, r_refdef.grect.width, r_refdef.grect.height-y, "No bones in model", CON_WHITEMASK, CPRINT_TALIGN|CPRINT_LALIGN, font_default, fs);
		}
#endif
		break;
	case MV_SHADER:
		{
			if (!mods->shadertext)
			{
				char *body = Shader_GetShaderBody(Mod_ShaderForSkin(ent.model, mods->surfaceidx, mods->skingroup), mods->shaderfile, sizeof(mods->shaderfile));
				if (!body)
					break;
				if (*mods->shaderfile)
					mods->shadertext = Z_StrDup(va("\n\nPress space to view+edit the shader\n\n%s", body));
				else
					mods->shadertext = Z_StrDup(body);
			}
			R_DrawTextField(r_refdef.grect.x, r_refdef.grect.y+16, r_refdef.grect.width, r_refdef.grect.height-16, mods->shadertext, CON_WHITEMASK, CPRINT_TALIGN|CPRINT_LALIGN, font_default, fs);

			//fixme: draw the shader's textures.
		}
		break;
	case MV_TEXTURE:
		{
			shader_t *shader = Mod_ShaderForSkin(ent.model, mods->surfaceidx, mods->skingroup);
			if (shader)
			{
				texnums_t *skin = shader->defaulttextures;
				shader = R_RegisterShader("modelviewertexturepreview", SUF_2D, "{\nprogram default2d\n{\nmap $diffuse\n}\n}\n");
				shader->defaulttextures->base = skin->base;
				R2D_Image(0, y, shader->defaulttextures->base->width, shader->defaulttextures->base->height, 0, 0, 1, 1, shader);
			}
		}
		break;
	}
}
static qboolean M_ModelViewerKey(struct menucustom_s *c, struct menu_s *m, int key, unsigned int unicode)
{
	modelview_t *mods = c->dptr;

	if (key == 'w')
	{
		mods->dist *= 0.9;
		if (mods->dist < 5)
			mods->dist = 5;
	}
	else if (key == 's')
		mods->dist /= 0.9;
	else if (key == 'm')
	{
		Z_Free(mods->shadertext);
		mods->shadertext = NULL;
		switch (mods->mode)
		{
		case MV_NONE:	mods->mode = MV_BONES;	break; 
		case MV_BONES:	mods->mode = MV_SHADER;	break;
		case MV_SHADER:	mods->mode = MV_TEXTURE;break;
		case MV_TEXTURE: mods->mode = MV_NONE;	break;
		}
	}
	else if (key == 'r')
	{
		mods->framechangetime = realtime;
		mods->skinchangetime = realtime;
	}
	else if (key == K_UPARROW)
		mods->pitch += 5;
	else if (key == K_DOWNARROW)
		mods->pitch -= 5;
	else if (key == K_LEFTARROW)
		mods->yaw -= 5;
	else if (key == K_RIGHTARROW)
		mods->yaw += 5;
	else if (key == K_HOME)
	{
		mods->skingroup = max(0, mods->skingroup-1);
		mods->skinchangetime = realtime;
		Z_Free(mods->shadertext);
		mods->shadertext = NULL;
	}
	else if (key == K_END)
	{
		mods->skingroup += 1;
		mods->skinchangetime = realtime;
		Z_Free(mods->shadertext);
		mods->shadertext = NULL;
	}
	else if (key == K_PGDN)
	{
		mods->framegroup = max(0, mods->framegroup-1);
		mods->framechangetime = realtime;
		Z_Free(mods->shadertext);
		mods->shadertext = NULL;
	}
	else if (key == K_PGUP)
	{
		mods->framegroup += 1;
		mods->framechangetime = realtime;
	}
	else if (key == K_INS)
	{
		mods->surfaceidx = max(0, mods->surfaceidx-1);
		Z_Free(mods->shadertext);
		mods->shadertext = NULL;
	}
	else if (key == K_DEL)
	{
		mods->surfaceidx += 1;
		Z_Free(mods->shadertext);
		mods->shadertext = NULL;
	}
	else if (key == K_SPACE)
	{
		if (*mods->shaderfile)
			Cbuf_AddText(va("\nedit %s\n", mods->shaderfile), RESTRICT_LOCAL);
		return true;
	}
	else
		return false;
	return true;
}

void M_Menu_ModelViewer_f(void)
{
	modelview_t *mv;
	menucustom_t *c;
	menu_t *menu;
		
	Key_Dest_Add(kdm_emenu);

	menu = M_CreateMenu(sizeof(*mv));
	mv = menu->data;
	c = MC_AddCustom(menu, 64, 32, mv, 0);
	menu->cursoritem = (menuoption_t*)c;
	c->draw = M_ModelViewerDraw;
	c->key = M_ModelViewerKey;

	mv->yaw = 180 + crandom()*45;
	mv->dist = 150;
	Q_strncpyz(mv->modelname, Cmd_Argv(1), sizeof(mv->modelname));
	Q_strncpyz(mv->forceshader, Cmd_Argv(2), sizeof(mv->forceshader));
}
#else
void M_Menu_ModelViewer_f(void)
{
	Con_Printf("modelviewer: not in this build\n");
}
#endif

typedef struct
{
	ftemanifest_t **manifests;
	size_t nummanifests;
	int y;
} modmenu_t;

static void Mods_Draw(int x, int y, struct menucustom_s *c, struct menu_s *m)
{
	modmenu_t *mods = c->dptr;
	int i, ym;
	c->common.height = vid.height - y;
	ym = y+c->common.height;

	mods->y = y;

	if (!mods->nummanifests)
	{
		Draw_FunString(x, y+0, "No games or mods known");
		Draw_FunString(x, y+8, "You may need to use -basedir $PATHTOGAME on the commandline");
		return;
	}

	for (i = 0; y+8 <= ym && i < mods->nummanifests; y+=8, i++)
	{
		if (mousecursor_y >= y && mousecursor_y < y+8)
			Draw_AltFunString(x, y, mods->manifests[i]->formalname);
		else
			Draw_FunString(x, y, mods->manifests[i]->formalname);
	}
}
static qboolean Mods_Key(struct menucustom_s *c, struct menu_s *m, int key, unsigned int unicode)
{
	modmenu_t *mods = c->dptr;
	int i;
	ftemanifest_t *man;
	if (key == K_MOUSE1)
	{
		qboolean wasgameless = !*FS_GetGamedir(false);
		i = (mousecursor_y - mods->y)/8;
		if (i < 0 || i > mods->nummanifests)
			return false;
		man = mods->manifests[i];
		mods->manifests[i] = NULL;	//make sure the manifest survives the menu being closed.
		M_RemoveMenu(m);
		FS_ChangeGame(man, true, true);

		if (wasgameless && !!*FS_GetGamedir(false))
		{
			//starting to a blank state generally means that the current(engine-default) config settings are utterly useless and windowed by default.
			//so generally when switching to a *real* game, we want to restart video just so things like fullscreen etc are saved+used properly.
			Cbuf_AddText("\nvid_restart\n", RESTRICT_LOCAL);
		}
		else
		{
			//if we're already running a game, this should probably just be a vid_reload instead to ensure that the conback etc is reloaded.
			//(a full restart is annoying)
			Cbuf_AddText("\nvid_reload\n", RESTRICT_LOCAL);
		}
		return true;
	}

	return false;
}
static void Mods_Remove	(struct menu_s *m)
{
	modmenu_t *mods = m->data;
	int i;

	for (i = 0; i < mods->nummanifests; i++)
	{
		if (mods->manifests[i])
			FS_Manifest_Free(mods->manifests[i]);
	}
	Z_Free(mods->manifests);
	mods->manifests = NULL;
}

static qboolean Mods_AddMod(void *usr, ftemanifest_t *man)
{
	modmenu_t *mods = usr;
	int i = mods->nummanifests;
	mods->manifests = BZ_Realloc(mods->manifests, (i+1) * sizeof(*mods->manifests));
	mods->manifests[i] = man;
	mods->nummanifests = i+1;
	return true;
}

#include "fs.h"

void M_Menu_Mods_f (void)
{
	modmenu_t mods;
	menucustom_t *c;
	menu_t *menu;

	memset(&mods, 0, sizeof(mods));
	FS_EnumerateKnownGames(Mods_AddMod, &mods);

	Key_Dest_Add(kdm_emenu);

	menu = M_CreateMenu(sizeof(modmenu_t));
	*(modmenu_t*)menu->data = mods;
	if (COM_FCheckExists("gfx/p_option.lmp"))
	{
		MC_AddPicture(menu, 16, 4, 32, 144, "gfx/qplaque.lmp");
		MC_AddCenterPicture(menu, 0, 24, "gfx/p_option.lmp");
	}

	c = MC_AddCustom(menu, 64, 32, menu->data, 0);
	menu->cursoritem = (menuoption_t*)c;
	c->draw = Mods_Draw;
	c->key = Mods_Key;
	menu->remove = Mods_Remove;
}

#if 0
extern ftemanifest_t	*fs_manifest;	//currently active manifest.
struct installermenudata
{
	menuedit_t *syspath;
};
static void Installer_Remove	(struct menu_s *m)
{
	Cbuf_AddText("quit force\n", RESTRICT_LOCAL);
}


void FS_CreateBasedir(const char *path);
#include <process.h>
static qboolean Installer_Go(menuoption_t *opt, menu_t *menu, int key)
{
	struct installermenudata *md = menu->data;
	
	if (key == K_MOUSE1 || key == K_ENTER)
	{
		extern int startuppending;
		vfsfile_t *f;
		char path[MAX_OSPATH];
		char exepath[MAX_OSPATH];
		char newexepath[MAX_OSPATH];

		Q_snprintfz(path, sizeof(path), "%s/", md->syspath->text);

		Con_Printf("path %s\n", path);

		menu->remove = NULL;
		M_RemoveMenu(menu);

		FS_CreateBasedir(path);

#ifdef _WIN32
		GetModuleFileName(NULL, exepath, sizeof(exepath));
		FS_NativePath(va("%s.exe", fs_manifest->installation), FS_ROOT, newexepath, sizeof(newexepath));
		CopyFile(exepath, newexepath, FALSE);

//		SetHookState(false);
		Host_Shutdown ();
//		CloseHandle (qwclsemaphore);
//		SetHookState(false);
//		_execv(newexepath, host_parms.argv);
		{
			PROCESS_INFORMATION childinfo;
			STARTUPINFO startinfo = {sizeof(startinfo)};
			CreateProcess(newexepath, va("\"%s\" +sys_register_file_associations %s", newexepath, COM_Parse(GetCommandLineA())), NULL, NULL, FALSE, 0, NULL, path, &startinfo, &childinfo);
		}
		exit(1);
#elif 0
#ifdef __linux__
		if (readlink("/proc/self/exe", exepath, sizeof(exepath)-1) <= 0)
#endif
			Q_strncpyz(exepath, host_parms.argv[0], sizeof(exepath));

		int fd = creat(newexepath, S_IRWXU | S_IRGRP|S_IXGRP);
		write(fd);
		close(fd);
#endif

		startuppending = 2;
		return TRUE;
	}

	return FALSE;
}

void M_Menu_Installer(void)
{
	menu_t *menu;
	struct installermenudata *md;

	Key_Dest_Add(kdm_menu);

	menu = M_CreateMenu(sizeof(*md));
	md = menu->data = (menu+1);

	md->syspath = MC_AddEdit(menu, 0, 160, 64, "Path", "C:/Games/AfterQuake/testinstall/base");//va("c:/%s", fs_manifest->installation));

	//FIXME: add path check display

	MC_AddCommand		(menu, 0, 160, 128, "Install", Installer_Go);
	MC_AddConsoleCommand(menu, 0, 160, 136,	"Cancel", "menu_quit\n");

	menu->selecteditem = (menuoption_t*)md->syspath;
	menu->cursoritem = (menuoption_t*)MC_AddWhiteText(menu, 250, 0, menu->selecteditem->common.posy, NULL, false);
	menu->remove = Installer_Remove;
}
#endif
#endif