#ifndef QCLIB_PROGTYPE_H
#define QCLIB_PROGTYPE_H

#ifndef DLL_PROG

#else
typedef float vec_t;
typedef vec_t vec3_t[3];
#endif

#ifndef t_bool
#define t_bool
typedef int pbool;

#else
#define t_bool
#endif
typedef int progsnum_t;
typedef int func_t;
typedef int string_t;

#endif /* QCLIB_PROGTYPE_H */

