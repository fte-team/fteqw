//#define HALFLIFE_API_VERSION 138
#define HALFLIFE_API_VERSION 140

typedef long hllong; //long is 64bit on amd64+linux, not sure that's what valve meant, but lets keep it for compatibility.
typedef struct hledict_s hledict_t;
typedef unsigned long hlintptr_t;	//this may be problematic. CRestore::ReadField needs a fix. Or 20.
typedef unsigned long hlcrc_t;	//supposed to be 32bit... *sigh*

typedef struct
{
	int allsolid;
	int startsolid;
	int inopen;
	int inwater;
	float fraction;
	vec3_t endpos;
	float planedist;
	vec3_t planenormal;
	hledict_t *touched;
	int hitgroup;
} hltraceresult_t;

#if HALFLIFE_API_VERSION <= 138
typedef struct
{
	int etype;
	int number;
	int flags;
	vec3_t origin;
	vec3_t angles;
	int modelindex;
	int sequence1;
	float frame;
	int colourmap;
	short skin;
	short solid;
	int effects;
	float scale;
	int rendermode;
	int renderamt;
	int rendercolour;
	int renderfx;
	int movetype;
	float frametime;
	float framerate;
	int body;
	qbyte controller[4];
	qbyte blending[2];
	short padding;
	vec3_t velocity;
	vec3_t mins;
	vec3_t maxs;
	int aiment;
} hlbaseline_t;
#endif

typedef struct
{
	string_t	classname;
	string_t	globalname;

	vec3_t	origin;
	vec3_t	oldorigin;
	vec3_t	velocity;
vec3_t	basevelocity;
vec3_t	clbasevelocity;

	vec3_t	movedir;

	vec3_t	angles;
	vec3_t	avelocity;
	vec3_t	punchangles;
	vec3_t	v_angle;

#if HALFLIFE_API_VERSION > 138
vec3_t	endpos;
vec3_t	startpos;
float	impacttime;
float	starttime;
#endif

	int		fixangle;

	float	ideal_pitch;
	float	pitch_speed;
	float	ideal_yaw;
	float	yaw_speed;


	int		modelindex;
	string_t	model;
int		vmodelindex;
int		vwmodelindex;

	vec3_t	absmin;
	vec3_t	absmax;
	vec3_t	mins;
	vec3_t	maxs;
	vec3_t	size;

	float	ltime;
	float	nextthink;
	int		movetype;
	int		solid;

	int	skin;
int		body;
	int		effects;

float	gravity;
float	friction;

int		light_level;

int		sequence1;
int		sequence2;
	float	frame;
float	framestarttime;
float	framerate;
qbyte	controller[4];
qbyte	blending[2];
float	scale;

int		rendermode;
float	renderamt;
vec3_t	rendercolour;
int		renderfx;

	float	health;
	float	frags;
int weapons;
	float	takedamage;
	int		deadflag;
	vec3_t	view_ofs;

int buttons;
	int impulse;

	hledict_t *chain;
	hledict_t *dmg_inflictor;
	hledict_t *enemy;
	hledict_t *aiment;
	hledict_t *owner;
	hledict_t *groundentity;


	int spawnflags;
	int flags;

	int	colormap;
	int	team;

	float	max_health;
	float	teleport_time;
	float	armortype;
	float	armorvalue;
	int	waterlevel;
	int	watertype;

	string_t	target;
	string_t	targetname;
	string_t	netname;
	string_t	message;	//WARNING: hexen2 uses a float and not a string

	float	dmg_take;
	float	dmg_save;
	float	dmg;
	float	dmg_time;

	string_t	noise;
	string_t	noise1;
	string_t	noise2;
	string_t	noise3;

float speed;
float air_finished;
float pain_finished;
float radsuit_finished;

hledict_t *edict;
#if HALFLIFE_API_VERSION > 138
int		playerclass;
float	maxspeed;
float	fov;
int		weaponanim;
int		msec;
int		ducking;
int		timestepsound;
int		swimtime;
int		ducktime;
int		stepleft;
float	fallvelocity;
int		gamestate;
int		oldbuttons;
int		groupinfo;

int		customi0;
int		customi1;
int		customi2;
int		customi3;
float		customf0;
float		customf1;
float		customf2;
float		customf3;
vec3_t		customv0;
vec3_t		customv1;
vec3_t		customv2;
vec3_t		customv3;
hledict_t *custome0;
hledict_t *custome1;
hledict_t *custome2;
hledict_t *custome3;
#endif
} hlentvars_t;

struct hledict_s
{
	qboolean	isfree;
	int			spawnnumber;
	link_t		area;				// linked to a division node or leaf

#if HALFLIFE_API_VERSION > 138
	int			headnode;
	int			num_leafs;
#define HLMAX_ENT_LEAFS 48
	short		leafnums[HLMAX_ENT_LEAFS];
#else
	int			num_leafs;
#define HLMAX_ENT_LEAFS 24
	short		leafnums[HLMAX_ENT_LEAFS];

	hlbaseline_t	baseline;
#endif

	float		freetime; // sv.time when the object was freed

	void		*moddata;
	hlentvars_t	v;
};

#define unk void
/*
#define	FCVAR_ARCHIVE		1	// set to cause it to be saved to vars.rc
#define	FCVAR_USERINFO		2	// changes the client's info string
#define	FCVAR_SERVERINFO	4	// notifies players when changed
#define FCVAR_SERVERDLL		8	// defined by external DLL
#define FCVAR_CLIENTDLL     16  // defined by the client dll
#define HLCVAR_PROTECTED     32  // It's a server cvar, but we don't send the data since it's a password, etc.  Sends 1 if it's not bland/zero, 0 otherwise as value
#define HLCVAR_SPONLY        64  // This cvar cannot be changed by clients connected to a multiplayer server.
*/
typedef struct hlcvar_s
{
	char	*name;
	char	*string;
	int		flags;
	float	value;
	struct hlcvar_s *next;
} hlcvar_t;

typedef struct
{
	char *classname;
	char *key;
	char *value;
	hllong handled;
} hlfielddef_t;



typedef struct
{
//	int	self;
//	int	other;
//	int	world;
	float	time;
	float	frametime;	
	float	force_retouch;
	string_t	mapname;
string_t startspot;
	float	deathmatch;
	float	coop;
	float	teamplay;
	float	serverflags;
//	float	total_secrets;
//	float	total_monsters;
	float	found_secrets;
//	float	killed_monsters;
//	float parms1, parm2, parm3, parm4, parm;
	vec3_t	v_forward;
	vec3_t	v_up;
	vec3_t	v_right;
	float	trace_allsolid;
	float	trace_startsolid;
	float	trace_fraction;
	vec3_t	trace_endpos;
	vec3_t	trace_plane_normal;
	float	trace_plane_dist;
	int	trace_ent;
	float	trace_inopen;
	float	trace_inwater;
int trace_hitgroup;
int trace_flags;
	int	msg_entity;
int audiotrack;
int maxclients;
int maxentities;

char *stringbase;
void *savedata;
vec3_t landmark;
//43...
} SVHL_Globals_t;




//http://metamod.org/dllapi_notes.html
typedef struct
{
	void (*GameDLLInit)(void);
    int (*DispatchSpawn)(hledict_t *ed);
    void (*DispatchThink)(hledict_t *ed);
    unk (*DispatchUse)(unk);
    void (*DispatchTouch)(hledict_t *e1, hledict_t *e2);
    void (*DispatchBlocked)(hledict_t *self, hledict_t *other);
    void (*DispatchKeyValue)(hledict_t *ed, hlfielddef_t *fdef);
    unk (*DispatchSave)(unk);
    unk (*DispatchRestore)(unk);
    unk (*DispatchObjectCollsionBox)(unk);
    unk (*SaveWriteFields)(unk);
    unk (*SaveReadFields)(unk);
    unk (*SaveGlobalState)(unk);
    unk (*RestoreGlobalState)(unk);
    unk (*ResetGlobalState)(unk);
    qboolean (*ClientConnect)(hledict_t *ed, char *name, char *ip, char reject[128]);
    void (*ClientDisconnect)(hledict_t *ed);
    void (*ClientKill)(hledict_t *ed);
    void (*ClientPutInServer)(hledict_t *ed);
    void (*ClientCommand)(hledict_t *ed);
    unk (*ClientUserInfoChanged)(unk);
    void (*ServerActivate)(hledict_t *edictlist, int numedicts, int numplayers);
#if HALFLIFE_API_VERSION > 138
    unk (*ServerDeactivate)(unk);
#endif
    void (*PlayerPreThink)(hledict_t *ed);
    void (*PlayerPostThink)(hledict_t *ed);
    unk (*StartFrame)(unk);
    unk (*ParmsNewLevel)(unk);
    unk (*ParmsChangeLevel)(unk);
    unk (*GetGameDescription)(unk);
    unk (*PlayerCustomization)(unk);
    unk (*SpectatorConnect)(unk);
    unk (*SpectatorDisconnect)(unk);
    unk (*SpectatorThink)(unk);
	//138
#if HALFLIFE_API_VERSION > 138
    unk (*Sys_Error)(unk);
    unk (*PM_Move)(unk);
    unk (*PM_Init)(unk);
    unk (*PM_FindTextureType)(unk);
    unk (*SetupVisibility)(unk);
    unk (*UpdateClientData)(unk);
    unk (*AddToFullPack)(unk);
    unk (*CreateBaseline)(unk);
    unk (*RegisterEncoders)(unk);
    unk (*GetWeaponData)(unk);
    unk (*CmdStart)(unk);
    unk (*CmdEnd)(unk);
    unk (*ConnectionlessPacket)(unk);
    unk (*GetHullBounds)(unk);
    unk (*CreateInstancedBaselines)(unk);
    unk (*InconsistentFile)(unk);
    unk (*AllowLagCompensation)(unk);
#endif
} SVHL_GameFuncs_t;

//http://metamod.org/newapi_notes.html
struct 
{
	unk (*OnFreeEntPrivateData)(unk);
    unk (*GameShutdown)(unk);
    unk (*ShouldCollide)(unk);
    unk (*CvarValue)(unk);
    unk (*CvarValue2 )(unk);
} *SVHL_GameFuncsEx;

// http://metamod.org/engine_notes.html
typedef struct
{
	int (*PrecacheModel)(char *name);
	int (*PrecacheSound)(char *name);
	void (*SetModel)(hledict_t *ed, char *modelname);
	unk (*ModelIndex)(unk);
	int (*ModelFrames)(int midx);
	void (*SetSize)(hledict_t *ed, float *mins, float *maxs);
	void (*ChangeLevel)(char *nextmap, char *startspot);
	unk (*GetSpawnParms)(unk);
	unk (*SaveSpawnParms)(unk);
	float (*VecToYaw)(float *inv);
	void (*VecToAngles)(float *inv, float *outa);
	unk (*MoveToOrigin)(unk);
	unk (*ChangeYaw)(unk);
	unk (*ChangePitch)(unk);
	hledict_t *(*FindEntityByString)(hledict_t *last, char *field, char *value);
	unk (*GetEntityIllum)(unk);
	hledict_t *(*FindEntityInSphere)(hledict_t *last, float *org, float radius);
	hledict_t *(*FindClientInPVS)(hledict_t *ed);
	unk (*EntitiesInPVS)(unk);
	void (*MakeVectors)(float *angles);
	void (*AngleVectors)(float *angles, float *forward, float *right, float *up);
	hledict_t *(*CreateEntity)(void);
	void (*RemoveEntity)(hledict_t *ed);
	hledict_t *(*CreateNamedEntity)(string_t name);
	unk (*MakeStatic)(unk);
	unk (*EntIsOnFloor)(unk);
	int (*DropToFloor)(hledict_t *ed);
	int (*WalkMove)(hledict_t *ed, float yaw, float dist, int mode);
	void (*SetOrigin)(hledict_t *ed, float *neworg);
	void (*EmitSound)(hledict_t *ed, int chan, char *soundname, float vol, float atten, int flags, int pitch);
	void (*EmitAmbientSound)(hledict_t *ed, float *org, char *samp, float vol, float attenuation, unsigned int flags, int pitch);
	void (*TraceLine)(float *start, float *end, int flags, hledict_t *ignore, hltraceresult_t *result);
	unk (*TraceToss)(unk);
	unk (*TraceMonsterHull)(unk);
	void (*TraceHull)(float *start, float *end, int flags, int hullnum, hledict_t *ignore, hltraceresult_t *result);
	unk (*TraceModel)(unk);
	char *(*TraceTexture)(hledict_t *againstent, vec3_t start, vec3_t end);
	unk (*TraceSphere)(unk);
	unk (*GetAimVector)(unk);
	void (*ServerCommand)(char *cmd);
	void (*ServerExecute)(void);
	unk (*ClientCommand)(unk);
	unk (*ParticleEffect)(unk);
	void (*LightStyle)(int stylenum, char *stylestr);
	int (*DecalIndex)(char *decalname);
	int (*PointContents)(float *org);
	void (*MessageBegin)(int dest, int svc, float *org, hledict_t *ent);
	void (*MessageEnd)(void);
	void (*WriteByte)(int value);
	void (*WriteChar)(int value);
	void (*WriteShort)(int value);
	void (*WriteLong)(int value);
	void (*WriteAngle)(float value);
	void (*WriteCoord)(float value);
	void (*WriteString)(char *string);
	void (*WriteEntity)(int entnum);
	void (*CVarRegister)(hlcvar_t *hlvar);
	float (*CVarGetFloat)(char *vname);
	char *(*CVarGetString)(char *vname);
	void (*CVarSetFloat)(char *vname, float v);
	void (*CVarSetString)(char *vname, char *v);
	void (*AlertMessage)(int level, char *fmt, ...);
	void (*EngineFprintf)(FILE *f, char *fmt, ...);
	void *(*PvAllocEntPrivateData)(hledict_t *ed, long quant);
	unk (*PvEntPrivateData)(unk);
	unk (*FreeEntPrivateData)(unk);
	unk (*SzFromIndex)(unk);
	string_t (*AllocString)(char *string);
	unk (*GetVarsOfEnt)(unk);
	hledict_t * (*PEntityOfEntOffset)(int ednum);
	int (*EntOffsetOfPEntity)(hledict_t *ed);
	int (*IndexOfEdict)(hledict_t *ed);
	hledict_t *(*PEntityOfEntIndex)(int idx);
	unk (*FindEntityByVars)(unk);
	void *(*GetModelPtr)(hledict_t *ed);
	int (*RegUserMsg)(char *msgname, int msgsize);
	unk (*AnimationAutomove)(unk);
	unk (*GetBonePosition)(unk);
	hlintptr_t (*FunctionFromName)(char *name);
	char *(*NameForFunction)(hlintptr_t);
	unk (*ClientPrintf)(unk);
	void (*ServerPrint)(char *msg);
	char *(*Cmd_Args)(void);
	char *(*Cmd_Argv)(int argno);
	int (*Cmd_Argc)(void);
	unk (*GetAttachment)(unk);
	void (*CRC32_Init)(hlcrc_t *crc);
	void (*CRC32_ProcessBuffer)(hlcrc_t *crc, qbyte *p, int len);
	void (*CRC32_ProcessByte)(hlcrc_t *crc, qbyte b);
	hlcrc_t (*CRC32_Final)(hlcrc_t crc);
	long (*RandomLong)(long minv, long maxv);
	float (*RandomFloat)(float minv, float maxv);
	unk (*SetView)(unk);
	unk (*Time)(unk);
	unk (*CrosshairAngle)(unk);
	void *(*LoadFileForMe)(char *name, int *size_out);
	void (*FreeFile)(void *fptr);
	unk (*EndSection)(unk);
	int (*CompareFileTime)(char *fname1, char *fname2, int *result);
	void (*GetGameDir)(char *gamedir);
	unk (*Cvar_RegisterVariable)(unk);
	unk (*FadeClientVolume)(unk);
	unk (*SetClientMaxspeed)(unk);
	unk (*CreateFakeClient)(unk);
	unk (*RunPlayerMove)(unk);
	unk (*NumberOfEntities)(unk);
	unk (*GetInfoKeyBuffer)(unk);
	unk (*InfoKeyValue)(unk);
	unk (*SetKeyValue)(unk);
	unk (*SetClientKeyValue)(unk);
	unk (*IsMapValid)(unk);
	unk (*StaticDecal)(unk);
	unk (*PrecacheGeneric)(unk);
	int (*GetPlayerUserId)(hledict_t *ed);
	unk (*BuildSoundMsg)(unk);
	unk (*IsDedicatedServer)(unk);
	//138
#if HALFLIFE_API_VERSION > 138
	hlcvar_t *(*CVarGetPointer)(char *varname);
	unk (*GetPlayerWONId)(unk);
	unk (*Info_RemoveKey)(unk);
	unk (*GetPhysicsKeyValue)(unk);
	unk (*SetPhysicsKeyValue)(unk);
	unk (*GetPhysicsInfoString)(unk);
	unsigned short (*PrecacheEvent)(int eventtype, char *eventname);
	void (*PlaybackEvent)(int flags, hledict_t *ent, unsigned short eventidx, float delay, float *origin, float *angles, float f1, float f2, int i1, int i2, int b1, int b2);
	unk (*SetFatPVS)(unk);
	unk (*SetFatPAS)(unk);
	unk (*CheckVisibility)(unk);
	unk (*DeltaSetField)(unk);
	unk (*DeltaUnsetField)(unk);
	unk (*DeltaAddEncoder)(unk);
	unk (*GetCurrentPlayer)(unk);
	unk (*CanSkipPlayer)(unk);
	unk (*DeltaFindField)(unk);
	unk (*DeltaSetFieldByIndex)(unk);
	unk (*DeltaUnsetFieldByIndex)(unk);
	unk (*SetGroupMask)(unk);
	unk (*CreateInstancedBaseline)(unk);
	unk (*Cvar_DirectSet)(unk);
	unk (*ForceUnmodified)(unk);
	unk (*GetPlayerStats)(unk);
	unk (*AddServerCommand)(unk);
	//
	unk (*Voice_GetClientListening)(unk);
	qboolean (*Voice_SetClientListening)(int listener, int sender, int shouldlisten);
	//140
	unk (*GetPlayerAuthId)(unk);
	unk (*SequenceGet)(unk);
	unk (*SequencePickSentence)(unk);
	unk (*GetFileSize)(unk);
	unk (*GetApproxWavePlayLen)(unk);
	unk (*IsCareerMatch)(unk);
	unk (*GetLocalizedStringLength)(unk);
	unk (*RegisterTutorMessageShown)(unk);
	unk (*GetTimesTutorMessageShown)(unk);
	unk (*ProcessTutorMessageDecayBuffer)(unk);
	unk (*ConstructTutorMessageDecayBuffer)(unk);
	unk (*ResetTutorMessageDecayData)(unk);
	unk (*QueryClientCvarValue)(unk);
	unk (*QueryClientCvarValue2)(unk);
#endif

	int check;	//added so I can be sure parameters match this list, etc
} SVHL_Builtins_t;

extern SVHL_Globals_t SVHL_Globals;
extern SVHL_GameFuncs_t SVHL_GameFuncs;

extern hledict_t *SVHL_Edict;
extern int SVHL_NumActiveEnts;





void SVHL_LinkEdict (hledict_t *ent, qboolean touch_triggers);
void SVHL_UnlinkEdict (hledict_t *ent);
hledict_t	*SVHL_TestEntityPosition (hledict_t *ent);
void SVHL_TouchLinks ( hledict_t *ent, areanode_t *node );
trace_t SVHL_Move (vec3_t start, vec3_t mins, vec3_t maxs, vec3_t end, int type, int forcehull, hledict_t *passedict);
int SVHL_PointContents (vec3_t p);
int SVHL_AreaEdicts (vec3_t mins, vec3_t maxs, hledict_t **list, int maxcount);
