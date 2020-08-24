//#define WORKINDP	//use workarounds for DP.
#define QSS
//#define FTEDEPENDANT	//explicitly depend upon FTE-only extensions (mostly prototype EXT_CSQC_1 features).
#define OWNPLAYERPHYSICS	//run our own prediction code, instead of the engine-supplied default

#define AUTOCVAR

#define MD3PMODELS	//support Q3 segmented player models
#define HLPMODELS		//support HalfLife skeletal models
#define POWERUP_SHELLS	//show shells around players for powerups
#define Q4PMODELS
//#define NOEXTENSIONS

#ifdef NOEXTENSIONS
#undef HLPMODELS
#undef MD3PMODELS
#endif

#ifdef WORKINDP
# warning "WORKINDP defined: Mod will only work properly in DP"
# ifdef FTEDEPENDANT
#  undef FTEDEPENDANT
# endif
#endif

#ifdef FTEDEPENDANT
# warning "FTEDEPENDANT defined: Mod will only work properly in FTE"
# pragma TARGET FTE
#endif























#ifdef AUTOCVAR
#define DEFCVAR_FLOAT(n,d) var float autocvar_##n = d;
#define DEFCVAR_STRING(n,d) var string autocvar_##n = d;
#define CVARF(n) autocvar_##n
#define CVARS(n) autocvar_##n
#else
#define DEFCVAR_FLOAT(n,d)
#define DEFCVAR_STRING(n,d)
#define CVARF(n) cvar(#n)
#define CVARS(n) cvar_string(#n)
#endif
