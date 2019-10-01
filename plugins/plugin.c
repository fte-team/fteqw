//contains generic plugin code for dll/qvm
//it's this one or the engine...
#include "plugin.h"
#include <stdarg.h>
#include <stdio.h>


plugcorefuncs_t *plugfuncs;
plugcmdfuncs_t *cmdfuncs;
plugcvarfuncs_t *cvarfuncs;
//plugclientfuncs_t *clientfuncs;




/* An implementation of some 'standard' functions */
void Q_strlncpy(char *d, const char *s, int sizeofd, int lenofs)
{
	int i;
	sizeofd--;
	if (sizeofd < 0)
		return;	//this could be an error

	for (i=0; lenofs-- > 0; i++)
	{
		if (i == sizeofd)
			break;
		*d++ = *s++;
	}
	*d='\0';
}
void Q_strlcpy(char *d, const char *s, int n)
{
	int i;
	n--;
	if (n < 0)
		return;	//this could be an error

	for (i=0; *s; i++)
	{
		if (i == n)
			break;
		*d++ = *s++;
	}
	*d='\0';
}
void Q_strlcat(char *d, const char *s, int n)
{
	if (n)
	{
		int dlen = strlen(d);
		int slen = strlen(s)+1;
		if (slen > (n-1)-dlen)
			slen = (n-1)-dlen;
		memcpy(d+dlen, s, slen);
		d[n - 1] = 0;
	}
}

char *Plug_Info_ValueForKey (const char *s, const char *key, char *out, size_t outsize)
{
	int isvalue = 0;
	const char *start;
	char *oout = out;
	*out = 0;
	if (*s != '\\')
		return out;	//gah, get lost with your corrupt infostrings.

	start = ++s;
	while(1)
	{
		while(s[0] == '\\' && s[1] == '\\')
			s+=2;
		if (s[0] != '\\' && *s)
		{
			s++;
			continue;
		}

		//okay, it terminates here
		isvalue = !isvalue;
		if (isvalue)
		{
			if (strlen(key) == (size_t)(s - start) && !strncmp(start, key, s - start))
			{
				s++;
				while (outsize --> 1)
				{
					if (s[0] == '\\' && s[1] == '\\')
						s++;
					else if (s[0] == '\\' || !s[0])
						break;
					*out++ = *s++;
				}
				*out++ = 0;
				return oout;
			}
		}
		if (*s)
			start = ++s;
		else
			break;
	}
	return oout;
}

#if defined(_MSC_VER) && _MSC_VER < 2015
int Q_vsnprintf(char *buffer, size_t maxlen, const char *format, va_list argptr)
{
	int r = _vsnprintf (buffer, maxlen-1, format, argptr);
	buffer[maxlen-1] = 0;	//make sure its null terminated
	if (r < 0)	//work around dodgy return value. we can't use this to check required length but can check for truncation
		r = maxlen;
	return r;
}
int Q_snprintf(char *buffer, size_t maxlen, const char *format, ...)
{
	int p;
	va_list		argptr;

	va_start (argptr, format);
	p = Q_vsnprintf (buffer, maxlen, format,argptr);
	va_end (argptr);

	return p;
}
#endif

char	*va(const char *format, ...)	//Identical in function to the one in Quake, though I can assure you that I wrote it...
{					//It's not exactly hard, just easy to use, so gets duplicated lots.
	va_list		argptr;
	static char		string[1024];
		
	va_start (argptr, format);
	Q_vsnprintf (string, sizeof(string), format,argptr);
	va_end (argptr);

	return string;	
}

#ifdef _WIN32
// don't use these functions in MSVC8
#if (_MSC_VER < 1400)
int QDECL linuxlike_snprintf(char *buffer, int size, const char *format, ...)
{
#undef _vsnprintf
	int ret;
	va_list		argptr;

	if (size <= 0)
		return 0;
	size--;

	va_start (argptr, format);
	ret = _vsnprintf (buffer,size, format,argptr);
	va_end (argptr);

	buffer[size] = '\0';

	return ret;
}
int QDECL linuxlike_vsnprintf(char *buffer, int size, const char *format, va_list argptr)
{
#undef _vsnprintf
	int ret;

	if (size <= 0)
		return 0;
	size--;

	ret = _vsnprintf (buffer,size, format,argptr);

	buffer[size] = '\0';

	return ret;
}
#else
int VARGS linuxlike_snprintf_vc8(char *buffer, int size, const char *format, ...)
{
	int ret;
	va_list		argptr;

	va_start (argptr, format);
	ret = vsnprintf_s (buffer,size, _TRUNCATE, format,argptr);
	va_end (argptr);

	return ret;
}
#endif
#endif

void Con_Printf(const char *format, ...)
{
	va_list		argptr;
	static char		string[1024];
		
	va_start (argptr, format);
	Q_vsnprintf (string, sizeof(string), format,argptr);
	va_end (argptr);

	plugfuncs->Print(string);
}
void Con_DPrintf(const char *format, ...)
{
	va_list		argptr;
	static char		string[1024];

	if (!cvarfuncs->GetFloat("developer"))
		return;
		
	va_start (argptr, format);
	Q_vsnprintf (string, sizeof(string), format,argptr);
	va_end (argptr);

	plugfuncs->Print(string);
}
void Sys_Errorf(const char *format, ...)
{
	va_list		argptr;
	static char		string[1024];
		
	va_start (argptr, format);
	Q_vsnprintf (string, sizeof(string), format,argptr);
	va_end (argptr);

	plugfuncs->Error(string);
}

#ifdef __cplusplus
extern "C"
#endif
qboolean NATIVEEXPORT FTEPlug_Init(plugcorefuncs_t *corefuncs)
{
	plugfuncs = corefuncs;
	cmdfuncs = (plugcmdfuncs_t*)plugfuncs->GetEngineInterface(plugcmdfuncs_name, sizeof(*cmdfuncs));
	cvarfuncs = (plugcvarfuncs_t*)plugfuncs->GetEngineInterface(plugcvarfuncs_name, sizeof(*cvarfuncs));

	return Plug_Init();
}

