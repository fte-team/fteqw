#ifdef Q3SHADERS
#define SHADER_PASS_MAX	8
#define SHADER_MAX_TC_MODS	8
#define SHADER_DEFORM_MAX	8
#define SHADER_MAX_ANIMFRAMES	8
#define SHADER_ANIM_FRAMES_MAX 16

#define SHADER_PROGPARMS_MAX 16

typedef enum {
	SHADER_BSP,
	SHADER_BSP_VERTEX,
	SHADER_BSP_FLARE,
	SHADER_MD3,
	SHADER_2D
} shadertype_t;

typedef enum {
	SHADER_SORT_NONE,
	SHADER_SORT_SKY,
	SHADER_SORT_PORTAL,
	SHADER_SORT_OPAQUE,
	SHADER_SORT_BANNER,
	SHADER_SORT_UNDERWATER,
	SHADER_SORT_ADDITIVE,
	SHADER_SORT_NEAREST
} shadersort_t;

typedef enum {
	MF_NONE			= 1<<0,
	MF_NORMALS		= 1<<1,
	MF_TRNORMALS	= 1<<2,
	MF_COLORS		= 1<<3,
	MF_STCOORDS		= 1<<4,
	MF_LMCOORDS		= 1<<5,
	MF_NOCULL		= 1<<6,
	MF_NONBATCHED	= 1<<7
} meshfeatures_t;

//colour manipulation
typedef struct
{
    enum {
		SHADER_FUNC_SIN,
		SHADER_FUNC_TRIANGLE,
		SHADER_FUNC_SQUARE,
		SHADER_FUNC_SAWTOOTH,
		SHADER_FUNC_INVERSESAWTOOTH,
		SHADER_FUNC_NOISE,
		SHADER_FUNC_CONSTANT
	} type;				// SHADER_FUNC enum
    float			args[4];			// offset, amplitude, phase_offset, rate
} shaderfunc_t;

#if _MSC_VER || __BORLANDC__
typedef unsigned __int64 msortkey_t;
#else
typedef unsigned long long msortkey_t;
#endif

typedef struct meshbuffer_s
{
	msortkey_t			sortkey;
	int					infokey;		// lightmap number or mesh number
	unsigned int		dlightbits;
	entity_t			*entity;
	struct shader_s		*shader;
	mesh_t				*mesh;
	struct mfog_s		*fog;
} meshbuffer_t;


//tecture coordinate manipulation
typedef struct 
{
	enum {
		SHADER_TCMOD_NONE,		//bug
		SHADER_TCMOD_SCALE,		//some sorta tabled deformation
		SHADER_TCMOD_SCROLL,	//boring moving texcoords with time
		SHADER_TCMOD_STRETCH,	//constant factor
		SHADER_TCMOD_ROTATE,
		SHADER_TCMOD_MAX,
		SHADER_TCMOD_TRANSFORM,
		SHADER_TCMOD_TURB
	} type;
	float			args[6];
} tcmod_t;

//vertex positioning manipulation.
typedef struct
{
	enum {
		DEFORMV_NONE,		//bug
		DEFORMV_MOVE,
		DEFORMV_WAVE,
		DEFORMV_NORMAL,
		DEFORMV_BULGE,
		DEFORMV_AUTOSPRITE,
		DEFORMV_AUTOSPRITE2,
		DEFORMV_PROJECTION_SHADOW
	} type;
    float			args[4];
    shaderfunc_t	func;
} deformv_t;


typedef struct shaderpass_s {
	int numMergedPasses;

	shaderfunc_t rgbgen_func;
	shaderfunc_t alphagen_func;

	struct cin_s *cin;

	
    unsigned int	blendsrc, blenddst; // glBlendFunc args
	unsigned int	blendmode, envmode;

	unsigned int	combinesrc0, combinesrc1, combinemode;

    unsigned int	depthfunc;			// glDepthFunc arg
    enum {
		SHADER_ALPHA_GT0,
		SHADER_ALPHA_LT128,
		SHADER_ALPHA_GE128
	} alphafunc;

	enum {
		TC_GEN_BASE,	//basic specified texture coords
		TC_GEN_LIGHTMAP,	//use loaded lightmap coords
		TC_GEN_ENVIRONMENT,
		TC_GEN_DOTPRODUCT,
		TC_GEN_VECTOR
	} tcgen;
	enum {
		RGB_GEN_WAVE,
		RGB_GEN_ENTITY,
		RGB_GEN_ONE_MINUS_ENTITY,
		RGB_GEN_VERTEX,
		RGB_GEN_EXACT_VERTEX,
		RGB_GEN_ONE_MINUS_VERTEX,
		RGB_GEN_IDENTITY_LIGHTING,
		RGB_GEN_IDENTITY,
		RGB_GEN_CONST,
		RGB_GEN_UNKNOWN,
		RGB_GEN_LIGHTING_DIFFUSE,
		RGB_GEN_TOPCOLOR,
		RGB_GEN_BOTTOMCOLOR
	} rgbgen;
	enum {
		ALPHA_GEN_ENTITY,
		ALPHA_GEN_WAVE,
		ALPHA_GEN_PORTAL,
		ALPHA_GEN_SPECULAR,
		ALPHA_GEN_IDENTITY,
		ALPHA_GEN_VERTEX,
		ALPHA_GEN_CONST
	} alphagen;

	int numtcmods;
	tcmod_t		tcmods[SHADER_MAX_TC_MODS];

	void (*flush) (meshbuffer_t *mb, struct shaderpass_s *pass);

	int anim_numframes;
	int			anim_frames[SHADER_MAX_ANIMFRAMES];
	float anim_fps;
	unsigned int texturetype;

	enum {
		SHADER_PASS_BLEND		= 1 << 0,
		SHADER_PASS_ALPHAFUNC	= 1 << 1,
		SHADER_PASS_DEPTHWRITE	= 1 << 2,

		SHADER_PASS_VIDEOMAP	= 1 << 3,
		SHADER_PASS_DETAIL		= 1 << 4,
		SHADER_PASS_LIGHTMAP	= 1 << 5,
		SHADER_PASS_DELUXMAP	= 1 << 6,
		SHADER_PASS_NOCOLORARRAY = 1<< 7,
		SHADER_PASS_ANIMMAP		= 1 << 8
	} flags;
} shaderpass_t;

typedef struct
{
	mesh_t			meshes[5];

	int				farbox_textures[6];
	int				nearbox_textures[6];
} skydome_t;

typedef struct {
	enum shaderprogparmtype_e {
		SP_BAD,

		SP_ENTCOLOURS,
		SP_TOPCOLOURS,
		SP_BOTTOMCOLOURS,
		SP_TIME,
		SP_EYEPOS,

		//things that are set immediatly
		SP_FIRSTIMMEDIATE,	//never set
		SP_CVARI,
		SP_CVARF,
		SP_TEXTURE
	} type;
	unsigned int handle;
} shaderprogparm_t;

typedef struct shader_s {
	enum {
		SSTYLE_CUSTOM,
		SSTYLE_FULLBRIGHT,
		SSTYLE_LIGHTMAPPED
	} style;
	int numpasses;	//careful... 0 means it's not loaded... and not actually a proper shader.
	struct shader_s *next;
	char name[MAX_QPATH];
	//end of shared fields.

	byte_vec4_t fog_color;
	float fog_dist;

	int numdeforms;
	deformv_t	deforms[SHADER_DEFORM_MAX];

	enum {
		SHADER_SKY				= 1 << 0,
		SHADER_NOMIPMAPS		= 1 << 1,
		SHADER_NOPICMIP			= 1 << 2,
		SHADER_CULL_FRONT		= 1 << 3,
		SHADER_CULL_BACK		= 1 << 4,
		SHADER_DEFORMV_BULGE	= 1 << 5,
		SHADER_AUTOSPRITE		= 1 << 6,
		SHADER_FLARE			= 1 << 7,
		SHADER_POLYGONOFFSET	= 1 << 8,
		SHADER_ENTITY_MERGABLE	= 1 << 9,
		SHADER_VIDEOMAP			= 1 << 10,
		SHADER_DEPTHWRITE		= 1 << 11,
		SHADER_AGEN_PORTAL		= 1 << 12,
		SHADER_BLEND			= 1 << 13,	//blend or alphatest (not 100% opaque).
		SHADER_NODRAW			= 1 << 14	//parsed only to pee off developers when they forget it on no-pass shaders.
	} flags;

	unsigned int programhandle;
	int numprogparams;
	shaderprogparm_t progparm[SHADER_PROGPARMS_MAX];

	shaderpass_t passes[SHADER_PASS_MAX];

	shadersort_t sort;

	skydome_t	*skydome;

	meshfeatures_t features;

	int registration_sequence;
} shader_t;

extern shader_t	r_shaders[];




void R_RenderMeshGeneric ( meshbuffer_t *mb, shaderpass_t *pass );
void R_RenderMeshCombined ( meshbuffer_t *mb, shaderpass_t *pass );
void R_RenderMeshMultitextured ( meshbuffer_t *mb, shaderpass_t *pass );
void R_RenderMeshProgram ( meshbuffer_t *mb, shaderpass_t *pass );

shader_t *R_RegisterPic (char *name);
shader_t *R_RegisterShader (char *name);
shader_t *R_RegisterShader_Vertex (char *name);
shader_t *R_RegisterShader_Flare (char *name);
shader_t *R_RegisterSkin (char *name);
shader_t *R_RegisterCustom (char *name, void(*defaultgen)(char *name, shader_t*));

cin_t *R_ShaderGetCinematic(char *name);

void Shader_DefaultSkinShell(char *shortname, shader_t *s);


void R_BackendInit (void);
void Shader_Shutdown (void);
qboolean Shader_Init (void);

mfog_t *CM_FogForOrigin(vec3_t org);
#endif
