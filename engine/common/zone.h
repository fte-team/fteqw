/*
Copyright (C) 1996-1997 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/
/*
 memory allocation


H_??? The hunk manages the entire memory block given to quake.  It must be
contiguous.  Memory can be allocated from either the low or high end in a
stack fashion.  The only way memory is released is by resetting one of the
pointers.

Hunk allocations should be given a name, so the Hunk_Print () function
can display usage.

Hunk allocations are guaranteed to be 16 byte aligned.

The video buffers are allocated high to avoid leaving a hole underneath
server allocations when changing to a higher video mode.


Z_??? Zone memory functions used for small, dynamic allocations like text
strings from command input.  There is only about 48K for it, allocated at
the very bottom of the hunk.

Cache_??? Cache memory is for objects that can be dynamically loaded and
can usefully stay persistant between levels.  The size of the cache
fluctuates from level to level.

To allocate a cachable object


Temp_??? Temp memory is used for file loading and surface caching.  The size
of the cache memory is adjusted so that there is a minimum of 512k remaining
for temp memory.


------ Top of Memory -------

high hunk allocations

<--- high hunk reset point held by vid

video buffer

z buffer

surface cache

<--- high hunk used

cachable memory

<--- low hunk used

client and server low hunk allocations

<-- low hunk reset point held by host

startup hunk allocations

Zone block

----- Bottom of Memory -----



*/

void Memory_Init (void *buf, int size);
void Memory_DeInit(void);

void VARGS Z_Free (void *ptr);
void *Z_Malloc (int size); // returns 0 filled memory
void *ZF_Malloc (int size); // allowed to fail
//#define Z_Malloc(x) Z_MallocNamed2(x, __FILE__, __LINE__ )
void *VARGS Z_TagMalloc (int size, int tag);
void VARGS Z_TagFree(void *ptr);
void VARGS Z_FreeTags(int tag);
//void *Z_Realloc (void *ptr, int size);

//Big Zone: allowed to fail, doesn't clear. The expectation is a large file, rather than sensative data structures.
//(this is a nicer name for malloc)
void *BZ_Malloc(int size);
void *BZF_Malloc(int size);
void *BZ_Realloc(void *ptr, int size);
void *BZF_Realloc(void *data, int newsize);
void BZ_Free(void *ptr);

#ifdef NAMEDMALLOCS
#define BZ_Malloc(size) Z_MallocNamed(size, __FILE__, __LINE__)


#define Z_Malloc(size) Z_MallocNamed(size, __FILE__, __LINE__)

#define BZ_Realloc(ptr, size) BZ_NamedRealloc(ptr, size, __FILE__, __LINE__)
#endif

void *Hunk_Alloc (int size);		// returns 0 filled memory
void *Hunk_AllocName (int size, char *name);

void *Hunk_HighAllocName (int size, char *name);

int	Hunk_LowMark (void);
void Hunk_FreeToLowMark (int mark);
int Hunk_LowMemAvailable(void);

int	Hunk_HighMark (void);
void Hunk_FreeToHighMark (int mark);

void *Hunk_TempAlloc (int size);
void *Hunk_TempAllocMore (int size); //Don't clear old temp

void Hunk_Check (void);

typedef struct cache_user_s
{
	void	*data;
	qboolean fake;
} cache_user_t;

void Cache_Flush (void);

void *Cache_Check (cache_user_t *c);
// returns the cached data, and moves to the head of the LRU list
// if present, otherwise returns NULL

void Cache_Free (cache_user_t *c);

void *Cache_Alloc (cache_user_t *c, int size, char *name);
// Returns NULL if all purgable data was tossed and there still
// wasn't enough room.

void Cache_Report (void);

// Constant Block memory functions
// - Constant blocks are used for loads of strings/etc that
// are allocated once and change very little during the rest
// of run time, such as cvar names and default values
typedef struct const_block_s {
	int curleft; // current bytes left in block
	int cursize; // current maximum size of block
	int memstep; // bytes to step per realloc
	char *point; // current block point
	char *block; // memory block
} const_block_t;

const_block_t *CB_Malloc (int size, int step);
//char *CB_Slice (const_block_t *cb, int size);
char *CB_Copy (const_block_t *cb, char *data, int size);
void CB_Free (const_block_t *cb);

