//read menu.h

#include "quakedef.h"
#include "winquake.h"

//these are awkward/strange
qboolean M_Options_AlwaysRun (union menuoption_s *option, chk_set_t set)
{
	if (set == CHK_CHECKED)
		return cl_forwardspeed.value > 200;
	else if (cl_forwardspeed.value > 200)
	{
		Cvar_SetValue (&cl_forwardspeed, 200);
		Cvar_SetValue (&cl_backspeed, 200);
		return false;
	}
	else
	{
		Cvar_SetValue (&cl_forwardspeed, 400);
		Cvar_SetValue (&cl_backspeed, 400);
		return true;
	}	
}
qboolean M_Options_InvertMouse (union menuoption_s *option, chk_set_t set)
{
	if (set == CHK_CHECKED)
		return m_pitch.value < 0;
	else
	{
		Cvar_SetValue (&m_pitch, -m_pitch.value);
		return m_pitch.value < 0;
	}	
}

//options menu.
void M_Menu_Options_f (void)
{
	int q1, q2, h2;
	extern cvar_t cl_standardchat;
#ifdef _WIN32
	extern qboolean vid_isfullscreen;
#endif
	menu_t *menu;
	int y = 32;

	key_dest = key_menu;
	m_state = m_complex;
	m_entersound = true;

	menu = M_CreateMenu(0);

	q1 = COM_FDepthFile("gfx/menudot1.lmp", true);
	h2 = COM_FDepthFile("gfx/menu/menudot1.lmp", true);
	q2 = COM_FDepthFile("pics/m_cursor1.lmp", true);
	if (q2 < h2 && q2 < q1)
	{	//q2...
	}
	else if (h2 < q1)
	{	//h2
		MC_AddPicture(menu, 16, 0, "gfx/menu/hplaque.lmp");
		MC_AddCenterPicture(menu, 0, "gfx/menu/title3.lmp");
		y+=32;
	}
	else
	{	//q1
		MC_AddPicture(menu, 16, 4, "gfx/qplaque.lmp");
		MC_AddCenterPicture(menu, 4, "gfx/p_option.lmp");
	}





	menu->selecteditem = (union menuoption_s *)
	MC_AddConsoleCommand(menu, 16, y,	"    Customize controls", "menu_keys\n"); y+=8;
	MC_AddConsoleCommand(menu, 16, y,	"         Go to console", "toggleconsole\nplay misc/menu2.wav\n"); y+=8;
	MC_AddConsoleCommand(menu, 16, y,	"     Reset to defaults", "exec default.cfg\nplay misc/menu2.wav\n"); y+=8;

	MC_AddSlider(menu, 16, y,			"           Mouse Speed", &sensitivity,		1,		10); y+=8;

	MC_AddCheckBox(menu, 16, y,		"            Always Run", NULL,0)->func = M_Options_AlwaysRun; y+=8;
	MC_AddCheckBox(menu, 16, y,		"          Invert Mouse", NULL,0)->func = M_Options_InvertMouse; y+=8;
	MC_AddCheckBox(menu, 16, y,		"            Lookspring", &lookspring,0); y+=8;
	MC_AddCheckBox(menu, 16, y,		"            Lookstrafe", &lookstrafe,0); y+=8;
	MC_AddCheckBox(menu, 16, y,		"    Use old status bar", &cl_sbar,0); y+=8;
	MC_AddCheckBox(menu, 16, y,		"      HUD on left side", &cl_hudswap,0); y+=8;
	MC_AddCheckBox(menu, 16, y,		"    Old-style chatting", &cl_standardchat,0);y+=8;
	y+=4;MC_AddEditCvar(menu, 16, y,		"           Imitate FPS", "cl_netfps"); y+=8+4;

	MC_AddConsoleCommand(menu, 16, y,	"         Video Options", "menu_video\n"); y+=8;
	MC_AddConsoleCommand(menu, 16, y,	"           FPS Options", "menu_fps\n"); y+=8;
	MC_AddConsoleCommand(menu, 16, y,	"         Audio Options", "menu_audio\n"); y+=8;

#ifdef _WIN32
	if (!vid_isfullscreen)	
#endif
	{
		MC_AddCheckBox(menu, 16, y,	"             Use Mouse", &_windowed_mouse,0); y+=8;
	}

	menu->cursoritem = (menuoption_t*)MC_AddWhiteText(menu, 200, 32, NULL, false);
}

#ifndef __CYGWIN__
typedef struct {
	int cursorpos;
	menuoption_t *cursoritem;

	menutext_t *speaker[6];
	menutext_t *testsoundsource;

	soundcardinfo_t *card;
} audiomenuinfo_t;

qboolean M_Audio_Key (int key, struct menu_s *menu)
{
	int i, x, y;
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
		x = info->testsoundsource->common.posx - 320/2;
		y = info->testsoundsource->common.posy - 200/2;
		sc->yaw[i] = (-atan2 (y,x)*180/M_PI) - 90;

		sc->dist[i] = 50/sqrt(x*x+y*y);
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
		info->speaker[i]->common.posx = 320/2 - sin(sc->yaw[i]*M_PI/180) * 50/sc->dist[i];
		info->speaker[i]->common.posy = 200/2 - cos(sc->yaw[i]*M_PI/180) * 50/sc->dist[i];
	}
	for (; i < 6; i++)
		info->speaker[i]->common.posy = -100;

	if (lasttime+0.5 < Sys_DoubleTime())
	{
		lasttime = Sys_DoubleTime();
		org[0] = listener_origin[0] + 2*(listener_right[0]*(info->testsoundsource->common.posx-320/2) + listener_forward[0]*(info->testsoundsource->common.posy-200/2));
		org[1] = listener_origin[1] + 2*(listener_right[1]*(info->testsoundsource->common.posx-320/2) + listener_forward[1]*(info->testsoundsource->common.posy-200/2));
		org[2] = listener_origin[2] + 2*(listener_right[2]*(info->testsoundsource->common.posx-320/2) + listener_forward[2]*(info->testsoundsource->common.posy-200/2));
		S_StartSound(-2, 0, S_PrecacheSound("player/pain3.wav"), org, 1, 4);
	}
}

void M_Menu_Audio_Speakers_f (void)
{
	int i;
	audiomenuinfo_t *info;
	menu_t *menu;

	key_dest = key_menu;
	m_state = m_complex;
	m_entersound = true;

	menu = M_CreateMenu(sizeof(audiomenuinfo_t));
	info = menu->data;
	menu->key = M_Audio_Key;
	menu->event = M_Audio_StartSound;

	for (i = 0; i < 6; i++)
		info->speaker[i] = MC_AddBufferedText(menu, 0, 0, va("%i", i), false, true);

	info->testsoundsource = MC_AddBufferedText(menu, 0, 0, "X", false, true);

	info->card = sndcardinfo;


	menu->selecteditem = NULL;
}

menucombo_t *MC_AddCvarCombo(menu_t *menu, int x, int y, const char *caption, cvar_t *cvar, const char **ops, const char **values);
void M_Menu_Audio_f (void)
{
	int y = 32;
	menu_t *menu;
	extern cvar_t nosound, precache, snd_leftisright, snd_khz, snd_eax, snd_speakers, ambient_level;

	static const char *soundqualityoptions[] = {
		"11025 Hz",
		"22050 Hz",
		"44100 Hz",
		NULL
	};

	static const char *soundqualityvalues[] = {
		"11.025",
		"22.050",
		"44.100",
		NULL
	};

	static const char *speakeroptions[] = {
		"Mono",
		"Stereo",
		"Quad",
		"5.1 surround",
		NULL
	};

	static const char *speakervalues[] = {
		"1",
		"2",
		"4",
		"6",
		NULL
	};

	key_dest = key_menu;
	m_state = m_complex;
	m_entersound = true;

	menu = M_CreateMenu(0);

	MC_AddPicture(menu, 16, 4, "gfx/qplaque.lmp");
	MC_AddCenterPicture(menu, 4, "gfx/p_option.lmp");

		



	menu->selecteditem = (union menuoption_s *)


	MC_AddSlider(menu, 16, y,			"       CD Music Volume", &bgmvolume,		0,		1);y+=8;
	MC_AddSlider(menu, 16, y,			"          Sound Volume", &volume,			0,		1);y+=8;
	MC_AddSlider(menu, 16, y,			"        Ambient Volume", &ambient_level,	0,		1);y+=8;
	MC_AddCheckBox(menu, 16, y,			"              no sound", &nosound,0);y+=8;
	MC_AddCheckBox(menu, 16, y,			"              precache", &precache,0);y+=8;
	MC_AddCheckBox(menu, 16, y,			"     Low Quality Sound", &loadas8bit,0);y+=8;
	MC_AddCheckBox(menu, 16, y,			"            Flip Sound", &snd_leftisright,0);y+=8;
	MC_AddCheckBox(menu, 16, y,			"    Experimental EAX 2", &snd_eax,0);y+=8;
	MC_AddCvarCombo(menu, 16, y,		"         Speaker setup", &snd_speakers, speakeroptions, speakervalues);y+=8;
	MC_AddCvarCombo(menu, 16, y,		"           Sound speed", &snd_khz, soundqualityoptions, soundqualityvalues);y+=8;
	MC_AddConsoleCommand(menu, 16, y,	"         Restart sound", "snd_restart\n");y+=8;


	menu->cursoritem = (menuoption_t*)MC_AddWhiteText(menu, 200, 32, NULL, false);
}

#else
void M_Menu_Audio_f (void)
{
	Con_Printf("No sound in cygwin\n");
}
#endif



void M_Menu_Particles_f (void)
{
	int y = 32;
	menu_t *menu;
	extern cvar_t r_bouncysparks, r_part_rain, gl_part_torch, gl_part_flame, gl_part_trifansparks;

	static const char *r_part_rain_options[] = {
		"off",
		"on",
		"bouncy",
		NULL
	};
	static const char *r_part_rain_values[] = {
		"off",
		"on",
		"bouncy",
		NULL
	};

	static const char *gl_part_effects_ops[] = {
		"models",
		"particles",
		"off",
		NULL
	};
	static const char *gl_part_effects_vals[] = {
		"0",
		"1",
		"-1",
		NULL
	};


	key_dest = key_menu;
	m_state = m_complex;
	m_entersound = true;

	menu = M_CreateMenu(0);

	MC_AddPicture(menu, 16, 4, "gfx/qplaque.lmp");
	MC_AddCenterPicture(menu, 4, "gfx/p_option.lmp");


	menu->selecteditem = (union menuoption_s *)

	MC_AddCheckBox(menu, 16, y,			"         sparks bounce", &r_bouncysparks,0);y+=8;
//	MC_AddSlider(menu, 16, y,			"       exp spark count", &r_particles_in_explosion, 16, 1024);y+=8;
	MC_AddCvarCombo(menu, 16, y,		"           rain effect", &r_part_rain, r_part_rain_options, r_part_rain_values);y+=8;

#ifdef RGLQUAKE
	if (qrenderer == QR_OPENGL)	//sw doesn't have these.
	{
		MC_AddCvarCombo(menu, 16, y,		"      WallTorch effect", &gl_part_torch, gl_part_effects_ops, gl_part_effects_vals);y+=8;
		MC_AddCvarCombo(menu, 16, y,		"     Open flame effect", &gl_part_flame, gl_part_effects_ops, gl_part_effects_vals);y+=8;
		MC_AddCheckBox(menu, 16, y,			"         Trifan Sparks", &gl_part_trifansparks,0);y+=8;
	}
#endif


	menu->cursoritem = (menuoption_t*)MC_AddWhiteText(menu, 200, 32, NULL, false);
}


void M_Menu_FPS_f (void)
{
	int y = 32;
	menu_t *menu;
#ifdef RGLQUAKE
	extern cvar_t gl_compress, gl_waterripples, gl_detail, gl_bump, r_flashblend;
#endif
#ifdef SWQUAKE
	extern cvar_t d_smooth, d_mipscale, d_mipcap;
#endif
	extern cvar_t r_stains, r_bloodstains, r_loadlits, r_dynamic, v_contentblend, show_fps;

	key_dest = key_menu;
	m_state = m_complex;
	m_entersound = true;

	menu = M_CreateMenu(0);

	MC_AddPicture(menu, 16, 4, "gfx/qplaque.lmp");
	MC_AddCenterPicture(menu, 4, "gfx/p_option.lmp");

		



	menu->selecteditem = (union menuoption_s *)

	MC_AddConsoleCommand(menu, 48, y,		"  Particle Options", "menu_particles\n"); y+=8;

	MC_AddCheckBox(menu, 48, y,				"          Show FPS", &show_fps,0);y+=8;

	MC_AddCheckBox(menu, 48, y,				"     Content blend", &v_contentblend,0);y+=8;
	MC_AddCheckBox(menu, 48, y,				"    Dynamic lights", &r_dynamic,0);y+=8;
	MC_AddCheckBox(menu, 48, y,			    "         Stainmaps", &r_stains,0);y+=8;

	switch(qrenderer)
	{
#ifdef RGLQUAKE
	case QR_OPENGL:
		MC_AddCheckBox(menu, 48, y,			"      Blood stains", &r_bloodstains,0);y+=8;
		MC_AddCheckBox(menu, 48, y,			"   Load .lit files", &r_loadlits,0);y+=8;
		MC_AddCheckBox(menu, 48, y,			"     Flashblending", &r_flashblend,0);y+=8;
		MC_AddCheckBox(menu, 48, y,			"        Detailmaps", &gl_detail,0);y+=8;
		MC_AddCheckBox(menu, 48, y,			"          Bumpmaps", &gl_bump,0);y+=8;
		MC_AddCheckBox(menu, 48, y,			"   Tex Compression", &gl_compress,0);y+=8;
		MC_AddCheckBox(menu, 48, y,			"Other Water Effect", &gl_waterripples,0);y+=8;
		MC_AddCheckBox(menu, 48, y,			"   32 bit textures", &gl_load24bit,0);y+=8;
		break;
#endif
#ifdef SWQUAKE
	case QR_SOFTWARE:
		if (r_pixbytes == 4)
		{MC_AddCheckBox(menu, 48, y,		"   Load .lit files", &r_loadlits,0);y+=8;}
		MC_AddCheckBox(menu, 48, y,			" Texture Smoothing", &d_smooth,0);y+=8;
		MC_AddSlider(menu, 48, y,			"      Mipmap scale", &d_mipscale,		0.1,	3);y+=8;
		MC_AddSlider(menu, 48, y,			"    Mipmap Capping", &d_mipcap,		0,		3);y+=8;
		break;
#endif
	default:
		break;
	}

	menu->cursoritem = (menuoption_t*)MC_AddWhiteText(menu, 200, 32, NULL, false);
}
