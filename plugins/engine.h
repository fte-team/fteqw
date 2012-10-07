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
	void *(*createdecoder)(char *name);	//needed
	void *(*decodeframe)(void *ctx, qboolean nosound, enum uploadfmt_e *fmt, int *width, int *height);	//needed
	void (*doneframe)(void *ctx, void *img);	//basically a free()
	void (*shutdown)(void *ctx);	//probably needed...
	void (*rewind)(void *ctx);

	//these are any interactivity functions you might want...
	void (*cursormove) (void *ctx, float posx, float posy);	//pos is 0-1
	void (*key) (void *ctx, int code, int unicode, int event);	//key event! event=1=down
	qboolean (*setsize) (void *ctx, int width, int height);	//updates the desired screen-space size
	void (*getsize) (void *ctx, int *width, int *height);	//retrieves the screen-space size
	void (*changestream) (void *ctx, char *streamname);	//can be used to accept commands from qc
} media_decoder_funcs_t;
