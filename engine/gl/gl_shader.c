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
static qboolean shader_reload_needed;
static qboolean shader_rescan_needed;

//cvars that affect shader generation
cvar_t r_vertexlight = SCVAR("r_vertexlight", "0");
extern cvar_t r_deluxemapping;
extern cvar_t r_fastturb, r_fastsky, r_skyboxname;
extern cvar_t r_drawflat;

//backend fills this in to say the max pass count
int be_maxpasses;


#define Q_stricmp stricmp
#define Com_sprintf snprintf
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

#define MAX_TOKEN_CHARS 1024

char *COM_ParseExt (char **data_p, qboolean nl)
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
		if (c == ',' && len)
			break;
	} while (c>32);

	if (len == MAX_TOKEN_CHARS)
	{
//		Com_Printf ("Token exceeded %i chars, discarded.\n", MAX_TOKEN_CHARS);
		len = 0;
	}
	com_token[len] = 0;

	*data_p = data;
	return com_token;
}







#define HASH_SIZE	128

typedef struct shaderkey_s
{
    char			*keyword;
    void			(*func)( shader_t *shader, shaderpass_t *pass, char **ptr );
} shaderkey_t;

typedef struct shadercache_s {
	char name[MAX_QPATH];
	char *path;
	unsigned int offset;
	struct shadercache_s *hash_next;
} shadercache_t;

static shadercache_t **shader_hash;
static char shaderbuf[MAX_QPATH * 256];
int shaderbuflen;

shader_t	*r_shaders;
static hashtable_t shader_active_hash;
void *shader_active_hash_mem;

//static char		r_skyboxname[MAX_QPATH];
//static float	r_skyheight;

char *Shader_Skip( char *ptr );
static qboolean Shader_Parsetok( shader_t *shader, shaderpass_t *pass, shaderkey_t *keys,
		char *token, char **ptr );
static void Shader_ParseFunc( char **args, shaderfunc_t *func );
static void Shader_MakeCache( char *path );
static void Shader_GetPathAndOffset( char *name, char **path, unsigned int *offset );
static void Shader_ReadShader(shader_t *s, char *shadersource);

//===========================================================================

static qboolean Shader_EvaluateCondition(char **ptr)
{
	char *token;
	cvar_t *cv;
	qboolean conditiontrue = true;
	token = COM_ParseExt ( ptr, false );
	if (*token == '!')
	{
		conditiontrue = false;
		token++;
	}
	if (*token == '$')
	{
		extern cvar_t gl_bump;
		token++;
		if (!Q_stricmp(token, "lightmap"))
			conditiontrue = conditiontrue == !r_fullbright.value;
		else if (!Q_stricmp(token, "deluxmap") )
			conditiontrue = conditiontrue == (r_deluxemapping.value && gl_bump.value);

		//normalmaps are generated if they're not already known.
		else if (!Q_stricmp(token, "normalmap") )
			conditiontrue = conditiontrue == !!gl_bump.value;

		else if (!Q_stricmp(token, "gles") )
		{
#ifdef GLQUAKE
			conditiontrue = conditiontrue == ((qrenderer == QR_OPENGL) && !!gl_config.gles);
#else
			conditiontrue = conditiontrue == false;
#endif
		}
		else if (!Q_stricmp(token, "nofixed") )
		{
#ifdef GLQUAKE
			conditiontrue = conditiontrue == ((qrenderer == QR_OPENGL) && !!gl_config.nofixedfunc);
#else
			conditiontrue = conditiontrue == false;
#endif
		}
		else if (!Q_stricmp(token, "glsl") )
		{
#ifdef GLQUAKE
			conditiontrue = conditiontrue == ((qrenderer == QR_OPENGL) && gl_config.arb_shader_objects);
#else
			conditiontrue = conditiontrue == false;
#endif
		}
		else if (!Q_stricmp(token, "hlsl") )
		{
#ifdef D3DQUAKE
			conditiontrue = conditiontrue == false;//((qrenderer == QR_DIRECT3D) && gl_config.arb_shader_objects);
#else
			conditiontrue = conditiontrue == false;
#endif
		}


// GCC hates these within if statements "error: expected '}' before 'else'"
#ifdef _MSC_VER
#pragma message("shader fixme")
#endif
		else if (!Q_stricmp(token, "diffuse") )
			conditiontrue = conditiontrue == true;
		else if (!Q_stricmp(token, "specular") )
			conditiontrue = conditiontrue == false;
		else if (!Q_stricmp(token, "fullbright") )
			conditiontrue = conditiontrue == false;
		else if (!Q_stricmp(token, "topoverlay") )
			conditiontrue = conditiontrue == false;
		else if (!Q_stricmp(token, "loweroverlay") )
			conditiontrue = conditiontrue == false;
		else
		{
			Con_Printf("Unrecognised builtin shader condition '%s'\n", token);
			conditiontrue = conditiontrue == false;
		}
	}
	else
	{
		cv = Cvar_Get(token, "", 0, "Shader Conditions");
		token = COM_ParseExt ( ptr, false );
		if (*token)
		{
			float rhs;
			char cmp[4];
			memcpy(cmp, token, 4);
			token = COM_ParseExt ( ptr, false );
			rhs = atof(token);
			if (!strcmp(cmp, "!="))
				conditiontrue = cv->value != rhs;
			else if (!strcmp(cmp, "=="))
				conditiontrue = cv->value == rhs;
			else if (!strcmp(cmp, "<"))
				conditiontrue = cv->value < rhs;
			else if (!strcmp(cmp, "<="))
				conditiontrue = cv->value <= rhs;
			else if (!strcmp(cmp, ">"))
				conditiontrue = cv->value > rhs;
			else if (!strcmp(cmp, ">="))
				conditiontrue = cv->value >= rhs;
			else
				conditiontrue = false;
		}
		else
		{
			if (cv)
				conditiontrue = conditiontrue == !!cv->value;
		}
	}
	token = COM_ParseExt ( ptr, false );
	if (!strcmp(token, "&&"))
		return Shader_EvaluateCondition(ptr) && conditiontrue;
	if (!strcmp(token, "||"))
		return Shader_EvaluateCondition(ptr) || conditiontrue;

	return conditiontrue;
}

static char *Shader_ParseString ( char **ptr )
{
	char *token;

	if ( !ptr || !(*ptr) ) {
		return "";
	}
	if ( !**ptr || **ptr == '}' ) {
		return "";
	}

	token = COM_ParseExt ( ptr, false );
	Q_strlwr ( token );

	return token;
}

static char *Shader_ParseSensString ( char **ptr )
{
	char *token;

	if ( !ptr || !(*ptr) ) {
		return "";
	}
	if ( !**ptr || **ptr == '}' ) {
		return "";
	}

	token = COM_ParseExt ( ptr, false );

	return token;
}

static float Shader_ParseFloat(char **ptr)
{
	char *token;
	if (!ptr || !(*ptr))
	{
		return 0;
	}
	if (!**ptr || **ptr == '}')
	{
		return 0;
	}

	token = COM_ParseExt(ptr, false);
	if (*token == '$')
	{
		cvar_t *var;
		var = Cvar_FindVar(token+1);
		if (var)
			return var->value;
	}
	return atof(token);
}

static void Shader_ParseVector ( char **ptr, vec3_t v )
{
	char *scratch;
	char *token;
	qboolean bracket;

	token = Shader_ParseString ( ptr );
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

		token = Shader_ParseString ( ptr );
	}
	if ( !Q_stricmp (token, "(") ) {
		bracket = true;
		token = Shader_ParseString ( ptr );
	} else if ( token[0] == '(' ) {
		bracket = true;
		token = &token[1];
	} else {
		bracket = false;
	}

	v[0] = atof ( token );
	v[1] = Shader_ParseFloat ( ptr );

	token = Shader_ParseString ( ptr );
	if ( !token[0] ) {
		v[2] = 0;
	} else if ( token[strlen(token)-1] == ')' ) {
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
					Com_sprintf ( path, sizeof(path), skyname_pattern[sp], texturename, skyname_suffix[ss][i] );
					images[i] = R_LoadHiResTexture ( path, NULL, IF_NOALPHA);
					if (TEXVALID(images[i]))
						break;
				}
				if (TEXVALID(images[i]))
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

static void Shader_ParseFunc ( char **ptr, shaderfunc_t *func )
{
	char *token;

	token = Shader_ParseString ( ptr );
	if ( !Q_stricmp (token, "sin") ) {
	    func->type = SHADER_FUNC_SIN;
	} else if ( !Q_stricmp (token, "triangle") ) {
	    func->type = SHADER_FUNC_TRIANGLE;
	} else if ( !Q_stricmp (token, "square") ) {
	    func->type = SHADER_FUNC_SQUARE;
	} else if ( !Q_stricmp (token, "sawtooth") ) {
	    func->type = SHADER_FUNC_SAWTOOTH;
	} else if (!Q_stricmp (token, "inversesawtooth") ) {
	    func->type = SHADER_FUNC_INVERSESAWTOOTH;
	} else if (!Q_stricmp (token, "noise") ) {
	    func->type = SHADER_FUNC_NOISE;
	}

	func->args[0] = Shader_ParseFloat ( ptr );
	func->args[1] = Shader_ParseFloat ( ptr );
	func->args[2] = Shader_ParseFloat ( ptr );
	func->args[3] = Shader_ParseFloat ( ptr );
}

//===========================================================================

static int Shader_SetImageFlags ( shader_t *shader )
{
	int flags = 0;

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
	if (!Q_stricmp (name, "$whiteimage"))
		return r_nulltex;
	else
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

	if ( shader->numdeforms >= SHADER_DEFORM_MAX ) {
		return;
	}

	deformv = &shader->deforms[shader->numdeforms];

	token = Shader_ParseString ( ptr );
	if ( !Q_stricmp (token, "wave") ) {
		deformv->type = DEFORMV_WAVE;
		deformv->args[0] = Shader_ParseFloat ( ptr );
		if ( deformv->args[0] ) {
			deformv->args[0] = 1.0f / deformv->args[0];
		}

		Shader_ParseFunc ( ptr, &deformv->func );
	} else if ( !Q_stricmp (token, "normal") ) {
		deformv->type = DEFORMV_NORMAL;
		deformv->args[0] = Shader_ParseFloat ( ptr );
		deformv->args[1] = Shader_ParseFloat ( ptr );
	} else if ( !Q_stricmp (token, "bulge") ) {
		deformv->type = DEFORMV_BULGE;

		Shader_ParseVector ( ptr, deformv->args );
		shader->flags |= SHADER_DEFORMV_BULGE;
	} else if ( !Q_stricmp (token, "move") ) {
		deformv->type = DEFORMV_MOVE;

		Shader_ParseVector ( ptr, deformv->args );
		Shader_ParseFunc ( ptr, &deformv->func );
	} else if ( !Q_stricmp (token, "autosprite") ) {
		deformv->type = DEFORMV_AUTOSPRITE;
		shader->flags |= SHADER_AUTOSPRITE;
	} else if ( !Q_stricmp (token, "autosprite2") ) {
		deformv->type = DEFORMV_AUTOSPRITE2;
		shader->flags |= SHADER_AUTOSPRITE;
	} else if ( !Q_stricmp (token, "projectionShadow") ) {
		deformv->type = DEFORMV_PROJECTION_SHADOW;
	} else {
		return;
	}

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

	skyheight = Shader_ParseFloat(ptr);
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

	Shader_ParseVector ( ptr, color );
	VectorScale ( color, div, color );
	ColorNormalize ( color, fcolor );

	shader->fog_color[0] = FloatToByte ( fcolor[0] );
	shader->fog_color[1] = FloatToByte ( fcolor[1] );
	shader->fog_color[2] = FloatToByte ( fcolor[2] );
	shader->fog_color[3] = 255;
	shader->fog_dist = Shader_ParseFloat ( ptr );

	if ( shader->fog_dist <= 0.0f ) {
		shader->fog_dist = 128.0f;
	}
	shader->fog_dist = 1.0f / shader->fog_dist;
}

static void Shader_SurfaceParm ( shader_t *shader, shaderpass_t *pass, char **ptr )
{
	char *token;

	token = Shader_ParseString ( ptr );
	if ( !Q_stricmp( token, "nodraw" ) )
		shader->flags |= SHADER_NODRAW;
	else if ( !Q_stricmp( token, "nodlight" ) )
		shader->flags |= SHADER_NODLIGHT;
}

static void Shader_Sort ( shader_t *shader, shaderpass_t *pass, char **ptr )
{
	char *token;


	token = Shader_ParseString ( ptr );
	if ( !Q_stricmp( token, "portal" ) ) {
		shader->sort = SHADER_SORT_PORTAL;
	} else if( !Q_stricmp( token, "sky" ) ) {
		shader->sort = SHADER_SORT_SKY;
	} else if( !Q_stricmp( token, "opaque" ) ) {
		shader->sort = SHADER_SORT_OPAQUE;
	} else if( !Q_stricmp( token, "decal" ) ) {
		shader->sort = SHADER_SORT_DECAL;
	} else if( !Q_stricmp( token, "seethrough" ) ) {
		shader->sort = SHADER_SORT_SEETHROUGH;
	} else if( !Q_stricmp( token, "banner" ) ) {
		shader->sort = SHADER_SORT_BANNER;
	} else if( !Q_stricmp( token, "additive" ) ) {
		shader->sort = SHADER_SORT_ADDITIVE;
	} else if( !Q_stricmp( token, "underwater" ) ) {
		shader->sort = SHADER_SORT_UNDERWATER;
	} else if( !Q_stricmp( token, "nearest" ) ) {
		shader->sort = SHADER_SORT_NEAREST;
	} else if( !Q_stricmp( token, "blend" ) ) {
		shader->sort = SHADER_SORT_BLEND;
	} else {
		shader->sort = atoi ( token );
		clamp ( shader->sort, SHADER_SORT_NONE, SHADER_SORT_NEAREST );
	}
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

static void Shader_ProgAutoFields(program_t *prog, char **cvarfnames);
/*program text is already loaded, this function parses the 'header' of it to see which permutations it provides, and how many times we need to recompile it*/
static void Shader_LoadPermutations(program_t *prog, char *script, int qrtype, int ver)
{
	static char *permutationname[] =
	{
		"#define BUMP\n",
		"#define SPECULAR\n",
		"#define FULLBRIGHT\n",
		"#define LOWER\n",
		"#define UPPER\n",
		"#define OFFSETMAPPING\n",
		NULL
	};
	char *permutationdefines[sizeof(permutationname)/sizeof(permutationname[0])];
	unsigned int nopermutation = ~0u;
	int p, n, pn;
	char *end;
	char *vers;

	char *cvarfnames[64];
	int cvarfcount = 0;

	cvarfnames[cvarfcount] = NULL;

	for(;;)
	{
		while (*script == ' ' || *script == '\r' || *script == '\n' || *script == '\t')
			script++;
		if (!strncmp(script, "!!cvarf", 7))
		{
			script += 7;
			while (*script == ' ' || *script == '\t')
				script++;
			end = script;
			while ((*end >= 'A' && *end <= 'Z') || (*end >= 'a' && *end <= 'z') || (*end >= '0' && *end <= '9') || *end == '_')
				end++;
			if (cvarfcount+1 != sizeof(cvarfnames)/sizeof(cvarfnames[0]))
				cvarfnames[cvarfcount++] = script;
			cvarfnames[cvarfcount] = NULL;
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
					nopermutation &= ~(1u<<p);
			}
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

	if (ver)
		vers = va("#version %u\n", ver);
	else
		vers = NULL;
	memset(prog->handle, 0, sizeof(*prog->handle)*PERMUTATIONS);
	for (p = 0; p < PERMUTATIONS; p++)
	{
		if (qrenderer != qrtype)
		{
		}
#ifdef GLQUAKE
		else if (qrenderer == QR_OPENGL)
		{
			if (nopermutation & p)
			{
				continue;
			}
			pn = 0;
			for (n = 0; permutationname[n]; n++)
			{
				if (p & (1u<<n))
					permutationdefines[pn++] = permutationname[n];
			}
			permutationdefines[pn++] = NULL;
			prog->handle[p].glsl = GLSlang_CreateProgram(vers, permutationdefines, script, script);
		}
#endif
#ifdef D3DQUAKE
		else if (qrenderer == QR_DIRECT3D)
		{
			if (nopermutation & p)
			{
				continue;
			}
			pn = 0;
			for (n = 0; permutationname[n]; n++)
			{
				if (p & (1u<<n))
					permutationdefines[pn++] = permutationname[n];
			}
			permutationdefines[pn++] = NULL;
			prog->handle[p] = D3DShader_CreateProgram(permutationdefines, script, script);
		}
#endif
	}

	Shader_ProgAutoFields(prog, cvarfnames);
}
typedef struct sgeneric_s
{
	struct sgeneric_s *next;
	char name[MAX_QPATH];
	qboolean failed;
	program_t prog;
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
#ifdef GLQUAKE
	/*a quick note on glsl versions:
	  gl versioning started with 110
	  gles versioning started at 100 and only had a single one defined
	  with gl3's combined support, gl3 supports 130+ and 100, but 110 requries compat extension
	  with gl4, versions are meant to match the gl version more closely, so gl4.0 uses 400.*/
	/*glsl es shaders require precisions to be defined for fragment shader variables
	  more precision for shaders would be a good candidate for a cvar */
	{QR_OPENGL/*ES*/, 100, "default2d",
		"#ifdef VERTEX_SHADER\n"
			"uniform mat4 m_view;\n"
			"uniform mat4 m_projection;\n"
			"attribute vec3 v_position;\n"
			"attribute vec2 v_texcoord;\n"
			"attribute vec4 v_colour;\n"
			"varying vec2 tc;\n"
			"varying vec4 vc;\n"

			"void main (void)\n"
			"{\n"
			"	tc = v_texcoord;\n"
			"	vc = v_colour;\n"
			"	gl_Position = m_projection * m_view * vec4(v_position, 1.0);\n"
			"}\n"
		"#endif\n"

		"#ifdef FRAGMENT_SHADER\n"
			"uniform sampler2D s_t0;\n"
			"varying mediump vec2 tc;\n"
			"varying lowp vec4 vc;\n"

			"void main (void)\n"
			"{\n"
			"	gl_FragColor = texture2D(s_t0, tc) * vc;\n"
			"}\n"
		"#endif\n"
	},
	{QR_OPENGL, 110, "default2d",
		"#ifdef VERTEX_SHADER\n"
			"uniform mat4 m_view;\n"
			"uniform mat4 m_projection;\n"
			"attribute vec3 v_position;\n"
			"attribute vec2 v_texcoord;\n"
			"attribute vec4 v_colour;\n"
			"varying vec2 tc;\n"
			"varying vec4 vc;\n"

			"void main (void)\n"
			"{\n"
			"	tc = v_texcoord;\n"
			"	vc = v_colour;\n"
			"	gl_Position = m_projection * m_view * vec4(v_position, 1.0);\n"
			"}\n"
		"#endif\n"

		"#ifdef FRAGMENT_SHADER\n"
			"uniform sampler2D s_t0;\n"
			"in vec2 tc;\n"
			"varying vec4 vc;\n"

			"void main (void)\n"
			"{\n"
			"	gl_FragColor = texture2D(s_t0, tc) * vc;\n"
			"}\n"
		"#endif\n"
	},
	{QR_OPENGL/*ES*/, 100, "defaultwall",
		"!!cvarf gl_overbright\n"
		"#ifdef VERTEX_SHADER\n"
			"uniform mat4 m_modelview;\n"
			"uniform mat4 m_projection;\n"
			"attribute vec3 v_position;\n"
			"attribute vec2 v_texcoord;\n"
			"attribute vec2 v_lmcoord;\n"
			"varying vec2 tc, lm;\n"

			"void main (void)\n"
			"{\n"
			"	tc = v_texcoord;\n"
			"	lm = v_lmcoord;\n"
			"	gl_Position = m_projection * m_modelview * vec4(v_position, 1.0);\n"
			"}\n"
		"#endif\n"

		"#ifdef FRAGMENT_SHADER\n"
			"uniform sampler2D s_t0;\n" /*tex_diffuse*/
			"uniform sampler2D s_t1;\n" /*tex_lightmap*/
			//"uniform sampler2D s_t2;\n" /*tex_normalmap*/
			//"uniform sampler2D s_t3;\n" /*tex_deluxmap*/
			//"uniform sampler2D s_t4;\n" /*tex_fullbright*/
			"varying mediump vec2 tc, lm;\n"
			"uniform mediump float cvar_gl_overbright;\n"

			"void main (void)\n"
			"{\n"
			"	mediump float scale = exp2(floor(clamp(cvar_gl_overbright, 0.0, 2.0)));\n"
			"	gl_FragColor = texture2D(s_t0, tc) * texture2D(s_t1, lm) * vec4(scale, scale, scale, 1);\n"
			"}\n"
		"#endif\n"
	},
	{QR_OPENGL, 110, "defaultwall",
		"!!cvarf gl_overbright\n"
		"#ifdef VERTEX_SHADER\n"
			"uniform mat4 m_modelview, m_projection;\n"
			"attribute vec3 v_position;\n"
			"attribute vec2 v_texcoord;\n"
			"attribute vec2 v_lmcoord;\n"
			"varying vec2 tc, lm;\n"

			"void main (void)\n"
			"{\n"
			"	tc = v_texcoord;\n"
			"	lm = v_lmcoord;\n"
			"	gl_Position = m_projection * m_modelview * vec4(v_position, 1.0);\n"
			"}\n"
		"#endif\n"

		"#ifdef FRAGMENT_SHADER\n"
			"uniform sampler2D s_t0;\n" /*tex_diffuse*/
			"uniform sampler2D s_t1;\n" /*tex_lightmap*/
			//"uniform sampler2D s_t2;\n" /*tex_normalmap*/
			//"uniform sampler2D s_t3;\n" /*tex_deluxmap*/
			//"uniform sampler2D s_t4;\n" /*tex_fullbright*/
			"varying vec2 tc, lm;\n"
			"uniform float cvar_gl_overbright;\n"

			"void main (void)\n"
			"{\n"
			"	float scale = exp2(floor(clamp(cvar_gl_overbright, 0.0, 2.0)));\n"
			"	gl_FragColor = texture2D(s_t0, tc) * texture2D(s_t1, lm) * vec4(scale, scale, scale, 1);\n"
			"}\n"
		"#endif\n"
	},
	{QR_OPENGL/*ES*/, 100, "defaultwarp",
		"!!cvarf r_wateralpha\n"
		"varying mediump vec2 tc;\n"
		"#ifdef VERTEX_SHADER\n"
			"uniform mat4 m_modelview;\n"
			"uniform mat4 m_projection;\n"
			"attribute vec3 v_position;\n"
			"attribute vec2 v_texcoord;\n"
			"void main (void)\n"
			"{\n"
			"	tc = v_texcoord;\n"
			"	gl_Position = m_projection * m_modelview * vec4(v_position, 1.0);\n"
			"}\n"
		"#endif\n"

		"#ifdef FRAGMENT_SHADER\n"
			"uniform sampler2D watertexture;\n"
			"uniform mediump float e_time;\n"
			"uniform lowp float cvar_r_wateralpha;\n"

			"void main (void)\n"
			"{\n"
			"	mediump vec2 ntc;\n"
			"	ntc.s = tc.s + sin(tc.t+e_time)*0.125;\n"
			"	ntc.t = tc.t + sin(tc.s+e_time)*0.125;\n"
			"	lowp vec3 ts = vec3(texture2D(watertexture, ntc));\n"

			"	gl_FragColor = vec4(ts, cvar_r_wateralpha);\n"
			"}\n"
		"#endif\n"
	},
	{QR_OPENGL, 110, "defaultwarp",
		"!!cvarf r_wateralpha\n"
		"varying vec2 tc;\n"
		"#ifdef VERTEX_SHADER\n"
			"uniform mat4 m_modelview;\n"
			"uniform mat4 m_projection;\n"
			"attribute vec3 v_position;\n"
			"attribute vec2 v_texcoord;\n"
			"void main (void)\n"
			"{\n"
			"	tc = v_texcoord.st;\n"
			"	gl_Position = m_projection * m_modelview * vec4(v_position, 1.0);\n"
			"}\n"
		"#endif\n"

		"#ifdef FRAGMENT_SHADER\n"
			"uniform sampler2D s_t0;\n"
			"uniform float e_time;\n"
			"uniform float cvar_r_wateralpha;\n"

			"void main (void)\n"
			"{\n"
			"	vec2 ntc;\n"
			"	ntc.s = tc.s + sin(tc.t+e_time)*0.125;\n"
			"	ntc.t = tc.t + sin(tc.s+e_time)*0.125;\n"
			"	vec3 ts = vec3(texture2D(s_t0, ntc));\n"

			"	gl_FragColor = vec4(ts, cvar_r_wateralpha);\n"
			"}\n"
		"#endif\n"
	},
	{QR_OPENGL/*ES*/, 100, "defaultsky",
		"#ifdef VERTEX_SHADER\n"
			"uniform mat4 m_modelview;\n"
			"uniform mat4 m_projection;\n"
			"attribute vec3 v_position;\n"
			"varying vec3 pos;\n"

			"void main (void)\n"
			"{\n"
			"	pos = v_position.xyz;\n"
			"	gl_Position = m_projection * m_modelview * vec4(v_position, 1.0);\n"
			"}\n"
		"#endif\n"

		"#ifdef FRAGMENT_SHADER\n"
			"uniform sampler2D s_t0;\n"
			"uniform sampler2D s_t1;\n"

			"uniform mediump float e_time;\n"
			"uniform mediump vec3 e_eyepos;\n"
			"varying mediump vec3 pos;\n"

			"void main (void)\n"
			"{\n"
			"	mediump vec2 tccoord;\n"

			"	mediump vec3 dir = pos - e_eyepos;\n"

			"	dir.z *= 3.0;\n"
			"	dir.xy /= 0.5*length(dir);\n"

			"	tccoord = (dir.xy + e_time*0.03125);\n"
			"	lowp vec3 solid = vec3(texture2D(s_t0, tccoord));\n"

			"	tccoord = (dir.xy + e_time*0.0625);\n"
			"	lowp vec4 clouds = texture2D(s_t1, tccoord);\n"

			"	gl_FragColor.rgb = (solid.rgb*(1.0-clouds.a)) + (clouds.a*clouds.rgb);\n"
//			"	gl_FragColor.rgb = solid.rgb;/*gl_FragColor.g = clouds.r;*/gl_FragColor.b = clouds.a;\n"
			"}\n"
		"#endif\n"
	},
	{QR_OPENGL, 110, "defaultsky",
		"#ifdef VERTEX_SHADER\n"
			"uniform mat4 m_modelview;\n"
			"uniform mat4 m_projection;\n"
			"attribute vec3 v_position;\n"
			"varying vec3 pos;\n"

			"void main (void)\n"
			"{\n"
			"	pos = v_position.xyz;\n"
			"	gl_Position = m_projection * m_modelview * vec4(v_position, 1.0);\n"
			"}\n"
		"#endif\n"

		"#ifdef FRAGMENT_SHADER\n"
			"uniform float e_time;\n"
			"uniform vec3 e_eyepos;\n"
			"varying vec3 pos;\n"
			"uniform sampler2D s_t0;\n"
			"uniform sampler2D s_t1;\n"

			"void main (void)\n"
			"{\n"
			"	vec2 tccoord;\n"

			"	vec3 dir = pos - e_eyepos;\n"

			"	dir.z *= 3.0;\n"
			"	dir.xy /= 0.5*length(dir);\n"

			"	tccoord = (dir.xy + e_time*0.03125);\n"
			"	vec3 solid = vec3(texture2D(s_t0, tccoord));\n"

			"	tccoord = (dir.xy + e_time*0.0625);\n"
			"	vec4 clouds = texture2D(s_t1, tccoord);\n"

			"	gl_FragColor.rgb = (solid.rgb*(1.0-clouds.a)) + (clouds.a*clouds.rgb);\n"
//			"	gl_FragColor.rgb = solid.rgb;/*gl_FragColor.g = clouds.r;*/gl_FragColor.b = clouds.a;\n"
			"}\n"
		"#endif\n"
	},
	{QR_OPENGL/*ES*/, 100, "defaultskin",
		"!!permu FULLBRIGHT\n"
		"!!permu LOWER\n"
		"!!permu UPPER\n"
		"#ifdef VERTEX_SHADER\n"
			"uniform mat4 m_modelview;\n"
			"uniform mat4 m_projection;\n"
			"attribute vec3 v_position;\n"
			"attribute vec2 v_texcoord;\n"
			"varying vec2 tc;\n"

			"attribute vec3 v_normal;\n"
			"uniform vec3 e_light_dir;\n"
			"uniform vec3 e_light_mul;\n"
			"uniform vec3 e_light_ambient;\n"
			"varying vec3 light;\n"

			"void main (void)\n"
			"{\n"
			"	light = e_light_ambient + (dot(v_normal,e_light_dir)*e_light_mul);\n"
			"	tc = v_texcoord;\n"
			"	gl_Position = m_projection * m_modelview * vec4(v_position, 1.0);\n"
			"}\n"
		"#endif\n"

		"#ifdef FRAGMENT_SHADER\n"
			"uniform sampler2D s_t0;\n" /*tex_diffuse*/
			"#ifdef LOWER\n"
			"uniform sampler2D s_t1;\n" /*tex_lower*/
			"uniform lowp vec3 e_lowercolour;\n"
			"#endif\n"
			"#ifdef UPPER\n"
			"uniform sampler2D s_t2;\n" /*tex_upper*/
			"uniform lowp vec3 e_uppercolour;\n"
			"#endif\n"
			"#ifdef FULLBRIGHT\n"
			"uniform sampler2D s_t3;\n" /*tex_fullbright*/
			"#endif\n"
			"varying mediump vec2 tc;\n"
			"varying lowp vec3 light;\n"
			"uniform lowp vec4 e_colourident;\n"

			"void main (void)\n"
			"{\n"
			"	lowp vec4 col;\n"
			"	col = texture2D(s_t0, tc);\n"
			"#ifdef UPPER\n"
			"	lowp vec4 uc = texture2D(s_t2, tc);\n"
			"	col.rgb = mix(col.rgb, uc.rgb*e_uppercolour, uc.a);\n"
			"#endif\n"
			"#ifdef LOWER\n"
			"	lowp vec4 lc = texture2D(s_t1, tc);\n"
			"	col.rgb = mix(col.rgb, lc.rgb*e_lowercolour, lc.a);\n"
			"#endif\n"
			"	col.rgb *= light;\n"
			"#ifdef FULLBRIGHT\n"
			"	lowp vec4 fb = texture2D(s_t3, tc);\n"
			"	col.rgb = mix(col.rgb, fb.rgb, fb.a);\n"
			"#endif\n"
			"	gl_FragColor = col * e_colourident;\n"
			"}\n"
		"#endif\n"
	},
	{QR_OPENGL, 110, "defaultskin",
		"!!permu FULLBRIGHT\n"
		"!!permu LOWER\n"
		"!!permu UPPER\n"
		"#ifdef VERTEX_SHADER\n"
			"uniform mat4 m_modelview;\n"
			"uniform mat4 m_projection;\n"
			"attribute vec3 v_position;\n"
			"attribute vec2 v_texcoord;\n"
			"varying vec2 tc;\n"

			"attribute vec3 v_normal;\n"
			"uniform vec3 e_light_dir;\n"
			"uniform vec3 e_light_mul;\n"
			"uniform vec3 e_light_ambient;\n"
			"varying vec3 light;\n"

			"void main (void)\n"
			"{\n"
			"	light = e_light_ambient + (dot(v_normal,e_light_dir)*e_light_mul);\n"
			"	tc = v_texcoord;\n"
			"	gl_Position = m_projection * m_modelview * vec4(v_position, 1.0);\n"
			"}\n"
		"#endif\n"

		"#ifdef FRAGMENT_SHADER\n"
			"uniform sampler2D s_t0;\n" /*tex_diffuse*/
			"#ifdef LOWER\n"
			"uniform sampler2D s_t1;\n" /*tex_lower*/
			"uniform vec3 e_lowercolour;\n"
			"#endif\n"
			"#ifdef UPPER\n"
			"uniform sampler2D s_t2;\n" /*tex_upper*/
			"uniform vec3 e_uppercolour;\n"
			"#endif\n"
			"#ifdef FULLBRIGHT\n"
			"uniform sampler2D s_t3;\n" /*tex_fullbright*/
			"#endif\n"
			"varying vec2 tc;\n"
			"varying vec3 light;\n"
			"uniform vec4 e_colourident;\n"

			"void main (void)\n"
			"{\n"
			"	vec4 col, sp;\n"
			"	col = texture2D(s_t0, tc);\n"
			"#ifdef UPPER\n"
			"	vec4 uc = texture2D(s_t2, tc);\n"
			"	col.rgb = mix(col.rgb, uc.rgb*e_uppercolour, uc.a);\n"
			"#endif\n"
			"#ifdef LOWER\n"
			"	vec4 lc = texture2D(s_t1, tc);\n"
			"	col.rgb = mix(col.rgb, lc.rgb*e_lowercolour, lc.a);\n"
			"#endif\n"
			"	col.rgb *= light;\n"
			"#ifdef FULLBRIGHT\n"
			"	vec4 fb = texture2D(s_t3, tc);\n"
			"	col.rgb = mix(col.rgb, fb.rgb, fb.a);\n"
			"#endif\n"
			"	gl_FragColor = col * e_colourident;\n"
			"}\n"
		"#endif\n"
	},
#endif
#if 0//def D3DQUAKE
		{QR_DIRECT3D, 9, "defaultsky",

			"struct a2v {\n"
				"float4 pos: POSITION;\n"
			"};\n"
			"struct v2f {\n"
		"#ifdef VERTEX_SHADER\n"
				"float4 pos: POSITION;\n"
		"#endif\n"
				"float3 vpos: COLOR;\n"
			"};\n"

		"#ifdef VERTEX_SHADER\n"
			"float4x4  ModelViewProj;\n"
			"v2f main (a2v inp)\n"
			"{\n"
			"	v2f outp;\n"
			"	outp.pos = mul(inp.pos, ModelViewProj);\n"
			"	outp.vpos = inp.pos;\n"
			"	return outp;\n"
			"}\n"
		"#endif\n"

		"#ifdef FRAGMENT_SHADER\n"
			"float e_time;\n"
			"float3 e_eyepos;\n"
			"sampler2D s_t0;\n"
			"sampler2D s_t1;\n"
			"float4 main (v2f inp) : COLOR0\n"
			"{\n"
			"	float2 tccoord;\n"

			"	float3 dir = inp.vpos - e_eyepos;\n"

			"	dir.z *= 3.0;\n"
			"	dir.xy /= 0.5*length(dir);\n"

			"	tccoord = (dir.xy + e_time*0.03125);\n"
			"	float4 solid = tex2D(s_t0, tccoord);\n"

			"	tccoord = (dir.xy + e_time*0.0625);\n"
			"	float4 clouds = tex2D(s_t1, tccoord);\n"

			"	return float4((solid.rgb*(1.0-clouds.a)) + (clouds.a*clouds.rgb), 1);\n"
//			"	return solid.rgb;/*gl_FragColor.g = clouds.r;*/gl_FragColor.b = clouds.a;\n"
			"}\n"
		"#endif\n"
	},
#endif
	{QR_NONE}
};
static sgeneric_t *sgenerics;
static void Shader_FlushGenerics(void)
{
	sgeneric_t *g;
	while (sgenerics)
	{
		g = sgenerics;
		sgenerics = g->next;
		free(g);
	}
}
static program_t *Shader_LoadGeneric(char *name, int qrtype)
{
	unsigned int i;
	void *file;

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

	if (strlen(name) >= sizeof(g->name))
		return NULL; /*name overflow*/
	g = malloc(sizeof(*g));
	memset(g, 0, sizeof(*g));
	strcpy(g->name, name);
	g->next = sgenerics;
	sgenerics = g;

	g->prog.refs = 1;

	FS_LoadFile(name, &file);
	if (file)
	{
		Shader_LoadPermutations(&g->prog, file, qrtype, 0);
		FS_FreeFile(file);

		g->prog.refs++;
		return &g->prog;
	}
	else
	{
		for (i = 0; *sbuiltins[i].name; i++)
		{
			if (sbuiltins[i].qrtype == qrenderer && !strcmp(sbuiltins[i].name, name))
			{
#ifdef GLQUAKE
				if (gl_config.gles)
				{
					if (sbuiltins[i].apiver != 100)
						continue;
				}
				else
				{
					if (sbuiltins[i].apiver == 100)
						continue;
				}
#endif
				Shader_LoadPermutations(&g->prog, sbuiltins[i].body, sbuiltins[i].qrtype, sbuiltins[i].apiver);

				g->prog.refs++;
				return &g->prog;
			}
		}
	}
	g->failed = true;
	return NULL;
}

static void Shader_ProgAutoFields(program_t *prog, char **cvarfnames)
{
	unsigned int i, p;
	qboolean found;
	int uniformloc;
	char tmpname[128];
	cvar_t *cvar;
	static struct
	{
		char *name;
		enum shaderprogparmtype_e ptype;
	} u[] =
	{
		/*vertex attributes*/
		{"v_position",				SP_ATTR_VERTEX},
		{"v_colour",				SP_ATTR_COLOUR},
		{"v_texcoord",				SP_ATTR_TEXCOORD},
		{"v_lmcoord",				SP_ATTR_LMCOORD},
		{"v_normal",				SP_ATTR_NORMALS},
		{"v_svector",				SP_ATTR_SNORMALS},
		{"v_tvector",				SP_ATTR_TNORMALS},

		/*matricies*/
		{"m_model",					SP_MODELMATRIX},
		{"m_view",					SP_VIEWMATRIX},
		{"m_modelview",				SP_MODELVIEWMATRIX},
		{"m_projection",			SP_PROJECTIONMATRIX},
		{"m_modelviewprojection",	SP_MODELVIEWPROJECTIONMATRIX},

		/*ent properties*/
		{"e_time",					SP_TIME},
		{"e_eyepos",				SP_EYEPOS},
		{"e_colour",				SP_ENTCOLOURS},
		{"e_colourident",			SP_ENTCOLOURSIDENT},
		{"e_topcolour",				SP_TOPCOLOURS},
		{"e_bottomcolour",			SP_BOTTOMCOLOURS},
		{"e_light_dir",				SP_E_L_DIR},
		{"e_light_mul",				SP_E_L_MUL},
		{"e_light_ambient",			SP_E_L_AMBIENT},
		{NULL}
	};
	prog->numparams = 0;
#ifdef GLQUAKE
	if (qrenderer == QR_OPENGL)
	{
		if (gl_config.nofixedfunc)
			prog->nofixedcompat = true;

		/*set cvar unirforms*/
		for (i = 0; cvarfnames[i]; i++)
		{
			for (p = 0; cvarfnames[i][p] && (unsigned char)cvarfnames[i][p] > 32 && p < sizeof(tmpname)-1; p++)
				tmpname[p] = cvarfnames[i][p];
			tmpname[p] = 0;
			cvar = Cvar_FindVar(tmpname);
			if (!cvar)
				continue;
			cvar->flags |= CVAR_SHADERSYSTEM;
			for (p = 0; p < PERMUTATIONS; p++)
			{
				if (!prog->handle[p].glsl)
					continue;
				GLSlang_UseProgram(prog->handle[p].glsl);
				uniformloc = qglGetUniformLocationARB(prog->handle[p].glsl, va("cvar_%s", tmpname));
				if (uniformloc != -1)
					qglUniform1fARB(uniformloc, cvar->value);
			}
		}
		for (i = 0; u[i].name; i++)
		{
			found = false;
			for (p = 0; p < PERMUTATIONS; p++)
			{
				if (!prog->handle[p].glsl)
					continue;
				GLSlang_UseProgram(prog->handle[p].glsl);
				if (u[i].ptype >= SP_FIRSTUNIFORM)
					uniformloc = qglGetUniformLocationARB(prog->handle[p].glsl, u[i].name);
				else
					uniformloc = qglGetAttribLocationARB(prog->handle[p].glsl, u[i].name);
				if (uniformloc != -1)
					found = true;
				prog->parm[prog->numparams].handle[p] = uniformloc;
			}
			if (found)
			{
				prog->parm[prog->numparams].type = u[i].ptype;
				prog->numparams++;

				if (u[i].ptype < SP_FIRSTUNIFORM)
					prog->nofixedcompat = true;
			}
		}
		/*set texture uniforms*/
		for (p = 0; p < PERMUTATIONS; p++)
		{
			if (!prog->handle[p].glsl)
				continue;
			GLSlang_UseProgram(prog->handle[p].glsl);
			for (i = 0; i < 8; i++)
			{
				uniformloc = qglGetUniformLocationARB(prog->handle[p].glsl, va("s_t%i", i));
				if (uniformloc != -1)
					qglUniform1iARB(uniformloc, i);
			}
		}
		GLSlang_UseProgram(0);
	}
#endif
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
			Con_Printf("shader \"%s\" missing program string\n", shader->name);
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
			programbody = BZ_Malloc(end - start + 1);
			memcpy(programbody, start, end-start);
			programbody[end-start] = 0;
			*ptr = end+1;/*skip over it all*/

			shader->prog = malloc(sizeof(*shader->prog));
			memset(shader->prog, 0, sizeof(*shader->prog));
			shader->prog->refs = 1;
			Shader_LoadPermutations(shader->prog, programbody, qrtype, 0);

			BZ_Free(programbody);
		}
		return;
	}

	shader->prog = Shader_LoadGeneric(Shader_ParseString(ptr), qrtype);
}

static void Shader_GLSLProgramName (shader_t *shader, shaderpass_t *pass, char **ptr)
{
	Shader_SLProgramName(shader,pass,ptr,QR_OPENGL);
}
static void Shader_ProgramName (shader_t *shader, shaderpass_t *pass, char **ptr)
{
	Shader_SLProgramName(shader,pass,ptr,qrenderer);
}
static void Shader_HLSLProgramName (shader_t *shader, shaderpass_t *pass, char **ptr)
{
	Shader_SLProgramName(shader,pass,ptr,QR_DIRECT3D);
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
		parmtype = SP_TIME;
	else if (!Q_stricmp(token, "eyepos"))
		parmtype = SP_EYEPOS;
	else if (!Q_stricmp(token, "entmatrix"))
		parmtype = SP_ENTMATRIX;
	else if (!Q_stricmp(token, "colours") || !Q_stricmp(token, "colors"))
		parmtype = SP_ENTCOLOURS;
	else if (!Q_stricmp(token, "upper"))
		parmtype = SP_TOPCOLOURS;
	else if (!Q_stricmp(token, "lower"))
		parmtype = SP_BOTTOMCOLOURS;
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
				if (!prog->handle[p].glsl)
					continue;
				GLSlang_UseProgram(prog->handle[p].glsl);
				if (parmtype >= SP_FIRSTUNIFORM)
					uniformloc = qglGetUniformLocationARB(prog->handle[p].glsl, token);
				else
					uniformloc = qglGetAttribLocationARB(prog->handle[p].glsl, token);
				prog->parm[prog->numparams].handle[p] = uniformloc;
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
			if (!foundone && !silent)
				Con_Printf("shader %s: param \"%s\" not found\n", shader->name, token);
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

	{"glslprogram",		Shader_GLSLProgramName},
	{"program",			Shader_ProgramName},	//legacy
	{"hlslprogram",		Shader_HLSLProgramName},	//for d3d
	{"param",			Shader_ProgramParam},

	/*doom3 compat*/
	{"diffusemap",		Shader_DiffuseMap},
	{"bumpmap",			NULL},
	{"specularmap",		NULL},
	{"nonsolid",		NULL},
	{"noimpact",		NULL},
	{"translucent",		Shader_Translucent},
	{"noshadows",		NULL},
	{"nooverlays",		NULL},
	{"nofragment",		NULL},

	{NULL,				NULL}
};

// ===============================================================

static qboolean ShaderPass_MapGen (shader_t *shader, shaderpass_t *pass, char *tname)
{
	if (!Q_stricmp (tname, "$lightmap"))
	{
		pass->tcgen = TC_GEN_LIGHTMAP;
		pass->flags |= SHADER_PASS_LIGHTMAP | SHADER_PASS_NOMIPMAP;
		pass->texgen = T_GEN_LIGHTMAP;
		shader->flags |= SHADER_HASLIGHTMAP;
	}
	else if (!Q_stricmp (tname, "$deluxmap"))
	{
		pass->tcgen = TC_GEN_LIGHTMAP;
		pass->flags |= SHADER_PASS_DELUXMAP | SHADER_PASS_NOMIPMAP;
		pass->texgen = T_GEN_DELUXMAP;
	}
	else if (!Q_stricmp (tname, "$diffuse"))
	{
		pass->texgen = T_GEN_DIFFUSE;
		pass->tcgen = TC_GEN_BASE;
		shader->flags |= SHADER_NOIMAGE;
	}
	else if (!Q_stricmp (tname, "$normalmap"))
	{
		pass->texgen = T_GEN_NORMALMAP;
		pass->tcgen = TC_GEN_BASE;
	}
	else if (!Q_stricmp (tname, "$specular"))
	{
		pass->texgen = T_GEN_SPECULAR;
		pass->tcgen = TC_GEN_BASE;
	}
	else if (!Q_stricmp (tname, "$fullbright"))
	{
		pass->texgen = T_GEN_FULLBRIGHT;
		pass->tcgen = TC_GEN_BASE;
	}
	else if (!Q_stricmp (tname, "$upperoverlay"))
	{
		shader->flags |= SHADER_HASTOPBOTTOM;
		pass->texgen = T_GEN_UPPEROVERLAY;
		pass->tcgen = TC_GEN_BASE;
	}
	else if (!Q_stricmp (tname, "$loweroverlay"))
	{
		shader->flags |= SHADER_HASTOPBOTTOM;
		pass->texgen = T_GEN_LOWEROVERLAY;
		pass->tcgen = TC_GEN_BASE;
	}
	else if (!Q_stricmp (tname, "$shadowmap"))
	{
		pass->texgen = T_GEN_SHADOWMAP;
		pass->tcgen = TC_GEN_BASE;	//FIXME: moo!
	}
	else if (!Q_stricmp (tname, "$currentrender"))
	{
		pass->texgen = T_GEN_CURRENTRENDER;
		pass->tcgen = TC_GEN_BASE;	//FIXME: moo!
	}
	else
		return false;
	return true;
}

static void Shaderpass_Map (shader_t *shader, shaderpass_t *pass, char **ptr)
{
	int flags;
	char *token;

	pass->anim_frames[0] = r_nulltex;

	token = Shader_ParseString (ptr);
	if (!ShaderPass_MapGen(shader, pass, token))
	{
		pass->texgen = T_GEN_SINGLEMAP;
		flags = Shader_SetImageFlags (shader);

		pass->tcgen = TC_GEN_BASE;
		pass->anim_frames[0] = Shader_FindImage (token, flags);
	}
}

static void Shaderpass_AnimMap (shader_t *shader, shaderpass_t *pass, char **ptr)
{
	int flags;
	char *token;
	texid_t image;

	flags = Shader_SetImageFlags (shader);

	pass->tcgen = TC_GEN_BASE;
	pass->flags |= SHADER_PASS_ANIMMAP;
	pass->texgen = T_GEN_ANIMMAP;
	pass->anim_fps = (int)Shader_ParseFloat (ptr);
	pass->anim_numframes = 0;

	for ( ; ; )
	{
		token = Shader_ParseString(ptr);
		if (!token[0])
		{
			break;
		}

		if (pass->anim_numframes < SHADER_ANIM_FRAMES_MAX)
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

	if (!ShaderPass_MapGen(shader, pass, token))
	{
		flags = Shader_SetImageFlags (shader);

		pass->tcgen = TC_GEN_BASE;
		pass->anim_frames[0] = Shader_FindImage (token, flags | IF_CLAMP);
		pass->texgen = T_GEN_SINGLEMAP;

		if (!TEXVALID(pass->anim_frames[0]))
		{
			pass->anim_frames[0] = missing_texture;
			Con_DPrintf (CON_WARNING "Shader %s has a stage with no image: %s.\n", shader->name, token);
		}
	}
	pass->flags |= SHADER_PASS_CLAMP;
}

static void Shaderpass_VideoMap (shader_t *shader, shaderpass_t *pass, char **ptr)
{
	char		*token;

	token = Shader_ParseString (ptr);

#ifdef NOMEDIA
#else
	if (pass->cin)
		Z_Free (pass->cin);

	pass->cin = Media_StartCin(token);
	if (!pass->cin)
		pass->cin = Media_StartCin(va("video/%s.roq", token));
	else
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
		Shader_ParseFunc ( ptr, &pass->rgbgen_func);
	}
	else if (!Q_stricmp(token, "entity"))
		pass->rgbgen = RGB_GEN_ENTITY;
	else if (!Q_stricmp (token, "oneMinusEntity"))
		pass->rgbgen = RGB_GEN_ONE_MINUS_ENTITY;
	else if (!Q_stricmp (token, "vertex"))
		pass->rgbgen = RGB_GEN_VERTEX;
	else if (!Q_stricmp (token, "oneMinusVertex"))
		pass->rgbgen = RGB_GEN_ONE_MINUS_VERTEX;
	else if (!Q_stricmp (token, "lightingDiffuse"))
		pass->rgbgen = RGB_GEN_LIGHTING_DIFFUSE;
	else if (!Q_stricmp (token, "exactvertex"))
		pass->rgbgen = RGB_GEN_EXACT_VERTEX;
	else if (!Q_stricmp (token, "const") || !Q_stricmp (token, "constant"))
	{
		pass->rgbgen = RGB_GEN_CONST;
		pass->rgbgen_func.type = SHADER_FUNC_CONSTANT;

		Shader_ParseVector (ptr, pass->rgbgen_func.args);
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
		shader->portaldist = Shader_ParseFloat(ptr);
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

		Shader_ParseFunc (ptr, &pass->alphagen_func);
	}
	else if ( !Q_stricmp (token, "lightingspecular"))
	{
		pass->alphagen = ALPHA_GEN_SPECULAR;
	}
	else if ( !Q_stricmp (token, "const") || !Q_stricmp (token, "constant"))
	{
		pass->alphagen = ALPHA_GEN_CONST;
		pass->alphagen_func.type = SHADER_FUNC_CONSTANT;
		pass->alphagen_func.args[0] = fabs(Shader_ParseFloat(ptr));
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

	speed = Shader_ParseFloat(ptr);
	min = Shader_ParseFloat(ptr);
	max = Shader_ParseFloat(ptr);

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

	pass->shaderbits &= ~(SBITS_BLEND_BITS);

	token = Shader_ParseString (ptr);
	if ( !Q_stricmp (token, "diffusemap"))
	{
		//if the shader is translucent then this pass must be meant to be blended
		if (shader->flags & SHADER_BLEND)
			pass->shaderbits |= SBITS_SRCBLEND_SRC_ALPHA | SBITS_DSTBLEND_ONE_MINUS_SRC_ALPHA;
		else
			pass->shaderbits |= SBITS_SRCBLEND_NONE | SBITS_DSTBLEND_NONE;
	}
	else if ( !Q_stricmp (token, "blend"))
	{
		pass->shaderbits |= SBITS_SRCBLEND_SRC_ALPHA | SBITS_DSTBLEND_ONE_MINUS_SRC_ALPHA;
	}
	else if (!Q_stricmp (token, "filter"))
	{
		pass->shaderbits |= SBITS_SRCBLEND_DST_COLOR | SBITS_DSTBLEND_ZERO;
	}
	else if (!Q_stricmp (token, "add"))
	{
		pass->shaderbits |= SBITS_SRCBLEND_ONE | SBITS_DSTBLEND_ONE;
	}
	else if (!Q_stricmp (token, "replace"))
	{
		pass->shaderbits |= SBITS_SRCBLEND_NONE | SBITS_DSTBLEND_NONE;
	}
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

static void Shaderpass_TcMod (shader_t *shader, shaderpass_t *pass, char **ptr)
{
	int i;
	tcmod_t *tcmod;
	char *token;

	if (pass->numtcmods >= SHADER_TCMOD_MAX)
	{
		return;
	}

	tcmod = &pass->tcmods[pass->numtcmods];

	token = Shader_ParseString (ptr);
	if (!Q_stricmp (token, "rotate"))
	{
		tcmod->args[0] = -Shader_ParseFloat(ptr) / 360.0f;
		if (!tcmod->args[0])
		{
			return;
		}

		tcmod->type = SHADER_TCMOD_ROTATE;
	}
	else if ( !Q_stricmp (token, "scale") )
	{
		tcmod->args[0] = Shader_ParseFloat (ptr);
		tcmod->args[1] = Shader_ParseFloat (ptr);
		tcmod->type = SHADER_TCMOD_SCALE;
	}
	else if ( !Q_stricmp (token, "scroll") )
	{
		tcmod->args[0] = Shader_ParseFloat (ptr);
		tcmod->args[1] = Shader_ParseFloat (ptr);
		tcmod->type = SHADER_TCMOD_SCROLL;
	}
	else if (!Q_stricmp(token, "stretch"))
	{
		shaderfunc_t func;

		Shader_ParseFunc(ptr, &func);

		tcmod->args[0] = func.type;
		for (i = 1; i < 5; ++i)
			tcmod->args[i] = func.args[i-1];
		tcmod->type = SHADER_TCMOD_STRETCH;
	}
	else if (!Q_stricmp (token, "transform"))
	{
		for (i = 0; i < 6; ++i)
			tcmod->args[i] = Shader_ParseFloat (ptr);
		tcmod->type = SHADER_TCMOD_TRANSFORM;
	}
	else if (!Q_stricmp (token, "turb"))
	{
		for (i = 0; i < 4; i++)
			tcmod->args[i] = Shader_ParseFloat (ptr);
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

	token = Shader_ParseString ( ptr );
	if (!strcmp(token, "static"))
	{
		tcmod->args[0] = Shader_ParseFloat ( ptr );
	}
	else
	{
		tcmod->args[0] = atof(token);
	}

	while (**ptr == ' ' || **ptr == '\t')
		*ptr+=1;
	if (**ptr == ',')
		*ptr+=1;

	token = Shader_ParseString ( ptr );
	if (!strcmp(token, "static"))
	{
		tcmod->args[1] = Shader_ParseFloat ( ptr );
	}
	else
	{
		tcmod->args[1] = atof(token);
	}

	pass->numtcmods++;
}

static void Shaderpass_Scroll ( shader_t *shader, shaderpass_t *pass, char **ptr )
{
	//seperate x and y
	char *token;
	tcmod_t *tcmod;

	tcmod = &pass->tcmods[pass->numtcmods];

	token = Shader_ParseString ( ptr );
	if (!strcmp(token, "static"))
	{
		tcmod->type = SHADER_TCMOD_SCROLL;
		tcmod->args[0] = Shader_ParseFloat ( ptr );
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
		tcmod->args[1] = Shader_ParseFloat ( ptr );
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
	if (Shader_ParseFloat(ptr) == 0.5)
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
	if (pass->tcgen == TC_GEN_BASE)
		pass->tcgen = TC_GEN_SKYBOX;
	pass->texgen = T_GEN_SKYBOX;
	pass->anim_frames[0] = r_nulltex;//Shader_FindImage(token, flags);

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
	{"cameracubemap",Shaderpass_CubeMap},
	{NULL,			NULL}
};

// ===============================================================

int Shader_InitCallback (const char *name, int size, void *param)
{
	strcpy(shaderbuf+shaderbuflen, name);
	Shader_MakeCache(shaderbuf+shaderbuflen);
	shaderbuflen += strlen(name)+1;

	return true;
}

qboolean Shader_Init (void)
{
	shaderbuflen = 0;

	r_shaders = calloc(MAX_SHADERS, sizeof(shader_t));

	shader_hash = calloc (HASH_SIZE, sizeof(*shader_hash));

	shader_active_hash_mem = malloc(Hash_BytesForBuckets(1024));
	memset(shader_active_hash_mem, 0, Hash_BytesForBuckets(1024));
	Hash_InitTable(&shader_active_hash, 1024, shader_active_hash_mem);

	Shader_FlushGenerics();
	shader_rescan_needed = true;
	Shader_NeedReload();
	Shader_DoReload();
	return true;
}

static void Shader_MakeCache ( char *path )
{
	unsigned int key, i;
	char filename[MAX_QPATH];
	char *buf, *ptr, *token, *t;
	shadercache_t *cache;
	int size;

	Com_sprintf( filename, sizeof(filename), "%s", path );
	Con_DPrintf ( "...loading '%s'\n", filename );

	size = FS_LoadFile ( filename, (void **)&buf );
	if ( !buf || size <= 0 )
	{
		return;
	}

	ptr = buf;
	do
	{
		if ( ptr - buf >= size )
			break;

		token = COM_ParseExt ( &ptr, true );
		if ( !token[0] || ptr - buf >= size )
			break;

		COM_CleanUpPath(token);

		t = NULL;
		Shader_GetPathAndOffset ( token, &t, &i );
		if (t)
		{
			ptr = Shader_Skip ( ptr );
			continue;
		}

		key = Hash_Key ( token, HASH_SIZE );

		cache = ( shadercache_t * )Z_Malloc ( sizeof(shadercache_t) );
		cache->hash_next = shader_hash[key];
		cache->path = path;
		cache->offset = ptr - buf;
		Com_sprintf ( cache->name, MAX_QPATH, "%s", token ); // warning: format not a string literal and no format arguments
		shader_hash[key] = cache;

		ptr = Shader_Skip ( ptr );
	} while ( ptr );

	FS_FreeFile ( buf );
}

char *Shader_Skip ( char *ptr )
{
	char *tok;
	int brace_count;

    // Opening brace
	tok = COM_ParseExt ( &ptr, true );

	if (!ptr)
		return NULL;

	if ( tok[0] != '{' )
	{
		tok = COM_ParseExt ( &ptr, true );
	}

	for (brace_count = 1; brace_count > 0 ; ptr++)
	{
		tok = COM_ParseExt ( &ptr, true );

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

static void Shader_GetPathAndOffset ( char *name, char **path, unsigned int *offset )
{
	unsigned int key;
	shadercache_t *cache;

	key = Hash_Key ( name, HASH_SIZE );
	cache = shader_hash[key];

	for ( ; cache; cache = cache->hash_next )
	{
		if ( !Q_stricmp (cache->name, name) )
		{
			*path = cache->path;
			*offset = cache->offset;
			return;
		}
	}

	path = NULL;
}

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

#ifdef GLQUAKE
	if (qrenderer == QR_OPENGL && shader->prog)
	{
		program_t *prog = shader->prog;
		int p;
		if (--prog->refs == 0)
		{
			for (p = 0; p < PERMUTATIONS; p++)
			{
				if (prog->handle[p].glsl)
					qglDeleteProgramObject_(prog->handle[p].glsl);
			}
			free(prog);
		}
		shader->prog = NULL;
	}
#endif
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
}

void Shader_Reset(shader_t *s)
{
	char name[MAX_QPATH];
	int uses = s->uses;
	shader_gen_t *defaultgen = s->generator;
	const char *genargs = s->genargs;
	texnums_t dt = s->defaulttextures;
	Q_strncpyz(name, s->name, sizeof(name));
	Shader_Free(s);
	memset(s, 0, sizeof(*s));

	s->defaulttextures = dt;
	s->generator = defaultgen;
	s->genargs = genargs;
	s->uses = uses;
	Q_strncpyz(s->name, name, sizeof(s->name));
	Hash_Add(&shader_active_hash, s->name, s, &s->bucket);
}

void Shader_Shutdown (void)
{
	int i;
	shader_t *shader;
	shadercache_t *cache, *cache_next;

	shader = r_shaders;
	if (!r_shaders)
		return;	/*nothing needs freeing yet*/
	for (i = 0; i < MAX_SHADERS; i++, shader++)
	{
		if ( !shader->uses )
			continue;

		Shader_Free ( shader );
	}

	for ( i = 0; i < HASH_SIZE; i++ )
	{
		cache = shader_hash[i];

		for ( ; cache; cache = cache_next )
		{
			cache_next = cache->hash_next;
			cache->hash_next = NULL;
			Z_Free ( cache );
		}
	}

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
		}
		else
		{
#ifdef _MSC_VER
#pragma message("is this correct?")
#endif
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
	pass->tcgen = TC_GEN_BASE;
	pass->numtcmods = 0;
	pass->numMergedPasses = 1;

	if (shader->flags & SHADER_NOMIPMAPS)
		pass->flags |= SHADER_PASS_NOMIPMAP;

	while ( *ptr )
	{
		token = COM_ParseExt (ptr, true);

		if ( !token[0] )
		{
			continue;
		}
		else if ( token[0] == '}' )
		{
			break;
		}
		else if (!Q_stricmp(token, "if"))
		{
			int nest = 0;
			qboolean conditionistrue = Shader_EvaluateCondition(ptr);

			while (*ptr)
			{
				token = COM_ParseExt (ptr, true);
				if ( !token[0] )
					continue;
				else if (token[0] == ']')
				{
					if (--nest <= 0)
					{
						nest++;
						if (!strcmp(token, "]["))
							conditionistrue = !conditionistrue;
						else
							break;
					}
				}
				else if (token[0] == '[')
					nest++;
				else if (conditionistrue)
				{
					Shader_Parsetok (shader, pass, shaderpasskeys, token, ptr);
				}
			}
		}
		else if ( Shader_Parsetok (shader, pass, shaderpasskeys, token, ptr) )
		{
			break;
		}
	}

	// check some things
	if ( ignore )
	{
		Shader_Free ( shader );
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

	if ((shader->flags & SHADER_SKY) && (shader->flags & SHADER_DEPTHWRITE))
	{
#ifdef _MSC_VER
#pragma message("is this valid?")
#endif
		pass->shaderbits &= ~SBITS_MISC_DEPTHWRITE;
	}
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

	// Next Line
	while (ptr)
	{
		token = COM_ParseExt ( ptr, false );
		if ( !token[0] ) {
			break;
		}
	}

	return false;
}

void Shader_SetPassFlush (shaderpass_t *pass, shaderpass_t *pass2)
{
	qboolean config_tex_env_combine = 1;//0;
	qboolean config_nv_tex_env_combine4 = 1;//0;
	qboolean config_env_add = 1;//0;

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

	// check if we can use multiple passes
	if (pass2->blendmode == PBM_DOTPRODUCT)
	{
		pass->numMergedPasses++;
	}
	else if (pass->numMergedPasses < be_maxpasses)
	{
		if (pass->blendmode == PBM_REPLACE || pass->blendmode == PBM_REPLACELIGHT)
		{
			if ((pass2->blendmode == PBM_DECAL && config_tex_env_combine) ||
				(pass2->blendmode == PBM_ADD && config_env_add) ||
				(pass2->blendmode && pass2->blendmode != PBM_ADD) ||	config_nv_tex_env_combine4)
			{
				pass->numMergedPasses++;
			}
		}
		else if (pass->blendmode == PBM_ADD &&
			pass2->blendmode == PBM_ADD && config_env_add)
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

	if (pass2->texgen == T_GEN_LIGHTMAP && pass2->blendmode == PBM_MODULATE)
		pass2->blendmode = PBM_OVERBRIGHT;
}

void Shader_SetFeatures ( shader_t *s )
{
	int i;
	qboolean trnormals;
	shaderpass_t *pass;

	s->features = MF_NONE;

	for (i = 0, trnormals = true; i < s->numdeforms; i++)
	{
		switch (s->deforms[i].type)
		{
			case DEFORMV_BULGE:
			case DEFORMV_WAVE:
				trnormals = false;
			case DEFORMV_NORMAL:
				s->features |= MF_NORMALS;
				break;
			case DEFORMV_MOVE:
				break;
			default:
				trnormals = false;
				break;
		}
	}

	if (trnormals)
	{
		s->features |= MF_TRNORMALS;
	}

	for (i = 0, pass = s->passes; i < s->numpasses; i++, pass++)
	{
		switch (pass->rgbgen)
		{
			case RGB_GEN_LIGHTING_DIFFUSE:
				s->features |= MF_NORMALS;
				break;
			case RGB_GEN_VERTEX:
			case RGB_GEN_ONE_MINUS_VERTEX:
			case RGB_GEN_EXACT_VERTEX:
				s->features |= MF_COLORS;
				break;
			default:
				break;
		}

		switch ( pass->alphagen )
		{
			case ALPHA_GEN_SPECULAR:
				s->features |= MF_NORMALS;
				break;
			case ALPHA_GEN_VERTEX:
				s->features |= MF_COLORS;
				break;
			default:
				break;
		}

		switch (pass->tcgen)
		{
			default:
				s->features |= MF_STCOORDS;
				break;
			case TC_GEN_LIGHTMAP:
				s->features |= MF_LMCOORDS;
				break;
			case TC_GEN_ENVIRONMENT:
			case TC_GEN_NORMAL:
				s->features |= MF_NORMALS;
				break;
			case TC_GEN_SVECTOR:
			case TC_GEN_TVECTOR:
				s->features |= MF_NORMALS;
				break;
		}
	}
}

void Shader_Finish (shader_t *s)
{
	int i;
	shaderpass_t *pass;

	if (s->flags & SHADER_SKY)
	{
		/*skies go all black if fastsky is set*/
		if (r_fastsky.ival)
			s->flags = 0;
		/*or if its purely a skybox and has missing textures*/
		if (!s->numpasses)
			for (i = 0; i < 6; i++)
				if (missing_texture.num == s->skydome->farbox_textures[i].num)
					s->flags = 0;
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

	if (TEXVALID(s->defaulttextures.base))
		s->flags &= ~SHADER_NOIMAGE;

	if (!s->numpasses && !(s->flags & (SHADER_NODRAW|SHADER_SKY)) && !s->fog_dist)
	{
		pass = &s->passes[s->numpasses++];
		pass = &s->passes[0];
		pass->tcgen = TC_GEN_BASE;
		if (TEXVALID(s->defaulttextures.base))
			pass->texgen = T_GEN_DIFFUSE;
		else
		{
			pass->texgen = T_GEN_SINGLEMAP;
			pass->anim_frames[0] = R_LoadHiResTexture(s->name, NULL, IF_NOALPHA);
			if (!TEXVALID(pass->anim_frames[0]))
			{
				Con_Printf("Shader %s failed to load default texture\n", s->name);
				pass->anim_frames[0] = missing_texture;
			}
			Con_Printf("Shader %s with no passes and no surfaceparm nodraw, inserting pass\n", s->name);
		}
		pass->shaderbits |= SBITS_MISC_DEPTHWRITE;
		pass->rgbgen = RGB_GEN_VERTEX;
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

	if (r_vertexlight.value && !s->prog)
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
			if (pass->rgbgen == RGB_GEN_VERTEX)
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

			s->passes[0].rgbgen = RGB_GEN_VERTEX;
			s->passes[0].alphagen = ALPHA_GEN_IDENTITY;
			s->passes[0].blendmode = 0;
			s->passes[0].flags &= ~(SHADER_PASS_ANIMMAP|SHADER_PASS_NOCOLORARRAY);
			pass->shaderbits &= ~SBITS_BLEND_BITS;
			s->passes[0].shaderbits |= SBITS_MISC_DEPTHWRITE;
			s->passes[0].numMergedPasses = 1;
			s->numpasses = 1;
			s->flags |= SHADER_DEPTHWRITE;
		}
	}
done:;

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
				if (!s->fog_dist && !(pass->flags & SHADER_PASS_LIGHTMAP))
					pass->rgbgen = RGB_GEN_IDENTITY_LIGHTING;
				else
					pass->rgbgen = RGB_GEN_IDENTITY;
			}

			Shader_SetBlendmode (pass);
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

	if (s->prog)
	{
		if (!s->numpasses)
			s->numpasses = 1;
		s->passes->numMergedPasses = s->numpasses;
	}

	Shader_SetFeatures(s);

#ifdef FORCEGLSL
	BE_GenerateProgram(s);
#endif
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

void R_BuildDefaultTexnums(texnums_t *tn, shader_t *shader)
{
	extern cvar_t gl_bump;

	/*dlights/realtime lighting needs some stuff*/
	if (!TEXVALID(tn->base))
	{
		tn->base = R_LoadHiResTexture(shader->name, NULL, IF_NOALPHA);
	}
	if (TEXVALID(tn->base))
		shader->flags &= ~SHADER_NOIMAGE;

	if (gl_bump.ival)
	{
		if (!TEXVALID(tn->bump))
			tn->bump = R_LoadHiResTexture(va("%s_norm", shader->name), NULL, IF_NOALPHA);
		if (!TEXVALID(tn->bump))
			tn->bump = R_LoadHiResTexture(va("%s_bump", shader->name), NULL, IF_NOALPHA);
		if (!TEXVALID(tn->bump))
			tn->bump = R_LoadHiResTexture(va("normalmaps/%s", shader->name), NULL, IF_NOALPHA);
	}

	if (shader->flags & SHADER_HASTOPBOTTOM)
	{
		if (!TEXVALID(tn->loweroverlay))
			tn->loweroverlay = R_LoadHiResTexture(va("%s_pants", shader->name), NULL, 0);	/*how rude*/
		if (!TEXVALID(tn->upperoverlay))
			tn->upperoverlay = R_LoadHiResTexture(va("%s_shirt", shader->name), NULL, 0);
	}

	shader->defaulttextures = *tn;
}

void Shader_DefaultScript(char *shortname, shader_t *s, const void *args)
{
	const char *f = args;
	if (!args)
		return;
	while (*f == ' ' || *f == '\t' || *f == '\n' || *f == '\r')
		f++;
	if (*f == '{')
	{
		f++;
		Shader_ReadShader(s, (void*)f);
	}
};

void Shader_DefaultBSPLM(char *shortname, shader_t *s, const void *args)
{
	char *builtin = NULL;
	if (!builtin && r_drawflat.value)
		builtin = (
				"{\n"
					"{\n"
						"map $lightmap\n"
						"tcgen lightmap\n"
						"rgbgen const $r_floorcolour\n"
					"}\n"
				"}\n"
			);
#ifdef GLQUAKE
	if (!builtin && gl_config.arb_shader_objects && gl_config.nofixedfunc)
	{
			builtin = (
				"{\n"
					"program defaultwall\n"
					/*"param texture 0 tex_diffuse\n"
					"param texture 1 tex_lightmap\n"
					"param texture 2 tex_normalmap\n"
					"param texture 3 tex_deluxmap\n"
					"param texture 4 tex_fullbright\n"*/
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
#endif
	if (!builtin)
		builtin = (
				"{\n"
					"if $deluxmap\n"
					"[\n"
						"{\n"
							"map $normalmap\n"
							"tcgen base\n"
							"depthwrite\n"
						"}\n"
						"{\n"
							"map $deluxmap\n"
							"tcgen lightmap\n"
						"}\n"
					"]\n"
					"{\n"
						"map $diffuse\n"
						"tcgen base\n"
						"if gl_bump\n"
						"[\n"
							"blendfunc gl_one gl_zero\n"
						"]\n"
					"}\n"
					"if !r_fullbright\n"
					"[\n"
						"{\n"
							"map $lightmap\n"
							"blendfunc gl_dst_color gl_zero\n"
						"}\n"
					"]\n"
					"if gl_fb_bmodels\n"
					"[\n"
						"{\n"
							"map $fullbright\n"
							"blendfunc add\n"
							"depthfunc equal\n"
						"}\n"
					"]\n"
				"}\n"
			);

	Shader_DefaultScript(shortname, s, builtin);
}

void Shader_DefaultCinematic(char *shortname, shader_t *s, const void *args)
{
	Shader_DefaultScript(shortname, s,
		va(
			"{\n"
				"{\n"
					"videomap %s\n"
				"}\n"
			"}\n"
		, (const char*)args)
	);
}

/*shortname should begin with 'skybox_'*/
void Shader_DefaultSkybox(char *shortname, shader_t *s, const void *args)
{
	Shader_DefaultScript(shortname, s,
		va(
			"{\n"
				"skyparms %s - -\n"
			"}\n"
		, shortname+7)
	);
}

void Shader_DefaultBSPQ2(char *shortname, shader_t *s, const void *args)
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
	else if (!strncmp(shortname, "warp/", 5))
	{
		Shader_DefaultScript(shortname, s,
				"{\n"
					"program defaultwarp\n"
					"{\n"
						"map $diffuse\n"
						"tcmod turb 0 0.01 0.5 0\n"
					"}\n"
				"}\n"
			);
	}
	else if (!strncmp(shortname, "warp33/", 7))
	{
		Shader_DefaultScript(shortname, s,
				"{\n"
					"{\n"
						"map $diffuse\n"
						"tcmod turb 0 0.01 0.5 0\n"
						"alphagen const 0.333\n"
						"blendfunc blend\n"
					"}\n"
				"}\n"
			);
	}
	else if (!strncmp(shortname, "warp66/", 7))
	{
		Shader_DefaultScript(shortname, s,
				"{\n"
					"{\n"
						"map $diffuse\n"
						"tcmod turb 0 0.01 0.5 0\n"
						"alphagen const 0.666\n"
						"blendfunc blend\n"
					"}\n"
				"}\n"
			);
	}
	else if (!strncmp(shortname, "trans/", 7))
		Shader_DefaultScript(shortname, s,
				"{\n"
					"{\n"
						"map $diffuse\n"
						"alphagen const 1\n"
						"blendfunc blend\n"
					"}\n"
				"}\n"
			);
	else if (!strncmp(shortname, "trans33/", 7))
		Shader_DefaultScript(shortname, s,
				"{\n"
					"{\n"
						"map $diffuse\n"
						"alphagen const 0.333\n"
						"blendfunc blend\n"
					"}\n"
				"}\n"
			);
	else if (!strncmp(shortname, "trans66/", 7))
		Shader_DefaultScript(shortname, s,
				"{\n"
					"{\n"
						"map $diffuse\n"
						"alphagen const 0.666\n"
						"blendfunc blend\n"
					"}\n"
				"}\n"
			);
	else
		Shader_DefaultBSPLM(shortname, s, args);
}

void Shader_DefaultBSPQ1(char *shortname, shader_t *s, const void *args)
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
		//q1 water
		if (r_wateralpha.value == 0)
		{
			builtin = (
				"{\n"
					"sort blend\n"
					"surfaceparm nodraw\n"
					"surfaceparm nodlight\n"
				"}\n"
			);
		}
		else if (r_fastturb.ival)
		{
			builtin = (
				"{\n"
					"sort blend\n"
					"{\n"
						"map $whiteimage\n"
						"rgbgen const $r_fastturbcolour\n"
					"}\n"
					"surfaceparm nodlight\n"
				"}\n"
			);
		}
		else
		{
			builtin = (
				"{\n"
					"sort blend\n" /*make sure it always has the same sort order, so switching on/off wateralpha doesn't break stuff*/
					"program defaultwarp\n"
					"{\n"
						"map $diffuse\n"
						"tcmod turb 0.02 0.1 0.5 0.1\n"
						"if r_wateralpha != 1\n"
						"[\n"
							"alphagen const $r_wateralpha\n"
							"blendfunc gl_src_alpha gl_one_minus_src_alpha\n"
						"]\n"
					"}\n"
					"surfaceparm nodlight\n"
				"}\n"
			);
		}
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
				"[\n"
					"{\n"
						"map $normalmap\n"
						"tcgen base\n"
					"}\n"
					"{\n"
						"map $deluxmap\n"
						"tcgen lightmap\n"
					"}\n"
				"]\n"*/
				"{\n"
					"map $diffuse\n"
					"tcgen base\n"
					"alphamask\n"
				"}\n"
				"if $lightmap\n"
				"[\n"
					"{\n"
						"map $lightmap\n"
						"blendfunc gl_dst_color gl_zero\n"
						"depthfunc equal\n"
					"}\n"
				"]\n"
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

void Shader_DefaultBSPVertex(char *shortname, shader_t *s, const void *args)
{
	shaderpass_t *pass;
	pass = &s->passes[0];
	pass->tcgen = TC_GEN_BASE;
	pass->anim_frames[0] = R_LoadHiResTexture(shortname, NULL, 0);
	pass->shaderbits |= SBITS_MISC_DEPTHWRITE;
	pass->rgbgen = RGB_GEN_VERTEX;
	pass->alphagen = ALPHA_GEN_IDENTITY;
	pass->numMergedPasses = 1;
	Shader_SetBlendmode(pass);

	if (!TEXVALID(pass->anim_frames[0]))
	{
		Con_DPrintf (CON_WARNING "Shader %s has a stage with no image: %s.\n", s->name, shortname );
		pass->anim_frames[0] = missing_texture;
	}

	s->numpasses = 1;
	s->numdeforms = 0;
	s->flags = SHADER_DEPTHWRITE|SHADER_CULL_FRONT;
	s->features = MF_STCOORDS|MF_COLORS;
	s->sort = SHADER_SORT_OPAQUE;
	s->uses = 1;
}
void Shader_DefaultBSPFlare(char *shortname, shader_t *s, const void *args)
{
	shaderpass_t *pass;
	pass = &s->passes[0];
	pass->flags = SHADER_PASS_NOCOLORARRAY;
	pass->shaderbits |= SBITS_SRCBLEND_ONE|SBITS_DSTBLEND_ONE;
	pass->anim_frames[0] = R_LoadHiResTexture(shortname, NULL, 0);
	pass->rgbgen = RGB_GEN_VERTEX;
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
	s->features = MF_STCOORDS|MF_COLORS;
	s->sort = SHADER_SORT_ADDITIVE;
	s->uses = 1;

	s->flags |= SHADER_NODRAW;
}
void Shader_DefaultSkin(char *shortname, shader_t *s, const void *args)
{
	Shader_DefaultScript(shortname, s,
		"{\n"
			"program defaultskin\n"
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
void Shader_DefaultSkinShell(char *shortname, shader_t *s, const void *args)
{
	Shader_DefaultScript(shortname, s,
		"{\n"
			"sort blend\n"
			"deformvertexes normal 1 1\n"
			"{\n"
				"map $diffuse\n"
				"rgbgen entity\n"
				"alphagen entity\n"
				"blendfunc blend\n"
			"}\n"
		"}\n"
		);
}
void Shader_Default2D(char *shortname, shader_t *s, const void *genargs)
{
	Shader_DefaultScript(shortname, s,
		"{\n"
			"if $nofixed\n"
			"[\n"
				"program default2d\n"
			"]\n"
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

	s->defaulttextures.base = R_LoadHiResTexture(shortname, NULL, IF_NOPICMIP|IF_NOMIPMAP|IF_CLAMP);
	if (!TEXVALID(s->defaulttextures.base))
	{
		unsigned char data[4*4] = {0};
		s->defaulttextures.base = R_LoadTexture8("black", 4, 4, data, 0, 0);
		s->flags |= SHADER_NOIMAGE;

		s->width = 64;
		s->height = 64;
	}
	else
	{
		s->flags &= ~SHADER_NOIMAGE;
		s->width = image_width;
		s->height = image_height;
	}
}

//loads a shader string into an existing shader object, and finalises it and stuff
static void Shader_ReadShader(shader_t *s, char *shadersource)
{
	char *token;

// set defaults
	s->flags = SHADER_CULL_FRONT;
	s->uses = 1;

	while (shadersource)
	{
		token = COM_ParseExt (&shadersource, true);

		if ( !token[0] )
			continue;
		else if ( token[0] == '}' )
			break;
		else if (!Q_stricmp(token, "if"))
		{
			int nest = 0;
			qboolean conditionistrue = Shader_EvaluateCondition(&shadersource);

			while (shadersource)
			{
				token = COM_ParseExt (&shadersource, true);
				if ( !token[0] )
					continue;
                                else if (token[0] == ']')
                                {
                                        if (--nest <= 0)
                                        {
                                                nest++;
						if (!strcmp(token, "]["))
                                                        conditionistrue = !conditionistrue;
                                                else
                                                        break;
                                        }
                                }
                                else if (token[0] == '[')
                                        nest++;
				else if (conditionistrue)
				{
					if (token[0] == '{')
						Shader_Readpass (s, &shadersource);
					else
						Shader_Parsetok (s, NULL, shaderkeys, token, &shadersource);
				}
			}
		}
		else if ( token[0] == '{' )
			Shader_Readpass ( s, &shadersource );
		else if ( Shader_Parsetok (s, NULL, shaderkeys, token, &shadersource ) )
			break;
	}

	Shader_Finish ( s );
}

static qboolean Shader_ParseShader(char *shortname, char *usename, shader_t *s)
{
	unsigned int offset = 0, length;
	char path[MAX_QPATH];
	char *buf = NULL, *ts = NULL;

	Shader_GetPathAndOffset( shortname, &ts, &offset );

	if ( ts )
	{
		Com_sprintf ( path, sizeof(path), "%s", ts );
		length = FS_LoadFile ( path, (void **)&buf );
	}
	else
		length = 0;

	// the shader is in the shader scripts
	if ( ts && buf && (offset < length) )
	{
		char *file, *token;


		file = buf + offset;
		token = COM_ParseExt (&file, true);
		if ( !file || token[0] != '{' )
		{
			FS_FreeFile(buf);
			return false;
		}

		Shader_Reset(s);

		Shader_ReadShader(s, file);

		FS_FreeFile(buf);
		return true;
	}

	if (buf)
		FS_FreeFile(buf);

	return false;
}
void R_UnloadShader(shader_t *shader)
{
	if (shader->uses-- == 1)
		Shader_Free(shader);
}
static int R_LoadShader ( char *name, shader_gen_t *defaultgen, const char *genargs)
{
	int i, f = -1;
	char shortname[MAX_QPATH];
	shader_t *s;

	if (!*name)
		name = "gfx/white";

	*(int*)shortname = 0;
	COM_StripExtension ( name, shortname, sizeof(shortname));

	COM_CleanUpPath(shortname);

	// check the hash first
	s = Hash_Get(&shader_active_hash, shortname);
	if (s)
	{
		i = s - r_shaders;
		r_shaders[i].uses++;
		return i;
	}

	// not loaded, find a free slot
	for (i = 0; i < MAX_SHADERS; i++)
	{
		if (!r_shaders[i].uses)
		{
			if ( f == -1 )	// free shader
			{
				f = i;
				break;
			}
		}
	}

	if ( f == -1 )
	{
		Sys_Error( "R_LoadShader: Shader limit exceeded.");
		return f;
	}

	s = &r_shaders[f];

	Q_strncpyz(s->name, shortname, sizeof(s->name));
	s->generator = defaultgen;
	s->genargs = genargs;

	if (ruleset_allow_shaders.ival)
	{
#ifdef GLQUAKE
		if (qrenderer == QR_OPENGL)
		{
			if (gl_config.gles && gl_config.glversion >= 2)
			{
				if (Shader_ParseShader(va("%s_gles2", shortname), shortname, s))
				{
					return f;
				}
			}
			if (gl_config.glversion >= 3)
			{
				if (Shader_ParseShader(va("%s_glsl3", shortname), shortname, s))
				{
					return f;
				}
			}
			if (gl_config.arb_shader_objects)
			{
				if (Shader_ParseShader(va("%s_glsl", shortname), shortname, s))
				{
					return f;
				}
			}
		}
#endif
#ifdef D3DQUAKE
		if (qrenderer == QR_DIRECT3D)
		{
			{
				if (Shader_ParseShader(va("%s_hlsl", shortname), shortname, s))
				{
					return f;
				}
			}
		}
#endif
		if (Shader_ParseShader(shortname, shortname, s))
		{
			return f;
		}
	}

	// make a default shader

	if (s->generator)
	{
		Shader_Reset(s);

		if (!strcmp(shortname, "textures/common/clip"))
			Shader_DefaultScript(shortname, s,
				"{\n"
					"surfaceparm nodraw\n"
					"surfaceparm nodlight\n"
				"}\n");
		else
			s->generator(shortname, s, s->genargs);
		return f;
	}
	else
	{
		r_shaders[i].uses = 0;
	}
	return -1;
}

void Shader_DoReload(void)
{
	shader_t *s;
	unsigned int i;
	char shortname[MAX_QPATH];

	if (shader_rescan_needed && ruleset_allow_shaders.ival)
	{
		Con_Printf ( "Initializing Shaders.\n" );

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
	Con_Printf("Reloading all shaders\n");

	for (s = r_shaders, i = 0; i < MAX_SHADERS; i++, s++)
	{
		if (!s->uses)
			continue;

		strcpy(shortname, s->name);
		if (ruleset_allow_shaders.ival)
		{
#ifdef GLQUAKE
			if (qrenderer == QR_OPENGL && gl_config.arb_shader_objects)
			{
				if (Shader_ParseShader(va("%s_glsl", shortname), shortname, s))
				{
					continue;
				}
			}
#endif
			if (Shader_ParseShader(shortname, shortname, s))
			{
				continue;
			}
		}
		if (s->generator)
		{
			Shader_Reset(s);

			s->generator(shortname, s, s->genargs);
		}
	}
}

void Shader_NeedReload(void)
{
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

cin_t *R_ShaderFindCinematic(char *name)
{
#ifdef NOMEDIA
	return NULL;
#else
	int i;
	char shortname[MAX_QPATH];

	COM_StripExtension ( name, shortname, sizeof(shortname));

	COM_CleanUpPath(shortname);

	//try and find it
	for (i = 0; i < MAX_SHADERS; i++)
	{
		if (!r_shaders[i].uses)
			continue;

		if (!Q_stricmp (shortname, r_shaders[i].name) )
			break;
	}
	if (i == MAX_SHADERS)
		return NULL;

	//found the named shader.
	return R_ShaderGetCinematic(&r_shaders[i]);
#endif
}

shader_t *R_RegisterPic (char *name)
{
	return &r_shaders[R_LoadShader (name, Shader_Default2D, NULL)];
}

shader_t *R_RegisterShader (char *name, const char *shaderscript)
{
	return &r_shaders[R_LoadShader (name, Shader_DefaultScript, shaderscript)];
}

shader_t *R_RegisterShader_Lightmap (char *name)
{
	return &r_shaders[R_LoadShader (name, Shader_DefaultBSPLM, NULL)];
}

shader_t *R_RegisterShader_Vertex (char *name)
{
	return &r_shaders[R_LoadShader (name, Shader_DefaultBSPVertex, NULL)];
}

shader_t *R_RegisterShader_Flare (char *name)
{
	return &r_shaders[R_LoadShader (name, Shader_DefaultBSPFlare, NULL)];
}

shader_t *R_RegisterSkin (char *name)
{
	return &r_shaders[R_LoadShader (name, Shader_DefaultSkin, NULL)];
}
shader_t *R_RegisterCustom (char *name, shader_gen_t *defaultgen, const void *args)
{
	int i;
	i = R_LoadShader (name, defaultgen, args);
	if (i < 0)
		return NULL;
	return &r_shaders[i];
}
#endif //SERVERONLY
