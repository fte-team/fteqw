/*
Copyright (C) 2011 Id Software, Inc.

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

#include "quakedef.h"

/*

  EXT_CSQC is the 'root' extension
  EXT_CSQC_1 are a collection of additional features to cover omissions in the original spec

  simplecsqc lacks CSQC_UpdateView and has CSQC_DrawHud+CSQC_DrawScores instead.
  if we're running arbitrary csqc, we block things that require too much game interaction...
*/

#ifdef CSQC_DAT

#include "glquake.h"	//evil to include this
#include "shader.h"

#include "pr_common.h"

extern usercmd_t cl_pendingcmd[MAX_SPLITS];


static pubprogfuncs_t *csqcprogs;

typedef struct csqctreadstate_s {
	float resumetime;
	struct qcthread_s *thread;
	int self;
	int other;

	struct csqctreadstate_s *next;
} csqctreadstate_t;

static qboolean csprogs_promiscuous;
static unsigned int csprogs_checksum;
static csqctreadstate_t *csqcthreads;
qboolean csqc_resortfrags;
world_t csqc_world;

int	csqc_playerseat;	//can be negative.
static playerview_t *csqc_playerview;
qboolean csqc_dp_lastwas3d;	//to emulate DP correctly, we need to track whether drawpic/drawfill or clearscene was called last. blame 515.
#ifdef NOLEGACY
#define csqc_isdarkplaces false	//hopefully this will allow a smart enough compiler to optimise it out cleanly
#else
static qboolean csqc_isdarkplaces;
#endif
static qboolean csqc_nogameaccess;	/*the module is not trusted by the server, so isn't allowed to access origins+stuffcmds+etc*/
static qboolean csqc_singlecheats;	/*single player or cheats active, allowing custom addons*/
static qboolean csqc_mayread;		/*csqc is allowed to ReadByte();*/
static qboolean csqc_worldchanged;	/*make sure any caches are rebuilt properly before the next renderscene*/

static char csqc_printbuffer[8192];

#define CSQCPROGSGROUP "CSQC progs control"
cvar_t	pr_csqc_maxedicts = CVAR("pr_csqc_maxedicts", "65536");	//not tied to protocol nor server. can be set arbitrarily high, except for memory allocations.
cvar_t	pr_csqc_memsize = CVAR("pr_csqc_memsize", "-1");
cvar_t	cl_csqcdebug = CVAR("cl_csqcdebug", "0");	//prints entity numbers which arrive (so I can tell people not to apply it to players...)
cvar_t  cl_nocsqc = CVAR("cl_nocsqc", "0");
cvar_t  pr_csqc_coreonerror = CVAR("pr_csqc_coreonerror", "1");
#if defined(NOBUILTINMENUS) && !defined(MENU_DAT)
cvar_t  pr_csqc_formenus = CVARF("pr_csqc_formenus", "1", CVAR_NOSET);
#else
cvar_t  pr_csqc_formenus = CVAR("pr_csqc_formenus", "0");
#endif
extern cvar_t dpcompat_stats;

// standard effect cvars/sounds
extern cvar_t r_explosionlight;
extern sfx_t			*cl_sfx_wizhit;
extern sfx_t			*cl_sfx_knighthit;
extern sfx_t			*cl_sfx_tink1;
extern sfx_t			*cl_sfx_ric1;
extern sfx_t			*cl_sfx_ric2;
extern sfx_t			*cl_sfx_ric3;
extern sfx_t			*cl_sfx_r_exp3;

#define ENDLIST	//clarifies \ in list macros.
#ifdef NOLEGACY
#define legacycsqcglobals
#else
#define legacycsqcglobals	\
	globalstring(trace_dphittexturename,	"trace_dphittexturename");			/*for dp compat*/	\
	globalfloat(trace_dpstartcontents,	"trace_dpstartcontents");			/*for dp compat*/	\
	globalfloat(trace_dphitcontents,	"trace_dphitcontents");				/*for dp compat*/	\
	globalfloat(trace_dphitq3surfaceflags,	"trace_dphitq3surfaceflags");	/*for dp compat*/	\
	globalfloat(trace_surfaceflagsf,	"trace_surfaceflagsf");				/*float		written by traceline, for mods that lack ints*/	\
	globalfloat(trace_endcontentsf,		"trace_endcontentsf");				/*float		written by traceline EXT_CSQC_1, for mods that lack ints*/	\
	ENDLIST
#endif

//If I do it like this, I'll never forget to register something...
#define csqcglobals	\
	globalfunction(init_function,		"CSQC_Init");	\
	globalfunction(worldloaded,			"CSQC_WorldLoaded");	\
	globalfunction(shutdown_function,	"CSQC_Shutdown");	\
	globalfunction(f_updateview,		"CSQC_UpdateView");	\
	globalfunction(f_updateviewloading,	"CSQC_UpdateViewLoading");	\
	globalfunction(f_drawhud,			"CSQC_DrawHud");/*simple csqc*/	\
	globalfunction(f_drawscores,		"CSQC_DrawScores");/*simple csqc*/	\
	globalfunction(parse_stuffcmd,		"CSQC_Parse_StuffCmd");	\
	globalfunction(parse_centerprint,	"CSQC_Parse_CenterPrint");	\
	globalfunction(parse_print,			"CSQC_Parse_Print");	\
	globalfunction(parse_event,			"CSQC_Parse_Event");	\
	globalfunction(parse_damage,		"CSQC_Parse_Damage");	\
	globalfunction(parse_setangles,		"CSQC_Parse_SetAngles");	\
	globalfunction(input_event,			"CSQC_InputEvent");	\
	globalfunction(input_frame,			"CSQC_Input_Frame");/*EXT_CSQC_1*/	\
	globalfunction(rendererrestarted,	"CSQC_RendererRestarted");	\
	globalfunction(console_command,		"CSQC_ConsoleCommand");	\
	globalfunction(console_link,		"CSQC_ConsoleLink");	\
	globalfunction(gamecommand,			"GameCommand");	/*DP extension*/\
	\
	globalfunction(ent_spawn,			"CSQC_Ent_Spawn");	\
	globalfunction(ent_update,			"CSQC_Ent_Update");	\
	globalfunction(ent_remove,			"CSQC_Ent_Remove");	\
	\
	globalfunction(event_sound,			"CSQC_Event_Sound");	\
	globalfunction(serversound,			"CSQC_ServerSound");/*obsolete, use event_sound*/	\
	/*globalfunction(loadresource,		"CSQC_LoadResource");*//*EXT_CSQC_1*/	\
	globalfunction(parse_tempentity,	"CSQC_Parse_TempEntity");/*EXT_CSQC_ABSOLUTLY_VILE*/	\
	\
	globalfunction(mapentityedited,		"CSQC_MapEntityEdited");\
	\
	/*These are pointers to the csqc's globals.*/	\
	globalfloat(simtime,				"time");				/*float		The simulation(aka: smoothed server) time, speed drifts based upon latency*/	\
	globalfloat(frametime,				"frametime");			/*float		Client render frame interval*/	\
	globalfloat(gamespeed,				"gamespeed");			/*float		Multiplier for real time -> simulation time*/	\
	globalfloat(cltime,					"cltime");				/*float		Clientside map uptime indepent of gamespeed, latency, and the server in general*/	\
	globalfloat(clframetime,			"clframetime");			/*float		time since last video frame*/	\
	globalfloat(netnewtime,				"servertime");			/*float		Server time of latest inbound network frame*/	\
	globalfloat(netoldtime,				"serverprevtime");		/*float		Server time of previous inbound network frame */	\
	globalfloat(netdeltatime,			"serverdeltatime");		/*float		new-old */	\
	globalfloat(physics_mode,			"physics_mode");		/*float		Written before entering most qc functions*/	\
	globalentity(self,					"self");				/*entity	Written before entering most qc functions*/	\
	globalentity(other,					"other");				/*entity	Written before entering most qc functions*/	\
	\
	globalentity(deathmatch,			"deathmatch");			/*for simplecsqc*/	\
	globalentity(coop,					"coop");				/*for simplecsqc*/	\
	\
	globalfloat(maxclients,				"maxclients");			/*float		max number of players allowed*/	\
	globalfloat(numclientseats,			"numclientseats");		/*float		number of seats/splitscreen clients running on this client*/	\
	\
	globalvector(forward,				"v_forward");			/*vector	written by anglevectors*/	\
	globalvector(right,					"v_right");				/*vector	written by anglevectors*/	\
	globalvector(up,					"v_up");				/*vector	written by anglevectors*/	\
	\
	globalfloat(trace_allsolid,			"trace_allsolid");		/*bool		written by traceline*/	\
	globalfloat(trace_startsolid,		"trace_startsolid");	/*bool		written by traceline*/	\
	globalfloat(trace_fraction,			"trace_fraction");		/*float		written by traceline*/	\
	globalfloat(trace_inwater,			"trace_inwater");		/*bool		written by traceline*/	\
	globalfloat(trace_inopen,			"trace_inopen");		/*bool		written by traceline*/	\
	globalvector(trace_endpos,			"trace_endpos");		/*vector	written by traceline*/	\
	globalvector(trace_plane_normal,	"trace_plane_normal");	/*vector	written by traceline*/	\
	globalfloat(trace_plane_dist,		"trace_plane_dist");	/*float		written by traceline*/	\
	globalentity(trace_ent,				"trace_ent");			/*entity	written by traceline*/	\
	globalint(trace_surfaceflagsi,		"trace_surfaceflagsi");	/*int		written by traceline*/	\
	globalstring(trace_surfacename,		"trace_surfacename");	/*string	written by traceline*/	\
	globalint(trace_endcontentsi,		"trace_endcontentsi");	/*int		written by traceline EXT_CSQC_1*/	\
	globalint(trace_brush_id,			"trace_brush_id");		/*int		written by traceline*/	\
	globalint(trace_brush_faceid,		"trace_brush_faceid");	/*int		written by traceline*/	\
	globalint(trace_surface_id,			"trace_surface_id");	/*int		written by traceline*/	\
	globalint(trace_bone_id,			"trace_bone_id");		/*int		written by traceline*/	\
	globalint(trace_triangle_id,		"trace_triangle_id");	/*int		written by traceline*/	\
	legacycsqcglobals \
	\
	globalfloat(clientcommandframe,		"clientcommandframe");	/*float		the next frame that will be sent*/ \
	globalfloat(servercommandframe,		"servercommandframe");	/*float		the most recent frame received from the server*/ \
	\
	globalfloat(player_localentnum,		"player_localentnum");	/*float		the entity number the local player is looking out from*/	\
	globalfloat(player_localnum,		"player_localnum");		/*float		the player number of the local player*/	\
	globalfloat(intermission,			"intermission");		/*float		set when the client receives svc_intermission*/	\
	globalfloat(intermission_time,		"intermission_time");	/*float		set when the client receives svc_intermission*/	\
	globalvector(view_angles,			"view_angles");			/*float		set to the view angles at the start of each new frame (EXT_CSQC_1)*/ \
	\
	globalvector(pmove_org,				"pmove_org");			/*deprecated. read/written by runplayerphysics*/ \
	globalvector(pmove_vel,				"pmove_vel");			/*deprecated. read/written by runplayerphysics*/ \
	globalvector(pmove_mins,			"pmove_mins");			/*deprecated. read/written by runplayerphysics*/ \
	globalvector(pmove_maxs,			"pmove_maxs");			/*deprecated. read/written by runplayerphysics*/ \
	globalfloat(pmove_jump_held,		"pmove_jump_held");		/*deprecated. read/written by runplayerphysics*/ \
	globalfloat(pmove_waterjumptime,	"pmove_waterjumptime");	/*deprecated. read/written by runplayerphysics*/ \
	globalfloat(pmove_onground,			"pmove_onground");		/*deprecated. read/written by runplayerphysics*/ \
	\
	globalfloat(input_timelength,		"input_timelength");	/*float		filled by getinputstate, read by runplayerphysics*/ \
	globalvector(input_angles,			"input_angles");		/*vector	filled by getinputstate, read by runplayerphysics*/ \
	globalvector(input_movevalues,		"input_movevalues");	/*vector	filled by getinputstate, read by runplayerphysics*/ \
	globalfloat(input_buttons,			"input_buttons");		/*float		filled by getinputstate, read by runplayerphysics*/ \
	globalfloat(input_impulse,			"input_impulse");		/*float		filled by getinputstate, read by runplayerphysics*/ \
	globalfloat(input_lightlevel,		"input_lightlevel");	/*unused float		filled by getinputstate, read by runplayerphysics*/ \
	globalfloat(input_weapon,			"input_weapon");		/*unused float		filled by getinputstate, read by runplayerphysics*/ \
	globalfloat(input_servertime,		"input_servertime");	/*float		filled by getinputstate, read by runplayerphysics*/ \
	globalfloat(input_clienttime,		"input_clienttime");	/*float		filled by getinputstate, read by runplayerphysics*/ \
	globalvector(input_cursor_screen,	"input_cursor_screen");	/*float		filled by getinputstate*/ \
	globalvector(input_cursor_start,	"input_cursor_trace_start");	/*float		filled by getinputstate*/ \
	globalvector(input_cursor_impact,	"input_cursor_trace_endpos");	/*float		filled by getinputstate*/ \
	globalfloat(input_cursor_entitynumber,	"input_cursor_entitynumber");	/*float		filled by getinputstate*/ \
	\
	globalvector(global_gravitydir,		"global_gravitydir");	/*vector	used when .gravitydir is 0 0 0 */ \
	globalfloat(dimension_default,		"dimension_default");	/*float		default value for dimension_hit+dimension_solid*/ \
	globalfloat(autocvar_vid_conwidth,	"autocvar_vid_conwidth");	/*float		hackfix for dp mods*/	\
	globalfloat(autocvar_vid_conheight,	"autocvar_vid_conheight");	/*float		hackfix for dp mods*/	\
	globalfloat(cycle_wrapped,			"cycle_wrapped");	\
	ENDLIST

typedef struct {
#define globalfloat(name,qcname) float *name
#define globalint(name,qcname) int *name
#define globalvector(name,qcname) float *name
#define globalentity(name,qcname) int *name
#define globalstring(name,qcname) string_t *name
#define globalfunction(name,qcname) func_t name
//These are the functions the engine will call to, found by name.

	csqcglobals

#undef globalfloat
#undef globalint
#undef globalvector
#undef globalentity
#undef globalstring
#undef globalfunction
} csqcglobals_t;
static csqcglobals_t csqcg;

playerview_t csqc_nullview;

static void VARGS CSQC_Abort (char *format, ...);	//an error occured.
static void cs_set_input_state (usercmd_t *cmd);

//fixme: we should be using entity numbers, not view numbers.
static void CSQC_ChangeLocalPlayer(int seat)
{
	if (seat < 0 || seat >= MAX_SPLITS)
	{
		csqc_playerseat = -1;
		csqc_playerview = &csqc_nullview;
	}
	else
	{
		csqc_playerseat = seat;
		csqc_playerview = &cl.playerview[seat];
	}
	if (csqcg.player_localentnum)
	{
		if (csqc_playerview->viewentity)
			*csqcg.player_localentnum = csqc_playerview->viewentity;
		else if (csqc_playerview->spectator && Cam_TrackNum(csqc_playerview) >= 0)
			*csqcg.player_localentnum = Cam_TrackNum(csqc_playerview) + 1;
		else if (csqc_playerview == &csqc_nullview)
			*csqcg.player_localentnum = 0;
		else
			*csqcg.player_localentnum = csqc_playerview->playernum+1;
	}
	if (csqcg.player_localnum)
		*csqcg.player_localnum = csqc_playerview->playernum;

	if (csqc_nogameaccess)
		return;	//don't give much info otherwise.

	if (csqcg.view_angles)
	{
		csqcg.view_angles[0] = csqc_playerview->viewangles[0];
		csqcg.view_angles[1] = csqc_playerview->viewangles[1];
		csqcg.view_angles[2] = csqc_playerview->viewangles[2];
	}
	if ((unsigned int)seat < MAX_SPLITS)
	{
//		int i;
		usercmd_t *cmd = &cl_pendingcmd[seat];
//		usercmd_t tmp;
//		for (i=0 ; i<3 ; i++)
//			cmd->angles[i] = ((int)(csqc_playerview->viewangles[i]*65536.0/360)&65535);
//		if (!cmd->msec)
//			CL_BaseMove (cmd, seat, cmd->msec, newtime);
//		tmp = *cmd;
//		cmd = &tmp;
//		cmd->msec = (realtime - cl.outframes[(cl.movesequence-1)&UPDATE_MASK].senttime)*1000;
		cs_set_input_state(cmd);

		if (csqcg.pmove_org)
			VectorCopy(csqc_playerview->simorg, csqcg.pmove_org);
		if (csqc_isdarkplaces)
		{	//dp mods tend to require these to be totally unlerped
			if (csqcg.pmove_vel)
				VectorCopy(cl.inframes[cl.validsequence&UPDATE_MASK].playerstate[csqc_playerview->playernum].velocity, csqcg.pmove_vel);
			if (csqcg.pmove_onground)
				*csqcg.pmove_onground = cl.inframes[cl.validsequence&UPDATE_MASK].playerstate[csqc_playerview->playernum].onground;
		}
		else
		{
			if (csqcg.pmove_vel)
				VectorCopy(csqc_playerview->simvel, csqcg.pmove_vel);
			if (csqcg.pmove_onground)
				*csqcg.pmove_onground = csqc_playerview->onground;
		}
	}
}

static void CSQC_FindGlobals(qboolean nofuncs)
{
	static eval_t junk;
	static float csphysicsmode = 0;
	static float dimension_default = 255;
	static vec3_t defaultgravity = {0, 0, -1};
#define globalfloat(name,qcname) csqcg.name = (float*)PR_FindGlobal(csqcprogs, qcname, 0, NULL);
#define globalint(name,qcname) csqcg.name = (int*)PR_FindGlobal(csqcprogs, qcname, 0, NULL);
#define globalvector(name,qcname) csqcg.name = (float*)PR_FindGlobal(csqcprogs, qcname, 0, NULL);
#define globalentity(name,qcname) csqcg.name = (int*)PR_FindGlobal(csqcprogs, qcname, 0, NULL);
#define globalstring(name,qcname) csqcg.name = (string_t*)PR_FindGlobal(csqcprogs, qcname, 0, NULL);
#define globalfunction(name,qcname) csqcg.name = nofuncs?0:PR_FindFunction(csqcprogs,qcname,PR_ANY);

	csqcglobals

#undef globalfloat
#undef globalint
#undef globalvector
#undef globalentity
#undef globalstring
#undef globalfunction

#define ensurefloat(name)	if (!csqcg.name) csqcg.name = &junk._float;
#define ensureint(name)		if (!csqcg.name) csqcg.name = &junk._int;
#define ensurevector(name)	if (!csqcg.name) csqcg.name = junk._vector;
#define ensureentity(name)	if (!csqcg.name) csqcg.name = &junk.edict;

	if (csqc_nogameaccess)
	{
		csqcg.f_updateview = 0;	//would fail
		csqcg.f_updateviewloading = 0;	//would fail
		csqcg.parse_stuffcmd = 0;	//could block cvar changes, thus allow cheats
		csqcg.parse_setangles = 0;	//too evil
		csqcg.input_frame = 0;	//no aimbot writing
		csqcg.event_sound = csqcg.serversound = 0; //no sound snooping
		csqcg.parse_tempentity = 0; //compat nightmare
		csqcg.view_angles = NULL;
		csqcg.physics_mode = NULL;	//no thinks stuff
		csqcg.pmove_org = NULL;	//can't make aimbots if you don't know where you're aiming from.
		csqcg.pmove_vel = NULL;	//no dead reckoning please
		csqcg.pmove_mins = csqcg.pmove_maxs = csqcg.pmove_jump_held = csqcg.pmove_waterjumptime = csqcg.pmove_onground = NULL; //I just want to kill theses
		csqcg.input_angles = csqcg.input_movevalues = csqcg.input_buttons = csqcg.input_impulse = csqcg.input_lightlevel = csqcg.input_weapon = csqcg.input_servertime = NULL;
		csqcg.input_clienttime = csqcg.input_cursor_screen = csqcg.input_cursor_start = csqcg.input_cursor_impact = csqcg.input_cursor_entitynumber = NULL;
	}
	else if (csqcg.f_updateview || csqcg.f_updateviewloading)
	{	//full csqc AND simplecsqc's entry points at the same time are a bad idea that just result in confusion.
		//full csqc mods should just disable engine hud drawing
		csqcg.f_drawhud = 0;
		csqcg.f_drawscores = 0;
	}

#ifdef NOLEGACY
	{
		etype_t etype = ev_void;
		if (!csqcg.trace_surfaceflagsi)
			csqcg.trace_surfaceflagsi = (int*)PR_FindGlobal(csqcprogs, "trace_surfaceflags", 0, &etype);
		if (!csqcg.trace_endcontentsi)
			csqcg.trace_endcontentsi = (int*)PR_FindGlobal(csqcprogs, "trace_endcontents", 0, &etype);
	}
#else
	if (!csqcg.trace_surfaceflagsf && !csqcg.trace_surfaceflagsi)
	{
		etype_t etype = ev_void;
		eval_t *v = PR_FindGlobal(csqcprogs, "trace_surfaceflags", 0, &etype);
		if (etype == ev_float)
			csqcg.trace_surfaceflagsf = &v->_float;
		else if (etype == ev_integer)
			csqcg.trace_surfaceflagsi = &v->_int;
	}
	if (!csqcg.trace_endcontentsf && !csqcg.trace_endcontentsi)
	{
		etype_t etype = ev_void;
		eval_t *v = PR_FindGlobal(csqcprogs, "trace_endcontents", 0, &etype);
		if (etype == ev_float)
			csqcg.trace_endcontentsf = &v->_float;
		else if (etype == ev_integer)
			csqcg.trace_endcontentsi = &v->_int;
	}
	ensurefloat(trace_surfaceflagsf);
	ensurefloat(trace_endcontentsf);
#endif

	ensurefloat(trace_allsolid);
	ensurefloat(trace_startsolid);
	ensurefloat(trace_fraction);
	ensurefloat(trace_inwater);
	ensurefloat(trace_inopen);
	ensurevector(trace_endpos);
	ensurevector(trace_plane_normal);
	ensurefloat(trace_plane_dist);
	ensureint(trace_surfaceflagsi);
	ensureint(trace_endcontentsi);
	ensureint(trace_brush_id);
	ensureint(trace_brush_faceid);
	ensureint(trace_surface_id);
	ensureint(trace_bone_id);
	ensureint(trace_triangle_id);
	ensureentity(trace_ent);


	if (csqcg.simtime)
		*csqcg.simtime = cl.servertime;
	if (csqcg.cltime)
		*csqcg.cltime = realtime;

	if (!csqcg.global_gravitydir)
		csqcg.global_gravitydir = defaultgravity;

	CSQC_ChangeLocalPlayer(cl_forceseat.ival?(cl_forceseat.ival - 1) % cl.splitclients:0);

	csqc_world.g.self = csqcg.self;
	csqc_world.g.other = csqcg.other;
	csqc_world.g.force_retouch = (float*)PR_FindGlobal(csqcprogs, "force_retouch", 0, NULL);
	csqc_world.g.physics_mode = csqcg.physics_mode;
	csqc_world.g.frametime = csqcg.frametime;
	csqc_world.g.newmis = (int*)PR_FindGlobal(csqcprogs, "newmis", 0, NULL);
	csqc_world.g.time = csqcg.simtime;
	csqc_world.g.v_forward = csqcg.forward;
	csqc_world.g.v_right = csqcg.right;
	csqc_world.g.v_up = csqcg.up;
	csqc_world.g.defaultgravitydir = csqcg.global_gravitydir;
	csqc_world.g.drawfont = (float*)PR_FindGlobal(csqcprogs, "drawfont", 0, NULL);
	csqc_world.g.drawfontscale = (float*)PR_FindGlobal(csqcprogs, "drawfontscale", 0, NULL);

	if (!csqc_world.g.physics_mode)
	{
		csphysicsmode = 0;	/*note: dp handles think functions as part of addentity rather than elsewhere. if we're in a compat mode, we don't want to have to duplicate work*/
		csqc_world.g.physics_mode = &csphysicsmode;
	}

	if (!csqcg.dimension_default)
		csqcg.dimension_default = &dimension_default;

	if (csqcg.maxclients)
		*csqcg.maxclients = cl.allocated_client_slots;
}



//note: doesn't even have to match the clprogs.dat :)
typedef struct {

#define comfieldfloat(csqcname,desc) float csqcname;
#define comfieldvector(csqcname,desc) vec3_t csqcname;
#define comfieldentity(csqcname,desc) int csqcname;
#define comfieldstring(csqcname,desc) string_t csqcname;
#define comfieldfunction(csqcname, typestr,desc) func_t csqcname;
comqcfields
#undef comfieldfloat
#undef comfieldvector
#undef comfieldentity
#undef comfieldstring
#undef comfieldfunction

#ifdef VM_Q1
} csqcentvars_t;
typedef struct {
#endif

#define comfieldfloat(name,desc) float name;
#define comfieldint(name,desc) int name;
#define comfieldvector(name,desc) vec3_t name;
#define comfieldentity(name,desc) int name;
#define comfieldstring(name,desc) string_t name;
#define comfieldfunction(name, typestr,desc) func_t name;
comextqcfields
csqcextfields
#undef comfieldfloat
#undef comfieldint
#undef comfieldvector
#undef comfieldentity
#undef comfieldstring
#undef comfieldfunction

#ifdef VM_Q1
} csqcextentvars_t;
#else
} csqcentvars_t;
#endif

typedef struct csqcedict_s
{
	enum ereftype_e	ereftype;
	float			freetime; // sv.time when the object was freed
	int				entnum;
	unsigned int	fieldsize;
	pbool			readonly;	//world
#ifdef VM_Q1
	csqcentvars_t	*v;
	csqcextentvars_t	*xv;
#else
	union {
		csqcentvars_t	*v;
		csqcentvars_t	*xv;
	};
#endif
	/*the above is shared with qclib*/
#ifdef USEAREAGRID
	areagridlink_t	gridareas[AREAGRIDPERENT];	//on overflow, use the inefficient overflow list.
	size_t			gridareasequence;	//used to avoid iterrating the same ent twice.
#else
	link_t	area;
#endif
	pvscache_t pvsinfo;
	int lastruntime;
	int solidsize;
#ifdef USERBE
	entityrbe_t rbe;
#endif
	/*the above is shared with ssqc*/

	//add whatever you wish here
	trailstate_t *trailstate;
	int		skinobject;
} csqcedict_t;


static void CSQC_InitFields(void)
{	//CHANGING THIS FUNCTION REQUIRES CHANGES TO csqcentvars_t
#define comfieldfloat(name,desc) PR_RegisterFieldVar(csqcprogs, ev_float, #name, (size_t)&((csqcentvars_t*)0)->name, -1);
#define comfieldint(name,desc) PR_RegisterFieldVar(csqcprogs, ev_integer, #name, (size_t)&((csqcentvars_t*)0)->name, -1);
#define comfieldvector(name,desc) PR_RegisterFieldVar(csqcprogs, ev_vector, #name, (size_t)&((csqcentvars_t*)0)->name, -1);
#define comfieldentity(name,desc) PR_RegisterFieldVar(csqcprogs, ev_entity, #name, (size_t)&((csqcentvars_t*)0)->name, -1);
#define comfieldstring(name,desc) PR_RegisterFieldVar(csqcprogs, ev_string, #name, (size_t)&((csqcentvars_t*)0)->name, -1);
#define comfieldfunction(name, typestr,desc) PR_RegisterFieldVar(csqcprogs, ev_function, #name, (size_t)&((csqcentvars_t*)0)->name, -1);
comqcfields
#undef comfieldfloat
#undef comfieldint
#undef comfieldvector
#undef comfieldentity
#undef comfieldstring
#undef comfieldfunction

#ifdef VM_Q1
#define comfieldfloat(name,desc) PR_RegisterFieldVar(csqcprogs, ev_float, #name, sizeof(csqcentvars_t) + (size_t)&((csqcextentvars_t*)0)->name, -1);
#define comfieldint(name,desc) PR_RegisterFieldVar(csqcprogs, ev_integer, #name, sizeof(csqcentvars_t) + (size_t)&((csqcextentvars_t*)0)->name, -1);
#define comfieldvector(name,desc) PR_RegisterFieldVar(csqcprogs, ev_vector, #name, sizeof(csqcentvars_t) + (size_t)&((csqcextentvars_t*)0)->name, -1);
#define comfieldentity(name,desc) PR_RegisterFieldVar(csqcprogs, ev_entity, #name, sizeof(csqcentvars_t) + (size_t)&((csqcextentvars_t*)0)->name, -1);
#define comfieldstring(name,desc) PR_RegisterFieldVar(csqcprogs, ev_string, #name, sizeof(csqcentvars_t) + (size_t)&((csqcextentvars_t*)0)->name, -1);
#define comfieldfunction(name, typestr,desc) PR_RegisterFieldVar(csqcprogs, ev_function, #name, sizeof(csqcentvars_t) + (size_t)&((csqcextentvars_t*)0)->name, -1);
#else
#define comfieldfloat(name,desc) PR_RegisterFieldVar(csqcprogs, ev_float, #name, (size_t)&((csqcentvars_t*)0)->name, -1);
#define comfieldint(name,desc) PR_RegisterFieldVar(csqcprogs, ev_integer, #name, (size_t)&((csqcentvars_t*)0)->name, -1);
#define comfieldvector(name,desc) PR_RegisterFieldVar(csqcprogs, ev_vector, #name, (size_t)&((csqcentvars_t*)0)->name, -1);
#define comfieldentity(name,desc) PR_RegisterFieldVar(csqcprogs, ev_entity, #name, (size_t)&((csqcentvars_t*)0)->name, -1);
#define comfieldstring(name,desc) PR_RegisterFieldVar(csqcprogs, ev_string, #name, (size_t)&((csqcentvars_t*)0)->name, -1);
#define comfieldfunction(name, typestr,desc) PR_RegisterFieldVar(csqcprogs, ev_function, #name, (size_t)&((csqcentvars_t*)0)->name, -1);
#endif
comextqcfields
csqcextfields
#undef fieldfloat
#undef fieldint
#undef fieldvector
#undef fieldentity
#undef fieldstring
#undef fieldfunction

	PR_RegisterFieldVar(csqcprogs, ev_void, NULL, pr_fixbrokenqccarrays.ival, -1);
}

static csqcedict_t **csqcent;
static int maxcsqcentities;

static int csqcentsize;

static const char *csqcmapentitydata;
static qboolean csqcmapentitydataloaded;

static unsigned int csqc_deprecated_warned;
#define csqc_deprecated(s) do {if (!csqc_deprecated_warned++){Con_Printf("csqc warning: %s\n", s); PR_StackTrace (prinst, false);}}while(0)

static model_t *CSQC_GetModelForIndex(int index);

static void CS_CheckVelocity(csqcedict_t *ent)
{
}









static void cs_getframestate(csqcedict_t *in, unsigned int rflags, framestate_t *out)
{
	//FTE_CSQC_HALFLIFE_MODELS
#ifdef HALFLIFEMODELS
	out->bonecontrols[0] = in->xv->bonecontrol1;
	out->bonecontrols[1] = in->xv->bonecontrol2;
	out->bonecontrols[2] = in->xv->bonecontrol3;
	out->bonecontrols[3] = in->xv->bonecontrol4;
	out->bonecontrols[4] = in->xv->bonecontrol5;
	out->g[FS_REG].subblendfrac = in->xv->subblendfrac;
	out->g[FS_REG].subblend2frac = in->xv->subblend2frac;
	out->g[FST_BASE].subblendfrac = in->xv->subblendfrac;
	out->g[FST_BASE].subblend2frac = in->xv->subblend2frac;
#endif

	//FTE_CSQC_BASEFRAME
	out->g[FST_BASE].endbone = in->xv->basebone;
	if (out->g[FST_BASE].endbone)
	{	//small optimisation.
		out->g[FST_BASE].endbone -= 1;

		out->g[FST_BASE].frame[0] = in->xv->baseframe;
		out->g[FST_BASE].frame[1] = in->xv->baseframe2;
		//out->g[FST_BASE].frame[2] = in->xv->baseframe3;
		//out->g[FST_BASE].frame[3] = in->xv->baseframe4;
		out->g[FST_BASE].lerpweight[1] = in->xv->baselerpfrac;
//		out->g[FST_BASE].lerpweight[2] = in->xv->baselerpfrac3;
//		out->g[FST_BASE].lerpweight[3] = in->xv->baselerpfrac4;
		out->g[FST_BASE].lerpweight[0] = 1-(out->g[FST_BASE].lerpweight[1]);
		if (rflags & CSQCRF_FRAMETIMESARESTARTTIMES)
		{
			out->g[FST_BASE].frametime[0] = *csqcg.simtime - in->xv->baseframe1time;
			out->g[FST_BASE].frametime[1] = *csqcg.simtime - in->xv->baseframe2time;
			//out->g[FST_BASE].frametime[2] = *csqcg.simtime - in->xv->baseframe3time;
			//out->g[FST_BASE].frametime[3] = *csqcg.simtime - in->xv->baseframe4time;
		}
		else
		{
			out->g[FST_BASE].frametime[0] = in->xv->baseframe1time;
			out->g[FST_BASE].frametime[1] = in->xv->baseframe2time;
			//out->g[FST_BASE].frametime[2] = in->xv->baseframe3time;
			//out->g[FST_BASE].frametime[3] = in->xv->baseframe4time;
		}
	}

	//and the normal frames.
	out->g[FS_REG].endbone = 0x7fffffff;
	out->g[FS_REG].frame[0] = in->v->frame;
	out->g[FS_REG].frame[1] = in->xv->frame2;
	out->g[FS_REG].frame[2] = in->xv->frame3;
	out->g[FS_REG].frame[3] = in->xv->frame4;
	out->g[FS_REG].lerpweight[1] = in->xv->lerpfrac;
	out->g[FS_REG].lerpweight[2] = in->xv->lerpfrac3;
	out->g[FS_REG].lerpweight[3] = in->xv->lerpfrac4;
	out->g[FS_REG].lerpweight[0] = 1-(out->g[FS_REG].lerpweight[1]+out->g[FS_REG].lerpweight[2]+out->g[FS_REG].lerpweight[3]);
	if ((rflags & CSQCRF_FRAMETIMESARESTARTTIMES) || csqc_isdarkplaces)
	{
		out->g[FS_REG].frametime[0] = *csqcg.simtime - in->xv->frame1time;
		out->g[FS_REG].frametime[1] = *csqcg.simtime - in->xv->frame2time;
		out->g[FS_REG].frametime[2] = 0;//*csqcg.simtime - in->xv->frame3time;
		out->g[FS_REG].frametime[3] = 0;//*csqcg.simtime - in->xv->frame4time;
	}
	else
	{
		out->g[FS_REG].frametime[0] = in->xv->frame1time;
		out->g[FS_REG].frametime[1] = in->xv->frame2time;
		out->g[FS_REG].frametime[2] = 0;//in->xv->frame3time;
		out->g[FS_REG].frametime[3] = 0;//in->xv->frame4time;
	}


#if defined(SKELETALOBJECTS) || defined(RAGDOLL)
	out->bonecount = 0;
	out->bonestate = NULL;
	if (in->xv->skeletonindex)
		skel_lookup(&csqc_world, in->xv->skeletonindex, out);
#endif
}


static void QCBUILTIN PF_cs_remove (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	csqcedict_t *ed;

	ed = (csqcedict_t*)G_EDICT(prinst, OFS_PARM0);

	if (ED_ISFREE(ed))
	{
		csqc_deprecated("Tried removing free entity");
		return;
	}

	if (!ed->entnum)
	{
		Con_Printf("Unable to remove the world. Try godmode.");
		PR_StackTrace (prinst, false);
		return;
	}
	if (ed->readonly)
	{
		Con_Printf("Entity %i is readonly.", ed->entnum);
		return;
	}

	pe->DelinkTrailstate(&ed->trailstate);
	World_UnlinkEdict((wedict_t*)ed);
	ED_Free (prinst, (void*)ed);
}

static void QCBUILTIN PF_cvar (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	cvar_t	*var;
	const char	*str;

	str = PR_GetStringOfs(prinst, OFS_PARM0);

	if (!strcmp(str, "vid_conwidth"))
	{
		csqc_deprecated("vid_conwidth cvar used");
		G_FLOAT(OFS_RETURN) = vid.width;
	}
	else if (!strcmp(str, "vid_conheight"))
	{
		csqc_deprecated("vid_conheight cvar used");
		G_FLOAT(OFS_RETURN) = vid.height;
	}
	else
	{
		var = Cvar_Get(str, "", 0, "csqc cvars");
		if (var && !(var->flags & CVAR_NOUNSAFEEXPAND))
			G_FLOAT(OFS_RETURN) = var->value;
		else
			G_FLOAT(OFS_RETURN) = 0;
	}
}

//too specific to the prinst's builtins.
static void QCBUILTIN PF_Fixme (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	int binum;
	char fname[MAX_QPATH];
	if (!prinst->GetBuiltinCallInfo(prinst, &binum, fname, sizeof(fname)))
	{
		binum = 0;
		strcpy(fname, "?unknown?");
	}

	Con_Printf("\n");

	prinst->RunError(prinst, "\nBuiltin %i:%s not implemented.\nCSQC is not compatible.", binum, fname);
	PR_BIError (prinst, "bulitin not implemented");
}
static void QCBUILTIN PF_NoCSQC (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	int binum;
	char fname[MAX_QPATH];
	if (!prinst->GetBuiltinCallInfo(prinst, &binum, fname, sizeof(fname)))
	{
		binum = 0;
		strcpy(fname, "?unknown?");
	}

	Con_Printf("\n");

	prinst->RunError(prinst, "\nBuiltin %i:%s does not make sense in csqc.\nCSQC is not compatible.", binum, fname);
	PR_BIError (prinst, "bulitin not implemented");
}
static void QCBUILTIN PF_checkbuiltin (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	func_t funcref = G_INT(OFS_PARM0);
	char *funcname = NULL;
	int args;
	int builtinno;
	if (prinst->GetFunctionInfo(prinst, funcref, &args, &builtinno, funcname, sizeof(funcname)))
	{	//qc defines the function at least. nothing weird there...
		if (builtinno > 0 && builtinno < prinst->parms->numglobalbuiltins)
		{
			if (!prinst->parms->globalbuiltins[builtinno] || prinst->parms->globalbuiltins[builtinno] == PF_Fixme || prinst->parms->globalbuiltins[builtinno] == PF_NoCSQC)
				G_FLOAT(OFS_RETURN) = false;	//the builtin with that number isn't defined.
			else
			{
				G_FLOAT(OFS_RETURN) = true;		//its defined, within the sane range, mapped, everything. all looks good.
				//we should probably go through the available builtins and validate that the qc's name matches what would be expected
				//this is really intended more for builtins defined as #0 though, in such cases, mismatched assumptions are impossible.
			}
		}
		else
			G_FLOAT(OFS_RETURN) = false;	//not a valid builtin (#0 builtins get remapped according to the function name)
	}
	else
	{	//not valid somehow.
		G_FLOAT(OFS_RETURN) = false;
	}
}

static void QCBUILTIN PF_cl_cprint (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	const char *str = PF_VarString(prinst, 0, pr_globals);
	if (csqc_playerseat >= 0)
		SCR_CenterPrint(csqc_playerseat, str, true);
}

static void QCBUILTIN PF_cs_makevectors (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	if (!csqcg.forward || !csqcg.right || !csqcg.up)
		Host_EndGame("PF_makevectors: one of v_forward, v_right or v_up was not defined\n");
	AngleVectors (G_VECTOR(OFS_PARM0), csqcg.forward, csqcg.right, csqcg.up);
}

static model_t *CSQC_GetModelForIndex(int index)
{
	if (index == 0)
		return NULL;
	else if (index > 0 && index < MAX_PRECACHE_MODELS)
		return cl.model_precache[index];
	else if (index < 0 && index > -MAX_CSMODELS)
	{
		if (!cl.model_csqcprecache[-index])
			cl.model_csqcprecache[-index] = Mod_ForName(cl.model_csqcname[-index], MLV_WARN);
		return cl.model_csqcprecache[-index];
	}
	else
		return NULL;
}

static float CSQC_PitchScaleForModelIndex(int index)
{
	model_t *mod = CSQC_GetModelForIndex(index);
	if (mod && (mod->type == mod_alias || mod->type == mod_halflife))
		return r_meshpitch.value;	//these are buggy.
	return 1;
}

#ifdef SKELETALOBJECTS
wedict_t *skel_gettaginfo_args (pubprogfuncs_t *prinst, vec3_t axis[3], vec3_t origin, int tagent, int tagnum);
#endif

static qboolean CopyCSQCEdictToEntity(csqcedict_t *in, entity_t *out)
{
	int ival;
	model_t *model;
	unsigned int rflags;
	unsigned int effects;

	ival = in->v->modelindex;
	model = CSQC_GetModelForIndex(ival);
	if (!model)
		return false; //there might be other ent types later as an extension that stop this.

	memset(out, 0, sizeof(*out));
	out->model = model;

	rflags = in->xv->renderflags;
	if (csqc_isdarkplaces)
		rflags ^= CSQCRF_FRAMETIMESARESTARTTIMES;
	if (rflags)
	{
		rflags = in->xv->renderflags;
		if (rflags & CSQCRF_VIEWMODEL)
			out->flags |= RF_DEPTHHACK|RF_WEAPONMODEL;
		if (rflags & CSQCRF_EXTERNALMODEL)
			out->flags |= RF_EXTERNALMODEL;
		if (rflags & CSQCRF_DEPTHHACK)
			out->flags |= RF_DEPTHHACK;
		if (rflags & CSQCRF_ADDITIVE)
			out->flags |= RF_ADDITIVE;
		//CSQCRF_USEAXIS is below
		if (rflags & CSQCRF_NOSHADOW)
			out->flags |= RF_NOSHADOW;
		//CSQCRF_FRAMETIMESARESTARTTIMES is handled by cs_getframestate below

		if (rflags & CSQCRF_REMOVED)
			Con_Printf("Warning: CSQCRF_NOAUTOADD is no longer supported\n");
	}

	effects = in->v->effects;
	if (effects & NQEF_ADDITIVE)
		out->flags |= RF_ADDITIVE;
	if (effects & EF_NOSHADOW)
		out->flags |= RF_NOSHADOW;
	if (effects & EF_NODEPTHTEST)
		out->flags |= RF_NODEPTHTEST;
	if ((effects & DPEF_NOGUNBOB) && (out->flags & RF_WEAPONMODEL))
		out->flags |= RF_WEAPONMODELNOBOB;

	cs_getframestate(in, rflags, &out->framestate);

	VectorCopy(in->v->origin, out->origin);
	VectorCopy(in->v->oldorigin, out->oldorigin);
	if (in->v->enemy)
	{
		csqcedict_t *ed = (csqcedict_t*)PROG_TO_EDICT(csqcprogs, in->v->enemy);
		VectorSubtract(out->oldorigin, ed->v->oldorigin, out->oldorigin);
	}

	if (rflags & CSQCRF_USEAXIS)
	{
		VectorCopy(csqcg.forward, out->axis[0]);
		VectorNegate(csqcg.right, out->axis[1]);
		VectorCopy(csqcg.up, out->axis[2]);
		out->scale = 1;
	}
	else
	{
		VectorCopy(in->v->angles, out->angles);
		if (model && model->type == mod_alias)
			out->angles[0] *= r_meshpitch.value;
		AngleVectors(out->angles, out->axis[0], out->axis[1], out->axis[2]);
		VectorInverse(out->axis[1]);

		if (!in->xv->scale)
			out->scale = 1;
		else
			out->scale = in->xv->scale;
	}

	if (csqc_isdarkplaces && in->xv->tag_entity)
	{
#ifdef SKELETALOBJECTS
		csqcedict_t *p = (csqcedict_t*)skel_gettaginfo_args(csqcprogs, out->axis, out->origin, in->xv->tag_entity, in->xv->tag_index);
		if (p && (int)p->xv->renderflags & CSQCRF_VIEWMODEL)
			out->flags |= RF_DEPTHHACK|RF_WEAPONMODEL;
#endif
	}

	ival = in->v->colormap;
	out->playerindex = -1;
	if (ival > 0 && ival <= MAX_CLIENTS)
	{
		out->playerindex = ival - 1;
		out->topcolour = cl.players[ival-1].dtopcolor;
		out->bottomcolour = cl.players[ival-1].dbottomcolor;
	}
	else if (ival /*>= 1024*/)
	{
		//DP COLORMAP extension
		out->topcolour = (ival>>4) & 0x0f;
		out->bottomcolour = ival & 0xf;
	}
	else
	{
		out->topcolour = TOP_DEFAULT;
		out->bottomcolour = BOTTOM_DEFAULT;
	}

	if (!in->xv->colormod[0] && !in->xv->colormod[1] && !in->xv->colormod[2])
		VectorSet(out->shaderRGBAf, 1, 1, 1);
	else
	{
		out->flags |= RF_FORCECOLOURMOD;
		VectorCopy(in->xv->colormod, out->shaderRGBAf);
	}
	if (!in->xv->alpha || in->xv->alpha == 1)
		out->shaderRGBAf[3] = 1.0f;
	else
	{
		out->flags |= RF_TRANSLUCENT;
		out->shaderRGBAf[3] = in->xv->alpha;
	}

	if (!in->xv->glowmod[0] && !in->xv->glowmod[1] && !in->xv->glowmod[2])
		VectorSet(out->glowmod, 1, 1, 1);
	else
		VectorCopy(in->xv->glowmod, out->glowmod);

#ifdef HEXEN2
	out->drawflags = in->xv->drawflags;
	out->abslight = in->xv->abslight;
#endif
	out->skinnum = in->v->skin;
	out->fatness = in->xv->fatness;
	ival = in->xv->forceshader;
	if (ival >= 1 && ival <= r_numshaders)
		out->forcedshader = r_shaders[(ival-1)];
	else
		out->forcedshader = NULL;
	out->customskin = (in->skinobject<0)?-in->skinobject:in->skinobject;

	if (in->xv->entnum && !in->xv->camera_transform)	//yes, camera_transform is this hacky
		out->keynum = in->xv->entnum;
	else
		out->keynum = -in->entnum;

	return true;
}

static void QCBUILTIN PF_cs_makestatic (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{	//still does a remove.
	csqcedict_t *in = (void*)G_EDICT(prinst, OFS_PARM0);
	entity_t *ent;

	if (cl.num_statics == cl_max_static_entities)
	{
		cl_max_static_entities += 16;
		cl_static_entities = BZ_Realloc(cl_static_entities, sizeof(*cl_static_entities) * cl_max_static_entities);
	}

	ent = &cl_static_entities[cl.num_statics].ent;
	if (CopyCSQCEdictToEntity(in, ent))
	{
		entity_state_t *state = &cl_static_entities[cl.num_statics].state;
		memset(state, 0, sizeof(*state));
		cl_static_entities[cl.num_statics].emit = NULL;
		cl_static_entities[cl.num_statics].mdlidx = in->v->modelindex;
		if (cl.worldmodel && cl.worldmodel->funcs.FindTouchedLeafs)
			cl.worldmodel->funcs.FindTouchedLeafs(cl.worldmodel, &cl_static_entities[cl.num_statics].pvscache, in->v->absmin, in->v->absmax);
		else
			memset(&cl_static_entities[cl.num_statics].pvscache, 0, sizeof(cl_static_entities[cl.num_statics].pvscache));
		cl.num_statics++;

		//rtlights kinda need all this junk
		VectorCopy(ent->origin, state->origin);
		VectorCopy(ent->angles, state->angles);
		state->modelindex = in->v->modelindex;
		state->light[3] = in->xv->light_lev;
		VectorCopy(in->xv->color, state->light);
		state->lightpflags = in->xv->pflags;
		state->lightstyle = in->xv->style;
		state->skinnum = in->v->skin;
	}

	PF_cs_remove(prinst, pr_globals);
}

static void QCBUILTIN PF_R_AddEntity(pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	csqcedict_t *in = (void*)G_EDICT(prinst, OFS_PARM0);
	entity_t ent;
	if (ED_ISFREE(in) || in->entnum == 0)
	{
		csqc_deprecated("Tried drawing a free/removed/world entity\n");
		return;
	}

	if (CopyCSQCEdictToEntity(in, &ent))
	{
		CLQ1_AddShadow(&ent);
		V_AddAxisEntity(&ent);
	}
}
static void QCBUILTIN PF_R_RemoveEntity(pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	csqcedict_t *in = (void*)G_EDICT(prinst, OFS_PARM0);
	int keynum, i;
	if (ED_ISFREE(in) || in->entnum == 0)
	{
		csqc_deprecated("Tried drawing a free/removed/world entity\n");
		return;
	}

	//work out the internal key that relates to the given ent. we'll remove all ents with the same key.
	if (in->xv->entnum && !in->xv->camera_transform)	//yes, camera_transform is this hacky
		keynum = in->xv->entnum;
	else
		keynum = -in->entnum;

	for (i = 0; i < cl_numvisedicts; )
	{
		if (cl_visedicts[i].keynum == keynum)
		{
			cl_numvisedicts--;
			memmove(&cl_visedicts[i], &cl_visedicts[i+1], sizeof(*cl_visedicts)*(cl_numvisedicts-i));
		}
		else
			i++;
	}
}
void CL_AddDecal(shader_t *shader, vec3_t origin, vec3_t up, vec3_t side, vec3_t rgbvalue, float alphavalue);
static void QCBUILTIN PF_R_AddDecal(pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	shader_t *shader = R_RegisterShader(PR_GetStringOfs(prinst, OFS_PARM0), SUF_NONE, 
		"{\n"
			"polygonOffset\n"
			"surfaceparms nodlight\n"
			"{\n"
				"map $diffuse\n"
				"rgbgen vertex\n"
				"alphagen vertex\n"
				"blendfunc gl_one gl_one_minus_src_alpha\n"
			"}\n"
		"}\n");
	float *org = G_VECTOR(OFS_PARM1);
	float *up = G_VECTOR(OFS_PARM2);
	float *side = G_VECTOR(OFS_PARM3);
	float *rgb = G_VECTOR(OFS_PARM4);
	float alpha = G_FLOAT(OFS_PARM5);
	if (shader)
		CL_AddDecal(shader, org, up, side, rgb, alpha);
}

static void QCBUILTIN PF_R_DynamicLight_Set(pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	const char *s;
	dlight_t *l;
	unsigned int lno = G_FLOAT(OFS_PARM0);
	int field = G_FLOAT(OFS_PARM1);
	while (lno >= cl_maxdlights)
	{
		if (lno > 1000)
			return;
		CL_AllocSlight();
	}
	l = cl_dlights+lno;
	switch (field)
	{
	case lfield_origin:
		VectorCopy(G_VECTOR(OFS_PARM2), l->origin);
		l->rebuildcache = true;
		break;
	case lfield_colour:
		VectorCopy(G_VECTOR(OFS_PARM2), l->color);
		break;
	case lfield_radius:
		l->radius = G_FLOAT(OFS_PARM2);
		l->rebuildcache = true;
		if (lno >= rtlights_max)
			rtlights_max = lno+1;
		break;
	case lfield_flags:
		l->flags = G_FLOAT(OFS_PARM2);
		l->rebuildcache = true;
		break;
	case lfield_style:
		l->style = G_FLOAT(OFS_PARM2)+1;
		break;
	case lfield_angles:
		AngleVectors(G_VECTOR(OFS_PARM2), l->axis[0], l->axis[1], l->axis[2]);
		VectorInverse(l->axis[1]);
		break;
	case lfield_fov:
		l->fov = G_FLOAT(OFS_PARM2);
		break;
	case lfield_corona:
		l->corona = G_FLOAT(OFS_PARM2);
		break;
	case lfield_coronascale:
		l->coronascale = G_FLOAT(OFS_PARM2);
		break;
	case lfield_cubemapname:
		s = PR_GetStringOfs(prinst, OFS_PARM2);
		Q_strncpyz(l->cubemapname, s, sizeof(l->cubemapname));
		if (*l->cubemapname)
			l->cubetexture = R_LoadReplacementTexture(l->cubemapname, "", IF_CUBEMAP, NULL, 0, 0, TF_INVALID);
		else
			l->cubetexture = r_nulltex;
		break;
#ifdef RTLIGHTS
	case lfield_ambientscale:
		l->lightcolourscales[0] = G_FLOAT(OFS_PARM2);
		break;
	case lfield_diffusescale:
		l->lightcolourscales[1] = G_FLOAT(OFS_PARM2);
		break;
	case lfield_specularscale:
		l->lightcolourscales[2] = G_FLOAT(OFS_PARM2);
		break;
	case lfield_rotation:
		l->rotation[0] = G_FLOAT(OFS_PARM2+0);
		l->rotation[1] = G_FLOAT(OFS_PARM2+1);
		l->rotation[2] = G_FLOAT(OFS_PARM2+2);
		break;
#endif
	case lfield_dietime:
		l->die = G_FLOAT(OFS_PARM2);
		break;
	case lfield_rgbdecay:
		l->channelfade[0] = G_FLOAT(OFS_PARM2+0);
		l->channelfade[1] = G_FLOAT(OFS_PARM2+1);
		l->channelfade[2] = G_FLOAT(OFS_PARM2+2);
		break;
	case lfield_radiusdecay:
		l->decay = G_FLOAT(OFS_PARM2);
		break;
	default:
		break;
	}
}
static void QCBUILTIN PF_R_DynamicLight_Get(pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	vec3_t v;
	dlight_t *l;
	unsigned int lno = G_FLOAT(OFS_PARM0);
	enum lightfield_e field = G_FLOAT(OFS_PARM1);
	if (lno >= rtlights_max)
	{
		if ((int)field == -1)
			G_FLOAT(OFS_RETURN) = rtlights_max;
		else
			G_INT(OFS_RETURN) = 0;
		return;
	}
	l = cl_dlights+lno;
	switch (field)
	{
	case lfield_origin:
		VectorCopy(l->origin, G_VECTOR(OFS_RETURN));
		break;
	case lfield_colour:
		VectorCopy(l->color, G_VECTOR(OFS_RETURN));
		break;
	case lfield_radius:
		G_FLOAT(OFS_RETURN) = l->radius;
		break;
	case lfield_flags:
		G_FLOAT(OFS_RETURN) = l->flags;
		break;
	case lfield_style:
		G_FLOAT(OFS_RETURN) = l->style-1;
		break;
	case lfield_angles:
		VectorAngles(l->axis[0], l->axis[2], v, false);
		G_FLOAT(OFS_RETURN+0) = anglemod(v[0]);
		G_FLOAT(OFS_RETURN+1) = v[1];
		G_FLOAT(OFS_RETURN+2) = v[2];
		break;
	case lfield_fov:
		G_FLOAT(OFS_RETURN) = l->fov;
		break;
	case lfield_corona:
		G_FLOAT(OFS_RETURN) = l->corona;
		break;
	case lfield_coronascale:
		G_FLOAT(OFS_RETURN) = l->coronascale;
		break;
	case lfield_cubemapname:
		RETURN_TSTRING(l->cubemapname);
		break;
#ifdef RTLIGHTS
	case lfield_ambientscale:
		G_FLOAT(OFS_RETURN) = l->lightcolourscales[0];
		break;
	case lfield_diffusescale:
		G_FLOAT(OFS_RETURN) = l->lightcolourscales[1];
		break;
	case lfield_specularscale:
		G_FLOAT(OFS_RETURN) = l->lightcolourscales[2];
		break;
	case lfield_rotation:
		G_FLOAT(OFS_RETURN+0) = l->rotation[0];
		G_FLOAT(OFS_RETURN+1) = l->rotation[1];
		G_FLOAT(OFS_RETURN+2) = l->rotation[2];
		break;
#endif
	case lfield_dietime:
		G_FLOAT(OFS_RETURN) = l->die;
		break;
	case lfield_rgbdecay:
		G_FLOAT(OFS_RETURN+0) = l->channelfade[0];
		G_FLOAT(OFS_RETURN+1) = l->channelfade[1];
		G_FLOAT(OFS_RETURN+2) = l->channelfade[2];
		break;
	case lfield_radiusdecay:
		G_FLOAT(OFS_RETURN) = l->decay;
		break;
	default:
		G_INT(OFS_RETURN) = 0;
		break;
	}
}

static void PF_R_DynamicLight_AddInternal(pubprogfuncs_t *prinst, struct globalvars_s *pr_globals, qboolean isstatic)
{
	float *org = G_VECTOR(OFS_PARM0);
	float radius = G_FLOAT(OFS_PARM1);
	float *rgb = G_VECTOR(OFS_PARM2);
	float style = (prinst->callargc > 3)?G_FLOAT(OFS_PARM3):0;
	const char *cubemapname = (prinst->callargc > 4)?PR_GetStringOfs(prinst, OFS_PARM4):"";
	int pflags = (prinst->callargc > 5)?G_FLOAT(OFS_PARM5):PFLAGS_CORONA;

#ifdef RTLIGHTS
//	float *ambientdiffusespec = (prinst->callargc > 5)?{0,1,1}:G_VECTOR(OFS_PARM6);
//	fov, orientation, corona, coronascale, rotation, die, etc, just use dynamiclight_set
#endif

	wedict_t *self;
	dlight_t *dl;
	int dlkey;
	
	if (prinst == csqc_world.progs)
	{
		self = PROG_TO_WEDICT(prinst, *csqcg.self);
		dlkey = VectorCompare(self->v->origin, org)?-self->entnum:0;
	}
	else
		dlkey = 0;

	if (isstatic)
	{
		dl = CL_AllocSlight();
		dl->die = 0;
		dl->flags = LFLAG_NORMALMODE|LFLAG_REALTIMEMODE;
	}
	else
	{
		dl = CL_AllocDlight(dlkey);
		dl->die = cl.time - 0.1;
		dl->flags = LFLAG_DYNAMIC;
	}

	VectorCopy(org, dl->origin);
	dl->radius = radius;
	VectorCopy(rgb, dl->color);

	if (*dl->cubemapname)
	{
		VectorCopy(csqcg.forward,	dl->axis[0]);
		VectorCopy(csqcg.right,		dl->axis[1]);
		VectorCopy(csqcg.up,		dl->axis[2]);
	}

	if (pflags & PFLAGS_NOSHADOW)
		dl->flags |= LFLAG_NOSHADOWS;
	if (pflags & PFLAGS_CORONA)
		dl->corona = 1;
	else
		dl->corona = 0;
	dl->style = style;
	Q_strncpyz(dl->cubemapname, cubemapname, sizeof(dl->cubemapname));
	if (*dl->cubemapname)
		dl->cubetexture = R_LoadReplacementTexture(dl->cubemapname, "", IF_CUBEMAP, NULL, 0, 0, TF_INVALID);
	else
		dl->cubetexture = r_nulltex;

	G_FLOAT(OFS_RETURN) = dl - cl_dlights;
}

void QCBUILTIN PF_R_DynamicLight_AddStatic(pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	PF_R_DynamicLight_AddInternal(prinst, pr_globals, true);
}
void QCBUILTIN PF_R_DynamicLight_AddDynamic(pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	PF_R_DynamicLight_AddInternal(prinst, pr_globals, false);
}

static void QCBUILTIN PF_R_AddEntityMask(pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	int mask = G_FLOAT(OFS_PARM0);
	csqcedict_t *ent;
	entity_t rent;
	int e;
	int maxe;

	int oldself = *csqcg.self;
	RSpeedMark();

	if (cl.worldmodel)
	{
		if (mask & MASK_DELTA)
		{
#ifdef Q2CLIENT
			if (cls.protocol == CP_QUAKE2)
				CLQ2_AddEntities();
			else
#endif
			{
				CL_LinkPlayers ();
				CL_LinkPacketEntities ();
			}
		}
	}

	if (csqc_isdarkplaces)
	{
		//hopelessly inefficient version for compat with DP.
		maxe = *prinst->parms->sv_num_edicts;
		for (e=1; e < maxe; e++)
		{
			ent = (void*)EDICT_NUM_PB(prinst, e);
			if (ED_ISFREE(ent))
				continue;
			if (ent->v->think)
			{
				WPhys_RunThink (&csqc_world, (wedict_t*)ent);
				if (ED_ISFREE(ent))
					continue;
			}
			if (ent->xv->predraw)
			{
				*csqcg.self = EDICT_TO_PROG(prinst, (void*)ent);
				PR_ExecuteProgram(prinst, ent->xv->predraw);
				if (ED_ISFREE(ent))
					continue;	//bummer...
			}
			if ((int)ent->xv->drawmask & mask)
			{
				if (CopyCSQCEdictToEntity(ent, &rent))
				{
					CLQ1_AddShadow(&rent);
					V_AddAxisEntity(&rent);
				}
			}
		}
	}
	else
	{
		maxe = *prinst->parms->sv_num_edicts;
		for (e=1; e < maxe; e++)
		{
			ent = (void*)EDICT_NUM_PB(prinst, e);
			if (ED_ISFREE(ent))
				continue;

			if ((int)ent->xv->drawmask & mask)
			{
				if (ent->xv->predraw)
				{
					*csqcg.self = EDICT_TO_PROG(prinst, (void*)ent);
					PR_ExecuteProgram(prinst, ent->xv->predraw);

					if (ED_ISFREE(ent) || G_FLOAT(OFS_RETURN))
						continue;	//bummer...
				}
				if (CopyCSQCEdictToEntity(ent, &rent))
				{
					CLQ1_AddShadow(&rent);
					V_AddAxisEntity(&rent);
				}
			}
		}
	}
	*csqcg.self = oldself;

	if (cl.worldmodel)
	{
		if (mask & MASK_STDVIEWMODEL)
		{
			CL_LinkViewModel ();
		}
		if (mask & MASK_DELTA)
		{
			CL_LinkProjectiles ();
			CL_UpdateTEnts ();
		}
	}

	RSpeedEnd(RSPEED_LINKENTITIES);
}

//enum {vb_vertexcoord, vb_texcoord, vb_rgba, vb_normal, vb_sdir, vb_tdir, vb_indexes, vb_rgb, vb_alpha};
//vboidx = vbuff_create(numverts, numidx, flags)
//vbuff_updateptr(vboidx, datatype, ptr, firstvert, numverts)
//vbuff_updateone(vboidx, datatype, index, __variant data)
//vbuff_render(vboidx, shaderid, uniforms, uniformssize)
//vbuff_delete(vboidx), vboidx=0

static shader_t *csqc_poly_shader;
static int csqc_poly_origvert;
static int csqc_poly_origidx;
static int csqc_poly_startvert;
static int csqc_poly_startidx;
static int csqc_poly_flags;
static int csqc_poly_2d;

#define DRAWFLAG_NORMAL		0
#define DRAWFLAG_ADD		1
#define DRAWFLAG_MODULATE	2
#define DRAWFLAG_MODULATE2	3
#define DRAWFLAG_2D			(1u<<2)
#define DRAWFLAG_TWOSIDED	0x400
#define DRAWFLAG_LINES		0x800

static void CSQC_PolyFlush(void)
{
	mesh_t mesh;
	R2D_Flush = NULL;

	//make sure there's actually something there...
	if (cl_numstrisvert == csqc_poly_origvert)
		return;

	if (!csqc_poly_2d)
	{
		scenetris_t *t;
		/*regular 3d polys are inserted into a 'scene trisoup' that the backend can then source from (multiple times, depending on how its drawn)*/
		if (cl_numstris == cl_maxstris)
		{
			cl_maxstris+=8;
			cl_stris = BZ_Realloc(cl_stris, sizeof(*cl_stris)*cl_maxstris);
		}
		t = &cl_stris[cl_numstris++];
		t->shader = csqc_poly_shader;
		t->flags = csqc_poly_flags;
		t->firstidx = csqc_poly_origidx;
		t->firstvert = csqc_poly_origvert;

		t->numidx = cl_numstrisidx - t->firstidx;
		t->numvert = cl_numstrisvert-csqc_poly_origvert;
	}
	else
	{
		/*2d polys need to be flushed now*/
		memset(&mesh, 0, sizeof(mesh));

		mesh.istrifan = (csqc_poly_origvert == csqc_poly_startvert);
		mesh.xyz_array = cl_strisvertv + csqc_poly_origvert;
		mesh.st_array = cl_strisvertt + csqc_poly_origvert;
		mesh.colors4f_array[0] = cl_strisvertc + csqc_poly_origvert;
		mesh.indexes = cl_strisidx + csqc_poly_origidx;
		mesh.numindexes = cl_numstrisidx - csqc_poly_origidx;
		mesh.numvertexes = cl_numstrisvert-csqc_poly_origvert;
		/*undo the positions so we don't draw the same verts more than once*/
		cl_numstrisvert = csqc_poly_origvert;
		cl_numstrisidx = csqc_poly_origvert;

		BE_DrawMesh_Single(csqc_poly_shader, &mesh, NULL, csqc_poly_flags);
	}

	//must call begin before the next poly
	csqc_poly_shader = NULL;
}

// #306 void(string texturename) R_BeginPolygon (EXT_CSQC_???)
void QCBUILTIN PF_R_PolygonBegin(pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	const char *shadername = PR_GetStringOfs(prinst, OFS_PARM0);
	int qcflags = (prinst->callargc > 1)?G_FLOAT(OFS_PARM1):0;
	shader_t *shader;
	extern shader_t *shader_draw_fill_trans;
	int beflags;
	qboolean twod;

	if (prinst->callargc > 2)
		twod = G_FLOAT(OFS_PARM2);
	else if (csqc_isdarkplaces)
	{
		twod = !csqc_dp_lastwas3d;
		csqc_deprecated("guessing 2d mode based upon random builtin calls");
	}
	else
		twod = qcflags & DRAWFLAG_2D;

	if ((qcflags & 3) == DRAWFLAG_ADD)
		beflags = BEF_NOSHADOWS|BEF_FORCEADDITIVE;
	else
		beflags = BEF_NOSHADOWS;
	if (csqc_isdarkplaces || (qcflags & DRAWFLAG_TWOSIDED))
		beflags |= BEF_FORCETWOSIDED;

	if (!*shadername)
		shader = shader_draw_fill_trans;	//dp compat...
	else if (twod)
		shader = R_RegisterPic(shadername, NULL);
	else
		shader = R_RegisterSkin(shadername, NULL);

	if (R2D_Flush && (R2D_Flush != CSQC_PolyFlush || csqc_poly_shader != shader || csqc_poly_flags != beflags || csqc_poly_2d != twod))
		R2D_Flush();
	if (!R2D_Flush)
	{	//this is where our current (2d) batch starts
		csqc_poly_origvert = cl_numstrisvert;
		csqc_poly_origidx = cl_numstrisidx;
	}
	R2D_Flush = CSQC_PolyFlush;
	csqc_poly_shader = shader;
	csqc_poly_flags = beflags;
	csqc_poly_2d = twod;

	//this is where our current poly starts
	csqc_poly_startvert = cl_numstrisvert;
	csqc_poly_startidx = cl_numstrisidx;
}

// #307 void(vector org, vector texcoords, vector rgb, float alpha) R_PolygonVertex (EXT_CSQC_???)
void QCBUILTIN PF_R_PolygonVertex(pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	if (cl_numstrisvert == cl_maxstrisvert)
	{
		cl_maxstrisvert+=64;
		cl_strisvertv = BZ_Realloc(cl_strisvertv, sizeof(*cl_strisvertv)*cl_maxstrisvert);
		cl_strisvertt = BZ_Realloc(cl_strisvertt, sizeof(*cl_strisvertt)*cl_maxstrisvert);
		cl_strisvertc = BZ_Realloc(cl_strisvertc, sizeof(*cl_strisvertc)*cl_maxstrisvert);
	}

	VectorCopy(G_VECTOR(OFS_PARM0), cl_strisvertv[cl_numstrisvert]);
	Vector2Copy(G_VECTOR(OFS_PARM1), cl_strisvertt[cl_numstrisvert]);
	VectorCopy(G_VECTOR(OFS_PARM2), cl_strisvertc[cl_numstrisvert]);
	cl_strisvertc[cl_numstrisvert][3] = G_FLOAT(OFS_PARM3);
	cl_numstrisvert++;
}

// #308 void() R_EndPolygon (EXT_CSQC_???)
void QCBUILTIN PF_R_PolygonEnd(pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	int i;
	int nv;
	int flags = csqc_poly_flags;
	int first;

	if (!csqc_poly_shader)
		return;

	nv = cl_numstrisvert-csqc_poly_startvert;
	if (nv == 2)
		flags |= BEF_LINES;
	else
		flags &= ~BEF_LINES;

	if (flags != csqc_poly_flags || (cl_numstrisvert-csqc_poly_origvert) >= 32768)
	{
		int sv = cl_numstrisvert - nv;
		cl_numstrisvert -= nv;
		CSQC_PolyFlush();

		csqc_poly_origvert = cl_numstrisvert;
		csqc_poly_origidx = cl_numstrisidx;
		R2D_Flush = CSQC_PolyFlush;
		csqc_poly_flags = flags;
		csqc_poly_startvert = cl_numstrisvert;
		csqc_poly_startidx = cl_numstrisidx;

		memcpy(cl_strisvertv+cl_numstrisvert, cl_strisvertv + sv, sizeof(*cl_strisvertv) * nv);
		memcpy(cl_strisvertt+cl_numstrisvert, cl_strisvertt + sv, sizeof(*cl_strisvertt) * nv);
		memcpy(cl_strisvertc+cl_numstrisvert, cl_strisvertc + sv, sizeof(*cl_strisvertc) * nv);
		cl_numstrisvert += nv;
	}

	if (flags & BEF_LINES)
	{
		nv = cl_numstrisvert-csqc_poly_startvert;
		if (cl_numstrisidx+nv > cl_maxstrisidx)
		{
			cl_maxstrisidx=cl_numstrisidx+nv + 64;
			cl_strisidx = BZ_Realloc(cl_strisidx, sizeof(*cl_strisidx)*cl_maxstrisidx);
		}

		first = csqc_poly_startvert - csqc_poly_origvert;
		/*build the line list fan out of triangles*/
		for (i = 1; i < nv; i++)
		{
			cl_strisidx[cl_numstrisidx++] = first + i-1;
			cl_strisidx[cl_numstrisidx++] = first + i;
		}
	}
	else
	{
		nv = cl_numstrisvert-csqc_poly_startvert;
		if (cl_numstrisidx+(nv-2)*3 > cl_maxstrisidx)
		{
			cl_maxstrisidx=cl_numstrisidx+(nv-2)*3 + 64;
			cl_strisidx = BZ_Realloc(cl_strisidx, sizeof(*cl_strisidx)*cl_maxstrisidx);
		}

		first = csqc_poly_startvert - csqc_poly_origvert;
		/*build the triangle fan out of triangles*/
		for (i = 2; i < nv; i++)
		{
			cl_strisidx[cl_numstrisidx++] = first + 0;
			cl_strisidx[cl_numstrisidx++] = first + i-1;
			cl_strisidx[cl_numstrisidx++] = first + i;
		}
	}

	/*set up ready for the next poly*/
	csqc_poly_startvert = cl_numstrisvert;
	csqc_poly_startidx = cl_numstrisidx;
}

typedef struct
{
	vec3_t xyz;
	vec2_t st;
	vec4_t rgba;
//	vec3_t norm;
//	vec3_t sdir;
//	vec3_t tdir;
} qcvertex_t;
void QCBUILTIN PF_R_AddTrisoup_Simple(pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	shader_t *shader;		//parm 0
	unsigned int qcflags	= G_INT(OFS_PARM1);
	unsigned int vertsptr	= G_INT(OFS_PARM2);
	unsigned int indexesptr	= G_INT(OFS_PARM3);
	unsigned int numindexes	= G_INT(OFS_PARM4);
	qboolean twod = qcflags & DRAWFLAG_2D;
	unsigned int beflags;
	unsigned int numverts;
	qcvertex_t *vert;
	unsigned int *idx;
	unsigned int i, j, first;

	if ((qcflags & 3) == DRAWFLAG_ADD)
		beflags = BEF_NOSHADOWS|BEF_FORCEADDITIVE;
	else if ((qcflags & 3) == DRAWFLAG_MODULATE)
		beflags = BEF_NOSHADOWS|BEF_FORCETRANSPARENT;
	else
		beflags = BEF_NOSHADOWS;
	if (qcflags & DRAWFLAG_TWOSIDED)
		beflags |= BEF_FORCETWOSIDED;
	if (qcflags & DRAWFLAG_LINES)
		beflags |= BEF_LINES;

	if (twod)
		shader = R_RegisterPic(PR_GetStringOfs(prinst, OFS_PARM0), NULL);
	else
		shader = R_RegisterSkin(PR_GetStringOfs(prinst, OFS_PARM0), NULL);

	if (R2D_Flush && (R2D_Flush != CSQC_PolyFlush || csqc_poly_shader != shader || csqc_poly_flags != beflags || csqc_poly_2d != twod))
		R2D_Flush();
	if (!R2D_Flush)
	{	//this is where our current (2d) batch starts
		csqc_poly_origvert = cl_numstrisvert;
		csqc_poly_origidx = cl_numstrisidx;
	}

	//validates the pointer.
	numverts = (prinst->stringtablesize - vertsptr) / sizeof(qcvertex_t);
	if (numverts < 1 || vertsptr <= 0 || vertsptr+numverts*sizeof(qcvertex_t) > prinst->stringtablesize)
	{
		PR_BIError(prinst, "PF_R_AddTrisoup: invalid vertexes pointer\n");
		return;
	}
	vert = (qcvertex_t*)(prinst->stringtable + vertsptr);
	if (indexesptr <= 0 || indexesptr+numindexes*sizeof(int) > prinst->stringtablesize)
	{
		PR_BIError(prinst, "PF_R_AddTrisoup: invalid indexes pointer\n");
		return;
	}
	idx = (int*)(prinst->stringtable + indexesptr);

	first = cl_numstrisvert - csqc_poly_origvert;
	if (first + numindexes > MAX_INDICIES)
	{
		if (numindexes > MAX_INDICIES)
		{
			PR_BIError(prinst, "PF_R_AddTrisoup: single batch exceeds MAX_INDICIES\n");
			return;
		}
		else if (R2D_Flush)	//should always be true
		{
			R2D_Flush();
			first = 0;
		}
	}

	R2D_Flush = CSQC_PolyFlush;
	csqc_poly_shader = shader;
	csqc_poly_flags = beflags;
	csqc_poly_2d = twod;

	//hacky crappy solution - make a copy of each used vert rather than copying the entire data out.

	if (cl_numstrisidx+numindexes > cl_maxstrisidx)
	{
		cl_maxstrisidx=cl_numstrisidx+numindexes;
		cl_strisidx = BZ_Realloc(cl_strisidx, sizeof(*cl_strisidx)*cl_maxstrisidx);
	}
	if (cl_numstrisvert+numindexes > cl_maxstrisvert)
	{
		cl_maxstrisvert=cl_numstrisvert+numindexes;
		cl_strisvertv = BZ_Realloc(cl_strisvertv, sizeof(*cl_strisvertv)*cl_maxstrisvert);
		cl_strisvertt = BZ_Realloc(cl_strisvertt, sizeof(*cl_strisvertt)*cl_maxstrisvert);
		cl_strisvertc = BZ_Realloc(cl_strisvertc, sizeof(*cl_strisvertc)*cl_maxstrisvert);
	}
	for (i = 0; i < numindexes; i++)
	{
		j = *idx++;
		if (j >= numverts)
			j = 0;	//out of bounds.

		VectorCopy(vert[j].xyz, cl_strisvertv[cl_numstrisvert]);
		Vector2Copy(vert[j].st, cl_strisvertt[cl_numstrisvert]);
		Vector4Copy(vert[j].rgba, cl_strisvertc[cl_numstrisvert]);
		cl_numstrisvert++;

		cl_strisidx[cl_numstrisidx++] = first++;
	}


	//in case someone calls polygonvertex+end without beginpolygon
	csqc_poly_startvert = cl_numstrisvert;
	csqc_poly_startidx = cl_numstrisidx;
}


qboolean csqc_rebuildmatricies;
float csqc_proj_matrix[16];
float csqc_proj_matrix_inverse[16];
void V_ApplyAFov(playerview_t *pv);
void buildmatricies(void)
{
	float modelview[16];
	float proj[16];
	float ofovx = r_refdef.fov_x,ofovy=r_refdef.fov_y;

	V_ApplyAFov(csqc_playerview);

	/*build view and projection matricies*/
	Matrix4x4_CM_ModelViewMatrix(modelview, r_refdef.viewangles, r_refdef.vieworg);
	if (r_refdef.useperspective)
		Matrix4x4_CM_Projection2(proj, r_refdef.fov_x, r_refdef.fov_y, 4);
	else
		Matrix4x4_CM_Orthographic(proj, -r_refdef.fov_x/2, r_refdef.fov_x/2, -r_refdef.fov_y/2, r_refdef.fov_y/2, r_refdef.mindist, r_refdef.maxdist>=1?r_refdef.maxdist:9999);

	/*build the vp matrix*/
	Matrix4_Multiply(proj, modelview, csqc_proj_matrix);

	/*build the unproject matrix (inverted vp matrix)*/
	Matrix4_Invert(csqc_proj_matrix, csqc_proj_matrix_inverse);

	csqc_rebuildmatricies = false;

	r_refdef.fov_x = ofovx,
	r_refdef.fov_y = ofovy;
}
static void QCBUILTIN PF_cs_project (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	if (csqc_rebuildmatricies)
		buildmatricies();


	{
		float *in = G_VECTOR(OFS_PARM0);
		float *out = G_VECTOR(OFS_RETURN);
		float v[4], tempv[4];

		v[0] = in[0];
		v[1] = in[1];
		v[2] = in[2];
		v[3] = 1;

		Matrix4x4_CM_Transform4(csqc_proj_matrix, v, tempv);

		tempv[0] /= tempv[3];
		tempv[1] /= tempv[3];
		tempv[2] /= tempv[3];

		out[0] = (1+tempv[0])/2;
		out[1] = 1-(1+tempv[1])/2;
		out[2] = tempv[2];

		out[0] = out[0]*r_refdef.vrect.width + r_refdef.vrect.x;
		out[1] = out[1]*r_refdef.vrect.height + r_refdef.vrect.y;

		if (tempv[3] < 0)
			out[2] *= -1;
	}
}
static void QCBUILTIN PF_cs_unproject (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	if (csqc_rebuildmatricies)
		buildmatricies();

	{
		float *in = G_VECTOR(OFS_PARM0);
		float *out = G_VECTOR(OFS_RETURN);
		float tx = in[0], ty = in[1];

		float v[4], tempv[4];

		tx = ((tx-r_refdef.vrect.x)/r_refdef.vrect.width);
		ty = ((ty-r_refdef.vrect.y)/r_refdef.vrect.height);
		ty = 1-ty;
		v[0] = tx*2-1;
		v[1] = ty*2-1;
		v[2] = in[2]*2-1;	//gl projection matrix scales -1 to 1 (unlike d3d, which is 0 to 1)
		v[3] = 1;

		//don't use 1, because the far clip plane really is an infinite distance away. and that tends to result division by infinity.
		if (v[2] >= 1)
			v[2] = 0.999999;

		Matrix4x4_CM_Transform4(csqc_proj_matrix_inverse, v, tempv);

		out[0] = tempv[0]/tempv[3];
		out[1] = tempv[1]/tempv[3];
		out[2] = tempv[2]/tempv[3];
	}
}

//clear scene, and set up the default stuff.
static void QCBUILTIN PF_R_ClearScene (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	if (R2D_Flush)
		R2D_Flush();

	if (prinst->callargc > 0)
		CSQC_ChangeLocalPlayer(G_FLOAT(OFS_PARM0));

	csqc_rebuildmatricies = true;
	csqc_dp_lastwas3d = true;	//cleared by the next drawpic.
	csqc_poly_shader = NULL;

	CL_DecayLights ();

#if defined(SKELETALOBJECTS) || defined(RAGDOLLS)
	skel_dodelete(&csqc_world);
#endif
	CL_ClearEntityLists();

	V_ClearRefdef(csqc_playerview);
	r_refdef.drawsbar = false;	//csqc defaults to no sbar.
	r_refdef.drawcrosshair = false;

	V_CalcRefdef(csqc_playerview);	//set up the defaults
}

void QCBUILTIN PF_R_GetViewFlag(pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	viewflags parametertype = G_FLOAT(OFS_PARM0);
	float *r = G_VECTOR(OFS_RETURN);

	r[0] = 0;
	r[1] = 0;
	r[2] = 0;

	switch(parametertype)
	{
nogameaccess:
		csqc_deprecated("PF_R_GetViewFlag: game access is blocked");
		break;
	case VF_ACTIVESEAT:
		if (prinst == csqc_world.progs)
			*r = csqc_playerseat;
		break;
	case VF_FOV:
		r[0] = r_refdef.fov_x;
		r[1] = r_refdef.fov_y;
		break;

	case VF_FOVX:
		*r = r_refdef.fov_x;
		break;

	case VF_FOVY:
		*r = r_refdef.fov_y;
		break;

	case VF_AFOV:
		*r = r_refdef.afov;
		break;

	case VF_ORIGIN:
		if (csqc_nogameaccess && prinst == csqc_world.progs)
			goto nogameaccess;
		VectorCopy(r_refdef.vieworg, r);
		break;

	case VF_ORIGIN_Z:
	case VF_ORIGIN_X:
	case VF_ORIGIN_Y:
		if (csqc_nogameaccess && prinst == csqc_world.progs)
			goto nogameaccess;
		*r = r_refdef.vieworg[parametertype-VF_ORIGIN_X];
		break;

	case VF_ANGLES:
		if (csqc_nogameaccess && prinst == csqc_world.progs)
			goto nogameaccess;
		VectorCopy(r_refdef.viewangles, r);
		break;
	case VF_ANGLES_X:
	case VF_ANGLES_Y:
	case VF_ANGLES_Z:
		if (csqc_nogameaccess && prinst == csqc_world.progs)
			goto nogameaccess;
		*r = r_refdef.viewangles[parametertype-VF_ANGLES_X];
		break;

	case VF_CL_VIEWANGLES_V:
		if (csqc_nogameaccess && prinst == csqc_world.progs)
			goto nogameaccess;
		if (csqc_playerview)
			VectorCopy(csqc_playerview->viewangles, r);
		break;
	case VF_CL_VIEWANGLES_X:
	case VF_CL_VIEWANGLES_Y:
	case VF_CL_VIEWANGLES_Z:
		if (csqc_nogameaccess && prinst == csqc_world.progs)
			goto nogameaccess;
		if (csqc_playerview)
			*r = csqc_playerview->viewangles[parametertype-VF_CL_VIEWANGLES_X];
		break;

	case VF_CARTESIAN_ANGLES:
		if (csqc_nogameaccess && prinst == csqc_world.progs)
			goto nogameaccess;
		Con_Printf(CON_WARNING "WARNING: CARTESIAN ANGLES ARE NOT YET SUPPORTED!\n");
		break;

	case VF_VIEWPORT:
		r[0] = r_refdef.grect.width;
		r[1] = r_refdef.grect.height;
		break;

	case VF_SIZE_X:
		*r = r_refdef.grect.width;
		if (csqc_isdarkplaces && prinst == csqc_world.progs)
			*r *= (float)vid.pixelwidth / vid.width;
		break;
	case VF_SIZE_Y:
		*r = r_refdef.grect.height;
		if (csqc_isdarkplaces && prinst == csqc_world.progs)
			*r *= (float)vid.pixelheight / vid.height;
		break;
	case VF_SIZE:
		r[0] = r_refdef.grect.width;
		r[1] = r_refdef.grect.height;
		r[2] = 0;

		if (csqc_isdarkplaces && prinst == csqc_world.progs)
		{
			r[0] *= (float)vid.pixelwidth / vid.width;
			r[1] *= (float)vid.pixelheight / vid.height;
		}
		break;

	case VF_MIN_X:
		*r = r_refdef.grect.x;
		break;
	case VF_MIN_Y:
		*r = r_refdef.grect.y;
		break;
	case VF_MIN:
		r[0] = r_refdef.grect.x;
		r[1] = r_refdef.grect.y;
		break;

	case VF_MINDIST:
		*r = r_refdef.mindist;
		break;
	case VF_MAXDIST:
		*r = r_refdef.maxdist;
		break;

	case VF_DRAWWORLD:
		*r = !(r_refdef.flags&RDF_NOWORLDMODEL);
		break;
	case VF_ENGINESBAR:
		*r = r_refdef.drawsbar;
		break;
	case VF_DRAWCROSSHAIR:
		*r = r_refdef.drawcrosshair;
		break;

	case VF_PERSPECTIVE:
		*r = r_refdef.useperspective;
		break;

	case VF_SCREENVSIZE:
		r[0] = vid.width;
		r[1] = vid.height;
		break;
	case VF_SCREENPSIZE:
		r[0] = vid.rotpixelwidth;
		r[1] = vid.rotpixelheight;
		break;

	default:
		Con_DPrintf("GetViewFlag: %i not recognised\n", parametertype);
		break;
	}
}
uploadfmt_t PR_TranslateTextureFormat(int qcformat)
{
	switch(qcformat)
	{
	case 1: return TF_RGBA32;
	case 2: return TF_RGBA16F;
	case 3: return TF_RGBA32F;
	case 4: return TF_DEPTH16;
	case 5: return TF_DEPTH24;
	case 6: return TF_DEPTH32;
	default:return TF_INVALID;
	}
}
void QCBUILTIN PF_R_SetViewFlag(pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	viewflags parametertype = G_FLOAT(OFS_PARM0);
	float *p = G_VECTOR(OFS_PARM1);

	if (prinst->callargc < 2)
	{
		csqc_deprecated("PF_R_SetViewFlag called with wrong argument count\n");
		PF_R_GetViewFlag(prinst, pr_globals);
		return;
	}

	if (R2D_Flush)
		R2D_Flush();

	csqc_rebuildmatricies = true;

	G_FLOAT(OFS_RETURN) = 1;
	switch(parametertype)
	{
	case VF_ACTIVESEAT:
		if (prinst == csqc_world.progs)
		{
			if (csqc_playerseat != *p)
			{
				CSQC_ChangeLocalPlayer(*p);
				if (prinst->callargc < 3 || G_FLOAT(OFS_PARM2))
					V_CalcRefdef(csqc_playerview);	//set up the default position+angles for the named player.
			}
		}
		break;
	case VF_VIEWENTITY:
		//switches over EXTERNALMODEL flags and clears WEAPONMODEL flagged entities.
		//FIXME: make affect addentities(MASK_ENGINE) calls too.
		V_EditExternalModels(*p, NULL, 0);
		break;
	case VF_FOV:
		//explicit fov overrides aproximate fov
		r_refdef.afov = 0;
		r_refdef.fov_x = p[0];
		r_refdef.fov_y = p[1];
		r_refdef.dirty |= RDFD_FOV;
		break;

	case VF_FOVX:
		r_refdef.afov = 0;
		r_refdef.fov_x = *p;
		r_refdef.dirty |= RDFD_FOV;
		break;

	case VF_FOVY:
		r_refdef.afov = 0;
		r_refdef.fov_y = *p;
		r_refdef.dirty |= RDFD_FOV;
		break;

	case VF_AFOV:
		r_refdef.afov = *p;
		r_refdef.fov_x = 0;
		r_refdef.fov_y = 0;
		r_refdef.dirty |= RDFD_FOV;
		break;

	case VF_ORIGIN:
		VectorCopy(p, r_refdef.vieworg);
		if (csqc_playerview)
			csqc_playerview->crouch = 0;
		break;

	case VF_ORIGIN_Z:
		if (csqc_playerview)
			csqc_playerview->crouch = 0;
	case VF_ORIGIN_X:
	case VF_ORIGIN_Y:
		r_refdef.vieworg[parametertype-VF_ORIGIN_X] = *p;
		break;

	case VF_ANGLES:
		VectorCopy(p, r_refdef.viewangles);
		break;
	case VF_ANGLES_X:
	case VF_ANGLES_Y:
	case VF_ANGLES_Z:
		r_refdef.viewangles[parametertype-VF_ANGLES_X] = *p;
		break;

	case VF_CL_VIEWANGLES_V:
		if (csqc_playerview)
			VectorCopy(p, csqc_playerview->viewangles);
		break;
	case VF_CL_VIEWANGLES_X:
	case VF_CL_VIEWANGLES_Y:
	case VF_CL_VIEWANGLES_Z:
		if (csqc_playerview)
			csqc_playerview->viewangles[parametertype-VF_CL_VIEWANGLES_X] = *p;
		break;

	case VF_CARTESIAN_ANGLES:
		Con_Printf(CON_WARNING "WARNING: CARTESIAN ANGLES ARE NOT YET SUPPORTED!\n");
		break;

	case VF_VIEWPORT:
		r_refdef.grect.x = p[0];
		r_refdef.grect.y = p[1];
		p = G_VECTOR(OFS_PARM2);
		r_refdef.grect.width = p[0];
		r_refdef.grect.height = p[1];
		r_refdef.dirty |= RDFD_FOV;
		break;

	case VF_SIZE_X:
		r_refdef.grect.width = *p;
		r_refdef.dirty |= RDFD_FOV;
		break;
	case VF_SIZE_Y:
		r_refdef.grect.height = *p;
		r_refdef.dirty |= RDFD_FOV;
		break;
	case VF_SIZE:
		r_refdef.grect.width = p[0];
		r_refdef.grect.height = p[1];
		r_refdef.dirty |= RDFD_FOV;

		if (csqc_isdarkplaces)
		{
			r_refdef.grect.width *= (float)vid.width / vid.pixelwidth;
			r_refdef.grect.height *= (float)vid.height / vid.pixelheight;
		}
		break;

	case VF_MIN_X:
		r_refdef.grect.x = *p;
		break;
	case VF_MIN_Y:
		r_refdef.grect.y = *p;
		break;
	case VF_MIN:
		r_refdef.grect.x = p[0];
		r_refdef.grect.y = p[1];
		break;
	case VF_MINDIST:
		r_refdef.mindist = *p;
		break;
	case VF_MAXDIST:
		r_refdef.maxdist = *p;
		break;

	case VF_DRAWWORLD:
		r_refdef.flags = (r_refdef.flags&~RDF_NOWORLDMODEL) | (*p?0:RDF_NOWORLDMODEL);
		break;
	case VF_ENGINESBAR:
		r_refdef.drawsbar = !!*p;
		break;
	case VF_DRAWCROSSHAIR:
		r_refdef.drawcrosshair = *p;
		break;

	case VF_PERSPECTIVE:
		r_refdef.useperspective = *p;
		break;

	case VF_RT_DESTCOLOUR0:
	case VF_RT_DESTCOLOUR1:
	case VF_RT_DESTCOLOUR2:
	case VF_RT_DESTCOLOUR3:
	case VF_RT_DESTCOLOUR4:
	case VF_RT_DESTCOLOUR5:
	case VF_RT_DESTCOLOUR6:
	case VF_RT_DESTCOLOUR7:
		{
			int i = parametertype - VF_RT_DESTCOLOUR0;
			Q_strncpyz(r_refdef.rt_destcolour[i].texname, PR_GetStringOfs(prinst, OFS_PARM1), sizeof(r_refdef.rt_destcolour[i].texname));
			if (prinst->callargc >= 4 && *r_refdef.rt_destcolour[i].texname)
			{
				float fmt = G_FLOAT(OFS_PARM2);
				float *size = G_VECTOR(OFS_PARM3);
				if (fmt < 0)
					R2D_RT_Configure(r_refdef.rt_destcolour[i].texname, size[0], size[1], PR_TranslateTextureFormat(-fmt), (RT_IMAGEFLAGS&~IF_LINEAR)|IF_NEAREST);
				else
					R2D_RT_Configure(r_refdef.rt_destcolour[i].texname, size[0], size[1], PR_TranslateTextureFormat(fmt), RT_IMAGEFLAGS);
			}
			BE_RenderToTextureUpdate2d(true);
		}
		break;
	case VF_RT_SOURCECOLOUR:
		Q_strncpyz(r_refdef.rt_sourcecolour.texname, PR_GetStringOfs(prinst, OFS_PARM1), sizeof(r_refdef.rt_sourcecolour));
		if (prinst->callargc >= 4 && *r_refdef.rt_sourcecolour.texname)
		{
			float fmt = G_FLOAT(OFS_PARM2);
			float *size = G_VECTOR(OFS_PARM3);
			if (fmt < 0)
				R2D_RT_Configure(r_refdef.rt_sourcecolour.texname, size[0], size[1], PR_TranslateTextureFormat(-fmt), (RT_IMAGEFLAGS&~IF_LINEAR)|IF_NEAREST);
			else
				R2D_RT_Configure(r_refdef.rt_sourcecolour.texname, size[0], size[1], PR_TranslateTextureFormat(fmt), RT_IMAGEFLAGS);
		}
		BE_RenderToTextureUpdate2d(false);
		break;
	case VF_RT_DEPTH:
		Q_strncpyz(r_refdef.rt_depth.texname, PR_GetStringOfs(prinst, OFS_PARM1), sizeof(r_refdef.rt_depth.texname));
		if (prinst->callargc >= 4 && *r_refdef.rt_depth.texname)
		{
			float fmt = G_FLOAT(OFS_PARM2);
			float *size = G_VECTOR(OFS_PARM3);
			if (fmt < 0)
				R2D_RT_Configure(r_refdef.rt_depth.texname, size[0], size[1], PR_TranslateTextureFormat(-fmt), (RT_IMAGEFLAGS&~IF_LINEAR)|IF_NEAREST);
			else
				R2D_RT_Configure(r_refdef.rt_depth.texname, size[0], size[1], PR_TranslateTextureFormat(fmt), RT_IMAGEFLAGS);
		}
		BE_RenderToTextureUpdate2d(false);
		break;
	case VF_RT_RIPPLE:
		Q_strncpyz(r_refdef.rt_ripplemap.texname, PR_GetStringOfs(prinst, OFS_PARM1), sizeof(r_refdef.rt_ripplemap.texname));
		if (prinst->callargc >= 4 && *r_refdef.rt_ripplemap.texname)
		{
			float fmt = G_FLOAT(OFS_PARM2);
			float *size = G_VECTOR(OFS_PARM3);
			if (fmt < 0)
				R2D_RT_Configure(r_refdef.rt_ripplemap.texname, size[0], size[1], PR_TranslateTextureFormat(-fmt), (RT_IMAGEFLAGS&~IF_LINEAR)|IF_NEAREST);
			else
				R2D_RT_Configure(r_refdef.rt_ripplemap.texname, size[0], size[1], PR_TranslateTextureFormat(fmt), RT_IMAGEFLAGS);
		}
		BE_RenderToTextureUpdate2d(false);
		break;

	case VF_ENVMAP:
		Q_strncpyz(r_refdef.nearenvmap.texname, PR_GetStringOfs(prinst, OFS_PARM1), sizeof(r_refdef.nearenvmap.texname));
		BE_RenderToTextureUpdate2d(false);
		break;

	case VF_USERDATA:
		{
			int qcptr = G_INT(OFS_PARM1);
			int size = G_INT(OFS_PARM2);
			void *ptr;

			if (size > sizeof(r_refdef.userdata))
				size = sizeof(r_refdef.userdata);

			//validates the pointer.
			if (qcptr < 0 || qcptr+size >= prinst->stringtablesize || size < 0)
			{
				PR_BIError(prinst, "PF_R_SetViewFlag: invalid pointer\n");
				return;
			}
			ptr = (struct reverbproperties_s*)(prinst->stringtable + qcptr);
			memcpy(r_refdef.userdata, ptr, size);
		}
		break;

	default:
		Con_DPrintf("SetViewFlag: %i not recognised\n", parametertype);
		G_FLOAT(OFS_RETURN) = 0;
		break;
	}
}

void R2D_PolyBlend (void);
void R_DrawNameTags(void);
static void QCBUILTIN PF_R_RenderScene(pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	qboolean scissored;

	if (R2D_Flush)
		R2D_Flush();
	csqc_poly_shader = NULL;

	if (csqc_worldchanged)
	{
		csqc_worldchanged = false;
		Surf_NewMap();
		CL_UpdateWindowTitle();

		World_RBE_Shutdown(&csqc_world);
		World_RBE_Start(&csqc_world);
	}

	if (cl.worldmodel)
		R_PushDlights ();

	r_refdef.playerview = csqc_playerview;

	V_CalcGunPositionAngle(csqc_playerview, V_CalcBob(csqc_playerview, true));

	V_ApplyRefdef();
	R_RenderView();
	R2D_PolyBlend ();

	if (r_refdef.grect.x || r_refdef.grect.y || r_refdef.grect.width != vid.fbvwidth || r_refdef.grect.height != vid.fbvheight)
	{
		srect_t srect;
		srect.x = (float)r_refdef.grect.x / vid.fbvwidth;
		srect.y = (float)r_refdef.grect.y / vid.fbvheight;
		srect.width = (float)r_refdef.grect.width / vid.fbvwidth;
		srect.height = (float)r_refdef.grect.height / vid.fbvheight;
		srect.dmin = -99999;
		srect.dmax = 99999;
		srect.y = (1-srect.y) - srect.height;
		BE_Scissor(&srect);
		scissored = true;
	}
	else
		scissored = false;

	R_DrawNameTags();

	if (r_refdef.drawsbar)
	{
#ifdef PLUGINS
		Plug_SBar (r_refdef.playerview);
#else
		if (Sbar_ShouldDraw(r_refdef.playerview))
		{
			SCR_TileClear (sb_lines);
			Sbar_Draw (r_refdef.playerview);
			Sbar_DrawScoreboard (r_refdef.playerview);
		}
		else
			SCR_TileClear (0);
#endif

		if (!Key_Dest_Has(kdm_emenu|kdm_gmenu|kdm_cwindows))
		{
			if (cl.intermissionmode == IM_NQFINALE || cl.intermissionmode == IM_NQCUTSCENE || cl.intermissionmode == IM_H2FINALE)
			{
				SCR_CheckDrawCenterString ();
			}
			else if (cl.intermissionmode != IM_NONE)
			{
				Sbar_IntermissionOverlay (r_refdef.playerview);
			}
		}

		SCR_ShowPics_Draw();
	}

	if (r_refdef.drawcrosshair)
		R2D_DrawCrosshair();

	if (scissored)
	{
		if (R2D_Flush)
			R2D_Flush();

		BE_Scissor(NULL);
	}
}

static void QCBUILTIN PF_cs_getstati(pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	int stnum = G_FLOAT(OFS_PARM0);
	if (stnum < 0 || stnum >= MAX_CL_STATS)
	{
		G_FLOAT(OFS_RETURN) = 0;
		PR_RunWarning(prinst, "invalid stat index");
	}
	else if (stnum >= 128 && csqc_isdarkplaces && cls.protocol != CP_NETQUAKE && !CPNQ_IS_DP)
	{	//dpp7 stats are fucked.
		G_FLOAT(OFS_RETURN) = csqc_playerview->statsf[stnum];
		csqc_deprecated("hacked stat type");
	}
	else
		G_INT(OFS_RETURN) = csqc_playerview->stats[stnum];
}
static void QCBUILTIN PF_cs_getstatbits(pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{	//read a numeric stat into a qc float.
	//if bits offsets are specified, reads explicitly the integer version of the stat, allowing high bits to be read for items2/serverflags. the float stat should have the same value, just with lower precision as a float can't hold a 32bit value. maybe we should just use doubles.

	int stnum = G_FLOAT(OFS_PARM0);
	if (stnum < 0 || stnum >= MAX_CL_STATS)
	{
		G_FLOAT(OFS_RETURN) = 0;
		PR_RunWarning(prinst, "invalid stat index");
	}
	else if (prinst->callargc > 1)
	{
		int val = csqc_playerview->stats[stnum];
		int first, count;
		first = G_FLOAT(OFS_PARM1);
		if (prinst->callargc > 2)
			count = G_FLOAT(OFS_PARM2);
		else
			count = 1;
		G_FLOAT(OFS_RETURN) = (((unsigned int)val)&(((1<<count)-1)<<first))>>first;
	}
	else if (csqc_isdarkplaces && cls.protocol != CP_NETQUAKE && !CPNQ_IS_DP)
	{
		G_FLOAT(OFS_RETURN) = (int)csqc_playerview->statsf[stnum];	//stupid. mods like xonotic end up with an ugly hud if they're actually given any precision
		if (G_FLOAT(OFS_RETURN) != csqc_playerview->statsf[stnum])
			csqc_deprecated("getstatf stat truncation");	//this is a common call. only get pissy if there's a reason to get pissy.
	}
	else
		G_FLOAT(OFS_RETURN) = csqc_playerview->statsf[stnum];
}
static void QCBUILTIN PF_cs_getstats(pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	int stnum = G_FLOAT(OFS_PARM0);

	if (stnum < 0 || stnum >= MAX_CL_STATS)
	{
		G_INT(OFS_RETURN) = 0;
		PR_RunWarning(prinst, "invalid stat index");
	}
	else if (cls.fteprotocolextensions & PEXT_CSQC)
		RETURN_TSTRING(csqc_playerview->statsstr[stnum]);
	else if (stnum >= MAX_CL_STATS-3)
	{
		G_INT(OFS_RETURN) = 0;
		PR_RunWarning(prinst, "invalid stat index");
	}
	else
	{
		char out[17];

		//the network protocol byteswaps

		((unsigned int*)out)[0] = LittleLong(csqc_playerview->stats[stnum+0]);
		((unsigned int*)out)[1] = LittleLong(csqc_playerview->stats[stnum+1]);
		((unsigned int*)out)[2] = LittleLong(csqc_playerview->stats[stnum+2]);
		((unsigned int*)out)[3] = LittleLong(csqc_playerview->stats[stnum+3]);
		((unsigned int*)out)[4] = 0;	//make sure it's null terminated

		RETURN_TSTRING(out);
	}
}

static void QCBUILTIN PF_cs_SetOrigin(pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	world_t *w = prinst->parms->user;
	wedict_t *ent = (void*)G_WEDICT(prinst, OFS_PARM0);
	float *org = G_VECTOR(OFS_PARM1);
	if (ent->readonly)
	{
		PR_RunWarning(prinst, "setorigin on entity %i\n", ent->entnum);
		return;
	}
	VectorCopy(org, ent->v->origin);
	World_LinkEdict(w, (wedict_t*)ent, false);
}

static void QCBUILTIN PF_cs_SetSize (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	world_t *w = prinst->parms->user;
	wedict_t	*e;
	float	*min, *max;

	e = G_WEDICT(prinst, OFS_PARM0);
	if (ED_ISFREE(e))
	{
		PR_RunWarning(prinst, "%s edict was free\n", "setsize");
		return;
	}
	if (e->readonly)
	{
		PR_RunWarning(prinst, "setsize on entity %i\n", e->entnum);
		return;
	}
	min = G_VECTOR(OFS_PARM1);
	max = G_VECTOR(OFS_PARM2);
	VectorCopy (min, e->v->mins);
	VectorCopy (max, e->v->maxs);
	VectorSubtract (max, min, e->v->size);
	World_LinkEdict (w, (wedict_t*)e, false);
}

static void cs_settracevars(pubprogfuncs_t *prinst, struct globalvars_s *pr_globals, trace_t *tr)
{
	*csqcg.trace_allsolid = tr->allsolid;
	*csqcg.trace_startsolid = tr->startsolid;
	*csqcg.trace_fraction = tr->fraction;
	*csqcg.trace_inwater = tr->inwater;
	*csqcg.trace_inopen = tr->inopen;
	VectorCopy (tr->endpos, csqcg.trace_endpos);
	VectorCopy (tr->plane.normal, csqcg.trace_plane_normal);
	*csqcg.trace_plane_dist =  tr->plane.dist;
	*csqcg.trace_surfaceflagsi = tr->surface?tr->surface->flags:0;
	if (csqcg.trace_surfacename)
		prinst->SetStringField(prinst, NULL, csqcg.trace_surfacename, tr->surface?tr->surface->name:NULL, true);
	*csqcg.trace_endcontentsi = tr->contents;
	*csqcg.trace_brush_id = tr->brush_id;
	*csqcg.trace_brush_faceid = tr->brush_face;
	*csqcg.trace_surface_id = tr->surface_id;
	*csqcg.trace_bone_id = tr->bone_id;
	*csqcg.trace_triangle_id = tr->triangle_id;
	if (tr->ent)
		*csqcg.trace_ent = EDICT_TO_PROG(csqcprogs, (void*)tr->ent);
	else
		*csqcg.trace_ent = EDICT_TO_PROG(csqcprogs, (void*)csqc_world.edicts);

#ifndef NOLEGACY
	*csqcg.trace_endcontentsf = tr->contents;
	*csqcg.trace_surfaceflagsf = tr->surface?tr->surface->flags:0;

	if (csqcg.trace_dphittexturename)
		prinst->SetStringField(prinst, NULL, csqcg.trace_dphittexturename, tr->surface?tr->surface->name:NULL, true);
	if (csqcg.trace_dpstartcontents)
		*csqcg.trace_dpstartcontents = FTEToDPContents(0);	//fixme, maybe
	if (csqcg.trace_dphitcontents)
		*csqcg.trace_dphitcontents = FTEToDPContents(tr->contents);
	if (csqcg.trace_dphitq3surfaceflags)
		*csqcg.trace_dphitq3surfaceflags = tr->surface?tr->surface->flags:0;
#endif
}

static void QCBUILTIN PF_cs_traceline(pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	float	*v1, *v2, *mins, *maxs;
	trace_t	trace;
	int		nomonsters;
	csqcedict_t	*ent;

	v1 = G_VECTOR(OFS_PARM0);
	v2 = G_VECTOR(OFS_PARM1);
	nomonsters = G_FLOAT(OFS_PARM2);
	ent = (csqcedict_t*)G_EDICT(prinst, OFS_PARM3);

//	if (*prinst->callargc == 6)
//	{
//		mins = G_VECTOR(OFS_PARM4);
//		maxs = G_VECTOR(OFS_PARM5);
//	}
//	else
	{
		mins = vec3_origin;
		maxs = vec3_origin;
	}

	trace = World_Move (&csqc_world, v1, mins, maxs, v2, nomonsters|MOVE_IGNOREHULL, (wedict_t*)ent);

	cs_settracevars(prinst, pr_globals, &trace);
}
static void QCBUILTIN PF_cs_tracebox(pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	float	*v1, *v2, *mins, *maxs;
	trace_t	trace;
	int		nomonsters;
	csqcedict_t	*ent;

	v1 = G_VECTOR(OFS_PARM0);
	mins = G_VECTOR(OFS_PARM1);
	maxs = G_VECTOR(OFS_PARM2);
	v2 = G_VECTOR(OFS_PARM3);
	nomonsters = G_FLOAT(OFS_PARM4);
	ent = (csqcedict_t*)G_EDICT(prinst, OFS_PARM5);

	trace = World_Move (&csqc_world, v1, mins, maxs, v2, nomonsters|MOVE_IGNOREHULL, (wedict_t*)ent);

	cs_settracevars(prinst, pr_globals, &trace);
}

static trace_t CS_Trace_Toss (csqcedict_t *tossent, csqcedict_t *ignore)
{
	int i;
	float gravity;
	vec3_t move, end;
	trace_t trace;
//	float maxvel = Cvar_Get("sv_maxvelocity", "2000", 0, "CSQC physics")->value;

	vec3_t origin, velocity;

	// this has to fetch the field from the original edict, since our copy is truncated
	gravity = 1;//tossent->v->gravity;
	if (!gravity)
		gravity = 1.0;
	gravity *= Cvar_Get("sv_gravity", "800", 0, "CSQC physics")->value * 0.05;

	VectorCopy (tossent->v->origin, origin);
	VectorCopy (tossent->v->velocity, velocity);

	CS_CheckVelocity (tossent);

	for (i = 0;i < 200;i++) // LordHavoc: sanity check; never trace more than 10 seconds
	{
		velocity[2] -= gravity;
		VectorScale (velocity, 0.05, move);
		VectorAdd (origin, move, end);
		trace = World_Move (&csqc_world, origin, tossent->v->mins, tossent->v->maxs, end, MOVE_NORMAL|MOVE_IGNOREHULL, (wedict_t*)tossent);
		VectorCopy (trace.endpos, origin);

		CS_CheckVelocity (tossent);

		if (trace.fraction < 1 && trace.ent && (void*)trace.ent != ignore)
			break;
	}

	trace.fraction = 0; // not relevant
	return trace;
}
static void QCBUILTIN PF_cs_tracetoss (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	trace_t	trace;
	csqcedict_t	*ent;
	csqcedict_t	*ignore;

	ent = (csqcedict_t*)G_EDICT(prinst, OFS_PARM0);
	if (ent == (csqcedict_t*)csqc_world.edicts)
		Con_DPrintf("tracetoss: can not use world entity\n");
	ignore = (csqcedict_t*)G_EDICT(prinst, OFS_PARM1);

	trace = CS_Trace_Toss (ent, ignore);

	cs_settracevars(prinst, pr_globals, &trace);
}

static void QCBUILTIN PF_cs_pointcontents(pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	world_t *w = prinst->parms->user;

	float	*v;
	int cont;

	v = G_VECTOR(OFS_PARM0);

	cont = cl.worldmodel?World_PointContents(w, v):FTECONTENTS_EMPTY;
	if (cont & FTECONTENTS_SOLID)
		G_FLOAT(OFS_RETURN) = Q1CONTENTS_SOLID;
	else if (cont & FTECONTENTS_SKY)
		G_FLOAT(OFS_RETURN) = Q1CONTENTS_SKY;
	else if (cont & FTECONTENTS_LAVA)
		G_FLOAT(OFS_RETURN) = Q1CONTENTS_LAVA;
	else if (cont & FTECONTENTS_SLIME)
		G_FLOAT(OFS_RETURN) = Q1CONTENTS_SLIME;
	else if (cont & FTECONTENTS_WATER)
		G_FLOAT(OFS_RETURN) = Q1CONTENTS_WATER;
	else
		G_FLOAT(OFS_RETURN) = Q1CONTENTS_EMPTY;
}

static int CS_FindModel(const char *name, int *free)
{
	int i;

	*free = 0;

	if (!name || !*name)
		return 0;

	name = Mod_FixName(name, csqc_world.worldmodel->name);

	for (i = 1; i < MAX_CSMODELS; i++)
	{
		if (!*cl.model_csqcname[i])
		{
			*free = -i;
			break;
		}
		if (!strcmp(cl.model_csqcname[i], name))
			return -i;
	}
	for (i = 1; i < MAX_PRECACHE_MODELS; i++)
	{
		if (!strcmp(cl.model_name[i], name))
			return i;
	}
	return 0;
}

static model_t *csqc_setmodel(pubprogfuncs_t *prinst, csqcedict_t *ent, int modelindex)
{
	model_t *model;
	
	if (ent->readonly)
	{
		Con_Printf("setmodel on readonly entity %i\n", ent->entnum);
		return NULL;
	}

	ent->v->modelindex = modelindex;
	if (modelindex < 0)
	{
		if (modelindex <= -MAX_CSMODELS)
			return NULL;
		prinst->SetStringField(prinst, (void*)ent, &ent->v->model, cl.model_csqcname[-modelindex], true);
		if (!cl.model_csqcprecache[-modelindex])
			cl.model_csqcprecache[-modelindex] = Mod_ForName(Mod_FixName(cl.model_csqcname[-modelindex], csqc_world.worldmodel->publicname), MLV_WARN);
		model = cl.model_csqcprecache[-modelindex];
	}
	else
	{
		if (modelindex >= MAX_PRECACHE_MODELS)
			return NULL;
		prinst->SetStringField(prinst, (void*)ent, &ent->v->model, cl.model_name[modelindex], true);
		model = cl.model_precache[modelindex];
	}
	if (model)
	{
		//csqc probably needs to know the actual model size for any entity. it might as well.
		while(model->loadstate == MLS_LOADING)
			COM_WorkerPartialSync(model, &model->loadstate, MLS_LOADING);

		VectorCopy(model->mins, ent->v->mins);
		VectorCopy(model->maxs, ent->v->maxs);
		VectorSubtract (model->maxs, model->mins, ent->v->size);

		if (!ent->entnum)
		{
			cl.worldmodel = r_worldentity.model = csqc_world.worldmodel = model;
			csqc_worldchanged = true;

			VectorAdd(ent->v->origin, ent->v->mins, ent->v->absmin);
			VectorAdd(ent->v->origin, ent->v->maxs, ent->v->absmax);
		}
	}
	else
	{
		VectorClear(ent->v->mins);
		VectorClear(ent->v->maxs);
	}

	World_LinkEdict(&csqc_world, (wedict_t*)ent, false);

	return model;
}

static void QCBUILTIN PF_cs_SetModel(pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	csqcedict_t *ent = (void*)G_EDICT(prinst, OFS_PARM0);
	const char *modelname = PR_GetStringOfs(prinst, OFS_PARM1);
	int freei;
	int modelindex = CS_FindModel(modelname, &freei);
	model_t *mod;

	if (!modelindex && modelname && *modelname)
	{
		if (!freei)
			Host_EndGame("CSQC ran out of model slots\n");
		Con_DPrintf("Late caching model \"%s\"\n", modelname);
		Q_strncpyz(cl.model_csqcname[-freei], modelname, sizeof(cl.model_csqcname[-freei]));	//allocate a slot now
		modelindex = freei;

		cl.model_csqcprecache[-freei] = NULL;
	}

	mod = csqc_setmodel(prinst, ent, modelindex);
	if (mod)
		ent->xv->modelflags = mod->flags;
}
static void QCBUILTIN PF_cs_SetModelIndex(pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	csqcedict_t *ent = (void*)G_EDICT(prinst, OFS_PARM0);
	int modelindex = G_FLOAT(OFS_PARM1);

	csqc_setmodel(prinst, ent, modelindex);
}
static int PF_cs_PrecacheModel_Internal(pubprogfuncs_t *prinst, const char *modelname, qboolean queryonly)
{
	int modelindex, freei;
	const char *fixedname;
	int i;

	if (!*modelname)
		return 0;

	fixedname = Mod_FixName(modelname, csqc_world.worldmodel->publicname);

	for (i = 1; i < MAX_PRECACHE_MODELS; i++)	//Make sure that the server specified model is loaded..
	{
		if (!*cl.model_name[i])
			break;
		if (!strcmp(cl.model_name[i], fixedname))
		{
			if (!cl.model_precache[i])
				cl.model_precache[i] = Mod_ForName(cl.model_name[i], MLV_WARN);
			break;
		}
	}

	modelindex = CS_FindModel(modelname, &freei);	//now load it

	if (!modelindex && !queryonly)
	{
		if (!freei)
			Host_EndGame("CSQC ran out of model slots\n");
		fixedname = Mod_FixName(modelname, csqc_world.worldmodel->publicname);
		Q_strncpyz(cl.model_csqcname[-freei], fixedname, sizeof(cl.model_csqcname[-freei]));	//allocate a slot now
		modelindex = freei;

		CL_CheckOrEnqueDownloadFile(modelname, modelname, 0);
		cl.model_csqcprecache[-freei] = Mod_ForName(fixedname, MLV_WARN);
	}

	return modelindex;
}
static void QCBUILTIN PF_cs_PrecacheModel (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	const char	*s = PR_GetStringOfs(prinst, OFS_PARM0);

	G_INT(OFS_RETURN) = G_INT(OFS_PARM0);
	PF_cs_PrecacheModel_Internal(prinst, s, false);
}

static void QCBUILTIN PF_cs_getmodelindex (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	const char	*s = PR_GetStringOfs(prinst, OFS_PARM0);
	qboolean queryonly = (prinst->callargc >= 2)?G_FLOAT(OFS_PARM1):false;

	G_FLOAT(OFS_RETURN) = PF_cs_PrecacheModel_Internal(prinst, s, queryonly);
}
static void QCBUILTIN PF_cs_precachefile(pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	const char *filename = PR_GetStringOfs(prinst, OFS_PARM0);
	G_FLOAT(OFS_RETURN) = CL_CheckOrEnqueDownloadFile(filename, NULL, 0);	
}
static void QCBUILTIN PF_cs_PrecacheSound(pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	const char *soundname = PR_GetStringOfs(prinst, OFS_PARM0);
	if (!*soundname)	//invalid
		return;
	Sound_CheckDownload(soundname);
	S_PrecacheSound(soundname);
}

static void QCBUILTIN PF_cs_ModelnameForIndex(pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	int modelindex = G_FLOAT(OFS_PARM0);

	if (modelindex < 0)
		G_INT(OFS_RETURN) = (int)PR_SetString(prinst, cl.model_csqcname[-modelindex]);
	else
		G_INT(OFS_RETURN) = (int)PR_SetString(prinst, cl.model_name[modelindex]);
}
void QCBUILTIN PF_cs_setcustomskin (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	csqcedict_t *ent = (void*)G_EDICT(prinst, OFS_PARM0);
	const char *fname = PR_GetStringOfs(prinst, OFS_PARM1);
	const char *skindata = PF_VarString(prinst, 2, pr_globals);

	if (ent->skinobject > 0)
	{
		Mod_WipeSkin(ent->skinobject, false);
		ent->skinobject = 0;
	}

	if (*fname || *skindata)
	{
		if (*skindata)
			ent->skinobject = Mod_ReadSkinFile(fname, skindata);
		else
			ent->skinobject = -(int)Mod_RegisterSkinFile(fname);
	}
}
void QCBUILTIN PF_cs_loadcustomskin (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	const char *fname = PR_GetStringOfs(prinst, OFS_PARM0);
	const char *skindata = PF_VarString(prinst, 1, pr_globals);

	if (*fname || *skindata)
	{
		if (*skindata)
			G_FLOAT(OFS_RETURN) = Mod_ReadSkinFile(fname, skindata);
		else
			G_FLOAT(OFS_RETURN) = -(int)Mod_RegisterSkinFile(fname);
	}
	else
		G_FLOAT(OFS_RETURN) = 0;
}
void QCBUILTIN PF_cs_releasecustomskin (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	int oldskin = G_FLOAT(OFS_PARM0);
	if (oldskin > 0)
		Mod_WipeSkin(oldskin, false);
}
void QCBUILTIN PF_cs_applycustomskin (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	csqcedict_t *ent = (void*)G_EDICT(prinst, OFS_PARM0);
	int newskin = G_FLOAT(OFS_PARM1);
	int oldskin = ent->skinobject;
	ent->skinobject = newskin;
	if (oldskin > 0)
		Mod_WipeSkin(oldskin, false);
}

static void QCBUILTIN PF_ReadByte(pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	if (!csqc_mayread)
	{
		CSQC_Abort("PF_ReadByte is not valid at this time");
		G_FLOAT(OFS_RETURN) = -1;
		return;
	}
	G_FLOAT(OFS_RETURN) = MSG_ReadByte();
}

static void QCBUILTIN PF_ReadChar(pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	if (!csqc_mayread)
	{
		CSQC_Abort("PF_ReadChar is not valid at this time");
		G_FLOAT(OFS_RETURN) = -1;
		return;
	}
	G_FLOAT(OFS_RETURN) = MSG_ReadChar();
}

static void QCBUILTIN PF_ReadShort(pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	if (!csqc_mayread)
	{
		CSQC_Abort("PF_ReadShort is not valid at this time");
		G_FLOAT(OFS_RETURN) = -1;
		return;
	}
	G_FLOAT(OFS_RETURN) = MSG_ReadShort();
}

static void QCBUILTIN PF_ReadEntityNum(pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	unsigned int val;
	if (!csqc_mayread)
	{
		CSQC_Abort("PF_ReadEntityNum is not valid at this time");
		G_FLOAT(OFS_RETURN) = -1;
		return;
	}
	val = MSGCL_ReadEntity();
	G_FLOAT(OFS_RETURN) = val;
}

static void QCBUILTIN PF_ReadLong(pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	if (!csqc_mayread)
	{
		CSQC_Abort("PF_ReadLong is not valid at this time");
		G_FLOAT(OFS_RETURN) = -1;
		return;
	}
	G_FLOAT(OFS_RETURN) = MSG_ReadLong();
}

static void QCBUILTIN PF_ReadCoord(pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	if (!csqc_mayread)
	{
		CSQC_Abort("PF_ReadCoord is not valid at this time");
		G_FLOAT(OFS_RETURN) = -1;
		return;
	}
	G_FLOAT(OFS_RETURN) = MSG_ReadCoord();
}

static void QCBUILTIN PF_ReadFloat(pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	if (!csqc_mayread)
	{
		CSQC_Abort("PF_ReadFloat is not valid at this time");
		G_FLOAT(OFS_RETURN) = -1;
		return;
	}
	G_FLOAT(OFS_RETURN) = MSG_ReadFloat();
}

static void QCBUILTIN PF_ReadString(pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	char *read;
	if (!csqc_mayread)
	{
		CSQC_Abort("PF_ReadString is not valid at this time");
		G_INT(OFS_RETURN) = 0;
		return;
	}
	read = MSG_ReadString();
	RETURN_TSTRING(read);
}

static void QCBUILTIN PF_ReadAngle(pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	if (!csqc_mayread)
	{
		CSQC_Abort("PF_ReadAngle is not valid at this time");
		G_FLOAT(OFS_RETURN) = -1;
		return;
	}
	G_FLOAT(OFS_RETURN) = MSG_ReadAngle();
}

//basically acts as a readstring, but with added precache (and download)
static void QCBUILTIN PF_ReadPicture(pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	char *imagename;
	unsigned short size;
	if (!csqc_mayread)
	{
		CSQC_Abort("PF_ReadPicture is not valid at this time");
		G_INT(OFS_RETURN) = 0;
		return;
	}
	imagename = MSG_ReadString();
	size = MSG_ReadShort();
	MSG_ReadSkip(size);

	//do the precache+download thing
	{
		shader_t *pic = R2D_SafeCachePic(imagename);
		char ext[8];

		//fixme: probably shouldn't block here.
		if ((!pic || !R_GetShaderSizes(pic, NULL, NULL, true)) && cls.state
#ifndef CLIENTONLY
			&& !sv.active
#endif
			&& *COM_FileExtension(imagename, ext, sizeof(ext)))	//only try to download it if it looks as though it contains a path.
			CL_CheckOrEnqueDownloadFile(imagename, imagename, 0);
	}

	RETURN_TSTRING(imagename);
}


static void QCBUILTIN PF_objerror (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	const char	*s;
	struct edict_s	*ed;

	s = PF_VarString(prinst, 0, pr_globals);
/*	Con_Printf ("======OBJECT ERROR in %s:\n%s\n", PR_GetString(pr_xfunction->s_name),s);
*/	ed = PROG_TO_EDICT(prinst, *csqcg.self);
/*	ED_Print (ed);
*/
	prinst->ED_Print(prinst, ed);
	Con_Printf("%s", s);

	if (developer.value)
		prinst->debug_trace = 2;
	else
	{
		ED_Free (prinst, ed);

		prinst->AbortStack(prinst);

		PR_BIError (prinst, "Program error: %s", s);
	}
}

static void QCBUILTIN PF_cs_setsensitivityscaler (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	in_sensitivityscale = G_FLOAT(OFS_PARM0);
}

static void QCBUILTIN PF_cs_boxparticles(pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	int effectnum = CL_TranslateParticleFromServer(G_FLOAT(OFS_PARM0));
//	csqcedict_t *ent	= (csqcedict_t*)G_EDICT(prinst, OFS_PARM1);
	float *org_from		= G_VECTOR(OFS_PARM2);
	float *org_to		= G_VECTOR(OFS_PARM3);
	float *vel_from		= G_VECTOR(OFS_PARM4);
	float *vel_to		= G_VECTOR(OFS_PARM5);
	float count			= G_FLOAT(OFS_PARM6);
	int flags			= (prinst->callargc < 7)?0:G_FLOAT(OFS_PARM7);

	if (flags & 128)
	{
		flags &= ~128;
		P_ParticleTrail(org_from, org_to, effectnum, 0, NULL, NULL);
	}
	else
	{
		P_RunParticleCube(effectnum, org_from, org_to, vel_from, vel_to, count, 0, true, 0);
	}

	if (flags)
		Con_DPrintf("PF_cs_boxparticles: flags & %x is not supported\n", flags);
}

static void QCBUILTIN PF_cs_pointparticles (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	int effectnum = G_FLOAT(OFS_PARM0);
	float *org = G_VECTOR(OFS_PARM1);
	float *vel = G_VECTOR(OFS_PARM2);
	float count = G_FLOAT(OFS_PARM3);

	if (prinst->callargc < 3)
		vel = vec3_origin;
	if (prinst->callargc < 4)
		count = 1;

	effectnum = CL_TranslateParticleFromServer(effectnum);
	P_RunParticleEffectType(org, vel, count, effectnum);
}

static void QCBUILTIN PF_cs_trailparticles (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	int efnum;
	csqcedict_t *ent;
	float *start = G_VECTOR(OFS_PARM2);
	float *end = G_VECTOR(OFS_PARM3);

	if ((unsigned int)G_INT(OFS_PARM1) >= MAX_EDICTS)
	{	//ents can't be negative, nor can they be huge (like floats are if expressed as an integer)
		efnum = G_FLOAT(OFS_PARM1);
		ent = (csqcedict_t*)G_EDICT(prinst, OFS_PARM0);
	}
	else
	{
		efnum = G_FLOAT(OFS_PARM0);
		ent = (csqcedict_t*)G_EDICT(prinst, OFS_PARM1);
	}
	efnum = CL_TranslateParticleFromServer(efnum);

	if (!ent->entnum)	//world trails are non-state-based.
		pe->ParticleTrail(start, end, efnum, 0, NULL, NULL);
	else
		pe->ParticleTrail(start, end, efnum, -ent->entnum, NULL, &ent->trailstate);
}

void CSQC_ResetTrails(void)
{
	pubprogfuncs_t *prinst = csqc_world.progs;
	int i;
	csqcedict_t	*ent;

	if (!prinst)
		return;

	for (i = 0; i < *prinst->parms->sv_num_edicts; i++)
	{
		ent = (csqcedict_t*)EDICT_NUM_PB(prinst, i);
		ent->trailstate = NULL;
	}
}

//0 for error, non-0 for success.
//>0 match server
//<0 are client-only.
static void QCBUILTIN PF_cs_particleeffectnum (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	int i;
	const char *effectname = PR_GetStringOfs(prinst, OFS_PARM0);

	G_FLOAT(OFS_RETURN) = 0;	//default to failure.

	//use the server's index first.
	for (i = 1; i < MAX_SSPARTICLESPRE && cl.particle_ssname[i]; i++)
	{
		if (!strcmp(cl.particle_ssname[i], effectname))
		{
			G_FLOAT(OFS_RETURN) = i;
			return;
		}
	}

	//then look for an existing client id
	for (i = 1; i < MAX_CSPARTICLESPRE && cl.particle_csname[i]; i++)
	{
		if (!strcmp(cl.particle_csname[i], effectname))
		{
			//effects can be in the list despite now being stale. they still take up a slot to avoid reuse as the qc can potentially still potentially reference it.
			//csqc needs to be able to detect a now-stale effect
			//if (cl.particle_csprecache[i] != P_INVALID)
				G_FLOAT(OFS_RETURN) = -i;
			return;
		}
	}
	//create if new
	if (i < MAX_CSPARTICLESPRE)
	{
		free(cl.particle_csname[i]);
		cl.particle_csname[i] = NULL;
		cl.particle_csprecache[i] = P_FindParticleType(effectname);
		//if (cl.particle_csprecache[i] != P_INVALID)
		{
			//it exists, allow it.
			cl.particle_csname[i] = strdup(effectname);
			G_FLOAT(OFS_RETURN) = -i;
		}
		cl.particle_csprecaches = true;
	}
}

static void QCBUILTIN PF_cs_particleeffectquery (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	int id = G_FLOAT(OFS_PARM0);
	qboolean body = G_FLOAT(OFS_PARM1);
	char retstr[8192];

	id = CL_TranslateParticleFromServer(id);

	if (pe->ParticleQuery && pe->ParticleQuery(id, body, retstr, sizeof(retstr)))
	{
		RETURN_TSTRING(retstr);
	}
	else
		G_INT(OFS_RETURN) = 0;
}

static void QCBUILTIN PF_cs_sendevent (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	csqcedict_t *ent;
	int i;
	const char *eventname = PR_GetStringOfs(prinst, OFS_PARM0);
	const char *argtypes = PR_GetStringOfs(prinst, OFS_PARM1);

	if (!cls.state)
		return;

	MSG_WriteByte(&cls.netchan.message, clcfte_qcrequest);

	for (i = 0; i < 6; i++)
	{
		if (argtypes[i] == 's')
		{
			MSG_WriteByte(&cls.netchan.message, ev_string);
			MSG_WriteString(&cls.netchan.message, PR_GetStringOfs(prinst, OFS_PARM2+i*3));
		}
		else if (argtypes[i] == 'f')
		{
			MSG_WriteByte(&cls.netchan.message, ev_float);
			MSG_WriteFloat(&cls.netchan.message, G_FLOAT(OFS_PARM2+i*3));
		}
		else if (argtypes[i] == 'i')
		{
			MSG_WriteByte(&cls.netchan.message, ev_integer);
			MSG_WriteLong(&cls.netchan.message, G_INT(OFS_PARM2+i*3));
		}
		else if (argtypes[i] == 'v')
		{
			MSG_WriteByte(&cls.netchan.message, ev_vector);
			MSG_WriteFloat(&cls.netchan.message, G_FLOAT(OFS_PARM2+i*3+0));
			MSG_WriteFloat(&cls.netchan.message, G_FLOAT(OFS_PARM2+i*3+1));
			MSG_WriteFloat(&cls.netchan.message, G_FLOAT(OFS_PARM2+i*3+2));
		}
		else if (argtypes[i] == 'e')
		{
			ent = (csqcedict_t*)G_EDICT(prinst, OFS_PARM2+i*3);
			MSG_WriteByte(&cls.netchan.message, ev_entity);
			MSG_WriteEntity(&cls.netchan.message, ent->xv->entnum);
		}
		else
			break;
	}
	if (csqc_playerseat > 0)
		MSG_WriteByte(&cls.netchan.message, 200+csqc_playerseat);
	MSG_WriteByte(&cls.netchan.message, 0);
	MSG_WriteString(&cls.netchan.message, eventname);
}

static void cs_set_input_state (usercmd_t *cmd)
{
	if (csqcg.input_timelength)
		*csqcg.input_timelength = cmd->msec/1000.0f * cl.gamespeed;
	if (csqcg.input_angles)
	{
		csqcg.input_angles[0] = SHORT2ANGLE(cmd->angles[0]);
		csqcg.input_angles[1] = SHORT2ANGLE(cmd->angles[1]);
		csqcg.input_angles[2] = SHORT2ANGLE(cmd->angles[2]);
	}
	if (csqcg.input_movevalues)
	{
		csqcg.input_movevalues[0] = cmd->forwardmove;
		csqcg.input_movevalues[1] = cmd->sidemove;
		csqcg.input_movevalues[2] = cmd->upmove;
	}
	if (csqcg.input_buttons)
		*csqcg.input_buttons = cmd->buttons;

	if (csqcg.input_impulse)
		*csqcg.input_impulse = cmd->impulse;
	if (csqcg.input_lightlevel)
		*csqcg.input_lightlevel = cmd->lightlevel;
	if (csqcg.input_weapon)
		*csqcg.input_weapon = cmd->weapon;
	if (csqcg.input_servertime)
		*csqcg.input_servertime = cmd->fservertime;
	if (csqcg.input_clienttime)
		*csqcg.input_clienttime = cmd->fclienttime/1000.0f;

	if (csqcg.input_cursor_screen)
	{
		Vector2Copy(cmd->cursor_screen, csqcg.input_cursor_screen);
		csqcg.input_cursor_screen[2] = 0;
	}
	if (csqcg.input_cursor_start)
		VectorCopy(cmd->cursor_start, csqcg.input_cursor_start);
	if (csqcg.input_cursor_impact)
		VectorCopy(cmd->cursor_impact, csqcg.input_cursor_impact);
	if (csqcg.input_cursor_entitynumber)
		*csqcg.input_cursor_entitynumber = cmd->cursor_entitynumber;
}

static void cs_get_input_state (usercmd_t *cmd)
{
	if (csqcg.input_timelength)
		cmd->msec = *csqcg.input_timelength*1000;
	if (csqcg.input_angles)
	{
		cmd->angles[0] = ANGLE2SHORT(csqcg.input_angles[0]);
		cmd->angles[1] = ANGLE2SHORT(csqcg.input_angles[1]);
		cmd->angles[2] = ANGLE2SHORT(csqcg.input_angles[2]);
	}
	if (csqcg.input_movevalues)
	{
		cmd->forwardmove = csqcg.input_movevalues[0];
		cmd->sidemove = csqcg.input_movevalues[1];
		cmd->upmove = csqcg.input_movevalues[2];
	}
	if (csqcg.input_buttons)
		cmd->buttons = *csqcg.input_buttons;

	if (csqcg.input_impulse)
		cmd->impulse = *csqcg.input_impulse;
	if (csqcg.input_lightlevel)
		cmd->lightlevel = *csqcg.input_lightlevel;
	if (csqcg.input_weapon)
		cmd->weapon = *csqcg.input_weapon;
	if (csqcg.input_servertime)
		cmd->fservertime = *csqcg.input_servertime;

	if (csqcg.input_cursor_screen)
		Vector2Copy(csqcg.input_cursor_screen, cmd->cursor_screen);
	if (csqcg.input_cursor_start)
		VectorCopy(csqcg.input_cursor_start, cmd->cursor_start);
	if (csqcg.input_cursor_impact)
		VectorCopy(csqcg.input_cursor_impact, cmd->cursor_impact);
	if (csqcg.input_cursor_entitynumber)
		cmd->cursor_entitynumber = *csqcg.input_cursor_entitynumber;
}

//get the input commands, and stuff them into some globals.
static void QCBUILTIN PF_cs_getinputstate (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	usercmd_t *cmd;
	usercmd_t tmp;
	extern usercmd_t cl_pendingcmd[MAX_SPLITS];
	int f = G_FLOAT(OFS_PARM0);
	int seat = ((prinst->callargc>1)?G_FLOAT(OFS_PARM1):csqc_playerseat);

	if (seat < 0 || seat >= MAX_SPLITS)
	{
		G_FLOAT(OFS_RETURN) = false;
		return;
	}

	f = G_FLOAT(OFS_PARM0);
	if (cl.paused && f >= cl.ackedmovesequence)
	{
		G_FLOAT(OFS_RETURN) = false;
		return;
	}

	/*outgoing_sequence says how many packets have actually been sent, but there's an extra pending packet which has not been sent yet - be warned though, its data will change in the coming frames*/
	if (f == cl.movesequence)
	{
		int i;
		cmd = &cl_pendingcmd[seat];

		tmp = *cmd;
		cmd = &tmp;
		for (i=0 ; i<3 ; i++)
			cmd->angles[i] = ((int)(csqc_playerview->viewangles[i]*65536.0/360)&65535);
		if (!cmd->msec)
			*cmd = cl.outframes[(f-1)&UPDATE_MASK].cmd[seat];
		cmd->msec = (realtime - cl.outframes[(f-1)&UPDATE_MASK].senttime)*1000;
	}
	else
	{
		if (cl.outframes[f&UPDATE_MASK].cmd_sequence != f)
		{
			G_FLOAT(OFS_RETURN) = false;
			return;
		}
		cmd = &cl.outframes[f&UPDATE_MASK].cmd[seat];
	}

	cs_set_input_state(cmd);

	G_FLOAT(OFS_RETURN) = true;
}

//read lots of globals, run the default player physics, write lots of globals.
//not intended to affect client state at all
static void QCBUILTIN PF_cs_runplayerphysics (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	float msecs;
	float oldtime = *csqcg.simtime;

	csqcedict_t *ent = (void*)G_EDICT(prinst, OFS_PARM0);
	int mt = ent->v->movetype;
	if (prinst->callargc < 1)
	{
		csqc_deprecated("runplayerphysics with no ent");
		return;
	}
	if (ent->readonly)
	{
		csqc_deprecated("runplayerphysics called on read-only entity");
		return;
	}

	if (!cl.worldmodel)
		return;	//urm..

	//debugging field
	pmove.sequence = *csqcg.clientcommandframe;

	pmove.jump_msec = 0;//(cls.z_ext & Z_EXT_PM_TYPE) ? 0 : from->jump_msec;

//set up the movement command
	msecs = *csqcg.input_timelength*1000;
	//precision inaccuracies. :(
	pmove.cmd.angles[0] = ANGLE2SHORT(csqcg.input_angles[0]);
	pmove.cmd.angles[1] = ANGLE2SHORT(csqcg.input_angles[1]);
	pmove.cmd.angles[2] = ANGLE2SHORT(csqcg.input_angles[2]);
	VectorCopy(csqcg.input_angles, pmove.angles);

	pmove.cmd.forwardmove = csqcg.input_movevalues[0];
	pmove.cmd.sidemove = csqcg.input_movevalues[1];
	pmove.cmd.upmove = csqcg.input_movevalues[2];
	pmove.cmd.buttons = *csqcg.input_buttons;
	pmove.safeorigin_known = false;
	pmove.capsule = false;	//FIXME

	if (ent->xv->entnum)
		pmove.skipent = ent->xv->entnum;
	else
		pmove.skipent = -1;
	mt &= 255;
	switch(mt)
	{
	default:
	case MOVETYPE_WALK:
		pmove.pm_type = PM_NORMAL;
		break;
	case MOVETYPE_NOCLIP:
		pmove.pm_type = PM_SPECTATOR;
		break;
	case MOVETYPE_FLY_WORLDONLY:
	case MOVETYPE_FLY:
		pmove.pm_type = PM_FLY;
		break;
	}
	pmove.jump_held = (int)ent->xv->pmove_flags & PMF_JUMP_HELD;
	pmove.waterjumptime = 0;
	pmove.onground = (int)ent->v->flags & FL_ONGROUND;
	VectorCopy(ent->v->origin, pmove.origin);
	VectorCopy(ent->v->velocity, pmove.velocity);
	VectorCopy(ent->v->maxs, pmove.player_maxs);
	VectorCopy(ent->v->mins, pmove.player_mins);

	CL_SetSolidEntities();

	while(msecs > 0)	//break up longer commands
	{
		pmove.cmd.msec = msecs;
		if (pmove.cmd.msec > 50)
			pmove.cmd.msec = 50;
		msecs -= pmove.cmd.msec;
		PM_PlayerMove(1);
	}

	VectorCopy(pmove.angles, ent->v->angles);
	ent->v->angles[0] *= r_meshpitch.value * 1/3.0f;	//FIXME
	VectorCopy(pmove.origin, ent->v->origin);
	VectorCopy(pmove.velocity, ent->v->velocity);
	if (pmove.onground)
		ent->v->flags = (int)ent->v->flags | FL_ONGROUND;
	else
		ent->v->flags = (int)ent->v->flags & ~FL_ONGROUND;
	ent->xv->pmove_flags = 0;
	ent->xv->pmove_flags += pmove.jump_held ? PMF_JUMP_HELD : 0;
	ent->xv->pmove_flags += pmove.onladder ? PMF_LADDER : 0;

	//fixme: touch triggers?
	World_LinkEdict (&csqc_world, (wedict_t*)ent, true);

	*csqcg.simtime = oldtime;
}

static void QCBUILTIN PF_cs_getentitytoken (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	if (prinst->callargc)
	{
		const char *s = PR_GetStringOfs(prinst, OFS_PARM0);
		if (*s == 0 && cl.worldmodel)
		{
			if (cl.worldmodel->loadstate == MLS_LOADING)
				COM_WorkerPartialSync(cl.worldmodel, &cl.worldmodel->loadstate, MLS_LOADING);
			s = Mod_GetEntitiesString(cl.worldmodel);
		}
		csqcmapentitydata = s;
		G_INT(OFS_RETURN) = 0;
		return;
	}

	if (!csqcmapentitydata)
	{
		//nothing more to parse
		G_INT(OFS_RETURN) = 0;
	}
	else
	{
		char *QCC_COM_Parse (const char *data);
		extern char qcc_token[];
		csqcmapentitydata = QCC_COM_Parse(csqcmapentitydata);

		if (!csqcmapentitydata)	//hit the end
			G_INT(OFS_RETURN) = 0;
		else
			RETURN_TSTRING(qcc_token);
	}
}

static void CheckSendPings(void)
{	//quakeworld sends a 'pings' client command to retrieve the frequently updating stuff
	if (realtime - cl.last_ping_request > 2)
	{
		cl.last_ping_request = realtime;
		CL_SendClientCommand(true, "pings");
	}
}

static const char *PF_cs_serverkey_internal(const char *keyname)
{
	char *ret;
	static char adr[MAX_ADR_SIZE];

	if (!strcmp(keyname, "ip"))
	{
		if (cls.demoplayback)
			ret = cls.lastdemoname;
		else
			ret = NET_AdrToString(adr, sizeof(adr), &cls.netchan.remote_address);
	}
	else if (!strcmp(keyname, "servername"))
		ret = cls.servername;
	else if (!strcmp(keyname, "constate"))
	{
		if (cls.state == ca_disconnected
#ifndef CLIENTONLY 
			&& !sv.state
#endif
				)
			ret = "disconnected";
		else if (cls.state == ca_active)
			ret = "active";
		else
			ret = "connecting";
	}
	else if (!strcmp(keyname, "loadstate"))
	{
		extern int			total_loading_size, current_loading_size, loading_stage;
		extern char			levelshotname[MAX_QPATH];
		ret = va("%i %u %u \"%s\"", loading_stage, current_loading_size, total_loading_size, levelshotname);
	}
	else if (!strcmp(keyname, "transferring"))
	{
		ret = CL_TryingToConnect();
		if (!ret)
			ret = "";
	}
	else if (!strcmp(keyname, "maxplayers"))
	{
		ret = va("%i", cl.allocated_client_slots);
	}
	else if (!strcmp(keyname, "dlstate"))
	{
		if (!cl.downloadlist && !cls.download)
			ret = "";	//nothing being downloaded right now
		else
		{
			unsigned int fcount;
			qofs_t tsize;
			qboolean sizeextra;
			CL_GetDownloadSizes(&fcount, &tsize, &sizeextra);
			if (cls.download)	//downloading something
				ret = va("%u %g %u \"%s\" \"%s\" %g %i %g %g", fcount, (float)tsize, sizeextra?1u:0u, cls.download->localname, cls.download->remotename, cls.download->percent, cls.download->rate, (float)cls.download->completedbytes, (float)cls.download->size);
			else	//not downloading anything right now
				ret = va("%u %g %u", fcount, (float)tsize, sizeextra?1u:0u);
		}
	}
	else if (!strcmp(keyname, "pausestate"))
		ret = cl.paused?"1":"0";
	else if (!strcmp(keyname, "protocol"))
	{	//using this is pretty acedemic, really. Not particuarly portable.
		switch (cls.protocol)
		{	//a tokenizable string
			//first is the base game qw/nq
			//second is branch (custom engine name)
			//third is protocol version.
		default:
		case CP_UNKNOWN:
			ret = "Unknown";
			break;
		case CP_QUAKEWORLD:
			if (cls.fteprotocolextensions||cls.fteprotocolextensions2)
				ret = "QuakeWorld FTE";
			else if (cls.z_ext)
				ret = "QuakeWorld ZQuake";
			else
				ret = "QuakeWorld";
			break;
		case CP_NETQUAKE:
			switch (cls.protocol_nq)
			{
			default:
				ret = "NetQuake";
				break;
			case CPNQ_FITZ666:
				ret = "Fitz666";
				break;
			case CPNQ_DP5:
				ret = "NetQuake DarkPlaces 5";
				break;
			case CPNQ_DP6:
				ret = "NetQuake DarkPlaces 6";
				break;
			case CPNQ_DP7:
				ret = "NetQuake DarkPlaces 7";
				break;
			}
			break;
		case CP_QUAKE2:
			ret = "Quake2";
			break;
		case CP_QUAKE3:
			ret = "Quake3";
			break;
		case CP_PLUGIN:
			ret = "External";
			break;
		}
	}
	else if (!strcmp(keyname, "challenge"))
	{
		ret = va("%u", cls.challenge);
	}
	else
	{
#ifndef CLIENTONLY
		if (sv.state >= ss_loading)
		{
			ret = InfoBuf_ValueForKey(&svs.info, keyname);
			if (!*ret)
				ret = InfoBuf_ValueForKey(&svs.localinfo, keyname);
		}
		else
#endif
			ret = InfoBuf_ValueForKey(&cl.serverinfo, keyname);
	}
	return ret;
}
static const char *PF_cs_getplayerkey_internal (unsigned int pnum, const char *keyname)
{
	static char buffer[64];
	char *ret;

	if ((unsigned int)pnum >= (unsigned int)cl.allocated_client_slots)
		ret = "";
	else if (!strcmp(keyname, "viewentity"))	//compat with DP. Yes, I know this is in the wrong place. This is an evil hack.
	{
		ret = buffer;
		sprintf(ret, "%i", pnum+1);
	}
	else if (!*cl.players[pnum].name)
		ret = "";	//player isn't on the server.
	else if (!strcmp(keyname, "ping"))
	{
		CheckSendPings();

		ret = buffer;
		sprintf(ret, "%i", cl.players[pnum].ping);
	}
	else if (!strcmp(keyname, "frags"))
	{
		ret = buffer;
		sprintf(ret, "%i", cl.players[pnum].frags);
	}
	else if (!strcmp(keyname, "userid"))
	{
		ret = buffer;
		sprintf(ret, "%i", cl.players[pnum].userid);
	}
	else if (!strcmp(keyname, "pl"))	//packet loss
	{
		CheckSendPings();

		ret = buffer;
		sprintf(ret, "%i", cl.players[pnum].pl);
	}
	else if (!strcmp(keyname, "activetime"))	//packet loss
	{
		ret = buffer;
		sprintf(ret, "%f", realtime - cl.players[pnum].realentertime);
	}
//	else if (!strcmp(keyname, "entertime"))	//packet loss
//	{
//		ret = buffer;
//		sprintf(ret, "%i", (int)cl.players[pnum].entertime);
//	}
	else if (!strcmp(keyname, "topcolor_rgb"))	//packet loss
	{
		unsigned int col = cl.players[pnum].dtopcolor;
		ret = buffer;
		if (col < 16)
		{
			col = Sbar_ColorForMap(col);
			sprintf(ret, "'%g %g %g'", host_basepal[col*3+0]/255.0, host_basepal[col*3+1]/255.0, host_basepal[col*3+2]/255.0);
		}
		else
			sprintf(ret, "'%g %g %g'", ((col&0xff0000)>>16)/255.0, ((col&0x00ff00)>>8)/255.0, ((col&0x0000ff)>>0)/255.0);
	}
	else if (!strcmp(keyname, "bottomcolor_rgb"))	//packet loss
	{
		unsigned int col = cl.players[pnum].dbottomcolor;
		ret = buffer;
		if (col < 16)
		{
			col = Sbar_ColorForMap(col);
			sprintf(ret, "'%g %g %g'", host_basepal[col*3+0]/255.0, host_basepal[col*3+1]/255.0, host_basepal[col*3+2]/255.0);
		}
		else
			sprintf(ret, "'%g %g %g'", ((col&0xff0000)>>16)/255.0, ((col&0x00ff00)>>8)/255.0, ((col&0x0000ff)>>0)/255.0);
	}
#ifndef NOLEGACY
	else if (csqc_isdarkplaces && !strcmp(keyname, "colors"))	//checks to see if a player has locally been set to ignored (for text chat)
	{
		ret = buffer;
		sprintf(ret, "%i", cl.players[pnum].ttopcolor + cl.players[pnum].tbottomcolor*16);
	}
#endif
	else if (!strcmp(keyname, "ignored"))	//checks to see if a player has locally been set to ignored (for text chat)
	{
		ret = buffer;
		sprintf(ret, "%i", (int)cl.players[pnum].ignored);
	}
#ifdef VOICECHAT
	else if (!strcmp(keyname, "vignored"))	//checks to see this player's voicechat is ignored.
	{
		ret = buffer;
		sprintf(ret, "%i", (int)cl.players[pnum].vignored);
	}
	else if (!strcmp(keyname, "voipspeaking"))
	{
		ret = buffer;
		sprintf(ret, "%i", S_Voip_Speaking(pnum));
	}
	else if (!strcmp(keyname, "voiploudness"))
	{
		ret = buffer;
		if (pnum == csqc_playerview->playernum)
			sprintf(ret, "%i", S_Voip_Loudness(false));
		else
			*ret = 0;
	}
#endif
	else
	{
		ret = InfoBuf_ValueForKey(&cl.players[pnum].userinfo, keyname);
	}
	return ret;
}

//string(string keyname)
static void QCBUILTIN PF_cs_serverkey (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	const char *keyname = PF_VarString(prinst, 0, pr_globals);
	const char *ret = PF_cs_serverkey_internal(keyname);
	if (*ret)
		RETURN_TSTRING(ret);
	else
		G_INT(OFS_RETURN) = 0;
}
static void QCBUILTIN PF_cs_serverkeyfloat (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	const char *keyname = PR_GetStringOfs(prinst, OFS_PARM0);
	const char *ret = PF_cs_serverkey_internal(keyname);
	if (*ret)
		G_FLOAT(OFS_RETURN) = strtod(ret, NULL);
	else
		G_FLOAT(OFS_RETURN) = (prinst->callargc >= 2)?G_FLOAT(OFS_PARM1):0;
}
static void QCBUILTIN PF_cs_serverkeyblob (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	const char *keyname = PR_GetStringOfs(prinst, OFS_PARM0);
	int qcptr = G_INT(OFS_PARM1);
	int qcsize = G_INT(OFS_PARM2);
	void *ptr;
	size_t blobsize = 0;
	const char *blob;

	if (qcptr < 0 || qcptr+qcsize >= prinst->stringtablesize)
	{
		PR_BIError(prinst, "PF_cs_serverkeyblob: invalid pointer\n");
		return;
	}
	ptr = (struct reverbproperties_s*)(prinst->stringtable + qcptr);

	blob = InfoBuf_BlobForKey(&cl.serverinfo, keyname, &blobsize);

	if (qcptr)
	{
		blobsize = min(blobsize, qcsize);
		memcpy(ptr, blob, blobsize);
		G_INT(OFS_RETURN) = blobsize;
	}
	else
		G_INT(OFS_RETURN) = blobsize;
}

//string(float pnum, string keyname)
static void QCBUILTIN PF_cs_getplayerkeystring (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	int pnum = G_FLOAT(OFS_PARM0);
	const char *keyname = PR_GetStringOfs(prinst, OFS_PARM1);
	const char *ret;
	if (pnum < 0)
	{
		if (csqc_resortfrags)
		{
			Sbar_SortFrags(true, false);
			csqc_resortfrags = false;
		}
		if (pnum >= -scoreboardlines)
		{//sort by
			pnum = fragsort[-(pnum+1)];
		}
	}

	ret = PF_cs_getplayerkey_internal(pnum, keyname);
	if (*ret)
		RETURN_TSTRING(ret);
	else
		G_INT(OFS_RETURN) = 0;
}
static void QCBUILTIN PF_cs_getplayerkeyfloat (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	int pnum = G_FLOAT(OFS_PARM0);
	const char *keyname = PR_GetStringOfs(prinst, OFS_PARM1);
	const char *ret;
	if (pnum < 0)
	{
		if (csqc_resortfrags)
		{
			Sbar_SortFrags(true, false);
			csqc_resortfrags = false;
		}
		if (pnum >= -scoreboardlines)
		{//sort by
			pnum = fragsort[-(pnum+1)];
		}
	}

	ret = PF_cs_getplayerkey_internal(pnum, keyname);
	if (*ret)
		G_FLOAT(OFS_RETURN) = strtod(ret, NULL);
	else
		G_FLOAT(OFS_RETURN) = (prinst->callargc >= 3)?G_FLOAT(OFS_PARM2):0;
}
static void QCBUILTIN PF_cs_getplayerkeyblob (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	int pnum = G_FLOAT(OFS_PARM0);
	const char *keyname = PR_GetStringOfs(prinst, OFS_PARM1);
	int qcptr = G_INT(OFS_PARM2);
	int qcsize = G_INT(OFS_PARM3);
	void *ptr;
	if (pnum < 0)
	{
		if (csqc_resortfrags)
		{
			Sbar_SortFrags(true, false);
			csqc_resortfrags = false;
		}
		if (pnum >= -scoreboardlines)
		{//sort by
			pnum = fragsort[-(pnum+1)];
		}
	}

	if (qcptr < 0 || qcptr+qcsize >= prinst->stringtablesize)
	{
		PR_BIError(prinst, "PF_cs_getplayerkeyblob: invalid pointer\n");
		return;
	}
	ptr = (struct reverbproperties_s*)(prinst->stringtable + qcptr);

	if ((unsigned int)pnum >= (unsigned int)cl.allocated_client_slots)
		G_INT(OFS_RETURN) = 0;
	else
	{
		size_t blobsize = 0;
		const char *blob = InfoBuf_BlobForKey(&cl.players[pnum].userinfo, keyname, &blobsize);

		if (qcptr)
		{
			blobsize = min(blobsize, qcsize);
			memcpy(ptr, blob, blobsize);
			G_INT(OFS_RETURN) = blobsize;
		}
		else
			G_INT(OFS_RETURN) = blobsize;
	}
}

static void QCBUILTIN PF_cs_infokey (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	csqcedict_t *ent = (csqcedict_t*)G_EDICT(prinst, OFS_PARM0);
	const char *keyname = PR_GetStringOfs(prinst, OFS_PARM1);
	const char *ret;
	if (!ent->entnum)
		ret = PF_cs_serverkey_internal(keyname);
	else
	{
		int pnum = ent->xv->entnum-1;	//figure out which ssqc entity this is meant to be
		if (pnum < 0)
		{
			csqc_deprecated("infokey: entity does not correlate to an ssqc entity\n");
			ret = "";
		}
		else if (pnum >= cl.allocated_client_slots)
		{
			csqc_deprecated("infokey: entity does not correlate to an ssqc player entity\n");
			ret = "";
		}
		else
			ret = PF_cs_getplayerkey_internal(pnum, keyname);
	}
	if (*ret)
		RETURN_TSTRING(ret);
	else
		G_INT(OFS_RETURN) = 0;
}

static void QCBUILTIN PF_checkextension (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	const char *extname = PR_GetStringOfs(prinst, OFS_PARM0);
	int i;
	for (i = 0; i < QSG_Extensions_count; i++)
	{
		if (!QSG_Extensions[i].name)
			continue;

		if (i < 32 && cls.protocol == CP_QUAKEWORLD)
			if (!(cls.fteprotocolextensions & (1<<i)))
				continue;

		if (!strcmp(QSG_Extensions[i].name, extname))
		{
			G_FLOAT(OFS_RETURN) = true;
			return;
		}
	}
	G_FLOAT(OFS_RETURN) = false;
}

void QCBUILTIN PF_soundupdate (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	wedict_t		*entity	= G_WEDICT(prinst, OFS_PARM0);
	int				channel	= G_FLOAT(OFS_PARM1);
	const char		*sample = PR_GetStringOfs(prinst, OFS_PARM2);
	float			volume = G_FLOAT(OFS_PARM3);
	float			attenuation = G_FLOAT(OFS_PARM4);
	float			pitchpct = (prinst->callargc >= 6)?G_FLOAT(OFS_PARM5)*0.01:0;
	unsigned int	flags = (prinst->callargc>=7)?G_FLOAT(OFS_PARM6):0;
	float			startoffset = (prinst->callargc>=8)?G_FLOAT(OFS_PARM7):0;

	sfx_t		*sfx = S_PrecacheSound(sample);
	vec3_t		org;

	VectorCopy(entity->v->origin, org);
	if (entity->v->solid == SOLID_BSP)
	{
		VectorMA(org, 0.5, entity->v->mins, org);
		VectorMA(org, 0.5, entity->v->maxs, org);
	}

	G_FLOAT(OFS_RETURN) = S_UpdateSound(-entity->entnum, channel, sfx, org, entity->v->velocity, volume, attenuation, startoffset, pitchpct, flags);
}
void QCBUILTIN PF_stopsound (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	wedict_t	*entity	= G_WEDICT(prinst, OFS_PARM0);
	int			channel	= G_FLOAT(OFS_PARM1);

	S_StopSound(-entity->entnum, channel);
}
void QCBUILTIN PF_getsoundtime (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	wedict_t	*entity	= G_WEDICT(prinst, OFS_PARM0);
	int			channel	= G_FLOAT(OFS_PARM1);

	G_FLOAT(OFS_RETURN) = S_GetSoundTime(-entity->entnum, channel);
}
static void QCBUILTIN PF_cs_sound(pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	const char		*sample;
	int			channel;
	csqcedict_t		*entity;
	float volume;
	float attenuation;
	float pitchpct;
	unsigned int flags;
	float startoffset;

	sfx_t *sfx;
	vec3_t		org;

	entity = (csqcedict_t*)G_EDICT(prinst, OFS_PARM0);
	channel = G_FLOAT(OFS_PARM1);
	sample = PR_GetStringOfs(prinst, OFS_PARM2);
	volume = G_FLOAT(OFS_PARM3);
	attenuation = G_FLOAT(OFS_PARM4);
	pitchpct = (prinst->callargc>=6)?G_FLOAT(OFS_PARM5)*0.01:0;
	flags = (prinst->callargc>=7)?G_FLOAT(OFS_PARM6):0;
	startoffset = (prinst->callargc>=8)?G_FLOAT(OFS_PARM7):0;

	sfx = S_PrecacheSound(sample);

	VectorCopy(entity->v->origin, org);
	if (entity->v->solid == SOLID_BSP)
	{
		VectorMA(org, 0.5, entity->v->mins, org);
		VectorMA(org, 0.5, entity->v->maxs, org);
	}

	if (sfx)
		S_StartSound(-entity->entnum, channel, sfx, org, entity->v->velocity, volume, attenuation, startoffset, pitchpct, flags);
};

static void QCBUILTIN PF_cs_pointsound(pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	const char *sample;
	float *origin;
	float volume;
	float attenuation;
	float pitchpct;

	sfx_t *sfx;

	origin = G_VECTOR(OFS_PARM0);
	sample = PR_GetStringOfs(prinst, OFS_PARM1);
	volume = G_FLOAT(OFS_PARM2);
	attenuation = G_FLOAT(OFS_PARM3);
	if (prinst->callargc >= 5)
		pitchpct = G_FLOAT(OFS_PARM4)*0.01;
	else
		pitchpct = 0;

	sfx = S_PrecacheSound(sample);
	if (sfx)
		S_StartSound(0, 0, sfx, origin, NULL, volume, attenuation, 0, pitchpct, 0);
}

static void QCBUILTIN PF_cs_particle(pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	float *org = G_VECTOR(OFS_PARM0);
	float *dir = G_VECTOR(OFS_PARM1);
	float colour = G_FLOAT(OFS_PARM2);
	float count = G_FLOAT(OFS_PARM2);

	pe->RunParticleEffect(org, dir, colour, count);
}
static void QCBUILTIN PF_cs_particle2(pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	float		*org, *dmin, *dmax;
	float		colour;
	float		count;
	float    effect;

	org = G_VECTOR(OFS_PARM0);
	dmin = G_VECTOR(OFS_PARM1);
	dmax = G_VECTOR(OFS_PARM2);
	colour = G_FLOAT(OFS_PARM3);
	effect = G_FLOAT(OFS_PARM4);
	count = G_FLOAT(OFS_PARM5);

	pe->RunParticleEffect2 (org, dmin, dmax, colour, effect, count);
}

static void QCBUILTIN PF_cs_particle3(pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	float		*org, *box;
	float		colour;
	float		count;
	float    effect;

	org = G_VECTOR(OFS_PARM0);
	box = G_VECTOR(OFS_PARM1);
	colour = G_FLOAT(OFS_PARM2);
	effect = G_FLOAT(OFS_PARM3);
	count = G_FLOAT(OFS_PARM4);

	pe->RunParticleEffect3(org, box, colour, effect, count);
}

static void QCBUILTIN PF_cs_particle4(pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	float		*org;
	float		radius;
	float		colour;
	float		count;
	float    effect;

	org = G_VECTOR(OFS_PARM0);
	radius = G_FLOAT(OFS_PARM1);
	colour = G_FLOAT(OFS_PARM2);
	effect = G_FLOAT(OFS_PARM3);
	count = G_FLOAT(OFS_PARM4);

	pe->RunParticleEffect4(org, radius, colour, effect, count);
}


void QCBUILTIN PF_cl_effect(pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	float *org = G_VECTOR(OFS_PARM0);
	const char *name = PR_GetStringOfs(prinst, OFS_PARM1);
	float startframe = G_FLOAT(OFS_PARM2);
	float endframe = G_FLOAT(OFS_PARM3);
	float framerate = G_FLOAT(OFS_PARM4);
	model_t *mdl;

	mdl = Mod_ForName(name, MLV_WARN);
	if (mdl)
		CL_SpawnSpriteEffect(org, NULL, NULL, mdl, startframe, endframe, framerate, mdl->type==mod_sprite?-1:1, 1, 0, 0, P_INVALID, 0, 0);
	else
		Con_Printf("PF_cl_effect: Couldn't load model %s\n", name);
}

void QCBUILTIN PF_cl_ambientsound(pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	const char		*samp;
	float		*pos;
	float 		vol, attenuation;

	pos = G_VECTOR (OFS_PARM0);
	samp = PR_GetStringOfs(prinst, OFS_PARM1);
	vol = G_FLOAT(OFS_PARM2);
	attenuation = G_FLOAT(OFS_PARM3);

	S_StaticSound (S_PrecacheSound (samp), pos, vol, attenuation);
}

static void QCBUILTIN PF_cs_lightstyle (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	int stnum = G_FLOAT(OFS_PARM0);
	const char *str = PR_GetStringOfs(prinst, OFS_PARM1);
	vec3_t rgb = {1,1,1};
	
	if (prinst->callargc >= 3)	//fte is a quakeworld engine
		VectorCopy(G_VECTOR(OFS_PARM2), rgb);

	if ((unsigned)stnum >= MAX_LIGHTSTYLES)
	{
		Con_Printf ("PF_cs_lightstyle: stnum > MAX_LIGHTSTYLES");
		return;
	}
	R_UpdateLightStyle(stnum, str, rgb[0],rgb[1],rgb[2]);
}

static void QCBUILTIN PF_getlightstyle (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	unsigned int stnum = G_FLOAT(OFS_PARM0);

	if (stnum >= MAX_LIGHTSTYLES)
	{
		VectorSet(G_VECTOR(OFS_PARM1), 0, 0, 0);
		G_INT(OFS_RETURN) = 0;
		return;
	}

	VectorCopy(cl_lightstyle[stnum].colours, G_VECTOR(OFS_PARM1));
	if (!cl_lightstyle[stnum].length)
		G_INT(OFS_RETURN) = 0;
	else
		RETURN_TSTRING(cl_lightstyle[stnum].map);
}
static void QCBUILTIN PF_getlightstylergb (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	unsigned int stnum = G_FLOAT(OFS_PARM0);
	int value;	//could be float, but that would exceed the precision of R_AnimateLight

	if (stnum >= MAX_LIGHTSTYLES)
	{
		Con_Printf ("PF_getlightstyle: stnum > MAX_LIGHTSTYLES");
		return;
	}

	if (!cl_lightstyle[stnum].length)
		value = ('m'-'a')*22 * r_lightstylescale.value;
	else if (cl_lightstyle[stnum].map[0] == '=')
		value = atof(cl_lightstyle[stnum].map+1)*256*r_lightstylescale.value;
	else
	{
		int v1, v2, vd, i;
		float f;
		
		f = (cl.time*r_lightstylespeed.value);
		if (f < 0)
			f = 0;
		i = (int)f;
		f -= i;	//this can require updates at 1000 times a second.. Depends on your framerate of course

		v1 = i % cl_lightstyle[stnum].length;
		v1 = cl_lightstyle[stnum].map[v1] - 'a';
		v2 = (i+1) % cl_lightstyle[stnum].length;
		v2 = cl_lightstyle[stnum].map[v2] - 'a';
		vd = v1 - v2;
		if (!r_lightstylesmooth.ival || vd < -r_lightstylesmooth_limit.ival || vd > r_lightstylesmooth_limit.ival)
			value = v1*22*r_lightstylescale.value;
		else
			value = (v1*(1-f) + v2*(f))*22*r_lightstylescale.value;
	}

	VectorScale(cl_lightstyle[stnum].colours, value*(1.0/256), G_VECTOR(OFS_RETURN));
}

static void QCBUILTIN PF_cl_te_gunshot (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	float *pos = G_VECTOR(OFS_PARM0);
	float scaler = 1;
	if (prinst->callargc >= 2)	//fte is a quakeworld engine
		scaler = G_FLOAT(OFS_PARM1);
	if (P_RunParticleEffectType(pos, NULL, scaler, pt_gunshot))
		P_RunParticleEffect (pos, vec3_origin, 0, 20*scaler);
}
static void QCBUILTIN PF_cl_te_bloodqw (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	float *pos = G_VECTOR(OFS_PARM0);
	float scaler = 1;
	if (prinst->callargc >= 2)	//fte is a quakeworld engine
		scaler = G_FLOAT(OFS_PARM1);
	if (P_RunParticleEffectType(pos, NULL, scaler, ptqw_blood))
		if (P_RunParticleEffectType(pos, NULL, scaler, ptdp_blood))
			P_RunParticleEffect (pos, vec3_origin, 73, 20*scaler);
}
static void QCBUILTIN PF_cl_te_blooddp (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	float *pos = G_VECTOR(OFS_PARM0);
	float *dir = G_VECTOR(OFS_PARM1);
	float scaler = G_FLOAT(OFS_PARM2);

	if (P_RunParticleEffectType(pos, dir, scaler, ptdp_blood))
		if (P_RunParticleEffectType(pos, dir, scaler, ptqw_blood))
			P_RunParticleEffect (pos, dir, 73, 20*scaler);
}
static void QCBUILTIN PF_cl_te_lightningblood (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	float *pos = G_VECTOR(OFS_PARM0);
	if (P_RunParticleEffectType(pos, NULL, 1, ptqw_lightningblood))
		P_RunParticleEffect (pos, vec3_origin, 225, 50);
}
static void QCBUILTIN PF_cl_te_spike (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	float *pos = G_VECTOR(OFS_PARM0);
	if (P_RunParticleEffectType(pos, NULL, 1, pt_spike))
		if (P_RunParticleEffectType(pos, NULL, 10, pt_gunshot))
			P_RunParticleEffect (pos, vec3_origin, 0, 10);
}
static void QCBUILTIN PF_cl_te_superspike (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	float *pos = G_VECTOR(OFS_PARM0);
	if (P_RunParticleEffectType(pos, NULL, 1, pt_superspike))
		if (P_RunParticleEffectType(pos, NULL, 2, pt_spike))
			if (P_RunParticleEffectType(pos, NULL, 20, pt_gunshot))
				P_RunParticleEffect (pos, vec3_origin, 0, 20);
}
static void QCBUILTIN PF_cl_te_explosion (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	float *pos = G_VECTOR(OFS_PARM0);

	// light
	if (r_explosionlight.value) {
		dlight_t *dl;

		dl = CL_AllocDlight (0);
		VectorCopy (pos, dl->origin);
		dl->radius = 150 + r_explosionlight.value*200;
		dl->die = cl.time + 1;
		dl->decay = 300;

		dl->color[0] = 0.2;
		dl->color[1] = 0.155;
		dl->color[2] = 0.05;
		dl->channelfade[0] = 0.196;
		dl->channelfade[1] = 0.23;
		dl->channelfade[2] = 0.12;
	}

	if (P_RunParticleEffectType(pos, NULL, 1, pt_explosion))
		P_RunParticleEffect(pos, NULL, 107, 1024); // should be 97-111

	Surf_AddStain(pos, -1, -1, -1, 100);

	S_StartSound (0, 0, cl_sfx_r_exp3, pos, NULL, 1, 1, 0, 0, 0);
}
static void QCBUILTIN PF_cl_te_tarexplosion (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	float *pos = G_VECTOR(OFS_PARM0);
	P_RunParticleEffectType(pos, NULL, 1, pt_tarexplosion);

	S_StartSound (0, 0, cl_sfx_r_exp3, pos, NULL, 1, 1, 0, 0, 0);
}
static void QCBUILTIN PF_cl_te_wizspike (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	float *pos = G_VECTOR(OFS_PARM0);
	if (P_RunParticleEffectType(pos, NULL, 1, pt_wizspike))
		P_RunParticleEffect (pos, vec3_origin, 20, 30);

	S_StartSound (0, 0, cl_sfx_knighthit, pos, NULL, 1, 1, 0, 0, 0);
}
static void QCBUILTIN PF_cl_te_knightspike (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	float *pos = G_VECTOR(OFS_PARM0);
	if (P_RunParticleEffectType(pos, NULL, 1, pt_knightspike))
		P_RunParticleEffect (pos, vec3_origin, 226, 20);

	S_StartSound (0, 0, cl_sfx_knighthit, pos, NULL, 1, 1, 0, 0, 0);
}
static void QCBUILTIN PF_cl_te_lavasplash (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	float *pos = G_VECTOR(OFS_PARM0);
	P_RunParticleEffectType(pos, NULL, 1, pt_lavasplash);
}
static void QCBUILTIN PF_cl_te_teleport (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	float *pos = G_VECTOR(OFS_PARM0);
	P_RunParticleEffectType(pos, NULL, 1, pt_teleportsplash);
}
static void QCBUILTIN PF_cl_te_gunshotquad (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	float *pos = G_VECTOR(OFS_PARM0);
	if (P_RunParticleEffectTypeString(pos, vec3_origin, 1, "te_gunshotquad"))
		if (P_RunParticleEffectType(pos, NULL, 1, pt_gunshot))
			P_RunParticleEffect (pos, vec3_origin, 0, 20);
}
static void QCBUILTIN PF_cl_te_spikequad (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	float *pos = G_VECTOR(OFS_PARM0);
	if (P_RunParticleEffectTypeString(pos, vec3_origin, 1, "te_spikequad"))
		if (P_RunParticleEffectType(pos, NULL, 1, pt_spike))
			if (P_RunParticleEffectType(pos, NULL, 10, pt_gunshot))
				P_RunParticleEffect (pos, vec3_origin, 0, 10);
}
static void QCBUILTIN PF_cl_te_superspikequad (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	float *pos = G_VECTOR(OFS_PARM0);
	if (P_RunParticleEffectTypeString(pos, vec3_origin, 1, "te_superspikequad"))
		if (P_RunParticleEffectType(pos, NULL, 1, pt_superspike))
			if (P_RunParticleEffectType(pos, NULL, 2, pt_spike))
				if (P_RunParticleEffectType(pos, NULL, 20, pt_gunshot))
					P_RunParticleEffect (pos, vec3_origin, 0, 20);
}
static void QCBUILTIN PF_cl_te_explosionquad (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	float *pos = G_VECTOR(OFS_PARM0);
	if (P_RunParticleEffectTypeString(pos, vec3_origin, 1, "te_explosionquad"))
		if (P_RunParticleEffectType(pos, NULL, 1, pt_explosion))
			P_RunParticleEffect(pos, NULL, 107, 1024); // should be 97-111

	Surf_AddStain(pos, -1, -1, -1, 100);

	// light
	if (r_explosionlight.value) {
		dlight_t *dl;

		dl = CL_AllocDlight (0);
		VectorCopy (pos, dl->origin);
		dl->radius = 150 + r_explosionlight.value*200;
		dl->die = cl.time + 1;
		dl->decay = 300;

		dl->color[0] = 0.2;
		dl->color[1] = 0.155;
		dl->color[2] = 0.05;
		dl->channelfade[0] = 0.196;
		dl->channelfade[1] = 0.23;
		dl->channelfade[2] = 0.12;
	}

	S_StartSound (0, 0, cl_sfx_r_exp3, pos, NULL, 1, 1, 0, 0, 0);
}

//void(vector org, float radius, float lifetime, vector color) te_customflash
static void QCBUILTIN PF_cl_te_customflash (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	float *org = G_VECTOR(OFS_PARM0);
	float radius = G_FLOAT(OFS_PARM1);
	float lifetime = G_FLOAT(OFS_PARM2);
	float *colour = G_VECTOR(OFS_PARM3);

	dlight_t *dl;
	// light
	dl = CL_AllocDlight (0);
	VectorCopy (org, dl->origin);
	dl->radius = radius;
	dl->die = cl.time + lifetime;
	dl->decay = dl->radius / lifetime;
	dl->color[0] = colour[0]*0.5f;
	dl->color[1] = colour[1]*0.5f;
	dl->color[2] = colour[2]*0.5f;
}

static void QCBUILTIN PF_cl_te_bloodshower (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	float *minb = G_VECTOR(OFS_PARM0);
	float *maxb = G_VECTOR(OFS_PARM1);
	vec3_t vel = {0,0,-G_FLOAT(OFS_PARM2)};
	float howmany = G_FLOAT(OFS_PARM3);
	
	P_RunParticleCube(P_FindParticleType("te_bloodshower"), minb, maxb, vel, vel, howmany, 0, false, 0);
}
static void QCBUILTIN PF_cl_te_particlecube (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	float *minb = G_VECTOR(OFS_PARM0);
	float *maxb = G_VECTOR(OFS_PARM1);
	float *vel = G_VECTOR(OFS_PARM2);
	float howmany = G_FLOAT(OFS_PARM3);
	float color = G_FLOAT(OFS_PARM4);
	float gravity = G_FLOAT(OFS_PARM5);
	float jitter = G_FLOAT(OFS_PARM6);

	P_RunParticleCube(P_INVALID, minb, maxb, vel, vel, howmany, color, gravity, jitter);
}
static void QCBUILTIN PF_cl_te_spark (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	float *pos = G_VECTOR(OFS_PARM0);
	float *pos2 = G_VECTOR(OFS_PARM1);
	float cnt = G_FLOAT(OFS_PARM2);
	P_RunParticleEffectType(pos, pos2, cnt, ptdp_spark);
}
static void QCBUILTIN PF_cl_te_smallflash (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	float *pos = G_VECTOR(OFS_PARM0);
	dlight_t *dl = CL_AllocDlight (0);
	VectorCopy (pos, dl->origin);
	dl->radius = 200;
	dl->decay = 1000;
	dl->die = cl.time + 0.2;
	dl->color[0] = 2.0;
	dl->color[1] = 2.0;
	dl->color[2] = 2.0;
}
static void QCBUILTIN PF_cl_te_explosion2 (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	float *pos = G_VECTOR(OFS_PARM0);
	int colorStart = G_FLOAT(OFS_PARM1);
	int colorLength = G_FLOAT(OFS_PARM2);
	int ef = P_FindParticleType(va("TE_EXPLOSION2_%i_%i", colorStart, colorLength));
	if (ef == P_INVALID)
		ef = pt_explosion;
	P_RunParticleEffectType(pos, NULL, 1, ef);
	if (r_explosionlight.value)
	{
		dlight_t *dl = CL_AllocDlight (0);
		VectorCopy (pos, dl->origin);
		dl->radius = 350;
		dl->die = cl.time + 0.5;
		dl->decay = 300;
	}
	S_StartSound (0, 0, cl_sfx_r_exp3, pos, NULL, 1, 1, 0, 0, 0);
}

static void QCBUILTIN PF_cl_te_flamejet (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	vec3_t pos, vel;
	int count;

	// origin
	pos[0] = MSG_ReadCoord ();
	pos[1] = MSG_ReadCoord ();
	pos[2] = MSG_ReadCoord ();

	// velocity
	vel[0] = MSG_ReadCoord ();
	vel[1] = MSG_ReadCoord ();
	vel[2] = MSG_ReadCoord ();

	// count
	count = MSG_ReadByte ();

	if (P_RunParticleEffectType(pos, vel, count, P_FindParticleType("TE_FLAMEJET")))
		P_RunParticleEffect (pos, vel, 232, count);
}

static void QCBUILTIN PF_cl_te_lightning1 (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	csqcedict_t *ent = (csqcedict_t*)G_EDICT(prinst, OFS_PARM0);
	float *start = G_VECTOR(OFS_PARM1);
	float *end = G_VECTOR(OFS_PARM2);

	CL_AddBeam(BT_Q1LIGHTNING1, ent->entnum+MAX_EDICTS, start, end);
}
static void QCBUILTIN PF_cl_te_lightning2 (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	csqcedict_t *ent = (csqcedict_t*)G_EDICT(prinst, OFS_PARM0);
	float *start = G_VECTOR(OFS_PARM1);
	float *end = G_VECTOR(OFS_PARM2);

	CL_AddBeam(BT_Q1LIGHTNING2, ent->entnum+MAX_EDICTS, start, end);
}
static void QCBUILTIN PF_cl_te_lightning3 (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	csqcedict_t *ent = (csqcedict_t*)G_EDICT(prinst, OFS_PARM0);
	float *start = G_VECTOR(OFS_PARM1);
	float *end = G_VECTOR(OFS_PARM2);

	CL_AddBeam(BT_Q1LIGHTNING3, ent->entnum+MAX_EDICTS, start, end);
}
static void QCBUILTIN PF_cl_te_beam (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	csqcedict_t *ent = (csqcedict_t*)G_EDICT(prinst, OFS_PARM0);
	float *start = G_VECTOR(OFS_PARM1);
	float *end = G_VECTOR(OFS_PARM2);

	CL_AddBeam(BT_Q1BEAM, ent->entnum+MAX_EDICTS, start, end);
}
static void QCBUILTIN PF_cl_te_plasmaburn (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	float *pos = G_VECTOR(OFS_PARM0);

	if (P_RunParticleEffectType(pos, NULL, 1, P_FindParticleType("te_plasmaburn")))
		P_RunParticleEffect(pos, vec3_origin, 15, 50);
}
static void QCBUILTIN PF_cl_te_explosionrgb (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	float *org = G_VECTOR(OFS_PARM0);
	float *colour = G_VECTOR(OFS_PARM1);

	dlight_t *dl;

	if (P_RunParticleEffectType(org, NULL, 1, pt_explosion))
		P_RunParticleEffect(org, NULL, 107, 1024); // should be 97-111

	Surf_AddStain(org, -1, -1, -1, 100);

	// light
	if (r_explosionlight.value)
	{
		dl = CL_AllocDlight (0);
		VectorCopy (org, dl->origin);
		dl->radius = 150 + r_explosionlight.value*200;
		dl->die = cl.time + 0.5;
		dl->decay = 300;

		dl->color[0] = 0.4f*colour[0];
		dl->color[1] = 0.4f*colour[1];
		dl->color[2] = 0.4f*colour[2];
		dl->channelfade[0] = 0;
		dl->channelfade[1] = 0;
		dl->channelfade[2] = 0;
	}

	S_StartSound (0, 0, cl_sfx_r_exp3, org, NULL, 1, 1, 0, 0, 0);
}
static void QCBUILTIN PF_cl_te_particlerain (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	float *min = G_VECTOR(OFS_PARM0);
	float *max = G_VECTOR(OFS_PARM1);
	float *vel = G_VECTOR(OFS_PARM2);
	float howmany = G_FLOAT(OFS_PARM3);
	float colour = G_FLOAT(OFS_PARM4);

	P_RunParticleWeather(min, max, vel, howmany, colour, "rain");
}
static void QCBUILTIN PF_cl_te_particlesnow (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	float *min = G_VECTOR(OFS_PARM0);
	float *max = G_VECTOR(OFS_PARM1);
	float *vel = G_VECTOR(OFS_PARM2);
	float howmany = G_FLOAT(OFS_PARM3);
	float colour = G_FLOAT(OFS_PARM4);

	P_RunParticleWeather(min, max, vel, howmany, colour, "snow");
}

void CSQC_RunThreads(void)
{
	csqctreadstate_t *state = csqcthreads, *next;
	csqcthreads = NULL;
	while(state)
	{
		next = state->next;

		if (state->resumetime > cl.servertime)
		{	//not time yet, reform original list.
			state->next = csqcthreads;
			csqcthreads = state;
		}
		else
		{	//call it and forget it ever happened. The Sleep biltin will recreate if needed.


			*csqcg.self = EDICT_TO_PROG(csqcprogs, EDICT_NUM_UB(csqcprogs, state->self));
			*csqcg.other = EDICT_TO_PROG(csqcprogs, EDICT_NUM_UB(csqcprogs, state->other));

			csqcprogs->RunThread(csqcprogs, state->thread);
			csqcprogs->parms->memfree(state->thread);
			csqcprogs->parms->memfree(state);
		}

		state = next;
	}
}

static void QCBUILTIN PF_cs_addprogs (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	const char *s = PR_GetStringOfs(prinst, OFS_PARM0);
	int newp;
	if (!s || !*s)
		newp = -1;
	else
	{
		newp = PR_LoadProgs(prinst, s);
		if (newp >= 0)
			PR_ProgsAdded(csqcprogs, newp, s);
	}
	G_FLOAT(OFS_RETURN) = newp;
}

static void QCBUILTIN PF_cs_OpenPortal (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
/*
#ifdef Q2BSPS
	if (cl.worldmodel->fromgame == fg_quake2)
	{
		int portal;
		int state	= G_FLOAT(OFS_PARM1)!=0;
		if (G_INT(OFS_PARM1) >= MAX_EDICTS)
			portal = G_FLOAT(OFS_PARM0);	//old legacy crap.
		else
			portal = G_WEDICT(prinst, OFS_PARM0)->xv->style;	//read the func_areaportal's style field.
		CMQ2_SetAreaPortalState(cl.worldmodel, portal, state);
	}
#endif
*/
#ifdef Q3BSPS
	if (cl.worldmodel->fromgame == fg_quake3)
	{
		int state	= G_FLOAT(OFS_PARM1)!=0;
		wedict_t *portal = G_WEDICT(prinst, OFS_PARM0);
		int area1 = portal->pvsinfo.areanum, area2 = portal->pvsinfo.areanum2;
		if (area1 == area2 || area1<0 || area2<0)
			return;
		CMQ3_SetAreaPortalState(cl.worldmodel, portal->pvsinfo.areanum, portal->pvsinfo.areanum2, state);
	}
#endif
}

static void QCBUILTIN PF_cs_droptofloor (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	csqcedict_t		*ent;
	vec3_t		end;
	vec3_t		start;
	trace_t		trace;

	ent = (csqcedict_t*)PROG_TO_EDICT(prinst, *csqcg.self);

	VectorCopy (ent->v->origin, end);
	end[2] -= 512;

	VectorCopy (ent->v->origin, start);
	trace = World_Move (&csqc_world, start, ent->v->mins, ent->v->maxs, end, MOVE_NORMAL, (wedict_t*)ent);

	if (trace.fraction == 1 || trace.allsolid)
		G_FLOAT(OFS_RETURN) = 0;
	else
	{
		VectorCopy (trace.endpos, ent->v->origin);
		World_LinkEdict(&csqc_world, (wedict_t*)ent, false);
		ent->v->flags = (int)ent->v->flags | FL_ONGROUND;
		ent->v->groundentity = EDICT_TO_PROG(prinst, trace.ent);
		G_FLOAT(OFS_RETURN) = 1;
	}
}

static void QCBUILTIN PF_cl_getlight (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	vec3_t ambient, diffuse, dir;
	if (!cl.worldmodel || cl.worldmodel->loadstate != MLS_LOADED || !cl.worldmodel->funcs.LightPointValues)
		VectorSet(G_VECTOR(OFS_RETURN), 0, 0, 0);
	else
	{
		cl.worldmodel->funcs.LightPointValues(cl.worldmodel, G_VECTOR(OFS_PARM0), ambient, diffuse, dir);
		VectorMA(ambient, 0.5, diffuse, G_VECTOR(OFS_RETURN));
	}
}

/*
static void QCBUILTIN PF_Stub (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	Con_Printf("Obsolete csqc builtin (%i) executed\n", prinst->lastcalledbuiltinnumber);
}
*/

static void QCBUILTIN PF_rotatevectorsbytag (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	csqcedict_t *ent = (csqcedict_t*)G_EDICT(prinst, OFS_PARM0);
	int tagnum = G_FLOAT(OFS_PARM1);

	float *srcorg = ent->v->origin;
	int modelindex = ent->v->modelindex;

	float *retorg = G_VECTOR(OFS_RETURN);

	model_t *mod = CSQC_GetModelForIndex(modelindex);
	float transforms[12];
	float src[12];
	float dest[12];
	int i;
	framestate_t fstate;

	cs_getframestate(ent, ent->xv->renderflags, &fstate);

	if (Mod_GetTag(mod, tagnum, &fstate, transforms))
	{
		VectorCopy(csqcg.forward, src+0);
		src[3] = 0;
		VectorNegate(csqcg.right, src+4);
		src[7] = 0;
		VectorCopy(csqcg.up, src+8);
		src[11] = 0;

		if (ent->xv->scale)
		{
			for (i = 0; i < 12; i+=4)
			{
				transforms[i+0] *= ent->xv->scale;
				transforms[i+1] *= ent->xv->scale;
				transforms[i+2] *= ent->xv->scale;
				transforms[i+3] *= ent->xv->scale;
			}
		}

		R_ConcatRotationsPad((void*)transforms, (void*)src, (void*)dest);

		VectorCopy(dest+0, csqcg.forward);
		VectorNegate(dest+4, csqcg.right);
		VectorCopy(dest+8, csqcg.up);

		VectorCopy(srcorg, retorg);
		for (i = 0 ; i < 3 ; i++)
		{
			retorg[0] += transforms[i*4+3]*src[4*i+0];
			retorg[1] += transforms[i*4+3]*src[4*i+1];
			retorg[2] += transforms[i*4+3]*src[4*i+2];
		}
		return;
	}

	VectorCopy(srcorg, retorg);
}




static void QCBUILTIN PF_cs_break (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	PR_RunWarning (prinst, "break statement\n");
}

//fixme merge with ssqc
static void QCBUILTIN PF_cs_walkmove (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	wedict_t	*ent;
	float	yaw, dist;
	vec3_t	move;
//	dfunction_t	*oldf;
	int 	oldself;
	qboolean settrace;
	vec3_t axis[3];
	float s;

	ent = PROG_TO_WEDICT(prinst, *csqcg.self);
	yaw = G_FLOAT(OFS_PARM0);
	dist = G_FLOAT(OFS_PARM1);
	if (prinst->callargc >= 3 && G_FLOAT(OFS_PARM2))
		settrace = true;
	else
		settrace = false;

	if ( !( (int)ent->v->flags & (FL_ONGROUND|FL_FLY|FL_SWIM) ) )
	{
		G_FLOAT(OFS_RETURN) = 0;
		return;
	}

	World_GetEntGravityAxis(ent, axis);

	yaw = yaw*M_PI*2 / 360;

	s = cos(yaw)*dist;
	VectorScale(axis[0], s, move);
	s = sin(yaw)*dist;
	VectorMA(move, s, axis[1], move);

// save program state, because CS_movestep may call other progs
	oldself = *csqcg.self;

	G_FLOAT(OFS_RETURN) = World_movestep(&csqc_world, (wedict_t*)ent, move, axis, true, false, settrace?cs_settracevars:NULL, pr_globals);

// restore program state
	*csqcg.self = oldself;
}

//fixme merge with ssqc
static void QCBUILTIN PF_cs_movetogoal (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	wedict_t	*ent;
	float dist;
	ent = (wedict_t*)PROG_TO_EDICT(prinst, *csqcg.self);
	dist = G_FLOAT(OFS_PARM0);
	World_MoveToGoal (&csqc_world, ent, dist);
}

static void CS_ConsoleCommand_f(void)
{
	char cmd[2048];
	int seat = CL_TargettedSplit(false);
	Q_snprintfz(cmd, sizeof(cmd), "%s %s", Cmd_Argv(0), Cmd_Args());
	CSQC_ConsoleCommand(seat, cmd);
}
static void QCBUILTIN PF_cs_registercommand (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	const char *str = PF_VarString(prinst, 0, pr_globals);
	if (!Cmd_Exists(str))
		Cmd_AddCommand(str, CS_ConsoleCommand_f);
}

static void QCBUILTIN PF_cs_setlistener (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	float *origin = G_VECTOR(OFS_PARM0);
	float *forward = G_VECTOR(OFS_PARM1);
	float *right = G_VECTOR(OFS_PARM2);
	float *up = G_VECTOR(OFS_PARM3);
	size_t reverbtype = (prinst->callargc>4)?G_FLOAT(OFS_PARM4):0;
	float *velocity = (prinst->callargc>5)?G_VECTOR(OFS_PARM5):NULL;

	int i = (csqc_playerseat>=0)?csqc_playerseat:0;

	cl.playerview[i].audio.defaulted = false;
	cl.playerview[i].audio.entnum = csqcg.player_localentnum?*csqcg.player_localentnum:cl.playerview[i].viewentity;
	VectorCopy(origin, cl.playerview[i].audio.origin);
	VectorCopy(forward, cl.playerview[i].audio.forward);
	VectorCopy(right, cl.playerview[i].audio.right);
	VectorCopy(up, cl.playerview[i].audio.up);
	cl.playerview[i].audio.reverbtype = reverbtype;
	if (velocity)
		VectorCopy(velocity, cl.playerview[i].audio.velocity);
	else
		VectorClear(cl.playerview[i].audio.velocity);
}
static void QCBUILTIN PF_cs_setupreverb (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	int reverbslot = G_FLOAT(OFS_PARM0);
	int qcptr = G_INT(OFS_PARM1);
	int size = G_INT(OFS_PARM2);
	struct reverbproperties_s *ptr;

	//validates the pointer.
	if (qcptr < 0 || qcptr+size >= prinst->stringtablesize)
	{
		PR_BIError(prinst, "PF_cs_setupreverb: invalid reverb pointer\n");
		return;
	}
	ptr = (struct reverbproperties_s*)(prinst->stringtable + qcptr);

	//let the sound system do its thing.
	S_UpdateReverb(reverbslot, ptr, size);
}

#define RSES_NOLERP 1
#define RSES_NOROTATE 2
#define RSES_NOTRAILS 4
#define RSES_NOLIGHTS 8

static void CSQC_LerpStateToCSQC(lerpents_t *le, csqcedict_t *ent, qboolean nolerp)
{
	ent->v->frame = le->newframe[FS_REG];
	ent->xv->frame1time = max(0, cl.servertime - le->newframestarttime[FS_REG]);
	ent->xv->frame2 = le->oldframe[FS_REG];
	ent->xv->frame2time = max(0, cl.servertime - le->oldframestarttime[FS_REG]);
	ent->xv->lerpfrac = bound(0, 1-(ent->xv->frame1time) / le->framelerpdeltatime[FS_REG], 1);

	ent->xv->baseframe = le->newframe[FST_BASE];
	ent->xv->baseframe1time = max(0, cl.servertime - le->newframestarttime[FST_BASE]);
	ent->xv->baseframe2 = le->oldframe[FST_BASE];
	ent->xv->baseframe2time = max(0, cl.servertime - le->oldframestarttime[FST_BASE]);
	ent->xv->baselerpfrac = bound(0, 1-(ent->xv->baseframe1time) / le->framelerpdeltatime[FST_BASE], 1);
	ent->xv->basebone = le->basebone;


	if (nolerp)
	{
		ent->v->origin[0] = le->neworigin[0];
		ent->v->origin[1] = le->neworigin[1];
		ent->v->origin[2] = le->neworigin[2];
		ent->v->angles[0] = le->newangle[0];
		ent->v->angles[1] = le->newangle[1];
		ent->v->angles[2] = le->newangle[2];
	}
	else
	{
		ent->v->origin[0] = le->origin[0];
		ent->v->origin[1] = le->origin[1];
		ent->v->origin[2] = le->origin[2];
		ent->v->angles[0] = le->angles[0];
		ent->v->angles[1] = le->angles[1];
		ent->v->angles[2] = le->angles[2];
	}
}

void CSQC_EntStateToCSQC(unsigned int flags, float lerptime, entity_state_t *src, csqcedict_t *ent)
{
	model_t *model;
	lerpents_t		*le;

	le = &cl.lerpents[src->number];

	CSQC_LerpStateToCSQC(le, ent, flags & RSES_NOLERP);


	model = cl.model_precache[src->modelindex];
	if (!(flags & RSES_NOTRAILS))
	{
		//use entnum as a test to see if its new (if the old origin isn't usable)
		if (ent->xv->entnum)
		{
			if (model->particletrail == P_INVALID || pe->ParticleTrail (ent->v->origin, src->origin, model->particletrail, src->number, NULL, &(le->trailstate)))
				if (model->traildefaultindex >= 0)
					pe->ParticleTrailIndex(ent->v->origin, src->origin, P_INVALID, model->traildefaultindex, 0, &(le->trailstate));
		}
	}

	ent->xv->entnum = src->number;
	ent->v->modelindex = src->modelindex;
//	ent->xv->vw_index = src->modelindex2;
//	ent->v->flags = src->flags;
	ent->v->effects = src->effects;

//we ignore the q2 state fields

	ent->v->colormap = src->colormap;
	ent->v->skin = src->skinnum;
	ent->xv->scale = src->scale/16.0f;
	ent->xv->fatness = src->fatness/16.0f;
#ifdef HEXEN2
	ent->xv->drawflags = src->hexen2flags;
	ent->xv->abslight = src->abslight;
#endif
//	ent->xv->abslight = src->abslight;
//	ent->v->dpflags = src->dpflags;
	ent->xv->colormod[0] = src->colormod[0]*(8/256.0f);
	ent->xv->colormod[1] = src->colormod[1]*(8/256.0f);
	ent->xv->colormod[2] = src->colormod[2]*(8/256.0f);
	ent->xv->glowmod[0] = src->glowmod[0]*(8/256.0f);
	ent->xv->glowmod[1] = src->glowmod[1]*(8/256.0f);
	ent->xv->glowmod[2] = src->glowmod[2]*(8/256.0f);
//	ent->xv->glow_size = src->glowsize*4;
//	ent->xv->glow_color = src->glowcolour;
//	ent->xv->glow_trail = !!(state->dpflags & RENDER_GLOWTRAIL);
	ent->xv->alpha = (src->trans==255)?0:src->trans/254.0f;
//	ent->v->solid = src->solid;
//	ent->v->color[0] = src->light[0]/255.0;
//	ent->v->color[1] = src->light[1]/255.0;
//	ent->v->color[2] = src->light[2]/255.0;
//	ent->v->light_lev = src->light[3];
//	ent->xv->style = src->lightstyle;
//	ent->xv->pflags = src->lightpflags;

	ent->xv->tag_entity = src->tagentity;
	ent->xv->tag_index = src->tagindex;

	if (src->solidsize == ES_SOLID_BSP)
	{
		ent->v->solid = SOLID_BSP;
		VectorCopy(model->mins, ent->v->mins);
		VectorCopy(model->maxs, ent->v->maxs);
	}
	else if (src->solidsize)
	{
		ent->v->solid = SOLID_BBOX;
		COM_DecodeSize(src->solidsize, ent->v->mins, ent->v->maxs);
	}
	else
		ent->v->solid = SOLID_NOT;

	ent->v->movetype = src->u.q1.pmovetype;
	ent->v->velocity[0] = src->u.q1.velocity[0] * (1/8.0);
	ent->v->velocity[1] = src->u.q1.velocity[1] * (1/8.0);
	ent->v->velocity[2] = src->u.q1.velocity[2] * (1/8.0);

	if (model)
	{
		if (!(flags & RSES_NOROTATE) && (model->flags & MF_ROTATE))
		{
			ent->v->angles[0] = 0;
			ent->v->angles[1] = 100*lerptime;
			ent->v->angles[2] = 0;
		}
	}
}
void CSQC_PlayerStateToCSQC(int pnum, player_state_t *srcp, csqcedict_t *ent)
{
	ent->xv->entnum = pnum+1;

	ent->v->modelindex = srcp->modelindex;
//	ent->xv->vw_index = srcp->modelindex2;
	ent->v->skin = srcp->skinnum;

	CSQC_LerpStateToCSQC(&cl.lerpplayers[pnum], ent, true);
	ent->xv->lerpfrac = 1-(ent->xv->frame1time) / cl.lerpplayers[pnum].framelerpdeltatime[FS_REG];
	ent->xv->lerpfrac = bound(0, ent->xv->lerpfrac, 1);


	VectorCopy(srcp->origin, ent->v->origin);
	VectorCopy(srcp->viewangles, ent->v->angles);

	VectorCopy(srcp->velocity, ent->v->velocity);
	ent->v->angles[0] *= r_meshpitch.value * 0.333;
	ent->v->colormap = pnum+1;
	ent->xv->scale = srcp->scale;
	//ent->v->fatness = srcp->fatness;
	ent->xv->alpha = (srcp->alpha==255)?0:(srcp->alpha/254.0f);

//	ent->v->colormod[0] = srcp->colormod[0]*(8/256.0f);
//	ent->v->colormod[1] = srcp->colormod[1]*(8/256.0f);
//	ent->v->colormod[2] = srcp->colormod[2]*(8/256.0f);
//	ent->v->effects = srcp->effects;
}

unsigned int deltaflags[MAX_PRECACHE_MODELS];
func_t deltafunction[MAX_PRECACHE_MODELS];

typedef struct
{
	unsigned int readpos;	//pos
	unsigned int numents;	//present
	unsigned int maxents;	//buffer size
	struct
	{
		unsigned int n;	//don't rely on the ent->v->entnum, as csqc can corrupt that
		csqcedict_t *e;	//the csqc ent
	} *e;
} csqcdelta_pack_t;
static csqcdelta_pack_t csqcdelta_pack_new;
static csqcdelta_pack_t csqcdelta_pack_old;
float csqcdelta_time;

static csqcedict_t *csqcdelta_playerents[MAX_CLIENTS];

static void CSQC_EntRemove(csqcedict_t *ed)
{
	if (csqcg.ent_remove)
	{
		*csqcg.self = EDICT_TO_PROG(csqcprogs, ed);
		PR_ExecuteProgram(csqcprogs, csqcg.ent_remove);
	}
	else
	{
		pe->DelinkTrailstate(&ed->trailstate);
		World_UnlinkEdict((wedict_t*)ed);
		ED_Free (csqcprogs, (void*)ed);
	}
}

qboolean CSQC_DeltaPlayer(int playernum, player_state_t *state)
{
	func_t func;

	if (!state || state->modelindex <= 0 || state->modelindex >= MAX_PRECACHE_MODELS)
	{
		if (csqcdelta_playerents[playernum])
		{
			CSQC_EntRemove(csqcdelta_playerents[playernum]);
			csqcdelta_playerents[playernum] = NULL;
		}
		return false;
	}

	func = deltafunction[state->modelindex];
	if (func)
	{
		void *pr_globals;
		csqcedict_t *ent;

		ent = csqcdelta_playerents[playernum];
		if (!ent)
		{
			ent = (csqcedict_t *)ED_Alloc(csqcprogs, false, 0);
			ent->xv->drawmask = MASK_DELTA;
		}

		CSQC_PlayerStateToCSQC(playernum, state, ent);

		*csqcg.self = EDICT_TO_PROG(csqcprogs, (void*)ent);
		pr_globals = PR_globals(csqcprogs, PR_CURRENT);
		G_FLOAT(OFS_PARM0) = !csqcdelta_playerents[playernum];
		PR_ExecuteProgram(csqcprogs, func);

		csqcdelta_playerents[playernum] = ent;

		return G_FLOAT(OFS_RETURN);
	}
	else if (csqcdelta_playerents[playernum])
	{
		CSQC_EntRemove(csqcdelta_playerents[playernum]);
		csqcdelta_playerents[playernum] = NULL;
	}
	return false;
}

void CSQC_DeltaStart(float time)
{
	csqcdelta_pack_t tmp;
	csqcdelta_time = time;

	tmp = csqcdelta_pack_new;
	csqcdelta_pack_new = csqcdelta_pack_old;
	csqcdelta_pack_old = tmp;

	csqcdelta_pack_new.numents = 0;

	csqcdelta_pack_new.readpos = 0;
	csqcdelta_pack_old.readpos = 0;
}
qboolean CSQC_DeltaUpdate(entity_state_t *src)
{
	//FTE ensures that this function is called with increasing ent numbers each time
	func_t func;
	func = deltafunction[src->modelindex];
	if (func)
	{
		void *pr_globals;
		csqcedict_t *ent, *oldent;

		while (csqcdelta_pack_old.readpos < csqcdelta_pack_old.numents && csqcdelta_pack_old.e[csqcdelta_pack_old.readpos].n < src->number)
		{
			//this entity is stale, remove it.
			CSQC_EntRemove(csqcdelta_pack_old.e[csqcdelta_pack_old.readpos].e);
			csqcdelta_pack_old.readpos++;
		}

		if (csqcdelta_pack_old.readpos >= csqcdelta_pack_old.numents)
			oldent = NULL;	//reached the end of the old frame's ents (so we must be new)
		else if (src->number < csqcdelta_pack_old.e[csqcdelta_pack_old.readpos].n)
			oldent = NULL;	//there's a gap, this one must be new.
		else
		{	//already known.
			oldent = csqcdelta_pack_old.e[csqcdelta_pack_old.readpos].e;
			csqcdelta_pack_old.readpos++;
		}

		if (src->number < maxcsqcentities && csqcent[src->number])
		{
			//in the csqc list (don't permit in the delta list too)
			if (oldent)
				CSQC_EntRemove(oldent);
			return false;
		}

		if (oldent)
			ent = oldent;
		else
		{
			ent = (csqcedict_t *)ED_Alloc(csqcprogs, false, 0);
			if (!csqc_isdarkplaces)
				ent->xv->drawmask = MASK_DELTA;
		}

		CSQC_EntStateToCSQC(deltaflags[src->modelindex], csqcdelta_time, src, ent);


		*csqcg.self = EDICT_TO_PROG(csqcprogs, (void*)ent);
		pr_globals = PR_globals(csqcprogs, PR_CURRENT);
		G_FLOAT(OFS_PARM0) = !oldent;
		PR_ExecuteProgram(csqcprogs, func);


		if (csqcdelta_pack_new.maxents <= csqcdelta_pack_new.numents)
		{
			csqcdelta_pack_new.maxents = csqcdelta_pack_new.numents + 64;
			csqcdelta_pack_new.e = BZ_Realloc(csqcdelta_pack_new.e, sizeof(*csqcdelta_pack_new.e)*csqcdelta_pack_new.maxents);
		}
		csqcdelta_pack_new.e[csqcdelta_pack_new.numents].e = ent;
		csqcdelta_pack_new.e[csqcdelta_pack_new.numents].n = src->number;
		csqcdelta_pack_new.numents++;

		return G_FLOAT(OFS_RETURN);
	}
	return false;
}

void CSQC_DeltaEnd(void)
{
	//remove any unreferenced ents stuck on the end
	while (csqcdelta_pack_old.readpos < csqcdelta_pack_old.numents)
	{
		CSQC_EntRemove(csqcdelta_pack_old.e[csqcdelta_pack_old.readpos].e);
		csqcdelta_pack_old.readpos++;
	}
}

static void QCBUILTIN PF_DeltaListen(pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	int i;
	const char *mname = PR_GetStringOfs(prinst, OFS_PARM0);
	func_t func = G_INT(OFS_PARM1);
	unsigned int flags = G_FLOAT(OFS_PARM2);

	if (csqc_nogameaccess)
	{
		csqc_deprecated("PF_DeltaListen: game access is blocked");
		return;
	}

	if (!prinst->GetFunctionInfo(prinst, func, NULL, NULL, NULL, 0))
	{
		Con_Printf("PF_DeltaListen: Bad function index\n");
		return;
	}

	if (!strcmp(mname, "*"))
	{
		//yes, even things that are not allocated yet
		for (i = 0; i < MAX_PRECACHE_MODELS; i++)
		{
			deltafunction[i] = func;
			deltaflags[i] = flags;
		}
	}
	else
	{
		for (i = 1; i < MAX_PRECACHE_MODELS; i++)
		{
			if (!*cl.model_name[i])
				break;
			if (!strcmp(cl.model_name[i], mname))
			{
				deltafunction[i] = func;
				deltaflags[i] = flags;
				break;
			}
		}
	}
}

static void AngleVectorsIndex (const vec3_t angles, int modelindex, vec3_t forward, vec3_t right, vec3_t up)
{
	vec3_t fixedangles;
	fixedangles[0] = angles[0] * CSQC_PitchScaleForModelIndex(modelindex);
	fixedangles[1] = angles[1];
	fixedangles[2] = angles[2];
	AngleVectors(fixedangles, forward, right, up);
}
static void QCBUILTIN PF_getentity(pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	int entnum = G_FLOAT(OFS_PARM0);
	int fldnum = G_FLOAT(OFS_PARM1);
	lerpents_t *le;
	entity_state_t *es;

	if (csqc_nogameaccess)
	{
		G_FLOAT(OFS_RETURN+0) = 0;
		G_FLOAT(OFS_RETURN+1) = 0;
		G_FLOAT(OFS_RETURN+2) = 0;
		return;
	}

	if (fldnum == GE_MAXENTS)
	{
		G_FLOAT(OFS_RETURN) = cl.maxlerpents;
		return;
	}

	if (entnum > 0 && entnum <= cl.allocated_client_slots && cl.lerpplayers[entnum-1].sequence == cl.lerpentssequence)
	{
		player_state_t *ps = &cl.inframes[cl.validsequence&UPDATE_MASK].playerstate[entnum-1];
		le = &cl.lerpplayers[entnum-1];

		switch(fldnum)
		{
		case GE_ACTIVE:
			G_FLOAT(OFS_RETURN) = 1;
			break;

		case GE_ORIGIN:
			/*lerped position*/
			VectorCopy(le->origin, G_VECTOR(OFS_RETURN));
			break;
		case GE_SCALE:
			G_FLOAT(OFS_RETURN) = ps->scale / 16.0f;
			break;
		case GE_ALPHA:
			G_FLOAT(OFS_RETURN) = ps->alpha / 255.0f;
			break;
		case GE_COLORMOD:
			G_FLOAT(OFS_RETURN+0) = ps->colourmod[0] / 8.0f;
			G_FLOAT(OFS_RETURN+1) = ps->colourmod[1] / 8.0f;
			G_FLOAT(OFS_RETURN+2) = ps->colourmod[2] / 8.0f;
			break;
		case GE_SKIN:
			G_FLOAT(OFS_RETURN) = ps->skinnum;
			break;
		case GE_MINS:
			VectorCopy(ps->szmins, G_VECTOR(OFS_RETURN));
			break;
		case GE_MAXS:
			VectorCopy(ps->szmaxs, G_VECTOR(OFS_RETURN));
			break;

		case GE_ABSMIN:
			VectorAdd(ps->szmins, le->origin, G_VECTOR(OFS_RETURN));
			break;
		case GE_ABSMAX:
			VectorAdd(ps->szmaxs, le->origin, G_VECTOR(OFS_RETURN));
			break;
		case GE_ORIGINANDVECTORS:
			VectorCopy(le->origin, G_VECTOR(OFS_RETURN));
			AngleVectorsIndex(le->angles, ps->modelindex, csqcg.forward, csqcg.right, csqcg.up);
			break;
		case GE_FORWARD:
			AngleVectorsIndex(le->angles, ps->modelindex, G_VECTOR(OFS_RETURN), NULL, NULL);
			break;
		case GE_RIGHT:
			AngleVectorsIndex(le->angles, ps->modelindex, NULL, G_VECTOR(OFS_RETURN), NULL);
			break;
		case GE_UP:
			AngleVectorsIndex(le->angles, ps->modelindex, NULL, NULL, G_VECTOR(OFS_RETURN));
			break;
		case GE_PANTSCOLOR:
			G_FLOAT(OFS_RETURN) = cl.players[entnum-1].dbottomcolor;
			break;
		case GE_SHIRTCOLOR:
			G_FLOAT(OFS_RETURN) = cl.players[entnum-1].dtopcolor;
			break;
		case GE_LIGHT:
			G_FLOAT(OFS_RETURN) = 0;
			break;

		case GE_MODELINDEX:
			G_FLOAT(OFS_RETURN) = ps->modelindex;
			break;
		case GE_MODELINDEX2:
			G_FLOAT(OFS_RETURN) = ps->command.impulse;	//evil hack
			break;
		case GE_EFFECTS:
			G_FLOAT(OFS_RETURN) = ps->effects;
			break;
		case GE_FRAME:
			G_FLOAT(OFS_RETURN) = ps->frame;
			break;
		case GE_ANGLES:
			VectorCopy(le->angles, G_VECTOR(OFS_RETURN));
			break;
		case GE_FATNESS:
			G_FLOAT(OFS_RETURN) = ps->fatness;
			break;
		case GE_DRAWFLAGS:
			G_FLOAT(OFS_RETURN) = SCALE_ORIGIN_ORIGIN;
			break;
		case GE_ABSLIGHT:
			G_FLOAT(OFS_RETURN) = 0;
			break;
		case GE_GLOWMOD:
			VectorSet(G_VECTOR(OFS_RETURN), 1, 1, 1);
			break;
		case GE_GLOWSIZE:
			G_FLOAT(OFS_RETURN) = 0;
			break;
		case GE_GLOWCOLOUR:
			G_FLOAT(OFS_RETURN) = 0;
			break;
		case GE_RTSTYLE:
			G_FLOAT(OFS_RETURN) = 0;
			break;
		case GE_RTPFLAGS:
			G_FLOAT(OFS_RETURN) = 0;
			break;
		case GE_RTCOLOUR:
			VectorSet(G_VECTOR(OFS_RETURN), 1, 1, 1);
			break;
		case GE_RTRADIUS:
			G_FLOAT(OFS_RETURN) = 0;
			break;
		case GE_TAGENTITY:
			G_FLOAT(OFS_RETURN) = 0;
			break;
		case GE_TAGINDEX:
			G_FLOAT(OFS_RETURN) = 0;
			break;
		case GE_GRAVITYDIR:
			VectorCopy(ps->gravitydir, G_VECTOR(OFS_RETURN));
			break;
		case GE_TRAILEFFECTNUM:
			G_FLOAT(OFS_RETURN) = 0;
			break;

		default:
			VectorCopy(vec3_origin, G_VECTOR(OFS_RETURN));
			break;
		}
		return;
	}

	if (entnum >= cl.maxlerpents || !cl.lerpentssequence || cl.lerpents[entnum].sequence != cl.lerpentssequence)
	{
		if (fldnum != GE_ACTIVE)
			Con_DPrintf("PF_getentity: entity %i is not valid\n", entnum);
		VectorCopy(vec3_origin, G_VECTOR(OFS_RETURN));
		return;
	}
	le = &cl.lerpents[entnum];
	es = le->entstate;
	switch(fldnum)
	{
	case GE_ACTIVE:
		G_FLOAT(OFS_RETURN) = 1;
		break;
	case GE_ORIGIN:
		/*lerped position*/
		VectorCopy(le->origin, G_VECTOR(OFS_RETURN));
		break;
	case GE_SCALE:
		G_FLOAT(OFS_RETURN) = es->scale / 16.0f;
		break;
	case GE_ALPHA:
		G_FLOAT(OFS_RETURN) = es->trans / 255.0f;
		break;
	case GE_COLORMOD:
		G_FLOAT(OFS_RETURN+0) = es->colormod[0] / 8.0f;
		G_FLOAT(OFS_RETURN+1) = es->colormod[1] / 8.0f;
		G_FLOAT(OFS_RETURN+2) = es->colormod[2] / 8.0f;
		break;
	case GE_SKIN:
		G_FLOAT(OFS_RETURN) = es->skinnum;
		break;
	case GE_MINS:
		{
			vec3_t maxs;
			COM_DecodeSize(es->solidsize, G_VECTOR(OFS_RETURN), maxs);
		}
		break;
	case GE_MAXS:
		{
			vec3_t mins;
			COM_DecodeSize(es->solidsize, mins, G_VECTOR(OFS_RETURN));
		}
		break;

	case GE_ABSMIN:
		{
			vec3_t maxs;
			COM_DecodeSize(es->solidsize, G_VECTOR(OFS_RETURN), maxs);
			VectorAdd(G_VECTOR(OFS_RETURN), le->origin, G_VECTOR(OFS_RETURN));
		}
		break;
	case GE_ABSMAX:
		{
			vec3_t mins;
			COM_DecodeSize(es->solidsize, mins, G_VECTOR(OFS_RETURN));
			VectorAdd(G_VECTOR(OFS_RETURN), le->origin, G_VECTOR(OFS_RETURN));
		}
		break;
	case GE_ORIGINANDVECTORS:
		VectorCopy(le->origin, G_VECTOR(OFS_RETURN));
		AngleVectorsIndex(le->angles, es->modelindex, csqcg.forward, csqcg.right, csqcg.up);
		break;
	case GE_FORWARD:
		AngleVectorsIndex(le->angles, es->modelindex, G_VECTOR(OFS_RETURN), NULL, NULL);
		break;
	case GE_RIGHT:
		AngleVectorsIndex(le->angles, es->modelindex, NULL, G_VECTOR(OFS_RETURN), NULL);
		break;
	case GE_UP:
		AngleVectorsIndex(le->angles, es->modelindex, NULL, NULL, G_VECTOR(OFS_RETURN));
		break;
	case GE_PANTSCOLOR:
		if (es->colormap <= cl.allocated_client_slots && !(es->dpflags & RENDER_COLORMAPPED))
			G_FLOAT(OFS_RETURN) = cl.players[es->colormap].dbottomcolor;
		else
			G_FLOAT(OFS_RETURN) = es->colormap & 15;
		break;
	case GE_SHIRTCOLOR:
		if (es->colormap <= cl.allocated_client_slots && !(es->dpflags & RENDER_COLORMAPPED))
			G_FLOAT(OFS_RETURN) = cl.players[es->colormap].dtopcolor;
		else
			G_FLOAT(OFS_RETURN) = es->colormap>>4;
		break;
	case GE_LIGHT:
		G_FLOAT(OFS_RETURN) = 0;
		break;

	case GE_MODELINDEX:
		G_FLOAT(OFS_RETURN) = es->modelindex;
		break;
	case GE_MODELINDEX2:
		G_FLOAT(OFS_RETURN) = es->modelindex2;
		break;
	case GE_EFFECTS:
		G_FLOAT(OFS_RETURN) = es->effects;
		break;
	case GE_FRAME:
		G_FLOAT(OFS_RETURN) = es->frame;
		break;
	case GE_ANGLES:
		VectorCopy(le->angles, G_VECTOR(OFS_RETURN));
		break;
	case GE_FATNESS:
		G_FLOAT(OFS_RETURN) = es->fatness;
		break;
	case GE_DRAWFLAGS:
		G_FLOAT(OFS_RETURN) = es->hexen2flags;
		break;
	case GE_ABSLIGHT:
		G_FLOAT(OFS_RETURN) = es->abslight;
		break;
	case GE_GLOWMOD:
		VectorScale(es->glowmod, 1/8.0, G_VECTOR(OFS_RETURN));
		break;
	case GE_GLOWSIZE:
		G_FLOAT(OFS_RETURN) = es->glowsize;
		break;
	case GE_GLOWCOLOUR:
		G_FLOAT(OFS_RETURN) = es->glowcolour;
		break;
	case GE_RTSTYLE:
		G_FLOAT(OFS_RETURN) = es->lightstyle;
		break;
	case GE_RTPFLAGS:
		G_FLOAT(OFS_RETURN) = es->lightpflags;
		break;
	case GE_RTCOLOUR:
		VectorScale(es->light, 1/1024.0, G_VECTOR(OFS_RETURN));
		break;
	case GE_RTRADIUS:
		G_FLOAT(OFS_RETURN) = es->light[3];
		break;
	case GE_TAGENTITY:
		G_FLOAT(OFS_RETURN) = es->tagentity;
		break;
	case GE_TAGINDEX:
		G_FLOAT(OFS_RETURN) = es->tagindex;
		break;
	case GE_GRAVITYDIR:
		{
			vec3_t a;
			a[0] = ((192+es->u.q1.gravitydir[0])/256.0f) * 360;
			a[1] = (es->u.q1.gravitydir[1]/256.0f) * 360;
			a[2] = 0;
			AngleVectors(a, G_VECTOR(OFS_RETURN), NULL, NULL);
		}
		break;
	case GE_TRAILEFFECTNUM:
		G_FLOAT(OFS_RETURN) = es->u.q1.traileffectnum;
		break;

	default:
		Con_Printf("PF_getentity: field %i is not supported\n", fldnum);
		VectorCopy(vec3_origin, G_VECTOR(OFS_RETURN));
		break;
	}
}


static void QCBUILTIN PF_cs_getplayerstat(pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	unsigned int playernum = G_FLOAT(OFS_PARM0);
	unsigned int statnum = G_FLOAT(OFS_PARM1);
	unsigned int stattype = G_FLOAT(OFS_PARM2);
	unsigned int i, j;
	if (playernum >= cl.allocated_client_slots || statnum >= MAX_CL_STATS)
		stattype = ev_void;

	switch(stattype)
	{
	default:
	case ev_void:
		G_FLOAT(OFS_RETURN+0) = 0;
		G_FLOAT(OFS_RETURN+1) = 0;
		G_FLOAT(OFS_RETURN+2) = 0;
		break;

	case ev_integer:
	case ev_field:		//Hopefully NOT useful, certainly not reliable
	case ev_function:	//Hopefully NOT useful
	case ev_pointer:	//NOT useful in a networked capacity.
		G_INT(OFS_RETURN) = cl.players[playernum].stats[statnum];
		break;

	case ev_float:
		G_FLOAT(OFS_RETURN) = cl.players[playernum].statsf[statnum];
		break;
	case ev_vector:
		G_FLOAT(OFS_RETURN+0) = (statnum+0 >= MAX_CL_STATS)?0:cl.players[playernum].statsf[statnum+0];
		G_FLOAT(OFS_RETURN+1) = (statnum+1 >= MAX_CL_STATS)?0:cl.players[playernum].statsf[statnum+1];
		G_FLOAT(OFS_RETURN+2) = (statnum+2 >= MAX_CL_STATS)?0:cl.players[playernum].statsf[statnum+2];
		break;
	case ev_entity:
		j = cl.players[playernum].stats[statnum];
		if (j < maxcsqcentities && csqcent[j])
			G_INT(OFS_RETURN) = EDICT_TO_PROG(csqcprogs, csqcent[j]);
		else if (j <= cl.allocated_client_slots && j > 0 && csqcdelta_playerents[j])
			G_INT(OFS_RETURN) = EDICT_TO_PROG(csqcprogs, csqcdelta_playerents[j]);
		else
		{
			G_INT(OFS_RETURN) = 0;
			//scan for the delta entity reference.
			for (i = 0; i < csqcdelta_pack_new.numents; i++)
			{
				if (csqcdelta_pack_old.e[i].n == j && csqcdelta_pack_old.e[i].e)
				{
					G_INT(OFS_RETURN) = EDICT_TO_PROG(csqcprogs, csqcdelta_pack_old.e[i].e);
					break;
				}
			}
		}
		break;
	case ev_string:
		G_INT(OFS_RETURN) = 0;	//FIXME: no info, these are not currently tracked in mvds apparently.
		break;
	}
}


static void QCBUILTIN PF_V_CalcRefdef(pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{	//this function is essentially an overcomplicated way to shirk from defining your own view bobbing.
	csqcedict_t *ent = (csqcedict_t*)G_EDICT(prinst, OFS_PARM0);
	/*enum
	{
		TELEPORTED,
		JUMPING,
		DEAD,
		INTERMISSION
	} flags = G_FLOAT(OFS_PARM1);*/
	csqc_deprecated("V_CalcRefdef has too much undefined behaviour.\n");
//	if (ent->xv->entnum >= 1 && ent->xv->entnum <= MAX_CLIENTS)
//		CSQC_ChangeLocalPlayer(ent->xv->entnum-1);

	csqc_rebuildmatricies = true;

	CL_DecayLights ();

	CL_ClearEntityLists();

	V_ClearRefdef(csqc_playerview);
	r_refdef.drawsbar = false;	//csqc defaults to no sbar.
	r_refdef.drawcrosshair = false;

	VectorCopy(ent->v->origin, csqc_playerview->simorg);

	V_CalcRefdef(csqc_playerview);	//set up the defaults
}

#if 1
//static void QCBUILTIN PF_ReadServerEntityState(pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
//{
//}
#else
packet_entities_t *CL_ProcessPacketEntities(float *servertime, qboolean nolerp);
static void QCBUILTIN PF_ReadServerEntityState(pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	//read the arguments the csqc gave us
	unsigned int flags = G_FLOAT(OFS_PARM0);
	float servertime = G_FLOAT(OFS_PARM1);

	//locals
	packet_entities_t *pack;
	csqcedict_t *ent;
	entity_state_t *src;
	unsigned int i;
	lerpents_t		*le;
	csqcedict_t *oldent;
	oldcsqcpack_t *oldlist, *newlist;
	int oldidx = 0, newidx = 0;
	model_t *model;
	player_state_t *srcp;

	//setup
	servertime += cl.servertime;
	pack = CL_ProcessPacketEntities(&servertime, (flags & RSES_NOLERP));
	if (!pack)
		return;	//we're lagging. can't do anything, just don't update

	for (i = 0; i < cl.allocated_client_slots; i++)
	{
		srcp = &cl.frames[cl.validsequence&UPDATE_MASK].playerstate[i];
		ent = deltaedplayerents[i];
		if (srcp->messagenum == cl.validsequence && (i+1 >= maxcsqcentities || !csqcent[i+1]))
		{
			if (!ent)
			{
				ent = (csqcedict_t *)ED_Alloc(prinst);
				deltaedplayerents[i] = ent;
				G_FLOAT(OFS_PARM0) = true;
			}
			else
			{
				G_FLOAT(OFS_PARM0) = false;
			}

			CSQC_PlayerStateToCSQC(i, srcp, ent);

			if (csqcg.delta_update)
			{
				*csqcg.self = EDICT_TO_PROG(prinst, (void*)ent);
				PR_ExecuteProgram(prinst, csqcg.delta_update);
			}
		}
		else if (ent)
		{
			*csqcg.self = EDICT_TO_PROG(prinst, (void*)ent);
			PR_ExecuteProgram(prinst, csqcg.delta_remove);
			deltaedplayerents[i] = NULL;
		}
	}

	oldlist = &loadedcsqcpack[loadedcsqcpacknum];
	loadedcsqcpacknum ^= 1;
	newlist = &loadedcsqcpack[loadedcsqcpacknum];
	newlist->numents = 0;

	for (i = 0; i < pack->num_entities; i++)
	{
		src = &pack->entities[i];
// CL_LinkPacketEntities

#ifdef warningmsg
#pragma warningmsg("what to do here?")
#endif
//		if (csqcent[src->number])
//			continue;	//don't add the entity if we have one sent specially via csqc protocols.

		if (oldidx == oldlist->numents)
		{	//reached the end of the old frame's ents
			oldent = NULL;
		}
		else
		{
			while (oldidx < oldlist->numents && oldlist->entnum[oldidx] < src->number)
			{
				//this entity is stale, remove it.
				oldent = oldlist->entptr[oldidx];
				*csqcg.self = EDICT_TO_PROG(prinst, (void*)oldent);
				PR_ExecuteProgram(prinst, csqcg.delta_remove);
				oldidx++;
			}

			if (src->number < oldlist->entnum[oldidx])
				oldent = NULL;
			else
			{
				oldent = oldlist->entptr[oldidx];
				oldidx++;
			}
		}

		if (src->number < maxcsqcentities && csqcent[src->number])
		{
			//in the csqc list
			if (oldent)
			{
				*csqcg.self = EDICT_TO_PROG(prinst, (void*)oldent);
				PR_ExecuteProgram(prinst, csqcg.delta_remove);
			}
			continue;
		}

		//note: we don't delta the state here. we just replace the old.
		//its already lerped

		if (oldent)
			ent = oldent;
		else
			ent = (csqcedict_t *)ED_Alloc(prinst);

		CSQC_EntStateToCSQC(flags, servertime, src, ent);

		if (csqcg.delta_update)
		{
			*csqcg.self = EDICT_TO_PROG(prinst, (void*)ent);
			G_FLOAT(OFS_PARM0) = !oldent;
			PR_ExecuteProgram(prinst, csqcg.delta_update);
		}

		if (newlist->maxents <= newidx)
		{
			newlist->maxents = newidx + 64;
			newlist->entptr = BZ_Realloc(newlist->entptr, sizeof(*newlist->entptr)*newlist->maxents);
			newlist->entnum = BZ_Realloc(newlist->entnum, sizeof(*newlist->entnum)*newlist->maxents);
		}
		newlist->entptr[newidx] = ent;
		newlist->entnum[newidx] = src->number;
		newidx++;

	}

	//remove any unreferenced ents stuck on the end
	while (oldidx < oldlist->numents)
	{
		oldent = oldlist->entptr[oldidx];
		*csqcg.self = EDICT_TO_PROG(prinst, (void*)oldent);
		PR_ExecuteProgram(prinst, csqcg.delta_remove);
		oldidx++;
	}

	newlist->numents = newidx;
}
#endif
//be careful to not touch the resource unless we're meant to, to avoid stalling
static void QCBUILTIN PF_resourcestatus(pubprogfuncs_t *prinst, struct globalvars_s *pr_globals)
{
	int restype = G_FLOAT(OFS_PARM0);
	int doload = G_FLOAT(OFS_PARM1);
	const char *resname = PR_GetStringOfs(prinst, OFS_PARM2);
	int idx, idx2;
	model_t *mod;
	image_t *img;
//	shader_t *sh;
	sfx_t *sfx;
	G_FLOAT(OFS_RETURN) = RESSTATE_NOTKNOWN;
	switch(restype)
	{
	case RESTYPE_MODEL:
		idx = CS_FindModel(resname, &idx2);
		if (idx < 0)
		{
			mod = cl.model_csqcprecache[-idx];
			if (!cl.model_csqcprecache[-idx] && doload)
				mod = cl.model_csqcprecache[-idx] = Mod_ForName(Mod_FixName(cl.model_csqcname[-idx], cl.model_name[1]), MLV_WARN);
		}
		else if (idx > 0)
		{
			mod = cl.model_precache[idx];
			if (!cl.model_precache[idx] && doload)
				mod = cl.model_precache[idx] = Mod_ForName(Mod_FixName(cl.model_name[idx], cl.model_name[1]), MLV_WARN);
		}
		else
			return;

		if (!mod)
			G_FLOAT(OFS_RETURN) = RESSTATE_NOTLOADED;
		else
		{
			if (doload && mod->loadstate == MLS_NOTLOADED)
				Mod_LoadModel (mod, MLV_SILENT);	//should avoid blocking.
			switch(mod->loadstate)
			{
			default:
			case MLS_NOTLOADED:
				G_FLOAT(OFS_RETURN) = RESSTATE_NOTLOADED;
				break;
			case MLS_LOADING:
				G_FLOAT(OFS_RETURN) = RESSTATE_LOADING;
				break;
			case MLS_LOADED:
				G_FLOAT(OFS_RETURN) = RESSTATE_LOADED;
				break;
			case MLS_FAILED:
				G_FLOAT(OFS_RETURN) = RESSTATE_FAILED;
				break;
			}
		}
		return;
	case RESTYPE_SOUND:
		sfx = NULL;
		for (idx=1 ; idx<MAX_PRECACHE_SOUNDS && cl.sound_name[idx] ; idx++)
		{
			if (!strcmp(cl.sound_name[idx], resname))
			{
				sfx = cl.sound_precache[idx];
				break;
			}
		}
		if (!sfx)
			sfx = S_FindName(resname, doload, false);
		if (!sfx)
			G_FLOAT(OFS_RETURN) = RESSTATE_NOTKNOWN;
		else
		{
			if (doload && sfx->loadstate == SLS_NOTLOADED)
				S_LoadSound(sfx);
			switch(sfx->loadstate)
			{
			case SLS_NOTLOADED:
				G_FLOAT(OFS_RETURN) = RESSTATE_NOTLOADED;
				break;
			case SLS_LOADING:
				G_FLOAT(OFS_RETURN) = RESSTATE_LOADING;
				break;
			case SLS_LOADED:
				G_FLOAT(OFS_RETURN) = RESSTATE_LOADED;
				break;
			case SLS_FAILED:
				G_FLOAT(OFS_RETURN) = RESSTATE_FAILED;
				break;
			}
		}
		break;
/*
	case RESTYPE_PARTICLE:
		G_FLOAT(OFS_RETURN) = RESSTATE_NOTKNOWN;
		break;
	case RESTYPE_SHADER:
		sh = R_RegisterCustom(resname, 0, NULL, NULL);
		if (sh)
		{
			//FIXME: scan through the images.
		}
		else
			G_FLOAT(OFS_RETURN) = RESSTATE_NOTKNOWN;
		break;
	case RESTYPE_SKIN:
		G_FLOAT(OFS_RETURN) = RESSTATE_NOTKNOWN;
		break;
*/
	case RESTYPE_TEXTURE:
		G_FLOAT(OFS_RETURN) = RESSTATE_NOTKNOWN;
		img = Image_FindTexture(resname, NULL, 0);
		if (!img)
			G_FLOAT(OFS_RETURN) = RESSTATE_NOTKNOWN;
		else
		{
			switch(img->status)
			{
			case TEX_NOTLOADED:
				G_FLOAT(OFS_RETURN) = RESSTATE_NOTLOADED;
				break;
			case TEX_LOADING:
				G_FLOAT(OFS_RETURN) = RESSTATE_LOADING;
				break;
			case TEX_LOADED:
				G_FLOAT(OFS_RETURN) = RESSTATE_LOADED;
				break;
			case TEX_FAILED:
				G_FLOAT(OFS_RETURN) = RESSTATE_FAILED;
				break;
			}
		}
		break;
	default:
		G_FLOAT(OFS_RETURN) = RESSTATE_UNSUPPORTED;
		break;
	}
}

void QCBUILTIN PF_CL_DrawTextField (pubprogfuncs_t *prinst, struct globalvars_s *pr_globals);

//prefixes:
//PF_ - common, works on any vm
//PF_cs_ - works in csqc only (dependant upon globals or fields)
//PF_cl_ - works in csqc and menu (if needed...)

//warning: functions that depend on globals are bad, mkay?
static struct {
	char *name;
	builtin_t bifunc;
	int ebfsnum;
}  BuiltinList[] = {
//0
	{"makevectors",				PF_cs_makevectors, 1},		// #1 void() makevectors (QUAKE)
	{"setorigin",				PF_cs_SetOrigin, 2},		// #2 void(entity e, vector org) setorigin (QUAKE)
	{"setmodel",				PF_cs_SetModel, 3},			// #3 void(entity e, string modl) setmodel (QUAKE)
	{"setsize",					PF_cs_SetSize, 4},			// #4 void(entity e, vector mins, vector maxs) setsize (QUAKE)
//5
	{"debugbreak",				PF_cs_break, 6},			// #6 void() debugbreak (QUAKE)
	{"random",					PF_random,	7},				// #7 float() random (QUAKE)
	{"sound",					PF_cs_sound,	8},			// #8 void(entity e, float chan, string samp, float vol, float atten) sound (QUAKE)
	{"normalize",				PF_normalize,	9},			// #9 vector(vector in) normalize (QUAKE)
//10
	{"error",					PF_error,	10},				// #10 void(string errortext) error (QUAKE)
	{"objerror",				PF_objerror,	11},			// #11 void(string errortext) onjerror (QUAKE)
	{"vlen",					PF_vlen,	12},				// #12 float(vector v) vlen (QUAKE)
	{"vectoyaw",				PF_vectoyaw,	13},			// #13 float(vector v) vectoyaw (QUAKE)
	{"spawn",					PF_Spawn,	14},				// #14 entity() spawn (QUAKE)
	{"remove",					PF_cs_remove,	15},			// #15 void(entity e) remove (QUAKE)
	{"traceline",				PF_cs_traceline,	16},		// #16 void(vector v1, vector v2, float nomonst, entity forent) traceline (QUAKE)
	{"checkclient",				PF_NoCSQC,	17},				// #17 entity() checkclient (QUAKE) (don't support)
	{"find",					PF_FindString,	18},			// #18 entity(entity start, .string fld, string match) findstring (QUAKE)
	{"precache_sound",			PF_cs_PrecacheSound,	19},	// #19 void(string str) precache_sound (QUAKE)
//20
	{"precache_model",			PF_cs_PrecacheModel,	20},	// #20 void(string str) precache_model (QUAKE)
	{"stuffcmd",				PF_NoCSQC,	21},		// #21 void(entity client, string s) stuffcmd (QUAKE) (don't support)
	{"findradius",				PF_findradius,	22},		// #22 entity(vector org, float rad) findradius (QUAKE)
	{"bprint",					PF_NoCSQC,	23},				// #23 void(string s, ...) bprint (QUAKE) (don't support)
	{"sprint",					PF_NoCSQC,	24},				// #24 void(entity e, string s, ...) sprint (QUAKE) (don't support)
	{"dprint",					PF_dprint,	25},				// #25 void(string s, ...) dprint (QUAKE)
	{"ftos",					PF_ftos,	26},				// #26 string(float f) ftos (QUAKE)
	{"vtos",					PF_vtos,	27},				// #27 string(vector f) vtos (QUAKE)
	{"coredump",				PF_coredump,	28},			// #28 void(void) coredump (QUAKE)
	{"traceon",					PF_traceon,	29},				// #29 void() traceon (QUAKE)
//30
	{"traceoff",				PF_traceoff,	30},			// #30 void() traceoff (QUAKE)
	{"eprint",					PF_eprint,	31},				// #31 void(entity e) eprint (QUAKE)
	{"walkmove",				PF_cs_walkmove,	32},			// #32 float(float yaw, float dist) walkmove (QUAKE)
	{"?",						PF_Fixme,	33},				// #33
	{"droptofloor",				PF_cs_droptofloor,	34},		// #34
	{"lightstyle",				PF_cs_lightstyle,	35},		// #35 void(float lightstyle, string stylestring) lightstyle (QUAKE)
	{"rint",					PF_rint,	36},				// #36 float(float f) rint (QUAKE)
	{"floor",					PF_floor,	37},				// #37 float(float f) floor (QUAKE)
	{"ceil",					PF_ceil,	38},				// #38 float(float f) ceil (QUAKE)
//	{"?",						PF_Fixme,	39},				// #39
//40
	{"checkbottom",				PF_checkbottom,	40},	// #40 float(entity e) checkbottom (QUAKE)
	{"pointcontents",			PF_cs_pointcontents,	41},	// #41 float(vector org) pointcontents (QUAKE)
//	{"?",						PF_Fixme,	42},				// #42
	{"fabs",					PF_fabs,	43},				// #43 float(float f) fabs (QUAKE)
	{"aim",						PF_NoCSQC,	44},				// #44 vector(entity e, float speed) aim (QUAKE) (don't support)
	{"cvar",					PF_cvar,	45},				// #45 float(string cvarname) cvar (QUAKE)
	{"localcmd",				PF_localcmd,	46},			// #46 void(string str) localcmd (QUAKE)
	{"nextent",					PF_nextent,	47},				// #47 entity(entity e) nextent (QUAKE)
	{"particle",				PF_cs_particle,	48},		// #48 void(vector org, vector dir, float colour, float count) particle (QUAKE)
	{"changeyaw",				PF_changeyaw,	49},			// #49 void() changeyaw (QUAKE)
//50
//	{"?",	PF_Fixme,	50},				// #50
	{"vectoangles",				PF_vectoangles,	51},			// #51 vector(vector v) vectoangles (QUAKE)
//	{"WriteByte",				PF_Fixme,	52},				// #52 void(float to, float f) WriteByte (QUAKE)
//	{"WriteChar",				PF_Fixme,	53},				// #53 void(float to, float f) WriteChar (QUAKE)
//	{"WriteShort",				PF_Fixme,	54},				// #54 void(float to, float f) WriteShort (QUAKE)

//	{"WriteLong",				PF_Fixme,	55},				// #55 void(float to, float f) WriteLong (QUAKE)
//	{"WriteCoord",				PF_Fixme,	56},				// #56 void(float to, float f) WriteCoord (QUAKE)
//	{"WriteAngle",				PF_Fixme,	57},				// #57 void(float to, float f) WriteAngle (QUAKE)
//	{"WriteString",				PF_Fixme,	58},				// #58 void(float to, float f) WriteString (QUAKE)
//	{"WriteEntity",				PF_Fixme,	59},				// #59 void(float to, float f) WriteEntity (QUAKE)

//60
	{"sin",						PF_Sin,	60},					// #60 float(float angle) sin (DP_QC_SINCOSSQRTPOW)
	{"cos",						PF_Cos,	61},					// #61 float(float angle) cos (DP_QC_SINCOSSQRTPOW)
	{"sqrt",					PF_Sqrt,	62},				// #62 float(float value) sqrt (DP_QC_SINCOSSQRTPOW)
	{"changepitch",				PF_changepitch,	63},			// #63 void(entity ent) changepitch (DP_QC_CHANGEPITCH)
	{"tracetoss",				PF_cs_tracetoss,	64},		// #64 void(entity ent, entity ignore) tracetoss (DP_QC_TRACETOSS)

	{"etos",					PF_etos,	65},				// #65 string(entity ent) etos (DP_QC_ETOS)
	{"?",						PF_Fixme,	66},				// #66
	{"movetogoal",				PF_cs_movetogoal,	67},		// #67 void(float step) movetogoal (QUAKE)
	{"precache_file",			PF_cs_precachefile,	68},		// #68 void(string s) precache_file (QUAKE) (don't support)
	{"makestatic",				PF_cs_makestatic,	69},		// #69 void(entity e) makestatic (QUAKE)
//70
	{"changelevel",				PF_NoCSQC,	70},				// #70 void(string mapname) changelevel (QUAKE) (don't support)
//	{"?",						PF_Fixme,	71},				// #71
	{"cvar_set",				PF_cvar_set,	72},			// #72 void(string cvarname, string valuetoset) cvar_set (QUAKE)
	{"centerprint",				PF_NoCSQC,	73},				// #73 void(entity ent, string text) centerprint (QUAKE) (don't support - cprint is supported instead)
	{"ambientsound",			PF_cl_ambientsound,	74},		// #74 void (vector pos, string samp, float vol, float atten) ambientsound (QUAKE)

	{"precache_model2",			PF_cs_PrecacheModel,	75},	// #75 void(string str) precache_model2 (QUAKE)
	{"precache_sound2",			PF_cs_PrecacheSound,	76},	// #76 void(string str) precache_sound2 (QUAKE)
	{"precache_file2",			PF_cs_precachefile,	77},		// #77 void(string str) precache_file2 (QUAKE)
	{"setspawnparms",			PF_NoCSQC,	78},				// #78 void() setspawnparms (QUAKE) (don't support)
	{"logfrag",					PF_NoCSQC,	79},				// #79 void(entity killer, entity killee) logfrag (QW_ENGINE) (don't support)

//80
	{"infokey",					PF_cs_infokey,	80},			// #80 string(entity e, string keyname) infokey (QW_ENGINE) (don't support)
	{"stof",					PF_stof,	81},				// #81 float(string s) stof (FRIK_FILE or QW_ENGINE)
	{"multicast",				PF_NoCSQC,	82},				// #82 void(vector where, float set) multicast (QW_ENGINE) (don't support)

	{"getlightstyle",			PF_getlightstyle,		0},
	{"getlightstylergb",		PF_getlightstylergb,		0},

//90
	{"tracebox",				PF_cs_tracebox,	90},
	{"randomvec",				PF_randomvector,	91},		// #91 vector() randomvec (DP_QC_RANDOMVEC)
	{"getlight",				PF_cl_getlight,	92},			// #92 vector(vector org) getlight (DP_QC_GETLIGHT)
	{"registercvar",			PF_registercvar,	93},		// #93 void(string cvarname, string defaultvalue) registercvar (DP_QC_REGISTERCVAR)
	{"min",						PF_min,	94},				// #94 float(float a, floats) min (DP_QC_MINMAXBOUND)

	{"max",						PF_max,	95},					// #95 float(float a, floats) max (DP_QC_MINMAXBOUND)
	{"bound",					PF_bound,	96},				// #96 float(float minimum, float val, float maximum) bound (DP_QC_MINMAXBOUND)
	{"pow",						PF_pow,	97},					// #97 float(float value) pow (DP_QC_SINCOSSQRTPOW)
	{"logarithm",				PF_Logarithm,	0},
	{"findfloat",				PF_FindFloat,	98},			// #98 entity(entity start, .float fld, float match) findfloat (DP_QC_FINDFLOAT)
	{"findentity",				PF_FindFloat,	98},			// #98 entity(entity start, .float fld, float match) findfloat (DP_QC_FINDFLOAT)
	{"checkextension",			PF_checkextension,	99},		// #99 float(string extname) checkextension (EXT_CSQC)
	{"checkbuiltin",			PF_checkbuiltin,	0},
	{"anglemod",				PF_anglemod,		102},

//110
	{"fopen",					PF_fopen,	110},				// #110 float(string strname, float accessmode) fopen (FRIK_FILE)
	{"fclose",					PF_fclose,	111},				// #111 void(float fnum) fclose (FRIK_FILE)
	{"fgets",					PF_fgets,	112},				// #112 string(float fnum) fgets (FRIK_FILE)
	{"fputs",					PF_fputs,	113},				// #113 void(float fnum, string str) fputs (FRIK_FILE)
	{"fread",					PF_fread,	0},
	{"fwrite",					PF_fwrite,	0},
	{"fseek",					PF_fseek,	0},
	{"fsize",					PF_fsize,	0},
	{"strlen",					PF_strlen,	114},				// #114 float(string str) strlen (FRIK_FILE)

	{"strcat",					PF_strcat,		115},			// #115 string(string str1, string str2, ...) strcat (FRIK_FILE)
	{"substring",				PF_substring,	116},			// #116 string(string str, float start, float length) substring (FRIK_FILE)
	{"stov",					PF_stov,		117},			// #117 vector(string str) stov (FRIK_FILE)
	{"strzone",					PF_strzone,		118},			// #118 string(string str) dupstring (FRIK_FILE)
	{"strunzone",				PF_strunzone,	119},			// #119 void(string str) freestring (FRIK_FILE)

	{"localsound",				PF_cl_localsound,	177},

//200
	{"getmodelindex",			PF_cs_getmodelindex,	200},
	{"externcall",				PF_externcall,	201},
	{"addprogs",				PF_cs_addprogs,	202},
	{"externvalue",				PF_externvalue,	203},
	{"externset",				PF_externset,	204},

	{"externrefcall",			PF_externrefcall,	205},
	{"instr",					PF_instr,	206},
	{"openportal",				PF_cs_OpenPortal,	207},	//q2bsps
	{"registertempent",			PF_NoCSQC,	208},//{"RegisterTempEnt", PF_RegisterTEnt,	0,		0,		0,		208},
	{"customtempent",			PF_NoCSQC,	209},//{"CustomTempEnt",	PF_CustomTEnt,		0,		0,		0,		209},
//210
//	{"fork",					PF_Fixme,	210},//{"fork",			PF_Fork,			0,		0,		0,		210},
	{"abort",					PF_Abort,	211}, //#211 void() abort (FTE_MULTITHREADED)
//	{"sleep",					PF_Fixme,	212},//{"sleep",			PF_Sleep,			0,		0,		0,		212},
	{"forceinfokey",			PF_NoCSQC,	213},//{"forceinfokey",	PF_ForceInfoKey,	0,		0,		0,		213},
	{"forceinfokeyblob",		PF_NoCSQC,	0},//{"forceinfokey",	PF_ForceInfoKey,	0,		0,		0,		213},
	{"chat",					PF_NoCSQC,	214},//{"chat",			PF_chat,			0,		0,		0,		214},// #214 void(string filename, float starttag, entity edict) SV_Chat (FTE_NPCCHAT)

	{"particle2",				PF_cs_particle2,	215}, //215 (FTE_PEXT_HEXEN2)
	{"particle3",				PF_cs_particle3,	216}, //216 (FTE_PEXT_HEXEN2)
	{"particle4",				PF_cs_particle4,	217}, //217 (FTE_PEXT_HEXEN2)

//EXT_DIMENSION_PLANES
	{"bitshift",				PF_bitshift,	218},		//#218 bitshift (EXT_DIMENSION_PLANES)
	{"te_lightningblood",		PF_cl_te_lightningblood,	219},// #219 te_lightningblood void(vector org) (FTE_TE_STANDARDEFFECTBUILTINS)

//220
//	{"map_builtin",				PF_Fixme,	220},	//like #100 - takes 2 args. arg0 is builtinname, 1 is number to map to.
	{"strstrofs",				PF_strstrofs,	221},	// #221 float(string s1, string sub) strstrofs (FTE_STRINGS)
	{"str2chr",					PF_str2chr,	222},		// #222 float(string str, float index) str2chr (FTE_STRINGS)
	{"chr2str",					PF_chr2str,	223},		// #223 string(float chr, ...) chr2str (FTE_STRINGS)
	{"strconv",					PF_strconv,	224},		// #224 string(float ccase, float redalpha, float redchars, string str, ...) strconv (FTE_STRINGS)

	{"strpad",					PF_strpad,	225},		// #225 string strpad(float pad, string str1, ...) strpad (FTE_STRINGS)
	{"infoadd",					PF_infoadd,	226},		// #226 string(string old, string key, string value) infoadd
	{"infoget",					PF_infoget,	227},		// #227 string(string info, string key) infoget
	{"strcmp",					PF_strncmp,	228},		// #228 float(string s1, string s2) strcmp (FTE_STRINGS)
	{"strncmp",					PF_strncmp,	228},		// #228 float(string s1, string s2, float len) strncmp (FTE_STRINGS)
	{"strcasecmp",				PF_strncasecmp,	229},	// #229 float(string s1, string s2) strcasecmp (FTE_STRINGS)

//230
	{"strncasecmp",				PF_strncasecmp,	230},	// #230 float(string s1, string s2, float len) strncasecmp (FTE_STRINGS)
	{"strtrim",					PF_strtrim,		0},
	{"calltimeofday",			PF_calltimeofday,	231},
	{"clientstat",				PF_NoCSQC,	232},		// #231 clientstat
	{"runclientphys",			PF_NoCSQC,	233},		// #232 runclientphys
//	{"isbackbuffered",			PF_NoCSQC,	234},		// #233 float(entity ent) isbackbuffered
//I messed up, 234 is meant to be isbackbuffered. luckily that's not present in csqc, but still, this is messy. Don't document this.
	{"rotatevectorsbytag",		PF_rotatevectorsbytag,	234},	// #234

	{"rotatevectorsbyangle",	PF_rotatevectorsbyangles,	235}, // #235
	{"rotatevectorsbyvectors",	PF_rotatevectorsbymatrix,	236}, // #236
	{"skinforname",				PF_skinforname,	237},		// #237
	{"shaderforname",			PF_shaderforname,	238},	// #238
	{"te_bloodqw",				PF_cl_te_bloodqw,	239},	// #239 void te_bloodqw(vector org[, float count]) (FTE_TE_STANDARDEFFECTBUILTINS)

	{"checkpvs",				PF_checkpvs,		240},
//	{"matchclientname",			PF_matchclient,		241},
	{"sendpacket",				PF_cl_SendPacket,	242},	//void(string dest, string content) sendpacket = #242; (FTE_QC_SENDPACKET)

//	{"bulleten",				PF_bulleten,		243}, (removed builtin)
	{"rotatevectorsbytag",		PF_rotatevectorsbytag,	244},
	{"mod",						PF_mod,				245},

	{"sqlconnect",				PF_NoCSQC,			250},	// #250 float([string host], [string user], [string pass], [string defaultdb], [string driver]) sqlconnect (FTE_SQL)
	{"sqldisconnect",			PF_NoCSQC,			251},	// #251 void(float serveridx) sqldisconnect (FTE_SQL)
	{"sqlopenquery",			PF_NoCSQC,			252},	// #252 float(float serveridx, void(float serveridx, float queryidx, float rows, float columns, float eof) callback, float querytype, string query) sqlopenquery (FTE_SQL)
	{"sqlclosequery",			PF_NoCSQC,			253},	// #253 void(float serveridx, float queryidx) sqlclosequery (FTE_SQL)
	{"sqlreadfield",			PF_NoCSQC,			254},	// #254 string(float serveridx, float queryidx, float row, float column) sqlreadfield (FTE_SQL)
	{"sqlerror",				PF_NoCSQC,			255},	// #255 string(float serveridx, [float queryidx]) sqlerror (FTE_SQL)
	{"sqlescape",				PF_NoCSQC,			256},	// #256 string(float serveridx, string data) sqlescape (FTE_SQL)
	{"sqlversion",				PF_NoCSQC,			257},	// #257 string(float serveridx) sqlversion (FTE_SQL)
	{"sqlreadfloat",			PF_NoCSQC,			258},	// #258 float(float serveridx, float queryidx, float row, float column) sqlreadfloat (FTE_SQL)

	{"stoi",					PF_stoi,			259},
	{"itos",					PF_itos,			260},
	{"stoh",					PF_stoh,			261},
	{"htos",					PF_htos,			262},
	{"ftoi",					PF_ftoi,			0},
	{"itof",					PF_itof,			0},

	{"skel_create",				PF_skel_create,			263},//float(float modlindex) skel_create = #263; // (FTE_CSQC_SKELETONOBJECTS)
	{"skel_build",				PF_skel_build,			264},//float(float skel, entity ent, float modelindex, float retainfrac, float firstbone, float lastbone, optional float addition) skel_build = #264; // (FTE_CSQC_SKELETONOBJECTS)
	{"skel_build_ptr",			PF_skel_build_ptr,		0},//float(float skel, int numblends, __variant *blends, int blendsize) skel_build_ptr = #0;
	{"skel_get_numbones",		PF_skel_get_numbones,	265},//float(float skel) skel_get_numbones = #265; // (FTE_CSQC_SKELETONOBJECTS)
	{"skel_get_bonename",		PF_skel_get_bonename,	266},//string(float skel, float bonenum) skel_get_bonename = #266; // (FTE_CSQC_SKELETONOBJECTS) (returns tempstring)
	{"skel_get_boneparent",		PF_skel_get_boneparent,	267},//float(float skel, float bonenum) skel_get_boneparent = #267; // (FTE_CSQC_SKELETONOBJECTS)
	{"skel_find_bone",			PF_skel_find_bone,		268},//float(float skel, string tagname) skel_get_boneidx = #268; // (FTE_CSQC_SKELETONOBJECTS)
	{"skel_get_bonerel",		PF_skel_get_bonerel,	269},//vector(float skel, float bonenum) skel_get_bonerel = #269; // (FTE_CSQC_SKELETONOBJECTS) (sets v_forward etc)
	{"skel_get_boneabs",		PF_skel_get_boneabs,	270},//vector(float skel, float bonenum) skel_get_boneabs = #270; // (FTE_CSQC_SKELETONOBJECTS) (sets v_forward etc)
	{"skel_set_bone",			PF_skel_set_bone,		271},//void(float skel, float bonenum, vector org) skel_set_bone = #271; // (FTE_CSQC_SKELETONOBJECTS) (reads v_forward etc)
	{"skel_premul_bone",		PF_skel_premul_bone,	272},//void(float skel, float bonenum, vector org) skel_mul_bone = #272; // (FTE_CSQC_SKELETONOBJECTS) (reads v_forward etc)
	{"skel_premul_bones",		PF_skel_premul_bones,	273},//void(float skel, float startbone, float endbone, vector org) skel_mul_bone = #273; // (FTE_CSQC_SKELETONOBJECTS) (reads v_forward etc)
	{"skel_postmul_bone",		PF_skel_postmul_bone,	0},//void(float skel, float bonenum, vector org) skel_mul_bone = #272; // (FTE_CSQC_SKELETONOBJECTS) (reads v_forward etc)
//	{"skel_postmul_bones",		PF_skel_postmul_bones,	0},//void(float skel, float startbone, float endbone, vector org) skel_mul_bone = #273; // (FTE_CSQC_SKELETONOBJECTS) (reads v_forward etc)
	{"skel_copybones",			PF_skel_copybones,		274},//void(float skeldst, float skelsrc, float startbone, float entbone) skel_copybones = #274; // (FTE_CSQC_SKELETONOBJECTS)
	{"skel_delete",				PF_skel_delete,			275},//void(float skel) skel_delete = #275; // (FTE_CSQC_SKELETONOBJECTS)
	{"frameforname",			PF_frameforname,		276},//void(float modidx, string framename) frameforname = #276 (FTE_CSQC_SKELETONOBJECTS)
	{"frameduration",			PF_frameduration,		277},//void(float modidx, float framenum) frameduration = #277 (FTE_CSQC_SKELETONOBJECTS)
	{"processmodelevents",		PF_processmodelevents,	0},
	{"getnextmodelevent",		PF_getnextmodelevent,	0},
	{"getmodeleventidx",		PF_getmodeleventidx,	0},

	{"crossproduct",			PF_crossproduct,		0},
	{"pushmove", 				PF_pushmove, 			0},
#ifdef TERRAIN
	{"terrain_edit",			PF_terrain_edit,		278},//void(float action, vector pos, float radius, float quant) terrain_edit = #278 (??FTE_TERRAIN_EDIT??)
	{"brush_get",				PF_brush_get,			0},
	{"brush_create",			PF_brush_create,		0},
	{"brush_delete",			PF_brush_delete,		0},
	{"brush_selected",			PF_brush_selected,		0},
	{"brush_getfacepoints",		PF_brush_getfacepoints,	0},
	{"brush_calcfacepoints",	PF_brush_calcfacepoints,0},
	{"brush_findinvolume",		PF_brush_findinvolume,	0},
#endif

#ifdef ENGINE_ROUTING
	{"route_calculate",		PF_route_calculate,		0},
#endif

	{"touchtriggers",			PF_touchtriggers,		279},//void() touchtriggers = #279;
	{"skel_ragupdate",			PF_skel_ragedit,		281},// (FTE_QC_RAGDOLL)
	{"skel_mmap",				PF_skel_mmap,			282},// (FTE_QC_RAGDOLL)
	{"skel_set_bone_world",		PF_skel_set_bone_world,	283},
	{"frametoname",				PF_frametoname,			284},//string(float modidx, float framenum) frametoname
	{"skintoname",				PF_skintoname,			285},//string(float modidx, float skin) skintoname
	{"resourcestatus",			PF_resourcestatus,		286},

	{"hash_createtab",			PF_hash_createtab,			287},
	{"hash_destroytab",			PF_hash_destroytab,			288},
	{"hash_add",				PF_hash_add,				289},
	{"hash_get",				PF_hash_get,				290},
	{"hash_delete",				PF_hash_delete,				291},
	{"hash_getkey",				PF_hash_getkey,				292},
	{"hash_getcb",				PF_hash_getcb,				293},
	{"checkcommand",			PF_checkcommand,			294},
	{"argescape",				PF_argescape,				295},

	{"modelframecount",			PF_modelframecount,			0},

//300
	{"clearscene",				PF_R_ClearScene,	300},				// #300 void() clearscene (EXT_CSQC)
	{"addentities",				PF_R_AddEntityMask,	301},				// #301 void(float mask) addentities (EXT_CSQC)
	{"addentity",				PF_R_AddEntity,		302},					// #302 void(entity ent) addentity (EXT_CSQC)
	{"removeentity",			PF_R_RemoveEntity,	0},
	{"setproperty",				PF_R_SetViewFlag,	303},				// #303 float(float property, ...) setproperty (EXT_CSQC)
	{"renderscene",				PF_R_RenderScene,	304},				// #304 void() renderscene (EXT_CSQC)

	{"dynamiclight_add",		PF_R_DynamicLight_AddDynamic,	305},			// #305 float(vector org, float radius, vector lightcolours) adddynamiclight (EXT_CSQC)

	{"R_BeginPolygon",			PF_R_PolygonBegin,	306},				// #306 void(string texturename) R_BeginPolygon (EXT_CSQC_???)
	{"R_PolygonVertex",			PF_R_PolygonVertex,	307},				// #307 void(vector org, vector texcoords, vector rgb, float alpha) R_PolygonVertex (EXT_CSQC_???)
	{"R_EndPolygon",			PF_R_PolygonEnd,	308},				// #308 void() R_EndPolygon (EXT_CSQC_???)
	{"addtrisoup_simple",		PF_R_AddTrisoup_Simple,	0},

	{"getproperty",				PF_R_GetViewFlag,	309},				// #309 vector/float(float property) getproperty (EXT_CSQC_1)

//310
//maths stuff that uses the current view settings.
	{"unproject",				PF_cs_unproject,	310},				// #310 vector (vector v) unproject (EXT_CSQC)
	{"project",					PF_cs_project,		311},				// #311 vector (vector v) project (EXT_CSQC)

//	{"?",	PF_Fixme,			312},				// #312
//	{"?",	PF_Fixme,		313},					// #313

//2d (immediate) operations
	{"drawtextfield",			PF_CL_DrawTextField,  0/*314*/},
	{"drawline",				PF_CL_drawline,				315},			// #315 void(float width, vector pos1, vector pos2) drawline (EXT_CSQC)
	{"iscachedpic",				PF_CL_is_cached_pic,		316},		// #316 float(string name) iscachedpic (EXT_CSQC)
	{"precache_pic",			PF_CL_precache_pic,			317},		// #317 string(string name, float trywad) precache_pic (EXT_CSQC)
	{"r_uploadimage",			PF_CL_uploadimage,			0},
	{"r_readimage",				PF_CL_readimage,			0},
	{"drawgetimagesize",		PF_CL_drawgetimagesize,		318},		// #318 vector(string picname) draw_getimagesize (EXT_CSQC)
	{"freepic",					PF_CL_free_pic,				319},		// #319 void(string name) freepic (EXT_CSQC)
//320
	{"drawcharacter",			PF_CL_drawcharacter,		320},		// #320 float(vector position, float character, vector scale, vector rgb, float alpha [, float flag]) drawcharacter (EXT_CSQC, [EXT_CSQC_???])
	{"drawrawstring",			PF_CL_drawrawstring,				321},	// #321 float(vector position, string text, vector scale, vector rgb, float alpha [, float flag]) drawstring (EXT_CSQC, [EXT_CSQC_???])
	{"drawpic",					PF_CL_drawpic,				322},		// #322 float(vector position, string pic, vector size, vector rgb, float alpha [, float flag]) drawpic (EXT_CSQC, [EXT_CSQC_???])
	{"drawrotpic",				PF_CL_drawrotpic,			0},
	{"drawfill",				PF_CL_drawfill,				323},		// #323 float(vector position, vector size, vector rgb, float alpha [, float flag]) drawfill (EXT_CSQC, [EXT_CSQC_???])
	{"drawsetcliparea",			PF_CL_drawsetcliparea,			324},	// #324 void(float x, float y, float width, float height) drawsetcliparea (EXT_CSQC_???)
	{"drawresetcliparea",		PF_CL_drawresetcliparea,	325},		// #325 void(void) drawresetcliparea (EXT_CSQC_???)

	{"drawstring",				PF_CL_drawcolouredstring,						326},	// #326
	{"stringwidth",				PF_CL_stringwidth,					327},	// #327 EXT_CSQC_'DARKPLACES'
	{"drawsubpic",				PF_CL_drawsubpic,						328},	// #328 EXT_CSQC_'DARKPLACES'
	{"drawrotsubpic",			PF_CL_drawrotsubpic,						0},
//	{"?",	PF_Fixme,						329},	// #329 EXT_CSQC_'DARKPLACES'

//330
	{"getstati",				PF_cs_getstati,					330},	// #330 float(float stnum) getstati (EXT_CSQC)
	{"getstatf",				PF_cs_getstatbits,				331},	// #331 float(float stnum) getstatf (EXT_CSQC)
	{"getstats",				PF_cs_getstats,					332},	// #332 string(float firststnum) getstats (EXT_CSQC)
	{"getplayerstat",			PF_cs_getplayerstat,			0},		// #0 __variant(float playernum, float statnum, float stattype) getplayerstat
	{"setmodelindex",			PF_cs_SetModelIndex,			333},	// #333 void(entity e, float mdlindex) setmodelindex (EXT_CSQC)
	{"modelnameforindex",		PF_cs_ModelnameForIndex,		334},	// #334 string(float mdlindex) modelnameforindex (EXT_CSQC)

	{"particleeffectnum",		PF_cs_particleeffectnum,		335},	// #335 float(string effectname) particleeffectnum (EXT_CSQC)
	{"trailparticles",			PF_cs_trailparticles,			336},	// #336 void(float effectnum, entity ent, vector start, vector end) trailparticles (EXT_CSQC),
	{"trailparticles_dp",		PF_cs_trailparticles,			336},	// #336 DP sucks
	{"pointparticles",			PF_cs_pointparticles,			337},	// #337 void(float effectnum, vector origin [, vector dir, float count]) pointparticles (EXT_CSQC)

	{"cprint",					PF_cl_cprint,					338},	// #338 void(string s) cprint (EXT_CSQC)
	{"print",					PF_print,						339},	// #339 void(string s) print (EXT_CSQC)

//340
	{"keynumtostring",			PF_cl_keynumtostring,			340},	// #340 string(float keynum) keynumtostring (EXT_CSQC)
	{"stringtokeynum",			PF_cl_stringtokeynum,			341},	// #341 float(string keyname) stringtokeynum (EXT_CSQC)
	{"getkeybind",				PF_cl_getkeybind,				342},	// #342 string(float keynum) getkeybind (EXT_CSQC)

	{"setcursormode",			PF_cl_setcursormode,			343},	// #343 This is originally a DP extension
	{"getcursormode",			PF_cl_getcursormode,			0},	// #343 This is originally a DP extension
	{"getmousepos",				PF_cl_getmousepos,				344},	// #344 This is a DP extension

	{"getinputstate",			PF_cs_getinputstate,			345},	// #345 float(float framenum) getinputstate (EXT_CSQC)
	{"setsensitivityscaler",	PF_cs_setsensitivityscaler, 	346},	// #346 void(float sens) setsensitivityscaler (EXT_CSQC)

	{"runstandardplayerphysics",PF_cs_runplayerphysics,			347},	// #347 void() runstandardplayerphysics (EXT_CSQC)

	{"getplayerkeyvalue",		PF_cs_getplayerkeystring,		348},	// #348 string(float playernum, string keyname) getplayerkeyvalue (EXT_CSQC)
	{"getplayerkeyfloat",		PF_cs_getplayerkeyfloat,		0},		// #348 string(float playernum, string keyname) getplayerkeyvalue
	{"getplayerkeyblob",		PF_cs_getplayerkeyblob,			0},		// #0   int(float playernum, string keyname, optional void *outptr, int size) getplayerkeyblob

	{"isdemo",					PF_cl_playingdemo,				349},	// #349 float() isdemo (EXT_CSQC)
//350
	{"isserver",				PF_cl_runningserver,			350},	// #350 float() isserver (EXT_CSQC)

	{"SetListener",				PF_cs_setlistener, 				351},	// #351 void(vector origin, vector forward, vector right, vector up) SetListener (EXT_CSQC)
	{"setup_reverb",			PF_cs_setupreverb, 				0},
	{"registercommand",			PF_cs_registercommand,			352},	// #352 void(string cmdname) registercommand (EXT_CSQC)
	{"wasfreed",				PF_WasFreed,					353},	// #353 float(entity ent) wasfreed (EXT_CSQC) (should be availabe on server too)

	{"serverkey",				PF_cs_serverkey,				354},	// #354 string(string key) serverkey;
	{"serverkeyfloat",			PF_cs_serverkeyfloat,			0},		// #0 float(string key) serverkeyfloat;
	{"serverkeyblob",			PF_cs_serverkeyblob,			0},
	{"getentitytoken",			PF_cs_getentitytoken,			355},	// #355 string() getentitytoken;
	{"findfont",				PF_CL_findfont,					356},
	{"loadfont",				PF_CL_loadfont,					357},
//	{"?",						PF_Fixme,						358},	// #358
	{"sendevent",				PF_cs_sendevent,				359},	// #359	void(string evname, string evargs, ...) (EXT_CSQC_1)

//360
//note that 'ReadEntity' is pretty hard to implement reliably. Modders should use a combination of ReadShort, and findfloat, and remember that it might not be known clientside (pvs culled or other reason)
	{"readbyte",				PF_ReadByte,					360},	// #360 float() readbyte (EXT_CSQC)
	{"readchar",				PF_ReadChar,					361},	// #361 float() readchar (EXT_CSQC)
	{"readshort",				PF_ReadShort,					362},	// #362 float() readshort (EXT_CSQC)
	{"readlong",				PF_ReadLong,					363},	// #363 float() readlong (EXT_CSQC)
	{"readcoord",				PF_ReadCoord,					364},	// #364 float() readcoord (EXT_CSQC)

	{"readangle",				PF_ReadAngle,					365},	// #365 float() readangle (EXT_CSQC)
	{"readstring",				PF_ReadString,					366},	// #366 string() readstring (EXT_CSQC)
	{"readfloat",				PF_ReadFloat,					367},	// #367 string() readfloat (EXT_CSQC)
	{"readentitynum",			PF_ReadEntityNum,				368},	// #368 float() readentitynum (EXT_CSQC)

//	{"readserverentitystate",	PF_ReadServerEntityState,		369},	// #369 void(float flags, float simtime) readserverentitystate (EXT_CSQC_1)
//	{"readsingleentitystate",	PF_ReadSingleEntityState,		370},
	{"deltalisten",				PF_DeltaListen,					371},		// #371 float(string modelname, float flags) deltalisten  (EXT_CSQC_1)

	{"dynamiclight_spawnstatic",PF_R_DynamicLight_AddStatic,0},
	{"dynamiclight_get",		PF_R_DynamicLight_Get,		372},
	{"dynamiclight_set",		PF_R_DynamicLight_Set,		373},
	{"particleeffectquery",		PF_cs_particleeffectquery,	374},
	{"adddecal",				PF_R_AddDecal,				375},
	{"setcustomskin",			PF_cs_setcustomskin,		376},
	{"loadcustomskin",			PF_cs_loadcustomskin,		377},
	{"applycustomskin",			PF_cs_applycustomskin,		378},
	{"releasecustomskin",		PF_cs_releasecustomskin,	379},

	{"memalloc",				PF_memalloc,				384},
	{"memfree",					PF_memfree,					385},
	{"memcpy",					PF_memcpy,					386},
	{"memfill8",				PF_memfill8,				387},
	{"memgetval",				PF_memgetval,				388},
	{"memsetval",				PF_memsetval,				389},
	{"memptradd",				PF_memptradd,				390},
	{"memstrsize",				PF_memstrsize,				0},

	{"con_getset",				PF_SubConGetSet,			391},
	{"con_printf",				PF_SubConPrintf,			392},
	{"con_draw",				PF_SubConDraw,				393},
	{"con_input",				PF_SubConInput,				394},

	{"setwindowcaption",		PF_cl_setwindowcaption,		0},

	{"cvars_haveunsaved",		PF_cvars_haveunsaved,		0},
	{"entityprotection",		PF_entityprotection,		0},
//400
	{"copyentity",				PF_copyentity,				400},	// #400 void(entity from, entity to) copyentity (DP_QC_COPYENTITY)
	{"setcolors",				PF_NoCSQC,					401},	// #401 void(entity cl, float colours) setcolors (DP_SV_SETCOLOR) (don't implement)
	{"findchain",				PF_findchain,				402},	// #402 entity(string field, string match) findchain (DP_QC_FINDCHAIN)
	{"findchainfloat",			PF_findchainfloat,			403},	// #403 entity(float fld, float match) findchainfloat (DP_QC_FINDCHAINFLOAT)
	{"effect",					PF_cl_effect,				404},		// #404 void(vector org, string modelname, float startframe, float endframe, float framerate) effect (DP_SV_EFFECT)

	{"te_blood",				PF_cl_te_blooddp,			405},	// #405 void(vector org, vector velocity, float howmany) te_blood (DP_TE_BLOOD)
	{"te_bloodshower",			PF_cl_te_bloodshower,		406},		// #406 void(vector mincorner, vector maxcorner, float explosionspeed, float howmany) te_bloodshower (DP_TE_BLOODSHOWER)
	{"te_explosionrgb",			PF_cl_te_explosionrgb,		407},	// #407 void(vector org, vector color) te_explosionrgb (DP_TE_EXPLOSIONRGB)
	{"te_particlecube",			PF_cl_te_particlecube,		408},		// #408 void(vector mincorner, vector maxcorner, vector vel, float howmany, float color, float gravityflag, float randomveljitter) te_particlecube (DP_TE_PARTICLECUBE)
	{"te_particlerain",			PF_cl_te_particlerain,		409},	// #409 void(vector mincorner, vector maxcorner, vector vel, float howmany, float color) te_particlerain (DP_TE_PARTICLERAIN)

	{"te_particlesnow",			PF_cl_te_particlesnow,		410},		// #410 void(vector mincorner, vector maxcorner, vector vel, float howmany, float color) te_particlesnow (DP_TE_PARTICLESNOW)
	{"te_spark",				PF_cl_te_spark,				411},		// #411 void(vector org, vector vel, float howmany) te_spark (DP_TE_SPARK)
	{"te_gunshotquad",			PF_cl_te_gunshotquad,		412},	// #412 void(vector org) te_gunshotquad (DP_TE_QUADEFFECTS1)
	{"te_spikequad",			PF_cl_te_spikequad,			413},		// #413 void(vector org) te_spikequad (DP_TE_QUADEFFECTS1)
	{"te_superspikequad",		PF_cl_te_superspikequad,	414},	// #414 void(vector org) te_superspikequad (DP_TE_QUADEFFECTS1)

	{"te_explosionquad",		PF_cl_te_explosionquad,		415},	// #415 void(vector org) te_explosionquad (DP_TE_QUADEFFECTS1)
	{"te_smallflash",			PF_cl_te_smallflash,		416},	// #416 void(vector org) te_smallflash (DP_TE_SMALLFLASH)
	{"te_customflash",			PF_cl_te_customflash,		417},	// #417 void(vector org, float radius, float lifetime, vector color) te_customflash (DP_TE_CUSTOMFLASH)
	{"te_gunshot",				PF_cl_te_gunshot,			418},		// #418 void(vector org) te_gunshot (DP_TE_STANDARDEFFECTBUILTINS)
	{"te_spike",				PF_cl_te_spike,				419},		// #419 void(vector org) te_spike (DP_TE_STANDARDEFFECTBUILTINS)

	{"te_superspike",			PF_cl_te_superspike,420},		// #420 void(vector org) te_superspike (DP_TE_STANDARDEFFECTBUILTINS)
	{"te_explosion",			PF_cl_te_explosion,	421},		// #421 void(vector org) te_explosion (DP_TE_STANDARDEFFECTBUILTINS)
	{"te_tarexplosion",			PF_cl_te_tarexplosion,422},		// #422 void(vector org) te_tarexplosion (DP_TE_STANDARDEFFECTBUILTINS)
	{"te_wizspike",				PF_cl_te_wizspike,	423},		// #423 void(vector org) te_wizspike (DP_TE_STANDARDEFFECTBUILTINS)
	{"te_knightspike",			PF_cl_te_knightspike,424},		// #424 void(vector org) te_knightspike (DP_TE_STANDARDEFFECTBUILTINS)

	{"te_lavasplash",			PF_cl_te_lavasplash,425},		// #425 void(vector org) te_lavasplash  (DP_TE_STANDARDEFFECTBUILTINS)
	{"te_teleport",				PF_cl_te_teleport,	426},		// #426 void(vector org) te_teleport (DP_TE_STANDARDEFFECTBUILTINS)
	{"te_explosion2",			PF_cl_te_explosion2,427},		// #427 void(vector org, float color, float colorlength) te_explosion2 (DP_TE_STANDARDEFFECTBUILTINS)
	{"te_lightning1",			PF_cl_te_lightning1,	428},	// #428 void(entity own, vector start, vector end) te_lightning1 (DP_TE_STANDARDEFFECTBUILTINS)
	{"te_lightning2",			PF_cl_te_lightning2,429},		// #429 void(entity own, vector start, vector end) te_lightning2 (DP_TE_STANDARDEFFECTBUILTINS)

	{"te_lightning3",			PF_cl_te_lightning3,				430},		// #430 void(entity own, vector start, vector end) te_lightning3 (DP_TE_STANDARDEFFECTBUILTINS)
	{"te_beam",					PF_cl_te_beam,						431},		// #431 void(entity own, vector start, vector end) te_beam (DP_TE_STANDARDEFFECTBUILTINS)
	{"vectorvectors",			PF_vectorvectors,					432},		// #432 void(vector dir) vectorvectors (DP_QC_VECTORVECTORS)
	{"te_plasmaburn",			PF_cl_te_plasmaburn,				433},		// #433 void(vector org) te_plasmaburn (DP_TE_PLASMABURN)
	{"getsurfacenumpoints",		PF_getsurfacenumpoints,				434},		// #434 float(entity e, float s) getsurfacenumpoints (DP_QC_GETSURFACE)

	{"getsurfacepoint",			PF_getsurfacepoint,					435},		// #435 vector(entity e, float s, float n) getsurfacepoint (DP_QC_GETSURFACE)
	{"getsurfacenormal",		PF_getsurfacenormal,				436},		// #436 vector(entity e, float s) getsurfacenormal (DP_QC_GETSURFACE)
	{"getsurfacetexture",		PF_getsurfacetexture,				437},			// #437 string(entity e, float s) getsurfacetexture (DP_QC_GETSURFACE)
	{"getsurfacenearpoint",		PF_getsurfacenearpoint,				438},		// #438 float(entity e, vector p) getsurfacenearpoint (DP_QC_GETSURFACE)
	{"getsurfaceclippedpoint",	PF_getsurfaceclippedpoint,			439},			// #439 vector(entity e, float s, vector p) getsurfaceclippedpoint (DP_QC_GETSURFACE)

	{"clientcommand",			PF_NoCSQC,			440},		// #440 void(entity e, string s) clientcommand (KRIMZON_SV_PARSECLIENTCOMMAND) (don't implement)
	{"tokenize",				PF_Tokenize,		441},		// #441 float(string s) tokenize (KRIMZON_SV_PARSECLIENTCOMMAND)
	{"argv",					PF_ArgV,			442},		// #442 string(float n) argv (KRIMZON_SV_PARSECLIENTCOMMAND)
	{"argc",					PF_ArgC,			0},			// #0 float() argc pointless, but whatever
	{"setattachment",			PF_setattachment,	443},		// #443 void(entity e, entity tagentity, string tagname) setattachment (DP_GFX_QUAKE3MODELTAGS)
	{"search_begin",			PF_search_begin,	444},		// #444 float	search_begin(string pattern, float caseinsensitive, float quiet) (DP_QC_FS_SEARCH)

	{"search_end",				PF_search_end,			445},	// #445 void	search_end(float handle) (DP_QC_FS_SEARCH)
	{"search_getsize",			PF_search_getsize,	446},		// #446 float	search_getsize(float handle) (DP_QC_FS_SEARCH)
	{"search_getfilename",		PF_search_getfilename,447},		// #447 string	search_getfilename(float handle, float num) (DP_QC_FS_SEARCH)
	{"search_getfilesize",		PF_search_getfilesize,	0},
	{"search_getfilemtime",		PF_search_getfilemtime,	0},
	{"cvar_string",				PF_cvar_string,		448},		// #448 string(float n) cvar_string (DP_QC_CVAR_STRING)
	{"findflags",				PF_FindFlags,		449},		// #449 entity(entity start, .entity fld, float match) findflags (DP_QC_FINDFLAGS)

	{"findchainflags",			PF_findchainflags,	450},		// #450 entity(.float fld, float match) findchainflags (DP_QC_FINDCHAINFLAGS)
	{"gettagindex",				PF_gettagindex,		451},		// #451 float(entity ent, string tagname) gettagindex (DP_MD3_TAGSINFO)
	{"gettaginfo",				PF_gettaginfo,		452},		// #452 vector(entity ent, float tagindex) gettaginfo (DP_MD3_TAGSINFO)
	{"dropclient",				PF_NoCSQC,			453},		// #453 void(entity player) dropclient (DP_SV_BOTCLIENT) (don't implement)
	{"spawnclient",				PF_NoCSQC,			454},		// #454	entity() spawnclient (DP_SV_BOTCLIENT) (don't implement)

	{"clienttype",				PF_NoCSQC,			455},		// #455 float(entity client) clienttype (DP_SV_BOTCLIENT) (don't implement)


//	{"WriteUnterminatedString",PF_WriteString2,		456},	//writestring but without the null terminator. makes things a little nicer.

//DP_TE_FLAMEJET
	{"te_flamejet",				PF_cl_te_flamejet,			457},	// #457 void(vector org, vector vel, float howmany) te_flamejet

	//no 458 documented.

//DP_QC_EDICT_NUM
	{"edict_num",				PF_edict_for_num,		459},	// #459 entity(float entnum) edict_num

//DP_QC_STRINGBUFFERS
	{"buf_create",				PF_buf_create,		460},	// #460 float() buf_create
	{"buf_del",					PF_buf_del,				461},	// #461 void(float bufhandle) buf_del
	{"buf_getsize",				PF_buf_getsize,		462},	// #462 float(float bufhandle) buf_getsize
	{"buf_copy",				PF_buf_copy,		463},	// #463 void(float bufhandle_from, float bufhandle_to) buf_copy
	{"buf_sort",				PF_buf_sort,		464},	// #464 void(float bufhandle, float sortpower, float backward) buf_sort
	{"buf_implode",				PF_buf_implode,		465},	// #465 string(float bufhandle, string glue) buf_implode
	{"bufstr_get",				PF_bufstr_get,		466},	// #466 string(float bufhandle, float string_index) bufstr_get
	{"bufstr_set",				PF_bufstr_set,		467},	// #467 void(float bufhandle, float string_index, string str) bufstr_set
	{"bufstr_add",				PF_bufstr_add,		468},	// #468 float(float bufhandle, string str, float order) bufstr_add
	{"bufstr_free",				PF_bufstr_free,			469},	// #469 void(float bufhandle, float string_index) bufstr_free

	//no 470 documented

//DP_QC_ASINACOSATANATAN2TAN
	{"asin",					PF_asin,			471},	// #471 float(float s) asin
	{"acos",					PF_acos,			472},	// #472 float(float c) acos
	{"atan",					PF_atan,			473},	// #473 float(float t) atan
	{"atan2",					PF_atan2,			474},	// #474 float(float c, float s) atan2
	{"tan",						PF_tan,				475},	// #475 float(float a) tan


//DP_QC_STRINGCOLORFUNCTIONS
	{"strlennocol",				PF_strlennocol,		476},	// #476 float(string s) strlennocol
	{"strdecolorize",			PF_strdecolorize,	477},	// #477 string(string s) strdecolorize

//DP_QC_STRFTIME
	{"strftime",				PF_strftime,		478},	// #478 string(float uselocaltime, string format, ...) strftime

//DP_QC_TOKENIZEBYSEPARATOR
	{"tokenizebyseparator",		PF_tokenizebyseparator,	479},	// #479 float(string s, string separator1, ...) tokenizebyseparator

//DP_QC_STRING_CASE_FUNCTIONS
	{"strtolower",				PF_strtolower,		480},	// #476 string(string s) strtolower
	{"strtoupper",				PF_strtoupper,		481},	// #476 string(string s) strlennocol

//DP_QC_CVAR_DEFSTRING
	{"cvar_defstring",			PF_cvar_defstring,	482},	// #482 string(string s) cvar_defstring

//DP_SV_POINTSOUND
	{"pointsound",				PF_cs_pointsound,		483},	// #483 void(vector origin, string sample, float volume, float attenuation) pointsound

//DP_QC_STRREPLACE
	{"strreplace",				PF_strreplace,		484},	// #484 string(string search, string replace, string subject) strreplace
	{"strireplace",				PF_strireplace,		485},	// #485 string(string search, string replace, string subject) strireplace


//DP_QC_GETSURFACEPOINTATTRIBUTE
	{"getsurfacepointattribute",PF_getsurfacepointattribute,	486},	// #486vector(entity e, float s, float n, float a) getsurfacepointattribute

#ifdef HAVE_MEDIA_DECODER
//DP_GECKO_SUPPORT
	{"gecko_create",			PF_cs_media_create,				487},	// #487 float(string name)
	{"gecko_destroy",			PF_cs_media_destroy,			488},	// #488 void(string name)
	{"gecko_navigate",			PF_cs_media_command,			489},	// #489 void(string name, string URI)
	{"gecko_keyevent",			PF_cs_media_keyevent,			490},	// #490 float(string name, float key, float eventtype)
	{"gecko_mousemove",			PF_cs_media_mousemove,			491},	// #491 void(string name, float x, float y)
	{"gecko_resize",			PF_cs_media_resize,				492},	// #492 void(string name, float w, float h)
	{"gecko_get_texture_extent",PF_cs_media_get_texture_extent,	493},	// #493 vector(string name)
	{"gecko_getproperty",		PF_cs_media_getproperty},
	{"cin_open",				PF_cs_media_create},
	{"cin_close",				PF_cs_media_destroy},
	{"cin_setstate",			PF_cs_media_setstate},
	{"cin_getstate",			PF_cs_media_getstate},
	{"cin_restart",				PF_cs_media_restart},
#endif

//DP_QC_CRC16
	{"crc16",					PF_crc16,					494},	// #494 float(float caseinsensitive, string s, ...) crc16

//DP_QC_CVAR_TYPE
	{"cvar_type",				PF_cvar_type,				495},	// #495 float(string name) cvar_type

//DP_QC_ENTITYDATA
	{"numentityfields",			PF_numentityfields,			496},	// #496 float() numentityfields
	{"findentityfield",			PF_findentityfield,			0},
	{"entityfieldref",			PF_entityfieldref,			0},
	{"entityfieldname",			PF_entityfieldname,			497},	// #497 string(float fieldnum) entityfieldname
	{"entityfieldtype",			PF_entityfieldtype,			498},	// #498 float(float fieldnum) entityfieldtype
	{"getentityfieldstring",	PF_getentityfieldstring,	499},	// #499 string(float fieldnum, entity ent) getentityfieldstring
	{"putentityfieldstring",	PF_putentityfieldstring,	500},	// #500 float(float fieldnum, entity ent, string s) putentityfieldstring

//DP_SV_WRITEPICTURE
	{"ReadPicture",				PF_ReadPicture,				501},	// #501 void(float to, string s, float sz) WritePicture

	{"boxparticles",			PF_cs_boxparticles,			502},

//DP_QC_WHICHPACK
	{"whichpack",				PF_whichpack,				503},	// #503 string(string filename) whichpack

//DP_CSQC_QUERYRENDERENTITY
	{"getentity",				PF_getentity,				504},	// #504 __variant(float entnum, fload fieldnum) getentity

//DP_QC_URI_ESCAPE
	{"uri_escape",				PF_uri_escape,				510},	// #510 string(string in) uri_escape
	{"uri_unescape",			PF_uri_unescape,			511},	// #511 string(string in) uri_unescape = #511;

//DP_QC_NUM_FOR_EDICT
	{"num_for_edict",			PF_num_for_edict,			512},	// #512 float(entity ent) num_for_edict

//DP_QC_URI_GET
	{"uri_get",					PF_uri_get,					513},	// #513 float(string uril, float id) uri_get
	{"uri_post",				PF_uri_get,					513},	// #513 float(string uril, float id) uri_post

	{"tokenize_console",		PF_tokenize_console,		514},
	{"argv_start_index",		PF_argv_start_index,		515},
	{"argv_end_index",			PF_argv_end_index,			516},
	{"buf_cvarlist",			PF_buf_cvarlist,			517},
	{"cvar_description",		PF_cvar_description,		518},

	{"gettime",					PF_gettime,					519},

	{"keynumtostring_omgwtf",	PF_cl_keynumtostring,		520},
	{"findkeysforcommand",		PF_cl_findkeysforcommand,	521},
	{"findkeysforcommandex",	PF_cl_findkeysforcommandex,	0},

	{"loadfromdata",			PF_loadfromdata,			529},
	{"loadfromfile",			PF_loadfromfile,			530},
	{"log",						PF_Logarithm,				532},

	{"stopsound",				PF_stopsound,				0},
	{"soundupdate",				PF_soundupdate,				0},
	{"getsoundtime",			PF_getsoundtime,			533},
	{"soundlength",				PF_soundlength,				534},
	{"buf_loadfile",			PF_buf_loadfile,			535},
	{"buf_writefile",			PF_buf_writefile,			536},
//	{"bufstr_find",				PF_Fixme,					537},
//	{"matchpattern",			PF_Fixme,					538},
//	{"undefined",				PF_Fixme,					539},

#ifdef USERBE
	{"physics_enable",			PF_physics_enable,			540},
	{"physics_addforce",		PF_physics_addforce,		541},
	{"physics_addtorque",		PF_physics_addtorque,		542},
#endif

	{"setmousetarget",			PF_cl_setmousetarget,		603},
	{"getmousetarget",			PF_cl_getmousetarget,		604},

	{"callfunction",			PF_callfunction,			605},
	{"writetofile",				PF_writetofile,				606},
	{"isfunction",				PF_isfunction,				607},
	{"getresolution",			PF_cl_getresolution,		608},
	{"keynumtostring_menu",		PF_cl_keynumtostring,		609},	//while present in dp's menuqc, dp doesn't actually support keynumtostring=609 in csqc. Which is probably a good thing because csqc would have 3 separate versions if it did.

	{"findkeysforcommand_menu",	PF_cl_findkeysforcommand,	610},
	{"gethostcachevalue",		PF_cl_gethostcachevalue,	611},
	{"gethostcachestring",		PF_cl_gethostcachestring,	612},
	{"parseentitydata",			PF_parseentitydata,			613},
	{"generateentitydata",		PF_generateentitydata,		0},
	{"stringtokeynum_menu",		PF_cl_stringtokeynum,		614},

	{"resethostcachemasks",		PF_cl_resethostcachemasks,	615},
	{"sethostcachemaskstring",	PF_cl_sethostcachemaskstring,616},
	{"sethostcachemasknumber",	PF_cl_sethostcachemasknumber,617},
	{"resorthostcache",			PF_cl_resorthostcache,		618},
	{"sethostcachesort",		PF_cl_sethostcachesort,		619},
	{"refreshhostcache",		PF_cl_refreshhostcache,		620},
	{"gethostcachenumber",		PF_cl_gethostcachenumber,	621},
	{"gethostcacheindexforkey",	PF_cl_gethostcacheindexforkey,622},
	{"addwantedhostcachekey",	PF_cl_addwantedhostcachekey,	623},
#ifdef CL_MASTER
	{"getextresponse",			PF_cl_getextresponse,		624},
#endif
	{"netaddress_resolve",		PF_netaddress_resolve,		625},
	{"getgamedirinfo",			PF_cl_getgamedirinfo,		626},
	{"sprintf",					PF_sprintf,					627},
	{"getsurfacenumtriangles",	PF_getsurfacenumtriangles,628},
	{"getsurfacetriangle",		PF_getsurfacetriangle,		629},

//	{"setkeybind",				PF_Fixme,					630},
	{"getbindmaps",				PF_cl_GetBindMap,			631},
	{"setbindmaps",				PF_cl_SetBindMap,			632},

	{"digest_hex",				PF_digest_hex,				639},
	{"digest_ptr",				PF_digest_ptr,				0},
	{"V_CalcRefdef",			PF_V_CalcRefdef,			640},

	{NULL}
};

int PR_CSQC_BuiltinValid(char *name, int num)
{
	int i;
	for (i = 0; BuiltinList[i].name; i++)
	{
		if (BuiltinList[i].ebfsnum == num)
		{
			if (!strcmp(BuiltinList[i].name, name))
			{
				if (BuiltinList[i].bifunc == PF_NoCSQC || BuiltinList[i].bifunc == PF_Fixme)
					return false;
				else
					return true;
			}
		}
	}
	return false;
}

static builtin_t csqc_builtin[800];

static int PDECL PR_CSQC_MapNamedBuiltin(pubprogfuncs_t *progfuncs, int headercrc, const char *builtinname)
{
	int i, binum;
	for (i = 0;BuiltinList[i].name;i++)
	{
		if (!strcmp(BuiltinList[i].name, builtinname) && BuiltinList[i].bifunc != PF_Fixme)
		{
			for (binum = sizeof(csqc_builtin)/sizeof(csqc_builtin[0]); --binum; )
			{
				if (csqc_builtin[binum] && csqc_builtin[binum] != PF_Fixme && BuiltinList[i].bifunc)
					continue;
				csqc_builtin[binum] = BuiltinList[i].bifunc;
				return binum;
			}
			Con_Printf("No more builtin slots to allocate for %s\n", builtinname);
			break;
		}
	}
	Con_DPrintf("Unknown csqc builtin: %s\n", builtinname);
	return 0;
}




static jmp_buf csqc_abort;
static progparms_t csqcprogparms;


//Any menu builtin error or anything like that will come here.
static void VARGS CSQC_Abort (char *format, ...)	//an error occured.
{
	va_list		argptr;
	char		string[1024];

	va_start (argptr, format);
	vsnprintf (string,sizeof(string)-1, format,argptr);
	va_end (argptr);

	Con_Printf("CSQC_Abort: %s\nShutting down csqc\n", string);

	if (pr_csqc_coreonerror.value)
	{
		size_t size = 1024*1024*8;
		char *buffer = BZ_Malloc(size);
		csqcprogs->save_ents(csqcprogs, buffer, &size, size, 3);
		COM_WriteFile("csqccore.txt", FS_GAMEONLY, buffer, size);
		BZ_Free(buffer);
	}

	Host_EndGame("csqc error");
}

static void CSQC_ForgetThreads(void)
{
	csqctreadstate_t *state = csqcthreads, *next;
	csqcthreads = NULL;
	while(state)
	{
		next = state->next;

		csqcprogs->parms->memfree(state->thread);
		csqcprogs->parms->memfree(state);

		state = next;
	}
}

static void PDECL CSQC_EntSpawn (struct edict_s *e, int loading)
{
	struct csqcedict_s *ent = (csqcedict_t*)e;
#ifdef VM_Q1
	if (!ent->xv)
		ent->xv = (csqcextentvars_t *)(ent->v+1);
#endif

	if (1)
	{
//		ent->xv->dimension_see = csqc_world.dimension_default;
//		ent->xv->dimension_seen = csqc_world.dimension_default;
//		ent->xv->dimension_ghost = 0;
		ent->xv->dimension_solid = *csqcg.dimension_default;
		ent->xv->dimension_hit = *csqcg.dimension_default;
	}
}

static pbool QDECL CSQC_EntFree (struct edict_s *e)
{
	struct csqcedict_s *ent = (csqcedict_t*)e;
	ent->v->solid = SOLID_NOT;
	ent->v->movetype = 0;
	ent->v->modelindex = 0;
	ent->v->think = 0;
	ent->v->nextthink = 0;
	ent->xv->predraw = 0;
	ent->xv->drawmask = 0;
	ent->xv->renderflags = 0;

	if (ent->skinobject>0)
		Mod_WipeSkin(ent->skinobject, false);
	ent->skinobject = 0;

#ifdef USERBE
	if (csqc_world.rbe)
	{
		csqc_world.rbe->RemoveFromEntity(&csqc_world, (wedict_t*)ent);
		csqc_world.rbe->RemoveJointFromEntity(&csqc_world, (wedict_t*)ent);
	}
#endif

	return true;
}

static void QDECL CSQC_Event_Touch(world_t *w, wedict_t *s, wedict_t *o)
{
	int oself = *csqcg.self;
	int oother = *csqcg.other;

	*csqcg.self = EDICT_TO_PROG(w->progs, (edict_t*)s);
	*csqcg.other = EDICT_TO_PROG(w->progs, (edict_t*)o);
	*csqcg.simtime = w->physicstime;

	PR_ExecuteProgram (w->progs, s->v->touch);

	*csqcg.self = oself;
	*csqcg.other = oother;
}

static void QDECL CSQC_Event_Think(world_t *w, wedict_t *s)
{
	*csqcg.self = EDICT_TO_PROG(w->progs, (edict_t*)s);
	*csqcg.other = EDICT_TO_PROG(w->progs, (edict_t*)w->edicts);
	*csqcg.simtime = w->physicstime;

	if (!s->v->think)
		Con_Printf("CSQC entity \"%s\" has nextthink with no think function\n", PR_GetString(w->progs, s->v->classname));
	else
		PR_ExecuteProgram (w->progs, s->v->think);
}

static void QDECL CSQC_Event_Sound (float *origin, wedict_t *wentity, int channel, const char *sample, int volume, float attenuation, float pitchadj, float timeoffset, unsigned int flags)
{
	int i;
	vec3_t originbuf;
	if (!origin)
	{
		if (wentity->v->solid == SOLID_BSP)
		{
			origin = originbuf;
			for (i=0 ; i<3 ; i++)
				origin[i] = wentity->v->origin[i]+0.5*(wentity->v->mins[i]+wentity->v->maxs[i]);
		}
		else
			origin = wentity->v->origin;
	}

	S_StartSound(NUM_FOR_EDICT(csqcprogs, (edict_t*)wentity), channel, S_PrecacheSound(sample), origin, NULL, volume/255.0, attenuation, timeoffset, pitchadj, flags);
}

static qboolean QDECL CSQC_Event_ContentsTransition(world_t *w, wedict_t *ent, int oldwatertype, int newwatertype)
{
	if (ent->xv->contentstransition)
	{
		void *pr_globals = PR_globals(w->progs, PR_CURRENT);
		*csqcg.self = EDICT_TO_PROG(w->progs, ent);
		*csqcg.simtime = w->physicstime;
		G_FLOAT(OFS_PARM0) = oldwatertype;
		G_FLOAT(OFS_PARM1) = newwatertype;
		PR_ExecuteProgram (w->progs, ent->xv->contentstransition);
		return true;
	}
	return false;	//do legacy behaviour
}

static model_t *QDECL CSQC_World_ModelForIndex(world_t *w, int modelindex)
{
	model_t *mod = CSQC_GetModelForIndex(modelindex);
	if (mod && mod->loadstate != MLS_LOADED)
	{
		if (mod->loadstate == MLS_NOTLOADED)
			Mod_LoadModel(mod, MLV_SILENT);
		if (mod->loadstate == MLS_LOADING)
			COM_WorkerPartialSync(mod, &mod->loadstate, MLS_LOADING);
		if (mod->loadstate != MLS_LOADED)
			mod = NULL;	//gah, it failed!
	}
	return mod;
}
static void QDECL CSQC_World_GetFrameState(world_t *w, wedict_t *win, framestate_t *out)
{
	csqcedict_t *in = (csqcedict_t *)win;
	cs_getframestate(in, in->xv->renderflags, out);
}

void CSQC_Shutdown(void)
{
	int i;
	if (csqcprogs)
	{
		key_dest_absolutemouse &= ~kdm_game;
		CSQC_ForgetThreads();
		PR_ReleaseFonts(kdm_game);
		PR_Common_Shutdown(csqcprogs, false);
		World_Destroy(&csqc_world);
		csqcprogs->CloseProgs(csqcprogs);
		csqc_world.progs = csqcprogs = NULL;
	}
	else
		World_Destroy(&csqc_world);

	Cmd_RemoveCommands(CS_ConsoleCommand_f);


	Z_Free(csqcdelta_pack_new.e);
	memset(&csqcdelta_pack_new, 0, sizeof(csqcdelta_pack_new));
	Z_Free(csqcdelta_pack_old.e);
	memset(&csqcdelta_pack_old, 0, sizeof(csqcdelta_pack_old));

	memset(&deltafunction, 0, sizeof(deltafunction));
	memset(csqcdelta_playerents, 0, sizeof(csqcdelta_playerents));

	Z_Free(csqcent);
	csqcent = NULL;
	maxcsqcentities = 0;

	for (i = 0; i < MAX_CSPARTICLESPRE && cl.particle_csname[i]; i++)
	{
		free(cl.particle_csname[i]);
		cl.particle_csname[i] = NULL;
	}

	csqcmapentitydata = NULL;
	csqcmapentitydataloaded = false;

	in_sensitivityscale = 1;
	csqc_world.num_edicts = 0;
	memset(&csqc_world, 0, sizeof(csqc_world));
	memset(&csqcg, 0, sizeof(csqcg));

	if (csqc_deprecated_warned>1)
	{
		Con_Printf("total %u csqc deprecation warnings suppressed\n", csqc_deprecated_warned-1);
		csqc_deprecated_warned = 0;
	}
}

//when the qclib needs a file, it calls out to this function.
void *PDECL CSQC_PRLoadFile (const char *path, unsigned char *(PDECL *buf_get)(void *ctx, size_t len), void *buf_ctx, size_t *sz, pbool issource)
{
	extern cvar_t sv_demo_write_csqc;
	qbyte *file = NULL;

	if (!strcmp(path, "csprogs.dat"))
	{
		char newname[MAX_QPATH];
		snprintf(newname, MAX_QPATH, "csprogsvers/%x.dat", csprogs_checksum);

		//we can use FSLF_IGNOREPURE because we have our own hashes/size checks instead.
		//this should make it slightly easier for server admins
		if (csprogs_checksum)
		{
			file = COM_LoadTempFile (newname, FSLF_IGNOREPURE, sz);
			if (file)
			{
				if (cls.protocol == CP_NETQUAKE && !(cls.fteprotocolextensions2 & PEXT2_PREDINFO))
				{
					if (QCRC_Block(file, *sz) != csprogs_checksum)
						file = NULL;
				}
				else
				{
					if (LittleLong(Com_BlockChecksum(file, *sz)) != csprogs_checksum)	//and the user wasn't trying to be cunning.
						file = NULL;
				}
			}
		}

		if (!file)
		{
			file = COM_LoadTempFile (path, FSLF_IGNOREPURE, sz);

			if (file && !cls.demoplayback)	//allow them to use csprogs.dat if playing a demo, and don't care about the checksum
			{
				if (csprogs_checksum && !csprogs_promiscuous)
				{
					if (cls.protocol == CP_NETQUAKE && !(cls.fteprotocolextensions2 & PEXT2_PREDINFO))
					{
						if (QCRC_Block(file, *sz) != csprogs_checksum)
							file = NULL;
					}
					else
					{
						if (LittleLong(Com_BlockChecksum(file, *sz)) != csprogs_checksum)
							file = NULL;	//not valid
					}

					//we write the csprogs into our archive if it was loaded from outside of there.
					//this is to ensure that demos will play on the same machine later on...
					//this is unreliable though, and redundant if we're writing the csqc into the demos themselves.
					//also kinda irrelevant with sv_pure.
#ifndef FTE_TARGET_WEB
					if (file
#ifndef CLIENTONLY
						&& !sv_demo_write_csqc.ival
#endif
						)
						//back it up
						COM_WriteFile(newname, FS_GAMEONLY, file, *sz);
#endif
				}
			}
		}
	}
	else
		file = COM_LoadTempFile (path, 0, sz);

	if (file)
	{
		qbyte *buffer = buf_get(buf_ctx, *sz);
		memcpy(buffer, file, *sz);
		return buffer;
	}
	return NULL;
}

int QDECL CSQC_PRFileSize (const char *path)
{
	return COM_FileSize(path);
}

qboolean CSQC_Inited(void)
{
	if (csqcprogs)
		return true;
	return false;
}

qboolean CSQC_UnconnectedOkay(qboolean inprinciple)
{
	if (!pr_csqc_formenus.ival)
		return false;

	if (!inprinciple)
	{
		if (!csqcprogs)
			return false;
	}
	return true;
}
qboolean CSQC_UnconnectedInit(void)
{
	if (!CSQC_UnconnectedOkay(true))
		return false;

	if (csqcprogs)
		return true;
	return CSQC_Init(true, true, 0);
}
void ASMCALL CSQC_StateOp(pubprogfuncs_t *prinst, float var, func_t func)
{
	world_t *w = prinst->parms->user;
	stdentvars_t *vars = PROG_TO_EDICT(prinst, *w->g.self)->v;
	vars->nextthink = *w->g.time+0.1;
	vars->think = func;
	vars->frame = var;
}
void ASMCALL CSQC_CStateOp(pubprogfuncs_t *progs, float first, float last, func_t currentfunc)
{
	float min, max;
	float step;
	world_t *w = progs->parms->user;
	wedict_t *e = PROG_TO_WEDICT(progs, *w->g.self);
	float frame = e->v->frame;

//	if (progstype == PROG_H2)
//		e->v->nextthink = *w->g.time+0.05;
//	else
		e->v->nextthink = *w->g.time+0.1;
	e->v->think = currentfunc;

	if (csqcg.cycle_wrapped)
		*csqcg.cycle_wrapped = false;

	if (first > last)
	{	//going backwards
		min = last;
		max = first;
		step = -1.0;
	}
	else
	{	//forwards
		min = first;
		max = last;
		step = 1.0;
	}
	if (frame < min || frame > max)
		frame = first;	//started out of range, must have been a different animation
	else
	{
		frame += step;
		if (frame < min || frame > max)
		{	//became out of range, must have wrapped
			if (csqcg.cycle_wrapped)
				*csqcg.cycle_wrapped = true;
			frame = first;
		}
	}
	e->v->frame = frame;
}
static void ASMCALL CSQC_CWStateOp (pubprogfuncs_t *prinst, float first, float last, func_t currentfunc)
{
	float min, max;
	float step;
	world_t *w = prinst->parms->user;
	wedict_t *e = PROG_TO_WEDICT(prinst, *w->g.self);
	float frame = e->v->weaponframe;

//	if (progstype == PROG_H2)
//		e->v->nextthink = *w->g.time+0.05;
//	else
		e->v->nextthink = *w->g.time+0.1;
	e->v->think = currentfunc;

	if (csqcg.cycle_wrapped)
		*csqcg.cycle_wrapped = false;

	if (first > last)
	{	//going backwards
		min = last;
		max = first;
		step = -1.0;
	}
	else
	{	//forwards
		min = first;
		max = last;
		step = 1.0;
	}
	if (frame < min || frame > max)
		frame = first;	//started out of range, must have been a different animation
	else
	{
		frame += step;
		if (frame < min || frame > max)
		{	//became out of range, must have wrapped
			if (csqcg.cycle_wrapped)
				*csqcg.cycle_wrapped = true;
			frame = first;
		}
	}
	e->v->weaponframe = frame;
}
void ASMCALL CSQC_ThinkTimeOp(pubprogfuncs_t *progs, edict_t *ed, float var)
{
	world_t *w = progs->parms->user;
	stdentvars_t *vars = ed->v;
	vars->nextthink = *w->g.time+var;
}

pbool PDECL CSQC_CheckHeaderCrc(pubprogfuncs_t *progs, progsnum_t num, int crc)
{
	if (!num)
	{
		if (crc == 22390)
			;	//fte's full csqc stuff
		else if (crc == 5927)
			;	//simple csqc. but only if
#ifndef csqc_isdarkplaces
		else if (crc == 52195)
		{
			csqc_isdarkplaces = true;
			Con_DPrintf(CON_WARNING "Running darkplaces csprogs.dat version\n");
		}
#endif
		else
			Con_Printf(CON_WARNING "Running unknown csprogs.dat version\n");
	}
	return true;
}

double  csqctime;
qboolean CSQC_Init (qboolean anycsqc, qboolean csdatenabled, unsigned int checksum)
{
	int i;
	string_t *str;
	csqcedict_t *worldent;
	char *cheats;
	if (csprogs_promiscuous != anycsqc || csprogs_checksum != checksum)
		CSQC_Shutdown();
	csprogs_promiscuous = anycsqc;
	csprogs_checksum = checksum;

	csqc_mayread = false;

	csqc_singlecheats = cls.demoplayback;
	cheats = InfoBuf_ValueForKey(&cl.serverinfo, "*cheats");
	if (!Q_strcasecmp(cheats, "ON") || atoi(cheats))
		csqc_singlecheats = true;
#ifndef CLIENTONLY
	else if (sv.state == ss_active && sv.allocated_client_slots == 1)
		csqc_singlecheats = true;
#endif

	//its already running...
	if (csqcprogs)
		return false;

	if (qrenderer == QR_NONE)
	{
		return false;
	}

	if (cl_nocsqc.value)
		return false;

	if (cls.state == ca_disconnected)
	{
		movevars.gravity = 800;
		movevars.entgravity = 1;
		movevars.maxspeed = 320;
		movevars.bunnyspeedcap = 0;//pm_bunnyspeedcap.value;
		movevars.ktjump = false;//pm_ktjump.value;
		movevars.slidefix = true;//(pm_slidefix.value != 0);
		movevars.airstep = true;//(pm_airstep.value != 0);
		movevars.stepdown = true;
		movevars.walljump = false;//(pm_walljump.value);
		movevars.slidyslopes = false;//(pm_slidyslopes.value!=0);
		movevars.watersinkspeed = 60;//*pm_watersinkspeed.string?pm_watersinkspeed.value:60;
		movevars.flyfriction = 4;//*pm_flyfriction.string?pm_flyfriction.value:4;
		movevars.stepheight = PM_DEFAULTSTEPHEIGHT;
		movevars.coordsize = 4;
	}

	for (i = 0; i < sizeof(csqc_builtin)/sizeof(csqc_builtin[0]); i++)
		csqc_builtin[i] = PF_Fixme;
	for (i = 0; BuiltinList[i].bifunc; i++)
	{
		if (BuiltinList[i].ebfsnum)
			csqc_builtin[BuiltinList[i].ebfsnum] = BuiltinList[i].bifunc;
	}

	csqc_deprecated_warned = false;
	memset(cl.model_csqcname, 0, sizeof(cl.model_csqcname));
	memset(cl.model_csqcprecache, 0, sizeof(cl.model_csqcprecache));

	csqcprogparms.progsversion = PROGSTRUCT_VERSION;
	csqcprogparms.ReadFile = CSQC_PRLoadFile;
	csqcprogparms.FileSize = CSQC_PRFileSize;//int (*FileSize) (char *fname);	//-1 if file does not exist
	csqcprogparms.WriteFile = QC_WriteFile;//bool (*WriteFile) (char *name, void *data, int len);
	csqcprogparms.Printf = PR_Printf;//Con_Printf;//void (*printf) (char *, ...);
	csqcprogparms.DPrintf = PR_DPrintf;//Con_Printf;//void (*printf) (char *, ...);
	csqcprogparms.Sys_Error = Sys_Error;
	csqcprogparms.Abort = CSQC_Abort;
	csqcprogparms.CheckHeaderCrc = CSQC_CheckHeaderCrc;
	csqcprogparms.edictsize = sizeof(csqcedict_t);

	csqcprogparms.entspawn = CSQC_EntSpawn;//void (*entspawn) (struct edict_s *ent);	//ent has been spawned, but may not have all the extra variables (that may need to be set) set
	csqcprogparms.entcanfree = CSQC_EntFree;//bool (*entcanfree) (struct edict_s *ent);	//return true to stop ent from being freed
	csqcprogparms.stateop = CSQC_StateOp;//StateOp;//void (*stateop) (float var, func_t func);
	csqcprogparms.cstateop = CSQC_CStateOp;//CStateOp;
	csqcprogparms.cwstateop = CSQC_CWStateOp;//CWStateOp;
	csqcprogparms.thinktimeop = CSQC_ThinkTimeOp;//ThinkTimeOp;

	csqcprogparms.MapNamedBuiltin = PR_CSQC_MapNamedBuiltin;
	csqcprogparms.loadcompleate = NULL;//void (*loadcompleate) (int edictsize);	//notification to reset any pointers.

	csqcprogparms.memalloc = PR_CB_Malloc;//void *(*memalloc) (int size);	//small string allocation	malloced and freed randomly
	csqcprogparms.memfree = PR_CB_Free;//void (*memfree) (void * mem);


	csqcprogparms.globalbuiltins = csqc_builtin;//builtin_t *globalbuiltins;	//these are available to all progs
	csqcprogparms.numglobalbuiltins = sizeof(csqc_builtin)/sizeof(csqc_builtin[0]);

	csqcprogparms.autocompile = PR_COMPILEIGNORE;//enum {PR_NOCOMPILE, PR_COMPILENEXIST, PR_COMPILECHANGED, PR_COMPILEALWAYS} autocompile;

	csqcprogparms.gametime = &csqctime;

	csqcprogparms.sv_edicts = (struct edict_s **)&csqc_world.edicts;
	csqcprogparms.sv_num_edicts = &csqc_world.num_edicts;

	csqcprogparms.useeditor = QCEditor;//void (*useeditor) (char *filename, int line, int nump, char **parms);
	csqcprogparms.user = &csqc_world;
	csqc_world.keydestmask = kdm_game;

	csqctime = Sys_DoubleTime();
	if (!csqcprogs)
	{
		int csprogsnum = -1;
		int csaddonnum = -1;
		in_sensitivityscale = 1;
		csqcmapentitydataloaded = true;
		csqcprogs = InitProgs(&csqcprogparms);
		csqc_world.progs = csqcprogs;
		csqc_world.usesolidcorpse = true;
		PR_Configure(csqcprogs, PR_ReadBytesString(pr_csqc_memsize.string), MAX_PROGS, pr_enable_profiling.ival);
		csqc_world.worldmodel = cl.worldmodel;
		csqc_world.Event_Touch = CSQC_Event_Touch;
		csqc_world.Event_Think = CSQC_Event_Think;
		csqc_world.Event_Sound = CSQC_Event_Sound;
		csqc_world.Event_ContentsTransition = CSQC_Event_ContentsTransition;
		csqc_world.Get_CModel = CSQC_World_ModelForIndex;
		csqc_world.Get_FrameState = CSQC_World_GetFrameState;
		World_ClearWorld(&csqc_world, false);
		CSQC_InitFields();	//let the qclib know the field order that the engine needs.

		if (setjmp(csqc_abort))
		{
			CSQC_Shutdown();
			return false;
		}

#ifndef csqc_isdarkplaces
		csqc_isdarkplaces = false;
#endif
		if (csdatenabled || csqc_singlecheats || anycsqc)
			csqc_nogameaccess = false;
		else
			csqc_nogameaccess = true;

		if (!csqc_nogameaccess)
		{	//only load csprogs if its expected to be able to work without failing for game access reasons
			csprogsnum = PR_LoadProgs(csqcprogs, "csprogs.dat");
			if (csprogsnum >= 0)
				Con_DPrintf("Loaded csprogs.dat\n");
		}
		
		if (csqc_singlecheats || anycsqc)
		{
			csaddonnum = PR_LoadProgs(csqcprogs, "csaddon.dat");
			if (csaddonnum >= 0)
				Con_DPrintf("loaded csaddon.dat...\n");
			else
				Con_DPrintf("unable to find csaddon.dat.\n");
		}
		else
			Con_DPrintf("skipping csaddon.dat due to cheat restrictions\n");

		if (csprogsnum < 0 && csaddonnum < 0)		//simple csqc optionally uses the nq progs, but its explicitly limited
		{
			csqc_nogameaccess = true;
			csprogsnum = PR_LoadProgs(csqcprogs, "progs.dat");
		}

		if (csprogsnum == -1 && csaddonnum == -1)
		{
			CSQC_Shutdown();
			return false;
		}

		if (csqc_nogameaccess && !PR_FindFunction (csqcprogs, "CSQC_DrawHud", PR_ANY))
		{	//simple csqc module is not csqc. abort now.
			CSQC_Shutdown();
			Con_DPrintf("Loaded progs.dat has no CSQC_DrawHud\n");
			return false;
		}
		else if (csqc_nogameaccess)
			Con_DPrintf("Loaded [csqc]progs.dat\n");

		PR_ProgsAdded(csqcprogs, csprogsnum, "csprogs.dat");
		PR_ProgsAdded(csqcprogs, csaddonnum, "csaddon.dat");

		PF_InitTempStrings(csqcprogs);

		csqc_world.physicstime = 0;
		if (!csqc_world.worldmodel)
			csqc_world.worldmodel = Mod_ForName("", MLV_SILENT); 
		csqc_worldchanged = false;

		memset(csqcent, 0, sizeof(*csqcent)*maxcsqcentities);	//clear the server->csqc entity translations.

		for (i = 0; i < csqcprogs->numprogs; i++)
		{
			func_t f = PR_FindFunction (csqcprogs, "init", i);
			if (f)
			{
				void *pr_globals = PR_globals(csqcprogs, PR_CURRENT);
				G_PROG(OFS_PARM0) = i-1;
				PR_ExecuteProgram(csqcprogs, f);
			}
		}

		if (csqc_isdarkplaces)
			CSQC_FindGlobals(true);
		else
			CSQC_FindGlobals(false);

		csqcentsize = PR_InitEnts(csqcprogs, pr_csqc_maxedicts.value);

		//world edict becomes readonly
		worldent = (csqcedict_t *)EDICT_NUM_PB(csqcprogs, 0);
		worldent->ereftype = ER_ENTITY;

		for (i = 0; i < csqcprogs->numprogs; i++)
		{
			func_t f = PR_FindFunction (csqcprogs, "initents", i);
			if (f)
			{
				void *pr_globals = PR_globals(csqcprogs, PR_CURRENT);
				G_PROG(OFS_PARM0) = i-1;
				PR_ExecuteProgram(csqcprogs, f);
			}
		}

		/*DP compat*/
		str = (string_t*)csqcprogs->GetEdictFieldValue(csqcprogs, (edict_t*)worldent, "message", ev_string, NULL);
		if (str)
			*str = PR_NewString(csqcprogs, cl.levelname);

		str = (string_t*)PR_FindGlobal(csqcprogs, "mapname", 0, NULL);
		if (str)
		{
			char *s = InfoBuf_ValueForKey(&cl.serverinfo, "map");
			if (!*s)
				s = cl.model_name[1];
			if (!*s)
				s = "unknown";
			*str = PR_NewString(csqcprogs, s);
		}

		if (csqcg.deathmatch)
			*csqcg.deathmatch = cl.deathmatch;
		if (csqcg.coop)
			*csqcg.coop = !cl.deathmatch && cl.allocated_client_slots > 1;

		if (csqcg.init_function)
		{
			void *pr_globals = PR_globals(csqcprogs, PR_CURRENT);
			G_FLOAT(OFS_PARM0) = CSQC_API_VERSION;	//api version
			(((string_t *)pr_globals)[OFS_PARM1] = PR_TempString(csqcprogs, FULLENGINENAME));
			G_FLOAT(OFS_PARM2) = version_number();
			PR_ExecuteProgram(csqcprogs, csqcg.init_function);
		}
/*
		{
			char *watchname = "something";
			void *dbg = PR_FindGlobal(csqcprogs, watchname, 0, NULL);
			if (!csqcprogs->SetWatchPoint(csqcprogs, watchname))
				Con_Printf("Unable to watch %s\n", watchname);
		}
*/
//		csqcprogs->ToggleBreak(csqcprogs, "something", 0, 2);

		Con_DPrintf("Loaded csqc\n");
		csqcmapentitydataloaded = false;

		csqc_world.physicstime = 0.1;

		CSQC_RendererRestarted();

		if (cls.state == ca_disconnected)
			CSQC_WorldLoaded();
	}

	return true; //success!
}

void CSQC_RendererRestarted(void)
{
	int i;
	if (!csqcprogs)
		return;

	csqc_world.worldmodel = cl.worldmodel;

	for (i = 0; i < MAX_CSMODELS; i++)
	{
		cl.model_csqcprecache[i] = NULL;
	}

	//FIXME: registered shaders

	//let the csqc know that its rendertargets got purged
	if (csqcg.rendererrestarted)
	{
		void *pr_globals = PR_globals(csqcprogs, PR_CURRENT);
		(((string_t *)pr_globals)[OFS_PARM1] = PR_TempString(csqcprogs, rf->description));
		PR_ExecuteProgram(csqcprogs, csqcg.rendererrestarted);
	}
	//in case it drew to any render targets.
	if (R2D_Flush)
		R2D_Flush();
	if (*r_refdef.rt_destcolour[0].texname)
	{
		Q_strncpyz(r_refdef.rt_destcolour[0].texname, "", sizeof(r_refdef.rt_destcolour[0].texname));
		BE_RenderToTextureUpdate2d(true);
	}
}

void CSQC_WorldLoaded(void)
{
	csqcedict_t *worldent;
	int tmp;
	int wmodelindex;

	if (!csqcprogs)
		return;
	if (csqcmapentitydataloaded)
		return;

	if (csqc_isdarkplaces)
		CSQC_FindGlobals(false);

	csqcmapentitydataloaded = true;
	csqcmapentitydata = Mod_GetEntitiesString(cl.worldmodel);

	csqc_world.worldmodel = cl.worldmodel;
	World_RBE_Start(&csqc_world);

	worldent = (csqcedict_t *)EDICT_NUM_PB(csqcprogs, 0);
	worldent->v->solid = SOLID_BSP;
	wmodelindex = CS_FindModel(cl.worldmodel?cl.worldmodel->name:"", &tmp);
	tmp = csqc_worldchanged;
	csqc_setmodel(csqcprogs, worldent, wmodelindex);
	csqc_worldchanged = tmp;

	worldent->readonly = false;	//just in case

	World_ClearWorld(&csqc_world, true);

	if (csqc_isdarkplaces)
	{
		if (csqcg.init_function)
		{
			void *pr_globals = PR_globals(csqcprogs, PR_CURRENT);
			G_FLOAT(OFS_PARM0) = CSQC_API_VERSION;	//api version
			(((string_t *)pr_globals)[OFS_PARM1] = PR_TempString(csqcprogs, FULLENGINENAME));
			G_FLOAT(OFS_PARM2) = version_number();
			PR_ExecuteProgram(csqcprogs, csqcg.init_function);
		}
	}

	if (csqcg.worldloaded)
		PR_ExecuteProgram(csqcprogs, csqcg.worldloaded);
	csqcmapentitydata = NULL;

	worldent->readonly = true;
}

void CSQC_CoreDump(void)
{
	if (!csqcprogs)
	{
		Con_Printf("Can't core dump, you need to be running the CSQC progs first.");
		return;
	}

	{
		size_t size = 1024*1024*8;
		char *buffer = BZ_Malloc(size);
		csqcprogs->save_ents(csqcprogs, buffer, &size, size, 3);
		COM_WriteFile("csqccore.txt", FS_GAMEONLY, buffer, size);
		BZ_Free(buffer);
	}

}

void PR_CSExtensionList_f(void)
{
	int i;
	int ebi;
	int bi;
	lh_extension_t *extlist;

#define SHOW_ACTIVEEXT 1
#define SHOW_ACTIVEBI 2
#define SHOW_NOTSUPPORTEDEXT 4
#define SHOW_NOTACTIVEEXT 8
#define SHOW_NOTACTIVEBI 16

	int showflags = atoi(Cmd_Argv(1));
	if (!showflags)
		showflags = SHOW_ACTIVEEXT|SHOW_NOTACTIVEEXT;

	//make sure the info is valid
	if (!csqc_builtin[0])
	{
		for (i = 0; i < sizeof(csqc_builtin)/sizeof(csqc_builtin[0]); i++)
			csqc_builtin[i] = PF_Fixme;
		for (i = 0; BuiltinList[i].bifunc; i++)
		{
			if (BuiltinList[i].ebfsnum)
				csqc_builtin[BuiltinList[i].ebfsnum] = BuiltinList[i].bifunc;
		}
	}


	if (showflags & (SHOW_ACTIVEBI|SHOW_NOTACTIVEBI))
	for (i = 0; BuiltinList[i].name; i++)
	{
		if (!BuiltinList[i].ebfsnum)
			continue;	//a reserved builtin.
		if (BuiltinList[i].bifunc == PF_Fixme)
			Con_Printf("^1%s:%i needs to be added\n", BuiltinList[i].name, BuiltinList[i].ebfsnum);
		else if (csqc_builtin[BuiltinList[i].ebfsnum] == BuiltinList[i].bifunc)
		{
			if (showflags & SHOW_ACTIVEBI)
				Con_Printf("%s is active on %i\n", BuiltinList[i].name, BuiltinList[i].ebfsnum);
		}
		else
		{
			if (showflags & SHOW_NOTACTIVEBI)
				Con_Printf("^4%s is NOT active (%i)\n", BuiltinList[i].name, BuiltinList[i].ebfsnum);
		}
	}

	if (showflags & (SHOW_NOTSUPPORTEDEXT|SHOW_NOTACTIVEEXT|SHOW_ACTIVEEXT))
	{
		extlist = QSG_Extensions;

		for (i = 0; i < QSG_Extensions_count; i++)
		{
			if (!extlist[i].name)
				continue;

			if (i < 32)
			{
				if (!(cls.fteprotocolextensions & (1<<i)))
				{
					if (showflags & SHOW_NOTSUPPORTEDEXT)
						Con_Printf("^4protocol %s is not supported\n", extlist[i].name);
					continue;
				}
			}

			for (ebi = 0; ebi < extlist[i].numbuiltins; ebi++)
			{
				for (bi = 0; BuiltinList[bi].name; bi++)
				{
					if (!strcmp(BuiltinList[bi].name, extlist[i].builtinnames[ebi]))
						break;
				}

				if (!BuiltinList[bi].name)
				{
					if (showflags & SHOW_NOTSUPPORTEDEXT)
						Con_Printf("^4%s is not supported\n", extlist[i].name);
					break;
				}
				if (csqc_builtin[BuiltinList[bi].ebfsnum] != BuiltinList[bi].bifunc)
				{
					if (csqc_builtin[BuiltinList[bi].ebfsnum] == PF_Fixme)
					{
						if (showflags & SHOW_NOTACTIVEEXT)
							Con_Printf("^4%s is not currently active (builtin: %s#%i)\n", extlist[i].name, BuiltinList[bi].name, BuiltinList[bi].ebfsnum);
					}
					else
					{
						if (showflags & SHOW_NOTACTIVEEXT)
							Con_Printf("^4%s was overridden (builtin: %s#%i)\n", extlist[i].name, BuiltinList[bi].name, BuiltinList[bi].ebfsnum);
					}
					break;
				}
			}
			if (ebi == extlist[i].numbuiltins)
			{
				if (showflags & SHOW_ACTIVEEXT)
				{
					if (!extlist[i].numbuiltins)
						Con_Printf("%s is supported\n", extlist[i].name);
					else
						Con_Printf("%s is currently active\n", extlist[i].name);
				}
			}
		}
	}
}

void CSQC_Breakpoint_f(void)
{
	int wasset;
	int isset;
	char *filename = Cmd_Argv(1);
	int line = atoi(Cmd_Argv(2));

	if (!csqcprogs)
	{
		Con_Printf("CSQC not running\n");
		return;
	}
	wasset = csqcprogs->ToggleBreak(csqcprogs, filename, line, 3);
	isset = csqcprogs->ToggleBreak(csqcprogs, filename, line, 2);

	if (wasset == isset)
		Con_Printf("Breakpoint was not valid\n");
	else if (isset)
		Con_Printf("Breakpoint has been set\n");
	else
		Con_Printf("Breakpoint has been cleared\n");

//	Cvar_Set(Cvar_FindVar("pr_debugger"), "1");
}

static void CSQC_Poke_f(void)
{
	if (!csqc_singlecheats && cls.state)
		Con_Printf("%s is a cheat command\n", Cmd_Argv(0));
	else if (csqcprogs)
		Con_Printf("Result: %s\n", csqcprogs->EvaluateDebugString(csqcprogs, Cmd_Args()));
	else
		Con_Printf("csqc not running\n");
}
void CSQC_WatchPoint_f(void)
{
	char *variable = Cmd_Argv(1);
	if (!*variable)
		variable = NULL;

	if (!csqc_singlecheats)
		Con_Printf("%s is a cheat command\n", Cmd_Argv(0));
	else if (!csqcprogs)
	{
		Con_Printf("csqc not running\n");
		return;
	}
	if (csqcprogs->SetWatchPoint(csqcprogs, variable))
		Con_Printf("Watchpoint set\n");
	else
		Con_Printf("Watchpoint cleared\n");
}
void PR_CSProfile_f(void)
{
	if (csqcprogs && csqcprogs->DumpProfile)
		if (!csqcprogs->DumpProfile(csqcprogs, !atof(Cmd_Argv(1))))
			Con_Printf("Enabled csqc Profiling.\n");
}

static void CSQC_GameCommand_f(void);
void CSQC_RegisterCvarsAndThings(void)
{
	Cmd_AddCommand("coredump_csqc", CSQC_CoreDump);
	Cmd_AddCommand ("extensionlist_csqc", PR_CSExtensionList_f);
	Cmd_AddCommandD("cl_cmd", CSQC_GameCommand_f, "Calls the csqc's GameCommand function");
	Cmd_AddCommand("breakpoint_csqc", CSQC_Breakpoint_f);
	Cmd_AddCommand ("watchpoint_csqc", CSQC_WatchPoint_f);
	Cmd_AddCommandD("poke_csqc", CSQC_Poke_f, "Allows you to inspect/debug ");
	Cmd_AddCommand ("profile_csqc", PR_CSProfile_f);

	Cvar_Register(&pr_csqc_formenus, CSQCPROGSGROUP);
	Cvar_Register(&pr_csqc_memsize, CSQCPROGSGROUP);
	Cvar_Register(&pr_csqc_maxedicts, CSQCPROGSGROUP);
	Cvar_Register(&cl_csqcdebug, CSQCPROGSGROUP);
	Cvar_Register(&cl_nocsqc, CSQCPROGSGROUP);
	Cvar_Register(&pr_csqc_coreonerror, CSQCPROGSGROUP);
}

void CSQC_CvarChanged(cvar_t *var)
{
	if (csqcprogs)
	{
		PR_AutoCvar(csqcprogs, var);
	}
}

qboolean CSQC_UseGamecodeLoadingScreen(void)
{
	return csqcprogs && csqcg.f_updateviewloading;
}

//evil evil function. calling qc from inside the renderer is BAD.
qboolean CSQC_SetupToRenderPortal(int entkeynum)
{
#ifdef TEXTEDITOR
	if (editormodal)
		return false;
#endif

	if (csqcprogs && entkeynum < 0)
	{
		csqcedict_t *e = (void*)EDICT_NUM_UB(csqcprogs, -entkeynum);
		if (e->xv->camera_transform)
		{
			int oself = *csqcg.self;
			void *pr_globals = PR_globals(csqcprogs, PR_CURRENT);

			*csqcg.self = EDICT_TO_PROG(csqcprogs, e);
			VectorCopy(r_refdef.vieworg, G_VECTOR(OFS_PARM0));
			VectorAngles(vpn, vup, G_VECTOR(OFS_PARM1), true/*FIXME*/);
			VectorCopy(vpn, csqcg.forward);
			VectorCopy(vright, csqcg.right);
			VectorCopy(vup, csqcg.up);
			VectorCopy(r_refdef.vieworg/*r_refdef.pvsorigin*/, csqcg.trace_endpos);

			PR_ExecuteProgram (csqcprogs, e->xv->camera_transform);

			VectorCopy(csqcg.forward, vpn);
			VectorCopy(csqcg.right, vright);
			VectorCopy(csqcg.up, vup);
			VectorCopy(G_VECTOR(OFS_RETURN), r_refdef.vieworg);
			VectorCopy(csqcg.trace_endpos, r_refdef.pvsorigin);

			*csqcg.self = oself;
			return true;
		}
	}
	return false;
}

qboolean CSQC_DrawView(void)
{
	int ticlimit = 10;
	float mintic = 0.01;
	double clframetime = host_frametime;
	RSpeedLocals();

	csqc_resortfrags = true;
	csqctime = Sys_DoubleTime();

	if (!csqcg.f_updateview || !csqcprogs)
		return false;

	if (cls.state < ca_active && !CSQC_UnconnectedOkay(false))
		return false;

	r_secondaryview = 0;

	if (csqcg.frametime)
		*csqcg.frametime = host_frametime;

	csqc_dp_lastwas3d = false;

	RSpeedRemark();
	if (csqc_isdarkplaces && *csqc_world.g.physics_mode == 1)
	{
		csqc_world.physicstime = cl.servertime;
	}
	else
	{
		while(1)
		{
			host_frametime = cl.servertime - csqc_world.physicstime;
			if (host_frametime < mintic)
				break;
			if (!--ticlimit)
			{
				csqc_world.physicstime = cl.servertime;
				break;
			}
			if (host_frametime > mintic)
				host_frametime = mintic;

#ifdef USERBE
			if (csqc_world.rbe)
			{
#ifdef RAGDOLL
				rag_doallanimations(&csqc_world);
#endif
				csqc_world.rbe->RunFrame(&csqc_world, host_frametime, 800);
			}
#endif

			World_Physics_Frame(&csqc_world);
			csqc_world.physicstime += host_frametime;
		}
	}
	RSpeedEnd(RSPEED_CSQCPHYSICS);

	RSpeedRemark();

	host_frametime = clframetime;

	if (csqcg.frametime)
	{
		if (1)//csqc_isdarkplaces)
		{
			if (cl.paused)
				*csqcg.frametime = 0;	//apparently people can't cope with microstutter when they're using this as a test to see if the game is paused.
			else
				*csqcg.frametime = bound(0, cl.time - cl.lasttime, 0.1);
		}
		else
			*csqcg.frametime = host_frametime;
	}

	if (csqcg.clframetime)
		*csqcg.clframetime = host_frametime;

	if (csqcg.numclientseats)
		*csqcg.numclientseats = cl.splitclients;

	if (csqcg.intermission)
		*csqcg.intermission = cl.intermissionmode;
	if (csqcg.intermission_time)
		*csqcg.intermission_time = cl.completed_time;

	//work out which packet entities are solid
	CL_SetSolidEntities ();
	CL_TransitionEntities();
	if (cl.worldmodel)
		CL_PredictMove ();

	if (csqcg.cltime)
		*csqcg.cltime = realtime;
	if (csqcg.simtime)
		*csqcg.simtime = cl.servertime;
	if (csqcg.clientcommandframe)
		*csqcg.clientcommandframe = cl.movesequence;
	if (csqcg.servercommandframe)
		*csqcg.servercommandframe = cl.ackedmovesequence;
	if (csqcg.gamespeed)
		*csqcg.gamespeed = cl.gamespeed;
	if (cl.paused)
	{
		if (csqcg.gamespeed)
			*csqcg.gamespeed = 0;
	}
	if (cl.currentpackentities && cl.previouspackentities)
	{
		if (csqcg.netnewtime)
			*csqcg.netnewtime = cl.currentpackentities->servertime;
		if (csqcg.netoldtime)
			*csqcg.netoldtime = cl.previouspackentities->servertime;
		if (csqcg.netdeltatime)
			*csqcg.netdeltatime = cl.currentpackentities->servertime - cl.previouspackentities->servertime;
	}

	//always revert to a usable default.
	CSQC_ChangeLocalPlayer(cl_forceseat.ival?(cl_forceseat.ival - 1) % cl.splitclients:0);
	DropPunchAngle (csqc_playerview);	//FIXME: this seems like the wrong place for this.
	if (cl.worldmodel)
		Surf_LessenStains();

	CSQC_RunThreads();	//wake up any qc threads

#ifndef NOLEGACY
	if (csqcg.autocvar_vid_conwidth)
		*csqcg.autocvar_vid_conwidth = vid.width;
	if (csqcg.autocvar_vid_conheight)
		*csqcg.autocvar_vid_conheight = vid.height;
#endif

	{
		void *pr_globals = PR_globals(csqcprogs, PR_CURRENT);
		if (csqc_isdarkplaces)
		{	//fucked for compatibility.
			G_FLOAT(OFS_PARM0) = vid.pixelwidth;
			G_FLOAT(OFS_PARM1) = vid.pixelheight;
		}
		else
		{
			G_FLOAT(OFS_PARM0) = vid.width;
			G_FLOAT(OFS_PARM1) = vid.height;
		}
		G_FLOAT(OFS_PARM2) = !Key_Dest_Has(kdm_emenu|kdm_gmenu|kdm_cwindows) && !r_refdef.eyeoffset[0] && !r_refdef.eyeoffset[1];

		if (csqcg.f_updateviewloading && cls.state && cls.state < ca_active)
			PR_ExecuteProgram(csqcprogs, csqcg.f_updateviewloading);
		else
			PR_ExecuteProgram(csqcprogs, csqcg.f_updateview);
	}

	if (*r_refdef.rt_destcolour[0].texname)
	{
		if (R2D_Flush)
			R2D_Flush();
		Q_strncpyz(r_refdef.rt_destcolour[0].texname, "", sizeof(r_refdef.rt_destcolour[0].texname));
		BE_RenderToTextureUpdate2d(true);
	}

	RSpeedEnd(RSPEED_CSQCREDRAW);


	return true;
}

qboolean CSQC_DrawHud(playerview_t *pv)
{
	if (csqcg.f_drawhud && (pv==cl.playerview/* || csqcg.numclientseats*/))
	{
		RSpeedMark();
		void *pr_globals = PR_globals(csqcprogs, PR_CURRENT);

		//set csqc globals
		CSQC_ChangeLocalPlayer(pv-cl.playerview);
		if (csqcg.numclientseats)
			*csqcg.numclientseats = cl.splitclients;
		if (csqcg.simtime)
			*csqcg.simtime = cl.time;
		if (csqcg.frametime)
			*csqcg.frametime = host_frametime;
		if (csqcg.cltime)
			*csqcg.cltime = realtime;

		G_FLOAT(OFS_PARM0+0) = r_refdef.grect.width;
		G_FLOAT(OFS_PARM0+1) = r_refdef.grect.height;
		G_FLOAT(OFS_PARM0+2) = 0;
#ifdef QUAKEHUD
		G_FLOAT(OFS_PARM1) = (pv->sb_showscores?1:0) | (pv->sb_showteamscores?2:0);
#else
		G_FLOAT(OFS_PARM1) = false;	//hmm
#endif
		G_FLOAT(OFS_PARM2+0) = 0;//r_refdef.grect.x;
		G_FLOAT(OFS_PARM2+1) = 0;//r_refdef.grect.y;
		G_FLOAT(OFS_PARM2+2) = 0;//pv-cl.playerview;
		PR_ExecuteProgram(csqcprogs, csqcg.f_drawhud);

		if (*r_refdef.rt_destcolour[0].texname)
		{
			if (R2D_Flush)
				R2D_Flush();
			Q_strncpyz(r_refdef.rt_destcolour[0].texname, "", sizeof(r_refdef.rt_destcolour[0].texname));
			BE_RenderToTextureUpdate2d(true);
		}

		RSpeedEnd(RSPEED_CSQCREDRAW);
		return true;
	}
	return false;
}
qboolean CSQC_DrawScores(playerview_t *pv)
{
	if (csqcg.f_drawscores && (pv==cl.playerview/* || csqcg.numclientseats*/))
	{
		RSpeedMark();
		void *pr_globals = PR_globals(csqcprogs, PR_CURRENT);

		//set csqc globals (in case CSQC_DrawHud wasn't implemented)
		CSQC_ChangeLocalPlayer(pv-cl.playerview);
		if (csqcg.numclientseats)
			*csqcg.numclientseats = cl.splitclients;
		if (csqcg.simtime)
			*csqcg.simtime = cl.time;
		if (csqcg.frametime)
			*csqcg.frametime = host_frametime;
		if (csqcg.cltime)
			*csqcg.cltime = realtime;

		G_FLOAT(OFS_PARM0+0) = r_refdef.grect.width;
		G_FLOAT(OFS_PARM0+1) = r_refdef.grect.height;
		G_FLOAT(OFS_PARM0+2) = 0;
#ifdef QUAKEHUD
		G_FLOAT(OFS_PARM1) = (pv->sb_showscores?1:0) | (pv->sb_showteamscores?2:0);
#else
		G_FLOAT(OFS_PARM1) = false;	//hmm
#endif
		G_FLOAT(OFS_PARM2+0) = 0;//r_refdef.grect.x;
		G_FLOAT(OFS_PARM2+1) = 0;//r_refdef.grect.y;
		G_FLOAT(OFS_PARM2+2) = 0;//pv-cl.playerview;
		PR_ExecuteProgram(csqcprogs, csqcg.f_drawscores);

		if (*r_refdef.rt_destcolour[0].texname)
		{
			if (R2D_Flush)
				R2D_Flush();
			Q_strncpyz(r_refdef.rt_destcolour[0].texname, "", sizeof(r_refdef.rt_destcolour[0].texname));
			BE_RenderToTextureUpdate2d(true);
		}

		RSpeedEnd(RSPEED_CSQCREDRAW);
		return true;
	}
	return false;
}

qboolean CSQC_KeyPress(int key, int unicode, qboolean down, unsigned int devid)
{
	static qbyte csqckeysdown[K_MAX];
	void *pr_globals;

	if (!csqcprogs || !csqcg.input_event)
		return false;
#ifdef TEXTEDITOR
	if (editormodal)
		return false;
#endif

	pr_globals = PR_globals(csqcprogs, PR_CURRENT);
	G_FLOAT(OFS_PARM0) = down?CSIE_KEYDOWN:CSIE_KEYUP;
	G_FLOAT(OFS_PARM1) = MP_TranslateFTEtoQCCodes(key);
	G_FLOAT(OFS_PARM2) = unicode;
	G_FLOAT(OFS_PARM3) = devid;

	//small sanity check, so things don't break too much if things get big.
	if ((unsigned)devid >= sizeof(csqckeysdown[0])*8)
		devid = sizeof(csqckeysdown[0])*8-1;
	if (key < 0 || key >= K_MAX)
		key = 0;	//panic. everyone panic.

	if (down)
	{
		qcinput_scan = G_FLOAT(OFS_PARM1);
		qcinput_unicode = G_FLOAT(OFS_PARM2);

		csqckeysdown[key] |= (1u<<devid);
	}
	else
	{
		if (key && !(csqckeysdown[key] & (1u<<devid)))
			return false;	//prevent up events being able to leak 
		csqckeysdown[key] &= ~(1u<<devid);
	}
	PR_ExecuteProgram (csqcprogs, csqcg.input_event);
	qcinput_scan = 0;	//and stop replay attacks
	qcinput_unicode = 0;

	return G_FLOAT(OFS_RETURN);
}
qboolean CSQC_MousePosition(float xabs, float yabs, unsigned int devid)
{
	void *pr_globals;

	if (!csqcprogs || !csqcg.input_event)
		return false;

	pr_globals = PR_globals(csqcprogs, PR_CURRENT);
	G_FLOAT(OFS_PARM0) = CSIE_MOUSEABS;
	G_FLOAT(OFS_PARM1) = (xabs * vid.width) / vid.pixelwidth;
	G_FLOAT(OFS_PARM2) = (yabs * vid.height) / vid.pixelheight;
	G_FLOAT(OFS_PARM3) = devid;

	PR_ExecuteProgram (csqcprogs, csqcg.input_event);

	return G_FLOAT(OFS_RETURN);
}
qboolean CSQC_MouseMove(float xdelta, float ydelta, unsigned int devid)
{
	void *pr_globals;

	if (!csqcprogs || !csqcg.input_event)
		return false;

	pr_globals = PR_globals(csqcprogs, PR_CURRENT);
	G_FLOAT(OFS_PARM0) = CSIE_MOUSEDELTA;
	G_FLOAT(OFS_PARM1) = (xdelta * vid.width) / vid.pixelwidth;
	G_FLOAT(OFS_PARM2) = (ydelta * vid.height) / vid.pixelheight;
	G_FLOAT(OFS_PARM3) = devid;

	PR_ExecuteProgram (csqcprogs, csqcg.input_event);

	return G_FLOAT(OFS_RETURN);
}

qboolean CSQC_JoystickAxis(int axis, float value, unsigned int devid)
{
	void *pr_globals;
	if (!csqcprogs || !csqcg.input_event)
		return false;
	pr_globals = PR_globals(csqcprogs, PR_CURRENT);

	G_FLOAT(OFS_PARM0) = CSIE_JOYAXIS;
	G_FLOAT(OFS_PARM1) = axis;
	G_FLOAT(OFS_PARM2) = value;
	G_FLOAT(OFS_PARM3) = devid;
	PR_ExecuteProgram (csqcprogs, csqcg.input_event);
	return G_FLOAT(OFS_RETURN);
}

qboolean CSQC_Accelerometer(float x, float y, float z)
{
	void *pr_globals;
	if (!csqcprogs || !csqcg.input_event)
		return false;
	pr_globals = PR_globals(csqcprogs, PR_CURRENT);

	G_FLOAT(OFS_PARM0) = CSIE_ACCELEROMETER;
	G_FLOAT(OFS_PARM1) = x;
	G_FLOAT(OFS_PARM2) = y;
	G_FLOAT(OFS_PARM3) = z;
	PR_ExecuteProgram (csqcprogs, csqcg.input_event);
	return G_FLOAT(OFS_RETURN);
}
qboolean CSQC_Gyroscope(float x, float y, float z)
{
	void *pr_globals;
	if (!csqcprogs || !csqcg.input_event)
		return false;
	pr_globals = PR_globals(csqcprogs, PR_CURRENT);

	G_FLOAT(OFS_PARM0) = CSIE_GYROSCOPE;
	G_FLOAT(OFS_PARM1) = x;
	G_FLOAT(OFS_PARM2) = y;
	G_FLOAT(OFS_PARM3) = z;
	PR_ExecuteProgram (csqcprogs, csqcg.input_event);
	return G_FLOAT(OFS_RETURN);
}

qboolean CSQC_ConsoleLink(char *text, char *info)
{
	void *pr_globals;
	if (!csqcprogs || !csqcg.console_link)
		return false;

	pr_globals = PR_globals(csqcprogs, PR_CURRENT);
	(((string_t *)pr_globals)[OFS_PARM1] = PR_TempString(csqcprogs, info));
	*info = 0;
	(((string_t *)pr_globals)[OFS_PARM0] = PR_TempString(csqcprogs, text));
	*info = '\\';
	PR_ExecuteProgram (csqcprogs, csqcg.console_link);
	return G_FLOAT(OFS_RETURN);
}

qboolean CSQC_ConsoleCommand(int seat, const char *cmd)
{
	void *pr_globals;
	if (!csqcprogs || !csqcg.console_command)
		return false;
#ifdef TEXTEDITOR
	if (editormodal)
		return false;
#endif

	if (seat < 0)
		seat = CL_TargettedSplit(false);
	CSQC_ChangeLocalPlayer(seat);

	pr_globals = PR_globals(csqcprogs, PR_CURRENT);
	(((string_t *)pr_globals)[OFS_PARM0] = PR_TempString(csqcprogs, cmd));

	PR_ExecuteProgram (csqcprogs, csqcg.console_command);
	return G_FLOAT(OFS_RETURN);
}
static void CSQC_GameCommand_f(void)
{
	void *pr_globals;
	if (!csqcprogs || !csqcg.gamecommand)
		return;

	pr_globals = PR_globals(csqcprogs, PR_CURRENT);
	(((string_t *)pr_globals)[OFS_PARM0] = PR_TempString(csqcprogs, Cmd_Args()));

	PR_ExecuteProgram (csqcprogs, csqcg.gamecommand);
}

qboolean CSQC_ParseTempEntity(void)
{
	int orc;
	void *pr_globals;
	if (!csqcprogs || !csqcg.parse_tempentity)
		return false;

	pr_globals = PR_globals(csqcprogs, PR_CURRENT);
	csqc_mayread = true;
	orc = msg_readcount;
	PR_ExecuteProgram (csqcprogs, csqcg.parse_tempentity);
	csqc_mayread = false;
	if (G_FLOAT(OFS_RETURN))
		return true;
	//failed. reset the read position.
	msg_readcount = orc;
	msg_badread = false;
	return false;
}

qboolean CSQC_ParseGamePacket(int seat)
{
	int parsefnc = csqcg.parse_event?csqcg.parse_event:csqcg.parse_tempentity;

	if (cl.csqcdebug)
	{
		int len = (unsigned short)MSG_ReadShort();
		int start = msg_readcount;

		if (!csqcprogs || !parsefnc)
		{
			MSG_ReadSkip(len);
			return false;
		}

		csqc_mayread = true;
		CSQC_ChangeLocalPlayer(seat);
		PR_ExecuteProgram (csqcprogs, parsefnc);

		if (msg_readcount != start + len)
		{
			Con_Printf("Gamecode misread a gamecode packet (%i bytes too much)\n", msg_readcount - (start+len));
			msg_readcount = start + len;
		}
	}
	else
	{
		if (!csqcprogs || !parsefnc)
		{
			Host_EndGame("CSQC not running or is unable to parse events.\n");
			return false;
		}
		csqc_mayread = true;
		CSQC_ChangeLocalPlayer(seat);
		PR_ExecuteProgram (csqcprogs, parsefnc);
	}
	csqc_mayread = false;
	return true;
}

void CSQC_MapEntityEdited(int modelindex, int idx, const char *newe)
{
	void *pr_globals;
	if (!csqcprogs || !csqcg.mapentityedited)
		return;

	if (modelindex != 1)
		return;

	pr_globals = PR_globals(csqcprogs, PR_CURRENT);
	G_INT(OFS_PARM0) = idx;
	(((string_t *)pr_globals)[OFS_PARM1] = PR_TempString(csqcprogs, newe));
	PR_ExecuteProgram (csqcprogs, csqcg.mapentityedited);
}

/*qboolean CSQC_LoadResource(char *resname, char *restype)
{
	void *pr_globals;
	if (!csqcprogs || !csqcg.loadresource)
		return true;

	pr_globals = PR_globals(csqcprogs, PR_CURRENT);
	(((string_t *)pr_globals)[OFS_PARM0] = PR_TempString(csqcprogs, resname));
	(((string_t *)pr_globals)[OFS_PARM1] = PR_TempString(csqcprogs, restype));

	PR_ExecuteProgram (csqcprogs, csqcg.loadresource);

	return !!G_FLOAT(OFS_RETURN);
}*/

qboolean CSQC_Parse_Damage(int seat, float save, float take, vec3_t source)
{
	void *pr_globals;
	if (!csqcprogs || !csqcg.parse_damage)
		return false;

	CSQC_ChangeLocalPlayer(seat);
	
	pr_globals = PR_globals(csqcprogs, PR_CURRENT);
	((float *)pr_globals)[OFS_PARM0] = save;
	((float *)pr_globals)[OFS_PARM1] = take;
	((float *)pr_globals)[OFS_PARM2+0] = source[0];
	((float *)pr_globals)[OFS_PARM2+1] = source[1];
	((float *)pr_globals)[OFS_PARM2+2] = source[2];
	PR_ExecuteProgram (csqcprogs, csqcg.parse_damage);

	return G_FLOAT(OFS_RETURN);
}

qboolean CSQC_ParsePrint(char *message, int printlevel)
{
	void *pr_globals;
	int bufferpos;
	char *nextline;
	qboolean doflush;
	if (!csqcprogs || !csqcg.parse_print)
	{
		return false;
	}

	bufferpos = strlen(csqc_printbuffer);

	//fix-up faked bot chat
	if (*message == '\1' && *csqc_printbuffer == '\1')
		message++;

	while(*message)
	{
		nextline = strchr(message, '\n');
		if (nextline)
		{
			nextline+=1;
			doflush = true;
		}
		else
		{
			nextline = message+strlen(message);
			doflush = false;
		}

		if (bufferpos + nextline-message >= sizeof(csqc_printbuffer))
		{
			//if this would overflow the buffer, cap its length and flush the buffer
			//this copes with too many strings and too long strings.
			nextline = message + sizeof(csqc_printbuffer)-1 - bufferpos;
			doflush = true;
		}

		memcpy(csqc_printbuffer+bufferpos, message, nextline-message);
		bufferpos += nextline-message;
		csqc_printbuffer[bufferpos] = '\0';
		message = nextline;

		if (doflush)
		{
			pr_globals = PR_globals(csqcprogs, PR_CURRENT);
			(((string_t *)pr_globals)[OFS_PARM0] = PR_TempString(csqcprogs, csqc_printbuffer));
			G_FLOAT(OFS_PARM1) = printlevel;
			PR_ExecuteProgram (csqcprogs, csqcg.parse_print);

			bufferpos = 0;
			csqc_printbuffer[bufferpos] = 0;
		}
	}
	return true;
}

qboolean CSQC_StuffCmd(int lplayernum, char *cmd, char *cmdend)
{
	void *pr_globals;
	char tmp[2];
	if (!csqcprogs || !csqcg.parse_stuffcmd)
		return false;

	CSQC_ChangeLocalPlayer(lplayernum);

	pr_globals = PR_globals(csqcprogs, PR_CURRENT);
	tmp[0] = cmdend[0];
	tmp[1] = cmdend[1];
	cmdend[0] = '\n';
	cmdend[1] = 0;
	(((string_t *)pr_globals)[OFS_PARM0] = PR_TempString(csqcprogs, cmd));
	cmdend[0] = tmp[0];
	cmdend[1] = tmp[1];

	PR_ExecuteProgram (csqcprogs, csqcg.parse_stuffcmd);
	return true;
}
qboolean CSQC_CenterPrint(int seat, const char *cmd)
{
	void *pr_globals;
	if (!csqcprogs || !csqcg.parse_centerprint)
		return false;

	CSQC_ChangeLocalPlayer(seat);

	pr_globals = PR_globals(csqcprogs, PR_CURRENT);
	(((string_t *)pr_globals)[OFS_PARM0] = PR_TempString(csqcprogs, cmd));

	PR_ExecuteProgram (csqcprogs, csqcg.parse_centerprint);
	return G_FLOAT(OFS_RETURN) || csqc_isdarkplaces;
}

qboolean CSQC_Parse_SetAngles(int seat, vec3_t newangles, qboolean wasdelta)
{
	void *pr_globals;
	if (!csqcprogs || !csqcg.parse_setangles)
		return false;

	CSQC_ChangeLocalPlayer(seat);

	pr_globals = PR_globals(csqcprogs, PR_CURRENT);
	((float *)pr_globals)[OFS_PARM0+0] = newangles[0];
	((float *)pr_globals)[OFS_PARM0+1] = newangles[1];
	((float *)pr_globals)[OFS_PARM0+2] = newangles[2];
	((float *)pr_globals)[OFS_PARM1] = wasdelta;

	PR_ExecuteProgram (csqcprogs, csqcg.parse_setangles);
	return G_FLOAT(OFS_RETURN);
}

void CSQC_Input_Frame(int seat, usercmd_t *cmd)
{
	if (!csqcprogs || !csqcg.input_frame)
		return;

	CSQC_ChangeLocalPlayer(seat);

	if (csqcg.simtime)
		*csqcg.simtime = cl.servertime;
	if (csqcg.cltime)
		*csqcg.cltime = realtime;

	if (csqcg.clientcommandframe)
		*csqcg.clientcommandframe = cl.movesequence;

	cs_set_input_state(cmd);
	PR_ExecuteProgram (csqcprogs, csqcg.input_frame);
	cs_get_input_state(cmd);
}

//this protocol allows up to 32767 edicts.
#ifdef PEXT_CSQC
static void CSQC_EntityCheck(unsigned int entnum)
{
	unsigned int newmax;

	if (entnum >= maxcsqcentities)
	{
		newmax = entnum+64;
		csqcent = BZ_Realloc(csqcent, sizeof(*csqcent)*newmax);
		memset(csqcent + maxcsqcentities, 0, (newmax - maxcsqcentities)*sizeof(csqcent));
		maxcsqcentities = newmax;
	}
}

int CSQC_StartSound(int entnum, int channel, char *soundname, vec3_t pos, float vol, float attenuation, float pitchmod, float timeofs, unsigned int flags)
{
	void *pr_globals;
	csqcedict_t *ent;

	if (!csqcprogs)
		return false;
	if (csqcg.event_sound)
	{
		pr_globals = PR_globals(csqcprogs, PR_CURRENT);

		CSQC_EntityCheck(entnum);
		ent = csqcent[entnum];
		if (ent)
			*csqcg.self = EDICT_TO_PROG(csqcprogs, (void*)ent);
		else
			*csqcg.self = 0;

		G_FLOAT(OFS_PARM0) = entnum;
		G_FLOAT(OFS_PARM1) = channel;
		G_INT(OFS_PARM2) = PR_TempString(csqcprogs, soundname);
		G_FLOAT(OFS_PARM3) = vol;
		G_FLOAT(OFS_PARM4) = attenuation;
		VectorCopy(pos, G_VECTOR(OFS_PARM5));
		G_FLOAT(OFS_PARM6) = pitchmod*100;
		G_FLOAT(OFS_PARM7) = flags;
//		G_FLOAT(OFS_PARM8) = timeofs;

		PR_ExecuteProgram(csqcprogs, csqcg.event_sound);

		return G_FLOAT(OFS_RETURN);
	}
	else if (csqcg.serversound)
	{
		CSQC_EntityCheck(entnum);
		ent = csqcent[entnum];
		if (!ent)
			return false;

		pr_globals = PR_globals(csqcprogs, PR_CURRENT);

		*csqcg.self = EDICT_TO_PROG(csqcprogs, (void*)ent);
		G_FLOAT(OFS_PARM0) = channel;
		G_INT(OFS_PARM1) = PR_TempString(csqcprogs, soundname);
		VectorCopy(pos, G_VECTOR(OFS_PARM2));
		G_FLOAT(OFS_PARM3) = vol;
		G_FLOAT(OFS_PARM4) = attenuation;
		G_FLOAT(OFS_PARM5) = flags;
		G_FLOAT(OFS_PARM6) = timeofs;

		PR_ExecuteProgram(csqcprogs, csqcg.serversound);

		return G_FLOAT(OFS_RETURN);
	}
	return false;
}

void CSQC_GetEntityOrigin(unsigned int csqcent, float *out)
{
	wedict_t *ent;
	if (!csqcprogs)
		return;
	ent = WEDICT_NUM_UB(csqcprogs, csqcent);
	VectorCopy(ent->v->origin, out);
}

void CSQC_ParseEntities(void)
{
	csqcedict_t *ent;
	unsigned int entnum;
	void *pr_globals;
	int packetsize;
	int packetstart;
	qboolean removeflag;

	if (!csqcprogs)
		Host_EndGame("CSQC needs to be initialized for this server.\n");

	if (!csqcg.ent_update || !csqcg.self)
		Host_EndGame("CSQC has no CSQC_Ent_Update function\n");
	if (!csqc_world.worldmodel || csqc_world.worldmodel->loadstate != MLS_LOADED)
		Host_EndGame("world is not yet initialised\n");

	pr_globals = PR_globals(csqcprogs, PR_CURRENT);

	CL_CalcClientTime();
	if (csqcg.simtime)		//estimated server time
		*csqcg.simtime = cl.servertime;
	if (csqcg.cltime)	//smooth client time.
		*csqcg.cltime = realtime;

	if (csqcg.netnewtime)
		*csqcg.netnewtime = cl.gametime;
	if (csqcg.netoldtime)
		*csqcg.netoldtime = cl.oldgametime;
	if (csqcg.netdeltatime)
		*csqcg.netdeltatime = cl.gametime - cl.oldgametime;

	if (!csqc_isdarkplaces)
	{
		if (csqcg.clientcommandframe)
			*csqcg.clientcommandframe = cl.movesequence;
		if (csqcg.servercommandframe)
			*csqcg.servercommandframe = cl.ackedmovesequence;
	}

	for(;;)
	{
		//replacement deltas now also includes 22bit entity num indicies.
		if (cls.fteprotocolextensions2 & PEXT2_REPLACEMENTDELTAS)
		{
			entnum = (unsigned short)MSG_ReadShort();
			removeflag = !!(entnum & 0x8000);
			if (entnum & 0x4000)
				entnum = (entnum & 0x3fff) | (MSG_ReadByte()<<14);
			else
				entnum &= ~0x8000;
		}
		else
		{
			entnum = (unsigned short)MSG_ReadShort();
			removeflag = !!(entnum & 0x8000);
			entnum &= ~0x8000;
		}

		if ((!entnum && !removeflag) || msg_badread)
			break;

		if (removeflag)
		{	//remove
			if (!entnum)
				Host_EndGame("CSQC cannot remove world!\n");

			CSQC_EntityCheck(entnum);

			if (cl_csqcdebug.ival)
				Con_Printf("Remove %i\n", entnum);

			ent = csqcent[entnum];
			csqcent[entnum] = NULL;

			if (!ent)	//hrm.
				continue;

			CSQC_EntRemove(ent);
			//the csqc is expected to call the remove builtin.
		}
		else
		{
			CSQC_EntityCheck(entnum);

			if (cl.csqcdebug)
			{
				packetsize = MSG_ReadShort();
				packetstart = msg_readcount;
			}
			else
			{
				packetsize = 0;
				packetstart = 0;
			}

			ent = csqcent[entnum];
			if (!ent)
			{
				if (csqcg.ent_spawn)
				{
					*csqcg.self = 0;
					G_FLOAT(OFS_PARM0) = entnum;
					PR_ExecuteProgram(csqcprogs, csqcg.ent_spawn);
					ent = csqcent[entnum] = (csqcedict_t*)PROG_TO_WEDICT(csqcprogs, *csqcg.self);	//allow the mod to change the ent.
				}
				else
				{
					ent = (csqcedict_t*)ED_Alloc(csqcprogs, false, 0);
					csqcent[entnum] = ent;
					ent->xv->entnum = entnum;
				}

				G_FLOAT(OFS_PARM0) = true;

				if (cl_csqcdebug.ival)
					Con_Printf("Add %i\n", entnum);
			}
			else
			{
				G_FLOAT(OFS_PARM0) = false;
				if (cl_csqcdebug.ival)
					Con_Printf("Update %i\n", entnum);
			}

			*csqcg.self = EDICT_TO_PROG(csqcprogs, (void*)ent);
			csqc_mayread = true;
			PR_ExecuteProgram(csqcprogs, csqcg.ent_update);
			csqc_mayread = false;
			if (csqcg.ent_spawn)
				csqcent[entnum] = (csqcedict_t*)PROG_TO_WEDICT(csqcprogs, *csqcg.self);	//allow the mod to change the ent.

			if (cl.csqcdebug)
			{
				if (msg_readcount != packetstart+packetsize)
				{
					if (msg_readcount > packetstart+packetsize)
						Con_Printf("CSQC overread entity %i. Size %i, read %i", entnum, packetsize, msg_readcount - packetstart);
					else
						Con_Printf("CSQC underread entity %i. Size %i, read %i", entnum, packetsize, msg_readcount - packetstart);
					Con_Printf(", first byte is %i(%x)\n", net_message.data[msg_readcount], net_message.data[msg_readcount]);
#ifndef CLIENTONLY
					if (sv.state)
					{
						Con_Printf("Server classname: \"%s\"\n", PR_GetString(svprogfuncs, EDICT_NUM_UB(svprogfuncs, entnum)->v->classname));
					}
#endif
				}
				msg_readcount = packetstart+packetsize;	//leetism.
			}
		}
	}
}
#endif

#endif
