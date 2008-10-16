
//FTE_CALLTIMEOFDAY
//IDEA: mvdsv team
void() calltimeofday = #231;
//void(float secs, float min, float hour, float day, float month, float year, string datestring) timeofday = {};
//A new builtin which immediatly calls a "timeofday" QC function. The key functionality here is that the parameters to the timeofday qc function are infact, the time of day.
//Note that expected use is to copy the parameters into globals, and then use the globals where the builtin was called from.
//History: This extension is identical to the calltimeofday builtin found in mvdsv. However, it uses a different builtin number in an effort to reduce builtin conflicts with other extensions (which mvdsv is not good for). Using cunning coding, you can achieve support for either.

//FTE_FORCEINFOKEY
//FIXME:forceinfokey

//FTE_ISBACKBUFFERED
//FIXME:isbackbuffered

//FTE_MEDIA_AVI
//The console command 'playfilm' supports avi files. Uncompressed audio only, however.

//FTE_MEDIA_CIN
//The console command 'playfilm' supports quake2 .cin files.

//FTE_MEDIA_ROQ
//The console command 'playfilm' supports quake3 .RoQ files.

//FTE_MULTIPROGS
//FIXME
//all sorts of fun multiple progs stuff all running in the same virtual machine.
//it's great fun.

//FTE_MULTITHREADED
void(float duration) sleep = #;
float() fork = #;
void(...) abort = #;
//sleep stops execution and magically restarts duration seconds later
//fork magically splits execution and somehow returns twice each individual time it's called.
//abort returns from the current function as well as all calling qc functions. Pass a parameter if the root qc function is meant to return something.
//note that sleep and fork preserve only locals and not globals or entity fields.

//FTE_MVD_PLAYBACK
//there is a playmvd console command, which can play a multiview demo to all connected clients, returning to the current gamestate when finished.

//FTE_NPCCHAT
//FIXME:chat
//a scripting language supporting qc callbacks, specially designed for one-on-one conversations with non-player charactures.

//FTE_SOLID_LADDER
float SOLID_LADDER = 20;
//players entering a solid type of this type using qw physics will treat it as a ladder.

//FTE_STRINGS
//FIXME:stof
//FIXME:strlen
//FIXME:strcat
//FIXME:substring
//FIXME:stov
//FIXME:strzone
//FIXME:strunzone
//FIXME:strstrofs
//FIXME:str2chr
//FIXME:chr2str
//FIXME:strconv
//FIXME:infoadd
//FIXME:infoget
//FIXME:strncmp
//FIXME:strcasecmp
//FIXME:strncasecmp



//FTE_PEXT_SETVIEW"},		//nq setview works.

//FTE_PEXT_LIGHTSTYLECOL
void(float style, string value, float colourmask) lightstylecol = #35;
float LS_RED	= 1;
float LS_GREEN	= 2;
float LS_BLUE	= 4;
//colours light effects according to bits. Doesn't even need lits. Useful for making an entire map red, and stuff.

//FTE_PEXT_VIEW2
.entity view2;
//when set, the player will gain an extra little window showing the world from a different viewpoint.
//Move and angle the view2 entity to position it. Great for rocket cams and alerts (ctf?..)

//FTE_PEXT_BULLETENS
//Certain texture names can be replaced with text. Can show leader/looser/adverts.

//FTE_PEXT_FATNESS
.float fatness;
//expands an entity along it's vertex normals.
//Makes it fat without scaling it up.
//vacuum chambers...

//FTE_PEXT_HULLSIZE
.float hull;
//Use this hull number minus one for bsp collisions, and a bbox for entity-entity collisions.

//FTE_PEXT_CUSTOMTENTS
//FIXME:RegisterTempEnt
//FIXME:CustomTempEnt

//FTE_PEXT_256PACKETENTITIES
//Removes the 64 entities visible at once that the qw protocol origionally suffered from.

//EXT_CSQC