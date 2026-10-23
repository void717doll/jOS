#define main dwm_main
#include "../dwm.c"
#undef main
#include <assert.h>

static Window owner, parents[512];
static int mapped[512], widths[512], positions[512], heights[512], removed, destroyed;
static unsigned long iconflags = XEMBED_MAPPED;
static XEvent message;
static int statusx, layoutx;

Window __wrap_XGetSelectionOwner(Display *d, Atom selection) { return owner; }
int __wrap_XSetSelectionOwner(Display *d, Atom selection, Window w, Time time) { owner = w; return 1; }
Window __wrap_XCreateWindow(Display *d, Window parent, int x, int y, unsigned int w,
	unsigned int h, unsigned int border, int depth, unsigned int class,
	Visual *visual, unsigned long mask, XSetWindowAttributes *attributes) { return 400; }
Status __wrap_XGetWindowAttributes(Display *d, Window w, XWindowAttributes *attributes)
{
	memset(attributes, 0, sizeof *attributes);
	attributes->width = attributes->height = 32;
	return w == 401 || w == 402;
}
int __wrap_XGetWindowProperty(Display *d, Window w, Atom property, long offset, long length,
	Bool del, Atom requested, Atom *type, int *format, unsigned long *count,
	unsigned long *remaining, unsigned char **data)
{
	assert(property == xatom[XembedInfo]);
	unsigned long *values = calloc(2, sizeof *values);
	values[1] = iconflags;
	*data = (unsigned char *)values;
	*type = requested;
	*format = 32;
	*count = 2;
	*remaining = 0;
	return Success;
}
Status __wrap_XSendEvent(Display *d, Window w, Bool propagate, long mask, XEvent *event)
{
	message = *event;
	return 1;
}
int __wrap_XChangeProperty(Display *d, Window w, Atom prop, Atom type, int format,
	int mode, const unsigned char *data, int count) { return 1; }
int __wrap_XSetWindowBorderWidth(Display *d, Window w, unsigned int width) { assert(width == 0); return 1; }
int __wrap_XSelectInput(Display *d, Window w, long mask) { return 1; }
int __wrap_XAddToSaveSet(Display *d, Window w) { return 1; }
int __wrap_XRemoveFromSaveSet(Display *d, Window w) { removed++; return 1; }
int __wrap_XReparentWindow(Display *d, Window w, Window parent, int x, int y) { parents[w] = parent; return 1; }
int __wrap_XChangeWindowAttributes(Display *d, Window w, unsigned long mask, XSetWindowAttributes *a) { return 1; }
int __wrap_XSync(Display *d, Bool discard) { return 1; }
int __wrap_XMoveResizeWindow(Display *d, Window w, int x, int y, unsigned int width, unsigned int height)
{
	assert(width > 0 && height > 0);
	widths[w] = width; positions[w] = x; heights[w] = height;
	return 1;
}
int __wrap_XMapRaised(Display *d, Window w) { mapped[w] = 1; return 1; }
int __wrap_XMapWindow(Display *d, Window w) { mapped[w] = 1; return 1; }
int __wrap_XUnmapWindow(Display *d, Window w) { mapped[w] = 0; return 1; }
int __wrap_XSetWindowBackground(Display *d, Window w, unsigned long pixel) { return 1; }
int __wrap_XConfigureWindow(Display *d, Window w, unsigned int mask, XWindowChanges *changes) { return 1; }
int __wrap_XClearWindow(Display *d, Window w) { return 1; }
int __wrap_XDestroyWindow(Display *d, Window w) { destroyed++; return 1; }
unsigned int __wrap_drw_fontset_getwidth(Drw *d, const char *text) { return strlen(text) * 8; }
void __wrap_drw_setscheme(Drw *d, Clr *colors) { }
void __wrap_drw_rect(Drw *d, int x, int y, unsigned int w, unsigned int h, int filled, int invert) { }
void __wrap_drw_map(Drw *d, Window w, int x, int y, unsigned int width, unsigned int height) { }
int __wrap_drw_text(Drw *d, int x, int y, unsigned int w, unsigned int h, unsigned int pad, const char *text, int invert)
{
	if (!strcmp(text, "status")) statusx = x;
	if (!strcmp(text, "[]=")) layoutx = x;
	return x + w;
}

int main(void)
{
	_XPrivDisplay display = calloc(1, sizeof *display);
	Screen screeninfo = {.root_depth = 24};
	Fnt font = {.h = 24};
	Drw drawing = {.fonts = &font};
	Clr colorset[3] = {0}, *schemes[LENGTH(colors)];
	Monitor first = {.wx = -1000, .ww = 1000, .by = 0, .showbar = 1, .barwin = 100};
	Monitor second = {.wx = 0, .ww = 800, .by = 0, .showbar = 1, .barwin = 101};
	XEvent event = {0};

	display->screens = &screeninfo;
	dpy = (Display *)display;
	drw = &drawing;
	for (size_t i = 0; i < LENGTH(schemes); i++) schemes[i] = colorset;
	scheme = schemes;
	mons = selmon = &first;
	first.next = &second;
	bh = 24;
	root = 1;
	for (int i = 0; i < NetLast; i++) netatom[i] = 100 + i;
	for (int i = 0; i < XLast; i++) xatom[i] = 200 + i;
	strcpy(stext, "status");
	strcpy(first.ltsymbol, "[]=");

	owner = 399;
	initsystray();
	assert(!systray && owner == 399 && getsystraywidth() == 0);
	owner = None;
	initsystray();
	assert(systray && owner == 400 && message.xclient.message_type == xatom[Manager]);
	assert(message.xclient.data.l[1] == netatom[NetSystemTray]);
	event.xclient = (XClientMessageEvent){.type = ClientMessage, .window = 400,
		.message_type = netatom[NetSystemTrayOP], .format = 32, .data.l = {0, 0, 401, 0, 0}};
	clientmessage(&event);
	assert(systray->icons && !systray->icons->next && parents[401] == 400);
	assert(message.xclient.message_type == xatom[Xembed]);
	assert(message.xclient.data.l[1] == XEMBED_EMBEDDED_NOTIFY);
	assert(getsystraywidth() == 28 && widths[100] == 972 && widths[101] == 800);
	assert(positions[400] == -28 && statusx == 1000 - 28 - 50);
	clientmessage(&event);
	assert(!systray->icons->next);

	event.xresizerequest = (XResizeRequestEvent){.window = 401, .width = 64, .height = 32};
	resizerequest(&event);
	assert(widths[401] == 48 && heights[401] == 24 && widths[100] == 948);
	event.xresizerequest.height = 0;
	resizerequest(&event);
	assert(widths[401] == 96);
	iconflags = 0;
	event.xproperty = (XPropertyEvent){.window = 401, .atom = xatom[XembedInfo]};
	propertynotify(&event);
	assert(!mapped[401] && !mapped[400] && getsystraywidth() == 0 && widths[100] == 1000);
	event.xunmap = (XUnmapEvent){.window = 401};
	unmapnotify(&event);
	assert(!mapped[401]);
	iconflags = XEMBED_MAPPED;
	event.xproperty = (XPropertyEvent){.window = 401, .atom = xatom[XembedInfo]};
	propertynotify(&event);
	assert(mapped[401] && mapped[400]);
	selmon = &second;
	drawbars();
	assert(layoutx == 8 * LENGTH(tags));
	assert(widths[100] == 1000 && widths[101] == 700 && positions[400] == 700);
	second.showbar = 0;
	updatesystray();
	assert(!mapped[400]);
	second.showbar = 1;
	updatesystray();
	assert(mapped[400]);
	event.xdestroywindow = (XDestroyWindowEvent){.window = 401};
	destroynotify(&event);
	assert(!systray->icons && widths[101] == 800);
	event.xclient = (XClientMessageEvent){.window = 400, .message_type = netatom[NetSystemTrayOP],
		.format = 32, .data.l = {0, 0, 402, 0, 0}};
	clientmessage(&event);
	event.xselectionclear.selection = netatom[NetSystemTray];
	selectionclear(&event);
	assert(!systray && parents[402] == root && removed == 1 && destroyed == 1);
	assert(widths[101] == 800);
	cleanupsystray();
	free(display);
	puts("systray ownership, docking, sizing, hidden icons, bar space, monitor changes and cleanup: passed (x calls mocked)");
	return 0;
}
