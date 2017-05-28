/*
mod_terrain_create terrorgen; edit maps/terrorgen.hmp; map terrorgen
you can use mod_terrain_convert to generate+save the entire map for redistribution to people without this particular plugin version, ensuring longevity.
(this paticular command was meant to load+save the entire map, once mod_terrain_savever 2 is default...)

FIXME: no way to speciffy which gen plugin to use for a particular map
*/

#include "../plugin.h"
#include "glquake.h"
#include "com_mesh.h"
#include "gl_terrain.h"

static terrainfuncs_t *terr;
static modplugfuncs_t *modfuncs;

static void TerrorGen_GenerateOne(heightmap_t *hm, int sx, int sy, hmsection_t *s)
{
	int x,y,i;
	qbyte *lm;

	s->flags |= TSF_RELIGHT;

	//pick the textures to blend between. I'm just hardcoding shit here. this is meant to be some sort example.
	Q_strlcpy(s->texname[0], "city4_2", sizeof(s->texname[0]));
	Q_strlcpy(s->texname[1], "ground1_2", sizeof(s->texname[1]));
	Q_strlcpy(s->texname[2], "ground1_8", sizeof(s->texname[2]));
	Q_strlcpy(s->texname[3], "ground1_1", sizeof(s->texname[3]));

	for (y = 0, i=0; y < SECTHEIGHTSIZE; y++)
	for (x = 0; x < SECTHEIGHTSIZE; x++, i++)
	{
		//calculate where it is in worldspace, if that's useful to you.
		float wx = hm->sectionsize*(sx + x/(float)(SECTHEIGHTSIZE-1));
		float wy = hm->sectionsize*(sy + y/(float)(SECTHEIGHTSIZE-1));

		//many shallow mounds, on a grid.
		s->heights[i] = 128*sin(wx * (2*M_PI/1024)) * sin(wy * (2*M_PI/1024));

		//calculate the RGBA tint. these are floats, so you can oversaturate.
		s->colours[i][0] = 1;
		s->colours[i][1] = 1;
		s->colours[i][2] = 1;
		s->colours[i][3] = 1;
	}

	//make sure there's lightmap storage available
	terr->InitLightmap(s, /*fill with default values*/true);
	lm = terr->GetLightmap(s, 0, /*flag as edited*/true);
	if (lm)
	{	//pleaseworkpleaseworkpleasework
		for (y = 0; y < SECTTEXSIZE; y++, lm += (HMLMSTRIDE)*4)
		for (x = 0; x < SECTTEXSIZE; x++)
		{
			//calculate where it is in worldspace, if that's useful to you.
			float wx = hm->sectionsize*(sx + x/(float)(SECTTEXSIZE-1));
			float wy = hm->sectionsize*(sy + y/(float)(SECTTEXSIZE-1));

			//calc which texture to use
			//adds to 1, with texture[3] taking the remainder.
			lm[x*4+0] = max(0, 255 - 255*fabs(wx/1024));
			lm[x*4+1] = max(0, 255 - 255*fabs(wy/1024));
			lm[x*4+2] = min(lm[x*4+0],lm[x*4+1]);
			lm[x*4+0] -= lm[x*4+2];
			lm[x*4+1] -= lm[x*4+2];

			//logically: lm[x*4+3] = 255-(lm[x*4+0]+lm[x*4+1]+lm[x*4+2]);
			//however, the fourth channel is actually used as a lighting multiplier.
			lm[x*4+3] = 255;
		}
	}

	//insert the occasional mesh...
	if ((sx&3) == 0 && (sy&3) == 0)
	{
		vec3_t ang, org, axis[3];
		org[0] = hm->sectionsize*sx;
		org[1] = hm->sectionsize*sy;
		org[2] = 128;
		VectorClear(ang);
		ang[0] = sy*12.5;	//lul
		ang[1] = sx*12.5;
		modfuncs->AngleVectors(ang, axis[0], axis[1], axis[2]);
		VectorNegate(axis[1],axis[1]);	//axis[1] needs to be left, not right. silly quakeisms.

		//obviously you can insert mdls instead... preferably do that!
		terr->AddMesh(hm, TGS_TRYLOAD, NULL, "maps/dm4.bsp", org, axis, 1);
	}
}

#define GENBLOCKSIZE 1
static qboolean QDECL TerrorGen_GenerateBlock(heightmap_t *hm, int sx, int sy, unsigned int tgsflags)
{
	hmsection_t *sect[GENBLOCKSIZE*GENBLOCKSIZE];
	int mx = sx & ~(GENBLOCKSIZE-1);
	int my = sy & ~(GENBLOCKSIZE-1);

	if (!terr->GenerateSections(hm, mx, my, GENBLOCKSIZE, sect))
		return false;

	for (sy = 0; sy < GENBLOCKSIZE; sy++)
	{
		for (sx = 0; sx < GENBLOCKSIZE; sx++)
		{
			if (!sect[sx + sy*GENBLOCKSIZE])
				continue;	//already in memory.

			TerrorGen_GenerateOne(hm, mx+sx-CHUNKBIAS, my+sy-CHUNKBIAS, sect[sx + sy*GENBLOCKSIZE]);
			terr->FinishedSection(sect[sx + sy*GENBLOCKSIZE], true);
		}
	}
	return true;
}

static qintptr_t TerrorGen_Shutdown(qintptr_t *args)
{	//if its still us, make sure there's no dangling pointers.
	if (terr->AutogenerateSection == TerrorGen_GenerateBlock)
		terr->AutogenerateSection = NULL;
	return true;
}
qintptr_t Plug_Init(qintptr_t *args)
{
	if (CHECKBUILTIN(Mod_GetPluginModelFuncs))
	{
		modfuncs = pMod_GetPluginModelFuncs(sizeof(modplugfuncs_t));
		if (modfuncs && modfuncs->version < MODPLUGFUNCS_VERSION)
			modfuncs = NULL;
	}

	if (modfuncs && modfuncs->GetTerrainFuncs)
		terr = modfuncs->GetTerrainFuncs();
	if (!terr)
		return false;
	if (!Plug_Export("Shutdown", TerrorGen_Shutdown))
		return false;

	terr->AutogenerateSection = TerrorGen_GenerateBlock;
	return true;
}