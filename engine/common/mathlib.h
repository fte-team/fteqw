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
#pragma once

#if _MSC_VER >= 1300
	#define FTE_ALIGN(a) __declspec(align(a))
#elif defined(__clang__)
	#define FTE_ALIGN(a) __attribute__((aligned(a)))
#elif __GNUC__ >= 3
	#define FTE_ALIGN(a) __attribute__((aligned(a)))
#else
	#define FTE_ALIGN(a)
#endif
#define BITOP_RUP1__(x)  (            (x) | (            (x) >>  1))
#define BITOP_RUP2__(x)  (BITOP_RUP1__(x) | (BITOP_RUP1__(x) >>  2))
#define BITOP_RUP4__(x)  (BITOP_RUP2__(x) | (BITOP_RUP2__(x) >>  4))
#define BITOP_RUP8__(x)  (BITOP_RUP4__(x) | (BITOP_RUP4__(x) >>  8))
#define BITOP_RUP16__(x) (BITOP_RUP8__(x) | (BITOP_RUP8__(x) >> 16))
#define BITOP_RUP(x) (BITOP_RUP16__((uint32_t)(x) - 1) + 1)

#define BITOP_LOG2__(x) (((((x) & 0xffff0000) != 0) << 4) \
                        |((((x) & 0xff00ff00) != 0) << 3) \
                        |((((x) & 0xf0f0f0f0) != 0) << 2) \
                        |((((x) & 0xcccccccc) != 0) << 1) \
                        |((((x) & 0xaaaaaaaa) != 0) << 0))

#define BITOP_LOG2(x) BITOP_LOG2__(BITOP_RUP(x))

enum align
{
	FTE_ALIGN_NONE     = (0 << 0),
	FTE_ALIGN_SCALAR   = (1 << 0),
	FTE_ALIGN_VECTOR   = (1 << 1),
	FTE_ALIGN_MATRIX   = (1 << 2),
	FTE_ALIGN_ADAPTIVE = (1 << 3),
};

typedef float vec_t;

#define FTE_VECTOR(N,T,A,prefix,name,suffix) \
typedef T FTE_ALIGN(((BITOP_RUP(N) == N && A != FTE_ALIGN_SCALAR) || A == FTE_ALIGN_VECTOR ? \
alignof(T) * BITOP_RUP(N) : alignof(T))) \
prefix##name##N##suffix[(BITOP_RUP(N) == N && A != FTE_ALIGN_SCALAR) || A == FTE_ALIGN_VECTOR ? BITOP_RUP(N) : N]

/* adaptive aligned vector types */
FTE_VECTOR(2,vec_t,FTE_ALIGN_ADAPTIVE,,vec,_t);
FTE_VECTOR(3,vec_t,FTE_ALIGN_ADAPTIVE,,vec,_t);
FTE_VECTOR(4,vec_t,FTE_ALIGN_ADAPTIVE,,vec,_t);
FTE_VECTOR(5,vec_t,FTE_ALIGN_ADAPTIVE,,vec,_t);
FTE_VECTOR(6,vec_t,FTE_ALIGN_ADAPTIVE,,vec,_t);
FTE_VECTOR(7,vec_t,FTE_ALIGN_ADAPTIVE,,vec,_t);
FTE_VECTOR(8,vec_t,FTE_ALIGN_ADAPTIVE,,vec,_t);

FTE_VECTOR(2,float,FTE_ALIGN_ADAPTIVE,,vec,f_t);
FTE_VECTOR(3,float,FTE_ALIGN_ADAPTIVE,,vec,f_t);
FTE_VECTOR(4,float,FTE_ALIGN_ADAPTIVE,,vec,f_t);
FTE_VECTOR(5,float,FTE_ALIGN_ADAPTIVE,,vec,f_t);
FTE_VECTOR(6,float,FTE_ALIGN_ADAPTIVE,,vec,f_t);
FTE_VECTOR(7,float,FTE_ALIGN_ADAPTIVE,,vec,f_t);
FTE_VECTOR(8,float,FTE_ALIGN_ADAPTIVE,,vec,f_t);

FTE_VECTOR(2,double,FTE_ALIGN_ADAPTIVE,,vec,d_t);
FTE_VECTOR(3,double,FTE_ALIGN_ADAPTIVE,,vec,d_t);
FTE_VECTOR(4,double,FTE_ALIGN_ADAPTIVE,,vec,d_t);
FTE_VECTOR(5,double,FTE_ALIGN_ADAPTIVE,,vec,d_t);
FTE_VECTOR(6,double,FTE_ALIGN_ADAPTIVE,,vec,d_t);
FTE_VECTOR(7,double,FTE_ALIGN_ADAPTIVE,,vec,d_t);
FTE_VECTOR(8,double,FTE_ALIGN_ADAPTIVE,,vec,d_t);

FTE_VECTOR(2,int32_t,FTE_ALIGN_ADAPTIVE,,vec,i_t);
FTE_VECTOR(3,int32_t,FTE_ALIGN_ADAPTIVE,,vec,i_t);
FTE_VECTOR(4,int32_t,FTE_ALIGN_ADAPTIVE,,vec,i_t);
FTE_VECTOR(5,int32_t,FTE_ALIGN_ADAPTIVE,,vec,i_t);
FTE_VECTOR(6,int32_t,FTE_ALIGN_ADAPTIVE,,vec,i_t);
FTE_VECTOR(7,int32_t,FTE_ALIGN_ADAPTIVE,,vec,i_t);
FTE_VECTOR(8,int32_t,FTE_ALIGN_ADAPTIVE,,vec,i_t);

FTE_VECTOR(2,uint32_t,FTE_ALIGN_ADAPTIVE,,vec,u_t);
FTE_VECTOR(3,uint32_t,FTE_ALIGN_ADAPTIVE,,vec,u_t);
FTE_VECTOR(4,uint32_t,FTE_ALIGN_ADAPTIVE,,vec,u_t);
FTE_VECTOR(5,uint32_t,FTE_ALIGN_ADAPTIVE,,vec,u_t);
FTE_VECTOR(6,uint32_t,FTE_ALIGN_ADAPTIVE,,vec,u_t);
FTE_VECTOR(7,uint32_t,FTE_ALIGN_ADAPTIVE,,vec,u_t);
FTE_VECTOR(8,uint32_t,FTE_ALIGN_ADAPTIVE,,vec,u_t);

FTE_VECTOR( 2,uint8_t,FTE_ALIGN_ADAPTIVE,,vec,ub_t);
FTE_VECTOR( 4,uint8_t,FTE_ALIGN_ADAPTIVE,,vec,ub_t);
FTE_VECTOR( 8,uint8_t,FTE_ALIGN_ADAPTIVE,,vec,ub_t);
FTE_VECTOR(16,uint8_t,FTE_ALIGN_ADAPTIVE,,vec,ub_t);
FTE_VECTOR(32,uint8_t,FTE_ALIGN_ADAPTIVE,,vec,ub_t);
FTE_VECTOR(64,uint8_t,FTE_ALIGN_ADAPTIVE,,vec,ub_t);

typedef vec4ub_t byte_vec4_t;
typedef vec4_t        quat_t;

/* vector aligned vector types */
FTE_VECTOR(2,vec_t,FTE_ALIGN_VECTOR,a,vec,_t);
FTE_VECTOR(3,vec_t,FTE_ALIGN_VECTOR,a,vec,_t);
FTE_VECTOR(4,vec_t,FTE_ALIGN_VECTOR,a,vec,_t);
FTE_VECTOR(5,vec_t,FTE_ALIGN_VECTOR,a,vec,_t);
FTE_VECTOR(6,vec_t,FTE_ALIGN_VECTOR,a,vec,_t);
FTE_VECTOR(7,vec_t,FTE_ALIGN_VECTOR,a,vec,_t);
FTE_VECTOR(8,vec_t,FTE_ALIGN_VECTOR,a,vec,_t);

FTE_VECTOR(2,int32_t,FTE_ALIGN_VECTOR,a,vec,i_t);
FTE_VECTOR(3,int32_t,FTE_ALIGN_VECTOR,a,vec,i_t);
FTE_VECTOR(4,int32_t,FTE_ALIGN_VECTOR,a,vec,i_t);
FTE_VECTOR(5,int32_t,FTE_ALIGN_VECTOR,a,vec,i_t);
FTE_VECTOR(6,int32_t,FTE_ALIGN_VECTOR,a,vec,i_t);
FTE_VECTOR(7,int32_t,FTE_ALIGN_VECTOR,a,vec,i_t);
FTE_VECTOR(8,int32_t,FTE_ALIGN_VECTOR,a,vec,i_t);

//VECV_STRIDE is used only as an argument for opengl.
#ifdef FTE_TARGET_WEB
	//emscripten is alergic to explicit strides without packed attributes, at least in emulated code.
	//so we need to keep everything packed. screw sse-friendly packing.
	#define vecV_t vec3_t
	#define VECV_STRIDE 0
#else
	#define vecV_t avec4_t
	#define VECV_STRIDE sizeof(vecV_t)
#endif

typedef vec3_t                                   mat3_t[3];
typedef avec3_t                                  amat3_t[3];
typedef vec3_t                                   mat43_t[4];
typedef avec3_t                                  amat43_t[4];
typedef vec4_t                                   mat34_t[3];
typedef vec4_t FTE_ALIGN(alignof(vec4_t) * 4)    mat4_t[4];
typedef vec8_t FTE_ALIGN(alignof(vec8_t) * 8)    mat8_t[8];

typedef vec2_t FTE_ALIGN(alignof(vec2_t) * 2)   matrix2x2[2];
typedef vec3_t                                  matrix3x3[3];
typedef vec4_t                                  matrix3x4[3];
typedef vec4_t FTE_ALIGN(alignof(vec4_t) * 4)   matrix4x4[4];
typedef vec8_t FTE_ALIGN(alignof(vec8_t) * 8)   matrix8x8[8];

typedef struct {
	vec_t m[2][2];
} matrix2x2_t;
typedef struct {
	vec_t m[3][3];
} matrix3x3_t;
typedef struct {
	vec_t m[3][4];
} matrix3x4_t;
typedef struct {
	vec_t m[4][4];
} matrix4x4_t;
typedef struct {
	vec_t m[8][8];
} matrix8x8_t;

typedef union {
	struct {
		vec3_t v;
		vec_t  s;
	} sv;
	quat_t q;
} quaternion;

typedef struct {
	vec_t r;
	vec_t e;
} dual;

typedef struct {
	quaternion q0;
	quaternion qe;
} dual_quat;

typedef	int	fixed4_t;
typedef	int	fixed8_t;
typedef	int	fixed16_t;

// plane_t structure
typedef struct mplane_s
{
	vec3_t	normal;
	float	dist;
	qbyte	type;			// for texture axis selection and fast side tests
	qbyte	signbits;		// signx + signy<<1 + signz<<1
	qbyte	pad[2];
} mplane_t;

// sphere_t structure
typedef struct sphere_s
{
	vec3_t center;
	vec_t  radius;
} sphere_t;

/*========================================== matrix functions =====================================*/

static mat3_t axisDefault = {{1, 0, 0},{0, 1, 0},{0, 0, 1}};

static void Matrix3_Transpose (mat3_t in, mat3_t out)
{
	out[0][0] = in[0][0];
	out[1][1] = in[1][1];
	out[2][2] = in[2][2];

	out[0][1] = in[1][0];
	out[0][2] = in[2][0];
	out[1][0] = in[0][1];
	out[1][2] = in[2][1];
	out[2][0] = in[0][2];
	out[2][1] = in[1][2];
}
static void Matrix3_Multiply_Vec3 (const mat3_t a, const vec3_t b, vec3_t product)
{
	product[0] = a[0][0]*b[0] + a[0][1]*b[1] + a[0][2]*b[2];
	product[1] = a[1][0]*b[0] + a[1][1]*b[1] + a[1][2]*b[2];
	product[2] = a[2][0]*b[0] + a[2][1]*b[1] + a[2][2]*b[2];
}

static int Matrix3_Compare(const mat3_t in, const mat3_t out)
{
	return memcmp(in, out, sizeof(mat3_t));
}

extern vec3_t vec3_origin;

#ifndef M_PI
#define M_PI		3.14159265358979323846	// matches value in gcc v2 math.h
#endif

#define bound(min,num,max) ((num) >= (min) ? ((num) < (max) ? (num) : (max)) : (min))

#define nanmask (255<<23)
#define	IS_NAN(x) (((*(int *)&x)&nanmask)==nanmask)

#define FloatInterpolate(a, bness, b, c) ((c) = (a) + (b - a)*bness)

#define DotProduct_Double(x,y) ((double)(x)[0]*(double)(y)[0]+(double)(x)[1]*(double)(y)[1]+(double)(x)[2]*(double)(y)[2])	//cast to doubles, to try to replicate x87 precision in 64bit sse builds etc. there'll still be a precision difference though.
#define DotProduct(x,y) ((x)[0]*(y)[0]+(x)[1]*(y)[1]+(x)[2]*(y)[2])
#define DotProduct2(x,y) ((x)[0]*(y)[0]+(x)[1]*(y)[1])
#define DotProduct4(x,y) ((x)[0]*(y)[0]+(x)[1]*(y)[1]+(x)[2]*(y)[2]+(x)[3]*(y)[3])
#define VectorProduct(v1,v2)                                                            \
(avec3_t){ ((vec_t*)v1)[1]*((vec_t*)v2)[2] - ((vec_t*)v1)[2]*((vec_t*)v2)[1],           \
           ((vec_t*)v1)[2]*((vec_t*)v2)[0] - ((vec_t*)v1)[0]*((vec_t*)v2)[2],           \
           ((vec_t*)v1)[0]*((vec_t*)v2)[1] - ((vec_t*)v1)[1]*((vec_t*)v2)[0] }

#define CrossProduct(v1,v2,cross)                                                       \
do {                                                                                    \
((vec_t*)cross)[0] = ((vec_t*)v1)[1]*((vec_t*)v2)[2] - ((vec_t*)v1)[2]*((vec_t*)v2)[1]; \
((vec_t*)cross)[1] = ((vec_t*)v1)[2]*((vec_t*)v2)[0] - ((vec_t*)v1)[0]*((vec_t*)v2)[2]; \
((vec_t*)cross)[2] = ((vec_t*)v1)[0]*((vec_t*)v2)[1] - ((vec_t*)v1)[1]*((vec_t*)v2)[0]; \
} while (0)

#define VectorSubtract(a,b,c) do{(c)[0]=(a)[0]-(b)[0];(c)[1]=(a)[1]-(b)[1];(c)[2]=(a)[2]-(b)[2];}while(0)
#define VectorAdd(a,b,c) do{(c)[0]=(a)[0]+(b)[0];(c)[1]=(a)[1]+(b)[1];(c)[2]=(a)[2]+(b)[2];}while(0)
#define VectorCopy(a,b) do{(b)[0]=(a)[0];(b)[1]=(a)[1];(b)[2]=(a)[2];}while(0)
#define VectorScale(a,s,b) do{(b)[0]=(s)*(a)[0];(b)[1]=(s)*(a)[1];(b)[2]=(s)*(a)[2];}while(0)
#define VectorMul(a,s,b) do{(b)[0]=(s)[0]*(a)[0];(b)[1]=(s)[1]*(a)[1];(b)[2]=(s)[2]*(a)[2];}while(0)
#define VectorClear(a)			((a)[0]=(a)[1]=(a)[2]=0)
#define VectorSet(r,x,y,z) do{(r)[0] = x; (r)[1] = y;(r)[2] = z;}while(0)
#define VectorNegate(a,b)		((b)[0]=-(a)[0],(b)[1]=-(a)[1],(b)[2]=-(a)[2])
#define VectorLength(a)		Length(a)
#define VectorMA(a,s,b,c) do{(c)[0] = (a)[0] + (s)*(b)[0];(c)[1] = (a)[1] + (s)*(b)[1];(c)[2] = (a)[2] + (s)*(b)[2];}while(0)
#define VectorEquals(a,b) ((a)[0] == (b)[0] && (a)[1] == (b)[1] && (a)[2] == (b)[2])
#define VectorAvg(a,b,c)		((c)[0]=((a)[0]+(b)[0])*0.5f,(c)[1]=((a)[1]+(b)[1])*0.5f, (c)[2]=((a)[2]+(b)[2])*0.5f)
#define VectorInterpolate(a, bness, b, c) FloatInterpolate((a)[0], bness, (b)[0], (c)[0]),FloatInterpolate((a)[1], bness, (b)[1], (c)[1]),FloatInterpolate((a)[2], bness, (b)[2], (c)[2])
#define Vector2Clear(a)			((a)[0]=(a)[1]=0)
#define Vector2Copy(a,b) do{(b)[0]=(a)[0];(b)[1]=(a)[1];}while(0)
#define Vector2Set(r,x,y) do{(r)[0] = x; (r)[1] = y;}while(0)
#define Vector2MA(a,s,b,c) do{(c)[0] = (a)[0] + (s)*(b)[0];(c)[1] = (a)[1] + (s)*(b)[1];}while(0)
#define Vector2Interpolate(a, bness, b, c) FloatInterpolate((a)[0], bness, (b)[0], (c)[0]),FloatInterpolate((a)[1], bness, (b)[1], (c)[1])

#define Vector4Copy(a,b) do{(b)[0]=(a)[0];(b)[1]=(a)[1];(b)[2]=(a)[2];(b)[3]=(a)[3];}while(0)
#define Vector4Scale(in,scale,out)		((out)[0]=(in)[0]*scale,(out)[1]=(in)[1]*scale,(out)[2]=(in)[2]*scale,(out)[3]=(in)[3]*scale)
#define Vector4Add(a,b,c)		((c)[0]=(((a[0])+(b[0]))),(c)[1]=(((a[1])+(b[1]))),(c)[2]=(((a[2])+(b[2]))),(c)[3]=(((a[3])+(b[3]))))
#define Vector4Set(r,x,y,z,w) (r)[0] = x, (r)[1] = y, (r)[2] = z, (r)[3]=w
#define Vector4Interpolate(a, bness, b, c) FloatInterpolate((a)[0], bness, (b)[0], (c)[0]),FloatInterpolate((a)[1], bness, (b)[1], (c)[1]),FloatInterpolate((a)[2], bness, (b)[2], (c)[2]),FloatInterpolate((a)[3], bness, (b)[3], (c)[3])
#define Vector4MA(a,s,b,c) do{(c)[0] = (a)[0] + (s)*(b)[0];(c)[1] = (a)[1] + (s)*(b)[1];(c)[2] = (a)[2] + (s)*(b)[2];(c)[3] = (a)[3] + (s)*(b)[3];}while(0)


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

//vec_t		_DotProduct (vec3_t v1, vec3_t v2);
//void		_VectorAdd (vec3_t veca, vec3_t vecb, vec3_t out);
//void		_VectorCopy (vec3_t in, vec3_t out);
//void		_VectorSubtract (vec3_t veca, vec3_t vecb, vec3_t out);
void		AddPointToBounds (const vec3_t v, vec3_t mins, vec3_t maxs);
vec_t		anglemod (vec_t a);
void		QDECL AngleVectors (const vec3_t angles, vec3_t forward, vec3_t right, vec3_t up);
void		QDECL AngleVectorsMesh (const vec3_t angles, vec3_t forward, vec3_t right, vec3_t up);
void		QDECL VectorAngles (const vec_t *forward, const vec_t *up, vec_t *angles, qboolean meshpitch);	//up may be NULL
void VARGS	BOPS_Error (void);
int VARGS	BoxOnPlaneSide (const vec3_t emins, const vec3_t emaxs, const struct mplane_s *plane);
void		ClearBounds (vec3_t mins, vec3_t maxs);
vec_t		ColorNormalize (const vec3_t in, vec3_t out);
void		FloorDivMod (double numer, double denom, int *quotient, int *rem);
int			GreatestCommonDivisor (int i1, int i2);
fixed16_t	Invert24To16 (fixed16_t val);
vec_t		Length (const vec3_t v);
void		MakeNormalVectors (const vec3_t forward, vec3_t right, vec3_t up);
vec_t		Q_rsqrt(vec_t number);

/*
_CM means column major.
_RM means row major
Note that openGL is column-major.
Logical C code uses row-major.
mat3x4 is always row-major (and functions can accept many RM mat4x4)
*/

void		Matrix3_Multiply (vec3_t *in1, vec3_t *in2, vec3_t *out);
void		Matrix4x4_Identity(vec_t *outm);
qboolean	Matrix4_Invert(const vec_t *m, vec_t *out);
void		Matrix3x4_Invert (const vec_t *in1, vec_t *out);
void		QDECL Matrix3x4_Invert_Simple (const vec_t *in1, vec_t *out);
void		Matrix3x4_InvertTo4x4_Simple (const vec_t *in1, vec_t *out);
void		Matrix3x3_RM_Invert_Simple(const vec3_t in[3], vec3_t out[3]);
void		Matrix4x4_RM_CreateTranslate (vec_t *out, vec_t x, vec_t y, vec_t z);
void		Matrix4x4_CM_CreateTranslate (vec_t *out, vec_t x, vec_t y, vec_t z);
void		Matrix4x4_CM_ModelMatrixFromAxis (vec_t *modelview, const vec3_t pn, const vec3_t right, const vec3_t up, const vec3_t vieworg);
void		Matrix4x4_CM_ModelMatrix(vec_t *modelview, vec_t x, vec_t y, vec_t z, vec_t pitch, vec_t yaw, vec_t roll, vec_t scale);
void		Matrix4x4_CM_ModelViewMatrix (vec_t *modelview, const vec3_t viewangles, const vec3_t vieworg);
void		Matrix4x4_CM_ModelViewMatrixFromAxis (vec_t *modelview, const vec3_t pn, const vec3_t right, const vec3_t up, const vec3_t vieworg);
void		Matrix4x4_CM_LightMatrixFromAxis(vec_t *modelview, const vec3_t px, const vec3_t py, const vec3_t pz, const vec3_t vieworg);	//
void		Matrix4_CreateFromQuakeEntity (vec_t *matrix, vec_t x, vec_t y, vec_t z, vec_t pitch, vec_t yaw, vec_t roll, vec_t scale);
void		Matrix4_Multiply (const vec_t *a, const vec_t *b, vec_t *out);
void		Matrix3x4_Multiply(const vec_t *a, const vec_t *b, vec_t *out);
qboolean	Matrix4x4_CM_Project (const vec3_t in, vec3_t out, const vec3_t viewangles, const vec3_t vieworg, vec_t fovx, vec_t fovy);
void		Matrix4x4_CM_Transform3x3(const vec_t *matrix, const vec_t *vector, vec3_t product);
void		Matrix4x4_CM_Transform3 (const vec_t *matrix, const vec_t *vector, vec3_t product);
void		Matrix4x4_CM_Transform4 (const vec_t *matrix, const vec_t *vector, vec4_t product);
void		Matrix4x4_CM_Transform34(const vec_t *matrix, const vec_t *vector, vec4_t product);
void		Matrix4x4_CM_UnProject (const vec3_t in, vec3_t out, const vec3_t viewangles, const vec3_t vieworg, vec_t fovx, vec_t fovy);
void		Matrix3x4_RM_FromAngles(const vec3_t angles, const vec3_t origin, vec_t *out);
void		Matrix3x4_RM_FromVectors(vec_t *out, const vec3_t vx, const vec3_t vy, const vec3_t vz, const vec3_t t);
void		Matrix4x4_RM_FromVectors(vec_t *out, const vec3_t vx, const vec3_t vy, const vec3_t vz, const vec3_t t);
void		Matrix3x4_RM_ToVectors(const vec_t *in, vec3_t vx, vec3_t vy, vec3_t vz, vec3_t t);
void		Matrix3x4_RM_Transform3(const vec_t *matrix, const vec_t *vector, vec_t *product);
void		Matrix3x4_RM_Transform3x3(const vec_t *matrix, const vec_t *vector, vec_t *product);

vec_t		*Matrix4x4_CM_NewRotation(vec_t a, vec_t x, vec_t y, vec_t z);
vec_t		*Matrix4x4_CM_NewTranslation(vec_t x, vec_t y, vec_t z);

void            Bones_To_PosQuat4(int numbones, const vec_t *matrix, short *result);
void            QDECL GenMatrixPosQuat4Scale(const vec3_t pos, const vec4_t quat, const vec3_t scale, vec_t result[12]);
void            QuaternionSlerp(const vec4_t p, vec4_t q, vec_t t, vec4_t qt);

#define AngleVectorsFLU(a,f,l,u) do{AngleVectors(a,f,l,u);VectorNegate(l,l);}while(0)

//projection matricies of different types... gesh
void            Matrix4x4_CM_Orthographic (vec_t *proj, vec_t xmin, vec_t xmax, vec_t ymax, vec_t ymin, vec_t znear, vec_t zfar);
void            Matrix4x4_CM_OrthographicD3D(vec_t *proj, vec_t xmin, vec_t xmax, vec_t ymax, vec_t ymin, vec_t znear, vec_t zfar);
void            Matrix4x4_CM_Projection_Offset(vec_t *proj, vec_t fovl, vec_t fovr, vec_t fovu, vec_t fovd, vec_t neard, vec_t fard, qboolean d3d);
void            Matrix4x4_CM_Projection_Far(vec_t *proj, vec_t fovx, vec_t fovy, vec_t neard, vec_t fard, qboolean d3d);
void            Matrix4x4_CM_Projection2 (vec_t *proj, vec_t fovx, vec_t fovy, vec_t neard);
void		Matrix4x4_CM_Projection_Inf(vec_t *proj, vec_t fovx, vec_t fovy, vec_t neard, qboolean d3d);

fixed16_t	Mul16_30 (fixed16_t multiplier, fixed16_t multiplicand);
int			Q_log2 (int val);

void		Matrix3x4_InvertTo3x3(const vec_t *in, vec_t *result);

fixed16_t	Mul16_30 (fixed16_t multiplier, fixed16_t multiplicand);
int			Q_log2 (int val);
void		R_ConcatRotations (const matrix3x3 in1, const matrix3x3 in2, matrix3x3 out);
void		R_ConcatRotationsPad (const matrix3x4 in1, const matrix3x4 in2, matrix3x4 out);
void		QDECL R_ConcatTransforms (const matrix3x4 in1, const matrix3x4 in2, matrix3x4 out);
void		R_ConcatTransformsAxis (const matrix3x3 in1, const matrix3x4 in2, matrix3x4 out);
void		PerpendicularVector(vec3_t dst, const vec3_t src);
void		RotatePointAroundVector (vec3_t dst, const vec3_t dir, const vec3_t point, vec_t degrees);
void		RotateLightVector(const vec3_t *axis, const vec3_t origin, const vec3_t lightpoint, vec3_t result);
int			VectorCompare (const vec3_t v1, const vec3_t v2);
int			Vector4Compare (const vec4_t v1, const vec4_t v2);
void		VectorInverse (vec3_t v);
void		_VectorMA (const vec3_t veca, const vec_t scale, const vec3_t vecb, vec3_t vecc);
vec_t		QDECL VectorNormalize (vec3_t v);		// returns vector length
vec_t		QDECL VectorNormalize2 (const vec3_t v, vec3_t out);
void		VectorNormalizeFast(vec3_t v);
void		VectorTransform (const vec3_t in1, const matrix3x4 in2, vec3_t out);
void		VectorVectors (const vec3_t forward, vec3_t right, vec3_t up);
