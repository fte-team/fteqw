#include "quakedef.h"
#ifndef SERVERONLY
#include "glquake.h"
#endif
#include "com_mesh.h"

#define MAX_Q3MAP_INDICES 0x8000000	//just a sanity limit
#define	MAX_Q3MAP_VERTEXES	0x800000	//just a sanity limit
//#define MAX_CM_PATCH_VERTS		(4096)
//#define MAX_CM_FACES			(MAX_Q2MAP_FACES)
#ifdef FTE_TARGET_WEB
#define MAX_CM_PATCHES			(0x1000)		//fixme
#else
#define MAX_CM_PATCHES			(0x10000)		//fixme
#endif
//#define MAX_CM_LEAFFACES		(MAX_Q2MAP_LEAFFACES)
#define	MAX_CM_AREAS			MAX_Q2MAP_AREAS

//#define Q3SURF_NODAMAGE		0x00000001
//#define Q3SURF_SLICK			0x00000002
//#define Q3SURF_SKY			0x00000004
//#define Q3SURF_LADDER			0x00000008
//#define Q3SURF_NOIMPACT		0x00000010
//#define Q3SURF_NOMARKS		0x00000020
//#define Q3SURF_FLESH			0x00000040
#define	Q3SURF_NODRAW			0x00000080	// don't generate a drawsurface at all
//#define Q3SURF_HINT			0x00000100
#define	Q3SURF_SKIP				0x00000200	// completely ignore, allowing non-closed brushes
//#define Q3SURF_NOLIGHTMAP		0x00000400
//#define Q3SURF_POINTLIGHT		0x00000800
//#define Q3SURF_METALSTEPS		0x00001000
//#define Q3SURF_NOSTEPS		0x00002000
#define	Q3SURF_NONSOLID			0x00004000	// don't collide against curves with this set
//#define Q3SURF_LIGHTFILTER	0x00008000
//#define Q3SURF_ALPHASHADOW	0x00010000
//#define Q3SURF_NODLIGHT		0x00020000
//#define Q3SURF_DUST			0x00040000
cvar_t q3bsp_surf_meshcollision_flag = CVARD("q3bsp_surf_meshcollision_flag", "0x80000000", "The surfaceparm flag(s) that enables q3bsp trisoup collision");
cvar_t q3bsp_surf_meshcollision_force = CVARD("q3bsp_surf_meshcollision_force", "0", "Force mesh-based collisions on all q3bsp trisoup surfaces.");
cvar_t q3bsp_mergeq3lightmaps = CVARD("q3bsp_mergedlightmaps", "16", "Specifies the maximum number of lightmaps that may be merged for performance reasons. Unfortunately this breaks tcgen on lightmap passes - if you care, set this to 1.");

#if Q3SURF_NODRAW != TI_NODRAW
#error "nodraw isn't constant"
#endif

extern cvar_t r_shadow_bumpscale_basetexture;

//these are in model.c (or gl_model.c)
qboolean Mod_LoadVertexes (model_t *loadmodel, qbyte *mod_base, lump_t *l);
qboolean Mod_LoadVertexNormals (model_t *loadmodel, qbyte *mod_base, lump_t *l);
qboolean Mod_LoadEdges (model_t *loadmodel, qbyte *mod_base, lump_t *l, qboolean lm);
qboolean Mod_LoadMarksurfaces (model_t *loadmodel, qbyte *mod_base, lump_t *l, qboolean lm);
qboolean Mod_LoadSurfedges (model_t *loadmodel, qbyte *mod_base, lump_t *l);
void Mod_LoadEntities (model_t *loadmodel, qbyte *mod_base, lump_t *l);

extern void BuildLightMapGammaTable (float g, float c);

#if defined(Q2BSPS) || defined(Q3BSPS)
static qboolean CM_NativeTrace(model_t *model, int forcehullnum, framestate_t *framestate, vec3_t axis[3], vec3_t start, vec3_t end, vec3_t mins, vec3_t maxs, qboolean capsule, unsigned int contents, trace_t *trace);
static unsigned int CM_NativeContents(struct model_s *model, int hulloverride, framestate_t *framestate, vec3_t axis[3], vec3_t p, vec3_t mins, vec3_t maxs);
static unsigned int Q2BSP_PointContents(model_t *mod, vec3_t axis[3], vec3_t p);
static int CM_PointCluster (model_t *mod, vec3_t p);
#endif

float RadiusFromBounds (vec3_t mins, vec3_t maxs)
{
	int		i;
	vec3_t	corner;

	for (i=0 ; i<3 ; i++)
	{
		corner[i] = fabs(mins[i]) > fabs(maxs[i]) ? fabs(mins[i]) : fabs(maxs[i]);
	}

	return Length (corner);
}

void CalcSurfaceExtents (model_t *mod, msurface_t *s)
{
	float	mins[2], maxs[2], val;
	int		i,j, e;
	mvertex_t	*v;
	mtexinfo_t	*tex;
	int		bmins[2], bmaxs[2];
	int idx;

	mins[0] = mins[1] = 999999;
	maxs[0] = maxs[1] = -999999;

	tex = s->texinfo;

	for (i=0 ; i<s->numedges ; i++)
	{
		e = mod->surfedges[s->firstedge+i];
		idx = e < 0;
		if (idx)
			e = -e;
		if (e < 0 || e >= mod->numedges)
			v = &mod->vertexes[0];
		else
			v = &mod->vertexes[mod->edges[e].v[idx]];

		for (j=0 ; j<2 ; j++)
		{
			val = v->position[0] * tex->vecs[j][0] +
				v->position[1] * tex->vecs[j][1] +
				v->position[2] * tex->vecs[j][2] +
				tex->vecs[j][3];
			if (val < mins[j])
				mins[j] = val;
			if (val > maxs[j])
				maxs[j] = val;
		}
	}

	for (i=0 ; i<2 ; i++)
	{
		bmins[i] = floor(mins[i]/(1<<s->lmshift));
		bmaxs[i] = ceil(maxs[i]/(1<<s->lmshift));

		s->texturemins[i] = bmins[i] << s->lmshift;
		s->extents[i] = (bmaxs[i] - bmins[i]);

//		if ( !(tex->flags & TEX_SPECIAL) && s->extents[i] > 17 )	//vanilla used 16(+1), glquake used 17(+1). FTE uses 255(+1), but we omit lightmapping instead of crashing if its larger than our limit, so we omit the check here. different engines use different limits here, many of them make no sense.
//			Con_Printf ("Bad surface extents (texture %s, more than %i lightmap samples)\n", s->texinfo->texture->name, s->extents[i]);

		s->extents[i] <<= s->lmshift;
	}
}

void AddPointToBounds (vec3_t v, vec3_t mins, vec3_t maxs)
{
	int		i;
	vec_t	val;

	for (i=0 ; i<3 ; i++)
	{
		val = v[i];
		if (val < mins[i])
			mins[i] = val;
		if (val > maxs[i])
			maxs[i] = val;
	}
}

void ClearBounds (vec3_t mins, vec3_t maxs)
{
	mins[0] = mins[1] = mins[2] = FLT_MAX;
	maxs[0] = maxs[1] = maxs[2] = -FLT_MAX;
}


void Mod_SortShaders(model_t *mod)
{
	//surely this isn't still needed?
	texture_t *textemp;
	int i, j;

	//sort loadmodel->textures
	for (i = 0; i < mod->numtextures; i++)
	{
		for (j = i+1; j < mod->numtextures; j++)
		{
			if ((mod->textures[i]->shader && mod->textures[j]->shader) && (mod->textures[j]->shader->sort < mod->textures[i]->shader->sort))
			{
				textemp = mod->textures[j];
				mod->textures[j] = mod->textures[i];
				mod->textures[i] = textemp;
			}
		}
	}
}



#if defined(Q2BSPS) || defined(Q3BSPS)

qbyte *ReadPCXPalette(qbyte *buf, int len, qbyte *out);

#ifdef SERVERONLY
#define Host_Error SV_Error
#endif

extern qbyte *mod_base;

#define capsuledist(dist,plane,mins,maxs)					\
		case shape_iscapsule:								\
			dist = DotProduct(trace_up, plane->normal);		\
			dist = dist*(trace_capsulesize[(dist<0)?1:2]) - trace_capsulesize[0];	\
			dist = plane->dist - dist;						\
			break;

unsigned char d_q28to24table[1024];


/*
typedef struct q2csurface_s
{
	char		name[16];
	int			flags;
	int			value;
} q2csurface_t;
*/

typedef struct q2mapsurface_s  // used internally due to name len probs //ZOID
{
	q2csurface_t	c;
	char		rname[32];
} q2mapsurface_t;

























typedef struct {
	char		shader[64];
	int			brushNum;
	int			visibleSide;	// the brush side that ray tests need to clip against (-1 == none)
} dfog_t;

typedef struct
{
	mplane_t	*plane;
	q2mapsurface_t	*surface;
} q2cbrushside_t;

typedef struct
{
	int			checkcount;		// to avoid repeated testings
	int			contents;
	vec3_t		absmins;
	vec3_t		absmaxs;
	int			numsides;
	q2cbrushside_t *brushside;
} q2cbrush_t;

typedef struct
{
	int		numareaportals;
	int		firstareaportal;
	int		floodnum;			// if two areas have equal floodnums, they are connected
	int		floodvalid;
} q2carea_t;

typedef struct
{
	int			numareaportals[MAX_CM_AREAS];
} q3carea_t;

typedef struct
{
	vec3_t		absmins, absmaxs;

	int			numfacets;
	q2cbrush_t	*facets;
#define numbrushes numfacets
#define brushes facets

	q2mapsurface_t	*surface;
	int			checkcount;		// to avoid repeated testings
} q3cpatch_t;

typedef struct
{
	vec3_t		absmins, absmaxs;

	vecV_t		*xyz_array;
	size_t numverts;
	index_t		*indicies;
	size_t numincidies;

	q2mapsurface_t	*surface;
	int			checkcount;		// to avoid repeated testings
} q3cmesh_t;

typedef struct
{
	int		facetype;

	int		numverts;
	int		firstvert;

	int		shadernum;
	union
	{
		struct
		{
			int		cp[2];
		} patch;
		struct
		{
			int firstindex;
			int numindicies;
		} soup;
	};
} q3cface_t;

typedef struct cmodel_s
{
	vec3_t		mins, maxs;
	vec3_t		origin;		// for sounds or lights
	mnode_t		*headnode;
	mleaf_t		*headleaf;
	int		numsurfaces;
	int		firstsurface;

	int firstbrush;	//q3 submodels are considered small enough that you will never need to walk any sort of tree.
	int num_brushes;//the brushes are checked instead.
} cmodel_t;

/*used to trace*/
static int			checkcount;

typedef struct cminfo_s
{
	int				numbrushsides;
	q2cbrushside_t *brushsides;

	q2mapsurface_t	*surfaces;

	int				numleafbrushes;
	q2cbrush_t		*leafbrushes[MAX_Q2MAP_LEAFBRUSHES];

	int				numcmodels;
	cmodel_t		*cmodels;

	int				numbrushes;
	q2cbrush_t		*brushes;

	int				numvisibility;
	q2dvis_t		*q2vis;
	q3dvis_t		*q3pvs;
	q3dvis_t		*q3phs;

	int				numareas;
	q2carea_t		q2areas[MAX_Q2MAP_AREAS];
	q3carea_t		q3areas[MAX_CM_AREAS];
	int				numareaportals;
	q2dareaportal_t	areaportals[MAX_Q2MAP_AREAPORTALS];

	//list of mesh surfaces within the leaf
	q3cmesh_t	cmeshes[MAX_CM_PATCHES];
	int			numcmeshes;
	int			*leafcmeshes;
	int			numleafcmeshes;
	int			maxleafcmeshes;

	//FIXME: remove the below
	//(deprecated) patch collisions
	q3cpatch_t	patches[MAX_CM_PATCHES];
	int			numpatches;
	int			*leafpatches;
	int			numleafpatches;
	int			maxleafpatches;
	//FIXME: remove the above

	int			floodvalid;
	qbyte		portalopen[MAX_Q2MAP_AREAPORTALS];	//memset will work if it's a qbyte, really it should be a qboolean

	int			mapisq3;



#ifdef Q3BSPS
	//this is for loading stuff. it used to be globals, but we have threads now. and multiple q3bsps at the same time is a problem.
	int			numvertexes;
	vecV_t		*verts;	//3points
	vec2_t		*vertstmexcoords;
	vec2_t		*vertlstmexcoords[MAXRLIGHTMAPS];
	vec4_t		*colors4f_array[MAXRLIGHTMAPS];
	vec3_t		*normals_array;
	//vec3_t		*map_svector_array;
	//vec3_t		*map_tvector_array;

	index_t *surfindexes;
	//int	map_numsurfindexes;

	q3cface_t	*faces;
	int			numfaces;
#endif
} cminfo_t;

static q2mapsurface_t	nullsurface;

cvar_t		map_noareas			= CVAR("map_noareas", "0");	//1 for lack of mod support.
cvar_t		map_noCurves		= CVARF("map_noCurves", "0", CVAR_CHEAT);
cvar_t		map_autoopenportals	= CVARD("map_autoopenportals", "0", "When set to 1, force-opens all area portals. Normally these start closed and are opened by doors when they move, but this requires the gamecode to signal this.");	//1 for lack of mod support.
cvar_t		r_subdivisions		= CVAR("r_subdivisions", "2");

static int		CM_NumInlineModels (model_t *model);
static cmodel_t	*CM_InlineModel (model_t *model, char *name);
void	CM_InitBoxHull (void);
static void CM_FinalizeBrush(q2cbrush_t *brush);
static void	FloodAreaConnections (cminfo_t	*prv);

qboolean BoundsIntersect (vec3_t mins1, vec3_t maxs1, vec3_t mins2, vec3_t maxs2)
{
	return (mins1[0] <= maxs2[0] && mins1[1] <= maxs2[1] && mins1[2] <= maxs2[2] &&
		 maxs1[0] >= mins2[0] && maxs1[1] >= mins2[1] && maxs1[2] >= mins2[2]);
}

#ifdef Q3BSPS

int	PlaneTypeForNormal ( vec3_t normal )
{
	vec_t	ax, ay, az;

// NOTE: should these have an epsilon around 1.0?
	if ( normal[0] >= 1.0)
		return PLANE_X;
	if ( normal[1] >= 1.0 )
		return PLANE_Y;
	if ( normal[2] >= 1.0 )
		return PLANE_Z;

	ax = fabs( normal[0] );
	ay = fabs( normal[1] );
	az = fabs( normal[2] );

	if ( ax >= ay && ax >= az )
		return PLANE_ANYX;
	if ( ay >= ax && ay >= az )
		return PLANE_ANYY;
	return PLANE_ANYZ;
}

void CategorizePlane ( mplane_t *plane )
{
	int i;

	plane->signbits = 0;
	plane->type = PLANE_ANYZ;
	for (i = 0; i < 3; i++)
	{
		if (plane->normal[i] < 0)
			plane->signbits |= 1<<i;
		if (plane->normal[i] == 1.0f)
			plane->type = i;
	}
	plane->type = PlaneTypeForNormal(plane->normal);
}

void PlaneFromPoints ( vec3_t verts[3], mplane_t *plane )
{
	vec3_t	v1, v2;

	VectorSubtract( verts[1], verts[0], v1 );
	VectorSubtract( verts[2], verts[0], v2 );
	CrossProduct( v2, v1, plane->normal );
	VectorNormalize( plane->normal );
	plane->dist = DotProduct( verts[0], plane->normal );
}

/*
===============
Patch_FlatnessTest
===============
*/
static int Patch_FlatnessTest( float maxflat2, const float *point0, const float *point1, const float *point2 )
{
	float d;
	int ft0, ft1;
	vec3_t t, n;
	vec3_t v1, v2, v3;

	VectorSubtract( point2, point0, n );
	if( !VectorNormalize( n ) )
		return 0;

	VectorSubtract( point1, point0, t );
	d = -DotProduct( t, n );
	VectorMA( t, d, n, t );
	if( DotProduct( t, t ) < maxflat2 )
		return 0;

	VectorAvg( point1, point0, v1 );
	VectorAvg( point2, point1, v2 );
	VectorAvg( v1, v2, v3 );

	ft0 = Patch_FlatnessTest( maxflat2, point0, v1, v3 );
	ft1 = Patch_FlatnessTest( maxflat2, v3, v2, point2 );

	return 1 + (int)( floor( max( ft0, ft1 ) ) + 0.5f );
}

/*
===============
Patch_GetFlatness
===============
*/
static void Patch_GetFlatness( float maxflat, const float *points, int comp, const int *patch_cp, int *flat )
{
	int i, p, u, v;
	float maxflat2 = maxflat * maxflat;

	flat[0] = flat[1] = 0;
	for( v = 0; v < patch_cp[1] - 1; v += 2 )
	{
		for( u = 0; u < patch_cp[0] - 1; u += 2 )
		{
			p = v * patch_cp[0] + u;

			i = Patch_FlatnessTest( maxflat2, &points[p*comp], &points[( p+1 )*comp], &points[( p+2 )*comp] );
			flat[0] = max( flat[0], i );
			i = Patch_FlatnessTest( maxflat2, &points[( p+patch_cp[0] )*comp], &points[( p+patch_cp[0]+1 )*comp], &points[( p+patch_cp[0]+2 )*comp] );
			flat[0] = max( flat[0], i );
			i = Patch_FlatnessTest( maxflat2, &points[( p+2*patch_cp[0] )*comp], &points[( p+2*patch_cp[0]+1 )*comp], &points[( p+2*patch_cp[0]+2 )*comp] );
			flat[0] = max( flat[0], i );

			i = Patch_FlatnessTest( maxflat2, &points[p*comp], &points[( p+patch_cp[0] )*comp], &points[( p+2*patch_cp[0] )*comp] );
			flat[1] = max( flat[1], i );
			i = Patch_FlatnessTest( maxflat2, &points[( p+1 )*comp], &points[( p+patch_cp[0]+1 )*comp], &points[( p+2*patch_cp[0]+1 )*comp] );
			flat[1] = max( flat[1], i );
			i = Patch_FlatnessTest( maxflat2, &points[( p+2 )*comp], &points[( p+patch_cp[0]+2 )*comp], &points[( p+2*patch_cp[0]+2 )*comp] );
			flat[1] = max( flat[1], i );
		}
	}
}

/*
===============
Patch_Evaluate_QuadricBezier
===============
*/
static void Patch_Evaluate_QuadricBezier( float t, const vec_t *point0, const vec_t *point1, const vec_t *point2, vec_t *out, int comp )
{
	int i;
	vec_t qt = t * t;
	vec_t dt = 2.0f * t, tt, tt2;

	tt = 1.0f - dt + qt;
	tt2 = dt - 2.0f * qt;

	for( i = 0; i < comp; i++ )
		out[i] = point0[i] * tt + point1[i] * tt2 + point2[i] * qt;
}

/*
===============
Patch_Evaluate
===============
*/
static void Patch_Evaluate( const vec_t *p, const int *numcp, const int *tess, vec_t *dest, int comp )
{
	int num_patches[2], num_tess[2];
	int index[3], dstpitch, i, u, v, x, y;
	float s, t, step[2];
	vec_t *tvec, *tvec2;
	const vec_t *pv[3][3];
	vec4_t v1, v2, v3;

	num_patches[0] = numcp[0] / 2;
	num_patches[1] = numcp[1] / 2;
	dstpitch = ( num_patches[0] * tess[0] + 1 ) * comp;

	step[0] = 1.0f / (float)tess[0];
	step[1] = 1.0f / (float)tess[1];

	for( v = 0; v < num_patches[1]; v++ )
	{
		// last patch has one more row
		if( v < num_patches[1] - 1 )
			num_tess[1] = tess[1];
		else
			num_tess[1] = tess[1] + 1;

		for( u = 0; u < num_patches[0]; u++ )
		{
			// last patch has one more column
			if( u < num_patches[0] - 1 )
				num_tess[0] = tess[0];
			else
				num_tess[0] = tess[0] + 1;

			index[0] = ( v * numcp[0] + u ) * 2;
			index[1] = index[0] + numcp[0];
			index[2] = index[1] + numcp[0];

			// current 3x3 patch control points
			for( i = 0; i < 3; i++ )
			{
				pv[i][0] = &p[( index[0]+i ) * comp];
				pv[i][1] = &p[( index[1]+i ) * comp];
				pv[i][2] = &p[( index[2]+i ) * comp];
			}

			tvec = dest + v * tess[1] * dstpitch + u * tess[0] * comp;
			for( y = 0, t = 0.0f; y < num_tess[1]; y++, t += step[1], tvec += dstpitch )
			{
				Patch_Evaluate_QuadricBezier( t, pv[0][0], pv[0][1], pv[0][2], v1, comp );
				Patch_Evaluate_QuadricBezier( t, pv[1][0], pv[1][1], pv[1][2], v2, comp );
				Patch_Evaluate_QuadricBezier( t, pv[2][0], pv[2][1], pv[2][2], v3, comp );

				for( x = 0, tvec2 = tvec, s = 0.0f; x < num_tess[0]; x++, s += step[0], tvec2 += comp )
					Patch_Evaluate_QuadricBezier( s, v1, v2, v3, tvec2, comp );
			}
		}
	}
}


#define	PLANE_NORMAL_EPSILON	0.00001
#define	PLANE_DIST_EPSILON	0.01
static qboolean ComparePlanes( const vec3_t p1normal, vec_t p1dist, const vec3_t p2normal, vec_t p2dist )
{
	if( fabs( p1normal[0] - p2normal[0] ) < PLANE_NORMAL_EPSILON
	    && fabs( p1normal[1] - p2normal[1] ) < PLANE_NORMAL_EPSILON
	    && fabs( p1normal[2] - p2normal[2] ) < PLANE_NORMAL_EPSILON
	    && fabs( p1dist - p2dist ) < PLANE_DIST_EPSILON )
		return true;

	return false;
}

static void SnapVector( vec3_t normal )
{
	int i;

	for( i = 0; i < 3; i++ )
	{
		if( fabs( normal[i] - 1 ) < PLANE_NORMAL_EPSILON )
		{
			VectorClear( normal );
			normal[i] = 1;
			break;
		}
		if( fabs( normal[i] + 1 ) < PLANE_NORMAL_EPSILON )
		{
			VectorClear( normal );
			normal[i] = -1;
			break;
		}
	}
}

#define Q_rint( x )   ( ( x ) < 0 ? ( (int)( ( x )-0.5f ) ) : ( (int)( ( x )+0.5f ) ) )
static void SnapPlane( vec3_t normal, vec_t *dist )
{
	SnapVector( normal );

	if( fabs( *dist - Q_rint( *dist ) ) < PLANE_DIST_EPSILON )
	{
		*dist = Q_rint( *dist );
	}
}

/*
===============================================================================

					PATCH LOADING

===============================================================================
*/

#define MAX_FACET_PLANES 32
#define cm_subdivlevel	15

/*
* CM_CreateFacetFromPoints
*/
static int CM_CreateFacetFromPoints(q2cbrush_t *facet, vec3_t *verts, int numverts, q2mapsurface_t *shaderref, mplane_t *brushplanes )
{
	int i, j;
	int axis, dir;
	vec3_t normal, mins, maxs;
	float d, dist;
	mplane_t mainplane;
	vec3_t vec, vec2;
	int numbrushplanes;

	// set default values for brush
	facet->numsides = 0;
	facet->brushside = NULL;
	facet->contents = shaderref->c.value;

	// calculate plane for this triangle
	PlaneFromPoints( verts, &mainplane );
	if( ComparePlanes( mainplane.normal, mainplane.dist, vec3_origin, 0 ) )
		return 0;

	// test a quad case
	if( numverts > 3 )
	{
		d = DotProduct( verts[3], mainplane.normal ) - mainplane.dist;
		if( d < -0.1 || d > 0.1 )
			return 0;

		if( 0 )
		{
			vec3_t v[3];
			mplane_t plane;

			// try different combinations of planes
			for( i = 1; i < 4; i++ )
			{
				VectorCopy( verts[i], v[0] );
				VectorCopy( verts[( i+1 )%4], v[1] );
				VectorCopy( verts[( i+2 )%4], v[2] );
				PlaneFromPoints( v, &plane );

				if( fabs( DotProduct( mainplane.normal, plane.normal ) ) < 0.9 )
					return 0;
			}
		}
	}

	numbrushplanes = 0;

	// add front plane
	SnapPlane( mainplane.normal, &mainplane.dist );
	VectorCopy( mainplane.normal, brushplanes[numbrushplanes].normal );
	brushplanes[numbrushplanes].dist = mainplane.dist; numbrushplanes++;

	// calculate mins & maxs
	ClearBounds( mins, maxs );
	for( i = 0; i < numverts; i++ )
		AddPointToBounds( verts[i], mins, maxs );

	// add the axial planes
	for( axis = 0; axis < 3; axis++ )
	{
		for( dir = -1; dir <= 1; dir += 2 )
		{
			for( i = 0; i < numbrushplanes; i++ )
			{
				if( brushplanes[i].normal[axis] == dir )
					break;
			}

			if( i == numbrushplanes )
			{
				VectorClear( normal );
				normal[axis] = dir;
				if( dir == 1 )
					dist = maxs[axis];
				else
					dist = -mins[axis];

				VectorCopy( normal, brushplanes[numbrushplanes].normal );
				brushplanes[numbrushplanes].dist = dist; numbrushplanes++;
			}
		}
	}

	// add the edge bevels
	for( i = 0; i < numverts; i++ )
	{
		j = ( i + 1 ) % numverts;
//		k = ( i + 2 ) % numverts;

		VectorSubtract( verts[i], verts[j], vec );
		if( VectorNormalize( vec ) < 0.5 )
			continue;

		SnapVector( vec );
		for( j = 0; j < 3; j++ )
		{
			if( vec[j] == 1 || vec[j] == -1 )
				break; // axial
		}
		if( j != 3 )
			continue; // only test non-axial edges

		// try the six possible slanted axials from this edge
		for( axis = 0; axis < 3; axis++ )
		{
			for( dir = -1; dir <= 1; dir += 2 )
			{
				// construct a plane
				VectorClear( vec2 );
				vec2[axis] = dir;
				CrossProduct( vec, vec2, normal );
				if( VectorNormalize( normal ) < 0.5 )
					continue;
				dist = DotProduct( verts[i], normal );

				for( j = 0; j < numbrushplanes; j++ )
				{
					// if this plane has already been used, skip it
					if( ComparePlanes( brushplanes[j].normal, brushplanes[j].dist, normal, dist ) )
						break;
				}
				if( j != numbrushplanes )
					continue;

				// if all other points are behind this plane, it is a proper edge bevel
				for( j = 0; j < numverts; j++ )
				{
					if( j != i )
					{
						d = DotProduct( verts[j], normal ) - dist;
						if( d > 0.1 )
							break; // point in front: this plane isn't part of the outer hull
					}
				}
				if( j != numverts )
					continue;

				// add this plane
				VectorCopy( normal, brushplanes[numbrushplanes].normal );
				brushplanes[numbrushplanes].dist = dist; numbrushplanes++;
				if( numbrushplanes == MAX_FACET_PLANES )
					break;
			}
		}
	}

	return ( facet->numsides = numbrushplanes );
}

/*
* CM_CreatePatch
*/
static void CM_CreatePatch(model_t *loadmodel, q3cpatch_t *patch, q2mapsurface_t *shaderref, const vec_t *verts, const int *patch_cp )
{
	int step[2], size[2], flat[2];
	int i, j, k ,u, v;
	int numsides, totalsides;
	q2cbrush_t *facets, *facet;
	vecV_t *points;
	vec3_t tverts[4];
	qbyte *data;
	mplane_t *brushplanes;

	patch->surface = shaderref;

	// find the degree of subdivision in the u and v directions
	Patch_GetFlatness( cm_subdivlevel, verts, sizeof(vecV_t)/sizeof(vec_t), patch_cp, flat );

	step[0] = 1 << flat[0];
	step[1] = 1 << flat[1];
	size[0] = ( patch_cp[0] >> 1 ) * step[0] + 1;
	size[1] = ( patch_cp[1] >> 1 ) * step[1] + 1;
	if( size[0] <= 0 || size[1] <= 0 )
		return;

	data = BZ_Malloc( size[0] * size[1] * sizeof( vecV_t ) +
		( size[0]-1 ) * ( size[1]-1 ) * 2 * ( sizeof( q2cbrush_t ) + 32 * sizeof( mplane_t ) ) );

	points = ( vecV_t * )data; data += size[0] * size[1] * sizeof( vecV_t );
	facets = ( q2cbrush_t * )data; data += ( size[0]-1 ) * ( size[1]-1 ) * 2 * sizeof( q2cbrush_t );
	brushplanes = ( mplane_t * )data; data += ( size[0]-1 ) * ( size[1]-1 ) * 2 * MAX_FACET_PLANES * sizeof( mplane_t );

	// fill in
	Patch_Evaluate(verts, patch_cp, step, points[0], sizeof(vecV_t)/sizeof(vec_t));

	totalsides = 0;
	patch->numfacets = 0;
	patch->facets = NULL;
	ClearBounds( patch->absmins, patch->absmaxs );

	// create a set of facets
	for( v = 0; v < size[1]-1; v++ )
	{
		for( u = 0; u < size[0]-1; u++ )
		{
			i = v * size[0] + u;
			VectorCopy( points[i], tverts[0] );
			VectorCopy( points[i + size[0]], tverts[1] );
			VectorCopy( points[i + size[0] + 1], tverts[2] );
			VectorCopy( points[i + 1], tverts[3] );

			for( i = 0; i < 4; i++ )
				AddPointToBounds( tverts[i], patch->absmins, patch->absmaxs );

			// try to create one facet from a quad
			numsides = CM_CreateFacetFromPoints( &facets[patch->numfacets], tverts, 4, shaderref, brushplanes + totalsides );
			if( !numsides )
			{	// create two facets from triangles
				VectorCopy( tverts[3], tverts[2] );
				numsides = CM_CreateFacetFromPoints( &facets[patch->numfacets], tverts, 3, shaderref, brushplanes + totalsides );
				if( numsides )
				{
					totalsides += numsides;
					patch->numfacets++;
				}

				VectorCopy( tverts[2], tverts[0] );
				VectorCopy( points[v *size[0] + u + size[0] + 1], tverts[2] );
				numsides = CM_CreateFacetFromPoints( &facets[patch->numfacets], tverts, 3, shaderref, brushplanes + totalsides );
			}

			if( numsides )
			{
				totalsides += numsides;
				patch->numfacets++;
			}
		}
	}

	if (patch->numfacets)
	{
		qbyte *data;

		data = ZG_Malloc(&loadmodel->memgroup, patch->numfacets * sizeof( q2cbrush_t ) + totalsides * ( sizeof( q2cbrushside_t ) + sizeof( mplane_t ) ));

		patch->facets = ( q2cbrush_t * )data; data += patch->numfacets * sizeof( q2cbrush_t );
		memcpy( patch->facets, facets, patch->numfacets * sizeof( q2cbrush_t ) );
		for( i = 0, k = 0, facet = patch->facets; i < patch->numfacets; i++, facet++ )
		{
			mplane_t *planes;
			q2cbrushside_t *s;

			facet->brushside = ( q2cbrushside_t * )data; data += facet->numsides * sizeof( q2cbrushside_t );
			planes = ( mplane_t * )data; data += facet->numsides * sizeof( mplane_t );

			for( j = 0, s = facet->brushside; j < facet->numsides; j++, s++ )
			{
				planes[j] = brushplanes[k++];

				s->plane = &planes[j];
				SnapPlane( s->plane->normal, &s->plane->dist );
				CategorizePlane( s->plane );
				s->surface = shaderref;
			}
		}

		for( i = 0; i < 3; i++ )
		{
			// spread the mins / maxs by a pixel
			patch->absmins[i] -= 1;
			patch->absmaxs[i] += 1;
		}
	}

	BZ_Free( points );
}

//======================================================

/*
=================
CM_CreatePatchesForLeafs
=================
*/
static qboolean CM_CreatePatchesForLeafs (model_t *loadmodel, cminfo_t *prv)
{
	int i, j, k;
	mleaf_t *leaf;
	q3cface_t *face;
	q2mapsurface_t *surf;
	q3cpatch_t *patch;
	q3cmesh_t *cmesh;
	int *checkout = alloca(sizeof(int)*prv->numfaces);

	if (map_noCurves.ival)
		return true;

	memset (checkout, -1, sizeof(int)*prv->numfaces);

	for (i = 0, leaf = loadmodel->leafs; i < loadmodel->numleafs; i++, leaf++)
	{
		leaf->numleafpatches = 0;
		leaf->firstleafpatch = prv->numleafpatches;
		leaf->numleafcmeshes = 0;
		leaf->firstleafcmesh = prv->numleafcmeshes;

		if (leaf->cluster == -1)
			continue;

		for (j=0 ; j<leaf->nummarksurfaces ; j++)
		{
			k = leaf->firstmarksurface[j] - loadmodel->surfaces;
			if (k >= prv->numfaces)
			{
				Con_Printf (CON_ERROR "CM_CreatePatchesForLeafs: corrupt map\n");
				break;
			}
			face = &prv->faces[k];

			if (face->numverts <= 0)
				continue;
			if (face->shadernum < 0 || face->shadernum >= loadmodel->numtextures)
				continue;
			surf = &prv->surfaces[face->shadernum];
			if (!surf->c.value)	//surface has no contents value, so can't ever block anything.
				continue;

			switch(face->facetype)
			{
			case MST_TRIANGLE_SOUP:
				if (!face->soup.numindicies)
					continue;
				//only enable mesh collisions if its meant to be enabled.
				//we haven't parsed any shaders, so we depend upon the stuff that the bsp compiler left lying around.
				if (!(surf->c.flags & q3bsp_surf_meshcollision_flag.ival) && !q3bsp_surf_meshcollision_force.ival)
					continue;

				if (prv->numleafcmeshes >= prv->maxleafcmeshes)
				{
					prv->maxleafcmeshes *= 2;
					prv->maxleafcmeshes += 16;
					if (prv->numleafcmeshes > prv->maxleafcmeshes)
					{	//detect overflow
						Con_Printf (CON_ERROR "CM_CreateCMeshesForLeafs: map is insanely huge!\n");
						return false;
					}
					prv->leafcmeshes = realloc(prv->leafcmeshes, sizeof(*prv->leafcmeshes) * prv->maxleafcmeshes);
				}

				// the patch was already built
				if (checkout[k] != -1)
				{
					prv->leafcmeshes[prv->numleafcmeshes] = checkout[k];
					cmesh = &prv->cmeshes[checkout[k]];
				}
				else
				{
					if (prv->numcmeshes >= MAX_CM_PATCHES)
					{
						Con_Printf (CON_ERROR "CM_CreatePatchesForLeafs: map has too many patches\n");
						return false;
					}

					cmesh = &prv->cmeshes[prv->numcmeshes];
					prv->leafcmeshes[prv->numleafcmeshes] = prv->numcmeshes;
					checkout[k] = prv->numcmeshes++;

	//gcc warns without this cast

					cmesh->surface = surf;
					cmesh->numverts = face->numverts;
					cmesh->numincidies = face->soup.numindicies;
					cmesh->xyz_array = ZG_Malloc(&loadmodel->memgroup, cmesh->numverts * sizeof(*cmesh->xyz_array) + cmesh->numincidies * sizeof(*cmesh->indicies));
					cmesh->indicies = (index_t*)(cmesh->xyz_array + cmesh->numverts);

					VectorCopy(prv->verts[face->firstvert+0], cmesh->xyz_array[0]);
					VectorCopy(cmesh->xyz_array[0], cmesh->absmaxs);
					VectorCopy(cmesh->xyz_array[0], cmesh->absmins);
					for (k = 1; k < cmesh->numverts; k++)
					{
						VectorCopy(prv->verts[face->firstvert+k], cmesh->xyz_array[k]);
						AddPointToBounds(cmesh->xyz_array[k], cmesh->absmins, cmesh->absmaxs);
					}
					for (k = 0; k < cmesh->numincidies; k++)
						cmesh->indicies[k] = prv->surfindexes[face->soup.firstindex+k];
				}
				leaf->contents |= surf->c.value;
				leaf->numleafcmeshes++;

				prv->numleafcmeshes++;

				break;
			case MST_PATCH:
				if (face->patch.cp[0] <= 0 || face->patch.cp[1] <= 0)
					continue;

				if ( !surf->c.value || (surf->c.flags & Q3SURF_NONSOLID) )
					continue;

				if (prv->numleafpatches >= prv->maxleafpatches)
				{
					prv->maxleafpatches *= 2;
					prv->maxleafpatches += 16;
					if (prv->numleafpatches > prv->maxleafpatches)
					{	//detect overflow
						Con_Printf (CON_ERROR "CM_CreatePatchesForLeafs: map is insanely huge!\n");
						return false;
					}
					prv->leafpatches = realloc(prv->leafpatches, sizeof(*prv->leafpatches) * prv->maxleafpatches);
				}

				// the patch was already built
				if (checkout[k] != -1)
				{
					prv->leafpatches[prv->numleafpatches] = checkout[k];
					patch = &prv->patches[checkout[k]];
				}
				else
				{
					if (prv->numpatches >= MAX_CM_PATCHES)
					{
						Con_Printf (CON_ERROR "CM_CreatePatchesForLeafs: map has too many patches\n");
						return false;
					}

					patch = &prv->patches[prv->numpatches];
					prv->leafpatches[prv->numleafpatches] = prv->numpatches;
					checkout[k] = prv->numpatches++;

	//gcc warns without this cast
					CM_CreatePatch (loadmodel, patch, surf, (const vec_t *)(prv->verts + face->firstvert), face->patch.cp );
				}
				leaf->contents |= patch->surface->c.value;
				leaf->numleafpatches++;

				prv->numleafpatches++;
				break;
			}
		}
	}

	return true;
}
#endif


/*
===============================================================================

					MAP LOADING

===============================================================================
*/

static void CMod_SetParent (mnode_t *node, mnode_t *parent)
{
	node->parent = parent;
	if (node->contents != -1)
		return;
	CMod_SetParent (node->children[0], node);
	CMod_SetParent (node->children[1], node);
}

#ifdef Q2BSPS
/*
=================
CMod_LoadSubmodels
=================
*/
static qboolean CModQ2_LoadSubmodels (model_t *loadmodel, qbyte *mod_base, lump_t *l)
{
	cminfo_t	*prv = (cminfo_t*)loadmodel->meshinfo;
	q2dmodel_t	*in;
	cmodel_t	*out;
	int			i, j, count;

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
	{
		Con_Printf (CON_ERROR "MOD_LoadBmodel: funny lump size\n");
		return false;
	}
	count = l->filelen / sizeof(*in);

	if (count < 1)
	{
		Con_Printf (CON_ERROR "Map with no models\n");
		return false;
	}
	if (count > SANITY_MAX_Q2MAP_MODELS)
	{
		Con_Printf (CON_ERROR "Map has too many models\n");
		return false;
	}

	out = prv->cmodels = ZG_Malloc(&loadmodel->memgroup, count * sizeof(*prv->cmodels));
	prv->numcmodels = count;

	for (i=0 ; i<count ; i++, in++, out++)
	{
		for (j=0 ; j<3 ; j++)
		{	// spread the mins / maxs by a pixel
			out->mins[j] = LittleFloat (in->mins[j]) - 1;
			out->maxs[j] = LittleFloat (in->maxs[j]) + 1;
			out->origin[j] = LittleFloat (in->origin[j]);
		}
		out->headnode = loadmodel->nodes + LittleLong (in->headnode);
		out->firstsurface = LittleLong (in->firstface);
		out->numsurfaces = LittleLong (in->numfaces);
	}

	AddPointToBounds(prv->cmodels[0].mins, loadmodel->mins, loadmodel->maxs);
	AddPointToBounds(prv->cmodels[0].maxs, loadmodel->mins, loadmodel->maxs);

	return true;
}


/*
=================
CMod_LoadSurfaces
=================
*/
static qboolean CModQ2_LoadSurfaces (model_t *mod, qbyte *mod_base, lump_t *l)
{
	cminfo_t	*prv = (cminfo_t*)mod->meshinfo;
	q2texinfo_t	*in;
	q2mapsurface_t	*out;
	int			i, count;

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
	{
		Con_Printf (CON_ERROR "MOD_LoadBmodel: funny lump size\n");
		return false;
	}
	count = l->filelen / sizeof(*in);
	if (count < 1)
	{
		Con_Printf (CON_ERROR "Map with no surfaces\n");
		return false;
	}
//	if (count > MAX_Q2MAP_TEXINFO)
//		Host_Error ("Map has too many surfaces");

	mod->numtexinfo = count;
	out = prv->surfaces = ZG_Malloc(&mod->memgroup, count * sizeof(*prv->surfaces));

	for ( i=0 ; i<count ; i++, in++, out++)
	{
		Q_strncpyz (out->c.name, in->texture, sizeof(out->c.name));
		Q_strncpyz (out->rname, in->texture, sizeof(out->rname));
		out->c.flags = LittleLong (in->flags);
		out->c.value = LittleLong (in->value);
	}

	return true;
}
#ifndef SERVERONLY
static texture_t *Mod_LoadWall(model_t *loadmodel, char *mapname, char *texname, char *shadername, unsigned int imageflags)
{
	char name[MAX_QPATH];
	q2miptex_t replacementwal;
	texture_t *tex;
	q2miptex_t *wal;
	image_t *base;

	Q_snprintfz (name, sizeof(name), "textures/%s.wal", texname);
	wal = (void *)FS_LoadMallocFile (name, NULL);
	if (!wal)
	{
		wal = &replacementwal;
		memset(wal, 0, sizeof(*wal));
		Q_strncpyz(wal->name, texname, sizeof(wal->name));
		wal->width = 64;
		wal->height = 64;
	}

	wal->width = LittleLong(wal->width);
	wal->height = LittleLong(wal->height);
	{
		int i;

		for (i = 0; i < MIPLEVELS; i++)
			wal->offsets[i] = LittleLong(wal->offsets[i]);
	}

	wal->flags = LittleLong(wal->flags);
	wal->contents = LittleLong(wal->contents);
	wal->value = LittleLong(wal->value);

	tex = ZG_Malloc(&loadmodel->memgroup, sizeof(texture_t));

	tex->width = wal->width;
	tex->height = wal->height;

	if (!tex->width || !tex->height || wal == &replacementwal)
	{
		imageflags |= IF_LOADNOW;	//make sure the size is known BEFORE it returns.
		if (wal->offsets[0])
			base = R_LoadReplacementTexture(wal->name, "bmodels", imageflags, (qbyte *)wal+wal->offsets[0], wal->width, wal->height, TF_SOLID8);
		else
			base = R_LoadReplacementTexture(wal->name, "bmodels", imageflags, NULL, 0, 0, TF_INVALID);
	}
	else
		base = NULL;

	if (wal == &replacementwal)
	{
		if (base)
		{
			if (base->status == TEX_LOADED||base->status==TEX_LOADING)
			{
				tex->width = base->width;
				tex->height = base->height;
			}
			else
				Con_Printf("Unable to load textures/%s.wal\n", wal->name);
		}

	}
	else
	{
		unsigned int size = 
		(wal->width>>0)*(wal->height>>0) +
		(wal->width>>1)*(wal->height>>1) +
		(wal->width>>2)*(wal->height>>2) +
		(wal->width>>3)*(wal->height>>3);

		tex->mips[0] = BZ_Malloc(size);
		tex->palette = host_basepal;
		tex->mips[1] = tex->mips[0] + (wal->width>>0)*(wal->height>>0);
		tex->mips[2] = tex->mips[1] + (wal->width>>1)*(wal->height>>1);
		tex->mips[3] = tex->mips[2] + (wal->width>>2)*(wal->height>>2);
		memcpy(tex->mips[0], (qbyte *)wal + wal->offsets[0], (wal->width>>0)*(wal->height>>0));
		memcpy(tex->mips[1], (qbyte *)wal + wal->offsets[1], (wal->width>>1)*(wal->height>>1));
		memcpy(tex->mips[2], (qbyte *)wal + wal->offsets[2], (wal->width>>2)*(wal->height>>2));
		memcpy(tex->mips[3], (qbyte *)wal + wal->offsets[3], (wal->width>>3)*(wal->height>>3));

		BZ_Free(wal);
	}

	return tex;
}

static qboolean CModQ2_LoadTexInfo (model_t *mod, qbyte *mod_base, lump_t *l, char *mapname)	//yes I know these load from the same place
{
	q2texinfo_t *in;
	mtexinfo_t *out;
	int 	i, j, count;
	char	*lwr;
	char	sname[MAX_QPATH];
	int texcount;

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
	{
		Con_Printf ("MOD_LoadBmodel: funny lump size in %s\n", mod->name);
		return false;
	}
	count = l->filelen / sizeof(*in);
	out = ZG_Malloc(&mod->memgroup, count*sizeof(*out));

	mod->textures = ZG_Malloc(&mod->memgroup, sizeof(texture_t *)*count);
	texcount = 0;

	mod->texinfo = out;
	mod->numtexinfo = count;

	if (in[0].nexttexinfo != -1)
	{
		for (i = 1; i < count && in[i].nexttexinfo == in[0].nexttexinfo; i++)
			;
		if (i == count)
		{
			Con_Printf("WARNING: invalid texture animations in \"%s\"\n", mod->name);
			for (i = 0; i < count; i++) 
				in[i].nexttexinfo = -1;
		}
	}

	for ( i=0 ; i<count ; i++, in++, out++)
	{
		out->flags = LittleLong (in->flags);

		for (j=0 ; j<4 ; j++)
			out->vecs[0][j] = LittleFloat (in->vecs[0][j]);
		for (j=0 ; j<4 ; j++)
			out->vecs[1][j] = LittleFloat (in->vecs[1][j]);
		out->vecscale[0] = 1.0/Length (out->vecs[0]);
		out->vecscale[1] = 1.0/Length (out->vecs[1]);

		if (out->flags & TI_SKY)
			Q_snprintfz(sname, sizeof(sname), "sky/%s", in->texture);
		else
			Q_snprintfz(sname, sizeof(sname), "%s", in->texture);
		if (out->flags & (TI_WARP))
			Q_strncatz(sname, "#WARP", sizeof(sname));
		if (out->flags & TI_FLOWING)
			Q_strncatz(sname, "#FLOW", sizeof(sname));
		if (out->flags & TI_TRANS66)
			Q_strncatz(sname, "#ALPHA=0.66", sizeof(sname));
		else if (out->flags & TI_TRANS33)
			Q_strncatz(sname, "#ALPHA=0.33", sizeof(sname));
		else if (out->flags & (TI_WARP))
			Q_strncatz(sname, "#ALPHA=1", sizeof(sname));
		if (in->nexttexinfo != -1)	//used to ensure non-looping and looping don't conflict and get confused.
			Q_strncatz(sname, "#ANIMLOOP", sizeof(sname));

		//in q2, 'TEX_SPECIAL' is TI_LIGHT, and that conflicts.
		out->flags &= ~TI_LIGHT;
		if (out->flags & (TI_SKY|TI_TRANS33|TI_TRANS66|TI_WARP))
			out->flags |= TEX_SPECIAL;

		//compact the textures.
		for (j=0; j < texcount; j++)
		{
			if (!strcmp(sname, mod->textures[j]->name))
			{
				out->texture = mod->textures[j];
				break;
			}
		}
		if (j == texcount)	//load a new one
		{
			for (lwr = in->texture; *lwr; lwr++)
			{
				if (*lwr >= 'A' && *lwr <= 'Z')
					*lwr = *lwr - 'A' + 'a';
			}
			out->texture = Mod_LoadWall (mod, mapname, in->texture, sname, (out->flags&TEX_SPECIAL)?0:IF_NOALPHA);
			if (!out->texture || !out->texture->width || !out->texture->height)
			{
				out->texture = ZG_Malloc(&mod->memgroup, sizeof(texture_t) + 16*16+8*8+4*4+2*2);

				Con_Printf (CON_WARNING "Couldn't load \"%s.wal\"\n", in->texture);
				memcpy(out->texture, r_notexture_mip, sizeof(texture_t) + 16*16+8*8+4*4+2*2);
			}

			Q_strncpyz(out->texture->name, sname, sizeof(out->texture->name));

			mod->textures[texcount++] = out->texture;
		}

//		if (in->nexttexinfo != -1)
//		{
//			Con_DPrintf("FIXME: %s should animate to %s\n", in->texture, (in->nexttexinfo+(q2texinfo_t *)(mod_base + l->fileofs))->texture);
//		}
	}

	in = (void *)(mod_base + l->fileofs);
	out = mod->texinfo;
	for (i=0 ; i<count ; i++)
	{
		if (in[i].nexttexinfo >= 0 && in[i].nexttexinfo < count)
			out[i].texture->anim_next = out[in[i].nexttexinfo].texture;
	}
	for (i=0 ; i<count ; i++)
	{
		texture_t *tex;
		if (!out[i].texture->anim_next)
			continue;

		out[i].texture->anim_total = 1;
		for (tex = out[i].texture->anim_next ; tex && tex != out[i].texture && out[i].texture->anim_total < 100; tex=tex->anim_next)
			out[i].texture->anim_total++;
	}

	mod->numtextures = texcount;

	Mod_SortShaders(mod);
	return true;
}
#endif
/*
static void CalcSurfaceExtents (msurface_t *s)
{
	float	mins[2], maxs[2], val;
	int		i,j, e;
	mvertex_t	*v;
	mtexinfo_t	*tex;
	int		bmins[2], bmaxs[2];

	mins[0] = mins[1] = 999999;
	maxs[0] = maxs[1] = -99999;

	tex = s->texinfo;

	for (i=0 ; i<s->numedges ; i++)
	{
		e = loadmodel->surfedges[s->firstedge+i];
		if (e >= 0)
			v = &loadmodel->vertexes[loadmodel->edges[e].v[0]];
		else
			v = &loadmodel->vertexes[loadmodel->edges[-e].v[1]];

		for (j=0 ; j<2 ; j++)
		{
			val = v->position[0] * tex->vecs[j][0] +
				v->position[1] * tex->vecs[j][1] +
				v->position[2] * tex->vecs[j][2] +
				tex->vecs[j][3];
			if (val < mins[j])
				mins[j] = val;
			if (val > maxs[j])
				maxs[j] = val;
		}
	}

	for (i=0 ; i<2 ; i++)
	{
		bmins[i] = floor(mins[i]/16);
		bmaxs[i] = ceil(maxs[i]/16);

		s->texturemins[i] = bmins[i] * 16;
		s->extents[i] = (bmaxs[i] - bmins[i]) * 16;

//		if ( !(tex->flags & TEX_SPECIAL) && s->extents[i] > 512 )// 256 )
//			Sys_Error ("Bad surface extents");
	}
}*/

/*
=================
Mod_LoadFaces
=================
*/
#ifndef SERVERONLY
static qboolean CModQ2_LoadFaces (model_t *mod, qbyte *mod_base, lump_t *l, qboolean lightofsisdouble)
{
	dsface_t		*in;
	msurface_t 	*out;
	int			i, count, surfnum;
	int			planenum, side;
	int			ti;

	unsigned short lmshift, lmscale;
	char buf[64];

	lmscale = atoi(Mod_ParseWorldspawnKey(mod, "lightmap_scale", buf, sizeof(buf)));
	if (!lmscale)
		lmshift = LMSHIFT_DEFAULT;
	else
	{
		for(lmshift = 0; lmscale > 1; lmshift++)
			lmscale >>= 1;
	}

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
	{
		Con_Printf ("MOD_LoadBmodel: funny lump size in %s\n",mod->name);
		return false;
	}
	count = l->filelen / sizeof(*in);
	out = ZG_Malloc(&mod->memgroup, (count+6)*sizeof(*out));	//spare for skybox

	mod->surfaces = out;
	mod->numsurfaces = count;

	for ( surfnum=0 ; surfnum<count ; surfnum++, in++, out++)
	{
		out->firstedge = LittleLong(in->firstedge);
		out->numedges = (unsigned short)LittleShort(in->numedges);
		out->flags = 0;

		planenum = (unsigned short)LittleShort(in->planenum);
		side = (unsigned short)LittleShort(in->side);
		if (side)
			out->flags |= SURF_PLANEBACK;

		out->plane = mod->planes + planenum;

		ti = (unsigned short)LittleShort (in->texinfo);
		if (ti < 0 || ti >= mod->numtexinfo)
		{
			Con_Printf (CON_ERROR "MOD_LoadBmodel: bad texinfo number\n");
			return false;
		}
		out->texinfo = mod->texinfo + ti;

#ifndef SERVERONLY
		if (out->texinfo->flags & TI_SKY)
		{
			out->flags |= SURF_DRAWSKY;
		}
		if (out->texinfo->flags & TI_WARP)
		{
			out->flags |= SURF_DRAWTURB|SURF_DRAWTILED;
		}
#endif

		out->lmshift = lmshift;
		CalcSurfaceExtents (mod, out);

	// lighting info

		for (i=0 ; i<MAXQ1LIGHTMAPS ; i++)
			out->styles[i] = in->styles[i];
		i = LittleLong(in->lightofs);
		if (i == -1)
			out->samples = NULL;
		else if (lightofsisdouble)
			out->samples = mod->lightdata + (i/2);
		else
			out->samples = mod->lightdata + i;

	// set the drawing flags

		if (out->texinfo->flags & TI_WARP)
		{
			out->flags |= SURF_DRAWTURB;
			for (i=0 ; i<2 ; i++)
			{
				out->extents[i] = 16384;
				out->texturemins[i] = -8192;
			}
		}

	}

	return true;
}
#endif

/*
=================
CMod_LoadNodes

=================
*/
static qboolean CModQ2_LoadNodes (model_t *mod, qbyte *mod_base, lump_t *l)
{
	q2dnode_t		*in;
	int			child;
	mnode_t		*out;
	int			i, j, count;

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
	{
		Con_Printf (CON_ERROR "MOD_LoadBmodel: funny lump size\n");
		return false;
	}
	count = l->filelen / sizeof(*in);

	if (count < 1)
	{
		Con_Printf (CON_ERROR "Map has no nodes\n");
		return false;
	}
	if (count > SANITY_MAX_MAP_NODES)
	{
		Con_Printf (CON_ERROR "Map has too many nodes\n");
		return false;
	}

	out = ZG_Malloc(&mod->memgroup, sizeof(mnode_t)*count);

	mod->nodes = out;
	mod->numnodes = count;

	for (i=0 ; i<count ; i++, out++, in++)
	{
		memset(out, 0, sizeof(*out));

		for (j=0 ; j<3 ; j++)
		{
			out->minmaxs[j] = LittleShort (in->mins[j]);
			out->minmaxs[3+j] = LittleShort (in->maxs[j]);
		}

		out->plane = mod->planes + LittleLong(in->planenum);

		out->firstsurface = (unsigned short)LittleShort (in->firstface);
		out->numsurfaces = (unsigned short)LittleShort (in->numfaces);
		out->contents = -1;	// differentiate from leafs

		for (j=0 ; j<2 ; j++)
		{
			child = LittleLong (in->children[j]);
			out->childnum[j] = child;
			if (child < 0)
				out->children[j] = (mnode_t *)(mod->leafs + -1-child);
			else
				out->children[j] = mod->nodes + child;
		}
	}

	CMod_SetParent (mod->nodes, NULL);	// sets nodes and leafs

	return true;
}

/*
=================
CMod_LoadBrushes

=================
*/
static qboolean CModQ2_LoadBrushes (model_t *mod, qbyte *mod_base, lump_t *l)
{
	cminfo_t	*prv = (cminfo_t*)mod->meshinfo;
	q2dbrush_t	*in;
	q2cbrush_t	*out;
	int			i, count;

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
	{
		Con_Printf (CON_ERROR "MOD_LoadBmodel: funny lump size\n");
		return false;
	}
	count = l->filelen / sizeof(*in);

	if (count > SANITY_MAX_MAP_BRUSHES)
	{
		Con_Printf (CON_ERROR "Map has too many brushes");
		return false;
	}

	prv->brushes = ZG_Malloc(&mod->memgroup, sizeof(*out) * (count+1));

	out = prv->brushes;

	prv->numbrushes = count;

	for (i=0 ; i<count ; i++, out++, in++)
	{
		//FIXME: missing bounds checks
		out->brushside = &prv->brushsides[LittleLong(in->firstside)];
		out->numsides = LittleLong(in->numsides);
		out->contents = LittleLong(in->contents);
		CM_FinalizeBrush(out);
	}

	return true;
}

/*
=================
CMod_LoadLeafs
=================
*/
static qboolean CModQ2_LoadLeafs (model_t *mod, qbyte *mod_base, lump_t *l)
{
	int			i, j;
	mleaf_t		*out;
	q2dleaf_t 	*in;
	int			count;

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
	{
		Con_Printf (CON_ERROR "MOD_LoadBmodel: funny lump size\n");
		return false;
	}
	count = l->filelen / sizeof(*in);

	if (count < 1)
	{
		Con_Printf (CON_ERROR "Map with no leafs\n");
		return false;
	}
	// need to save space for box planes
	if (count > SANITY_MAX_MAP_LEAFS)
	{
		Con_Printf (CON_ERROR "Map has too many leafs\n");
		return false;
	}

	out = ZG_Malloc(&mod->memgroup, sizeof(*out) * (count+1));
	mod->numclusters = 0;

	mod->leafs = out;
	mod->numleafs = count;

	for ( i=0 ; i<count ; i++, in++, out++)
	{
		memset(out, 0, sizeof(*out));

		for (j=0 ; j<3 ; j++)
		{
			out->minmaxs[j] = LittleShort (in->mins[j]);
			out->minmaxs[3+j] = LittleShort (in->maxs[j]);
		}

		out->contents = LittleLong (in->contents);
		out->cluster = (unsigned short)LittleShort (in->cluster);
		if (out->cluster == 0xffff)
			out->cluster = -1;

		out->area = (unsigned short)LittleShort (in->area);
		out->firstleafbrush = (unsigned short)LittleShort (in->firstleafbrush);
		out->numleafbrushes = (unsigned short)LittleShort (in->numleafbrushes);

		out->firstmarksurface = mod->marksurfaces +
			(unsigned short)LittleShort(in->firstleafface);
		out->nummarksurfaces = (unsigned short)LittleShort(in->numleaffaces);

		if (out->cluster >= mod->numclusters)
			mod->numclusters = out->cluster + 1;
	}
	out = mod->leafs;
	mod->pvsbytes = ((mod->numclusters + 31)>>3)&~3;

	if (out[0].contents != Q2CONTENTS_SOLID)
	{
		Con_Printf (CON_ERROR "Map leaf 0 is not CONTENTS_SOLID\n");
		return false;
	}

	return true;
}

/*
=================
CMod_LoadPlanes
=================
*/
static qboolean CModQ2_LoadPlanes (model_t *mod, qbyte *mod_base, lump_t *l)
{
	int			i, j;
	mplane_t	*out;
	dplane_t 	*in;
	int			count;
	int			bits;

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
	{
		Con_Printf (CON_ERROR "MOD_LoadBmodel: funny lump size\n");
		return false;
	}
	count = l->filelen / sizeof(*in);

	if (count < 1)
	{
		Con_Printf (CON_ERROR "Map with no planes\n");
		return false;
	}
	// need to save space for box planes
	if (count >= SANITY_MAX_MAP_PLANES)
	{
		Con_Printf (CON_ERROR "Map has too many planes (%i)\n", count);
		return false;
	}

	mod->planes = out = ZG_Malloc(&mod->memgroup, sizeof(*out) * count);
	mod->numplanes = count;

	for ( i=0 ; i<count ; i++, in++, out++)
	{
		bits = 0;
		for (j=0 ; j<3 ; j++)
		{
			out->normal[j] = LittleFloat (in->normal[j]);
			if (out->normal[j] < 0)
				bits |= 1<<j;
		}

		out->dist = LittleFloat (in->dist);
		out->type = LittleLong (in->type);
		out->signbits = bits;
	}

	return true;
}

/*
=================
CMod_LoadLeafBrushes
=================
*/
static qboolean CModQ2_LoadLeafBrushes (model_t *mod, qbyte *mod_base, lump_t *l)
{
	cminfo_t	*prv = (cminfo_t*)mod->meshinfo;
	int			i;
	q2cbrush_t	**out;
	unsigned short 	*in;
	int			count;

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
	{
		Con_Printf (CON_ERROR "MOD_LoadBmodel: funny lump size\n");
		return false;
	}
	count = l->filelen / sizeof(*in);

	if (count < 1)
	{
		Con_Printf (CON_ERROR "Map with no planes\n");
		return false;
	}
	// need to save space for box planes
	if (count > MAX_Q2MAP_LEAFBRUSHES)
	{
		Con_Printf (CON_ERROR "Map has too many leafbrushes\n");
		return false;
	}

	out = prv->leafbrushes;
	prv->numleafbrushes = count;

	for ( i=0 ; i<count ; i++, in++, out++)
		*out = prv->brushes + (unsigned short)(short)LittleShort (*in);

	return true;
}

/*
=================
CMod_LoadBrushSides
=================
*/
static qboolean CModQ2_LoadBrushSides (model_t *mod, qbyte *mod_base, lump_t *l)
{
	cminfo_t	*prv = (cminfo_t*)mod->meshinfo;
	unsigned int			i, j;
	q2cbrushside_t	*out;
	q2dbrushside_t 	*in;
	int			count;
	int			num;

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
	{
		Con_Printf (CON_ERROR "MOD_LoadBmodel: funny lump size\n");
		return false;
	}
	count = l->filelen / sizeof(*in);

	// need to save space for box planes
	if (count > SANITY_MAX_MAP_BRUSHSIDES)
	{
		Con_Printf (CON_ERROR "Map has too many brushsides (%i)\n", count);
		return false;
	}

	out = prv->brushsides = ZG_Malloc(&mod->memgroup, sizeof(*out) * count);
	prv->numbrushsides = count;

	for ( i=0 ; i<count ; i++, in++, out++)
	{
		num = (unsigned short)LittleShort (in->planenum);
		out->plane = &mod->planes[num];
		j = (unsigned short)LittleShort (in->texinfo);
		if (j >= mod->numtexinfo)
			out->surface = &nullsurface;
		else
			out->surface = &prv->surfaces[j];
	}

	return true;
}

/*
=================
CMod_LoadAreas
=================
*/
static qboolean CModQ2_LoadAreas (model_t *mod, qbyte *mod_base, lump_t *l)
{
	cminfo_t	*prv = (cminfo_t*)mod->meshinfo;
	int			i;
	q2carea_t		*out;
	q2darea_t 	*in;
	int			count;

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
	{
		Con_Printf (CON_ERROR "MOD_LoadBmodel: funny lump size\n");
		return false;
	}
	count = l->filelen / sizeof(*in);

	if (count > MAX_Q2MAP_AREAS)
	{
		Con_Printf (CON_ERROR "Map has too many areas\n");
		return false;
	}

	out = prv->q2areas;
	prv->numareas = count;

	for ( i=0 ; i<count ; i++, in++, out++)
	{
		out->numareaportals = LittleLong (in->numareaportals);
		out->firstareaportal = LittleLong (in->firstareaportal);
		out->floodvalid = 0;
		out->floodnum = 0;
	}

	return true;
}

/*
=================
CMod_LoadAreaPortals
=================
*/
static qboolean CModQ2_LoadAreaPortals (model_t *mod, qbyte *mod_base, lump_t *l)
{
	cminfo_t	*prv = (cminfo_t*)mod->meshinfo;
	int			i;
	q2dareaportal_t		*out;
	q2dareaportal_t 	*in;
	int			count;

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
	{
		Con_Printf (CON_ERROR "MOD_LoadBmodel: funny lump size\n");
		return false;
	}
	count = l->filelen / sizeof(*in);

	if (count > MAX_Q2MAP_AREAS)
	{
		Con_Printf (CON_ERROR "Map has too many areas\n");
		return false;
	}

	out = prv->areaportals;
	prv->numareaportals = count;

	for ( i=0 ; i<count ; i++, in++, out++)
	{
		out->portalnum = LittleLong (in->portalnum);
		out->otherarea = LittleLong (in->otherarea);
	}

	return true;
}

/*
=================
CMod_LoadVisibility
=================
*/
static qboolean CModQ2_LoadVisibility (model_t *mod, qbyte *mod_base, lump_t *l)
{
	cminfo_t	*prv = (cminfo_t*)mod->meshinfo;
	int		i;

	prv->numvisibility = l->filelen;
//	if (l->filelen > MAX_Q2MAP_VISIBILITY)
//	{
//		Con_Printf (CON_ERROR "Map has too large visibility lump\n");
//		return false;
//	}

	prv->q2vis = ZG_Malloc(&mod->memgroup, l->filelen);
	memcpy (prv->q2vis, mod_base + l->fileofs, l->filelen);

	mod->vis = prv->q2vis;

	prv->q2vis->numclusters = LittleLong (prv->q2vis->numclusters);
	for (i=0 ; i<prv->q2vis->numclusters ; i++)
	{
		prv->q2vis->bitofs[i][0] = LittleLong (prv->q2vis->bitofs[i][0]);
		prv->q2vis->bitofs[i][1] = LittleLong (prv->q2vis->bitofs[i][1]);
	}
	mod->numclusters = prv->q2vis->numclusters;
	mod->pvsbytes = ((mod->numclusters + 31)>>3)&~3;

	return true;
}
#endif	//q2bsps

#ifdef Q3BSPS
static qboolean CModQ3_LoadMarksurfaces (model_t *loadmodel, qbyte *mod_base, lump_t *l)
{
	int		i, j, count;
	int		*in;
	msurface_t **out;

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
	{
		Con_Printf (CON_ERROR "CModQ3_LoadMarksurfaces: funny lump size in %s\n",loadmodel->name);
		return false;
	}
	count = l->filelen / sizeof(*in);
	out = ZG_Malloc(&loadmodel->memgroup, count*sizeof(*out));

	loadmodel->marksurfaces = out;
	loadmodel->nummarksurfaces = count;

	for ( i=0 ; i<count ; i++)
	{
		j = LittleLong(in[i]);
		if (j < 0 || j >= loadmodel->numsurfaces)
		{
			Con_Printf (CON_ERROR "Mod_ParseMarksurfaces: bad surface number\n");
			return false;
		}
		out[i] = loadmodel->surfaces + j;
	}

	return true;
}

static qboolean CModQ3_LoadSubmodels (model_t *mod, qbyte *mod_base, lump_t *l)
{
	cminfo_t	*prv = (cminfo_t*)mod->meshinfo;
	q3dmodel_t	*in;
	cmodel_t	*out;
	int			i, j, count;
	q2cbrush_t **leafbrush;
	mleaf_t		*bleaf;

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
	{
		Con_Printf (CON_ERROR "MOD_LoadBmodel: funny lump size\n");
		return false;
	}
	count = l->filelen / sizeof(*in);

	if (count < 1)
	{
		Con_Printf (CON_ERROR "Map with no models\n");
		return false;
	}
	if (count > SANITY_MAX_Q2MAP_MODELS)
	{
		Con_Printf (CON_ERROR "Map has too many models\n");
		return false;
	}

	out = prv->cmodels = ZG_Malloc(&mod->memgroup, count * sizeof(*prv->cmodels));
	prv->numcmodels = count;

	if (count > 1)
		bleaf = ZG_Malloc(&mod->memgroup, (count-1) * sizeof(*bleaf));
	else
		bleaf = NULL;

	prv->mapisq3 = true;

	for (i=0 ; i<count ; i++, in++, out++)
	{
		for (j=0 ; j<3 ; j++)
		{	// spread the mins / maxs by a pixel
			out->mins[j] = LittleFloat (in->mins[j]) - 1;
			out->maxs[j] = LittleFloat (in->maxs[j]) + 1;
			out->origin[j] = (out->maxs[j] + out->mins[j])/2;
		}
		out->firstsurface = LittleLong (in->firstsurface);
		out->numsurfaces = LittleLong (in->num_surfaces);
		if (!i)
		{
			out->headnode = mod->nodes;
			out->headleaf = NULL;
		}
		else
		{
//create a new leaf to hold the brushes and be directly clipped
			out->headleaf = bleaf;
			out->headnode = NULL;

//			out->firstbrush = LittleLong(in->firstbrush);
//			out->num_brushes = LittleLong(in->num_brushes);

			bleaf->numleafbrushes = LittleLong ( in->num_brushes );
			bleaf->firstleafbrush = prv->numleafbrushes;
			bleaf->contents = 0;

			leafbrush = &prv->leafbrushes[prv->numleafbrushes];
			for ( j = 0; j < bleaf->numleafbrushes; j++, leafbrush++ )
			{
				*leafbrush = prv->brushes + LittleLong ( in->firstbrush ) + j;
				bleaf->contents |= (*leafbrush)->contents;
			}
			prv->numleafbrushes += bleaf->numleafbrushes;
			bleaf++;
		}
		//submodels
	}

	AddPointToBounds(prv->cmodels[0].mins, mod->mins, mod->maxs);
	AddPointToBounds(prv->cmodels[0].maxs, mod->mins, mod->maxs);

	return true;
}

static qboolean CModQ3_LoadShaders (model_t *mod, qbyte *mod_base, lump_t *l)
{
	cminfo_t	*prv = (cminfo_t*)mod->meshinfo;
	dq3shader_t	*in;
	q2mapsurface_t	*out;
	int				i, count;
	texture_t *tex;

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
	{
		Con_Printf (CON_ERROR "MOD_LoadBmodel: funny lump size\n");
		return false;
	}
	count = l->filelen / sizeof(*in);

	if (count < 1)
	{
		Con_Printf (CON_ERROR "Map with no shaders\n");
		return false;
	}
//	else if (count > MAX_Q2MAP_TEXINFO)
//		Host_Error ("Map has too many shaders");

	mod->numtexinfo = count;
	out = prv->surfaces = ZG_Malloc(&mod->memgroup, count*sizeof(*out));

	mod->textures = ZG_Malloc(&mod->memgroup, (sizeof(texture_t*)+sizeof(mtexinfo_t)+sizeof(texture_t))*(count*2+1));	//+1 is 'noshader' for flares.
	mod->texinfo = (mtexinfo_t*)(mod->textures+(count*2+1));
	tex = (texture_t*)(mod->texinfo+(count*2+1));
	mod->numtextures = count*2+1;

	for ( i=0 ; i<count ; i++, in++, out++ )
	{
		out->c.flags = LittleLong ( in->surfflags );
		out->c.value = LittleLong ( in->contents );

		mod->texinfo[i].texture = tex+i;
		mod->texinfo[i].flags = prv->surfaces[i].c.flags;
		Q_strncpyz(mod->texinfo[i].texture->name, in->shadername, sizeof(mod->texinfo[i].texture->name));
		mod->textures[i] = mod->texinfo[i].texture;
	}
	for ( i=0, in-=count ; i<count ; i++, in++ )
	{
		mod->texinfo[i+count].texture = tex+i+count;
		mod->texinfo[i+count].flags = prv->surfaces[i].c.flags;
		Q_strncpyz(mod->texinfo[i+count].texture->name, in->shadername, sizeof(mod->texinfo[i+count].texture->name));
		mod->textures[i+count] = mod->texinfo[i+count].texture;
	}

	//and for flares, which are not supported at this time.
	mod->texinfo[count*2].texture = tex+count*2;
	mod->texinfo[i+count].flags = 0;
	Q_strncpyz(mod->texinfo[count*2].texture->name, "noshader", sizeof(mod->texinfo[count*2].texture->name));
	mod->textures[count*2] = mod->texinfo[count*2].texture;

	return true;
}

static qboolean CModQ3_LoadVertexes (model_t *mod, qbyte *mod_base, lump_t *l)
{
	cminfo_t	*prv = (cminfo_t*)mod->meshinfo;
	q3dvertex_t	*in;
	vecV_t		*out;
	vec3_t		*nout;
	//, *sout, *tout;
	int			i, count, j;
	vec2_t		*lmout, *stout;
	vec4_t *cout;
	extern cvar_t gl_overbright;
	extern qbyte lmgamma[256];

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
	{
		Con_Printf (CON_ERROR "CMOD_LoadVertexes: funny lump size\n");
		return false;
	}
	count = l->filelen / sizeof(*in);

	if (count > MAX_Q3MAP_VERTEXES)
	{
		Con_Printf (CON_ERROR "Map has too many vertexes\n");
		return false;
	}

	BuildLightMapGammaTable(1, 1<<(2-gl_overbright.ival));

	out = ZG_Malloc(&mod->memgroup, count*sizeof(*out));
	stout = ZG_Malloc(&mod->memgroup, count*sizeof(*stout));
	lmout = ZG_Malloc(&mod->memgroup, count*sizeof(*lmout));
	cout = ZG_Malloc(&mod->memgroup, count*sizeof(*cout));
	nout = ZG_Malloc(&mod->memgroup, count*sizeof(*nout));
//	sout = ZG_Malloc(&mod->memgroup, count*sizeof(*nout));
//	tout = ZG_Malloc(&mod->memgroup, count*sizeof(*nout));
	prv->verts = out;
	prv->vertstmexcoords = stout;
	for (i = 0; i < MAXRLIGHTMAPS; i++)
	{
		prv->vertlstmexcoords[i] = lmout;
		prv->colors4f_array[i] = cout;
	}
	prv->normals_array = nout;
//	prv->svector_array = sout;
//	prv->tvector_array = tout;
	prv->numvertexes = count;

	for ( i=0 ; i<count ; i++, in++)
	{
		for ( j=0 ; j < 3 ; j++)
		{
			out[i][j] = LittleFloat ( in->point[j] );
			nout[i][j] = LittleFloat (in->normal[j]);
		}
		for ( j=0 ; j < 2 ; j++)
		{
			stout[i][j] = LittleFloat ( ((float *)in->texcoords)[j] );
			lmout[i][j] = LittleFloat ( ((float *)in->texcoords)[j+2] );
		}
		cout[i][0] = (lmgamma[in->color[0]]<<gl_overbright.ival)/255.0f;
		cout[i][1] = (lmgamma[in->color[1]]<<gl_overbright.ival)/255.0f;
		cout[i][2] = (lmgamma[in->color[2]]<<gl_overbright.ival)/255.0f;
		cout[i][3] = in->color[3]/255.0f;
	}

//	if (r_lightmap_saturation.value != 1.0f)
//		SaturateR8G8B8(cout, count*4, r_lightmap_saturation.value);

	return true;
}

#ifdef RFBSPS
static qboolean CModRBSP_LoadVertexes (model_t *mod, qbyte *mod_base, lump_t *l)
{
	cminfo_t	*prv = (cminfo_t*)mod->meshinfo;
	rbspvertex_t	*in;
	vecV_t		*out;
	vec3_t		*nout;
	//, *sout, *tout;
	int			i, count, j;
	vec2_t		*lmout, *stout;
	vec4_t *cout;
	int sty;
	extern qbyte lmgamma[256];

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
	{
		Con_Printf (CON_ERROR "CMOD_LoadVertexes: funny lump size\n");
		return false;
	}
	count = l->filelen / sizeof(*in);

	if (count > MAX_Q3MAP_VERTEXES)
	{
		Con_Printf (CON_ERROR "Map has too many vertexes\n");
		return false;
	}

	out = ZG_Malloc(&mod->memgroup, count*sizeof(*out));
	stout = ZG_Malloc(&mod->memgroup, count*sizeof(*stout));
	lmout = ZG_Malloc(&mod->memgroup, MAXRLIGHTMAPS*count*sizeof(*lmout));
	cout = ZG_Malloc(&mod->memgroup, MAXRLIGHTMAPS*count*sizeof(*cout));
	nout = ZG_Malloc(&mod->memgroup, count*sizeof(*nout));
//	sout = ZG_Malloc(&mod->memgroup, count*sizeof(*sout));
//	tout = ZG_Malloc(&mod->memgroup, count*sizeof(*tout));
	prv->verts = out;
	prv->vertstmexcoords = stout;
	for (sty = 0; sty < MAXRLIGHTMAPS; sty++)
	{
		prv->vertlstmexcoords[sty] = lmout + sty*count;
		prv->colors4f_array[sty] = cout + sty*count;
	}
	prv->normals_array = nout;
//	prv->svector_array = sout;
//	prv->tvector_array = tout;
	prv->numvertexes = count;

	for ( i=0 ; i<count ; i++, in++)
	{
		for ( j=0 ; j < 3 ; j++)
		{
			out[i][j] = LittleFloat ( in->point[j] );
			nout[i][j] = LittleFloat (in->normal[j]);
		}
		for ( j=0 ; j < 2 ; j++)
		{
			stout[i][j] = LittleFloat ( ((float *)in->texcoords)[j] );
			for (sty = 0; sty < MAXRLIGHTMAPS; sty++)
				prv->vertlstmexcoords[sty][i][j] = LittleFloat ( ((float *)in->texcoords)[j+2*(sty+1)] );
		}
		for (sty = 0; sty < MAXRLIGHTMAPS; sty++)
		{
			prv->colors4f_array[sty][i][0] = lmgamma[in->color[sty][0]]/255.0f;
			prv->colors4f_array[sty][i][1] = lmgamma[in->color[sty][1]]/255.0f;
			prv->colors4f_array[sty][i][2] = lmgamma[in->color[sty][2]]/255.0f;
			prv->colors4f_array[sty][i][3] = in->color[sty][3]/255.0f;
		}
	}

	return true;
}
#endif

#ifndef SERVERONLY
static qboolean CModQ3_LoadIndexes (model_t *loadmodel, qbyte *mod_base, lump_t *l)
{
	cminfo_t	*prv = (cminfo_t*)loadmodel->meshinfo;
	int		i, count;
	int		*in;
	index_t	*out;

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
	{
		Con_Printf (CON_ERROR "MOD_LoadBmodel: funny lump size in %s\n", loadmodel->name);
		return false;
	}
	count = l->filelen / sizeof(*in);
	if (count < 1 || count >= MAX_Q3MAP_INDICES)
	{
		Con_Printf (CON_ERROR "MOD_LoadBmodel: too many indicies in %s: %i\n",
					loadmodel->name, count);
		return false;
	}

	out = ZG_Malloc(&loadmodel->memgroup, count*sizeof(*out));

	prv->surfindexes = out;
//	prv->numsurfindexes = count;

	for ( i=0 ; i<count ; i++)
		out[i] = LittleLong (in[i]);

	return true;
}
#endif

/*
=================
CMod_LoadFaces
=================
*/
static qboolean CModQ3_LoadFaces (model_t *mod, qbyte *mod_base, lump_t *l)
{
	cminfo_t	*prv = (cminfo_t*)mod->meshinfo;
	q3dface_t		*in;
	q3cface_t		*out;
	int			i, count;

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
	{
		Con_Printf (CON_ERROR "MOD_LoadBmodel: funny lump size\n");
		return false;
	}
	count = l->filelen / sizeof(*in);

	if (count > SANITY_MAX_MAP_FACES)
	{
		Con_Printf (CON_ERROR "Map has too many faces\n");
		return false;
	}

	out = BZ_Malloc ( count*sizeof(*out) );
	prv->faces = out;
	prv->numfaces = count;

	for ( i=0 ; i<count ; i++, in++, out++)
	{
		out->facetype = LittleLong ( in->facetype );
		out->shadernum = LittleLong ( in->shadernum );

		out->numverts = LittleLong ( in->num_vertices );
		out->firstvert = LittleLong ( in->firstvertex );

		if (out->facetype == MST_PATCH)
		{
			out->patch.cp[0] = LittleLong ( in->patchwidth );
			out->patch.cp[1] = LittleLong ( in->patchheight );
		}
		else
		{
			out->soup.firstindex = LittleLong(in->firstindex);
			out->soup.numindicies = LittleLong(in->num_indexes);
		}
	}

	mod->numsurfaces = i;

	return true;
}

#ifdef RFBSPS
static qboolean CModRBSP_LoadFaces (model_t *mod, qbyte *mod_base, lump_t *l)
{
	cminfo_t	*prv = (cminfo_t*)mod->meshinfo;
	rbspface_t		*in;
	q3cface_t		*out;
	int			i, count;

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
	{
		Con_Printf (CON_ERROR "MOD_LoadBmodel: funny lump size\n");
		return false;
	}
	count = l->filelen / sizeof(*in);

	if (count > SANITY_MAX_MAP_FACES)
	{
		Con_Printf (CON_ERROR "Map has too many faces\n");
		return false;
	}

	out = BZ_Malloc ( count*sizeof(*out) );
	prv->faces = out;
	prv->numfaces = count;

	for ( i=0 ; i<count ; i++, in++, out++)
	{
		out->facetype = LittleLong ( in->facetype );
		out->shadernum = LittleLong ( in->shadernum );

		out->numverts = LittleLong ( in->num_vertices );
		out->firstvert = LittleLong ( in->firstvertex );

		if (out->facetype == MST_PATCH)
		{
			out->patch.cp[0] = LittleLong ( in->patchwidth );
			out->patch.cp[1] = LittleLong ( in->patchheight );
		}
		else
		{
			out->soup.firstindex = LittleLong(in->firstindex);
			out->soup.numindicies = LittleLong(in->num_indexes);
		}
	}

	mod->numsurfaces = i;
	return true;
}
#endif

#ifndef SERVERONLY

/*
=================
Mod_LoadFogs
=================
*/
static qboolean CModQ3_LoadFogs (model_t *mod, qbyte *mod_base, lump_t *l)
{
	cminfo_t	*prv = (cminfo_t*)mod->meshinfo;
	dfog_t 	*in;
	mfog_t 	*out;
	q2cbrush_t *brush;
	q2cbrushside_t *visibleside, *brushsides;
	int		i, j, count;

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
	{
		Con_Printf (CON_ERROR "MOD_LoadBmodel: funny lump size in %s\n", mod->name);
		return false;
	}
	count = l->filelen / sizeof(*in);
	out = ZG_Malloc(&mod->memgroup, count*sizeof(*out));

	mod->fogs = out;
	mod->numfogs = count;

	for ( i=0 ; i<count ; i++, in++, out++)
	{
		if ( LittleLong ( in->visibleSide ) == -1 )
		{
			continue;
		}

		brush = prv->brushes + LittleLong ( in->brushNum );
		brushsides = brush->brushside;
		visibleside = brushsides + LittleLong ( in->visibleSide );

		out->visibleplane = visibleside->plane;
		Q_strncpyz(out->shadername, in->shader, sizeof(out->shadername));
		out->numplanes = brush->numsides;
		out->planes = ZG_Malloc(&mod->memgroup, out->numplanes*sizeof(cplane_t *));

		for ( j = 0; j < out->numplanes; j++ )
		{
			out->planes[j] = brushsides[j].plane;
		}
	}

	return true;
}

image_t *Mod_CubemapForOrigin(model_t *wmodel, vec3_t org)
{
	int i;
	menvmap_t 	*e;
	float bestdist = FLT_MAX, dist;
	image_t *ret = NULL;
	vec3_t move;
	if (!wmodel || wmodel->loadstate != MLS_LOADED)
		return NULL;
	for ( i=0 , e=wmodel->envmaps ; i<wmodel->numenvmaps ; i++, e++)
	{
		VectorSubtract(org, e->origin, move);
		dist = DotProduct(move,move);
		if (bestdist > dist)
		{
			bestdist = dist;
			ret = e->image;
		}
	}
	return ret;
}

mfog_t *Mod_FogForOrigin(model_t *wmodel, vec3_t org)
{
	int i, j;
	mfog_t 	*ret;
	float dot;

	if (!wmodel || wmodel->loadstate != MLS_LOADED)
		return NULL;

	for ( i=0 , ret=wmodel->fogs ; i<wmodel->numfogs ; i++, ret++)
	{
		if (!ret->shader)
			continue;
		for (j = 0; j < ret->numplanes; j++)
		{
			dot = DotProduct(ret->planes[j]->normal, org);
			if (dot - ret->planes[j]->dist > 0)
				break;
		}
		if (j == ret->numplanes)
		{
			return ret;
		}
	}

	return NULL;
}

//Convert a patch in to a list of glpolys

#define MAX_ARRAY_VERTS 65535

static index_t tempIndexesArray[MAX_ARRAY_VERTS*6];

static void GL_SizePatch(mesh_t *mesh, int patchwidth, int patchheight, int numverts, int firstvert, cminfo_t *prv)
{
	int patch_cp[2], step[2], size[2], flat[2];
	float subdivlevel;

	patch_cp[0] = patchwidth;
	patch_cp[1] = patchheight;

	if (patch_cp[0] <= 0 || patch_cp[1] <= 0 )
	{
		mesh->numindexes = 0;
		mesh->numvertexes = 0;
		return;
	}

	subdivlevel = r_subdivisions.value;
	if ( subdivlevel < 1 )
		subdivlevel = 1;

// find the degree of subdivision in the u and v directions
	Patch_GetFlatness ( subdivlevel, prv->verts[firstvert], sizeof(vecV_t)/sizeof(vec_t), patch_cp, flat );

// allocate space for mesh
	step[0] = (1 << flat[0]);
	step[1] = (1 << flat[1]);
	size[0] = (patch_cp[0] / 2) * step[0] + 1;
	size[1] = (patch_cp[1] / 2) * step[1] + 1;

	mesh->numvertexes = size[0] * size[1];
	mesh->numindexes = (size[0]-1) * (size[1]-1) * 6;
}

//mesh_t *GL_CreateMeshForPatch ( model_t *mod, q3dface_t *surf )
static void GL_CreateMeshForPatch (model_t *mod, mesh_t *mesh, int patchwidth, int patchheight, int numverts, int firstvert)
{
	cminfo_t	*prv = (cminfo_t*)mod->meshinfo;
	int numindexes, patch_cp[2], step[2], size[2], flat[2], i, u, v, p;
	index_t	*indexes;
	float subdivlevel;
	int sty;

	patch_cp[0] = patchwidth;
	patch_cp[1] = patchheight;

	if (patch_cp[0] <= 0 || patch_cp[1] <= 0 )
	{
		mesh->numindexes = 0;
		mesh->numvertexes = 0;
		return;
	}

	subdivlevel = r_subdivisions.value;
	if ( subdivlevel < 1 )
		subdivlevel = 1;

// find the degree of subdivision in the u and v directions
	Patch_GetFlatness ( subdivlevel, prv->verts[firstvert], sizeof(vecV_t)/sizeof(vec_t), patch_cp, flat );

// allocate space for mesh
	step[0] = (1 << flat[0]);
	step[1] = (1 << flat[1]);
	size[0] = (patch_cp[0] / 2) * step[0] + 1;
	size[1] = (patch_cp[1] / 2) * step[1] + 1;
	numverts = size[0] * size[1];

	if ( numverts < 0 || numverts > MAX_ARRAY_VERTS )
	{
		mesh->numindexes = 0;
		mesh->numvertexes = 0;
		return;
	}


	if (mesh->numvertexes != numverts)
	{
		mesh->numindexes = 0;
		mesh->numvertexes = 0;
		return;
	}

// fill in

	Patch_Evaluate ( prv->verts[firstvert], patch_cp, step, mesh->xyz_array[0], sizeof(vecV_t)/sizeof(vec_t));
	for (sty = 0; sty < MAXRLIGHTMAPS; sty++)
	{
		if (mesh->colors4f_array[sty])
			Patch_Evaluate ( prv->colors4f_array[sty][firstvert], patch_cp, step, mesh->colors4f_array[sty][0], 4 );
	}
	Patch_Evaluate ( prv->normals_array[firstvert], patch_cp, step, mesh->normals_array[0], 3 );
	Patch_Evaluate ( prv->vertstmexcoords[firstvert], patch_cp, step, mesh->st_array[0], 2 );
	for (sty = 0; sty < MAXRLIGHTMAPS; sty++)
	{
		if (mesh->lmst_array[sty])
			Patch_Evaluate ( prv->vertlstmexcoords[sty][firstvert], patch_cp, step, mesh->lmst_array[sty][0], 2 );
	}

// compute new indexes avoiding adding invalid triangles
	numindexes = 0;
	indexes = tempIndexesArray;
	for (v = 0, i = 0; v < size[1]-1; v++)
	{
		for (u = 0; u < size[0]-1; u++, i += 6)
		{
			indexes[0] = p = v * size[0] + u;
			indexes[1] = p + size[0];
			indexes[2] = p + 1;

//			if ( !VectorEquals(mesh->xyz_array[indexes[0]], mesh->xyz_array[indexes[1]]) &&
//				!VectorEquals(mesh->xyz_array[indexes[0]], mesh->xyz_array[indexes[2]]) &&
//				!VectorEquals(mesh->xyz_array[indexes[1]], mesh->xyz_array[indexes[2]]) )
			{
				indexes += 3;
				numindexes += 3;
			}

			indexes[0] = p + 1;
			indexes[1] = p + size[0];
			indexes[2] = p + size[0] + 1;

//			if ( !VectorEquals(mesh->xyz_array[indexes[0]], mesh->xyz_array[indexes[1]]) &&
//				!VectorEquals(mesh->xyz_array[indexes[0]], mesh->xyz_array[indexes[2]]) &&
//				!VectorEquals(mesh->xyz_array[indexes[1]], mesh->xyz_array[indexes[2]]) )
			{
				indexes += 3;
				numindexes += 3;
			}
		}
	}

// allocate and fill index table

	mesh->numindexes = numindexes;

	memcpy (mesh->indexes, tempIndexesArray, numindexes * sizeof(index_t) );
}

#ifdef RFBSPS
static void CModRBSP_BuildSurfMesh(model_t *mod, msurface_t *out, builddata_t *bd)
{
	cminfo_t	*prv = (cminfo_t*)mod->meshinfo;
	rbspface_t *in = (rbspface_t*)(bd+1);
	int idx = (out - mod->surfaces) - mod->firstmodelsurface;
	int sty;
	in += idx;

	if (LittleLong(in->facetype) == MST_PATCH)
	{
		GL_CreateMeshForPatch(mod, out->mesh, LittleLong(in->patchwidth), LittleLong(in->patchheight), LittleLong(in->num_vertices), LittleLong(in->firstvertex));
	}
	else if (LittleLong(in->facetype) == MST_PLANAR || LittleLong(in->facetype) == MST_TRIANGLE_SOUP)
	{
		unsigned int fv = LittleLong(in->firstvertex), i;
		for (i = 0; i < out->mesh->numvertexes; i++)
		{
			VectorCopy(prv->verts[fv + i], out->mesh->xyz_array[i]);
			Vector2Copy(prv->vertstmexcoords[fv + i], out->mesh->st_array[i]);
			for (sty = 0; sty < MAXRLIGHTMAPS; sty++)
			{
				Vector2Copy(prv->vertlstmexcoords[sty][fv + i], out->mesh->lmst_array[sty][i]);
				Vector4Copy(prv->colors4f_array[sty][fv + i], out->mesh->colors4f_array[sty][i]);
			}

			VectorCopy(prv->normals_array[fv + i], out->mesh->normals_array[i]);
		}
		fv = LittleLong(in->firstindex);
		for (i = 0; i < out->mesh->numindexes; i++)
		{
			out->mesh->indexes[i] = prv->surfindexes[fv + i];
		}
	}
	else
	{
/*		//flare
		int r, g, b;
		extern index_t r_quad_indexes[6];
		static vec2_t	st[4] = {{0,0},{0,1},{1,1},{1,0}};

		mesh = out->mesh = (mesh_t *)Hunk_Alloc(sizeof(mesh_t));
		mesh->xyz_array = (vecV_t *)Hunk_Alloc(sizeof(vecV_t)*4);
		mesh->colors4b_array = (byte_vec4_t *)Hunk_Alloc(sizeof(byte_vec4_t)*4);
		mesh->numvertexes = 4;
		mesh->indexes = r_quad_indexes;
		mesh->st_array = st;
		mesh->numindexes = 6;

		VectorCopy (in->lightmap_origin, mesh->xyz_array[0]);
		VectorCopy (in->lightmap_origin, mesh->xyz_array[1]);
		VectorCopy (in->lightmap_origin, mesh->xyz_array[2]);
		VectorCopy (in->lightmap_origin, mesh->xyz_array[3]);

		r = LittleFloat(in->lightmap_vecs[0][0]) * 255.0f;
		r = bound (0, r, 255);
		g = LittleFloat(in->lightmap_vecs[0][1]) * 255.0f;
		g = bound (0, g, 255);
		b = LittleFloat(in->lightmap_vecs[0][2]) * 255.0f;
		b = bound (0, b, 255);

		mesh->colors4b_array[0][0] = r;
		mesh->colors4b_array[0][1] = g;
		mesh->colors4b_array[0][2] = b;
		mesh->colors4b_array[0][3] = 255;
		Vector4Copy(mesh->colors4b_array[0], mesh->colors4b_array[1]);
		Vector4Copy(mesh->colors4b_array[0], mesh->colors4b_array[2]);
		Vector4Copy(mesh->colors4b_array[0], mesh->colors4b_array[3]);
*/
	}

	Mod_AccumulateMeshTextureVectors(out->mesh);
	Mod_NormaliseTextureVectors(out->mesh->normals_array, out->mesh->snormals_array, out->mesh->tnormals_array, out->mesh->numvertexes, false);
}
#endif

static void CModQ3_BuildSurfMesh(model_t *mod, msurface_t *out, builddata_t *bd)
{
	cminfo_t	*prv = (cminfo_t*)mod->meshinfo;
	q3dface_t *in = (q3dface_t*)(bd+1);
	int idx = (out - mod->surfaces) - mod->firstmodelsurface;
	in += idx;

	if (LittleLong(in->facetype) == MST_PATCH)
	{
		GL_CreateMeshForPatch(mod, out->mesh, LittleLong(in->patchwidth), LittleLong(in->patchheight), LittleLong(in->num_vertices), LittleLong(in->firstvertex));
	}
	else if (LittleLong(in->facetype) == MST_PLANAR || LittleLong(in->facetype) == MST_TRIANGLE_SOUP)
	{
		unsigned int fv = LittleLong(in->firstvertex), fi = LittleLong(in->firstindex), i;
		for (i = 0; i < out->mesh->numvertexes; i++)
		{
			VectorCopy(prv->verts[fv + i], out->mesh->xyz_array[i]);
			Vector2Copy(prv->vertstmexcoords[fv + i], out->mesh->st_array[i]);
			Vector2Copy(prv->vertlstmexcoords[0][fv + i], out->mesh->lmst_array[0][i]);
			Vector4Copy(prv->colors4f_array[0][fv + i], out->mesh->colors4f_array[0][i]);

			VectorCopy(prv->normals_array[fv + i], out->mesh->normals_array[i]);
		}
		for (i = 0; i < out->mesh->numindexes; i++)
		{
			out->mesh->indexes[i] = prv->surfindexes[fi + i];
		}
	}
	else
	{
/*		//flare
		int r, g, b;
		extern index_t r_quad_indexes[6];
		static vec2_t	st[4] = {{0,0},{0,1},{1,1},{1,0}};

		mesh = out->mesh = (mesh_t *)Hunk_Alloc(sizeof(mesh_t));
		mesh->xyz_array = (vecV_t *)Hunk_Alloc(sizeof(vecV_t)*4);
		mesh->colors4b_array = (byte_vec4_t *)Hunk_Alloc(sizeof(byte_vec4_t)*4);
		mesh->numvertexes = 4;
		mesh->indexes = r_quad_indexes;
		mesh->st_array = st;
		mesh->numindexes = 6;

		VectorCopy (in->lightmap_origin, mesh->xyz_array[0]);
		VectorCopy (in->lightmap_origin, mesh->xyz_array[1]);
		VectorCopy (in->lightmap_origin, mesh->xyz_array[2]);
		VectorCopy (in->lightmap_origin, mesh->xyz_array[3]);

		r = LittleFloat(in->lightmap_vecs[0][0]) * 255.0f;
		r = bound (0, r, 255);
		g = LittleFloat(in->lightmap_vecs[0][1]) * 255.0f;
		g = bound (0, g, 255);
		b = LittleFloat(in->lightmap_vecs[0][2]) * 255.0f;
		b = bound (0, b, 255);

		mesh->colors4b_array[0][0] = r;
		mesh->colors4b_array[0][1] = g;
		mesh->colors4b_array[0][2] = b;
		mesh->colors4b_array[0][3] = 255;
		Vector4Copy(mesh->colors4b_array[0], mesh->colors4b_array[1]);
		Vector4Copy(mesh->colors4b_array[0], mesh->colors4b_array[2]);
		Vector4Copy(mesh->colors4b_array[0], mesh->colors4b_array[3]);
*/
	}

	Mod_AccumulateMeshTextureVectors(out->mesh);
	Mod_NormaliseTextureVectors(out->mesh->normals_array, out->mesh->snormals_array, out->mesh->tnormals_array, out->mesh->numvertexes, false);
}

static qboolean CModQ3_LoadRFaces (model_t *mod, qbyte *mod_base, lump_t *l)
{
	cminfo_t	*prv = (cminfo_t*)mod->meshinfo;
	extern cvar_t r_vertexlight;
	q3dface_t *in;
	msurface_t *out;
	mplane_t *pl;

	int facetype;
	int count;
	int surfnum;

	int fv;
	int sty; 

	mesh_t *mesh;

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
	{
		Con_Printf (CON_ERROR "MOD_LoadBmodel: funny lump size in %s\n",mod->name);
		return false;
	}
	count = l->filelen / sizeof(*in);
	out = ZG_Malloc(&mod->memgroup, count*sizeof(*out));
	pl = ZG_Malloc(&mod->memgroup, count*sizeof(*pl));//create a new array of planes for speed.
	mesh = ZG_Malloc(&mod->memgroup, count*sizeof(*mesh));

	mod->surfaces = out;
	mod->numsurfaces = count;
	mod->lightmaps.first = 0;

	for (surfnum = 0; surfnum < count; surfnum++, out++, in++, pl++)
	{
		out->plane = pl;
		
		facetype = LittleLong(in->facetype);
		out->texinfo = mod->texinfo + LittleLong(in->shadernum);
		out->lightmaptexturenums[0] = LittleLong(in->lightmapnum);
		if (facetype == MST_FLARE)
			out->texinfo = mod->texinfo + mod->numtexinfo*2;
		else if (out->lightmaptexturenums[0] < 0 /*|| facetype == MST_TRIANGLE_SOUP*/ || r_vertexlight.value)
			out->texinfo += mod->numtexinfo;	//various surfaces use a different version of the same shader (with all the lightmaps collapsed)

		out->light_s[0] = LittleLong(in->lightmap_x);
		out->light_t[0] = LittleLong(in->lightmap_y);
		out->styles[0] = 255;
		for (sty = 1; sty < MAXRLIGHTMAPS; sty++)
		{
			out->styles[sty] = 255;
			out->lightmaptexturenums[sty] = -1;
		}
		out->lmshift = LMSHIFT_DEFAULT;
		//fixme: determine texturemins from lightmap_origin
		out->extents[0] = (LittleLong(in->lightmap_width)-1)<<out->lmshift;
		out->extents[1] = (LittleLong(in->lightmap_height)-1)<<out->lmshift;
		out->samples=NULL;

		if (mod->lightmaps.count < out->lightmaptexturenums[0]+1)
			mod->lightmaps.count = out->lightmaptexturenums[0]+1;

		fv = LittleLong(in->firstvertex);
		{
			vec3_t v[3];
			VectorCopy(prv->verts[fv+0], v[0]);
			VectorCopy(prv->verts[fv+1], v[1]);
			VectorCopy(prv->verts[fv+2], v[2]);
			PlaneFromPoints(v, pl);
			CategorizePlane(pl);
		}

		if (prv->surfaces[LittleLong(in->shadernum)].c.value == 0 || prv->surfaces[LittleLong(in->shadernum)].c.value & Q3CONTENTS_TRANSLUCENT)
				//q3dm10's thingie is 0
			out->flags |= SURF_DRAWALPHA;

		if (mod->texinfo[LittleLong(in->shadernum)].flags & TI_SKY)
			out->flags |= SURF_DRAWSKY;

		if (LittleLong(in->fognum) == -1 || !mod->numfogs)
			out->fog = NULL;
		else
			out->fog = mod->fogs + LittleLong(in->fognum);
		if (prv->surfaces[LittleLong(in->shadernum)].c.flags & (Q3SURF_NODRAW | Q3SURF_SKIP))
		{
			out->mesh = &mesh[surfnum];
			out->mesh->numindexes = 0;
			out->mesh->numvertexes = 0;
		}
		else if (facetype == MST_PATCH)
		{
			out->mesh = &mesh[surfnum];
			GL_SizePatch(out->mesh, LittleLong(in->patchwidth), LittleLong(in->patchheight), LittleLong(in->num_vertices), LittleLong(in->firstvertex), prv);
		}
		else if (facetype == MST_PLANAR || facetype == MST_TRIANGLE_SOUP)
		{
			out->mesh = &mesh[surfnum];
			out->mesh->numindexes = LittleLong(in->num_indexes);
			out->mesh->numvertexes = LittleLong(in->num_vertices);
/*
			Mod_AccumulateMeshTextureVectors(out->mesh);
*/
		}
		else
		{
			out->mesh = &mesh[surfnum];
			out->mesh->numindexes = 6;
			out->mesh->numvertexes = 4;
		}
	}

	Mod_SortShaders(mod);
	return true;
}

#ifdef RFBSPS
static qboolean CModRBSP_LoadRFaces (model_t *mod, qbyte *mod_base, lump_t *l)
{
	cminfo_t	*prv = (cminfo_t*)mod->meshinfo;
	extern cvar_t r_vertexlight;
	rbspface_t *in;
	msurface_t *out;
	mplane_t *pl;
	int facetype;

	int count;
	int surfnum;

	int fv;
	int j;

	mesh_t *mesh;


	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
	{
		Con_Printf (CON_ERROR "MOD_LoadBmodel: funny lump size in %s\n",mod->name);
		return false;
	}
	count = l->filelen / sizeof(*in);
	out = ZG_Malloc(&mod->memgroup, count*sizeof(*out));
	pl = ZG_Malloc(&mod->memgroup, count*sizeof(*pl));//create a new array of planes for speed.
	mesh = ZG_Malloc(&mod->memgroup, count*sizeof(*mesh));

	mod->surfaces = out;
	mod->numsurfaces = count;

	for (surfnum = 0; surfnum < count; surfnum++, out++, in++, pl++)
	{
		out->plane = pl;
		facetype = LittleLong(in->facetype);
		out->texinfo = mod->texinfo + LittleLong(in->shadernum);
		for (j = 0; j < 4 && j < MAXRLIGHTMAPS; j++)
		{
			out->lightmaptexturenums[j] = LittleLong(in->lightmapnum[j]);
			out->light_s[j] = LittleLong(in->lightmap_offs[0][j]);
			out->light_t[j] = LittleLong(in->lightmap_offs[1][j]);
			out->styles[j] = in->lm_styles[j];

			if (mod->lightmaps.count < out->lightmaptexturenums[j]+1)
				mod->lightmaps.count = out->lightmaptexturenums[j]+1;
		}
		if (facetype == MST_FLARE)
			out->texinfo = mod->texinfo + mod->numtexinfo*2;
		else if (out->lightmaptexturenums[0]<0 || r_vertexlight.value)
			out->texinfo += mod->numtexinfo;	//soup/vertex light uses a different version of the same shader (with all the lightmaps collapsed)

		out->lmshift = LMSHIFT_DEFAULT;
		out->extents[0] = (LittleLong(in->lightmap_width)-1)<<out->lmshift;
		out->extents[1] = (LittleLong(in->lightmap_height)-1)<<out->lmshift;
		out->samples=NULL;

		fv = LittleLong(in->firstvertex);
		{
			vec3_t v[3];
			VectorCopy(prv->verts[fv+0], v[0]);
			VectorCopy(prv->verts[fv+1], v[1]);
			VectorCopy(prv->verts[fv+2], v[2]);
			PlaneFromPoints(v, pl);
			CategorizePlane(pl);
		}

		if (prv->surfaces[in->shadernum].c.value == 0 || prv->surfaces[in->shadernum].c.value & Q3CONTENTS_TRANSLUCENT)
				//q3dm10's thingie is 0
			out->flags |= SURF_DRAWALPHA;

		if (mod->texinfo[in->shadernum].flags & TI_SKY)
			out->flags |= SURF_DRAWSKY;

		if (in->fognum < 0 || in->fognum >= mod->numfogs)
			out->fog = NULL;
		else
			out->fog = mod->fogs + in->fognum;

		if (prv->surfaces[LittleLong(in->shadernum)].c.flags & (Q3SURF_NODRAW | Q3SURF_SKIP))
		{
			out->mesh = &mesh[surfnum];
			out->mesh->numindexes = 0;
			out->mesh->numvertexes = 0;
		}
		else if (facetype == MST_PATCH)
		{
			out->mesh = &mesh[surfnum];
			GL_SizePatch(out->mesh, LittleLong(in->patchwidth), LittleLong(in->patchheight), LittleLong(in->num_vertices), LittleLong(in->firstvertex), prv);
		}
		else if (facetype == MST_PLANAR || facetype == MST_TRIANGLE_SOUP)
		{
			out->mesh = &mesh[surfnum];
			out->mesh->numindexes = LittleLong(in->num_indexes);
			out->mesh->numvertexes = LittleLong(in->num_vertices);
/*
			Mod_AccumulateMeshTextureVectors(out->mesh);
*/
		}
		else
		{
			out->mesh = &mesh[surfnum];
			out->mesh->numindexes = 6;
			out->mesh->numvertexes = 4;
		}
	}
	
	Mod_SortShaders(mod);
	return true;
}
#endif
#endif

static qboolean CModQ3_LoadNodes (model_t *loadmodel, qbyte *mod_base, lump_t *l)
{
	int			i, j, count, p;
	q3dnode_t	*in;
	mnode_t 	*out;
	//dnode_t

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
	{
		Con_Printf (CON_ERROR "MOD_LoadBmodel: funny lump size in %s\n",loadmodel->name);
		return false;
	}
	count = l->filelen / sizeof(*in);
	out = ZG_Malloc(&loadmodel->memgroup, count*sizeof(*out));

	if (count > SANITY_MAX_MAP_NODES)
	{
		Con_Printf (CON_ERROR "Too many nodes on map\n");
		return false;
	}

	loadmodel->nodes = out;
	loadmodel->numnodes = count;

	for ( i=0 ; i<count ; i++, in++, out++)
	{
		for (j=0 ; j<3 ; j++)
		{
			out->minmaxs[j] = LittleLong (in->mins[j]);
			out->minmaxs[3+j] = LittleLong (in->maxs[j]);
		}
		AddPointToBounds(out->minmaxs, loadmodel->mins, loadmodel->maxs);
		AddPointToBounds(out->minmaxs+3, loadmodel->mins, loadmodel->maxs);

		p = LittleLong(in->plane);
		out->plane = loadmodel->planes + p;

		out->firstsurface = 0;//LittleShort (in->firstface);
		out->numsurfaces = 0;//LittleShort (in->numfaces);

		out->contents = -1;

		for (j=0 ; j<2 ; j++)
		{
			p = LittleLong (in->children[j]);
			out->childnum[j] = p;
			if (p >= 0)
			{
				out->children[j] = loadmodel->nodes + p;
			}
			else
				out->children[j] = (mnode_t *)(loadmodel->leafs + (-1 - p));
		}
	}

	CMod_SetParent (loadmodel->nodes, NULL);	// sets nodes and leafs

	return true;
}

static qboolean CModQ3_LoadBrushes (model_t *mod, qbyte *mod_base, lump_t *l)
{
	cminfo_t	*prv = (cminfo_t*)mod->meshinfo;
	q3dbrush_t	*in;
	q2cbrush_t	*out;
	int			i, count;
	int			shaderref;

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
	{
		Con_Printf (CON_ERROR "MOD_LoadBmodel: funny lump size\n");
		return false;
	}
	count = l->filelen / sizeof(*in);

	if (count > SANITY_MAX_MAP_BRUSHES)
	{
		Con_Printf (CON_ERROR "Map has too many brushes");
		return false;
	}

	prv->brushes = ZG_Malloc(&mod->memgroup, sizeof(*out) * (count+1));

	out = prv->brushes;

	prv->numbrushes = count;

	for (i=0 ; i<count ; i++, out++, in++)
	{
		shaderref = LittleLong ( in->shadernum );
		out->contents = prv->surfaces[shaderref].c.value;
		out->brushside = &prv->brushsides[LittleLong ( in->firstside )];
		out->numsides = LittleLong ( in->num_sides );
		CM_FinalizeBrush(out);
	}

	return true;
}

static qboolean CModQ3_LoadLeafs (model_t *mod, qbyte *mod_base, lump_t *l)
{
	cminfo_t	*prv = (cminfo_t*)mod->meshinfo;
	int			i, j;
	mleaf_t		*out;
	q3dleaf_t 	*in;
	int			count;
	q2cbrush_t	*brush;

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
	{
		Con_Printf (CON_ERROR "MOD_LoadBmodel: funny lump size\n");
		return false;
	}
	count = l->filelen / sizeof(*in);

	if (count < 1)
	{
		Con_Printf (CON_ERROR "Map with no leafs\n");
		return false;
	}
	// need to save space for box planes

	if (count > SANITY_MAX_MAP_LEAFS)
	{
		Con_Printf (CON_ERROR "Too many leaves on map");
		return false;
	}

	out = ZG_Malloc(&mod->memgroup, sizeof(*out) * (count+1));

	mod->leafs = out;
	mod->numleafs = count;

	for ( i=0 ; i<count ; i++, in++, out++)
	{
		for (j = 0; j < 3; j++)
		{
			out->minmaxs[0+j] = LittleLong(in->mins[j]);
			out->minmaxs[3+j] = LittleLong(in->maxs[j]);
		}
		out->cluster = LittleLong(in->cluster);
		out->area = LittleLong(in->area);
//		out->firstleafface = LittleLong(in->firstleafsurface);
//		out->numleaffaces = LittleLong(in->num_leafsurfaces);
		out->contents = 0;
		out->firstleafbrush = LittleLong(in->firstleafbrush);
		out->numleafbrushes = LittleLong(in->num_leafbrushes);

		out->firstmarksurface = mod->marksurfaces + LittleLong(in->firstleafsurface);
		out->nummarksurfaces = LittleLong(in->num_leafsurfaces);

		if (out->minmaxs[0] > out->minmaxs[3+0] || out->minmaxs[1] > out->minmaxs[3+1] ||
			out->minmaxs[2] > out->minmaxs[3+2])// || VectorEquals (out->minmaxs, out->minmaxs+3))
		{
			out->nummarksurfaces = 0;
		}

		for (j=0 ; j<out->numleafbrushes ; j++)
		{
			brush = prv->leafbrushes[out->firstleafbrush + j];
			out->contents |= brush->contents;
		}

		if (out->area >= prv->numareas)
		{
			prv->numareas = out->area + 1;
		}
	}

	return true;
}

static qboolean CModQ3_LoadPlanes (model_t *loadmodel, qbyte *mod_base, lump_t *l)
{
	int			i, j;
	mplane_t	*out;
	Q3PLANE_t 	*in;
	int			count;

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
	{
		Con_Printf (CON_ERROR "MOD_LoadBmodel: funny lump size in %s\n",loadmodel->name);
		return false;
	}
	count = l->filelen / sizeof(*in);

	if (count > SANITY_MAX_MAP_PLANES)
	{
		Con_Printf (CON_ERROR "Too many planes on map (%i)\n", count);
		return false;
	}

	loadmodel->planes = out = ZG_Malloc(&loadmodel->memgroup, sizeof(*out) * count);
	loadmodel->numplanes = count;

	for ( i=0 ; i<count ; i++, in++, out++)
	{
		for (j=0 ; j<3 ; j++)
		{
			out->normal[j] = LittleFloat (in->n[j]);
		}
		out->dist = LittleFloat (in->d);

		CategorizePlane(out);
	}

	return true;
}

static qboolean CModQ3_LoadLeafBrushes (model_t *mod, qbyte *mod_base, lump_t *l)
{
	cminfo_t	*prv = (cminfo_t*)mod->meshinfo;
	int			i;
	q2cbrush_t  **out;
	int 	*in;
	int			count;

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
	{
		Con_Printf (CON_ERROR "MOD_LoadBmodel: funny lump size\n");
		return false;
	}
	count = l->filelen / sizeof(*in);

	if (count < 1)
	{
		Con_Printf (CON_ERROR "Map with no leafbrushes\n");
		return false;
	}
	// need to save space for box planes
	if (count > MAX_Q2MAP_LEAFBRUSHES)
	{
		Con_Printf (CON_ERROR "Map has too many leafbrushes\n");
		return false;
	}

	out = prv->leafbrushes;
	prv->numleafbrushes = count;

	for ( i=0 ; i<count ; i++, in++, out++)
		*out = prv->brushes + LittleLong (*in);

	return true;
}

static qboolean CModQ3_LoadBrushSides (model_t *mod, qbyte *mod_base, lump_t *l)
{
	cminfo_t	*prv = (cminfo_t*)mod->meshinfo;
	int			i, j;
	q2cbrushside_t	*out;
	q3dbrushside_t 	*in;
	int			count;
	int			num;

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
	{
		Con_Printf (CON_ERROR "MOD_LoadBmodel: funny lump size\n");
		return false;
	}
	count = l->filelen / sizeof(*in);

	// need to save space for box planes
	if (count > SANITY_MAX_MAP_BRUSHSIDES)
	{
		Con_Printf (CON_ERROR "Map has too many brushsides (%i)\n", count);
		return false;
	}

	out = prv->brushsides = ZG_Malloc(&mod->memgroup, sizeof(*out) * count);
	prv->numbrushsides = count;

	for ( i=0 ; i<count ; i++, in++, out++)
	{
		num = LittleLong (in->planenum);
		out->plane = &mod->planes[num];
		j = LittleLong (in->texinfo);
		if (j >= mod->numtexinfo)
		{
			Con_Printf (CON_ERROR "Bad brushside texinfo\n");
			return false;
		}
		out->surface = &prv->surfaces[j];
	}

	return true;
}

#ifdef RFBSPS
static qboolean CModRBSP_LoadBrushSides (model_t *mod, qbyte *mod_base, lump_t *l)
{
	cminfo_t	*prv = (cminfo_t*)mod->meshinfo;
	int			i, j;
	q2cbrushside_t	*out;
	rbspbrushside_t 	*in;
	int			count;
	int			num;

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
	{
		Con_Printf (CON_ERROR "MOD_LoadBmodel: funny lump size\n");
		return false;
	}
	count = l->filelen / sizeof(*in);

	// need to save space for box planes
	if (count > SANITY_MAX_MAP_BRUSHSIDES)
	{
		Con_Printf (CON_ERROR "Map has too many brushsides (%i)\n", count);
		return false;
	}

	out = prv->brushsides = ZG_Malloc(&mod->memgroup, sizeof(*out) * count);
	prv->numbrushsides = count;

	for ( i=0 ; i<count ; i++, in++, out++)
	{
		num = LittleLong (in->planenum);
		out->plane = &mod->planes[num];
		j = LittleLong (in->texinfo);
		if (j >= mod->numtexinfo)
		{
			Con_Printf (CON_ERROR "Bad brushside texinfo\n");
			return false;
		}
		out->surface = &prv->surfaces[j];
	}

	return true;
}
#endif

static qboolean CModQ3_LoadVisibility (model_t *mod, qbyte *mod_base, lump_t *l)
{
	cminfo_t	*prv = (cminfo_t*)mod->meshinfo;
	unsigned int numclusters;
	if (l->filelen == 0)
	{
		int i;
#if 0
		//the 'correct' code
		numclusters = 0;
		for (i = 0; i < mod->numleafs; i++)
			if (numclusters < mod->leafs[i].cluster+1)
				numclusters = mod->leafs[i].cluster+1;

		numclusters++;
#else
		//but its much faster to merge all leafs into a single pvs cluster. no vis is no vis.
		numclusters = 8*sizeof(int);
		for (i = 0; i < mod->numleafs; i++)
			mod->leafs[i].cluster = !!mod->leafs[i].cluster;
#endif
		prv->q3pvs = ZG_Malloc(&mod->memgroup, sizeof(*prv->q3pvs) + (numclusters+7)/8 * numclusters);
		memset (prv->q3pvs, 0xff, sizeof(*prv->q3pvs) + (numclusters+7)/8 * numclusters);
		prv->q3pvs->numclusters = numclusters;
		prv->numvisibility = 0;
		prv->q3pvs->rowsize = (prv->q3pvs->numclusters+7)/8;
	}
	else
	{
		prv->numvisibility = l->filelen;

		prv->q3pvs = ZG_Malloc(&mod->memgroup, l->filelen);
		mod->vis = (q2dvis_t *)prv->q3pvs;
		memcpy (prv->q3pvs, mod_base + l->fileofs, l->filelen);

		numclusters = prv->q3pvs->numclusters = LittleLong (prv->q3pvs->numclusters);
		prv->q3pvs->rowsize = LittleLong (prv->q3pvs->rowsize);
	}
	mod->numclusters = numclusters;
	mod->pvsbytes = ((mod->numclusters + 31)>>3)&~3;

	return true;
}

#ifndef SERVERONLY
static void CModQ3_LoadLighting (model_t *loadmodel, qbyte *mod_base, lump_t *l)
{
	qbyte *in = mod_base + l->fileofs;
	qbyte *out;
	unsigned int samples = l->filelen;
	int m, s;
	int mapsize = loadmodel->lightmaps.width*loadmodel->lightmaps.height*3;
	int maps;

	extern cvar_t gl_overbright;
	extern qbyte lmgamma[256];
	loadmodel->lightmaps.fmt = LM_L8;

	//round up the samples, in case the last one is partial.
	maps = ((samples+mapsize-1)&~(mapsize-1)) / mapsize;

	//q3 maps have built in 4-fold overbright.
	//if we're not rendering with that, we need to brighten the lightmaps in order to keep the darker parts the same brightness. we loose the 2 upper bits. those bright areas become uniform and indistinct.
	gl_overbright.flags |= CVAR_RENDERERLATCH;
	BuildLightMapGammaTable(1, (1<<(2-gl_overbright.ival)));

	loadmodel->lightmaps.merge = 0;
	if (!samples)
		return;

	loadmodel->engineflags |= MDLF_NEEDOVERBRIGHT;

	loadmodel->lightmaps.fmt = LM_RGB8;

	if (loadmodel->lightmaps.deluxemapping)
		maps /= 2;

	{
		int limit = min(sh_config.texture2d_maxsize / loadmodel->lightmaps.height, q3bsp_mergeq3lightmaps.ival);
		loadmodel->lightmaps.merge = 1;
		while (loadmodel->lightmaps.merge*2 <= limit && loadmodel->lightmaps.merge < maps)
			loadmodel->lightmaps.merge *= 2;
	}

	//q3bsp itself does not support deluxemapping.
	//the way it works is by interleaving the data in lightmap+deluxemap pairs.
	//the surface data makes no references to the deluxemap maps, they're implied by lightmap+1
	//if no surface references an odd lightmap index then we know we have deluxemaps... assuming there are at least two lightmaps.
	//q3map2 likes generating null lightmaps, so beware false positives.

	//note that external lighting makes this even more fun.

	//if we have deluxemapping data then we split it here. beware externals.
	if (loadmodel->lightmaps.deluxemapping)
	{
		m = loadmodel->lightmaps.merge;
		while (m < maps)
			m += loadmodel->lightmaps.merge;
		loadmodel->lightdata = ZG_Malloc(&loadmodel->memgroup, mapsize*m*2);
		loadmodel->lightdatasize = mapsize*m*2;
	}
	else
	{
		loadmodel->lightdatasize = samples;
		loadmodel->lightdata = ZG_Malloc(&loadmodel->memgroup, samples);
	}

	if (!loadmodel->lightdata)
		return;


	//be careful here, q3bsp deluxemapping is done using interleaving. we want to unoverbright ONLY lightmaps and not deluxemaps.
	for (m = 0; m < maps; m++)
	{
		out = loadmodel->lightdata;
		//figure out which merged lightmap we're putting it into
		out += (m/loadmodel->lightmaps.merge)*loadmodel->lightmaps.merge*mapsize * (loadmodel->lightmaps.deluxemapping?2:1);
		//and the submap
		out += (m%loadmodel->lightmaps.merge)*mapsize;

#if 1
		//q3bsp has 4-fold overbrights, so if we're not using overbrights then we basically need to scale the values up by 4
		//this will require clamping, which can result in oversaturation of channels, meaning discolouration
		for(s = 0; s < mapsize; )
		{
			float scale = (1<<(2-gl_overbright.ival));
			float i;
			vec3_t l;
			l[0] = *in++;
			l[1] = *in++;
			l[2] = *in++;
			VectorScale(l, scale, l);		//it should be noted that this maths is wrong if you're trying to use srgb lightmaps.
			i = max(l[0], max(l[1], l[2]));
			if (i > 255)
				VectorScale(l, 255/i, l);	//clamp the brightest channel, scaling the others down to retain chromiance.
			out[s++] = l[0];
			out[s++] = l[1];
			out[s++] = l[2];
		}
#else
		for(s = 0; s < mapsize; s++)
			out[s] = lmgamma[*in++];
#endif
		if (r_lightmap_saturation.value != 1.0f)
			SaturateR8G8B8(out, mapsize, r_lightmap_saturation.value);
		
		if (loadmodel->lightmaps.deluxemapping)
		{
			out+= loadmodel->lightmaps.merge*mapsize;

			//no gamma for deluxemap
			for(s = 0; s < mapsize; s+=3)
			{
				*out++ = in[0];
				*out++ = in[1];
				*out++ = in[2];
				in += 3;
			}
		}
	}
	/*for (; m%loadmodel->lightmaps.merge; m++)
	{
		out = loadmodel->lightdata;
		//figure out which merged lightmap we're putting it into
		out += (m/loadmodel->lightmaps.merge)*loadmodel->lightmaps.merge*mapsize * (loadmodel->lightmaps.deluxemapping?2:1);
		//and the submap
		out += (m%loadmodel->lightmaps.merge)*mapsize;

		for(s = 0; s < mapsize; s+=3)
		{
			out[s+0] = 0;
			out[s+1] = 255;
			out[s+2] = 0;
		}
	}*/
}

static qboolean CModQ3_LoadLightgrid (model_t *loadmodel, qbyte *mod_base, lump_t *l)
{
	dq3gridlight_t 	*in;
	dq3gridlight_t 	*out;
	q3lightgridinfo_t *grid;
	int	count;

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
	{
		Con_Printf (CON_ERROR "MOD_LoadBmodel: funny lump size in %s\n",loadmodel->name);
		return false;
	}
	count = l->filelen / sizeof(*in);
	grid = ZG_Malloc(&loadmodel->memgroup, sizeof(q3lightgridinfo_t) + count*sizeof(*out));
	grid->lightgrid = (dq3gridlight_t*)(grid+1);
	out = grid->lightgrid;

	loadmodel->lightgrid = grid;
	grid->numlightgridelems = count;

	// lightgrid is all 8 bit
	memcpy ( out, in, count*sizeof(*out) );

	return true;
}

#ifdef RFBSPS
static qboolean CModRBSP_LoadLightgrid (model_t *loadmodel, qbyte *mod_base, lump_t *elements, lump_t *indexes)
{
	unsigned short	*iin;
	rbspgridlight_t	*ein;
	unsigned short	*iout;
	rbspgridlight_t	*eout;
	q3lightgridinfo_t *grid;
	int	ecount;
	int icount;

	int i;

	ein = (void *)(mod_base + elements->fileofs);
	iin = (void *)(mod_base + indexes->fileofs);
	if (indexes->filelen % sizeof(*iin) || elements->filelen % sizeof(*ein))
	{
		Con_Printf (CON_ERROR "MOD_LoadBmodel: funny lump size in %s\n",loadmodel->name);
		return false;
	}
	icount = indexes->filelen / sizeof(*iin);
	ecount = elements->filelen / sizeof(*ein);

	grid = ZG_Malloc(&loadmodel->memgroup, sizeof(q3lightgridinfo_t) + ecount*sizeof(*eout) + icount*sizeof(*iout));
	grid->rbspelements = (rbspgridlight_t*)((char *)grid + sizeof(q3lightgridinfo_t));
	grid->rbspindexes = (unsigned short*)((char *)grid + sizeof(q3lightgridinfo_t) + ecount*sizeof(*eout));
	eout = grid->rbspelements;
	iout = grid->rbspindexes;

	loadmodel->lightgrid = grid;

	grid->numlightgridelems = icount;

	// elements are all 8 bit
	memcpy ( eout, ein, ecount*sizeof(*eout) );

	for (i = 0; i < icount; i++)
		iout[i] = LittleShort(iin[i]);

	return true;
}
#endif
#endif
#endif

#if !defined(SERVERONLY) && defined(Q2BSPS)
qbyte *ReadPCXPalette(qbyte *buf, int len, qbyte *out);
static int CM_GetQ2Palette (void)
{
	char *f;
	size_t sz;
	sz = FS_LoadFile("pics/colormap.pcx", (void**)&f);
	if (!f)
	{
		Con_Printf (CON_WARNING "Couldn't find pics/colormap.pcx\n");
		return -1;
	}
	if (!ReadPCXPalette(f, sz, d_q28to24table))
	{
		Con_Printf (CON_WARNING "Couldn't read pics/colormap.pcx\n");
		FS_FreeFile(f);
		return -1;
	}
	FS_FreeFile(f);


#if 1
	{
		float	inf;
		qbyte	palette[768];
		qbyte *pal;
		int		i;

		pal = d_q28to24table;

		for (i=0 ; i<768 ; i++)
		{
			inf = ((pal[i]+1)/256.0)*255 + 0.5;
			if (inf < 0)
				inf = 0;
			if (inf > 255)
				inf = 255;
			palette[i] = inf;
		}

		memcpy (d_q28to24table, palette, sizeof(palette));
	}
#endif
	return 0;
}
#endif

#if 0
static void CM_OpenAllPortals(model_t *mod, char *ents)	//this is a compleate hack. About as compleate as possible.
{	//q2 levels contain a thingie called area portals. Basically, doors can seperate two areas and
	//the engine knows when this portal is open, and weather to send ents from both sides of the door
	//or not. It's not just ents, but also walls. We want to just open them by default and hope the
	//progs knows how to close them.

	char style[8];
	char name[64];

	if (!map_autoopenportals.value)
		return;

	while(*ents)
	{
		if (*ents == '{')	//an entity
		{
			ents++;
			*style = '\0';
			*name = '\0';
			while (*ents)
			{
				ents = COM_Parse(ents);

				if (!strcmp(com_token, "classname"))
				{
					ents = COM_ParseOut(ents, name, sizeof(name));
				}
				else if (!strcmp(com_token, "style"))
				{
					ents = COM_ParseOut(ents, style, sizeof(style));
				}
				else if (*com_token == '}')
					break;
				else
					ents = COM_Parse(ents);	//other field
				ents++;
			}

			if (!strcmp(name, "func_areaportal"))
			{
				CMQ2_SetAreaPortalState(mod, atoi(style), true);
			}
		}

		ents++;
	}
}
#endif


#if !defined(CLIENTONLY) && defined(Q3BSPS)
static void CMQ3_CalcPHS (model_t *mod)
{
	cminfo_t	*prv = (cminfo_t*)mod->meshinfo;
	int		rowbytes, rowwords;
	int		i, j, k, l, index;
	int		bitbyte;
	unsigned int	*dest, *src;
	qbyte	*scan;
	int		count, vcount;
	int		numclusters;
	qboolean buggytools = false;

	Con_DPrintf ("Building PHS...\n");

	prv->q3phs = ZG_Malloc(&mod->memgroup, sizeof(*prv->q3phs) + prv->q3pvs->rowsize * prv->q3pvs->numclusters);

	rowwords = prv->q3pvs->rowsize / sizeof(int);
	rowbytes = prv->q3pvs->rowsize;

	memset ( prv->q3phs, 0, sizeof(*prv->q3phs) + prv->q3pvs->rowsize * prv->q3pvs->numclusters );

	prv->q3phs->rowsize = prv->q3pvs->rowsize;
	prv->q3phs->numclusters = numclusters = prv->q3pvs->numclusters;
	if (!numclusters)
		return;

	vcount = 0;
	for (i=0 ; i<numclusters ; i++)
	{
		scan = CM_ClusterPVS (mod, i, NULL, PVM_FAST);
		for (j=0 ; j<numclusters ; j++)
		{
			if ( scan[j>>3] & (1<<(j&7)) )
			{
				vcount++;
			}
		}
	}

	count = 0;
	scan = (qbyte *)prv->q3pvs->data;
	dest = (unsigned int *)(prv->q3phs->data);

	for (i=0 ; i<numclusters ; i++, dest += rowwords, scan += rowbytes)
	{
		memcpy (dest, scan, rowbytes);
		for (j=0 ; j<rowbytes ; j++)
		{
			bitbyte = scan[j];
			if (!bitbyte)
				continue;
			for (k=0 ; k<8 ; k++)
			{
				if (! (bitbyte & (1<<k)) )
					continue;
				// OR this pvs row into the phs
				index = (j<<3) + k;
				if (index >= numclusters)
				{
					if (!buggytools)
						Con_Printf ("CM_CalcPHS: Bad bit(s) in PVS (%i >= %i)\n", index, numclusters);	// pad bits should be 0
					buggytools = true;
				}
				else
				{
					src = (unsigned int *)(prv->q3pvs->data) + index*rowwords;
					for (l=0 ; l<rowwords ; l++)
						dest[l] |= src[l];
				}
			}
		}
		for (j=0 ; j<numclusters ; j++)
			if ( ((qbyte *)dest)[j>>3] & (1<<(j&7)) )
				count++;
	}

	Con_DPrintf ("Average clusters visible / hearable / total: %i / %i / %i\n"
		, vcount/numclusters, count/numclusters, numclusters);
}
#endif

/*
static qbyte *CM_LeafnumPVS (model_t *model, int leafnum, qbyte *buffer, unsigned int buffersize)
{
	return CM_ClusterPVS(model, CM_LeafCluster(model, leafnum), buffer, buffersize);
}
*/

#ifndef SERVERONLY
#define GLQ2BSP_LightPointValues GLQ1BSP_LightPointValues

extern int	r_dlightframecount;
static void Q2BSP_MarkLights (dlight_t *light, int bit, mnode_t *node)
{
	mplane_t	*splitplane;
	float		dist;
	msurface_t	*surf;
	int			i;

	if (node->contents != -1)
	{
		mleaf_t *leaf = (mleaf_t *)node;
		msurface_t **mark;

		i = leaf->nummarksurfaces;
		mark = leaf->firstmarksurface;
		while(i--!=0)
		{
			surf = *mark++;
			if (surf->dlightframe != r_dlightframecount)
			{
				surf->dlightbits = 0;
				surf->dlightframe = r_dlightframecount;
			}
			surf->dlightbits |= bit;
		}
		return;
	}

	splitplane = node->plane;
	dist = DotProduct (light->origin, splitplane->normal) - splitplane->dist;

	if (dist > light->radius)
	{
		Q2BSP_MarkLights (light, bit, node->children[0]);
		return;
	}
	if (dist < -light->radius)
	{
		Q2BSP_MarkLights (light, bit, node->children[1]);
		return;
	}

// mark the polygons
	surf = cl.worldmodel->surfaces + node->firstsurface;
	for (i=0 ; i<node->numsurfaces ; i++, surf++)
	{
		if (surf->dlightframe != r_dlightframecount)
		{
			surf->dlightbits = 0;
			surf->dlightframe = r_dlightframecount;
		}
		surf->dlightbits |= bit;
	}

	Q2BSP_MarkLights (light, bit, node->children[0]);
	Q2BSP_MarkLights (light, bit, node->children[1]);
}

#ifndef SERVERONLY
static void GLR_Q2BSP_StainNode (mnode_t *node, float *parms)
{
	mplane_t	*splitplane;
	float		dist;
	msurface_t	*surf;
	int			i;

	if (node->contents != -1)
		return;

	splitplane = node->plane;
	dist = DotProduct ((parms+1), splitplane->normal) - splitplane->dist;

	if (dist > (*parms))
	{
		GLR_Q2BSP_StainNode (node->children[0], parms);
		return;
	}
	if (dist < (-*parms))
	{
		GLR_Q2BSP_StainNode (node->children[1], parms);
		return;
	}

// mark the polygons
	surf = cl.worldmodel->surfaces + node->firstsurface;
	for (i=0 ; i<node->numsurfaces ; i++, surf++)
	{
		if (surf->flags&~(SURF_DONTWARP|SURF_PLANEBACK))
			continue;
		Surf_StainSurf(surf, parms);
	}

	GLR_Q2BSP_StainNode (node->children[0], parms);
	GLR_Q2BSP_StainNode (node->children[1], parms);
}
#endif

#endif

void GLQ2BSP_LightPointValues(model_t *mod, vec3_t point, vec3_t res_diffuse, vec3_t res_ambient, vec3_t res_dir);

/*
==================
CM_LoadMap

Loads in the map and all submodels
==================
*/
static cmodel_t *CM_LoadMap (model_t *mod, qbyte *filein, size_t filelen, qboolean clientload, unsigned *checksum)
{
	unsigned		*buf;
	int				i;
	q2dheader_t		header;
	int				length;
	static unsigned	last_checksum;
	qboolean noerrors = true;
	model_t			*wmod = mod;
	char			loadname[32];
	qbyte			*mod_base = (qbyte *)filein;
	bspx_header_t	*bspx = NULL;
#ifdef Q3BSPS
	extern cvar_t	gl_overbright;
#endif

#ifndef SERVERONLY
	void (*buildmeshes)(model_t *mod, msurface_t *surf, builddata_t *cookie) = NULL;
	qbyte *facedata = NULL;
	unsigned int facesize = 0;
#endif
	cminfo_t	*prv;

	COM_FileBase (mod->name, loadname, sizeof(loadname));

	// free old stuff
	mod->meshinfo = prv = ZG_Malloc(&mod->memgroup, sizeof(*prv));
	prv->numcmodels = 0;
	prv->numvisibility = 0;

	mod->type = mod_brush;

	if (!mod->name[0])
	{
		prv->cmodels = ZG_Malloc(&mod->memgroup, 1 * sizeof(*prv->cmodels));
		mod->leafs = ZG_Malloc(&mod->memgroup, 1 * sizeof(*mod->leafs));
		prv->numcmodels = 1;
		prv->numareas = 1;
		*checksum = 0;
		prv->cmodels[0].headnode = (mnode_t*)mod->leafs;	//directly start with the empty leaf
		return &prv->cmodels[0];			// cinematic servers won't have anything at all
	}

	//
	// load the file
	//
	buf = (unsigned	*)filein;
	length = filelen;
	if (!buf)
	{
		Con_Printf (CON_ERROR "Couldn't load %s\n", mod->name);
		return NULL;
	}

	last_checksum = LittleLong (Com_BlockChecksum (buf, length));
	*checksum = last_checksum;

	header = *(q2dheader_t *)(buf);
	header.ident = LittleLong(header.ident);
	header.version = LittleLong(header.version);

	ClearBounds(mod->mins, mod->maxs);

	switch(header.version)
	{
	default:
		Con_Printf (CON_ERROR "Quake 2 or Quake 3 based BSP with unknown header (%s: %i should be %i or %i)\n"
			, mod->name, header.version, BSPVERSION_Q2, BSPVERSION_Q3);
		return NULL;
		break;
#ifdef Q3BSPS
#ifdef RFBSPS
	case BSPVERSION_RBSP: //rbsp/fbsp
#endif
	case BSPVERSION_RTCW:	//rtcw
	case BSPVERSION_Q3:
#ifdef RFBSPS
		if (header.ident == (('F'<<0)+('B'<<8)+('S'<<16)+('P'<<24)))
		{
			mod->lightmaps.width = 512;
			mod->lightmaps.height = 512;
		}
		else
#endif
		{
			mod->lightmaps.width = 128;
			mod->lightmaps.height = 128;
		}

		prv->mapisq3 = true;
		mod->fromgame = fg_quake3;
		for (i=0 ; i<Q3LUMPS_TOTAL ; i++)
		{
#ifdef RFBSPS
			if (i == RBSPLUMP_LIGHTINDEXES && header.version != BSPVERSION_RBSP)
			{
				header.lumps[i].filelen = 0;
				header.lumps[i].fileofs = 0;
			}
			else
#endif
			{
				header.lumps[i].filelen = LittleLong (header.lumps[i].filelen);
				header.lumps[i].fileofs = LittleLong (header.lumps[i].fileofs);

				if (header.lumps[i].filelen && header.lumps[i].fileofs + header.lumps[i].filelen > filelen)
				{
					Con_Printf (CON_ERROR "WARNING: q3bsp %s truncated (lump %i, %i+%i > %u)\n", mod->name, i, header.lumps[i].fileofs, header.lumps[i].filelen, (unsigned int)filelen);
					header.lumps[i].filelen = filelen - header.lumps[i].fileofs;
					if (header.lumps[i].filelen < 0)
						header.lumps[i].filelen = 0;
				}
			}
		}
		/*
		#ifndef SERVERONLY
			GLMod_LoadVertexes		(mod, cmod_base, &header.lumps[Q3LUMP_DRAWVERTS]);
//			GLMod_LoadEdges			(mod, cmod_base, &header.lumps[Q3LUMP_EDGES]);
//			GLMod_LoadSurfedges		(mod, cmod_base, &header.lumps[Q3LUMP_SURFEDGES]);
			GLMod_LoadLighting		(mod, cmod_base, &header.lumps[Q3LUMP_LIGHTMAPS]);
		#endif
			CModQ3_LoadShaders		(mod, cmod_base, &header.lumps[Q3LUMP_SHADERS]);
			CModQ3_LoadPlanes		(mod, cmod_base, &header.lumps[Q3LUMP_PLANES]);
			CModQ3_LoadLeafBrushes	(mod, cmod_base, &header.lumps[Q3LUMP_LEAFBRUSHES]);
			CModQ3_LoadBrushes		(mod, cmod_base, &header.lumps[Q3LUMP_BRUSHES]);
			CModQ3_LoadBrushSides	(mod, cmod_base, &header.lumps[Q3LUMP_BRUSHSIDES]);
		#ifndef SERVERONLY
			CMod_LoadTexInfo		(mod, cmod_base, &header.lumps[Q3LUMP_SHADERS]);
			CMod_LoadFaces			(mod, cmod_base, &header.lumps[Q3LUMP_SURFACES]);
//			GLMod_LoadMarksurfaces	(mod, cmod_base, &header.lumps[Q3LUMP_LEAFFACES]);
		#endif
			CMod_LoadVisibility		(mod, cmod_base, &header.lumps[Q3LUMP_VISIBILITY]);
			CModQ3_LoadSubmodels	(mod, cmod_base, &header.lumps[Q3LUMP_MODELS]);
			CModQ3_LoadLeafs		(mod, cmod_base, &header.lumps[Q3LUMP_LEAFS]);
			CModQ3_LoadNodes		(mod, cmod_base, &header.lumps[Q3LUMP_NODES]);
//			CMod_LoadAreas			(mod, cmod_base, &header.lumps[Q3LUMP_AREAS]);
//			CMod_LoadAreaPortals	(mod, cmod_base, &header.lumps[Q3LUMP_AREAPORTALS]);
			CMod_LoadEntityString	(mod, cmod_base, &header.lumps[Q3LUMP_ENTITIES]);
*/

		prv->faces = NULL;

		bspx = BSPX_Setup(mod, mod_base, filelen, header.lumps, Q3LUMPS_TOTAL);

		//q3 maps have built in 4-fold overbright.
		//if we're not rendering with that, we need to brighten the lightmaps in order to keep the darker parts the same brightness. we loose the 2 upper bits. those bright areas become uniform and indistinct.
		//this is used for both the lightmap AND vertex lighting
		//FIXME: when not using overbrights, we suffer a loss of precision.
		gl_overbright.flags |= CVAR_RENDERERLATCH;
		BuildLightMapGammaTable(1, (1<<(2-gl_overbright.ival)));

		prv->mapisq3 = true;
		noerrors = noerrors && CModQ3_LoadShaders				(mod, mod_base, &header.lumps[Q3LUMP_SHADERS]);
		noerrors = noerrors && CModQ3_LoadPlanes				(mod, mod_base, &header.lumps[Q3LUMP_PLANES]);
#ifdef RFBSPS
		if (header.version == BSPVERSION_RBSP)
		{
			noerrors = noerrors && CModRBSP_LoadBrushSides		(mod, mod_base, &header.lumps[Q3LUMP_BRUSHSIDES]);
			noerrors = noerrors && CModRBSP_LoadVertexes		(mod, mod_base, &header.lumps[Q3LUMP_DRAWVERTS]);
		}
		else
#endif
		{
			noerrors = noerrors && CModQ3_LoadBrushSides		(mod, mod_base, &header.lumps[Q3LUMP_BRUSHSIDES]);
			noerrors = noerrors && CModQ3_LoadVertexes			(mod, mod_base, &header.lumps[Q3LUMP_DRAWVERTS]);
		}
		noerrors = noerrors && CModQ3_LoadBrushes				(mod, mod_base, &header.lumps[Q3LUMP_BRUSHES]);
		noerrors = noerrors && CModQ3_LoadLeafBrushes			(mod, mod_base, &header.lumps[Q3LUMP_LEAFBRUSHES]);
#ifdef RFBSPS
		if (header.version == BSPVERSION_RBSP)
			noerrors = noerrors && CModRBSP_LoadFaces			(mod, mod_base, &header.lumps[Q3LUMP_SURFACES]);
		else
#endif
			noerrors = noerrors && CModQ3_LoadFaces				(mod, mod_base, &header.lumps[Q3LUMP_SURFACES]);

		if (noerrors)
			Mod_LoadEntities								(mod, mod_base, &header.lumps[Q3LUMP_ENTITIES]);
#ifndef SERVERONLY
		if (qrenderer != QR_NONE)
		{
#ifdef RFBSPS
			if (header.version == BSPVERSION_RBSP)
				noerrors = noerrors && CModRBSP_LoadLightgrid	(mod, mod_base, &header.lumps[Q3LUMP_LIGHTGRID], &header.lumps[RBSPLUMP_LIGHTINDEXES]);
			else
#endif
				noerrors = noerrors && CModQ3_LoadLightgrid		(mod, mod_base, &header.lumps[Q3LUMP_LIGHTGRID]);
			noerrors = noerrors && CModQ3_LoadIndexes			(mod, mod_base, &header.lumps[Q3LUMP_DRAWINDEXES]);

			if (header.version != BSPVERSION_RTCW)
				noerrors = noerrors && CModQ3_LoadFogs			(mod, mod_base, &header.lumps[Q3LUMP_FOGS]);
			else
				mod->numfogs = 0;

			facedata = (void *)(mod_base + header.lumps[Q3LUMP_SURFACES].fileofs);
#ifdef RFBSPS
			if (header.version == BSPVERSION_RBSP)
			{
				noerrors = noerrors && CModRBSP_LoadRFaces		(mod, mod_base, &header.lumps[Q3LUMP_SURFACES]);
				buildmeshes = CModRBSP_BuildSurfMesh;
				facesize = sizeof(rbspface_t);
				mod->lightmaps.surfstyles = 4;
			}
			else
#endif
			{
				noerrors = noerrors && CModQ3_LoadRFaces		(mod, mod_base, &header.lumps[Q3LUMP_SURFACES]);
				buildmeshes = CModQ3_BuildSurfMesh;
				facesize = sizeof(q3dface_t);
				mod->lightmaps.surfstyles = 1;
			}
			if (noerrors)
			{
				i = header.lumps[Q3LUMP_LIGHTMAPS].filelen / (mod->lightmaps.width*mod->lightmaps.height*3);
				mod->lightmaps.deluxemapping = !(i&1);
				mod->lightmaps.count = max(mod->lightmaps.count, i);
				mod->lightmaps.deluxemapping_modelspace = true;	//we assume true for q3bsp.

				for (i = 0; i < mod->numsurfaces && mod->lightmaps.deluxemapping; i++)
				{
					if (mod->surfaces[i].lightmaptexturenums[0] >= 0 && (mod->surfaces[i].lightmaptexturenums[0] & 1))
						mod->lightmaps.deluxemapping = false;
				}

				{
					char deluxeMaps[64], *key;
					key = (char*)Mod_ParseWorldspawnKey(mod, "deluxeMaps", deluxeMaps, sizeof(deluxeMaps));
					if (*key)
					{
						switch(atoi(key))
						{
						case 0:
							mod->lightmaps.deluxemapping = false;
							break;
						case 1:
		//					mod->lightmaps.deluxemapping = true;
							mod->lightmaps.deluxemapping_modelspace = true;
							break;
						case 2:
		//					mod->lightmaps.deluxemapping = true;
							mod->lightmaps.deluxemapping_modelspace = false;
							break;
						}
					}
				}
			}

			if (noerrors)
				CModQ3_LoadLighting								(mod, mod_base, &header.lumps[Q3LUMP_LIGHTMAPS]);	//fixme: duplicated loading.
		}
#endif
		noerrors = noerrors && CModQ3_LoadMarksurfaces			(mod, mod_base, &header.lumps[Q3LUMP_LEAFSURFACES]);
		noerrors = noerrors && CModQ3_LoadLeafs					(mod, mod_base, &header.lumps[Q3LUMP_LEAFS]);
		noerrors = noerrors && CModQ3_LoadNodes					(mod, mod_base, &header.lumps[Q3LUMP_NODES]);
		noerrors = noerrors && CModQ3_LoadSubmodels				(mod, mod_base, &header.lumps[Q3LUMP_MODELS]);
		noerrors = noerrors && CModQ3_LoadVisibility			(mod, mod_base, &header.lumps[Q3LUMP_VISIBILITY]);

		if (!noerrors)
		{
			if (prv->faces)
				BZ_Free(prv->faces);
			return NULL;
		}

#ifndef CLIENTONLY
		mod->funcs.FatPVS					= Q23BSP_FatPVS;
		mod->funcs.EdictInFatPVS			= Q23BSP_EdictInFatPVS;
		mod->funcs.FindTouchedLeafs		= Q23BSP_FindTouchedLeafs;
#endif
		mod->funcs.ClusterPVS				= CM_ClusterPVS;
		mod->funcs.ClusterForPoint		= CM_PointCluster;

#ifndef SERVERONLY
		mod->funcs.LightPointValues		= GLQ3_LightGrid;
		mod->funcs.StainNode				= GLR_Q2BSP_StainNode;
		mod->funcs.MarkLights				= Q2BSP_MarkLights;
#endif
		mod->funcs.PointContents			= Q2BSP_PointContents;
		mod->funcs.NativeTrace			= CM_NativeTrace;
		mod->funcs.NativeContents			= CM_NativeContents;

#ifndef SERVERONLY
		//light grid info
		if (mod->lightgrid)
		{
			char gridsize[256], *key;
			char val[64];
			float maxs;
			q3lightgridinfo_t *lg = mod->lightgrid;
			key = (char*)Mod_ParseWorldspawnKey(mod, "gridsize", gridsize, sizeof(gridsize));

			key = COM_ParseOut(key, val, sizeof(val));
			lg->gridSize[0] = atof(val);
			key = COM_ParseOut(key, val, sizeof(val));
			lg->gridSize[1] = atof(val);
			key = COM_ParseOut(key, val, sizeof(val));
			lg->gridSize[2] = atof(val);

			if ( lg->gridSize[0] < 1 || lg->gridSize[1] < 1 || lg->gridSize[2] < 1 )
			{
				lg->gridSize[0] = 64;
				lg->gridSize[1] = 64;
				lg->gridSize[2] = 128;
			}

			for ( i = 0; i < 3; i++ )
			{
				lg->gridMins[i] = lg->gridSize[i] * ceil( (prv->cmodels->mins[i] + 1) / lg->gridSize[i] );
				maxs = lg->gridSize[i] * floor( (prv->cmodels->maxs[i] - 1) / lg->gridSize[i] );
				lg->gridBounds[i] = (maxs - lg->gridMins[i])/lg->gridSize[i] + 1;
			}

			lg->gridBounds[3] = lg->gridBounds[1] * lg->gridBounds[0];
		}
#endif

		if (!CM_CreatePatchesForLeafs (mod, prv))	//for clipping
		{
			BZ_Free(prv->faces);
			return NULL;
		}
#ifndef CLIENTONLY
		CMQ3_CalcPHS(mod);
#endif
//			BZ_Free(map_verts);
		BZ_Free(prv->faces);
		break;
#endif
#ifdef Q2BSPS
	case BSPVERSION_Q2:
	case BSPVERSION_Q2W:
		mod->lightmaps.width = LMBLOCK_SIZE_MAX;
		mod->lightmaps.height = LMBLOCK_SIZE_MAX;

		prv->mapisq3 = false;
		mod->engineflags |= MDLF_NEEDOVERBRIGHT;
		for (i=0 ; i<Q2HEADER_LUMPS ; i++)
		{
			header.lumps[i].filelen = LittleLong (header.lumps[i].filelen);
			header.lumps[i].fileofs = LittleLong (header.lumps[i].fileofs);
		}
		if (header.version == BSPVERSION_Q2W)
		{
			header.lumps[i].filelen = LittleLong (header.lumps[i].filelen);
			header.lumps[i].fileofs = LittleLong (header.lumps[i].fileofs);
			i++;
		}
		Q1BSPX_Setup(mod, mod_base, filelen, header.lumps, i);

#ifndef SERVERONLY
		if (CM_GetQ2Palette())
			memcpy(d_q28to24table, host_basepal, 768);
#endif

		switch(qrenderer)
		{
		case QR_NONE:	//dedicated only
			noerrors = noerrors && CModQ2_LoadSurfaces		(mod, mod_base, &header.lumps[Q2LUMP_TEXINFO]);
			noerrors = noerrors && CModQ2_LoadPlanes		(mod, mod_base, &header.lumps[Q2LUMP_PLANES]);
			noerrors = noerrors && CModQ2_LoadVisibility	(mod, mod_base, &header.lumps[Q2LUMP_VISIBILITY]);
			noerrors = noerrors && CModQ2_LoadBrushSides	(mod, mod_base, &header.lumps[Q2LUMP_BRUSHSIDES]);
			noerrors = noerrors && CModQ2_LoadBrushes		(mod, mod_base, &header.lumps[Q2LUMP_BRUSHES]);
			noerrors = noerrors && CModQ2_LoadLeafBrushes	(mod, mod_base, &header.lumps[Q2LUMP_LEAFBRUSHES]);
			noerrors = noerrors && CModQ2_LoadLeafs			(mod, mod_base, &header.lumps[Q2LUMP_LEAFS]);
			noerrors = noerrors && CModQ2_LoadNodes			(mod, mod_base, &header.lumps[Q2LUMP_NODES]);
			noerrors = noerrors && CModQ2_LoadSubmodels		(mod, mod_base, &header.lumps[Q2LUMP_MODELS]);
			noerrors = noerrors && CModQ2_LoadAreas			(mod, mod_base, &header.lumps[Q2LUMP_AREAS]);
			noerrors = noerrors && CModQ2_LoadAreaPortals	(mod, mod_base, &header.lumps[Q2LUMP_AREAPORTALS]);
			if (noerrors)
				Mod_LoadEntities							(mod, mod_base, &header.lumps[Q2LUMP_ENTITIES]);

#ifndef CLIENTONLY
			mod->funcs.FatPVS				= Q23BSP_FatPVS;
			mod->funcs.EdictInFatPVS		= Q23BSP_EdictInFatPVS;
			mod->funcs.FindTouchedLeafs		= Q23BSP_FindTouchedLeafs;
#endif
			mod->funcs.LightPointValues		= NULL;
			mod->funcs.StainNode			= NULL;
			mod->funcs.MarkLights			= NULL;
			mod->funcs.ClusterPVS			= CM_ClusterPVS;
			mod->funcs.ClusterForPoint		= CM_PointCluster;
			mod->funcs.PointContents		= Q2BSP_PointContents;
			mod->funcs.NativeTrace			= CM_NativeTrace;
			mod->funcs.NativeContents		= CM_NativeContents;

			break;
#ifndef SERVERONLY
		default:
		// load into heap
			noerrors = noerrors && Mod_LoadVertexes			(mod, mod_base, &header.lumps[Q2LUMP_VERTEXES]);
			if (header.version == BSPVERSION_Q2W)
				/*noerrors = noerrors &&*/ Mod_LoadVertexNormals(mod, mod_base, &header.lumps[19]);
			noerrors = noerrors && Mod_LoadEdges			(mod, mod_base, &header.lumps[Q2LUMP_EDGES], false);
			noerrors = noerrors && Mod_LoadSurfedges		(mod, mod_base, &header.lumps[Q2LUMP_SURFEDGES]);
			if (noerrors)
				Mod_LoadLighting							(mod, mod_base, &header.lumps[Q2LUMP_LIGHTING], header.version == BSPVERSION_Q2W, NULL);
			noerrors = noerrors && CModQ2_LoadSurfaces		(mod, mod_base, &header.lumps[Q2LUMP_TEXINFO]);
			noerrors = noerrors && CModQ2_LoadPlanes		(mod, mod_base, &header.lumps[Q2LUMP_PLANES]);
			noerrors = noerrors && CModQ2_LoadTexInfo		(mod, mod_base, &header.lumps[Q2LUMP_TEXINFO], loadname);
			if (noerrors)
				Mod_LoadEntities							(mod, mod_base, &header.lumps[Q2LUMP_ENTITIES]);
			noerrors = noerrors && CModQ2_LoadFaces			(mod, mod_base, &header.lumps[Q2LUMP_FACES], header.version == BSPVERSION_Q2W);
			noerrors = noerrors && Mod_LoadMarksurfaces		(mod, mod_base, &header.lumps[Q2LUMP_LEAFFACES], false);
			noerrors = noerrors && CModQ2_LoadVisibility	(mod, mod_base, &header.lumps[Q2LUMP_VISIBILITY]);
			noerrors = noerrors && CModQ2_LoadBrushSides	(mod, mod_base, &header.lumps[Q2LUMP_BRUSHSIDES]);
			noerrors = noerrors && CModQ2_LoadBrushes		(mod, mod_base, &header.lumps[Q2LUMP_BRUSHES]);
			noerrors = noerrors && CModQ2_LoadLeafBrushes	(mod, mod_base, &header.lumps[Q2LUMP_LEAFBRUSHES]);
			noerrors = noerrors && CModQ2_LoadLeafs			(mod, mod_base, &header.lumps[Q2LUMP_LEAFS]);
			noerrors = noerrors && CModQ2_LoadNodes			(mod, mod_base, &header.lumps[Q2LUMP_NODES]);
			noerrors = noerrors && CModQ2_LoadSubmodels		(mod, mod_base, &header.lumps[Q2LUMP_MODELS]);
			noerrors = noerrors && CModQ2_LoadAreas			(mod, mod_base, &header.lumps[Q2LUMP_AREAS]);
			noerrors = noerrors && CModQ2_LoadAreaPortals	(mod, mod_base, &header.lumps[Q2LUMP_AREAPORTALS]);

			if (!noerrors)
			{
				return NULL;
			}
#ifndef CLIENTONLY
			mod->funcs.FatPVS				= Q23BSP_FatPVS;
			mod->funcs.EdictInFatPVS		= Q23BSP_EdictInFatPVS;
			mod->funcs.FindTouchedLeafs		= Q23BSP_FindTouchedLeafs;
#endif
			mod->funcs.LightPointValues		= GLQ2BSP_LightPointValues;
			mod->funcs.StainNode			= GLR_Q2BSP_StainNode;
			mod->funcs.MarkLights			= Q2BSP_MarkLights;
			mod->funcs.ClusterPVS			= CM_ClusterPVS;
			mod->funcs.ClusterForPoint		= CM_PointCluster;
			mod->funcs.PointContents		= Q2BSP_PointContents;
			mod->funcs.NativeTrace			= CM_NativeTrace;
			mod->funcs.NativeContents		= CM_NativeContents;
			break;
#endif
		}
#endif
	}

	BSPX_LoadEnvmaps(mod, bspx, mod_base);

	if (map_autoopenportals.value)
		memset (prv->portalopen, 1, sizeof(prv->portalopen));	//open them all. Used for progs that havn't got a clue.
	else
		memset (prv->portalopen, 0, sizeof(prv->portalopen));	//make them start closed.
	FloodAreaConnections (prv);

	mod->checksum = mod->checksum2 = *checksum;

	mod->nummodelsurfaces = mod->numsurfaces;
	memset(&mod->batches, 0, sizeof(mod->batches));
	mod->vbos = NULL;

	mod->numsubmodels = CM_NumInlineModels(mod);

	mod->hulls[0].firstclipnode = prv->cmodels[0].headnode-mod->nodes;
	mod->rootnode = prv->cmodels[0].headnode;
	mod->nummodelsurfaces = prv->cmodels[0].numsurfaces;

#ifndef SERVERONLY
	if (qrenderer != QR_NONE)
	{
		builddata_t *bd = NULL;
		if (buildmeshes)
		{
			bd = BZ_Malloc(sizeof(*bd) + facesize*mod->nummodelsurfaces);
			bd->buildfunc = buildmeshes;
			memcpy(bd+1, facedata + mod->firstmodelsurface*facesize, facesize*mod->nummodelsurfaces);
		}
		COM_AddWork(WG_MAIN, ModBrush_LoadGLStuff, mod, bd, 0, 0);
	}
#endif

	for (i=1 ; i< mod->numsubmodels ; i++)
	{
		cmodel_t	*bm;

		char	name[MAX_QPATH];

		Q_snprintfz (name, sizeof(name), "*%i:%s", i, wmod->publicname);
		mod = Mod_FindName (name);
		*mod = *wmod;
		mod->entities_raw = NULL;
		mod->submodelof = wmod;
		Q_strncpyz(mod->publicname, name, sizeof(mod->publicname));
		Q_snprintfz (mod->name, sizeof(mod->name), "*%i:%s", i, wmod->name);
		memset(&mod->memgroup, 0, sizeof(mod->memgroup));

		bm = CM_InlineModel (wmod, name);


		
		mod->hulls[0].firstclipnode = -1;	//no nodes, 
		if (bm->headleaf)
		{
			mod->leafs = bm->headleaf;
			mod->nodes = NULL;
			mod->hulls[0].firstclipnode = -1;	//make it refer directly to the first leaf, for things that still use numbers. 
			mod->rootnode = (mnode_t*)bm->headleaf;
		}
		else
		{
			mod->leafs = wmod->leafs;
			mod->nodes = wmod->nodes;
			mod->hulls[0].firstclipnode = bm->headnode - mod->nodes;	//determine the correct node index
			mod->rootnode = bm->headnode;
		}
		mod->nummodelsurfaces = bm->numsurfaces;
		mod->firstmodelsurface = bm->firstsurface;

		memset(&mod->batches, 0, sizeof(mod->batches));
		mod->vbos = NULL;

		VectorCopy (bm->maxs, mod->maxs);
		VectorCopy (bm->mins, mod->mins);
#ifndef SERVERONLY
		mod->radius = RadiusFromBounds (mod->mins, mod->maxs);

		if (qrenderer != QR_NONE)
		{
			builddata_t *bd = NULL;
			if (buildmeshes)
			{
				bd = BZ_Malloc(sizeof(*bd) + facesize*mod->nummodelsurfaces);
				bd->buildfunc = buildmeshes;
				memcpy(bd+1, facedata + mod->firstmodelsurface*facesize, facesize*mod->nummodelsurfaces);
			}
			COM_AddWork(WG_MAIN, ModBrush_LoadGLStuff, mod, bd, i, 0);
		}
#endif
		COM_AddWork(WG_MAIN, Mod_ModelLoaded, mod, NULL, MLS_LOADED, 0);
	}

#ifdef TERRAIN
	mod->terrain = Mod_LoadTerrainInfo(mod, loadname, false);
#endif

	return &prv->cmodels[0];
}

/*
==================
CM_InlineModel
==================
*/
static cmodel_t	*CM_InlineModel (model_t *model, char *name)
{
	cminfo_t	*prv = (cminfo_t*)model->meshinfo;
	int		num;

	if (!name)
		Host_Error("Bad model\n");
	else if (name[0] != '*')
		Host_Error("Bad model\n");

	num = atoi (name+1);

	if (num < 1 || num >= prv->numcmodels)
		Host_Error ("CM_InlineModel: bad number");

	return &prv->cmodels[num];
}

int		CM_ClusterBytes (model_t *model)
{
#ifdef Q3BSPS
	if (model->fromgame == fg_quake3)
	{
		cminfo_t	*prv = (cminfo_t*)model->meshinfo;
		return prv->q3pvs->rowsize ? prv->q3pvs->rowsize : model->pvsbytes;
	}
	else
#endif
		return model->pvsbytes;
}

static int		CM_NumInlineModels (model_t *model)
{
	cminfo_t	*prv = (cminfo_t*)model->meshinfo;
	return prv->numcmodels;
}

int		CM_LeafContents (model_t *model, int leafnum)
{
	if (leafnum < 0 || leafnum >= model->numleafs)
		Host_Error ("CM_LeafContents: bad number");
	return model->leafs[leafnum].contents;
}

int		CM_LeafCluster (model_t *model, int leafnum)
{
	if (leafnum < 0 || leafnum >= model->numleafs)
		Host_Error ("CM_LeafCluster: bad number");
	return model->leafs[leafnum].cluster;
}

int		CM_LeafArea (model_t *model, int leafnum)
{
	if (leafnum < 0 || leafnum >= model->numleafs)
		Host_Error ("CM_LeafArea: bad number");
	return model->leafs[leafnum].area;
}

//=======================================================================

#define PlaneDiff(point,plane) (((plane)->type < 3 ? (point)[(plane)->type] : DotProduct((point), (plane)->normal)) - (plane)->dist)

mplane_t		box_planes[6];
model_t			box_model;
q2cbrush_t		box_brush;
q2cbrushside_t	box_sides[6];
static qboolean BM_NativeTrace(model_t *model, int forcehullnum, framestate_t *framestate, vec3_t axis[3], vec3_t start, vec3_t end, vec3_t mins, vec3_t maxs, qboolean capsule, unsigned int contents, trace_t *trace);
static unsigned int BM_NativeContents(struct model_s *model, int hulloverride, framestate_t *framestate, vec3_t axis[3], vec3_t p, vec3_t mins, vec3_t maxs)
{
	unsigned int j;
	q2cbrushside_t *brushside = box_sides;
	for ( j = 0; j < 6; j++, brushside++ )
	{
		if ( PlaneDiff (p, brushside->plane) > 0 )
			return 0;
	}
	return FTECONTENTS_BODY;
}

/*
===================
CM_InitBoxHull

Set up the planes and nodes so that the six floats of a bounding box
can just be stored out and get a proper clipping hull structure.
===================
*/
void CM_InitBoxHull (void)
{
	int			i;
	mplane_t	*p;
	q2cbrushside_t	*s;

/*
#ifndef CLIENTONLY
	box_model.funcs.FatPVS				= Q2BSP_FatPVS;
	box_model.funcs.EdictInFatPVS		= Q2BSP_EdictInFatPVS;
	box_model.funcs.FindTouchedLeafs	= Q23BSP_FindTouchedLeafs;
#endif

#ifndef SERVERONLY
	box_model.funcs.MarkLights			= Q2BSP_MarkLights;
#endif
	box_model.funcs.ClusterPVS			= CM_ClusterPVS;
	box_model.funcs.ClusterForPoint		= CM_PointCluster;
*/
	box_model.funcs.NativeContents		= BM_NativeContents;
	box_model.funcs.NativeTrace			= BM_NativeTrace;


	box_model.loadstate = MLS_LOADED;

	box_brush.contents = FTECONTENTS_BODY;
	box_brush.numsides = 6;
	box_brush.brushside = box_sides;

	for (i=0 ; i<6 ; i++)
	{
		//the pointers
		s = &box_sides[i];
		p = &box_planes[i];

		// brush sides
		s->plane = 	p;
		s->surface = &nullsurface;

		// planes
		p->type = ((i>=3)?i-3:i);
		p->signbits = 0;
		VectorClear (p->normal);
		p->normal[p->type] = ((i>=3)?-1:1);
	}
}


/*
===================
CM_HeadnodeForBox

To keep everything totally uniform, bounding boxes are turned into small
BSP trees instead of being compared directly.
===================
*/
void CM_SetTempboxSize (vec3_t mins, vec3_t maxs)
{
	box_planes[0].dist = maxs[0];
	box_planes[1].dist = maxs[1];
	box_planes[2].dist = maxs[2];
	box_planes[3].dist = -mins[0];
	box_planes[4].dist = -mins[1];
	box_planes[5].dist = -mins[2];
}

model_t *CM_TempBoxModel(vec3_t mins, vec3_t maxs)
{
	CM_SetTempboxSize(mins, maxs);
	return &box_model;
}

/*
==================
CM_PointLeafnum_r

==================
*/
static int CM_PointLeafnum_r (model_t *mod, vec3_t p, int num)
{
	float		d;
	mnode_t		*node;
	mplane_t	*plane;

	while (num >= 0)
	{
		node = mod->nodes + num;
		plane = node->plane;

		if (plane->type < 3)
			d = p[plane->type] - plane->dist;
		else
			d = DotProduct (plane->normal, p) - plane->dist;
		if (d < 0)
			num = node->childnum[1];
		else
			num = node->childnum[0];
	}

	return -1 - num;
}

int CM_PointLeafnum (model_t *mod, vec3_t p)
{
	if (!mod || mod->loadstate != MLS_LOADED)
		return 0;		// sound may call this without map loaded
	return CM_PointLeafnum_r (mod, p, 0);
}

static int CM_PointCluster (model_t *mod, vec3_t p)
{
	if (!mod || mod->loadstate != MLS_LOADED)
		return 0;		// sound may call this without map loaded
	return CM_LeafCluster(mod, CM_PointLeafnum_r (mod, p, 0));
}

/*
=============
CM_BoxLeafnums

Fills in a list of all the leafs touched
=============
*/
int		leaf_count, leaf_maxcount;
int		*leaf_list;
float	*leaf_mins, *leaf_maxs;
int		leaf_topnode;

void CM_BoxLeafnums_r (model_t *mod, int nodenum)
{
	mplane_t	*plane;
	mnode_t		*node;
	int		s;

	while (1)
	{
		if (nodenum < 0)
		{
			if (leaf_count >= leaf_maxcount)
			{
//				Com_Printf ("CM_BoxLeafnums_r: overflow\n");
				return;
			}
			leaf_list[leaf_count++] = -1 - nodenum;
			return;
		}

		node = &mod->nodes[nodenum];
		plane = node->plane;
//		s = BoxOnPlaneSide (leaf_mins, leaf_maxs, plane);
		s = BOX_ON_PLANE_SIDE(leaf_mins, leaf_maxs, plane);
		if (s == 1)
			nodenum = node->childnum[0];
		else if (s == 2)
			nodenum = node->childnum[1];
		else
		{	// go down both
			if (leaf_topnode == -1)
				leaf_topnode = nodenum;
			CM_BoxLeafnums_r (mod, node->childnum[0]);
			nodenum = node->childnum[1];
		}

	}
}

int	CM_BoxLeafnums_headnode (model_t *mod, vec3_t mins, vec3_t maxs, int *list, int listsize, int headnode, int *topnode)
{
	leaf_list = list;
	leaf_count = 0;
	leaf_maxcount = listsize;
	leaf_mins = mins;
	leaf_maxs = maxs;

	leaf_topnode = -1;

	CM_BoxLeafnums_r (mod, headnode);

	if (topnode)
		*topnode = leaf_topnode;

	return leaf_count;
}

int	CM_BoxLeafnums (model_t *mod, vec3_t mins, vec3_t maxs, int *list, int listsize, int *topnode)
{
	return CM_BoxLeafnums_headnode (mod, mins, maxs, list,
		listsize, mod->hulls[0].firstclipnode, topnode);
}



/*
==================
CM_PointContents

==================
*/
int CM_PointContents (model_t *mod, vec3_t p)
{
	cminfo_t		*prv = (cminfo_t*)mod->meshinfo;
	int				i, j, contents;
	mleaf_t			*leaf;
	q2cbrush_t		*brush;
	q2cbrushside_t	*brushside;

	if (!mod)	// map not loaded
		return 0;

	i = CM_PointLeafnum_r (mod, p, mod->hulls[0].firstclipnode);

	if (mod->fromgame == fg_quake2)
		contents = mod->leafs[i].contents;	//q2 is simple.
	else
	{
		leaf = &mod->leafs[i];

	//	if ( leaf->contents & CONTENTS_NODROP ) {
	//		contents = CONTENTS_NODROP;
	//	} else {
			contents = 0;
	//	}

		for (i = 0; i < leaf->numleafbrushes; i++)
		{
			brush = prv->leafbrushes[leaf->firstleafbrush + i];

			// check if brush actually adds something to contents
			if ( (contents & brush->contents) == brush->contents ) {
				continue;
			}

			brushside = brush->brushside;
			for ( j = 0; j < brush->numsides; j++, brushside++ )
			{
				if ( PlaneDiff (p, brushside->plane) > 0 )
					break;
			}

			if (j == brush->numsides)
				contents |= brush->contents;
		}
	}
#ifdef TERRAIN
	if (mod->terrain)
		contents |= Heightmap_PointContents(mod, NULL, p);
#endif
	return contents;
}

unsigned int CM_NativeContents(struct model_s *model, int hulloverride, framestate_t *framestate, vec3_t axis[3], vec3_t p, vec3_t mins, vec3_t maxs)
{
	cminfo_t	*prv = (cminfo_t*)model->meshinfo;
	int	contents;
	if (!DotProduct(mins, mins) && !DotProduct(maxs, maxs))
		return CM_PointContents(model, p);

	if (!model)	// map not loaded
		return 0;


	{
		int i, j, k;
		mleaf_t			*leaf;
		q2cbrush_t		*brush;
		q2cbrushside_t	*brushside;
		vec3_t absmin, absmax;

		int leaflist[64];

		k = CM_BoxLeafnums (model, absmin, absmax, leaflist, 64, NULL);

		contents = 0;
		for (k--; k >= 0; k--)
		{
			leaf = &model->leafs[leaflist[k]];
			if (model->fromgame != fg_quake2)
			{	//q3 is more complex
				for (i = 0; i < leaf->numleafbrushes; i++)
				{
					brush = prv->leafbrushes[leaf->firstleafbrush + i];

					// check if brush actually adds something to contents
					if ( (contents & brush->contents) == brush->contents ) {
						continue;
					}

					brushside = brush->brushside;
					for ( j = 0; j < brush->numsides; j++, brushside++ )
					{
						if ( PlaneDiff (p, brushside->plane) > 0 )
							break;
					}

					if (j == brush->numsides)
						contents |= brush->contents;
				}
			}
			else	//q2 is simple
				contents |= leaf->contents;
		}
	}

	return contents;
}

/*
==================
CM_TransformedPointContents

Handles offseting and rotation of the end points for moving and
rotating entities
==================
*/
int	CM_TransformedPointContents (model_t *mod, vec3_t p, int headnode, vec3_t origin, vec3_t angles)
{
	vec3_t		p_l;
	vec3_t		temp;
	vec3_t		forward, right, up;

	// subtract origin offset
	VectorSubtract (p, origin, p_l);

	// rotate start and end into the models frame of reference
	if (angles[0] || angles[1] || angles[2])
	{
		AngleVectors (angles, forward, right, up);

		VectorCopy (p_l, temp);
		p_l[0] = DotProduct (temp, forward);
		p_l[1] = -DotProduct (temp, right);
		p_l[2] = DotProduct (temp, up);
	}

	return CM_PointContents(mod, p);
}


/*
===============================================================================

BOX TRACING

===============================================================================
*/

// 1/32 epsilon to keep floating point happy
#define	DIST_EPSILON	(0.03125)

static vec3_t	trace_start, trace_end;
static vec3_t	trace_mins, trace_maxs;
static vec3_t	trace_extents;
static vec3_t	trace_absmins, trace_absmaxs;
static vec3_t	trace_up;	//capsule points upwards in this direction
static vec3_t	trace_capsulesize;	//radius, up, down
static float	trace_truefraction;
static float	trace_nearfraction;

static trace_t	trace_trace;
static int		trace_contents;
static enum
{
	shape_isbox,
	shape_iscapsule,
	shape_ispoint
} trace_shape;		// optimized case


static void CM_FinalizeBrush(q2cbrush_t *brush)
{
	vecV_t verts[256];
	vec4_t planes[256];
	int i, j;
	ClearBounds(brush->absmins, brush->absmaxs);
	for (i = 0; i < brush->numsides; i++)
	{
		VectorCopy(brush->brushside[i].plane->normal, planes[i]);
		planes[i][3] = brush->brushside[i].plane->dist;
	}
	for (i = 0; i < brush->numsides; i++)
	{
		j = Fragment_ClipPlaneToBrush(verts, countof(verts), planes, sizeof(planes[0]), brush->numsides, planes[i]);
		while (j-- > 0)
			AddPointToBounds(verts[j], brush->absmins, brush->absmaxs);
	}
}

/*
================
CM_ClipBoxToBrush
================
*/
static void CM_ClipBoxToBrush (vec3_t mins, vec3_t maxs, vec3_t p1, vec3_t p2,
					  trace_t *trace, q2cbrush_t *brush)
{
	int			i, j;
	mplane_t	*plane, *clipplane;
	float		dist;
	float		enterfrac, leavefrac;
	vec3_t		ofs;
	float		d1, d2;
	qboolean	getout, startout;
	float		f;
	q2cbrushside_t	*side, *leadside;

	float nearfrac=0;
	enterfrac = -1;
	leavefrac = 2;
	clipplane = NULL;

	if (!brush->numsides)
		return;

	getout = false;
	startout = false;
	leadside = NULL;

	for (i=0 ; i<brush->numsides ; i++)
	{
		side = brush->brushside+i;
		plane = side->plane;

		switch(trace_shape)
		{
		default:
		case shape_isbox: // general box case
			// push the plane out apropriately for mins/maxs

			// FIXME: use signbits into 8 way lookup for each mins/maxs
			for (j=0 ; j<3 ; j++)
			{
				if (plane->normal[j] < 0)
					ofs[j] = maxs[j];
				else
					ofs[j] = mins[j];
			}
			dist = DotProduct (ofs, plane->normal);
			dist = plane->dist - dist;
			break;
		capsuledist(dist,plane,mins,maxs)
		case shape_ispoint: // special point case
			dist = plane->dist;
			break;
		}

		d1 = DotProduct (p1, plane->normal) - dist;
		d2 = DotProduct (p2, plane->normal) - dist;

		if (d2 > 0)
			getout = true;	// endpoint is not in solid
		if (d1 > 0)
			startout = true;

		// if completely in front of face, no intersection
		if (d1 > 0 && d2 >= d1)
			return;

		if (d1 <= 0 && d2 <= 0)
			continue;

		// crosses face
		if (d1 > d2)
		{	// enter
			f = (d1) / (d1-d2);
			if (f > enterfrac)
			{
				enterfrac = f;
				nearfrac = (d1-DIST_EPSILON) / (d1-d2);
				clipplane = plane;
				leadside = side;
			}
		}
		else
		{	// leave
			f = (d1) / (d1-d2);
			if (f < leavefrac)
				leavefrac = f;
		}
	}

	if (!startout)
	{	// original point was inside brush
		trace->startsolid = true;
		if (!getout)
			trace->allsolid = true;
		return;
	}
	if (enterfrac <= leavefrac)
	{
		if (enterfrac > -1 && enterfrac <= trace_truefraction)
		{
			if (enterfrac < 0)
				enterfrac = 0;

			trace_nearfraction = nearfrac;
			trace_truefraction = enterfrac;

			trace->plane.dist = clipplane->dist;
			VectorCopy(clipplane->normal, trace->plane.normal);
			trace->surface = &(leadside->surface->c);
			trace->contents = brush->contents;
		}
	}
}

#ifdef Q3BSPS
static void CM_ClipBoxToPlanes (vec3_t trmins, vec3_t trmaxs, vec3_t p1, vec3_t p2, trace_t *trace, vec3_t plmins, vec3_t plmaxs, mplane_t *plane, int numplanes, q2csurface_t *surf)
{
	int			i, j;
	mplane_t	*clipplane;
	float		dist;
	float		enterfrac, leavefrac;
	vec3_t		ofs;
	float		d1, d2;
	qboolean	getout, startout;
	float		f;
//	q2cbrushside_t	*side, *leadside;
	static mplane_t	bboxplanes[6] = //we change the dist, but nothing else
	{
		{{1, 0, 0}},
		{{0, 1, 0}},
		{{0, 0, 1}},
		{{-1, 0, 0}},
		{{0, -1, 0}},
		{{0, 0, -1}},
	};

	float nearfrac=0;
	enterfrac = -1;
	leavefrac = 2;
	clipplane = NULL;

	getout = false;
	startout = false;
//	leadside = NULL;

	for (i=0 ; i<numplanes ; i++, plane++)
	{
		switch(trace_shape)
		{
		default:
		case shape_isbox:	// general box case
			// push the plane out apropriately for mins/maxs

			// FIXME: special case for axial
			// FIXME: use signbits into 8 way lookup for each mins/maxs
			for (j=0 ; j<3 ; j++)
			{
				if (plane->normal[j] < 0)
					ofs[j] = trmaxs[j];
				else
					ofs[j] = trmins[j];
			}
			dist = DotProduct (ofs, plane->normal);
			dist = plane->dist - dist;
			break;
		capsuledist(dist,plane,trmins,trmaxs)
		case shape_ispoint:	// special point case
			dist = plane->dist;
			break;
		}

		d1 = DotProduct (p1, plane->normal) - dist;
		d2 = DotProduct (p2, plane->normal) - dist;

		if (d2 > 0)
			getout = true;	// endpoint is not in solid
		if (d1 > 0)
			startout = true;

		// if completely in front of face, no intersection
		if (d1 > 0 && d2 >= d1)
			return;

		if (d1 <= 0 && d2 <= 0)
			continue;

		// crosses face
		if (d1 > d2)
		{	// enter
			f = (d1) / (d1-d2);
			if (f > enterfrac)
			{
				enterfrac = f;
				nearfrac = (d1-DIST_EPSILON) / (d1-d2);
				clipplane = plane;
//				leadside = side;
			}
		}
		else
		{	// leave
			f = (d1) / (d1-d2);
			if (f < leavefrac)
				leavefrac = f;
		}
	}

	//bevel the brush axially (to match the player's bbox), in case that wasn't already done
	for (i=0, plane = bboxplanes; i<6 ; i++, plane++)
	{
		if (i < 3)
		{	//positive normal
			dist = trmins[i];
			plane->dist = plmaxs[i];
			dist = plane->dist - dist;
			d1 = p1[i] - dist;
			d2 = p2[i] - dist;
		}
		else
		{	//negative normal
			j = i-3;
			dist = -trmaxs[j];
			plane->dist = -plmins[j];
			dist = plane->dist - dist;
			d1 = -p1[j] - dist;
			d2 = -p2[j] - dist;
		}

		if (d2 > 0)
			getout = true;	// endpoint is not in solid
		if (d1 > 0)
			startout = true;

		// if completely in front of face, no intersection
		if (d1 > 0 && d2 >= d1)
			return;

		if (d1 <= 0 && d2 <= 0)
			continue;

		// crosses face
		if (d1 > d2)
		{	// enter
			f = (d1) / (d1-d2);
			if (f > enterfrac)
			{
				enterfrac = f;
				nearfrac = (d1-DIST_EPSILON) / (d1-d2);
				clipplane = plane;
//				leadside = side;
			}
		}
		else
		{	// leave
			f = (d1) / (d1-d2);
			if (f < leavefrac)
				leavefrac = f;
		}
	}

	if (!startout)
	{	// original point was inside brush
		trace->startsolid = true;
		if (!getout)
			trace->allsolid = true;
		return;
	}
	if (enterfrac <= leavefrac)
	{
		if (enterfrac > -1 && enterfrac <= trace_truefraction)
		{
			if (enterfrac < 0)
				enterfrac = 0;

			trace_nearfraction = nearfrac;
			trace_truefraction = enterfrac;

			trace->plane.dist = clipplane->dist;
			VectorCopy(clipplane->normal, trace->plane.normal);
			trace->surface = surf;
			trace->contents = surf->value;
		}
	}
}

static void Mod_Trace_Trisoup_(vecV_t *posedata, index_t *indexes, size_t numindexes, vec3_t start, vec3_t end, vec3_t mins, vec3_t maxs, trace_t *trace, q2csurface_t *surf)
{
	size_t i;
	int j;
	float *p1, *p2, *p3;
	vec3_t edge1, edge2, edge3;
	mplane_t planes[5];
	vec3_t tmins, tmaxs;

	for (i = 0; i < numindexes; i+=3)
	{
		p1 = posedata[indexes[i+0]];
		p2 = posedata[indexes[i+1]];
		p3 = posedata[indexes[i+2]];

		//determine the triangle extents, and skip the triangle if we're completely out of bounds
		for (j = 0; j < 3; j++)
		{
			tmins[j] = p1[j];
			if (tmins[j] > p2[j])
				tmins[j] = p2[j];
			if (tmins[j] > p3[j])
				tmins[j] = p3[j];
			if (trace_absmaxs[j]+(1/8.f) < tmins[j])
				break;
			tmaxs[j] = p1[j];
			if (tmaxs[j] < p2[j])
				tmaxs[j] = p2[j];
			if (tmaxs[j] < p3[j])
				tmaxs[j] = p3[j];
			if (trace_absmins[j]-(1/8.f) > tmaxs[j])
				break;
		}
		//skip any triangles which are completely outside the trace bounds
		if (j < 3)
			continue;

		VectorSubtract(p1, p2, edge1);
		VectorSubtract(p3, p2, edge2);
		VectorSubtract(p1, p3, edge3);
		CrossProduct(edge1, edge2, planes[0].normal);
		VectorNormalize(planes[0].normal);
		planes[0].dist = DotProduct(p1, planes[0].normal);
		VectorNegate(planes[0].normal, planes[1].normal);
		planes[1].dist = -planes[0].dist + 4;

		//determine edges
		//FIXME: use adjacency info
		CrossProduct(edge1, planes[0].normal, planes[2].normal);
		VectorNormalize(planes[2].normal);
		planes[2].dist = DotProduct(p2, planes[2].normal);

		CrossProduct(planes[0].normal, edge2, planes[3].normal);
		VectorNormalize(planes[3].normal);
		planes[3].dist = DotProduct(p3, planes[3].normal);

		CrossProduct(planes[0].normal, edge3, planes[4].normal);
		VectorNormalize(planes[4].normal);
		planes[4].dist = DotProduct(p1, planes[4].normal);

		CM_ClipBoxToPlanes(mins, maxs, start, end, trace, tmins, tmaxs, planes, 5, surf); 
	}
}

/*
static void CM_ClipBoxToMesh (vec3_t mins, vec3_t maxs, vec3_t p1, vec3_t p2, trace_t *trace, mesh_t *mesh)
{
	trace_truefraction = trace->truefraction;
	trace_nearfraction = trace->fraction;
	Mod_Trace_Trisoup_(mesh->xyz_array, mesh->indexes, mesh->numindexes, p1, p2, mins, maxs, trace, &nullsurface.c);
	trace->truefraction = trace_truefraction;
	trace->fraction = trace_nearfraction;
}
*/

static void CM_ClipBoxToPatch (vec3_t mins, vec3_t maxs, vec3_t p1, vec3_t p2,
					  trace_t *trace, q2cbrush_t *brush)
{
	int			i, j;
	mplane_t	*plane, *clipplane;
	float		enterfrac, leavefrac, nearfrac = 0;
	vec3_t		ofs;
	float		d1, d2;
	float dist;
	qboolean	startout;
	float		f;
	q2cbrushside_t	*side, *leadside;

	if (!brush->numsides)
		return;

	enterfrac = -1;
	leavefrac = 2;
	clipplane = NULL;
	startout = false;
	leadside = NULL;

	for (i=0 ; i<brush->numsides ; i++)
	{
		side = brush->brushside+i;
		plane = side->plane;

		// push the plane out apropriately for mins/maxs
		switch(trace_shape)
		{
		default:
		case shape_isbox:	// general box case
			// FIXME: use signbits into 8 way lookup for each mins/maxs
			for (j=0 ; j<3 ; j++)
			{
				if (plane->normal[j] < 0)
					ofs[j] = maxs[j];
				else
					ofs[j] = mins[j];
			}
			dist = DotProduct (ofs, plane->normal);
			dist = plane->dist - dist;
			break;
		capsuledist(dist,plane,mins,maxs)
		case shape_ispoint:	// special point case
			dist = plane->dist;
			break;
		}

		d1 = DotProduct (p1, plane->normal) - dist;
		d2 = DotProduct (p2, plane->normal) - dist;

		// if completely in front of face, no intersection
		if (d1 > 0 && d2 >= d1)
			return;

		if (d1 > 0)
			startout = true;

		if (d1 <= 0 && d2 <= 0)
			continue;

		// crosses face
		if (d1 > d2)
		{	// enter
			f = (d1) / (d1-d2);
			if (f > enterfrac)
			{
				enterfrac = f;
				nearfrac = (d1-DIST_EPSILON) / (d1-d2);
				clipplane = plane;
				leadside = side;
			}
		}
		else
		{	// leave
			f = (d1) / (d1-d2);
			if (f < leavefrac)
				leavefrac = f;
		}
	}

	if (!startout)
	{
		trace->startsolid = true;
		return;		// original point is inside the patch
	}

	if (nearfrac <= leavefrac)
	{
		if (leadside && leadside->surface
			&& enterfrac <= trace_truefraction)
		{
			if (enterfrac < 0)
				enterfrac = 0;
			trace_truefraction = enterfrac;
			trace_nearfraction = nearfrac;
			trace->plane.dist = clipplane->dist;
			VectorCopy(clipplane->normal, trace->plane.normal);
			trace->surface = &leadside->surface->c;
			trace->contents = brush->contents;
		}
		else if (enterfrac < trace_truefraction)
			leavefrac=0;
	}
}
#endif

/*
================
CM_TestBoxInBrush
================
*/
static void CM_TestBoxInBrush (vec3_t mins, vec3_t maxs, vec3_t p1,
					  trace_t *trace, q2cbrush_t *brush)
{
	int			i, j;
	mplane_t	*plane;
	float		dist;
	vec3_t		ofs;
	float		d1;
	q2cbrushside_t	*side;

	if (!brush->numsides)
		return;

	for (i=0 ; i<brush->numsides ; i++)
	{
		side = brush->brushside+i;
		plane = side->plane;

		switch(trace_shape)
		{
		default:
		case shape_isbox:	// general box case

			// push the plane out apropriately for mins/maxs

			// FIXME: use signbits into 8 way lookup for each mins/maxs
			for (j=0 ; j<3 ; j++)
			{
				if (plane->normal[j] < 0)
					ofs[j] = maxs[j];
				else
					ofs[j] = mins[j];
			}
			dist = DotProduct (ofs, plane->normal);
			dist = plane->dist - dist;
			break;
		capsuledist(dist,plane,mins,maxs)
		case shape_ispoint:
			dist = plane->dist;
			break;
		}

		d1 = DotProduct (p1, plane->normal) - dist;

		// if completely in front of face, no intersection
		if (d1 > 0)
			return;
	}

	// inside this brush
	trace->startsolid = trace->allsolid = true;
	trace->contents |= brush->contents;
}

#ifdef Q3BSPS
static void CM_TestBoxInPatch (vec3_t mins, vec3_t maxs, vec3_t p1,
					  trace_t *trace, q2cbrush_t *brush)
{
	int			i, j;
	mplane_t	*plane;
	vec3_t		ofs;
	float dist;
	float		d1, maxdist;
	q2cbrushside_t	*side;

	if (!brush->numsides)
		return;

	maxdist = -9999;

	for (i=0 ; i<brush->numsides ; i++)
	{
		side = brush->brushside+i;
		plane = side->plane;

		switch(trace_shape)
		{
		default:
		case shape_isbox:
			// general box case

			// push the plane out apropriately for mins/maxs

			// FIXME: use signbits into 8 way lookup for each mins/maxs
			for (j=0 ; j<3 ; j++)
			{
				if (plane->normal[j] < 0)
					ofs[j] = maxs[j];
				else
					ofs[j] = mins[j];
			}

			dist = DotProduct (ofs, plane->normal);
			dist = plane->dist - dist;
			break;
		capsuledist(dist,plane,mins,maxs)
		case shape_ispoint:
			dist = plane->dist;
			break;
		}

		d1 = DotProduct (p1, plane->normal) - dist;

		// if completely in front of face, no intersection
		if (d1 > 0)
			return;

		if (side->surface && d1 > maxdist)
			maxdist = d1;
	}

// FIXME
	if (maxdist < -0.25)
		return;		// deep inside the patch

	// inside this patch
	trace->startsolid = trace->allsolid = true;
	trace->contents = brush->contents;
}
#endif

/*
================
CM_TraceToLeaf
================
*/
static void CM_TraceToLeaf (cminfo_t	*prv, mleaf_t		*leaf)
{
	int			k;
	q2cbrush_t	*b;

#ifdef Q3BSPS
	int patchnum, j;
	q3cpatch_t *patch;
	q3cmesh_t *cmesh;
#endif

	if ( !(leaf->contents & trace_contents))
		return;
	// trace line against all brushes in the leaf
	for (k=0 ; k<leaf->numleafbrushes ; k++)
	{
		b = prv->leafbrushes[leaf->firstleafbrush+k];
		if (b->checkcount == checkcount)
			continue;	// already checked this brush in another leaf
		b->checkcount = checkcount;

		if ( !(b->contents & trace_contents))
			continue;
		if (!BoundsIntersect(b->absmins, b->absmaxs, trace_absmins, trace_absmaxs))
			continue;
		CM_ClipBoxToBrush (trace_mins, trace_maxs, trace_start, trace_end, &trace_trace, b);
		if (trace_nearfraction <= 0)
			return;
	}

#ifdef Q3BSPS
	if (!prv->mapisq3 || map_noCurves.value)
		return;

	// trace line against all patches in the leaf
	for (k = 0; k < leaf->numleafpatches; k++)
	{
		patchnum = prv->leafpatches[leaf->firstleafpatch+k];

		patch = &prv->patches[patchnum];
		if (patch->checkcount == checkcount)
			continue;	// already checked this patch in another leaf
		patch->checkcount = checkcount;
		if ( !(patch->surface->c.value & trace_contents) )
			continue;
		if ( !BoundsIntersect(patch->absmins, patch->absmaxs, trace_absmins, trace_absmaxs) )
			continue;
		for (j = 0; j < patch->numbrushes; j++)
		{
			CM_ClipBoxToPatch (trace_mins, trace_maxs, trace_start, trace_end, &trace_trace, &patch->brushes[j]);
			if (trace_nearfraction<=0)
				return;
		}
	}

	for (k = 0; k < leaf->numleafcmeshes; k++)
	{
		patchnum = prv->leafcmeshes[leaf->firstleafcmesh+k];
		cmesh = &prv->cmeshes[patchnum];
		if (cmesh->checkcount == checkcount)
			continue;	// already checked this patch in another leaf
		cmesh->checkcount = checkcount;
		if ( !(cmesh->surface->c.value & trace_contents) )
			continue;
		if ( !BoundsIntersect(cmesh->absmins, cmesh->absmaxs, trace_absmins, trace_absmaxs) )
			continue;

		Mod_Trace_Trisoup_(cmesh->xyz_array, cmesh->indicies, cmesh->numincidies, trace_start, trace_end, trace_mins, trace_maxs, &trace_trace, &cmesh->surface->c);
		if (trace_nearfraction<=0)
			return;
	}
#endif
}


/*
================
CM_TestInLeaf
================
*/
static void CM_TestInLeaf (cminfo_t *prv, mleaf_t *leaf)
{
	int			k;
	q2cbrush_t	*b;
#ifdef Q3BSPS
	int patchnum, j;
	q3cmesh_t	*cmesh;
	q3cpatch_t *patch;
#endif

	if ( !(leaf->contents & trace_contents))
		return;
	// trace line against all brushes in the leaf
	for (k=0 ; k<leaf->numleafbrushes ; k++)
	{
		b = prv->leafbrushes[leaf->firstleafbrush+k];
		if (b->checkcount == checkcount)
			continue;	// already checked this brush in another leaf
		b->checkcount = checkcount;

		if (!(b->contents & trace_contents))
			continue;
		if (!BoundsIntersect(b->absmins, b->absmaxs, trace_absmins, trace_absmaxs))
			continue;
		CM_TestBoxInBrush (trace_mins, trace_maxs, trace_start, &trace_trace, b);
		if (!trace_trace.fraction)
			return;
	}

#ifdef Q3BSPS
	if (!prv->mapisq3 || map_noCurves.value)
		return;

	// trace line against all patches in the leaf
	for (k = 0; k < leaf->numleafpatches; k++)
	{
		patchnum = prv->leafpatches[leaf->firstleafpatch+k];

		patch = &prv->patches[patchnum];
		if (patch->checkcount == checkcount)
			continue;	// already checked this patch in another leaf
		patch->checkcount = checkcount;
		if ( !(patch->surface->c.value & trace_contents) )
			continue;
		if ( !BoundsIntersect(patch->absmins, patch->absmaxs, trace_absmins, trace_absmaxs) )
			continue;
		for (j = 0; j < patch->numbrushes; j++)
		{
			CM_TestBoxInPatch (trace_mins, trace_maxs, trace_start, &trace_trace, &patch->brushes[j]);
			if (!trace_trace.fraction)
				return;
		}
	}

	for (k = 0; k < leaf->numleafcmeshes; k++)
	{
		patchnum = prv->leafcmeshes[leaf->firstleafcmesh+k];
		cmesh = &prv->cmeshes[patchnum];
		if (cmesh->checkcount == checkcount)
			continue;	// already checked this patch in another leaf
		cmesh->checkcount = checkcount;
		if ( !(cmesh->surface->c.value & trace_contents) )
			continue;
		if ( !BoundsIntersect(cmesh->absmins, cmesh->absmaxs, trace_absmins, trace_absmaxs) )
			continue;

		Mod_Trace_Trisoup_(cmesh->xyz_array, cmesh->indicies, cmesh->numincidies, trace_start, trace_end, trace_mins, trace_maxs, &trace_trace, &cmesh->surface->c);
		if (trace_nearfraction<=0)
			return;
	}
#endif
}


/*
==================
CM_RecursiveHullCheck

==================
*/
static void CM_RecursiveHullCheck (model_t *mod, int num, float p1f, float p2f, vec3_t p1, vec3_t p2)
{
	mnode_t		*node;
	mplane_t	*plane;
	float		t1, t2, offset;
	float		frac, frac2;
	float		idist;
	int			i;
	vec3_t		mid;
	int			side;
	float		midf;

	if (trace_truefraction <= p1f)
		return;		// already hit something nearer

	// if < 0, we are in a leaf node
	if (num < 0)
	{
		CM_TraceToLeaf (mod->meshinfo, &mod->leafs[-1-num]);
		return;
	}

	//
	// find the point distances to the seperating plane
	// and the offset for the size of the box
	//
	node = mod->nodes + num;
	plane = node->plane;

	if (plane->type < 3)
	{
		t1 = p1[plane->type] - plane->dist;
		t2 = p2[plane->type] - plane->dist;
		offset = trace_extents[plane->type];
	}
	else
	{
		t1 = DotProduct (plane->normal, p1) - plane->dist;
		t2 = DotProduct (plane->normal, p2) - plane->dist;
		if (trace_shape == shape_ispoint)
			offset = 0;
		else
			offset = fabs(trace_extents[0]*plane->normal[0]) +
				fabs(trace_extents[1]*plane->normal[1]) +
				fabs(trace_extents[2]*plane->normal[2]);
	}


#if 0
CM_RecursiveHullCheck (node->childnum[0], p1f, p2f, p1, p2);
CM_RecursiveHullCheck (node->childnum[1], p1f, p2f, p1, p2);
return;
#endif

	// see which sides we need to consider
	if (t1 >= offset && t2 >= offset)
	{
		CM_RecursiveHullCheck (mod, node->childnum[0], p1f, p2f, p1, p2);
		return;
	}
	if (t1 < -offset && t2 < -offset)
	{
		CM_RecursiveHullCheck (mod, node->childnum[1], p1f, p2f, p1, p2);
		return;
	}

	// put the crosspoint DIST_EPSILON pixels on the near side
	if (t1 < t2)
	{
		idist = 1.0/(t1-t2);
		side = 1;
		frac2 = (t1 + offset + DIST_EPSILON)*idist;
		frac = (t1 - offset + DIST_EPSILON)*idist;
	}
	else if (t1 > t2)
	{
		idist = 1.0/(t1-t2);
		side = 0;
		frac2 = (t1 - offset - DIST_EPSILON)*idist;
		frac = (t1 + offset + DIST_EPSILON)*idist;
	}
	else
	{
		side = 0;
		frac = 1;
		frac2 = 0;
	}

	// move up to the node
	if (frac < 0)
		frac = 0;
	if (frac > 1)
		frac = 1;

	midf = p1f + (p2f - p1f)*frac;
	for (i=0 ; i<3 ; i++)
		mid[i] = p1[i] + frac*(p2[i] - p1[i]);

	CM_RecursiveHullCheck (mod, node->childnum[side], p1f, midf, p1, mid);


	// go past the node
	if (frac2 < 0)
		frac2 = 0;
	if (frac2 > 1)
		frac2 = 1;

	midf = p1f + (p2f - p1f)*frac2;
	for (i=0 ; i<3 ; i++)
		mid[i] = p1[i] + frac2*(p2[i] - p1[i]);

	CM_RecursiveHullCheck (mod, node->childnum[side^1], midf, p2f, mid, p2);
}


//======================================================================

/*
==================
CM_BoxTrace
==================
*/
static trace_t		CM_BoxTrace (model_t *mod, vec3_t start, vec3_t end,
						  vec3_t mins, vec3_t maxs, qboolean capsule,
						  int brushmask)
{
	int		i;
	vec3_t point;


	checkcount++;		// for multi-check avoidance

	// fill in a default trace
	memset (&trace_trace, 0, sizeof(trace_trace));
	trace_truefraction = 1;
	trace_nearfraction = 1;
	trace_trace.fraction = 1;
	trace_trace.truefraction = 1;
	trace_trace.surface = &(nullsurface.c);

	if (!mod)	// map not loaded
		return trace_trace;

	trace_contents = brushmask;
	VectorCopy (start, trace_start);
	VectorCopy (end, trace_end);
	VectorCopy (mins, trace_mins);
	VectorCopy (maxs, trace_maxs);

	if (1)
	{
		VectorAdd(trace_maxs, trace_mins, point);
		VectorScale(point, 0.5, point);

		VectorAdd(trace_start, point, trace_start);
		VectorAdd(trace_end, point, trace_end);
		VectorSubtract(trace_mins, point, trace_mins);
		VectorSubtract(trace_maxs, point, trace_maxs);
	}



	// build a bounding box of the entire move (for patches)
	ClearBounds (trace_absmins, trace_absmaxs);

	//determine the type of trace that we're going to use, and the max extents
	if (trace_mins[0] == 0 && trace_mins[1] == 0 && trace_mins[2] == 0 && trace_maxs[0] == 0 && trace_maxs[1] == 0 && trace_maxs[2] == 0)
	{
		trace_shape = shape_ispoint;
		VectorSet (trace_extents, 1/32.0, 1/32.0, 1/32.0);
		//acedemic
		AddPointToBounds (trace_start, trace_absmins, trace_absmaxs);
		AddPointToBounds (trace_end, trace_absmins, trace_absmaxs);
	}
	else if (capsule)
	{
		float ext;
		trace_shape = shape_iscapsule;
		//determine the capsule sizes
		trace_capsulesize[0] = ((maxs[0]-mins[0]) + (maxs[1]-mins[1]))/4.0;
		trace_capsulesize[1] = maxs[2];
		trace_capsulesize[2] = mins[2];
		ext = (trace_capsulesize[1] > -trace_capsulesize[2])?trace_capsulesize[1]:-trace_capsulesize[2];
		trace_capsulesize[1] -= trace_capsulesize[0];
		trace_capsulesize[2] += trace_capsulesize[0];
		trace_extents[0] = ext+1;
		trace_extents[1] = ext+1;
		trace_extents[2] = ext+1;

		//determine the total range
		VectorSubtract (trace_start, trace_extents, point);
		AddPointToBounds (point, trace_absmins, trace_absmaxs);
		VectorAdd (trace_start, trace_extents, point);
		AddPointToBounds (point, trace_absmins, trace_absmaxs);
		VectorSubtract (trace_end, trace_extents, point);
		AddPointToBounds (point, trace_absmins, trace_absmaxs);
		VectorAdd (trace_end, trace_extents, point);
		AddPointToBounds (point, trace_absmins, trace_absmaxs);
	}
	else
	{
		VectorAdd (trace_start, trace_mins, point);
		AddPointToBounds (point, trace_absmins, trace_absmaxs);
		VectorAdd (trace_start, trace_maxs, point);
		AddPointToBounds (point, trace_absmins, trace_absmaxs);
		VectorAdd (trace_end, trace_mins, point);
		AddPointToBounds (point, trace_absmins, trace_absmaxs);
		VectorAdd (trace_end, trace_maxs, point);
		AddPointToBounds (point, trace_absmins, trace_absmaxs);

		trace_shape = shape_isbox;
		trace_extents[0] = ((-trace_mins[0] > trace_maxs[0]) ? -trace_mins[0] : trace_maxs[0])+1;
		trace_extents[1] = ((-trace_mins[1] > trace_maxs[1]) ? -trace_mins[1] : trace_maxs[1])+1;
		trace_extents[2] = ((-trace_mins[2] > trace_maxs[2]) ? -trace_mins[2] : trace_maxs[2])+1;
	}

	trace_absmins[0] -= 1.0;
	trace_absmins[1] -= 1.0;
	trace_absmins[2] -= 1.0;
	trace_absmaxs[0] += 1.0;
	trace_absmaxs[1] += 1.0;
	trace_absmaxs[2] += 1.0;

#if 0
	if (0)
	{	//treat *ALL* tests against the actual geometry instead of using any brushes.
		//also ignores the bsp etc. not fast. testing only.

		trace_ispoint = trace_mins[0] == 0 && trace_mins[1] == 0 && trace_mins[2] == 0
				&& trace_maxs[0] == 0 && trace_maxs[1] == 0 && trace_maxs[2] == 0;
	
		for (i = 0; i < mod->numsurfaces; i++)
		{
			CM_ClipBoxToMesh(trace_mins, trace_maxs, trace_start, trace_end, &trace_trace, mod->surfaces[i].mesh);
		}
	}
	else
	if (0)
	{
		trace_ispoint = trace_mins[0] == 0 && trace_mins[1] == 0 && trace_mins[2] == 0
				&& trace_maxs[0] == 0 && trace_maxs[1] == 0 && trace_maxs[2] == 0;
	
		for (i = 0; i < mod->numleafs; i++)
			CM_TraceToLeaf(&mod->leafs[i]);
	}
	else
#endif
	//
	// check for position test special case
	//
	if (start[0] == end[0] && start[1] == end[1] && start[2] == end[2])
	{
		int		leafs[1024];
		int		i, numleafs;
		int		topnode;

		numleafs = CM_BoxLeafnums_headnode (mod, trace_absmins, trace_absmaxs, leafs, sizeof(leafs)/sizeof(leafs[0]), mod->hulls[0].firstclipnode, &topnode);
		for (i=0 ; i<numleafs ; i++)
		{
			CM_TestInLeaf (mod->meshinfo, &mod->leafs[leafs[i]]);
			if (trace_trace.allsolid)
				break;
		}
		VectorCopy (start, trace_trace.endpos);
		return trace_trace;
	}
	//
	// general aabb trace
	//
	else
	{
		CM_RecursiveHullCheck (mod, mod->hulls[0].firstclipnode, 0, 1, trace_start, trace_end);
	}

	if (trace_nearfraction == 1)
	{
		trace_trace.fraction = 1;
		VectorCopy (end, trace_trace.endpos);
	}
	else
	{
		if (trace_nearfraction<0)
			trace_nearfraction=0;
		trace_trace.fraction = trace_nearfraction;
		for (i=0 ; i<3 ; i++)
			trace_trace.endpos[i] = start[i] + trace_trace.fraction * (end[i] - start[i]);
	}
	return trace_trace;
}

static qboolean BM_NativeTrace(model_t *model, int forcehullnum, framestate_t *framestate, vec3_t axis[3], vec3_t start, vec3_t end, vec3_t mins, vec3_t maxs, qboolean capsule, unsigned int contents, trace_t *trace)
{
	int i;
	memset (trace, 0, sizeof(*trace));
	trace_truefraction = 1;
	trace_nearfraction = 1;
	trace->fraction = 1;
	trace->truefraction = 1;
	trace->surface = &(nullsurface.c);

	if (contents & FTECONTENTS_BODY)
	{
		trace_contents = contents;
		VectorCopy (start, trace_start);
		VectorCopy (end, trace_end);
		VectorCopy (mins, trace_mins);
		VectorCopy (maxs, trace_maxs);

		if (trace_mins[0] == 0 && trace_mins[1] == 0 && trace_mins[2] == 0 && trace_maxs[0] == 0 && trace_maxs[1] == 0 && trace_maxs[2] == 0)
			trace_shape = shape_ispoint;
		else if (capsule)
			trace_shape = shape_iscapsule;
		else
			trace_shape = shape_isbox;

		CM_ClipBoxToBrush (trace_mins, trace_maxs, trace_start, trace_end, trace, &box_brush);
	}

	if (trace_nearfraction == 1)
	{
		trace->fraction = 1;
		VectorCopy (trace_end, trace->endpos);
	}
	else
	{
		if (trace_nearfraction<0)
			trace_nearfraction=0;
		trace->fraction = trace_nearfraction;
		trace->truefraction = trace_truefraction;
		for (i=0 ; i<3 ; i++)
			trace->endpos[i] = trace_start[i] + trace->fraction * (trace_end[i] - trace_start[i]);
	}
	return trace->fraction != 1;
}
static qboolean CM_NativeTrace(model_t *model, int forcehullnum, framestate_t *framestate, vec3_t axis[3], vec3_t start, vec3_t end, vec3_t mins, vec3_t maxs, qboolean capsule, unsigned int contents, trace_t *trace)
{
	if (axis)
	{
		vec3_t start_l;
		vec3_t end_l;
		start_l[0] = DotProduct(start, axis[0]);
		start_l[1] = DotProduct(start, axis[1]);
		start_l[2] = DotProduct(start, axis[2]);
		end_l[0] = DotProduct(end, axis[0]);
		end_l[1] = DotProduct(end, axis[1]);
		end_l[2] = DotProduct(end, axis[2]);
		VectorSet(trace_up, axis[0][2], -axis[1][2], axis[2][2]);
		*trace = CM_BoxTrace(model, start_l, end_l, mins, maxs, capsule, contents);
#ifdef TERRAIN
		if (model->terrain)
		{
			trace_t hmt;
			Heightmap_Trace(model, forcehullnum, framestate, NULL, start, end, mins, maxs, capsule, contents, &hmt);
			if (hmt.fraction < trace->fraction)
				*trace = hmt;
		}
#endif

		if (trace->fraction == 1)
		{
			VectorCopy (end, trace->endpos);
		}
		else
		{
			vec3_t iaxis[3];
			vec3_t norm;
			Matrix3x3_RM_Invert_Simple((void *)axis, iaxis);
			VectorCopy(trace->plane.normal, norm);
			trace->plane.normal[0] = DotProduct(norm, iaxis[0]);
			trace->plane.normal[1] = DotProduct(norm, iaxis[1]);
			trace->plane.normal[2] = DotProduct(norm, iaxis[2]);

			/*just interpolate it, its easier than inverse matrix rotations*/
			VectorInterpolate(start, trace->fraction, end, trace->endpos);
		}
	}
	else
	{
		VectorSet(trace_up, 0, 0, 1);
		*trace = CM_BoxTrace(model, start, end, mins, maxs, capsule, contents);
#ifdef TERRAIN
		if (model->terrain)
		{
			trace_t hmt;
			Heightmap_Trace(model, forcehullnum, framestate, NULL, start, end, mins, maxs, capsule, contents, &hmt);
			if (hmt.fraction < trace->fraction)
				*trace = hmt;
		}
#endif
	}
	return trace->fraction != 1;
}

/*
===============================================================================

PVS / PHS

===============================================================================
*/

/*
===================
CM_DecompressVis
===================
*/

/*
qbyte *Mod_Q2DecompressVis (qbyte *in, model_t *model)
{
	static qbyte	decompressed[MAX_MAP_LEAFS/8];
	int		c;
	qbyte	*out;
	int		row;

	row = (model->vis->numclusters+7)>>3;
	out = decompressed;

	if (!in)
	{	// no vis info, so make all visible
		while (row)
		{
			*out++ = 0xff;
			row--;
		}
		return decompressed;
	}

	do
	{
		if (*in)
		{
			*out++ = *in++;
			continue;
		}

		c = in[1];
		in += 2;
		while (c)
		{
			*out++ = 0;
			c--;
		}
	} while (out - decompressed < row);

	return decompressed;
}

#define	DVIS_PVS	0
#define	DVIS_PHS	1
qbyte *Mod_ClusterPVS (int cluster, model_t *model)
{
	if (cluster == -1 || !model->vis)
		return mod_novis;
	return Mod_Q2DecompressVis ( (qbyte *)model->vis + model->vis->bitofs[cluster][DVIS_PVS],
		model);
}
*/
static void CM_DecompressVis (model_t *mod, qbyte *in, qbyte *out, qboolean merge)
{
	cminfo_t	*prv = (cminfo_t*)mod->meshinfo;
	int		c;
	qbyte	*out_p;
	int		row;

	row = (mod->numclusters+7)>>3;
	out_p = out;

	if (!in || !prv->numvisibility)
	{	// no vis info, so make all visible
		while (row)
		{
			*out_p++ = 0xff;
			row--;
		}
		return;
	}

	if (merge)
	{
		do
		{
			if (*in)
			{
				*out_p++ |= *in++;
				continue;
			}

			out_p += in[1];
			in += 2;
		} while (out_p - out < row);
	}
	else
	{
		do
		{
			if (*in)
			{
				*out_p++ = *in++;
				continue;
			}

			c = in[1];
			in += 2;
			if ((out_p - out) + c > row)
			{
				c = row - (out_p - out);
				Con_DPrintf ("warning: Vis decompression overrun\n");
			}
			while (c)
			{
				*out_p++ = 0;
				c--;
			}
		} while (out_p - out < row);
	}
}

static pvsbuffer_t	pvsrow;
static pvsbuffer_t	phsrow;



qbyte	*CM_ClusterPVS (model_t *mod, int cluster, pvsbuffer_t *buffer, pvsmerge_t merge)
{
	cminfo_t	*prv = (cminfo_t*)mod->meshinfo;
	if (!buffer)
		buffer = &pvsrow;
	if (buffer->buffersize < mod->pvsbytes)
		buffer->buffer = BZ_Realloc(buffer->buffer, buffer->buffersize=mod->pvsbytes);

	if (mod->fromgame == fg_quake2)
	{
		if (cluster == -1)
			memset (buffer->buffer, 0, (mod->numclusters+7)>>3);
		else
			CM_DecompressVis (mod, ((qbyte*)prv->q2vis) + prv->q2vis->bitofs[cluster][DVIS_PVS], buffer->buffer, merge==PVM_MERGE);
		return buffer->buffer;
	}
	else
	{
		if (cluster != -1 && prv->q3pvs->numclusters)
		{
			if (merge == PVM_FAST)
				return (qbyte *)prv->q3pvs->data + cluster * prv->q3pvs->rowsize;
			else if (merge == PVM_REPLACE)
				memcpy(buffer->buffer, prv->q3pvs->data + cluster * prv->q3pvs->rowsize, mod->pvsbytes);
			else
			{
				int c;
				char *in = prv->q3pvs->data + cluster * prv->q3pvs->rowsize;
				for (c = 0; c < mod->pvsbytes; c+=4)
					*(int*)&buffer->buffer[c] |= *(int*)&in[c];
			}
		}
		else
		{
			if (merge != PVM_REPLACE)
				memset (buffer->buffer, 0, (mod->numclusters+7)>>3);
		}
		return buffer->buffer;
	}
}

qbyte	*CM_ClusterPHS (model_t *mod, int cluster, pvsbuffer_t *buffer)
{
	cminfo_t	*prv = (cminfo_t*)mod->meshinfo;

	if (!buffer)
		buffer = &phsrow;
	if (buffer->buffersize < mod->pvsbytes)
		buffer->buffer = BZ_Realloc(buffer->buffer, buffer->buffersize=mod->pvsbytes);

	if (mod->fromgame != fg_quake2)
	{
		if (cluster != -1 && prv->q3phs->numclusters)
		{
			return (qbyte *)prv->q3phs->data + cluster * prv->q3phs->rowsize;
		}
		else
		{
			memset (buffer->buffer, 0, (mod->numclusters+7)>>3);
			return buffer->buffer;
		}
	}

	if (cluster == -1)
		memset (buffer->buffer, 0, (mod->numclusters+7)>>3);
	else
		CM_DecompressVis (mod, ((qbyte*)prv->q2vis) + prv->q2vis->bitofs[cluster][DVIS_PHS], buffer->buffer, false);
	return buffer->buffer;
}

unsigned int  SV_Q2BSP_FatPVS (model_t *mod, vec3_t org, pvsbuffer_t *result, qboolean merge)
{
	int	leafs[64];
	int		i, j, count;
	vec3_t	mins, maxs;

	for (i=0 ; i<3 ; i++)
	{
		mins[i] = org[i] - 8;
		maxs[i] = org[i] + 8;
	}

	count = CM_BoxLeafnums (mod, mins, maxs, leafs, countof(leafs), NULL);
	if (count < 1)
		Sys_Error ("SV_Q2FatPVS: count < 1");

	// convert leafs to clusters
	for (i=0 ; i<count ; i++)
		leafs[i] = CM_LeafCluster(mod, leafs[i]);

	//grow the buffer if needed
	if (result->buffersize < mod->pvsbytes)
		result->buffer = BZ_Realloc(result->buffer, result->buffersize=mod->pvsbytes);

	if (count == 1 && leafs[0] == -1)
	{	//if the only leaf is the outside then broadcast it.
		memset(result->buffer, 0xff, mod->pvsbytes);
		i = count;
	}
	else
	{
		i = 0;
		if (!merge)
			mod->funcs.ClusterPVS(mod, leafs[i++], result, PVM_REPLACE);
		// or in all the other leaf bits
		for ( ; i<count ; i++)
		{
			for (j=0 ; j<i ; j++)
				if (leafs[i] == leafs[j])
					break;
			if (j != i)
				continue;		// already have the cluster we want
			mod->funcs.ClusterPVS(mod, leafs[i], result, PVM_MERGE);
		}
	}
	return mod->pvsbytes;
}

static int		clientarea;
unsigned int Q23BSP_FatPVS(model_t *mod, vec3_t org, pvsbuffer_t *buffer, qboolean merge)
{//fixme: this doesn't add areas
	int		leafnum;
	leafnum = CM_PointLeafnum (mod, org);
	clientarea = CM_LeafArea (mod, leafnum);

	return SV_Q2BSP_FatPVS (mod, org, buffer, merge);
}

qboolean Q23BSP_EdictInFatPVS(model_t *mod, pvscache_t *ent, qbyte *pvs)
{
	int i,l;
	int nullarea = (mod->fromgame == fg_quake2)?0:-1;
	if (clientarea == ent->areanum)
	{
		if (clientarea == nullarea)
			return false;
	}
	else  if (!CM_AreasConnected (mod, clientarea, ent->areanum))
	{	// doors can legally straddle two areas, so
		// we may need to check another one
		if (ent->areanum2 == nullarea
			|| !CM_AreasConnected (mod, clientarea, ent->areanum2))
			return false;		// blocked by a door
	}

	if (ent->num_leafs == -1)
	{	// too many leafs for individual check, go by headnode
		if (!CM_HeadnodeVisible (mod, ent->headnode, pvs))
			return false;
	}
	else
	{	// check individual leafs
		for (i=0 ; i < ent->num_leafs ; i++)
		{
			l = ent->leafnums[i];
			if (pvs[l >> 3] & (1 << (l&7) ))
				break;
		}
		if (i == ent->num_leafs)
			return false;		// not visible
	}
	return true;
}

/*
===============================================================================

AREAPORTALS

===============================================================================
*/

static void FloodArea_r (cminfo_t	*prv, q2carea_t *area, int floodnum)
{
	int		i;

	if (area->floodvalid == prv->floodvalid)
	{
		if (area->floodnum == floodnum)
			return;
		Con_Printf ("FloodArea_r: reflooded\n");
		return;
	}

	area->floodnum = floodnum;
	area->floodvalid = prv->floodvalid;
	if (prv->mapisq3)
	{
		for (i=0 ; i<prv->numareas ; i++)
		{
			if (prv->q3areas[area - prv->q2areas].numareaportals[i]>0)
				FloodArea_r (prv, &prv->q2areas[i], floodnum);
		}
	}
	else
	{
		q2dareaportal_t	*p = &prv->areaportals[area->firstareaportal];
		for (i=0 ; i<area->numareaportals ; i++, p++)
		{
			if (prv->portalopen[p->portalnum])
				FloodArea_r (prv, &prv->q2areas[p->otherarea], floodnum);
		}
	}
}

/*
====================
FloodAreaConnections


====================
*/
static void	FloodAreaConnections (cminfo_t	*prv)
{
	int		i;
	q2carea_t	*area;
	int		floodnum;

	// all current floods are now invalid
	prv->floodvalid++;
	floodnum = 0;

	// area 0 is not used
	for (i=0 ; i<prv->numareas ; i++)
	{
		area = &prv->q2areas[i];
		if (area->floodvalid == prv->floodvalid)
			continue;		// already flooded into
		floodnum++;
		FloodArea_r (prv, area, floodnum);
	}

}

void	CMQ2_SetAreaPortalState (model_t *mod, unsigned int portalnum, qboolean open)
{
	cminfo_t	*prv;
	if (!mod)
		return;
	prv = (cminfo_t*)mod->meshinfo;
	if (prv->mapisq3)
		return;
	if (portalnum > prv->numareaportals)
		Host_Error ("areaportal > numareaportals");

	if (prv->portalopen[portalnum] == open)
		return;
	prv->portalopen[portalnum] = open;
	FloodAreaConnections (prv);

	return;
}

void	CMQ3_SetAreaPortalState (model_t *mod, unsigned int area1, unsigned int area2, qboolean open)
{
	cminfo_t	*prv = (cminfo_t*)mod->meshinfo;
	if (!prv->mapisq3)
		return;
//		Host_Error ("CMQ3_SetAreaPortalState on non-q3 map");

	if (area1 >= prv->numareas || area2 >= prv->numareas)
		Host_Error ("CMQ3_SetAreaPortalState: area > numareas");

	if (open)
	{
		prv->q3areas[area1].numareaportals[area2]++;
		prv->q3areas[area2].numareaportals[area1]++;
	}
	else
	{
		prv->q3areas[area1].numareaportals[area2]--;
		prv->q3areas[area2].numareaportals[area1]--;
	}

	FloodAreaConnections(prv);
}

qboolean	VARGS CM_AreasConnected (model_t *mod, unsigned int area1, unsigned int area2)
{
	cminfo_t	*prv = (cminfo_t*)mod->meshinfo;

	if (map_noareas.value)
		return true;

	if (area1 == ~0 || area2 == ~0)
		return area1 == area2;
	if (area1 > prv->numareas || area2 > prv->numareas)
		Host_Error ("area > numareas");

	if (prv->q2areas[area1].floodnum == prv->q2areas[area2].floodnum)
		return true;
	return false;
}


/*
=================
CM_WriteAreaBits

Writes a length qbyte followed by a bit vector of all the areas
that area in the same flood as the area parameter

This is used by the client refreshes to cull visibility
=================
*/
int CM_WriteAreaBits (model_t *mod, qbyte *buffer, int area, qboolean merge)
{
	cminfo_t	*prv = (cminfo_t*)mod->meshinfo;
	int		i;
	int		floodnum;
	int		bytes;

	bytes = (prv->numareas+7)>>3;

	if (map_noareas.value || (area < 0 && !merge))
	{	// for debugging, send everything
		if (!merge)
			memset (buffer, 255, bytes);
	}
	else
	{
		if (!merge)
			memset (buffer, 0, bytes);

		floodnum = prv->q2areas[area].floodnum;
		for (i=0 ; i<prv->numareas ; i++)
		{
			if (prv->q2areas[i].floodnum == floodnum || !area)
				buffer[i>>3] |= 1<<(i&7);
		}
	}

	return bytes;
}

/*
===================
CM_WritePortalState

Returns a size+pointer to the data that needs to be written into a saved game. 
===================
*/
size_t CM_WritePortalState (model_t *mod, void **data)
{
	cminfo_t	*prv = (cminfo_t*)mod->meshinfo;

	if (mod->type == mod_brush && (mod->fromgame == fg_quake2 || mod->fromgame == fg_quake3))
	{
#ifdef Q3BSPS
		if (prv->mapisq3)
		{	//endian issues. oh well.
			*data = prv->q3areas;
			return sizeof(prv->q3areas);
		}
		else
#endif
		{
			*data = prv->portalopen;
			return sizeof(prv->portalopen);
		}
	}
	*data = NULL;
	return 0;
}

/*
===================
CM_ReadPortalState

Reads the portal state from a savegame file
and recalculates the area connections
===================
*/
qofs_t	CM_ReadPortalState (model_t *mod, qbyte *ptr, qofs_t ptrsize)
{
	cminfo_t	*prv = (cminfo_t*)mod->meshinfo;

	if (mod->type == mod_brush && (mod->fromgame == fg_quake2 || mod->fromgame == fg_quake3))
	{
#ifdef Q3BSPS
		if (prv->mapisq3)
		{
			if (ptrsize < sizeof(prv->q3areas))
				Con_Printf("CM_ReadPortalState() expected %u, but only %u available\n",(unsigned int)sizeof(prv->q3areas),(unsigned int)ptrsize);
			else
			{
				memcpy(prv->q3areas, ptr, sizeof(prv->q3areas));

				FloodAreaConnections (prv);
				return sizeof(prv->portalopen);
			}
		}
		else
#endif
		{
			if (ptrsize < sizeof(prv->portalopen))
				Con_Printf("CM_ReadPortalState() expected %u, but only %u available\n",(unsigned int)sizeof(prv->portalopen),(unsigned int)ptrsize);
			else
			{
				memcpy(prv->portalopen, ptr, sizeof(prv->portalopen));

				FloodAreaConnections (prv);
				return sizeof(prv->portalopen);
			}
		}
	}
	return 0;
}

/*
=============
CM_HeadnodeVisible

Returns true if any leaf under headnode has a cluster that
is potentially visible
=============
*/
qboolean CM_HeadnodeVisible (model_t *mod, int nodenum, qbyte *visbits)
{
	int		leafnum;
	int		cluster;
	mnode_t	*node;

	if (nodenum < 0)
	{
		leafnum = -1-nodenum;
		cluster = mod->leafs[leafnum].cluster;
		if (cluster == -1)
			return false;
		if (visbits[cluster>>3] & (1<<(cluster&7)))
			return true;
		return false;
	}

	node = &mod->nodes[nodenum];
	if (CM_HeadnodeVisible(mod, node->childnum[0], visbits))
		return true;
	return CM_HeadnodeVisible(mod, node->childnum[1], visbits);
}

unsigned int Q2BSP_PointContents(model_t *mod, vec3_t axis[3], vec3_t p)
{
	int pc;
	pc = CM_PointContents (mod, p);
	return pc;
}







int map_checksum;
qboolean QDECL Mod_LoadQ2BrushModel (model_t *mod, void *buffer, size_t fsize)
{
	mod->fromgame = fg_quake2;
	return CM_LoadMap(mod, buffer, fsize, true, &map_checksum) != NULL;
}

void CM_Init(void)	//register cvars.
{
#define MAPOPTIONS "Map Cvar Options"
	Cvar_Register(&map_noareas, MAPOPTIONS);
	Cvar_Register(&map_noCurves, MAPOPTIONS);
	Cvar_Register(&map_autoopenportals, MAPOPTIONS);
	Cvar_Register(&q3bsp_surf_meshcollision_flag, MAPOPTIONS);
	Cvar_Register(&q3bsp_surf_meshcollision_force, MAPOPTIONS);
	Cvar_Register(&q3bsp_mergeq3lightmaps, MAPOPTIONS);
	Cvar_Register(&r_subdivisions, MAPOPTIONS);

	CM_InitBoxHull ();
}
#endif
