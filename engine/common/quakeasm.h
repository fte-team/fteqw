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
//
// quakeasm.h: general asm header file
//
#ifdef _WIN32
#define __i386__	1
#endif

#if defined(NOASM)
#define id386 0
#else
#ifdef	__i386__
#define id386	1
#else
#define id386	0
#endif
#endif


#ifdef MQUAKE
#define GLQUAKE
#endif

// !!! must be kept the same as in d_iface.h !!!
#define TRANSPARENT_COLOR	255

#ifndef NeXT

	.extern C(paintbuffer)
//	.extern C(snd_linear_count)
	.extern C(cursndcard)
	.extern C(snd_p)
	.extern C(snd_vol)
	.extern C(snd_out)
	.extern C(vright)
	.extern C(vup)
	.extern C(vpn)
	.extern C(BOPS_Error)

#endif
