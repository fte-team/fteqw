#include "../plugin.h"
#include "../engine.h"

#include "berkelium/Berkelium.hpp"
#include "berkelium/Window.hpp"
#include "berkelium/WindowDelegate.hpp"
#include "berkelium/Context.hpp"

#include <string>

class decctx
{
public:
	Berkelium::Window *wnd;
	int width;
	int height;
	unsigned int *buffer;
	bool repainted;
};

class MyDelegate : public Berkelium::WindowDelegate
{
private:
	decctx *ctx;

	virtual void onPaint(Berkelium::Window *wini, const unsigned char *bitmap_in, const Berkelium::Rect &bitmap_rect, size_t num_copy_rects, const Berkelium::Rect *copy_rects, int dx, int dy, const Berkelium::Rect& scroll_rect)
	{
		int i;
		// handle paint events...
		if (dx || dy)
		{
			int y;
			int t = scroll_rect.top();
			int l = scroll_rect.left();
			int w = scroll_rect.width();
			int h = scroll_rect.height();
			if (dy > 0)
			{
				//if we're moving downwards, we need to write the bottom before the top (so we don't overwrite the data before its copied)
				for (y = t+h-1; y >= t; y--)
				{
					if (y < 0 || y >= ctx->height)
						continue;
					if (y+dy < 0 || y+dy >= ctx->height)
						continue;
					memmove(ctx->buffer + ((l+dx) + (y+dy)*ctx->width), ctx->buffer + (l + y*ctx->width), w*4);
				}
			}
			else
			{
				//moving upwards requires we write the top row first
				for (y = t; y < t+h; y++)
				{
					if (y < 0 || y >= ctx->height)
						continue;
					if (y+dy < 0 || y+dy >= ctx->height)
						continue;

					memmove(ctx->buffer + ((l+dx) + (y+dy)*ctx->width), ctx->buffer + (l + y*ctx->width), w*4);
				}
			}
		}
		for (i = 0; i < num_copy_rects; i++)
		{
			unsigned int *out = ctx->buffer;
			const unsigned int *in = (const unsigned int*)bitmap_in;
			int x, y;
			int t = copy_rects[i].top() - bitmap_rect.top();
			int l = copy_rects[i].left() - bitmap_rect.left();
			int r = copy_rects[i].width() + l;
			int b = copy_rects[i].height() + t;
			unsigned int instride = bitmap_rect.width() - (r - l);
			unsigned int outstride = ctx->width - (r - l);

			out += copy_rects[i].left();
			out += copy_rects[i].top() * ctx->width;

			in += l;
			in += t * bitmap_rect.width();

			for (y = t; y < b; y++)
			{
				for (x = l; x < r; x++)
				{
					*out++ = *in++;
				}
				in += instride;
				out += outstride;
			}
		}

		ctx->repainted = true;
	}

public:
	MyDelegate(decctx *_ctx) : ctx(_ctx) {};
};

static void *Dec_Create(char *medianame)
{
	/*only respond to berkelium: media prefixes*/
	if (!strncmp(medianame, "berkelium:", 10))
		medianame = medianame + 10;
	else if (!strcmp(medianame, "berkelium"))
		medianame = "about:blank";
	else if (!strncmp(medianame, "http:", 5))
		medianame = medianame;	//and direct http requests.
	else
		return NULL;

	decctx *ctx = new decctx();

	Berkelium::Context* context = Berkelium::Context::create();
	ctx->width = 1024;
	ctx->height = 1024;
	ctx->repainted = false;
	ctx->buffer = (unsigned int*)malloc(ctx->width * ctx->height * 4);
	ctx->wnd = Berkelium::Window::create(context);
	delete context;

	ctx->wnd->setDelegate(new MyDelegate(ctx));


	ctx->wnd->resize(ctx->width, ctx->height);
	std::string url = medianame;
	ctx->wnd->navigateTo(Berkelium::URLString::point_to(url.data(), url.length()));

	return ctx;
}

static void *Dec_DisplayFrame(void *vctx, qboolean nosound, enum uploadfmt_e *fmt, int *width, int *height)
{
	decctx *ctx = (decctx*)vctx;
	*fmt = TF_BGRA32;
	*width = ctx->width;
	*height = ctx->height;

	if (!ctx->repainted)
		return NULL;
	ctx->repainted = false;
	return ctx->buffer;
}
static void Dec_Destroy(void *vctx)
{
	decctx *ctx = (decctx*)vctx;
	ctx->wnd->destroy();
	delete ctx;
}
static void Dec_GetSize (void *vctx, int *width, int *height)
{
	decctx *ctx = (decctx*)vctx;
	*width = ctx->width;
	*height = ctx->height;
}
static qboolean Dec_SetSize (void *vctx, int width, int height)
{
	decctx *ctx = (decctx*)vctx;
	if (ctx->width == width || ctx->height == height)
		return qtrue;	//no point

	//there's no resize notification. apparently javascript cannot resize windows. yay.
	unsigned int *newbuf = (unsigned int*)realloc(ctx->buffer, width * height * sizeof(*newbuf));
	if (!newbuf)
		return qfalse;	//failed?!?
	ctx->width = width;
	ctx->height = height;
	ctx->buffer = newbuf;
	ctx->repainted = false;

	ctx->wnd->resize(ctx->width, ctx->height);

	return qtrue;
}
static void Dec_CursorMove (void *vctx, float posx, float posy)
{
	decctx *ctx = (decctx*)vctx;
	ctx->wnd->mouseMoved((int)(posx * ctx->width), (int)(posy * ctx->height));
}
static void Dec_Key (void *vctx, int code, int unicode, int isup)
{
	decctx *ctx = (decctx*)vctx;
	wchar_t wchr = unicode;

	if (code >= 178 && code < 178+6)
		ctx->wnd->mouseButton(code - 178, !isup);
	else if (code == 188 || code == 189)
		ctx->wnd->mouseWheel(0, (code==189)?-30:30);
	else
	{
		if (code)
		{
			int mods = 0;
			if (code == 127)
				code = 0x2e;
			ctx->wnd->keyEvent(!isup, mods, code, 0);
		}
		if (unicode && !isup)
		{
			wchar_t chars[2] = {unicode};
			if (unicode == 127 || unicode == 8 || unicode == 9 || unicode == 27)
				return;
			ctx->wnd->textEvent(chars, 1);
		}
	}
}

static void Dec_ChangeStream(void *vctx, char *newstream)
{
	decctx *ctx = (decctx*)vctx;

	if (!strncmp(newstream, "cmd:", 4))
	{
		if (!strcmp(newstream+4, "refresh"))
			ctx->wnd->refresh();
		else if (!strcmp(newstream+4, "transparent"))
			ctx->wnd->setTransparent(true);
		else if (!strcmp(newstream+4, "opaque"))
			ctx->wnd->setTransparent(true);
		else if (!strcmp(newstream+4, "stop"))
			ctx->wnd->stop();
		else if (!strcmp(newstream+4, "back"))
			ctx->wnd->goBack();
		else if (!strcmp(newstream+4, "forward"))
			ctx->wnd->goForward();
		else if (!strcmp(newstream+4, "cut"))
			ctx->wnd->cut();
		else if (!strcmp(newstream+4, "copy"))
			ctx->wnd->copy();
		else if (!strcmp(newstream+4, "paste"))
			ctx->wnd->paste();
		else if (!strcmp(newstream+4, "del"))
			ctx->wnd->del();
		else if (!strcmp(newstream+4, "selectall"))
			ctx->wnd->selectAll();
	}
	else if (!strncmp(newstream, "javascript:", 11))
	{
		newstream+=11;
		int len = mblen(newstream, MB_CUR_MAX);
		wchar_t *wchrs = (wchar_t *)malloc((len+1)*2);
		len = mbstowcs(wchrs, newstream, len);
		ctx->wnd->executeJavascript(Berkelium::WideString::point_to(wchrs, len));
		free(wchrs);
	}
	else
	{
		std::string url = newstream;
		ctx->wnd->navigateTo(Berkelium::URLString::point_to(url.data(), url.length()));
	}
}

static bool Dec_Init(void)
{
	if (!Berkelium::init(Berkelium::FileString::empty()))
	{
		Con_Printf("Couldn't initialize Berkelium.\n");
		return false;
	}
	return true;
}

static int Dec_Tick(int *args)
{
	//need to keep it ticking over, if any work is to be done.
	Berkelium::update();
	return 0;
}

static int Dec_Shutdown(int *args)
{
	//force-kill all.
	Berkelium::destroy();
	return 0;
}

static media_decoder_funcs_t decoderfuncs =
{
	Dec_Create,
	Dec_DisplayFrame,
	NULL,//doneframe
	Dec_Destroy,
	NULL,//rewind

	Dec_CursorMove,
	Dec_Key,
	Dec_SetSize,
	Dec_GetSize,
	Dec_ChangeStream
};

int Plug_Init(int *args)
{
	if (!Plug_Export("Tick", Dec_Tick))
	{
		Con_Printf("Berkelium plugin failed: Engine doesn't support Tick feature\n");
		return false;
	}
	if (!Plug_Export("Shutdown", Dec_Shutdown))
	{
		Con_Printf("Berkelium plugin warning: Engine doesn't support Shutdown feature\n");
	//	return false;
	}
	if (!Plug_ExportNative("Media_VideoDecoder", &decoderfuncs))
	{
		Con_Printf("Berkelium plugin failed: Engine doesn't support media decoder plugins\n");
		return false;
	}
	return Dec_Init();
}
