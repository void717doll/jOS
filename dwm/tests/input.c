#define main dwm_main
#include "../dwm.c"
#undef main
#include <assert.h>

static int launches;
static Clr *drawn_scheme;
static int segments;
static char rendered[256];
static XEvent tab_event;
static int tab_messages;
static int copied, flushed;
static int pointerx, pointery, motionx, motiony, mouseevent, pointerwarps, pointerreleases;
static Bool pointervalid = True;
static int handlecount, handledestroyed, handleboxes[8][4], handlemapped[8];
static Cursor dragcursor;
static int titlebox[4], titlemapped, clientx, clienty, clientheight;
static int titlecleared, releasex, releasey;
static int buttongrabs, focusgrabs;
static unsigned long borderrows[4096], bordercolor, frameborder;
static int borderheight, borderdepth;
static Window destroyedclient;
static Window raisedwindow;
static int replayedclicks;

Status __wrap_XGetWMProtocols(Display *display, Window window, Atom **protocols, int *count) { return 0; }
int __wrap_XMoveWindow(Display *display, Window window, int x, int y) { return 1; }

Pixmap __wrap_XCreatePixmap(Display *display, Drawable drawable, unsigned int w, unsigned int h, unsigned int depth)
{
	assert(!destroyedclient || drawable != destroyedclient);
	assert(w == 1 && h > 0 && h <= LENGTH(borderrows));
	borderheight = h;
	borderdepth = depth;
	return 3000;
}
GC __wrap_XCreateGC(Display *display, Drawable drawable, unsigned long mask, XGCValues *values) { return (GC)borderrows; }
int __wrap_XFreeGC(Display *display, GC gc) { return 1; }
int __wrap_XFreePixmap(Display *display, Pixmap pixmap) { return 1; }
int __wrap_XSetForeground(Display *display, GC gc, unsigned long pixel) { bordercolor = pixel; return 1; }
int __wrap_XFillRectangle(Display *display, Drawable drawable, GC gc, int x, int y, unsigned int w, unsigned int h)
{
	assert(drawable == 3000 && x == 0 && w == 1 && y >= 0 && y + h <= borderheight);
	for (unsigned int row = y; row < y + h; row++)
		borderrows[row] = bordercolor;
	return 1;
}
int __wrap_XSetWindowBorderPixmap(Display *display, Window window, Pixmap pixmap) { assert(pixmap == 3000); return 1; }

XModifierKeymap *__wrap_XGetModifierMapping(Display *display)
{
	return calloc(1, sizeof(XModifierKeymap));
}
int __wrap_XUngrabButton(Display *display, unsigned int button, unsigned int modifiers, Window window) { return 1; }
int __wrap_XGrabButton(Display *display, unsigned int button, unsigned int modifiers,
                      Window window, Bool owner, unsigned int mask, int pointermode,
                      int keyboardmode, Window confined, Cursor cursorid)
{
	assert(keyboardmode == GrabModeAsync);
	if (button == AnyButton) {
		assert(pointermode == GrabModeSync);
		focusgrabs++;
	} else {
		assert(pointermode == GrabModeAsync);
		buttongrabs++;
	}
	return 1;
}

Window __wrap_XCreateWindow(Display *display, Window parent, int x, int y, unsigned int width,
                            unsigned int height, unsigned int border, int depth, unsigned int class,
                            Visual *visual, unsigned long mask, XSetWindowAttributes *attributes)
{
	if (class != InputOnly) {
		assert(class == CopyFromParent && depth == 24 && border == 1);
		return 2000;
	}
	int i = handlecount++;
	assert(i < 8 && class == InputOnly && depth == 0 && border == 0);
	assert(attributes->override_redirect && attributes->cursor == edgecursors[i]);
	assert(attributes->event_mask & ButtonPressMask);
	handleboxes[i][0] = x; handleboxes[i][1] = y;
	handleboxes[i][2] = width; handleboxes[i][3] = height;
	return 1000 + i;
}
int __wrap_XMoveResizeWindow(Display *display, Window window, int x, int y, unsigned int w, unsigned int h)
{
	if (window == 42) {
		clientx = x; clienty = y; clientheight = h;
		return 1;
	}
	if (window == 2000) {
		titlebox[0] = x; titlebox[1] = y; titlebox[2] = w; titlebox[3] = h;
		return 1;
	}
	assert(window >= 1000 && window < 1008 && w > 0 && h > 0);
	int i = window - 1000;
	handleboxes[i][0] = x; handleboxes[i][1] = y;
	handleboxes[i][2] = w; handleboxes[i][3] = h;
	return 1;
}
int __wrap_XMapWindow(Display *display, Window window) { if (window == 2000) titlemapped = 1; else handlemapped[window - 1000] = 1; return 1; }
int __wrap_XUnmapWindow(Display *display, Window window) { if (window == 2000) titlemapped = 0; else handlemapped[window - 1000] = 0; return 1; }
int __wrap_XDestroyWindow(Display *display, Window window) { handledestroyed++; return 1; }
int __wrap_XChangeProperty(Display *display, Window window, Atom prop, Atom type,
	int format, int mode, const unsigned char *data, int count) { return 1; }
int __wrap_XDefineCursor(Display *display, Window window, Cursor cursor) { return 1; }
int __wrap_XSetWindowBorder(Display *display, Window window, unsigned long pixel) { if (window == 2000) frameborder = pixel; return 1; }
int __wrap_XSetWindowBackground(Display *display, Window window, unsigned long pixel) { return 1; }
int __wrap_XClearWindow(Display *display, Window window) { if (window == 2000) titlecleared++; return 1; }
int __wrap_XSetWindowBorderWidth(Display *display, Window window, unsigned int width) { return 1; }

int __wrap_XRaiseWindow(Display *display, Window window) { raisedwindow = window; return 1; }
int __wrap_XAllowEvents(Display *display, int mode, Time time)
{
	assert(mode == ReplayPointer);
	replayedclicks++;
	return 1;
}
Bool __wrap_XCheckMaskEvent(Display *display, long mask, XEvent *event) { return False; }
int __wrap_XGrabPointer(Display *display, Window window, Bool owner, unsigned int mask,
                       int pointermode, int keyboardmode, Window confined, Cursor cursorid, Time time)
{
	dragcursor = cursorid;
	assert(pointermode == GrabModeAsync && keyboardmode == GrabModeAsync);
	return GrabSuccess;
}
int __wrap_XUngrabPointer(Display *display, Time time) { pointerreleases++; return 1; }
Bool __wrap_XQueryPointer(Display *display, Window window, Window *rootwindow, Window *child,
                         int *rx, int *ry, int *wx, int *wy, unsigned int *mask)
{
	*rx = pointerx;
	*ry = pointery;
	return pointervalid;
}
int __wrap_XWarpPointer(Display *display, Window source, Window dest, int x, int y,
                       unsigned int width, unsigned int height, int dx, int dy)
{
	pointerwarps++;
	return 1;
}
int __wrap_XMaskEvent(Display *display, long mask, XEvent *event)
{
	assert(mouseevent < 2);
	*event = (XEvent){.xmotion = {.type = mouseevent++ ? ButtonRelease : MotionNotify,
		.x = motionx, .y = motiony, .time = 1000}};
	if (event->type == ButtonRelease) {
		event->xbutton.x += releasex;
		event->xbutton.y += releasey;
	}
	return 1;
}

int __wrap_XConfigureWindow(Display *display, Window window, unsigned int mask, XWindowChanges *changes)
{
	if (window == 42 && (mask & CWHeight)) { clientheight = changes->height; clienty = changes->y; }
	if (window == 2000 && (mask & CWStackMode))
		assert(changes->sibling == 42 && changes->stack_mode == Below);
	if (mask & CWWidth) assert(changes->width > 0);
	if (mask & CWHeight) assert(changes->height > 0);
	if (window >= 1000 && window < 1008) {
		assert(mask == (CWSibling|CWStackMode));
		assert(changes->sibling == (window == 1000 ? 42 : window - 1) && changes->stack_mode == Above);
	}
	return 1;
}
int __wrap_XSync(Display *display, Bool discard) { return 1; }

int __wrap_XCopyArea(Display *display, Drawable source, Drawable target, GC gc,
                    int sx, int sy, unsigned int width, unsigned int height, int dx, int dy)
{
	copied++;
	return 1;
}
int __wrap_XFlush(Display *display) { flushed++; return 1; }

Status __wrap_XGetClassHint(Display *display, Window window, XClassHint *hint)
{
	if (window == 102 || window == 103) {
		hint->res_name = strdup(window == 103 ? "mpv" : "copyq");
		hint->res_class = strdup(window == 103 ? "mpv" : "copyq");
		return 1;
	}
	if (window != 100 && window != 101)
		return 0;
	hint->res_name = strdup("dwm-tabs");
	hint->res_class = strdup("tabbed");
	return 1;
}
Atom __wrap_XInternAtom(Display *display, const char *name, Bool only_existing) { return 42; }
Status __wrap_XSendEvent(Display *display, Window window, Bool propagate, long mask, XEvent *event)
{
	tab_event = *event;
	tab_messages++;
	return 1;
}

unsigned int __wrap_drw_fontset_getwidth(Drw *context, const char *text) { return strlen(text) * 8; }
void __wrap_drw_setscheme(Drw *context, Clr *colors) { drawn_scheme = colors; }
static int mask_x, mask_width;
void __wrap_drw_rect(Drw *context, int x, int y, unsigned int w, unsigned int h, int filled, int invert)
{
	if (filled && invert && w != 2) { mask_x = x; mask_width = w; }
}
int __wrap_drw_text(Drw *context, int x, int y, unsigned int w, unsigned int h, unsigned int pad, const char *text, int invert)
{
	for (const unsigned char *p = (unsigned char *)text; *p; p++)
		assert(*p >= 32);
	if (!strcmp(text, "bat 5%"))
		assert(drawn_scheme == scheme[SchemeCritical]);
	if (!strcmp(text, "muted"))
		assert(drawn_scheme == scheme[SchemeMuted]);
	if (!strcmp(text, "bat 73% charging"))
		assert(drawn_scheme == scheme[SchemeYellow]);
	strcat(rendered, text);
	segments++;
	return x + w;
}

pid_t __wrap_fork(void) { launches++; return 123; }
KeySym __wrap_XKeycodeToKeysym(Display *display, KeyCode code, int index)
{
	switch (code) {
	case 41: return XK_f;
	case 65: return XK_space;
	case 67: return XK_F1;
	case 134: return XK_Super_R;
	case 20: return XK_minus;
	case 21: return XK_equal;
	case 22: return XK_plus;
	case 82: return XK_KP_Subtract;
	case 86: return XK_KP_Add;
	}
	return code == 133 ? XK_Super_L : code == 37 ? XK_Control_L : XK_a;
}

int
main(void)
{
	Monitor monitor = {.lt = {&layouts[0]}};
	Client client = {0};
	XEvent focus_event = {.xfocus = {.mode = NotifyGrab, .window = 2}};
	assert(LENGTH(tags) == 5 && TAGMASK == 31);
	assert(!strcmp(layouts[0].symbol, "[]=") && !strcmp(layouts[1].symbol, "><>"));
	selmon = &monitor;
	grabbuttons(&client);
	assert(focusgrabs == 1 && buttongrabs > 0);
	buttongrabs = 0;
	grabbuttons(&client);
	assert(focusgrabs == 2 && buttongrabs > 0);

	for (int code = 133; code <= 134; code++) {
		XEvent event = {.xkey = {.keycode = code}};
		keypress(&event);
		keyrelease(&event);
		assert(launches == 0);
	}
	{
		XEvent event = {.xkey = {.keycode = 65, .state = MODKEY}};
		keypress(&event);
		assert(launches == 1);
		event.xkey.keycode = 67;
		event.xkey.state = Mod1Mask;
		keypress(&event);
		assert(launches == 2);
	}

	/* popup grabs must not redirect focus back into the selected client. */
	monitor.sel = &client;
	focusin(&focus_event);
	focus_event.xfocus.mode = NotifyUngrab;
	focusin(&focus_event);
	quit(NULL);
	assert(launches == 3);
	{
		Drw context = {.w = 800, .h = 24, .drawable = 1};
		drw_resize(&context, 800, 24);
		assert(context.drawable == 1);
		drw_map(&context, 10, 0, 0, 800, 24);
		assert(copied == 1 && flushed == 1);
	}
	{
		Client other = {.win = 20, .tags = 1, .mon = &monitor};
		client = (Client){.win = 10, .tags = 1, .mon = &monitor, .next = &other};
		monitor.clients = monitor.sel = &client;
		monitor.tagset[0] = 1;
		grouptabs(NULL);
		assert(launches == 4 && tab_messages == 0);
		other.win = 100;
		grouptabs(NULL);
		assert(tab_messages == 1 && tab_event.xclient.window == 100);
		assert(tab_event.xclient.data.l[0] == 0 && tab_event.xclient.data.l[1] == 10);
		client.win = 100;
		other.win = 20;
		grouptabs(NULL);
		assert(tab_messages == 2 && tab_event.xclient.data.l[1] == 20);
		other.win = 101;
		grouptabs(NULL);
		assert(tab_messages == 2 && launches == 4);
		ungrouptab(NULL);
		assert(tab_messages == 3 && tab_event.xclient.data.l[0] == 1);
		client.win = 10;
		ungrouptab(NULL);
		other.ishidden = 1;
		grouptabs(NULL);
		assert(tab_messages == 3 && launches == 4);
		other.ishidden = 0;
		other.win = 20;
		monitor.lt[0] = &layouts[1];
		grouptabs(NULL);
		assert(launches == 5 && tab_messages == 3);
		monitor.clients = monitor.sel = NULL;
	}
	{
		Clr palette[LENGTH(colors)][3] = {0};
		Clr *schemes[LENGTH(colors)];
		for (size_t i = 0; i < LENGTH(colors); i++)
			schemes[i] = palette[i];
		scheme = schemes;
		lrpad = 16;
		strcpy(stext, "\003bat 5%\001 | volume: \005muted\001");
		assert(statusbar(0, 0) == (int)strlen("bat 5% | volume: muted") * 8 + 2);
		assert(statusbar(100, 1) == statusbar(0, 0));
		assert(!strcmp(rendered, "bat 5% | volume: muted") && segments == 3);
		rendered[0] = '\0';
		strcpy(stext, "\006bat 73% charging\001");
		statusbar(100, 1);
		assert(!strcmp(rendered, "bat 73% charging"));
		assert(wifistart == -1 && wifiend == -1);
		strcpy(stext, "\005 muted\001   \007\004\021\004 15%\005 43ms\010\001   \030\004 on\031\001   \032 sunny\033   \026 date\027");
		statusbar(100, 0);
		assert(wifistart == (int)strlen(" muted   ") * 8);
		assert(wifiend - wifistart == (int)strlen(" 15% 43ms") * 8);
		{
			Monitor m = {.ww = 1800, .barwin = 700, .lt = {&layouts[0]}};
			Fnt font = {.h = 22};
			Drw drawing = {.fonts = &font};
			Systray tray = {0};
			Client icon = {.w = 24, .tags = 1};
			XEvent click = {.xbutton = {.window = 700, .button = Button1}};
			int before = launches, start;
			mons = selmon = &m;
			systray = &tray;
			tray.icons = &icon;
			strcpy(m.ltsymbol, layouts[0].symbol);
			start = m.ww - statusbar(0, 0) - getsystraywidth();
			click.xbutton.x = start + wifistart;
			buttonpress(&click);
			assert(launches == before + 1);
			click.xbutton.x = start + wifiend;
			buttonpress(&click);
			assert(launches == before + 1);
			click.xbutton.x--;
			buttonpress(&click);
			assert(launches == before + 2);
			click.xbutton.button = Button2;
			buttonpress(&click);
			assert(launches == before + 3);
			assert(bluetoothstart == wifiend + 3 * 8);
			assert(bluetoothend - bluetoothstart == (int)strlen(" on") * 8);
		assert(weatherstart == bluetoothend + 3 * 8);
		assert(weatherend - weatherstart == (int)strlen(" sunny") * 8);
		assert(clockstart == weatherend + 3 * 8);
			assert(clockend - clockstart == (int)strlen(" date") * 8);
			click.xbutton.button = Button1;
			click.xbutton.x = start + clockstart;
			buttonpress(&click);
			assert(launches == before + 4);
			click.xbutton.x = start + clockend;
			buttonpress(&click);
			assert(launches == before + 4);
			click.xbutton.x = start + bluetoothstart;
			buttonpress(&click);
			assert(launches == before + 5);
			click.xbutton.x = start + bluetoothend;
			buttonpress(&click);
			assert(launches == before + 5);
			click.xbutton.x--;
			buttonpress(&click);
			assert(launches == before + 6);
			click.xbutton.button = Button2;
			buttonpress(&click);
			assert(launches == before + 7);
			click.xbutton.button = Button1;
			click.xbutton.x = start + weatherstart;
			buttonpress(&click);
			assert(launches == before + 8);
			click.xbutton.x = start + weatherend;
			buttonpress(&click);
			assert(launches == before + 8);
			drw = &drawing;
			click.xbutton.button = Button1;
			click.xbutton.x = TEXTW(tags[0]) * LENGTH(tags) + 1;
			buttonpress(&click);
			assert(m.lt[m.sellt] == &layouts[0]);
			click.xbutton.button = Button3;
			buttonpress(&click);
			assert(m.lt[m.sellt] == &layouts[2]);
			drw = NULL;
			systray = NULL;
			mons = NULL;
			selmon = &monitor;
		}
		for (int bars = 1; bars <= 5; bars++) {
			snprintf(stext, sizeof stext, "\007\004%c\004 50%%\010\001", 16 + bars);
			mask_x = mask_width = 0;
			statusbar(100, 1);
			if (bars < 5) {
				assert(mask_x == 100 + 24 * bars / 5);
				assert(mask_width == 24 - 24 * bars / 5);
			} else assert(mask_width == 0);
		}
		strcpy(stext, "\007\005\010\001");
		statusbar(0, 0);
		assert(wifistart == 0 && wifiend == (int)strlen("") * 8);
		strcpy(stext, "\037plain\001");
		assert(statusbar(0, 0) == 42);
		assert(wifistart == -1 && wifiend == -1);
		assert(bluetoothstart == -1 && bluetoothend == -1);
		memset(stext, 'a', sizeof stext - 1);
		stext[sizeof stext - 1] = '\0';
		assert(statusbar(0, 0) == (sizeof stext - 1) * 8 + 2);
	}
	{
		Monitor m = {.wx = 100, .wy = 30, .ww = 1000, .wh = 800,
			.mfact = 0.55, .nmaster = 1, .gappx = 3, .tagset = {1, 1}, .lt = {&layouts[0]}};
		Client clients[3] = {0};
		for (int i = 0; i < 3; i++) {
			clients[i].mon = &m;
			clients[i].tags = 1;
			clients[i].bw = 1;
			clients[i].next = i < 2 ? &clients[i + 1] : NULL;
		}
		m.clients = clients;
		tile(&m);
		assert(clients[0].x == 103 && clients[0].y == 33);
		assert(clients[1].x - clients[0].x - WIDTH(&clients[0]) == 3);
		assert(clients[2].y - clients[1].y - HEIGHT(&clients[1]) == 3);
		assert(m.wx + m.ww - clients[1].x - WIDTH(&clients[1]) == 3);
		assert(m.wy + m.wh - clients[2].y - HEIGHT(&clients[2]) == 3);
		clients[0].next = NULL;
		tile(&m);
		assert(WIDTH(&clients[0]) == 994 && HEIGHT(&clients[0]) == 794);
		m.gappx = 0;
		tile(&m);
		assert(clients[0].x == 100 && WIDTH(&clients[0]) == 1000);
		m.gappx = 3;
		m.nmaster = 0;
		tile(&m);
		assert(clients[0].x == 103 && WIDTH(&clients[0]) == 994);
	}
	{
		Monitor m = {.ww = 1000, .wh = 800, .mfact = 0.55, .nmaster = 1,
			.gappx = 6, .tagset = {1, 1}, .lt = {&layouts[0]}};
		Fnt font = {.h = 22};
		Drw drawing = {.fonts = &font};
		Client clients[4] = {0};
		drw = &drawing;
		for (int i = 0; i < 4; i++) {
			clients[i].mon = &m;
			clients[i].tags = 1;
			clients[i].bw = 1;
			clients[i].next = i < 3 ? &clients[i + 1] : NULL;
		}
		m.clients = clients;
		tile(&m);
		assert(clients[0].tileedges == EdgeRight);
		assert(clients[1].tileedges == (EdgeLeft|EdgeBottom));
		assert(clients[2].tileedges == (EdgeLeft|EdgeTop|EdgeBottom));
		assert(clients[3].tileedges == (EdgeLeft|EdgeTop));
		int width = clients[0].w, height = clients[1].h, lastheight = clients[3].h;
		resizetiled(&clients[0], EdgeRight, 80, 0);
		assert(abs(clients[0].w - width - 80) <= 1);
		assert(clients[1].h == height);
		resizetiled(&clients[1], EdgeBottom, 0, 50);
		assert(abs(clients[1].h - height - 50) <= 1);
		assert(abs(clients[3].h - lastheight) <= 1);
		height = clients[1].h;
		resizetiled(&clients[2], EdgeTop, 0, -30);
		assert(abs(clients[1].h - height + 30) <= 1);
		for (int i = 0; i < 4; i++)
			assert(!clients[i].isfloating);
		m.lt[0] = &layouts[3];
		for (int i = 0; i < 4; i++) clients[i].tileweight = 0;
		bsptile(&m);
		width = clients[0].w;
		resizetiled(&clients[0], EdgeRight, -60, 0);
		assert(abs(clients[0].w - width + 60) <= 1);
		height = clients[1].h;
		resizetiled(&clients[1], EdgeBottom, 0, 40);
		assert(abs(clients[1].h - height - 40) <= 1);
		width = clients[2].w;
		resizetiled(&clients[2], EdgeRight, 30, 0);
		assert(abs(clients[2].w - width - 30) <= 1);
		for (Client *a = m.clients; a; a = a->next) {
			assert(!a->isfloating && a->w > 0 && a->h > 0);
			for (Client *b = a->next; b; b = b->next)
				assert(a->x + WIDTH(a) + 6 <= b->x || b->x + WIDTH(b) + 6 <= a->x
				    || a->y + HEIGHT(a) + 6 <= b->y || b->y + HEIGHT(b) + 6 <= a->y);
		}
		for (int layout = 0; layout <= 3; layout += 3) {
			m.lt[0] = &layouts[layout];
			for (int i = 0; i < 4; i++) clients[i].tileweight = 0;
			arrange(&m);
			for (int repeat = 0; repeat < 4; repeat++)
				for (int i = 0; i < 4; i++)
					for (int edge = EdgeLeft; edge <= EdgeBottom; edge <<= 1) {
						resizetiled(&clients[i], edge, repeat % 2 ? 2000 : -2000,
							repeat % 2 ? 2000 : -2000);
						for (Client *a = m.clients; a; a = a->next) {
							assert(!a->isfloating && a->w > 0 && a->h > 0);
							assert(a->x >= 6 && a->y >= 6);
							assert(a->x + WIDTH(a) <= m.ww - 6 && a->y + HEIGHT(a) <= m.wh - 6);
							for (Client *b = a->next; b; b = b->next)
								assert(a->x + WIDTH(a) + 6 <= b->x || b->x + WIDTH(b) + 6 <= a->x
								    || a->y + HEIGHT(a) + 6 <= b->y || b->y + HEIGHT(b) + 6 <= a->y);
						}
					}
		}
		drw = NULL;
	}
	{
		Monitor m = {.wx = -1200, .wy = 40, .ww = 1200, .wh = 900,
			.mfact = 0.55, .nmaster = 1, .gappx = 6, .tagset = {1, 1}, .lt = {&layouts[3]}};
		Client clients[6] = {0}, newest = {.mon = &m, .tags = 1, .bw = 1};
		int oldwidth, oldheight, found = 0;
		for (int i = 0; i < 6; i++) {
			clients[i].mon = &m;
			clients[i].tags = 1;
			clients[i].bw = 1;
			clients[i].next = i < 5 ? &clients[i + 1] : NULL;
		}
		m.clients = clients;
		bsptile(&m);
		oldwidth = WIDTH(&clients[0]);
		oldheight = HEIGHT(&clients[0]);
		assert(clients[0].x == m.wx + 6 && clients[0].y == m.wy + 6);
		assert(oldheight == m.wh - 12 && oldwidth > m.ww / 2);
		attach(&newest);
		bsptile(&m);
		assert(m.clients == &newest && newest.next == clients);
		assert(newest.x == m.wx + 6 && WIDTH(&newest) == oldwidth);
		assert(HEIGHT(&newest) == oldheight);
		assert(clients[0].x == newest.x + WIDTH(&newest) + 6);
		assert(clients[0].y == m.wy + 6);
		for (Client *a = m.clients; a; a = a->next) {
			assert(a->w > 0 && a->h > 0);
			assert(a->x >= m.wx + 6 && a->y >= m.wy + 6);
			assert(a->x + WIDTH(a) <= m.wx + m.ww - 6);
			assert(a->y + HEIGHT(a) <= m.wy + m.wh - 6);
			for (Client *b = a->next; b; b = b->next)
				assert(a->x + WIDTH(a) + 6 <= b->x || b->x + WIDTH(b) + 6 <= a->x
				    || a->y + HEIGHT(a) + 6 <= b->y || b->y + HEIGHT(b) + 6 <= a->y);
		}
		detach(&newest);
		bsptile(&m);
		assert(WIDTH(&clients[0]) == oldwidth && HEIGHT(&clients[0]) == oldheight);
		m.mfact = 0.65;
		bsptile(&m);
		assert(WIDTH(&clients[0]) > oldwidth);
		clients[1].isfloating = 1;
		clients[2].ishidden = 1;
		clients[3].tags = 2;
		for (int i = 1; i <= 3; i++) clients[i].x = 12345;
		bsptile(&m);
		for (int i = 1; i <= 3; i++) assert(clients[i].x == 12345);
		clients[0].next = NULL;
		bsptile(&m);
		assert(WIDTH(&clients[0]) == m.ww - 12 && HEIGHT(&clients[0]) == m.wh - 12);
		m.gappx = 0;
		bsptile(&m);
		assert(clients[0].x == m.wx && WIDTH(&clients[0]) == m.ww);
		clients[0].next = &clients[4];
		m.ww = 600;
		m.wh = 1000;
		bsptile(&m);
		assert(clients[4].y == clients[0].y + HEIGHT(&clients[0]));
		assert(WIDTH(&clients[0]) == 600 && HEIGHT(&clients[0]) == 650);
		m.nmaster = 0;
		bsptile(&m);
		assert(HEIGHT(&clients[0]) == 500);
		m.ww = 9;
		m.wh = 7;
		m.gappx = 100;
		bsptile(&m);
		assert(clients[0].w > 0 && clients[4].h > 0 && clients[5].w > 0);
		m.clients = NULL;
		bsptile(&m);
		int centerbinding = 0;
		for (size_t i = 0; i < LENGTH(keys); i++)
			if (keys[i].mod == (MODKEY|ShiftMask) && keys[i].keysym == XK_c) {
				assert(keys[i].func == centerfloating);
				centerbinding++;
			}
		assert(centerbinding == 1);
		found = 0;
		for (size_t i = 0; i < LENGTH(keys); i++)
			if (keys[i].mod == (MODKEY|ControlMask) && keys[i].keysym == XK_s) {
				assert(keys[i].func == setlayout && keys[i].arg.v == &layouts[3]);
				found++;
			}
		assert(found == 1 && layouts[0].arrange == tile);
	}

	{
		Monitor m = {.wx = -1000, .wy = 30, .ww = 1000, .wh = 800, .lt = {&layouts[0]}};
		Client c = {.mon = &m, .x = -800, .y = 200, .w = 400, .h = 300,
			.bw = 1, .hintsvalid = 1, .isfloating = 1};
		Arg grow = {.i = 50}, shrink = {.i = -50};
		selmon = mons = &m;
		m.sel = &c;
		bh = 24;
		resizefloating(&grow);
		assert(c.w == 450 && c.h == 350 && c.x == -800 && c.y == 200);
		resizefloating(&shrink);
		assert(c.w == 400 && c.h == 300 && c.x == -800 && c.y == 200);
		{
			unsigned int modifiers[] = {MODKEY, MODKEY, MODKEY};
			KeyCode larger[] = {21, 86, 22}, smaller[] = {20, 82, 20};
			int grouped = launches;
			for (size_t i = 0; i < LENGTH(modifiers); i++) {
				XEvent event = {.xkey = {.state = modifiers[i], .keycode = larger[i]}};
				keypress(&event);
				assert(c.w == 450 && c.h == 350 && c.x == -800 && c.y == 200);
				event.xkey.keycode = smaller[i];
				keypress(&event);
				assert(c.w == 400 && c.h == 300);
			}
			assert(launches == grouped);
		}
		{
			Fnt font = {.h = 24};
			Drw drawing = {.fonts = &font};
			drw = &drawing;
			XEvent event = {.xkey = {.keycode = 65, .state = MODKEY|ControlMask}};
			snapwindow(&(Arg){.i = 5});
			assert(c.snapmode == 5 && c.isfloating);
			keypress(&event);
			assert(!c.isfloating && c.snapmode == 0);
			keypress(&event);
			assert(c.isfloating && c.snapmode == 0);
			resizeclient(&c, -800, 200, 400, 300);
			snapwindow(&(Arg){.i = 5});
			assert(c.snapmode == 5 && c.w == 998 && c.h == 798);
			snapwindow(&(Arg){.i = 5});
			assert(c.snapmode == 0 && c.x == -800 && c.y == 200 && c.w == 400 && c.h == 300);
		}
		/* berry uses pixel sizes even when clients request larger increments. */
		c.incw = c.inch = 64;
		resizefloating(&grow);
		assert(c.w == 450 && c.h == 350);
		resizefloating(&shrink);
		c.incw = c.inch = 0;
		c.isfloating = 0;
		resizefloating(&grow);
		assert(c.w == 400 && c.h == 300);
		m.lt[0] = &layouts[1];
		resizefloating(&grow);
		assert(c.w == 450 && c.h == 350);
		c.isfullscreen = 1;
		resizefloating(&shrink);
		assert(c.w == 450 && c.h == 350);
		c.isfullscreen = 0;
		c.snapmode = 5;
		for (int i = 0; i < 40; i++)
			resizefloating(&grow);
		assert(c.x == -800 && c.y == 200);
		assert(c.x + WIDTH(&c) == m.wx + m.ww && c.y + HEIGHT(&c) == m.wy + m.wh);
		assert(c.snapmode == 0);
		for (int i = 0; i < 40; i++)
			resizefloating(&shrink);
		assert(c.w == 30 && c.h == 30 && c.x == -800 && c.y == 200);
		c.w = 400;
		c.h = 300;
		adjustfloating(&(Arg){.v = (int[]){-50, 0, 0, 0}});
		assert(c.x == -850 && c.y == 200 && c.w == 400 && c.h == 300);
		adjustfloating(&(Arg){.v = (int[]){0, 50, 0, 0}});
		assert(c.x == -850 && c.y == 250);
		adjustfloating(&(Arg){.v = (int[]){0, 0, -50, 0}});
		assert(c.x == -850 && c.y == 250 && c.w == 350 && c.h == 300);
		adjustfloating(&(Arg){.v = (int[]){-2000, -2000, 0, 0}});
		assert(c.x == m.wx && c.y == m.wy);
		adjustfloating(&(Arg){.v = (int[]){2000, 2000, 0, 0}});
		assert(c.x + WIDTH(&c) == m.wx + m.ww && c.y + HEIGHT(&c) == m.wy + m.wh);
		centerfloating(NULL);
		assert(c.x == m.wx + (m.ww - WIDTH(&c)) / 2 && c.y == m.wy + (m.wh - HEIGHT(&c)) / 2);
		{
			Cur pointer = {0};
			Fnt font = {.h = 24};
			Drw drawing = {.fonts = &font};
			drw = &drawing;
			cursor[CurMove] = cursor[CurResize] = &pointer;
			c.isfloating = 1;
			c.x = -800; c.y = 200; c.w = 400; c.h = 300;
			pointerx = -600; pointery = 250;
			motionx = -550; motiony = 270;
			mouseevent = 0;
			resizemouse(NULL);
			assert(c.w == 450 && c.h == 320 && c.x == -800 && c.y == 200);
			assert(pointerwarps == 0 && pointerreleases == 1);
			motionx = -650; motiony = 200;
			mouseevent = 0;
			resizemouse(NULL);
			assert(c.w == 400 && c.h == 270 && c.x == -800 && c.y == 200);
			motionx = -3000; motiony = -3000;
			mouseevent = 0;
			movemouse(NULL);
			assert(c.x == m.wx && c.y == m.wy);
			assert(pointerwarps == 0 && pointerreleases == 3);
			pointervalid = False;
			resizemouse(NULL);
			movemouse(NULL);
			assert(pointerreleases == 5);
			pointervalid = True;
			for (int cycle = 0; cycle < 3; cycle++) {
				resizeclient(&c, -800, 200, 400, 300);
				snapwindow(&(Arg){.i = 5});
				pointerx = -500; pointery = 40;
				motionx = -400; motiony = 200;
				mouseevent = 0;
				movemouse(NULL);
				assert(c.snapmode == 0 && c.w == 400 && c.h == 300);
				assert(c.x > m.wx && c.y > m.wy);
				pointerx = motionx; pointery = motiony;
				motiony = m.wy;
				mouseevent = 0;
				movemouse(NULL);
				assert(c.snapmode == 5 && c.w == 998 && c.h == 798);
				snapwindow(&(Arg){.i = 5});
				assert(c.snapmode == 0 && c.w == 400 && c.h == 300);
			}
			/* saved sizes can already span the screen in either direction. */
			for (int axis = 0; axis < 2; axis++) {
				int width = axis ? 400 : m.ww - 2 * c.bw;
				int height = axis ? m.wh - 2 * c.bw : 300;
				resizeclient(&c, m.wx, m.wy, width, height);
				snapwindow(&(Arg){.i = 5});
				pointerx = -500; pointery = 40;
				motionx = -400; motiony = 200;
				mouseevent = 0;
				movemouse(NULL);
				assert(c.snapmode == 0 && c.w == width && c.h == height);
			}
			c.snapmode = 0;
			c.isfloating = 0;
			m.lt[0] = &layouts[0];
			resizeclient(&c, m.wx, m.wy, m.ww - 2, m.wh - 2);
			pointerx = -500; pointery = 40;
			motionx = -400; motiony = 160;
			mouseevent = 0;
			movemouse(NULL);
			assert(c.isfloating && c.snapmode == 0 && c.w == 750 && c.h == 600);
			assert(c.x > m.wx && c.y > m.wy);
			/* modifier dragging in cherry must not snap at the screen edges. */
			for (size_t layout = 0; layout < LENGTH(layouts); layout++) {
				if (!layouts[layout].arrange)
					continue;
				m.lt[0] = &layouts[layout];
				for (int edge = 0; edge < 3; edge++) {
					resizeclient(&c, -800, 200, 400, 300);
					pointerx = -600; pointery = 250;
					motionx = edge == 0 ? -500 : edge == 1 ? m.wx : m.wx + m.ww - 1;
					motiony = edge == 0 ? m.wy : 300;
					mouseevent = 0;
					movemouse(NULL);
					assert(c.isfloating && !c.snapmode && c.w == 400 && c.h == 300);
				}
			}
			m.lt[0] = &layouts[1];
			m.mx = -1000; m.my = 0; m.mw = 1000; m.mh = 900;
			m.clients = &c;
			for (int cycle = 0; cycle < 3; cycle++) {
				XEvent request = {.xconfigurerequest = {.window = c.win,
					.value_mask = CWX|CWY|CWWidth|CWHeight|CWBorderWidth,
					.x = 20, .y = 30, .width = 400, .height = 300, .border_width = 8}};
				resizeclient(&c, -800, 200, 400, 300);
				fullscreen(NULL);
				assert(c.isfullscreen && c.bw == 0 && c.w == 1000 && c.h == 900);
				configurerequest(&request);
				assert(c.x == -1000 && c.y == 0 && c.w == 1000 && c.h == 900 && c.bw == 0);
				resizeclient(&c, m.mx, m.my, m.mw, m.mh);
				fullscreen(NULL);
				assert(!c.isfullscreen && c.isfloating && c.bw == 1);
				assert(c.x == -800 && c.y == 200 && c.w == 400 && c.h == 300);
			}
			m.clients = NULL;
			c.win = 42;
			c.tags = m.tagset[0] = 1;
			m.lt[0] = &layouts[1];
			m.clients = m.stack = &c;
			c.x = -800; c.y = 200; c.w = 400; c.h = 300;
			for (size_t i = 0; i < LENGTH(edgecursors); i++)
				edgecursors[i] = 100 + i;
			updatehandles(&c);
			assert(c.bw == borderpx && handlecount == 8 && c.handlesmapped);
			for (size_t i = 0; i < LENGTH(c.handles); i++) {
				int edge = 0, *box = handleboxes[i];
				assert(handleclient(c.handles[i], &edge) == &c && edge == resizeedges[i]);
				assert(handlemapped[i]);
				assert(box[0] >= c.x - (int)resizegrip + c.bw && box[1] >= c.y - (int)resizegrip + c.bw);
				assert(box[0] + box[2] <= c.x + WIDTH(&c) + (int)resizegrip - c.bw);
				assert(box[1] + box[3] <= c.y + HEIGHT(&c) + (int)resizegrip - c.bw);
				assert(box[2] >= (int)resizegrip && box[3] >= (int)resizegrip);
				assert(box[0] + box[2] <= c.x + c.bw || box[0] >= c.x + c.bw + c.w ||
				       box[1] + box[3] <= c.y + c.bw || box[1] >= c.y + c.bw + c.h);
			}
			for (size_t i = 0; i < LENGTH(resizeedges); i++) {
				int edge = resizeedges[i];
				c.x = -800; c.y = 200; c.w = 400; c.h = 300;
				pointerx = -800; pointery = 200;
				motionx = pointerx + (edge & EdgeLeft ? 50 : edge & EdgeRight ? -50 : 0);
				motiony = pointery + (edge & EdgeTop ? 40 : edge & EdgeBottom ? -40 : 0);
				mouseevent = 0;
				resizemouse(&(Arg){.i = edge});
				assert(c.w == (edge & (EdgeLeft|EdgeRight) ? 350 : 400));
				assert(c.h == (edge & (EdgeTop|EdgeBottom) ? 260 : 300));
				assert(c.x == (edge & EdgeLeft ? -750 : -800));
				assert(c.y == (edge & EdgeTop ? 240 : 200));
				assert(dragcursor == edgecursors[i] && pointerwarps == 0);
			}
			c.ishidden = 1;
			updatehandles(&c);
			assert(!c.handlesmapped);
			assert(!handleclient(c.handles[0], NULL));
			for (size_t i = 0; i < LENGTH(c.handles); i++) assert(!handlemapped[i]);
			c.ishidden = 0;
			for (size_t layout = 0; layout < LENGTH(layouts); layout++) {
				if (!layouts[layout].arrange)
					continue;
				m.lt[0] = &layouts[layout];
				for (int floating = 0; floating <= 1; floating++) {
					c.isfloating = floating;
					updatehandles(&c);
					assert(c.handlesmapped == floating && c.bw == 1);
					assert(handleclient(c.handles[0], NULL) == (floating ? &c : NULL));
					for (size_t i = 0; i < LENGTH(c.handles); i++) assert(handlemapped[i] == floating);
				}
			}
			m.lt[0] = &layouts[1];
			c.isfloating = 1;
			updatehandles(&c);
			assert(c.handlesmapped && handlecount == 8);
			c.isfullscreen = 1;
			updatehandles(&c);
			assert(!c.handlesmapped && c.bw == 0);
			destroyhandles(&c);
			assert(handledestroyed == 8 && !handleclient(1000, NULL));
		}
		m.sel = NULL;
		resizefloating(&grow);
	}
	{
		Monitor m = {.wx = 0, .wy = 0, .ww = 800, .wh = 600, .tagset = {1, 1}, .lt = {&layouts[0]}};
		Client c = {.win = 42, .x = 10, .y = 20, .w = 300, .h = 232, .bw = 1, .tags = 1, .mon = &m};
		Client group = {.win = 100};
		Client clipboard = {.win = 102, .mon = &m};
		Fnt font = {.h = 22};
		Drw drawing = {.fonts = &font};
		Clr titlecolorset[3] = {[ColBg] = {.pixel = 22}, [ColBorder] = {.pixel = 44}};
		Clr normalcolors[3] = {[ColBorder] = {.pixel = 66}};
		Clr selectedcolors[3] = {[ColBorder] = {.pixel = 88}};
		Clr *frameschemes[LENGTH(colors)] = {0};
		Clr **previousscheme = scheme;
		Cur normal = {0};
		_XPrivDisplay display = calloc(1, sizeof *display);
		Screen screeninfo = {.root_depth = 24};
		display->screens = &screeninfo;
		dpy = (Display *)display;
		drw = &drawing;
		cursor[CurNormal] = &normal;
		titlescheme[0] = titlescheme[1] = titlecolorset;
		frameschemes[SchemeNorm] = normalcolors;
		frameschemes[SchemeSel] = selectedcolors;
		scheme = frameschemes;
		c.depth = 24;
		bh = 24;
		mons = selmon = &m;
		m.clients = m.stack = m.sel = &c;
		strcpy(c.name, "ordinary window");
		rendered[0] = '\0';
		createtitlebar(&group);
		assert(!group.titlebar);
		titles = 0;
		createtitlebar(&c);
		assert(c.titlebar == 2000 && titleheight(&c) == 0);
		titles = 1;
		assert(c.titlebar == 2000 && titleheight(&c) == 32);
		assert(titleclient(2000) == &c && titleclient(0) == NULL);
		resizeclient(&c, 10, 20, 300, 232);
		assert(titlemapped && c.titlemapped);
		assert(titlebox[0] == 10 && titlebox[1] == 20 && titlebox[2] == 300 && titlebox[3] == 232);
		assert(clienty == 52 && clientheight == 200);
		assert(tab_event.xconfigure.y == 52 && tab_event.xconfigure.height == 200);
		assert(titlecleared && !rendered[0]);
		for (int floating = 0; floating <= 1; floating++) {
			m.lt[0] = &layouts[floating];
			for (int selected = 0; selected <= 1; selected++) {
				m.sel = selected ? &c : NULL;
				c.depth = selected ? 32 : 24;
				drawtitlebar(&c);
				/* the server can destroy a client before its event is handled. */
				destroyedclient = c.win;
				drawtitlebar(&c);
				destroyedclient = None;
				assert(frameborder == 44 && borderdepth == c.depth);
				assert(borderheight == 202 && borderrows[201] == 22);
				for (int y = 0; y <= 200; y++)
					assert(borderrows[y] == (selected ? 88 : 66));
			}
		}
		handlecount = 0;
		for (size_t layout = 0; layout < LENGTH(layouts); layout++) {
			m.lt[0] = &layouts[layout];
			for (int state = 0; state < 4; state++) {
				c.ishidden = state == 1;
				c.isfullscreen = state == 2;
				c.isfloating = state == 3;
				c.bw = c.isfullscreen ? 0 : 1;
				resizeclient(&c, 10, 20, 300, 232);
				toggletitles(NULL);
				assert(!titles && !c.titlemapped && !titlemapped);
				assert(titleheight(&c) == 0 && clienty == 20 && clientheight == 232);
				assert(clientx == (c.ishidden ? -2 * WIDTH(&c) : 10));
				assert(c.frameextent == 2 * c.bw);
				toggletitles(NULL);
				assert(titles && c.titlemapped == (!c.ishidden && !c.isfullscreen));
				assert(clienty == 20 + titleheight(&c));
				assert(clientheight == 232 - titleheight(&c));
				assert(clientx == (c.ishidden ? -2 * WIDTH(&c) : 10));
				assert(c.frameextent == titleheight(&c) + 2 * c.bw);
				assert(c.x == 10 && c.y == 20 && c.w == 300 && c.h == 232);
			}
		}
		c.isfloating = c.isfullscreen = c.ishidden = 0;
		m.lt[0] = &layouts[0];
		c.ishidden = 1;
		updatehandles(&c);
		assert(!titlemapped && !c.titlemapped);
		c.ishidden = 0;
		updatehandles(&c);
		assert(titlemapped);
		c.isfullscreen = 1;
		resizeclient(&c, 0, 0, 800, 600);
		assert(!titlemapped && titleheight(&c) == 0 && clienty == 0 && clientheight == 600);
		c.isfullscreen = 0;
		resizeclient(&c, 10, 20, 300, 232);
		assert(titlemapped && clienty == 52 && clientheight == 200);
		/* title-bar dragging applies to floating clients in every cherry layout. */
		handlecount = 0;
		c.neverfocus = 1;
		m.sel = &c;
		cursor[CurMove] = &normal;
		for (size_t layout = 0; layout < LENGTH(layouts); layout++) {
			m.lt[0] = &layouts[layout];
			for (int floating = 0; floating <= 1; floating++) {
				XEvent click = {.xbutton = {.window = c.titlebar, .button = Button1}};
				int releases = pointerreleases;
				c.isfloating = floating;
				resizeclient(&c, 10, 20, 300, 232);
				pointerx = 100; pointery = 30;
				motionx = 150; motiony = 90;
				mouseevent = 0;
				buttonpress(&click);
				if (floating || !layouts[layout].arrange) {
					assert(c.x == 60 && c.y == 80 && pointerreleases == releases + 1);
				} else {
					assert(c.x == 10 && c.y == 20 && pointerreleases == releases);
				}
				click.xbutton.window = c.win;
				raisedwindow = None;
				int replayed = replayedclicks;
				buttonpress(&click);
				assert(replayedclicks == replayed + 1);
				assert(raisedwindow == c.win);
			}
		}
		m.lt[0] = &layouts[1];
		cursor[CurResize] = &normal;
		c.isfloating = 0;
		for (int visible = 0; visible <= 1; visible++) {
			if (titles != visible)
				toggletitles(NULL);
			for (size_t i = 0; i < LENGTH(resizeedges); i++) {
				int edge = resizeedges[i];
				resizeclient(&c, m.wx, m.wy, m.ww - 2 * c.bw, m.wh - 2 * c.bw);
				for (size_t j = 0; j < LENGTH(c.handles); j++) {
					int *box = handleboxes[j];
					assert(box[0] >= m.wx && box[1] >= m.wy);
					assert(box[0] + box[2] <= m.wx + m.ww);
					assert(box[1] + box[3] <= m.wy + m.wh);
					assert(box[2] >= (int)resizegrip && box[3] >= (int)resizegrip);
				}
				pointerx = m.wx + m.ww / 2;
				pointery = m.wy + m.wh / 2;
				motionx = pointerx + (edge & EdgeLeft ? 50 : edge & EdgeRight ? -50 : 0);
				motiony = pointery + (edge & EdgeTop ? 40 : edge & EdgeBottom ? -40 : 0);
				mouseevent = 0;
				XEvent click = {.xbutton = {.window = c.handles[i], .button = Button1}};
				buttonpress(&click);
				assert(c.w == m.ww - 2 * c.bw - (edge & (EdgeLeft|EdgeRight) ? 50 : 0));
				assert(c.h == m.wh - 2 * c.bw - (edge & (EdgeTop|EdgeBottom) ? 40 : 0));
				assert(dragcursor == edgecursors[i] && raisedwindow == c.win);
			}
		}
		Client neighbour = {.mon = &m, .tags = 1, .bw = 1};
		m.lt[0] = &layouts[0];
		m.mfact = 0.55;
		m.nmaster = 1;
		c.isfloating = 0;
		for (int visible = 0; visible <= 1; visible++) {
			c.next = NULL;
			if (titles != visible)
				toggletitles(NULL);
			c.next = &neighbour;
			tile(&m);
			assert(c.handlesmapped && c.handlemask == (1u << 1));
			for (size_t i = 0; i < LENGTH(c.handles); i++) {
				assert(handlemapped[i] == (i == 1));
				assert(handleclient(c.handles[i], NULL) == (i == 1 ? &c : NULL));
			}
			int width = c.w;
			pointerx = c.x + WIDTH(&c);
			pointery = motiony = c.y + c.h / 2;
			motionx = pointerx + 40;
			mouseevent = 0;
			XEvent click = {.xbutton = {.window = c.handles[1], .button = Button1}};
			buttonpress(&click);
			assert(!c.isfloating && !neighbour.isfloating);
			assert(abs(c.w - width - 40) <= 1);
		}
		m.lt[0] = &layouts[2];
		updatehandles(&c);
		assert(!c.handlesmapped && !c.handlemask);
		c.next = NULL;
		m.lt[0] = &layouts[0];
		c.isfloating = 1;
		resizeclient(&c, 10, 20, 300, 232);
		/* shrinking from the top keeps the bottom fixed, including the blank bar. */
		handlecount = 0;
		cursor[CurResize] = &normal;
		pointerx = motionx = 100;
		pointery = 20; motiony = 500;
		mouseevent = 0;
		resizemouse(&(Arg){.i = EdgeTop});
		assert(c.y + c.h == 252 && c.h == 62 && clientheight == 30);
		assert(borderheight == 32 && borderrows[31] == 22 && borderrows[30] == 88);
		/* the final pointer position still applies when motion events are throttled. */
		pointery = motiony = 0;
		pointerx = motionx = 100;
		releasex = 15;
		mouseevent = 0;
		resizemouse(&(Arg){.i = EdgeRight});
		assert(c.w == 315);
		releasex = 0;
		applyrules(&clipboard);
		assert(clipboard.isfloating);
		scheme = previousscheme;
		free(display);
		dpy = NULL;
	}
	{
		Monitor second = {.lt = {&layouts[0], &layouts[1]}};
		Monitor first = {.lt = {&layouts[0], &layouts[1]}, .next = &second};
		int floatingkey = 0, tiledkey = 0;
		Fnt font = {.h = 22};
		Drw drawing = {.fonts = &font};

		drw = &drawing;
		mons = selmon = &first;
		layoutpath[0] = '\0';
		for (size_t i = 0; i < LENGTH(keys); i++) {
			if (keys[i].mod == (MODKEY|ShiftMask) && keys[i].func == setmode) {
				floatingkey |= keys[i].keysym == XK_f && keys[i].arg.i == 1;
				tiledkey |= keys[i].keysym == XK_t && keys[i].arg.i == 0;
			}
		}
		assert(floatingkey && tiledkey);
		setlayout(&(Arg){.v = &layouts[3]});
		setmode(&(Arg){.i = 1});
		setmode(&(Arg){.i = 1});
		assert(first.lt[first.sellt] == &layouts[1] && second.lt[second.sellt] == &layouts[1]);
		setmode(&(Arg){.i = 0});
		setmode(&(Arg){.i = 0});
		assert(first.lt[first.sellt] == &layouts[3] && second.lt[second.sellt] == &layouts[3]);
	}
	{
		Monitor m = {.wx = -1000, .wy = 30, .ww = 1000, .wh = 800,
			.tagset = {1, 1}, .lt = {&layouts[0], &layouts[1]}};
		Client dialog = {.mon = &m, .tags = 1, .isfloating = 1, .alwaysfloating = 1,
			.x = -1000, .y = 30, .w = 100, .h = 100};
		Client c = {.mon = &m, .tags = 1, .x = -900, .y = 100, .w = 980, .h = 780,
			.floatw = 300, .floath = 200, .next = &dialog};
		Client added = {.mon = &m, .tags = 1, .w = 200, .h = 100};
		mons = selmon = &m;
		m.clients = &c;
		pointerx = -800; pointery = 200;
		switchlayout(&m, &layouts[1]);
		assert(c.w == 300 && c.h == 200 && c.floatvalid);
		assert(c.x >= m.wx && c.x + c.w <= m.wx + m.ww);
		assert(c.y >= m.wy && c.y + c.h <= m.wy + m.wh);
		placefloating(&added);
		assert(c.x == -800 && c.y == 200);
		assert(added.x == c.x + 32 && added.y == c.y + 32);
		resizefloatingclient(&c, -700, 130, 400, 250);
		c.isfloating = 1;
		switchlayout(&m, &layouts[0]);
		assert(!c.isfloating && dialog.isfloating);
		assert(c.floatx == -700 && c.floaty == 130 && c.floatw == 400 && c.floath == 250);
		c.x = -1000; c.y = 30; c.w = 1000; c.h = 800;
		switchlayout(&m, &layouts[1]);
		assert(c.x == -700 && c.y == 130 && c.w == 400 && c.h == 250);
		c.isfullscreen = 1; c.isfloating = 1; c.oldstate = 1;
		c.fsx = -600; c.fsy = 200; c.fsw = 350; c.fsh = 240;
		switchlayout(&m, &layouts[0]);
		assert(c.isfullscreen && !c.oldstate);
		assert(c.floatx == -600 && c.floaty == 200 && c.floatw == 350 && c.floath == 240);
		m.clients = NULL;
		pointervalid = False;
		placefloating(&added);
		assert(added.x == -600 && added.y == 380);
		pointervalid = True;
	}
	{
		Monitor m = {.ww = 1000, .wh = 800, .tagset = {1, 1}, .lt = {&layouts[1]}};
		Client crowded[4] = {0};
		Client added = {.mon = &m, .tags = 1, .w = 200, .h = 150, .bw = 1};

		pointerx = 100; pointery = 150;
		for (int i = 0; i < 4; i++) {
			Client *c = &crowded[i];
			c->mon = &m; c->tags = 1; c->w = 400; c->h = 300;
			placefloating(c);
			assert(c->x == pointerx + i * 32 && c->y == pointery + i * 32);
			c->next = m.clients;
			m.clients = c;
		}
		pointerx = 500; pointery = 400;
		placefloating(&added);
		assert(added.x == 500 && added.y == 400);
		for (int i = 0; i < 4; i++)
			crowded[i].ishidden = 1;
		pointerx = 100; pointery = 150;
		placefloating(&added);
		assert(added.x == 100 && added.y == 150);
		crowded[0].ishidden = 0; crowded[0].tags = 2;
		placefloating(&added);
		assert(added.x == 100 && added.y == 150);
		pointerx = 999; pointery = 799;
		placefloating(&added);
		assert(added.x + WIDTH(&added) == m.ww);
		assert(added.y + HEIGHT(&added) == m.wh);
		crowded[0] = added;
		crowded[0].next = NULL;
		m.clients = &crowded[0];
		placefloating(&added);
		assert(added.x == crowded[0].x - 32 && added.y == crowded[0].y - 32);
	}
	{
		Monitor m = {.ww = 1920, .wh = 1080, .mw = 1920, .mh = 1080,
			.tagset = {1, 1}, .lt = {&layouts[0]}};
		Client c = {.win = 103, .mon = &m, .tags = 1, .bw = 1, .titlebar = 2000,
			.w = 1920, .h = 1112};
		XEvent request = {.xconfigurerequest = {.value_mask = CWWidth|CWHeight,
			.width = 3840, .height = 2160}};

		mons = selmon = &m;
		applyrules(&c);
		assert(c.ismpv && c.isfloating);
		placefloating(&c);
		assert(WIDTH(&c) <= 960 && HEIGHT(&c) <= 540);
		assert(abs(c.w * 1080 - (c.h - titleheight(&c)) * 1920) < 1920);
		c.w = 320; c.h = 212;
		fitfloating(&c);
		assert(c.w == 320 && c.h == 212);
		/* late video dimensions must not undo the initial size cap. */
		c.win = c.titlebar = 0;
		c.ishidden = 1;
		m.clients = &c;
		configurerequest(&request);
		assert(WIDTH(&c) <= 960 && HEIGHT(&c) <= 540);
		c.isfullscreen = 1;
		c.w = 1920; c.h = 1080;
		configurerequest(&request);
		assert(c.w == 1920 && c.h == 1080);
	}
	{
		char directory[] = "/tmp/dwm-layout-test-XXXXXX", state[PATH_MAX];
		const char *value = getenv("XDG_STATE_HOME");
		char *previous = value ? strdup(value) : NULL;
		Monitor *m;
		FILE *file;

		assert(mkdtemp(directory));
		assert(setenv("XDG_STATE_HOME", directory, 1) == 0);
		loadlayout();
		assert(defaultlayout == 0 && tiledlayout == 0);
		defaultlayout = 1; tiledlayout = 3;
		savelayout();
		loadlayout();
		assert(defaultlayout == 1 && tiledlayout == 3);
		m = createmon();
		assert(m->lt[m->sellt] == &layouts[1]);
		free(m);
		file = fopen(layoutpath, "w");
		assert(file);
		fputs("1 1\n", file);
		fclose(file);
		loadlayout();
		assert(defaultlayout == 0 && tiledlayout == 0);
		assert(unlink(layoutpath) == 0);
		snprintf(state, sizeof state, "%s/dwm", directory);
		assert(rmdir(state) == 0 && rmdir(directory) == 0);
		if (previous) {
			setenv("XDG_STATE_HOME", previous, 1);
			free(previous);
		} else {
			unsetenv("XDG_STATE_HOME");
		}
		layoutpath[0] = '\0';
	}
	puts("persistent global layout modes, pointer-based floating placement and mpv sizing: passed (x calls mocked)");
	puts("input, focus, logout, tab grouping, status, fullgaps, title bars, clipboard rules and floating resize: passed (x calls mocked)");
	return 0;
}
