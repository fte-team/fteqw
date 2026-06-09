#include "quakedef.h"

#ifdef USE_INTERNAL_JOLT
#define FTEENGINE
#undef FTEPLUGIN
#include "../../plugins/jolt/jolt.cpp"
#endif
