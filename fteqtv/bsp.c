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

#include "qtv.h"

#define	MAX_MAP_LEAFS		32767

typedef struct {
	float planen[3];
	float planedist;
	int child[2];
} node_t;
struct bsp_s {
	node_t *nodes;
	unsigned char *pvslump;
	unsigned char **pvsofs;

	unsigned char decpvs[(MAX_MAP_LEAFS+7)/8];	//decompressed pvs
	int pvsbytecount;
};


typedef struct
{
	int			contents;
	int			visofs;				// -1 = no visibility info

	short		mins[3];			// for frustum culling
	short		maxs[3];

	unsigned short		firstmarksurface;
	unsigned short		nummarksurfaces;
#define NUM_AMBIENTS 4
	unsigned char		ambient_level[NUM_AMBIENTS];
} dleaf_t;
typedef struct
{
	unsigned int			planenum;
	short		children[2];	// negative numbers are -(leafs+1), not nodes
	short		mins[3];		// for sphere culling
	short		maxs[3];
	unsigned short	firstface;
	unsigned short	numfaces;	// counting both sides
} dnode_t;
typedef struct
{
	float	normal[3];
	float	dist;
	unsigned int		type;		// PLANE_X - PLANE_ANYZ ?remove? trivial to regenerate
} dplane_t;
typedef struct
{
	int		fileofs, filelen;
} lump_t;
#define	LUMP_PLANES		1
#define	LUMP_VISIBILITY	4
#define	LUMP_NODES		5
#define	LUMP_LEAFS		10
#define	HEADER_LUMPS	15
typedef struct
{
	int			version;	
	lump_t		lumps[HEADER_LUMPS];
} dheader_t;



void DecompressVis(unsigned char *in, unsigned char *out, int bytecount)
{
	int c;
	unsigned char *end;

	for (end = out + bytecount; out < end; )
	{
		c = *in;
		if (!c)
		{	//a 0 is always followed by the count of 0s.
			c = in[1];
			in += 2;
			for (; c > 4; c-=4)
			{
				*(unsigned int*)out = 0;
				out+=4;
			}
			for (; c; c--)
				*out++ = 0;
		}
		else
		{
			in++;
			*out++ = c;
		}
	}
}

typedef struct {
	char name[56];
	int offset;
	int length;
} pakfile;
// PACK, offset, lengthofpakfiles
FILE *FindInPaks(char *gamedir, char *filename, int *size)
{
	FILE *f;
	char fname[1024];
	int i, j;
	int numfiles;
	unsigned int header[3];

	pakfile pf;

	for (i = 0; ; i++)
	{
		sprintf(fname, "%s/pak%i.pak", gamedir, i);
		f = fopen(fname, "rb");
		if (!f)
			return NULL;	//ran out of possible pak files.

		fread(header, 1, sizeof(header), f);
		if (header[0] != *(unsigned int*)"PACK")
		{	//err... hmm.
			fclose(f);
			continue;
		}
		numfiles = header[2]/sizeof(pakfile);
		fseek(f, header[1], SEEK_SET);
		for (j = 0; j < numfiles; j++)
		{
			fread(&pf, 1, sizeof(pf), f);
			if (!strcmp(pf.name, filename))
			{
				fseek(f, pf.offset, 0);
				*size = pf.length;
				return f;
			}
		}
		fclose(f);
		//not found
	}
	return NULL;
}

unsigned char *ReadFile_WINDOWSSUCKS(char *gamedir, char *filename, int *size)
{
	unsigned char *data;

	FILE *f;
	char fname[1024];

	//try and read it straight out of the file system
	sprintf(fname, "%s/%s", gamedir, filename);
	f = fopen(fname, "rb");
	if (!f)
		f = fopen(filename, "rb");	//see if we're being run from inside the gamedir
	if (!f)
	{
		f = FindInPaks(gamedir, filename, size);
		if (!f)
			f = FindInPaks("id1", filename, size);
		if (!f)
		{
			printf("Couldn't open bsp file\n");
			return NULL;
		}
	}
	else
	{
		fseek(f, 0, SEEK_END);
		*size = ftell(f);
		fseek(f, 0, SEEK_SET);
	}
	data = malloc(*size);
	fread(data, 1, *size, f);
	fclose(f);

	return data;
}

bsp_t *BSP_LoadModel(char *gamedir, char *bspname)
{
	unsigned char *data;
	unsigned int size;

	dheader_t *header;
	dplane_t *planes;
	dnode_t *nodes;
	dleaf_t *leaf;

	int numnodes, i;
	int numleafs;

	bsp_t *bsp;

	if (!gamedir)
		gamedir = "qw";

	data = ReadFile_WINDOWSSUCKS(gamedir, bspname, &size);
	if (!data)
	{
		data = ReadFile_WINDOWSSUCKS("id1", bspname, &size);
		if (!data)
		{
			printf("Couldn't open bsp file \"%s\" (gamedir \"%s\")\n", bspname, gamedir);
			return NULL;
		}
	}


	header = (dheader_t*)data;
	if (data[0] != 29)
	{
		printf("BSP not version 29\n", bspname, gamedir);
		return NULL;
	}

	planes = (dplane_t*)(data+header->lumps[LUMP_PLANES].fileofs);
	nodes = (dnode_t*)(data+header->lumps[LUMP_NODES].fileofs);
	leaf = (dleaf_t*)(data+header->lumps[LUMP_LEAFS].fileofs);

	numnodes = header->lumps[LUMP_NODES].filelen/sizeof(dnode_t);
	numleafs = header->lumps[LUMP_LEAFS].filelen/sizeof(dleaf_t);

	bsp = malloc(sizeof(bsp_t) + sizeof(node_t)*numnodes + header->lumps[LUMP_VISIBILITY].filelen + sizeof(unsigned char *)*numleafs);
	bsp->nodes = (node_t*)(bsp+1);
	bsp->pvsofs = (unsigned char**)(bsp->nodes+numnodes);
	bsp->pvslump = (unsigned char*)(bsp->pvsofs+numleafs);

	bsp->pvsbytecount = (numleafs+7)/8;

	for (i = 0; i < numnodes; i++)
	{
		bsp->nodes[i].child[0] = nodes[i].children[0];
		bsp->nodes[i].child[1] = nodes[i].children[1];
		bsp->nodes[i].planedist = planes[nodes[i].planenum].dist;
		bsp->nodes[i].planen[0] = planes[nodes[i].planenum].normal[0];
		bsp->nodes[i].planen[1] = planes[nodes[i].planenum].normal[1];
		bsp->nodes[i].planen[2] = planes[nodes[i].planenum].normal[2];
	}
	memcpy(bsp->pvslump, data+header->lumps[LUMP_VISIBILITY].fileofs, header->lumps[LUMP_VISIBILITY].filelen);

	for (i = 0; i < numleafs; i++)
	{
		if (leaf[i].visofs < 0)
			bsp->pvsofs[i] = NULL;
		else
			bsp->pvsofs[i] = bsp->pvslump+leaf[i].visofs;
	}

	free(data);

	return bsp;
}

void BSP_Free(bsp_t *bsp)
{
	free(bsp);
}

int BSP_SphereLeafNums_r(bsp_t *bsp, int first, int maxleafs, unsigned short *list, float *pos, float radius)
{
	node_t *node;
	float dot;
	int rn;
	int numleafs = 0;

	if (!bsp)
		return 0;

	for(rn = first;rn >= 0;)
	{
		node = &bsp->nodes[rn];
		dot = (node->planen[0]*pos[0] + node->planen[1]*pos[1] + node->planen[2]*pos[2]) - node->planedist;
		if (dot < -radius)
			rn = node->child[1];
		else if (dot > radius)
			rn = node->child[0];
		else
		{
			rn = BSP_SphereLeafNums_r(bsp, node->child[0], maxleafs-numleafs, list+numleafs, pos, radius);
			if (rn < 0)
				return -1;	//ran out, so don't use pvs for this entity.
			else
				numleafs += rn;
			rn = node->child[1];	//both sides
		}
	}

	rn = -1-rn;

	if (maxleafs>numleafs)
	{
		list[numleafs] = rn-1;
		numleafs++;
	}
	else
		return -1;	//there are just too many

	return numleafs;
}

int BSP_SphereLeafNums(bsp_t *bsp, int maxleafs, unsigned short *list, float x, float y, float z, float radius)
{
	float pos[3];
	pos[0] = x;
	pos[1] = y;
	pos[2] = z;
	return BSP_SphereLeafNums_r(bsp, 0, maxleafs, list, pos, radius);
}

int BSP_LeafNum(bsp_t *bsp, float x, float y, float z)
{
	node_t *node;
	float dot;
	int rn;

	if (!bsp)
		return 0;

	for(rn = 0;rn >= 0;)
	{
		node = &bsp->nodes[rn];
		dot = node->planen[0]*x + node->planen[1]*y + node->planen[2]*z;
		rn = node->child[(dot-node->planedist) <= 0];
	}

	return -1-rn;
}

qboolean BSP_Visible(bsp_t *bsp, int leafcount, unsigned short *list)
{
	int i;
	if (!bsp)
		return true;

	if (leafcount < 0)	//too many, so pvs was switched off.
		return true;

	for (i = 0; i < leafcount; i++)
	{
		if (bsp->decpvs[list[i]>>3] & (1<<(list[i]&7)))
			return true;
	}
	return false;
}

void BSP_SetupForPosition(bsp_t *bsp, float x, float y, float z)
{
	int leafnum;
	if (!bsp)
		return;

	leafnum = BSP_LeafNum(bsp, x, y, z);
	DecompressVis(bsp->pvsofs[leafnum], bsp->decpvs, bsp->pvsbytecount);
}

