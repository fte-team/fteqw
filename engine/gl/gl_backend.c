#include "quakedef.h"

//#define FORCESTATE


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
varying vec4 vshadowcoord;\n\
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
	vshadowcoord = gl_TextureMatrix[7] * (entmatrix*gl_Vertex);\n\
#endif\n\
}\n\
#endif\n\
"

/*this is full 4*4 PCF, with an added attempt at prenumbra*/
/*the offset consts are 1/(imagesize*2) */
#define PCF16P(f)	"\
	float xPixelOffset = (1.0+shadowcoord.b/lightradius)/texx;\
	float yPixelOffset = (1.0+shadowcoord.b/lightradius)/texy;\
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
	const float xPixelOffset = 1.0/texx;\
	const float yPixelOffset = 1.0/texy;\
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
	float xPixelOffset = 1.0/texx;\
	float yPixelOffset = 1.0/texy;\
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
#endif\n\
""\n\
#ifdef PCF\n\
#if defined(SPOT)\n\
const float texx = 512.0;\n\
const float texy = 512.0;\n\
vec4 shadowcoord = vshadowcoord;\n\
#else\n\
const float texx = 512.0;\n\
const float texy = 512.0;\n\
vec4 shadowcoord;\n\
shadowcoord.zw = vshadowcoord.zw;\n\
shadowcoord.xy = vshadowcoord.xy;\n\
#endif\n\
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
	#define PCF\n\
	%s%s%s\n\
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


extern cvar_t r_shadow_glsl_offsetmapping, r_noportals;

#if 0//def _DEBUG
#define checkerror() if (qglGetError()) Con_Printf("Error detected at line %s:%i\n", __FILE__, __LINE__)
#else
#define checkerror()
#endif

static void BE_SendPassBlendAndDepth(unsigned int sbits);
static void BE_SubmitBatch(batch_t *batch);

void PPL_CreateShaderObjects(void){}
void PPL_BaseBModelTextures(entity_t *e){}

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
static char *spotpassname[PERMUTATIONS] =
{
	"lightpass_spot",
	"lightpass_spot_bump",
	"lightpass_spot_spec",
	"lightpass_spot_bump_spec",
	"lightpass_spot_offset",
	"lightpass_spot_offset_bump",
	"lightpass_spot_offset_spec",
	"lightpass_spot_offset_bump_spec"
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
		qboolean initedspotpasses;
		const shader_t *spotpassshader[PERMUTATIONS];

		qboolean force2d;
		int currenttmu;
		int texenvmode[SHADER_PASS_MAX];
		int currenttextures[SHADER_PASS_MAX];

		polyoffset_t curpolyoffset;
		unsigned int curcull;
		texid_t curshadowmap;

		unsigned int shaderbits;

		vbo_t dummyvbo;
		int currentvbo;
		int currentebo;

		mesh_t **meshes;
		unsigned int meshcount;

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
#ifndef FORCESTATE
	if (mode != shaderstate.texenvmode[shaderstate.currenttmu])
#endif
	{
		qglTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, mode);
		shaderstate.texenvmode[shaderstate.currenttmu] = mode;
	}
}

/*OpenGL requires glDepthMask(GL_TRUE) or glClear(GL_DEPTH_BUFFER_BIT) will fail*/
void GL_ForceDepthWritable(void)
{
	if (!(shaderstate.shaderbits & SBITS_MISC_DEPTHWRITE))
	{
		shaderstate.shaderbits |= SBITS_MISC_DEPTHWRITE;
		qglDepthMask(GL_TRUE);
	}
}

void GL_SetShaderState2D(qboolean is2d)
{
	shaderstate.force2d = is2d;
	BE_SelectMode(BEM_STANDARD, 0);
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
#ifndef FORCESTATE
	if (shaderstate.currentvbo != vbo)
#endif
	{
		shaderstate.currentvbo = vbo;
		qglBindBufferARB(GL_ARRAY_BUFFER_ARB, shaderstate.currentvbo);
	}
}
void GL_SelectEBO(int vbo)
{
#ifndef FORCESTATE
	if (shaderstate.currentebo != vbo)
#endif
	{
		shaderstate.currentebo = vbo;
		qglBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, shaderstate.currentebo);
	}
}

static void GL_ApplyVertexPointer(void)
{
#ifndef FORCESTATE
	if (shaderstate.curvertexpointer != shaderstate.pendingvertexpointer || shaderstate.pendingvertexvbo != shaderstate.curvertexvbo)
#endif
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

#ifndef FORCESTATE
	if (shaderstate.currenttextures[shaderstate.currenttmu] == texnum.num)
		return;
#endif

	shaderstate.currenttextures[shaderstate.currenttmu] = texnum.num;
	bindTexFunc (GL_TEXTURE_2D, texnum.num);
}

void GL_Bind(texid_t texnum)
{
#ifndef FORCESTATE
	if (shaderstate.currenttextures[shaderstate.currenttmu] == texnum.num)
		return;
#endif

	shaderstate.currenttextures[shaderstate.currenttmu] = texnum.num;

	bindTexFunc (GL_TEXTURE_2D, texnum.num);
}

void GL_BindType(int type, texid_t texnum)
{
#ifndef FORCESTATE
	if (shaderstate.currenttextures[shaderstate.currenttmu] == texnum.num)
		return;
#endif

	shaderstate.currenttextures[shaderstate.currenttmu] = texnum.num;
	bindTexFunc (type, texnum.num);
}

void GL_CullFace(unsigned int sflags)
{
#ifndef FORCESTATE
	if (shaderstate.curcull == sflags)
		return;
#endif
	shaderstate.curcull = sflags;

	if (shaderstate.curcull & SHADER_CULL_FRONT)
	{
		qglEnable(GL_CULL_FACE);
		qglCullFace(r_refdef.flipcull?GL_BACK:GL_FRONT);
	}
	else if (shaderstate.curcull & SHADER_CULL_BACK)
	{
		qglEnable(GL_CULL_FACE);
		qglCullFace(r_refdef.flipcull?GL_FRONT:GL_BACK);
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

	GL_TexEnv(GL_REPLACE);

	qglColor3f(1,1,1);

	shaderstate.shaderbits &= ~(SBITS_MISC_DEPTHEQUALONLY|SBITS_MISC_DEPTHCLOSERONLY);
	shaderstate.shaderbits |= SBITS_MISC_DEPTHWRITE;

	qglDepthFunc(GL_LEQUAL);
	qglDepthMask(GL_TRUE);

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

	BE_SelectMode(BEM_DEPTHONLY, 0);
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
	sprintf(shadertext, LIGHTPASS_SHADER, (char*)args, LIGHTPASS_GLSL_SHARED LIGHTPASS_GLSL_VERTEX LIGHTPASS_GLSL_FRAGMENT);
	Shader_DefaultScript(shortname, s, shadertext);
}
void Shader_LightPass_PCF(char *shortname, shader_t *s, const void *args)
{
	char shadertext[8192*2];
	sprintf(shadertext, PCFPASS_SHADER, (char*)args, "", LIGHTPASS_GLSL_SHARED LIGHTPASS_GLSL_VERTEX LIGHTPASS_GLSL_FRAGMENT);
	Shader_DefaultScript(shortname, s, shadertext);
}
void Shader_LightPass_Spot(char *shortname, shader_t *s, const void *args)
{
	char shadertext[8192*2];
	sprintf(shadertext, PCFPASS_SHADER, (char*)args, "#define SPOT\n", LIGHTPASS_GLSL_SHARED LIGHTPASS_GLSL_VERTEX LIGHTPASS_GLSL_FRAGMENT);
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
	if (r_shadow_realtime_dlight.ival && !shaderstate.initedlightpasses && gl_config.arb_shader_objects)
	{
		int i;
		shaderstate.initedlightpasses = true;
		for (i = 0; i < PERMUTATIONS; i++)
		{
			shaderstate.lightpassshader[i] = R_RegisterCustom(lightpassname[i], Shader_LightPass_Std, permutationdefines[i]);
		}
	}

	shaderstate.shaderbits = ~0;
	BE_SendPassBlendAndDepth(0);
	qglEnableClientState(GL_VERTEX_ARRAY);

	currententity = &r_worldentity;
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

	for (i = 0 ; i < numverts ; i++, xyz += sizeof(vecV_t)/sizeof(vec_t), normal += 3, st += 2 ) 
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

static void GenerateTCMods(const shaderpass_t *pass, int passnum)
{
#if 1
	int i, m;
	float *src;
	mesh_t *mesh;
	for (m = 0; m < shaderstate.meshcount; m++)
	{
		mesh = shaderstate.meshes[m];

		src = tcgen(pass, mesh->numvertexes, texcoordarray[passnum]+mesh->vbofirstvert*2, mesh);
		//tcgen might return unmodified info
		if (pass->numtcmods)
		{
			tcmod(&pass->tcmods[0], mesh->numvertexes, src, texcoordarray[passnum]+mesh->vbofirstvert*2, mesh);
			for (i = 1; i < pass->numtcmods; i++)
			{
				tcmod(&pass->tcmods[i], mesh->numvertexes, texcoordarray[passnum]+mesh->vbofirstvert*2, texcoordarray[passnum]+mesh->vbofirstvert*2, mesh);
			}
			src = texcoordarray[passnum]+mesh->vbofirstvert*2;
		}
		else if (src != texcoordarray[passnum]+mesh->vbofirstvert*2)
		{
			//this shouldn't actually ever be true
			memcpy(texcoordarray[passnum]+mesh->vbofirstvert*2, src, 8*mesh->numvertexes);
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
static void colourgen(const shaderpass_t *pass, int cnt, const vec4_t *src, vec4_t *dst, const mesh_t *mesh)
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
		if (cnt)
		{
			int r, g, b;
			R_FetchTopColour(&r, &g, &b);
			dst[0][0] = r/255.0f;
			dst[0][1] = g/255.0f;
			dst[0][2] = b/255.0f;
			while((cnt)--)
			{
				dst[cnt][0] = dst[0][0];
				dst[cnt][1] = dst[0][1];
				dst[cnt][2] = dst[0][2];
			}
		}
		break;
	case RGB_GEN_BOTTOMCOLOR:
		if (cnt)
		{
			int r, g, b;
			R_FetchBottomColour(&r, &g, &b);
			dst[0][0] = r/255.0f;
			dst[0][1] = g/255.0f;
			dst[0][2] = b/255.0f;
			while((cnt)--)
			{
				dst[cnt][0] = dst[0][0];
				dst[cnt][1] = dst[0][1];
				dst[cnt][2] = dst[0][2];
			}
		}
		break;
	}
}

static void deformgen(const deformv_t *deformv, int cnt, const vecV_t *src, vecV_t *dst, const mesh_t *mesh)
{
	float *table;
	int j, k;
	float args[4];
	float deflect;
	switch (deformv->type)
	{
	default:
	case DEFORMV_NONE:
		if (src != (const avec4_t*)dst)
			memcpy(dst, src, sizeof(*src)*cnt);
		break;

	case DEFORMV_WAVE:
		if (!mesh->normals_array)
		{
			if (src != (const avec4_t*)dst)
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
		if (src != (const avec4_t*)dst)
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
				dst[k][0] = mid[0] + radius*((mesh->st_array[k][0]-0.5)*r_refdef.m_view[0+0]-(mesh->st_array[k][1]-0.5)*r_refdef.m_view[0+1]);
				dst[k][1] = mid[1] + radius*((mesh->st_array[k][0]-0.5)*r_refdef.m_view[4+0]-(mesh->st_array[k][1]-0.5)*r_refdef.m_view[4+1]);
				dst[k][2] = mid[2] + radius*((mesh->st_array[k][0]-0.5)*r_refdef.m_view[8+0]-(mesh->st_array[k][1]-0.5)*r_refdef.m_view[8+1]);
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

static void GenerateVertexDeforms(const shader_t *shader)
{
	int i, m;
	mesh_t *meshlist;
	for (m = 0; m < shaderstate.meshcount; m++)
	{
		meshlist = shaderstate.meshes[m];

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
		if (shaderstate.flags & BEF_FORCETRANSPARENT)
		{
			while(cnt--)
				dst[cnt][3] = shaderstate.curentity->shaderRGBAf[3];
		}
		else
		{
			while(cnt--)
				dst[cnt][3] = 1;
		}
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
		if (r_refdef.recurse)
			f = 1;
		else
		{
			VectorAdd(mesh->xyz_array[0], shaderstate.curentity->origin, v1);
			VectorSubtract(r_origin, v1, v2);
			f = VectorLength(v2) * (1.0 / shaderstate.curshader->portaldist);
			f = bound(0.0f, f, 1.0f);
		}

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
				Matrix3_Multiply_Vec3(axis, v1, v2);
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

static void GenerateColourMods(const shaderpass_t *pass)
{
	unsigned int m;
	mesh_t *meshlist;
	meshlist = shaderstate.meshes[0];

	if (meshlist->colors4b_array)
	{
		//hack...
		GL_SelectVBO(0);
		qglColorPointer(4, GL_UNSIGNED_BYTE, 0, meshlist->colors4b_array);
		qglEnableClientState(GL_COLOR_ARRAY);
		qglShadeModel(GL_SMOOTH);
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

		for (m = 0; m < shaderstate.meshcount; m++)
		{
			meshlist = shaderstate.meshes[m];

			colourgen(pass, meshlist->numvertexes, meshlist->colors4f_array, coloursarray + meshlist->vbofirstvert, meshlist);
			alphagen(pass, meshlist->numvertexes, meshlist->colors4f_array, coloursarray + meshlist->vbofirstvert, meshlist);
		}
		GL_SelectVBO(0);
		qglColorPointer(4, GL_FLOAT, 0, coloursarray);
		qglEnableClientState(GL_COLOR_ARRAY);
	}
}

static void BE_GeneratePassTC(const shaderpass_t *pass, int passno)
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
			GenerateTCMods(pass, passno);
		}
	}
	else
	{
		GenerateTCMods(pass, passno);
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
	if (shaderstate.flags & ~BEF_PUSHDEPTH)
	{
		if (shaderstate.flags & BEF_FORCEADDITIVE)
			sbits = (sbits & ~(SBITS_MISC_DEPTHWRITE|SBITS_BLEND_BITS|SBITS_ATEST_BITS))
						| (SBITS_SRCBLEND_ONE | SBITS_DSTBLEND_ONE);
		else if ((shaderstate.flags & BEF_FORCETRANSPARENT) && !(sbits & SBITS_BLEND_BITS)) 	/*if transparency is forced, clear alpha test bits*/
			sbits = (sbits & ~(SBITS_MISC_DEPTHWRITE|SBITS_BLEND_BITS|SBITS_ATEST_BITS))
						| (SBITS_SRCBLEND_SRC_ALPHA | SBITS_DSTBLEND_ONE_MINUS_SRC_ALPHA);

		if (shaderstate.flags & BEF_FORCENODEPTH) 	/*EF_NODEPTHTEST dp extension*/
			sbits |= SBITS_MISC_NODEPTHTEST;
		else
		{
			if (shaderstate.flags & BEF_FORCEDEPTHTEST)
				sbits &= ~SBITS_MISC_NODEPTHTEST;
			if (shaderstate.flags & BEF_FORCEDEPTHWRITE)
				sbits |= SBITS_MISC_DEPTHWRITE;
		}
	}


	delta = sbits^shaderstate.shaderbits;

#ifdef FORCESTATE
	delta |= ~0;
#endif
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
			default:
			case SBITS_SRCBLEND_ONE:					src = GL_ONE;	break;
			case SBITS_SRCBLEND_DST_COLOR:				src = GL_DST_COLOR;	break;
			case SBITS_SRCBLEND_ONE_MINUS_DST_COLOR:	src = GL_ONE_MINUS_DST_COLOR;	break;
			case SBITS_SRCBLEND_SRC_ALPHA:				src = GL_SRC_ALPHA;	break;
			case SBITS_SRCBLEND_ONE_MINUS_SRC_ALPHA:	src = GL_ONE_MINUS_SRC_ALPHA;	break;
			case SBITS_SRCBLEND_DST_ALPHA:				src = GL_DST_ALPHA;	break;
			case SBITS_SRCBLEND_ONE_MINUS_DST_ALPHA:	src = GL_ONE_MINUS_DST_ALPHA;	break;
			case SBITS_SRCBLEND_ALPHA_SATURATE:			src = GL_SRC_ALPHA_SATURATE;	break;
			}
			switch(sbits & SBITS_DSTBLEND_BITS)
			{
			case SBITS_DSTBLEND_ZERO:					dst = GL_ZERO;	break;
			default:
			case SBITS_DSTBLEND_ONE:					dst = GL_ONE;	break;
			case SBITS_DSTBLEND_SRC_COLOR:				dst = GL_SRC_COLOR;	break;
			case SBITS_DSTBLEND_ONE_MINUS_SRC_COLOR:	dst = GL_ONE_MINUS_SRC_COLOR;	break;
			case SBITS_DSTBLEND_SRC_ALPHA:				dst = GL_SRC_ALPHA;	break;
			case SBITS_DSTBLEND_ONE_MINUS_SRC_ALPHA:	dst = GL_ONE_MINUS_SRC_ALPHA;	break;
			case SBITS_DSTBLEND_DST_ALPHA:				dst = GL_DST_ALPHA;	break;
			case SBITS_DSTBLEND_ONE_MINUS_DST_ALPHA:	dst = GL_ONE_MINUS_DST_ALPHA;	break;
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
			break;
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

	if (delta & SBITS_MISC_NODEPTHTEST)
	{
		if (sbits & SBITS_MISC_NODEPTHTEST)
			qglDisable(GL_DEPTH_TEST);
		else
			qglEnable(GL_DEPTH_TEST);
	}
	if (delta & SBITS_MISC_DEPTHWRITE)
	{
		if (sbits & SBITS_MISC_DEPTHWRITE)
			qglDepthMask(GL_TRUE);
		else
			qglDepthMask(GL_FALSE);
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

static void BE_SubmitMeshChain(void)
{
	int startv, starti, endv, endi;
	int m;
	mesh_t *mesh;

/*
	if (qglLockArraysEXT)
	{
		endv = 0;
		startv = 0x7fffffff;
		for (m = 0; m < shaderstate.meshcount; m++)
		{
			mesh = shaderstate.meshes[m];
			starti = mesh->vbofirstvert;
			if (starti < startv)
				startv = starti;
			endi = mesh->vbofirstvert+mesh->numvertexes;
			if (endi > endv)
				endv = endi;
		}
		qglLockArraysEXT(startv, endv);
	}
*/
	for (m = 0, mesh = shaderstate.meshes[0]; m < shaderstate.meshcount; )
	{
		startv = mesh->vbofirstvert;
		starti = mesh->vbofirstelement;

		endv = startv+mesh->numvertexes;
		endi = starti+mesh->numindexes;

		//find consecutive surfaces
		for (++m; m < shaderstate.meshcount; m++)
		{
			mesh = shaderstate.meshes[m];
			if (endi == mesh->vbofirstelement)
			{
				endv = mesh->vbofirstvert+mesh->numvertexes;
				endi = mesh->vbofirstelement+mesh->numindexes;
			}
			else
			{
				break;
			}
		}

		qglDrawRangeElements(GL_TRIANGLES, startv, endv, endi-starti, GL_INDEX_TYPE, shaderstate.sourcevbo->indicies + starti);
	}
/*
	if (qglUnlockArraysEXT)
		qglUnlockArraysEXT();
*/
}

static void DrawPass(const shaderpass_t *pass)
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
	GenerateColourMods(pass+i);
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
		BE_GeneratePassTC(pass, i);

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
			//fallthrough
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

	BE_SubmitMeshChain();

	checkerror();
}

static void BE_RenderMeshProgram(const shader_t *shader, const shaderpass_t *pass)
{
	const shader_t *s = shader;
	int	i;
	vec3_t param3;
	float m16[16];
	int r, g, b;

	BE_SendPassBlendAndDepth(pass->shaderbits);
	GenerateColourMods(pass);

	for ( i = 0; i < pass->numMergedPasses; i++)
	{
		GL_MBind(i, Shader_TextureForPass(pass+i));
		if (i >= shaderstate.lastpasstmus)
		{
			qglEnable(GL_TEXTURE_2D);
			qglEnableClientState(GL_TEXTURE_COORD_ARRAY);
		}
		BE_GeneratePassTC(pass, i);
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
			param3[0] = r/255.0f;
			param3[1] = g/255.0f;
			param3[2] = b/255.0f;
			qglUniform3fvARB(s->progparm[i].handle, 1, param3);
			break;
		case SP_BOTTOMCOLOURS:
			R_FetchBottomColour(&r, &g, &b);
			param3[0] = r/255.0f;
			param3[1] = g/255.0f;
			param3[2] = b/255.0f;
			qglUniform3fvARB(s->progparm[i].handle, 1, param3);
			break;

		case SP_RENDERTEXTURESCALE:
			if (gl_config.arb_texture_non_power_of_two)
			{
				param3[0] = 1;
				param3[1] = 1;
			}
			else
			{
				r = 1;
				g = 1;
				while (r < vid.pixelwidth)
					r *= 2;
				while (g < vid.pixelheight)
					g *= 2;
				param3[0] = vid.pixelwidth/(float)r;
				param3[1] = vid.pixelheight/(float)g;
			}
			param3[2] = 1;
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
	BE_SubmitMeshChain();
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

	if (mode != shaderstate.mode)
	{
#ifdef RTLIGHTS
		if (mode == BEM_STENCIL)
		{
			qglColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);

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
#endif
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
			GL_CullFace(SHADER_CULL_FRONT);
		}
		if (shaderstate.mode == BEM_STENCIL || shaderstate.mode == BEM_DEPTHONLY)
		{
			qglColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
		}
#ifdef RTLIGHTS
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
		if (mode == BEM_SMAPLIGHTSPOT)
		{
			if (!shaderstate.initedspotpasses)
			{
				int i;
				shaderstate.initedspotpasses = true;
				for (i = 0; i < PERMUTATIONS; i++)
				{
					shaderstate.spotpassshader[i] = R_RegisterCustom(spotpassname[i], Shader_LightPass_Spot, permutationdefines[i]);
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
#endif
	}
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

void BE_PushOffsetShadow(qboolean pushdepth)
{
	if (pushdepth)
	{
		/*some quake doors etc are flush with the walls that they're meant to be hidden behind, or plats the same height as the floor, etc
		we move them back very slightly using polygonoffset to avoid really ugly z-fighting*/
		extern cvar_t r_polygonoffset_submodel_offset, r_polygonoffset_submodel_factor;
		polyoffset_t po;
		po.factor = r_polygonoffset_submodel_factor.value;
		po.unit = r_polygonoffset_submodel_offset.value;

#ifndef FORCESTATE
		if (((int*)&shaderstate.curpolyoffset)[0] != ((int*)&po)[0] || ((int*)&shaderstate.curpolyoffset)[1] != ((int*)&po)[1])
#endif
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
#ifndef FORCESTATE
		if (*(int*)&shaderstate.curpolyoffset != 0 || *(int*)&shaderstate.curpolyoffset != 0)
#endif
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
}

void BE_PolyOffset(qboolean pushdepth)
{
	if (pushdepth)
	{
		/*some quake doors etc are flush with the walls that they're meant to be hidden behind, or plats the same height as the floor, etc
		we move them back very slightly using polygonoffset to avoid really ugly z-fighting*/
		extern cvar_t r_polygonoffset_submodel_offset, r_polygonoffset_submodel_factor;
		polyoffset_t po;
		po.factor = shaderstate.curshader->polyoffset.factor + r_polygonoffset_submodel_factor.value;
		po.unit = shaderstate.curshader->polyoffset.unit + r_polygonoffset_submodel_offset.value;

#ifndef FORCESTATE
		if (((int*)&shaderstate.curpolyoffset)[0] != ((int*)&po)[0] || ((int*)&shaderstate.curpolyoffset)[1] != ((int*)&po)[1])
#endif
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
#ifndef FORCESTATE
		if (*(int*)&shaderstate.curpolyoffset != *(int*)&shaderstate.curshader->polyoffset || *(int*)&shaderstate.curpolyoffset != *(int*)&shaderstate.curshader->polyoffset)
#endif
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
}

static void DrawMeshes(void)
{
	const shaderpass_t *p;
	int passno;
#ifdef RTLIGHTS
	int perm;
#endif
	passno = 0;

	GL_SelectEBO(shaderstate.sourcevbo->vboe);
	if (shaderstate.curshader->numdeforms)
		GenerateVertexDeforms(shaderstate.curshader);
	else
	{
		shaderstate.pendingvertexpointer = shaderstate.sourcevbo->coord;
		shaderstate.pendingvertexvbo = shaderstate.sourcevbo->vbocoord;
	}

#ifndef FORCESTATE
	if (shaderstate.curcull != (shaderstate.curshader->flags & (SHADER_CULL_FRONT|SHADER_CULL_BACK)))
#endif
	{
		shaderstate.curcull = (shaderstate.curshader->flags & (SHADER_CULL_FRONT|SHADER_CULL_BACK));

		if (shaderstate.curcull & SHADER_CULL_FRONT)
		{
			qglEnable(GL_CULL_FACE);
			qglCullFace(r_refdef.flipcull?GL_BACK:GL_FRONT);
		}
		else if (shaderstate.curcull & SHADER_CULL_BACK)
		{
			qglEnable(GL_CULL_FACE);
			qglCullFace(r_refdef.flipcull?GL_FRONT:GL_BACK);
		}
		else
		{
			qglDisable(GL_CULL_FACE);
		}
	}

	BE_PolyOffset(shaderstate.flags & BEF_PUSHDEPTH);

	switch(shaderstate.mode)
	{
	case BEM_STENCIL:
		Host_Error("Shader system is not meant to accept stencil meshes\n");
		break;
#ifdef RTLIGHTS
	case BEM_SMAPLIGHTSPOT:
		perm = 0;
		if (TEXVALID(shaderstate.curtexnums->bump) && shaderstate.spotpassshader[perm|PERMUTATION_BUMPMAP])
			perm |= PERMUTATION_BUMPMAP;
		if (TEXVALID(shaderstate.curtexnums->specular) && shaderstate.spotpassshader[perm|PERMUTATION_SPECULAR])
			perm |= PERMUTATION_SPECULAR;
		if (r_shadow_glsl_offsetmapping.ival && TEXVALID(shaderstate.curtexnums->bump) && shaderstate.spotpassshader[perm|PERMUTATION_OFFSET])
			perm |= PERMUTATION_OFFSET;
		BE_RenderMeshProgram(shaderstate.spotpassshader[perm], shaderstate.spotpassshader[perm]->passes);
		break;
	case BEM_SMAPLIGHT:
		perm = 0;
		if (TEXVALID(shaderstate.curtexnums->bump) && shaderstate.pcfpassshader[perm|PERMUTATION_BUMPMAP])
			perm |= PERMUTATION_BUMPMAP;
		if (TEXVALID(shaderstate.curtexnums->specular) && shaderstate.pcfpassshader[perm|PERMUTATION_SPECULAR])
			perm |= PERMUTATION_SPECULAR;
		if (r_shadow_glsl_offsetmapping.ival && TEXVALID(shaderstate.curtexnums->bump) && shaderstate.pcfpassshader[perm|PERMUTATION_OFFSET])
			perm |= PERMUTATION_OFFSET;
		BE_RenderMeshProgram(shaderstate.pcfpassshader[perm], shaderstate.pcfpassshader[perm]->passes);
		break;
	case BEM_LIGHT:
		perm = 0;
		if (TEXVALID(shaderstate.curtexnums->bump) && shaderstate.lightpassshader[perm|PERMUTATION_BUMPMAP])
			perm |= PERMUTATION_BUMPMAP;
		if (TEXVALID(shaderstate.curtexnums->specular) && shaderstate.lightpassshader[perm|PERMUTATION_SPECULAR])
			perm |= PERMUTATION_SPECULAR;
		if (r_shadow_glsl_offsetmapping.ival && TEXVALID(shaderstate.curtexnums->bump) && shaderstate.lightpassshader[perm|PERMUTATION_OFFSET])
			perm |= PERMUTATION_OFFSET;
		BE_RenderMeshProgram(shaderstate.lightpassshader[perm], shaderstate.lightpassshader[perm]->passes);
		break;
#endif
	case BEM_DEPTHONLY:
#pragma message("fixme: support alpha test")
		GL_ApplyVertexPointer();
		BE_SubmitMeshChain();
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
			BE_SubmitMeshChain();
			break;
		}
		//fallthrough
	case BEM_STANDARD:
	default:
		if (shaderstate.curshader->programhandle.glsl)
			BE_RenderMeshProgram(shaderstate.curshader, shaderstate.curshader->passes);
		else
		{
			while (passno < shaderstate.curshader->numpasses)
			{
				p = &shaderstate.curshader->passes[passno];
				passno += p->numMergedPasses;
		//		if (p->flags & SHADER_PASS_DETAIL)
		//			continue;

				DrawPass(p);
			}
		}
		break;
	}
}

void BE_DrawMesh_List(shader_t *shader, int nummeshes, mesh_t **meshlist, vbo_t *vbo, texnums_t *texnums)
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
		shaderstate.curtime = cl.servertime - currententity->shaderTime;

		while (nummeshes--)
		{
			m = *meshlist++;

			shaderstate.dummyvbo.coord = m->xyz_array;
			shaderstate.dummyvbo.texcoord = m->st_array;
			shaderstate.dummyvbo.indicies = m->indexes;
			shaderstate.dummyvbo.normals = m->normals_array;
			shaderstate.dummyvbo.svector = m->snormals_array;
			shaderstate.dummyvbo.tvector = m->tnormals_array;
			shaderstate.dummyvbo.colours4f = m->colors4f_array;

			shaderstate.meshcount = 1;
			shaderstate.meshes = &m;
			DrawMeshes();
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

		shaderstate.meshcount = nummeshes;
		shaderstate.meshes = meshlist;
		DrawMeshes();
	}
}
void BE_DrawMesh_Single(shader_t *shader, mesh_t *mesh, vbo_t *vbo, texnums_t *texnums)
{
	shader->next = NULL;
	BE_DrawMesh_List(shader, 1, &mesh, NULL, texnums);
}

static void BaseBrushTextures(entity_t *ent)
{
	int i;
	msurface_t *s, *chain;
	model_t *model;

	batch_t batch;
	mesh_t *batchmeshes[64];

	model = ent->model;

	if (R_CullEntityBox (ent, model->mins, model->maxs))
		return;

#ifdef RTLIGHTS
	if (BE_LightCullModel(ent->origin, model))
		return;
#endif

	qglPushMatrix();
	R_RotateForEntity(ent, model);

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
		if ((ent->drawflags & MLS_MASKIN) == MLS_ABSLIGHT)
		{
			//update lightmaps.
			for (s = model->surfaces+model->firstmodelsurface,i = 0; i < model->nummodelsurfaces; i++, s++)
				Surf_RenderAmbientLightmaps (s, shift, ent->abslight);
		}
		else
		{
			//update lightmaps.
			for (s = model->surfaces+model->firstmodelsurface,i = 0; i < model->nummodelsurfaces; i++, s++)
				Surf_RenderDynamicLightmaps (s, shift);
		}
	}

	memset(&batch, 0, sizeof(batch));
	batch.maxmeshes = sizeof(batchmeshes)/sizeof(batchmeshes[0]);
	batch.mesh = batchmeshes;
	batch.lightmap = -1;
	batch.texture = NULL;
	for (s = model->surfaces+model->firstmodelsurface,i = 0; i < model->nummodelsurfaces; i++, s++)
	{
		if (batch.meshes == batch.maxmeshes || batch.lightmap != s->lightmaptexturenum || batch.texture != s->texinfo->texture)
		{
			if (batch.texture)
				BE_SubmitBatch(&batch);
			batch.texture = s->texinfo->texture;
			batch.shader = R_TextureAnimation (batch.texture)->shader;
			batch.lightmap = s->lightmaptexturenum;
			batch.meshes = 0;
		}
		batch.mesh[batch.meshes++] = s->mesh;
	}
	if (batch.texture)
		BE_SubmitBatch(&batch);

	qglPopMatrix();
}

#ifdef RTLIGHTS
void BE_BaseEntShadowDepth(void)
{
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
	int		i;
	unsigned int bef;

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
		if (!R_ShouldDraw(currententity))
			continue;
		switch(currententity->model->type)
		{
		case mod_brush:
			if (r_drawentities.ival == 2)
				continue;
			bef = BEF_PUSHDEPTH;
			if (currententity->flags & Q2RF_ADDITIVE)
				bef |= BEF_FORCEADDITIVE;
			else if (currententity->drawflags & DRF_TRANSLUCENT && r_wateralpha.value != 1)
			{
				bef |= BEF_FORCETRANSPARENT;
				currententity->shaderRGBAf[3] = r_wateralpha.value;
			}
			else if (currententity->shaderRGBAf[3] < 1 && cls.protocol != CP_QUAKE3)
				bef |= BEF_FORCETRANSPARENT;
			if (currententity->flags & RF_NODEPTHTEST)
				bef |= BEF_FORCENODEPTH;
			BE_SelectMode(shaderstate.mode, bef);
			BaseBrushTextures(currententity);
			break;
		case mod_alias:
			if (r_drawentities.ival == 3)
				continue;
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
		BE_DrawMesh_Single(cl_stris[i].shader, &m, NULL, &cl_stris[i].shader->defaulttextures);
	}
}
static void BE_SubmitBatch(batch_t *batch)
{
	model_t *model = cl.worldmodel;
	int lm;

	lm = batch->lightmap;
	if (lm < 0)
	{
		shaderstate.curlightmap = r_nulltex;
		shaderstate.curdeluxmap = r_nulltex;
	}
	else
	{
		shaderstate.curlightmap = lightmap_textures[lm];
		shaderstate.curdeluxmap = deluxmap_textures[lm];
	}

	shaderstate.sourcevbo = &batch->texture->vbo;
	shaderstate.curshader = batch->shader;
	shaderstate.curentity = currententity;
	shaderstate.curtime = realtime;

	shaderstate.curtexnums = &shaderstate.curshader->defaulttextures;

	if (0)
	{
		int i;
		for (i = batch->firstmesh; i < batch->meshes; i++)
		{
			shaderstate.meshcount = 1;
			shaderstate.meshes = &batch->mesh[i];
			DrawMeshes();
		}
	}
	else
	{
		shaderstate.meshcount = batch->meshes - batch->firstmesh;
		shaderstate.meshes = batch->mesh+batch->firstmesh;
		DrawMeshes();
	}
}

void BE_SubmitMeshes (void)
{
	model_t *model = cl.worldmodel;
	batch_t *batch, *old;
	currententity = &r_worldentity;

	/*find all the portal shaders and try to draw them*/
	if (!r_noportals.ival && !r_refdef.recurse)
	{
		if (shaderstate.mode == BEM_STANDARD)
		{
			for (batch = model->batches; batch; batch = batch->next)
			{
				batch->shader = R_TextureAnimation (batch->texture)->shader;
				if (batch->shader->sort > SHADER_SORT_PORTAL)
					break;

				if (batch->meshes == batch->firstmesh)
					continue;

				BE_SelectMode(BEM_DEPTHONLY, 0);
				for (old = model->batches; old != batch; old = old->next)
				{
					if (old->meshes == old->firstmesh)
						continue;
					BE_SubmitBatch(old);
				}
				BE_SelectMode(BEM_STANDARD, 0);


				R_DrawPortal(batch);

				/*clear depth again*/
				GL_ForceDepthWritable();
				qglClear(GL_DEPTH_BUFFER_BIT);
				currententity = &r_worldentity;
			}
		}
	}

	for (batch = model->batches; batch; batch = batch->next)
	{
		if (batch->meshes == batch->firstmesh)
			continue;

		batch->shader = R_TextureAnimation (batch->texture)->shader;

		if (batch->shader->sort == SHADER_SORT_PORTAL)
		{
			if (shaderstate.mode != BEM_STANDARD)
				continue;
		}

		if (batch->shader->flags & SHADER_NODLIGHT)
			if (shaderstate.mode == BEM_LIGHT || shaderstate.mode == BEM_SMAPLIGHT)
				continue;

		if (batch->shader->flags & SHADER_SKY)
		{
			if (shaderstate.mode == BEM_STANDARD)
				R_DrawSkyChain (batch);
			continue;
		}

		BE_SubmitBatch(batch);
	}

	if (shaderstate.mode == BEM_STANDARD)
		BE_DrawPolys(true);

	checkerror();
	BE_BaseEntTextures();
	checkerror();
	currententity = &r_worldentity;
}

static void BE_UpdateLightmaps(void)
{
	int lm;
	for (lm = 0; lm < numlightmaps; lm++)
	{
		if (!lightmap[lm])
			continue;
		if (lightmap[lm]->modified)
		{
			extern cvar_t temp1;
			glRect_t *theRect;
			lightmap[lm]->modified = false;
			theRect = &lightmap[lm]->rectchange;
			GL_Bind(lightmap_textures[lm]);
			switch (lightmap_bytes)
			{
			case 4:
				qglTexSubImage2D(GL_TEXTURE_2D, 0, 0, theRect->t, 
					LMBLOCK_WIDTH, theRect->h, (lightmap_bgra?GL_BGRA_EXT:GL_RGBA), GL_UNSIGNED_INT_8_8_8_8_REV,
					lightmap[lm]->lightmaps+(theRect->t) *LMBLOCK_WIDTH*4);
				break;
			case 3:
				qglTexSubImage2D(GL_TEXTURE_2D, 0, 0, theRect->t, 
					LMBLOCK_WIDTH, theRect->h, (lightmap_bgra?GL_BGR_EXT:GL_RGB), GL_UNSIGNED_BYTE,
					lightmap[lm]->lightmaps+(theRect->t) *LMBLOCK_WIDTH*3);
				break;
			case 1:
				qglTexSubImage2D(GL_TEXTURE_2D, 0, 0, theRect->t, 
					LMBLOCK_WIDTH, theRect->h, GL_LUMINANCE, GL_UNSIGNED_BYTE,
					lightmap[lm]->lightmaps+(theRect->t) *LMBLOCK_WIDTH);
				break;
			}
			theRect->l = LMBLOCK_WIDTH;
			theRect->t = LMBLOCK_HEIGHT;
			theRect->h = 0;
			theRect->w = 0;
			checkerror();

			if (lightmap[lm]->deluxmodified)
			{
				lightmap[lm]->deluxmodified = false;
				theRect = &lightmap[lm]->deluxrectchange;
				GL_Bind(deluxmap_textures[lm]);
				qglTexSubImage2D(GL_TEXTURE_2D, 0, 0, theRect->t, 
					LMBLOCK_WIDTH, theRect->h, GL_RGB, GL_UNSIGNED_BYTE,
					lightmap[lm]->deluxmaps+(theRect->t) *LMBLOCK_WIDTH*3);
				theRect->l = LMBLOCK_WIDTH;
				theRect->t = LMBLOCK_HEIGHT;
				theRect->h = 0;
				theRect->w = 0;
				checkerror();
			}
		}
	}
}

void BE_DrawWorld (qbyte *vis)
{
	extern cvar_t r_shadow_realtime_world, r_shadow_realtime_world_lightmaps;
	RSpeedLocals();
	GL_DoSwap();

	BE_UpdateLightmaps();

	//make sure the world draws correctly
	r_worldentity.shaderRGBAf[0] = 1;
	r_worldentity.shaderRGBAf[1] = 1;
	r_worldentity.shaderRGBAf[2] = 1;
	r_worldentity.shaderRGBAf[3] = 1;
	r_worldentity.axis[0][0] = 1;
	r_worldentity.axis[1][1] = 1;
	r_worldentity.axis[2][2] = 1;

#ifdef RTLIGHTS
	if (r_shadow_realtime_world.value)
		shaderstate.identitylighting = r_shadow_realtime_world_lightmaps.value;
	else
#endif
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
