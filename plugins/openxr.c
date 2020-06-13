#include "plugin.h"
static plugfsfuncs_t *fsfuncs;

#include "../engine/client/vr.h"

//#define XR_NO_PROTOTYPES

//figure out which platforms(read: windowing apis) we need...
#if defined(_WIN32)
	#define XR_USE_PLATFORM_WIN32
//#elif defined(ANDROID)
//	#define XR_USE_PLATFORM_ANDROID
#else
	#ifndef NO_X11
		#define XR_USE_PLATFORM_XLIB
	#endif
	#if defined(GLQUAKE) && defined(USE_EGL)
		//wayland, android, and x11-egl can all just use the EGL path...
		//at least once the openxr spec gets fixed (the wayland extension is apparently basically unusable)
		//note: XR_MND_egl_enable is a vendor extension to work around openxr stupidly trying to ignore it entirely.
		#define XR_USE_PLATFORM_EGL
	#endif
#endif

//figure out which graphics apis we need...
#ifdef GLQUAKE
	#define XR_USE_GRAPHICS_API_OPENGL
#endif
#ifdef VKQUAKE
	#define XR_USE_GRAPHICS_API_VULKAN
#endif
#ifdef D3D11QUAKE
	#ifdef _WIN32
		#define XR_USE_GRAPHICS_API_D3D11
	#endif
#endif

//include any headers we need for things to make sense.
#ifdef XR_USE_GRAPHICS_API_OPENGL
	#include "glquake.h"
#endif
#ifdef XR_USE_GRAPHICS_API_VULKAN
	#include "../engine/vk/vkrenderer.h"
#endif
#ifdef XR_USE_GRAPHICS_API_D3D11
	#include <d3d11.h>
#endif
#ifdef XR_USE_PLATFORM_EGL
	#include "gl_videgl.h"
#endif
#ifdef XR_USE_PLATFORM_XLIB
	#include <GL/glx.h>
#endif

//and finally include openxr stuff now that its hopefully not going to fail about missing typedefs.
#include <openxr/openxr_platform.h>

#ifdef XR_NO_PROTOTYPES
#define XRFUNCS		\
		XRFUNC(xrGetInstanceProcAddr)	\
		XRFUNC(xrEnumerateInstanceExtensionProperties)	\
		XRFUNC(xrCreateInstance)	\
		XRFUNC(xrGetInstanceProperties)	\
		XRFUNC(xrGetSystem)	\
		XRFUNC(xrGetSystemProperties)	\
		XRFUNC(xrEnumerateViewConfigurations) \
		XRFUNC(xrEnumerateViewConfigurationViews)	\
		XRFUNC(xrCreateSession)	\
		XRFUNC(xrCreateReferenceSpace)	\
		XRFUNC(xrCreateActionSet)	\
		XRFUNC(xrStringToPath)	\
		XRFUNC(xrCreateAction)	\
		XRFUNC(xrSuggestInteractionProfileBindings)	\
		XRFUNC(xrCreateActionSpace)	\
		XRFUNC(xrAttachSessionActionSets)	\
		XRFUNC(xrSyncActions)	\
		XRFUNC(xrGetActionStatePose)	\
		XRFUNC(xrLocateSpace)	\
		XRFUNC(xrGetActionStateBoolean)	\
		XRFUNC(xrGetActionStateFloat)	\
		XRFUNC(xrGetActionStateVector2f)	\
		XRFUNC(xrGetCurrentInteractionProfile)	\
		XRFUNC(xrEnumerateBoundSourcesForAction)	\
		XRFUNC(xrGetInputSourceLocalizedName)	\
		XRFUNC(xrPathToString)	\
		XRFUNC(xrCreateSwapchain)	\
		XRFUNC(xrEnumerateSwapchainFormats)	\
		XRFUNC(xrEnumerateSwapchainImages)	\
		XRFUNC(xrPollEvent)	\
		XRFUNC(xrBeginSession)	\
		XRFUNC(xrWaitFrame)	\
		XRFUNC(xrBeginFrame)	\
		XRFUNC(xrLocateViews)	\
		XRFUNC(xrAcquireSwapchainImage)	\
		XRFUNC(xrWaitSwapchainImage)	\
		XRFUNC(xrReleaseSwapchainImage)	\
		XRFUNC(xrEndFrame)	\
		XRFUNC(xrRequestExitSession)	\
		XRFUNC(xrEndSession)	\
		XRFUNC(xrDestroySwapchain)	\
		XRFUNC(xrDestroySpace)	\
		XRFUNC(xrDestroySession)	\
		XRFUNC(xrDestroyInstance)
#define XRFUNC(n) static PFN_##n n;
XRFUNCS
#undef XRFUNC
#endif

#ifdef SVNREVISION
	#define APPLICATIONVERSION atoi(STRINGIFY(SVNREVISION))
	#define ENGINEVERSION atoi(STRINGIFY(SVNREVISION))
#else
	#define APPLICATIONVERSION 0
	#define ENGINEVERSION 0
#endif

static cvar_t *xr_enable;
static cvar_t *xr_formfactor;
static cvar_t *xr_viewconfig;
static cvar_t *xr_metresize;
static cvar_t *xr_skipregularview;

#define METRES_TO_QUAKE(x) ((x)*xr_metresize->value)
#define QUAKE_TO_METRES(x) ((x)/xr_metresize->value)

static void XR_PoseToTransform(XrPosef *pose, vec3_t axisorg[4])
{
	float xx, xy, xz, xw, yy, yz, yw, zz, zw;
	float x2, y2, z2;
	x2 = pose->orientation.x + pose->orientation.x;
	y2 = pose->orientation.y + pose->orientation.y;
	z2 = pose->orientation.z + pose->orientation.z;

	xx = pose->orientation.x * x2;   xy = pose->orientation.x * y2;   xz = pose->orientation.x * z2;
	yy = pose->orientation.y * y2;   yz = pose->orientation.y * z2;   zz = pose->orientation.z * z2;
	xw = pose->orientation.w * x2;   yw = pose->orientation.w * y2;   zw = pose->orientation.w * z2;

	axisorg[0][0] = (1.0f - (yy + zz));
	axisorg[0][1] = (xy + zw);
	axisorg[0][2] = (xz - yw);

	axisorg[1][0] = (xy - zw);
	axisorg[1][1] = (1.0f - (xx + zz));
	axisorg[1][2] = (yz + xw);

	axisorg[2][0] = (xz + yw);
	axisorg[2][1] = (yz - xw);
	axisorg[2][2] = (1.0f - (xx + yy));

	axisorg[3][0]  =     METRES_TO_QUAKE(-pose->position.z);	//-z forward
	axisorg[3][1]  =     METRES_TO_QUAKE(pose->position.x);		//+x right
	axisorg[3][2]  =     METRES_TO_QUAKE(pose->position.y);		//+y up
}
#define VectorAngles VectorAnglesPluginsSuck
void QDECL VectorAngles(const float *forward, const float *up, float *result, qboolean meshpitch)	//up may be NULL
{
	float	yaw, pitch, roll;

	if (forward[1] == 0 && forward[0] == 0)
	{
		if (forward[2] > 0)
		{
			pitch = -M_PI * 0.5;
			yaw = up ? atan2(-up[1], -up[0]) : 0;
		}
		else
		{
			pitch = M_PI * 0.5;
			yaw = up ? atan2(up[1], up[0]) : 0;
		}
		roll = 0;
	}
	else
	{
		yaw = atan2(forward[1], forward[0]);
		pitch = -atan2(forward[2], sqrt (forward[0]*forward[0] + forward[1]*forward[1]));

		if (up)
		{
			vec_t cp = cos(pitch), sp = sin(pitch);
			vec_t cy = cos(yaw), sy = sin(yaw);
			vec3_t tleft, tup;
			tleft[0] = -sy;
			tleft[1] = cy;
			tleft[2] = 0;
			tup[0] = sp*cy;
			tup[1] = sp*sy;
			tup[2] = cp;
			roll = -atan2(DotProduct(up, tleft), DotProduct(up, tup));
		}
		else
			roll = 0;
	}

	pitch *= 180 / M_PI;
	yaw *= 180 / M_PI;
	roll *= 180 / M_PI;
//	if (meshpitch)
//		pitch *= r_meshpitch.value;
	if (pitch < 0)
		pitch += 360;
	if (yaw < 0)
		yaw += 360;
	if (roll < 0)
		roll += 360;

	result[0] = pitch;
	result[1] = yaw;
	result[2] = roll;
}

static struct
{
//instance state (in case we want to start up)
	XrInstance instance;	//loader context
	XrSystemId systemid;	//device type thingie we're going for
#define MAX_VIEW_COUNT 12	//kinda abusive, but that's VR for you.
	unsigned int viewcount;
	XrViewConfigurationView *views;
	XrViewConfigurationType viewtype;

//engine context info (for restarting sessions)
	int renderer;			//rendering api we're using
	void *bindinginfo;		//appropriate XrGraphicsBinding*KHR struct so we can restart sessions.

//session state
	XrSession session;		//driver context
	XrSessionState state;
	XrSpace space;
	struct
	{	//basically just swapchain state.
		XrSwapchain swapchain;
		unsigned int numswapimages;
		XrSwapchainSubImage subimage;
		image_t *swapimages;
	} eye[MAX_VIEW_COUNT];	//note that eye is a vauge term.

	XrActiveActionSet actionset;

	qboolean timeknown;
	XrTime time;
	XrFrameState framestate;
	int srgb;	//<0 = gamma-only. 0 = no srgb at all, >0 full srgb, including textures and stuff

	unsigned int numactions;
	struct
	{
		XrActionType acttype;
		const char *actname;		//doubles up as command names for buttons
		const char *actdescription;	//user-visible string (exposed via openxr runtime somehow)
		const char *subactionpath;	//somethingblahblah

		XrAction	action;	//for querying.
		XrPath		path;	//for querying.
		XrSpace		space;	//for poses.
		qboolean	held;	//for buttons.
	} actions[256];
} xr;

static qboolean QDECL XR_PluginMayUnload(void)
{
	if (xr.instance)
		return false;	//something is still using us... don't let our code go away.
	return true;
}
static void XR_SessionEnded(void)
{
	size_t u;
	if (xr.space)
	{
		xrDestroySpace(xr.space);
		xr.space = XR_NULL_HANDLE;
	}

	for (u = 0; u < countof(xr.eye); u++)
	{
		free(xr.eye[u].swapimages);
		xr.eye[u].swapimages = NULL;
		xr.eye[u].numswapimages = 0;
		if (xr.eye[u].swapchain)
		{
			xrDestroySwapchain(xr.eye[u].swapchain);
			xr.eye[u].swapchain = XR_NULL_HANDLE;
		}
	}

	if (xr.session)
	{
		xrDestroySession(xr.session);
		xr.session = XR_NULL_HANDLE;
	}
}
static void XR_Shutdown(void)
{	//called on any kind of failure
	XR_SessionEnded();

	free(xr.bindinginfo);
	free(xr.views);
	if (xr.instance)
		xrDestroyInstance(xr.instance);

	memset(&xr, 0, sizeof(xr));
}

static qboolean XR_PreInit(vrsetup_t *qreqs)
{
	XrResult res;
	const char *ext;

	XR_Shutdown();	//just in case...

	if (qreqs->structsize != sizeof(*qreqs))
		return false;	//nope, get lost.

	switch(qreqs->vrplatform)
	{
	/*case VR_HEADLESS:
		ext = XR_MND_HEADLESS_EXTENSION_NAME;
		break;*/
#ifdef XR_USE_GRAPHICS_API_VULKAN
	case VR_VULKAN:
		ext = XR_KHR_VULKAN_ENABLE_EXTENSION_NAME;
		break;
#endif
#ifdef XR_USE_GRAPHICS_API_OPENGL
#ifdef XR_MND_EGL_ENABLE_EXTENSION_NAME
	case VR_EGL:
		ext = XR_MND_EGL_ENABLE_EXTENSION_NAME;
		break;
#endif
#ifdef XR_USE_PLATFORM_XLIB
	case VR_X11_GLX:
#endif
#ifdef XR_USE_PLATFORM_WIN32
	case VR_WIN_WGL:
#endif
		ext = XR_KHR_OPENGL_ENABLE_EXTENSION_NAME;
		break;
#endif
#ifdef XR_USE_GRAPHICS_API_D3D11
	case VR_D3D11:
		ext = XR_KHR_D3D11_ENABLE_EXTENSION_NAME;
		break;
#endif
	default:
		Con_Printf("OpenXR: windowing-api or rendering-api not supported\n");
		return false;
	}

#ifdef XR_NO_PROTOTYPES
	{
		static dllhandle_t *lib;
		static dllfunction_t funcs[] = {
			#define XRFUNC(n) {(void*)&n, #n},
				XRFUNCS
			#undef XRFUNC
			{NULL}};
#define XR_LOADER_LIBNAME "libopenxr_loader"
		if (!lib)
			lib = plugfuncs->LoadDLL(XR_LOADER_LIBNAME, funcs);
		if (!lib)
		{
			Con_Printf(CON_ERROR"OpenXR: Unable to load "XR_LOADER_LIBNAME"\n");
			return false;
		}
	}
#endif

	{
		unsigned int exts = 0, u=0;
		XrExtensionProperties *extlist;
		res = xrEnumerateInstanceExtensionProperties(NULL, 0, &exts, NULL);
		if (XR_SUCCEEDED(res))
		{
			extlist = calloc(exts, sizeof(*extlist));
			for (u = 0; u < exts; u++)
				extlist[u].type = XR_TYPE_EXTENSION_PROPERTIES;
			xrEnumerateInstanceExtensionProperties(NULL, exts, &exts, extlist);
			for (u = 0; u < exts; u++)
				if (!strcmp(extlist[u].extensionName, ext))
					break;
			free(extlist);
		}
		if (u == exts)
		{
			Con_Printf("OpenXR: instance driver does not support required %s\n", ext);
			return false;
		}
	}

	xr.instance = XR_NULL_HANDLE;

	//create our instance
	{
		XrInstanceCreateInfo createinfo = {XR_TYPE_INSTANCE_CREATE_INFO};
		createinfo.createFlags = 0;
		Q_strlcpy(createinfo.applicationInfo.applicationName, FULLENGINENAME, sizeof(createinfo.applicationInfo.applicationName));
		createinfo.applicationInfo.applicationVersion = APPLICATIONVERSION;
		Q_strlcpy(createinfo.applicationInfo.engineName, "FTEQW", sizeof(createinfo.applicationInfo.engineName));
		createinfo.applicationInfo.engineVersion = ENGINEVERSION;
		createinfo.applicationInfo.apiVersion = XR_CURRENT_API_VERSION;
		createinfo.enabledApiLayerCount = 0;
		createinfo.enabledApiLayerNames = NULL;
		createinfo.enabledExtensionCount = ext?1:0;
		createinfo.enabledExtensionNames = &ext;
		res = xrCreateInstance(&createinfo, &xr.instance);
	}
	if (XR_FAILED(res) || !xr.instance)
		return false;

	{
		XrInstanceProperties props = {XR_TYPE_INSTANCE_PROPERTIES};
		if (!XR_FAILED(xrGetInstanceProperties(xr.instance, &props)))
			Con_Printf("OpenXR Runtime: %s    %u.%u.%u\n", props.runtimeName, XR_VERSION_MAJOR(props.runtimeVersion), XR_VERSION_MINOR(props.runtimeVersion), XR_VERSION_PATCH(props.runtimeVersion));
		else
			Con_Printf("OpenXR Runtime: Unable to determine runtime version\n");
	}

	{
		XrSystemGetInfo systemInfo = { XR_TYPE_SYSTEM_GET_INFO };
		if (!strncasecmp(xr_formfactor->string, "hand", 4))
			systemInfo.formFactor = XR_FORM_FACTOR_HANDHELD_DISPLAY;
		else if (!strncasecmp(xr_formfactor->string, "head",4))
			systemInfo.formFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;
		else
		{
			if (*xr_formfactor->string)
				Con_Printf("\"%s\" is not a recognised value for xr_formfactor\n", xr_formfactor->string);
			else
				Con_Printf("xr_formfactor not set, assuming headmounted\n");
			systemInfo.formFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;
		}
		res = xrGetSystem(xr.instance, &systemInfo, &xr.systemid);
		if (XR_FAILED(res) || !xr.systemid)
			return false;
	}

	{
		XrSystemProperties props = {XR_TYPE_SYSTEM_PROPERTIES};
		if (XR_SUCCEEDED(xrGetSystemProperties(xr.instance, xr.systemid, &props)))
		{
			Con_Printf("OpenXR System: %s\n", props.systemName);
		}
	}

	switch(qreqs->vrplatform)
	{
	default:
		XR_Shutdown();
		return false;
	case VR_HEADLESS:
		break;

#ifdef XR_USE_GRAPHICS_API_VULKAN
	case VR_VULKAN:
		{
			XrGraphicsRequirementsVulkanKHR reqs = {XR_TYPE_GRAPHICS_REQUIREMENTS_VULKAN_KHR};
			VkInstance inst = VK_NULL_HANDLE;
			VkPhysicalDevice physdev;
			uint32_t extlen;
			char *extstr;	//space-delimited list, for some reason. writable though.

			PFN_xrGetVulkanGraphicsRequirementsKHR xrGetVulkanGraphicsRequirementsKHR;
			PFN_xrGetVulkanInstanceExtensionsKHR xrGetVulkanInstanceExtensionsKHR;
			PFN_xrGetVulkanDeviceExtensionsKHR xrGetVulkanDeviceExtensionsKHR;
			PFN_xrGetVulkanGraphicsDeviceKHR xrGetVulkanGraphicsDeviceKHR;
			if (XR_FAILED(xrGetInstanceProcAddr(xr.instance, "xrGetVulkanGraphicsRequirementsKHR", (PFN_xrVoidFunction*)&xrGetVulkanGraphicsRequirementsKHR)) ||
				XR_FAILED(xrGetInstanceProcAddr(xr.instance, "xrGetVulkanInstanceExtensionsKHR", (PFN_xrVoidFunction*)&xrGetVulkanInstanceExtensionsKHR)) ||
				XR_FAILED(xrGetInstanceProcAddr(xr.instance, "xrGetVulkanDeviceExtensionsKHR", (PFN_xrVoidFunction*)&xrGetVulkanDeviceExtensionsKHR)) ||
				XR_FAILED(xrGetInstanceProcAddr(xr.instance, "xrGetVulkanGraphicsDeviceKHR", (PFN_xrVoidFunction*)&xrGetVulkanGraphicsDeviceKHR)))
				return false;
			xrGetVulkanGraphicsRequirementsKHR(xr.instance, xr.systemid, &reqs);
			qreqs->maxver.major = XR_VERSION_MAJOR(reqs.maxApiVersionSupported);
			qreqs->maxver.minor = XR_VERSION_MINOR(reqs.maxApiVersionSupported);
			qreqs->minver.major = XR_VERSION_MAJOR(reqs.minApiVersionSupported);
			qreqs->minver.minor = XR_VERSION_MINOR(reqs.minApiVersionSupported);

			xrGetVulkanInstanceExtensionsKHR(xr.instance, xr.systemid, 0, &extlen, NULL);
			extstr = malloc(extlen);
			xrGetVulkanInstanceExtensionsKHR(xr.instance, xr.systemid, extlen, &extlen, extstr);

			//create vulkan instance now...
			qreqs->createinstance(qreqs, extstr, &inst);
			free(extstr);

			xrGetVulkanDeviceExtensionsKHR(xr.instance, xr.systemid, 0, &extlen, NULL);
			extstr = malloc(extlen);
			xrGetVulkanDeviceExtensionsKHR(xr.instance, xr.systemid, extlen, &extlen, extstr);

			res = xrGetVulkanGraphicsDeviceKHR(xr.instance, xr.systemid, inst, &physdev);

			qreqs->deviceextensions = extstr;
			qreqs->vk.physicaldevice = physdev;
		}
		break;
#endif

#ifdef XR_USE_GRAPHICS_API_OPENGL
	case VR_X11_GLX:
	case VR_EGL:
	case VR_WIN_WGL:
		{
			XrGraphicsRequirementsOpenGLKHR reqs = {XR_TYPE_GRAPHICS_REQUIREMENTS_OPENGL_KHR};
			PFN_xrGetOpenGLGraphicsRequirementsKHR xrGetOpenGLGraphicsRequirementsKHR;
			if (XR_SUCCEEDED(xrGetInstanceProcAddr(xr.instance, "xrGetOpenGLGraphicsRequirementsKHR", (PFN_xrVoidFunction*)&xrGetOpenGLGraphicsRequirementsKHR)))
				xrGetOpenGLGraphicsRequirementsKHR(xr.instance, xr.systemid, &reqs);

			qreqs->maxver.major = XR_VERSION_MAJOR(reqs.maxApiVersionSupported);
			qreqs->maxver.minor = XR_VERSION_MINOR(reqs.maxApiVersionSupported);
			qreqs->minver.major = XR_VERSION_MAJOR(reqs.minApiVersionSupported);
			qreqs->minver.minor = XR_VERSION_MINOR(reqs.minApiVersionSupported);
			//caller must validate when creating its context.
		}
		break;
#endif

#ifdef XR_USE_GRAPHICS_API_D3D11
	case VR_D3D11:
		{
			XrGraphicsRequirementsD3D11KHR reqs = {XR_TYPE_GRAPHICS_REQUIREMENTS_D3D11_KHR};
			PFN_xrGetD3D11GraphicsRequirementsKHR xrGetD3D11GraphicsRequirementsKHR;
			if (XR_SUCCEEDED(xrGetInstanceProcAddr(xr.instance, "xrGetD3D11GraphicsRequirementsKHR", (PFN_xrVoidFunction*)&xrGetD3D11GraphicsRequirementsKHR)))
				xrGetD3D11GraphicsRequirementsKHR(xr.instance, xr.systemid, &reqs);

			qreqs->minver.major = reqs.minFeatureLevel;
			qreqs->deviceid[0] = reqs.adapterLuid.LowPart;
			qreqs->deviceid[1] = reqs.adapterLuid.HighPart;
		}
		break;
#endif
	}

	{
		XrViewConfigurationType *viewtype;
		uint32_t viewtypes, u;
		res = xrEnumerateViewConfigurations(xr.instance, xr.systemid, 0, &viewtypes, NULL);
		viewtype = alloca(viewtypes*sizeof(viewtype));
		res = xrEnumerateViewConfigurations(xr.instance, xr.systemid, viewtypes, &viewtypes, viewtype);
		xr.viewtype = (XrViewConfigurationType)0;
		for (u = 0; u < viewtypes; u++)
		{
			switch(viewtype[u])
			{
			case XR_VIEW_CONFIGURATION_TYPE_PRIMARY_MONO:
				if (!strcasecmp(xr_viewconfig->string, "mono"))
					xr.viewtype = viewtype[u];
				break;
			case XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO:
				if (!strcasecmp(xr_viewconfig->string, "stereo"))
					xr.viewtype = viewtype[u];
				break;
			case XR_VIEW_CONFIGURATION_TYPE_PRIMARY_QUAD_VARJO:
				if (!strcasecmp(xr_viewconfig->string, "quad"))
					xr.viewtype = viewtype[u];
				break;
			default:
				break;
			}
		}
		if (!xr.viewtype)
		{
			if (viewtypes)
				xr.viewtype = viewtype[0];

			if (*xr_viewconfig->string)
			{
				Con_Printf("OpenXR: Viewtype %s unavailable, using ", xr_viewconfig->string);
				switch(xr.viewtype)
				{
				case XR_VIEW_CONFIGURATION_TYPE_PRIMARY_MONO:		Con_Printf("mono\n"); break;
				case XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO:		Con_Printf("stereo\n"); break;
				case XR_VIEW_CONFIGURATION_TYPE_PRIMARY_QUAD_VARJO:	Con_Printf("quad\n"); break;
				default:
					Con_Printf("unknown (%i)\n", xr.viewtype);
					break;
				}
			}
		}
	}

	res = xrEnumerateViewConfigurationViews(xr.instance, xr.systemid, xr.viewtype, 0, &xr.viewcount, NULL);
	if (xr.viewcount > MAX_VIEW_COUNT)
		xr.viewcount = MAX_VIEW_COUNT;	//oh noes! evile!
	xr.views = calloc(1,sizeof(*xr.views)*xr.viewcount);
	res = xrEnumerateViewConfigurationViews(xr.instance, xr.systemid, xr.viewtype, xr.viewcount, &xr.viewcount, xr.views);

	//caller now knows what device/contextversion/etc to init with
	return true;
}

static qboolean XR_Init(vrsetup_t *qreqs, rendererstate_t *info)
{
	xr.srgb = info->srgb;
	switch(qreqs->vrplatform)
	{
	case VR_HEADLESS:
		break;
	default:
		return false;	//error. not supported in this build.
#ifdef XR_USE_GRAPHICS_API_VULKAN
	case VR_VULKAN:
		{
			XrGraphicsBindingVulkanKHR *vk = xr.bindinginfo = calloc(1, sizeof(*vk));
			vk->type = XR_TYPE_GRAPHICS_BINDING_VULKAN_KHR;
			vk->instance = qreqs->vk.instance;
			vk->physicalDevice = qreqs->vk.physicaldevice;
			vk->device = qreqs->vk.device;
			vk->queueFamilyIndex = qreqs->vk.queuefamily;
			vk->queueIndex = qreqs->vk.queueindex;
			xr.renderer = QR_VULKAN;
		}
		break;
#endif
#ifdef XR_USE_GRAPHICS_API_OPENGL
#ifdef XR_MND_EGL_ENABLE_EXTENSION_NAME
	case VR_EGL:	//x11-egl, wayland, and hopefully android...
		{
			XrGraphicsBindingEGLMND *egl = xr.bindinginfo = calloc(1, sizeof(*egl));
			egl->type = XR_TYPE_GRAPHICS_BINDING_EGL_MND;
			egl->getProcAddress = qreqs->egl.getprocaddr;
			egl->display = qreqs->egl.egldisplay;
			egl->config = qreqs->egl.eglconfig;
			egl->context = qreqs->egl.eglcontext;
			xr.renderer = QR_OPENGL;
		}
		break;
#endif
#ifdef XR_USE_PLATFORM_XLIB
	case VR_X11_GLX:
		{
			XrGraphicsBindingOpenGLXlibKHR *glx = xr.bindinginfo = calloc(1, sizeof(*glx));
			glx->type = XR_TYPE_GRAPHICS_BINDING_OPENGL_XLIB_KHR;
			glx->xDisplay = qreqs->x11_glx.display;
			glx->visualid = qreqs->x11_glx.visualid;
			glx->glxFBConfig = qreqs->x11_glx.glxfbconfig;
			glx->glxDrawable = qreqs->x11_glx.drawable;
			glx->glxContext = qreqs->x11_glx.glxcontext;
			xr.renderer = QR_OPENGL;
		}
		break;
#endif
#ifdef XR_USE_PLATFORM_WIN32
	case VR_WIN_WGL:
		{
			XrGraphicsBindingOpenGLWin32KHR *wgl = xr.bindinginfo = calloc(1, sizeof(*wgl));
			wgl->type = XR_TYPE_GRAPHICS_BINDING_OPENGL_WIN32_KHR;
			wgl->hDC = qreqs->wgl.hdc;
			wgl->hGLRC = qreqs->wgl.hglrc;
			xr.renderer = QR_OPENGL;
		}
		break;
#endif
#endif	//def XR_USE_GRAPHICS_API_OPENGL
#ifdef XR_USE_GRAPHICS_API_D3D11
	case VR_D3D11:
		{
			XrGraphicsBindingD3D11KHR *d3d = xr.bindinginfo = calloc(1, sizeof(*d3d));
			d3d->type = XR_TYPE_GRAPHICS_BINDING_D3D11_KHR;
			d3d->device = qreqs->d3d.device;
			xr.renderer = QR_DIRECT3D11;
		}
		break;
#endif
	}
	return true;
}

static XrAction XR_DefineAction(XrActionType type, const char *name, const char *description, const char *root)
{
	XrActionCreateInfo info = {XR_TYPE_ACTION_CREATE_INFO};
	XrResult res;
	char *ffs;

	size_t u;
	int nconflicts = 0;
	int dconflicts = 0;
	for (u = 0; u < xr.numactions; u++)
	{
		if (xr.actions[u].acttype == type && !strcmp(xr.actions[u].actname, name) && !strcmp(xr.actions[u].actdescription, description) && !strcmp(xr.actions[u].subactionpath?xr.actions[u].subactionpath:"", root?root:""))
		{	//looks like a dupe...
			return xr.actions[u].action;
		}
		if (!strcasecmp(xr.actions[u].actname, name))
			nconflicts++;	//arse balls knob cock
		if (!strcasecmp(xr.actions[u].actdescription, description))
			dconflicts++;	//arse balls knob cock
	}
	if (xr.numactions == countof(xr.actions))
		return XR_NULL_HANDLE;	//nope, list full. sorry.

	xr.actions[u].acttype = type;
	xr.actions[u].actname = strdup(name);
	xr.actions[u].actdescription = strdup(description);
	xr.actions[u].subactionpath = (root&&*root)?strdup(root):NULL;
	xr.numactions++;

	if (xr.actions[u].subactionpath)
		xrStringToPath(xr.instance, xr.actions[u].subactionpath, &xr.actions[u].path);
	else
		xr.actions[u].path = XR_NULL_PATH;
	if (xr.actions[u].path == XR_NULL_PATH)
	{
		info.countSubactionPaths = 0;
		info.subactionPaths = NULL;
	}
	else
	{
		info.countSubactionPaths = 1;
		info.subactionPaths = &xr.actions[u].path;
	}
	info.actionType = xr.actions[u].acttype;
	if (*xr.actions[u].actname == '+')
		Q_strlcpy(info.actionName, xr.actions[u].actname+1, sizeof(info.actionName));
	else
		Q_strlcpy(info.actionName, xr.actions[u].actname, sizeof(info.actionName));
	while ((ffs=strchr(info.actionName, ' ')))	*ffs = '_';	//convert spaces to underscores.
	Q_strlcpy(info.localizedActionName, xr.actions[u].actdescription, sizeof(info.localizedActionName));
	res = xrCreateAction(xr.actionset.actionSet, &info, &xr.actions[u].action);
	if (XR_FAILED(res))
		Con_Printf("openxr: Unable to create action %s [%s] - %i\n", info.actionName, info.localizedActionName, res);

	return xr.actions[u].action;
}

static qboolean XR_ReadLine(const char **text, char *buffer, size_t buflen)
{
	char in;
	char *out = buffer;
	size_t len;
	if (buflen <= 1)
		return false;
	len = buflen-1;
	while (len > 0)
	{
		in = *(*text);
		if (!in)
		{
			if (len == buflen-1)
				return false;
			*out = 0;
			return true;
		}
		(*text)++;
		if (in == '\n')
			break;
		*out++ = in;
		len--;
	}
	*out = '\0';

	//if there's a trailing \r, strip it.
	if (out > buffer)
		if (out[-1] == '\r')
			out[-1] = 0;

	return true;
}

static int XR_BindProfileStr(const char *fname, const char *file)
{
	XrAction act;
	XrResult res;
	XrPath path;
	char line[1024];
	char name[1024];
	char type[256];
	char desc[1024];
	char bind[1024];
	char root[1024];
	unsigned int p;
	char prefix[2][1024];

	//first line is eg: /interaction_profiles/khr/simple_controller
	while (XR_ReadLine(&file, line, sizeof(line)))
	{
		cmdfuncs->TokenizeString(line);
		if (cmdfuncs->Argc())
			break;
	}
	cmdfuncs->Argv(0, name, sizeof(name));
	for (p = 0; p < countof(prefix); p++)
		cmdfuncs->Argv(p+1, prefix[p], sizeof(prefix[p]));
	if (*name && XR_SUCCEEDED(xrStringToPath(xr.instance, name, &path)))
	{	//okay, it accepted that path at least...
		XrInteractionProfileSuggestedBinding suggestedbindings = {XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING};
		unsigned int acts = 0;
		XrActionSuggestedBinding bindings[256];

		while (XR_ReadLine(&file, line, sizeof(line)))
		{
			cmdfuncs->TokenizeString(line);
			if (cmdfuncs->Argc())
			{
				cmdfuncs->Argv(0, name, sizeof(name));
				cmdfuncs->Argv(1, desc, sizeof(desc));
				cmdfuncs->Argv(2, type, sizeof(type));
				cmdfuncs->Argv(3, bind, sizeof(bind));
				cmdfuncs->Argv(4, root, sizeof(root));

				if (*type)
				{
					XrActionType xrtype;
					if (!strcasecmp(type, "button"))
						xrtype = XR_ACTION_TYPE_BOOLEAN_INPUT;
					else if (!strcasecmp(type, "float"))
						xrtype = XR_ACTION_TYPE_FLOAT_INPUT;
					else if (!strcasecmp(type, "vector2f"))
						xrtype = XR_ACTION_TYPE_VECTOR2F_INPUT;
					else if (!strcasecmp(type, "pose"))
						xrtype = XR_ACTION_TYPE_POSE_INPUT;
					else if (!strcasecmp(type, "vibration"))
						xrtype = XR_ACTION_TYPE_VIBRATION_OUTPUT;
					else
						continue;

					act = XR_DefineAction(xrtype, name, desc, root);
					if (act != XR_NULL_HANDLE && *bind)
					{
						if (*bind == '/')
						{
							res = xrStringToPath(xr.instance, bind, &bindings[acts].binding);
							if (XR_SUCCEEDED(res))
								bindings[acts++].action = act;
						}
						else for (p = 0; p < countof(prefix) && *prefix[p]=='/'; p++)
						{
							res = xrStringToPath(xr.instance, va("%s%s", prefix[p], bind), &bindings[acts].binding);
							if (XR_SUCCEEDED(res))
								bindings[acts++].action = act;
						}
					}
				}
			}
		}

		if (acts)
		{
			suggestedbindings.interactionProfile = path;
			suggestedbindings.countSuggestedBindings = acts;
			suggestedbindings.suggestedBindings = bindings;
			res = xrSuggestInteractionProfileBindings(xr.instance, &suggestedbindings);
			if (XR_FAILED(res))
				Con_Printf("%s: xrSuggestInteractionProfileBindings failed - %i\n", fname, res);
			return acts;
		}
	}
	return 0;
}

static int QDECL XR_BindProfileFile(const char *fname, qofs_t fsize, time_t mtime, void *ctx, struct searchpathfuncs_s *package)
{
	vfsfile_t *f = fsfuncs->OpenVFS(fname, "rb", FS_GAME);
	if (f)
	{
		size_t len = VFS_GETLEN(f);
		char *buf = malloc(len+1);
		VFS_READ(f, buf, len);
		buf[len] = 0;
		*(unsigned int*)ctx += XR_BindProfileStr(fname, buf);
		free(buf);
		VFS_CLOSE(f);
	}
	return false;
}

static void XR_SetupInputs(void)
{
	unsigned int h;
	XrResult res;

//begin instance-level init
	{
		XrActionSetCreateInfo info = {XR_TYPE_ACTION_SET_CREATE_INFO};
		Q_strlcpy(info.actionSetName, "actions", sizeof(info.actionSetName));
		Q_strlcpy(info.localizedActionSetName, FULLENGINENAME" Actions", sizeof(info.localizedActionSetName));
		info.priority = 0;

		xr.actionset.subactionPath = XR_NULL_PATH;
		res = xrCreateActionSet(xr.instance, &info, &xr.actionset.actionSet);
		if (XR_FAILED(res))
			Con_Printf("openxr: Unable to create actionset - %i\n", res);
	}

	h = 0;
	if (fsfuncs)
		fsfuncs->EnumerateFiles("oxr_*.binds", XR_BindProfileFile, &h);
	if (!h)	//no user bindings defined, use fallbacks. probably this needs to be per-mod.
	{
		//FIXME: set up some proper bindings!
		XR_BindProfileStr("khr_simple",
			"/interaction_profiles/khr/simple_controller    /user/hand/left/ /user/hand/right/\n"
			"+select		\"Select\"				button		input/select/click\n"
			"togglemenu		\"Toggle Menu\"			button		input/menu/click\n"
			"grip_pose		\"Grip Pose\"			pose		input/grip/pose\n"
			"aim_pose		\"Aim Pose\"			pose		input/aim/pose\n"
			"vibrate		\"A Vibrator\"			vibration	output/haptic\n"
			);

		XR_BindProfileStr("valve_index",
			"/interaction_profiles/valve/index_controller    /user/hand/left/ /user/hand/right/\n"
			//"unbound		\"Unused Button\"		button		input/system/click\n"
    		//"unbound		\"Unused Button\"		button		input/system/touch\n"
    		//"unbound		\"Unused Button\"		button		input/a/click\n"
    		//"unbound		\"Unused Button\"		button		input/a/touch\n"
    		//"unbound		\"Unused Button\"		button		input/b/click\n"
    		//"unbound		\"Unused Button\"		button		input/b/touch\n"
    		//"unbound		\"Unused Button\"		float		input/squeeze/value\n"
    		//"unbound		\"Unused Button\"		button		input/squeeze/force\n"
    		//"unbound		\"Unused Button\"		button		input/trigger/click\n"
    		//"unbound		\"Unused Button\"		float		input/trigger/value\n"
    		//"unbound		\"Unused Button\"		button		input/trigger/touch\n"
    		//"unbound		\"Unused Button\"		float		input/thumbstick/x\n"
    		//"unbound		\"Unused Button\"		float		input/thumbstick/y\n"
    		//"unbound		\"Unused Button\"		button		input/thumbstick/click\n"
    		//"unbound		\"Unused Button\"		button		input/thumbstick/touch\n"
    		//"unbound		\"Unused Button\"		float		input/trackpad/x\n"
    		//"unbound		\"Unused Button\"		float		input/trackpad/y\n"
    		//"unbound		\"Unused Button\"		button		input/trackpad/force\n"
    		//"unbound		\"Unused Button\"		button		input/trackpad/touch\n"
    		//"unbound		\"Unused Button\"		pose		input/grip/pose\n"
    		//"unbound		\"Unused Button\"		pose		input/aim/pose\n"
    		//"unbound		\"Unused Button\"		vibration	output/haptic\n"
			);

		//FIXME: map to quake's keys.
		XR_BindProfileStr("gamepad", "/interaction_profiles/microsoft/xbox_controller    /user/gamepad/\n"
			"togglemenu		Menu					button		input/menu/click\n"
			//"unbound		\"Unused Button\"		button		input/view/click\n"
			//"unbound		\"Unused Button\"		button		input/a/click\n"
			//"unbound		\"Unused Button\"		button		input/b/click\n"
			//"unbound		\"Unused Button\"		button		input/x/click\n"
			//"unbound		\"Unused Button\"		button		input/y/click\n"
			"+back			\"Move Backwards\"		button		input/dpad_down/click\n"
			"+moveright		\"Move Right\"			button		input/dpad_right/click\n"
			"+forward		\"Move Forward\"		button		input/dpad_up/click\n"
			"+moveleft		\"Move Left\"			button		input/dpad_left/click\n"
			"+jump			\"Jump\"				button		input/shoulder_left/click\n"
			"+attack		\"Attack\"				button		input/shoulder_right/click\n"
			//"unbound		\"Unused Button\"		button		input/thumbstick_left/click\n"
			//"unbound		\"Unused Button\"		button		input/thumbstick_right/click\n"
			//"unbound		\"Unused Axis\"			float		input/trigger_left/click\n"
			//"unbound		\"Unused Axis\"			float		input/trigger_right/click\n"
			//"unbound		\"Unused Axis\"			float		input/thumbstick_left/x\n"
			//"unbound		\"Unused Axis\"			float		input/thumbstick_left/y\n"
			//"unbound		\"Unused Axis\"			float		input/thumbstick_right/x\n"
			//"unbound		\"Unused Axis\"			float		input/thumbstick_right/y\n"
			//"unbound		\"Unused Vibrator\"		vibration	output/haptic_left\n"
			//"unbound		\"Unused Vibrator\"		vibration	output/haptic_left_trigger\n"
			//"unbound		\"Unused Vibrator\"		vibration	output/haptic_right\n"
			//"unbound		\"Unused Vibrator\"		vibration	output/haptic_right_trigger\n"
			);
	}

//begin session specific. stuff

	//create action space stuff.
	for (h = 0; h < xr.numactions; h++)
	{
		switch(xr.actions[h].acttype)
		{
		case XR_ACTION_TYPE_POSE_INPUT:
			{
				XrActionSpaceCreateInfo info = {XR_TYPE_ACTION_SPACE_CREATE_INFO};
				info.action = xr.actions[h].action;
				info.subactionPath = xr.actions[h].path;
				info.poseInActionSpace.orientation.w = 1;	//just fill with identity.

				res = xrCreateActionSpace(xr.session, &info, &xr.actions[h].space);
				if (XR_FAILED(res))
					Con_Printf("openxr: xrCreateActionSpace failed - %i\n", res);
			}
			break;
		default:
			xr.actions[h].space = XR_NULL_HANDLE;
			break;
		}
	}

	//and attach it.
	{
		XrSessionActionSetsAttachInfo info = {XR_TYPE_SESSION_ACTION_SETS_ATTACH_INFO};
		info.countActionSets = 1;
		info.actionSets = &xr.actionset.actionSet;
		res = xrAttachSessionActionSets(xr.session, &info);
		if (XR_FAILED(res))
			Con_Printf("openxr: xrAttachSessionActionSets failed - %i\n", res);
	}

#if 1
	if (cvarfuncs->GetFloat("developer"))
	{
		XrInteractionProfileState profile = {XR_TYPE_INTERACTION_PROFILE_STATE};
		XrPath path;
		unsigned int u;
		static const char *paths[] = {"/user/hand/left", "/user/hand/right", "/user/head", "/user/gamepad", "/user/treadmill", "/user/"};
		for (u = 0; u < countof(paths); u++)
		{
			xrStringToPath(xr.instance, paths[u], &path);
			res = xrGetCurrentInteractionProfile(xr.session, path, &profile);
			if (XR_SUCCEEDED(res))
			{
				char buf[256];
				uint32_t len = sizeof(buf);
				if (!profile.interactionProfile)
					Con_Printf("openxr: %s == no profile/device\n", paths[u]);
				else
				{
					res = xrPathToString(xr.instance, profile.interactionProfile, sizeof(buf), &len, buf);
					Con_Printf("openxr: %s == %s\n", paths[u], buf);
				}
			}
		}


		Con_Printf("Bound actions:\n");
		for (u = 0; u < xr.numactions; u++)
		{
			XrBoundSourcesForActionEnumerateInfo info = {XR_TYPE_BOUND_SOURCES_FOR_ACTION_ENUMERATE_INFO};
			uint32_t inputs, i, bufsize;
			XrPath input[20];
			info.action = xr.actions[u].action;
			res = xrEnumerateBoundSourcesForAction(xr.session, &info, countof(input), &inputs, input);
			if (XR_SUCCEEDED(res))
			{
				Con_Printf("\t%s:\n", xr.actions[u].actname);
				for (i = 0; i < inputs; i++)
				{
					char buffer[8192];
					XrInputSourceLocalizedNameGetInfo info = {XR_TYPE_INPUT_SOURCE_LOCALIZED_NAME_GET_INFO};
					info.sourcePath = input[i];
					info.whichComponents =	XR_INPUT_SOURCE_LOCALIZED_NAME_USER_PATH_BIT|	//'left hand'
											XR_INPUT_SOURCE_LOCALIZED_NAME_INTERACTION_PROFILE_BIT|	//'foo controller'
											XR_INPUT_SOURCE_LOCALIZED_NAME_COMPONENT_BIT;	//'trigger'
					res = xrGetInputSourceLocalizedName(xr.session, &info, sizeof(buffer), &bufsize, buffer);
					if (XR_FAILED(res))
						Q_snprintf(buffer, sizeof(buffer), "error %i", res);
					Con_Printf("\t\t%s\n", buffer);
				}
			}
			else if (res == XR_ERROR_HANDLE_INVALID)	//monado reports this for unimplemented things.
				Con_Printf("\t%s: error XR_ERROR_HANDLE_INVALID (not implemented?)\n", xr.actions[u].actname);
			else
				Con_Printf("\t%s: error %i\n", xr.actions[u].actname, res);
		}
	}
#endif
}
static void XR_UpdateInputs(XrTime time)
{
	XrResult res;
	unsigned int h;

	{
		XrActionsSyncInfo syncinfo = {XR_TYPE_ACTIONS_SYNC_INFO};
		syncinfo.countActiveActionSets = 1;
		syncinfo.activeActionSets = &xr.actionset;
		res = xrSyncActions(xr.session, &syncinfo);
		if (res == XR_SESSION_NOT_FOCUSED)
			;	//handle it anyway, giving us a chance to disable various inputs.
		else if (XR_FAILED(res))
			return;
	}

	for (h = 0; h < xr.numactions; h++)
	{
		if (xr.actions[h].action == XR_NULL_HANDLE)	//failed to init
			continue;
		switch(xr.actions[h].acttype)
		{
		case XR_ACTION_TYPE_POSE_INPUT:
			{
				XrActionStatePose pose = {XR_TYPE_ACTION_STATE_POSE};
				XrActionStateGetInfo info = {XR_TYPE_ACTION_STATE_GET_INFO};
				info.action = xr.actions[h].action;
				info.subactionPath = xr.actions[h].path;

				xrGetActionStatePose(xr.session, &info, &pose);
				if (pose.isActive)
				{	//its mapped to something, woo.
					XrSpaceLocation loc = {XR_TYPE_SPACE_LOCATION};
					vec3_t transform[4];
					xrLocateSpace(xr.actions[h].space, xr.space, time, &loc);
					//if (loc.locationFlags & XR_SPACE_LOCATION_ORIENTATION_VALID_BIT)
					//if (loc.locationFlags & XR_SPACE_LOCATION_POSITION_VALID_BIT)
					//if (loc.locationFlags & XR_SPACE_LOCATION_ORIENTATION_TRACKED_BIT)
					//if (loc.locationFlags & XR_SPACE_LOCATION_POSITION_TRACKED_BIT)
					XR_PoseToTransform(&loc.pose, transform);

					{
						vec3_t angles;
						char cmd[256];
						VectorAngles(transform[0], transform[2], angles, false);
						Q_snprintf(cmd, sizeof(cmd), "echo %s %g %g %g %g %g %g\n", xr.actions[h].actname, angles[0], angles[1], angles[2], transform[3][0], transform[3][1], transform[3][2]);
						cmdfuncs->AddText(cmd, false);
					}
				}
			}
			break;
		case XR_ACTION_TYPE_BOOLEAN_INPUT:
			{
				XrActionStateBoolean state = {XR_TYPE_ACTION_STATE_BOOLEAN};
				XrActionStateGetInfo info = {XR_TYPE_ACTION_STATE_GET_INFO};
				info.action = xr.actions[h].action;
				info.subactionPath = xr.actions[h].path;
				xrGetActionStateBoolean(xr.session, &info, &state);
				if (!state.isActive) state.currentState = XR_FALSE;
				if ((!!state.currentState) != xr.actions[h].held)
				{
					xr.actions[h].held = !!state.currentState;
					if (xr.actions[h].held || *xr.actions[h].actname == '+')
					{
						char cmd[256];
						Q_strlcpy(cmd, xr.actions[h].actname, sizeof(cmd));
						Q_strlcat(cmd, "\n", sizeof(cmd));
						if (!xr.actions[h].held)
							*cmd = '-';	//release events.
						cmdfuncs->AddText(cmd, false);
					}
				}
			}
			break;
		case XR_ACTION_TYPE_FLOAT_INPUT:
			{
				XrActionStateFloat state = {XR_TYPE_ACTION_STATE_FLOAT};
				XrActionStateGetInfo info = {XR_TYPE_ACTION_STATE_GET_INFO};
				info.action = xr.actions[h].action;
				info.subactionPath = xr.actions[h].path;
				xrGetActionStateFloat(xr.session, &info, &state);

				if (!state.isActive) state.currentState = 0.0f;
				{
					char cmd[256];
					Q_snprintf(cmd, sizeof(cmd), "echo %s %g\n", xr.actions[h].actname, state.currentState);
					cmdfuncs->AddText(cmd, false);
				}
			}
			break;
		case XR_ACTION_TYPE_VECTOR2F_INPUT:
			{
				XrActionStateVector2f state = {XR_TYPE_ACTION_STATE_VECTOR2F};
				XrActionStateGetInfo info = {XR_TYPE_ACTION_STATE_GET_INFO};
				info.action = xr.actions[h].action;
				info.subactionPath = xr.actions[h].path;
				xrGetActionStateVector2f(xr.session, &info, &state);

				if (!state.isActive) state.currentState.x = state.currentState.y = 0.0f;
				{
					char cmd[256];
					Q_snprintf(cmd, sizeof(cmd), "echo %s %g %g\n", xr.actions[h].actname, state.currentState.x, state.currentState.y);
					cmdfuncs->AddText(cmd, false);
				}
			}
			break;
		case XR_ACTION_TYPE_VIBRATION_OUTPUT:
		default:
			break;
		}
	}
}

static qboolean XR_Begin(void)
{
	uint32_t u;
	XrResult res;
	uint32_t swapfmts;
	int64_t *fmts, fmttouse=0;
	{
		XrSessionCreateInfo sessioninfo = {XR_TYPE_SESSION_CREATE_INFO};
		sessioninfo.next = xr.bindinginfo;
		sessioninfo.createFlags = 0;
		sessioninfo.systemId = xr.systemid;
		res = xrCreateSession(xr.instance, &sessioninfo, &xr.session);
	}
	if (XR_FAILED(res))
		return false;

	{
		XrReferenceSpaceCreateInfo info = {XR_TYPE_REFERENCE_SPACE_CREATE_INFO};
		info.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_VIEW;
		info.poseInReferenceSpace.orientation.w = 1;
		res = xrCreateReferenceSpace(xr.session, &info, &xr.space);
		if (XR_FAILED(res))
			return false;
	}

	xrEnumerateSwapchainFormats(xr.session, 0, &swapfmts, NULL);
	fmts = alloca(sizeof(*fmts)*swapfmts);
	xrEnumerateSwapchainFormats(xr.session, swapfmts, &swapfmts, fmts);
	if (!swapfmts)
		Con_Printf("OpenXR: No swapchain formats to use\n");
#ifdef XR_USE_GRAPHICS_API_OPENGL
	else if (xr.renderer == QR_OPENGL)
	{
		fmttouse = fmts[0];	//favour the first... its probably a bad choice though...
		for (u = 0; u < swapfmts; u++) switch(fmts[u])
		{
		case GL_RGBA16F:			Con_DPrintf("OpenXr fmt%u: %s\n", u, "GL_RGBA16F");		if (xr.srgb) fmttouse = fmts[u],u=swapfmts; break;
		case GL_RGBA8:				Con_DPrintf("OpenXr fmt%u: %s\n", u, "GL_RGBA8");		if (!xr.srgb) fmttouse = fmts[u],u=swapfmts; break;
		case GL_SRGB8_ALPHA8_EXT:	Con_DPrintf("OpenXr fmt%u: %s\n", u, "GL_SRGB8_ALPHA8");if (xr.srgb) fmttouse = fmts[u],u=swapfmts; break;
		default:
			Con_DPrintf("OpenXr fmt%u: %x\n", u, (unsigned)fmts[u]);
			break;
		}
	}
#endif
#ifdef XR_USE_GRAPHICS_API_VULKAN
	else if (xr.renderer == QR_VULKAN)
	{
		fmttouse = fmts[0];	//favour the first... its probably a bad choice though...
		for (u = 0; u < swapfmts; u++) switch(fmts[u])
		{
		case VK_FORMAT_R16G16B16A16_SFLOAT:		Con_DPrintf("OpenXr fmt%u: %s\n", u, "VK_FORMAT_R16G16B16A16_SFLOAT");	if (xr.srgb) fmttouse = fmts[u],u=swapfmts; break;
		case VK_FORMAT_B8G8R8A8_UNORM:			Con_DPrintf("OpenXr fmt%u: %s\n", u, "VK_FORMAT_B8G8R8A8_UNORM");		if (!xr.srgb) fmttouse = fmts[u],u=swapfmts; break;
		case VK_FORMAT_R8G8B8A8_UNORM:			Con_DPrintf("OpenXr fmt%u: %s\n", u, "VK_FORMAT_R8G8B8A8_UNORM");		if (!xr.srgb) fmttouse = fmts[u],u=swapfmts; break;
		case VK_FORMAT_B8G8R8A8_SRGB:			Con_DPrintf("OpenXr fmt%u: %s\n", u, "VK_FORMAT_B8G8R8A8_SRGB");		if (xr.srgb) fmttouse = fmts[u],u=swapfmts; break;
		case VK_FORMAT_R8G8B8A8_SRGB:			Con_DPrintf("OpenXr fmt%u: %s\n", u, "VK_FORMAT_R8G8B8A8_SRGB");		if (xr.srgb) fmttouse = fmts[u],u=swapfmts; break;
		default:
			Con_DPrintf("OpenXr fmt%u: %u\n", u, (unsigned)fmts[u]);
			break;
		}
	}
#endif
#ifdef XR_USE_GRAPHICS_API_D3D11
	else if (xr.renderer == QR_DIRECT3D11)
	{
		fmttouse = fmts[0];	//favour the first... its probably a bad choice though...
		for (u = 0; u < swapfmts; u++) switch(fmts[u])
		{
		case DXGI_FORMAT_R16G16B16A16_FLOAT:	Con_DPrintf("OpenXr fmt%u: %s\n", u, "DXGI_FORMAT_R16G16B16A16_FLOAT");		if (xr.srgb) fmttouse = fmts[u],u=swapfmts; break;
		case DXGI_FORMAT_B8G8R8A8_UNORM:		Con_DPrintf("OpenXr fmt%u: %s\n", u, "DXGI_FORMAT_B8G8R8A8_UNORM");			if (!xr.srgb) fmttouse = fmts[u],u=swapfmts; break;
		case DXGI_FORMAT_B8G8R8X8_UNORM:		Con_DPrintf("OpenXr fmt%u: %s\n", u, "DXGI_FORMAT_B8G8R8X8_UNORM");			if (!xr.srgb) fmttouse = fmts[u],u=swapfmts; break;
		case DXGI_FORMAT_R8G8B8A8_UNORM:		Con_DPrintf("OpenXr fmt%u: %s\n", u, "DXGI_FORMAT_R8G8B8A8_UNORM");			if (!xr.srgb) fmttouse = fmts[u],u=swapfmts; break;
		case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:	Con_DPrintf("OpenXr fmt%u: %s\n", u, "DXGI_FORMAT_B8G8R8A8_UNORM_SRGB");	if (xr.srgb) fmttouse = fmts[u],u=swapfmts; break;
		case DXGI_FORMAT_B8G8R8X8_UNORM_SRGB:	Con_DPrintf("OpenXr fmt%u: %s\n", u, "DXGI_FORMAT_B8G8R8X8_UNORM_SRGB");	if (xr.srgb) fmttouse = fmts[u],u=swapfmts; break;
		case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:	Con_DPrintf("OpenXr fmt%u: %s\n", u, "DXGI_FORMAT_R8G8B8A8_UNORM_SRGB");	if (xr.srgb) fmttouse = fmts[u],u=swapfmts; break;
		default:
			Con_DPrintf("OpenXr fmt%u: %x\n", u, (unsigned)fmts[u]);
			break;
		}
	}
#endif
	else
	{
		fmttouse = fmts[0];
		for (u = 0; u < swapfmts; u++)
			Con_Printf("fmt%u: %u / %x\n", u, (unsigned)fmts[u], (unsigned)fmts[u]);
	}

	for (u = 0; u < xr.viewcount; u++)
	{
		XrSwapchainCreateInfo swapinfo = {XR_TYPE_SWAPCHAIN_CREATE_INFO};
		swapinfo.createFlags = 0;
		swapinfo.usageFlags = XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT;
		swapinfo.format = fmttouse;
		swapinfo.sampleCount = 1;//xr.views->recommendedSwapchainSampleCount;
		swapinfo.width = xr.views->recommendedImageRectWidth;
		swapinfo.height = xr.views->recommendedImageRectHeight;
		swapinfo.faceCount = 1;	//2d, not a cube
		swapinfo.arraySize = 1;	//1 for 2d, a 1d array isn't allowed
		swapinfo.mipCount = 1;
		res = xrCreateSwapchain(xr.session, &swapinfo, &xr.eye[u].swapchain);
		if (XR_FAILED(res))
			return false;
		res = xrEnumerateSwapchainImages(xr.eye[u].swapchain, 0, &xr.eye[u].numswapimages, NULL);
		if (XR_FAILED(res))
			return false;

		//using a separate swapchain for each eye, so just depend upon npot here and use the whole image.
		xr.eye[u].subimage.imageRect.offset.x = 0;
		xr.eye[u].subimage.imageRect.offset.y = 0;
		xr.eye[u].subimage.imageRect.extent.width = swapinfo.width;
		xr.eye[u].subimage.imageRect.extent.height = swapinfo.height;
		xr.eye[u].subimage.swapchain = xr.eye[u].swapchain;
		xr.eye[u].subimage.imageArrayIndex = 0;

		//okay, this is annoying. the returned array size has different strides for different apis, etc.
		//translate it into something the relevant backend should understand.
		switch(xr.renderer)
		{
		default:
			return false;	//erk?
#ifdef XR_USE_GRAPHICS_API_D3D11
		case QR_DIRECT3D11:
			{
				uint32_t i;
				XrSwapchainImageD3D11KHR *xrimg = calloc(xr.eye[u].numswapimages, sizeof(*xrimg));
				for (i = 0; i < xr.eye[u].numswapimages; i++)
					xrimg[i].type = XR_TYPE_SWAPCHAIN_IMAGE_VULKAN_KHR;
				res = xrEnumerateSwapchainImages(xr.eye[u].swapchain, xr.eye[u].numswapimages, &xr.eye[u].numswapimages, (XrSwapchainImageBaseHeader*)xrimg);
				if (XR_FAILED(res))
					xr.eye[u].numswapimages = 0;

				xr.eye[u].swapimages = calloc(xr.eye[u].numswapimages, sizeof(*xr.eye[u].swapimages));
				for (i = 0; i < xr.eye[u].numswapimages; i++)
				{
					xr.eye[u].swapimages[i].ptr = xrimg[i].texture;
					xr.eye[u].swapimages[i].ptr2 = NULL;	//view
					xr.eye[u].swapimages[i].width = swapinfo.width;
					xr.eye[u].swapimages[i].height = swapinfo.height;
					xr.eye[u].swapimages[i].depth = 1;
					xr.eye[u].swapimages[i].status = TEX_LOADED;
				}
			}
			break;
#endif
#ifdef XR_USE_GRAPHICS_API_VULKAN
		case QR_VULKAN:
			{
				uint32_t i;
				XrSwapchainImageVulkanKHR *xrimg = calloc(xr.eye[u].numswapimages, sizeof(*xrimg));
				struct vk_image_s *vkimg;
				for (i = 0; i < xr.eye[u].numswapimages; i++)
					xrimg[i].type = XR_TYPE_SWAPCHAIN_IMAGE_VULKAN_KHR;
				res = xrEnumerateSwapchainImages(xr.eye[u].swapchain, xr.eye[u].numswapimages, &xr.eye[u].numswapimages, (XrSwapchainImageBaseHeader*)xrimg);
				if (XR_FAILED(res))
					xr.eye[u].numswapimages = 0;

				xr.eye[u].swapimages = calloc(xr.eye[u].numswapimages, sizeof(*xr.eye[u].swapimages)+sizeof(struct vk_image_s));
				vkimg = (struct vk_image_s*)&xr.eye[u].swapimages[xr.eye[u].numswapimages];
				for (i = 0; i < xr.eye[u].numswapimages; i++)
				{
					xr.eye[u].swapimages[i].vkimage = &vkimg[i];
					vkimg[i].image = xrimg[i].image;
					//vkimg[i].mem.* = 0;
					vkimg[i].view = VK_NULL_HANDLE;
					vkimg[i].sampler = VK_NULL_HANDLE;
					vkimg[i].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
					vkimg[i].width = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
					xr.eye[u].swapimages[i].width = vkimg[i].width = swapinfo.width;
					xr.eye[u].swapimages[i].height = vkimg[i].height = swapinfo.height;
					xr.eye[u].swapimages[i].depth = vkimg[i].layers = 1;
					vkimg[i].mipcount = swapinfo.mipCount;
					vkimg[i].encoding = PTI_INVALID; //blurgh, is this needed?
					vkimg[i].type = PTI_2D;
					xr.eye[u].swapimages[i].status = TEX_LOADED;
				}
			}
			break;
#endif
#ifdef XR_USE_GRAPHICS_API_OPENGL
		case QR_OPENGL:
			{
				uint32_t i;
				XrSwapchainImageOpenGLKHR *xrimg = calloc(xr.eye[u].numswapimages, sizeof(*xrimg));
				for (i = 0; i < xr.eye[u].numswapimages; i++)
					xrimg[i].type = XR_TYPE_SWAPCHAIN_IMAGE_OPENGL_KHR;
				res = xrEnumerateSwapchainImages(xr.eye[u].swapchain, xr.eye[u].numswapimages, &xr.eye[u].numswapimages, (XrSwapchainImageBaseHeader*)xrimg);
				if (XR_FAILED(res))
					xr.eye[u].numswapimages = 0;

				xr.eye[u].swapimages = calloc(xr.eye[u].numswapimages, sizeof(*xr.eye[u].swapimages));
				for (i = 0; i < xr.eye[u].numswapimages; i++)
				{
					xr.eye[u].swapimages[i].num = xrimg[i].image;
					xr.eye[u].swapimages[i].width = swapinfo.width;
					xr.eye[u].swapimages[i].height = swapinfo.height;
					xr.eye[u].swapimages[i].depth = 1;
					xr.eye[u].swapimages[i].status = TEX_LOADED;
				}
			}
			break;
#endif
		}
	}
	if (XR_FAILED(res))
		return false;

	XR_SetupInputs();

	return true;
}

static void XR_ProcessEvents(void)
{
	XrEventDataBuffer ev;
	XrResult res;
	for (;;)
	{
		res = xrPollEvent(xr.instance, &ev);
		if (res == XR_EVENT_UNAVAILABLE || XR_FAILED(res))
			return;	//nothing interesting here folks

		switch(ev.type)
		{
		default:	//no idea wtf that is
			Con_Printf("openxr event %u\n", ev.type);
			break;
		case XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED:
			{
				XrEventDataSessionStateChanged *s = (XrEventDataSessionStateChanged*)&ev;
				switch(s->state)
				{
				default:
					break;	//urgh
				case XR_SESSION_STATE_READY:
					{
						XrSessionBeginInfo info = {XR_TYPE_SESSION_BEGIN_INFO};
						info.primaryViewConfigurationType = xr.viewtype;
						res = xrBeginSession(xr.session, &info);
						if (XR_FAILED(res))
							Con_Printf("Unable to begin session: %i\n", res);
					}
					break;
				case XR_SESSION_STATE_STOPPING:
					res = xrEndSession(xr.session);
					if (XR_FAILED(res))
						Con_Printf("Unable to end session: %i\n", res);
					break;
				}
				xr.state = s->state;
			}
			break;
		}
	}
}
static qboolean XR_SyncFrame(double *frametime)
{
	XrResult res;

	if (!xr.instance)
		return false;

	memset(&xr.framestate, 0, sizeof(xr.framestate));
	xr.framestate.type = XR_TYPE_FRAME_STATE;
	switch(xr.state)
	{
	case XR_SESSION_STATE_FOCUSED:
	case XR_SESSION_STATE_SYNCHRONIZED:
	case XR_SESSION_STATE_VISIBLE:
		xr.framestate.shouldRender = !!xr.session;
		break;
	default:
		break;
	}

	if (xr.framestate.shouldRender)
	{
		XrTime time;
		res = xrWaitFrame(xr.session, NULL, &xr.framestate);
		if (XR_FAILED(res))
		{
			Con_Printf("xrWaitFrame: %i\n", res);
			return false;
		}
		time = xr.framestate.predictedDisplayTime;
		if (xr.timeknown)
		{
			if (time < xr.time)	//make sure time doesn't go backward...
				time = xr.time;
			*frametime = (time-xr.time)/1000000000.0;
		}
		xr.time = time;
		xr.timeknown = true;
	}

	XR_ProcessEvents();
	if (xr.session)
		XR_UpdateInputs(xr.framestate.predictedDisplayTime);

	return true;
}
static qboolean XR_Render(void(*rendereye)(texid_t tex, vec4_t fovoverride, vec3_t axisorg[4]))
{
	XrFrameEndInfo endframeinfo = {XR_TYPE_FRAME_END_INFO};
	unsigned int u;
	XrResult res;

	XrCompositionLayerProjection proj = {XR_TYPE_COMPOSITION_LAYER_PROJECTION};
	const XrCompositionLayerBaseHeader *projlist[] = {(XrCompositionLayerBaseHeader*)&proj};
	XrCompositionLayerProjectionView projviews[MAX_VIEW_COUNT];

	if (!xr.instance)
		return false;	//err... noooes!

	if (!xr.session)
	{
		if (!xr_enable->ival)
			return false;
		if (!XR_Begin())
		{	//something catasrophic went wrong. don't spam begins.
			XR_Shutdown();
		}
		return false;
	}

	if (!xr_enable->ival)
	{
		res = xrRequestExitSession(xr.session);
		if (XR_FAILED(res))
			Con_Printf("openxr: Unable to request session end: %i\n", res);

		XR_ProcessEvents();
	}

	switch(xr.state)
	{
	case XR_SESSION_STATE_EXITING:
		XR_SessionEnded();	//destroys the session but not the instance, so it can be started up again if desired.
		return false;
	case XR_SESSION_STATE_FOCUSED:
	case XR_SESSION_STATE_SYNCHRONIZED:
	case XR_SESSION_STATE_VISIBLE:
		break;
	default:
		return false;	//not ready.
	}

	res = xrBeginFrame(xr.session, NULL);
	if (XR_FAILED(res))
		Con_Printf("xrBeginFrame: %i\n", res);
	if (xr.framestate.shouldRender)
	{
		uint32_t eyecount;
		XrViewState viewstate = {XR_TYPE_VIEW_STATE};
		XrViewLocateInfo locateinfo = {XR_TYPE_VIEW_LOCATE_INFO};
		XrView eyeview[MAX_VIEW_COUNT]={};
		vec3_t transform[4];
		for (u = 0; u < MAX_VIEW_COUNT; u++)
			eyeview[u].type = XR_TYPE_VIEW;

		locateinfo.displayTime = xr.framestate.predictedDisplayTime;
		locateinfo.space = xr.space;
		res = xrLocateViews(xr.session, &locateinfo, &viewstate, xr.viewcount, &eyecount, eyeview);
		if (XR_FAILED(res))
			Con_Printf("xrLocateViews: %i\n", res);

		proj.layerFlags = 0;
		proj.space = xr.space;
		proj.views = projviews;
		endframeinfo.layerCount = 1;

		for (u = 0; u < xr.viewcount && u < eyecount; u++)
		{
			vec4_t fovoverride;
			XrSwapchainImageWaitInfo waitinfo = {XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO};
			unsigned int imgidx = 0;
			res = xrAcquireSwapchainImage(xr.eye[u].swapchain, NULL, &imgidx);
			if (XR_FAILED(res))
				Con_Printf("xrAcquireSwapchainImage: %i\n", res);

			memset(&projviews[u], 0, sizeof(projviews[u]));
			projviews[u].type = XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW;
			projviews[u].pose = eyeview[u].pose;
			projviews[u].fov = eyeview[u].fov;
			projviews[u].subImage = xr.eye[u].subimage;

			XR_PoseToTransform(&eyeview[u].pose, transform);
			fovoverride[0] = eyeview[u].fov.angleLeft * (180/M_PI);
			fovoverride[1] = eyeview[u].fov.angleRight * (180/M_PI);
			fovoverride[2] = eyeview[u].fov.angleDown * (180/M_PI);
			fovoverride[3] = eyeview[u].fov.angleUp * (180/M_PI);

			waitinfo.timeout = 100000;
			res = xrWaitSwapchainImage(xr.eye[u].swapchain, &waitinfo);
			if (XR_FAILED(res))
				Con_Printf("xrWaitSwapchainImage: %i\n", res);
			rendereye(&xr.eye[u].swapimages[imgidx], fovoverride, transform);
			//GL note: the OpenXR specification says NOTHING about the application having to glFlush or glFinish.
			//	I take this to mean that the openxr runtime is responsible for setting up barriers or w/e inside ReleaseSwapchainImage.
			//VK note: the OpenXR spec does say that it needs to be color_attachment_optimal+owned by queue. which it is.
			//	I take this to mean that the openxr runtime is responsible for barriers (as it'll need to transition it to general or shader-read anyway).
			res = xrReleaseSwapchainImage(xr.eye[u].swapchain, NULL);
			if (XR_FAILED(res))
				Con_Printf("xrReleaseSwapchainImage: %i\n", res);
		}
		proj.viewCount = u;
	}

	endframeinfo.layers = projlist;
	endframeinfo.displayTime = xr.framestate.predictedDisplayTime;
	endframeinfo.environmentBlendMode = XR_ENVIRONMENT_BLEND_MODE_OPAQUE;	//we don't do the alpha channel very well.
	res = xrEndFrame(xr.session, &endframeinfo);
	if (XR_FAILED(res))
	{
		Con_Printf("xrEndFrame: %i\n", res);
		if (res == XR_ERROR_SESSION_LOST || res == XR_ERROR_SESSION_NOT_RUNNING || res == XR_ERROR_SWAPCHAIN_RECT_INVALID)
			XR_SessionEnded();	//something sessiony
		else //if (res == XR_ERROR_INSTANCE_LOST)
			XR_Shutdown();	//don't really know what it was, just kill everything
	}

	return xr_skipregularview->ival;
}

static plugvrfuncs_t openxr =
{
	"OpenXR",
	XR_PreInit,
	XR_Init,
	XR_SyncFrame,
	XR_Render,
	XR_Shutdown,
};

qboolean Plug_Init(void)
{
	fsfuncs = plugfuncs->GetEngineInterface(plugfsfuncs_name, sizeof(*fsfuncs));
	plugfuncs->ExportFunction("MayUnload", XR_PluginMayUnload);
	if (plugfuncs->ExportInterface(plugvrfuncs_name, &openxr, sizeof(openxr)))
	{
		xr_enable			= cvarfuncs->GetNVFDG("xr_enable",			"1",			0,				"Controls whether to use openxr rendering or not.",									"OpenXR configuration");
		xr_formfactor		= cvarfuncs->GetNVFDG("xr_formfactor",		"head",			CVAR_ARCHIVE,	"Controls which VR system to try to use. Valid options are head, or hand",			"OpenXR configuration");
		xr_viewconfig		= cvarfuncs->GetNVFDG("xr_viewconfig",		"",				CVAR_ARCHIVE,	"Controls the type of view we aim for. Valid options are mono, stereo, or quad",	"OpenXR configuration");
		xr_metresize		= cvarfuncs->GetNVFDG("xr_metresize",		"26.24671916",	CVAR_ARCHIVE,	"Size of a metre in game units",													"OpenXR configuration");
		xr_skipregularview	= cvarfuncs->GetNVFDG("xr_skipregularview", "0",			CVAR_ARCHIVE,	"Skip rendering the regular view when OpenXR is active.",							"OpenXR configuration");
		return true;
	}
	return false;
}