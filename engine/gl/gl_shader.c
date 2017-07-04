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

extern texid_t missing_texture;
texid_t r_whiteimage;
qboolean shader_reload_needed;
static qboolean shader_rescan_needed;
static char **saveshaderbody;

sh_config_t sh_config;

//cvars that affect shader generation
cvar_t r_vertexlight = CVARFD("r_vertexlight", "0", CVAR_SHADERSYSTEM, "Hack loaded shaders to remove detail pass and lightmap sampling for faster rendering.");
cvar_t r_forceprogramify = CVARAFD("r_forceprogramify", "0", "dpcompat_makeshitup", CVAR_SHADERSYSTEM, "Reduce the shader to a single texture, and then make stuff up about its mother. The resulting fist fight results in more colour when you shine a light upon its face.\nSet to 2 to ignore 'depthfunc equal' and 'tcmod scale' in order to tolerate bizzare shaders made for a bizzare engine.\nBecause most shaders made for DP are by people who _clearly_ have no idea what the heck they're doing, you'll typically need the '2' setting.");
extern cvar_t r_glsl_offsetmapping_reliefmapping;
extern cvar_t r_fastturb, r_fastsky, r_skyboxname;
extern cvar_t r_drawflat;
extern cvar_t r_shaderblobs;
extern cvar_t r_tessellation;

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

	COM_AssertMainThread("COM_ParseExt");

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
	while ((c = (unsigned char)*data) <= ' ')
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
// skip /* comments
	if (c == '/' && data[1] == '*')
	{
		data+=2;
		newlines = true;
		for(;data[0];)
		{
			if (data[0] == '*' && data[1] == '/')
			{
				data+=2;
				break;
			}
			data++;
		}
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
	qboolean droppass;

	qboolean forceprogramify;
	//for dpwater compat, used to generate a program
	int dpwatertype;
	float reflectmin;
	float reflectmax;
	float reflectfactor;
	float refractfactor;
	vec3_t refractcolour;
	vec3_t reflectcolour;
	float wateralpha;
} parsestate;

typedef struct shaderkey_s
{
	char			*keyword;
	void			(*func)( shader_t *shader, shaderpass_t *pass, char **ptr );
	char			*prefix;
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
			conditiontrue = conditiontrue == r_lightprepass;
		else if (!Q_stricmp(token, "lightmap"))
			conditiontrue = conditiontrue == !r_fullbright.value;
		else if (!Q_stricmp(token, "deluxmap"))
			conditiontrue = conditiontrue == r_deluxmapping;
		else if (!Q_stricmp(token, "softwarebanding"))
			conditiontrue = conditiontrue == r_softwarebanding;

		//normalmaps are generated if they're not already known.
		else if (!Q_stricmp(token, "normalmap"))
			conditiontrue = conditiontrue == r_loadbumpmapping;

		else if (!Q_stricmp(token, "vulkan"))
			conditiontrue = conditiontrue == (qrenderer == QR_VULKAN);
		else if (!Q_stricmp(token, "opengl"))
			conditiontrue = conditiontrue == (qrenderer == QR_OPENGL);
		else if (!Q_stricmp(token, "d3d8"))
			conditiontrue = conditiontrue == (qrenderer == QR_DIRECT3D8);
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

static float Shader_ParseFloat(shader_t *shader, char **ptr, float defaultval)
{
	char *token;
	if (!ptr || !(*ptr))
		return defaultval;
	if (!**ptr || **ptr == '}')
		return defaultval;

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
			{
				if (*var->string)
					return var->value;
				else
					return defaultval;
			}
		}
	}
	if (!*token)
		return defaultval;
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

	/*
	if (v[0] > 5 || v[1] > 5 || v[2] > 5)
	{
		VectorScale(v, 1.0f/255, v);
	}
	*/
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

	func->args[0] = Shader_ParseFloat (shader, ptr, 0);
	func->args[1] = Shader_ParseFloat (shader, ptr, 0);
	func->args[2] = Shader_ParseFloat (shader, ptr, 0);
	func->args[3] = Shader_ParseFloat (shader, ptr, 0);
}

//===========================================================================

static int Shader_SetImageFlags(shader_t *shader, shaderpass_t *pass, char **name)
{
	//fixme: pass flags should be handled elsewhere.
	int flags = 0;

	for(;name;)
	{
		if (!Q_strnicmp(*name, "$rt:", 4))
		{
			*name += 4;
			flags |= IF_NOMIPMAP|IF_CLAMP|IF_RENDERTARGET;
			if (!(flags & (IF_NEAREST|IF_LINEAR)))
			{
				flags |= IF_LINEAR;
				if (pass)
					pass->flags |= SHADER_PASS_LINEAR;
			}
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
			flags &= ~IF_LINEAR;
			flags |= IF_NEAREST;
			if (pass)
			{
				pass->flags &= ~SHADER_PASS_LINEAR;
				pass->flags |= SHADER_PASS_NEAREST;
			}
		}
		else if (!Q_strnicmp(*name, "$linear:", 8))
		{
			*name+=8;
			flags &= ~IF_NEAREST;
			flags |= IF_LINEAR;
			if (pass)
			{
				pass->flags &= ~SHADER_PASS_NEAREST;
				pass->flags |= SHADER_PASS_LINEAR;
			}
		}
		else
			break;
	}

//	if (shader->flags & SHADER_SKY)
//		flags |= IF_SKY;
	if (shader->flags & SHADER_NOMIPMAPS)
		flags |= IF_NOMIPMAP;
	if (shader->flags & SHADER_NOPICMIP)
		flags |= IF_NOPICMIP;
	flags |= IF_MIPCAP;

	return flags;
}

texid_t R_LoadColourmapImage(void)
{
	//FIXME: cache the result, because this is abusive
	unsigned int w = 256, h = VID_GRADES;
	unsigned int x;
	unsigned int data[256*(VID_GRADES)];
	qbyte *colourmappal = (qbyte *)FS_LoadMallocFile ("gfx/colormap.lmp", NULL);
	if (!colourmappal)
	{
		size_t sz;
		qbyte *pcx = FS_LoadMallocFile("pics/colormap.pcx", &sz);
		if (pcx)
		{
			colourmappal = Z_Malloc(256*VID_GRADES);
			ReadPCXData(pcx, sz, 256, VID_GRADES, colourmappal);
			BZ_Free(pcx);
		}
	}
	if (colourmappal)
	{
		for (x = 0; x < sizeof(data)/sizeof(data[0]); x++)
			data[x] = d_8to24rgbtable[colourmappal[x]];
	}
	else
	{	//erk
		//fixme: generate a proper colourmap
		for (x = 0; x < sizeof(data)/sizeof(data[0]); x++)
		{
			int r, g, b;
			float l = 1.0-((x/256)/(float)VID_GRADES);
			r = d_8to24rgbtable[x & 0xff];
			g = (r>>16)&0xff;
			b = (r>>8)&0xff;
			r = (r>>0)&0xff;
			data[x] = d_8to24rgbtable[GetPaletteIndex(r*l,g*l,b*l)];
		}
	}
	BZ_Free(colourmappal);
	return R_LoadTexture("$colourmap", w, h, TF_RGBA32, data, IF_NOMIPMAP|IF_NOPICMIP|IF_NEAREST|IF_NOGAMMA|IF_CLAMP);
}

static texid_t Shader_FindImage ( char *name, int flags )
{
	extern texid_t missing_texture_normal;
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
		if (!Q_stricmp (name, "$blackimage"))
		{
			int wibuf[16] = {0};
			return R_LoadTexture("$blackimage", 4, 4, TF_RGBA32, wibuf, IF_NOMIPMAP|IF_NOPICMIP|IF_NEAREST|IF_NOGAMMA);
		}
		if (!Q_stricmp (name, "$identitynormal"))
			return missing_texture_normal;
		if (!Q_stricmp (name, "$colourmap"))
			return R_LoadColourmapImage();
	}
	if (flags & IF_RENDERTARGET)
		return R2D_RT_Configure(name, 0, 0, TF_INVALID, flags);
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

static void Shader_Affine ( shader_t *shader, shaderpass_t *pass, char **ptr )
{
	shader->flags |= SBITS_AFFINE;
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

	shader->flags |= SHADER_NOMARKS;	//just in case...

	token = Shader_ParseString ( ptr );
	if ( !Q_stricmp (token, "wave") )
	{
		deformv->type = DEFORMV_WAVE;
		deformv->args[0] = Shader_ParseFloat (shader, ptr, 0);
		if (deformv->args[0])
			deformv->args[0] = 1.0f / deformv->args[0];
		Shader_ParseFunc (shader, ptr, &deformv->func );
	}
	else if ( !Q_stricmp (token, "normal") )
	{
		deformv->type = DEFORMV_NORMAL;
		deformv->args[0] = Shader_ParseFloat (shader, ptr, 0);
		deformv->args[1] = Shader_ParseFloat (shader, ptr, 0);
	}
	else if ( !Q_stricmp (token, "bulge") )
	{
		deformv->type = DEFORMV_BULGE;
		Shader_ParseVector (shader, ptr, deformv->args);
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
	}
	else if ( !Q_stricmp (token, "autosprite2") )
	{
		deformv->type = DEFORMV_AUTOSPRITE2;
	}
	else if ( !Q_stricmp (token, "projectionShadow") )
		deformv->type = DEFORMV_PROJECTION_SHADOW;
	else
		return;

	shader->numdeforms++;
}

static void Shader_ClutterParms(shader_t *shader, shaderpass_t *pass, char **ptr)
{
	struct shader_clutter_s *clut;
	char *modelname;

	modelname = Shader_ParseString(ptr);
	clut = Z_Malloc(sizeof(*clut) + strlen(modelname));
	strcpy(clut->modelname, modelname);
	clut->spacing	= Shader_ParseFloat(shader, ptr, 1000);
	clut->scalemin	= Shader_ParseFloat(shader, ptr, 1);
	clut->scalemax	= Shader_ParseFloat(shader, ptr, 1);
	clut->zofs		= Shader_ParseFloat(shader, ptr, 0);
	clut->anglemin	= Shader_ParseFloat(shader, ptr, 0) * M_PI * 2 / 360.;
	clut->anglemax	= Shader_ParseFloat(shader, ptr, 360) * M_PI * 2 / 360.;

	clut->next = shader->clutter;
	shader->clutter = clut;
}

static void Shader_SkyParms(shader_t *shader, shaderpass_t *pass, char **ptr)
{
	skydome_t *skydome;
//	float skyheight;
	char *boxname;

	if (shader->skydome)
	{
		Z_Free(shader->skydome);
	}

	skydome = (skydome_t *)Z_Malloc(sizeof(skydome_t));
	shader->skydome = skydome;

	boxname = Shader_ParseString(ptr);
	Shader_ParseSkySides(shader->name, boxname, skydome->farbox_textures);

	/*skyheight =*/ Shader_ParseFloat(shader, ptr, 512);

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
	shader->fog_dist = Shader_ParseFloat (shader, ptr, 128);

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
		shader->flags |= SHADER_NODRAW;	//an alternative so that q3map2 won't see+strip it.
	else if ( !Q_stricmp( token, "nodlight" ) )
		shader->flags |= SHADER_NODLIGHT;
	else if ( !Q_stricmp( token, "noshadows" ) )
		shader->flags |= SHADER_NOSHADOWS;

	else if ( !Q_stricmp( token, "sky" ) )
		shader->flags |= SHADER_SKY;

	else if ( !Q_stricmp( token, "noimpact" ) )
		shader->flags |= SHADER_NOMARKS;	//wrong, but whatever.
	else if ( !Q_stricmp( token, "nomarks" ) )
		shader->flags |= SHADER_NOMARKS;
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
	else if( !Q_stricmp( token, "decal" ) ||  !Q_stricmp( token, "litdecal" ) )
		shader->sort = SHADER_SORT_DECAL;
	else if( !Q_stricmp( token, "seethrough" ) )
		shader->sort = SHADER_SORT_SEETHROUGH;
	else if( !Q_stricmp( token, "unlitdecal" ) )
		shader->sort = SHADER_SORT_UNLITDECAL;
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
	shader->flags |= SHADER_POLYGONOFFSET;	//some backends might be lazy and only allow simple values.
}

static void Shader_EntityMergable ( shader_t *shader, shaderpass_t *pass, char **ptr )
{
	shader->flags |= SHADER_ENTITY_MERGABLE;
}

#if defined(GLQUAKE) || defined(D3DQUAKE)
static qboolean Shader_ParseProgramCvar(char *script, cvar_t **cvarrefs, char **cvarnames, int *cvartypes, int cvartype)
{
	char body[MAX_QPATH];
	char *out;
	char *namestart;
	while (*script == ' ' || *script == '\t')
		script++;
	namestart = script;
	while ((*script >= 'A' && *script <= 'Z') || (*script >= 'a' && *script <= 'z') || (*script >= '0' && *script <= '9') || *script == '_')
		script++;

	cvartypes[0] = cvartype;
	cvarnames[0] = Z_Malloc(script - namestart + 1);
	memcpy(cvarnames[0], namestart, script - namestart);
	cvarnames[0][script - namestart] = 0;
	cvarnames[1] = NULL;

	while (*script == ' ' || *script == '\t')
		script++;
	if (*script == '=')
	{
		script++;
		while (*script == ' ' || *script == '\t')
			script++;

		out = body;
		while (out < com_token+countof(body)-1 && *script != '\n' && !(script[0] == '/' && script[1] == '/')) 
			*out++ = *script++;
		*out++ = 0;
		cvarrefs[0] = Cvar_Get(cvarnames[0], body, 0, "GLSL Variables");
	}
	else
		cvarrefs[0] = Cvar_Get(cvarnames[0], "", 0, "GLSL Variables");
	return true;
}
#endif

const struct sh_defaultsamplers_s sh_defaultsamplers[] =
{
	{"s_shadowmap",		1u<<0},
	{"s_projectionmap",	1u<<1},
	{"s_diffuse",		1u<<2},
	{"s_normalmap",		1u<<3},
	{"s_specular",		1u<<4},
	{"s_upper",			1u<<5},
	{"s_lower",			1u<<6},
	{"s_fullbright",	1u<<7},
	{"s_paletted",		1u<<8},
	{"s_reflectcube",	1u<<9},
	{"s_reflectmask",	1u<<10},
	{"s_lightmap",		1u<<11},
	{"s_deluxmap",		1u<<12},
#if MAXRLIGHTMAPS > 1
	{"s_lightmap1",		1u<<13},
	{"s_lightmap2",		1u<<14},
	{"s_lightmap3",		1u<<15},
	{"s_deluxmap1",		1u<<16},
	{"s_deluxmap2",		1u<<17},
	{"s_deluxmap3",		1u<<18},
#else
	{"s_lightmap1",		0},
	{"s_lightmap2",		0},
	{"s_lightmap3",		0},
	{"s_deluxmap1",		0},
	{"s_deluxmap2",		0},
	{"s_deluxmap3",		0},
#endif
	{NULL}
};

/*program text is already loaded, this function parses the 'header' of it to see which permutations it provides, and how many times we need to recompile it*/
static qboolean Shader_LoadPermutations(char *name, program_t *prog, char *script, int qrtype, int ver, char *blobfilename)
{
#if defined(GLQUAKE) || defined(D3DQUAKE)
	static struct
	{
		char *name;
		unsigned int bitmask;
	} permutations[] =
	{
		{"#define BUMP\n", PERMUTATION_BUMPMAP},
		{"#define FULLBRIGHT\n", PERMUTATION_FULLBRIGHT},
		{"#define UPPERLOWER\n", PERMUTATION_UPPERLOWER},
		{"#define REFLECTCUBEMASK\n", PERMUTATION_REFLECTCUBEMASK},
		{"#define SKELETAL\n", PERMUTATION_SKELETAL},
		{"#define FOG\n", PERMUTATION_FOG},
		{"#define FRAMEBLEND\n", PERMUTATION_FRAMEBLEND},
		{"#define LIGHTSTYLED\n", PERMUTATION_LIGHTSTYLES}
	};
#define MAXMODIFIERS 64
	const char *permutationdefines[countof(permutations) + MAXMODIFIERS + 1];
	unsigned int nopermutation = PERMUTATIONS-1;
	int nummodifiers = 0;
	int p, n, pn;
	char *end;
	vfsfile_t *blobfile;
	unsigned int permuoffsets[PERMUTATIONS], initoffset=0;
	unsigned int blobheaderoffset=0;
	qboolean blobadded;
	qboolean geom = false;
	qboolean tess = false;
	qboolean cantess = false;

	char maxgpubones[128];
	cvar_t *cvarrefs[64];
	char *cvarnames[64];
	int cvartypes[64];
	int cvarcount = 0;
	qboolean onefailed = false;
	extern cvar_t gl_specular;
#endif

#ifdef VKQUAKE
	if (qrenderer == QR_VULKAN && (qrtype == QR_VULKAN || qrtype == QR_OPENGL))
	{	//vulkan can potentially load glsl, f it has the extensions enabled.
		if (qrtype == QR_VULKAN && VK_LoadBlob(prog, script, name))
			return true;
	}
	else
#endif
	if (qrenderer != qrtype)
	{
		return false;
	}

#if defined(GLQUAKE) || defined(D3DQUAKE)
	ver = 0;

	if (!sh_config.pCreateProgram && !sh_config.pLoadBlob)
		return false;

	cvarnames[cvarcount] = NULL;

	prog->nofixedcompat = true;
	prog->numsamplers = 0;
	prog->defaulttextures = 0;
	for(;;)
	{
		while (*script == ' ' || *script == '\r' || *script == '\n' || *script == '\t')
			script++;
		if (!strncmp(script, "!!fixed", 7))
		{
			prog->nofixedcompat = false;
			script += 7;
		}
		else if (!strncmp(script, "!!geom", 6))
		{
			geom = true;
			script += 6;
		}
		else if (!strncmp(script, "!!tess", 6))
		{
			tess = true;
			script += 6;
		}
		else if (!strncmp(script, "!!samps", 7))
		{
			script += 7;
			while (*script != '\n' && *script != '\r')
			{
				int i;
				char *start;
				while (*script == ' ' || *script == '\t')
					script++;
				start = script;
				while (*script != ' ' && *script != '\t' && *script != '\r' && *script != '\n')
					script++;

				for (i = 0; sh_defaultsamplers[i].name; i++)
				{
					if (!strncmp(start, sh_defaultsamplers[i].name+2, script-start) && sh_defaultsamplers[i].name[2+script-start] == 0)
					{
						prog->defaulttextures |= sh_defaultsamplers[i].defaulttexbits;
						break;
					}
				}
				if (!sh_defaultsamplers[i].name)
				{
					i = atoi(start);
					if (i)
						prog->numsamplers = i;
					else
						Con_Printf("Unknown texture name in %s\n", name);
				}
			}
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
				char valuebuf[64];
				memcpy(namebuf, script, end - script);
				namebuf[end - script] = 0;
				while (*end == ' ' || *end == '\t')
					end++;
				if (*end == '=')
				{
					script = ++end;
					while (*end && *end != '\n' && *end != '\r' && end < script+sizeof(namebuf)-1)
						end++;
					memcpy(valuebuf, script, end - script);
					valuebuf[end - script] = 0;
				}
				else
					strcpy(valuebuf, "0");
				var = Cvar_Get(namebuf, valuebuf, CVAR_SHADERSYSTEM, "GLSL Variables");
				if (var)
					permutationdefines[nummodifiers++] = Z_StrDup(va("#define %s %g\n", namebuf, var->value));
			}
			script = end;
		}
		else if (!strncmp(script, "!!cvard3", 8))
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
				char valuebuf[64];
				memcpy(namebuf, script, end - script);
				namebuf[end - script] = 0;
				while (*end == ' ' || *end == '\t')
					end++;
				if (*end == '=')
				{
					script = ++end;
					while (*end && *end != '\n' && *end != '\r' && end < script+sizeof(namebuf)-1)
						end++;
					memcpy(valuebuf, script, end - script);
					valuebuf[end - script] = 0;
				}
				else
					strcpy(valuebuf, "0");
				var = Cvar_Get(namebuf, valuebuf, CVAR_SHADERSYSTEM, "GLSL Variables");
				if (var)
					permutationdefines[nummodifiers++] = Z_StrDup(va("#define %s %s(%g,%g,%g)\n", namebuf, ((qrenderer == QR_OPENGL)?"vec3":"float3"), var->vec4[0], var->vec4[1], var->vec4[2]));
			}
			script = end;
		}
		else if (!strncmp(script, "!!cvard4", 8))
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
				char valuebuf[64];
				memcpy(namebuf, script, end - script);
				namebuf[end - script] = 0;
				while (*end == ' ' || *end == '\t')
					end++;
				if (*end == '=')
				{
					script = ++end;
					while (*end && *end != '\n' && *end != '\r' && end < script+sizeof(namebuf)-1)
						end++;
					memcpy(valuebuf, script, end - script);
					valuebuf[end - script] = 0;
				}
				else
					strcpy(valuebuf, "0");
				var = Cvar_Get(namebuf, valuebuf, CVAR_SHADERSYSTEM, "GLSL Variables");
				if (var)
					permutationdefines[nummodifiers++] = Z_StrDup(va("#define %s %s(%g,%g,%g,%g)\n", namebuf, ((qrenderer == QR_OPENGL)?"vec4":"float4"), var->vec4[0], var->vec4[1], var->vec4[2], var->vec4[3]));
			}
			script = end;
		}
		else if (!strncmp(script, "!!cvarf", 7))
		{
			if (cvarcount+1 != sizeof(cvarnames)/sizeof(cvarnames[0]))
				cvarcount += Shader_ParseProgramCvar(script+7, &cvarrefs[cvarcount], &cvarnames[cvarcount], &cvartypes[cvarcount], SP_CVARF);
		}
		else if (!strncmp(script, "!!cvari", 7))
		{
			if (cvarcount+1 != sizeof(cvarnames)/sizeof(cvarnames[0]))
				cvarcount += Shader_ParseProgramCvar(script+7, &cvarrefs[cvarcount], &cvarnames[cvarcount], &cvartypes[cvarcount], SP_CVARI);
		}
		else if (!strncmp(script, "!!cvarv", 7))
		{
			if (cvarcount+1 != sizeof(cvarnames)/sizeof(cvarnames[0]))
				cvarcount += Shader_ParseProgramCvar(script+7, &cvarrefs[cvarcount], &cvarnames[cvarcount], &cvartypes[cvarcount], SP_CVAR3F);
		}
		else if (!strncmp(script, "!!permu", 7))
		{
			script += 7;
			while (*script == ' ' || *script == '\t')
				script++;
			end = script;
			while ((*end >= 'A' && *end <= 'Z') || (*end >= 'a' && *end <= 'z') || (*end >= '0' && *end <= '9') || *end == '_')
				end++;
			for (p = 0; p < countof(permutations); p++)
			{
				if (!strncmp(permutations[p].name+8, script, end - script) && permutations[p].name[8+end-script] == '\n')
				{
					nopermutation &= ~permutations[p].bitmask;
					break;
				}
			}
			if (p == countof(permutations))
			{
				//we 'recognise' ones that are force-defined, despite not being actual permutations.
				if (end - script == 4 && !strncmp("TESS", script, 4))
					cantess = true;
				else if (strncmp("SPECULAR", script, end - script))
				if (strncmp("DELUXE", script, end - script))
				if (strncmp("DELUX", script, end - script))
				if (strncmp("OFFSETMAPPING", script, end - script))
				if (strncmp("RELIEFMAPPING", script, end - script))
					Con_DPrintf("Unknown pemutation in glsl program %s\n", name);
			}
			script = end;
		}
		else if (!strncmp(script, "!!ver", 5))
		{
			int minver, maxver;
			script += 5;
			while (*script == ' ' || *script == '\t')
				script++;
			end = script;
			while ((*end >= 'A' && *end <= 'Z') || (*end >= 'a' && *end <= 'z') || (*end >= '0' && *end <= '9') || *end == '_')
				end++;
			minver = strtol(script, &script, 0);
			while (*script == ' ' || *script == '\t')
				script++;
			maxver = strtol(script, NULL, 0); 
			if (!maxver)
				maxver = minver;

			ver = maxver;
			if (ver > sh_config.maxver)
				ver = sh_config.maxver;
			if (ver < minver)
				ver = minver;	//some kind of error.

			script = end;
		}
		else if (!strncmp(script, "//", 2))
		{
			script += 2;
			while (*script == ' ' || *script == '\t')
				script++;
		}
		else
			break;
		while (*script && *script != '\n')
			script++;
	};

	if (qrenderer == qrtype && ver < 150)
		tess = cantess = false;	//GL_ARB_tessellation_shader requires glsl 150(gl3.2) (or glessl 3.1). nvidia complains about layouts if you try anyway

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

	if (!sh_config.max_gpu_bones)
	{
		Q_snprintfz(maxgpubones, sizeof(maxgpubones), "");
		nopermutation |= PERMUTATION_SKELETAL;
	}
	else if (qrenderer == QR_OPENGL && sh_config.maxver < 120)	//with old versions of glsl (including gles), mat3x4 is not supported, and we have to emulate it with 3*vec4. maybe we should just do that unconditionally, but whatever.
		Q_snprintfz(maxgpubones, sizeof(maxgpubones), "#define MAX_GPU_BONES %i\n#define PACKEDBONES\n", sh_config.max_gpu_bones);
	else
		Q_snprintfz(maxgpubones, sizeof(maxgpubones), "#define MAX_GPU_BONES %i\n", sh_config.max_gpu_bones);
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
		if (end-start == 7 && !Q_strncasecmp(start, "usemods", 7))
			prog->nofixedcompat = false;
		if (nummodifiers < MAXMODIFIERS)
		{
			if (end-start == 4 && !Q_strncasecmp(start, "tess", 4))
				tess |= cantess;

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

	prog->tess = tess;
	prog->supportedpermutations = ~nopermutation;
	for (p = 0; p < PERMUTATIONS; p++)
	{
		qboolean isprimary;
		memset(&prog->permu[p].h, 0, sizeof(prog->permu[p].h));
		if (nopermutation & p)
		{
			continue;
		}
		pn = nummodifiers;
		for (n = 0; n < countof(permutations); n++)
		{
			if (p & permutations[n].bitmask)
				permutationdefines[pn++] = permutations[n].name;
		}
		isprimary = (pn-nummodifiers)==1;
		if (p & PERMUTATION_UPPERLOWER)
			permutationdefines[pn++] = "#define UPPER\n#define LOWER\n";
		if (p & PERMUTATION_SKELETAL)
			permutationdefines[pn++] = maxgpubones;
		if (p & PERMUTATION_BUMPMAP)
		{
			if (r_glsl_offsetmapping.ival)
			{
				permutationdefines[pn++] = "#define OFFSETMAPPING\n";
				if (r_glsl_offsetmapping_reliefmapping.ival && (p & PERMUTATION_BUMPMAP))
					permutationdefines[pn++] = "#define RELIEFMAPPING\n";
			}

			if (r_deluxmapping)	//fixme: should be per-model really
				permutationdefines[pn++] = "#define DELUXE\n";
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
#define SILENTPERMUTATIONS (developer.ival?0:PERMUTATION_SKELETAL)
		if (!sh_config.pCreateProgram(prog, name, p, ver, permutationdefines, script, tess?script:NULL, tess?script:NULL, geom?script:NULL, script, (p & SILENTPERMUTATIONS)?true:onefailed, sh_config.pValidateProgram?NULL:blobfile))
		{
			if (isprimary)
				prog->supportedpermutations &= ~p;
			if (!(p & SILENTPERMUTATIONS))
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
		sh_config.pProgAutoFields(prog, name, cvarrefs, cvarnames, cvartypes);

	while(cvarcount)
		Z_Free((char*)cvarnames[--cvarcount]);

	if (blobfile && blobadded)
	{
		VFS_SEEK(blobfile, blobheaderoffset);
		VFS_WRITE(blobfile, permuoffsets, sizeof(permuoffsets));
	}
	if (blobfile)
		VFS_CLOSE(blobfile);

	if (p == PERMUTATIONS)
		return true;
#endif
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
		sh_config.pDeleteProg(prog);

	Z_Free(prog);
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
	{	//explicit path
		FS_LoadFile(basicname, &file);
		*blobname = 0;
	}
	else if (ruleset_allow_shaders.ival)
	{	//renderer-specific files
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
	else
	{
		file = NULL;
		*blobname = 0;
	}

	if (sh_config.pDeleteProg)
	{
		sh_config.pDeleteProg(&g->prog);
	}

	if (file)
	{
		Con_DPrintf("Loaded %s from disk\n", va(sh_config.progpath, basicname));
		g->failed = !Shader_LoadPermutations(g->name, &g->prog, file, qrtype, 0, blobname);
		FS_FreeFile(file);
		return;
	}
	else
	{
		int ver;
		for (i = 0; *sbuiltins[i].name; i++)
		{
			if (sbuiltins[i].qrtype == qrtype && !strcmp(sbuiltins[i].name, basicname))
			{
				ver = sbuiltins[i].apiver;

				if (ver < sh_config.minver || ver > sh_config.maxver)
					if (!(qrenderer==QR_OPENGL&&ver==110&&sh_config.maxver==100))
						continue;

				g->failed = !Shader_LoadPermutations(g->name, &g->prog, sbuiltins[i].body, qrtype, ver, blobname);

				if (g->failed)
					continue;

				return;
			}
		}
	}
}
program_t *Shader_FindGeneric(char *name, int qrtype)
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

	g = BZ_Malloc(sizeof(*g) + strlen(name)+1);
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
		//this happens if some cvar changed that affects the glsl itself. supposedly.
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
	{"e_vlscale",				SP_E_VLSCALE}, /*no lightmaps, no overbrights*/
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

	{"s_colour",				SP_S_COLOUR},

	/*rtlight properties, use with caution*/
	{"l_lightscreen",			SP_LIGHTSCREEN},
	{"l_lightradius",			SP_LIGHTRADIUS},
	{"l_lightcolour",			SP_LIGHTCOLOUR},
	{"l_lightposition",			SP_LIGHTPOSITION},
	{"l_lightcolourscale",		SP_LIGHTCOLOURSCALE},
	{"l_cubematrix",			SP_LIGHTCUBEMATRIX},
	{"l_shadowmapproj",			SP_LIGHTSHADOWMAPPROJ},
	{"l_shadowmapscale",		SP_LIGHTSHADOWMAPSCALE},

	{"e_sourcesize",			SP_SOURCESIZE},
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
	program_t *newprog;

	programbody = Shader_ParseBody(shader->name, ptr);
	if (programbody)
	{
		newprog = BZ_Malloc(sizeof(*newprog));
		memset(newprog, 0, sizeof(*newprog));
		newprog->refs = 1;
		if (!Shader_LoadPermutations(shader->name, newprog, programbody, qrtype, 0, NULL))
		{
			BZ_Free(newprog);
			newprog = NULL;
		}

		BZ_Free(programbody);
	}
	else
	{
		hash = strchr(shader->name, '#');
		if (hash)
		{
			//pass the # postfixes from the shader name onto the generic glsl to use
			char newname[512];
			Q_snprintfz(newname, sizeof(newname), "%s%s", Shader_ParseExactString(ptr), hash);
			newprog = Shader_FindGeneric(newname, qrtype);
		}
		else
			newprog = Shader_FindGeneric(Shader_ParseExactString(ptr), qrtype);
	}

	if (pass)
	{
		if (pass->numMergedPasses)
		{
			Shader_ReleaseGeneric(newprog);
			Con_DPrintf("shader %s: program defined after first texture map\n", shader->name);
		}
		else
		{
			Shader_ReleaseGeneric(pass->prog);
			pass->prog = newprog;
		}
	}
	else
	{
		Shader_ReleaseGeneric(shader->prog);
		shader->prog = newprog;
	}
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
#if 1
	Con_DPrintf("shader %s: 'param' no longer supported\n", shader->name);
#elif defined(GLQUAKE)
	cvar_t *cv = NULL;
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

	if (qrenderer == QR_OPENGL)
	{
		int specialint = 0;
		float specialfloat = 0;
		vec3_t specialvec = {0};

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

static void Shader_ReflectCube(shader_t *shader, shaderpass_t *pass, char **ptr)
{
	char *token = Shader_ParseString(ptr);
	unsigned int flags = Shader_SetImageFlags (shader, NULL, &token);
	shader->defaulttextures->reflectcube = Shader_FindImage(token, flags|IF_CUBEMAP);
}
static void Shader_ReflectMask(shader_t *shader, shaderpass_t *pass, char **ptr)
{
	char *token = Shader_ParseString(ptr);
	unsigned int flags = Shader_SetImageFlags (shader, NULL, &token);
	shader->defaulttextures->reflectmask = Shader_FindImage(token, flags);
}

static void Shader_DiffuseMap(shader_t *shader, shaderpass_t *pass, char **ptr)
{
	char *token = Shader_ParseString(ptr);
	unsigned int flags = Shader_SetImageFlags (shader, NULL, &token);
	shader->defaulttextures->base = Shader_FindImage(token, flags);

	Q_strncpyz(shader->defaulttextures->mapname, token, sizeof(shader->defaulttextures->mapname));
}
static void Shader_SpecularMap(shader_t *shader, shaderpass_t *pass, char **ptr)
{
	char *token = Shader_ParseString(ptr);
	unsigned int flags = Shader_SetImageFlags (shader, NULL, &token);
	shader->defaulttextures->specular = Shader_FindImage(token, flags);
}
static void Shader_NormalMap(shader_t *shader, shaderpass_t *pass, char **ptr)
{
	char *token = Shader_ParseString(ptr);
	unsigned int flags = Shader_SetImageFlags (shader, NULL, &token);
	shader->defaulttextures->bump = Shader_FindImage(token, flags|IF_TRYBUMP);
}
static void Shader_FullbrightMap(shader_t *shader, shaderpass_t *pass, char **ptr)
{
	char *token = Shader_ParseString(ptr);
	unsigned int flags = Shader_SetImageFlags (shader, NULL, &token);
	shader->defaulttextures->fullbright = Shader_FindImage(token, flags);
}
static void Shader_UpperMap(shader_t *shader, shaderpass_t *pass, char **ptr)
{
	char *token = Shader_ParseString(ptr);
	unsigned int flags = Shader_SetImageFlags (shader, NULL, &token);
	shader->defaulttextures->upperoverlay = Shader_FindImage(token, flags);
}
static void Shader_LowerMap(shader_t *shader, shaderpass_t *pass, char **ptr)
{
	char *token = Shader_ParseString(ptr);
	unsigned int flags = Shader_SetImageFlags (shader, NULL, &token);
	shader->defaulttextures->loweroverlay = Shader_FindImage(token, flags);
}

static qboolean Shaderpass_MapGen (shader_t *shader, shaderpass_t *pass, char *tname);
static void Shader_ProgMap(shader_t *shader, shaderpass_t *pass, char **ptr)
{
	//fixme
//	Shaderpass_BlendFunc (shader, pass, ptr);
}
static void Shader_ProgBlendFunc(shader_t *shader, shaderpass_t *pass, char **ptr)
{
	//fixme
}

static void Shader_Translucent(shader_t *shader, shaderpass_t *pass, char **ptr)
{
	shader->flags |= SHADER_BLEND;
}

static void Shader_DP_Camera(shader_t *shader, shaderpass_t *pass, char **ptr)
{
	shader->sort = SHADER_SORT_PORTAL;
}
static void Shader_DP_Water(shader_t *shader, shaderpass_t *pass, char **ptr)
{
	parsestate.forceprogramify = true;

	parsestate.dpwatertype |= 3;
	parsestate.reflectmin = Shader_ParseFloat(shader, ptr, 0);
	parsestate.reflectmax = Shader_ParseFloat(shader, ptr, 0);
	parsestate.refractfactor = Shader_ParseFloat(shader, ptr, 0);
	parsestate.reflectfactor = Shader_ParseFloat(shader, ptr, 0);
	Shader_ParseVector(shader, ptr, parsestate.refractcolour);
	Shader_ParseVector(shader, ptr, parsestate.reflectcolour);
	parsestate.wateralpha = Shader_ParseFloat(shader, ptr, 0);
}
static void Shader_DP_Reflect(shader_t *shader, shaderpass_t *pass, char **ptr)
{
	parsestate.forceprogramify = true;

	parsestate.dpwatertype |= 1;
	parsestate.reflectmin = 1;
	parsestate.reflectmax = 1;
	parsestate.reflectfactor = Shader_ParseFloat(shader, ptr, 0);
	Shader_ParseVector(shader, ptr, parsestate.reflectcolour);
}
static void Shader_DP_Refract(shader_t *shader, shaderpass_t *pass, char **ptr)
{
	parsestate.forceprogramify = true;

	parsestate.dpwatertype |= 2;
	parsestate.refractfactor = Shader_ParseFloat(shader, ptr, 0);
	Shader_ParseVector(shader, ptr, parsestate.refractcolour);
}

static void Shader_BEMode(shader_t *shader, shaderpass_t *pass, char **ptr)
{
	char subname[1024];
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
	else if (!Q_stricmp(token, "gbuffer") || !Q_stricmp(token, "prelight"))
		mode = bemoverride_gbuffer;
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
			Q_snprintfz(subname, sizeof(subname), "%s%s%s%s%s", tokencopy,
																(mode & LSHADER_SMAP)?"#PCF":"",
																(mode & LSHADER_SPOT)?"#SPOT":"",
																(mode & LSHADER_CUBE)?"#CUBE":"",
#ifdef GLQUAKE
																(qrenderer == QR_OPENGL && gl_config.arb_shadow && (mode & (LSHADER_SMAP|LSHADER_SPOT)))?"#USE_ARB_SHADOW":""
#else
																""
#endif
																);
			shader->bemoverrides[mode] = R_RegisterCustom(subname, shader->usageflags|(embed?SUR_FORCEFALLBACK:0), embed?Shader_DefaultScript:NULL, embed);
		}
	}
	else
	{
		shader->bemoverrides[mode] = R_RegisterCustom(tokencopy, shader->usageflags|(embed?SUR_FORCEFALLBACK:0), embed?Shader_DefaultScript:NULL, embed);
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
	{"clutter",			Shader_ClutterParms,		"fte"},
	{"lpp_light",		Shader_Prelight,			"fte"},
	{"glslprogram",		Shader_GLSLProgramName,		"fte"},
	{"program",			Shader_ProgramName,			"fte"},	//gl or d3d
	{"hlslprogram",		Shader_HLSL9ProgramName,	"fte"},	//for d3d
	{"hlsl11program",	Shader_HLSL11ProgramName,	"fte"},	//for d3d
	{"param",			Shader_ProgramParam,		"fte"},	//legacy
	{"affine",			Shader_Affine,				"fte"},	//some hardware is horribly slow, and can benefit from certain hints.

	{"bemode",			Shader_BEMode,				"fte"},

	{"diffusemap",		Shader_DiffuseMap,			"fte"},
	{"normalmap",		Shader_NormalMap,			"fte"},
	{"specularmap",		Shader_SpecularMap,			"fte"},
	{"fullbrightmap",	Shader_FullbrightMap,		"fte"},
	{"uppermap",		Shader_UpperMap,			"fte"},
	{"lowermap",		Shader_LowerMap,			"fte"},
	{"reflectmask",		Shader_ReflectMask,			"fte"},

	//dp compat
	{"reflectcube",		Shader_ReflectCube,			"dp"},
	{"camera",			Shader_DP_Camera,			"dp"},
	{"water",			Shader_DP_Water,			"dp"},
	{"reflect",			Shader_DP_Reflect,			"dp"},
	{"refract",			Shader_DP_Refract,			"dp"},

	/*doom3 compat*/
	{"diffusemap",		Shader_DiffuseMap,			"doom3"},	//macro for "{\nstage diffusemap\nmap <map>\n}"
	{"bumpmap",			Shader_NormalMap,			"doom3"},	//macro for "{\nstage bumpmap\nmap <map>\n}"
	{"discrete",		NULL,						"doom3"},
	{"nonsolid",		NULL,						"doom3"},
	{"noimpact",		NULL,						"doom3"},
	{"translucent",		Shader_Translucent,			"doom3"},
	{"noshadows",		NULL,						"doom3"},
	{"nooverlays",		NULL,						"doom3"},
	{"nofragment",		NULL,						"doom3"},

	/*simpler parsing for fte shaders*/
	{"progblendfunc",	Shader_ProgBlendFunc,		"fte"},
	{"progmap",			Shader_ProgMap,				"fte"},

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
		shader->flags |= SHADER_HASDIFFUSE;
	}
	else if (!Q_stricmp (tname, "$paletted"))
	{
		pass->texgen = T_GEN_PALETTED;
		shader->flags |= SHADER_HASPALETTED;
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
		shader->flags |= SHADER_HASCURRENTRENDER;
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

shaderpass_t *Shaderpass_DefineMap(shader_t *shader, shaderpass_t *pass)
{
	//'map foo' works a bit differently when there's a program in the same pass.
	//instead of corrupting the previous one, it collects multiple maps so that {prog foo;map t0;map t1; map t2; blendfunc add} can work as expected
	if (pass->prog)
	{
		if (pass->numMergedPasses==0)
			pass->numMergedPasses++;
		else
		{	//FIXME: bounds check!
			if (shader->numpasses == SHADER_PASS_MAX || shader->numpasses == SHADER_TMU_MAX)
			{
				Con_DPrintf (CON_WARNING "Shader %s has too many texture passes.\n", shader->name);
				parsestate.droppass = true;
			}
//			else if (shader->numpasses == be_maxpasses)
//				parsestate.droppass = true;
			else
			{
				pass->numMergedPasses++;
				shader->numpasses++;
			}
			pass = shader->passes+shader->numpasses-1;
			memset(pass, 0, sizeof(*pass));
		}
	}
	else
		pass->numMergedPasses = 1;
	return pass;
}

static void Shaderpass_Map (shader_t *shader, shaderpass_t *pass, char **ptr)
{
	int flags;
	char *token;

	pass = Shaderpass_DefineMap(shader, pass);

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
		if (!*shader->defaulttextures->mapname && *token != '$' && pass->tcgen == TC_GEN_BASE)
			Q_strncpyz(shader->defaulttextures->mapname, token, sizeof(shader->defaulttextures->mapname));
		pass->anim_frames[0] = Shader_FindImage (token, flags);
	}
}

static void Shaderpass_AnimMap (shader_t *shader, shaderpass_t *pass, char **ptr)
{
	int flags;
	char *token;
	texid_t image;
	qboolean isdiffuse = false;

	flags = Shader_SetImageFlags (shader, pass, NULL);

	if (pass->tcgen == TC_GEN_UNSPECIFIED)
		pass->tcgen = TC_GEN_BASE;
	pass->flags |= SHADER_PASS_ANIMMAP;
	pass->texgen = T_GEN_ANIMMAP;
	pass->anim_fps = Shader_ParseFloat (shader, ptr, 0);
	pass->anim_numframes = 0;

	for ( ; ; )
	{
		token = Shader_ParseString(ptr);
		if (!token[0])
		{
			break;
		}

		if (!pass->anim_numframes && !*shader->defaulttextures->mapname && *token != '$' && pass->tcgen == TC_GEN_BASE)
		{
			isdiffuse = true;
			shader->defaulttextures_fps = pass->anim_fps;
		}

		if (pass->anim_numframes < SHADER_MAX_ANIMFRAMES)
		{
			image = Shader_FindImage (token, flags);

			if (isdiffuse)
			{
				if (shader->numdefaulttextures < pass->anim_numframes+1)
				{
					int newmax = pass->anim_numframes+1;
					shader->defaulttextures = BZ_Realloc(shader->defaulttextures, sizeof(*shader->defaulttextures) * (newmax));
					memset(shader->defaulttextures+shader->numdefaulttextures, 0, sizeof(*shader->defaulttextures) * (newmax-shader->numdefaulttextures));
					shader->numdefaulttextures = newmax;
				}
				Q_strncpyz(shader->defaulttextures[pass->anim_numframes].mapname, token, sizeof(shader->defaulttextures[pass->anim_numframes].mapname));
			}
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
	char		*token = Shader_ParseSensString (ptr);

#ifndef HAVE_MEDIA_DECODER
	(void)token;
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

static void Shaderpass_SLProgramName (shader_t *shader, shaderpass_t *pass, char **ptr, int qrtype)
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
	//char *programbody;
	char *hash;

	/*programbody = Shader_ParseBody(shader->name, ptr);
	if (programbody)
	{
		shader->prog = BZ_Malloc(sizeof(*shader->prog));
		memset(shader->prog, 0, sizeof(*shader->prog));
		shader->prog->refs = 1;
		if (!Shader_LoadPermutations(shader->name, shader->prog, programbody, qrtype, 0, NULL))
		{
			BZ_Free(shader->prog);
			shader->prog = NULL;
		}

		BZ_Free(programbody);
		return;
	}*/

	hash = strchr(shader->name, '#');
	if (hash)
	{
		//pass the # postfixes from the shader name onto the generic glsl to use
		char newname[512];
		Q_snprintfz(newname, sizeof(newname), "%s%s", Shader_ParseExactString(ptr), hash);
		pass->prog = Shader_FindGeneric(newname, qrtype);
	}
	else
		pass->prog = Shader_FindGeneric(Shader_ParseExactString(ptr), qrtype);
}
static void Shaderpass_ProgramName (shader_t *shader, shaderpass_t *pass, char **ptr)
{
	Shaderpass_SLProgramName(shader,pass,ptr,qrenderer);
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
		shader->portaldist = Shader_ParseFloat(shader, ptr, 256);
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
		pass->alphagen_func.args[0] = fabs(Shader_ParseFloat(shader, ptr, 0));
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

	speed = Shader_ParseFloat(shader, ptr, 0);
	min = Shader_ParseFloat(shader, ptr, 0);
	max = Shader_ParseFloat(shader, ptr, 0);

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
	else if ( !Q_stricmp (token, "premul"))			//gets rid of feathering.
		pass->shaderbits |= SBITS_SRCBLEND_ONE | SBITS_DSTBLEND_ONE_MINUS_SRC_ALPHA;
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
		pass->shaderbits |= SBITS_ATEST_GT0;
	}
	else if (!Q_stricmp (token, "lt128"))
	{
		pass->shaderbits |= SBITS_ATEST_LT128;
	}
	else if (!Q_stricmp (token, "ge128"))
	{
		pass->shaderbits |= SBITS_ATEST_GE128;
	}
}

static void Shaderpass_DepthFunc (shader_t *shader, shaderpass_t *pass, char **ptr)
{
	char *token;

	pass->shaderbits &= ~(SBITS_MISC_DEPTHEQUALONLY|SBITS_MISC_DEPTHCLOSERONLY);

	token = Shader_ParseString (ptr);
	if (!Q_stricmp (token, "equal"))
		pass->shaderbits |= SBITS_MISC_DEPTHEQUALONLY;
	else if (!Q_stricmp (token, "lequal"))
		;	//default
	else if (!Q_stricmp (token, "less"))
		pass->shaderbits |= SBITS_MISC_DEPTHCLOSERONLY;
	else if (!Q_stricmp (token, "greater"))
		pass->shaderbits |= SBITS_MISC_DEPTHCLOSERONLY|SBITS_MISC_DEPTHEQUALONLY;
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
		tcmod->args[0] = -Shader_ParseFloat(shader, ptr, 0) / 360.0f;
		if (!tcmod->args[0])
		{
			return;
		}

		tcmod->type = SHADER_TCMOD_ROTATE;
	}
	else if ( !Q_stricmp (token, "scale") )
	{
		tcmod->args[0] = Shader_ParseFloat (shader, ptr, 0);
		tcmod->args[1] = Shader_ParseFloat (shader, ptr, 0);
		tcmod->type = SHADER_TCMOD_SCALE;
	}
	else if ( !Q_stricmp (token, "scroll") )
	{
		tcmod->args[0] = Shader_ParseFloat (shader, ptr, 0);
		tcmod->args[1] = Shader_ParseFloat (shader, ptr, 0);
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
			tcmod->args[i] = Shader_ParseFloat (shader, ptr, 0);
		tcmod->type = SHADER_TCMOD_TRANSFORM;
	}
	else if (!Q_stricmp (token, "turb"))
	{
		for (i = 0; i < 4; i++)
			tcmod->args[i] = Shader_ParseFloat (shader, ptr, 0);
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
		tcmod->args[0] = Shader_ParseFloat (shader, ptr, 0);
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
		tcmod->args[1] = Shader_ParseFloat (shader, ptr, 0);
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
		tcmod->args[0] = Shader_ParseFloat (shader, ptr, 0);
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
		tcmod->args[1] = Shader_ParseFloat (shader, ptr, 0);
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
	} else if ( !Q_stricmp (token, "vector") )
	{
		pass->tcgen = TC_GEN_VECTOR;
		Shader_ParseVector (shader, ptr, pass->tcgenvec[0]);
		Shader_ParseVector (shader, ptr, pass->tcgenvec[1]);
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
	pass->rgbgen_func.args[0] = Shader_ParseFloat(shader, ptr, 0);
}
static void Shaderpass_Green(shader_t *shader, shaderpass_t *pass, char **ptr)
{
	pass->rgbgen = RGB_GEN_CONST;
	pass->rgbgen_func.args[1] = Shader_ParseFloat(shader, ptr, 0);
}
static void Shaderpass_Blue(shader_t *shader, shaderpass_t *pass, char **ptr)
{
	pass->rgbgen = RGB_GEN_CONST;
	pass->rgbgen_func.args[2] = Shader_ParseFloat(shader, ptr, 0);
}
static void Shaderpass_Alpha(shader_t *shader, shaderpass_t *pass, char **ptr)
{
	pass->alphagen = ALPHA_GEN_CONST;
	pass->alphagen_func.args[0] = Shader_ParseFloat(shader, ptr, 0);
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
	if (Shader_ParseFloat(shader, ptr, 0) == 0.5)
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
	{"envmap",		Shaderpass_EnvMap,			"rscript"},//for alienarena
	{"nolightmap",	Shaderpass_NoLightMap,		"rscript"},//for alienarena
	{"scale",		Shaderpass_Scale,			"rscript"},//for alienarena
	{"scroll",		Shaderpass_Scroll,			"rscript"},//for alienarena
	{"alphagen",	Shaderpass_AlphaGen,		"rscript"},
	{"alphashift",	Shaderpass_AlphaShift,		"rscript"},//for alienarena
	{"alphamask",	Shaderpass_AlphaMask,		"rscript"},//for alienarena
	{"detail",		Shaderpass_Detail,			"rscript"},

	{"program",		Shaderpass_ProgramName,		"fte"},

	/*doom3 compat*/
	{"blend",		Shaderpass_BlendFunc,		"doom3"},
	{"maskcolor",	Shaderpass_MaskColor,		"doom3"},
	{"maskred",		Shaderpass_MaskRed,			"doom3"},
	{"maskgreen",	Shaderpass_MaskGreen,		"doom3"},
	{"maskblue",	Shaderpass_MaskBlue,		"doom3"},
	{"maskalpha",	Shaderpass_MaskAlpha,		"doom3"},
	{"alphatest",	Shaderpass_AlphaTest,		"doom3"},
	{"texgen",		Shaderpass_TexGen,			"doom3"},
	{"cubemap",		Shaderpass_CubeMap,			"doom3"},	//one of these is wrong
	{"cameracubemap",Shaderpass_CubeMap,		"doom3"},	//one of these is wrong
	{"red",			Shaderpass_Red,				"doom3"},
	{"green",		Shaderpass_Green,			"doom3"},
	{"blue",		Shaderpass_Blue,			"doom3"},
	{"alpha",		Shaderpass_Alpha,			"doom3"},
	{NULL,			NULL}
};

// ===============================================================


void Shader_FreePass (shaderpass_t *pass)
{
#ifdef HAVE_MEDIA_DECODER
	if ( pass->flags & SHADER_PASS_VIDEOMAP )
	{
		Media_ShutdownCin(pass->cin);
		pass->cin = NULL;
	}
#endif

	if (pass->prog)
	{
		Shader_ReleaseGeneric(pass->prog);
		pass->prog = NULL;
	}
}

void Shader_ReleaseGeneric(program_t *prog)
{
	if (prog)
		if (prog->refs-- == 1)
			Shader_UnloadProg(prog);
}
void Shader_Free (shader_t *shader)
{
	int i;
	shaderpass_t *pass;

	if (shader->bucket.data == shader)
		Hash_RemoveData(&shader_active_hash, shader->name, shader);
	shader->bucket.data = NULL;

	Shader_ReleaseGeneric(shader->prog);
	shader->prog = NULL;

	if (shader->skydome)
		Z_Free (shader->skydome);
	shader->skydome = NULL;
	while (shader->clutter)
	{
		void *t = shader->clutter;
		shader->clutter = shader->clutter->next;
		Z_Free(t);
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

	Z_Free(shader->defaulttextures);
	shader->defaulttextures = NULL;
}





int QDECL Shader_InitCallback (const char *name, qofs_t size, time_t mtime, void *param, searchpathfuncs_t *spath)
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

		if (!sh_config.progs_supported)
			sh_config.max_gpu_bones = 0;
		else
		{
			extern cvar_t r_max_gpu_bones;
			if (!*r_max_gpu_bones.string)
			{
#ifdef FTE_TARGET_WEB
				sh_config.max_gpu_bones = 0;	//webgl tends to crap out if this is too high, so 32 is a good enough value to play safe. some browsers have really shitty uniform performance too, so lets just default to pure-cpu transforms. in javascript. yes, its that bad.
#else
				sh_config.max_gpu_bones = 64;	//ATI drivers bug out and start to crash if you put this at 128.
#endif
			}
			else
				sh_config.max_gpu_bones = bound(0, r_max_gpu_bones.ival, MAX_BONES);
		}
	}
	
	memset(wibuf, 0xff, sizeof(wibuf));
	if (!qrenderer)
		r_whiteimage = r_nulltex;
	else
		r_whiteimage = R_LoadTexture("$whiteimage", 4, 4, TF_RGBA32, wibuf, IF_NOMIPMAP|IF_NOPICMIP|IF_NEAREST|IF_NOGAMMA);

	Shader_NeedReload(true);
	Shader_DoReload();
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
	texnums_t *dt = s->defaulttextures;
	int dtcount = s->numdefaulttextures;
	float dtrate = s->defaulttextures_fps;	//FIXME!
	int w = s->width;
	int h = s->height;
	unsigned int uf = s->usageflags;
	Q_strncpyz(name, s->name, sizeof(name));
	s->genargs = NULL;
	s->defaulttextures = NULL;
	Shader_Free(s);
	memset(s, 0, sizeof(*s));

	s->remapto = s;
	s->id = id;
	s->width = w;
	s->height = h;
	s->defaulttextures = dt;
	s->numdefaulttextures = dtcount;
	s->defaulttextures_fps = dtrate;
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
		if (pass->texgen == T_GEN_LIGHTMAP)
			pass->blendmode = PBM_OVERBRIGHT;
		else if ((pass->rgbgen == RGB_GEN_IDENTITY) && (pass->alphagen == ALPHA_GEN_IDENTITY))
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
		pass->blendmode = (pass->texgen == T_GEN_LIGHTMAP)?PBM_OVERBRIGHT:PBM_MODULATE;
	else if ((pass->shaderbits&SBITS_BLEND_BITS) == (SBITS_SRCBLEND_ONE|SBITS_DSTBLEND_ONE))
		pass->blendmode = PBM_ADD;
	else if ((pass->shaderbits&SBITS_BLEND_BITS) == (SBITS_SRCBLEND_SRC_ALPHA|SBITS_DSTBLEND_ONE_MINUS_SRC_ALPHA))
		pass->blendmode = PBM_DECAL;
	else
		pass->blendmode = (pass->texgen == T_GEN_LIGHTMAP)?PBM_OVERBRIGHT:PBM_MODULATE;
}

void Shader_FixupProgPasses(shader_t *shader, shaderpass_t *pass)
{
	int i;
	int maxpasses = SHADER_PASS_MAX - (pass-shader->passes);
	struct
	{
		int gen;
		unsigned int flags;
	} defaulttgen[] =
	{
		//light
		{T_GEN_SHADOWMAP,		0},						//1
		{T_GEN_LIGHTCUBEMAP,	0},						//2

		//material
		{T_GEN_DIFFUSE,			SHADER_HASDIFFUSE},		//3
		{T_GEN_NORMALMAP,		SHADER_HASNORMALMAP},	//4
		{T_GEN_SPECULAR,		SHADER_HASGLOSS},		//5
		{T_GEN_UPPEROVERLAY,	SHADER_HASTOPBOTTOM},	//6
		{T_GEN_LOWEROVERLAY,	SHADER_HASTOPBOTTOM},	//7
		{T_GEN_FULLBRIGHT,		SHADER_HASFULLBRIGHT},	//8
		{T_GEN_PALETTED,		SHADER_HASPALETTED},	//9
		{T_GEN_REFLECTCUBE,		0},						//10
		{T_GEN_REFLECTMASK,		0},						//11
//			{T_GEN_REFLECTION,		SHADER_HASREFLECT},		//
//			{T_GEN_REFRACTION,		SHADER_HASREFRACT},		//
//			{T_GEN_REFRACTIONDEPTH,	SHADER_HASREFRACTDEPTH},//
//			{T_GEN_RIPPLEMAP,		SHADER_HASRIPPLEMAP},	//

		//batch
		{T_GEN_LIGHTMAP,		SHADER_HASLIGHTMAP},	//12
		{T_GEN_DELUXMAP,		0},						//13
		//more lightmaps								//14,15,16
		//mode deluxemaps								//17,18,19
	};

#ifdef HAVE_MEDIA_DECODER
	cin_t *cin = R_ShaderGetCinematic(shader);
#endif

	//if the glsl doesn't specify all samplers, just trim them.
	pass->numMergedPasses = pass->prog->numsamplers;

#ifdef HAVE_MEDIA_DECODER
	if (cin && R_ShaderGetCinematic(shader) == cin)
		cin = NULL;
#endif

	//if the glsl has specific textures listed, be sure to provide a pass for them.
	for (i = 0; i < sizeof(defaulttgen)/sizeof(defaulttgen[0]); i++)
	{
		if (pass->prog->defaulttextures & (1u<<i))
		{
			if (pass->numMergedPasses >= maxpasses)
			{	//panic...
				parsestate.droppass = true;
				break;
			}
			pass[pass->numMergedPasses].flags |= SHADER_PASS_NOCOLORARRAY;
			pass[pass->numMergedPasses].flags &= ~SHADER_PASS_DEPTHCMP;
			if (defaulttgen[i].gen == T_GEN_SHADOWMAP)
				pass[pass->numMergedPasses].flags |= SHADER_PASS_DEPTHCMP;
#ifdef HAVE_MEDIA_DECODER
			if (!i && cin)
			{
				pass[pass->numMergedPasses].texgen = T_GEN_VIDEOMAP;
				pass[pass->numMergedPasses].cin = cin;
				cin = NULL;
			}
			else
#endif
			{
				pass[pass->numMergedPasses].texgen = defaulttgen[i].gen;
#ifdef HAVE_MEDIA_DECODER
				pass[pass->numMergedPasses].cin = NULL;
#endif
			}
			pass->numMergedPasses++;
			shader->flags |= defaulttgen[i].flags;
		}
	}

	//must have at least one texture.
	if (!pass->numMergedPasses)
	{
#ifdef HAVE_MEDIA_DECODER
		pass[0].texgen = cin?T_GEN_VIDEOMAP:T_GEN_DIFFUSE;
		pass[0].cin = cin;
#else
		pass[0].texgen = T_GEN_DIFFUSE;
#endif
		pass->numMergedPasses = 1;
	}
#ifdef HAVE_MEDIA_DECODER
	else if (cin)
		Media_ShutdownCin(cin);
#endif

	shader->numpasses = (pass-shader->passes)+pass->numMergedPasses;
}

void Shader_Readpass (shader_t *shader, char **ptr)
{
	char *token;
	shaderpass_t *pass;
	static shader_t dummy;
	int conddepth = 0;
	int cond[8] = {0};
	unsigned int oldflags = shader->flags;
#define COND_IGNORE 1
#define COND_IGNOREPARENT 2
#define COND_ALLOWELSE 4

	if ( shader->numpasses >= SHADER_PASS_MAX )
	{
		parsestate.droppass = true;
		shader = &dummy;
		shader->numpasses = 1;
		pass = shader->passes;
	}
	else
	{
		parsestate.droppass = false;
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
	pass->stagetype = ST_AMBIENT;
	pass->numMergedPasses = 0;

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

	//if there was no texgen, then its too late now.
	if (!pass->numMergedPasses)
		pass->numMergedPasses = 1;

	if (conddepth)
	{
		Con_Printf("if statements without endif in shader %s\n", shader->name);
	}

	if (pass->tcgen == TC_GEN_UNSPECIFIED)
		pass->tcgen = TC_GEN_BASE;

	if (!parsestate.droppass)
	{
		switch(pass->stagetype)
		{
		case ST_DIFFUSEMAP:
			if (pass->texgen == T_GEN_SINGLEMAP)
				shader->defaulttextures->base = pass->anim_frames[0];
			break;
		case ST_AMBIENT:
			break;
		case ST_BUMPMAP:
			if (pass->texgen == T_GEN_SINGLEMAP)
				shader->defaulttextures->bump = pass->anim_frames[0];
			parsestate.droppass = true;	//fixme: scrolling etc may be important. but we're not doom3.
			break;
		case ST_SPECULARMAP:
			if (pass->texgen == T_GEN_SINGLEMAP)
				shader->defaulttextures->specular = pass->anim_frames[0];
			parsestate.droppass = true;	//fixme: scrolling etc may be important. but we're not doom3.
			break;
		}
	}

	// check some things

	if (!parsestate.droppass)
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

	//if this pass specified a program, make sure it has all the textures that the program requires
	if (!parsestate.droppass && pass->prog)
		Shader_FixupProgPasses(shader, pass);

	if (parsestate.droppass)
	{
		while (pass->numMergedPasses > 0)
		{
			Shader_FreePass (pass+--pass->numMergedPasses);
			shader->numpasses--;
		}
		shader->flags = oldflags;
		return;
	}
}

static qboolean Shader_Parsetok (shader_t *shader, shaderpass_t *pass, shaderkey_t *keys, char *token, char **ptr)
{
	shaderkey_t *key;
	char *prefix;

	//handle known prefixes.
	if		(!Q_strncasecmp(token, "fte", 3))		{prefix = token; token += 3; }
	else if	(!Q_strncasecmp(token, "dp", 2))		{prefix = token; token += 2; }
	else if (!Q_strncasecmp(token, "doom3", 5))		{prefix = token; token += 5; }
	else if (!Q_strncasecmp(token, "rscript", 7))	{prefix = token; token += 7; }
	else	prefix = NULL;
	if (prefix && *token == '_')
		token++;

	for (key = keys; key->keyword != NULL; key++)
	{
		if (!Q_stricmp (token, key->keyword))
		{
			if (!prefix || (prefix && key->prefix && !Q_strncasecmp(prefix, key->prefix, strlen(key->prefix))))
			{
				if (key->func)
					key->func ( shader, pass, ptr );

				return ( ptr && *ptr && **ptr == '}' );
			}
		}
	}

	if (prefix)
		Con_DPrintf("Unknown shader directive parsing %s: \"%s\"\n", shader->name, prefix);
	else
		Con_DPrintf("Unknown shader directive parsing %s: \"%s\"\n", shader->name, token);

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

	//don't merge passes if they're got their own programs.
	if (pass->prog || pass2->prog)
		return;

	/*identity alpha is required for merging*/
	if (pass->alphagen != ALPHA_GEN_IDENTITY || pass2->alphagen != ALPHA_GEN_IDENTITY)
		return;

	/*don't merge passes if the hardware cannot support it*/
	if (pass->numMergedPasses >= be_maxpasses)
		return;

	/*rgbgen must be identity too except if the later pass is identity_ligting, in which case all is well and we can switch the first pass to identity_lighting instead*/
	if (pass2->rgbgen == RGB_GEN_IDENTITY_LIGHTING && pass2->blendmode == PBM_MODULATE && pass->rgbgen == RGB_GEN_IDENTITY)
	{
		if (pass->blendmode == PBM_REPLACE)
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
		else if ((pass->blendmode == PBM_MODULATE || pass->blendmode == PBM_OVERBRIGHT) && pass2->blendmode == PBM_MODULATE)
		{
			pass->numMergedPasses++;
		}
		else
			return;
	}
	else return;

	/*if (pass->texgen == T_GEN_LIGHTMAP && (pass->blendmode == PBM_REPLACE || pass->blendmode == PBM_REPLACELIGHT) && pass2->blendmode == PBM_MODULATE && sh_config.tex_env_combine)
	{
//		if (pass->rgbgen == RGB_GEN_IDENTITY)
//			pass->rgbgen = RGB_GEN_IDENTITY_OVERBRIGHT;	//get the light levels right
//		pass2->blendmode = PBM_OVERBRIGHT;
	}
	if (pass2->texgen == T_GEN_LIGHTMAP && pass2->blendmode == PBM_OVERBRIGHT && sh_config.tex_env_combine)
	{
//		if (pass->rgbgen == RGB_GEN_IDENTITY)
//			pass->rgbgen = RGB_GEN_IDENTITY_OVERBRIGHT;	//get the light levels right
//		pass->blendmode = PBM_REPLACELIGHT;
//		pass2->blendmode = PBM_OVERBRIGHT;
		pass->blendmode = PBM_OVERBRIGHT;
	}
	*/
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
	qboolean reflectrefract = false;
	char *prog = NULL;
	const char *mask;
/*	enum
	{
		T_UNKNOWN,
		T_WALL,
		T_MODEL
	} type = 0;*/
	int i;
	shaderpass_t *pass, *lightmap = NULL, *modellighting = NULL, *vertexlighting = NULL;
	for (i = 0; i < s->numpasses; i++)
	{
		pass = &s->passes[i];
		if (pass->rgbgen == RGB_GEN_LIGHTING_DIFFUSE)
			modellighting = pass;
		else if (pass->rgbgen == RGB_GEN_ENTITY)
			modellighting = pass;
		else if (pass->rgbgen == RGB_GEN_VERTEX_LIGHTING || pass->rgbgen == RGB_GEN_VERTEX_EXACT)
			vertexlighting = pass;
		else if (pass->texgen == T_GEN_LIGHTMAP && pass->tcgen == TC_GEN_LIGHTMAP)
			lightmap = pass;

		/*if (pass->numtcmods || (pass->shaderbits & SBITS_ATEST_BITS))
			return;
		if (pass->texgen == T_GEN_LIGHTMAP && pass->tcgen == TC_GEN_LIGHTMAP)
			;
		else if (pass->texgen != T_GEN_LIGHTMAP && pass->tcgen == TC_GEN_BASE)
			;
		else
			return;*/
	}

	if (parsestate.dpwatertype)
	{
		prog = va("altwater%s#USEMODS#FRESNEL_EXP=2.0"
				//variable parts
				"#STRENGTH_REFR=%g#STRENGTH_REFL=%g"
				"#TINT_REFR=%g,%g,%g"
				"#TINT_REFL=%g,%g,%g"
				"#FRESNEL_MIN=%g#FRESNEL_RANGE=%g"
				"#ALPHA=%g",
				//those args
				(parsestate.dpwatertype&1)?"#REFLECT":"",
				parsestate.refractfactor*0.01, parsestate.reflectfactor*0.01,
				parsestate.refractcolour[0],parsestate.refractcolour[1],parsestate.refractcolour[2],
				parsestate.reflectcolour[0],parsestate.reflectcolour[1],parsestate.reflectcolour[2],
				parsestate.reflectmin, parsestate.reflectmax-parsestate.reflectmin,
				parsestate.wateralpha
			);
		//clear out blending and force regular depth.
		s->passes[0].shaderbits &= ~(SBITS_BLEND_BITS|SBITS_MISC_NODEPTHTEST|SBITS_MISC_DEPTHEQUALONLY|SBITS_MISC_DEPTHCLOSERONLY);
		s->passes[0].shaderbits |= SBITS_MISC_DEPTHWRITE;

		reflectrefract = true;
	}
	else if (modellighting)
	{
		pass = modellighting;
		prog = "defaultskin";
	}
	else if (lightmap)
	{
		pass = modellighting;
		prog = "defaultwall";
	}
	else if (vertexlighting)
	{
		pass = vertexlighting;
		prog = "default2d";
	}
	else
	{
		pass = NULL;
		prog = "default2d";
		return;
	}

	mask = Shader_AlphaMaskProgArgs(s);

	s->prog = Shader_FindGeneric(va("%s%s", prog, mask), qrenderer);
	if (s->prog)
	{
		s->numpasses = 0;
		if (reflectrefract)
		{
			if (s->passes[0].numtcmods > 0 && s->passes[0].tcmods[0].type == SHADER_TCMOD_SCALE)
			{	//crappy workaround for DP bug.
				s->passes[0].tcmods[0].args[0] *= 4;
				s->passes[0].tcmods[0].args[1] *= 4;
			}
			s->passes[s->numpasses++].texgen = T_GEN_REFRACTION;
			s->passes[s->numpasses++].texgen = T_GEN_REFLECTION;
//			s->passes[s->numpasses++].texgen = T_GEN_RIPPLEMAP;
//			s->passes[s->numpasses++].texgen = T_GEN_REFRACTIONDEPTH;
			s->flags |= SHADER_HASREFRACT;
			s->flags |= SHADER_HASREFLECT;
		}
		else
		{
			s->passes[s->numpasses++].texgen = T_GEN_DIFFUSE;
			s->flags |= SHADER_HASDIFFUSE;
		}
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

	if (s->prog && !s->numpasses)
	{
		pass = &s->passes[s->numpasses++];
		pass->tcgen = TC_GEN_BASE;
		pass->texgen = T_GEN_DIFFUSE;
		pass->shaderbits |= SBITS_MISC_DEPTHWRITE;
		pass->rgbgen = RGB_GEN_IDENTITY;
		pass->alphagen = ALPHA_GEN_IDENTITY;
		pass->numMergedPasses = 1;
		Shader_SetBlendmode(pass);
	}

	if (!s->numpasses && s->sort != SHADER_SORT_PORTAL && !(s->flags & (SHADER_NODRAW|SHADER_SKY)) && !s->fog_dist)
	{
		pass = &s->passes[s->numpasses++];
		pass = &s->passes[0];
		pass->tcgen = TC_GEN_BASE;
		if (TEXVALID(s->defaulttextures->base))
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

		if (!r_vertexlight.value && pass->rgbgen == RGB_GEN_IDENTITY)
		{	//we found the lightmap pass. if we need a vertex-lit shader then just switch over the rgbgen+texture and hope other things work out
			pass->rgbgen = RGB_GEN_VERTEX_LIGHTING;
			pass->flags &= ~SHADER_PASS_LIGHTMAP;
			pass->tcgen = T_GEN_SINGLEMAP;
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
 	if (!TEXVALID(s->defaulttextures->specular) && !(s->flags & SHADER_HASGLOSS))
	{
		for (pass = s->passes, i = 0; i < s->numpasses; i++, pass++)
		{
			if (pass->alphagen == ALPHA_GEN_SPECULAR)
				if (pass->texgen == T_GEN_ANIMMAP || pass->texgen == T_GEN_SINGLEMAP)
					s->defaulttextures->specular = pass->anim_frames[0];
		}
	}

	if (!TEXVALID(s->defaulttextures->base) && !(s->flags & SHADER_HASDIFFUSE) && !s->prog)
	{	//if one of the other passes specifies $diffuse, don't try and guess one, because that means that other pass's texture gets used for BOTH passes, which isn't good.
		//also, don't guess one if a program was specified.
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

			if ((pass->texgen == T_GEN_ANIMMAP || pass->texgen == T_GEN_SINGLEMAP) && pass->anim_frames[0] && *pass->anim_frames[0]->ident == '$')
				weight += 1500;
			
			if (weight < bestweight)
			{
				bestweight = weight;
				best = pass;
			}
		}

		if (best)
		{
			if (best->texgen == T_GEN_ANIMMAP || best->texgen == T_GEN_SINGLEMAP)
			{
				if (best->anim_frames[0] && *best->anim_frames[0]->ident != '$')
					s->defaulttextures->base = best->anim_frames[0];
			}
#ifdef HAVE_MEDIA_DECODER
			else if (pass->texgen == T_GEN_VIDEOMAP && pass->cin)
				s->defaulttextures->base = Media_UpdateForShader(best->cin);
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
				s->defaulttextures->fullbright = pass->anim_frames[0];
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
			for (j = i + pass->numMergedPasses; j < s->numpasses-i && j == i + pass->numMergedPasses && j < be_maxpasses; j++)
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
		if (*mask || (s->prog&&s->prog->tess))
			s->bemoverrides[bemoverride_depthonly] = R_RegisterShader(va("depthonly%s%s", mask, (s->prog&&s->prog->tess)?"#TESS":""), SUF_NONE, 
				"{\n"
					"program depthonly\n"
					"{\n"
						"map $diffuse\n"
						"depthwrite\n"
						"maskcolor\n"
					"}\n"
				"}\n");
	}
	if (!s->bemoverrides[LSHADER_STANDARD] && (s->prog&&s->prog->tess))
	{
		int mode;
		for (mode = 0; mode < LSHADER_MODES; mode++)
		{
			if ((mode & LSHADER_CUBE) && (mode & LSHADER_SPOT))
				continue;
			if (s->bemoverrides[mode])
				continue;
			s->bemoverrides[mode] = R_RegisterShader(va("rtlight%s%s%s%s#TESS", 
																(mode & LSHADER_SMAP)?"#PCF":"",
																(mode & LSHADER_SPOT)?"#SPOT":"",
																(mode & LSHADER_CUBE)?"#CUBE":"",
#ifdef GLQUAKE
																(qrenderer == QR_OPENGL && gl_config.arb_shadow && (mode & (LSHADER_SMAP|LSHADER_SPOT)))?"#USE_ARB_SHADOW":""
#else
																""
#endif
																)
														, s->usageflags, 
				"{\n"
					"program rtlight\n"
					"{\n"
						"map $diffuse\n"
						"blendfunc add\n"
					"}\n"
				"}\n");
		}
	}

	if (!s->prog && sh_config.progs_supported && (r_forceprogramify.ival || parsestate.forceprogramify))
	{
		Shader_Programify(s);
		if (r_forceprogramify.ival >= 2)
		{
			if (s->passes[0].numtcmods == 1 && s->passes[0].tcmods[0].type == SHADER_TCMOD_SCALE)
				s->passes[0].numtcmods = 0;	//DP sucks and doesn't use normalized texture coords *if* there's a shader specified. so lets ignore any extra scaling that this imposes.
			if (s->passes[0].shaderbits & SBITS_ATEST_BITS)	//mimic DP's limited alphafunc support
				s->passes[0].shaderbits = (s->passes[0].shaderbits & ~SBITS_ATEST_BITS) | SBITS_ATEST_GE128;
			s->passes[0].shaderbits &= ~SBITS_MISC_DEPTHEQUALONLY;	//DP ignores this too.
		}
	}

	if (s->prog)
	{
		struct
		{
			int gen;
			unsigned int flags;
		} defaulttgen[] =
		{
			//light
			{T_GEN_SHADOWMAP,		0},						//1
			{T_GEN_LIGHTCUBEMAP,	0},						//2

			//material
			{T_GEN_DIFFUSE,			SHADER_HASDIFFUSE},		//3
			{T_GEN_NORMALMAP,		SHADER_HASNORMALMAP},	//4
			{T_GEN_SPECULAR,		SHADER_HASGLOSS},		//5
			{T_GEN_UPPEROVERLAY,	SHADER_HASTOPBOTTOM},	//6
			{T_GEN_LOWEROVERLAY,	SHADER_HASTOPBOTTOM},	//7
			{T_GEN_FULLBRIGHT,		SHADER_HASFULLBRIGHT},	//8
			{T_GEN_PALETTED,		SHADER_HASPALETTED},	//9
			{T_GEN_REFLECTCUBE,		0},						//10
			{T_GEN_REFLECTMASK,		0},						//11
//			{T_GEN_REFLECTION,		SHADER_HASREFLECT},		//
//			{T_GEN_REFRACTION,		SHADER_HASREFRACT},		//
//			{T_GEN_REFRACTIONDEPTH,	SHADER_HASREFRACTDEPTH},//
//			{T_GEN_RIPPLEMAP,		SHADER_HASRIPPLEMAP},	//

			//batch
			{T_GEN_LIGHTMAP,		SHADER_HASLIGHTMAP},	//12
			{T_GEN_DELUXMAP,		0},						//13
			//more lightmaps								//14,15,16
			//mode deluxemaps								//17,18,19
		};

#ifdef HAVE_MEDIA_DECODER
		cin_t *cin = R_ShaderGetCinematic(s);
#endif

		//if the glsl doesn't specify all samplers, just trim them.
		s->numpasses = s->prog->numsamplers;

#ifdef HAVE_MEDIA_DECODER
		if (cin && R_ShaderGetCinematic(s) == cin)
			cin = NULL;
#endif

		//if the glsl has specific textures listed, be sure to provide a pass for them.
		for (i = 0; i < sizeof(defaulttgen)/sizeof(defaulttgen[0]); i++)
		{
			if (s->prog->defaulttextures & (1u<<i))
			{
				if (s->numpasses >= SHADER_PASS_MAX)
					break;	//panic...
				s->passes[s->numpasses].flags &= ~SHADER_PASS_DEPTHCMP;
				if (defaulttgen[i].gen == T_GEN_SHADOWMAP)
					s->passes[s->numpasses].flags |= SHADER_PASS_DEPTHCMP;
#ifdef HAVE_MEDIA_DECODER
				if (!i && cin)
				{
					s->passes[s->numpasses].texgen = T_GEN_VIDEOMAP;
					s->passes[s->numpasses].cin = cin;
					cin = NULL;
				}
				else
#endif
				{
					s->passes[s->numpasses].texgen = defaulttgen[i].gen;
#ifdef HAVE_MEDIA_DECODER
					s->passes[s->numpasses].cin = NULL;
#endif
				}
				s->numpasses++;
				s->flags |= defaulttgen[i].flags;
			}
		}

		//must have at least one texture.
		if (!s->numpasses)
		{
#ifdef HAVE_MEDIA_DECODER
			s->passes[0].texgen = cin?T_GEN_VIDEOMAP:T_GEN_DIFFUSE;
			s->passes[0].cin = cin;
#else
			s->passes[0].texgen = T_GEN_DIFFUSE;
#endif
			s->numpasses = 1;
		}
#ifdef HAVE_MEDIA_DECODER
		else if (cin)
			Media_ShutdownCin(cin);
#endif
		s->passes->numMergedPasses = s->numpasses;
	}
	else if (s->numdeforms)
		s->flags |= SHADER_NEEDSARRAYS;
	else
	{
		for (i = 0; i < s->numpasses; i++)
		{
			pass = &s->passes[i];
			if (pass->numtcmods || (s->passes[i].tcgen != TC_GEN_BASE && s->passes[i].tcgen != TC_GEN_LIGHTMAP) || !(s->passes[i].flags & SHADER_PASS_NOCOLORARRAY))
			{
				s->flags |= SHADER_NEEDSARRAYS;
				break;
			}
			if (!(pass->flags & SHADER_PASS_NOCOLORARRAY))
			{
				if (!(((pass->rgbgen == RGB_GEN_VERTEX_LIGHTING) ||
					(pass->rgbgen == RGB_GEN_VERTEX_EXACT) ||
					(pass->rgbgen == RGB_GEN_ONE_MINUS_VERTEX)) &&
					(pass->alphagen == ALPHA_GEN_VERTEX)))
				{
					s->flags |= SHADER_NEEDSARRAYS;
					break;
				}
			}
		}
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

/*
	if (*shader_diffusemapname)
	{
		if (!s->defaulttextures.base)
			s->defaulttextures.base = Shader_FindImage (va("%s.tga", shader_diffusemapname), 0);
		if (!s->defaulttextures.bump)
			s->defaulttextures.bump = Shader_FindImage (va("%s_norm.tga", shader_diffusemapname), 0);
		if (!s->defaulttextures.fullbright)
			s->defaulttextures.fullbright = Shader_FindImage (va("%s_glow.tga", shader_diffusemapname), 0);
		if (!s->defaulttextures.specular)
			s->defaulttextures.specular = Shader_FindImage (va("%s_gloss.tga", shader_diffusemapname), 0);
		if (!s->defaulttextures.upperoverlay)
			s->defaulttextures.upperoverlay = Shader_FindImage (va("%s_shirt.tga", shader_diffusemapname), 0);
		if (!s->defaulttextures.loweroverlay)
			s->defaulttextures.loweroverlay = Shader_FindImage (va("%s_pants.tga", shader_diffusemapname), 0);	//stupid yanks...
	}
*/
void Shader_DefaultSkin(const char *shortname, shader_t *s, const void *args);
void QDECL R_BuildDefaultTexnums(texnums_t *src, shader_t *shader)
{
	char *h;
	char imagename[MAX_QPATH];
	char mapname[MAX_QPATH];
	char *subpath = NULL;
	texnums_t *tex;
	unsigned int a, aframes;
	unsigned int imageflags = 0;
	strcpy(imagename, shader->name);
	h = strchr(imagename, '#');
	if (h)
		*h = 0;
	if (*imagename == '/' || strchr(imagename, ':'))
	{	//this is not security. this is anti-spam for the verbose security in the filesystem code.
		Con_Printf("Warning: shader has absolute path: %s\n", shader->name);
		*imagename = 0;
	}

	//skins can use an alternative path in certain cases, to work around dodgy models.
	if (shader->generator == Shader_DefaultSkin)
		subpath = shader->genargs;

	tex = shader->defaulttextures;
	aframes = max(1, shader->numdefaulttextures);
	//if any were specified explicitly, replicate that into all.
	//this means animmap can be used, with any explicit textures overriding all.
	if (!shader->numdefaulttextures && src)
	{
		//only do this if there wasn't an animmap thing to break everything.
		if (!TEXVALID(tex->base))
			tex->base			= src->base;
		if (!TEXVALID(tex->bump))
			tex->bump			= src->bump;
		if (!TEXVALID(tex->fullbright))
			tex->fullbright		= src->fullbright;
		if (!TEXVALID(tex->specular))
			tex->specular		= src->specular;
		if (!TEXVALID(tex->loweroverlay))
			tex->loweroverlay	= src->loweroverlay;
		if (!TEXVALID(tex->upperoverlay))
			tex->upperoverlay	= src->upperoverlay;
		if (!TEXVALID(tex->reflectmask))
			tex->reflectmask	= src->reflectmask;
		if (!TEXVALID(tex->reflectcube))
			tex->reflectcube	= src->reflectcube;
	}
	for (a = 1; a < aframes; a++)
	{
		if (!TEXVALID(tex[a].base))
			tex[a].base			= tex[0].base;
		if (!TEXVALID(tex[a].bump))
			tex[a].bump			= tex[0].bump;
		if (!TEXVALID(tex[a].fullbright))
			tex[a].fullbright	= tex[0].fullbright;
		if (!TEXVALID(tex[a].specular))
			tex[a].specular		= tex[0].specular;
		if (!TEXVALID(tex[a].loweroverlay))
			tex[a].loweroverlay	= tex[0].loweroverlay;
		if (!TEXVALID(tex[a].upperoverlay))
			tex[a].upperoverlay	= tex[0].upperoverlay;
		if (!TEXVALID(tex[a].reflectmask))
			tex[a].reflectmask	= tex[0].reflectmask;
		if (!TEXVALID(tex[a].reflectcube))
			tex[a].reflectcube	= tex[0].reflectcube;
	}
	for (a = 0; a < aframes; a++, tex++)
	{
		COM_StripExtension(tex->mapname, mapname, sizeof(mapname));

		if (!TEXVALID(tex->base))
		{
			/*dlights/realtime lighting needs some stuff*/
			if (!TEXVALID(tex->base) && *tex->mapname)// && (shader->flags & SHADER_HASDIFFUSE))
				tex->base = R_LoadHiResTexture(tex->mapname, NULL, 0);

			if (!TEXVALID(tex->base))
				tex->base = R_LoadHiResTexture(imagename, subpath, (*imagename=='{')?0:IF_NOALPHA);
		}

		if ((shader->flags & SHADER_HASPALETTED) && !TEXVALID(tex->paletted))
		{
			if (!TEXVALID(tex->paletted) && *tex->mapname)
				tex->paletted = R_LoadHiResTexture(va("%s", tex->mapname), NULL, 0|IF_NEAREST|IF_PALETTIZE);
			if (!TEXVALID(tex->paletted))
				tex->paletted = R_LoadHiResTexture(va("%s", imagename), subpath, ((*imagename=='{')?0:IF_NOALPHA)|IF_NEAREST|IF_PALETTIZE);
		}

		imageflags |= IF_LOWPRIORITY;

		COM_StripExtension(imagename, imagename, sizeof(imagename));

		if (!TEXVALID(tex->bump))
		{
			if (r_loadbumpmapping || (shader->flags & SHADER_HASNORMALMAP))
			{
				if (!TEXVALID(tex->bump) && *mapname && (shader->flags & SHADER_HASNORMALMAP))
					tex->bump = R_LoadHiResTexture(va("%s_norm", mapname), NULL, imageflags|IF_TRYBUMP);
				if (!TEXVALID(tex->bump))
					tex->bump = R_LoadHiResTexture(va("%s_norm", imagename), subpath, imageflags|IF_TRYBUMP);
			}
		}

		if (!TEXVALID(tex->loweroverlay))
		{
			if (shader->flags & SHADER_HASTOPBOTTOM)
			{
				if (!TEXVALID(tex->loweroverlay) && *mapname)
					tex->loweroverlay = R_LoadHiResTexture(va("%s_pants", mapname), NULL, imageflags);
				if (!TEXVALID(tex->loweroverlay))
					tex->loweroverlay = R_LoadHiResTexture(va("%s_pants", imagename), subpath, imageflags);	/*how rude*/
			}
		}

		if (!TEXVALID(tex->upperoverlay))
		{
			if (shader->flags & SHADER_HASTOPBOTTOM)
			{
				if (!TEXVALID(tex->upperoverlay) && *mapname)
					tex->upperoverlay = R_LoadHiResTexture(va("%s_shirt", mapname), NULL, imageflags);
				if (!TEXVALID(tex->upperoverlay))
					tex->upperoverlay = R_LoadHiResTexture(va("%s_shirt", imagename), subpath, imageflags);
			}
		}

		if (!TEXVALID(tex->specular))
		{
			extern cvar_t gl_specular;
			if ((shader->flags & SHADER_HASGLOSS) && gl_specular.value && gl_load24bit.value)
			{
				if (!TEXVALID(tex->specular) && *mapname)
					tex->specular = R_LoadHiResTexture(va("%s_gloss", mapname), NULL, imageflags);
				if (!TEXVALID(tex->specular))
					tex->specular = R_LoadHiResTexture(va("%s_gloss", imagename), subpath, imageflags);
			}
		}

		if (!TEXVALID(tex->fullbright))
		{
			extern cvar_t r_fb_bmodels;
			if ((shader->flags & SHADER_HASFULLBRIGHT) && r_fb_bmodels.value && gl_load24bit.value)
			{
				if (!TEXVALID(tex->fullbright) && *mapname)
					tex->fullbright = R_LoadHiResTexture(va("%s_luma:%s_glow", mapname, mapname), NULL, imageflags);
				if (!TEXVALID(tex->fullbright))
					tex->fullbright = R_LoadHiResTexture(va("%s_luma:%s_glow", imagename, imagename), subpath, imageflags);
			}
		}
	}
}

//call this with some fallback textures to directly load some textures
void QDECL R_BuildLegacyTexnums(shader_t *shader, const char *fallbackname, const char *subpath, unsigned int loadflags, unsigned int imageflags, uploadfmt_t basefmt, size_t width, size_t height, qbyte *mipdata[4], qbyte *palette)
{
	char *h;
	char imagename[MAX_QPATH];
	char mapname[MAX_QPATH];	//as specified by the shader.
	//extern cvar_t gl_miptexLevel;
	texnums_t *tex = shader->defaulttextures;
	int a, aframes;
	qbyte *dontcrashme[4] = {NULL};
	if (!mipdata)
		mipdata = dontcrashme;
	/*else if (gl_miptexLevel.ival)
	{
		unsigned int miplevel = 0, i;
		for (i = 0; i < 3 && i < gl_miptexLevel.ival && mipdata[i]; i++)
			miplevel = i;
		for (i = 0; i < 3; i++)
			dontcrashme[i] = (miplevel+i)>3?NULL:mipdata[miplevel+i];
		width >>= miplevel;
		height >>= miplevel;
		mipdata = dontcrashme;
	}
	*/
	strcpy(imagename, shader->name);
	h = strchr(imagename, '#');
	if (h)
		*h = 0;
	if (*imagename == '/' || strchr(imagename, ':'))
	{	//this is not security. this is anti-spam for the verbose security in the filesystem code.
		Con_Printf("Warning: shader has absolute path: %s\n", shader->name);
		*imagename = 0;
	}

	//for water texture replacements
	while((h = strchr(imagename, '*')))
		*h = '#';

	loadflags &= shader->flags;

	//skins can use an alternative path in certain cases, to work around dodgy models.
	if (shader->generator == Shader_DefaultSkin)
		subpath = shader->genargs;

	//optimise away any palette info if we can...
	if (!palette || palette == host_basepal)
	{
		if (basefmt == TF_MIP4_8PAL24)
			basefmt = TF_MIP4_SOLID8;
//		if (basefmt == TF_MIP4_8PAL24_T255)
//			basefmt = TF_MIP4_TRANS8;
	}

	//make sure the noalpha thing is set properly.
	switch(basefmt)
	{
	case TF_MIP4_8PAL24:
	case TF_MIP4_SOLID8:
	case TF_SOLID8:
		imageflags |= IF_NOALPHA;
		if (!mipdata[0] || !mipdata[1] || !mipdata[2] || !mipdata[3])
			basefmt = TF_SOLID8;
		break;
	default:
		if (!mipdata[0] || !mipdata[1] || !mipdata[2] || !mipdata[3])
			basefmt = TF_SOLID8;
		break;
	}
	imageflags |= IF_MIPCAP;

	COM_StripExtension(imagename, imagename, sizeof(imagename));

	aframes = max(1, shader->numdefaulttextures);
	//if any were specified explicitly, replicate that into all.
	//this means animmap can be used, with any explicit textures overriding all.
	for (a = 1; a < aframes; a++)
	{
		if (!TEXVALID(tex[a].base))
			tex[a].base			= tex[0].base;
		if (!TEXVALID(tex[a].bump))
			tex[a].bump			= tex[0].bump;
		if (!TEXVALID(tex[a].fullbright))
			tex[a].fullbright	= tex[0].fullbright;
		if (!TEXVALID(tex[a].specular))
			tex[a].specular		= tex[0].specular;
		if (!TEXVALID(tex[a].loweroverlay))
			tex[a].loweroverlay	= tex[0].loweroverlay;
		if (!TEXVALID(tex[a].upperoverlay))
			tex[a].upperoverlay	= tex[0].upperoverlay;
		if (!TEXVALID(tex[a].reflectmask))
			tex[a].reflectmask	= tex[0].reflectmask;
		if (!TEXVALID(tex[a].reflectcube))
			tex[a].reflectcube	= tex[0].reflectcube;
	}
	for (a = 0; a < aframes; a++, tex++)
	{
		COM_StripExtension(tex->mapname, mapname, sizeof(mapname));

		/*dlights/realtime lighting needs some stuff*/
		if (loadflags & SHADER_HASDIFFUSE)
		{
			if (!TEXVALID(tex->base) && *mapname)
				tex->base = R_LoadHiResTexture(mapname, NULL, imageflags);
			if (!TEXVALID(tex->base) && fallbackname)
			{
				if (gl_load24bit.ival)
				{
					tex->base = Image_GetTexture(imagename, subpath, imageflags|IF_NOWORKER, NULL, NULL, width, height, basefmt);
					if (!TEXLOADED(tex->base))
					{
						tex->base = Image_GetTexture(fallbackname, subpath, imageflags|IF_NOWORKER, NULL, NULL, width, height, basefmt);
						if (TEXLOADED(tex->base))
							Q_strncpyz(imagename, fallbackname, sizeof(imagename));
					}
				}
				if (!TEXLOADED(tex->base))
					tex->base = Image_GetTexture(imagename, subpath, imageflags, mipdata[0], palette, width, height, basefmt);
			}
			else if (!TEXVALID(tex->base))
				tex->base = Image_GetTexture(imagename, subpath, imageflags, mipdata[0], palette, width, height, basefmt);
		}

		if (loadflags & SHADER_HASPALETTED)
		{
			if (!TEXVALID(tex->paletted) && *mapname)
				tex->paletted = R_LoadHiResTexture(va("%s_pal", mapname), NULL, imageflags|IF_NEAREST);
			if (!TEXVALID(tex->paletted))
				tex->paletted = Image_GetTexture(va("%s_pal", imagename), subpath, imageflags|IF_NEAREST, mipdata[0], palette, width, height, (basefmt==TF_MIP4_SOLID8)?TF_MIP4_LUM8:TF_LUM8);
		}

		imageflags |= IF_LOWPRIORITY;
		//all the rest need/want an alpha channel in some form.
		imageflags &= ~IF_NOALPHA;
		imageflags |= IF_NOGAMMA;

		if (loadflags & SHADER_HASNORMALMAP||*imagename=='#')
		{
			extern cvar_t r_shadow_bumpscale_basetexture;
			if (!TEXVALID(tex->bump) && *mapname)
				tex->bump = R_LoadHiResTexture(va("%s_norm", mapname), NULL, imageflags|IF_TRYBUMP);
			if (!TEXVALID(tex->bump) && (r_shadow_bumpscale_basetexture.ival||*imagename=='#'||gl_load24bit.ival))
				tex->bump = Image_GetTexture(va("%s_norm", imagename), subpath, imageflags|IF_TRYBUMP|(*imagename=='#'?IF_LINEAR:0), (r_shadow_bumpscale_basetexture.ival||*imagename=='#')?mipdata[0]:NULL, palette, width, height, TF_HEIGHT8PAL);
		}

		if (loadflags & SHADER_HASTOPBOTTOM)
		{
			if (!TEXVALID(tex->loweroverlay) && *mapname)
				tex->loweroverlay = R_LoadHiResTexture(va("%s_pants", mapname), NULL, imageflags);
			if (!TEXVALID(tex->loweroverlay))
				tex->loweroverlay = Image_GetTexture(va("%s_pants", imagename), subpath, imageflags, NULL, palette, width, height, 0);
		}
		if (loadflags & SHADER_HASTOPBOTTOM)
		{
			if (!TEXVALID(tex->upperoverlay) && *mapname)
				tex->upperoverlay = R_LoadHiResTexture(va("%s_shirt", mapname), NULL, imageflags);
			if (!TEXVALID(tex->upperoverlay))
				tex->upperoverlay = Image_GetTexture(va("%s_shirt", imagename), subpath, imageflags, NULL, palette, width, height, 0);
		}

		if (loadflags & SHADER_HASGLOSS)
		{
			if (!TEXVALID(tex->specular) && *mapname)
				tex->specular = R_LoadHiResTexture(va("%s_gloss", mapname), NULL, imageflags);
			if (!TEXVALID(tex->specular))
				tex->specular = Image_GetTexture(va("%s_gloss", imagename), subpath, imageflags, NULL, palette, width, height, 0);
		}
		
		if (tex->reflectcube)
		{
			extern cvar_t r_shadow_bumpscale_basetexture;
			if (!TEXVALID(tex->reflectmask) && *mapname)
				tex->reflectmask = R_LoadHiResTexture(va("%s_reflect", mapname), NULL, imageflags);
			if (!TEXVALID(tex->reflectmask))
				tex->reflectmask = Image_GetTexture(va("%s_reflect", imagename), subpath, imageflags, NULL, NULL, width, height, TF_INVALID);
		}

		if (loadflags & SHADER_HASFULLBRIGHT)
		{
			if (!TEXVALID(tex->fullbright) && *mapname)
				tex->fullbright = R_LoadHiResTexture(va("%s_luma", mapname), NULL, imageflags);
			if (!TEXVALID(tex->fullbright))
			{
				int s=-1;
				if (mipdata[0] && (!palette || palette == host_basepal))
				for(s = width*height-1; s>=0; s--)
				{
					if (mipdata[0][s] >= 256-vid.fullbright)
						break;
				}
				tex->fullbright = Image_GetTexture(va("%s_luma:%s_glow", imagename,imagename), subpath, imageflags, (s>=0)?mipdata[0]:NULL, palette, width, height, TF_TRANS8_FULLBRIGHT);
			}
		}
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
	if (Shader_ParseShader("defaultwall", s))
		return;

	if (!builtin && r_softwarebanding && (qrenderer == QR_OPENGL || qrenderer == QR_VULKAN) && sh_config.progs_supported)
		builtin = (
				"{\n"
					"{\n"
						"program defaultwall#EIGHTBIT\n"
						"map $colourmap\n"
					"}\n"
				"}\n"
			);

	if (!builtin && r_lightmap.ival)
		builtin = (
				"{\n"
					"fte_program drawflat_wall\n"
					"{\n"
						"map $lightmap\n"
						"tcgen lightmap\n"
						"rgbgen const 255 255 255\n"
					"}\n"
				"}\n"
			);

	if (!builtin && r_drawflat.ival)
		builtin = (
				"{\n"
					"fte_program drawflat_wall\n"
					"{\n"
						"map $lightmap\n"
						"tcgen lightmap\n"
						"rgbgen const $r_floorcolour\n"
					"}\n"
				"}\n"
			);


	if (!builtin && r_lightprepass)
	{
		builtin = (
			"{\n"
				"fte_program lpp_wall\n"
				"{\n"
					"map $sourcecolour\n"
				"}\n"

				//this is drawn during the gbuffer pass to prepare it
				"fte_bemode gbuffer\n"
				"{\n"
					"fte_program lpp_depthnorm\n"
					"{\n"
						"map $normalmap\n"
						"tcgen base\n"
					"}\n"
				"}\n"
			"}\n"
		);
	}
	if (!builtin && ((sh_config.progs_supported && qrenderer == QR_OPENGL) || sh_config.progs_required))
	{
			builtin = (
					"{\n"
						"fte_program defaultwall\n"
						"{\n"
							//FIXME: these maps are a legacy thing, and could be removed if third-party glsl properly contains s_diffuse
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

	if (r_lightprepass)
		s->flags |= SHADER_HASNORMALMAP;
}

void Shader_DefaultCinematic(const char *shortname, shader_t *s, const void *args)
{
	Shader_DefaultScript(shortname, s,
		va(
			"{\n"
				"program default2d\n"
				"{\n"
					"videomap \"%s\"\n"
					"blendfunc gl_one gl_one_minus_src_alpha\n"
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

	if (!strncmp(shortname, "*portal", 7))
	{
		return	"{\n"
					"portal\n"
				"}\n";
	}
	else if (!strncmp(shortname, "*lava", 5))
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
	{
		if (ruleset_allow_watervis.ival)
			alpha = *alphavars[type]->string?alphavars[type]->value:alphavars[0]->value;
		else
			alpha = 1;
	}

	if (alpha <= 0)
		wstyle = -1;
	else if (r_fastturb.ival)
		wstyle = 0;
	else if (*stylevars[type]->string)
		wstyle = stylevars[type]->ival;
	else if (stylevars[0]->ival > 0)
		wstyle = stylevars[0]->ival;
	else
		wstyle = 1;

	if (wstyle > 1 && !ruleset_allow_watervis.ival)
		wstyle = 1;

	if (wstyle > 1 && !sh_config.progs_supported)
		wstyle = 1;

	//extra silly limitations
	switch(qrenderer)
	{
#ifdef GLQUAKE
	case QR_OPENGL:
		if (wstyle > 2 && !gl_config.ext_framebuffer_objects)
			wstyle = 2;
		break;
#endif
#ifdef VKQUAKE
	case QR_VULKAN:
		if (wstyle > 3)
			wstyle = 3;
		break;
#endif
	default:	//altwater not supported with other renderers
		if (wstyle > 1)
			wstyle = 1;
	}

	switch(wstyle)
	{
	case -1:	//invisible
		return (
			"{\n"
				"surfaceparm nodraw\n"
				"surfaceparm nodlight\n"
				"surfaceparm nomarks\n"
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
				"surfaceparm nomarks\n"
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
				"surfaceparm nomarks\n"
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
					"surfaceparm nomarks\n"
				"}\n"
				, explicitalpha?"":va("#ALPHA=%g",alpha), alpha, alpha);
	case 2:	//refraction of the underwater surface, with a fresnel
		return (
			"{\n"
				"surfaceparm nodlight\n"
				"surfaceparm nomarks\n"
				"{\n"
					"map $refraction\n"
				"}\n"
				"{\n"
					"map $null\n"//$reflection
				"}\n"
				"{\n"
					"map $null\n"//$ripplemap
				"}\n"
				"{\n"
					"map $null\n"//$refractiondepth
				"}\n"
				"program altwater#FRESNEL=4\n"
			"}\n"
		);
	case 3:	//reflections
		return (
			"{\n"
				"surfaceparm nodlight\n"
				"surfaceparm nomarks\n"
				"{\n"
					"map $refraction\n"
				"}\n"
				"{\n"
					"map $reflection\n"
				"}\n"
				"{\n"
					"map $null\n"//$ripplemap
				"}\n"
				"{\n"
					"map $null\n"//$refractiondepth
				"}\n"
				"program altwater#REFLECT#FRESNEL=4\n"
			"}\n"
		);
	case 4:	//ripples
		return (
			"{\n"
				"surfaceparm nodlight\n"
				"surfaceparm nomarks\n"
				"{\n"
					"map $refraction\n"
				"}\n"
				"{\n"
					"map $null\n"//$reflection
				"}\n"
				"{\n"
					"map $ripplemap\n"
				"}\n"
				"{\n"
					"map $null\n"//$refractiondepth
				"}\n"
				"program altwater#RIPPLEMAP#FRESNEL=4\n"
			"}\n"
		);
	case 5:	//ripples+reflections
		return (
			"{\n"
				"surfaceparm nodlight\n"
				"surfaceparm nomarks\n"
				"{\n"
					"map $refraction\n"
				"}\n"
				"{\n"
					"map $reflection\n"
				"}\n"
				"{\n"
					"map $ripplemap\n"
				"}\n"
				"{\n"
					"map $null\n"//$refractiondepth
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
					"surfaceparm nodlight\n"
				"}\n"
			);
	}
	else if (Shader_FloatArgument(s, "#WARP"))//!strncmp(shortname, "warp/", 5) || !strncmp(shortname, "warp33/", 7) || !strncmp(shortname, "warp66/", 7))
	{
		Shader_DefaultScript(shortname, s, Shader_DefaultBSPWater(s, shortname));
	}
	else if (Shader_FloatArgument(s, "#ALPHA"))//   !strncmp(shortname, "trans/", 6))
	{
		Shader_DefaultScript(shortname, s,
				"{\n"
					"{\n"
						"map $diffuse\n"
						"alphagen const $#ALPHA\n"
						"blendfunc blend\n"
					"}\n"
				"}\n"
			);
	}
	else
		Shader_DefaultBSPLM(shortname, s, args);
}

void Shader_DefaultBSPQ1(const char *shortname, shader_t *s, const void *args)
{
	char *builtin = NULL;

	if (!strcmp(shortname, "mirror_portal"))
	{
		builtin =	"{\n"
						"portal\n"
					"}\n";
	}
	else if (r_mirroralpha.value < 1 && (!strcmp(shortname, "window02_1") || !strncmp(shortname, "mirror", 6)))
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

	if (!builtin && (*shortname == '*' || *shortname == '!'))
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
		/*FIXME: use defaultwall#ALPHA=0.666 or so*/
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
					"alphafunc ge128\n"
				"}\n"
//				"if $lightmap\n"
					"{\n"
						"map $lightmap\n"
						"if gl_overbright > 1\n"
						"blendfunc gl_dst_color gl_src_color\n"	//scale it up twice. will probably still get clamped, but what can you do
						"else\n"
						"blendfunc gl_dst_color gl_zero\n"
						"endif\n"
						"depthfunc equal\n"
					"}\n"
//				"endif\n"
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
	char *builtin = NULL;

	if (Shader_ParseShader("defaultvertexlit", s))
		return;

	if (!builtin)
	{
		builtin = (
			"{\n"
				"program defaultwall#VERTEXLIT\n"
				"{\n"
					"map $diffuse\n"
					"rgbgen vertex\n"
					"alphagen vertex\n"
				"}\n"
			"}\n"
		);
	}

	Shader_DefaultScript(shortname, s, builtin);
}
void Shader_DefaultBSPFlare(const char *shortname, shader_t *s, const void *args)
{
	shaderpass_t *pass;
	if (Shader_ParseShader("defaultflare", s))
		return;

	pass = &s->passes[0];
	pass->flags = SHADER_PASS_NOCOLORARRAY;
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
	s->flags = SHADER_FLARE;
	s->sort = SHADER_SORT_ADDITIVE;

	s->flags |= SHADER_NODRAW;
}
void Shader_DefaultSkin(const char *shortname, shader_t *s, const void *args)
{
	if (Shader_ParseShader("defaultskin", s))
		return;

	if (r_softwarebanding && qrenderer == QR_OPENGL && sh_config.progs_supported)
	{
		Shader_DefaultScript(shortname, s,
			"{\n"
				"program defaultskin#EIGHTBIT\n"
				"affine\n"
				"{\n"
					"map $colourmap\n"
				"}\n"
			"}\n"
			);
		return;
	}
	if (r_tessellation.ival && sh_config.progs_supported)
	{
		Shader_DefaultScript(shortname, s,
			"{\n"
				"program defaultskin#TESS\n"
			"}\n"
			);
		return;
	}

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
	if (sh_config.progs_supported && qrenderer != QR_DIRECT3D9)
	{
		//hexen2 needs premultiplied alpha to avoid looking ugly
		//but that results in problems where things are drawn with alpha not 0, so scale vertex colour by alpha in the fragment program
		Shader_DefaultScript(shortname, s,
			"{\n"
				"affine\n"
				"nomipmaps\n"
				"program default2d#PREMUL\n"
				"{\n"
				"map $diffuse\n"
				"blendfunc gl_one gl_one_minus_src_alpha\n"
				"}\n"
				"sort additive\n"
			"}\n"
			);
		TEXASSIGN(s->defaulttextures->base, R_LoadHiResTexture(s->name, genargs, IF_PREMULTIPLYALPHA|IF_UIPIC|IF_NOPICMIP|IF_NOMIPMAP|IF_CLAMP));
	}
	else
	{
		Shader_DefaultScript(shortname, s,
			"{\n"
				"affine\n"
				"nomipmaps\n"
				"{\n"
					"clampmap $diffuse\n"
					"rgbgen vertex\n"
					"alphagen vertex\n"
					"blendfunc gl_src_alpha gl_one_minus_src_alpha\n"
				"}\n"
				"sort additive\n"
			"}\n"
			);
		TEXASSIGN(s->defaulttextures->base, R_LoadHiResTexture(s->name, genargs, IF_UIPIC|IF_NOPICMIP|IF_NOMIPMAP|IF_CLAMP));
	}
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

	if (!s->defaulttextures)
	{
		s->defaulttextures = Z_Malloc(sizeof(*s->defaulttextures));
		s->numdefaulttextures = 0;
	}

// set defaults
	s->flags = SHADER_CULL_FRONT;

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
	if (!shader)
		return;
	if (shader->uses <= 0)
	{
		Con_Printf("Shader double free (%s %i)\n", shader->name, shader->usageflags);
		return;
	}
	if (--shader->uses == 0)
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
			if (!s->uses)
				break;
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
	{
		s = r_shaders[f] = Z_Malloc(sizeof(*s));
	}
	s->id = f;
	if (r_numshaders < f+1)
		r_numshaders = f+1;

	if (!s->defaulttextures)
		s->defaulttextures = Z_Malloc(sizeof(*s->defaulttextures));
	else
		memset(s->defaulttextures, 0, sizeof(*s->defaulttextures));
	s->numdefaulttextures = 0;
	Q_strncpyz(s->name, cleanname, sizeof(s->name));
	s->usageflags = usageflags;
	s->generator = defaultgen;
	s->width = 0;
	s->height = 0;
	s->uses = 1;
	if (genargs)
		s->genargs = strdup(genargs);
	else
		s->genargs = NULL;

	//now determine the 'short name'. ie: the shader that is loaded off disk (no args, no extension)
	argsstart = strchr(cleanname, '#');
	if (argsstart)
		*argsstart = 0;
	COM_StripExtension (cleanname, shortname, sizeof(shortname));

	if (ruleset_allow_shaders.ival && !(usageflags & SUR_FORCEFALLBACK))
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

#ifdef _DEBUG
char *Shader_Decompose(shader_t *s)
{
	static char decomposebuf[32768];
	char *o = decomposebuf;
	sprintf(o, "<---\n"); o+=strlen(o);

	switch (s->sort)
	{
	case SHADER_SORT_NONE:		sprintf(o, "sort %i (SHADER_SORT_NONE)\n", s->sort); break;
	case SHADER_SORT_RIPPLE:	sprintf(o, "sort %i (SHADER_SORT_RIPPLE)\n", s->sort); break;
	case SHADER_SORT_PRELIGHT:	sprintf(o, "sort %i (SHADER_SORT_PRELIGHT)\n", s->sort); break;
	case SHADER_SORT_PORTAL:	sprintf(o, "sort %i (SHADER_SORT_PORTAL)\n", s->sort); break;
	case SHADER_SORT_SKY:		sprintf(o, "sort %i (SHADER_SORT_SKY)\n", s->sort); break;
	case SHADER_SORT_OPAQUE:	sprintf(o, "sort %i (SHADER_SORT_OPAQUE)\n", s->sort); break;
	case SHADER_SORT_DECAL:		sprintf(o, "sort %i (SHADER_SORT_DECAL)\n", s->sort); break;
	case SHADER_SORT_SEETHROUGH:sprintf(o, "sort %i (SHADER_SORT_SEETHROUGH)\n", s->sort); break;
	case SHADER_SORT_BANNER:	sprintf(o, "sort %i (SHADER_SORT_BANNER)\n", s->sort); break;
	case SHADER_SORT_UNDERWATER:sprintf(o, "sort %i (SHADER_SORT_UNDERWATER)\n", s->sort); break;
	case SHADER_SORT_BLEND:		sprintf(o, "sort %i (SHADER_SORT_BLEND)\n", s->sort); break;
	case SHADER_SORT_ADDITIVE:	sprintf(o, "sort %i (SHADER_SORT_ADDITIVE)\n", s->sort); break;
	case SHADER_SORT_NEAREST:	sprintf(o, "sort %i (SHADER_SORT_NEAREST)\n", s->sort); break;
	default:					sprintf(o, "sort %i\n", s->sort); break;
	}
	o+=strlen(o);

	if (s->prog)
	{
		sprintf(o, "program\n");
		o+=strlen(o);
	}
	else
	{
		unsigned int i, j;
		shaderpass_t *p;
		for (i = 0; i < s->numpasses; i+= p->numMergedPasses)
		{
			p = &s->passes[i];
			sprintf(o, "{\n"); o+=strlen(o);

			switch(p->rgbgen)
			{
			case RGB_GEN_ENTITY: sprintf(o, "RGB_GEN_ENTITY "); break;
			case RGB_GEN_ONE_MINUS_ENTITY: sprintf(o, "RGB_GEN_ONE_MINUS_ENTITY "); break;
			case RGB_GEN_VERTEX_LIGHTING: sprintf(o, "RGB_GEN_VERTEX_LIGHTING "); break;
			case RGB_GEN_VERTEX_EXACT: sprintf(o, "RGB_GEN_VERTEX_EXACT "); break;
			case RGB_GEN_ONE_MINUS_VERTEX: sprintf(o, "RGB_GEN_ONE_MINUS_VERTEX "); break;
			case RGB_GEN_IDENTITY_LIGHTING: sprintf(o, "RGB_GEN_IDENTITY_LIGHTING "); break;
			case RGB_GEN_IDENTITY_OVERBRIGHT: sprintf(o, "RGB_GEN_IDENTITY_OVERBRIGHT "); break;
			default:
			case RGB_GEN_IDENTITY: sprintf(o, "RGB_GEN_IDENTITY "); break;
			case RGB_GEN_CONST: sprintf(o, "RGB_GEN_CONST "); break;
			case RGB_GEN_LIGHTING_DIFFUSE: sprintf(o, "RGB_GEN_LIGHTING_DIFFUSE "); break;
			case RGB_GEN_WAVE: sprintf(o, "RGB_GEN_WAVE "); break;
			case RGB_GEN_TOPCOLOR: sprintf(o, "RGB_GEN_TOPCOLOR "); break;
			case RGB_GEN_BOTTOMCOLOR: sprintf(o, "RGB_GEN_BOTTOMCOLOR "); break;
			}
			o+=strlen(o);
			sprintf(o, "\n"); o+=strlen(o);

			if (p->shaderbits & SBITS_MISC_DEPTHWRITE)		{	sprintf(o, "SBITS_MISC_DEPTHWRITE\n"); o+=strlen(o); }
			if (p->shaderbits & SBITS_MISC_NODEPTHTEST)		{	sprintf(o, "SBITS_MISC_NODEPTHTEST\n"); o+=strlen(o); }
			if (p->shaderbits & SBITS_MISC_DEPTHEQUALONLY)	{	sprintf(o, "SBITS_MISC_DEPTHEQUALONLY\n"); o+=strlen(o); }
			if (p->shaderbits & SBITS_MISC_DEPTHCLOSERONLY)	{	sprintf(o, "SBITS_MISC_DEPTHCLOSERONLY\n"); o+=strlen(o); }
			if (p->shaderbits & SBITS_TESSELLATION)			{	sprintf(o, "SBITS_TESSELLATION\n"); o+=strlen(o); }
			if (p->shaderbits & SBITS_AFFINE)				{	sprintf(o, "SBITS_AFFINE\n"); o+=strlen(o); }
			if (p->shaderbits & SBITS_MASK_BITS)			{	sprintf(o, "SBITS_MASK_BITS\n"); o+=strlen(o); }

			if (p->shaderbits & SBITS_BLEND_BITS)
			{
				sprintf(o, "blendfunc"); 
				o+=strlen(o);
				switch(p->shaderbits & SBITS_SRCBLEND_BITS)
				{
				case SBITS_SRCBLEND_NONE:							sprintf(o, " SBITS_SRCBLEND_NONE"); break;
				case SBITS_SRCBLEND_ZERO:							sprintf(o, " SBITS_SRCBLEND_ZERO"); break;
				case SBITS_SRCBLEND_ONE:							sprintf(o, " SBITS_SRCBLEND_ONE"); break;
				case SBITS_SRCBLEND_DST_COLOR:						sprintf(o, " SBITS_SRCBLEND_DST_COLOR"); break;
				case SBITS_SRCBLEND_ONE_MINUS_DST_COLOR:			sprintf(o, " SBITS_SRCBLEND_ONE_MINUS_DST_COLOR"); break;
				case SBITS_SRCBLEND_SRC_ALPHA:						sprintf(o, " SBITS_SRCBLEND_SRC_ALPHA"); break;
				case SBITS_SRCBLEND_ONE_MINUS_SRC_ALPHA:			sprintf(o, " SBITS_SRCBLEND_ONE_MINUS_SRC_ALPHA"); break;
				case SBITS_SRCBLEND_DST_ALPHA:						sprintf(o, " SBITS_SRCBLEND_DST_ALPHA"); break;
				case SBITS_SRCBLEND_ONE_MINUS_DST_ALPHA:			sprintf(o, " SBITS_SRCBLEND_ONE_MINUS_DST_ALPHA"); break;
				case SBITS_SRCBLEND_SRC_COLOR_INVALID:				sprintf(o, " SBITS_SRCBLEND_SRC_COLOR_INVALID"); break;
				case SBITS_SRCBLEND_ONE_MINUS_SRC_COLOR_INVALID:	sprintf(o, " SBITS_SRCBLEND_ONE_MINUS_SRC_COLOR_INVALID"); break;
				case SBITS_SRCBLEND_ALPHA_SATURATE:					sprintf(o, " SBITS_SRCBLEND_ALPHA_SATURATE"); break;
				default:											sprintf(o, " SBITS_SRCBLEND_INVALID"); break;
				}
				o+=strlen(o);
				switch(p->shaderbits & SBITS_DSTBLEND_BITS)
				{
				case SBITS_DSTBLEND_NONE:							sprintf(o, " SBITS_DSTBLEND_NONE"); break;
				case SBITS_DSTBLEND_ZERO:							sprintf(o, " SBITS_DSTBLEND_ZERO"); break;
				case SBITS_DSTBLEND_ONE:							sprintf(o, " SBITS_DSTBLEND_ONE"); break;
				case SBITS_DSTBLEND_DST_COLOR_INVALID:				sprintf(o, " SBITS_DSTBLEND_DST_COLOR_INVALID"); break;
				case SBITS_DSTBLEND_ONE_MINUS_DST_COLOR_INVALID:	sprintf(o, " SBITS_DSTBLEND_ONE_MINUS_DST_COLOR_INVALID"); break;
				case SBITS_DSTBLEND_SRC_ALPHA:						sprintf(o, " SBITS_DSTBLEND_SRC_ALPHA"); break;
				case SBITS_DSTBLEND_ONE_MINUS_SRC_ALPHA:			sprintf(o, " SBITS_DSTBLEND_ONE_MINUS_SRC_ALPHA"); break;
				case SBITS_DSTBLEND_DST_ALPHA:						sprintf(o, " SBITS_DSTBLEND_DST_ALPHA"); break;
				case SBITS_DSTBLEND_ONE_MINUS_DST_ALPHA:			sprintf(o, " SBITS_DSTBLEND_ONE_MINUS_DST_ALPHA"); break;
				case SBITS_DSTBLEND_SRC_COLOR:						sprintf(o, " SBITS_DSTBLEND_SRC_COLOR"); break;
				case SBITS_DSTBLEND_ONE_MINUS_SRC_COLOR:			sprintf(o, " SBITS_DSTBLEND_ONE_MINUS_SRC_COLOR"); break;
				case SBITS_DSTBLEND_ALPHA_SATURATE_INVALID:			sprintf(o, " SBITS_DSTBLEND_ALPHA_SATURATE_INVALID"); break;
				default:											sprintf(o, " SBITS_DSTBLEND_INVALID"); break;
				}
				o+=strlen(o);

				sprintf(o, "\n"); 
				o+=strlen(o);
			}


			switch(p->shaderbits & SBITS_ATEST_BITS)
			{
			case SBITS_ATEST_GE128: sprintf(o, "SBITS_ATEST_GE128\n"); break;
			case SBITS_ATEST_LT128: sprintf(o, "SBITS_ATEST_LT128\n"); break;
			case SBITS_ATEST_GT0: sprintf(o, "SBITS_ATEST_GT0\n"); break;
			}
			o+=strlen(o);

			for (j = 0; j < p->numMergedPasses; j++)
			{
				switch(p[j].blendmode)
				{
				default:
				case PBM_MODULATE: sprintf(o, "modulate "); break;
				case PBM_OVERBRIGHT: sprintf(o, "overbright "); break;
				case PBM_DECAL: sprintf(o, "decal "); break;
				case PBM_ADD:sprintf(o, "add "); break;
				case PBM_DOTPRODUCT: sprintf(o, "dotproduct "); break;
				case PBM_REPLACE: sprintf(o, "replace "); break;
				case PBM_REPLACELIGHT: sprintf(o, "replacelight "); break;
				case PBM_MODULATE_PREV_COLOUR: sprintf(o, "modulate_prev "); break;
				}
				o+=strlen(o);

				switch(p[j].texgen)
				{
				default:
				case T_GEN_SINGLEMAP: sprintf(o, "singlemap "); break;
				case T_GEN_ANIMMAP: sprintf(o, "animmap "); break;
				case T_GEN_LIGHTMAP: sprintf(o, "lightmap "); break;
				case T_GEN_DELUXMAP: sprintf(o, "deluxmap "); break;
				case T_GEN_SHADOWMAP: sprintf(o, "shadowmap "); break;
				case T_GEN_LIGHTCUBEMAP: sprintf(o, "lightcubemap "); break;
				case T_GEN_DIFFUSE: sprintf(o, "diffuse "); break;
				case T_GEN_NORMALMAP: sprintf(o, "normalmap "); break;
				case T_GEN_SPECULAR: sprintf(o, "specular "); break;
				case T_GEN_UPPEROVERLAY: sprintf(o, "upperoverlay "); break;
				case T_GEN_LOWEROVERLAY: sprintf(o, "loweroverlay "); break;
				case T_GEN_FULLBRIGHT: sprintf(o, "fullbright "); break;
				case T_GEN_PALETTED: sprintf(o, "paletted "); break;
				case T_GEN_REFLECTCUBE: sprintf(o, "reflectcube "); break;
				case T_GEN_REFLECTMASK: sprintf(o, "reflectmask "); break;
				case T_GEN_CURRENTRENDER: sprintf(o, "currentrender "); break;
				case T_GEN_SOURCECOLOUR: sprintf(o, "sourcecolour "); break;
				case T_GEN_SOURCEDEPTH: sprintf(o, "sourcedepth "); break;
				case T_GEN_REFLECTION: sprintf(o, "reflection "); break;
				case T_GEN_REFRACTION: sprintf(o, "refraction "); break;
				case T_GEN_REFRACTIONDEPTH: sprintf(o, "refractiondepth "); break;
				case T_GEN_RIPPLEMAP: sprintf(o, "ripplemap "); break;
				case T_GEN_SOURCECUBE: sprintf(o, "sourcecube "); break;
				case T_GEN_VIDEOMAP: sprintf(o, "videomap "); break;
				case T_GEN_CUBEMAP: sprintf(o, "cubemap "); break;
				case T_GEN_3DMAP: sprintf(o, "3dmap "); break;
				}
				o+=strlen(o);

				sprintf(o, "\n"); o+=strlen(o);
			}
			sprintf(o, "}\n"); o+=strlen(o);
		}
	}
	sprintf(o, "--->\n"); o+=strlen(o);
	return decomposebuf;
}
#endif

char *Shader_GetShaderBody(shader_t *s, char *fname, size_t fnamesize)
{
	char *adr, *parsename=NULL;
	char cleanname[MAX_QPATH];
	char shortname[MAX_QPATH];
	char drivername[MAX_QPATH];
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
			Q_snprintfz(drivername, sizeof(drivername), sh_config.shadernamefmt, cleanname);
			if (!parsename && Shader_ParseShader(drivername, s))
				parsename = drivername;
		}
		if (!parsename && Shader_ParseShader(cleanname, s))
			parsename = cleanname;
		if (!parsename && Shader_ParseShader(shortname, s))
			parsename = shortname;
	}
	if (!parsename && s->generator)
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

	if (fnamesize)
	{
		if (parsename)
		{
			unsigned int key;
			shadercache_t *cache;
			key = Hash_Key (parsename, HASH_SIZE);
			cache = shader_hash[key];
			for ( ; cache; cache = cache->hash_next)
			{
				if (!Q_stricmp (cache->name, parsename))
				{
					char *c, *stop;
					int line = 1;
					//okay, this is the shader we're looking for, we know where it came from too, so there's handy.
					//figure out the line index now, by just counting the \ns up to the offset
					for (c = cache->source->data, stop = c+cache->offset; c < stop; c++)
					{
						if (*c == '\n')
							line++;
					}
					Q_snprintfz(fname, fnamesize, "%s:%i", cache->source->name, line);
					break;
				}
			}
		}
		else
			*fname = 0;
	}

#ifdef _DEBUG
	{
		char *add, *ret;
		add = Shader_Decompose(s);
		if (*add)
		{
			ret = Z_Malloc(strlen(add) + strlen(adr) + 1);
			strcpy(ret, adr);
			strcpy(ret + strlen(ret), add);
			Z_Free(adr);
			adr = ret;
		}
	}
#endif
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
		char *body = Shader_GetShaderBody(o, NULL, 0);
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

void Shader_TouchTextures(void)
{
	int i, j, k;
	shader_t *s;
	shaderpass_t *p;
	texnums_t *t;
	for (i = 0; i < r_numshaders; i++)
	{
		s = r_shaders[i];
		if (!s || !s->uses)
			continue;

		for (j = 0; j < s->numpasses; j++)
		{
			p = &s->passes[j];
			for (k = 0; k < countof(p->anim_frames); k++)
				if (p->anim_frames[k])
					p->anim_frames[k]->regsequence = r_regsequence;
		}
		for (j = 0; j < max(1,s->numdefaulttextures); j++)
		{
			t = &s->defaulttextures[j];
			if (t->base)
				t->base->regsequence = r_regsequence;
			if (t->bump)
				t->bump->regsequence = r_regsequence;
			if (t->fullbright)
				t->fullbright->regsequence = r_regsequence;
			if (t->specular)
				t->specular->regsequence = r_regsequence;
			if (t->upperoverlay)
				t->upperoverlay->regsequence = r_regsequence;
			if (t->loweroverlay)
				t->loweroverlay->regsequence = r_regsequence;
		}
	}
}

void Shader_DoReload(void)
{
	shader_t *s;
	unsigned int i;
	char shortname[MAX_QPATH];
	char cleanname[MAX_QPATH];
	int oldsort;
	qboolean resort = false;

	//don't spam shader reloads while we're connecting, as that's just wasteful.
	if (cls.state && cls.state < ca_active)
		return;
	if (!r_shaders)
		return;	//err, not ready yet

	if (shader_rescan_needed)
	{
		Shader_FlushCache();

		if (ruleset_allow_shaders.ival)
		{
			COM_EnumerateFiles("materials/*.mtr", Shader_InitCallback, NULL);
			COM_EnumerateFiles("shaders/*.shader", Shader_InitCallback, NULL);
			COM_EnumerateFiles("scripts/*.shader", Shader_InitCallback, NULL);
			COM_EnumerateFiles("scripts/*.rscript", Shader_InitCallback, NULL);
		}

		shader_reload_needed = true;
		shader_rescan_needed = false;

		Con_DPrintf("Rescanning shaders\n");
	}
	else
	{
		if (!shader_reload_needed)
			return;
		Con_DPrintf("Reloading shaders\n");
	}
	shader_reload_needed = false;
	R2D_ImageColours(1,1,1,1);
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
#ifdef HAVE_MEDIA_DECODER
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

shader_t *R_ShaderFind(const char *name)
{
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
			return s;
	}
	return NULL;
}

cin_t *R_ShaderFindCinematic(const char *name)
{
#ifdef HAVE_MEDIA_DECODER
	return R_ShaderGetCinematic(R_ShaderFind(name));
#else
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
	if (!shader->width && !shader->height)
	{
		int i;
		if (width)
			*width = 0;
		if (height)
			*height = 0;
		if ((shader->flags & SHADER_HASDIFFUSE) && shader->defaulttextures->base)
		{
			if (shader->defaulttextures->base->status == TEX_LOADING)
			{
				if (!blocktillloaded)
					return -1;
				COM_WorkerPartialSync(shader->defaulttextures->base, &shader->defaulttextures->base->status, TEX_LOADING);
			}
			if (shader->defaulttextures->base->status == TEX_LOADED)
			{
				shader->width = shader->defaulttextures->base->width;
				shader->height = shader->defaulttextures->base->height;
			}
		}
		else if ((shader->flags & SHADER_HASPALETTED) && shader->defaulttextures->paletted)
		{
			if (shader->defaulttextures->paletted->status == TEX_LOADING)
			{
				if (!blocktillloaded)
					return -1;
				COM_WorkerPartialSync(shader->defaulttextures->paletted, &shader->defaulttextures->paletted->status, TEX_LOADING);
			}
			if (shader->defaulttextures->paletted->status == TEX_LOADED)
			{
				shader->width = shader->defaulttextures->paletted->width;
				shader->height = shader->defaulttextures->paletted->height;
			}
		}
		else
		{
			for (i = 0; i < shader->numpasses; i++)
			{
				if (shader->passes[i].texgen == T_GEN_SINGLEMAP && shader->passes[i].anim_frames[0] && shader->passes[i].anim_frames[0]->status == TEX_LOADING)
				{
					if (!blocktillloaded)
						return -1;
					COM_WorkerPartialSync(shader->passes[i].anim_frames[0], &shader->passes[i].anim_frames[0]->status, TEX_LOADING);
				}
				if (shader->passes[i].texgen == T_GEN_DIFFUSE && (shader->defaulttextures->base && shader->defaulttextures->base->status == TEX_LOADING))
				{
					if (!blocktillloaded)
						return -1;
					COM_WorkerPartialSync(shader->defaulttextures->base, &shader->defaulttextures->base->status, TEX_LOADING);
				}
				if (shader->passes[i].texgen == T_GEN_PALETTED && (shader->defaulttextures->paletted && shader->defaulttextures->paletted->status == TEX_LOADING))
				{
					if (!blocktillloaded)
						return -1;
					COM_WorkerPartialSync(shader->defaulttextures->paletted, &shader->defaulttextures->paletted->status, TEX_LOADING);
				}
			}

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
				if (shader->passes[i].texgen == T_GEN_DIFFUSE)
				{
					if (shader->defaulttextures->base && shader->defaulttextures->base->status == TEX_LOADED)
					{
						shader->width = shader->defaulttextures->base->width;
						shader->height = shader->defaulttextures->base->height;
					}
					break;
				}
				if (shader->passes[i].texgen == T_GEN_PALETTED)
				{
					if (shader->defaulttextures->paletted && shader->defaulttextures->paletted->status == TEX_LOADED)
					{
						shader->width = shader->defaulttextures->paletted->width;
						shader->height = shader->defaulttextures->paletted->height;
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
shader_t *R_RegisterPic (const char *name, const char *subdirs)
{
	shader_t *shader;
	shader = R_LoadShader (name, SUF_2D, Shader_Default2D, subdirs);
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
