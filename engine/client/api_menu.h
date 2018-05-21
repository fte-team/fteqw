 /*
 * Copyright (c) 2015-2018
 * Marco Hladik  All rights reserved.
 * 
 * This is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.

 * This is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this. If not, see <http://www.gnu.org/licenses/>.
 */

#define NATIVEMENU_API_VERSION_MIN 0	//will be updated any time a symbol is renamed.
#define NATIVEMENU_API_VERSION_MAX 0	//bumped for any change.
#ifndef NATIVEMENU_API_VERSION			//so you can hold back the reported version in order to work with older engines.
#define NATIVEMENU_API_VERSION NATIVEMENU_API_VERSION_MAX	//version reported to the other side.
#endif

struct vfsfile_s;
struct serverinfo_s;
struct searchpathfuncs_s;
enum slist_test_e;
enum hostcachekey_e;	//obtained via calls to gethostcacheindexforkey
enum fs_relative;
#ifndef __QUAKEDEF_H__
	#ifdef __cplusplus
		typedef enum {qfalse, qtrue} qboolean;//false and true are forcivly defined.
	#else
		typedef enum {false, true}	qboolean;
	#endif
	typedef float vec_t;
	typedef vec_t vec2_t[2];
	typedef vec_t vec3_t[3];
	typedef vec_t vec4_t[4];
	#ifdef _MSC_VER
		#define QDECL __cdecl
	#else
		#define QDECL
	#endif

	#include <stdint.h>
	typedef uint64_t qofs_t;
#endif

struct menu_inputevent_args_s
{
	enum {
		MIE_KEYDOWN		= 0,
		MIE_KEYUP		= 1,
		MIE_MOUSEDELTA	= 2,
		MIE_MOUSEABS	= 3,
	} eventtype;
	unsigned int devid;
	union
	{
		struct
		{
			unsigned int scancode;
			unsigned int charcode;
		} key;
		struct
		{
			float delta[2];
			float screen[2]; //virtual coords
		} mouse;
	};
};

typedef struct {
	int							api_version;	//this may be higher than you expect.

	int (*checkextension)		(const char *ext);
	void (*error)				(const char *err);
	void (*printf)				(const char *text, ...);
	void (*dprintf)				(const char *text, ...);
	void (*localcmd)			(const char *cmd);
	float (*cvar_float)			(const char *name);
	const char *(*cvar_string)	(const char *name);	//return value lasts until cvar_set is called, etc, so don't cache.
	void (*cvar_set)			(const char *name, const char *value);
	void (*registercvar)		(const char *name, const char *defaultvalue, unsigned int flags, const char *description);

	int (*isserver)				(void);
	int (*getclientstate)		(void);
	void (*localsound)			(const char *sample, int channel, float volume);

	// file input / search crap
	struct vfsfile_s *(*fopen)	(const char *filename, char *modestring, enum fs_relative fsroot);	//modestring should be one of rb,r+b,wb,w+b,ab,wbp. Mostly use a root of FS_GAMEONLY for writes, otherwise FS_GAME for reads.
	void (*fclose)				(struct vfsfile_s *fhandle);
	char *(*fgets)				(struct vfsfile_s *fhandle, char *out, size_t outsize);	//returns output buffer, or NULL
	void (*fprintf)				(struct vfsfile_s *fhandle, const char *s, ...);
	void (*EnumerateFiles)		(const char *match, int (QDECL *callback)(const char *fname, qofs_t fsize, time_t mtime, void *ctx, struct searchpathfuncs_s *package), void *ctx);

	// Drawing stuff
//	int (*iscachedpic)			(const char *name);
	void *(*precache_pic)		(const char *name);
	int (*drawgetimagesize)		(void *pic, int *x, int *y);
	void (*drawquad)			(vec2_t position[4], vec2_t texcoords[4], void *pic, vec4_t rgba, unsigned int be_flags);
//	void (*drawsubpic)			(vec2_t pos, vec2_t sz, const char *pic, vec2_t srcpos, vec2_t srcsz, vec4_t rgba, unsigned int be_flags);
//	void (*drawfill)			(vec2_t position, vec2_t size, vec4_t rgba, unsigned int be_flags);
//	float (*drawcharacter)		(vec2_t position, int character, vec2_t scale, vec4_t rgba, unsigned int be_flags);
//	float (*drawrawstring)		(vec3_t position, char *text, vec3_t scale, vec4_t rgba, unsigned int be_flags);
	float (*drawstring)			(vec2_t position, const char *text, float height, vec4_t rgba, unsigned int be_flags);
	float (*stringwidth)		(const char *text, float height);
	void (*drawsetcliparea)		(float x, float y, float width, float height);
	void (*drawresetcliparea)	(void);

	// Menu specific stuff
	qboolean (*setkeydest)			(qboolean focused);	//returns whether it changed.
	int (*getkeydest)				(void);				//returns 0 if unfocused, -1 if active-but-unfocused, 1 if focused-and-active.
	int (*setmousetarget)			(const char *cursorname, float hot_x, float hot_y, float scale);	//forces absolute mouse coords whenever cursorname isn't NULL
	const char *(*keynumtostring)	(int keynum, int modifier);
	int (*stringtokeynum)			(const char *key, int *modifier);
	int (*findkeysforcommand)		(int bindmap, const char *command, int *out_scancodes, int *out_modifiers, int keycount);

	// Server browser stuff
	int (*gethostcachevalue)						(int type);
	char *(*gethostcachestring)						(struct serverinfo_s *host, enum hostcachekey_e fld);
	float (*gethostcachenumber)						(struct serverinfo_s *host, enum hostcachekey_e fld);
	void (*resethostcachemasks)						(void);
	void (*sethostcachemaskstring)					(qboolean or, enum hostcachekey_e fld, char *str, enum slist_test_e op);
	void (*sethostcachemasknumber)					(qboolean or, enum hostcachekey_e fld, int num, enum slist_test_e op);
	void (*sethostcachesort)						(enum hostcachekey_e fld, qboolean descending);
	void (*resorthostcache)							(void);
	struct serverinfo_s *(*getsortedhost)			(int idx);
	void (*refreshhostcache)						(qboolean fullreset);
	enum hostcachekey_e (*gethostcacheindexforkey)	(const char *key);
} menu_import_t;

typedef struct {
	int		api_version;

	void	(*Init)				(void);
	void	(*Shutdown)			(void);
	void	(*Draw)				(int width, int height, float frametime);
	void	(*DrawLoading)		(int width, int height, float frametime);
	void	(*Toggle)			(int wantmode);
	int		(*InputEvent)		(struct menu_inputevent_args_s ev);
	void	(*ConsoleCommand)	(const char *cmd);
} menu_export_t;

#ifndef NATIVEEXPORT
	#ifdef _WIN32
		#define NATIVEEXPORTPROTO QDECL
		#define NATIVEEXPORT __declspec(dllexport) NATIVEEXPORTPROTO
	#else
		#define NATIVEEXPORTPROTO
		#define NATIVEEXPORT __attribute__((visibility("default")))
	#endif
#endif

menu_export_t *NATIVEEXPORTPROTO GetMenuAPI	(menu_import_t *import); 
