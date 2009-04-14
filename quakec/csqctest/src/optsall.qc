//#define WORKINDP	//use workarounds for DP.
//#define FTEDEPENDANT	//explicitly depend upon FTE-only extensions (mostly prototype EXT_CSQC_1 features).
//#define OWNPLAYERPHYSICS	//run our own prediction code, instead of the engine-supplied default

#define MD3PMODELS	//support Q3 segmented player models
#define HLPMODELS		//support HalfLife skeletal models
#define POWERUP_SHELLS	//show shells around players for powerups

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
