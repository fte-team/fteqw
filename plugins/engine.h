typedef enum uploadfmt_e
{
	TF_INVALID,
	TF_RGBA32,
	TF_BGRA32,
	TF_RGBX32,
	TF_BGRX32
} uploadfmt_t;

typedef struct
{
	void *(QDECL *createdecoder)(char *name);	//needed
	void *(QDECL *decodeframe)(void *ctx, qboolean nosound, enum uploadfmt_e *fmt, int *width, int *height);	//needed
	void (QDECL *doneframe)(void *ctx, void *img);	//basically a free()
	void (QDECL *shutdown)(void *ctx);	//probably needed...
	void (QDECL *rewind)(void *ctx);

	//these are any interactivity functions you might want...
	void (QDECL *cursormove) (void *ctx, float posx, float posy);	//pos is 0-1
	void (QDECL *key) (void *ctx, int code, int unicode, int event);	//key event! event=1=down
	qboolean (QDECL *setsize) (void *ctx, int width, int height);	//updates the desired screen-space size
	void (QDECL *getsize) (void *ctx, int *width, int *height);	//retrieves the screen-space size
	void (QDECL *changestream) (void *ctx, char *streamname);	//can be used to accept commands from qc
} media_decoder_funcs_t;

typedef struct
{
	void *(QDECL *capture_begin) (char *streamname, int videorate, int width, int height, int *sndkhz, int *sndchannels, int *sndbits);
	void (QDECL *capture_video) (void *ctx, void *data, int frame, int width, int height);
	void (QDECL *capture_audio) (void *ctx, void *data, int bytes);
	void (QDECL *capture_end) (void *ctx);
} media_encoder_funcs_t;
