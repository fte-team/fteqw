#include "quakedef.h"
#ifdef VKQUAKE
#include "vkrenderer.h"
#include "gl_draw.h"
#include "shader.h"
#include "renderque.h"	//is anything still using this?

extern qboolean vid_isfullscreen;
extern cvar_t vk_submissionthread;
extern cvar_t vk_debug;
extern cvar_t vid_srgb, vid_vsync, vid_triplebuffer, r_stereo_method;
void R2D_Console_Resize(void);

const char *vklayerlist[] =
{
#if 1
	"VK_LAYER_LUNARG_standard_validation"
#else
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
		
struct vulkaninfo_s vk;
static struct vk_rendertarg postproc[2];
static unsigned int postproc_buf;

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

	Sys_LockConditional(vk.submitcondition);
	vk.neednewswapchain = true;
	Sys_ConditionSignal(vk.submitcondition);
	Sys_UnlockConditional(vk.submitcondition);
	if (vk.submitthread)
	{
		Sys_WaitOnThread(vk.submitthread);
		vk.submitthread = NULL;
	}
#ifdef THREADACQUIRE
	while (vk.aquirenext < vk.aquirelast)
	{
		VkAssert(vkWaitForFences(vk.device, 1, &vk.acquirefences[vk.aquirenext%ACQUIRELIMIT], VK_FALSE, UINT64_MAX));
		vk.aquirenext++;
	}
#endif
	while (vk.work)
	{
		Sys_LockConditional(vk.submitcondition);
		VK_Submit_DoWork();
		Sys_UnlockConditional(vk.submitcondition);
	}
	vkDeviceWaitIdle(vk.device);
	VK_FencedCheck();
	while(vk.frameendjobs)
	{	//we've fully synced the gpu now, we can clean up any resources that were pending but not assigned yet.
		struct vk_fencework *job = vk.frameendjobs;
		vk.frameendjobs = job->next;
		job->Passed(job);
		if (job->fence || job->cbuf)
			Con_Printf("job with junk\n");
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
	}

#ifdef THREADACQUIRE
	while (vk.aquirenext < vk.aquirelast)
	{
		VkAssert(vkWaitForFences(vk.device, 1, &vk.acquirefences[vk.aquirenext%ACQUIRELIMIT], VK_FALSE, UINT64_MAX));
		vk.aquirenext++;
	}
	for (i = 0; i < ACQUIRELIMIT; i++)
	{
		if (vk.acquirefences[i])
			vkDestroyFence(vk.device, vk.acquirefences[i], vkallocationcb);
		vk.acquirefences[i] = VK_NULL_HANDLE;
	}
#endif

	while(vk.unusedframes)
	{
		struct vkframe *frame = vk.unusedframes;
		vk.unusedframes = frame->next;

		VKBE_ShutdownFramePools(frame);

		vkResetCommandBuffer(frame->cbuf, VK_COMMAND_BUFFER_RESET_RELEASE_RESOURCES_BIT);
		vkFreeCommandBuffers(vk.device, vk.cmdpool, 1, &frame->cbuf);
#ifndef THREADACQUIRE
		vkDestroySemaphore(vk.device, frame->vsyncsemaphore, vkallocationcb);
#endif
		vkDestroySemaphore(vk.device, frame->presentsemaphore, vkallocationcb);
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
	VkImageView attachments[2];
	VkFramebufferCreateInfo fb_info = {VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};

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

	swapinfo.imageColorSpace = VK_COLORSPACE_SRGB_NONLINEAR_KHR;
	swapinfo.imageFormat = vid_srgb.ival?VK_FORMAT_B8G8R8A8_SRGB:VK_FORMAT_B8G8R8A8_UNORM;
	for (i = 0, curpri = 0; i < fmtcount; i++)
	{
		uint32_t priority = 0;
		switch(surffmts[i].format)
		{
		case VK_FORMAT_B8G8R8A8_UNORM:
		case VK_FORMAT_R8G8B8A8_UNORM:
			priority = 4+!vid_srgb.ival;
			break;
		case VK_FORMAT_B8G8R8A8_SRGB:
		case VK_FORMAT_R8G8B8A8_SRGB:
			priority = 4+!!vid_srgb.ival;
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
	VkAssert(vkGetSwapchainImagesKHR(vk.device, vk.swapchain, &vk.backbuf_count, images));

#ifdef THREADACQUIRE
	vk.aquirelast = vk.aquirenext = 0;
	for (i = 0; i < ACQUIRELIMIT; i++)
	{
		VkFenceCreateInfo fci = {VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
		VkAssert(vkCreateFence(vk.device,&fci,vkallocationcb,&vk.acquirefences[i]));
	}
	/*-1 to hide any weird thread issues*/
	while (vk.aquirelast < ACQUIRELIMIT-1 && vk.aquirelast < vk.backbuf_count && vk.aquirelast < 2 && vk.aquirelast <= vk.backbuf_count-surfcaps.minImageCount)
	{
		VkAssert(vkAcquireNextImageKHR(vk.device, vk.swapchain, UINT64_MAX, VK_NULL_HANDLE, vk.acquirefences[vk.aquirelast%ACQUIRELIMIT], &vk.acquirebufferidx[vk.aquirelast%ACQUIRELIMIT]));
		vk.aquirelast++;
	}
#endif

	VK_CreateRenderPass();
	if (reloadshaders)
	{
		Shader_NeedReload(true);
		Shader_DoReload();
	}

	attachments[1] = VK_NULL_HANDLE;
	attachments[0] = VK_NULL_HANDLE;

	fb_info.renderPass = vk.renderpass[0];
	fb_info.attachmentCount = countof(attachments);
	fb_info.pAttachments = attachments;
	fb_info.width = swapinfo.imageExtent.width;
	fb_info.height = swapinfo.imageExtent.height;
	fb_info.layers = 1;


	vk.backbufs = malloc(sizeof(*vk.backbufs)*vk.backbuf_count);
	memset(vk.backbufs, 0, sizeof(*vk.backbufs)*vk.backbuf_count);
	for (i = 0; i < vk.backbuf_count; i++)
	{
		VkImageViewCreateInfo ivci = {VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
		ivci.format = swapinfo.imageFormat;
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
		VkAssert(vkCreateImageView(vk.device, &ivci, vkallocationcb, &vk.backbufs[i].colour.view));

		{
			VkImageCreateInfo depthinfo = {VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
			depthinfo.flags = 0;
			depthinfo.imageType = VK_IMAGE_TYPE_2D;
			depthinfo.format = vk.depthformat;
			depthinfo.extent.width = swapinfo.imageExtent.width;
			depthinfo.extent.height = swapinfo.imageExtent.height;
			depthinfo.extent.depth = 1;
			depthinfo.mipLevels = 1;
			depthinfo.arrayLayers = 1;
			depthinfo.samples = VK_SAMPLE_COUNT_1_BIT;
			depthinfo.tiling = VK_IMAGE_TILING_OPTIMAL;
			depthinfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
			depthinfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
			depthinfo.queueFamilyIndexCount = 0;
			depthinfo.pQueueFamilyIndices = NULL;
			depthinfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			VkAssert(vkCreateImage(vk.device, &depthinfo, vkallocationcb, &vk.backbufs[i].depth.image));
		}

		{
			VkMemoryRequirements mem_reqs;
			VkMemoryAllocateInfo memAllocInfo = {VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
			vkGetImageMemoryRequirements(vk.device, vk.backbufs[i].depth.image, &mem_reqs);
			memAllocInfo.allocationSize = mem_reqs.size;
			memAllocInfo.memoryTypeIndex = vk_find_memory_require(mem_reqs.memoryTypeBits, 0);
			VkAssert(vkAllocateMemory(vk.device, &memAllocInfo, vkallocationcb, &vk.backbufs[i].depth.memory));
			VkAssert(vkBindImageMemory(vk.device, vk.backbufs[i].depth.image, vk.backbufs[i].depth.memory, 0));
		}

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


		attachments[0] = vk.backbufs[i].colour.view;
		VkAssert(vkCreateFramebuffer(vk.device, &fb_info, vkallocationcb, &vk.backbufs[i].framebuffer));
	}
	free(images);

	vid.pixelwidth = swapinfo.imageExtent.width;
	vid.pixelheight = swapinfo.imageExtent.height;
	R2D_Console_Resize();
	return true;
}

	
void	VK_Draw_Init(void)
{
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

void VK_UpdateFiltering(image_t *imagelist, int filtermip[3], int filterpic[3], int mipcap[2], float anis)
{
	uint32_t i;
	for (i = 0; i < countof(vk.filtermip); i++)
		vk.filtermip[i] = filtermip[i];
	for (i = 0; i < countof(vk.filterpic); i++)
		vk.filterpic[i] = filterpic[i];
	for (i = 0; i < countof(vk.mipcap); i++)
		vk.mipcap[i] = mipcap[i];
	vk.max_anistophy = anis;

	vkDeviceWaitIdle(vk.device);
	while(imagelist)
	{
		if (imagelist->vkimage)
			VK_CreateSampler(imagelist->flags, imagelist->vkimage);
		imagelist = imagelist->next;
	}
}

vk_image_t VK_CreateTexture2DArray(uint32_t width, uint32_t height, uint32_t layers, uint32_t mips, unsigned int encoding, unsigned int type)
{
	vk_image_t ret;
	qboolean staging = layers == 0;
	VkMemoryRequirements mem_reqs;
	VkMemoryAllocateInfo memAllocInfo = {VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
	VkImageCreateInfo ici = {VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
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
void *VK_AtFrameEnd(void (*passed)(void *work), size_t worksize)
{
	//FIXME: OMG! BIG LEAK!
	struct vk_fencework *w = Z_Malloc(worksize?worksize:sizeof(*w));

	w->Passed = passed;
	w->next = vk.frameendjobs;
	vk.frameendjobs = w;

	return w;
}

struct texturefence
{
	struct vk_fencework w;

	int mips;
	vk_image_t staging[32];
};
static void VK_TextureLoaded(void *ctx)
{
	struct texturefence *w = ctx;
	unsigned int i;
	for (i = 0; i < w->mips; i++)
		if (w->staging[i].image != VK_NULL_HANDLE)
		{
			vkDestroyImage(vk.device, w->staging[i].image, vkallocationcb);
			vkFreeMemory(vk.device, w->staging[i].memory, vkallocationcb);
		}
}
qboolean VK_LoadTextureMips (texid_t tex, struct pendingtextureinfo *mips)
{
	struct texturefence *fence;
	VkCommandBuffer vkloadcmd;
	vk_image_t target;
	uint32_t i, y;
	uint32_t blocksize;
	uint32_t blockbytes;
	uint32_t layers;
	if (mips->type != PTI_2D && mips->type != PTI_CUBEMAP)
		return false;
	if (!mips->mipcount || mips->mip[0].width == 0 || mips->mip[0].height == 0)
		return false;

	layers = (mips->type == PTI_CUBEMAP)?6:1;

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
	fence->mips = mips->mipcount;
	vkloadcmd = fence->w.cbuf;

	//create our target image

	if (tex->vkimage)
	{
		if (tex->vkimage->width != mips->mip[0].width ||
			tex->vkimage->height != mips->mip[0].height ||
			tex->vkimage->layers != layers ||
			tex->vkimage->mipcount != mips->mipcount ||
			tex->vkimage->encoding != mips->encoding ||
			tex->vkimage->type != mips->type)
		{
			vkDeviceWaitIdle(vk.device);	//erk, we can't cope with a commandbuffer poking the texture while things happen
			VK_FencedCheck();
			VK_DestroyVkTexture(tex->vkimage);
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
			imgbarrier.subresourceRange.levelCount = mips->mipcount/layers;
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
		target = VK_CreateTexture2DArray(mips->mip[0].width, mips->mip[0].height, layers, mips->mipcount/layers, mips->encoding, mips->type);

		{
			//images have weird layout representations.
			//we need to use a side-effect of memory barriers in order to convert from one layout to another, so that we can actually use the image.
			VkImageMemoryBarrier imgbarrier = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
			imgbarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			imgbarrier.newLayout = target.layout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
			imgbarrier.image = target.image;
			imgbarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			imgbarrier.subresourceRange.baseMipLevel = 0;
			imgbarrier.subresourceRange.levelCount = mips->mipcount/layers;
			imgbarrier.subresourceRange.baseArrayLayer = 0;
			imgbarrier.subresourceRange.layerCount = layers;
			imgbarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			imgbarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

			imgbarrier.srcAccessMask = 0;
			imgbarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			vkCmdPipelineBarrier(vkloadcmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 0, 0, NULL, 0, NULL, 1, &imgbarrier);
		}
	}

	//create the staging images and fill them
	for (i = 0; i < mips->mipcount; i++)
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
			region.dstSubresource.mipLevel = i%(mips->mipcount/layers);
			region.dstSubresource.baseArrayLayer = i/(mips->mipcount/layers);
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
		imgbarrier.subresourceRange.levelCount = mips->mipcount/layers;
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
	VK_DestroySwapChain();
	VKBE_Shutdown();
	Shader_Shutdown();
	Image_Shutdown();
}

void VK_SetupViewPortProjection(void)
{
	extern cvar_t gl_mindist;
	int		x, x2, y2, y, w, h;

	float fov_x, fov_y;

	AngleVectors (r_refdef.viewangles, vpn, vright, vup);
	VectorCopy (r_refdef.vieworg, r_origin);

	//
	// set up viewpoint
	//
	x = r_refdef.vrect.x * vid.pixelwidth/(int)vid.width;
	x2 = (r_refdef.vrect.x + r_refdef.vrect.width) * vid.pixelwidth/(int)vid.width;
	y = (r_refdef.vrect.y) * vid.pixelheight/(int)vid.height;
	y2 = ((int)(r_refdef.vrect.y + r_refdef.vrect.height)) * vid.pixelheight/(int)vid.height;

	// fudge around because of frac screen scale
	if (x > 0)
		x--;
	if (x2 < vid.pixelwidth)
		x2++;
	if (y < 0)
		y--;
	if (y2 < vid.pixelheight)
		y2++;

	w = x2 - x;
	h = y2 - y;

	r_refdef.pxrect.x = x;
	r_refdef.pxrect.y = y;
	r_refdef.pxrect.width = w;
	r_refdef.pxrect.height = h;

	fov_x = r_refdef.fov_x;//+sin(cl.time)*5;
	fov_y = r_refdef.fov_y;//-sin(cl.time+1)*5;

	if ((r_refdef.flags & RDF_UNDERWATER) && !(r_refdef.flags & RDF_WATERWARP))
	{
		fov_x *= 1 + (((sin(cl.time * 4.7) + 1) * 0.015) * r_waterwarp.value);
		fov_y *= 1 + (((sin(cl.time * 3.0) + 1) * 0.015) * r_waterwarp.value);
	}

//	screenaspect = (float)r_refdef.vrect.width/r_refdef.vrect.height;

	/*view matrix*/
	Matrix4x4_CM_ModelViewMatrixFromAxis(r_refdef.m_view, vpn, vright, vup, r_refdef.vieworg);
	Matrix4x4_CM_Projection_Inf(r_refdef.m_projection, fov_x, fov_y, bound(0.1, gl_mindist.value, 4));
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
		vkCmdSetViewport(vk.frame->cbuf, 0, countof(vp), vp);
		vkCmdSetScissor(vk.frame->cbuf, 0, countof(scissor), scissor);
	}

	VKBE_Set2D(true);

	if (0)
		Matrix4x4_CM_Orthographic(r_refdef.m_projection, 0, vid.fbvwidth, 0, vid.fbvheight, -99999, 99999);
	else
		Matrix4x4_CM_Orthographic(r_refdef.m_projection, 0, vid.fbvwidth, vid.fbvheight, 0, -99999, 99999);
	Matrix4x4_Identity(r_refdef.m_view);

	BE_SelectEntity(&r_worldentity);
}

void VK_Init_PostProc(void)
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
}


void	VK_R_RenderView				(void)
{
	extern unsigned int r_viewcontents;
	struct vk_rendertarg *rt;

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

	VK_SetupViewPortProjection();

	//FIXME: RDF_BLOOM|RDF_FISHEYE|RDF_CUSTOMPOSTPROC|RDF_ANTIALIAS|RDF_RENDERSCALE

	if (r_refdef.flags & RDF_ALLPOSTPROC)
	{
		rt = &postproc[postproc_buf++%countof(postproc)];
		if (rt->width != r_refdef.pxrect.width || rt->height != r_refdef.pxrect.height)
			VKBE_RT_Gen(rt, r_refdef.pxrect.width, r_refdef.pxrect.height);
		VKBE_RT_Begin(rt, rt->width, rt->height);
	}
	else
		rt = NULL;

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
		vkCmdSetViewport(vk.frame->cbuf, 0, countof(vp), vp);
		vkCmdSetScissor(vk.frame->cbuf, 0, countof(scissor), scissor);
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
		vkCmdClearAttachments(vk.frame->cbuf, 1, &clr, 1, &rect);
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

	if (r_refdef.flags & RDF_ALLPOSTPROC)
	{
		if (!vk.scenepp_waterwarp)
			VK_Init_PostProc();
		//FIXME: chain renderpasses as required.
		if (r_refdef.flags & RDF_WATERWARP)
		{
			r_refdef.flags &= ~RDF_WATERWARP;
			VKBE_RT_End();	//WARNING: redundant begin+end renderpasses.
			vk.sourcecolour = &rt->q_colour;
			vk.sourcecolour = &rt->q_colour;
			if (r_refdef.flags & RDF_ALLPOSTPROC)
			{
				rt = &postproc[postproc_buf++];
				VKBE_RT_Begin(rt, 320, 240);
			}

			R2D_Image(r_refdef.vrect.x, r_refdef.vrect.y, r_refdef.vrect.width, r_refdef.vrect.height, 0, 0, 1, 1, vk.scenepp_waterwarp);
			R2D_Flush();
		}
	}

	vk.sourcecolour = r_nulltex;
}

char	*VKVID_GetRGBInfo			(int *truevidwidth, int *truevidheight, enum uploadfmt *fmt)
{
	//with vulkan, we need to create a staging image to write into, submit a copy, wait for completion, map the copy, copy that out, free the staging.
	//its enough to make you pitty anyone that writes opengl drivers.
	if (VK_SCR_GrabBackBuffer())
	{
		void *imgdata, *outdata;
		uint32_t y;
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

		set_image_layout(fence->cbuf, vk.frame->backbuf->colour.image, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_ACCESS_TRANSFER_READ_BIT, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT);
		set_image_layout(fence->cbuf, tempimage, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_HOST_READ_BIT);

		VK_FencedSync(fence);

		outdata = BZ_Malloc(4*vid.pixelwidth*vid.pixelheight);
		*fmt = PTI_BGRA8;
		*truevidwidth = vid.pixelwidth;
		*truevidheight = vid.pixelheight;

		subres.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		subres.mipLevel = 0;
		subres.arrayLayer = 0;
		vkGetImageSubresourceLayout(vk.device, tempimage, &subres, &layout);
		VkAssert(vkMapMemory(vk.device, tempmemory, 0, mem_reqs.size, 0, &imgdata));
		for (y = 0; y < vid.pixelheight; y++)
			memcpy((char*)outdata + (vid.pixelheight-1-y)*vid.pixelwidth*4, (char*)imgdata + layout.offset + y*layout.rowPitch, vid.pixelwidth*4);
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

qboolean VK_SCR_GrabBackBuffer(void)
{
	RSpeedLocals();
#ifndef THREADACQUIRE
	VkResult err;
#endif

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

#ifdef THREADACQUIRE
	while (vk.aquirenext == vk.aquirelast)
	{	//we're still waiting for the render thread to increment acquirelast.
		if (vk.vsync)
			Sys_Sleep(0);	//o.O
	}

	//wait for the queued acquire to actually finish
	if (vk.vsync)
	{
		//friendly wait
		VkAssert(vkWaitForFences(vk.device, 1, &vk.acquirefences[vk.aquirenext%ACQUIRELIMIT], VK_FALSE, UINT64_MAX));
	}
	else
	{	//busy wait, to try to get the highest fps possible
		while (VK_TIMEOUT == vkGetFenceStatus(vk.device, vk.acquirefences[vk.aquirenext%ACQUIRELIMIT]))
				;
	}
	vk.bufferidx = vk.acquirebufferidx[vk.aquirenext%ACQUIRELIMIT];
	VkAssert(vkResetFences(vk.device, 1, &vk.acquirefences[vk.aquirenext%ACQUIRELIMIT]));
	vk.aquirenext++;
#else
	Sys_LockMutex(vk.swapchain_mutex);
	err = vkAcquireNextImageKHR(vk.device, vk.swapchain, UINT64_MAX, vk.unusedframes->vsyncsemaphore, vk.acquirefence, &vk.bufferidx);
	Sys_UnlockMutex(vk.swapchain_mutex);
	switch(err)
	{
	case VK_ERROR_OUT_OF_DATE_KHR:
		vk.neednewswapchain = true;
		return false;
	case VK_SUBOPTIMAL_KHR:
		vk.neednewswapchain = true;
		break;	//this is still a success
	case VK_SUCCESS:
		break;	//yay
	case VK_ERROR_SURFACE_LOST_KHR:
		//window was destroyed.
		//shouldn't really happen...
		return false;
	case VK_NOT_READY:	//VK_NOT_READY is returned if timeout is zero and no image was available. (timeout is not 0)
		RSpeedEnd(RSPEED_SETUP);
		Con_DPrintf("vkAcquireNextImageKHR: unexpected VK_NOT_READY\n");
		return false;	//timed out
	case VK_TIMEOUT:	//VK_TIMEOUT is returned if timeout is greater than zero and less than UINT64_MAX, and no image became available within the time allowed. 
		RSpeedEnd(RSPEED_SETUP);
		Con_DPrintf("vkAcquireNextImageKHR: unexpected VK_TIMEOUT\n");
		return false;	//timed out
	default:
		Sys_Error("vkAcquireNextImageKHR == %i", err);
		break;
	}
#endif

	//grab the first unused
	Sys_LockConditional(vk.submitcondition);
	vk.frame = vk.unusedframes;
	vk.unusedframes = vk.frame->next;
	vk.frame->next = NULL;
	Sys_UnlockConditional(vk.submitcondition);

	VkAssert(vkResetFences(vk.device, 1, &vk.frame->finishedfence));

	vk.frame->backbuf = &vk.backbufs[vk.bufferidx];

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
		vkBeginCommandBuffer(vk.frame->cbuf, &begininf);
	}

	VKBE_RestartFrame();

//	VK_DebugFramerate();

//	vkCmdWriteTimestamp(vk.frame->cbuf, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, querypool, vk.bufferidx*2+0);

	{
		VkImageMemoryBarrier imgbarrier = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
		imgbarrier.pNext = NULL;
		imgbarrier.srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
		imgbarrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		imgbarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;// VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;	//'Alternately, oldLayout can be VK_IMAGE_LAYOUT_UNDEFINED, if the image’s contents need not be preserved.'
		imgbarrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		imgbarrier.image = vk.frame->backbuf->colour.image;
		imgbarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		imgbarrier.subresourceRange.baseMipLevel = 0;
		imgbarrier.subresourceRange.levelCount = 1;
		imgbarrier.subresourceRange.baseArrayLayer = 0;
		imgbarrier.subresourceRange.layerCount = 1;
		imgbarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		imgbarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		vkCmdPipelineBarrier(vk.frame->cbuf, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0, 0, NULL, 0, NULL, 1, &imgbarrier);
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
		vkCmdPipelineBarrier(vk.frame->cbuf, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0, 0, NULL, 0, NULL, 1, &imgbarrier);
	}

	{
		VkClearValue	clearvalues[2];
		extern cvar_t r_clear;
		VkRenderPassBeginInfo rpbi = {VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};

		clearvalues[0].color.float32[0] = !!(r_clear.ival & 1);
		clearvalues[0].color.float32[1] = !!(r_clear.ival & 2);
		clearvalues[0].color.float32[2] = !!(r_clear.ival & 4);
		clearvalues[0].color.float32[3] = 1;
		clearvalues[1].depthStencil.depth = 1.0;
		clearvalues[1].depthStencil.stencil = 0;
		rpbi.clearValueCount = 2;

		if (r_clear.ival)
			rpbi.renderPass = vk.renderpass[2];
		else
			rpbi.renderPass = vk.renderpass[1];
		rpbi.framebuffer = vk.frame->backbuf->framebuffer;
		rpbi.renderArea.offset.x = 0;
		rpbi.renderArea.offset.y = 0;
		rpbi.renderArea.extent.width = vid.pixelwidth;
		rpbi.renderArea.extent.height = vid.pixelheight;
		rpbi.pClearValues = clearvalues;
		vkCmdBeginRenderPass(vk.frame->cbuf, &rpbi, VK_SUBPASS_CONTENTS_INLINE);

		rpbi.clearValueCount = 0;
		rpbi.pClearValues = NULL;
		rpbi.renderPass = vk.renderpass[0];
		vk.rendertarg = vk.frame->backbuf;
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
		struct vk_fencework *job = frame->frameendjobs;
		frame->frameendjobs = job->next;
		job->Passed(job);
		if (job->fence || job->cbuf)
			Con_Printf("job with junk\n");
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
	RSpeedLocals();
	VkCommandBuffer bufs[1];

	VK_FencedCheck();

	//a few cvars need some extra work if they're changed
	if (vk_submissionthread.modified || vid_vsync.modified || vid_triplebuffer.modified || vid_srgb.modified)
	{
		vid_vsync.modified = false;
		vid_triplebuffer.modified = false;
		vid_srgb.modified = false;
		vk_submissionthread.modified = false;

		vk.triplebuffer = vid_triplebuffer.ival;
		vk.vsync = vid_vsync.ival;
		vk.neednewswapchain = true;
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
		vkDeviceWaitIdle(vk.device);
		VK_CreateSwapChain();
		vk.neednewswapchain = false;

		if (vk_submissionthread.ival)
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

	vkCmdEndRenderPass(vk.frame->cbuf);

	{
		VkImageMemoryBarrier imgbarrier = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
		imgbarrier.pNext = NULL;
		imgbarrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		imgbarrier.dstAccessMask = 0;
		imgbarrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		imgbarrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
		imgbarrier.image = vk.frame->backbuf->colour.image;
		imgbarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		imgbarrier.subresourceRange.baseMipLevel = 0;
		imgbarrier.subresourceRange.levelCount = 1;
		imgbarrier.subresourceRange.baseArrayLayer = 0;
		imgbarrier.subresourceRange.layerCount = 1;
		imgbarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		imgbarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		vkCmdPipelineBarrier(vk.frame->cbuf, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 0, 0, NULL, 0, NULL, 1, &imgbarrier);
	}

//	vkCmdWriteTimestamp(vk.frame->cbuf, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, querypool, vk.bufferidx*2+1);
	vkEndCommandBuffer(vk.frame->cbuf);

	{
		RSpeedRemark();
		VKBE_FlushDynamicBuffers();
		RSpeedEnd(RSPEED_SUBMIT);
	}

	bufs[0] = vk.frame->cbuf;

#ifndef THREADACQUIRE
	{
		RSpeedRemark();
		//make sure we actually got a buffer. required for vsync
		VkAssert(vkWaitForFences(vk.device, 1, &vk.acquirefence, VK_FALSE, UINT64_MAX));
		VkAssert(vkResetFences(vk.device, 1, &vk.acquirefence));
		RSpeedEnd(RSPEED_SETUP);
	}
#endif

	{
		struct vk_presented *fw = Z_Malloc(sizeof(*fw));
		fw->fw.Passed = VK_Presented;
		fw->fw.fence = vk.frame->finishedfence;
		fw->frame = vk.frame;
		//hand over any post-frame jobs to the frame in question.
		vk.frame->frameendjobs = vk.frameendjobs;
		vk.frameendjobs = NULL;

		VK_Submit_Work(bufs[0],
#ifndef THREADACQUIRE
			vk.frame->vsyncsemaphore
#else
			VK_NULL_HANDLE
#endif
			, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, vk.frame->presentsemaphore, vk.frame->finishedfence, vk.frame, &fw->fw);
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
static	VkAttachmentReference color_reference;
static	VkAttachmentReference depth_reference;
static	VkAttachmentDescription attachments[2] = {{0}};
static	VkSubpassDescription subpass = {0};
static 	VkRenderPassCreateInfo rp_info = {VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};

	for (pass = 0; pass < 3; pass++)
	{
		if (vk.renderpass[pass] != VK_NULL_HANDLE)
			continue;
		color_reference.attachment = 0;
		color_reference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		
		depth_reference.attachment = 1;
		depth_reference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

		attachments[color_reference.attachment].format = vk.backbufformat;
		attachments[color_reference.attachment].samples = VK_SAMPLE_COUNT_1_BIT;
//		attachments[color_reference.attachment].loadOp = pass?VK_ATTACHMENT_LOAD_OP_LOAD:VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attachments[color_reference.attachment].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		attachments[color_reference.attachment].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attachments[color_reference.attachment].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachments[color_reference.attachment].initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		attachments[color_reference.attachment].finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		attachments[depth_reference.attachment].format = vk.depthformat;
		attachments[depth_reference.attachment].samples = VK_SAMPLE_COUNT_1_BIT;
//		attachments[depth_reference.attachment].loadOp = pass?VK_ATTACHMENT_LOAD_OP_LOAD:VK_ATTACHMENT_LOAD_OP_CLEAR;
		attachments[depth_reference.attachment].storeOp = VK_ATTACHMENT_STORE_OP_STORE;//VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachments[depth_reference.attachment].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attachments[depth_reference.attachment].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachments[depth_reference.attachment].initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
		attachments[depth_reference.attachment].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

		subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpass.flags = 0;
		subpass.inputAttachmentCount = 0;
		subpass.pInputAttachments = NULL;
		subpass.colorAttachmentCount = 1;
		subpass.pColorAttachments = &color_reference;
		subpass.pResolveAttachments = NULL;
		subpass.pDepthStencilAttachment = &depth_reference;
		subpass.preserveAttachmentCount = 0;
		subpass.pPreserveAttachments = NULL;

		rp_info.attachmentCount = countof(attachments);
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

static void VK_Submit_DoWork(void)
{
	VkCommandBuffer cbuf[64];
	VkSemaphore wsem[64];
	VkPipelineStageFlags wsemstageflags[64];
	VkSemaphore ssem[64];

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

		subcount++;

		present = work->present;

		vk.work = work->next;
		Z_Free(work);
	}

	Sys_UnlockConditional(vk.submitcondition);	//don't block people giving us work while we're occupied
	if (subcount || waitfence)
	{
		RSpeedMark();
		err = vkQueueSubmit(vk.queue_render, subcount, subinfo, waitfence);
		if (err)
		{
			Con_Printf("ERROR: vkQueueSubmit: %x\n", err);
			errored = vk.neednewswapchain = true;
		}
		RSpeedEnd(RSPEED_SUBMIT);
	}

	if (present)
	{
//		struct vkframe **link;
		uint32_t framenum = present->backbuf - vk.backbufs;
		VkPresentInfoKHR presinfo = {VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
		presinfo.waitSemaphoreCount = 1;
		presinfo.pWaitSemaphores = &present->presentsemaphore;
		presinfo.swapchainCount = 1;
		presinfo.pSwapchains = &vk.swapchain;
		presinfo.pImageIndices = &framenum;

		if (!errored)
		{
			RSpeedMark();
			Sys_LockMutex(vk.swapchain_mutex);
			err = vkQueuePresentKHR(vk.queue_present, &presinfo);
			Sys_UnlockMutex(vk.swapchain_mutex);
			RSpeedEnd(RSPEED_PRESENT);
			RSpeedRemark();
			if (err)
			{
				Con_Printf("ERROR: vkQueuePresentKHR: %x\n", err);
				errored = vk.neednewswapchain = true;
			}
#ifdef THREADACQUIRE
			else
			{
				err = vkAcquireNextImageKHR(vk.device, vk.swapchain, 0, VK_NULL_HANDLE, vk.acquirefences[vk.aquirelast%ACQUIRELIMIT], &vk.acquirebufferidx[vk.aquirelast%ACQUIRELIMIT]);
				if (err)
				{
					Con_Printf("ERROR: vkAcquireNextImageKHR: %x\n", err);
					errored = vk.neednewswapchain = true;
				}
				vk.aquirelast++;
			}
#endif
			RSpeedEnd(RSPEED_ACQUIRE);
		}
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
		
		//Sys_ConditionSignal(vk.acquirecondition);
	
	}
	Sys_UnlockConditional(vk.submitcondition);
	return true;
}

void VK_Submit_Work(VkCommandBuffer cmdbuf, VkSemaphore semwait, VkPipelineStageFlags semwaitstagemask, VkSemaphore semsignal, VkFence fencesignal, struct vkframe *presentframe, struct vk_fencework *fencedwork)
{
	struct vkwork_s *work = Z_Malloc(sizeof(*work));
	struct vkwork_s **link;

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

void VK_CheckTextureFormats(void)
{
	struct {
		unsigned int pti;
		VkFormat vulkan;
		unsigned int needextra;
	} texfmt[] =
	{
		{PTI_RGBA8,		VK_FORMAT_R8G8B8A8_UNORM},
		{PTI_RGBX8,		VK_FORMAT_R8G8B8A8_UNORM},
		{PTI_BGRA8,		VK_FORMAT_B8G8R8A8_UNORM},
		{PTI_BGRX8,		VK_FORMAT_B8G8R8A8_UNORM},
		{PTI_RGB565,	VK_FORMAT_R5G6B5_UNORM_PACK16},
		{PTI_RGBA4444,	VK_FORMAT_R4G4B4A4_UNORM_PACK16},
		{PTI_ARGB4444,	VK_FORMAT_B4G4R4A4_UNORM_PACK16},
		{PTI_RGBA5551,	VK_FORMAT_R5G5B5A1_UNORM_PACK16},
		{PTI_ARGB1555,	VK_FORMAT_A1R5G5B5_UNORM_PACK16},
		{PTI_RGBA16F,	VK_FORMAT_R16G16B16A16_SFLOAT, VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT|VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BLEND_BIT},
		{PTI_RGBA32F,	VK_FORMAT_R32G32B32A32_SFLOAT, VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT|VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BLEND_BIT},
		{PTI_R8,		VK_FORMAT_R8_UNORM},
		{PTI_RG8,		VK_FORMAT_R8G8_UNORM},

		{PTI_DEPTH16,	VK_FORMAT_D16_UNORM,			VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT},
		{PTI_DEPTH24,	VK_FORMAT_X8_D24_UNORM_PACK32,	VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT},
		{PTI_DEPTH32,	VK_FORMAT_D32_SFLOAT,			VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT},
		{PTI_DEPTH24_8,	VK_FORMAT_D24_UNORM_S8_UINT,	VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT},

		{PTI_S3RGB1,	VK_FORMAT_BC1_RGB_UNORM_BLOCK},
		{PTI_S3RGBA1,	VK_FORMAT_BC1_RGBA_UNORM_BLOCK},
		{PTI_S3RGBA3,	VK_FORMAT_BC2_UNORM_BLOCK},
		{PTI_S3RGBA5,	VK_FORMAT_BC3_UNORM_BLOCK},
	};
	unsigned int i;
	VkPhysicalDeviceProperties props;

	vkGetPhysicalDeviceProperties(vk.gpu, &props);

	sh_config.texture_maxsize = props.limits.maxImageDimension2D;

	for (i = 0; i < countof(texfmt); i++)
	{
		unsigned int need = VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT | texfmt[i].needextra;
		VkFormatProperties fmt;
		vkGetPhysicalDeviceFormatProperties(vk.gpu, texfmt[i].vulkan, &fmt);

		if ((fmt.optimalTilingFeatures & need) == need)
			sh_config.texfmt[texfmt[i].pti] = true;
	}
}

//initialise the vulkan instance, context, device, etc.
qboolean VK_Init(rendererstate_t *info, const char *sysextname, qboolean (*createSurface)(void))
{
	VkResult err;
	VkApplicationInfo app;
	VkInstanceCreateInfo inst_info;
	const char *extensions[] = {	sysextname,
									VK_KHR_SURFACE_EXTENSION_NAME,
									VK_EXT_DEBUG_REPORT_EXTENSION_NAME
		};
	uint32_t extensions_count;

	if (vk_debug.ival)
		extensions_count = 3;
	else
		extensions_count = 2;

	vk.neednewswapchain = true;
	vk.triplebuffer = info->triplebuffer;
	vk.vsync = info->wait;
	memset(&sh_config, 0, sizeof(sh_config));


	//get second set of pointers...
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
			vkGetPhysicalDeviceProperties(devs[i], &props);
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

	//create the platform-specific surface
	createSurface();

	//figure out which of the device's queue's we're going to use
	{
		uint32_t queue_count, i;
		VkQueueFamilyProperties *queueprops;
		vkGetPhysicalDeviceQueueFamilyProperties(vk.gpu, &queue_count, NULL);
		queueprops = malloc(sizeof(VkQueueFamilyProperties)*queue_count);	//Oh how I wish I was able to use C99.
		vkGetPhysicalDeviceQueueFamilyProperties(vk.gpu, &queue_count, queueprops);

		vk.queueidx[0] = ~0u;
		vk.queueidx[1] = ~0u;
		for (i = 0; i < queue_count; i++)
		{
			VkBool32 supportsPresent;
			VkAssert(vkGetPhysicalDeviceSurfaceSupportKHR(vk.gpu, i, vk.surface, &supportsPresent));

			if ((queueprops[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) && supportsPresent)
			{
				vk.queueidx[0] = i;
				vk.queueidx[1] = i;
				break;
			}
			else if (vk.queueidx[0] == ~0u && (queueprops[i].queueFlags & VK_QUEUE_GRAPHICS_BIT))
				vk.queueidx[0] = i;
			else if (vk.queueidx[1] == ~0u && supportsPresent)
				vk.queueidx[1] = i;
		}

		free(queueprops);

		if (vk.queueidx[0] == ~0u || vk.queueidx[1] == ~0u)
		{
			Con_Printf("unable to find suitable queues\n");
			return false;
		}
	}


	{
		const char *devextensions[] = {	VK_KHR_SWAPCHAIN_EXTENSION_NAME};
		float queue_priorities[1] = {1.0};
		VkDeviceQueueCreateInfo queueinf[2] = {{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO},{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO}};
		VkDeviceCreateInfo devinf = {VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
		queueinf[0].pNext = NULL;
		queueinf[0].queueFamilyIndex = vk.queueidx[0];
		queueinf[0].queueCount = countof(queue_priorities);
		queueinf[0].pQueuePriorities = queue_priorities;
		queueinf[1].pNext = NULL;
		queueinf[1].queueFamilyIndex = vk.queueidx[1];
		queueinf[1].queueCount = countof(queue_priorities);
		queueinf[1].pQueuePriorities = queue_priorities;

		devinf.queueCreateInfoCount = (vk.queueidx[0]==vk.queueidx[0])?1:2;
		devinf.pQueueCreateInfos = queueinf;
		devinf.enabledLayerCount = vklayercount;
		devinf.ppEnabledLayerNames = vklayerlist;
		devinf.enabledExtensionCount = countof(devextensions);
		devinf.ppEnabledExtensionNames = devextensions;
		devinf.pEnabledFeatures = NULL;

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

	vkGetDeviceQueue(vk.device, vk.queueidx[0], 0, &vk.queue_render);
	vkGetDeviceQueue(vk.device, vk.queueidx[1], 0, &vk.queue_present);


	vkGetPhysicalDeviceMemoryProperties(vk.gpu, &vk.memory_properties);

	{
		VkCommandPoolCreateInfo cpci = {VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
		cpci.queueFamilyIndex = vk.queueidx[0];
		cpci.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT|VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
		VkAssert(vkCreateCommandPool(vk.device, &cpci, vkallocationcb, &vk.cmdpool));
	}

	
	sh_config.progpath = NULL;//"vulkan";
	sh_config.blobpath = "spirv";
	sh_config.shadernamefmt = NULL;//".spv";

	sh_config.progs_supported = true;
	sh_config.progs_required = false;//true;
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

	VK_CheckTextureFormats();


	sh_config.pDeleteProg = NULL;
	sh_config.pLoadBlob = NULL;
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

#ifndef THREADACQUIRE
	{
		VkFenceCreateInfo fci = {VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
		VkAssert(vkCreateFence(vk.device,&fci,vkallocationcb,&vk.acquirefence));
	}
#endif

/*
	void	 (*pDeleteProg)		(program_t *prog, unsigned int permu);
	qboolean (*pLoadBlob)		(program_t *prog, const char *name, unsigned int permu, vfsfile_t *blobfile);
	qboolean (*pCreateProgram)	(program_t *prog, const char *name, unsigned int permu, int ver, const char **precompilerconstants, const char *vert, const char *tcs, const char *tes, const char *geom, const char *frag, qboolean noerrors, vfsfile_t *blobfile);
	qboolean (*pValidateProgram)(program_t *prog, const char *name, unsigned int permu, qboolean noerrors, vfsfile_t *blobfile);
	void	 (*pProgAutoFields)	(program_t *prog, const char *name, cvar_t **cvars, char **cvarnames, int *cvartypes);
*/


	vk.swapchain_mutex = Sys_CreateMutex();
	vk.submitcondition = Sys_CreateConditional();
	vk.acquirecondition = Sys_CreateConditional();

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

		if (vk_submissionthread.ival)
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
		VKBE_RT_Destroy(&postproc[i]);

	vkDestroyCommandPool(vk.device, vk.cmdpool, vkallocationcb);
	VK_DestroyRenderPass();
#ifndef THREADACQUIRE
	vkDestroyFence(vk.device, vk.acquirefence, vkallocationcb);
#endif

	{
		size_t size;
		if (VK_SUCCESS == vkGetPipelineCacheData(vk.device, vk.pipelinecache, &size, NULL))
		{
			void *ptr = BZ_Malloc(size);
			if (VK_SUCCESS == vkGetPipelineCacheData(vk.device, vk.pipelinecache, &size, ptr))
				FS_WriteFile("vulkan.pcache", ptr, size, FS_ROOT);
			BZ_Free(ptr);
		}
		vkDestroyPipelineCache(vk.device, vk.pipelinecache, vkallocationcb);
	}

	vkDestroyDevice(vk.device, vkallocationcb);
	if (vk_debugcallback)
	{
		vkDestroyDebugReportCallbackEXT(vk.instance, vk_debugcallback, vkallocationcb);
		vk_debugcallback = VK_NULL_HANDLE;
	}
	vkDestroySurfaceKHR(vk.instance, vk.surface, vkallocationcb);
	vkDestroyInstance(vk.instance, vkallocationcb);
	Sys_DestroyMutex(vk.swapchain_mutex);
	Sys_DestroyConditional(vk.submitcondition);
	Sys_DestroyConditional(vk.acquirecondition);
	memset(&vk, 0, sizeof(vk));
	qrenderer = QR_NONE;

#ifdef VK_NO_PROTOTYPES
	#define VKFunc(n) vk##n = NULL;
	VKFuncs
	#undef VKFunc
#endif

}
#endif
