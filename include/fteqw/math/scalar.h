#pragma once

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <inttypes.h>
#include <math.h>

#if _MSC_VER >= 1300
	#define FTE_ALIGN(a) __declspec(align(a))
#elif (__GNUC__ >= 3 || defined(__clang__))
	#define FTE_ALIGN(a) __attribute__((aligned(a)))
#else
	#define FTE_ALIGN(a)
#endif
	
#if ((__cplusplus < 201103L) && (__STDC_VERSION__ < 202311L))
#define alignof _Alignof
#define alignas _Alignas
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

#ifndef FTE_NO_OPENGL
#include <GL/glcorearb.h>

/* floating point types */
typedef GLfloat              qfloat_t;
typedef GLdouble             qdouble_t;

/* integer types */
typedef khronos_intptr_t     qintptr_t;
typedef khronos_uintptr_t    quintptr_t;
typedef khronos_int16_t      qint16_t;
typedef khronos_uint16_t     quint16_t;
typedef khronos_int32_t      qint32_t;
typedef khronos_uint32_t     quint32_t;
typedef khronos_int64_t      qint64_t;
typedef khronos_uint64_t     quint64_t;
typedef intmax_t             qintmax_t;
typedef uintmax_t            quintmax_t;
typedef khronos_uint8_t      qbyte;

/* character types */
typedef char                 qchar;

/* enumerated boolean */
typedef unsigned int qboolean;
#define qfalse (KHRONOS_FALSE)
#define qtrue (KHRONOS_TRUE)
#undef  true
#undef  false
#ifndef __cplusplus
#define true qtrue
#define false qfalse
#endif
#else
#error "No OpenGL/KHR core platform headers found!"
#endif

//make shared
#ifndef QDECL
	#ifdef _MSC_VER
		#define QDECL _cdecl
	#else
		#define QDECL
	#endif
#endif
#ifndef VARGS
	#define VARGS QDECL
#endif

#ifndef FTE_WORDSIZE
	#ifdef __WORDSIZE
		#define FTE_WORDSIZE __WORDSIZE
	#elif defined(_WIN64)
		#define FTE_WORDSIZE 64
	#else
		#define FTE_WORDSIZE 32
	#endif
#endif

#ifdef _MSC_VER
	#if _MSC_VER >= 1900
		// MSVC 14 supports these
	#elif _MSC_VER >= 1310
		#define strtoull _strtoui64
		#define strtoll _strtoi64
	#else
		#define strtoull strtoul	//hopefully this won't cause too many issues
		#define strtoll strtol	//hopefully this won't cause too many issues
		#define DWORD_PTR DWORD		//32bit only
		#define ULONG_PTR ULONG
	#endif
#endif

#ifdef _MSC_VER
	#define pPRId "d"
	#define pPRIi "i"
	#define pPRIu "u"
	#define pPRIx "x"
	#define pPRIi64 "I64i"
	#define pPRIu64 "I64u"
	#define pPRIx64 "I64x"
	#define pPRIuSIZE PRIxPTR
#else
	#define pPRId PRId32
	#define pPRIi PRIi32
	#define pPRIu PRIu32
	#define pPRIx PRIx32
	#define pPRIi64 PRIi64
	#define pPRIu64 PRIu64
	#define pPRIx64 PRIx64
	#define pPRIuSIZE PRIxPTR
#endif
#define QCVM_32

