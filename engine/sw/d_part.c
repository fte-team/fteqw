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
// d_part.c: software driver module for drawing particles

#include "quakedef.h"
#include "d_local.h"

//Spike: Particles are depth sorted. So why depth write? They are the last to be drawn anyway.


/*
==============
D_EndParticles
==============
*/
void D_EndParticles (void)
{
// not used by software driver
}


/*
==============
D_StartParticles
==============
*/
void D_StartParticles (void)
{
// not used by software driver
}

#if	!id386

/*
==============
D_DrawParticle
==============
*/
void D_DrawParticle (particle_t *pparticle)
{
	vec3_t	local, transformed;
	float	zi;
	qbyte	*pdest;
	short	*pz;
	int		i, izi, pix, count, u, v;

// transform point
	VectorSubtract (pparticle->org, r_origin, local);

	transformed[0] = DotProduct(local, r_pright);
	transformed[1] = DotProduct(local, r_pup);
	transformed[2] = DotProduct(local, r_ppn);		

	if (transformed[2] < PARTICLE_Z_CLIP)
		return;

// project the point
// FIXME: preadjust xcenter and ycenter
	zi = 1.0 / transformed[2];
	u = (int)(xcenter + zi * transformed[0] + 0.5);
	v = (int)(ycenter - zi * transformed[1] + 0.5);

	if ((v > d_vrectbottom_particle) || 
		(u > d_vrectright_particle) ||
		(v < d_vrecty) ||
		(u < d_vrectx))
	{
		return;
	}

	pz = d_pzbuffer + (d_zwidth * v) + u;
	pdest = d_viewbuffer + d_scantable[v] + u;
	izi = (int)(zi * 0x8000);

	pix = izi >> d_pix_shift;
	pix *= pparticle->scale;

	if (pix < d_pix_min)
		pix = d_pix_min;
	else if (pix > d_pix_max)
		pix = d_pix_max;

	switch (pix)
	{
	case 1:
		count = 1 << d_y_aspect_shift;

		for ( ; count ; count--, pz += d_zwidth, pdest += screenwidth)
		{
			if (pz[0] <= izi)
			{
//				pz[0] = izi;
				pdest[0] = pparticle->color;
			}
		}
		break;

	case 2:
		count = 2 << d_y_aspect_shift;

		for ( ; count ; count--, pz += d_zwidth, pdest += screenwidth)
		{
			if (pz[0] <= izi)
			{
//				pz[0] = izi;
				pdest[0] = pparticle->color;
			}

			if (pz[1] <= izi)
			{
//				pz[1] = izi;
				pdest[1] = pparticle->color;
			}
		}
		break;

	case 3:
		count = 3 << d_y_aspect_shift;

		for ( ; count ; count--, pz += d_zwidth, pdest += screenwidth)
		{
			if (pz[0] <= izi)
			{
//				pz[0] = izi;
				pdest[0] = pparticle->color;
			}

			if (pz[1] <= izi)
			{
//				pz[1] = izi;
				pdest[1] = pparticle->color;
			}

			if (pz[2] <= izi)
			{
//				pz[2] = izi;
				pdest[2] = pparticle->color;
			}
		}
		break;

	case 4:
		count = 4 << d_y_aspect_shift;

		for ( ; count ; count--, pz += d_zwidth, pdest += screenwidth)
		{
			if (pz[0] <= izi)
			{
//				pz[0] = izi;
				pdest[0] = pparticle->color;
			}

			if (pz[1] <= izi)
			{
//				pz[1] = izi;
				pdest[1] = pparticle->color;
			}

			if (pz[2] <= izi)
			{
//				pz[2] = izi;
				pdest[2] = pparticle->color;
			}

			if (pz[3] <= izi)
			{
//				pz[3] = izi;
				pdest[3] = pparticle->color;
			}
		}
		break;

	default:
		count = pix << d_y_aspect_shift;

		for ( ; count ; count--, pz += d_zwidth, pdest += screenwidth)
		{
			for (i=0 ; i<pix ; i++)
			{
				if (pz[i] <= izi)
				{
//					pz[i] = izi;
					pdest[i] = pparticle->color;
				}
			}
		}
		break;
	}
}

#endif	// !id386

void D_DrawParticle16 (particle_t *pparticle)
{
	vec3_t	local, transformed;
	float	zi;
	unsigned short	*pdest;
	int a;
	short	*pz;
	int		i, izi, pix, count, u, v;

	if (pparticle->alpha <= 0.2)
		return;

// transform point
	VectorSubtract (pparticle->org, r_origin, local);

	transformed[0] = DotProduct(local, r_pright);
	transformed[1] = DotProduct(local, r_pup);
	transformed[2] = DotProduct(local, r_ppn);		

	if (transformed[2] < PARTICLE_Z_CLIP)
		return;

// project the point
// FIXME: preadjust xcenter and ycenter
	zi = 1.0 / transformed[2];
	u = (int)(xcenter + zi * transformed[0] + 0.5);
	v = (int)(ycenter - zi * transformed[1] + 0.5);

	if ((v > d_vrectbottom_particle) || 
		(u > d_vrectright_particle) ||
		(v < d_vrecty) ||
		(u < d_vrectx))
	{
		return;
	}

	pz = d_pzbuffer + (d_zwidth * v) + u;	
	izi = (int)(zi * 0x8000);

	pix = ((int)(izi*pparticle->scale)) >> d_pix_shift;	

	if (pix < d_pix_min)
		pix = d_pix_min;
	else if (pix > d_pix_max)
		pix = d_pix_max;

	u -= pix/2;
	v -= pix/2;
	if (u < 0) u = 0;
	if (v < 0) v = 0;
	pdest = (unsigned short	*)d_viewbuffer + ((d_scantable[v] + u));

	a = 255*pparticle->alpha;

	switch (pix)
	{
	default:
		count = pix << d_y_aspect_shift;

		for ( ; count ; count--, pz += d_zwidth, pdest += screenwidth)
		{
			for (i=0 ; i<pix ; i++)
			{
				if (pz[i] <= izi)
				{
//					pz[i] = izi;
					pdest[i] = d_8to16table[(int)pparticle->color];
				}
			}
		}
		break;
	}
}

void D_DrawParticle32 (particle_t *pparticle)
{
	vec3_t	local, transformed;
	float	zi;
	qbyte	*pdest;
	qbyte	*pal;
	int a;
	short	*pz;
	int		i, izi, pix, count, u, v;

	if (pparticle->alpha <= 0.0)
		return;

// transform point
	VectorSubtract (pparticle->org, r_origin, local);

	transformed[0] = DotProduct(local, r_pright);
	transformed[1] = DotProduct(local, r_pup);
	transformed[2] = DotProduct(local, r_ppn);		

	if (transformed[2] < PARTICLE_Z_CLIP)
		return;

// project the point
// FIXME: preadjust xcenter and ycenter
	zi = 1.0 / transformed[2];
	u = (int)(xcenter + zi * transformed[0] + 0.5);
	v = (int)(ycenter - zi * transformed[1] + 0.5);

	if ((v > d_vrectbottom_particle) || 
		(u > d_vrectright_particle) ||
		(v < d_vrecty) ||
		(u < d_vrectx))
	{
		return;
	}

	pz = d_pzbuffer + (d_zwidth * v) + u;	
	izi = (int)(zi * 0x8000);

	pix = ((int)(izi*pparticle->scale)) >> d_pix_shift;	

	if (pix < d_pix_min)
		pix = d_pix_min;
	else if (pix > d_pix_max)
		pix = d_pix_max;

	u -= pix/2;
	v -= pix/2;
	if (u < 0) u = 0;
	if (v < 0) v = 0;
	pdest = d_viewbuffer + ((d_scantable[v] + u)<<2);
	pal = (qbyte *)&d_8to32table[(int)pparticle->color];

	a = 255*pparticle->alpha;

	switch (pix)
	{
	default:
		count = pix << d_y_aspect_shift;

		for ( ; count ; count--, pz += d_zwidth, pdest += screenwidth<<2)
		{
			for (i=0 ; i<pix ; i++)
			{
				if (pz[i] <= izi)
				{
//					pz[i] = izi;
					pdest[(i<<2)+0] = (pdest[(i<<2)+0]*(255-a) + pal[0]*a) / 255;
					pdest[(i<<2)+1] = (pdest[(i<<2)+1]*(255-a) + pal[1]*a) / 255;
					pdest[(i<<2)+2] = (pdest[(i<<2)+2]*(255-a) + pal[2]*a) / 255;
				}
			}
		}
		break;
	}
}

#define draw(x, y) x=Trans(x,(int)y)
#define rdraw(x, y) x=Trans((int)y,x)
void D_DrawParticleReverseTrans (particle_t *pparticle)
{
	vec3_t	local, transformed;
	float	zi;
	qbyte	*pdest;
	short	*pz;
	int		i, izi, pix, count, u, v;

// transform point
	VectorSubtract (pparticle->org, r_origin, local);

	transformed[0] = DotProduct(local, r_pright);
	transformed[1] = DotProduct(local, r_pup);
	transformed[2] = DotProduct(local, r_ppn);		

	if (transformed[2] < PARTICLE_Z_CLIP)
		return;

// project the point
// FIXME: preadjust xcenter and ycenter
	zi = 1.0 / transformed[2];
	u = (int)(xcenter + zi * transformed[0] + 0.5);
	v = (int)(ycenter - zi * transformed[1] + 0.5);

	if ((v > d_vrectbottom_particle) || 
		(u > d_vrectright_particle) ||
		(v < d_vrecty) ||
		(u < d_vrectx))
	{
		return;
	}

	pz = d_pzbuffer + (d_zwidth * v) + u;	
	izi = (int)(zi * 0x8000);

	pix = ((int)(izi*pparticle->scale)) >> d_pix_shift;	

	if (pix < d_pix_min)
		pix = d_pix_min;
	else if (pix > d_pix_max)
		pix = d_pix_max;

	u -= pix/2;
	v -= pix/2;
	if (u < 0) u = 0;
	if (v < 0) v = 0;
	pdest = d_viewbuffer + d_scantable[v] + u;

	switch (pix)
	{
	case 1:
		count = 1 << d_y_aspect_shift;

		for ( ; count ; count--, pz += d_zwidth, pdest += screenwidth)
		{
			if (pz[0] <= izi)
			{
//				pz[0] = izi;
				rdraw(pdest[0], pparticle->color);
			}
		}
		break;

	case 2:
		count = 2 << d_y_aspect_shift;

		for ( ; count ; count--, pz += d_zwidth, pdest += screenwidth)
		{
			if (pz[0] <= izi)
			{
//				pz[0] = izi;
				rdraw(pdest[0], pparticle->color);
			}

			if (pz[1] <= izi)
			{
//				pz[1] = izi;
				rdraw(pdest[1], pparticle->color);
			}
		}
		break;

	case 3:
		count = 3 << d_y_aspect_shift;

		for ( ; count ; count--, pz += d_zwidth, pdest += screenwidth)
		{
			if (pz[0] <= izi)
			{
//				pz[0] = izi;
				rdraw(pdest[0], pparticle->color);
			}

			if (pz[1] <= izi)
			{
//				pz[1] = izi;
				rdraw(pdest[1], pparticle->color);
			}

			if (pz[2] <= izi)
			{
//				pz[2] = izi;
				rdraw(pdest[2], pparticle->color);
			}
		}
		break;

	case 4:
		count = 4 << d_y_aspect_shift;

		for ( ; count ; count--, pz += d_zwidth, pdest += screenwidth)
		{
			if (pz[0] <= izi)
			{
//				pz[0] = izi;
				rdraw(pdest[0], pparticle->color);
			}

			if (pz[1] <= izi)
			{
//				pz[1] = izi;
				rdraw(pdest[1], pparticle->color);
			}

			if (pz[2] <= izi)
			{
//				pz[2] = izi;
				rdraw(pdest[2], pparticle->color);
			}

			if (pz[3] <= izi)
			{
//				pz[3] = izi;
				rdraw(pdest[3], pparticle->color);
			}
		}
		break;

	default:
		count = pix << d_y_aspect_shift;

		for ( ; count ; count--, pz += d_zwidth, pdest += screenwidth)
		{
			for (i=0 ; i<pix ; i++)
			{
				if (pz[i] <= izi)
				{
//					pz[i] = izi;
					rdraw(pdest[i], pparticle->color);
				}
			}
		}
		break;
	}
}

void D_DrawParticleTrans (particle_t *pparticle)
{
	vec3_t	local, transformed;
	float	zi;
	qbyte	*pdest;
	short	*pz;
	int		i, izi, pix, count, u, v;

	if (r_pixbytes == 4)
	{
		D_DrawParticle32(pparticle);
		return;
	}
	if (r_pixbytes == 2)
	{
		D_DrawParticle16(pparticle);
		return;
	}

	Set_TransLevelF(pparticle->alpha);

	if (t_state & TT_ZERO)
		return;
	
	if (t_state & TT_ONE)
	{
		D_DrawParticle(pparticle);
		return;
	}

	if (t_state & TT_REVERSE)
	{
		D_DrawParticleReverseTrans(pparticle);
		return;
	}

// transform point
	VectorSubtract (pparticle->org, r_origin, local);

	transformed[0] = DotProduct(local, r_pright);
	transformed[1] = DotProduct(local, r_pup);
	transformed[2] = DotProduct(local, r_ppn);		

	if (transformed[2] < PARTICLE_Z_CLIP)
		return;

// project the point
// FIXME: preadjust xcenter and ycenter
	zi = 1.0 / transformed[2];
	u = (int)(xcenter + zi * transformed[0] + 0.5);
	v = (int)(ycenter - zi * transformed[1] + 0.5);

	if ((v > d_vrectbottom_particle) || 
		(u > d_vrectright_particle) ||
		(v < d_vrecty) ||
		(u < d_vrectx))
	{
		return;
	}

	pz = d_pzbuffer + (d_zwidth * v) + u;	
	izi = (int)(zi * 0x8000);

	pix = ((int)(izi*pparticle->scale)) >> d_pix_shift;	

	if (pix < d_pix_min)
		pix = d_pix_min;
	else if (pix > d_pix_max)
		pix = d_pix_max;

	u -= pix/2;
	v -= pix/2;
	if (u < 0) u = 0;
	if (v < 0) v = 0;
	pdest = d_viewbuffer + d_scantable[v] + u;

	switch (pix)
	{
	case 1:
		count = 1 << d_y_aspect_shift;

		for ( ; count ; count--, pz += d_zwidth, pdest += screenwidth)
		{
			if (pz[0] <= izi)
			{
//				pz[0] = izi;
				draw(pdest[0], pparticle->color);
			}
		}
		break;

	case 2:
		count = 2 << d_y_aspect_shift;

		for ( ; count ; count--, pz += d_zwidth, pdest += screenwidth)
		{
			if (pz[0] <= izi)
			{
//				pz[0] = izi;
				draw(pdest[0], pparticle->color);
			}

			if (pz[1] <= izi)
			{
//				pz[1] = izi;
				draw(pdest[1], pparticle->color);
			}
		}
		break;

	case 3:
		count = 3 << d_y_aspect_shift;

		for ( ; count ; count--, pz += d_zwidth, pdest += screenwidth)
		{
			if (pz[0] <= izi)
			{
//				pz[0] = izi;
				draw(pdest[0], pparticle->color);
			}

			if (pz[1] <= izi)
			{
//				pz[1] = izi;
				draw(pdest[1], pparticle->color);
			}

			if (pz[2] <= izi)
			{
//				pz[2] = izi;
				draw(pdest[2], pparticle->color);
			}
		}
		break;

	case 4:
		count = 4 << d_y_aspect_shift;

		for ( ; count ; count--, pz += d_zwidth, pdest += screenwidth)
		{
			if (pz[0] <= izi)
			{
//				pz[0] = izi;
				draw(pdest[0], pparticle->color);
			}

			if (pz[1] <= izi)
			{
//				pz[1] = izi;
				draw(pdest[1], pparticle->color);
			}

			if (pz[2] <= izi)
			{
//				pz[2] = izi;
				draw(pdest[2], pparticle->color);
			}

			if (pz[3] <= izi)
			{
//				pz[3] = izi;
				draw(pdest[3], pparticle->color);
			}
		}
		break;

	default:
		count = pix << d_y_aspect_shift;

		for ( ; count ; count--, pz += d_zwidth, pdest += screenwidth)
		{
			for (i=0 ; i<pix ; i++)
			{
				if (pz[i] <= izi)
				{
//					pz[i] = izi;
					draw(pdest[i], pparticle->color);
				}
			}
		}
		break;
	}
}

void D_2dPos(vec3_t pos, int *u, int *v, int *z)
{
	float	zi;
	vec3_t	local, transformed;

	// transform point
	VectorSubtract (pos, r_origin, local);

	transformed[2] = DotProduct(local, r_ppn);		

	if (transformed[2] < PARTICLE_Z_CLIP)	//near clip
	{
		*u = -1;	//send it off the side intentionally.
		return;
	}

	transformed[0] = DotProduct(local, r_pright);
	transformed[1] = DotProduct(local, r_pup);

// project the point
	zi = 1.0 / transformed[2];
	*u = (int)(xcenter + zi * transformed[0] + 0.5);
	*v = (int)(ycenter - zi * transformed[1] + 0.5);

	*z = (int)(zi * 0x8000);
}
vec_t VI2Length(int x, int y)
{
	float	length;
	length = (float)x*x + (float)y*y;	
	length = sqrt (length);
	return length;
}
void D_DrawSparkTrans32 (particle_t *pparticle)	//draw a line in 3d space
{
	/*
	Finds 2d coords for the points, then draws a line between them with an appropriate alpha
	*/
	vec3_t delta;
	unsigned char *pdest;
	unsigned char *pal;
	short	*pz;
	int		count, u1, v1, z1, a1, a, ia;
	int u2, v2, z2;
	float speed;

	int du, dv, dz, da;

	if (pparticle->alpha <= 0.0)
		return;

	speed = Length(pparticle->vel);	
	if ((speed) < 1)
	{
		D_2dPos(pparticle->org, &u1, &v1, &z1);
		D_2dPos(pparticle->org, &u2, &v2, &z2);
	}
	else
	{	//causes flickers with lower vels (due to bouncing in physics)
		if (speed < 50)
			speed *= 50/speed;
		VectorMA(pparticle->org, 5/(speed), pparticle->vel, delta);
		D_2dPos(delta, &u1, &v1, &z1);
		VectorMA(pparticle->org, -5/(speed), pparticle->vel, delta);
		D_2dPos(delta, &u2, &v2, &z2);
	}

	if ((v1 > d_vrectbottom_particle) || 
		(u1 > d_vrectright_particle) ||
		(v1 < d_vrecty) ||
		(u1 < d_vrectx))
	{
		return;
	}

	if ((v2 > d_vrectbottom_particle) || 
		(u2 > d_vrectright_particle) ||
		(v2 < d_vrecty) ||
		(u2 < d_vrectx))
	{
		return;
	}	
pal = (qbyte *)(d_8to32table + (int)pparticle->color);
	a1 = 255 * pparticle->alpha;

	du = u2 - u1;
	dv = v2 - v1;
	dz = z2 - z1;
	da = 0 - a1;

	if (!du && !dv)
		count = 1;
	else
	{
		count = VI2Length(du, dv);
		if (!count)
			count = 1;
	}

	du *= 256*256;
	dv *= 256*256;
	dz *= 256*256;
	da *= 256*256;
	u1 = u1<<16;
	v1 = v1<<16;
	z1 = z1<<16;
	a1 = a1<<16;
		{
			du /= count;
			dv /= count;
			dz /= count;
			da /= count;
		}
	do
	{		
		pz = d_pzbuffer + (d_zwidth * (v1>>16)) + (u1>>16);		

		if (*pz <= z1>>16)
		{
//			*pz = z1>>16;

			a = a1>>16;
			ia = 255-a;
			pdest = (qbyte *)((unsigned int *)d_viewbuffer + ((d_scantable[v1>>16] + (u1>>16))));
			pdest[0] = (pdest[0]*((ia)) + pal[0]*(a))/255;
			pdest[1] = (pdest[1]*((ia)) + pal[1]*(a))/255;
			pdest[2] = (pdest[2]*((ia)) + pal[2]*(a))/255;
		}

		u1 += du;
		v1 += dv;
		z1 += dz;
		a1 += da;
	} while (count--);
}

void D_DrawSparkTrans16 (particle_t *pparticle)	//draw a line in 3d space, 8bpp
{
	vec3_t delta;
	unsigned short	*pdest;
	short	*pz;
	int		count, u1, v1, z1;
	int u2, v2, z2;
	float speed;

	int du, dv, dz;

	if (pparticle->alpha <= 0.0)
		return;

	speed = Length(pparticle->vel);	
	if ((speed) < 1)
	{
		D_2dPos(pparticle->org, &u1, &v1, &z1);
		D_2dPos(pparticle->org, &u2, &v2, &z2);
	}
	else
	{	//causes flickers with lower vels (due to bouncing in physics)
		if (speed < 50)
			speed *= 50/speed;
		VectorMA(pparticle->org, 2.5/(speed), pparticle->vel, delta);
		D_2dPos(delta, &u1, &v1, &z1);
		VectorMA(pparticle->org, -2.5/(speed), pparticle->vel, delta);
		D_2dPos(delta, &u2, &v2, &z2);
	}

	if ((v1 > d_vrectbottom_particle) || 
		(u1 > d_vrectright_particle) ||
		(v1 < d_vrecty) ||
		(u1 < d_vrectx))
	{
		return;
	}

	if ((v2 > d_vrectbottom_particle) || 
		(u2 > d_vrectright_particle) ||
		(v2 < d_vrecty) ||
		(u2 < d_vrectx))
	{
		return;
	}	

	du = u2 - u1;
	dv = v2 - v1;
	dz = z2 - z1;

	if (!du && !dv)
		count = 1;
	else
	{
		count = VI2Length(du, dv);
		if (!count)
			count = 1;
	}

	du *= 256*256;
	dv *= 256*256;
	dz *= 256*256;
	u1 = u1<<16;
	v1 = v1<<16;
	z1 = z1<<16;
		{
			du /= count;
			dv /= count;
			dz /= count;
		}
	do
	{		
		pz = d_pzbuffer + (d_zwidth * (v1>>16)) + (u1>>16);

		if (*pz <= z1>>16)
		{
//			*pz = z1>>16;

			pdest = (unsigned short*)d_viewbuffer + d_scantable[v1>>16] + (u1>>16);
			*pdest = d_8to16table[(int)pparticle->color];
		}

		u1 += du;
		v1 += dv;
		z1 += dz;
	} while (count--);
}

void D_DrawSparkTrans (particle_t *pparticle)	//draw a line in 3d space, 8bpp
{
	vec3_t delta;
	qbyte	*pdest;
	short	*pz;
	int		count, u1, v1, z1;
	int u2, v2, z2;
	float speed;

	int du, dv, dz;
/*
	D_DrawParticleTrans(pparticle);
	return;
*/
	if (r_pixbytes == 4)
	{
		D_DrawSparkTrans32(pparticle);
		return;
	}
	if (r_pixbytes == 2)
	{
		D_DrawSparkTrans16(pparticle);
		return;
	}

	Set_TransLevelF(pparticle->alpha);

	if (t_state & TT_ZERO)
		return;

	speed = Length(pparticle->vel);	
	if ((speed) < 1)
	{
		D_2dPos(pparticle->org, &u1, &v1, &z1);
		D_2dPos(pparticle->org, &u2, &v2, &z2);
	}
	else
	{	//causes flickers with lower vels (due to bouncing in physics)
		if (speed < 50)
			speed *= 50/speed;
		VectorMA(pparticle->org, 2.5/(speed), pparticle->vel, delta);
		D_2dPos(delta, &u1, &v1, &z1);
		VectorMA(pparticle->org, -2.5/(speed), pparticle->vel, delta);
		D_2dPos(delta, &u2, &v2, &z2);
	}

	if ((v1 > d_vrectbottom_particle) || 
		(u1 > d_vrectright_particle) ||
		(v1 < d_vrecty) ||
		(u1 < d_vrectx))
	{
		return;
	}

	if ((v2 > d_vrectbottom_particle) || 
		(u2 > d_vrectright_particle) ||
		(v2 < d_vrecty) ||
		(u2 < d_vrectx))
	{
		return;
	}	

	du = u2 - u1;
	dv = v2 - v1;
	dz = z2 - z1;

	if (!du && !dv)
		count = 1;
	else
	{
		count = VI2Length(du, dv);
		if (!count)
			count = 1;
	}

	du *= 256*256;
	dv *= 256*256;
	dz *= 256*256;
	u1 = u1<<16;
	v1 = v1<<16;
	z1 = z1<<16;
		{
			du /= count;
			dv /= count;
			dz /= count;
		}

	if (t_state & TT_ONE)
	{
		do
		{		
			pz = d_pzbuffer + (d_zwidth * (v1>>16)) + (u1>>16);

			if (*pz <= z1>>16)
			{
				pdest = d_viewbuffer + d_scantable[v1>>16] + (u1>>16);
				*pdest = pparticle->color;
			}

			u1 += du;
			v1 += dv;
			z1 += dz;
		} while (count--);
	}
	else if (t_state & TT_REVERSE)
	{
		do
		{		
			pz = d_pzbuffer + (d_zwidth * (v1>>16)) + (u1>>16);

			if (*pz <= z1>>16)
			{
				pdest = d_viewbuffer + d_scantable[v1>>16] + (u1>>16);
				rdraw(*pdest, pparticle->color);
			}

			u1 += du;
			v1 += dv;
			z1 += dz;
		} while (count--);
	}
	else
	{
		do
		{		
			pz = d_pzbuffer + (d_zwidth * (v1>>16)) + (u1>>16);

			if (*pz <= z1>>16)
			{
//				*pz = z1>>16;
				pdest = d_viewbuffer + d_scantable[v1>>16] + (u1>>16);
				draw(*pdest, pparticle->color);
			}

			u1 += du;
			v1 += dv;
			z1 += dz;
		} while (count--);
	}
}
