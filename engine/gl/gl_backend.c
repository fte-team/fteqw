#include "quakedef.h"
#ifdef GLQUAKE

#include "glquake.h"
#include "shader.h"

#define LIGHTPASS_GLSL_SHARED	"\
varying vec2 tcbase;\n\
varying vec3 lightvector;\n\
#if defined(SPECULAR) || defined(USEOFFSETMAPPING)\n\
varying vec3 eyevector;\n\
#endif\n\
#ifdef PCF\n\
varying vec4 shadowcoord;\n\
uniform mat4 entmatrix;\n\
#endif\n\
"

#define LIGHTPASS_GLSL_VERTEX	"\
#ifdef VERTEX_SHADER\n\
\
uniform vec3 lightposition;\n\
\
#if defined(SPECULAR) || defined(USEOFFSETMAPPING)\n\
uniform vec3 eyeposition;\n\
#endif\n\
\
void main (void)\n\
{\n\
	gl_Position = ftransform();\n\
\
	tcbase = gl_MultiTexCoord0.xy;	//pass the texture coords straight through\n\
\
	vec3 lightminusvertex = lightposition - gl_Vertex.xyz;\n\
	lightvector.x = dot(lightminusvertex, gl_MultiTexCoord2.xyz);\n\
	lightvector.y = dot(lightminusvertex, gl_MultiTexCoord3.xyz);\n\
	lightvector.z = dot(lightminusvertex, gl_MultiTexCoord1.xyz);\n\
\
#if defined(SPECULAR)||defined(USEOFFSETMAPPING)\n\
	vec3 eyeminusvertex = eyeposition - gl_Vertex.xyz;\n\
	eyevector.x = dot(eyeminusvertex, gl_MultiTexCoord2.xyz);\n\
	eyevector.y = dot(eyeminusvertex, gl_MultiTexCoord3.xyz);\n\
	eyevector.z = dot(eyeminusvertex, gl_MultiTexCoord1.xyz);\n\
#endif\n\
#if defined(PCF) || defined(SPOT) || defined(PROJECTION)\n\
	shadowcoord = gl_TextureMatrix[7] * (entmatrix*gl_Vertex);\n\
#endif\n\
}\n\
#endif\n\
"

/*this is full 4*4 PCF, with an added attempt at prenumbra*/
/*the offset consts are 1/(imagesize*2) */
#define PCF16P(f)	"\
	float xPixelOffset = (1.0+shadowcoord.b/lightradius)/512.0;\
	float yPixelOffset = (1.0+shadowcoord.b/lightradius)/512.0;\
	float s = 0.0;\n\
	s += "f"Proj(shadowmap, shadowcoord + vec4(-1.5 * xPixelOffset * shadowcoord.w, -1.5 * yPixelOffset * shadowcoord.w, 0.05, 0.0)).r;\n\
	s += "f"Proj(shadowmap, shadowcoord + vec4(-1.5 * xPixelOffset * shadowcoord.w, -0.5 * yPixelOffset * shadowcoord.w, 0.05, 0.0)).r;\n\
	s += "f"Proj(shadowmap, shadowcoord + vec4(-1.5 * xPixelOffset * shadowcoord.w, 0.5 * yPixelOffset * shadowcoord.w, 0.05, 0.0)).r;\n\
	s += "f"Proj(shadowmap, shadowcoord + vec4(-1.5 * xPixelOffset * shadowcoord.w, 1.1 * yPixelOffset * shadowcoord.w, 0.05, 0.0)).r;\n\
\
	s += "f"Proj(shadowmap, shadowcoord + vec4(-0.5 * xPixelOffset * shadowcoord.w, -1.5 * yPixelOffset * shadowcoord.w, 0.05, 0.0)).r;\n\
	s += "f"Proj(shadowmap, shadowcoord + vec4(-0.5 * xPixelOffset * shadowcoord.w, -0.5 * yPixelOffset * shadowcoord.w, 0.05, 0.0)).r;\n\
	s += "f"Proj(shadowmap, shadowcoord + vec4(-0.5 * xPixelOffset * shadowcoord.w, 0.5 * yPixelOffset * shadowcoord.w, 0.05, 0.0)).r;\n\
	s += "f"Proj(shadowmap, shadowcoord + vec4(-0.5 * xPixelOffset * shadowcoord.w, 1.1 * yPixelOffset * shadowcoord.w, 0.05, 0.0)).r;\n\
\
	s += "f"Proj(shadowmap, shadowcoord + vec4(0.5 * xPixelOffset * shadowcoord.w, -1.5 * yPixelOffset * shadowcoord.w, 0.05, 0.0)).r;\n\
	s += "f"Proj(shadowmap, shadowcoord + vec4(0.5 * xPixelOffset * shadowcoord.w, -0.5 * yPixelOffset * shadowcoord.w, 0.05, 0.0)).r;\n\
	s += "f"Proj(shadowmap, shadowcoord + vec4(0.5 * xPixelOffset * shadowcoord.w, 0.5 * yPixelOffset * shadowcoord.w, 0.05, 0.0)).r;\n\
	s += "f"Proj(shadowmap, shadowcoord + vec4(0.5 * xPixelOffset * shadowcoord.w, 1.1 * yPixelOffset * shadowcoord.w, 0.05, 0.0)).r;\n\
\
	s += "f"Proj(shadowmap, shadowcoord + vec4(1.5 * xPixelOffset * shadowcoord.w, -1.5 * yPixelOffset * shadowcoord.w, 0.05, 0.0)).r;\n\
	s += "f"Proj(shadowmap, shadowcoord + vec4(1.5 * xPixelOffset * shadowcoord.w, -0.5 * yPixelOffset * shadowcoord.w, 0.05, 0.0)).r;\n\
	s += "f"Proj(shadowmap, shadowcoord + vec4(1.5 * xPixelOffset * shadowcoord.w, 0.5 * yPixelOffset * shadowcoord.w, 0.05, 0.0)).r;\n\
	s += "f"Proj(shadowmap, shadowcoord + vec4(1.5 * xPixelOffset * shadowcoord.w, 1.1 * yPixelOffset * shadowcoord.w, 0.05, 0.0)).r;\n\
\
	colorscale *= s/5.0;\n\
	"

/*this is pcf 3*3*/
/*the offset consts are 1/(imagesize*2) */
#define PCF9(f)	"\
	const float xPixelOffset = 1.0/512.0;\
	const float yPixelOffset = 1.0/512.0;\
	float s = 0.0;\n\
	s += "f"Proj(shadowmap, shadowcoord + vec4(-1.0 * xPixelOffset * shadowcoord.w, -1.0 * yPixelOffset * shadowcoord.w, 0.05, 0.0)).r;\n\
	s += "f"Proj(shadowmap, shadowcoord + vec4(-1.0 * xPixelOffset * shadowcoord.w, 0.0 * yPixelOffset * shadowcoord.w, 0.05, 0.0)).r;\n\
	s += "f"Proj(shadowmap, shadowcoord + vec4(-1.0 * xPixelOffset * shadowcoord.w, 1.0 * yPixelOffset * shadowcoord.w, 0.05, 0.0)).r;\n\
\
	s += "f"Proj(shadowmap, shadowcoord + vec4(0.0 * xPixelOffset * shadowcoord.w, -1.0 * yPixelOffset * shadowcoord.w, 0.05, 0.0)).r;\n\
	s += "f"Proj(shadowmap, shadowcoord + vec4(0.0 * xPixelOffset * shadowcoord.w, 0.0 * yPixelOffset * shadowcoord.w, 0.05, 0.0)).r;\n\
	s += "f"Proj(shadowmap, shadowcoord + vec4(0.0 * xPixelOffset * shadowcoord.w, 1.0 * yPixelOffset * shadowcoord.w, 0.05, 0.0)).r;\n\
\
	s += "f"Proj(shadowmap, shadowcoord + vec4(1.0 * xPixelOffset * shadowcoord.w, -1.0 * yPixelOffset * shadowcoord.w, 0.05, 0.0)).r;\n\
	s += "f"Proj(shadowmap, shadowcoord + vec4(1.0 * xPixelOffset * shadowcoord.w, 0.0 * yPixelOffset * shadowcoord.w, 0.05, 0.0)).r;\n\
	s += "f"Proj(shadowmap, shadowcoord + vec4(1.0 * xPixelOffset * shadowcoord.w, 1.0 * yPixelOffset * shadowcoord.w, 0.05, 0.0)).r;\n\
	colorscale *= s/9.0;\n\
	"

/*this is a lazy form of pcf. take 5 samples in an x*/
/*the offset consts are 1/(imagesize*2) */
#define PCF5(f)	"\
	float xPixelOffset = 1.0/512.0;\
	float yPixelOffset = 1.0/512.0;\
	float s = 0.0;\n\
	s += "f"Proj(shadowmap, shadowcoord + vec4(-1.0 * xPixelOffset * shadowcoord.w, -1.0 * yPixelOffset * shadowcoord.w, 0.05, 0.0)).r;\n\
	s += "f"Proj(shadowmap, shadowcoord + vec4(-1.0 * xPixelOffset * shadowcoord.w, 1.0 * yPixelOffset * shadowcoord.w, 0.05, 0.0)).r;\n\
	s += "f"Proj(shadowmap, shadowcoord + vec4(0.0 * xPixelOffset * shadowcoord.w, 0.0 * yPixelOffset * shadowcoord.w, 0.05, 0.0)).r;\n\
	s += "f"Proj(shadowmap, shadowcoord + vec4(1.0 * xPixelOffset * shadowcoord.w, -1.0 * yPixelOffset * shadowcoord.w, 0.05, 0.0)).r;\n\
	s += "f"Proj(shadowmap, shadowcoord + vec4(1.0 * xPixelOffset * shadowcoord.w, 1.0 * yPixelOffset * shadowcoord.w, 0.05, 0.0)).r;\n\
	colorscale *= s/5.0;\n\
	"

/*this is unfiltered*/
#define PCF1(f)	"\
	colorscale *= "f"Proj(shadowmap, shadowcoord).r;\n"

#define LIGHTPASS_GLSL_FRAGMENT	"\
#ifdef FRAGMENT_SHADER\n\
uniform sampler2D baset;\n\
#if defined(BUMP) || defined(SPECULAR) || defined(USEOFFSETMAPPING)\n\
uniform sampler2D bumpt;\n\
#endif\n\
#ifdef SPECULAR\n\
uniform sampler2D speculart;\n\
#endif\n\
#ifdef PROJECTION\n\
uniform sampler2D projected;\n\
#endif\n\
#ifdef PCF\n\
#ifdef CUBE\n\
uniform samplerCubeShadow shadowmap;\n\
#else\n\
uniform sampler2DShadow shadowmap;\n\
#endif\n\
#endif\n\
\
\
uniform float lightradius;\n\
uniform vec3 lightcolour;\n\
\
#ifdef USEOFFSETMAPPING\n\
uniform float offsetmapping_scale;\n\
uniform float offsetmapping_bias;\n\
#endif\n\
\
\
void main (void)\n\
{\n\
#ifdef USEOFFSETMAPPING\n\
	// this is 3 sample because of ATI Radeon 9500-9800/X300 limits\n\
	vec2 OffsetVector = normalize(eyevector).xy * vec2(-0.333, 0.333);\n\
	vec2 TexCoordOffset = tcbase + OffsetVector * (offsetmapping_bias + offsetmapping_scale * texture2D(bumpt, tcbase).w);\n\
	TexCoordOffset += OffsetVector * (offsetmapping_bias + offsetmapping_scale * texture2D(bumpt, TexCoordOffset).w);\n\
	TexCoordOffset += OffsetVector * (offsetmapping_bias + offsetmapping_scale * texture2D(bumpt, TexCoordOffset).w);\n\
#define tcbase TexCoordOffset\n\
#endif\n\
\
\
#ifdef BUMP\n\
	vec3 bases = vec3(texture2D(baset, tcbase));\n\
#else\n\
	vec3 diff = vec3(texture2D(baset, tcbase));\n\
#endif\n\
#if defined(BUMP) || defined(SPECULAR)\n\
	vec3 bumps = vec3(texture2D(bumpt, tcbase)) * 2.0 - 1.0;\n\
#endif\n\
#ifdef SPECULAR\n\
	vec3 specs = vec3(texture2D(speculart, tcbase));\n\
#endif\n\
\
	vec3 nl = normalize(lightvector);\n\
	float colorscale = max(1.0 - dot(lightvector, lightvector)/(lightradius*lightradius), 0.0);\n\
\
#ifdef BUMP\n\
	vec3 diff;\n\
	diff = bases * max(dot(bumps, nl), 0.0);\n\
#endif\n\
#ifdef SPECULAR\n\
	vec3 halfdir = (normalize(eyevector) + normalize(lightvector))/2.0;\n\
	float dv = dot(halfdir, bumps);\n\
	diff += pow(dv, 8.0) * specs;\n\
	diff.g = pow(dv, 8.0);\n\
#endif\n\
\n\
#ifdef PCF\n\
#ifdef CUBE\n\
"PCF9("shadowCube") /*valid are 1,5,9*/"\n\
#else\n\
"PCF9("shadow2D") /*valid are 1,5,9*/"\n\
#endif\n\
#endif\n\
#if defined(SPOT)\n\
/*Actually, this isn't correct*/\n\
if (shadowcoord.w < 0.0) discard;\n\
vec2 spot = ((shadowcoord.st)/shadowcoord.w - 0.5)*2.0;colorscale*=1.0-(dot(spot,spot));\n\
#endif\n\
#if defined(PROJECTION)\n\
	lightcolour *= texture2d(projected, shadowcoord);\n\
#endif\n\
\n\
	gl_FragColor.rgb = diff*colorscale*lightcolour;\n\
}\n\
\
#endif\n\
"
static const char LIGHTPASS_SHADER[] = "\
{\n\
	program\n\
	{\n\
	%s%s\n\
	}\n\
\
	//incoming fragment\n\
	param texture 0 baset\n\
	param opt texture 1 bumpt\n\
	param opt texture 2 speculart\n\
\
	//light info\n\
	param lightpos lightposition\n\
	param lightradius lightradius\n\
	param lightcolour lightcolour\n\
\
	param opt cvarf r_shadow_glsl_offsetmapping_bias offsetmapping_bias\n\
	param opt cvarf r_shadow_glsl_offsetmapping_scale offsetmapping_scale\n\
\
	//eye pos\n\
	param opt eyepos EyePosition\n\
\
	{\n\
		map $diffuse\n\
		blendfunc add\n\
		tcgen base\n\
	}\n\
	{\n\
		map $normalmap\n\
		tcgen normal\n\
	}\n\
	{\n\
		map $specular\n\
		tcgen svector\n\
	}\n\
	{\n\
		tcgen tvector\n\
	}\n\
}";
static const char PCFPASS_SHADER[] = "\
{\n\
	program\n\
	{\n\
	//#define CUBE\n\
	#define SPOT\n\
	#define PCF\n\
	%s%s\n\
	}\n\
\
	//incoming fragment\n\
	param texture 7 shadowmap\n\
	param texture 1 baset\n\
	param opt texture 2 bumpt\n\
	param opt texture 3 speculart\n\
\
	//light info\n\
	param lightpos lightposition\n\
	param lightradius lightradius\n\
	param lightcolour lightcolour\n\
\
	param opt cvarf r_shadow_glsl_offsetmapping_bias offsetmapping_bias\n\
	param opt cvarf r_shadow_glsl_offsetmapping_scale offsetmapping_scale\n\
\
	//eye pos\n\
	param opt eyepos EyePosition\n\
	param opt entmatrix entmatrix\n\
\
	{\n\
		map $shadowmap\n\
		blendfunc add\n\
		tcgen base\n\
	}\n\
	{\n\
		map $diffuse\n\
		tcgen normal\n\
	}\n\
	{\n\
		map $normalmap\n\
		tcgen svector\n\
	}\n\
	{\n\
		map $specular\n\
		tcgen tvector\n\
	}\n\
}";


extern cvar_t r_shadow_glsl_offsetmapping;

#if 0//def _DEBUG
#define checkerror() if (qglGetError()) Con_Printf("Error detected at line %s:%i\n", __FILE__, __LINE__)
#else
#define checkerror()
#endif


void PPL_CreateShaderObjects(void){}
void PPL_BaseBModelTextures(entity_t *e){}
void R_DrawBeam( entity_t *e ){}
qboolean PPL_ShouldDraw(void)
{
	if (currententity->flags & Q2RF_EXTERNALMODEL && r_secondaryview != 3)
		return false;
	if (!Cam_DrawPlayer(r_refdef.currentplayernum, currententity->keynum-1))
		return false;
	return true;
}

enum{
PERMUTATION_GENERIC = 0,
PERMUTATION_BUMPMAP = 1,
PERMUTATION_SPECULAR = 2,
PERMUTATION_BUMP_SPEC,
PERMUTATION_OFFSET = 4,
PERMUTATION_OFFSET_BUMP,
PERMUTATION_OFFSET_SPEC,
PERMUTATION_OFFSET_BUMP_SPEC,

PERMUTATIONS
};
static char *lightpassname[PERMUTATIONS] =
{
	"lightpass_flat",
	"lightpass_bump",
	"lightpass_spec",
	"lightpass_bump_spec",
	"lightpass_offset",
	"lightpass_offset_bump",
	"lightpass_offset_spec",
	"lightpass_offset_bump_spec"
};
static char *pcfpassname[PERMUTATIONS] =
{
	"lightpass_pcf",
	"lightpass_pcf_bump",
	"lightpass_pcf_spec",
	"lightpass_pcf_bump_spec",
	"lightpass_pcf_offset",
	"lightpass_pcf_offset_bump",
	"lightpass_pcf_offset_spec",
	"lightpass_pcf_offset_bump_spec"
};
static char *permutationdefines[PERMUTATIONS] = {
	"",
	"#define BUMP\n",
	"#define SPECULAR\n",
	"#define SPECULAR\n#define BUMP\n",
	"#define USEOFFSETMAPPING\n",
	"#define USEOFFSETMAPPING\n#define BUMP\n",
	"#define USEOFFSETMAPPING\n#define SPECULAR\n",
	"#define USEOFFSETMAPPING\n#define SPECULAR\n#define BUMP\n"
};

struct {
	//internal state
	struct {
		int lastpasstmus;
		int vbo_colour;
		int vbo_texcoords[SHADER_PASS_MAX];
		int vbo_deforms;	//holds verticies... in case you didn't realise.

		qboolean initedlightpasses;
		const shader_t *lightpassshader[PERMUTATIONS];
		qboolean initedpcfpasses;
		const shader_t *pcfpassshader[PERMUTATIONS];

		qboolean force2d;
		int currenttmu;
		int texenvmode[SHADER_PASS_MAX];
		int currenttextures[SHADER_PASS_MAX];

		polyoffset_t curpolyoffset;
		unsigned int curcull;
		texid_t curshadowmap;

		unsigned int shaderbits;

		mesh_t *pushedmeshes;
		vbo_t dummyvbo;
		int currentvbo;
		int currentebo;

		int pendingvertexvbo;
		void *pendingvertexpointer;
		int curvertexvbo;
		void *curvertexpointer;

		float identitylighting;	//set to how bright lightmaps should be (reduced for overbright or realtime_world_lightmaps)

		texid_t temptexture;
	};

	//exterior state
	struct {
		backendmode_t mode;
		unsigned int flags;

		vbo_t *sourcevbo;
		const shader_t *curshader;
		const entity_t *curentity;
		const texnums_t *curtexnums;
		texid_t curlightmap;
		texid_t curdeluxmap;

		float curtime;

		vec3_t lightorg;
		vec3_t lightcolours;
		float lightradius;
		texid_t lighttexture;
	};
} shaderstate;

struct {
	int numlights;
	int shadowsurfcount;
} bench;

void GL_TexEnv(GLenum mode)
{
	if (mode != shaderstate.texenvmode[shaderstate.currenttmu])
	{
		qglTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, mode);
		shaderstate.texenvmode[shaderstate.currenttmu] = mode;
	}
}

void GL_SetShaderState2D(qboolean is2d)
{
	shaderstate.force2d = is2d;

	if (!is2d)
	{
		qglEnable(GL_DEPTH_TEST);
		shaderstate.shaderbits &= ~SBITS_MISC_NODEPTHTEST;

		qglDepthMask(GL_TRUE);
		shaderstate.shaderbits |= SBITS_MISC_DEPTHWRITE;
	}
}

void GL_SelectTexture(int target) 
{
	shaderstate.currenttmu = target;
	if (qglClientActiveTextureARB)
	{
		qglClientActiveTextureARB(target + mtexid0);
		qglActiveTextureARB(target + mtexid0);
	}
	else
		qglSelectTextureSGIS(target + mtexid0);
}

void GL_SelectVBO(int vbo)
{
	if (shaderstate.currentvbo != vbo)
	{
		shaderstate.currentvbo = vbo;
		qglBindBufferARB(GL_ARRAY_BUFFER_ARB, shaderstate.currentvbo);
	}
}
void GL_SelectEBO(int vbo)
{
	if (shaderstate.currentebo != vbo)
	{
		shaderstate.currentebo = vbo;
		qglBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, shaderstate.currentebo);
	}
}

static void GL_ApplyVertexPointer(void)
{
	if (shaderstate.curvertexpointer != shaderstate.pendingvertexpointer || shaderstate.pendingvertexvbo != shaderstate.curvertexvbo)
	{
		shaderstate.curvertexpointer = shaderstate.pendingvertexpointer;
		shaderstate.curvertexvbo = shaderstate.pendingvertexvbo;
		GL_SelectVBO(shaderstate.curvertexvbo);
		qglVertexPointer(3, GL_FLOAT, sizeof(vecV_t), shaderstate.curvertexpointer);
	}
}

void GL_MBind(int target, texid_t texnum)
{
	GL_SelectTexture(target);

	if (shaderstate.currenttextures[shaderstate.currenttmu] == texnum.num)
		return;

	shaderstate.currenttextures[shaderstate.currenttmu] = texnum.num;
	bindTexFunc (GL_TEXTURE_2D, texnum.num);
}

void GL_Bind(texid_t texnum)
{
	if (shaderstate.currenttextures[shaderstate.currenttmu] == texnum.num)
		return;

	shaderstate.currenttextures[shaderstate.currenttmu] = texnum.num;

	bindTexFunc (GL_TEXTURE_2D, texnum.num);
}

void GL_BindType(int type, texid_t texnum)
{
	if (shaderstate.currenttextures[shaderstate.currenttmu] == texnum.num)
		return;

	shaderstate.currenttextures[shaderstate.currenttmu] = texnum.num;
	bindTexFunc (type, texnum.num);
}

void GL_CullFace(unsigned int sflags)
{
	if (shaderstate.curcull == sflags)
		return;
	shaderstate.curcull = sflags;

	if (shaderstate.curcull & SHADER_CULL_FRONT)
	{
		qglEnable(GL_CULL_FACE);
		qglCullFace(GL_FRONT);
	}
	else if (shaderstate.curcull & SHADER_CULL_BACK)
	{
		qglEnable(GL_CULL_FACE);
		qglCullFace(GL_BACK);
	}
	else
	{
		qglDisable(GL_CULL_FACE);
	}
}

void R_FetchTopColour(int *retred, int *retgreen, int *retblue)
{
	int i;

	if (currententity->scoreboard)
	{
		i = currententity->scoreboard->ttopcolor;
	}
	else
		i = TOP_RANGE>>4;
	if (i > 8)
	{
		i<<=4;
	}
	else
	{
		i<<=4;
		i+=15;
	}
	i*=3;
	*retred = host_basepal[i+0];
	*retgreen = host_basepal[i+1];
	*retblue = host_basepal[i+2];
/*	if (!gammaworks)
	{
		*retred = gammatable[*retred];
		*retgreen = gammatable[*retgreen];
		*retblue = gammatable[*retblue];
	}*/
}
void R_FetchBottomColour(int *retred, int *retgreen, int *retblue)
{
	int i;

	if (currententity->scoreboard)
	{
		i = currententity->scoreboard->tbottomcolor;
	}
	else
		i = BOTTOM_RANGE>>4;
	if (i > 8)
	{
		i<<=4;
	}
	else
	{
		i<<=4;
		i+=15;
	}
	i*=3;
	*retred = host_basepal[i+0];
	*retgreen = host_basepal[i+1];
	*retblue = host_basepal[i+2];
/*	if (!gammaworks)
	{
		*retred = gammatable[*retred];
		*retgreen = gammatable[*retgreen];
		*retblue = gammatable[*retblue];
	}*/
}

static void RevertToKnownState(void)
{
	shaderstate.curvertexvbo = ~0;
	GL_SelectVBO(0);
	GL_SelectEBO(0);

	checkerror();
	while(shaderstate.lastpasstmus>0)
	{
		GL_SelectTexture(--shaderstate.lastpasstmus);
		qglDisable(GL_TEXTURE_2D);
		qglDisableClientState(GL_TEXTURE_COORD_ARRAY);
	}
	GL_SelectTexture(0);

	qglEnableClientState(GL_VERTEX_ARRAY);
	checkerror();

	qglColor3f(1,1,1);
	qglDepthFunc(GL_LEQUAL);
	qglDepthMask(GL_TRUE);
	shaderstate.shaderbits &= ~(SBITS_MISC_DEPTHEQUALONLY|SBITS_MISC_DEPTHCLOSERONLY);
	shaderstate.shaderbits |= SBITS_MISC_DEPTHWRITE;

	qglColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
}

void PPL_RevertToKnownState(void)
{
	RevertToKnownState();
}

void R_IBrokeTheArrays(void)
{
	RevertToKnownState();
}

void R_UnlockArrays(void)
{
}
void R_ClearArrays(void)
{
}
void GL_FlushBackEnd(void)
{
	memset(&shaderstate, 0, sizeof(shaderstate));
	shaderstate.curcull = ~0;
}
void R_BackendInit(void)
{
}
qboolean R_MeshWillExceed(mesh_t *mesh)
{
	return false;
}

#ifdef RTLIGHTS
//called from gl_shadow
void BE_SetupForShadowMap(void)
{
	while(shaderstate.lastpasstmus>0)
	{
		GL_SelectTexture(--shaderstate.lastpasstmus);
		qglDisable(GL_TEXTURE_2D);
		qglDisableClientState(GL_TEXTURE_COORD_ARRAY);
	}

	qglDisableClientState(GL_TEXTURE_COORD_ARRAY);
	qglShadeModel(GL_FLAT);
	GL_TexEnv(GL_REPLACE);
	qglDepthMask(GL_TRUE);
	shaderstate.shaderbits |= SBITS_MISC_DEPTHWRITE;
//	qglColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
}
#endif

static texid_t T_Gen_CurrentRender(void)
{
	int vwidth, vheight;
	if (gl_config.arb_texture_non_power_of_two)
	{
		vwidth = vid.pixelwidth;
		vheight = vid.pixelheight;
	}
	else
	{
		vwidth = 1;
		vheight = 1;
		while (vwidth < vid.pixelwidth)
		{
			vwidth *= 2;
		}
		while (vheight < vid.pixelheight)
		{
			vheight *= 2;
		}
	}
	// copy the scene to texture
	if (!TEXVALID(shaderstate.temptexture))
		shaderstate.temptexture = GL_AllocNewTexture();
	GL_Bind(shaderstate.temptexture);
	qglCopyTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 0, 0, vwidth, vheight, 0);
	qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	return shaderstate.temptexture;
}

static texid_t Shader_TextureForPass(const shaderpass_t *pass)
{
	switch(pass->texgen)
	{
	default:
	case T_GEN_SINGLEMAP:
		return pass->anim_frames[0];
	case T_GEN_ANIMMAP:
		return pass->anim_frames[(int)(pass->anim_fps * shaderstate.curtime) % pass->anim_numframes];
	case T_GEN_LIGHTMAP:
		return shaderstate.curlightmap;
	case T_GEN_DELUXMAP:
		return shaderstate.curdeluxmap;
	case T_GEN_DIFFUSE:
		return shaderstate.curtexnums?shaderstate.curtexnums->base:r_nulltex;
	case T_GEN_NORMALMAP:
		return shaderstate.curtexnums?shaderstate.curtexnums->bump:r_nulltex;
	case T_GEN_SPECULAR:
		return shaderstate.curtexnums->specular;
	case T_GEN_UPPEROVERLAY:
		return shaderstate.curtexnums->upperoverlay;
	case T_GEN_LOWEROVERLAY:
		return shaderstate.curtexnums->loweroverlay;
	case T_GEN_FULLBRIGHT:
		return shaderstate.curtexnums->fullbright;
	case T_GEN_SHADOWMAP:
		return shaderstate.curshadowmap;

	case T_GEN_VIDEOMAP:
#ifdef NOMEDIA
		return shaderstate.curtexnums?shaderstate.curtexnums->base:r_nulltex;
#else
		return Media_UpdateForShader(pass->cin);
#endif

	case T_GEN_CURRENTRENDER:
		return T_Gen_CurrentRender();
	}
}

/*========================================== matrix functions =====================================*/

typedef vec3_t mat3_t[3];
static mat3_t axisDefault={{1, 0, 0},
					{0, 1, 0},
					{0, 0, 1}};

static void Matrix3_Transpose (mat3_t in, mat3_t out)
{
	out[0][0] = in[0][0];
	out[1][1] = in[1][1];
	out[2][2] = in[2][2];

	out[0][1] = in[1][0];
	out[0][2] = in[2][0];
	out[1][0] = in[0][1];
	out[1][2] = in[2][1];
	out[2][0] = in[0][2];
	out[2][1] = in[1][2];
}
static void Matrix3_Multiply_Vec3 (mat3_t a, vec3_t b, vec3_t product)
{
	product[0] = a[0][0]*b[0] + a[0][1]*b[1] + a[0][2]*b[2];
	product[1] = a[1][0]*b[0] + a[1][1]*b[1] + a[1][2]*b[2];
	product[2] = a[2][0]*b[0] + a[2][1]*b[1] + a[2][2]*b[2];
}

static int Matrix3_Compare(mat3_t in, mat3_t out)
{
	return memcmp(in, out, sizeof(mat3_t));
}

//end matrix functions
/*========================================== tables for deforms =====================================*/
#define frand() (rand()*(1.0/RAND_MAX))
#define FTABLE_SIZE		1024
#define FTABLE_CLAMP(x)	(((int)((x)*FTABLE_SIZE) & (FTABLE_SIZE-1)))
#define FTABLE_EVALUATE(table,x) (table ? table[FTABLE_CLAMP(x)] : frand()*((x)-floor(x)))

static	float	r_sintable[FTABLE_SIZE];
static	float	r_triangletable[FTABLE_SIZE];
static	float	r_squaretable[FTABLE_SIZE];
static	float	r_sawtoothtable[FTABLE_SIZE];
static	float	r_inversesawtoothtable[FTABLE_SIZE];

static float *FTableForFunc ( unsigned int func )
{
	switch (func)
	{
		case SHADER_FUNC_SIN:
			return r_sintable;

		case SHADER_FUNC_TRIANGLE:
			return r_triangletable;

		case SHADER_FUNC_SQUARE:
			return r_squaretable;

		case SHADER_FUNC_SAWTOOTH:
			return r_sawtoothtable;

		case SHADER_FUNC_INVERSESAWTOOTH:
			return r_inversesawtoothtable;
	}

	//bad values allow us to crash (so I can debug em)
	return NULL;
}

void Shader_LightPass_Std(char *shortname, shader_t *s, const void *args)
{
	char shadertext[8192*2];
	sprintf(shadertext, LIGHTPASS_SHADER, args, LIGHTPASS_GLSL_SHARED LIGHTPASS_GLSL_VERTEX LIGHTPASS_GLSL_FRAGMENT);
	Shader_DefaultScript(shortname, s, shadertext);
}
void Shader_LightPass_PCF(char *shortname, shader_t *s, const void *args)
{
	char shadertext[8192*2];
	sprintf(shadertext, PCFPASS_SHADER, args, LIGHTPASS_GLSL_SHARED LIGHTPASS_GLSL_VERTEX LIGHTPASS_GLSL_FRAGMENT);
	Shader_DefaultScript(shortname, s, shadertext);
}

void BE_Init(void)
{
	int i;
	double t;

	be_maxpasses = gl_mtexarbable;

	for (i = 0; i < FTABLE_SIZE; i++)
	{
		t = (double)i / (double)FTABLE_SIZE;

		r_sintable[i] = sin(t * 2*M_PI);
		
		if (t < 0.25) 
			r_triangletable[i] = t * 4.0;
		else if (t < 0.75)
			r_triangletable[i] = 2 - 4.0 * t;
		else
			r_triangletable[i] = (t - 0.75) * 4.0 - 1.0;

		if (t < 0.5) 
			r_squaretable[i] = 1.0f;
		else
			r_squaretable[i] = -1.0f;

		r_sawtoothtable[i] = t;
		r_inversesawtoothtable[i] = 1.0 - t;
	}

	shaderstate.identitylighting = 1;

	/*normally we load these lazily, but if they're probably going to be used anyway, load them now to avoid stalls.*/
	if (r_shadow_realtime_dlight.ival && !shaderstate.initedlightpasses)
	{
		int i;
		shaderstate.initedlightpasses = true;
		for (i = 0; i < PERMUTATIONS; i++)
		{
			shaderstate.lightpassshader[i] = R_RegisterCustom(lightpassname[i], Shader_LightPass_Std, permutationdefines[i]);
		}
	}

	qglEnableClientState(GL_VERTEX_ARRAY);
}

//end tables

#define MAX_ARRAY_VERTS 65535
static avec4_t		coloursarray[MAX_ARRAY_VERTS];
static float		texcoordarray[SHADER_PASS_MAX][MAX_ARRAY_VERTS*2];
static vecV_t		vertexarray[MAX_ARRAY_VERTS];

/*========================================== texture coord generation =====================================*/

static void tcgen_environment(float *st, unsigned int numverts, float *xyz, float *normal) 
{
	int			i;
	vec3_t		viewer, reflected;
	float		d;

	vec3_t		rorg;


	RotateLightVector(shaderstate.curentity->axis, shaderstate.curentity->origin, r_origin, rorg);

	for (i = 0 ; i < numverts ; i++, xyz += 3, normal += 3, st += 2 ) 
	{
		VectorSubtract (rorg, xyz, viewer);
		VectorNormalizeFast (viewer);

		d = DotProduct (normal, viewer);

		reflected[0] = normal[0]*2*d - viewer[0];
		reflected[1] = normal[1]*2*d - viewer[1];
		reflected[2] = normal[2]*2*d - viewer[2];

		st[0] = 0.5 + reflected[1] * 0.5;
		st[1] = 0.5 - reflected[2] * 0.5;
	}
}

static float *tcgen(const shaderpass_t *pass, int cnt, float *dst, const mesh_t *mesh)
{
	int i;
	vecV_t *src;
	switch (pass->tcgen)
	{
	default:
	case TC_GEN_BASE:
		return (float*)mesh->st_array;
	case TC_GEN_LIGHTMAP:
		return (float*)mesh->lmst_array;
	case TC_GEN_NORMAL:
		return (float*)mesh->normals_array;
	case TC_GEN_SVECTOR:
		return (float*)mesh->snormals_array;
	case TC_GEN_TVECTOR:
		return (float*)mesh->tnormals_array;
	case TC_GEN_ENVIRONMENT:
		tcgen_environment(dst, cnt, (float*)mesh->xyz_array, (float*)mesh->normals_array);
		return dst;

//	case TC_GEN_DOTPRODUCT:
//		return mesh->st_array[0];
	case TC_GEN_VECTOR:
		src = mesh->xyz_array;
		for (i = 0; i < cnt; i++, dst += 2)
		{
			static vec3_t tc_gen_s = { 1.0f, 0.0f, 0.0f };
			static vec3_t tc_gen_t = { 0.0f, 1.0f, 0.0f };
			
			dst[0] = DotProduct(tc_gen_s, src[i]);
			dst[1] = DotProduct(tc_gen_t, src[i]);
		}
		return dst;
	}
}

/*src and dst can be the same address when tcmods are chained*/
static void tcmod(const tcmod_t *tcmod, int cnt, const float *src, float *dst, const mesh_t *mesh)
{
	float *table;
	float t1, t2;
	float cost, sint;
	int j;
#define R_FastSin(x) sin((x)*(2*M_PI))
	switch (tcmod->type)
	{
		case SHADER_TCMOD_ROTATE:
			cost = tcmod->args[0] * shaderstate.curtime;
			sint = R_FastSin(cost);
			cost = R_FastSin(cost + 0.25);

			for (j = 0; j < cnt; j++, dst+=2,src+=2)
			{
				t1 = cost * (src[0] - 0.5f) - sint * (src[1] - 0.5f) + 0.5f;
				t2 = cost * (src[1] - 0.5f) + sint * (src[0] - 0.5f) + 0.5f;
				dst[0] = t1;
				dst[1] = t2;
			}
			break;

		case SHADER_TCMOD_SCALE:
			t1 = tcmod->args[0];
			t2 = tcmod->args[1];

			for (j = 0; j < cnt; j++, dst+=2,src+=2)
			{
				dst[0] = src[0] * t1;
				dst[1] = src[1] * t2;
			}
			break;

		case SHADER_TCMOD_TURB:
			t1 = tcmod->args[2] + shaderstate.curtime * tcmod->args[3];
			t2 = tcmod->args[1];

			for (j = 0; j < cnt; j++, dst+=2,src+=2)
			{
				dst[0] = src[0] + R_FastSin (src[0]*t2+t1) * t2;
				dst[1] = src[1] + R_FastSin (src[1]*t2+t1) * t2;
			}
			break;
		
		case SHADER_TCMOD_STRETCH:
			table = FTableForFunc(tcmod->args[0]);
			t2 = tcmod->args[3] + shaderstate.curtime * tcmod->args[4];
			t1 = FTABLE_EVALUATE(table, t2) * tcmod->args[2] + tcmod->args[1];
			t1 = t1 ? 1.0f / t1 : 1.0f;
			t2 = 0.5f - 0.5f * t1;
			for (j = 0; j < cnt; j++, dst+=2,src+=2)
			{
				dst[0] = src[0] * t1 + t2;
				dst[1] = src[1] * t1 + t2;
			}
			break;
					
		case SHADER_TCMOD_SCROLL:
			t1 = tcmod->args[0] * shaderstate.curtime;
			t2 = tcmod->args[1] * shaderstate.curtime;

			for (j = 0; j < cnt; j++, dst += 2, src+=2)
			{
				dst[0] = src[0] + t1;
				dst[1] = src[1] + t2;
			}
			break;
				
		case SHADER_TCMOD_TRANSFORM:
			for (j = 0; j < cnt; j++, dst+=2, src+=2)
			{
				t1 = src[0];
				t2 = src[1];
				dst[0] = t1 * tcmod->args[0] + t2 * tcmod->args[2] + tcmod->args[4];
				dst[1] = t2 * tcmod->args[1] + t1 * tcmod->args[3] + tcmod->args[5];
			}
			break;

		default:
			break;
	}
}

static void GenerateTCMods(const shaderpass_t *pass, int passnum, const mesh_t *meshlist)
{
#if 1
	for (; meshlist; meshlist = meshlist->next)
	{
		int i;
		float *src;
		src = tcgen(pass, meshlist->numvertexes, texcoordarray[passnum]+meshlist->vbofirstvert*2, meshlist);
		//tcgen might return unmodified info
		if (pass->numtcmods)
		{
			tcmod(&pass->tcmods[0], meshlist->numvertexes, src, texcoordarray[passnum]+meshlist->vbofirstvert*2, meshlist);
			for (i = 1; i < pass->numtcmods; i++)
			{
				tcmod(&pass->tcmods[i], meshlist->numvertexes, texcoordarray[passnum]+meshlist->vbofirstvert*2, texcoordarray[passnum]+meshlist->vbofirstvert*2, meshlist);
			}
			src = texcoordarray[passnum]+meshlist->vbofirstvert*2;
		}
		else if (src != texcoordarray[passnum]+meshlist->vbofirstvert*2)
		{
			//this shouldn't actually ever be true
			memcpy(texcoordarray[passnum]+meshlist->vbofirstvert*2, src, 8*meshlist->numvertexes);
		}
	}
	GL_SelectVBO(0);
	qglTexCoordPointer(2, GL_FLOAT, 0, texcoordarray[passnum]);
#else
	if (!shaderstate.vbo_texcoords[passnum])
	{
		qglGenBuffersARB(1, &shaderstate.vbo_texcoords[passnum]);
	}
	GL_SelectVBO(shaderstate.vbo_texcoords[passnum]);

	{
		qglBufferDataARB(GL_ARRAY_BUFFER_ARB, MAX_ARRAY_VERTS*sizeof(float)*2, NULL, GL_STREAM_DRAW_ARB);
		for (; meshlist; meshlist = meshlist->next)
		{
			int i;
			float *src;
			src = tcgen(pass, meshlist->numvertexes, texcoordarray[passnum], meshlist);
			//tcgen might return unmodified info
			if (pass->numtcmods)
			{
				tcmod(&pass->tcmods[0], meshlist->numvertexes, src, texcoordarray[passnum], meshlist);
				for (i = 1; i < pass->numtcmods; i++)
				{
					tcmod(&pass->tcmods[i], meshlist->numvertexes, texcoordarray[passnum], texcoordarray[passnum], meshlist);
				}
				src = texcoordarray[passnum];
			}
			qglBufferSubDataARB(GL_ARRAY_BUFFER_ARB, meshlist->vbofirstvert*8, meshlist->numvertexes*8, src);
		}
	}
	qglTexCoordPointer(2, GL_FLOAT, 0, NULL);
#endif
}

//end texture coords
/*========================================== colour generation =====================================*/

//source is always packed
//dest is packed too
static void colourgen(const shaderpass_t *pass, int cnt, const avec4_t *src, avec4_t *dst, const mesh_t *mesh)
{
	switch (pass->rgbgen)
	{
	case RGB_GEN_ENTITY:
		while((cnt)--)
		{
			dst[cnt][0] = shaderstate.curentity->shaderRGBAf[0];
			dst[cnt][1] = shaderstate.curentity->shaderRGBAf[1];
			dst[cnt][2] = shaderstate.curentity->shaderRGBAf[2];
		}
		break;
	case RGB_GEN_ONE_MINUS_ENTITY:
		while((cnt)--)
		{
			dst[cnt][0] = 1-shaderstate.curentity->shaderRGBAf[0];
			dst[cnt][1] = 1-shaderstate.curentity->shaderRGBAf[1];
			dst[cnt][2] = 1-shaderstate.curentity->shaderRGBAf[2];
		}
		break;
	case RGB_GEN_VERTEX:
	case RGB_GEN_EXACT_VERTEX:
		if (!src)
		{
			while((cnt)--)
			{
				dst[cnt][0] = shaderstate.identitylighting;
				dst[cnt][1] = shaderstate.identitylighting;
				dst[cnt][2] = shaderstate.identitylighting;
			}
			break;
		}

		while((cnt)--)
		{
			dst[cnt][0] = src[cnt][0];
			dst[cnt][1] = src[cnt][1];
			dst[cnt][2] = src[cnt][2];
		}
		break;
	case RGB_GEN_ONE_MINUS_VERTEX:
		while((cnt)--)
		{
			dst[cnt][0] = 1-src[cnt][0];
			dst[cnt][1] = 1-src[cnt][1];
			dst[cnt][2] = 1-src[cnt][2];
		}
		break;
	case RGB_GEN_IDENTITY_LIGHTING:
		//compensate for overbrights
		while((cnt)--)
		{
			dst[cnt][0] = shaderstate.identitylighting;
			dst[cnt][1] = shaderstate.identitylighting;
			dst[cnt][2] = shaderstate.identitylighting;
		}
		break;
	default:
	case RGB_GEN_IDENTITY:
		while((cnt)--)
		{
			dst[cnt][0] = 1;
			dst[cnt][1] = 1;
			dst[cnt][2] = 1;
		}
		break;
	case RGB_GEN_CONST:
		while((cnt)--)
		{
			dst[cnt][0] = pass->rgbgen_func.args[0];
			dst[cnt][1] = pass->rgbgen_func.args[1];
			dst[cnt][2] = pass->rgbgen_func.args[2];
		}
		break;
	case RGB_GEN_LIGHTING_DIFFUSE:
		//collect lighting details for mobile entities
		if (!mesh->normals_array)
		{
			while((cnt)--)
			{
				dst[cnt][0] = 1;
				dst[cnt][1] = 1;
				dst[cnt][2] = 1;
			}
		}
		else
		{
			R_LightArrays(mesh->xyz_array, dst, cnt, mesh->normals_array);
		}
		break;
	case RGB_GEN_WAVE:
		{
			float *table;
			float c;

			table = FTableForFunc(pass->rgbgen_func.type);
			c = pass->rgbgen_func.args[2] + shaderstate.curtime * pass->rgbgen_func.args[3];
			c = FTABLE_EVALUATE(table, c) * pass->rgbgen_func.args[1] + pass->rgbgen_func.args[0];
			c = bound(0.0f, c, 1.0f);

			while((cnt)--)
			{
				dst[cnt][0] = c;
				dst[cnt][1] = c;
				dst[cnt][2] = c;
			}
		}
		break;

	case RGB_GEN_TOPCOLOR:
	case RGB_GEN_BOTTOMCOLOR:
#pragma message("fix 24bit player colours")
		while((cnt)--)
		{
			dst[cnt][0] = 1;
			dst[cnt][1] = 1;
			dst[cnt][2] = 1;
		}
	//	Con_Printf("RGB_GEN %i not supported\n", pass->rgbgen);
		break;
	}
}

static void deformgen(const deformv_t *deformv, int cnt, const avec4_t *src, avec4_t *dst, const mesh_t *mesh)
{
	float *table;
	int j, k;
	float args[4];
	float deflect;
	switch (deformv->type)
	{
	default:
	case DEFORMV_NONE:
		if (src != dst)
			memcpy(dst, src, sizeof(*src)*cnt);
		break;

	case DEFORMV_WAVE:
		if (!mesh->normals_array)
		{
			if (src != dst)
				memcpy(dst, src, sizeof(*src)*cnt);
			return;
		}
		args[0] = deformv->func.args[0];
		args[1] = deformv->func.args[1];
		args[3] = deformv->func.args[2] + deformv->func.args[3] * shaderstate.curtime;
		table = FTableForFunc(deformv->func.type);

		for ( j = 0; j < cnt; j++ )
		{
			deflect = deformv->args[0] * (src[j][0]+src[j][1]+src[j][2]) + args[3];
			deflect = FTABLE_EVALUATE(table, deflect) * args[1] + args[0];

			// Deflect vertex along its normal by wave amount
			VectorMA(src[j], deflect, mesh->normals_array[j], dst[j]);
		}
		break;

	case DEFORMV_NORMAL:
		//normal does not actually move the verts, but it does change the normals array
		//we don't currently support that.
		if (src != dst)
			memcpy(dst, src, sizeof(*src)*cnt);
/*
		args[0] = deformv->args[1] * shaderstate.curtime;

		for ( j = 0; j < cnt; j++ )
		{
			args[1] = normalsArray[j][2] * args[0];

			deflect = deformv->args[0] * R_FastSin(args[1]);
			normalsArray[j][0] *= deflect;
			deflect = deformv->args[0] * R_FastSin(args[1] + 0.25);
			normalsArray[j][1] *= deflect;
			VectorNormalizeFast(normalsArray[j]);
		}
*/		break;

	case DEFORMV_MOVE:
		table = FTableForFunc(deformv->func.type);
		deflect = deformv->func.args[2] + shaderstate.curtime * deformv->func.args[3];
		deflect = FTABLE_EVALUATE(table, deflect) * deformv->func.args[1] + deformv->func.args[0];

		for ( j = 0; j < cnt; j++ )
			VectorMA(src[j], deflect, deformv->args, dst[j]);
		break;

	case DEFORMV_BULGE:
		args[0] = deformv->args[0]/(2*M_PI);
		args[1] = deformv->args[1];
		args[2] = shaderstate.curtime * deformv->args[2]/(2*M_PI);

		for (j = 0; j < cnt; j++)
		{
			deflect = R_FastSin(mesh->st_array[j][0]*args[0] + args[2])*args[1];
			dst[j][0] = src[j][0]+deflect*mesh->normals_array[j][0];
			dst[j][1] = src[j][1]+deflect*mesh->normals_array[j][1];
			dst[j][2] = src[j][2]+deflect*mesh->normals_array[j][2];
		}
		break;

	case DEFORMV_AUTOSPRITE:
		if (mesh->numindexes < 6)
			break;

		for (j = 0; j < cnt-3; j+=4, src+=4, dst+=4)
		{
			vec3_t mid, d;
			float radius;
			mid[0] = 0.25*(src[0][0] + src[1][0] + src[2][0] + src[3][0]);
			mid[1] = 0.25*(src[0][1] + src[1][1] + src[2][1] + src[3][1]);
			mid[2] = 0.25*(src[0][2] + src[1][2] + src[2][2] + src[3][2]);
			VectorSubtract(src[0], mid, d);
			radius = 2*VectorLength(d);

			for (k = 0; k < 4; k++)
			{
				dst[k][0] = mid[0] + radius*((mesh->st_array[k][0]-0.5)*r_view_matrix[0+0]+(mesh->st_array[k][1]-0.5)*r_view_matrix[0+1]);
				dst[k][1] = mid[1] + radius*((mesh->st_array[k][0]-0.5)*r_view_matrix[4+0]+(mesh->st_array[k][1]-0.5)*r_view_matrix[4+1]);
				dst[k][2] = mid[2] + radius*((mesh->st_array[k][0]-0.5)*r_view_matrix[8+0]+(mesh->st_array[k][1]-0.5)*r_view_matrix[8+1]);
			}
		}
		break;

	case DEFORMV_AUTOSPRITE2:
		if (mesh->numindexes < 6)
			break;

		for (k = 0; k < mesh->numindexes; k += 6)
		{
			int long_axis, short_axis;
			vec3_t axis;
			float len[3];
			mat3_t m0, m1, m2, result;
			float *quad[4];
			vec3_t rot_centre, tv;

			quad[0] = (float *)(dst + mesh->indexes[k+0]);
			quad[1] = (float *)(dst + mesh->indexes[k+1]);
			quad[2] = (float *)(dst + mesh->indexes[k+2]);

			for (j = 2; j >= 0; j--)
			{
				quad[3] = (float *)(dst + mesh->indexes[k+3+j]);
				if (!VectorEquals (quad[3], quad[0]) && 
					!VectorEquals (quad[3], quad[1]) &&
					!VectorEquals (quad[3], quad[2]))
				{
					break;
				}
			}

			// build a matrix were the longest axis of the billboard is the Y-Axis
			VectorSubtract(quad[1], quad[0], m0[0]);
			VectorSubtract(quad[2], quad[0], m0[1]);
			VectorSubtract(quad[2], quad[1], m0[2]);
			len[0] = DotProduct(m0[0], m0[0]);
			len[1] = DotProduct(m0[1], m0[1]);
			len[2] = DotProduct(m0[2], m0[2]);

			if ((len[2] > len[1]) && (len[2] > len[0]))
			{
				if (len[1] > len[0])
				{
					long_axis = 1;
					short_axis = 0;
				}
				else
				{
					long_axis = 0;
					short_axis = 1;
				}
			}
			else if ((len[1] > len[2]) && (len[1] > len[0]))
			{
				if (len[2] > len[0])
				{
					long_axis = 2;
					short_axis = 0;
				}
				else
				{
					long_axis = 0;
					short_axis = 2;
				}
			}
			else //if ( (len[0] > len[1]) && (len[0] > len[2]) )
			{
				if (len[2] > len[1])
				{
					long_axis = 2;
					short_axis = 1;
				}
				else
				{
					long_axis = 1;
					short_axis = 2;
				}
			}

			if (DotProduct(m0[long_axis], m0[short_axis]))
			{
				VectorNormalize2(m0[long_axis], axis);
				VectorCopy(axis, m0[1]);

				if (axis[0] || axis[1])
				{
					VectorVectors(m0[1], m0[2], m0[0]);
				}
				else
				{
					VectorVectors(m0[1], m0[0], m0[2]);
				}
			}
			else
			{
				VectorNormalize2(m0[long_axis], axis);
				VectorNormalize2(m0[short_axis], m0[0]);
				VectorCopy(axis, m0[1]);
				CrossProduct(m0[0], m0[1], m0[2]);
			}

			for (j = 0; j < 3; j++)
				rot_centre[j] = (quad[0][j] + quad[1][j] + quad[2][j] + quad[3][j]) * 0.25;

			if (shaderstate.curentity)
			{
				VectorAdd(shaderstate.curentity->origin, rot_centre, tv);
			}
			else
			{
				VectorCopy(rot_centre, tv);
			}
			VectorSubtract(r_origin, tv, tv);

			// filter any longest-axis-parts off the camera-direction
			deflect = -DotProduct(tv, axis);

			VectorMA(tv, deflect, axis, m1[2]);
			VectorNormalizeFast(m1[2]);
			VectorCopy(axis, m1[1]);
			CrossProduct(m1[1], m1[2], m1[0]);

			Matrix3_Transpose(m1, m2);
			Matrix3_Multiply(m2, m0, result);

			for (j = 0; j < 4; j++)
			{
				VectorSubtract(quad[j], rot_centre, tv);
				Matrix3_Multiply_Vec3(result, tv, quad[j]);
				VectorAdd(rot_centre, quad[j], quad[j]);
			}
		}
		break;

//	case DEFORMV_PROJECTION_SHADOW:
//		break;
	}
}

static void GenerateVertexDeforms(const shader_t *shader, const mesh_t *meshlist)
{
	int i;
	for (; meshlist; meshlist = meshlist->next)
	{
		deformgen(&shader->deforms[0], meshlist->numvertexes, meshlist->xyz_array, vertexarray+meshlist->vbofirstvert, meshlist);
		for (i = 1; i < shader->numdeforms; i++)
		{
			deformgen(&shader->deforms[i], meshlist->numvertexes, vertexarray+meshlist->vbofirstvert, vertexarray+meshlist->vbofirstvert, meshlist);
		}
	}

	shaderstate.pendingvertexpointer = vertexarray;
	shaderstate.pendingvertexvbo = 0;
}

/*======================================alpha ===============================*/

static void alphagen(const shaderpass_t *pass, int cnt, const avec4_t *src, avec4_t *dst, const mesh_t *mesh)
{
	float *table;
	float t;
	float f;
	vec3_t v1, v2;
	int i;

	switch (pass->alphagen)
	{
	default:
	case ALPHA_GEN_IDENTITY:
		while(cnt--)
			dst[cnt][3] = 1;
		break;

	case ALPHA_GEN_CONST:
		t = pass->alphagen_func.args[0];
		while(cnt--)
			dst[cnt][3] = t;
		break;

	case ALPHA_GEN_WAVE:
		table = FTableForFunc(pass->alphagen_func.type);
		f = pass->alphagen_func.args[2] + shaderstate.curtime * pass->alphagen_func.args[3];
		f = FTABLE_EVALUATE(table, f) * pass->alphagen_func.args[1] + pass->alphagen_func.args[0];
		t = bound(0.0f, f, 1.0f);
		while(cnt--)
			dst[cnt][3] = t;
		break;

	case ALPHA_GEN_PORTAL:
		//FIXME: should this be per-vert?
		VectorAdd(mesh->xyz_array[0], shaderstate.curentity->origin, v1);
		VectorSubtract(r_origin, v1, v2);
		f = VectorLength(v2) * (1.0 / 255.0);
		f = bound(0.0f, f, 1.0f);

		while(cnt--)
			dst[cnt][3] = f;
		break;

	case ALPHA_GEN_VERTEX:
		if (!src)
		{
			while(cnt--)
			{
				dst[cnt][3] = 1;
			}
			break;
		}

		while(cnt--)
		{
			dst[cnt][3] = src[cnt][3];
		}
		break;

	case ALPHA_GEN_ENTITY:
		f = bound(0, shaderstate.curentity->shaderRGBAf[3], 1);
		while(cnt--)
		{
			dst[cnt][3] = f;
		}
		break;


	case ALPHA_GEN_SPECULAR:
		{
			mat3_t axis;
			AngleVectors(shaderstate.curentity->angles, axis[0], axis[1], axis[2]);
			VectorSubtract(r_origin, shaderstate.curentity->origin, v1);

			if (!Matrix3_Compare(axis, axisDefault))
			{
				Matrix3_Multiply_Vec3(axis, v2, v2);
			}
			else
			{
				VectorCopy(v1, v2);
			}

			for (i = 0; i < cnt; i++)
			{
				VectorSubtract(v2, mesh->xyz_array[i], v1);
				f = DotProduct(v1, mesh->normals_array[i] ) * Q_rsqrt(DotProduct(v1,v1));
				f = f * f * f * f * f;
				dst[i][3] = bound (0.0f, f, 1.0f);
			}
		}
		break;
	}
}

#define DVBOMETHOD 1

static void GenerateColourMods(const shaderpass_t *pass, const mesh_t *meshlist)
{
	if (meshlist->colors4b_array)
	{
		//hack...
		GL_SelectVBO(0);
		qglColorPointer(4, GL_UNSIGNED_BYTE, 0, meshlist->colors4b_array);
		qglEnableClientState(GL_COLOR_ARRAY);
		qglShadeModel(GL_FLAT);
		return;
	}
	if (pass->flags & SHADER_PASS_NOCOLORARRAY)
	{
		avec4_t scol;
	
		colourgen(pass, 1, meshlist->colors4f_array, &scol, meshlist);
		alphagen(pass, 1, meshlist->colors4f_array, &scol, meshlist);
		qglDisableClientState(GL_COLOR_ARRAY);
		qglColor4fv(scol);
		qglShadeModel(GL_FLAT);
		checkerror();
	}
	else
	{
		extern cvar_t r_nolightdir;
		if (pass->rgbgen == RGB_GEN_LIGHTING_DIFFUSE && r_nolightdir.ival)
		{
			extern avec3_t ambientlight, shadelight;
			qglDisableClientState(GL_COLOR_ARRAY);
			qglColor4f(	ambientlight[0]*0.5+shadelight[0],
						ambientlight[1]*0.5+shadelight[1],
						ambientlight[2]*0.5+shadelight[2],
						shaderstate.curentity->shaderRGBAf[3]);
			qglShadeModel(GL_FLAT);
			checkerror();
			return;
		}

		qglShadeModel(GL_SMOOTH);

		//if its vetex lighting, just use the vbo
		if ((pass->rgbgen == RGB_GEN_VERTEX || pass->rgbgen == RGB_GEN_EXACT_VERTEX) && pass->alphagen == ALPHA_GEN_VERTEX)
		{
			GL_SelectVBO(shaderstate.sourcevbo->vbocolours);
			qglColorPointer(4, GL_FLOAT, 0, shaderstate.sourcevbo->colours4f);
			qglEnableClientState(GL_COLOR_ARRAY);
			return;
		}

		for (; meshlist; meshlist = meshlist->next)
		{
			colourgen(pass, meshlist->numvertexes, meshlist->colors4f_array, coloursarray + meshlist->vbofirstvert, meshlist);
			alphagen(pass, meshlist->numvertexes, meshlist->colors4f_array, coloursarray + meshlist->vbofirstvert, meshlist);
		}
		GL_SelectVBO(0);
		qglColorPointer(4, GL_FLOAT, 0, coloursarray);
		qglEnableClientState(GL_COLOR_ARRAY);
	}
}

static void BE_GeneratePassTC(const shaderpass_t *pass, int passno, const mesh_t *meshlist)
{
	pass += passno;
	if (!pass->numtcmods)
	{
		//if there are no tcmods, pass through here as fast as possible
		if (pass->tcgen == TC_GEN_BASE)
		{
			GL_SelectVBO(shaderstate.sourcevbo->vbotexcoord);
			qglTexCoordPointer(2, GL_FLOAT, 0, shaderstate.sourcevbo->texcoord);
		}
		else if (pass->tcgen == TC_GEN_LIGHTMAP)
		{
			GL_SelectVBO(shaderstate.sourcevbo->vbolmcoord);
			qglTexCoordPointer(2, GL_FLOAT, 0, shaderstate.sourcevbo->lmcoord);
		}
		else if (pass->tcgen == TC_GEN_NORMAL)
		{
			GL_SelectVBO(shaderstate.sourcevbo->vbonormals);
			qglTexCoordPointer(3, GL_FLOAT, 0, shaderstate.sourcevbo->normals);
		}
		else if (pass->tcgen == TC_GEN_SVECTOR)
		{
			GL_SelectVBO(shaderstate.sourcevbo->vbosvector);
			qglTexCoordPointer(3, GL_FLOAT, 0, shaderstate.sourcevbo->svector);
		}
		else if (pass->tcgen == TC_GEN_TVECTOR)
		{
			GL_SelectVBO(shaderstate.sourcevbo->vbotvector);
			qglTexCoordPointer(3, GL_FLOAT, 0, shaderstate.sourcevbo->tvector);
		}
		else
		{
			//specular highlights and reflections have no fixed data, and must be generated.
			GenerateTCMods(pass, passno, meshlist);
		}
	}
	else
	{
		GenerateTCMods(pass, passno, meshlist);
	}
}

static void BE_SendPassBlendAndDepth(unsigned int sbits)
{
	unsigned int delta;

	/*2d mode doesn't depth test or depth write*/
#pragma message("fixme: q3 doesn't seem to have this, why do we need it?")
	if (shaderstate.force2d)
	{
		sbits &= ~(SBITS_MISC_DEPTHWRITE|SBITS_MISC_DEPTHEQUALONLY);
		sbits |= SBITS_MISC_NODEPTHTEST;
	}

	delta = sbits^shaderstate.shaderbits;

#pragma message("Hack to work around the fact that other bits of code change this state")
	delta |= SBITS_MISC_NODEPTHTEST|SBITS_MISC_DEPTHEQUALONLY;
	if (!delta)
		return;
	shaderstate.shaderbits = sbits;

	if (delta & SBITS_BLEND_BITS)
	{
		if (sbits & SBITS_BLEND_BITS)
		{
			int src, dst;
			/*unpack the src and dst factors*/
			switch(sbits & SBITS_SRCBLEND_BITS)
			{
			case SBITS_SRCBLEND_ZERO:					src = GL_ZERO;	break;
			case SBITS_SRCBLEND_ONE:					src = GL_ONE;	break;
			case SBITS_SRCBLEND_DST_COLOR:				src = GL_DST_COLOR;	break;
			case SBITS_SRCBLEND_ONE_MINUS_DST_COLOR:	src = GL_ONE_MINUS_DST_COLOR;	break;
			case SBITS_SRCBLEND_SRC_ALPHA:				src = GL_SRC_ALPHA;	break;
			case SBITS_SRCBLEND_ONE_MINUS_SRC_ALPHA:	src = GL_ONE_MINUS_SRC_ALPHA;	break;
			case SBITS_SRCBLEND_DST_ALPHA:				src = GL_DST_ALPHA;	break;
			case SBITS_SRCBLEND_ONE_MINUS_DST_ALPHA:	src = GL_ONE_MINUS_DST_ALPHA;	break;
			case SBITS_SRCBLEND_ALPHA_SATURATE:			src = GL_SRC_ALPHA_SATURATE;	break;
			default: Sys_Error("Invalid shaderbits\n");
			}
			switch(sbits & SBITS_DSTBLEND_BITS)
			{
			case SBITS_DSTBLEND_ZERO:					dst = GL_ZERO;	break;
			case SBITS_DSTBLEND_ONE:					dst = GL_ONE;	break;
			case SBITS_DSTBLEND_SRC_COLOR:				dst = GL_SRC_COLOR;	break;
			case SBITS_DSTBLEND_ONE_MINUS_SRC_COLOR:	dst = GL_ONE_MINUS_SRC_COLOR;	break;
			case SBITS_DSTBLEND_SRC_ALPHA:				dst = GL_SRC_ALPHA;	break;
			case SBITS_DSTBLEND_ONE_MINUS_SRC_ALPHA:	dst = GL_ONE_MINUS_SRC_ALPHA;	break;
			case SBITS_DSTBLEND_DST_ALPHA:				dst = GL_DST_ALPHA;	break;
			case SBITS_DSTBLEND_ONE_MINUS_DST_ALPHA:	dst = GL_ONE_MINUS_DST_ALPHA;	break;
			default: Sys_Error("Invalid shaderbits\n");
			}
			qglEnable(GL_BLEND);
			qglBlendFunc(src, dst);
		}
		else
			qglDisable(GL_BLEND);
	}

	if (delta & SBITS_ATEST_BITS)
	{
		switch (sbits & SBITS_ATEST_BITS)
		{
		default:
			qglDisable(GL_ALPHA_TEST);
			break;
		case SBITS_ATEST_GT0:
			qglEnable(GL_ALPHA_TEST);
			qglAlphaFunc(GL_GREATER, 0);
		case SBITS_ATEST_LT128:
			qglEnable(GL_ALPHA_TEST);
			qglAlphaFunc(GL_LESS, 0.5f);
			break;
		case SBITS_ATEST_GE128:
			qglEnable(GL_ALPHA_TEST);
			qglAlphaFunc(GL_GEQUAL, 0.5f);
			break;
		}
	}

	if (delta & SBITS_MISC_DEPTHWRITE)
	{
		if (sbits & SBITS_MISC_DEPTHWRITE)
			qglDepthMask(GL_TRUE);
		else
			qglDepthMask(GL_FALSE);
	}
	if (delta & SBITS_MISC_NODEPTHTEST)
	{
		if (sbits & SBITS_MISC_NODEPTHTEST)
			qglDisable(GL_DEPTH_TEST);
		else
			qglEnable(GL_DEPTH_TEST);
	}
	if (delta & (SBITS_MISC_DEPTHEQUALONLY|SBITS_MISC_DEPTHCLOSERONLY))
	{
		extern int gldepthfunc;
		switch (sbits & (SBITS_MISC_DEPTHEQUALONLY|SBITS_MISC_DEPTHCLOSERONLY))
		{
		case SBITS_MISC_DEPTHEQUALONLY:
			qglDepthFunc(GL_EQUAL);
			break;
		case SBITS_MISC_DEPTHCLOSERONLY:
			if (gldepthfunc == GL_LEQUAL)
				qglDepthFunc(GL_LESS);
			else
				qglDepthFunc(GL_GREATER);
			break;
		default:
			qglDepthFunc(gldepthfunc);
			break;
		}
	}
}

static void BE_SubmitMeshChain(const mesh_t *meshlist)
{
	int startv, starti, endv, endi;
	while (meshlist)
	{
		startv = meshlist->vbofirstvert;
		starti = meshlist->vbofirstelement;

		endv = startv+meshlist->numvertexes;
		endi = starti+meshlist->numindexes;

		//find consecutive surfaces
		for (meshlist = meshlist->next; meshlist; meshlist = meshlist->next)
		{
			if (endi == meshlist->vbofirstelement)
			{
				endv = meshlist->vbofirstvert+meshlist->numvertexes;
				endi = meshlist->vbofirstelement+meshlist->numindexes;
			}
			else
			{
				break;
			}
		}

		qglDrawRangeElements(GL_TRIANGLES, startv, endv, endi-starti, GL_INDEX_TYPE, shaderstate.sourcevbo->indicies + starti);
	}
}

static void DrawPass(const shaderpass_t *pass, const mesh_t *meshlist)
{
	int i;
	int tmu;
	int lastpass = pass->numMergedPasses;

	for (i = 0; i < lastpass; i++)
	{
		if (pass[i].texgen == T_GEN_UPPEROVERLAY && !TEXVALID(shaderstate.curtexnums->upperoverlay))
			continue;
		if (pass[i].texgen == T_GEN_LOWEROVERLAY && !TEXVALID(shaderstate.curtexnums->loweroverlay))
			continue;
		if (pass[i].texgen == T_GEN_FULLBRIGHT && !TEXVALID(shaderstate.curtexnums->fullbright))
			continue;
		break;
	}
	if (i == lastpass)
		return;

	checkerror();
	BE_SendPassBlendAndDepth(pass[i].shaderbits);
	GenerateColourMods(pass+i, meshlist);
	checkerror();
	tmu = 0;
	for (; i < lastpass; i++)
	{
		if (pass[i].texgen == T_GEN_UPPEROVERLAY && !TEXVALID(shaderstate.curtexnums->upperoverlay))
			continue;
		if (pass[i].texgen == T_GEN_LOWEROVERLAY && !TEXVALID(shaderstate.curtexnums->loweroverlay))
			continue;
		if (pass[i].texgen == T_GEN_FULLBRIGHT && !TEXVALID(shaderstate.curtexnums->fullbright))
			continue;
		GL_MBind(tmu, Shader_TextureForPass(pass+i));

		checkerror();
		BE_GeneratePassTC(pass, i, meshlist);

		checkerror();
		if (tmu >= shaderstate.lastpasstmus)
		{
			qglEnable(GL_TEXTURE_2D);
			qglEnableClientState(GL_TEXTURE_COORD_ARRAY);
		}


		switch (pass[i].blendmode)
		{
		case GL_DOT3_RGB_ARB:
			GL_TexEnv(GL_COMBINE_EXT);
			qglTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB_ARB, GL_TEXTURE);
			qglTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_RGB_ARB, GL_PREVIOUS_ARB);
			qglTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB_EXT, pass[i].blendmode);
			break;
		case GL_REPLACE:
			GL_TexEnv(GL_REPLACE);
			break;
		case GL_DECAL:
		case GL_ADD:
			if (tmu != 0)
			{
				GL_TexEnv(pass[i].blendmode);
				break;
			}
		default:
		case GL_MODULATE:
			GL_TexEnv(GL_MODULATE);
			break;
		}
		checkerror();
		tmu++;
	}
	checkerror();

	for (i = tmu; i < shaderstate.lastpasstmus; i++)
	{
		GL_SelectTexture(i);
		qglDisableClientState(GL_TEXTURE_COORD_ARRAY);
		qglDisable(GL_TEXTURE_2D);
	}
	shaderstate.lastpasstmus = tmu;
	GL_ApplyVertexPointer();

	BE_SubmitMeshChain(meshlist);

	checkerror();
}
void Matrix4_TransformN3(float *matrix, float *vector, float *product)
{
	product[0] = -matrix[12] - matrix[0]*vector[0] - matrix[4]*vector[1] - matrix[8]*vector[2];
	product[1] = -matrix[13] - matrix[1]*vector[0] - matrix[5]*vector[1] - matrix[9]*vector[2];
	product[2] = -matrix[14] - matrix[2]*vector[0] - matrix[6]*vector[1] - matrix[10]*vector[2];
}

static void BE_RenderMeshProgram(const shader_t *shader, const shaderpass_t *pass, const mesh_t *meshlist)
{
	const shader_t *s = shader;
	int	i;
	vec3_t param3;
	float m16[16];
	int r, g, b;

	BE_SendPassBlendAndDepth(pass->shaderbits);
	GenerateColourMods(pass, meshlist);

	for ( i = 0; i < pass->numMergedPasses; i++)
	{
		GL_MBind(i, Shader_TextureForPass(pass+i));
		if (i >= shaderstate.lastpasstmus)
		{
			qglEnable(GL_TEXTURE_2D);
			qglEnableClientState(GL_TEXTURE_COORD_ARRAY);
		}
		BE_GeneratePassTC(pass, i, meshlist);
	}
	for (; i < shaderstate.lastpasstmus; i++)
	{
		GL_SelectTexture(i);
		qglDisableClientState(GL_TEXTURE_COORD_ARRAY);
		qglDisable(GL_TEXTURE_2D);
	}
	shaderstate.lastpasstmus = pass->numMergedPasses;

	GLSlang_UseProgram(s->programhandle.glsl);
	for (i = 0; i < s->numprogparams; i++)
	{
		switch(s->progparm[i].type)
		{
		case SP_TIME:
			qglUniform1fARB(s->progparm[i].handle, shaderstate.curtime);
			break;
		case SP_ENTMATRIX:
			Matrix4_ModelMatrixFromAxis(m16, currententity->axis[0], currententity->axis[1], currententity->axis[2], currententity->origin);
/*			VectorCopy(currententity->axis[0], m16+0);
			m16[3] = 0;
			VectorCopy(currententity->axis[1], m16+1);
			m16[7] = 0;
			VectorCopy(currententity->axis[2], m16+2);
			m16[11] = 0;
			VectorCopy(currententity->origin, m16+3);
			m16[15] = 1;
			*/
			qglUniformMatrix4fvARB(s->progparm[i].handle, 1, false, m16);
			break;
		case SP_ENTCOLOURS:
			qglUniform4fvARB(s->progparm[i].handle, 1, currententity->shaderRGBAf);
			break;
		case SP_TOPCOLOURS:
			R_FetchTopColour(&r, &g, &b);
			param3[0] = r/255;
			param3[1] = g/255;
			param3[2] = b/255;
			qglUniform3fvARB(s->progparm[i].handle, 1, param3);
			break;
		case SP_BOTTOMCOLOURS:
			R_FetchBottomColour(&r, &g, &b);
			param3[0] = r/255;
			param3[1] = g/255;
			param3[2] = b/255;
			qglUniform3fvARB(s->progparm[i].handle, 1, param3);
			break;

		case SP_LIGHTRADIUS:
			qglUniform1fARB(s->progparm[i].handle, shaderstate.lightradius);
			break;
		case SP_LIGHTCOLOUR:
			qglUniform3fvARB(s->progparm[i].handle, 1, shaderstate.lightcolours);
			break;
		case SP_EYEPOS:
			{
#pragma message("is this correct?")
//				vec3_t t1;
				vec3_t t2;
				Matrix4_ModelMatrixFromAxis(m16, currententity->axis[0], currententity->axis[1], currententity->axis[2], currententity->origin);
				Matrix4_Transform3(m16, r_origin, t2);
//				VectorSubtract(r_origin, currententity->origin, t1);
//				Matrix3_Multiply_Vec3(currententity->axis, t1, t2);
				qglUniform3fvARB(s->progparm[i].handle, 1, t2);
			}
			break;
		case SP_LIGHTPOSITION:
			{
#pragma message("is this correct?")
				float inv[16];
//				vec3_t t1;
				vec3_t t2;
				qboolean Matrix4_Invert(const float *m, float *out);

				Matrix4_ModelMatrixFromAxis(m16, currententity->axis[0], currententity->axis[1], currententity->axis[2], currententity->origin);
				Matrix4_Invert(m16, inv);
				Matrix4_Transform3(inv, shaderstate.lightorg, t2);
//				VectorSubtract(shaderstate.lightorg, currententity->origin, t1);
//				Matrix3_Multiply_Vec3(currententity->axis, t1, t2);
				qglUniform3fvARB(s->progparm[i].handle, 1, t2);
			}
			break;

		default:
			Host_EndGame("Bad shader program parameter type (%i)", s->progparm[i].type);
			break;
		}
	}
	GL_ApplyVertexPointer();
	BE_SubmitMeshChain(meshlist);
	GLSlang_UseProgram(0);
}

#ifdef RTLIGHTS
qboolean BE_LightCullModel(vec3_t org, model_t *model)
{
	if (shaderstate.mode == BEM_LIGHT || shaderstate.mode == BEM_STENCIL)
	{
		float dist;
		vec3_t disp;
		if (model->type == mod_alias)
		{
			VectorSubtract(org, shaderstate.lightorg, disp);
			dist = DotProduct(disp, disp);
			if (dist > model->radius*model->radius + shaderstate.lightradius*shaderstate.lightradius)
				return true;
		}
		else
		{
			int i;

			for (i = 0; i < 3; i++)
			{
				if (shaderstate.lightorg[i]-shaderstate.lightradius > org[i] + model->maxs[i])
					return true;
				if (shaderstate.lightorg[i]+shaderstate.lightradius < org[i] + model->mins[i])
					return true;
			}
		}
	}
	return false;
}
#endif

//Note: Be cautious about using BEM_LIGHT here.
void BE_SelectMode(backendmode_t mode, unsigned int flags)
{
	extern int gldepthfunc;

#ifdef RTLIGHTS
	if (mode != shaderstate.mode)
	{
qglDisable(GL_POLYGON_OFFSET_FILL);
shaderstate.curpolyoffset.factor = 0;
shaderstate.curpolyoffset.unit = 0;

		if (mode == BEM_STENCIL)
		{
			qglColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);

shaderstate.curpolyoffset.factor = 0;
shaderstate.curpolyoffset.unit = 0;
qglEnable(GL_POLYGON_OFFSET_FILL);
qglPolygonOffset(shaderstate.curpolyoffset.factor, shaderstate.curpolyoffset.unit);

			/*BEM_STENCIL doesn't support mesh writing*/
			qglDisableClientState(GL_COLOR_ARRAY);
			//disable all tmus
			while(shaderstate.lastpasstmus>0)
			{
				GL_SelectTexture(--shaderstate.lastpasstmus);
				qglDisable(GL_TEXTURE_2D);
				qglDisableClientState(GL_TEXTURE_COORD_ARRAY);
			}
			qglShadeModel(GL_FLAT);
			//replace mode please
			GL_TexEnv(GL_REPLACE);

			//we don't write or blend anything (maybe alpha test... but mneh)
			BE_SendPassBlendAndDepth(SBITS_MISC_DEPTHCLOSERONLY);

			//don't change cull stuff, and 
			//don't actually change stencil stuff - caller needs to be 
			//aware of how many times stuff is drawn, so they can do that themselves.
		}
		if (mode == BEM_DEPTHONLY)
		{
			qglColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
			/*BEM_DEPTHONLY does support mesh writing, but its not the only way its used... FIXME!*/
			qglDisableClientState(GL_COLOR_ARRAY);
			while(shaderstate.lastpasstmus>0)
			{
				GL_SelectTexture(--shaderstate.lastpasstmus);
				qglDisable(GL_TEXTURE_2D);
				qglDisableClientState(GL_TEXTURE_COORD_ARRAY);
			}
			qglShadeModel(GL_FLAT);

			//we don't write or blend anything (maybe alpha test... but mneh)
			BE_SendPassBlendAndDepth(SBITS_MISC_DEPTHWRITE);

			GL_TexEnv(GL_REPLACE);
			qglCullFace(GL_FRONT);
		}
		if (shaderstate.mode == BEM_STENCIL || shaderstate.mode == BEM_DEPTHONLY)
			qglColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);


		if (mode == BEM_SMAPLIGHT)
		{
			if (!shaderstate.initedpcfpasses)
			{
				int i;
				shaderstate.initedpcfpasses = true;
				for (i = 0; i < PERMUTATIONS; i++)
				{
					shaderstate.pcfpassshader[i] = R_RegisterCustom(pcfpassname[i], Shader_LightPass_PCF, permutationdefines[i]);
				}
			}
		}
		if (mode == BEM_LIGHT)
		{
			if (!shaderstate.initedlightpasses)
			{
				int i;
				shaderstate.initedlightpasses = true;
				for (i = 0; i < PERMUTATIONS; i++)
				{
					shaderstate.lightpassshader[i] = R_RegisterCustom(lightpassname[i], Shader_LightPass_Std, permutationdefines[i]);
				}
			}
		}
	}
#endif
	shaderstate.mode = mode;
	shaderstate.flags = flags;
}

#ifdef RTLIGHTS
void BE_SelectDLight(dlight_t *dl, vec3_t colour)
{
	shaderstate.lightradius = dl->radius;
	VectorCopy(dl->origin, shaderstate.lightorg);
	VectorCopy(colour, shaderstate.lightcolours);
	shaderstate.curshadowmap = dl->stexture;
}
#endif

static void DrawMeshChain(const mesh_t *meshlist)
{
	const shaderpass_t *p;
	int passno,perm;
	passno = 0;

	GL_SelectEBO(shaderstate.sourcevbo->vboe);
	if (shaderstate.curshader->numdeforms)
		GenerateVertexDeforms(shaderstate.curshader, meshlist);
	else
	{
		shaderstate.pendingvertexpointer = shaderstate.sourcevbo->coord;
		shaderstate.pendingvertexvbo = shaderstate.sourcevbo->vbocoord;
	}

	if (shaderstate.curcull != (shaderstate.curshader->flags & (SHADER_CULL_FRONT|SHADER_CULL_BACK)))
	{
		shaderstate.curcull = (shaderstate.curshader->flags & (SHADER_CULL_FRONT|SHADER_CULL_BACK));

		if (shaderstate.curcull & SHADER_CULL_FRONT)
		{
			qglEnable(GL_CULL_FACE);
			qglCullFace(GL_FRONT);
		}
		else if (shaderstate.curcull & SHADER_CULL_BACK)
		{
			qglEnable(GL_CULL_FACE);
			qglCullFace(GL_BACK);
		}
		else
		{
			qglDisable(GL_CULL_FACE);
		}
	}

	if (shaderstate.curentity != &r_worldentity)
	{
		/*some quake doors etc are flush with the walls that they're meant to be hidden behind, or plats the same height as the floor, etc
		we move them back very slightly using polygonoffset to avoid really ugly z-fighting*/
		extern cvar_t r_polygonoffset_submodel_offset, r_polygonoffset_submodel_factor;
		polyoffset_t po;
		po.factor = shaderstate.curshader->polyoffset.factor + r_polygonoffset_submodel_factor.value;
		po.unit = shaderstate.curshader->polyoffset.unit + r_polygonoffset_submodel_offset.value;

		if (((int*)&shaderstate.curpolyoffset)[0] != ((int*)&po)[0] || ((int*)&shaderstate.curpolyoffset)[1] != ((int*)&po)[1])
		{
			shaderstate.curpolyoffset = po;
			if (shaderstate.curpolyoffset.factor || shaderstate.curpolyoffset.unit)
			{
				qglEnable(GL_POLYGON_OFFSET_FILL);
				qglPolygonOffset(shaderstate.curpolyoffset.factor, shaderstate.curpolyoffset.unit);
			}
			else
				qglDisable(GL_POLYGON_OFFSET_FILL);
		}
	}
	else
	{
		if (*(int*)&shaderstate.curpolyoffset != *(int*)&shaderstate.curshader->polyoffset || *(int*)&shaderstate.curpolyoffset != *(int*)&shaderstate.curshader->polyoffset)
		{
			shaderstate.curpolyoffset = shaderstate.curshader->polyoffset;
			if (shaderstate.curpolyoffset.factor || shaderstate.curpolyoffset.unit)
			{
				qglEnable(GL_POLYGON_OFFSET_FILL);
				qglPolygonOffset(shaderstate.curpolyoffset.factor, shaderstate.curpolyoffset.unit);
			}
			else
				qglDisable(GL_POLYGON_OFFSET_FILL);
		}
	}

	switch(shaderstate.mode)
	{
	case BEM_STENCIL:
		Host_Error("Shader system is not meant to accept stencil meshes\n");
		break;
	case BEM_SMAPLIGHT:
		perm = 0;
		if (TEXVALID(shaderstate.curtexnums->bump) && shaderstate.pcfpassshader[perm|PERMUTATION_BUMPMAP])
			perm |= PERMUTATION_BUMPMAP;
		if (TEXVALID(shaderstate.curtexnums->specular) && shaderstate.pcfpassshader[perm|PERMUTATION_SPECULAR])
			perm |= PERMUTATION_SPECULAR;
		if (r_shadow_glsl_offsetmapping.ival && TEXVALID(shaderstate.curtexnums->bump) && shaderstate.pcfpassshader[perm|PERMUTATION_OFFSET])
			perm |= PERMUTATION_OFFSET;
		BE_RenderMeshProgram(shaderstate.pcfpassshader[perm], shaderstate.pcfpassshader[perm]->passes, meshlist);
		break;
	case BEM_LIGHT:
		perm = 0;
		if (TEXVALID(shaderstate.curtexnums->bump) && shaderstate.lightpassshader[perm|PERMUTATION_BUMPMAP])
			perm |= PERMUTATION_BUMPMAP;
		if (TEXVALID(shaderstate.curtexnums->specular) && shaderstate.lightpassshader[perm|PERMUTATION_SPECULAR])
			perm |= PERMUTATION_SPECULAR;
		if (r_shadow_glsl_offsetmapping.ival && TEXVALID(shaderstate.curtexnums->bump) && shaderstate.lightpassshader[perm|PERMUTATION_OFFSET])
			perm |= PERMUTATION_OFFSET;
		BE_RenderMeshProgram(shaderstate.lightpassshader[perm], shaderstate.lightpassshader[perm]->passes, meshlist);
		break;

	case BEM_DEPTHONLY:
#pragma message("fixme: support alpha test")
		GL_ApplyVertexPointer();
		BE_SubmitMeshChain(meshlist);
		break;

	case BEM_DEPTHDARK:
		if (shaderstate.curshader->flags & SHADER_HASLIGHTMAP)
		{
			qglColor3f(0,0,0);
			qglDisableClientState(GL_COLOR_ARRAY);
			while(shaderstate.lastpasstmus>0)
			{
				GL_SelectTexture(--shaderstate.lastpasstmus);
				qglDisable(GL_TEXTURE_2D);
				qglDisableClientState(GL_TEXTURE_COORD_ARRAY);
			}
			GL_TexEnv(GL_REPLACE);
			BE_SendPassBlendAndDepth(shaderstate.curshader->passes[0].shaderbits);

			GL_ApplyVertexPointer();
			BE_SubmitMeshChain(meshlist);
			break;
		}
		//fallthrough
	case BEM_STANDARD:
	default:
		if (shaderstate.curshader->programhandle.glsl)
			BE_RenderMeshProgram(shaderstate.curshader, shaderstate.curshader->passes, meshlist);
		else
		{
			while (passno < shaderstate.curshader->numpasses)
			{
				p = &shaderstate.curshader->passes[passno];
				passno += p->numMergedPasses;
		//		if (p->flags & SHADER_PASS_DETAIL)
		//			continue;

				DrawPass(p, meshlist);
			}
		}
		break;
	}
}

void BE_DrawMeshChain(shader_t *shader, mesh_t *meshchain, vbo_t *vbo, texnums_t *texnums)
{
	if (!vbo)
	{
		mesh_t *m;
		shaderstate.sourcevbo = &shaderstate.dummyvbo;
		shaderstate.curshader = shader;
		shaderstate.curentity = currententity;
		shaderstate.curtexnums = texnums;
		shaderstate.curlightmap = r_nulltex;
		shaderstate.curdeluxmap = r_nulltex;
		shaderstate.curtime = realtime;

		while (meshchain)
		{
			m = meshchain;
			meshchain = meshchain->next;

			shaderstate.dummyvbo.coord = m->xyz_array;
			shaderstate.dummyvbo.texcoord = m->st_array;
			shaderstate.dummyvbo.indicies = m->indexes;
			shaderstate.dummyvbo.normals = m->normals_array;
			shaderstate.dummyvbo.svector = m->snormals_array;
			shaderstate.dummyvbo.tvector = m->tnormals_array;
			shaderstate.dummyvbo.colours4f = m->colors4f_array;

			m->next = NULL;
			DrawMeshChain(m);
			m->next = meshchain;
		}
	}
	else
	{
		shaderstate.sourcevbo = vbo;
		shaderstate.curshader = shader;
		shaderstate.curentity = currententity;
		shaderstate.curtexnums = texnums;
		shaderstate.curlightmap = r_nulltex;
		shaderstate.curdeluxmap = r_nulltex;
		shaderstate.curtime = realtime;

		DrawMeshChain(meshchain);
	}
}

//FIXME: Legacy code
void R_RenderMeshBuffer(meshbuffer_t *mb, qboolean shadowpass)
{
	mesh_t *m;
	if (!shaderstate.pushedmeshes)
		return;

	BE_SelectMode(BEM_STANDARD, 0);
	shaderstate.sourcevbo = &shaderstate.dummyvbo;
	shaderstate.curshader = mb->shader;
	shaderstate.curentity = mb->entity;
	shaderstate.curtexnums = NULL;
	shaderstate.curlightmap = r_nulltex;
	shaderstate.curdeluxmap = r_nulltex;
	if (shaderstate.force2d || !shaderstate.curentity)
		shaderstate.curtime = realtime;
	else
		shaderstate.curtime = r_refdef.time - shaderstate.curentity->shaderTime;

	while (shaderstate.pushedmeshes)
	{
		m = shaderstate.pushedmeshes;
		shaderstate.pushedmeshes = m->next;
		m->next = NULL;
		shaderstate.dummyvbo.coord = m->xyz_array;
		shaderstate.dummyvbo.texcoord = m->st_array;
		shaderstate.dummyvbo.indicies = m->indexes;
		shaderstate.dummyvbo.normals = m->normals_array;
		shaderstate.dummyvbo.svector = m->snormals_array;
		shaderstate.dummyvbo.tvector = m->tnormals_array;
		shaderstate.dummyvbo.colours4f = m->colors4f_array;
		if (m->vbofirstvert || m->vbofirstelement)
			return;
		DrawMeshChain(m);
	}
}
void R_PushMesh(mesh_t *mesh, int features)
{
	mesh->next = shaderstate.pushedmeshes;
	shaderstate.pushedmeshes = mesh;
}

static void DrawSurfaceChain(msurface_t *s, shader_t *shader, vbo_t *vbo)
{	//doesn't merge surfaces, but tells gl to do each vertex arrayed surface individually, which means no vertex copying.
	int i;
	mesh_t *ml, *m;

	if (!vbo)
		return;
	
	ml = NULL;
	for (; s ; s=s->texturechain)
	{
		m = s->mesh;
		if (!m)	//urm.
			continue;
		if (m->numvertexes <= 1)
			continue;

		if (s->lightmaptexturenum < 0)
		{
			m->next = ml;
			ml = m;
		}
		else
		{
			m->next = lightmap[s->lightmaptexturenum]->meshchain;
			lightmap[s->lightmaptexturenum]->meshchain = m;
		}
	}

	shaderstate.sourcevbo = vbo;
	shaderstate.curshader = shader;
	shaderstate.curentity = currententity;
	shaderstate.curtime = realtime;

	if (ml)
	{
		shaderstate.curlightmap = r_nulltex;
		shaderstate.curdeluxmap = r_nulltex;
		DrawMeshChain(ml);
	}
	checkerror();
	for (i = 0; i < numlightmaps; i++)
	{
		if (!lightmap[i] || !lightmap[i]->meshchain)
			continue;

		if (lightmap[i]->modified)
		{
			glRect_t *theRect;
			lightmap[i]->modified = false;
			theRect = &lightmap[i]->rectchange;
			GL_Bind(lightmap_textures[i]);
			qglTexSubImage2D(GL_TEXTURE_2D, 0, 0, theRect->t, 
				LMBLOCK_WIDTH, theRect->h, ((lightmap_bytes==3)?GL_RGB:GL_LUMINANCE), GL_UNSIGNED_BYTE,
				lightmap[i]->lightmaps+(theRect->t) *LMBLOCK_WIDTH*lightmap_bytes);
			theRect->l = LMBLOCK_WIDTH;
			theRect->t = LMBLOCK_HEIGHT;
			theRect->h = 0;
			theRect->w = 0;
			checkerror();

			if (lightmap[i]->deluxmodified)
			{
				lightmap[i]->deluxmodified = false;
				theRect = &lightmap[i]->deluxrectchange;
				GL_Bind(deluxmap_textures[i]);
				qglTexSubImage2D(GL_TEXTURE_2D, 0, 0, theRect->t, 
					LMBLOCK_WIDTH, theRect->h, GL_RGB, GL_UNSIGNED_BYTE,
					lightmap[i]->deluxmaps+(theRect->t) *LMBLOCK_WIDTH*3);
				theRect->l = LMBLOCK_WIDTH;
				theRect->t = LMBLOCK_HEIGHT;
				theRect->h = 0;
				theRect->w = 0;
				checkerror();
			}
		}

		shaderstate.curlightmap = lightmap_textures[i];
		shaderstate.curdeluxmap = deluxmap_textures[i];
		DrawMeshChain(lightmap[i]->meshchain);
		lightmap[i]->meshchain = NULL;
	}
}


static void BE_BaseTextureChain(msurface_t *first)
{
	texture_t *t, *tex;
	shader_t *shader;
	t = first->texinfo->texture;
	tex = R_TextureAnimation (t);

	//TEMP: use shader as an input parameter, not tex.
	shader = tex->shader;
	if (!shader)
	{
		shader = R_RegisterShader_Lightmap(tex->name);
		tex->shader = shader;
	}

	shaderstate.curtexnums = &shader->defaulttextures;
	DrawSurfaceChain(first, shader, &t->vbo);
}

static void BaseBrushTextures(entity_t *ent)
{
	int i;
	msurface_t *s, *chain;
	model_t *model;

	if (BE_LightCullModel(ent->origin, ent->model))
		return;

	qglPushMatrix();
	R_RotateForEntity(ent);

	ent->shaderRGBAf[0] = 1;
	ent->shaderRGBAf[1] = 1;
	ent->shaderRGBAf[2] = 1;
	ent->shaderRGBAf[3] = 1;

	model = ent->model;
	chain = NULL;

// calculate dynamic lighting for bmodel if it's not an
// instanced model
	if (model->fromgame != fg_quake3)
	{
		int k;
		int shift;

		if (model->nummodelsurfaces != 0 && r_dynamic.value)
		{
			for (k=rtlights_first; k<RTL_FIRST; k++)
			{
				if (!cl_dlights[k].radius)
					continue;
				if (!(cl_dlights[k].flags & LFLAG_ALLOW_LMHACK))
					continue;

				model->funcs.MarkLights (&cl_dlights[k], 1<<k,
					model->nodes + model->hulls[0].firstclipnode);
			}
		}

		shift = Surf_LightmapShift(model);

//update lightmaps.
		for (s = model->surfaces+model->firstmodelsurface,i = 0; i < model->nummodelsurfaces; i++, s++)
			Surf_RenderDynamicLightmaps (s, shift);
	}

	for (s = model->surfaces+model->firstmodelsurface,i = 0; i < model->nummodelsurfaces; i++, s++)
	{
		if (chain && s->texinfo->texture != chain->texinfo->texture)	//last surface or not the same as the next
		{
			BE_BaseTextureChain(chain);
			chain = NULL;
		}

		s->texturechain = chain;
		chain = s;
	}

	if (chain)
		BE_BaseTextureChain(chain);

	qglPopMatrix();
}

#ifdef RTLIGHTS
void BE_BaseEntShadowDepth(void)
{
	extern model_t *currentmodel;
	int		i;

	if (!r_drawentities.value)
		return;

	// draw sprites seperately, because of alpha blending
	for (i=0 ; i<cl_numvisedicts ; i++)
	{
		currententity = &cl_visedicts[i];
		if (!currententity->model)
			continue;
		if (currententity->model->needload)
			continue;
		if (currententity->flags & Q2RF_WEAPONMODEL)
			continue;
		switch(currententity->model->type)
		{
		case mod_brush:
			BaseBrushTextures(currententity);
			break;
		case mod_alias:
			R_DrawGAliasModel (currententity, BEM_DEPTHONLY);
			break;
		}
	}
}
#endif

void BE_BaseEntTextures(void)
{
	extern model_t *currentmodel;
	int		i;

	if (!r_drawentities.ival)
		return;

	// draw sprites seperately, because of alpha blending
	for (i=0 ; i<cl_numvisedicts ; i++)
	{
		currententity = &cl_visedicts[i];
		if (!currententity->model)
			continue;
		if (currententity->model->needload)
			continue;
		if (!PPL_ShouldDraw())
			continue;
		switch(currententity->model->type)
		{
		case mod_brush:
			BaseBrushTextures(currententity);
			break;
		case mod_alias:
			R_DrawGAliasModel (currententity, shaderstate.mode);
			break;
		}
	}
}

void BE_DrawPolys(qboolean decalsset)
{
	unsigned int i;
	mesh_t m;

	if (!cl_numstris)
		return;

	memset(&m, 0, sizeof(m));
	for (i = 0; i < cl_numstris; i++)
	{
		if ((cl_stris[i].shader->sort <= SHADER_SORT_DECAL) ^ decalsset)
			continue;

		m.xyz_array = cl_strisvertv + cl_stris[i].firstvert;
		m.st_array = cl_strisvertt + cl_stris[i].firstvert;
		m.colors4f_array = cl_strisvertc + cl_stris[i].firstvert;
		m.indexes = cl_strisidx + cl_stris[i].firstidx;
		m.numindexes = cl_stris[i].numidx;
		m.numvertexes = cl_stris[i].numvert;
		BE_DrawMeshChain(cl_stris[i].shader, &m, NULL, &cl_stris[i].shader->defaulttextures);
	}
}

void BE_SubmitMeshes (void)
{
	texture_t *t;
	msurface_t *s;
	int i;
	model_t *model = cl.worldmodel;
	unsigned int fl;
	currententity = &r_worldentity;

	for (i=0 ; i<model->numtextures ; i++)
	{
		t = model->textures[i];
		if (!t)
			continue;
		s = t->texturechain;
		if (!s)
			continue;

		fl = s->texinfo->texture->shader->flags;
		if (fl & SHADER_NODLIGHT)
			if (shaderstate.mode == BEM_LIGHT || shaderstate.mode == BEM_SMAPLIGHT)
				continue;

		if (fl & SHADER_SKY)
		{
			if (shaderstate.mode == BEM_STANDARD)
				R_DrawSkyChain (s);
		}
		else
			BE_BaseTextureChain(s);
	}

	if (shaderstate.mode == BEM_STANDARD)
		BE_DrawPolys(true);

	checkerror();
	BE_BaseEntTextures();
	checkerror();
}

void BE_DrawWorld (qbyte *vis)
{
	extern cvar_t r_shadow_realtime_world, r_shadow_realtime_world_lightmaps;
	RSpeedLocals();
	GL_DoSwap();

	//make sure the world draws correctly
	r_worldentity.shaderRGBAf[0] = 1;
	r_worldentity.shaderRGBAf[1] = 1;
	r_worldentity.shaderRGBAf[2] = 1;
	r_worldentity.shaderRGBAf[3] = 1;
	r_worldentity.axis[0][0] = 1;
	r_worldentity.axis[1][1] = 1;
	r_worldentity.axis[2][2] = 1;

	if (r_shadow_realtime_world.value)
		shaderstate.identitylighting = r_shadow_realtime_world_lightmaps.value;
	else
		shaderstate.identitylighting = 1;

	if (shaderstate.identitylighting == 0)
		BE_SelectMode(BEM_DEPTHDARK, 0);
	else
		BE_SelectMode(BEM_STANDARD, 0);

	checkerror();

	RSpeedRemark();
	BE_SubmitMeshes();
	RSpeedEnd(RSPEED_WORLD);

#ifdef RTLIGHTS
	RSpeedRemark();
	Sh_DrawLights(vis);
	RSpeedEnd(RSPEED_STENCILSHADOWS);
#endif
	checkerror();

	BE_DrawPolys(false);
}
#endif
