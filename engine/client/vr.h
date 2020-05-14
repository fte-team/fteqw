#include "merged.h"
typedef struct vrsetup_s
{
	//engine-set
	size_t structsize;
	enum
	{
		VR_HEADLESS,	//not to be confused with decapitation
		VR_EGL,
		VR_X11_GLX,
//		VR_ANDROID_EGL,
		VR_WIN_WGL,
		VR_VULKAN,		//vulkan has no platform variation
		VR_D3D11,		//d3d11 only works on windows, so no platform variation
	} vrplatform;		//the type of renderer/args getting inited. abort if unknown.
	void *userctx;		//for use in callbacks.
	qboolean (*createinstance)(struct vrsetup_s *, char *instanceextensions, void *result);				//used by vulkan, can be null for other renderers

	//vr-set (by preinit)
	struct
	{
		int major, minor;
	} minver, maxver;
	unsigned int deviceid[2];
	char *deviceextensions;


	//engine-set (for full init)
	//this stuff is intentionally at the end
	union {
		struct
		{
			void *display;
			int visualid;
			void *glxfbconfig;
			unsigned long drawable;	//really int32
			void *glxcontext;
		} x11_glx;

		struct
		{
			void *(*getprocaddr)(const char *name);
			void *egldisplay;
			void *eglconfig;
			void *eglcontext;
		} egl;

		struct
		{
			void *hdc;
			void *hglrc;
		} wgl;

		struct
		{
			void *device;
		} d3d;

		struct
		{	//sometimes pointers, sometimes ints. nasty datatypes that suck. this is hideous.
#ifndef VulkanAPIRandomness
	#if defined(__LP64__) || defined(_WIN64)
		#define VulkanAPIRandomness void*
	#elif defined(_MSC_VER) && _MSC_VER < 1300
		#define VulkanAPIRandomness __int64
	#else
		#define VulkanAPIRandomness long long
	#endif
#endif
			VulkanAPIRandomness instance;
			VulkanAPIRandomness physicaldevice;
			VulkanAPIRandomness device;
			unsigned int queuefamily;
			unsigned int queueindex;
		} vk;
	};
} vrsetup_t;

//interface registered by plugins for VR stuff.
typedef struct plugvrfuncs_s
{
	const char	*description;
	qboolean	(*Prepare)	(vrsetup_t *setupinfo);	//called before graphics context init
	qboolean	(*Init)		(vrsetup_t *setupinfo, rendererstate_t *info);	//called after graphics context init
	qboolean	(*SyncFrame)(double *frametime);	//called in the client's main loop, to block/tweak frame times. True means the game should render as fast as possible.
	qboolean	(*Render)	(void(*rendereye)(texid_t tex, vec4_t fovoverride, vec3_t axisorg[4]));
	void		(*Shutdown)	(void);
#define plugvrfuncs_name "VR"
} plugvrfuncs_t;
