//This is my attempt at wayland support for both opengl and vulkan.
//Note that this is sorely under-tested - I haven't tested vulkan-on-wayland at all as none of the drivers for nvidia support it.

//in no particular order...
//TODO: leaks on shutdown
//TODO: hardware cursors
//TODO: mouse grabs
//TODO: window decorations...
//TODO: kb autorepeat
//TODO: generic keymap (and not just UK)
//TODO: system clipboard
//TODO: drag+drop

#include "bothdefs.h"
#ifdef WAYLANDQUAKE
#include "gl_videgl.h"	//define this BEFORE the wayland stuff. This means the EGL types will have their (x11) defaults instead of getting mixed up with wayland. we expect to be able to use the void* verions instead for wayland anyway.
#include <wayland-client.h>
#include <wayland-egl.h>
#include <linux/input.h>	//this is shite.
#include "quakedef.h"
#if defined(GLQUAKE) && defined(USE_EGL)
#include "gl_draw.h"
#endif
#if defined(VKQUAKE)
#include "vk/vkrenderer.h"
#endif

#if WAYLAND_VERSION_MAJOR < 1
#error "wayland headers are too old"
#endif

#include "glquake.h"
#include "shader.h"

#if 1

static struct wl_display *(*pwl_display_connect)(const char *name);
static int (*pwl_display_dispatch)(struct wl_display *display);
static int (*pwl_display_dispatch_pending)(struct wl_display *display);
static int (*pwl_display_roundtrip)(struct wl_display *display);

static struct wl_proxy *(*pwl_proxy_marshal_constructor)(struct wl_proxy *proxy, uint32_t opcode, const struct wl_interface *interface, ...);
static struct wl_proxy *(*pwl_proxy_marshal_constructor_versioned)(struct wl_proxy *proxy, uint32_t opcode, const struct wl_interface *interface, uint32_t version, ...);
static void (*pwl_proxy_destroy)(struct wl_proxy *proxy);
static void (*pwl_proxy_marshal)(struct wl_proxy *p, uint32_t opcode, ...);
static int (*pwl_proxy_add_listener)(struct wl_proxy *proxy, void (**implementation)(void), void *data);

static const struct wl_interface		*pwl_keyboard_interface;
static const struct wl_interface		*pwl_pointer_interface;
static const struct wl_interface		*pwl_compositor_interface;
static const struct wl_interface		*pwl_region_interface;
static const struct wl_interface		*pwl_surface_interface;
static const struct wl_interface		*pwl_shell_surface_interface;
static const struct wl_interface		*pwl_shell_interface;
static const struct wl_interface		*pwl_seat_interface;
static const struct wl_interface		*pwl_registry_interface;

static dllfunction_t waylandexports_wl[] =
{
	{(void**)&pwl_display_connect,			"wl_display_connect"},
	{(void**)&pwl_display_dispatch,			"wl_display_dispatch"},
	{(void**)&pwl_display_dispatch_pending,		"wl_display_dispatch_pending"},
	{(void**)&pwl_display_roundtrip,		"wl_display_roundtrip"},
	{(void**)&pwl_proxy_marshal_constructor,	"wl_proxy_marshal_constructor"},
	{(void**)&pwl_proxy_marshal_constructor_versioned,"wl_proxy_marshal_constructor_versioned"},
	{(void**)&pwl_proxy_destroy,			"wl_proxy_destroy"},
	{(void**)&pwl_proxy_marshal,			"wl_proxy_marshal"},
	{(void**)&pwl_proxy_add_listener,		"wl_proxy_add_listener"},
	{(void**)&pwl_keyboard_interface,		"wl_keyboard_interface"},
	{(void**)&pwl_pointer_interface,		"wl_pointer_interface"},
	{(void**)&pwl_compositor_interface,		"wl_compositor_interface"},
	{(void**)&pwl_region_interface,			"wl_region_interface"},
	{(void**)&pwl_surface_interface,		"wl_surface_interface"},
	{(void**)&pwl_shell_surface_interface,		"wl_shell_surface_interface"},
	{(void**)&pwl_shell_interface,			"wl_shell_interface"},
	{(void**)&pwl_seat_interface,			"wl_seat_interface"},
	{(void**)&pwl_registry_interface,		"wl_registry_interface"},
	{NULL, NULL}
};
static dllhandle_t *lib_wayland_wl;
static qboolean WL_InitLibrary(void)
{
	lib_wayland_wl = Sys_LoadLibrary("libwayland-client.so.0", waylandexports_wl);
	if (!lib_wayland_wl)
		return false;
	return true;
}

#if defined(GLQUAKE) && defined(USE_EGL)
static struct wl_egl_window *(*pwl_egl_window_create)(struct wl_surface *surface, int width, int height);
static void (*pwl_egl_window_destroy)(struct wl_egl_window *egl_window);
static void (*pwl_egl_window_resize)(struct wl_egl_window *egl_window, int width, int height, int dx, int dy);
//static void (*pwl_egl_window_get_attached_size(struct wl_egl_window *egl_window, int *width, int *height);
static dllfunction_t waylandexports_egl[] =
{
	{(void**)&pwl_egl_window_create,		"wl_egl_window_create"},
	{(void**)&pwl_egl_window_destroy,		"wl_egl_window_destroy"},
	{(void**)&pwl_egl_window_resize,		"wl_egl_window_resize"},
//	{(void**)&pwl_egl_window_get_attached_size,	"wl_egl_window_get_attached_size"},
	{NULL, NULL}
};
static dllhandle_t *lib_wayland_egl;
#endif


//I hate wayland.
static inline struct wl_region *pwl_compositor_create_region(struct wl_compositor *wl_compositor)				{return (struct wl_region*)(struct wl_proxy *) pwl_proxy_marshal_constructor((struct wl_proxy *) wl_compositor, WL_COMPOSITOR_CREATE_REGION, pwl_region_interface, NULL);}
static inline struct wl_surface *pwl_compositor_create_surface(struct wl_compositor *wl_compositor)				{return (struct wl_surface *)(struct wl_proxy *) pwl_proxy_marshal_constructor((struct wl_proxy *) wl_compositor, WL_COMPOSITOR_CREATE_SURFACE, pwl_surface_interface, NULL);}
static inline void pwl_surface_set_opaque_region(struct wl_surface *wl_surface, struct wl_region *region)		{pwl_proxy_marshal((struct wl_proxy *) wl_surface, WL_SURFACE_SET_OPAQUE_REGION, region);}
static inline void pwl_region_add(struct wl_region *wl_region, int32_t x, int32_t y, int32_t width, int32_t height)	{pwl_proxy_marshal((struct wl_proxy *) wl_region, WL_REGION_ADD, x, y, width, height);}
static inline struct wl_shell_surface *pwl_shell_get_shell_surface(struct wl_shell *wl_shell, struct wl_surface *surface)	{return (struct wl_shell_surface *)(struct wl_proxy *) pwl_proxy_marshal_constructor((struct wl_proxy *) wl_shell, WL_SHELL_GET_SHELL_SURFACE, pwl_shell_surface_interface, NULL, surface);}
static inline void pwl_shell_surface_set_toplevel(struct wl_shell_surface *wl_shell_surface)					{pwl_proxy_marshal((struct wl_proxy *) wl_shell_surface, WL_SHELL_SURFACE_SET_TOPLEVEL);}
static inline void pwl_shell_surface_set_fullscreen(struct wl_shell_surface *wl_shell_surface, uint32_t method, uint32_t framerate, struct wl_output *output)	{pwl_proxy_marshal((struct wl_proxy *) wl_shell_surface, WL_SHELL_SURFACE_SET_FULLSCREEN, method, framerate, output);}
static inline int pwl_shell_surface_add_listener(struct wl_shell_surface *wl_shell_surface, const struct wl_shell_surface_listener *listener, void *data)		{return pwl_proxy_add_listener((struct wl_proxy *) wl_shell_surface, (void (**)(void)) listener, data);}
static inline void pwl_shell_surface_pong(struct wl_shell_surface *wl_shell_surface, uint32_t serial)			{pwl_proxy_marshal((struct wl_proxy *) wl_shell_surface, WL_SHELL_SURFACE_PONG, serial);}
static inline void pwl_shell_surface_set_title(struct wl_shell_surface *wl_shell_surface, const char *title)	{pwl_proxy_marshal((struct wl_proxy *) wl_shell_surface, WL_SHELL_SURFACE_SET_TITLE, title);}
static inline struct wl_registry *pwl_display_get_registry(struct wl_display *wl_display)	{return (struct wl_registry *)pwl_proxy_marshal_constructor((struct wl_proxy *) wl_display, WL_DISPLAY_GET_REGISTRY, pwl_registry_interface, NULL);}
static inline void *pwl_registry_bind(struct wl_registry *wl_registry, uint32_t name, const struct wl_interface *interface, uint32_t version)	{return (void*)pwl_proxy_marshal_constructor_versioned((struct wl_proxy *) wl_registry, WL_REGISTRY_BIND, interface, version, name, interface->name, version, NULL);}
static inline int pwl_registry_add_listener(struct wl_registry *wl_registry, const struct wl_registry_listener *listener, void *data)			{return pwl_proxy_add_listener((struct wl_proxy *) wl_registry, (void (**)(void)) listener, data);}
static inline void pwl_keyboard_destroy(struct wl_keyboard *wl_keyboard)			{pwl_proxy_destroy((struct wl_proxy *) wl_keyboard);}
static inline int pwl_keyboard_add_listener(struct wl_keyboard *wl_keyboard, const struct wl_keyboard_listener *listener, void *data)			{return pwl_proxy_add_listener((struct wl_proxy *) wl_keyboard, (void (**)(void)) listener, data);}
static inline void pwl_pointer_destroy(struct wl_pointer *wl_pointer)				{pwl_proxy_destroy((struct wl_proxy *) wl_pointer);}
static inline int pwl_pointer_add_listener(struct wl_pointer *wl_pointer, const struct wl_pointer_listener *listener, void *data)				{return pwl_proxy_add_listener((struct wl_proxy *) wl_pointer, (void (**)(void)) listener, data);}
static inline struct wl_pointer *pwl_seat_get_pointer(struct wl_seat *wl_seat)		{return (struct wl_pointer *)(struct wl_proxy *) pwl_proxy_marshal_constructor((struct wl_proxy *) wl_seat, WL_SEAT_GET_POINTER, pwl_pointer_interface, NULL);}
static inline struct wl_keyboard *pwl_seat_get_keyboard(struct wl_seat *wl_seat)	{return (struct wl_keyboard *)(struct wl_proxy *) pwl_proxy_marshal_constructor((struct wl_proxy *) wl_seat, WL_SEAT_GET_KEYBOARD, pwl_keyboard_interface, NULL);}
static inline int pwl_seat_add_listener(struct wl_seat *wl_seat, const struct wl_seat_listener *listener, void *data)	{return pwl_proxy_add_listener((struct wl_proxy *) wl_seat, (void (**)(void)) listener, data);}

#else
#define pwl_keyboard_interface		&wl_keyboard_interface
#define pwl_pointer_interface		&wl_pointer_interface
#define pwl_compositor_interface	&wl_compositor_interface
#define pwl_registry_interface		&wl_registry_interface
#define pwl_region_interface		&wl_region_interface
#define pwl_surface_interface		&wl_surface_interface
#define pwl_shell_surface_interface	&wl_shell_surface_interface
#define pwl_shell_interface			&wl_shell_interface
#define pwl_seat_interface			&wl_seat_interface
#endif
static const struct wl_interface *pzwp_relative_pointer_v1_interface;

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

	qboolean absmouse;
	struct zwp_relative_pointer_manager_v1 *relative_pointer_manager;
	struct zwp_relative_pointer_v1 *relative_pointer;

	//window stuff
#if defined(GLQUAKE) && defined(USE_EGL)
	struct wl_egl_window *enwindow;
#endif
	struct wl_surface *surface;
	struct wl_shell_surface *ssurface;
} w;
static void WL_shell_handle_ping(void *data, struct wl_shell_surface *shell_surface, uint32_t serial)
{
	pwl_shell_surface_pong(shell_surface, serial);
}
static void WL_shell_handle_configure(void *data, struct wl_shell_surface *shell_surface, uint32_t edges, int32_t width, int32_t height)
{
#if defined(GLQUAKE) && defined(USE_EGL)
	if (w.enwindow)
		pwl_egl_window_resize(w.enwindow, width, height, 0, 0);
#endif

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
static qbyte waylandinputsucksbighairydonkeyballs[] =
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
static qbyte waylandinputsucksbighairydonkeyballsshift[] =
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
		pwl_pointer_set_cursor(pointer, serial, NULL, 0, 0);
	else if (cursor) {
		image = display->default_cursor->images[0];
		buffer = pwl_cursor_image_get_buffer(image);
		pwl_pointer_set_cursor(pointer, serial,
				      display->cursor_surface,
				      image->hotspot_x,
				      image->hotspot_y);
		pwl_surface_attach(display->cursor_surface, buffer, 0, 0);
		pwl_surface_damage(display->cursor_surface, 0, 0,
				  image->width, image->height);
		pwl_surface_commit(display->cursor_surface);
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
	if (w.absmouse)
		IN_MouseMove(0, true, wl_fixed_to_double(sx), wl_fixed_to_double(sy), 0, 0);
}

static void WL_pointer_handle_button(void *data, struct wl_pointer *wl_pointer, uint32_t serial, uint32_t time, uint32_t button, uint32_t state)
{
//	struct wdisplay *display = data;
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
//		pwl_shell_surface_move(display->window->shell_surface, display->seat, serial);
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

struct zwp_relative_pointer_v1;
#define ZWP_RELATIVE_POINTER_MANAGER_V1_DESTROY 0
#define ZWP_RELATIVE_POINTER_MANAGER_V1_GET_RELATIVE_POINTER 1
#define ZWP_RELATIVE_POINTER_V1_DESTROY 0

static void WL_pointer_handle_delta(void *data, struct zwp_relative_pointer_v1 *pointer, uint32_t time_hi, uint32_t time_lo, wl_fixed_t dx_w, wl_fixed_t dy_w, wl_fixed_t dx_raw_w, wl_fixed_t dy_raw_w)
{
	if (!w.absmouse)
		IN_MouseMove(0, false, wl_fixed_to_double(dx_raw_w), wl_fixed_to_double(dy_raw_w), 0, 0);
}
struct zwp_relative_pointer_v1_listener
{
	void (*delta)(void *data, struct zwp_relative_pointer_v1 *pointer, uint32_t time_hi, uint32_t time_lo, wl_fixed_t dx_w, wl_fixed_t dy_w, wl_fixed_t dx_raw_w, wl_fixed_t dy_raw_w);
};
static const struct zwp_relative_pointer_v1_listener relative_pointer_listener =
{
	WL_pointer_handle_delta,
};

static void WL_BindRelativePointerManager(struct wl_registry *registry, uint32_t id)
{	/*oh hey, I wrote lots of code! pay me more! fuck that shit.*/

	static const struct wl_interface *types[8];
	static const struct wl_message zwp_relative_pointer_manager_v1_requests[] = {
		{ "destroy", "", types + 0 },
		{ "get_relative_pointer", "no", types + 6 },
	};
	static const struct wl_interface zwp_relative_pointer_manager_v1_interface = {
		"zwp_relative_pointer_manager_v1", 1,
		2, zwp_relative_pointer_manager_v1_requests,
		0, NULL,
	};
	static const struct wl_message zwp_relative_pointer_v1_requests[] = {
		{ "destroy", "", types + 0 },
	};
	static const struct wl_message zwp_relative_pointer_v1_events[] = {
		{ "relative_motion", "uuffff", types + 0 },
	};
	static const struct wl_interface zwp_relative_pointer_v1_interface = {
		"zwp_relative_pointer_v1", 1,
		1, zwp_relative_pointer_v1_requests,
		1, zwp_relative_pointer_v1_events,
	};

	//fix up types...
	types[6] = &zwp_relative_pointer_v1_interface;
	types[7] = pwl_pointer_interface;

	pzwp_relative_pointer_v1_interface = &zwp_relative_pointer_v1_interface;
	w.relative_pointer_manager = pwl_registry_bind(registry, id, &zwp_relative_pointer_manager_v1_interface, 1);
}

static void WL_keyboard_handle_keymap(void *data, struct wl_keyboard *keyboard, uint32_t format, int fd, uint32_t size)
{
}

static void WL_keyboard_handle_enter(void *data, struct wl_keyboard *keyboard, uint32_t serial, struct wl_surface *surface, struct wl_array *keys)
{
	vid.activeapp = true;
}

static void WL_keyboard_handle_leave(void *data, struct wl_keyboard *keyboard, uint32_t serial, struct wl_surface *surface)
{
	vid.activeapp = false;
}

static void WL_keyboard_handle_key(void *data, struct wl_keyboard *keyboard, uint32_t serial, uint32_t time, uint32_t key, uint32_t state)
{
	extern int		shift_down;
//	struct display *d = data;
	uint32_t qkey;
	uint32_t ukey;

	//FIXME: this stuff is fucked, especially the ukey stuff.
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
		s->pointer = pwl_seat_get_pointer(seat);
		pwl_pointer_add_listener(s->pointer, &pointer_listener, s);

		if (w.relative_pointer_manager)
		{	//and try and get relative pointer events too. so much fucking boilerplate.
			w.relative_pointer = (struct zwp_relative_pointer_v1 *)pwl_proxy_marshal_constructor((struct wl_proxy *) w.relative_pointer_manager, ZWP_RELATIVE_POINTER_MANAGER_V1_GET_RELATIVE_POINTER, pzwp_relative_pointer_v1_interface, NULL, w.pointer);
			pwl_proxy_add_listener((struct wl_proxy *) w.relative_pointer, (void (**)(void)) &relative_pointer_listener, &w);
		}
	}
	else if (!(caps & WL_SEAT_CAPABILITY_POINTER) && s->pointer)
	{
		pwl_pointer_destroy(s->pointer);
		s->pointer = NULL;

		if (w.relative_pointer)
		{
			pwl_proxy_marshal((struct wl_proxy *) w.relative_pointer, ZWP_RELATIVE_POINTER_V1_DESTROY);
			pwl_proxy_destroy((struct wl_proxy *) w.relative_pointer);
		}
	}

	if ((caps & WL_SEAT_CAPABILITY_KEYBOARD) && !s->keyboard)
	{
		s->keyboard = pwl_seat_get_keyboard(seat);
		pwl_keyboard_add_listener(s->keyboard, &keyboard_listener, s);
	}
	else if (!(caps & WL_SEAT_CAPABILITY_KEYBOARD) && s->keyboard)
	{
		pwl_keyboard_destroy(s->keyboard);
		s->keyboard = NULL;
	}
}
static const struct wl_seat_listener seat_listener =
{
	WL_seat_handle_capabilities
};


#if 0
static void WL_BindDecoraionManager(struct wl_registry *registry, uint32_t id)
{	/*oh hey, I wrote lots of code! pay me more! fuck that shit.*/

	static const struct wl_interface *types[3];
	static const struct wl_message zxdg_decoration_manager_v1_requests[] = {
		{ "destroy", "", types + 0 },
		{ "get_toplevel_decoration", "no", types + 1 },
	};
	static const struct wl_interface zxdg_decoration_manager_v1_interface = {
		"zxdg_decoration_manager_v1", 1,
		2, zxdg_decoration_manager_v1_requests,
		0, NULL,
	};
	static const struct wl_message zxdg_toplevel_decoration_v1_requests[] = {
		{ "destroy", "", types + 0 },
		{ "set_mode", "u", types + 0 },
		{ "unset_mode", "", types + 0 },
	};
	static const struct wl_message zxdg_toplevel_decoration_v1_events[] = {
		{ "configure", "u", types + 0 },
	};
	static const struct wl_interface zxdg_toplevel_decoration_v1_interface = {
		"zxdg_toplevel_decoration_v1", 1,
		3, zxdg_toplevel_decoration_v1_requests,
		1, zxdg_toplevel_decoration_v1_events,
	};

	//fix up types...
	types[1] = &zxdg_toplevel_decoration_v1_interface;
	types[2] = NULL;//&xdg_toplevel_interface;

//	pzwp_relative_pointer_v1_interface = &zxdg_toplevel_decoration_v1_interface;
	w.decoration_manager = pwl_registry_bind(registry, id, &zxdg_decoration_manager_v1_interface, 1);
}
#endif


static void WL_handle_global(void *data, struct wl_registry *registry, uint32_t id, const char *interface, uint32_t version)
{
	struct wdisplay_s *d = data;
Con_DLPrintf(2, "Wayland Interface %s id %u\n", interface, id);
	if (strcmp(interface, "wl_compositor") == 0)
		d->compositor = pwl_registry_bind(registry, id, pwl_compositor_interface, 1);
	else if (strcmp(interface, "wl_shell") == 0)
		d->shell = pwl_registry_bind(registry, id, pwl_shell_interface, 1);
	else if (strcmp(interface, "wl_seat") == 0 && !d->seat)
	{
		d->seat = pwl_registry_bind(registry, id, pwl_seat_interface, 1);
		pwl_seat_add_listener(d->seat, &seat_listener, d);
	}
	else if (strcmp(interface, "zwp_relative_pointer_manager_v1") == 0)
		WL_BindRelativePointerManager(registry, id);
//	else if (strcmp(interface, "zxdg_decoration_manager_v1") == 0)
//		WL_BindDecorationManager(registry, id);
//	else if (strcmp(interface, "zwp_pointer_constraints_v1") == 0)
//		d->shell = pwl_registry_bind(registry, id, pwl_shell_interface, 1);
/*	else if (!strcmp(interface, "input_device"))
		display_add_input(id);
*/
}

static const struct wl_registry_listener WL_registry_listener = {
	WL_handle_global
};

static void WL_SwapBuffers(void)
{
	TRACE(("WL_SwapBuffers\n"));

	switch(qrenderer)
	{
#if defined(GLQUAKE) && defined(USE_EGL)
	case QR_OPENGL:
		EGL_SwapBuffers();
		//wl_surface_damage(w.surface, 0, 0, vid.pixelwidth, vid.pixelheight);
		pwl_display_dispatch_pending(w.display);
		break;
#endif
	case QR_VULKAN: 	//the vulkan stuff handles this itself. FIXME: are we still receiving inputs? no idea!
	default:
		break;
	}

	w.absmouse = Key_MouseShouldBeFree() || !w.relative_pointer;
}

#ifdef VKQUAKE
static qboolean WLVK_SetupSurface(void)
{
	VkWaylandSurfaceCreateInfoKHR inf = {VK_STRUCTURE_TYPE_WAYLAND_SURFACE_CREATE_INFO_KHR};
    inf.flags = 0;
    inf.display = w.display;
    inf.surface = w.surface;

    if (VK_SUCCESS == vkCreateWaylandSurfaceKHR(vk.instance, &inf, vkallocationcb, &vk.surface))
        return true;
    return false;
}
#endif

static qboolean WL_Init (rendererstate_t *info, unsigned char *palette)
{
	if (!WL_InitLibrary())
	{
		Con_Printf("couldn't load wayland client libraries\n");
		return false;
	}
	switch(qrenderer)
	{
#if defined(GLQUAKE) && defined(USE_EGL)
	case QR_OPENGL:
		lib_wayland_egl = Sys_LoadLibrary("libwayland-egl.so.1", waylandexports_egl);
		if (!lib_wayland_egl)
		{
			Con_Printf("couldn't load libwayland-egl.so.1 library\n");
			return false;
		}

//		setenv("EGL_PLATFORM", "wayland", 1);	//if this actually matters then we're kinda screwed
		if (!EGL_LoadLibrary(info->subrenderer))
		{
			Con_Printf("couldn't load EGL library\n");
			return false;
		}

		break;
#endif
#ifdef VKQUAKE
	case QR_VULKAN:
		#ifdef VK_NO_PROTOTYPES
		{	//vulkan requires that vkGetInstanceProcAddr is set in advance.
			dllfunction_t func[] =
			{
				{(void*)&vkGetInstanceProcAddr,	"vkGetInstanceProcAddr"},
				{NULL,							NULL}
			};

			if (!Sys_LoadLibrary("libvulkan.so.1", func))
			{
				if (!Sys_LoadLibrary("libvulkan.so", func))
				{
					Con_Printf("Couldn't intialise libvulkan.so\nvulkan loader is not installed\n");
					return false;
				}
			}
		}
		#endif
		break;
#endif
	default:
		return false;	//not supported dude...
	}

	memset(&w, 0, sizeof(w));
	w.display = pwl_display_connect(NULL);
	if (!w.display)
	{
		Con_Printf("couldn't connect to wayland server\n");
		return false;
	}
	w.registry = pwl_display_get_registry(w.display);
	pwl_registry_add_listener(w.registry, &WL_registry_listener, &w);	//w.compositor =
	pwl_display_dispatch(w.display);
	pwl_display_roundtrip(w.display);
	if (!w.compositor)
	{
		Con_Printf("no compositor running, apparently\n");
		return false;
	}

	w.surface = pwl_compositor_create_surface(w.compositor);
	if (!w.surface)
	{
		Con_Printf("no compositor running, apparently\n");
		return false;
	}
	w.ssurface = pwl_shell_get_shell_surface(w.shell, w.surface);
	
	if (info->fullscreen)
		pwl_shell_surface_set_fullscreen(w.ssurface, WL_SHELL_SURFACE_FULLSCREEN_METHOD_DEFAULT, 60, NULL);
	else
		pwl_shell_surface_set_toplevel(w.ssurface);

	{
		struct wl_region *region = pwl_compositor_create_region(w.compositor);
		pwl_region_add(region, 0, 0, info->width, info->height);
		pwl_surface_set_opaque_region(w.surface, region);
		//FIXME: leaks region?
	}

	pwl_shell_surface_add_listener(w.ssurface, &shell_surface_listener, &w);

	vid.pixelwidth = info->width;
	vid.pixelheight = info->height;

	vid.activeapp = true;

	//window_set_keyboard_focus_handler(window, WL_handler_keyfocus);
	//window_set_resize_handler(w.surface, WL_handler_resize);

	switch(qrenderer)
	{
	default:
		return false;
#ifdef VKQUAKE
	case QR_VULKAN:
		{
			const char *extnames[] = {VK_KHR_WAYLAND_SURFACE_EXTENSION_NAME, NULL};
			if (VK_Init(info, extnames, WLVK_SetupSurface, NULL))
				return true;
			Con_Printf(CON_ERROR "Unable to initialise vulkan-on-wayland.\n");
			return false;
		}
		break;
#endif
#if defined(GLQUAKE) && defined(USE_EGL)
	case QR_OPENGL:
		w.enwindow = pwl_egl_window_create(w.surface, info->width, info->height);
		if (!EGL_Init(info, palette, EGL_PLATFORM_WAYLAND_KHR, w.enwindow, w.display, (EGLNativeWindowType)w.enwindow, (EGLNativeDisplayType)w.display))
		{
			Con_Printf("couldn't initialise EGL context\n");
			return false;
		}

		pwl_display_dispatch_pending(w.display);

		return GL_Init(info, &EGL_Proc);
		break;
#endif
	}
	return true;
}
static void WL_DeInit(void)
{
#if defined(GLQUAKE) && defined(USE_EGL)
	EGL_Shutdown();
	if (w.enwindow)
		pwl_egl_window_destroy(w.enwindow);
#endif
}
static qboolean WL_ApplyGammaRamps(unsigned int gammarampsize, unsigned short *ramps)
{
	//not supported
	return false;
}
static void WL_SetCaption(const char *text)
{
	pwl_shell_surface_set_title(w.ssurface, text);
}

static int WL_GetPriority(void)
{
	//2 = above x11, 0 = below x11.
	char *stype = getenv("XDG_SESSION_TYPE");
	char *dpyname;
	if (!strcmp(stype, "wayland"))
		return 2;
	if (!strcmp(stype, "x11"))
		return 0;
	if (!strcmp(stype, "tty"))	//FIXME: support this!
		return 0;

	//otherwise if both WAYLAND_DISPLAY and DISPLAY are defined, then we assume that we were started from xwayland wrapper thing, and that the native/preferred windowing system is wayland.
	//(lets just hope our wayland support is comparable)

	dpyname = getenv("WAYLAND_DISPLAY");
	if (dpyname && *dpyname)
		return 2;	//something above X11.
	return 0;	//default.
}

#if defined(GLQUAKE) && defined(USE_EGL)
rendererinfo_t rendererinfo_wayland_gl =
{
	"OpenGL (Wayland)",
	{
		"wlgl",
		"wayland",
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
	NULL,	//CreateCursor
	NULL,	//SetCursor
	NULL,	//DestroyCursor
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

	"",
	WL_GetPriority
};
#endif

#ifdef VKQUAKE
rendererinfo_t rendererinfo_wayland_vk =
{
    "Vulkan (Wayland)",
    {
        "wlvk",
        "vk",
        "vulkan",
        "wayland"
    },
    QR_VULKAN,

    VK_Draw_Init,
    VK_Draw_Shutdown,

    VK_UpdateFiltering,
    VK_LoadTextureMips,
    VK_DestroyTexture,

    VK_R_Init,
    VK_R_DeInit,
    VK_R_RenderView,

    WL_Init,
    WL_DeInit,
    WL_SwapBuffers,
    WL_ApplyGammaRamps,

    NULL,
    NULL,
    NULL,
    WL_SetCaption,       //setcaption
    VKVID_GetRGBInfo,


    VK_SCR_UpdateScreen,

    VKBE_SelectMode,
    VKBE_DrawMesh_List,
    VKBE_DrawMesh_Single,
    VKBE_SubmitBatch,
    VKBE_GetTempBatch,
    VKBE_DrawWorld,
    VKBE_Init,
    VKBE_GenBrushModelVBO,
    VKBE_ClearVBO,
    VKBE_UploadAllLightmaps,
    VKBE_SelectEntity,
    VKBE_SelectDLight,
    VKBE_Scissor,
    VKBE_LightCullModel,

    VKBE_VBO_Begin,
    VKBE_VBO_Data,
    VKBE_VBO_Finish,
    VKBE_VBO_Destroy,

    VKBE_RenderToTextureUpdate2d,

    "",
	WL_GetPriority
};
#endif

#endif

