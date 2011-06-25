
//csqc only builtins.

float(string s) checkextension = #99;	//this is core for csqc, but still an extension for ssqc


void() clearscene			= #300; // (EXT_CSQC)
void(float mask) addentities		= #301; //  (EXT_CSQC)
void(entity ent) addentity		= #302; //  (EXT_CSQC)
float(float property, ...) setviewprop	= #303; //  (EXT_CSQC)
void() renderscene			= #304; //  (EXT_CSQC)
float(float property) getviewpropf	= #309; //  (EXT_CSQC)
vector(float property) getviewpropv	= #309; //  (EXT_CSQC)

void(vector org, float radius, vector lightcolours) adddynamiclight		= #305; //  (EXT_CSQC)

//void(string texturename) R_BeginPolygon	= #306; //  (EXT_CSQC_???)
//void(vector org, vector texcoords, vector rgb, float alpha) R_PolygonVertex	= #307; //  (EXT_CSQC_???)
//void() R_EndPolygon 			= #308; //  (EXT_CSQC_???)

//maths stuff that uses the current view settings.
vector (vector v) unproject 		= #310; //  (EXT_CSQC)
vector (vector v) project 		= #311; //  (EXT_CSQC)

//2d (immediate) operations
void(float width, vector pos1, vector pos2) drawline = #315; //  (EXT_CSQC)
float(string name) iscachedpic 			= #316; //  (EXT_CSQC)
string(string name, float trywad) precache_pic 	= #317; //  (EXT_CSQC)
vector(string picname) draw_getimagesize 	= #318; //  (EXT_CSQC)
void(string name) freepic 			= #319; //  (EXT_CSQC)
void(vector position, float character, vector scale, vector rgb, float alpha, ...) drawcharacter	= #320; //  (EXT_CSQC, [EXT_CSQC_???])
void(vector position, string text, vector scale, vector rgb, float alpha, ...) drawrawstring		= #321; //  (EXT_CSQC, [EXT_CSQC_???])
void(vector position, string pic, vector size, vector rgb, float alpha, ...) drawpic			= #322; //  (EXT_CSQC, [EXT_CSQC_???])
void(vector position, vector size, vector rgb, float alpha, ...) drawfill				= #323; //  (EXT_CSQC, [EXT_CSQC_???])

void(vector position, string text, vector scale, float alpha, ...) drawstring		= #326; //  (EXT_CSQC_1)

float(float stnum) getstati							= #330; //  (EXT_CSQC)
float(float stnum) getstatf							= #331; //  (EXT_CSQC)
float(float stnum, float first, float count) getstatbits			= #331; //  (EXT_CSQC)
string(float firststnum) getstats						= #332; //  (EXT_CSQC)
void(entity e, float mdlindex) setmodelindex					= #333; //  (EXT_CSQC)
string(float mdlindex) modelnameforindex 					= #334; //  (EXT_CSQC)

float(string effectname) particleeffectforname					= #335; //  (EXT_CSQC)
#ifdef WORKINDP
//DP got the arguments the wrong way around...
void(entity ent, float effectnum, vector start, vector end) trailparticles_dp	= #336; //  (EXT_CSQC),
void(float effectnum, entity ent, vector start, vector end) trailparticles =
{
	trailparticles_dp(ent, effectnum, start, end);
};
#else
void(float effectnum, entity ent, vector start, vector end) trailparticles	= #336; //  (EXT_CSQC),
#endif
void(float effectnum, vector origin, ...) pointparticles			= #337; //  (EXT_CSQC)

void(string s, ...) cprint		= #338; //  (EXT_CSQC)
void(string s, ...) print		= #339; //  (EXT_CSQC)

string(float keynum) keynumtostring	= #340; //  (EXT_CSQC)
float(string keyname) stringtokeynum	= #341; //  (EXT_CSQC)
string(float keynum) getkeybind		= #342; //  (EXT_CSQC)

float(float framenum) getinputstate	= #345; //  (EXT_CSQC)
void(float sens) setsensitivityscaler	= #346; //  (EXT_CSQC)

void(entity ent) runstandardplayerphysics		= #347; //  (EXT_CSQC_1)

string(float playernum, string keyname) getplayerkeyvalue			= #348; //  (EXT_CSQC)

float() isdemo				= #349; //  (EXT_CSQC)
float() isserver			= #350; //  (EXT_CSQC)

void(vector origin, vector forward, vector right, vector up) SetListener	= #351; //  (EXT_CSQC)
void(string cmdname) registercommand						= #352; //  (EXT_CSQC)
float(entity ent) wasfreed							= #353; //  (EXT_CSQC) (should be availabe on server too)

//note that 'ReadEntity' is pretty hard to implement reliably. Modders should use a combination of ReadShort, and findfloat, and remember that it might not be known clientside (pvs culled or other reason)
float() readbyte			= #360; //  (EXT_CSQC)
float() readchar			= #361; //  (EXT_CSQC)
float() readshort			= #362; //  (EXT_CSQC)
float() readlong			= #363; //  (EXT_CSQC)
float() readcoord			= #364; //  (EXT_CSQC)
float() readangle			= #365; //  (EXT_CSQC)
string() readstring			= #366; //  (EXT_CSQC)
string() readfloat			= #367; //  (EXT_CSQC)

void(string model, void(float isnew) updatefunc, float flags) deltalisten = #371;
//void(float flags, float simtime) readserverentitystate = #369;
#define RSES_NOLERP 1	//only really for use on predicted player entities
#define RSES_AUTOROTATE 0
#define RSES_NOROTATE 2
#define RSES_AUTOTRAILS 0
#define RSES_NOTRAILS 4
#define RSES_AUTOLIGHTS 0
#define RSES_NOLIGHTS 8

void(string evname, string argdesc, ...) sendevent = #359;
