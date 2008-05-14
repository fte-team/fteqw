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
// Z_zone.c

#include "quakedef.h"
#ifdef _WIN32
#include "winquake.h"
#endif

#undef malloc
#undef free

#define NOZONE
#define NOCACHE
#ifdef _WIN32
#define NOHIGH
#endif

void Cache_FreeLow (int new_low_hunk);
void Cache_FreeHigh (int new_high_hunk);

#ifdef _DEBUG
//#define MEMDEBUG	8192 //Debugging adds sentinels (the number is the size - I have the ram) 
#endif

//must be multiple of 4.
#define TEMPDEBUG 4
#define ZONEDEBUG 4
#define HUNKDEBUG 4
#define CACHEDEBUG 4

//these need to be defined because it makes some bits of code simpler
#ifndef HUNKDEBUG
#define HUNKDEBUG 0
#endif
#ifndef ZONEDEBUG
#define ZONEDEBUG 0
#endif
#ifndef TEMPDEBUG
#define TEMPDEBUG 0
#endif
#ifndef CACHEDEBUG
#define CACHEDEBUG 0
#endif

#if ZONEDEBUG>0 || HUNKDEBUG>0 || TEMPDEBUG>0||CACHEDEBUG>0
qbyte sentinalkey;
#endif

#define TAGLESS 1

typedef struct memheader_s {
	int size;
	int tag;
} memheader_t;

typedef struct zone_s {
	struct zone_s *next;
	struct zone_s *pvdn; // down if first, previous if not
	memheader_t mh;
} zone_t;
zone_t *zone_head;
#ifdef MULTITHREAD
void *zonelock;
#endif

void *VARGS Z_TagMalloc(int size, int tag)
{
	zone_t *zone;

	zone = (zone_t *)malloc(size + sizeof(zone_t));
	if (!zone)
		Sys_Error("Z_Malloc: Failed on allocation of %i bytes", size);
	Q_memset(zone, 0, size + sizeof(zone_t));
	zone->mh.tag = tag;
	zone->mh.size = size;

#ifdef MULTITHREAD
	if (zonelock)
		Sys_LockMutex(zonelock);
#endif
	if (zone_head == NULL)
		zone_head = zone;
	else if (zone_head->mh.tag == tag)
	{
		zone->next = zone_head->next;
		zone_head->next = zone;
	}
	else
	{
		zone_t *s = zone_head->pvdn;

		while (s && s->mh.tag != tag)
			s = s->pvdn;

		if (s)
		{ // tag match
			zone->next = s->next;
			s->next = zone;
		}
		else
		{
			zone->pvdn = zone_head;
			zone_head = zone;
		}
	}
#ifdef MULTITHREAD
	if (zonelock)
		Sys_UnlockMutex(zonelock);
#endif

	return (void *)(zone + 1);
}

void *ZF_Malloc(int size)
{
	return calloc(size, 1);
}

void *Z_Malloc(int size)
{
	void *mem = ZF_Malloc(size);
	if (!mem)
		Sys_Error("Z_Malloc: Failed on allocation of %i bytes", size);

	return mem;
}

void VARGS Z_TagFree(void *mem)
{
	zone_t *zone = ((zone_t *)mem) - 1;

#ifdef MULTITHREAD
	if (zonelock)
		Sys_LockMutex(zonelock);
#endif
	if (zone->next)
		zone->next->pvdn = zone->pvdn;
	if (zone->pvdn && zone->pvdn->mh.tag == zone->mh.tag)
		zone->pvdn->next = zone->next;
	else
	{ // zone is first entry in a tag list 
		zone_t *s = zone_head;

		if (zone != s)
		{ // traverse and update down list
			while (s->pvdn != zone) 
				s = s->next;

			s->pvdn = zone->pvdn;
		}
	}

	if (zone == zone_head)
	{ // freeing head node so update head pointer
		if (zone->next) // move to next, pvdn should be maintained properly
			zone_head = zone->next;
		else // no more entries with this tag so move head down
			zone_head = zone->pvdn;
	}
#ifdef MULTITHREAD
	if (zonelock)
		Sys_UnlockMutex(zonelock);
#endif

	free(zone);
}

void VARGS Z_Free(void *mem)
{
	free(mem);
}

void VARGS Z_FreeTags(int tag)
{
	zone_t *taglist;
	zone_t *t;

#ifdef MULTITHREAD
	if (zonelock)
		Sys_LockMutex(zonelock);
#endif
	if (zone_head)
	{
		if (zone_head->mh.tag == tag)
		{ // just pull off the head
			taglist = zone_head;
			zone_head = zone_head->pvdn;
		}
		else
		{ // search for tag list and isolate it
			zone_t *z;
			z = zone_head;
			while (z->next != NULL && z->next->mh.tag != tag)
				z = z->next;

			if (z->next == NULL)
				taglist = NULL;
			else
			{
				taglist = z->next;
				z->next = z->next->next;
			}
		}
	}
	else
		taglist = NULL;
#ifdef MULTITHREAD
	if (zonelock)
		Sys_UnlockMutex(zonelock);
#endif

	// actually free list
	while (taglist != NULL)
	{
		t = taglist->next;
		free(taglist);
		taglist = t;
	}
}

/*
void *Z_Realloc(void *data, int newsize)
{
	memheader_t *memref;

	if (!data)
		return Z_Malloc(newsize);

	memref = ((memheader_t *)data) - 1;

	if (memref[0].tag != TAGLESS)
	{ // allocate a new block and copy since we need to maintain the lists
		zone_t *zone = ((zone_t *)data) - 1;
		int size = zone->mh.size;
		if (size != newsize)
		{
			void *newdata = Z_Malloc(newsize);

			if (size > newsize)
				size = newsize;
			memcpy(newdata, data, size);

			Z_Free(data);
			data = newdata;
		}
	}
	else
	{
		int oldsize = memref[0].size;
		memref = realloc(memref, newsize + sizeof(memheader_t));
		memref->size = newsize;
		if (newsize > oldsize)
			memset((qbyte *)memref + sizeof(memheader_t) + oldsize, 0, newsize - oldsize);
		data = ((memheader_t *)memref) + 1;
	}

	return data;
}
*/

void *BZF_Malloc(int size)	//BZ_Malloc but allowed to fail - like straight malloc.
{
	return malloc(size); 
}

void *BZ_Malloc(int size)	//Doesn't clear. The expectation is a large file, rather than sensative data structures.
{
	void *mem = BZF_Malloc(size);
	if (!mem)
		Sys_Error("BZ_Malloc: Failed on allocation of %i bytes", size);

	return mem;
}

void *BZF_Realloc(void *data, int newsize)
{
	return realloc(data, newsize);
}

void *BZ_Realloc(void *data, int newsize)
{
	void *mem = BZF_Realloc(data, newsize);

	if (!mem)
		Sys_Error("BZ_Realloc: Failed on reallocation of %i bytes", newsize);

	return mem;
}

void BZ_Free(void *data)
{
	free(data);
}


#if 0 //NOZONE	//zone memory is for small dynamic things.
/*
void *Z_TagMalloc(int size, int tag)
{
	return malloc(size);
}

void *Z_Malloc(int size)
{
	qbyte *buf;
	buf = Z_TagMalloc(size, 1);
	if (!buf)
		Sys_Error("Z_Malloc: Failed on allocation of %i bytes", size);
	Q_memset(buf, 0, size);
	return buf;
}

void Z_Free (void *buf)
{
	free(buf);
}

void Z_FreeTags (void *buf)
{
	free(buf);
}

*/


















#define	ZONEID	0x1d4a11

#define ZONESENTINAL 0xdeadbeaf

typedef struct zone_s {
//	int sentinal1;

	struct zone_s *next;
	struct zone_s *prev;
	int size;
	int tag;

//	int sentinal2;
} zone_t;
zone_t *zone_head;
/*
void Z_CheckSentinals(void)
{
	zone_t *zone;
	for(zone = zone_head; zone; zone=zone->next)
	{
		if (zone->sentinal1 != ZONESENTINAL || zone->sentinal2 != ZONESENTINAL)
			Sys_Error("Memory sentinal destroyed\n");
	}
}*/


void VARGS Z_Free (void *c)
{
	zone_t *nz;
	nz = ((zone_t *)((char*)c-ZONEDEBUG))-1;

//	Z_CheckSentinals();

#if ZONEDEBUG>0
	{
		int i;
		qbyte *buf;
		buf = (qbyte *)(nz+1);
		for (i = 0; i < ZONEDEBUG; i++)
		{
			if (buf[i] != sentinalkey)
				Sys_Error("corrupt memory block (%i? bytes)\n", nz->size);
		}
		buf+=ZONEDEBUG;
		//app data
		buf += nz->size;
		for (i = 0; i < ZONEDEBUG; i++)
		{
			if (buf[i] != sentinalkey)
				Sys_Error("corrupt memory block (%i? bytes)\n", nz->size);
		}
	}
#endif

//	if (nz->sentinal1 != ZONESENTINAL || nz->sentinal2 != ZONESENTINAL)
//		Sys_Error("zone was not z_malloced\n");

	if (nz->next)
		nz->next->prev = nz->prev;
	if (nz->prev)
		nz->prev->next = nz->next;

	if (nz == zone_head)
		zone_head = nz->next;

//	Con_Printf("Free of %i bytes\n", nz->size);

	free(nz);
}

void BZ_CheckSentinals(void *c)
{
#if ZONEDEBUG>0
	zone_t *nz;
	nz = ((zone_t *)((char*)c-ZONEDEBUG))-1;

//	Z_CheckSentinals();
	{
		int i;
		qbyte *buf;
		buf = (qbyte *)(nz+1);
		for (i = 0; i < ZONEDEBUG; i++)
		{
			if (buf[i] != sentinalkey)
				Sys_Error("corrupt memory block (%i? bytes)\n", nz->size);
		}
		buf+=ZONEDEBUG;
		//app data
		buf += nz->size;
		for (i = 0; i < ZONEDEBUG; i++)
		{
			if (buf[i] != sentinalkey)
				Sys_Error("corrupt memory block (%i? bytes)\n", nz->size);
		}
	}
#endif

}
	//revive this function each time you get memory corruption and need to trace it.
void BZ_CheckAllSentinals(void)
{
	zone_t *zone;
	for(zone = zone_head; zone; zone=zone->next)
	{
		int i;
		qbyte *buf;
		buf = (qbyte *)(zone+1);
		for (i = 0; i < ZONEDEBUG; i++)
		{
			if (buf[i] != sentinalkey)
				Sys_Error("corrupt memory block (%i? bytes)\n", zone->size);
		}
		buf+=ZONEDEBUG;
		//app data
		buf += zone->size;
		for (i = 0; i < ZONEDEBUG; i++)
		{
			if (buf[i] != sentinalkey)
				Sys_Error("corrupt memory block (%i? bytes)\n", zone->size);
		}
	}
}


void VARGS Z_FreeTags(int tag)
{
	zone_t *zone, *next;
	for(zone = zone_head; zone; zone=next)
	{
		next = zone->next;
		if (zone->tag == tag)
			Z_Free((char*)(zone+1)+ZONEDEBUG);
	}
}

#ifdef NAMEDMALLOCS
void *Z_BaseTagMalloc (int size, int tag, qboolean clear, char *descrip, ...)
#else
void *Z_BaseTagMalloc (int size, int tag, qboolean clear)
#endif
{
#ifdef NAMEDMALLOCS
	va_list		argptr;
	char buffer[512];
#endif
	void	*buf;
	zone_t *nt;

//	Z_CheckSentinals();
//Con_Printf("Malloc of %i bytes\n", size);
//if (size>20)
//Con_Printf("Big malloc\n");
	if (size <= 0)
		Sys_Error ("Z_Malloc: size %i", size);

#ifdef NAMEDMALLOCS

	va_start (argptr, descrip);
	vsprintf (buffer, descrip,argptr);
	va_end (argptr);

	nt = (zone_t*)malloc(size + sizeof(zone_t)+strlen(buffer)+1 + ZONEDEBUG*2);
#else
	nt = (zone_t*)malloc(size + sizeof(zone_t)+ ZONEDEBUG*2);
#endif
	if (!nt)
		Sys_Error("Z_BaseTagMalloc: failed on allocation of %i bytes", size);
	nt->next = zone_head;
	nt->prev = NULL;
	nt->size = size;
	nt->tag = tag;
//	nt->sentinal1 = ZONESENTINAL;
//	nt->sentinal2 = ZONESENTINAL;
	if (zone_head)
		zone_head->prev = nt;
	zone_head = nt;
	buf = (void *)(nt+1);

#if ZONEDEBUG > 0
	memset(buf, sentinalkey, ZONEDEBUG);
	buf = (char*)buf+ZONEDEBUG;
	memset((char*)buf+size, sentinalkey, ZONEDEBUG);
#endif

	if (clear)
		Q_memset(buf, 0, size);

#ifdef NAMEDMALLOCS
	strcpy((char *)(nt+1) + nt->size + ZONEDEBUG*2, buffer);
#endif
	return buf;
}
void *VARGS Z_TagMalloc (int size, int tag)
{
#ifdef NAMEDMALLOCS
	return Z_BaseTagMalloc(size, tag, true, "");
#else
	return Z_BaseTagMalloc(size, tag, true);
#endif
}

#ifdef NAMEDMALLOCS
void *Z_MallocNamed (int size, char *file, int lineno)
{
	qbyte *buf;
	buf = Z_BaseTagMalloc(size, 1, true, "%s: %i", file, lineno);
	if (!buf)
		Sys_Error("Z_Malloc: Failed on allocation of %i bytes", size);
	return buf;
}
#else
void *Z_Malloc(int size)
{
	qbyte *buf;
	buf = (qbyte*)Z_TagMalloc(size, 1);
	if (!buf)
		Sys_Error("Z_Malloc: Failed on allocation of %i bytes", size);
	return buf;
}



void *BZ_Malloc(int size)	//Doesn't clear. The expectation is a large file, rather than sensative data structures.
{
	void *data = Z_BaseTagMalloc(size, 1, true);
	if (!data)
		Sys_Error("BZ_Malloc failed on %i bytes", size);

	return data;
}
#endif
void *BZF_Malloc(int size)	//BZ_Malloc but allowed to fail - like straight malloc.
{
#ifdef NAMEDMALLOCS
	return Z_BaseTagMalloc(size, 1, false, "");
#else
	return Z_BaseTagMalloc(size, 1, false);
#endif
}


#ifdef NAMEDMALLOCS
void *BZ_NamedRealloc(void *data, int newsize, char *file, int lineno)
#else
void *BZ_Realloc(void *data, int newsize)
#endif
{
	zone_t *oldzone;
	void *newdata;
#ifdef NAMEDMALLOCS
	if (!data)
		return Z_MallocNamed(newsize, file, lineno);
	oldzone = ((zone_t *)((char *)data-ZONEDEBUG))-1;
	if (oldzone->size == newsize)
		return data;
	newdata = Z_MallocNamed(newsize, file, lineno);
#else
	if (!data)
		return Z_Malloc(newsize);
	oldzone = ((zone_t *)((char *)data-ZONEDEBUG))-1;
	if (oldzone->size == newsize)
		return data;
	newdata = BZ_Malloc(newsize);
#endif
	if (oldzone->size < newsize)
	{
		memcpy(newdata, data, oldzone->size);
		memset((char *)newdata + oldzone->size, 0, newsize - oldzone->size);
	}
	else
		memcpy(newdata, data, newsize);
	BZ_Free(data);

	return newdata;
}

void BZ_Free(void *data)
{
	Z_Free(data);
}

#ifdef NAMEDMALLOCS

// Zone_Groups_f: prints out zones sorting into groups
// and tracking number of allocs and total group size as
// well as a group delta against the last Zone_Group_f call
#define ZONEGROUPS 64
void Zone_Groups_f(void)
{
	zone_t *zone;
	char *zonename[ZONEGROUPS];
	int zonesize[ZONEGROUPS];
	int zoneallocs[ZONEGROUPS];
	static int zonelast[ZONEGROUPS];
	int groups, i;
	int allocated = 0;

	// initialization
	for (groups = 0; groups < ZONEGROUPS; groups++)
		zonename[groups] = NULL;

	groups = 0;
	i = 0;

	for (zone = zone_head; zone; zone=zone->next)
	{
		char *czg = (char *)(zone+1) + zone->size+ZONEDEBUG*2;
		// check against existing tracked groups
		for (i = 0; i < groups; i++)
		{
			if (!strcmp(czg, zonename[i]))
			{
				// update stats for tracked group
				zonesize[i] += zone->size;
				zoneallocs[i]++;
				break;
			}
		}

		if (groups == i) // no existing group found
		{
			// track new zone group
			zonename[groups] = czg;
			zonesize[groups] = zone->size;
			zoneallocs[groups] = 1;
			groups++;

			// max groups bounds check
			if (groups >= ZONEGROUPS)
			{
				groups = ZONEGROUPS;
				break;
			}
		}
	}

	// print group statistics
	for (i = 0; i < groups; i++)
	{
		allocated += zonesize[i];
		Con_Printf("%s, size: %i, allocs: %i, delta: %i\n", zonename[i], zonesize[i], zoneallocs[i], zonesize[i] - zonelast[i]);
		zonelast[i] = zonesize[i]; // update delta tracking for next call
	}

	Con_Printf("Total: %i bytes\n", allocated);
}
#endif

void Zone_Print_f(void)
{
	int overhead=0;
	int allocated = 0;
	int blocks = 0;
	int futurehide = false;
	int minsize = 0;
	zone_t *zone;
#if ZONEDEBUG > 0
#ifdef NAMEDMALLOCS
	int i;
	qbyte *sent;
#endif
	qboolean testsent = false;
	if (*Cmd_Argv(1) == 't')
	{
		Con_Printf("Testing Zone sentinels\n");
		testsent = true;
	}
	else
#endif
	if (*Cmd_Argv(1) == 'h')
		futurehide = true;
	else if (*Cmd_Argv(1))
		minsize = atoi(Cmd_Argv(1));
	for(zone = zone_head; zone; zone=zone->next)
	{
		blocks++;
		allocated+= zone->size;

#ifdef NAMEDMALLOCS
		if (*((char *)(zone+1)+zone->size+ZONEDEBUG*2)!='#')
		{
#if ZONEDEBUG > 0
			if (testsent)
			{
				sent = (qbyte *)(zone+1);
				for (i = 0; i < ZONEDEBUG; i++)
				{
					if (sent[i] != sentinalkey)
					{
						Con_Printf(CON_ERROR "%i %i-%s\n", zone->size, i, (char *)(zone+1) + zone->size+ZONEDEBUG*2);
						break;
					}
				}
				sent += zone->size+ZONEDEBUG;
				for (i = 0; i < ZONEDEBUG; i++)
				{
					if (sent[i] != sentinalkey)
					{
						Con_Printf(CON_ERROR "%i %i-%s\n", zone->size, i, (char *)(zone+1) + zone->size+ZONEDEBUG*2);
						break;
					}
				}
			}
			else if (zone->size >= minsize)
#endif
				Con_Printf("%i-%s\n", zone->size, (char *)(zone+1) + zone->size+ZONEDEBUG*2);
			if (futurehide)
				*((char *)(zone+1)+zone->size+ZONEDEBUG*2) = '#';

//			Sleep(10);
		}
		overhead += sizeof(zone_t)+ZONEDEBUG*2 + strlen((char *)(zone+1) + zone->size+ZONEDEBUG*2) +1;
#else
		Con_Printf("%i-%i ", zone->size, zone->tag);
		overhead += sizeof(zone_t)+ZONEDEBUG*2;
#endif
	}
	Con_Printf(CON_NOTICE "Zone:%i bytes in %i blocks\n", allocated, blocks);
	Con_Printf(CON_NOTICE "Overhead %i bytes\n", overhead);
}

#elif 0//#else







//dmw was 0x50000 19/12/02 - playing with dynamic sound system.
//was 0x80000 15/01/03 - playing with genuine pk3 files
#define	DYNAMIC_SIZE	0x100000

#define	ZONEID	0x1d4a11
#define MINFRAGMENT	64

typedef struct memblock_s
{
	int		size;           // including the header and possibly tiny fragments
	int     tag;            // a tag of 0 is a free block
	int     id;        		// should be ZONEID
	struct memblock_s       *next, *prev;
	int		pad;			// pad to 64 bit boundary
} memblock_t;

typedef struct
{
	int		size;		// total bytes malloced, including header
	memblock_t	blocklist;		// start / end cap for linked list
	memblock_t	*rover;
} memzone_t;

/*
==============================================================================

						ZONE MEMORY ALLOCATION

There is never any space between memblocks, and there will never be two
contiguous free memblocks.

The rover can be left pointing at a non-empty block

The zone calls are pretty much only used for small strings and structures,
all big things are allocated on the hunk.
==============================================================================
*/

memzone_t	*mainzone;

void Z_ClearZone (memzone_t *zone, int size);


/*
========================
Z_ClearZone
========================
*/
void Z_ClearZone (memzone_t *zone, int size)
{
	memblock_t	*block;
	
// set the entire zone to one free block

	zone->blocklist.next = zone->blocklist.prev = block =
		(memblock_t *)( (qbyte *)zone + sizeof(memzone_t) );
	zone->blocklist.tag = 1;	// in use block
	zone->blocklist.id = 0;
	zone->blocklist.size = 0;
	zone->rover = block;
	
	block->prev = block->next = &zone->blocklist;
	block->tag = 0;			// free block
	block->id = ZONEID;
	block->size = size - sizeof(memzone_t);
}


/*
========================
Z_Free
========================
*/
void Z_Free (void *ptr)
{
	memblock_t	*block, *other;
	
	if (!ptr)
		Sys_Error ("Z_Free: NULL pointer");

	block = (memblock_t *) ( (qbyte *)ptr - sizeof(memblock_t));
	if (block->id != ZONEID)
		Sys_Error ("Z_Free: freed a pointer without ZONEID");
	if (block->tag == 0)
		Sys_Error ("Z_Free: freed a freed pointer");

	block->tag = 0;		// mark as free
	
	other = block->prev;
	if (!other->tag)
	{	// merge with previous free block
		other->size += block->size;
		other->next = block->next;
		other->next->prev = other;
		if (block == mainzone->rover)
			mainzone->rover = other;
		block = other;
	}
	
	other = block->next;
	if (!other->tag)
	{	// merge the next free block onto the end
		block->size += other->size;
		block->next = other->next;
		block->next->prev = block;
		if (other == mainzone->rover)
			mainzone->rover = block;
	}
}


/*
========================
Z_Malloc
========================
*/
#undef Z_Malloc
void *Z_Malloc (int size)
{
	void	*buf;
	
Z_CheckHeap ();	// DEBUG
	buf = Z_TagMalloc (size, 1);
	if (!buf)
		Sys_Error ("Z_Malloc: failed on allocation of %i bytes",size);
	Q_memset (buf, 0, size);

	return buf;
}

void *Z_MallocNamed (int size, char *name)
{
	void	*buf;
	
Z_CheckHeap ();	// DEBUG
	buf = Z_TagMalloc (size, 1);
	if (!buf)
		Sys_Error ("Z_Malloc: %s failed on allocation of %i bytes", name, size);
//	Sys_DebugLog("zmalloc.log", "%s allocates %i bytes\n", name, size);
	Q_memset (buf, 0, size);

	return buf;
}

void *Z_MallocNamed2 (int size, char *name, int line)
{
	void	*buf;
	
Z_CheckHeap ();	// DEBUG
	buf = Z_TagMalloc (size, 1);
	if (!buf)
		Sys_Error ("Z_Malloc: %s %i failed on allocation of %i bytes", name, line, size);
//	Sys_DebugLog("zmalloc.log", "%s %i allocates %i bytes\n", name, line, size);
	Q_memset (buf, 0, size);

	return buf;
}

void *Z_TagMalloc (int size, int tag)
{
	int		extra;
	memblock_t	*start, *rover, *newz, *base;

	if (!tag)
		Sys_Error ("Z_TagMalloc: tried to use a 0 tag");

//
// scan through the block list looking for the first free block
// of sufficient size
//
	size += sizeof(memblock_t);	// account for size of block header
	size += 4;					// space for memory trash tester
	size = (size + 7) & ~7;		// align to 8-qbyte boundary
	
	base = rover = mainzone->rover;
	start = base->prev;
	
	do
	{
		if (rover == start)	// scaned all the way around the list
			return NULL;
		if (rover->tag)
			base = rover = rover->next;
		else
			rover = rover->next;
	} while (base->tag || base->size < size);
	
//
// found a block big enough
//
	extra = base->size - size;
	if (extra >  MINFRAGMENT)
	{	// there will be a free fragment after the allocated block
		newz = (memblock_t *) ((qbyte *)base + size );
		newz->size = extra;
		newz->tag = 0;			// free block
		newz->prev = base;
		newz->id = ZONEID;
		newz->next = base->next;
		newz->next->prev = newz;
		base->next = newz;
		base->size = size;
	}
	
	base->tag = tag;				// no longer a free block
	
	mainzone->rover = base->next;	// next allocation will start looking here
	
	base->id = ZONEID;

// marker for memory trash testing
	*(int *)((qbyte *)base + base->size - 4) = ZONEID;

	return (void *) ((qbyte *)base + sizeof(memblock_t));
}


/*
========================
Z_Print
========================
*/
void Z_Print (memzone_t *zone)
{
	memblock_t	*block;
	
	Con_Printf ("zone size: %i  location: %p\n",mainzone->size,mainzone);
	
	for (block = zone->blocklist.next ; ; block = block->next)
	{
		Con_Printf ("block:%p    size:%7i    tag:%3i\n",
			block, block->size, block->tag);
		
		if (block->next == &zone->blocklist)
			break;			// all blocks have been hit	
		if ( (qbyte *)block + block->size != (qbyte *)block->next)
			Con_Printf ("ERROR: block size does not touch the next block\n");
		if ( block->next->prev != block)
			Con_Printf ("ERROR: next block doesn't have proper back link\n");
		if (!block->tag && !block->next->tag)
			Con_Printf ("ERROR: two consecutive free blocks\n");
	}
}






void *BZ_Malloc(int size)
{
	void *data;
	data = malloc(size);
	memset(data, 0, size);
	return data;
}

void BZ_Free(void *data)
{
	free(data);
}



#endif







//============================================================================

#define	HUNK_SENTINAL	0x1df001ed

typedef struct
{
	int		sentinal;
	int		size;		// including sizeof(hunk_t), -1 = not allocated
	char	name[8];
} hunk_t;

qbyte	*hunk_base;
int		hunk_size;

int		hunk_low_used;
int		hunk_high_used;

qboolean	hunk_tempactive;
int		hunk_tempmark;

void R_FreeTextures (void);

/*
==============
Hunk_Check

Run consistancy and sentinal trahing checks
==============
*/
void Hunk_Check (void)
{
	hunk_t	*h;
	
	for (h = (hunk_t *)hunk_base ; (qbyte *)h != hunk_base + hunk_low_used ; )
	{
		if (h->sentinal != HUNK_SENTINAL)
			Sys_Error ("Hunk_Check: trahsed sentinal");
		if (h->size < 16+HUNKDEBUG*2 || h->size + (qbyte *)h - hunk_base > hunk_size)
			Sys_Error ("Hunk_Check: bad size");
#if HUNKDEBUG > 0
		{
			qbyte *present;
			qbyte *postsent;
			int i;
			present = (qbyte *)(h+1);
			postsent = (qbyte *)h + h->size-HUNKDEBUG;
			for (i = 0; i < HUNKDEBUG; i++)
			{
				if (present[i] != sentinalkey)
					*(int*)0 = -3;
				if (postsent[i] != sentinalkey)
					*(int*)0 = -3;
			}
		}
#endif
		h = (hunk_t *)((qbyte *)h+h->size);
	}
}

/*
==============
Hunk_Print

If "all" is specified, every single allocation is printed.
Otherwise, allocations with the same name will be totaled up before printing.
==============
*/
void Hunk_Print (qboolean all)
{
	hunk_t	*h, *next, *endlow, *starthigh, *endhigh;
	int		count, sum;
	int		totalblocks;
	char	name[9];

	name[8] = 0;
	count = 0;
	sum = 0;
	totalblocks = 0;
	
	h = (hunk_t *)hunk_base;
	endlow = (hunk_t *)(hunk_base + hunk_low_used);
	starthigh = (hunk_t *)(hunk_base + hunk_size - hunk_high_used);
	endhigh = (hunk_t *)(hunk_base + hunk_size);

	Con_Printf ("          :%8i total hunk size\n", hunk_size);
	Con_Printf ("-------------------------\n");

	while (1)
	{
	//
	// skip to the high hunk if done with low hunk
	//
		if ( h == endlow )
		{
			Con_Printf ("-------------------------\n");
			Con_Printf ("          :%8i REMAINING\n", hunk_size - hunk_low_used - hunk_high_used);
			Con_Printf ("               :%8i USED\n", hunk_low_used + hunk_high_used);
			Con_Printf ("-------------------------\n");
			h = starthigh;
		}
		
	//
	// if totally done, break
	//
		if ( h == endhigh )
			break;

	//
	// run consistancy checks
	//
		if (h->sentinal != HUNK_SENTINAL)
			Sys_Error ("Hunk_Check: trahsed sentinal");
		if (h->size < 16 || h->size + (qbyte *)h - hunk_base > hunk_size)
			Sys_Error ("Hunk_Check: bad size");
#if HUNKDEBUG > 0
		{
			qbyte *present;
			qbyte *postsent;
			int i;
			present = (qbyte *)(h+1);
			postsent = (qbyte *)h + h->size-HUNKDEBUG;
			for (i = 0; i < HUNKDEBUG; i++)
			{
				if (present[i] != sentinalkey)
					*(int*)0 = -3;
				if (postsent[i] != sentinalkey)
					*(int*)0 = -3;
			}
		}
#endif
		next = (hunk_t *)((qbyte *)h+h->size);
		count++;
		totalblocks++;
		sum += h->size;

	//
	// print the single block
	//
		memcpy (name, h->name, 8);
		if (all)
			Con_Printf ("%8p :%8i %8s\n",h, h->size, name);
			
	//
	// print the total
	//
		if (next == endlow || next == endhigh || 
		strncmp (h->name, next->name, 8) )
		{
			if (!all)
				Con_Printf ("          :%8i %8s (TOTAL)\n",sum, name);
			count = 0;
			sum = 0;
		}

		h = next;
	}

	Con_Printf ("-------------------------\n");
	Con_Printf ("%8i total blocks\n", totalblocks);
	
}

/*
===================
Hunk_AllocName
===================
*/
void *Hunk_AllocName (int size, char *name)
{
#ifdef NOHIGH
	int roundup;
	int roundupold;
#endif
	hunk_t	*h;
	
#ifdef PARANOID
	Hunk_Check ();
#endif

	if (size < 0)
		Sys_Error ("Hunk_Alloc: bad size: %i", size);
		
	size = sizeof(hunk_t) + HUNKDEBUG*2 + ((size+15)&~15);
	
#ifndef _WIN32
	if (hunk_size - hunk_low_used - hunk_high_used < size)
//		Sys_Error ("Hunk_Alloc: failed on %i bytes",size);
#ifdef _WIN32
	  	Sys_Error ("Not enough RAM allocated on allocation of \"%s\".  Try starting using \"-heapsize 16000\" on the QuakeWorld command line.", name);
#else
	  	Sys_Error ("Not enough RAM allocated.  Try starting using \"-mem 16\" on the QuakeWorld command line.");
#endif
#endif

	h = (hunk_t *)(hunk_base + hunk_low_used);

#ifdef NOHIGH

	roundupold = hunk_low_used+sizeof(hunk_t);
	roundupold += 1024*128;
	roundupold &= ~(1024*128 - 1);

	roundup = hunk_low_used+size+sizeof(hunk_t);
	roundup += 1024*128;
	roundup &= ~(1024*128 - 1);


	if (!hunk_low_used || roundup != roundupold)
	if (!VirtualAlloc (hunk_base, roundup, MEM_COMMIT, PAGE_READWRITE))
	{
		char *buf;
		Hunk_Print(true);
		FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM, NULL, GetLastError(), MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPTSTR) &buf, 0, NULL);
		Sys_Error ("VirtualCommit failed\nNot enough RAM allocated on allocation of \"%s\".  Try starting using \"-heapsize 64000\" on the QuakeWorld command line.", name);
	}
#endif

	hunk_low_used += size;

	Cache_FreeLow (hunk_low_used);

	memset (h, 0, size-HUNKDEBUG);

#if HUNKDEBUG>0
	memset ((h+1), sentinalkey, HUNKDEBUG);
	memset ((qbyte *)h+size-HUNKDEBUG, sentinalkey, HUNKDEBUG);
#endif
	
	h->size = size;
	h->sentinal = HUNK_SENTINAL;
	Q_strncpyz (h->name, COM_SkipPath(name), sizeof(h->name));
	
	return (void *)((char *)(h+1)+HUNKDEBUG);
}

/*
===================
Hunk_Alloc
===================
*/
void *Hunk_Alloc (int size)
{
	return Hunk_AllocName (size, "unknown");
}

int	Hunk_LowMark (void)
{
	return hunk_low_used;
}

int Hunk_LowMemAvailable(void)
{
	return hunk_size - hunk_low_used - hunk_high_used;
}

void Hunk_FreeToLowMark (int mark)
{
	if (mark < 0 || mark > hunk_low_used)
		Sys_Error ("Hunk_FreeToLowMark: bad mark %i", mark);
	memset (hunk_base + mark, 0, hunk_low_used - mark);
	hunk_low_used = mark;

#ifdef NOHIGH
	if (!VirtualAlloc (hunk_base, hunk_low_used+sizeof(hunk_t), MEM_COMMIT, PAGE_READWRITE))
	{
		char *buf;
		FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM, NULL, GetLastError(), MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPTSTR) &buf, 0, NULL);
		Sys_Error ("VirtualAlloc commit failed.\n%s", buf);
	}
#endif
}

int	Hunk_HighMark (void)
{
	if (hunk_tempactive)
	{
		hunk_tempactive = false;
		Hunk_FreeToHighMark (hunk_tempmark);
	}

	return hunk_high_used;
}

void Hunk_FreeToHighMark (int mark)
{
	if (hunk_tempactive)
	{
		hunk_tempactive = false;
		Hunk_FreeToHighMark (hunk_tempmark);
	}
	if (mark < 0 || mark > hunk_high_used)
		Sys_Error ("Hunk_FreeToHighMark: bad mark %i", mark);
	memset (hunk_base + hunk_size - hunk_high_used, 0, hunk_high_used - mark);
	hunk_high_used = mark;
}


/*
===================
Hunk_HighAllocName
===================
*/
void *Hunk_HighAllocName (int size, char *name)
{
#ifdef NOHIGH
	Sys_Error("High hunk was disabled");
	return NULL;
#else

	hunk_t	*h;

	if (size < 0)
		Sys_Error ("Hunk_HighAllocName: bad size: %i", size);

	if (hunk_tempactive)
	{
		Hunk_FreeToHighMark (hunk_tempmark);
		hunk_tempactive = false;
	}

#ifdef PARANOID
	Hunk_Check ();
#endif

	size = sizeof(hunk_t) + ((size+15)&~15);

	if (hunk_size - hunk_low_used - hunk_high_used < size)
	{
		Con_Printf ("Hunk_HighAlloc: failed on %i bytes\n",size);
		return NULL;
	}

	hunk_high_used += size;
	Cache_FreeHigh (hunk_high_used);

	h = (hunk_t *)(hunk_base + hunk_size - hunk_high_used);

	memset (h, 0, size);
	h->size = size;
	h->sentinal = HUNK_SENTINAL;
	Q_strncpyz (h->name, name, sizeof(h->name));

	return (void *)(h+1);
#endif
}


/*
=================
Hunk_TempAlloc

Return space from the top of the hunk
clears old temp.
=================
*/
#ifdef NOHIGH
typedef struct hnktemps_s {
	struct hnktemps_s *next;
#if TEMPDEBUG>0
	int len;
#endif
} hnktemps_t;
hnktemps_t *hnktemps;

void Hunk_TempFree(void)
{
	hnktemps_t *nt;

	while (hnktemps)
	{
#if TEMPDEBUG>0
		int i;
		qbyte *buf;
		buf = (qbyte *)(hnktemps+1);
		for (i = 0; i < TEMPDEBUG; i++)
		{
			if (buf[i] != sentinalkey)
				*(int*)0 = -3;	//force a crash... this'll get our attention.
		}
		buf+=TEMPDEBUG;
		//app data
		buf += hnktemps->len;
		for (i = 0; i < TEMPDEBUG; i++)
		{
			if (buf[i] != sentinalkey)
				*(int*)0 = -3;	//force a crash... this'll get our attention.
		}
#endif

		nt = hnktemps->next;

		free(hnktemps);
		hnktemps = nt;
	}
}
#endif


//allocates without clearing previous temp.
//safer than my hack that fuh moaned about...
void *Hunk_TempAllocMore (int size)
{
	void	*buf;
#ifdef NOHIGH
#if TEMPDEBUG>0
	hnktemps_t *nt;
	nt = (hnktemps_t*)malloc(size + sizeof(hnktemps_t) + TEMPDEBUG*2);
	nt->next = hnktemps;
	nt->len = size;
	hnktemps = nt;
	buf = (void *)(nt+1);
	memset(buf, sentinalkey, TEMPDEBUG);
	buf = (char *)buf + TEMPDEBUG;
	memset(buf, 0, size);
	memset((char *)buf + size, sentinalkey, TEMPDEBUG);
	return buf;
#else
	hnktemps_t *nt;
	nt = (hnktemps_t*)malloc(size + sizeof(hnktemps_t));
	nt->next = hnktemps;
	hnktemps = nt;
	buf = (void *)(nt+1);
	memset(buf, 0, size);
	return buf;
#endif
#else
	
	if (!hunk_tempactive)
		return Hunk_TempAlloc(size);

	size = (size+15)&~15;

	hunk_tempactive = false;	//so it doesn't wipe old temp.
	buf = Hunk_HighAllocName (size, "mtmp");
	hunk_tempactive = true;

	return buf;
#endif
}


void *Hunk_TempAlloc (int size)
{
#ifdef NOHIGH

	Hunk_TempFree();

	return Hunk_TempAllocMore(size);
#else
	void	*buf;

	size = (size+15)&~15;
	
	if (hunk_tempactive)
	{
		Hunk_FreeToHighMark (hunk_tempmark);
		hunk_tempactive = false;
	}
	
	hunk_tempmark = Hunk_HighMark ();

	buf = Hunk_HighAllocName (size, "temp");

	hunk_tempactive = true;

	return buf;
#endif
}

/*
===============================================================================

CACHE MEMORY

===============================================================================
*/
#ifdef NOCACHE


typedef struct cache_system_s {
	cache_user_t			*user;
	struct cache_system_s *next;
	struct cache_system_s *prev;
	int size;
	char					name[16];
} cache_system_t;
cache_system_t *cache_head;

void Cache_Free (cache_user_t *c)
{
	cache_system_t *cs;
	if (c->data == NULL)
	{
		cache_head = NULL;	//this is evil and should never happen
		Sys_Error("Cache was already free\n");
		return;
	}
	cs = ((cache_system_t *)c->data)-1;
	cs = (cache_system_t*)((char*)cs - CACHEDEBUG);

	cs->user->data = NULL;

#if CACHEDEBUG>0
	{
		int i;
		qbyte *buf;
		buf = (qbyte *)(cs+1);
		for (i = 0; i < CACHEDEBUG; i++)
		{
			if (buf[i] != sentinalkey)
				Sys_Error("Cache memory corrupted (%i? bytes)", cs->size);
		}
		buf+=CACHEDEBUG;
		//app data
		buf += cs->size;
		for (i = 0; i < CACHEDEBUG; i++)
		{
			if (buf[i] != sentinalkey)
				Sys_Error("Cache memory corrupted (%i? bytes)", cs->size);
		}
	}
#endif


	if (cs->next)
		cs->next->prev = cs->prev;
	if (cs->prev)
		cs->prev->next = cs->next;

	if (cs == cache_head)
		cache_head = cs->next;

	BZ_Free(cs);
}

void *Cache_Check(cache_user_t *c)
{
	if (!c->data)
		return NULL;

	return c->data;
}

void Cache_Flush(void)
{
	while(cache_head)
	{
		Cache_Free(cache_head->user);
	}
}

void *Cache_Alloc (cache_user_t *c, int size, char *name)
{
	void	*buf;
	cache_system_t *nt;

	if (c->data)
		Sys_Error ("Cache_Alloc: already allocated");
	
	if (size <= 0)
		Sys_Error ("Cache_Alloc: size %i", size);

//	size = (size + 15) & ~15;

	nt = (cache_system_t*)BZ_Malloc(size + sizeof(cache_system_t) + CACHEDEBUG*2);
	if (!nt)
		Sys_Error("Cache_Alloc: failed on allocation of %i bytes", size);
	nt->next = cache_head;
	nt->prev = NULL;
	nt->user = c;
	nt->size = size;
	Q_strncpyz(nt->name, name, sizeof(nt->name));
	if (cache_head)
		cache_head->prev = nt;
	cache_head = nt;
	nt->user->fake = false;
	buf = (void *)(nt+1);
	memset(buf, sentinalkey, CACHEDEBUG);
	buf = (char*)buf+CACHEDEBUG;
	memset(buf, 0, size);
	memset((char *)buf+size, sentinalkey, CACHEDEBUG);
	c->data = buf;
	return c->data;
}

void Cache_FreeLow(int newlow)
{
}

void Cache_FreeHigh(int newhigh)
{
}

void Cache_Report (void)
{
}

void Hunk_Print_f (void)
{
	cache_system_t *cs;
	int zoneblocks;
	int cacheused;
	int zoneused;
	Hunk_Print(true);

	cacheused = 0;
	zoneused = 0;
	zoneblocks = 0;
	for (cs = cache_head; cs; cs = cs->next)
	{
		cacheused += cs->size;
	}
	Con_Printf("Cache: %iKB\n", cacheused/1024);
#if 0
	{
		zone_t *zone;

		for(zone = zone_head; zone; zone=zone->next)
		{
			zoneused += zone->size + sizeof(zone_t);
			zoneblocks++;
		}
		Con_Printf("Zone: %i containing %iKB\n", zoneblocks, zoneused/1024);
	}
#endif
}
void Cache_Init(void)
{
	Cmd_AddCommand ("flush", Cache_Flush);
	Cmd_AddCommand ("hunkprint", Hunk_Print_f);
#if 0
	Cmd_AddCommand ("zoneprint", Zone_Print_f);
#endif
#ifdef NAMEDMALLOCS
	Cmd_AddCommand ("zonegroups", Zone_Groups_f);
#endif
}

#else
typedef struct cache_system_s
{
	int						size;		// including this header
	cache_user_t			*user;
	char					name[16];
	struct cache_system_s	*prev, *next;
	struct cache_system_s	*lru_prev, *lru_next;	// for LRU flushing	
} cache_system_t;

cache_system_t *Cache_TryAlloc (int size, qboolean nobottom);

cache_system_t	cache_head;

/*
===========
Cache_Move
===========
*/
void Cache_Move ( cache_system_t *c)
{
	cache_system_t		*newc;

// we are clearing up space at the bottom, so only allocate it late
	newc = Cache_TryAlloc (c->size, true);
	if (newc)
	{
//		Con_Printf ("cache_move ok\n");

		Q_memcpy ( newc+1, c+1, c->size - sizeof(cache_system_t) );
		newc->user = c->user;
		Q_memcpy (newc->name, c->name, sizeof(newc->name));
		Cache_Free (c->user);
		newc->user->data = (void *)(newc+1);
	}
	else
	{
//		Con_Printf ("cache_move failed\n");

		Cache_Free (c->user);		// tough luck...
	}
}

/*
============
Cache_FreeLow

Throw things out until the hunk can be expanded to the given point
============
*/
void Cache_FreeLow (int new_low_hunk)
{
	cache_system_t	*c;
	
	while (1)
	{
		c = cache_head.next;
		if (c == &cache_head)
			return;		// nothing in cache at all
		if ((qbyte *)c >= hunk_base + new_low_hunk)
			return;		// there is space to grow the hunk
		Cache_Move ( c );	// reclaim the space
	}
}

/*
============
Cache_FreeHigh

Throw things out until the hunk can be expanded to the given point
============
*/
void Cache_FreeHigh (int new_high_hunk)
{
	cache_system_t	*c, *prev;
	
	prev = NULL;
	while (1)
	{
		c = cache_head.prev;
		if (c == &cache_head)
			return;		// nothing in cache at all
		if ( (qbyte *)c + c->size <= hunk_base + hunk_size - new_high_hunk)
			return;		// there is space to grow the hunk
		if (c == prev)
			Cache_Free (c->user);	// didn't move out of the way
		else
		{
			Cache_Move (c);	// try to move it
			prev = c;
		}
	}
}

void Cache_UnlinkLRU (cache_system_t *cs)
{
	if (!cs->lru_next || !cs->lru_prev)
		Sys_Error ("Cache_UnlinkLRU: NULL link");

	cs->lru_next->lru_prev = cs->lru_prev;
	cs->lru_prev->lru_next = cs->lru_next;
	
	cs->lru_prev = cs->lru_next = NULL;
}

void Cache_MakeLRU (cache_system_t *cs)
{
	if (cs->lru_next || cs->lru_prev)
		Sys_Error ("Cache_MakeLRU: active link");

	cache_head.lru_next->lru_prev = cs;
	cs->lru_next = cache_head.lru_next;
	cs->lru_prev = &cache_head;
	cache_head.lru_next = cs;
}

/*
============
Cache_TryAlloc

Looks for a free block of memory between the high and low hunk marks
Size should already include the header and padding
============
*/
cache_system_t *Cache_TryAlloc (int size, qboolean nobottom)
{
	cache_system_t	*cs, *newc;
	
// is the cache completely empty?

	if (!nobottom && cache_head.prev == &cache_head)
	{
		if (hunk_size - hunk_high_used - hunk_low_used < size)
			Sys_Error ("Cache_TryAlloc: %i is greater then free hunk", size);

		newc = (cache_system_t *) (hunk_base + hunk_low_used);
		memset (newc, 0, sizeof(*newc));
		newc->size = size;

		cache_head.prev = cache_head.next = newc;
		newc->prev = newc->next = &cache_head;
		
		Cache_MakeLRU (newc);
		return newc;
	}
	
// search from the bottom up for space

	newc = (cache_system_t *) (hunk_base + hunk_low_used);
	cs = cache_head.next;
	
	do
	{
		if (!nobottom || cs != cache_head.next)
		{
			if ( (qbyte *)cs - (qbyte *)newc >= size)
			{	// found space
				memset (newc, 0, sizeof(*newc));
				newc->size = size;
				
				newc->next = cs;
				newc->prev = cs->prev;
				cs->prev->next = newc;
				cs->prev = newc;
				
				Cache_MakeLRU (newc);
	
				return newc;
			}
		}

	// continue looking		
		newc = (cache_system_t *)((qbyte *)cs + cs->size);
		cs = cs->next;

	} while (cs != &cache_head);
	
// try to allocate one at the very end
	if ( hunk_base + hunk_size - hunk_high_used - (qbyte *)newc >= size)
	{
		memset (newc, 0, sizeof(*newc));
		newc->size = size;
		
		newc->next = &cache_head;
		newc->prev = cache_head.prev;
		cache_head.prev->next = newc;
		cache_head.prev = newc;
		
		Cache_MakeLRU (newc);

		return newc;
	}
	
	return NULL;		// couldn't allocate
}

/*
============
Cache_Flush

Throw everything out, so new data will be demand cached
============
*/
void Cache_Flush (void)
{
	while (cache_head.next != &cache_head)
		Cache_Free ( cache_head.next->user );	// reclaim the space
}


/*
============
Cache_Print

============
*/
void Cache_Print (void)
{
	cache_system_t	*cd;

	for (cd = cache_head.next ; cd != &cache_head ; cd = cd->next)
	{
		Con_Printf ("%8i : %s\n", cd->size, cd->name);
	}
}

/*
============
Cache_Report

============
*/
void Cache_Report (void)
{
	Con_DPrintf ("%4.1f megabyte data cache\n", (hunk_size - hunk_high_used - hunk_low_used) / (float)(1024*1024) );
}

/*
============
Cache_Compact

============
*/
void Cache_Compact (void)
{
}

/*
============
Cache_Init

============
*/
void Hunk_Print_f (void) {Hunk_Print(true);}
void Cache_Init (void)
{
	cache_head.next = cache_head.prev = &cache_head;
	cache_head.lru_next = cache_head.lru_prev = &cache_head;

	Cmd_AddCommand ("flush", Cache_Flush);

	Cmd_AddCommand ("hp", Hunk_Print_f);
}

/*
==============
Cache_Free

Frees the memory and removes it from the LRU list
==============
*/
void Cache_Free (cache_user_t *c)
{
	cache_system_t	*cs;

	if (!c->data)
		Sys_Error ("Cache_Free: not allocated");

	cs = ((cache_system_t *)c->data) - 1;

	cs->prev->next = cs->next;
	cs->next->prev = cs->prev;
	cs->next = cs->prev = NULL;

	c->data = NULL;

	Cache_UnlinkLRU (cs);
}



/*
==============
Cache_Check
==============
*/
void *Cache_Check (cache_user_t *c)
{
	cache_system_t	*cs;

	if (!c->data)
		return NULL;

	if (c->fake)	//malloc or somesuch.
		return c->data;

	cs = ((cache_system_t *)c->data) - 1;

// move to head of LRU
	Cache_UnlinkLRU (cs);
	Cache_MakeLRU (cs);
	
	return c->data;
}


/*
==============
Cache_Alloc
==============
*/
void *Cache_Alloc (cache_user_t *c, int size, char *name)
{
	cache_system_t	*cs;

	if (c->data)
		Sys_Error ("Cache_Alloc: already allocated");
	
	if (size <= 0)
		Sys_Error ("Cache_Alloc: size %i", size);

	size = (size + sizeof(cache_system_t) + 15) & ~15;

// find memory for it	
	while (1)
	{
		cs = Cache_TryAlloc (size, false);
		if (cs)
		{
			strncpy (cs->name, name, sizeof(cs->name)-1);
			c->data = (void *)(cs+1);
			cs->user = c;
			break;
		}
	
	// free the least recently used cahedat
		if (cache_head.lru_prev == &cache_head)
			Sys_Error ("Cache_Alloc: out of memory");
													// not enough memory at all
		Cache_Free ( cache_head.lru_prev->user );
	} 
	
	return Cache_Check (c);
}
#endif
//============================================================================

// Constant block functions

// CB_Malloc: creates a usable const_block
const_block_t *CB_Malloc (int size, int step)
{
	// alloc new const block
	const_block_t *cb = Z_Malloc(sizeof(const_block_t));

	// init cb members
	cb->block = BZ_Malloc(size);
	cb->point = cb->block;
	cb->curleft = size;
	cb->cursize = size;
	cb->memstep = step;

	return cb;
}

// CB_Slice: slices a chunk of memory off of the const block, and
// reallocs if necessary
char *CB_Slice (const_block_t *cb, int size)
{
	char *c;

	while (size > cb->curleft)
	{
		cb->block = BZ_Realloc(cb->block, cb->cursize + cb->memstep);
		cb->point = cb->block + (cb->cursize - cb->curleft);

		cb->cursize += cb->memstep;
		cb->curleft += cb->memstep;
	}

	c = cb->point;
	cb->point += size;
	cb->curleft -= size;

	return c;
}

// CB_Copy: copies a stream of bytes into a const block, returns
// pointer of copied string
char *CB_Copy (const_block_t *cb, char *data, int size)
{
	char *c;

	c = CB_Slice(cb, size);
	Q_memcpy(c, data, size);

	return c;
}

// CB_Free: frees a const block
void CB_Free (const_block_t *cb)
{
	BZ_Free(cb->block);
	Z_Free(cb);
}

#if 0
// CB_Reset: resets a const block to size
void CB_Reset (const_block_t *cb, int size)
{
	if (cb->cursize != size)
	{
		cb->block = BZ_Realloc(cb->block, size);
		cb->cursize = size;
	}

	cb->point = cb->block;
	cb->curleft = cb->cursize;
}

// CB_Trim: trims a const block to minimal size
void CB_Trim (const_block_t *cb)
{
	if (cb->curleft > 0)
	{
		cb->cursize -= cb->curleft;
		cb->block = BZ_Realloc(cb->block, cb->cursize);
		cb->point = cb->block + cb->cursize;
	}

	cb->curleft = 0;
}
#endif

/*
========================
Memory_Init
========================
*/
void Memory_Init (void *buf, int size)
{
#if 0 //ndef NOZONE
	int p;
	int zonesize = DYNAMIC_SIZE;
#endif

	hunk_base = (qbyte*)buf;
	hunk_size = size;
	hunk_low_used = 0;
	hunk_high_used = 0;

#if ZONEDEBUG>0 || HUNKDEBUG>0 || TEMPDEBUG>0||CACHEDEBUG>0
	srand(time(0));
	sentinalkey = rand();
#endif

	Cache_Init ();

#ifdef MULTITHREAD
	if (!zonelock)
		zonelock = Sys_CreateMutex(); // this can fail!
#endif

#if 0 //ndef NOZONE
	p = COM_CheckParm ("-zone");
	if (p)
	{
		if (p < com_argc-1)
			zonesize = Q_atoi (com_argv[p+1]) * 1024;
		else
			Sys_Error ("Memory_Init: you must specify a size in KB after -zone");
	}
	mainzone = Hunk_AllocName ( zonesize, "zone" );
	Z_ClearZone (mainzone, zonesize);
#endif
}

void Memory_DeInit(void)
{
#ifdef MULTITHREAD
	if (zonelock)
	{
		Sys_DestroyMutex(zonelock);
		zonelock = NULL;
	}
#endif
}

