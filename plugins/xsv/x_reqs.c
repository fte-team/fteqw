#include "../plugin.h"
#include <math.h>

#include "qux.h"
#undef strncpy

XRequest XRequests [256];

#ifdef XBigReqExtensionName
int	X_BigReqCode;
#endif

void XR_MapWindow(xclient_t *cl, xReq *request);
void XR_UnmapWindow(xclient_t *cl, xReq *request);


#define	GXclear			0x0		/* 0 */
#define GXand			0x1		/* src AND dst */
#define GXandReverse		0x2		/* src AND NOT dst */
#define GXcopy			0x3		/* src */
#define GXandInverted		0x4		/* NOT src AND dst */
#define	GXnoop			0x5		/* dst */
#define GXxor			0x6		/* src XOR dst */
#define GXor			0x7		/* src OR dst */
#define GXnor			0x8		/* NOT src AND NOT dst */
#define GXequiv			0x9		/* NOT src XOR dst */
#define GXinvert		0xa		/* NOT dst */
#define GXorReverse		0xb		/* src OR NOT dst */
#define GXcopyInverted		0xc		/* NOT src */
#define GXorInverted		0xd		/* NOT src OR dst */
#define GXnand			0xe		/* NOT src OR NOT dst */
#define GXset			0xf		/* 1 */

#define GCFunc(src, dst, fnc, out, setval)	\
switch(fnc)							\
{									\
case GXclear:						\
	out = 0;						\
	break;							\
case GXand:							\
	out = src&dst;					\
	break;							\
case GXandReverse:					\
	out = src&~dst;					\
	break;							\
case GXcopy:						\
	out = src;						\
	break;							\
case GXandInverted:					\
	out = ~src&dst;					\
	break;							\
case GXnoop:						\
	out = dst;						\
	break;							\
case GXxor:							\
	out = src^dst;					\
	break;							\
case GXor:							\
	out = src|dst;					\
	break;							\
case GXnor:							\
	out = ~src&~dst;				\
	break;							\
case GXequiv:						\
	out = ~src^dst;					\
	break;							\
case GXinvert:						\
	out = ~dst;						\
	break;							\
case GXorReverse:					\
	out = src|~dst;					\
	break;							\
case GXcopyInverted:				\
	out = ~src;						\
	break;							\
case GXorInverted:					\
	out = ~src|dst;					\
	break;							\
case GXnand:						\
	out = ~src|~dst;				\
	break;							\
case GXset:							\
	out = setval;					\
	break;							\
}
void XW_ClearArea(xwindow_t *wnd, int xp, int yp, int width, int height);

void XR_QueryExtension (xclient_t *cl, xReq *request)
{
	char extname[256];
	xQueryExtensionReply rep;
	xQueryExtensionReq *req = (xQueryExtensionReq *)request;

	if (req->nbytes > sizeof(extname)-1)
		req->nbytes = sizeof(extname)-1;
	memcpy(extname, (char *)(req+1), req->nbytes);
	extname[req->nbytes] = '\0';

#ifdef XBigReqExtensionName
	if (X_BigReqCode && !strcmp(extname, XBigReqExtensionName))
	{
		rep.major_opcode	= X_BigReqCode;
		rep.present			= true;
		rep.first_event		= 0;
		rep.first_error		= 0;
	}
	else
#endif
	{
		Con_Printf("Extension %s not supported\n", extname);
		rep.major_opcode	= 0;
		rep.present			= false;
		rep.first_event		= 0;
		rep.first_error		= 0;
	}

    rep.type			= X_Reply;
    rep.pad1			= 0;
    rep.sequenceNumber	= cl->requestnum;
    rep.length			= 0;
    rep.pad3			= 0;
    rep.pad4			= 0;
    rep.pad5			= 0;
    rep.pad6			= 0;
    rep.pad7			= 0;

	X_SendData(cl, &rep, sizeof(rep));
}


void XW_ExposeWindowRegionInternal(xwindow_t *root, int x, int y, int width, int height)
{
	int nx,ny,nw,nh;
	xEvent ev;
	if (!root->mapped || root->inputonly)
		return;


	ev.u.u.type						= VisibilityNotify;
	ev.u.u.detail					= 0;
	ev.u.u.sequenceNumber			= 0;
	ev.u.visibility.window			= root->res.id;
	ev.u.visibility.state			= VisibilityUnobscured;
	ev.u.visibility.pad1			= 0;
	ev.u.visibility.pad2			= 0;
	ev.u.visibility.pad3			= 0;

	X_SendNotificationMasked(&ev, root, VisibilityChangeMask);

	ev.u.u.type						= Expose;
	ev.u.u.detail					= 0;
	ev.u.u.sequenceNumber			= 0;
	ev.u.expose.window				= root->res.id;
	ev.u.expose.x					= x;
	ev.u.expose.y					= y;
	ev.u.expose.width				= width;
	ev.u.expose.height				= height;
	ev.u.expose.count				= false;	//other expose events following (none - rewrite to group these then send all in one go...)
	ev.u.expose.pad2				= 0;

	X_SendNotificationMasked(&ev, root, ExposureMask);

	if (root->buffer && root != rootwindow)
	{
//		XW_ClearArea(root, 0, 0, root->width, root->height);
//		free(root->buffer);
//		root->buffer = NULL;
	}

	for (root = root->child; root; root = root->sibling)
	{
		if (!root->mapped || root->inputonly)
			continue;

		//subtract the minpos
		nx = x - root->xpos;	
		nw = width;
		ny = y - root->ypos;
		nh = height;

		//cap new minpos to the child window.
		if (nx < 0)
		{
			nw += nx;
			nx = 0;
		}
		if (ny < 0)
		{
			nh += ny;
			ny = 0;
		}

		//cap new maxpos
		if (nx+nw > x + root->width)
			nw = x+root->width - nx;
		if (ny+nh > y + root->height)
			nh = y+root->height - ny;

		if (nw > 0 && nh > 0)	//make sure some is valid.
			XW_ExposeWindowRegionInternal(root, nx, ny, nw, nh);
	}
}

void XW_ExposeWindow(xwindow_t *root, int x, int y, int width, int height)
{//we have to go back to the root so we know the exact region, and can expose our sibling's windows.
	while(root)
	{
		x += root->xpos;
		y += root->ypos;
		root = root->parent;
	}
	
	XW_ExposeWindowRegionInternal(rootwindow, x, y, width, height);
}

void XR_ListExtensions (xclient_t *cl, xReq *request)
{
	char buffer[8192];
	xListExtensionsReply *rep = (xListExtensionsReply *)buffer;
	char *out;

    rep->type			= X_Reply;
    rep->nExtensions	= 0;
    rep->sequenceNumber	= cl->requestnum;
    rep->length			= 0;
    rep->pad2			= 0;
    rep->pad3			= 0;
    rep->pad4			= 0;
    rep->pad5			= 0;
    rep->pad6			= 0;
    rep->pad7			= 0;

	out = (char *)(rep+1);

#ifdef XBigReqExtensionName
	rep->nExtensions++;
	strcpy(out, XBigReqExtensionName);
	out+=strlen(out)+1;
#endif

	rep->length = (out-(char *)(rep+1) + 3)/4;


	X_SendData(cl, rep, sizeof(xListExtensionsReply) + rep->length*4);
}

void XR_SetCloseDownMode(xclient_t *cl, xReq *request)
{
	xSetCloseDownModeReq *req = (xSetCloseDownModeReq*)request;

	switch(req->mode)
	{
	case DestroyAll:
	case RetainPermanent:
	case RetainTemporary:
		break;
	default:
		X_SendError(cl, BadValue, req->mode, X_SetCloseDownMode, 0);
		return;
	}
	cl->closedownmode = req->mode;
}

void XR_GetAtomName (xclient_t *cl, xReq *request)
{
	xResourceReq *req = (xResourceReq*)request;
	char buffer[8192];
	xGetAtomNameReply *rep = (xGetAtomNameReply*)buffer;

	xatom_t	*xa;

	if (XS_GetResource(req->id, (void**)&xa) != x_atom)
	{
		X_SendError(cl, BadAtom, req->id, X_GetAtomName, 0);
		return;
	}

	rep->type			= X_Reply;
	rep->pad1			= 0;
	rep->sequenceNumber	= cl->requestnum;
	rep->length			= (strlen(xa->atomname)+3)/4;
	rep->nameLength		= strlen(xa->atomname);
	rep->pad2			= 0;
	rep->pad3			= 0;
	rep->pad4			= 0;
	rep->pad5			= 0;
	rep->pad6			= 0;
	rep->pad7			= 0;
	strcpy((char *)(rep+1), xa->atomname);

	X_SendData(cl, rep, sizeof(*rep)+rep->length*4);
}

void XR_InternAtom (xclient_t *cl, xReq *request)
{
	xInternAtomReq *req = (xInternAtomReq*)request;
	xInternAtomReply rep;
	char atomname[1024];
	Atom atom;

	if (req->nbytes >= sizeof(atomname))
	{	//exceeded that limit then...
		X_SendError(cl, BadImplementation, 0, X_InternAtom, 0);
		return;
	}

	strncpy(atomname, (char *)(req+1), req->nbytes);
	atomname[req->nbytes] = '\0';

	atom = XS_FindAtom(atomname);
	if (atom == None && !req->onlyIfExists)
	{
		atom = XS_NewResource();
		XS_CreateAtom(atom, atomname, NULL);	//global atom...
	}

	rep.type	= X_Reply;
	rep.pad1	= 0;
	rep.sequenceNumber	= cl->requestnum;
	rep.length	= 0;
	rep.atom	= atom;
	rep.pad2	= 0;
	rep.pad3	= 0;
	rep.pad4	= 0;
	rep.pad5	= 0;
	rep.pad6	= 0;

	X_SendData(cl, &rep, sizeof(rep));
}

void XR_GetProperty (xclient_t *cl, xReq *request)
{
	xGetPropertyReq *req = (xGetPropertyReq*)request;
	char buffer[8192];
	xwindow_t *wnd;
	int datalen;
	int format;
	int trailing;
	xGetPropertyReply *rep = (xGetPropertyReply*)buffer;

	if (XS_GetResource(req->window, (void**)&wnd) != x_window)
	{	//wait a minute, That's not a window!!!
		X_SendError(cl, BadWindow, req->window, X_GetProperty, 0);
		return;
	}
	if (XS_GetResource(req->property, (void**)NULL) != x_atom)
	{	//whoops
		X_SendError(cl, BadAtom, req->property, X_GetProperty, 0);
		return;
	}

	if (req->longLength > sizeof(buffer) - sizeof(req)/4)
		req->longLength = sizeof(buffer) - sizeof(req)/4;
	datalen = XS_GetProperty(wnd, req->property, &rep->propertyType, (char *)(rep+1), req->longLength*4, req->longOffset*4, &trailing, &format);

	rep->type			= X_Reply;
    rep->format			= format;
    rep->sequenceNumber	= cl->requestnum;
    rep->length			= (datalen+3)/4;
	//rep->propertyType	= None;
    rep->bytesAfter		= trailing;
	if (format)
		rep->nItems			= datalen / (format/8);
	else
		rep->nItems		= 0;
    rep->pad1			= 0;
    rep->pad2			= 0;
    rep->pad3			= 0;

	X_SendData(cl, rep, rep->length*4 + sizeof(*rep));

	if (req->delete)
	{
		xEvent ev;

		XS_DeleteProperty(wnd, req->property);

		ev.u.u.type						= PropertyNotify;
		ev.u.u.detail					= 0;
		ev.u.u.sequenceNumber			= 0;
		ev.u.property.window			= req->window;
		ev.u.property.atom				= req->property;
		ev.u.property.time				= pSys_Milliseconds();
		ev.u.property.state				= PropertyDelete;

		ev.u.property.pad1				= 0;
		ev.u.property.pad2				= 0;

		X_SendNotificationMasked(&ev, wnd, PropertyChangeMask);
	}
}

void XR_ListProperties(xclient_t *cl, xReq *request)
{
	xproperty_t *xp;
	xResourceReq *req = (xResourceReq*)request;
	char buffer[65536];
	xwindow_t *wnd;
	xListPropertiesReply *rep = (xListPropertiesReply*)buffer;
	Atom *out = (Atom *)(rep+1);

	if (XS_GetResource(req->id, (void**)&wnd) != x_window)
	{	//wait a minute, That's not a window!!!
		X_SendError(cl, BadWindow, req->id, X_GetProperty, 0);
		return;
	}


	rep->type			= X_Reply;
    rep->sequenceNumber	= cl->requestnum;
    rep->length			= 0;
	rep->nProperties	= 0;
    rep->pad1			= 0;
    rep->pad2			= 0;
    rep->pad3			= 0;
	rep->pad4			= 0;
	rep->pad5			= 0;
	rep->pad6			= 0;
	rep->pad7			= 0;

	for (xp = wnd->properties; xp; xp = xp->next)
	{
		rep->nProperties++;
		*out = xp->atomid;
	}

	rep->length = rep->nProperties;

	X_SendData(cl, rep, rep->length*4 + sizeof(*rep));
}

void XR_ChangeProperty (xclient_t *cl, xReq *request)
{
	xChangePropertyReq *req = (xChangePropertyReq*)request;
	int len;

	xatom_t *atom;
	xwindow_t *wnd;

	if (XS_GetResource(req->window, (void**)&wnd) != x_window)
	{	//wait a minute, That's not a window!!!
		X_SendError(cl, BadWindow, req->window, X_ChangeProperty, 0);
		return;
	}

	if (XS_GetResource(req->property, (void**)&atom) != x_atom)
	{
		X_SendError(cl, BadAtom, req->property, X_ChangeProperty, 0);
		return;
	}

	len = req->nUnits * (req->format/8);

	if (req->mode == PropModeReplace)
		XS_SetProperty(wnd, req->property, req->type, (char *)(req+1), len, req->format);
	else if (req->mode == PropModePrepend)
	{
		X_SendError(cl, BadImplementation, req->window, X_ChangeProperty, 0);
		return;
	}
	else if (req->mode == PropModeAppend)
	{
		char hugebuffer[65536];
		int trailing;
		int format, datalen;
		Atom proptype;


		datalen = XS_GetProperty(wnd, req->property, &proptype, hugebuffer, sizeof(hugebuffer), 0, &trailing, &format);
		if (datalen+len > sizeof(hugebuffer))
		{
			X_SendError(cl, BadImplementation, req->window, X_ChangeProperty, 0);
			return;
		}
		memcpy(hugebuffer + datalen, (char *)(req+1), len);

		XS_SetProperty(wnd, req->property, proptype, hugebuffer, datalen+len, format);
	}

	{
		xEvent ev;

		ev.u.u.type						= PropertyNotify;
		ev.u.u.detail					= 0;
		ev.u.u.sequenceNumber			= 0;
		ev.u.property.window			= req->window;
		ev.u.property.atom				= req->property;
		ev.u.property.time				= pSys_Milliseconds();
		ev.u.property.state				= PropertyNewValue;

		ev.u.property.pad1				= 0;
		ev.u.property.pad2				= 0;

		X_SendNotificationMasked(&ev, wnd, PropertyChangeMask);
	}
}

void XR_DeleteProperty(xclient_t *cl, xReq *request)
{
	xDeletePropertyReq *req = (xDeletePropertyReq*)request;

	xwindow_t *wnd;

	if (XS_GetResource(req->window, (void**)&wnd) != x_window)
	{	//wait a minute, That's not a window!!!
		X_SendError(cl, BadWindow, req->window, X_DeleteProperty, 0);
		return;
	}

	if (XS_GetResource(req->property, (void**)NULL) != x_atom)
	{
		X_SendError(cl, BadAtom, req->property, X_DeleteProperty, 0);
		return;
	}

	XS_DeleteProperty(wnd, req->property);

	{
		xEvent ev;

		ev.u.u.type						= PropertyNotify;
		ev.u.u.detail					= 0;
		ev.u.u.sequenceNumber			= 0;
		ev.u.property.window			= req->window;
		ev.u.property.atom				= req->property;
		ev.u.property.time				= pSys_Milliseconds();
		ev.u.property.state				= PropertyDelete;

		ev.u.property.pad1				= 0;
		ev.u.property.pad2				= 0;

		X_SendNotificationMasked(&ev, wnd, PropertyChangeMask);
	}
}

void XR_GetSelectionOwner (xclient_t *cl, xReq *request)
{
	xResourceReq *req = (xResourceReq *)request;
	xGetSelectionOwnerReply reply;
	xatom_t *atom;

	if (XS_GetResource(req->id, (void**)&atom) != x_atom)
	{
		X_SendError(cl, BadAtom, req->id, X_GetSelectionOwner, 0);
		return;
	}

	if (XS_GetResource(atom->selectionownerwindowid, (void**)NULL) != x_window)	//make sure the window still exists.
	{
		atom->selectionownerwindowid = None;
	}

	reply.type				= X_Reply;
    reply.sequenceNumber	= cl->requestnum;
    reply.length			= 0;
	reply.owner				= atom->selectionownerwindowid;
    reply.pad1				= 0;
    reply.pad2				= 0;
    reply.pad3				= 0;
	reply.pad4				= 0;
	reply.pad5				= 0;
	reply.pad6				= 0;

	X_SendData(cl, &reply, sizeof(reply));
}
void XR_SetSelectionOwner (xclient_t *cl, xReq *request)
{
	xSetSelectionOwnerReq *req = (xSetSelectionOwnerReq *)request;
	xatom_t *atom;
	xwindow_t *window;

	if (XS_GetResource(req->selection, (void**)&atom) != x_atom)
	{
		X_SendError(cl, BadAtom, req->selection, X_SetSelectionOwner, 0);
		return;
	}

	if (XS_GetResource(req->window, (void**)&window) != x_window)	//make sure the window still exists.
	{
		X_SendError(cl, BadWindow, req->window, X_SetSelectionOwner, 0);
		return;
	}

	if (req->window)
	{
		atom->selectionownerwindowid = req->window;
		atom->selectionownerclient = cl;
	}
	else
	{
		atom->selectionownerwindowid = None;
		atom->selectionownerclient = NULL;
	}
}




extern int x_windowwithcursor;

void XR_GetInputFocus (xclient_t *cl, xReq *request)
{
	xGetInputFocusReply rep;
	extern xwindow_t *xfocusedwindow;

    rep.type			= X_Reply;
    rep.revertTo		= None;
    rep.sequenceNumber	= cl->requestnum;
    rep.length			= 0;
    rep.focus			= xfocusedwindow?xfocusedwindow->res.id:None;
    rep.pad1			= 0;
    rep.pad2 			= 0;
    rep.pad3			= 0;
    rep.pad4			= 0;
    rep.pad5			= 0;


	X_SendData(cl, &rep, sizeof(rep));
}

void XR_SetInputFocus (xclient_t *cl, xReq *request)
{
	extern xwindow_t *xfocusedwindow;
	xResourceReq	*req = (xResourceReq *)request;
	xwindow_t *wnd;

	if (XS_GetResource(req->id, (void**)&wnd) != x_window)
	{
		X_SendError(cl, BadDrawable, req->id, X_SetInputFocus, 0);
		return;
	}
	
	xfocusedwindow = wnd;

	X_EvalutateFocus(NotifyWhileGrabbed);
}

void XR_QueryBestSize (xclient_t *cl, xReq *request)
{
	xQueryBestSizeReq	*req = (xQueryBestSizeReq	*)request;
	xQueryBestSizeReply rep;

	if (req->class == CursorShape && req->drawable != rootwindow->res.id)
	{
		X_SendError(cl, BadDrawable, req->drawable, X_QueryBestSize, req->class);
		return;
	}
	else if (req->class != CursorShape)
	{
		X_SendError(cl, BadImplementation, req->drawable, X_QueryBestSize, req->class);
		return;
	}

	rep.type			= X_Reply;
	rep.pad1			= 0;
	rep.sequenceNumber	= cl->requestnum;
	rep.length			= 0;
	rep.width			= req->width;
	rep.height			= req->height;
	rep.pad3			= 0;
	rep.pad4			= 0;
	rep.pad5			= 0;
	rep.pad6			= 0;
	rep.pad7			= 0;

	X_SendData(cl, &rep, sizeof(rep));
}

void XR_GetGeometry (xclient_t *cl, xReq *request)
{ 
	xResourceReq *req = (xResourceReq *)request;
	xGetGeometryReply	rep;
	xresource_t *drawable;

	xwindow_t *wnd;
	xpixmap_t *pm;

	rep.type			= X_Reply;
	rep.depth			= 24;
	rep.sequenceNumber	= cl->requestnum;
	rep.length			= 0;
	rep.root			= 0;
	rep.x				= 0;
	rep.y				= 0;
	rep.width			= 0;
	rep.height			= 0;
	rep.borderWidth		= 0;
	rep.pad1			= 0;
	rep.pad2			= 0;
	rep.pad3			= 0;

	switch(XS_GetResource(req->id, (void**)&drawable))
	{
	case x_window:
		wnd = (xwindow_t*)drawable;
		rep.x = wnd->xpos;
		rep.y = wnd->ypos;
		rep.borderWidth = 0;	//fixme
		rep.width = wnd->width;
		rep.height = wnd->height;
		rep.root = rootwindow->res.id;
		break;
	case x_pixmap:
		pm = (xpixmap_t*)drawable;
		rep.width = pm->width;
		rep.height = pm->height;
		break;
	default:
		X_SendError(cl, BadDrawable, req->id, X_GetGeometry, 0);
		return;
	}


	X_SendData(cl, &rep, sizeof(rep));
}

void XR_CreateWindow (xclient_t *cl, xReq *request)
{
	xCreateWindowReq *req = (xCreateWindowReq *)request;
	xwindow_t *parent;
	xwindow_t *wnd;
	CARD32 *parameters;

	if (req->class == InputOnly && req->depth != 0)
	{
		X_SendError(cl, BadMatch, req->wid, X_CreateWindow, 0);
		return;
	}
	if (XS_GetResource(req->wid, (void**)&parent) != x_none)
	{
		X_SendError(cl, BadIDChoice, req->wid, X_CreateWindow, 0);
		return;
	}

	if (XS_GetResource(req->parent, (void**)&parent) != x_window)
	{
		X_SendError(cl, BadWindow, req->parent, X_CreateWindow, 0);
		return;
	}

	wnd = XS_CreateWindow(req->wid, cl, parent, req->x, req->y, req->width, req->height);

	if (req->depth != 0)
		wnd->depth = req->depth;
	else
		wnd->depth = parent->depth;

	if (req->class == CopyFromParent)
		wnd->inputonly = parent->inputonly;
	else
		wnd->inputonly = (req->class == InputOnly);

	//FIXME: Depth must be valid
	//FIXME: visual id must be valid.

	parameters = (CARD32 *)(req+1);
	if (req->mask & CWBackPixmap)
	{
		wnd->backpixmap = NULL;
		if (XS_GetResource(*parameters, (void**)&wnd->backpixmap) != x_pixmap)
		{
			if (*parameters)
				X_SendError(cl, BadPixmap, *parameters, X_CreateWindow, 0);
		}
		else
			wnd->backpixmap->references++;
		parameters++;
	}
	if (req->mask & CWBackPixel)//
	{
		wnd->backpixel = *parameters;
		parameters++;
	}
	if (req->mask & CWBorderPixmap)
		parameters+=0;
	if (req->mask & CWBorderPixel)//
	{
		wnd->borderpixel = *parameters;
		parameters++;
	}
	if (req->mask & CWBitGravity)//
	{
		wnd->bitgravity = *parameters;
		parameters++;
	}
	if (req->mask & CWWinGravity)
		wnd->bitgravity = *parameters++;
	if (req->mask & CWBackingStore)
		parameters++;	//ignored
	if (req->mask & CWBackingPlanes)
		parameters+=0;
	if (req->mask & CWBackingPixel)
		parameters+=0;
	if (req->mask & CWOverrideRedirect)
		wnd->overrideredirect = *parameters++;
	else
		wnd->overrideredirect = false;
	if (req->mask & CWSaveUnder)
		parameters++;
	if (req->mask & CWEventMask)//
	{
		xnotificationmask_t *nm;
		nm = malloc(sizeof(xnotificationmask_t));
		nm->client = cl;
		nm->next = NULL;
		nm->mask = *parameters;
		wnd->notificationmask = nm;
		parameters++;

		wnd->notificationmasks = 0;
		for (nm = wnd->notificationmask; nm; nm = nm->next)
			wnd->notificationmasks |= nm->mask;
	}
	if (req->mask & CWDontPropagate)
		wnd->donotpropagate = *parameters++;
	if (req->mask & CWColormap)//
	{
		wnd->colormap = *parameters;
		parameters++;
	}
	if (req->mask & CWCursor)
		parameters++;

	#define CWBackPixmap		(1L<<0)
#define CWBackPixel		(1L<<1)
#define CWBorderPixmap		(1L<<2)
#define CWBorderPixel           (1L<<3)
#define CWBitGravity		(1L<<4)
#define CWWinGravity		(1L<<5)
#define CWBackingStore          (1L<<6)
#define CWBackingPlanes	        (1L<<7)
#define CWBackingPixel	        (1L<<8)
#define CWOverrideRedirect	(1L<<9)
#define CWSaveUnder		(1L<<10)
#define CWEventMask		(1L<<11)
#define CWDontPropagate	        (1L<<12)
#define CWColormap		(1L<<13)
#define CWCursor	        (1L<<14)
/*
    CARD8 depth;
    Window wid;
	Window parent;
    INT16 x B16, y B16;
    CARD16 width B16, height B16, borderWidth B16;  
#if defined(__cplusplus) || defined(c_plusplus)
    CARD16 c_class B16;
#else
    CARD16 class B16;
#endif
    VisualID visual B32;
    CARD32 mask B32;
*/

	if (wnd->inputonly)
		return;


	{
		xEvent ev;

		ev.u.u.type						= CreateNotify;
		ev.u.u.detail					= 0;
		ev.u.u.sequenceNumber			= 0;
		ev.u.createNotify.parent		= wnd->parent->res.id;
		ev.u.createNotify.window		= wnd->res.id;
		ev.u.createNotify.x				= wnd->xpos;
		ev.u.createNotify.y				= wnd->ypos;
		ev.u.createNotify.width			= wnd->width;
		ev.u.createNotify.height		= wnd->height; 
		ev.u.createNotify.borderWidth	= req->borderWidth;
		ev.u.createNotify.override		= wnd->overrideredirect;
		ev.u.createNotify.bpad			= 0;

		X_SendNotificationMasked (&ev, wnd, SubstructureNotifyMask);
	}

/*	{
		xEvent ev;

		ev.u.u.type						= MapRequest;
		ev.u.u.detail					= 0;
		ev.u.u.sequenceNumber			= 0;
		ev.u.mapRequest.window			= wnd->res.id;
		ev.u.mapRequest.parent			= wnd->parent->res.id;

		X_SendNotificationMasked(&ev, wnd, SubstructureRedirectMask);
	}*/
/*	{
		xEvent ev;

		ev.u.u.type						= GraphicsExpose;
		ev.u.u.detail					= 0;
		ev.u.u.sequenceNumber			= 0;
		ev.u.expose.window				= wnd->res.id;
		ev.u.expose.x					= 0;
		ev.u.expose.y					= 0;
		ev.u.expose.width				= wnd->width;
		ev.u.expose.height				= wnd->height;
		ev.u.expose.count				= 0;	//matching expose events after this one
		ev.u.expose.pad2				= 0;

		X_SendNotificationMasked(&ev, wnd, ExposureMask);
	}*/
}

void XR_ChangeWindowAttributes (xclient_t *cl, xReq *request)
{
	CARD32 *parameters;
	xChangeWindowAttributesReq *req = (xChangeWindowAttributesReq *)request;
	xwindow_t *wnd;

	if (XS_GetResource(req->window, (void**)&wnd) != x_window)
	{
		X_SendError(cl, BadWindow, req->window, X_ChangeWindowAttributes, 0);
		return;
	}

	parameters = (CARD32 *)(req+1);

	if (req->valueMask & CWBackPixmap)
	{
		if (wnd->backpixmap)
			wnd->backpixmap->references--;
		wnd->backpixmap = NULL;
		if (XS_GetResource(*parameters, (void**)&wnd->backpixmap) != x_pixmap)
		{
			if (*parameters)
				X_SendError(cl, BadPixmap, *parameters, X_ChangeWindowAttributes, 0);
		}
		else
			wnd->backpixmap->references++;
		parameters++;
	}

	if (req->valueMask & CWBackPixel)
	{
		if (wnd->backpixmap)
			wnd->backpixmap->references--;
		wnd->backpixmap = NULL;
		wnd->backpixel = *parameters++;
	}

	if (req->valueMask & CWBorderPixmap)
	{
		X_SendError(cl, BadImplementation, 0, X_ChangeWindowAttributes, 0);
/*		wnd->borderpixmap = NULL;
		if (XS_GetResource(*parameters, (void**)&wnd->borderpixmap) != x_pixmap)
		{
			if (*parameters)
				X_SendError(cl, BadPixmap, *parameters, X_ChangeWindowAttributes, 0);
		}
		else
			wnd->backpixmap->references++;
*/		parameters++;
	}

	if (req->valueMask & CWBorderPixel)
		wnd->borderpixel = *parameters++;
	
	if (req->valueMask & CWBitGravity)
	{
		wnd->bitgravity = *parameters++;
	}

	if (req->valueMask & CWWinGravity)
	{
		wnd->wingravity = *parameters++;
	}

	if (req->valueMask & CWBackingStore)
	{
//		X_SendError(cl, BadImplementation, 0, X_ChangeWindowAttributes, 0);
		parameters++;	//ignore
	}

	if (req->valueMask & CWBackingPlanes)
	{
		X_SendError(cl, BadImplementation, 0, X_ChangeWindowAttributes, 0);
		parameters++;
	}

	if (req->valueMask & CWBackingPixel)
	{
		X_SendError(cl, BadImplementation, 0, X_ChangeWindowAttributes, 0);
		parameters++;
	}

	if (req->valueMask & CWOverrideRedirect)
	{
		wnd->overrideredirect = *parameters++;
	}

	if (req->valueMask & CWSaveUnder)
	{
//		X_SendError(cl, BadImplementation, 0, X_ChangeWindowAttributes, 0);
		parameters++;
	}

	if (req->valueMask & CWEventMask)
	{
		xnotificationmask_t *nm;

		if (*parameters & (SubstructureRedirectMask | ResizeRedirectMask))
		{	//you're only allowed one client with that one at a time.
			for (nm = wnd->notificationmask; nm; nm = nm->next)
			{
				if (nm->mask & (*parameters))
					if (nm->client != cl)
						break;
			}
		}
		else
			nm = NULL;
		if (nm)	//client has this one.
			X_SendError(cl, BadAccess, *parameters, X_ChangeWindowAttributes, CWEventMask);
		else
		{
			for (nm = wnd->notificationmask; nm; nm = nm->next)
			{
				if (nm->client == cl)
					break;
			}
			if (!nm)
			{
				nm = malloc(sizeof(xnotificationmask_t));
				nm->next = wnd->notificationmask;
				wnd->notificationmask = nm;
				nm->client = cl;
			}
			nm->mask = *parameters;

			wnd->notificationmasks = 0;
			for (nm = wnd->notificationmask; nm; nm = nm->next)
				wnd->notificationmasks |= nm->mask;
		}
		parameters++;
	}

	if (req->valueMask & CWDontPropagate)
	{
		wnd->donotpropagate = *parameters++;
	}

	if (req->valueMask & CWColormap)
	{
		X_SendError(cl, BadImplementation, 0, X_ChangeWindowAttributes, 0);
		parameters++;
	}

	if (req->valueMask & CWCursor)
	{
//		X_SendError(cl, BadImplementation, 0, X_ChangeWindowAttributes, 0);
		parameters++;
	}

	xrefreshed=true;

	if (req->valueMask > CWCursor)	//anything else is an error on some implementation's part.
		X_SendError(cl, BadImplementation, 0, X_ChangeWindowAttributes, 0);

//	XW_ClearArea(wnd, 0, 0, wnd->width, wnd->height);
}

void XR_ConfigureWindow (xclient_t *cl, xReq *request)
{
	int newx, newy, neww, newh, sibid, newbw;
	xConfigureWindowReq *req = (xConfigureWindowReq *)request;
	xwindow_t *wnd;

	CARD32 *parm;

	if (XS_GetResource(req->window, (void**)&wnd) != x_window)
	{
		X_SendError(cl, BadWindow, req->window, X_ConfigureWindow, 0);
		return;
	}

	if (!wnd->parent)	//root window.
	{	//can't resize this one.
		X_SendError(cl, BadWindow, req->window, X_ConfigureWindow, 0);
		return;
	}

	parm = (CARD32 *)(req+1);

    if (req->mask & CWX)
		newx = *parm++;
	else
		newx=wnd->xpos;
	if (req->mask & CWY)
		newy = *parm++;
	else
		newy=wnd->ypos;

	if (req->mask & CWWidth)
		neww = *parm++;
	else
		neww=wnd->width;

	if (wnd->width <= 0)
		wnd->width = 1;

	if (req->mask & CWHeight)
		newh = *parm++;
	else
		newh=wnd->height;

	if (req->mask & CWBorderWidth)
		newbw = *parm++;
	else
		newbw = 0;
	if (req->mask & CWSibling)
		sibid = *parm++;
	else
		sibid = 0;
	if (req->mask & CWStackMode)
		*parm++;

	if (!wnd->overrideredirect && X_NotifcationMaskPresent(wnd, SubstructureRedirectMask, cl))
	{
		xEvent ev;



		ev.u.u.type							= ConfigureRequest;
		ev.u.u.detail						= 0;
		ev.u.u.sequenceNumber				= 0;
		ev.u.configureRequest.parent		= wnd->parent->res.id;
		ev.u.configureRequest.window		= wnd->res.id;
		ev.u.configureRequest.sibling		= wnd->sibling?wnd->sibling->res.id:None;
		ev.u.configureRequest.x				= newx;
		ev.u.configureRequest.y				= newy;
		ev.u.configureRequest.width			= neww;
		ev.u.configureRequest.height		= newh;
		ev.u.configureRequest.borderWidth	= newbw;
		ev.u.configureRequest.valueMask		= req->mask;
		ev.u.configureRequest.pad1			= 0;

		X_SendNotificationMasked(&ev, wnd, SubstructureRedirectMask);
	}
	else
	{
		xEvent ev;

	/*	if (wnd->xpos == newx && wnd->ypos == newy)
		{
			ev.u.u.type							= ResizeRequest;
			ev.u.u.detail						= 0;
			ev.u.u.sequenceNumber				= 0;
			ev.u.resizeRequest.window			= wnd->res.id;
			ev.u.resizeRequest.width			= wnd->width;
			ev.u.resizeRequest.height			= wnd->height;

			X_SendNotificationMasked(&ev, wnd, StructureNotifyMask);
			X_SendNotificationMasked(&ev, wnd, SubstructureNotifyMask);

			return;
		}*/

		wnd->xpos = newx;
		wnd->ypos = newy;

		if ((wnd->width != neww || wnd->height != newh) && wnd->buffer)
		{
			free(wnd->buffer);
			wnd->buffer = NULL;
		}
		wnd->width = neww;
		wnd->height = newh;

		if (wnd->mapped)
			xrefreshed = true;

		ev.u.u.type							= ConfigureNotify;
		ev.u.u.detail						= 0;
		ev.u.u.sequenceNumber				= 0;
		ev.u.configureNotify.event			= wnd->res.id;
		ev.u.configureNotify.window			= wnd->res.id;
		ev.u.configureNotify.aboveSibling	= None;
		ev.u.configureNotify.x				= wnd->xpos;
		ev.u.configureNotify.y				= wnd->ypos;
		ev.u.configureNotify.width			= wnd->width;
		ev.u.configureNotify.height			= wnd->height;
		ev.u.configureNotify.borderWidth	= 0;
		ev.u.configureNotify.override		= wnd->overrideredirect;
		ev.u.configureNotify.bpad			= 0;

		X_SendNotificationMasked(&ev, wnd, StructureNotifyMask);
		X_SendNotificationMasked(&ev, wnd, SubstructureNotifyMask);
	}
}

void XR_ReparentWindow (xclient_t *cl, xReq *request)
{
	qboolean wasmapped;
	xEvent ev;
	xReparentWindowReq *req = (xReparentWindowReq *)request;
	xwindow_t *wnd, *parent;

	if (XS_GetResource(req->window, (void**)&wnd) != x_window)
	{
		X_SendError(cl, BadWindow, req->window, X_ReparentWindow, 0);
		return;
	}
	if (XS_GetResource(req->parent, (void**)&parent) != x_window)
	{
		X_SendError(cl, BadWindow, req->parent, X_ReparentWindow, 0);
		return;
	}

	if (wnd->mapped)
	{
		XR_UnmapWindow(cl, request);
		wasmapped = true;
	}
	else
		wasmapped = false;

	ev.u.u.type						= ReparentNotify;
	ev.u.u.detail					= 0;
	ev.u.reparent.override = wnd->overrideredirect;
	ev.u.reparent.window = wnd->res.id;
	ev.u.reparent.parent = wnd->res.id;
	ev.u.reparent.x = req->x;
	ev.u.reparent.y = req->y;

	X_SendNotificationMasked (&ev, wnd, SubstructureNotifyMask);

	XS_SetParent(wnd, parent);
	wnd->xpos = req->x;
	wnd->ypos = req->y;

	X_SendNotificationMasked (&ev, wnd, SubstructureNotifyMask);	//and again, now that we have the new parent.

	ev.u.reparent.event = wnd->res.id;
	X_SendNotificationMasked (&ev, wnd, StructureNotifyMask);

	if (wasmapped)
		XR_MapWindow(cl, request);
}

void XR_DestroyWindow (xclient_t *cl, xReq *request)
{
	xResourceReq *req = (xResourceReq *)request;
	xwindow_t *wnd;

	if (XS_GetResource(req->id, (void**)&wnd) != x_window)
	{
		X_SendError(cl, BadWindow, req->id, X_DestroyWindow, 0);
		return;
	}
	if (!wnd->res.owner)	//root window.
		return;
	XS_DestroyResource(&wnd->res);
}
void XR_QueryTree (xclient_t *cl, xReq *request)
{
	char buffer[8192];
	xResourceReq *req = (xResourceReq *)request;
	xQueryTreeReply *rep = (xQueryTreeReply*)buffer;

	xwindow_t *wnd;

	Window	*cwnd;


	if (XS_GetResource(req->id, (void**)&wnd) != x_window)
	{
		X_SendError(cl, BadWindow, req->id, X_DestroyWindow, 0);
		return;
	}

	//FIXME: be careful of the count of children overflowing buffer.

	rep->type	= X_Reply;
	rep->pad1	= 0;
	rep->sequenceNumber	= cl->requestnum;
	rep->length	= 0;
	rep->root	= rootwindow->res.id;	//we only have one root
	if (wnd->parent)
		rep->parent	= wnd->parent->res.id;
	else
		rep->parent	= 0;
	rep->nChildren	= 0;
	rep->pad2	= 0;
	rep->pad3	= 0;
	rep->pad4	= 0;
	rep->pad5	= 0;

	cwnd = (Window*)(rep+1);

	for (wnd = wnd->child ; wnd ; wnd = wnd->sibling)
	{
		*cwnd++ = wnd->res.id;
		rep->nChildren++;
	}

	rep->length = rep->nChildren;

	X_SendData(cl, rep, sizeof(*rep)+rep->length*4);
}

void XR_GetWindowAttributes (xclient_t *cl, xReq *request)
{
	xnotificationmask_t *nm;
	xResourceReq *req = (xResourceReq *)request;
	xwindow_t *wnd;

	xGetWindowAttributesReply rep;

	if (XS_GetResource(req->id, (void**)&wnd) != x_window)
	{
		X_SendError(cl, BadWindow, req->id, X_GetWindowAttributes, 0);
		return;
	}

	rep.type				= X_Reply;
	rep.backingStore		= 2;
	rep.sequenceNumber		= cl->requestnum;
	rep.visualID			= 0x22;
	rep.class				= wnd->inputonly;
	rep.bitGravity			= wnd->bitgravity;
	rep.winGravity			= wnd->wingravity;
	rep.backingBitPlanes	= wnd->depth;
	rep.backingPixel		= wnd->backpixel;
	rep.saveUnder			= 1;
	rep.mapInstalled		= !!wnd->buffer;
	rep.mapState			= wnd->mapped*2;
	rep.override			= wnd->overrideredirect;
	rep.colormap			= wnd->colormap;
	rep.yourEventMask		= 0;
	rep.allEventMasks		= 0;
	for (nm = wnd->notificationmask; nm; nm = nm->next)
	{
		if (nm->client == cl)
			rep.yourEventMask = nm->mask;
		rep.allEventMasks |= nm->mask;
	}
	rep.doNotPropagateMask	= wnd->donotpropagate;
	rep.pad					= 0;

	rep.length = (sizeof(xGetWindowAttributesReply) - sizeof(xGenericReply) + 3)/4;

	X_SendData(cl, &rep, sizeof(xGetWindowAttributesReply));
}

void XR_GetKeyboardMapping (xclient_t *cl, xReq *request)
{//fixme: send the XK equivelents.
	xGetKeyboardMappingReq *req = (xGetKeyboardMappingReq *)request;
	char buffer[8192];
	xGetKeyboardMappingReply *rep = (xGetKeyboardMappingReply *)buffer;
	int i;

	rep->type				= X_Reply;
	rep->keySymsPerKeyCode	= 1;
	rep->sequenceNumber		= cl->requestnum;
	rep->length				= req->count;
	rep->pad2				= 0;
	rep->pad3				= 0;
	rep->pad4				= 0;
	rep->pad5				= 0;
	rep->pad6				= 0;
	rep->pad7				= 0;

	for (i = 0; i < req->count; i++)
	{
		switch (req->firstKeyCode+i)
		{
/*
		case ' ':			((int *)(rep+1))[i] = XK_space;				break;

		case K_PGUP:		((int *)(rep+1))[i] = XK_Page_Up;			break;
		case K_PGDN:		((int *)(rep+1))[i] = XK_Page_Down;			break;
		case K_HOME:		((int *)(rep+1))[i] = XK_Home;				break;
		case K_END:			((int *)(rep+1))[i] = XK_End;				break;

		case K_LEFTARROW:	((int *)(rep+1))[i] = XK_Left;				break;
		case K_RIGHTARROW:	((int *)(rep+1))[i] = XK_Right;				break;
		case K_DOWNARROW:	((int *)(rep+1))[i] = XK_Down;				break;
		case K_UPARROW:		((int *)(rep+1))[i] = XK_Up;				break;

		case K_ENTER:		((int *)(rep+1))[i] = XK_Return;			break;
		case K_TAB:			((int *)(rep+1))[i] = XK_Tab;				break;
		case K_ESCAPE:		((int *)(rep+1))[i] = XK_Escape;			break;

		case K_F1:			((int *)(rep+1))[i] = XK_F1;				break;
		case K_F2:			((int *)(rep+1))[i] = XK_F2;				break;
		case K_F3:			((int *)(rep+1))[i] = XK_F3;				break;
		case K_F4:			((int *)(rep+1))[i] = XK_F4;				break;
		case K_F5:			((int *)(rep+1))[i] = XK_F5;				break;
		case K_F6:			((int *)(rep+1))[i] = XK_F6;				break;
		case K_F7:			((int *)(rep+1))[i] = XK_F7;				break;
		case K_F8:			((int *)(rep+1))[i] = XK_F8;				break;
		case K_F9:			((int *)(rep+1))[i] = XK_F9;				break;
		case K_F10:			((int *)(rep+1))[i] = XK_F10;				break;
		case K_F11:			((int *)(rep+1))[i] = XK_F11;				break;
		case K_F12:			((int *)(rep+1))[i] = XK_F12;				break;

		case K_BACKSPACE:	((int *)(rep+1))[i] = XK_BackSpace;			break;
		case K_DEL:			((int *)(rep+1))[i] = XK_Delete;			break;
		case K_INS:			((int *)(rep+1))[i] = XK_Insert;			break;
		case K_PAUSE:		((int *)(rep+1))[i] = XK_Pause;				break;
		case K_SHIFT:		((int *)(rep+1))[i] = XK_Shift_L;			break;
		case K_CTRL:		((int *)(rep+1))[i] = XK_Control_L;			break;
		case K_ALT:			((int *)(rep+1))[i] = XK_Alt_L;				break;


		case K_KP_HOME:		((int *)(rep+1))[i] = XK_Home;				break;
		case K_KP_UPARROW:	((int *)(rep+1))[i] = XK_Up;				break;



		case K_KP_PGUP:			((int *)(rep+1))[i] = XK_KP_Page_Up;	break;
		case K_KP_LEFTARROW:	((int *)(rep+1))[i] = XK_KP_Left;		break;
		case K_KP_5:			((int *)(rep+1))[i] = XK_KP_Space;		break;
		case K_KP_RIGHTARROW:	((int *)(rep+1))[i] = XK_KP_Right;		break;
		case K_KP_END:			((int *)(rep+1))[i] = XK_KP_End;		break;
		case K_KP_DOWNARROW:	((int *)(rep+1))[i] = XK_KP_Down;		break;
		case K_KP_PGDN:			((int *)(rep+1))[i] = XK_KP_Page_Down;	break;
		case K_KP_ENTER:		((int *)(rep+1))[i] = XK_KP_Enter;		break;
		case K_KP_INS:			((int *)(rep+1))[i] = XK_KP_Insert;		break;
		case K_KP_DEL:			((int *)(rep+1))[i] = XK_KP_Delete;		break;
		case K_KP_SLASH:		((int *)(rep+1))[i] = XK_KP_Divide;		break;
		case K_KP_MINUS:		((int *)(rep+1))[i] = XK_KP_Subtract;	break;
		case K_KP_PLUS:			((int *)(rep+1))[i] = XK_KP_Add;		break;
		case K_KP_STAR:			((int *)(rep+1))[i] = XK_KP_Multiply;	break;
		case K_KP_EQUALS:		((int *)(rep+1))[i] = XK_KP_Enter;		break;
*/
		default:
			((int *)(rep+1))[i]		= req->firstKeyCode+i;
			break;
		}
	}

	X_SendData(cl, rep, sizeof(*rep)+rep->length*4);
}

void XR_QueryPointer (xclient_t *cl, xReq *request)
{
	extern int x_mousex, x_mousey, x_mousestate;
	xQueryPointerReply rep;

	rep.type			= X_Reply;
	rep.sameScreen		= 1;
	rep.sequenceNumber	= cl->requestnum;
	rep.length	= 0;
	rep.root	= rootwindow->res.id;
	rep.child	= rootwindow->res.id;
	rep.rootX	= x_mousex;
	rep.rootY	= x_mousey;
	rep.winX	= x_mousex;
	rep.winY	= x_mousey;
	rep.mask	= 0;
	rep.pad1	= 0;
	rep.pad		= 0;

	if ((x_mousestate) & Button1Mask)
		rep.mask |= Button1MotionMask;
	if ((x_mousestate) & Button2Mask)
		rep.mask |= Button2MotionMask;
	if ((x_mousestate) & Button3Mask)
		rep.mask |= Button3MotionMask;
	if ((x_mousestate) & Button4Mask)
		rep.mask |= Button4MotionMask;
	if ((x_mousestate) & Button5Mask)
		rep.mask |= Button5MotionMask;

	X_SendData(cl, &rep, sizeof(rep));
}

void XR_CreateCursor (xclient_t *cl, xReq *request)
{
	xCreateCursorReq *req = (xCreateCursorReq *)request;

	//	X_SendError(cl, BadImplementation, 0, req->reqType, 0);
}
void XR_CreateGlyphCursor (xclient_t *cl, xReq *request)
{
	xCreateGlyphCursorReq *req = (xCreateGlyphCursorReq *)request;
//	char buffer[8192];
//	xGetKeyboardMappingReply *rep = (xGetKeyboardMappingReply *)buffer;

//	X_SendError(cl, BadImplementation, 0, req->reqType, 0);

//	X_SendError(cl, BadAlloc, req->id, X_DestroyWindow, 0);
//	X_SendError(cl, BadFont, req->id, X_DestroyWindow, 0);
//	X_SendError(cl, BadValue, req->id, X_DestroyWindow, 0);
}
void XR_FreeCursor (xclient_t *cl, xReq *request)
{
//	X_SendError(cl, BadImplementation, 0, req->reqType, 0);
//	X_SendError(cl, BadValue, req->id, X_DestroyWindow, 0);
}

void XR_ChangeGCInternal(unsigned int mask, xgcontext_t *gc, CARD32 *param)
{
	if (mask & GCFunction)
		gc->function = *param++;
	if (mask & GCPlaneMask)
		param++;
	if (mask & GCForeground)
		gc->fgcolour = *param++;
	if (mask & GCBackground)
		gc->bgcolour = *param++;
	if (mask & GCLineWidth)
		param++;
	if (mask & GCLineStyle)
		param++;
	if (mask & GCCapStyle)
		param++;
	if (mask & GCJoinStyle)
		param++;
	if (mask & GCFillStyle)
		param++;
	if (mask & GCFillRule)
		param++;
	if (mask & GCTile)
		param++;
	if (mask & GCStipple)
		param++;
	if (mask & GCTileStipXOrigin)
		param++;
	if (mask & GCTileStipYOrigin)
		param++;
	if (mask & GCFont)
	{
		if (XS_GetResource(*param++, &gc->font) != x_font)
			gc->font = NULL;
	}
	if (mask & GCSubwindowMode)
		param++;
	if (mask & GCGraphicsExposures)
		param++;
	if (mask & GCClipXOrigin)
		param++;
	if (mask & GCClipYOrigin)
		param++;
	if (mask & GCClipMask)
		param++;
	if (mask & GCDashOffset)
		param++;
	if (mask & GCDashList)
		param++;
	if (mask & GCArcMode)
		param++;
}
void XR_CopyGCInternal(unsigned int mask, xgcontext_t *dest, xgcontext_t *src)
{
	int param=0;
	if (mask & GCFunction)
		dest->function = src->function;
	if (mask & GCPlaneMask)
		param++;
	if (mask & GCForeground)
		dest->fgcolour = src->fgcolour;
	if (mask & GCBackground)
		dest->bgcolour = src->fgcolour;
	if (mask & GCLineWidth)
		param++;
	if (mask & GCLineStyle)
		param++;
	if (mask & GCCapStyle)
		param++;
	if (mask & GCJoinStyle)
		param++;
	if (mask & GCFillStyle)
		param++;
	if (mask & GCFillRule)
		param++;
	if (mask & GCTile)
		param++;
	if (mask & GCStipple)
		param++;
	if (mask & GCTileStipXOrigin)
		param++;
	if (mask & GCTileStipYOrigin)
		param++;
	if (mask & GCFont)
	{
		dest->font = src->font;
	}
	if (mask & GCSubwindowMode)
		param++;
	if (mask & GCGraphicsExposures)
		param++;
	if (mask & GCClipXOrigin)
		param++;
	if (mask & GCClipYOrigin)
		param++;
	if (mask & GCClipMask)
		param++;
	if (mask & GCDashOffset)
		param++;
	if (mask & GCDashList)
		param++;
	if (mask & GCArcMode)
		param++;
}
void XR_ChangeGC(xclient_t *cl, xReq *request)
{
	xChangeGCReq *req = (xChangeGCReq *)request;
	xgcontext_t *gc;

	if (XS_GetResource(req->gc, (void**)&gc) != x_gcontext)
	{
		X_SendError(cl, BadGC, req->gc, X_FreeGC, 0);
		return;
	}

	XR_ChangeGCInternal(req->mask, gc, (CARD32	*)(req + 1));
}

void XR_CopyGC(xclient_t *cl, xReq *request)
{
	xCopyGCReq *req = (xCopyGCReq *)request;
	xgcontext_t *dest, *src;

	if (XS_GetResource(req->dstGC, (void**)&dest) != x_gcontext)
	{
		X_SendError(cl, BadGC, req->dstGC, X_FreeGC, 0);
		return;
	}
	if (XS_GetResource(req->srcGC, (void**)&src) != x_gcontext)
	{
		X_SendError(cl, BadGC, req->srcGC, X_FreeGC, 0);
		return;
	}

	XR_CopyGCInternal(req->mask, dest, src);
}

void XR_CreateGC(xclient_t *cl, xReq *request)
{
	xCreateGCReq *req = (xCreateGCReq *)request;
	xresource_t *drawable;

	if (XS_GetResource(req->gc, (void**)&drawable) != x_none)
	{
//		if (req->gc == cl->ridbase&&drawable->owner)
//			XS_DestroyResourcesOfClient(drawable->owner);
//		else
		{
			X_SendError(cl, BadIDChoice, req->gc, X_CreateGC, 0);
			return;
		}
	}
	XS_GetResource(req->drawable, (void**)&drawable);
	/*if (drawable->restype != x_window && drawable->restype != x_gcontext)
	{
		X_SendError(cl, BadDrawable, req->drawable, X_CreateGC, 0);
		return;
	}*/

	XR_ChangeGCInternal(req->mask, XS_CreateGContext(req->gc, cl, drawable), (CARD32	*)(req + 1));
}

void XR_FreeGC(xclient_t *cl, xReq *request)
{
	xResourceReq *req = (xResourceReq *)request;
	xresource_t *gc;
	if (XS_GetResource(req->id, (void**)&gc) != x_gcontext)
	{
		X_SendError(cl, BadGC, req->id, X_FreeGC, 0);
		return;
	}

	XS_DestroyResource(gc);
}

void XW_ClearArea(xwindow_t *wnd, int xp, int yp, int width, int height)
{
	if (!wnd->buffer)
	{
		if (wnd->width*wnd->height<=0)
			wnd->buffer = malloc(1);
		else
			wnd->buffer = malloc(wnd->width*wnd->height*4);
	}

	if (xp < 0)
	{
		width += xp;
		xp = 0;
	}
	if (xp>wnd->width)
		xp = wnd->width;
	if (yp < 0)
	{
		height += yp;
		xp = 0;
	}
	if (yp>wnd->height)
		yp = wnd->height;
	if (width+xp > wnd->width)
		width = wnd->width - xp;
	if (height+yp > wnd->height)
		height = wnd->height - yp;

	if (wnd->backpixmap && wnd->backpixmap->width && wnd->backpixmap->height)
	{
		int x, xs;
		int y, ys;
		unsigned int *out;
		unsigned int *in;

		out = (unsigned int *)wnd->buffer + xp +yp*wnd->width;
		in = (unsigned int *)wnd->backpixmap->data;

		for (y = 0, ys = 0; y < height; y++)
		{
			for (x = 0; x < width; x+=wnd->backpixmap->width)
			{
				//when do we stop?
				xs = wnd->backpixmap->width;
				if (xs > wnd->width-x-1)
					xs = wnd->width-x-1;
				for (; xs > 0; xs--)
				{
					out[x+xs] = in[xs+ys*wnd->backpixmap->width];
				}
			}
			out += wnd->width;
			ys++;
			if (ys >= wnd->backpixmap->height)
				ys = 0;
		}
	}
	else
	{
		int x;
		int y;
		unsigned int *out;

		out = (unsigned int *)wnd->buffer + xp +yp*wnd->width;

		for (y = yp; y < height; y++)
		{
			for (x = xp; x < width; x++)
			{
				out[x] = wnd->backpixel;
			}
			out+=wnd->width;
		}
	}
}

void XW_CopyArea(unsigned int *dest, int dx, int dy, int dwidth, int dheight, unsigned int *source, int sx, int sy, int swidth, int sheight, int cwidth, int cheight, xgcontext_t *gc)
{
	int x, y;

	//tlcap on dest
	if (dx < 0)
	{
		cwidth += dx;
		dx = 0;
	}
	if (dy < 0)
	{
		cheight += dy;
		dy = 0;
	}

	//tlcap on source
	if (sx < 0)
	{
		cwidth += sx;
		sx = 0;
	}
	if (sy < 0)
	{
		cheight += sy;
		sy = 0;
	}

	//brcap on dest
	if (cwidth > dwidth - dx)
		cwidth = dwidth - dx;

	if (cheight > dheight - dy)
		cheight = dheight - dy;

	//brcap on source
	if (cwidth > swidth - sx)
		cwidth = swidth - sx;

	if (cheight > sheight - sy)
		cheight = sheight - sy;

	if (cwidth<=0)
		return;
	if (cheight<=0)
		return;

	dest += dx+dy*dwidth;
	source += sx+sy*swidth;

	for (y = 0; y < cheight; y++)
	{
		for (x = 0; x < cwidth;x++)
		{
			GCFunc(gc->fgcolour, dest[x], gc->function, source[x], 0xffffff);
		}
		dest += dwidth;
		source += swidth;
	}
}

void XR_ClearArea(xclient_t *cl, xReq *request)
{//FIXME: Should be area rather than entire window
	xClearAreaReq *req = (xClearAreaReq *)request;
	xwindow_t *wnd;

	if (XS_GetResource(req->window, (void**)&wnd) != x_window)
	{
		X_SendError(cl, BadWindow, req->window, X_ClearArea, 0);
		return;
	}

	if (req->x < 0)
	{
		if (req->width)
			req->width += req->x;
		req->x = 0;
	}
	if (req->y < 0)
	{
		if (req->height)
			req->height += req->y;
		req->y = 0;
	}

	if (!req->width || req->width + req->x > wnd->width)
	{
		if (req->width)
			req->width = wnd->width - req->x;
		req->width = wnd->width - req->x;
	}
	if (!req->height || req->height + req->y > wnd->height)
	{
		if (req->height)
			req->height = wnd->height - req->y;
		req->height = wnd->height - req->y;
	}

	XW_ClearArea(wnd, req->x, req->y, req->width, req->height);

	if (req->exposures)
		XW_ExposeWindowRegionInternal(wnd, req->x, req->y, req->width, req->height);

	xrefreshed=true;
}

void XR_CopyArea(xclient_t *cl, xReq *request)	//from and to pixmap or drawable.
{
	xCopyAreaReq *req = (xCopyAreaReq *)request;

	xresource_t *drawable;
	xgcontext_t *gc;


	unsigned int *outbuffer;
	unsigned int *inbuffer;
	int inwidth;
	int inheight;
	int outwidth;
	int outheight;

	if (XS_GetResource(req->gc, (void**)&gc) == x_none)
	{
		X_SendError(cl, BadGC, req->gc, X_CopyArea, 0);
		return;
	}


	switch (XS_GetResource(req->srcDrawable, (void**)&drawable))
	{
	default:
		X_SendError(cl, BadDrawable, req->srcDrawable, X_CopyArea, 0);
		return;

	case x_window:
		{
			xwindow_t *wnd;
			wnd = (xwindow_t *)drawable;
			if (!wnd->buffer)
			{
				wnd->buffer = malloc(wnd->width*wnd->height*4);
				XW_ClearArea(wnd, 0, 0, wnd->width, wnd->height);
			}

			inwidth = wnd->width;
			inheight = wnd->height;
			inbuffer = (unsigned int *)wnd->buffer;
		}
		break;

	case x_pixmap:
		{
			xpixmap_t *pm;
			pm = (xpixmap_t *)drawable;
			if (!pm->data)
			{
				pm->data = malloc(pm->width*pm->height*4);
				memset(pm->data, rand(), pm->width*pm->height*4);
			}

			inwidth = pm->width;
			inheight = pm->height;
			inbuffer = (unsigned int *)pm->data;
		}
		break;
	}

	switch (XS_GetResource(req->dstDrawable, (void**)&drawable))
	{
	default:
		X_SendError(cl, BadDrawable, req->dstDrawable, X_CopyArea, 0);
		return;

	case x_window:
		{
			xwindow_t *wnd;
			wnd = (xwindow_t *)drawable;
			if (!wnd->buffer)
			{
				wnd->buffer = malloc(wnd->width*wnd->height*4);
				XW_ClearArea(wnd, 0, 0, wnd->width, wnd->height);
			}

			outwidth = wnd->width;
			outheight = wnd->height;
			outbuffer = (unsigned int *)wnd->buffer;
		}
		break;

	case x_pixmap:
		{
			xpixmap_t *pm;
			pm = (xpixmap_t *)drawable;
			if (!pm->data)
			{
				pm->data = malloc(pm->width*pm->height*4);
				memset(pm->data, rand(), pm->width*pm->height*4);
			}

			outwidth = pm->width;
			outheight = pm->height;
			outbuffer = (unsigned int *)pm->data;
		}
		break;
	}

	XW_CopyArea(outbuffer, req->dstX, req->dstY, outwidth, outheight, inbuffer, req->srcX, req->srcY, inwidth, inheight, req->width, req->height, gc);

	xrefreshed=true;
}

void XR_MapWindow(xclient_t *cl, xReq *request)
{
	xResourceReq *req = (xResourceReq *)request;

	xwindow_t *wnd;
	if (XS_GetResource(req->id, (void**)&wnd) != x_window)
	{
		X_SendError(cl, BadWindow, req->id, X_MapWindow, 0);
		return;
	}

//	if (wnd->mapped)
//		return;

	if (!wnd->overrideredirect && X_NotifcationMaskPresent(wnd, SubstructureRedirectMask, cl))
	{
		xEvent ev;

		ev.u.u.type						= MapRequest;
		ev.u.u.detail					= 0;
		ev.u.u.sequenceNumber			= 0;
		ev.u.mapRequest.parent			= wnd->parent->res.id;
		ev.u.mapRequest.window			= wnd->res.id;

		X_SendNotificationMasked(&ev, wnd, SubstructureRedirectMask);

		return;
	}

	if (!wnd->buffer)
		XW_ClearArea(wnd, 0, 0, wnd->width, wnd->height);

	wnd->mapped = true;

	{
		xEvent ev;

		ev.u.u.type						= MapNotify;
		ev.u.u.detail					= 0;
		ev.u.u.sequenceNumber			= 0;
		ev.u.mapNotify.event			= wnd->res.id;
		ev.u.mapNotify.window			= wnd->res.id;
		ev.u.mapNotify.override			= wnd->overrideredirect;
		ev.u.mapNotify.pad1				= 0;
		ev.u.mapNotify.pad2				= 0;
		ev.u.mapNotify.pad3				= 0;

		X_SendNotificationMasked(&ev, wnd, StructureNotifyMask);
		X_SendNotificationMasked(&ev, wnd, SubstructureNotifyMask);
	}

/*	{
		xEvent ev;

		ev.u.u.type						= GraphicsExpose;
		ev.u.u.detail					= 0;
		ev.u.u.sequenceNumber			= 0;
		ev.u.mapNotify.window			= wnd->res.id;
		ev.u.mapNotify.override			= false;
		ev.u.mapNotify.pad1				= 0;
		ev.u.mapNotify.pad2				= 0;
		ev.u.mapNotify.pad3				= 0;

		X_SendNotificationMasked(&ev, wnd, ExposureMask);
	}*/

	XW_ExposeWindowRegionInternal(wnd, 0, 0, wnd->width, wnd->height);
	/*

	while(wnd->mapped)
	{
		xEvent ev;

		ev.u.u.type						= VisibilityNotify;
		ev.u.u.detail					= 0;
		ev.u.u.sequenceNumber			= 0;
		ev.u.visibility.window			= wnd->res.id;
		ev.u.visibility.state			= 0;
		ev.u.visibility.pad1			= 0;
		ev.u.visibility.pad2			= 0;
		ev.u.visibility.pad3			= 0;

		X_SendNotificationMasked(&ev, wnd, VisibilityChangeMask);
	}

	{
		xEvent ev;

		ev.u.u.type						= Expose;
		ev.u.u.detail					= 0;
		ev.u.u.sequenceNumber			= 0;
		ev.u.expose.window				= wnd->res.id;
		ev.u.expose.x					= 0;
		ev.u.expose.y					= 0;
		ev.u.expose.width				= wnd->width;
		ev.u.expose.height				= wnd->height;
		ev.u.expose.count				= false;	//other expose events following
		ev.u.expose.pad2				= 0;

		X_SendNotificationMasked(&ev, wnd, ExposureMask);
	}*/

	xrefreshed = true;
}

void XR_UnmapWindow(xclient_t *cl, xReq *request)
{
	xResourceReq *req = (xResourceReq *)request;

	xwindow_t *wnd;
	if (XS_GetResource(req->id, (void**)&wnd) != x_window)
	{
		X_SendError(cl, BadWindow, req->id, X_UnmapWindow, 0);
		return;
	}

	if (!wnd->mapped || !wnd->parent)
		return;

	wnd->mapped = false;
	xrefreshed=true;

	XW_ExposeWindow(wnd, 0, 0, wnd->width, wnd->height);

	{
		xEvent ev;

		ev.u.u.type						= UnmapNotify;
		ev.u.u.detail					= 0;
		ev.u.u.sequenceNumber			= 0;
		ev.u.unmapNotify.event			= wnd->res.id;
		ev.u.unmapNotify.window			= wnd->res.id;
		ev.u.unmapNotify.fromConfigure	= 0;
		ev.u.unmapNotify.pad1			= 0;
		ev.u.unmapNotify.pad2			= 0;
		ev.u.unmapNotify.pad3			= 0;

		X_SendNotificationMasked(&ev, wnd, StructureNotifyMask);
		X_SendNotificationMasked(&ev, wnd, SubstructureNotifyMask);
	}
}

void XR_MapSubwindows(xclient_t *cl, xReq *request)
{
	xResourceReq *req = (xResourceReq *)request;
	xwindow_t *wnd;

	if (XS_GetResource(req->id, (void**)&wnd) != x_window)
	{
		X_SendError(cl, BadWindow, req->id, X_MapWindow, 0);
		return;
	}

	for (wnd = wnd->child; wnd; wnd = wnd->sibling)
	{
		req->id = wnd->res.id;
		XR_MapWindow(cl, request);
	}
}

void XR_CreatePixmap(xclient_t *cl, xReq *request)
{
	xCreatePixmapReq *req = (xCreatePixmapReq *)request;

	xresource_t *res;
	xpixmap_t *newpix;
	
	switch(XS_GetResource(req->drawable, (void**)&res))
	{
	case x_window:
	case x_pixmap:
		break;
	default:
		X_SendError(cl, BadDrawable, req->drawable, X_CreatePixmap, 0);
		return;
	}

	//depth must be one of the depths supported by the drawable's root window
	if (req->depth != 24 && req->depth != 32)
	{
//		X_SendError(cl, BadValue, req->depth, X_CreatePixmap, 0);
//		return;
	}

	if (XS_GetResource(req->pid, (void**)&newpix) != x_none)
	{
		X_SendError(cl, BadIDChoice, req->pid, X_CreatePixmap, 0);
	}
	XS_CreatePixmap(req->pid, cl, req->width, req->height, req->depth);
}

void XR_FreePixmap(xclient_t *cl, xReq *request)
{
	xResourceReq *req = (xResourceReq *)request;
	xresource_t *pm;
	if (XS_GetResource(req->id, (void**)&pm) != x_pixmap)
	{
		X_SendError(cl, BadPixmap, req->id, X_FreePixmap, 0);
		return;
	}

	XS_DestroyResource(pm);
}

void XR_PutImage(xclient_t *cl, xReq *request)
{
	unsigned char *out;
	unsigned char *in;
	xPutImageReq *req = (xPutImageReq *)request;
	xresource_t *drawable;
	xgcontext_t *gc;
	int i;

	int drwidth;
	int drheight;
	int drdepth;
	unsigned char *drbuffer;

	if (XS_GetResource(req->drawable, (void**)&drawable) == x_none)
	{
		X_SendError(cl, BadDrawable, req->drawable, X_PutImage, 0);
		return;
	}

	if (XS_GetResource(req->gc, (void**)&gc) == x_none)
	{
		X_SendError(cl, BadGC, req->gc, X_PutImage, 0);
		return;
	}


	if (drawable->restype == x_window)
	{
		xwindow_t *wnd;
		wnd = (xwindow_t *)drawable;
		if (!wnd->buffer)
		{
			wnd->buffer = malloc(wnd->width*wnd->height*4);
			XW_ClearArea(wnd, 0, 0, wnd->width, wnd->height);
		}

		drwidth = wnd->width;
		drheight = wnd->height;
		drbuffer = wnd->buffer;
		drdepth = wnd->depth;
	}
	else if (drawable->restype == x_pixmap)
	{
		xpixmap_t *pm;
		pm = (xpixmap_t *)drawable;
		if (!pm->data)
		{
			pm->data = malloc(pm->width*pm->height*4);
			memset(pm->data, rand(), pm->width*pm->height*4);
		}

		drwidth = pm->width;
		drheight = pm->height;
		drbuffer = pm->data;
		drdepth = pm->depth;
	}
	else
	{
		X_SendError(cl, BadDrawable, req->drawable, X_PutImage, 0);
		return;
	}

	xrefreshed = true;

	if (req->dstX < 0)
	{
		req->width += req->dstX;
		req->dstX = 0;
	}
	if (req->dstY < 0)
	{
		req->height += req->dstY;
		req->dstY = 0;
	}

	if (req->width > drwidth - req->dstX)
		req->width = drwidth - req->dstX;

	if (req->height > drheight - req->dstY)
		req->height = drheight - req->dstY;

	in = (qbyte *)(req+1);

	//my own bugs...
	if (req->leftPad != 0)
	{
		X_SendError(cl, BadImplementation, req->drawable, X_PutImage, req->format);
		return;
	}

	if (req->format == XYBitmap)
	{	//bitmaps are just a 'mask' specifying which pixels get foreground and which get background.
		int bnum;
		unsigned int *o4;
		bnum=0;
		if (req->depth == 1)
		{
			while(req->height)
			{
				bnum += req->leftPad;

				out = drbuffer + (req->dstX + req->dstY*drwidth)*4;
				o4 = (unsigned int*)out;
				for (i = 0; i < req->width; i++)
				{
					if (in[bnum>>8]&(1<<(bnum&7)))
						o4[i] = gc->fgcolour;
					else
						o4[i] = gc->bgcolour;
					bnum++;
				}
				bnum += req->width;

				req->height--;
				req->dstY++;
			}
		}
		else
			X_SendError(cl, BadMatch, req->drawable, X_PutImage, 0);
	}
	else if (req->depth != drdepth)	/*depth must match*/
	{
		X_SendError(cl, BadMatch, req->drawable, X_PutImage, 0);
	}
	else if (req->format == ZPixmap)	//32 bit network bandwidth (hideous)
	{
		if (req->leftPad != 0)
		{
			X_SendError(cl, BadMatch, req->drawable, X_PutImage, 0);
			return;
		}

		if (req->depth == 1)
		{
			unsigned int *o4;
			int bnum = 0;
			while(req->height)
			{
				out = drbuffer + (req->dstX + req->dstY*drwidth)*4;
				o4 = (unsigned int*)out;
				for (i = 0; i < req->width; i++, bnum++)
				{
					if (in[bnum>>8]&(1<<(bnum&7)))
						o4[i] = 0xffffff;
					else
						o4[i] = 0;
				}

				req->height--;
				req->dstY++;
			}
		}
		else
		{
			while(req->height)
			{
				unsigned int *i4, *o4;
				out = drbuffer + (req->dstX + req->dstY*drwidth)*4;
				i4 = (unsigned int*)in;
				o4 = (unsigned int*)out;
				for (i = 0; i < req->width; i++)
				{
					o4[i] = i4[i];
				}
	/*			for (i = 0; i < req->width; i++)
				{
					out[i*4+0] = in[i*4+0];
					out[i*4+1] = in[i*4+1];
					out[i*4+2] = in[i*4+2];
				}
	*/			in += req->width*4;

				req->height--;
				req->dstY++;
			}
		}
	}
	else if (req->format == XYPixmap)
	{
		while(req->height)
		{
			unsigned int *o4;
			out = drbuffer + (req->dstX + req->dstY*drwidth)*4;
			o4 = (unsigned int*)out;
			for (i = 0; i < req->width; i++)
			{
				if (in[i>>3] & (1u<<(i&7)))
					o4[i] = rand();
				else
					o4[i] = rand();
			}
			in += (req->width+7)/8;

			req->height--;
			req->dstY++;
		}
	}
	else
	{
		X_SendError(cl, BadImplementation, req->drawable, X_PutImage, req->format);
	}
}
void XR_GetImage(xclient_t *cl, xReq *request)
{
	unsigned char *out;
	unsigned char *in;
	xGetImageReq *req = (xGetImageReq *)request;
	xresource_t *drawable;
	int i;

	unsigned int buffer[65535];
	xGetImageReply *rep = (xGetImageReply *)buffer;

	int drwidth;
	int drheight;
	unsigned char *drbuffer;

	if (XS_GetResource(req->drawable, (void**)&drawable) == x_none)
	{
		X_SendError(cl, BadDrawable, req->drawable, X_GetImage, 0);
		return;
	}


	if (drawable->restype == x_window)
	{
		xwindow_t *wnd;
		wnd = (xwindow_t *)drawable;
		if (!wnd->buffer)
		{
			wnd->buffer = malloc(wnd->width*wnd->height*4);
			XW_ClearArea(wnd, 0, 0, wnd->width, wnd->height);
		}

		drwidth = wnd->width;
		drheight = wnd->height;
		drbuffer = wnd->buffer;

		rep->visual			= 0x22;
	}
	else if (drawable->restype == x_pixmap)
	{
		xpixmap_t *pm;
		pm = (xpixmap_t *)drawable;
		if (!pm->data)
		{
			pm->data = malloc(pm->width*pm->height*4);
			memset(pm->data, rand(), pm->width*pm->height*4);
		}

		drwidth = pm->width;
		drheight = pm->height;
		drbuffer = pm->data;

		rep->visual			= 0;
	}
	else
	{
		X_SendError(cl, BadDrawable, req->drawable, X_GetImage, 0);
		return;
	}

	if (req->x < 0)
	{
		X_SendError(cl, BadValue, req->drawable, X_GetImage, 0);
		return;
	}
	if (req->y < 0)
	{
		X_SendError(cl, BadValue, req->drawable, X_GetImage, 0);
		return;
	}

	if (req->width > drwidth - req->x)
	{
		X_SendError(cl, BadValue, req->drawable, X_GetImage, 0);
		return;
	}

	if (req->height > drheight - req->y)
	{
		X_SendError(cl, BadValue, req->drawable, X_GetImage, 0);
		return;
	}

	out = (qbyte *)(rep+1);
	if (req->format == 2)	//32 bit network bandwidth (hideous)
	{
		while(req->height)
		{
			in = drbuffer + (req->x + req->y*drwidth)*4;
			for (i = 0; i < req->width; i++)
			{
				out[i*4+0] = in[i*4+0];
				out[i*4+1] = in[i*4+1];
				out[i*4+2] = in[i*4+2];
			}
			out += req->width*4;

			req->height--;
			req->y++;
		}
	}
	else
	{
		X_SendError(cl, BadImplementation, req->drawable, X_GetImage, req->format);
		return;
	}

	rep->type			= X_Reply;
	rep->sequenceNumber	= cl->requestnum;
	rep->length			= (out-(qbyte *)(rep+1)+3)/4;
	rep->depth			= 24;
	rep->pad3			= 0;
	rep->pad4			= 0;
	rep->pad5			= 0;
	rep->pad6			= 0;
	rep->pad7			= 0;

	X_SendData(cl, rep, sizeof(*rep)+rep->length*4);
}

void XW_PolyLine(unsigned int *dbuffer, int dwidth, int dheight, int x1, int x2, int y1, int y2, xgcontext_t *gc)
{
	//FIXME: cap to region.
	int len;

	int dx, dy;

	if (x1 < 0)
		return;
	if (x2 < 0)
		return;
	if (y1 < 0)
		return;
	if (y2 < 0)
		return;

	if (x1 >= dwidth)
		x1 = dwidth-1;
	if (x2 >= dwidth)
		x2 = dwidth-1;

	if (y1 >= dheight)
		y1 = dheight-1;
	if (y2 >= dheight)
		y2 = dheight-1;

	dx = (x2 - x1);
	dy = (y2 - y1);
	len = (int)sqrt(dx*dx+dy*dy);

	if (!len)
		return;

	x1<<=16;
	y1<<=16;
	dx=(dx<<16)/len;
	dy=(dy<<16)/len;

	for (; len ; len--)
	{
		GCFunc(gc->fgcolour, dbuffer[(x1>>16)+dwidth*(y1>>16)], gc->function, dbuffer[(x1>>16)+dwidth*(y1>>16)], 0xffffff);
		x1+=dx;
		y1+=dy;
	}
}

void XR_PolyLine(xclient_t *cl, xReq *request)
{
	xPolyLineReq *req = (xPolyLineReq *)request;

	xresource_t *drawable;
	xgcontext_t *gc;

	int pointsleft;

	int drwidth;
	int drheight;
	unsigned char *drbuffer;
	INT16 start[2], end[2];

	INT16 *points;
	points = (INT16 *)(req+1);



	if (XS_GetResource(req->drawable, (void**)&drawable) == x_none)
	{
		X_SendError(cl, BadDrawable, req->drawable, X_PolyRectangle, 0);
		return;
	}

	if (XS_GetResource(req->gc, (void**)&gc) == x_none)
	{
		X_SendError(cl, BadGC, req->gc, X_PolyRectangle, 0);
		return;
	}

	if (drawable->restype == x_window)
	{
		xwindow_t *wnd;
		wnd = (xwindow_t *)drawable;
		if (!wnd->buffer)
		{
			wnd->buffer = malloc(wnd->width*wnd->height*4);
			XW_ClearArea(wnd, 0, 0, wnd->width, wnd->height);
		}

		drwidth = wnd->width;
		drheight = wnd->height;
		drbuffer = wnd->buffer;
	}
	else if (drawable->restype == x_pixmap)
	{
		xpixmap_t *pm;
		pm = (xpixmap_t *)drawable;
		if (!pm->data)
		{
			pm->data = malloc(pm->width*pm->height*4);
			memset(pm->data, rand(), pm->width*pm->height*4);
		}

		drwidth = pm->width;
		drheight = pm->height;
		drbuffer = pm->data;
	}
	else
	{
		X_SendError(cl, BadDrawable, req->drawable, X_PolyRectangle, 0);
		return;
	}

	xrefreshed = true;

	if (req->reqType == X_PolySegment)
	{
		for (pointsleft = req->length/2-3; pointsleft>0; pointsleft--)
		{
			XW_PolyLine((unsigned int *)drbuffer, drwidth, drheight, points[0], points[2], points[1], points[3], gc);
			points+=4;
		}
	}
	else
	{
		end[0] = 0;
		end[1] = 0;

		for (pointsleft = req->length-3; pointsleft>0; pointsleft-=2)
		{
			if (req->coordMode == 1/*Previous*/)
			{
				start[0] = end[0] + points[0];
				start[1] = end[1] + points[1];
				end[0] = start[0] + points[2];
				end[1] = start[1] + points[3];
			}
			else
			{
				start[0] = points[0];
				start[1] = points[1];
				end[0] = points[2];
				end[1] = points[3];
			}
			XW_PolyLine((unsigned int *)drbuffer, drwidth, drheight, start[0], end[0], start[1], end[1], gc);
			points+=4;
		}
	}
}

void XR_FillPoly(xclient_t *cl, xReq *request)
{
	xFillPolyReq *req = (xFillPolyReq *)request;
	INT16 *points = (INT16*)(req+1);
	int numpoints = req->length-4;

	xresource_t *drawable;
	xgcontext_t *gc;

	int drwidth;
	int drheight;
	unsigned char *drbuffer;
	INT16 start[2], end[2];

	points = (INT16 *)(req+1);

	

	if (XS_GetResource(req->drawable, (void**)&drawable) == x_none)
	{
		X_SendError(cl, BadDrawable, req->drawable, X_PolyRectangle, 0);
		return;
	}

	if (XS_GetResource(req->gc, (void**)&gc) == x_none)
	{
		X_SendError(cl, BadGC, req->gc, X_PolyRectangle, 0);
		return;
	}

	if (drawable->restype == x_window)
	{
		xwindow_t *wnd;
		wnd = (xwindow_t *)drawable;
		if (!wnd->buffer)
		{
			wnd->buffer = malloc(wnd->width*wnd->height*4);
			XW_ClearArea(wnd, 0, 0, wnd->width, wnd->height);
		}

		drwidth = wnd->width;
		drheight = wnd->height;
		drbuffer = wnd->buffer;
	}
	else if (drawable->restype == x_pixmap)
	{
		xpixmap_t *pm;
		pm = (xpixmap_t *)drawable;
		if (!pm->data)
		{
			pm->data = malloc(pm->width*pm->height*4);
			memset(pm->data, rand(), pm->width*pm->height*4);
		}

		drwidth = pm->width;
		drheight = pm->height;
		drbuffer = pm->data;
	}
	else
	{
		X_SendError(cl, BadDrawable, req->drawable, X_PolyRectangle, 0);
		return;
	}

	xrefreshed = true;



	{
		start[0] = points[(numpoints*2)-2];
		start[1] = points[(numpoints*2)-1];
		while (numpoints-->0)
		{
			if (req->coordMode == 1/*Previous*/)
			{
				end[0] = start[0] + points[0];
				end[1] = start[1] + points[1];
			}
			else
			{
				end[0] = points[0];
				end[1] = points[1];
			}
			points+=2;

//			XW_PolyLine((unsigned int *)drbuffer, drwidth, drheight, start[0], start[1], end[0], end[1], gc);
			points++;

			start[0] = end[0];
			start[1] = end[1];
		}
	}
}

void XR_PolyRectangle(xclient_t *cl, xReq *request)
{
	unsigned int *out;
	xPolyRectangleReq *req = (xPolyRectangleReq *)request;
	xresource_t *drawable;
	xgcontext_t *gc;
	int i;

	short *rect;
	int rectnum;

	int drwidth;
	int drheight;
	unsigned char *drbuffer;



	if (XS_GetResource(req->drawable, (void**)&drawable) == x_none)
	{
		X_SendError(cl, BadDrawable, req->drawable, X_PolyRectangle, 0);
		return;
	}

	if (XS_GetResource(req->gc, (void**)&gc) == x_none)
	{
		X_SendError(cl, BadGC, req->gc, X_PolyRectangle, 0);
		return;
	}

	if (drawable->restype == x_window)
	{
		xwindow_t *wnd;
		wnd = (xwindow_t *)drawable;
		if (!wnd->buffer)
		{
			wnd->buffer = malloc(wnd->width*wnd->height*4);
			XW_ClearArea(wnd, 0, 0, wnd->width, wnd->height);
		}

		drwidth = wnd->width;
		drheight = wnd->height;
		drbuffer = wnd->buffer;
	}
	else if (drawable->restype == x_pixmap)
	{
		xpixmap_t *pm;
		pm = (xpixmap_t *)drawable;
		if (!pm->data)
		{
			pm->data = malloc(pm->width*pm->height*4);
			memset(pm->data, rand(), pm->width*pm->height*4);
		}

		drwidth = pm->width;
		drheight = pm->height;
		drbuffer = pm->data;
	}
	else
	{
		X_SendError(cl, BadDrawable, req->drawable, X_PolyRectangle, 0);
		return;
	}

	xrefreshed = true;


	rect = (short *)(req+1);
	for (rectnum = req->length/2 - 1; rectnum>0; rectnum--)
	{

//		Con_Printf("polyrect %i %i %i %i %i\n", req->drawable, rect[0], rect[1], rect[2], rect[3]);

		if (rect[2] < 0)
		{
			rect[2] *= -1;
		}
		if (rect[3] < 0)
		{
			rect[3] *= -1;
		}

		if (rect[0] < 0)
		{
			rect[2] += rect[0];
			rect[0] = 0;
		}
		if (rect[0] >= drwidth)
			rect[0] = drwidth-1;
		if (rect[1] < 0)
		{
			rect[3] += rect[1];
			rect[1] = 0;
		}
		if (rect[1] >= drheight)
			rect[1] = drheight-1;

		if (rect[0] + rect[2] > drwidth)
			rect[2] = drwidth - rect[0];
		if (rect[1] + rect[3] > drheight)
			rect[3] = drheight - rect[1];
		if (request->reqType == X_PolyFillRectangle)	//fill
		{
			while(rect[3])
			{
				out = (unsigned int *)drbuffer + (rect[0] + rect[1]*drwidth);
				for (i = 0; i < rect[2]; i++)
				{
					GCFunc(gc->fgcolour, *(char *)&out[i], gc->function, *(char *)&out[i], 0xff);
				}

				rect[3]--;
				rect[1]++;
			}
		}
		else	//outline
		{
			//top
			out = (unsigned int *)drbuffer + (rect[0] + rect[1]*drwidth);
			for (i = 1; i < rect[2]-1; i++)
			{
				GCFunc(gc->fgcolour, out[i], gc->function, out[i], 0xffffff);
			}

			//bottom
			if (rect[3]-1)
			{
				out = (unsigned int *)drbuffer + (rect[0] + (rect[1]+rect[3]-1)*drwidth);
				for (i = 1; i < rect[2]-1; i++)
				{
					GCFunc(gc->fgcolour, out[i], gc->function, out[i], 0xffffff);
				}
			}

			//left
			out = (unsigned int *)drbuffer + (rect[0] + rect[1]*drwidth);
			for (i = 0; i < rect[3]; i++)
			{
				GCFunc(gc->fgcolour, out[i*drwidth], gc->function, out[i*drwidth], 0xffffff);
			}

			//right
			if (rect[2]-1)
			{
				out = (unsigned int *)drbuffer + (rect[0]+rect[2]-1 + rect[1]*drwidth);
				for (i = 0; i < rect[3]; i++)
				{
					GCFunc(gc->fgcolour, out[i*drwidth], gc->function, out[i*drwidth], 0xffffff);
				}
			}
		}

		rect+=4;
	}
}

void XR_PolyPoint(xclient_t *cl, xReq *request)
{
	xPolyPointReq *req = (xPolyPointReq *)request;

	unsigned int *out;
	xresource_t *drawable;
	xgcontext_t *gc;

	short *point;
	int pointnum;

	int drwidth;
	int drheight;
	unsigned char *drbuffer;

	short lastpoint[2];

	if (XS_GetResource(req->drawable, (void**)&drawable) == x_none)
	{
		X_SendError(cl, BadDrawable, req->drawable, X_PolyPoint, 0);
		return;
	}

	if (XS_GetResource(req->gc, (void**)&gc) != x_gcontext)
	{
		X_SendError(cl, BadGC, req->gc, X_PolyPoint, 0);
		return;
	}

	if (drawable->restype == x_window)
	{
		xwindow_t *wnd;
		wnd = (xwindow_t *)drawable;
		if (!wnd->buffer)
		{
			wnd->buffer = malloc(wnd->width*wnd->height*4);
			XW_ClearArea(wnd, 0, 0, wnd->width, wnd->height);
		}

		drwidth = wnd->width;
		drheight = wnd->height;
		drbuffer = wnd->buffer;
	}
	else if (drawable->restype == x_pixmap)
	{
		xpixmap_t *pm;
		pm = (xpixmap_t *)drawable;
		if (!pm->data)
		{
			pm->data = malloc(pm->width*pm->height*4);
			memset(pm->data, rand(), pm->width*pm->height*4);
		}

		drwidth = pm->width;
		drheight = pm->height;
		drbuffer = pm->data;
	}
	else
	{
		X_SendError(cl, BadDrawable, req->drawable, X_PolyPoint, 0);
		return;
	}

	point = (short*)(req+1);

	if (req->coordMode)	//relative
	{
		lastpoint[0] = 0;	//do the absolute stuff
		lastpoint[1] = 0;
		for (pointnum = 1; pointnum>0; pointnum--)
		{
			lastpoint[0] += point[0];	//do the absolute stuff
			lastpoint[1] += point[1];

			out = (unsigned int *)drbuffer + lastpoint[0] + lastpoint[1]*drwidth;
			GCFunc(gc->fgcolour, *out, gc->function, *out, 0xffffff);
			point+=2;
		}

	}
	else	//absolute
	{
		for (pointnum = req->length-1; pointnum>0; pointnum--)
		{
			if (!(point[0] < 0 || point[1] < 0 || point[0] >= drwidth || point[1] >= drheight))
			{
				out = (unsigned int *)drbuffer + point[0] + point[1]*drwidth;
				GCFunc(gc->fgcolour, *out, gc->function, *out, 0xffffff);
			}

			point+=2;
		}
	}
}

void Draw_CharToDrawable (int num, unsigned int *drawable, int x, int y, int width, int height, xgcontext_t *gc)
{
	int		row, col;
	unsigned int	*source;
	int		drawline;
	int		i;

	int s, e;

	xfont_t *font;

	font = gc->font;
	if (!font || font->res.restype != x_font)
		return;

	s = 0;
	e = font->charwidth;
	if (x<0)
		s = s - x;

	if (x > width - e)
		e = width - x;

	if (s > e)
		return;
//	if (y >= height-e)
//		return;

//	if (y < -font->charheight)
//		return;

	if (y <= 0)
		drawable += x;
	else
		drawable += (width*y) + x;

	row = num>>4;
	col = num&15;
	source = font->data + (row*font->rowwidth*font->charheight) + (col*font->charwidth);
	if (y < 0)
		source -= font->rowwidth*y;

	drawline = height-y;
	if (drawline > font->charheight)
		drawline = font->charheight;

	if (y < 0)
			drawline += y;

	while (drawline-->=0)
	{
		for (i=s ; i<e ; i++)
			if ((char*)(&source[i])[3] > 128 && source[i])
//				GCFunc(gc->fgcolour, drawable[i], gc->function, drawable[i], source[i]);
				drawable[i] = source[i];
		source += font->rowwidth;
		drawable += width;
	}
}

void XR_PolyText(xclient_t *cl, xReq *request)
{
	unsigned char *str;
  
	xPolyTextReq	*req = (xPolyTextReq*)request;
	
	xresource_t *drawable;
	xgcontext_t *gc;


	int drwidth;
	int drheight;
	unsigned char *drbuffer;

	short charnum;
	short numchars;

	int xpos, ypos;

	if (XS_GetResource(req->drawable, (void**)&drawable) == x_none)
	{
		X_SendError(cl, BadDrawable, req->drawable, req->reqType, 0);
		return;
	}

	if (XS_GetResource(req->gc, (void**)&gc) != x_gcontext)
	{
		X_SendError(cl, BadGC, req->gc, req->reqType, 0);
		return;
	}
	if (!gc->font)
	{
		X_SendError(cl, BadGC, req->gc, req->reqType, 0);
		return;
	}

	if (drawable->restype == x_window)
	{
		xwindow_t *wnd;
		wnd = (xwindow_t *)drawable;
		if (!wnd->buffer)
		{
			wnd->buffer = malloc(wnd->width*wnd->height*4);
			XW_ClearArea(wnd, 0, 0, wnd->width, wnd->height);
		}

		drwidth = wnd->width;
		drheight = wnd->height;
		drbuffer = wnd->buffer;
	}
	else if (drawable->restype == x_pixmap)
	{
		xpixmap_t *pm;
		pm = (xpixmap_t *)drawable;
		if (!pm->data)
		{
			pm->data = malloc(pm->width*pm->height*4);
			memset(pm->data, rand(), pm->width*pm->height*4);
		}

		drwidth = pm->width;
		drheight = pm->height;
		drbuffer = pm->data;
	}
	else
	{
		X_SendError(cl, BadDrawable, req->drawable, req->reqType, 0);
		return;
	}

	xrefreshed = true;

	xpos = req->x;
	ypos = req->y-gc->font->charheight/2;

	str = (char*)(req+1);

	if (req->reqType == X_ImageText16 || req->reqType == X_ImageText8)
	{
		while(1)
		{
			charnum = 0;
			charnum |= *str++;
			if (req->reqType == X_ImageText16)
				charnum |= (*str++)<<8;
			if (!charnum)
				return;

			Draw_CharToDrawable(charnum&255, (unsigned int *)drbuffer, xpos, ypos, drwidth, drheight, gc);

			xpos += gc->font->charwidth;
		}
	}
	else
	{
		numchars = *(short*) str;
		str+=2;
		while(1)
		{
			charnum = 0;
			if (req->reqType == X_PolyText16)
				charnum |= (*str++)<<8;
			charnum |= *str++;
			if (!numchars--)
				return;

			Draw_CharToDrawable(charnum, (unsigned int *)drbuffer, xpos, ypos, drwidth, drheight, gc);

			xpos += gc->font->charwidth;
		}
	}
}

void XR_OpenFont(xclient_t *cl, xReq *request)	//basically ignored. We only support one font...
{
	xOpenFontReq *req = (xOpenFontReq *)request;
	char *name;

	name = (char *)(req+1);

	XS_CreateFont(req->fid, cl, name);
}

void XR_ListFonts(xclient_t *cl, xReq *request)	//basically ignored. We only support one font...
{
//	xListFontsReq *req = (xListFontsReq *)request;
	int buffer[256];
	xListFontsReply *reply = (xListFontsReply *)buffer;

	reply->type	= X_Reply;
    reply->sequenceNumber	= cl->requestnum;
    reply->length	= 0;
    reply->nFonts	= 0;

	X_SendData(cl, reply, sizeof(xGenericReply)+reply->length*4);
}

void XR_QueryFont(xclient_t *cl, xReq *request)	//basically ignored. We only support one font...
{
	xResourceReq *req = (xResourceReq *)request;
	char buffer[8192];
	int i;
	xCharInfo *ci;
	xQueryFontReply *rep = (xQueryFontReply *)buffer;
	xfont_t	*font;
	if (XS_GetResource(req->id, &font) != x_font)
	{
		X_SendError(cl, BadFont, req->id, req->reqType, 0);
		return;
	}

	rep->type			= X_Reply;
	rep->pad1			= 0;
	rep->sequenceNumber	= cl->requestnum;
	rep->length			= 0;  /* definitely > 0, even if "nCharInfos" is 0 */

	rep->minBounds.leftSideBearing	= 0;
	rep->minBounds.rightSideBearing	= 0;
	rep->minBounds.characterWidth	= font->charwidth;
	rep->minBounds.ascent			= font->charheight/2;
	rep->minBounds.descent			= font->charheight/2;
	rep->minBounds.attributes		= 0;

#ifndef WORD64
	rep->walign1		= 0;
#endif
	rep->maxBounds.leftSideBearing	= 0;
	rep->maxBounds.rightSideBearing	= 0;
	rep->maxBounds.characterWidth	= font->charwidth;
	rep->maxBounds.ascent			= font->charheight/2;
	rep->maxBounds.descent			= font->charheight/2;
	rep->maxBounds.attributes		= 0;
#ifndef WORD64
	rep->walign2		= 0;
#endif
	rep->minCharOrByte2	= 0;
	rep->maxCharOrByte2	= 0;
	rep->defaultChar	= 0;
	rep->nFontProps		= 0;  /* followed by this many xFontProp structures */
	rep->drawDirection	= 0;
	rep->minByte1		= 0;
	rep->maxByte1		= 0;
	rep->allCharsExist	= 0;
	rep->fontAscent		= font->charheight/2;
	rep->fontDescent	= font->charheight/2;
	rep->nCharInfos		= 255; /* followed by this many xCharInfo structures */

	rep->length = ((sizeof(xQueryFontReply) - sizeof(xGenericReply)) + rep->nFontProps*sizeof(xFontProp) + rep->nCharInfos*sizeof(xCharInfo))/4;

	ci = (xCharInfo*)(rep+1);
	for (i = 0; i < rep->nCharInfos; i++)
	{
		ci[i].leftSideBearing = 0;
		ci[i].rightSideBearing = 0;
		ci[i].characterWidth = font->charwidth;
		ci[i].ascent = font->charheight/2;
		ci[i].descent = font->charheight/2;
	}

	X_SendData(cl, rep, sizeof(xGenericReply)+rep->length*4);
}

//esentually just throw it back at them.
void XR_AllocColor(xclient_t *cl, xReq *request)
{
	xAllocColorReq *req = (xAllocColorReq *)request;
	xAllocColorReply rep;
	unsigned char rgb[3] = {req->red>>8, req->green>>8, req->blue>>8};

	rep.type			= X_Reply;
	rep.pad1			= 0;
	rep.sequenceNumber	= cl->requestnum;
	rep.length			= 0;
	rep.red				= req->red;
	rep.green			= req->green;
	rep.blue			= req->blue;
	rep.pad2			= 0;
	rep.pixel			= (rgb[0]<<16) | (rgb[1]<<8) | (rgb[2]);
	rep.pad3			= 0;
	rep.pad4			= 0;
	rep.pad5			= 0;

	X_SendData(cl, &rep, sizeof(rep));
}

void XR_LookupColor(xclient_t *cl, xReq *request)
{
	typedef struct  {
		char *name;
		float r, g, b;
	} colour_t;
	char colourname[256];
	colour_t *c, colour[] = {
		{"black",	0,0,0},
		{"grey",	0.5f,0.5f,0.5f},
		{"gray",	0.5f,0.5f,0.5f},	//wmaker uses this one. humour it.
		{"gray90",	0.9f,0.9f,0.9f},
		{"gray80",	0.8f,0.8f,0.8f},
		{"gray70",	0.7f,0.7f,0.7f},
		{"gray60",	0.6f,0.6f,0.6f},
		{"gray50",	0.5f,0.5f,0.5f},
		{"gray40",	0.4f,0.4f,0.4f},
		{"gray30",	0.3f,0.3f,0.3f},
		{"gray20",	0.2f,0.2f,0.2f},
		{"gray10",	0.1f,0.1f,0.1f},
		{"grey10",	0.1f,0.1f,0.1f},
		{"white",	1,1,1},
		{"red",		1,0,0},
		{"green",	0,1,0},
		{"blue",	0,0,1},
		{"blue4",	0,0,0.4f},
		{NULL}
	};

	xLookupColorReq *req = (xLookupColorReq *)request;
	xLookupColorReply rep;

	if (req->nbytes >= sizeof(colourname))
	{
		X_SendError(cl, BadName, 0, X_LookupColor, 0);
		return;
	}
	memcpy(colourname, (char *)(req+1), req->nbytes);
	colourname[req->nbytes] = '\0';

	for (c = colour; c->name; c++)
	{
		if (!strcasecmp(c->name, colourname))
		{
			break;
		}
	}

	if (!c->name)
	{
		X_SendError(cl, BadName, 0, X_LookupColor, 0);
		return;
	}


	rep.type			= X_Reply;
	rep.pad1			= 0;
	rep.sequenceNumber	= cl->requestnum;
	rep.length			= 0;
	rep.exactRed		= (unsigned short)(c->r*0xffffu);
	rep.exactGreen		= (unsigned short)(c->g*0xffffu);
	rep.exactBlue		= (unsigned short)(c->b*0xffffu);
	rep.screenRed		= rep.exactRed;
	rep.screenGreen		= rep.exactGreen;
	rep.screenBlue		= rep.exactBlue;
	rep.pad3			= 0;
	rep.pad4			= 0;
	rep.pad5			= 0;

	X_SendData(cl, &rep, sizeof(rep));
}

//get's keyboard status stuff.
void XR_GetKeyboardControl(xclient_t *cl, xReq *request)
{
	xGetKeyboardControlReply rep;

	rep.type				= X_Reply;
	rep.globalAutoRepeat	= false;
	rep.sequenceNumber		= cl->requestnum;
	rep.length				= 5;
	rep.ledMask				= 0;
	rep.keyClickPercent		= 0;
	rep.bellPercent			= 0;
	rep.bellPitch			= 0;
	rep.bellDuration		= 0;
	rep.pad					= 0;
	memset(rep.map, 0, sizeof(rep.map));	//per key map

	X_SendData(cl, &rep, sizeof(rep));
}

void XR_WarpPointer(xclient_t *cl, xReq *request)
{
//	xWarpPointerReq *req = (xWarpPointerReq *)request;
//	req->m
}

#ifdef XBigReqExtensionName
void XR_BigReq(xclient_t *cl, xReq *request)
{
	xBigReqEnableReply rep;

	rep.type				= X_Reply;
	rep.pad0				= 0;
	rep.sequenceNumber		= cl->requestnum;
	rep.length				= 0;
	rep.max_request_size	= 65535*1000;
	rep.pad1				= 0;
	rep.pad2				= 0;
	rep.pad3				= 0;
	rep.pad4				= 0;
	rep.pad5				= 0;

	X_SendData(cl, &rep, sizeof(rep));
}
#endif

#define MAXREQUESTSIZE	65535

//cl -> cl protocol
void XR_SendEvent (xclient_t *cl, xReq *request)
{
	int count;
	xSendEventReq *req = (xSendEventReq *)request;
	xwindow_t *wnd;

	if (XS_GetResource(req->destination, (void**)&wnd) != x_window)
	{
		X_SendError(cl, BadWindow, req->destination, X_SendEvent, 0);
		return;
	}

	if (!req->eventMask)	//goes to owner.
	{
		req->event.u.u.sequenceNumber = cl->requestnum;
		X_SendData(wnd->res.owner, &req->event, sizeof(req->event));
	}
	else
	{
		xnotificationmask_t *nm;
		count = 0;
		while(!count)
		{
			for (nm = wnd->notificationmask; nm; nm = nm->next)
			{
				if (!(nm->mask & req->eventMask))
					continue;
				cl = nm->client;

				if (cl->stillinitialising)
					continue;

				count++;

				if (cl->tobedropped)
					continue;
				if (cl->outbufferlen > MAXREQUESTSIZE*4)
				{
					cl->tobedropped = true;
					continue;
				}

				req->event.u.u.sequenceNumber = cl->requestnum;
				X_SendData(cl, &req->event, sizeof(xEvent));
			}
			if (req->propagate)
				wnd = wnd->parent;
			else
				break;
		}
	}
}

void XR_GrabServer (xclient_t *cl, xReq *request)
{
	xgrabbedclient = cl;
}
void XR_UngrabServer (xclient_t *cl, xReq *request)
{
	xgrabbedclient = NULL;
}

xclient_t *xpointergrabclient;
xwindow_t *xpgrabbedwindow;
xwindow_t *xpconfinewindow;
unsigned int xpointergrabmask;
CARD32 xpointergrabcursor;
void XR_GrabPointer (xclient_t *cl, xReq *request)
{
	xGrabPointerReq *req = (xGrabPointerReq *)request;
	xGrabPointerReply reply;

	reply.type				= X_Reply;
    reply.status			= 0;
    reply.sequenceNumber 	= cl->requestnum;
    reply.length 			= 0;
    reply.pad1 				= 0;
    reply.pad2				= 0;
    reply.pad3 				= 0;
    reply.pad4 				= 0;
    reply.pad5				= 0;
    reply.pad6				= 0;

	if (xpointergrabclient && xpointergrabclient != cl)
	{	//you can't have it.
//		if (pointerstatus == Frozen)
//			reply.status			= GrabFrozen;
//		else
			reply.status			= AlreadyGrabbed;
		X_SendData(cl, &reply, sizeof(reply));
		return;
	}

	xpointergrabclient = cl;
	XS_GetResource(req->grabWindow, (void**)&xpgrabbedwindow);
	XS_GetResource(req->confineTo, (void**)&xpconfinewindow);
	xpointergrabmask = req->eventMask;
	xpointergrabcursor = req->cursor;
//	xpointergrabtime = req->time;

	X_EvalutateCursorOwner(NotifyGrab);


	reply.status			= GrabSuccess;
	X_SendData(cl, &reply, sizeof(reply));
}
void XR_ChangeActivePointerGrab (xclient_t *cl, xReq *request)
{
	xChangeActivePointerGrabReq *req = (xChangeActivePointerGrabReq *)request;

	if (xpointergrabclient != cl)
	{	//its not yours to change
		return;
	}

	xpointergrabmask = req->eventMask;
	xpointergrabcursor = req->cursor;
//	xpointergrabtime = req->time;
}
void XR_UngrabPointer (xclient_t *cl, xReq *request)
{
	xpointergrabclient = NULL;
	xpgrabbedwindow = NULL;
	xpconfinewindow = NULL;

	X_EvalutateCursorOwner(NotifyUngrab);
}

void XR_NoOperation (xclient_t *cl, xReq *request)
{
}

void X_InitRequests(void)
{
	int ExtentionCode = X_NoOperation+1;

	memset(XRequests, 0, sizeof(XRequests));

	XRequests[X_QueryExtension] = XR_QueryExtension; 
	XRequests[X_ListExtensions] = XR_ListExtensions;
	XRequests[X_SetCloseDownMode] = XR_SetCloseDownMode;
	XRequests[X_GetProperty] = XR_GetProperty; 
	XRequests[X_ChangeProperty] = XR_ChangeProperty;
	XRequests[X_DeleteProperty] = XR_DeleteProperty;
	XRequests[X_ListProperties] = XR_ListProperties;
	XRequests[X_SetInputFocus] = XR_SetInputFocus;
	XRequests[X_GetInputFocus] = XR_GetInputFocus;
	XRequests[X_QueryBestSize] = XR_QueryBestSize;
	XRequests[X_CreateWindow] = XR_CreateWindow;
	XRequests[X_DestroyWindow] = XR_DestroyWindow;
	XRequests[X_QueryTree] = XR_QueryTree;
	XRequests[X_ChangeWindowAttributes] = XR_ChangeWindowAttributes;
	XRequests[X_GetWindowAttributes] = XR_GetWindowAttributes;
	XRequests[X_CreateGC] = XR_CreateGC;
	XRequests[X_ChangeGC] = XR_ChangeGC;
	XRequests[X_CopyGC] = XR_CopyGC;
	XRequests[X_FreeGC] = XR_FreeGC;
	XRequests[X_CreatePixmap] = XR_CreatePixmap;
	XRequests[X_FreePixmap] = XR_FreePixmap;
	XRequests[X_MapWindow] = XR_MapWindow;
	XRequests[X_MapSubwindows] = XR_MapSubwindows;
	XRequests[X_UnmapWindow] = XR_UnmapWindow;
	XRequests[X_ClearArea] = XR_ClearArea;
	XRequests[X_CopyArea] = XR_CopyArea;
	XRequests[X_InternAtom] = XR_InternAtom;
	XRequests[X_GetAtomName] = XR_GetAtomName;
	XRequests[X_PutImage] = XR_PutImage;
	XRequests[X_GetImage] = XR_GetImage;
	XRequests[X_PolyRectangle] = XR_PolyRectangle;
	XRequests[X_PolyFillRectangle] = XR_PolyRectangle;
	XRequests[X_PolyPoint] = XR_PolyPoint;
	XRequests[X_PolyLine] = XR_PolyLine;
	XRequests[X_PolySegment] = XR_PolyLine;
	XRequests[X_QueryPointer] = XR_QueryPointer;
	XRequests[X_GetKeyboardMapping] = XR_GetKeyboardMapping;
	XRequests[X_GetKeyboardControl] = XR_GetKeyboardControl;
	XRequests[X_AllocColor] = XR_AllocColor;
	XRequests[X_LookupColor] = XR_LookupColor;
	XRequests[X_GetGeometry] = XR_GetGeometry;
	XRequests[X_CreateCursor] = XR_CreateCursor;
	XRequests[X_CreateGlyphCursor] = XR_CreateGlyphCursor;
	XRequests[X_FreeCursor] = XR_FreeCursor;

	XRequests[X_WarpPointer] = XR_WarpPointer;

	XRequests[X_ListFonts] = XR_ListFonts;
	XRequests[X_OpenFont] = XR_OpenFont;
	XRequests[X_QueryFont] = XR_QueryFont;
	XRequests[X_PolyText8] = XR_PolyText;
	XRequests[X_PolyText16] = XR_PolyText;
	XRequests[X_ImageText8] = XR_PolyText;
	XRequests[X_ImageText16] = XR_PolyText;

	XRequests[X_ConfigureWindow] = XR_ConfigureWindow;
	XRequests[X_ReparentWindow] = XR_ReparentWindow;

	XRequests[X_GrabServer] = XR_GrabServer;
	XRequests[X_UngrabServer] = XR_UngrabServer;
	XRequests[X_GrabPointer] = XR_GrabPointer;
	XRequests[X_ChangeActivePointerGrab] = XR_ChangeActivePointerGrab;
	XRequests[X_UngrabPointer] = XR_UngrabPointer;


	XRequests[X_SendEvent] = XR_SendEvent;

	XRequests[X_GetSelectionOwner] = XR_GetSelectionOwner;
	XRequests[X_SetSelectionOwner] = XR_SetSelectionOwner;

	XRequests[X_GrabKey] = XR_NoOperation;
	XRequests[X_AllowEvents] = XR_NoOperation;
	XRequests[X_FillPoly] = XR_FillPoly;
	XRequests[X_NoOperation] = XR_NoOperation;

#ifdef XBigReqExtensionName
	X_BigReqCode=ExtentionCode++;
	XRequests[X_BigReqCode] = XR_BigReq;
#endif
}

