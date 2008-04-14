#ifndef MYSQLDLL_H
#define MYSQLDLL_H

#include <mysql.h>

HINSTANCE mysqldll;
int mysqldllerror;

#define MYSQLDLL_NOCONV

#define MYSQLDLL_LOADFUNC(funcname) \
	int loaded_##funcname = 0; \
	int funcload_##funcname() \
	{ \
		if (mysqldll) \
		{ \
			if (!loaded_##funcname) \
			{ \
				cb_##funcname = (DLLFUNC_##funcname)GetProcAddress(mysqldll, #funcname); \
				loaded_##funcname = 1; \
				mysqldllerror = GetLastError(); \
				\
			} \
			return (cb_##funcname != NULL); \
		} \
		return 0; \
	}

#define MYSQLDLL_FUNC0(returntype, funcname) \
	typedef returntype (CALLBACK *DLLFUNC_##funcname)(); \
	DLLFUNC_##funcname cb_##funcname; \
	MYSQLDLL_LOADFUNC(funcname) \
	returntype STDCALL funcname() \
	{ \
		if (funcload_##funcname()) \
			return cb_##funcname(); \
		return (returntype)0; \
	}

#define MYSQLDLL_FUNC1(returntype, funcname, paramtype1) \
	typedef returntype (CALLBACK *DLLFUNC_##funcname)(paramtype1); \
	DLLFUNC_##funcname cb_##funcname; \
	MYSQLDLL_LOADFUNC(funcname) \
	returntype STDCALL funcname(paramtype1 p1) \
	{ \
		if (funcload_##funcname()) \
			return cb_##funcname(p1); \
		return (returntype)0; \
	}

#define MYSQLDLL_FUNC2(returntype, funcname, paramtype1, paramtype2) \
	typedef returntype (CALLBACK *DLLFUNC_##funcname)(paramtype1, paramtype2); \
	DLLFUNC_##funcname cb_##funcname; \
	MYSQLDLL_LOADFUNC(funcname) \
	returntype STDCALL funcname(paramtype1 p1, paramtype2 p2) \
	{ \
		if (funcload_##funcname()) \
			return cb_##funcname(p1, p2); \
		return (returntype)0; \
	}

#define MYSQLDLL_FUNC3(returntype, funcname, paramtype1, paramtype2, paramtype3) \
	typedef returntype (CALLBACK *DLLFUNC_##funcname)(paramtype1, paramtype2, paramtype3); \
	DLLFUNC_##funcname cb_##funcname; \
	MYSQLDLL_LOADFUNC(funcname) \
	returntype STDCALL funcname(paramtype1 p1, paramtype2 p2, paramtype3 p3) \
	{ \
		if (funcload_##funcname()) \
			return cb_##funcname(p1, p2, p3); \
		return (returntype)0; \
	}

#define MYSQLDLL_FUNC4(returntype, funcname, paramtype1, paramtype2, paramtype3, paramtype4) \
	typedef returntype (CALLBACK *DLLFUNC_##funcname)(paramtype1, paramtype2, paramtype3, paramtype4); \
	DLLFUNC_##funcname cb_##funcname; \
	MYSQLDLL_LOADFUNC(funcname) \
	returntype STDCALL funcname(paramtype1 p1, paramtype2 p2, paramtype3 p3, paramtype4 p4) \
	{ \
		if (funcload_##funcname()) \
			return cb_##funcname(p1, p2, p3, p4); \
		return (returntype)0; \
	}

#define MYSQLDLL_FUNC8(returntype, funcname, paramtype1, paramtype2, paramtype3, paramtype4, paramtype5, paramtype6, paramtype7, paramtype8) \
	typedef returntype (CALLBACK *DLLFUNC_##funcname)(paramtype1, paramtype2, paramtype3, paramtype4, paramtype5, paramtype6, paramtype7, paramtype8); \
	DLLFUNC_##funcname cb_##funcname; \
	MYSQLDLL_LOADFUNC(funcname) \
	returntype STDCALL funcname(paramtype1 p1, paramtype2 p2, paramtype3 p3, paramtype4 p4, paramtype5 p5, paramtype6 p6, paramtype7 p7, paramtype8 p8) \
	{ \
		if (funcload_##funcname()) \
			return cb_##funcname(p1, p2, p3, p4, p5, p6, p7, p8); \
		return (returntype)0; \
	}

#define MYSQLDLL_NORETFUNC0(funcname) \
	typedef void (CALLBACK *DLLFUNC_##funcname)(); \
	DLLFUNC_##funcname cb_##funcname; \
	MYSQLDLL_LOADFUNC(funcname) \
	void STDCALL funcname() \
	{ \
		if (funcload_##funcname()) \
			cb_##funcname(); \
	}

#define MYSQLDLL_NORETFUNC1(funcname, paramtype1) \
	typedef void (CALLBACK *DLLFUNC_##funcname)(paramtype1); \
	DLLFUNC_##funcname cb_##funcname; \
	MYSQLDLL_LOADFUNC(funcname) \
	void STDCALL funcname(paramtype1 p1) \
	{ \
		if (funcload_##funcname()) \
			cb_##funcname(p1); \
	}

#define MYSQLDLL_NORETFUNC2(funcname, paramtype1, paramtype2) \
	typedef void (CALLBACK *DLLFUNC_##funcname)(paramtype1, paramtype2); \
	DLLFUNC_##funcname cb_##funcname; \
	MYSQLDLL_LOADFUNC(funcname) \
	void STDCALL funcname(paramtype1 p1, paramtype2 p2) \
	{ \
		if (funcload_##funcname()) \
			cb_##funcname(p1, p2); \
	}


// prototypes
int mysql_dll_init();
int mysql_dll_close();

#endif