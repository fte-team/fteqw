/*
 +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
    Half-Life Model Renderer (Experimental) Copyright (C) 2001 James 'Ender' Brown [ender@quakesrc.org] This program is
    free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation; either version 2 of the License, or (at your option) any later version.
    This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied
    warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more
    details. You should have received a copy of the GNU General Public License along with this program; if not, write
    to the Free Software Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA. fromquake.h -
    
	model_hl.h - halflife model structure
 +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 */
#define HLPOLYHEADER	(('T' << 24) + ('S' << 16) + ('D' << 8) + 'I')	/* little-endian "IDST" */
#define HLMDLHEADER		"IDST"

/*
 -----------------------------------------------------------------------------------------------------------------------
    main model header
 -----------------------------------------------------------------------------------------------------------------------
 */
typedef struct
{
	int		filetypeid;	//IDSP
	int		version;	//10
	char	name[64];
	int		filesize;
	vec3_t	unknown3[5];
	int		unknown4;	//flags
	int		numbones;
	int		boneindex;
	int		numcontrollers;
	int		controllerindex;
	int		unknown5[2];	//hitboxes
	int		numseq;
	int		seqindex;
	int		unknown6;		//external sequences
	int		seqgroups;
	int		numtextures;
	int		textures;
	int		unknown7;	//something to do with external textures
	int		skinrefs;
	int		skingroups;
	int		skins;
	int		numbodyparts;
	int		bodypartindex;
	int		unknown9[8];	//attachments, sounds, transitions
} hlmdl_header_t;

/*
 -----------------------------------------------------------------------------------------------------------------------
    skin info
 -----------------------------------------------------------------------------------------------------------------------
 */
typedef struct
{
	char	name[64];
	int		flags; /*flat, chrome, fullbright*/
	int		w;	/* width */
	int		h;	/* height */
	int		offset;	/* index */
} hlmdl_tex_t;

/*
 -----------------------------------------------------------------------------------------------------------------------
    body part index
 -----------------------------------------------------------------------------------------------------------------------
 */
typedef struct
{
	char	name[64];
	int		nummodels;
	int		base;
	int		modelindex;
} hlmdl_bodypart_t;

/*
 -----------------------------------------------------------------------------------------------------------------------
    meshes
 -----------------------------------------------------------------------------------------------------------------------
 */
typedef struct
{
	int numtris;
	int index;
	int skinindex;
	int unknown2;
	int unknown3;
} hlmdl_mesh_t;

/*
 -----------------------------------------------------------------------------------------------------------------------
    bones
 -----------------------------------------------------------------------------------------------------------------------
 */
typedef struct
{
	char	name[32];
	int		parent;
	int		unknown1;
	int		bonecontroller[6];
	float	value[6];
	float	scale[6];
} hlmdl_bone_t;

/*
 -----------------------------------------------------------------------------------------------------------------------
    bone controllers
 -----------------------------------------------------------------------------------------------------------------------
 */
typedef struct
{
	int		name;
	int		type;
	float	start;
	float	end;
	int		unknown1;
	int		index;
} hlmdl_bonecontroller_t;

/*
 -----------------------------------------------------------------------------------------------------------------------
    halflife submodel descriptor
 -----------------------------------------------------------------------------------------------------------------------
 */
typedef struct
{
	char	name[64];
	int		unknown1;
	float	unknown2;
	int		nummesh;
	int		meshindex;
	int		numverts;
	int		vertinfoindex;
	int		vertindex;
	int		unknown3[2];
	int		normindex;
	int		unknown4[2];
} hlmdl_submodel_t;

/*
 -----------------------------------------------------------------------------------------------------------------------
    animation
 -----------------------------------------------------------------------------------------------------------------------
 */
typedef struct
{
	unsigned short	offset[6];
} hlmdl_anim_t;

/*
 -----------------------------------------------------------------------------------------------------------------------
    animation frames
 -----------------------------------------------------------------------------------------------------------------------
 */
typedef union
{
	struct {
		qbyte	valid;
		qbyte	total;
	} num;
	short	value;
} hlmdl_animvalue_t;

/*
 -----------------------------------------------------------------------------------------------------------------------
    sequence descriptions
 -----------------------------------------------------------------------------------------------------------------------
 */
typedef struct
{
	char	name[32];
	float	timing;
	int		loop;
	int		unknown1[4];
	int		numframes;
	int		unknown2[2];
	int		motiontype;
	int		motionbone;
	vec3_t	unknown3;
	int		unknown4[2];
	vec3_t	bbox[2];
	int		hasblendseq;
	int		index;
	int		unknown7[2];
	float	unknown[4];
	int		unknown8;
	unsigned int		seqindex;
	int		unknown9[4];
} hlmdl_sequencelist_t;

/*
 -----------------------------------------------------------------------------------------------------------------------
    sequence groups
 -----------------------------------------------------------------------------------------------------------------------
 */
typedef struct
{
	char			name[96];	/* should be split label[32] and name[64] */
	unsigned int	cache;
	int				data;
} hlmdl_sequencedata_t;

typedef struct
{
	int magic;		//IDSQ
	int version;	//10
	char name[64];
	int unk1;
} hlmdl_sequencefile_t;
/*
 -----------------------------------------------------------------------------------------------------------------------
    halflife model internal structure
 -----------------------------------------------------------------------------------------------------------------------
 */

#define MAX_ANIM_GROUPS	16	//submodel files containing anim data.
typedef struct	//this is stored as the cache. an hlmodel_t is generated when drawing
{
	//updated while rendering...
	float	controller[5];				/* Position of bone controllers */
	float	adjust[5];

	hlmdl_header_t			*header;
	hlmdl_bone_t			*bones;
	hlmdl_bonecontroller_t	*bonectls;
	hlmdl_sequencefile_t	*animcache[MAX_ANIM_GROUPS];
	zonegroup_t				*memgroup;
	struct hlmodelshaders_s
	{
		char name[MAX_QPATH];
		texnums_t defaulttex;
		shader_t *shader;
		int w, h;
	} *shaders;
	short *skinref;
	int numskinrefs;
	int numskingroups;

	int numgeomsets;
	struct
	{
		int numalternatives;
		struct hlalternative_s
		{
			mesh_t mesh;
			int numsubmeshes;
			struct
			{
				int firstindex;
				int numindexes;
			} *submesh;
		} *alternatives;
	} *geomset;
} hlmodel_t;

/* HL mathlib prototypes: */
void	QuaternionGLAngle(const vec3_t angles, vec4_t quaternion);
void	QuaternionGLMatrix(float x, float y, float z, float w, vec4_t *GLM);
//void	UploadTexture(hlmdl_tex_t *ptexture, qbyte *data, qbyte *pal);

/* HL drawing */
qboolean QDECL Mod_LoadHLModel (model_t *mod, void *buffer, size_t fsize);
void	R_DrawHLModel(entity_t	*curent);

/* physics stuff */
void *Mod_GetHalfLifeModelData(model_t *mod);

//reflectioney things, including bone data
int HLMDL_BoneForName(model_t *mod, const char *name);
int HLMDL_FrameForName(model_t *mod, const char *name);
const char *HLMDL_FrameNameForNum(model_t *model, int surfaceidx, int num);
qboolean HLMDL_FrameInfoForNum(model_t *model, int surfaceidx, int num, char **name, int *numframes, float *duration, qboolean *loop);
int HLMDL_GetNumBones(model_t *mod);
int HLMDL_GetBoneData(model_t *model, int firstbone, int lastbone, framestate_t *fstate, float *result);
