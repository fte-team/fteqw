#ifdef SSQC //this is core for csqc
float(string s) checkextension = #99;
#endif

//DP_QC_TRACEBOX
void(vector v1, vector min, vector max, vector v2, float nomonsters, entity forent) tracebox = #90;

//DP_QC_REGISTERCVAR
void(string name, string value) registercvar = #93;

//DP_QC_FINDFLOAT
entity(entity start, .float fld, float match) findfloat = #98;
entity(entity start, .entity fld, entity match) findentity = #98;

//FRIK_FILE (file subset)
float FILE_READ = 0;
float FILE_WRITE = 1;
float FILE_APPEND = 2;
float(string filename, float mode) fopen = #110;
void(float fhandle) fclose = #111;
string(float fhandle) fgets = #112;
void(float fhandle, string s) fputs = #113;

//FRIK_FILE (strings subset)
float(string s) strlen = #114;
string(...) strcat = #115;
string(string s, float start, float length) substring = #116;
vector(string s) stov = #117;
string(string s) strzone = #118;
void(string s) strunzone = #119;
//QW_ENGINE FRIK_FILE
#ifndef QUAKEWORLD
float(string s) stof	= #81;
#endif


//KRIMZON_SV_PARSECLIENTCOMMAND (excluding clientcommand which is inherantly sv only)
float(string s) tokenize = #441;
string(float argnum) argv = #442;

//DP_QC_SINCOSSQRTPOW
//idea: id Software, LordHavoc
//darkplaces implementation: id Software, LordHavoc
//builtin definitions:
float(float val) sin = #60;
float(float val) cos = #61;
float(float val) sqrt = #62;
float(float a, float b) pow = #97;
//description:
//useful math functions, sine of val, cosine of val, square root of val, and raise a to power b, respectively.

//FTE_STRINGS
float(string str, string match) strstrofs = #221;
float(string str, float ofs) str2chr = #222;
string(float c, ...) chr2str = #223;
string(float ccase, float redalpha, float redchars, string str) strconv = #224;
string(string info, string key, string value) infoadd = #226;
string(string info, string key) infoget = #227;
float(string s1, string s2, float len) strncmp = #228;
float(string s1, string s2) strcasecmp = #229;
float(string s1, string s2, float len) strncasecmp = #230;

//DP_QC_FS_SEARCH
//idea: Black
//darkplaces implementation: Black
//builtin definitions:
float(string pattern, float caseinsensitive, float quiet) search_begin = #444;
void(float handle) search_end = #445;
float(float handle) search_getsize = #446;
string(float handle, float num) search_getfilename = #447;
//description:
//search_begin performs a filename search with the specified pattern (for example "maps/*.bsp") and stores the results in a search slot (minimum of 128 supported by any engine with this extension), the other functions take this returned search slot number, be sure to search_free when done (they are also freed on progs reload).
//search_end frees a search slot (also done at progs reload).
//search_getsize returns how many filenames were found.
//search_getfilename returns a filename from the search.

//DP_QC_CVAR_STRING
//idea: Paul Timofeyev
//DarkPlaces implementation: Paul Timofeyev, LordHavoc
//builtin definitions:
string(string s) cvar_string = #448;
//description:
//returns the value of a cvar, as a tempstring.



//This file contains optional client-specific extensions which may not exist in all CSQC implementations.

//UNNAMED_FTE_CS_SKINS_AND_SHADERS
#ifdef CSQC
float(float modelindex, string skinname) skinforname = #237;
float(string skinname) shaderforname = #238;
float(float midx, string framename) frameforname = #275;
#endif

//UNNAMED_FTE_CS_TAG_SUPPORT
#ifdef CSQC
vector(entity ent, float tagnum) RotateVectorsByTag = #234;
void(vector angle) RotateVectorsByAngle = #235;
void(vector fwd, vector right, vector up) RotateVectorsByVectors = #236;
#endif

//DP_MD3_TAGSINFO
float(entity ent, string tagname) gettagindex = #451;
vector(entity ent, float tagindex) gettaginfo = #452;



//DP_GECKO_SUPPORT
#if defined(CSQC) || defined(MENU)
//idea: Res2k, BlackHC
//darkplaces implementation: Res2k, BlackHC
//FTE implementation: Spike (curtesy of liboffscreengecko)
//constant definitions:
float GECKO_BUTTON_DOWN         = 0;
float GECKO_BUTTON_UP           = 1;
// either use down and up or just press but not all of them!
float GECKO_BUTTON_PRESS        = 2;
// use this for mouse events if needed?
float GECKO_BUTTON_DOUBLECLICK  = 3;
//builtin definitions:
float(string name) gecko_create( string name ) = #487;
void(string name) gecko_destroy( string name ) = #488;
void(string name) gecko_navigate( string name, string URI ) = #489;
float(string name) gecko_keyevent( string name, float key, float eventtype ) = #490;
void gecko_mousemove( string name, float x, float y ) = #491;
void gecko_resize( string name, float w, float h ) = #492;
vector gecko_get_texture_extent( string name ) = #493;
//engine-called QC prototypes:
//string(string name, string query) Qecko_Query;
//description:
//provides an interface to the offscreengecko library and allows for internet browsing in games
#else
#warning no gecko
#endif


#ifdef SSQC
.float dimension_seen;
.float dimension_see;
float dimension_send;
#endif

//DP_TE_STANDARDEFFECTBUILTINS)
void(vector org) te_gunshot = #418;
void(vector org) te_spike = #419;
void(vector org) te_superspike = #420;
void(vector org) te_explosion = #421;
void(vector org) te_tarexplosion = #420;
void(vector org) te_wizspike = #423;
void(vector org) te_knightspike = #424;
void(vector org) te_lavasplash = #425;
void(vector org) te_teleport = #426;
void(vector org, float colorstart, float colorlength) te_explosion2 = #427;
void(entity own, vector start, vector end) te_lightning1 = #428;
void(entity own, vector start, vector end) te_lightning2 = #429;
void(entity own, vector start, vector end) te_lightning3 = #430;
void(entity own, vector start, vector end) te_beam = #431;

//DP_QC_WHICHPACK
string(string filename) whichpack = #503;
