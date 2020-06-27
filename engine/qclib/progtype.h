#ifndef QCLIB_PROGTYPE_H
#define QCLIB_PROGTYPE_H

#if 0
//64bit primitives allows for:
//	greater precision timers (so maps can last longer without getting restarted)
//	planet-sized maps (with the engine's vec_t types changed too, and with some sort of magic for the gpu's precision).
//TODO: for this to work, someone'll have to go through the code to somehow deal with the vec_t/pvec_t/float differences.
#warning FTE isnt ready for this.
#include <stdint.h>
typedef double pvec_t;
typedef int64_t pint_t;
typedef uint64_t puint_t;

#include <inttypes.h>
#define pPRId PRId64
#define pPRIi PRIi64
#define pPRIu PRIu64
#define pPRIx PRIx64
#define QCVM_64
#else
//use 32bit types, for sanity.
typedef float pvec_t;
typedef int pint_t;
typedef unsigned int puint_t;
#define pPRId "d"
#define pPRIi "i"
#define pPRIu "u"
#define pPRIx "x"
#define QCVM_32
#endif

typedef unsigned int pbool;
typedef pvec_t pvec3_t[3];
typedef pint_t progsnum_t;
typedef puint_t func_t;
typedef puint_t string_t;

extern pvec3_t pvec3_origin;

#endif /* QCLIB_PROGTYPE_H */

