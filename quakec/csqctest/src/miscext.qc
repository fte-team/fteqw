//definitions that id Software left out
//these are passed as the 'nomonsters' parameter to traceline/tracebox (yes really this was supported in all quake engines, nomonsters is misnamed)
float MOVE_NORMAL = 0; // same as FALSE
float MOVE_NOMONSTERS = 1; // same as TRUE
float MOVE_MISSILE = 2; // save as movement with .movetype == MOVETYPE_FLYMISSILE

//checkextension function
//idea: expected by almost everyone
//darkplaces implementation: LordHavoc
float(string s) checkextension = #99;
//description:
//check if (cvar("pr_checkextension")) before calling this, this is the only
//guarenteed extension to be present in the extension system, it allows you
//to check if an extension is available, by name, to check for an extension
//use code like this:
//// (it is recommended this code be placed in worldspawn or a worldspawn called function somewhere)
//if (cvar("pr_checkextension"))
//if (checkextension("DP_SV_SETCOLOR"))
//	ext_setcolor = TRUE;
//from then on you can check ext_setcolor to know if that extension is available







//EXT_BITSHIFT
//IDEA: Spike
float(float number, float quantity) bitshift = #218;
//Shifts 'number' 'quantity' bits to the left. So a shift of 1 bit essentually doubles the value while a shift of -1 bit halves. 2 quadrouples and so on.
//The function acts on floats, but casts as integers. The maximum reliable bit stored in a float is 1e23. Adding a twenty fourth bit is possible, but can result in some of the lower bits becoming corrupted.

//EXT_DIMENSION_GHOST
//IDEA: ?????FIXME?????
.float dimension_ghost;
.float dimension_ghost_alpha;
//depends on EXT_DIMENSION_VISIBLE
//provides an alternate mask for entities to see this one.
//entities seen with this mask and not the dimension_seen mask will have thier alpha value multiplied by thier dimension_ghost_alpha.
//they default to 255 upon entity creation.

//EXT_DIMENSION_PHYSICS
//IDEA: ?????FIXME?????, Spike
.float dimension_hit;
.float dimension_solid;
//Controls which entities are allowed to collide with other entities.
//dimension_solid is the mask of dimensions in which an entity is solid.
//dimension_hit is the mask of dimensions which will not pass through dimension_solid.
//most of the time they will be the same.
//if one entity walks into annother, they must be able to ignore the other even if thier masks say not.
//they default to 255 upon entity creation.

//EXT_DIMENSION_VISIBLE
//IDEA: ?????FIXME?????, Spike
.float dimension_see;
.float dimension_seen;
float  dimension_send;
//Controls which players see which entities/players.
//You always see yourself.
//the dimension_seen mask is the mask of dimensions which an entity can be seen, while dimension_see is which dimensions an entity can see into.
//dimension_send is set around the sound and various particle effect builtins, and will be sent only to players who's dimension_see matches.
//note that this can be used for sound directed at a single player, per-class pickups, and other cool stuff.
//they default to 255 upon entity creation - note that saved games will need to be fixed-up if there are no values set on any entity after load.

//FRIK_FILE
//idea: FrikaC
//darkplaces implementation: LordHavoc
//builtin definitions:
float(string s) stof = #81; // get numerical value from a string
float(string filename, float mode) fopen = #110; // opens a file inside quake/gamedir/data/ (mode is FILE_READ, FILE_APPEND, or FILE_WRITE), returns fhandle >= 0 if successful, or fhandle < 0 if unable to open file for any reason
void(float fhandle) fclose = #111; // closes a file
string(float fhandle) fgets = #112; // reads a line of text from the file and returns as a tempstring
void(float fhandle, string s) fputs = #113; // writes a line of text to the end of the file
float(string s) strlen = #114; // returns how many characters are in a string
string(string s1, string s2) strcat = #115; // concatenates two strings (for example "abc", "def" would return "abcdef") and returns as a tempstring
string(string s, float start, float length) substring = #116; // returns a section of a string as a tempstring
vector(string s) stov = #117; // returns vector value from a string
string(string s) strzone = #118; // makes a copy of a string into the string zone and returns it, this is often used to keep around a tempstring for longer periods of time (tempstrings are replaced often)
void(string s) strunzone = #119; // removes a copy of a string from the string zone (you can not use that string again or it may crash!!!)
//constants:
float FILE_READ = 0;
float FILE_APPEND = 1;
float FILE_WRITE = 2;
//cvars:
//pr_zone_min_strings : default 64 (64k), min 64 (64k), max 8192 (8mb)
//description:
//provides text file access functions and string manipulation functions, note that you may want to set pr_zone_min_strings in the worldspawn function if 64k is not enough string zone space.

//KRIMZON_SV_PARSECLIENTCOMMAND
//idea: KrimZon
//darkplaces implementation: KrimZon, LordHavoc
//engine-called QC prototypes:
//void(string s) SV_ParseClientCommand;
//builtin definitions:
void(entity e, string s) clientcommand = #440;
float(string s) tokenize = #441;
string(float n) argv = #442;
//description:
//provides QC the ability to completely control server interpretation of client commands ("say" and "color" for example, clientcommand is necessary for this and substring (FRIK_FILE) is useful) as well as adding new commands (tokenize, argv, and stof (FRIK_FILE) are useful for this)), whenever a clc_stringcmd is received the QC function is called, and it is up to the QC to decide what (if anything) to do with it

//NEH_CMD_PLAY2
//idea: Nehahra
//darkplaces implementation: LordHavoc
//description:
//shows that the engine supports the "play2" console command (plays a sound without spatialization).

//NEH_RESTOREGAME
//idea: Nehahra
//darkplaces implementation: LordHavoc
//engine-called QC prototypes:
//void() RestoreGame;
//description:
//when a savegame is loaded, this function is called

//NEXUIZ_PLAYERMODEL
//idea: Nexuiz
//darkplaces implementation: Black
//console commands:
//playermodel <name> - FIXME: EXAMPLE NEEDED
//playerskin <name> - FIXME: EXAMPLE NEEDED
//field definitions:
.string playermodel; // name of player model sent by client
.string playerskin; // name of player skin sent by client
//description:
//these client properties are used by Nexuiz.

//NXQ_GFX_LETTERBOX
//idea: nxQuake
//darkplaces implementation: LordHavoc
//description:
//shows that the engine supports the "r_letterbox" console variable, set to values in the range 0-100 this restricts the view vertically (and turns off sbar and crosshair), value is a 0-100 percentage of how much to constrict the view, <=0 = normal view height, 25 = 75% of normal view height, 50 = 50%, 75 = 25%, >=100 = no view

//PRYDON_CLIENTCURSOR
//idea: FrikaC
//darkplaces implementation: LordHavoc
//effects bit:
float EF_SELECTABLE = 16384; // allows cursor to highlight entity (brighten)
//field definitions:
.float cursor_active; // true if cl_prydoncursor mode is on
.vector cursor_screen; // screen position of cursor as -1 to +1 in _x and _y (_z unused)
.vector cursor_trace_start; // position of camera
.vector cursor_trace_endpos; // position of cursor in world (as traced from camera)
.entity cursor_trace_ent; // entity the cursor is pointing at (server forces this to world if the entity is currently free at time of receipt)
//cvar definitions:
//cl_prydoncursor (0/1+, default 0, 1 and above use cursors named gfx/prydoncursor%03i.lmp - or .tga and such if DP_GFX_EXTERNALTEXTURES is implemented)
//description:
//shows that the engine supports the cl_prydoncursor cvar, this puts a clientside mouse pointer on the screen and feeds input to the server for the QuakeC to use as it sees fit.
//the mouse pointer triggers button4 if cursor is at left edge of screen, button5 if at right edge of screen, button6 if at top edge of screen, button7 if at bottom edge of screen.
//the clientside trace skips transparent entities (except those marked EF_SELECTABLE).
//the selected entity highlights only if EF_SELECTABLE is set, a typical selection method would be doubling the brightness of the entity by some means (such as colormod[] *= 2).
//intended to be used by Prydon Gate.

//QSG_FILE
//IDEA: ?????FIXME?????
//Same as FRIK_FILE, but definitions numbers aren't the same.

//TENEBRAE_GFX_DLIGHTS
//idea: Tenebrae
//darkplaces implementation: LordHavoc
//fields:
.float light_lev; // radius (does not affect brightness), typical value 350
.vector color; // color (does not affect radius), typical value '1 1 1' (bright white), can be up to '255 255 255' (nuclear blast)
.float style; // light style (like normal light entities, flickering torches or switchable, etc)
.float pflags; // flags (see PFLAGS_ constants)
.vector angles; // orientation of the light
.float skin; // cubemap filter number, can be 1-255 (0 is assumed to be none, and tenebrae only allows 16-255), this selects a projective light filter, a value of 1 loads cubemaps/1posx.tga and cubemaps/1negx.tga and posy, negy, posz, and negz, similar to skybox but some sides need to be rotated or flipped
//constants:
float PFLAGS_NOSHADOW = 1; // light does not cast shadows
float PFLAGS_CORONA = 2; // light has a corona flare
float PFLAGS_FULLDYNAMIC = 128; // light enable (without this set no light is produced!)
//description:
//more powerful dynamic light settings
//warning: it is best not to use cubemaps on a light entity that has a model, as using a skin number that a model does not have will cause issues in glquake, and produce warnings in darkplaces (use developer 1 to see them)
//changes compared to tenebrae (because they're too 'leet' for standards):
//EF_FULLDYNAMIC effects flag replaced by PFLAGS_FULLDYNAMIC flag (EF_FULLDYNAMIC conflicts with EF_NODRAW)

//TEI_MOVRELATIVESTATIC
//IDEA: Tei
//movetype definitions:
float MOVETYPE_RELATIVESTATIC = 12;
//Same as MOVETYPE_FOLLOW but it doesn't take owner's angles into account
//e.g. entity will not rotate around owner.

//TEI_SHOWLMP2
//IDEA: Tei
void (string slot, string picfile, float x, float y, float org, entity to) ShowPicEnt = #108; //Show pic to player
void (string slot, entity to) HidePicEnt                                              = #109; //Hide pic from player
//Shows/hides pics to/from the player.
//You can do fantastic huds with it, but don't forget it's better to use it only in singleplayer.
//Cause netplay will suck.

//TQ_DLIGHTS
//IDEA: Tomaz
//field definition:
.float glow_size;
.float glow_red; 
.float glow_green;
.float glow_blue;
//Customizable glowing light effect on the entity, glow_red, glow_green and glow_blue is the color in the range 0-1. 
//glow_size is is the size of the glow (duh) in the range 0-1024.

//TQ_PLASMA
//IDEA: Tomaz
//temp entity definitions:
float TE_PLASMA = 16;
//New weapon effect.

//TQ_RAILTRAIL
//IDEA: id Software, code by TerribleOne modified by Tomaz
//temp entity definitions:
float TE_RAILTRAIL = 17;
//New weapontrail effect.

//TQ_SLOWMO
//IDEA: Tomaz
//Same as DP_SV_SLOWMO

//TW_SV_STEPCONTROL
//idea: Transfusion
//darkplaces implementation: LordHavoc
//cvars:
//sv_jumpstep (0/1, default 1)
//sv_stepheight (default 18)
//description:
//sv_jumpstep allows stepping up onto stairs while airborn, sv_stepheight controls how high a single step can be.






//ZQ_QC_CHECKBUILTIN
//IDEA: Tonik
float(float num, ...)          checkbuiltin      = #23040;
float(float first, float last) checkbuiltinrange = #23041;
//ZQuake (and Betwix :) provides a way to check for the presence of a QC extension by builtin number rather than by name. E.g. to see if the engine supports DP_QC_TRACEBOX (builtin #90), you can use
//   if (checkbuiltin(90)) {...}
//instead of
//   if (checkextension("DP_QC_TRACEBOX")) {...}
//Several (up to 8) extension numbers may be checked in one call to checkbuiltin(). TRUE will be returned if all of the extensions are supported, FALSE otherwise.
//E.g.
//   if (checkbuiltin(94,95,96)) {...}
//is equivalent to
//   if (checkextension("DP_QC_MINMAXBOUND")) {...}
//Another way to check several contiguous builtin numbers in one call:
//   if (checkbuiltinrange(94,96)) {...}






