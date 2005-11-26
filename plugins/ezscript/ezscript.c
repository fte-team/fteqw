// GPL'd, really needed?


#include "../plugin.h"
//#include <time.h>
//#include <ctype.h>

/*void ezScript_InitCvars(void)
{
	vmcvar_t *v;
	int i;

	for (v = cvarlist[0],i=0; i < sizeof(cvarlist)/sizeof(cvarlist[0]); v++, i++)
		v->handle = Cvar_Register(v->name, v->string, v->flags, v->group);
}*/

/*int ezScript_CvarUpdate(void) // perhaps void instead?
{
	vmcvar_t *v;
	int i;
	for (v = cvarlist[0],i=0; i < sizeof(cvarlist)/sizeof(cvarlist[0]); v++, i++)
		v->modificationcount = Cvar_Update(v->handle, v->modificationcount, v->string, &v->value);
	return 0;
}*/

int Plug_ExecuteCommand(int *args)
{
	char cmd[256];
	char param[256];
	Cmd_Argv(0, cmd, sizeof(cmd));
	Cmd_Argv(1, param, sizeof(param));

	if ( (!strcmp("loadsky", cmd)) || (!strcmp("r_skyname", cmd)) )
	{
		char cvar[20] = "r_skybox";

		Cvar_SetString(cvar,param);
		Con_Printf("-------------------------------------\n^7ezScript: ^1%s^7 is a ^3Fuh/ez/Z/More quakeworld ^7cvar, sending '^6%s^7' to ^2%s^7\n-------------------------------------\n",cmd,param,cvar);
		return 1;
	}
	else if ( (!strcmp("r_skycolor", cmd)) || (!strcmp("fps_skycolor", cmd)) )
	{
		char cvar[20] = "r_fastskycolour"; // note the england spelling, spike is a englander

		Cvar_SetString(cvar,param);
		Con_Printf("-------------------------------------\n^7ezScript: ^1%s^7 is a ^3Fuh/ez/Z/More quakeworld ^7cvar, sending '^6%s^7' to ^2%s^7\n-------------------------------------\n",cmd,param,cvar);
		return 1;
	}
	else if (!strcmp("fps_sky", cmd))
	{
		char cvar[20] = "r_fastsky";

		Cvar_SetString(cvar,param);
		Con_Printf("-------------------------------------\n^7ezScript: ^1%s^7 is a ^3Fuh/ez/Z/More quakeworld ^7cvar, sending '^6%s^7' to ^2%s^7\n-------------------------------------\n",cmd,param,cvar);
		return 1;
	}
	else if (!strcmp("gl_consolefont", cmd))
	{
		char cvar[20] = "gl_font";

		Cvar_SetString(cvar,param);
		Con_Printf("-------------------------------------\n^7ezScript: ^1%s^7 is a ^3Fuh/ez/Z/More quakeworld ^7cvar, sending '^6%s^7' to ^2%s^7\n-------------------------------------\n",cmd,param,cvar);
		return 1;
	}
	else if (!strcmp("gl_bounceparticles", cmd))
	{
		char cvar[20] = "r_bouncysparks";

		Cvar_SetString(cvar,param);
		Con_Printf("-------------------------------------\n^7ezScript: ^1%s^7 is a ^3Fuh/ez/Z/More quakeworld ^7cvar, sending '^6%s^7' to ^2%s^7\n-------------------------------------\n",cmd,param,cvar);
		return 1;
	}
	else if (!strcmp("gl_loadlitfiles", cmd))
	{
		char cvar[20] = "r_loadlit";

		Cvar_SetString(cvar,param);
		Con_Printf("-------------------------------------\n^7ezScript: ^1%s^7 is a ^3Fuh/ez/Z/More quakeworld ^7cvar, sending '^6%s^7' to ^2%s^7\n-------------------------------------\n",cmd,param,cvar);
		return 1;
	}
	else if (!strcmp("gl_weather_rain", cmd))
	{
		char cvar[20] = "r_part_rain";

		Cvar_SetString(cvar,param);
		Con_Printf("-------------------------------------\n^7ezScript: ^1%s^7 is a ^3Fuh/ez/Z/More quakeworld ^7cvar, sending '^6%s^7' to ^2%s^7\n-------------------------------------\n",cmd,param,cvar);
		return 1;
	}
	else if (!strcmp("cl_bonusflash", cmd))
	{
		char cvar[20] = "v_bonusflash";

		Cvar_SetString(cvar,param);
		Con_Printf("-------------------------------------\n^7ezScript: ^1%s^7 is a ^3Fuh/ez/Z/More quakeworld ^7cvar, sending '^6%s^7' to ^2%s^7\n-------------------------------------\n",cmd,param,cvar);
		return 1;
	}
	else if ( (!strcmp("gl_gamma", cmd)) || (!strcmp("sw_gamma", cmd)) )
	{
		char cvar[20] = "gamma";

		Cvar_SetString(cvar,param);
		Con_Printf("-------------------------------------\n^7ezScript: ^1%s^7 is a ^3Fuh/ez/Z/More quakeworld ^7cvar, sending '^6%s^7' to ^2%s^7\n-------------------------------------\n",cmd,param,cvar);
		return 1;
	}
	else if ( (!strcmp("gl_contrast", cmd)) || (!strcmp("sw_contrast", cmd)) )
	{
		char cvar[20] = "gamma";

		Cvar_SetString(cvar,param);
		Con_Printf("-------------------------------------\n^7ezScript: ^1%s^7 is a ^3Fuh/ez/Z/More quakeworld ^7cvar, sending '^6%s^7' to ^2%s^7\n-------------------------------------\n",cmd,param,cvar);
		return 1;
	}
	else if (!strcmp("s_khz", cmd))
	{
		char cvar[20] = "snd_khz";

		Cvar_SetString(cvar,param);
		Con_Printf("-------------------------------------\n^7ezScript: ^1%s^7 is a ^3Fuh/ez/Z/More quakeworld ^7cvar, sending '^6%s^7' to ^2%s^7\n-------------------------------------\n",cmd,param,cvar);
		return 1;
	}
	else if (!strcmp("s_loadas8bit", cmd))
	{
		char cvar[20] = "loadas8bit";

		Cvar_SetString(cvar,param);
		Con_Printf("-------------------------------------\n^7ezScript: ^1%s^7 is a ^3Fuh/ez/Z/More quakeworld ^7cvar, sending '^6%s^7' to ^2%s^7\n-------------------------------------\n",cmd,param,cvar);
		return 1;
	}
	else if (!strcmp("s_mixahead", cmd))
	{
		char cvar[20] = "_snd_mixahead";

		Cvar_SetString(cvar,param);
		Con_Printf("-------------------------------------\n^7ezScript: ^1%s^7 is a ^3Fuh/ez/Z/More quakeworld ^7cvar, sending '^6%s^7' to ^2%s^7\n-------------------------------------\n",cmd,param,cvar);
		return 1;
	}
	else if (!strcmp("s_noextraupdate", cmd))
	{
		char cvar[20] = "snd_noextraupdate";

		Cvar_SetString(cvar,param);
		Con_Printf("-------------------------------------\n^7ezScript: ^1%s^7 is a ^3Fuh/ez/Z/More quakeworld ^7cvar, sending '^6%s^7' to ^2%s^7\n-------------------------------------\n",cmd,param,cvar);
		return 1;
	}
	else if (!strcmp("s_nosound", cmd))
	{
		char cvar[20] = "nosound";

		Cvar_SetString(cvar,param);
		Con_Printf("-------------------------------------\n^7ezScript: ^1%s^7 is a ^3Fuh/ez/Z/More quakeworld ^7cvar, sending '^6%s^7' to ^2%s^7\n-------------------------------------\n",cmd,param,cvar);
		return 1;
	}
	else if (!strcmp("s_precache", cmd))
	{
		char cvar[20] = "precache";

		Cvar_SetString(cvar,param);
		Con_Printf("-------------------------------------\n^7ezScript: ^1%s^7 is a ^3Fuh/ez/Z/More quakeworld ^7cvar, sending '^6%s^7' to ^2%s^7\n-------------------------------------\n",cmd,param,cvar);
		return 1;
	}
	else if (!strcmp("s_show", cmd))
	{
		char cvar[20] = "snd_show";

		Cvar_SetString(cvar,param);
		Con_Printf("-------------------------------------\n^7ezScript: ^1%s^7 is a ^3Fuh/ez/Z/More quakeworld ^7cvar, sending '^6%s^7' to ^2%s^7\n-------------------------------------\n",cmd,param,cvar);
		return 1;
	}
	else if (!strcmp("s_swapstereo", cmd))
	{
		char cvar[20] = "snd_leftisright";

		Cvar_SetString(cvar,param);
		Con_Printf("-------------------------------------\n^7ezScript: ^1%s^7 is a ^3Fuh/ez/Z/More quakeworld ^7cvar, sending '^6%s^7' to ^2%s^7\n-------------------------------------\n",cmd,param,cvar);
		return 1;
	}
	else if (!strcmp("s_ambientlevel", cmd))
	{
		char cvar[20] = "ambient_level";

		Cvar_SetString(cvar,param);
		Con_Printf("-------------------------------------\n^7ezScript: ^1%s^7 is a ^3Fuh/ez/Z/More quakeworld ^7cvar, sending '^6%s^7' to ^2%s^7\n-------------------------------------\n",cmd,param,cvar);
		return 1;
	}
	else if (!strcmp("s_ambientfade", cmd))
	{
		char cvar[20] = "ambient_fade";

		Cvar_SetString(cvar,param);
		Con_Printf("-------------------------------------\n^7ezScript: ^1%s^7 is a ^3Fuh/ez/Z/More quakeworld ^7cvar, sending '^6%s^7' to ^2%s^7\n-------------------------------------\n",cmd,param,cvar);
		return 1;
	}
	else if (!strcmp("tp_triggers", cmd))
	{
		char cvar[20] = "cl_triggers";

		Cvar_SetString(cvar,param);
		Con_Printf("-------------------------------------\n^7ezScript: ^1%s^7 is a ^3Fuh/ez/Z/More quakeworld ^7cvar, sending '^6%s^7' to ^2%s^7\n-------------------------------------\n",cmd,param,cvar);
		return 1;
	}
	else if (!strcmp("teamskin", cmd))
	{
		char cvar[20] = "cl_teamskin";

		Cvar_SetString(cvar,param);
		Con_Printf("-------------------------------------\n^7ezScript: ^1%s^7 is a ^3Fuh/ez/Z/More quakeworld ^7cvar, sending '^6%s^7' to ^2%s^7\n-------------------------------------\n",cmd,param,cvar);
		return 1;
	}
	else if (!strcmp("enemyskin", cmd))
	{
		char cvar[20] = "cl_enemyskin";

		Cvar_SetString(cvar,param);
		Con_Printf("-------------------------------------\n^7ezScript: ^1%s^7 is a ^3Fuh/ez/Z/More quakeworld ^7cvar, sending '^6%s^7' to ^2%s^7\n-------------------------------------\n",cmd,param,cvar);
		return 1;
	}
	else if (!strcmp("scr_consize", cmd))
	{		char cvar[20] = "con_height";

		Cvar_SetString(cvar,param);
		Con_Printf("-------------------------------------\n^7ezScript: ^1%s^7 is a ^3Fuh/ez/Z/More quakeworld ^7cvar, sending '^6%s^7' to ^2%s^7\n-------------------------------------\n",cmd,param,cvar);
		return 1;
	}
	else if (!strcmp("cl_predictPlayers", cmd)) // why capital P i dont know
	{
		char cvar[20] = "cl_predict_players"; //lets not forget there is a cl_predict_players2

		Cvar_SetString(cvar,param);
		Con_Printf("-------------------------------------\n^7ezScript: ^1%s^7 is a ^3Fuh/ez/Z/More quakeworld ^7cvar, sending '^6%s^7' to ^2%s^7\n-------------------------------------\n",cmd,param,cvar);
		return 1;
	}
	else if (!strcmp("sshot_format", cmd))
	{
		char cvar[20] = "scr_sshot_type";

		Cvar_SetString(cvar,param);
		Con_Printf("-------------------------------------\n^7ezScript: ^1%s^7 is a ^3Fuh/ez/Z/More quakeworld ^7cvar, sending '^6%s^7' to ^2%s^7\n-------------------------------------\n",cmd,param,cvar);
		return 1;
	}
	else if (!strcmp("cl_solidPlayers", cmd))
	{
		char cvar[20] = "cl_solid_players";

		Cvar_SetString(cvar,param);
		Con_Printf("-------------------------------------\n^7ezScript: ^1%s^7 is a ^3Fuh/ez/Z/More quakeworld ^7cvar, sending '^6%s^7' to ^2%s^7\n-------------------------------------\n",cmd,param,cvar);
		return 1;
	}
	else if (!strcmp("fps_muzzleflash", cmd))
	{
		char cvar[20] = "cl_muzzleflash";

		Cvar_SetString(cvar,param);
		Con_Printf("-------------------------------------\n^7ezScript: ^1%s^7 is a ^3Fuh/ez/Z/More quakeworld ^7cvar, sending '^6%s^7' to ^2%s^7\n-------------------------------------\n",cmd,param,cvar);
		return 1;
	}

	return 0;
}

void ezScript_InitCommands(void) // not really needed actually
{
	//skyboxes
	Cmd_AddCommand("loadsky");
	Cmd_AddCommand("r_skyname");
	Cmd_AddCommand("r_skycolor");
	Cmd_AddCommand("fps_sky");
	Cmd_AddCommand("fps_skycolor");
	//gl stuff
	Cmd_AddCommand("gl_consolefont");
	Cmd_AddCommand("gl_bounceparticles");
	Cmd_AddCommand("gl_loadlitfiles");
	Cmd_AddCommand("gl_weather_rain");
	//misc
	Cmd_AddCommand("cl_bonusflash");
	//gamma
	Cmd_AddCommand("gl_gamma");
	Cmd_AddCommand("sw_gamma");
	Cmd_AddCommand("gl_contrast");
	Cmd_AddCommand("sw_contrast");
	//sound
	Cmd_AddCommand("s_khz");
	Cmd_AddCommand("s_loadas8bit");
	Cmd_AddCommand("s_mixahead");
	Cmd_AddCommand("s_noextraupdate");
	Cmd_AddCommand("s_nosound");
	Cmd_AddCommand("s_precache");
	Cmd_AddCommand("s_show");
	Cmd_AddCommand("s_swapstereo");
	Cmd_AddCommand("s_ambientlevel");
	Cmd_AddCommand("s_ambientfade");
	//teamplay
	Cmd_AddCommand("tp_triggers");
	Cmd_AddCommand("teamskin");
	Cmd_AddCommand("enemyskin");
	//console
	Cmd_AddCommand("scr_consize");
	//misc
	Cmd_AddCommand("cl_predictPlayers");
	Cmd_AddCommand("sshot_format");
	Cmd_AddCommand("cl_solidPlayers");
	Cmd_AddCommand("fps_muzzleflash");
}

int Plug_Init(int *args)
{
	if (Plug_Export("ExecuteCommand", Plug_ExecuteCommand))
	{
		Con_Printf("ezScript Plugin Build 1 by Moodles Loaded\n");
	}
	else
	{
		Con_Printf("ezScript Plugin failed\n");
	}

	ezScript_InitCommands();
	return 1;
}