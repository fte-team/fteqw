/*
Copyright (C) 2002-2003 Victor Luchits

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
// r_shader.c - based on code by Stephen C. Taylor
// Ported to FTE from qfusion, there are numerous changes since then.


#include "quakedef.h"
#ifndef SERVERONLY
#include "glquake.h"
#include "shader.h"

#include "hash.h"


#include <ctype.h>

#ifdef D3D9QUAKE
#include <d3d9.h>
extern LPDIRECT3DDEVICE9 pD3DDev9;
#endif

extern texid_t missing_texture;
texid_t r_whiteimage;
static qboolean shader_reload_needed;
static qboolean shader_rescan_needed;
static char **saveshaderbody;

sh_config_t sh_config;

//cvars that affect shader generation
cvar_t r_vertexlight = CVARFD("r_vertexlight", "0", CVAR_SHADERSYSTEM, "Hack loaded shaders to remove detail pass and lightmap sampling for faster rendering.");
extern cvar_t r_glsl_offsetmapping_reliefmapping;
extern cvar_t r_deluxemapping;
extern cvar_t r_fastturb, r_fastsky, r_skyboxname;
extern cvar_t r_drawflat;
extern cvar_t r_shaderblobs;

//backend fills this in to say the max pass count
int be_maxpasses;


#define Q_stricmp stricmp
#define Q_strnicmp strnicmp
#define clamp(v,min, max) (v) = (((v)<(min))?(min):(((v)>(max))?(max):(v)));

typedef union {
	float			f;
	unsigned int	i;
} float_int_t;
qbyte FloatToByte( float x )
{
	static float_int_t f2i;

	// shift float to have 8bit fraction at base of number
	f2i.f = x + 32768.0f;

	// then read as integer and kill float bits...
	return (qbyte) min(f2i.i & 0x7FFFFF, 255);
}



cvar_t r_detailtextures;

#define MAX_TOKEN_CHARS sizeof(com_token)

char *COM_ParseExt (char **data_p, qboolean nl, qboolean comma)
{
	int		c;
	int		len;
	char	*data;
	qboolean newlines = false;

	data = *data_p;
	len = 0;
	com_token[0] = 0;

	if (!data)
	{
		*data_p = NULL;
		return "";
	}

// skip whitespace
skipwhite:
	while ((c = *data) <= ' ')
	{
		if (c == 0)
		{
			*data_p = NULL;
			return "";
		}
		if (c == '\n')
			newlines = true;
		data++;
	}

	if (newlines && !nl)
	{
		*data_p = data;
		return com_token;
	}

// skip // comments
	if (c == '/' && data[1] == '/')
	{
		while (*data && *data != '\n')
			data++;
		goto skipwhite;
	}

// handle quoted strings specially
	if (c == '\"')
	{
		data++;
		while (1)
		{
			c = *data++;
			if (c=='\"' || !c)
			{
				com_token[len] = 0;
				*data_p = data;
				return com_token;
			}
			if (len < MAX_TOKEN_CHARS)
			{
				com_token[len] = c;
				len++;
			}
		}
	}

// parse a regular word
	do
	{
		if (len < MAX_TOKEN_CHARS)
		{
			com_token[len] = c;
			len++;
		}
		data++;
		c = *data;
		if (c == ',' && len && comma)
			break;
	} while (c>32);

	if (len == MAX_TOKEN_CHARS)
	{
		Con_DPrintf ("Token exceeded %i chars, discarded.\n", (int)MAX_TOKEN_CHARS);
		len = 0;
	}
	com_token[len] = 0;

	*data_p = data;
	return com_token;
}

static float Shader_FloatArgument(shader_t *shader, char *arg)
{
	char *var;
	int arglen = strlen(arg);

	//grab an argument instead, otherwise 0
	var = shader->name;
	while((var = strchr(var, '#')))
	{
		if (!strnicmp(var, arg, arglen))
		{
			if (var[arglen] == '=')
				return strtod(var+arglen+1, NULL);
			if (var[arglen] == '#' || !var[arglen])
				return 1;	//present, but no value
		}
		var++;
	}
	return 0;	//not present.
}





#define HASH_SIZE	128

enum shaderparsemode_e
{
	SPM_DEFAULT, /*quake3/fte internal*/
	SPM_DOOM3,
};

static struct
{
	enum shaderparsemode_e mode;
} parsestate;

typedef struct shaderkey_s
{
    char			*keyword;
    void			(*func)( shader_t *shader, shaderpass_t *pass, char **ptr );
} shaderkey_t;
typedef struct shadercachefile_s {
	char *data;
	size_t length;
	enum shaderparsemode_e parsemode;
	struct shadercachefile_s *next;
	char name[1];
} shadercachefile_t;
typedef struct shadercache_s {
	shadercachefile_t *source;
	size_t offset;
	struct shadercache_s *hash_next;
	char name[1];
} shadercache_t;

static shadercachefile_t *shaderfiles;	//contents of a .shader file
static shadercache_t **shader_hash;		//locations of known inactive shaders.

unsigned int r_numshaders;	//number of active slots in r_shaders array.
unsigned int r_maxshaders;	//max length of r_shaders array. resized if exceeded.
shader_t	**r_shaders;	//list of active shaders for a id->shader lookup
static hashtable_t shader_active_hash;	//list of active shaders for a name->shader lookup
void *shader_active_hash_mem;

//static char		r_skyboxname[MAX_QPATH];
//static float	r_skyheight;

char *Shader_Skip( char *ptr );
static qboolean Shader_Parsetok(shader_t *shader, shaderpass_t *pass, shaderkey_t *keys, char *token, char **ptr);
static void Shader_ParseFunc(shader_t *shader, char **args, shaderfunc_t *func);
static void Shader_MakeCache(const char *path);
static qboolean Shader_LocateSource(char *name, char **buf, size_t *bufsize, size_t *offset, enum shaderparsemode_e *parsemode);
static void Shader_ReadShader(shader_t *s, char *shadersource, int parsemode);
static qboolean Shader_ParseShader(char *parsename, shader_t *s);

//===========================================================================

static qboolean Shader_EvaluateCondition(shader_t *shader, char **ptr)
{
	char *token;
	cvar_t *cv;
	qboolean conditiontrue = true;
	token = COM_ParseExt(ptr, false, false);
	if (*token == '!')
	{
		conditiontrue = false;
		token++;
	}
	if (*token == '$')
	{
		token++;
		if (*token == '#')
			conditiontrue = conditiontrue == !!Shader_FloatArgument(shader, token);
		else if (!Q_stricmp(token, "lpp"))
			conditiontrue = conditiontrue == r_lightprepass.ival;
		else if (!Q_stricmp(token, "lightmap"))
			conditiontrue = conditiontrue == !r_fullbright.value;
		else if (!Q_stricmp(token, "deluxmap"))
			conditiontrue = conditiontrue == r_deluxemapping.ival;

		//normalmaps are generated if they're not already known.
		else if (!Q_stricmp(token, "normalmap"))
			conditiontrue = conditiontrue == r_loadbumpmapping;

		else if (!Q_stricmp(token, "opengl"))
			conditiontrue = conditiontrue == (qrenderer == QR_OPENGL);
		else if (!Q_stricmp(token, "d3d9"))
			conditiontrue = conditiontrue == (qrenderer == QR_DIRECT3D9);
		else if (!Q_stricmp(token, "d3d11"))
			conditiontrue = conditiontrue == (qrenderer == QR_DIRECT3D11);
		else if (!Q_stricmp(token, "gles"))
			conditiontrue = conditiontrue == ((qrenderer == QR_OPENGL) && sh_config.minver == 100);
		else if (!Q_stricmp(token, "nofixed"))
			conditiontrue = conditiontrue == sh_config.progs_required;
		else if (!Q_stricmp(token, "glsl"))
			conditiontrue = conditiontrue == ((qrenderer == QR_OPENGL) && sh_config.progs_supported);
		else if (!Q_stricmp(token, "hlsl"))
			conditiontrue = conditiontrue == ((qrenderer == QR_DIRECT3D9 || qrenderer == QR_DIRECT3D11) && sh_config.progs_supported);	
		else if (!Q_stricmp(token, "haveprogram"))
			conditiontrue = conditiontrue == !!shader->prog;
		else if (!Q_stricmp(token, "programs"))
			conditiontrue = conditiontrue == sh_config.progs_supported;
		else if (!Q_stricmp(token, "diffuse"))
			conditiontrue = conditiontrue == true;
		else if (!Q_stricmp(token, "specular"))
			conditiontrue = conditiontrue == false;
		else if (!Q_stricmp(token, "fullbright"))
			conditiontrue = conditiontrue == false;
		else if (!Q_stricmp(token, "topoverlay"))
			conditiontrue = conditiontrue == false;
		else if (!Q_stricmp(token, "loweroverlay"))
			conditiontrue = conditiontrue == false;
		else
		{
			Con_Printf("Unrecognised builtin shader condition '%s'\n", token);
			conditiontrue = conditiontrue == false;
		}
	}
	else
	{
		float lhs;
		if (*token >= '0' && *token <= '9')
			lhs = strtod(token, NULL);
		else
		{
			cv = Cvar_Get(token, "", 0, "Shader Conditions");
			if (cv)
			{
				cv->flags |= CVAR_SHADERSYSTEM;
				lhs = cv->value;
			}
			else
			{
				Con_Printf("Shader_EvaluateCondition: '%s' is not a cvar\n", token);
				return conditiontrue;
			}
		}
		if (*token)
			token = COM_ParseExt(ptr, false, false);
		if (*token)
		{
			float rhs;
			char cmp[4];
			memcpy(cmp, token, 4);
			token = COM_ParseExt(ptr, false, false);
			rhs = atof(token);
			if (!strcmp(cmp, "!="))
				conditiontrue = lhs != rhs;
			else if (!strcmp(cmp, "=="))
				conditiontrue = lhs == rhs;
			else if (!strcmp(cmp, "<"))
				conditiontrue = lhs < rhs;
			else if (!strcmp(cmp, "<="))
				conditiontrue = lhs <= rhs;
			else if (!strcmp(cmp, ">"))
				conditiontrue = lhs > rhs;
			else if (!strcmp(cmp, ">="))
				conditiontrue = lhs >= rhs;
			else
				conditiontrue = false;
		}
		else
		{
			conditiontrue = conditiontrue == !!lhs;
		}
	}
	if (*token)
		token = COM_ParseExt(ptr, false, false);
	if (!strcmp(token, "&&"))
		return Shader_EvaluateCondition(shader, ptr) && conditiontrue;
	if (!strcmp(token, "||"))
		return Shader_EvaluateCondition(shader, ptr) || conditiontrue;

	return conditiontrue;
}

static char *Shader_ParseExactString(char **ptr)
{
	char *token;

	if (!ptr || !(*ptr))
		return "";
	if (!**ptr || **ptr == '}')
		return "";

	token = COM_ParseExt(ptr, false, false);
	return token;
}

static char *Shader_ParseString(char **ptr)
{
	char *token;

	if (!ptr || !(*ptr))
		return "";
	if (!**ptr || **ptr == '}')
		return "";

	token = COM_ParseExt(ptr, false, true);
	Q_strlwr ( token );

	return token;
}

static char *Shader_ParseSensString(char **ptr)
{
	char *token;

	if (!ptr || !(*ptr))
		return "";
	if (!**ptr || **ptr == '}')
		return "";

	token = COM_ParseExt(ptr, false, true);

	return token;
}

static float Shader_ParseFloat(shader_t *shader, char **ptr)
{
	char *token;
	if (!ptr || !(*ptr))
		return 0;
	if (!**ptr || **ptr == '}')
		return 0;

	token = COM_ParseExt(ptr, false, true);
	if (*token == '$')
	{
		if (token[1] == '#')
		{
			return Shader_FloatArgument(shader, token+1);
		}
		else
		{
			cvar_t *var;
			var = Cvar_FindVar(token+1);
			if (var)
				return var->value;
		}
	}
	return atof(token);
}

static void Shader_ParseVector(shader_t *shader, char **ptr, vec3_t v)
{
	char *scratch;
	char *token;
	qboolean bracket;
	qboolean fromcvar = false;

	token = Shader_ParseString(ptr);
	if (*token == '$')
	{
		cvar_t *var;
		var = Cvar_FindVar(token+1);
		if (!var)
		{
			v[0] = 1;
			v[1] = 1;
			v[2] = 1;
			return;
		}
		var->flags |= CVAR_SHADERSYSTEM;
		ptr = &scratch;
		scratch = var->string;

		token = Shader_ParseString( ptr);
		fromcvar = true;
	}
	if (!Q_stricmp (token, "("))
	{
		bracket = true;
		token = Shader_ParseString(ptr);
	}
	else if (token[0] == '(')
	{
		bracket = true;
		token = &token[1];
	}
	else
		bracket = false;

	v[0] = atof ( token );
	
	token = Shader_ParseString ( ptr );
	if ( !token[0] ) {
		v[1] = fromcvar?v[0]:0;
	} else if (bracket &&  token[strlen(token)-1] == ')' ) {
		bracket = false;
		token[strlen(token)-1] = 0;
		v[1] = atof ( token );
	} else {
		v[1] = atof ( token );
	}

	token = Shader_ParseString ( ptr );
	if ( !token[0] ) {
		v[2] = fromcvar?v[1]:0;
	} else if (bracket && token[strlen(token)-1] == ')' ) {
		token[strlen(token)-1] = 0;
		v[2] = atof ( token );
	} else {
		v[2] = atof ( token );
		if ( bracket ) {
			Shader_ParseString ( ptr );
		}
	}

	if (v[0] > 5 || v[1] > 5 || v[2] > 5)
	{
		VectorScale(v, 1.0f/255, v);
	}
}

qboolean Shader_ParseSkySides (char *shadername, char *texturename, texid_t *images)
{
	qboolean allokay = true;
	int i, ss, sp;
	char path[MAX_QPATH];

	static char	*skyname_suffix[][6] = {
		{"rt", "bk", "lf", "ft", "up", "dn"},
		{"px", "py", "nx", "ny", "pz", "nz"},
		{"posx", "posy", "negx", "negy", "posz", "negz"},
		{"_px", "_py", "_nx", "_ny", "_pz", "_nz"},
		{"_posx", "_posy", "_negx", "_negy", "_posz", "_negz"},
		{"_rt", "_bk", "_lf", "_ft", "_up", "_dn"}
	};

	static char *skyname_pattern[] = {
		"%s_%s",
		"%s%s",
		"env/%s%s",
		"gfx/env/%s%s"
	};

	if (*texturename == '$')
	{
		cvar_t *v;
		v = Cvar_FindVar(texturename+1);
		if (v)
			texturename = v->string;
	}
	if (!*texturename)
		texturename = "-";

	for ( i = 0; i < 6; i++ )
	{
		if ( texturename[0] == '-' )
		{
			images[i] = r_nulltex;
		}
		else
		{
			for (sp = 0; sp < sizeof(skyname_pattern)/sizeof(skyname_pattern[0]); sp++)
			{
				for (ss = 0; ss < sizeof(skyname_suffix)/sizeof(skyname_suffix[0]); ss++)
				{
					Q_snprintfz ( path, sizeof(path), skyname_pattern[sp], texturename, skyname_suffix[ss][i] );
					images[i] = R_LoadHiResTexture ( path, NULL, IF_NOALPHA|IF_CLAMP|IF_NOWORKER);
					if (images[i]->status == TEX_LOADING)	//FIXME: unsafe, as it can recurse through shader loading and mess up internal parse state.
						COM_WorkerPartialSync(images[i], &images[i]->status, TEX_LOADING);
					if (TEXLOADED(images[i]))
						break;
				}
				if (TEXLOADED(images[i]))
					break;
			}
			if (!TEXVALID(images[i]))
			{
				Con_Printf("Sky \"%s\" missing texture: %s\n", shadername, path);
				images[i] = missing_texture;
				allokay = false;
			}
		}
	}
	return allokay;
}

static void Shader_ParseFunc (shader_t *shader, char **ptr, shaderfunc_t *func)
{
	char *token;

	token = Shader_ParseString (ptr);
	if (!Q_stricmp (token, "sin"))
	    func->type = SHADER_FUNC_SIN;
	else if (!Q_stricmp (token, "triangle"))
	    func->type = SHADER_FUNC_TRIANGLE;
	else if (!Q_stricmp (token, "square"))
	    func->type = SHADER_FUNC_SQUARE;
	else if (!Q_stricmp (token, "sawtooth"))
	    func->type = SHADER_FUNC_SAWTOOTH;
	else if (!Q_stricmp (token, "inversesawtooth"))
	    func->type = SHADER_FUNC_INVERSESAWTOOTH;
	else if (!Q_stricmp (token, "noise"))
	    func->type = SHADER_FUNC_NOISE;

	func->args[0] = Shader_ParseFloat (shader, ptr);
	func->args[1] = Shader_ParseFloat (shader, ptr);
	func->args[2] = Shader_ParseFloat (shader, ptr);
	func->args[3] = Shader_ParseFloat (shader, ptr);
}

//===========================================================================

static int Shader_SetImageFlags(shader_t *shader, shaderpass_t *pass, char **name)
{
	int flags = 0;

	while (name)
	{
		if (!Q_strnicmp(*name, "$rt:", 4))
		{
			*name += 4;
			flags |= IF_NOMIPMAP|IF_CLAMP|IF_LINEAR|IF_RENDERTARGET;
		}
		else if (!Q_strnicmp(*name, "$clamp:", 7))
		{
			*name += 7;
			flags |= IF_CLAMP;
		}
		else if (!Q_strnicmp(*name, "$3d:", 4))
		{
			*name+=4;
			flags = (flags&~IF_TEXTYPE) | IF_3DMAP;
		}
		else if (!Q_strnicmp(*name, "$cube:", 6))
		{
			*name+=6;
			flags = (flags&~IF_TEXTYPE) | IF_CUBEMAP;
		}
		else if (!Q_strnicmp(*name, "$nearest:", 9))
		{
			*name+=9;
			flags|= IF_NEAREST;
			pass->flags |= SHADER_PASS_NEAREST;
		}
		else if (!Q_strnicmp(*name, "$linear:", 8))
		{
			*name+=8;
			flags|= IF_LINEAR;
			pass->flags |= SHADER_PASS_LINEAR;
		}
		else
			name = NULL;
	}

//	if (shader->flags & SHADER_SKY)
//		flags |= IF_SKY;
	if (shader->flags & SHADER_NOMIPMAPS)
		flags |= IF_NOMIPMAP;
	if (shader->flags & SHADER_NOPICMIP)
		flags |= IF_NOPICMIP;

	return flags;
}

static texid_t Shader_FindImage ( char *name, int flags )
{
	if (parsestate.mode == SPM_DOOM3)
	{
		if (!Q_stricmp (name, "_default"))
			return r_whiteimage; /*fixme*/
		if (!Q_stricmp (name, "_white"))
			return r_whiteimage;
		if (!Q_stricmp (name, "_black"))
		{
			int wibuf[16] = {0};
			return R_LoadTexture("$blackimage", 4, 4, TF_RGBA32, wibuf, IF_NOMIPMAP|IF_NOPICMIP|IF_NEAREST|IF_NOGAMMA);
		}
	}
	else
	{
		if (!Q_stricmp (name, "$whiteimage"))
			return r_whiteimage;
	}
	if (flags & IF_RENDERTARGET)
		return R2D_RT_Configure(name, 0, 0, TF_INVALID);
	return R_LoadHiResTexture(name, NULL, flags);
}


/****************** shader keyword functions ************************/

static void Shader_Cull ( shader_t *shader, shaderpass_t *pass, char **ptr )
{
	char *token;

	shader->flags &= ~(SHADER_CULL_FRONT|SHADER_CULL_BACK);

	token = Shader_ParseString ( ptr );
	if ( !Q_stricmp (token, "disable") || !Q_stricmp (token, "none") || !Q_stricmp (token, "twosided") ) {
	} else if ( !Q_stricmp (token, "front") ) {
		shader->flags |= SHADER_CULL_FRONT;
	} else if ( !Q_stricmp (token, "back") || !Q_stricmp (token, "backside") || !Q_stricmp (token, "backsided") ) {
		shader->flags |= SHADER_CULL_BACK;
	} else {
		shader->flags |= SHADER_CULL_FRONT;
	}
}

static void Shader_NoMipMaps ( shader_t *shader, shaderpass_t *pass, char **ptr )
{
	shader->flags |= (SHADER_NOMIPMAPS|SHADER_NOPICMIP);
}

static void Shader_NoPicMip ( shader_t *shader, shaderpass_t *pass, char **ptr )
{
	shader->flags |= SHADER_NOPICMIP;
}

static void Shader_DeformVertexes ( shader_t *shader, shaderpass_t *pass, char **ptr )
{
	char *token;
	deformv_t *deformv;

	if ( shader->numdeforms >= SHADER_DEFORM_MAX )
		return;

	deformv = &shader->deforms[shader->numdeforms];

	token = Shader_ParseString ( ptr );
	if ( !Q_stricmp (token, "wave") )
	{
		deformv->type = DEFORMV_WAVE;
		deformv->args[0] = Shader_ParseFloat (shader, ptr);
		if (deformv->args[0])
			deformv->args[0] = 1.0f / deformv->args[0];
		Shader_ParseFunc (shader, ptr, &deformv->func );
	}
	else if ( !Q_stricmp (token, "normal") )
	{
		deformv->type = DEFORMV_NORMAL;
		deformv->args[0] = Shader_ParseFloat (shader, ptr );
		deformv->args[1] = Shader_ParseFloat (shader, ptr );
	}
	else if ( !Q_stricmp (token, "bulge") )
	{
		deformv->type = DEFORMV_BULGE;
		Shader_ParseVector (shader, ptr, deformv->args );
		shader->flags |= SHADER_DEFORMV_BULGE;
	}
	else if ( !Q_stricmp (token, "move") )
	{
		deformv->type = DEFORMV_MOVE;
		Shader_ParseVector (shader, ptr, deformv->args );
		Shader_ParseFunc (shader, ptr, &deformv->func );
	}
	else if ( !Q_stricmp (token, "autosprite") )
	{
		deformv->type = DEFORMV_AUTOSPRITE;
		shader->flags |= SHADER_AUTOSPRITE;
	}
	else if ( !Q_stricmp (token, "autosprite2") )
	{
		deformv->type = DEFORMV_AUTOSPRITE2;
		shader->flags |= SHADER_AUTOSPRITE;
	}
	else if ( !Q_stricmp (token, "projectionShadow") )
		deformv->type = DEFORMV_PROJECTION_SHADOW;
	else
		return;

	shader->numdeforms++;
}


static void Shader_SkyParms(shader_t *shader, shaderpass_t *pass, char **ptr)
{
	skydome_t *skydome;
	float skyheight;
	char *boxname;

	if (shader->skydome)
	{
		Z_Free(shader->skydome);
	}

	skydome = (skydome_t *)Z_Malloc(sizeof(skydome_t));
	shader->skydome = skydome;

	boxname = Shader_ParseString(ptr);
	Shader_ParseSkySides(shader->name, boxname, skydome->farbox_textures);

	skyheight = Shader_ParseFloat(shader, ptr);
	if (!skyheight)
	{
		skyheight = 512.0f;
	}

	boxname = Shader_ParseString(ptr);
	Shader_ParseSkySides(shader->name, boxname, skydome->nearbox_textures);

	shader->flags |= SHADER_SKY;
	shader->sort = SHADER_SORT_SKY;
}

static void Shader_FogParms ( shader_t *shader, shaderpass_t *pass, char **ptr )
{
	float div;
	vec3_t color, fcolor;

//	if ( !r_ignorehwgamma->value )
//		div = 1.0f / pow(2, max(0, floor(r_overbrightbits->value)));
//	else
		div = 1.0f;

	Shader_ParseVector (shader, ptr, color );
	VectorScale ( color, div, color );
	ColorNormalize ( color, fcolor );

	shader->fog_color[0] = FloatToByte ( fcolor[0] );
	shader->fog_color[1] = FloatToByte ( fcolor[1] );
	shader->fog_color[2] = FloatToByte ( fcolor[2] );
	shader->fog_color[3] = 255;
	shader->fog_dist = Shader_ParseFloat (shader, ptr );

	if ( shader->fog_dist <= 0.0f ) {
		shader->fog_dist = 128.0f;
	}
	shader->fog_dist = 1.0f / shader->fog_dist;

	shader->flags |= SHADER_NODLIGHT|SHADER_NOSHADOWS;
}

static void Shader_SurfaceParm ( shader_t *shader, shaderpass_t *pass, char **ptr )
{
	char *token;

	token = Shader_ParseString ( ptr );
	if ( !Q_stricmp( token, "nodraw" ) )
		shader->flags |= SHADER_NODRAW;
	else if ( !Q_stricmp( token, "nodraw2" ) )
		shader->flags |= SHADER_NODRAW;
	else if ( !Q_stricmp( token, "nodlight" ) )
		shader->flags |= SHADER_NODLIGHT;
	else if ( !Q_stricmp( token, "noshadows" ) )
		shader->flags |= SHADER_NOSHADOWS;
}

static void Shader_Sort ( shader_t *shader, shaderpass_t *pass, char **ptr )
{
	char *token;

	token = Shader_ParseString ( ptr );
	if ( !Q_stricmp( token, "portal" ) )
		shader->sort = SHADER_SORT_PORTAL;
	else if( !Q_stricmp( token, "sky" ) )
		shader->sort = SHADER_SORT_SKY;
	else if( !Q_stricmp( token, "opaque" ) )
		shader->sort = SHADER_SORT_OPAQUE;
	else if( !Q_stricmp( token, "decal" ) )
		shader->sort = SHADER_SORT_DECAL;
	else if( !Q_stricmp( token, "seethrough" ) )
		shader->sort = SHADER_SORT_SEETHROUGH;
	else if( !Q_stricmp( token, "banner" ) )
		shader->sort = SHADER_SORT_BANNER;
	else if( !Q_stricmp( token, "additive" ) )
		shader->sort = SHADER_SORT_ADDITIVE;
	else if( !Q_stricmp( token, "underwater" ) )
		shader->sort = SHADER_SORT_UNDERWATER;
	else if( !Q_stricmp( token, "nearest" ) )
		shader->sort = SHADER_SORT_NEAREST;
	else if( !Q_stricmp( token, "blend" ) )
		shader->sort = SHADER_SORT_BLEND;
	else if ( !Q_stricmp( token, "lpp_light" ) )
		shader->sort = SHADER_SORT_PRELIGHT;
	else if ( !Q_stricmp( token, "ripple" ) )
		shader->sort = SHADER_SORT_RIPPLE;
	else
	{
		shader->sort = atoi ( token );
		clamp ( shader->sort, SHADER_SORT_NONE, SHADER_SORT_NEAREST );
	}
}

static void Shader_Prelight ( shader_t *shader, shaderpass_t *pass, char **ptr )
{
	shader->sort = SHADER_SORT_PRELIGHT;
}

static void Shader_Portal ( shader_t *shader, shaderpass_t *pass, char **ptr )
{
	shader->sort = SHADER_SORT_PORTAL;
}

static void Shader_PolygonOffset ( shader_t *shader, shaderpass_t *pass, char **ptr )
{
	/*the q3 defaults*/
	shader->polyoffset.factor = -0.05;
	shader->polyoffset.unit = -25;
}

static void Shader_EntityMergable ( shader_t *shader, shaderpass_t *pass, char **ptr )
{
	shader->flags |= SHADER_ENTITY_MERGABLE;
}

/*program text is already loaded, this function parses the 'header' of it to see which permutations it provides, and how many times we need to recompile it*/
static qboolean Shader_LoadPermutations(char *name, program_t *prog, char *script, int qrtype, int ver, char *blobfilename)
{
	static char *permutationname[] =
	{
		"#define BUMP\n",
		"#define FULLBRIGHT\n",
		"#define UPPERLOWER\n",
		"#define DELUXE\n",
		"#define SKELETAL\n",
		"#define FOG\n",
		"#define FRAMEBLEND\n",
		"#define LIGHTSTYLED\n",
		NULL
	};
#define MAXMODIFIERS 64
	const char *permutationdefines[sizeof(permutationname)/sizeof(permutationname[0]) + MAXMODIFIERS + 1];
	unsigned int nopermutation = ~0u;
	int nummodifiers = 0;
	int p, n, pn;
	char *end;
	vfsfile_t *blobfile;
	unsigned int permuoffsets[PERMUTATIONS], initoffset=0;
	unsigned int blobheaderoffset=0;
	qboolean blobadded;
	qboolean tess = false;

	char *cvarnames[64];
	int cvartypes[64];
	int cvarcount = 0;
	qboolean onefailed = false;
	extern cvar_t gl_specular;

	ver = 0;

	if (qrenderer != qrtype)
	{
		return false;
	}
	if (!sh_config.pCreateProgram && !sh_config.pLoadBlob)
		return false;

	cvarnames[cvarcount] = NULL;

	prog->nofixedcompat = true;
	for(;;)
	{
		while (*script == ' ' || *script == '\r' || *script == '\n' || *script == '\t')
			script++;
		if (!strncmp(script, "!!fixed", 7))
		{
			prog->nofixedcompat = false;
			script += 7;
			while (*script && *script != '\n')
				script++;
		}
		else if (!strncmp(script, "!!tess", 6))
		{
			tess = true;
			script += 6;
			while (*script && *script != '\n')
				script++;
		}
		else if (!strncmp(script, "!!cvardf", 8))
		{
			script += 8;
			while (*script == ' ' || *script == '\t')
				script++;
			end = script;
			while ((*end >= 'A' && *end <= 'Z') || (*end >= 'a' && *end <= 'z') || (*end >= '0' && *end <= '9') || *end == '_')
				end++;
			if (nummodifiers < MAXMODIFIERS && end - script < 64)
			{
				cvar_t *var;
				char namebuf[64];
				memcpy(namebuf, script, end - script);
				namebuf[end - script] = 0;
				var = Cvar_Get(namebuf, "0", CVAR_SHADERSYSTEM, "GLSL Variables");
				if (var)
					permutationdefines[nummodifiers++] = Z_StrDup(va("#define %s %g\n", namebuf, var->value));
			}
			script = end;
		}
		else if (!strncmp(script, "!!cvarf", 7))
		{
			script += 7;
			while (*script == ' ' || *script == '\t')
				script++;
			end = script;
			while ((*end >= 'A' && *end <= 'Z') || (*end >= 'a' && *end <= 'z') || (*end >= '0' && *end <= '9') || *end == '_')
				end++;
			if (cvarcount+1 != sizeof(cvarnames)/sizeof(cvarnames[0]))
			{
				cvartypes[cvarcount] = SP_CVARF;
				cvarnames[cvarcount++] = script;
				cvarnames[cvarcount] = NULL;
			}
			script = end;
		}
		else if (!strncmp(script, "!!cvari", 7))
		{
			script += 7;
			while (*script == ' ' || *script == '\t')
				script++;
			end = script;
			while ((*end >= 'A' && *end <= 'Z') || (*end >= 'a' && *end <= 'z') || (*end >= '0' && *end <= '9') || *end == '_')
				end++;
			if (cvarcount+1 != sizeof(cvarnames)/sizeof(cvarnames[0]))
			{
				cvartypes[cvarcount] = SP_CVARI;
				cvarnames[cvarcount++] = script;
				cvarnames[cvarcount] = NULL;
			}
			script = end;
		}
		else if (!strncmp(script, "!!cvarv", 7))
		{
			script += 7;
			while (*script == ' ' || *script == '\t')
				script++;
			end = script;
			while ((*end >= 'A' && *end <= 'Z') || (*end >= 'a' && *end <= 'z') || (*end >= '0' && *end <= '9') || *end == '_')
				end++;
			if (cvarcount+1 != sizeof(cvarnames)/sizeof(cvarnames[0]))
			{
				cvartypes[cvarcount] = SP_CVAR3F;
				cvarnames[cvarcount++] = script;
				cvarnames[cvarcount] = NULL;
			}
			script = end;
		}
		else if (!strncmp(script, "!!permu", 7))
		{
			script += 7;
			while (*script == ' ' || *script == '\t')
				script++;
			end = script;
			while ((*end >= 'A' && *end <= 'Z') || (*end >= 'a' && *end <= 'z') || (*end >= '0' && *end <= '9') || *end == '_')
				end++;
			for (p = 0; permutationname[p]; p++)
			{
				if (!strncmp(permutationname[p]+8, script, end - script) && permutationname[p][8+end-script] == '\n')
				{
					nopermutation &= ~(1u<<p);
					break;
				}
			}
			if (!permutationname[p])
				Con_DPrintf("Unknown pemutation in glsl program %s\n", name);
			script = end;
		}
		else if (!strncmp(script, "!!ver", 5))
		{
			script += 5;
			while (*script == ' ' || *script == '\t')
				script++;
			end = script;
			while ((*end >= 'A' && *end <= 'Z') || (*end >= 'a' && *end <= 'z') || (*end >= '0' && *end <= '9') || *end == '_')
				end++;
			ver = strtol(script, NULL, 0);
			script = end;
		}
		else
			break;
	};

	if (sh_config.pLoadBlob && blobfilename && *blobfilename)
		blobfile = FS_OpenVFS(blobfilename, "w+b", FS_GAMEONLY);
	else
		blobfile = NULL;

	if (blobfile)
	{
		unsigned int magic;
		unsigned int corrupt = false;
		char ever[MAX_QPATH];
		char *thisever = version_string();
		corrupt |= VFS_READ(blobfile, &magic, sizeof(magic)) != sizeof(magic);
		corrupt |= magic != *(unsigned int*)"FBLB";
		corrupt |= VFS_READ(blobfile, &blobheaderoffset, sizeof(blobheaderoffset)) != sizeof(blobheaderoffset);
		corrupt |= VFS_READ(blobfile, ever, sizeof(ever)) != sizeof(ever);

		corrupt |= strcmp(ever, thisever);
		//if the magic or header didn't read properly then the file is corrupt
		if (corrupt)
		{
			//close and reopen it without the + flag, to replace it with a new file.
			VFS_CLOSE(blobfile);
			blobfile = FS_OpenVFS(blobfilename, "wb", FS_GAMEONLY);

			if (blobfile)
			{
				blobheaderoffset = 0;
				VFS_SEEK(blobfile, 0);
				magic = *(unsigned int*)"FBLB";	//magic
				VFS_WRITE(blobfile, &magic, sizeof(magic));
				VFS_WRITE(blobfile, &blobheaderoffset, sizeof(blobheaderoffset));
				memset(ever, 0, sizeof(ever));	//make sure we don't leak stuff.
				Q_strncpyz(ever, thisever, sizeof(ever));
				VFS_WRITE(blobfile, ever, sizeof(ever));
				blobheaderoffset = 0;
			}
		}
	}
	blobadded = false;

	if (gl_specular.value)
	{
		if (nummodifiers < MAXMODIFIERS)
			permutationdefines[nummodifiers++] = Z_StrDup("#define SPECULAR\n");
	}
	for (end = strchr(name, '#'); end && *end; )
	{
		char *start = end+1, *d;
		end = strchr(start, '#');
		if (!end)
			end = start + strlen(start);
		if (nummodifiers < MAXMODIFIERS)
		{
			permutationdefines[nummodifiers] = d = BZ_Malloc(10 + end - start);
			memcpy(d, "#define ", 8);
			memcpy(d+8, start, end - start);
			memcpy(d+8+(end-start), "\n", 2);

			start = strchr(d+8, '=');
			if (start)
				*start = ' ';

			for (start = d+8; *start; start++)
				*start = toupper(*start);
			nummodifiers++;
			permutationdefines[nummodifiers] = NULL;
		}
	}

	if (blobfile)
	{
		unsigned int next;
		unsigned int argsz;
		char *args, *mp;
		const char *mv;
		int ml, mi;
		unsigned int bloblink = 4;

		//walk through looking for an argset match
		while (blobheaderoffset)
		{
			VFS_SEEK(blobfile, blobheaderoffset);
			VFS_READ(blobfile, &next, sizeof(next));
			VFS_READ(blobfile, &argsz, sizeof(argsz));
			args = Z_Malloc(argsz+1);
			VFS_READ(blobfile, args, argsz);
			args[argsz] = 0;
			for (mi = 0, mp = args; mi < nummodifiers; mi++)
			{
				mv = permutationdefines[mi]+8;
				ml = strlen(mv);
				if (mp+ml > args+argsz)
					break;
				if (strncmp(mp, mv, ml))
					break;
				mp += ml;
			}
			//this one is a match. 
			if (mi == nummodifiers && mp == args+argsz)
			{
				blobheaderoffset = VFS_TELL(blobfile);
				VFS_READ(blobfile, permuoffsets, sizeof(permuoffsets));
				break;
			}

			bloblink = blobheaderoffset;
			blobheaderoffset = next;
		}

		//these arguments have never been seen before. add a new argset.
		if (!blobheaderoffset)
		{
			unsigned int link = 0;
			initoffset = VFS_GETLEN(blobfile);
			VFS_SEEK(blobfile, initoffset);
			VFS_WRITE(blobfile, &link, sizeof(link));

			for (mi = 0, argsz = 0; mi < nummodifiers; mi++)
			{
				mv = permutationdefines[mi]+8;
				ml = strlen(mv);
				argsz += ml;
			}
			VFS_WRITE(blobfile, &argsz, sizeof(argsz));
			for (mi = 0; mi < nummodifiers; mi++)
			{
				mv = permutationdefines[mi]+8;
				ml = strlen(mv);
				VFS_WRITE(blobfile, mv, ml);
			}

			//and the offsets come here
			blobheaderoffset = VFS_TELL(blobfile);
			memset(permuoffsets, 0, sizeof(permuoffsets));
			VFS_WRITE(blobfile, permuoffsets, sizeof(permuoffsets));

			//now rewrite the link to add us. the value in the file should always be set to 0.
			VFS_SEEK(blobfile, bloblink);
			VFS_WRITE(blobfile, &initoffset, sizeof(initoffset));
		}
	}

	for (p = 0; p < PERMUTATIONS; p++)
	{
		memset(&prog->permu[p].handle, 0, sizeof(prog->permu[p].handle));
		if (nopermutation & p)
		{
			continue;
		}
		pn = nummodifiers;
		for (n = 0; permutationname[n]; n++)
		{
			if (p & (1u<<n))
				permutationdefines[pn++] = permutationname[n];
		}
		if (p & PERMUTATION_UPPERLOWER)
			permutationdefines[pn++] = "#define UPPER\n#define LOWER\n";
		if (p & PERMUTATION_BUMPMAP)
		{
			if (r_glsl_offsetmapping.ival)
			{
				permutationdefines[pn++] = "#define OFFSETMAPPING\n";
				if (r_glsl_offsetmapping_reliefmapping.ival && (p & PERMUTATION_BUMPMAP))
					permutationdefines[pn++] = "#define RELIEFMAPPING\n";
			}
		}
		permutationdefines[pn++] = NULL;


		if (blobfile && permuoffsets[p])
		{
			VFS_SEEK(blobfile, permuoffsets[p]);
			if (sh_config.pLoadBlob(prog, name, p, blobfile))
				continue;	//blob was loaded from disk, yay.
			//otherwise fall through.
		}
		if (blobfile && !sh_config.pValidateProgram)
		{
			initoffset = VFS_GETLEN(blobfile);
			VFS_SEEK(blobfile, initoffset);
		}
		if (!sh_config.pCreateProgram(prog, name, p, ver, permutationdefines, script, tess?script:NULL, tess?script:NULL, script, (p & PERMUTATION_SKELETAL)?true:onefailed, sh_config.pValidateProgram?NULL:blobfile))
		{
			if (!(p & PERMUTATION_SKELETAL))
				onefailed = true;	//don't flag it if skeletal failed.
			if (!p)	//give up if permutation 0 failed. that one failing is fatal.
				break;
		}
		if (!sh_config.pValidateProgram && blobfile && initoffset != VFS_GETLEN(blobfile))
		{
			permuoffsets[p] = initoffset;
			blobadded = true;
		}
	}
	while(nummodifiers)
		Z_Free((char*)permutationdefines[--nummodifiers]);

	//extra loop to validate the programs actually linked properly.
	//delaying it like this gives certain threaded drivers a chance to compile them all while we're messing around with other junk
	if (sh_config.pValidateProgram)
	for (p = 0; p < PERMUTATIONS; p++)
	{
		if (nopermutation & p)
			continue;
		if (blobfile)
		{
			initoffset = VFS_GETLEN(blobfile);
			VFS_SEEK(blobfile, initoffset);
		}
		if (!sh_config.pValidateProgram(prog, name, p, (p & PERMUTATION_SKELETAL)?true:onefailed, blobfile))
		{
			if (!(p & PERMUTATION_SKELETAL))
			{
				onefailed = true;	//don't flag it if skeletal failed.
				continue;
			}
			if (!p)
				break;
		}
		if (blobfile && initoffset != VFS_GETLEN(blobfile))
		{
			permuoffsets[p] = initoffset;
			blobadded = true;
		}
	}

	if (sh_config.pProgAutoFields)
		sh_config.pProgAutoFields(prog, cvarnames, cvartypes);

	if (blobfile && blobadded)
	{
		VFS_SEEK(blobfile, blobheaderoffset);
		VFS_WRITE(blobfile, permuoffsets, sizeof(permuoffsets));
	}
	if (blobfile)
		VFS_CLOSE(blobfile);

	if (p == PERMUTATIONS)
		return true;
	return false;
}
typedef struct sgeneric_s
{
	program_t prog;
	struct sgeneric_s *next;
	char *name;
	qboolean failed;
} sgeneric_t;
static sgeneric_t *sgenerics;
struct sbuiltin_s
{
	int qrtype;
	int apiver;
	char name[MAX_QPATH];
	char *body;
} sbuiltins[] =
{
#include "r_bishaders.h"
	{QR_NONE}
};
void Shader_UnloadProg(program_t *prog)
{
	if (sh_config.pDeleteProg)
	{
		int p;
		for (p = 0; p < PERMUTATIONS; p++)
			sh_config.pDeleteProg(prog, p);
	}

	free(prog);
}
static void Shader_FlushGenerics(void)
{
	sgeneric_t *g;
	while (sgenerics)
	{
		g = sgenerics;
		sgenerics = g->next;

		if (g->prog.refs == 1)
		{
			g->prog.refs--;
			Shader_UnloadProg(&g->prog);
		}
		else
			Con_Printf("generic shader still used\n"); 
	}
}
static void Shader_LoadGeneric(sgeneric_t *g, int qrtype)
{
	unsigned int i;
	void *file;
	char basicname[MAX_QPATH];
	char blobname[MAX_QPATH];
	char *h;

	g->failed = true;

	basicname[1] = 0;
	Q_strncpyz(basicname, g->name, sizeof(basicname));
	h = strchr(basicname+1, '#');
	if (h)
		*h = '\0';

	if (strchr(basicname, '/') || strchr(basicname, '.'))
	{
		FS_LoadFile(basicname, &file);
		*blobname = 0;
	}
	else
	{
		if (sh_config.progpath)
		{
			Q_snprintfz(blobname, sizeof(blobname), sh_config.progpath, basicname);
			FS_LoadFile(blobname, &file);
		}
		else
			file = NULL;
		if (sh_config.blobpath && r_shaderblobs.ival)
			Q_snprintfz(blobname, sizeof(blobname), sh_config.blobpath, basicname);
		else
			*blobname = 0;
	}

	if (file)
	{
		Con_DPrintf("Loaded %s from disk\n", basicname);
		g->failed = !Shader_LoadPermutations(g->name, &g->prog, file, qrtype, 0, blobname);
		FS_FreeFile(file);
		return;
	}
	else
	{
		int ver;
		for (i = 0; *sbuiltins[i].name; i++)
		{
			if (sbuiltins[i].qrtype == qrenderer && !strcmp(sbuiltins[i].name, basicname))
			{
				ver = sbuiltins[i].apiver;

				if (ver < sh_config.minver || ver > sh_config.maxver)
					continue;

				g->failed = !Shader_LoadPermutations(g->name, &g->prog, sbuiltins[i].body, sbuiltins[i].qrtype, ver, blobname);

				if (g->failed)
					continue;

				return;
			}
		}
	}
}
static program_t *Shader_FindGeneric(char *name, int qrtype)
{
	sgeneric_t *g;

	for (g = sgenerics; g; g = g->next)
	{
		if (!strcmp(name, g->name))
		{
			if (g->failed)
				return NULL;
			g->prog.refs++;
			return &g->prog;
		}
	}

	//don't even try if we know it won't work.
	if (!sh_config.progs_supported)
		return NULL;

	g = malloc(sizeof(*g) + strlen(name)+1);
	memset(g, 0, sizeof(*g));
	g->name = (char*)(g+1);
	strcpy(g->name, name);
	g->next = sgenerics;
	sgenerics = g;

	g->prog.refs = 1;

	Shader_LoadGeneric(g, qrtype);
	if (g->failed)
		return NULL;
	g->prog.refs++;
	return &g->prog;
}
static void Shader_ReloadGenerics(void)
{
	sgeneric_t *g;
	for (g = sgenerics; g; g = g->next)
	{
		Shader_LoadGeneric(g, qrenderer);
	}

	//this shader can take a while to load due to its number of permutations.
	//because this all happens on the main thread, try to avoid random stalls by pre-loading it.
	if (sh_config.progs_supported)
	{
		program_t *p = Shader_FindGeneric("defaultskin", qrenderer);
		if (p)	//generics get held on to in order to avoid so much churn. so we can just release the reference we just created and it'll be held until shutdown anyway.
			p->refs--;
	}
}
void Shader_WriteOutGenerics_f(void)
{
	int i;
	char *name;
	for (i = 0; *sbuiltins[i].name; i++)
	{
		name = NULL;
		if (sbuiltins[i].qrtype == QR_OPENGL)
		{
			if (sbuiltins[i].apiver == 100)
				name = va("gles/eg_%s.glsl", sbuiltins[i].name);
			else
				name = va("glsl/eg_%s.glsl", sbuiltins[i].name);
		}
		else if (sbuiltins[i].qrtype == QR_DIRECT3D9)
			name = va("hlsl/eg_%s.hlsl", sbuiltins[i].name);
		else if (sbuiltins[i].qrtype == QR_DIRECT3D11)
			name = va("hlsl11/eg_%s.hlsl", sbuiltins[i].name);

		if (name)
		{
			vfsfile_t *f = FS_OpenVFS(name, "rb", FS_GAMEONLY);
			if (f)
			{
				int len = VFS_GETLEN(f);
				char *buf = Hunk_TempAlloc(len);
				VFS_READ(f, buf, len);
				if (len != strlen(sbuiltins[i].body) || memcmp(buf, sbuiltins[i].body, len))
					Con_Printf("Not writing %s - modified version in the way\n", name);
				else
					Con_Printf("%s is unmodified\n", name);
				VFS_CLOSE(f);
			}
			else
			{
				Con_Printf("Writing %s\n", name);
				FS_WriteFile(name, sbuiltins[i].body, strlen(sbuiltins[i].body), FS_GAMEONLY);
			}
		}
	}
}

struct shader_field_names_s shader_attr_names[] =
{
	/*vertex attributes*/
	{"v_position1",				VATTR_VERTEX1},
	{"v_position2",				VATTR_VERTEX2},
	{"v_colour",				VATTR_COLOUR},
	{"v_texcoord",				VATTR_TEXCOORD},
	{"v_lmcoord",				VATTR_LMCOORD},
	{"v_normal",				VATTR_NORMALS},
	{"v_svector",				VATTR_SNORMALS},
	{"v_tvector",				VATTR_TNORMALS},
	{"v_bone",					VATTR_BONENUMS},
	{"v_weight",				VATTR_BONEWEIGHTS},
#if MAXRLIGHTMAPS > 1
	{"v_lmcoord1",				VATTR_LMCOORD},
	{"v_lmcoord2",				VATTR_LMCOORD2},
	{"v_lmcoord3",				VATTR_LMCOORD3},
	{"v_lmcoord4",				VATTR_LMCOORD4},
	{"v_colour1",				VATTR_COLOUR},
	{"v_colour2",				VATTR_COLOUR2},
	{"v_colour3",				VATTR_COLOUR3},
	{"v_colour4",				VATTR_COLOUR4},
#endif
	{NULL}
};

struct shader_field_names_s shader_unif_names[] =
{
	/*matricies*/
	{"m_model",					SP_M_MODEL},
	{"m_view",					SP_M_VIEW},
	{"m_modelview",				SP_M_MODELVIEW},
	{"m_projection",			SP_M_PROJECTION},
	{"m_modelviewprojection",	SP_M_MODELVIEWPROJECTION},
	{"m_bones",					SP_M_ENTBONES},
	{"m_invviewprojection",		SP_M_INVVIEWPROJECTION},
	{"m_invmodelviewprojection",SP_M_INVMODELVIEWPROJECTION},

	/*viewer properties*/
	{"v_eyepos",				SP_V_EYEPOS},
	{"w_fog",					SP_W_FOG},

	/*ent properties*/
	{"e_vblend",				SP_E_VBLEND},
	{"e_lmscale",				SP_E_LMSCALE}, /*overbright shifting*/
	{"e_origin",				SP_E_ORIGIN},
	{"e_time",					SP_E_TIME},
	{"e_eyepos",				SP_E_EYEPOS},
	{"e_colour",				SP_E_COLOURS},
	{"e_colourident",			SP_E_COLOURSIDENT},
	{"e_glowmod",				SP_E_GLOWMOD},
	{"e_uppercolour",			SP_E_TOPCOLOURS},
	{"e_lowercolour",			SP_E_BOTTOMCOLOURS},
	{"e_light_dir",				SP_E_L_DIR},
	{"e_light_mul",				SP_E_L_MUL},
	{"e_light_ambient",			SP_E_L_AMBIENT},

	/*rtlight properties, use with caution*/
	{"l_lightscreen",			SP_LIGHTSCREEN},
	{"l_lightradius",			SP_LIGHTRADIUS},
	{"l_lightcolour",			SP_LIGHTCOLOUR},
	{"l_lightposition",			SP_LIGHTPOSITION},
	{"l_lightcolourscale",		SP_LIGHTCOLOURSCALE},
	{"l_cubematrix",			SP_LIGHTCUBEMATRIX},
	{"l_shadowmapproj",			SP_LIGHTSHADOWMAPPROJ},
	{"l_shadowmapscale",		SP_LIGHTSHADOWMAPSCALE},

	{"e_rendertexturescale",	SP_RENDERTEXTURESCALE},
	{NULL}
};

static char *Shader_ParseBody(char *debugname, char **ptr)
{
	char *body;
	char *start, *end;

	end = *ptr;
	while (*end == ' ' || *end == '\t' || *end == '\r')
		end++;
	if (*end == '\n')
	{
		int count;
		end++;
		while (*end == ' ' || *end == '\t')
			end++;
		if (*end != '{')
		{
			Con_Printf("shader \"%s\" missing program string\n", debugname);
		}
		else
		{
			end++;
			start = end;
			for (count = 1; *end; end++)
			{
				if (*end == '}')
				{
					count--;
					if (!count)
						break;
				}
				else if (*end == '{')
					count++;
			}
			body = BZ_Malloc(end - start + 1);
			memcpy(body, start, end-start);
			body[end-start] = 0;
			*ptr = end+1;/*skip over it all*/

			return body;
		}
	}
	return NULL;
}

static void Shader_SLProgramName (shader_t *shader, shaderpass_t *pass, char **ptr, int qrtype)
{
	/*accepts:
	program
	{
		BLAH
	}
	where BLAH is both vertex+frag with #ifdefs
	or
	program fname
	on one line.
	*/
	char *programbody;
	char *hash;

	programbody = Shader_ParseBody(shader->name, ptr);
	if (programbody)
	{
		shader->prog = malloc(sizeof(*shader->prog));
		memset(shader->prog, 0, sizeof(*shader->prog));
		shader->prog->refs = 1;
		if (!Shader_LoadPermutations(shader->name, shader->prog, programbody, qrtype, 0, NULL))
		{
			free(shader->prog);
			shader->prog = NULL;
		}

		BZ_Free(programbody);
		return;
	}

	hash = strchr(shader->name, '#');
	if (hash)
	{
		//pass the # postfixes from the shader name onto the generic glsl to use
		char newname[512];
		Q_snprintfz(newname, sizeof(newname), "%s%s", Shader_ParseExactString(ptr), hash);
		shader->prog = Shader_FindGeneric(newname, qrtype);
	}
	else
		shader->prog = Shader_FindGeneric(Shader_ParseExactString(ptr), qrtype);
}

static void Shader_GLSLProgramName (shader_t *shader, shaderpass_t *pass, char **ptr)
{
	Shader_SLProgramName(shader,pass,ptr,QR_OPENGL);
}
static void Shader_ProgramName (shader_t *shader, shaderpass_t *pass, char **ptr)
{
	Shader_SLProgramName(shader,pass,ptr,qrenderer);
}
static void Shader_HLSL9ProgramName (shader_t *shader, shaderpass_t *pass, char **ptr)
{
	Shader_SLProgramName(shader,pass,ptr,QR_DIRECT3D9);
}
static void Shader_HLSL11ProgramName (shader_t *shader, shaderpass_t *pass, char **ptr)
{
	Shader_SLProgramName(shader,pass,ptr,QR_DIRECT3D11);
}

static void Shader_ProgramParam ( shader_t *shader, shaderpass_t *pass, char **ptr )
{
	cvar_t *cv = NULL;
	int specialint = 0;
	float specialfloat = 0;
	vec3_t specialvec = {0};
	enum shaderprogparmtype_e parmtype = SP_BAD;
	char *token;
	qboolean silent = false;
	char *forcename = NULL;

	token = Shader_ParseString(ptr);
	if (!Q_stricmp(token, "opt"))
	{
		silent = true;
		token = Shader_ParseString(ptr);
	}
	if (!Q_stricmp(token, "texture"))
	{
		token = Shader_ParseString(ptr);
		specialint = atoi(token);
		parmtype = SP_TEXTURE;
	}
	else if (!Q_stricmp(token, "consti"))
	{
		token = Shader_ParseSensString(ptr);
		specialint = atoi(token);
		parmtype = SP_CONSTI;
	}
	else if (!Q_stricmp(token, "constf"))
	{
		token = Shader_ParseSensString(ptr);
		specialfloat = atof(token);
		parmtype = SP_CONSTF;
	}
	else if (!Q_stricmp(token, "cvari"))
	{
		token = Shader_ParseSensString(ptr);
		cv = Cvar_Get(token, "", 0, "GLSL Shader parameters");
		if (!cv)
			return;
		parmtype = SP_CVARI;
	}
	else if (!Q_stricmp(token, "cvarf"))
	{
		token = Shader_ParseSensString(ptr);
		cv = Cvar_Get(token, "", 0, "GLSL Shader parameters");
		if (!cv)
			return;
		parmtype = SP_CVARF;
	}
	else if (!Q_stricmp(token, "cvar3f"))
	{
		token = Shader_ParseSensString(ptr);
		cv = Cvar_Get(token, "", 0, "GLSL Shader parameters");
		if (!cv)
			return;
		parmtype = SP_CVAR3F;
	}
	else if (!Q_stricmp(token, "time"))
		parmtype = SP_E_TIME;
	else if (!Q_stricmp(token, "eyepos"))
		parmtype = SP_E_EYEPOS;
	else if (!Q_stricmp(token, "entmatrix"))
		parmtype = SP_M_MODEL;
	else if (!Q_stricmp(token, "colours") || !Q_stricmp(token, "colors"))
		parmtype = SP_E_COLOURS;
	else if (!Q_stricmp(token, "upper"))
		parmtype = SP_E_TOPCOLOURS;
	else if (!Q_stricmp(token, "lower"))
		parmtype = SP_E_BOTTOMCOLOURS;
	else if (!Q_stricmp(token, "lightradius"))
		parmtype = SP_LIGHTRADIUS;
	else if (!Q_stricmp(token, "lightcolour"))
		parmtype = SP_LIGHTCOLOUR;
	else if (!Q_stricmp(token, "lightpos"))
		parmtype = SP_LIGHTPOSITION;
	else if (!Q_stricmp(token, "rendertexturescale"))
		parmtype = SP_RENDERTEXTURESCALE;
	else
		Con_Printf("shader %s: parameter type \"%s\" not known\n", shader->name, token);

	if (forcename)
		token = forcename;
	else
		token = Shader_ParseSensString(ptr);

#ifdef GLQUAKE
	if (qrenderer == QR_OPENGL)
	{
		int p;
		qboolean foundone;
		unsigned int uniformloc;
		program_t *prog = shader->prog;
		if (!prog)
		{
			Con_Printf("shader %s: param without program set\n", shader->name);
		}
		else if (prog->numparams == SHADER_PROGPARMS_MAX)
			Con_Printf("shader %s: too many parms\n", shader->name);
		else
		{
			if (prog->refs != 1)
				Con_Printf("shader %s: parms on shared shader\n", shader->name);

			foundone = false;
			prog->parm[prog->numparams].type = parmtype;
			for (p = 0; p < PERMUTATIONS; p++)
			{
				if (!prog->permu[p].handle.glsl.handle)
					continue;
				GLSlang_UseProgram(prog->permu[p].handle.glsl.handle);

				uniformloc = qglGetUniformLocationARB(prog->permu[p].handle.glsl.handle, token);
				prog->permu[p].parm[prog->numparams] = uniformloc;

				if (uniformloc != -1)
				{
					foundone = true;
					switch(parmtype)
					{
					case SP_BAD:
						foundone = false;
						break;
					case SP_TEXTURE:
					case SP_CONSTI:
						prog->parm[prog->numparams].ival = specialint;
						break;
					case SP_CONSTF:
						prog->parm[prog->numparams].fval = specialfloat;
						break;
					case SP_CVARF:
					case SP_CVARI:
						prog->parm[prog->numparams].pval = cv;
						break;
					case SP_CVAR3F:
						prog->parm[prog->numparams].pval = cv;
						qglUniform3fvARB(uniformloc, 1, specialvec);
						break;
					default:
						break;
					}
				}
			}
			if (!foundone)
			{
				if (!silent)
					Con_Printf("shader %s: param \"%s\" not found\n", shader->name, token);
			}
			else
				prog->numparams++;

			GLSlang_UseProgram(0);
		}
	}
#endif
}

static void Shader_DiffuseMap(shader_t *shader, shaderpass_t *pass, char **ptr)
{
	char *token;
	token = Shader_ParseString(ptr);
	shader->defaulttextures.base = R_LoadHiResTexture(token, NULL, 0);
}

static void Shader_Translucent(shader_t *shader, shaderpass_t *pass, char **ptr)
{
	shader->flags |= SHADER_BLEND;
}

static void Shader_DP_Camera(shader_t *shader, shaderpass_t *pass, char **ptr)
{
	shader->sort = SHADER_SORT_PORTAL;
}

static void Shader_BEMode(shader_t *shader, shaderpass_t *pass, char **ptr)
{
	int mode;
	char tokencopy[1024];
	char *token;
	char *embed = NULL;
	token = Shader_ParseString(ptr);
	if (!Q_stricmp(token, "rtlight"))
		mode = -1;	//all light types
	else if (!Q_stricmp(token, "rtlight_only"))
		mode = LSHADER_STANDARD;
	else if (!Q_stricmp(token, "rtlight_smap"))
		mode = LSHADER_SMAP;
	else if (!Q_stricmp(token, "rtlight_spot"))
		mode = LSHADER_SPOT;
	else if (!Q_stricmp(token, "rtlight_cube"))
		mode = LSHADER_CUBE;
	else if (!Q_stricmp(token, "rtlight_cube_smap"))
		mode = LSHADER_CUBE|LSHADER_SMAP;
	else if (!Q_stricmp(token, "rtlight_cube_spot"))		//doesn't make sense.
		mode = LSHADER_CUBE|LSHADER_SPOT;
	else if (!Q_stricmp(token, "rtlight_spot_smap"))
		mode = LSHADER_SMAP|LSHADER_SPOT;
	else if (!Q_stricmp(token, "rtlight_cube_spot_smap"))	//doesn't make sense.
		mode = LSHADER_CUBE|LSHADER_SPOT|LSHADER_SMAP;
	else if (!Q_stricmp(token, "crepuscular"))
		mode = bemoverride_crepuscular;
	else if (!Q_stricmp(token, "depthonly"))
		mode = bemoverride_depthonly;
	else if (!Q_stricmp(token, "depthdark"))
		mode = bemoverride_depthdark;
	else if (!Q_stricmp(token, "prelight"))
		mode = bemoverride_prelight;
	else if (!Q_stricmp(token, "fog"))
		mode = bemoverride_fog;
	else
	{
		Con_DPrintf(CON_WARNING "Shader %s specifies unknown bemode %s.\n", shader->name, token);
		return;	//not supported.
	}

	embed = Shader_ParseBody(shader->name, ptr);
	if (embed)
	{
		int l = strlen(embed) + 6;
		char *b = BZ_Malloc(l);
		Q_snprintfz(b, l, "{\n%s\n}\n", embed);
		BZ_Free(embed);
		embed = b;
		//generate a unique name
		Q_snprintfz(tokencopy, sizeof(tokencopy), "%s_mode%i", shader->name, mode);
	}
	else
	{
		token = Shader_ParseString(ptr);
		Q_strncpyz(tokencopy, token, sizeof(tokencopy));	//make sure things don't go squiff.
	}

	if (mode == -1)
	{
		//shorthand for rtlights
		for (mode = 0; mode < LSHADER_MODES; mode++)
		{
			if ((mode & LSHADER_CUBE) && (mode & LSHADER_SPOT))
				continue;
			shader->bemoverrides[mode] = R_RegisterCustom(va("%s%s%s%s%s", 
																tokencopy,
																(mode & LSHADER_SMAP)?"#PCF":"",
																(mode & LSHADER_SPOT)?"#SPOT":"",
																(mode & LSHADER_CUBE)?"#CUBE":"",
#ifdef GLQUAKE
																(qrenderer == QR_OPENGL && gl_config.arb_shadow && (mode & (LSHADER_SMAP|LSHADER_SPOT)))?"#USE_ARB_SHADOW":""
#else
																""
#endif
																)
														, shader->usageflags, embed?Shader_DefaultScript:NULL, embed);
		}
	}
	else
	{
		shader->bemoverrides[mode] = R_RegisterCustom(tokencopy, shader->usageflags, embed?Shader_DefaultScript:NULL, embed);
	}
	if (embed)
		BZ_Free(embed);
}

static shaderkey_t shaderkeys[] =
{
	{"cull",			Shader_Cull},
	{"skyparms",		Shader_SkyParms},
	{"fogparms",		Shader_FogParms},
	{"surfaceparm",		Shader_SurfaceParm},
	{"nomipmaps",		Shader_NoMipMaps},
	{"nopicmip",		Shader_NoPicMip},
	{"polygonoffset",	Shader_PolygonOffset},
	{"sort",			Shader_Sort},
	{"deformvertexes",	Shader_DeformVertexes},
	{"portal",			Shader_Portal},
	{"entitymergable",	Shader_EntityMergable},

	//fte extensions
	{"lpp_light",		Shader_Prelight},
	{"glslprogram",		Shader_GLSLProgramName},
	{"program",			Shader_ProgramName},	//gl or d3d
	{"hlslprogram",		Shader_HLSL9ProgramName},	//for d3d
	{"hlsl11program",	Shader_HLSL11ProgramName},	//for d3d
	{"param",			Shader_ProgramParam},	//legacy

	{"bemode",			Shader_BEMode},

	//dp compat
	{"dp_camera",		Shader_DP_Camera},

	/*doom3 compat*/
	{"diffusemap",		Shader_DiffuseMap},	//macro for "{\nstage diffusemap\nmap <map>\n}"
	{"bumpmap",			NULL},				//macro for "{\nstage bumpmap\nmap <map>\n}"
	{"specularmap",		NULL},				//macro for "{\nstage specularmap\nmap <map>\n}"
	{"discrete",		NULL},
	{"nonsolid",		NULL},
	{"noimpact",		NULL},
	{"translucent",		Shader_Translucent},
	{"noshadows",		NULL},
	{"nooverlays",		NULL},
	{"nofragment",		NULL},

	{NULL,				NULL}
};

static struct
{
	char *name;
	char *body;
} shadermacros[] =
{
	{"decal_macro", 	"polygonOffset 1\ndiscrete\nsort decal\nnoShadows"},
//	{"diffusemap", 		"{\nblend diffusemap\nmap %1\n}"},
//	{"bumpmap", 		"{\nblend bumpmap\nmap %1\n}"},
//	{"specularmap", 	"{\nblend specularmap\nmap %1\n}"},
	{NULL}
};

// ===============================================================

static qboolean Shaderpass_MapGen (shader_t *shader, shaderpass_t *pass, char *tname)
{
	int tcgen = TC_GEN_BASE;
	if (!Q_stricmp (tname, "$lightmap"))
	{
		tcgen = TC_GEN_LIGHTMAP;
		pass->flags |= SHADER_PASS_LIGHTMAP | SHADER_PASS_NOMIPMAP;
		pass->texgen = T_GEN_LIGHTMAP;
		shader->flags |= SHADER_HASLIGHTMAP;
	}
	else if (!Q_stricmp (tname, "$deluxmap"))
	{
		tcgen = TC_GEN_LIGHTMAP;
		pass->flags |= SHADER_PASS_DELUXMAP | SHADER_PASS_NOMIPMAP;
		pass->texgen = T_GEN_DELUXMAP;
	}
	else if (!Q_stricmp (tname, "$diffuse"))
	{
		pass->texgen = T_GEN_DIFFUSE;
	}
	else if (!Q_stricmp (tname, "$normalmap"))
	{
		pass->texgen = T_GEN_NORMALMAP;
		shader->flags |= SHADER_HASNORMALMAP;
	}
	else if (!Q_stricmp (tname, "$specular"))
	{
		pass->texgen = T_GEN_SPECULAR;
		shader->flags |= SHADER_HASGLOSS;
	}
	else if (!Q_stricmp (tname, "$fullbright"))
	{
		pass->texgen = T_GEN_FULLBRIGHT;
		shader->flags |= SHADER_HASFULLBRIGHT;
	}
	else if (!Q_stricmp (tname, "$upperoverlay"))
	{
		shader->flags |= SHADER_HASTOPBOTTOM;
		pass->texgen = T_GEN_UPPEROVERLAY;
	}
	else if (!Q_stricmp (tname, "$loweroverlay"))
	{
		shader->flags |= SHADER_HASTOPBOTTOM;
		pass->texgen = T_GEN_LOWEROVERLAY;
	}
	else if (!Q_stricmp (tname, "$shadowmap"))
	{
		pass->texgen = T_GEN_SHADOWMAP;
		pass->flags |= SHADER_PASS_DEPTHCMP;
	}
	else if (!Q_stricmp (tname, "$lightcubemap"))
	{
		pass->texgen = T_GEN_LIGHTCUBEMAP;
	}
	else if (!Q_stricmp (tname, "$currentrender"))
	{
		pass->texgen = T_GEN_CURRENTRENDER;
	}
	else if (!Q_stricmp (tname, "$sourcecolour"))
	{
		pass->texgen = T_GEN_SOURCECOLOUR;
	}
	else if (!Q_stricmp (tname, "$sourcecube"))
	{
		pass->texgen = T_GEN_SOURCECUBE;
	}
	else if (!Q_stricmp (tname, "$sourcedepth"))
	{
		pass->texgen = T_GEN_SOURCEDEPTH;
	}
	else if (!Q_stricmp (tname, "$reflection"))
	{
		shader->flags |= SHADER_HASREFLECT;
		pass->texgen = T_GEN_REFLECTION;
	}
	else if (!Q_stricmp (tname, "$refraction"))
	{
		shader->flags |= SHADER_HASREFRACT;
		pass->texgen = T_GEN_REFRACTION;
	}
	else if (!Q_stricmp (tname, "$refractiondepth"))
	{
		shader->flags |= SHADER_HASREFRACT;
		pass->texgen = T_GEN_REFRACTIONDEPTH;
	}
	else if (!Q_stricmp (tname, "$ripplemap"))
	{
		shader->flags |= SHADER_HASRIPPLEMAP;
		pass->texgen = T_GEN_RIPPLEMAP;
	}
	else if (!Q_stricmp (tname, "$null"))
	{
		pass->flags |= SHADER_PASS_NOMIPMAP|SHADER_PASS_DETAIL;
		pass->texgen = T_GEN_SINGLEMAP;
	}
	else
		return false;

	if (pass->tcgen == TC_GEN_UNSPECIFIED)
		pass->tcgen = tcgen;
	return true;
}

static void Shaderpass_Map (shader_t *shader, shaderpass_t *pass, char **ptr)
{
	int flags;
	char *token;

	pass->anim_frames[0] = r_nulltex;

	token = Shader_ParseString (ptr);
	flags = Shader_SetImageFlags (shader, pass, &token);
	if (!Shaderpass_MapGen(shader, pass, token))
	{
		switch((flags & IF_TEXTYPE) >> IF_TEXTYPESHIFT)
		{
		case 0:
			pass->texgen = T_GEN_SINGLEMAP;
			break;
		case 1:
			pass->texgen = T_GEN_3DMAP;
			break;
		default:
			pass->texgen = T_GEN_CUBEMAP;
			break;
		}

		if (pass->tcgen == TC_GEN_UNSPECIFIED)
			pass->tcgen = TC_GEN_BASE;
		pass->anim_frames[0] = Shader_FindImage (token, flags);
	}
}

static void Shaderpass_AnimMap (shader_t *shader, shaderpass_t *pass, char **ptr)
{
	int flags;
	char *token;
	texid_t image;

	flags = Shader_SetImageFlags (shader, pass, NULL);

	if (pass->tcgen == TC_GEN_UNSPECIFIED)
		pass->tcgen = TC_GEN_BASE;
	pass->flags |= SHADER_PASS_ANIMMAP;
	pass->texgen = T_GEN_ANIMMAP;
	pass->anim_fps = (int)Shader_ParseFloat (shader, ptr);
	pass->anim_numframes = 0;

	for ( ; ; )
	{
		token = Shader_ParseString(ptr);
		if (!token[0])
		{
			break;
		}

		if (pass->anim_numframes < SHADER_MAX_ANIMFRAMES)
		{
			image = Shader_FindImage (token, flags);

			if (!TEXVALID(image))
			{
				pass->anim_frames[pass->anim_numframes++] = missing_texture;
				Con_DPrintf (CON_WARNING "Shader %s has an animmap with no image: %s.\n", shader->name, token );
			}
			else
			{
				pass->anim_frames[pass->anim_numframes++] = image;
			}
		}
	}
}

static void Shaderpass_ClampMap (shader_t *shader, shaderpass_t *pass, char **ptr)
{
	int flags;
	char *token;

	token = Shader_ParseString (ptr);

	flags = Shader_SetImageFlags (shader, pass, &token);
	if (!Shaderpass_MapGen(shader, pass, token))
	{
		if (pass->tcgen == TC_GEN_UNSPECIFIED)
			pass->tcgen = TC_GEN_BASE;
		pass->anim_frames[0] = Shader_FindImage (token, flags | IF_CLAMP);

		switch((flags & IF_TEXTYPE) >> IF_TEXTYPESHIFT)
		{
		case 0:
			pass->texgen = T_GEN_SINGLEMAP;
			break;
		case 1:
			pass->texgen = T_GEN_3DMAP;
			break;
		default:
			pass->texgen = T_GEN_CUBEMAP;
			break;
		}

		if (!TEXVALID(pass->anim_frames[0]))
		{
			if (flags & (IF_3DMAP | IF_CUBEMAP))
				pass->anim_frames[0] = r_nulltex;
			else
				pass->anim_frames[0] = missing_texture;
			Con_DPrintf (CON_WARNING "Shader %s has a stage with no image: %s.\n", shader->name, token);
		}
	}
	pass->flags |= SHADER_PASS_CLAMP;
}

static void Shaderpass_VideoMap (shader_t *shader, shaderpass_t *pass, char **ptr)
{
	char		*token;

	token = Shader_ParseSensString (ptr);

#ifdef NOMEDIA
#else
	if (pass->cin)
		Z_Free (pass->cin);

	pass->cin = Media_StartCin(token);
	if (!pass->cin)
		pass->cin = Media_StartCin(va("video/%s.roq", token));
	if (!pass->cin)
		Con_DPrintf (CON_WARNING "(shader %s) Couldn't load video %s\n", shader->name, token);

	if (pass->cin)
	{
		pass->flags |= SHADER_PASS_VIDEOMAP;
		shader->flags |= SHADER_VIDEOMAP;
		pass->texgen = T_GEN_VIDEOMAP;
	}
	else
	{
		pass->texgen = T_GEN_DIFFUSE;
		pass->rgbgen = RGB_GEN_CONST;
		pass->rgbgen_func.type = SHADER_FUNC_CONSTANT;
		pass->rgbgen_func.args[0] = pass->rgbgen_func.args[1] = pass->rgbgen_func.args[2] = 0;
	}
#endif
}

static void Shaderpass_RGBGen (shader_t *shader, shaderpass_t *pass, char **ptr)
{
	char		*token;

	token = Shader_ParseString (ptr);
	if (!Q_stricmp (token, "identitylighting"))
		pass->rgbgen = RGB_GEN_IDENTITY_LIGHTING;
	else if (!Q_stricmp (token, "identity"))
		pass->rgbgen = RGB_GEN_IDENTITY;
	else if (!Q_stricmp (token, "wave"))
	{
		pass->rgbgen = RGB_GEN_WAVE;
		Shader_ParseFunc (shader, ptr, &pass->rgbgen_func);
	}
	else if (!Q_stricmp(token, "entity"))
		pass->rgbgen = RGB_GEN_ENTITY;
	else if (!Q_stricmp (token, "oneMinusEntity"))
		pass->rgbgen = RGB_GEN_ONE_MINUS_ENTITY;
	else if (!Q_stricmp (token, "vertex"))
		pass->rgbgen = RGB_GEN_VERTEX_LIGHTING;
	else if (!Q_stricmp (token, "oneMinusVertex"))
		pass->rgbgen = RGB_GEN_ONE_MINUS_VERTEX;
	else if (!Q_stricmp (token, "lightingDiffuse"))
		pass->rgbgen = RGB_GEN_LIGHTING_DIFFUSE;
	else if (!Q_stricmp (token, "exactvertex"))
		pass->rgbgen = RGB_GEN_VERTEX_EXACT;
	else if (!Q_stricmp (token, "const") || !Q_stricmp (token, "constant"))
	{
		pass->rgbgen = RGB_GEN_CONST;
		pass->rgbgen_func.type = SHADER_FUNC_CONSTANT;

		Shader_ParseVector (shader, ptr, pass->rgbgen_func.args);
	}
	else if (!Q_stricmp (token, "topcolor"))
		pass->rgbgen = RGB_GEN_TOPCOLOR;
	else if (!Q_stricmp (token, "bottomcolor"))
		pass->rgbgen = RGB_GEN_BOTTOMCOLOR;
}

static void Shaderpass_AlphaGen (shader_t *shader, shaderpass_t *pass, char **ptr)
{
	char		*token;

	token = Shader_ParseString(ptr);
	if (!Q_stricmp (token, "portal"))
	{
		pass->alphagen = ALPHA_GEN_PORTAL;
		shader->portaldist = Shader_ParseFloat(shader, ptr);
		if (!shader->portaldist)
			shader->portaldist = 256;
		shader->flags |= SHADER_AGEN_PORTAL;
	}
	else if (!Q_stricmp (token, "vertex"))
	{
		pass->alphagen = ALPHA_GEN_VERTEX;
	}
	else if (!Q_stricmp (token, "entity"))
	{
		pass->alphagen = ALPHA_GEN_ENTITY;
	}
	else if (!Q_stricmp (token, "wave"))
	{
		pass->alphagen = ALPHA_GEN_WAVE;

		Shader_ParseFunc (shader, ptr, &pass->alphagen_func);
	}
	else if ( !Q_stricmp (token, "lightingspecular"))
	{
		pass->alphagen = ALPHA_GEN_SPECULAR;
	}
	else if ( !Q_stricmp (token, "const") || !Q_stricmp (token, "constant"))
	{
		pass->alphagen = ALPHA_GEN_CONST;
		pass->alphagen_func.type = SHADER_FUNC_CONSTANT;
		pass->alphagen_func.args[0] = fabs(Shader_ParseFloat(shader, ptr));
	}
}
static void Shaderpass_AlphaShift (shader_t *shader, shaderpass_t *pass, char **ptr)	//for alienarena
{
	float speed;
	float min, max;
	pass->alphagen = ALPHA_GEN_WAVE;

	pass->alphagen_func.type = SHADER_FUNC_SIN;


	//arg0 = add
	//arg1 = scale
	//arg2 = timeshift
	//arg3 = timescale

	speed = Shader_ParseFloat(shader, ptr);
	min = Shader_ParseFloat(shader, ptr);
	max = Shader_ParseFloat(shader, ptr);

	pass->alphagen_func.args[0] = min + (max - min)/2;
	pass->alphagen_func.args[1] = (max - min)/2;
	pass->alphagen_func.args[2] = 0;
	pass->alphagen_func.args[3] = 1/speed;
}

static int Shader_BlendFactor(char *name, qboolean dstnotsrc)
{
	int factor;
	if (!strnicmp(name, "gl_", 3))
		name += 3;

	if (!Q_stricmp(name, "zero"))
		factor = SBITS_SRCBLEND_ZERO;
	else if ( !Q_stricmp(name, "one"))
		factor = SBITS_SRCBLEND_ONE;
	else if (!Q_stricmp(name, "dst_color"))
		factor = SBITS_SRCBLEND_DST_COLOR;
	else if (!Q_stricmp(name, "one_minus_src_alpha"))
		factor = SBITS_SRCBLEND_ONE_MINUS_SRC_ALPHA;
	else if (!Q_stricmp(name, "src_alpha"))
		factor = SBITS_SRCBLEND_SRC_ALPHA;
	else if (!Q_stricmp(name, "src_color"))
		factor = SBITS_SRCBLEND_SRC_COLOR_INVALID;
	else if (!Q_stricmp(name, "one_minus_dst_color"))
		factor = SBITS_SRCBLEND_ONE_MINUS_DST_COLOR;
	else if (!Q_stricmp(name, "one_minus_src_color"))
		factor = SBITS_SRCBLEND_ONE_MINUS_SRC_COLOR_INVALID;
	else if (!Q_stricmp(name, "dst_alpha") )
		factor = SBITS_SRCBLEND_DST_ALPHA;
	else if (!Q_stricmp(name, "one_minus_dst_alpha"))
		factor = SBITS_SRCBLEND_ONE_MINUS_DST_ALPHA;
	else
		factor = SBITS_SRCBLEND_NONE;

	if (dstnotsrc)
	{
		//dest factors are shifted
		factor <<= 4;

		/*gl doesn't accept dst_color for destinations*/
		if (factor == SBITS_DSTBLEND_NONE ||
			factor == SBITS_DSTBLEND_DST_COLOR_INVALID ||
			factor == SBITS_DSTBLEND_ONE_MINUS_DST_COLOR_INVALID ||
			factor == SBITS_DSTBLEND_ALPHA_SATURATE_INVALID)
		{
			Con_DPrintf("Invalid shader dst blend \"%s\"\n", name);
			factor = SBITS_DSTBLEND_ONE;
		}
	}
	else
	{
		/*gl doesn't accept src_color for sources*/
		if (factor == SBITS_SRCBLEND_NONE ||
			factor == SBITS_SRCBLEND_SRC_COLOR_INVALID ||
			factor == SBITS_SRCBLEND_ONE_MINUS_SRC_COLOR_INVALID)
		{
			Con_DPrintf("Unrecognised shader src blend \"%s\"\n", name);
			factor = SBITS_SRCBLEND_ONE;
		}
	}

	return factor;
}

static void Shaderpass_BlendFunc (shader_t *shader, shaderpass_t *pass, char **ptr)
{
	char		*token;

	//reset to defaults
	pass->shaderbits &= ~(SBITS_BLEND_BITS);
	pass->stagetype = ST_AMBIENT;

	token = Shader_ParseString (ptr);
	if ( !Q_stricmp (token, "bumpmap"))				//doom3 is awkward...
		pass->stagetype = ST_BUMPMAP;
	else if ( !Q_stricmp (token, "specularmap"))	//doom3 is awkward...
		pass->stagetype = ST_SPECULARMAP;
	else if ( !Q_stricmp (token, "diffusemap"))		//doom3 is awkward...
		pass->stagetype = ST_DIFFUSEMAP;
	else if ( !Q_stricmp (token, "blend"))
		pass->shaderbits |= SBITS_SRCBLEND_SRC_ALPHA | SBITS_DSTBLEND_ONE_MINUS_SRC_ALPHA;
	else if (!Q_stricmp (token, "filter"))
		pass->shaderbits |= SBITS_SRCBLEND_DST_COLOR | SBITS_DSTBLEND_ZERO;
	else if (!Q_stricmp (token, "add"))
		pass->shaderbits |= SBITS_SRCBLEND_ONE | SBITS_DSTBLEND_ONE;
	else if (!Q_stricmp (token, "replace"))
		pass->shaderbits |= SBITS_SRCBLEND_NONE | SBITS_DSTBLEND_NONE;
	else
	{
		pass->shaderbits |= Shader_BlendFactor(token, false);

		token = Shader_ParseString (ptr);
		if (*token == ',')
			token = Shader_ParseString (ptr);
		pass->shaderbits |= Shader_BlendFactor(token, true);
	}
}

static void Shaderpass_AlphaFunc (shader_t *shader, shaderpass_t *pass, char **ptr)
{
	char *token;

	pass->shaderbits &= ~SBITS_ATEST_BITS;

	token = Shader_ParseString (ptr);
	if (!Q_stricmp (token, "gt0"))
	{
		pass->shaderbits = SBITS_ATEST_GT0;
	}
	else if (!Q_stricmp (token, "lt128"))
	{
		pass->shaderbits = SBITS_ATEST_LT128;
	}
	else if (!Q_stricmp (token, "ge128"))
	{
		pass->shaderbits = SBITS_ATEST_GE128;
	}
}

static void Shaderpass_DepthFunc (shader_t *shader, shaderpass_t *pass, char **ptr)
{
	char *token;

	token = Shader_ParseString (ptr);
	if (!Q_stricmp (token, "equal"))
		pass->shaderbits |= SBITS_MISC_DEPTHEQUALONLY;
	else if (!Q_stricmp (token, "lequal"))
		pass->shaderbits &= ~SBITS_MISC_DEPTHEQUALONLY;
	else
		Con_DPrintf("Invalid depth func %s\n", token);
}

static void Shaderpass_DepthWrite (shader_t *shader, shaderpass_t *pass, char **ptr)
{
	shader->flags |= SHADER_DEPTHWRITE;
	pass->shaderbits |= SBITS_MISC_DEPTHWRITE;
}

static void Shaderpass_NoDepthTest (shader_t *shader, shaderpass_t *pass, char **ptr)
{
	shader->flags |= SHADER_DEPTHWRITE;
	pass->shaderbits |= SBITS_MISC_NODEPTHTEST;
}

static void Shaderpass_NoDepth (shader_t *shader, shaderpass_t *pass, char **ptr)
{
	shader->flags |= SHADER_DEPTHWRITE;
}

static void Shaderpass_TcMod (shader_t *shader, shaderpass_t *pass, char **ptr)
{
	int i;
	tcmod_t *tcmod;
	char *token;

	if (pass->numtcmods >= SHADER_MAX_TC_MODS)
	{
		return;
	}

	tcmod = &pass->tcmods[pass->numtcmods];

	token = Shader_ParseString (ptr);
	if (!Q_stricmp (token, "rotate"))
	{
		tcmod->args[0] = -Shader_ParseFloat(shader, ptr) / 360.0f;
		if (!tcmod->args[0])
		{
			return;
		}

		tcmod->type = SHADER_TCMOD_ROTATE;
	}
	else if ( !Q_stricmp (token, "scale") )
	{
		tcmod->args[0] = Shader_ParseFloat (shader, ptr);
		tcmod->args[1] = Shader_ParseFloat (shader, ptr);
		tcmod->type = SHADER_TCMOD_SCALE;
	}
	else if ( !Q_stricmp (token, "scroll") )
	{
		tcmod->args[0] = Shader_ParseFloat (shader, ptr);
		tcmod->args[1] = Shader_ParseFloat (shader, ptr);
		tcmod->type = SHADER_TCMOD_SCROLL;
	}
	else if (!Q_stricmp(token, "stretch"))
	{
		shaderfunc_t func;

		Shader_ParseFunc(shader, ptr, &func);

		tcmod->args[0] = func.type;
		for (i = 1; i < 5; ++i)
			tcmod->args[i] = func.args[i-1];
		tcmod->type = SHADER_TCMOD_STRETCH;
	}
	else if (!Q_stricmp (token, "transform"))
	{
		for (i = 0; i < 6; ++i)
			tcmod->args[i] = Shader_ParseFloat (shader, ptr);
		tcmod->type = SHADER_TCMOD_TRANSFORM;
	}
	else if (!Q_stricmp (token, "turb"))
	{
		for (i = 0; i < 4; i++)
			tcmod->args[i] = Shader_ParseFloat (shader, ptr);
		tcmod->type = SHADER_TCMOD_TURB;
	}
	else
	{
		return;
	}

	pass->numtcmods++;
}

static void Shaderpass_Scale ( shader_t *shader, shaderpass_t *pass, char **ptr )
{
	//seperate x and y
	char *token;
	tcmod_t *tcmod;

	tcmod = &pass->tcmods[pass->numtcmods];

	tcmod->type = SHADER_TCMOD_SCALE;

	token = Shader_ParseString (ptr);
	if (!strcmp(token, "static"))
	{
		tcmod->args[0] = Shader_ParseFloat (shader, ptr);
	}
	else
	{
		tcmod->args[0] = atof(token);
	}

	while (**ptr == ' ' || **ptr == '\t')
		*ptr+=1;
	if (**ptr == ',')
		*ptr+=1;

	token = Shader_ParseString (ptr);
	if (!strcmp(token, "static"))
	{
		tcmod->args[1] = Shader_ParseFloat (shader, ptr);
	}
	else
	{
		tcmod->args[1] = atof(token);
	}

	pass->numtcmods++;
}

static void Shaderpass_Scroll (shader_t *shader, shaderpass_t *pass, char **ptr)
{
	//seperate x and y
	char *token;
	tcmod_t *tcmod;

	tcmod = &pass->tcmods[pass->numtcmods];

	token = Shader_ParseString ( ptr );
	if (!strcmp(token, "static"))
	{
		tcmod->type = SHADER_TCMOD_SCROLL;
		tcmod->args[0] = Shader_ParseFloat (shader, ptr );
	}
	else
	{
		Con_Printf("Bad shader scale\n");
		return;
	}

	token = Shader_ParseString ( ptr );
	if (!strcmp(token, "static"))
	{
		tcmod->type = SHADER_TCMOD_SCROLL;
		tcmod->args[1] = Shader_ParseFloat (shader, ptr );
	}
	else
	{
		Con_Printf("Bad shader scale\n");
		return;
	}

	pass->numtcmods++;
}


static void Shaderpass_TcGen ( shader_t *shader, shaderpass_t *pass, char **ptr )
{
	char *token;

	token = Shader_ParseString ( ptr );
	if ( !Q_stricmp (token, "base") ) {
		pass->tcgen = TC_GEN_BASE;
	} else if ( !Q_stricmp (token, "lightmap") ) {
		pass->tcgen = TC_GEN_LIGHTMAP;
	} else if ( !Q_stricmp (token, "environment") ) {
		pass->tcgen = TC_GEN_ENVIRONMENT;
	} else if ( !Q_stricmp (token, "vector") ) {
		pass->tcgen = TC_GEN_BASE;
	} else if ( !Q_stricmp (token, "normal") ) {
		pass->tcgen = TC_GEN_NORMAL;
	} else if ( !Q_stricmp (token, "svector") ) {
		pass->tcgen = TC_GEN_SVECTOR;
	} else if ( !Q_stricmp (token, "tvector") ) {
		pass->tcgen = TC_GEN_TVECTOR;
	}
}
static void Shaderpass_EnvMap ( shader_t *shader, shaderpass_t *pass, char **ptr )	//for alienarena
{
	pass->tcgen = TC_GEN_ENVIRONMENT;
}

static void Shaderpass_Detail ( shader_t *shader, shaderpass_t *pass, char **ptr )
{
	pass->flags |= SHADER_PASS_DETAIL;
}

static void Shaderpass_AlphaMask ( shader_t *shader, shaderpass_t *pass, char **ptr )
{
	pass->shaderbits &= ~SBITS_ATEST_BITS;
	pass->shaderbits |= SBITS_ATEST_GE128;
}

static void Shaderpass_NoLightMap ( shader_t *shader, shaderpass_t *pass, char **ptr )
{
	pass->rgbgen = RGB_GEN_IDENTITY;
}

static void Shaderpass_Red(shader_t *shader, shaderpass_t *pass, char **ptr)
{
	pass->rgbgen = RGB_GEN_CONST;
	pass->rgbgen_func.args[0] = Shader_ParseFloat(shader, ptr);
}
static void Shaderpass_Green(shader_t *shader, shaderpass_t *pass, char **ptr)
{
	pass->rgbgen = RGB_GEN_CONST;
	pass->rgbgen_func.args[1] = Shader_ParseFloat(shader, ptr);
}
static void Shaderpass_Blue(shader_t *shader, shaderpass_t *pass, char **ptr)
{
	pass->rgbgen = RGB_GEN_CONST;
	pass->rgbgen_func.args[2] = Shader_ParseFloat(shader, ptr);
}
static void Shaderpass_Alpha(shader_t *shader, shaderpass_t *pass, char **ptr)
{
	pass->alphagen = ALPHA_GEN_CONST;
	pass->alphagen_func.args[0] = Shader_ParseFloat(shader, ptr);
}
static void Shaderpass_MaskColor(shader_t *shader, shaderpass_t *pass, char **ptr)
{
	pass->shaderbits |= SBITS_MASK_RED|SBITS_MASK_GREEN|SBITS_MASK_BLUE;
}
static void Shaderpass_MaskRed(shader_t *shader, shaderpass_t *pass, char **ptr)
{
	pass->shaderbits |= SBITS_MASK_RED;
}
static void Shaderpass_MaskGreen(shader_t *shader, shaderpass_t *pass, char **ptr)
{
	pass->shaderbits |= SBITS_MASK_GREEN;
}
static void Shaderpass_MaskBlue(shader_t *shader, shaderpass_t *pass, char **ptr)
{
	pass->shaderbits |= SBITS_MASK_BLUE;
}
static void Shaderpass_MaskAlpha(shader_t *shader, shaderpass_t *pass, char **ptr)
{
	pass->shaderbits |= SBITS_MASK_ALPHA;
}
static void Shaderpass_AlphaTest(shader_t *shader, shaderpass_t *pass, char **ptr)
{
	if (Shader_ParseFloat(shader, ptr) == 0.5)
		pass->shaderbits |= SBITS_ATEST_GE128;
	else
		Con_Printf("unsupported alphatest value\n");
}
static void Shaderpass_TexGen(shader_t *shader, shaderpass_t *pass, char **ptr)
{
	char *token = Shader_ParseString(ptr);
	if (!strcmp(token, "normal"))
		pass->tcgen = TC_GEN_NORMAL;
	else if (!strcmp(token, "skybox"))
		pass->tcgen = TC_GEN_SKYBOX;
	else if (!strcmp(token, "wobblesky"))
	{
		pass->tcgen = TC_GEN_WOBBLESKY;
		token = Shader_ParseString(ptr);
		token = Shader_ParseString(ptr);
		token = Shader_ParseString(ptr);
	}
	else if (!strcmp(token, "reflect"))
		pass->tcgen = TC_GEN_REFLECT;
	else
	{
		Con_Printf("texgen token not understood\n");
	}
}
static void Shaderpass_CubeMap(shader_t *shader, shaderpass_t *pass, char **ptr)
{
	char *token = Shader_ParseString(ptr);

	if (pass->tcgen == TC_GEN_BASE)
		pass->tcgen = TC_GEN_SKYBOX;
	pass->texgen = T_GEN_CUBEMAP;
	pass->anim_frames[0] = Shader_FindImage(token, IF_CUBEMAP);

	if (!TEXVALID(pass->anim_frames[0]))
	{
		pass->texgen = T_GEN_SINGLEMAP;
		pass->anim_frames[0] = missing_texture;
	}
}

static shaderkey_t shaderpasskeys[] =
{
	{"rgbgen",		Shaderpass_RGBGen },
	{"blendfunc",	Shaderpass_BlendFunc },
	{"depthfunc",	Shaderpass_DepthFunc },
	{"depthwrite",	Shaderpass_DepthWrite },
	{"nodepthtest",	Shaderpass_NoDepthTest },
	{"nodepth",		Shaderpass_NoDepth },
	{"alphafunc",	Shaderpass_AlphaFunc },
	{"tcmod",		Shaderpass_TcMod },
	{"map",			Shaderpass_Map },
	{"animmap",		Shaderpass_AnimMap },
	{"clampmap",	Shaderpass_ClampMap },
	{"videomap",	Shaderpass_VideoMap },
	{"tcgen",		Shaderpass_TcGen },
	{"envmap",		Shaderpass_EnvMap },//for alienarena
	{"nolightmap",	Shaderpass_NoLightMap },//for alienarena
	{"scale",		Shaderpass_Scale },//for alienarena
	{"scroll",		Shaderpass_Scroll },//for alienarena
	{"alphagen",	Shaderpass_AlphaGen },
	{"alphashift",	Shaderpass_AlphaShift },//for alienarena
	{"alphamask",	Shaderpass_AlphaMask },//for alienarena
	{"detail",		Shaderpass_Detail },

	/*doom3 compat*/
	{"blend",		Shaderpass_BlendFunc},
	{"maskcolor",	Shaderpass_MaskColor},
	{"maskred",		Shaderpass_MaskRed},
	{"maskgreen",	Shaderpass_MaskGreen},
	{"maskblue",	Shaderpass_MaskBlue},
	{"maskalpha",	Shaderpass_MaskAlpha},
	{"alphatest",	Shaderpass_AlphaTest},
	{"texgen",		Shaderpass_TexGen},
	{"cubemap",		Shaderpass_CubeMap},	//one of these is wrong
	{"cameracubemap",Shaderpass_CubeMap},	//one of these is wrong
	{"red",			Shaderpass_Red},
	{"green",		Shaderpass_Green},
	{"blue",		Shaderpass_Blue},
	{"alpha",		Shaderpass_Alpha},
	{NULL,			NULL}
};

// ===============================================================


void Shader_FreePass (shaderpass_t *pass)
{
#ifndef NOMEDIA
	if ( pass->flags & SHADER_PASS_VIDEOMAP )
	{
		Media_ShutdownCin(pass->cin);
		pass->cin = NULL;
	}
#endif
}

void Shader_Free (shader_t *shader)
{
	int i;
	shaderpass_t *pass;

	if (shader->bucket.data == shader)
		Hash_RemoveData(&shader_active_hash, shader->name, shader);
	shader->bucket.data = NULL;

	if (shader->prog)
	{
		if (shader->prog->refs-- == 1)
			Shader_UnloadProg(shader->prog);
	}
	shader->prog = NULL;

	if (shader->skydome)
	{
		Z_Free (shader->skydome);
	}

	pass = shader->passes;
	for (i = 0; i < shader->numpasses; i++, pass++)
	{
		Shader_FreePass (pass);
	}
	shader->numpasses = 0;

	if (shader->genargs)
	{
		free(shader->genargs);
		shader->genargs = NULL;
	}
	shader->uses = 0;

	memset(&shader->defaulttextures, 0, sizeof(shader->defaulttextures));
}





int QDECL Shader_InitCallback (const char *name, qofs_t size, void *param, searchpathfuncs_t *spath)
{
	Shader_MakeCache(name);
	return true;
}

qboolean Shader_Init (void)
{
	int wibuf[16];

	if (!r_shaders)
	{
		r_numshaders = 0;
		r_maxshaders = 256;
		r_shaders = calloc(r_maxshaders, sizeof(*r_shaders));

		shader_hash = calloc (HASH_SIZE, sizeof(*shader_hash));

		shader_active_hash_mem = malloc(Hash_BytesForBuckets(1024));
		memset(shader_active_hash_mem, 0, Hash_BytesForBuckets(1024));
		Hash_InitTable(&shader_active_hash, 1024, shader_active_hash_mem);

		Shader_FlushGenerics();
	}
	Shader_NeedReload(true);
	Shader_DoReload();

	memset(wibuf, 0xff, sizeof(wibuf));
	if (!qrenderer)
		r_whiteimage = r_nulltex;
	else
		r_whiteimage = R_LoadTexture("$whiteimage", 4, 4, TF_RGBA32, wibuf, IF_NOMIPMAP|IF_NOPICMIP|IF_NEAREST|IF_NOGAMMA);
	return true;
}

void Shader_FlushCache(void)
{
	shadercachefile_t *sf;
	shadercache_t *cache, *cache_next;
	int i;

	for (i = 0; i < HASH_SIZE; i++)
	{
		cache = shader_hash[i];
		shader_hash[i] = NULL;

		for (; cache; cache = cache_next)
		{
			cache_next = cache->hash_next;
			cache->hash_next = NULL;
			Z_Free(cache);
		}
	}

	while(shaderfiles)
	{
		sf = shaderfiles;
		shaderfiles = sf->next;
		if (sf->data)
			FS_FreeFile(sf->data);
		Z_Free(sf);
	}
}

static void Shader_MakeCache(const char *path)
{
	unsigned int key;
	char *buf, *ptr, *token;
	shadercache_t *cache;
	shadercachefile_t *cachefile, *filelink = NULL;
	qofs_t size;

	for (cachefile = shaderfiles; cachefile; cachefile = cachefile->next)
	{
		if (!Q_stricmp(cachefile->name, path))
			return;	//already loaded. there's no source package or anything.
		filelink = cachefile;
	}

	
	Con_DPrintf ("...loading '%s'\n", path);

	cachefile = Z_Malloc(sizeof(*cachefile) + strlen(path));
	strcpy(cachefile->name, path);
	size = FS_LoadFile(path, (void **)&cachefile->data);
	cachefile->length = size;
	if (filelink)
		filelink->next = cachefile;
	else
		shaderfiles = cachefile;

	if (qofs_Error(size))
	{
		Con_Printf("Unable to read %s\n", path);
		cachefile->length = 0;
		return;
	}
	if (size > 1024*1024*64)	//sanity limit
	{
		Con_Printf("Refusing to parse %s due to size\n", path);
		cachefile->length = 0;
		FS_FreeFile(cachefile->data);
		cachefile->data = NULL;
		return;
	}

	ptr = buf = cachefile->data;
	size = cachefile->length;
	do
	{
		if ( ptr - buf >= size )
			break;

		token = COM_ParseExt (&ptr, true, true);
		if ( !token[0] || ptr - buf >= size )
			break;

		COM_CleanUpPath(token);

		if (Shader_LocateSource(token, NULL, NULL, NULL, NULL))
		{
			ptr = Shader_Skip ( ptr );
			continue;
		}

		key = Hash_Key ( token, HASH_SIZE );

		cache = ( shadercache_t * )Z_Malloc(sizeof(shadercache_t) + strlen(token));
		strcpy(cache->name, token);
		cache->hash_next = shader_hash[key];
		cache->source = cachefile;
		cache->offset = ptr - cachefile->data;

		shader_hash[key] = cache;

		ptr = Shader_Skip ( ptr );
	} while ( ptr );
}

static qboolean Shader_LocateSource(char *name, char **buf, size_t *bufsize, size_t *offset, enum shaderparsemode_e *parsemode)
{
	unsigned int key;
	shadercache_t *cache;

	key = Hash_Key ( name, HASH_SIZE );
	cache = shader_hash[key];

	for ( ; cache; cache = cache->hash_next )
	{
		if ( !Q_stricmp (cache->name, name) )
		{
			if (buf)
			{
				*buf = cache->source->data;
				*bufsize = cache->source->length;
				*offset = cache->offset;
				*parsemode = cache->source->parsemode;
			}
			return true;
		}
	}
	return false;
}

char *Shader_Skip ( char *ptr )
{
	char *tok;
	int brace_count;

    // Opening brace
	tok = COM_ParseExt(&ptr, true, true);

	if (!ptr)
		return NULL;

	if ( tok[0] != '{' )
	{
		tok = COM_ParseExt (&ptr, true, true);
	}

	for (brace_count = 1; brace_count > 0 ; ptr++)
	{
		tok = COM_ParseExt (&ptr, true, true);

		if ( !tok[0] )
			return NULL;

		if (tok[0] == '{')
		{
			brace_count++;
		} else if (tok[0] == '}')
		{
			brace_count--;
		}
	}

	return ptr;
}

void Shader_Reset(shader_t *s)
{
	char name[MAX_QPATH];
	int id = s->id;
	int uses = s->uses;
	shader_gen_t *defaultgen = s->generator;
	char *genargs = s->genargs;
	texnums_t dt = s->defaulttextures;
	int w = s->width;
	int h = s->height;
	unsigned int uf = s->usageflags;
	Q_strncpyz(name, s->name, sizeof(name));
	s->genargs = NULL;
	Shader_Free(s);
	memset(s, 0, sizeof(*s));

	s->flags |= SHADER_IMAGEPENDING;

	s->remapto = s;
	s->id = id;
	s->width = w;
	s->height = h;
	s->defaulttextures = dt;
	s->generator = defaultgen;
	s->genargs = genargs;
	s->usageflags = uf;
	s->uses = uses;
	Q_strncpyz(s->name, name, sizeof(s->name));
	Hash_Add(&shader_active_hash, s->name, s, &s->bucket);
}

void Shader_Shutdown (void)
{
	int i;
	shader_t *shader;

	if (!r_shaders)
		return;	/*nothing needs freeing yet*/
	for (i = 0; i < r_numshaders; i++)
	{
		shader = r_shaders[i];
		if (!shader)
			continue;

		Shader_Free(shader);
		Z_Free(r_shaders[i]);
		r_shaders[i] = NULL;
	}

	Shader_FlushCache();
	Shader_FlushGenerics();

	r_maxshaders = 0;
	r_numshaders = 0;

	free(r_shaders);
	r_shaders = NULL;
	free(shader_hash);
	shader_hash = NULL;
	free(shader_active_hash_mem);
	shader_active_hash_mem = NULL;

	shader_reload_needed = false;
}

void Shader_SetBlendmode (shaderpass_t *pass)
{
	if (pass->texgen == T_GEN_DELUXMAP)
	{
		pass->blendmode = PBM_DOTPRODUCT;
		return;
	}

	if (pass->texgen < T_GEN_DIFFUSE && !TEXVALID(pass->anim_frames[0]) && !(pass->flags & SHADER_PASS_LIGHTMAP))
	{
		pass->blendmode = PBM_MODULATE;
		return;
	}

	if (!(pass->shaderbits & SBITS_BLEND_BITS))
	{
		if ((pass->rgbgen == RGB_GEN_IDENTITY) && (pass->alphagen == ALPHA_GEN_IDENTITY))
		{
			pass->blendmode = PBM_REPLACE;
			return;
		}
		else if ((pass->rgbgen == RGB_GEN_IDENTITY_LIGHTING) && (pass->alphagen == ALPHA_GEN_IDENTITY))
		{
			pass->shaderbits &= ~SBITS_BLEND_BITS;
			pass->shaderbits |= SBITS_SRCBLEND_ONE;
			pass->shaderbits |= SBITS_DSTBLEND_ZERO;
			pass->blendmode = PBM_REPLACELIGHT;
		}
		else
		{
			pass->shaderbits &= ~SBITS_BLEND_BITS;
			pass->shaderbits |= SBITS_SRCBLEND_ONE;
			pass->shaderbits |= SBITS_DSTBLEND_ZERO;
			pass->blendmode = PBM_MODULATE;
		}
		return;
	}

	if (((pass->shaderbits&SBITS_BLEND_BITS) == (SBITS_SRCBLEND_ZERO|SBITS_DSTBLEND_SRC_COLOR)) ||
		((pass->shaderbits&SBITS_BLEND_BITS) == (SBITS_SRCBLEND_DST_COLOR|SBITS_DSTBLEND_ZERO)))
		pass->blendmode = PBM_MODULATE;
	else if ((pass->shaderbits&SBITS_BLEND_BITS) == (SBITS_SRCBLEND_ONE|SBITS_DSTBLEND_ONE))
		pass->blendmode = PBM_ADD;
	else if ((pass->shaderbits&SBITS_BLEND_BITS) == (SBITS_SRCBLEND_SRC_ALPHA|SBITS_DSTBLEND_ONE_MINUS_SRC_ALPHA))
		pass->blendmode = PBM_DECAL;
	else
		pass->blendmode = PBM_MODULATE;
}

void Shader_Readpass (shader_t *shader, char **ptr)
{
	char *token;
	shaderpass_t *pass;
	qboolean ignore;
	static shader_t dummy;
	int conddepth = 0;
	int cond[8] = {0};
#define COND_IGNORE 1
#define COND_IGNOREPARENT 2
#define COND_ALLOWELSE 4

	if ( shader->numpasses >= SHADER_PASS_MAX )
	{
		ignore = true;
		shader = &dummy;
		shader->numpasses = 1;
		pass = shader->passes;
	}
	else
	{
		ignore = false;
		pass = &shader->passes[shader->numpasses++];
	}

    // Set defaults
	pass->flags = 0;
	pass->anim_frames[0] = r_nulltex;
	pass->anim_numframes = 0;
	pass->rgbgen = RGB_GEN_UNKNOWN;
	pass->alphagen = ALPHA_GEN_IDENTITY;
	pass->tcgen = TC_GEN_UNSPECIFIED;
	pass->numtcmods = 0;
	pass->numMergedPasses = 1;
	pass->stagetype = ST_AMBIENT;

	if (shader->flags & SHADER_NOMIPMAPS)
		pass->flags |= SHADER_PASS_NOMIPMAP;

	while ( *ptr )
	{
		token = COM_ParseExt (ptr, true, true);

		if ( !token[0] )
		{
			continue;
		}
		else if (!Q_stricmp(token, "if"))
		{
			if (conddepth+1 == sizeof(cond)/sizeof(cond[0]))
			{
				Con_Printf("if statements nest too deeply in shader %s\n", shader->name);
				break;
			}
			conddepth++;
			cond[conddepth] = (Shader_EvaluateCondition(shader, ptr)?0:COND_IGNORE);
			cond[conddepth] |= COND_ALLOWELSE;
			if (cond[conddepth-1] & (COND_IGNORE|COND_IGNOREPARENT))
				cond[conddepth] |= COND_IGNOREPARENT;
		}
		else if (!Q_stricmp(token, "endif"))
		{
			if (!conddepth)
			{
				Con_Printf("endif without if in shader %s\n", shader->name);
				break;
			}
			conddepth--;
		}
		else if (!Q_stricmp(token, "else"))
		{
			if (cond[conddepth] & COND_ALLOWELSE)
			{
				cond[conddepth] ^= COND_IGNORE;
				cond[conddepth] &= ~COND_ALLOWELSE;
			}
			else
				Con_Printf("unexpected else statement in shader %s\n", shader->name);
		}
		else if (cond[conddepth] & (COND_IGNORE|COND_IGNOREPARENT))
		{
			//eat it
			while (ptr)
			{
				token = COM_ParseExt(ptr, false, true);
				if ( !token[0] )
					break;
			}
		}
		else
		{
			if ( token[0] == '}' )
				break;
			else if ( Shader_Parsetok (shader, pass, shaderpasskeys, token, ptr) )
				break;
		}
	}

	if (conddepth)
	{
		Con_Printf("if statements without endif in shader %s\n", shader->name);
	}

	if (pass->tcgen == TC_GEN_UNSPECIFIED)
		pass->tcgen = TC_GEN_BASE;

	if (!ignore)
	{
		switch(pass->stagetype)
		{
		case ST_DIFFUSEMAP:
			if (pass->texgen == T_GEN_SINGLEMAP)
				shader->defaulttextures.base = pass->anim_frames[0];
			break;
		case ST_AMBIENT:
			break;
		case ST_BUMPMAP:
			if (pass->texgen == T_GEN_SINGLEMAP)
				shader->defaulttextures.bump = pass->anim_frames[0];
			ignore = true;
			break;
		case ST_SPECULARMAP:
			if (pass->texgen == T_GEN_SINGLEMAP)
				shader->defaulttextures.specular = pass->anim_frames[0];
			ignore = true;
			break;
		}
	}

	// check some things
	if (ignore)
	{
		Shader_FreePass (pass);
		shader->numpasses--;
		return;
	}

	if ((pass->shaderbits&SBITS_BLEND_BITS) == (SBITS_SRCBLEND_ONE|SBITS_DSTBLEND_ZERO))
	{
		pass->shaderbits |= SBITS_MISC_DEPTHWRITE;
		shader->flags |= SHADER_DEPTHWRITE;
	}

	switch (pass->rgbgen)
	{
		case RGB_GEN_IDENTITY_LIGHTING:
		case RGB_GEN_IDENTITY:
		case RGB_GEN_CONST:
		case RGB_GEN_WAVE:
		case RGB_GEN_ENTITY:
		case RGB_GEN_ONE_MINUS_ENTITY:
		case RGB_GEN_UNKNOWN:	// assume RGB_GEN_IDENTITY or RGB_GEN_IDENTITY_LIGHTING

			switch (pass->alphagen)
			{
				case ALPHA_GEN_IDENTITY:
				case ALPHA_GEN_CONST:
				case ALPHA_GEN_WAVE:
				case ALPHA_GEN_ENTITY:
					pass->flags |= SHADER_PASS_NOCOLORARRAY;
					break;
				default:
					break;
			}

			break;
		default:
			break;
	}

	/*if ((shader->flags & SHADER_SKY) && (shader->flags & SHADER_DEPTHWRITE))
	{
#ifdef warningmsg
#pragma warningmsg("is this valid?")
#endif
		pass->shaderbits &= ~SBITS_MISC_DEPTHWRITE;
	}
	*/
}

static qboolean Shader_Parsetok (shader_t *shader, shaderpass_t *pass, shaderkey_t *keys, char *token, char **ptr)
{
	shaderkey_t *key;

	for (key = keys; key->keyword != NULL; key++)
	{
		if (!Q_stricmp (token, key->keyword))
		{
			if (key->func)
				key->func ( shader, pass, ptr );

			return ( ptr && *ptr && **ptr == '}' );
		}
	}

//	Con_Printf("Unknown shader directive: \"%s\"\n", token);

	// Next Line
	while (ptr)
	{
		token = COM_ParseExt(ptr, false, true);
		if ( !token[0] )
		{
			break;
		}
	}

	return false;
}

void Shader_SetPassFlush (shaderpass_t *pass, shaderpass_t *pass2)
{
	if (((pass->flags & SHADER_PASS_DETAIL) && !r_detailtextures.value) ||
		((pass2->flags & SHADER_PASS_DETAIL) && !r_detailtextures.value) ||
		 (pass->flags & SHADER_PASS_VIDEOMAP) || (pass2->flags & SHADER_PASS_VIDEOMAP))
	{
		return;
	}

	/*identity alpha is required for merging*/
	if (pass->alphagen != ALPHA_GEN_IDENTITY || pass2->alphagen != ALPHA_GEN_IDENTITY)
		return;

	/*rgbgen must be identity too except if the later pass is identity_ligting, in which case all is well and we can switch the first pass to identity_lighting instead*/
	if (pass2->rgbgen == RGB_GEN_IDENTITY_LIGHTING && pass2->blendmode == PBM_MODULATE && pass->rgbgen == RGB_GEN_IDENTITY)
	{
		pass->blendmode = PBM_REPLACELIGHT;
		pass->rgbgen = RGB_GEN_IDENTITY_LIGHTING;
		pass2->rgbgen = RGB_GEN_IDENTITY;
	}
	/*rgbgen must be identity (or the first is identity_lighting)*/
	else if (pass2->rgbgen != RGB_GEN_IDENTITY || (pass->rgbgen != RGB_GEN_IDENTITY && pass->rgbgen != RGB_GEN_IDENTITY_LIGHTING))
		return;

	/*if its alphatest, don't merge with anything other than lightmap*/
	if ((pass->shaderbits & SBITS_ATEST_BITS) && (!(pass2->shaderbits & SBITS_MISC_DEPTHEQUALONLY) || pass2->texgen != T_GEN_LIGHTMAP))
		return;

	if ((pass->shaderbits & SBITS_MASK_BITS) != (pass2->shaderbits & SBITS_MASK_BITS))
		return;

	/*don't merge passes if the hardware cannot support it*/
	if (pass->numMergedPasses >= be_maxpasses)
		return;

	// check if we can use multiple passes
	if (pass2->blendmode == PBM_DOTPRODUCT)
	{
		pass->numMergedPasses++;
	}
	else if (pass->numMergedPasses < be_maxpasses)
	{
		if (pass->blendmode == PBM_REPLACE || pass->blendmode == PBM_REPLACELIGHT)
		{
			if ((pass2->blendmode == PBM_DECAL && sh_config.tex_env_combine) ||
				(pass2->blendmode == PBM_ADD && sh_config.env_add) ||
				(pass2->blendmode && pass2->blendmode != PBM_ADD) ||	sh_config.nv_tex_env_combine4)
			{
				pass->numMergedPasses++;
			}
		}
		else if (pass->blendmode == PBM_ADD &&
			pass2->blendmode == PBM_ADD && sh_config.env_add)
		{
			pass->numMergedPasses++;
		}
		else if (pass->blendmode == PBM_MODULATE && pass2->blendmode == PBM_MODULATE)
		{
			pass->numMergedPasses++;
		}
		else
			return;
	}
	else return;

	if (pass->texgen == T_GEN_LIGHTMAP && pass->blendmode == PBM_REPLACELIGHT && pass2->blendmode == PBM_MODULATE && sh_config.tex_env_combine)
	{
		if (pass->rgbgen == RGB_GEN_IDENTITY)
			pass->rgbgen = RGB_GEN_IDENTITY_OVERBRIGHT;	//get the light levels right
		pass2->blendmode = PBM_OVERBRIGHT;
	}
	if (pass2->texgen == T_GEN_LIGHTMAP && pass2->blendmode == PBM_MODULATE && sh_config.tex_env_combine)
	{
		if (pass->rgbgen == RGB_GEN_IDENTITY)
			pass->rgbgen = RGB_GEN_IDENTITY_OVERBRIGHT;	//get the light levels right
		pass->blendmode = PBM_REPLACELIGHT;
		pass2->blendmode = PBM_OVERBRIGHT;
	}
}

const char *Shader_AlphaMaskProgArgs(shader_t *s)
{
	if (s->numpasses)
	{
		//alpha mask values ALWAYS come from the first pass.
		shaderpass_t *pass = &s->passes[0];
		switch(pass->shaderbits & SBITS_ATEST_BITS)
		{
		default:
			break;
		//cases inverted. the test is to enable 
		case SBITS_ATEST_GT0:
			return "#MASK=0.0#MASKOP=>";
		case SBITS_ATEST_LT128:
			return "#MASK=0.5#MASKOP=<";
		case SBITS_ATEST_GE128:
			return "#MASK=0.5";
		}
	}
	return "";
}

void Shader_Programify (shader_t *s)
{
	char *prog = NULL;
	const char *mask;
/*	enum
	{
		T_UNKNOWN,
		T_WALL,
		T_MODEL
	} type = 0;*/
	int i;
	shaderpass_t *pass, *lightmap = NULL, *modellighting = NULL;
	for (i = 0; i < s->numpasses; i++)
	{
		pass = &s->passes[i];
		if (pass->rgbgen == RGB_GEN_LIGHTING_DIFFUSE)
			modellighting = pass;
		else if (pass->texgen == T_GEN_LIGHTMAP && pass->tcgen == TC_GEN_LIGHTMAP)
			lightmap = pass;
	}

	if (modellighting)
	{
		pass = modellighting;
		prog = "defaultskin";
	}
	else if (lightmap)
	{
		pass = modellighting;
		prog = "defaultwall";
	}
	else
	{
		pass = NULL;
		prog = "default2d";
		return;
	}

	mask = Shader_AlphaMaskProgArgs(s);

	s->prog = Shader_FindGeneric(va("%s%s", prog, mask), qrenderer);
	s->numpasses = 0;
	s->passes[s->numpasses++].texgen = T_GEN_DIFFUSE;

	if (lightmap)
	{
		s->passes[s->numpasses++].texgen = T_GEN_LIGHTMAP;
		s->passes[s->numpasses++].texgen = T_GEN_NORMALMAP;
		s->passes[s->numpasses++].texgen = T_GEN_DELUXMAP;
		s->passes[s->numpasses++].texgen = T_GEN_FULLBRIGHT;
		s->passes[s->numpasses++].texgen = T_GEN_SPECULAR;
	}

	if (modellighting)
	{
		s->passes[s->numpasses++].texgen = T_GEN_LOWEROVERLAY;
		s->passes[s->numpasses++].texgen = T_GEN_UPPEROVERLAY;
		s->passes[s->numpasses++].texgen = T_GEN_FULLBRIGHT;
		s->passes[s->numpasses++].texgen = T_GEN_NORMALMAP;
		s->passes[s->numpasses++].texgen = T_GEN_SPECULAR;
	}
}

void Shader_Finish (shader_t *s)
{
	int i;
	shaderpass_t *pass;
	
	//FIXME: reorder doom3 stages.
	//put diffuse first. give it a lightmap pass also, if we found a diffuse one with no lightmap.
	//then the ambient stages.
	//and forget about the bump/specular stages as we don't support them and already stripped them.

	if (s->flags & SHADER_SKY)
	{
		/*skies go all black if fastsky is set*/
		if (r_fastsky.ival)
			s->flags = 0;
		/*or if its purely a skybox and has missing textures*/
//		if (!s->numpasses)
//			for (i = 0; i < 6; i++)
//				if (missing_texture.ref == s->skydome->farbox_textures[i].ref)
//					s->flags = 0;
		if (!(s->flags & SHADER_SKY))
		{
			Shader_Reset(s);

			Shader_DefaultScript(s->name, s,
						"{\n"
							"sort sky\n"
							"{\n"
								"map $whiteimage\n"
								"rgbgen const $r_fastskycolour\n"
							"}\n"
							"surfaceparm nodlight\n"
						"}\n"
					);
			return;
		}
	}

	if (!s->numpasses && s->sort != SHADER_SORT_PORTAL && !(s->flags & (SHADER_NODRAW|SHADER_SKY)) && !s->fog_dist && !s->prog)
	{
		pass = &s->passes[s->numpasses++];
		pass = &s->passes[0];
		pass->tcgen = TC_GEN_BASE;
		if (TEXVALID(s->defaulttextures.base))
			pass->texgen = T_GEN_DIFFUSE;
		else
		{
			pass->texgen = T_GEN_SINGLEMAP;
			TEXASSIGN(pass->anim_frames[0], R_LoadHiResTexture(s->name, NULL, IF_NOALPHA));
			if (!TEXVALID(pass->anim_frames[0]))
			{
				Con_Printf("Shader %s failed to load default texture\n", s->name);
				pass->anim_frames[0] = missing_texture;
			}
			Con_Printf("Shader %s with no passes and no surfaceparm nodraw, inserting pass\n", s->name);
		}
		pass->shaderbits |= SBITS_MISC_DEPTHWRITE;
		pass->rgbgen = RGB_GEN_VERTEX_LIGHTING;
		pass->alphagen = ALPHA_GEN_IDENTITY;
		pass->numMergedPasses = 1;
		Shader_SetBlendmode(pass);
	}

	if (!Q_stricmp (s->name, "flareShader"))
	{
		s->flags |= SHADER_FLARE;
		s->flags |= SHADER_NODRAW;
	}

	if (!s->numpasses && !s->sort)
	{
		s->sort = SHADER_SORT_ADDITIVE;
		return;
	}

	if (!s->sort && s->passes->texgen == T_GEN_CURRENTRENDER)
		s->sort = SHADER_SORT_NEAREST;


	if ((s->polyoffset.unit < 0) && !s->sort)
	{
		s->sort = SHADER_SORT_DECAL;
	}

	if ((r_vertexlight.value || !(s->usageflags & SUF_LIGHTMAP)) && !s->prog)
	{
		// do we have a lightmap pass?
		pass = s->passes;
		for (i = 0; i < s->numpasses; i++, pass++)
		{
			if (pass->flags & SHADER_PASS_LIGHTMAP)
				break;
		}

		if (i == s->numpasses)
		{
			goto done;
		}

		// try to find pass with rgbgen set to RGB_GEN_VERTEX
		pass = s->passes;
		for (i = 0; i < s->numpasses; i++, pass++)
		{
			if (pass->rgbgen == RGB_GEN_VERTEX_LIGHTING)
				break;
		}

		if (i < s->numpasses)
		{		// we found it
			pass->flags |= SHADER_CULL_FRONT;
			pass->flags &= ~SHADER_PASS_ANIMMAP;
			pass->shaderbits &= ~SBITS_BLEND_BITS;
			pass->blendmode = 0;
			pass->shaderbits |= SBITS_MISC_DEPTHWRITE;
			pass->alphagen = ALPHA_GEN_IDENTITY;
			pass->numMergedPasses = 1;
			s->flags |= SHADER_DEPTHWRITE;
			s->sort = SHADER_SORT_OPAQUE;
			s->numpasses = 1;
			memcpy(&s->passes[0], pass, sizeof(shaderpass_t));
		}
		else
		{	// we didn't find it - simply remove all lightmap passes
			pass = s->passes;
			for(i = 0; i < s->numpasses; i++, pass++)
			{
				if (pass->flags & SHADER_PASS_LIGHTMAP)
					break;
			}

			if ( i == s->numpasses -1 )
			{
				s->numpasses--;
			}
			else if ( i < s->numpasses - 1 )
			{
				for ( ; i < s->numpasses - 1; i++, pass++ )
				{
					memcpy ( pass, &s->passes[i+1], sizeof(shaderpass_t) );
				}
				s->numpasses--;
			}

			if ( s->passes[0].numtcmods )
			{
				pass = s->passes;
				for ( i = 0; i < s->numpasses; i++, pass++ )
				{
					if ( !pass->numtcmods )
						break;
				}

				memcpy ( &s->passes[0], pass, sizeof(shaderpass_t) );
			}

			s->passes[0].rgbgen = RGB_GEN_VERTEX_LIGHTING;
			s->passes[0].alphagen = ALPHA_GEN_IDENTITY;
			s->passes[0].blendmode = 0;
			s->passes[0].flags &= ~(SHADER_PASS_ANIMMAP|SHADER_PASS_NOCOLORARRAY);
			s->passes[0].shaderbits &= ~SBITS_BLEND_BITS;
			s->passes[0].shaderbits |= SBITS_MISC_DEPTHWRITE;
			s->passes[0].numMergedPasses = 1;
			s->numpasses = 1;
			s->flags |= SHADER_DEPTHWRITE;
		}
	}
done:;

	//if we've no specular map, try and find whatever the q3 syntax said. hopefully it'll be compatible...
	if (!TEXVALID(s->defaulttextures.specular))
	{
		for (pass = s->passes, i = 0; i < s->numpasses; i++, pass++)
		{
			if (pass->alphagen == ALPHA_GEN_SPECULAR)
				if (pass->texgen == T_GEN_ANIMMAP || pass->texgen == T_GEN_SINGLEMAP)
					s->defaulttextures.specular = pass->anim_frames[0];
		}
	}

	if (!TEXVALID(s->defaulttextures.base))
	{
		shaderpass_t *best = NULL;
		int bestweight = 9999999;
		int weight;

		for (pass = s->passes, i = 0; i < s->numpasses; i++, pass++)
		{
			weight = 0;
			if (pass->flags & SHADER_PASS_DETAIL)
				weight += 500;	//prefer not to use a detail pass. these are generally useless.
			if (pass->numtcmods || pass->tcgen != TC_GEN_BASE)
				weight += 200;
			if (pass->rgbgen != RGB_GEN_IDENTITY && pass->rgbgen != RGB_GEN_IDENTITY_OVERBRIGHT && pass->rgbgen != RGB_GEN_IDENTITY_LIGHTING)
				weight += 100;

			if (pass->texgen != T_GEN_ANIMMAP && pass->texgen != T_GEN_SINGLEMAP && pass->texgen != T_GEN_VIDEOMAP)
				weight += 1000;
			
			if (weight < bestweight)
			{
				bestweight = weight;
				best = pass;
			}
		}

		if (best)
		{
			if (best->texgen == T_GEN_ANIMMAP || best->texgen == T_GEN_SINGLEMAP)
				s->defaulttextures.base = best->anim_frames[0];
#ifndef NOMEDIA
			else if (pass->texgen == T_GEN_VIDEOMAP && pass->cin)
				s->defaulttextures.base = Media_UpdateForShader(best->cin);
#endif
		}
	}

	pass = s->passes;
	for (i = 0; i < s->numpasses; i++, pass++)
	{
		if (!(pass->shaderbits & (SBITS_BLEND_BITS|SBITS_MASK_BITS)))
		{
			break;
		}
	}

	// all passes have blendfuncs
	if (i == s->numpasses)
	{
		int opaque;

		opaque = -1;
		pass = s->passes;
		for (i = 0; i < s->numpasses; i++, pass++ )
		{
			if (pass->shaderbits & SBITS_ATEST_BITS)
			{
				opaque = i;
			}

			if (pass->rgbgen == RGB_GEN_UNKNOWN)
			{
				if (   (pass->shaderbits & SBITS_SRCBLEND_BITS) == 0
					|| (pass->shaderbits & SBITS_SRCBLEND_BITS) == SBITS_SRCBLEND_ONE
					|| (pass->shaderbits & SBITS_SRCBLEND_BITS) == SBITS_SRCBLEND_SRC_ALPHA)
					pass->rgbgen = RGB_GEN_IDENTITY_LIGHTING;
				else
					pass->rgbgen = RGB_GEN_IDENTITY;
			}

			Shader_SetBlendmode (pass);

			if (pass->blendmode == PBM_ADD)
				s->defaulttextures.fullbright = pass->anim_frames[0];
		}

		if (!(s->flags & SHADER_SKY ) && !s->sort)
		{
			if (opaque == -1)
				s->sort = SHADER_SORT_BLEND;
			else
				s->sort = SHADER_SORT_SEETHROUGH;
		}
	}
	else
	{
		int	j;
		shaderpass_t *sp;

		sp = s->passes;
		for (j = 0; j < s->numpasses; j++, sp++)
		{
			if (sp->rgbgen == RGB_GEN_UNKNOWN)
			{
				if (sp->flags & SHADER_PASS_LIGHTMAP)
					sp->rgbgen = RGB_GEN_IDENTITY_LIGHTING;
				else
					sp->rgbgen = RGB_GEN_IDENTITY;
			}

			Shader_SetBlendmode (sp);
		}

		if (!s->sort)
		{
			if (pass->shaderbits & SBITS_ATEST_BITS)
				s->sort = SHADER_SORT_SEETHROUGH;
		}

		if (!( s->flags & SHADER_DEPTHWRITE) &&
			!(s->flags & SHADER_SKY))
		{
			pass->shaderbits |= SBITS_MISC_DEPTHWRITE;
			s->flags |= SHADER_DEPTHWRITE;
		}
	}

	if (s->numpasses >= 2)
	{
		int j;

		pass = s->passes;
		for (i = 0; i < s->numpasses;)
		{
			if (i == s->numpasses - 1)
				break;

			pass = s->passes + i;
			for (j = 1; j < s->numpasses-i && j == i + pass->numMergedPasses && j < be_maxpasses; j++)
				Shader_SetPassFlush (pass, pass + j);

			i += pass->numMergedPasses;
		}
	}

	if (!s->sort)
	{
		s->sort = SHADER_SORT_OPAQUE;
	}

	if ((s->flags & SHADER_SKY) && (s->flags & SHADER_DEPTHWRITE))
	{
		s->flags &= ~SHADER_DEPTHWRITE;
	}

	if (!s->bemoverrides[bemoverride_depthonly])
	{
		const char *mask = Shader_AlphaMaskProgArgs(s);
		if (*mask)
			s->bemoverrides[bemoverride_depthonly] = R_RegisterShader(va("depthonly%s", mask), SUF_NONE, 
				"{\n"
					"program depthonly\n"
					"{\n"
						"map $diffuse\n"
						"depthwrite\n"
						"maskcolor\n"
					"}\n"
				"}\n");
	}

	if (!s->prog && sh_config.progs_required)
		Shader_Programify(s);

	if (s->prog)
	{
		if (!s->numpasses)
		{
			s->passes[0].texgen = T_GEN_DIFFUSE;
			s->numpasses = 1;
		}
		s->passes->numMergedPasses = s->numpasses;
	}
}
/*
void Shader_UpdateRegistration (void)
{
	int i, j, l;
	shader_t *shader;
	shaderpass_t *pass;

	shader = r_shaders;
	for (i = 0; i < MAX_SHADERS; i++, shader++)
	{
		if (!shader->registration_sequence)
			continue;
		if (shader->registration_sequence != registration_sequence)
		{
			Shader_Free ( shader );
			shader->registration_sequence = 0;
			continue;
		}

		pass = shader->passes;
		for (j = 0; j < shader->numpasses; j++, pass++)
		{
			if (pass->flags & SHADER_PASS_ANIMMAP)
			{
				for (l = 0; l < pass->anim_numframes; l++)
				{
					if (pass->anim_frames[l])
						pass->anim_frames[l]->registration_sequence = registration_sequence;
				}
			}
			else if ( pass->flags & SHADER_PASS_VIDEOMAP )
			{
				// Shader_RunCinematic will do the job
//				pass->cin->frame = -1;
			}
			else if ( !(pass->flags & SHADER_PASS_LIGHTMAP) )
			{
				if ( pass->anim_frames[0] )
					pass->anim_frames[0]->registration_sequence = registration_sequence;
			}
		}
	}
}
*/

void Shader_DefaultSkin(const char *shortname, shader_t *s, const void *args);
void QDECL R_BuildDefaultTexnums(texnums_t *tn, shader_t *shader)
{
	char *h;
	char imagename[MAX_QPATH];
	char *subpath = NULL;
	strcpy(imagename, shader->name);
	h = strchr(imagename, '#');
	if (h)
		*h = 0;

	//skins can use an alternative path in certain cases, to work around dodgy models.
	if (shader->generator == Shader_DefaultSkin)
		subpath = shader->genargs;

	if (!tn)
		tn = &shader->defaulttextures;
	if (!TEXVALID(shader->defaulttextures.base))
	{
		/*dlights/realtime lighting needs some stuff*/
		if (!TEXVALID(tn->base))
		{
			tn->base = R_LoadHiResTexture(imagename, subpath, IF_NOALPHA);
		}

		TEXASSIGN(shader->defaulttextures.base, tn->base);
	}

	COM_StripExtension(imagename, imagename, sizeof(imagename));

	if (!TEXVALID(shader->defaulttextures.bump))
	{
		if (r_loadbumpmapping)
		{
			if (!TEXVALID(tn->bump))
				tn->bump = R_LoadHiResTexture(va("%s_norm", imagename), subpath, IF_TRYBUMP);
		}
		TEXASSIGN(shader->defaulttextures.bump, tn->bump);
	}

	if (!TEXVALID(shader->defaulttextures.loweroverlay))
	{
		if (shader->flags & SHADER_HASTOPBOTTOM)
		{
			if (!TEXVALID(tn->loweroverlay))
				tn->loweroverlay = R_LoadHiResTexture(va("%s_pants", imagename), subpath, 0);	/*how rude*/
		}
		TEXASSIGN(shader->defaulttextures.loweroverlay, tn->loweroverlay);
	}

	if (!TEXVALID(shader->defaulttextures.upperoverlay))
	{
		if (shader->flags & SHADER_HASTOPBOTTOM)
		{
			if (!TEXVALID(tn->upperoverlay))
				tn->upperoverlay = R_LoadHiResTexture(va("%s_shirt", imagename), subpath, 0);
		}
		TEXASSIGN(shader->defaulttextures.upperoverlay, tn->upperoverlay);
	}

	if (!TEXVALID(shader->defaulttextures.specular))
	{
		extern cvar_t gl_specular;
		if ((shader->flags & SHADER_HASGLOSS) && gl_specular.value && gl_load24bit.value)
		{
			if (!TEXVALID(tn->specular))
				tn->specular = R_LoadHiResTexture(va("%s_gloss", imagename), subpath, 0);
		}
		TEXASSIGN(shader->defaulttextures.specular, tn->specular);
	}

	if (!TEXVALID(shader->defaulttextures.fullbright))
	{
		extern cvar_t r_fb_bmodels;
		if ((shader->flags & SHADER_HASFULLBRIGHT) && r_fb_bmodels.value && gl_load24bit.value)
		{
			if (!TEXVALID(tn->fullbright))
				tn->specular = R_LoadHiResTexture(va("%s_luma", imagename), subpath, 0);
		}
		TEXASSIGN(shader->defaulttextures.fullbright, tn->fullbright);
	}
}

void Shader_DefaultScript(const char *shortname, shader_t *s, const void *args)
{
	const char *f = args;
	if (!args)
		return;
	while (*f == ' ' || *f == '\t' || *f == '\n' || *f == '\r')
		f++;
	if (*f == '{')
	{
		f++;
		Shader_ReadShader(s, (void*)f, SPM_DEFAULT);
	}
};

void Shader_DefaultBSPLM(const char *shortname, shader_t *s, const void *args)
{
	char *builtin = NULL;
	if (!builtin && r_drawflat.ival)
		builtin = (
				"{\n"
					"program drawflat_wall\n"
					"{\n"
						"map $lightmap\n"
						"tcgen lightmap\n"
						"rgbgen const $r_floorcolour\n"
					"}\n"
				"}\n"
			);
#ifdef D3D11QUAKE
	if (qrenderer == QR_DIRECT3D11)
	{
		if (!builtin)
			builtin = (
						"{\n"
							"program defaultwall\n"
							"{\n"
								"map $diffuse\n"
							"}\n"
							"{\n"
								"map $lightmap\n"
							"}\n"
							"{\n"
								"map $normalmap\n"
							"}\n"
							"{\n"
								"map $deluxmap\n"
							"}\n"
							"{\n"
								"map $fullbright\n"
							"}\n"
							"{\n"
								"map $specular\n"
							"}\n"
						"}\n"
					);
	}
#endif

#if 0//def D3D9QUAKE
	if (qrenderer == QR_DIRECT3D9)
	{
		if (!builtin)
			builtin = (
						"{\n"
							"program defaultwall\n"
							"{\n"
								"map $diffuse\n"
							"}\n"
							"{\n"
								"map $lightmap\n"
							"}\n"
							"{\n"
								"map $normalmap\n"
							"}\n"
							"{\n"
								"map $deluxmap\n"
							"}\n"
							"{\n"
								"map $fullbright\n"
							"}\n"
							"{\n"
								"map $specular\n"
							"}\n"
						"}\n"
					);
	}
#endif

#ifdef GLQUAKE
	if (qrenderer == QR_OPENGL)
	{
		if (!builtin && r_lightprepass.ival)
		{
				builtin = (
					"{\n"
						"program lpp_wall\n"
						"{\n"
							"map $sourcecolour\n"
						"}\n"
						"{\n"
							"map $diffuse\n"
						"}\n"
						"{\n"
							"map $lightmap\n"
						"}\n"
						"{\n"
							"map $normalmap\n"
						"}\n"
						"{\n"
							"map $deluxmap\n"
						"}\n"
						"{\n"
							"map $fullbright\n"
						"}\n"
					"}\n"
				);
		}
		if (!builtin && gl_config.arb_shader_objects)
		{
				builtin = (
					"{\n"
						"program defaultwall\n"
						"{\n"
							"map $diffuse\n"
						"}\n"
						"{\n"
							"map $lightmap\n"
						"}\n"
						"{\n"
							"map $normalmap\n"
						"}\n"
						"{\n"
							"map $deluxmap\n"
						"}\n"
						"{\n"
							"map $fullbright\n"
						"}\n"
						"{\n"
							"map $specular\n"
						"}\n"
					"}\n"
				);
		}
	}
#endif
	if (!builtin)
		builtin = (
				"{\n"
/*					"if $deluxmap\n"
						"{\n"
							"map $normalmap\n"
							"tcgen base\n"
							"depthwrite\n"
						"}\n"
						"{\n"
							"map $deluxmap\n"
							"tcgen lightmap\n"
						"}\n"
					"endif\n"
*///				"if !r_fullbright\n"
						"{\n"
							"map $lightmap\n"
//							"if $deluxmap\n"
//								"blendfunc gl_dst_color gl_zero\n"
//							"endif\n"
						"}\n"
//					"endif\n"
					"{\n"
						"map $diffuse\n"
						"tcgen base\n"
//						"if $deluxmap || !r_fullbright\n"
//							"blendfunc gl_dst_color gl_zero\n"
							"blendfunc filter\n"
//						"endif\n"
					"}\n"
					"if gl_fb_bmodels\n"
						"{\n"
							"map $fullbright\n"
							"blendfunc add\n"
							"depthfunc equal\n"
						"}\n"
					"endif\n"
				"}\n"
			);

	Shader_DefaultScript(shortname, s, builtin);
}

void Shader_DefaultCinematic(const char *shortname, shader_t *s, const void *args)
{
	Shader_DefaultScript(shortname, s,
		va(
			"{\n"
				"program default2d\n"
				"{\n"
					"videomap \"%s\"\n"
				"}\n"
			"}\n"
		, (const char*)args)
	);
}

/*shortname should begin with 'skybox_'*/
void Shader_DefaultSkybox(const char *shortname, shader_t *s, const void *args)
{
	Shader_DefaultScript(shortname, s,
		va(
			"{\n"
				"skyparms %s - -\n"
			"}\n"
		, shortname+7)
	);
}

char *Shader_DefaultBSPWater(shader_t *s, const char *shortname)
{
	int wstyle;
	int type;
	float alpha;
	qboolean explicitalpha = false;
	cvar_t *alphavars[] = {	&r_wateralpha, &r_lavaalpha, &r_slimealpha, &r_telealpha};
	cvar_t *stylevars[] = {	&r_waterstyle, &r_lavastyle, &r_slimestyle, &r_telestyle};
	if (!strncmp(shortname, "*lava", 5))
		type = 1;
	else if (!strncmp(shortname, "*slime", 6))
		type = 2;
	else if (!strncmp(shortname, "*tele", 5))
		type = 3;
	else
		type = 0;
	alpha = Shader_FloatArgument(s, "#ALPHA");
	if (alpha)
		explicitalpha = true;
	else
		alpha = *alphavars[type]->string?alphavars[type]->value:alphavars[0]->value;

	if (alpha <= 0)
		wstyle = -1;
	else if (r_fastturb.ival)
		wstyle = 0;
#ifdef GLQUAKE
	else if (qrenderer == QR_OPENGL && gl_config.arb_shader_objects && *stylevars[type]->string)
		wstyle = stylevars[type]->ival;
	else if (qrenderer == QR_OPENGL && gl_config.arb_shader_objects && stylevars[0]->ival > 0)
		wstyle = stylevars[0]->ival;
#endif
	else
		wstyle = 1;

#ifdef GLQUAKE
	if (wstyle > 2 && !gl_config.ext_framebuffer_objects)
		wstyle = 2;
#endif
	switch(wstyle)
	{
	case -1:	//invisible
		return (
			"{\n"
				"surfaceparm nodraw\n"
				"surfaceparm nodlight\n"
			"}\n"
		);
	case -2:	//regular with r_wateralpha forced off.
		return (
			"{\n"
				"program defaultwarp\n"
				"{\n"
					"map $diffuse\n"
					"tcmod turb 0.02 0.1 0.5 0.1\n"
				"}\n"
				"surfaceparm nodlight\n"
			"}\n"
		);
	case 0:	//fastturb
		return (
			"{\n"
//				"program defaultfill\n"
				"{\n"
					"map $whiteimage\n"
					"rgbgen const $r_fastturbcolour\n"
				"}\n"
				"surfaceparm nodlight\n"
			"}\n"
		);
	default:
	case 1:	//vanilla style
		return va(
				"{\n"
					"program defaultwarp%s\n"
					"{\n"
						"map $diffuse\n"
						"tcmod turb 0.02 0.1 0.5 0.1\n"
						"if %g < 1\n"
							"alphagen const %g\n"
							"blendfunc gl_src_alpha gl_one_minus_src_alpha\n"
						"endif\n"
					"}\n"
					"surfaceparm nodlight\n"
				"}\n"
				, explicitalpha?"":va("#ALPHA=%g",alpha), alpha, alpha);
	case 2:	//refraction of the underwater surface, with a fresnel
		return (
			"{\n"
				"surfaceparm nodlight\n"
				"{\n"
					"map $refraction\n"
				"}\n"
				"{\n"
					"map $normalmap\n"
				"}\n"
				"{\n"
					"map $diffuse\n"
				"}\n"
//				"{\n"
//					"map $refractiondepth\n"
//				"}\n"
				"program altwater#FRESNEL=4\n"
			"}\n"
		);
	case 3:	//reflections
		return (
			"{\n"
				"surfaceparm nodlight\n"
				"{\n"
					"map $refraction\n"
				"}\n"
				"{\n"
					"map $normalmap\n"
				"}\n"
				"{\n"
					"map $reflection\n"
				"}\n"
//				"{\n"
//					"map $refractiondepth\n"
//				"}\n"
				"program altwater#REFLECT#FRESNEL=4\n"
			"}\n"
		);
	case 4:	//ripples
		return (
			"{\n"
				"surfaceparm nodlight\n"
				"{\n"
					"map $refraction\n"
				"}\n"
				"{\n"
					"map $normalmap\n"
				"}\n"
				"{\n"
					"map $diffuse\n"
				"}\n"
//				"{\n"
//					"map $refractiondepth\n"
//				"}\n"
				"{\n"
					"map $ripplemap\n"
				"}\n"
				"program altwater#RIPPLEMAP#FRESNEL=4\n"
			"}\n"
		);
	case 5:	//ripples+reflections
		return (
			"{\n"
				"surfaceparm nodlight\n"
				"{\n"
					"map $refraction\n"
				"}\n"
				"{\n"
					"map $normalmap\n"
				"}\n"
				"{\n"
					"map $reflection\n"
				"}\n"
//				"{\n"
//					"map $refractiondepth\n"
//				"}\n"
				"{\n"
					"map $ripplemap\n"
				"}\n"
				"program altwater#REFLECT#RIPPLEMAP#FRESNEL=4\n"
			"}\n"
		);
	}
}

void Shader_DefaultWaterShader(const char *shortname, shader_t *s, const void *args)
{
	Shader_DefaultScript(shortname, s, Shader_DefaultBSPWater(s, shortname));
}
void Shader_DefaultBSPQ2(const char *shortname, shader_t *s, const void *args)
{
	if (!strncmp(shortname, "sky/", 4))
	{
		Shader_DefaultScript(shortname, s,
				"{\n"
					"surfaceparm nodlight\n"
					"skyparms - - -\n"
				"}\n"
			);
	}
	else if (!strncmp(shortname, "warp/", 5) || !strncmp(shortname, "warp33/", 7) || !strncmp(shortname, "warp66/", 7))
	{
		Shader_DefaultScript(shortname, s, Shader_DefaultBSPWater(s, shortname));
	}
	else if (!strncmp(shortname, "trans/", 6))
		Shader_DefaultScript(shortname, s,
				"{\n"
					"{\n"
						"map $diffuse\n"
						"alphagen const $#ALPHA\n"
						"blendfunc blend\n"
					"}\n"
				"}\n"
			);
	else
		Shader_DefaultBSPLM(shortname, s, args);
}

void Shader_DefaultBSPQ1(const char *shortname, shader_t *s, const void *args)
{
	char *builtin = NULL;
	if (r_mirroralpha.value < 1 && !strcmp(shortname, "window02_1"))
	{
		if (r_mirroralpha.value < 0)
		{
			builtin =	"{\n"
							"portal\n"
							"{\n"
								"map $diffuse\n"
								"blendfunc blend\n"
								"alphagen portal 512\n"
								"depthwrite\n"
							"}\n"
						"}\n";
		}
		else
		{
			builtin =	"{\n"
							"portal\n"
							"{\n"
								"map $diffuse\n"
								"blendfunc blend\n"
								"alphagen const $r_mirroralpha\n"
								"depthwrite\n"
							"}\n"
							"surfaceparm nodlight\n"
						"}\n";
		}

	}

	if (!builtin && (*shortname == '*'))
	{
		builtin = Shader_DefaultBSPWater(s, shortname);
	}
	if (!builtin && !strncmp(shortname, "sky", 3))
	{
		//q1 sky
		if (r_fastsky.ival)
		{
			builtin = (
					"{\n"
						"sort sky\n"
						"{\n"
							"map $whiteimage\n"
							"rgbgen const $r_fastskycolour\n"
						"}\n"
						"surfaceparm nodlight\n"
					"}\n"
				);
		}
		else if (*r_skyboxname.string)
		{
			builtin = (
					"{\n"
						"sort sky\n"
						"skyparms $r_skybox - -\n"
						"surfaceparm nodlight\n"
					"}\n"
				);
			Shader_DefaultScript(shortname, s, builtin);
			if (s->flags & SHADER_SKY)
				return;
			builtin = NULL;
			/*if the r_skybox failed to load or whatever, reset and fall through and just use the regular sky*/
			Shader_Reset(s);
		}
		if (!builtin)
			builtin = (
				"{\n"
					"sort sky\n"
					"program defaultsky\n"
					"skyparms - 512 -\n"
					/*WARNING: these values are not authentic quake, only close aproximations*/
					"{\n"
						"map $diffuse\n"
						"tcmod scale 10 10\n"
						"tcmod scroll 0.04 0.04\n"
						"depthwrite\n"
					"}\n"
					"{\n"
						"map $fullbright\n"
						"blendfunc blend\n"
						"tcmod scale 10 10\n"
						"tcmod scroll 0.02 0.02\n"
					"}\n"
				"}\n"
			);
	}
	if (!builtin && *shortname == '{')
	{
		/*alpha test*/
		builtin = (
			"{\n"
		/*		"if $deluxmap\n"
					"{\n"
						"map $normalmap\n"
						"tcgen base\n"
					"}\n"
					"{\n"
						"map $deluxmap\n"
						"tcgen lightmap\n"
					"}\n"
				"endif\n"*/
				"{\n"
					"map $diffuse\n"
					"tcgen base\n"
						"alphamask\n"
				"}\n"
				"if $lightmap\n"
					"{\n"
						"map $lightmap\n"
						"blendfunc gl_dst_color gl_zero\n"
						"depthfunc equal\n"
					"}\n"
				"endif\n"
				"{\n"
					"map $fullbright\n"
					"blendfunc add\n"
					"depthfunc equal\n"
				"}\n"
			"}\n"
		);
	}

	/*Hack: note that halflife would normally expect you to use rendermode/renderampt*/
	if (!builtin && (!strncmp(shortname, "glass", 5)/* || !strncmp(shortname, "window", 6)*/))
	{
		/*alpha bended*/
		builtin = (
			"{\n"
				"{\n"
					"map $diffuse\n"
					"tcgen base\n"
					"blendfunc blend\n"
				"}\n"
			"}\n"
		);
	}

	if (builtin)
		Shader_DefaultScript(shortname, s, builtin);
	else
		Shader_DefaultBSPLM(shortname, s, args);
}

void Shader_DefaultBSPVertex(const char *shortname, shader_t *s, const void *args)
{
	shaderpass_t *pass;

//	s->defaulttextures.base = R_LoadHiResTexture(va("%s_d.tga", shortname), NULL, 0);

	if (Shader_ParseShader("defaultvertexlit", s))
		return;

	pass = &s->passes[0];
	pass->tcgen = TC_GEN_BASE;
	pass->shaderbits |= SBITS_MISC_DEPTHWRITE;
	pass->rgbgen = RGB_GEN_VERTEX_LIGHTING;
	pass->alphagen = ALPHA_GEN_IDENTITY;
	pass->numMergedPasses = 1;
	Shader_SetBlendmode(pass);

/*	if (TEXVALID(s->defaulttextures.base))
	{
		pass->texgen = T_GEN_DIFFUSE;
	}
	else*/
	{
		s->defaulttextures.base = R_LoadHiResTexture(shortname, NULL, 0);
		pass->texgen = T_GEN_DIFFUSE;
		if (!TEXVALID(s->defaulttextures.base))
			Con_DPrintf (CON_WARNING "Shader %s has a stage with no image: %s.\n", s->name, shortname );
	}

	s->numpasses = 1;
	s->numdeforms = 0;
	s->flags = SHADER_IMAGEPENDING|SHADER_DEPTHWRITE|SHADER_CULL_FRONT;
	s->sort = SHADER_SORT_OPAQUE;
	s->uses = 1;
}
void Shader_DefaultBSPFlare(const char *shortname, shader_t *s, const void *args)
{
	shaderpass_t *pass;
	if (Shader_ParseShader("defaultflare", s))
		return;

	pass = &s->passes[0];
	pass->flags = SHADER_IMAGEPENDING|SHADER_PASS_NOCOLORARRAY;
	pass->shaderbits |= SBITS_SRCBLEND_ONE|SBITS_DSTBLEND_ONE;
	pass->anim_frames[0] = R_LoadHiResTexture(shortname, NULL, 0);
	pass->rgbgen = RGB_GEN_VERTEX_LIGHTING;
	pass->alphagen = ALPHA_GEN_IDENTITY;
	pass->numtcmods = 0;
	pass->tcgen = TC_GEN_BASE;
	pass->numMergedPasses = 1;
	Shader_SetBlendmode(pass);

	if (!TEXVALID(pass->anim_frames[0]))
	{
		Con_DPrintf (CON_WARNING "Shader %s has a stage with no image: %s.\n", s->name, shortname );
		pass->anim_frames[0] = missing_texture;
	}

	s->numpasses = 1;
	s->numdeforms = 0;
	s->flags = SHADER_IMAGEPENDING|SHADER_FLARE;
	s->sort = SHADER_SORT_ADDITIVE;
	s->uses = 1;

	s->flags |= SHADER_NODRAW;
}
void Shader_DefaultSkin(const char *shortname, shader_t *s, const void *args)
{
	if (Shader_ParseShader("defaultskin", s))
		return;

	Shader_DefaultScript(shortname, s,
		"{\n"
			"if $lpp\n"
				"program lpp_skin\n"
			"else\n"
				"program defaultskin\n"
			"endif\n"
			"if gl_affinemodels\n"
				"affine\n"
			"endif\n"
			"{\n"
				"map $diffuse\n"
				"rgbgen lightingDiffuse\n"
			"}\n"
			"{\n"
				"map $loweroverlay\n"
				"rgbgen bottomcolor\n"
				"blendfunc gl_src_alpha gl_one\n"
			"}\n"
			"{\n"
				"map $upperoverlay\n"
				"rgbgen topcolor\n"
				"blendfunc gl_src_alpha gl_one\n"
			"}\n"
			"{\n"
				"map $fullbright\n"
				"blendfunc add\n"
			"}\n"
			"if $haveprogram\n"
				"{\n"
					"map $normalmap\n"
				"}\n"
				"{\n"
					"map $specular\n"
				"}\n"
			"endif\n"
		"}\n"
		);
}
void Shader_DefaultSkinShell(const char *shortname, shader_t *s, const void *args)
{
	if (Shader_ParseShader("defaultskinshell", s))
		return;

	Shader_DefaultScript(shortname, s,
		"{\n"
			"sort seethrough\n"	//before blend, but after other stuff. should fix most issues with shotgun etc effects obscuring it.
//			"deformvertexes normal 1 1\n"
			//draw it with depth but no colours at all
			"{\n"
				"map $whiteimage\n"
				"maskcolor\n"
				"depthwrite\n"
			"}\n"
			//now draw it again, depthfunc = equal should fill only the near-side, avoiding any excess-brightness issues with overlapping triangles
			"{\n"
				"map $whiteimage\n"
				"rgbgen entity\n"
				"alphagen entity\n"
				"blendfunc blend\n"
			"}\n"
		"}\n"
		);
}
void Shader_Default2D(const char *shortname, shader_t *s, const void *genargs)
{
	if (Shader_ParseShader("default2d", s))
		return;
	Shader_DefaultScript(shortname, s,
		"{\n"
			"if $nofixed\n"
				"program default2d\n"
			"endif\n"
			"affine\n"
			"nomipmaps\n"
			"{\n"
				"clampmap $diffuse\n"
				"rgbgen vertex\n"
				"alphagen vertex\n"
				"blendfunc gl_one gl_one_minus_src_alpha\n"
			"}\n"
			"sort additive\n"
		"}\n"
		);

	TEXASSIGN(s->defaulttextures.base, R_LoadHiResTexture(s->name, NULL, IF_PREMULTIPLYALPHA|IF_UIPIC|IF_NOPICMIP|IF_NOMIPMAP|IF_CLAMP));
}

qboolean Shader_ReadShaderTerms(shader_t *s, char **shadersource, int parsemode, int *conddepth, int maxconddepth, int *cond)
{
	char *token;

#define COND_IGNORE 1
#define COND_IGNOREPARENT 2
#define COND_ALLOWELSE 4

	if (!*shadersource)
		return false;

	token = COM_ParseExt (shadersource, true, true);

	if ( !token[0] )
		return true;
	else if (!Q_stricmp(token, "if"))
	{
		if (*conddepth+1 == maxconddepth)
		{
			Con_Printf("if statements nest too deeply in shader %s\n", s->name);
			return false;
		}
		*conddepth+=1;
		cond[*conddepth] = (!Shader_EvaluateCondition(s, shadersource)?COND_IGNORE:0);
		cond[*conddepth] |= COND_ALLOWELSE;
		if (cond[*conddepth-1] & (COND_IGNORE|COND_IGNOREPARENT))
			cond[*conddepth] |= COND_IGNOREPARENT;
	}
	else if (!Q_stricmp(token, "endif"))
	{
		if (!*conddepth)
		{
			Con_Printf("endif without if in shader %s\n", s->name);
			return false;
		}
		*conddepth-=1;
	}
	else if (!Q_stricmp(token, "else"))
	{
		if (cond[*conddepth] & COND_ALLOWELSE)
		{
			cond[*conddepth] ^= COND_IGNORE;
			cond[*conddepth] &= ~COND_ALLOWELSE;
		}
		else
			Con_Printf("unexpected else statement in shader %s\n", s->name);
	}
	else if (cond[*conddepth] & (COND_IGNORE|COND_IGNOREPARENT))
	{
		//eat it.
		while (**shadersource)
		{
			token = COM_ParseExt(shadersource, false, true);
			if ( !token[0] )
				break;
		}
	}
	else
	{
		int i;
		for (i = 0; shadermacros[i].name; i++)
		{
			if (!Q_stricmp (token, shadermacros[i].name))
			{
#define SHADER_MACRO_ARGS 6
				int argn = 0;
				char *body;
				char arg[SHADER_MACRO_ARGS][256];
				int cond = 0;
				//parse args until the end of the line
				while (*shadersource)
				{
					token = COM_ParseExt(shadersource, false, true);
					if ( !token[0] )
					{
						break;
					}
					if (argn <= SHADER_MACRO_ARGS)
					{
						Q_strncpyz(arg[argn], token, sizeof(arg[argn]));
						argn++;
					}
				}
				body = shadermacros[i].body;
				Shader_ReadShaderTerms(s, &body, parsemode, &cond, 0, &cond);
				return true;
			}
		}
		if (token[0] == '}')
			return false;
		else if (token[0] == '{')
			Shader_Readpass(s, shadersource);
		else if (Shader_Parsetok(s, NULL, shaderkeys, token, shadersource))
			return false;
	}
	return true;
}

//loads a shader string into an existing shader object, and finalises it and stuff
static void Shader_ReadShader(shader_t *s, char *shadersource, int parsemode)
{
	char *shaderstart = shadersource;
	int conddepth = 0;
	int cond[8];
	cond[0] = 0;

	memset(&parsestate, 0, sizeof(parsestate));
	parsestate.mode = parsemode;

// set defaults
	s->flags = SHADER_IMAGEPENDING|SHADER_CULL_FRONT;
	s->uses = 1;

	while (Shader_ReadShaderTerms(s, &shadersource, parsemode, &conddepth, sizeof(cond)/sizeof(cond[0]), cond))
	{
	}

	if (conddepth)
	{
		Con_Printf("if statements without endif in shader %s\n", s->name);
	}

	Shader_Finish ( s );

	//querying the shader body often requires generating the shader, which then gets parsed.
	if (saveshaderbody)
	{
		size_t l = shadersource - shaderstart;
		Z_Free(*saveshaderbody);
		*saveshaderbody = BZ_Malloc(l+1);
		(*saveshaderbody)[l] = 0;
		memcpy(*saveshaderbody, shaderstart, l);
		saveshaderbody = NULL;
	}
}

static qboolean Shader_ParseShader(char *parsename, shader_t *s)
{
	size_t offset = 0, length;
	char *buf = NULL;
	enum shaderparsemode_e parsemode = SPM_DEFAULT;

	if (Shader_LocateSource(parsename, &buf, &length, &offset, &parsemode))
	{
		// the shader is in the shader scripts
		if (buf && offset < length )
		{
			char *file, *token;


			file = buf + offset;
			token = COM_ParseExt (&file, true, true);
			if ( !file || token[0] != '{' )
			{
				FS_FreeFile(buf);
				return false;
			}

			Shader_Reset(s);

			Shader_ReadShader(s, file, parsemode);

			return true;
		}
	}

	return false;
}
void R_UnloadShader(shader_t *shader)
{
	if (shader->uses-- == 1)
		Shader_Free(shader);
}
static shader_t *R_LoadShader (const char *name, unsigned int usageflags, shader_gen_t *defaultgen, const char *genargs)
{
	int i, f = -1;
	char cleanname[MAX_QPATH];
	char shortname[MAX_QPATH];
	char *argsstart;
	shader_t *s;

	if (!*name)
		name = "gfx/unspecified";

	COM_AssertMainThread("R_LoadShader");

	Q_strncpyz(cleanname, name, sizeof(cleanname));
	COM_CleanUpPath(cleanname);

	// check the hash first
	s = Hash_Get(&shader_active_hash, cleanname);
	while (s)
	{
		//make sure the same texture can be used as either a lightmap or vertexlit shader
		//if it has an explicit shader overriding it then that still takes precidence. we might just have multiple copies of it.
		//q3 has a separate (internal) shader for every lightmap.
		if (!((s->usageflags ^ usageflags) & SUF_LIGHTMAP))
		{
			s->uses++;
			return s;
		}
		s = Hash_GetNext(&shader_active_hash, cleanname, s);
	}

	// not loaded, find a free slot
	for (i = 0; i < r_numshaders; i++)
	{
		if (!r_shaders[i] || !r_shaders[i]->uses)
		{
			if ( f == -1 )	// free shader
			{
				f = i;
				break;
			}
		}
	}

	if (f == -1)
	{
		shader_t **n;
		int nm;
		f = r_numshaders;
		if (f == r_maxshaders)
		{
			if (!r_maxshaders)
				Sys_Error( "R_LoadShader: shader system not inited.");

			nm = r_maxshaders * 2;
			n = realloc(r_shaders, nm*sizeof(*n));
			if (!n)
			{
				Sys_Error( "R_LoadShader: Shader limit exceeded.");
				return NULL;
			}
			memset(n+r_maxshaders, 0, (nm - r_maxshaders)*sizeof(*n));
			r_shaders = n;
			r_maxshaders = nm;
		}
	}
	if (strlen(cleanname) >= sizeof(s->name))
	{
		Sys_Error( "R_LoadShader: Shader name too long.");
		return NULL;
	}

	s = r_shaders[f];
	if (!s)
		s = r_shaders[f] = Z_Malloc(sizeof(*s));
	s->id = f;
	if (r_numshaders < f+1)
		r_numshaders = f+1;

	Q_strncpyz(s->name, cleanname, sizeof(s->name));
	s->usageflags = usageflags;
	s->generator = defaultgen;
	if (genargs)
		s->genargs = strdup(genargs);
	else
		s->genargs = NULL;

	//now determine the 'short name'. ie: the shader that is loaded off disk (no args, no extension)
	argsstart = strchr(cleanname, '#');
	if (argsstart)
		*argsstart = 0;
	COM_StripExtension (cleanname, shortname, sizeof(shortname));

	if (ruleset_allow_shaders.ival)
	{
		if (sh_config.shadernamefmt)
		{
			char drivername[MAX_QPATH];
			Q_snprintfz(drivername, sizeof(drivername), sh_config.shadernamefmt, cleanname);
			if (Shader_ParseShader(drivername, s))
				return s;
		}
		if (Shader_ParseShader(cleanname, s))
			return s;
		if (Shader_ParseShader(shortname, s))
			return s;
	}

	// make a default shader

	if (s->generator)
	{
		Shader_Reset(s);

		if (!strcmp(shortname, "textures/common/clip"))
			Shader_DefaultScript(cleanname, s,
				"{\n"
					"surfaceparm nodraw\n"
					"surfaceparm nodlight\n"
				"}\n");
		else
			s->generator(cleanname, s, s->genargs);
		return s;
	}
	else
	{
		Shader_Free(s);
	}
	return NULL;
}

char *Shader_GetShaderBody(shader_t *s)
{
	char *adr;
	char cleanname[MAX_QPATH];
	char shortname[MAX_QPATH];
	int oldsort;
	qboolean resort = false;
	if (!s || !s->uses)
		return NULL;

	adr = Z_StrDup("UNKNOWN BODY");
	saveshaderbody = &adr;

	strcpy(cleanname, s->name);
	COM_StripExtension (cleanname, shortname, sizeof(shortname));
	if (ruleset_allow_shaders.ival)
	{
		if (sh_config.shadernamefmt)
		{
			char drivername[MAX_QPATH];
			Q_snprintfz(drivername, sizeof(drivername), sh_config.shadernamefmt, cleanname);
			if (Shader_ParseShader(drivername, s))
				return adr;
		}
		if (Shader_ParseShader(cleanname, s))
			return adr;
		if (Shader_ParseShader(shortname, s))
			return adr;
	}
	if (s->generator)
	{
		oldsort = s->sort;
		Shader_Reset(s);

		s->generator(shortname, s, s->genargs);

		if (s->sort != oldsort)
			resort = true;
	}

	if (resort)
	{
		Mod_ResortShaders();
	}
	return adr;
}

void Shader_ShowShader_f(void)
{
	char *sourcename = Cmd_Argv(1);
	shader_t *o = R_LoadShader(sourcename, SUF_NONE, NULL, NULL);
	if (!o)
		o = R_LoadShader(sourcename, SUF_LIGHTMAP, NULL, NULL);
	if (!o)
		o = R_LoadShader(sourcename, SUF_2D, NULL, NULL);
	if (o)
	{
		char *body = Shader_GetShaderBody(o);
		if (body)
		{
			Con_Printf("%s\n{%s\n", o->name, body);
			Z_Free(body);
		}
		else
		{
			Con_Printf("Shader \"%s\" is not in use\n", o->name);
		}
	}
	else
		Con_Printf("Shader \"%s\" is not loaded\n", sourcename);
}

void Shader_DoReload(void)
{
	shader_t *s;
	unsigned int i;
	char shortname[MAX_QPATH];
	char cleanname[MAX_QPATH];
	int oldsort;
	qboolean resort = false;

	if (shader_rescan_needed && ruleset_allow_shaders.ival)
	{
		Shader_FlushCache();

		COM_EnumerateFiles("materials/*.mtr", Shader_InitCallback, NULL);
		COM_EnumerateFiles("shaders/*.shader", Shader_InitCallback, NULL);
		COM_EnumerateFiles("scripts/*.shader", Shader_InitCallback, NULL);
		COM_EnumerateFiles("scripts/*.rscript", Shader_InitCallback, NULL);

		shader_reload_needed = true;
		shader_rescan_needed = false;
	}

	if (!shader_reload_needed)
		return;
	shader_reload_needed = false;
	Font_InvalidateColour();
	Shader_ReloadGenerics();

	for (i = 0; i < r_numshaders; i++)
	{
		s = r_shaders[i];
		if (!s || !s->uses)
			continue;

		strcpy(cleanname, s->name);
		COM_StripExtension (cleanname, shortname, sizeof(shortname));
		if (ruleset_allow_shaders.ival)
		{
			if (sh_config.shadernamefmt)
			{
				char drivername[MAX_QPATH];
				Q_snprintfz(drivername, sizeof(drivername), sh_config.shadernamefmt, cleanname);
				if (Shader_ParseShader(drivername, s))
					continue;
			}
			if (Shader_ParseShader(cleanname, s))
				continue;
			if (Shader_ParseShader(shortname, s))
				continue;
		}
		if (s->generator)
		{
			oldsort = s->sort;
			Shader_Reset(s);

			s->generator(shortname, s, s->genargs);

			if (s->sort != oldsort)
				resort = true;
		}
	}

	if (resort)
	{
		Mod_ResortShaders();
	}
}

void Shader_NeedReload(qboolean rescanfs)
{
	if (rescanfs)
		shader_rescan_needed = true;
	shader_reload_needed = true;
}

cin_t *R_ShaderGetCinematic(shader_t *s)
{
#ifndef NOMEDIA
	int j;
	if (!s)
		return NULL;
	for (j = 0; j < s->numpasses; j++)
		if (s->passes[j].cin)
			return s->passes[j].cin;
#endif
	/*no cinematic in this shader!*/
	return NULL;
}

cin_t *R_ShaderFindCinematic(const char *name)
{
#ifdef NOMEDIA
	return NULL;
#else
	int i;
	char shortname[MAX_QPATH];
	shader_t *s;

	if (!r_shaders)
		return NULL;

	COM_StripExtension ( name, shortname, sizeof(shortname));

	COM_CleanUpPath(shortname);

	//try and find it
	for (i = 0; i < r_numshaders; i++)
	{
		s = r_shaders[i];
		if (!s || !s->uses)
			continue;

		if (!Q_stricmp (shortname, s->name) )
			return R_ShaderGetCinematic(s);
	}
	return NULL;
#endif
}

void Shader_ResetRemaps(void)
{
	shader_t *s;
	int i;
	for (i = 0; i < r_numshaders; i++)
	{
		s = r_shaders[i];
		if (!s)
			continue;
		s->remapto = s;
		s->remaptime = 0;
	}
}

void R_RemapShader(const char *sourcename, const char *destname, float timeoffset)
{
	shader_t *o;
	shader_t *n;

	//make sure all types of the shader are remapped properly.
	//if there's a .shader file with it then it should 'just work'.

	o = R_LoadShader (sourcename, SUF_NONE, NULL, NULL);
	n = R_LoadShader (destname, SUF_NONE, NULL, NULL);
	if (o)
	{
		if (!n)
			n = o;
		o->remapto = n;
		o->remaptime = timeoffset;	//this just feels wrong.
	}

	o = R_LoadShader (sourcename, SUF_2D, NULL, NULL);
	n = R_LoadShader (destname, SUF_2D, NULL, NULL);
	if (o)
	{
		if (!n)
			n = o;
		o->remapto = n;
		o->remaptime = timeoffset;
	}

	o = R_LoadShader (sourcename, SUF_LIGHTMAP, NULL, NULL);
	n = R_LoadShader (destname, SUF_LIGHTMAP, NULL, NULL);
	if (o)
	{
		if (!n)
		{
			n = R_LoadShader (destname, SUF_2D, NULL, NULL);
			if (!n)
				n = o;
		}
		o->remapto = n;
		o->remaptime = timeoffset;
	}
}

void Shader_RemapShader_f(void)
{
	char *sourcename = Cmd_Argv(1);
	char *destname = Cmd_Argv(2);
	float timeoffset = atof(Cmd_Argv(3));
	
	if (!Cmd_FromGamecode() && strcmp(Info_ValueForKey(cl.serverinfo, "*cheats"), "ON"))
	{
		Con_Printf("%s may only be used from gamecode, or when cheats are enabled\n", Cmd_Argv(0));
		return;
	}
	if (!*sourcename)
	{
		Con_Printf("%s originalshader remappedshader starttime\n", Cmd_Argv(0));
		return;
	}
	R_RemapShader(sourcename, destname, timeoffset);
}

//blocks
int R_GetShaderSizes(shader_t *shader, int *width, int *height, qboolean blocktillloaded)
{
	if (!shader)
		return false;
	if (shader->flags &	SHADER_IMAGEPENDING)
	{
		int i;
		if (width)
			*width = 0;
		if (height)
			*height = 0;
		for (i = 0; i < shader->numpasses; i++)
		{
			if (shader->passes[i].texgen == T_GEN_SINGLEMAP && shader->passes[i].anim_frames[0] && shader->passes[i].anim_frames[0]->status == TEX_LOADING)
			{
				if (!blocktillloaded)
					return -1;
				COM_WorkerPartialSync(shader->passes[i].anim_frames[0], &shader->passes[i].anim_frames[0]->status, TEX_LOADING);
			}
			if (shader->passes[i].texgen == T_GEN_DIFFUSE && (shader->defaulttextures.base && shader->defaulttextures.base->status == TEX_LOADING))
			{
				if (!blocktillloaded)
					return -1;
				COM_WorkerPartialSync(shader->defaulttextures.base, &shader->defaulttextures.base->status, TEX_LOADING);
			}
		}

		shader->flags &= ~SHADER_IMAGEPENDING;
		for (i = 0; i < shader->numpasses; i++)
		{
			if (shader->passes[i].texgen == T_GEN_SINGLEMAP)
			{
				if (shader->passes[i].anim_frames[0] && shader->passes[i].anim_frames[0]->status == TEX_LOADED)
				{
					shader->width = shader->passes[i].anim_frames[0]->width;
					shader->height = shader->passes[i].anim_frames[0]->height;
				}
				break;
			}
			if (shader->passes[i].texgen == T_GEN_DIFFUSE && shader->defaulttextures.base)
			{
				if (shader->defaulttextures.base->status == TEX_LOADED)
				{
					shader->width = shader->defaulttextures.base->width;
					shader->height = shader->defaulttextures.base->height;
				}
				break;
			}
		}
		if (i == shader->numpasses)
		{	//this shader has no textures from which to source a width and height
			if (!shader->width)
				shader->width = 64;
			if (!shader->height)
				shader->height = 64;
		}
	}
	if (shader->width && shader->height)
	{
		if (width)
			*width = shader->width;
		if (height)
			*height = shader->height;
		return true;	//final size
	}
	else
	{
		//fill with dummy values
		if (width)
			*width = 64;
		if (height)
			*height = 64;
		return false;
	}
}
shader_t *R_RegisterPic (const char *name)
{
	shader_t *shader;
	shader = R_LoadShader (name, SUF_2D, Shader_Default2D, NULL);
	return shader;
}

shader_t *QDECL R_RegisterShader (const char *name, unsigned int usageflags, const char *shaderscript)
{
	return R_LoadShader (name, usageflags, Shader_DefaultScript, shaderscript);
}

shader_t *R_RegisterShader_Lightmap (const char *name)
{
	return R_LoadShader (name, SUF_LIGHTMAP, Shader_DefaultBSPLM, NULL);
}

shader_t *R_RegisterShader_Vertex (const char *name)
{
	return R_LoadShader (name, 0, Shader_DefaultBSPVertex, NULL);
}

shader_t *R_RegisterShader_Flare (const char *name)
{
	return R_LoadShader (name, 0, Shader_DefaultBSPFlare, NULL);
}

shader_t *QDECL R_RegisterSkin (const char *shadername, const char *modname)
{
	char newsname[MAX_QPATH];
	shader_t *shader;
#ifdef _DEBUG
	if (shadername == com_token)
		Con_Printf("R_RegisterSkin was passed com_token. that will bug out.\n");
#endif

	newsname[0] = 0;
	if (modname && !strchr(shadername, '/') && *shadername)
	{
		char *b = COM_SkipPath(modname);
		if (b != modname && b-modname + strlen(shadername)+1 < sizeof(newsname))
		{
			b--;	//no trailing /
			memcpy(newsname, modname, b - modname);
			newsname[b-modname] = 0;
		}
	}
	if (*newsname)
	{
		int l = strlen(newsname);
		Q_strncpyz(newsname+l, ":models", sizeof(newsname)-l);
	}
	else
		Q_strncpyz(newsname, "models", sizeof(newsname));
	shader = R_LoadShader (shadername, 0, Shader_DefaultSkin, newsname);
	return shader;
}
shader_t *R_RegisterCustom (const char *name, unsigned int usageflags, shader_gen_t *defaultgen, const void *args)
{
	return R_LoadShader (name, usageflags, defaultgen, args);
}
#endif //SERVERONLY
