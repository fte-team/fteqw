//These are defined later in the source tree. This file should probably be moved to a later spot.
struct progfuncs_s;
struct globalvars_s;
struct texture_s;

//function prototypes

#if defined(SERVERONLY)
#define qrenderer QR_NONE
#define FNC(n) (n)
#else
#define FNC(n) (*n)
extern r_qrenderer_t qrenderer;
extern char *q_renderername;


extern mpic_t	*(*Draw_PicFromWad)					(char *name);
extern mpic_t	*(*Draw_SafePicFromWad)				(char *name);
extern mpic_t	*(*Draw_CachePic)					(char *path);
extern mpic_t	*(*Draw_SafeCachePic)				(char *path);
extern void	(*Draw_Init)							(void);
extern void	(*Draw_ReInit)							(void);
extern void	(*Draw_Character)						(int x, int y, unsigned int num);
extern void	(*Draw_ColouredCharacter)				(int x, int y, unsigned int num);
extern void	(*Draw_String)							(int x, int y, const qbyte *str);
extern void	(*Draw_Alt_String)						(int x, int y, const qbyte *str);
extern void	(*Draw_Crosshair)						(void);
extern void	(*Draw_DebugChar)						(qbyte num);
extern void	(*Draw_Pic)								(int x, int y, mpic_t *pic);
extern void	(*Draw_ScalePic)						(int x, int y, int width, int height, mpic_t *pic);
extern void	(*Draw_SubPic)							(int x, int y, mpic_t *pic, int srcx, int srcy, int width, int height);
extern void	(*Draw_TransPic)						(int x, int y, mpic_t *pic);
extern void	(*Draw_TransPicTranslate)				(int x, int y, int width, int height, qbyte *image, qbyte *translation);
extern void	(*Draw_ConsoleBackground)				(int lines);
extern void	(*Draw_EditorBackground)				(int lines);
extern void	(*Draw_TileClear)						(int x, int y, int w, int h);
extern void	(*Draw_Fill)							(int x, int y, int w, int h, int c);
extern void	(*Draw_FadeScreen)						(void);
extern void	(*Draw_BeginDisc)						(void);
extern void	(*Draw_EndDisc)							(void);
extern qboolean (*Draw_IsCached)					(char *picname);	//can be null

extern void	(*Draw_Image)							(float x, float y, float w, float h, float s1, float t1, float s2, float t2, mpic_t *pic);	//gl-style scaled/coloured/subpic 
extern void	(*Draw_ImageColours)					(float r, float g, float b, float a);

extern void	(*R_Init)								(void);
extern void	(*R_DeInit)								(void);
extern void	(*R_ReInit)								(void);
extern void	(*R_RenderView)							(void);		// must set r_refdef first

extern void	(*R_InitSky)							(struct texture_s *mt);	// called at level load
extern qboolean	(*R_CheckSky)						(void);
extern void (*R_SetSky)								(char *name, float rotate, vec3_t axis);

extern void	(*R_NewMap)								(void);
extern void	(*R_PreNewMap)							(void);
extern int	(*R_LightPoint)							(vec3_t point);

extern void	(*R_PushDlights)						(void);
extern void	(*R_AddStain)							(vec3_t org, float red, float green, float blue, float radius);
extern void	(*R_LessenStains)						(void);
extern void	(*R_DrawWaterSurfaces)					(void);

extern void (*Media_ShowFrameBGR_24_Flip)			(qbyte *framedata, int inwidth, int inheight);	//input is bottom up...
extern void (*Media_ShowFrameRGBA_32)				(qbyte *framedata, int inwidth, int inheight);	//top down
extern void (*Media_ShowFrame8bit)					(qbyte *framedata, int inwidth, int inheight, qbyte *palette);	//paletted topdown (framedata is 8bit indexes into palette)

extern qboolean	(*VID_Init)							(rendererstate_t *info, unsigned char *palette);
extern void	(*VID_DeInit)							(void);
extern void	(*VID_HandlePause)						(qboolean pause);
extern void	(*VID_LockBuffer)						(void);
extern void	(*VID_UnlockBuffer)						(void);
extern void	(*D_BeginDirectRect)					(int x, int y, qbyte *pbitmap, int width, int height);
extern void	(*D_EndDirectRect)						(int x, int y, int width, int height);
extern void	(*VID_ForceLockState)					(int lk);
extern int		(*VID_ForceUnlockedAndReturnState)	(void);
extern void	(*VID_SetPalette)						(unsigned char *palette);
extern void	(*VID_ShiftPalette)						(unsigned char *palette);
extern char *(*VID_GetRGBInfo)						(int prepad, int *truevidwidth, int *truevidheight);
extern void	(*VID_SetWindowCaption)					(char *msg);

extern void SCR_Init								(void);
extern void SCR_DeInit								(void);
extern void (*SCR_UpdateScreen)						(void);
extern void SCR_BeginLoadingPlaque					(void);
extern void SCR_EndLoadingPlaque					(void);
extern void SCR_DrawConsole							(qboolean noback);
extern void SCR_SetUpToDrawConsole					(void);
extern void SCR_EraseCenterString					(void);
extern void SCR_CenterPrint							(int pnum, char *str);

#endif

extern void	FNC(Mod_Init)							(void);
extern void	FNC(Mod_ClearAll)						(void);
extern struct model_s *FNC(Mod_ForName)				(char *name, qboolean crash);
extern struct model_s *FNC(Mod_FindName)			(char *name);
extern void	*FNC(Mod_Extradata)						(struct model_s *mod);	// handles caching
extern void	FNC(Mod_TouchModel)						(char *name);

extern struct mleaf_s *FNC(Mod_PointInLeaf)			(float *p, struct model_s *model);
extern qbyte	*FNC(Mod_Q1LeafPVS)					(struct mleaf_s *leaf, struct model_s *model, qbyte *buffer);	//purly for q1
extern void	FNC(Mod_NowLoadExternal)				(void);

extern void	FNC(Mod_Think)							(void);
extern void	(*Mod_GetTag)							(struct model_s *model, int tagnum, int frame, float **org, float **axis);
extern int (*Mod_TagNumForName)						(struct model_s *model, char *name);

#undef FNC

void Draw_FunString(int x, int y, char *str);


#ifdef SERVERONLY
#define Mod_Q1LeafPVS Mod_LeafPVS
qbyte *Mod_LeafPVS (struct mleaf_s *leaf, struct model_s *model, qbyte *buffer);
#endif
