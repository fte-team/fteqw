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

#ifndef __ASM_I386__
#define __ASM_I386__

#if defined(CYGORELF) || defined(ELF)
#define C(label) label
#else
#define C(label) _##label
#endif

//
// !!! note that this file must match the corresponding C structures at all
// times !!!
//

// plane_t structure
// !!! if this is changed, it must be changed in model.h too !!!
// !!! if the size of this is changed, the array lookup in SV_HullPointContents
//     must be changed too !!!
#define pl_normal	0
#define pl_dist		12
#define pl_type		16
#define pl_signbits	17
#define pl_pad		18
#define pl_size		20

// hull_t structure
// !!! if this is changed, it must be changed in model.h too !!!
#define	hu_clipnodes		0
#define	hu_planes			4
#define	hu_firstclipnode	8
#define	hu_lastclipnode		12
#define	hu_clip_mins		16
#define	hu_clip_maxs		28
#define hu_available		40
#define hu_size  			44

// dnode_t structure
// !!! if this is changed, it must be changed in bspfile.h too !!!
#define	nd_planenum		0
#define	nd_children		4
#define	nd_mins			8
#define	nd_maxs			20
#define	nd_firstface	32
#define	nd_numfaces		36
#define nd_size			40

// sfxcache_t structure
// !!! if this is changed, it much be changed in sound.h too !!!
#define sfxc_length		0
#define sfxc_loopstart	4
#define sfxc_speed		8
#define sfxc_width		12
#define sfxc_stereo		16
#define sfxc_data		20

// channel_t structure
// !!! if this is changed, it much be changed in sound.h too !!!
#define MAXSOUNDCHANNELS 6
#define ch_sfx			0
#define ch_vol			ch_sfx+4
#define ch_end			ch_vol+4*MAXSOUNDCHANNELS
#define ch_pos			ch_end+4
#define ch_looping		ch_looping+4
#define ch_entnum		ch_entnum+4
#define ch_entchannel	ch_entchannel+4
#define ch_origin		ch_origin+4
#define ch_dist_mult	ch_dist_mult+4
#define ch_master_vol	ch_master_vol+4
#define ch_size			ch_size+4

// portable_samplepair_t structure
// !!! if this is changed, it much be changed in sound.h too !!!
#define psp_left		0
#define psp_right		4
#define psp_size		8

#endif

