/*
Copyright (C) 1996-1997 Id Software, Inc.

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
// common.c -- misc functions used in client and server

#include <ctype.h>

#ifdef SERVERONLY 
#include "qwsvdef.h"
#else
#include "quakedef.h"
#endif






#define ONE_FS_HASH	//cause all filesystems files to be hased in one group.
					//this is to prevent us from going into each directory and fopening and crazy stuff like that.


//#define HASH_FILESYSTEM

#if defined(HASH_FILESYSTEM) || defined(ONE_FS_HASH)
//#include "hash.h"

#define Hash_BytesForBuckets(b) (sizeof(bucket_t)*b)

#define STRCMP(s1,s2) (((*s1)!=(*s2)) || strcmp(s1+1,s2+1))	//saves about 2-6 out of 120 - expansion of idea from fastqcc
typedef struct bucket_s {
	void *data;
	char *keystring;
	struct bucket_s *next;
} bucket_t;
typedef struct hashtable_s {
	int numbuckets;
	bucket_t **bucket;
} hashtable_t;

void Hash_InitTable(hashtable_t *table, int numbucks, void *mem);	//mem must be 0 filled. (memset(mem, 0, size))
int Hash_Key(char *name, int modulus);
void *Hash_Get(hashtable_t *table, char *name);
void *Hash_GetKey(hashtable_t *table, int key);
void *Hash_GetNext(hashtable_t *table, char *name, void *old);
void *Hash_Add(hashtable_t *table, char *name, void *data);
void *Hash_Add2(hashtable_t *table, char *name, void *data, bucket_t *buck);
void *Hash_AddKey(hashtable_t *table, int key, void *data);
void Hash_Remove(hashtable_t *table, char *name);
#endif





#ifdef ONE_FS_HASH
hashtable_t filesystemhash;
qboolean com_fschanged = true;

extern cvar_t com_fs_cache;

int active_fs_cachetype;
#endif





#undef malloc
#undef free


#define NUM_SAFE_ARGVS	6

usercmd_t nullcmd; // guarenteed to be zero

static char	*largv[MAX_NUM_ARGVS + NUM_SAFE_ARGVS + 1];
static char	*argvdummy = " ";

static char	*safeargvs[NUM_SAFE_ARGVS] =
	{"-stdvid", "-nolan", "-nosound", "-nocdaudio", "-nojoy", "-nomouse"};

cvar_t	registered = {"registered","0"};

qboolean	com_modified;	// set true if using non-id files

int		static_registered = 1;	// only for startup check, then set

qboolean		msg_suppress_1 = 0;

void COM_InitFilesystem (void);
void COM_Path_f (void);


// if a packfile directory differs from this, it is assumed to be hacked
#define	PAK0_COUNT		339
#define	PAK0_CRC		52883

qboolean		standard_quake = true, rogue, hipnotic;

char	gamedirfile[MAX_OSPATH];

// this graphic needs to be in the pak file to use registered features
unsigned short pop[] =
{
 0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000
,0x0000,0x0000,0x6600,0x0000,0x0000,0x0000,0x6600,0x0000
,0x0000,0x0066,0x0000,0x0000,0x0000,0x0000,0x0067,0x0000
,0x0000,0x6665,0x0000,0x0000,0x0000,0x0000,0x0065,0x6600
,0x0063,0x6561,0x0000,0x0000,0x0000,0x0000,0x0061,0x6563
,0x0064,0x6561,0x0000,0x0000,0x0000,0x0000,0x0061,0x6564
,0x0064,0x6564,0x0000,0x6469,0x6969,0x6400,0x0064,0x6564
,0x0063,0x6568,0x6200,0x0064,0x6864,0x0000,0x6268,0x6563
,0x0000,0x6567,0x6963,0x0064,0x6764,0x0063,0x6967,0x6500
,0x0000,0x6266,0x6769,0x6a68,0x6768,0x6a69,0x6766,0x6200
,0x0000,0x0062,0x6566,0x6666,0x6666,0x6666,0x6562,0x0000
,0x0000,0x0000,0x0062,0x6364,0x6664,0x6362,0x0000,0x0000
,0x0000,0x0000,0x0000,0x0062,0x6662,0x0000,0x0000,0x0000
,0x0000,0x0000,0x0000,0x0061,0x6661,0x0000,0x0000,0x0000
,0x0000,0x0000,0x0000,0x0000,0x6500,0x0000,0x0000,0x0000
,0x0000,0x0000,0x0000,0x0000,0x6400,0x0000,0x0000,0x0000
};

/*


All of Quake's data access is through a hierchal file system, but the contents of the file system can be transparently merged from several sources.

The "base directory" is the path to the directory holding the quake.exe and all game directories.  The sys_* files pass this to host_init in quakeparms_t->basedir.  This can be overridden with the "-basedir" command line parm to allow code debugging in a different directory.  The base directory is
only used during filesystem initialization.

The "game directory" is the first tree on the search path and directory that all generated files (savegames, screenshots, demos, config files) will be saved to.  This can be overridden with the "-game" command line parameter.  The game directory can never be changed while quake is executing.  This is a precacution against having a malicious server instruct clients to write files over areas they shouldn't.

The "cache directory" is only used during development to save network bandwidth, especially over ISDN / T1 lines.  If there is a cache directory
specified, when a file is found by the normal search path, it will be mirrored
into the cache directory, then opened there.
	
*/

//============================================================================


// ClearLink is used for new headnodes
void ClearLink (link_t *l)
{
	l->prev = l->next = l;
}

void RemoveLink (link_t *l)
{
	l->next->prev = l->prev;
	l->prev->next = l->next;
}

void InsertLinkBefore (link_t *l, link_t *before)
{
	l->next = before;
	l->prev = before->prev;
	l->prev->next = l;
	l->next->prev = l;
}
void InsertLinkAfter (link_t *l, link_t *after)
{
	l->next = after->next;
	l->prev = after;
	l->prev->next = l;
	l->next->prev = l;
}

/*
============================================================================

					LIBRARY REPLACEMENT FUNCTIONS

============================================================================
*/

#if 0
void Q_memset (void *dest, int fill, int count)
{
	int		i;
	
	if ( (((long)dest | count) & 3) == 0)
	{
		count >>= 2;
		fill = fill | (fill<<8) | (fill<<16) | (fill<<24);
		for (i=0 ; i<count ; i++)
			((int *)dest)[i] = fill;
	}
	else
		for (i=0 ; i<count ; i++)
			((qbyte *)dest)[i] = fill;
}

void Q_memcpy (void *dest, void *src, int count)
{
	int		i;
	
	if (( ( (long)dest | (long)src | count) & 3) == 0 )
	{
		count>>=2;
		for (i=0 ; i<count ; i++)
			((int *)dest)[i] = ((int *)src)[i];
	}
	else
		for (i=0 ; i<count ; i++)
			((qbyte *)dest)[i] = ((qbyte *)src)[i];
}

int Q_memcmp (void *m1, void *m2, int count)
{
	while(count)
	{
		count--;
		if (((qbyte *)m1)[count] != ((qbyte *)m2)[count])
			return -1;
	}
	return 0;
}

void Q_strcpy (char *dest, char *src)
{
	while (*src)
	{
		*dest++ = *src++;
	}
	*dest++ = 0;
}

void Q_strncpy (char *dest, char *src, int count)
{
	while (*src && count--)
	{
		*dest++ = *src++;
	}
	if (count)
		*dest++ = 0;
}

int Q_strlen (char *str)
{
	int		count;
	
	count = 0;
	while (str[count])
		count++;

	return count;
}

char *Q_strrchr(char *s, char c)
{
    int len = Q_strlen(s);
    s += len;
    while (len--)
        if (*--s == c) return s;
    return 0;
}

void Q_strcat (char *dest, char *src)
{
	dest += Q_strlen(dest);
	Q_strcpy (dest, src);
}

int Q_strcmp (char *s1, char *s2)
{
	while (1)
	{
		if (*s1 != *s2)
			return -1;		// strings not equal	
		if (!*s1)
			return 0;		// strings are equal
		s1++;
		s2++;
	}
	
	return -1;
}

int Q_strncmp (char *s1, char *s2, int count)
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

int Q_strncasecmp (char *s1, char *s2, int n)
{
	int		c1, c2;
	
	while (1)
	{
		c1 = *s1++;
		c2 = *s2++;

		if (!n--)
			return 0;		// strings are equal until end point
		
		if (c1 != c2)
		{
			if (c1 >= 'a' && c1 <= 'z')
				c1 -= ('a' - 'A');
			if (c2 >= 'a' && c2 <= 'z')
				c2 -= ('a' - 'A');
			if (c1 != c2)
				return -1;		// strings not equal
		}
		if (!c1)
			return 0;		// strings are equal
//		s1++;
//		s2++;
	}
	
	return -1;
}

int Q_strcasecmp (char *s1, char *s2)
{
	return Q_strncasecmp (s1, s2, 99999);
}

#endif

char *Q_strlwr(char *s)
{
	char *ret=s;
	while(*s)
	{
		if (*s >= 'A' && *s <= 'Z')
			*s=*s-'A'+'a';
		s++;
	}

	return ret;
}
	

int Q_strwildcmp(char *s1, char *s2)	//2 is wild
{
	while (1)
	{
		if (!*s1 && !*s2)
			return 0;	//yay, they are equivalent
		if (!*s1 || !*s2)
			return -1;	//one ended too soon.

		if (*s2 == '*')
		{
			s2++;
			while (1)
			{
				if (!*s1 && *s2)
					return -1;
				if (*s1 == '.' || *s1 == '/' || !*s1)
					break;
				s1++;
			}			
		}
		else if (*s2 != *s1 && *s2 != '?')			
			return -1;
		else
		{
			s1++;
			s2++;
		}
	}
}

int Q_atoi (char *str)
{
	int		val;
	int		sign;
	int		c;
	
	if (*str == '-')
	{
		sign = -1;
		str++;
	}
	else
		sign = 1;
		
	val = 0;

//
// check for hex
//
	if (str[0] == '0' && (str[1] == 'x' || str[1] == 'X') )
	{
		str += 2;
		while (1)
		{
			c = *str++;
			if (c >= '0' && c <= '9')
				val = (val<<4) + c - '0';
			else if (c >= 'a' && c <= 'f')
				val = (val<<4) + c - 'a' + 10;
			else if (c >= 'A' && c <= 'F')
				val = (val<<4) + c - 'A' + 10;
			else
				return val*sign;
		}
	}
	
//
// check for character
//
	if (str[0] == '\'')
	{
		return sign * str[1];
	}
	
//
// assume decimal
//
	while (1)
	{
		c = *str++;
		if (c <'0' || c > '9')
			return val*sign;
		val = val*10 + c - '0';
	}
	
	return 0;
}


float Q_atof (char *str)
{
	double	val;
	int		sign;
	int		c;
	int		decimal, total;
	
	if (*str == '-')
	{
		sign = -1;
		str++;
	}
	else
		sign = 1;
		
	val = 0;

//
// check for hex
//
	if (str[0] == '0' && (str[1] == 'x' || str[1] == 'X') )
	{
		str += 2;
		while (1)
		{
			c = *str++;
			if (c >= '0' && c <= '9')
				val = (val*16) + c - '0';
			else if (c >= 'a' && c <= 'f')
				val = (val*16) + c - 'a' + 10;
			else if (c >= 'A' && c <= 'F')
				val = (val*16) + c - 'A' + 10;
			else
				return val*sign;
		}
	}
	
//
// check for character
//
	if (str[0] == '\'')
	{
		return sign * str[1];
	}
	
//
// assume decimal
//
	decimal = -1;
	total = 0;
	while (1)
	{
		c = *str++;
		if (c == '.')
		{
			decimal = total;
			continue;
		}
		if (c <'0' || c > '9')
			break;
		val = val*10 + c - '0';
		total++;
	}

	if (decimal == -1)
		return val*sign;
	while (total > decimal)
	{
		val /= 10;
		total--;
	}
	
	return val*sign;
}

/*
============================================================================

					qbyte ORDER FUNCTIONS

============================================================================
*/

qboolean	bigendien;

short	(*BigShort) (short l);
short	(*LittleShort) (short l);
int	(*BigLong) (int l);
int	(*LittleLong) (int l);
float	(*BigFloat) (float l);
float	(*LittleFloat) (float l);

short   ShortSwap (short l)
{
	qbyte    b1,b2;

	b1 = l&255;
	b2 = (l>>8)&255;

	return (b1<<8) + b2;
}

short	ShortNoSwap (short l)
{
	return l;
}

int    LongSwap (int l)
{
	qbyte    b1,b2,b3,b4;

	b1 = l&255;
	b2 = (l>>8)&255;
	b3 = (l>>16)&255;
	b4 = (l>>24)&255;

	return ((int)b1<<24) + ((int)b2<<16) + ((int)b3<<8) + b4;
}

int	LongNoSwap (int l)
{
	return l;
}

float FloatSwap (float f)
{
	union
	{
		float	f;
		qbyte	b[4];
	} dat1, dat2;
	
	
	dat1.f = f;
	dat2.b[0] = dat1.b[3];
	dat2.b[1] = dat1.b[2];
	dat2.b[2] = dat1.b[1];
	dat2.b[3] = dat1.b[0];
	return dat2.f;
}

float FloatNoSwap (float f)
{
	return f;
}

/*
==============================================================================

			MESSAGE IO FUNCTIONS

Handles qbyte ordering and avoids alignment errors
==============================================================================
*/

//
// writing functions
//

void MSG_WriteChar (sizebuf_t *sb, int c)
{
	qbyte	*buf;
	
#ifdef PARANOID
	if (c < -128 || c > 127)
		Sys_Error ("MSG_WriteChar: range error");
#endif

	buf = SZ_GetSpace (sb, 1);
	buf[0] = c;
}

void MSG_WriteByte (sizebuf_t *sb, int c)
{
	qbyte	*buf;
	
#ifdef PARANOID
	if (c < 0 || c > 255)
		Sys_Error ("MSG_WriteByte: range error");
#endif

	buf = SZ_GetSpace (sb, 1);
	buf[0] = c;
}

void MSG_WriteShort (sizebuf_t *sb, int c)
{
	qbyte	*buf;
	
#ifdef PARANOID
	if (c < ((short)0x8000) || c > (short)0x7fff)
		Sys_Error ("MSG_WriteShort: range error");
#endif

	buf = SZ_GetSpace (sb, 2);
	buf[0] = c&0xff;
	buf[1] = c>>8;
}

void MSG_WriteLong (sizebuf_t *sb, int c)
{
	qbyte	*buf;
	
	buf = SZ_GetSpace (sb, 4);
	buf[0] = c&0xff;
	buf[1] = (c>>8)&0xff;
	buf[2] = (c>>16)&0xff;
	buf[3] = c>>24;
}

void MSG_WriteFloat (sizebuf_t *sb, float f)
{
	union
	{
		float	f;
		int	l;
	} dat;
	
	
	dat.f = f;
	dat.l = LittleLong (dat.l);
	
	SZ_Write (sb, &dat.l, 4);
}

void MSG_WriteString (sizebuf_t *sb, char *s)
{
	if (!s)
		SZ_Write (sb, "", 1);
	else
		SZ_Write (sb, s, Q_strlen(s)+1);
}

void MSG_WriteCoord (sizebuf_t *sb, float f)
{
	MSG_WriteShort (sb, (int)(f*8));
}
void MSG_WriteBigCoord (sizebuf_t *sb, float f)
{
	int o = f*8;
	MSG_WriteShort (sb, o&0xffff);
	MSG_WriteByte (sb, (o&0xff0000)>>16);
}

void MSG_WriteAngle (sizebuf_t *sb, float f)
{
	MSG_WriteByte (sb, (int)(f*256/360) & 255);
}

void MSG_WriteAngle16 (sizebuf_t *sb, float f)
{
	MSG_WriteShort (sb, (int)(f*65536/360) & 65535);
}

void MSG_WriteDeltaUsercmd (sizebuf_t *buf, usercmd_t *from, usercmd_t *cmd)
{
	int		bits;

//
// send the movement message
//
	bits = 0;
#ifdef Q2CLIENT
	if (cls.q2server)
	{
		if (cmd->angles[0] != from->angles[0])
			bits |= Q2CM_ANGLE1;
		if (cmd->angles[1] != from->angles[1])
			bits |= Q2CM_ANGLE2;
		if (cmd->angles[2] != from->angles[2])
			bits |= Q2CM_ANGLE3;
		if (cmd->forwardmove != from->forwardmove)
			bits |= Q2CM_FORWARD;
		if (cmd->sidemove != from->sidemove)
			bits |= Q2CM_SIDE;
		if (cmd->upmove != from->upmove)
			bits |= Q2CM_UP;
		if (cmd->buttons != from->buttons)
			bits |= Q2CM_BUTTONS;
		if (cmd->impulse != from->impulse)
			bits |= Q2CM_IMPULSE;

		MSG_WriteByte (buf, bits);

		if (bits & Q2CM_ANGLE1)
			MSG_WriteShort (buf, cmd->angles[0]);
		if (bits & Q2CM_ANGLE2)
			MSG_WriteShort (buf, cmd->angles[1]);
		if (bits & Q2CM_ANGLE3)
			MSG_WriteShort (buf, cmd->angles[2]);
		
		if (bits & Q2CM_FORWARD)
			MSG_WriteShort (buf, cmd->forwardmove);
		if (bits & Q2CM_SIDE)
	  		MSG_WriteShort (buf, cmd->sidemove);
		if (bits & Q2CM_UP)
			MSG_WriteShort (buf, cmd->upmove);

 		if (bits & Q2CM_BUTTONS)
	  		MSG_WriteByte (buf, cmd->buttons);
 		if (bits & Q2CM_IMPULSE)
			MSG_WriteByte (buf, cmd->impulse);
		MSG_WriteByte (buf, cmd->msec);

		MSG_WriteByte (buf, cmd->lightlevel);

	}
	else
#endif
	{
		if (cmd->angles[0] != from->angles[0])
			bits |= CM_ANGLE1;
		if (cmd->angles[1] != from->angles[1])
			bits |= CM_ANGLE2;
		if (cmd->angles[2] != from->angles[2])
			bits |= CM_ANGLE3;
		if (cmd->forwardmove != from->forwardmove)
			bits |= CM_FORWARD;
		if (cmd->sidemove != from->sidemove)
			bits |= CM_SIDE;
		if (cmd->upmove != from->upmove)
			bits |= CM_UP;
		if (cmd->buttons != from->buttons)
			bits |= CM_BUTTONS;
		if (cmd->impulse != from->impulse)
			bits |= CM_IMPULSE;

		MSG_WriteByte (buf, bits);

		if (bits & CM_ANGLE1)
			MSG_WriteShort (buf, cmd->angles[0]);
		if (bits & CM_ANGLE2)
			MSG_WriteShort (buf, cmd->angles[1]);
		if (bits & CM_ANGLE3)
			MSG_WriteShort (buf, cmd->angles[2]);
		
		if (bits & CM_FORWARD)
			MSG_WriteShort (buf, cmd->forwardmove);
		if (bits & CM_SIDE)
	  		MSG_WriteShort (buf, cmd->sidemove);
		if (bits & CM_UP)
			MSG_WriteShort (buf, cmd->upmove);

 		if (bits & CM_BUTTONS)
	  		MSG_WriteByte (buf, cmd->buttons);
 		if (bits & CM_IMPULSE)
			MSG_WriteByte (buf, cmd->impulse);
		MSG_WriteByte (buf, cmd->msec);
	}
}


//
// reading functions
//
int			msg_readcount;
qboolean	msg_badread;

void MSG_BeginReading (void)
{
	msg_readcount = 0;
	msg_badread = false;
}

int MSG_GetReadCount(void)
{
	return msg_readcount;
}

// returns -1 and sets msg_badread if no more characters are available
int MSG_ReadChar (void)
{
	int	c;
	
	if (msg_readcount+1 > net_message.cursize)
	{
		msg_badread = true;
		return -1;
	}
		
	c = (signed char)net_message.data[msg_readcount];
	msg_readcount++;
	
	return c;
}

int MSG_ReadByte (void)
{
	int	c;
	
	if (msg_readcount+1 > net_message.cursize)
	{
		msg_badread = true;
		return -1;
	}
		
	c = (unsigned char)net_message.data[msg_readcount];
	msg_readcount++;
	
	return c;
}

int MSG_ReadShort (void)
{
	int	c;
	
	if (msg_readcount+2 > net_message.cursize)
	{
		msg_badread = true;
		return -1;
	}
		
	c = (short)(net_message.data[msg_readcount]
	+ (net_message.data[msg_readcount+1]<<8));
	
	msg_readcount += 2;
	
	return c;
}

int MSG_ReadLong (void)
{
	int	c;
	
	if (msg_readcount+4 > net_message.cursize)
	{
		msg_badread = true;
		return -1;
	}
		
	c = net_message.data[msg_readcount]
	+ (net_message.data[msg_readcount+1]<<8)
	+ (net_message.data[msg_readcount+2]<<16)
	+ (net_message.data[msg_readcount+3]<<24);
	
	msg_readcount += 4;
	
	return c;
}

float MSG_ReadFloat (void)
{
	union
	{
		qbyte	b[4];
		float	f;
		int	l;
	} dat;

	if (msg_readcount+4 > net_message.cursize)
	{
		msg_badread = true;
		return -1;
	}
	
	dat.b[0] =	net_message.data[msg_readcount];
	dat.b[1] =	net_message.data[msg_readcount+1];
	dat.b[2] =	net_message.data[msg_readcount+2];
	dat.b[3] =	net_message.data[msg_readcount+3];
	msg_readcount += 4;
	
	dat.l = LittleLong (dat.l);

	return dat.f;	
}

char *MSG_ReadString (void)
{
	static char	string[2048];
	int		l,c;
	
	l = 0;
	do
	{
		c = MSG_ReadChar ();
		if (c == -1 || c == 0)
			break;
		string[l] = c;
		l++;
	} while (l < sizeof(string)-1);
	
	string[l] = 0;
	
	return string;
}

char *MSG_ReadStringLine (void)
{
	static char	string[2048];
	int		l,c;
	
	l = 0;
	do
	{
		c = MSG_ReadChar ();
		if (c == -1 || c == 0 || c == '\n')
			break;
		string[l] = c;
		l++;
	} while (l < sizeof(string)-1);
	
	string[l] = 0;
	
	return string;
}

float MSG_ReadCoord (void)
{
	return MSG_ReadShort() * (1.0/8);
}

void MSG_ReadPos (vec3_t pos)
{
	pos[0] = MSG_ReadShort() * (1.0/8);
	pos[1] = MSG_ReadShort() * (1.0/8);
	pos[2] = MSG_ReadShort() * (1.0/8);
}

#define Q2NUMVERTEXNORMALS	162
vec3_t	bytedirs[Q2NUMVERTEXNORMALS] =
{
#include "../client/q2anorms.h"
};
#ifndef SERVERONLY
void MSG_ReadDir (vec3_t dir)
{
	int		b;

	b = MSG_ReadByte ();
	if (b >= Q2NUMVERTEXNORMALS)
		Host_EndGame ("MSG_ReadDir: out of range");
	VectorCopy (bytedirs[b], dir);
}
#endif
void MSG_WriteDir (sizebuf_t *sb, vec3_t dir)
{
	int		i, best;
	float	d, bestd;
	
	if (!dir)
	{
		MSG_WriteByte (sb, 0);
		return;
	}

	bestd = 0;
	best = 0;
	for (i=0 ; i<Q2NUMVERTEXNORMALS ; i++)
	{
		d = DotProduct (dir, bytedirs[i]);
		if (d > bestd)
		{
			bestd = d;
			best = i;
		}
	}
	MSG_WriteByte (sb, best);
}
float MSG_ReadAngle (void)
{
	return MSG_ReadChar() * (360.0/256);
}

float MSG_ReadAngle16 (void)
{
	return MSG_ReadShort() * (360.0/65536);
}

void MSG_ReadDeltaUsercmd (usercmd_t *from, usercmd_t *move)
{
	int bits;

	memcpy (move, from, sizeof(*move));

	bits = MSG_ReadByte ();
		
// read current angles
	if (bits & CM_ANGLE1)
		move->angles[0] = MSG_ReadShort ();
	if (bits & CM_ANGLE2)
		move->angles[1] = MSG_ReadShort ();
	if (bits & CM_ANGLE3)
		move->angles[2] = MSG_ReadShort ();
		
// read movement
	if (bits & CM_FORWARD)
		move->forwardmove = MSG_ReadShort ();
	if (bits & CM_SIDE)
		move->sidemove = MSG_ReadShort ();
	if (bits & CM_UP)
		move->upmove = MSG_ReadShort ();
	
// read buttons
	if (bits & CM_BUTTONS)
		move->buttons = MSG_ReadByte ();

	if (bits & CM_IMPULSE)
		move->impulse = MSG_ReadByte ();

// read time to run command
	move->msec = MSG_ReadByte ();
}

void MSGQ2_ReadDeltaUsercmd (usercmd_t *from, usercmd_t *move)
{
	int bits;

	memcpy (move, from, sizeof(*move));

	bits = MSG_ReadByte ();
		
// read current angles
	if (bits & Q2CM_ANGLE1)
		move->angles[0] = MSG_ReadShort ();
	if (bits & Q2CM_ANGLE2)
		move->angles[1] = MSG_ReadShort ();
	if (bits & Q2CM_ANGLE3)
		move->angles[2] = MSG_ReadShort ();
		
// read movement
	if (bits & Q2CM_FORWARD)
		move->forwardmove = MSG_ReadShort ();
	if (bits & Q2CM_SIDE)
		move->sidemove = MSG_ReadShort ();
	if (bits & Q2CM_UP)
		move->upmove = MSG_ReadShort ();
	
// read buttons
	if (bits & Q2CM_BUTTONS)
		move->buttons = MSG_ReadByte ();

	if (bits & Q2CM_IMPULSE)
		move->impulse = MSG_ReadByte ();

// read time to run command
	move->msec = MSG_ReadByte ();

	move->lightlevel = MSG_ReadByte ();
}

void MSG_ReadData (void *data, int len)
{
	int		i;

	for (i=0 ; i<len ; i++)
		((qbyte *)data)[i] = MSG_ReadByte ();
}


//===========================================================================

void SZ_Clear (sizebuf_t *buf)
{
	buf->cursize = 0;
	buf->overflowed = false;
}

void *SZ_GetSpace (sizebuf_t *buf, int length)
{
	void	*data;
	
	if (buf->cursize + length > buf->maxsize)
	{
		if (!buf->allowoverflow)
			Sys_Error ("SZ_GetSpace: overflow without allowoverflow set (%d)", buf->maxsize);
		
		if (length > buf->maxsize)
			Sys_Error ("SZ_GetSpace: %i is > full buffer size", length);
			
		Sys_Printf ("SZ_GetSpace: overflow\n");	// because Con_Printf may be redirected
		SZ_Clear (buf); 
		buf->overflowed = true;
	}

	data = buf->data + buf->cursize;
	buf->cursize += length;
	
	return data;
}

void SZ_Write (sizebuf_t *buf, const void *data, int length)
{
	Q_memcpy (SZ_GetSpace(buf,length),data,length);		
}

void SZ_Print (sizebuf_t *buf, const char *data)
{
	int		len;
	
	len = Q_strlen(data)+1;

	if (!buf->cursize || buf->data[buf->cursize-1])
		Q_memcpy ((qbyte *)SZ_GetSpace(buf, len),data,len); // no trailing 0
	else
	{
		qbyte *msg;
		msg = SZ_GetSpace(buf, len-1);
		if (msg == buf->data)	//whoops. SZ_GetSpace can return buf->data if it overflowed.
			msg++;
		Q_memcpy (msg-1,data,len); // write over trailing 0
	}
}


//============================================================================


/*
============
COM_SkipPath
============
*/
char *COM_SkipPath (char *pathname)
{
	char	*last;
	
	last = pathname;
	while (*pathname)
	{
		if (*pathname=='/')
			last = pathname+1;
		pathname++;
	}
	return last;
}

/*
============
COM_StripExtension
============
*/
void COM_StripExtension (char *in, char *out)
{
	while (*in && *in != '.')
		*out++ = *in++;
	*out = 0;
}

/*
============
COM_FileExtension
============
*/
char *COM_FileExtension (char *in)
{
	static char exten[8];
	int		i;

	while (*in && *in != '.')
		in++;
	if (!*in)
		return "";
	in++;
	for (i=0 ; i<7 && *in ; i++,in++)
		exten[i] = *in;
	exten[i] = 0;
	return exten;
}

/*
============
COM_FileBase
============
*/
void COM_FileBase (char *in, char *out)
{
	char *s, *s2;
	
	s = in + strlen(in) - 1;
	
	while (s != in && *s != '.')
		s--;
	
	for (s2 = s ; *s2 && *s2 != '/' ; s2--)
	;

	if (in > s2)
		s2 = in;
	
	if (s-s2 < 2)
		strcpy (out,"?model?");
	else
	{
		s--;
		Q_strncpyS (out,s2+1, s-s2);
		out[s-s2] = 0;
	}
}


/*
==================
COM_DefaultExtension
==================
*/
void COM_DefaultExtension (char *path, char *extension)
{
	char    *src;
//
// if path doesn't have a .EXT, append extension
// (extension should include the .)
//
	src = path + strlen(path) - 1;

	while (*src != '/' && src != path)
	{
		if (*src == '.')
			return;                 // it has an extension
		src--;
	}

	strcat (path, extension);
}

//============================================================================

#define TOKENSIZE sizeof(com_token)
char		com_token[TOKENSIZE];
int		com_argc;
char	**com_argv;

com_tokentype_t com_tokentype;


/*
==============
COM_Parse

Parse a token out of a string
==============
*/
char *COM_Parse (char *data)
{
	int		c;
	int		len;
	
	len = 0;
	com_token[0] = 0;
	
	if (!data)
		return NULL;
		
// skip whitespace
skipwhite:
	while ( (c = *data) <= ' ')
	{
		if (c == 0)
			return NULL;			// end of file;
		data++;
	}
	
// skip // comments
	if (c=='/')
	{
		if (data[1] == '/')
		{
			while (*data && *data != '\n')
				data++;
			goto skipwhite;
		}
	}
	

// handle quoted strings specially
	if (c == '\"')
	{
		data++;
		while (1)
		{
			if (len >= TOKENSIZE-1)
				return data;

			c = *data++;
			if (c=='\"' || !c)
			{
				com_token[len] = 0;
				return data;
			}
			com_token[len] = c;
			len++;
		}
	}

// parse a regular word
	do
	{
		if (len >= TOKENSIZE-1)
			return data;

		com_token[len] = c;
		data++;
		len++;
		c = *data;
	} while (c>32);
	
	com_token[len] = 0;
	return data;
}

char *COM_ParseOut (char *data, char *out, int outlen)
{
	int		c;
	int		len;
	
	len = 0;
	out[0] = 0;
	
	if (!data)
		return NULL;
		
// skip whitespace
skipwhite:
	while ( (c = *data) <= ' ')
	{
		if (c == 0)
			return NULL;			// end of file;
		data++;
	}
	
// skip // comments
	if (c=='/')
	{
		if (data[1] == '/')
		{
			while (*data && *data != '\n')
				data++;
			goto skipwhite;
		}
	}
	

// handle quoted strings specially
	if (c == '\"')
	{
		data++;
		while (1)
		{
			if (len >= outlen-1)
				return data;

			c = *data++;
			if (c=='\"' || !c)
			{
				out[len] = 0;
				return data;
			}
			out[len] = c;
			len++;
		}
	}

// parse a regular word
	do
	{
		if (len >= outlen-1)
			return data;

		out[len] = c;
		data++;
		len++;
		c = *data;
	} while (c>32);
	
	out[len] = 0;
	return data;
}

//same as COM_Parse, but parses two quotes next to each other as a single quote as part of the string
char *COM_StringParse (char *data)
{
	int		c;
	int		len;
	unsigned char *s;
	
	len = 0;
	com_token[0] = 0;
	
	if (!data)
		return NULL;
		
// skip whitespace
skipwhite:
	while ( (c = *data) <= ' ')
	{
		if (c == 0)
			return NULL;			// end of file;
		data++;
	}
	
// skip // comments
	if (c=='/')
	{
		if (data[1] == '/')
		{
			while (*data && *data != '\n')
				data++;
			goto skipwhite;
		}
	}
	

// handle quoted strings specially
	if (c == '\"')
	{
		data++;
		while (1)
		{
			if (len >= TOKENSIZE-1)
				return data;


			c = *data++;
			if (c=='\"')
			{
				c = *(data);
				if (c!='\"')
				{
					com_token[len] = 0;
					return data;
				}
				while (c=='\"')
				{
					com_token[len] = c;
					len++;
					data++;
					c = *(data+1);
				}
			}
			if (!c)
			{
				com_token[len] = 0;
				return data;
			}
			com_token[len] = c;
			len++;
		}
	}

// parse a regular word
	do
	{
		if (len >= TOKENSIZE-1)
			return data;

		com_token[len] = c;
		data++;
		len++;
		c = *data;
	} while (c>32);

	com_token[len] = 0;
#ifndef CLIENTONLY
	{
	extern redirect_t	sv_redirected;
	if (sv_redirected)	//servers shouldn't give the values of cvars to all clients... Like password...
		return data;
	}
#endif
	//now we check for macros.
	for (s = com_token, c= 0; c < len; c++, s++)	//this isn't a quoted token by the way.
	{
		if (*s == '$')
		{
			cvar_t *macro;
			char name[64];
			int i;

			for (i = 1; i < sizeof(name); i++)
			{
				if (s[i] <= ' ' || s[i] == '$')
					break;
			}

			Q_strncpyz(name, s+1, i);
			i-=1;
			
			macro = Cvar_FindVar(name);
			if (macro)	//got one...
			{
				memmove(s+strlen(macro->string), s+i+1, len-c-i);
				memcpy(s, macro->string, strlen(macro->string));
				s+=strlen(macro->string);
				len+=strlen(macro->string)-(i+1);
			}
		}
	}
	
	return data;
}

char *COM_ParseToken (char *data)
{
	int		c;
	int		len;
	
	len = 0;
	com_token[0] = 0;
	
	if (!data)
		return NULL;
		
// skip whitespace
skipwhite:
	while ( (c = *data) <= ' ')
	{
		if (c == 0)
			return NULL;			// end of file;
		data++;
	}
	
// skip // comments
	if (c=='/')
	{
		if (data[1] == '/')
		{
			while (*data && *data != '\n')
				data++;
			goto skipwhite;
		}
		else if (data[1] == '*')
		{
			data+=2;
			while (*data && (*data != '*' || data[1] != '/'))
				data++;
			data+=2;
			goto skipwhite;
		}
	}
	

// handle quoted strings specially
	if (c == '\"')
	{
		com_tokentype = TTP_STRING;
		data++;
		while (1)
		{
			c = *data++;
			if (c=='\"' || !c)
			{
				com_token[len] = 0;
				return data;
			}
			com_token[len] = c;
			len++;
		}
	}

	com_tokentype = TTP_UNKNOWN;

// parse single characters
	if (c==',' || c=='{' || c=='}'|| c==')'|| c=='(' || c=='\'' || c==':' || c==';' || c == '=' || c == '!' || c == '>' || c == '<' || c == '&' || c == '|' || c == '+')
	{
		com_token[len] = c;
		len++;
		com_token[len] = 0;
		return data+1;
	}

// parse a regular word
	do
	{
		com_token[len] = c;
		data++;
		len++;
		c = *data;
		if (c==',' || c=='{' || c=='}'|| c==')'|| c=='(' || c=='\'' || c==':' || c==';' || c == '=' || c == '!' || c == '>' || c == '<' || c == '&' || c == '|' || c == '+')
			break;
	} while (c>32);
	
	com_token[len] = 0;
	return data;
}

char *COM_ParseCString (char *data)
{
	int		c;
	int		len;
	
	len = 0;
	com_token[0] = 0;
	
	if (!data)
		return NULL;
		
// skip whitespace
skipwhite:
	while ( (c = *data) <= ' ')
	{
		if (c == 0)
			return NULL;			// end of file;
		data++;
	}
	
// skip // comments
	if (c=='/')
	{
		if (data[1] == '/')
		{
			while (*data && *data != '\n')
				data++;
			goto skipwhite;
		}
	}
	

// handle quoted strings specially
	if (c == '\"')
	{
		data++;
		while (1)
		{
			c = *data++;
			if (!c)
			{
				com_token[len] = 0;
				return data;
			}
			if (c == '\\')
			{
				c = *data++;
				switch(c)
				{
				case 'n':
					c = '\n';
					break;
				case '\\':
					c = '\\';
					break;
				case '"':
					c = '"';
					com_token[len] = c;
					len++;
					continue;
				default:
					com_token[len] = 0;
					return data;
				}
			}
			if (c=='\"' || !c)
			{
				com_token[len] = 0;
				return data;
			}
			com_token[len] = c;
			len++;
		}
	}

// parse a regular word
	do
	{
		com_token[len] = c;
		data++;
		len++;
		c = *data;
	} while (c>32);
	
	com_token[len] = 0;
	return data;
}


/*
================
COM_CheckParm

Returns the position (1 to argc-1) in the program's argument list
where the given parameter apears, or 0 if not present
================
*/

int COM_CheckNextParm (char *parm, int last)
{
	int i = last+1;
	
	for (i=1 ; i<com_argc ; i++)
	{
		if (!com_argv[i])
			continue;		// NEXTSTEP sometimes clears appkit vars.
		if (!Q_strcmp (parm,com_argv[i]))
			return i;
	}
		
	return 0;
}

int COM_CheckParm (char *parm)
{
	return COM_CheckNextParm(parm, 0);
}

/*
================
COM_CheckRegistered

Looks for the pop.txt file and verifies it.
Sets the "registered" cvar.
Immediately exits out if an alternate game was attempted to be started without
being registered.
================
*/
void COM_CheckRegistered (void)
{
	FILE		*h;
	unsigned short	check[128];
	int			i;

	COM_FOpenFile("gfx/pop.lmp", &h);
	static_registered = 0;

	if (!h)
	{
		Con_TPrintf (TL_SHAREWAREVERSION);
#if 0//ndef SERVERONLY
// FIXME DEBUG -- only temporary
		if (com_modified)
			Sys_Error ("You must have the registered version to play QuakeWorld");
#endif
		return;
	}

	fread (check, 1, sizeof(check), h);
	fclose (h);
	
	for (i=0 ; i<128 ; i++)
		if (pop[i] != (unsigned short)BigShort (check[i]))
			Sys_Error ("Corrupted data file.");
	
	static_registered = 1;
	Con_TPrintf (TL_REGISTEREDVERSION);
}



/*
================
COM_InitArgv
================
*/
void COM_InitArgv (int argc, char **argv)	//not allowed to tprint
{
	qboolean	safe;
	int			i;

	FILE *f;

	f = fopen(va("%s_p.txt", argv[0]), "rb");
	if (f)
	{
		char *buffer;
		int len;
		fseek(f, 0, SEEK_END);
		len = ftell(f);
		fseek(f, 0, SEEK_SET);

		buffer = malloc(len+1);
		fread(buffer, 1, len, f);
		buffer[len] = '\0';

		while (*buffer && (argc < MAX_NUM_ARGVS))
		{
			while (*buffer && ((*buffer <= 32) || (*buffer > 126)))
				buffer++;

			if (*buffer)
			{
				argv[argc] = buffer;
				argc++;

				while (*buffer && ((*buffer > 32) && (*buffer <= 126)))
					buffer++;

				if (*buffer)
				{
					*buffer = 0;
					buffer++;
				}
				
			}
		}


		fclose(f);
	}

	safe = false;

	for (com_argc=0 ; (com_argc<MAX_NUM_ARGVS) && (com_argc < argc) ;
		 com_argc++)
	{
		largv[com_argc] = argv[com_argc];
		if (!Q_strcmp ("-safe", argv[com_argc]))
			safe = true;
	}

	if (safe)
	{ 
	// force all the safe-mode switches. Note that we reserved extra space in
	// case we need to add these, so we don't need an overflow check
		for (i=0 ; i<NUM_SAFE_ARGVS ; i++)
		{
			largv[com_argc] = safeargvs[i];
			com_argc++;
		}
	}

	largv[com_argc] = argvdummy;
	com_argv = largv;
}

/*
================
COM_AddParm

Adds the given string at the end of the current argument list
================
*/
void COM_AddParm (char *parm)
{
	largv[com_argc++] = parm;
}


/*
================
COM_Init
================
*/
void COM_Init (void)
{
	qbyte	swaptest[2] = {1,0};

// set the qbyte swapping variables in a portable manner	
	if ( *(short *)swaptest == 1)
	{
		bigendien = false;
		BigShort = ShortSwap;
		LittleShort = ShortNoSwap;
		BigLong = LongSwap;
		LittleLong = LongNoSwap;
		BigFloat = FloatSwap;
		LittleFloat = FloatNoSwap;
	}
	else
	{
		bigendien = true;
		BigShort = ShortNoSwap;
		LittleShort = ShortSwap;
		BigLong = LongNoSwap;
		LittleLong = LongSwap;
		BigFloat = FloatNoSwap;
		LittleFloat = FloatSwap;
	}

	Cmd_AddCommand ("path", COM_Path_f);

	COM_InitFilesystem ();

	COM_CheckRegistered ();
	if (static_registered)
		registered.string = "1";
	else
		registered.string = "0";

	Cvar_Register (&registered, "Copy protection");
}


/*
============
va

does a varargs printf into a temp buffer, so I don't need to have
varargs versions of all text functions.
FIXME: make this buffer size safe someday
============
*/
char	*VARGS va(char *format, ...)
{
#define VA_BUFFERS 2 //power of two
	va_list		argptr;
	static char		string[VA_BUFFERS][1024];
	static int bufnum;

	bufnum++;
	bufnum &= (VA_BUFFERS-1);
	
	va_start (argptr, format);
	_vsnprintf (string[bufnum],sizeof(string[bufnum])-1, format,argptr);
	va_end (argptr);

	return string[bufnum];	
}


/// just for debugging
int	memsearch (qbyte *start, int count, int search)
{
	int		i;
	
	for (i=0 ; i<count ; i++)
		if (start[i] == search)
			return i;
	return -1;
}

/*
=============================================================================

QUAKE FILESYSTEM

=============================================================================
*/

int	com_filesize;


//
// in memory
//

typedef struct
{
	char	name[MAX_QPATH];
	int		filepos, filelen;

#if defined(HASH_FILESYSTEM) || defined(ONE_FS_HASH)
	bucket_t bucket;
#endif
} packfile_t;

typedef struct pack_s
{
	char	filename[MAX_OSPATH];
	FILE	*handle;
	int		numfiles;
	packfile_t	*files;

#if defined(HASH_FILESYSTEM)
	hashtable_t hash;
#endif

} pack_t;

//
// on disk
//
typedef struct
{
	char	name[56];
	int		filepos, filelen;
} dpackfile_t;

typedef struct
{
	int		filepos, filelen;
	char	name[8];
} dwadfile_t;

typedef struct
{
	char	id[4];
	int		dirofs;
	int		dirlen;
} dpackheader_t;

typedef struct
{
	char	id[4];
	int		dirlen;
	int		dirofs;
} dwadheader_t;

#define	MAX_FILES_IN_PACK	2048

char	com_gamedir[MAX_OSPATH];
char	com_basedir[MAX_OSPATH];

#ifdef ZLIB
#ifdef _WIN32
#pragma comment( lib, "../libs/zlib.lib" )
#endif
#ifndef __linux__
#define uShort ZLIBuShort
#define uLong ZLIBuLong
#else
//typedef unsigned short uShort;
#endif
#include "unzip.c"

typedef struct zipfile_s
{
	char filename[MAX_QPATH];
	unzFile handle;
	int		numfiles;
	packfile_t	*files;

#ifdef HASH_FILESYSTEM
	hashtable_t hash;
#endif
} zipfile_t;

static packfile_t *Com_FileInZip(zipfile_t *zip, char *filename);
char *Com_ReadFileInZip(zipfile_t *zip, char *buffer);
int com_filenum;
void *com_pathforfile;	//fread and stuff is preferable if null.
#endif

typedef struct searchpath_s
{
	enum {SPT_OS, SPT_PACK
#ifdef ZLIB
		, SPT_ZIP
#endif
	} type;
	char	filename[MAX_OSPATH];
	union {
		pack_t	*pack;		// only one of filename / pack will be used
#ifdef ZLIB
		zipfile_t *zip;
#endif
	} u;
	struct searchpath_s *next;
} searchpath_t;

searchpath_t	*com_searchpaths;
searchpath_t	*com_base_searchpaths;	// without gamedirs

/*
================
COM_filelength
================
*/
int COM_filelength (FILE *f)
{
	int		pos;
	int		end;

	pos = ftell (f);
	fseek (f, 0, SEEK_END);
	end = ftell (f);
	fseek (f, pos, SEEK_SET);

	return end;
}
int COM_FileOpenRead (char *path, FILE **hndl)
{
	FILE	*f;

	f = fopen(path, "rb");
	if (!f)
	{
		*hndl = NULL;
		return -1;
	}
	*hndl = f;
	
	return COM_filelength(f);
}

int COM_FileSize(char *path)
{
	int len;
	FILE *h;	
	len = COM_FOpenFile(path, &h);
	if (len>=0)
		fclose(h);

	return len;
}

/*
============
COM_Path_f

============
*/
void COM_Path_f (void)
{
	searchpath_t	*s;
	
	Con_TPrintf (TL_CURRENTSEARCHPATH);
	for (s=com_searchpaths ; s ; s=s->next)
	{
		if (s == com_base_searchpaths)
			Con_Printf ("----------\n");
		switch (s->type)
		{
		case SPT_PACK:
			Con_TPrintf (TL_SERACHPATHISPACK, s->u.pack->filename, s->u.pack->numfiles);
			break;
#ifdef ZLIB
		case SPT_ZIP:
			Con_TPrintf (TL_SERACHPATHISZIP, s->u.zip->filename, s->u.zip->numfiles);
			break;
#endif
		default:
			Con_Printf ("%s\n", s->filename);
		}
	}
}

/*
============
COM_WriteFile

The filename will be prefixed by the current game directory
============
*/
void COM_WriteFile (char *filename, void *data, int len)
{
	FILE	*f;
	char	name[MAX_OSPATH];
	
	sprintf (name, "%s/%s", com_gamedir, filename);
	
	f = fopen (name, "wb");
	if (!f)
	{
		Sys_mkdir(com_gamedir);
		f = fopen (name, "wb");
		if (!f)
		{
			Con_Printf("Error opening %s\n", filename);
			return;
		}
	}
	
	Sys_Printf ("COM_WriteFile: %s\n", name);
	fwrite (data, 1, len, f);
	fclose (f);

	com_fschanged=true;
}


/*
============
COM_CreatePath

Only used for CopyFile and download
============
*/
void	COM_CreatePath (char *path)
{
	char	*ofs;
	
	for (ofs = path+1 ; *ofs ; ofs++)
	{
		if (*ofs == '/')
		{	// create the directory
			*ofs = 0;
			Sys_mkdir (path);
			*ofs = '/';
		}
	}
}


/*
===========
COM_CopyFile

Copies a file over from the net to the local cache, creating any directories
needed.  This is for the convenience of developers using ISDN from home.
===========
*/
void COM_CopyFile (char *netpath, char *cachepath)
{
	FILE	*in, *out;
	int		remaining, count;
	char	buf[4096];
	
	remaining = COM_FileOpenRead (netpath, &in);		
	COM_CreatePath (cachepath);	// create directories up to the cache file
	out = fopen(cachepath, "wb");
	if (!out)
		Sys_Error ("Error opening %s", cachepath);
	
	while (remaining)
	{
		if (remaining < sizeof(buf))
			count = remaining;
		else
			count = sizeof(buf);
		fread (buf, 1, count, in);
		fwrite (buf, 1, count, out);
		remaining -= count;
	}

	fclose (in);
	fclose (out);
}

int fs_hash_dups;
int fs_hash_files;
int FS_RebuildOSFSHash(char *filename, int filesize, void *data)
{
	if (filename[strlen(filename)-1] == '/')
	{	//this is actually a directory

		char childpath[256];
		sprintf(childpath, "%s*.*", filename);
		Sys_EnumerateFiles(((searchpath_t	*)data)->filename, childpath, FS_RebuildOSFSHash, data);
		sprintf(childpath, "%s*", filename);
		Sys_EnumerateFiles(((searchpath_t	*)data)->filename, childpath, FS_RebuildOSFSHash, data);
		return true;
	}
	if (!Hash_Get(&filesystemhash, filename))
	{
		bucket_t *bucket = BZ_Malloc(sizeof(bucket_t) + strlen(filename)+1);
		strcpy((char *)(bucket+1), filename);
#ifdef _WIN32
		Q_strlwr((char *)(bucket+1));
#endif
		Hash_Add2(&filesystemhash, (char *)(bucket+1), data, bucket);

		fs_hash_files++;
	}
	else
		fs_hash_dups++;
	return true;
}

void FS_RebuildFSHash(void)
{
	int i;
	searchpath_t	*search;
	if (!filesystemhash.numbuckets)
	{
		filesystemhash.numbuckets = 1024;
		filesystemhash.bucket = BZ_Malloc(Hash_BytesForBuckets(filesystemhash.numbuckets));
	}
	else
		memset(filesystemhash.bucket, 0, Hash_BytesForBuckets(filesystemhash.numbuckets));
	Hash_InitTable(&filesystemhash, filesystemhash.numbuckets, filesystemhash.bucket);

	fs_hash_dups = 0;
	fs_hash_files = 0;

	for (search = com_searchpaths ; search ; search = search->next)
	{
		switch (search->type)
		{
		case SPT_PACK:
			for (i = 0; i < search->u.pack->numfiles; i++)
			{
				if (!Hash_Get(&filesystemhash, search->u.pack->files[i].name))
				{
					fs_hash_files++;
					Hash_Add2(&filesystemhash, search->u.pack->files[i].name, &search->u.pack->files[i], &search->u.pack->files[i].bucket);
				}
				else
					fs_hash_dups++;
			}
			break;
#ifdef ZLIB
		case SPT_ZIP:
			for (i = 0; i < search->u.zip->numfiles; i++)
			{
				if (!Hash_Get(&filesystemhash, search->u.zip->files[i].name))
				{
					fs_hash_files++;
					Hash_Add2(&filesystemhash, search->u.zip->files[i].name, &search->u.zip->files[i], &search->u.zip->files[i].bucket);
				}
				else
					fs_hash_dups++;
			}
			break;
#endif
		case SPT_OS:
			Sys_EnumerateFiles(search->filename, "*.*", FS_RebuildOSFSHash, search);
			break;
		default:
			Sys_Error("FS_RebuildFSHash: Bad searchpath type\n");
			break;
		}
	}

	com_fschanged = false;

	Con_Printf("%i unique files, %i duplicates\n", fs_hash_files, fs_hash_dups);
}

/*
===========
COM_FindFile

Finds the file in the search path.
Sets com_filesize and one of handle or file
===========
*/
int file_from_pak; // global indicating file came from pack file ZOID
#if 0
/*
int COM_FOpenFile2 (char *sensativename, FILE **file, qboolean compressedokay)
{
	searchpath_t	*search;
	char		netpath[MAX_OSPATH];
	int			findtime;
	char		filename[MAX_QPATH];

	Q_strncpyz(filename, sensativename, sizeof(filename));
	Q_strlwr(filename);

	if (filename[strlen(filename)-1] == '/')
	{
		*file = NULL;
		com_filesize = -1;
#ifdef ZLIB
		com_pathforfile=NULL;
#endif
		return -1;
	}

	if (com_fs_cache.value && !developer.value)
	{
		if (com_fschanged)
			FS_RebuildFSHash();

		if (filesystemhash.numbuckets)
		{
			void *value;
			value = Hash_Get(&filesystemhash, filename);

			if (!value)	//we don't know about a file by this name
			{
				Con_DPrintf ("FindFile: don't know %s\n", filename);
		
				*file = NULL;
				com_filesize = -1;
#ifdef ZLIB
				com_pathforfile=NULL;
#endif
				return -1;
			}

			for (search = com_searchpaths ; search ; search = search->next)
			{
				switch (search->type)
				{
				case SPT_PACK:
					{
						pack_t		*pak;
						packfile_t *pf = value;
						pak = search->u.pack;
						if (pf >= pak->files && pf < pak->files+pak->numfiles)	//is the range right?
						{
							//is in this pack file
							Con_DPrintf ("PackFile: %s : %s\n",pak->filename, filename);

					// open a new file on the pakfile
							*file = fopen (pak->filename, "rb");
							if (!*file)
								Sys_Error ("Couldn't reopen %s", pak->filename);	
							fseek (*file, pf->filepos, SEEK_SET);
							com_filesize = pf->filelen;

							file_from_pak = 1;
#ifdef ZLIB
							com_pathforfile=NULL;
#endif
							return com_filesize;
						}
					}
					break;
#ifdef ZLIB
				case SPT_ZIP:
					{
						zipfile_t	*zip;
						packfile_t *pfile = value;
						zip = search->u.zip;
						if (pfile >= zip->files && pfile < zip->files+zip->numfiles)	//is the pointer to within the list?
						{
							Con_DPrintf ("ZipFile: %s : %s\n",zip->filename, filename);

							file_from_pak = 2;
							com_filenum = pfile	- zip->files;
							com_filesize = pfile->filelen ;
							if (!compressedokay)
							{
								unzLocateFileMy (zip->handle, com_filenum, zip->files[com_filenum].filepos);
								if ((*file = unzOpenCurrentFileFile(zip->handle, search->filename)))	//phew!
								{											
									com_pathforfile = NULL;
									return pfile->filelen;
								}
								Con_TPrintf(TL_COMPRESSEDFILEOPENFAILED, filename);
								break;
							}
							else
							{
								*file = NULL;
								com_pathforfile = search;
								return pfile->filelen;
							}
						}
					}
					break;
#endif
				case SPT_OS:
					if (value == search)	//hash tables refer to the searchpath.
					{
						sprintf (netpath, "%s/%s",search->filename, filename);
				
						findtime = Sys_FileTime (netpath);
						if (findtime == -1)
							break;

						Con_DPrintf ("FindFile: %s\n",netpath);

						*file = fopen (netpath, "rb");
						file_from_pak = 0;
#ifdef ZLIB
						com_pathforfile=NULL;
#endif
						return COM_filelength (*file);
					}
					break;
				}
			}

			Con_Printf ("FindFile: can't find %s\n", filename);

			*file = NULL;
			com_filesize = -1;
			return -1;
		}
	}

//
// search through the path, one element at a time
//
	for (search = com_searchpaths ; search ; search = search->next)
	{
	// is the element a pak file?
		switch (search->type)
		{
		case SPT_PACK:
			{
				int i;
				pack_t		*pak;

			// look through all the pak file elements
				pak = search->u.pack;
				for (i=0 ; i<pak->numfiles ; i++)
				{
					if (!strcmp (pak->files[i].name, filename))
					{	// found it!
						Con_DPrintf ("PackFile: %s : %s\n",pak->filename, filename);

					// open a new file on the pakfile
						*file = fopen (pak->filename, "rb");
						if (!*file)
							Sys_Error ("Couldn't reopen %s", pak->filename);	
						fseek (*file, pak->files[i].filepos, SEEK_SET);
						com_filesize = pak->files[i].filelen;
						file_from_pak = 1;
	#ifdef ZLIB
						com_pathforfile=NULL;
	#endif
						return com_filesize;
					}
				}
			}
			break;
#ifdef ZLIB
		case SPT_ZIP:
			{
				packfile_t *pfile;
				zipfile_t	*zip = search->u.zip;
				if ((pfile = Com_FileInZip(zip, filename)))
				{
					file_from_pak = 2;
					com_filenum = pfile	- zip->files;
					com_filesize = pfile->filelen ;
					if (!compressedokay)
					{
						unzLocateFileMy (zip->handle, com_filenum, zip->files[com_filenum].filepos);
						if ((*file = unzOpenCurrentFileFile(zip->handle, search->filename)))	//phew!
						{											
							com_pathforfile = NULL;
							return pfile->filelen;
						}

						//this code copies it to a temp file for ultimate hackage.
						{
							char *buf;
							FILE *f = tmpfile();
							buf = BZ_Malloc(pfile->filelen);
							Com_ReadFileInZip(zip, buf);
							fwrite(buf, 1, pfile->filelen, f);
							fseek(f, 0, SEEK_SET);

							*file = f;
							com_pathforfile = search;
							return pfile->filelen;
						}
						Con_TPrintf(TL_COMPRESSEDFILEOPENFAILED, filename);
						continue;
					}								
					*file = NULL;
					com_pathforfile = search;
					return pfile->filelen;
				}
			}
			break;
#endif
		case SPT_OS:
	// check a file in the directory tree
			if (!static_registered)
			{	// if not a registered version, don't ever go beyond base
				if ( strchr (filename, '/') || strchr (filename,'\\'))
					continue;
			}
			
			_snprintf (netpath, sizeof(netpath)-1, "%s/%s",search->filename, filename);
			
			findtime = Sys_FileTime (netpath);
			if (findtime == -1)
				continue;

			Con_DPrintf ("FindFile: %s\n",netpath);

			*file = fopen (netpath, "rb");
			file_from_pak = 0;
#ifdef ZLIB
			com_pathforfile=NULL;
#endif
			return COM_filelength (*file);
		default:
			Sys_Error("COM_FOpenFile2: bad searchpath type\n");
			break;
		}
		
	}
	
	//Con_DPrintf ("FindFile: can't find %s\n", filename);
	
	*file = NULL;
	com_filesize = -1;
	return -1;
}
*/
#endif
//if loc is valid, loc->search is always filled in, the others are filled on success.
//returns -1 if couldn't find.
int FS_FLocateFile(char *filename, FSLF_ReturnType_e returntype, flocation_t *loc)
{
	char		netpath[MAX_OSPATH];
	int depth=0, len;
	searchpath_t	*search;

	packfile_t *pf;
//Con_Printf("Finding %s: ", filename);

	if (com_fs_cache.value)
	{
		if (com_fschanged)
			FS_RebuildFSHash();
		pf = Hash_Get(&filesystemhash, filename);
		if (!pf)
			goto fail;
	}
	else
		pf = NULL;
//
// search through the path, one element at a time
//
	for (search = com_searchpaths ; search ; search = search->next)
	{
	// is the element a pak file?
		switch (search->type)
		{
		case SPT_PACK:
			{
				int i;
				pack_t		*pak;

				if (returntype == FSLFRT_DEPTH_ANYPATH)
					depth++;

			// look through all the pak file elements
				pak = search->u.pack;

				if (pf)
				{	//is this a pointer to a file in this pak?
					if (pf < pak->files || pf > pak->files + pak->numfiles)
						continue;
				}
				else
				{
					for (i=0 ; i<pak->numfiles ; i++)	//look for the file
					{
						if (!strcmp (pak->files[i].name, filename))
						{
							pf = &pak->files[i];
							break;
						}
					}
				}

				if (pf)
				{
					len = pf->filelen;
					if (loc)
					{
						loc->search = search;
						loc->index = pf - pak->files;
						strcpy(loc->rawname, pak->filename);
						loc->offset = pf->filepos;
						loc->len = pf->filelen;
					}
					goto out;
				}
			}
			break;
#ifdef ZLIB
		case SPT_ZIP:
			{
				zipfile_t	*zip = search->u.zip;

				if (returntype == FSLFRT_DEPTH_ANYPATH)
					depth++;

				if (pf)
				{
					if (pf < zip->files || pf > zip->files + zip->numfiles)
						continue;
				}
				else
					pf = Com_FileInZip(zip, filename);
				if (pf)
				{
					len = pf->filelen;
					if (loc)
					{
						loc->search = search;
						loc->index = pf - zip->files;
						strcpy(loc->rawname, "");	//no rawname, as it needs special processing.
						loc->offset = pf->filepos;
						loc->len = pf->filelen;

						unzLocateFileMy (zip->handle, com_filenum, zip->files[com_filenum].filepos);
						loc->offset = unzGetCurrentFileUncompressedPos(zip->handle);
						if (loc->offset<0)
						{	//file not found...
							*loc->rawname = '\0';
							loc->offset=0;
						}
					}
					goto out;
				}
			}
			break;
#endif
		case SPT_OS:
			{
				FILE *f;

				if (pf && (void *)pf != (void *)search)
					continue;

/*
				if (!static_registered)
				{	// if not a registered version, don't ever go beyond base
					if ( strchr (filename, '/') || strchr (filename,'\\'))
						continue;
				}
*/			

				depth++;
		// check a file in the directory tree
				_snprintf (netpath, sizeof(netpath)-1, "%s/%s",search->filename, filename);
				
				f = fopen(netpath, "rb");
				if (!f)
					continue;

				fseek(f, 0, SEEK_END);
				len = ftell(f);
				fclose(f);
				if (loc)
				{
					loc->len = len;
					loc->offset = 0;
					loc->search = search;
					loc->index = 0;
					Q_strncpyz(loc->rawname, netpath, sizeof(loc->rawname));
				}

				goto out;
			}
			break;
		
		default:
			Sys_Error("COM_FLocateFile: Bad filesystem type\n");
			break;
		}
	}
fail:
	if (loc)
		loc->search = NULL;
	depth = 0x7fffffff;
	len = -1;
out:

/*	if (len>=0)
	{
		if (loc)
			Con_Printf("Found %s:%i\n", loc->rawname, loc->len);
		else
			Con_Printf("Found %s\n", filename);
	}
	else
		Con_Printf("Failed\n");
*/	if (returntype == FSLFRT_LENGTH)
		return len;
	else
		return depth;
}
int COM_FOpenLocationFILE(flocation_t *loc, FILE **file)
{
	if (!*loc->rawname)
	{
		if (!loc->len)
		{
			*file = NULL;
			return -1;
		}

#ifdef ZLIB
		if (loc->search->type == SPT_ZIP)
		{//create a new, temp file, bung the contents of the compressed file into it, then continue.
			char *buf;
			FILE *f = tmpfile();
			buf = BZ_Malloc(loc->len);
			com_filenum = loc->index;
			Com_ReadFileInZip(loc->search->u.zip, buf);
			fwrite(buf, 1, loc->len, f);
			BZ_Free(buf);
			fseek(f, 0, SEEK_SET);

			*file = f;
			com_pathforfile = loc->search;
			return loc->len;
		}
#endif
	}
//	Con_Printf("Opening %s\n", loc->rawname);
	*file = fopen(loc->rawname, "rb");
	if (!*file)
		return -1;
	fseek(*file, loc->offset, SEEK_SET);
#ifdef ZLIB
	com_pathforfile = loc->search;
#endif
	return loc->len;
}

int COM_FOpenFile(char *filename, FILE **file)
{
	flocation_t loc;
	FS_FLocateFile(filename, FSLFRT_LENGTH, &loc);

	com_filesize = -1;
	if (loc.search)
	{
		file_from_pak = loc.search->type;
		com_filesize = COM_FOpenLocationFILE(&loc, file);
	}
	else
		*file = NULL;
	return com_filesize;
}
//int COM_FOpenFile (char *filename, FILE **file) {file_from_pak=0;return COM_FOpenFile2 (filename, file, false);}	//FIXME: TEMPORARY


cache_user_t *loadcache;
qbyte	*loadbuf;
int		loadsize;

/*
============
COM_LoadFile

Filename are reletive to the quake directory.
Allways appends a 0 qbyte to the loaded data.
============
*/
qbyte *COM_LoadFile (char *path, int usehunk)
{
	char *buf;
	FILE *f;
	int len;
	char	base[32];
	flocation_t loc;
	FS_FLocateFile(path, FSLFRT_LENGTH, &loc);

	if (!loc.search)
		return NULL;	//wasn't found

	if (*loc.rawname)
	{
//		Con_Printf("Opening %s\n", loc.rawname);
		f = fopen(loc.rawname, "rb");
		if (!f)
			return NULL;
		fseek(f, loc.offset, SEEK_SET);
	}
	else
		f = NULL;

	com_filesize = len = loc.len;
	// extract the filename base name for hunk tag
	COM_FileBase (path, base);

	if (usehunk == 0)
		buf = Z_Malloc (len+1);
	else if (usehunk == 1)
		buf = Hunk_AllocName (len+1, base);
	else if (usehunk == 2)
		buf = Hunk_TempAlloc (len+1);
	else if (usehunk == 3)
		buf = Cache_Alloc (loadcache, len+1, base);
	else if (usehunk == 4)
	{
		if (len+1 > loadsize)
			buf = Hunk_TempAlloc (len+1);
		else
			buf = loadbuf;
	}
	else if (usehunk == 5)
		buf = BZ_Malloc(len+1);
	else if (usehunk == 6)
		buf = Hunk_TempAllocMore (len+1);
	else
	{
		Sys_Error ("COM_LoadFile: bad usehunk");
		buf = NULL;
	}

	if (!buf)
		Sys_Error ("COM_LoadFile: not enough space for %s", path);

	((qbyte *)buf)[len] = 0;
#ifndef SERVERONLY
	if (qrenderer)
		if (Draw_BeginDisc)
			Draw_BeginDisc ();
#endif

	if (f)
	{
		fread (buf, 1, len, f);
		fclose (f);
	}
	else
	{
		switch(loc.search->type)
		{
#ifdef ZLIB
		case SPT_ZIP:
			com_filenum = loc.index;
			Com_ReadFileInZip(loc.search->u.zip, buf);
			break;
#endif
		default:
			Sys_Error("COM_LoadFile: Bad search path type");
			break;
		}
	}

#ifndef SERVERONLY
	if (qrenderer)
		if (Draw_EndDisc)
			Draw_EndDisc ();
#endif

	return buf;
}

/*
============
COM_LoadFile

Filename are reletive to the quake directory.
Allways appends a 0 qbyte to the loaded data.
============
*/
/*
cache_user_t *loadcache;
qbyte	*loadbuf;
int		loadsize;
qbyte *COM_LoadFile (char *path, int usehunk)
{
	FILE	*h;
	qbyte	*buf;
	char	base[32];
	int		len;

	buf = NULL;	// quiet compiler warning

// look for it in the filesystem or pack files
	len = com_filesize = COM_FOpenFile2 (path, &h, true);
	if (len<0)
		return NULL;

// extract the filename base name for hunk tag
	COM_FileBase (path, base);
	
	if (usehunk == 1)
		buf = Hunk_AllocName (len+1, base);
	else if (usehunk == 2)
		buf = Hunk_TempAlloc (len+1);
	else if (usehunk == 0)
		buf = Z_Malloc (len+1);
	else if (usehunk == 3)
		buf = Cache_Alloc (loadcache, len+1, base);
	else if (usehunk == 4)
	{
		if (len+1 > loadsize)
			buf = Hunk_TempAlloc (len+1);
		else
			buf = loadbuf;
	}
	else if (usehunk == 5)
		buf = BZ_Malloc(len+1);
	else
		Sys_Error ("COM_LoadFile: bad usehunk");

	if (!buf)
		Sys_Error ("COM_LoadFile: not enough space for %s", path);
		
	((qbyte *)buf)[len] = 0;
#ifndef SERVERONLY
	if (qrenderer)
		if (Draw_BeginDisc)
			Draw_BeginDisc ();
#endif
#ifdef ZLIB
	if (com_pathforfile)
		Com_ReadFileInZip(((searchpath_t *)com_pathforfile)->u.zip, buf);
	else
#endif
	{
		fread (buf, 1, len, h);
		fclose (h);
	}
#ifndef SERVERONLY
	if (qrenderer)
		if (Draw_EndDisc)
			Draw_EndDisc ();
#endif

	return buf;
}
*/

qbyte *COM_LoadMallocFile (char *path)	//used for temp info along side temp hunk
{
	return COM_LoadFile (path, 5);
}

qbyte *COM_LoadHunkFile (char *path)
{
	return COM_LoadFile (path, 1);
}

qbyte *COM_LoadTempFile (char *path)
{
	return COM_LoadFile (path, 2);
}
qbyte *COM_LoadTempFile2 (char *path)
{
	return COM_LoadFile (path, 6);
}

void COM_LoadCacheFile (char *path, struct cache_user_s *cu)
{
	loadcache = cu;
	COM_LoadFile (path, 3);
}

// uses temp hunk if larger than bufsize
qbyte *COM_LoadStackFile (char *path, void *buffer, int bufsize)
{
	qbyte	*buf;
	
	loadbuf = (qbyte *)buffer;
	loadsize = bufsize;
	buf = COM_LoadFile (path, 4);
	
	return buf;
}


int Q_strwildcmp(char *s1, char *s2);
#ifdef ZLIB
int COM_EnumerateZipFiles (zipfile_t *zip, char *match, int (*func)(char *, int, void *), void *parm);
#endif
int COM_EnumeratePackFiles (pack_t *zip, char *match, int (*func)(char *, int, void *), void *parm)
{
	int		num;

	for (num = 0; num<(int)zip->numfiles; num++)
	{
		if (!Q_strwildcmp(zip->files[num].name, match))
		{
			if (!func(zip->files[num].name, zip->files[num].filelen, parm))
				return false;
		}
	}

	return true;
}

void COM_EnumerateFiles (char *match, int (*func)(char *, int, void *), void *parm)
{
	searchpath_t    *search;
	for (search = com_searchpaths; search ; search = search->next)
	{
	// is the element a pak file?
		switch(search->type)
		{
		case SPT_PACK:
			COM_EnumeratePackFiles(search->u.pack, match, func, parm);
			break;
#ifdef ZLIB
		case SPT_ZIP:
			COM_EnumerateZipFiles(search->u.zip, match, func, parm);
			break;
#endif
		case SPT_OS:
			Sys_EnumerateFiles(search->filename, match, func, parm);
			break;
		}
	}
}


/*
=================
COM_LoadPackFile

Takes an explicit (not game tree related) path to a pak file.

Loads the header and directory, adding the files at the beginning
of the list so they override previous pack files.
=================
*/
pack_t *COM_LoadPackFile (char *packfile)
{
	dpackheader_t	header;
	int				i;
//	int				j;
	packfile_t		*newfiles;
	int				numpackfiles;
	pack_t			*pack;
	FILE			*packhandle;
	dpackfile_t		info;
//	unsigned short		crc;

	if (COM_FileOpenRead (packfile, &packhandle) == -1)
		return NULL;

	fread (&header, 1, sizeof(header), packhandle);
	if (header.id[0] != 'P' || header.id[1] != 'A'
	|| header.id[2] != 'C' || header.id[3] != 'K')
	{
		return NULL;
//		Sys_Error ("%s is not a packfile", packfile);
	}
	header.dirofs = LittleLong (header.dirofs);
	header.dirlen = LittleLong (header.dirlen);

	numpackfiles = header.dirlen / sizeof(dpackfile_t);

//	if (numpackfiles > MAX_FILES_IN_PACK)
//		Sys_Error ("%s has %i files", packfile, numpackfiles);

//	if (numpackfiles != PAK0_COUNT)
//		com_modified = true;	// not the original file

	newfiles = Z_Malloc (numpackfiles * sizeof(packfile_t));

	fseek (packhandle, header.dirofs, SEEK_SET);
//	fread (&info, 1, header.dirlen, packhandle);

// crc the directory to check for modifications
//	crc = CRC_Block((qbyte *)info, header.dirlen);
	

//	CRC_Init (&crc);

	pack = Z_Malloc (sizeof (pack_t));
#ifdef HASH_FILESYSTEM
	Hash_InitTable(&pack->hash, numpackfiles+1, Z_Malloc(Hash_BytesForBuckets(numpackfiles+1)));
#endif
// parse the directory
	for (i=0 ; i<numpackfiles ; i++)
	{
		fread (&info, 1, sizeof(info), packhandle);
/*
		for (j=0 ; j<sizeof(info) ; j++)
			CRC_ProcessByte(&crc, ((qbyte *)&info)[j]);
*/
		strcpy (newfiles[i].name, info.name);
		Q_strlwr(newfiles[i].name);
		newfiles[i].filepos = LittleLong(info.filepos);
		newfiles[i].filelen = LittleLong(info.filelen);
#ifdef HASH_FILESYSTEM
		Hash_Add2(&pack->hash, newfiles[i].name, &newfiles[i], &newfiles[i].bucket);
#endif
	}
/*
	if (crc != PAK0_CRC)
		com_modified = true;
*/
	strcpy (pack->filename, packfile);
	pack->handle = packhandle;
	pack->numfiles = numpackfiles;
	pack->files = newfiles;
	
	Con_TPrintf (TL_ADDEDPACKFILE, packfile, numpackfiles);
	return pack;
}

void COM_FlushTempoaryPacks(void)
{
	searchpath_t *next;
	while (*com_searchpaths->filename == '*')
	{
		switch (com_searchpaths->type)
		{
		case SPT_PACK:
			fclose (com_searchpaths->u.pack->handle);
			Z_Free (com_searchpaths->u.pack->files);
#ifdef HASH_FILESYSTEM
			Z_Free (com_searchpaths->u.pack->hash.bucket);
#endif
			Z_Free (com_searchpaths->u.pack);
			break;
#ifdef ZLIB
		case SPT_ZIP:
			unzClose(com_searchpaths->u.zip->handle);
			Z_Free (com_searchpaths->u.zip->files);
#ifdef HASH_FILESYSTEM
			Z_Free (com_searchpaths->u.zip->hash.bucket);
#endif
			Z_Free (com_searchpaths->u.zip);
			break;
#endif
		case SPT_OS:
			break;
		}
		next = com_searchpaths->next;
		Z_Free (com_searchpaths);
		com_searchpaths = next;

		com_fschanged = true;
	}
}

qboolean COM_LoadMapPackFile (char *filename, int ofs)
{
	dpackheader_t	header;
	int				i;
	packfile_t		*newfiles;
	int				numpackfiles;
	pack_t			*pack;
	FILE			*packhandle;
	dpackfile_t		info;
	int fstart;
	char *ballsup;

	searchpath_t *search;
	flocation_t loc;

	FS_FLocateFile(filename, FSLFRT_LENGTH, &loc);

	if (!loc.search)
	{
		Con_Printf("Couldn't refind file\n");
		return false;
	}

	if (!*loc.rawname)
	{
		Con_Printf("File %s is compressed\n");
		return false;
	}
	packhandle = fopen(loc.rawname, "rb");
	if (!packhandle)
	{
		Con_Printf("Couldn't reopen file\n");
		return -1;
	}
	fseek(packhandle, loc.offset, SEEK_SET);

	fstart = loc.offset;
	fseek(packhandle, ofs+fstart, SEEK_SET);

	fread (&header, 1, sizeof(header), packhandle);
	if (header.id[0] != 'P' || header.id[1] != 'A'
	|| header.id[2] != 'C' || header.id[3] != 'K')
	{
		return false;
	}
	header.dirofs = LittleLong (header.dirofs);
	header.dirlen = LittleLong (header.dirlen);

	numpackfiles = header.dirlen / sizeof(dpackfile_t);

	newfiles = Z_Malloc (numpackfiles * sizeof(packfile_t));

	fseek (packhandle, header.dirofs+fstart, SEEK_SET);

	pack = Z_Malloc (sizeof (pack_t));
#ifdef HASH_FILESYSTEM
	Hash_InitTable(&pack->hash, numpackfiles+1, Z_Malloc(Hash_BytesForBuckets(numpackfiles+1)));
#endif
// parse the directory
	for (i=0 ; i<numpackfiles ; i++)
	{
		fread (&info, 1, sizeof(info), packhandle);

		strcpy (newfiles[i].name, info.name);
		Q_strlwr(newfiles[i].name);
		while ((ballsup = strchr(newfiles[i].name, '\\')))
			*ballsup = '/';
		newfiles[i].filepos = LittleLong(info.filepos)+fstart;
		newfiles[i].filelen = LittleLong(info.filelen);
#ifdef HASH_FILESYSTEM
		Hash_Add2(&pack->hash, newfiles[i].name, &newfiles[i], &newfiles[i].bucket);
#endif
	}

	strcpy (pack->filename, loc.rawname);
	pack->handle = packhandle;
	pack->numfiles = numpackfiles;
	pack->files = newfiles;
	
	Con_TPrintf (TL_ADDEDPACKFILE, filename, numpackfiles);

	search = Z_Malloc (sizeof(searchpath_t));
	search->type = SPT_PACK;
	*search->filename = '*';
	strcpy (search->filename+1, filename);
	search->u.pack = pack;
	search->next = com_searchpaths;
	com_searchpaths = search;	

	com_fschanged = true;
	return true;
}

#ifdef DOOMWADS
qboolean COM_LoadWadFile (char *wadname)
{
	dwadheader_t	header;
	int				i;
	packfile_t		*newfiles;
	int				numpackfiles;
	pack_t			*pack;
	FILE			*packhandle;
	dwadfile_t		info;
	int fstart;

	int section=0;
	char sectionname[MAX_QPATH];
	char filename[52];
	char neatwadname[52];

	searchpath_t *search;
	flocation_t loc;

	FS_FLocateFile(wadname, FSLFRT_LENGTH, &loc);

	if (!loc.search)
	{
		return false;
	}

	if (!*loc.rawname)
	{
		Con_Printf("File %s is compressed\n");
		return false;
	}
	packhandle = fopen(loc.rawname, "rb");
	if (!packhandle)
	{
		Con_Printf("Couldn't open file\n");
		return false;
	}
	fseek(packhandle, loc.offset, SEEK_SET);

	fstart = loc.offset;
	fseek(packhandle, fstart, SEEK_SET);

	fread (&header, 1, sizeof(header), packhandle);
	if (header.id[1] != 'W'
	|| header.id[2] != 'A' || header.id[3] != 'D')
	{
		return false;
	}
	if (header.id[0] == 'I')
		*neatwadname = '\0';
	else if (header.id[0] == 'P')
	{
		COM_StripExtension(wadname, neatwadname);
		strcat(neatwadname, ":");
	}
	else
		return false;
	header.dirofs = LittleLong (header.dirofs);
	header.dirlen = LittleLong (header.dirlen);

	numpackfiles = header.dirlen;

	newfiles = Z_Malloc (numpackfiles * sizeof(packfile_t));

	fseek (packhandle, header.dirofs+fstart, SEEK_SET);

	pack = Z_Malloc (sizeof (pack_t));
#ifdef HASH_FILESYSTEM
	Hash_InitTable(&pack->hash, numpackfiles+1, Z_Malloc(Hash_BytesForBuckets(numpackfiles+1)));
#endif
// parse the directory
	for (i=0 ; i<numpackfiles ; i++)
	{
		fread (&info, 1, sizeof(info), packhandle);

		strcpy (filename, info.name);
		filename[8] = '\0';
		Q_strlwr(filename);

		newfiles[i].filepos = LittleLong(info.filepos)+fstart;
		newfiles[i].filelen = LittleLong(info.filelen);

		switch(section)	//be prepared to remap filenames.
		{
newsection:
		case 0:
			if (info.filelen == 0)
			{	//marker for something...

				if (!strcmp(filename, "s_start"))
				{
					section = 2;
					sprintf (newfiles[i].name, "sprites/%s", filename);
					break;
				}
				if (!strcmp(filename, "p_start"))
				{
					section = 3;
					sprintf (newfiles[i].name, "patches/%s", filename);
					break;
				}
				if (!strcmp(filename, "f_start"))
				{
					section = 4;
					sprintf (newfiles[i].name, "flats/%s", filename);
					break;
				}
				if ((filename[0] == 'e' && filename[2] == 'm') || !strncmp(filename, "map", 3))
				{	//this is the start of a beutiful new map
					section = 1;
					strcpy(sectionname, filename);
					sprintf (newfiles[i].name, "maps/%s%s.bsp", neatwadname, filename);
					newfiles[i].filepos = fstart;
					newfiles[i].filelen = 4;
					break;
				}
				if (!strncmp(filename, "gl_", 3) && ((filename[4] == 'e' && filename[5] == 'm') || !strncmp(filename+3, "map", 3)))
				{	//this is the start of a beutiful new map
					section = 5;
					strcpy(sectionname, filename+3);
					break;
				}
			}

			sprintf (newfiles[i].name, "wad/%s", filename);
			break;
		case 1:	//map section
			if (strcmp(filename, "things") &&
				strcmp(filename, "linedefs") &&
				strcmp(filename, "sidedefs") &&
				strcmp(filename, "vertexes") &&
				strcmp(filename, "segs") &&
				strcmp(filename, "ssectors") &&
				strcmp(filename, "nodes") &&
				strcmp(filename, "sectors") &&
				strcmp(filename, "reject") &&
				strcmp(filename, "blockmap"))
			{
				section = 0;
				goto newsection;
			}
			sprintf (newfiles[i].name, "maps/%s%s.%s", neatwadname, sectionname, filename);
			break;
		case 5:	//glbsp output section
			if (strcmp(filename, "gl_vert") &&
				strcmp(filename, "gl_segs") &&
				strcmp(filename, "gl_ssect") &&
				strcmp(filename, "gl_pvs") &&
				strcmp(filename, "gl_nodes"))
			{
				section = 0;
				goto newsection;
			}
			sprintf (newfiles[i].name, "maps/%s%s.%s", neatwadname, sectionname, filename);
			break;
		case 2:	//sprite section
			if (!strcmp(filename, "s_end"))
			{
				section = 0;
				goto newsection;
			}
			sprintf (newfiles[i].name, "sprites/%s", filename);
			break;
		case 3:	//patches section
			if (!strcmp(filename, "p_end"))
			{
				section = 0;
				goto newsection;
			}
			sprintf (newfiles[i].name, "patches/%s", filename);
			break;
		case 4:	//flats section
			if (!strcmp(filename, "f_end"))
			{
				section = 0;
				goto newsection;
			}
			sprintf (newfiles[i].name, "flats/%s", filename);
			break;
		}
#ifdef HASH_FILESYSTEM
		Hash_Add2(&pack->hash, newfiles[i].name, &newfiles[i], &newfiles[i].bucket);
#endif
	}

	strcpy (pack->filename, loc.rawname);
	pack->handle = packhandle;
	pack->numfiles = numpackfiles;
	pack->files = newfiles;

	Con_Printf ("Added wad file %s\n", wadname, numpackfiles);

	search = Z_Malloc (sizeof(searchpath_t));
	search->type = SPT_PACK;
	strcpy (search->filename, wadname);
	search->u.pack = pack;
	search->next = com_searchpaths;
	com_searchpaths = search;

	com_fschanged = true;

	COM_StripExtension(wadname, sectionname);
	strcat(sectionname, ".gwa");
	if (strcmp(sectionname, wadname))
		COM_LoadWadFile(sectionname);
	return true;
}
#endif

#ifdef ZLIB
/*
=================
COM_LoadZipFile

Takes an explicit (not game tree related) path to a pak file.

Loads the header and directory, adding the files at the beginning
of the list so they override previous pack files.
=================
*/
zipfile_t *COM_LoadZipFile (char *packfile)
{
	int i;

	zipfile_t *zip;
	packfile_t		*newfiles;

	unz_global_info	globalinf;
	unz_file_info	file_info;	

	FILE			*packhandle;
	if (COM_FileOpenRead (packfile, &packhandle) == -1)
		return NULL;
	fclose(packhandle);	

	zip = Z_Malloc(sizeof(zipfile_t));
	Q_strncpyz(zip->filename, packfile, sizeof(zip->filename));
	zip->handle = unzOpen (packfile);
	if (!zip->handle)
	{
		Con_TPrintf (TL_COULDNTOPENZIP, packfile);
		return NULL;
	}

	unzGetGlobalInfo (zip->handle, &globalinf);

	zip->numfiles = globalinf.number_entry;

	zip->files = newfiles = Z_Malloc (zip->numfiles * sizeof(packfile_t));
#ifdef HASH_FILESYSTEM
	Hash_InitTable(&zip->hash, zip->numfiles+1, Z_Malloc(Hash_BytesForBuckets(zip->numfiles+1)));
#endif
	for (i = 0; i < zip->numfiles; i++)
	{
		unzGetCurrentFileInfo (zip->handle, &file_info, newfiles[i].name, sizeof(newfiles[i].name), NULL, 0, NULL, 0);
		Q_strlwr(newfiles[i].name);
		newfiles[i].filelen = file_info.uncompressed_size;
		newfiles[i].filepos = file_info.c_offset;
#ifdef HASH_FILESYSTEM
		Hash_Add2(&zip->hash, newfiles[i].name, &newfiles[i], &newfiles[i].bucket);
#endif
		unzGoToNextFile (zip->handle);
	}

	Con_TPrintf (TL_ADDEDZIPFILE, packfile, zip->numfiles);
	return zip;
}

packfile_t *Com_FileInZip(zipfile_t *zip, char *filename)
{
//	int err;

#ifdef HASH_FILESYSTEM
	return Hash_Get(&zip->hash, filename);
#else
	int		num;

	for (num = 0; num<(int)zip->numfiles; num++)
	{
		if (!stricmp(filename, zip->files[num].name))
			return &zip->files[num];
	}
#endif
	return NULL;
}
char *Com_ReadFileInZip(zipfile_t *zip, char *buffer)
{
	int err;

	unzLocateFileMy (zip->handle, com_filenum, zip->files[com_filenum].filepos);

	unzOpenCurrentFile (zip->handle);
	err = unzReadCurrentFile (zip->handle, buffer, zip->files[com_filenum].filelen);
	unzCloseCurrentFile (zip->handle);

	if (err!=zip->files[com_filenum].filelen)
	{
		printf ("Can't extract file \"%s:%s\"", zip->filename, zip->files[com_filenum].name);
		return 0;
	}
  
	return 0;
}

int Q_strwildcmp(char *s1, char *s2);
int COM_EnumerateZipFiles (zipfile_t *zip, char *match, int (*func)(char *, int, void *), void *parm)
{
	int		num;

	for (num = 0; num<(int)zip->numfiles; num++)
	{
		if (!Q_strwildcmp(zip->files[num].name, match))
		{
			if (!func(zip->files[num].name, zip->files[num].filelen, parm))
				return false;
		}
	}

	return true;
}
#endif

#ifdef DOOMWADS
int COM_AddWad (char *descriptor)
{
	searchpath_t	*search;
	char			pakfile[MAX_OSPATH];

	sprintf (pakfile, descriptor, com_gamedir);

	for (search = com_searchpaths; search; search = search->next)
	{
		if (!stricmp(search->filename, pakfile))
			return true; //already loaded (base paths?)
	}

	return COM_LoadWadFile (descriptor);
}
#endif

void COM_AddPacks (char *descriptor)
{
	searchpath_t	*search;
	int				i;
	pack_t			*pak;
	char			pakfile[MAX_OSPATH];
	for (i=0 ; ; i++)
	{
		sprintf (pakfile, descriptor, com_gamedir, i);
		pak = COM_LoadPackFile (pakfile);
		if (!pak)
			break;
		search = Z_Malloc (sizeof(searchpath_t));
		search->type = SPT_PACK;
		strcpy (search->filename, pakfile);
		search->u.pack = pak;
		search->next = com_searchpaths;
		com_searchpaths = search;	
		
		com_fschanged = true;
	}
}

int COM_AddPacksWild (char *descriptor, int size, void *param)
{
	searchpath_t	*search;
	pack_t			*pak;
	char			pakfile[MAX_OSPATH];

	sprintf (pakfile, "%s/%s", com_gamedir, descriptor);

	for (search = com_searchpaths; search; search = search->next)
	{
		if (!stricmp(search->filename, pakfile))
			return true; //already loaded (base paths?)
	}

	pak = COM_LoadPackFile (pakfile);
	if (!pak)
		return true;
	search = Z_Malloc (sizeof(searchpath_t));
	search->type = SPT_PACK;
	strcpy (search->filename, pakfile);
	search->u.pack = pak;
	search->next = com_searchpaths;
	com_searchpaths = search;

	com_fschanged = true;

	return true;
}

#ifdef ZLIB
void COM_AddZips (char *descriptor)
{
	searchpath_t	*search;
	int				i;
	zipfile_t		*zip;
	char			pakfile[MAX_OSPATH];
	for (i=0 ; ; i++)
	{
		sprintf (pakfile, descriptor, com_gamedir, i);
		zip = COM_LoadZipFile (pakfile);
		if (!zip)
			break;
		search = Z_Malloc (sizeof(searchpath_t));
		search->type = SPT_ZIP;
		Q_strncpyz (search->filename, pakfile, sizeof(search->filename));
		search->u.zip = zip;
		search->next = com_searchpaths;
		com_searchpaths = search;

		com_fschanged = true;
	}
}

int COM_AddZipsWild (char *descriptor, int size, void *param)
{
	searchpath_t	*search;
	zipfile_t		*zip;
	char			pakfile[MAX_OSPATH];

	sprintf (pakfile, "%s/%s", com_gamedir, descriptor);

	for (search = com_searchpaths; search; search = search->next)
	{
		if (!stricmp(search->filename, pakfile))
			return true; //already loaded (base paths?)
	}

	zip = COM_LoadZipFile (pakfile);
	if (!zip)
		return true;
	search = Z_Malloc (sizeof(searchpath_t));
	search->type = SPT_ZIP;
	strcpy (search->filename, pakfile);
	search->u.zip = zip;
	search->next = com_searchpaths;
	com_searchpaths = search;

	com_fschanged = true;

	return true;
}

#endif


void COM_RefreshFSCache_f(void)
{
	com_fschanged=true;
}

void COM_FlushFSCache(void)
{
	if (com_fs_cache.value != 2)
		com_fschanged=true;
}
/*
================
COM_AddGameDirectory

Sets com_gamedir, adds the directory to the head of the path,
then loads and adds pak1.pak pak2.pak ... 
================
*/
void COM_AddGameDirectory (char *dir)
{
	searchpath_t	*search;

	char			*p;

	if ((p = strrchr(dir, '/')) != NULL)
		strcpy(gamedirfile, ++p);
	else
		strcpy(gamedirfile, p);
	strcpy (com_gamedir, dir);

	for (search = com_searchpaths; search; search = search->next)
	{
		if (!strcmp(search->filename, com_gamedir))
			return; //already loaded (base paths?)
	}

//
// add the directory to the search path
//
	search = Z_Malloc (sizeof(searchpath_t));
	search->type = SPT_OS;
	strcpy (search->filename, dir);
	search->next = com_searchpaths;
	com_searchpaths = search;

	COM_AddPacks("%s/pak%i.pak");
#ifdef ZLIB
	COM_AddZips("%s/pak%i.zip");
	COM_AddZips("%s/pak%i.pk3");
#endif

#ifdef DOOMWADS
	COM_AddWad("doom.wad");
	COM_AddWad("doom2.wad");
	COM_AddWad("heretic.wad");
	COM_AddWad("dv.wad");
#endif

	
	Sys_EnumerateFiles(com_gamedir, "*.pak", COM_AddPacksWild, NULL);
#ifdef ZLIB
	Sys_EnumerateFiles(com_gamedir, "*.pk3", COM_AddZipsWild, NULL);
	//don't do zips. we could, but don't. it's not a great idea.
#endif
}

char *COM_NextPath (char *prevpath)
{
	searchpath_t	*s;
	char			*prev;

	if (!prevpath)
		return com_gamedir;

	prev = com_gamedir;
	for (s=com_searchpaths ; s ; s=s->next)
	{
		if (s->type != SPT_OS)
			continue;

		if (prevpath == prev)
			return s->filename;
		prev = s->filename;
	}

	return NULL;
}

/*
================
COM_Gamedir

Sets the gamedir and path to a different directory.
================
*/
void COM_Gamedir (char *dir)
{
	searchpath_t	*search, *next;

	if (strstr(dir, "..") || strstr(dir, "/")
		|| strstr(dir, "\\") || strstr(dir, ":") )
	{
		Con_TPrintf (TL_GAMEDIRAINTPATH);
		return;
	}

	if (!strcmp(gamedirfile, dir))
		return;		// still the same

#ifndef SERVERONLY
	Host_WriteConfiguration();	//before we change anything.
#endif

	strcpy (gamedirfile, dir);

#ifndef CLIENTONLY
	sv.gamedirchanged = true;
#endif
#ifndef SERVERONLY
	cl.gamedirchanged = true;
#endif

	//
	// free up any current game dir info
	//
	while (com_searchpaths != com_base_searchpaths)
	{
		switch(com_searchpaths->type)
		{
		case SPT_PACK:
			fclose (com_searchpaths->u.pack->handle);
			Z_Free (com_searchpaths->u.pack->files);
#ifdef HASH_FILESYSTEM
			Z_Free (com_searchpaths->u.pack->hash.bucket);
#endif
			Z_Free (com_searchpaths->u.pack);
			break;
#ifdef ZLIB
		case SPT_ZIP:
			unzClose(com_searchpaths->u.zip->handle);
			Z_Free (com_searchpaths->u.zip->files);
#ifdef HASH_FILESYSTEM
			Z_Free (com_searchpaths->u.zip->hash.bucket);
#endif
			Z_Free (com_searchpaths->u.zip);
			break;
#endif
		case SPT_OS:
			break;
		}
		next = com_searchpaths->next;
		Z_Free (com_searchpaths);
		com_searchpaths = next;
	}

	com_fschanged = true;

	//
	// flush all data, so it will be forced to reload
	//
	Cache_Flush ();

	sprintf (com_gamedir, "%s/%s", com_basedir, dir);

	for (search = com_searchpaths; search; search = search->next)	//see if it's already loaded (base paths)
	{
		if (!strcmp(search->filename, com_gamedir))
			break;
	}

	if (!search)	//was already part of the basic.
	{
		//
		// add the directory to the search path
		//
		search = Z_Malloc (sizeof(searchpath_t));
		search->type = SPT_OS;
		strcpy (search->filename, com_gamedir);
		search->next = com_searchpaths;
		com_searchpaths = search;

		COM_AddPacks("%s/pak%i.pak");

#ifdef ZLIB
		COM_AddZips("%s/pak%i.zip");
		COM_AddZips("%s/pak%i.pk3");
#endif

#ifdef DOOMWADS
		COM_AddWad("doom.wad");
		COM_AddWad("doom2.wad");
		COM_AddWad("dv.wad");
#endif

		Sys_EnumerateFiles(com_gamedir, "*.pak", COM_AddPacksWild, NULL);
#ifdef ZLIB
		Sys_EnumerateFiles(com_gamedir, "*.pk3", COM_AddZipsWild, NULL);
		//don't do zips. we could, but don't. it's not a great idea.
#endif

		com_fschanged = true;
	}

#ifndef SERVERONLY
	{
		char	fn[MAX_OSPATH];
		FILE *f;

//		if (qrenderer)	//only do this if we have already started the renderer
//			Cbuf_InsertText("vid_restart\n", RESTRICT_LOCAL);

		sprintf(fn, "%s/%s", com_gamedir, "config.cfg");
		if ((f = fopen(fn, "r")) != NULL)
		{
			fclose(f);
			Cbuf_InsertText("cl_warncmd 1\n", RESTRICT_LOCAL);
			Cbuf_InsertText("exec frontend.cfg\n", RESTRICT_LOCAL);
			Cbuf_InsertText("exec config.cfg\n", RESTRICT_LOCAL);
			Cbuf_InsertText("cl_warncmd 0\n", RESTRICT_LOCAL);
		}
	}

	Validation_FlushFileList();	//prevent previous hacks from making a difference.

	//FIXME: load new palette, if different cause a vid_restart.

#endif
}

/*
================
COM_InitFilesystem
================
*/
void COM_InitFilesystem (void)
{
	FILE *f;
	int		i;

//
// -basedir <path>
// Overrides the system supplied base directory (under id1)
//
	i = COM_CheckParm ("-basedir");
	if (i && i < com_argc-1)
		strcpy (com_basedir, com_argv[i+1]);
	else
		strcpy (com_basedir, host_parms.basedir);

//
// start up with id1 by default
//
	i = COM_CheckParm ("-basegame");
	if (i && i < com_argc-1)
	{
		do	//use multiple -basegames
		{
			COM_AddGameDirectory (va("%s/%s", com_basedir, com_argv[i+1]) );

			i = COM_CheckNextParm ("-basegame", i);
		}
		while (i && i < com_argc-1);
	}
	else
	{
		//if there is no pak0.pak file in id1, and baseq2 has one, use that instead.
		f = fopen(va("%s/id1/pak0.pak", com_basedir), "rb");
		if (f)
		{
			fclose(f);
			COM_AddGameDirectory (va("%s/id1", com_basedir) );
		}
		else
		{
			f = fopen(va("%s/baseq2/pak0.pak", com_basedir), "rb");
			if (f)
			{
				fclose(f);
				COM_AddGameDirectory (va("%s/baseq2", com_basedir) );
			}
			else
			{	//hexen2.
				f = fopen(va("%s/data1/pak0.pak", com_basedir), "rb");
				if (f)
				{
					fclose(f);
					COM_AddGameDirectory (va("%s/data1", com_basedir) );
				}
				else
					COM_AddGameDirectory (va("%s/id1", com_basedir) );
			}
		}
	}
	COM_AddGameDirectory (va("%s/qw", com_basedir) );

	// any set gamedirs will be freed up to here
	com_base_searchpaths = com_searchpaths;

	i = COM_CheckParm ("-game");	//effectivly replace with +gamedir x (But overridable)
	if (i && i < com_argc-1)
	{
		COM_AddGameDirectory (va("%s/%s", com_basedir, com_argv[i+1]) );

#ifndef CLIENTONLY
		Info_SetValueForStarKey (svs.info, "*gamedir", com_argv[i+1], MAX_SERVERINFO_STRING);
#endif
	}
}



/*
=====================================================================

  INFO STRINGS

=====================================================================
*/

/*
===============
Info_ValueForKey

Searches the string for the given
key and returns the associated value, or an empty string.
===============
*/
char *Info_ValueForKey (char *s, char *key)
{
	char	pkey[1024];
	static	char value[4][1024];	// use two buffers so compares
								// work without stomping on each other
	static	int	valueindex;
	char	*o;
	
	valueindex = (valueindex + 1) % 4;
	if (*s == '\\')
		s++;
	while (1)
	{
		o = pkey;
		while (*s != '\\')
		{
			if (!*s)
			{
				*value[valueindex]='\0';
				return value[valueindex];
			}
			*o++ = *s++;
		}
		*o = 0;
		s++;

		o = value[valueindex];

		while (*s != '\\' && *s)
		{
			if (!*s)
			{
				*value[valueindex]='\0';
				return value[valueindex];
			}
			*o++ = *s++;
		}
		*o = 0;

		if (!strcmp (key, pkey) )
			return value[valueindex];

		if (!*s)
		{
			*value[valueindex]='\0';
			return value[valueindex];
		}
		s++;
	}
}

void Info_RemoveKey (char *s, char *key)
{
	char	*start;
	char	pkey[1024];
	char	value[1024];
	char	*o;

	if (strstr (key, "\\"))
	{
		Con_TPrintf (TL_KEYHASSLASH);
		return;
	}

	while (1)
	{
		start = s;
		if (*s == '\\')
			s++;
		o = pkey;
		while (*s != '\\')
		{
			if (!*s)
				return;
			*o++ = *s++;
		}
		*o = 0;
		s++;

		o = value;
		while (*s != '\\' && *s)
		{
			if (!*s)
				return;
			*o++ = *s++;
		}
		*o = 0;

		if (!strcmp (key, pkey) )
		{
			strcpy (start, s);	// remove this part
			return;
		}

		if (!*s)
			return;
	}

}

void Info_RemovePrefixedKeys (char *start, char prefix)
{
	char	*s;
	char	pkey[1024];
	char	value[1024];
	char	*o;

	s = start;

	while (1)
	{
		if (*s == '\\')
			s++;
		o = pkey;
		while (*s != '\\')
		{
			if (!*s)
				return;
			*o++ = *s++;
		}
		*o = 0;
		s++;

		o = value;
		while (*s != '\\' && *s)
		{
			if (!*s)
				return;
			*o++ = *s++;
		}
		*o = 0;

		if (pkey[0] == prefix)
		{
			Info_RemoveKey (start, pkey);
			s = start;
		}

		if (!*s)
			return;
	}

}

void Info_RemoveNonStarKeys (char *start)
{
	char	*s;
	char	pkey[1024];
	char	value[1024];
	char	*o;

	s = start;

	while (1)
	{
		if (*s == '\\')
			s++;
		o = pkey;
		while (*s != '\\')
		{
			if (!*s)
				return;
			*o++ = *s++;
		}
		*o = 0;
		s++;

		o = value;
		while (*s != '\\' && *s)
		{
			if (!*s)
				return;
			*o++ = *s++;
		}
		*o = 0;

		if (pkey[0] != '*')
		{
			Info_RemoveKey (start, pkey);
			s = start;
		}

		if (!*s)
			return;
	}

}

void Info_SetValueForStarKey (char *s, char *key, char *value, int maxsize)
{
	char	new[1024], *v;
	int		c;
#ifdef SERVERONLY
	extern cvar_t sv_highchars;
#endif

	if (strstr (key, "\\") || strstr (value, "\\") )
	{
		Con_TPrintf (TL_KEYHASSLASH);
		return;
	}

	if (strstr (key, "\"") || strstr (value, "\"") )
	{
		Con_TPrintf (TL_KEYHASQUOTE);
		return;
	}

	if (strlen(key) >= MAX_INFO_KEY || strlen(value) >= MAX_INFO_KEY)
	{
		Con_TPrintf (TL_KEYTOOLONG);
		return;
	}

	// this next line is kinda trippy
	if (*(v = Info_ValueForKey(s, key)))
	{
		// key exists, make sure we have enough room for new value, if we don't,
		// don't change it!
		if (strlen(value) - strlen(v) + strlen(s) + 1 > maxsize)
		{
			if (*Info_ValueForKey(s, "*ver"))	//quick hack to kill off unneeded info on overflow. We can't simply increase the quantity of this stuff.
			{
				Info_RemoveKey(s, "*ver");
				Info_SetValueForStarKey (s, key, value, maxsize);
				return;
			}
			Con_TPrintf (TL_INFOSTRINGTOOLONG);
			return;
		}
	}
	Info_RemoveKey (s, key);
	if (!value || !strlen(value))
		return;

	sprintf (new, "\\%s\\%s", key, value);

	if ((int)(strlen(new) + strlen(s) + 1) > maxsize)
	{
		Con_TPrintf (TL_INFOSTRINGTOOLONG);
		return;
	}

	// only copy ascii values
	s += strlen(s);
	v = new;
	while (*v)
	{
		c = (unsigned char)*v++;
#ifndef SERVERONLY
		// client only allows highbits on name
		if (stricmp(key, "name") != 0) {
			c &= 127;
			if (c < 32 || c > 127)
				continue;
			// auto lowercase team
			if (stricmp(key, "team") == 0)
				c = tolower(c);
		}
#else
		if (!sv_highchars.value) {
			c &= 127;
			if (c < 32 || c > 127)
				continue;
		}
#endif
//		c &= 127;		// strip high bits
		if (c > 13) // && c < 127)
			*s++ = c;
	}
	*s = 0;
}

void Info_SetValueForKey (char *s, char *key, char *value, int maxsize)
{
	if (key[0] == '*')
	{
		Con_TPrintf (TL_STARKEYPROTECTED);
		return;
	}

	Info_SetValueForStarKey (s, key, value, maxsize);
}

void Info_Print (char *s)
{
	char	key[1024];
	char	value[1024];
	char	*o;
	int		l;

	if (*s == '\\')
		s++;
	while (*s)
	{
		o = key;
		while (*s && *s != '\\')
			*o++ = *s++;

		l = o - key;
		if (l < 20)
		{
			memset (o, ' ', 20-l);
			key[20] = 0;
		}
		else
			*o = 0;
		Con_Printf ("%s", key);

		if (!*s)
		{
			Con_TPrintf (TL_KEYHASNOVALUE);
			return;
		}

		o = value;
		s++;
		while (*s && *s != '\\')
			*o++ = *s++;
		*o = 0;

		if (*s)
			s++;
		Con_Printf ("%s\n", value);
	}
}

static qbyte chktbl[1024 + 4] = {
0x78,0xd2,0x94,0xe3,0x41,0xec,0xd6,0xd5,0xcb,0xfc,0xdb,0x8a,0x4b,0xcc,0x85,0x01,
0x23,0xd2,0xe5,0xf2,0x29,0xa7,0x45,0x94,0x4a,0x62,0xe3,0xa5,0x6f,0x3f,0xe1,0x7a,
0x64,0xed,0x5c,0x99,0x29,0x87,0xa8,0x78,0x59,0x0d,0xaa,0x0f,0x25,0x0a,0x5c,0x58,
0xfb,0x00,0xa7,0xa8,0x8a,0x1d,0x86,0x80,0xc5,0x1f,0xd2,0x28,0x69,0x71,0x58,0xc3,
0x51,0x90,0xe1,0xf8,0x6a,0xf3,0x8f,0xb0,0x68,0xdf,0x95,0x40,0x5c,0xe4,0x24,0x6b,
0x29,0x19,0x71,0x3f,0x42,0x63,0x6c,0x48,0xe7,0xad,0xa8,0x4b,0x91,0x8f,0x42,0x36,
0x34,0xe7,0x32,0x55,0x59,0x2d,0x36,0x38,0x38,0x59,0x9b,0x08,0x16,0x4d,0x8d,0xf8,
0x0a,0xa4,0x52,0x01,0xbb,0x52,0xa9,0xfd,0x40,0x18,0x97,0x37,0xff,0xc9,0x82,0x27,
0xb2,0x64,0x60,0xce,0x00,0xd9,0x04,0xf0,0x9e,0x99,0xbd,0xce,0x8f,0x90,0x4a,0xdd,
0xe1,0xec,0x19,0x14,0xb1,0xfb,0xca,0x1e,0x98,0x0f,0xd4,0xcb,0x80,0xd6,0x05,0x63,
0xfd,0xa0,0x74,0xa6,0x86,0xf6,0x19,0x98,0x76,0x27,0x68,0xf7,0xe9,0x09,0x9a,0xf2,
0x2e,0x42,0xe1,0xbe,0x64,0x48,0x2a,0x74,0x30,0xbb,0x07,0xcc,0x1f,0xd4,0x91,0x9d,
0xac,0x55,0x53,0x25,0xb9,0x64,0xf7,0x58,0x4c,0x34,0x16,0xbc,0xf6,0x12,0x2b,0x65,
0x68,0x25,0x2e,0x29,0x1f,0xbb,0xb9,0xee,0x6d,0x0c,0x8e,0xbb,0xd2,0x5f,0x1d,0x8f,
0xc1,0x39,0xf9,0x8d,0xc0,0x39,0x75,0xcf,0x25,0x17,0xbe,0x96,0xaf,0x98,0x9f,0x5f,
0x65,0x15,0xc4,0x62,0xf8,0x55,0xfc,0xab,0x54,0xcf,0xdc,0x14,0x06,0xc8,0xfc,0x42,
0xd3,0xf0,0xad,0x10,0x08,0xcd,0xd4,0x11,0xbb,0xca,0x67,0xc6,0x48,0x5f,0x9d,0x59,
0xe3,0xe8,0x53,0x67,0x27,0x2d,0x34,0x9e,0x9e,0x24,0x29,0xdb,0x69,0x99,0x86,0xf9,
0x20,0xb5,0xbb,0x5b,0xb0,0xf9,0xc3,0x67,0xad,0x1c,0x9c,0xf7,0xcc,0xef,0xce,0x69,
0xe0,0x26,0x8f,0x79,0xbd,0xca,0x10,0x17,0xda,0xa9,0x88,0x57,0x9b,0x15,0x24,0xba,
0x84,0xd0,0xeb,0x4d,0x14,0xf5,0xfc,0xe6,0x51,0x6c,0x6f,0x64,0x6b,0x73,0xec,0x85,
0xf1,0x6f,0xe1,0x67,0x25,0x10,0x77,0x32,0x9e,0x85,0x6e,0x69,0xb1,0x83,0x00,0xe4,
0x13,0xa4,0x45,0x34,0x3b,0x40,0xff,0x41,0x82,0x89,0x79,0x57,0xfd,0xd2,0x8e,0xe8,
0xfc,0x1d,0x19,0x21,0x12,0x00,0xd7,0x66,0xe5,0xc7,0x10,0x1d,0xcb,0x75,0xe8,0xfa,
0xb6,0xee,0x7b,0x2f,0x1a,0x25,0x24,0xb9,0x9f,0x1d,0x78,0xfb,0x84,0xd0,0x17,0x05,
0x71,0xb3,0xc8,0x18,0xff,0x62,0xee,0xed,0x53,0xab,0x78,0xd3,0x65,0x2d,0xbb,0xc7,
0xc1,0xe7,0x70,0xa2,0x43,0x2c,0x7c,0xc7,0x16,0x04,0xd2,0x45,0xd5,0x6b,0x6c,0x7a,
0x5e,0xa1,0x50,0x2e,0x31,0x5b,0xcc,0xe8,0x65,0x8b,0x16,0x85,0xbf,0x82,0x83,0xfb,
0xde,0x9f,0x36,0x48,0x32,0x79,0xd6,0x9b,0xfb,0x52,0x45,0xbf,0x43,0xf7,0x0b,0x0b,
0x19,0x19,0x31,0xc3,0x85,0xec,0x1d,0x8c,0x20,0xf0,0x3a,0xfa,0x80,0x4d,0x2c,0x7d,
0xac,0x60,0x09,0xc0,0x40,0xee,0xb9,0xeb,0x13,0x5b,0xe8,0x2b,0xb1,0x20,0xf0,0xce,
0x4c,0xbd,0xc6,0x04,0x86,0x70,0xc6,0x33,0xc3,0x15,0x0f,0x65,0x19,0xfd,0xc2,0xd3,

// map checksum goes here
0x00,0x00,0x00,0x00
};

#if 0

static qbyte chkbuf[16 + 60 + 4];

static unsigned last_mapchecksum = 0;


/*
====================
COM_BlockSequenceCheckByte

For proxy protecting
====================
*/
qbyte	COM_BlockSequenceCheckByte (qbyte *base, int length, int sequence, unsigned mapchecksum)
{
	int		checksum;
	qbyte	*p;

	if (last_mapchecksum != mapchecksum) {
		last_mapchecksum = mapchecksum;
		chktbl[1024] = (mapchecksum & 0xff000000) >> 24;
		chktbl[1025] = (mapchecksum & 0x00ff0000) >> 16;
		chktbl[1026] = (mapchecksum & 0x0000ff00) >> 8;
		chktbl[1027] = (mapchecksum & 0x000000ff);

		Com_BlockFullChecksum (chktbl, sizeof(chktbl), chkbuf);
	}

	p = chktbl + (sequence % (sizeof(chktbl) - 8));

	if (length > 60)
		length = 60;
	memcpy (chkbuf + 16, base, length);

	length += 16;

	chkbuf[length] = (sequence & 0xff) ^ p[0];
	chkbuf[length+1] = p[1];
	chkbuf[length+2] = ((sequence>>8) & 0xff) ^ p[2];
	chkbuf[length+3] = p[3];

	length += 4;

	checksum = LittleLong(Com_BlockChecksum (chkbuf, length));

	checksum &= 0xff;

	return checksum;
}
#endif

/*
====================
COM_BlockSequenceCRCByte

For proxy protecting
====================
*/
qbyte	COM_BlockSequenceCRCByte (qbyte *base, int length, int sequence)
{
	unsigned short crc;
	qbyte	*p;
	qbyte chkb[60 + 4];

	p = chktbl + (sequence % (sizeof(chktbl) - 8));

	if (length > 60)
		length = 60;
	memcpy (chkb, base, length);

	chkb[length] = (sequence & 0xff) ^ p[0];
	chkb[length+1] = p[1];
	chkb[length+2] = ((sequence>>8) & 0xff) ^ p[2];
	chkb[length+3] = p[3];

	length += 4;

	crc = CRC_Block(chkb, length);

	crc &= 0xff;

	return crc;
}


#if defined(Q2CLIENT) || defined(Q2SERVER)
static qbyte q2chktbl[1024] = {
0x84, 0x47, 0x51, 0xc1, 0x93, 0x22, 0x21, 0x24, 0x2f, 0x66, 0x60, 0x4d, 0xb0, 0x7c, 0xda,
0x88, 0x54, 0x15, 0x2b, 0xc6, 0x6c, 0x89, 0xc5, 0x9d, 0x48, 0xee, 0xe6, 0x8a, 0xb5, 0xf4,
0xcb, 0xfb, 0xf1, 0x0c, 0x2e, 0xa0, 0xd7, 0xc9, 0x1f, 0xd6, 0x06, 0x9a, 0x09, 0x41, 0x54,
0x67, 0x46, 0xc7, 0x74, 0xe3, 0xc8, 0xb6, 0x5d, 0xa6, 0x36, 0xc4, 0xab, 0x2c, 0x7e, 0x85,
0xa8, 0xa4, 0xa6, 0x4d, 0x96, 0x19, 0x19, 0x9a, 0xcc, 0xd8, 0xac, 0x39, 0x5e, 0x3c, 0xf2,
0xf5, 0x5a, 0x72, 0xe5, 0xa9, 0xd1, 0xb3, 0x23, 0x82, 0x6f, 0x29, 0xcb, 0xd1, 0xcc, 0x71,
0xfb, 0xea, 0x92, 0xeb, 0x1c, 0xca, 0x4c, 0x70, 0xfe, 0x4d, 0xc9, 0x67, 0x43, 0x47, 0x94,
0xb9, 0x47, 0xbc, 0x3f, 0x01, 0xab, 0x7b, 0xa6, 0xe2, 0x76, 0xef, 0x5a, 0x7a, 0x29, 0x0b,
0x51, 0x54, 0x67, 0xd8, 0x1c, 0x14, 0x3e, 0x29, 0xec, 0xe9, 0x2d, 0x48, 0x67, 0xff, 0xed,
0x54, 0x4f, 0x48, 0xc0, 0xaa, 0x61, 0xf7, 0x78, 0x12, 0x03, 0x7a, 0x9e, 0x8b, 0xcf, 0x83,
0x7b, 0xae, 0xca, 0x7b, 0xd9, 0xe9, 0x53, 0x2a, 0xeb, 0xd2, 0xd8, 0xcd, 0xa3, 0x10, 0x25,
0x78, 0x5a, 0xb5, 0x23, 0x06, 0x93, 0xb7, 0x84, 0xd2, 0xbd, 0x96, 0x75, 0xa5, 0x5e, 0xcf,
0x4e, 0xe9, 0x50, 0xa1, 0xe6, 0x9d, 0xb1, 0xe3, 0x85, 0x66, 0x28, 0x4e, 0x43, 0xdc, 0x6e,
0xbb, 0x33, 0x9e, 0xf3, 0x0d, 0x00, 0xc1, 0xcf, 0x67, 0x34, 0x06, 0x7c, 0x71, 0xe3, 0x63,
0xb7, 0xb7, 0xdf, 0x92, 0xc4, 0xc2, 0x25, 0x5c, 0xff, 0xc3, 0x6e, 0xfc, 0xaa, 0x1e, 0x2a,
0x48, 0x11, 0x1c, 0x36, 0x68, 0x78, 0x86, 0x79, 0x30, 0xc3, 0xd6, 0xde, 0xbc, 0x3a, 0x2a,
0x6d, 0x1e, 0x46, 0xdd, 0xe0, 0x80, 0x1e, 0x44, 0x3b, 0x6f, 0xaf, 0x31, 0xda, 0xa2, 0xbd,
0x77, 0x06, 0x56, 0xc0, 0xb7, 0x92, 0x4b, 0x37, 0xc0, 0xfc, 0xc2, 0xd5, 0xfb, 0xa8, 0xda,
0xf5, 0x57, 0xa8, 0x18, 0xc0, 0xdf, 0xe7, 0xaa, 0x2a, 0xe0, 0x7c, 0x6f, 0x77, 0xb1, 0x26,
0xba, 0xf9, 0x2e, 0x1d, 0x16, 0xcb, 0xb8, 0xa2, 0x44, 0xd5, 0x2f, 0x1a, 0x79, 0x74, 0x87,
0x4b, 0x00, 0xc9, 0x4a, 0x3a, 0x65, 0x8f, 0xe6, 0x5d, 0xe5, 0x0a, 0x77, 0xd8, 0x1a, 0x14,
0x41, 0x75, 0xb1, 0xe2, 0x50, 0x2c, 0x93, 0x38, 0x2b, 0x6d, 0xf3, 0xf6, 0xdb, 0x1f, 0xcd,
0xff, 0x14, 0x70, 0xe7, 0x16, 0xe8, 0x3d, 0xf0, 0xe3, 0xbc, 0x5e, 0xb6, 0x3f, 0xcc, 0x81,
0x24, 0x67, 0xf3, 0x97, 0x3b, 0xfe, 0x3a, 0x96, 0x85, 0xdf, 0xe4, 0x6e, 0x3c, 0x85, 0x05,
0x0e, 0xa3, 0x2b, 0x07, 0xc8, 0xbf, 0xe5, 0x13, 0x82, 0x62, 0x08, 0x61, 0x69, 0x4b, 0x47,
0x62, 0x73, 0x44, 0x64, 0x8e, 0xe2, 0x91, 0xa6, 0x9a, 0xb7, 0xe9, 0x04, 0xb6, 0x54, 0x0c,
0xc5, 0xa9, 0x47, 0xa6, 0xc9, 0x08, 0xfe, 0x4e, 0xa6, 0xcc, 0x8a, 0x5b, 0x90, 0x6f, 0x2b,
0x3f, 0xb6, 0x0a, 0x96, 0xc0, 0x78, 0x58, 0x3c, 0x76, 0x6d, 0x94, 0x1a, 0xe4, 0x4e, 0xb8,
0x38, 0xbb, 0xf5, 0xeb, 0x29, 0xd8, 0xb0, 0xf3, 0x15, 0x1e, 0x99, 0x96, 0x3c, 0x5d, 0x63,
0xd5, 0xb1, 0xad, 0x52, 0xb8, 0x55, 0x70, 0x75, 0x3e, 0x1a, 0xd5, 0xda, 0xf6, 0x7a, 0x48,
0x7d, 0x44, 0x41, 0xf9, 0x11, 0xce, 0xd7, 0xca, 0xa5, 0x3d, 0x7a, 0x79, 0x7e, 0x7d, 0x25,
0x1b, 0x77, 0xbc, 0xf7, 0xc7, 0x0f, 0x84, 0x95, 0x10, 0x92, 0x67, 0x15, 0x11, 0x5a, 0x5e,
0x41, 0x66, 0x0f, 0x38, 0x03, 0xb2, 0xf1, 0x5d, 0xf8, 0xab, 0xc0, 0x02, 0x76, 0x84, 0x28,
0xf4, 0x9d, 0x56, 0x46, 0x60, 0x20, 0xdb, 0x68, 0xa7, 0xbb, 0xee, 0xac, 0x15, 0x01, 0x2f,
0x20, 0x09, 0xdb, 0xc0, 0x16, 0xa1, 0x89, 0xf9, 0x94, 0x59, 0x00, 0xc1, 0x76, 0xbf, 0xc1,
0x4d, 0x5d, 0x2d, 0xa9, 0x85, 0x2c, 0xd6, 0xd3, 0x14, 0xcc, 0x02, 0xc3, 0xc2, 0xfa, 0x6b,
0xb7, 0xa6, 0xef, 0xdd, 0x12, 0x26, 0xa4, 0x63, 0xe3, 0x62, 0xbd, 0x56, 0x8a, 0x52, 0x2b,
0xb9, 0xdf, 0x09, 0xbc, 0x0e, 0x97, 0xa9, 0xb0, 0x82, 0x46, 0x08, 0xd5, 0x1a, 0x8e, 0x1b,
0xa7, 0x90, 0x98, 0xb9, 0xbb, 0x3c, 0x17, 0x9a, 0xf2, 0x82, 0xba, 0x64, 0x0a, 0x7f, 0xca,
0x5a, 0x8c, 0x7c, 0xd3, 0x79, 0x09, 0x5b, 0x26, 0xbb, 0xbd, 0x25, 0xdf, 0x3d, 0x6f, 0x9a,
0x8f, 0xee, 0x21, 0x66, 0xb0, 0x8d, 0x84, 0x4c, 0x91, 0x45, 0xd4, 0x77, 0x4f, 0xb3, 0x8c,
0xbc, 0xa8, 0x99, 0xaa, 0x19, 0x53, 0x7c, 0x02, 0x87, 0xbb, 0x0b, 0x7c, 0x1a, 0x2d, 0xdf,
0x48, 0x44, 0x06, 0xd6, 0x7d, 0x0c, 0x2d, 0x35, 0x76, 0xae, 0xc4, 0x5f, 0x71, 0x85, 0x97,
0xc4, 0x3d, 0xef, 0x52, 0xbe, 0x00, 0xe4, 0xcd, 0x49, 0xd1, 0xd1, 0x1c, 0x3c, 0xd0, 0x1c,
0x42, 0xaf, 0xd4, 0xbd, 0x58, 0x34, 0x07, 0x32, 0xee, 0xb9, 0xb5, 0xea, 0xff, 0xd7, 0x8c,
0x0d, 0x2e, 0x2f, 0xaf, 0x87, 0xbb, 0xe6, 0x52, 0x71, 0x22, 0xf5, 0x25, 0x17, 0xa1, 0x82,
0x04, 0xc2, 0x4a, 0xbd, 0x57, 0xc6, 0xab, 0xc8, 0x35, 0x0c, 0x3c, 0xd9, 0xc2, 0x43, 0xdb,
0x27, 0x92, 0xcf, 0xb8, 0x25, 0x60, 0xfa, 0x21, 0x3b, 0x04, 0x52, 0xc8, 0x96, 0xba, 0x74,
0xe3, 0x67, 0x3e, 0x8e, 0x8d, 0x61, 0x90, 0x92, 0x59, 0xb6, 0x1a, 0x1c, 0x5e, 0x21, 0xc1,
0x65, 0xe5, 0xa6, 0x34, 0x05, 0x6f, 0xc5, 0x60, 0xb1, 0x83, 0xc1, 0xd5, 0xd5, 0xed, 0xd9,
0xc7, 0x11, 0x7b, 0x49, 0x7a, 0xf9, 0xf9, 0x84, 0x47, 0x9b, 0xe2, 0xa5, 0x82, 0xe0, 0xc2,
0x88, 0xd0, 0xb2, 0x58, 0x88, 0x7f, 0x45, 0x09, 0x67, 0x74, 0x61, 0xbf, 0xe6, 0x40, 0xe2,
0x9d, 0xc2, 0x47, 0x05, 0x89, 0xed, 0xcb, 0xbb, 0xb7, 0x27, 0xe7, 0xdc, 0x7a, 0xfd, 0xbf,
0xa8, 0xd0, 0xaa, 0x10, 0x39, 0x3c, 0x20, 0xf0, 0xd3, 0x6e, 0xb1, 0x72, 0xf8, 0xe6, 0x0f,
0xef, 0x37, 0xe5, 0x09, 0x33, 0x5a, 0x83, 0x43, 0x80, 0x4f, 0x65, 0x2f, 0x7c, 0x8c, 0x6a,
0xa0, 0x82, 0x0c, 0xd4, 0xd4, 0xfa, 0x81, 0x60, 0x3d, 0xdf, 0x06, 0xf1, 0x5f, 0x08, 0x0d,
0x6d, 0x43, 0xf2, 0xe3, 0x11, 0x7d, 0x80, 0x32, 0xc5, 0xfb, 0xc5, 0xd9, 0x27, 0xec, 0xc6,
0x4e, 0x65, 0x27, 0x76, 0x87, 0xa6, 0xee, 0xee, 0xd7, 0x8b, 0xd1, 0xa0, 0x5c, 0xb0, 0x42,
0x13, 0x0e, 0x95, 0x4a, 0xf2, 0x06, 0xc6, 0x43, 0x33, 0xf4, 0xc7, 0xf8, 0xe7, 0x1f, 0xdd,
0xe4, 0x46, 0x4a, 0x70, 0x39, 0x6c, 0xd0, 0xed, 0xca, 0xbe, 0x60, 0x3b, 0xd1, 0x7b, 0x57,
0x48, 0xe5, 0x3a, 0x79, 0xc1, 0x69, 0x33, 0x53, 0x1b, 0x80, 0xb8, 0x91, 0x7d, 0xb4, 0xf6,
0x17, 0x1a, 0x1d, 0x5a, 0x32, 0xd6, 0xcc, 0x71, 0x29, 0x3f, 0x28, 0xbb, 0xf3, 0x5e, 0x71,
0xb8, 0x43, 0xaf, 0xf8, 0xb9, 0x64, 0xef, 0xc4, 0xa5, 0x6c, 0x08, 0x53, 0xc7, 0x00, 0x10,
0x39, 0x4f, 0xdd, 0xe4, 0xb6, 0x19, 0x27, 0xfb, 0xb8, 0xf5, 0x32, 0x73, 0xe5, 0xcb, 0x32
};

/*
====================
COM_BlockSequenceCRCByte

For proxy protecting
====================
*/
qbyte	Q2COM_BlockSequenceCRCByte (qbyte *base, int length, int sequence)
{
	int		n;
	qbyte	*p;
	int		x;
	qbyte chkb[60 + 4];
	unsigned short crc;


	if (sequence < 0)
		Sys_Error("sequence < 0, this shouldn't happen\n");

	p = q2chktbl + (sequence % (sizeof(q2chktbl) - 4));

	if (length > 60)
		length = 60;
	memcpy (chkb, base, length);

	chkb[length] = p[0];
	chkb[length+1] = p[1];
	chkb[length+2] = p[2];
	chkb[length+3] = p[3];

	length += 4;

	crc = CRC_Block(chkb, length);

	for (x=0, n=0; n<length; n++)
		x += chkb[n];

	crc = (crc ^ x) & 0xff;

	return crc;
}

#endif

// char *date = "Oct 24 1996";
static char *date = __DATE__ ;
static char *mon[12] = 
{ "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };
static char mond[12] = 
{ 31,    28,    31,    30,    31,    30,    31,    31,    30,    31,    30,    31 };

// returns days since Oct 24 1996
int build_number( void )
{
	int m = 0; 
	int d = 0;
	int y = 0;
	static int b = 0;

	if (b != 0)
		return b;

	for (m = 0; m < 11; m++)
	{
		if (Q_strncasecmp( &date[0], mon[m], 3 ) == 0)
			break;
		d += mond[m];
	}

	d += atoi( &date[4] ) - 1;

	y = atoi( &date[7] ) - 1900;

	b = d + (int)((y - 1) * 365.25);

	if (((y % 4) == 0) && m > 1)
	{
		b += 1;
	}

	b -= 35778; // Dec 16 1998

	return b;
}

