
// Engine support code (ENG) --
// Checks to see what extensions are supported by current running engine and
// has wrappers for builtins based on engine (other than EF_/TE_/viewkick/flash effects)

var float chan_no_phs_add = 0;

INTEGER eng_support;
#define ENG_STOF       0x01
#define ENG_CVARSTRING 0x02
#define ENG_TEBUILTINS 0x04
#define ENG_TEBLOOD    0x08
#define ENG_EFRED      0x10
#define ENG_EFBLUE     0x20

// prototypes
void() EFF_SetEffects;

// builtins
float(string s) cvar = #45; 
void(vector o, vector d, float color, float count) particle = #48; // NQ only
void(entity killer, entity killee) _logfrag = #79; // QW only
string(entity e, string key) _infokey = #80; // QW only
void(vector where, float set) multicast = #82;  // QW only
float(string s) checkextension = #99; // QSG extension
void(vector org, vector velocity, float howmany) te_blood = #405; // DP_TE_BLOOD
void(vector org) _te_gunshot = #418; // DP_TE_STANDARDEFFECTBUILTINS
void(vector org) _te_spike = #419; // DP_TE_STANDARDEFFECTBUILTINS
void(vector org) _te_superspike = #420; // DP_TE_STANDARDEFFECTBUILTINS
void(vector org) _te_explosion = #421; // DP_TE_STANDARDEFFECTBUILTINS
void(vector org) _te_tarexplosion = #422; // DP_TE_STANDARDEFFECTBUILTINS
void(vector org) _te_wizspike = #423; // DP_TE_STANDARDEFFECTBUILTINS
void(vector org) _te_knightspike = #424; // DP_TE_STANDARDEFFECTBUILTINS
void(vector org) _te_lavasplash = #425; // DP_TE_STANDARDEFFECTBUILTINS
void(vector org) _te_teleport = #426; // DP_TE_STANDARDEFFECTBUILTINS
void(vector org, float color, float colorlength) _te_explosion2 = #427; // DP_TE_STANDARDEFFECTBUILTINS
void(entity own, vector start, vector end) _te_lightning1 = #428; // DP_TE_STANDARDEFFECTBUILTINS
void(entity own, vector start, vector end) _te_lightning2 = #429; // DP_TE_STANDARDEFFECTBUILTINS
void(entity own, vector start, vector end) _te_lightning3 = #430; // DP_TE_STANDARDEFFECTBUILTINS
void(entity own, vector start, vector end) _te_beam = #431; // DP_TE_STANDARDEFFECTBUILTINS
string(string s) cvar_string = #448; // DP_CVAR_STRING

// wrappers
// print wrapper
#ifdef NETQUAKE
void(...) _bprint = #23; // NQ
void(entity client, ...) _sprint = #24; // NQ
#define bprint1(l,a) _bprint(a)
#define bprint2(l,a,b) _bprint(a,b)
#define bprint3(l,a,b,c) _bprint(a,b,c)
#define bprint4(l,a,b,c,d) _bprint(a,b,c,d)
#define bprint5(l,a,b,c,d,e) _bprint(a,b,c,d,e)
#define bprint6(l,a,b,c,d,e,f) _bprint(a,b,c,d,e,f)
#define bprint7(l,a,b,c,d,e,f,g) _bprint(a,b,c,d,e,f,g)

#define sprint1(t,l,a) _sprint(t,a)
#define sprint2(t,l,a,b) _sprint(t,a,b)
#define sprint3(t,l,a,b,c) _sprint(t,a,b,c)
#define sprint4(t,l,a,b,c,d) _sprint(t,a,b,c,d)
#define sprint5(t,l,a,b,c,d,e) _sprint(t,a,b,c,d,e)
#define sprint6(t,l,a,b,c,d,e,f) _sprint(t,a,b,c,d,e,f)
#else
void(float level, ...) _bprint = #23; // QW
void(entity client, float level, ...) _sprint = #24; // QW
#define bprint1(l,a) _bprint(l,a)
#define bprint2(l,a,b) _bprint(l,a,b)
#define bprint3(l,a,b,c) _bprint(l,a,b,c)
#define bprint4(l,a,b,c,d) _bprint(l,a,b,c,d)
#define bprint5(l,a,b,c,d,e) _bprint(l,a,b,c,d,e)
#define bprint6(l,a,b,c,d,e,f) _bprint(l,a,b,c,d,e,f)
#define bprint7(l,a,b,c,d,e,f,g) _bprint(l,a,b,c,d,e,f,g)

#define sprint1(t,l,a) _sprint(t,l,a)
#define sprint2(t,l,a,b) _sprint(t,l,a,b)
#define sprint3(t,l,a,b,c) _sprint(t,l,a,b,c)
#define sprint4(t,l,a,b,c,d) _sprint(t,l,a,b,c,d)
#define sprint5(t,l,a,b,c,d,e) _sprint(t,l,a,b,c,d,e)
#define sprint6(t,l,a,b,c,d,e,f) _sprint(t,l,a,b,c,d,e,f)
#endif

// logfrag wrapper
#ifdef NETQUAKE
// empty define to strip logfrag out
#define logfrag(x,y)
#else
#define logfrag(x,y) _logfrag(x,y)
#endif

// stof
float(string s) _stof = #81;
float(string s) _stofdummy =
{
	return 0;
}

var float(string s) stof = _stofdummy;

// infokey wrapper
#ifdef NETQUAKE
#define numberserverinfokey(x) cvar(x)
string(string cv) stringserverinfokey =
{
	if (eng_support & ENG_CVARSTRING)
		return cvar_string(cv);

	return "";
}

#define numberclientinfokey(x,y) 0
#define stringclientinfokey(x,y) ""
#else
#define numberserverinfokey(x) stof(_infokey(world,x))
#define stringserverinfokey(x) _infokey(world,x)
#define numberclientinfokey(x,y) stof(_infokey(x,y))
#define stringclientinfokey(x,y) _infokey(x,y)
#endif

// cdtrack wrapper
void(float track, float looptrack) ENG_SwitchTrack =
{
	WriteByte (MSG_ALL, SVC_CDTRACK);
	WriteByte (MSG_ALL, track);
#ifdef NETQUAKE
	// only NetQuake supports loop track
	WriteByte (MSG_ALL, looptrack);	
#endif
};

// engine check routine
void() ENG_Check =
{
#ifndef NETQUAKE
	eng_support = ENG_STOF | ENG_EFRED | ENG_EFBLUE;
	chan_no_phs_add = 8;
#endif

	if (!cvar("pr_checkextension"))
		return; // no extensions supported

	if (checkextension("FRIK_FILE"))
		eng_support |= ENG_STOF;

	if (checkextension("DP_CVAR_STRING"))
		eng_support |= ENG_CVARSTRING;

	if (checkextension("DP_TE_STANDARDEFFECTBUILTINS"))
		eng_support |= ENG_TEBUILTINS;

	if (checkextension("DP_TE_BLOOD"))
		eng_support |= ENG_TEBLOOD;

	if (checkextension("DP_EF_RED"))
		eng_support |= ENG_EFRED;

	if (checkextension("DP_EF_BLUE"))
		eng_support |= ENG_EFBLUE;

	// check and set stof extension
	if (eng_support & ENG_STOF)
		stof = _stof;

	EFF_SetEffects();
};

