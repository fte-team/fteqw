#include "quakedef.h"
#ifdef MAP_DOOM
#include "glquake.h"
#include "shader.h"

vec3_t doom_player1_start;
float  doom_player1_yaw;

typedef struct doommap_s doommap_t;
static shader_t *Doom_MonsterSpriteShader(const char *lump, short *w, short *h, short *xo, short *yo);
static shader_t *Doom_SpriteShaderFor(const char *lump, texid_t tex);
static void Doom_VoxShader(void);
static qboolean Doom_DrawVoxelByName(const char *name, const vec3_t origin, float yawdeg, float scale);
static void R_DoomDrawHUD(doommap_t *dm);



char *va2(char *buffer, size_t buffersize, const char *format, ...)
{
	va_list		argptr;

	va_start (argptr, format);
	buffer[--buffersize] = 0;
	vsnprintf (buffer, buffersize, format, argptr);
	va_end (argptr);

	return buffer;
}

int SignbitsForPlane (mplane_t *out);
int	PlaneTypeForNormal ( vec3_t normal );

//the engine's global r_visframecount was removed; the Doom renderer only uses it
//to dedupe sector draws within a single R_RecursiveDoomNode pass, so a private
//monotonic counter is sufficient.
static int r_visframecount;

//coded from file specifications provided by:
//Matthew S Fell (msfell@aol.com)
//Unofficial Doom Specs

//(aol suck)


//assumptions:
//1. That there is a node, and thus two ssectors.
//2. That the user doesn't want textures...
//3. That all segs ssectors for a single sector are all the same.
//4. That ALL sectors are fully enclosed, and not made of two areas.
//5. That no sectors are inside out.

/*FIXME:
we need to do a bsp2prt type thing (walk nodes and determine actual leaf/ssector shapes based upon those).
build sector geometry based upon this.
this is because flats in doom were implemented using a flood-fill algorithm and thus omits various unecessary inner edges, while 3d rendering apis all need tri-soup instead.
attempting to generate sane volumes from most doom maps is doomed to failure because quite often the sector values on linedefs is just buggy, resulting in some really whacky polygons that cannot be souped in any meaningful way.
this may still result in a mess of floor polygons outside the world, so be sure to draw those last, for early-z.
*/

enum {
	THING_PLAYER		= 1,
	THING_PLAYER2		= 2,
	THING_PLAYER3		= 3,
	THING_PLAYER4		= 4,
	THING_DMSPAWN		= 11,

//we need to balance weapons according to ammo types.
	THING_WCHAINSAW		= 2005,	//-> quad
	THING_WSHOTGUN1		= 2001,	//-> ng
	THING_WSHOTGUN2		= 82,	//-> sng
	THING_WCHAINGUN		= 2002,	//-> ssg
	THING_WROCKETL		= 2003,	//-> lightning
	THING_WPLASMA		= 2004,	//-> grenade
	THING_WBFG			= 2006	//-> rocket
} THING_TYPES;

//thing flags
//skill/dm is appears in rather than quake's excuded in.
#define THING_EASY			1
#define THING_MEDIUM		2
#define THING_HARD			4
#define THING_DEAF			8
#define	THING_DEATHMATCH	16
//other bits are ignored



typedef struct {
	short xpos;
	short ypos;
	short angle;
	unsigned short type;
	unsigned short flags;
} dthing_t;

typedef struct {
	short xpos;
	short ypos;
} ddoomvertex_t;

typedef struct {
	float xpos;
	float ypos;
} mdoomvertex_t;

typedef struct {
	unsigned short vert[2];
	unsigned short flags;
	short types;
	short tag;
	unsigned short sidedef[2]; //(0xffff is none for sidedef[1])
} dlinedef_t;
#define LINEDEF_IMPASSABLE		1
#define	LINEDEF_BLOCKMONSTERS	2
#define LINEDEF_TWOSIDED		4
#define LINEDEF_UPPERUNPEGGED	8
#define LINEDEF_LOWERUNPEGGED	16
#define LINEDEF_SECRET			32	//seen as singlesided on automap, does nothing else.
#define LINEDEF_BLOCKSOUND		64
#define LINEDEF_NOTONMAP		128	//doesn't appear on automap.
#define LINEDEF_STARTONMAP		256
//others are ignored.

typedef struct {
	short texx;
	short texy;
	char uppertex[8];
	char lowertex[8];
	char middletex[8];
	unsigned short sector;
} dsidedef_t;
typedef struct {
	float texx;
	float texy;
	int uppertex;
	int lowertex;
	int middletex;
	unsigned short sector;
} msidedef_t;

typedef struct {	//figure out which linedef to use and throw the rest away.
	unsigned short	vert[2];
	short	angle;
	unsigned short	linedef;
	short	direction;
	short	offset;
} dseg_t;

typedef struct {
	unsigned short	vert[2];
	unsigned short	linedef;
	short	direction;
	unsigned short Partner;	//the one on the other side of the owner's linedef
} dgl_seg1_t;

typedef struct {
	unsigned int	vert[2];
	unsigned short	linedef;
	short	direction;
	unsigned int Partner;	//the one on the other side of the owner's linedef
} dgl_seg3_t;

typedef struct {
	unsigned short segcount;
	unsigned short first;
} dssector_t;

typedef struct {
	struct msector_s *sector;
	unsigned short segcount;
	unsigned short first;
} mssector_t;

typedef struct {
	short x;
	short y;
	short dx;
	short dy;
	short y1upper;
	short y1lower;
	short x1lower;
	short x1upper;
	short y2upper;
	short y2lower;
	short x2lower;
	short x2upper;
	unsigned short node1;
	unsigned short node2;
} ddoomnode_t;
#define NODE_IS_SSECTOR	0x8000

typedef struct {
	short floorheight;
	short ceilingheight;
	char floortexture[8];
	char ceilingtexture[8];
	short lightlevel;
	short specialtype;
	short tag;
} dsector_t;

typedef struct msector_s {
	int visframe;
	int floortex;
	int ceilingtex;

	short floorheight;
	short ceilingheight;

	qbyte lightlev;
	qbyte pad;
	int numflattris;
	short tag;
	short specialtype;

	unsigned short *flats;
} msector_t;


typedef struct {
	short xorg;
	short yorg;
	short columns;
	short rows;
} blockmapheader_t;

typedef struct
{
	char name[16];
	shader_t *shader;
	unsigned short width;
	unsigned short height;
	batch_t batch;
	mesh_t *meshptr;
	mesh_t mesh;
	int maxverts;
	int maxindicies;
} doomtexture_t;

typedef struct doommap_s
{
	model_t			*model;
	int				skytex;		//-1 if no sky flat loaded yet

	ddoomnode_t		*node;
	plane_t			*nodeplane;
	unsigned int	numnodes;

	mssector_t		*ssector;	//aka: leafs
	unsigned int	numssectors;

	msector_t		*sector;
	unsigned int	numsectors;

	dthing_t		*thing;
	unsigned int	numthings;

	mdoomvertex_t	*vertexes;
	unsigned int	numvertexes;

	dgl_seg3_t		*seg;
	unsigned int	numsegs;

	dlinedef_t		*linedef;
	plane_t			*lineplane;
	unsigned int	 numlinedefs;

	msidedef_t		*sidedef;
	unsigned int	numsidedefs;

	blockmapheader_t *blockmap;
	unsigned short	*blockmapofs;

	unsigned int	vertexsglbase;

	doomtexture_t	*textures;
	unsigned int	numtextures;

	// Sector ceiling/floor animation (doors, lifts, floors)
	struct doorsector_s {
		int		sector_idx;
		int		state;		// 0=idle, 1=opening/raising, 2=waiting, 3=closing/lowering
		short	ceil_target;
		short	ceil_original;
		short	floor_target;
		short	floor_original;
		float	wait_time;
		float	wait_max;
		float	speed;
		float	frac;		// carried sub-unit movement (sector heights are short - without
					// this the fractional per-tick move truncates away and the door sticks)
		qboolean move_floor;	// true if floor moves, false if ceiling
		qboolean repeating;	// DR = reopens on use; D1 = stays open
		int		special;	// original linedef special
	} *doorsectors;
	unsigned int numactive_doors;
	unsigned int maxactive_doors;

	// Doom item/decoration sprites, drawn as camera-facing billboards (R_DoomDrawSprites)
	struct doomsprite_s {
		vec3_t		origin;		// world position (x, y, floor-z)
		shader_t	*shader;	// per-sprite-texture shader (alpha-tested)
		short		w, h;		// sprite pixel size (1px = 1 map unit)
		short		xo, yo;		// sprite left/top offsets (the thing's hotspot)
		qbyte		pickup;		// 1 = collectable item (removed when the player walks over it)
		unsigned short type;	// original Doom thing type (so a pickup knows its effect)
		char		voxname[8];	// voxel model name (sprite frame minus rotation digit, e.g. "COLUA")
	} *sprites;
	unsigned int numsprites;

	// Doom monsters: spawned from monster thing types, chase the player (Doom_TickMonsters),
	// rendered as billboards (R_DoomDrawMonsters). The 8 view rotations share a directional
	// sprite set; index [frame][rot]. Updated server-side, rendered client-side (shared model
	// in local play, like the doors).
	struct doommonster_s {
		vec3_t		origin;		// current position (x, y, feet-z)
		float		yaw;		// facing direction (degrees)
		int			health;
		unsigned short type;	// doom thing type
		qbyte		mstate;		// 0=idle, 1=chasing, 2=dead/gibbed
		qbyte		radius;		// collision/move radius
		short		speed;		// move units/sec
		float		atkcool;	// AI: seconds until this monster may attack again
		//attack profile (cached from Doom_MonsterInfo at spawn)
		qbyte		atk;		// MATK_* bitmask (melee/hitscan/missile/homing/vile)
		short		meleedmg;	// melee damage multiplier (dmg = meleedmg*rand(1..meleerand))
		qbyte		meleerand;	// melee random max (8 default, demon/revenant 10, caco 6)
		short		misdmg;		// missile damage base (on hit: misdmg*rand(1..8))
		short		misspeed;	// projectile speed (units/sec)
		qbyte		bullets;	// hitscan shots per attack
		qbyte		alerted;	// 0 until the monster sees or hears the player, then it chases
		qbyte		floating;	// 1 = moves in 3D (caco/lostsoul/pain), doesn't snap to floor
		qbyte		exploded;	// barrels: 1 once the blast has been dealt
		float		animt;		// walk-cycle animation timer (front frames A-D in shader[0..3])
		float		deathtime;	// seconds since killed (drives the death-frame animation; <0 = alive)
		float		paintime;	// seconds since hurt (drives the pain-frame animation; <0 = normal)
		float		atktime;	// seconds since started attacking (drives the attack animation)
		float		vilet;		// archvile: seconds into the hellfire windup (-1 = not casting)
		float		risetime;	// archvile resurrection: seconds into the reverse-death Raise (-1 = no)
		vec3_t		spawnorigin;// initial state, restored on a full map reset (player respawn)
		float		spawnyaw;
		int			spawnhealth;
		shader_t	*shader[4][8];	// per-rotation sprite shader (rot 1..8 -> [0..7]); [frame][rot]
		short		w[4][8], h[4][8], xo[4][8], yo[4][8];
		qbyte		wmir[4][8];	// 1 = this rotation comes from a mirrored combined lump (drawn flipped)
		shader_t	*atkfr[4];	// attack animation sequence
		short		afw[4], afh[4], afxo[4];
		shader_t	*painfr[4];	// pain animation sequence
		short		pfw[4], pfh[4], pfxo[4];
		shader_t	*deathfr[12];	// death animation sequence (last frame is the resting corpse)
		short		dfw[12], dfh[12], dfxo[12];
		qbyte		ndeath;		// number of loaded death frames
		qbyte		natk;		// number of loaded attack frames
		qbyte		npain;		// number of loaded pain frames
		qbyte		nwalk;		// number of walk frames (1 for caco/single-frame floaters, else up to 4)
		const char	*spr;		// sprite/voxel base name (e.g. "POSS"), for the voxel renderer
	} *monsters;
	unsigned int nummonsters;

	// monster/player projectiles (imp/caco/baron balls, revenant homing tracer, rockets, ...)
	struct doomproj_s {
		vec3_t		origin, vel;	// position + velocity (units/sec)
		int			damage;			// applied to whatever it hits
		float		life;			// seconds before it self-expires
		qbyte		homing;			// 1 = steer toward the player each frame (revenant tracer)
		qbyte		type;			// 0=monster ball, 1=rocket, 2=plasma, 3=BFG, 4=BFG-tracer
		qbyte		owner;			// 0=monster, 1=player
		shader_t	*shader;
		short		w, h, xo, yo;
	} *projectiles;
	unsigned int numprojectiles;
} doommap_t;

void Doom_SetModelFunc(model_t *mod);

////////////////////////////////////////////////////////////////////////////////////////////
//door/sector animation

#define DOOR_SPEED		64	// units per second
#define DOOR_WAIT		3.5f	// seconds before auto-close
#define PLAT_WAIT		3.0f	// seconds before lift returns

static float Doom_FindLowestFloorSurrounding(doommap_t *dm, int sec_idx)
{
	unsigned int j;
	float lowest = 32767;
	for (j = 0; j < dm->numlinedefs; j++)
	{
		dlinedef_t *ld = &dm->linedef[j];
		if (ld->sidedef[1] == 0xffff) continue;
		int s0 = dm->sidedef[ld->sidedef[0]].sector;
		int s1 = dm->sidedef[ld->sidedef[1]].sector;
		int other = -1;
		if (s0 == sec_idx) other = s1;
		else if (s1 == sec_idx) other = s0;
		if (other < 0) continue;
		if (dm->sector[other].floorheight < lowest)
			lowest = dm->sector[other].floorheight;
	}
	return lowest == 32767 ? dm->sector[sec_idx].floorheight : lowest;
}

static float Doom_FindHighestFloorSurrounding(doommap_t *dm, int sec_idx)
{
	unsigned int j;
	float highest = -32768;
	for (j = 0; j < dm->numlinedefs; j++)
	{
		dlinedef_t *ld = &dm->linedef[j];
		if (ld->sidedef[1] == 0xffff) continue;
		int s0 = dm->sidedef[ld->sidedef[0]].sector;
		int s1 = dm->sidedef[ld->sidedef[1]].sector;
		int other = -1;
		if (s0 == sec_idx) other = s1;
		else if (s1 == sec_idx) other = s0;
		if (other < 0) continue;
		if (dm->sector[other].floorheight > highest)
			highest = dm->sector[other].floorheight;
	}
	return highest == -32768 ? dm->sector[sec_idx].floorheight : highest;
}

static float Doom_FindLowestCeilingSurrounding(doommap_t *dm, int sec_idx)
{
	unsigned int j;
	float lowest = 32767;
	for (j = 0; j < dm->numlinedefs; j++)
	{
		dlinedef_t *ld = &dm->linedef[j];
		if (ld->sidedef[1] == 0xffff) continue;
		int s0 = dm->sidedef[ld->sidedef[0]].sector;
		int s1 = dm->sidedef[ld->sidedef[1]].sector;
		int other = -1;
		if (s0 == sec_idx) other = s1;
		else if (s1 == sec_idx) other = s0;
		if (other < 0 || other == sec_idx) continue;
		if (dm->sector[other].ceilingheight < lowest)
			lowest = dm->sector[other].ceilingheight;
	}
	return (lowest == 32767) ? dm->sector[sec_idx].ceilingheight : lowest;
}

static float Doom_FindHighestCeilingSurrounding(doommap_t *dm, int sec_idx)
{
	unsigned int j;
	float highest = -32768;
	for (j = 0; j < dm->numlinedefs; j++)
	{
		dlinedef_t *ld = &dm->linedef[j];
		if (ld->sidedef[1] == 0xffff) continue;
		int s0 = dm->sidedef[ld->sidedef[0]].sector;
		int s1 = dm->sidedef[ld->sidedef[1]].sector;
		int other = -1;
		if (s0 == sec_idx) other = s1;
		else if (s1 == sec_idx) other = s0;
		if (other < 0) continue;
		if (dm->sector[other].ceilingheight > highest)
			highest = dm->sector[other].ceilingheight;
	}
	return highest == -32768 ? dm->sector[sec_idx].ceilingheight : highest;
}

static float Doom_FindNextHighestFloor(doommap_t *dm, int sec_idx)
{
	unsigned int j;
	float current = dm->sector[sec_idx].floorheight;
	float next = 32767;
	for (j = 0; j < dm->numlinedefs; j++)
	{
		dlinedef_t *ld = &dm->linedef[j];
		if (ld->sidedef[1] == 0xffff) continue;
		int s0 = dm->sidedef[ld->sidedef[0]].sector;
		int s1 = dm->sidedef[ld->sidedef[1]].sector;
		int other = -1;
		if (s0 == sec_idx) other = s1;
		else if (s1 == sec_idx) other = s0;
		if (other < 0) continue;
		float f = dm->sector[other].floorheight;
		if (f > current && f < next)
			next = f;
	}
	return next == 32767 ? current : next;
}

// Return true if linedef special is a door/sector type we handle
qboolean Doom_IsActivatableLinedef(int special)
{
	switch(special)
	{
	case 1:  case 26: case 27: case 28:	// DR doors (reusable)
	case 31: case 32: case 33: case 34:	// D1 doors (one-shot)
	case 117: case 118:			// fast doors
	case 2:  case 3:  case 4:		// W1 open/close
	case 46:				// GR open stays
	case 61: case 63:			// SR open stay / close
	case 103:				// S1 open stay
	case 75: case 76:			// WR close/open
	case 10: case 88:			// Plat Down-Wait-Up-Stay
	case 22:				// Raise floor to next highest floor and change texture
	case 19:				// Lower floor to highest surrounding floor
	case 38:				// Lower floor to lowest surrounding floor
	case 5:  case 91:			// Raise floor to lowest surrounding ceiling
	case 62: case 123:			// Plat Down-Wait-Up-Stay (SR/S1)
	case 11: case 51: case 52: case 124:	// Exit level
	case 30:				// Raise floor to shortest texture height
	case 37:				// Lower floor to lowest adjacent and change texture
	case 40:				// Raise ceiling lower floor
		return true;
	}
	return false;
}

static int Doom_FindOrAddSectorAnim(doommap_t *dm, int sec_idx)
{
	unsigned int j;
	for (j = 0; j < dm->numactive_doors; j++)
		if (dm->doorsectors[j].sector_idx == sec_idx)
			return (int)j;
	if (dm->numactive_doors >= dm->maxactive_doors)
	{
		dm->maxactive_doors += 16;
		dm->doorsectors = BZ_Realloc(dm->doorsectors, sizeof(*dm->doorsectors)*dm->maxactive_doors);
	}
	memset(&dm->doorsectors[dm->numactive_doors], 0, sizeof(dm->doorsectors[0]));
	dm->doorsectors[dm->numactive_doors].sector_idx = sec_idx;
	dm->doorsectors[dm->numactive_doors].ceil_original = dm->sector[sec_idx].ceilingheight;
	dm->doorsectors[dm->numactive_doors].ceil_target = dm->sector[sec_idx].ceilingheight;
	dm->doorsectors[dm->numactive_doors].floor_original = dm->sector[sec_idx].floorheight;
	dm->doorsectors[dm->numactive_doors].floor_target = dm->sector[sec_idx].floorheight;
	dm->doorsectors[dm->numactive_doors].speed = DOOR_SPEED;
	return (int)dm->numactive_doors++;
}

static void Doom_ApplySpecialToSector(doommap_t *dm, int si, int special, int tag, int linedef_idx)
{
	int anim_idx = Doom_FindOrAddSectorAnim(dm, si);
	struct doorsector_s *d = &dm->doorsectors[anim_idx];
	d->special = special;

	switch(special) {
		// --- DOORS ---
		case 1:  case 26: case 27: case 28: case 117: // DR reusable
		case 31: case 32: case 33: case 34: case 118: // D1 one-shot
		case 2:  case 3:  case 4:  case 103:          // W1/S1
		case 46: case 61: case 63: case 75: case 76:  // GR/SR/WR
			if (d->state == 0 || d->state == 3) {
				d->state = 1; // raising
				d->move_floor = false;
				d->speed = (special == 117 || special == 118) ? DOOR_SPEED*4 : DOOR_SPEED;
				d->repeating = (special==1||special==26||special==27||special==28||special==117||special==61||special==63||special==75||special==76);
				d->wait_max = DOOR_WAIT;
				// Target is lowest adjacent ceiling - 4
				d->ceil_target = (short)Doom_FindLowestCeilingSurrounding(dm, si) - 4;
			} else if (d->state == 2 && d->repeating) {
				d->state = 3; // close now
			}
			break;

		// --- PLATFORMS / LIFTS ---
		case 10: case 88: case 62: case 123: // Down-Wait-Up-Stay
			if (d->state == 0) {
				d->state = 3; // lowering
				d->move_floor = true;
				d->speed = DOOR_SPEED * 3;
				d->repeating = true; // returns up
				d->wait_max = PLAT_WAIT;
				d->floor_target = Doom_FindLowestFloorSurrounding(dm, si);
				d->floor_original = dm->sector[si].floorheight;
			}
			break;

		// --- FLOORS ---
		case 19: // Lower floor to highest surrounding floor
			if (d->state == 0) {
				d->state = 3; d->move_floor = true; d->speed = DOOR_SPEED;
				d->floor_target = Doom_FindHighestFloorSurrounding(dm, si);
			}
			break;
		case 38: // Lower floor to lowest surrounding floor
			if (d->state == 0) {
				d->state = 3; d->move_floor = true; d->speed = DOOR_SPEED;
				d->floor_target = Doom_FindLowestFloorSurrounding(dm, si);
			}
			break;
		case 5: case 91: // Raise floor to lowest surrounding ceiling
			if (d->state == 0) {
				d->state = 1; d->move_floor = true; d->speed = DOOR_SPEED;
				d->floor_target = Doom_FindLowestCeilingSurrounding(dm, si);
			}
			break;
		case 22: // Raise floor to next highest floor
			if (d->state == 0) {
				d->state = 1; d->move_floor = true; d->speed = DOOR_SPEED/2;
				d->floor_target = Doom_FindNextHighestFloor(dm, si);
			}
			break;
		case 30: // Raise floor to shortest texture height
			if (d->state == 0) {
				d->state = 1; d->move_floor = true; d->speed = DOOR_SPEED;
				// Simplified: raise by 128 units
				d->floor_target = dm->sector[si].floorheight + 128;
			}
			break;
		case 37: // Lower floor to lowest adjacent and change texture
			if (d->state == 0) {
				d->state = 3; d->move_floor = true; d->speed = DOOR_SPEED;
				d->floor_target = Doom_FindLowestFloorSurrounding(dm, si);
			}
			break;
		case 40: // Raise ceiling lower floor (Secrets)
			{
				d->state = 1; d->move_floor = false; d->speed = DOOR_SPEED;
				d->ceil_target = Doom_FindHighestCeilingSurrounding(dm, si);
			}
			break;

		// --- EXITS ---
		case 11: case 51: case 52: case 124:
			Cbuf_AddText("echo LEVEL COMPLETE; nextmap\n", 0);
			break;
	}
}

void Doom_ActivateLinedef(model_t *model, int linedef_idx)
{
	doommap_t *dm = model->meshinfo;
	dlinedef_t *ld;
	int special, tag;
	int s;

	if (!dm || linedef_idx < 0 || (unsigned)linedef_idx >= dm->numlinedefs)
		return;
	ld = dm->linedef + linedef_idx;
	special = ld->types;
	tag = ld->tag;

	if (!Doom_IsActivatableLinedef(special))
		return;

	if (tag == 0)
	{
		// Type 1: activate the back sector directly (typical for simple doors)
		if (ld->sidedef[1] == 0xffff) return;
		Doom_ApplySpecialToSector(dm, dm->sidedef[ld->sidedef[1]].sector, special, tag, linedef_idx);
	}
	else
	{
		// Tagged sector action
		for (s = 0; s < (int)dm->numsectors; s++)
		{
			if (dm->sector[s].tag == tag)
				Doom_ApplySpecialToSector(dm, s, special, tag, linedef_idx);
		}
	}
}

// Per-frame tick: animate all active door/platform/floor sectors
void Doom_TickDoors(model_t *model, float frametime)
{
	doommap_t *dm = model->meshinfo;
	unsigned int i;
	float move;

	if (!dm) return;

	// Update scrolling textures and other specials
	for (i = 0; i < dm->numlinedefs; i++)
	{
		dlinedef_t *ld = &dm->linedef[i];
		if (ld->types == 48) { // scroll left
			dm->sidedef[ld->sidedef[0]].texx += 64 * frametime;
		}
	}

	// Simple light effects
	static float lightt = 0;
	lightt += frametime;
	for (i = 0; i < dm->numsectors; i++)
	{
		msector_t *s = &dm->sector[i];
		if (s->specialtype == 1) { // blinking (random)
			if (((int)(lightt*8))&1) s->lightlev = 128; else s->lightlev = 255;
		} else if (s->specialtype == 2 || s->specialtype == 3) { // strobe
			float period = (s->specialtype == 2) ? 0.5f : 1.0f;
			if (fmod(lightt, period) < 0.1f) s->lightlev = 128; else s->lightlev = 255;
		} else if (s->specialtype == 8) { // oscillation
			s->lightlev = 128 + (qbyte)(127 * (0.5f + 0.5f * sin(lightt*4)));
		} else if (s->specialtype == 17) { // random flicker
			if (rand()&1) s->lightlev = 160; else s->lightlev = 255;
		}
	}

	for (i = 0; i < dm->numactive_doors; i++)
	{
		struct doorsector_s *d = &dm->doorsectors[i];
		msector_t *sec = &dm->sector[d->sector_idx];

		//sector heights are shorts, so accumulate the fractional move and only apply whole units
		//(carrying the remainder) - otherwise a sub-unit per-tick move truncates to 0 and the door
		//never progresses (at high framerate speed*frametime < 1).
		move = d->speed * frametime + d->frac;
		d->frac = move - floorf(move);
		move = floorf(move);

		switch(d->state)
		{
		case 1:	// raising
			if (d->move_floor) {
				sec->floorheight += move;
				if (sec->floorheight >= d->floor_target) {
					sec->floorheight = d->floor_target;
					d->state = 0; // done
				}
			} else {
				sec->ceilingheight += move;
				if (sec->ceilingheight >= d->ceil_target) {
					sec->ceilingheight = d->ceil_target;
					if (d->repeating) { d->state = 2; d->wait_time = d->wait_max; }
					else d->state = 0;
				}
			}
			break;
		case 2:	// waiting
			d->wait_time -= frametime;
			if (d->wait_time <= 0)
				d->state = 3; // start closing/lowering
			break;
		case 3:	// lowering
			if (d->move_floor) {
				sec->floorheight -= move;
				if (sec->floorheight <= d->floor_target) {
					sec->floorheight = d->floor_target;
					if (d->repeating) {
						// Lift reached bottom: wait and return
						d->state = 2; d->wait_time = d->wait_max;
						d->floor_target = d->floor_original;
						d->repeating = false; // final return
					} else d->state = 0;
				}
			} else {
				sec->ceilingheight -= move;
				if (sec->ceilingheight <= sec->floorheight + 4) {
					sec->ceilingheight = sec->floorheight + 4;
					d->state = 0;
				}
			}
			break;
		}
	}
}

////////////////////////////////////////////////////////////////////////////////////////////
//physics

/*walk the bsp tree*/
msector_t *Doom_SectorNearPoint(doommap_t *dm, const vec3_t p)
{
	ddoomnode_t *node;
	plane_t *plane;
	int num;
	float d;
	num = dm->numnodes-1;
	while (1)
	{
		if (num & NODE_IS_SSECTOR)
		{
			num -= NODE_IS_SSECTOR;
			return dm->ssector[num].sector;
		}

		node = dm->node + num;
		plane = dm->nodeplane + num;
		
//		if (plane->type < 3)
//			d = p[plane->type] - plane->dist;
//		else
			d = DotProduct (plane->normal, p) - plane->dist;
		if (d < 0)
			num = node->node2;
		else
			num = node->node1;
	}
	
	return NULL;
}

unsigned int Doom_PointContents(model_t *model, const vec3_t axis[3], const vec3_t p)
{
	doommap_t *dm = model->meshinfo;
	msector_t *sec = Doom_SectorNearPoint(dm, p);
	if (p[2] < sec->floorheight)
		return FTECONTENTS_SOLID;
	if (p[2] > sec->ceilingheight)
		return FTECONTENTS_SOLID;
	return FTECONTENTS_EMPTY;
}

/*
fixme:
use q2-style bsp collision using the trisoup for flats collisions.
use blockmap for walls
*/
//P_BoxOnLineSide (chocolate-doom p_maputl.c): which side of linedef ld is the 2D box on?
//0=front, 1=back, -1=the box straddles the line. A pure integer-style box-vs-line side test (no
//floating-point plane sweep), used by the post-trace guard below to GUARANTEE the box never ends a
//move sitting inside a solid wall - which the swept clips can still let happen a hair at corners.
//0=front, 1=back, -1=the box straddles the line. A pure integer-style box-vs-line side test (no
//floating-point plane sweep), used by the post-trace guard below to GUARANTEE the box never ends a
//move sitting inside a solid wall - which the swept clips can still let happen a hair at corners.
static int Doom_PointOnLineSide(float x, float y, dlinedef_t *line, doommap_t *dm)
{
	mdoomvertex_t *v1 = &dm->vertexes[line->vert[0]];
	mdoomvertex_t *v2 = &dm->vertexes[line->vert[1]];
	float ldx = v2->xpos - v1->xpos;
	float ldy = v2->ypos - v1->ypos;
	if (!ldx) {
		if (x <= v1->xpos) return ldy > 0;
		return ldy < 0;
	}
	if (!ldy) {
		if (y <= v1->ypos) return ldx < 0;
		return ldx > 0;
	}
	float dx = (x - v1->xpos);
	float dy = (y - v1->ypos);
	if (dy * ldx < dx * ldy) return 0; //front
	return 1; //back
}

static int Doom_BoxOnLineSide(float bminx, float bminy, float bmaxx, float bmaxy, dlinedef_t *ld, doommap_t *dm)
{
	int p1, p2;
	mdoomvertex_t *v1 = &dm->vertexes[ld->vert[0]];
	mdoomvertex_t *v2 = &dm->vertexes[ld->vert[1]];
	float ldx = v2->xpos - v1->xpos;
	float ldy = v2->ypos - v1->ypos;

	if (ldy == 0) { //horizontal
		p1 = bmaxy > v1->ypos;
		p2 = bminy > v1->ypos;
		if (ldx < 0) { p1 ^= 1; p2 ^= 1; }
	} else if (ldx == 0) { //vertical
		p1 = bmaxx < v1->xpos;
		p2 = bminx < v1->xpos;
		if (ldy < 0) { p1 ^= 1; p2 ^= 1; }
	} else if (ldx * ldy > 0) { //positive slope
		p1 = Doom_PointOnLineSide(bminx, bmaxy, ld, dm);
		p2 = Doom_PointOnLineSide(bmaxx, bminy, ld, dm);
	} else { //negative slope
		p1 = Doom_PointOnLineSide(bmaxx, bmaxy, ld, dm);
		p2 = Doom_PointOnLineSide(bminx, bminy, ld, dm);
	}
	return (p1 == p2) ? p1 : -1;
}

static void Doom_LineOpening(dlinedef_t *ld, doommap_t *dm, float *opentop, float *openbottom, float *lowfloor)
{
	msector_t *front, *back;
	if (ld->sidedef[1] == 0xffff) {
		*opentop = *openbottom = *lowfloor = 0;
		return;
	}
	front = &dm->sector[dm->sidedef[ld->sidedef[0]].sector];
	back  = &dm->sector[dm->sidedef[ld->sidedef[1]].sector];
	*opentop = (front->ceilingheight < back->ceilingheight) ? front->ceilingheight : back->ceilingheight;
	if (front->floorheight > back->floorheight) {
		*openbottom = front->floorheight;
		*lowfloor = back->floorheight;
	} else {
		*openbottom = back->floorheight;
		*lowfloor = front->floorheight;
	}
}

static qboolean Doom_CheckPosition(doommap_t *dm, float x, float y, float radius, float height, float feetz, const vec3_t dir, dlinedef_t **hitline, float *tmfloorz, float *tmceilingz, float *tmdropoffz)
{
	int xl, xh, yl, yh, bx, by;
	float bminx = x - radius, bmaxx = x + radius;
	float bminy = y - radius, bmaxy = y + radius;
	vec3_t p = {x, y, feetz + 1.0f};
	msector_t *sec = Doom_SectorNearPoint(dm, p);
	qboolean solid = false;
	float best_dot = -1.1f;
	if (!sec) return false;

	*tmfloorz = *tmdropoffz = sec->floorheight;
	*tmceilingz = sec->ceilingheight;
	if (hitline) *hitline = NULL;
	
	dlinedef_t *floorline = NULL;
	dlinedef_t *ceilline = NULL;
	dlinedef_t *dropline = NULL;

	xl = (int)(bminx - dm->blockmap->xorg) >> 7;
	xh = (int)(bmaxx - dm->blockmap->xorg) >> 7;
	yl = (int)(bminy - dm->blockmap->yorg) >> 7;
	yh = (int)(bmaxy - dm->blockmap->yorg) >> 7;

	for (bx = xl; bx <= xh; bx++)
	for (by = yl; by <= yh; by++)
	{
		int bmi = bx + by * dm->blockmap->columns;
		unsigned short *vl;
		if (bx < 0 || bx >= dm->blockmap->columns || by < 0 || by >= dm->blockmap->rows) continue;
		for (vl = (unsigned short*)dm->blockmap + dm->blockmapofs[bmi]+1; *vl != 0xffff; vl++)
		{
			dlinedef_t *ld = &dm->linedef[*vl];
			float ot, ob, lf;
			mdoomvertex_t *v1 = &dm->vertexes[ld->vert[0]];
			mdoomvertex_t *v2 = &dm->vertexes[ld->vert[1]];
			float lminx = (v1->xpos < v2->xpos) ? v1->xpos : v2->xpos;
			float lmaxx = (v1->xpos > v2->xpos) ? v1->xpos : v2->xpos;
			float lminy = (v1->ypos < v2->ypos) ? v1->ypos : v2->ypos;
			float lmaxy = (v1->ypos > v2->ypos) ? v1->ypos : v2->ypos;
			
			if (bmaxx <= lminx || bminx >= lmaxx || bmaxy <= lminy || bminy >= lmaxy)
				continue; // AABB reject

			if (Doom_BoxOnLineSide(bminx, bminy, bmaxx, bmaxy, ld, dm) != -1) continue;
			
			if (ld->sidedef[1] == 0xffff || (ld->flags & LINEDEF_IMPASSABLE)) {
				solid = true;
				if (hitline) {
					plane_t *lp = &dm->lineplane[*vl];
					float dot = -DotProduct(dir, lp->normal);
					if (dot > best_dot) {
						best_dot = dot;
						*hitline = ld;
					}
				}
				continue;
			}
			
			Doom_LineOpening(ld, dm, &ot, &ob, &lf);
			if (ot < *tmceilingz) { *tmceilingz = ot; ceilline = ld; }
			if (ob > *tmfloorz)   { *tmfloorz   = ob; floorline = ld; }
			if (lf < *tmdropoffz) { *tmdropoffz = lf; dropline = ld; }
		}
	}

	if (solid) return false;

	if (*tmceilingz - *tmfloorz < height) {
		if (hitline) *hitline = ceilline ? ceilline : floorline;
		return false; //too short
	}
	if (*tmfloorz - feetz > 24.1f) {
		if (hitline) *hitline = floorline;
		return false; //step too high
	}
	if (*tmfloorz - *tmdropoffz > 24.1f) {
		if (hitline) *hitline = dropline ? dropline : floorline;
		return false; //dropoff too deep
	}
	return true;
}

qboolean Doom_Trace(model_t *model, int hulloverride, const framestate_t *framestate, const vec3_t axis[3], const vec3_t start, const vec3_t end, const vec3_t mins, const vec3_t maxs, qboolean iscapsule, unsigned int contentstype, trace_t *trace)
{
	doommap_t *dm = model->meshinfo;
	float radius = maxs[0], height = maxs[2] - mins[2];
	float start_feetz = start[2] + mins[2];
	float fz, cz, dz;
	dlinedef_t *hitld = NULL;
	vec3_t move, dir;
	float dist, step;
	int i, num_steps;

	trace->fraction = 1;
	trace->allsolid = trace->startsolid = false;
	VectorCopy(end, trace->endpos);

	if (radius <= 0) return true; //not player/monster

	VectorSubtract(end, start, move);
	dist = VectorLength(move);
	if (dist < 0.1f) { VectorClear(dir); }
	else { VectorScale(move, 1.0f/dist, dir); }

	//Start pos check (lenient radius)
	if (!Doom_CheckPosition(dm, start[0], start[1], radius - 0.1f, height, start_feetz, dir, &hitld, &fz, &cz, &dz)) {
		trace->startsolid = trace->allsolid = true;
		trace->fraction = 0;
		VectorCopy(start, trace->endpos);
		return false;
	}

	if (dist < 0.1f) return true;

	step = 4.0f;
	num_steps = (int)ceil(dist / step);
	if (num_steps < 1) num_steps = 1;

	for (i = 1; i <= num_steps; i++)
	{
		float cur_dist = (i == num_steps) ? dist : i * step;
		float nx = start[0] + dir[0] * cur_dist;
		float ny = start[1] + dir[1] * cur_dist;
		float nz = start[2] + dir[2] * cur_dist;
		float current_feetz = nz + mins[2];

		if (!Doom_CheckPosition(dm, nx, ny, radius, height, start_feetz, dir, &hitld, &fz, &cz, &dz)) {
			//Wall hit: Binary search for exact impact point
			dlinedef_t *best_hitld = hitld;
			float safe_dist = cur_dist - step;
			if (safe_dist < 0) safe_dist = 0;
			float fail_dist = cur_dist;
			for (int b = 0; b < 10; b++) {
				float mid = (safe_dist + fail_dist) * 0.5f;
				float mx = start[0] + dir[0] * mid;
				float my = start[1] + dir[1] * mid;
				dlinedef_t *test_hitld = NULL;
				if (Doom_CheckPosition(dm, mx, my, radius, height, start_feetz, dir, &test_hitld, &fz, &cz, &dz)) {
					safe_dist = mid;
				} else {
					fail_dist = mid;
					best_hitld = test_hitld;
				}
			}
			trace->fraction = safe_dist / dist;
			VectorMA(start, trace->fraction, move, trace->endpos);
			if (best_hitld) {
				plane_t *lp = &dm->lineplane[best_hitld - dm->linedef];
				VectorCopy(lp->normal, trace->plane.normal);
				if (DotProduct(trace->plane.normal, dir) > 0)
					VectorScale(trace->plane.normal, -1.0f, trace->plane.normal);
				trace->plane.dist = DotProduct(trace->plane.normal, trace->endpos);
			} else {
				VectorScale(dir, -1, trace->plane.normal);
				trace->plane.dist = DotProduct(trace->plane.normal, trace->endpos);
			}
			return false;
		}

		//Check floor/ceiling collision
		if (current_feetz < fz - 0.1f) {
			if (fabs(dir[2]) > 1e-6f) {
				float hitz = fz - mins[2];
				trace->fraction = (hitz - start[2]) / (end[2] - start[2]);
			} else {
				trace->fraction = (i == 1) ? 0 : (cur_dist - step) / dist;
			}
			if (trace->fraction < 0) trace->fraction = 0;
			if (trace->fraction > 1) trace->fraction = 1;
			VectorMA(start, trace->fraction, move, trace->endpos);
			VectorSet(trace->plane.normal, 0, 0, 1);
			trace->plane.dist = fz;
			return false;
		}
		if (current_feetz + height > cz + 0.1f) {
			if (fabs(dir[2]) > 1e-6f) {
				float hitz = cz - maxs[2];
				trace->fraction = (hitz - start[2]) / (end[2] - start[2]);
			} else {
				trace->fraction = (i == 1) ? 0 : (cur_dist - step) / dist;
			}
			if (trace->fraction < 0) trace->fraction = 0;
			if (trace->fraction > 1) trace->fraction = 1;
			VectorMA(start, trace->fraction, move, trace->endpos);
			VectorSet(trace->plane.normal, 0, 0, -1);
			trace->plane.dist = -cz;
			return false;
		}
	}

	//Monster clip
	{
		float pr = radius - 0.1f; //lenient radius for sliding
		vec3_t fulld;
		unsigned int mi2;
		VectorSubtract(end, start, fulld);
		for (mi2 = 0; mi2 < dm->nummonsters; mi2++)
		{
			struct doommonster_s *m = &dm->monsters[mi2];
			float R, tmin, tmax, plo, phi, mlo, mhi;
			qboolean miss = false;
			int ax;
			if (m->mstate == 2) continue;
			plo = start[2] + mins[2]; phi = start[2] + maxs[2];
			mlo = m->origin[2]; mhi = m->origin[2] + (m->h[0][0] ? m->h[0][0] : 56);
			if (phi <= mlo || plo >= mhi) continue;
			R = m->radius + pr;
			tmin = 0; tmax = 1;
			for (ax = 0; ax < 2; ax++) {
				float c = m->origin[ax], d = fulld[ax], s = start[ax], t1, t2;
				if (fabs(d) < 1e-6f) { if (s < c - R || s > c + R) { miss = true; break; } }
				else {
					t1 = (c - R - s) / d; t2 = (c + R - s) / d;
					if (t1 > t2) { float tt = t1; t1 = t2; t2 = tt; }
					if (t1 > tmin) tmin = t1;
					if (t2 < tmax) tmax = t2;
					if (tmin > tmax) { miss = true; break; }
				}
			}
			if (miss || tmin < 0 || tmin >= trace->fraction) continue;
			
			//If tmin is 0, we are already overlapping - but only if the step isn't small.
			//Doom's discrete logic can handle a tiny amount of overlap.
			if (tmin < 0.001f && VectorLength(fulld) < 0.1f) continue;

			trace->fraction = tmin;
			VectorMA(start, tmin, fulld, trace->endpos);
			float nx = trace->endpos[0]-m->origin[0], ny = trace->endpos[1]-m->origin[1], nl = sqrt(nx*nx+ny*ny);
			if (nl < 1e-6f) { nx = -fulld[0]; ny = -fulld[1]; nl = sqrt(nx*nx+ny*ny); if (nl<1e-6f){nx=1;ny=0;nl=1;} }
			trace->plane.normal[0] = nx/nl; trace->plane.normal[1] = ny/nl; trace->plane.normal[2] = 0;
			trace->plane.dist = DotProduct(trace->plane.normal, trace->endpos);
		}
	}

	return (trace->fraction == 1);
}
























#ifndef SERVERONLY
qbyte doompalette[768];
static qboolean paletteloaded;

void Doom_LoadPalette(void)
{
	char *file;
	int greyscale;
	if (!paletteloaded)
	{
		paletteloaded = true;
		file = FS_LoadMallocFile("wad/playpal", NULL);
		if (file)
		{
			memcpy(doompalette, file, 768);
			Z_Free(file);
		}
		else
		{
			for (greyscale = 0; greyscale < 256; greyscale++)
			{
				doompalette[greyscale*3+0] = greyscale;
				doompalette[greyscale*3+1] = greyscale;
				doompalette[greyscale*3+2] = greyscale;
			}
		}
	}
}
#endif
int Doom_LoadFlat(doommap_t *dm, char *flatname)
{
#ifndef SERVERONLY
	char texname[64];
	int texnum;

	if (!strncasecmp(flatname, "F_SKY1", 6))
	{
		if (dm->skytex < 0)
		{
			sprintf(texname, "flats/f_sky1");
			dm->textures = BZ_Realloc(dm->textures, sizeof(*dm->textures)*((dm->numtextures+16)&~15));
			memset(dm->textures + dm->numtextures, 0, sizeof(dm->textures[dm->numtextures]));
			Q_strncpyz(dm->textures[dm->numtextures].name, texname, sizeof(dm->textures[dm->numtextures].name));
			dm->textures[dm->numtextures].width = 64;
			dm->textures[dm->numtextures].height = 64;
			dm->skytex = dm->numtextures++;
		}
		return dm->skytex;
	}

	sprintf(texname, "flats/%-.8s", flatname);

	for (texnum = 0; texnum < dm->numtextures; texnum++)
	{
		if (!strcmp(dm->textures[texnum].name, texname))
			return texnum;
	}
	
	dm->textures = BZ_Realloc(dm->textures, sizeof(*dm->textures)*((dm->numtextures+16)&~15));
	memset(dm->textures + dm->numtextures, 0, sizeof(dm->textures[dm->numtextures]));
	dm->numtextures++;

	Q_strncpyz(dm->textures[texnum].name, texname, sizeof(dm->textures[texnum].name));

	dm->textures[texnum].width = 64;
	dm->textures[texnum].height = 64;

	return texnum;
#else
	return 0;
#endif
}

#ifndef SERVERONLY
//pegtop is the world height at which the texture's top edge (v=0) sits, with the
//sidedef's row offset already folded in. v increases downward at 1 texel per world
//unit, so a wall vertex at world-z maps to v = (pegtop - z)/height. Expressing the
//various Doom peg rules as a single "where does the texture top sit" lets the four
//cases (upper/lower x pegged/unpegged) share one formula - see R_DrawSSector.
static void R_DrawWall(doommap_t *dm, int texnum, int s, float pegtop, float x1, float y1, float z1, float x2, float y2, float z2, unsigned int colour4b)
{
	doomtexture_t *tex = dm->textures+texnum;
	mesh_t *mesh = &tex->mesh;
	float len = sqrt((x1-x2)*(x1-x2) + (y1-y2)*(y1-y2));
	float s1, s2;
	float t1, t2;
	unsigned int col;

	s1 = s/tex->width;
	s2 = s1 + len/tex->width;

	t1 = (pegtop - z2)/tex->height;
	t2 = (pegtop - z1)/tex->height;

	if (mesh->numvertexes+4 > tex->maxverts)
	{
		if (mesh->numvertexes+4 > MAX_INDICIES)
			BE_DrawMesh_Single(tex->shader, mesh, NULL, 0);
		else
		{
			tex->maxverts = mesh->numvertexes+4;
			mesh->colors4b_array = BZ_Realloc(mesh->colors4b_array, sizeof(*mesh->colors4b_array) * tex->maxverts);
			mesh->xyz_array = BZ_Realloc(mesh->xyz_array, sizeof(*mesh->xyz_array) * tex->maxverts);
			mesh->st_array = BZ_Realloc(mesh->st_array, sizeof(*mesh->st_array) * tex->maxverts);
		}
	}
	if (mesh->numindexes+6 > tex->maxindicies)
	{
		tex->maxindicies = mesh->numindexes+6;
		mesh->indexes = BZ_Realloc(mesh->indexes, sizeof(*mesh->indexes) * tex->maxindicies);
	}

	col = colour4b * 0x01010101;
	((unsigned char*)&col)[3] = 0xff;
	*(unsigned int*)mesh->colors4b_array[mesh->numvertexes+0] = col;
	*(unsigned int*)mesh->colors4b_array[mesh->numvertexes+1] = col;
	*(unsigned int*)mesh->colors4b_array[mesh->numvertexes+2] = col;
	*(unsigned int*)mesh->colors4b_array[mesh->numvertexes+3] = col;
	VectorSet(mesh->xyz_array[mesh->numvertexes+0], x1, y1, z1);
	VectorSet(mesh->xyz_array[mesh->numvertexes+1], x1, y1, z2);
	VectorSet(mesh->xyz_array[mesh->numvertexes+2], x2, y2, z2);
	VectorSet(mesh->xyz_array[mesh->numvertexes+3], x2, y2, z1);
	Vector2Set(mesh->st_array[mesh->numvertexes+0], s1, t2);
	Vector2Set(mesh->st_array[mesh->numvertexes+1], s1, t1);
	Vector2Set(mesh->st_array[mesh->numvertexes+2], s2, t1);
	Vector2Set(mesh->st_array[mesh->numvertexes+3], s2, t2);

	mesh->indexes[mesh->numindexes+0] = mesh->numvertexes+0;
	mesh->indexes[mesh->numindexes+1] = mesh->numvertexes+1;
	mesh->indexes[mesh->numindexes+2] = mesh->numvertexes+2;

	mesh->indexes[mesh->numindexes+3] = mesh->numvertexes+0;
	mesh->indexes[mesh->numindexes+4] = mesh->numvertexes+2;
	mesh->indexes[mesh->numindexes+5] = mesh->numvertexes+3;

	mesh->numvertexes += 4;
	mesh->numindexes += 6;
}

static void R_DrawFlats(doommap_t *dm, int floortexnum, int floorheight, int ceiltexnum, int ceilheight, int numverts, unsigned short *verts, unsigned int colour4b)
{
	mesh_t *mesh;
	unsigned int col;
	unsigned int v, i;
	//floor
	{
		doomtexture_t *floortex = dm->textures + floortexnum;
		mesh = &floortex->mesh;
		if (mesh->numvertexes+numverts > floortex->maxverts)
		{
			if (mesh->numvertexes+numverts > MAX_INDICIES)
			{
				BE_DrawMesh_Single(floortex->shader, mesh, NULL, 0);
				mesh->numvertexes = 0;
				mesh->numindexes = 0;
			}
			else
			{
				floortex->maxverts = mesh->numvertexes+numverts;
				mesh->colors4b_array = BZ_Realloc(mesh->colors4b_array, sizeof(*mesh->colors4b_array) * floortex->maxverts);
				mesh->xyz_array = BZ_Realloc(mesh->xyz_array, sizeof(*mesh->xyz_array) * floortex->maxverts);
				mesh->st_array = BZ_Realloc(mesh->st_array, sizeof(*mesh->st_array) * floortex->maxverts);
			}
		}
		if (mesh->numindexes+numverts > floortex->maxindicies)
		{
			floortex->maxindicies = mesh->numindexes+numverts;
			mesh->indexes = BZ_Realloc(mesh->indexes, sizeof(*mesh->indexes) * floortex->maxindicies);
		}

		col = colour4b * 0x01010101;
		((unsigned char*)&col)[3] = 0xff;

		for (i = 0; i < numverts; i++)
		{
			v = verts[i];
			VectorSet(mesh->xyz_array[mesh->numvertexes+i], dm->vertexes[v].xpos, dm->vertexes[v].ypos, floorheight);
			Vector2Set(mesh->st_array[mesh->numvertexes+i], dm->vertexes[v].xpos/64.0f, dm->vertexes[v].ypos/64.0f);
			*(unsigned int*)mesh->colors4b_array[mesh->numvertexes+i] = col;
		}

		for (i = 0; i < numverts; i++)
		{
			mesh->indexes[mesh->numindexes+i] = mesh->numvertexes+i;
		}

		mesh->numvertexes += numverts;
		mesh->numindexes += numverts;

//		if (floortex->shader)
//			BE_DrawMesh_Single(floortex->shader, mesh, NULL, 0);
	}

	//ceiling
	{
		doomtexture_t *ceiltex = dm->textures + ceiltexnum;
		mesh = &ceiltex->mesh;
		if (mesh->numvertexes+numverts > ceiltex->maxverts)
		{
			if (mesh->numvertexes+numverts > MAX_INDICIES)
			{
				BE_DrawMesh_Single(ceiltex->shader, mesh, NULL, 0);
				mesh->numvertexes = 0;
				mesh->numindexes = 0;
			}
			else
			{
				ceiltex->maxverts = mesh->numvertexes+numverts;
				mesh->colors4b_array = BZ_Realloc(mesh->colors4b_array, sizeof(*mesh->colors4b_array) * ceiltex->maxverts);
				mesh->xyz_array = BZ_Realloc(mesh->xyz_array, sizeof(*mesh->xyz_array) * ceiltex->maxverts);
				mesh->st_array = BZ_Realloc(mesh->st_array, sizeof(*mesh->st_array) * ceiltex->maxverts);
			}
		}
		if (mesh->numindexes+numverts > ceiltex->maxindicies)
		{
			ceiltex->maxindicies = mesh->numindexes+numverts;
			mesh->indexes = BZ_Realloc(mesh->indexes, sizeof(*mesh->indexes) * ceiltex->maxindicies);
		}

		col = colour4b * 0x01010101;
		((unsigned char*)&col)[3] = 0xff;

		for (i = 0; i < numverts; i++)
		{
			v = verts[numverts-1-i];
			VectorSet(mesh->xyz_array[mesh->numvertexes+i], dm->vertexes[v].xpos, dm->vertexes[v].ypos, ceilheight);
			Vector2Set(mesh->st_array[mesh->numvertexes+i], dm->vertexes[v].xpos/64.0f, dm->vertexes[v].ypos/64.0f);
			*(unsigned int*)mesh->colors4b_array[mesh->numvertexes+i] = col;
		}

		for (i = 0; i < numverts; i++)
		{
			mesh->indexes[mesh->numindexes+i] = mesh->numvertexes+i;
		}

		mesh->numvertexes += numverts;
		mesh->numindexes += numverts;

//		if (ceiltex->shader)
//			BE_DrawMesh_Single(ceiltex->shader, mesh, NULL, 0);
	}
}

static void R_DrawSSector(doommap_t *dm, unsigned int ssec)
{
	short v0, v1;
	int sd;
	dlinedef_t *ld;
	int seg;
	msector_t *sec, *sec2;

	for (seg = dm->ssector[ssec].first + dm->ssector[ssec].segcount-1; seg >= dm->ssector[ssec].first; seg--)
		if (dm->seg[seg].linedef != 0xffff)
			break;
	sec = dm->sector + dm->sidedef[dm->linedef[dm->seg[seg].linedef].sidedef[dm->seg[seg].direction]].sector;

	if (sec->visframe != r_visframecount)
	{
		R_DrawFlats(dm, sec->floortex, sec->floorheight, sec->ceilingtex, sec->ceilingheight, sec->numflattris*3, sec->flats, sec->lightlev);

		sec->visframe = r_visframecount;
	}
	for (seg = dm->ssector[ssec].first + dm->ssector[ssec].segcount-1; seg >= dm->ssector[ssec].first; seg--)
	{
		if (dm->seg[seg].linedef == 0xffff)
			continue;

		v0 = dm->seg[seg].vert[0];
		v1 = dm->seg[seg].vert[1];
		if (v0==v1)
			continue;
		ld = dm->linedef + dm->seg[seg].linedef;
		sd = ld->sidedef[dm->seg[seg].direction];

		if (ld->sidedef[1] != 0xffff)	//we can see through this linedef
		{
			//offsets come from THIS side's sidedef (sd) - the one whose textures we draw -
			//the same as the middle/solid cases below. (It used to read the far sidedef.)
			float ro = dm->sidedef[sd].texy;
			float so = dm->sidedef[sd].texx;
			sec2 = dm->sector + dm->sidedef[ld->sidedef[1-dm->seg[seg].direction]].sector;

			if (sec->floorheight < sec2->floorheight)
			{	//lower texture - fills the step from our floor up to the back (higher) floor.
				int ltex = dm->sidedef[sd].lowertex;
				//default: texture top at the higher floor (top of the step). lower-unpegged
				//(ML_DONTPEGBOTTOM): peg to our ceiling so it stays continuous with the wall above.
				float pegtop = ((ld->flags & LINEDEF_LOWERUNPEGGED) ? sec->ceilingheight : sec2->floorheight) + ro;
				R_DrawWall(dm, ltex, so, pegtop,
					dm->vertexes[v0].xpos, dm->vertexes[v0].ypos, sec->floorheight,
					dm->vertexes[v1].xpos, dm->vertexes[v1].ypos, sec2->floorheight, sec->lightlev);
			}

			if (sec->ceilingheight > sec2->ceilingheight)
			{
				//Doom sky hack: a wall between two sky-ceilinged sectors shows SKY, not its upper
				//texture. We draw the quad into the sky texture's mesh so it (a) occludes the map
				//geometry behind it - its depth is forced like the other sky surfaces in R_DoomWorld -
				//and (b) shows the skydome. Drawing the actual upper texture (often "none"/black) here
				//gave a black gap; simply skipping it let you see through the map to the far side.
				if (sec->ceilingtex == dm->skytex && sec2->ceilingtex == dm->skytex)
					R_DrawWall(dm, dm->skytex, 0, sec->ceilingheight,
						dm->vertexes[v0].xpos, dm->vertexes[v0].ypos, sec2->ceilingheight,
						dm->vertexes[v1].xpos, dm->vertexes[v1].ypos, sec->ceilingheight, sec->lightlev);
				else
				{	//upper texture - fills from the back (lower) ceiling up to our ceiling.
					int utex = dm->sidedef[sd].uppertex;
					//default (Doom): texture BOTTOM at the lower ceiling. upper-unpegged
					//(ML_DONTPEGTOP): texture TOP at our (higher) ceiling.
					float pegtop = (ld->flags & LINEDEF_UPPERUNPEGGED)
						? sec->ceilingheight + ro
						: sec2->ceilingheight + dm->textures[utex].height + ro;
					R_DrawWall(dm, utex, so, pegtop,
						dm->vertexes[v0].xpos, dm->vertexes[v0].ypos, sec2->ceilingheight,
						dm->vertexes[v1].xpos, dm->vertexes[v1].ypos, sec->ceilingheight, sec->lightlev);
				}
			}

			if (dm->sidedef[sd].middletex)
			{	//mid texture on a 2-sided line (grates/bars). Keep prior pegging (top of the drawn
				//span) using the existing swapped vertex order: pegtop = span bottom + rowoffset.
				float ztop = (sec2->ceilingheight < sec->ceilingheight)?sec2->ceilingheight:sec->ceilingheight;
				float zbot = (sec2->floorheight > sec->floorheight)?sec2->floorheight:sec->floorheight;
				R_DrawWall(dm, dm->sidedef[sd].middletex, so, zbot + ro,
					dm->vertexes[v1].xpos, dm->vertexes[v1].ypos, ztop,
					dm->vertexes[v0].xpos, dm->vertexes[v0].ypos, zbot, sec->lightlev);
			}
		}
		else
		{	//solid wall, draw full wall.
			int mtex = dm->sidedef[sd].middletex;
			//default: texture TOP at the ceiling. lower-unpegged: texture BOTTOM at the floor.
			float pegtop = (ld->flags & LINEDEF_LOWERUNPEGGED)
				? sec->floorheight + dm->textures[mtex].height + dm->sidedef[sd].texy
				: sec->ceilingheight + dm->sidedef[sd].texy;
			R_DrawWall(dm, mtex, dm->sidedef[sd].texx, pegtop,
				dm->vertexes[v0].xpos, dm->vertexes[v0].ypos, sec->floorheight,
				dm->vertexes[v1].xpos, dm->vertexes[v1].ypos, sec->ceilingheight, sec->lightlev);
		}
	}
}

mplane_t	frustum2d[2];
static int Box2DOnPlaneSide (short emins[2], short emaxs[2], mplane_t *p)
{
	float	dist1, dist2;
	int		sides;

// general case
	switch (p->signbits)
	{
	case 0:
dist1 = p->normal[0]*emaxs[0] + p->normal[1]*emaxs[1];
dist2 = p->normal[0]*emins[0] + p->normal[1]*emins[1];
		break;
	case 1:
dist1 = p->normal[0]*emins[0] + p->normal[1]*emaxs[1];
dist2 = p->normal[0]*emaxs[0] + p->normal[1]*emins[1];
		break;
	case 2:
dist1 = p->normal[0]*emaxs[0] + p->normal[1]*emins[1];
dist2 = p->normal[0]*emins[0] + p->normal[1]*emaxs[1];
		break;
	case 3:
dist1 = p->normal[0]*emins[0] + p->normal[1]*emins[1];
dist2 = p->normal[0]*emaxs[0] + p->normal[1]*emaxs[1];
		break;
	case 4:
dist1 = p->normal[0]*emaxs[0] + p->normal[1]*emaxs[1];
dist2 = p->normal[0]*emins[0] + p->normal[1]*emins[1];
		break;
	case 5:
dist1 = p->normal[0]*emins[0] + p->normal[1]*emaxs[1];
dist2 = p->normal[0]*emaxs[0] + p->normal[1]*emins[1];
		break;
	case 6:
dist1 = p->normal[0]*emaxs[0] + p->normal[1]*emins[1];
dist2 = p->normal[0]*emins[0] + p->normal[1]*emaxs[1];
		break;
	case 7:
dist1 = p->normal[0]*emins[0] + p->normal[1]*emins[1];
dist2 = p->normal[0]*emaxs[0] + p->normal[1]*emaxs[1];
		break;
	default:
		dist1 = dist2 = 0;		// shut up compiler
//		BOPS_Error ();
		break;
	}

	sides = 0;
	if (dist1 >= p->dist)
		sides = 1;
	if (dist2 < p->dist)
		sides |= 2;

#ifdef PARANOID
if (sides == 0)
	Sys_Error ("Box2DOnPlaneSide: sides==0");
#endif

	return sides;
}
static qboolean R_Cull2DBox (short mins_x, short mins_y, short maxs_x, short maxs_y)
{
	short mins[2], maxs[2];
	int		i;
//return false;
	mins[0] = mins_x;
	mins[1] = mins_y;
	maxs[0] = maxs_x;
	maxs[1] = maxs_y;

	for (i=0 ; i<2 ; i++)
		if (Box2DOnPlaneSide (mins, maxs, &frustum2d[i]) == 2)
			return true;
	return false;
}

void R_Set2DFrustum (void)
{
	int		i;
	vec3_t vpn, vright, vup, viewang;

	if ((int)r_novis.value & 4)
		return;

	viewang[0] = 0;
	viewang[1] = r_refdef.viewangles[1];
	viewang[2] = 0;
	AngleVectors (viewang, vpn, vright, vup);

/*	if (r_refdef.fov_x == 90) 
	{
		// front side is visible

		VectorAdd (vpn, vright, frustum2d[0].normal);
		VectorSubtract (vpn, vright, frustum2d[1].normal);
	}
	else*/
	{

		// rotate VPN right by FOV_X/2 degrees
		RotatePointAroundVector( frustum2d[0].normal, vup, vpn, -(90-r_refdef.fov_x / 2 ) );
		// rotate VPN left by FOV_X/2 degrees
		RotatePointAroundVector( frustum2d[1].normal, vup, vpn, 90-r_refdef.fov_x / 2 );
	}

	for (i=0 ; i<2 ; i++)
	{
		frustum2d[i].type = PLANE_ANYZ;
		frustum2d[i].dist = DotProduct (r_origin, frustum2d[i].normal);
		frustum2d[i].signbits = SignbitsForPlane (&frustum2d[i]);
	}
}


static void R_RecursiveDoomNode(doommap_t *dm, unsigned int node)
{
	if (node & NODE_IS_SSECTOR)
	{
		R_DrawSSector(dm, node & ~NODE_IS_SSECTOR);		

		return;
	}

	if (!R_Cull2DBox(dm->node[node].x1lower, dm->node[node].y1lower, dm->node[node].x1upper, dm->node[node].y1upper))
		R_RecursiveDoomNode(dm, dm->node[node].node1);
	if (!R_Cull2DBox(dm->node[node].x2lower, dm->node[node].y2lower, dm->node[node].x2upper, dm->node[node].y2upper))
		R_RecursiveDoomNode(dm, dm->node[node].node2);
}

static void R_DoomDrawMonsters(doommap_t *dm);	//defined with the monster code, below

//draw item/decoration things as camera-facing vertical billboards using their Doom
//sprites. Called after the opaque world so they depth-test against it; alpha-tested so
//the transparent sprite background is masked with crisp edges.
static void R_DoomDrawSprites(doommap_t *dm)
{
	unsigned int i;
	vec3_t viewang, vpn, vright, vup;
	mesh_t m;
	vecV_t xyz[4];
	vec2_t st[4] = {{0,0},{1,0},{1,1},{0,1}};
	byte_vec4_t col[4];
	index_t idx[6] = {0,1,2, 0,2,3};

	int usevox; float voxscale, voxyaw;
	if (!dm->numsprites)
		return;
	usevox  = (int)Cvar_Get("doom_voxels", "1", CVAR_ARCHIVE, "Doom")->value;
	voxscale= Cvar_Get("doom_voxscale", "1", CVAR_ARCHIVE, "Doom")->value;
	voxyaw  = Cvar_Get("doom_voxyaw", "90", CVAR_ARCHIVE, "Doom")->value;
	if (usevox) Doom_VoxShader();

	//cylindrical billboard: sprites stay upright and turn to face the player horizontally
	viewang[0] = 0;
	viewang[1] = r_refdef.viewangles[1];
	viewang[2] = 0;
	AngleVectors(viewang, vpn, vright, vup);

	Vector4Set(col[0], 255,255,255,255);
	Vector4Set(col[1], 255,255,255,255);
	Vector4Set(col[2], 255,255,255,255);
	Vector4Set(col[3], 255,255,255,255);

	memset(&m, 0, sizeof(m));
	m.numvertexes = 4;
	m.numindexes = 6;
	m.xyz_array = xyz;
	m.st_array = st;
	m.colors4b_array = col;
	m.indexes = idx;

	for (i = 0; i < dm->numsprites; i++)
	{
		struct doomsprite_s *s = &dm->sprites[i];
		float zb = s->origin[2];	//rest the sprite's bottom on the floor (Doom items sit on the ground)
		float zt = zb + s->h;
		vec3_t l, r;
		if (usevox && s->voxname[0] && Doom_DrawVoxelByName(s->voxname, s->origin, voxyaw, voxscale))
			continue;	//rendered as a voxel; otherwise fall back to the sprite billboard
		if (!s->shader)
			continue;
		VectorMA(s->origin, -s->xo,        vright, l);	//left edge (origin column = leftoffset)
		VectorMA(s->origin,  s->w - s->xo, vright, r);	//right edge
		VectorSet(xyz[0], l[0], l[1], zt);
		VectorSet(xyz[1], r[0], r[1], zt);
		VectorSet(xyz[2], r[0], r[1], zb);
		VectorSet(xyz[3], l[0], l[1], zb);
		BE_DrawMesh_Single(s->shader, &m, NULL, 0);
	}
}

void R_DoomWorld(void)
{
	model_t *mod = cl.worldmodel;
	doommap_t *dm = mod->meshinfo;
	int texnum;
	doomtexture_t *t;
	if (!dm->node || !dm->numnodes)
		return;	//err... buggy

	for (texnum = 0; texnum < dm->numtextures; texnum++)	//a hash table might be a good plan.
	{
		t = &dm->textures[texnum];
		t->mesh.numindexes = 0;
		t->mesh.numvertexes = 0;
	}
	R_Set2DFrustum();
	r_visframecount++;
	R_RecursiveDoomNode(dm, dm->numnodes-1);

	memset(mod->batches, 0, sizeof(mod->batches));

	//Draw the sky FIRST (all F_SKY1 ceilings/floors + sky-hack walls are collected in the skytex
	//mesh). R_DrawSkyChain paints the skydome as a backdrop; then we force the sky surfaces' own
	//depth so that map geometry behind them is occluded. Doom maps set allow_unmaskedskyboxes, which
	//disables FTE's built-in sky depth-masking, so without this you'd see through sky-hack walls to
	//the far side of the map. Doing it before the world geometry means that geometry then depth-tests
	//against the sky and is hidden where it sits behind a sky surface.
	if (dm->skytex >= 0)
	{
		t = &dm->textures[dm->skytex];
		if (t->mesh.numindexes && t->shader)
		{
			t->meshptr = &t->mesh;
			t->batch.mesh = &t->meshptr;
			t->batch.firstmesh = 0;
			t->batch.meshes = 1;
			t->batch.shader = t->shader;
			t->batch.ent = &r_worldentity;
			R_DrawSkyChain(&t->batch);
			BE_SelectMode(BEM_DEPTHONLY);
			BE_DrawMesh_Single(t->shader, &t->mesh, NULL, 0);
			BE_SelectMode(BEM_STANDARD);
		}
	}

	for (texnum = 0; texnum < dm->numtextures; texnum++)	//a hash table might be a good plan.
	{
		if (texnum == dm->skytex)
			continue;	//sky already drawn (as a depth-masked backdrop) above
		t = &dm->textures[texnum];
		if (t->mesh.numindexes && t->shader)
		{
			t->batch.next = mod->batches[t->shader->sort];
			mod->batches[t->shader->sort] = &t->batch;

			BE_DrawMesh_Single(t->shader, &t->mesh, NULL, 0);
		}
	}

	R_DoomDrawSprites(dm);	//item/decoration billboards, over the opaque world
	R_DoomDrawMonsters(dm);	//monsters (billboards), over the opaque world
	R_DoomDrawHUD(dm);
}

//draw the first-person weapon sprite (HUD). Picks the lump from the player's equipped weapon
//stat and cycles frames if they are firing.
static void R_DoomDrawHUD(doommap_t *dm)
{
	int wi, fi;
	char lump[16];
	short w, h, xo, yo;
	shader_t *sh;
	mesh_t mesh;
	vecV_t xyz[4];
	vec2_t st[4] = {{0,0},{1,0},{1,1},{0,1}};
	byte_vec4_t col[4];
	index_t idx[6] = {0,1,2, 0,2,3};
	float screen_w = (float)r_refdef.vrect.width;
	float screen_h = (float)r_refdef.vrect.height;
	float scale;
	const char *anims[] = {"ABCD", "AB", "ABCD", "ABCDEFGH", "ABCDEFGHIJKL", "AB", "AB", "AB", "AB"};
	const char *names[] = {"PUN", "SAW", "PIS", "SHT", "SHT2", "CHG", "MIS", "PLS", "BFG"};

	//get equipped weapon and frame from stats
	wi = (int)cl.playerview[r_refdef.playerview - cl.playerview].stats[STAT_ACTIVEWEAPON];
	fi = (int)cl.playerview[r_refdef.playerview - cl.playerview].stats[STAT_WEAPONFRAME];
	if (wi < 0 || wi >= 9) wi = 2; //pistol default

	//pick frame letter from anim string
	char frame = 'A';
	if (fi >= 0 && (unsigned)fi < strlen(anims[wi]))
		frame = anims[wi][fi];

	Q_snprintfz(lump, sizeof(lump), "%sG%c0", names[wi], frame);
	sh = Doom_MonsterSpriteShader(lump, &w, &h, &xo, &yo);
	if (!sh) return;

	//draw as a 2D overlay in the bottom center
	scale = screen_h / 200.0f; //Doom internal res is 320x200
	VectorSet(xyz[0], screen_w/2.0f - (xo*scale), screen_h - (h-yo)*scale, 0);
	VectorSet(xyz[1], screen_w/2.0f + (w-xo)*scale, screen_h - (h-yo)*scale, 0);
	VectorSet(xyz[2], screen_w/2.0f + (w-xo)*scale, screen_h + yo*scale, 0);
	VectorSet(xyz[3], screen_w/2.0f - (xo*scale), screen_h + yo*scale, 0);

	Vector4Set(col[0],255,255,255,255); Vector4Set(col[1],255,255,255,255);
	Vector4Set(col[2],255,255,255,255); Vector4Set(col[3],255,255,255,255);
	
	memset(&mesh, 0, sizeof(mesh));
	mesh.numvertexes=4; mesh.numindexes=6;
	mesh.xyz_array=xyz; mesh.st_array=st; mesh.colors4b_array=col; mesh.indexes=idx;

	BE_SelectMode(BEM_STANDARD);
	BE_DrawMesh_Single(sh, &mesh, NULL, 0);
}
#endif


//find the first ssector, go through it's list/
//grab the lines into multiple arrays.
//make sure all arrays are looped fully. If not, error out.
//if we have two arrays, we have a hole in the middle.
//with multiple arrays, from the second onwards
//	grab two adjacent verts and find the nearest point in any other array, that is also on the positive side.
//	One of the two should be an extreeme, and the external point should be in the direction that the angle points at.
//		none found = error
//	create a triangle from these points, fix array links.
//	move on to next spare array.
//we now have a concave polygon with no holes.
//pick a point, follow along the walls making a triangle fan, until an angle of > 180, throw out fan, rebuild arrays.
//at new point, start a new fan. Be prepared to not be able to generate one.

#define MAX_REGIONS		256
#define MAX_POLYVERTS	(MAX_FLATTRIS*3)
#define MAX_FLATTRIS	1024

//buffer to hold tris
static unsigned short indexes[MAX_POLYVERTS];
static unsigned int numindexes;

typedef struct {
	int vertex[MAX_POLYVERTS];
	unsigned int numverts;
	float angle;
} conectedregion_t;
static conectedregion_t polyregions[MAX_REGIONS];	//we need to be able to join them as we go.
static unsigned int regions;

//throw out duplicates.
static void Triangulate_AddLine(int v1, int v2)	//order makes a difference
{
	int r, v;
	int beginingof = -1;
	int endof = -1;
	int freer = -1;

	for (r = 0; r < regions; r++)
	{
		if (!polyregions[r].numverts)
		{
			freer = r;
			continue;
		}
		if (polyregions[r].vertex[0] == v2)
			beginingof = r;
		if (polyregions[r].vertex[polyregions[r].numverts-1] == v1)
			endof = r;

		for (v = polyregions[r].numverts-2; v >= 0; v--)
			if (polyregions[r].vertex[v] == v1 && polyregions[r].vertex[v+1] == v2)
				return;	//whoops. Duplicate line.
	}
	if (beginingof >= 0 && endof >= 0)
	{	//merge two regions. Copy one onto the end of the other.
		if (beginingof == endof)
		{	//close up
			if (polyregions[endof].numverts+1 >= MAX_POLYVERTS)
			{
				Con_Printf(CON_WARNING "WARNING: Map region is too large.\n");
				return;
			}
			polyregions[endof].vertex[polyregions[endof].numverts] = v2;
			polyregions[endof].numverts++;
		}
		else
		{
			if (polyregions[endof].numverts+polyregions[beginingof].numverts >= MAX_POLYVERTS)
			{
				Con_Printf(CON_WARNING "WARNING: Map region is too large.\n");
				return;
			}
			memcpy(polyregions[endof].vertex + polyregions[endof].numverts,
				polyregions[beginingof].vertex,
				sizeof(polyregions[beginingof].vertex[0])*polyregions[beginingof].numverts);
			polyregions[endof].numverts += polyregions[beginingof].numverts;
			polyregions[beginingof].numverts = 0;
		}
	}
	else if (beginingof >= 0)
	{	//insert into
		if (polyregions[beginingof].numverts+1 >= MAX_POLYVERTS)
		{
			Con_Printf(CON_WARNING "WARNING: Map region is too large.\n");
			return;
		}

		memmove(polyregions[beginingof].vertex + 1,
			polyregions[beginingof].vertex,
			sizeof(polyregions[beginingof].vertex[0])*polyregions[beginingof].numverts);
		polyregions[beginingof].vertex[0] = v1;
		polyregions[beginingof].numverts++;
	}
	else if (endof >= 0)
	{	//stick outselves on the end
		if (polyregions[endof].numverts+1 >= MAX_POLYVERTS)
		{
			Con_Printf(CON_WARNING "WARNING: Map region is too large.\n");
			return;
		}
		polyregions[endof].vertex[polyregions[endof].numverts] = v2;
		polyregions[endof].numverts++;
	}
	else
	{	//new region.
		if (freer < 0)
		{
			freer = regions++;
			if (regions > MAX_REGIONS)
			{
				Con_Printf(CON_WARNING "WARNING: Too many regions. Sector is too chaotic/complicated.\n");
				freer = 0;
				regions = 1;
			}
		}

		polyregions[freer].numverts = 2;
		polyregions[freer].vertex[0] = v1;
		polyregions[freer].vertex[1] = v2;
	}
}

static unsigned short *Triangulate_Finish(doommap_t *dm, int *numtris, unsigned short *old, int oldindexcount)
{
	unsigned short *out;
	unsigned int v1, v2, v3, v;
	unsigned int r, v2s, f;
	float a1;
	float a2;
	for (r = 0; r < regions; r++)
	{
		if (!polyregions[r].numverts)
			continue;

		if (polyregions[r].vertex[0] != polyregions[r].vertex[polyregions[r].numverts-1])
		{
			Con_DPrintf("Sector is not enclosed\n");	//common+harmless for maps without glBSP gl_nodes; we auto-close the loop below. developer-only to avoid spamming hundreds of lines per map load.
			polyregions[r].vertex[polyregions[r].numverts] = polyregions[r].vertex[0];
			polyregions[r].numverts++;

			/*
			*numtris = 0;
			regions = 0;


			return NULL;*/
		}

		polyregions[r].angle = 0;
		polyregions[r].numverts--;//start == end
		for (v = 0; v < polyregions[r].numverts; v++)
		{
			v1 = polyregions[r].vertex[v];
			v2 = polyregions[r].vertex[(v+1)%(polyregions[r].numverts)];
			v3 = polyregions[r].vertex[(v+2)%(polyregions[r].numverts)];
			a1 = atan2(dm->vertexes[v3].ypos - dm->vertexes[v2].ypos, dm->vertexes[v3].xpos - dm->vertexes[v2].xpos);
			a2 = atan2(dm->vertexes[v1].ypos - dm->vertexes[v2].ypos, dm->vertexes[v1].xpos - dm->vertexes[v2].xpos);
			polyregions[r].angle += fabs(a1 - a2);
		}
	}

	//FIXME: inner loops should find the nearest point in a forwards direction from one of the extreeme points.

	//angle should be either (numverts-2)*PI	//inner loop
	//or PI*numverts+2*PI						//outer loop
	//unfortuantly it's rarly either of them...

	for (r = 0; r < regions; r++)
	{
		if (polyregions[r].numverts<3)
			continue;
		v1 = polyregions[r].vertex[0];
		v2 = polyregions[r].vertex[1];
		v2s = 1;
		f=0;
		for (v = 2; polyregions[r].numverts>=3; )
		{	//build a triangle fan.
			if (numindexes+3 > MAX_POLYVERTS)
			{
				Con_Printf(CON_WARNING "WARNING: Sector is too big for triangulation\n");
				break;
			}
			v3 = polyregions[r].vertex[v];

			a1 = atan2(dm->vertexes[v3].ypos - dm->vertexes[v2].ypos, dm->vertexes[v3].xpos - dm->vertexes[v2].xpos);
			a2 = atan2(dm->vertexes[v1].ypos - dm->vertexes[v2].ypos, dm->vertexes[v1].xpos - dm->vertexes[v2].xpos);
			if (fabs(a1-a2) > M_PI+0.01)	//this would be a reflex angle then.;.
			{
/*				indexes[numindexes++] = 0;
				indexes[numindexes++] = v2;
				indexes[numindexes++] = 1;
*/
				v1 = v2;
				v2 = v3;
				v2s = v;
				v=(v+1)%polyregions[r].numverts;
				f++;
				if (f >= 1000)
				{	//infinate loop - shouldn't happen. must have got the angle stuff wrong.
					Con_Printf(CON_WARNING "WARNING: Failed to triangulate polygon\n");
					break;
				}
				continue;
			}

			//FIXME: make sure v1 -> v3 doesn't clip any same-region lines.

			indexes[numindexes++] = v1;
			indexes[numindexes++] = v2;
			indexes[numindexes++] = v3;
			memmove(polyregions[r].vertex+v2s, polyregions[r].vertex+v2s+1, (polyregions[r].numverts-v2s)*sizeof(polyregions[r].vertex[0]));
			polyregions[r].numverts--;
			v=(v)%polyregions[r].numverts;
			v2 = v3;
			v2s = v;
			polyregions[r].vertex[polyregions[r].numverts] = 0;
		}
	}

	if (!numindexes)
	{
		Con_DPrintf(CON_WARNING "Warning: Sector is empty\n");

		*numtris = 0;
		regions = 0;

		return NULL;
	}

	out = BZ_Realloc(old, sizeof(*out)*(numindexes+oldindexcount*3));
	memcpy(out+oldindexcount*3, indexes, sizeof(*out)*numindexes);

	*numtris = numindexes/3+oldindexcount;
	regions = 0;
	numindexes = 0;

	return out;
}

//For glBSP GL subsectors each subsector is already a convex polygon whose segs
//(real linedefs + GL minisegs) form a single ordered loop. Triangulate it directly
//as a fan from its first vertex and append to the owning sector. This is robust and
//avoids the angle-based ear-clipper in Triangulate_Finish, whose atan2 reflex test
//(fabs(a1-a2) > M_PI) false-flags a convex vertex whenever the two edge angles
//straddle the +-PI wrap. That false reflex made it skip a vertex and emit an
//overlapping/folded triangle - the stray "shiny" z-fighting flat tri in eg the
//e1m1 nukage pool.
static void Triangulate_ConvexSSector(doommap_t *dm, int nsec, unsigned int first, unsigned int segcount)
{
	unsigned int k, ntris, base;
	unsigned short *out, v0;
	if (segcount < 3)
		return;	//degenerate - nothing to draw.
	ntris = segcount - 2;
	base = dm->sector[nsec].numflattris;
	out = BZ_Realloc(dm->sector[nsec].flats, sizeof(*out)*(base+ntris)*3);
	v0 = dm->seg[first].vert[0];	//consecutive seg vert[0]s are the polygon's ordered vertices.
	for (k = 1; k <= segcount-2; k++)
	{
		out[(base+k-1)*3+0] = v0;
		out[(base+k-1)*3+1] = dm->seg[first+k].vert[0];
		out[(base+k-1)*3+2] = dm->seg[first+k+1].vert[0];
	}
	dm->sector[nsec].flats = out;
	dm->sector[nsec].numflattris = base + ntris;
}

static void Triangulate_Sectors(doommap_t *dm, dsector_t *sectorl, qboolean glbspinuse)
{
	int seg, nsec;
	int i, sec=-1;

	if (glbspinuse)
	{
		for (i = 0; i < dm->numssectors; i++)
		{	//each GL subsector is its own convex polygon - triangulate it independently
			//(see Triangulate_ConvexSSector) so a sector's flats never extend into an
			//embedded sector (pool/carpet) and we never fold a bad overlapping triangle.
			for (seg = dm->ssector[i].first; seg < dm->ssector[i].first + dm->ssector[i].segcount; seg++)
				if (dm->seg[seg].linedef != 0xffff)
					break;

			if (seg == dm->ssector[i].first + dm->ssector[i].segcount)	//no real linedef -> can't find its sector.
			{
				Con_DPrintf("SubSector %i has absolutly no walls\n", i);
				continue;
			}

			nsec = dm->sidedef[dm->linedef[dm->seg[seg].linedef].sidedef[dm->seg[seg].direction]].sector;
			Triangulate_ConvexSSector(dm, nsec, dm->ssector[i].first, dm->ssector[i].segcount);
		}
	}
	else
	{
		// Use SSectors (convex subregions) so that sector A's triangulation never
		// extends into embedded sector B's area (pool, carpet patches, etc.).
		for (i = 0; i < dm->numssectors; i++)
		{
			for (seg = dm->ssector[i].first; seg < dm->ssector[i].first + dm->ssector[i].segcount; seg++)
				if (dm->seg[seg].linedef != 0xffff)
					break;
			if (seg == dm->ssector[i].first + dm->ssector[i].segcount)
				continue;

			nsec = dm->sidedef[dm->linedef[dm->seg[seg].linedef].sidedef[dm->seg[seg].direction]].sector;
			if (sec != nsec)
			{
				if (sec >= 0)
					dm->sector[sec].flats = Triangulate_Finish(dm, &dm->sector[sec].numflattris, dm->sector[sec].flats, dm->sector[sec].numflattris);
				sec = nsec;
			}
			for (seg = dm->ssector[i].first; seg < dm->ssector[i].first + dm->ssector[i].segcount; seg++)
				Triangulate_AddLine(dm->seg[seg].vert[0], dm->seg[seg].vert[1]);
		}
		if (sec >= 0)
			dm->sector[sec].flats = Triangulate_Finish(dm, &dm->sector[sec].numflattris, dm->sector[sec].flats, dm->sector[sec].numflattris);
	}

	/*
	for (i = 0; i < ssectorsc; i++)
	{	//only do linedefs.
		seg = dm->ssector[i].first;
		nsec = dm->sidedef[dm->linedef[dm->seg[seg].linedef].sidedef[dm->seg[seg].direction]].sector;
		if (sec != nsec)
		{
			if (sec>=0)
				dm->sector[sec].flats = Triangulate_Finish(&dm->sector[sec].numflattris);
			sec = nsec;
		}
		for (seg = dm->ssector[i].first; seg < dm->ssector[i].first + dm->ssector[i].segcount; seg++)
		{	//ignore direction, it's do do with the intersection rather than the draw direction.
			Triangulate_AddLine(dm->seg[seg].vert[0], dm->seg[seg].vert[1]);
		}
	}
	if (sec>=0)
		dm->sector[sec].flats = Triangulate_Finish(&dm->sector[sec].numflattris);
	*/

	for (i = 0; i < dm->numsectors; i++)
	{
		dm->sector[i].ceilingtex = Doom_LoadFlat(dm, sectorl[i].ceilingtexture);
		dm->sector[i].floortex = Doom_LoadFlat(dm, sectorl[i].floortexture);
		dm->sector[i].lightlev = sectorl[i].lightlevel;
		dm->sector[i].specialtype = sectorl[i].specialtype;
		dm->sector[i].tag = sectorl[i].tag;
		dm->sector[i].ceilingheight = sectorl[i].ceilingheight;
		dm->sector[i].floorheight = sectorl[i].floorheight;
	}
}

#ifndef SERVERONLY
static void *textures1;
static void *textures2;
static char *pnames;
static void Doom_LoadTextureInfos(void)
{
	textures1 = FS_LoadMallocFile("wad/texture1", NULL);
	textures2 = FS_LoadMallocFile("wad/texture2", NULL);
	pnames = FS_LoadMallocFile("wad/pnames", NULL);
}

typedef struct {
	char name[8];
	short always0_0;
	short always0_1;
	short width;
	short height;
	short always0_2;
	short always0_3;
	short componantcount;
} ddoomtexture_t;
typedef struct {
	short xoffset;
	short yoffset;
	unsigned short patchnum;
	unsigned short always_1;
	unsigned short always_0;
} ddoomtexturecomponant_t;

typedef struct {
	short width;
	short height;
	short xpos;
	short ypos;
} doomimage_t;

static void Doom_ExtractPName(unsigned int *out, doomimage_t *di, size_t imgsize, int outwidth, int outheight, int x, int y)
{
	unsigned int *colpointers;
	int c, fr, rc, extra;
	unsigned char *data, *coldata;
	extern qbyte		gammatable[256];

	if (!di)
		return;

	data = (char *)di;


//	out += x/*+di->xpos*/;
//	out += (y/*+di->ypos*/)*outwidth;

	colpointers = (unsigned int*)(data+sizeof(doomimage_t));
	for (c = 0; c < di->width; c++)
	{
		if (c+x < 0)
			continue;
		if (c+x >= outwidth)
			break;

		if (colpointers[c] >= imgsize)
			break;
		coldata = data + colpointers[c];
		while(1)
		{
			fr = *coldata++;
			if (fr == 255)
				break;

			rc = *coldata++;

			coldata++;	//one not drawn, on each side

			fr+=y;

			if (fr<0)
			{
				coldata += -fr;	//plus
				rc -= -fr;
				fr = 0;
			}

			if ((fr+rc) > outheight)
			{
				extra = rc - (outheight - fr) +1;
				rc = outheight - fr;
				if (rc < 0)
					break;
			}
			else
				extra = 1;

			while(rc)
			{
				out[c+x + fr*outwidth] = (gammatable[doompalette[*coldata*3]]) + (gammatable[doompalette[*coldata*3+1]]<<8) + (gammatable[doompalette[*coldata*3+2]]<<16) + (255<<24);
				coldata++;
				fr++;
				rc--;
			}

			coldata+=extra; //one not drawn, on each side
		}
	}
}

static texid_t Doom_LoadPatchFromTexWad(char *name, void *texlump, unsigned short *width, unsigned short *height, qboolean *hasalpha)
{
	char patch[32] = "patches/";
	unsigned int *tex;
	ddoomtexture_t *tx;
	ddoomtexturecomponant_t *tc;
	texid_t result;
	int i;
	int count;

	count = *(int *)texlump;
	tex = (int *)texlump+1;

	for (i = 0; i < count; i++)
	{
		tx = (ddoomtexture_t*)((unsigned char*)texlump + *tex);
		if (!strncmp(tx->name, name, 8))
		{
			tex = BZ_Malloc(tx->width*tx->height*4);
			memset(tex, 0, tx->width*tx->height*4);
			*width = tx->width;
			*height = tx->height;
			tc = (ddoomtexturecomponant_t*)(tx+1);
			for (i = 0; i < tx->componantcount; i++, tc++)
			{
				doomimage_t *img;
				size_t imgsize;
				strncpy(patch+8, pnames+4+8*tc->patchnum, 8);
				Q_strlwr(patch+8);
				patch[16] = '\0';
				Q_strncatz(patch, ".pat", sizeof(patch));

				img = (doomimage_t *)FS_LoadMallocFile(patch, &imgsize);
				Doom_ExtractPName(tex, img, imgsize, tx->width, tx->height, tc->xoffset, tc->yoffset);
				BZ_Free(img);
			}

			*hasalpha = false;
			for (i = 0; i < tx->width * tx->height; i++)
			{
				if (!(tex[i] & 0xff000000))
				{
					*hasalpha = true;
					break;
				}
			}

			result = R_LoadTexture32(name, tx->width, tx->height, tex, 0);
			BZ_Free(tex);
			return result;
		}

		tex++;
	}

	return r_nulltex;
}
static int Doom_LoadPatch(doommap_t *dm, char *name)
{
	int texnum;
	size_t nlen = strnlen(name, 8);

	for (texnum = 0; texnum < dm->numtextures; texnum++)	//a hash table might be a good plan.
	{
		if(!memcmp(name, dm->textures[texnum].name, nlen) && !dm->textures[texnum].name[nlen])
		{
			return texnum;
		}
	}
	//couldn't find it.
//	texnum = dm->numtextures;

	dm->textures = BZ_Realloc(dm->textures, sizeof(*dm->textures)*((dm->numtextures+16)&~15));
	memset(dm->textures + dm->numtextures, 0, sizeof(dm->textures[dm->numtextures]));
	dm->numtextures++;

	memcpy(dm->textures[texnum].name, name, nlen);
	dm->textures[texnum].name[nlen] = 0;

	return texnum;
}

//load a single Doom sprite picture (sprites/<name>) into an RGBA texture, returning
//its pixel size and left/top offsets (the hotspot used to place the billboard).
static texid_t Doom_LoadSprite(const char *name, short *w, short *h, short *xo, short *yo)
{
	char path[MAX_QPATH];
	size_t sz = 0;
	doomimage_t *img;
	unsigned int *tex;
	texid_t r;

	Q_snprintfz(path, sizeof(path), "sprites/%s", name);
	img = (doomimage_t*)FS_LoadMallocFile(path, &sz);
	if (!img || sz < sizeof(*img))
	{
		if (img) BZ_Free(img);
		return r_nulltex;
	}
	*w = img->width; *h = img->height; *xo = img->xpos; *yo = img->ypos;
	tex = BZ_Malloc(img->width*img->height*4);
	memset(tex, 0, img->width*img->height*4);	//uncovered posts stay transparent (alpha 0)
	Doom_ExtractPName(tex, img, sz, img->width, img->height, 0, 0);
	r = R_LoadTexture32(name, img->width, img->height, tex, IF_NOMIPMAP|IF_CLAMP);
	BZ_Free(tex);
	BZ_Free(img);
	return r;
}

//map a Doom thing type to its sprite lump (frame A, rotation 0). Items, keys,
//powerups and the barrel; monsters are left to the entity/gamecode path.
static const char *Doom_ThingSprite(unsigned short type)
{
	switch(type)
	{
	//health & armor
	case 2011: return "STIMA0";	case 2012: return "MEDIA0";
	case 2014: return "BON1A0";	case 2015: return "BON2A0";
	case 2018: return "ARM1A0";	case 2019: return "ARM2A0";
	case 2013: return "SOULA0";	case 83:   return "MEGAA0";
	//ammo
	case 2007: return "CLIPA0";	case 2048: return "AMMOA0";
	case 2008: return "SHELA0";	case 2049: return "SBOXA0";
	case 2010: return "ROCKA0";	case 2046: return "BROKA0";
	case 2047: return "CELLA0";	case 17:   return "CELPA0";
	case 8:    return "BPAKA0";	//backpack
	//weapons
	case 2001: return "SHOTA0";	case 82:   return "SGN2A0";
	case 2002: return "MGUNA0";	case 2003: return "LAUNA0";
	case 2004: return "PLASA0";	case 2006: return "BFUGA0";
	case 2005: return "CSAWA0";
	//keys
	case 5:  return "BKEYA0";	case 40: return "BSKUA0";
	case 6:  return "YKEYA0";	case 39: return "YSKUA0";
	case 13: return "RKEYA0";	case 38: return "RSKUA0";
	//powerups
	case 2022: return "PINVA0";	case 2023: return "PSTRA0";
	case 2024: return "PINSA0";	case 2025: return "SUITA0";
	case 2026: return "PMAPA0";	case 2045: return "PVISA0";
	//decorations / obstacles (standing things - the ones that read as "pillars" etc.)
	//NB: the exploding barrel (2035) is handled as a shootable monster-like entity, not here.
	case 2028: return "COLUA0";	//floor lamp
	case 30:   return "COL1A0";	//tall green pillar
	case 31:   return "COL2A0";	//short green pillar
	case 32:   return "COL3A0";	//tall red pillar
	case 33:   return "COL4A0";	//short red pillar
	case 36:   return "COL5A0";	//short green pillar (beating heart)
	case 37:   return "COL6A0";	//short red pillar (skull)
	case 48:   return "ELECA0";	//tall techno column
	case 35:   return "CBRAA0";	//candelabra
	case 34:   return "CANDA0";	//candle
	case 43:   return "TRE1A0";	//burnt tree
	case 54:   return "TRE2A0";	//large brown tree
	case 47:   return "SMITA0";	//stalagmite
	default: return NULL;
	}
}

//true for things the player collects by walking over them (health/armour/ammo/weapons/keys/
//powerups) - as opposed to solid scenery (barrels, pillars, trees) that share the sprite path.
static qboolean Doom_IsPickup(unsigned short type)
{
	switch(type)
	{
	case 2011: case 2012: case 2014: case 2015: case 2018: case 2019: case 2013: case 83:	//health & armor
	case 2007: case 2048: case 2008: case 2049: case 2010: case 2046: case 2047: case 17: case 8:	//ammo
	case 2001: case 82: case 2002: case 2003: case 2004: case 2006: case 2005:	//weapons
	case 5: case 40: case 6: case 39: case 13: case 38:	//keys
	case 2022: case 2023: case 2024: case 2025: case 2026: case 2045:	//powerups
		return true;
	default:
		return false;	//decorations / obstacles
	}
}

//resolve every item/decoration thing to a billboard (sprite shader + floor position),
//ready for R_DoomDrawSprites. Runs on the main thread (textures) once geometry is loaded.
static void Doom_LoadThingSprites(doommap_t *dm)
{
	unsigned int i;
	dm->numsprites = 0;
	for (i = 0; i < dm->numthings; i++)
	{
		const char *spr = Doom_ThingSprite(dm->thing[i].type);
		texid_t tex;
		short sw, sh, sxo, syo;
		vec3_t p;
		msector_t *sec;
		struct doomsprite_s *out;

		if (!spr)
			continue;
		if (dm->thing[i].flags & THING_DEATHMATCH)
			continue;	//multiplayer-only thing (MTF_NOTSINGLE): not present in single player
		tex = Doom_LoadSprite(spr, &sw, &sh, &sxo, &syo);
		if (!TEXVALID(tex))
			continue;

		//one billboard shader per sprite texture (shares the monster path so the doom_sprmode
		//render-mode toggle applies to item/decoration sprites too)
		out = BZ_Realloc(dm->sprites, sizeof(*dm->sprites)*(dm->numsprites+1));
		dm->sprites = out;
		out += dm->numsprites;
		out->shader = Doom_SpriteShaderFor(spr, tex);

		p[0] = dm->thing[i].xpos; p[1] = dm->thing[i].ypos; p[2] = 0;
		sec = Doom_SectorNearPoint(dm, p);
		out->origin[0] = p[0];
		out->origin[1] = p[1];
		out->origin[2] = sec ? sec->floorheight : 0;
		out->w = sw; out->h = sh; out->xo = sxo; out->yo = syo;
		out->pickup = Doom_IsPickup(dm->thing[i].type);
		out->type = dm->thing[i].type;
		Q_strncpyz(out->voxname, spr, sizeof(out->voxname));	//voxel name = sprite frame minus the rotation digit
		{ int l=strlen(out->voxname); if(l>0) out->voxname[l-1]=0; }
		dm->numsprites++;
	}
}

//////////////////////////////////////////////////////////////////////////////////////////
//monsters: spawn from thing types, chase the player, render as billboards.

//monster attack flags (a monster may have several; melee is preferred when adjacent)
#define MATK_MELEE   1
#define MATK_HITSCAN 2
#define MATK_MISSILE 4
#define MATK_HOMING  8	//its missile steers toward the player (revenant)
#define MATK_VILE    16	//archvile line-of-sight hellfire (no projectile)
#define MATK_BARREL  32	//not an attack: an exploding barrel (shootable, blasts on death)

typedef struct {
	const char	*spr;		//sprite prefix
	short		health, radius, speed;
	qbyte		atk;		//MATK_* bitmask
	short		meleedmg;	//melee: dmg = meleedmg * random(1..meleerand)
	qbyte		meleerand;	//melee random multiplier max (Doom: imp/baron 8, demon/revenant 10, caco 6). 0 -> 8
	short		misdmg;		//missile/hellfire base damage
	short		misspeed;	//projectile speed (units/sec)
	qbyte		bullets;	//hitscan shots per burst
	qboolean	floating;
} doommonsterinfo_t;

//map a monster thing type to its sprite prefix, stats and attack profile (from gzdoom's
//zscript: A_PosAttack/A_TroopAttack/A_SargAttack/A_SkelFist+A_SkelMissile/A_VileAttack/...).
//returns false for non-monsters. speeds are units/sec (~Doom speed * a few).
static qboolean Doom_MonsterInfo(unsigned short type, doommonsterinfo_t *o)
{
	memset(o, 0, sizeof(*o));
	switch(type)
	{
	//hitscan (former humans + spider)
	case 3004: o->spr="POSS"; o->health=20;  o->radius=20; o->speed=130; o->atk=MATK_HITSCAN; o->bullets=1; o->misdmg=3; break;	//zombieman
	case 9:    o->spr="SPOS"; o->health=30;  o->radius=20; o->speed=130; o->atk=MATK_HITSCAN; o->bullets=3; o->misdmg=3; break;	//shotgun guy
	case 65:   o->spr="CPOS"; o->health=70;  o->radius=20; o->speed=130; o->atk=MATK_HITSCAN; o->bullets=2; o->misdmg=3; break;	//chaingunner
	case 7:    o->spr="SPID"; o->health=3000;o->radius=128;o->speed=180; o->atk=MATK_HITSCAN; o->bullets=3; o->misdmg=3; break;	//spider mastermind
	case 84:   o->spr="SSWV"; o->health=50;  o->radius=20; o->speed=130; o->atk=MATK_HITSCAN; o->bullets=1; o->misdmg=3; break;	//wolfenstein ss
	//melee only
	case 3002: o->spr="SARG"; o->health=150; o->radius=30; o->speed=170; o->atk=MATK_MELEE; o->meleedmg=4; o->meleerand=10; break;	//demon: (1..10)*4
	case 58:   o->spr="SARG"; o->health=150; o->radius=30; o->speed=170; o->atk=MATK_MELEE; o->meleedmg=4; o->meleerand=10; break;	//spectre
	case 3006: o->spr="SKUL"; o->health=100; o->radius=16; o->speed=170; o->atk=MATK_MELEE; o->meleedmg=3; o->floating=true; break;	//lost soul
	//melee + missile
	case 3001: o->spr="TROO"; o->health=60;  o->radius=20; o->speed=130; o->atk=MATK_MELEE|MATK_MISSILE; o->meleedmg=3; o->misdmg=3;  o->misspeed=200; break;	//imp
	case 3005: o->spr="HEAD"; o->health=400; o->radius=31; o->speed=130; o->atk=MATK_MELEE|MATK_MISSILE; o->meleedmg=10;o->meleerand=6;o->misdmg=5;  o->misspeed=200; o->floating=true; break;	//cacodemon: melee (1..6)*10
	case 3003: o->spr="BOSS"; o->health=1000;o->radius=24; o->speed=130; o->atk=MATK_MELEE|MATK_MISSILE; o->meleedmg=10;o->misdmg=8;  o->misspeed=300; break;	//baron
	case 69:   o->spr="BOS2"; o->health=500; o->radius=24; o->speed=130; o->atk=MATK_MELEE|MATK_MISSILE; o->meleedmg=10;o->misdmg=8;  o->misspeed=300; break;	//hell knight
	case 66:   o->spr="SKEL"; o->health=300; o->radius=20; o->speed=170; o->atk=MATK_MELEE|MATK_MISSILE|MATK_HOMING; o->meleedmg=6; o->meleerand=10; o->misdmg=10; o->misspeed=200; break;	//revenant: melee (1..10)*6
	//missile only
	case 67:   o->spr="FATT"; o->health=600; o->radius=48; o->speed=130; o->atk=MATK_MISSILE; o->misdmg=8; o->misspeed=400; break;	//mancubus
	case 68:   o->spr="BSPI"; o->health=500; o->radius=64; o->speed=170; o->atk=MATK_MISSILE; o->misdmg=5; o->misspeed=500; break;	//arachnotron
	case 16:   o->spr="CYBR"; o->health=4000;o->radius=40; o->speed=130; o->atk=MATK_MISSILE; o->misdmg=20;o->misspeed=400; break;	//cyberdemon (rockets)
	case 71:   o->spr="PAIN"; o->health=400; o->radius=31; o->speed=130; o->atk=0; o->floating=true; break;	//pain elemental (spawns souls - chase only for now)
	//archvile hellfire
	case 64:   o->spr="VILE"; o->health=700; o->radius=20; o->speed=200; o->atk=MATK_VILE; o->misdmg=20; break;	//arch-vile
	//exploding barrel: not a monster, but reuses the shootable + death-animation path
	case 2035: o->spr="BAR1"; o->health=20;  o->radius=10; o->speed=0;   o->atk=MATK_BARREL; break;	//barrel (BEXP blast on death)
	default: return false;
	}
	return true;
}

//Build the billboard shader for a Doom sprite lump. The exact shader is selectable at runtime via
//the `doom_sprmode` cvar so the monster-"blink"/missing-frame artifact (a hardware-specific
//rasterisation issue - frame selection/rotations/textures are all verified correct) can be A/B'd on
//real hardware. Set the cvar then reload the map (`map e1m1`); the mode is baked into the shader
//name so each mode caches separately. Modes:
//  1 = FTE's own sprite program (defaultsprite#MASK) - the engine's proven flicker-free path
//  2 = alpha-test + depthwrite, opaque sort (the original)
//  3 = alpha-test, NO depthwrite
//  4 = alpha-test + depthwrite, sort seethrough
//  5 = alpha-test + depthwrite, nodepthtest (always drawn on top)
static int Doom_SprMode(void)
{
	return (int)Cvar_Get("doom_sprmode", "1", CVAR_ARCHIVE, "Doom Sprites")->value;
}
static const char *Doom_SprShaderBody(int mode)
{
	switch (mode)
	{
	case 2:  return "{\ncull none\n{\nmap $diffuse\nalphafunc ge128\ndepthwrite\n}\n}\n";
	case 3:  return "{\ncull none\n{\nmap $diffuse\nalphafunc ge128\n}\n}\n";
	case 4:  return "{\nsort seethrough\ncull none\n{\nmap $diffuse\nalphafunc ge128\ndepthwrite\n}\n}\n";
	case 5:  return "{\nnodepthtest\ncull none\n{\nmap $diffuse\nalphafunc ge128\ndepthwrite\n}\n}\n";
	default: return "{\nprogram defaultsprite#MASK=0.666\ncull none\n{\nmap $diffuse\nalphafunc ge128\ndepthwrite\nrgbgen vertex\nalphagen vertex\n}\nsurfaceparm noshadows\nsurfaceparm nodlight\n}\n";	//1
	}
}
static shader_t *Doom_SpriteShaderFor(const char *lump, texid_t tex)
{
	int mode = Doom_SprMode();
	texnums_t stn;
	shader_t *sh;
	char sname[48];
	Q_snprintfz(sname, sizeof(sname), "doom_spr%d_%s", mode, lump);
	memset(&stn, 0, sizeof(stn));
	stn.base = tex;
	sh = R_RegisterShader(sname, SUF_NONE, Doom_SprShaderBody(mode));
	R_BuildDefaultTexnums(&stn, sh, IF_NOMIPMAP);
	return sh;
}

//build the alpha-tested billboard shader for a monster sprite lump (sprites/<lump>).
static shader_t *Doom_MonsterSpriteShader(const char *lump, short *w, short *h, short *xo, short *yo)
{
	texid_t tex = Doom_LoadSprite(lump, w, h, xo, yo);
	if (!TEXVALID(tex)) return NULL;
	return Doom_SpriteShaderFor(lump, tex);
}

//full death-animation frame sequence for each monster sprite, from gzdoom's zscript Death
//states (multi-letter sub-frames expanded). Played over ~0.7s when killed; the LAST frame is
//the resting corpse (held for -1 tics in Doom). NULL = no death frames (shouldn't happen).
static const char *Doom_DeathSeq(const char *spr)
{
	if (!strcmp(spr,"POSS")) return "HIJKL";
	if (!strcmp(spr,"SPOS")) return "HIJKL";
	if (!strcmp(spr,"CPOS")) return "HIJKLMN";
	if (!strcmp(spr,"SPID")) return "JKLMNOPQRS";
	if (!strcmp(spr,"SSWV")) return "HIJKLM";
	if (!strcmp(spr,"SARG")) return "IJKLMN";
	if (!strcmp(spr,"SKUL")) return "FGHIJK";	//lost soul: bursts apart, no real corpse
	if (!strcmp(spr,"TROO")) return "IJKLM";
	if (!strcmp(spr,"HEAD")) return "GHIJKL";
	if (!strcmp(spr,"BOSS")) return "IJKLMNO";
	if (!strcmp(spr,"BOS2")) return "IJKLMNO";
	if (!strcmp(spr,"SKEL")) return "LMNOPQ";
	if (!strcmp(spr,"FATT")) return "KLMNOPQRST";
	if (!strcmp(spr,"BSPI")) return "JKLMNOP";
	if (!strcmp(spr,"CYBR")) return "HIJKLMNOP";
	if (!strcmp(spr,"VILE")) return "QRSTUVWXYZ";
	if (!strcmp(spr,"PAIN")) return "HIJKLM";	//pain elemental: explodes
	return NULL;
}

//per-monster attack (Missile/Melee) frame letters, from gzdoom zscript actors/doom/*.zs. These are
//the SUB-frames shown during the attack, in order (rotation-0 sprites "<SPR><letter>0"). The old
//code hardcoded "EFG" for everyone, which is wrong for most (e.g. zombieman's missile is E,F,E - the
//G it showed is actually the PAIN frame), so the shoot looked like it had a wrong/missing frame.
static const char *Doom_AttackSeq(const char *spr)
{
	if (!strcmp(spr,"POSS")) return "EFE";		//zombieman:   E F E
	if (!strcmp(spr,"SPOS")) return "EFE";		//shotgunner:  E F E
	if (!strcmp(spr,"CPOS")) return "EFEF";		//chaingunner: E F E F
	if (!strcmp(spr,"TROO")) return "EFG";		//imp:         E F G (G = throw)
	if (!strcmp(spr,"SARG")) return "EFG";		//demon:       melee E F G
	if (!strcmp(spr,"HEAD")) return "BCD";		//caco:        B C D
	if (!strcmp(spr,"BOSS")) return "EFG";		//baron
	if (!strcmp(spr,"BOS2")) return "EFG";		//hell knight
	if (!strcmp(spr,"SKUL")) return "CDCD";		//lost soul charge
	if (!strcmp(spr,"CYBR")) return "EFEF";		//cyberdemon (EFEFEF, capped to 4)
	if (!strcmp(spr,"SPID")) return "AGHH";		//spider
	if (!strcmp(spr,"BSPI")) return "AGHH";		//arachnotron
	if (!strcmp(spr,"SKEL")) return "GHIJ";		//revenant (melee GHI + missile J...)
	if (!strcmp(spr,"FATT")) return "GHIG";		//mancubus
	if (!strcmp(spr,"VILE")) return "GHIJ";		//archvile (long hellfire, capped)
	if (!strcmp(spr,"SSWV")) return "EFGF";		//SS
	if (!strcmp(spr,"PAIN")) return "DEF";		//pain elemental
	return "EFG";
}

//per-monster walk (See state) frame letters, from zscript. Most former-humans/imp/demon walk on
//A-D, but the cacodemon has a SINGLE walk frame (A; B-D are its missile frames), and the lost soul
///pain elemental use fewer. Capped at 4 (shader[4]); the 6-frame walkers (revenant/mancubus/etc.)
//just use A-D. Without this, the caco cycled through its own attack poses while idle.
static const char *Doom_WalkFrames(const char *spr)
{
	if (!strcmp(spr,"HEAD")) return "A";	//cacodemon: one floating frame
	if (!strcmp(spr,"SKUL")) return "AB";	//lost soul
	if (!strcmp(spr,"PAIN")) return "ABC";	//pain elemental
	return "ABCD";
}

//per-monster pain frame letter (from zscript Pain state). The pain anim holds this one frame.
static char Doom_PainFrame(const char *spr)
{
	if (!strcmp(spr,"POSS")||!strcmp(spr,"SPOS")||!strcmp(spr,"CPOS")||!strcmp(spr,"CYBR")||!strcmp(spr,"PAIN")) return 'G';
	if (!strcmp(spr,"TROO")||!strcmp(spr,"SARG")||!strcmp(spr,"BOSS")||!strcmp(spr,"BOS2")||!strcmp(spr,"SSWV")) return 'H';
	if (!strcmp(spr,"HEAD")||!strcmp(spr,"SKUL")) return 'E';
	if (!strcmp(spr,"SPID")||!strcmp(spr,"BSPI")) return 'I';
	if (!strcmp(spr,"SKEL")) return 'L';
	if (!strcmp(spr,"FATT")) return 'J';
	if (!strcmp(spr,"VILE")) return 'Q';
	return 'E';
}

//spawn monsters from the map's things. shader[0][rot] holds the walk cycle frames.
static void Doom_LoadMonsters(doommap_t *dm)
{
	unsigned int i;
	dm->nummonsters = 0;
	for (i = 0; i < dm->numthings; i++)
	{
		doommonsterinfo_t mi;
		struct doommonster_s *m;
		vec3_t p; msector_t *sec;
		if (!Doom_MonsterInfo(dm->thing[i].type, &mi))
			continue;
		if (dm->thing[i].flags & THING_DEATHMATCH)
			continue;
		dm->monsters = BZ_Realloc(dm->monsters, sizeof(*dm->monsters)*(dm->nummonsters+1));
		m = &dm->monsters[dm->nummonsters++];
		memset(m, 0, sizeof(*m));
		p[0]=dm->thing[i].xpos; p[1]=dm->thing[i].ypos; p[2]=0;
		sec = Doom_SectorNearPoint(dm, p);
		m->origin[0]=p[0]; m->origin[1]=p[1]; m->origin[2]=sec?sec->floorheight:0;
		m->yaw = dm->thing[i].angle;
		m->health=mi.health; m->radius=(qbyte)mi.radius; m->speed=mi.speed; m->type=dm->thing[i].type; m->spr=mi.spr;
		m->atk=mi.atk; m->meleedmg=mi.meleedmg; m->meleerand=mi.meleerand?mi.meleerand:8; m->misdmg=mi.misdmg; m->misspeed=mi.misspeed; m->bullets=mi.bullets; m->floating=mi.floating?1:0;
		m->atkcool = 0.5f + (rand()&255)/128.0f;
		m->deathtime = -1; m->paintime = -1; m->atktime = -1;
		m->vilet = -1; m->risetime = -1;
		m->natk = 0; m->npain = 0; m->nwalk = 1;	//default (barrels skip the attack/pain frame load below)
		VectorCopy(m->origin, m->spawnorigin); m->spawnyaw = m->yaw; m->spawnhealth = m->health;
		
		if (!(mi.atk & MATK_BARREL))
		{	//load the walk frames (per-monster count) with 8 rotations each
			int f, r, f2; const char *wf = Doom_WalkFrames(mi.spr);
			m->nwalk = 0;
			for (f = 0; f < 4 && wf[f]; f++)
			{
				qboolean frame_ok = false;
				for (r = 0; r < 8; r++)
				{
					char wl[16]; short ww=0, wh=0, wxo=0, wyo=0; shader_t *wsh; int mir=0, p;
					//Standalone rotation: <SPR><frm><rot>
					Q_snprintfz(wl, sizeof(wl), "%s%c%d", mi.spr, wf[f], r+1);
					wsh = Doom_MonsterSpriteShader(wl, &ww,&wh,&wxo,&wyo);
					//Combined mirrored rotations: <SPR><f1><r1><f2><r2>. Doom allows different
					//frame letters to share a lump (e.g. TROOA2B8). Try all walk frames.
					for (f2 = 0; !wsh && f2 < 4 && wf[f2]; f2++)
					{
						for (p = 1; p <= 8; p++)
						{
							if (f == f2 && p == r+1) continue;
							Q_snprintfz(wl, sizeof(wl), "%s%c%d%c%d", mi.spr, wf[f], r+1, wf[f2], p);
							wsh = Doom_MonsterSpriteShader(wl, &ww,&wh,&wxo,&wyo);
							if (wsh) break;
							Q_snprintfz(wl, sizeof(wl), "%s%c%d%c%d", mi.spr, wf[f2], p, wf[f], r+1);
							wsh = Doom_MonsterSpriteShader(wl, &ww,&wh,&wxo,&wyo);
							if (wsh) { mir = 1; break; }
						}
					}
					if (!wsh) { //last resort: non-rotating sprite <SPR><frm>0
						Q_snprintfz(wl, sizeof(wl), "%s%c0", mi.spr, wf[f]);
						wsh = Doom_MonsterSpriteShader(wl, &ww,&wh,&wxo,&wyo);
					}
					if (wsh) {
						m->shader[m->nwalk][r]=wsh; m->w[m->nwalk][r]=ww; m->h[m->nwalk][r]=wh;
						m->xo[m->nwalk][r]=wxo; m->yo[m->nwalk][r]=wyo; m->wmir[m->nwalk][r]=mir;
						frame_ok = true;
					}
				}
				if (frame_ok)
				{	//ensure no NULL rotations: fallback to the first available rotation for this frame
					int first = -1;
					for (r = 0; r < 8; r++) if (m->shader[m->nwalk][r]) { first = r; break; }
					for (r = 0; r < 8; r++) {
						if (m->shader[m->nwalk][r]) continue;
						m->shader[m->nwalk][r] = m->shader[m->nwalk][first];
						m->w[m->nwalk][r] = m->w[m->nwalk][first]; m->h[m->nwalk][r] = m->h[m->nwalk][first];
						m->xo[m->nwalk][r] = m->xo[m->nwalk][first]; m->yo[m->nwalk][r] = m->yo[m->nwalk][first];
						m->wmir[m->nwalk][r] = m->wmir[m->nwalk][first];
					}
					m->nwalk++;
				}
			}
			if (!m->nwalk) m->nwalk = 1;

			//load the pain frame (one frame, held during the flinch)
			m->npain = 0;
			{
				char pc = Doom_PainFrame(mi.spr);
				char pl[16]; short pw, ph, pxo, pyo; shader_t *psh;
				Q_snprintfz(pl, sizeof(pl), "%s%c1", mi.spr, pc); //rotated (front)
				psh = Doom_MonsterSpriteShader(pl, &pw,&ph,&pxo,&pyo);
				if (!psh) { Q_snprintfz(pl, sizeof(pl), "%s%c0", mi.spr, pc); psh = Doom_MonsterSpriteShader(pl, &pw,&ph,&pxo,&pyo); }
				if (psh) { m->painfr[0]=psh; m->pfw[0]=pw; m->pfh[0]=ph; m->pfxo[0]=pxo; m->npain = 1; }
			}

			//load the attack (Missile/Melee) frames, per-monster from the zscript - in order.
			m->natk = 0;
			if (mi.atk & (MATK_HITSCAN|MATK_MISSILE|MATK_HOMING|MATK_VILE|MATK_MELEE))
			{
				const char *aseq = Doom_AttackSeq(mi.spr);
				for (f = 0; aseq[f] && m->natk < 4; f++) {
					char al[16]; short aw, ah, axo, ayo; shader_t *ash;
					Q_snprintfz(al, sizeof(al), "%s%c1", mi.spr, aseq[f]); //rotated (front)
					ash = Doom_MonsterSpriteShader(al, &aw,&ah,&axo,&ayo);
					if (!ash) { Q_snprintfz(al, sizeof(al), "%s%c0", mi.spr, aseq[f]); ash = Doom_MonsterSpriteShader(al, &aw,&ah,&axo,&ayo); }
					if (ash) { m->atkfr[m->natk]=ash; m->afw[m->natk]=aw; m->afh[m->natk]=ah; m->afxo[m->natk]=axo; m->natk++; }
				}
			}
		}

		else
		{	//barrel just needs frame A for all cycles (they don't walk)
			int f, r;
			shader_t *bsh = Doom_MonsterSpriteShader("BAR1A0", &m->w[0][0], &m->h[0][0], &m->xo[0][0], &m->yo[0][0]);
			for (f = 0; f < 4; f++) {
				for (r = 0; r < 8; r++) {
					m->shader[f][r] = bsh;
					m->w[f][r] = m->w[0][0]; m->h[f][r] = m->h[0][0];
					m->xo[f][r] = m->xo[0][0]; m->yo[f][r] = m->yo[0][0];
				}
			}
		}
		{	//death animation
			const char *seq = (mi.atk & MATK_BARREL) ? "ABCDE" : Doom_DeathSeq(mi.spr);
			const char *dspr = (mi.atk & MATK_BARREL) ? "BEXP" : mi.spr;
			m->ndeath = 0;
			while (seq && *seq && m->ndeath < 12)
			{
				char clump[16]; short cw,ch,cxo,cyo; shader_t *csh;
				Q_snprintfz(clump, sizeof(clump), "%s%c0", dspr, *seq);
				csh = Doom_MonsterSpriteShader(clump, &cw,&ch,&cxo,&cyo);
				if (csh) { m->deathfr[m->ndeath]=csh; m->dfw[m->ndeath]=cw; m->dfh[m->ndeath]=ch; m->dfxo[m->ndeath]=cxo; m->ndeath++; }
				seq++;
			}
		}
	}
	Con_Printf("quoom: glmod build %s %s | spawned %u monsters\n", __DATE__, __TIME__, dm->nummonsters);
}

//fully reset the map to its just-loaded state, used when the player respawns so it plays like a
//new game: monsters back alive at their spawn spots, projectiles cleared, every door we opened
//shut again (ceiling heights restored), and collected items put back.
void Doom_ResetMap(model_t *model)
{
	doommap_t *dm = model?model->meshinfo:NULL;
	unsigned int i;
	if (!dm)
		return;
	for (i = 0; i < dm->nummonsters; i++)
	{
		struct doommonster_s *m = &dm->monsters[i];
		VectorCopy(m->spawnorigin, m->origin);
		m->yaw = m->spawnyaw;
		m->health = m->spawnhealth;
		m->mstate = 0;
		m->alerted = 0;
		m->deathtime = -1;
		m->paintime = -1; m->atktime = -1; m->vilet = -1; m->risetime = -1; m->exploded = 0;
		m->atkcool = 0.5f + (rand()&255)/128.0f;
	}
	dm->numprojectiles = 0;
	for (i = 0; i < dm->numactive_doors; i++)
	{	//restore the ceiling each door/tagged action moved, leaving it shut
		struct doorsector_s *d = &dm->doorsectors[i];
		if (d->sector_idx >= 0 && (unsigned)d->sector_idx < dm->numsectors)
			dm->sector[d->sector_idx].ceilingheight = d->ceil_original;
	}
	dm->numactive_doors = 0;
	Doom_LoadThingSprites(dm);	//rebuild the item/decoration billboards -> picked-up items return
}

//Doom damage rolls: base*random(1..8) (or *random(1..5) for bullets). A plain rand() is fine here.
static int Doom_Rand(int lo, int hi) { return lo + (rand()%(hi-lo+1)); }

//apply damage to the player, Doom-style: armour soaks a fraction (green 1/3), capped by how much
//armour is left, and the rest comes off health. (P_DamageMobj in p_inter.c.)
static void Doom_HurtPlayer(float *health, float *armor, int dmg)
{
	if (dmg <= 0)
		return;
	if (armor && *armor > 0)
	{
		int saved = dmg/3;	//green armour absorbs 1/3 (blue 1/2; we don't track type yet)
		if (saved > (int)*armor) saved = (int)*armor;
		*armor -= saved;
		dmg -= saved;
	}
	if (health)
		*health -= dmg;
}

//Doom hitscan accuracy: the bullet is fired at the monster's facing plus a random spread
//(A_PosAttack: angle += P_SubRandom()<<20, i.e. ~+-22deg, triangular). The trace connects only if
//that spread stays within the player's 16-unit radius at this range - so monsters are reliable up
//close and miss progressively more with distance, just like the real P_LineAttack trace. Without
//this every shot landed (100% accuracy), which made even a lone zombieman feel like a one-shotter.
static qboolean Doom_HitscanHits(float dist)
{
	float spread = ((rand()&255) - (rand()&255)) * 0.087890625f;	//(P_SubRandom()<<20) in degrees
	float halfwidth = atan2(16.0, dist < 1 ? 1 : dist) * (180.0/M_PI);
	return fabs(spread) <= halfwidth;
}

//can a sight line from a to b travel without a wall/step blocking it? marches the segment and
//requires the line height to stay within each sector's floor..ceiling (approximates P_CheckSight).
//true if eye a can see point b: the 2D sight ray must not cross any solid wall, nor any
//two-sided linedef whose vertical opening (between the higher floor and lower ceiling of its
//two sectors) doesn't admit the line's height at the crossing. Brute-force over linedefs, which
//is fine for the monster counts here, and - crucially - actually blocks line-of-sight through
//walls (the old height-only march let monsters across the map shoot the player on spawn).
static qboolean Doom_SightLine(doommap_t *dm, const vec3_t a, const vec3_t b)
{
	unsigned int j;
	float adx = b[0]-a[0], ady = b[1]-a[1];	//sight ray (2D)
	for (j = 0; j < dm->numlinedefs; j++)
	{
		dlinedef_t *ld = &dm->linedef[j];
		mdoomvertex_t *v1 = &dm->vertexes[ld->vert[0]];
		mdoomvertex_t *v2 = &dm->vertexes[ld->vert[1]];
		float lx = v2->xpos - v1->xpos, ly = v2->ypos - v1->ypos;	//linedef segment
		float denom = adx*ly - ady*lx;
		float qmx = v1->xpos - a[0], qmy = v1->ypos - a[1];
		float t, u, sz, openbottom, opentop;
		msector_t *fs, *bs;
		if (denom > -1e-6f && denom < 1e-6f)
			continue;	//ray parallel to the linedef
		t = (qmx*ly  - qmy*lx ) / denom;	//param along the sight ray
		u = (qmx*ady - qmy*adx) / denom;	//param along the linedef
		if (t <= 0.001f || t >= 0.999f || u < 0.0f || u > 1.0f)
			continue;	//crossing not strictly between the two endpoints / off the wall
		//one-sided (or non-two-sided) linedef = solid wall: blocks sight
		if (ld->sidedef[1] == 0xffff || !(ld->flags & LINEDEF_TWOSIDED))
			return false;
		//two-sided: sight passes only through the open gap between the sectors
		fs = &dm->sector[dm->sidedef[ld->sidedef[0]].sector];
		bs = &dm->sector[dm->sidedef[ld->sidedef[1]].sector];
		openbottom = (fs->floorheight   > bs->floorheight)   ? fs->floorheight   : bs->floorheight;
		opentop    = (fs->ceilingheight < bs->ceilingheight) ? fs->ceilingheight : bs->ceilingheight;
		if (opentop <= openbottom)
			return false;	//shut (e.g. a closed door)
		sz = a[2] + (b[2]-a[2])*t;
		if (sz < openbottom || sz > opentop)
			return false;	//line passes into the floor step or the lintel/ceiling
	}
	return true;
}

//hurt a monster: subtract health, wake it up, and trigger the pain animation.
static void Doom_HurtMonster(struct doommonster_s *m, int damage)
{
	if (m->mstate == 2) return;
	m->health -= damage;
	m->alerted = 1;
	if (m->health <= 0)
	{
		m->mstate = 2;
		m->deathtime = 0;
	}
	else if (m->painfr[0])
	{
		//trigger pain animation (100% chance for now, vanilla is random)
		m->paintime = 0;
	}
}

//Doom radius damage (P_RadiusAttack): hurt the player and every nearby monster within radius,
//with damage falling off by distance.
void Doom_RadiusDamage(doommap_t *dm, const vec3_t org, float radius, float damage, const vec3_t playerorg, float *ph, float *pa)
{
	unsigned int k;
	float dx, dy, dist, dmg;
	if (ph)
	{
		dx = playerorg[0]-org[0]; dy = playerorg[1]-org[1];
		dist = sqrt(dx*dx+dy*dy);
		if (dist < radius)
		{
			dmg = damage - dist;
			if (dmg > 0) Doom_HurtPlayer(ph, pa, (int)dmg);
		}
	}
	for (k = 0; k < dm->nummonsters; k++)
	{
		struct doommonster_s *o = &dm->monsters[k];
		if (o->mstate == 2) continue;
		dx = o->origin[0]-org[0]; dy = o->origin[1]-org[1];
		dist = sqrt(dx*dx+dy*dy);
		if (dist < radius)
		{
			dmg = damage - dist;
			if (dmg > 0) Doom_HurtMonster(o, (int)dmg);
		}
	}
}

//spawn a monster projectile flying from a monster toward the player.
static void Doom_SpawnProjectile(doommap_t *dm, const vec3_t org, const vec3_t playerorg, int damage, int speed, qboolean homing)
{
	struct doomproj_s *pr; vec3_t to; float len; short w,h,xo,yo; shader_t *sh;
	to[0]=playerorg[0]-org[0]; to[1]=playerorg[1]-org[1]; to[2]=(playerorg[2]+24)-(org[2]+32);
	len = VectorLength(to);
	if (len < 1)
		return;
	sh = Doom_MonsterSpriteShader("BAL1A0", &w, &h, &xo, &yo);	//imp-fireball billboard for all
	if (!sh)
		return;
	dm->projectiles = BZ_Realloc(dm->projectiles, sizeof(*dm->projectiles)*(dm->numprojectiles+1));
	pr = &dm->projectiles[dm->numprojectiles++];
	memset(pr, 0, sizeof(*pr));
	pr->origin[0]=org[0]; pr->origin[1]=org[1]; pr->origin[2]=org[2]+32;	//chest height
	VectorScale(to, speed/len, pr->vel);
	pr->damage = damage; pr->life = 6; pr->homing = homing?1:0;
	pr->type = 0; pr->owner = 0;
	pr->shader=sh; pr->w=w; pr->h=h; pr->xo=xo; pr->yo=yo;
}

//spawn a player projectile (Rocket/Plasma/BFG) flying in the direction the player is facing.
void Doom_PlayerProjectile(model_t *model, const vec3_t org, float yaw, int type)
{
	doommap_t *dm = model?model->meshinfo:NULL;
	struct doomproj_s *pr; short w,h,xo,yo; shader_t *sh;
	const char *spr; float speed; int damage;
	if (!dm) return;
	switch(type) {
		case 1: spr="MISLA0"; speed=900; damage=20; break; //Rocket
		case 2: spr="PLSSA0"; speed=700; damage=20; break; //Plasma
		case 3: spr="BFS1A0"; speed=600; damage=100; break; //BFG
		default: return;
	}
	sh = Doom_MonsterSpriteShader(spr, &w, &h, &xo, &yo);
	if (!sh) return;
	dm->projectiles = BZ_Realloc(dm->projectiles, sizeof(*dm->projectiles)*(dm->numprojectiles+1));
	pr = &dm->projectiles[dm->numprojectiles++];
	memset(pr, 0, sizeof(*pr));
	pr->origin[0]=org[0]; pr->origin[1]=org[1]; pr->origin[2]=org[2]+16; //eye height
	pr->vel[0] = cos(yaw*M_PI/180.0)*speed;
	pr->vel[1] = sin(yaw*M_PI/180.0)*speed;
	pr->vel[2] = 0;
	pr->damage = damage; pr->life = 6; pr->type = type; pr->owner = 1;
	pr->shader=sh; pr->w=w; pr->h=h; pr->xo=xo; pr->yo=yo;
}

//Archvile hellfire timing (from archvile.zs Missile state, ~35 tics/sec): the fire is conjured
//early in the windup and tracks the player; the blast lands at A_VileAttack; you escape by breaking
//line-of-sight before then. Condensed from the vanilla ~2.7s to a snappier ~1.3s window.
#define VILE_CASTHIT	0.9f	//seconds into the cast when the hellfire lands (A_VileAttack)
#define VILE_CASTEND	1.3f	//seconds: total cast length, then the vile recovers

//archvile fire (MT_FIRE): a flame conjured at the player's feet that follows them for the cast.
//Carried as a projectile (type 5) so it renders/ticks with the others; it deals no contact damage
//(the blast is done in the vile's tick at A_VileAttack) and expires by life alone.
static void Doom_SpawnVileFire(doommap_t *dm, const vec3_t playerorg)
{
	struct doomproj_s *pr; short w,h,xo,yo;
	shader_t *sh = Doom_MonsterSpriteShader("FIREA0", &w,&h,&xo,&yo);
	dm->projectiles = BZ_Realloc(dm->projectiles, sizeof(*dm->projectiles)*(dm->numprojectiles+1));
	pr = &dm->projectiles[dm->numprojectiles++];
	memset(pr, 0, sizeof(*pr));
	pr->origin[0]=playerorg[0]; pr->origin[1]=playerorg[1]; pr->origin[2]=playerorg[2]-24;//feet
	pr->damage=0; pr->life=VILE_CASTEND; pr->type=5; pr->owner=0;
	pr->shader=sh; pr->w=w; pr->h=h; pr->xo=xo; pr->yo=yo;
}

void Doom_PlayerAttack(model_t *model, const vec3_t org, float yaw, int pellets, int dmgbase, float maxrange);

//BFG spray tracers (A_BFGSpray): fires 40 tracers in a wide cone from the player's position
//toward the impact area. Deals heavy damage to anything caught.
static void Doom_BFGSpray(doommap_t *dm, const vec3_t playerorg)
{
	int i;
	for (i = 0; i < 40; i++)
	{
		//simplified cone: re-use Doom_PlayerAttack with a very wide spread
		//(true Doom tracers are more complex, but this gives the feel)
		Doom_PlayerAttack(dm->model, playerorg, r_refdef.viewangles[1] + (Doom_Rand(0, 1000)-500)*0.09f, 1, 15, 1000);
	}
}

//move/collide projectiles; damage the player on contact, remove on wall/floor/ceiling hit.
static void Doom_TickProjectiles(doommap_t *dm, float frametime, const vec3_t playerorg, float *playerhealth, float *playerarmor)
{
	unsigned int i;
	for (i = 0; i < dm->numprojectiles; )
	{
		struct doomproj_s *pr = &dm->projectiles[i];
		vec3_t np; msector_t *sec; float dx,dy,dz; qboolean dead=false;
		pr->life -= frametime;
		if (pr->type == 5)
		{	//archvile fire: glued to the player's feet, no collision; gone when the cast ends
			pr->origin[0]=playerorg[0]; pr->origin[1]=playerorg[1]; pr->origin[2]=playerorg[2]-24;
			if (pr->life <= 0) { dm->projectiles[i] = dm->projectiles[--dm->numprojectiles]; continue; }
			i++; continue;
		}
		if (pr->homing)
		{	//steer gradually toward the player (revenant tracer)
			vec3_t to; float vl, sp=VectorLength(pr->vel);
			to[0]=playerorg[0]-pr->origin[0]; to[1]=playerorg[1]-pr->origin[1]; to[2]=(playerorg[2]+24)-pr->origin[2];
			vl = VectorLength(to);
			if (vl > 1 && sp > 1)
			{
				VectorScale(to, 1.0f/vl, to);
				VectorMA(pr->vel, sp*0.12f, to, pr->vel);
				vl = VectorLength(pr->vel);
				if (vl > 1) VectorScale(pr->vel, sp/vl, pr->vel);	//keep constant speed
			}
		}
		VectorMA(pr->origin, frametime, pr->vel, np);
		
		//check hit player
		dx=playerorg[0]-np[0]; dy=playerorg[1]-np[1]; dz=(playerorg[2]+24)-np[2];
		if (dx*dx+dy*dy < 24*24 && fabs(dz) < 40)
		{
			if (pr->owner != 1 || pr->life < 5.8f)
			{
				Doom_HurtPlayer(playerhealth, playerarmor, pr->damage * Doom_Rand(1,8));
				dead = true;
			}
		}
		//check hit monsters
		if (!dead)
		{
			unsigned int k;
			for (k = 0; k < dm->nummonsters; k++)
			{
				struct doommonster_s *m = &dm->monsters[k];
				if (m->mstate == 2) continue;
				dx=m->origin[0]-np[0]; dy=m->origin[1]-np[1]; dz=(m->origin[2]+20)-np[2];
				if (dx*dx+dy*dy < m->radius*m->radius && fabs(dz) < 40)
				{
					if (pr->owner != 0)
					{
						Doom_HurtMonster(m, pr->damage * Doom_Rand(1,8));
						dead = true; break;
					}
				}
			}
		}

		sec = Doom_SectorNearPoint(dm, np);
		if (!sec || np[2] < sec->floorheight || np[2] > sec->ceilingheight)
			dead = true;
		if (dead || pr->life <= 0)
		{
			if (dead && pr->type == 1) //Rocket explosion
				Doom_RadiusDamage(dm, np, 128, 128, playerorg, playerhealth, playerarmor);
			if (dead && pr->type == 3) //BFG blast
			{
				Doom_RadiusDamage(dm, np, 128, 128, playerorg, playerhealth, playerarmor);
				Doom_BFGSpray(dm, playerorg);
			}
			dm->projectiles[i] = dm->projectiles[--dm->numprojectiles];	//swap-remove
		}
		else
		{ VectorCopy(np, pr->origin); i++; }
	}
}

//exploding barrel blast (Doom barrel uses P_RadiusAttack, damage/radius 128): hurt the player and
//every nearby thing, with damage falling off by distance. Other barrels caught in the blast die
//here and detonate on the next tick, giving the classic chain reaction.
static void Doom_BarrelExplode(doommap_t *dm, struct doommonster_s *barrel, const vec3_t playerorg, float *ph, float *pa)
{
	Doom_RadiusDamage(dm, barrel->origin, 128, 128, playerorg, ph, pa);
}

//Would moving this monster's body (a circle of radius m->radius) to (nx,ny) jam it against a
//wall? Doom's P_TryMove line check, brute-forced over the linedefs (same cost as the LOS scan):
//a one-sided wall, an impassable or block-monsters line, or a two-sided line whose opening is
//too short / steps up too far blocks if the monster's circle would push past it. Touching a wall
//at exactly radius is allowed (so it can slide along), only crossing into it blocks.
//Canonical Doom player move check - ported from the reference ports (sdldoom / chocolate-doom
//p_map.c: P_TryMove + P_CheckPosition + PIT_CheckLine + P_LineOpening). This is the RIGHT model:
//it validates ONLY the XY move and reports the floor/ceiling the player would stand under; it does
//NOT touch z (Doom never decides walls and floors in one pass - Z is handled separately, snapping
//the player onto floorz with gravity, like P_ZMovement). Foundation for replacing the swept-AABB
//Doom_Trace for the fg_doom player.
//
//Tests a box of `radius`/`height` at (nx,ny) with the mover's current feet z `feetz`. Accumulates
//tmfloorz = highest opening floor across all touched lines (the floor you'd stand on), tmceilingz =
//lowest opening ceiling, tmdropoffz = lowest adjacent floor. Returns false (blocked) for: a solid/
//impassable line, an opening shorter than `height`, a step up >24, or a dropoff >24 (vanilla blocks
//players from walking off tall ledges too). Player ignores BLOCKMONSTERS. Outputs *outfloor/*outceil.
qboolean Doom_PlayerTryMove(doommap_t *dm, float nx, float ny, float feetz,
	float radius, float height, float *outfloor, float *outceil)
{
	unsigned int j;
	vec3_t p; msector_t *sec;
	float tmfloorz, tmceilingz, tmdropoffz;
	p[0]=nx; p[1]=ny; p[2]=feetz;
	sec = Doom_SectorNearPoint(dm, p);
	if (!sec)
		return false;
	tmfloorz = tmdropoffz = sec->floorheight;	//start from the destination subsector (P_CheckPosition)
	tmceilingz = sec->ceilingheight;
	for (j = 0; j < dm->numlinedefs; j++)
	{
		dlinedef_t *ld = &dm->linedef[j];
		mdoomvertex_t *v1 = &dm->vertexes[ld->vert[0]];
		mdoomvertex_t *v2 = &dm->vertexes[ld->vert[1]];
		float ex = v2->xpos-v1->xpos, ey = v2->ypos-v1->ypos;
		float len2 = ex*ex+ey*ey, t, cx, cy, ddx, ddy;
		msector_t *fs, *bs; float ot, ob, lf;
		if (len2 < 0.001f)
			continue;
		t = ((nx-v1->xpos)*ex + (ny-v1->ypos)*ey)/len2;	//closest point on the segment to (nx,ny)
		if (t<0) t=0; else if (t>1) t=1;
		cx = v1->xpos + t*ex; cy = v1->ypos + t*ey;
		ddx = nx-cx; ddy = ny-cy;
		if (ddx*ddx+ddy*ddy >= radius*radius)
			continue;	//the box doesn't reach this line (PIT_CheckLine bbox reject)
		if (ld->sidedef[1] == 0xffff || (ld->flags & LINEDEF_IMPASSABLE))
			return false;	//one-sided wall or explicitly impassable (player ignores BLOCKMONSTERS)
		//P_LineOpening: tightest opening across every touched line
		fs = &dm->sector[dm->sidedef[ld->sidedef[0]].sector];
		bs = &dm->sector[dm->sidedef[ld->sidedef[1]].sector];
		ot = (fs->ceilingheight < bs->ceilingheight) ? fs->ceilingheight : bs->ceilingheight;
		if (fs->floorheight > bs->floorheight) { ob = fs->floorheight; lf = bs->floorheight; }
		else                                   { ob = bs->floorheight; lf = fs->floorheight; }
		if (ot < tmceilingz) tmceilingz = ot;
		if (ob > tmfloorz)   tmfloorz   = ob;
		if (lf < tmdropoffz) tmdropoffz = lf;
	}
	*outfloor = tmfloorz;
	*outceil  = tmceilingz;
	if (tmceilingz - tmfloorz < height)	return false;	//doesn't fit (closed door / low gap)
	if (tmfloorz - feetz > 24)			return false;	//step up too big
	if (tmfloorz - tmdropoffz > 24)		return false;	//dropoff too big (vanilla blocks players)
	return true;
}

static qboolean Doom_MonsterBlocked(doommap_t *dm, struct doommonster_s *m, float nx, float ny, const vec3_t playerorg)
{
	float r = m->radius;
	unsigned int j;
	float dx, dy;

	//don't walk into the player
	dx = nx - playerorg[0]; dy = ny - playerorg[1];
	if (dx*dx + dy*dy < (r+16)*(r+16)) return true;

	for (j = 0; j < dm->numlinedefs; j++)
	{
		dlinedef_t *ld = &dm->linedef[j];
		mdoomvertex_t *v1 = &dm->vertexes[ld->vert[0]];
		mdoomvertex_t *v2 = &dm->vertexes[ld->vert[1]];
		float ex = v2->xpos - v1->xpos, ey = v2->ypos - v1->ypos;
		float len2 = ex*ex + ey*ey, t, cx, cy, ddx, ddy;
		if (len2 < 0.001f)
			continue;
		//closest point on the line segment to the monster's new centre
		t = ((nx - v1->xpos)*ex + (ny - v1->ypos)*ey) / len2;
		if (t < 0) t = 0; else if (t > 1) t = 1;
		cx = v1->xpos + t*ex; cy = v1->ypos + t*ey;
		ddx = nx - cx; ddy = ny - cy;
		if (ddx*ddx + ddy*ddy >= r*r)
			continue;	//the circle doesn't reach this line
		if (ld->sidedef[1] == 0xffff || (ld->flags & LINEDEF_IMPASSABLE) || (ld->flags & 2))
			return true;	//one-sided wall, impassable, or block-monsters (Doom flag 0x2)
		else
		{
			msector_t *fs = &dm->sector[dm->sidedef[ld->sidedef[0]].sector];
			msector_t *bs = &dm->sector[dm->sidedef[ld->sidedef[1]].sector];
			float opentop = (fs->ceilingheight < bs->ceilingheight) ? fs->ceilingheight : bs->ceilingheight;
			float openbot = (fs->floorheight   > bs->floorheight)   ? fs->floorheight   : bs->floorheight;
			float lowfloor = (fs->floorheight  < bs->floorheight)   ? fs->floorheight   : bs->floorheight;
			if (opentop - openbot < 56 || openbot - m->origin[2] > 24)
				return true;	//opening too short to fit through, or the step up is too tall
			if (openbot - lowfloor > 24 && !m->floating)
				return true;	//too deep to drop (vanilla blocks non-floating monsters)
		}
	}
	//don't pile onto another live monster.
	for (j = 0; j < dm->nummonsters; j++)
	{
		struct doommonster_s *o = &dm->monsters[j];
		float rr, cur2, new2, ox, oy, cx2, cy2;
		if (o == m || o->mstate == 2)
			continue;	//self, or a corpse
		rr = r + o->radius;
		ox = nx - o->origin[0];          oy = ny - o->origin[1];          new2 = ox*ox + oy*oy;
		cx2 = m->origin[0]-o->origin[0]; cy2 = m->origin[1]-o->origin[1]; cur2 = cx2*cx2 + cy2*cy2;
		if (new2 < rr*rr && new2 < cur2)
			return true;
	}

	//don't walk through solid decorations (pillars, barrels, etc.)
	for (j = 0; j < dm->numsprites; j++)
	{
		struct doomsprite_s *s = &dm->sprites[j];
		//Doom solid decorations are usually hardcoded or based on type.
		//For now, assume anything with a radius-like type is solid.
		//(In Doom, many sprites are solid: 30, 31, 32, 33, 37, 47, 48, 54, etc.)
		qboolean solid = false;
		switch(s->type) {
			case 2035: //barrel
			case 30: case 31: case 32: case 33: case 37: case 47: case 48: case 54: //pillars/trees
			case 70: case 41: case 42: case 43: case 44: case 45: case 46: //torches
				solid = true; break;
		}
		if (solid)
		{
			float sr = 16; //default decoration radius
			dx = nx - s->origin[0]; dy = ny - s->origin[1];
			if (dx*dx + dy*dy < (r+sr)*(r+sr)) return true;
		}
	}
	return false;
}

//Owned-weapons bitmask, stored in the player edict's .items (so it syncs to the client for the
//HUD). KEEP IN SYNC with the copy in sv_user.c.
#define DWEP_FIST      1
#define DWEP_CHAINSAW  2
#define DWEP_PISTOL    4
#define DWEP_SHOTGUN   8
#define DWEP_SSG       16
#define DWEP_CHAINGUN  32
#define DWEP_ROCKET    64
#define DWEP_PLASMA    128
#define DWEP_BFG       256

enum { DW_FIST, DW_CHAINSAW, DW_PISTOL, DW_SHOTGUN, DW_SSG, DW_CHAINGUN, DW_ROCKET, DW_PLASMA, DW_BFG, DW_COUNT };

//Item pickups: collect any pickup billboard the player walks over (touch radius ~ the two radii
//summed) and apply it to the inventory. Called from sv_user.c each frame with the player's edict
//fields. Ammo is capped at the Doom maxima; weapons grant ownership (a DWEP bit in *items) plus a
//little ammo. Keys/powerups are collected but have no effect yet. Sprite removed by swap-with-last.
void Doom_TryPickups(model_t *model, const vec3_t playerorg, float *health, float *armor,
	float *bullets, float *shells, float *rockets, float *cells, float *items, float *weapon)
{
	doommap_t *dm = model?model->meshinfo:NULL;
	unsigned int s;
	if (!dm)
		return;
	for (s = 0; s < dm->numsprites; )
	{
		struct doomsprite_s *sp = &dm->sprites[s];
		float pdx = playerorg[0]-sp->origin[0], pdy = playerorg[1]-sp->origin[1];
		if (sp->pickup && pdx*pdx+pdy*pdy < 36*36 && fabs(playerorg[2]-sp->origin[2]) < 72)
		{
			int wb = items ? (int)*items : 0;
			int nw = -1;
			switch(sp->type)
			{
			//health
			case 2014: if (health && *health < 200) *health += 1; break;	//health bonus
			case 2011: if (health && *health < 100) *health = min(100,*health+10); break;	//stimpack
			case 2012: if (health && *health < 100) *health = min(100,*health+25); break;	//medikit
			case 2013: if (health && *health < 200) *health = min(200,*health+100); break;	//soulsphere
			case 83:   if (health) *health=200; if (armor) *armor=200; break;	//megasphere
			//armor
			case 2015: if (armor && *armor < 200) *armor += 1; break;	//armor bonus
			case 2018: if (armor && *armor < 100) *armor = 100; break;	//green armor
			case 2019: if (armor) *armor = 200; break;	//blue armor
			//ammo (caps: bullets 200, shells 50, rockets 50, cells 300)
			case 2007: if (bullets) *bullets = min(200,*bullets+10); break;	//clip
			case 2048: if (bullets) *bullets = min(200,*bullets+50); break;	//box of bullets
			case 2008: if (shells)  *shells  = min(50, *shells +4);  break;	//4 shells
			case 2049: if (shells)  *shells  = min(50, *shells +20); break;	//box of shells
			case 2010: if (rockets) *rockets = min(50, *rockets+1);  break;	//rocket
			case 2046: if (rockets) *rockets = min(50, *rockets+5);  break;	//box of rockets
			case 2047: if (cells)   *cells   = min(300,*cells+20);   break;	//cell
			case 17:   if (cells)   *cells   = min(300,*cells+100);  break;	//cell pack
			case 8:	//backpack
				if (bullets) *bullets = min(200,*bullets+10);
				if (shells)  *shells  = min(50, *shells +4);
				if (rockets) *rockets = min(50, *rockets+1);
				if (cells)   *cells   = min(300,*cells+20);
				break;
			//weapons (ownership + Doom's bundled ammo)
			case 2001: wb |= DWEP_SHOTGUN;  if (shells)  *shells  = min(50,*shells+8);   nw=DW_SHOTGUN; break;
			case 82:   wb |= DWEP_SSG;      if (shells)  *shells  = min(50,*shells+8);   nw=DW_SSG;     break;
			case 2002: wb |= DWEP_CHAINGUN; if (bullets) *bullets = min(200,*bullets+20);nw=DW_CHAINGUN;break;
			case 2005: wb |= DWEP_CHAINSAW; nw=DW_CHAINSAW; break;
			case 2003: wb |= DWEP_ROCKET;   if (rockets) *rockets = min(50,*rockets+2);  nw=DW_ROCKET;  break;
			case 2004: wb |= DWEP_PLASMA;   if (cells)   *cells   = min(300,*cells+40);  nw=DW_PLASMA;  break;
			case 2006: wb |= DWEP_BFG;      if (cells)   *cells   = min(300,*cells+40);  nw=DW_BFG;     break;
			default: break;	//keys/powerups: collected, no effect yet
			}
			if (items)
			{
				int old = (int)*items;
				*items = (float)wb;
				//auto-switch to better weapon
				if (wb != old && weapon && nw != -1) {
					//Ranking: BFG > Plasma > Rocket > Chaingun > SSG > Shotgun > Chainsaw > Pistol > Fist
					int rank_val[] = {0, 2, 1, 3, 4, 5, 6, 7, 8};
					if (nw >= 0 && nw < 9 && (*weapon < 0 || *weapon >= 9 || rank_val[nw] > rank_val[(int)*weapon]))
						*weapon = (float)nw;
				}
			}
			dm->sprites[s] = dm->sprites[--dm->numsprites];
			continue;
		}
		s++;
	}
}

//dynamically spawn a new monster into the map (e.g. Lost Souls from a Pain Elemental).
static void Doom_SpawnMonster(doommap_t *dm, unsigned short type, const vec3_t org, float yaw)
{
	doommonsterinfo_t mi;
	struct doommonster_s *m;
	if (!Doom_MonsterInfo(type, &mi))
		return;
	dm->monsters = BZ_Realloc(dm->monsters, sizeof(*dm->monsters)*(dm->nummonsters+1));
	m = &dm->monsters[dm->nummonsters++];
	memset(m, 0, sizeof(*m));
	VectorCopy(org, m->origin); m->yaw = yaw;
	m->health=mi.health; m->radius=(qbyte)mi.radius; m->speed=mi.speed; m->type=type;
	m->atk=mi.atk; m->meleedmg=mi.meleedmg; m->meleerand=mi.meleerand?mi.meleerand:8; m->misdmg=mi.misdmg; m->misspeed=mi.misspeed; m->bullets=mi.bullets; m->floating=mi.floating?1:0;
	m->atkcool = 0.5f; m->deathtime = -1; m->paintime = -1; m->alerted = 1;
	VectorCopy(m->origin, m->spawnorigin); m->spawnyaw = m->yaw; m->spawnhealth = m->health;
	{	//load walk frames and rotations
		int f, r; const char wf[] = "ABCD";
		for (f = 0; f < 4; f++)
		{
			for (r = 0; r < 8; r++)
			{
				char wl[16]; short ww, wh, wxo, wyo; shader_t *wsh;
				Q_snprintfz(wl, sizeof(wl), "%s%c%d", mi.spr, wf[f], r+1);
				wsh = Doom_MonsterSpriteShader(wl, &ww,&wh,&wxo,&wyo);
				if (!wsh) {
					Q_snprintfz(wl, sizeof(wl), "%s%c1", mi.spr, wf[f]);
					wsh = Doom_MonsterSpriteShader(wl, &ww,&wh,&wxo,&wyo);
					if (!wsh) {
						Q_snprintfz(wl, sizeof(wl), "%s%c0", mi.spr, wf[f]);
						wsh = Doom_MonsterSpriteShader(wl, &ww,&wh,&wxo,&wyo);
					}
				}
				if (wsh) { m->shader[f][r]=wsh; m->w[f][r]=ww; m->h[f][r]=wh; m->xo[f][r]=wxo; m->yo[f][r]=wyo; }
			}
		}
		for (f = 0; f < 2; f++) {
			char pl[16]; short pw, ph, pxo, pyo; shader_t *psh;
			Q_snprintfz(pl, sizeof(pl), "%s%c0", mi.spr, (type==3005?'G':'E'));
			psh = Doom_MonsterSpriteShader(pl, &pw,&ph,&pxo,&pyo);
			if (psh) { m->painfr[f]=psh; m->pfw[f]=pw; m->pfh[f]=ph; m->pfxo[f]=pxo; }
		}
		const char *seq = Doom_DeathSeq(mi.spr);
		m->ndeath = 0;
		while (seq && *seq && m->ndeath < 12)
		{
			char clump[16]; short cw,ch,cxo,cyo; shader_t *csh;
			Q_snprintfz(clump, sizeof(clump), "%s%c0", mi.spr, *seq);
			csh = Doom_MonsterSpriteShader(clump, &cw,&ch,&cxo,&cyo);
			if (csh) { m->deathfr[m->ndeath]=csh; m->dfw[m->ndeath]=cw; m->dfh[m->ndeath]=ch; m->dfxo[m->ndeath]=cxo; m->ndeath++; }
			seq++;
		}
	}
}

//chase + attack AI, ticked server-side once per frame (from sv_phys.c). A live monster within
//range faces the player; if it can attack (cooldown ready + line of sight) it melees / fires
//hitscan / launches a missile / does the archvile hellfire, else it walks closer. Movement is
//gated on the destination sector being walkable (small step-up, enough headroom) and not blocked
//by a wall; when the straight path is blocked it tries angled steps so it slides around obstacles.
void Doom_TickMonsters(model_t *model, float frametime, const vec3_t playerorg, float *playerhealth, float *playerarmor)
{
	doommap_t *dm = model?model->meshinfo:NULL;
	unsigned int i, old_nummonsters;
	if (!dm)
		return;

	//(item pickups are handled in Doom_TryPickups, called from sv_user.c where the full player
	//inventory - ammo and owned weapons, not just health/armour - is available.)

	Doom_TickProjectiles(dm, frametime, playerorg, playerhealth, playerarmor);
	old_nummonsters = dm->nummonsters;
	for (i = 0; i < old_nummonsters; i++)
	{
		struct doommonster_s *m = &dm->monsters[i];
		float dx, dy, dist, step, meleerange; vec3_t np, eye, peye; msector_t *sec; qboolean sight;
		if (m->mstate == 2)
		{	//dead
			if (m->risetime >= 0)
			{	//being resurrected by an archvile (A_VileChase -> the corpse's Raise state): play the
				//death frames in reverse, then come back alive at full health.
				m->risetime += frametime;
				if (m->risetime >= m->ndeath * 0.07f)
				{
					m->mstate = 0; m->health = m->spawnhealth; m->alerted = 1;
					m->deathtime = -1; m->risetime = -1; m->atktime = -1; m->paintime = -1; m->exploded = 0;
				}
				continue;
			}
			//detonate a freshly-killed barrel, then advance the death animation timer (no AI)
			if ((m->atk & MATK_BARREL) && !m->exploded)
				Doom_BarrelExplode(dm, m, playerorg, playerhealth, playerarmor);
			if (m->deathtime >= 0) m->deathtime += frametime;
			continue;
		}
		if (m->atk & MATK_BARREL)
		{	m->animt += frametime;	//idle bob (voxel BAR1 A/B); barrels otherwise have no AI
			continue;		//Without the skip they'd get alerted into the attack state (no
		}				//attack frames -> the sprite render leaves sh=NULL and they vanish).
		if (m->paintime >= 0) m->paintime += frametime;
		if (m->atktime >= 0)
		{	//attack animation in progress: advance it, and end it after natk frames (~0.25s each)
			m->atktime += frametime;
			if (m->natk > 0 && m->atktime >= m->natk*0.25f) m->atktime = -1;
		}
		m->animt += frametime;	//advance the walk cycle (render uses it while the monster is chasing)
		dx = playerorg[0]-m->origin[0]; dy = playerorg[1]-m->origin[1];
		dist = sqrt(dx*dx+dy*dy);

		//both "eyes" at ~feet+40: monster origin is its feet; the player origin is feet+24 (hull
		//mins.z=-24), so +16 gives feet+40. (The old +40 on the player put the sight target above
		//its head, so LOS often failed and monsters never attacked.)
		VectorSet(eye,  m->origin[0], m->origin[1], m->origin[2]+40);
		VectorSet(peye, playerorg[0], playerorg[1], playerorg[2]+16);
		sight = (dist < 3000) ? Doom_SightLine(dm, eye, peye) : false;

		if (!m->alerted)
		{	//dormant until it sees the player at moderate range or hears a shot (Doom_NoiseAlert)
			if (dist < 1280 && sight) m->alerted = 1;
			else continue;
		}
		if (dist > 3000)
			continue;	//alerted but the player ran far off: idle until closer

		m->mstate = 1;
		m->yaw = atan2(dy, dx)*180.0/M_PI;
		if (m->atkcool > 0)
			m->atkcool -= frametime;

		if (m->vilet >= 0)
		{	//archvile mid-cast: keep facing the player, drive the attack animation from the cast
			//timer, and at A_VileAttack deal 20 + up to 70 blast IF still in sight - break LOS to dodge.
			m->vilet += frametime;
			m->yaw = atan2(dy, dx)*180.0/M_PI;
			if (m->natk > 0) m->atktime = (m->vilet / VILE_CASTEND) * (m->natk * 0.25f);
			if (m->vilet >= VILE_CASTHIT && m->vilet - frametime < VILE_CASTHIT && sight)
				Doom_HurtPlayer(playerhealth, playerarmor, 20 + Doom_Rand(0, 70));
			if (m->vilet >= VILE_CASTEND) { m->vilet = -1; m->atktime = -1; m->atkcool = 1.5f; }
			continue;	//no movement / other attacks while casting
		}

		if (m->type == 64)
		{	//A_VileChase: scan for a nearby raisable corpse and resurrect it (priority over attacking,
			//like vanilla - checked every chase frame). The corpse plays its Raise
			//state (reverse-death animation, via risetime) and comes back alive; the vile pauses (Heal).
			unsigned int k;
			for (k = 0; k < dm->nummonsters; k++)
			{
				struct doommonster_s *c = &dm->monsters[k];
				if (c->mstate == 2 && c->risetime < 0 && c->ndeath > 0 && !(c->atk & MATK_BARREL)
					&& c->deathtime >= (c->ndeath-1)*0.15f)	//only a fully-settled corpse (on its last frame)
				{
					float cdx = c->origin[0]-m->origin[0], cdy = c->origin[1]-m->origin[1];
					if (cdx*cdx+cdy*cdy < 64*64)
					{
	c->risetime = 0;	//begin the reverse-death Raise; revives when it finishes
						m->atkcool = 1.0f; m->atktime = 0; break;	//vile Heal pause
					}
				}
			}
		}
		
		if (m->type == 71 && m->atkcool <= 0 && sight && dist < 1000)
		{	//Pain Elemental: spawn a Lost Soul
			vec3_t sporg;
			sporg[0] = m->origin[0] + cos(m->yaw*M_PI/180.0)*48;
			sporg[1] = m->origin[1] + sin(m->yaw*M_PI/180.0)*48;
			sporg[2] = m->origin[2] + 24;
			Doom_SpawnMonster(dm, 3006, sporg, m->yaw);
			m->atkcool = 2.0f;
			m = &dm->monsters[i]; //refresh pointer after possible realloc
		}

		meleerange = m->radius + 36;	//+player radius ~16 + slack

		if (m->atk && m->atkcool <= 0 && sight)
		{
			m->atktime = 0; //start attack animation
			if ((m->atk & MATK_MELEE) && dist <= meleerange)
			{	//bite/claw/fist - melee always connects in range (no spread)
				Doom_HurtPlayer(playerhealth, playerarmor, m->meleedmg * Doom_Rand(1, m->meleerand));
				m->atkcool = 1.0f; continue;
			}
			if (m->atk & MATK_HITSCAN)
			{	//former human / spider chaingun: instant bullets, each rolled for accuracy
				int b; for (b=0;b<m->bullets;b++)
					if (Doom_HitscanHits(dist))
						Doom_HurtPlayer(playerhealth, playerarmor, m->misdmg * Doom_Rand(1,5));	//((rand%5)+1)*3 per bullet
				m->atkcool = 1.0f; continue;
			}
			if (m->atk & MATK_VILE)
			{	//archvile: begin the hellfire cast (A_VileStart/A_VileTarget) - conjure the tracking
				//fire at the player; the blast lands later in the cast (handled above) if LOS holds.
				m->vilet = 0; m->atktime = 0;
				Doom_SpawnVileFire(dm, playerorg);
				m->atkcool = 2.0f; continue;
			}
			if (m->atk & MATK_MISSILE)
			{	//imp/caco/baron ball, revenant homing tracer, rockets, ...
				Doom_SpawnProjectile(dm, m->origin, playerorg, m->misdmg, m->misspeed, !!(m->atk & MATK_HOMING));
				m->atkcool = 1.5f; continue;
			}
		}

		//Doom monsters run OR shoot, never both: while the attack (or pain) animation is playing the
		//monster stands still (it still faces the player, set above). A_Chase only moves between
		//attacks - it returns without moving when it switches to the Missile/Melee state. (chocolate-
		//doom p_enemy.c A_Chase: P_SetMobjState(missilestate); return;)
		if (m->atktime >= 0 || m->paintime >= 0)
			continue;
		if (dist <= meleerange)
			continue;	//adjacent: hold position
		step = m->speed * frametime;
		if (step > dist) step = dist;
		{	//walk toward the player, but when the straight path is blocked try increasingly
			//angled steps (±30/60/90deg) so the monster slides around walls and pillars
			//instead of jamming into them. First direction that's walkable wins.
			float baseang = atan2(dy, dx);
			static const float trya[] = { 0, 0.5236f, -0.5236f, 1.0472f, -1.0472f, 1.5708f, -1.5708f };
			int ti;
			for (ti = 0; ti < (int)(sizeof(trya)/sizeof(trya[0])); ti++)
			{
				float a = baseang + trya[ti];
				np[0] = m->origin[0] + cos(a)*step;
				np[1] = m->origin[1] + sin(a)*step;
				np[2] = m->origin[2];
				sec = Doom_SectorNearPoint(dm, np);
				if (sec && sec->floorheight <= m->origin[2]+24 && sec->ceilingheight - sec->floorheight >= 56
					&& !Doom_MonsterBlocked(dm, m, np[0], np[1], playerorg))
				{
					m->origin[0]=np[0]; m->origin[1]=np[1];
					if (m->floating)
					{
						//gradually move toward the player's eye height
						float dz = (playerorg[2]+16) - m->origin[2];
						if (fabs(dz) > 8)
							m->origin[2] += (dz > 0 ? 1 : -1) * m->speed * frametime * 0.5f;
						//keep within the sector vertical bounds
						if (m->origin[2] < sec->floorheight) m->origin[2] = sec->floorheight;
						if (m->origin[2] > sec->ceilingheight - 40) m->origin[2] = sec->ceilingheight - 40;
					}
					else
						m->origin[2]=sec->floorheight;
					break;
				}
			}
		}
	}
}

//Doom sound propagation (P_RecursiveSound / P_NoiseAlert): a noise (here, the player's gunshot)
//floods out from the noise sector through open two-sided lines, waking every monster in a reached
//sector. Sound-block lines (LINEDEF_BLOCKSOUND) and shut openings stop it, so it travels around
//corners and through doorways but not through solid walls. BFS over sectors, adjacency scanned on
//the fly from the linedefs (fine as a one-shot on each shot).
static void Doom_NoiseAlert(doommap_t *dm, const vec3_t noiseorg)
{
	msector_t *psec;
	qbyte *vis; int *queue; int head=0, tail=0, pstart;
	unsigned int j;
	if (!dm || !dm->numsectors)
		return;
	psec = Doom_SectorNearPoint(dm, noiseorg);
	if (!psec)
		return;
	pstart = (int)(psec - dm->sector);
	if (pstart < 0 || pstart >= (int)dm->numsectors)
		return;
	vis = Z_Malloc(dm->numsectors);
	queue = Z_Malloc(sizeof(int)*dm->numsectors);
	vis[pstart] = 1; queue[tail++] = pstart;
	while (head < tail)
	{
		int sc = queue[head++];
		for (j = 0; j < dm->numlinedefs; j++)
		{
			dlinedef_t *ld = &dm->linedef[j];
			int a, b, other; msector_t *fs, *bs; float openbottom, opentop;
			if (ld->sidedef[1] == 0xffff || !(ld->flags & LINEDEF_TWOSIDED))
				continue;	//solid wall: sound doesn't pass
			if (ld->flags & LINEDEF_BLOCKSOUND)
				continue;	//explicit sound-block line
			a = dm->sidedef[ld->sidedef[0]].sector;
			b = dm->sidedef[ld->sidedef[1]].sector;
			if (a == sc) other = b; else if (b == sc) other = a; else continue;
			if (other < 0 || other >= (int)dm->numsectors || vis[other])
				continue;
			fs = &dm->sector[a]; bs = &dm->sector[b];
			openbottom = (fs->floorheight   > bs->floorheight)   ? fs->floorheight   : bs->floorheight;
			opentop    = (fs->ceilingheight < bs->ceilingheight) ? fs->ceilingheight : bs->ceilingheight;
			if (opentop <= openbottom)
				continue;	//shut (closed door / solid step): blocks sound
			vis[other] = 1; queue[tail++] = other;
		}
	}
	for (j = 0; j < dm->nummonsters; j++)
	{
		struct doommonster_s *m = &dm->monsters[j];
		msector_t *ms; int si;
		if (m->mstate == 2 || m->alerted)
			continue;
		ms = Doom_SectorNearPoint(dm, m->origin);
		if (!ms) continue;
		si = (int)(ms - dm->sector);
		if (si >= 0 && si < (int)dm->numsectors && vis[si])
			m->alerted = 1;
	}
	Z_Free(queue);
	Z_Free(vis);
}

//Teleport linedef support (Doom specials 39 W1 / 97 WR). Find the destination for teleport line
//linedef_idx: the type-14 "teleport landing" thing standing in a sector tagged to match the line.
//Telefrag (gib) any live monster already on that spot, then hand the landing position + facing
//back to the caller (sv_user.c moves the player there). Returns false if the line has no landing.
qboolean Doom_TeleportThing(model_t *model, int linedef_idx, vec3_t outorg, float *outyaw)
{
	doommap_t *dm = model->meshinfo;
	dlinedef_t *ld;
	int tag;
	unsigned int i;
	if (!dm || linedef_idx < 0 || (unsigned)linedef_idx >= dm->numlinedefs)
		return false;
	ld = dm->linedef + linedef_idx;
	tag = ld->tag;
	if (!tag)
		return false;	//teleporters are always tagged to their destination sector
	for (i = 0; i < dm->numthings; i++)
	{
		vec3_t tp;
		msector_t *ts;
		unsigned int k;
		if (dm->thing[i].type != 14)	//MT_TELEPORTMAN - the teleport landing marker
			continue;
		tp[0] = dm->thing[i].xpos; tp[1] = dm->thing[i].ypos; tp[2] = 0;
		ts = Doom_SectorNearPoint(dm, tp);
		if (!ts || ts->tag != tag)
			continue;	//this landing isn't in the sector this teleporter targets
		//telefrag: anything standing on the landing is gibbed (its body + the arriving player's
		//16u radius), so you violently displace whatever's in the way - classic Doom telefrag.
		for (k = 0; k < dm->nummonsters; k++)
		{
			struct doommonster_s *mo = &dm->monsters[k];
			float ddx, ddy, rr;
			if (mo->mstate == 2)
				continue;	//already a corpse
			rr = mo->radius + 16;
			ddx = mo->origin[0]-tp[0]; ddy = mo->origin[1]-tp[1];
			if (ddx*ddx + ddy*ddy < rr*rr)
			{	//gibbed by the arrival
				mo->mstate = 2;
				mo->deathtime = 0;
			}
		}
		outorg[0] = tp[0];
		outorg[1] = tp[1];
		outorg[2] = ts->floorheight;
		*outyaw = dm->thing[i].angle;
		return true;
	}
	return false;	//tagged sector had no landing thing
}

//player fired their weapon: auto-aim hitscan at the nearest live monster roughly in front (with
//line of sight) and damage it. Called from sv_user.c when the attack button is pressed.
//Player weapon fire: auto-aim at the nearest live monster in a cone within maxrange (with line of
//sight) and deal `pellets` hits of Doom_Rand(1,3)*dmgbase. Hitscan guns pass a long range with
//1 pellet (pistol/chaingun), 7 (shotgun) or 20 (super shotgun); the fist/chainsaw pass a short
//melee range. All pellets land on the aimed target for now (true per-pellet spread is a later pass).
void Doom_PlayerAttack(model_t *model, const vec3_t org, float yaw, int pellets, int dmgbase, float maxrange)
{
	doommap_t *dm = model?model->meshinfo:NULL;
	int p; vec3_t a;
	if (!dm)
		return;
	VectorSet(a, org[0], org[1], org[2]+16);	//player body ~feet+40 (origin is feet+24)
	for (p = 0; p < pellets; p++)
	{
		unsigned int i, best=~0u; float bestdist, fwdx, fwdy, pyaw;
		bestdist = (maxrange > 0) ? maxrange : 2000;
		//apply horizontal spread for multi-pellet weapons (shotguns)
		pyaw = yaw + (pellets > 1 ? (float)(Doom_Rand(0, 1000)-500)*0.02f : 0);
		fwdx = cos(pyaw*M_PI/180.0); fwdy = sin(pyaw*M_PI/180.0);
		for (i = 0; i < dm->nummonsters; i++)
		{
			struct doommonster_s *m = &dm->monsters[i];
			float dx,dy,dist,dot; vec3_t b;
			if (m->mstate == 2)
				continue;
			dx=m->origin[0]-org[0]; dy=m->origin[1]-org[1];
			dist=sqrt(dx*dx+dy*dy);
			if (dist < 1 || dist > bestdist)
				continue;
			dot=(dx*fwdx+dy*fwdy)/dist;
			if (dot < 0.96f)	//~16 deg auto-aim cone
				continue;
			VectorSet(b, m->origin[0], m->origin[1], m->origin[2]+40);
			if (!Doom_SightLine(dm, a, b))
				continue;
			best=i; bestdist=dist;
		}
		if (best != ~0u)
		{
			struct doommonster_s *m = &dm->monsters[best];
			Doom_HurtMonster(m, Doom_Rand(1,3) * dmgbase);
		}
	}
	Doom_NoiseAlert(dm, org);	//the gunshot wakes monsters within sound range
}

//============================ Doom voxel models (KVX) ====================================
// Optional voxel rendering for monsters (VoxelDoom .kvx at voxels/<NAME>.kvx, e.g. POSSA.kvx).
// One voxel per animation frame-letter, rotation-free (unlike the 8-rotation sprites), drawn as a
// solid vertex-coloured mesh - opaque like the world geometry (which never blinks). Enabled by the
// doom_voxels cvar (default on); the sprite path is kept as the fallback. KVX parsed per Ken
// Silverman's spec / gzdoom R_LoadKVX. Built once per frame-name and cached.
typedef struct doomvoxel_s {
	char        name[16];
	qboolean    tried;		// load attempted (true even if it produced no geometry)
	int         nverts, nidx;
	vecV_t      *xyz;		// LOCAL space: x,y centred on the pivot, z up from the feet
	byte_vec4_t *col;
	index_t     *idx;
	float       angleoffset;	// per-voxel facing offset (deg) from VOXELDEF.txt (e.g. ammo = 270)
} doomvoxel_t;
static doomvoxel_t *doomvox; static int doomvoxcount, doomvoxmax;
static shader_t *doomvoxshader;
static vecV_t *doomvoxsxyz; static vec2_t *doomvoxsst; static int doomvoxscap;	// world-transform scratch
typedef struct { char name[16]; float ao; } doomvoxdef_t;	// VOXELDEF.txt per-voxel options
static doomvoxdef_t *doomvoxdef; static int doomvoxdefcount; static qboolean doomvoxdefloaded;

static int   Doom_LE32(const qbyte *p){ return (int)(p[0]|(p[1]<<8)|(p[2]<<16)|((unsigned)p[3]<<24)); }
static short Doom_LE16(const qbyte *p){ return (short)(p[0]|(p[1]<<8)); }
static int   Doom_VoxSolid(const qbyte *s,int sx,int sy,int sz,int x,int y,int z){
	if (x<0||y<0||z<0||x>=sx||y>=sy||z>=sz) return 0; return s[(x*sy+y)*sz+z]; }

static void Doom_LoadVoxelDef(void)
{	//parse VOXELDEF.txt (gzdoom format) for per-voxel options; we use AngleOffset (facing, degrees)
	char *file, *data; size_t sz=0;
	if (doomvoxdefloaded) return;
	doomvoxdefloaded=true;
	file=FS_LoadMallocFile("VOXELDEF.txt",&sz);
	if (!file) return;
	data=file;
	for(;;)
	{
		char nm[16]; float ao=0;
		data=COM_Parse(data); if(!data||!com_token[0]) break;		//entry name (frame)
		Q_strncpyz(nm,com_token,sizeof(nm));
		for(;;){ data=COM_Parse(data); if(!data||!com_token[0])break; if(!strcmp(com_token,"{"))break; }	//skip = "file"
		if(!data)break;
		for(;;){ data=COM_Parse(data); if(!data||!com_token[0]||!strcmp(com_token,"}"))break;
			if(!Q_strcasecmp(com_token,"AngleOffset")){ data=COM_Parse(data); if(!data)break; data=COM_Parse(data); if(!data)break; ao=atof(com_token); } }
		doomvoxdef=BZ_Realloc(doomvoxdef,(doomvoxdefcount+1)*sizeof(*doomvoxdef));
		Q_strncpyz(doomvoxdef[doomvoxdefcount].name,nm,sizeof(doomvoxdef[0].name));
		doomvoxdef[doomvoxdefcount].ao=ao; doomvoxdefcount++;
		if(!data)break;
	}
	BZ_Free(file);
}
static float Doom_VoxAngleOffset(const char *name)
{
	int i; Doom_LoadVoxelDef();
	for(i=0;i<doomvoxdefcount;i++) if(!Q_strcasecmp(doomvoxdef[i].name,name)) return doomvoxdef[i].ao;
	return 0;
}

static void Doom_BuildVoxel(doomvoxel_t *v, const char *name)
{	//parse the KVX and emit one quad per exposed voxel face, vertex-coloured from its palette
	char path[64]; qbyte *d=NULL; size_t fsz=0;
	int sx,sy,sz,numbytes,offsetsize,voxdatasize,x,y,z,dir,faces=0,vi=0,ii=0;
	float px,py,pz; const qbyte *pal,*slabbase; qbyte *solid; byte_vec4_t *cgrid;
	static const float fsh[6]={0.72f,0.72f,0.86f,0.86f,1.0f,0.5f};	//-x +x -y +y top bottom

	Q_strncpyz(v->name, name, sizeof(v->name)); v->tried=true; v->nverts=v->nidx=0;
	v->angleoffset = Doom_VoxAngleOffset(name);
	Q_snprintfz(path,sizeof(path),"voxels/%s.kvx",name);
	d=FS_LoadMallocFile(path,&fsz);
	if (!d || fsz<=768+28) { if(d)BZ_Free(d); return; }
	numbytes=Doom_LE32(d); sx=Doom_LE32(d+4); sy=Doom_LE32(d+8); sz=Doom_LE32(d+12);
	px=Doom_LE32(d+16)/256.0f; py=Doom_LE32(d+20)/256.0f; pz=Doom_LE32(d+24)/256.0f;
	if (sx<=0||sy<=0||sz<=0||sx>256||sy>256||sz>256) { BZ_Free(d); return; }
	offsetsize=(sx+1)*4 + sx*(sy+1)*2;
	voxdatasize=numbytes-24-offsetsize;
	if (voxdatasize<0 || (size_t)(28+offsetsize+voxdatasize) > fsz) { BZ_Free(d); return; }
	slabbase=d+28+offsetsize;
	pal=d+fsz-768;

	solid=BZ_Malloc(sx*sy*sz); memset(solid,0,sx*sy*sz);
	cgrid=BZ_Malloc(sx*sy*sz*sizeof(byte_vec4_t));
	for (x=0;x<sx;x++)
	{
		int xoff=Doom_LE32(d+28+x*4)-offsetsize;
		for (y=0;y<sy;y++)
		{
			int s0=xoff+Doom_LE16(d+28+(sx+1)*4 + (x*(sy+1)+y)*2);
			int s1=xoff+Doom_LE16(d+28+(sx+1)*4 + (x*(sy+1)+y+1)*2);
			const qbyte *p,*pe; if (s0<0||s1>voxdatasize||s1<s0) continue;
			p=slabbase+s0; pe=slabbase+s1;
			while (p+3<=pe)
			{
				int ztop=p[0],zleng=p[1],k; const qbyte *cols=p+3; p+=3+zleng;
				if (cols+zleng>pe) break;
				for (k=0;k<zleng;k++){ int zz=ztop+k,gi,c; if(zz<0||zz>=sz)continue;
					gi=(x*sy+y)*sz+zz; c=cols[k]; solid[gi]=1;
					cgrid[gi][0]=(pal[c*3+0]<<2)|(pal[c*3+0]>>4);
					cgrid[gi][1]=(pal[c*3+1]<<2)|(pal[c*3+1]>>4);
					cgrid[gi][2]=(pal[c*3+2]<<2)|(pal[c*3+2]>>4); cgrid[gi][3]=255; }
			}
		}
	}
	//count exposed faces (neighbour empty), then emit
	for (x=0;x<sx;x++) for (y=0;y<sy;y++) for (z=0;z<sz;z++){ if(!solid[(x*sy+y)*sz+z])continue;
		if(!Doom_VoxSolid(solid,sx,sy,sz,x-1,y,z))faces++; if(!Doom_VoxSolid(solid,sx,sy,sz,x+1,y,z))faces++;
		if(!Doom_VoxSolid(solid,sx,sy,sz,x,y-1,z))faces++; if(!Doom_VoxSolid(solid,sx,sy,sz,x,y+1,z))faces++;
		if(!Doom_VoxSolid(solid,sx,sy,sz,x,y,z-1))faces++; if(!Doom_VoxSolid(solid,sx,sy,sz,x,y,z+1))faces++; }
	if (faces>16250) faces=16250;	//cap: immediate-mode meshes / 16-bit indices stay under 65536 verts
	if (faces)
	{
		v->xyz=BZ_Malloc(faces*4*sizeof(vecV_t));
		v->col=BZ_Malloc(faces*4*sizeof(byte_vec4_t));
		v->idx=BZ_Malloc(faces*6*sizeof(index_t));
		for (x=0;x<sx;x++) for (y=0;y<sy;y++) for (z=0;z<sz;z++)
		{
			int gi=(x*sy+y)*sz+z; if(!solid[gi])continue;
			if (vi+24 > faces*4) { x=sx; y=sy; break; }	//hit the cap
			//local cube: y flipped (Build is left-handed), z up from the feet
			float x0=x-px, x1=x0+1, y0=-(y-py), y1=y0-1, zt=pz-z, zb=zt-1;
			for (dir=0;dir<6;dir++)
			{
				int nx=x,ny=y,nz=z; float q[4][3]; int j;
				if(dir==0)nx--; else if(dir==1)nx++; else if(dir==2)ny--; else if(dir==3)ny++; else if(dir==4)nz--; else nz++;
				if (Doom_VoxSolid(solid,sx,sy,sz,nx,ny,nz)) continue;
				switch(dir){
				case 0: q[0][0]=x0;q[0][1]=y0;q[0][2]=zb; q[1][0]=x0;q[1][1]=y1;q[1][2]=zb; q[2][0]=x0;q[2][1]=y1;q[2][2]=zt; q[3][0]=x0;q[3][1]=y0;q[3][2]=zt; break;
				case 1: q[0][0]=x1;q[0][1]=y0;q[0][2]=zb; q[1][0]=x1;q[1][1]=y1;q[1][2]=zb; q[2][0]=x1;q[2][1]=y1;q[2][2]=zt; q[3][0]=x1;q[3][1]=y0;q[3][2]=zt; break;
				case 2: q[0][0]=x0;q[0][1]=y0;q[0][2]=zb; q[1][0]=x1;q[1][1]=y0;q[1][2]=zb; q[2][0]=x1;q[2][1]=y0;q[2][2]=zt; q[3][0]=x0;q[3][1]=y0;q[3][2]=zt; break;
				case 3: q[0][0]=x0;q[0][1]=y1;q[0][2]=zb; q[1][0]=x1;q[1][1]=y1;q[1][2]=zb; q[2][0]=x1;q[2][1]=y1;q[2][2]=zt; q[3][0]=x0;q[3][1]=y1;q[3][2]=zt; break;
				case 4: q[0][0]=x0;q[0][1]=y0;q[0][2]=zt; q[1][0]=x1;q[1][1]=y0;q[1][2]=zt; q[2][0]=x1;q[2][1]=y1;q[2][2]=zt; q[3][0]=x0;q[3][1]=y1;q[3][2]=zt; break;
				default:q[0][0]=x0;q[0][1]=y0;q[0][2]=zb; q[1][0]=x1;q[1][1]=y0;q[1][2]=zb; q[2][0]=x1;q[2][1]=y1;q[2][2]=zb; q[3][0]=x0;q[3][1]=y1;q[3][2]=zb; break; }
				for (j=0;j<4;j++){ VectorCopy(q[j], v->xyz[vi+j]);
					v->col[vi+j][0]=(qbyte)(cgrid[gi][0]*fsh[dir]); v->col[vi+j][1]=(qbyte)(cgrid[gi][1]*fsh[dir]);
					v->col[vi+j][2]=(qbyte)(cgrid[gi][2]*fsh[dir]); v->col[vi+j][3]=255; }
				v->idx[ii+0]=vi+0; v->idx[ii+1]=vi+1; v->idx[ii+2]=vi+2;
				v->idx[ii+3]=vi+0; v->idx[ii+4]=vi+2; v->idx[ii+5]=vi+3;
				vi+=4; ii+=6;
			}
		}
		v->nverts=vi; v->nidx=ii;
	}
	BZ_Free(solid); BZ_Free(cgrid); BZ_Free(d);
}

static doomvoxel_t *Doom_GetVoxel(const char *name)
{	//cache lookup; build on first use
	int i;
	for (i=0;i<doomvoxcount;i++) if(!strcmp(doomvox[i].name,name)) return &doomvox[i];
	if (doomvoxcount>=doomvoxmax){ doomvoxmax=doomvoxmax?doomvoxmax*2:64; doomvox=BZ_Realloc(doomvox,doomvoxmax*sizeof(*doomvox)); }
	memset(&doomvox[doomvoxcount],0,sizeof(doomvox[0]));
	Doom_BuildVoxel(&doomvox[doomvoxcount], name);
	return &doomvox[doomvoxcount++];
}

static void Doom_DrawVoxel(doomvoxel_t *v, const vec3_t origin, float yawdeg, float scale)
{	//rotate the cached local mesh by yaw into world space and draw it (opaque, vertex-coloured)
	mesh_t mesh; int i; float c,s,a=(yawdeg+v->angleoffset)*(M_PI/180.0);
	c=cos(a); s=sin(a);
	if (v->nverts>doomvoxscap){ doomvoxscap=v->nverts+256;
		doomvoxsxyz=BZ_Realloc(doomvoxsxyz,doomvoxscap*sizeof(vecV_t));
		doomvoxsst =BZ_Realloc(doomvoxsst, doomvoxscap*sizeof(vec2_t));
		memset(doomvoxsst,0,doomvoxscap*sizeof(vec2_t)); }
	for (i=0;i<v->nverts;i++){ float lx=v->xyz[i][0]*scale, ly=v->xyz[i][1]*scale, lz=v->xyz[i][2]*scale;
		doomvoxsxyz[i][0]=origin[0]+lx*c-ly*s; doomvoxsxyz[i][1]=origin[1]+lx*s+ly*c; doomvoxsxyz[i][2]=origin[2]+lz; }
	memset(&mesh,0,sizeof(mesh));
	mesh.numvertexes=v->nverts; mesh.numindexes=v->nidx;
	mesh.xyz_array=doomvoxsxyz; mesh.st_array=doomvoxsst; mesh.colors4b_array=v->col; mesh.indexes=v->idx;
	BE_DrawMesh_Single(doomvoxshader,&mesh,NULL,0);
}

static void Doom_VoxShader(void)
{	//vertex-coloured opaque shader (white 1x1 diffuse, colour from the mesh), drawn double-sided
	texnums_t tn; unsigned int white=0xffffffff;
	if (doomvoxshader) return;
	doomvoxshader=R_RegisterShader("doom_voxel",SUF_NONE,"{\ncull none\n{\nmap $diffuse\nrgbgen vertex\n}\n}\n");
	memset(&tn,0,sizeof(tn));
	tn.base=R_LoadTexture32("doom_voxwhite",1,1,&white,IF_NOMIPMAP);
	R_BuildDefaultTexnums(&tn,doomvoxshader,0);
}

static qboolean Doom_DrawVoxelByName(const char *name, const vec3_t origin, float yawdeg, float scale)
{	//draw the named voxel if it exists/has geometry; returns false so the caller can fall back to a sprite
	doomvoxel_t *v=Doom_GetVoxel(name);
	if (!v || !v->nverts) return false;
	Doom_DrawVoxel(v, origin, yawdeg, scale);
	return true;
}

//draw monsters as upright camera-facing billboards (same technique as R_DoomDrawSprites).
static void R_DoomDrawMonsters(doommap_t *dm)
{
	unsigned int i;
	vec3_t viewang, vpn, vright, vup;
	mesh_t mesh;
	vecV_t xyz[4];
	vec2_t st[4] = {{0,0},{1,0},{1,1},{0,1}};
	vec2_t stm[4] = {{1,0},{0,0},{0,1},{1,1}};	//mirrored U, for flipped combined-lump rotations
	byte_vec4_t col[4];
	index_t idx[6] = {0,1,2, 0,2,3};

	int sprrot, sprfreeze, usevox;
	float voxscale, voxyaw;
	if (!dm->nummonsters && !dm->numprojectiles)
		return;
	sprrot = (int)Cvar_Get("doom_sprrot", "1", CVAR_ARCHIVE, "Doom Sprites")->value;	//1=8-way+mirror, 0=front only
	sprfreeze = (int)Cvar_Get("doom_sprfreeze", "-1", CVAR_ARCHIVE, "Doom Sprites")->value;	//>=0 holds that walk frame
	usevox  = (int)Cvar_Get("doom_voxels", "1", CVAR_ARCHIVE, "Doom")->value;	//1=voxel models, 0=sprites
	voxscale= Cvar_Get("doom_voxscale", "1", CVAR_ARCHIVE, "Doom")->value;		//world units per voxel
	voxyaw  = Cvar_Get("doom_voxyaw", "90", CVAR_ARCHIVE, "Doom")->value;		//facing offset (deg) for tuning
	if (usevox) Doom_VoxShader();
	viewang[0]=0; viewang[1]=r_refdef.viewangles[1]; viewang[2]=0;
	AngleVectors(viewang, vpn, vright, vup);
	Vector4Set(col[0],255,255,255,255); Vector4Set(col[1],255,255,255,255);
	Vector4Set(col[2],255,255,255,255); Vector4Set(col[3],255,255,255,255);
	memset(&mesh, 0, sizeof(mesh));
	mesh.numvertexes=4; mesh.numindexes=6;
	mesh.xyz_array=xyz; mesh.st_array=st; mesh.colors4b_array=col; mesh.indexes=idx;

	for (i = 0; i < dm->nummonsters; i++)
	{
		struct doommonster_s *m = &dm->monsters[i];
		shader_t *sh = NULL; short sw=0, shh=0, sxo=0, syo=0; qboolean mirror=false;
		const char *vbase=m->spr; char vlet=0;	//voxel name = vbase+vlet for the current frame
		float zb, zt; vec3_t l, r;
		if (m->mstate == 2)
		{	//dead: play death animation - or, if an archvile is raising it, the death frames in REVERSE
			int df;
			if (m->risetime >= 0)
			{	//resurrection (corpse Raise state): walk the death frames backwards
				df = (m->ndeath-1) - (int)(m->risetime / 0.07f);
				if (df < 0) df = 0;
			}
			else
			{
				df = (int)(m->deathtime / 0.15f);
				if (df >= m->ndeath) {
					if (m->atk & MATK_BARREL) continue;
					df = m->ndeath - 1;
				}
			}
			if (df < 0 || df >= m->ndeath) continue;
			sh = m->deathfr[df]; sw = m->dfw[df]; shh = m->dfh[df]; sxo = m->dfxo[df]; syo = 0;
			{ const char *ds=(m->atk&MATK_BARREL)?"ABCDE":Doom_DeathSeq(m->spr);
			  if(m->atk&MATK_BARREL)vbase="BEXP"; if(ds&&df<(int)strlen(ds))vlet=ds[df]; }
		}
		else if (m->paintime >= 0 && m->npain > 0)
		{	//pain: play the flinch frame for its loaded length
			int pf = (int)(m->paintime / 0.1f);
			if (pf >= m->npain) { m->paintime = -1; pf = 0; } //end pain
			sh = m->painfr[pf]; sw = m->pfw[pf]; shh = m->pfh[pf]; sxo = m->pfxo[pf]; syo = 0;
			vlet = Doom_PainFrame(m->spr);
		}
		else if (m->atktime >= 0 && m->natk > 0)
		{	//attacking: play the per-monster attack sequence (Doom_AttackSeq) at ~0.25s/frame so it
			//matches vanilla timing (former-human missile is ~0.74s) instead of flashing past in 0.3s.
			int af = (int)(m->atktime / 0.25f);
			if (af >= m->natk) { m->atktime = -1; af = 0; } //end attack
			if (m->atkfr[af]) {
				sh = m->atkfr[af]; sw = m->afw[af]; shh = m->afh[af]; sxo = m->afxo[af]; syo = 0;
			}
			{ const char *as=Doom_AttackSeq(m->spr); if(as&&af<(int)strlen(as))vlet=as[af]; }
		}
		else
		{	//alive: pick walk frame and rotation
			int wf = (m->alerted && m->nwalk > 1) ? ((int)(m->animt / 0.25f) % m->nwalk) : 0;
			if (sprfreeze >= 0) wf = sprfreeze % m->nwalk;	//diagnostic: hold a single walk frame
			//pick rotation (0-7) based on monster yaw vs view angle
			float ang = m->yaw - viewang[1] + 180 + 22.5f;
			int rot = ((int)(ang / 45.0f)) & 7;
			if (!sprrot) rot = 0;	//doom_sprrot 0 = front view only (isolates the 8-rotation/mirror code)
			sh = m->shader[wf][rot]; sw = m->w[wf][rot]; shh = m->h[wf][rot]; sxo = m->xo[wf][rot]; syo = m->yo[wf][rot];
			mirror = sprrot ? m->wmir[wf][rot] : 0;
			if (m->atk & MATK_BARREL)	//barrel idles by bobbing between BAR1 A and B (~0.17s each)
				vlet = ((int)(m->animt/0.17f)&1) ? 'B' : 'A';
			else { const char *ws=Doom_WalkFrames(m->spr); if(ws&&wf<(int)strlen(ws))vlet=ws[wf]; }
		}
		if (usevox && vlet && vbase)
		{	//voxel model for this frame, if one exists; otherwise fall through to the sprite
			char vn[16]; vec3_t tom;
			VectorSubtract(m->origin, r_refdef.vieworg, tom);
			if (DotProduct(tom, vpn) < -96) continue;	//behind the camera: skip the (heavy) voxel draw
			Q_snprintfz(vn,sizeof(vn),"%s%c",vbase,vlet);
			if (Doom_DrawVoxelByName(vn, m->origin, m->yaw+voxyaw, voxscale)) continue;
		}
		if (!sh || sw<=0 || shh<=0)	//a sprite that would blink/vanish: log it (silent otherwise)
			Con_Printf("DOOMSPRITE-MISSING t=%i mst=%i atkt=%.2f pt=%.2f dt=%.2f rt=%.2f | nw=%i na=%i np=%i nd=%i | sh=%p w=%i h=%i\n",
				m->type, m->mstate, m->atktime, m->paintime, m->deathtime, m->risetime,
				m->nwalk, m->natk, m->npain, m->ndeath, (void*)sh, (int)sw, (int)shh);
		if (!sh) continue;
		zb = m->origin[2] + syo - shh;
		zt = m->origin[2] + syo;
		//mirrored rotations measure the hotspot from the opposite edge and draw the texture flipped
		{ short xoe = mirror ? (short)(sw - sxo) : sxo;
		  VectorMA(m->origin, -xoe,      vright, l);
		  VectorMA(m->origin,  sw - xoe, vright, r); }
		mesh.st_array = mirror ? stm : st;
		VectorSet(xyz[0], l[0], l[1], zt); VectorSet(xyz[1], r[0], r[1], zt);
		VectorSet(xyz[2], r[0], r[1], zb); VectorSet(xyz[3], l[0], l[1], zb);
		BE_DrawMesh_Single(sh, &mesh, NULL, 0);
	}
	mesh.st_array = st;	//monsters above may have left it pointing at the mirrored set
	for (i = 0; i < dm->numprojectiles; i++)
	{
		struct doomproj_s *pr = &dm->projectiles[i];
		float zb = pr->origin[2] - pr->h/2.0f, zt = zb + pr->h;
		vec3_t l, r;
		if (!pr->shader) continue;
		VectorMA(pr->origin, -pr->w/2.0f, vright, l);
		VectorMA(l, pr->w, vright, r);
		VectorSet(xyz[0], l[0], l[1], zt); VectorSet(xyz[1], r[0], r[1], zt);
		VectorSet(xyz[2], r[0], r[1], zb); VectorSet(xyz[3], l[0], l[1], zb);
		BE_DrawMesh_Single(pr->shader, &mesh, NULL, 0);
	}
}

static void Doom_LoadShaders(void *ctx, void *data, size_t a, size_t b)
{
	model_t *mod = ctx;
	doommap_t *dm = mod->meshinfo;
	texnums_t tn;
	qboolean hasalpha = false;
	qboolean isflat;
	int texnum;
	char tmp[MAX_QPATH];
	char skyname[16] = "sky1", shadername[32], shaderbody[256];

	if (dm->skytex >= 0)
	{
		R_SetSky("");	//clear any global forced skybox - it takes precedence in R_DrawSkyChain
				//and the cubemap auto-path loads incompletely. The doom_sky shader below
				//carries its own skyparms (6-face) box, which uses the GL_DrawSkyBox path.

		//pick the sky per map, matching vanilla Doom: ExMy -> skyN (N=1..4, Doom 1
		//episodes); mapNN -> d2sky1 (1-11), d2sky2 (12-20), d2sky3 (21+) for Doom 2.
		{
			char mb[MAX_QPATH];
			const char *m;
			COM_FileBase(mod->name, mb, sizeof(mb));
			m = strchr(mb, '#');	//skip any pwad "wad#" prefix
			m = m ? m+1 : mb;
			if ((m[0]=='e'||m[0]=='E') && m[1]>='1' && m[1]<='4' && (m[2]=='m'||m[2]=='M'))
				Q_snprintfz(skyname, sizeof(skyname), "sky%c", m[1]);
			else if (!Q_strncasecmp(m, "map", 3))
			{
				int n = atoi(m+3);
				Q_strncpyz(skyname, (n<=11)?"d2sky1":(n<=20)?"d2sky2":"d2sky3", sizeof(skyname));
			}
		}
	}

	for (texnum = 0; texnum < dm->numtextures; texnum++)	//a hash table might be a good plan.
	{
		isflat = !strncmp(dm->textures[texnum].name, "flats/", 6);
		memset(&tn, 0, sizeof(tn));

		if (texnum == dm->skytex)
		{
			unsigned short sw = 256, sh = 128;
			tn.base = Doom_LoadPatchFromTexWad("SKY1", textures1, &sw, &sh, &hasalpha);
			if (!TEXVALID(tn.base) && textures2)
				tn.base = Doom_LoadPatchFromTexWad("SKY1", textures2, &sw, &sh, &hasalpha);
			dm->textures[texnum].width  = sw;
			dm->textures[texnum].height = sh;
			// Render F_SKY1 surfaces as a real 6-face skybox (env/sky1_{rt,bk,lf,ft,up,dn}),
			// like gzdoom's skybox, instead of tiling the SKY1 patch flat on the ceiling (the
			// smear) or stretching it over a sphere. 'skyparms' loads the box faces and gives the
			// shader a skydome, so R_DrawSkyChain takes the GL_DrawSkyBox path. 'sort sky' routes
			// this batch through R_DrawSkyChain; 'surfaceparm sky' sets the SHADER_SKY flag; the
			// sky is masked purely by depth (opaque geometry occludes it, sky openings show it).
			//unique shader name per sky so episodes don't share a cached shader
			Q_snprintfz(shadername, sizeof(shadername), "doom_sky_%s", skyname);
			Q_snprintfz(shaderbody, sizeof(shaderbody),
				"{\n"
					"sort sky\n"
					"skyparms \"%s\" 512 -\n"
					"surfaceparm nodlight\n"
					"surfaceparm sky\n"
				"}\n", skyname);
			dm->textures[texnum].shader = R_RegisterShader(shadername, SUF_NONE, shaderbody);
			R_BuildDefaultTexnums(&tn, dm->textures[texnum].shader, IF_WORLDTEX);
			continue;
		}

		if (isflat)
		{
			void *file = FS_LoadMallocFile(va2(tmp, sizeof(tmp), "%s.raw", dm->textures[texnum].name), NULL);
			if (file)
			{
				tn.base = Image_GetTexture(dm->textures[texnum].name, NULL, 0, file, doompalette, 64, 64, TF_8PAL24);
				Z_Free(file);
			}
			dm->textures[texnum].width = 64;
			dm->textures[texnum].height = 64;
		}
		else
		{
			if (textures1 && !TEXVALID(tn.base))
				tn.base = Doom_LoadPatchFromTexWad(dm->textures[texnum].name, textures1, &dm->textures[texnum].width, &dm->textures[texnum].height, &hasalpha);
			if (textures2 && !TEXVALID(tn.base))
				tn.base = Doom_LoadPatchFromTexWad(dm->textures[texnum].name, textures2, &dm->textures[texnum].width, &dm->textures[texnum].height, &hasalpha);
		}
		if (!TEXVALID(tn.base))
		{
			dm->textures[texnum].width = 64;
			dm->textures[texnum].height = 64;
			hasalpha = false;
		}

		if (hasalpha)
			dm->textures[texnum].shader = R_RegisterShader(dm->textures[texnum].name, SUF_NONE, "{\n{\nmap $diffuse\nrgbgen vertex\nalphagen vertex\nalphafunc ge128\n}\n}\n");
		else
			dm->textures[texnum].shader = R_RegisterShader(dm->textures[texnum].name, SUF_NONE, "{\n{\nmap $diffuse\nrgbgen vertex\nalphagen vertex\n}\n}\n");

		R_BuildDefaultTexnums(&tn, dm->textures[texnum].shader, 0);
	}

	//item/decoration billboards: resolve sprites now that textures+geometry are ready
	Doom_LoadThingSprites(dm);
	Doom_LoadMonsters(dm);
};

static void Doom_Purge (struct model_s *mod)
{
	int texnum;
	doommap_t *dm = mod->meshinfo;
	for (texnum = 0; texnum < dm->numtextures; texnum++)
	{
		BZ_Free(dm->textures[texnum].mesh.colors4b_array);
		BZ_Free(dm->textures[texnum].mesh.st_array);
		BZ_Free(dm->textures[texnum].mesh.xyz_array);
		BZ_Free(dm->textures[texnum].mesh.indexes);
	}
	BZ_Free(dm->textures);
	dm->textures = NULL;
	BZ_Free(dm->sprites);
	dm->sprites = NULL;
	dm->numsprites = 0;
	BZ_Free(dm->monsters);
	dm->monsters = NULL;
	dm->nummonsters = 0;
	doomvoxshader = NULL;	//shader system is reset between maps; rebuilt lazily on next draw
	BZ_Free(dm->projectiles);
	dm->projectiles = NULL;
	dm->numprojectiles = 0;
}
#endif
static void CleanWalls(doommap_t *dm, dsidedef_t *sidedefsl)
{
	int i;
	char texname[64];
	char lastmiddle[9]="-";
	char lastlower[9]="-";
	char lastupper[9]="-";
	int lastmidtex=0, lastuptex=0, lastlowtex=0;
	dm->sidedef = BZ_Malloc(dm->numsidedefs * sizeof(*dm->sidedef));
	for (i = 0; i < dm->numsidedefs; i++)
	{
#if 1//def GLQUAKE
		strncpy(texname, sidedefsl[i].middletex, 8);
		texname[8] = '\0';
		if (!strcmp(texname, "-"))
			dm->sidedef[i].middletex = 0;
		else
		{
			if (!strncmp(texname, lastmiddle, 8))
				dm->sidedef[i].middletex = lastmidtex;
			else
			{
				strncpy(lastmiddle, texname, 8);
				dm->sidedef[i].middletex = lastmidtex = Doom_LoadPatch(dm, texname);
			}
		}

		strncpy(texname, sidedefsl[i].lowertex, 8);
		texname[8] = '\0';
		if (!strcmp(texname, "-"))
			dm->sidedef[i].lowertex = 0;
		else
		{
			if (!strncmp(texname, lastlower, 8))
				dm->sidedef[i].lowertex = lastlowtex;
			else
			{
				strncpy(lastlower, texname, 8);
				dm->sidedef[i].lowertex = lastlowtex = Doom_LoadPatch(dm, texname);
			}
		}

		strncpy(texname, sidedefsl[i].uppertex, 8);
		texname[8] = '\0';
		if (!strcmp(texname, "-"))
			dm->sidedef[i].uppertex = 0;
		else
		{
			if (!strncmp(texname, lastupper, 8))
				dm->sidedef[i].uppertex = lastuptex;
			else
			{
				strncpy(lastupper, texname, 8);
				dm->sidedef[i].uppertex = lastuptex = Doom_LoadPatch(dm, texname);
			}
		}
#endif
		dm->sidedef[i].sector = sidedefsl[i].sector;
		dm->sidedef[i].texx = sidedefsl[i].texx;
		dm->sidedef[i].texy = sidedefsl[i].texy;
	}
}

void QuakifyThings(doommap_t *dm)
{
	msector_t *sector;
	int spawnflags;
	char *name;
	int i;
	int zpos;
	static char newlump[1024*1024]; //FIXME

	char *ptr = newlump;

	vec3_t point;

	sprintf(ptr,	"{\n"
					"\"classname\" \"worldspawn\"\n"
					"}\n");
	ptr += strlen(ptr);

	for (i = 0; i < dm->numthings; i++)
	{
		float zbias = 24;
		switch(dm->thing[i].type)
		{
		// ---- Spawns ----
		case THING_PLAYER:
			name = "info_player_start";
			break;
		case THING_PLAYER2:
		case THING_PLAYER3:
		case THING_PLAYER4:
			name = "info_player_coop";
			break;
		case THING_DMSPAWN:
			name = "info_player_deathmatch";
			break;

		// ---- Weapons ----
		case 2005:	// Chainsaw     → Axe (quad damage stand-in)
			name = "item_artifact_super_damage";
			break;
		case 2001:	// Shotgun      → Shotgun
			name = "weapon_shotgun";
			break;
		case 82:	// SSG          → Super Shotgun
			name = "weapon_supershotgun";
			break;
		case 2002:	// Chaingun     → Nailgun
			name = "weapon_nailgun";
			break;
		case 2003:	// Rocket launcher
			name = "weapon_rocketlauncher";
			break;
		case 2004:	// Plasma gun   → Grenade launcher
			name = "weapon_grenadelauncher";
			break;
		case 2006:	// BFG          → Lightning gun
			name = "weapon_lightning";
			break;

		// ---- Ammo ----
		case 2007:	// Clip         → Shells (small)
			name = "item_shells";
			break;
		case 2048:	// Box of bullets → Shells (large)
			name = "item_shells";
			break;
		case 2008:	// Shotgun shells → Shells
			name = "item_shells";
			break;
		case 2049:	// Box of shells
			name = "item_shells";
			break;
		case 2010:	// Rocket
			name = "item_rockets";
			break;
		case 2046:	// Box of rockets
			name = "item_rockets";
			break;
		case 2047:	// Cell charge    → Cells (no direct Quake equiv, use rockets)
			name = "item_rockets";
			break;
		case 17:	// Cell charge pack
			name = "item_rockets";
			break;

		// ---- Health ----
		case 2014:	// Health bonus  (+1) → small health
			name = "item_health";
			break;
		case 2011:	// Stimpack (+10)
			name = "item_health";
			break;
		case 2012:	// Medikit (+25)
			name = "item_health";
			break;
		case 2013:	// Soulsphere (+100%) → Megahealth
			name = "item_megahealth";
			break;

		// ---- Armor ----
		case 2015:	// Armor bonus
			name = "item_armor1";	// jacket armor
			break;
		case 2018:	// Security armor (green, 100%)
			name = "item_armor1";
			break;
		case 2019:	// Combat armor (blue, 200%)
			name = "item_armor2";
			break;
		case 2024:	// Mega armor
			name = "item_armorInv";
			break;

		// ---- Power-ups ----
		case 2023:	// Berserk pack  → Quad damage
			name = "item_artifact_super_damage";
			break;
		case 2022:	// Invisibility  → Ring of shadows
			name = "item_artifact_invisibility";
			break;
		case 2045:	// Rad suit      → Biosuit
			name = "item_artifact_envirosuit";
			break;
		case 2026:	// Computer map  → Pentagram (invulnerability as stand-in)
			name = "item_artifact_invulnerability";
			break;
		case 24:	// Light amp goggles → skip (DOOM type 24 is actually an artifact)
			name = "item_artifact_invulnerability";
			break;
		case 8:		// Backpack      → Backpack
			name = "item_backpack";
			break;

		// ---- Keys ----
		case 5:		// Blue keycard   → Silver key
			name = "item_key1";
			break;
		case 6:		// Yellow keycard → Gold key
			name = "item_key2";
			break;
		case 13:	// Red keycard    → Silver key (no third Quake key)
			name = "item_key1";
			break;
		case 40:	// Blue skull key → Silver key
			name = "item_key1";
			break;
		case 39:	// Yellow skull key → Gold key
			name = "item_key2";
			break;
		case 38:	// Red skull key   → Silver key
			name = "item_key1";
			break;

		// ---- Monsters ----
		case 3004:	// Zombieman        → Grunt
			name = "monster_army";
			zbias = 24;
			break;
		case 9:		// Shotgun Guy      → Enforcer (similar range weapon)
			name = "monster_enforcer";
			zbias = 24;
			break;
		case 65:	// Heavy Weapon Dude → Enforcer
			name = "monster_enforcer";
			zbias = 24;
			break;
		case 3001:	// Imp              → Demon (melee+fireball)
			name = "monster_demon";
			zbias = 24;
			break;
		case 3002:	// Demon/Pinky      → Knight
			name = "monster_knight";
			zbias = 24;
			break;
		case 58:	// Spectre          → Dog (fast melee)
			name = "monster_dog";
			zbias = 24;
			break;
		case 3006:	// Lost Soul        → Wizard (flying)
			name = "monster_wizard";
			zbias = 24;
			break;
		case 3005:	// Cacodemon        → Ogre (airborne-feel, projectile)
			name = "monster_ogre";
			zbias = 24;
			break;
		case 3003:	// Baron of Hell    → Hell Knight
			name = "monster_hell_knight";
			zbias = 24;
			break;
		case 69:	// Hell Knight (Doom2) → Knight
			name = "monster_knight";
			zbias = 24;
			break;
		case 7:		// Spider Mastermind → Shambler (boss)
			name = "monster_shambler";
			zbias = 24;
			break;
		case 16:	// Cyberdemon       → Shambler
			name = "monster_shambler";
			zbias = 24;
			break;
		case 64:	// Archvile         → Shalrath (Vore)
			name = "monster_shalrath";
			zbias = 24;
			break;
		case 67:	// Mancubus         → Ogre
			name = "monster_ogre";
			zbias = 24;
			break;
		case 68:	// Arachnotron      → Tarbaby
			name = "monster_tarbaby";
			zbias = 24;
			break;
		case 71:	// Pain Elemental   → Wizard
			name = "monster_wizard";
			zbias = 24;
			break;
		case 66:	// Revenant         → Enforcer
			name = "monster_enforcer";
			zbias = 24;
			break;
		case 72:	// Keen             → skip
			name = NULL;
			break;
		case 84:	// Commander Keen   → skip
			name = NULL;
			break;

		default:
			name = NULL;	// skip unknown things
			zbias = 0;
			break;
		}

		if (!name)
			continue;	// skip things with no Quake equivalent

		point[0] = dm->thing[i].xpos;
		point[1] = dm->thing[i].ypos;
		point[2] = 0;
		sector = Doom_SectorNearPoint(dm, point);
		zpos = sector->floorheight + zbias;	//things have no z coord, so find the sector they're in

		if (dm->thing[i].type == THING_PLAYER && !doom_player1_start[0] && !doom_player1_start[1])
		{
			doom_player1_start[0] = dm->thing[i].xpos;
			doom_player1_start[1] = dm->thing[i].ypos;
			doom_player1_start[2] = zpos;
			doom_player1_yaw = dm->thing[i].angle;
		}

		spawnflags = SPAWNFLAG_NOT_EASY | SPAWNFLAG_NOT_MEDIUM | SPAWNFLAG_NOT_HARD | SPAWNFLAG_NOT_DEATHMATCH;
		if (dm->thing[i].flags & THING_EASY)
			spawnflags -= SPAWNFLAG_NOT_EASY;
		if (dm->thing[i].flags & THING_MEDIUM)
			spawnflags -= SPAWNFLAG_NOT_MEDIUM;
		if (dm->thing[i].flags & THING_HARD)
			spawnflags -= SPAWNFLAG_NOT_HARD;
		if (dm->thing[i].flags & THING_DEATHMATCH)
			spawnflags -= SPAWNFLAG_NOT_DEATHMATCH;
		if (dm->thing[i].flags & THING_DEAF)
			spawnflags |= 1;

		Q_snprintfz(ptr, newlump+sizeof(newlump)-ptr,	"{\n"
						"\"classname\" \"%s\"\n"
						"\"origin\" \"%i %i %i\"\n"
						"\"spawnflags\" \"%i\"\n"
						"\"angle\" \"%i\"\n"
						"}\n",
							name,
							dm->thing[i].xpos, dm->thing[i].ypos, zpos,
							spawnflags,
							dm->thing[i].angle
						);
		ptr += strlen(ptr);
	}

	Mod_SetEntitiesStringLen(dm->model, newlump, ptr-newlump);
}

void Doom_GeneratePlanes(doommap_t *dm)
{
	vec3_t point, up, line;
	int n;
	up[0] = 0;
	up[1] = 0;
	up[2] = 1;
	line[2] = 0;
	dm->nodeplane = BZ_Malloc(sizeof(*dm->nodeplane)*dm->numnodes);
	dm->lineplane = BZ_Malloc(sizeof(*dm->lineplane)*dm->numlinedefs);
	point[2] = 0;
	for (n = 0; n < dm->numnodes; n++)
	{
		line[0] = dm->node[n].dx;
		line[1] = dm->node[n].dy;
		point[0] = dm->node[n].x;
		point[1] = dm->node[n].y;
		CrossProduct(line, up, dm->nodeplane[n].normal);
		VectorNormalize(dm->nodeplane[n].normal);
		dm->nodeplane[n].dist = DotProduct (point, dm->nodeplane[n].normal);
	}

	for (n = 0; n < dm->numlinedefs; n++)
	{
		point[0] = dm->vertexes[dm->linedef[n].vert[0]].xpos;
		point[1] = dm->vertexes[dm->linedef[n].vert[0]].ypos;
		line[0] = dm->vertexes[dm->linedef[n].vert[1]].xpos-point[0];
		line[1] = dm->vertexes[dm->linedef[n].vert[1]].ypos-point[1];
		CrossProduct(line, up, dm->lineplane[n].normal);
		VectorNormalize(dm->lineplane[n].normal);
		dm->lineplane[n].dist = DotProduct (point, dm->lineplane[n].normal);
	}
}

/*
doom maps have no network limitations, but has +/-32767 map size limits (same as quake bsp)
fte defaults to a +/- 4096 world
a lot of maps are off-centered and can be moved to get them to fit fte's constraints, so if we can, do so
*/
static void MoveWorld(doommap_t *dm)
{
	int v;
	short adj[2];
	short min[2], max[2];
	min[0] = 4096;
	min[1] = 4096;
	max[0] = -4096;
	max[1] = -4096;

	for (v = 0; v < dm->numvertexes; v++)
	{
		if (min[0] > dm->vertexes[v].xpos)
			min[0] = dm->vertexes[v].xpos;
		if (min[1] > dm->vertexes[v].ypos)
			min[1] = dm->vertexes[v].ypos;

		if (max[0] < dm->vertexes[v].xpos)
			max[0] = dm->vertexes[v].xpos;
		if (max[1] < dm->vertexes[v].ypos)
			max[1] = dm->vertexes[v].ypos;
	}

	if (min[0]>=-4096 && max[0]<=4096)
		if (min[1]>=-4096 && max[1]<=4096)
			adj[0] = adj[1] = 0;	//doesn't need adjusting, live with it.

	if (max[0]-min[0]>=8192 || max[1]-min[1]>=8192)
	{
		Con_Printf(CON_WARNING "Warning: Map is too large for the network protocol\n");
		adj[0] = adj[1] = 0;
	}
	else
	{
		adj[0] = (max[0]-4096)&~63;	//don't harm the tiling.
		adj[1] = (max[1]-4096)&~63;
	}

	dm->model->mins[0] = min[0] - adj[0];
	dm->model->mins[1] = min[1] - adj[1];
	dm->model->mins[2] = -32768;

	dm->model->maxs[0] = max[0] - adj[0];
	dm->model->maxs[1] = max[1] - adj[1];
	dm->model->maxs[2] = 32767;

	if (!adj[0] && !adj[1])
		return;

	Con_Printf("Adjusting map (%i %i)\n", -adj[0], -adj[1]);

	for (v = 0; v < dm->numvertexes; v++)
	{
		dm->vertexes[v].xpos -= adj[0];
		dm->vertexes[v].ypos -= adj[1];
	}

	for (v = 0; v < dm->numnodes; v++)
	{
		dm->node[v].x -= adj[0];
		dm->node[v].y -= adj[1];

		dm->node[v].x1lower -= adj[0];
		dm->node[v].x1upper -= adj[0];
		dm->node[v].y1lower -= adj[1];
		dm->node[v].y1upper -= adj[1];

		dm->node[v].x2lower -= adj[0];
		dm->node[v].x2upper -= adj[0];
		dm->node[v].y2lower -= adj[1];
		dm->node[v].y2upper -= adj[1];
	}

	for (v = 0; v < dm->numthings; v++)
	{
		dm->thing[v].xpos -= adj[0];
		dm->thing[v].ypos -= adj[1];
	}

	dm->blockmap->xorg -= adj[0];
	dm->blockmap->yorg -= adj[1];
}


//Doom maps without glBSP GL nodes can't tessellate sector flats properly (segs lack the BSP
//partition edges) - floors/ceilings come out floating/partial with black HOM. When a map has
//no GL nodes we build them on the fly (see engine/gl/doom_glbsp.cpp, wrapping ZDBSP) and feed
//the resulting gNd2 lumps to the normal loaders via these in-memory overrides.
enum {DGL_VERT, DGL_SEGS, DGL_SSECT, DGL_NODES, DGL_COUNT};
#ifdef HAVE_DOOM_GLBSP
extern int Doom_GLBSP_Build(const void*,int, const void*,int, const void*,int, const void*,int,
	void**,int*, void**,int*, void**,int*, void**,int*);

static void  *doom_glbuf[DGL_COUNT];
static size_t doom_glbufsz[DGL_COUNT];

static void Doom_ClearGLBuild(void)
{
	int i;
	for (i = 0; i < DGL_COUNT; i++)
	{
		if (doom_glbuf[i]) Z_Free(doom_glbuf[i]);
		doom_glbuf[i] = NULL;
		doom_glbufsz[i] = 0;
	}
}

static qboolean Doom_TryBuildGLNodes(const char *name)
{
	char tmp[MAX_QPATH];
	size_t vl=0, ll=0, sl=0, cl=0;
	void *verts, *lines=NULL, *sides=NULL, *sects=NULL;
	void *ov=NULL,*os=NULL,*oss=NULL,*on=NULL;
	int ovl=0,osl=0,ossl=0,onl=0;
	qboolean ok=false;

	verts = FS_LoadMallocFile(va2(tmp,sizeof(tmp),"%s.vertexes", name), &vl);
	if (verts)
	{
		lines = FS_LoadMallocFile(va2(tmp,sizeof(tmp),"%s.linedefs", name), &ll);
		sides = FS_LoadMallocFile(va2(tmp,sizeof(tmp),"%s.sidedefs", name), &sl);
		sects = FS_LoadMallocFile(va2(tmp,sizeof(tmp),"%s.sectors",  name), &cl);
	}
	if (verts && lines && sides && sects &&
		Doom_GLBSP_Build(verts,(int)vl, lines,(int)ll, sides,(int)sl, sects,(int)cl,
			&ov,&ovl, &os,&osl, &oss,&ossl, &on,&onl))
	{
		Doom_ClearGLBuild();
		doom_glbuf[DGL_VERT]=ov;   doom_glbufsz[DGL_VERT]=ovl;
		doom_glbuf[DGL_SEGS]=os;   doom_glbufsz[DGL_SEGS]=osl;
		doom_glbuf[DGL_SSECT]=oss; doom_glbufsz[DGL_SSECT]=ossl;
		doom_glbuf[DGL_NODES]=on;  doom_glbufsz[DGL_NODES]=onl;
		ok = true;
		Con_DPrintf("Doom: built GL nodes for %s (no glBSP data in wad)\n", name);
	}
	if (verts) Z_Free(verts);
	if (lines) Z_Free(lines);
	if (sides) Z_Free(sides);
	if (sects) Z_Free(sects);
	return ok;
}

//returns a GL lump, preferring a just-built in-memory buffer (ownership transfers to caller).
static void *Doom_LoadGLLump(int which, const char *fname, size_t *sz)
{
	if (doom_glbuf[which])
	{
		void *b = doom_glbuf[which];
		*sz = doom_glbufsz[which];
		doom_glbuf[which] = NULL;
		doom_glbufsz[which] = 0;
		return b;
	}
	return FS_LoadMallocFile(fname, sz);
}
#else
#define Doom_TryBuildGLNodes(name) false
#define Doom_ClearGLBuild()
#define Doom_LoadGLLump(which, fname, sz) FS_LoadMallocFile(fname, sz)
#endif


static void Doom_LoadVerticies(doommap_t *dm, char *name)
{
	ddoomvertex_t *std, *gl1;
	int stdc, glc;
	int *gl2, *gl2base;
	int i;
	size_t fsize;
	char tmp[MAX_QPATH];

	std		= (void *)FS_LoadMallocFile	(va2(tmp,sizeof(tmp),"%s.vertexes",	name), &fsize);
	stdc	= fsize/sizeof(*std);

	gl2		= (void *)Doom_LoadGLLump	(DGL_VERT, va2(tmp,sizeof(tmp),"%s.gl_vert",	name), &fsize);
	gl2base	= gl2;	//gl2 gets advanced past the "gNd2" magic below, but we must free the original allocation pointer.
	if (!gl2)
	{
		glc = 0;
		gl1 = NULL;
	}
	else if (gl2[0] == (('g'<<0)|('N'<<8)|('d'<<16)|('2'<<24)))
	{
		gl2++;
		glc = (fsize-4)/sizeof(int)/2;
		gl1 = NULL;
	}
	else
	{
		glc	= fsize/sizeof(*gl1);
		gl1 = (ddoomvertex_t*)gl2;
	}

	if (stdc)
	{
		dm->numvertexes = stdc + glc;
		dm->vertexes = BZ_Malloc(dm->numvertexes*sizeof(*dm->vertexes));

		dm->vertexsglbase = stdc;

		for (i = 0; i < stdc; i++)
		{
			dm->vertexes[i].xpos = std[i].xpos;
			dm->vertexes[i].ypos = std[i].ypos;
		}
		if (gl1)
		{
			for (i = 0; i < glc; i++)
			{
				dm->vertexes[stdc+i].xpos = gl1[i].xpos;
				dm->vertexes[stdc+i].ypos = gl1[i].ypos;
			}
		}
		else
		{
			for (i = 0; i < glc; i++)
			{
				dm->vertexes[stdc+i].xpos = (float)gl2[i*2] / 0x10000;
				dm->vertexes[stdc+i].ypos = (float)gl2[i*2+1] / 0x10000;
			}
		}
	}
	Z_Free(std);
	Z_Free(gl2base);
}

static void Doom_LoadSSectors(doommap_t *dm, char *name)
{
	dssector_t *in;
	size_t fsize;
	unsigned int i;
	char tmp[MAX_QPATH];
	in	= (void *)Doom_LoadGLLump	(DGL_SSECT, va2(tmp, sizeof(tmp), "%s.gl_ssect",	name), &fsize);
	if (!in)
		in	= (void *)FS_LoadMallocFile	(va2(tmp, sizeof(tmp), "%s.ssectors",	name), &fsize);
	//FIXME: "gNd3" means that it's glbsp version 3.
	dm->numssectors	= fsize/sizeof(*in);

	dm->ssector = Z_Malloc(dm->numssectors * sizeof(*dm->ssector));
	for (i = 0; i < dm->numssectors; i++)
	{
		dm->ssector[i].segcount = in[i].segcount;
		dm->ssector[i].first = in[i].first;
	}
	Z_Free(in);
}
static void Doom_CalcSubsectorSectors(doommap_t *dm)
{	//kinda shitty
	unsigned int num, seg;
	for (num = 0; num < dm->numssectors; num++)
	{
		dm->ssector[num].sector = &dm->sector[dm->sidedef[dm->linedef[dm->seg[dm->ssector[num].first].linedef].sidedef[dm->seg[dm->ssector[num].first].direction]].sector];
		for (seg = dm->ssector[num].first+1; seg < dm->ssector[num].first + dm->ssector[num].segcount; seg++)
			if (dm->seg[seg].linedef != 0xffff)
			{
				dm->ssector[num].sector = &dm->sector[dm->sidedef[dm->linedef[dm->seg[seg].linedef].sidedef[dm->seg[seg].direction]].sector];
				break;
			}
	}
}

static void Doom_LoadSSegs(doommap_t *dm, char *name)
{	//these skirt the subsectors

	void *file;
	dgl_seg3_t	*s3;
	dgl_seg1_t	*s1;
	dseg_t		*s0;
	int i;
	size_t fsize;
	char tmp[MAX_QPATH];

	file	= (void *)Doom_LoadGLLump	(DGL_SEGS, va2(tmp, sizeof(tmp), "%s.gl_segs",	name), &fsize);
	if (!file)
	{
		s0 = (void *)FS_LoadMallocFile	(va2(tmp, sizeof(tmp), "%s.segs",	name), &fsize);
		dm->numsegs	= fsize/sizeof(*s0);

		dm->seg = BZ_Malloc(dm->numsegs * sizeof(*dm->seg));
		for (i = 0; i < dm->numsegs; i++)
		{
			dm->seg[i].vert[0] = s0[i].vert[0];
			dm->seg[i].vert[1] = s0[i].vert[1];
			dm->seg[i].linedef = s0[i].linedef;
			dm->seg[i].direction = s0[i].direction;
			dm->seg[i].Partner = 0xffff;
		}
	}
	else if (*(int *)file == *(int *)"gNd3")
	{
		s3 = file;
		dm->numsegs	= fsize/sizeof(*s3);

		dm->seg = s3;
	}
	else if (!file)
		return;
	else
	{
		s1 = file;
		dm->numsegs	= fsize/sizeof(*s1);

		dm->seg = BZ_Malloc(dm->numsegs * sizeof(*dm->seg));
		for (i = 0; i < dm->numsegs; i++)
		{
			if (s1[i].vert[0] & 0x8000)
				dm->seg[i].vert[0] = (s1[i].vert[0]&0x7fff)+dm->vertexsglbase;
			else
				dm->seg[i].vert[0] = s1[i].vert[0];
			if (s1[i].vert[1] & 0x8000)
				dm->seg[i].vert[1] = (s1[i].vert[1]&0x7fff)+dm->vertexsglbase;
			else
				dm->seg[i].vert[1] = s1[i].vert[1];
			dm->seg[i].linedef = s1[i].linedef;
			dm->seg[i].direction = s1[i].direction;
			if (s1[i].Partner == 0xffff)
				dm->seg[i].Partner = 0xffffffff;
			else
				dm->seg[i].Partner = s1[i].Partner;
		}
	}
}

qboolean QDECL Mod_LoadDoomLevel(model_t *mod, void *buffer, size_t fsize)
{
	int h;
	dsector_t		*sectorl;
	dsidedef_t		*sidedefsl;
	char name[MAX_QPATH];
	char tmp[MAX_QPATH];
	doommap_t *dm;

	int *gl_nodes;

	if (fsize != 4)
	{
		Con_Printf("Wad map %s does actually exist... weird.\n", mod->name);
		return false;
	}

	dm = Z_Malloc(sizeof(*dm));
	dm->model = mod;
	mod->meshinfo = dm;
	dm->skytex = -1;
	VectorClear(doom_player1_start);
	doom_player1_yaw = 0;

	COM_StripExtension(mod->name, name, sizeof(name));

	gl_nodes	= (void *)Doom_LoadGLLump	(DGL_NODES, va2(tmp,sizeof(tmp),"%s.gl_nodes",	name), &fsize);
	if (!gl_nodes && Doom_TryBuildGLNodes(name))	//no GL nodes in the wad - build them on the fly so flats render correctly.
		gl_nodes = (void *)Doom_LoadGLLump(DGL_NODES, va2(tmp,sizeof(tmp),"%s.gl_nodes", name), &fsize);
	if (gl_nodes && fsize>0)
	{
		dm->node = (void *)gl_nodes;
		dm->numnodes = fsize/sizeof(*dm->node);
	}
	else
	{
		gl_nodes=NULL;
		dm->node		= (void *)FS_LoadMallocFile	(va2(tmp,sizeof(tmp),"%s.nodes",		name), &fsize);
		dm->numnodes		= fsize/sizeof(*dm->node);
	}
	sectorl		= (void *)FS_LoadMallocFile	(va2(tmp,sizeof(tmp),"%s.sectors",	name), &fsize);
	dm->numsectors		= fsize/sizeof(*sectorl);
	dm->sector = Z_Malloc(dm->numsectors * sizeof(*dm->sector));

#ifndef SERVERONLY
	dm->numtextures=0;
	Doom_LoadPalette();
#endif


	Doom_LoadVerticies(dm, name);

	Doom_LoadSSegs(dm, name);
	Doom_LoadSSectors(dm, name);

	dm->thing		= (void *)FS_LoadMallocFile	(va2(tmp,sizeof(tmp),"%s.things",	name), &fsize);
	dm->numthings		= fsize/sizeof(*dm->thing);
	dm->linedef	= (void *)FS_LoadMallocFile	(va2(tmp,sizeof(tmp),"%s.linedefs",	name), &fsize);
	dm->numlinedefs	= fsize/sizeof(*dm->linedef);
	sidedefsl	= (void *)FS_LoadMallocFile	(va2(tmp,sizeof(tmp),"%s.sidedefs",	name), &fsize);
	dm->numsidedefs	= fsize/sizeof(*sidedefsl);
	dm->blockmap	= (void *)FS_LoadMallocFile	(va2(tmp,sizeof(tmp),"%s.blockmap",	name), &fsize);

#ifndef SERVERONLY
	Doom_LoadTextureInfos();
#endif
	dm->blockmapofs = (unsigned short*)(dm->blockmap+1);

	if (!dm->node || !sectorl || !dm->seg || !dm->ssector || !dm->thing || !dm->linedef || !sidedefsl || !dm->vertexes)
	{
		Sys_Error("Wad map doesn't contain enough lumps\n");
		dm->node = NULL;
		return false;
	}

	MoveWorld(dm);

	Doom_GeneratePlanes(dm);

	mod->hulls[0].clip_mins[0] = 0;
	mod->hulls[0].clip_mins[1] = 0;
	mod->hulls[0].clip_mins[2] = 0;
	mod->hulls[0].clip_maxs[0] = 0;
	mod->hulls[0].clip_maxs[1] = 0;
	mod->hulls[0].clip_maxs[2] = 0;
	mod->hulls[0].available = true;

	for (h = 1; h < MAX_MAP_HULLSM; h++)
		mod->hulls[h].available = false;

	Doom_SetModelFunc(mod);

	mod->fromgame = fg_doom;
	mod->type = mod_brush;
	mod->nodes = (void*)0x1;
	mod->numclusters = dm->numsectors;

	CleanWalls(dm, sidedefsl);

	Doom_CalcSubsectorSectors(dm);

	Triangulate_Sectors(dm, sectorl, !!gl_nodes);

	QuakifyThings(dm);

	COM_AddWork(WG_MAIN, Doom_LoadShaders, mod, NULL, 0, 0);
	return true;
}

static void Doom_LightPointValues(model_t *model, const vec3_t point, vec3_t res_diffuse, vec3_t res_ambient, vec3_t res_dir)
{
	doommap_t *dm = model->meshinfo;
	msector_t *sec;
	sec = Doom_SectorNearPoint(dm, point);

	res_dir[0] = 0;
	res_dir[1] = 1;
	res_dir[2] = 1;
	res_diffuse[0] = sec->lightlev;
	res_diffuse[1] = sec->lightlev;
	res_diffuse[2] = sec->lightlev;
	res_ambient[0] = sec->lightlev;
	res_ambient[1] = sec->lightlev;
	res_ambient[2] = sec->lightlev;
}

//return pvs bits for point
static unsigned int Doom_FatPVS(struct model_s *model, const vec3_t org, pvsbuffer_t *pvsbuffer, qboolean merge)
{
	//FIXME: use REJECT lump.
	return 0;
}

//check if an ent is within the given pvs
static qboolean Doom_EdictInFatPVS(struct model_s *model, const struct pvscache_s *edict, const qbyte *pvsbuffer, const int *areas)
{	//FIXME: use REJECT lump.
	return true;
}

static int Doom_ClusterForPoint(struct model_s *model, const vec3_t point, int *areaout)
{
	doommap_t *dm = model->meshinfo;
	return Doom_SectorNearPoint(dm, point) - dm->sector;
}
static qbyte *Doom_ClusterPVS(struct model_s *model, int cluster, pvsbuffer_t *pvsbuffer, pvsmerge_t merge)
{	//FIXME: use REJECT lump.
	return NULL;
}

//generate useful info for correct functioning of Doom_EdictInFatPVS.
static void Doom_FindTouchedLeafs(struct model_s *model, struct pvscache_s *ent, const vec3_t cullmins, const vec3_t cullmaxs)
{
	//work out the sectors this ent is in for easy pvs.
}

//requires lightmaps - not supported.
static void Doom_StainNode(struct model_s *model, float *parms)
{
}

//requires lightmaps - not supported.
static void Doom_MarkLights(struct dlight_s *light, dlightbitmask_t bit, struct mnode_s *node)
{
}

void Doom_SetModelFunc(model_t *mod)
{
	doommap_t *dm = mod->meshinfo;
	dm->model = mod;
#ifndef SERVERONLY
	mod->funcs.PurgeModel			= Doom_Purge;
#endif
	mod->funcs.FatPVS				= Doom_FatPVS;
	mod->funcs.EdictInFatPVS		= Doom_EdictInFatPVS;
	mod->funcs.FindTouchedLeafs		= Doom_FindTouchedLeafs;
	mod->funcs.ClusterForPoint		= Doom_ClusterForPoint;
	mod->funcs.ClusterPVS			= Doom_ClusterPVS;

	mod->funcs.LightPointValues		= Doom_LightPointValues;
	mod->funcs.StainNode			= Doom_StainNode;
	mod->funcs.MarkLights			= Doom_MarkLights;

//	mod->funcs.LeafPVS)			(struct model_s *model, int num, qbyte *buffer, unsigned int buffersize);

	mod->funcs.NativeTrace			= Doom_Trace;
	mod->funcs.PointContents		= Doom_PointContents;

	//Doom_SetCollisionFuncs(mod);
}

#endif
