void emscriptenfte_async_wget_data2(const char *url, void *ctx, void (*onload)(void*ctx,void*buf,int sz), void (*onerror)(void*ctx,int code), void (*onprogress)(void*ctx,int prog,int total));

//filesystem buffers are implemented in javascript so that we are not bound by power-of-two heap limitations quite so much.
//also, we can't use emscripten because it reserves 16m file handles or something.
int emscriptenfte_buf_create(void);
int emscriptenfte_buf_open(const char *name, int createifneeded);
int emscriptenfte_buf_rename(const char *oldname, const char *newname);
int emscriptenfte_buf_delete(const char *fname);
void emscriptenfte_buf_release(int handle);
unsigned int emscriptenfte_buf_getsize(int handle);
int emscriptenfte_buf_read(int handle, int offset, void *data, int len);
int emscriptenfte_buf_write(int handle, int offset, const void *data, int len);

//websocket is implemented in javascript because there is no usable C api (emscripten's javascript implementation is shite).
int emscriptenfte_ws_connect(const char *url);
void emscriptenfte_ws_close(int sockid);
int emscriptenfte_ws_cansend(int sockid, int extra, int maxpending);
int emscriptenfte_ws_send(int sockid, const void *data, int len);
int emscriptenfte_ws_recv(int sockid, void *data, int len);

void Sys_Print(const char *msg);
unsigned long emscriptenfte_ticks_ms(void);

int emscriptenfte_setupcanvas(
	int width,
	int height,
	void(*Resized)(int newwidth, int newheight),
	void(*Mouse)(int devid,int abs,float x,float y,float z,float size),
	void(*Button)(int devid, int down, int mbutton),
	void(*Keyboard)(int devid, int down, int keycode, int unicode)
	);

