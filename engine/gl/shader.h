#define SHADER_PASS_MAX	8
#define SHADER_MAX_TC_MODS	8
#define SHADER_DEFORM_MAX	8
#define SHADER_MAX_ANIMFRAMES	8

//colour manipulation
typedef struct
{
    enum {
		SHADER_FUNC_SIN,
		SHADER_FUNC_TRIANGLE,
		SHADER_FUNC_SQUARE,
		SHADER_FUNC_SAWTOOTH,
		SHADER_FUNC_INVERSESAWTOOTH,
		SHADER_FUNC_NOISE
	} type;				// SHADER_FUNC enum
    float			args[4];			// offset, amplitude, phase_offset, rate
} shaderfunc_t;

//tecture coordinate manipulation
typedef struct 
{
	enum {
		SHADER_TCMOD_NONE,		//bug
		SHADER_TCMOD_SCALE,		//some sorta tabled deformation
		SHADER_TCMOD_SCROLL,	//boring moving texcoords with time
		SHADER_TCMOD_STRETCH,	//constant factor
		SHADER_TCMOD_ROTATE
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


typedef struct {
	int mergedpasses;

	shaderfunc_t rgbgen_func;
	shaderfunc_t alphagen_func;

	
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
		TC_GEN_DOTPRODUCT
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
		RGB_GEN_CONST
	} rgbgen;
	enum {
		ALPHA_GEN_ENTITY,
		ALPHA_GEN_WAVE,
		ALPHA_GEN_PORTAL,
		ALPHA_GEN_SPECULAR,
		ALPHA_GEN_IDENTITY
	} alphagen;

	int numtcmods;
	tcmod_t		tcmod[SHADER_MAX_TC_MODS];

	int anim_numframes;
	int			anim_frames[SHADER_MAX_ANIMFRAMES];
	float fps;
	unsigned int texturetype;

	enum {
		SHADER_PASS_BLEND		= 1 << 0,
		SHADER_PASS_ALPHAFUNC	= 1 << 1,
		SHADER_PASS_DEPTHWRITE	= 1 << 2
	} flags;
} shaderpass_t;

typedef struct shader_s {
	int numpasses;	//careful... 0 means it's not loaded... and not actually a proper shader.
	struct shader_s *next;
	char name[MAX_QPATH];
	//end of shared fields.

	byte_vec4_t fog_color;

	int numdeforms;
	deformv_t	deforms[SHADER_DEFORM_MAX];

	enum {
		SHADER_SKY			= 1 << 0,
		SHADER_NOMIPMAPS	= 1 << 1,
		SHADER_NOPICMIP		= 1 << 2,
		SHADER_CULL_FRONT	= 1 << 3,
		SHADER_CULL_BACK	= 1 << 4,
		SHADER_DEFORMV_BULGE = 1 << 5,
		SHADER_AUTOSPRITE = 1 << 5
	} flags;

	shaderpass_t pass[SHADER_PASS_MAX];
} shader_t;




void GLR_MeshInit(void);
void GL_DrawMesh(mesh_t *mesh, shader_t *shader, int texturenum, int lmtexturenum);
void GL_DrawMeshBump(mesh_t *mesh, int texturenum, int lmtexturenum, int bumpnum, int deluxnum);
