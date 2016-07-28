#include "bothdefs.h"
#if defined(GLQUAKE) && defined(USE_EGL)
#include <wayland-client.h>
#include <wayland-egl.h>
#include <linux/input.h>	//this is shite.
#include "gl_videgl.h"

#if WAYLAND_VERSION_MAJOR < 1
#error "wayland headers are too old"
#endif

#include "glquake.h"
#include "shader.h"

static struct wdisplay_s
{
	//display stuff
	struct wl_display *display;
	struct wl_registry *registry;
	struct wl_compositor *compositor;
	struct wl_shell *shell;

	//seat stuff
	void *pointer;
	void *keyboard;
	void *seat;

	//window stuff
	struct wl_egl_window *enwindow;
	struct wl_surface *surface;
	struct wl_shell_surface *ssurface;
} w;
static void WL_shell_handle_ping(void *data, struct wl_shell_surface *shell_surface, uint32_t serial)
{
	wl_shell_surface_pong(shell_surface, serial);
}
static void WL_shell_handle_configure(void *data, struct wl_shell_surface *shell_surface, uint32_t edges, int32_t width, int32_t height)
{
	if (w.enwindow)
		wl_egl_window_resize(w.enwindow, width, height, 0, 0);

	vid.pixelwidth = width;
	vid.pixelheight = height;
}
static void WL_shell_handle_popup_done(void *data, struct wl_shell_surface *shell_surface)
{
}

static const struct wl_shell_surface_listener shell_surface_listener =
{
	WL_shell_handle_ping,
	WL_shell_handle_configure,
	WL_shell_handle_popup_done
};

//qkeys are ascii-compatible for the most part.
qbyte waylandinputsucksbighairydonkeyballs[] =
{
	0, 	K_ESCAPE,'1','2','3','4','5','6',	//0x
	'7','8','9','0','-','=',K_BACKSPACE,K_TAB,
	'q','w','e','r','t','y','u','i',		//1x
	'o','p','[',']',K_ENTER,K_LCTRL,'a', 's',
	'd','f','g','h','j','k','l',';',		//2x
	'\'','`',K_LSHIFT,'#','z','x','c','v',
	'b','n','m',',','.','/',K_RSHIFT,K_KP_STAR,	//3x
	K_LALT,' ',K_CAPSLOCK,K_F1,K_F2,K_F3,K_F4,K_F5,
	K_F6,K_F7,K_F8,K_F9,K_F10,K_KP_NUMLOCK,K_SCRLCK,K_KP_HOME,//4x
	K_KP_UPARROW,K_KP_PGUP,K_KP_MINUS,K_KP_LEFTARROW,K_KP_5,K_KP_RIGHTARROW,K_KP_PLUS,K_KP_END,
	K_KP_DOWNARROW,K_KP_PGDN,K_KP_INS,K_KP_DEL,0,0,'\\',K_F11,	//5x
	K_F12,0,0,0,0,0,0,0,
	K_KP_ENTER,0,K_KP_SLASH,0,K_RALT,0,K_HOME,K_UPARROW,	//6x
	K_PGUP,K_LEFTARROW,K_RIGHTARROW,K_END,K_DOWNARROW,K_PGDN,K_INS,K_DEL,
	0,0,0,0,0,0,0,K_PAUSE,	//7x
	0,0,0,0,0,K_LWIN,K_RWIN,K_APP	
};
qbyte waylandinputsucksbighairydonkeyballsshift[] =
{
	0, 	K_ESCAPE,'!','\"','3','$','%','^',	//0x
	'&','*','(',')','_','+',K_BACKSPACE,K_TAB,
	'Q','W','E','R','T','Y','U','I',		//1x
	'O','P','{','}',K_ENTER,K_LCTRL,'A', 'S',
	'D','F','G','H','J','K','L',':',		//2x
	'@','`',K_LSHIFT,'~','Z','X','C','V',
	'B','N','M','<','>','?',K_RSHIFT,K_KP_STAR,	//3x
	K_LALT,' ',K_CAPSLOCK,K_F1,K_F2,K_F3,K_F4,K_F5,
	K_F6,K_F7,K_F8,K_F9,K_F10,K_KP_NUMLOCK,K_SCRLCK,K_KP_HOME,//4x
	K_KP_UPARROW,K_KP_PGUP,K_KP_MINUS,K_KP_LEFTARROW,K_KP_5,K_KP_RIGHTARROW,K_KP_PLUS,K_KP_END,
	K_KP_DOWNARROW,K_KP_PGDN,K_KP_INS,K_KP_DEL,0,0,'|',K_F11,	//5x
	K_F12,0,0,0,0,0,0,0,
	K_KP_ENTER,0,K_KP_SLASH,0,K_RALT,0,K_HOME,K_UPARROW,	//6x
	K_PGUP,K_LEFTARROW,K_RIGHTARROW,K_END,K_DOWNARROW,K_PGDN,K_INS,K_DEL,
	0,0,0,0,0,0,0,K_PAUSE,	//7x
	0,0,0,0,0,K_LWIN,K_RWIN,K_APP	
};
static void WL_pointer_handle_enter(void *data, struct wl_pointer *pointer, uint32_t serial, struct wl_surface *surface, wl_fixed_t sx, wl_fixed_t sy)
{
/*
	struct display *display = data;
	struct wl_buffer *buffer;
	struct wl_cursor *cursor = display->default_cursor;
	struct wl_cursor_image *image;

	if (display->window->fullscreen)
		wl_pointer_set_cursor(pointer, serial, NULL, 0, 0);
	else if (cursor) {
		image = display->default_cursor->images[0];
		buffer = wl_cursor_image_get_buffer(image);
		wl_pointer_set_cursor(pointer, serial,
				      display->cursor_surface,
				      image->hotspot_x,
				      image->hotspot_y);
		wl_surface_attach(display->cursor_surface, buffer, 0, 0);
		wl_surface_damage(display->cursor_surface, 0, 0,
				  image->width, image->height);
		wl_surface_commit(display->cursor_surface);
	}
*/
}
static void WL_pointer_handle_leave(void *data, struct wl_pointer *pointer, uint32_t serial, struct wl_surface *surface)
{
}

static void WL_pointer_handle_motion(void *data, struct wl_pointer *pointer, uint32_t time, wl_fixed_t sx, wl_fixed_t sy)
{
	//wayland is shite shite shite.
	//1.4 still has no relative mouse motion.
	IN_MouseMove(0, true, wl_fixed_to_double(sx), wl_fixed_to_double(sy), 0, 0);
}

static void WL_pointer_handle_button(void *data, struct wl_pointer *wl_pointer, uint32_t serial, uint32_t time, uint32_t button, uint32_t state)
{
	struct wdisplay *display = data;
	int qkey;

	switch(button)
	{
	default:
		return;	//blurgh.
	case BTN_LEFT:
		qkey = K_MOUSE1;
		break;
	case BTN_RIGHT:
		qkey = K_MOUSE2;
		break;
	case BTN_MIDDLE:
		qkey = K_MOUSE3;
		break;
	case BTN_SIDE:
		qkey = K_MOUSE4;
		break;
	case BTN_EXTRA:
		qkey = K_MOUSE5;
		break;
	case BTN_FORWARD:
		qkey = K_MOUSE6;
		break;
	case BTN_BACK:
		qkey = K_MOUSE7;
		break;
	case BTN_TASK:
		qkey = K_MOUSE8;
		break;
	}
	IN_KeyEvent(0, !!state, qkey, 0);
//		wl_shell_surface_move(display->window->shell_surface, display->seat, serial);
}

static void WL_pointer_handle_axis(void *data, struct wl_pointer *wl_pointer, uint32_t time, uint32_t axis, wl_fixed_t value)
{
	if (value < 0)
	{
		IN_KeyEvent(0, 1, K_MWHEELUP, 0);
		IN_KeyEvent(0, 0, K_MWHEELUP, 0);
	}
	else
	{
		IN_KeyEvent(0, 1, K_MWHEELDOWN, 0);
		IN_KeyEvent(0, 0, K_MWHEELDOWN, 0);
	}
}

static const struct wl_pointer_listener pointer_listener =
{
	WL_pointer_handle_enter,
	WL_pointer_handle_leave,
	WL_pointer_handle_motion,
	WL_pointer_handle_button,
	WL_pointer_handle_axis,
};

static void WL_keyboard_handle_keymap(void *data, struct wl_keyboard *keyboard, uint32_t format, int fd, uint32_t size)
{
}

static void WL_keyboard_handle_enter(void *data, struct wl_keyboard *keyboard, uint32_t serial, struct wl_surface *surface, struct wl_array *keys)
{
}

static void WL_keyboard_handle_leave(void *data, struct wl_keyboard *keyboard, uint32_t serial, struct wl_surface *surface)
{
}

static void WL_keyboard_handle_key(void *data, struct wl_keyboard *keyboard, uint32_t serial, uint32_t time, uint32_t key, uint32_t state)
{
	extern int		shift_down;
	struct display *d = data;
	uint32_t qkey;
	uint32_t ukey;

	if (key < sizeof(waylandinputsucksbighairydonkeyballs)/sizeof(waylandinputsucksbighairydonkeyballs[0]))
	{
		qkey = waylandinputsucksbighairydonkeyballs[key];
		if (shift_down)
			ukey = waylandinputsucksbighairydonkeyballsshift[key];
		else
			ukey = waylandinputsucksbighairydonkeyballs[key];
	}
	else
		ukey = qkey = 0;
	if (ukey < ' ' || ukey > 127)
		ukey = 0;

	if (state)
		IN_KeyEvent(0, 1, qkey, ukey);
	else
		IN_KeyEvent(0, 0, qkey, 0);
}

static void WL_keyboard_handle_modifiers(void *data, struct wl_keyboard *keyboard, uint32_t serial, uint32_t mods_depressed, uint32_t mods_latched, uint32_t mods_locked, uint32_t group)
{
}

static const struct wl_keyboard_listener keyboard_listener =
{
	WL_keyboard_handle_keymap,
	WL_keyboard_handle_enter,
	WL_keyboard_handle_leave,
	WL_keyboard_handle_key,
	WL_keyboard_handle_modifiers
};
static void WL_seat_handle_capabilities(void *data, struct wl_seat *seat, enum wl_seat_capability caps)
{
	struct wdisplay_s *s = data;
	if ((caps & WL_SEAT_CAPABILITY_POINTER) && !s->pointer)
	{
		s->pointer = wl_seat_get_pointer(seat);
		wl_pointer_add_listener(s->pointer, &pointer_listener, s);
	}
	else if (!(caps & WL_SEAT_CAPABILITY_POINTER) && s->pointer)
	{
		wl_pointer_destroy(s->pointer);
		s->pointer = NULL;
	}

	if ((caps & WL_SEAT_CAPABILITY_KEYBOARD) && !s->keyboard)
	{
		s->keyboard = wl_seat_get_keyboard(seat);
		wl_keyboard_add_listener(s->keyboard, &keyboard_listener, s);
	}
	else if (!(caps & WL_SEAT_CAPABILITY_KEYBOARD) && s->keyboard)
	{
		wl_keyboard_destroy(s->keyboard);
		s->keyboard = NULL;
	}
}
static const struct wl_seat_listener seat_listener =
{
	WL_seat_handle_capabilities
};

static void WL_handle_global(void *data, struct wl_registry *registry,
uint32_t id, const char *interface, uint32_t version)
{
	struct wdisplay_s *d = data;
//Sys_Printf("Interface %s id %u\n", interface, id);
	if (strcmp(interface, "wl_compositor") == 0)
		d->compositor = wl_registry_bind(registry, id, &wl_compositor_interface, 1);
	else if (strcmp(interface, "wl_shell") == 0)
		d->shell = wl_registry_bind(registry, id, &wl_shell_interface, 1);
	else if (strcmp(interface, "wl_seat") == 0 && !d->seat)
	{
		d->seat = wl_registry_bind(registry, id, &wl_seat_interface, 1);
		wl_seat_add_listener(d->seat, &seat_listener, d);
	}
/*	else if (!strcmp(interface, "input_device"))
		display_add_input(id);
*/
}

static const struct wl_registry_listener WL_registry_listener = {
	WL_handle_global
};

static void WL_waitabit(void)
{
    wl_display_roundtrip(w.display);
}

static void WL_SwapBuffers(void)
{
	float r;
	TRACE(("WL_SwapBuffers\n"));

	wl_surface_set_opaque_region(w.surface, NULL);

	EGL_SwapBuffers();
	//wl_surface_damage(w.surface, 0, 0, vid.pixelwidth, vid.pixelheight);
	wl_display_dispatch_pending(w.display);
}

static qboolean WL_Init (rendererstate_t *info, unsigned char *palette)
{
	cvar_t *v;
	w.display = wl_display_connect(NULL);
	if (!w.display)
	{
		Con_Printf("couldn't connect to wayland server\n");
		return false;
	}
	w.registry = wl_display_get_registry(w.display);
	wl_registry_add_listener(w.registry, &WL_registry_listener, &w);	//w.compositor =

	v = Cvar_FindVar("gl_menutint_shader");
	if (v && v->ival)
	{
		Con_Printf("Disabling gl_menutint_shader to avoid wayland/mesa EGL bugs.\n");
		Cvar_SetValue(v, 0);
	}
	v = Cvar_FindVar("r_waterstyle");
	if (v && v->ival>1)
	{
		Con_Printf("Disabling r_waterstyle to avoid wayland/mesa EGL bugs.\n");
		Cvar_SetValue(v, 1);
	}
	v = Cvar_FindVar("r_slimestyle");
	if (v && v->ival>1)
	{
		Con_Printf("Disabling r_slimestyle to avoid wayland/mesa EGL bugs.\n");
		Cvar_SetValue(v, 1);
	}
	v = Cvar_FindVar("r_lavastyle");
	if (v && v->ival>1)
	{
		Con_Printf("Disabling r_lavastyle to avoid wayland/mesa EGL bugs.\n");
		Cvar_SetValue(v, 1);
	}

	WL_waitabit();

	if (!w.compositor)
	{
		Con_Printf("no compositor running, apparently\n");
		return false;
	}

	w.surface = wl_compositor_create_surface(w.compositor);
	w.ssurface = wl_shell_get_shell_surface(w.shell, w.surface);
	wl_shell_surface_add_listener(w.ssurface, &shell_surface_listener, &w);
	w.enwindow = wl_egl_window_create(w.surface, info->width, info->height);

	vid.pixelwidth = info->width;
	vid.pixelheight = info->height;

	setenv("EGL_PLATFORM", "wayland", 1);
	if (!EGL_LoadLibrary(info->subrenderer))
	{
		Con_Printf("couldn't load EGL library\n");
		return false;
	}

	if (!EGL_Init(info, palette, (EGLNativeWindowType)w.enwindow, (EGLNativeDisplayType) w.display))
	{
		Con_Printf("couldn't initialise EGL context\n");
		return false;
	}

	if (info->fullscreen)
		wl_shell_surface_set_fullscreen(w.ssurface, WL_SHELL_SURFACE_FULLSCREEN_METHOD_DEFAULT, 60, NULL);
	else
		wl_shell_surface_set_toplevel(w.ssurface);


	//window_set_keyboard_focus_handler(window, WL_handler_keyfocus);
	//window_set_resize_handler(w.surface, WL_handler_resize);

	wl_display_dispatch_pending(w.display);

	GL_Init(&EGL_Proc);

//	while(1)
//		WL_SwapBuffers();
	return true;
}
static void WL_DeInit(void)
{
	EGL_Shutdown();
	wl_egl_window_destroy(w.enwindow);
}
static qboolean WL_ApplyGammaRamps(unsigned int gammarampsize, unsigned short *ramps)
{
	//not supported
	return false;
}
static void WL_SetCaption(char *text)
{
	wl_shell_surface_set_title(w.ssurface, text);
}

#include "gl_draw.h"
rendererinfo_t waylandrendererinfo =
{
    "Wayland",
    {
        "wayland"
    },
    QR_OPENGL,

    GLDraw_Init,
    GLDraw_DeInit,

	GL_UpdateFiltering,
	GL_LoadTextureMips,
	GL_DestroyTexture,

    GLR_Init,
    GLR_DeInit,
    GLR_RenderView,

    WL_Init,
    WL_DeInit,
	WL_SwapBuffers,
    WL_ApplyGammaRamps,
    WL_SetCaption,       //setcaption
    GLVID_GetRGBInfo,


    GLSCR_UpdateScreen,

    GLBE_SelectMode,
    GLBE_DrawMesh_List,
    GLBE_DrawMesh_Single,
    GLBE_SubmitBatch,
    GLBE_GetTempBatch,
    GLBE_DrawWorld,
    GLBE_Init,
    GLBE_GenBrushModelVBO,
    GLBE_ClearVBO,
    GLBE_UploadAllLightmaps,
    GLBE_SelectEntity,
    GLBE_SelectDLight,
    GLBE_Scissor,
    GLBE_LightCullModel,

    GLBE_VBO_Begin,
    GLBE_VBO_Data,
    GLBE_VBO_Finish,
    GLBE_VBO_Destroy,

    GLBE_RenderToTextureUpdate2d,

    ""
};

#endif

