//common builtins.

void(vector ang)	makevectors		= #1;		// sets v_forward, etc globals
void(entity e, vector o) setorigin	= #2;
void(entity e, string m) setmodel	= #3;		// set movetype and solid first
void(entity e, vector min, vector max) setsize = #4;

void() break						= #6;
float() random						= #7;		// returns 0 - 1
void(entity e, float chan, string samp, float vol, float atten) sound = #8;
void(entity e, float chan, string samp, float vol, float atten, float ppct) soundp = #8;
vector(vector v) normalize			= #9;
void(string e) error				= #10;
void(string e) objerror				= #11;
float(vector v) vlen				= #12;
float(vector v) vectoyaw			= #13;
entity() spawn						= #14;
void(entity e) remove				= #15;

// sets trace_* globals
// nomonsters can be:
// An entity will also be ignored for testing if forent == test,
// forent->owner == test, or test->owner == forent
// a forent of world is ignored
void(vector v1, vector v2, float nomonsters, entity forent) traceline = #16;


entity(entity start, .string fld, string match) find = #18;
string(string s) precache_sound		= #19;
float(string s) precache_model		= #20;
void(entity client, string s)stuffcmd = #21;
entity(vector org, float rad) findradius = #22;

void(string s, ...) dprint				= #25;
string(float f) ftos				= #26;
string(vector v) vtos				= #27;
void() coredump						= #28;		// prints all edicts
void() traceon						= #29;		// turns statment trace on
void() traceoff						= #30;
void(entity e) eprint				= #31;		// prints an entire edict

float() droptofloor= #34;	// TRUE if landed on floor

float(float v) rint					= #36;		// round to nearest int
float(float v) floor				= #37;		// largest integer <= v
float(float v) ceil					= #38;		// smallest integer >= v

float(vector v) pointcontents		= #41;		// returns a CONTENT_*

float(float f) fabs = #43;

float(string s) cvar = #45;						// return cvar.value
void(string s, ...) localcmd = #46;					// put string into local que
entity(entity e) nextent = #47;					// for looping through all ents
void(vector o, vector d, float color, float count) particle = #48;// start a particle effect
void() ChangeYaw = #49;						// turn towards self.ideal_yaw
											// at self.yaw_speed

vector(vector v) vectoangles			= #51;




void(string var, string val) cvar_set = #72;	// sets cvar.value



string(float modelindex) name_for_modelindex = #334;
float(string modelname) modelindex_for_name = #20;

string(string s) precache_file          = #68;  // no effect except for -copy
string(string s) precache_model2        = #75;          // registered version only
string(string s) precache_sound2        = #76;          // registered version only
string(string s) precache_file2         = #77;          // registered version only




