//these structures are shared with the exe.

#define UIMAX_SCOREBOARDNAME 16
#define UIMAX_INFO_STRING EXTENDED_INFO_STRING

typedef struct {
	int userid;
	char name[UIMAX_SCOREBOARDNAME];	//for faster reading.
	float starttime;
	int frags;
	int ping;
	int pl;

	int topcolour;
	int bottomcolour;

	char userinfo[UIMAX_INFO_STRING];	//should this size be enforced?
									//you can get all sorts of stuff like names.
} vmuiclientinfo_t;

//useful for it's width/height. The others are a little pointless to be honest.
typedef struct {
	unsigned int width;
	unsigned int height;
	unsigned int bpp;
	unsigned int refreshrate;	//quakeworld normally only draws 30 frames per second dontcha know?
	qboolean fullscreen;	//oposite of windowed.
	char renderername[256];		//Human readable

	int vidbugs;	//flags for the buggy implementations of opengl or whatever.
} vidinfo_t;

//is there any point to these?
enum {
	VB_NOSCALE			= 1<<0,	//software rendering, incapable of scaling.
	VB_NOCOLOUR			= 1<<1,	//software rendering that doesn't allow belnding colours. (8 bit paletted)
	VB_NOCOLOURINTERP		= 1<<2,	//software rendering that supports a blending of colours, but not per vertex.

	VB_NOINTERPOLATEALPHA	= 1<<3,	//riva128
	VB_NOMODULATEALPHA	= 1<<4,	//ragepro
	VB_NOSRCTIMESDST		= 1<<5,	//permedia
};

typedef enum {
	SID_Q2STATUSBAR		= -4,
	SID_Q2LAYOUT		= -3,
	SID_CENTERPRINTTEXT	= -2,
	SID_SERVERNAME		= -1,
	//q2's config strings come here.
} stringid_e;

#define UI_API_VERSION 5000
typedef enum {
	UI_GETAPIVERSION	= 0,
	UI_INIT		= 1,
	UI_SHUTDOWN		= 2,
	UI_KEY_EVENT	= 3,
	UI_MOUSE_DELTA	= 4,
	UI_REFRESH		= 5,
	UI_IS_FULLSCREEN	= 6,
	UI_SET_ACTIVE_MENU = 7,
	UI_CONSOLE_COMMAND = 8,
	UI_DRAW_CONNECT_SCREEN = 9,
	UI_HASUNIQUECDKEY = 10,
	//return value expected
	//0 means don't take input
	//1 means engine should skip map/scrback update,
	//2 means fade the screen or draw console back.	(expected to be most used)
	//3 means don't fade the screen

	UI_DRAWSTATUSBAR	= 500,
	UI_MOUSE_POS,
	UI_INTERMISSION,
	UI_FINALE,
	UI_STRINGCHANGED,	//parma is the string id
	UI_NEWSERVER	//indicates that all the strings have changed.
} uiExport_t;

typedef enum {
	UI_ERROR					= 0,
	UI_PRINT					= 1,
	UI_MILLISECONDS				= 2,
	UI_CVAR_SET					= 3,
	UI_CVAR_VARIABLEVALUE		= 4,
	UI_CVAR_VARIABLESTRINGBUFFER	= 5,
	UI_CVAR_SETVALUE			= 6,
	UI_CVAR_RESET				= 7,
	UI_CVAR_CREATE				= 8,
	UI_CVAR_INFOSTRINGBUFFER	= 9,
	UI_ARGC						= 10,
	UI_ARGV						= 11,
	UI_CMD_EXECUTETEXT			= 12,
	UI_FS_FOPENFILE				= 13,
	UI_FS_READ					= 14,
	UI_FS_WRITE					= 15,
	UI_FS_FCLOSEFILE			= 16,
	UI_FS_GETFILELIST			= 17,
	UI_R_REGISTERMODEL			= 18,
	UI_R_REGISTERSKIN			= 19,
	UI_R_REGISTERSHADERNOMIP	= 20,
	UI_R_CLEARSCENE				= 21,
	UI_R_ADDREFENTITYTOSCENE	= 22,
	UI_R_ADDPOLYTOSCENE			= 23,
	UI_R_ADDLIGHTTOSCENE		= 24,
	UI_R_RENDERSCENE			= 25,
	UI_R_SETCOLOR				= 26,
	UI_R_DRAWSTRETCHPIC			= 27,
	UI_UPDATESCREEN				= 28,
	UI_CM_LERPTAG				= 29,
	UI_CM_LOADMODEL				= 30,
	UI_S_REGISTERSOUND			= 31,
	UI_S_STARTLOCALSOUND		= 32,
	UI_KEY_KEYNUMTOSTRINGBUF	= 33,
	UI_KEY_GETBINDINGBUF		= 34,
	UI_KEY_SETBINDING			= 35,
	UI_KEY_ISDOWN				= 36,
	UI_KEY_GETOVERSTRIKEMODE	= 37,
	UI_KEY_SETOVERSTRIKEMODE	= 38,
	UI_KEY_CLEARSTATES			= 39,
	UI_KEY_GETCATCHER			= 40,
	UI_KEY_SETCATCHER			= 41,
	UI_GETCLIPBOARDDATA			= 42,
	UI_GETGLCONFIG				= 43,
	UI_GETCLIENTSTATE			= 44,
	UI_GETCONFIGSTRING			= 45,
	UI_LAN_GETPINGQUEUECOUNT	= 46,
	UI_LAN_CLEARPING			= 47,
	UI_LAN_GETPING				= 48,
	UI_LAN_GETPINGINFO			= 49,
	UI_CVAR_REGISTER			= 50,
	UI_CVAR_UPDATE				= 51,
	UI_MEMORY_REMAINING			= 52,
	UI_GET_CDKEY				= 53,
	UI_SET_CDKEY				= 54,
	UI_R_REGISTERFONT			= 55,
	UI_R_MODELBOUNDS			= 56,
	UI_PC_ADD_GLOBAL_DEFINE		= 57,
	UI_PC_LOAD_SOURCE			= 58,
	UI_PC_FREE_SOURCE			= 59,
	UI_PC_READ_TOKEN			= 60,
	UI_PC_SOURCE_FILE_AND_LINE	= 61,
	UI_S_STOPBACKGROUNDTRACK	= 62,
	UI_S_STARTBACKGROUNDTRACK	= 63,
	UI_REAL_TIME				= 64,
	UI_LAN_GETSERVERCOUNT		= 65,
	UI_LAN_GETSERVERADDRESSSTRING	= 66,
	UI_LAN_GETSERVERINFO		= 67,
	UI_LAN_MARKSERVERVISIBLE	= 68,
	UI_LAN_UPDATEVISIBLEPINGS	= 69,
	UI_LAN_RESETPINGS			= 70,
	UI_LAN_LOADCACHEDSERVERS	= 71,
	UI_LAN_SAVECACHEDSERVERS	= 72,
	UI_LAN_ADDSERVER			= 73,
	UI_LAN_REMOVESERVER			= 74,
	UI_CIN_PLAYCINEMATIC		= 75,
	UI_CIN_STOPCINEMATIC		= 76,
	UI_CIN_RUNCINEMATIC			= 77,
	UI_CIN_DRAWCINEMATIC		= 78,
	UI_CIN_SETEXTENTS			= 79,
	UI_R_REMAP_SHADER			= 80,
	UI_VERIFY_CDKEY				= 81,
	UI_LAN_SERVERSTATUS			= 82,
	UI_LAN_GETSERVERPING		= 83,
	UI_LAN_SERVERISVISIBLE		= 84,
	UI_LAN_COMPARESERVERS		= 85,
	// 1.32
	UI_FS_SEEK					= 86,
	UI_SET_PBCLSTATUS			= 87,

	UI_MEMSET = 100,
	UI_MEMCPY,
	UI_STRNCPY,
	UI_SIN,
	UI_COS,
	UI_ATAN2,
	UI_SQRT,
	UI_FLOOR,
	UI_CEIL,


	UI_CACHE_PIC		= 500,
	UI_PICFROMWAD		= 501,
	UI_GETPLAYERINFO	= 502,
	UI_GETSTAT			= 503,
	UI_GETVIDINFO		= 504,
	UI_GET_STRING		= 510,

} ui_builtinnum_t;
