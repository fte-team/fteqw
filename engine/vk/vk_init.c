#include "quakedef.h"
#ifdef VKQUAKE
#include "vkrenderer.h"
#include "gl_draw.h"
#include "shader.h"
#include "renderque.h"	//is anything still using this?

extern qboolean vid_isfullscreen;
extern cvar_t vk_submissionthread;
extern cvar_t vk_debug;
extern cvar_t vk_dualqueue;
extern cvar_t vk_busywait;
extern cvar_t vk_nv_glsl_shader;
extern cvar_t vk_nv_dedicated_allocation;
extern cvar_t vk_khr_dedicated_allocation;
extern cvar_t vk_khr_push_descriptor;
extern cvar_t vid_srgb, vid_vsync, vid_triplebuffer, r_stereo_method, vid_multisample;
void R2D_Console_Resize(void);

const char *vklayerlist[] =
{
#if 1
	"VK_LAYER_LUNARG_standard_validation"
#else
		//older versions of the sdk were crashing out on me,
//	"VK_LAYER_LUNARG_api_dump",
"VK_LAYER_LUNARG_device_limits",
//"VK_LAYER_LUNARG_draw_state",
"VK_LAYER_LUNARG_image",
//"VK_LAYER_LUNARG_mem_tracker",
"VK_LAYER_LUNARG_object_tracker",
"VK_LAYER_LUNARG_param_checker",
"VK_LAYER_LUNARG_screenshot",
"VK_LAYER_LUNARG_swapchain",
"VK_LAYER_GOOGLE_threading",
"VK_LAYER_GOOGLE_unique_objects",
//"VK_LAYER_LUNARG_vktrace",
#endif
};
#define vklayercount (vk_debug.ival>1?countof(vklayerlist):0)


//code to initialise+destroy vulkan contexts.
//this entire file is meant to be platform-agnostic.
//the vid code still needs to set up vkGetInstanceProcAddr, and do all the window+input stuff.

#ifdef VK_NO_PROTOTYPES
	#define VKFunc(n) PFN_vk##n vk##n;
	VKFunc(CreateDebugReportCallbackEXT)
	VKFunc(DestroyDebugReportCallbackEXT)
	VKFuncs
	#undef VKFunc
#endif

void VK_Submit_Work(VkCommandBuffer cmdbuf, VkSemaphore semwait, VkPipelineStageFlags semwaitstagemask, VkSemaphore semsignal, VkFence fencesignal, struct vkframe *presentframe, struct vk_fencework *fencedwork);
static int VK_Submit_Thread(void *arg);
static void VK_Submit_DoWork(void);

static void VK_DestroyRenderPass(void);
static void VK_CreateRenderPass(void);
static void VK_Shutdown_PostProc(void);
		
struct vulkaninfo_s vk;
static struct vk_rendertarg postproc[4];
static unsigned int postproc_buf;
static struct vk_rendertarg_cube vk_rt_cubemap;

qboolean VK_SCR_GrabBackBuffer(void);


static VkDebugReportCallbackEXT vk_debugcallback;
static VkBool32 VKAPI_PTR mydebugreportcallback(
				VkDebugReportFlagsEXT                       flags,
				VkDebugReportObjectTypeEXT                  objectType,
				uint64_t                                    object,
				size_t                                      location,
				int32_t                                     messageCode,
				const char*                                 pLayerPrefix,
				const char*                                 pMessage,
				void*                                       pUserData)
{
	if (flags & VK_DEBUG_REPORT_ERROR_BIT_EXT)
		Con_Printf("%s: %s\n", pLayerPrefix, pMessage);
	else if (flags & VK_DEBUG_REPORT_WARNING_BIT_EXT)
	{
		if (!strncmp(pMessage, "Additional bits in Source accessMask", 36) && strstr(pMessage, "VK_IMAGE_LAYOUT_UNDEFINED"))
			return false;	//I don't give a fuck. undefined can be used to change layouts on a texture that already exists too.
		Con_Printf("%s: %s\n", pLayerPrefix, pMessage);
	}
	else if (flags & VK_DEBUG_REPORT_DEBUG_BIT_EXT)
		Con_Printf("%s: %s\n", pLayerPrefix, pMessage);
	else if (flags & VK_DEBUG_REPORT_INFORMATION_BIT_EXT)
	{
#ifdef _WIN32
	//	OutputDebugString(va("%s\n", pMessage));
#endif
//		Con_Printf("%s: %s\n", pLayerPrefix, pMessage);
	}
	else if (flags & VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT)
		Con_Printf("%s: %s\n", pLayerPrefix, pMessage);    
	else
		Con_Printf("%s: %s\n", pLayerPrefix, pMessage);
	return false;
}

//typeBits is some vulkan requirement thing (like textures must be device-local).
//requirements_mask are things that the engine may require (like host-visible).
//note that there is absolutely no guarentee that hardware requirements will match what the host needs.
//thus you may need to use staging.
uint32_t vk_find_memory_try(uint32_t typeBits, VkFlags requirements_mask)
{
	uint32_t i;
	for (i = 0; i < 32; i++)
	{
		if ((typeBits & 1) == 1)
		{
			if ((vk.memory_properties.memoryTypes[i].propertyFlags & requirements_mask) == requirements_mask)
				return i;
		}
		typeBits >>= 1;
	}
	return ~0u;
}
uint32_t vk_find_memory_require(uint32_t typeBits, VkFlags requirements_mask)
{
	uint32_t ret = vk_find_memory_try(typeBits, requirements_mask);
	if (ret == ~0)
		Sys_Error("Unable to find suitable vulkan memory pool\n");
	return ret;
}

void VK_DestroyVkTexture(vk_image_t *img)
{
	if (!img)
		return;
	if (img->sampler)
		vkDestroySampler(vk.device, img->sampler, vkallocationcb);
	if (img->view)
		vkDestroyImageView(vk.device, img->view, vkallocationcb);
	if (img->image)
		vkDestroyImage(vk.device, img->image, vkallocationcb);
	if (img->memory)
		vkFreeMemory(vk.device, img->memory, vkallocationcb);
}

static void VK_DestroySwapChain(void)
{
	uint32_t i;

	if (vk.submitcondition)
	{
		Sys_LockConditional(vk.submitcondition);
		vk.neednewswapchain = true;
		Sys_ConditionSignal(vk.submitcondition);
		Sys_UnlockConditional(vk.submitcondition);
	}
	if (vk.submitthread)
	{
		Sys_WaitOnThread(vk.submitthread);
		vk.submitthread = NULL;
	}
	while (vk.work)
	{
		Sys_LockConditional(vk.submitcondition);
		VK_Submit_DoWork();
		Sys_UnlockConditional(vk.submitcondition);
	}
	if (vk.dopresent)
		vk.dopresent(NULL);
	if (vk.device)
		vkDeviceWaitIdle(vk.device);
	/*while (vk.aquirenext < vk.aquirelast)
	{
		VkWarnAssert(vkWaitForFences(vk.device, 1, &vk.acquirefences[vk.aquirenext%ACQUIRELIMIT], VK_FALSE, UINT64_MAX));
		vk.aquirenext++;
	}*/
	VK_FencedCheck();
	while(vk.frameendjobs)
	{	//we've fully synced the gpu now, we can clean up any resources that were pending but not assigned yet.
		struct vk_frameend *job = vk.frameendjobs;
		vk.frameendjobs = job->next;
		job->FrameEnded(job+1);
		Z_Free(job);
	}

	if (vk.frame)
	{
		vk.frame->next = vk.unusedframes;
		vk.unusedframes = vk.frame;
		vk.frame = NULL;
	}

	for (i = 0; i < vk.backbuf_count; i++)
	{
		//swapchain stuff
		if (vk.backbufs[i].framebuffer)
			vkDestroyFramebuffer(vk.device, vk.backbufs[i].framebuffer, vkallocationcb);
		vk.backbufs[i].framebuffer = VK_NULL_HANDLE;
		if (vk.backbufs[i].colour.view)
			vkDestroyImageView(vk.device, vk.backbufs[i].colour.view, vkallocationcb);
		vk.backbufs[i].colour.view = VK_NULL_HANDLE;
		VK_DestroyVkTexture(&vk.backbufs[i].depth);
		VK_DestroyVkTexture(&vk.backbufs[i].mscolour);
	}

	if (vk.dopresent)
		vk.dopresent(NULL);
	while (vk.aquirenext < vk.aquirelast)
	{
		VkWarnAssert(vkWaitForFences(vk.device, 1, &vk.acquirefences[vk.aquirenext%ACQUIRELIMIT], VK_FALSE, UINT64_MAX));
		vk.aquirenext++;
	}
	if (vk.device)
		vkDeviceWaitIdle(vk.device);
	for (i = 0; i < ACQUIRELIMIT; i++)
	{
		if (vk.acquirefences[i])
			vkDestroyFence(vk.device, vk.acquirefences[i], vkallocationcb);
		vk.acquirefences[i] = VK_NULL_HANDLE;
	}

	while(vk.unusedframes)
	{
		struct vkframe *frame = vk.unusedframes;
		vk.unusedframes = frame->next;

		VKBE_ShutdownFramePools(frame);

		vkFreeCommandBuffers(vk.device, vk.cmdpool, frame->maxcbufs, frame->cbufs);
		BZ_Free(frame->cbufs);
		vkDestroyFence(vk.device, frame->finishedfence, vkallocationcb);
		Z_Free(frame);
	}

	if (vk.swapchain)
	{
		vkDestroySwapchainKHR(vk.device, vk.swapchain, vkallocationcb);
		vk.swapchain = VK_NULL_HANDLE;
	}

	if (vk.backbufs)
		free(vk.backbufs);
	vk.backbufs = NULL;
	vk.backbuf_count = 0;
}

static qboolean VK_CreateSwapChain(void)
{
	qboolean reloadshaders = false;
	uint32_t fmtcount;
	VkSurfaceFormatKHR *surffmts;
	uint32_t presentmodes;
	VkPresentModeKHR *presentmode;
	VkSurfaceCapabilitiesKHR surfcaps;
	VkSwapchainCreateInfoKHR swapinfo = {VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR};
	uint32_t i, curpri;
	VkSwapchainKHR newvkswapchain;
	VkImage *images;
	VkDeviceMemory *memories;
	VkImageView attachments[3];
	VkFramebufferCreateInfo fb_info = {VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
	VkSampleCountFlagBits oldms;

	vk.dopresent(NULL);	//make sure they're all pushed through.


	vid_vsync.modified = false;
	vid_triplebuffer.modified = false;
	vid_srgb.modified = false;
	vk_submissionthread.modified = false;
	vid_multisample.modified = false;

	vk.triplebuffer = vid_triplebuffer.ival;
	vk.vsync = vid_vsync.ival;

	if (!vk.khr_swapchain)
	{	//headless
		if (vk.swapchain || vk.backbuf_count)
			VK_DestroySwapChain();

		vk.backbufformat = (vid.srgb||vid_srgb.ival)?VK_FORMAT_B8G8R8A8_SRGB:VK_FORMAT_B8G8R8A8_UNORM;
		vk.backbuf_count = 4;

		swapinfo.imageExtent.width = vid.pixelwidth;
		swapinfo.imageExtent.height = vid.pixelheight;

		images = malloc(sizeof(VkImage)*vk.backbuf_count);
		memset(images, 0, sizeof(VkImage)*vk.backbuf_count);
		memories = malloc(sizeof(VkDeviceMemory)*vk.backbuf_count);
		memset(memories, 0, sizeof(VkDeviceMemory)*vk.backbuf_count);

		vk.aquirelast = vk.aquirenext = 0;
		for (i = 0; i < ACQUIRELIMIT; i++)
		{
			VkFenceCreateInfo fci = {VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
			fci.flags = VK_FENCE_CREATE_SIGNALED_BIT;
			VkAssert(vkCreateFence(vk.device,&fci,vkallocationcb,&vk.acquirefences[i]));

			vk.acquirebufferidx[vk.aquirelast%ACQUIRELIMIT] = vk.aquirelast%vk.backbuf_count;
			vk.aquirelast++;
		}

		for (i = 0; i < vk.backbuf_count; i++)
		{
			VkMemoryRequirements mem_reqs;
			VkMemoryAllocateInfo memAllocInfo = {VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
			VkDedicatedAllocationMemoryAllocateInfoNV nv_damai = {VK_STRUCTURE_TYPE_DEDICATED_ALLOCATION_MEMORY_ALLOCATE_INFO_NV};
			VkImageCreateInfo ici = {VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
			VkDedicatedAllocationImageCreateInfoNV nv_daici = {VK_STRUCTURE_TYPE_DEDICATED_ALLOCATION_IMAGE_CREATE_INFO_NV};

			ici.flags = 0;
			ici.imageType = VK_IMAGE_TYPE_2D;
			ici.format = vk.backbufformat;
			ici.extent.width = vid.pixelwidth;
			ici.extent.height = vid.pixelheight;
			ici.extent.depth = 1;
			ici.mipLevels = 1;
			ici.arrayLayers = 1;
			ici.samples = VK_SAMPLE_COUNT_1_BIT;
			ici.tiling = VK_IMAGE_TILING_OPTIMAL;
			ici.usage = VK_IMAGE_USAGE_SAMPLED_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT|VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
			ici.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
			ici.queueFamilyIndexCount = 0;
			ici.pQueueFamilyIndices = NULL;
			ici.initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

			if (vk.nv_dedicated_allocation)
			{	//render targets should always have dedicated allocations, supposedly. and we're doing it anyway.
				nv_daici.dedicatedAllocation = true;
				nv_daici.pNext = ici.pNext;
				ici.pNext = &nv_daici;
			}

			VkAssert(vkCreateImage(vk.device, &ici, vkallocationcb, &images[i]));

			vkGetImageMemoryRequirements(vk.device, images[i], &mem_reqs);

			memAllocInfo.allocationSize = mem_reqs.size;
			memAllocInfo.memoryTypeIndex = vk_find_memory_try(mem_reqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
			if (memAllocInfo.memoryTypeIndex == ~0)
				memAllocInfo.memoryTypeIndex = vk_find_memory_try(mem_reqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
			if (memAllocInfo.memoryTypeIndex == ~0)
				memAllocInfo.memoryTypeIndex = vk_find_memory_try(mem_reqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
			if (memAllocInfo.memoryTypeIndex == ~0)
				memAllocInfo.memoryTypeIndex = vk_find_memory_require(mem_reqs.memoryTypeBits, 0);

			if (vk.nv_dedicated_allocation)
			{	//annoying, but hey.
				nv_damai.pNext = memAllocInfo.pNext;
				nv_damai.image = images[i];
				memAllocInfo.pNext = &nv_damai;
			}

			VkAssert(vkAllocateMemory(vk.device, &memAllocInfo, vkallocationcb, &memories[i]));
			VkAssert(vkBindImageMemory(vk.device, images[i], memories[i], 0));
		}
	}
	else
	{	//using vulkan's presentation engine.
		VkAssert(vkGetPhysicalDeviceSurfaceFormatsKHR(vk.gpu, vk.surface, &fmtcount, NULL));
		surffmts = malloc(sizeof(VkSurfaceFormatKHR)*fmtcount);
		VkAssert(vkGetPhysicalDeviceSurfaceFormatsKHR(vk.gpu, vk.surface, &fmtcount, surffmts));

		VkAssert(vkGetPhysicalDeviceSurfacePresentModesKHR(vk.gpu, vk.surface, &presentmodes, NULL));
		presentmode = malloc(sizeof(VkPresentModeKHR)*presentmodes);
		VkAssert(vkGetPhysicalDeviceSurfacePresentModesKHR(vk.gpu, vk.surface, &presentmodes, presentmode));

		vkGetPhysicalDeviceSurfaceCapabilitiesKHR(vk.gpu, vk.surface, &surfcaps);

		swapinfo.surface = vk.surface;
		swapinfo.minImageCount = surfcaps.minImageCount+vk.triplebuffer;
		if (swapinfo.minImageCount > surfcaps.maxImageCount)
			swapinfo.minImageCount = surfcaps.maxImageCount;
		if (swapinfo.minImageCount < surfcaps.minImageCount)
			swapinfo.minImageCount = surfcaps.minImageCount;
		swapinfo.imageExtent.width = surfcaps.currentExtent.width;
		swapinfo.imageExtent.height = surfcaps.currentExtent.height;
		swapinfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
		swapinfo.preTransform = surfcaps.currentTransform;//VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
		if (surfcaps.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR)
			swapinfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
		else if (surfcaps.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR)
			swapinfo.compositeAlpha = VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR;
		else if (surfcaps.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR)
			swapinfo.compositeAlpha = VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR;
		else
			swapinfo.compositeAlpha = VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR;	//erk?
		swapinfo.imageArrayLayers = /*(r_stereo_method.ival==1)?2:*/1;
		swapinfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
		swapinfo.queueFamilyIndexCount = 0;
		swapinfo.pQueueFamilyIndices = NULL;
		swapinfo.oldSwapchain = vk.swapchain;
		swapinfo.clipped = vid_isfullscreen?VK_FALSE:VK_TRUE;	//allow fragment shaders to be skipped on parts that are obscured by another window. screenshots might get weird, so use proper captures if required/automagic.

		swapinfo.presentMode = VK_PRESENT_MODE_FIFO_KHR;	//supposed to be guarenteed support.
		for (i = 0, curpri = 0; i < presentmodes; i++)
		{
			uint32_t priority = 0;
			switch(presentmode[i])
			{
			default://ignore it.
				break;
			case VK_PRESENT_MODE_IMMEDIATE_KHR:
				priority = (vk.vsync?0:2) + 2;	//for most quake players, latency trumps tearing.
				break;
			case VK_PRESENT_MODE_MAILBOX_KHR:
				priority = (vk.vsync?0:2) + 1;
				break;
			case VK_PRESENT_MODE_FIFO_KHR:
				priority = (vk.vsync?2:0) + 1;
				break;
			case VK_PRESENT_MODE_FIFO_RELAXED_KHR:
				priority = (vk.vsync?2:0) + 2;	//strict vsync results in weird juddering if rtlights etc caues framerates to drop below the refreshrate
				break;
			}
			if (priority > curpri)
			{
				curpri = priority;
				swapinfo.presentMode = presentmode[i];
			}
		}

		vk.srgbcapable = false;
		swapinfo.imageColorSpace = VK_COLORSPACE_SRGB_NONLINEAR_KHR;
		swapinfo.imageFormat = (vid.srgb||vid_srgb.ival)?VK_FORMAT_B8G8R8A8_SRGB:VK_FORMAT_B8G8R8A8_UNORM;
		for (i = 0, curpri = 0; i < fmtcount; i++)
		{
			uint32_t priority = 0;
			switch(surffmts[i].format)
			{
			case VK_FORMAT_B8G8R8A8_UNORM:
			case VK_FORMAT_R8G8B8A8_UNORM:
				priority = 4+!(vid.srgb||vid_srgb.ival);
				break;
			case VK_FORMAT_B8G8R8A8_SRGB:
			case VK_FORMAT_R8G8B8A8_SRGB:
				priority = 4+!!(vid.srgb||vid_srgb.ival);
				vk.srgbcapable = true;
				break;
			case VK_FORMAT_R16G16B16A16_SFLOAT:	//16bit per-channel formats
			case VK_FORMAT_R16G16B16A16_SNORM:
				priority = 3;
				break;
			case VK_FORMAT_R32G32B32A32_SFLOAT:	//32bit per-channel formats
				priority = 2;
				break;
			default:	//16 bit formats (565).
				priority = 1;
				break;
			}
			if (priority > curpri)
			{
				curpri = priority;
				swapinfo.imageColorSpace = surffmts[i].colorSpace;
				swapinfo.imageFormat = surffmts[i].format;
			}
		}

		if (vk.backbufformat != swapinfo.imageFormat)
		{
			VK_DestroyRenderPass();
			reloadshaders = true;
		}
		vk.backbufformat = swapinfo.imageFormat;

		free(presentmode);
		free(surffmts);

		VkAssert(vkCreateSwapchainKHR(vk.device, &swapinfo, vkallocationcb, &newvkswapchain));
		if (!newvkswapchain)
			return false;
		if (vk.swapchain)
		{
			VK_DestroySwapChain();
		}
		vk.swapchain = newvkswapchain;

		VkAssert(vkGetSwapchainImagesKHR(vk.device, vk.swapchain, &vk.backbuf_count, NULL));
		images = malloc(sizeof(VkImage)*vk.backbuf_count);
		memories = NULL;
		VkAssert(vkGetSwapchainImagesKHR(vk.device, vk.swapchain, &vk.backbuf_count, images));

		vk.aquirelast = vk.aquirenext = 0;
		for (i = 0; i < ACQUIRELIMIT; i++)
		{
			VkFenceCreateInfo fci = {VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
			VkAssert(vkCreateFence(vk.device,&fci,vkallocationcb,&vk.acquirefences[i]));
		}
		/*-1 to hide any weird thread issues*/
		while (vk.aquirelast < ACQUIRELIMIT-1 && vk.aquirelast < vk.backbuf_count && vk.aquirelast <= vk.backbuf_count-surfcaps.minImageCount)
		{
			VkAssert(vkAcquireNextImageKHR(vk.device, vk.swapchain, UINT64_MAX, VK_NULL_HANDLE, vk.acquirefences[vk.aquirelast%ACQUIRELIMIT], &vk.acquirebufferidx[vk.aquirelast%ACQUIRELIMIT]));
			vk.aquirelast++;
		}
	}

	oldms = vk.multisamplebits;

	vk.multisamplebits = VK_SAMPLE_COUNT_1_BIT;
#ifdef _DEBUG
	if (vid_multisample.ival>1)
	{
		VkSampleCountFlags fl = vk.limits.framebufferColorSampleCounts & vk.limits.framebufferDepthSampleCounts;
		Con_Printf("Warning: vulkan multisample does not work with rtlights or render targets etc etc\n");
		for (i = 1; i < 30; i++)
			if ((fl & (1<<i)) && (1<<i) <= vid_multisample.ival)
				vk.multisamplebits = (1<<i);
	}
#endif

	if (oldms != vk.multisamplebits)
	{
		VK_DestroyRenderPass();
		reloadshaders = true;
	}

	VK_CreateRenderPass();
	if (reloadshaders)
	{
		Shader_NeedReload(true);
		Shader_DoReload();
	}

	attachments[0] = VK_NULL_HANDLE;	//colour
	attachments[1] = VK_NULL_HANDLE;	//depth
	attachments[2] = VK_NULL_HANDLE;	//mscolour

	fb_info.renderPass = vk.renderpass[0];
	if (vk.multisamplebits != VK_SAMPLE_COUNT_1_BIT)
		fb_info.attachmentCount = 3;
	else
		fb_info.attachmentCount = 2;
	fb_info.pAttachments = attachments;
	fb_info.width = swapinfo.imageExtent.width;
	fb_info.height = swapinfo.imageExtent.height;
	fb_info.layers = 1;


	vk.backbufs = malloc(sizeof(*vk.backbufs)*vk.backbuf_count);
	memset(vk.backbufs, 0, sizeof(*vk.backbufs)*vk.backbuf_count);
	for (i = 0; i < vk.backbuf_count; i++)
	{
		VkImageViewCreateInfo ivci = {VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
		ivci.format = vk.backbufformat;
		ivci.components.r = VK_COMPONENT_SWIZZLE_R;
		ivci.components.g = VK_COMPONENT_SWIZZLE_G;
		ivci.components.b = VK_COMPONENT_SWIZZLE_B;
		ivci.components.a = VK_COMPONENT_SWIZZLE_A;
		ivci.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		ivci.subresourceRange.baseMipLevel = 0;
		ivci.subresourceRange.levelCount = 1;
		ivci.subresourceRange.baseArrayLayer = 0;
		ivci.subresourceRange.layerCount = 1;
		ivci.viewType = VK_IMAGE_VIEW_TYPE_2D;
		ivci.flags = 0;
		ivci.image = images[i];
		vk.backbufs[i].colour.image = images[i];
		if (memories)
			vk.backbufs[i].colour.memory = memories[i];
		vk.backbufs[i].colour.width = swapinfo.imageExtent.width;
		vk.backbufs[i].colour.height = swapinfo.imageExtent.height;
		VkAssert(vkCreateImageView(vk.device, &ivci, vkallocationcb, &vk.backbufs[i].colour.view));

		vk.backbufs[i].firstuse = true;

		//create the depth buffer texture. possibly multisampled.
		{
			//depth image
			{
				VkImageCreateInfo depthinfo = {VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
				VkDedicatedAllocationImageCreateInfoNV nv_daici = {VK_STRUCTURE_TYPE_DEDICATED_ALLOCATION_IMAGE_CREATE_INFO_NV};
				depthinfo.flags = 0;
				depthinfo.imageType = VK_IMAGE_TYPE_2D;
				depthinfo.format = vk.depthformat;
				depthinfo.extent.width = swapinfo.imageExtent.width;
				depthinfo.extent.height = swapinfo.imageExtent.height;
				depthinfo.extent.depth = 1;
				depthinfo.mipLevels = 1;
				depthinfo.arrayLayers = 1;
				depthinfo.samples = vk.multisamplebits;
				depthinfo.tiling = VK_IMAGE_TILING_OPTIMAL;
				depthinfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
				depthinfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
				depthinfo.queueFamilyIndexCount = 0;
				depthinfo.pQueueFamilyIndices = NULL;
				depthinfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
				if (vk.nv_dedicated_allocation)
				{
					nv_daici.dedicatedAllocation = true;
					nv_daici.pNext = depthinfo.pNext;
					depthinfo.pNext = &nv_daici;
				}
				VkAssert(vkCreateImage(vk.device, &depthinfo, vkallocationcb, &vk.backbufs[i].depth.image));
			}

			//depth memory
			{
				VkMemoryRequirements mem_reqs;
				VkMemoryAllocateInfo memAllocInfo = {VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
				VkDedicatedAllocationMemoryAllocateInfoNV nv_damai = {VK_STRUCTURE_TYPE_DEDICATED_ALLOCATION_MEMORY_ALLOCATE_INFO_NV};
				vkGetImageMemoryRequirements(vk.device, vk.backbufs[i].depth.image, &mem_reqs);
				memAllocInfo.allocationSize = mem_reqs.size;
				memAllocInfo.memoryTypeIndex = vk_find_memory_require(mem_reqs.memoryTypeBits, 0);
				if (vk.nv_dedicated_allocation)
				{
					nv_damai.image = vk.backbufs[i].depth.image;
					nv_damai.pNext = memAllocInfo.pNext;
					memAllocInfo.pNext = &nv_damai;
				}
				VkAssert(vkAllocateMemory(vk.device, &memAllocInfo, vkallocationcb, &vk.backbufs[i].depth.memory));
				VkAssert(vkBindImageMemory(vk.device, vk.backbufs[i].depth.image, vk.backbufs[i].depth.memory, 0));
			}

			//depth view
			{
				VkImageViewCreateInfo depthviewinfo = {VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
				depthviewinfo.format = vk.depthformat;
				depthviewinfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
				depthviewinfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
				depthviewinfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
				depthviewinfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
				depthviewinfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;//|VK_IMAGE_ASPECT_STENCIL_BIT;
				depthviewinfo.subresourceRange.baseMipLevel = 0;
				depthviewinfo.subresourceRange.levelCount = 1;
				depthviewinfo.subresourceRange.baseArrayLayer = 0;
				depthviewinfo.subresourceRange.layerCount = 1;
				depthviewinfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
				depthviewinfo.flags = 0;
				depthviewinfo.image = vk.backbufs[i].depth.image;
				VkAssert(vkCreateImageView(vk.device, &depthviewinfo, vkallocationcb, &vk.backbufs[i].depth.view));
				attachments[1] = vk.backbufs[i].depth.view;
			}
		}

		//if we're using multisampling, create the intermediate multisample texture that we're actually going to render to.
		if (vk.multisamplebits != VK_SAMPLE_COUNT_1_BIT)
		{
			//mscolour image
			{
				VkImageCreateInfo mscolourinfo = {VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
				VkDedicatedAllocationImageCreateInfoNV nv_daici = {VK_STRUCTURE_TYPE_DEDICATED_ALLOCATION_IMAGE_CREATE_INFO_NV};
				mscolourinfo.flags = 0;
				mscolourinfo.imageType = VK_IMAGE_TYPE_2D;
				mscolourinfo.format = vk.backbufformat;
				mscolourinfo.extent.width = swapinfo.imageExtent.width;
				mscolourinfo.extent.height = swapinfo.imageExtent.height;
				mscolourinfo.extent.depth = 1;
				mscolourinfo.mipLevels = 1;
				mscolourinfo.arrayLayers = 1;
				mscolourinfo.samples = vk.multisamplebits;
				mscolourinfo.tiling = VK_IMAGE_TILING_OPTIMAL;
				mscolourinfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
				mscolourinfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
				mscolourinfo.queueFamilyIndexCount = 0;
				mscolourinfo.pQueueFamilyIndices = NULL;
				mscolourinfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
				if (vk.nv_dedicated_allocation)
				{
					nv_daici.dedicatedAllocation = true;
					nv_daici.pNext = mscolourinfo.pNext;
					mscolourinfo.pNext = &nv_daici;
				}
				VkAssert(vkCreateImage(vk.device, &mscolourinfo, vkallocationcb, &vk.backbufs[i].mscolour.image));
			}

			//mscolour memory
			{
				VkMemoryRequirements mem_reqs;
				VkMemoryAllocateInfo memAllocInfo = {VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
				VkDedicatedAllocationMemoryAllocateInfoNV nv_damai = {VK_STRUCTURE_TYPE_DEDICATED_ALLOCATION_MEMORY_ALLOCATE_INFO_NV};
				vkGetImageMemoryRequirements(vk.device, vk.backbufs[i].mscolour.image, &mem_reqs);
				memAllocInfo.allocationSize = mem_reqs.size;
				memAllocInfo.memoryTypeIndex = vk_find_memory_require(mem_reqs.memoryTypeBits, 0);
				if (vk.nv_dedicated_allocation)
				{
					nv_damai.image = vk.backbufs[i].mscolour.image;
					nv_damai.pNext = memAllocInfo.pNext;
					memAllocInfo.pNext = &nv_damai;
				}
				VkAssert(vkAllocateMemory(vk.device, &memAllocInfo, vkallocationcb, &vk.backbufs[i].mscolour.memory));
				VkAssert(vkBindImageMemory(vk.device, vk.backbufs[i].mscolour.image, vk.backbufs[i].mscolour.memory, 0));
			}

			//mscolour view
			{
				VkImageViewCreateInfo mscolourviewinfo = {VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
				mscolourviewinfo.format = vk.backbufformat;
				mscolourviewinfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
				mscolourviewinfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
				mscolourviewinfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
				mscolourviewinfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
				mscolourviewinfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
				mscolourviewinfo.subresourceRange.baseMipLevel = 0;
				mscolourviewinfo.subresourceRange.levelCount = 1;
				mscolourviewinfo.subresourceRange.baseArrayLayer = 0;
				mscolourviewinfo.subresourceRange.layerCount = 1;
				mscolourviewinfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
				mscolourviewinfo.flags = 0;
				mscolourviewinfo.image = vk.backbufs[i].mscolour.image;
				VkAssert(vkCreateImageView(vk.device, &mscolourviewinfo, vkallocationcb, &vk.backbufs[i].mscolour.view));
				attachments[2] = vk.backbufs[i].mscolour.view;
			}
		}


		attachments[0] = vk.backbufs[i].colour.view;
		VkAssert(vkCreateFramebuffer(vk.device, &fb_info, vkallocationcb, &vk.backbufs[i].framebuffer));

		{
			VkSemaphoreCreateInfo seminfo = {VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
			VkAssert(vkCreateSemaphore(vk.device, &seminfo, vkallocationcb, &vk.backbufs[i].presentsemaphore));
		}
	}
	free(images);
	free(memories);

	vid.pixelwidth = swapinfo.imageExtent.width;
	vid.pixelheight = swapinfo.imageExtent.height;
	R2D_Console_Resize();
	return true;
}

	
void	VK_Draw_Init(void)
{
	qboolean srgb = vid_srgb.ival > 1 && vk.srgbcapable;
	if (vid.srgb != srgb)
		vid_srgb.modified = true;
	vid.srgb = srgb;

	R2D_Init();
}
void	VK_Draw_Shutdown(void)
{
	R2D_Shutdown();
	Image_Shutdown();
	Shader_Shutdown();
}

void VK_CreateSampler(unsigned int flags, vk_image_t *img)
{
	qboolean clamptoedge = flags & IF_CLAMP;
	VkSamplerCreateInfo lmsampinfo = {VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};

	if (img->sampler)
		vkDestroySampler(vk.device, img->sampler, vkallocationcb);

	if (flags & IF_LINEAR)
	{
		lmsampinfo.minFilter = lmsampinfo.magFilter = VK_FILTER_LINEAR;
		lmsampinfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
	}
	else if (flags & IF_NEAREST)
	{
		lmsampinfo.minFilter = lmsampinfo.magFilter = VK_FILTER_NEAREST;
		lmsampinfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
	}
	else
	{
		int *filter = (flags & IF_UIPIC)?vk.filterpic:vk.filtermip;
		if (filter[0])
			lmsampinfo.minFilter = VK_FILTER_LINEAR;
		else
			lmsampinfo.minFilter = VK_FILTER_NEAREST;
		if (filter[1])
			lmsampinfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
		else
			lmsampinfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
		if (filter[2])
			lmsampinfo.magFilter = VK_FILTER_LINEAR;
		else
			lmsampinfo.magFilter = VK_FILTER_NEAREST;
	}

	lmsampinfo.addressModeU = clamptoedge?VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE:VK_SAMPLER_ADDRESS_MODE_REPEAT;
	lmsampinfo.addressModeV = clamptoedge?VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE:VK_SAMPLER_ADDRESS_MODE_REPEAT;
	lmsampinfo.addressModeW = clamptoedge?VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE:VK_SAMPLER_ADDRESS_MODE_REPEAT;
	lmsampinfo.mipLodBias = 0.0;
	lmsampinfo.anisotropyEnable = (flags & IF_NEAREST)?false:(vk.max_anistophy > 1);
	lmsampinfo.maxAnisotropy = vk.max_anistophy;
	lmsampinfo.compareEnable = VK_FALSE;
	lmsampinfo.compareOp = VK_COMPARE_OP_NEVER;
	lmsampinfo.minLod = vk.mipcap[0];	//this isn't quite right
	lmsampinfo.maxLod = vk.mipcap[1];
	lmsampinfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK;
	lmsampinfo.unnormalizedCoordinates = VK_FALSE;
	VkAssert(vkCreateSampler(vk.device, &lmsampinfo, NULL, &img->sampler));
}

static void VK_DestroySampler(struct vk_frameend *w)
{
	VkSampler s = *(VkSampler*)w;
	vkDestroySampler(vk.device, s, vkallocationcb);
}
void VK_UpdateFiltering(image_t *imagelist, int filtermip[3], int filterpic[3], int mipcap[2], float anis)
{
	uint32_t i;
	for (i = 0; i < countof(vk.filtermip); i++)
		vk.filtermip[i] = filtermip[i];
	for (i = 0; i < countof(vk.filterpic); i++)
		vk.filterpic[i] = filterpic[i];
	for (i = 0; i < countof(vk.mipcap); i++)
		vk.mipcap[i] = mipcap[i];
	vk.max_anistophy = bound(1.0, anis, vk.max_anistophy_limit);

	while(imagelist)
	{
		if (imagelist->vkimage)
		{
			if (imagelist->vkimage->sampler)
			{	//the sampler might still be in use, so clean it up at the end of the frame.
				//all this to avoid syncing all the queues...
				VK_AtFrameEnd(VK_DestroySampler, &imagelist->vkimage->sampler, sizeof(imagelist->vkimage->sampler));
				imagelist->vkimage->sampler = VK_NULL_HANDLE;
			}
			VK_CreateSampler(imagelist->flags, imagelist->vkimage);
		}
		imagelist = imagelist->next;
	}
}

vk_image_t VK_CreateTexture2DArray(uint32_t width, uint32_t height, uint32_t layers, uint32_t mips, unsigned int encoding, unsigned int type, qboolean rendertarget)
{
	vk_image_t ret;
#ifdef USE_STAGING_BUFFERS
	qboolean staging = layers == 0;
#else
	const qboolean staging = false;
#endif
	VkMemoryRequirements mem_reqs;
	VkMemoryAllocateInfo memAllocInfo = {VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
	VkDedicatedAllocationMemoryAllocateInfoNV nv_damai = {VK_STRUCTURE_TYPE_DEDICATED_ALLOCATION_MEMORY_ALLOCATE_INFO_NV};

	VkImageCreateInfo ici = {VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
	VkDedicatedAllocationImageCreateInfoNV nv_daici = {VK_STRUCTURE_TYPE_DEDICATED_ALLOCATION_IMAGE_CREATE_INFO_NV};
	VkFormat format;

	ret.width = width;
	ret.height = height;
	ret.layers = layers;
	ret.mipcount = mips;
	ret.encoding = encoding;
	ret.type = type;
	ret.layout = staging?VK_IMAGE_LAYOUT_PREINITIALIZED:VK_IMAGE_LAYOUT_UNDEFINED;

	//16bit formats.
	if (encoding == PTI_RGB565)
		format = VK_FORMAT_R5G6B5_UNORM_PACK16;
	else if (encoding == PTI_RGBA4444) 
		format = VK_FORMAT_R4G4B4A4_UNORM_PACK16;
	else if (encoding == PTI_ARGB4444) 
		format = VK_FORMAT_B4G4R4A4_UNORM_PACK16;	//fixme: this seems wrong.
	else if (encoding == PTI_RGBA5551) 
		format = VK_FORMAT_R5G5B5A1_UNORM_PACK16;
	else if (encoding == PTI_ARGB1555) 
		format = VK_FORMAT_A1R5G5B5_UNORM_PACK16;
	//float formats
	else if (encoding == PTI_RGBA16F) 
		format = VK_FORMAT_R16G16B16A16_SFLOAT;
	else if (encoding == PTI_RGBA32F) 
		format = VK_FORMAT_R32G32B32A32_SFLOAT;
	//weird formats
	else if (encoding == PTI_R8) 
		format = VK_FORMAT_R8_UNORM;
	else if (encoding == PTI_RG8)
		format = VK_FORMAT_R8G8_UNORM;
	//compressed formats
	else if (encoding == PTI_S3RGB1)
		format = VK_FORMAT_BC1_RGB_UNORM_BLOCK;
	else if (encoding == PTI_S3RGBA1)
		format = VK_FORMAT_BC1_RGBA_UNORM_BLOCK;
	else if (encoding == PTI_S3RGBA3)
		format = VK_FORMAT_BC2_UNORM_BLOCK;
	else if (encoding == PTI_S3RGBA5)
		format = VK_FORMAT_BC3_UNORM_BLOCK;
	//depth formats
	else if (encoding == PTI_DEPTH16)
		format = VK_FORMAT_D16_UNORM;
	else if (encoding == PTI_DEPTH24)
		format = VK_FORMAT_X8_D24_UNORM_PACK32;
	else if (encoding == PTI_DEPTH32)
		format = VK_FORMAT_D32_SFLOAT;
	else if (encoding == PTI_DEPTH24_8)
		format = VK_FORMAT_D24_UNORM_S8_UINT;
	//srgb formats
	else if (encoding == PTI_BGRA8_SRGB || encoding == PTI_BGRX8_SRGB)
		format = VK_FORMAT_B8G8R8A8_SRGB;
	else if (encoding == PTI_RGBA8_SRGB || encoding == PTI_RGBX8_SRGB)
		format = VK_FORMAT_R8G8B8A8_SRGB;
	//standard formats
	else if (encoding == PTI_BGRA8 || encoding == PTI_BGRX8)
		format = VK_FORMAT_B8G8R8A8_UNORM;
	else //if (encoding == PTI_RGBA8 || encoding == PTI_RGBX8)
		format = VK_FORMAT_R8G8B8A8_UNORM;
	ici.flags = (ret.type==PTI_CUBEMAP)?VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT:0;
	ici.imageType = VK_IMAGE_TYPE_2D;
	ici.format = format;
	ici.extent.width = width;
	ici.extent.height = height;
	ici.extent.depth = 1;
	ici.mipLevels = mips;
	ici.arrayLayers = staging?1:layers;
	ici.samples = VK_SAMPLE_COUNT_1_BIT;
	ici.tiling = staging?VK_IMAGE_TILING_LINEAR:VK_IMAGE_TILING_OPTIMAL;
	ici.usage = staging?VK_IMAGE_USAGE_TRANSFER_SRC_BIT:(VK_IMAGE_USAGE_SAMPLED_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT);
	ici.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	ici.queueFamilyIndexCount = 0;
	ici.pQueueFamilyIndices = NULL;
	ici.initialLayout = ret.layout;
	if (vk.nv_dedicated_allocation/* && (size > foo || rendertarget)*/)	//FIXME: should only really be used for rendertargets or large images, other stuff should be merged. however, as we don't support any memory suballocations, we're going to just flag everything for now.
	{
		nv_daici.dedicatedAllocation = true;
		nv_daici.pNext = ici.pNext;
		ici.pNext = &nv_daici;
	}
	VkAssert(vkCreateImage(vk.device, &ici, vkallocationcb, &ret.image));

	vkGetImageMemoryRequirements(vk.device, ret.image, &mem_reqs);

	memAllocInfo.allocationSize = mem_reqs.size;
	if (staging)
		memAllocInfo.memoryTypeIndex = vk_find_memory_require(mem_reqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
	else
	{
		memAllocInfo.memoryTypeIndex = vk_find_memory_try(mem_reqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		if (memAllocInfo.memoryTypeIndex == ~0)
			memAllocInfo.memoryTypeIndex = vk_find_memory_try(mem_reqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		if (memAllocInfo.memoryTypeIndex == ~0)
			memAllocInfo.memoryTypeIndex = vk_find_memory_try(mem_reqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
		if (memAllocInfo.memoryTypeIndex == ~0)
			memAllocInfo.memoryTypeIndex = vk_find_memory_require(mem_reqs.memoryTypeBits, 0);
	}
	if (nv_daici.dedicatedAllocation)
	{
		nv_damai.image = ret.image;
		nv_damai.pNext = memAllocInfo.pNext;
		memAllocInfo.pNext = &nv_damai;
	}
	VkAssert(vkAllocateMemory(vk.device, &memAllocInfo, vkallocationcb, &ret.memory));
	VkAssert(vkBindImageMemory(vk.device, ret.image, ret.memory, 0));

	ret.view = VK_NULL_HANDLE;
	ret.sampler = VK_NULL_HANDLE;

	if (!staging)
	{
		VkImageViewCreateInfo viewInfo = {VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
		viewInfo.flags = 0;
		viewInfo.image = ret.image;
		viewInfo.viewType = (ret.type==PTI_CUBEMAP)?VK_IMAGE_VIEW_TYPE_CUBE:VK_IMAGE_VIEW_TYPE_2D;
		viewInfo.format = format;
		viewInfo.components.r = VK_COMPONENT_SWIZZLE_R;
		viewInfo.components.g = VK_COMPONENT_SWIZZLE_G;
		viewInfo.components.b = VK_COMPONENT_SWIZZLE_B;
		viewInfo.components.a = (encoding == PTI_RGBX8 || encoding == PTI_BGRX8)?VK_COMPONENT_SWIZZLE_ONE:VK_COMPONENT_SWIZZLE_A;
		viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		viewInfo.subresourceRange.baseMipLevel = 0;
		viewInfo.subresourceRange.levelCount = mips;
		viewInfo.subresourceRange.baseArrayLayer = 0;
		viewInfo.subresourceRange.layerCount = layers;
		VkAssert(vkCreateImageView(vk.device, &viewInfo, NULL, &ret.view));
	}
	return ret;
}
void set_image_layout(VkCommandBuffer cmd, VkImage image, VkImageAspectFlags aspectMask, VkImageLayout old_image_layout, VkAccessFlags srcaccess, VkImageLayout new_image_layout, VkAccessFlags dstaccess)
{
	//images have weird layout representations.
	//we need to use a side-effect of memory barriers in order to convert from one layout to another, so that we can actually use the image.
	VkImageMemoryBarrier imgbarrier = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
	imgbarrier.pNext = NULL;
	imgbarrier.srcAccessMask = srcaccess;
	imgbarrier.dstAccessMask = dstaccess;
	imgbarrier.oldLayout = old_image_layout;
	imgbarrier.newLayout = new_image_layout;
	imgbarrier.image = image;
	imgbarrier.subresourceRange.aspectMask = aspectMask;
	imgbarrier.subresourceRange.baseMipLevel = 0;
	imgbarrier.subresourceRange.levelCount = 1;
	imgbarrier.subresourceRange.baseArrayLayer = 0;
	imgbarrier.subresourceRange.layerCount = 1;
	imgbarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	imgbarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
/*
	if (new_image_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)	// Make sure anything that was copying from this image has completed
		imgbarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	else if (new_image_layout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL)	// Make sure anything that was copying from this image has completed
		imgbarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
	else if (new_image_layout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
		imgbarrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	else if (new_image_layout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
		imgbarrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
	else if (new_image_layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) // Make sure any Copy or CPU writes to image are flushed 
		imgbarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_INPUT_ATTACHMENT_READ_BIT;

	if (old_image_layout == VK_IMAGE_LAYOUT_PREINITIALIZED)
		imgbarrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT;
	else if (old_image_layout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL)
		imgbarrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
	else if (old_image_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
		imgbarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
*/
	vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 0, 0, NULL, 0, NULL, 1, &imgbarrier);
}

void VK_FencedCheck(void)
{
	while(vk.fencework)
	{
		Sys_LockConditional(vk.submitcondition);
		if (VK_SUCCESS == vkGetFenceStatus(vk.device, vk.fencework->fence))
		{
			struct vk_fencework *w;
			w = vk.fencework;
			vk.fencework = w->next;
			if (!vk.fencework)
				vk.fencework_last = NULL;
			Sys_UnlockConditional(vk.submitcondition);

			if (w->Passed)
				w->Passed(w);
			if (w->cbuf)
				vkFreeCommandBuffers(vk.device, vk.cmdpool, 1, &w->cbuf);
			if (w->fence)
				vkDestroyFence(vk.device, w->fence, vkallocationcb);
			Z_Free(w);
			continue;
		}
		Sys_UnlockConditional(vk.submitcondition);
		break;
	}
}
//allocate and begin a commandbuffer so we can do the copies
void *VK_FencedBegin(void (*passed)(void *work), size_t worksize)
{
	struct vk_fencework *w = BZ_Malloc(worksize?worksize:sizeof(*w));

	VkCommandBufferAllocateInfo cbai = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
	VkCommandBufferInheritanceInfo cmdinh = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO};
	VkCommandBufferBeginInfo cmdinf = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
	cbai.commandPool = vk.cmdpool;
	cbai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	cbai.commandBufferCount = 1;
	VkAssert(vkAllocateCommandBuffers(vk.device, &cbai, &w->cbuf));
	cmdinf.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	cmdinf.pInheritanceInfo = &cmdinh;
	vkBeginCommandBuffer(w->cbuf, &cmdinf);

	w->Passed = passed;
	w->next = NULL;

	return w;
}
//end+submit a commandbuffer, and set up a fence so we know when its complete
void VK_FencedSubmit(void *work)
{
	struct vk_fencework *w = work;
	VkFenceCreateInfo fenceinfo = {VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};

	if (w->cbuf)
		vkEndCommandBuffer(w->cbuf);

	//check if we can release anything yet.
	VK_FencedCheck();

	vkCreateFence(vk.device, &fenceinfo, vkallocationcb, &w->fence);

	VK_Submit_Work(w->cbuf, VK_NULL_HANDLE, 0, VK_NULL_HANDLE, w->fence, NULL, w);
}

void VK_FencedSync(void *work)
{
	struct vk_fencework *w = work;
	VK_FencedSubmit(w);

	//fixme: waiting for the fence while it may still be getting created by the worker is unsafe.
	vkWaitForFences(vk.device, 1, &w->fence, VK_FALSE, UINT64_MAX);
}

//called to schedule the release of a resource that may be referenced by an active command buffer.
//the command buffer in question may even have not yet been submitted yet.
void *VK_AtFrameEnd(void (*frameended)(void *work), void *workdata, size_t worksize)
{
	struct vk_frameend *w = Z_Malloc(sizeof(*w) + worksize);

	w->FrameEnded = frameended;
	w->next = vk.frameendjobs;
	vk.frameendjobs = w;

	if (workdata)
		memcpy(w+1, workdata, worksize);

	return w+1;
}

#define USE_STAGING_BUFFERS
struct texturefence
{
	struct vk_fencework w;

	int mips;
#ifdef USE_STAGING_BUFFERS
	VkBuffer stagingbuffer;
	VkDeviceMemory stagingmemory;
#else
	vk_image_t staging[32];
#endif
};
static void VK_TextureLoaded(void *ctx)
{
	struct texturefence *w = ctx;
#ifdef USE_STAGING_BUFFERS
	vkDestroyBuffer(vk.device, w->stagingbuffer, vkallocationcb);
	vkFreeMemory(vk.device, w->stagingmemory, vkallocationcb);
#else
	unsigned int i;
	for (i = 0; i < w->mips; i++)
		if (w->staging[i].image != VK_NULL_HANDLE)
		{
			vkDestroyImage(vk.device, w->staging[i].image, vkallocationcb);
			vkFreeMemory(vk.device, w->staging[i].memory, vkallocationcb);
		}
#endif
}
qboolean VK_LoadTextureMips (texid_t tex, const struct pendingtextureinfo *mips)
{
#ifdef USE_STAGING_BUFFERS
	VkBufferCreateInfo bci = {VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
	VkMemoryRequirements mem_reqs;
	VkMemoryAllocateInfo memAllocInfo = {VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
	void *mapdata;
#else
	uint32_t y;
#endif

	struct texturefence *fence;
	VkCommandBuffer vkloadcmd;
	vk_image_t target;
	uint32_t i;
	uint32_t blocksize;
	uint32_t blockbytes;
	uint32_t layers;
	uint32_t mipcount = mips->mipcount;
	if (mips->type != PTI_2D && mips->type != PTI_CUBEMAP)
		return false;
	if (!mipcount || mips->mip[0].width == 0 || mips->mip[0].height == 0)
		return false;

	layers = (mips->type == PTI_CUBEMAP)?6:1;

	if (layers == 1 && mipcount > 1)
	{	//npot mipmapped textures are awkward.
		//vulkan floors.
		for (i = 1; i < mipcount; i++)
		{
			if (mips->mip[i].width != max(1,(mips->mip[i-1].width>>1)) ||
				mips->mip[i].height != max(1,(mips->mip[i-1].height>>1)))
			{	//okay, this mip looks like it was sized wrongly. this can easily happen with dds files.
				mipcount = i;
				break;
			}
		}
	}

	switch(mips->encoding)
	{
	case PTI_RGB565:
	case PTI_RGBA4444:
	case PTI_ARGB4444:
	case PTI_RGBA5551:
	case PTI_ARGB1555:
		blocksize = 1;
		blockbytes = 2;	//16bit formats
		break;
	case PTI_RGBA8:
	case PTI_RGBX8:
	case PTI_BGRA8:
	case PTI_BGRX8:
	case PTI_RGBA8_SRGB:
	case PTI_RGBX8_SRGB:
	case PTI_BGRA8_SRGB:
	case PTI_BGRX8_SRGB:
		blocksize = 1;	//in texels
		blockbytes = 4;
		break;
	case PTI_S3RGB1:
	case PTI_S3RGBA1:
		blocksize = 4;
		blockbytes = 8;
		break;
	case PTI_S3RGBA3:
	case PTI_S3RGBA5:
		blocksize = 4;
		blockbytes = 16;
		break;
	case PTI_RGBA16F:
		blocksize = 1;
		blockbytes = 4*2;
		break;
	case PTI_RGBA32F:
		blocksize = 1;
		blockbytes = 4*4;
		break;
	case PTI_R8:
		blocksize = 1;
		blockbytes = 1;
		break;
	case PTI_RG8:
		blocksize = 1;
		blockbytes = 2;
		break;

	default:
		return false;
	}

	fence = VK_FencedBegin(VK_TextureLoaded, sizeof(*fence));
	fence->mips = mipcount;
	vkloadcmd = fence->w.cbuf;

	//create our target image

	if (tex->vkimage)
	{
		if (tex->vkimage->width != mips->mip[0].width ||
			tex->vkimage->height != mips->mip[0].height ||
			tex->vkimage->layers != layers ||
			tex->vkimage->mipcount != mipcount ||
			tex->vkimage->encoding != mips->encoding ||
			tex->vkimage->type != mips->type)
		{
			VK_AtFrameEnd(VK_DestroyVkTexture, tex->vkimage, sizeof(*tex->vkimage));
//			vkDeviceWaitIdle(vk.device);	//erk, we can't cope with a commandbuffer poking the texture while things happen
//			VK_FencedCheck();
//			VK_DestroyVkTexture(tex->vkimage);
			Z_Free(tex->vkimage);
			tex->vkimage = NULL;
		}
	}

	if (tex->vkimage)
	{
		target = *tex->vkimage;	//can reuse it
		Z_Free(tex->vkimage);
		//we're meant to be replacing the entire thing, so we can just transition from undefined here
//		set_image_layout(vkloadcmd, target.image, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_ACCESS_SHADER_READ_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_ACCESS_TRANSFER_WRITE_BIT);

		{
			//images have weird layout representations.
			//we need to use a side-effect of memory barriers in order to convert from one layout to another, so that we can actually use the image.
			VkImageMemoryBarrier imgbarrier = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
			imgbarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			imgbarrier.newLayout = target.layout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
			imgbarrier.image = target.image;
			imgbarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			imgbarrier.subresourceRange.baseMipLevel = 0;
			imgbarrier.subresourceRange.levelCount = mipcount/layers;
			imgbarrier.subresourceRange.baseArrayLayer = 0;
			imgbarrier.subresourceRange.layerCount = layers;
			imgbarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			imgbarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

			imgbarrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
			imgbarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			vkCmdPipelineBarrier(vkloadcmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 0, 0, NULL, 0, NULL, 1, &imgbarrier);
		}
	}
	else
	{
		target = VK_CreateTexture2DArray(mips->mip[0].width, mips->mip[0].height, layers, mipcount/layers, mips->encoding, mips->type, !!(tex->flags&IF_RENDERTARGET));

		{
			//images have weird layout representations.
			//we need to use a side-effect of memory barriers in order to convert from one layout to another, so that we can actually use the image.
			VkImageMemoryBarrier imgbarrier = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
			imgbarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			imgbarrier.newLayout = target.layout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
			imgbarrier.image = target.image;
			imgbarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			imgbarrier.subresourceRange.baseMipLevel = 0;
			imgbarrier.subresourceRange.levelCount = mipcount/layers;
			imgbarrier.subresourceRange.baseArrayLayer = 0;
			imgbarrier.subresourceRange.layerCount = layers;
			imgbarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			imgbarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

			imgbarrier.srcAccessMask = 0;
			imgbarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			vkCmdPipelineBarrier(vkloadcmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 0, 0, NULL, 0, NULL, 1, &imgbarrier);
		}
	}

#ifdef USE_STAGING_BUFFERS
	//figure out how big our staging buffer needs to be
	bci.size = 0;
	for (i = 0; i < mipcount; i++)
	{
		uint32_t blockwidth = (mips->mip[i].width+blocksize-1) / blocksize;
		uint32_t blockheight = (mips->mip[i].height+blocksize-1) / blocksize;

		bci.size += blockwidth*blockheight*blockbytes;
	}
	bci.flags = 0;
	bci.usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
	bci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	bci.queueFamilyIndexCount = 0;
	bci.pQueueFamilyIndices = NULL;

	//FIXME: nvidia's vkCreateBuffer ends up calling NtYieldExecution.
	//which is basically a waste of time, and its hurting framerates.

	//create+map the staging buffer
	VkAssert(vkCreateBuffer(vk.device, &bci, vkallocationcb, &fence->stagingbuffer));
	vkGetBufferMemoryRequirements(vk.device, fence->stagingbuffer, &mem_reqs);
	memAllocInfo.allocationSize = mem_reqs.size;
	memAllocInfo.memoryTypeIndex = vk_find_memory_require(mem_reqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
	VkAssert(vkAllocateMemory(vk.device, &memAllocInfo, vkallocationcb, &fence->stagingmemory));
	VkAssert(vkBindBufferMemory(vk.device, fence->stagingbuffer, fence->stagingmemory, 0));
	VkAssert(vkMapMemory(vk.device, fence->stagingmemory, 0, bci.size, 0, &mapdata));
	if (!mapdata)
		Sys_Error("Unable to map staging image\n");

	bci.size = 0;
	for (i = 0; i < mipcount; i++)
	{
		VkImageSubresource subres = {0};
		VkBufferImageCopy region;
		//figure out the number of 'blocks' in the image.
		//for non-compressed formats this is just the width directly.
		//for compressed formats (ie: s3tc/dxt) we need to round up to deal with npot.
		uint32_t blockwidth = (mips->mip[i].width+blocksize-1) / blocksize;
		uint32_t blockheight = (mips->mip[i].height+blocksize-1) / blocksize;

		if (mips->mip[i].data)
			memcpy((char*)mapdata + bci.size, (char*)mips->mip[i].data, blockwidth*blockbytes*blockheight);
		else
			memset((char*)mapdata + bci.size, 0, blockwidth*blockbytes*blockheight);

		//queue up a buffer->image copy for this mip
		region.bufferOffset = bci.size;
		region.bufferRowLength = blockwidth*blocksize;//*blockbytes;
		region.bufferImageHeight = blockheight*blocksize;
		region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		region.imageSubresource.mipLevel = i%(mipcount/layers);
		region.imageSubresource.baseArrayLayer = i/(mipcount/layers);
		region.imageSubresource.layerCount = 1;
		region.imageOffset.x = 0;
		region.imageOffset.y = 0;
		region.imageOffset.z = 0;
		region.imageExtent.width = mips->mip[i].width;//blockwidth*blocksize;
		region.imageExtent.height = mips->mip[i].height;//blockheight*blocksize;
		region.imageExtent.depth = 1;

		vkCmdCopyBufferToImage(vkloadcmd, fence->stagingbuffer, target.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

		bci.size += blockwidth*blockheight*blockbytes;
	}
	vkUnmapMemory(vk.device, fence->stagingmemory);
#else
//create the staging images and fill them
	for (i = 0; i < mipcount; i++)
	{
		VkImageSubresource subres = {0};
		VkSubresourceLayout layout;
		void *mapdata;
		//figure out the number of 'blocks' in the image.
		//for non-compressed formats this is just the width directly.
		//for compressed formats (ie: s3tc/dxt) we need to round up to deal with npot.
		uint32_t blockwidth = (mips->mip[i].width+blocksize-1) / blocksize;
		uint32_t blockheight = (mips->mip[i].height+blocksize-1) / blocksize;

		fence->staging[i] = VK_CreateTexture2DArray(mips->mip[i].width, mips->mip[i].height, 0, 1, mips->encoding, PTI_2D);
		subres.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		subres.mipLevel = 0;
		subres.arrayLayer = 0;
		vkGetImageSubresourceLayout(vk.device, fence->staging[i].image, &subres, &layout);
		VkAssert(vkMapMemory(vk.device, fence->staging[i].memory, 0, layout.size, 0, &mapdata));
		if (mapdata)
		{
			for (y = 0; y < blockheight; y++)
				memcpy((char*)mapdata + layout.offset + y*layout.rowPitch, (char*)mips->mip[i].data + y*blockwidth*blockbytes, blockwidth*blockbytes);
		}
		else
			Sys_Error("Unable to map staging image\n");
	
		vkUnmapMemory(vk.device, fence->staging[i].memory);

		//queue up an image copy for this mip
		{
			VkImageCopy region;
			region.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			region.srcSubresource.mipLevel = 0;
			region.srcSubresource.baseArrayLayer = 0;
			region.srcSubresource.layerCount = 1;
			region.srcOffset.x = 0;
			region.srcOffset.y = 0;
			region.srcOffset.z = 0;
			region.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			region.dstSubresource.mipLevel = i%(mipcount/layers);
			region.dstSubresource.baseArrayLayer = i/(mipcount/layers);
			region.dstSubresource.layerCount = 1;
			region.dstOffset.x = 0;
			region.dstOffset.y = 0;
			region.dstOffset.z = 0;
			region.extent.width = mips->mip[i].width;
			region.extent.height = mips->mip[i].height;
			region.extent.depth = 1;

			set_image_layout(vkloadcmd, fence->staging[i].image, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_PREINITIALIZED, VK_ACCESS_HOST_WRITE_BIT, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_ACCESS_TRANSFER_READ_BIT);
			vkCmdCopyImage(vkloadcmd, fence->staging[i].image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, target.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
		}
	}
#endif

	//layouts are annoying. and weird.
	{
		//images have weird layout representations.
		//we need to use a side-effect of memory barriers in order to convert from one layout to another, so that we can actually use the image.
		VkImageMemoryBarrier imgbarrier = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
		imgbarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		imgbarrier.newLayout = target.layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		imgbarrier.image = target.image;
		imgbarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		imgbarrier.subresourceRange.baseMipLevel = 0;
		imgbarrier.subresourceRange.levelCount = mipcount/layers;
		imgbarrier.subresourceRange.baseArrayLayer = 0;
		imgbarrier.subresourceRange.layerCount = layers;
		imgbarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		imgbarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

		imgbarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		imgbarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_INPUT_ATTACHMENT_READ_BIT;
		vkCmdPipelineBarrier(vkloadcmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 0, 0, NULL, 0, NULL, 1, &imgbarrier);
	}

	VK_FencedSubmit(fence);

	//FIXME: should probably reuse these samplers.
	if (!target.sampler)
		VK_CreateSampler(tex->flags, &target);

	tex->vkdescriptor = VK_NULL_HANDLE;

	tex->vkimage = Z_Malloc(sizeof(*tex->vkimage));
	*tex->vkimage = target;

	return true;
}
void    VK_DestroyTexture			(texid_t tex)
{
	if (tex->vkimage)
	{
		VK_DestroyVkTexture(tex->vkimage);
		Z_Free(tex->vkimage);
		tex->vkimage = NULL;
	}
	tex->vkdescriptor = VK_NULL_HANDLE;
}




void	VK_R_Init					(void)
{
}
void	VK_R_DeInit					(void)
{
	R_GAliasFlushSkinCache(true);
	Surf_DeInit();
	VK_Shutdown_PostProc();
	VK_DestroySwapChain();
	VKBE_Shutdown();
	Shader_Shutdown();
	Image_Shutdown();
}

void VK_SetupViewPortProjection(qboolean flipy)
{
	float fov_x, fov_y;

	AngleVectors (r_refdef.viewangles, vpn, vright, vup);
	VectorCopy (r_refdef.vieworg, r_origin);

	fov_x = r_refdef.fov_x;//+sin(cl.time)*5;
	fov_y = r_refdef.fov_y;//-sin(cl.time+1)*5;

	if ((r_refdef.flags & RDF_UNDERWATER) && !(r_refdef.flags & RDF_WATERWARP))
	{
		fov_x *= 1 + (((sin(cl.time * 4.7) + 1) * 0.015) * r_waterwarp.value);
		fov_y *= 1 + (((sin(cl.time * 3.0) + 1) * 0.015) * r_waterwarp.value);
	}

//	screenaspect = (float)r_refdef.vrect.width/r_refdef.vrect.height;

	/*view matrix*/
	if (flipy)	//mimic gl and give bottom-up
	{
		vec3_t down;
		VectorNegate(vup, down);
		VectorCopy(down, vup);
		Matrix4x4_CM_ModelViewMatrixFromAxis(r_refdef.m_view, vpn, vright, down, r_refdef.vieworg);
		r_refdef.flipcull = SHADER_CULL_FRONT | SHADER_CULL_BACK;
	}
	else
	{
		Matrix4x4_CM_ModelViewMatrixFromAxis(r_refdef.m_view, vpn, vright, vup, r_refdef.vieworg);
		r_refdef.flipcull = 0;
	}
	if (r_refdef.maxdist)
		Matrix4x4_CM_Projection_Far(r_refdef.m_projection, fov_x, fov_y, r_refdef.mindist, r_refdef.maxdist);
	else
		Matrix4x4_CM_Projection_Inf(r_refdef.m_projection, fov_x, fov_y, r_refdef.mindist);
}

void VK_Set2D(void)
{
	vid.fbvwidth = vid.width;
	vid.fbvheight = vid.height;
	vid.fbpwidth = vid.pixelwidth;
	vid.fbpheight = vid.pixelheight;

	r_refdef.pxrect.x = 0;
	r_refdef.pxrect.y = 0;
	r_refdef.pxrect.width = vid.fbpwidth;
	r_refdef.pxrect.height = vid.fbpheight;
	r_refdef.pxrect.maxheight = vid.pixelheight;

/*
	{
		VkClearDepthStencilValue val;
		VkImageSubresourceRange range;
		val.depth = 1;
		val.stencil = 0;
		range.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
		range.baseArrayLayer = 0;
		range.baseMipLevel = 0;
		range.layerCount = 1;
		range.levelCount = 1;
		vkCmdClearDepthStencilImage(vk.frame->cbuf, vk.depthbuf.image, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, &val, 1, &range);
	}
*/
	
/*
	vkCmdEndRenderPass(vk.frame->cbuf);
	{
		VkRenderPassBeginInfo rpiinfo = {VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
		VkClearValue	clearvalues[1];
		clearvalues[0].depthStencil.depth = 1.0;
		clearvalues[0].depthStencil.stencil = 0;
		rpiinfo.renderPass = vk.renderpass[1];
		rpiinfo.renderArea.offset.x = r_refdef.pxrect.x;
		rpiinfo.renderArea.offset.y = r_refdef.pxrect.y;
		rpiinfo.renderArea.extent.width = r_refdef.pxrect.width;
		rpiinfo.renderArea.extent.height = r_refdef.pxrect.height;
		rpiinfo.framebuffer = vk.frame->backbuf->framebuffer;
		rpiinfo.clearValueCount = 1;
		rpiinfo.pClearValues = clearvalues;
		vkCmdBeginRenderPass(vk.frame->cbuf, &rpiinfo, VK_SUBPASS_CONTENTS_INLINE);
	}
*/
	{
		VkViewport vp[1];
		VkRect2D scissor[1];
		vp[0].x = r_refdef.pxrect.x;
		vp[0].y = r_refdef.pxrect.y;
		vp[0].width = r_refdef.pxrect.width;
		vp[0].height = r_refdef.pxrect.height;
		vp[0].minDepth = 0.0;
		vp[0].maxDepth = 1.0;
		scissor[0].offset.x = r_refdef.pxrect.x;
		scissor[0].offset.y = r_refdef.pxrect.y;
		scissor[0].extent.width = r_refdef.pxrect.width;
		scissor[0].extent.height = r_refdef.pxrect.height;
		vkCmdSetViewport(vk.rendertarg->cbuf, 0, countof(vp), vp);
		vkCmdSetScissor(vk.rendertarg->cbuf, 0, countof(scissor), scissor);
	}

	VKBE_Set2D(true);

	if (0)
		Matrix4x4_CM_Orthographic(r_refdef.m_projection, 0, vid.fbvwidth, 0, vid.fbvheight, -99999, 99999);
	else
		Matrix4x4_CM_Orthographic(r_refdef.m_projection, 0, vid.fbvwidth, vid.fbvheight, 0, -99999, 99999);
	Matrix4x4_Identity(r_refdef.m_view);

	BE_SelectEntity(&r_worldentity);
}

static void VK_Shutdown_PostProc(void)
{
	unsigned int i;
	for (i = 0; i < countof(postproc); i++)
		VKBE_RT_Gen(&postproc[i], 0, 0, true, RT_IMAGEFLAGS);

	vk.scenepp_waterwarp = NULL;
	vk.scenepp_antialias = NULL;
	VK_R_BloomShutdown();
}
static void VK_Init_PostProc(void)
{
	texid_t scenepp_texture_warp, scenepp_texture_edge;
	//this block liberated from the opengl code
	{
#define PP_WARP_TEX_SIZE 64
#define PP_AMP_TEX_SIZE 64
#define PP_AMP_TEX_BORDER 4
		int i, x, y;
		unsigned char pp_warp_tex[PP_WARP_TEX_SIZE*PP_WARP_TEX_SIZE*4];
		unsigned char pp_edge_tex[PP_AMP_TEX_SIZE*PP_AMP_TEX_SIZE*4];

//		scenepp_postproc_cube = r_nulltex;

//		TEXASSIGN(sceneblur_texture, Image_CreateTexture("***postprocess_blur***", NULL, 0));

		TEXASSIGN(scenepp_texture_warp, Image_CreateTexture("***postprocess_warp***", NULL, IF_NOMIPMAP|IF_NOGAMMA|IF_LINEAR));
		TEXASSIGN(scenepp_texture_edge, Image_CreateTexture("***postprocess_edge***", NULL, IF_NOMIPMAP|IF_NOGAMMA|IF_LINEAR));

		// init warp texture - this specifies offset in
		for (y=0; y<PP_WARP_TEX_SIZE; y++)
		{
			for (x=0; x<PP_WARP_TEX_SIZE; x++)
			{
				float fx, fy;

				i = (x + y*PP_WARP_TEX_SIZE) * 4;

				fx = sin(((double)y / PP_WARP_TEX_SIZE) * M_PI * 2);
				fy = cos(((double)x / PP_WARP_TEX_SIZE) * M_PI * 2);

				pp_warp_tex[i  ] = (fx+1.0f)*127.0f;
				pp_warp_tex[i+1] = (fy+1.0f)*127.0f;
				pp_warp_tex[i+2] = 0;
				pp_warp_tex[i+3] = 0xff;
			}
		}

		Image_Upload(scenepp_texture_warp, TF_RGBX32, pp_warp_tex, NULL, PP_WARP_TEX_SIZE, PP_WARP_TEX_SIZE, IF_LINEAR|IF_NOMIPMAP|IF_NOGAMMA);

		// TODO: init edge texture - this is ampscale * 2, with ampscale calculated
		// init warp texture - this specifies offset in
		for (y=0; y<PP_AMP_TEX_SIZE; y++)
		{
			for (x=0; x<PP_AMP_TEX_SIZE; x++)
			{
				float fx = 1, fy = 1;

				i = (x + y*PP_AMP_TEX_SIZE) * 4;

				if (x < PP_AMP_TEX_BORDER)
				{
					fx = (float)x / PP_AMP_TEX_BORDER;
				}
				if (x > PP_AMP_TEX_SIZE - PP_AMP_TEX_BORDER)
				{
					fx = (PP_AMP_TEX_SIZE - (float)x) / PP_AMP_TEX_BORDER;
				}
				
				if (y < PP_AMP_TEX_BORDER)
				{
					fy = (float)y / PP_AMP_TEX_BORDER;
				}
				if (y > PP_AMP_TEX_SIZE - PP_AMP_TEX_BORDER)
				{
					fy = (PP_AMP_TEX_SIZE - (float)y) / PP_AMP_TEX_BORDER;
				}

				//avoid any sudden changes.
				fx=sin(fx*M_PI*0.5);
				fy=sin(fy*M_PI*0.5);

				//lame
				fx = fy = min(fx, fy);

				pp_edge_tex[i  ] = fx * 255;
				pp_edge_tex[i+1] = fy * 255;
				pp_edge_tex[i+2] = 0;
				pp_edge_tex[i+3] = 0xff;
			}
		}

		Image_Upload(scenepp_texture_edge, TF_RGBX32, pp_edge_tex, NULL, PP_AMP_TEX_SIZE, PP_AMP_TEX_SIZE, IF_LINEAR|IF_NOMIPMAP|IF_NOGAMMA);
	}


	vk.scenepp_waterwarp = R_RegisterShader("waterwarp", SUF_NONE,
		"{\n"
			"program underwaterwarp\n"
			"{\n"
				"map $sourcecolour\n"
			"}\n"
			"{\n"
				"map $upperoverlay\n"
			"}\n"
			"{\n"
				"map $loweroverlay\n"
			"}\n"
		"}\n"
		);
	vk.scenepp_waterwarp->defaulttextures->upperoverlay = scenepp_texture_warp;
	vk.scenepp_waterwarp->defaulttextures->loweroverlay = scenepp_texture_edge;

	vk.scenepp_antialias = R_RegisterShader("fte_ppantialias", 0, 
		"{\n"
			"program fxaa\n"
			"{\n"
				"map $sourcecolour\n"
			"}\n"
		"}\n"
		);
}



static qboolean VK_R_RenderScene_Cubemap(struct vk_rendertarg *fb)
{
	int cmapsize = 512;
	int i;
	static vec3_t ang[6] =
				{	{0, -90, 0}, {0, 90, 0},
					{90, 0, 0}, {-90, 0, 0},
					{0, 0, 0}, {0, -180, 0}	};
	vec3_t saveang;
	vec3_t saveorg;

	vrect_t vrect;
	pxrect_t prect;
	extern cvar_t ffov;

	shader_t *shader;
	int facemask;
	extern cvar_t r_projection;
	int osm;
	struct vk_rendertarg_cube *rtc = &vk_rt_cubemap;

	if (!*ffov.string || !strcmp(ffov.string, "0"))
	{
		if (ffov.vec4[0] != scr_fov.value)
		{
			ffov.value = ffov.vec4[0] = scr_fov.value;
			Shader_NeedReload(false);	//gah!
		}
	}

	facemask = 0;
	switch(r_projection.ival)
	{
	default:	//invalid.
		return false;
	case PROJ_STEREOGRAPHIC:
		shader = R_RegisterShader("postproc_stereographic", SUF_NONE,
				"{\n"
					"program postproc_stereographic\n"
					"{\n"
						"map $sourcecube\n"
					"}\n"
				"}\n"
				);

		facemask |= 1<<4; /*front view*/
		if (ffov.value > 70)
		{
			facemask |= (1<<0) | (1<<1); /*side/top*/
			if (ffov.value > 85)
				facemask |= (1<<2) | (1<<3); /*bottom views*/
			if (ffov.value > 300)
				facemask |= 1<<5; /*back view*/
		}
		break;
	case PROJ_FISHEYE:
		shader = R_RegisterShader("postproc_fisheye", SUF_NONE,
				"{\n"
					"program postproc_fisheye\n"
					"{\n"
						"map $sourcecube\n"
					"}\n"
				"}\n"
				);

		//fisheye view sees up to a full sphere
		facemask |= 1<<4; /*front view*/
		if (ffov.value > 77)
			facemask |= (1<<0) | (1<<1) | (1<<2) | (1<<3); /*side/top/bottom views*/
		if (ffov.value > 270)
			facemask |= 1<<5; /*back view*/
		break;
	case PROJ_PANORAMA:
		shader = R_RegisterShader("postproc_panorama", SUF_NONE,
				"{\n"
					"program postproc_panorama\n"
					"{\n"
						"map $sourcecube\n"
					"}\n"
				"}\n"
				);

		//panoramic view needs at most the four sides
		facemask |= 1<<4; /*front view*/
		if (ffov.value > 90)
		{
			facemask |= (1<<0) | (1<<1); /*side views*/
			if (ffov.value > 270)
				facemask |= 1<<5; /*back view*/
		}
		facemask = 0x3f;
		break;
	case PROJ_LAEA:
		shader = R_RegisterShader("postproc_laea", SUF_NONE,
				"{\n"
					"program postproc_laea\n"
					"{\n"
						"map $sourcecube\n"
					"}\n"
				"}\n"
				);

		facemask |= 1<<4; /*front view*/
		if (ffov.value > 90)
		{
			facemask |= (1<<0) | (1<<1) | (1<<2) | (1<<3); /*side/top/bottom views*/
			if (ffov.value > 270)
				facemask |= 1<<5; /*back view*/
		}
		break;

	case PROJ_EQUIRECTANGULAR:
		shader = R_RegisterShader("postproc_equirectangular", SUF_NONE,
				"{\n"
					"program postproc_equirectangular\n"
					"{\n"
						"map $sourcecube\n"
					"}\n"
				"}\n"
				);

		facemask = 0x3f;
#if 0
		facemask |= 1<<4; /*front view*/
		if (ffov.value > 90)
		{
			facemask |= (1<<0) | (1<<1) | (1<<2) | (1<<3); /*side/top/bottom views*/
			if (ffov.value > 270)
				facemask |= 1<<5; /*back view*/
		}
#endif
		break;
	}

	if (!shader || !shader->prog)
		return false;	//erk. shader failed.

	//FIXME: we should be able to rotate the view

	vrect = r_refdef.vrect;
	prect = r_refdef.pxrect;
//	prect.x = (vrect.x * vid.pixelwidth)/vid.width;
//	prect.width = (vrect.width * vid.pixelwidth)/vid.width;
//	prect.y = (vrect.y * vid.pixelheight)/vid.height;
//	prect.height = (vrect.height * vid.pixelheight)/vid.height;

	if (sh_config.texture_non_power_of_two_pic)
	{
		cmapsize = prect.width > prect.height?prect.width:prect.height;
		if (cmapsize > 4096)//sh_config.texture_maxsize)
			cmapsize = 4096;//sh_config.texture_maxsize;
	}


	r_refdef.flags |= RDF_FISHEYE;
	vid.fbpwidth = vid.fbpheight = cmapsize;

	//FIXME: gl_max_size

	VectorCopy(r_refdef.vieworg, saveorg);
	VectorCopy(r_refdef.viewangles, saveang);
	saveang[2] = 0;

	osm = r_refdef.stereomethod;
	r_refdef.stereomethod = STEREO_OFF;

	VKBE_RT_Gen_Cube(rtc, cmapsize, r_clear.ival?true:false);

	vrect = r_refdef.vrect;	//save off the old vrect

	r_refdef.vrect.width = (cmapsize * vid.fbvwidth) / vid.fbpwidth;
	r_refdef.vrect.height = (cmapsize * vid.fbvheight) / vid.fbpheight;
	r_refdef.vrect.x = 0;
	r_refdef.vrect.y = prect.y;

	ang[0][0] = -saveang[0];
	ang[0][1] = -90;
	ang[0][2] = -saveang[0];

	ang[1][0] = -saveang[0];
	ang[1][1] = 90;
	ang[1][2] = saveang[0];
	ang[5][0] = -saveang[0]*2;

	//in theory, we could use a geometry shader to duplicate the polygons to each face.
	//that would of course require that every bit of glsl had such a geometry shader.
	//it would at least reduce cpu load quite a bit.
	for (i = 0; i < 6; i++)
	{
		if (!(facemask & (1<<i)))
			continue;

		VKBE_RT_Begin(&rtc->face[i]);

		r_refdef.fov_x = 90;
		r_refdef.fov_y = 90;
		r_refdef.viewangles[0] = saveang[0]+ang[i][0];
		r_refdef.viewangles[1] = saveang[1]+ang[i][1];
		r_refdef.viewangles[2] = saveang[2]+ang[i][2];


		VK_SetupViewPortProjection(true);

		/*if (!vk.rendertarg->depthcleared)
		{
			VkClearAttachment clr;
			VkClearRect rect;
			clr.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
			clr.clearValue.depthStencil.depth = 1;
			clr.clearValue.depthStencil.stencil = 0;
			clr.colorAttachment = 1;
			rect.rect.offset.x = r_refdef.pxrect.x;
			rect.rect.offset.y = r_refdef.pxrect.y;
			rect.rect.extent.width = r_refdef.pxrect.width;
			rect.rect.extent.height = r_refdef.pxrect.height;
			rect.layerCount = 1;
			rect.baseArrayLayer = 0;
			vkCmdClearAttachments(vk.frame->cbuf, 1, &clr, 1, &rect);
			vk.rendertarg->depthcleared = true;
		}*/

		VKBE_SelectEntity(&r_worldentity);

		R_SetFrustum (r_refdef.m_projection, r_refdef.m_view);
		RQ_BeginFrame();
		if (!(r_refdef.flags & RDF_NOWORLDMODEL))
		{
			if (cl.worldmodel)
				P_DrawParticles ();
		}
		Surf_DrawWorld();
		RQ_RenderBatchClear();

		vk.rendertarg->depthcleared = false;

		if (R2D_Flush)
			Con_Printf("no flush\n");
	}

	r_refdef.vrect = vrect;
	r_refdef.pxrect = prect;
	VectorCopy(saveorg, r_refdef.vieworg);
	r_refdef.stereomethod = osm;

	VKBE_RT_Begin(fb);

	r_refdef.flipcull = 0;
	VK_Set2D();

	shader->defaulttextures->reflectcube = &rtc->q_colour;

	// draw it through the shader
	if (r_projection.ival == PROJ_EQUIRECTANGULAR)
	{
		//note vr screenshots have requirements here
		R2D_Image(vrect.x, vrect.y, vrect.width, vrect.height, 0, 1, 1, 0, shader);
	}
	else if (r_projection.ival == PROJ_PANORAMA)
	{
		float saspect = .5;
		float taspect = vrect.height / vrect.width * ffov.value / 90;//(0.5 * vrect.width) / vrect.height;
		R2D_Image(vrect.x, vrect.y, vrect.width, vrect.height, -saspect, taspect, saspect, -taspect, shader);
	}
	else if (vrect.width > vrect.height)
	{
		float aspect = (0.5 * vrect.height) / vrect.width;
		R2D_Image(vrect.x, vrect.y, vrect.width, vrect.height, -0.5, aspect, 0.5, -aspect, shader);
	}
	else
	{
		float aspect = (0.5 * vrect.width) / vrect.height;
		R2D_Image(vrect.x, vrect.y, vrect.width, vrect.height, -aspect, 0.5, aspect, -0.5, shader);
	}

	if (R2D_Flush)
		R2D_Flush();

	return true;
}

void	VK_R_RenderView				(void)
{
	extern unsigned int r_viewcontents;
	struct vk_rendertarg *rt, *rtscreen = vk.rendertarg;
	extern cvar_t r_fxaa;
	extern	cvar_t r_renderscale, r_postprocshader;
	float renderscale = r_renderscale.value;
	shader_t *custompostproc;

	if (r_norefresh.value || !vid.fbpwidth || !vid.fbpwidth)
	{
		VK_Set2D ();
		return;
	}

	VKBE_Set2D(false);

	Surf_SetupFrame();

	//check if we can do underwater warp
	if (cls.protocol != CP_QUAKE2)	//quake2 tells us directly
	{
		if (r_viewcontents & FTECONTENTS_FLUID)
			r_refdef.flags |= RDF_UNDERWATER;
		else
			r_refdef.flags &= ~RDF_UNDERWATER;
	}
	if (r_refdef.flags & RDF_UNDERWATER)
	{
		extern cvar_t r_projection;
		if (!r_waterwarp.value || r_projection.ival)
			r_refdef.flags &= ~RDF_UNDERWATER;	//no warp at all
		else if (r_waterwarp.value > 0)
			r_refdef.flags |= RDF_WATERWARP;	//try fullscreen warp instead if we can
	}

	if (!r_refdef.globalfog.density)
	{
		int fogtype = ((r_refdef.flags & RDF_UNDERWATER) && cl.fog[1].density)?1:0;
		CL_BlendFog(&r_refdef.globalfog, &cl.oldfog[fogtype], realtime, &cl.fog[fogtype]);
		r_refdef.globalfog.density /= 64;	//FIXME
	}

	custompostproc = NULL;
	if (r_refdef.flags & RDF_NOWORLDMODEL)
		renderscale = 1;	//with no worldmodel, this is probably meant to be transparent so make sure that there's no post-proc stuff messing up transparencies.
	else
	{
		if (*r_postprocshader.string)
		{
			custompostproc = R_RegisterCustom(r_postprocshader.string, SUF_NONE, NULL, NULL);
			if (custompostproc)
				r_refdef.flags |= RDF_CUSTOMPOSTPROC;
		}

		if (r_fxaa.ival) //overlays will have problems.
			r_refdef.flags |= RDF_ANTIALIAS;

		if (R_CanBloom())
			r_refdef.flags |= RDF_BLOOM;
	}

	if (vk.multisamplebits != VK_SAMPLE_COUNT_1_BIT)	//these are unsupported right now.
		r_refdef.flags &= ~(RDF_CUSTOMPOSTPROC|RDF_ANTIALIAS|RDF_BLOOM);

	//
	// figure out the viewport
	//
	{
		int x = r_refdef.vrect.x * vid.pixelwidth/(int)vid.width;
		int x2 = (r_refdef.vrect.x + r_refdef.vrect.width) * vid.pixelwidth/(int)vid.width;
		int y = (r_refdef.vrect.y) * vid.pixelheight/(int)vid.height;
		int y2 = ((int)(r_refdef.vrect.y + r_refdef.vrect.height)) * vid.pixelheight/(int)vid.height;

		// fudge around because of frac screen scale
		if (x > 0)
			x--;
		if (x2 < vid.pixelwidth)
			x2++;
		if (y < 0)
			y--;
		if (y2 < vid.pixelheight)
			y2++;

		r_refdef.pxrect.x = x;
		r_refdef.pxrect.y = y;
		r_refdef.pxrect.width = x2 - x;
		r_refdef.pxrect.height = y2 - y;
		r_refdef.pxrect.maxheight = vid.pixelheight;
	}

	if (renderscale != 1.0)
	{
		r_refdef.flags |= RDF_RENDERSCALE;
		if (renderscale < 0)
			renderscale *= -1;
		r_refdef.pxrect.width *= renderscale;
		r_refdef.pxrect.height *= renderscale;
		r_refdef.pxrect.maxheight = r_refdef.pxrect.height;
	}

	if (r_refdef.pxrect.width <= 0 || r_refdef.pxrect.height <= 0)
		return;	//you're not allowed to do that, dude.

	//FIXME: VF_RT_*
	//FIXME: if we're meant to be using msaa, render the scene to an msaa target and then resolve.

	postproc_buf = 0;
	if (r_refdef.flags & (RDF_ALLPOSTPROC|RDF_RENDERSCALE))
	{
		r_refdef.pxrect.x = 0;
		r_refdef.pxrect.y = 0;
		rt = &postproc[postproc_buf++%countof(postproc)];
		VKBE_RT_Gen(rt, r_refdef.pxrect.width, r_refdef.pxrect.height, false, (r_renderscale.value < 0)?RT_IMAGEFLAGS-IF_LINEAR+IF_NEAREST:RT_IMAGEFLAGS);
	}
	else
		rt = rtscreen;

	if (!(r_refdef.flags & RDF_NOWORLDMODEL) && VK_R_RenderScene_Cubemap(rt))
	{
	}
	else
	{
		VK_SetupViewPortProjection(false);

		if (rt != rtscreen)
			VKBE_RT_Begin(rt);
		else
		{
			VkViewport vp[1];
			VkRect2D scissor[1];
			vp[0].x = r_refdef.pxrect.x;
			vp[0].y = r_refdef.pxrect.y;
			vp[0].width = r_refdef.pxrect.width;
			vp[0].height = r_refdef.pxrect.height;
			vp[0].minDepth = 0.0;
			vp[0].maxDepth = 1.0;
			scissor[0].offset.x = r_refdef.pxrect.x;
			scissor[0].offset.y = r_refdef.pxrect.y;
			scissor[0].extent.width = r_refdef.pxrect.width;
			scissor[0].extent.height = r_refdef.pxrect.height;
			vkCmdSetViewport(vk.rendertarg->cbuf, 0, countof(vp), vp);
			vkCmdSetScissor(vk.rendertarg->cbuf, 0, countof(scissor), scissor);
		}

		if (!vk.rendertarg->depthcleared)
		{
			VkClearAttachment clr;
			VkClearRect rect;
			clr.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
			clr.clearValue.depthStencil.depth = 1;
			clr.clearValue.depthStencil.stencil = 0;
			clr.colorAttachment = 1;
			rect.rect.offset.x = r_refdef.pxrect.x;
			rect.rect.offset.y = r_refdef.pxrect.y;
			rect.rect.extent.width = r_refdef.pxrect.width;
			rect.rect.extent.height = r_refdef.pxrect.height;
			rect.layerCount = 1;
			rect.baseArrayLayer = 0;
			vkCmdClearAttachments(vk.rendertarg->cbuf, 1, &clr, 1, &rect);
			vk.rendertarg->depthcleared = true;
		}

		VKBE_SelectEntity(&r_worldentity);

		R_SetFrustum (r_refdef.m_projection, r_refdef.m_view);
		RQ_BeginFrame();
		if (!(r_refdef.flags & RDF_NOWORLDMODEL))
		{
			if (cl.worldmodel)
				P_DrawParticles ();
		}
		Surf_DrawWorld();
		RQ_RenderBatchClear();

		vk.rendertarg->depthcleared = false;

		VK_Set2D ();

		if (rt != rtscreen)
			VKBE_RT_End(rt);
	}

	if (r_refdef.flags & RDF_ALLPOSTPROC)
	{
		if (!vk.scenepp_waterwarp)
			VK_Init_PostProc();
		//FIXME: chain renderpasses as required.
		if (r_refdef.flags & RDF_WATERWARP)
		{
			r_refdef.flags &= ~RDF_WATERWARP;
			vk.sourcecolour = &rt->q_colour;
			if (r_refdef.flags & RDF_ALLPOSTPROC)
			{
				rt = &postproc[postproc_buf++];
				VKBE_RT_Gen(rt, 320, 200, false, RT_IMAGEFLAGS);
			}
			else
				rt = rtscreen;
			if (rt != rtscreen)
				VKBE_RT_Begin(rt);
			R2D_Image(r_refdef.vrect.x, r_refdef.vrect.y, r_refdef.vrect.width, r_refdef.vrect.height, 0, 0, 1, 1, vk.scenepp_waterwarp);
			R2D_Flush();
			if (rt != rtscreen)
				VKBE_RT_End(rt);
		}
		if (r_refdef.flags & RDF_CUSTOMPOSTPROC)
		{
			r_refdef.flags &= ~RDF_CUSTOMPOSTPROC;
			vk.sourcecolour = &rt->q_colour;
			if (r_refdef.flags & RDF_ALLPOSTPROC)
			{
				rt = &postproc[postproc_buf++];
				VKBE_RT_Gen(rt, 320, 200, false, RT_IMAGEFLAGS);
			}
			else
				rt = rtscreen;
			if (rt != rtscreen)
				VKBE_RT_Begin(rt);
			R2D_Image(r_refdef.vrect.x, r_refdef.vrect.y, r_refdef.vrect.width, r_refdef.vrect.height, 0, 1, 1, 0, custompostproc);
			R2D_Flush();
			if (rt != rtscreen)
				VKBE_RT_End(rt);
		}
		if (r_refdef.flags & RDF_ANTIALIAS)
		{
			r_refdef.flags &= ~RDF_ANTIALIAS;
			R2D_ImageColours(rt->width, rt->height, 1, 1);
			vk.sourcecolour = &rt->q_colour;
			if (r_refdef.flags & RDF_ALLPOSTPROC)
			{
				rt = &postproc[postproc_buf++];
				VKBE_RT_Gen(rt, 320, 200, false, RT_IMAGEFLAGS);
			}
			else
				rt = rtscreen;
			if (rt != rtscreen)
				VKBE_RT_Begin(rt);
			R2D_Image(r_refdef.vrect.x, r_refdef.vrect.y, r_refdef.vrect.width, r_refdef.vrect.height, 0, 1, 1, 0, vk.scenepp_antialias);
			R2D_ImageColours(1, 1, 1, 1);
			R2D_Flush();
			if (rt != rtscreen)
				VKBE_RT_End(rt);
		}
		if (r_refdef.flags & RDF_BLOOM)
		{
			VK_R_BloomBlend(&rt->q_colour, r_refdef.vrect.x, r_refdef.vrect.y, r_refdef.vrect.width, r_refdef.vrect.height);
			rt = rtscreen;
		}
	}
	else if (r_refdef.flags & RDF_RENDERSCALE)
	{
		if (!vk.scenepp_rescale)
			vk.scenepp_rescale = R_RegisterShader("fte_rescaler", 0, 
				"{\n"
					"program default2d\n"
					"{\n"
						"map $sourcecolour\n"
					"}\n"
				"}\n"
				);
		vk.sourcecolour = &rt->q_colour;
		rt = rtscreen;
		R2D_Image(r_refdef.vrect.x, r_refdef.vrect.y, r_refdef.vrect.width, r_refdef.vrect.height, 0, 0, 1, 1, vk.scenepp_rescale);
		R2D_Flush();
	}
	vk.sourcecolour = r_nulltex;
}


typedef struct
{
	uint32_t imageformat;
	uint32_t imagestride;
	uint32_t imagewidth;
	uint32_t imageheight;
	VkBuffer buffer;
	size_t memsize;
	VkDeviceMemory memory;
	void (*gotrgbdata) (void *rgbdata, intptr_t bytestride, size_t width, size_t height, enum uploadfmt fmt);
} vkscreencapture_t;

static void VKVID_CopiedRGBData (void*ctx)
{	//some fence got hit, we did our copy, data is now cpu-visible, cache-willing.
	vkscreencapture_t *capt = ctx;
	void *imgdata;
	VkAssert(vkMapMemory(vk.device, capt->memory, 0, capt->memsize, 0, &imgdata));
	capt->gotrgbdata(imgdata, capt->imagestride, capt->imagewidth, capt->imageheight, capt->imageformat);
	vkUnmapMemory(vk.device, capt->memory);
	vkDestroyBuffer(vk.device, capt->buffer, vkallocationcb);
	vkFreeMemory(vk.device, capt->memory, vkallocationcb);
}
void VKVID_QueueGetRGBData			(void (*gotrgbdata) (void *rgbdata, intptr_t bytestride, size_t width, size_t height, enum uploadfmt fmt))
{
	//should be half way through rendering
	vkscreencapture_t *capt;

	VkBufferImageCopy icpy;

	VkMemoryRequirements mem_reqs;
	VkMemoryAllocateInfo memAllocInfo = {VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
	VkBufferCreateInfo bci = {VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};


	if (!VK_SCR_GrabBackBuffer())
		return;

	if (!vk.frame->backbuf->colour.width || !vk.frame->backbuf->colour.height)
		return; //erm, some kind of error?

	capt = VK_AtFrameEnd(VKVID_CopiedRGBData, NULL, sizeof(*capt));
	capt->gotrgbdata = gotrgbdata;

	capt->imageformat = TF_BGRA32;
	capt->imagestride = vk.frame->backbuf->colour.width*4;	//vulkan is top-down, so this should be positive.
	capt->imagewidth = vk.frame->backbuf->colour.width;
	capt->imageheight = vk.frame->backbuf->colour.height;

	bci.flags = 0;
	bci.size = capt->memsize = capt->imagewidth*capt->imageheight*4;
	bci.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	bci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	bci.queueFamilyIndexCount = 0;
	bci.pQueueFamilyIndices = NULL;

	VkAssert(vkCreateBuffer(vk.device, &bci, vkallocationcb, &capt->buffer));
	vkGetBufferMemoryRequirements(vk.device, capt->buffer, &mem_reqs);
	memAllocInfo.allocationSize = mem_reqs.size;
	memAllocInfo.memoryTypeIndex = vk_find_memory_require(mem_reqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
	VkAssert(vkAllocateMemory(vk.device, &memAllocInfo, vkallocationcb, &capt->memory));
	VkAssert(vkBindBufferMemory(vk.device, capt->buffer, capt->memory, 0));

	set_image_layout(vk.rendertarg->cbuf, vk.frame->backbuf->colour.image, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_ACCESS_TRANSFER_READ_BIT);

	icpy.bufferOffset = 0;
	icpy.bufferRowLength = 0;	//packed
	icpy.bufferImageHeight = 0;	//packed
	icpy.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	icpy.imageSubresource.mipLevel = 0;
	icpy.imageSubresource.baseArrayLayer = 0;
	icpy.imageSubresource.layerCount = 1;
	icpy.imageOffset.x = 0;
	icpy.imageOffset.y = 0;
	icpy.imageOffset.z = 0;
	icpy.imageExtent.width = capt->imagewidth;
	icpy.imageExtent.height = capt->imageheight;
	icpy.imageExtent.depth = 1;

	vkCmdCopyImageToBuffer(vk.rendertarg->cbuf, vk.frame->backbuf->colour.image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, capt->buffer, 1, &icpy);

	set_image_layout(vk.rendertarg->cbuf, vk.frame->backbuf->colour.image, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_ACCESS_TRANSFER_READ_BIT, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT);
}

char	*VKVID_GetRGBInfo			(int *bytestride, int *truevidwidth, int *truevidheight, enum uploadfmt *fmt)
{
	//with vulkan, we need to create a staging image to write into, submit a copy, wait for completion, map the copy, copy that out, free the staging.
	//its enough to make you pitty anyone that writes opengl drivers.
	if (VK_SCR_GrabBackBuffer())
	{
		void *imgdata, *outdata;
		struct vk_fencework *fence = VK_FencedBegin(NULL, 0);
		VkImageCopy icpy;
		VkImage tempimage;
		VkDeviceMemory tempmemory;
		VkImageSubresource subres = {0};
		VkSubresourceLayout layout;

		VkMemoryRequirements mem_reqs;
		VkMemoryAllocateInfo memAllocInfo = {VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
		VkImageCreateInfo ici = {VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
		ici.flags = 0;
		ici.imageType = VK_IMAGE_TYPE_2D;
		ici.format = VK_FORMAT_B8G8R8A8_UNORM;
		ici.extent.width = vid.pixelwidth;
		ici.extent.height = vid.pixelheight;
		ici.extent.depth = 1;
		ici.mipLevels = 1;
		ici.arrayLayers = 1;
		ici.samples = VK_SAMPLE_COUNT_1_BIT;
		ici.tiling = VK_IMAGE_TILING_LINEAR;
		ici.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT;
		ici.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		ici.queueFamilyIndexCount = 0;
		ici.pQueueFamilyIndices = NULL;
		ici.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		VkAssert(vkCreateImage(vk.device, &ici, vkallocationcb, &tempimage));
		vkGetImageMemoryRequirements(vk.device, tempimage, &mem_reqs);
		memAllocInfo.allocationSize = mem_reqs.size;
		memAllocInfo.memoryTypeIndex = vk_find_memory_require(mem_reqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
		VkAssert(vkAllocateMemory(vk.device, &memAllocInfo, vkallocationcb, &tempmemory));
		VkAssert(vkBindImageMemory(vk.device, tempimage, tempmemory, 0));



		set_image_layout(fence->cbuf, vk.frame->backbuf->colour.image, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_ACCESS_TRANSFER_READ_BIT);
		set_image_layout(fence->cbuf, tempimage, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_UNDEFINED, 0, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_ACCESS_TRANSFER_WRITE_BIT);


		//fixme: transition layouts!
		icpy.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		icpy.srcSubresource.mipLevel = 0;
		icpy.srcSubresource.baseArrayLayer = 0;
		icpy.srcSubresource.layerCount = 1;
		icpy.srcOffset.x = 0;
		icpy.srcOffset.y = 0;
		icpy.srcOffset.z = 0;
		icpy.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		icpy.dstSubresource.mipLevel = 0;
		icpy.dstSubresource.baseArrayLayer = 0;
		icpy.dstSubresource.layerCount = 1;
		icpy.dstOffset.x = 0;
		icpy.dstOffset.y = 0;
		icpy.dstOffset.z = 0;
		icpy.extent.width = vid.pixelwidth;
		icpy.extent.height = vid.pixelheight;
		icpy.extent.depth = 1;
		vkCmdCopyImage(fence->cbuf, vk.frame->backbuf->colour.image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, tempimage, VK_IMAGE_LAYOUT_GENERAL, 1, &icpy);

		set_image_layout(fence->cbuf, vk.frame->backbuf->colour.image, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_ACCESS_TRANSFER_READ_BIT, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT);
		set_image_layout(fence->cbuf, tempimage, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_HOST_READ_BIT);

		VK_FencedSync(fence);

		outdata = BZ_Malloc(4*vid.pixelwidth*vid.pixelheight);	//FIXME: this sucks.
		*fmt = PTI_BGRA8;
		*bytestride = vid.pixelwidth*4;
		*truevidwidth = vid.pixelwidth;
		*truevidheight = vid.pixelheight;

		subres.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		subres.mipLevel = 0;
		subres.arrayLayer = 0;
		vkGetImageSubresourceLayout(vk.device, tempimage, &subres, &layout);
		VkAssert(vkMapMemory(vk.device, tempmemory, 0, mem_reqs.size, 0, &imgdata));
		memcpy(outdata, imgdata, 4*vid.pixelwidth*vid.pixelheight);
		vkUnmapMemory(vk.device, tempmemory);

		vkDestroyImage(vk.device, tempimage, vkallocationcb);
		vkFreeMemory(vk.device, tempmemory, vkallocationcb);

		return outdata;
	}
	return NULL;
}

static void VK_PaintScreen(void)
{
	int uimenu;
	qboolean nohud;
	qboolean noworld;

	
	vid.fbvwidth = vid.width;
	vid.fbvheight = vid.height;
	vid.fbpwidth = vid.pixelwidth;
	vid.fbpheight = vid.pixelheight;

	r_refdef.pxrect.x = 0;
	r_refdef.pxrect.y = 0;
	r_refdef.pxrect.width = vid.fbpwidth;
	r_refdef.pxrect.height = vid.fbpheight;
	r_refdef.pxrect.maxheight = vid.pixelheight;

	vid.numpages = vk.backbuf_count + 1;

	R2D_Font_Changed();

	VK_Set2D ();

	Shader_DoReload();

	if (scr_disabled_for_loading)
	{
		extern float scr_disabled_time;
		if (Sys_DoubleTime() - scr_disabled_time > 60 || !Key_Dest_Has(~kdm_game))
		{
			//FIXME: instead of reenabling the screen, we should just draw the relevent things skipping only the game.
			scr_disabled_for_loading = false;
		}
		else
		{
//			scr_drawloading = true;
			SCR_DrawLoading (true);
//			scr_drawloading = false;
			return;
		}
	}

/*	if (!scr_initialized || !con_initialized)
	{
		RSpeedEnd(RSPEED_TOTALREFRESH);
		return;                         // not initialized yet
	}
*/

#ifdef VM_UI
	uimenu = UI_MenuState();
#else
	uimenu = 0;
#endif

#ifdef TEXTEDITOR
	if (editormodal)
	{
		Editor_Draw();
		V_UpdatePalette (false);
#if defined(_WIN32) && defined(GLQUAKE)
		Media_RecordFrame();
#endif
		R2D_BrightenScreen();

		if (key_dest_mask & kdm_console)
			Con_DrawConsole(vid.height/2, false);
		else
			Con_DrawConsole(0, false);
//		SCR_DrawCursor();
		return;
	}
#endif
	if (Media_ShowFilm())
	{
		M_Draw(0);
		V_UpdatePalette (false);
		R2D_BrightenScreen();
#if defined(_WIN32) && defined(GLQUAKE)
		Media_RecordFrame();
#endif
		return;
	}

//
// do 3D refresh drawing, and then update the screen
//
	SCR_SetUpToDrawConsole ();

	noworld = false;
	nohud = false;

#ifdef VM_CG
	if (CG_Refresh())
		nohud = true;
	else
#endif
#ifdef CSQC_DAT
		if (CSQC_DrawView())
		nohud = true;
	else
#endif
	{
		if (uimenu != 1)
		{
			if (r_worldentity.model && cls.state == ca_active)
 				V_RenderView ();
			else
			{
				noworld = true;
			}
		}
	}

//	scr_con_forcedraw = false;
	if (noworld)
	{
		extern char levelshotname[];

		//draw the levelshot or the conback fullscreen
		if (*levelshotname)
			R2D_ScalePic(0, 0, vid.width, vid.height, R2D_SafeCachePic (levelshotname));
		else if (scr_con_current != vid.height)
			R2D_ConsoleBackground(0, vid.height, true);
//		else
//			scr_con_forcedraw = true;

		nohud = true;
	}

	SCR_DrawTwoDimensional(uimenu, nohud);

	V_UpdatePalette (false);
	R2D_BrightenScreen();

#if defined(_WIN32) && defined(GLQUAKE)
	Media_RecordFrame();
#endif

	RSpeedShow();
}

VkCommandBuffer VK_AllocFrameCBuf(void)
{
	struct vkframe *frame = vk.frame;
	if (frame->numcbufs == frame->maxcbufs)
	{
		VkCommandBufferAllocateInfo cbai = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};

		frame->maxcbufs++;
		frame->cbufs = BZ_Realloc(frame->cbufs, sizeof(*frame->cbufs)*frame->maxcbufs);

		cbai.commandPool = vk.cmdpool;
		cbai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		cbai.commandBufferCount = frame->maxcbufs - frame->numcbufs;
		VkAssert(vkAllocateCommandBuffers(vk.device, &cbai, frame->cbufs+frame->numcbufs));
	}
	return frame->cbufs[frame->numcbufs++];
}

qboolean VK_SCR_GrabBackBuffer(void)
{
	RSpeedLocals();

	if (vk.frame)	//erk, we already have one...
		return true;


	RSpeedRemark();

	VK_FencedCheck();

	if (!vk.unusedframes)
	{
		struct vkframe *newframe = Z_Malloc(sizeof(*vk.frame));
		VKBE_InitFramePools(newframe);
		newframe->next = vk.unusedframes;
		vk.unusedframes = newframe;
	}

	while (vk.aquirenext == vk.aquirelast)
	{	//we're still waiting for the render thread to increment acquirelast.
		Sys_Sleep(0);	//o.O
#ifdef _WIN32
		Sys_SendKeyEvents();
#endif
	}

	//wait for the queued acquire to actually finish
	if (vk_busywait.ival)
	{	//busy wait, to try to get the highest fps possible
		while (VK_TIMEOUT == vkGetFenceStatus(vk.device, vk.acquirefences[vk.aquirenext%ACQUIRELIMIT]))
				;
	}
	else
	{
		//friendly wait
		VkResult err = vkWaitForFences(vk.device, 1, &vk.acquirefences[vk.aquirenext%ACQUIRELIMIT], VK_FALSE, UINT64_MAX);
		if (err)
		{
			if (err == VK_ERROR_DEVICE_LOST)
				Sys_Error("Vulkan device lost");
			return false;
		}
	}
	vk.bufferidx = vk.acquirebufferidx[vk.aquirenext%ACQUIRELIMIT];
	VkAssert(vkResetFences(vk.device, 1, &vk.acquirefences[vk.aquirenext%ACQUIRELIMIT]));
	vk.aquirenext++;

	//grab the first unused
	Sys_LockConditional(vk.submitcondition);
	vk.frame = vk.unusedframes;
	vk.unusedframes = vk.frame->next;
	vk.frame->next = NULL;
	Sys_UnlockConditional(vk.submitcondition);

	VkAssert(vkResetFences(vk.device, 1, &vk.frame->finishedfence));

	vk.frame->backbuf = &vk.backbufs[vk.bufferidx];
	vk.rendertarg = vk.frame->backbuf;

	vk.frame->numcbufs = 0;
	vk.rendertarg->cbuf = VK_AllocFrameCBuf();

	RSpeedEnd(RSPEED_SETUP);





	
	{
		VkCommandBufferBeginInfo begininf = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
		VkCommandBufferInheritanceInfo inh = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO};
		begininf.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
		begininf.pInheritanceInfo = &inh;
		inh.renderPass = VK_NULL_HANDLE;	//unused
		inh.subpass = 0;					//unused
		inh.framebuffer = VK_NULL_HANDLE;	//unused
		inh.occlusionQueryEnable = VK_FALSE;
		inh.queryFlags = 0;
		inh.pipelineStatistics = 0;
		vkBeginCommandBuffer(vk.rendertarg->cbuf, &begininf);
	}

	VKBE_RestartFrame();

//	VK_DebugFramerate();

//	vkCmdWriteTimestamp(vk.frame->cbuf, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, querypool, vk.bufferidx*2+0);

	if (vk.multisamplebits == VK_SAMPLE_COUNT_1_BIT)
	{
		VkImageMemoryBarrier imgbarrier = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
		imgbarrier.pNext = NULL;
		imgbarrier.srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
		imgbarrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		imgbarrier.oldLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;	//'Alternately, oldLayout can be VK_IMAGE_LAYOUT_UNDEFINED, if the image’s contents need not be preserved.'
		imgbarrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		imgbarrier.image = vk.frame->backbuf->colour.image;
		imgbarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		imgbarrier.subresourceRange.baseMipLevel = 0;
		imgbarrier.subresourceRange.levelCount = 1;
		imgbarrier.subresourceRange.baseArrayLayer = 0;
		imgbarrier.subresourceRange.layerCount = 1;
		imgbarrier.srcQueueFamilyIndex = vk.queuefam[1];
		imgbarrier.dstQueueFamilyIndex = vk.queuefam[0];
		if (vk.frame->backbuf->firstuse)
		{
			imgbarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			imgbarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			imgbarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			vk.frame->backbuf->firstuse = false;
		}
		vkCmdPipelineBarrier(vk.rendertarg->cbuf, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0, 0, NULL, 0, NULL, 1, &imgbarrier);
	}
	{
		VkImageMemoryBarrier imgbarrier = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
		imgbarrier.pNext = NULL;
		imgbarrier.srcAccessMask = 0;
		imgbarrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
		imgbarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		imgbarrier.newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
		imgbarrier.image = vk.frame->backbuf->depth.image;
		imgbarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
		imgbarrier.subresourceRange.baseMipLevel = 0;
		imgbarrier.subresourceRange.levelCount = 1;
		imgbarrier.subresourceRange.baseArrayLayer = 0;
		imgbarrier.subresourceRange.layerCount = 1;
		imgbarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		imgbarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		vkCmdPipelineBarrier(vk.rendertarg->cbuf, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0, 0, NULL, 0, NULL, 1, &imgbarrier);
	}

	{
		VkClearValue	clearvalues[3];
		extern cvar_t r_clear;
		VkRenderPassBeginInfo rpbi = {VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};

		//attachments are: screen[1], depth[msbits], (screen[msbits])

		clearvalues[0].color.float32[0] = !!(r_clear.ival & 1);
		clearvalues[0].color.float32[1] = !!(r_clear.ival & 2);
		clearvalues[0].color.float32[2] = !!(r_clear.ival & 4);
		clearvalues[0].color.float32[3] = 1;
		clearvalues[1].depthStencil.depth = 1.0;
		clearvalues[1].depthStencil.stencil = 0;

		if (vk.multisamplebits != VK_SAMPLE_COUNT_1_BIT)
		{
			clearvalues[2].color.float32[0] = !!(r_clear.ival & 1);
			clearvalues[2].color.float32[1] = !!(r_clear.ival & 2);
			clearvalues[2].color.float32[2] = !!(r_clear.ival & 4);
			clearvalues[2].color.float32[3] = 1;
			rpbi.clearValueCount = 3;
		}
		else
			rpbi.clearValueCount = 2;

		if (r_clear.ival)
			rpbi.renderPass = vk.renderpass[2];
		else
			rpbi.renderPass = vk.renderpass[1];	//may still clear
		rpbi.framebuffer = vk.frame->backbuf->framebuffer;
		rpbi.renderArea.offset.x = 0;
		rpbi.renderArea.offset.y = 0;
		rpbi.renderArea.extent.width = vid.pixelwidth;
		rpbi.renderArea.extent.height = vid.pixelheight;
		rpbi.pClearValues = clearvalues;
		vkCmdBeginRenderPass(vk.rendertarg->cbuf, &rpbi, VK_SUBPASS_CONTENTS_INLINE);

		vk.frame->backbuf->width = vid.pixelwidth;
		vk.frame->backbuf->height = vid.pixelheight;

		rpbi.clearValueCount = 0;
		rpbi.pClearValues = NULL;
		rpbi.renderPass = vk.renderpass[0];
		vk.rendertarg->restartinfo = rpbi;
		vk.rendertarg->depthcleared = true;
	}
	return true;
}

struct vk_presented
{
	struct vk_fencework fw;
	struct vkframe *frame;
};
void VK_Presented(void *fw)
{
	struct vk_presented *pres = fw;
	struct vkframe *frame = pres->frame;
	pres->fw.fence = VK_NULL_HANDLE;	//don't allow that to be freed.

	while(frame->frameendjobs)
	{
		struct vk_frameend *job = frame->frameendjobs;
		frame->frameendjobs = job->next;
		job->FrameEnded(job+1);
		Z_Free(job);
	}

	frame->next = vk.unusedframes;
	vk.unusedframes = frame;
}

#if 0
void VK_DebugFramerate(void)
{
	static double lastupdatetime;
	static double lastsystemtime;
	double t;
	extern int fps_count;
	float lastfps;

	float frametime;

	t = Sys_DoubleTime();
	if ((t - lastupdatetime) >= 1.0)
	{
		lastfps = fps_count/(t - lastupdatetime);
		fps_count = 0;
		lastupdatetime = t;

		OutputDebugStringA(va("%g fps\n", lastfps));
	}
	frametime = t - lastsystemtime;
	lastsystemtime = t;
}
#endif

qboolean	VK_SCR_UpdateScreen			(void)
{
	uint32_t fblayout;

	VK_FencedCheck();

	//a few cvars need some extra work if they're changed
	if ((vk.allowsubmissionthread && vk_submissionthread.modified) || vid_vsync.modified || vid_triplebuffer.modified || vid_srgb.modified || vid_multisample.modified)
		vk.neednewswapchain = true;

	if (vk.devicelost)
	{	//vkQueueSubmit returning vk_error_device_lost means we give up and try resetting everything.
		//if someone's installing new drivers then wait a little time before reloading everything, in the hope that any other dependant files got copied. or something.
		//fixme: don't allow this to be spammed...
		Sys_Sleep(5);
		Con_Printf("Device was lost. Restarting video\n");
		Cmd_ExecuteString("vid_restart", RESTRICT_LOCAL);
		return false;
	}

	if (vk.neednewswapchain && !vk.frame)
	{
		//kill the thread
		if (vk.submitthread)
		{
			Sys_LockConditional(vk.submitcondition);	//annoying, but required for it to be reliable with respect to other things.
			Sys_ConditionSignal(vk.submitcondition);
			Sys_UnlockConditional(vk.submitcondition);
			Sys_WaitOnThread(vk.submitthread);
			vk.submitthread = NULL;
		}
		//make sure any work is actually done BEFORE the swapchain gets destroyed
		while (vk.work)
		{
			Sys_LockConditional(vk.submitcondition);
			VK_Submit_DoWork();
			Sys_UnlockConditional(vk.submitcondition);
		}
		if (vk.dopresent)
			vk.dopresent(NULL);
		vkDeviceWaitIdle(vk.device);
		VK_CreateSwapChain();
		vk.neednewswapchain = false;

		if (vk.allowsubmissionthread && (vk_submissionthread.ival || !*vk_submissionthread.string))
		{
			vk.submitthread = Sys_CreateThread("vksubmission", VK_Submit_Thread, NULL, THREADP_HIGHEST, 0);
		}
	}

	if (!VK_SCR_GrabBackBuffer())
		return false;

	VKBE_Set2D(true);
	VKBE_SelectDLight(NULL, vec3_origin, NULL, 0);

	VK_PaintScreen();

	if (R2D_Flush)
		R2D_Flush();

	vkCmdEndRenderPass(vk.rendertarg->cbuf);

	fblayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	/*if (0)
	{
		vkscreencapture_t *capt = VK_AtFrameEnd(atframeend, sizeof(vkscreencapture_t));
		VkImageMemoryBarrier imgbarrier = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
		VkBufferImageCopy region;
		imgbarrier.pNext = NULL;
		imgbarrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		imgbarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
		imgbarrier.oldLayout = fblayout;
		imgbarrier.newLayout = fblayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
		imgbarrier.image = vk.frame->backbuf->colour.image;
		imgbarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		imgbarrier.subresourceRange.baseMipLevel = 0;
		imgbarrier.subresourceRange.levelCount = 1;
		imgbarrier.subresourceRange.baseArrayLayer = 0;
		imgbarrier.subresourceRange.layerCount = 1;
		imgbarrier.srcQueueFamilyIndex = vk.queuefam[0];
		imgbarrier.dstQueueFamilyIndex = vk.queuefam[0];
		vkCmdPipelineBarrier(vk.frame->cbuf, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 0, 0, NULL, 0, NULL, 1, &imgbarrier);

		region.bufferOffset = 0;
		region.bufferRowLength = 0;		//tightly packed
		region.bufferImageHeight = 0;	//tightly packed
		region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		region.imageSubresource.mipLevel = 0;
		region.imageSubresource.baseArrayLayer = 0;
		region.imageSubresource.layerCount = 1;
		region.imageOffset.x = 0;
		region.imageOffset.y = 0;
		region.imageOffset.z = 0;
		region.imageExtent.width = capt->imagewidth = vk.frame->backbuf->colour.width;
		region.imageExtent.height = capt->imageheight = vk.frame->backbuf->colour.height;
		region.imageExtent.depth = 1;
		vkCmdCopyImageToBuffer(vk.frame->cbuf, vk.frame->backbuf->colour.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, buffer, 1, &region);
	}*/

	if (vk.multisamplebits == VK_SAMPLE_COUNT_1_BIT)
	{
		VkImageMemoryBarrier imgbarrier = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
		imgbarrier.pNext = NULL;
		imgbarrier.srcAccessMask = /*VK_ACCESS_TRANSFER_READ_BIT|*/VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		imgbarrier.dstAccessMask = 0;
		imgbarrier.oldLayout = fblayout;
		imgbarrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
		imgbarrier.image = vk.frame->backbuf->colour.image;
		imgbarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		imgbarrier.subresourceRange.baseMipLevel = 0;
		imgbarrier.subresourceRange.levelCount = 1;
		imgbarrier.subresourceRange.baseArrayLayer = 0;
		imgbarrier.subresourceRange.layerCount = 1;
		imgbarrier.srcQueueFamilyIndex = vk.queuefam[0];
		imgbarrier.dstQueueFamilyIndex = vk.queuefam[1];
		vkCmdPipelineBarrier(vk.rendertarg->cbuf, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 0, 0, NULL, 0, NULL, 1, &imgbarrier);
	}

//	vkCmdWriteTimestamp(vk.rendertarg->cbuf, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, querypool, vk.bufferidx*2+1);
	vkEndCommandBuffer(vk.rendertarg->cbuf);

	VKBE_FlushDynamicBuffers();

	{
		struct vk_presented *fw = Z_Malloc(sizeof(*fw));
		fw->fw.Passed = VK_Presented;
		fw->fw.fence = vk.frame->finishedfence;
		fw->frame = vk.frame;
		//hand over any post-frame jobs to the frame in question.
		vk.frame->frameendjobs = vk.frameendjobs;
		vk.frameendjobs = NULL;

		VK_Submit_Work(vk.rendertarg->cbuf, VK_NULL_HANDLE, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, vk.frame->backbuf->presentsemaphore, vk.frame->finishedfence, vk.frame, &fw->fw);
	}

	//now would be a good time to do any compute work or lightmap updates...

	vk.frame = NULL;

	VK_FencedCheck();

	VID_SwapBuffers();

#ifdef TEXTEDITOR
	if (editormodal)
	{	//FIXME
		VK_SCR_GrabBackBuffer();
	}
#endif
	return true;
}

void	VKBE_RenderToTextureUpdate2d(qboolean destchanged)
{
}

static void VK_DestroyRenderPass(void)
{
	int i;
	for (i = 0; i < countof(vk.renderpass); i++)
	{
		if (vk.renderpass[i] != VK_NULL_HANDLE)
		{
			vkDestroyRenderPass(vk.device, vk.renderpass[i], vkallocationcb);
			vk.renderpass[i] = VK_NULL_HANDLE;
		}
	}
}
static void VK_CreateRenderPass(void)
{
	int pass;
	int numattachments;
static	VkAttachmentReference color_reference;
static	VkAttachmentReference depth_reference;
static	VkAttachmentReference resolve_reference;
static	VkAttachmentDescription attachments[3] = {{0}};
static	VkSubpassDescription subpass = {0};
static 	VkRenderPassCreateInfo rp_info = {VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};

//two render passes are compatible for piplines when they match exactly except for:
//initial and final layouts in attachment descriptions.
//load and store operations in attachment descriptions.
//image layouts in attachment references.


	for (pass = 0; pass < 3; pass++)
	{
		if (vk.renderpass[pass] != VK_NULL_HANDLE)
			continue;

		numattachments = 0;
		if (vk.multisamplebits != VK_SAMPLE_COUNT_1_BIT)
		{
			resolve_reference.attachment = numattachments++;
			depth_reference.attachment = numattachments++;
			color_reference.attachment = numattachments++;
		}
		else
		{
			color_reference.attachment = numattachments++;
			depth_reference.attachment = numattachments++;
			resolve_reference.attachment = ~(uint32_t)0;
		}

		color_reference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		depth_reference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
		resolve_reference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		attachments[color_reference.attachment].format = vk.backbufformat;
		attachments[color_reference.attachment].samples = vk.multisamplebits;
//		attachments[color_reference.attachment].loadOp = pass?VK_ATTACHMENT_LOAD_OP_LOAD:VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attachments[color_reference.attachment].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		attachments[color_reference.attachment].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attachments[color_reference.attachment].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachments[color_reference.attachment].initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		attachments[color_reference.attachment].finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		attachments[depth_reference.attachment].format = vk.depthformat;
		attachments[depth_reference.attachment].samples = vk.multisamplebits;
//		attachments[depth_reference.attachment].loadOp = pass?VK_ATTACHMENT_LOAD_OP_LOAD:VK_ATTACHMENT_LOAD_OP_CLEAR;
		attachments[depth_reference.attachment].storeOp = VK_ATTACHMENT_STORE_OP_STORE;//VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachments[depth_reference.attachment].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attachments[depth_reference.attachment].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachments[depth_reference.attachment].initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
		attachments[depth_reference.attachment].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

		if (resolve_reference.attachment != ~(uint32_t)0)
		{
			attachments[resolve_reference.attachment].format = vk.backbufformat;
			attachments[resolve_reference.attachment].samples = VK_SAMPLE_COUNT_1_BIT;
			attachments[resolve_reference.attachment].loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
			attachments[resolve_reference.attachment].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
			attachments[resolve_reference.attachment].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
			attachments[resolve_reference.attachment].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
			attachments[resolve_reference.attachment].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			attachments[resolve_reference.attachment].finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
		}

		subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpass.flags = 0;
		subpass.inputAttachmentCount = 0;
		subpass.pInputAttachments = NULL;
		subpass.colorAttachmentCount = 1;
		subpass.pColorAttachments = &color_reference;
		subpass.pResolveAttachments = (resolve_reference.attachment != ~(uint32_t)0)?&resolve_reference:NULL;
		subpass.pDepthStencilAttachment = &depth_reference;
		subpass.preserveAttachmentCount = 0;
		subpass.pPreserveAttachments = NULL;

		rp_info.attachmentCount = numattachments;
		rp_info.pAttachments = attachments;
		rp_info.subpassCount = 1;
		rp_info.pSubpasses = &subpass;
		rp_info.dependencyCount = 0;
		rp_info.pDependencies = NULL;

		if (pass == 0)
		{	//nothing cleared, both are just re-loaded.
			attachments[color_reference.attachment].loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
			attachments[depth_reference.attachment].loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
		}
		else if (pass == 1)
		{	//depth cleared, colour is whatever.
			attachments[color_reference.attachment].loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
			attachments[depth_reference.attachment].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		}
		else
		{	//both cleared
			attachments[color_reference.attachment].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
			attachments[depth_reference.attachment].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		}

		VkAssert(vkCreateRenderPass(vk.device, &rp_info, vkallocationcb, &vk.renderpass[pass]));
	}
}

void VK_DoPresent(struct vkframe *theframe)
{
	VkResult err;
	uint32_t framenum;
	VkPresentInfoKHR presinfo = {VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
	if (!theframe)
		return;	//used to ensure that the queue is flushed at shutdown
	framenum = theframe->backbuf - vk.backbufs;
	presinfo.waitSemaphoreCount = 1;
	presinfo.pWaitSemaphores = &theframe->backbuf->presentsemaphore;
	presinfo.swapchainCount = 1;
	presinfo.pSwapchains = &vk.swapchain;
	presinfo.pImageIndices = &framenum;

	{
		RSpeedMark();
		err = vkQueuePresentKHR(vk.queue_present, &presinfo);
		RSpeedEnd(RSPEED_PRESENT);
	}
	{
		RSpeedMark();
		if (err)
		{
			Con_Printf("ERROR: vkQueuePresentKHR: %x\n", err);
			vk.neednewswapchain = true;
		}
		else
		{
			err = vkAcquireNextImageKHR(vk.device, vk.swapchain, 0, VK_NULL_HANDLE, vk.acquirefences[vk.aquirelast%ACQUIRELIMIT], &vk.acquirebufferidx[vk.aquirelast%ACQUIRELIMIT]);
			if (err)
			{
				Con_Printf("ERROR: vkAcquireNextImageKHR: %x\n", err);
				vk.neednewswapchain = true;
				vk.devicelost |= (err == VK_ERROR_DEVICE_LOST);
			}
			vk.aquirelast++;
		}
		RSpeedEnd(RSPEED_ACQUIRE);
	}
}

static void VK_Submit_DoWork(void)
{
	VkCommandBuffer cbuf[64];
	VkSemaphore wsem[64];
	VkPipelineStageFlags wsemstageflags[64];
	VkSemaphore ssem[64];

	VkQueue	subqueue = NULL;
	VkSubmitInfo subinfo[64];
	unsigned int subcount = 0;
	struct vkwork_s *work;
	struct vkframe *present = NULL;
	VkFence waitfence = VK_NULL_HANDLE;
	VkResult err;
	struct vk_fencework *fencedwork = NULL;
	qboolean errored = false;

	while(vk.work && !present && !waitfence && !fencedwork && subcount < countof(subinfo))
	{
		work = vk.work;
		if (subcount && subqueue != work->queue)
			break;
		subinfo[subcount].sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		subinfo[subcount].pNext = NULL;
		subinfo[subcount].waitSemaphoreCount = work->semwait?1:0;
		subinfo[subcount].pWaitSemaphores = &wsem[subcount];
		wsem[subcount] = work->semwait;
		subinfo[subcount].pWaitDstStageMask = &wsemstageflags[subcount];
		wsemstageflags[subcount] = work->semwaitstagemask;
		subinfo[subcount].commandBufferCount = work->cmdbuf?1:0;
		subinfo[subcount].pCommandBuffers = &cbuf[subcount];
		cbuf[subcount] = work->cmdbuf;
		subinfo[subcount].signalSemaphoreCount = work->semsignal?1:0;
		subinfo[subcount].pSignalSemaphores = &ssem[subcount];
		ssem[subcount] = work->semsignal;
		waitfence = work->fencesignal;
		fencedwork = work->fencedwork;
		subqueue = work->queue;

		subcount++;

		present = work->present;

		vk.work = work->next;
		Z_Free(work);
	}

	Sys_UnlockConditional(vk.submitcondition);	//don't block people giving us work while we're occupied
	if (subcount || waitfence)
	{
		RSpeedMark();
		err = vkQueueSubmit(subqueue, subcount, subinfo, waitfence);
		if (err)
		{
			Con_Printf("ERROR: vkQueueSubmit: %i\n", err);
			errored = vk.neednewswapchain = true;
			vk.devicelost |= (err==VK_ERROR_DEVICE_LOST);
		}
		RSpeedEnd(RSPEED_SUBMIT);
	}

	if (present && !errored)
	{
		vk.dopresent(present);
	}
	
	Sys_LockConditional(vk.submitcondition);

	if (fencedwork)
	{	//this is used for loading and cleaning up things after the gpu has consumed it.
		if (vk.fencework_last)
		{
			vk.fencework_last->next = fencedwork;
			vk.fencework_last = fencedwork;
		}
		else
			vk.fencework_last = vk.fencework = fencedwork;
	}
}

//oh look. a thread.
//nvidia's drivers seem to like doing a lot of blocking in queuesubmit and queuepresent(despite the whole QUEUE thing).
//so thread this work so the main thread doesn't have to block so much.
int VK_Submit_Thread(void *arg)
{
	Sys_LockConditional(vk.submitcondition);
	while(!vk.neednewswapchain)
	{
		if (!vk.work)
			Sys_ConditionWait(vk.submitcondition);

		VK_Submit_DoWork();
	}
	Sys_UnlockConditional(vk.submitcondition);
	return true;
}

void VK_Submit_Work(VkCommandBuffer cmdbuf, VkSemaphore semwait, VkPipelineStageFlags semwaitstagemask, VkSemaphore semsignal, VkFence fencesignal, struct vkframe *presentframe, struct vk_fencework *fencedwork)
{
	struct vkwork_s *work = Z_Malloc(sizeof(*work));
	struct vkwork_s **link;

	work->queue = vk.queue_render;
	work->cmdbuf = cmdbuf;
	work->semwait = semwait;
	work->semwaitstagemask = semwaitstagemask;
	work->semsignal = semsignal;
	work->fencesignal = fencesignal;
	work->present = presentframe;
	work->fencedwork = fencedwork;

	Sys_LockConditional(vk.submitcondition);
	//add it on the end in a lazy way.
	for (link = &vk.work; *link; link = &(*link)->next)
		;
	*link = work;

	if (vk.submitthread && !vk.neednewswapchain)
		Sys_ConditionSignal(vk.submitcondition);
	else
		VK_Submit_DoWork();
	Sys_UnlockConditional(vk.submitcondition);
}

void VK_Submit_Sync(void)
{
	Sys_LockConditional(vk.submitcondition);
	//FIXME: 
	vkDeviceWaitIdle(vk.device); //just in case
	Sys_UnlockConditional(vk.submitcondition);
}

void VK_CheckTextureFormats(void)
{
	struct {
		unsigned int pti;
		VkFormat vulkan;
		unsigned int needextra;
	} texfmt[] =
	{
		{PTI_RGBA8,			VK_FORMAT_R8G8B8A8_UNORM,			VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT},
		{PTI_RGBX8,			VK_FORMAT_R8G8B8A8_UNORM,			VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT},
		{PTI_BGRA8,			VK_FORMAT_B8G8R8A8_UNORM,			VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT},
		{PTI_BGRX8,			VK_FORMAT_B8G8R8A8_UNORM,			VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT},

		{PTI_RGBA8_SRGB,	VK_FORMAT_R8G8B8A8_SRGB,			VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT|VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT},
		{PTI_RGBX8_SRGB,	VK_FORMAT_R8G8B8A8_SRGB,			VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT|VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT},
		{PTI_BGRA8_SRGB,	VK_FORMAT_B8G8R8A8_SRGB,			VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT|VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT},
		{PTI_BGRX8_SRGB,	VK_FORMAT_B8G8R8A8_SRGB,			VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT|VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT},

		{PTI_RGB565,		VK_FORMAT_R5G6B5_UNORM_PACK16,		VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT},
		{PTI_RGBA4444,		VK_FORMAT_R4G4B4A4_UNORM_PACK16,	VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT},
		{PTI_ARGB4444,		VK_FORMAT_B4G4R4A4_UNORM_PACK16,	VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT},
		{PTI_RGBA5551,		VK_FORMAT_R5G5B5A1_UNORM_PACK16,	VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT},
		{PTI_ARGB1555,		VK_FORMAT_A1R5G5B5_UNORM_PACK16,	VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT},
		{PTI_RGBA16F,		VK_FORMAT_R16G16B16A16_SFLOAT,		VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT|VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT|VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BLEND_BIT},
		{PTI_RGBA32F,		VK_FORMAT_R32G32B32A32_SFLOAT,		VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT|VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT|VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BLEND_BIT},
		{PTI_R8,			VK_FORMAT_R8_UNORM,					VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT},
		{PTI_RG8,			VK_FORMAT_R8G8_UNORM,				VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT},

		{PTI_DEPTH16,		VK_FORMAT_D16_UNORM,				VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT},
		{PTI_DEPTH24,		VK_FORMAT_X8_D24_UNORM_PACK32,		VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT},
		{PTI_DEPTH32,		VK_FORMAT_D32_SFLOAT,				VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT},
		{PTI_DEPTH24_8,		VK_FORMAT_D24_UNORM_S8_UINT,		VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT},

		{PTI_S3RGB1,		VK_FORMAT_BC1_RGB_UNORM_BLOCK,		VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT},
		{PTI_S3RGBA1,		VK_FORMAT_BC1_RGBA_UNORM_BLOCK,		VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT},
		{PTI_S3RGBA3,		VK_FORMAT_BC2_UNORM_BLOCK,			VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT},
		{PTI_S3RGBA5,		VK_FORMAT_BC3_UNORM_BLOCK,			VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT},
	};
	unsigned int i;
	VkPhysicalDeviceProperties props;

	vkGetPhysicalDeviceProperties(vk.gpu, &props);
	vk.limits = props.limits;

	sh_config.texture_maxsize = props.limits.maxImageDimension2D;

	for (i = 0; i < countof(texfmt); i++)
	{
		unsigned int need = /*VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT |*/ texfmt[i].needextra;
		VkFormatProperties fmt;
		vkGetPhysicalDeviceFormatProperties(vk.gpu, texfmt[i].vulkan, &fmt);

		if ((fmt.optimalTilingFeatures & need) == need)
			sh_config.texfmt[texfmt[i].pti] = true;
	}
}

//initialise the vulkan instance, context, device, etc.
qboolean VK_Init(rendererstate_t *info, const char *sysextname, qboolean (*createSurface)(void), void (*dopresent)(struct vkframe *theframe))
{
	VkQueueFamilyProperties *queueprops;
	VkResult err;
	VkApplicationInfo app;
	VkInstanceCreateInfo inst_info;
	const char *extensions[8];
	uint32_t extensions_count = 0;

	//device extensions that want to enable
	struct
	{
		qboolean *flag;
		const char *name;
		cvar_t *var;
		qboolean def;
		qboolean *superseeded;	//if this is set then the extension will not be enabled after all
		const char *neededfor;
		qboolean supported;
	} knowndevexts[] =
	{
		{&vk.khr_swapchain,				VK_KHR_SWAPCHAIN_EXTENSION_NAME,			NULL,							true, NULL, " Nothing will be drawn!"},
		{&vk.nv_glsl_shader,			VK_NV_GLSL_SHADER_EXTENSION_NAME,			&vk_nv_glsl_shader,				false, NULL, " Direct use of glsl is not supported."},
		{&vk.nv_dedicated_allocation,	VK_NV_DEDICATED_ALLOCATION_EXTENSION_NAME,	&vk_nv_dedicated_allocation,	true, &vk.khr_dedicated_allocation, ""},
//		{&vk.khr_dedicated_allocation,	VK_KHR_DEDICATED_ALLOCATION_EXTENSION_NAME,	&vk_khr_dedicated_allocation,	true, NULL, ""},
		{&vk.khr_push_descriptor,		VK_KHR_PUSH_DESCRIPTOR_EXTENSION_NAME,		&vk_khr_push_descriptor,		true, NULL, ""},
	};
	size_t e;

	for (e = 0; e < countof(knowndevexts); e++)
		*knowndevexts[e].flag = false;
	vk.allowsubmissionthread = true;
	vk.neednewswapchain = true;
	vk.triplebuffer = info->triplebuffer;
	vk.vsync = info->wait;
	vk.dopresent = dopresent?dopresent:VK_DoPresent;
	vk.max_anistophy_limit = 1.0;
	memset(&sh_config, 0, sizeof(sh_config));


	//get second set of pointers... (instance-level)
#ifdef VK_NO_PROTOTYPES
	if (!vkGetInstanceProcAddr)
	{
		Con_Printf("vkGetInstanceProcAddr is null\n");
		return false;
	}
#define VKFunc(n) vk##n = (PFN_vk##n)vkGetInstanceProcAddr(VK_NULL_HANDLE, "vk"#n);
	VKInstFuncs
#undef VKFunc
#endif

	//try and enable some instance extensions...
	{
		qboolean surfext = false;
		uint32_t count, i;
		VkExtensionProperties *ext;
		vkEnumerateInstanceExtensionProperties(NULL, &count, NULL);
		ext = malloc(sizeof(*ext)*count);
		vkEnumerateInstanceExtensionProperties(NULL, &count, ext);
		for (i = 0; i < count && extensions_count < countof(extensions); i++)
		{
			if (!strcmp(ext[i].extensionName, VK_EXT_DEBUG_REPORT_EXTENSION_NAME) && vk_debug.ival)
				extensions[extensions_count++] = VK_EXT_DEBUG_REPORT_EXTENSION_NAME;
			else if (!strcmp(ext[i].extensionName, VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME))
				extensions[extensions_count++] = VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME;
			else if (sysextname && !strcmp(ext[i].extensionName, sysextname))
			{
				extensions[extensions_count++] = sysextname;
				vk.khr_swapchain = true;
			}
			else if (sysextname && !strcmp(ext[i].extensionName, VK_KHR_SURFACE_EXTENSION_NAME))
			{
				extensions[extensions_count++] = VK_KHR_SURFACE_EXTENSION_NAME;
				surfext = true;
			}
		}
		free(ext);
		if (sysextname && (!vk.khr_swapchain || !surfext))
		{
			Con_Printf("Vulkan instance driver lacks support for %s\n", sysextname);
			return false;
		}
	}

#define ENGINEVERSION 1
	memset(&app, 0, sizeof(app));
	app.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	app.pNext = NULL;
	app.pApplicationName = NULL;
	app.applicationVersion = 0;
	app.pEngineName = FULLENGINENAME;
	app.engineVersion = ENGINEVERSION;
	app.apiVersion = VK_MAKE_VERSION(1, 0, 2);

	memset(&inst_info, 0, sizeof(inst_info));
	inst_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	inst_info.pApplicationInfo = &app;
	inst_info.enabledLayerCount = vklayercount;
	inst_info.ppEnabledLayerNames = vklayerlist;
	inst_info.enabledExtensionCount = extensions_count;
	inst_info.ppEnabledExtensionNames = extensions;

	err = vkCreateInstance(&inst_info, vkallocationcb, &vk.instance);
	switch(err)
	{
	case VK_ERROR_INCOMPATIBLE_DRIVER:
		Con_Printf("VK_ERROR_INCOMPATIBLE_DRIVER: please install an appropriate vulkan driver\n");
		return false;
	case VK_ERROR_EXTENSION_NOT_PRESENT:
		Con_Printf("VK_ERROR_EXTENSION_NOT_PRESENT: something on a system level is probably misconfigured\n");
		return false;
	case VK_ERROR_LAYER_NOT_PRESENT:
		Con_Printf("VK_ERROR_LAYER_NOT_PRESENT: requested layer is not known/usable\n");
		return false;
	default:
		Con_Printf("Unknown vulkan instance creation error: %x\n", err);
		return false;
	case VK_SUCCESS:
		break;
	}

	//third set of functions...
#ifdef VK_NO_PROTOTYPES
	vkGetInstanceProcAddr = (PFN_vkGetInstanceProcAddr)vkGetInstanceProcAddr(vk.instance, "vkGetInstanceProcAddr");
#define VKFunc(n) vk##n = (PFN_vk##n)vkGetInstanceProcAddr(vk.instance, "vk"#n);
	VKInst2Funcs
#undef VKFunc
#endif

	//set up debug callbacks
	if (vk_debug.ival)
	{
		vkCreateDebugReportCallbackEXT = (PFN_vkCreateDebugReportCallbackEXT)vkGetInstanceProcAddr(vk.instance, "vkCreateDebugReportCallbackEXT");
		vkDestroyDebugReportCallbackEXT = (PFN_vkDestroyDebugReportCallbackEXT)vkGetInstanceProcAddr(vk.instance, "vkDestroyDebugReportCallbackEXT");
		if (vkCreateDebugReportCallbackEXT && vkDestroyDebugReportCallbackEXT)
		{
			VkDebugReportCallbackCreateInfoEXT dbgCreateInfo;
			memset(&dbgCreateInfo, 0, sizeof(dbgCreateInfo));
			dbgCreateInfo.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CREATE_INFO_EXT;
			dbgCreateInfo.pfnCallback = mydebugreportcallback;
			dbgCreateInfo.pUserData = NULL;
			dbgCreateInfo.flags =	VK_DEBUG_REPORT_ERROR_BIT_EXT |
									VK_DEBUG_REPORT_WARNING_BIT_EXT	|
/*									VK_DEBUG_REPORT_INFORMATION_BIT_EXT	| */
									VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT |
									VK_DEBUG_REPORT_DEBUG_BIT_EXT;
			vkCreateDebugReportCallbackEXT(vk.instance, &dbgCreateInfo, vkallocationcb, &vk_debugcallback);
		}
	}

	//create the platform-specific surface
	createSurface();

	//figure out which gpu we're going to use
	{
		uint32_t gpucount = 0, i;
		uint32_t bestpri = ~0u, pri;
		VkPhysicalDevice *devs;
		vkEnumeratePhysicalDevices(vk.instance, &gpucount, NULL);
		if (!gpucount)
		{
			Con_Printf("vulkan: no devices known!\n");
			return false;
		}
		devs = malloc(sizeof(VkPhysicalDevice)*gpucount);
		vkEnumeratePhysicalDevices(vk.instance, &gpucount, devs);
		for (i = 0; i < gpucount; i++)
		{
			VkPhysicalDeviceProperties props;
			uint32_t j, queue_count;
			vkGetPhysicalDeviceProperties(devs[i], &props);
			vkGetPhysicalDeviceQueueFamilyProperties(devs[i], &queue_count, NULL);

			if (vk.khr_swapchain)
			{
				for (j = 0; j < queue_count; j++)
				{
					VkBool32 supportsPresent = false;
					VkAssert(vkGetPhysicalDeviceSurfaceSupportKHR(devs[i], j, vk.surface, &supportsPresent));
					if (supportsPresent)
						break;	//okay, this one should be usable
				}
				if (j == queue_count)
				{
					//no queues can present to that surface, so I guess we can't use that device
					Con_DPrintf("vulkan: ignoring device \"%s\" as it can't present to window\n", props.deviceName);
					continue;
				}
			}
			Con_DPrintf("Found Vulkan Device \"%s\"\n", props.deviceName);

			if (!vk.gpu)
				vk.gpu = devs[i];
			switch(props.deviceType)
			{
			default:
			case VK_PHYSICAL_DEVICE_TYPE_OTHER:
				pri = 5;
				break;
			case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU:
				pri = 2;
				break;
			case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:
				pri = 1;
				break;
			case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:
				pri = 3;
				break;
			case VK_PHYSICAL_DEVICE_TYPE_CPU:
				pri = 4;
				break;
			}
			if (!Q_strcasecmp(props.deviceName, info->subrenderer))
				pri = 0;
			if (pri < bestpri)
			{
				vk.gpu = devs[i];
				bestpri = pri;
			}
		}
		free(devs);

		if (bestpri == ~0u)
		{
			Con_Printf("vulkan: unable to pick a usable device\n");
			return false;
		}
	}

	{
		char *vendor, *type;
		VkPhysicalDeviceProperties props;
		vkGetPhysicalDeviceProperties(vk.gpu, &props);
	
		switch(props.vendorID)
		{
		//explicit vendors
		case 0x10001: vendor = "Vivante";		break;
		case 0x10002: vendor = "VeriSilicon";	break;

		//pci vendor ids
		//there's a lot of pci vendors, some even still exist, but not all of them actually have 3d hardware.
		//many of these probably won't even be used... Oh well.
		//anyway, here's some of the ones that are listed
		case 0x1002: vendor = "AMD";		break;
		case 0x10DE: vendor = "NVIDIA";		break;
		case 0x8086: vendor = "Intel";		break; //cute
		case 0x13B5: vendor = "ARM";		break;
		case 0x5143: vendor = "Qualcomm";	break;
		case 0x1AEE: vendor = "Imagination";break;
		case 0x1957: vendor = "Freescale";	break;

		case 0x1AE0: vendor = "Google";		break;
		case 0x5333: vendor = "S3";			break;
		case 0xA200: vendor = "NEC";		break;
		case 0x0A5C: vendor = "Broadcom";	break;
		case 0x1131: vendor = "NXP";		break;
		case 0x1099: vendor = "Samsung";	break;
		case 0x10C3: vendor = "Samsung";	break;
		case 0x11E2: vendor = "Samsung";	break;
		case 0x1249: vendor = "Samsung";	break;
		
		default:	vendor = va("VEND_%x", props.vendorID); break;
		}

		switch(props.deviceType)
		{
		default:
		case VK_PHYSICAL_DEVICE_TYPE_OTHER:				type = "(other)"; break;
		case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU:	type = "integrated"; break;
		case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:		type = "discrete"; break;
		case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:		type = "virtual"; break;
		case VK_PHYSICAL_DEVICE_TYPE_CPU:				type = "software"; break;
		}

		Con_Printf("Vulkan %u.%u.%u: %s %s %s (%u.%u.%u)\n", VK_VERSION_MAJOR(props.apiVersion), VK_VERSION_MINOR(props.apiVersion), VK_VERSION_PATCH(props.apiVersion),
			type, vendor, props.deviceName,
			VK_VERSION_MAJOR(props.driverVersion), VK_VERSION_MINOR(props.driverVersion), VK_VERSION_PATCH(props.driverVersion)
			);
	}

	//figure out which of the device's queue's we're going to use
	{
		uint32_t queue_count, i;
		vkGetPhysicalDeviceQueueFamilyProperties(vk.gpu, &queue_count, NULL);
		queueprops = malloc(sizeof(VkQueueFamilyProperties)*queue_count);	//Oh how I wish I was able to use C99.
		vkGetPhysicalDeviceQueueFamilyProperties(vk.gpu, &queue_count, queueprops);

		vk.queuefam[0] = ~0u;
		vk.queuefam[1] = ~0u;
		vk.queuenum[0] = 0;
		vk.queuenum[1] = 0;

		/*
		//try to find a 'dedicated' present queue
		for (i = 0; i < queue_count; i++)
		{
			VkBool32 supportsPresent = FALSE;
			VkAssert(vkGetPhysicalDeviceSurfaceSupportKHR(vk.gpu, i, vk.surface, &supportsPresent));

			if (supportsPresent && !(queueprops[i].queueFlags & VK_QUEUE_GRAPHICS_BIT))
			{
				vk.queuefam[1] = i;
				break;
			}
		}

		if (vk.queuefam[1] != ~0u)
		{	//try to find a good graphics queue
			for (i = 0; i < queue_count; i++)
			{
				if (queueprops[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
				{
					vk.queuefam[0] = i;
					break;
				}
			}
		}
		else*/
		{
			for (i = 0; i < queue_count; i++)
			{
				VkBool32 supportsPresent = false;
				if (!vk.khr_swapchain)
					supportsPresent = true;	//won't be used anyway.
				else
					VkAssert(vkGetPhysicalDeviceSurfaceSupportKHR(vk.gpu, i, vk.surface, &supportsPresent));

				if ((queueprops[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) && supportsPresent)
				{
					vk.queuefam[0] = i;
					vk.queuefam[1] = i;
					break;
				}
				else if (vk.queuefam[0] == ~0u && (queueprops[i].queueFlags & VK_QUEUE_GRAPHICS_BIT))
					vk.queuefam[0] = i;
				else if (vk.queuefam[1] == ~0u && supportsPresent)
					vk.queuefam[1] = i;
			}
		}


		if (vk.queuefam[0] == ~0u || vk.queuefam[1] == ~0u)
		{
			free(queueprops);
			Con_Printf("unable to find suitable queues\n");
			return false;
		}
	}

	{
		uint32_t extcount = 0;
		VkExtensionProperties *ext;
		vkEnumerateDeviceExtensionProperties(vk.gpu, NULL, &extcount, NULL);
		ext = malloc(sizeof(*ext)*extcount);
		vkEnumerateDeviceExtensionProperties(vk.gpu, NULL, &extcount, ext);
		while (extcount --> 0)
		{
			for (e = 0; e < countof(knowndevexts); e++)
			{
				if (!strcmp(ext[extcount].extensionName, knowndevexts[e].name))
				{
					if (knowndevexts[e].var)
						*knowndevexts[e].flag = !!knowndevexts[e].var->ival || (!*knowndevexts[e].var->string && knowndevexts[e].def);
					knowndevexts[e].supported = true;
				}
			}
		}
		free(ext);
	}
	{
		const char *devextensions[1+countof(knowndevexts)];
		size_t numdevextensions = 0;
		float queue_priorities[2] = {0.8, 1.0};
		VkDeviceQueueCreateInfo queueinf[2] = {{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO},{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO}};
		VkDeviceCreateInfo devinf = {VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
		VkPhysicalDeviceFeatures features;
		VkPhysicalDeviceFeatures avail;
		memset(&features, 0, sizeof(features));

		vkGetPhysicalDeviceFeatures(vk.gpu, &avail);

		//try to enable whatever we can use, if we can.
		features.robustBufferAccess		= avail.robustBufferAccess;
		features.textureCompressionBC	= avail.textureCompressionBC;
		features.samplerAnisotropy		= avail.samplerAnisotropy;
		features.geometryShader			= avail.geometryShader;
		features.tessellationShader		= avail.tessellationShader;

		//Add in the extensions we support
		for (e = 0; e < countof(knowndevexts); e++)
		{	//prints are to let the user know what's going on. only warn if its explicitly enabled
			if (knowndevexts[e].superseeded && *knowndevexts[e].superseeded)
			{
				Con_DPrintf("Superseeded %s.\n", knowndevexts[e].name);
				*knowndevexts[e].flag = false;
			}
			else if (*knowndevexts[e].flag)
			{
				Con_DPrintf("Using %s.\n", knowndevexts[e].name);
				devextensions[numdevextensions++] = knowndevexts[e].name;
			}
			else if (knowndevexts[e].var->ival)
				Con_Printf("unable to enable %s extension.%s\n", knowndevexts[e].name, knowndevexts[e].neededfor);
			else if (knowndevexts[e].supported)
				Con_DPrintf("Ignoring %s.\n", knowndevexts[e].name);
			else
				Con_DPrintf("Unavailable %s.\n", knowndevexts[e].name);
		}

		queueinf[0].pNext = NULL;
		queueinf[0].queueFamilyIndex = vk.queuefam[0];
		queueinf[0].queueCount = 1;
		queueinf[0].pQueuePriorities = &queue_priorities[0];
		queueinf[1].pNext = NULL;
		queueinf[1].queueFamilyIndex = vk.queuefam[1];
		queueinf[1].queueCount = 1;
		queueinf[1].pQueuePriorities = &queue_priorities[1];

		if (vk.queuefam[0] == vk.queuefam[1])
		{
			devinf.queueCreateInfoCount = 1;

			if (queueprops[queueinf[0].queueFamilyIndex].queueCount >= 2 && vk_dualqueue.ival)
			{
				queueinf[0].queueCount = 2;
				vk.queuenum[1] = 1; 
				Con_DPrintf("Using duel queue\n");
			}
			else
			{
				queueinf[0].queueCount = 1;
				if (vk.khr_swapchain)
					vk.dopresent = VK_DoPresent;	//can't split submit+present onto different queues, so do these on a single thread.
				Con_DPrintf("Using single queue\n");
			}
		}
		else
		{
			devinf.queueCreateInfoCount = 2;
			Con_DPrintf("Using separate queue families\n");
		}

		free(queueprops);

		devinf.pQueueCreateInfos = queueinf;
		devinf.enabledLayerCount = vklayercount;
		devinf.ppEnabledLayerNames = vklayerlist;
		devinf.enabledExtensionCount = numdevextensions;
		devinf.ppEnabledExtensionNames = devextensions;
		devinf.pEnabledFeatures = &features;

		err = vkCreateDevice(vk.gpu, &devinf, NULL, &vk.device);
		switch(err)
		{
		case VK_ERROR_INCOMPATIBLE_DRIVER:
			Con_Printf("VK_ERROR_INCOMPATIBLE_DRIVER: please install an appropriate vulkan driver\n");
			return false;
		case VK_ERROR_EXTENSION_NOT_PRESENT:
			Con_Printf("VK_ERROR_EXTENSION_NOT_PRESENT: something on a system level is probably misconfigured\n");
			return false;
		default:
			Con_Printf("Unknown vulkan device creation error: %x\n", err);
			return false;
		case VK_SUCCESS:
			break;
		}
	}

#ifdef VK_NO_PROTOTYPES
	vkGetDeviceProcAddr = (PFN_vkGetDeviceProcAddr)vkGetInstanceProcAddr(vk.instance, "vkGetDeviceProcAddr");
#define VKFunc(n) vk##n = (PFN_vk##n)vkGetDeviceProcAddr(vk.device, "vk"#n);
	VKDevFuncs
#undef VKFunc
#endif

	vkGetDeviceQueue(vk.device, vk.queuefam[0], vk.queuenum[0], &vk.queue_render);
	vkGetDeviceQueue(vk.device, vk.queuefam[1], vk.queuenum[1], &vk.queue_present);


	vkGetPhysicalDeviceMemoryProperties(vk.gpu, &vk.memory_properties);

	{
		VkCommandPoolCreateInfo cpci = {VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
		cpci.queueFamilyIndex = vk.queuefam[0];
		cpci.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT|VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
		VkAssert(vkCreateCommandPool(vk.device, &cpci, vkallocationcb, &vk.cmdpool));
	}

	
	sh_config.progpath = NULL;
	sh_config.blobpath = "spirv";
	sh_config.shadernamefmt = NULL;//".spv";

	if (vk.nv_glsl_shader)
	{
		sh_config.progpath = "glsl/%s.glsl";
		sh_config.shadernamefmt = "%s_glsl";
	}

	sh_config.progs_supported = true;
	sh_config.progs_required = true;
	sh_config.minver = -1;
	sh_config.maxver = -1;

	sh_config.texture_maxsize = 4096;		//must be at least 4096, FIXME: query this properly
	sh_config.texture_non_power_of_two = true;	//is this always true?
	sh_config.texture_non_power_of_two_pic = true;	//probably true...
	sh_config.npot_rounddown = false;
	sh_config.tex_env_combine = false;		//fixme: figure out what this means...
	sh_config.nv_tex_env_combine4 = false;	//fixme: figure out what this means...
	sh_config.env_add = false;				//fixme: figure out what this means...

	sh_config.can_mipcap = true;
	sh_config.havecubemaps = true;

	VK_CheckTextureFormats();


	sh_config.pDeleteProg = NULL;
	sh_config.pLoadBlob = NULL;
	if (vk.nv_glsl_shader)
		sh_config.pCreateProgram = VK_LoadGLSL;
	else
		sh_config.pCreateProgram = NULL;
	sh_config.pValidateProgram = NULL;
	sh_config.pProgAutoFields = NULL;

	if (sh_config.texfmt[PTI_DEPTH32])
		vk.depthformat = VK_FORMAT_D32_SFLOAT;
	else if (sh_config.texfmt[PTI_DEPTH24])
		vk.depthformat = VK_FORMAT_X8_D24_UNORM_PACK32;
	else if (sh_config.texfmt[PTI_DEPTH24_8])
		vk.depthformat = VK_FORMAT_D24_UNORM_S8_UINT;
	else	//16bit depth is guarenteed in vulkan
		vk.depthformat = VK_FORMAT_D16_UNORM;

	vk.submitcondition = Sys_CreateConditional();

	{
		VkPipelineCacheCreateInfo pci = {VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO};
		qofs_t size = 0;
		pci.pInitialData = FS_MallocFile("vulkan.pcache", FS_ROOT, &size);
		pci.initialDataSize = size;
		VkAssert(vkCreatePipelineCache(vk.device, &pci, vkallocationcb, &vk.pipelinecache));
		FS_FreeFile((void*)pci.pInitialData);
	}

	if (VK_CreateSwapChain())
	{
		vk.neednewswapchain = false;

		if (vk.allowsubmissionthread && (vk_submissionthread.ival || !*vk_submissionthread.string))
		{
			vk.submitthread = Sys_CreateThread("vksubmission", VK_Submit_Thread, NULL, THREADP_HIGHEST, 0);
		}
	}
	return true;
}
void VK_Shutdown(void)
{
	uint32_t i;

	VK_DestroySwapChain();

	for (i = 0; i < countof(postproc); i++)
		VKBE_RT_Gen(&postproc[i], 0, 0, false, RT_IMAGEFLAGS);
	VKBE_RT_Gen_Cube(&vk_rt_cubemap, 0, false);
	VK_R_BloomShutdown();

	if (vk.cmdpool)
		vkDestroyCommandPool(vk.device, vk.cmdpool, vkallocationcb);
	VK_DestroyRenderPass();

	if (vk.pipelinecache)
	{
		size_t size;
		if (VK_SUCCESS == vkGetPipelineCacheData(vk.device, vk.pipelinecache, &size, NULL))
		{
			void *ptr = Z_Malloc(size);	//valgrind says nvidia isn't initialising this.
			if (VK_SUCCESS == vkGetPipelineCacheData(vk.device, vk.pipelinecache, &size, ptr))
				FS_WriteFile("vulkan.pcache", ptr, size, FS_ROOT);
			Z_Free(ptr);
		}
		vkDestroyPipelineCache(vk.device, vk.pipelinecache, vkallocationcb);
	}

	if (vk.device)
		vkDestroyDevice(vk.device, vkallocationcb);
	if (vk_debugcallback)
	{
		vkDestroyDebugReportCallbackEXT(vk.instance, vk_debugcallback, vkallocationcb);
		vk_debugcallback = VK_NULL_HANDLE;
	}

	if (vk.surface)
		vkDestroySurfaceKHR(vk.instance, vk.surface, vkallocationcb);
	if (vk.instance)
		vkDestroyInstance(vk.instance, vkallocationcb);
	if (vk.submitcondition)
		Sys_DestroyConditional(vk.submitcondition);

	memset(&vk, 0, sizeof(vk));

#ifdef VK_NO_PROTOTYPES
	#define VKFunc(n) vk##n = NULL;
	VKFuncs
	#undef VKFunc
#endif

}
#endif
