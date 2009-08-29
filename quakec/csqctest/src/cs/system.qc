
//NOTE: THIS IS AN INTERFACE FILE. DO NOT EDIT.
//MODIFYING THIS FILE CAN RESULT IN CRC ERRORS.
//YOU HAVE BEEN WARNED.

//It is safe to edit below the end_sys_fields define,
//but please don't report the engine rejecting due to
//crc mismatches when you've modified above that line.

//feel free to look though. :)



#ifndef CSQC
#define CSQC 1
#endif

#ifdef FTEQCC
#pragma noref 1	//remove warnings about unreferenced variables.
#endif

/*
==============================================================================

			SOURCE FOR GLOBALVARS_T C STRUCTURE

==============================================================================
*/

//
// system globals
//
entity		self;
entity		other;
entity		world;
float		time;
#ifndef WORKINDP
float		cltime;
#endif
float		frametime;

float 		player_localentnum;	//the entnum
float 		player_localnum;	//the playernum
float		maxclients;	//a constant filled in by the engine. gah, portability eh?

float		clientcommandframe;	//player movement
float		servercommandframe;	//clientframe echoed off the server

string		mapname;	//current brief map name

#ifndef WORKINDP
float		intermission;	//non-zero if the server sent an svc_intermission.
#endif

//
// global variables set by built in functions
//
vector		v_forward, v_up, v_right;	// set by makevectors()
#ifndef WORKINDP
vector view_angles;
#endif

// set by traceline / tracebox
float		trace_allsolid;
float		trace_startsolid;
float		trace_fraction;
vector		trace_endpos;
vector		trace_plane_normal;
float		trace_plane_dist;
entity		trace_ent;
float		trace_inopen;
float		trace_inwater;

#ifdef WORKINDP
void()	CSQC_Init;
void()	CSQC_Shutdown;
float(float f, float t, float n)	CSQC_InputEvent;
void(float width, float height, float do2d)	CSQC_UpdateView;
float(string s)	CSQC_ConsoleCommand;

//these fields are read and set by the default player physics
vector		pmove_org;
vector		pmove_vel;
vector		pmove_mins;
vector		pmove_maxs;
#endif

//retrieved from the current movement commands (read by player physics)
float		input_timelength;
vector		input_angles;
vector		input_movevalues;	//forwards, right, up.
float		input_buttons;		//attack, use, jump (default physics only uses jump)
#ifndef WORKINDP
float		input_impulse;
#endif

#ifdef WORKINDP
float		movevar_gravity;
float		movevar_stopspeed;
float		movevar_maxspeed;
float		movevar_spectatormaxspeed;	//used by NOCLIP movetypes.
float		movevar_accelerate;
float		movevar_airaccelerate;
float		movevar_wateraccelerate;
float		movevar_friction;
float		movevar_waterfriction;
float		movevar_entgravity;	//the local player's gravity field. Is a multiple (1 is the normal value)
#endif

//================================================
void		end_sys_globals;		// flag for structure dumping
//================================================

/*
==============================================================================

			SOURCE FOR ENTVARS_T C STRUCTURE

==============================================================================
*/

//
// system fields (*** = do not set in prog code, maintained by C code)
//
.float		modelindex;		// *** model index in the precached list
.vector		absmin, absmax;	// *** origin + mins / maxs

.float		entnum;	// *** the ent number as on the server
.float		drawmask;
.void()		predraw;

.float		movetype;
.float		solid;

.vector		origin;			// ***
.vector		oldorigin;		// ***
.vector		velocity;
.vector		angles;
.vector		avelocity;
#ifndef WORKINDP
.float		pmove_flags;
#endif

.string		classname;		// spawn function

#ifndef WORKINDP
.float		renderflags;
#endif
.string		model;
.float		frame;
#ifndef WORKINDP
.float		frame1time;	//for automatic framegroups.
.float		frame2;
.float		frame2time;
.float		lerpfrac;	//the ammount of frame2 to use.
#endif
.float		skin;

.float		effects;

.vector		mins, maxs;		// bounding box extents reletive to origin
.vector		size;			// maxs - mins

.void()		touch;
#ifdef WORKINDP
.void()		use;
#endif
.void()		think;
.void()		blocked;		// for doors or plats, called when can't push other

.float		nextthink;

.entity		chain;

#ifdef WORKINDP
.string		netname;
#endif

.entity 		enemy;

.float		flags;

.float		colormap;

.entity		owner;		// who launched a missile

//================================================
void		end_sys_fields;			// flag for structure dumping
//================================================

#ifdef FTEQCC
#pragma noref 0
#endif


#ifdef WORKINDP
.float frame2;
.float frame1time;
.float frame2time;
.float renderflags;
.float lerpfrac;

vector view_angles;
#endif

//optional extras
.float alpha;
.float scale;
.float forceshader;	//an fte extension, to force a q3 shader upon a model
.float fatness;	//an fte feature, to expand alias models along thier normals.
