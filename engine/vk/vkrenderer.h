//#include "glquake.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define VK_USE_PLATFORM_WIN32_KHR
#define VKInstWin32Funcs VKFunc(CreateWin32SurfaceKHR)
#endif

#ifdef __linux__
#define VK_USE_PLATFORM_XLIB_KHR
#define VKInstXLibFuncs VKFunc(CreateXlibSurfaceKHR)
#endif

#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>

#if defined(_MSC_VER) && !defined(UINT64_MAX)
#define UINT64_MAX _UI64_MAX
#endif

#define THREADACQUIRE			//should be better behaved, with no extra locks needed.



#ifndef VKInstWin32Funcs
#define VKInstWin32Funcs
#endif
#ifndef VKInstXLibFuncs
#define VKInstXLibFuncs
#endif

//funcs needed for creating an instance
#define VKInstFuncs \
	VKFunc(EnumerateInstanceLayerProperties)		\
	VKFunc(EnumerateInstanceExtensionProperties)	\
	VKFunc(CreateInstance)

#define VKInstArchFuncs VKInstWin32Funcs VKInstXLibFuncs

//funcs specific to an instance
#define VKInst2Funcs \
	VKFunc(EnumeratePhysicalDevices)				\
	VKFunc(EnumerateDeviceExtensionProperties)		\
	VKFunc(GetPhysicalDeviceProperties)				\
	VKFunc(GetPhysicalDeviceQueueFamilyProperties)	\
	VKFunc(GetPhysicalDeviceSurfaceSupportKHR)		\
	VKFunc(GetPhysicalDeviceSurfaceFormatsKHR)		\
	VKFunc(GetPhysicalDeviceSurfacePresentModesKHR)	\
	VKFunc(GetPhysicalDeviceSurfaceCapabilitiesKHR)	\
	VKFunc(GetPhysicalDeviceMemoryProperties)		\
	VKFunc(GetPhysicalDeviceFormatProperties)		\
	VKFunc(DestroySurfaceKHR)						\
	VKFunc(CreateDevice)							\
	VKFunc(DestroyInstance)							\
	VKInstArchFuncs

//funcs specific to a device
#define VKDevFuncs \
	VKFunc(AcquireNextImageKHR)			\
	VKFunc(QueuePresentKHR)				\
	VKFunc(CreateSwapchainKHR)			\
	VKFunc(GetSwapchainImagesKHR)		\
	VKFunc(DestroySwapchainKHR)			\
	VKFunc(CmdBeginRenderPass)			\
	VKFunc(CmdEndRenderPass)			\
	VKFunc(CmdBindPipeline)				\
	VKFunc(CmdDrawIndexedIndirect)		\
	VKFunc(CmdDraw)						\
	VKFunc(CmdDrawIndexed)				\
	VKFunc(CmdSetViewport)				\
	VKFunc(CmdSetScissor)				\
	VKFunc(CmdBindDescriptorSets)		\
	VKFunc(CmdBindIndexBuffer)			\
	VKFunc(CmdBindVertexBuffers)		\
	VKFunc(CmdPushConstants)			\
	VKFunc(CmdClearAttachments)			\
	VKFunc(CmdClearColorImage)			\
	VKFunc(CmdClearDepthStencilImage)	\
	VKFunc(CmdCopyImage)				\
	VKFunc(CmdCopyBuffer)				\
	VKFunc(CmdBlitImage)				\
	VKFunc(CmdPipelineBarrier)			\
	VKFunc(CreateDescriptorSetLayout)	\
	VKFunc(DestroyDescriptorSetLayout)	\
	VKFunc(CreatePipelineLayout)		\
	VKFunc(DestroyPipelineLayout)		\
	VKFunc(CreateShaderModule)			\
	VKFunc(DestroyShaderModule)			\
	VKFunc(CreateGraphicsPipelines)		\
	VKFunc(DestroyPipeline)				\
	VKFunc(CreatePipelineCache)			\
	VKFunc(GetPipelineCacheData)		\
	VKFunc(DestroyPipelineCache)		\
	VKFunc(QueueSubmit)					\
	VKFunc(QueueWaitIdle)				\
	VKFunc(DeviceWaitIdle)				\
	VKFunc(BeginCommandBuffer)			\
	VKFunc(ResetCommandBuffer)			\
	VKFunc(EndCommandBuffer)			\
	VKFunc(DestroyDevice)				\
	VKFunc(GetDeviceQueue)				\
	VKFunc(GetBufferMemoryRequirements)	\
	VKFunc(GetImageMemoryRequirements)	\
	VKFunc(GetImageSubresourceLayout)	\
	VKFunc(CreateFramebuffer)			\
	VKFunc(DestroyFramebuffer)			\
	VKFunc(CreateCommandPool)			\
	VKFunc(ResetCommandPool)			\
	VKFunc(DestroyCommandPool)			\
	VKFunc(CreateDescriptorPool)		\
	VKFunc(ResetDescriptorPool)			\
	VKFunc(DestroyDescriptorPool)		\
	VKFunc(AllocateDescriptorSets)		\
	VKFunc(CreateSampler)				\
	VKFunc(DestroySampler)				\
	VKFunc(CreateImage)					\
	VKFunc(DestroyImage)				\
	VKFunc(CreateBuffer)				\
	VKFunc(DestroyBuffer)				\
	VKFunc(AllocateMemory)				\
	VKFunc(FreeMemory)					\
	VKFunc(BindBufferMemory)			\
	VKFunc(BindImageMemory)				\
	VKFunc(MapMemory)					\
	VKFunc(FlushMappedMemoryRanges)		\
	VKFunc(UnmapMemory)					\
	VKFunc(UpdateDescriptorSets)		\
	VKFunc(AllocateCommandBuffers)		\
	VKFunc(FreeCommandBuffers)			\
	VKFunc(CreateRenderPass)			\
	VKFunc(DestroyRenderPass)			\
	VKFunc(CreateSemaphore)				\
	VKFunc(DestroySemaphore)			\
	VKFunc(CreateFence)					\
	VKFunc(GetFenceStatus)				\
	VKFunc(WaitForFences)				\
	VKFunc(ResetFences)					\
	VKFunc(DestroyFence)				\
	VKFunc(CreateImageView)				\
	VKFunc(DestroyImageView)


//all vulkan funcs
#define VKFuncs \
	VKInstFuncs		\
	VKInst2Funcs	\
	VKDevFuncs		\
	VKFunc(GetInstanceProcAddr)\
	VKFunc(GetDeviceProcAddr)


#ifdef VK_NO_PROTOTYPES
	#define VKFunc(n) extern PFN_vk##n vk##n;
	VKFuncs
	#undef VKFunc
#else
//	#define VKFunc(n) static const PFN_vk##n vk##n = vk##n;
//	VKFuncs
//	#undef VKFunc
#endif

#define vkallocationcb NULL
#ifdef _DEBUG
#define VkAssert(f) do {VkResult err = f; if (err) Sys_Error("%s == %i", #f, err); } while(0)
#else
#define VkAssert(f) f
#endif

typedef struct vk_image_s
{
	VkImage image;
	VkDeviceMemory memory;
	VkImageView view;
	VkSampler sampler;
	VkImageLayout layout;

	uint32_t width;
	uint32_t height;
	uint32_t layers;
	uint32_t mipcount;
	uint32_t encoding;
	uint32_t type;
} vk_image_t;
enum dynbuf_e
{
	DB_VBO,
	DB_EBO,
	DB_UBO,
	DB_MAX
};
struct vk_rendertarg
{
	VkFramebuffer framebuffer;
	vk_image_t colour, depth;

	image_t q_colour, q_depth;	//extra sillyness...

	uint32_t width;
	uint32_t height;

	qboolean depthcleared;	//starting a new gameview needs cleared depth relative to other views, but the first probably won't.

	VkRenderPassBeginInfo restartinfo;
};
struct vk_rendertarg_cube
{
	uint32_t size;
	image_t q_colour, q_depth;	//extra sillyness...
	vk_image_t colour, depth;
	struct vk_rendertarg face[6];
};

extern struct vulkaninfo_s
{
	unsigned short	triplebuffer;
	qboolean		vsync;

	VkInstance instance;
	VkDevice device;
	VkPhysicalDevice gpu;
	VkSurfaceKHR surface;
	uint32_t queueidx[2];	//queue families, render+present
	VkQueue queue_render;
	VkQueue queue_present;
	VkPhysicalDeviceMemoryProperties memory_properties;
	VkCommandPool cmdpool;

#ifdef THREADACQUIRE
#define ACQUIRELIMIT 8
	VkFence acquirefences[ACQUIRELIMIT];
	uint32_t acquirebufferidx[ACQUIRELIMIT];
	unsigned int aquirenext;
	volatile unsigned int aquirelast;	//set inside the submission thread
#else
	VkFence acquirefence;
#endif

	VkPipelineCache pipelinecache;

	struct vk_fencework 
	{
		VkFence fence;
		struct vk_fencework *next;
		void (*Passed) (void*);
		VkCommandBuffer cbuf;
	} *fencework, *fencework_last;	//callback for each fence as its passed. mostly for loading code or freeing memory.

	int filtermip[3];
	int filterpic[3];
	int mipcap[2];
	float max_anistophy;

	struct descpool
	{
		VkDescriptorPool pool;
		int availsets;
		int totalsets;
		struct descpool *next;
	} *descpool;
	struct dynbuffer
	{
		size_t offset;
		size_t size;
		size_t align;
		VkBuffer stagingbuf;
		VkDeviceMemory stagingmemory;
		VkBuffer devicebuf;
		VkDeviceMemory devicememory;
		VkBuffer renderbuf;	//either staging or device.
		void *ptr;

		struct dynbuffer *next;
	} *dynbuf[DB_MAX];
	struct vk_rendertarg *backbufs;
	struct vk_rendertarg *rendertarg;
	struct vkframe {
		struct vkframe *next;
#ifndef THREADACQUIRE
		VkSemaphore vsyncsemaphore;
#endif
		VkSemaphore presentsemaphore;
		VkCommandBuffer cbuf;
		struct dynbuffer *dynbufs[DB_MAX];
		struct descpool *descpools;
		VkFence finishedfence;
		struct vk_fencework *frameendjobs;

		struct vk_rendertarg *backbuf;
	} *frame, *unusedframes;
	struct vk_fencework *frameendjobs;
	uint32_t backbuf_count;

	VkRenderPass shadow_renderpass;	//clears depth etc.
	VkRenderPass renderpass[3];	//initial, resume
	VkSwapchainKHR swapchain;
	void *swapchain_mutex;	//acquire+present need some syncronisation.
	uint32_t bufferidx;

	VkFormat depthformat;
	VkFormat backbufformat;

	qboolean neednewswapchain;

	struct vkwork_s
	{
		struct vkwork_s *next;
		VkCommandBuffer cmdbuf;
		VkSemaphore semwait;
		VkPipelineStageFlags semwaitstagemask;
		VkSemaphore semsignal;
		VkFence fencesignal;

		struct vk_fencework *fencedwork;
		struct vkframe *present;
	} *work;
	void *submitthread;
	void *submitcondition;
	void *acquirecondition;

	texid_t sourcecolour;
	texid_t sourcedepth;

	shader_t *scenepp_waterwarp;
	shader_t *scenepp_antialias;
	shader_t *scenepp_rescale;
} vk;

struct pipeline_s
{
	struct pipeline_s *next;
	unsigned int permu:16;	//matches the permutation (masked by permutations that are supposed to be supported)
	unsigned int flags:16;	//matches the shader flags (cull etc)
	unsigned int blendbits; //matches blend state.
	VkPipeline pipeline;
};

uint32_t vk_find_memory_try(uint32_t typeBits, VkFlags requirements_mask);
uint32_t vk_find_memory_require(uint32_t typeBits, VkFlags requirements_mask);

qboolean VK_LoadTextureMips (texid_t tex, struct pendingtextureinfo *mips);

qboolean VK_Init(rendererstate_t *info, const char *sysextname, qboolean (*createSurface)(void));
void VK_Shutdown(void);

void VK_R_BloomBlend (texid_t source, int x, int y, int w, int h);
void VK_R_BloomShutdown(void);
qboolean R_CanBloom(void);

struct programshared_s;
qboolean VK_LoadGLSL(struct programshared_s *prog, const char *name, unsigned int permu, int ver, const char **precompilerconstants, const char *vert, const char *tcs, const char *tes, const char *geom, const char *frag, qboolean noerrors, vfsfile_t *blobfile);

void VKBE_Init(void);
void VKBE_InitFramePools(struct vkframe *frame);
void VKBE_RestartFrame(void);
void VKBE_FlushDynamicBuffers(void);
void VKBE_Set2D(qboolean twodee);
void VKBE_ShutdownFramePools(struct vkframe *frame);
void VKBE_Shutdown(void);
void VKBE_SelectMode(backendmode_t mode);
void VKBE_DrawMesh_List(shader_t *shader, int nummeshes, mesh_t **mesh, vbo_t *vbo, texnums_t *texnums, unsigned int beflags);
void VKBE_DrawMesh_Single(shader_t *shader, mesh_t *meshchain, vbo_t *vbo, unsigned int beflags);
void VKBE_SubmitBatch(batch_t *batch);
batch_t *VKBE_GetTempBatch(void);
void VKBE_GenBrushModelVBO(model_t *mod);
void VKBE_ClearVBO(vbo_t *vbo);
void VKBE_UploadAllLightmaps(void);
void VKBE_DrawWorld (batch_t **worldbatches, qbyte *vis);
qboolean VKBE_LightCullModel(vec3_t org, model_t *model);
void VKBE_SelectEntity(entity_t *ent);
qboolean VKBE_SelectDLight(dlight_t *dl, vec3_t colour, vec3_t axis[3], unsigned int lmode);
void VKBE_VBO_Begin(vbobctx_t *ctx, size_t maxsize);
void VKBE_VBO_Data(vbobctx_t *ctx, void *data, size_t size, vboarray_t *varray);
void VKBE_VBO_Finish(vbobctx_t *ctx, void *edata, size_t esize, vboarray_t *earray, void **vbomem, void **ebomem);
void VKBE_VBO_Destroy(vboarray_t *vearray, void *mem);
void VKBE_Scissor(srect_t *rect);
void VKBE_BaseEntTextures(void);

struct vk_shadowbuffer;
struct vk_shadowbuffer *VKBE_GenerateShadowBuffer(vecV_t *verts, int numverts, index_t *indicies, int numindicies, qboolean istemp);
void VKBE_DestroyShadowBuffer(struct vk_shadowbuffer *buf);
void VKBE_RenderShadowBuffer(struct vk_shadowbuffer *buf);
void VKBE_SetupForShadowMap(dlight_t *dl, qboolean isspot, int texwidth, int texheight, float shadowscale);
qboolean VKBE_BeginShadowmap(qboolean isspot, uint32_t width, uint32_t height);
void VKBE_BeginShadowmapFace(void);
void VKBE_DoneShadows(void);

void VKBE_RT_Gen_Cube(struct vk_rendertarg_cube *targ, uint32_t size, qboolean clear);
void VKBE_RT_Gen(struct vk_rendertarg *targ, uint32_t width, uint32_t height, qboolean clear);
void VKBE_RT_Begin(struct vk_rendertarg *targ);
void VKBE_RT_Destroy(struct vk_rendertarg *targ);


struct stagingbuf
{
	VkBuffer buf;
	VkBuffer retbuf;
	VkDeviceMemory memory;
	size_t size;
	VkBufferUsageFlags usage;
};
vk_image_t VK_CreateTexture2DArray(uint32_t width, uint32_t height, uint32_t layers, uint32_t mips, unsigned int encoding, unsigned int type);
void set_image_layout(VkCommandBuffer cmd, VkImage image, VkImageAspectFlags aspectMask, VkImageLayout old_image_layout, VkAccessFlags srcaccess, VkImageLayout new_image_layout, VkAccessFlags dstaccess);
void VK_CreateSampler(unsigned int flags, vk_image_t *img);
void *VKBE_CreateStagingBuffer(struct stagingbuf *n, size_t size, VkBufferUsageFlags usage);
VkBuffer VKBE_FinishStaging(struct stagingbuf *n, VkDeviceMemory *memptr);
void *VK_FencedBegin(void (*passed)(void *work), size_t worksize);
void VK_FencedSubmit(void *work);
void VK_FencedCheck(void);
void *VK_AtFrameEnd(void (*passed)(void *work), size_t worksize);



void	VK_Draw_Init(void);
void	VK_Draw_Shutdown(void);

void	VK_UpdateFiltering			(image_t *imagelist, int filtermip[3], int filterpic[3], int mipcap[2], float anis);
qboolean VK_LoadTextureMips			(texid_t tex, struct pendingtextureinfo *mips);
void    VK_DestroyTexture			(texid_t tex);
void	VK_DestroyVkTexture			(vk_image_t *img);

void	VK_R_Init					(void);
void	VK_R_DeInit					(void);
void	VK_R_RenderView				(void);

char	*VKVID_GetRGBInfo			(int *truevidwidth, int *truevidheight, enum uploadfmt *fmt);

qboolean	VK_SCR_UpdateScreen			(void);

void	VKBE_RenderToTextureUpdate2d(qboolean destchanged);
