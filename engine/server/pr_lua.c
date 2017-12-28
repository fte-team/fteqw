#include "quakedef.h"

#ifdef VM_LUA

#include "pr_common.h"
#include "hash.h"

#define luagloballist	\
	globalentity	(true, self)	\
	globalentity	(true, other)	\
	globalentity	(true, world)	\
	globalfloat		(true, time)	\
	globalfloat		(true, frametime)	\
	globalentity	(false, newmis)	\
	globalfloat		(false, force_retouch)	\
	globalstring	(true, mapname)	\
	globalfloat		(false, deathmatch)	\
	globalfloat		(false, coop)	\
	globalfloat		(false, teamplay)	\
	globalfloat		(true, serverflags)	\
	globalfloat		(false, dimension_send)	\
	globalfloat		(false, physics_mode)	\
	globalfloat		(true, total_secrets)	\
	globalfloat		(true, total_monsters)	\
	globalfloat		(true, found_secrets)	\
	globalfloat		(true, killed_monsters)	\
	globalvec		(true, v_forward)	\
	globalvec		(true, v_up)	\
	globalvec		(true, v_right)	\
	globalfloat		(true, trace_allsolid)	\
	globalfloat		(true, trace_startsolid)	\
	globalfloat		(true, trace_fraction)	\
	globalvec		(true, trace_endpos)	\
	globalvec		(true, trace_plane_normal)	\
	globalfloat		(true, trace_plane_dist)	\
	globalentity	(true, trace_ent)	\
	globalfloat		(true, trace_inopen)	\
	globalfloat		(true, trace_inwater)	\
	globalfloat		(false, trace_endcontentsf)	\
	globalint		(false, trace_endcontentsi)	\
	globalfloat		(false, trace_surfaceflagsf)	\
	globalint		(false, trace_surfaceflagsi)	\
	globalfloat		(false, cycle_wrapped)	\
	globalentity	(false, msg_entity)	\
	globalfunc		(false, main)	\
	globalfunc		(true, StartFrame)	\
	globalfunc		(true, PlayerPreThink)	\
	globalfunc		(true, PlayerPostThink)	\
	globalfunc		(true, ClientKill)	\
	globalfunc		(true, ClientConnect)	\
	globalfunc		(true, PutClientInServer)	\
	globalfunc		(true, ClientDisconnect)	\
	globalfunc		(false, SetNewParms)	\
	globalfunc		(false, SetChangeParms)

//any globals or functions that the server might want access to need to be known also.
#define luaextragloballist	\
	globalstring	(true, startspot)	\
	globalstring	(true, ClientReEnter) \
	globalfloat		(false, dimension_default)

typedef struct
{
#define globalentity(required, name) int name;
#define globalint(required, name) int name;
#define globalfloat(required, name) float name;
#define globalstring(required, name) string_t name;
#define globalvec(required, name) vec3_t name;
#define globalfunc(required, name) int name;
luagloballist
luaextragloballist
#undef globalentity
#undef globalint
#undef globalfloat
#undef globalstring
#undef globalvec
#undef globalfunc
} luaglobalvars_t;

typedef struct
{
	int type;
	ptrdiff_t offset;
	char *name;
	bucket_t buck;
} luafld_t;

typedef struct lua_State lua_State;
typedef void *(QDECL *lua_Alloc) (void *ud, void *ptr, size_t osize, size_t nsize);
typedef const char *(QDECL *lua_Reader)(lua_State *L, void *data, size_t *size);
typedef int (QDECL *lua_CFunction) (lua_State *L);
typedef double lua_Number;
typedef int lua_Integer;

//I'm using this struct for all the global stuff.
static struct
{
	lua_State *ctx;
	char readbuf[1024];
	pubprogfuncs_t progfuncs;
	progexterns_t progfuncsparms;
	edict_t **edicttable;
	unsigned int maxedicts;
	luaglobalvars_t globals;	//internal global structure
	hashtable_t globalfields;	//name->luafld_t
	luafld_t globflds[1024];	//fld->offset+type
	hashtable_t entityfields;	//name->luafld_t
	luafld_t entflds[1024];		//fld->offset+type

	qboolean triedlib;
	dllhandle_t *lib;

	lua_State *		(QDECL *newstate)		(lua_Alloc f, void *ud);
	lua_CFunction	(QDECL *atpanic)		(lua_State *L, lua_CFunction panicf);
	void			(QDECL *close)			(lua_State *L);
	int				(QDECL *load)			(lua_State *L, lua_Reader reader, void *dt, const char *chunkname, const char *mode);
	int				(QDECL *pcallk)			(lua_State *L, int nargs, int nresults, int errfunc, int ctx, lua_CFunction k);
	void			(QDECL *callk)			(lua_State *L, int nargs, int nresults,				 int ctx, lua_CFunction k);
	void			(QDECL *getfield)		(lua_State *L, int idx, const char *k);
	void			(QDECL *setfield)		(lua_State *L, int idx, const char *k);
	void			(QDECL *gettable)		(lua_State *L, int idx);
	void			(QDECL *settable)		(lua_State *L, int idx);
	void			(QDECL *getglobal)		(lua_State *L, const char *var);
	void			(QDECL *setglobal)		(lua_State *L, const char *var);
	int				(QDECL *error)			(lua_State *L);
	int				(QDECL *type)			(lua_State *L, int idx);
	const char     *(QDECL *typename)		(lua_State *L, int tp);
	void			(QDECL *rawget)			(lua_State *L, int idx);
	void			(QDECL *rawset)			(lua_State *L, int idx);
	void			(QDECL *createtable)	(lua_State *L, int narr, int nrec);
	int				(QDECL *setmetatable)	(lua_State *L, int objindex);
	void		   *(QDECL *newuserdata)	(lua_State *L, size_t usize);

	void			(QDECL *replace)		(lua_State *L, int idx);
	int				(QDECL *gettop)			(lua_State *L);
	int				(QDECL *settop)			(lua_State *L, int idx);
	void			(QDECL *pushboolean)	(lua_State *L, int b);
	void			(QDECL *pushnil)		(lua_State *L);
	void			(QDECL *pushnumber)		(lua_State *L, lua_Number n);
	void			(QDECL *pushinteger)	(lua_State *L, lua_Integer n);
	void			(QDECL *pushvalue)		(lua_State *L, int idx);
	void			(QDECL *pushcclosure)	(lua_State *L, lua_CFunction fn, int n);
	const char *	(QDECL *pushfstring)	(lua_State *L, const char *fmt, ...);
	void			(QDECL *pushlightuserdata)	(lua_State *L, void *p);
	const char *	(QDECL *tolstring)		(lua_State *L, int idx, size_t *len);
	int             (QDECL *toboolean)		(lua_State *L, int idx);
	lua_Number      (QDECL *tonumberx)		(lua_State *L, int idx, int *isnum);
	lua_Integer     (QDECL *tointegerx)		(lua_State *L, int idx, int *isnum);
	const void     *(QDECL *topointer)		(lua_State *L, int idx);
	void		   *(QDECL *touserdata)		(lua_State *L, int idx);

	int				(QDECL *Lcallmeta)		(lua_State *L, int obj, const char *e);
	int				(QDECL *Lnewmetatable)	(lua_State *L, const char *tname);
} lua;
#define pcall(L,n,r,f)	pcallk(L, (n), (r), (f), 0, NULL)
#define call(L,n,r)		callk(L, (n), (r), 0, NULL)
#define pop(L,n)		settop(L, -(n)-1)
#define pushstring(L,s)	pushfstring(L,"%s",s)

#define LUA_TNONE			(-1)
#define LUA_TNIL			0
#define LUA_TBOOLEAN		1
#define LUA_TLIGHTUSERDATA	2
#define LUA_TNUMBER			3
#define LUA_TSTRING			4
#define LUA_TTABLE			5
#define LUA_TFUNCTION		6
#define LUA_TUSERDATA		7
#define LUA_TTHREAD			8
#define LUA_NUMTAGS			9

#define LUA_REGISTRYINDEX (-1000000 - 1000)

static qboolean init_lua(void)
{
	if (!lua.triedlib)
	{
		dllfunction_t luafuncs[] =
		{
			{(void*)&lua.newstate,		"lua_newstate"},
			{(void*)&lua.atpanic,		"lua_atpanic"},
			{(void*)&lua.close,			"lua_close"},
			{(void*)&lua.load,			"lua_load"},
			{(void*)&lua.pcallk,		"lua_pcallk"},
			{(void*)&lua.callk,			"lua_callk"},
			{(void*)&lua.getfield,		"lua_getfield"},
			{(void*)&lua.setfield,		"lua_setfield"},
			{(void*)&lua.gettable,		"lua_gettable"},
			{(void*)&lua.settable,		"lua_settable"},
			{(void*)&lua.getglobal,		"lua_getglobal"},
			{(void*)&lua.setglobal,		"lua_setglobal"},
			{(void*)&lua.error,			"lua_error"},
			{(void*)&lua.type,			"lua_type"},
			{(void*)&lua.typename,		"lua_typename"},
			{(void*)&lua.rawget,		"lua_rawget"},
			{(void*)&lua.rawset,		"lua_rawset"},
			{(void*)&lua.createtable,	"lua_createtable"},
			{(void*)&lua.setmetatable,	"lua_setmetatable"},
			{(void*)&lua.newuserdata,	"lua_newuserdata"},

			{(void*)&lua.replace,		"lua_replace"},
			{(void*)&lua.gettop,		"lua_gettop"},
			{(void*)&lua.settop,		"lua_settop"},
			{(void*)&lua.pushboolean,	"lua_pushboolean"},
			{(void*)&lua.pushnil,		"lua_pushnil"},
			{(void*)&lua.pushnumber,	"lua_pushnumber"},
			{(void*)&lua.pushinteger,	"lua_pushinteger"},
			{(void*)&lua.pushvalue,		"lua_pushvalue"},
			{(void*)&lua.pushcclosure,	"lua_pushcclosure"},
			{(void*)&lua.pushfstring,	"lua_pushfstring"},
			{(void*)&lua.pushlightuserdata,	"lua_pushlightuserdata"},
			{(void*)&lua.tolstring,		"lua_tolstring"},
			{(void*)&lua.toboolean,		"lua_toboolean"},
			{(void*)&lua.tonumberx,		"lua_tonumberx"},
			{(void*)&lua.tointegerx,	"lua_tointegerx"},
			{(void*)&lua.topointer,		"lua_topointer"},
			{(void*)&lua.touserdata,	"lua_touserdata"},

			{(void*)&lua.Lcallmeta,		"luaL_callmeta"},
			{(void*)&lua.Lnewmetatable,	"luaL_newmetatable"},

			{NULL, NULL}
		};
		lua.triedlib = true;
		lua.lib = Sys_LoadLibrary("lua52", luafuncs);
	}
	if (!lua.lib)
		return false;
	return true;
}

static void *my_lua_alloc (void *ud, void *ptr, size_t osize, size_t nsize)
{
	if (nsize == 0)
	{
		free(ptr);
		return NULL;
	}
	else
		return realloc(ptr, nsize);
}
const char * my_lua_Reader(lua_State *L, void *data, size_t *size)
{
	vfsfile_t *f = data;
	*size = VFS_READ(f, lua.readbuf, sizeof(lua.readbuf));
	return lua.readbuf;
}

//replace lua's standard 'print' function to use the console instead of stdout. intended to use the same linebreak rules.
static int my_lua_print(lua_State *L)
{
	//the problem is that we can only really accept strings here.
	//so lets just use the tostring function to make sure things are actually readable as strings.
	int args = lua.gettop(L);
	int i;
	const char *s;
	lua.getglobal(L, "tostring");
	//args now start at 1
	for(i = 1; i <= args; i++)
	{
		lua.pushvalue(L, -1);
		lua.pushvalue(L, i);
		lua.pcall(L, 1, 1, 0);	//pops args+func
		s = lua.tolstring(L, -1, NULL);
		if(s == NULL)
			s = "?";

		if(i > 1) Con_Printf("\t");
		Con_Printf("%s", s);
		lua.pop(L, 1);			//pop our lstring
	};
	lua.pop(L, 1);			//pop the cached tostring.
	Con_Printf("\n");
	return 0;
};
//more like quakec's print
static int my_lua_conprint(lua_State *L)
{
	//the problem is that we can only really accept strings here.
	//so lets just use the tostring function to make sure things are actually readable as strings.
	int args = lua.gettop(L);
	int i;
	const char *s;

	lua.getglobal(L, "tostring");
	//args start at stack index 1
	for(i = 1; i <= args; i++)
	{
		lua.pushvalue(L, -1);	//dupe the tostring
		lua.pushvalue(L, i);	//dupe the argument
		lua.pcall(L, 1, 1, 0);	//pops args+func, pushes the string result
		s = lua.tolstring(L, -1, NULL);
		if(s == NULL)
			s = "?";

		Con_Printf("%s", s);
		lua.pop(L, 1);			//pop our lstring
	};
	lua.pop(L, 1);			//pop the cached tostring.
	return 0;
};
static int bi_lua_dprint(lua_State *L)
{
	if (!developer.ival)
		return 0;
	return my_lua_conprint(L);
}

//taken from lua's baselib.c, with dependancies reduced a little.
static int my_lua_tostring(lua_State *L)
{
//	if (lua.type(L, 1) == LUA_TNONE)
//		luaL_argerror(L, narg, "value expected");
	if (lua.Lcallmeta(L, 1, "__tostring"))
		return 1;
	switch (lua.type(L, 1))
	{
	case LUA_TNUMBER:
		lua.pushfstring(L, lua.tolstring(L, 1, NULL));
		break;
	case LUA_TSTRING:
		lua.pushvalue(L, 1);
		break;
	case LUA_TBOOLEAN:
		lua.pushstring(L, (lua.toboolean(L, 1) ? "true" : "false"));
		break;
	case LUA_TNIL:
		lua.pushstring(L, "nil");
		break;
	case LUA_TTABLE:
		//special check for things that look like vectors.
		lua.getfield(L, 1, "x");
		lua.getfield(L, 1, "y");
		lua.getfield(L, 1, "z");
		if (lua.type(L, -3) == LUA_TNUMBER && lua.type(L, -2) == LUA_TNUMBER && lua.type(L, -1) == LUA_TNUMBER)
		{
			lua.pushfstring(L, "'%g %g %g'", lua.tonumberx(L, -3, NULL), lua.tonumberx(L, -2, NULL), lua.tonumberx(L, -1, NULL));
			return 1;
		}
		//fallthrough
	default:
		lua.pushfstring(L, "%s: %p", lua.typename(L, lua.type(L, 1)), lua.topointer(L, 1));
		break;
	}
	return 1;
}

static int my_lua_panic(lua_State *L)
{
	const char *s = lua.tolstring(L, -1, NULL);
	Sys_Error("lua error: %s", s);
}

static int my_lua_entity_eq(lua_State *L)
{
	//table1=1
	//table2=2
	unsigned int entnum1, entnum2;
	lua.getfield(L, 1, "entnum");
	entnum1 = lua.tointegerx(L, -1, NULL);
	lua.getfield(L, 2, "entnum");
	entnum2 = lua.tointegerx(L, -1, NULL);
	lua.pop(L, 2);

	lua.pushboolean(L, entnum1 == entnum2);
	return 1;
}

static int my_lua_entity_tostring(lua_State *L)
{
	//table=1
	unsigned int entnum;
	lua.getfield(L, 1, "entnum");
	entnum = lua.tointegerx(L, -1, NULL);
	lua.pop(L, 1);

	lua.pushstring(L, va("entity: %u", entnum));
	return 1;
}

static void lua_readvector(lua_State *L, int idx, float *result)
{
	switch(lua.type(L, idx))
	{
	case LUA_TSTRING:
		{
			//we parse strings primnarily for easy .ent(or bsp) loading support.
			const char *str = lua.tolstring(L, idx, NULL);
			str = COM_Parse(str);
			result[0] = atof(com_token);
			str = COM_Parse(str);
			result[1] = atof(com_token);
			str = COM_Parse(str);
			result[2] = atof(com_token);
		}
		break;
	case LUA_TTABLE:
		lua.getfield(L, idx, "x");
		result[0] = lua.tonumberx(L, -1, NULL);
		lua.getfield(L, idx, "y");
		result[1] = lua.tonumberx(L, -1, NULL);
		lua.getfield(L, idx, "z");
		result[2] = lua.tonumberx(L, -1, NULL);
		lua.pop(L, 3);
		break;
	case LUA_TNIL:
		result[0] = 0;
		result[1] = 0;
		result[2] = 0;
		break;
	default:
		Con_Printf("Expected vector, got something that wasn't\n");
	}
}

static int my_lua_entity_set(lua_State *L)	//__newindex
{
//	Con_Printf("lua_entity_set: ");
//	my_lua_print(L);
	//table=1
	//key=2
	//value=3

	if (lua.type(L, 2) == LUA_TSTRING)
	{
		const char *s = lua.tolstring(L, 2, NULL);
		luafld_t *fld = Hash_GetInsensitive(&lua.entityfields, s);
		eval_t *eval;
		unsigned int entnum;
		if (fld)
		{
			lua.getfield(L, 1, "entnum");
			entnum = lua.tointegerx(L, -1, NULL);
			lua.pop(L, 1);
			if (entnum < lua.maxedicts && lua.edicttable[entnum] && !ED_ISFREE(lua.edicttable[entnum]))
			{
				eval = (eval_t*)((char*)lua.edicttable[entnum]->v + fld->offset);
				switch(fld->type)
				{
				case ev_float:
					eval->_float = lua.tonumberx(L, 3, NULL);
					return 0;
				case ev_vector:
					lua_readvector(L, 3, eval->_vector);
					return 0;
				case ev_integer:
					eval->_int = lua.tointegerx(L, 3, NULL);
					return 0;
				case ev_function:
					if (lua.type(L, 3) == LUA_TNIL)
						eval->function = 0;	//so the engine can distinguish between nil and not.
					else
						eval->function = fld->offset | ((entnum+1)<<10);
					lua.pushlightuserdata(L, (void *)(qintptr_t)fld->offset);	//execute only knows a function id, so we need to translate the store to match.
					lua.replace(L, 2);
					lua.rawset(L, 1);
					return 0;
				case ev_string:
					if (lua.type(L, 3) == LUA_TNIL)
						eval->string = 0;	//so the engine can distinguish between nil and not.
					else
						eval->string = fld->offset | ((entnum+1)<<10);
					lua.pushlightuserdata(L, (void *)(qintptr_t)fld->offset);	//execute only knows a string id, so we need to translate the store to match.
					lua.replace(L, 2);
					lua.rawset(L, 1);
					return 0;
				case ev_entity:
					//read the table's entnum field so we know which one its meant to be.
					lua.getfield(L, 3, "entnum");
					eval->edict = lua.tointegerx(L, -1, NULL);
					return 0;
				}
			}
		}
	}

	lua.rawset(L, 1);
	return 0;
}
static int my_lua_entity_get(lua_State *L)	//__index
{
//	Con_Printf("lua_entity_get: ");
//	my_lua_print(L);
	//table=1
	//key=2

	if (lua.type(L, 2) == LUA_TSTRING)
	{
		const char *s = lua.tolstring(L, 2, NULL);
		luafld_t *fld = Hash_GetInsensitive(&lua.entityfields, s);
		eval_t *eval;
		int entnum;
		if (fld)
		{
			lua.getfield(L, 1, "entnum");
			entnum = lua.tointegerx(L, -1, NULL);
			lua.pop(L, 1);
			if (entnum < lua.maxedicts && lua.edicttable[entnum])// && !lua.edicttable[entnum]->isfree)
			{
				eval = (eval_t*)((char*)lua.edicttable[entnum]->v + fld->offset);
				switch(fld->type)
				{
				case ev_float:
					lua.pushnumber(L, eval->_float);
					return 1;
				case ev_integer:
					lua.pushinteger(L, eval->_int);
					return 1;
				case ev_vector:
					lua.createtable(L, 0, 0);
					//FIXME: should provide a metatable with a __tostring
					lua.pushnumber(L, eval->_vector[0]);
					lua.setfield (L, -2, "x");
					lua.pushnumber(L, eval->_vector[1]);
					lua.setfield (L, -2, "y");
					lua.pushnumber(L, eval->_vector[2]);
					lua.setfield (L, -2, "z");
					return 1;
				case ev_function:
				case ev_string:
					lua.pushlightuserdata(L, (void *)(qintptr_t)(eval->function & 1023));	//execute only knows a function id, so we need to translate the store to match.
					lua.replace(L, 2);
					lua.rawget(L, 1);
					return 1;
				case ev_entity:
					//return the table for the entity via the lua registry.
					lua.pushlightuserdata(lua.ctx, lua.edicttable[eval->edict]);
					lua.gettable(lua.ctx, LUA_REGISTRYINDEX);
					return 1;
				}
			}
		}
	}

	//make sure it exists so we don't get called constantly if code loops through stuff that wasn't set.
//	lua.pushstring(L, "nil");
	lua.rawget(L, 1);
	return 1;
}
static int my_lua_global_set(lua_State *L)	//__newindex
{
//	Con_Printf("my_lua_global_set: ");
//	my_lua_print(L);
	//table=1
	//key=2
	//value=3

	if (lua.type(L, 2) == LUA_TSTRING)
	{
		const char *s = lua.tolstring(L, 2, NULL);
		luafld_t *fld = Hash_GetInsensitive(&lua.globalfields, s);
		eval_t *eval;
		if (fld)
		{
			eval = (eval_t*)((char*)&lua.globals + fld->offset);
			switch(fld->type)
			{
			case ev_float:
				eval->_float = lua.tonumberx(L, 3, NULL);
				return 0;
			case ev_vector:
				lua.getfield(L, 3, "x");
				eval->_vector[0] = lua.tonumberx(L, -1, NULL);
				lua.getfield(L, 3, "y");
				eval->_vector[1] = lua.tonumberx(L, -1, NULL);
				lua.getfield(L, 3, "z");
				eval->_vector[2] = lua.tonumberx(L, -1, NULL);
				return 0;
			case ev_integer:
				eval->_int = lua.tointegerx(L, 3, NULL);
				return 0;
			case ev_function:
				if (lua.type(L, 3) == LUA_TNIL)
					eval->function = 0;	//so the engine can distinguish between nil and not.
				else
					eval->function = fld->offset;
				lua.pushlightuserdata(L, (void *)fld->offset);	//execute only knows a function id, so we need to translate the store to match.
				lua.replace(L, 2);
				lua.rawset(L, 1);
				return 0;
			case ev_string:
				if (lua.type(L, 3) == LUA_TNIL)
					eval->string = 0;	//so the engine can distinguish between nil and not.
				else
					eval->string = fld->offset;
				lua.pushlightuserdata(L, (void *)fld->offset);	//execute only knows a string id, so we need to translate the store to match.
				lua.replace(L, 2);
				lua.rawset(L, 1);
				return 0;
			case ev_entity:
				//read the table's entnum field so we know which one its meant to be.
				lua.getfield(L, 3, "entnum");
				eval->edict = lua.tointegerx(L, -1, NULL);
				return 0;
			}
		}
	}

	lua.rawset(L, 1);
	return 0;
}
static int my_lua_global_get(lua_State *L)	//__index
{
//	Con_Printf("my_lua_global_get: ");
//	my_lua_print(L);
	//table=1
	//key=2

	if (lua.type(L, 2) == LUA_TSTRING)
	{
		const char *s = lua.tolstring(L, 2, NULL);
		luafld_t *fld = Hash_GetInsensitive(&lua.globalfields, s);
		eval_t *eval;
		if (fld)
		{
			eval = (eval_t*)((char*)&lua.globals + fld->offset);
			switch(fld->type)
			{
			case ev_float:
				lua.pushnumber(L, eval->_float);
				return 1;
			case ev_function:
				lua.pushlightuserdata(L, (void *)(qintptr_t)eval->function);	//execute only knows a function id, so we need to translate the store to match.
				lua.replace(L, 2);
				lua.rawget(L, 1);
				return 1;
			case ev_entity:
				//return the table for the entity via the lua registry.
				lua.pushlightuserdata(lua.ctx, lua.edicttable[eval->edict]);
				lua.gettable(lua.ctx, LUA_REGISTRYINDEX);
				return 1;
			}
		}
	}

	//make sure it exists so we don't get called constantly if code loops through stuff that wasn't set.
//	lua.pushstring(L, "nil");
	lua.rawget(L, 1);
	return 1;
}

static int bi_lua_setmodel(lua_State *L)
{
	int entnum;
	edict_t *e;
	lua.getfield(L, 1, "entnum");
	entnum = lua.tointegerx(L, -1, NULL);
	e = (entnum>=lua.maxedicts)?NULL:lua.edicttable[entnum];
	PF_setmodel_Internal(&lua.progfuncs, e, lua.tolstring(L, 2, NULL));
	return 0;
}
static int bi_lua_precache_model(lua_State *L)
{
	PF_precache_model_Internal(&lua.progfuncs, lua.tolstring(L, 1, NULL), false);
	return 0;
}
static int bi_lua_precache_sound(lua_State *L)
{
	PF_precache_sound_Internal(&lua.progfuncs, lua.tolstring(L, 1, NULL));
	return 0;
}
static int bi_lua_lightstyle(lua_State *L)
{
	vec3_t rgb;
	lua_readvector(L, 3, rgb);
	PF_applylightstyle(lua.tointegerx(L, 1, NULL), lua.tolstring(L, 2, NULL), rgb);
	return 0;
}
static int bi_lua_spawn(lua_State *L)
{
	edict_t *e = lua.progfuncs.EntAlloc(&lua.progfuncs, false, 0);
	if (e)
	{
		lua.pushlightuserdata(L, e);
		lua.gettable(L, LUA_REGISTRYINDEX);
	}
	else
		lua.pushnil(L);
	return 1;
}
static int bi_lua_remove(lua_State *L)
{
	int entnum;
	edict_t *e;
	lua.getfield(L, 1, "entnum");
	entnum = lua.tointegerx(L, -1, NULL);
	e = (entnum>=lua.maxedicts)?NULL:lua.edicttable[entnum];
	if (e)
		lua.progfuncs.EntFree(&lua.progfuncs, e);
	return 0;
}
static int bi_lua_setorigin(lua_State *L)
{
	edict_t *e;
	lua.getfield(L, 1, "entnum");
	e = EDICT_NUM((&lua.progfuncs), lua.tointegerx(L, -1, NULL));
	lua_readvector(L, 2, e->v->origin);
	World_LinkEdict (&sv.world, (wedict_t*)e, false);
	return 0;
}
static int bi_lua_setsize(lua_State *L)
{
	edict_t *e;
	lua.getfield(L, 1, "entnum");
	e = EDICT_NUM((&lua.progfuncs), lua.tointegerx(L, -1, NULL));
	lua_readvector(L, 2, e->v->mins);
	lua_readvector(L, 3, e->v->maxs);
	VectorSubtract (e->v->maxs, e->v->mins, e->v->size);
	World_LinkEdict (&sv.world, (wedict_t*)e, false);
	return 0;
}

static int bi_lua_localcmd(lua_State *L)
{
	const char	*str = lua.tolstring(lua.ctx, 1, NULL);
	Cbuf_AddText (str, RESTRICT_INSECURE);
	return 0;
}
static int bi_lua_changelevel(lua_State *L)
{
	const char	*s, *spot;

// make sure we don't issue two changelevels (unless the last one failed)
	if (sv.mapchangelocked)
		return 0;
	sv.mapchangelocked = true;

	if (lua.type(L, 2) == LUA_TSTRING)	//and not nil or none
	{
		s = lua.tolstring(lua.ctx, 1, NULL);
		spot = lua.tolstring(lua.ctx, 2, NULL);
		Cbuf_AddText (va("\nchangelevel %s %s\n",s, spot), RESTRICT_LOCAL);
	}
	else
	{
		s = lua.tolstring(lua.ctx, 1, NULL);
		Cbuf_AddText (va("\nmap %s\n",s), RESTRICT_LOCAL);
	}
	return 0;
}

static int bi_lua_stuffcmd(lua_State *L)
{
	int entnum;
	const char *str;
	lua.getfield(L, 1, "entnum");
	entnum = lua.tointegerx(L, -1, NULL);
	str = lua.tolstring(L, 2, NULL);

	PF_stuffcmd_Internal(entnum, str, 0);
	return 0;
}
static int bi_lua_centerprint(lua_State *L)
{
	int entnum;
	const char *str;
	lua.getfield(L, 1, "entnum");
	entnum = lua.tointegerx(L, -1, NULL);
	str = lua.tolstring(L, 2, NULL);

	PF_centerprint_Internal(entnum, false, str);
	return 0;
}
static int bi_lua_getinfokey(lua_State *L)
{
	int entnum;
	const char *key;
	lua.getfield(L, 1, "entnum");
	entnum = lua.tointegerx(L, -1, NULL);
	key = lua.tolstring(L, 2, NULL);

	key = PF_infokey_Internal(entnum, key);
	lua.pushstring(L, key);
	return 1;
}
static int bi_lua_setinfokey(lua_State *L)
{
	int entnum;
	const char *key;
	const char *value;
	int result;
	lua.getfield(L, 1, "entnum");
	entnum = lua.tointegerx(L, -1, NULL);
	key = lua.tolstring(L, 2, NULL);
	value = lua.tolstring(L, 3, NULL);

	result = PF_ForceInfoKey_Internal(entnum, key, value);
	lua.pushinteger(L, result);
	return 1;
}
static int bi_lua_ambientsound(lua_State *L)
{
	vec3_t pos;
	const char *samp = lua.tolstring(L, 2, NULL);
	float vol = lua.tonumberx(L, 3, NULL);
	float attenuation = lua.tonumberx(L, 4, NULL);
	lua_readvector(L, 1, pos);

	PF_ambientsound_Internal(pos, samp, vol, attenuation);
	return 0;
}
static int bi_lua_sound(lua_State *L)
{
	int entnum;
	float channel = lua.tonumberx(L, 2, NULL);
	const char *samp = lua.tolstring(L, 3, NULL);
	float volume = lua.tonumberx(L, 4, NULL);
	float attenuation = lua.tonumberx(L, 5, NULL);

	lua.getfield(L, 1, "entnum");
	entnum = lua.tointegerx(L, -1, NULL);

	//note: channel & 256 == reliable

	SVQ1_StartSound (NULL, (wedict_t*)EDICT_NUM((&lua.progfuncs), entnum), channel, samp, volume*255, attenuation, 0, 0, 0);
	return 0;
}

static int bi_lua_pointcontents(lua_State *L)
{
	vec3_t pos;
	lua_readvector(L, 1, pos);
	lua.pushinteger(L, sv.world.worldmodel->funcs.PointContents(sv.world.worldmodel, NULL, pos));
	return 1;
}

static int bi_lua_setspawnparms(lua_State *L)
{
	globalvars_t pr_globals;

	lua.getfield(L, 1, "entnum");
	pr_globals.param[0].i = lua.tointegerx(L, -1, NULL);
	PF_setspawnparms(&lua.progfuncs, &pr_globals);
	return 0;
}
static int bi_lua_makestatic(lua_State *L)
{
	globalvars_t pr_globals;

	lua.getfield(L, 1, "entnum");
	pr_globals.param[0].i = lua.tointegerx(L, -1, NULL);
	PF_makestatic(&lua.progfuncs, &pr_globals);
	return 0;
}

static int bi_lua_droptofloor(lua_State *L)
{
	extern cvar_t pr_droptofloorunits;
	wedict_t	*ent;
	vec3_t		end;
	vec3_t		start;
	trace_t		trace;
	const float *gravitydir;
	static const vec3_t standardgravity = {0,0,-1};
	pubprogfuncs_t *prinst = &lua.progfuncs;
	world_t *world = prinst->parms->user;

	ent = PROG_TO_WEDICT((&lua.progfuncs), pr_global_struct->self);

	if (ent->xv->gravitydir[2] || ent->xv->gravitydir[1] || ent->xv->gravitydir[0])
		gravitydir = ent->xv->gravitydir;
	else
		gravitydir = standardgravity;

	VectorCopy (ent->v->origin, end);
	if (pr_droptofloorunits.value > 0)
		VectorMA(end, pr_droptofloorunits.value, gravitydir, end);
	else
		VectorMA(end, 256, gravitydir, end);

	VectorCopy (ent->v->origin, start);
	trace = World_Move (world, start, ent->v->mins, ent->v->maxs, end, MOVE_NORMAL, ent);

	if (trace.fraction == 1 || trace.allsolid)
		lua.pushboolean(L, false);
	else
	{
		VectorCopy (trace.endpos, ent->v->origin);
		World_LinkEdict (world, ent, false);
		ent->v->flags = (int)ent->v->flags | FL_ONGROUND;
		ent->v->groundentity = EDICT_TO_PROG(prinst, trace.ent);
		lua.pushboolean(L, true);
	}
	return 1;
}

static int bi_lua_checkbottom(lua_State *L)
{
	qboolean okay;
	int entnum;
	vec3_t up = {0,0,1};
	lua.getfield(L, 1, "entnum");
	entnum = lua.tointegerx(L, -1, NULL);
	okay = World_CheckBottom(&sv.world, (wedict_t*)EDICT_NUM((&lua.progfuncs), entnum), up);
	lua.pushboolean(L, okay);
	return 1;
}

static int bi_lua_bprint(lua_State *L)
{
	int level = lua.tointegerx(L, 1, NULL);
	const char *str = lua.tolstring(L, 2, NULL);
	SV_BroadcastPrintf (level, "%s", str);
	return 0;
}
static int bi_lua_sprint(lua_State *L)
{
	int entnum;
	int level = lua.tointegerx(L, 2, NULL);
	const char *str = lua.tolstring(L, 3, NULL);

	lua.getfield(L, 1, "entnum");
	entnum = lua.tointegerx(L, -1, NULL);

	if (entnum < 1 || entnum > sv.allocated_client_slots)
	{
		Con_TPrintf ("tried to sprint to a non-client\n");
		return 0;
	}
	SV_ClientPrintf (&svs.clients[entnum-1], level, "%s", str);
	return 0;
}

static int bi_lua_cvar_set(lua_State *L)
{
	const char *name = lua.tolstring(L, 1, NULL);
	const char *str = lua.tolstring(L, 2, NULL);
	cvar_t *var = Cvar_FindVar(name);
	if (var)
		Cvar_Set(var, str);
	return 0;
}
static int bi_lua_cvar_get(lua_State *L)
{
	const char *name = lua.tolstring(L, 1, NULL);
	cvar_t *var = Cvar_FindVar(name);
	if (var)
		lua.pushstring(L, var->string);
	else
		lua.pushnil(L);
	return 0;
}

static void set_trace_globals(trace_t *trace)
{
	pr_global_struct->trace_allsolid = trace->allsolid;
	pr_global_struct->trace_startsolid = trace->startsolid;
	pr_global_struct->trace_fraction = trace->fraction;
	pr_global_struct->trace_inwater = trace->inwater;
	pr_global_struct->trace_inopen = trace->inopen;
	pr_global_struct->trace_surfaceflagsf = trace->surface?trace->surface->flags:0;
	pr_global_struct->trace_surfaceflagsi = trace->surface?trace->surface->flags:0;
//	if (pr_global_struct->trace_surfacename)
//		prinst->SetStringField(prinst, NULL, &pr_global_struct->trace_surfacename, tr->surface?tr->surface->name:"", true);
	pr_global_struct->trace_endcontentsf = trace->contents;
	pr_global_struct->trace_endcontentsi = trace->contents;
//	if (trace.fraction != 1)
//		VectorMA (trace->endpos, 4, trace->plane.normal, P_VEC(trace_endpos));
//	else
		VectorCopy (trace->endpos, P_VEC(trace_endpos));
	VectorCopy (trace->plane.normal, P_VEC(trace_plane_normal));
	pr_global_struct->trace_plane_dist =  trace->plane.dist;
	pr_global_struct->trace_ent = trace->ent?((wedict_t*)trace->ent)->entnum:0;
}

static int bi_lua_traceline(lua_State *L)
{
	vec3_t v1, v2;
	trace_t	trace;
	int		nomonsters;
	wedict_t	*ent;

	lua_readvector(L, 1, v1);
	lua_readvector(L, 2, v2);
	nomonsters = lua.tointegerx(L, 3, NULL);
	lua.getfield(L, 4, "entnum");
	ent = (wedict_t*)EDICT_NUM((&lua.progfuncs), lua.tointegerx(L, -1, NULL));

	trace = World_Move (&sv.world, v1, vec3_origin, vec3_origin, v2, nomonsters|MOVE_IGNOREHULL, (wedict_t*)ent);

	//FIXME: should we just return a table instead, and ignore the globals?
	set_trace_globals(&trace);
	return 0;
}

static int bi_lua_tracebox(lua_State *L)
{
	vec3_t v1, v2, mins, maxs;
	trace_t	trace;
	int		nomonsters;
	wedict_t	*ent;

	lua_readvector(L, 1, v1);
	lua_readvector(L, 2, v2);
	lua_readvector(L, 3, mins);
	lua_readvector(L, 4, maxs);
	nomonsters = lua.tointegerx(L, 5, NULL);
	lua.getfield(L, 6, "entnum");
	ent = (wedict_t*)EDICT_NUM((&lua.progfuncs), lua.tointegerx(L, -1, NULL));

	trace = World_Move (&sv.world, v1, mins, maxs, v2, nomonsters|MOVE_IGNOREHULL, (wedict_t*)ent);

	//FIXME: should we just return a table instead, and ignore the globals?
	set_trace_globals(&trace);
	return 0;
}

static int bi_lua_walkmove(lua_State *L)
{
	pubprogfuncs_t *prinst = &lua.progfuncs;
	world_t *world = prinst->parms->user;
	wedict_t	*ent;
	float	yaw, dist;
	vec3_t	move;
	int 	oldself;
	vec3_t	axis[3];

	ent = PROG_TO_WEDICT(prinst, *world->g.self);
	yaw = lua.tonumberx(L, 1, NULL);
	dist = lua.tonumberx(L, 2, NULL);

	if ( !( (int)ent->v->flags & (FL_ONGROUND|FL_FLY|FL_SWIM) ) )
	{
		lua.pushboolean(L, false);
		return 1;
	}

	World_GetEntGravityAxis(ent, axis);

	yaw = yaw*M_PI*2 / 360;

	VectorScale(axis[0], cos(yaw)*dist, move);
	VectorMA(move, sin(yaw)*dist, axis[1], move);

// save program state, because World_movestep may call other progs
	oldself = *world->g.self;

	lua.pushboolean(L, World_movestep(world, ent, move, axis, true, false, NULL, NULL));

// restore program state
	*world->g.self = oldself;

	return 1;
}
static int bi_lua_movetogoal(lua_State *L)
{
	pubprogfuncs_t *prinst = &lua.progfuncs;
	world_t *world = prinst->parms->user;
	wedict_t	*ent;
	float dist;
	ent = PROG_TO_WEDICT(prinst, *world->g.self);
	dist = lua.tonumberx(L, 1, NULL);
	World_MoveToGoal (world, ent, dist);
	return 0;
}

static int bi_lua_nextent(lua_State *L)
{
	world_t *world = &sv.world;
	int		i;
	wedict_t	*ent;

	lua.getfield(L, 1, "entnum");
	i = lua.tointegerx(L, -1, NULL);

	while (1)
	{
		i++;
		if (i == world->num_edicts)
		{
			ent = world->edicts;
			break;
		}
		ent = WEDICT_NUM(world->progs, i);
		if (!ED_ISFREE(ent))
		{
			break;
		}
	}
	lua.pushlightuserdata(L, ent);
	lua.gettable(L, LUA_REGISTRYINDEX);
	return 1;
}

static int bi_lua_nextclient(lua_State *L)
{
	world_t *world = &sv.world;
	int		i;
	wedict_t	*ent;

	lua.getfield(L, 1, "entnum");
	i = lua.tointegerx(L, -1, NULL);

	while (1)
	{
		i++;
		if (i == sv.allocated_client_slots)
		{
			ent = world->edicts;
			break;
		}
		ent = WEDICT_NUM(world->progs, i);
		if (!ED_ISFREE(ent))
		{
			break;
		}
	}
	lua.pushlightuserdata(L, ent);
	lua.gettable(L, LUA_REGISTRYINDEX);
	return 1;
}

static int bi_lua_checkclient(lua_State *L)
{
	pubprogfuncs_t *prinst = &lua.progfuncs;
	wedict_t *ent;
	ent = WEDICT_NUM(prinst, PF_checkclient_Internal(prinst));
	lua.pushlightuserdata(L, ent);
	lua.gettable(L, LUA_REGISTRYINDEX);
	return 1;
}

static int bi_lua_random(lua_State *L)
{
	lua.pushnumber(L, (rand ()&0x7fff) / ((float)0x8000));
	return 1;
}

static int bi_lua_makevectors(lua_State *L)
{
	vec3_t angles;
	//this is annoying as fuck in lua, what with it writing globals and stuff.
	//perhaps we should support f,u,l=makevectors(ang)... meh, cba.
	lua_readvector(L, 1, angles);
	AngleVectors (angles, pr_global_struct->v_forward, pr_global_struct->v_right, pr_global_struct->v_up);
	return 0;
}
static int bi_lua_vectoangles(lua_State *L)
{
	vec3_t forward;
	vec3_t up;
	float *uv = NULL;
	vec3_t ret;
	lua_readvector(L, 1, forward);
	if (lua.type(L, 2) != LUA_TNONE)
	{
		lua_readvector(L, 1, up);
		uv = up;
	}
	VectorAngles(forward, uv, ret, true);

	lua.createtable(L, 0, 0);
	//FIXME: should provide a metatable with a __tostring
	lua.pushnumber(L, ret[0]);
	lua.setfield (L, -2, "x");
	lua.pushnumber(L, ret[1]);
	lua.setfield (L, -2, "y");
	lua.pushnumber(L, ret[2]);
	lua.setfield (L, -2, "z");
	return 1;
}

static int bi_lua_tokenize(lua_State *L)
{
	const char *instring = lua.tolstring(L, 1, NULL);
	int argc = 0;
	lua.createtable(L, 0, 0);
	while(NULL != (instring = COM_Parse(instring)))
	{
		//lua is traditionally 1-based
		//for i=1,t.argc do
		lua.pushinteger(L, ++argc);
		lua.pushstring(L, com_token);
		lua.settable(L, -3);

		if (argc == 1)
		{
			while (*instring == ' ' || *instring == '\t')
				instring++;
			lua.pushstring(L, instring);
			lua.setfield(L, -2, "args");	//args is all-but-the-first
		}
	}
	lua.pushinteger(L, argc);
	lua.setfield(L, -2, "argc");	//argc is the count.
	return 1;
}

static int bi_lua_findradiuschain(lua_State *L)
{
	extern cvar_t sv_gameplayfix_blowupfallenzombies;
	world_t *world = &sv.world;
	edict_t	*ent, *chain;
	float	rad;
	vec3_t	org;
	vec3_t	eorg;
	int		i, j;

	chain = (edict_t *)world->edicts;

	lua_readvector(L, 1, org);
	rad = lua.tonumberx(L, 2, NULL);
	rad = rad*rad;

	for (i=1 ; i<world->num_edicts ; i++)
	{
		ent = EDICT_NUM(world->progs, i);
		if (ED_ISFREE(ent))
			continue;
		if (ent->v->solid == SOLID_NOT && (progstype != PROG_QW || !((int)ent->v->flags & FL_FINDABLE_NONSOLID)) && !sv_gameplayfix_blowupfallenzombies.value)
			continue;
		for (j=0 ; j<3 ; j++)
			eorg[j] = org[j] - (ent->v->origin[j] + (ent->v->mins[j] + ent->v->maxs[j])*0.5);
		if (DotProduct(eorg,eorg) > rad)
			continue;

		ent->v->chain = EDICT_TO_PROG(world->progs, chain);
		chain = ent;
	}

	lua.pushlightuserdata(L, chain);
	lua.gettable(L, LUA_REGISTRYINDEX);
	return 1;
}
static int bi_lua_findradiustable(lua_State *L)
{
	extern cvar_t sv_gameplayfix_blowupfallenzombies;
	world_t *world = &sv.world;
	edict_t	*ent, *chain;
	float	rad;
	vec3_t	org;
	vec3_t	eorg;
	int		i, j;
	int results = 1;	//lua arrays are 1-based

	chain = (edict_t *)world->edicts;

	lua_readvector(L, 1, org);
	rad = lua.tonumberx(L, 2, NULL);
	rad = rad*rad;

	lua.createtable(L, 0, 0);	//our return value.

	for (i=1 ; i<world->num_edicts ; i++)
	{
		ent = EDICT_NUM(world->progs, i);
		if (ED_ISFREE(ent))
			continue;
		if (ent->v->solid == SOLID_NOT && (progstype != PROG_QW || !((int)ent->v->flags & FL_FINDABLE_NONSOLID)) && !sv_gameplayfix_blowupfallenzombies.value)
			continue;
		for (j=0 ; j<3 ; j++)
			eorg[j] = org[j] - (ent->v->origin[j] + (ent->v->mins[j] + ent->v->maxs[j])*0.5);
		if (DotProduct(eorg,eorg) > rad)
			continue;

		lua.pushinteger(L, ++results);
		lua.pushlightuserdata(L, ent);
		lua.gettable(L, LUA_REGISTRYINDEX);
		lua.settable(L, -3);
	}

	lua.pushlightuserdata(L, chain);
	lua.gettable(L, LUA_REGISTRYINDEX);
	return 1;
}
#define bi_lua_findradius bi_lua_findradiuschain

static int bi_lua_multicast(lua_State *L)
{
	int dest;
	vec3_t org;

	dest = lua.tointegerx(L, 1, NULL);
	lua_readvector(L, 2, org);

	NPP_Flush();
	SV_Multicast (org, dest);

	return 0;
}
extern sizebuf_t csqcmsgbuffer;
static int bi_lua_writechar(lua_State *L)
{
	globalvars_t pr_globals;
	pr_globals.param[0].f = (csqcmsgbuffer.maxsize?MSG_CSQC:MSG_MULTICAST);
	pr_globals.param[1].f = lua.tonumberx(L, 1, NULL);
	PF_WriteChar(&lua.progfuncs, &pr_globals);
	return 0;
}
static int bi_lua_writebyte(lua_State *L)
{
	globalvars_t pr_globals;
	pr_globals.param[0].f = (csqcmsgbuffer.maxsize?MSG_CSQC:MSG_MULTICAST);
	pr_globals.param[1].f = lua.tonumberx(L, 1, NULL);
	PF_WriteByte(&lua.progfuncs, &pr_globals);
	return 0;
}
static int bi_lua_writeshort(lua_State *L)
{
	globalvars_t pr_globals;
	pr_globals.param[0].f = (csqcmsgbuffer.maxsize?MSG_CSQC:MSG_MULTICAST);
	pr_globals.param[1].f = lua.tonumberx(L, 1, NULL);
	PF_WriteShort(&lua.progfuncs, &pr_globals);
	return 0;
}
static int bi_lua_writelong(lua_State *L)
{
	globalvars_t pr_globals;
	pr_globals.param[0].f = (csqcmsgbuffer.maxsize?MSG_CSQC:MSG_MULTICAST);
	pr_globals.param[1].f = lua.tonumberx(L, 1, NULL);
	PF_WriteLong(&lua.progfuncs, &pr_globals);
	return 0;
}
static int bi_lua_writeangle(lua_State *L)
{
	globalvars_t pr_globals;
	pr_globals.param[0].f = (csqcmsgbuffer.maxsize?MSG_CSQC:MSG_MULTICAST);
	pr_globals.param[1].f = lua.tonumberx(L, 1, NULL);
	PF_WriteAngle(&lua.progfuncs, &pr_globals);
	return 0;
}
static int bi_lua_writecoord(lua_State *L)
{
	globalvars_t pr_globals;
	pr_globals.param[0].f = (csqcmsgbuffer.maxsize?MSG_CSQC:MSG_MULTICAST);
	pr_globals.param[1].f = lua.tonumberx(L, 1, NULL);
	PF_WriteCoord(&lua.progfuncs, &pr_globals);
	return 0;
}
static int bi_lua_writestring(lua_State *L)
{
	PF_WriteString_Internal((csqcmsgbuffer.maxsize?MSG_CSQC:MSG_MULTICAST),lua.tolstring(L, 1, NULL));
	return 0;
}
static int bi_lua_writeentity(lua_State *L)
{
	globalvars_t pr_globals;
	lua.getfield(L, 1, "entnum");
	pr_globals.param[0].f = (csqcmsgbuffer.maxsize?MSG_CSQC:MSG_MULTICAST);
	pr_globals.param[1].i = lua.tointegerx(L, -1, NULL);
	PF_WriteEntity(&lua.progfuncs, &pr_globals);
	return 0;
}

static int bi_lua_bitnot(lua_State *L)
{
	lua.pushinteger(L, ~lua.tointegerx(L, 1, NULL));
	return 1;
}
static int bi_lua_bitclear(lua_State *L)
{
	lua.pushinteger(L, lua.tointegerx(L, 1, NULL)&~lua.tointegerx(L, 2, NULL));
	return 1;
}
static int bi_lua_bitset(lua_State *L)
{
	lua.pushnumber(L, lua.tointegerx(L, 1, NULL)|lua.tointegerx(L, 2, NULL));
	return 1;
}
#define bi_lua_bitor bi_lua_bitset
static int bi_lua_bitand(lua_State *L)
{
	lua.pushnumber(L, lua.tointegerx(L, 1, NULL)&lua.tointegerx(L, 2, NULL));
	return 1;
}
static int bi_lua_bitxor(lua_State *L)
{
	lua.pushnumber(L, lua.tointegerx(L, 1, NULL)^lua.tointegerx(L, 2, NULL));
	return 1;
}

static int bi_lua_sin(lua_State *L)
{
	lua.pushnumber(L, sin(lua.tonumberx(L, 1, NULL)));
	return 1;
}
static int bi_lua_cos(lua_State *L)
{
	lua.pushnumber(L, cos(lua.tonumberx(L, 1, NULL)));
	return 1;
}
static int bi_lua_atan2(lua_State *L)
{
	lua.pushnumber(L, atan2(lua.tonumberx(L, 1, NULL), lua.tonumberx(L, 2, NULL)));
	return 1;
}
static int bi_lua_sqrt(lua_State *L)
{
	lua.pushnumber(L, sin(lua.tonumberx(L, 1, NULL)));
	return 1;
}
static int bi_lua_floor(lua_State *L)
{
	lua.pushnumber(L, floor(lua.tonumberx(L, 1, NULL)));
	return 1;
}
static int bi_lua_ceil(lua_State *L)
{
	lua.pushnumber(L, ceil(lua.tonumberx(L, 1, NULL)));
	return 1;
}
static int bi_lua_acos(lua_State *L)
{
	lua.pushnumber(L, acos(lua.tonumberx(L, 1, NULL)));
	return 1;
}

typedef struct
{
	lua_State *L;
	int idx;
} luafsenum_t;
static int QDECL lua_file_enumerate(const char *fname, qofs_t fsize, time_t mtime, void *param, searchpathfuncs_t *spath)
{
	luafsenum_t *e = param;
	lua.pushinteger(e->L, e->idx++);
	lua.pushfstring(e->L, "%s", fname);
	lua.settable(e->L, -3);
	return true;
}
static int bi_lua_getfilelist(lua_State *L)
{
	luafsenum_t e;
	const char *path = lua.tolstring(L, 1, NULL);
	e.L = L;
	e.idx = 1;	//lua arrays are 1-based.

	lua.createtable(L, 0, 0);	//our return value.
	COM_EnumerateFiles(path, lua_file_enumerate, &e);
	return 1;
}

static int bi_lua_fclose(lua_State *L)
{
	//both fclose and __gc.
	//should we use a different function so that we can warn on dupe fcloses without bugging out on fclose+gc?
	//meh, cba
	vfsfile_t **f = lua.touserdata(L, 1);
	if (f && *f != NULL)
	{
		VFS_CLOSE(*f);
		*f = NULL;
	}
	return 0;
}
static int bi_lua_fopen(lua_State *L)
{
	vfsfile_t *f;
	const char *fname = lua.tolstring(L, 1, NULL);
	qboolean read = true;
	vfsfile_t **ud;
	if (read)
		f = FS_OpenVFS(fname, "rb", FS_GAME);
	else
		f = FS_OpenVFS(fname, "wb", FS_GAMEONLY);
	if (!f)
	{
		lua.pushnil(L);
		return 1;
	}
	ud = lua.newuserdata(L, sizeof(vfsfile_t*));
	*ud = f;
	
	lua.createtable(L, 0, 0);
	lua.pushcclosure(L, bi_lua_fclose, 0);
	lua.setfield(L, -2, "__gc");
	lua.setmetatable(L, -2);
	return 1;
}
static int bi_lua_fgets(lua_State *L)
{
	vfsfile_t **f = lua.touserdata(L, 1);
	char line[8192];
	char *r = NULL;
	if (f && *f)
		r = VFS_GETS(*f, line, sizeof(line));

	if (r)
		lua.pushfstring(L, "%s", r);
	else
		lua.pushnil(L);
	return 1;
}
static int bi_lua_fputs(lua_State *L)
{
	vfsfile_t **f = lua.touserdata(L, 1);
	size_t l;
	const char *str = lua.tolstring(L, 2, &l);
	if (f && *f != NULL)
		VFS_WRITE(*f, str, l);
	return 0;
}

static int bi_lua_loadlua(lua_State *L)
{
	const char *fname = lua.tolstring(L, 1, NULL);
	vfsfile_t *sourcefile = FS_OpenVFS(fname, "rb", FS_GAME);
	if (!sourcefile)
	{
		Con_Printf("Error trying to load %s\n", fname);
		lua.pushnil(L);
	}
	else if (0 != lua.load(L, my_lua_Reader, sourcefile, fname, "bt"))	//load the file, embed it within a function and push it
	{
		Con_Printf("Error trying to parse %s: %s\n", fname, lua.tolstring(L, -1, NULL));
		lua.pushnil(L);
	}
	VFS_CLOSE(sourcefile);
	return 1;
}

#define registerfunc(n) lua.pushcclosure(L, bi_lua_##n, 0); lua.setglobal(L, #n);
static void my_lua_registerbuiltins(lua_State *L)
{
	lua.atpanic (L, my_lua_panic);

	//standard lua library replacement
	//this avoids the risk of including any way to access os.execute etc, or other file access.
	lua.pushcclosure(L, my_lua_tostring, 0);
	lua.setglobal(L, "tostring");
	lua.pushcclosure(L, my_lua_print, 0);
	lua.setglobal(L, "print");

	lua.pushcclosure(L, my_lua_conprint, 0);	//for the luls.
	lua.setglobal(L, "conprint");

	registerfunc(loadlua);

	registerfunc(setmodel);
	registerfunc(precache_model);
	registerfunc(precache_sound);
	registerfunc(lightstyle);
	registerfunc(spawn);
	registerfunc(remove);
	registerfunc(nextent);
	registerfunc(nextclient);
	//registerfunc(AIM);
	registerfunc(makestatic);
	registerfunc(setorigin);
	registerfunc(setsize);

	registerfunc(dprint);
	registerfunc(bprint);
	registerfunc(sprint);
	registerfunc(centerprint);
	registerfunc(ambientsound);
	registerfunc(sound);
	registerfunc(random);
	registerfunc(checkclient);
	registerfunc(stuffcmd);
	registerfunc(localcmd);
	registerfunc(cvar_get);
	registerfunc(cvar_set);
	registerfunc(findradius);	//qc legacy compat. should probably warn when its called or sommit.
	registerfunc(findradiuschain);	//like qc.
	registerfunc(findradiustable);	//findradius, but returns an array/table instead.

	registerfunc(traceline);
	registerfunc(tracebox);
	registerfunc(walkmove);
	registerfunc(movetogoal);
	registerfunc(droptofloor);
	registerfunc(checkbottom);
	registerfunc(pointcontents);

	registerfunc(setspawnparms);
	registerfunc(changelevel);
	//registerfunc(LOGFRAG);
	registerfunc(getinfokey);
	registerfunc(setinfokey);
	registerfunc(multicast);
	registerfunc(writebyte);
	registerfunc(writechar);
	registerfunc(writeshort);
	registerfunc(writelong);
	registerfunc(writeangle);
	registerfunc(writecoord);
	registerfunc(writestring);
	registerfunc(writeentity);
	registerfunc(bitnot);
	registerfunc(bitclear);
	registerfunc(bitset);
	registerfunc(bitor);
	registerfunc(bitand);
	registerfunc(bitxor);
	registerfunc(sin);
	registerfunc(cos);
	registerfunc(atan2);
	registerfunc(sqrt);
	registerfunc(floor);
	registerfunc(ceil);
	registerfunc(acos);
	registerfunc(fopen);
	registerfunc(fclose);
	registerfunc(fgets);
	registerfunc(fputs);
	registerfunc(getfilelist);
	//registerfunc(Find);

	//registerfunc(strftime);
	registerfunc(tokenize);
	registerfunc(makevectors);
	registerfunc(vectoangles);

	//registerfunc(PRECACHE_VWEP_MODEL);
	//registerfunc(SETPAUSE);



	lua.createtable(L, 0, 0);
	if (lua.Lnewmetatable(L, "globals"))
	{
		lua.pushcclosure(L, my_lua_global_set, 0);	//for the luls.
		lua.setfield (L, -2, "__newindex");

		lua.pushcclosure(L, my_lua_global_get, 0);	//for the luls.
		lua.setfield (L, -2, "__index");
	}
	lua.setmetatable(L, -2);
	lua.setglobal(L, "glob");
}






static edict_t *QDECL Lua_EdictNum(pubprogfuncs_t *pf, unsigned int num)
{
	int newcount;
	if (num >= lua.maxedicts)
	{
		newcount = num + 64;
		lua.edicttable = realloc(lua.edicttable, newcount*sizeof(*lua.edicttable));
		while(lua.maxedicts < newcount)
			lua.edicttable[lua.maxedicts++] = NULL;
	}
	return lua.edicttable[num];
}
static unsigned int QDECL Lua_NumForEdict(pubprogfuncs_t *pf, edict_t *e)
{
	return e->entnum;
}
static int QDECL Lua_EdictToProgs(pubprogfuncs_t *pf, edict_t *e)
{
	return e->entnum;
}
static edict_t *QDECL Lua_ProgsToEdict(pubprogfuncs_t *pf, int num)
{
	return Lua_EdictNum(pf, num);
}
void Lua_EntClear (pubprogfuncs_t *pf, edict_t *e)
{
	int num = e->entnum;
	memset (e->v, 0, sv.world.edict_size);
	e->ereftype = ER_ENTITY;
	e->entnum = num;
}
edict_t *Lua_CreateEdict(unsigned int num)
{
	edict_t *e;
	e = lua.edicttable[num] = Z_Malloc(sizeof(edict_t) + sv.world.edict_size);
	e->v = (stdentvars_t*)(e+1);
#ifdef VM_Q1
	e->xv = (extentvars_t*)(e->v + 1);
#endif
	e->entnum = num;
	return e;
}
static void QDECL Lua_EntRemove(pubprogfuncs_t *pf, edict_t *e)
{
	lua_State *L = lua.ctx;

	if (!ED_CanFree(e))
		return;
	e->ereftype = ER_FREE;
	e->freetime = sv.time;

	//clear out the lua version of the entity, so that it can be garbage collected.
	//should probably clear out its entnum field too, just in case.
	lua.pushlightuserdata(L, e);
	lua.pushnil(L);
	lua.settable(L, LUA_REGISTRYINDEX);
}
static edict_t *Lua_DoRespawn(pubprogfuncs_t *pf, edict_t *e, int num)
{
	lua_State *L = lua.ctx;
	if (!e)
		e = Lua_CreateEdict(num);
	else
		Lua_EntClear (pf, e);

	ED_Spawned((struct edict_s *) e, false);

	//create a new table for the entity, give it a suitable metatable, and store it into the registry (avoiding GC and allowing us to actually hold on to it).
	lua.pushlightuserdata(L, lua.edicttable[num]);
	lua.createtable(L, 0, 0);
	if (lua.Lnewmetatable(L, "entity"))
	{
		lua.pushcclosure(L, my_lua_entity_set, 0);	//known writes should change the internal info so the engine can use the information.
		lua.setfield (L, -2, "__newindex");

		lua.pushcclosure(L, my_lua_entity_get, 0);	//we need to de-translate the engine's fields too.
		lua.setfield (L, -2, "__index");

		lua.pushcclosure(L, my_lua_entity_tostring, 0);	//cos its prettier than seeing 'table 0x5425729' all over the place
		lua.setfield (L, -2, "__tostring");

		lua.pushcclosure(L, my_lua_entity_eq, 0);	//for comparisons, you know?
		lua.setfield (L, -2, "__eq");
	}
	lua.setmetatable(L, -2);
	lua.pushinteger(L, num);
	lua.setfield (L, -2, "entnum");	//so we know which entity it is.
	lua.settable(L, LUA_REGISTRYINDEX);
	return e;
}
static edict_t *QDECL Lua_EntAlloc(pubprogfuncs_t *pf, pbool isobject, size_t extrasize)
{
	int i;
	edict_t *e;
	for ( i=0 ; i<sv.world.num_edicts ; i++)
	{
		e = (edict_t*)EDICT_NUM(pf, i);
		// the first couple seconds of server time can involve a lot of
		// freeing and allocating, so relax the replacement policy
		if (!e || (ED_ISFREE(e) && ( e->freetime < 2 || sv.time - e->freetime > 0.5 ) ))
		{
			e = Lua_DoRespawn(pf, e, i);
			return (struct edict_s *)e;
		}
	}

	if (i >= sv.world.max_edicts-1)	//try again, but use timed out ents.
	{
		for ( i=0 ; i<sv.world.num_edicts ; i++)
		{
			e = (edict_t*)EDICT_NUM(pf, i);
			// the first couple seconds of server time can involve a lot of
			// freeing and allocating, so relax the replacement policy
			if (!e || ED_ISFREE(e))
			{
				e = Lua_DoRespawn(pf, e, i);
				return (struct edict_s *)e;
			}
		}

		if (i >= sv.world.max_edicts-1)
		{
			Sys_Error ("ED_Alloc: no free edicts");
		}
	}

	sv.world.num_edicts++;
	e = (edict_t*)EDICT_NUM(pf, i);

	e = Lua_DoRespawn(pf, e, i);

	return (struct edict_s *)e;
}

static int QDECL Lua_LoadEnts(pubprogfuncs_t *pf, const char *mapstring, void *ctx, void (PDECL *callback) (pubprogfuncs_t *progfuncs, struct edict_s *ed, void *ctx, const char *entstart, const char *entend))
{
	lua_State *L = lua.ctx;
	int i = 0;
	lua.getglobal(L, "LoadEnts");
	lua.createtable(L, 0, 0);
	while(NULL != (mapstring = COM_Parse(mapstring)))
	{
		lua.pushinteger(L, i++);
		lua.pushstring(L, com_token);
		lua.settable(L, -3);
	}
//	lua.pushinteger(L, spawnflags);

	if (lua.pcall(L, 2, 0, 0) != 0)
	{
		const char *s = lua.tolstring(L, -1, NULL);
		Con_Printf(CON_WARNING "%s\n", s);
		lua.pop(L, 1);
	}

	return sv.world.edict_size;
}

static eval_t *QDECL Lua_GetEdictFieldValue(pubprogfuncs_t *pf, edict_t *e, char *fieldname, etype_t type, evalc_t *cache)
{
	eval_t *val;
	luafld_t *fld;
	fld = Hash_GetInsensitive(&lua.entityfields, fieldname);
	if (fld)
	{
		val = (eval_t*)((char*)e->v + fld->offset);
		return val;
	}
	return NULL;
}

static eval_t	*QDECL Lua_FindGlobal		(pubprogfuncs_t *prinst, const char *name, progsnum_t num, etype_t *type)
{
	eval_t *val;
	luafld_t *fld;
	fld = Hash_GetInsensitive(&lua.globalfields, name);
	if (fld)
	{
		val = (eval_t*)((char*)&lua.globals + fld->offset);
		return val;
	}

	Con_Printf("Lua_FindGlobal: %s\n", name);
	return NULL;
}
static func_t QDECL Lua_FindFunction		(pubprogfuncs_t *prinst, const char *name, progsnum_t num)
{
	eval_t *val;
	luafld_t *fld;
	fld = Hash_GetInsensitive(&lua.globalfields, name);
	if (fld)
	{
		val = (eval_t*)((char*)&lua.globals + fld->offset);
		return val->function;
	}

	Con_Printf("Lua_FindFunction: %s\n", name);
	return 0;
}

static globalvars_t *QDECL Lua_Globals(pubprogfuncs_t *prinst, int prnum)
{
//	Con_Printf("Lua_Globals: called\n");
	return NULL;
}

char *QDECL Lua_AddString(pubprogfuncs_t *prinst, const char *val, int minlength, pbool demarkup)
{
	char *ptr;
	int len = strlen(val)+1;
	if (len < minlength)
		len = minlength;
	ptr = Z_TagMalloc(len, 0x55780128);
	strcpy(ptr, val);
	return ptr;
}
static string_t QDECL Lua_StringToProgs(pubprogfuncs_t *prinst, const char *str)
{
	Con_Printf("Lua_StringToProgs called instead of Lua_SetStringField\n");
	return 0;
}

//passing NULL for ed means its setting a global.
static void QDECL Lua_SetStringField(pubprogfuncs_t *prinst, edict_t *ed, string_t *fld, const char *str, pbool str_is_static)
{
	lua_State *L = lua.ctx;
	string_t val;
	string_t base;
	if (ed)
	{
		base = (ed->entnum+1)<<10;
		val = (char*)fld-(char*)ed->v;

		//push the entity table
		lua.pushlightuserdata(lua.ctx, lua.edicttable[ed->entnum]);
		lua.gettable(lua.ctx, LUA_REGISTRYINDEX);
	}
	else
	{
		base = 0;
		val = (char*)fld-(char*)&lua.globals;

		//push the globals list
		lua.getglobal(lua.ctx, "glob");
	}
	*fld = base | val;	//set the engine's value

	//set the stuff so that lua can read it properly.
	lua.pushlightuserdata(L, (void *)(qintptr_t)val);
	lua.pushfstring(L, "%s", str);
	lua.rawset(L, -3);

	//and pop the table
	lua.pop(L, 1);
}

static const char *ASMCALL QDECL Lua_StringToNative(pubprogfuncs_t *prinst, string_t str)
{
	const char *ret = "";
	unsigned int entnum = str >> 10;
	if (str)
	{
		str &= 1023;
		if (!entnum)
		{
			//okay, its the global table.
			lua.getglobal(lua.ctx, "glob");
		}
		else
		{
			entnum-=1;
			if (entnum >= lua.maxedicts)
				return ret;	//erk...
			//get the entity's table
			lua.pushlightuserdata(lua.ctx, lua.edicttable[entnum]);
			lua.gettable(lua.ctx, LUA_REGISTRYINDEX);
		}

		//read the function from the table
		lua.pushlightuserdata(lua.ctx, (void *)(qintptr_t)str);
		lua.rawget(lua.ctx, -2);
		ret = lua.tolstring(lua.ctx, -1, NULL);
		lua.pop(lua.ctx, 2);	//pop the table+string.
		//popping the string is 'safe' on the understanding that the string reference is still held by its containing table, so don't store the string anywhere.
	}

	return ret;
}

static void Lua_Event_Touch(world_t *w, wedict_t *s, wedict_t *o)
{
	int oself = pr_global_struct->self;
	int oother = pr_global_struct->other;

	pr_global_struct->self = EDICT_TO_PROG(w->progs, s);
	pr_global_struct->other = EDICT_TO_PROG(w->progs, o);
	pr_global_struct->time = w->physicstime;
	PR_ExecuteProgram (w->progs, s->v->touch);

	pr_global_struct->self = oself;
	pr_global_struct->other = oother;
}

static void Lua_Event_Think(world_t *w, wedict_t *s)
{
	pr_global_struct->self = EDICT_TO_PROG(w->progs, s);
	pr_global_struct->other = EDICT_TO_PROG(w->progs, w->edicts);
	PR_ExecuteProgram (w->progs, s->v->think);
}

static qboolean Lua_Event_ContentsTransition(world_t *w, wedict_t *ent, int oldwatertype, int newwatertype)
{
	return false;	//always do legacy behaviour
}

static void Lua_SetupGlobals(world_t *world)
{
	int flds;
	int bucks;
	comentvars_t	*v = NULL;
	extentvars_t	*xv = (extentvars_t*)(v+1);

	memset(&lua.globals, 0, sizeof(lua.globals));
	lua.globals.physics_mode = 2;
	lua.globals.dimension_send = 255;
	lua.globals.dimension_default = 255;

	flds = 0;
	bucks = 64;
	Hash_InitTable(&lua.globalfields, bucks, Z_Malloc(Hash_BytesForBuckets(bucks)));

//WARNING: global is not remapped yet...
//This code is written evilly, but works well enough
#define doglobal(n, t)	\
		pr_global_ptrs->n = &lua.globals.n;	\
		lua.globflds[flds].offset = (char*)&lua.globals.n - (char*)&lua.globals;	\
		lua.globflds[flds].name = #n;		\
		lua.globflds[flds].type = t;		\
		Hash_AddInsensitive(&lua.globalfields, lua.globflds[flds].name, &lua.globflds[flds], &lua.globflds[flds].buck);	\
		flds++;
#define doglobal_v(o, f, t)	\
		lua.globflds[flds].offset = (char*)&lua.globals.o - (char*)&lua.globals;	\
		lua.globflds[flds].name = #f;		\
		lua.globflds[flds].type = t;		\
		Hash_AddInsensitive(&lua.globalfields, lua.globflds[flds].name, &lua.globflds[flds], &lua.globflds[flds].buck);	\
		flds++;
#define globalentity(required, name) doglobal(name, ev_entity)
#define globalint(required, name) doglobal(name, ev_integer)
#define globalfloat(required, name) doglobal(name, ev_float)
#define globalstring(required, name) doglobal(name, ev_string)
#define globalvec(required, name) doglobal(name, ev_vector) doglobal_v(name[0], name##_x, ev_float) doglobal_v(name[1], name##_y, ev_float) doglobal_v(name[2], name##_z, ev_float)
#define globalfunc(required, name) doglobal(name, ev_function)
	luagloballist
#undef doglobal
#define doglobal(n, t) doglobal_v(n,n,t)
	luaextragloballist

	flds = 0;
	bucks = 256;
	Hash_InitTable(&lua.entityfields, bucks, Z_Malloc(Hash_BytesForBuckets(bucks)));


#define doefield(n, t)	\
		lua.entflds[flds].offset = (char*)&v->n - (char*)v;	\
		lua.entflds[flds].name = #n;		\
		lua.entflds[flds].type = t;		\
		Hash_AddInsensitive(&lua.entityfields, lua.entflds[flds].name, &lua.entflds[flds], &lua.entflds[flds].buck);	\
		flds++;
#define doefield_v(o, f, t)	\
		lua.entflds[flds].offset = (char*)&v->o - (char*)v;	\
		lua.entflds[flds].name = #f;		\
		lua.entflds[flds].type = t;		\
		Hash_AddInsensitive(&lua.entityfields, lua.entflds[flds].name, &lua.entflds[flds], &lua.entflds[flds].buck);	\
		flds++;
#define comfieldentity(name,desc) doefield(name, ev_entity)
#define comfieldint(name,desc) doefield(name, ev_integer)
#define comfieldfloat(name,desc) doefield(name, ev_float)
#define comfieldstring(name,desc) doefield(name, ev_string)
#define comfieldvector(name,desc) doefield(name, ev_vector) doefield_v(name[0], name##_x, ev_float) doefield_v(name[1], name##_y, ev_float) doefield_v(name[2], name##_z, ev_float)
#define comfieldfunction(name,typestr,desc) doefield(name, ev_function)
	comqcfields
#undef doefield
#undef doefield_v
#define doefield(n, t)	\
		lua.entflds[flds].offset = (char*)&xv->n - (char*)v;	\
		lua.entflds[flds].name = #n;		\
		lua.entflds[flds].type = t;		\
		Hash_AddInsensitive(&lua.entityfields, lua.entflds[flds].name, &lua.entflds[flds], &lua.entflds[flds].buck);	\
		flds++;
#define doefield_v(o, f, t)	\
		lua.entflds[flds].offset = (char*)&xv->o - (char*)v;	\
		lua.entflds[flds].name = #f;		\
		lua.entflds[flds].type = t;		\
		Hash_AddInsensitive(&lua.entityfields, lua.entflds[flds].name, &lua.entflds[flds], &lua.entflds[flds].buck);	\
		flds++;
	comextqcfields
	svextqcfields

	PR_SV_FillWorldGlobals(world);
}

void QDECL Lua_ExecuteProgram(pubprogfuncs_t *funcs, func_t func)
{
	unsigned int entnum = func >> 10;
	func &= 1023;
	if (!entnum)
	{
		//okay, its the global table.
		lua.getglobal(lua.ctx, "glob");
	}
	else
	{
		entnum-=1;
		if (entnum >= lua.maxedicts)
			return;	//erk...
		//get the entity's table
		lua.pushlightuserdata(lua.ctx, lua.edicttable[entnum]);
		lua.gettable(lua.ctx, LUA_REGISTRYINDEX);
	}

	//read the function from the table
	lua.pushlightuserdata(lua.ctx, (void *)(qintptr_t)func);
	lua.rawget(lua.ctx, -2);

	//and now invoke it.
	if (lua.pcall(lua.ctx, 0, 0, 0) != 0)
	{
		const char *s = lua.tolstring(lua.ctx, -1, NULL);
		Con_Printf(CON_WARNING "%s\n", s);
		lua.pop(lua.ctx, 1);
	}
}

void PDECL Lua_CloseProgs(pubprogfuncs_t *inst)
{
	lua.close(lua.ctx);
	free(lua.edicttable);
	lua.edicttable = NULL;
	lua.maxedicts = 0;
}

qboolean PR_LoadLua(void)
{
	world_t *world = &sv.world;
	pubprogfuncs_t *pf;
	vfsfile_t *sourcefile = FS_OpenVFS("progs.lua", "rb", FS_GAME);
	if (!sourcefile)
		return false;

	if (!init_lua())
	{
		VFS_CLOSE(sourcefile);
		Con_Printf("WARNING: Found progs.lua, but could load load lua library\n");
		return false;
	}

	progstype = PROG_QW;


	pf = svprogfuncs = &lua.progfuncs;

	pf->CloseProgs = Lua_CloseProgs;
	pf->AddString = Lua_AddString;
	pf->EDICT_NUM = Lua_EdictNum;
	pf->NUM_FOR_EDICT = Lua_NumForEdict;
	pf->EdictToProgs = Lua_EdictToProgs;
	pf->ProgsToEdict = Lua_ProgsToEdict;
	pf->EntAlloc = Lua_EntAlloc;
	pf->EntFree = Lua_EntRemove;
	pf->EntClear = Lua_EntClear;
	pf->FindGlobal = Lua_FindGlobal;
	pf->load_ents = Lua_LoadEnts;
	pf->globals = Lua_Globals;
	pf->GetEdictFieldValue = Lua_GetEdictFieldValue;
	pf->SetStringField = Lua_SetStringField;
	pf->StringToProgs = Lua_StringToProgs;
	pf->StringToNative = Lua_StringToNative;
	pf->ExecuteProgram = Lua_ExecuteProgram;
	pf->FindFunction = Lua_FindFunction;

	world->Event_Touch = Lua_Event_Touch;
	world->Event_Think = Lua_Event_Think;
	world->Event_Sound = SVQ1_StartSound;
	world->Event_ContentsTransition = Lua_Event_ContentsTransition;
	world->Get_CModel = SVPR_GetCModel;

	world->progs = pf;
	world->progs->parms = &lua.progfuncsparms;
	world->progs->parms->user = world;
	world->usesolidcorpse = true;

	Lua_SetupGlobals(world);

	svs.numprogs = 0;	//Why is this svs?
#ifdef VM_Q1
	world->edict_size = sizeof(stdentvars_t) + sizeof(extentvars_t);
#else
	world->edict_size = sizeof(stdentvars_t);
#endif

	//force items2 instead of serverflags
	sv.haveitems2 = true;

	//initalise basic lua context
	lua.ctx = lua.newstate(my_lua_alloc, NULL);					//create our lua state
	my_lua_registerbuiltins(lua.ctx);

	//spawn the world, woo.
	world->edicts = (wedict_t*)pf->EntAlloc(pf,false,0);

	//load the gamecode now. it should be safe for it to call various builtins.
	if (0 != lua.load(lua.ctx, my_lua_Reader, sourcefile, "progs.lua", "bt"))	//load the file, embed it within a function and push it
	{
		Con_Printf("Error trying to parse %s: %s\n", "progs.lua", lua.tolstring(lua.ctx, -1, NULL));
		lua.pop(lua.ctx, 1);
	}
	else
	{
		if (lua.pcall(lua.ctx, 0, 0, 0) != 0)
		{
			const char *s = lua.tolstring(lua.ctx, -1, NULL);
			Con_Printf(CON_WARNING "%s\n", s);
			lua.pop(lua.ctx, 1);
		}
	}
	VFS_CLOSE(sourcefile);

	return true;
}
#endif	//VM_LUA
