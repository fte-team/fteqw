/* An implementation of some 'standard' functions */

/* We use this with msvc too, because msvc's implementations of
   _snprintf and _vsnprint differ - besides, this way we can make 
   sure qvms handle all the printf stuff that dlls do*/
#include "plugin.h"

/*
this is a fairly basic implementation.
don't expect it to do much.
You can probably get a better version from somewhere.
*/
int Q_vsnprintf(char *buffer, size_t maxlen, const char *format, va_list vargs)
{
	int tokens=0;
	char *string;
	char tempbuffer[64];
	char sign;
	unsigned int _uint;
	int _int;
	float _float;
	int i;
	int use0s;
	int width, useprepad, plus;
	int precision;

	if (!maxlen)
		return 0;
maxlen--;

	while(*format)
	{
		switch(*format)
		{
		case '%':
			plus = 0;
			width= 0;
			precision=-1;
			useprepad=0;
			use0s= 0;
retry:
			switch(*(++format))
			{
			case '-':
				useprepad=true;
				goto retry;
			case '+':
				plus = true;
				goto retry;
			case '.':
				precision = 0;
				while (format[1] >= '0' && format[1] <= '9')
					precision = precision*10+*++format-'0';
				goto retry;
			case '0':
				if (!width)
				{
					use0s=true;
					goto retry;
				}
			case '1':
			case '2':
			case '3':
			case '4':
			case '5':
			case '6':
			case '7':
			case '8':
			case '9':
				width=width*10+*format-'0';
				goto retry;
			case '%':	/*emit a %*/
				if (maxlen-- == 0) 
					{*buffer++='\0';return tokens;}
				*buffer++ = *format;
				break;
			case 's':
				string = va_arg(vargs, char *);
				if (!string)
					string = "(null)";
				if (width)
				{
					while (*string && width--)
					{
						if (maxlen-- == 0) 
							{*buffer++='\0';return tokens;}
						*buffer++ = *string++;
					}
				}
				else
				{
					while (*string)
					{
						if (maxlen-- == 0) 
							{*buffer++='\0';return tokens;}
						*buffer++ = *string++;
					}
				}
				tokens++;
				break;
			case 'c':
				_int = va_arg(vargs, int);
				if (maxlen-- == 0) 
					{*buffer++='\0';return tokens;}
				*buffer++ = _int;
				tokens++;
				break;
			case 'p':
				if (1)
				_uint = (size_t)va_arg(vargs, void*);
				else
			case 'x':
				_uint = va_arg(vargs, unsigned int);
				i = sizeof(tempbuffer)-2;
				tempbuffer[i+1] = '\0';
				while(_uint)
				{
					tempbuffer[i] = (_uint&0xf) + '0';
					if (tempbuffer[i] > '9')
						tempbuffer[i] = tempbuffer[i] - ':' + 'a';
					_uint/=16;
					i--;
				}
				string = tempbuffer+i+1;

				if (!*string)
				{
					i=61;
					string = tempbuffer+i+1;
					string[0] = '0';
					string[1] = '\0';
				}

				width -= 62-i;
				while (width>0)
				{
					string--;
					if (use0s)
						*string = '0';
					else
						*string = ' ';
					width--;
				}

				while (*string)
				{
					if (maxlen-- == 0) 
						{*buffer++='\0';return tokens;}
					*buffer++ = *string++;
				}
				tokens++;
				break;
			case 'd':
			case 'u':
			case 'i':
				_int = va_arg(vargs, int);
				if (useprepad)
				{
/*
					if (_int >= 1000)
						useprepad = 4;
					else if (_int >= 100)
						useprepad = 3;
					else if (_int >= 10)
						useprepad = 2;
					else if (_int >= 0)
						useprepad = 1;
					else if (_int <= -1000)
						useprepad = 5;
					else if (_int <= -100)
						useprepad = 4;
					else if (_int <= -10)
						useprepad = 3;
					else
						useprepad = 2;

					useprepad = precision - useprepad;
Con_Printf("add %i chars\n", useprepad);
					while (useprepad>0)
					{
						if (--maxlen < 0) 
							{*buffer++='\0';return tokens;}
						*buffer++ = ' ';
						useprepad--;
					}
Con_Printf("%i bytes left\n", maxlen);
*/
				}
				if (_int < 0)
				{
					sign = '-';
					_int *= -1;
				}
				else if (plus)
					sign = '+';
				else
					sign = 0;
				i = sizeof(tempbuffer)-2;
				tempbuffer[sizeof(tempbuffer)-1] = '\0';
				while(_int)
				{
					tempbuffer[i--] = _int%10 + '0';
					_int/=10;
				}
				if (sign)
					tempbuffer[i--] = sign;
				string = tempbuffer+i+1;

				if (!*string)
				{
					i=61;
					string = tempbuffer+i+1;
					string[0] = '0';
					string[1] = '\0';
				}

				width -= 62-i;
/*				while (width>0)
				{
					string--;
					*string = ' ';
					width--;
				}
*/
				while(width>0)
				{
					if (maxlen-- == 0) 
						{*buffer++='\0';return tokens;}
					if (use0s)
						*buffer++ = '0';
					else
						*buffer++ = ' ';
					width--;
				}

				while (*string)
				{
					if (maxlen-- == 0) 
						{*buffer++='\0';return tokens;}
					*buffer++ = *string++;
				}
				tokens++;
				break;
			case 'f':
				_float = (float)va_arg(vargs, double);

//integer part.
				_int = (int)_float;
				if (_int < 0)
				{
					if (maxlen-- == 0) 
						{*buffer++='\0';return tokens;}
					*buffer++ = '-';
					_int *= -1;
				}
				i = sizeof(tempbuffer)-2;
				tempbuffer[sizeof(tempbuffer)-1] = '\0';
				if (!_int)
				{
					tempbuffer[i--] = '0';
				}
				else
				{
					while(_int)
					{
						tempbuffer[i--] = _int%10 + '0';
						_int/=10;
					}
				}
				string = tempbuffer+i+1;
				while (*string)
				{
					if (maxlen-- == 0) 
						{*buffer++='\0';return tokens;}
					*buffer++ = *string++;
				}

				_int = sizeof(tempbuffer)-2-i;

//floating point part.
				_float -= (int)_float;
				i = 0;
				tempbuffer[i++] = '.';
				if (precision < 0)
					precision = 7;
				while(_float - (int)_float)
				{
					if (i > precision)	//remove the excess presision.
						break;

					_float*=10;
					tempbuffer[i++] = (int)_float%10 + '0';
				}
				if (i == 1)	//no actual fractional part
				{
					tokens++;
					break;
				}

				//concatinate to our string
				tempbuffer[i] = '\0';
				string = tempbuffer;
				while (*string)
				{
					if (maxlen-- == 0) 
						{*buffer++='\0';return tokens;}
					*buffer++ = *string++;
				}

				tokens++;
				break;
			default:
				string = "ERROR IN FORMAT";
				while (*string)
				{
					if (maxlen-- == 0) 
						{*buffer++='\0';return tokens;}
					*buffer++ = *string++;
				}
				break;
			}
			break;
		default:
			if (maxlen-- == 0) 
				{*buffer++='\0';return tokens;}
			*buffer++ = *format;
			break;
		}
		format++;
	}
	{*buffer++='\0';return tokens;}
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

#ifdef Q3_VM
//libc functions that are actually properly supported on all other platforms (c89)
int strlen(const char *s)
{
	int len = 0;
	while(*s++)
		len++;
	return len;
}

int strncmp (const char *s1, const char *s2, int count)
{
	while (1)
	{
		if (!count--)
			return 0;
		if (*s1 != *s2)
			return -1;		// strings not equal	
		if (!*s1)
			return 0;		// strings are equal
		s1++;
		s2++;
	}
	
	return -1;
}

int strnicmp(const char *s1, const char *s2, int count)
{
	char c1, c2;
	char ct;
	while(*s1)
	{
		if (!count--)
			return 0;
		c1 = *s1;
		c2 = *s2;
		if (c1 != c2)
		{
			if (c1 >= 'a' && c1 <= 'z') c1 = c1-'a' + 'A';
			if (c2 >= 'a' && c2 <= 'z') c2 = c2-'a' + 'A';
			if (c1 != c2)
				return c1<c2?-1:1;
		}
		s1++;
		s2++;
	}
	if (*s2)	//s2 was longer.
		return 1;
	return 0;
}

int strcmp(const char *s1, const char *s2)
{
	while(*s1)
	{
		if (*s1 != *s2)
			return *s1<*s2?-1:1;
		s1++;
		s2++;
	}
	if (*s2)	//s2 was longer.
		return 1;
	return 0;
}

int stricmp(const char *s1, const char *s2)
{
	char c1, c2;
	char ct;
	while(*s1)
	{
		c1 = *s1;
		c2 = *s2;
		if (c1 != c2)
		{
			if (c1 >= 'a' && c1 <= 'z') c1 = c1-'a' + 'A';
			if (c2 >= 'a' && c2 <= 'z') c2 = c2-'a' + 'A';
			if (c1 != c2)
				return c1<c2?-1:1;
		}
		s1++;
		s2++;
	}
	if (*s2)	//s2 was longer.
		return 1;
	return 0;
}

char *strstr(char *str, const char *sub)
{
	char *p;
	char *p2;
	int l = strlen(sub)-1;
	if (l < 0)
		return NULL;

	while(*str)
	{
		if (*str == *sub)
		{
			if (!strncmp (str+1, sub+1, l))
				return str;
		}	
		str++;
	}

	return NULL;
}
char *strchr(char *str, char sub)
{
	char *p;
	char *p2;

	while(*str)
	{
		if (*str == sub)
			return str;
		str++;
	}

	return NULL;
}

int atoi(char *str)
{
	int sign;
	int num = 0;
	int base = 10;

	while(*(unsigned char*)str < ' ' && *str)
		str++;

	if (*str == '-')
	{
		sign = -1;
		str++;
	}
	else sign = 1;

	if (str[0] == '0' && (str[1] == 'x' || str[1] == 'X'))
	{
		base = 16;
		str += 2;
	}

	while(1)
	{
		if (*str >= '0' && *str <= '9') 
			num = num*base + (*str - '0');
		else if (*str >= 'a' && *str <= 'a'+base-10)
			num = num*base + (*str - 'a')+10;
		else if (*str >= 'A' && *str <= 'A'+base-10)
			num = num*base + (*str - 'A')+10;
		else break;	//bad char
		str++;
	}	
	return num*sign;
}

float atof(char *str)
{
	int sign;
	float num = 0.0f;
	float unit = 1;

	while(*(unsigned char*)str < ' ' && *str)
		str++;

	if (*str == '-')
	{
		sign = -1;
		str++;
	}
	else sign = 1;

	while(1)
	{//each time we find a new digit, increase the value of the previous digets by a factor of ten, and add the new
		if (*str >= '0' && *str <= '9') 
			num = num*10 + (*str - '0');
		else break;	//bad char
		str++;
	}
	if (*str == '.')
	{	//each time we find a new digit, decrease the value of the following digits.
		str++;
		while(1)
		{
			if (*str >= '0' && *str <= '9')
			{
				unit /= 10;
				num = num + (*str - '0')*unit;
			}
			else break;	//bad char
			str++;
		}
	}
	return num*sign;
}

void strcpy(char *d, const char *s)
{
	while (*s)
	{
		*d++ = *s++;
	}
	*d='\0';
}

static	long	randx = 1;
void srand(unsigned int x)
{
	randx = x;
}
int getseed(void)
{
	return randx;
}
int rand(void)
{
	return(((randx = randx*1103515245 + 12345)>>16) & 077777);
}
#endif

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
			if (strlen(key) == s - start && !strncmp(start, key, s - start))
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
