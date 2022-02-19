#include "quakedef.h"

//node destruction
static void JSON_Orphan(json_t *t)
{
	if (t->parent)
	{
		json_t *p = t->parent, **l = &p->child;
		if (p->arraymax)
		{
			size_t idx = atoi(t->name);
			if (idx <= p->arraymax)
				p->array[idx] = NULL;
			//FIXME: sibling links are screwed. be careful iterrating after a removal.
		}
		else
		{
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
		}
		t->parent = NULL;
		t->sibling = NULL;
	}
}
void JSON_Destroy(json_t *t)
{
	if (t)
	{
		if (t->arraymax)
		{
			size_t idx;
			for (idx = 0; idx < t->arraymax; idx++)
				if (t->array[idx])
					JSON_Destroy(t->array[idx]);
			free(t->array);
		}
		else
		{
			while(t->child)
				JSON_Destroy(t->child);
		}
		JSON_Orphan(t);
		free(t);
	}
}

//node creation
static json_t *JSON_CreateNode(json_t *parent, const char *namestart, const char *nameend, const char *bodystart, const char *bodyend, int type)
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
	//FIXME: escapes within names are a thing. a stupid thing, but still a thing.
	j = malloc(sizeof(*j) + nameend-namestart + (dupbody?1+bodyend-bodystart:0));
	memcpy(j->name, namestart, nameend-namestart);
	j->name[nameend-namestart] = 0;
	j->bodystart = bodystart;
	j->bodyend = bodyend;

	j->child = NULL;
	j->sibling = NULL;
	j->arraymax = 0;
	j->type = type;
	if (type == json_type_array)
	{	//pre-initialise the array a bit.
		j->arraymax = 32;
		j->array = calloc(j->arraymax, sizeof(*j->array));
	}
	else
		j->childlink = &j->child;
	j->parent = parent;
	if (parent)
	{
		if (parent->arraymax)
		{
			size_t idx = atoi(j->name);
			if (idx >= parent->arraymax)
			{
				size_t oldmax = parent->arraymax;
				parent->arraymax = max(idx+1, parent->arraymax*2);
				parent->array = realloc(parent->array, sizeof(*parent->array)*parent->arraymax);
				while (oldmax < parent->arraymax)
					parent->array[oldmax++] = NULL;	//make sure there's no gaps.
			}
			parent->array[idx] = j;
			if (!idx)
				parent->child = j;
			else if (parent->array[idx-1])
				parent->array[idx-1]->sibling = j;
		}
		else
		{
			*parent->childlink = j;
			parent->childlink = &j->sibling;
		}
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
//handles the not-null-terminated nature of our bodies.
double JSON_ReadFloat(json_t *t, double fallback)
{
	if (t)
	{
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

#if defined(FTEPLUGIN) || defined(IQMTOOL)	//grr, stupid copypasta.
unsigned int utf8_encode(void *out, unsigned int unicode, int maxlen)
{
	unsigned int bcount = 1;
	unsigned int lim = 0x80;
	unsigned int shift;
	if (!unicode)
	{	//modified utf-8 encodes encapsulated nulls as over-long.
		bcount = 2;
	}
	else
	{
		while (unicode >= lim)
		{
			if (bcount == 1)
				lim <<= 4;
			else if (bcount < 7)
				lim <<= 5;
			else
				lim <<= 6;
			bcount++;
		}
	}

	//error if needed
	if (maxlen < bcount)
		return 0;

	//output it.
	if (bcount == 1)
	{
		*((unsigned char *)out) = (unsigned char)(unicode&0x7f);
		out = (char*)out + 1;
	}
	else
	{
		shift = bcount*6;
		shift = shift-6;
		*((unsigned char *)out) = (unsigned char)((unicode>>shift)&(0x0000007f>>bcount)) | ((0xffffff00 >> bcount) & 0xff);
		out = (char*)out + 1;
		do
		{
			shift = shift-6;
			*((unsigned char *)out) = (unsigned char)((unicode>>shift)&0x3f) | 0x80;
			out = (char*)out + 1;
		}
		while(shift);
	}
	return bcount;
}
#endif
static int dehex(int chr, unsigned int *ret, int shift)
{
	if      (chr >= '0' && chr <= '9')
		*ret |= (chr-'0') << shift;
	else if (chr >= 'A' && chr <= 'F')
		*ret |= (chr-'A'+10) << shift;
	else if (chr >= 'a' && chr <= 'f')
		*ret |= (chr-'a'+10) << shift;
	else
		return 0;
	return 1;
}

//writes the body to a null-terminated string, handling escapes as needed.
//returns required body length (without terminator) (NOTE: return value is not escape-aware, so this is an over-estimate).
size_t JSON_ReadBody(json_t *t, char *out, size_t outsize)
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
					case 'u':
						{
							unsigned int code = 0, low = 0;
							if (dehex(out[0], &code, 12) &&	//javscript escapes are strictly 16bit...
								dehex(out[1], &code, 8) &&
								dehex(out[2], &code, 4) &&
								dehex(out[3], &code, 0) )
							{
								in += 4;
								//and as its actually UTF-16 we need to waste more cpu cycles on this insanity when its a high-surrogate.
								if (code >= 0xd800u && code < 0xdc00u && out[4] == '\\' && out[5] == 'u' &&
									dehex(out[6], &low, 12) &&
									dehex(out[7], &low, 8) &&
									dehex(out[8], &low, 4) &&
									dehex(out[9], &low, 0) && low >= 0xdc00 && low < 0xde00)
								{
									in += 6;
									code = 0x10000 + (code-0xd800)*0x400 + (low-0xdc00);
								}

								out += utf8_encode(out, code, outend-out);
								break;
							}
							}
						//fall through.
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
json_t *JSON_ParseNode(json_t *t, const char *namestart, const char *nameend, const char *json, int *jsonpos, int jsonlen)
{
	const char *childstart, *childend;
	JSON_SkipWhite(json, jsonpos, jsonlen);

	if (*jsonpos < jsonlen)
	{
		if (json[*jsonpos] == '{')
		{
			*jsonpos+=1;
			JSON_SkipWhite(json, jsonpos, jsonlen);

			t = JSON_CreateNode(t, namestart, nameend, NULL, NULL, json_type_object);

			while (*jsonpos < jsonlen && json[*jsonpos] == '\"')
			{
				if (!JSON_ParseString(json, jsonpos, jsonlen, &childstart, &childend))
					break;
				JSON_SkipWhite(json, jsonpos, jsonlen);
				if (*jsonpos < jsonlen && json[*jsonpos] == ':')
				{
					*jsonpos+=1;
					if (!JSON_ParseNode(t, childstart, childend, json, jsonpos, jsonlen))
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

			t = JSON_CreateNode(t, namestart, nameend, NULL, NULL, json_type_array);

			for(;;)
			{
				Q_snprintfz(idxname, sizeof(idxname), "%u", idx++);
				if (!JSON_ParseNode(t, idxname, NULL, json, jsonpos, jsonlen))
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
			if (json[*jsonpos] == '\"')
			{
				if (JSON_ParseString(json, jsonpos, jsonlen, &childstart, &childend))
					return JSON_CreateNode(t, namestart, nameend, childstart, childend, json_type_string);
			}
			else
			{
				if (JSON_ParseString(json, jsonpos, jsonlen, &childstart, &childend))
				{
					if (childend-childstart == 4 && !strncasecmp(childstart, "true", 4))
						return JSON_CreateNode(t, namestart, nameend, childstart, childend, json_type_true);
					else if (childend-childstart == 5 && !strncasecmp(childstart, "false", 5))
						return JSON_CreateNode(t, namestart, nameend, childstart, childend, json_type_false);
					else if (childend-childstart == 4 && !strncasecmp(childstart, "null", 4))
						return JSON_CreateNode(t, namestart, nameend, childstart, childend, json_type_null);
					else
						return JSON_CreateNode(t, namestart, nameend, childstart, childend, json_type_number);
				}
			}
		}
	}
	return NULL;
}
json_t *JSON_Parse(const char *json)
{
	size_t jsonlen = strlen(json);
	int pos = (json[0] == '\xef' && json[1] == '\xbb' && json[2] == '\xbf')?3:0;	//skip a utf-8 bom, if present, to be a bit more permissive.
	json_t *n = JSON_ParseNode(NULL, NULL, NULL, json, &pos, jsonlen);
	JSON_SkipWhite(json, &pos, jsonlen);
	if (pos == jsonlen)
		return n;
	JSON_Destroy(n);	//trailing junk?... fail it.
	return NULL;
}



//we don't really understand arrays here (we just treat them as tables) so eg "foo.0.bar" to find t->foo[0]->bar
json_t *JSON_FindChild(json_t *t, const char *child)
{
	if (t)
	{
		size_t nl;
		const char *dot = strchr(child, '.');
		if (dot)
			nl = dot-child;
		else
			nl = strlen(child);
		if (t->arraymax)
		{
			size_t idx = atoi(child);
			if (idx < t->arraymax)
			{
				t = t->array[idx];
				if (t)
					goto found;
			}
		}
		else
		{
			for (t = t->child; t; t = t->sibling)
			{
				if (!strncmp(t->name, child, nl) && (t->name[nl] == '.' || !t->name[nl]))
				{
found:
					child+=nl;
					t->used = true;
					if (*child == '.')
						return JSON_FindChild(t, child+1);
					return t;
				}
			}
		}
	}
	return NULL;
}
json_t *JSON_GetIndexed(json_t *t, unsigned int idx)
{
	if (t)
	{
		if (t->arraymax)
		{
			if (idx < t->arraymax)
			{
				t = t->array[idx];
				if (t)
				{
					t->used = true;
					return t;
				}
			}
		}
		else
		{
			for (t = t->child; t; t = t->sibling, idx--)
			{
				if (!idx)
				{
					t->used = true;
					return t;
				}
			}
		}
	}
	return NULL;
}


//helpers...
json_t *JSON_FindIndexedChild(json_t *t, const char *child, unsigned int idx)
{
	if (child)
		t = JSON_FindChild(t, child);
	return JSON_GetIndexed(t, idx);
}
qboolean JSON_Equals(json_t *t, const char *child, const char *expected)
{
	if (child)
		t = JSON_FindChild(t, child);
	if (t && t->bodyend-t->bodystart == strlen(expected))
		return !strncmp(t->bodystart, expected, t->bodyend-t->bodystart);
	return false;
}
quintptr_t JSON_GetUInteger(json_t *t, const char *child, unsigned int fallback)
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
qintptr_t JSON_GetInteger(json_t *t, const char *child, int fallback)
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
qintptr_t JSON_GetIndexedInteger(json_t *t, unsigned int idx, int fallback)
{
	char idxname[MAX_QPATH];
	Q_snprintfz(idxname, sizeof(idxname), "%u", idx);
	return JSON_GetInteger(t, idxname, fallback);
}
double JSON_GetFloat(json_t *t, const char *child, double fallback)
{
	if (child)
		t = JSON_FindChild(t, child);
	return JSON_ReadFloat(t, fallback);
}
double JSON_GetIndexedFloat(json_t *t, unsigned int idx, double fallback)
{
	char idxname[MAX_QPATH];
	Q_snprintfz(idxname, sizeof(idxname), "%u", idx);
	return JSON_GetFloat(t, idxname, fallback);
}
const char *JSON_GetString(json_t *t, const char *child, char *buffer, size_t buffersize, const char *fallback)
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
