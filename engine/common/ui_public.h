//these structures are shared with the exe.

#define UIMAX_SCOREBOARDNAME 16
#define UIMAX_INFO_STRING 196

#ifdef MAX_SCOREBOARDNAME 
#if MAX_SCOREBOARDNAME != UIMAX_SCOREBOARDNAME
#pragma message("MAX_SCOREBOARDNAME doesn't match UIMAX_SCOREBOARDNAME")
#endif
#endif

#ifdef UIMAX_INFO_STRING 
#if UIMAX_INFO_STRING != UIMAX_INFO_STRING 
#pragma message("MAX_SCOREBOARDNAME doesn't match UIMAX_SCOREBOARDNAME")
#endif
#endif

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
	UI_DRAWMENU		= 5,
	UI_FULLSCREEN	= 6,
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
	UI_SYSERROR			= 0,
	UI_PRINT			= 1,
	UI_CVAR_SET			= 3,
	UI_CVAR_GET_VALUE	= 4,
	UI_CVAR_GET_STRING	= 5,
	UI_CVAR_SET_VALUE	= 6,
	UI_CVAR_RESET		= 7,		//not yet implemented

	UI_CBUF_ADD_COMMAND	= 12,
	UI_FS_OPEN			= 13,
	UI_FS_READ			= 14,
	UI_FS_WRITE			= 15,
	UI_FS_CLOSE			= 16,
	UI_FS_LISTFILES		= 17,

	UI_PRECACHE_MODEL	= 18,
	UI_PRECACHE_SKIN	= 19,	//fte doesn't implement.
	UI_DRAW_CACHEPIC	= 20,

	UI_SCENE_CLEAR		= 21,
	UI_SCENE_ADD_ENTITY	= 22,
	UI_SCENE_ADD_LIGHT	= 24,	//non-functional
	UI_SCENE_RENDER		= 25,

	UI_DRAW_COLOUR		= 26,
	UI_DRAW_IMAGE		= 27,
	UI_LERP_TAG			= 29,	//currently non-functional.

	UI_SOUND_PRECACHE	= 31,
	UI_SOUND_PLAY		= 32,

	UI_KEY_NAME			= 33,			//retrieves text for the name of a key - eg leftarrow
	UI_KEY_GETBINDING	= 34,
	UI_KEY_SETBINDING	= 35,
	UI_KEY_ISDOWN		= 36,

	UI_KEY_CLEARALL		= 39,
	UI_KEY_GETDEST		= 40,
	UI_KEY_SETDEST		= 41,
	UI_GET_VID_CONFIG	= 43,			//very minimal
	UI_GET_CLIENT_STATE	= 44,	//not going to do anything btw.
	UI_GET_SERVERINFO	= 45,

//server browser stuph.
	UI_MS_GETSERVERCOUNT	= 46,
	UI_CLEAR_PINGS		= 47,
	UI_GET_PINGADDRESS	= 48,
	UI_GET_PINGINFO		= 49,

	UI_CVAR_REGISTER	= 50,
	UI_CVAR_UPDATE		= 51,
	UI_MEM_AVAILABLE	= 52,

	UI_CDKEY_GET		= 53,
	UI_CDKEY_SET		= 54,

	UI_REGISTERFRONT	= 55,

	UI_GET_REALTIME		= 62,
	UI_LAN_GET_COUNT	= 65,
	UI_LAN_GET_ADDRESS	= 66,

	UI_SOMETHINGTHATRETURNSTRUE	= 81,
	UI_SOMETHINGTODOWITHPUNKBUSTER	= 87,



	UI_MEMSET			= 100,
	UI_MEMMOVE			= 101,
	UI_STRNCPY			= 102,
	UI_SIN				= 103,
	UI_COS				= 104,
	UI_ATAN2			= 105,
	UI_SQRT				= 106,
	UI_FLOOR			= 107,
	UI_CEIL				= 108,



	UI_CACHE_PIC		= 500,
	UI_PICFROMWAD		= 501,
	UI_GETPLAYERINFO	= 502,
	UI_GETSTAT			= 503,
	UI_GETVIDINFO		= 504,
	UI_GET_STRING		= 510,

} ui_builtinnum_t;
