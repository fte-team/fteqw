/*
doom_glbsp.cpp - in-engine GL-node builder for Doom maps.

Wraps ZDBSP's FNodeBuilder (tools/zdbsp) so the engine can synthesise glBSP-style
GL nodes at map-load time for any Doom WAD that lacks them. Without GL nodes the
sector floor/ceiling triangulation can't close (segs don't include the BSP partition
edges), producing floating/partial flats and HOM. This builds them on the fly.

Compiled in an isolated static library (see CMakeLists) with the ZDBSP sources so its
C++ types don't clash with the engine's C headers. Exposes a single extern "C" entry
point; output buffers are allocated with the engine's Z_Malloc so the C loader frees
them normally. Outputs the classic uncompressed gNd2 lump format the engine already
parses (GL_VERT/GL_SEGS/GL_SSECT/GL_NODES).
*/

#include <math.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "zdbsp.h"
#include "doomdata.h"
#include "nodebuild.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

//engine allocator (zeroes memory). declared here to avoid pulling in FTE's C headers.
extern "C" void *Z_Malloc(int size);

//-----------------------------------------------------------------------------
// ZDBSP globals it expects (normally defined in main.cpp), plus FLevel methods
// (normally in processor.cpp). We don't compile those files, so provide them here.
//-----------------------------------------------------------------------------
int   MaxSegs = 64;
int   SplitCost = 8;
int   AAPreference = 16;
int   SSELevel = 0;

//referenced by nodebuild_extract.cpp (normally defined in ZDBSP's main.cpp). Stay quiet.
void Warn (const char *format, ...)
{
	(void)format;
}

angle_t PointToAngle (fixed_t x, fixed_t y)
{
	double ang = atan2 (double(y), double(x));
	const double rad2bam = double(1<<30) / M_PI;
	double dbam = ang * rad2bam;
	return angle_t(dbam) << 1;
}

FLevel::FLevel ()
{
	memset (this, 0, sizeof(*this));
}
FLevel::~FLevel ()
{
	if (Vertices)		delete[] Vertices;
	if (Subsectors)		delete[] Subsectors;
	if (Segs)			delete[] Segs;
	if (Nodes)			delete[] Nodes;
	if (Blockmap)		delete[] Blockmap;
	if (Reject)			delete[] Reject;
	if (GLSubsectors)	delete[] GLSubsectors;
	if (GLSegs)			delete[] GLSegs;
	if (GLNodes)		delete[] GLNodes;
	if (GLVertices)		delete[] GLVertices;
	if (GLPVS)			delete[] GLPVS;
	if (OrgSectorMap)	delete[] OrgSectorMap;
}

//-----------------------------------------------------------------------------
// gNd2 serialisers - byte-identical to ZDBSP's WriteGLVertices/WriteGLSegs/
// WriteSSectors2/WriteNodes2 (uncompressed v2), which the engine's loader parses.
// We're little-endian on the supported targets, so no byteswap is performed.
//-----------------------------------------------------------------------------
static void SerializeGLVert (FLevel &lev, void **out, int *outlen)
{
	int i, count = lev.NumGLVertices - lev.NumOrgVerts;
	WideVertex *vd = lev.GLVertices + lev.NumOrgVerts;
	int len = 4 + count*2*(int)sizeof(int);
	char *buf = (char*)Z_Malloc (len);
	int *p;
	buf[0]='g'; buf[1]='N'; buf[2]='d'; buf[3]='2';
	p = (int*)(buf+4);
	for (i = 0; i < count; ++i)
	{
		p[i*2+0] = vd[i].x;
		p[i*2+1] = vd[i].y;
	}
	*out = buf; *outlen = len;
}
static void SerializeGLSegs (FLevel &lev, void **out, int *outlen)
{
	int i, count = lev.NumGLSegs;
	int len = count*(int)sizeof(MapSegGL);
	MapSegGL *sd = (MapSegGL*)Z_Malloc (len);
	for (i = 0; i < count; ++i)
	{
		MapSegGLEx &g = lev.GLSegs[i];
		sd[i].v1 = (g.v1 < (DWORD)lev.NumOrgVerts) ? (WORD)g.v1 : (WORD)(0x8000 | (g.v1 - lev.NumOrgVerts));
		sd[i].v2 = (g.v2 < (DWORD)lev.NumOrgVerts) ? (WORD)g.v2 : (WORD)(0x8000 | (g.v2 - lev.NumOrgVerts));
		sd[i].linedef = (WORD)g.linedef;
		sd[i].side    = (WORD)g.side;
		sd[i].partner = (WORD)g.partner;	//NO_INDEX (0xffffffff) -> 0xffff, which the loader treats as "none"
	}
	*out = sd; *outlen = len;
}
static void SerializeGLSSect (FLevel &lev, void **out, int *outlen)
{
	int i, count = lev.NumGLSubsectors;
	int len = count*(int)sizeof(MapSubsector);
	MapSubsector *ss = (MapSubsector*)Z_Malloc (len);
	for (i = 0; i < count; ++i)
	{
		ss[i].firstline = (WORD)lev.GLSubsectors[i].firstline;
		ss[i].numlines  = (WORD)lev.GLSubsectors[i].numlines;
	}
	*out = ss; *outlen = len;
}
static void SerializeGLNodes (FLevel &lev, void **out, int *outlen)
{
	int i, j, count = lev.NumGLNodes;
	int len = count*(int)sizeof(MapNode);
	short *base = (short*)Z_Malloc (len);
	short *n = base;
	for (i = 0; i < count; ++i)
	{
		MapNodeEx &z = lev.GLNodes[i];
		const short *ib = (const short*)&z.bbox[0][0];
		n[0] = (short)(z.x>>16); n[1] = (short)(z.y>>16);
		n[2] = (short)(z.dx>>16); n[3] = (short)(z.dy>>16);
		n += 4;
		for (j = 0; j < 2*4; ++j)
			n[j] = ib[j];
		n += 8;
		for (j = 0; j < 2; ++j)
		{
			DWORD child = z.children[j];
			if (child & NFX_SUBSECTOR)
				*n++ = (short)(WORD)(child - (NFX_SUBSECTOR + NF_SUBSECTOR));
			else
				*n++ = (short)(WORD)child;
		}
	}
	*out = base; *outlen = len;
}

//-----------------------------------------------------------------------------
// Doom_GLBSP_Build: takes the raw VERTEXES/LINEDEFS/SIDEDEFS/SECTORS lumps (as the
// engine already loaded them) and produces the four gNd2 GL lumps. Returns 1 on
// success (outputs Z_Malloc'd, caller owns), 0 on failure (outputs untouched).
//-----------------------------------------------------------------------------
extern "C" int Doom_GLBSP_Build (
	const void *vtx, int vtxlen,
	const void *lin, int linlen,
	const void *sid, int sidlen,
	const void *sec, int seclen,
	void **o_vert,  int *o_vertlen,
	void **o_segs,  int *o_segslen,
	void **o_ssect, int *o_ssectlen,
	void **o_nodes, int *o_nodeslen)
{
	int i;
	int nverts    = vtxlen / (int)sizeof(MapVertex);
	int nlines    = linlen / (int)sizeof(MapLineDef);
	int nsides    = sidlen / (int)sizeof(MapSideDef);
	int nsectors  = seclen / (int)sizeof(MapSector);

	if (nverts < 3 || nlines < 3 || nsides < 1 || nsectors < 1)
		return 0;

	try
	{
		FLevel Level;
		const MapVertex  *mv = (const MapVertex*)vtx;
		const MapLineDef *ml = (const MapLineDef*)lin;
		const MapSideDef *ms = (const MapSideDef*)sid;
		const MapSector  *mc = (const MapSector*)sec;
		fixed_t minx, miny, maxx, maxy;

		// vertices
		Level.Vertices = new WideVertex[nverts];
		Level.NumVertices = nverts;
		for (i = 0; i < nverts; ++i)
		{
			Level.Vertices[i].x = (fixed_t)mv[i].x << FRACBITS;
			Level.Vertices[i].y = (fixed_t)mv[i].y << FRACBITS;
			Level.Vertices[i].index = 0;
		}
		// map bounds (inlined FLevel::FindMapBounds)
		minx = maxx = Level.Vertices[0].x;
		miny = maxy = Level.Vertices[0].y;
		for (i = 1; i < nverts; ++i)
		{
				 if (Level.Vertices[i].x < minx) minx = Level.Vertices[i].x;
			else if (Level.Vertices[i].x > maxx) maxx = Level.Vertices[i].x;
				 if (Level.Vertices[i].y < miny) miny = Level.Vertices[i].y;
			else if (Level.Vertices[i].y > maxy) maxy = Level.Vertices[i].y;
		}
		Level.MinX = minx; Level.MinY = miny; Level.MaxX = maxx; Level.MaxY = maxy;

		// linedefs (vanilla/binary format)
		Level.Lines.Resize (nlines);
		for (i = 0; i < nlines; ++i)
		{
			Level.Lines[i].v1 = ml[i].v1;
			Level.Lines[i].v2 = ml[i].v2;
			Level.Lines[i].flags = ml[i].flags;
			Level.Lines[i].sidenum[0] = ml[i].sidenum[0];
			Level.Lines[i].sidenum[1] = ml[i].sidenum[1];
			if (Level.Lines[i].sidenum[0] == NO_MAP_INDEX) Level.Lines[i].sidenum[0] = NO_INDEX;
			if (Level.Lines[i].sidenum[1] == NO_MAP_INDEX) Level.Lines[i].sidenum[1] = NO_INDEX;
			Level.Lines[i].special = 0;
			Level.Lines[i].args[0] = ml[i].special;
			Level.Lines[i].args[1] = ml[i].tag;
			Level.Lines[i].args[2] = Level.Lines[i].args[3] = Level.Lines[i].args[4] = 0;
		}

		// sidedefs (only 'sector' matters to the builder)
		Level.Sides.Resize (nsides);
		for (i = 0; i < nsides; ++i)
		{
			Level.Sides[i].textureoffset = ms[i].textureoffset;
			Level.Sides[i].rowoffset = ms[i].rowoffset;
			memcpy (Level.Sides[i].toptexture,    ms[i].toptexture,    8);
			memcpy (Level.Sides[i].bottomtexture, ms[i].bottomtexture, 8);
			memcpy (Level.Sides[i].midtexture,    ms[i].midtexture,    8);
			Level.Sides[i].sector = ms[i].sector;
			if (Level.Sides[i].sector == NO_MAP_INDEX) Level.Sides[i].sector = NO_INDEX;
		}

		// sectors
		Level.Sectors.Resize (nsectors);
		for (i = 0; i < nsectors; ++i)
			Level.Sectors[i].data = mc[i];

		// build GL nodes
		TArray<FNodeBuilder::FPolyStart> nostarts, noanchors;
		FNodeBuilder builder (Level, nostarts, noanchors, "DOOM", true);
		builder.GetVertices (Level.GLVertices, Level.NumGLVertices);
		builder.GetGLNodes (Level.GLNodes, Level.NumGLNodes,
			Level.GLSegs, Level.NumGLSegs,
			Level.GLSubsectors, Level.NumGLSubsectors);

		if (!Level.GLNodes || !Level.GLSegs || !Level.GLSubsectors || !Level.GLVertices)
			return 0;

		SerializeGLVert  (Level, o_vert,  o_vertlen);
		SerializeGLSegs  (Level, o_segs,  o_segslen);
		SerializeGLSSect (Level, o_ssect, o_ssectlen);
		SerializeGLNodes (Level, o_nodes, o_nodeslen);
		return 1;
	}
	catch (...)
	{
		return 0;
	}
}
