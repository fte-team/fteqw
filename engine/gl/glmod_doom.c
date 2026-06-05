#include "quakedef.h"
#ifdef MAP_DOOM
#include "glquake.h"
#include "shader.h"

vec3_t doom_player1_start;
float  doom_player1_yaw;



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

	// Sector ceiling/floor animation (doors, lifts, etc.)
	struct doorsector_s {
		int		sector_idx;
		int		state;		// 0=closed, 1=opening, 2=open_wait, 3=closing
		short	ceil_target;
		short	ceil_original;
		float	wait_time;
		qboolean repeating;	// DR = reopens on use; D1 = stays open
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
		qbyte		exploded;	// barrels: 1 once the blast has been dealt
		float		animt;		// walk-cycle animation timer (front frames A-D in shader[0..3])
		float		deathtime;	// seconds since killed (drives the death-frame animation; <0 = alive)
		vec3_t		spawnorigin;// initial state, restored on a full map reset (player respawn)
		float		spawnyaw;
		int			spawnhealth;
		shader_t	*shader[8];	// per-rotation sprite shader (rot 1..8 -> [0..7]); [0] is the fallback
		short		w[8], h[8], xo[8], yo[8];
		shader_t	*deathfr[12];	// death animation sequence (last frame is the resting corpse)
		short		dfw[12], dfh[12], dfxo[12];
		qbyte		ndeath;		// number of loaded death frames
	} *monsters;
	unsigned int nummonsters;

	// monster/player projectiles (imp/caco/baron balls, revenant homing tracer, rockets, ...)
	struct doomproj_s {
		vec3_t		origin, vel;	// position + velocity (units/sec)
		int			damage;			// applied to whatever it hits
		float		life;			// seconds before it self-expires
		qbyte		homing;			// 1 = steer toward the player each frame (revenant tracer)
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
		return true;
	}
	return false;
}

static int Doom_FindOrAddDoor(doommap_t *dm, int sec_idx)
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
	return (int)dm->numactive_doors++;
}

// Called when player activates a linedef (USE or walk-over)
void Doom_ActivateLinedef(model_t *model, int linedef_idx)
{
	doommap_t *dm = model->meshinfo;
	dlinedef_t *ld;
	int special, tag;
	int sec_idx;
	int door_idx;
	unsigned int j;
	short highest_adj;
	short lowest_ceil;
	int s;

	if (!dm || linedef_idx < 0 || (unsigned)linedef_idx >= dm->numlinedefs)
		return;
	ld = dm->linedef + linedef_idx;
	special = ld->types;
	tag = ld->tag;

	if (!Doom_IsActivatableLinedef(special))
		return;

	// Find all sectors with the matching tag (or use back sector for door type 1)
	if (tag == 0)
	{
		// Type 1: activate the back sector directly
		if (ld->sidedef[1] == 0xffff)
			return;
		sec_idx = dm->sidedef[ld->sidedef[1]].sector;

		// Find highest adjacent floor for door target ceiling
		highest_adj = dm->sector[sec_idx].floorheight;
		for (j = 0; j < dm->numlinedefs; j++)
		{
			if (dm->linedef[j].sidedef[1] == 0xffff) continue;
			if (dm->sidedef[dm->linedef[j].sidedef[0]].sector == (unsigned short)sec_idx ||
				dm->sidedef[dm->linedef[j].sidedef[1]].sector == (unsigned short)sec_idx)
			{
				int other = (dm->sidedef[dm->linedef[j].sidedef[0]].sector == (unsigned short)sec_idx)
					? dm->sidedef[dm->linedef[j].sidedef[1]].sector
					: dm->sidedef[dm->linedef[j].sidedef[0]].sector;
				if (dm->sector[other].ceilingheight - 4 > highest_adj)
					highest_adj = dm->sector[other].ceilingheight - 4;
			}
		}

		door_idx = Doom_FindOrAddDoor(dm, sec_idx);
		if (dm->doorsectors[door_idx].state == 0 || dm->doorsectors[door_idx].state == 3)
		{
			dm->doorsectors[door_idx].state = 1; // opening
			dm->doorsectors[door_idx].ceil_target = highest_adj;
			dm->doorsectors[door_idx].repeating = (special == 1 || special == 26 || special == 27 || special == 28 || special == 117);
		}
		else if (dm->doorsectors[door_idx].state == 2)
		{
			dm->doorsectors[door_idx].state = 3; // start closing now
		}
	}
	else
	{
		// Tagged sector action
		for (s = 0; s < (int)dm->numsectors; s++)
		{
			if (dm->sector[s].tag != tag)
				continue;
			sec_idx = s;

			// Find lowest adjacent ceiling for target
			lowest_ceil = 32767;
			for (j = 0; j < dm->numlinedefs; j++)
			{
				dlinedef_t *ld2 = dm->linedef+j;
				if (ld2->sidedef[1] == 0xffff) continue;
				int a = dm->sidedef[ld2->sidedef[0]].sector;
				int b = dm->sidedef[ld2->sidedef[1]].sector;
				int other = -1;
				if (a == sec_idx) other = b;
				else if (b == sec_idx) other = a;
				if (other < 0) continue;
				if (dm->sector[other].ceilingheight - 4 < lowest_ceil)
					lowest_ceil = dm->sector[other].ceilingheight - 4;
			}
			if (lowest_ceil == 32767) lowest_ceil = dm->sector[sec_idx].ceilingheight;

			door_idx = Doom_FindOrAddDoor(dm, sec_idx);
			if (dm->doorsectors[door_idx].state == 0 || dm->doorsectors[door_idx].state == 3)
			{
				dm->doorsectors[door_idx].state = 1;
				dm->doorsectors[door_idx].ceil_target = lowest_ceil;
				dm->doorsectors[door_idx].repeating = false;
			}
		}
	}
}

// Per-frame tick: animate all active door sectors
void Doom_TickDoors(model_t *model, float frametime)
{
	doommap_t *dm = model->meshinfo;
	unsigned int i;
	int move;

	if (!dm) return;

	for (i = 0; i < dm->numactive_doors; i++)
	{
		struct doorsector_s *d = &dm->doorsectors[i];
		msector_t *sec = &dm->sector[d->sector_idx];

		move = (int)(DOOR_SPEED * frametime);
		if (move < 1) move = 1;

		switch(d->state)
		{
		case 1:	// opening
			sec->ceilingheight += move;
			if (sec->ceilingheight >= d->ceil_target)
			{
				sec->ceilingheight = d->ceil_target;
				if (d->repeating)
				{
					d->state = 2;
					d->wait_time = DOOR_WAIT;
				}
				else
					d->state = 0;	// D1: stays open (but don't remove — leave at open)
			}
			break;
		case 2:	// waiting before auto-close
			d->wait_time -= frametime;
			if (d->wait_time <= 0)
				d->state = 3;
			break;
		case 3:	// closing
			sec->ceilingheight -= move;
			if (sec->ceilingheight <= sec->floorheight + 4)
			{
				sec->ceilingheight = sec->floorheight + 4;
				d->state = 0;
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
static int Doom_BoxOnLineSide(float bminx, float bminy, float bmaxx, float bmaxy, dlinedef_t *ld, doommap_t *dm)
{
	mdoomvertex_t *v1 = &dm->vertexes[ld->vert[0]];
	mdoomvertex_t *v2 = &dm->vertexes[ld->vert[1]];
	float dx = v2->xpos - v1->xpos, dy = v2->ypos - v1->ypos;
	int p1, p2;
	if (dy == 0)
	{	//horizontal line
		p1 = bmaxy > v1->ypos;
		p2 = bminy > v1->ypos;
		if (dx < 0) { p1 ^= 1; p2 ^= 1; }
	}
	else if (dx == 0)
	{	//vertical line
		p1 = bmaxx < v1->xpos;
		p2 = bminx < v1->xpos;
		if (dy < 0) { p1 ^= 1; p2 ^= 1; }
	}
	else
	{	//diagonal: test the two opposite box corners chosen by the slope's sign
		int pos = (dx*dy) > 0;
		float cxa = pos ? bminx : bmaxx, cya = bmaxy;
		float cxb = pos ? bmaxx : bminx, cyb = bminy;
		p1 = ((cxa - v1->xpos)*dy - (cya - v1->ypos)*dx) > 0;
		p2 = ((cxb - v1->xpos)*dy - (cyb - v1->ypos)*dx) > 0;
	}
	return (p1 == p2) ? p1 : -1;
}

qboolean Doom_Trace(model_t *model, int hulloverride, const framestate_t *framestate, const vec3_t axis[3], const vec3_t start, const vec3_t end, const vec3_t mins, const vec3_t maxs, qboolean iscapsule, unsigned int contentstype, trace_t *trace)
{
	doommap_t *dm = model->meshinfo;
#if 1
#define TRACESTEP	16
	unsigned short *linedefs;
	dlinedef_t *ld;
	int bmi;
	vec3_t delta;
	msector_t *sec1 = Doom_SectorNearPoint(dm, start);
	vec3_t p1, pointonplane, ofs;
	float d1, d2, c1, c2, planedist;
	plane_t *lp;
	mdoomvertex_t *v1, *v2;
	msector_t *fs, *bs;
	float opentop, openbot;
	int j;
	float p1f, p2f;

	float clipfrac;
#define	DIST_EPSILON	(0.03125)

//	Con_Printf("%i\n", sec1);

	if (start[2] < sec1->floorheight-mins[2])	//whoops, started outside... ?
	{
		trace->fraction = 0;
		trace->allsolid = trace->startsolid = true;
		trace->endpos[0] = start[0];
		trace->endpos[1] = start[1];
		trace->endpos[2] = start[2];	//yeah, we do mean this - startsolid
		trace->plane.normal[0] = 0;
		trace->plane.normal[1] = 0;
		trace->plane.normal[2] = 1;
		trace->plane.dist = sec1->floorheight-mins[2];

		return false;
	}
	if (start[2] > sec1->ceilingheight-maxs[2])	//whoops, started outside... ?
	{
		trace->fraction = 0;
		trace->allsolid = trace->startsolid = true;
		trace->endpos[0] = start[0];
		trace->endpos[1] = start[1];
		trace->endpos[2] = start[2];
		trace->plane.normal[0] = 0;
		trace->plane.normal[1] = 0;
		trace->plane.normal[2] = -1;
		trace->plane.dist = -(sec1->ceilingheight-maxs[2]);
		return false;
	}

	VectorSubtract(end, start, delta);
	p2f = Length(delta)+DIST_EPSILON;
	if (IS_NAN(p2f) || p2f > 100000)
		p2f = 100000;
	VectorNormalize(delta);

	trace->endpos[0] = end[0];
	trace->endpos[1] = end[1];
	trace->endpos[2] = end[2];

	VectorCopy(start, p1);
	p1f = 0;

	trace->fraction = 1;
	while(1)
	{
		//Test every blockmap cell the player's bounding box overlaps at this point, not just the
		//cell under its centre. A blockmap lists a linedef only in the cells the LINE crosses, so a
		//wall lying in an adjacent cell - within the 16u box radius - would otherwise never be tested
		//and the box would slide straight into it and stick. (Doom does this via P_BlockLinesIterator
		//over the bounding box.) Retesting a linedef across cells/steps is harmless: the clips below
		//only ever shrink trace->fraction.
		int bx, by;
		int bxlo = ((int)(p1[0]+mins[0]) - dm->blockmap->xorg)/128;
		int bxhi = ((int)(p1[0]+maxs[0]) - dm->blockmap->xorg)/128;
		int bylo = ((int)(p1[1]+mins[1]) - dm->blockmap->yorg)/128;
		int byhi = ((int)(p1[1]+maxs[1]) - dm->blockmap->yorg)/128;
		for (by = bylo; by <= byhi; by++)
		for (bx = bxlo; bx <= bxhi; bx++)
		{
			bmi = bx + by*dm->blockmap->columns;
			if (bmi < 0 || bmi >= dm->blockmap->rows*dm->blockmap->columns)
				continue;
			for(linedefs = (short*)dm->blockmap + dm->blockmapofs[bmi]+1; *linedefs != 0xffff; linedefs++)
			{
				ld = dm->linedef + *linedefs;
				if (ld->sidedef[1] != 0xffff)
				{
					if (dm->sector[dm->sidedef[ld->sidedef[0]].sector].floorheight == dm->sector[dm->sidedef[ld->sidedef[1]].sector].floorheight &&
						dm->sector[dm->sidedef[ld->sidedef[0]].sector].ceilingheight == dm->sector[dm->sidedef[ld->sidedef[1]].sector].ceilingheight)
						continue;
				}
				
				lp = dm->lineplane + *linedefs;

				if (1)
				{	//figure out how far to move the plane out by
					for (j=0 ; j<2 ; j++)
					{
						if (lp->normal[j] < 0)
							ofs[j] = maxs[j];
						else
							ofs[j] = mins[j];
					}
					ofs[2] = 0;
					planedist = lp->dist - DotProduct (ofs, lp->normal);
				}
				else
					planedist = lp->dist;

				d1 = DotProduct(lp->normal, start) - (planedist);
				d2 = DotProduct(lp->normal, end) - (planedist);
				if (d1 > 0 && d2 > 0)
					continue;	//both points on the front side.
				if (d1 < 0)	//start on back side
				{
					if (ld->sidedef[1] != 0xffff)	//two sided (optimisation)
					{
						planedist = -planedist+lp->dist;
						if (/*d1 < planedist*-1 &&*/ d1 > planedist*2)
						{	//right, we managed to end up just on the other side of a wall's plane.
							v1 = &dm->vertexes[ld->vert[0]];
							v2 = &dm->vertexes[ld->vert[1]];
							if (!(d1 - d2))
								continue;
							if (d1<0)	//back to front.
								c1 = (d1+DIST_EPSILON) / (d1 - d2);
							else
								c1 = (d1-DIST_EPSILON) / (d1 - d2);
							c2 = 1-c1;
							pointonplane[0] = start[0]*c2 + end[0]*c1;
/*							if (pointonplane[0] > v1->xpos+DIST_EPSILON*2+hull->clip_maxs[0] && pointonplane[0] > v2->xpos+DIST_EPSILON*2+hull->clip_maxs[0])
								continue;
							if (pointonplane[0] < v1->xpos-DIST_EPSILON*2+hull->clip_mins[0] && pointonplane[0] < v2->xpos-DIST_EPSILON*2+hull->clip_mins[0])
								continue;
*/							pointonplane[1] = start[1]*c2 + end[1]*c1;
/*							if (pointonplane[1] > v1->ypos+DIST_EPSILON*2+hull->clip_maxs[1] && pointonplane[1] > v2->ypos+DIST_EPSILON*2+hull->clip_maxs[1])
								continue;
							if (pointonplane[1] < v1->ypos-DIST_EPSILON*2+hull->clip_mins[1] && pointonplane[1] < v2->ypos-DIST_EPSILON*2+hull->clip_mins[1])
								continue;
*/
							pointonplane[2] = start[2]*c2 + end[2]*c1;

							;//Con_Printf("Started in wall\n");
							j = dm->sidedef[ld->sidedef[d1 < planedist]].sector;
							//yup, we are in the thing
							//prevent ourselves from entering the back-sector's floor/ceiling at the point of impact
							if (pointonplane[2] < dm->sector[j].floorheight-mins[2])	//whoops, started outside... ?
							{
								;//Con_Printf("Started in floor\n");
								trace->allsolid = trace->startsolid = false;
								trace->endpos[2] = dm->sector[j].floorheight-mins[2];
								trace->fraction = fabs(trace->endpos[2] - start[2]) / fabs(end[2] - start[2]);
								trace->endpos[0] = start[0]+delta[0]*trace->fraction*p2f;
								trace->endpos[1] = start[1]+delta[1]*trace->fraction*p2f;
						//		if (IS_NAN(trace->endpos[2]))
						//			Con_Printf("Nanny\n");
								trace->plane.normal[0] = 0;
								trace->plane.normal[1] = 0;
								trace->plane.normal[2] = 1;
								trace->plane.dist = dm->sector[j].floorheight-mins[2];

								continue;
							}
							if (pointonplane[2] > dm->sector[j].ceilingheight-maxs[2])	//whoops, started outside... ?
							{
								;//Con_Printf("Started in ceiling\n");
								trace->allsolid = trace->startsolid = false;
								trace->endpos[0] = pointonplane[0];
								trace->endpos[1] = pointonplane[1];
								trace->endpos[2] = dm->sector[j].ceilingheight-maxs[2];
								trace->fraction = fabs(trace->endpos[2] - start[2]) / fabs(end[2] - start[2]);
								trace->plane.normal[0] = 0;
								trace->plane.normal[1] = 0;
								trace->plane.normal[2] = -1;
								trace->plane.dist = -(dm->sector[j].ceilingheight-maxs[2]);
								continue;
							}
						}
					}
					if (d2 < 0)
						continue;	//both points on the reverse side.
				}

				//line crosses plane.

				v1 = &dm->vertexes[ld->vert[0]];
				v2 = &dm->vertexes[ld->vert[1]];

				if (d1<0)	//back to front.
				{
					if (ld->sidedef[1] == 0xffff)
						continue;	//hack to allow them to pass
					c1 = (d1+DIST_EPSILON) / (d1 - d2);
				}
				else
					c1 = (d1-DIST_EPSILON) / (d1 - d2);
				c2 = 1-c1;
				pointonplane[0] = start[0]*c2 + end[0]*c1;
				if (pointonplane[0] > v1->xpos+DIST_EPSILON*2+maxs[0] && pointonplane[0] > v2->xpos+DIST_EPSILON*2+maxs[0])
					continue;
				if (pointonplane[0] < v1->xpos-DIST_EPSILON*2+mins[0] && pointonplane[0] < v2->xpos-DIST_EPSILON*2+mins[0])
					continue;
				pointonplane[1] = start[1]*c2 + end[1]*c1;
				if (pointonplane[1] > v1->ypos+DIST_EPSILON*2+maxs[1] && pointonplane[1] > v2->ypos+DIST_EPSILON*2+maxs[1])
					continue;
				if (pointonplane[1] < v1->ypos-DIST_EPSILON*2+mins[1] && pointonplane[1] < v2->ypos-DIST_EPSILON*2+mins[1])
					continue;
				pointonplane[2] = start[2]*c2 + end[2]*c1;

				//a two-sided line is a solid wall if the player can't fit through the opening
				//(closed door, low lintel), not just when explicitly impassable. Without this the
				//player walks horizontally through a closed door and gets stuck startsolid inside.
				fs = &dm->sector[dm->sidedef[ld->sidedef[0]].sector];
				bs = (ld->sidedef[1] != 0xffff) ? &dm->sector[dm->sidedef[ld->sidedef[1]].sector] : fs;
				opentop = (fs->ceilingheight < bs->ceilingheight) ? fs->ceilingheight : bs->ceilingheight;
				openbot = (fs->floorheight  > bs->floorheight)  ? fs->floorheight  : bs->floorheight;
				//A two-sided line is a solid wall to the player if it's explicitly impassable, one-sided,
				//the gap is too short to fit through, OR the opening's floor (a window sill / raised ledge)
				//is more than 24u above the sector the player is standing in. That last step-up limit is
				//what vanilla Doom enforces on the floor (it has no jump) - without it you hop out of windows.
				if (ld->flags & LINEDEF_IMPASSABLE || ld->sidedef[1] == 0xffff
					|| opentop - openbot < maxs[2] - mins[2]
					|| openbot - ((d1 > 0) ? fs : bs)->floorheight > 24)
				{	//unconditionally clipped - the wall clip below blocks horizontally.
				}
				else
				{	//ensure that the side we are passing on to passes the clip (no ceiling/floor clips happened first)
					msector_t *sec2;

					if (d1<0)
						sec2 = &dm->sector[dm->sidedef[ld->sidedef[1]].sector];
					else
						sec2 = &dm->sector[dm->sidedef[ld->sidedef[0]].sector];

					if (pointonplane[2] < sec2->floorheight-mins[2])
					{	//hit the floor first.
						c1 = fabs(sec1->floorheight-mins[2] - start[2]);
						c2 = fabs(end[2] - start[2]);
						if (!c2)
							c1 = 1;
						else
							c1 = (c1-DIST_EPSILON) / c2;
						if (trace->fraction > c1)
						{
//							Con_Printf("Hit floor\n");
							trace->fraction = c1;
							trace->allsolid = trace->startsolid = true;
							trace->endpos[0] = start[0] + trace->fraction*(end[0]-start[0]);
							trace->endpos[1] = start[1] + trace->fraction*(end[1]-start[1]);
							trace->endpos[2] = start[2] + trace->fraction*(end[2]-start[2]);
							trace->plane.normal[0] = 0;
							trace->plane.normal[1] = 0;
							trace->plane.normal[2] = 1;
							trace->plane.dist = sec1->floorheight-mins[2];
						}
						continue;
					}

					if (pointonplane[2] > sec2->ceilingheight-maxs[2])
					{	//hit the floor first.
						c1 = fabs((sec1->ceilingheight-maxs[2]) - start[2]);
						c2 = fabs(end[2] - start[2]);
						if (!c2)
							c1 = 1;
						else
							c1 = (c1-DIST_EPSILON) / c2;


						if (trace->fraction > c1)
						{
//							Con_Printf("Hit ceiling\n");
							trace->fraction = c1;
							trace->allsolid = trace->startsolid = true;
							trace->endpos[0] = start[0] + trace->fraction*(end[0]-start[0]);
							trace->endpos[1] = start[1] + trace->fraction*(end[1]-start[1]);
							trace->endpos[2] = start[2] + trace->fraction*(end[2]-start[2]);
							trace->plane.normal[0] = 0;
							trace->plane.normal[1] = 0;
							trace->plane.normal[2] = -1;
							trace->plane.dist = -(sec1->ceilingheight-maxs[2]);
						}
						continue;
					}

					if (d1<0)
						sec2 = &dm->sector[dm->sidedef[ld->sidedef[0]].sector];
					else
						sec2 = &dm->sector[dm->sidedef[ld->sidedef[1]].sector];

					if(sec2->ceilingheight == sec2->floorheight)
						sec2->ceilingheight += 64;

					if (pointonplane[2] > sec2->floorheight-mins[2] &&
						pointonplane[2] < sec2->ceilingheight-maxs[2])
					{
//						Con_Printf("Two sided passed\n");
						continue;
					}

//					Con_Printf("blocked by two sided line\n");
//					sec2->floorheight--;
				}

				if (d1<0)	//back to front.
					c1 = (d1+DIST_EPSILON) / (d1 - d2);
				else
					c1 = (d1-DIST_EPSILON) / (d1 - d2);


				clipfrac = c1;

				if (clipfrac < 0)
					clipfrac = 0;
				if (clipfrac > 1)
					clipfrac = 1;

				if (trace->fraction > clipfrac)
				{
					trace->fraction = clipfrac;
					VectorMA(pointonplane, 0, lp->normal, trace->endpos);
					VectorMA(trace->endpos, -0.1, delta, trace->endpos);
//					if (IS_NAN(trace->endpos[2]))
//						Con_Printf("Buggy clipping\n");
					VectorCopy(lp->normal, trace->plane.normal);
					trace->plane.dist = planedist;
//					if (IS_NAN(trace->plane.normal[2]))
//						Con_Printf("Buggy clipping\n");

					//if (clipfrac)
					//	Con_Printf("Clip Wall %f\n", clipfrac);
				}
			}	//end linedef loop
		}	//end blockmap-cell neighbourhood loop

		p1f += TRACESTEP;
		if (p1f >= p2f)
			break;

		VectorMA(p1, TRACESTEP, delta, p1);
	}

//	VectorMA(start, p2f*trace->fraction, delta, p2);

	if (end[2] != start[2])
	{
		if (sec1 == Doom_SectorNearPoint(dm, trace->endpos))	//special test.
		{
			if (end[2] <= sec1->floorheight-mins[2])	//whoops, started outside... ?
			{
				p1f = fabs(sec1->floorheight-mins[2] - start[2]);
				p2f = fabs(end[2] - start[2]);
				if (!p2f)
					c1 = 1;
				else
					c1 = (p1f-DIST_EPSILON) / p2f;
				if (trace->fraction > c1)
				{
					trace->fraction = c1;
					trace->allsolid = trace->startsolid = false;
					trace->endpos[0] = start[0] + trace->fraction*(end[0]-start[0]);
					trace->endpos[1] = start[1] + trace->fraction*(end[1]-start[1]);
					trace->endpos[2] = start[2] + trace->fraction*(end[2]-start[2]);
					trace->plane.normal[0] = 0;
					trace->plane.normal[1] = 0;
					trace->plane.normal[2] = 1;
					trace->plane.dist = sec1->floorheight-mins[2];
				}
			}
			if (end[2] >= sec1->ceilingheight-maxs[2])	//whoops, started outside... ?
			{
				p1f = fabs(sec1->ceilingheight-maxs[2] - start[2]);
				p2f = fabs(end[2] - start[2]);
				if (!p2f)
					c1 = 1;
				else
					c1 = (p1f-DIST_EPSILON) / p2f;
				if (trace->fraction > c1)
				{
					trace->fraction = c1;
					trace->allsolid = trace->startsolid = false;
					trace->endpos[0] = start[0] + trace->fraction*(end[0]-start[0]);
					trace->endpos[1] = start[1] + trace->fraction*(end[1]-start[1]);
					trace->endpos[2] = start[2] + trace->fraction*(end[2]-start[2]);
					trace->plane.normal[0] = 0;
					trace->plane.normal[1] = 0;
					trace->plane.normal[2] = -1;
					trace->plane.dist = -(sec1->ceilingheight-maxs[2]);
				}
			}
		}
	}

	//player vs monsters: the world trace above only knows the map's linedefs, so also clip the
	//move against live monsters (and intact barrels) - solid upright cylinders of radius m->radius.
	//Without this the player walks straight through them. Only for box (player) traces.
	if (maxs[0] > mins[0])
	{
		float pr = maxs[0];	//player half-width (~16)
		vec3_t fulld;
		unsigned int mi2;
		VectorSubtract(end, start, fulld);
		for (mi2 = 0; mi2 < dm->nummonsters; mi2++)
		{
			struct doommonster_s *m = &dm->monsters[mi2];
			float R, tmin, tmax, plo, phi, mlo, mhi;
			qboolean miss = false;
			int ax;
			if (m->mstate == 2)
				continue;	//corpses (and spent barrels) don't block
			//vertical overlap: the player's z span vs the monster's (feet..feet+sprite height)
			plo = (start[2] < end[2] ? start[2] : end[2]) + mins[2];
			phi = (start[2] > end[2] ? start[2] : end[2]) + maxs[2];
			mlo = m->origin[2];
			mhi = m->origin[2] + (m->h[0] ? m->h[0] : 56);
			if (phi <= mlo || plo >= mhi)
				continue;	//on different levels: no horizontal collision
			//2D slab test of the move ray vs the monster cylinder expanded by the player radius
			R = m->radius + pr;
			tmin = 0; tmax = 1;
			for (ax = 0; ax < 2; ax++)
			{
				float c = m->origin[ax], d = fulld[ax], s = start[ax], t1, t2;
				if (d > -1e-6f && d < 1e-6f)
				{	//not moving on this axis: must already straddle the slab
					if (s < c - R || s > c + R) { miss = true; break; }
				}
				else
				{
					t1 = (c - R - s) / d;
					t2 = (c + R - s) / d;
					if (t1 > t2) { float tt = t1; t1 = t2; t2 = tt; }
					if (t1 > tmin) tmin = t1;
					if (t2 < tmax) tmax = t2;
					if (tmin > tmax) { miss = true; break; }
				}
			}
			if (miss || tmin < 0 || tmin >= trace->fraction)
				continue;	//miss / already overlapping at the start / further than the current clip
			trace->fraction = tmin;
			trace->endpos[0] = start[0] + tmin*fulld[0];
			trace->endpos[1] = start[1] + tmin*fulld[1];
			trace->endpos[2] = start[2] + tmin*fulld[2];
			{	//push-away normal so the player slides around the monster
				float nx = trace->endpos[0]-m->origin[0], ny = trace->endpos[1]-m->origin[1];
				float nl = sqrt(nx*nx+ny*ny);
				if (nl < 1e-6f) { nx = -fulld[0]; ny = -fulld[1]; nl = sqrt(nx*nx+ny*ny); if (nl<1e-6f){nx=1;ny=0;nl=1;} }
				trace->plane.normal[0] = nx/nl;
				trace->plane.normal[1] = ny/nl;
				trace->plane.normal[2] = 0;
				trace->plane.dist = DotProduct(trace->plane.normal, trace->endpos);
			}
		}
	}

	//P_CheckPosition-style destination guard (chocolate-doom PIT_CheckLine): the swept clips above
	//can still leak the box a hair through a wall while gliding into a corner, after which the point
	//->sector lookup lands in the void and everything goes wrong. Test the FINAL box against every
	//blocking line in its blockmap cells with the integer box-vs-line side test, and if it straddles
	//a solid one, push it back out along that line's normal so it can NEVER end inside a wall. Box
	//(player) traces only; a few passes resolve a multi-wall corner.
	if (maxs[0] > mins[0] && dm->blockmap)
	{
		int pass, bx, by;
		for (pass = 0; pass < 4; pass++)
		{
			qboolean pushed = false;
			float bminx = trace->endpos[0]+mins[0], bmaxx = trace->endpos[0]+maxs[0];
			float bminy = trace->endpos[1]+mins[1], bmaxy = trace->endpos[1]+maxs[1];
			int vbxlo = ((int)bminx - dm->blockmap->xorg)/128, vbxhi = ((int)bmaxx - dm->blockmap->xorg)/128;
			int vbylo = ((int)bminy - dm->blockmap->yorg)/128, vbyhi = ((int)bmaxy - dm->blockmap->yorg)/128;
			for (by = vbylo; by <= vbyhi; by++)
			for (bx = vbxlo; bx <= vbxhi; bx++)
			{
				int vbmi = bx + by*dm->blockmap->columns;
				unsigned short *vl;
				if (vbmi < 0 || vbmi >= dm->blockmap->rows*dm->blockmap->columns)
					continue;
				for (vl = (unsigned short*)dm->blockmap + dm->blockmapofs[vbmi]+1; *vl != 0xffff; vl++)
				{
					dlinedef_t *vld = dm->linedef + *vl;
					qboolean solid;
					float d;
					if (Doom_BoxOnLineSide(bminx,bminy,bmaxx,bmaxy, vld, dm) != -1)
						continue;	//box entirely on one side - doesn't straddle this line
					if (vld->sidedef[1] == 0xffff)
						solid = true;	//one-sided wall
					else
					{
						msector_t *vfs = &dm->sector[dm->sidedef[vld->sidedef[0]].sector];
						msector_t *vbs = &dm->sector[dm->sidedef[vld->sidedef[1]].sector];
						float vot = (vfs->ceilingheight < vbs->ceilingheight) ? vfs->ceilingheight : vbs->ceilingheight;
						float vob = (vfs->floorheight   > vbs->floorheight)   ? vfs->floorheight   : vbs->floorheight;
						solid = (vld->flags & LINEDEF_IMPASSABLE)
							|| (vot - vob < maxs[2] - mins[2])	//opening too short to fit
							|| (vob - sec1->floorheight > 24);	//step up too tall (P_TryMove's 24u limit)
					}
					if (!solid)
						continue;
					//straddling a SOLID line: depenetrate endpos to its front face + epsilon.
					lp = dm->lineplane + *vl;
					for (j = 0; j < 2; j++)
						ofs[j] = (lp->normal[j] < 0) ? maxs[j] : mins[j];
					ofs[2] = 0;
					planedist = lp->dist - DotProduct(ofs, lp->normal);
					d = DotProduct(lp->normal, trace->endpos) - planedist;
					if (d < DIST_EPSILON)
					{
						VectorMA(trace->endpos, (DIST_EPSILON - d), lp->normal, trace->endpos);
						VectorCopy(lp->normal, trace->plane.normal);
						trace->plane.dist = planedist;
						if (trace->fraction == 1)
							trace->fraction = 0.99f;	//tell pmove the move was obstructed
						pushed = true;
					}
				}
			}
			if (!pushed)
				break;
		}
	}

	//we made it all the way through. yay.

	trace->allsolid = trace->startsolid = false;
//Con_Printf("total = %f\n", trace->fraction);
#endif
	return trace->fraction==1;
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

	if (!dm->numsprites)
		return;

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
	qboolean hasalpha = false;
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
		texnums_t stn;
		short sw, sh, sxo, syo;
		vec3_t p;
		msector_t *sec;
		struct doomsprite_s *out;
		char sname[32];

		if (!spr)
			continue;
		if (dm->thing[i].flags & THING_DEATHMATCH)
			continue;	//multiplayer-only thing (MTF_NOTSINGLE): not present in single player
		tex = Doom_LoadSprite(spr, &sw, &sh, &sxo, &syo);
		if (!TEXVALID(tex))
			continue;

		//one alpha-tested shader per sprite texture (cached by name, like wall textures)
		Q_snprintfz(sname, sizeof(sname), "doom_sprite_%s", spr);
		memset(&stn, 0, sizeof(stn));
		stn.base = tex;
		out = BZ_Realloc(dm->sprites, sizeof(*dm->sprites)*(dm->numsprites+1));
		dm->sprites = out;
		out += dm->numsprites;
		out->shader = R_RegisterShader(sname, SUF_NONE,
			"{\n"
				"cull none\n"		//billboard quad is visible from either winding
				"{\n"
					"map $diffuse\n"
					"alphafunc ge128\n"	//1-bit Doom sprite alpha: crisp edges + correct depth
					"depthwrite\n"
				"}\n"
			"}\n");
		R_BuildDefaultTexnums(&stn, out->shader, IF_NOMIPMAP);

		p[0] = dm->thing[i].xpos; p[1] = dm->thing[i].ypos; p[2] = 0;
		sec = Doom_SectorNearPoint(dm, p);
		out->origin[0] = p[0];
		out->origin[1] = p[1];
		out->origin[2] = sec ? sec->floorheight : 0;
		out->w = sw; out->h = sh; out->xo = sxo; out->yo = syo;
		out->pickup = Doom_IsPickup(dm->thing[i].type);
		out->type = dm->thing[i].type;
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
	case 3006: o->spr="SKUL"; o->health=100; o->radius=16; o->speed=170; o->atk=MATK_MELEE; o->meleedmg=3; break;	//lost soul
	//melee + missile
	case 3001: o->spr="TROO"; o->health=60;  o->radius=20; o->speed=130; o->atk=MATK_MELEE|MATK_MISSILE; o->meleedmg=3; o->misdmg=3;  o->misspeed=200; break;	//imp
	case 3005: o->spr="HEAD"; o->health=400; o->radius=31; o->speed=130; o->atk=MATK_MELEE|MATK_MISSILE; o->meleedmg=10;o->meleerand=6;o->misdmg=5;  o->misspeed=200; break;	//cacodemon: melee (1..6)*10
	case 3003: o->spr="BOSS"; o->health=1000;o->radius=24; o->speed=130; o->atk=MATK_MELEE|MATK_MISSILE; o->meleedmg=10;o->misdmg=8;  o->misspeed=300; break;	//baron
	case 69:   o->spr="BOS2"; o->health=500; o->radius=24; o->speed=130; o->atk=MATK_MELEE|MATK_MISSILE; o->meleedmg=10;o->misdmg=8;  o->misspeed=300; break;	//hell knight
	case 66:   o->spr="SKEL"; o->health=300; o->radius=20; o->speed=170; o->atk=MATK_MELEE|MATK_MISSILE|MATK_HOMING; o->meleedmg=6; o->meleerand=10; o->misdmg=10; o->misspeed=200; break;	//revenant: melee (1..10)*6
	//missile only
	case 67:   o->spr="FATT"; o->health=600; o->radius=48; o->speed=130; o->atk=MATK_MISSILE; o->misdmg=8; o->misspeed=400; break;	//mancubus
	case 68:   o->spr="BSPI"; o->health=500; o->radius=64; o->speed=170; o->atk=MATK_MISSILE; o->misdmg=5; o->misspeed=500; break;	//arachnotron
	case 16:   o->spr="CYBR"; o->health=4000;o->radius=40; o->speed=130; o->atk=MATK_MISSILE; o->misdmg=20;o->misspeed=400; break;	//cyberdemon (rockets)
	case 71:   o->spr="PAIN"; o->health=400; o->radius=31; o->speed=130; o->atk=0; break;	//pain elemental (spawns souls - chase only for now)
	//archvile hellfire
	case 64:   o->spr="VILE"; o->health=700; o->radius=20; o->speed=200; o->atk=MATK_VILE; o->misdmg=20; break;	//arch-vile
	//exploding barrel: not a monster, but reuses the shootable + death-animation path
	case 2035: o->spr="BAR1"; o->health=20;  o->radius=10; o->speed=0;   o->atk=MATK_BARREL; break;	//barrel (BEXP blast on death)
	default: return false;
	}
	return true;
}

//build the alpha-tested billboard shader for a monster sprite lump (sprites/<lump>).
static shader_t *Doom_MonsterSpriteShader(const char *lump, short *w, short *h, short *xo, short *yo)
{
	texid_t tex = Doom_LoadSprite(lump, w, h, xo, yo);
	texnums_t stn;
	shader_t *sh;
	char sname[40];
	if (!TEXVALID(tex)) return NULL;
	Q_snprintfz(sname, sizeof(sname), "doom_sprite_%s", lump);
	memset(&stn, 0, sizeof(stn));
	stn.base = tex;
	sh = R_RegisterShader(sname, SUF_NONE,
		"{\ncull none\n{\nmap $diffuse\nalphafunc ge128\ndepthwrite\n}\n}\n");
	R_BuildDefaultTexnums(&stn, sh, IF_NOMIPMAP);
	return sh;
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

//spawn monsters from the map's things. (For now each uses its front frame "A1" for all view
//angles; the 8-rotation/animation set is a later refinement.) shader[1] holds the death corpse.
static void Doom_LoadMonsters(doommap_t *dm)
{
	unsigned int i;
	dm->nummonsters = 0;
	for (i = 0; i < dm->numthings; i++)
	{
		doommonsterinfo_t mi;
		struct doommonster_s *m;
		vec3_t p; msector_t *sec;
		char lump[16]; short w,h,xo,yo; shader_t *sh;
		if (!Doom_MonsterInfo(dm->thing[i].type, &mi))
			continue;
		if (dm->thing[i].flags & THING_DEATHMATCH)
			continue;
		Q_snprintfz(lump, sizeof(lump), "%sA1", mi.spr);	//front frame
		sh = Doom_MonsterSpriteShader(lump, &w, &h, &xo, &yo);
		if (!sh)
			continue;
		dm->monsters = BZ_Realloc(dm->monsters, sizeof(*dm->monsters)*(dm->nummonsters+1));
		m = &dm->monsters[dm->nummonsters++];
		memset(m, 0, sizeof(*m));
		p[0]=dm->thing[i].xpos; p[1]=dm->thing[i].ypos; p[2]=0;
		sec = Doom_SectorNearPoint(dm, p);
		m->origin[0]=p[0]; m->origin[1]=p[1]; m->origin[2]=sec?sec->floorheight:0;
		m->yaw = dm->thing[i].angle;
		m->health=mi.health; m->radius=(qbyte)mi.radius; m->speed=mi.speed; m->type=dm->thing[i].type;
		m->atk=mi.atk; m->meleedmg=mi.meleedmg; m->meleerand=mi.meleerand?mi.meleerand:8; m->misdmg=mi.misdmg; m->misspeed=mi.misspeed; m->bullets=mi.bullets;
		m->atkcool = 0.5f + (rand()&255)/128.0f;	//stagger first attacks (0.5-2.5s) so monsters don't all volley on frame 1
		m->deathtime = -1;	//alive
		VectorCopy(m->origin, m->spawnorigin); m->spawnyaw = m->yaw; m->spawnhealth = m->health;	//for map reset on respawn
		m->shader[0]=sh; m->w[0]=w; m->h[0]=h; m->xo[0]=xo; m->yo[0]=yo;
		if (!(mi.atk & MATK_BARREL))
		{	//load the other 3 front walk frames (B1,C1,D1) for the walk cycle in shader[1..3]
			int f; const char wf[3] = {'B','C','D'};
			for (f = 0; f < 3; f++)
			{
				char wl[16]; short ww,wh,wxo,wyo; shader_t *wsh;
				Q_snprintfz(wl, sizeof(wl), "%s%c1", mi.spr, wf[f]);
				wsh = Doom_MonsterSpriteShader(wl, &ww,&wh,&wxo,&wyo);
				if (wsh) { m->shader[f+1]=wsh; m->w[f+1]=ww; m->h[f+1]=wh; m->xo[f+1]=wxo; m->yo[f+1]=wyo; }
			}
		}
		{	//death animation: monsters use their own Death frames (rotation 0, last = corpse); the
			//exploding barrel uses the shared BEXP explosion sprites and then vanishes.
			const char *seq = (mi.atk & MATK_BARREL) ? "ABCDE" : Doom_DeathSeq(mi.spr);
			const char *dspr = (mi.atk & MATK_BARREL) ? "BEXP" : mi.spr;
			m->ndeath = 0;
			while (seq && *seq && m->ndeath < 12)
			{
				char clump[16]; short cw,ch,cxo,cyo; shader_t *csh;
				Q_snprintfz(clump, sizeof(clump), "%s%c0", dspr, *seq);
				csh = Doom_MonsterSpriteShader(clump, &cw,&ch,&cxo,&cyo);
				if (csh)
				{
					m->deathfr[m->ndeath]=csh; m->dfw[m->ndeath]=cw; m->dfh[m->ndeath]=ch; m->dfxo[m->ndeath]=cxo;
					m->ndeath++;
				}
				seq++;
			}
		}
	}
	Con_DPrintf("Doom: spawned %u monsters\n", dm->nummonsters);
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

//spawn a monster projectile flying from a monster toward the player.
static void Doom_SpawnProjectile(doommap_t *dm, const vec3_t org, const vec3_t playerorg, int damage, int speed, qboolean homing)
{
	struct doomproj_s *pr; vec3_t to; float len; short w,h,xo,yo; shader_t *sh;
	to[0]=playerorg[0]-org[0]; to[1]=playerorg[1]-org[1]; to[2]=(playerorg[2]+24)-(org[2]+32);
	len = VectorLength(to);
	if (len < 1)
		return;
	sh = Doom_MonsterSpriteShader("BAL1A0", &w, &h, &xo, &yo);	//imp-fireball billboard for all (per-type sprite later)
	if (!sh)
		return;
	dm->projectiles = BZ_Realloc(dm->projectiles, sizeof(*dm->projectiles)*(dm->numprojectiles+1));
	pr = &dm->projectiles[dm->numprojectiles++];
	memset(pr, 0, sizeof(*pr));
	pr->origin[0]=org[0]; pr->origin[1]=org[1]; pr->origin[2]=org[2]+32;	//chest height
	VectorScale(to, speed/len, pr->vel);
	pr->damage = damage; pr->life = 6; pr->homing = homing?1:0;
	pr->shader=sh; pr->w=w; pr->h=h; pr->xo=xo; pr->yo=yo;
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
		dx=playerorg[0]-np[0]; dy=playerorg[1]-np[1]; dz=(playerorg[2]+24)-np[2];
		if (dx*dx+dy*dy < 24*24 && fabs(dz) < 40)
		{
			Doom_HurtPlayer(playerhealth, playerarmor, pr->damage * Doom_Rand(1,8));	//(rand%8+1)*info->damage
			dead = true;
		}
		sec = Doom_SectorNearPoint(dm, np);
		if (!sec || np[2] < sec->floorheight || np[2] > sec->ceilingheight)
			dead = true;
		if (dead || pr->life <= 0)
			dm->projectiles[i] = dm->projectiles[--dm->numprojectiles];	//swap-remove
		else
		{ VectorCopy(np, pr->origin); i++; }
	}
}

//exploding barrel blast (Doom barrel uses P_RadiusAttack, damage/radius 128): hurt the player and
//every nearby thing, with damage falling off by distance. Other barrels caught in the blast die
//here and detonate on the next tick, giving the classic chain reaction.
static void Doom_BarrelExplode(doommap_t *dm, struct doommonster_s *barrel, const vec3_t playerorg, float *ph, float *pa)
{
	unsigned int k;
	float dx, dy, d;
	barrel->exploded = 1;
	dx = playerorg[0]-barrel->origin[0]; dy = playerorg[1]-barrel->origin[1];
	d = sqrt(dx*dx+dy*dy);
	if (d < 128)
		Doom_HurtPlayer(ph, pa, (int)(128 - d));
	for (k = 0; k < dm->nummonsters; k++)
	{
		struct doommonster_s *o = &dm->monsters[k];
		if (o == barrel || o->mstate == 2)
			continue;
		dx = o->origin[0]-barrel->origin[0]; dy = o->origin[1]-barrel->origin[1];
		d = sqrt(dx*dx+dy*dy);
		if (d < 128)
		{
			o->health -= (int)(128 - d);
			o->alerted = 1;
			if (o->health <= 0) { o->mstate = 2; o->deathtime = 0; }
		}
	}
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

static qboolean Doom_MonsterBlocked(doommap_t *dm, struct doommonster_s *m, float nx, float ny)
{
	float r = m->radius;
	unsigned int j;
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
			if (opentop - openbot < 56 || openbot - m->origin[2] > 24)
				return true;	//opening too short to fit through, or the step up is too tall
		}
	}
	//don't pile onto another live monster. Block a step that lands within the two bodies' combined
	//radius AND moves us closer than we already are. Steps that keep or grow the gap are always
	//allowed, so a pair that rushed the same spot and ended up overlapping pushes apart instead of
	//deadlocking - and crucially never merges into a single blob (the old "skip if overlapping"
	//guard switched collision off once they touched, letting a crowd stack on one point).
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

//Item pickups: collect any pickup billboard the player walks over (touch radius ~ the two radii
//summed) and apply it to the inventory. Called from sv_user.c each frame with the player's edict
//fields. Ammo is capped at the Doom maxima; weapons grant ownership (a DWEP bit in *items) plus a
//little ammo. Keys/powerups are collected but have no effect yet. Sprite removed by swap-with-last.
void Doom_TryPickups(model_t *model, const vec3_t playerorg, float *health, float *armor,
	float *bullets, float *shells, float *rockets, float *cells, float *items)
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
			case 2001: wb |= DWEP_SHOTGUN;  if (shells)  *shells  = min(50,*shells+8);   break;	//shotgun
			case 82:   wb |= DWEP_SSG;      if (shells)  *shells  = min(50,*shells+8);   break;	//super shotgun
			case 2002: wb |= DWEP_CHAINGUN; if (bullets) *bullets = min(200,*bullets+20);break;	//chaingun
			case 2005: wb |= DWEP_CHAINSAW; break;	//chainsaw
			case 2003: wb |= DWEP_ROCKET;   if (rockets) *rockets = min(50,*rockets+2);  break;	//rocket launcher
			case 2004: wb |= DWEP_PLASMA;   if (cells)   *cells   = min(300,*cells+40);  break;	//plasma
			case 2006: wb |= DWEP_BFG;      if (cells)   *cells   = min(300,*cells+40);  break;	//BFG
			default: break;	//keys/powerups: collected, no effect yet
			}
			if (items) *items = (float)wb;
			dm->sprites[s] = dm->sprites[--dm->numsprites];
			continue;
		}
		s++;
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
	unsigned int i;
	if (!dm)
		return;

	//(item pickups are handled in Doom_TryPickups, called from sv_user.c where the full player
	//inventory - ammo and owned weapons, not just health/armour - is available.)

	Doom_TickProjectiles(dm, frametime, playerorg, playerhealth, playerarmor);
	for (i = 0; i < dm->nummonsters; i++)
	{
		struct doommonster_s *m = &dm->monsters[i];
		float dx, dy, dist, step, meleerange; vec3_t np, eye, peye; msector_t *sec; qboolean sight;
		if (m->mstate == 2)
		{	//dead: detonate a freshly-killed barrel, then advance the death animation timer (no AI)
			if ((m->atk & MATK_BARREL) && !m->exploded)
				Doom_BarrelExplode(dm, m, playerorg, playerhealth, playerarmor);
			if (m->deathtime >= 0) m->deathtime += frametime;
			continue;
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
		meleerange = m->radius + 36;	//+player radius ~16 + slack

		if (m->atk && m->atkcool <= 0 && sight)
		{
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
			{	//archvile hellfire: 20 direct + up to 70 blast (no projectile, can't be dodged)
				Doom_HurtPlayer(playerhealth, playerarmor, 20 + Doom_Rand(0, 70));
				m->atkcool = 2.0f; continue;
			}
			if (m->atk & MATK_MISSILE)
			{	//imp/caco/baron ball, revenant homing tracer, rockets, ...
				Doom_SpawnProjectile(dm, m->origin, playerorg, m->misdmg, m->misspeed, !!(m->atk & MATK_HOMING));
				m->atkcool = 1.5f; continue;
			}
		}

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
					&& !Doom_MonsterBlocked(dm, m, np[0], np[1]))
				{
					m->origin[0]=np[0]; m->origin[1]=np[1]; m->origin[2]=sec->floorheight;
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
	unsigned int i, best=~0u; float bestdist, fwdx, fwdy; vec3_t a;
	if (!dm)
		return;
	bestdist = (maxrange > 0) ? maxrange : 2000;
	fwdx = cos(yaw*M_PI/180.0); fwdy = sin(yaw*M_PI/180.0);
	VectorSet(a, org[0], org[1], org[2]+16);	//player body ~feet+40 (origin is feet+24)
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
		int p;
		m->alerted = 1;	//being shot wakes it
		for (p = 0; p < pellets; p++)
			m->health -= Doom_Rand(1,3) * dmgbase;
		if (m->health <= 0)
		{	//killed: start the death animation
			m->mstate = 2;
			m->deathtime = 0;
		}
	}
	Doom_NoiseAlert(dm, org);	//the gunshot wakes monsters within sound range
}

//draw monsters as upright camera-facing billboards (same technique as R_DoomDrawSprites).
static void R_DoomDrawMonsters(doommap_t *dm)
{
	unsigned int i;
	vec3_t viewang, vpn, vright, vup;
	mesh_t mesh;
	vecV_t xyz[4];
	vec2_t st[4] = {{0,0},{1,0},{1,1},{0,1}};
	byte_vec4_t col[4];
	index_t idx[6] = {0,1,2, 0,2,3};

	if (!dm->nummonsters && !dm->numprojectiles)
		return;
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
		float zb, zt; vec3_t l, r;
		shader_t *fsh; short fw, fh, fxo;
		if (m->mstate == 2)
		{	//dead: play the death-frame sequence by deathtime, then hold the last frame (corpse)
			int df;
			if (!m->ndeath)
				continue;	//no death frames (shouldn't happen) -> draw nothing
			df = (int)(m->deathtime / 0.15f);	//~6.7 frames/sec, matching Doom's ~5-tic death frames
			if (df >= m->ndeath)
			{
				if (m->atk & MATK_BARREL)
					continue;	//barrel: gone once its explosion animation finishes
				df = m->ndeath-1;	//monster corpse: hold the final frame
			}
			if (df < 0) df = 0;
			fsh=m->deathfr[df]; fw=m->dfw[df]; fh=m->dfh[df]; fxo=m->dfxo[df];
		}
		else
		{	//alive: cycle the front walk frames A-D while chasing; stand on frame A while dormant
			int wf = (m->alerted) ? (((int)(m->animt / 0.25f)) & 3) : 0;	//~0.25s/frame, like Doom's 8-tic walk frames
			if (!m->shader[wf]) wf = 0;
			if (!m->shader[wf])
				continue;
			fsh=m->shader[wf]; fw=m->w[wf]; fh=m->h[wf]; fxo=m->xo[wf];
		}
		zb = m->origin[2];
		zt = zb + fh;
		VectorMA(m->origin, -fxo,      vright, l);
		VectorMA(m->origin,  fw - fxo, vright, r);
		VectorSet(xyz[0], l[0], l[1], zt);
		VectorSet(xyz[1], r[0], r[1], zt);
		VectorSet(xyz[2], r[0], r[1], zb);
		VectorSet(xyz[3], l[0], l[1], zb);
		BE_DrawMesh_Single(fsh, &mesh, NULL, 0);
	}
	for (i = 0; i < dm->numprojectiles; i++)
	{	//flying projectiles: centred billboards (not floor-rested)
		struct doomproj_s *pr = &dm->projectiles[i];
		float zb, zt; vec3_t l, r;
		if (!pr->shader)
			continue;
		zb = pr->origin[2] - pr->h*0.5f;
		zt = pr->origin[2] + pr->h*0.5f;
		VectorMA(pr->origin, -pr->w*0.5f, vright, l);
		VectorMA(pr->origin,  pr->w*0.5f, vright, r);
		VectorSet(xyz[0], l[0], l[1], zt);
		VectorSet(xyz[1], r[0], r[1], zt);
		VectorSet(xyz[2], r[0], r[1], zb);
		VectorSet(xyz[3], l[0], l[1], zb);
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
	static char newlump[1024*1024];	//FIXME
	char thingname[MAX_QPATH];

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
