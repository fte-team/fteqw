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
int vsnprintf(char *buffer, int maxlen, char *format, va_list vargs)
{
	int tokens=0;
	char *string;
	char tempbuffer[64];
	int _int;
	float _float;
	int i;
	int use0s;
	int precision;

	if (!maxlen)
		return 0;
maxlen--;

	while(*format)
	{
		switch(*format)
		{
		case '%':
			precision= 0;
			use0s=0;
retry:
			switch(*(++format))
			{
			case '0':
				if (!precision)
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
				precision=precision*10+*format-'0';
				goto retry;
			case '%':	/*emit a %*/
				if (--maxlen < 0) 
					{*buffer++='\0';return tokens;}
				*buffer++ = *format;
				break;
			case 's':
				string = va_arg(vargs, char *);
				if (!string)
					string = "(null)";
				if (precision)
				{
					while (*string && precision--)
					{
						if (--maxlen < 0) 
							{*buffer++='\0';return tokens;}
						*buffer++ = *string++;
					}
				}
				else
				{
					while (*string)
					{
						if (--maxlen < 0) 
							{*buffer++='\0';return tokens;}
						*buffer++ = *string++;
					}
				}
				tokens++;
				break;
			case 'c':
				_int = va_arg(vargs, char);
				if (--maxlen < 0) 
					{*buffer++='\0';return tokens;}
				*buffer++ = _int;
				tokens++;
				break;
			case 'd':
			case 'i':
				_int = va_arg(vargs, int);
				if (_int < 0)
				{
					if (--maxlen < 0) 
						{*buffer++='\0';return tokens;}
					*buffer++ = '-';
					_int *= -1;
				}
				i = sizeof(tempbuffer)-2;
				tempbuffer[sizeof(tempbuffer)-1] = '\0';
				while(_int)
				{
					tempbuffer[i] = _int%10 + '0';
					_int/=10;
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

				precision -= 62-i;
				while (precision>0)
				{
					string--;
					if (use0s)
						*string = '0';
					else
						*string = ' ';
					precision--;
				}

				while (*string)
				{
					if (--maxlen < 0) 
						{*buffer++='\0';return tokens;}
					*buffer++ = *string++;
				}
				tokens++;
				break;
			case 'f':
				_float = va_arg(vargs, float);

//integer part.
				_int = (int)_float;
				if (_int < 0)
				{
					if (--maxlen < 0) 
						{*buffer++='\0';return tokens;}
					*buffer++ = '-';
					_int *= -1;
				}
				i = sizeof(tempbuffer)-2;
				tempbuffer[sizeof(tempbuffer)-1] = '\0';
				while(_int)
				{
					tempbuffer[i--] = _int%10 + '0';
					_int/=10;
				}
				string = tempbuffer+i+1;
				while (*string)
				{
					if (--maxlen < 0) 
						{*buffer++='\0';return tokens;}
					*buffer++ = *string++;
				}

				_int = sizeof(tempbuffer)-2-i;

//floating point part.
				_float -= (int)_float;
				i = 0;
				tempbuffer[i++] = '.';
				while(_float - (int)_float)
				{
					if (i + _int > 7)	//remove the excess presision.
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
					if (--maxlen < 0) 
						{*buffer++='\0';return tokens;}
					*buffer++ = *string++;
				}

				tokens++;
				break;
			default:
				string = "ERROR IN FORMAT";
				while (*string)
				{
					if (--maxlen < 0) 
						{*buffer++='\0';return tokens;}
					*buffer++ = *string++;
				}
				break;
			}
			break;
		default:
			if (--maxlen < 0) 
				{*buffer++='\0';return tokens;}
			*buffer++ = *format;
			break;
		}
		format++;
	}
	{*buffer++='\0';return tokens;}
}

int snprintf(char *buffer, int maxlen, char *format, ...)
{
	int p;
	va_list		argptr;
		
	va_start (argptr, format);
	p = vsnprintf (buffer, maxlen, format,argptr);
	va_end (argptr);

	return p;
}

#ifdef Q3_VM
int strlen(const char *s)
{
	int len = 0;
	while(*s++)
		len++;
	return len;
}

int strncmp (char *s1, char *s2, int count)
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

int strcmp(char *s1, char *s2)
{
	while(*s1)
	{
		if (*s1 != *s2)
			return *s1<*s1?-1:1;
		s1++;
		s2++;
	}
	if (*s2)	//s2 was longer.
		return 1;
	return 0;
}

char *strstr(char *str, char *sub)
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
#endif
