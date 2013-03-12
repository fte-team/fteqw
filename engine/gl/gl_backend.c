#include "quakedef.h"

//#define FORCESTATE

void DumpGLState(void);

#ifdef GLQUAKE

#define r_refract_fboival 1

#include "glquake.h"
#include "shader.h"
#ifdef _WIN32
#include <malloc.h>
#else
#include <alloca.h>
#endif

#ifdef FORCESTATE
#pragma warningmsg("FORCESTATE is active")
#endif

#ifdef ANDROID
/*android appears to have a bug, and requires f and not i*/
#define qglTexEnvi qglTexEnvf
#endif

extern cvar_t gl_overbright;
extern cvar_t gl_ati_truform;
extern cvar_t r_wireframe;

static const char LIGHTPASS_SHADER[] = "\
{\n\
	program rtlight%s\n\
\
	{\n\
		map $diffuse\n\
		blendfunc add\n\
	}\n\
	{\n\
		map $normalmap\n\
	}\n\
	{\n\
		map $specular\n\
	}\n\
	{\n\
		map $lightcubemap\n\
	}\n\
	{\n\
		map $shadowmap\n\
	}\n\
	{\n\
		map $loweroverlay\n\
	}\n\
	{\n\
		map $upperoverlay\n\
	}\n\
}";


enum
{
	LSHADER_STANDARD,
	LSHADER_CUBE,
	LSHADER_SMAP,
	LSHADER_SPOT,
	LSHADER_MODES
};

extern cvar_t r_glsl_offsetmapping, r_noportals;

static void BE_SendPassBlendDepthMask(unsigned int sbits);
void GLBE_RenderToTexture(texid_t sourcecol, texid_t sourcedepth, texid_t destcol, qboolean usedepth);
void GLBE_SubmitBatch(batch_t *batch);
static qboolean GLBE_RegisterLightShader(int mode);

struct {
	//internal state
	struct {
		int lastpasstmus;
//		int vbo_colour;
//		int vbo_texcoords[SHADER_PASS_MAX];
//		int vbo_deforms;	//holds verticies... in case you didn't realise.

		const shader_t *shader_light[1u<<LSHADER_MODES];
		qboolean inited_shader_light[1u<<LSHADER_MODES];

		const shader_t *crepskyshader;
		const shader_t *crepopaqueshader;

		GLhandleARB	allblackshader;

		qboolean initeddepthnorm;
		const shader_t *depthnormshader;
		texid_t tex_normals;
		texid_t tex_diffuse;
		int fbo_diffuse;
		int rb_depth;
		int rb_stencil;
		texid_t tex_sourcecol; /*this is used by $sourcecolour tgen*/
		texid_t tex_sourcedepth;
		int fbo_depthless;
		int fbo_reflection;
		texid_t tex_reflection;	/*basically a portal rendered to texture*/
		texid_t tex_refraction;	/*the (culled) underwater view*/
		texid_t tex_ripplemap;	/*temp image for waves and things.*/

		qboolean force2d;
		int currenttmu;
		int blendmode[SHADER_TMU_MAX];
		int texenvmode[SHADER_TMU_MAX];
		int currenttextures[SHADER_TMU_MAX];
		GLenum curtexturetype[SHADER_TMU_MAX];

		polyoffset_t curpolyoffset;
		unsigned int curcull;
		texid_t curshadowmap;

		unsigned int shaderbits;
		unsigned int sha_attr;
		int currentprogram;
		int lastuniform; /*program which was last set, so using the same prog for multiple surfaces on the same ent (ie: world) does not require lots of extra uniform chnges*/

		batch_t dummybatch;
		vbo_t dummyvbo;
		int colourarraytype;
		int currentvbo;
		int currentebo;
		int currentvao;

		mesh_t **meshes;
		unsigned int meshcount;
		float modelmatrix[16];
		float modelmatrixinv[16];
		float modelviewmatrix[16];

		vec4_t pendingcolourflat;
		int pendingcolourvbo;
		void *pendingcolourpointer;
		int curcolourvbo;
		void *curcolourpointer;

		int pendingvertexvbo;
		void *pendingvertexpointer;
		int curvertexvbo;
		void *curvertexpointer;

		int pendingtexcoordparts[SHADER_TMU_MAX];
		int pendingtexcoordvbo[SHADER_TMU_MAX];
		void *pendingtexcoordpointer[SHADER_TMU_MAX];

		float identitylighting;	//set to how bright lightmaps should be (reduced for overbright or realtime_world_lightmaps)

		texid_t temptexture; //$current
		texid_t fogtexture;
		texid_t normalisationcubemap;
		float fogfar;
		float depthrange;

		batch_t **mbatches;	//model batches (ie: not world)
	};

	//exterior state (paramters)
	struct {
		backendmode_t mode;
		unsigned int flags;
		int oldwidth, oldheight;

		vbo_t *sourcevbo;
		const shader_t *curshader;
		const entity_t *curentity;
		const batch_t *curbatch;
		const texnums_t *curtexnums;
		const mfog_t *fog;

		float curtime;
		float updatetime;

		int lightmode;
		vec3_t lightorg;
		vec3_t lightcolours;
		vec3_t lightcolourscale;
		float lightradius;
		texid_t lighttexture;
		texid_t lightcubemap;
		float lightprojmatrix[16]; /*world space*/
	};

	int wbatch;
	int maxwbatches;
	batch_t *wbatches;
} shaderstate;

struct {
	int numlights;
	int shadowsurfcount;
} bench;

static void BE_PolyOffset(qboolean pushdepth)
{
	polyoffset_t po;
	po.factor = shaderstate.curshader->polyoffset.factor;
	po.unit = shaderstate.curshader->polyoffset.unit;

	if (pushdepth)
	{
		/*some quake doors etc are flush with the walls that they're meant to be hidden behind, or plats the same height as the floor, etc
		we move them back very slightly using polygonoffset to avoid really ugly z-fighting*/
		extern cvar_t r_polygonoffset_submodel_offset, r_polygonoffset_submodel_factor;
		po.factor += r_polygonoffset_submodel_factor.value;
		po.unit += r_polygonoffset_submodel_offset.value;
	}
	if (shaderstate.mode == BEM_DEPTHONLY)
	{
		po.factor += 5;
		po.unit += 25;
	}

#ifndef FORCESTATE
	if (shaderstate.curpolyoffset.factor != po.factor || shaderstate.curpolyoffset.unit != po.unit)
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

static void BE_SetPassBlendMode(int tmu, int pbm)
{
#ifndef FORCESTATE
	if (shaderstate.blendmode[tmu] != pbm)
#endif
	{
		shaderstate.blendmode[tmu] = pbm;
#ifndef FORCESTATE
		if (shaderstate.currenttmu != tmu)
#endif
			GL_SelectTexture(tmu);

		switch (pbm)
		{
		case PBM_DOTPRODUCT:
			GL_TexEnv(GL_COMBINE_ARB);
			qglTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB_ARB, GL_TEXTURE);
			qglTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_RGB_ARB, GL_PREVIOUS_ARB);
			qglTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB_ARB, GL_DOT3_RGB_ARB);
			qglTexEnvf(GL_TEXTURE_ENV, GL_RGB_SCALE_ARB, 1);
			break;
		case PBM_MODULATE_PREV_COLOUR:
			GL_TexEnv(GL_COMBINE_ARB);
			qglTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB_ARB, GL_PRIMARY_COLOR_ARB);
			qglTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_RGB_ARB, GL_PREVIOUS_ARB);
			qglTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB_ARB, GL_MODULATE);
			qglTexEnvf(GL_TEXTURE_ENV, GL_RGB_SCALE_ARB, 1);
			break;
		case PBM_REPLACELIGHT:
			if (shaderstate.identitylighting != 1)
				goto forcemod;
			GL_TexEnv(GL_REPLACE);
			break;
		case PBM_REPLACE:
			GL_TexEnv(GL_REPLACE);
			break;
		case PBM_DECAL:
			if (tmu == 0)
				goto forcemod;
			GL_TexEnv(GL_DECAL);
			break;
		case PBM_ADD:
			if (tmu == 0)
				goto forcemod;
			GL_TexEnv(GL_ADD);
			break;
		case PBM_OVERBRIGHT:
			GL_TexEnv(GL_COMBINE_ARB);
			qglTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB_ARB, GL_TEXTURE);
			qglTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_RGB_ARB, GL_PREVIOUS_ARB);
			qglTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB_ARB, GL_MODULATE);
			qglTexEnvf(GL_TEXTURE_ENV, GL_RGB_SCALE_ARB, 1<<gl_overbright.ival);
			break;
		default:
		case PBM_MODULATE:
		forcemod:
			GL_TexEnv(GL_MODULATE);
			break;
		}
	}
}

/*OpenGL requires glDepthMask(GL_TRUE) or glClear(GL_DEPTH_BUFFER_BIT) will fail*/
void GL_ForceDepthWritable(void)
{
#ifndef FORCESTATE
	if (!(shaderstate.shaderbits & SBITS_MISC_DEPTHWRITE))
#endif
	{
		shaderstate.shaderbits |= SBITS_MISC_DEPTHWRITE;
		qglDepthMask(GL_TRUE);
	}
}

void GL_SetShaderState2D(qboolean is2d)
{
	shaderstate.updatetime = realtime;
	shaderstate.force2d = is2d;
	if (is2d)
		memcpy(shaderstate.modelviewmatrix, r_refdef.m_view, sizeof(shaderstate.modelviewmatrix));
	BE_SelectMode(BEM_STANDARD);


	if (cl.paused || cls.state < ca_active)
		shaderstate.updatetime = r_refdef.time;
	else
		shaderstate.updatetime = cl.servertime;
	BE_SelectEntity(&r_worldentity);
	shaderstate.curtime = shaderstate.updatetime - shaderstate.curentity->shaderTime;
}

void GL_SelectTexture(int target)
{
	shaderstate.currenttmu = target;
	if (qglActiveTextureARB)
		qglActiveTextureARB(target + mtexid0);
	else if (qglSelectTextureSGIS)
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
void GL_DeselectVAO(void)
{
	if (shaderstate.currentvao)
	{
		qglBindVertexArray(0);
		shaderstate.currentvao = 0;
	}
}
void GL_SelectEBO(int vbo)
{
	//EBO is part of the current VAO, so keep things matching that
#ifndef FORCESTATE
	if (shaderstate.currentebo != vbo)
#endif
	{
		shaderstate.currentebo = vbo;
		qglBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, shaderstate.currentebo);
	}
}

void GL_MTBind(int tmu, int target, texid_t texnum)
{
	GL_SelectTexture(tmu);

#ifndef FORCESTATE
	if (shaderstate.currenttextures[tmu] == texnum.num)
		return;
#endif

	shaderstate.currenttextures[tmu] = texnum.num;
	if (target)
		bindTexFunc (target, texnum.num);

#ifndef FORCESTATE
	if (shaderstate.curtexturetype[tmu] != target && !gl_config.nofixedfunc)
#endif
	{
		if (shaderstate.curtexturetype[tmu])
			qglDisable(shaderstate.curtexturetype[tmu]);
		shaderstate.curtexturetype[tmu] = target;
		if (target)
			qglEnable(target);
	}
}

void GL_LazyBind(int tmu, int target, texid_t texnum)
{
#ifndef FORCESTATE
	if (shaderstate.currenttextures[tmu] != texnum.num)
#endif
	{
		GL_SelectTexture(tmu);

		shaderstate.currenttextures[shaderstate.currenttmu] = texnum.num;

#ifndef FORCESTATE
		if (shaderstate.curtexturetype[tmu] != target)
#endif
		{
			if (shaderstate.curtexturetype[tmu])
				bindTexFunc (shaderstate.curtexturetype[tmu], texnum.num);
			if (gl_config.nofixedfunc)
			{
				shaderstate.curtexturetype[tmu] = target;
			}
			else
			{
				if (shaderstate.curtexturetype[tmu])
					qglDisable(shaderstate.curtexturetype[tmu]);
				shaderstate.curtexturetype[tmu] = target;
				if (target)
					qglEnable(target);
			}
		}

		if (target)
			bindTexFunc (target, texnum.num);
	}
}

static void BE_ApplyAttributes(unsigned int bitstochange, unsigned int bitstoendisable)
{
	unsigned int i;

	//legacy colour attribute (including flat shaded)
	if ((bitstochange) & (1u<<VATTR_LEG_COLOUR))
	{
		if (!shaderstate.pendingcolourpointer && !shaderstate.pendingcolourvbo)
		{
			if (shaderstate.curcolourpointer || shaderstate.curcolourvbo)
			{
				qglShadeModel(GL_FLAT);
				qglDisableClientState(GL_COLOR_ARRAY);
			}
			shaderstate.curcolourpointer = NULL;
			shaderstate.curcolourvbo = 0;
			qglColor4fv(shaderstate.pendingcolourflat);
		}
		else
		{
		#ifndef FORCESTATE
			if (shaderstate.curcolourpointer != shaderstate.pendingcolourpointer || shaderstate.pendingcolourvbo != shaderstate.curcolourvbo)
		#endif
			{
				if (!shaderstate.curcolourpointer && !shaderstate.curcolourvbo)
				{
					qglShadeModel(GL_SMOOTH);
					bitstoendisable |= (1u<<VATTR_LEG_COLOUR);
				}
				shaderstate.curcolourpointer = shaderstate.pendingcolourpointer;
				shaderstate.curcolourvbo = shaderstate.pendingcolourvbo;
				GL_SelectVBO(shaderstate.curcolourvbo);
				qglColorPointer(4, shaderstate.colourarraytype, 0, shaderstate.curcolourpointer);
			}

			if ((bitstoendisable) & (1u<<VATTR_LEG_COLOUR))
			{
				qglEnableClientState(GL_COLOR_ARRAY);
			}
		}
	}
	else
	{
		if ((bitstoendisable) & (1u<<VATTR_LEG_COLOUR))
		{
			qglDisableClientState(GL_COLOR_ARRAY);
		}
	}

	//legacy tmus
	if ((bitstoendisable|bitstochange) >= (1u<<VATTR_LEG_TMU0))
	{
		for (i = VATTR_LEG_TMU0; (bitstoendisable|bitstochange) >= (1u<<i); i++)
		{
			if ((bitstochange) & (1u<<i))
			{
				qglClientActiveTextureARB(i-VATTR_LEG_TMU0 + mtexid0);
				if (bitstoendisable & (1u<<i))
					qglEnableClientState(GL_TEXTURE_COORD_ARRAY);
				GL_SelectVBO(shaderstate.pendingtexcoordvbo[i-VATTR_LEG_TMU0]);
				qglTexCoordPointer(shaderstate.pendingtexcoordparts[i-VATTR_LEG_TMU0], GL_FLOAT, 0, shaderstate.pendingtexcoordpointer[i-VATTR_LEG_TMU0]);
			}

#ifndef FORCESTATE
			else if (bitstoendisable & (1u<<i))
#endif
			{
				qglClientActiveTextureARB(i-VATTR_LEG_TMU0 + mtexid0);
				if (bitstochange & (1u<<i))
					qglEnableClientState(GL_TEXTURE_COORD_ARRAY);
				else
					qglDisableClientState(GL_TEXTURE_COORD_ARRAY);
			}
		}
	}

	//legacy vertex coords
	if ((bitstochange) & (1u<<VATTR_LEG_VERTEX))
	{
	#ifndef FORCESTATE
		if (shaderstate.currentvao || shaderstate.curvertexpointer != shaderstate.pendingvertexpointer || shaderstate.pendingvertexvbo != shaderstate.curvertexvbo)
	#endif
		{
			shaderstate.curvertexpointer = shaderstate.pendingvertexpointer;
			shaderstate.curvertexvbo = shaderstate.pendingvertexvbo;
			GL_SelectVBO(shaderstate.curvertexvbo);
			qglVertexPointer(3, GL_FLOAT, sizeof(vecV_t), shaderstate.curvertexpointer);
		}
		if ((bitstoendisable) & (1u<<VATTR_LEG_VERTEX))
		{
			qglEnableClientState(GL_VERTEX_ARRAY);
		}
	}
	else
	{
		if ((bitstoendisable) & (1u<<VATTR_LEG_VERTEX))
		{
			qglDisableClientState(GL_VERTEX_ARRAY);
		}
	}

	if (!((bitstochange|bitstoendisable) & ~((1u<<VATTR_LEG_VERTEX) | (1u<<VATTR_LEG_COLOUR))))
		return;

	for (i = 1; i < VATTR_LEG_FIRST; i++)
	{
		if ((bitstochange) & (1u<<i))
		{
			switch (i)
			{
			case VATTR_VERTEX1:
				/*we still do vertex transforms for billboards and shadows and such*/
				GL_SelectVBO(shaderstate.pendingvertexvbo);
				qglVertexAttribPointer(i, 3, GL_FLOAT, GL_FALSE, sizeof(vecV_t), shaderstate.pendingvertexpointer);
				break;
			case VATTR_VERTEX2:
				if (!shaderstate.sourcevbo->coord2.gl.vbo && !shaderstate.sourcevbo->coord2.gl.addr)
				{
					GL_SelectVBO(shaderstate.pendingvertexvbo);
					qglVertexAttribPointer(i, 3, GL_FLOAT, GL_FALSE, sizeof(vecV_t), shaderstate.pendingvertexpointer);
				}
				else
				{
					GL_SelectVBO(shaderstate.sourcevbo->coord2.gl.vbo);
					qglVertexAttribPointer(VATTR_VERTEX2, 3, GL_FLOAT, GL_FALSE, sizeof(vecV_t), shaderstate.sourcevbo->coord2.gl.addr);
				}
				break;
			case VATTR_COLOUR:
				if (shaderstate.sourcevbo->colours.gl.addr)
				{
					GL_SelectVBO(shaderstate.sourcevbo->colours.gl.vbo);
					qglVertexAttribPointer(VATTR_COLOUR, 4, shaderstate.colourarraytype, ((shaderstate.colourarraytype==GL_FLOAT)?GL_FALSE:GL_TRUE), 0, shaderstate.sourcevbo->colours.gl.addr);
					break;
				}
				break;
			case VATTR_TEXCOORD:
				GL_SelectVBO(shaderstate.sourcevbo->texcoord.gl.vbo);
				qglVertexAttribPointer(VATTR_TEXCOORD, 2, GL_FLOAT, GL_FALSE, sizeof(vec2_t), shaderstate.sourcevbo->texcoord.gl.addr);
				break;
			case VATTR_LMCOORD:
				if (!shaderstate.sourcevbo->lmcoord[0].gl.vbo && !shaderstate.sourcevbo->lmcoord[0].gl.addr)
				{
					GL_SelectVBO(shaderstate.sourcevbo->texcoord.gl.vbo);
					qglVertexAttribPointer(VATTR_LMCOORD, 2, GL_FLOAT, GL_FALSE, sizeof(vec2_t), shaderstate.sourcevbo->texcoord.gl.addr);
				}
				else
				{
					GL_SelectVBO(shaderstate.sourcevbo->lmcoord[0].gl.vbo);
					qglVertexAttribPointer(VATTR_LMCOORD, 2, GL_FLOAT, GL_FALSE, sizeof(vec2_t), shaderstate.sourcevbo->lmcoord[0].gl.addr);
				}
				break;
			case VATTR_LMCOORD2:
				GL_SelectVBO(shaderstate.sourcevbo->lmcoord[1].gl.vbo);
				qglVertexAttribPointer(VATTR_LMCOORD2, 2, GL_FLOAT, GL_FALSE, sizeof(vec2_t), shaderstate.sourcevbo->lmcoord[1].gl.addr);
				break;
			case VATTR_LMCOORD3:
				GL_SelectVBO(shaderstate.sourcevbo->lmcoord[2].gl.vbo);
				qglVertexAttribPointer(VATTR_LMCOORD3, 2, GL_FLOAT, GL_FALSE, sizeof(vec2_t), shaderstate.sourcevbo->lmcoord[2].gl.addr);
				break;
			case VATTR_LMCOORD4:
				GL_SelectVBO(shaderstate.sourcevbo->lmcoord[3].gl.vbo);
				qglVertexAttribPointer(VATTR_LMCOORD4, 2, GL_FLOAT, GL_FALSE, sizeof(vec2_t), shaderstate.sourcevbo->lmcoord[3].gl.addr);
				break;
			case VATTR_NORMALS:
				if (!shaderstate.sourcevbo->normals.gl.addr)
				{
					shaderstate.sha_attr &= ~(1u<<i);
					qglDisableVertexAttribArray(i);
					continue;
				}
				GL_SelectVBO(shaderstate.sourcevbo->normals.gl.vbo);
				qglVertexAttribPointer(VATTR_NORMALS, 3, GL_FLOAT, GL_FALSE, sizeof(vec3_t), shaderstate.sourcevbo->normals.gl.addr);
				break;
			case VATTR_SNORMALS:
				if (!shaderstate.sourcevbo->svector.gl.addr)
				{
					shaderstate.sha_attr &= ~(1u<<i);
					qglDisableVertexAttribArray(i);
					continue;
				}
				GL_SelectVBO(shaderstate.sourcevbo->svector.gl.vbo);
				qglVertexAttribPointer(VATTR_SNORMALS, 3, GL_FLOAT, GL_FALSE, sizeof(vec3_t), shaderstate.sourcevbo->svector.gl.addr);
				break;
			case VATTR_TNORMALS:
				if (!shaderstate.sourcevbo->tvector.gl.addr)
				{
					shaderstate.sha_attr &= ~(1u<<i);
					qglDisableVertexAttribArray(i);
					continue;
				}
				GL_SelectVBO(shaderstate.sourcevbo->tvector.gl.vbo);
				qglVertexAttribPointer(VATTR_TNORMALS, 3, GL_FLOAT, GL_FALSE, sizeof(vec3_t), shaderstate.sourcevbo->tvector.gl.addr);
				break;
			case VATTR_BONENUMS:
				GL_SelectVBO(shaderstate.sourcevbo->bonenums.gl.vbo);
				qglVertexAttribPointer(VATTR_BONENUMS, 4, GL_UNSIGNED_BYTE, GL_FALSE, sizeof(byte_vec4_t), shaderstate.sourcevbo->bonenums.gl.addr);
				break;
			case VATTR_BONEWEIGHTS:
				GL_SelectVBO(shaderstate.sourcevbo->boneweights.gl.vbo);
				qglVertexAttribPointer(VATTR_BONEWEIGHTS, 4, GL_FLOAT, GL_FALSE, sizeof(vec4_t), shaderstate.sourcevbo->boneweights.gl.addr);
				break;
			}
			if ((bitstoendisable) & (1u<<i))
			{
				qglEnableVertexAttribArray(i);
			}
		}
		else
		{
			if ((bitstoendisable) & (1u<<i))
			{
				qglDisableVertexAttribArray(i);
			}
		}
	}
}

static void BE_EnableShaderAttributes(unsigned int progattrmask, int usevao)
{
	unsigned int bitstochange, bitstoendisable;

	if (shaderstate.currentvao != usevao)
	{
		shaderstate.currentvao = usevao;
		qglBindVertexArray(usevao); 
	}

	if (shaderstate.currentvao)
	{
		bitstochange = shaderstate.sourcevbo->vaodynamic&progattrmask;
		bitstoendisable = 0;

		if (bitstochange & (1u<<VATTR_LEG_ELEMENTS))
		{
			qglBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, shaderstate.sourcevbo->indicies.gl.vbo);
		}
	}
	else
	{
		bitstochange = progattrmask;
		bitstoendisable = progattrmask^shaderstate.sha_attr;

		shaderstate.sha_attr = progattrmask;

#ifndef FORCESTATE
		if (shaderstate.currentebo != shaderstate.sourcevbo->indicies.gl.vbo)
#endif
		{
			shaderstate.currentebo = shaderstate.sourcevbo->indicies.gl.vbo;
			qglBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, shaderstate.currentebo);
		}
	}

	if (bitstochange || bitstoendisable)
		BE_ApplyAttributes(bitstochange, bitstoendisable);
}

void GLBE_SetupVAO(vbo_t *vbo, unsigned vaodynamic)
{
	if (qglGenVertexArrays)
	{
		unsigned int availbits;
		qglGenVertexArrays(1, &vbo->vao);

		availbits =
			(1u<<VATTR_LEG_ELEMENTS)|
			(1u<<(gl_config.nofixedfunc?VATTR_VERTEX1:VATTR_LEG_VERTEX))|
			(1u<<VATTR_TEXCOORD)|
			(1u<<VATTR_LMCOORD)|
			(1u<<VATTR_LMCOORD2)|
			(1u<<VATTR_LMCOORD3)|
			(1u<<VATTR_LMCOORD4)|
			(1u<<VATTR_COLOUR)|
			(1u<<VATTR_NORMALS)|
			(1u<<VATTR_SNORMALS)|
			(1u<<VATTR_TNORMALS)|
			0;

		shaderstate.curvertexpointer = NULL;
		shaderstate.curvertexvbo = 0;

		shaderstate.sourcevbo = vbo;
		shaderstate.pendingvertexvbo = shaderstate.sourcevbo->coord.gl.vbo;
		shaderstate.pendingvertexpointer = shaderstate.sourcevbo->coord.gl.addr;
		shaderstate.colourarraytype = GL_FLOAT;

		shaderstate.currentvao = vbo->vao;
		qglBindVertexArray(vbo->vao);
		qglBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, shaderstate.sourcevbo->indicies.gl.vbo);
		BE_ApplyAttributes(availbits, availbits);
		GL_SelectVBO(shaderstate.sourcevbo->coord.gl.vbo);
		vbo->vaodynamic = vaodynamic;

		shaderstate.curvertexpointer = NULL;
		shaderstate.curvertexvbo = 0;
	}
	else
	{
		GL_DeselectVAO();

		/*always select the coord vbo and indicies ebo, for easy bufferdata*/
		GL_SelectEBO(vbo->indicies.gl.vbo);
		GL_SelectVBO(vbo->coord.gl.vbo);
	}
}

void GL_SelectProgram(int program)
{
	if (shaderstate.currentprogram != program)
	{
		qglUseProgramObjectARB(program);
		shaderstate.currentprogram = program;
	}
}
static void GL_DeSelectProgram(void)
{
	if (shaderstate.currentprogram != 0)
	{
		qglUseProgramObjectARB(0);
		shaderstate.currentprogram = 0;
	}
}


void GLBE_RenderShadowBuffer(unsigned int numverts, int vbo, vecV_t *verts, unsigned numindicies, int ibo, index_t *indicies)
{
	shaderstate.pendingvertexvbo = vbo;
	shaderstate.pendingvertexpointer = verts;

	shaderstate.sourcevbo = &shaderstate.dummyvbo;
	shaderstate.dummyvbo.indicies.gl.vbo = ibo;

	if (shaderstate.allblackshader)
	{
		GL_SelectProgram(shaderstate.allblackshader);

		BE_EnableShaderAttributes(gl_config.nofixedfunc?(1u<<VATTR_VERTEX1):(1u<<VATTR_LEG_VERTEX), 0);

		if (shaderstate.allblackshader != shaderstate.lastuniform)
		{
			float m16[16];
			Matrix4_Multiply(r_refdef.m_projection, shaderstate.modelviewmatrix, m16);
			qglUniformMatrix4fvARB(qglGetUniformLocationARB(shaderstate.allblackshader, "m_modelviewprojection"), 1, false, m16);
		}
		shaderstate.lastuniform = shaderstate.allblackshader;


		qglDrawRangeElements(GL_TRIANGLES, 0, numverts, numindicies, GL_INDEX_TYPE, indicies);
	}
	else
	{
		GL_DeSelectProgram();
		BE_EnableShaderAttributes((1u<<VATTR_LEG_VERTEX), 0);

		//draw cached world shadow mesh
		qglDrawRangeElements(GL_TRIANGLES, 0, numverts, numindicies, GL_INDEX_TYPE, indicies);
	}
	RQuantAdd(RQUANT_SHADOWFACES, numindicies);
	shaderstate.dummyvbo.indicies.gl.vbo = 0;
	shaderstate.sourcevbo = NULL;
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
	unsigned int cv = shaderstate.curentity->topcolour;

	if (cv >= 16)
	{
		*retred = (((cv&0xff0000)>>16)**((unsigned char*)&d_8to24rgbtable[15]+0))>>8;
		*retgreen = (((cv&0x00ff00)>>8)**((unsigned char*)&d_8to24rgbtable[15]+1))>>8;
		*retblue = (((cv&0x0000ff)>>0)**((unsigned char*)&d_8to24rgbtable[15]+2))>>8;
		return;
	}
	if (cv >= 0)
		i = cv;
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
	unsigned int cv = shaderstate.curentity->bottomcolour;

	if (cv >= 16)
	{
		*retred = (((cv&0xff0000)>>16)**((unsigned char*)&d_8to24rgbtable[15]+0))>>8;
		*retgreen = (((cv&0x00ff00)>>8)**((unsigned char*)&d_8to24rgbtable[15]+1))>>8;
		*retblue = (((cv&0x0000ff)>>0)**((unsigned char*)&d_8to24rgbtable[15]+2))>>8;
		return;
	}
	if (cv >= 0)
		i = cv;
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
	if (shaderstate.currentvao)
		qglBindVertexArray(0);
	shaderstate.currentvao = 0;
	shaderstate.curvertexvbo = ~0;
	GL_SelectVBO(0);
//	GL_SelectEBO(0);

	while(shaderstate.lastpasstmus>0)
	{
		GL_LazyBind(--shaderstate.lastpasstmus, 0, r_nulltex);
	}
	GL_SelectTexture(0);


	if (!gl_config.nofixedfunc)
	{
		BE_SetPassBlendMode(0, PBM_REPLACE);
		qglColor3f(1,1,1);

		GL_DeSelectProgram();
	}

	shaderstate.shaderbits &= ~(SBITS_MISC_DEPTHEQUALONLY|SBITS_MISC_DEPTHCLOSERONLY|SBITS_MASK_BITS);
	shaderstate.shaderbits |= SBITS_MISC_DEPTHWRITE;

	shaderstate.shaderbits &= ~(SBITS_BLEND_BITS);
	qglDisable(GL_BLEND);

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

#ifdef RTLIGHTS
//called from gl_shadow
void GLBE_SetupForShadowMap(texid_t shadowmaptex)
{
	shaderstate.curshadowmap = shadowmaptex;
	while(shaderstate.lastpasstmus>0)
	{
		GL_LazyBind(--shaderstate.lastpasstmus, 0, r_nulltex);
	}

	shaderstate.shaderbits &= ~SBITS_MISC_DEPTHWRITE;

	qglShadeModel(GL_FLAT);
	BE_SetPassBlendMode(0, PBM_REPLACE);
	GL_ForceDepthWritable();
//	qglColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);

	BE_SelectMode(BEM_DEPTHONLY);
}
#endif

static void T_Gen_CurrentRender(int tmu)
{
	int vwidth, vheight;
	if (r_refdef.recurse)
		return;

	if (r_config.texture_non_power_of_two)
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
		TEXASSIGN(shaderstate.temptexture, GL_AllocNewTexture("***$currentrender***", vwidth, vheight, 0));
	GL_MTBind(tmu, GL_TEXTURE_2D, shaderstate.temptexture);
	qglCopyTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 0, 0, vwidth, vheight, 0);
	qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
}

static void Shader_BindTextureForPass(int tmu, const shaderpass_t *pass)
{
	extern texid_t missing_texture;
	extern texid_t missing_texture_gloss;
	extern texid_t scenepp_postproc_cube;
	extern texid_t r_whiteimage;

	texid_t t;
	switch(pass->texgen)
	{
	default:
	case T_GEN_SINGLEMAP:
		t = pass->anim_frames[0];
		break;
	case T_GEN_ANIMMAP:
		t = pass->anim_frames[(int)(pass->anim_fps * shaderstate.curtime) % pass->anim_numframes];
		break;
	case T_GEN_LIGHTMAP:
		if (shaderstate.curbatch->lightmap[0] < 0)
			t = r_whiteimage;
		else
			t = lightmap[shaderstate.curbatch->lightmap[0]]->lightmap_texture;
		break;
	case T_GEN_DELUXMAP:
		{
			int lmi = shaderstate.curbatch->lightmap[0];
			if (lmi < 0 || !lightmap[lmi]->hasdeluxe)
				t = r_nulltex;	//fixme
			else
				t = lightmap[lmi+1]->lightmap_texture;
		}
		break;
	case T_GEN_DIFFUSE:
		if (shaderstate.curtexnums && TEXVALID(shaderstate.curtexnums->base))
			t = shaderstate.curtexnums->base;
		else
			t = missing_texture;
		break;
	case T_GEN_NORMALMAP:
		t = shaderstate.curtexnums?shaderstate.curtexnums->bump:r_nulltex; /*FIXME: nulltex is not correct*/
		break;
	case T_GEN_SPECULAR:
		if (TEXVALID(shaderstate.curtexnums->specular))
			t = shaderstate.curtexnums->specular;
		else
			t = missing_texture_gloss;
		break;
	case T_GEN_UPPEROVERLAY:
		t = shaderstate.curtexnums->upperoverlay;
		break;
	case T_GEN_LOWEROVERLAY:
		t = shaderstate.curtexnums->loweroverlay;
		break;
	case T_GEN_FULLBRIGHT:
		t = shaderstate.curtexnums->fullbright;
		break;
	case T_GEN_SHADOWMAP:
		t = shaderstate.curshadowmap;
		break;

	case T_GEN_LIGHTCUBEMAP:
		GL_LazyBind(tmu, GL_TEXTURE_CUBE_MAP_ARB, shaderstate.lightcubemap);
		return;
	case T_GEN_CUBEMAP:
		t = pass->anim_frames[0];
		GL_LazyBind(tmu, GL_TEXTURE_CUBE_MAP_ARB, t);
		return;
	case T_GEN_SOURCECUBE:
		t = scenepp_postproc_cube;
		GL_LazyBind(tmu, GL_TEXTURE_CUBE_MAP_ARB, t);
		return;

	case T_GEN_3DMAP:
		t = pass->anim_frames[0];
		GL_LazyBind(tmu, GL_TEXTURE_3D, t);
		return;

	case T_GEN_VIDEOMAP:
#ifdef NOMEDIA
		t = shaderstate.curtexnums?shaderstate.curtexnums->base:r_nulltex;
#else
		t = Media_UpdateForShader(pass->cin);
#endif
		break;

	case T_GEN_CURRENTRENDER:
		T_Gen_CurrentRender(tmu);
		return;

	case T_GEN_SOURCECOLOUR:
		t = shaderstate.tex_sourcecol;
		break;
	case T_GEN_SOURCEDEPTH:
		t = shaderstate.tex_sourcedepth;
		break;
	case T_GEN_REFLECTION:
		t = shaderstate.tex_reflection;
		break;
	case T_GEN_REFRACTION:
		if (!r_refract_fboival)
		{
			T_Gen_CurrentRender(tmu);
			return;
		}
		t = shaderstate.tex_refraction;
		break;
	case T_GEN_RIPPLEMAP:
		t = shaderstate.tex_ripplemap;
		break;
	}
	GL_LazyBind(tmu, GL_TEXTURE_2D, t);
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

void Shader_LightPass(char *shortname, shader_t *s, const void *args)
{
	char shadertext[8192*2];
	sprintf(shadertext, LIGHTPASS_SHADER, "");
	Shader_DefaultScript(shortname, s, shadertext);
}

void GenerateFogTexture(texid_t *tex, float density, float zscale)
{
#define FOGS 256
#define FOGT 32
	byte_vec4_t fogdata[FOGS*FOGT];
	int s, t;
	float f, z;
	static float fogdensity, fogzscale;
	if (TEXVALID(*tex) && density == fogdensity && zscale == fogzscale)
		return;
	fogdensity = density;
	fogzscale = zscale;

	for(s = 0; s < FOGS; s++)
		for(t = 0; t < FOGT; t++)
		{
			z = (float)s / (FOGS-1);
			z *= zscale;

			if (0)//q3
				f = pow(z, 0.5);
			else if (1)//GL_EXP
				f = 1-exp(-density * z);
			else //GL_EXP2
				f = 1-exp(-(density*density) * z);
			if (f < 0)
				f = 0;
			if (f > 1)
				f = 1;

			fogdata[t*FOGS + s][0] = 255;
			fogdata[t*FOGS + s][1] = 255;
			fogdata[t*FOGS + s][2] = 255;
			fogdata[t*FOGS + s][3] = 255*f;
		}

	if (!TEXVALID(*tex))
		*tex = R_AllocNewTexture("***fog***", FOGS, FOGT, 0);
	R_Upload(*tex, "fog", TF_RGBA32, fogdata, NULL, FOGS, FOGT, IF_CLAMP|IF_NOMIPMAP);
}

void GLBE_Shutdown(void)
{
	BZ_Free(shaderstate.wbatches);
	shaderstate.wbatches = NULL;
	shaderstate.maxwbatches = 0;
}

void GLBE_Init(void)
{
	int i;
	double t;

	GLBE_Shutdown();

	memset(&shaderstate, 0, sizeof(shaderstate));

	shaderstate.curentity = &r_worldentity;
	be_maxpasses = gl_mtexarbable;
	gl_stencilbits = 0;
	qglGetIntegerv(GL_STENCIL_BITS, &gl_stencilbits);
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
	for (i = 0; i < MAXLIGHTMAPS; i++)
		shaderstate.dummybatch.lightmap[i] = -1;

#ifdef RTLIGHTS
	if (r_shadow_realtime_dlight.ival || r_shadow_realtime_world.ival)
	{
		if (r_shadow_shadowmapping.ival)
			GLBE_RegisterLightShader(LSHADER_SMAP);
		else
			GLBE_RegisterLightShader(0);
	}
#endif

	gl_overbright.modified = true; /*in case the d3d renderer does the same*/
	/*lock the cvar down if the backend can't actually do it*/
	if (!gl_config.tex_env_combine && !gl_config.nofixedfunc && gl_overbright.ival)
		Cvar_ApplyLatchFlag(&gl_overbright, "0", CVAR_RENDERERLATCH);
	shaderstate.shaderbits = ~SBITS_ATEST_BITS;
	BE_SendPassBlendDepthMask(0);
	currententity = &r_worldentity;


	shaderstate.fogtexture = r_nulltex;


	//make sure the world draws correctly
	r_worldentity.shaderRGBAf[0] = 1;
	r_worldentity.shaderRGBAf[1] = 1;
	r_worldentity.shaderRGBAf[2] = 1;
	r_worldentity.shaderRGBAf[3] = 1;
	r_worldentity.axis[0][0] = 1;
	r_worldentity.axis[1][1] = 1;
	r_worldentity.axis[2][2] = 1;

	R_InitFlashblends();
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

static void tcgen_fog(float *st, unsigned int numverts, float *xyz, mfog_t *fog)
{
	int			i;

	float z;
	vec4_t zmat;

	//generate a simple matrix to calc only the projected z coord
	zmat[0] = -shaderstate.modelviewmatrix[2];
	zmat[1] = -shaderstate.modelviewmatrix[6];
	zmat[2] = -shaderstate.modelviewmatrix[10];
	zmat[3] = -shaderstate.modelviewmatrix[14];

	Vector4Scale(zmat, shaderstate.fogfar, zmat);

	for (i = 0 ; i < numverts ; i++, xyz += sizeof(vecV_t)/sizeof(vec_t), st += 2 )
	{
		z = DotProduct(xyz, zmat) + zmat[3];
		st[0] = z;
		st[1] = realtime - (int)realtime;
	}
}

static float *tcgen(unsigned int tcgen, int cnt, float *dst, const mesh_t *mesh)
{
	int i;
	vecV_t *src;
	switch (tcgen)
	{
	default:
	case TC_GEN_BASE:
		return (float*)mesh->st_array;
	case TC_GEN_LIGHTMAP:
		if (!mesh->lmst_array)
			return (float*)mesh->st_array;
		else
			return (float*)mesh->lmst_array;
	case TC_GEN_NORMAL:
		return (float*)mesh->normals_array;
	case TC_GEN_SVECTOR:
		return (float*)mesh->snormals_array;
	case TC_GEN_TVECTOR:
		return (float*)mesh->tnormals_array;
	case TC_GEN_ENVIRONMENT:
		if (!mesh->normals_array)
			return (float*)mesh->st_array;
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

static void GenerateTCFog(int passnum, mfog_t *fog)
{
	int m;
	mesh_t *mesh;
	for (m = 0; m < shaderstate.meshcount; m++)
	{
		mesh = shaderstate.meshes[m];
		tcgen_fog(texcoordarray[passnum]+mesh->vbofirstvert*2, mesh->numvertexes, (float*)mesh->xyz_array, fog);
	}

	shaderstate.pendingtexcoordparts[passnum] = 2;
	shaderstate.pendingtexcoordvbo[passnum] = 0;
	shaderstate.pendingtexcoordpointer[passnum] = texcoordarray[passnum];
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

		src = tcgen(pass->tcgen, mesh->numvertexes, texcoordarray[passnum]+mesh->vbofirstvert*2, mesh);
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
	shaderstate.pendingtexcoordparts[passnum] = 2;
	shaderstate.pendingtexcoordvbo[passnum] = 0;
	shaderstate.pendingtexcoordpointer[passnum] = texcoordarray[passnum];
#else
	GL_DeselectVAO();
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

	shaderstate.pendingtexcoordparts[passnum] = 2;
	shaderstate.pendingtexcoordvbo[passnum] = shaderstate.vbo_texcoords[passnum];
	shaderstate.pendingtexcoordpointer[passnum] = NULL;
#endif
}

//end texture coords
/*========================================== colour generation =====================================*/

//source is always packed
//dest is packed too
static void colourgen(const shaderpass_t *pass, int cnt, vec4_t *src, vec4_t *dst, const mesh_t *mesh)
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
	case RGB_GEN_VERTEX_LIGHTING:
		if (shaderstate.identitylighting != 1)
		{
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
				dst[cnt][0] = src[cnt][0]*shaderstate.identitylighting;
				dst[cnt][1] = src[cnt][1]*shaderstate.identitylighting;
				dst[cnt][2] = src[cnt][2]*shaderstate.identitylighting;
			}
			break;
		}
	case RGB_GEN_VERTEX_EXACT:
		if (!src)
		{
			while((cnt)--)
			{
				dst[cnt][0] = 1;
				dst[cnt][1] = 1;
				dst[cnt][2] = 1;
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
		if (shaderstate.curbatch->lightstyle[0] != 255)
		{
			while((cnt)--)
			{
				dst[cnt][0] = shaderstate.identitylighting * d_lightstylevalue[shaderstate.curbatch->lightstyle[0]]/256.0f;
				dst[cnt][1] = shaderstate.identitylighting * d_lightstylevalue[shaderstate.curbatch->lightstyle[0]]/256.0f;
				dst[cnt][2] = shaderstate.identitylighting * d_lightstylevalue[shaderstate.curbatch->lightstyle[0]]/256.0f;
			}
		}
		else
		{
			//compensate for overbrights
			while((cnt)--)
			{
				dst[cnt][0] = shaderstate.identitylighting;
				dst[cnt][1] = shaderstate.identitylighting;
				dst[cnt][2] = shaderstate.identitylighting;
			}
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
			R_LightArrays(shaderstate.curentity, mesh->xyz_array, dst, cnt, mesh->normals_array, shaderstate.identitylighting);
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

static void deformgen(const deformv_t *deformv, int cnt, vecV_t *src, vecV_t *dst, const mesh_t *mesh)
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

static void GenerateVertexBlends(const shader_t *shader)
{
	int i, m;
	mesh_t *meshlist;
	vecV_t *ov, *iv1, *iv2;
	float w1, w2;
	for (m = 0; m < shaderstate.meshcount; m++)
	{
		meshlist = shaderstate.meshes[m];

		ov = vertexarray+meshlist->vbofirstvert;
		iv1 = meshlist->xyz_array;
		iv2 = meshlist->xyz2_array;
		w1 = meshlist->xyz_blendw[0];
		w2 = meshlist->xyz_blendw[1];
		for (i = 0; i < meshlist->numvertexes; i++)
		{
			ov[i][0] = iv1[i][0]*w1 + iv2[i][0]*w2;
			ov[i][1] = iv1[i][1]*w1 + iv2[i][1]*w2;
			ov[i][2] = iv1[i][2]*w1 + iv2[i][2]*w2;
		}
		for (i = 0; i < shader->numdeforms; i++)
		{
			deformgen(&shader->deforms[i], meshlist->numvertexes, vertexarray+meshlist->vbofirstvert, vertexarray+meshlist->vbofirstvert, meshlist);
		}
	}

	shaderstate.pendingvertexpointer = vertexarray;
	shaderstate.pendingvertexvbo = 0;
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

static void alphagen(const shaderpass_t *pass, int cnt, avec4_t *const src, avec4_t *dst, const mesh_t *mesh)
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

	if (pass->flags & SHADER_PASS_NOCOLORARRAY && qglColor4fv)
	{
		colourgen(pass, 1, meshlist->colors4f_array, &shaderstate.pendingcolourflat, meshlist);
		alphagen(pass, 1, meshlist->colors4f_array, &shaderstate.pendingcolourflat, meshlist);
		shaderstate.pendingcolourvbo = 0;
		shaderstate.pendingcolourpointer = NULL;
	}
	else
	{
		extern cvar_t r_nolightdir;
		if (pass->rgbgen == RGB_GEN_LIGHTING_DIFFUSE)
		{
			if (shaderstate.mode == BEM_DEPTHDARK || shaderstate.mode == BEM_DEPTHONLY)
			{
				shaderstate.pendingcolourflat[0] = shaderstate.pendingcolourflat[1] = shaderstate.pendingcolourflat[2] = 0;
				alphagen(pass, 1, meshlist->colors4f_array, &shaderstate.pendingcolourflat, meshlist);
				shaderstate.pendingcolourvbo = 0;
				shaderstate.pendingcolourpointer = NULL;
				return;
			}
			if (shaderstate.mode == BEM_LIGHT)
			{
				shaderstate.pendingcolourflat[0] = shaderstate.pendingcolourflat[1] = shaderstate.pendingcolourflat[2] = 1;
				alphagen(pass, 1, meshlist->colors4f_array, &shaderstate.pendingcolourflat, meshlist);
				shaderstate.pendingcolourvbo = 0;
				shaderstate.pendingcolourpointer = NULL;
				return;
			}
			if (r_nolightdir.ival)
			{
				VectorCopy(shaderstate.curentity->light_avg, shaderstate.pendingcolourflat);
				shaderstate.pendingcolourflat[3] = shaderstate.curentity->shaderRGBAf[3];
				shaderstate.pendingcolourvbo = 0;
				shaderstate.pendingcolourpointer = NULL;
				return;
			}
		}

		//if its vetex lighting, just use the vbo
		if (((pass->rgbgen == RGB_GEN_VERTEX_LIGHTING && shaderstate.identitylighting == 1) || pass->rgbgen == RGB_GEN_VERTEX_EXACT) && pass->alphagen == ALPHA_GEN_VERTEX)
		{
			shaderstate.pendingcolourvbo = shaderstate.sourcevbo->colours.gl.vbo;
			shaderstate.pendingcolourpointer = shaderstate.sourcevbo->colours.gl.addr;
			return;
		}

		for (m = 0; m < shaderstate.meshcount; m++)
		{
			meshlist = shaderstate.meshes[m];

			colourgen(pass, meshlist->numvertexes, meshlist->colors4f_array, coloursarray + meshlist->vbofirstvert, meshlist);
			alphagen(pass, meshlist->numvertexes, meshlist->colors4f_array, coloursarray + meshlist->vbofirstvert, meshlist);
		}

		shaderstate.colourarraytype = GL_FLOAT;
		shaderstate.pendingcolourvbo = 0;
		shaderstate.pendingcolourpointer = coloursarray;
	}
}

static void BE_GeneratePassTC(const shaderpass_t *pass, int tmu)
{
	if (!pass->numtcmods)
	{
		//if there are no tcmods, pass through here as fast as possible
		if (pass->tcgen == TC_GEN_BASE)
		{
			shaderstate.pendingtexcoordparts[tmu] = 2;
			shaderstate.pendingtexcoordvbo[tmu] = shaderstate.sourcevbo->texcoord.gl.vbo;
			shaderstate.pendingtexcoordpointer[tmu] = shaderstate.sourcevbo->texcoord.gl.addr;
		}
		else if (pass->tcgen == TC_GEN_LIGHTMAP)
		{
			if (!shaderstate.sourcevbo->lmcoord[0].gl.addr)
			{
				shaderstate.pendingtexcoordparts[tmu] = 2;
				shaderstate.pendingtexcoordvbo[tmu] = shaderstate.sourcevbo->texcoord.gl.vbo;
				shaderstate.pendingtexcoordpointer[tmu] = shaderstate.sourcevbo->texcoord.gl.addr;
			}
			else
			{
				shaderstate.pendingtexcoordparts[tmu] = 2;
				shaderstate.pendingtexcoordvbo[tmu] = shaderstate.sourcevbo->lmcoord[0].gl.vbo;
				shaderstate.pendingtexcoordpointer[tmu] = shaderstate.sourcevbo->lmcoord[0].gl.addr;
			}
		}
		else if (pass->tcgen == TC_GEN_NORMAL)
		{
			shaderstate.pendingtexcoordparts[tmu] = 3;
			shaderstate.pendingtexcoordvbo[tmu] = shaderstate.sourcevbo->normals.gl.vbo;
			shaderstate.pendingtexcoordpointer[tmu] = shaderstate.sourcevbo->normals.gl.addr;
		}
		else if (pass->tcgen == TC_GEN_SVECTOR)
		{
			shaderstate.pendingtexcoordparts[tmu] = 3;
			shaderstate.pendingtexcoordvbo[tmu] = shaderstate.sourcevbo->svector.gl.vbo;
			shaderstate.pendingtexcoordpointer[tmu] = shaderstate.sourcevbo->svector.gl.addr;
		}
		else if (pass->tcgen == TC_GEN_TVECTOR)
		{
			shaderstate.pendingtexcoordparts[tmu] = 3;
			shaderstate.pendingtexcoordvbo[tmu] = shaderstate.sourcevbo->tvector.gl.vbo;
			shaderstate.pendingtexcoordpointer[tmu] = shaderstate.sourcevbo->tvector.gl.addr;
		}
		else
		{
			//specular highlights and reflections have no fixed data, and must be generated.
			GenerateTCMods(pass, tmu);
		}
	}
	else
	{
		GenerateTCMods(pass, tmu);
	}
}

static void BE_SendPassBlendDepthMask(unsigned int sbits)
{
	unsigned int delta;

	/*2d mode doesn't depth test or depth write*/
	if (shaderstate.force2d)
	{
#ifdef warningmsg
#pragma warningmsg("fixme: q3 doesn't seem to have this, why do we need it?")
#endif
		sbits &= ~(SBITS_MISC_DEPTHWRITE|SBITS_MISC_DEPTHEQUALONLY);
		sbits |= SBITS_MISC_NODEPTHTEST;
	}
	if (shaderstate.flags & (BEF_FORCEADDITIVE|BEF_FORCETRANSPARENT|BEF_FORCENODEPTH|BEF_FORCEDEPTHTEST|BEF_FORCEDEPTHWRITE))
	{
		if (shaderstate.flags & BEF_FORCEADDITIVE)
			sbits = (sbits & ~(SBITS_MISC_DEPTHWRITE|SBITS_BLEND_BITS|SBITS_ATEST_BITS))
						| (SBITS_SRCBLEND_SRC_ALPHA | SBITS_DSTBLEND_ONE);
		else if (shaderstate.flags & BEF_FORCETRANSPARENT)
		{
			if ((sbits & SBITS_BLEND_BITS) == (SBITS_SRCBLEND_ONE| SBITS_DSTBLEND_ZERO) || !(sbits & SBITS_BLEND_BITS)) 	/*if transparency is forced, clear alpha test bits*/
				sbits = (sbits & ~(SBITS_BLEND_BITS|SBITS_ATEST_BITS))
							| (SBITS_SRCBLEND_SRC_ALPHA | SBITS_DSTBLEND_ONE_MINUS_SRC_ALPHA);
		}

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
			switch((sbits & SBITS_DSTBLEND_BITS)>>4)
			{
			case SBITS_DSTBLEND_ZERO>>4:				dst = GL_ZERO;	break;
			default:
			case SBITS_DSTBLEND_ONE>>4:					dst = GL_ONE;	break;
			case SBITS_DSTBLEND_SRC_COLOR>>4:			dst = GL_SRC_COLOR;	break;
			case SBITS_DSTBLEND_ONE_MINUS_SRC_COLOR>>4:	dst = GL_ONE_MINUS_SRC_COLOR;	break;
			case SBITS_DSTBLEND_SRC_ALPHA>>4:			dst = GL_SRC_ALPHA;	break;
			case SBITS_DSTBLEND_ONE_MINUS_SRC_ALPHA>>4:	dst = GL_ONE_MINUS_SRC_ALPHA;	break;
			case SBITS_DSTBLEND_DST_ALPHA>>4:			dst = GL_DST_ALPHA;	break;
			case SBITS_DSTBLEND_ONE_MINUS_DST_ALPHA>>4:	dst = GL_ONE_MINUS_DST_ALPHA;	break;
			}
			qglEnable(GL_BLEND);
			qglBlendFunc(src, dst);
		}
		else
			qglDisable(GL_BLEND);
	}

#ifdef GL_ALPHA_TEST	//alpha test doesn't exist in gles2
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
#endif

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
	if (delta & (SBITS_MASK_BITS))
	{
		qglColorMask(
				(sbits&SBITS_MASK_RED)?GL_FALSE:GL_TRUE,
				(sbits&SBITS_MASK_GREEN)?GL_FALSE:GL_TRUE,
				(sbits&SBITS_MASK_BLUE)?GL_FALSE:GL_TRUE,
				(sbits&SBITS_MASK_ALPHA)?GL_FALSE:GL_TRUE
				);
	}
	if ((delta & SBITS_TRUFORM) && qglPNTrianglesiATI)
	{
		if ((sbits & SBITS_TRUFORM) && gl_ati_truform.ival)
			qglEnable(GL_PN_TRIANGLES_ATI);
		else
			qglDisable(GL_PN_TRIANGLES_ATI);
	}
}

static void BE_SubmitMeshChain(void)
{
	int startv, starti, endv, endi;
	int m;
	mesh_t *mesh;
	int batchtype = GL_TRIANGLES;

	if (shaderstate.flags & BEF_LINES)
		batchtype = GL_LINES;

#if 0
	if (!shaderstate.currentebo)
	{
	if (shaderstate.meshcount == 1)
	{
		mesh = shaderstate.meshes[0];
		qglDrawRangeElements(batchtype, mesh->vbofirstvert, mesh->vbofirstvert+mesh->numvertexes, mesh->numindexes, GL_INDEX_TYPE, shaderstate.sourcevbo->indicies + mesh->vbofirstelement);
		RQuantAdd(RQUANT_DRAWS, 1);
		return;
	}
	else
	{
		index_t *ilst;
		mesh = shaderstate.meshes[0];
		startv = mesh->vbofirstvert;
		endv = startv + mesh->numvertexes;
		endi = mesh->numindexes;
		for (m = 1; m < shaderstate.meshcount; m++)
		{
			mesh = shaderstate.meshes[m];
			endi += mesh->numindexes;

			if (startv > mesh->vbofirstvert)
				startv = mesh->vbofirstvert;
			if (endv < mesh->vbofirstvert+mesh->numvertexes)
				endv = mesh->vbofirstvert+mesh->numvertexes;
		}


		ilst = alloca(endi*sizeof(index_t));
		endi = 0;
		for (m = 0; m < shaderstate.meshcount; m++)
		{
			mesh = shaderstate.meshes[m];
			for (starti = 0; starti < mesh->numindexes; )
				ilst[endi++] = mesh->vbofirstvert + mesh->indexes[starti++];
		}
		qglDrawRangeElements(batchtype, startv, endv, endi, GL_INDEX_TYPE, ilst);
		RQuantAdd(RQUANT_DRAWS, 1);
	}


	return;
	}
#endif

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

		qglDrawRangeElements(batchtype, startv, endv, endi-starti, GL_INDEX_TYPE, (index_t*)shaderstate.sourcevbo->indicies.gl.addr + starti);
		RQuantAdd(RQUANT_DRAWS, 1);
 	}
/*
	if (qglUnlockArraysEXT)
		qglUnlockArraysEXT();
*/
}

static void DrawPass(const shaderpass_t *pass)
{
	int i, j, k;
	int tmu;
	int lastpass = pass->numMergedPasses;
	unsigned int attr = (1u<<VATTR_LEG_VERTEX) | (1u<<VATTR_LEG_COLOUR);

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
	BE_SendPassBlendDepthMask(pass[i].shaderbits);
	GenerateColourMods(pass+i);
	tmu = 0;
	for (; i < lastpass; i++)
	{
		if (pass[i].texgen == T_GEN_UPPEROVERLAY && !TEXVALID(shaderstate.curtexnums->upperoverlay))
			continue;
		if (pass[i].texgen == T_GEN_LOWEROVERLAY && !TEXVALID(shaderstate.curtexnums->loweroverlay))
			continue;
		if (pass[i].texgen == T_GEN_FULLBRIGHT && !TEXVALID(shaderstate.curtexnums->fullbright))
			continue;
		Shader_BindTextureForPass(tmu, pass+i);
		attr |= (1u<<(VATTR_LEG_TMU0+tmu));

		BE_GeneratePassTC(pass+i, tmu);

		BE_SetPassBlendMode(tmu, pass[i].blendmode);

		tmu++;

		//add in 
		if (pass[i].texgen == T_GEN_LIGHTMAP)
		{
			//first pass should have been REPLACE
			//second pass should be an ADD
			//this depends upon rgbgens for light levels, so each pass *must* be pushed to hardware individually

			for (j = 1; j < MAXLIGHTMAPS && shaderstate.curbatch->lightmap[j] >= 0; j++)
			{
				if (j == 1)
					BE_SetPassBlendMode(tmu, PBM_REPLACE);

				/*make sure no textures linger*/
				for (k = tmu; k < shaderstate.lastpasstmus; k++)
				{
					GL_LazyBind(k, 0, r_nulltex);
				}
				shaderstate.lastpasstmus = tmu;

				/*push it*/
				BE_EnableShaderAttributes(attr, 0);
				BE_SubmitMeshChain();
				tmu = 0;

				/*bind the light texture*/
				GL_LazyBind(tmu, GL_TEXTURE_2D, lightmap[shaderstate.curbatch->lightmap[j]]->lightmap_texture);

				/*set up the colourmod for this style's lighting*/
				shaderstate.pendingcolourvbo = 0;
				shaderstate.pendingcolourpointer = NULL;

				shaderstate.pendingcolourflat[0] = shaderstate.identitylighting * d_lightstylevalue[shaderstate.curbatch->lightstyle[j]]/256.0f;
				shaderstate.pendingcolourflat[1] = shaderstate.identitylighting * d_lightstylevalue[shaderstate.curbatch->lightstyle[j]]/256.0f;
				shaderstate.pendingcolourflat[2] = shaderstate.identitylighting * d_lightstylevalue[shaderstate.curbatch->lightstyle[j]]/256.0f;
				shaderstate.pendingcolourflat[3] = 1;

				/*pick the correct st coords for this lightmap pass*/
				shaderstate.pendingtexcoordparts[tmu] = 2;
				shaderstate.pendingtexcoordvbo[tmu] = shaderstate.sourcevbo->lmcoord[j].gl.vbo;
				shaderstate.pendingtexcoordpointer[tmu] = shaderstate.sourcevbo->lmcoord[j].gl.addr;

				BE_SetPassBlendMode(tmu, PBM_ADD);
				BE_SendPassBlendDepthMask((pass[0].shaderbits & ~SBITS_BLEND_BITS) | SBITS_SRCBLEND_ONE | SBITS_DSTBLEND_ONE);

				attr = (1u<<VATTR_LEG_VERTEX) | (1u<<VATTR_LEG_COLOUR);
				attr |= (1u<<(VATTR_LEG_TMU0+tmu));

				tmu++;
			}

			//might need to break the pass here
			if (j > 1 && i != lastpass)
			{
				for (k = tmu; k < shaderstate.lastpasstmus; k++)
				{
					GL_LazyBind(k, 0, r_nulltex);
				}
				shaderstate.lastpasstmus = tmu;
				BE_EnableShaderAttributes(attr, 0);

				BE_SubmitMeshChain();
				tmu = 0;

				BE_SendPassBlendDepthMask(pass[i+1].shaderbits);
				GenerateColourMods(&pass[i+1]);
			}
		}
	}

	if (!tmu)
		return;

	for (i = tmu; i < shaderstate.lastpasstmus; i++)
	{
		GL_LazyBind(i, 0, r_nulltex);
	}
	shaderstate.lastpasstmus = tmu;
	BE_EnableShaderAttributes(attr, 0);

	BE_SubmitMeshChain();
}

static void BE_Program_Set_Attributes(const program_t *prog, unsigned int perm, qboolean entunchanged)
{
	vec4_t param4;
	int r, g, b;
	int i;
	unsigned int ph;
	const shaderprogparm_t *p;

	/*don't bother setting it if the ent properties are unchanged (but do if the mesh changed)*/
	if (entunchanged)
		return;

	for (i = 0; i < prog->numparams; i++)
	{
		p = &prog->parm[i];
		ph = prog->permu[perm].parm[i];
		if (ph == -1)
			continue;	/*not in this permutation*/

		switch(p->type)
		{
		case SP_M_VIEW:
			qglUniformMatrix4fvARB(ph, 1, false, r_refdef.m_view);
			break;
		case SP_M_PROJECTION:
			qglUniformMatrix4fvARB(ph, 1, false, r_refdef.m_projection);
			break;
		case SP_M_MODELVIEW:
			qglUniformMatrix4fvARB(ph, 1, false, shaderstate.modelviewmatrix);
			break;
		case SP_M_MODELVIEWPROJECTION:
			{
				float m16[16];
				Matrix4_Multiply(r_refdef.m_projection, shaderstate.modelviewmatrix, m16);
				qglUniformMatrix4fvARB(ph, 1, false, m16);
			}
			break;
		case SP_M_INVMODELVIEWPROJECTION:
			{
				float m16[16], inv[16];
				Matrix4_Multiply(r_refdef.m_projection, shaderstate.modelviewmatrix, m16);
				Matrix4_Invert(m16, inv);
				qglUniformMatrix4fvARB(ph, 1, false, inv);
			}
			break;
		case SP_M_MODEL:
			qglUniformMatrix4fvARB(ph, 1, false, shaderstate.modelmatrix);
			break;
		case SP_M_ENTBONES:
			{
				qglUniformMatrix3x4fv(ph, shaderstate.sourcevbo->numbones, false, shaderstate.sourcevbo->bones);
			}
			break;
		case SP_M_INVVIEWPROJECTION:
			{
				float m16[16], inv[16];
				Matrix4_Multiply(r_refdef.m_projection, r_refdef.m_view, m16);
				Matrix4_Invert(m16, inv);
				qglUniformMatrix4fvARB(ph, 1, false, inv);
			}
			break;

		case SP_E_VBLEND:
			qglUniform2fvARB(ph, 1, shaderstate.meshes[0]->xyz_blendw);
			break;

		case SP_E_LMSCALE:
			if (perm & PERMUTATION_LIGHTSTYLES)
			{
				vec4_t colscale[MAXLIGHTMAPS];
				int j, s;
				for (j = 0; j < MAXLIGHTMAPS ; j++)
				{
					s = shaderstate.curbatch->lightstyle[j];
					if (s == 255)
					{
						for (; j < MAXLIGHTMAPS ; j++)
						{
							colscale[j][0] = 0;
							colscale[j][1] = 0;
							colscale[j][2] = 0;
							colscale[j][3] = 1;
						}
						break;
					}
					if (shaderstate.curentity->model && shaderstate.curentity->model->engineflags & MDLF_NEEDOVERBRIGHT)
					{
						float sc = (1<<bound(0, gl_overbright.ival, 2)) * shaderstate.identitylighting;
						VectorSet(colscale[j], sc, sc, sc);
					}
					else
					{
						VectorSet(colscale[j], shaderstate.identitylighting, shaderstate.identitylighting, shaderstate.identitylighting);
					}
					colscale[j][3] = 1;

					VectorScale(colscale[j], d_lightstylevalue[s]/256.0f, colscale[j]);
				}

				qglUniform4fvARB(ph, j, (GLfloat*)colscale);
				shaderstate.lastuniform = 0;
			}
			else
			{
				vec4_t colscale[4];
				if (shaderstate.curentity->model && shaderstate.curentity->model->engineflags & MDLF_NEEDOVERBRIGHT)
				{
					float sc = (1<<bound(0, gl_overbright.ival, 2)) * shaderstate.identitylighting;
					VectorSet(colscale[0], sc, sc, sc);
				}
				else
				{
					VectorSet(colscale[0], shaderstate.identitylighting, shaderstate.identitylighting, shaderstate.identitylighting);
				}
				colscale[0][3] = 1;

				qglUniform4fvARB(ph, 1, (GLfloat*)colscale);
			}
			break;

		case SP_E_GLOWMOD:
			qglUniform3fvARB(ph, 1, (GLfloat*)shaderstate.curentity->glowmod);
			break;
		case SP_E_ORIGIN:
			qglUniform3fvARB(ph, 1, (GLfloat*)shaderstate.curentity->origin);
			break;
		case SP_E_COLOURS:
			qglUniform4fvARB(ph, 1, (GLfloat*)shaderstate.curentity->shaderRGBAf);
			break;
		case SP_E_COLOURSIDENT:
			if (shaderstate.flags & BEF_FORCECOLOURMOD)
				qglUniform4fvARB(ph, 1, (GLfloat*)shaderstate.curentity->shaderRGBAf);
			else
				qglUniform4fARB(ph, 1, 1, 1, shaderstate.curentity->shaderRGBAf[3]);
			break;
		case SP_E_TOPCOLOURS:
			R_FetchTopColour(&r, &g, &b);
			param4[0] = r/255.0f;
			param4[1] = g/255.0f;
			param4[2] = b/255.0f;
			qglUniform3fvARB(ph, 1, param4);
			break;
		case SP_E_BOTTOMCOLOURS:
			R_FetchBottomColour(&r, &g, &b);
			param4[0] = r/255.0f;
			param4[1] = g/255.0f;
			param4[2] = b/255.0f;
			qglUniform3fvARB(ph, 1, param4);
			break;

		case SP_RENDERTEXTURESCALE:
			if (r_config.texture_non_power_of_two)
			{
				param4[0] = 1;
				param4[1] = 1;
			}
			else
			{
				r = 1;
				g = 1;
				while (r < vid.pixelwidth)
					r *= 2;
				while (g < vid.pixelheight)
					g *= 2;
				param4[0] = vid.pixelwidth/(float)r;
				param4[1] = vid.pixelheight/(float)g;
			}
			param4[2] = 0;
			param4[3] = 0;
			qglUniform4fvARB(ph, 1, param4);
			break;

		case SP_LIGHTSCREEN:
			{
				float v[4], tempv[4];

				v[0] = shaderstate.lightorg[0];
				v[1] = shaderstate.lightorg[1];
				v[2] = shaderstate.lightorg[2];
				v[3] = 1;

				Matrix4x4_CM_Transform4(shaderstate.modelviewmatrix, v, tempv); 
				Matrix4x4_CM_Transform4(r_refdef.m_projection, tempv, v);

				v[3] *= 2;
				v[0] = (v[0]/v[3]) + 0.5;
				v[1] = (v[1]/v[3]) + 0.5;
				v[2] = (v[2]/v[3]) + 0.5;

				qglUniform3fvARB(ph, 1, v);
			}
			break;
		case SP_LIGHTRADIUS:
			qglUniform1fARB(ph, shaderstate.lightradius);
			break;
		case SP_LIGHTCOLOUR:
			qglUniform3fvARB(ph, 1, shaderstate.lightcolours);
			break;
		case SP_W_FOG:
			qglUniform4fvARB(ph, 1, r_refdef.gfog_rgbd);
			break;
		case SP_V_EYEPOS:
			qglUniform3fvARB(ph, 1, r_origin);
			break;
		case SP_E_EYEPOS:
			{
				/*eye position in model space*/
				vec3_t t2;
				Matrix4x4_CM_Transform3(shaderstate.modelmatrixinv, r_origin, t2);
				qglUniform3fvARB(ph, 1, t2);
			}
			break;
		case SP_LIGHTPOSITION:
			{
				/*light position in model space*/
				vec3_t t2;
				Matrix4x4_CM_Transform3(shaderstate.modelmatrixinv, shaderstate.lightorg, t2);
				qglUniform3fvARB(ph, 1, t2);
			}
			break;
		case SP_LIGHTCOLOURSCALE:
			qglUniform3fvARB(ph, 1, shaderstate.lightcolourscale);
			break;
		case SP_LIGHTPROJMATRIX:
			{
				float t[16];
				Matrix4x4_CM_Projection_Far(t, 90, 90, 4, 3000);
				qglUniformMatrix4fvARB(ph, 1, false, t);
			}
			break;
		case SP_LIGHTCUBEMATRIX:
			/*light's texture projection matrix*/
			{
				float t[16];
				Matrix4_Multiply(shaderstate.modelmatrix, shaderstate.lightprojmatrix, t);
				qglUniformMatrix4fvARB(ph, 1, false, t);
			}
			break;

		/*static lighting info*/
		case SP_E_L_DIR:
			qglUniform3fvARB(ph, 1, (float*)shaderstate.curentity->light_dir);
			break;
		case SP_E_L_MUL:
			qglUniform3fvARB(ph, 1, (float*)shaderstate.curentity->light_range);
			break;
		case SP_E_L_AMBIENT:
			qglUniform3fvARB(ph, 1, (float*)shaderstate.curentity->light_avg);
			break;

		case SP_E_TIME:
			qglUniform1fARB(ph, shaderstate.curtime);
			break;
		case SP_CONSTI:
		case SP_TEXTURE:
			qglUniform1iARB(ph, p->ival);
			break;
		case SP_CONSTF:
			qglUniform1fARB(ph, p->fval);
			break;
		case SP_CVARI:
			qglUniform1iARB(ph, ((cvar_t*)p->pval)->ival);
			break;
		case SP_CVARF:
			qglUniform1fARB(ph, ((cvar_t*)p->pval)->value);
			break;
		case SP_CVAR3F:
			{
				cvar_t *var = (cvar_t*)p->pval;
				char *vs = var->string;
				vs = COM_Parse(vs);
				param4[0] = atof(com_token);
				vs = COM_Parse(vs);
				param4[1] = atof(com_token);
				vs = COM_Parse(vs);
				param4[2] = atof(com_token);
				qglUniform3fvARB(ph, 1, param4);
			}
			break;

		default:
			Host_EndGame("Bad shader program parameter type (%i)", p->type);
			break;
		}
	}
}

static void BE_RenderMeshProgram(const shader_t *shader, const shaderpass_t *pass)
{
	program_t *p = shader->prog;
	int	i;

	int perm;

	perm = 0;
	if (shaderstate.sourcevbo->numbones)
	{
		if (p->permu[perm|PERMUTATION_SKELETAL].handle.glsl)
			perm |= PERMUTATION_SKELETAL;
		else
			return;
	}
	if (p->permu[perm|PERMUTATION_FRAMEBLEND].handle.glsl && shaderstate.sourcevbo->coord2.gl.addr)
		perm |= PERMUTATION_FRAMEBLEND;
	if (TEXVALID(shaderstate.curtexnums->bump) && p->permu[perm|PERMUTATION_BUMPMAP].handle.glsl)
		perm |= PERMUTATION_BUMPMAP;
	if (TEXVALID(shaderstate.curtexnums->fullbright) && p->permu[perm|PERMUTATION_FULLBRIGHT].handle.glsl)
		perm |= PERMUTATION_FULLBRIGHT;
	if ((TEXVALID(shaderstate.curtexnums->loweroverlay) || TEXVALID(shaderstate.curtexnums->upperoverlay)) && p->permu[perm|PERMUTATION_UPPERLOWER].handle.glsl)
		perm |= PERMUTATION_UPPERLOWER;
	if (r_refdef.gfog_rgbd[3] && p->permu[perm|PERMUTATION_FOG].handle.glsl)
		perm |= PERMUTATION_FOG;
	if (p->permu[perm|PERMUTATION_DELUXE].handle.glsl && TEXVALID(shaderstate.curtexnums->bump) && shaderstate.curbatch->lightmap[0] >= 0 && lightmap[shaderstate.curbatch->lightmap[0]]->hasdeluxe)
		perm |= PERMUTATION_DELUXE;
	if (shaderstate.curbatch->lightmap[1] >= 0 && p->permu[perm|PERMUTATION_LIGHTSTYLES].handle.glsl)
		perm |= PERMUTATION_LIGHTSTYLES;

	GL_SelectProgram(p->permu[perm].handle.glsl);
#ifndef FORCESTATE
	if (shaderstate.lastuniform == p->permu[perm].handle.glsl)
		i = true;
	else
#endif
	{
		i = false;
		shaderstate.lastuniform = p->permu[perm].handle.glsl;
	}
	BE_Program_Set_Attributes(p, perm, i);

	BE_SendPassBlendDepthMask(pass->shaderbits);
	BE_EnableShaderAttributes(p->permu[perm].attrmask, shaderstate.sourcevbo->vao);
	if (p->nofixedcompat)
	{
		for (i = 0; i < pass->numMergedPasses; i++)
		{
			Shader_BindTextureForPass(i, pass+i);
		}
		if (perm & PERMUTATION_LIGHTSTYLES)
		{
			GL_LazyBind(i++, GL_TEXTURE_2D, shaderstate.curbatch->lightmap[1] >= 0?lightmap[shaderstate.curbatch->lightmap[1]]->lightmap_texture:r_nulltex);
			GL_LazyBind(i++, GL_TEXTURE_2D, shaderstate.curbatch->lightmap[2] >= 0?lightmap[shaderstate.curbatch->lightmap[2]]->lightmap_texture:r_nulltex);
			GL_LazyBind(i++, GL_TEXTURE_2D, shaderstate.curbatch->lightmap[3] >= 0?lightmap[shaderstate.curbatch->lightmap[3]]->lightmap_texture:r_nulltex);

			//we need this loop to fix up fixed-function stuff
			for (; i < shaderstate.lastpasstmus; i++)
			{
				GL_LazyBind(i, 0, r_nulltex);
			}
			shaderstate.lastpasstmus = pass->numMergedPasses+3;
		}
		else
		{
			//we need this loop to fix up fixed-function stuff
			for (; i < shaderstate.lastpasstmus; i++)
			{
				GL_LazyBind(i, 0, r_nulltex);
			}
			shaderstate.lastpasstmus = pass->numMergedPasses;
		}
	}
	else
	{
		GenerateColourMods(pass);
		for (i = 0; i < pass->numMergedPasses; i++)
		{
			Shader_BindTextureForPass(i, pass+i);
			BE_GeneratePassTC(pass+i, i);
		}
		for (; i < shaderstate.lastpasstmus; i++)
		{
			GL_LazyBind(i, 0, r_nulltex);
		}
		shaderstate.lastpasstmus = pass->numMergedPasses;
	}
	BE_SubmitMeshChain();
}

qboolean GLBE_LightCullModel(vec3_t org, model_t *model)
{
#ifdef RTLIGHTS
	if ((shaderstate.mode == BEM_LIGHT || shaderstate.mode == BEM_STENCIL || shaderstate.mode == BEM_DEPTHONLY))
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
#endif
	return false;
}

//Note: Be cautious about using BEM_LIGHT here, as it won't select the light.
void GLBE_SelectMode(backendmode_t mode)
{
	extern int gldepthfunc;

	if (mode != shaderstate.mode)
	{
		shaderstate.mode = mode;
		shaderstate.flags = 0;
		switch (mode)
		{
		default:
			break;
		case BEM_DEPTHONLY:
			GL_DeSelectProgram();
			/*BEM_DEPTHONLY does support mesh writing, but its not the only way its used... FIXME!*/
			while(shaderstate.lastpasstmus>0)
			{
				GL_LazyBind(--shaderstate.lastpasstmus, 0, r_nulltex);
			}

			//we don't write or blend anything (maybe alpha test... but mneh)
			BE_SendPassBlendDepthMask(SBITS_MISC_DEPTHWRITE | SBITS_MASK_BITS);

			BE_SetPassBlendMode(0, PBM_REPLACE);
			GL_CullFace(SHADER_CULL_FRONT);
			break;

#ifdef RTLIGHTS
		case BEM_STENCIL:
			/*BEM_STENCIL doesn't support mesh writing*/
			GLBE_PushOffsetShadow(false);

			if (gl_config.nofixedfunc && !shaderstate.allblackshader)
			{
				char *defs[] = {NULL};
				shaderstate.allblackshader = GLSlang_CreateProgram("allblackprogram", gl_config.gles?100:110, defs, "#include \"sys/skeletal.h\"\nvoid main(){gl_Position = skeletaltransform();}", "void main(){gl_FragColor=vec4(0.0,0.0,0.0,1.0);}", false);
			}

			//disable all tmus
			while(shaderstate.lastpasstmus>0)
			{
				GL_LazyBind(--shaderstate.lastpasstmus, 0, r_nulltex);
			}
			if (!gl_config.nofixedfunc)
			{
				GL_DeSelectProgram();

			//replace mode please
				BE_SetPassBlendMode(0, PBM_REPLACE);
			}

			//we don't write or blend anything (maybe alpha test... but mneh)
			BE_SendPassBlendDepthMask(SBITS_MISC_DEPTHCLOSERONLY | SBITS_MASK_BITS);
			GL_CullFace(0);

			//don't change cull stuff, and
			//don't actually change stencil stuff - caller needs to be
			//aware of how many times stuff is drawn, so they can do that themselves.
			break;
		case BEM_CREPUSCULAR:
			if (!shaderstate.crepopaqueshader)
			{
				shaderstate.crepopaqueshader = R_RegisterShader("crepuscular_opaque",
					"{\n"
						"program crepuscular_opaque\n"
					"}\n"
					);
			}
			if (!shaderstate.crepskyshader)
			{
				shaderstate.crepskyshader = R_RegisterShader("crepuscular_sky",
					"{\n"
						"program crepuscular_sky\n"
						"{\n"
							"map $diffuse\n"
						"}\n"
						"{\n"
							"map $fullbright\n"
						"}\n"
					"}\n"
					);
			}
			break;
#endif
		case BEM_FOG:
			while(shaderstate.lastpasstmus>0)
			{
				GL_LazyBind(--shaderstate.lastpasstmus, 0, r_nulltex);
			}
			GL_LazyBind(0, GL_TEXTURE_2D, shaderstate.fogtexture);
			shaderstate.lastpasstmus = 1;

			Vector4Set(shaderstate.pendingcolourflat, 1, 1, 1, 1);
			shaderstate.pendingcolourvbo = 0;
			shaderstate.pendingcolourpointer = NULL;
			BE_SetPassBlendMode(0, PBM_MODULATE);
			BE_SendPassBlendDepthMask(SBITS_SRCBLEND_SRC_ALPHA | SBITS_DSTBLEND_ONE_MINUS_SRC_ALPHA | SBITS_MISC_DEPTHEQUALONLY);
			break;
		}
	}
}

void GLBE_SelectEntity(entity_t *ent)
{
	float nd;
	shaderstate.curentity = ent;
	currententity = ent;
	R_RotateForEntity(shaderstate.modelmatrix, shaderstate.modelviewmatrix, shaderstate.curentity, shaderstate.curentity->model);
	Matrix4_Invert(shaderstate.modelmatrix, shaderstate.modelmatrixinv);
	if (qglLoadMatrixf)
		qglLoadMatrixf(shaderstate.modelviewmatrix);

	if (shaderstate.curentity->flags & Q2RF_DEPTHHACK)
		nd = 0.3;
	else
		nd = 1;
	if (shaderstate.depthrange != nd)
	{
		shaderstate.depthrange = nd;
		if (qglDepthRange)
			qglDepthRange (gldepthmin, gldepthmin + shaderstate.depthrange*(gldepthmax-gldepthmin));
		else if (qglDepthRangef)
			qglDepthRangef (gldepthmin, gldepthmin + shaderstate.depthrange*(gldepthmax-gldepthmin));
	}

	shaderstate.lastuniform = 0;
	shaderstate.curtime = shaderstate.updatetime - shaderstate.curentity->shaderTime;
}
#if 0
static void BE_SelectFog(vec3_t colour, float alpha, float density)
{
	float zscale;

	density /= 64;

	zscale = 2048;	/*this value is meant to be the distance at which fog the value becomes as good as fully fogged, just hack it to 2048...*/
	GenerateFogTexture(&shaderstate.fogtexture, density, zscale);
	shaderstate.fogfar = 1/zscale; /*scaler for z coords*/

	qglColor4f(colour[0], colour[1], colour[2], alpha);
}
#endif
#ifdef RTLIGHTS
static qboolean GLBE_RegisterLightShader(int mode)
{
	if (!shaderstate.inited_shader_light[mode])
	{
		char *name = va("rtlight%s%s%s", 
			(mode & (1u<<LSHADER_SMAP))?"#PCF":"",
			(mode & (1u<<LSHADER_SPOT))?"#SPOT":"",
			(mode & (1u<<LSHADER_CUBE))?"#CUBE":"");

		shaderstate.inited_shader_light[mode] = true;
		shaderstate.shader_light[mode] = R_RegisterCustom(name, Shader_LightPass, NULL);

		//make sure it has a program and forget it if it doesn't, to save a compare.
		if (!shaderstate.shader_light[mode] || !shaderstate.shader_light[mode]->prog)
		{
			shaderstate.shader_light[mode] = NULL;
		}
	}

	if (shaderstate.shader_light[mode]);
		return true;
	return false;
}
#endif

void GLBE_SelectDLight(dlight_t *dl, vec3_t colour)
{
	float view[16], proj[16];
	int lmode;
	extern cvar_t gl_specular;
	extern cvar_t r_shadow_shadowmapping;

	/*generate light projection information*/
	float nearplane = 4;
	if (dl->fov)
	{
		Matrix4x4_CM_Projection_Far(proj, dl->fov, dl->fov, nearplane, dl->radius);
		Matrix4x4_CM_ModelViewMatrixFromAxis(view, dl->axis[0], dl->axis[1], dl->axis[2], dl->origin);
		Matrix4_Multiply(proj, view, shaderstate.lightprojmatrix);
	}
	else
	{
		Matrix4x4_CM_Projection_Far(proj, 90, 90, nearplane, dl->radius);
		Matrix4x4_CM_ModelViewMatrixFromAxis(shaderstate.lightprojmatrix, dl->axis[0], dl->axis[1], dl->axis[2], dl->origin);
	}

	/*simple info*/
	shaderstate.lightradius = dl->radius;
	VectorCopy(dl->origin, shaderstate.lightorg);
	VectorCopy(dl->lightcolourscales, shaderstate.lightcolourscale);
	shaderstate.lightcolourscale[2] *= gl_specular.value;
	VectorCopy(colour, shaderstate.lightcolours);
#ifdef RTLIGHTS
	shaderstate.lightcubemap = dl->cubetexture;
#endif

	shaderstate.lastuniform = 0;

	lmode = 0;
#ifdef RTLIGHTS
	if (((dl->flags & LFLAG_SHADOWMAP) || r_shadow_shadowmapping.ival) && GLBE_RegisterLightShader(lmode | (1u<<LSHADER_SMAP)))
		lmode |= 1u<<LSHADER_SMAP;
	else
		GLBE_RegisterLightShader(lmode);	//make sure either shadowmapping or non-shadowmapping is loaded...
	if (TEXVALID(shaderstate.lightcubemap) && GLBE_RegisterLightShader(lmode | (1u<<LSHADER_CUBE)))
		lmode |= 1u<<LSHADER_CUBE;
	else if (dl->fov && GLBE_RegisterLightShader(lmode | (1u<<LSHADER_SPOT)))
		lmode |= 1u<<LSHADER_SPOT;
#endif
	shaderstate.lightmode = lmode;
}

void GLBE_PushOffsetShadow(qboolean pushdepth)
{
	extern cvar_t r_polygonoffset_stencil_offset, r_polygonoffset_stencil_factor;
	polyoffset_t po;
	if (pushdepth)
	{
		/*some quake doors etc are flush with the walls that they're meant to be hidden behind, or plats the same height as the floor, etc
		we move them back very slightly using polygonoffset to avoid really ugly z-fighting*/
		extern cvar_t r_polygonoffset_submodel_offset, r_polygonoffset_submodel_factor;
		po.factor = r_polygonoffset_submodel_factor.value + r_polygonoffset_stencil_factor.value;
		po.unit = r_polygonoffset_submodel_offset.value + r_polygonoffset_stencil_offset.value;
	}
	else
	{
		po.factor = r_polygonoffset_stencil_factor.value;
		po.unit = r_polygonoffset_stencil_offset.value;
	}

#ifndef FORCESTATE
	if (shaderstate.curpolyoffset.factor != po.factor || shaderstate.curpolyoffset.unit != po.unit)
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

#ifdef RTLIGHTS
texid_t GenerateNormalisationCubeMap(void);
static void BE_LegacyLighting(void)
{
	//bigfoot wants rtlight support without glsl, so here goes madness...
	//register combiners for bumpmapping using 4 tmus...
	float *col;
	float *ldir;
	vec3_t lightdir, rellight;
	float scale;
	int i, m;
	mesh_t *mesh;
	unsigned int attr = (1u<<VATTR_LEG_VERTEX) | (1u<<VATTR_LEG_COLOUR);
	int tmu = 0;

	BE_SendPassBlendDepthMask(SBITS_SRCBLEND_ONE | SBITS_DSTBLEND_ONE);

	//rotate this into modelspace
	Matrix4x4_CM_Transform3(shaderstate.modelmatrixinv, shaderstate.lightorg, rellight);

	for (m = 0; m < shaderstate.meshcount; m++)
	{
		mesh = shaderstate.meshes[m];

		col = coloursarray[0] + mesh->vbofirstvert*4;
		ldir = texcoordarray[0] + mesh->vbofirstvert*3;
		for (i = 0; i < mesh->numvertexes; i++, col+=4, ldir+=3)
		{
			VectorSubtract(rellight, mesh->xyz_array[i], lightdir);
			scale = VectorNormalize(lightdir);
			scale = 1 - (scale/shaderstate.lightradius);
			VectorScale(shaderstate.lightcolours, scale, col);
			col[3] = 1;
			ldir[0] = -DotProduct(lightdir, mesh->snormals_array[i]);
			ldir[1] = DotProduct(lightdir, mesh->tnormals_array[i]);
			ldir[2] = DotProduct(lightdir, mesh->normals_array[i]);
		}
	}

	if (shaderstate.curtexnums->bump.num && gl_config.arb_texture_cube_map && gl_config.arb_texture_env_dot3 && gl_config.arb_texture_env_combine && be_maxpasses >= 4)
	{	//we could get this down to 2 tmus by arranging for the dot3 result to be written the alpha buffer. But then we'd need to have an alpha buffer too.

		if (!shaderstate.normalisationcubemap.num)
			shaderstate.normalisationcubemap = GenerateNormalisationCubeMap();

		//tmu0: normalmap+replace+regular tex coords
		GL_LazyBind(tmu, GL_TEXTURE_2D, shaderstate.curtexnums->bump);
		BE_SetPassBlendMode(tmu, PBM_REPLACE);
		shaderstate.pendingtexcoordparts[tmu] = 2;
		shaderstate.pendingtexcoordvbo[tmu] = shaderstate.sourcevbo->texcoord.gl.vbo;
		shaderstate.pendingtexcoordpointer[tmu] = shaderstate.sourcevbo->texcoord.gl.addr;
		attr |= (1u<<(VATTR_LEG_TMU0+tmu));
		tmu++;

		//tmu1: normalizationcubemap+dot3+lightdir
		GL_LazyBind(tmu, GL_TEXTURE_CUBE_MAP_ARB, shaderstate.normalisationcubemap);
		BE_SetPassBlendMode(tmu, PBM_DOTPRODUCT);
		shaderstate.pendingtexcoordparts[tmu] = 3;
		shaderstate.pendingtexcoordvbo[tmu] = 0;
		shaderstate.pendingtexcoordpointer[tmu] = texcoordarray[0];
		attr |= (1u<<(VATTR_LEG_TMU0+tmu));
		tmu++;

		//tmu2: $diffuse+multiply+regular tex coords
		GL_LazyBind(tmu, GL_TEXTURE_2D, shaderstate.curtexnums->base);	//texture not used, its just to make sure the code leaves it enabled.
		BE_SetPassBlendMode(tmu, PBM_MODULATE);
		shaderstate.pendingtexcoordparts[tmu] = 2;
		shaderstate.pendingtexcoordvbo[tmu] = shaderstate.sourcevbo->texcoord.gl.vbo;
		shaderstate.pendingtexcoordpointer[tmu] = shaderstate.sourcevbo->texcoord.gl.addr;
		attr |= (1u<<(VATTR_LEG_TMU0+tmu));
		tmu++;
	
		//tmu3: $any+multiply-by-colour+notc
		GL_LazyBind(tmu, GL_TEXTURE_2D, shaderstate.curtexnums->bump);	//texture not used, its just to make sure the code leaves it enabled.
		BE_SetPassBlendMode(tmu, PBM_MODULATE_PREV_COLOUR);
		shaderstate.pendingtexcoordparts[tmu] = 0;
		shaderstate.pendingtexcoordvbo[tmu] = 0;
		shaderstate.pendingtexcoordpointer[tmu] = NULL;
		tmu++;

		//note we need 4 combiners in the first because we can't use the colour argument in the first without breaking the normals.

		for (i = tmu; i < shaderstate.lastpasstmus; i++)
		{
			GL_LazyBind(i, 0, r_nulltex);
		}
		shaderstate.lastpasstmus = tmu;
	}
	else
	{
		attr |= (1u<<(VATTR_LEG_TMU0));

		//tmu0: $diffuse+multiply+regular tex coords
		//multiplies by vertex colours
		GL_LazyBind(0, GL_TEXTURE_2D, shaderstate.curtexnums->base);	//texture not used, its just to make sure the code leaves it enabled.
		BE_SetPassBlendMode(0, PBM_MODULATE);
		shaderstate.pendingtexcoordvbo[0] = shaderstate.sourcevbo->texcoord.gl.vbo;
		shaderstate.pendingtexcoordpointer[0] = shaderstate.sourcevbo->texcoord.gl.addr;

		for (i = 1; i < shaderstate.lastpasstmus; i++)
		{
			GL_LazyBind(i, 0, r_nulltex);
		}
		shaderstate.lastpasstmus = 1;
	}

	shaderstate.colourarraytype = GL_FLOAT;
	shaderstate.pendingcolourvbo = 0;
	shaderstate.pendingcolourpointer = coloursarray;

	GL_DeSelectProgram();
	BE_EnableShaderAttributes(attr, 0);

	BE_SubmitMeshChain();

	GL_LazyBind(1, 0, r_nulltex);
	GL_LazyBind(2, 0, r_nulltex);
	GL_LazyBind(3, 0, r_nulltex);
}
#endif

static void DrawMeshes(void)
{
	const shaderpass_t *p;
	int passno;
	passno = 0;

	if (shaderstate.force2d)
	{
		RQuantAdd(RQUANT_2DBATCHES, 1);
	}
	else if (shaderstate.curentity == &r_worldentity)
	{
		RQuantAdd(RQUANT_WORLDBATCHES, 1);
	}
	else
	{
		RQuantAdd(RQUANT_ENTBATCHES, 1);
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

	if (shaderstate.sourcevbo->coord2.gl.addr)
		GenerateVertexBlends(shaderstate.curshader);
	else if (shaderstate.curshader->numdeforms)
		GenerateVertexDeforms(shaderstate.curshader);
	else
	{
		shaderstate.pendingvertexpointer = shaderstate.sourcevbo->coord.gl.addr;
		shaderstate.pendingvertexvbo = shaderstate.sourcevbo->coord.gl.vbo;
	}

	BE_PolyOffset(shaderstate.flags & BEF_PUSHDEPTH);
	switch(shaderstate.mode)
	{
	case BEM_STENCIL:
		Host_Error("Shader system is not meant to accept stencil meshes\n");
		break;
#ifdef RTLIGHTS
	case BEM_SMAPLIGHTSPOT:
//		if (shaderstate.shader_spot->prog)
//			BE_RenderMeshProgram(shaderstate.shader_spot, shaderstate.shader_spot->passes);
//		break;
	case BEM_SMAPLIGHT:
//		if (shaderstate.shader_smap->prog)
//			BE_RenderMeshProgram(shaderstate.shader_smap, shaderstate.shader_smap->passes);
//		break;
	case BEM_LIGHT:
		if (!shaderstate.shader_light[shaderstate.lightmode])
			BE_LegacyLighting();
		else
			BE_RenderMeshProgram(shaderstate.shader_light[shaderstate.lightmode], shaderstate.shader_light[shaderstate.lightmode]->passes);
		break;
	case BEM_DEPTHNORM:
		BE_RenderMeshProgram(shaderstate.depthnormshader, shaderstate.depthnormshader->passes);
		break;
#endif
	case BEM_CREPUSCULAR:
		if (shaderstate.curshader->flags & SHADER_SKY)
			BE_RenderMeshProgram(shaderstate.crepskyshader, shaderstate.crepskyshader->passes);
		else
			BE_RenderMeshProgram(shaderstate.crepopaqueshader, shaderstate.crepopaqueshader->passes);
		break;
	case BEM_DEPTHONLY:
		GL_DeSelectProgram();
#ifdef warningmsg
#pragma warningmsg("fixme: support alpha test")
#endif
		BE_EnableShaderAttributes((1u<<VATTR_LEG_VERTEX), 0);
		BE_SubmitMeshChain();
		break;

	case BEM_FOG:
		GL_DeSelectProgram();

		GenerateTCFog(0, NULL);
		BE_EnableShaderAttributes((1u<<VATTR_LEG_VERTEX), 0);
		BE_SubmitMeshChain();
		break;

	case BEM_WIREFRAME:
		//FIXME: do this with a shader instead? its not urgent as we can draw the shader normally anyway, just faster.
		GL_DeSelectProgram();
		shaderstate.pendingcolourvbo = 0;
		shaderstate.pendingcolourpointer = NULL;
		Vector4Set(shaderstate.pendingcolourflat, 1, 1, 1, 1);
		while(shaderstate.lastpasstmus>0)
		{
			GL_LazyBind(--shaderstate.lastpasstmus, 0, r_nulltex);
		}
		BE_SetPassBlendMode(0, PBM_REPLACE);
		BE_SendPassBlendDepthMask(shaderstate.curshader->passes[0].shaderbits | SBITS_MISC_NODEPTHTEST);

		BE_EnableShaderAttributes((1u<<VATTR_LEG_VERTEX) | (1u<<VATTR_LEG_COLOUR), 0);
		BE_SubmitMeshChain();
		break;
	case BEM_DEPTHDARK:
		if ((shaderstate.curshader->flags & SHADER_HASLIGHTMAP) && !TEXVALID(shaderstate.curtexnums->fullbright) && !gl_config.nofixedfunc)
		{
			if (gl_config.arb_shader_objects)
			{
				char *defs[] = {NULL};
				if (!shaderstate.allblackshader)
					shaderstate.allblackshader = GLSlang_CreateProgram("allblackprogram", gl_config.gles?100:110, defs, "#include \"sys/skeletal.h\"\nvoid main(){gl_Position = skeletaltransform();}", "void main(){gl_FragColor=vec4(0.0,0.0,0.0,1.0);}", false);

				GL_SelectProgram(shaderstate.allblackshader);
				BE_SendPassBlendDepthMask(shaderstate.curshader->passes[0].shaderbits);
				BE_EnableShaderAttributes(1u<<VATTR_LEG_VERTEX, 0);
				if (shaderstate.allblackshader != shaderstate.lastuniform)
				{
					float m16[16];
					Matrix4_Multiply(r_refdef.m_projection, shaderstate.modelviewmatrix, m16);
					qglUniformMatrix4fvARB(qglGetUniformLocationARB(shaderstate.allblackshader, "m_modelviewprojection"), 1, false, m16);
				}
				BE_SubmitMeshChain();

				shaderstate.lastuniform = shaderstate.allblackshader;
			}
			else
			{
				GL_DeSelectProgram();
				shaderstate.pendingcolourvbo = 0;
				shaderstate.pendingcolourpointer = NULL;
				Vector4Set(shaderstate.pendingcolourflat, 0, 0, 0, 1);
				while(shaderstate.lastpasstmus>0)
				{
					GL_LazyBind(--shaderstate.lastpasstmus, 0, r_nulltex);
				}

				BE_SetPassBlendMode(0, PBM_REPLACE);
				BE_SendPassBlendDepthMask(shaderstate.curshader->passes[0].shaderbits);

				BE_EnableShaderAttributes((1u<<VATTR_LEG_VERTEX) | (1u<<VATTR_LEG_COLOUR), 0);
				BE_SubmitMeshChain();
			}
			break;
		}
		//fallthrough
	case BEM_STANDARD:
	default:
		if (shaderstate.curshader->prog)
		{
			BE_RenderMeshProgram(shaderstate.curshader, shaderstate.curshader->passes);
		}
		else if (gl_config.nofixedfunc)
			break;
		else
		{
			GL_DeSelectProgram();
			while (passno < shaderstate.curshader->numpasses)
			{
				p = &shaderstate.curshader->passes[passno];
				passno += p->numMergedPasses;
		//		if (p->flags & SHADER_PASS_DETAIL)
		//			continue;

				DrawPass(p);
			}
		}
		if (shaderstate.curbatch->fog)
		{
			GL_DeSelectProgram();

			GenerateFogTexture(&shaderstate.fogtexture, shaderstate.curbatch->fog->shader->fog_dist, 2048);
			shaderstate.fogfar = 1.0f/2048; /*scaler for z coords*/

			while(shaderstate.lastpasstmus>0)
			{
				GL_LazyBind(--shaderstate.lastpasstmus, 0, r_nulltex);
			}
			GL_LazyBind(0, GL_TEXTURE_2D, shaderstate.fogtexture);
			shaderstate.lastpasstmus = 1;

			Vector4Scale(shaderstate.curbatch->fog->shader->fog_color, (1/255.0), shaderstate.pendingcolourflat);
			shaderstate.pendingcolourvbo = 0;
			shaderstate.pendingcolourpointer = NULL;
			BE_SetPassBlendMode(0, PBM_MODULATE);
			BE_SendPassBlendDepthMask(SBITS_SRCBLEND_SRC_ALPHA | SBITS_DSTBLEND_ONE_MINUS_SRC_ALPHA | SBITS_MISC_DEPTHEQUALONLY);

			GenerateTCFog(0, shaderstate.curbatch->fog);
			BE_EnableShaderAttributes((1u<<VATTR_LEG_VERTEX) | (1u<<VATTR_LEG_COLOUR) | (1u<<VATTR_LEG_TMU0), 0);
			BE_SubmitMeshChain();
		}
		break;
	}
}

void GLBE_DrawMesh_List(shader_t *shader, int nummeshes, mesh_t **meshlist, vbo_t *vbo, texnums_t *texnums, unsigned int beflags)
{
	shaderstate.curbatch = &shaderstate.dummybatch;
	if (!vbo)
	{
		mesh_t *m;
		shaderstate.sourcevbo = &shaderstate.dummyvbo;
		shaderstate.curshader = shader;
		shaderstate.flags = beflags;
		TRACE(("GLBE_DrawMesh_List: shader %s\n", shader->name));
		if (shaderstate.curentity != &r_worldentity)
		{
			GLBE_SelectEntity(&r_worldentity);
		}
		shaderstate.curtexnums = texnums;

		while (nummeshes--)
		{
			m = *meshlist++;

			shaderstate.dummyvbo.coord.gl.addr = m->xyz_array;
			shaderstate.dummyvbo.coord2.gl.addr = m->xyz2_array;
			shaderstate.dummyvbo.texcoord.gl.addr = m->st_array;
			shaderstate.dummyvbo.indicies.gl.addr = m->indexes;
			shaderstate.dummyvbo.normals.gl.addr = m->normals_array;
			shaderstate.dummyvbo.svector.gl.addr = m->snormals_array;
			shaderstate.dummyvbo.tvector.gl.addr = m->tnormals_array;
			if (m->colors4f_array)
			{
				shaderstate.colourarraytype = GL_FLOAT;
				shaderstate.dummyvbo.colours.gl.addr = m->colors4f_array;
			}
			else
			{
				shaderstate.colourarraytype = GL_UNSIGNED_BYTE;
				shaderstate.dummyvbo.colours.gl.addr = m->colors4b_array;
			}
			shaderstate.dummyvbo.bones = m->bones;
			shaderstate.dummyvbo.numbones = m->numbones;
			shaderstate.dummyvbo.bonenums.gl.addr = m->bonenums;
			shaderstate.dummyvbo.boneweights.gl.addr = m->boneweights;

			shaderstate.meshcount = 1;
			shaderstate.meshes = &m;
			DrawMeshes();
		}
	}
	else
	{
		shaderstate.sourcevbo = vbo;
		shaderstate.colourarraytype = GL_FLOAT;
		shaderstate.curshader = shader;
		shaderstate.flags = beflags;
		if (shaderstate.curentity != &r_worldentity)
		{
			GLBE_SelectEntity(&r_worldentity);
		}
		shaderstate.curtexnums = texnums;

		shaderstate.meshcount = nummeshes;
		shaderstate.meshes = meshlist;
		DrawMeshes();
	}
}
void GLBE_DrawMesh_Single(shader_t *shader, mesh_t *mesh, vbo_t *vbo, texnums_t *texnums, unsigned int beflags)
{
	shader->next = NULL;
	BE_DrawMesh_List(shader, 1, &mesh, NULL, texnums, beflags);
}

void GLBE_SubmitBatch(batch_t *batch)
{
	int lm;

	shaderstate.curbatch = batch;
	if (batch->vbo)
	{
		shaderstate.sourcevbo = batch->vbo;
		shaderstate.colourarraytype = GL_FLOAT;
	}
	else
	{
		shaderstate.dummyvbo.coord.gl.addr = batch->mesh[0]->xyz_array;
		shaderstate.dummyvbo.coord2.gl.addr = batch->mesh[0]->xyz2_array;
		shaderstate.dummyvbo.texcoord.gl.addr = batch->mesh[0]->st_array;
		shaderstate.dummyvbo.lmcoord[0].gl.addr = batch->mesh[0]->lmst_array[0];
		shaderstate.dummyvbo.indicies.gl.addr = batch->mesh[0]->indexes;
		shaderstate.dummyvbo.normals.gl.addr = batch->mesh[0]->normals_array;
		shaderstate.dummyvbo.svector.gl.addr = batch->mesh[0]->snormals_array;
		shaderstate.dummyvbo.tvector.gl.addr = batch->mesh[0]->tnormals_array;
		if (batch->mesh[0]->colors4f_array)
		{
			shaderstate.colourarraytype = GL_FLOAT;
			shaderstate.dummyvbo.colours.gl.addr = batch->mesh[0]->colors4f_array;
		}
		else
		{
			shaderstate.colourarraytype = GL_UNSIGNED_BYTE;
			shaderstate.dummyvbo.colours.gl.addr = batch->mesh[0]->colors4b_array;
		}
		shaderstate.dummyvbo.bones = batch->mesh[0]->bones;
		shaderstate.dummyvbo.numbones = batch->mesh[0]->numbones;
		shaderstate.dummyvbo.bonenums.gl.addr = batch->mesh[0]->bonenums;
		shaderstate.dummyvbo.boneweights.gl.addr = batch->mesh[0]->boneweights;
		shaderstate.sourcevbo = &shaderstate.dummyvbo;
		lm = -1;
	}

	shaderstate.curshader = batch->shader;
	shaderstate.flags = batch->flags;
	if (shaderstate.curentity != batch->ent)
	{
		GLBE_SelectEntity(batch->ent);
	}
	if (batch->skin)
		shaderstate.curtexnums = batch->skin;
	else
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

static void GLBE_SubmitMeshesPortals(batch_t **worldlist, batch_t *dynamiclist)
{
	batch_t *batch, *old;
	int i;
	/*attempt to draw portal shaders*/
	if (shaderstate.mode == BEM_STANDARD)
	{
		for (i = 0; i < 2; i++)
		{
			for (batch = i?dynamiclist:worldlist[SHADER_SORT_PORTAL]; batch; batch = batch->next)
			{
				if (batch->meshes == batch->firstmesh)
					continue;

				if (batch->buildmeshes)
					batch->buildmeshes(batch);
				else
					batch->shader = R_TextureAnimation(batch->ent->framestate.g[FS_REG].frame[0], batch->texture)->shader;


				/*draw already-drawn portals as depth-only, to ensure that their contents are not harmed*/
				GLBE_SelectMode(BEM_DEPTHONLY);
				for (old = worldlist[SHADER_SORT_PORTAL]; old && old != batch; old = old->next)
				{
					if (old->meshes == old->firstmesh)
						continue;
					GLBE_SubmitBatch(old);
				}
				if (!old)
				{
					for (old = dynamiclist; old != batch; old = old->next)
					{
						if (old->meshes == old->firstmesh)
							continue;
						GLBE_SubmitBatch(old);
					}
				}
				GLBE_SelectMode(BEM_STANDARD);

				GLR_DrawPortal(batch, worldlist, 0);

				/*clear depth again*/
				GL_ForceDepthWritable();
				qglClear(GL_DEPTH_BUFFER_BIT);
				currententity = &r_worldentity;
				shaderstate.curtime = shaderstate.updatetime - shaderstate.curentity->shaderTime;
			}
		}
	}
}

static void GLBE_SubmitMeshesSortList(batch_t *sortlist)
{
	batch_t *batch;
	for (batch = sortlist; batch; batch = batch->next)
	{
		if (batch->meshes == batch->firstmesh)
			continue;

		if (batch->flags & BEF_NODLIGHT)
			if (shaderstate.mode == BEM_LIGHT || shaderstate.mode == BEM_SMAPLIGHT)
				continue;
		if (batch->flags & BEF_NOSHADOWS)
			if (shaderstate.mode == BEM_STENCIL || shaderstate.mode == BEM_DEPTHONLY)	//fixme: depthonly is not just shadows.
				continue;

		if (batch->buildmeshes)
		{
			TRACE(("GLBE_SubmitMeshesSortList: build\n"));
			batch->buildmeshes(batch);
		}
		else if (batch->texture)
			batch->shader = R_TextureAnimation(batch->ent->framestate.g[FS_REG].frame[0], batch->texture)->shader;

		TRACE(("GLBE_SubmitMeshesSortList: shader %s\n", batch->shader->name));

		if (batch->shader->flags & SHADER_NODRAW)
			continue;
		if (batch->shader->flags & SHADER_NODLIGHT)
			if (shaderstate.mode == BEM_LIGHT || shaderstate.mode == BEM_SMAPLIGHT)
				continue;
		if (batch->shader->flags & SHADER_SKY)
		{
			if (shaderstate.mode == BEM_STANDARD || shaderstate.mode == BEM_DEPTHDARK)
			{
				if (!batch->shader->prog)
				{
					R_DrawSkyChain (batch);
					continue;
				}
			}
			else if (shaderstate.mode != BEM_FOG && shaderstate.mode != BEM_CREPUSCULAR && shaderstate.mode != BEM_WIREFRAME)
				continue;
		}

		if ((batch->shader->flags & (SHADER_HASREFLECT | SHADER_HASREFRACT | SHADER_HASRIPPLEMAP)) && shaderstate.mode != BEM_WIREFRAME)
		{
			int oldbem;
			//these flags require rendering some view as an fbo
			if (r_refdef.recurse)
				continue;
			if (shaderstate.mode != BEM_STANDARD && shaderstate.mode != BEM_DEPTHDARK)
				continue;
			oldbem = shaderstate.mode;

			if (batch->shader->flags & SHADER_HASREFLECT)
			{
				vrect_t orect = r_refdef.vrect;
				if (!shaderstate.tex_reflection.num)
				{
					shaderstate.tex_reflection = GL_AllocNewTexture("***tex_reflection***", vid.pixelwidth/2, vid.pixelheight/2, 0);
					GL_MTBind(0, GL_TEXTURE_2D, shaderstate.tex_reflection);
					qglTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, vid.pixelwidth/2, vid.pixelheight/2, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
					qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
					qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
					qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
					qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
				}
				GL_ForceDepthWritable();
				GLBE_RenderToTexture(r_nulltex, r_nulltex, shaderstate.tex_reflection, true);
				qglViewport (0, 0, vid.pixelwidth/2, vid.pixelheight/2);
				r_refdef.vrect.x = 0;
				r_refdef.vrect.y = 0;
				r_refdef.vrect.width = vid.width/2;
				r_refdef.vrect.height = vid.height/2;
				GL_ForceDepthWritable();
				qglClearColor(0, 0, 0, 0);
				qglClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
				GLR_DrawPortal(batch, cl.worldmodel->batches, 1);
				GLBE_RenderToTexture(r_nulltex, r_nulltex, r_nulltex, false);
				qglViewport (0, 0, vid.pixelwidth, vid.pixelheight);
				r_refdef.vrect = orect;
			}
			if (batch->shader->flags & SHADER_HASREFRACT)
			{
				if (r_refract_fboival)
				{
					vrect_t orect;
					if (!shaderstate.tex_refraction.num)
					{
						shaderstate.tex_refraction = GL_AllocNewTexture("***tex_refraction***", vid.pixelwidth/2, vid.pixelheight/2, 0);
						GL_MTBind(0, GL_TEXTURE_2D, shaderstate.tex_refraction);
						qglTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, vid.pixelwidth/2, vid.pixelheight/2, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
						qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
						qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
						qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
						qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
					}
					GL_ForceDepthWritable();
					GLBE_RenderToTexture(r_nulltex, r_nulltex, shaderstate.tex_refraction, true);

					qglViewport (0, 0, vid.pixelwidth/2, vid.pixelheight/2);
					orect = r_refdef.vrect;
					r_refdef.vrect.x = 0;
					r_refdef.vrect.y = 0;
					r_refdef.vrect.width = vid.width/2;
					r_refdef.vrect.height = vid.height/2;

					GL_ForceDepthWritable();
					qglClearColor(0, 0, 0, 0);
					qglClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
					GLR_DrawPortal(batch, cl.worldmodel->batches, 2);
					GLBE_RenderToTexture(r_nulltex, r_nulltex, r_nulltex, false);

					qglViewport (0, 0, vid.pixelwidth, vid.pixelheight);
					r_refdef.vrect = orect;
				}
				else
					GLR_DrawPortal(batch, cl.worldmodel->batches, 2);
			}
			if (batch->shader->flags & SHADER_HASRIPPLEMAP)
			{
				vrect_t orect;
				if (!shaderstate.tex_ripplemap.num)
				{
					shaderstate.tex_ripplemap = GL_AllocNewTexture("***tex_ripplemap***", vid.pixelwidth/2, vid.pixelheight/2, 0);
					GL_MTBind(0, GL_TEXTURE_2D, shaderstate.tex_ripplemap);
					qglTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F_ARB, vid.pixelwidth/2, vid.pixelheight/2, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
					qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
					qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
					qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
					qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
				}
				GLBE_RenderToTexture(r_nulltex, r_nulltex, shaderstate.tex_ripplemap, false);
				qglViewport (0, 0, vid.pixelwidth/2, vid.pixelheight/2);
				orect = r_refdef.vrect;
				r_refdef.vrect.x = 0;
				r_refdef.vrect.y = 0;
				r_refdef.vrect.width = vid.width/2;
				r_refdef.vrect.height = vid.height/2;

				qglClearColor(0, 0, 0, 0);
				qglClear(GL_COLOR_BUFFER_BIT);

//				r_refdef.waterheight = DotProduct(batch->mesh[0]->xyz_array[0], batch->mesh[0]->normals_array[0]);

				r_refdef.recurse = true; //paranoid, should stop potential infinite loops
				GLBE_SubmitMeshes(true, SHADER_SORT_RIPPLE, SHADER_SORT_RIPPLE);
				r_refdef.recurse = false;
				GLBE_RenderToTexture(r_nulltex, r_nulltex, r_nulltex, false);

				qglViewport (0, 0, vid.pixelwidth, vid.pixelheight);
				r_refdef.vrect = orect;
			}
			BE_SelectMode(oldbem);
		}

		GLBE_SubmitBatch(batch);
	}
}

void GLBE_SubmitMeshes (qboolean drawworld, int start, int stop)
{
	model_t *model = cl.worldmodel;
	int i;

	for (i = start; i <= stop; i++)
	{
		if (drawworld)
		{
			if (i == SHADER_SORT_PORTAL && !r_noportals.ival && !r_refdef.recurse)
				GLBE_SubmitMeshesPortals(model->batches, shaderstate.mbatches[i]);

			GLBE_SubmitMeshesSortList(model->batches[i]);
		}
		GLBE_SubmitMeshesSortList(shaderstate.mbatches[i]);
	}
}

static void BE_UpdateLightmaps(void)
{
	lightmapinfo_t *lm;
	int lmidx;
	int glformat, gltype;
	switch (lightmap_bytes)
	{
	case 4:
		glformat = lightmap_bgra?GL_BGRA_EXT:GL_RGBA;
		gltype = GL_UNSIGNED_INT_8_8_8_8_REV;
		break;
	case 3:
		glformat = lightmap_bgra?GL_BGR_EXT:GL_RGB;
		gltype = GL_UNSIGNED_BYTE;
		break;
	default:
		glformat = GL_LUMINANCE;
		gltype = GL_UNSIGNED_BYTE;
		break;
	}
	for (lmidx = 0; lmidx < numlightmaps; lmidx++)
	{
		lm = lightmap[lmidx];
		if (!lm)
			continue;
		if (lm->modified)
		{
			lm->modified = false;
			if (!TEXVALID(lm->lightmap_texture))
			{
				TEXASSIGN(lm->lightmap_texture, R_AllocNewTexture("***lightmap***", lm->width, lm->height, IF_LINEAR|IF_NOMIPMAP));
				GL_MTBind(0, GL_TEXTURE_2D, lm->lightmap_texture);
				qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
				qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
				qglTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA,
						lm->width, lm->height, 0, glformat, gltype,
						lm->lightmaps);
			}
			else
			{
				GL_MTBind(0, GL_TEXTURE_2D, lm->lightmap_texture);
				qglTexSubImage2D(GL_TEXTURE_2D, 0, 0, lm->rectchange.t,
						lm->width, lm->rectchange.h, glformat, gltype,
						lm->lightmaps+(lm->rectchange.t) *lm->width*lightmap_bytes);
			}
			lm->rectchange.l = LMBLOCK_WIDTH;
			lm->rectchange.t = LMBLOCK_HEIGHT;
			lm->rectchange.h = 0;
			lm->rectchange.w = 0;
		}
	}
}

batch_t *GLBE_GetTempBatch(void)
{
	batch_t *b;
	if (shaderstate.wbatch >= shaderstate.maxwbatches)
	{
		shaderstate.wbatch++;
		return NULL;
	}
	b = &shaderstate.wbatches[shaderstate.wbatch++];
	b->fog = NULL;
	return b;
}

/*called from shadowmapping code*/
#ifdef RTLIGHTS
void GLBE_BaseEntTextures(void)
{
	batch_t *batches[SHADER_SORT_COUNT];
	batch_t **ob = shaderstate.mbatches;
	shaderstate.mbatches = batches;
	BE_GenModelBatches(batches);
	GLBE_SubmitMeshes(false, SHADER_SORT_PORTAL, SHADER_SORT_DECAL);
	GLBE_SelectEntity(&r_worldentity);
	shaderstate.mbatches = ob;
}
#endif

void GLBE_RenderToTexture(texid_t sourcecol, texid_t sourcedepth, texid_t destcol, qboolean usedepth)
{
	shaderstate.tex_sourcecol = sourcecol;
	shaderstate.tex_sourcedepth = sourcedepth;
	if (!destcol.num)
		qglBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
	else
	{
		if (usedepth)
		{
			if (!shaderstate.fbo_diffuse)
			{
				qglGenFramebuffersEXT(1, &shaderstate.fbo_diffuse);
				qglBindFramebufferEXT(GL_FRAMEBUFFER_EXT, shaderstate.fbo_diffuse);

				//create an unnamed depth buffer
				qglGenRenderbuffersEXT(1, &shaderstate.rb_depth);
				qglBindRenderbufferEXT(GL_RENDERBUFFER_EXT, shaderstate.rb_depth);
				if (gl_config.ext_packed_depth_stencil)
				{
					qglRenderbufferStorageEXT(GL_RENDERBUFFER_EXT, GL_DEPTH24_STENCIL8_EXT, vid.pixelwidth/2, vid.pixelheight/2);
					qglFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT, GL_STENCIL_ATTACHMENT_EXT, GL_RENDERBUFFER_EXT, shaderstate.rb_depth);
				}
				else
				{
					qglGenRenderbuffersEXT(1, &shaderstate.rb_stencil);
					qglRenderbufferStorageEXT(GL_RENDERBUFFER_EXT, GL_STENCIL_INDEX8_EXT, vid.pixelwidth/2, vid.pixelheight/2);
					qglFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT, GL_STENCIL_ATTACHMENT_EXT, GL_RENDERBUFFER_EXT, shaderstate.rb_stencil);
					
					qglRenderbufferStorageEXT(GL_RENDERBUFFER_EXT, GL_DEPTH_COMPONENT24_ARB, vid.pixelwidth/2, vid.pixelheight/2);
				}
				qglFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT, GL_DEPTH_ATTACHMENT_EXT, GL_RENDERBUFFER_EXT, shaderstate.rb_depth);

				qglDrawBuffer(GL_COLOR_ATTACHMENT0_EXT);
				qglReadBuffer(GL_NONE);
			}
			else
				qglBindFramebufferEXT(GL_FRAMEBUFFER_EXT, shaderstate.fbo_diffuse);
		}
		else
		{
			if (!shaderstate.fbo_depthless)
			{
				qglGenFramebuffersEXT(1, &shaderstate.fbo_depthless);
				qglBindFramebufferEXT(GL_FRAMEBUFFER_EXT, shaderstate.fbo_depthless);

				qglDrawBuffer(GL_COLOR_ATTACHMENT0_EXT);
				qglReadBuffer(GL_NONE);
			}
			else
				qglBindFramebufferEXT(GL_FRAMEBUFFER_EXT, shaderstate.fbo_depthless);
		}
		
		qglFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D, destcol.num, 0);
	}
}

void GLBE_DrawLightPrePass(qbyte *vis)
{
	if (!shaderstate.initeddepthnorm)
	{
		shaderstate.initeddepthnorm = true;
		shaderstate.depthnormshader = R_RegisterShader("lpp_depthnorm", 
					"{\n"
						"program lpp_depthnorm\n"
						"{\n"
							"map $normalmap\n"
							"tcgen base\n"
						"}\n"
					"}\n"
				);
	}
	if (!shaderstate.depthnormshader)
	{
		Con_Printf("%s requires content support\n", r_lightprepass.name);
		r_lightprepass.ival = 0;
		return;
	}
	/*do portals*/
	BE_SelectMode(BEM_STANDARD);
	GLBE_SubmitMeshes(true, SHADER_SORT_PORTAL, SHADER_SORT_PORTAL);

	BE_SelectMode(BEM_DEPTHNORM);
	if (!shaderstate.depthnormshader)
	{
		BE_SelectMode(BEM_STANDARD);
		return;
	}

	if (!TEXVALID(shaderstate.tex_normals))
	{
		shaderstate.tex_normals = GL_AllocNewTexture("***prepass normals***", vid.pixelwidth, vid.pixelheight, 0);
		r_lightprepass.modified = true;
	}
	if (r_lightprepass.modified)
	{
		GL_MTBind(0, GL_TEXTURE_2D, shaderstate.tex_normals);
		qglTexImage2D(GL_TEXTURE_2D, 0, (r_lightprepass.ival==2)?GL_RGBA32F_ARB:GL_RGBA16F_ARB, vid.pixelwidth, vid.pixelheight, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
		qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		r_lightprepass.modified = false;
	}

	if (!TEXVALID(shaderstate.tex_diffuse))
	{
		int drb;

		shaderstate.tex_diffuse = GL_AllocNewTexture("***prepass diffuse***", vid.pixelwidth, vid.pixelheight, 0);
		GL_MTBind(0, GL_TEXTURE_2D, shaderstate.tex_diffuse);
		qglTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, vid.pixelwidth, vid.pixelheight, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
		qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

		GL_MTBind(0, GL_TEXTURE_2D, shaderstate.tex_normals);
		qglTexImage2D(GL_TEXTURE_2D, 0, (r_lightprepass.ival==2)?GL_RGBA32F_ARB:GL_RGBA16F_ARB, vid.pixelwidth, vid.pixelheight, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
		r_lightprepass.modified = false;
		qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);


		qglGenFramebuffersEXT(1, &shaderstate.fbo_diffuse);
		qglBindFramebufferEXT(GL_FRAMEBUFFER_EXT, shaderstate.fbo_diffuse);

		qglGenRenderbuffersEXT(1, &drb);
		qglBindRenderbufferEXT(GL_RENDERBUFFER_EXT, drb);
		qglRenderbufferStorageEXT(GL_RENDERBUFFER_EXT, GL_DEPTH_COMPONENT24_ARB, vid.pixelwidth, vid.pixelheight);
		qglFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT, GL_DEPTH_ATTACHMENT_EXT, GL_RENDERBUFFER_EXT, drb);


		qglDrawBuffer(GL_COLOR_ATTACHMENT0_EXT);
//		qglReadBuffer(GL_NONE);
	}
	else
		qglBindFramebufferEXT(GL_FRAMEBUFFER_EXT, shaderstate.fbo_diffuse);

	/*set the FB up to draw surface info*/
	qglFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D, shaderstate.tex_normals.num, 0);
	GL_ForceDepthWritable();
	qglClear(GL_DEPTH_BUFFER_BIT);

	if (GL_FRAMEBUFFER_COMPLETE_EXT != qglCheckFramebufferStatusEXT(GL_FRAMEBUFFER_EXT))
	{
		Con_Printf("Bad framebuffer\n");
		return;
	}

	/*draw surfaces that can be drawn this way*/
	GLBE_SubmitMeshes(true, SHADER_SORT_OPAQUE, SHADER_SORT_OPAQUE);

	/*reconfigure - now drawing diffuse light info using the previous fb image as a source image*/
	shaderstate.tex_sourcecol = shaderstate.tex_normals;
	qglFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D, shaderstate.tex_diffuse.num, 0);

	BE_SelectMode(BEM_STANDARD);
	qglClearColor (0,0,0,0);
	qglClear(GL_COLOR_BUFFER_BIT);

	GLBE_SelectEntity(&r_worldentity);
	/*now draw the prelights*/
	GLBE_SubmitMeshes(true, SHADER_SORT_PRELIGHT, SHADER_SORT_PRELIGHT);

	/*final reconfigure - now drawing final surface data onto true framebuffer*/
	qglBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
	shaderstate.tex_sourcecol = shaderstate.tex_diffuse;
	qglDrawBuffer(GL_BACK);

	/*now draw the postlight passes (this includes blended stuff which will NOT be lit)*/
	GLBE_SelectEntity(&r_worldentity);
	GLBE_SubmitMeshes(true, SHADER_SORT_SKY, SHADER_SORT_NEAREST);

#ifdef RTLIGHTS
	/*regular lighting now*/
	GLBE_SelectEntity(&r_worldentity);
	Sh_DrawLights(vis);
#endif

	shaderstate.tex_sourcecol = r_nulltex;
	shaderstate.tex_sourcedepth = r_nulltex;

	qglClearColor (1,0,0,1);
}

void GLBE_DrawWorld (qboolean drawworld, qbyte *vis)
{
	extern cvar_t r_shadow_realtime_world, r_shadow_realtime_world_lightmaps;
	batch_t *batches[SHADER_SORT_COUNT];
	batch_t **ob = shaderstate.mbatches;
	RSpeedLocals();
	shaderstate.mbatches = batches;

	shaderstate.depthrange = 0;

	TRACE(("GLBE_DrawWorld: %i %p\n", drawworld, vis));

	if (!r_refdef.recurse)
	{
		GL_DoSwap();
		if (shaderstate.wbatch + 50 > shaderstate.maxwbatches)
		{
			int newm = shaderstate.wbatch + 100;
			shaderstate.wbatches = BZ_Realloc(shaderstate.wbatches, newm * sizeof(*shaderstate.wbatches));
			memset(shaderstate.wbatches + shaderstate.maxwbatches, 0, (newm - shaderstate.maxwbatches) * sizeof(*shaderstate.wbatches));
			shaderstate.maxwbatches = newm;
		}

		shaderstate.wbatch = 0;
	}
	if (shaderstate.oldwidth != vid.pixelwidth || shaderstate.oldheight != vid.pixelheight)
	{
		if (shaderstate.tex_reflection.num)
		{
			R_DestroyTexture(shaderstate.tex_reflection);
			shaderstate.tex_reflection = r_nulltex;
		}
		if (shaderstate.tex_refraction.num)
		{
			R_DestroyTexture(shaderstate.tex_refraction);
			shaderstate.tex_refraction = r_nulltex;
		}
		if (shaderstate.temptexture.num)
		{
			R_DestroyTexture(shaderstate.temptexture);
			shaderstate.temptexture = r_nulltex;
		}
		if (shaderstate.fbo_diffuse)
		{
			qglDeleteFramebuffersEXT(1, &shaderstate.fbo_diffuse);
			shaderstate.fbo_diffuse = 0;
		}
		if (shaderstate.rb_depth)
			qglDeleteRenderbuffersEXT(1, &shaderstate.rb_depth);
		shaderstate.rb_depth = 0;
		if (shaderstate.rb_stencil)
			qglDeleteRenderbuffersEXT(1, &shaderstate.rb_stencil);
		shaderstate.rb_stencil = 0;
		shaderstate.oldwidth = vid.pixelwidth;
		shaderstate.oldheight = vid.pixelheight;

		while(shaderstate.lastpasstmus>0)
		{
			GL_LazyBind(--shaderstate.lastpasstmus, 0, r_nulltex);
		}
#ifdef RTLIGHTS
		Sh_Reset();
#endif
	}
	BE_GenModelBatches(batches);
	R_GenDlightBatches(batches);
	shaderstate.curentity = &r_worldentity;
	if (cl.paused || cls.state < ca_active)
		shaderstate.updatetime = r_refdef.time;
	else
		shaderstate.updatetime = cl.servertime;

	GLBE_SelectEntity(&r_worldentity);

	BE_UpdateLightmaps();
	if (drawworld)
	{
		if (gl_overbright.modified)
		{
			int i;
			gl_overbright.modified = false;
			if (gl_overbright.ival > 2)
				gl_overbright.ival = 2;

			for (i = 0; i < SHADER_TMU_MAX; i++)
				shaderstate.blendmode[i] = -1;
		}

#ifdef RTLIGHTS
		if (vis && r_shadow_realtime_world.ival)
			shaderstate.identitylighting = r_shadow_realtime_world_lightmaps.value;
		else
#endif
			shaderstate.identitylighting = 1;
//		shaderstate.identitylighting /= 1<<gl_overbright.ival;

#ifdef RTLIGHTS
		if (r_lightprepass.ival)
		{
			GLBE_DrawLightPrePass(vis);
		}
		else
#endif
		{
			if (shaderstate.identitylighting == 0)
				BE_SelectMode(BEM_DEPTHDARK);
			else
				BE_SelectMode(BEM_STANDARD);

			RSpeedRemark();
			GLBE_SubmitMeshes(true, SHADER_SORT_PORTAL, SHADER_SORT_DECAL);
			RSpeedEnd(RSPEED_WORLD);
		}

#ifdef RTLIGHTS
		if (drawworld)
		{
			RSpeedRemark();
			TRACE(("GLBE_DrawWorld: drawing lights\n"));
			GLBE_SelectEntity(&r_worldentity);
			Sh_DrawLights(vis);
			RSpeedEnd(RSPEED_STENCILSHADOWS);
			TRACE(("GLBE_DrawWorld: lights drawn\n"));
		}
#endif

		shaderstate.identitylighting = 1;

		GLBE_SubmitMeshes(true, SHADER_SORT_DECAL, SHADER_SORT_NEAREST);

/*		if (r_refdef.gfog_alpha)
		{
			BE_SelectMode(BEM_FOG);
			BE_SelectFog(r_refdef.gfog_rgb, r_refdef.gfog_alpha, r_refdef.gfog_density);
			GLBE_SubmitMeshes(true, batches, SHADER_SORT_PORTAL, SHADER_SORT_NEAREST);
		}
*/

#ifdef GL_LINE	//no gles
		if (r_wireframe.ival && qglPolygonMode)
		{
			BE_SelectMode(BEM_WIREFRAME);
			qglPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
			GLBE_SubmitMeshes(true, SHADER_SORT_PORTAL, SHADER_SORT_NEAREST);
			BE_SelectMode(BEM_STANDARD);
			qglPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
		}
#endif
	}
	else
	{
		GLBE_SubmitMeshes(false, SHADER_SORT_PORTAL, SHADER_SORT_NEAREST);

#ifdef GL_LINE	//no gles
		if (r_wireframe.ival && qglPolygonMode)
		{
			BE_SelectMode(BEM_WIREFRAME);
			qglPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
			GLBE_SubmitMeshes(false, SHADER_SORT_PORTAL, SHADER_SORT_NEAREST);
			BE_SelectMode(BEM_STANDARD);
			qglPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
		}
#endif
	}

	GLBE_SelectEntity(&r_worldentity);
	shaderstate.curtime = shaderstate.updatetime = realtime;

	shaderstate.identitylighting = 1;

	shaderstate.mbatches = ob;

	TRACE(("GLBE_DrawWorld: drawn everything\n"));
}
#endif
