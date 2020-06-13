#if !defined(GLQUAKE) && !defined(FTEENGINE)
#define GLQUAKE	//this is shit, but ensures index sizes come out the right size
#endif
#ifdef IQMTOOL
#define FTEPLUGIN
#endif
#include "../plugin.h"
#include "com_mesh.h"

#ifdef IQMTOOL
#define Con_Printf printf
#define Con_DPrintf printf
#endif

#ifdef SKELETALMODELS
#define GLTFMODELS
#endif

/*Limitations:
	materials:
		material names (when present) are assumed to be either globally unique, or the material attributes must match those of all other materials with the same name.
		texture modes (like clamp) must match on both axis (either both clamp or both wrap, no mixing)
		mirrored-repeat not supported
		mip-mag-mip filters must match (all linear, or all nearest)
		gltf1: techniques are disabled by default.
		gltf1: symbol names are linked via defines. this results in potential conflicts.
		gltf1: static uniforms are not set.
	animations:
		input framerates are not well-defined. this can result in issues when converting to other formats (especially with stepping anims). this does not necessarily affect directly-loaded animations.
		total nodes(+joints) must be < MAX_BONES, and ideally <MAX_GPU_BONES too but it is sufficient for that to be per-mesh.
	meshes:
		multiple texture coord sets are not supported.
		additional colours/weights attributes are not supported.
		multiple meshes with the same material will not be merged.
	scene:
		cameras are parsed, but are not necessarily useful as they are not exposed to the gamecode.
	extensions:
		no KHR_draco_mesh_compression, so many sample models will fail.
		unknown extensions will result in warning spam for each occurence. :(
*/


//'The units for all linear distances are meters.'
//'feh: 1 metre is approx. 26.24671916 qu.'
//if the player is 1.6m tall, and the player's model is around 48qu, then 1m=30qu, which is a slightly nicer number to work with, and 1qu is a really poorly defined unit.
#define MAX_MORPHWEIGHTS 16	//we're required to support up to 8 accessors (so mandatory max changes according to supplied data, so 8 morphs with just positions, 4 with positions+normals, or 2 with positions+normals+tangents - plus the base info)

#ifdef GLTFMODELS
static plugmodfuncs_t *modfuncs;
static plugfsfuncs_t *filefuncs;

static cvar_t *mod_gltf_scale;
static cvar_t *mod_gltf_fixbuggyanims;
static cvar_t *mod_gltf_privatematerials;
static cvar_t *mod_gltf_ignoretechniques;

#include <stdarg.h>
void VARGS Q_snprintfcat (char *dest, size_t size, const char *fmt, ...)
{
	va_list		argptr;

	size_t skip = strlen(dest);
	dest += skip;
	size -= skip;

	va_start (argptr, fmt);
	vsnprintf(dest, size, fmt, argptr);
	va_end (argptr);
}


typedef struct json_s
{
	const char *bodystart;
	const char *bodyend;

	struct json_s *parent;
	struct json_s *child;
	struct json_s *sibling;
	struct json_s **childlink;
	qboolean used;	//set to say when something actually read/walked it, so we can flag unsupported things gracefully
	char name[1];
} json_t;

//node destruction
static void JSON_Orphan(json_t *t)
{
	if (t->parent)
	{
		json_t *p = t->parent, **l = &p->child;
		while (*l)
		{
			if (*l == t)
			{
				*l = t->sibling;
				if (*l)
					p->childlink = l;
				break;
			}
			l = &(*l)->sibling;
		}
		t->parent = NULL;
		t->sibling = NULL;
	}
}
static void JSON_Destroy(json_t *t)
{
	if (t)
	{
		while(t->child)
			JSON_Destroy(t->child);
		JSON_Orphan(t);
		free(t);
	}
}

//node creation
static json_t *JSON_CreateNode(json_t *parent, const char *namestart, const char *nameend, const char *bodystart, const char *bodyend)
{
	json_t *j;
	qboolean dupbody = false;
	if (namestart && !nameend)
		nameend = namestart+strlen(namestart);
	if (bodystart && !bodyend)
	{
		dupbody = true;
		bodyend = bodystart+strlen(bodystart);
	}
	j = malloc(sizeof(*j) + nameend-namestart + (dupbody?1+bodyend-bodystart:0));
	memcpy(j->name, namestart, nameend-namestart);
	j->name[nameend-namestart] = 0;
	j->bodystart = bodystart;
	j->bodyend = bodyend;

	j->child = NULL;
	j->sibling = NULL;
	j->childlink = &j->child;
	j->parent = parent;
	if (parent)
	{
		*parent->childlink = j;
		parent->childlink = &j->sibling;
		j->used = false;
	}
	else
		j->used = true;

	if (dupbody)
	{
		char *bod = j->name + (nameend-namestart)+1;
		j->bodystart = bod;
		j->bodyend = j->bodystart + (bodyend-bodystart);
		memcpy(bod, bodystart, bodyend-bodystart);
		bod[bodyend-bodystart] = 0;
	}
	return j;
}

//node parsing
static void JSON_SkipWhite(const char *msg, int *pos, int max)
{
	while (*pos < max)
	{
		//if its simple whitespace then keep skipping over it
		if (msg[*pos] == ' ' ||
			msg[*pos] == '\t' ||
			msg[*pos] == '\r' ||
			msg[*pos] == '\n' )
		{
			*pos+=1;
			continue;
		}

		//BEGIN NON-STANDARD - Note that comments are NOT part of json, but people insist on using them anyway (c-style, like javascript).
		else if (msg[*pos] == '/' && *pos+1 < max)
		{
			if (msg[*pos+1] == '/')
			{	//C++ style single-line comments that continue till the next line break
				*pos+=2;
				while (*pos < max)
				{
					if (msg[*pos] == '\r' || msg[*pos] == '\n')
						break;	//ends on first line break (the break is then whitespace will will be skipped naturally)
					*pos+=1;	//not yet
				}
				continue;
			}
			else if (msg[*pos+1] == '*')
			{	/*C style multi-line comment*/
				*pos+=2;
				while (*pos+1 < max)
				{
					if (msg[*pos] == '*' && msg[*pos+1] == '/')
					{
						*pos+=2;	//skip past the terminator ready for whitespace or trailing comments directly after
						break;
					}
					*pos+=1;	//not yet
				}
				continue;
			}
		}
		//END NON-STANDARD
		break;	//not whitespace/comment/etc.
	}
}
//writes the body to a null-terminated string, handling escapes as needed.
//returns required body length (without terminator) (NOTE: return value is not escape-aware, so this is an over-estimate).
static size_t JSON_ReadBody(json_t *t, char *out, size_t outsize)
{
//	size_t bodysize;
	if (!t)
	{
		if (out)
			*out = 0;
		return 0;
	}
	if (out && outsize)
	{
		char *outend = out+outsize-1;	//compensate for null terminator
		const char *in = t->bodystart;
		while (in < t->bodyend && out < outend)
		{
			if (*in == '\\')
			{
				if (++in < t->bodyend)
				{
					switch(*in++)
					{
					case '\"':	*out++ = '\"'; break;
					case '\\':	*out++ = '\\'; break;
					case '/':	*out++ =  '/'; break;	//json is not C...
					case 'b':	*out++ = '\b'; break;
					case 'f':	*out++ = '\f'; break;
					case 'n':	*out++ = '\n'; break;
					case 'r':	*out++ = '\r'; break;
					case 't':	*out++ = '\t'; break;
//					case 'u':
//						out += utf8_encode(out, code, outend-out);
//						break;
					default:
						//unknown escape. will warn when actually reading it.
						*out++ = '\\';
						if (out < outend)
							*out++ = in[-1];
						break;
					}
				}
				else
					*out++ = '\\';	//error...
			}
			else
				*out++ = *in++;
		}
		*out = 0;
	}
	return t->bodyend-t->bodystart;
}

static qboolean JSON_ParseString(char const*msg, int *pos, int max, char const**start, char const** end)
{
	if (*pos < max && msg[*pos] == '\"')
	{	//quoted string
		//FIXME: no handling of backslash followed by one of "\/bfnrtu
		*pos+=1;
		*start = msg+*pos;
		while (*pos < max)
		{
			if (msg[*pos] == '\"')
				break;
			if (msg[*pos] == '\\')
			{	//escapes are expanded elsewhere, we're just skipping over them here.
				switch(msg[*pos+1])
				{
				case '\"':
				case '\\':
				case '/':
				case 'b':
				case 'f':
				case 'n':
				case 'r':
				case 't':
					*pos+=2;
					break;
				case 'u':
					*pos+=2;
					//*pos+=4; //4 hex digits, not escapes so just wait till later before parsing them properly.
					break;
				default:
					//unknown escape. will warn when actually reading it.
					*pos+=1;
					break;
				}
			}
			else
				*pos+=1;
		}
		if (*pos < max && msg[*pos] == '\"')
		{
			*end = msg+*pos;
			*pos+=1;
			return true;
		}
	}
	else
	{	//name
		*start = msg+*pos;
		while (*pos < max
			&& msg[*pos] != ' '
			&& msg[*pos] != '\t'
			&& msg[*pos] != '\r'
			&& msg[*pos] != '\n'
			&& msg[*pos] != ':'
			&& msg[*pos] != ','
			&& msg[*pos] != '}'
			&& msg[*pos] != '{'
			&& msg[*pos] != '['
			&& msg[*pos] != ']')
		{
			*pos+=1;
		}
		*end = msg+*pos;
		if (*start != *end)
			return true;
	}
	*end = *start;
	return false;
}
static json_t *JSON_Parse(json_t *t, const char *namestart, const char *nameend, const char *json, int *jsonpos, int jsonlen)
{
	const char *childstart, *childend;
	JSON_SkipWhite(json, jsonpos, jsonlen);

	if (*jsonpos < jsonlen)
	{
		if (json[*jsonpos] == '{')
		{
			*jsonpos+=1;
			JSON_SkipWhite(json, jsonpos, jsonlen);

			t = JSON_CreateNode(t, namestart, nameend, NULL, NULL);

			while (*jsonpos < jsonlen && json[*jsonpos] == '\"')
			{
				if (!JSON_ParseString(json, jsonpos, jsonlen, &childstart, &childend))
					break;
				JSON_SkipWhite(json, jsonpos, jsonlen);
				if (*jsonpos < jsonlen && json[*jsonpos] == ':')
				{
					*jsonpos+=1;
					if (!JSON_Parse(t, childstart, childend, json, jsonpos, jsonlen))
						break;
				}
				JSON_SkipWhite(json, jsonpos, jsonlen);

				if (*jsonpos < jsonlen && json[*jsonpos] == ',')
				{
					*jsonpos+=1;
					JSON_SkipWhite(json, jsonpos, jsonlen);
					continue;
				}
				break;
			}

			if (*jsonpos < jsonlen && json[*jsonpos] == '}')
			{
				*jsonpos+=1;
				return t;
			}
			JSON_Destroy(t);
		}
		else if (json[*jsonpos] == '[')
		{
			char idxname[MAX_QPATH];
			unsigned int idx = 0;
			*jsonpos+=1;
			JSON_SkipWhite(json, jsonpos, jsonlen);

			t = JSON_CreateNode(t, namestart, nameend, NULL, NULL);

			for(;;)
			{
				Q_snprintf(idxname, sizeof(idxname), "%u", idx++);
				if (!JSON_Parse(t, idxname, NULL, json, jsonpos, jsonlen))
					break;

				if (*jsonpos < jsonlen && json[*jsonpos] == ',')
				{
					*jsonpos+=1;
					JSON_SkipWhite(json, jsonpos, jsonlen);
					continue;
				}
				break;
			}

			JSON_SkipWhite(json, jsonpos, jsonlen);
			if (*jsonpos < jsonlen && json[*jsonpos] == ']')
			{
				*jsonpos+=1;
				return t;
			}
			JSON_Destroy(t);
		}
		else
		{
			if (JSON_ParseString(json, jsonpos, jsonlen, &childstart, &childend))
				return JSON_CreateNode(t, namestart, nameend, childstart, childend);
		}
	}
	return NULL;
}

//we don't understand arrays here (we just treat them as tables) so eg "foo.0.bar" to find t->foo[0]->bar
static json_t *JSON_FindChild(json_t *t, const char *child)
{
	if (t)
	{
		size_t nl;
		const char *dot = strchr(child, '.');
		if (dot)
			nl = dot-child;
		else
			nl = strlen(child);
		for (t = t->child; t; t = t->sibling)
		{
			if (!strncmp(t->name, child, nl) && (t->name[nl] == '.' || !t->name[nl]))
			{
				child+=nl;
				t->used = true;
				if (*child == '.')
					return JSON_FindChild(t, child+1);
				if (!*child)
					return t;
				break;
			}
		}
	}
	return NULL;
}
static json_t *JSON_FindIndexedChild(json_t *t, const char *child, unsigned int idx)
{
	char idxname[MAX_QPATH];
	if (child)
		Q_snprintf(idxname, sizeof(idxname), "%s.%u", child, idx);
	else
		Q_snprintf(idxname, sizeof(idxname), "%u", idx);
	return JSON_FindChild(t, idxname);
}
static qboolean JSON_Equals(json_t *t, const char *child, const char *expected)
{
	if (child)
		t = JSON_FindChild(t, child);
	if (t && t->bodyend-t->bodystart == strlen(expected))
		return !strncmp(t->bodystart, expected, t->bodyend-t->bodystart);
	return false;
}
#include <inttypes.h>
static quintptr_t JSON_GetUInteger(json_t *t, const char *child, unsigned int fallback)
{
	if (child)
		t = JSON_FindChild(t, child);
	if (t)
	{	//copy it to another buffer. can probably skip that tbh.
		char tmp[MAX_QPATH];
		char *trail;
		size_t l = t->bodyend-t->bodystart;
		quintptr_t r;
		if (l > MAX_QPATH-1)
			l = MAX_QPATH-1;
		memcpy(tmp, t->bodystart, l);
		tmp[l] = 0;
		if (!strcmp(tmp, "false"))	//special cases, for booleans
			return 0u;
		if (!strcmp(tmp, "true"))	//special cases, for booleans
			return 1u;
		r = (quintptr_t)strtoull(tmp, &trail, 0);
		if (!*trail)
			return r;
	}
	return fallback;
}
static qintptr_t JSON_GetInteger(json_t *t, const char *child, int fallback)
{
	if (child)
		t = JSON_FindChild(t, child);
	if (t)
	{	//copy it to another buffer. can probably skip that tbh.
		char tmp[MAX_QPATH];
		char *trail;
		size_t l = t->bodyend-t->bodystart;
		qintptr_t r;
		if (l > MAX_QPATH-1)
			l = MAX_QPATH-1;
		memcpy(tmp, t->bodystart, l);
		tmp[l] = 0;
		if (!strcmp(tmp, "false"))	//special cases, for booleans
			return 0;
		if (!strcmp(tmp, "true"))	//special cases, for booleans
			return 1;
		r = (qintptr_t)strtoll(tmp, &trail, 0);
		if (!*trail)
			return r;
	}
	return fallback;
}
#ifndef SERVERONLY//ffs
static qintptr_t JSON_GetIndexedInteger(json_t *t, unsigned int idx, int fallback)
{
	char idxname[MAX_QPATH];
	Q_snprintf(idxname, sizeof(idxname), "%u", idx);
	return JSON_GetInteger(t, idxname, fallback);
}
#endif
static double JSON_GetFloat(json_t *t, const char *child, double fallback)
{
	if (child)
		t = JSON_FindChild(t, child);
	if (t)
	{	//copy it to another buffer. can probably skip that tbh.
		char tmp[MAX_QPATH];
		size_t l = t->bodyend-t->bodystart;
		if (l > MAX_QPATH-1)
			l = MAX_QPATH-1;
		memcpy(tmp, t->bodystart, l);
		tmp[l] = 0;
		return atof(tmp);
	}
	return fallback;
}
static double JSON_GetIndexedFloat(json_t *t, unsigned int idx, double fallback)
{
	char idxname[MAX_QPATH];
	Q_snprintf(idxname, sizeof(idxname), "%u", idx);
	return JSON_GetFloat(t, idxname, fallback);
}
static const char *JSON_GetString(json_t *t, const char *child, char *buffer, size_t buffersize, const char *fallback)
{
	if (child)
		t = JSON_FindChild(t, child);
	if (t)
	{	//copy it to another buffer. can probably skip that tbh.
		JSON_ReadBody(t, buffer, buffersize);
		return buffer;
	}
	return fallback;
}

static void JSON_GetPath(json_t *t, qboolean ignoreroot, char *buffer, size_t buffersize)
{
	if (t->parent && (t->parent->parent || !ignoreroot))
	{
		JSON_GetPath(t->parent, ignoreroot, buffer, buffersize);
		Q_strlcat(buffer, ".", buffersize);
	}
	Q_strlcat(buffer, t->name, buffersize);
}
static void JSON_WarnUnused(json_t *t, int *warnlimit)
{
	if (!t)
		return;
	if (t->used)
	{
		for (t = t->child; t; t = t->sibling)
			JSON_WarnUnused(t, warnlimit);
	}
	else
	{
		char path[8192];
		*path = 0;
		JSON_GetPath(t, false, path, sizeof(path));
		if ((*warnlimit) --> 0)
			Con_DPrintf(CON_WARNING"GLTF property %s was not used\n", path);
	}
}
static void JSON_FlagAsUsed(json_t *t, const char *child)
{
	if (child)
	{
		t = JSON_FindChild(t, child);
		if (!t)
			return;
	}
	t->used = true;
	for (t = t->child; t; t = t->sibling)
		JSON_FlagAsUsed(t, NULL);
}
static void JSON_WarnIfChild(json_t *t, const char *child, int *warnlimit)
{
	t = JSON_FindChild(t, child);
	if (t)
	{
		char path[8192];
		*path = 0;
		JSON_GetPath(t, false, path, sizeof(path));
		if ((*warnlimit) --> 0)
			Con_Printf(CON_WARNING"Standard feature %s is not supported\n", path);
		JSON_FlagAsUsed(t, NULL);
	}
}

static unsigned int FromBase64(char c)
{
	if (c >= 'A' && c <= 'Z')
		return 0+(c-'A');
	if (c >= 'a' && c <= 'z')
		return 26+(c-'a');
	if (c >= '0' && c <= '9')
		return 52+(c-'0');
	if (c == '+')
		return 62;
	if (c == '/')
		return 63;
	return 64;	//'=' for no-more/padding.
}
//fancy parsing of content. NOTE: doesn't bother to handle escape codes, which shouldn't be present (\u for ascii chars is horribly wasteful).
static void *JSON_MallocDataURI(json_t *t, size_t *outlen)
{
	size_t bl = t->bodyend-t->bodystart;
	if (bl >= 5 && !strncmp(t->bodystart, "data:", 5))
	{
		const char *mimestart = t->bodystart+5;
		const char *mimeend;
		const char *encstart;
		const char *encend;
		const char *in;
		char *out, *r;

		for (mimeend = mimestart; *mimeend && mimeend < t->bodyend; mimeend++)
		{
			if (*mimeend == ';')	//start of encoding
				break;
			if (*mimeend == ',')	//start of data
				break;
		}
		if (*mimeend == ';')
		{
			for (encend = encstart = mimeend+1; *encend && encend < t->bodyend; encend++)
			{
				if (*encend == ',')	//start of data
					break;
			}
		}
		else
			encstart = encend = mimeend;

		if (*encend == ',' && encend < t->bodyend)
		{
			in = encend+1;
			if (encend-encstart == 6 && !strncmp(encstart, "base64", 6))
			{
				//base64
				r = out = malloc(((t->bodyend-in)*3)/4 + 1);
				while (in+3 < t->bodyend)
				{
					unsigned int c1, c2, c3, c4;
					c1 = FromBase64(*in++);
					c2 = FromBase64(*in++);
					if (c1 >= 64 || c2 >= 64)
						break;
					*out++ = (c1<<2) | (c2>>4);
					c3 = FromBase64(*in++);
					if (c3 >= 64)
						break;
					*out++ = (c2<<4) | (c3>>2);
					c4 = FromBase64(*in++);
					if (c4 >= 64)
						break;
					*out++ = (c3<<6) | (c4>>0);
				}
				*outlen = out-r;
				*out = 0;
				return r;
			}
			else if (encend == encstart)
			{	//url encoding. yuck, sod off.
			}
		}
	}
	return NULL;
}







//glTF 1.0 and 2.0 differ in that 1 uses names and 2 uses indexes. There's also some significant differences with materials.
//we only support 2.0

//buffers are raw blobs that can come from multiple different sources
struct gltf_buffer
{
	qboolean loaded;
	qboolean malloced;
	void *data;
	size_t length;
};
struct galiasbone_gltf_s
{	//stored in galiasinfo_t->ctx
	double rmatrix[16];			//gah
	double quat[4], scale[3], trans[3];	//annoying smeg
};
typedef struct gltf_s
{
	struct model_s *mod;
	unsigned int numsurfaces;
	json_t *r;
	int ver;

	int *bonemap;//[MAX_BONES];	//remap skinned bones. I hate that we have to do this.
	struct gltfbone_s
	{
		char name[32];
		char jointname[32];	//gltf1 only
		int parent;
		int camera;
		double amatrix[16];
		double inverse[16];
		struct galiasbone_gltf_s rel;

		struct {
			struct gltf_accessor *input;
			struct gltf_accessor *output;
		} *rot, *scale, *translation;
	} *bones;//[MAX_BONES];
	unsigned int numbones;

	int warnlimit;	//don't spam warnings. this is a loader, not a spammer

	struct gltf_buffer buffers[64];
} gltf_t;

static void GLTF_FlagExtras(json_t *node)
{
	JSON_FindChild(node, "extensions");	//warn about child extensions, but not the extensions table itself.
	JSON_FlagAsUsed(node, "extras");	//don't warn about application-specific extras
}

static json_t *GLTF_FindJSONIDParent(struct gltf_s *gltf, json_t *parent, json_t *id, quintptr_t *idx)
{
	if (gltf->ver == 1)
	{	//gltf1 uses string-based names
		char name[64];
		JSON_ReadBody(id, name, sizeof(name));
		id = parent;
		if (idx)
			*idx = 0;
		if (id)
		for (id = id->child; id; id = id->sibling)
		{
			if (!strcmp(id->name, name))
			{
				id->used = true;
				return id;
			}
			if (idx)
				*idx += 1;
		}
		return NULL;
	}
	else
	{	//gltf2 uses array indexes.
		quintptr_t num;
		num = id?JSON_GetInteger(id, NULL, -1):-1;
		if (idx)
			*idx = num;
		return JSON_FindIndexedChild(parent, NULL, num);
	}
}
static json_t *GLTF_FindJSONID(struct gltf_s *gltf, const char *restype, json_t *id, quintptr_t *idx)
{	//if there's no scene info, treat it as "-1"
	return GLTF_FindJSONIDParent(gltf, JSON_FindChild(gltf->r, restype), id, idx);
}
static json_t *GLTF_FindJSONID_First(struct gltf_s *gltf, const char *restype, json_t *id, quintptr_t *idx)
{	//if there's no scene info, treat it as "0"

	if (!id && gltf->ver > 1)
		return JSON_FindIndexedChild(gltf->r, restype, 0);
	return GLTF_FindJSONIDParent(gltf, JSON_FindChild(gltf->r, restype), id, idx);
}

static void GLTF_RelativePath(const char *base, const char *relative, char *out, size_t outsize)
{
	size_t t;
	const char *sep;
	const char *end = base;
	if (*relative == '/')
	{
		relative++;
	}
	else
	{
		for (sep = end; *sep; sep++)
		{
			if (*sep == '/' || *sep == '\\')
				end = sep+1;
		}
	}
	while (!strncmp(relative, "../", 3))
	{
		if (end > base)
		{
			end--;
			while (end > base)
			{
				end--;
				if (*end == '/' || *end == '\\')
				{
					relative += 3;
					end++;
					break;
				}
			}
		}
		else
			break;
	}
	outsize--;	//for the null

	t = end-base;
	if (t > outsize)
		t = outsize;
	memcpy(out, base, t);
	out += t;
	outsize -= t;

	//FIXME: uris should be percent-decoded here.
	t = strlen(relative);
	if (t > outsize)
		t = outsize;
	memcpy(out, relative, t);
	out += t;
	outsize -= t;

	*out = 0;
}

static struct gltf_buffer *GLTF_GetBufferData(gltf_t *gltf, json_t *bufferid)
{
	quintptr_t bufferidx;
	json_t *b = GLTF_FindJSONID(gltf, "buffers", bufferid, &bufferidx);
	json_t *uri = JSON_FindChild(b, "uri");
	size_t length = JSON_GetUInteger(b, "byteLength", 0);
	struct gltf_buffer *out;

	if (gltf->ver <= 1)
	{
		char body[64];
		JSON_ReadBody(bufferid, body, sizeof(body));
		if (!strcmp(body, "binary_glTF") && !gltf->buffers[0].malloced && gltf->buffers[0].data)
			return &gltf->buffers[0];
		else
			bufferidx++;

		JSON_FlagAsUsed(b, "type");	//default: "arraybuffer"... yeah, not relevant to anything.
	}
	JSON_FlagAsUsed(b, "name");
	GLTF_FlagExtras(b);


	if (bufferidx >= countof(gltf->buffers))
		return NULL;
	out = &gltf->buffers[bufferidx];

	//we may have been through here before...
	if (out->loaded)
		return out->data?out:NULL;
	out->loaded = true;

	if (uri)
	{
		out->malloced = true;
		out->data = JSON_MallocDataURI(uri, &out->length);
		if (!out->data)
		{
			//read a file from disk.
			vfsfile_t *f;
			char uritext[MAX_QPATH];
			char filename[MAX_QPATH];
			JSON_ReadBody(uri, uritext, sizeof(uritext));
			GLTF_RelativePath(gltf->mod->name, uritext, filename, sizeof(filename));
			f = filefuncs->OpenVFS(filename, "rb", FS_GAME);
			if (f)
			{
				out->length = VFS_GETLEN(f);
				out->length = min(out->length, length);
				out->data = malloc(length);
				VFS_READ(f, out->data, length);
				VFS_CLOSE(f);
			}
			else
				Con_Printf(CON_WARNING"%s: Unable to read buffer file %s\n", gltf->mod->name, filename);
		}
	}
	return out->data?out:NULL;
}
//buffer views are aka VBOs. each has its own VBO data type (vbo/ebo), and can be uploaded as-is.
struct gltf_bufferview
{
	void *data;
	size_t length;
	int bytestride;
};
static qboolean GLTF_GetBufferViewData(gltf_t *gltf, json_t *bufferviewid, struct gltf_bufferview *view)
{
	struct gltf_buffer *buf;
	json_t *bv = GLTF_FindJSONID(gltf, "bufferViews", bufferviewid, NULL);
	size_t offset;
	if (!bv)
		return false;

	buf = GLTF_GetBufferData(gltf, JSON_FindChild(bv, "buffer"));
	if (!buf)
		return false;
	offset = JSON_GetUInteger(bv, "byteOffset", 0);
	view->data = (char*)buf->data + offset;
	view->length = JSON_GetUInteger(bv, "byteLength", 0);	//required
	view->bytestride = (gltf->ver<=1)?0:JSON_GetInteger(bv, "byteStride", 0);
	if (offset + view->length > buf->length)
		return false;

	JSON_FlagAsUsed(bv, "target");	//required, but not useful for us.
	JSON_FlagAsUsed(bv, "name");
	GLTF_FlagExtras(bv);
	return true;
}
//accessors are basically VAs blocks that refer inside a bufferview/VBO.
struct gltf_accessor
{
	void *data;
	size_t length;
	size_t bytestride;

	int componentType;		//5120 BYTE, 5121 UNSIGNED_BYTE, 5122 SHORT, 5123 UNSIGNED_SHORT, 5125 UNSIGNED_INT, 5126 FLOAT
	qboolean normalized;
	size_t count;
	int type;	//1,2,3,4 says component count, 256|(4,9,16) for square matricies...

	double mins[16];
	double maxs[16];
};
static qboolean GLTF_GetAccessor(gltf_t *gltf, json_t *accessorid, struct gltf_accessor *out)
{
	struct gltf_bufferview bv;
	json_t *a, *mins, *maxs;
	size_t offset;
	int j;
	memset(out, 0, sizeof(*out));

	a = GLTF_FindJSONID(gltf, "accessors", accessorid, NULL);
	if (!a)
		return false;

	JSON_FlagAsUsed(a, "name");

	if (!GLTF_GetBufferViewData(gltf, JSON_FindChild(a, "bufferView"), &bv))
		return false;
	offset = JSON_GetUInteger(a, "byteOffset", 0);
	if (offset > bv.length)
		return false;
	out->length = bv.length - offset;
	if (gltf->ver <= 1)
		out->bytestride = JSON_GetInteger(a, "byteStride", 0);
	else
		out->bytestride = bv.bytestride;
	out->componentType = JSON_GetInteger(a, "componentType", 0);
	out->normalized = JSON_GetInteger(a, "normalized", false);
	out->count = JSON_GetInteger(a, "count", 0);
	if (JSON_Equals(a, "type", "SCALAR"))
		out->type = (1<<8) | 1;
	else if (JSON_Equals(a, "type", "VEC2"))
		out->type = (1<<8) | 2;
	else if (JSON_Equals(a, "type", "VEC3"))
		out->type = (1<<8) | 3;
	else if (JSON_Equals(a, "type", "VEC4"))
		out->type = (1<<8) | 4;
	else if (JSON_Equals(a, "type", "MAT2"))
		out->type = (2<<8) | 2;
	else if (JSON_Equals(a, "type", "MAT3"))
		out->type = (3<<8) | 3;
	else if (JSON_Equals(a, "type", "MAT4"))
		out->type = (4<<8) | 4;
	else
	{
		if (gltf->warnlimit --> 0)
			Con_Printf(CON_WARNING"%s: glTF2 unsupported type\n", gltf->mod->name);
		out->type = 1;
	}

	if (!out->bytestride)
	{
		out->bytestride = (out->type & 0xff) * (out->type>>8);
		switch(out->componentType)
		{
		default:
			if (gltf->warnlimit --> 0)
				Con_Printf(CON_WARNING"GLTF_GetAccessor: %s: glTF2 unsupported componentType (%i)\n", gltf->mod->name, out->componentType);
		case 5120:	//BYTE
		case 5121:	//UNSIGNED_BYTE
			break;
		case 5122: //SHORT
		case 5123: //UNSIGNED_SHORT
			out->bytestride *= 2;
			break;
		case 5125: //UNSIGNED_INT
		case 5126: //FLOAT
			out->bytestride *= 4;
			break;
		}
	}


	mins = JSON_FindChild(a, "min");
	maxs = JSON_FindChild(a, "max");
	for (j = 0; j < (out->type>>8)*(out->type&0xff); j++)
	{	//'must' be set in various situations.
		out->mins[j] = JSON_GetIndexedFloat(mins, j, 0);
		out->maxs[j] = JSON_GetIndexedFloat(maxs, j, 0);
	}

//	JSON_WarnIfChild(a, "sparse");
//	JSON_WarnIfChild(a, "name");
	GLTF_FlagExtras(a);

	out->data = (char*)bv.data + offset;
	return true;
}

static void GLTF_AccessorToTangents(gltf_t *gltf, const vec3_t *norm, size_t outverts, const struct gltf_accessor *a, vec3_t *sdir, vec3_t *tdir)
{	//input MUST be a single float4
	//output is two vec3s. wasteful perhaps.
	vec3_t *os = sdir;
	vec3_t *ot = tdir;
	char *in = a->data;

	size_t v, c;
	float side;
	if ((a->type&0xff) != 4)
		return;
	switch(a->componentType)
	{
	default:
		if (gltf->warnlimit --> 0)
			Con_Printf(CON_WARNING"GLTF_AccessorToTangents: %s: glTF2 unsupported componentType (%i)\n", gltf->mod->name, a->componentType);
	case 0:
		memset(os, 0, sizeof(*os) * outverts);
		memset(ot, 0, sizeof(*ot) * outverts);
		break;
	case 5120: //BYTE	KHR_mesh_quantization (always normalized)
		for (v = 0; v < outverts; v++)
		{
			for (c = 0; c < 3; c++)
				os[v][c] = max(-1.0, ((signed char*)in)[c] / 127.0);	//negative values are larger, but we want to allow 1.0
			side = max(-1.0, ((signed char*)in)[3] / 127.0);

			//bitangent = cross(normal, tangent.xyz) * tangent.w
			ot[v][0] = (norm[v][1]*os[v][2] - norm[v][2]*os[v][1]) * side;
			ot[v][1] = (norm[v][2]*os[v][0] - norm[v][0]*os[v][2]) * side;
			ot[v][2] = (norm[v][0]*os[v][1] - norm[v][1]*os[v][0]) * side;

			in += a->bytestride;
		}
		break;
//	case 5121: //UNSIGNED_BYTE
	case 5122: //SHORT	KHR_mesh_quantization (always normalized)
		for (v = 0; v < outverts; v++)
		{
			for (c = 0; c < 3; c++)
				os[v][c] = max(-1.0, ((signed short*)in)[c] / 32767.0);
			side = max(-1.0, ((signed short*)in)[3] / 32767.0);

			//bitangent = cross(normal, tangent.xyz) * tangent.w
			ot[v][0] = (norm[v][1]*os[v][2] - norm[v][2]*os[v][1]) * side;
			ot[v][1] = (norm[v][2]*os[v][0] - norm[v][0]*os[v][2]) * side;
			ot[v][2] = (norm[v][0]*os[v][1] - norm[v][1]*os[v][0]) * side;

			in += a->bytestride;
		}
		break;
//	case 5123: //UNSIGNED_SHORT
//	case 5125: //UNSIGNED_INT
	case 5126: //FLOAT
		for (v = 0; v < outverts; v++)
		{
			for (c = 0; c < 3; c++)
				os[v][c] = ((float*)in)[c];
			side = ((float*)in)[3];

			//bitangent = cross(normal, tangent.xyz) * tangent.w
			ot[v][0] = (norm[v][1]*os[v][2] - norm[v][2]*os[v][1]) * side;
			ot[v][1] = (norm[v][2]*os[v][0] - norm[v][0]*os[v][2]) * side;
			ot[v][2] = (norm[v][0]*os[v][1] - norm[v][1]*os[v][0]) * side;

			in += a->bytestride;
		}
		break;
	}
}

static void *GLTF_AccessorToDataF(gltf_t *gltf, size_t outverts, unsigned int outcomponents, const struct gltf_accessor *a, void *out)
{
	float *ret = out, *o;
	char *in = a->data;

	size_t c, ic = a->type&0xff;
	if (ic > outcomponents)
		ic = outcomponents;
	if (!ret)
		ret = modfuncs->ZG_Malloc(&gltf->mod->memgroup, sizeof(*ret) * outcomponents * outverts);
	o = ret;
	switch(a->componentType)
	{
	default:
		if (gltf->warnlimit --> 0)
			Con_Printf(CON_WARNING"GLTF_AccessorToDataF: %s: glTF2 unsupported componentType (%i)\n", gltf->mod->name, a->componentType);
	case 0:
		memset(ret, 0, sizeof(*ret) * outcomponents * outverts);
		break;
	case 5120:	//BYTE
		if (!a->normalized)
		{	//KHR_mesh_quantization
			while(outverts --> 0)
			{
				for (c = 0; c < ic; c++)
					o[c] = ((signed char*)in)[c];
				for (; c < outcomponents; c++)
					o[c] = 0;
				o += outcomponents;
				in += a->bytestride;
			}
		}
		else
		{
			while(outverts --> 0)
			{
				for (c = 0; c < ic; c++)
					o[c] = max(-1.0, ((signed char*)in)[c] / 127.0);	//negative values are larger, but we want to allow 1.0
				for (; c < outcomponents; c++)
					o[c] = 0;
				o += outcomponents;
				in += a->bytestride;
			}
		}
		break;
	case 5121:	//UNSIGNED_BYTE
		if (!a->normalized)
		{	//KHR_mesh_quantization
			while(outverts --> 0)
			{
				for (c = 0; c < ic; c++)
					o[c] = ((unsigned char*)in)[c];
				for (; c < outcomponents; c++)
					o[c] = 0;
				o += outcomponents;
				in += a->bytestride;
			}
		}
		else
		{
			while(outverts --> 0)
			{
				for (c = 0; c < ic; c++)
					o[c] = ((unsigned char*)in)[c] / 255.0;
				for (; c < outcomponents; c++)
					o[c] = 0;
				o += outcomponents;
				in += a->bytestride;
			}
		}
		break;
	case 5122: //SHORT
		if (!a->normalized)
		{	//KHR_mesh_quantization
			while(outverts --> 0)
			{
				for (c = 0; c < ic; c++)
					o[c] = ((signed short*)in)[c];
				for (; c < outcomponents; c++)
					o[c] = 0;
				o += outcomponents;
				in += a->bytestride;
			}
		}
		else
		{
			while(outverts --> 0)
			{
				for (c = 0; c < ic; c++)
					o[c] = max(-1.0, ((signed short*)in)[c] / 32767.0);	//negative values are larger, but we want to allow 1.0
				for (; c < outcomponents; c++)
					o[c] = 0;
				o += outcomponents;
				in += a->bytestride;
			}
		}
		break;
	case 5123: //UNSIGNED_SHORT
		if (!a->normalized)
		{	//KHR_mesh_quantization
			while(outverts --> 0)
			{
				for (c = 0; c < ic; c++)
					o[c] = ((unsigned short*)in)[c];
				for (; c < outcomponents; c++)
					o[c] = 0;
				o += outcomponents;
				in += a->bytestride;
			}
		}
		else
		{
			while(outverts --> 0)
			{
				for (c = 0; c < ic; c++)
					o[c] = ((unsigned short*)in)[c] / 65535.0;
				for (; c < outcomponents; c++)
					o[c] = 0;
				o += outcomponents;
				in += a->bytestride;
			}
		}
		break;
	case 5125: //UNSIGNED_INT
		if (!a->normalized)
		{	//?!?!?!?!
			while(outverts --> 0)
			{
				for (c = 0; c < ic; c++)
					o[c] = ((unsigned int*)in)[c];
				for (; c < outcomponents; c++)
					o[c] = 0;
				o += outcomponents;
				in += a->bytestride;
			}
		}
		else
		{
			while(outverts --> 0)
			{
				for (c = 0; c < ic; c++)
					o[c] = ((unsigned int*)in)[c] / (double)~0u;	//stupid format to use. will be lossy.
				for (; c < outcomponents; c++)
					o[c] = 0;
				o += outcomponents;
				in += a->bytestride;
			}
		}
		break;
	case 5126: //FLOAT
		while(outverts --> 0)
		{
			for (c = 0; c < ic; c++)
				o[c] = ((float*)in)[c];
			for (; c < outcomponents; c++)
				o[c] = 0;
			o += outcomponents;
			in += a->bytestride;
		}
		break;
	}
	return ret;
}
static void *GLTF_AccessorToDataUB(gltf_t *gltf, size_t outverts, unsigned int outcomponents, struct gltf_accessor *a)
{	//only used for colour, with fallback to float, so only UNSIGNED_BYTE needs to work.
	unsigned char *ret = modfuncs->ZG_Malloc(&gltf->mod->memgroup, sizeof(*ret) * outcomponents * outverts), *o;
	char *in = a->data;

	size_t c, ic = a->type&0xff;
	if (ic > outcomponents)
		ic = outcomponents;
	o = ret;
	switch(a->componentType)
	{
	default:
		if (gltf->warnlimit --> 0)
			Con_Printf(CON_WARNING"GLTF_AccessorToDataUB: %s: glTF2 unsupported componentType (%i)\n", gltf->mod->name, a->componentType);
	case 0:
		memset(ret, 0, sizeof(*ret) * outcomponents * outverts);
		break;
//	case 5120:	//BYTE
	case 5121:	//UNSIGNED_BYTE
		while(outverts --> 0)
		{
			for (c = 0; c < ic; c++)
				o[c] = ((unsigned char*)in)[c];
			for (; c < outcomponents; c++)
				o[c] = 0;
			o += outcomponents;
			in += a->bytestride;
		}
		break;
//	case 5122: //SHORT
//	case 5123: //UNSIGNED_SHORT
//	case 5125: //UNSIGNED_INT
/*	case 5126: //FLOAT
		while(outverts --> 0)
		{
			for (c = 0; c < ic; c++)
				o[c] = ((float*)in)[c];
			for (; c < outcomponents; c++)
				o[c] = 0;
			o += outcomponents;
			in += a->bytestride;
		}
		break;*/
	}
	return ret;
}
static void *GLTF_AccessorToDataBone(gltf_t *gltf, size_t outverts, struct gltf_accessor *a)
{	//input should only be ubytes||ushorts.
	const unsigned int outcomponents = 4;
	boneidx_t *ret = modfuncs->ZG_Malloc(&gltf->mod->memgroup, sizeof(*ret) * outcomponents * outverts), *o;
	char *in = a->data;


	size_t c, ic = a->type&0xff;
	if (ic > outcomponents)
		ic = outcomponents;
	o = ret;
	if (a->normalized)
		if (gltf->warnlimit --> 0)
			Con_Printf(CON_WARNING"GLTF_AccessorToDataBone: %s: normalised input\n", gltf->mod->name);
	switch(a->componentType)
	{
	default:
		if (gltf->warnlimit --> 0)
			Con_Printf(CON_WARNING"GLTF_AccessorToDataBone: %s: glTF2 unsupported componentType (%i)\n", gltf->mod->name, a->componentType);
	case 0:
		memset(ret, 0, sizeof(*ret) * outcomponents * outverts);
		break;
	case 5120:	//BYTE - should not be negative, so ignore sign bit
	case 5121:	//UNSIGNED_BYTE
		while(outverts --> 0)
		{
			unsigned char v;
			for (c = 0; c < ic; c++)
			{
				v = ((unsigned char*)in)[c];
				if ((unsigned int)v >= MAX_BONES)
					v = 0;
				o[c] = gltf->bonemap[v];
			}
			for (; c < outcomponents; c++)
				o[c] = gltf->bonemap[0];
			o += outcomponents;
			in += a->bytestride;
		}
		break;
	case 5122: //SHORT - should not be negative, so ignore sign bit
	case 5123: //UNSIGNED_SHORT
		while(outverts --> 0)
		{
			unsigned short v;
			for (c = 0; c < ic; c++)
			{
				v = ((unsigned short*)in)[c];
				if (v >= MAX_BONES)
					v = 0;
				o[c] = gltf->bonemap[v];
			}
			for (; c < outcomponents; c++)
				o[c] = gltf->bonemap[0];
			o += outcomponents;
			in += a->bytestride;
		}
		break;
		//the spec doesn't require these.
	case 5125: //UNSIGNED_INT
		while(outverts --> 0)
		{
			unsigned int v;
			for (c = 0; c < ic; c++)
			{
				v = ((unsigned short*)in)[c];
				if (v >= MAX_BONES)
					v = 0;
				o[c] = gltf->bonemap[v];
			}
			for (; c < outcomponents; c++)
				o[c] = gltf->bonemap[0];
			o += outcomponents;
			in += a->bytestride;
		}
		break;
	case 5126: //FLOAT. for bone indexes. wtf?
		while(outverts --> 0)
		{
			unsigned int v;
			for (c = 0; c < ic; c++)
			{
				v = ((float*)in)[c];
				if (v >= MAX_BONES)
					v = 0;
				o[c] = gltf->bonemap[v];
			}
			for (; c < outcomponents; c++)
				o[c] = gltf->bonemap[0];
			o += outcomponents;
			in += a->bytestride;
		}
		break;
	}
	return ret;
}
static void TransformPosArray(vecV_t *data, size_t vcount, double matrix[])
{
	while (vcount --> 0)
	{
		vec3_t t;
		VectorCopy((*data), t);

		(*data)[0] = matrix[0]*t[0] + matrix[4]*t[1] + matrix[8]*t[2] + matrix[12];
		(*data)[1] = matrix[1]*t[0] + matrix[5]*t[1] + matrix[9]*t[2] + matrix[13];
		(*data)[2] = matrix[2]*t[0] + matrix[6]*t[1] + matrix[10]*t[2] + matrix[14];
		//1        = matrix[3]*t[0] + matrix[7]*t[1] + matrix[11]*t[2] + matrix[14];	//hopefully...
		data++;
	}
}
static void TransformDirArray(vec3_t *data, size_t vcount, double matrix[])
{
	vec3_t t;
	float mag;
	while (vcount --> 0)
	{
		t[0] = matrix[0]*(*data)[0] + matrix[4]*(*data)[1] + matrix[8]*(*data)[2];
		t[1] = matrix[1]*(*data)[0] + matrix[5]*(*data)[1] + matrix[9]*(*data)[2];
		t[2] = matrix[2]*(*data)[0] + matrix[6]*(*data)[1] + matrix[10]*(*data)[2];

		//scaling is bad for axis.
		mag = DotProduct(t,t);
		if (mag)
		{
			mag = 1/sqrt(mag);
			VectorScale(t, mag, t);
		}

		VectorCopy(t, (*data));
		data++;
	}
}
#ifndef SERVERONLY
static texid_t GLTF_LoadImage(gltf_t *gltf, json_t *imageid, unsigned int flags)
{
	size_t size;
	texid_t ret = r_nulltex;
	json_t *image			= GLTF_FindJSONID(gltf, "images", imageid, NULL);
	json_t *uri				= JSON_FindChild(image, "uri");
	json_t *mimeType		= JSON_FindChild(image, "mimeType");
	json_t *bufferViewid	= JSON_FindChild(image, "bufferView");
	char uritext[MAX_QPATH];
	char filename[MAX_QPATH];
	void *mem;
	struct gltf_bufferview view;

	JSON_FlagAsUsed(image, "name");

	if (gltf->ver <= 1)
	{
		json_t *binary_glTF	= JSON_FindChild(image, "extensions.KHR_binary_glTF");
		if (binary_glTF)
		{
			bufferViewid = JSON_FindChild(binary_glTF, "bufferView");
			mimeType = JSON_FindChild(binary_glTF, "mimeType");
			JSON_FlagAsUsed(binary_glTF, "width");
			JSON_FlagAsUsed(binary_glTF, "height");
			uri = NULL;
		}
	}

	//potentially valid mime types:
	//image/png
	//image/vnd-ms.dds (MSFT_texture_dds)
	(void)mimeType;

	*uritext = 0;
	if (uri)
	{
		mem = JSON_MallocDataURI(uri, &size);
		if (mem)
		{
			JSON_GetPath(image, false, uritext, sizeof(uritext));
			ret = modfuncs->GetTexture(uritext, NULL, flags, mem, NULL, size, 0, TF_INVALID);
			free(mem);
		}
		else
		{
			JSON_ReadBody(uri, uritext, sizeof(uritext));
			GLTF_RelativePath(gltf->mod->name, uritext, filename, sizeof(filename));
			ret = modfuncs->GetTexture(filename, NULL, flags, NULL, NULL, 0, 0, TF_INVALID);
		}
	}
	else if (bufferViewid)
	{
		if (GLTF_GetBufferViewData(gltf, bufferViewid, &view))
		{
			JSON_GetPath(image, false, uritext, sizeof(uritext));
			ret = modfuncs->GetTexture(uritext, NULL, flags, view.data, NULL, view.length, 0, TF_INVALID);
		}
	}

	return ret;
}
static texid_t GLTF_LoadTexture(gltf_t *gltf, json_t *textureid, unsigned int flags)
{
	json_t *tex = GLTF_FindJSONID(gltf, "textures", textureid, NULL);
	json_t *sampler = GLTF_FindJSONID(gltf, "samplers", JSON_FindChild(tex, "sampler"), NULL);

	int magFilter = JSON_GetInteger(sampler, "magFilter", 0);
	int minFilter = JSON_GetInteger(sampler, "minFilter", 0);
	int wrapS = JSON_GetInteger(sampler, "wrapS", 10497);
	int wrapT = JSON_GetInteger(sampler, "wrapT", 10497);
	json_t *sourceid;

	JSON_FlagAsUsed(sampler, "name");
	JSON_FlagAsUsed(sampler, "extensions");

	switch(magFilter)
	{
	default:
		break;
	case 9728: //NEAREST
		flags |= IF_NOMIPMAP|IF_NEAREST;
		if (minFilter != 9728)
			if (gltf->warnlimit --> 0)
				Con_Printf(CON_WARNING"%s: mixed min/mag filters\n", gltf->mod->name);
		break;
	case 9986: // NEAREST_MIPMAP_LINEAR
		if (gltf->warnlimit --> 0)
			Con_Printf(CON_WARNING"%s: mixed mag/mip filters\n", gltf->mod->name);
		//fallthrough
	case 9984: // NEAREST_MIPMAP_NEAREST
		flags |= IF_NEAREST;
		if (minFilter != 9728)
			if (gltf->warnlimit --> 0)
				Con_Printf(CON_WARNING"%s: mixed min/mag filters\n", gltf->mod->name);
		break;

	case 9729: //LINEAR
		flags |= IF_NOMIPMAP|IF_LINEAR;
		if (minFilter != 9729)
			if (gltf->warnlimit --> 0)
				Con_Printf(CON_WARNING"%s: mixed min/mag filters\n", gltf->mod->name);
		break;
	case 9985: // LINEAR_MIPMAP_NEAREST
		if (gltf->warnlimit --> 0)
			Con_Printf(CON_WARNING"%s: mixed mag/mip filters\n", gltf->mod->name);
		//fallthrough
	case 9987: // LINEAR_MIPMAP_LINEAR
		flags |= IF_LINEAR;
		if (minFilter != 9729)
			if (gltf->warnlimit --> 0)
				Con_Printf(CON_WARNING"%s: mixed min/mag filters\n", gltf->mod->name);
		break;
	}
	if (wrapS == 10497 && wrapT == 10497)	//REPEAT
		;
	else if (wrapS == 33071 && wrapT == 33071)	//CLAMP_TO_EDGE
		flags |= IF_CLAMP;
	else if (wrapS == 33648 && wrapT == 33648)	//MIRRORED_REPEAT
	{
		if (gltf->warnlimit --> 0)
			Con_Printf(CON_WARNING"%s: MIRRORED_REPEAT wrap mode not supported\n", gltf->mod->name);
	}
	else
	{
		if (gltf->warnlimit --> 0)
			Con_Printf(CON_WARNING"%s: unsupported/mixed texture wrap modes %i,%i\n", gltf->mod->name, wrapS, wrapT);

		if (wrapS == 33071 || wrapT == 33071)
			flags |= IF_CLAMP;
	}

	flags |= IF_NOREPLACE;

	sourceid = JSON_FindChild(tex, "extensions.MSFT_texture_dds.source");	//load a dds instead, if one is available.
	if (!sourceid)
		sourceid = JSON_FindChild(tex, "source");	//fall back on the normal source
	return GLTF_LoadImage(gltf, sourceid, flags);
}
static char *GLTF1_LoadShader(gltf_t *gltf, json_t *shaderid)
{	//reads a vertex or fragment shader blob
	json_t *shader = GLTF_FindJSONID(gltf, "shaders", shaderid, NULL);
	json_t *uri = JSON_FindChild(shader, "uri");
	char *out = NULL;
	json_t *bufferviewid = JSON_FindChild(shader, "extensions.KHR_binary_glTF.bufferView");
	struct gltf_bufferview view;
	if (bufferviewid && GLTF_GetBufferViewData(gltf, bufferviewid, &view) && view.data && view.length)
	{
		out = malloc(view.length+1);
		memcpy(out, view.data, view.length);
		out[view.length] = 0;
	}
	else
	{
		JSON_FlagAsUsed(shader, "type");	//don't care

		if (uri)
		{
			size_t length;
			out = JSON_MallocDataURI(uri, &length);	//try and decode data schemes...
			if (!out)
			{
				//read a file from disk.
				vfsfile_t *f;
				char uritext[MAX_QPATH];
				char filename[MAX_QPATH];
				JSON_ReadBody(uri, uritext, sizeof(uritext));
				GLTF_RelativePath(gltf->mod->name, uritext, filename, sizeof(filename));
				f = filefuncs->OpenVFS(filename, "rb", FS_GAME);
				if (f)
				{
					length = VFS_GETLEN(f);
					length = min(length, length);
					out = malloc(length+1);
					out[length] = 0;
					VFS_READ(f, out, length);
					VFS_CLOSE(f);
				}
				else
					Con_Printf(CON_WARNING"%s: Unable to read buffer file %s\n", gltf->mod->name, filename);
			}
		}
	}

	//if it starts with a precision modifier then just strip that out... gl doesn't like gles's precision modifiers and we tend to try to provide our own too, which doesn't help things.
	if (out && !strncmp(out, "precision ", 10))
	{
		char *le = strchr(out, '\n');
		if (le++)
			memmove(out, le, strlen(le)+1);
	}
	return out;
}
static qboolean GLTF1_LoadMaterial(gltf_t *gltf, json_t *mat, texnums_t *texnums, char *shadertext, size_t shadertextsize)
{
	json_t *technique = GLTF_FindJSONID(gltf, "techniques", JSON_FindChild(mat, "technique"), NULL);
	json_t *parameters = JSON_FindChild(technique, "parameters");
	json_t *values = JSON_FindChild(mat, "values");
	json_t *common = JSON_FindChild(mat, "extensions.KHR_materials_common");
	json_t *v;
	json_t *p;
	char header[8192];
	char samplers[8192];
	char attributes[8192];
	char uniforms[8192];
	char *vertshader = NULL;
	char *fragshader = NULL;
	int type;
	char semantic[64];
	int sampidx = 0;
	texid_t tex;

	if (common)
	{
		json_t *values = JSON_FindChild(common, "values");
//		char techniquebuf[64];
//		const char *technique = JSON_GetString(common, "technique", techniquebuf, sizeof(techniquebuf), "");
		qboolean doubleSided = JSON_GetInteger(common, "doubleSided", false);
//		qboolean transparent = JSON_GetInteger(common, "transparent", false);
//		vec4_t ambient;
		json_t *diffusename = JSON_FindChild(values, "diffuse");
		vec4_t diffusetint = {1,1,1,1};
		json_t *emissionname = JSON_FindChild(values, "emission");
		vec4_t emissiontint = {1,1,1,1};
		json_t *specularname = JSON_FindChild(values, "emission");
		vec4_t speculartint = {1,1,1,1};
		float shininess = JSON_GetFloat(values, "shininess", 0);

		if (emissionname)
		{
			if (!emissionname->child)	//a string, so a texture id
				texnums->fullbright = GLTF_LoadTexture(gltf, emissionname, IF_NOALPHA);
			else
			{	//child nodes means its a vec4.
				emissiontint[0] = JSON_GetIndexedFloat(emissionname, 0, 0);
				emissiontint[1] = JSON_GetIndexedFloat(emissionname, 1, 0);
				emissiontint[2] = JSON_GetIndexedFloat(emissionname, 2, 0);
				emissiontint[3] = JSON_GetIndexedFloat(emissionname, 3, 1);

				if (emissiontint[0] || emissiontint[1] || emissiontint[2])
					texnums->fullbright = modfuncs->GetTexture("$whiteimage", NULL, IF_NOMIPMAP|IF_NOPICMIP|IF_NEAREST|IF_NOGAMMA, NULL, NULL, 0, 0, TF_INVALID);
			}
		}
		if (diffusename)
		{
			if (!diffusename->child)	//a string, so a texture id
				texnums->base = GLTF_LoadTexture(gltf, diffusename, 0);
			else
			{	//child nodes means its a vec4.
				texnums->base = modfuncs->GetTexture("$whiteimage", NULL, IF_NOMIPMAP|IF_NOPICMIP|IF_NEAREST|IF_NOGAMMA, NULL, NULL, 0, 0, TF_INVALID);
				diffusetint[0] = JSON_GetIndexedFloat(diffusename, 0, 0);
				diffusetint[1] = JSON_GetIndexedFloat(diffusename, 1, 0);
				diffusetint[2] = JSON_GetIndexedFloat(diffusename, 2, 0);
				diffusetint[3] = JSON_GetIndexedFloat(diffusename, 3, 1);
			}
		}
		if (specularname)
		{
			if (!specularname->child)	//a string, so a texture id
				texnums->specular = GLTF_LoadTexture(gltf, specularname, IF_NOALPHA);
			else
			{	//child nodes means its a vec4.
				texnums->specular = modfuncs->GetTexture("$whiteimage", NULL, IF_NOMIPMAP|IF_NOPICMIP|IF_NEAREST|IF_NOGAMMA, NULL, NULL, 0, 0, TF_INVALID);
				speculartint[0] = JSON_GetIndexedFloat(specularname, 0, 0);
				speculartint[1] = JSON_GetIndexedFloat(specularname, 1, 0);
				speculartint[2] = JSON_GetIndexedFloat(specularname, 2, 0);
				speculartint[3] = JSON_GetIndexedFloat(specularname, 3, 1);
			}
		}

		Q_snprintf(shadertext, shadertextsize,
			"{\n"
				"%s"//cull
				"program defaultskin#VC%s#FTE_SPECULAR_EXPONENT=%f\n"
				"{\n"
					"map $diffuse\n"
					"%s"	//blend
					"%s"	//rgbgen
				"}\n"
				"fte_basefactor %f %f %f %f\n"
				"fte_specularfactor %f %f %f %f\n"
				"fte_fullbrightfactor %f %f %f %f\n"
			"}\n",
			doubleSided?"cull disable\n":"",
			"",//alphaCutoffmodifier,
			shininess,
			"",//(alphaMode==1)?"":(alphaMode==2)?"blendfunc blend\n":"",
			"",//vertexcolours?"rgbgen vertex\nalphagen vertex\n":"",
			diffusetint[0],diffusetint[1],diffusetint[2],diffusetint[3],
			speculartint[0],speculartint[1],speculartint[2],speculartint[3],
			emissiontint[0],emissiontint[1],emissiontint[2],emissiontint[3]);
		return true;
	}

	//mat->values.diffuse tends to be quite common. make an executive descision...
	texnums->base = GLTF_LoadTexture(gltf, JSON_FindChild(values, "diffuse"), 0);
	if (mod_gltf_ignoretechniques->ival)
	{	//mat->values.diffuse tends to be quite common
		return false;
	}
	if (!technique)
	{
		//missing technique is supposed to result in a greyscale model
		Q_snprintf(shadertext, shadertextsize,
			"{\n"
				"program defaultskin\n"
				"diffusemap $whiteimage\n"
				"{\n"
					"map $diffuse\n"
					"rgbgen const 0.5 0.5 0.5\n"
					"alphagen const 1.0\n"
				"}\n"
			"}\n"
			);
		return true;
	}

	*header = 0;
	*samplers = 0;
	*attributes = 0;
	*uniforms = 0;

	//this is supposed to be glessl 100. lets try to do our best to get something compatible.
	Q_snprintfcat(header, sizeof(header), "!!ver 100 120\n");
	//and reduce conflicts with fte's normal symbols.
	Q_snprintfcat(header, sizeof(header), "!!explicit\n");

	v = JSON_FindChild(technique, "uniforms");
	if (v)
		for (v = v->child; v; v = v->sibling)
		{
			enum {
				GLTF_BYTE = 5120,
				GLTF_UNSIGNED_BYTE = 5121,
				GLTF_SHORT = 5122,
				GLTF_UNSIGNED_SHORT = 5123,
				GLTF_INT = 5124,
				GLTF_UNSIGNED_INT = 5125,
				GLTF_FLOAT = 5126,
				GLTF_FLOAT_VEC2 = 35664,
				GLTF_FLOAT_VEC3 = 35665,
				GLTF_FLOAT_VEC4 = 35666,
				GLTF_INT_VEC2 = 35667,
				GLTF_INT_VEC3 = 35668,
				GLTF_INT_VEC4 = 35669,
				GLTF_BOOL = 35670,
				GLTF_BOOL_VEC2 = 35671,
				GLTF_BOOL_VEC3 = 35672,
				GLTF_BOOL_VEC4 = 35673,
				GLTF_FLOAT_MAT2 = 35674,
				GLTF_FLOAT_MAT3 = 35675,
				GLTF_FLOAT_MAT4 = 35676,
				GLTF_SAMPLER_2D = 35678,
			};
			v->used = true;
			p = GLTF_FindJSONIDParent(gltf, parameters, v, NULL);

			if (1 != JSON_GetInteger(p, "count", 1))
			{
				if (gltf->warnlimit --> 0)
					Con_Printf(CON_WARNING"%s: Unsupported parameter->count for uniform %s\n", gltf->mod->name, v->name);
				return false;
			}
			if (JSON_GetString(p, "node", semantic, sizeof(semantic), NULL))
			{
				if (gltf->warnlimit --> 0)
					Con_Printf(CON_WARNING"%s: Unsupported parameter->node for uniform %s\n", gltf->mod->name, v->name);
				return false;
			}

			if (!JSON_GetString(p, "semantic", semantic, sizeof(semantic), NULL))
				*semantic = 0;
			type = JSON_GetInteger(p, "type", 0);

			if (!strcasecmp(semantic, "MODEL") && type == GLTF_FLOAT_MAT4)
				Q_snprintfcat(header, sizeof(header), "!!semantic %s m_model\n", v->name);
			else if (!strcasecmp(semantic, "VIEW") && type == GLTF_FLOAT_MAT4)
				Q_snprintfcat(header, sizeof(header), "!!semantic %s m_view\n", v->name);
			else if (!strcasecmp(semantic, "PROJECTION") && type == GLTF_FLOAT_MAT4)
				Q_snprintfcat(header, sizeof(header), "!!semantic %s m_projection\n", v->name);
			else if (!strcasecmp(semantic, "MODELVIEW") && type == GLTF_FLOAT_MAT4)
				Q_snprintfcat(header, sizeof(header), "!!semantic %s m_modelview\n", v->name);
			else if (!strcasecmp(semantic, "MODELVIEWPROJECTION") && type == GLTF_FLOAT_MAT4)
				Q_snprintfcat(header, sizeof(header), "!!semantic %s m_modelviewprojection\n", v->name);
			else if (!strcasecmp(semantic, "MODELINVERSE") && type == GLTF_FLOAT_MAT4)
				Q_snprintfcat(header, sizeof(header), "!!semantic %s m_invmodel\n", v->name);
			else if (!strcasecmp(semantic, "VIEWINVERSE") && type == GLTF_FLOAT_MAT4)
				Q_snprintfcat(header, sizeof(header), "!!semantic %s m_invviewprojection\n", v->name);
			else if (!strcasecmp(semantic, "PROJECTIONINVERSE") && type == GLTF_FLOAT_MAT4)
				Q_snprintfcat(header, sizeof(header), "!!semantic %s m_invprojection\n", v->name);
			else if (!strcasecmp(semantic, "MODELVIEWINVERSE") && type == GLTF_FLOAT_MAT4)
				Q_snprintfcat(header, sizeof(header), "!!semantic %s m_invmodelview\n", v->name);
			else if (!strcasecmp(semantic, "MODELVIEWPROJECTIONINVERSE") && type == GLTF_FLOAT_MAT4)
				Q_snprintfcat(header, sizeof(header), "!!semantic %s m_invmodelviewprojection\n", v->name);
			else if (!strcasecmp(semantic, "MODELINVERSETRANSPOSE") && type == GLTF_FLOAT_MAT3)
				Q_snprintfcat(header, sizeof(header), "!!semantic %s m_invmodeltranspose\n", v->name);
			else if (!strcasecmp(semantic, "MODELVIEWINVERSETRANSPOSE") && type == GLTF_FLOAT_MAT3)
				Q_snprintfcat(header, sizeof(header), "!!semantic %s m_invmodelviewtranspose\n", v->name);
//			else if (!strcasecmp(semantic, "VIEWPORT") && type == GLTF_FLOAT_VEC4)
//				Q_snprintfcat(header, sizeof(header), "!!semantic %s UNSUPPORTED\n", v->name);
			else if (!strcasecmp(semantic, "JOINTMATRIX") && type == GLTF_FLOAT_MAT4)
				Q_snprintfcat(header, sizeof(header), "!!semantic %s m_bones_mat4\n", v->name);
//			else if (!strcasecmp(semantic, "LOCAL") && type == GLTF_FLOAT_MAT4)
//				Q_snprintfcat(header, sizeof(header), "!!semantic %s UNSUPPORTED\n", v->name);
			else if (!strcasecmp(semantic, ""))
			{
				json_t *val = GLTF_FindJSONIDParent(gltf, values, v, NULL);
				switch(type)
				{
				case GLTF_SAMPLER_2D:
					Q_snprintfcat(header, sizeof(header), "!!constt %s %i\n", v->name, sampidx++);
					tex = GLTF_LoadTexture(gltf, val, 0);
					Q_snprintfcat(samplers, sizeof(samplers), "{\nmap %s\n}\n", tex?tex->ident:"$whiteimage");
					break;

				case GLTF_FLOAT:		Q_snprintfcat(header, sizeof(header), "!!const1f %s %f\n", v->name, JSON_GetFloat(val, NULL, 0));	break;
				case GLTF_FLOAT_VEC2:	Q_snprintfcat(header, sizeof(header), "!!const2f %s %f %f\n", v->name, JSON_GetIndexedFloat(val, 0, 0), JSON_GetIndexedFloat(val, 1, 0));	break;
				case GLTF_FLOAT_VEC3:	Q_snprintfcat(header, sizeof(header), "!!const3f %s %f %f %f\n", v->name, JSON_GetIndexedFloat(val, 0, 0), JSON_GetIndexedFloat(val, 1, 0), JSON_GetIndexedFloat(val, 2, 0));	break;
				case GLTF_FLOAT_VEC4:	Q_snprintfcat(header, sizeof(header), "!!const4f %s %f %f %f %f\n", v->name, JSON_GetIndexedFloat(val, 0, 0), JSON_GetIndexedFloat(val, 1, 0), JSON_GetIndexedFloat(val, 2, 0), JSON_GetIndexedFloat(val, 3, 0));	break;

				case GLTF_BOOL:			//glsl's bool/bvecN types can be ininitialised as floats or ints. lets just use ints here.
				case GLTF_BYTE:			//FIXME: it is not specified whether these are meant to be normalized or not. are they always float/vecN or int/ivecN? the spec doesn't say.
				case GLTF_SHORT:
				case GLTF_INT:			Q_snprintfcat(header, sizeof(header), "!!const1i %s %i\n", v->name, JSON_GetInteger(val, NULL, 0));	break;
				case GLTF_BOOL_VEC2:
				case GLTF_INT_VEC2:		Q_snprintfcat(header, sizeof(header), "!!const2i %s %i %i\n", v->name, JSON_GetIndexedInteger(val, 0, 0), JSON_GetIndexedInteger(val, 1, 0));	break;
				case GLTF_BOOL_VEC3:
				case GLTF_INT_VEC3:		Q_snprintfcat(header, sizeof(header), "!!const3i %s %i %i %i\n", v->name, JSON_GetIndexedInteger(val, 0, 0), JSON_GetIndexedInteger(val, 1, 0), JSON_GetIndexedInteger(val, 2, 0));	break;
				case GLTF_BOOL_VEC4:
				case GLTF_INT_VEC4:		Q_snprintfcat(header, sizeof(header), "!!const4i %s %i %i %i %i\n", v->name, JSON_GetIndexedInteger(val, 0, 0), JSON_GetIndexedInteger(val, 1, 0), JSON_GetIndexedInteger(val, 2, 0), JSON_GetIndexedInteger(val, 3, 0));	break;

				case GLTF_UNSIGNED_BYTE://FIXME: it is not specified whether these are meant to be normalized or not. are they always float/vecN or int/ivecN? the spec doesn't say.
				case GLTF_UNSIGNED_SHORT:
				case GLTF_UNSIGNED_INT:	Q_snprintfcat(header, sizeof(header), "!!const1u %s %f\n", v->name, JSON_GetFloat(val, NULL, 0));	break;
				//curiously no uvecs listed by the spec

				case GLTF_FLOAT_MAT2:	Q_snprintfcat(header, sizeof(header), "!!const2m %s %f %f %f %f\n", v->name, JSON_GetIndexedFloat(val, 0, 0), JSON_GetIndexedFloat(val, 1, 0), JSON_GetIndexedFloat(val, 2, 0), JSON_GetIndexedFloat(val, 3, 0));	break;
				case GLTF_FLOAT_MAT3:	Q_snprintfcat(header, sizeof(header), "!!const3m %s %f %f %f %f %f %f %f %f %f\n", v->name, JSON_GetIndexedFloat(val, 0, 0), JSON_GetIndexedFloat(val, 1, 0), JSON_GetIndexedFloat(val, 2, 0), JSON_GetIndexedFloat(val, 3, 0), JSON_GetIndexedFloat(val, 4, 0), JSON_GetIndexedFloat(val, 5, 0), JSON_GetIndexedFloat(val, 6, 0), JSON_GetIndexedFloat(val, 7, 0), JSON_GetIndexedFloat(val, 8, 0));	break;
				case GLTF_FLOAT_MAT4:	Q_snprintfcat(header, sizeof(header), "!!const4m %s %f %f %f %f %f %f %f %f %f %f %f %f %f %f %f %f\n", v->name, JSON_GetIndexedFloat(val, 0, 0), JSON_GetIndexedFloat(val, 1, 0), JSON_GetIndexedFloat(val, 2, 0), JSON_GetIndexedFloat(val, 3, 0), JSON_GetIndexedFloat(val, 4, 0), JSON_GetIndexedFloat(val, 5, 0), JSON_GetIndexedFloat(val, 6, 0), JSON_GetIndexedFloat(val, 7, 0), JSON_GetIndexedFloat(val, 8, 0), JSON_GetIndexedFloat(val, 9, 0), JSON_GetIndexedFloat(val, 10, 0), JSON_GetIndexedFloat(val, 11, 0), JSON_GetIndexedFloat(val, 12, 0), JSON_GetIndexedFloat(val, 13, 0), JSON_GetIndexedFloat(val, 14, 0), JSON_GetIndexedFloat(val, 15, 0));	break;

				default:
					if (gltf->warnlimit --> 0)
						Con_Printf(CON_WARNING"%s: Unsupported constant uniform type %i for uniform %s\n", gltf->mod->name, type, v->name);
					return false;
				}
			}
			else
			{
				if (gltf->warnlimit --> 0)
					Con_Printf(CON_WARNING"%s: Unknown/Unsupported semantic %s for uniform %s\n", gltf->mod->name, semantic, v->name);
				return false;
			}
		}
	v = JSON_FindChild(technique, "attributes");
	if (v)
		for (v = v->child; v; v = v->sibling)
		{
			v->used = true;
			p = GLTF_FindJSONIDParent(gltf, parameters, v, NULL);
			if (!JSON_GetString(p, "semantic", semantic, sizeof(semantic), NULL))
				*semantic = 0;
			type = JSON_GetInteger(p, "type", 0);

			if (!strcasecmp(semantic, "POSITION"))
				Q_snprintfcat(attributes, sizeof(attributes), "#define %s fte_v_position\n", v->name);
			else if (!strcasecmp(semantic, "NORMAL"))
				Q_snprintfcat(attributes, sizeof(attributes), "#define %s fte_v_normal\n", v->name);
			else if (!strcasecmp(semantic, "TEXCOORD_0"))
				Q_snprintfcat(attributes, sizeof(attributes), "#define %s fte_v_texcoord\n", v->name);
			else if (!strcasecmp(semantic, "TEXCOORD_1"))
				Q_snprintfcat(attributes, sizeof(attributes), "#define %s fte_v_lmcoord\n", v->name);
			else if (!strcasecmp(semantic, "JOINT"))
				Q_snprintfcat(attributes, sizeof(attributes), "#define %s fte_v_bone\n", v->name);
			else if (!strcasecmp(semantic, "WEIGHT"))
				Q_snprintfcat(attributes, sizeof(attributes), "#define %s fte_v_weight\n", v->name);
			else
			{
				if (gltf->warnlimit --> 0)
					Con_Printf(CON_WARNING"%s: Unknown semantic %s for attribute %s\n", gltf->mod->name, semantic, v->name);
				return false;
			}
		}

	p = GLTF_FindJSONID(gltf, "programs", JSON_FindChild(technique, "program"), NULL);
	vertshader = GLTF1_LoadShader(gltf, JSON_FindChild(p, "vertexShader"));
	fragshader = GLTF1_LoadShader(gltf, JSON_FindChild(p, "fragmentShader"));

	Q_snprintf(shadertext, shadertextsize,
		"{\n"
			"surfaceparm nodlight\n"	//o.O
			"surfaceparm noshadows\n"	//no surprises please.
			"glslprogram\n"
			"{\n"
				"%s"	//header
				"%s"	//uniformmaps
				"#ifdef VERTEX_SHADER\n"
					"%s"	//attributemaps
					"%s\n"	//vertexshader
				"#endif\n"
				"#ifdef FRAGMENT_SHADER\n"
					"%s\n"	//fragmentshader
				"#endif\n"
			"}\n"
			"%s"	//{map foo} {map}...
		"}\n",
		header,
		uniforms,
		attributes,
		vertshader,
		fragshader,
		samplers
		);
	free(vertshader);
	free(fragshader);
	return true;
}

static galiasskin_t *GLTF_LoadMaterial(gltf_t *gltf, json_t *materialid, qboolean vertexcolours)
{
	qboolean doubleSided;
	int alphaMode;
	double alphaCutoff;
	char shader[65536];
	char alphaCutoffmodifier[128];
	quintptr_t materialidx;
	json_t *mat = GLTF_FindJSONID(gltf, "materials", materialid, &materialidx);
	galiasskin_t *ret;
	char tmp[64];
	const char *t;

	json_t *nam, *unlit, *pbrsg, *pbrmr, *blinn;

	nam = JSON_FindChild(mat, "name");
	unlit = JSON_FindChild(mat, "extensions.KHR_materials_unlit");
	pbrsg = JSON_FindChild(mat, "extensions.KHR_materials_pbrSpecularGlossiness");
	pbrmr = JSON_FindChild(mat, "pbrMetallicRoughness");
	blinn = JSON_FindChild(mat, "extensions.KHR_materials_cmnBlinnPhong");

	doubleSided = JSON_GetInteger(mat, "doubleSided", false);
	alphaCutoff = JSON_GetFloat(mat, "alphaCutoff", 0.5);
	t = JSON_GetString(mat, "alphaMode", tmp, sizeof(tmp), "OPAQUE");
	if (!strcmp(t, "MASK"))
		alphaMode = 1;
	else if (!strcmp(t, "BLEND"))
		alphaMode = 2;
	else if (!strcmp(t, "OPAQUE"))
		alphaMode = 0;
	else
	{
		alphaMode = 0;
		if (gltf->warnlimit --> 0)
			Con_Printf(CON_WARNING"%s: unsupported alphaMode: %s\n", gltf->mod->name, t);
	}

	ret = modfuncs->ZG_Malloc(&gltf->mod->memgroup, sizeof(*ret));
	ret->numframes = 1;
	ret->skinspeed = 0.1;
	ret->frame = modfuncs->ZG_Malloc(&gltf->mod->memgroup, sizeof(*ret->frame));

	{
		int skip;
		if (nam)
			JSON_ReadBody(nam, shader, sizeof(shader));
		else if (mat && *mat->name)
			Q_snprintf(shader, sizeof(shader), "%s", mat->name);
		else if (!mat)	//explicit invalid material
			Q_snprintf(shader, sizeof(shader), "");
		else
			Q_snprintf(shader, sizeof(shader), "%i", (int)materialidx);
		skip = sizeof(ret->frame->shadername)-32 - strlen(shader);
		if (skip > 0)
			skip = 0;
		if (mod_gltf_privatematerials->ival && !strchr(shader, '/'))
		{
			Q_snprintf(ret->frame->shadername, sizeof(ret->frame->shadername), "%s", gltf->mod->name-skip);
			Q_strncatz(ret->frame->shadername, "/", sizeof(ret->frame->shadername));
		}
		else
			*ret->frame->shadername = 0;

		Q_strncatz(ret->frame->shadername, shader, sizeof(ret->frame->shadername));
	}

	if (alphaMode == 1)
		Q_snprintf(alphaCutoffmodifier, sizeof(alphaCutoffmodifier), "#ALPHATEST=>%f", alphaCutoff);
	else
		*alphaCutoffmodifier = 0;

	if (gltf->ver <= 1)
	{	//fixme: break
		if (!GLTF1_LoadMaterial(gltf, mat, &ret->frame->texnums, shader, sizeof(shader)))
		{	//some lame placeholder/fallback.
			Q_snprintf(shader, sizeof(shader),
					"{\n"
						"program defaultskin\n"
					"}\n"
				);
		}
	}
	else if (unlit)
	{	//if this extension was present, then we don't get ANY lighting info.
		json_t *albedo = JSON_FindChild(pbrmr, "baseColorTexture.index");	//.rgba
		ret->frame->texnums.base     = GLTF_LoadTexture(gltf, albedo, 0);

		Q_snprintf(shader, sizeof(shader),
			"{\n"
				"surfaceparm nodlight\n"
				"%s"//cull
				"program default2d%s\n"	//fixme: there's no gpu skeletal stuff with this prog
				"{\n"
					"map $diffuse\n"
					"%s"	//blend
					"%s"	//rgbgen
				"}\n"
				"fte_basefactor %f %f %f %f\n"
			"}\n",
			doubleSided?"cull disable\n":"",
			alphaCutoffmodifier,
			(alphaMode==1)?"":(alphaMode==2)?"blendfunc blend\n":"",
			vertexcolours?"rgbgen vertex\nalphagen vertex\n":"",
			JSON_GetFloat(pbrmr, "baseColorFactor.0", 1),
				JSON_GetFloat(pbrmr, "baseColorFactor.1", 1),
				JSON_GetFloat(pbrmr, "baseColorFactor.2", 1),
				JSON_GetFloat(pbrmr, "baseColorFactor.3", 1)
			);
	}
	else if (blinn)
	{
		Con_DPrintf(CON_WARNING"%s: KHR_materials_cmnBlinnPhong implemented according to draft spec\n", gltf->mod->name);

		ret->frame->texnums.base     = GLTF_LoadTexture(gltf, JSON_FindChild(pbrsg, "diffuseTexture.index"), 0);
		ret->frame->texnums.specular = GLTF_LoadTexture(gltf, JSON_FindChild(pbrsg, "specularGlossinessTexture.index"), 0);

		//you wouldn't normally want this, but we have separate factors so lack of a texture is technically valid.
		if (!ret->frame->texnums.base)
			ret->frame->texnums.base = modfuncs->GetTexture("$whiteimage", NULL, IF_NOMIPMAP|IF_NOPICMIP|IF_NEAREST|IF_NOGAMMA, NULL, NULL, 0, 0, TF_INVALID);
		if (!ret->frame->texnums.specular)
			ret->frame->texnums.specular = modfuncs->GetTexture("$whiteimage", NULL, IF_NOMIPMAP|IF_NOPICMIP|IF_NEAREST|IF_NOGAMMA, NULL, NULL, 0, 0, TF_INVALID);

		Q_snprintf(shader, sizeof(shader),
			"{\n"
				"%s"//cull
				"program defaultskin#VC%s\n"
				"{\n"
					"map $diffuse\n"
					"%s"	//blend
					"%s"	//rgbgen
				"}\n"
				"fte_basefactor %f %f %f %f\n"
				"fte_specularfactor %f %f %f %f\n"
				"fte_fullbrightfactor %f %f %f 1.0\n"
			"}\n",
			doubleSided?"cull disable\n":"",
			alphaCutoffmodifier,
			(alphaMode==1)?"":(alphaMode==2)?"blendfunc blend\n":"",
			vertexcolours?"rgbgen vertex\nalphagen vertex\n":"",
			JSON_GetFloat(pbrsg, "diffuseFactor.0", 1),
				JSON_GetFloat(pbrsg, "diffuseFactor.1", 1),
				JSON_GetFloat(pbrsg, "diffuseFactor.2", 1),
				JSON_GetFloat(pbrsg, "diffuseFactor.3", 1),
			JSON_GetFloat(pbrsg, "specularFactor.0", 1),	//FIXME: divide by gl_specular
				JSON_GetFloat(pbrsg, "specularFactor.1", 1),
				JSON_GetFloat(pbrsg, "specularFactor.2", 1),
				JSON_GetFloat(pbrsg, "shininessFactor", 1),	//FIXME: divide by gl_specular_power
			JSON_GetFloat(mat, "emissiveFactor.0", 0),
				JSON_GetFloat(mat, "emissiveFactor.1", 0),
				JSON_GetFloat(mat, "emissiveFactor.2", 0)
			);
	}
	else if (pbrsg)
	{	//if this extension was used, then we can use rgb gloss instead of metalness stuff.
		json_t *occ = JSON_FindChild(mat, "occlusionTexture.index");	//.r
		ret->frame->texnums.base     = GLTF_LoadTexture(gltf, JSON_FindChild(pbrsg, "diffuseTexture.index"), 0);
		ret->frame->texnums.specular = GLTF_LoadTexture(gltf, JSON_FindChild(pbrsg, "specularGlossinessTexture.index"), 0);
		if (occ)
			ret->frame->texnums.occlusion = GLTF_LoadTexture(gltf, occ, IF_NOSRGB);

		Q_snprintf(shader, sizeof(shader),
			"{\n"
				"%s"//cull
				"program defaultskin#SG#VC%s%s\n"
				"{\n"
					"map $diffuse\n"
					"%s"	//blend
					"%s"	//rgbgen
				"}\n"
				"fte_basefactor %f %f %f %f\n"
				"fte_specularfactor %f %f %f %f\n"
				"fte_fullbrightfactor %f %f %f 1.0\n"
				"bemode rtlight rtlight_sg\n"
			"}\n",
			doubleSided?"cull disable\n":"",
			(occ)?"#OCCLUDE":"",
			alphaCutoffmodifier,
			(alphaMode==1)?"":(alphaMode==2)?"blendfunc blend\n":"",
			vertexcolours?"rgbgen vertex\nalphagen vertex\n":"",
			JSON_GetFloat(pbrsg, "diffuseFactor.0", 1),
				JSON_GetFloat(pbrsg, "diffuseFactor.1", 1),
				JSON_GetFloat(pbrsg, "diffuseFactor.2", 1),
				JSON_GetFloat(pbrsg, "diffuseFactor.3", 1),
			JSON_GetFloat(pbrsg, "specularFactor.0", 1),
				JSON_GetFloat(pbrsg, "specularFactor.1", 1),
				JSON_GetFloat(pbrsg, "specularFactor.2", 1),
			JSON_GetFloat(pbrsg, "glossinessFactor", 1),
			JSON_GetFloat(mat, "emissiveFactor.0", 0),
				JSON_GetFloat(mat, "emissiveFactor.1", 0),
				JSON_GetFloat(mat, "emissiveFactor.2", 0)
			);
	}
	else// if (pbrmr)
	{	//this is the standard lighting model for gltf2
		//'When not specified, all the default values of pbrMetallicRoughness apply'
		json_t *albedo = JSON_FindChild(pbrmr, "baseColorTexture.index");	//.rgba
		json_t *mrt = JSON_FindChild(pbrmr, "metallicRoughnessTexture.index");	//.r = unused, .g = roughness, .b = metalic, .a = unused
		json_t *occ = JSON_FindChild(mat, "occlusionTexture.index");	//.r
		json_t *n;
		char occname[MAX_QPATH];
		char mrtname[MAX_QPATH];

		if (JSON_GetInteger(pbrmr, "baseColorTexture.texCoord", 0) != 0)
			if (gltf->warnlimit --> 0)
				Con_Printf("%s: Unsupported baseColorTexture texCoord value\n", gltf->mod->name);
		if (JSON_GetInteger(pbrmr, "metallicRoughnessTexture.texCoord", 0) != 0)
			if (gltf->warnlimit --> 0)
				Con_Printf("%s: Unsupported metallicRoughnessTexture texCoord value\n", gltf->mod->name);
		if (JSON_GetInteger(mat, "occlusionTexture.texCoord", 0) != 0)
			if (gltf->warnlimit --> 0)
				Con_Printf("%s: Unsupported occlusionTexture texCoord value\n", gltf->mod->name);

		//now work around potential lame exporters (yay dds?).
		n = JSON_FindChild(mat, "extensions.MSFT_packing_occlusionRoughnessMetallic.occlusionRoughnessMetallicTexture.index");
		if (n)
			occ = n;
		n = JSON_FindChild(mat, "extensions.MSFT_packing_occlusionRoughnessMetallic.occlusionRoughnessMetallicTexture.index");
		if (n)
			mrt = n;

		//ideally we use the ORM.r for the occlusion map, but some people just love being annoying.
		JSON_ReadBody(occ, occname, sizeof(occname));
		JSON_ReadBody(mrt, mrtname, sizeof(mrtname));
		if (strcmp(occname,mrtname) && occ)
			ret->frame->texnums.occlusion = GLTF_LoadTexture(gltf, occ, IF_NOSRGB);

		//note: extensions.MSFT_packing_normalRoughnessMetallic.normalRoughnessMetallicTexture.index gives rg=normalxy, b=roughness, .a=metalic
		//(would still need an ao map, and probably wouldn't work well as bc3 either)

		if (albedo)
			ret->frame->texnums.base     = GLTF_LoadTexture(gltf, albedo, 0);
		if (mrt)
			ret->frame->texnums.specular = GLTF_LoadTexture(gltf, mrt, IF_NOSRGB);

		Q_snprintf(shader, sizeof(shader),
			"{\n"
				"%s"//cull
				"program defaultskin#ORM#VC%s%s\n"
				"{\n"
					"map $diffuse\n"
					"%s"	//blend
					"%s"	//rgbgen
				"}\n"
				"fte_basefactor %f %f %f %f\n"
				"fte_specularfactor %f %f %f 1.0\n"
				"fte_fullbrightfactor %f %f %f 1.0\n"
				"bemode rtlight rtlight_orm\n"
			"}\n",
			doubleSided?"cull disable\n":"",
			(!occ)?"#NOOCCLUDE":(strcmp(occname,mrtname)?"#OCCLUDE":""),
			alphaCutoffmodifier,
			(alphaMode==1)?"":(alphaMode==2)?"blendfunc blend\n":"",
			vertexcolours?"rgbgen vertex\nalphagen vertex\n":"",
			JSON_GetFloat(pbrmr, "baseColorFactor.0", 1),
				JSON_GetFloat(pbrmr, "baseColorFactor.1", 1),
				JSON_GetFloat(pbrmr, "baseColorFactor.2", 1),
				JSON_GetFloat(pbrmr, "baseColorFactor.3", 1),
			JSON_GetFloat(mat, "occlusionTexture.strength", 1),
				JSON_GetFloat(pbrmr, "metallicFactor", 1),
				JSON_GetFloat(pbrmr, "roughnessFactor", 1),
			JSON_GetFloat(mat, "emissiveFactor.0", 0),
				JSON_GetFloat(mat, "emissiveFactor.1", 0),
				JSON_GetFloat(mat, "emissiveFactor.2", 0)
			);
	}
	if (!ret->frame->texnums.bump)
		ret->frame->texnums.bump = GLTF_LoadTexture(gltf, JSON_FindChild(mat, "normalTexture.index"), IF_NOSRGB|IF_TRYBUMP);
	if (!ret->frame->texnums.fullbright)
		ret->frame->texnums.fullbright = GLTF_LoadTexture(gltf, JSON_FindChild(mat, "emissiveTexture.index"), 0);

	if (!ret->frame->texnums.base)
		ret->frame->texnums.base = modfuncs->GetTexture("$whiteimage", NULL, IF_NOMIPMAP|IF_NOPICMIP|IF_NEAREST|IF_NOGAMMA, NULL, NULL, 0, 0, TF_INVALID);

	ret->frame->defaultshader = memcpy(modfuncs->ZG_Malloc(&gltf->mod->memgroup, strlen(shader)+1), shader, strlen(shader)+1);

	Q_strlcpy(ret->name, ret->frame->shadername, sizeof(ret->name));
	return ret;
}
#endif
static const float *QDECL GLTF_AnimateMorphs(const galiasinfo_t *surf, const framestate_t *framestate);
static qboolean GLTF_ProcessMesh(gltf_t *gltf, json_t *meshid, int basebone, double skinmatrix[])
{
	model_t *mod = gltf->mod;
	quintptr_t meshidx;
	json_t *mesh = GLTF_FindJSONID(gltf, "meshes", meshid, &meshidx);
	json_t *prim;
	json_t *meshname = JSON_FindChild(mesh, "name");
	json_t *target = NULL;
	float	morphweights[MAX_MORPHWEIGHTS];
	size_t morphtargets;

	target = JSON_FindChild(mesh, "weights");
	for (morphtargets = 0; morphtargets < MAX_MORPHWEIGHTS; morphtargets++)
		morphweights[morphtargets] = JSON_GetIndexedFloat(target, morphtargets, 0);
	morphtargets = ~0;
	GLTF_FlagExtras(mesh);

	for(prim = JSON_FindIndexedChild(mesh, "primitives", 0); prim; prim = prim->sibling)
	{
		int mode  = JSON_GetInteger(prim, "mode", 4);
		json_t *attr = JSON_FindChild(prim, "attributes");
		struct gltf_accessor tc_0, tc_1, norm, tang, vpos, col0, idx, sidx, swgt;
		struct gltf_accessor morph_vpos[MAX_MORPHWEIGHTS], morph_norm[MAX_MORPHWEIGHTS], morph_tang[MAX_MORPHWEIGHTS];
		galiasinfo_t *surf;
		size_t i, j;

		prim->used = true;

		if (mode != 4)
		{
			Con_Printf("Primitive mode %i not supported\n", mode);
			continue;
		}

		for (i = 0; ; i++)
		{
			target = JSON_FindIndexedChild(prim, "targets", i);
			if (!target)
				break;
			GLTF_GetAccessor(gltf, JSON_FindChild(target, "POSITION"), &morph_vpos[i]);
			GLTF_GetAccessor(gltf, JSON_FindChild(target, "NORMAL"), &morph_norm[i]);
			GLTF_GetAccessor(gltf, JSON_FindChild(target, "TANGENT"), &morph_tang[i]);
		}
		if (i != morphtargets)
		{
			if (morphtargets == ~0)
				morphtargets = i;
			else if (gltf->warnlimit --> 0)
				Con_Printf(CON_WARNING"morphtargets count changed between primitives\n");
			for (; i < morphtargets; i++)
			{
				memset(&morph_vpos[i], 0, sizeof(morph_vpos[i]));
				memset(&morph_norm[i], 0, sizeof(morph_norm[i]));
				memset(&morph_tang[i], 0, sizeof(morph_tang[i]));
			}
		}

		GLTF_FlagExtras(prim);

		GLTF_GetAccessor(gltf, JSON_FindChild(attr, "TEXCOORD_0"),	&tc_0);	//float, ubyte, ushort
		GLTF_GetAccessor(gltf, JSON_FindChild(attr, "TEXCOORD_1"),	&tc_1);	//float, ubyte, ushort
		GLTF_GetAccessor(gltf, JSON_FindChild(attr, "NORMAL"),		&norm);	//float
		GLTF_GetAccessor(gltf, JSON_FindChild(attr, "TANGENT"),		&tang);	//float
		GLTF_GetAccessor(gltf, JSON_FindChild(attr, "POSITION"),	&vpos);	//float
		GLTF_GetAccessor(gltf, JSON_FindChild(attr, "COLOR_0"),		&col0);	//float, ubyte, ushort
		GLTF_GetAccessor(gltf, JSON_FindChild(prim, "indices"),		&idx);
		if (gltf->ver <= 1)
		{
			GLTF_GetAccessor(gltf, JSON_FindChild(attr, "JOINT"),	&sidx);	//ubyte, ushort
			GLTF_GetAccessor(gltf, JSON_FindChild(attr, "WEIGHT"),	&swgt);	//float, ubyte, ushort
		}
		else
		{	//potentially multiple, each a vec4.
			GLTF_GetAccessor(gltf, JSON_FindChild(attr, "JOINTS_0"),	&sidx);	//ubyte, ushort
			GLTF_GetAccessor(gltf, JSON_FindChild(attr, "WEIGHTS_0"),	&swgt);	//float, ubyte, ushort
		}

		if (JSON_GetInteger(attr, "JOINTS_1",	-1) != -1 || JSON_GetInteger(attr, "WEIGHTS_1",	-1) != -1)
			if (gltf->warnlimit --> 0)
				Con_Printf(CON_WARNING "%s: only 4 bones supported per vert\n", gltf->mod->name);	//in case a model tries supplying more. we ought to renormalise the weights in this case.

		if (!vpos.count)
			continue;

		surf = modfuncs->ZG_Malloc(&mod->memgroup, sizeof(*surf) + (morphtargets*sizeof(float)));

		surf->surfaceid = JSON_GetInteger(prim, "extras.fte.surfaceid", meshidx);
		surf->contents = JSON_GetInteger(prim, "extras.fte.contents", FTECONTENTS_BODY);
		surf->csurface.flags = JSON_GetInteger(prim, "extras.fte.surfaceflags", 0);
		surf->geomset = JSON_GetInteger(prim, "extras.fte.geomset", ~0u);
		surf->geomid = JSON_GetInteger(prim, "extras.fte.geomid", 0);
		surf->mindist = JSON_GetInteger(prim, "extras.fte.mindist", 0);
		surf->maxdist = JSON_GetInteger(prim, "extras.fte.maxdist", 0);

		surf->shares_bones = gltf->numsurfaces;
		surf->shares_verts = gltf->numsurfaces;
		JSON_ReadBody(meshname, surf->surfacename, sizeof(surf->surfacename));

		surf->numverts = vpos.count;
		if (idx.data)
		{
			surf->numindexes = idx.count;
			surf->ofs_indexes = modfuncs->ZG_Malloc(&mod->memgroup, sizeof(*surf->ofs_indexes) * idx.count);
			if (idx.componentType == 5123)
			{	//unsigned shorts
				for (i = 0; i < idx.count; i++)
					surf->ofs_indexes[i] = *(unsigned short *)((char*)idx.data + i*idx.bytestride);
			}
			else if (idx.componentType == 5121)
			{	//unsigned bytes
				for (i = 0; i < idx.count; i++)
					surf->ofs_indexes[i] = *(unsigned char *)((char*)idx.data + i*idx.bytestride);
			}
			else if (idx.componentType == 5125)
			{	//unsigned ints
				for (i = 0; i < idx.count; i++)
					surf->ofs_indexes[i] = *(unsigned int *)((char*)idx.data + i*idx.bytestride);	//FIXME: bounds check.
			}
			else
				continue;
		}
		else
		{
			surf->numindexes = surf->numverts;
			surf->ofs_indexes = modfuncs->ZG_Malloc(&mod->memgroup, sizeof(*surf->ofs_indexes) * surf->numverts);
			for (i = 0; i < surf->numverts; i++)
				surf->ofs_indexes[i] = i;
		}

		//swap winding order. we cull wrongly.
		for (i = 0; i < idx.count; i+=3)
		{
			index_t t = surf->ofs_indexes[i+0];
			surf->ofs_indexes[i+0] = surf->ofs_indexes[i+2];
			surf->ofs_indexes[i+2] = t;
		}

		surf->AnimateMorphs = GLTF_AnimateMorphs;
		memcpy((float*)(surf+1), morphweights, sizeof(float)*morphtargets);
		surf->nummorphs = morphtargets;
		surf->ofs_skel_xyz = modfuncs->ZG_Malloc(&mod->memgroup, (sizeof(*surf->ofs_skel_xyz)+sizeof(*surf->ofs_skel_norm)+sizeof(*surf->ofs_skel_svect)+sizeof(*surf->ofs_skel_tvect)) * surf->numverts * (1+morphtargets));
		surf->ofs_skel_norm = (vec3_t*)(surf->ofs_skel_xyz+surf->numverts*(1+morphtargets));
		surf->ofs_skel_svect = (vec3_t*)(surf->ofs_skel_norm+surf->numverts*(1+morphtargets));
		surf->ofs_skel_tvect = (vec3_t*)(surf->ofs_skel_svect+surf->numverts*(1+morphtargets));

		surf->ofs_skel_xyz		= GLTF_AccessorToDataF(gltf, surf->numverts, countof(surf->ofs_skel_xyz[0]),		&vpos, surf->ofs_skel_xyz);
		surf->ofs_skel_norm		= GLTF_AccessorToDataF(gltf, surf->numverts, countof(surf->ofs_skel_norm[0]),	&norm, surf->ofs_skel_norm);			//if no normals, normals should be flat (fragment shader or unwelding the verts...)
		GLTF_AccessorToTangents(gltf, surf->ofs_skel_norm, surf->numverts, &tang, surf->ofs_skel_svect, surf->ofs_skel_tvect);

		for (i = 0; i < morphtargets; i++)
		{
			size_t offset = (i+1) * surf->numverts;
			GLTF_AccessorToDataF(gltf, surf->numverts, countof(surf->ofs_skel_xyz[0]),	&morph_vpos[i], surf->ofs_skel_xyz+offset);
			GLTF_AccessorToDataF(gltf, surf->numverts, countof(surf->ofs_skel_norm[0]),	&morph_norm[i], surf->ofs_skel_norm+offset);			//if no normals, normals should be flat (fragment shader or unwelding the verts...)
			GLTF_AccessorToTangents(gltf, surf->ofs_skel_norm+offset, surf->numverts,   &morph_tang[i], surf->ofs_skel_svect+offset, surf->ofs_skel_tvect+offset);
		}

		surf->ofs_st_array		= GLTF_AccessorToDataF(gltf, surf->numverts, countof(surf->ofs_st_array[0]),		&tc_0, NULL);
		if (tc_1.data)
			surf->ofs_lmst_array	= GLTF_AccessorToDataF(gltf, surf->numverts, countof(surf->ofs_lmst_array[0]),	&tc_1, NULL);
		if (col0.data && col0.componentType == 5121)	//UNSIGNED_BYTE
			surf->ofs_rgbaub	= GLTF_AccessorToDataUB(gltf, surf->numverts, countof(surf->ofs_rgbaub[0]),		&col0);
		else if (col0.data)
			surf->ofs_rgbaf		= GLTF_AccessorToDataF(gltf, surf->numverts, countof(surf->ofs_rgbaf[0]),		&col0, NULL);
		/*else
		{
			surf->ofs_rgbaub = modfuncs->ZG_Malloc(&gltf->mod->memgroup, sizeof(*surf->ofs_rgbaub) * surf->numverts);
			memset(surf->ofs_rgbaub, 0xff, sizeof(*surf->ofs_rgbaub) * surf->numverts);
		}*/
		if (sidx.data && swgt.data)
		{
			surf->ofs_skel_idx		= GLTF_AccessorToDataBone(gltf,surf->numverts, &sidx);
			surf->ofs_skel_weight	= GLTF_AccessorToDataF(gltf, surf->numverts, countof(surf->ofs_skel_weight[0]),	&swgt, NULL);

			for (i = 0; i < surf->numverts; i++)
			{
				float len = surf->ofs_skel_weight[i][0]+surf->ofs_skel_weight[i][1]+surf->ofs_skel_weight[i][2]+surf->ofs_skel_weight[i][3];
				if (len)
					Vector4Scale(surf->ofs_skel_weight[i], 1/len, surf->ofs_skel_weight[i]);
				else
					Vector4Set(surf->ofs_skel_weight[i], 0.5, 0.5, 0.5, 0.5);
			}
		}
		else
		{
			surf->ofs_skel_idx = modfuncs->ZG_Malloc(&gltf->mod->memgroup, sizeof(surf->ofs_skel_idx[0]) * surf->numverts);
			surf->ofs_skel_weight = modfuncs->ZG_Malloc(&gltf->mod->memgroup, sizeof(surf->ofs_skel_weight[0]) * surf->numverts);
			for (i = 0; i < surf->numverts; i++)
			{
				Vector4Set(surf->ofs_skel_idx[i], basebone, 0, 0, 0);
				Vector4Set(surf->ofs_skel_weight[i], 1, 0, 0, 0);
			}
		}

		if (skinmatrix)
		{
			TransformPosArray(surf->ofs_skel_xyz, surf->numverts, skinmatrix);
			if (norm.data)
				TransformDirArray(surf->ofs_skel_norm, surf->numverts, skinmatrix);
			if (tang.data)
			{
				TransformDirArray(surf->ofs_skel_svect, surf->numverts, skinmatrix);
				TransformDirArray(surf->ofs_skel_tvect, surf->numverts, skinmatrix);
			}
		}

		for (i = 0; i < surf->numverts; i++)
		{
//			VectorScale(surf->ofs_skel_xyz[i], 32, surf->ofs_skel_xyz[i]);
			for (j = 0; j < 3; j++)
			{
				if (mod->maxs[j] < surf->ofs_skel_xyz[i][j])
					mod->maxs[j] = surf->ofs_skel_xyz[i][j];
				if (mod->mins[j] > surf->ofs_skel_xyz[i][j])
					mod->mins[j] = surf->ofs_skel_xyz[i][j];
			}
		}

#ifndef SERVERONLY
		surf->numskins = 1;
		surf->ofsskins = GLTF_LoadMaterial(gltf, JSON_FindChild(prim, "material"), surf->ofs_rgbaub||surf->ofs_rgbaf);
#endif

		if (!tang.data)
		{
			modfuncs->AccumulateTextureVectors(surf->ofs_skel_xyz, surf->ofs_st_array, surf->ofs_skel_norm, surf->ofs_skel_svect, surf->ofs_skel_tvect, surf->ofs_indexes, surf->numindexes, !norm.data);
			modfuncs->NormaliseTextureVectors(surf->ofs_skel_norm, surf->ofs_skel_svect, surf->ofs_skel_tvect, surf->numverts, !norm.data);
		}

		gltf->numsurfaces++;
		surf->nextsurf = mod->meshinfo;
		mod->meshinfo = surf;
	}
	return true;
}

static void Matrix4D_Multiply(const double *a, const double *b, double *out)
{
	out[0]  = a[0] * b[0] + a[4] * b[1] + a[8] * b[2] + a[12] * b[3];
	out[1]  = a[1] * b[0] + a[5] * b[1] + a[9] * b[2] + a[13] * b[3];
	out[2]  = a[2] * b[0] + a[6] * b[1] + a[10] * b[2] + a[14] * b[3];
	out[3]  = a[3] * b[0] + a[7] * b[1] + a[11] * b[2] + a[15] * b[3];

	out[4]  = a[0] * b[4] + a[4] * b[5] + a[8] * b[6] + a[12] * b[7];
	out[5]  = a[1] * b[4] + a[5] * b[5] + a[9] * b[6] + a[13] * b[7];
	out[6]  = a[2] * b[4] + a[6] * b[5] + a[10] * b[6] + a[14] * b[7];
	out[7]  = a[3] * b[4] + a[7] * b[5] + a[11] * b[6] + a[15] * b[7];

	out[8]  = a[0] * b[8] + a[4] * b[9] + a[8] * b[10] + a[12] * b[11];
	out[9]  = a[1] * b[8] + a[5] * b[9] + a[9] * b[10] + a[13] * b[11];
	out[10] = a[2] * b[8] + a[6] * b[9] + a[10] * b[10] + a[14] * b[11];
	out[11] = a[3] * b[8] + a[7] * b[9] + a[11] * b[10] + a[15] * b[11];

	out[12] = a[0] * b[12] + a[4] * b[13] + a[8] * b[14] + a[12] * b[15];
	out[13] = a[1] * b[12] + a[5] * b[13] + a[9] * b[14] + a[13] * b[15];
	out[14] = a[2] * b[12] + a[6] * b[13] + a[10] * b[14] + a[14] * b[15];
	out[15] = a[3] * b[12] + a[7] * b[13] + a[11] * b[14] + a[15] * b[15];
}

static void GenMatrixPosQuat4ScaleDouble(const double pos[3], const double quat[4], const double scale[3], double result[16])
{
	float xx, xy, xz, xw, yy, yz, yw, zz, zw;
	float x2, y2, z2;
	float s;
	x2 = quat[0] + quat[0];
	y2 = quat[1] + quat[1];
	z2 = quat[2] + quat[2];

	xx = quat[0] * x2;   xy = quat[0] * y2;   xz = quat[0] * z2;
	yy = quat[1] * y2;   yz = quat[1] * z2;   zz = quat[2] * z2;
	xw = quat[3] * x2;   yw = quat[3] * y2;   zw = quat[3] * z2;

	s = scale[0];
	result[0*4+0] = s*(1.0f - (yy + zz));
	result[1*4+0] = s*(xy + zw);
	result[2*4+0] = s*(xz - yw);
	result[3*4+0] = 0;

	s = scale[1];
	result[0*4+1] = s*(xy - zw);
	result[1*4+1] = s*(1.0f - (xx + zz));
	result[2*4+1] = s*(yz + xw);
	result[3*4+1] = 0;

	s = scale[2];
	result[0*4+2] = s*(xz + yw);
	result[1*4+2] = s*(yz - xw);
	result[2*4+2] = s*(1.0f - (xx + yy));
	result[3*4+2] = 0;

	result[0*4+3] = pos[0];
	result[1*4+3] = pos[1];
	result[2*4+3] = pos[2];
	result[3*4+3] = 1;
}

static qboolean GLTF_ProcessNode(gltf_t *gltf, json_t *nodeid, double pmatrix[16], int parentidx, qboolean isjoint)
{
	double skinmatrix[16], *skinmatrixptr=NULL;
	json_t *c;
	json_t *node;
	json_t *t;
	json_t *skin;
	json_t *meshid;
	quintptr_t nodeidx;
	struct gltfbone_s *b;
	node = GLTF_FindJSONID(gltf, "nodes", nodeid, &nodeidx);
	if (nodeidx >= gltf->numbones)
	{
		if (nodeidx < MAX_BONES)	//don't spam if its detected elsewhere.
			Con_Printf(CON_WARNING"%s: Invalid node index %i\n", gltf->mod->name, (int)nodeidx);
		return false;
	}
	if (!node)
	{
		Con_Printf(CON_WARNING"%s: Invalid node index %i\n", gltf->mod->name, (int)nodeidx);
		return false;
	}

	b = &gltf->bones[nodeidx];
	b->parent = parentidx;

	t = JSON_FindChild(node, "matrix");
	if (t)
	{
		b->rel.rmatrix[0*4+0] = JSON_GetIndexedFloat(t, 0, 1.0);
		b->rel.rmatrix[1*4+0] = JSON_GetIndexedFloat(t, 1, 0.0);
		b->rel.rmatrix[2*4+0] = JSON_GetIndexedFloat(t, 2, 0.0);
		b->rel.rmatrix[3*4+0] = JSON_GetIndexedFloat(t, 3, 0.0);
		b->rel.rmatrix[0*4+1] = JSON_GetIndexedFloat(t, 4, 0.0);
		b->rel.rmatrix[1*4+1] = JSON_GetIndexedFloat(t, 5, 1.0);
		b->rel.rmatrix[2*4+1] = JSON_GetIndexedFloat(t, 6, 0.0);
		b->rel.rmatrix[3*4+1] = JSON_GetIndexedFloat(t, 7, 0.0);
		b->rel.rmatrix[0*4+2] = JSON_GetIndexedFloat(t, 8, 0.0);
		b->rel.rmatrix[1*4+2] = JSON_GetIndexedFloat(t, 9, 0.0);
		b->rel.rmatrix[2*4+2] = JSON_GetIndexedFloat(t, 10,1.0);
		b->rel.rmatrix[3*4+2] = JSON_GetIndexedFloat(t, 11,0.0);
		b->rel.rmatrix[0*4+3] = JSON_GetIndexedFloat(t, 12,0.0);
		b->rel.rmatrix[1*4+3] = JSON_GetIndexedFloat(t, 13,0.0);
		b->rel.rmatrix[2*4+3] = JSON_GetIndexedFloat(t, 14,0.0);
		b->rel.rmatrix[3*4+3] = JSON_GetIndexedFloat(t, 15,1.0);

		Vector4Set(b->rel.quat, 0,0,0,1);
		VectorSet(b->rel.scale,1,1,1);
		VectorSet(b->rel.trans,0,0,0);
	}
	else
	{
		double rot[4];
		double scale[3];
		double trans[3];
		t = JSON_FindChild(node, "rotation");
		rot[0] = JSON_GetIndexedFloat(t, 0, 0.0);
		rot[1] = JSON_GetIndexedFloat(t, 1, 0.0);
		rot[2] = JSON_GetIndexedFloat(t, 2, 0.0);
		rot[3] = JSON_GetIndexedFloat(t, 3, 1.0);
		t = JSON_FindChild(node, "scale");
		scale[0] = JSON_GetIndexedFloat(t, 0, 1.0);
		scale[1] = JSON_GetIndexedFloat(t, 1, 1.0);
		scale[2] = JSON_GetIndexedFloat(t, 2, 1.0);
		t = JSON_FindChild(node, "translation");
		trans[0] = JSON_GetIndexedFloat(t, 0, 0.0);
		trans[1] = JSON_GetIndexedFloat(t, 1, 0.0);
		trans[2] = JSON_GetIndexedFloat(t, 2, 0.0);

		Vector4Copy(rot, b->rel.quat);
		VectorCopy(scale, b->rel.scale);
		VectorCopy(trans, b->rel.trans);

		//T * R * S
		GenMatrixPosQuat4ScaleDouble(trans, rot, scale, b->rel.rmatrix);
	}
	Matrix4D_Multiply(b->rel.rmatrix, pmatrix, b->amatrix);

	skin = GLTF_FindJSONID(gltf, "skins", JSON_FindChild(node, "skin"), NULL);
	if (skin)
	{
		quintptr_t j;
		json_t *joints;
		struct gltf_accessor inverse;
		float *inversef;

		if (gltf->ver <= 1)
			joints = JSON_FindChild(skin, "jointNames");
		else
			joints = JSON_FindChild(skin, "joints");
		if (joints)
			joints = joints->child;
		GLTF_GetAccessor(gltf, JSON_FindChild(skin, "inverseBindMatrices"), &inverse);
		inversef = inverse.data;
		if (inverse.componentType != 5126/*FLOAT*/ || inverse.type != ((4<<8) | 4)/*mat4x4*/)
			inverse.count = 0;
		memset(gltf->bonemap, 0, sizeof(*gltf->bonemap)*MAX_BONES);	//avoid unexpected surprises...
		for (j = 0; j < MAX_BONES && joints; j++, inversef+=inverse.bytestride/sizeof(float), joints=joints->sibling)
		{
			quintptr_t b;
			joints->used = true;
			if (gltf->ver <= 1)
			{	//urgh
				char jointname[64];
				JSON_ReadBody(joints, jointname, sizeof(jointname));	//this is matched to nodes[b].jointName rather than (textual) b, so we can't use our helpers.
				for (b = 0; b < gltf->numbones; b++)
				{
					if (!strcmp(gltf->bones[b].jointname, jointname))
						break;
				}
				if (b == gltf->numbones)
					break;
			}
			else
			{
				b = JSON_GetUInteger(joints, NULL, ~0);
				if (b >= gltf->numbones)
					break;
			}
			gltf->bonemap[j] = b;
			if (j < inverse.count)
			{
				gltf->bones[b].inverse[0] = inversef[0*4+0];
				gltf->bones[b].inverse[1] = inversef[1*4+0];
				gltf->bones[b].inverse[2] = inversef[2*4+0];
				gltf->bones[b].inverse[3] = inversef[3*4+0];

				gltf->bones[b].inverse[4] = inversef[0*4+1];
				gltf->bones[b].inverse[5] = inversef[1*4+1];
				gltf->bones[b].inverse[6] = inversef[2*4+1];
				gltf->bones[b].inverse[7] = inversef[3*4+1];

				gltf->bones[b].inverse[8] = inversef[0*4+2];
				gltf->bones[b].inverse[9] = inversef[1*4+2];
				gltf->bones[b].inverse[10]= inversef[2*4+2];
				gltf->bones[b].inverse[11]= inversef[3*4+2];

				gltf->bones[b].inverse[12]= inversef[0*4+3];
				gltf->bones[b].inverse[13]= inversef[1*4+3];
				gltf->bones[b].inverse[14]= inversef[2*4+3];
				gltf->bones[b].inverse[15]= inversef[3*4+3];
			}
			else
			{
				gltf->bones[b].inverse[0] = 1;
				gltf->bones[b].inverse[1] = 0;
				gltf->bones[b].inverse[2] = 0;
				gltf->bones[b].inverse[3] = 0;

				gltf->bones[b].inverse[4] = 0;
				gltf->bones[b].inverse[5] = 1;
				gltf->bones[b].inverse[6] = 0;
				gltf->bones[b].inverse[7] = 0;

				gltf->bones[b].inverse[8] = 0;
				gltf->bones[b].inverse[9] = 0;
				gltf->bones[b].inverse[10]= 1;
				gltf->bones[b].inverse[11]= 0;

				gltf->bones[b].inverse[12]= 0;
				gltf->bones[b].inverse[13]= 0;
				gltf->bones[b].inverse[14]= 0;
				gltf->bones[b].inverse[15]= 1;
			}
		}

//		GLTF_ProcessNode(gltf, JSON_FindChild(skin, "skeleton"), identity, nodeidx, true);

		if (gltf->ver <= 1)
		{
			int i;
			json_t *bdsm = JSON_FindChild(skin, "bindShapeMatrix");
			if (bdsm)
			{
				skinmatrixptr = skinmatrix;
				for (i = 0; i < 16; i++)
					skinmatrix[i] = JSON_GetIndexedFloat(bdsm, i, ((i%5)==0)?1.0:0.0);
			}
		}
		JSON_FlagAsUsed(node, "name");
	}

	if (gltf->ver <= 1)
	{	//multiple in gltf1
		meshid = JSON_FindChild(node, "meshes");
		if (meshid)
			for (meshid = meshid->child; meshid; meshid = meshid->sibling)
				GLTF_ProcessMesh(gltf, meshid, nodeidx, skinmatrixptr);
	}
	else
	{	//gltf2 moved to only one.
		meshid = JSON_FindChild(node, "mesh");
		if (meshid)
			GLTF_ProcessMesh(gltf, meshid, nodeidx, skinmatrixptr);
	}

	for(c = JSON_FindIndexedChild(node, "children", 0); c; c = c->sibling)
	{
		c->used = true;
		GLTF_ProcessNode(gltf, c, b->amatrix, nodeidx, isjoint);
	}

	b->camera = JSON_GetInteger(node, "camera", -1);

	JSON_WarnIfChild(node, "weights", &gltf->warnlimit);	//default value for morph weight animations
	GLTF_FlagExtras(node);

	return true;
}

struct gltf_animsampler
{
	enum {
		AINTERP_LINEAR,	//(s)lerp
		AINTERP_STEP,	//round down
		AINTERP_CUBICSPLINE, //3 outputs per input, requires at least two inputs. messy.
	} interptype;
	struct gltf_accessor input;	//timestamps
	struct gltf_accessor output;	//values
	int outputs;
};
static void GLTF_Animation_Persist(gltf_t *gltf, struct gltf_accessor *accessor)
{
	model_t *mod = gltf->mod;
	qbyte *newdata = modfuncs->ZG_Malloc(&mod->memgroup, accessor->length);
	memcpy(newdata, accessor->data, accessor->length);
	accessor->data = newdata;
}
static struct gltf_animsampler GLTF_AnimationSampler(gltf_t *gltf, json_t *samplers, json_t *params, json_t *samplerid, int elems)
{
	int outsperinput=1;
	struct gltf_animsampler r;
	json_t *sampler = GLTF_FindJSONIDParent(gltf, samplers, samplerid, NULL);

	char t[32];
	const char *lerptype = JSON_GetString(sampler, "interpolation", t, sizeof(t), "LINEAR");
	if (!strcmp(lerptype, "LINEAR"))
		r.interptype = AINTERP_LINEAR;
	else if (!strcmp(lerptype, "STEP"))
		r.interptype = AINTERP_STEP;
	else if (!strcmp(lerptype, "CUBICSPLINE"))
	{
		outsperinput = 3;
		r.interptype = AINTERP_CUBICSPLINE;
	}
	else
	{
		Con_Printf("Unknown interpolation type %s\n", lerptype);
		r.interptype = AINTERP_LINEAR;
	}

	if (gltf->ver <= 1)
	{
		GLTF_GetAccessor(gltf, GLTF_FindJSONIDParent(gltf, params, JSON_FindChild(sampler, "input"), NULL), &r.input);
		GLTF_GetAccessor(gltf, GLTF_FindJSONIDParent(gltf, params, JSON_FindChild(sampler, "output"), NULL), &r.output);
	}
	else
	{
		GLTF_GetAccessor(gltf, JSON_FindChild(sampler, "input"), &r.input);
		GLTF_GetAccessor(gltf, JSON_FindChild(sampler, "output"), &r.output);
	}

	if (!r.input.count)
		r.input.count = 1;
	else
		r.outputs = r.output.count / (r.input.count*outsperinput);
	if (!r.input.data || !r.output.data || r.input.count*outsperinput*r.outputs != r.output.count)
		memset(&r, 0, sizeof(r));
	else
	{
		GLTF_Animation_Persist(gltf, &r.input);
		GLTF_Animation_Persist(gltf, &r.output);
	}
	return r;
}

static float Anim_GetTime(const struct gltf_accessor *in, int index)
{
	//read the input sampler (to get timestamps)
	switch(in->componentType)
	{
	case 5120:	//BYTE
		return max(-1, (*(signed char*)((qbyte*)in->data + in->bytestride*index)) / 127.0);
	case 5121:	//UNSIGNED_BYTE
		return (*(unsigned char*)((qbyte*)in->data + in->bytestride*index)) / 255.0;
	case 5122: //SHORT
		return max(-1, (*(signed short*)((qbyte*)in->data + in->bytestride*index)) / 32767.0);
	case 5123: //UNSIGNED_SHORT
		return (*(unsigned short*)((qbyte*)in->data + in->bytestride*index)) / 65535.0;
	case 5125: //UNSIGNED_INT
		return (*(unsigned int*)((qbyte*)in->data + in->bytestride*index)) / (double)~0u;
	case 5126: //FLOAT
		return *(float*)((qbyte*)in->data + in->bytestride*index);
	default:
		Con_Printf("Unsupported input component type %i\n", in->componentType);
		return 0;
	}
}
static void Anim_GetVal(const struct gltf_accessor *in, int index, float *result, int elems)
{
	//read the input sampler (to get timestamps)
	switch(in->componentType)
	{
	case 5120:	//BYTE
		while (elems --> 0)
			result[elems] = max(-1, ((signed char*)((qbyte*)in->data + in->bytestride*index))[elems] / 127.0);
		break;
	case 5121:	//UNSIGNED_BYTE
		while (elems --> 0)
			result[elems] = ((unsigned char*)((qbyte*)in->data + in->bytestride*index))[elems] / 255.0;
		break;
	case 5122: //SHORT
		while (elems --> 0)
			result[elems] = max(-1, ((signed short*)((qbyte*)in->data + in->bytestride*index))[elems] / 32767.0);
		break;
	case 5123: //UNSIGNED_SHORT
		while (elems --> 0)
			result[elems] = ((unsigned short*)((qbyte*)in->data + in->bytestride*index))[elems] / 65535.0;
		break;
	case 5125: //UNSIGNED_INT
		while (elems --> 0)
			result[elems] = ((unsigned int*)((qbyte*)in->data + in->bytestride*index))[elems] / (double)~0u;
		break;
	case 5126: //FLOAT
		while (elems --> 0)
			result[elems] = ((float*)((qbyte*)in->data + in->bytestride*index))[elems];
		break;
	default:
		Con_Printf("Unsupported output component type %i\n", in->componentType);
		break;
	}
}
static void QuaternionSlerp_(const vec4_t p, const vec4_t q, float t, vec4_t qt)
{
	int i;
	float omega, cosom, sinom, sclp, sclq;
	vec4_t flipped;

	// decide if one of the quaternions is backwards
	float a = 0;
	float b = 0;
	for (i = 0; i < 4; i++) {
		a += (p[i]-q[i])*(p[i]-q[i]);
		b += (p[i]+q[i])*(p[i]+q[i]);
	}
	if (a > b) {
		for (i = 0; i < 4; i++) {
			flipped[i] = -q[i];
		}
		q = flipped;
	}

	cosom = p[0]*q[0] + p[1]*q[1] + p[2]*q[2] + p[3]*q[3];

	if ((1.0 + cosom) > 0.00000001) {
		if ((1.0 - cosom) > 0.00000001) {
			omega = acos( cosom );
			sinom = sin( omega );
			sclp = sin( (1.0 - t)*omega) / sinom;
			sclq = sin( t*omega ) / sinom;
		}
		else {
			sclp = 1.0 - t;
			sclq = t;
		}
		for (i = 0; i < 4; i++) {
			qt[i] = sclp * p[i] + sclq * q[i];
		}
	}
	else {
		qt[0] = -p[1];
		qt[1] = p[0];
		qt[2] = -p[3];
		qt[3] = p[2];
		sclp = sin( (1.0 - t) * 0.5 * M_PI);
		sclq = sin( t * 0.5 * M_PI);
		for (i = 0; i < 4; i++) {
			qt[i] = sclp * p[i] + sclq * qt[i];
		}
	}
}
static void LerpAnimData(const struct gltf_animsampler *samp, float time, float *result, int elems, qboolean slerp)
{
	float t0, t1;
	float w0, w1;
	float v0[4], v1[4];
	int f0, f1, c;

	const struct gltf_accessor *in = &samp->input;
	const struct gltf_accessor *out = &samp->output;

	t0 = t1 = Anim_GetTime(in, f1=f0=0);
	while (time > t1 && f1 < in->count-1)
	{
		t0 = t1;
		f0 = f1;
		f1++;
		t1 = Anim_GetTime(in, f1);
	}

	f0 *= samp->outputs;
	f1 *= samp->outputs;

	if (samp->interptype == AINTERP_CUBICSPLINE)
	{
		float step=t1-t0;
		float t=bound(0, (time-t0)/step, 1);
		float tt=t*t, ttt=tt*t;
		//Hermite spline factors
		float m0 = (2*ttt - 3*tt + 1);
		float mb = (ttt - 2*tt + t)*step;
		float m1 = (-2*ttt + 3*tt);
		float ma = (ttt - tt)*step;
		float a[4], b[4];

		//get the relevant tangents+sample values
		//<quote>When used with CUBICSPLINE interpolation, tangents (ak, bk) and values (vk) are grouped within keyframes:
		//a1,a2,...an,v1,v2,...vn,b1,b2,...bn</quote>
		//so ignore that and use avb,avb,avb groups...
		Anim_GetVal(out, f1*3+0, a, elems);
		Anim_GetVal(out, f0*3+1, v0, elems);
		Anim_GetVal(out, f1*3+1, v1, elems);
		Anim_GetVal(out, f0*3+2, b, elems);

		//and compute the spline.
		for (c = 0; c < elems; c++)
			result[c] = m0*v0[c] + mb*b[c] + m1*v1[c] + ma*a[c];

		//quats must be normalized.
		if (slerp)
		{
			float len = sqrt(DotProduct4(result,result));
			Vector4Scale(result, 1/len, result);
		}
		return;
	}
	else if (time <= t0)			//if before the first time, clamp it.
		w1 = 0;
	else if (time >= t1)	//if after tha last frame we could find, clamp it to the last.
		w1 = 1;
	else if (samp->interptype == AINTERP_LINEAR)
		w1 = (time-t0)/(t1-t0);
	else //if (samp->interptype == AINTERP_STEP)
		w1 = 0;

	if (w1 <= 0)
		Anim_GetVal(out, f0, result, elems);
	else if (w1 >= 1)
		Anim_GetVal(out, f1, result, elems);
	else
	{
		Anim_GetVal(out, f0, v0, elems);
		Anim_GetVal(out, f1, v1, elems);
		if (slerp)
			QuaternionSlerp_(v0, v1, w1, result);
		else
		{
			w0 = 1-w1;
			for (c = 0; c < elems; c++)
				result[c] = v0[c]*w0 + w1*v1[c];
		}
	}
}

static void GLTF_RemapBone(gltf_t *gltf, size_t *nextidx, size_t b)
{	//potentially needs to walk to the root before the child. recursion sucks.
	if (b == -1 || gltf->bonemap[b] >= 0)
		return;	//already got remapped
	GLTF_RemapBone(gltf, nextidx, gltf->bones[b].parent);
	gltf->bonemap[b] = (*nextidx)++;
}
static void GLTF_RewriteBoneTree(gltf_t *gltf)
{
	galiasinfo_t *surf;
	size_t j, n;
	struct gltfbone_s *tmpbones;

	for (j = 0; j < gltf->numbones; j++)
	{
		if (gltf->bones[j].parent >= j)
			break;
	}
	if (j == gltf->numbones)
	{
		for (j = 0; j < gltf->numbones; j++)
			gltf->bonemap[j] = j;
		return;	//all are ordered okay
	}

	for (j = 0; j < gltf->numbones; j++)
		gltf->bonemap[j] = -1;
	for (     ; j < MAX_BONES; j++)
		gltf->bonemap[j] = 0;
	n = 0;
	for (j = 0; j < gltf->numbones; j++)
		GLTF_RemapBone(gltf, &n, j);

	tmpbones = malloc(sizeof(*tmpbones)*gltf->numbones);
	memcpy(tmpbones, gltf->bones, sizeof(*tmpbones)*gltf->numbones);
	for (j = 0; j < gltf->numbones; j++)
		gltf->bones[gltf->bonemap[j]] = tmpbones[j];
	for (j = 0; j < gltf->numbones; j++)
		if (gltf->bones[j].parent >= 0)
			gltf->bones[j].parent = gltf->bonemap[gltf->bones[j].parent];

	for(surf = gltf->mod->meshinfo; surf; surf = surf->nextsurf)
	{
		for (j = 0; j < surf->numverts; j++)
			for (n = 0; n < countof(surf->ofs_skel_idx[j]); n++)
				surf->ofs_skel_idx[j][n] = gltf->bonemap[surf->ofs_skel_idx[j][n]];
	}
}

struct galiasanimation_gltf_s
{	//stored in galiasanimation_t->boneofs
	float duration;
	struct
	{
		struct gltf_animsampler rot,scale,trans,morph;
	} bone[1];
};
cvar_t temp1;
static const float *QDECL GLTF_AnimateMorphs(const galiasinfo_t *surf, const framestate_t *framestate)
{
	static float morphs[MAX_MORPHWEIGHTS];
	float imorphs[MAX_MORPHWEIGHTS], *src;
	size_t influence, m;
	int bone = temp1.ival;
	const struct galiasanimation_gltf_s *a;
	const struct framestateregion_s *fg = &framestate->g[FS_REG];
	memset(morphs, 0, sizeof(morphs[0])*surf->nummorphs);
	for (influence = 0; influence < countof(framestate->g[FS_REG].frame); influence++)
	{
		m = 0;
		if (!fg->lerpweight[influence])
			continue;	//mneh, don't care.
		if (surf->ofsanimations && (unsigned int)fg->frame[influence] < (unsigned int)surf->numanimations)
		{
			a = surf->ofsanimations[fg->frame[influence]].boneofs;
			if (a && a->bone[bone].morph.input.count)
			{
				int asz = min(a->bone[bone].morph.outputs, surf->nummorphs);
				double time = fg->frametime[influence];
				if (surf->ofsanimations[fg->frame[influence]].loop && time >= a->duration)
					time = time - a->duration*floor(time/a->duration);
				LerpAnimData(&a->bone[bone].morph, time, imorphs, asz, false);
				for (; m < surf->nummorphs && m < asz; m++)
					morphs[m] += fg->lerpweight[influence] * imorphs[m];
			}
		}

		src = (float*)(surf+1);	//our static morphs are stuck on the end of our surf struct.;
		for (; m < surf->nummorphs; m++)
			morphs[m] += fg->lerpweight[influence]*src[m];
	}
	return morphs;
}
static float *QDECL GLTF_AnimateBones(const galiasinfo_t *surf, const galiasanimation_t *anim, float time, float *bonematrix, int numbones)
{
	const struct galiasbone_gltf_s *defbone = surf->ctx;
	int j = 0, l;
	const struct galiasanimation_gltf_s *a = anim->boneofs;

	if (anim->loop && time >= a->duration)
		time = time - a->duration*floor(time/a->duration);

	for (j = 0; j < numbones; j++, bonematrix+=12)
	{
		float scale[3];
		float rot[4];
		float trans[3];
		//eww, weird inheritance crap.
		if (a->bone[j].rot.input.data || a->bone[j].scale.input.data || a->bone[j].trans.input.data)
		{
			VectorCopy(defbone[j].scale, scale);
			Vector4Copy(defbone[j].quat, rot);
			VectorCopy(defbone[j].trans, trans);

			if (a->bone[j].rot.input.data)
				LerpAnimData(&a->bone[j].rot, time, rot, 4, true);
			if (a->bone[j].scale.input.data)
				LerpAnimData(&a->bone[j].scale, time, scale, 3, false);
			if (a->bone[j].trans.input.data)
				LerpAnimData(&a->bone[j].trans, time, trans, 3, false);
			//figure out the bone matrix...
			modfuncs->GenMatrixPosQuat4Scale(trans, rot, scale, bonematrix);
		}
		else
		{	//nothing animated, use what we calculated earlier.
			for (l = 0; l < 12; l++)
				bonematrix[l] = defbone[j].rmatrix[l];
		}
		if (surf->ofsbones[j].parent < 0)
		{	//rotate any root bones from gltf to quake's orientation.
			float fnar[12];
			float toquake[12]={
				0,0,mod_gltf_scale->value, 0,
				mod_gltf_scale->value,0,0, 0,
				0,mod_gltf_scale->value,0, 0};
			memcpy(fnar, bonematrix, sizeof(fnar));
			modfuncs->ConcatTransforms((void*)toquake, (void*)fnar, (void*)bonematrix);
		}
	}
	return bonematrix - j*12;
}

//okay, so gltf is some weird scene thing.
//mostly there should be some default scene, so we'll just use that.
//we do NOT supported nested nodes right now...
static qboolean GLTF_LoadModel(struct model_s *mod, char *json, size_t jsonsize, void *buffer, size_t buffersize)
{
	static struct knowngltfextensions_s
	{
		const char *name;
		qboolean supported;	//unsupported extensions don't really need to be listed, but they do prevent warnings from unknown-but-used extensions
		qboolean draft;		//true when our implementation is probably buggy on account of the spec maybe changing.
	} extensions_v1[] =
	{
		{"KHR_binary_glTF",							true,	false},
		{"KHR_materials_common",					true,   false},
		{NULL}
	}, extensions_v2[] =
	{
		{"KHR_materials_pbrSpecularGlossiness",		true,   false},
//		{"KHR_materials_cmnBlinnPhong",				true,   true},
		{"KHR_materials_unlit",						true,	false},
		{"KHR_texture_transform",					false,	true},	//probably not fatal
		{"KHR_draco_mesh_compression",				false,	true},	//probably fatal
		{"KHR_mesh_quantization",					true,	true},
		{"MSFT_texture_dds",						true,	false},
		{"MSFT_packing_occlusionRoughnessMetallic", true,	false},
		{NULL}
	}, *extensions;
	gltf_t gltf;
	int pos=0;
	quintptr_t j,k;
	json_t *scene, *n, *anim;
	double rootmatrix[16];
	double gltfver;
	galiasinfo_t *surf;
	galiasbone_t *bone;
	galiasanimation_t *framegroups = NULL;
	unsigned int numframegroups = 0;
	float *baseframe;
	struct galiasbone_gltf_s *gltfbone;
	memset(&gltf, 0, sizeof(gltf));
	gltf.bonemap = malloc(sizeof(*gltf.bonemap)*MAX_BONES);
	gltf.bones = malloc(sizeof(*gltf.bones)*MAX_BONES);
	memset(gltf.bones, 0, sizeof(*gltf.bones)*MAX_BONES);
	gltf.r = JSON_Parse(NULL, mod->name, NULL, json, &pos, jsonsize);
	gltf.mod = mod;
	gltf.buffers[0].data = buffer;
	gltf.buffers[0].length = buffersize;
	gltf.warnlimit = 5;

	//asset.version must exist, supposedly. so default to something b0rked
	gltfver = JSON_GetFloat(gltf.r, "asset.minVersion", 0.0);
	if (gltfver != 2.0)
		gltfver = JSON_GetFloat(gltf.r, "asset.version", 0.0);
	if (gltfver == 2.0)
		gltf.ver = 2;
	if (gltfver == 1.0)	//we load gltf1 models. we don't get the materials right but gltf1 sucks for that anyway.
		gltf.ver = 1;
	if (gltf.ver)
	{
		if (gltf.ver <= 1)
		{
			JSON_FlagAsUsed(gltf.r, "asset.profile");
			JSON_FlagAsUsed(gltf.r, "asset.premultipliedAlpha");
			extensions = extensions_v1;
		}
		else
			extensions = extensions_v2;
		JSON_FlagAsUsed(gltf.r, "asset.copyright");
		JSON_FlagAsUsed(gltf.r, "asset.generator");
		JSON_WarnIfChild(gltf.r, "asset.minVersion", &gltf.warnlimit);
		JSON_WarnIfChild(gltf.r, "asset.extensions", &gltf.warnlimit);

		for(n = JSON_FindIndexedChild(gltf.r, "extensionsRequired", 0); n; n = n->sibling)
		{
			char extname[256];
			JSON_ReadBody(n, extname, sizeof(extname));
			for (j = 0; extensions[j].name; j++)
			{
				if (!strcmp(extname, extensions[j].name))
					break;
			}
			if (!extensions[j].supported)
			{
				Con_Printf(CON_ERROR "%s: Required gltf%i extension \"%s\" not supported\n", mod->name, gltf.ver, extname);
				goto abort;
			}
		}

		for(n = JSON_FindIndexedChild(gltf.r, "extensionsUsed", 0); n; n = n->sibling)
		{	//must be a superset of the above.
			char extname[256];
			JSON_ReadBody(n, extname, sizeof(extname));
			for (j = 0; extensions[j].name; j++)
			{
				if (!strcmp(extname, extensions[j].name))
					break;
			}
			if (!extensions[j].supported)
				Con_Printf(CON_WARNING "%s: gltf%i extension \"%s\" not known\n", mod->name, gltf.ver, extname);
			else if (extensions[j].draft)
				Con_Printf(CON_WARNING "%s: gltf%i extension \"%s\" follows draft implementation, and may be non-standard/buggy\n", mod->name, gltf.ver, extname);
		}

		VectorClear(mod->maxs);
		VectorClear(mod->mins);

		//we don't really care about cameras.
		JSON_FlagAsUsed(gltf.r, "cameras");

		scene = GLTF_FindJSONID_First(&gltf, "scenes", JSON_FindChild(gltf.r, "scene"), NULL);

		memset(&rootmatrix, 0, sizeof(rootmatrix));
#if 1	//transform from gltf to quake. mostly only needed for the base pose.
		rootmatrix[2] = rootmatrix[4] = rootmatrix[9] = mod_gltf_scale->value; rootmatrix[15] = 1;
#else
		rootmatrix[0] = rootmatrix[5] = rootmatrix[10] = 1; rootmatrix[15] = 1;
#endif

		n = JSON_FindChild(gltf.r, "nodes");
		for (j = 0; ; j++)
		{
			if (gltf.ver <= 1)
				n = j?n->sibling:n->child;
			else
				n = JSON_FindIndexedChild(gltf.r, "nodes", j);
			if (!n)
				break;
			if (j == MAX_BONES)
			{
				Con_Printf(CON_WARNING"%s: too many nodes (max %i)\n", mod->name, MAX_BONES);
				break;
			}
			JSON_ReadBody(JSON_FindChild(n, "jointName"), gltf.bones[j].jointname, sizeof(gltf.bones[j].jointname));
			if (!JSON_ReadBody(JSON_FindChild(n, "name"), gltf.bones[j].name, sizeof(gltf.bones[j].name)))
			{
				if (n)
					JSON_GetPath(n, true, gltf.bones[j].name, sizeof(gltf.bones[j].name));
				else
					Q_snprintf(gltf.bones[j].name, sizeof(gltf.bones[j].name), "bone%u", (unsigned)j);
			}
			gltf.bones[j].camera = -1;
			gltf.bones[j].parent = -1;
			gltf.bones[j].amatrix[0] = gltf.bones[j].amatrix[5] = gltf.bones[j].amatrix[10] = gltf.bones[j].amatrix[15] = 1;
			gltf.bones[j].inverse[0] = gltf.bones[j].inverse[5] = gltf.bones[j].inverse[10] = gltf.bones[j].inverse[15] = 1;
			gltf.bones[j].rel.rmatrix[0] = gltf.bones[j].rel.rmatrix[5] = gltf.bones[j].rel.rmatrix[10] = gltf.bones[j].rel.rmatrix[15] = 1;
		}
		gltf.numbones = j;

		JSON_FlagAsUsed(scene, "name");
		GLTF_FlagExtras(scene);
		for (j = 0; ; j++)
		{
			n = JSON_FindIndexedChild(scene, "nodes", j);
			if (!n)
				break;
			n->used = true;
//			if (!
			GLTF_ProcessNode(&gltf, n, rootmatrix, -1, false);
//				break;
		}

		GLTF_RewriteBoneTree(&gltf);

		gltfbone = modfuncs->ZG_Malloc(&mod->memgroup, sizeof(*gltfbone)*gltf.numbones);
		bone = modfuncs->ZG_Malloc(&mod->memgroup, sizeof(*bone)*gltf.numbones);
		baseframe = modfuncs->ZG_Malloc(&mod->memgroup, sizeof(float)*12*gltf.numbones);
		for (j = 0; j < gltf.numbones; j++)
		{
			Q_strlcpy(bone[j].name, gltf.bones[j].name, sizeof(bone[j].name));
			bone[j].parent = gltf.bones[j].parent;

			if (gltf.bones[j].camera >= 0 && !mod->camerabone)
				mod->camerabone = j+1;

			for(k = 0; k < 12; k++)
			{
				baseframe[j*12+k] = gltf.bones[j].amatrix[k];
				bone[j].inverse[k] = gltf.bones[j].inverse[k];
			}
			gltfbone[j] = gltf.bones[j].rel;
		}

		anim = JSON_FindChild(gltf.r, "animations");
		if (anim)
			for(anim = anim->child; anim; anim = anim->sibling)
				numframegroups++;
		if (numframegroups)
		{
			struct galiasanimation_gltf_s **mergeanims = NULL;
			if (mod_gltf_fixbuggyanims->ival)
			{
				mergeanims = alloca(sizeof(*mergeanims)*gltf.numbones);
				memset(mergeanims, 0, sizeof(mergeanims)*gltf.numbones);
			}
			framegroups = modfuncs->ZG_Malloc(&mod->memgroup, sizeof(*framegroups)*numframegroups);
			anim = JSON_FindChild(gltf.r, "animations")->child;
			for (k = 0; k < numframegroups; k++, anim = anim->sibling)
			{
				galiasanimation_t *fg = &framegroups[k];
				json_t *chan;
				json_t *samps = JSON_FindChild(anim, "samplers");
				json_t *params = gltf.ver<=1?JSON_FindChild(anim, "parameters"):0;	//gltf1
				float maxtime = 0;
				unsigned maxposes = 0;
				struct galiasanimation_gltf_s *a = modfuncs->ZG_Malloc(&mod->memgroup, sizeof(*a)+sizeof(a->bone[0])*(gltf.numbones-1));
				anim->used = true;

				if (!JSON_ReadBody(JSON_FindChild(anim, "name"), fg->name, sizeof(fg->name)))
				{
					if (gltf.ver <= 1)
						Q_snprintf(fg->name, sizeof(fg->name), "%s", anim->name);
					else if (anim)
						JSON_GetPath(anim, true, fg->name, sizeof(fg->name));
					else
						Q_snprintf(fg->name, sizeof(fg->name), "anim%u", (unsigned int)k);
				}
				fg->loop = true;
				fg->skeltype = SKEL_RELATIVE;
				for(chan = JSON_FindIndexedChild(anim, "channels", 0); chan; chan = chan->sibling)
				{
					struct gltf_animsampler s;
					json_t *targ = JSON_FindChild(chan, "target");
					json_t *samplerid = JSON_FindChild(chan, "sampler");
					qintptr_t bone;
					json_t *path = JSON_FindChild(targ, "path");
					chan->used = true;

					if (gltf.ver <= 1)
						GLTF_FindJSONID(&gltf, "nodes", JSON_FindChild(targ, "id"), (quintptr_t*)&bone);
					else
					{
						bone = JSON_GetInteger(targ, "node", -2);
						if (bone == -2)
							continue;	//'When node isn't defined, channel should be ignored'
					}
					if (bone < 0 || bone >= gltf.numbones)
					{
						if (gltf.warnlimit --> 0)
							Con_Printf("%s: invalid node index %i\n", mod->name, (int)bone);
						continue;	//error...
					}
					bone = gltf.bonemap[bone];
					if (mergeanims)
					{
						if (mergeanims[bone] && mergeanims[bone] != a)
							mergeanims = NULL;	//was some other animation... which means two animations are fighting over the same joint. which means we don't need to use this hack! yay!
						else
							mergeanims[bone] = a;
					}
					s = GLTF_AnimationSampler(&gltf, samps, params, samplerid, 4);
					if (!s.input.maxs[0] && s.input.count)
						s.input.maxs[0] = Anim_GetTime(&s.input, s.input.count-1);
					maxtime = max(maxtime, s.input.maxs[0]);
					maxposes = max(maxposes, s.input.count);
					if (JSON_Equals(path, NULL, "rotation") && s.outputs==1)
						a->bone[bone].rot = s;
					else if (JSON_Equals(path, NULL, "scale") && s.outputs==1)
						a->bone[bone].scale = s;
					else if (JSON_Equals(path, NULL, "translation") && s.outputs==1)
						a->bone[bone].trans = s;
					else if (JSON_Equals(path, NULL, "weights") && s.outputs>=1)
						a->bone[bone].morph = s;
					else
					{
						if (gltf.warnlimit --> 0)
						{
							char buf[64];
							JSON_ReadBody(path, buf, sizeof(buf));
							Con_Printf("%s: unknown animation data - %s\n", mod->name, buf);
						}
					}
				}

				if (!maxtime)
					maxtime = 1.0/30;	//some stuff doesn't like 0-length animations. divisions by 0 are not nice.
				a->duration = maxtime;

				//calc average framerate
				fg->numposes = maxposes;
				if (maxtime)
					fg->rate = fg->numposes/maxtime;	//fix up the rate so we hit the exact end of the animation (so it doesn't have to be quite so exact).
				else
					fg->rate = 60;

				fg->skeltype = SKEL_RELATIVE;
				fg->GetRawBones = GLTF_AnimateBones;
				fg->boneofs = a;
			}

			if (mergeanims)
			{	//if we got this far then no two animations referenced the same bone.
				//this is a common issue with various sample models, I'm going to call it an exporter bug, and work around it by merging them into a single animation.
				for (k = 1; k < numframegroups && framegroups[k].rate == framegroups[0].rate && framegroups[k].numposes == framegroups[0].numposes; k++)
					;
				if (k == numframegroups)
				{	//yup, all have the same duration+posecount. lets call it a valid fix.
					struct galiasanimation_gltf_s *merged = framegroups[0].boneofs;
					Q_snprintf(framegroups->name, sizeof(framegroups->name), "allbones");
					numframegroups = 1;
					for (k = 0; k < gltf.numbones; k++)
					{
						if (mergeanims[k])
							merged->bone[k] = mergeanims[k]->bone[k];	//copy over that bone.
					}
				}
			}
		}

		for(surf = mod->meshinfo; surf; surf = surf->nextsurf)
		{
			surf->shares_bones = 0;
			surf->numbones = gltf.numbones;
			surf->ofsbones = bone;
			surf->ctx = gltfbone;
			surf->baseframeofs = baseframe;
			surf->ofsanimations = framegroups;
			surf->numanimations = numframegroups;
			surf->contents = FTECONTENTS_BODY;
			surf->csurface.flags = 0;
			surf->geomset = ~0;	//invalid set = always visible. FIXME: set this according to scene numbers?
			surf->geomid = 0;
		}
		VectorScale(mod->mins, mod_gltf_scale->value, mod->mins);
		VectorScale(mod->maxs, mod_gltf_scale->value, mod->maxs);

		if (!mod->meshinfo)
			Con_Printf("%s: Doesn't contain any meshes...\n", mod->name);
		JSON_WarnUnused(gltf.r, &gltf.warnlimit);
	}
	else
		Con_Printf("%s: unsupported gltf version (%.2f)\n", mod->name, gltfver);
abort:
	JSON_Destroy(gltf.r);
	free(gltf.bones);
	free(gltf.bonemap);

	mod->type = mod_alias;
	return !!mod->meshinfo;
}
qboolean QDECL Mod_LoadGLTFModel (struct model_s *mod, void *buffer, size_t fsize)
{
	//just straight json.
	return GLTF_LoadModel(mod, buffer, fsize, NULL, 0);
}
//glb files are some binary header, a lump with json data, and optionally a lump with binary data
qboolean QDECL Mod_LoadGLBModel (struct model_s *mod, void *buffer, size_t fsize)
{
	unsigned char *header = buffer;
	unsigned int magic = header[0]|(header[1]<<8)|(header[2]<<16)|(header[3]<<24);		//gltf
	unsigned int version = header[4]|(header[5]<<8)|(header[6]<<16)|(header[7]<<24);	//2
	unsigned int length = header[8]|(header[9]<<8)|(header[10]<<16)|(header[11]<<24);	//fsize

	unsigned int jsonlen = header[12]|(header[13]<<8)|(header[14]<<16)|(header[15]<<24);
	unsigned int jsontype = header[16]|(header[17]<<8)|(header[18]<<16)|(header[19]<<24);
	char *json = (char*)(header+20);

	if (fsize < 28)
		return false;
	if (magic != (('F'<<24)+('T'<<16)+('l'<<8)+'g'))
		return false;
	if (fsize < length)
		return false;	//allow padding on the end, but not truncation
	if (version == 1)
	{
		unsigned int binlen = (length-20) - jsonlen;
		unsigned char *bin = header+20+jsonlen;

		if (jsonlen&3)
			return false;	//exporter is expected to pad with spaces.
		if (jsontype != 0)	//'JSON'
			return false;
		if (length != 20+jsonlen+binlen)
			return false;

		return GLTF_LoadModel(mod, json, jsonlen, bin, binlen);
	}
	else if (version == 2)
	{
		unsigned int binlen = header[20+jsonlen]|(header[21+jsonlen]<<8)|(header[22+jsonlen]<<16)|(header[23+jsonlen]<<24);
		unsigned int bintype = header[24+jsonlen]|(header[25+jsonlen]<<8)|(header[26+jsonlen]<<16)|(header[27+jsonlen]<<24);
		unsigned char *bin = header+28+jsonlen;

		if (jsontype != 0x4E4F534A)	//'JSON'
			return false;
		if (length != 28+jsonlen+binlen)
			return false;
		if (bintype != 0x004E4942)	//'BIN\0'
			return false;

		return GLTF_LoadModel(mod, json, jsonlen, bin, binlen);
	}
	return false;
}

qboolean Plug_GLTF_Init(void)
{
	filefuncs = plugfuncs->GetEngineInterface(plugfsfuncs_name, sizeof(*filefuncs));
	modfuncs = plugfuncs->GetEngineInterface(plugmodfuncs_name, sizeof(*modfuncs));
	if (modfuncs && modfuncs->version < MODPLUGFUNCS_VERSION)
		modfuncs = NULL;
	mod_gltf_scale = cvarfuncs->GetNVFDG("mod_gltf_scale", "30", CVAR_RENDERERLATCH, "This defines the number of units per metre, in order to correctly load standard-scale gltf models.", "GLTF Models");
	mod_gltf_fixbuggyanims = cvarfuncs->GetNVFDG("mod_gltf_fixbuggyanims", "1", CVAR_RENDERERLATCH, "Work around buggy exporters by merging animations that affect only a single bone.", "GLTF Models");
#ifdef IQMTOOL
	mod_gltf_privatematerials = cvarfuncs->GetNVFDG("mod_gltf_privatematerials", "0", CVAR_RENDERERLATCH, "Add the model path to material names, to isolate them between different models.", "GLTF Models");
#else
	mod_gltf_privatematerials = cvarfuncs->GetNVFDG("mod_gltf_privatematerials", "1", CVAR_RENDERERLATCH, "Add the model path to material names, to isolate them between different models.", "GLTF Models");
#endif
	mod_gltf_ignoretechniques = cvarfuncs->GetNVFDG("mod_gltf_ignoretechniques", "1", CVAR_RENDERERLATCH, "Ignore the gltf1 model-specific glsl. This is enabled by default because it just doesn't work very well.", "GLTF Models");

	if (modfuncs && filefuncs)
	{
		modfuncs->RegisterModelFormatText("glTF models (glTF)", ".gltf", Mod_LoadGLTFModel);
		modfuncs->RegisterModelFormatMagic("glTF models (glb)", (('F'<<24)+('T'<<16)+('l'<<8)+'g'), Mod_LoadGLBModel);
		return true;
	}
	return false;
}
#endif

