//server only builtins.

entity() checkclient				= #17;	// returns a client to look for

void(string s) bprint				= #23;
void(entity client, string s) sprint = #24;

float(float yaw, float dist) walkmove	= #32;	// returns TRUE or FALSE
void(float style, string value) lightstyle = #35;
float(entity e) checkbottom			= #40;		// true if self is on ground

vector(entity e, float speed) aim = #44;		// returns the shooting vector


//
// direct client message generation
//
void(float to, float f) WriteByte		= #52;
void(float to, float f) WriteChar		= #53;
void(float to, float f) WriteShort		= #54;
void(float to, float f) WriteLong		= #55;
void(float to, float f) WriteCoord		= #56;
void(float to, float f) WriteAngle		= #57;
void(float to, string s) WriteString	= #58;
void(float to, entity s) WriteEntity	= #59;

void(float step) movetogoal				= #67;

void(entity e) makestatic		= #69;



void(entity client, string s) centerprint = #73;	// sprint, but in middle
void(vector pos, string samp, float vol, float atten) ambientsound = #74;
void(string s) changelevel = #70;

void(entity e) setspawnparms		= #78;		// set parm1... to the
												// values at level start
												// for coop respawn




