
typedef struct
{
	texcom_t com;

	char name[64];
	int width;
	int height;
	int pitch;
	unsigned int data[1];
} swimage_t;

typedef struct
{
	volatile unsigned int readpoint;	//the command queue point its reading from
	void *thread;

#ifdef _DEBUG
	float idletime;
	float activetime;
#endif

	unsigned int interlaceline;
	unsigned int interlacemod;
	unsigned int threadnum;	//for relocating viewport info
	unsigned int *vpdbuf;
	unsigned int *vpcbuf;
	unsigned int vpwidth;
	unsigned int vpheight;
	qintptr_t vpcstride;
	struct workqueue_s *wq;
} swthread_t;

typedef struct
{
	vec4_t vcoord;
	vec2_t tccoord;
	vec2_t lmcoord;
	byte_vec4_t colour;
	unsigned int clipflags;	/*1=left,2=right,4=top,8=bottom,16=near*/
} swvert_t;

#define WQ_SIZE 1024*1024*8
#define WQ_MASK (WQ_SIZE-1)
#define WQ_MAXTHREADS 64
struct workqueue_s
{
	unsigned int numthreads;
	qbyte queue[WQ_SIZE];
	volatile unsigned int pos;

	swthread_t swthreads[WQ_MAXTHREADS];
};
extern struct workqueue_s commandqueue;



enum wqcmd_e
{
	WTC_DIE,
	WTC_SYNC,
	WTC_NEWFRAME,
	WTC_NOOP,
	WTC_VIEWPORT,
	WTC_TRIFAN,
	WTC_TRISOUP,
	WTC_SPANS
};

enum
{
	CLIP_LEFT_FLAG		= 1,
	CLIP_RIGHT_FLAG		= 2,
	CLIP_TOP_FLAG		= 4,
	CLIP_BOTTOM_FLAG	= 8,
	CLIP_NEAR_FLAG		= 16
};

typedef union
{
	unsigned char align[16];

	struct wqcom_s
	{
		enum wqcmd_e command;
		unsigned int cmdsize;
	} com;
	struct
	{
		struct wqcom_s com;

		swimage_t *texture;
		int numverts;
		swvert_t verts[1];
	} trifan;
	struct
	{
		struct wqcom_s com;

		swimage_t *texture;
		int numverts;
		int numidx;
		swvert_t verts[1];
	} trisoup;
	struct
	{
		struct wqcom_s com;
		unsigned int *cbuf;
		unsigned int *dbuf;
		unsigned int width;
		unsigned int height;
		qintptr_t stride;
		unsigned int interlace;
		unsigned int framenum;

		qboolean cleardepth;
		qboolean clearcolour;
	} viewport;
	struct
	{
		int foo;
	} spans;
} wqcom_t;



void SWRast_EndCommand(struct workqueue_s *wq, wqcom_t *com);
wqcom_t *SWRast_BeginCommand(struct workqueue_s *wq, int cmdtype, unsigned int size);
void SWRast_Sync(struct workqueue_s *wq);



qboolean SW_VID_Init(rendererstate_t *info, unsigned char *palette);
void SW_VID_DeInit(void);
qboolean SW_VID_ApplyGammaRamps		(unsigned short *ramps);
char *SW_VID_GetRGBInfo(int prepad, int *truevidwidth, int *truevidheight);
void SW_VID_SetWindowCaption(char *msg);
void SW_VID_SwapBuffers(void);
void SW_VID_UpdateViewport(wqcom_t *com);




texid_tf SW_LoadTexture(char *identifier, int width, int height, uploadfmt_t fmt, void *data, unsigned int flags);
texid_tf SW_LoadTexture8Pal24(char *identifier, int width, int height, qbyte *data, qbyte *palette24, unsigned int flags);
texid_tf SW_LoadTexture8Pal32(char *identifier, int width, int height, qbyte *data, qbyte *palette32, unsigned int flags);
texid_tf SW_LoadCompressed(char *name);
texid_tf SW_FindTexture(char *identifier, unsigned int flags);
texid_tf SW_AllocNewTexture(char *identifier, int w, int h, unsigned int flags);
void SW_Upload(texid_t tex, char *name, uploadfmt_t fmt, void *data, void *palette, int width, int height, unsigned int flags);
void SW_DestroyTexture(texid_t tex);


void SWBE_SelectMode(backendmode_t mode);
void SWBE_DrawMesh_List(shader_t *shader, int nummeshes, struct mesh_s **mesh, struct vbo_s *vbo, struct texnums_s *texnums, unsigned int be_flags);
void SWBE_DrawMesh_Single(shader_t *shader, struct mesh_s *meshchain, struct vbo_s *vbo, struct texnums_s *texnums, unsigned int be_flags);
void SWBE_SubmitBatch(struct batch_s *batch);
struct batch_s *SWBE_GetTempBatch(void);
void SWBE_DrawWorld(qboolean drawworld, qbyte *vis);
void SWBE_Init(void);
void SWBE_GenBrushModelVBO(struct model_s *mod);
void SWBE_ClearVBO(struct vbo_s *vbo);
void SWBE_UploadAllLightmaps(void);
void SWBE_SelectEntity(struct entity_s *ent);
void SWBE_SelectDLight(struct dlight_s *dl, vec3_t colour);
qboolean SWBE_LightCullModel(vec3_t org, struct model_s *model);
void SWBE_Set2D(void);
