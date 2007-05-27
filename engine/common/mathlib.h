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
// mathlib.h

typedef float vec_t;
typedef vec_t vec2_t[2];
typedef vec_t vec3_t[3];
typedef vec_t vec4_t[4];
typedef vec_t vec5_t[5];

typedef qbyte byte_vec4_t[4];

typedef	int	fixed4_t;
typedef	int	fixed8_t;
typedef	int	fixed16_t;

#ifndef M_PI
#define M_PI		3.14159265358979323846	// matches value in gcc v2 math.h
#endif

struct mplane_s;

extern vec3_t vec3_origin;
extern	int nanmask;

#define bound(min,num,max) ((num) >= (min) ? ((num) < (max) ? (num) : (max)) : (min))

#define	IS_NAN(x) (((*(int *)&x)&nanmask)==nanmask)

#define DotProduct(x,y) (x[0]*y[0]+x[1]*y[1]+x[2]*y[2])
#define VectorSubtract(a,b,c) {(c)[0]=(a)[0]-(b)[0];(c)[1]=(a)[1]-(b)[1];(c)[2]=(a)[2]-(b)[2];}
#define VectorAdd(a,b,c) {(c)[0]=(a)[0]+(b)[0];(c)[1]=(a)[1]+(b)[1];(c)[2]=(a)[2]+(b)[2];}
#define VectorCopy(a,b) {(b)[0]=(a)[0];(b)[1]=(a)[1];(b)[2]=(a)[2];}
#define VectorClear(a)			((a)[0]=(a)[1]=(a)[2]=0)
#define VectorNegate(a,b)		((b)[0]=-(a)[0],(b)[1]=-(a)[1],(b)[2]=-(a)[2])
#define VectorLength(a)		Length(a)

void VectorMA (vec3_t veca, float scale, vec3_t vecb, vec3_t vecc);

vec_t _DotProduct (vec3_t v1, vec3_t v2);
void _VectorSubtract (vec3_t veca, vec3_t vecb, vec3_t out);
void _VectorAdd (vec3_t veca, vec3_t vecb, vec3_t out);
void _VectorCopy (vec3_t in, vec3_t out);

int VectorCompare (vec3_t v1, vec3_t v2);
vec_t Length (vec3_t v);
void CrossProduct (vec3_t v1, vec3_t v2, vec3_t cross);
float VectorNormalize (vec3_t v);		// returns vector length
vec_t VectorNormalize2 (vec3_t v, vec3_t out);
void VectorInverse (vec3_t v);
void VectorScale (vec3_t in, vec_t scale, vec3_t out);
int Q_log2(int val);

float ColorNormalize (vec3_t in, vec3_t out);
void MakeNormalVectors (vec3_t forward, vec3_t right, vec3_t up);

typedef float matrix3x4[3][4];
typedef float matrix3x3[3][3];

void R_ConcatRotations (float in1[3][3], float in2[3][3], float out[3][3]);
void R_ConcatRotationsPad (float in1[3][4], float in2[3][4], float out[3][4]);
void R_ConcatTransforms (matrix3x4 in1, matrix3x4 in2, matrix3x4 out);
void VectorTransform(const vec3_t in1, matrix3x4 in2, vec3_t out);

void FloorDivMod (double numer, double denom, int *quotient,
		int *rem);
fixed16_t Invert24To16(fixed16_t val);
fixed16_t Mul16_30(fixed16_t multiplier, fixed16_t multiplicand);
int GreatestCommonDivisor (int i1, int i2);

void VectorVectors(vec3_t forward, vec3_t right, vec3_t up);
void AngleVectors (vec3_t angles, vec3_t forward, vec3_t right, vec3_t up);
int VARGS BoxOnPlaneSide (vec3_t emins, vec3_t emaxs, struct mplane_s *plane);
float	anglemod(float a);

void RotatePointAroundVector( vec3_t dst, const vec3_t dir, const vec3_t point, float degrees );


#define BOX_ON_PLANE_SIDE(emins, emaxs, p)	\
	(((p)->type < 3)?						\
	(										\
		((p)->dist <= (emins)[(p)->type])?	\
			1								\
		:									\
		(									\
			((p)->dist >= (emaxs)[(p)->type])?\
				2							\
			:								\
				3							\
		)									\
	)										\
	:										\
		BoxOnPlaneSide( (emins), (emaxs), (p)))

typedef struct {
	float m[4][4];
} matrix4x4_t;

//used for crosshair stuff.
void Matrix4_Project (vec3_t in, vec3_t out, vec3_t viewangles, vec3_t vieworg, float wdivh, float fovy);
void Matrix4_UnProject(vec3_t in, vec3_t out, vec3_t viewangles, vec3_t vieworg, float wdivh, float fovy);
void Matrix3_Multiply (vec3_t *in1, vec3_t *in2, vec3_t *out);
void Matrix4_Multiply(float *a, float *b, float *out);
void Matrix4_Transform3(float *matrix, float *vector, float *product);
void Matrix4_Transform4(float *matrix, float *vector, float *product);
void Matrix4_Invert_Simple (matrix4x4_t *out, const matrix4x4_t *in1);
void Matrix4_ModelViewMatrix(float *modelview, vec3_t viewangles, vec3_t vieworg);
void Matrix4_Projection2(float *proj, float fovx, float fovy, float neard);
void Matrix4_Orthographic(float *proj, float xmin, float xmax, float ymax, float ymin, float znear, float zfar);
void Matrix4_ModelViewMatrixFromAxis(float *modelview, vec3_t pn, vec3_t right, vec3_t up, vec3_t vieworg);


void AddPointToBounds (vec3_t v, vec3_t mins, vec3_t maxs);
void ClearBounds (vec3_t mins, vec3_t maxs);
