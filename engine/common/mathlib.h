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

/*16-byte aligned vectors, for auto-vectorising, should propogate to structs
sse and altivec can unroll loops using aligned reads, which should be faster... 4 at once.
*/
#ifdef _MSC_VER
typedef __declspec(align(16)) vec3_t avec3_t;
typedef __declspec(align(16)) vec4_t avec4_t;
typedef __declspec(align(4)) qbyte byte_vec4_t[4];
#elif __GNUC__ >= 3
typedef __attribute__((aligned(16))) vec3_t avec3_t;
typedef __attribute__((aligned(16))) vec4_t avec4_t;
typedef __attribute__((aligned(4))) qbyte byte_vec4_t[4];
#else
typedef vec3_t avec3_t;
typedef vec4_t avec4_t;
typedef qbyte byte_vec4_t[4];
#endif

#define vecV_t avec4_t

typedef	int	fixed4_t;
typedef	int	fixed8_t;
typedef	int	fixed16_t;

#ifndef M_PI
#define M_PI		3.14159265358979323846	// matches value in gcc v2 math.h
#endif

struct mplane_s;

extern vec3_t vec3_origin;

#define bound(min,num,max) ((num) >= (min) ? ((num) < (max) ? (num) : (max)) : (min))

#define nanmask (255<<23)
#define	IS_NAN(x) (((*(int *)&x)&nanmask)==nanmask)

#define FloatInterpolate(a, bness, b, c) (c) = (a)*(1-bness) + (b)*bness

#define DotProduct(x,y) ((x)[0]*(y)[0]+(x)[1]*(y)[1]+(x)[2]*(y)[2])
#define VectorSubtract(a,b,c) do{(c)[0]=(a)[0]-(b)[0];(c)[1]=(a)[1]-(b)[1];(c)[2]=(a)[2]-(b)[2];}while(0)
#define VectorAdd(a,b,c) do{(c)[0]=(a)[0]+(b)[0];(c)[1]=(a)[1]+(b)[1];(c)[2]=(a)[2]+(b)[2];}while(0)
#define VectorCopy(a,b) do{(b)[0]=(a)[0];(b)[1]=(a)[1];(b)[2]=(a)[2];}while(0)
#define VectorScale(a,s,b) do{(b)[0]=(s)*(a)[0];(b)[1]=(s)*(a)[1];(b)[2]=(s)*(a)[2];}while(0)
#define VectorClear(a)			((a)[0]=(a)[1]=(a)[2]=0)
#define VectorSet(r,x,y,z) {(r)[0] = x; (r)[1] = y;(r)[2] = z;}
#define VectorNegate(a,b)		((b)[0]=-(a)[0],(b)[1]=-(a)[1],(b)[2]=-(a)[2])
#define VectorLength(a)		Length(a)
#define VectorMA(a,s,b,c) do{(c)[0] = (a)[0] + (s)*(b)[0];(c)[1] = (a)[1] + (s)*(b)[1];(c)[2] = (a)[2] + (s)*(b)[2];}while(0)
#define VectorEquals(a,b) ((a)[0] == (b)[0] && (a)[1] == (b)[1] && (a)[2] == (b)[2])
#define VectorAvg(a,b,c)		((c)[0]=((a)[0]+(b)[0])*0.5f,(c)[1]=((a)[1]+(b)[1])*0.5f, (c)[2]=((a)[2]+(b)[2])*0.5f)
#define VectorInterpolate(a, bness, b, c) FloatInterpolate((a)[0], bness, (b)[0], (c)[0]),FloatInterpolate((a)[1], bness, (b)[1], (c)[1]),FloatInterpolate((a)[2], bness, (b)[2], (c)[2])
#define Vector2Copy(a,b) {(b)[0]=(a)[0];(b)[1]=(a)[1];}
#define Vector2Set(r,x,y) {(r)[0] = x; (r)[1] = y;}

#define Vector4Copy(a,b) do{(b)[0]=(a)[0];(b)[1]=(a)[1];(b)[2]=(a)[2];(b)[3]=(a)[3];}while(0)
#define Vector4Scale(in,scale,out)		((out)[0]=(in)[0]*scale,(out)[1]=(in)[1]*scale,(out)[2]=(in)[2]*scale,(out)[3]=(in)[3]*scale)
#define Vector4Add(a,b,c)		((c)[0]=(((a[0])+(b[0]))),(c)[1]=(((a[1])+(b[1]))),(c)[2]=(((a[2])+(b[2]))),(c)[3]=(((a[3])+(b[3]))))

typedef float matrix3x4[3][4];
typedef float matrix3x3[3][3];


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



vec_t		_DotProduct (vec3_t v1, vec3_t v2);
void		_VectorAdd (vec3_t veca, vec3_t vecb, vec3_t out);
void		_VectorCopy (vec3_t in, vec3_t out);
void		_VectorSubtract (vec3_t veca, vec3_t vecb, vec3_t out);
void		AddPointToBounds (vec3_t v, vec3_t mins, vec3_t maxs);
float		anglemod (float a);
void		AngleVectors (const vec3_t angles, vec3_t forward, vec3_t right, vec3_t up);
void		VectorAngles (float *forward, float *up, float *angles);	//up may be NULL
void VARGS	BOPS_Error (void);
int VARGS	BoxOnPlaneSide (vec3_t emins, vec3_t emaxs, struct mplane_s *plane);
void		ClearBounds (vec3_t mins, vec3_t maxs);
float		ColorNormalize (vec3_t in, vec3_t out);
void		CrossProduct (const vec3_t v1, const vec3_t v2, vec3_t cross);
void		FloorDivMod (double numer, double denom, int *quotient, int *rem);
int			GreatestCommonDivisor (int i1, int i2);
fixed16_t	Invert24To16 (fixed16_t val);
vec_t		Length (vec3_t v);
void		MakeNormalVectors (vec3_t forward, vec3_t right, vec3_t up);
float		Q_rsqrt(float number);

//used for crosshair stuff.
void		Matrix3_Multiply (vec3_t *in1, vec3_t *in2, vec3_t *out);
void		Matrix4_Identity(float *outm);
qboolean	Matrix4_Invert(const float *m, float *out);
void		Matrix4Q_Invert_Simple (const float *in1, float *out);
void		Matrix4_CreateTranslate (float *out, float x, float y, float z);
void		Matrix4Q_CreateTranslate (float *out, float x, float y, float z);
void		Matrix4_ModelMatrixFromAxis (float *modelview, const vec3_t pn, const vec3_t right, const vec3_t up, const vec3_t vieworg);
void		Matrix4_ModelViewMatrix (float *modelview, const vec3_t viewangles, const vec3_t vieworg);
void		Matrix4_ModelViewMatrixFromAxis (float *modelview, const vec3_t pn, const vec3_t right, const vec3_t up, const vec3_t vieworg);
void		Matrix4_CreateFromQuakeEntity (float *matrix, float x, float y, float z, float pitch, float yaw, float roll, float scale);
void		Matrix4_Multiply (const float *a, const float *b, float *out);
void		Matrix4_Project (const vec3_t in, vec3_t out, const vec3_t viewangles, const vec3_t vieworg, float fovx, float fovy);
void		Matrix4_Transform3 (const float *matrix, const float *vector, float *product);
void		Matrix4_Transform4 (const float *matrix, const float *vector, float *product);
void		Matrix4_UnProject (const vec3_t in, vec3_t out, const vec3_t viewangles, const vec3_t vieworg, float fovx, float fovy);
void		Matrix4Q_FromVectors(float *out, const float vx[3], const float vy[3], const float vz[3], const float t[3]);
void		Matrix4Q_ToVectors(const float *in, float vx[3], float vy[3], float vz[3], float t[3]);

#define AngleVectorsFLU(a,f,l,u) do{AngleVectors(a,f,l,u);VectorNegate(l,l);}while(0)

//projection matricies of different types... gesh
void		Matrix4_Orthographic (float *proj, float xmin, float xmax, float ymax, float ymin, float znear, float zfar);
void		Matrix4_Projection_Far(float *proj, float fovx, float fovy, float neard, float fard);
void		Matrix4_Projection2 (float *proj, float fovx, float fovy, float neard);
void		Matrix4_Projection_Inf(float *proj, float fovx, float fovy, float neard);

fixed16_t	Mul16_30 (fixed16_t multiplier, fixed16_t multiplicand);
int			Q_log2 (int val);

void		Matrix3x4_InvertTo3x3(float *in, float *result);

fixed16_t	Mul16_30 (fixed16_t multiplier, fixed16_t multiplicand);
int			Q_log2 (int val);
void		R_ConcatRotations (float in1[3][3], float in2[3][3], float out[3][3]);
void		R_ConcatRotationsPad (float in1[3][4], float in2[3][4], float out[3][4]);
void		R_ConcatTransforms (matrix3x4 in1, matrix3x4 in2, matrix3x4 out);
void		RotatePointAroundVector (vec3_t dst, const vec3_t dir, const vec3_t point, float degrees);
int			VectorCompare (const vec3_t v1, const vec3_t v2);
void		VectorInverse (vec3_t v);
void		_VectorMA (const vec3_t veca, const float scale, const vec3_t vecb, vec3_t vecc);
float		VectorNormalize (vec3_t v);		// returns vector length
vec_t		VectorNormalize2 (const vec3_t v, vec3_t out);
void		VectorNormalizeFast(vec3_t v);
void		VectorTransform (const vec3_t in1, const matrix3x4 in2, vec3_t out);
void		VectorVectors (const vec3_t forward, vec3_t right, vec3_t up);
