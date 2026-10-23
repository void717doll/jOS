/* See LICENSE file for copyright and license details.
 *
 * dynamic window manager is designed like any other X client as well. It is
 * driven through handling X events. In contrast to other X clients, a window
 * manager selects for SubstructureRedirectMask on the root window, to receive
 * events about window (dis-)appearance. Only one X connection at a time is
 * allowed to select for this event mask.
 *
 * The event handlers of dwm are organized in an array which is accessed
 * whenever a new event has been fetched. This allows event dispatching
 * in O(1) time.
 *
 * Each child of the root window is called a client, except windows which have
 * set the override_redirect flag. Clients are organized in a linked client
 * list on each monitor, the focus history is remembered through a stack list
 * on each monitor. Each client contains a bit array to indicate the tags of a
 * client.
 *
 * Keys and tagging rules are organized as arrays and defined in config.h.
 *
 * To understand everything else, start reading main().
 */
#include <errno.h>
#include <locale.h>
#include <limits.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <X11/cursorfont.h>
#include <X11/keysym.h>
#include <X11/XF86keysym.h>
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xproto.h>
#include <X11/Xutil.h>
#ifdef XINERAMA
#include <X11/extensions/Xinerama.h>
#endif /* XINERAMA */
#include <X11/Xft/Xft.h>

#include "drw.h"
#include "util.h"

/* macros */
#define BUTTONMASK              (ButtonPressMask|ButtonReleaseMask)
#define CLEANMASK(mask)         (mask & ~(numlockmask|LockMask) & (ShiftMask|ControlMask|Mod1Mask|Mod2Mask|Mod3Mask|Mod4Mask|Mod5Mask))
#define INTERSECT(x,y,w,h,m)    (MAX(0, MIN((x)+(w),(m)->wx+(m)->ww) - MAX((x),(m)->wx)) \
				 * MAX(0, MIN((y)+(h),(m)->wy+(m)->wh) - MAX((y),(m)->wy)))
#define ISTAGVISIBLE(C)         ((C)->tags & (C)->mon->tagset[(C)->mon->seltags])
#define ISVISIBLE(C)            (ISTAGVISIBLE(C) && !(C)->ishidden && !(C)->mon->showdesktop)
#define MOUSEMASK               (BUTTONMASK|PointerMotionMask)
#define WIDTH(X)                ((X)->w + 2 * (X)->bw)
#define HEIGHT(X)               ((X)->h + 2 * (X)->bw)
#define TAGMASK                 ((1 << LENGTH(tags)) - 1)
#define TEXTW(X)                (drw_fontset_getwidth(drw, (X)) + lrpad)

#define SYSTEM_TRAY_REQUEST_DOCK 0
#define XEMBED_EMBEDDED_NOTIFY 0
#define XEMBED_WINDOW_ACTIVATE 1
#define XEMBED_WINDOW_DEACTIVATE 2
#define XEMBED_MAPPED (1 << 0)
#define XEMBED_EMBEDDED_VERSION 0

/* enums */
enum { CurNormal, CurResize, CurMove, CurLast }; /* cursor */
enum { EdgeLeft = 1, EdgeRight = 2, EdgeTop = 4, EdgeBottom = 8 };
enum { SchemeNorm, SchemeSel, SchemeWarn, SchemeCritical, SchemeGood, SchemeMuted, SchemeYellow }; /* color schemes */
enum { NetSupported, NetWMName, NetWMState, NetWMCheck,
	NetSystemTray, NetSystemTrayOP, NetSystemTrayOrientation,
	NetWMFullscreen, NetActiveWindow, NetWMWindowType,
	NetWMWindowTypeDialog, NetClientList, NetWMFrameExtents, NetLast }; /* EWMH atoms */
enum { Manager, Xembed, XembedInfo, XLast };
enum { WMProtocols, WMDelete, WMState, WMTakeFocus, WMLast }; /* default atoms */
enum { ClkTagBar, ClkLtSymbol, ClkStatusText, ClkWifi, ClkBluetooth, ClkWeather, ClkClock, ClkWinTitle,
	ClkClientWin, ClkRootWin, ClkLast }; /* clicks */

typedef union {
	int i;
	unsigned int ui;
	float f;
	const void *v;
} Arg;

typedef struct {
	unsigned int click;
	unsigned int mask;
	unsigned int button;
	void (*func)(const Arg *arg);
	const Arg arg;
} Button;

typedef struct Monitor Monitor;
typedef struct Client Client;
struct Client {
	char name[256];
	float mina, maxa;
	float tileweight;
	int tileedges;
	int x, y, w, h;
	int oldx, oldy, oldw, oldh;
	int fsx, fsy, fsw, fsh;
	int basew, baseh, incw, inch, maxw, maxh, minw, minh, hintsvalid;
	int bw, oldbw, depth;
	int ishidden, snapmode, savedfloating;
	int savedx, savedy, savedw, savedh;
	int floatx, floaty, floatw, floath, floatvalid, alwaysfloating, ismpv;
	int titlex, titlew;
	unsigned int tags;
	int isfixed, isfloating, isurgent, neverfocus, oldstate, isfullscreen;
	Client *next;
	Client *snext;
	Monitor *mon;
	Window win, titlebar;
	int titlemapped, frameextent;
	Window handles[8];
	int handlesmapped;
	unsigned int handlemask;
};

typedef struct {
	unsigned int mod;
	KeySym keysym;
	void (*func)(const Arg *);
	const Arg arg;
} Key;

typedef struct {
	const char *symbol;
	void (*arrange)(Monitor *);
} Layout;

struct Monitor {
	char ltsymbol[32];
	float mfact;
	int nmaster;
	int num;
	int by;               /* bar geometry */
	int mx, my, mw, mh;   /* screen size */
	int wx, wy, ww, wh;   /* window area  */
	int gappx;
	unsigned int seltags;
	unsigned int sellt;
	unsigned int tagset[2];
	int showbar;
	int showdesktop;
	int topbar;
	Client *clients;
	Client *sel;
	Client *stack;
	Monitor *next;
	Window barwin;
	const Layout *lt[2];
};

typedef struct {
	const char *class;
	const char *instance;
	const char *title;
	unsigned int tags;
	int isfloating;
	int monitor;
} Rule;

typedef struct {
	Window win;
	Client *icons;
} Systray;

/* function declarations */
static int titleheight(Client *c);
static Client *titleclient(Window w);
static void createtitlebar(Client *c);
static void drawtitlebar(Client *c);
static void updatetitlebar(Client *c);
static void toggletitles(const Arg *arg);
static void initsystray(void);
static void cleanupsystray(void);
static void removesystrayicon(Client *i);
static void resizebarwin(Monitor *m);
static void resizerequest(XEvent *e);
static float tileweight(Client *c);
static float tileweights(Client *c, int n);
static void resizetiled(Client *c, int edges, int dx, int dy);
static void updatesystray(void);
static void updatesystrayicongeom(Client *i, int w, int h);
static void updatesystrayiconstate(Client *i, XPropertyEvent *ev);
static Client *wintosystrayicon(Window w);
static void selectionclear(XEvent *e);
static void reparentnotify(XEvent *e);
static void cycle_flycolors(const Arg *arg);
static void update_scheme(void);
static void alttab(const Arg *arg);
static void grouptabs(const Arg *arg);
static void ungrouptab(const Arg *arg);
static void endalttab(int accept);
static void drawalttab(void);
static void keyrelease(XEvent *e);
static void snapwindow(const Arg *arg);
static void minimize(const Arg *arg);
static void restore(const Arg *arg);
static void showdesktop(const Arg *arg);
static void focusdirection(const Arg *arg);
static void movedesktop(const Arg *arg);
static void fullscreen(const Arg *arg);
static void applyrules(Client *c);
static int applysizehints(Client *c, int *x, int *y, int *w, int *h, int interact);
static void arrange(Monitor *m);
static void arrangemon(Monitor *m);
static void attach(Client *c);
static void attachstack(Client *c);
static void bsptile(Monitor *m);
static void buttonpress(XEvent *e);
static void checkotherwm(void);
static void cleanup(void);
static void cleanupmon(Monitor *mon);
static void clientmessage(XEvent *e);
static void configure(Client *c);
static void configurenotify(XEvent *e);
static void configurerequest(XEvent *e);
static Monitor *createmon(void);
static void destroynotify(XEvent *e);
static void detach(Client *c);
static void detachstack(Client *c);
static Monitor *dirtomon(int dir);
static void drawbar(Monitor *m);
static int statusbar(int x, int draw);
static void drawbars(void);
static void enternotify(XEvent *e);
static void expose(XEvent *e);
static void focus(Client *c);
static void focusin(XEvent *e);
static void focusmon(const Arg *arg);
static void focusstack(const Arg *arg);
static Atom getatomprop(Client *c, Atom prop);
static int getrootptr(int *x, int *y);
static long getstate(Window w);
static unsigned int getsystraywidth(void);
static int gettextprop(Window w, Atom atom, char *text, unsigned int size);
static void grabbuttons(Client *c);
static void grabkeys(void);
static void incnmaster(const Arg *arg);
static void keypress(XEvent *e);
static void killclient(const Arg *arg);
static void manage(Window w, XWindowAttributes *wa);
static void mappingnotify(XEvent *e);
static void maprequest(XEvent *e);
static void monocle(Monitor *m);
static void motionnotify(XEvent *e);
static void movemouse(const Arg *arg);
static Client *nexttiled(Client *c);
static void pop(Client *c);
static void propertynotify(XEvent *e);
static void quit(const Arg *arg);
static Monitor *recttomon(int x, int y, int w, int h);
static void resize(Client *c, int x, int y, int w, int h, int interact);
static void resizeclient(Client *c, int x, int y, int w, int h);
static void resizefloating(const Arg *arg);
static void resizefloatingclient(Client *c, int x, int y, int w, int h);
static void updatehandles(Client *c);
static void destroyhandles(Client *c);
static Client *handleclient(Window window, int *edge);
static void adjustfloating(const Arg *arg);
static void centerfloating(const Arg *arg);
static void resizemouse(const Arg *arg);
static void restack(Monitor *m);
static void run(void);
static void scan(void);
static int sendevent(Window w, Atom proto, int mask, long d0, long d1, long d2, long d3, long d4);
static void sendmon(Client *c, Monitor *m);
static void setclientstate(Client *c, long state);
static void setfocus(Client *c);
static void setfullscreen(Client *c, int fullscreen);
static void setlayout(const Arg *arg);
static void setmode(const Arg *arg);
static void setmfact(const Arg *arg);
static void setgaps(const Arg *arg);
static void setup(void);
static void seturgent(Client *c, int urg);
static void showhide(Client *c);
static void spawn(const Arg *arg);
static Monitor *systraytomon(Monitor *m);
static void tag(const Arg *arg);
static void tagmon(const Arg *arg);
static void tile(Monitor *m);
static void togglebar(const Arg *arg);
static void togglefloating(const Arg *arg);
static void toggletag(const Arg *arg);
static void toggleview(const Arg *arg);
static void unfocus(Client *c, int setfocus);
static void unmanage(Client *c, int destroyed);
static void unmapnotify(XEvent *e);
static void updatebarpos(Monitor *m);
static void updatebars(void);
static void updateclientlist(void);
static int updategeom(void);
static void updatenumlockmask(void);
static void updatesizehints(Client *c);
static void updatestatus(void);
static void updatetitle(Client *c);
static void updatewindowtype(Client *c);
static void updatewmhints(Client *c);
static void view(const Arg *arg);
static Client *wintoclient(Window w);
static Monitor *wintomon(Window w);
static int xerror(Display *dpy, XErrorEvent *ee);
static int xerrordummy(Display *dpy, XErrorEvent *ee);
static int xerrorstart(Display *dpy, XErrorEvent *ee);
static void zoom(const Arg *arg);

/* variables */
static Systray *systray;
static unsigned int iflycol = 0;
static Window *tablist, tabwin;
static int tabcount, tabindex, tabmode;
static const char broken[] = "broken";
static char stext[256];
static int wifistart = -1, wifiend = -1;
static int bluetoothstart = -1, bluetoothend = -1;
static int weatherstart = -1, weatherend = -1;
static int clockstart = -1, clockend = -1;
static int screen;
static int sw, sh;           /* X display screen geometry width, height */
static int bh;               /* bar height */
static int lrpad;            /* sum of left and right padding for text */
static int (*xerrorxlib)(Display *, XErrorEvent *);
static unsigned int numlockmask = 0;
static void (*handler[LASTEvent]) (XEvent *) = {
	[ButtonPress] = buttonpress,
	[ClientMessage] = clientmessage,
	[ConfigureRequest] = configurerequest,
	[ConfigureNotify] = configurenotify,
	[DestroyNotify] = destroynotify,
	[EnterNotify] = enternotify,
	[Expose] = expose,
	[FocusIn] = focusin,
	[KeyPress] = keypress,
	[KeyRelease] = keyrelease,
	[MappingNotify] = mappingnotify,
	[MapRequest] = maprequest,
	[MotionNotify] = motionnotify,
	[PropertyNotify] = propertynotify,
	[ResizeRequest] = resizerequest,
	[SelectionClear] = selectionclear,
	[ReparentNotify] = reparentnotify,
	[UnmapNotify] = unmapnotify
};
static Atom wmatom[WMLast], netatom[NetLast], xatom[XLast], logoutatom;
static int running = 1;
static Cur *cursor[CurLast];
static Cursor edgecursors[8];
static const int resizeedges[] = {EdgeLeft, EdgeRight, EdgeTop, EdgeBottom,
	EdgeLeft|EdgeTop, EdgeRight|EdgeTop, EdgeLeft|EdgeBottom, EdgeRight|EdgeBottom};
static Clr **scheme;
static Clr *titlescheme[2];
static Display *dpy;
static Drw *drw;
static Monitor *mons, *selmon;
static int defaultlayout, tiledlayout;
static char layoutpath[PATH_MAX];
static Window root, wmcheckwin;

/* configuration, allows nested code to access above variables */
#include "config.h"
#include "desktop.c"

/* compile-time check if all tags fit into an unsigned int bit array. */
struct NumTags { char limitexceeded[LENGTH(tags) > 31 ? -1 : 1]; };

/* function implementations */
void
applyrules(Client *c)
{
	const char *class, *instance;
	unsigned int i;
	const Rule *r;
	Monitor *m;
	XClassHint ch = { NULL, NULL };

	/* rule matching */
	c->isfloating = 0;
	c->tags = 0;
	XGetClassHint(dpy, c->win, &ch);
	class    = ch.res_class ? ch.res_class : broken;
	instance = ch.res_name  ? ch.res_name  : broken;
	c->ismpv = !strcmp(class, "mpv") || !strcmp(class, "Mpv") || !strcmp(instance, "mpv");

	for (i = 0; i < LENGTH(rules); i++) {
		r = &rules[i];
		if ((!r->title || strstr(c->name, r->title))
		    && (!r->class || strstr(class, r->class))
		    && (!r->instance || strstr(instance, r->instance)))
		{
			c->isfloating = r->isfloating;
			c->tags |= r->tags;
			for (m = mons; m && m->num != r->monitor; m = m->next);
			if (m)
				c->mon = m;
		}
	}
	if (ch.res_class)
		XFree(ch.res_class);
	if (ch.res_name)
		XFree(ch.res_name);
	c->tags = c->tags & TAGMASK ? c->tags & TAGMASK : c->mon->tagset[c->mon->seltags];
}

int
applysizehints(Client *c, int *x, int *y, int *w, int *h, int interact)
{
	int baseismin;
	Monitor *m = c->mon;

	/* set minimum possible */
	*w = MAX(1, *w);
	*h = MAX(1, *h);
	if (interact) {
		if (*x > sw)
			*x = sw - WIDTH(c);
		if (*y > sh)
			*y = sh - HEIGHT(c);
		if (*x + *w + 2 * c->bw < 0)
			*x = 0;
		if (*y + *h + 2 * c->bw < 0)
			*y = 0;
	} else {
		if (*x >= m->wx + m->ww)
			*x = m->wx + m->ww - WIDTH(c);
		if (*y >= m->wy + m->wh)
			*y = m->wy + m->wh - HEIGHT(c);
		if (*x + *w + 2 * c->bw <= m->wx)
			*x = m->wx;
		if (*y + *h + 2 * c->bw <= m->wy)
			*y = m->wy;
	}
	if (*h < bh)
		*h = bh;
	if (*w < bh)
		*w = bh;
	*h = MAX(1, *h - titleheight(c));
	if (resizehints || c->isfloating || !c->mon->lt[c->mon->sellt]->arrange) {
		if (!c->hintsvalid)
			updatesizehints(c);
		/* see last two sentences in ICCCM 4.1.2.3 */
		baseismin = c->basew == c->minw && c->baseh == c->minh;
		if (!baseismin) { /* temporarily remove base dimensions */
			*w -= c->basew;
			*h -= c->baseh;
		}
		/* adjust for aspect limits */
		if (c->mina > 0 && c->maxa > 0) {
			if (c->maxa < (float)*w / *h)
				*w = *h * c->maxa + 0.5;
			else if (c->mina < (float)*h / *w)
				*h = *w * c->mina + 0.5;
		}
		if (baseismin) { /* increment calculation requires this */
			*w -= c->basew;
			*h -= c->baseh;
		}
		/* adjust for increment value */
		if (c->incw)
			*w -= *w % c->incw;
		if (c->inch)
			*h -= *h % c->inch;
		/* restore base dimensions */
		*w = MAX(*w + c->basew, c->minw);
		*h = MAX(*h + c->baseh, c->minh);
		if (c->maxw)
			*w = MIN(*w, c->maxw);
		if (c->maxh)
			*h = MIN(*h, c->maxh);
	}
	*h += titleheight(c);
	return *x != c->x || *y != c->y || *w != c->w || *h != c->h;
}

void
arrange(Monitor *m)
{
	if (m)
		showhide(m->stack);
	else for (m = mons; m; m = m->next)
		showhide(m->stack);
	if (m) {
		arrangemon(m);
		restack(m);
	} else for (m = mons; m; m = m->next)
		arrangemon(m);
}

void
arrangemon(Monitor *m)
{
	strncpy(m->ltsymbol, m->lt[m->sellt]->symbol, sizeof m->ltsymbol);
	if (m->lt[m->sellt]->arrange)
		m->lt[m->sellt]->arrange(m);
}

void
attach(Client *c)
{
	c->next = c->mon->clients;
	c->mon->clients = c;
}

void
attachstack(Client *c)
{
	c->snext = c->mon->stack;
	c->mon->stack = c;
}

void
buttonpress(XEvent *e)
{
	unsigned int i, x, click;
	int edge, statusx;
	Arg arg = {0};
	Client *c;
	Monitor *m;
	XButtonPressedEvent *ev = &e->xbutton;

	if (tabwin)
		endalttab(0);
	if ((c = handleclient(ev->window, &edge))) {
		focus(c);
		restack(c->mon);
		if (ev->button == Button1)
			resizemouse(&(Arg){.i = edge});
		return;
	}
	if ((c = titleclient(ev->window))) {
		focus(c);
		restack(c->mon);
		if (ev->button == Button1 && (c->isfloating || !c->mon->lt[c->mon->sellt]->arrange))
			movemouse(NULL);
		return;
	}
	click = ClkRootWin;
	/* focus monitor if necessary */
	if ((m = wintomon(ev->window)) && m != selmon) {
		unfocus(selmon->sel, 1);
		selmon = m;
		focus(NULL);
	}
	if (ev->window == selmon->barwin) {
		statusx = selmon->ww - statusbar(0, 0)
			- (selmon == systraytomon(NULL) ? (int)getsystraywidth() : 0);
		i = x = 0;
		do
			x += TEXTW(tags[i]);
		while (ev->x >= x && ++i < LENGTH(tags));
		if (i < LENGTH(tags)) {
			click = ClkTagBar;
			arg.ui = 1 << i;
		} else if (ev->x >= x && ev->x < x + TEXTW(selmon->ltsymbol))
			click = ClkLtSymbol;
		else if (ev->x >= statusx) {
			click = ClkStatusText;
			if (wifistart >= 0 && ev->x >= statusx + wifistart && ev->x < statusx + wifiend)
				click = ClkWifi;
			else if (bluetoothstart >= 0 && ev->x >= statusx + bluetoothstart && ev->x < statusx + bluetoothend)
				click = ClkBluetooth;
			else if (weatherstart >= 0 && ev->x >= statusx + weatherstart && ev->x < statusx + weatherend)
				click = ClkWeather;
			else if (clockstart >= 0 && ev->x >= statusx + clockstart && ev->x < statusx + clockend)
				click = ClkClock;
		} else {
			click = ClkWinTitle;
			for (c = selmon->clients; c; c = c->next)
				if (c->titlew > 0 && ev->x >= c->titlex && ev->x < c->titlex + c->titlew) {
					c->ishidden = 0;
					selmon->showdesktop = 0;
					setclientstate(c, NormalState);
					focus(c);
					arrange(selmon);
					break;
				}
		}
	} else if ((c = wintoclient(ev->window))) {
		focus(c);
		restack(selmon);
		XAllowEvents(dpy, ReplayPointer, CurrentTime);
		click = ClkClientWin;
	}
	for (i = 0; i < LENGTH(buttons); i++)
		if (click == buttons[i].click && buttons[i].func && buttons[i].button == ev->button
		    && CLEANMASK(buttons[i].mask) == CLEANMASK(ev->state))
			buttons[i].func(click == ClkTagBar && buttons[i].arg.i == 0 ? &arg : &buttons[i].arg);
}

void
checkotherwm(void)
{
	xerrorxlib = XSetErrorHandler(xerrorstart);
	/* this causes an error if some other window manager is running */
	XSelectInput(dpy, DefaultRootWindow(dpy), SubstructureRedirectMask);
	XSync(dpy, False);
	XSetErrorHandler(xerror);
	XSync(dpy, False);
}

void
cleanup(void)
{
	Arg a = {.ui = ~0};
	Layout foo = { "", NULL };
	Monitor *m;
	size_t i;

	endalttab(0);
	for (m = mons; m; m = m->next) {
		m->showdesktop = 0;
		for (Client *c = m->clients; c; c = c->next)
			c->ishidden = 0;
	}
	view(&a);
	selmon->lt[selmon->sellt] = &foo;
	for (m = mons; m; m = m->next)
		while (m->stack)
			unmanage(m->stack, 0);
	XUngrabKey(dpy, AnyKey, AnyModifier, root);
	cleanupsystray();
	while (mons)
		cleanupmon(mons);
	for (i = 0; i < CurLast; i++)
		drw_cur_free(drw, cursor[i]);
	for (i = 0; i < LENGTH(edgecursors); i++)
		XFreeCursor(dpy, edgecursors[i]);
	for (i = 0; i < LENGTH(colors); i++)
		drw_scm_free(drw, scheme[i], 3);
	free(scheme);
	for (i = 0; i < LENGTH(titlescheme); i++)
		drw_scm_free(drw, titlescheme[i], 3);
	XDestroyWindow(dpy, wmcheckwin);
	drw_free(drw);
	XSync(dpy, False);
	XSetInputFocus(dpy, PointerRoot, RevertToPointerRoot, CurrentTime);
	XDeleteProperty(dpy, root, netatom[NetActiveWindow]);
}

void
cleanupmon(Monitor *mon)
{
	Monitor *m;

	endalttab(0);

	if (mon == mons)
		mons = mons->next;
	else {
		for (m = mons; m && m->next != mon; m = m->next);
		m->next = mon->next;
	}
	XUnmapWindow(dpy, mon->barwin);
	XDestroyWindow(dpy, mon->barwin);
	free(mon);
}

void
clientmessage(XEvent *e)
{
	XWindowAttributes wa;
	XSetWindowAttributes swa;
	XClientMessageEvent *cme = &e->xclient;
	Client *c = wintoclient(cme->window);

	if (systray && cme->window == systray->win && cme->message_type == netatom[NetSystemTrayOP] && cme->format == 32) {
		if (cme->data.l[1] == SYSTEM_TRAY_REQUEST_DOCK &&
		    cme->data.l[2] && !wintosystrayicon(cme->data.l[2]) &&
		    XGetWindowAttributes(dpy, cme->data.l[2], &wa)) {
			c = ecalloc(1, sizeof *c);
			c->win = cme->data.l[2];
			c->mon = selmon;
			c->next = systray->icons;
			systray->icons = c;
			c->x = c->oldx = c->y = c->oldy = 0;
			c->w = c->oldw = wa.width;
			c->h = c->oldh = wa.height;
			c->oldbw = wa.border_width;
			c->bw = 0;
			c->isfloating = True;
			/* reuse tags field as mapped status */
			c->tags = !!(getatomprop(c, xatom[XembedInfo]) & XEMBED_MAPPED);
			XSetWindowBorderWidth(dpy, c->win, 0);
			updatesystrayicongeom(c, wa.width, wa.height);
			XAddToSaveSet(dpy, c->win);
			XSelectInput(dpy, c->win, StructureNotifyMask | PropertyChangeMask | ResizeRedirectMask);
			XReparentWindow(dpy, c->win, systray->win, 0, 0);
			if (!c->tags)
				XUnmapWindow(dpy, c->win);
			/* use parents background color */
			swa.background_pixel  = scheme[SchemeNorm][ColBg].pixel;
			XChangeWindowAttributes(dpy, c->win, CWBackPixel, &swa);
			sendevent(c->win, xatom[Xembed], NoEventMask, CurrentTime, XEMBED_EMBEDDED_NOTIFY, 0 , systray->win, XEMBED_EMBEDDED_VERSION);
			XSync(dpy, False);
			drawbars();
			setclientstate(c, c->tags ? NormalState : WithdrawnState);
		}
		return;
	}

	if (!c)
		return;
	if (cme->message_type == netatom[NetWMState]) {
		if (cme->data.l[1] == netatom[NetWMFullscreen]
		    || cme->data.l[2] == netatom[NetWMFullscreen])
			setfullscreen(c, (cme->data.l[0] == 1 /* _NET_WM_STATE_ADD    */
					  || (cme->data.l[0] == 2 /* _NET_WM_STATE_TOGGLE */ && !c->isfullscreen)));
	} else if (cme->message_type == netatom[NetActiveWindow]) {
		if (cme->data.l[0] == 2) {
			c->ishidden = 0;
			c->mon->showdesktop = 0;
			unfocus(selmon->sel, 0);
			selmon = c->mon;
			view(&(Arg){.ui = c->tags});
			setclientstate(c, NormalState);
			focus(c);
			arrange(selmon);
		} else if (c != selmon->sel && !c->isurgent)
			seturgent(c, 1);
	}
}

void
configure(Client *c)
{
	XConfigureEvent ce;

	ce.type = ConfigureNotify;
	ce.display = dpy;
	ce.event = c->win;
	ce.window = c->win;
	ce.x = c->x;
	ce.y = c->y + titleheight(c);
	ce.width = c->w;
	ce.height = MAX(1, c->h - titleheight(c));
	ce.border_width = c->bw;
	ce.above = None;
	ce.override_redirect = False;
	XSendEvent(dpy, c->win, False, StructureNotifyMask, (XEvent *)&ce);
}

void
configurenotify(XEvent *e)
{
	Monitor *m;
	Client *c;
	XConfigureEvent *ev = &e->xconfigure;
	int dirty;

	/* TODO: updategeom handling sucks, needs to be simplified */
	if (ev->window == root) {
		endalttab(0);
		dirty = (sw != ev->width || sh != ev->height);
		sw = ev->width;
		sh = ev->height;
		if (updategeom() || dirty) {
			drw_resize(drw, sw, bh);
			updatebars();
			for (m = mons; m; m = m->next) {
				for (c = m->clients; c; c = c->next)
					if (c->isfullscreen)
						resizeclient(c, m->mx, m->my, m->mw, m->mh);
				resizebarwin(m);
			}
			focus(NULL);
			arrange(NULL);
		}
	}
}

void
configurerequest(XEvent *e)
{
	Client *c;
	Monitor *m;
	XConfigureRequestEvent *ev = &e->xconfigurerequest;
	XWindowChanges wc;

	if ((c = wintoclient(ev->window))) {
		if (c->isfullscreen) {
			configure(c);
			XSync(dpy, False);
			return;
		}
		if (ev->value_mask & CWBorderWidth)
			c->bw = ev->border_width;
		else if (c->isfloating || !selmon->lt[selmon->sellt]->arrange) {
			m = c->mon;
			if (ev->value_mask & CWX) {
				c->oldx = c->x;
				c->x = m->mx + ev->x;
			}
			if (ev->value_mask & CWY) {
				c->oldy = c->y;
				c->y = m->my + ev->y - titleheight(c);
			}
			if (ev->value_mask & CWWidth) {
				c->oldw = c->w;
				c->w = ev->width;
			}
			if (ev->value_mask & CWHeight) {
				c->oldh = c->h;
				c->h = ev->height + titleheight(c);
			}
			if (c->ismpv && (ev->value_mask & (CWWidth|CWHeight))) {
				fitfloating(c);
				configure(c);
			}
			if ((c->x + c->w) > m->mx + m->mw && c->isfloating)
				c->x = m->mx + (m->mw / 2 - WIDTH(c) / 2); /* center in x direction */
			if ((c->y + c->h) > m->my + m->mh && c->isfloating)
				c->y = m->my + (m->mh / 2 - HEIGHT(c) / 2); /* center in y direction */
			if ((ev->value_mask & (CWX|CWY)) && !(ev->value_mask & (CWWidth|CWHeight)))
				configure(c);
			if (ISVISIBLE(c))
				XMoveResizeWindow(dpy, c->win, c->x, c->y + titleheight(c), c->w, MAX(1, c->h - titleheight(c)));
		} else
			configure(c);
		updatehandles(c);
	} else {
		wc.x = ev->x;
		wc.y = ev->y;
		wc.width = ev->width;
		wc.height = ev->height;
		wc.border_width = ev->border_width;
		wc.sibling = ev->above;
		wc.stack_mode = ev->detail;
		XConfigureWindow(dpy, ev->window, ev->value_mask, &wc);
	}
	XSync(dpy, False);
}

Monitor *
createmon(void)
{
	Monitor *m;

	m = ecalloc(1, sizeof(Monitor));
	m->tagset[0] = m->tagset[1] = 1;
	m->mfact = mfact;
	m->nmaster = nmaster;
	m->showbar = showbar;
	m->topbar = topbar;
	m->gappx = gappx;
	m->lt[0] = &layouts[defaultlayout];
	m->lt[1] = &layouts[1 % LENGTH(layouts)];
	strncpy(m->ltsymbol, m->lt[0]->symbol, sizeof m->ltsymbol);
	return m;
}

void
destroynotify(XEvent *e)
{
	Client *c;
	XDestroyWindowEvent *ev = &e->xdestroywindow;

	if ((c = wintoclient(ev->window)))
		unmanage(c, 1);
	else if ((c = wintosystrayicon(ev->window))) {
		removesystrayicon(c);
		drawbars();
	}
}

void
detach(Client *c)
{
	Client **tc;

	for (tc = &c->mon->clients; *tc && *tc != c; tc = &(*tc)->next);
	*tc = c->next;
}

void
detachstack(Client *c)
{
	Client **tc, *t;

	for (tc = &c->mon->stack; *tc && *tc != c; tc = &(*tc)->snext);
	*tc = c->snext;

	if (c == c->mon->sel) {
		for (t = c->mon->stack; t && !ISVISIBLE(t); t = t->snext);
		c->mon->sel = t;
	}
}

Monitor *
dirtomon(int dir)
{
	Monitor *m = NULL;

	if (dir > 0) {
		if (!(m = selmon->next))
			m = mons;
	} else if (selmon == mons)
		for (m = mons; m->next; m = m->next);
	else
		for (m = mons; m->next != selmon; m = m->next);
	return m;
}

int
statusbar(int x, int draw)
{
	static const int schemes[] = { SchemeNorm, SchemeWarn, SchemeCritical, SchemeGood, SchemeMuted, SchemeYellow };
	char text[sizeof stext];
	const unsigned char *p = (unsigned char *)stext;
	int start = x, color = SchemeNorm, width, signalbars = 5, visible;
	size_t length;

	wifistart = wifiend = -1;
	bluetoothstart = bluetoothend = -1;
	weatherstart = weatherend = -1;
	clockstart = clockend = -1;
	while (*p) {
		if (*p < 32) {
			if (*p == 7)
				wifistart = x - start;
			else if (*p == 8)
				wifiend = x - start;
			else if (*p == 22)
				clockstart = x - start;
			else if (*p == 23)
				clockend = x - start;
			else if (*p == 24)
				bluetoothstart = x - start;
			else if (*p == 25)
				bluetoothend = x - start;
			else if (*p == 26)
				weatherstart = x - start;
			else if (*p == 27)
				weatherend = x - start;
			else if (*p >= 17 && *p <= 21)
				signalbars = *p - 16;
			else
				color = *p <= LENGTH(schemes) ? schemes[*p - 1] : SchemeNorm;
			p++;
			continue;
		}
		for (length = 0; *p >= 32 && length < sizeof text - 1; p++)
			text[length++] = *p;
		text[length] = '\0';
		width = TEXTW(text) - lrpad;
		if (draw) {
			drw_setscheme(drw, scheme[color]);
			drw_text(drw, x, 0, width, bh, 0, text, 0);
			if (signalbars < 5 && !strcmp(text, "")) {
				/* the font supplies five bars; hide those above the signal level. */
				visible = width * signalbars / 5;
				drw_rect(drw, x + visible, 0, width - visible, bh, 1, 1);
			}
		}
		signalbars = 5;
		x += width;
	}
	if (wifistart >= 0 && wifiend < 0)
		wifiend = x - start;
	if (bluetoothstart >= 0 && bluetoothend < 0)
		bluetoothend = x - start;
	if (draw) {
		drw_setscheme(drw, scheme[SchemeNorm]);
		drw_rect(drw, x, 0, 2, bh, 1, 1);
	}
	return x - start + 2;
}

void
drawbar(Monitor *m)
{
	int x, w, tw = 0, mw, ew = 0;
	int lw = TEXTW(m->ltsymbol);
	int stw = m == systraytomon(NULL) ? getsystraywidth() : 0;
	int boxs = drw->fonts->h / 9;
	int boxw = drw->fonts->h / 6 + 2;
	unsigned int i, occ = 0, urg = 0, n = 0;
	Client *c;

	for (c = m->clients; c; c = c->next)
		c->titlew = 0;
	if (!m->showbar)
		return;

	/* draw status first so it can be overdrawn by tags later */
	if (m == selmon) { /* status is only drawn on selected monitor */
		tw = statusbar(0, 0);
		statusbar(m->ww - tw - stw, 1);
	}

	for (c = m->clients; c; c = c->next) {
		if (ISTAGVISIBLE(c))
			n++;
		occ |= c->tags;
		if (c->isurgent)
			urg |= c->tags;
	}
	x = 0;
	for (i = 0; i < LENGTH(tags); i++) {
		w = TEXTW(tags[i]);
		drw_setscheme(drw, scheme[m->tagset[m->seltags] & 1 << i ? SchemeSel : SchemeNorm]);
		drw_text(drw, x, 0, w, bh, lrpad / 2, tags[i], urg & 1 << i);
		if (occ & 1 << i)
			drw_rect(drw, x + boxs, boxs, boxw, boxw,
				 m == selmon && selmon->sel && selmon->sel->tags & 1 << i,
				 urg & 1 << i);
		x += w;
	}
	drw_setscheme(drw, scheme[SchemeNorm]);
	drw_text(drw, x, 0, lw, bh, lrpad / 2, m->ltsymbol, 0);
	x += lw;

	if ((w = m->ww - tw - stw - x) > bh) {
		if (n > 0) {
			tw = m->sel ? TEXTW(m->sel->name) + lrpad : 0;
			mw = m->sel ? ((tw >= w || n == 1) ? 0 : (w - tw) / (n - 1)) : w / n;

			i = 0;
			for (c = m->clients; c; c = c->next) {
				if (!ISTAGVISIBLE(c) || c == m->sel)
					continue;
				tw = TEXTW(c->name);
				if (tw < mw)
					ew += (mw - tw);
				else
					i++;
			}
			if (i > 0)
				mw += ew / i;

			for (c = m->clients; c; c = c->next) {
				if (!ISTAGVISIBLE(c))
					continue;
				tw = MAX(0, MIN(w, MIN(m->sel == c ? w : mw, (int)TEXTW(c->name))));
				c->titlex = x;
				c->titlew = tw;

				drw_setscheme(drw, scheme[m == selmon && m->sel == c ? SchemeSel : SchemeNorm]);
				if (tw > lrpad / 2)
					drw_text(drw, x, 0, tw, bh, lrpad / 2, c->name, c->ishidden);
				if (c->isfloating && tw > boxs + boxw)
					drw_rect(drw, x + boxs, boxs, boxw, boxw, c->isfixed, 0);
				x += tw;
				w -= tw;
			}
		}
		drw_setscheme(drw, scheme[SchemeNorm]);
		drw_rect(drw, x, 0, w, bh, 1, 1);
	}
	if (systray)
		resizebarwin(m);
	drw_map(drw, m->barwin, 0, 0, MAX(1, m->ww - stw), bh);
}

void
drawbars(void)
{
	Monitor *m;

	updatesystray();
	for (m = mons; m; m = m->next) {
		drawbar(m);
		for (Client *c = m->clients; c; c = c->next)
			drawtitlebar(c);
	}
}

void
enternotify(XEvent *e)
{
	Client *c;
	Monitor *m;
	XCrossingEvent *ev = &e->xcrossing;

	if (tabwin)
		return;
	if ((ev->mode != NotifyNormal || ev->detail == NotifyInferior) && ev->window != root)
		return;
	c = wintoclient(ev->window);
	if (!c)
		c = titleclient(ev->window);
	if (!c)
		c = handleclient(ev->window, NULL);
	m = c ? c->mon : wintomon(ev->window);
	if (m != selmon) {
		unfocus(selmon->sel, 1);
		selmon = m;
	} else if (!c || c == selmon->sel)
		return;
	focus(c);
}

void
expose(XEvent *e)
{
	Monitor *m;
	XExposeEvent *ev = &e->xexpose;

	Client *c;
	if ((c = titleclient(ev->window))) {
		if (ev->count == 0)
			drawtitlebar(c);
		return;
	}
	if (tabwin && ev->window == tabwin) {
		drawalttab();
		return;
	}
	if (ev->count == 0 && (m = wintomon(ev->window)))
		drawbar(m);
}

void
focus(Client *c)
{
	if (!c || !ISVISIBLE(c))
		for (c = selmon->stack; c && !ISVISIBLE(c); c = c->snext);
	if (selmon->sel && selmon->sel != c)
		unfocus(selmon->sel, 0);
	if (c) {
		if (c->mon != selmon)
			selmon = c->mon;
		if (c->isurgent)
			seturgent(c, 0);
		detachstack(c);
		attachstack(c);
		grabbuttons(c);
		XSetWindowBorder(dpy, c->win, scheme[SchemeSel][ColBorder].pixel);
		setfocus(c);
	} else {
		XSetInputFocus(dpy, root, RevertToPointerRoot, CurrentTime);
		XDeleteProperty(dpy, root, netatom[NetActiveWindow]);
	}
	selmon->sel = c;
	drawbars();
}

/* there are some broken focus acquiring clients needing extra handling */
void
focusin(XEvent *e)
{
	XFocusChangeEvent *ev = &e->xfocus;

	if (ev->mode != NotifyNormal || tabwin)
		return;
	if (selmon->sel && ev->window != selmon->sel->win)
		setfocus(selmon->sel);
}

void
focusmon(const Arg *arg)
{
	Monitor *m;

	if (!mons->next)
		return;
	if ((m = dirtomon(arg->i)) == selmon)
		return;
	unfocus(selmon->sel, 0);
	selmon = m;
	focus(NULL);
}

void
focusstack(const Arg *arg)
{
	Client *c = NULL, *i;

	if (!selmon->sel || (selmon->sel->isfullscreen && lockfullscreen))
		return;
	if (arg->i > 0) {
		for (c = selmon->sel->next; c && !ISVISIBLE(c); c = c->next);
		if (!c)
			for (c = selmon->clients; c && !ISVISIBLE(c); c = c->next);
	} else {
		for (i = selmon->clients; i != selmon->sel; i = i->next)
			if (ISVISIBLE(i))
				c = i;
		if (!c)
			for (; i; i = i->next)
				if (ISVISIBLE(i))
					c = i;
	}
	if (c) {
		focus(c);
		restack(selmon);
	}
}

Atom
getatomprop(Client *c, Atom prop)
{
	int format;
	unsigned long nitems, dl;
	unsigned char *p = NULL;
	Atom da, atom = None;

	/* xembed stores version and flags; callers need the flags. */
	Atom req = XA_ATOM;
	if (prop == xatom[XembedInfo])
		req = xatom[XembedInfo];

	if (XGetWindowProperty(dpy, c->win, prop, 0L, sizeof atom, False, req,
			       &da, &format, &nitems, &dl, &p) == Success && p) {
		if (nitems > 0 && format == 32)
			atom = *(long *)p;
		if (da == xatom[XembedInfo] && nitems == 2 && format == 32)
			atom = ((long *)p)[1];
		XFree(p);
	}
	return atom;
}

unsigned int
getsystraywidth(void)
{
	unsigned int w = 0;
	Client *i;

	if (!showsystray || !systray)
		return 0;
	for (i = systray->icons; i; i = i->next)
		if (i->tags)
			w += i->w + systrayspacing;
	return w ? MIN(w + systrayspacing, MAX(1, systraytomon(NULL)->ww / 2)) : 0;
}

int
getrootptr(int *x, int *y)
{
	int di;
	unsigned int dui;
	Window dummy;

	return XQueryPointer(dpy, root, &dummy, &dummy, x, y, &di, &di, &dui);
}

long
getstate(Window w)
{
	int format;
	long result = -1;
	unsigned char *p = NULL;
	unsigned long n, extra;
	Atom real;

	if (XGetWindowProperty(dpy, w, wmatom[WMState], 0L, 2L, False, wmatom[WMState],
			       &real, &format, &n, &extra, &p) != Success)
		return -1;
	if (n != 0 && format == 32)
		result = *(long *)p;
	XFree(p);
	return result;
}

int
gettextprop(Window w, Atom atom, char *text, unsigned int size)
{
	char **list = NULL;
	int n;
	XTextProperty name;

	if (!text || size == 0)
		return 0;
	text[0] = '\0';
	if (!XGetTextProperty(dpy, w, &name, atom) || !name.nitems)
		return 0;
	if (name.encoding == XA_STRING) {
		strncpy(text, (char *)name.value, size - 1);
	} else if (XmbTextPropertyToTextList(dpy, &name, &list, &n) >= Success && n > 0 && *list) {
		strncpy(text, *list, size - 1);
		XFreeStringList(list);
	}
	text[size - 1] = '\0';
	XFree(name.value);
	return 1;
}

void
grabbuttons(Client *c)
{
	updatenumlockmask();
	{
		unsigned int i, j;
		unsigned int modifiers[] = { 0, LockMask, numlockmask, numlockmask|LockMask };
		XUngrabButton(dpy, AnyButton, AnyModifier, c->win);
		/* hover focus must not stop clicks from raising the window. */
		XGrabButton(dpy, AnyButton, AnyModifier, c->win, False,
			    BUTTONMASK, GrabModeSync, GrabModeAsync, None, None);
		for (i = 0; i < LENGTH(buttons); i++)
			if (buttons[i].click == ClkClientWin)
				for (j = 0; j < LENGTH(modifiers); j++)
					XGrabButton(dpy, buttons[i].button,
						    buttons[i].mask | modifiers[j],
						    c->win, False, BUTTONMASK,
						    GrabModeAsync, GrabModeAsync, None, None);
	}
}

void
grabkeys(void)
{
	updatenumlockmask();
	{
		unsigned int i, j, k;
		unsigned int modifiers[] = { 0, LockMask, numlockmask, numlockmask|LockMask };
		int start, end, skip;
		KeySym *syms;

		XUngrabKey(dpy, AnyKey, AnyModifier, root);
		XDisplayKeycodes(dpy, &start, &end);
		syms = XGetKeyboardMapping(dpy, start, end - start + 1, &skip);
		if (!syms)
			return;
		for (k = start; k <= end; k++)
			for (i = 0; i < LENGTH(keys); i++)
				/* skip modifier codes, we do that ourselves */
				if (keys[i].keysym == syms[(k - start) * skip])
					for (j = 0; j < LENGTH(modifiers); j++)
						XGrabKey(dpy, k,
							 keys[i].mod | modifiers[j],
							 root, True,
							 GrabModeAsync, GrabModeAsync);
		XFree(syms);
	}
}

void
incnmaster(const Arg *arg)
{
	selmon->nmaster = !selmon->nmaster;
	arrange(selmon);
}

#ifdef XINERAMA
static int
isuniquegeom(XineramaScreenInfo *unique, size_t n, XineramaScreenInfo *info)
{
	while (n--)
		if (unique[n].x_org == info->x_org && unique[n].y_org == info->y_org
		    && unique[n].width == info->width && unique[n].height == info->height)
			return 0;
	return 1;
}
#endif /* XINERAMA */

void
keypress(XEvent *e)
{
	unsigned int i;
	KeySym keysym;
	XKeyEvent *ev;

	ev = &e->xkey;
	keysym = XKeycodeToKeysym(dpy, (KeyCode)ev->keycode, 0);
	if (tabwin) {
		if (keysym == XK_Escape)
			endalttab(0);
		else if (keysym == XK_Return)
			endalttab(1);
		else if (keysym == XK_Tab || keysym == XK_grave || keysym == XK_Down || keysym == XK_Up) {
			tabindex = (tabindex + tabcount + ((ev->state & ShiftMask) || keysym == XK_Up ? -1 : 1)) % tabcount;
			drawalttab();
		}
		return;
	}
	for (i = 0; i < LENGTH(keys); i++)
		if (keysym == keys[i].keysym
		    && CLEANMASK(keys[i].mod) == CLEANMASK(ev->state)
		    && keys[i].func)
			keys[i].func(&(keys[i].arg));
}

void
killclient(const Arg *arg)
{
	if (!selmon->sel)
		return;

	if (!sendevent(selmon->sel->win, wmatom[WMDelete], NoEventMask, wmatom[WMDelete], CurrentTime, 0 , 0, 0)) {
		XGrabServer(dpy);
		XSetErrorHandler(xerrordummy);
		XSetCloseDownMode(dpy, DestroyAll);
		XKillClient(dpy, selmon->sel->win);
		XSync(dpy, False);
		XSetErrorHandler(xerror);
		XUngrabServer(dpy);
	}
}

void
manage(Window w, XWindowAttributes *wa)
{
	Client *c, *t = NULL;
	int px, py;
	Window trans = None;
	XWindowChanges wc;

	c = ecalloc(1, sizeof(Client));
	c->win = w;
	c->frameextent = -1;
	/* geometry */
	c->x = c->oldx = wa->x;
	c->y = c->oldy = wa->y;
	c->w = c->oldw = wa->width;
	c->h = c->oldh = wa->height;
	c->oldbw = wa->border_width;
	c->depth = wa->depth;

	updatetitle(c);
	if (XGetTransientForHint(dpy, w, &trans) && (t = wintoclient(trans))) {
		c->mon = t->mon;
		c->tags = t->tags;
	} else {
		c->mon = selmon;
		if (!selmon->lt[selmon->sellt]->arrange && getrootptr(&px, &py))
			c->mon = recttomon(px, py, 1, 1);
		applyrules(c);
	}

	c->bw = borderpx;
	createtitlebar(c);
	c->y -= titleheight(c);
	c->oldy = c->y;
	c->h += titleheight(c);
	c->oldh = c->h;

	if (c->x + WIDTH(c) > c->mon->wx + c->mon->ww)
		c->x = c->mon->wx + c->mon->ww - WIDTH(c);
	if (c->y + HEIGHT(c) > c->mon->wy + c->mon->wh)
		c->y = c->mon->wy + c->mon->wh - HEIGHT(c);
	c->x = MAX(c->x, c->mon->wx);
	c->y = MAX(c->y, c->mon->wy);
	c->bw = borderpx;

	wc.border_width = c->bw;
	XConfigureWindow(dpy, w, CWBorderWidth, &wc);
	XSetWindowBorder(dpy, w, scheme[SchemeNorm][ColBorder].pixel);
	configure(c); /* propagates border_width, if size doesn't change */
	updatewindowtype(c);
	updatesizehints(c);
	updatewmhints(c);
	XSelectInput(dpy, w, EnterWindowMask|FocusChangeMask|PropertyChangeMask|StructureNotifyMask);
	grabbuttons(c);
	if (!c->isfloating)
		c->isfloating = c->oldstate = trans != None || c->isfixed;
	c->alwaysfloating |= (c->isfullscreen ? c->oldstate : c->isfloating) || c->isfixed;
	c->floatw = c->w;
	c->floath = c->h;
	if ((c->isfloating || !c->mon->lt[c->mon->sellt]->arrange) && !c->isfullscreen) {
		placefloating(c);
		c->floatx = c->x; c->floaty = c->y;
		c->floatw = c->w; c->floath = c->h;
		c->floatvalid = 1;
	}
	if (c->isfloating || !c->mon->lt[c->mon->sellt]->arrange)
		XRaiseWindow(dpy, c->win);
	c->mon->showdesktop = 0;
	attach(c);
	attachstack(c);
	XChangeProperty(dpy, root, netatom[NetClientList], XA_WINDOW, 32, PropModeAppend,
			(unsigned char *) &(c->win), 1);
	XMoveResizeWindow(dpy, c->win, c->x + 2 * sw, c->y + titleheight(c), c->w, MAX(1, c->h - titleheight(c))); /* some windows require this */
	setclientstate(c, NormalState);
	if (c->mon == selmon)
		unfocus(selmon->sel, 0);
	c->mon->sel = c;
	arrange(c->mon);
	XMapWindow(dpy, c->win);
	focus(NULL);
}

void
mappingnotify(XEvent *e)
{
	XMappingEvent *ev = &e->xmapping;

	XRefreshKeyboardMapping(ev);
	if (ev->request == MappingKeyboard)
		grabkeys();
}

void
maprequest(XEvent *e)
{
	static XWindowAttributes wa;
	XMapRequestEvent *ev = &e->xmaprequest;

	Client *i;
	if ((i = wintosystrayicon(ev->window))) {
		updatesystrayiconstate(i, &(XPropertyEvent){.atom = xatom[XembedInfo]});
		drawbars();
		return;
	}

	if (!XGetWindowAttributes(dpy, ev->window, &wa) || wa.override_redirect)
		return;
	if (!wintoclient(ev->window))
		manage(ev->window, &wa);
}

void
monocle(Monitor *m)
{
	unsigned int n = 0;
	Client *c;

	for (c = m->clients; c; c = c->next)
		if (ISVISIBLE(c))
			n++;
	if (n > 0) /* override layout symbol */
		snprintf(m->ltsymbol, sizeof m->ltsymbol, "mono [%u]", n);
	for (c = nexttiled(m->clients); c; c = nexttiled(c->next))
		resize(c, m->wx, m->wy, m->ww - 2 * c->bw, m->wh - 2 * c->bw, 0);
}

void
motionnotify(XEvent *e)
{
	static Monitor *mon = NULL;
	Monitor *m;
	XMotionEvent *ev = &e->xmotion;

	if (tabwin || ev->window != root)
		return;
	if ((m = recttomon(ev->x_root, ev->y_root, 1, 1)) != mon && mon) {
		unfocus(selmon->sel, 1);
		selmon = m;
		focus(NULL);
	}
	mon = m;
}

void
movemouse(const Arg *arg)
{
	int x, y, ocx, ocy, nx, ny, rw, rh, moved = 0;
	Client *c;
	Monitor *m;
	XEvent ev;
	Time lasttime = 0;

	if (!(c = selmon->sel))
		return;
	if (c->isfullscreen) /* no support moving fullscreen windows by mouse */
		return;
	restack(selmon);
	ocx = c->x;
	ocy = c->y;
	if (XGrabPointer(dpy, root, False, MOUSEMASK, GrabModeAsync, GrabModeAsync,
			 None, cursor[CurMove]->cursor, CurrentTime) != GrabSuccess)
		return;
	if (!getrootptr(&x, &y)) {
		XUngrabPointer(dpy, CurrentTime);
		return;
	}
	do {
		XMaskEvent(dpy, MOUSEMASK|ExposureMask|SubstructureRedirectMask, &ev);
		switch(ev.type) {
		case ConfigureRequest:
		case Expose:
		case MapRequest:
			handler[ev.type](&ev);
			break;
		case MotionNotify:
		case ButtonRelease:
			if (ev.type == MotionNotify && (ev.xmotion.time - lasttime) <= (1000 / refreshrate))
				continue;
			lasttime = ev.xmotion.time;

			if (c->snapmode || (!c->isfloating && c->mon->lt[c->mon->sellt]->arrange)) {
				if (abs(ev.xmotion.x - x) <= snap && abs(ev.xmotion.y - y) <= snap)
					break;
				rw = c->snapmode ? c->savedw : c->w;
				rh = c->snapmode ? c->savedh : c->h;
				/* only unsnapped tiles need a new size when pulled free. */
				if (!c->snapmode) {
					if (rw + 2 * c->bw >= c->mon->ww)
						rw = c->mon->ww * 3 / 4;
					if (rh + 2 * c->bw >= c->mon->wh)
						rh = c->mon->wh * 3 / 4;
				}
				/* keep the pointer over the same part of the restored window. */
				ocx = x - (int)((long long)(x - c->x) * rw / MAX(1, c->w));
				ocy = y - (int)((long long)(y - c->y) * rh / MAX(1, c->h));
				moved = 1;
				c->snapmode = 0;
				c->isfloating = 1;
				resizefloatingclient(c, ocx, ocy, rw, rh);
			}
			nx = ocx + (ev.xmotion.x - x);
			ny = ocy + (ev.xmotion.y - y);
			if (!selmon->lt[selmon->sellt]->arrange || c->isfloating) {
				moved |= nx != c->x || ny != c->y;
				c->snapmode = 0;
				resizefloatingclient(c,
					MAX(c->mon->wx, MIN(nx, c->mon->wx + c->mon->ww - WIDTH(c))),
					MAX(c->mon->wy, MIN(ny, c->mon->wy + c->mon->wh - HEIGHT(c))), c->w, c->h);
			}
			break;
		}
	} while (ev.type != ButtonRelease);
	XUngrabPointer(dpy, CurrentTime);
	if (moved)
		arrange(c->mon);
	if ((m = recttomon(c->x, c->y, c->w, c->h)) != selmon) {
		sendmon(c, m);
		selmon = m;
		focus(NULL);
	}
	m = c->mon;
	if (moved && !m->lt[m->sellt]->arrange
	    && ev.xbutton.x >= m->wx && ev.xbutton.x < m->wx + m->ww
	    && ev.xbutton.y >= m->my && ev.xbutton.y < m->wy + m->wh) {
		if (ev.xbutton.y <= m->wy + snap)
			snapwindow(&(Arg){.i = 5});
		else if (ev.xbutton.x <= m->wx + snap)
			snapwindow(&(Arg){.i = 1});
		else if (ev.xbutton.x >= m->wx + m->ww - snap)
			snapwindow(&(Arg){.i = 2});
	}
}

Client *
nexttiled(Client *c)
{
	for (; c && (c->isfloating || !ISVISIBLE(c)); c = c->next);
	return c;
}

void
pop(Client *c)
{
	detach(c);
	attach(c);
	focus(c);
	arrange(c->mon);
}

void
propertynotify(XEvent *e)
{
	Client *c;
	Window trans;
	XPropertyEvent *ev = &e->xproperty;

	if ((c = wintosystrayicon(ev->window))) {
		updatesystrayiconstate(c, ev);
		drawbars();
		return;
	}

	if (ev->window == root && ev->atom == logoutatom && ev->state == PropertyNewValue) {
		char command[16];
		if (gettextprop(root, logoutatom, command, sizeof command) && !strcmp(command, "yes"))
			running = 0;
		XDeleteProperty(dpy, root, logoutatom);
		return;
	}
	if ((ev->window == root) && (ev->atom == XA_WM_NAME))
		updatestatus();
	else if (ev->state == PropertyDelete)
		return; /* ignore */
	else if ((c = wintoclient(ev->window))) {
		switch(ev->atom) {
		default: break;
		case XA_WM_TRANSIENT_FOR:
			 if (XGetTransientForHint(dpy, c->win, &trans) && wintoclient(trans)) {
				 c->alwaysfloating = c->isfloating = 1;
				 arrange(c->mon);
			 }
			 break;
		case XA_WM_NORMAL_HINTS:
			 c->hintsvalid = 0;
			 break;
		case XA_WM_HINTS:
			 updatewmhints(c);
			 drawbars();
			 break;
		}
		if (ev->atom == XA_WM_NAME || ev->atom == netatom[NetWMName]) {
			updatetitle(c);
			if (c == c->mon->sel)
				drawbar(c->mon);
		}
		if (ev->atom == netatom[NetWMWindowType])
			updatewindowtype(c);
	}
}

void
quit(const Arg *arg)
{
	static const char *command[] = { "dwm-action", "logout", NULL };

	spawn(&(Arg){.v = command});
}

Monitor *
recttomon(int x, int y, int w, int h)
{
	Monitor *m, *r = selmon;
	int a, area = 0;

	for (m = mons; m; m = m->next)
		if ((a = INTERSECT(x, y, w, h, m)) > area) {
			area = a;
			r = m;
		}
	return r;
}

void
removesystrayicon(Client *i)
{
	Client **link;

	if (!systray || !i)
		return;
	for (link = &systray->icons; *link && *link != i; link = &(*link)->next);
	if (*link) {
		*link = i->next;
		free(i);
	}
}

void
resize(Client *c, int x, int y, int w, int h, int interact)
{
	if (applysizehints(c, &x, &y, &w, &h, interact))
		resizeclient(c, x, y, w, h);
	else
		updatehandles(c);
}

void
resizebarwin(Monitor *m)
{
	unsigned int w = m->ww;
	if (systray && m == systraytomon(NULL))
		w = MAX(1, (int)w - (int)getsystraywidth());
	XMoveResizeWindow(dpy, m->barwin, m->wx, m->by, w, bh);
}

void
resizeclient(Client *c, int x, int y, int w, int h)
{
	XWindowChanges wc;

	c->oldx = c->x; c->x = wc.x = x;
	c->oldy = c->y; c->y = y;
	c->oldw = c->w; c->w = wc.width = w;
	c->oldh = c->h; c->h = MAX(h, titleheight(c) + 1);
	updatehandles(c);
	wc.y = y + titleheight(c);
	wc.height = MAX(1, c->h - titleheight(c));
	wc.border_width = c->bw;
	XConfigureWindow(dpy, c->win, CWX|CWY|CWWidth|CWHeight|CWBorderWidth, &wc);
	configure(c);
	XSync(dpy, False);
}

void
resizerequest(XEvent *e)
{
	XResizeRequestEvent *ev = &e->xresizerequest;
	Client *i;

	if ((i = wintosystrayicon(ev->window))) {
		updatesystrayicongeom(i, ev->width, ev->height);
		drawbars();
	}
}

void
resizemouse(const Arg *arg)
{
	int x, y, ocx, ocy, ocw, och, nx, ny, nw, nh, edges, minh, tiled;
	Client *c;
	XEvent ev;
	Time lasttime = 0;
	Cursor dragcursor;

	if (!(c = selmon->sel) || c->isfullscreen)
		return;
	restack(selmon);
	ocx = c->x;
	ocy = c->y;
	ocw = c->w;
	och = c->h;
	minh = 30 + titleheight(c);
	edges = arg && arg->i ? arg->i : EdgeRight|EdgeBottom;
	tiled = arg && arg->i && !c->isfloating && c->mon->lt[c->mon->sellt]->arrange;
	dragcursor = cursor[CurResize]->cursor;
	if (arg && arg->i)
		for (unsigned int i = 0; i < LENGTH(resizeedges); i++)
			if (resizeedges[i] == edges)
				dragcursor = edgecursors[i];
	if (XGrabPointer(dpy, root, False, MOUSEMASK, GrabModeAsync, GrabModeAsync,
			 None, dragcursor, CurrentTime) != GrabSuccess)
		return;
	if (!getrootptr(&x, &y)) {
		XUngrabPointer(dpy, CurrentTime);
		return;
	}
	do {
		XMaskEvent(dpy, MOUSEMASK|ExposureMask|SubstructureRedirectMask, &ev);
		switch (ev.type) {
		case ConfigureRequest:
		case Expose:
		case MapRequest:
			handler[ev.type](&ev);
			break;
		case MotionNotify:
		case ButtonRelease:
			if (ev.type == MotionNotify && (ev.xmotion.time - lasttime) <= (1000 / refreshrate))
				continue;
			lasttime = ev.xmotion.time;
			if (tiled) {
				resizetiled(c, edges, ev.xmotion.x - x, ev.xmotion.y - y);
				x = ev.xmotion.x;
				y = ev.xmotion.y;
				break;
			}
			nx = ocx;
			ny = ocy;
			nw = ocw;
			nh = och;
			if (edges & EdgeLeft) {
				nx = MAX(c->mon->wx, MIN(ocx + ev.xmotion.x - x, ocx + ocw - 30));
				nw = ocx + ocw - nx;
			} else if (edges & EdgeRight)
				nw = MAX(30, ocw + ev.xmotion.x - x);
			if (edges & EdgeTop) {
				ny = MAX(c->mon->wy, MIN(ocy + ev.xmotion.y - y, ocy + och - minh));
				nh = ocy + och - ny;
			} else if (edges & EdgeBottom)
				nh = MAX(minh, och + ev.xmotion.y - y);
			if (!c->isfloating && selmon->lt[selmon->sellt]->arrange
			    && (abs(nw - c->w) > snap || abs(nh - c->h) > snap))
				togglefloating(NULL);
			if (!selmon->lt[selmon->sellt]->arrange || c->isfloating) {
				c->snapmode = 0;
				resizefloatingclient(c, nx, ny, nw, nh);
			}
			break;
		}
	} while (ev.type != ButtonRelease);
	XUngrabPointer(dpy, CurrentTime);
	while (XCheckMaskEvent(dpy, EnterWindowMask, &ev));
}

void
restack(Monitor *m)
{
	Client *c;
	XEvent ev;
	XWindowChanges wc;

	drawbar(m);
	if (!m->sel)
		return;
	if (m->sel->isfloating || !m->lt[m->sellt]->arrange)
		XRaiseWindow(dpy, m->sel->win);
	if (m->lt[m->sellt]->arrange) {
		wc.stack_mode = Below;
		wc.sibling = m->barwin;
		for (c = m->stack; c; c = c->snext)
			if (!c->isfloating && ISVISIBLE(c)) {
				XConfigureWindow(dpy, c->win, CWSibling|CWStackMode, &wc);
				wc.sibling = c->win;
			}
	}
	for (c = m->clients; c; c = c->next)
		updatehandles(c);
	XSync(dpy, False);
	while (XCheckMaskEvent(dpy, EnterWindowMask, &ev));
}

void
run(void)
{
	XEvent ev;
	/* main event loop */
	XSync(dpy, False);
	while (running && !XNextEvent(dpy, &ev))
		if (handler[ev.type])
			handler[ev.type](&ev); /* call handler */
}

void
scan(void)
{
	unsigned int i, num;
	Window d1, d2, *wins = NULL;
	XWindowAttributes wa;

	if (XQueryTree(dpy, root, &d1, &d2, &wins, &num)) {
		for (i = 0; i < num; i++) {
			if (!XGetWindowAttributes(dpy, wins[i], &wa)
			    || wa.override_redirect || XGetTransientForHint(dpy, wins[i], &d1))
				continue;
			if (wa.map_state == IsViewable || getstate(wins[i]) == IconicState)
				manage(wins[i], &wa);
		}
		for (i = 0; i < num; i++) { /* now the transients */
			if (!XGetWindowAttributes(dpy, wins[i], &wa))
				continue;
			if (XGetTransientForHint(dpy, wins[i], &d1)
			    && (wa.map_state == IsViewable || getstate(wins[i]) == IconicState))
				manage(wins[i], &wa);
		}
		if (wins)
			XFree(wins);
	}
}

void
sendmon(Client *c, Monitor *m)
{
	if (c->mon == m)
		return;
	unfocus(c, 1);
	detach(c);
	detachstack(c);
	c->mon = m;
	c->tags = m->tagset[m->seltags]; /* assign tags of target monitor */
	attach(c);
	attachstack(c);
	if (c->isfullscreen)
		resizeclient(c, m->mx, m->my, m->mw, m->mh);
	focus(NULL);
	arrange(NULL);
}

void
setclientstate(Client *c, long state)
{
	long data[] = { state, None };

	XChangeProperty(dpy, c->win, wmatom[WMState], wmatom[WMState], 32,
			PropModeReplace, (unsigned char *)data, 2);
}

int
sendevent(Window w, Atom proto, int mask, long d0, long d1, long d2, long d3, long d4)
{
	int n;
	Atom *protocols, mt;
	int exists = 0;
	XEvent ev = {0};

	if (proto == wmatom[WMTakeFocus] || proto == wmatom[WMDelete]) {
		mt = wmatom[WMProtocols];
		if (XGetWMProtocols(dpy, w, &protocols, &n)) {
			while (!exists && n--)
				exists = protocols[n] == proto;
			XFree(protocols);
		}
	} else {
		exists = True;
		mt = proto;
	}

	if (exists) {
		ev.type = ClientMessage;
		ev.xclient.window = w;
		ev.xclient.message_type = mt;
		ev.xclient.format = 32;
		ev.xclient.data.l[0] = d0;
		ev.xclient.data.l[1] = d1;
		ev.xclient.data.l[2] = d2;
		ev.xclient.data.l[3] = d3;
		ev.xclient.data.l[4] = d4;
		XSendEvent(dpy, w, False, mask, &ev);
	}
	return exists;
}

void
setfocus(Client *c)
{
	if (!c->neverfocus)
		XSetInputFocus(dpy, c->win, RevertToPointerRoot, CurrentTime);
	XChangeProperty(dpy, root, netatom[NetActiveWindow], XA_WINDOW, 32,
			PropModeReplace, (unsigned char *)&c->win, 1);
	sendevent(c->win, wmatom[WMTakeFocus], NoEventMask, wmatom[WMTakeFocus], CurrentTime, 0, 0, 0);
}

void
setfullscreen(Client *c, int fullscreen)
{
	if (fullscreen && !c->isfullscreen) {
		XChangeProperty(dpy, c->win, netatom[NetWMState], XA_ATOM, 32,
				PropModeReplace, (unsigned char*)&netatom[NetWMFullscreen], 1);
		c->fsx = c->x;
		c->fsy = c->y;
		c->fsw = c->w;
		c->fsh = c->h;
		c->isfullscreen = 1;
		c->oldstate = c->isfloating;
		c->oldbw = c->bw;
		c->bw = 0;
		c->isfloating = 1;
		resizeclient(c, c->mon->mx, c->mon->my, c->mon->mw, c->mon->mh);
		XRaiseWindow(dpy, c->win);
	} else if (!fullscreen && c->isfullscreen){
		XChangeProperty(dpy, c->win, netatom[NetWMState], XA_ATOM, 32,
				PropModeReplace, (unsigned char*)0, 0);
		c->isfullscreen = 0;
		c->isfloating = c->oldstate;
		c->bw = c->oldbw;
		/* monitor resizes must not overwrite the pre-fullscreen geometry. */
		resizeclient(c, c->fsx, c->fsy, c->fsw, c->fsh);
		arrange(c->mon);
	}
}

void
setlayout(const Arg *arg)
{
	Monitor *m;
	const Layout *layout = arg && arg->v ? arg->v : selmon->lt[selmon->sellt ^ 1];
	int i;

	for (i = 0; i < LENGTH(layouts); i++)
		if (layout == &layouts[i])
			break;
	if (i == LENGTH(layouts))
		return;
	defaultlayout = i;
	if (layout->arrange)
		tiledlayout = i;
	for (m = mons; m; m = m->next) {
		switchlayout(m, layout);
		arrange(m);
	}
	savelayout();
}

void
setmode(const Arg *arg)
{
	setlayout(&(Arg){.v = &layouts[arg->i ? 1 : tiledlayout]});
}

/* arg > 1.0 will set mfact absolutely */
void
setmfact(const Arg *arg)
{
	float f;

	if (!arg || !selmon->lt[selmon->sellt]->arrange)
		return;
	f = arg->f < 1.0 ? arg->f + selmon->mfact : arg->f - 1.0;
	if (f < 0.05 || f > 0.95)
		return;
	selmon->mfact = f;
	arrange(selmon);
}

void
setup(void)
{
	int i;
	XSetWindowAttributes wa;
	Atom utf8string;
	struct sigaction sa;

	/* do not transform children into zombies when they terminate */
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = SA_NOCLDSTOP | SA_NOCLDWAIT | SA_RESTART;
	sa.sa_handler = SIG_IGN;
	sigaction(SIGCHLD, &sa, NULL);

	/* clean up any zombies (inherited from .xinitrc etc) immediately */
	while (waitpid(-1, NULL, WNOHANG) > 0);

	/* init screen */
	screen = DefaultScreen(dpy);
	sw = DisplayWidth(dpy, screen);
	sh = DisplayHeight(dpy, screen);
	root = RootWindow(dpy, screen);
	drw = drw_create(dpy, screen, root, sw, sh);
	if (!drw_fontset_create(drw, fonts, LENGTH(fonts)))
		die("no fonts could be loaded.");
	lrpad = drw->fonts->h;
	bh = drw->fonts->h + 2;
	loadlayout();
	updategeom();
	logoutatom = XInternAtom(dpy, "_DWM_LOGOUT", False);
	XDeleteProperty(dpy, root, logoutatom);
	/* init atoms */
	utf8string = XInternAtom(dpy, "UTF8_STRING", False);
	wmatom[WMProtocols] = XInternAtom(dpy, "WM_PROTOCOLS", False);
	wmatom[WMDelete] = XInternAtom(dpy, "WM_DELETE_WINDOW", False);
	wmatom[WMState] = XInternAtom(dpy, "WM_STATE", False);
	wmatom[WMTakeFocus] = XInternAtom(dpy, "WM_TAKE_FOCUS", False);
	netatom[NetActiveWindow] = XInternAtom(dpy, "_NET_ACTIVE_WINDOW", False);
	netatom[NetSupported] = XInternAtom(dpy, "_NET_SUPPORTED", False);
	char trayname[64];
	snprintf(trayname, sizeof trayname, "_NET_SYSTEM_TRAY_S%d", screen);
	netatom[NetSystemTray] = XInternAtom(dpy, trayname, False);
	netatom[NetSystemTrayOP] = XInternAtom(dpy, "_NET_SYSTEM_TRAY_OPCODE", False);
	netatom[NetSystemTrayOrientation] = XInternAtom(dpy, "_NET_SYSTEM_TRAY_ORIENTATION", False);
	netatom[NetWMName] = XInternAtom(dpy, "_NET_WM_NAME", False);
	netatom[NetWMState] = XInternAtom(dpy, "_NET_WM_STATE", False);
	netatom[NetWMCheck] = XInternAtom(dpy, "_NET_SUPPORTING_WM_CHECK", False);
	netatom[NetWMFullscreen] = XInternAtom(dpy, "_NET_WM_STATE_FULLSCREEN", False);
	netatom[NetWMWindowType] = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE", False);
	netatom[NetWMWindowTypeDialog] = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE_DIALOG", False);
	netatom[NetClientList] = XInternAtom(dpy, "_NET_CLIENT_LIST", False);
	netatom[NetWMFrameExtents] = XInternAtom(dpy, "_NET_FRAME_EXTENTS", False);
	xatom[Manager] = XInternAtom(dpy, "MANAGER", False);
	xatom[Xembed] = XInternAtom(dpy, "_XEMBED", False);
	xatom[XembedInfo] = XInternAtom(dpy, "_XEMBED_INFO", False);
	/* init cursors */
	{
		int shapes[] = {XC_left_side, XC_right_side, XC_top_side, XC_bottom_side,
			XC_top_left_corner, XC_top_right_corner, XC_bottom_left_corner, XC_bottom_right_corner};
		for (i = 0; i < LENGTH(shapes); i++)
			edgecursors[i] = XCreateFontCursor(dpy, shapes[i]);
	}
	cursor[CurNormal] = drw_cur_create(drw, XC_left_ptr);
	cursor[CurResize] = drw_cur_create(drw, XC_sizing);
	cursor[CurMove] = drw_cur_create(drw, XC_fleur);
	/* init appearance */
	scheme = ecalloc(LENGTH(colors), sizeof(Clr *));
	for (i = 0; i < LENGTH(colors); i++)
		scheme[i] = drw_scm_create(drw, colors[i], 3);
	for (i = 0; i < LENGTH(titlescheme); i++)
		titlescheme[i] = drw_scm_create(drw, titlecolors[i], 3);
	/* init bars */
	updatebars();
	initsystray();
	updatestatus();
	/* supporting window for NetWMCheck */
	wmcheckwin = XCreateSimpleWindow(dpy, root, 0, 0, 1, 1, 0, 0, 0);
	{
		unsigned long pid = getpid();
		XChangeProperty(dpy, wmcheckwin, XInternAtom(dpy, "_NET_WM_PID", False), XA_CARDINAL, 32,
		                PropModeReplace, (unsigned char *)&pid, 1);
	}
	XChangeProperty(dpy, wmcheckwin, netatom[NetWMCheck], XA_WINDOW, 32,
			PropModeReplace, (unsigned char *) &wmcheckwin, 1);
	XChangeProperty(dpy, wmcheckwin, netatom[NetWMName], utf8string, 8,
			PropModeReplace, (unsigned char *) "dwm", 3);
	XChangeProperty(dpy, root, netatom[NetWMCheck], XA_WINDOW, 32,
			PropModeReplace, (unsigned char *) &wmcheckwin, 1);
	/* EWMH support per view */
	XChangeProperty(dpy, root, netatom[NetSupported], XA_ATOM, 32,
			PropModeReplace, (unsigned char *) netatom, NetLast);
	XDeleteProperty(dpy, root, netatom[NetClientList]);
	/* select events */
	wa.cursor = cursor[CurNormal]->cursor;
	wa.event_mask = KeyReleaseMask|SubstructureRedirectMask|SubstructureNotifyMask
		|ButtonPressMask|PointerMotionMask|EnterWindowMask
		|LeaveWindowMask|StructureNotifyMask|PropertyChangeMask;
	XChangeWindowAttributes(dpy, root, CWEventMask|CWCursor, &wa);
	XSelectInput(dpy, root, wa.event_mask);
	grabkeys();
	focus(NULL);
}

void
seturgent(Client *c, int urg)
{
	XWMHints *wmh;

	c->isurgent = urg;
	if (!(wmh = XGetWMHints(dpy, c->win)))
		return;
	wmh->flags = urg ? (wmh->flags | XUrgencyHint) : (wmh->flags & ~XUrgencyHint);
	XSetWMHints(dpy, c->win, wmh);
	XFree(wmh);
}

void
showhide(Client *c)
{
	if (!c)
		return;
	updatehandles(c);
	if (ISVISIBLE(c)) {
		/* show clients top down */
		XMoveWindow(dpy, c->win, c->x, c->y + titleheight(c));
		if ((!c->mon->lt[c->mon->sellt]->arrange || c->isfloating) && !c->isfullscreen)
			resizefloatingclient(c, c->x, c->y, c->w, c->h);
		showhide(c->snext);
	} else {
		/* hide clients bottom up */
		showhide(c->snext);
		XMoveWindow(dpy, c->win, WIDTH(c) * -2, c->y);
	}
}

void
spawn(const Arg *arg)
{
	struct sigaction sa;

	if (arg->v == dmenucmd)
		snprintf(dmenumon, sizeof dmenumon, "%d", selmon->num);
	if (fork() == 0) {
		if (dpy)
			close(ConnectionNumber(dpy));
		setsid();

		sigemptyset(&sa.sa_mask);
		sa.sa_flags = 0;
		sa.sa_handler = SIG_DFL;
		sigaction(SIGCHLD, &sa, NULL);

		execvp(((char **)arg->v)[0], (char **)arg->v);
		die("dwm: execvp '%s' failed:", ((char **)arg->v)[0]);
	}
}

void
tag(const Arg *arg)
{
	if (selmon->sel && arg->ui & TAGMASK) {
		selmon->sel->tags = arg->ui & TAGMASK;
		focus(NULL);
		arrange(selmon);
	}
}

void
tagmon(const Arg *arg)
{
	if (!selmon->sel || !mons->next)
		return;
	sendmon(selmon->sel, dirtomon(arg->i));
}

void
tile(Monitor *m)
{
	int i, n, h, mw, my, ty, gap;
	float masterweight = 0, stackweight = 0;
	Client *c;

	for (n = 0, c = nexttiled(m->clients); c; c = nexttiled(c->next), n++) {
		if (n < m->nmaster)
			masterweight += tileweight(c);
		else
			stackweight += tileweight(c);
	}
	if (n == 0)
		return;

	/* keep tiny screens and large client stacks from producing negative sizes. */
	gap = MIN(m->gappx, MIN(m->ww / 4, m->wh / (n + 1)));
	if (n > m->nmaster)
		mw = m->nmaster ? (m->ww - gap) * m->mfact : 0;
	else
		mw = m->ww - gap;
	for (i = 0, my = ty = gap, c = nexttiled(m->clients); c; c = nexttiled(c->next), i++)
		if (i < m->nmaster) {
			c->tileedges = (n > m->nmaster ? EdgeRight : 0)
				| (i > 0 ? EdgeTop : 0) | (i + 1 < MIN(n, m->nmaster) ? EdgeBottom : 0);
			h = (m->wh - my - gap * (MIN(n, m->nmaster) - i)) * tileweight(c) / masterweight;
			resize(c, m->wx + gap, m->wy + my, MAX(1, mw - 2*c->bw - gap), MAX(1, h - 2*c->bw), 0);
			my += HEIGHT(c) + gap;
			masterweight -= tileweight(c);
		} else {
			c->tileedges = (m->nmaster > 0 ? EdgeLeft : 0)
				| (i > m->nmaster ? EdgeTop : 0) | (i + 1 < n ? EdgeBottom : 0);
			h = (m->wh - ty - gap * (n - i)) * tileweight(c) / stackweight;
			resize(c, m->wx + mw + gap, m->wy + ty, MAX(1, m->ww - mw - 2*c->bw - 2*gap), MAX(1, h - 2*c->bw), 0);
			ty += HEIGHT(c) + gap;
			stackweight -= tileweight(c);
		}
}

static Client *
bspregion(Client *c, int n, int x, int y, int w, int h, int gap, int masters, int edges)
{
	Client *second;
	int first, split, minimum, maximum, horizontal;
	float ratio;

	if (n == 1) {
		c->tileedges = edges;
		resize(c, x, y, MAX(1, w - 2*c->bw), MAX(1, h - 2*c->bw), 0);
		return nexttiled(c->next);
	}
	first = masters > 0 && masters < n ? masters : n / 2;
	second = c;
	for (int i = 0; i < first; i++)
		second = nexttiled(second->next);
	ratio = bspratio(c, n, first, masters);
	/* split the longer side, keeping earlier clients in the first child. */
	horizontal = w > h;
	if (horizontal) {
		minimum = tileminimum(c, first, 1);
		maximum = w - gap - tileminimum(second, n - first, 1);
		gap = MIN(gap, MAX(0, w - minimum - tileminimum(second, n - first, 1)));
		maximum = w - gap - tileminimum(second, n - first, 1);
		split = MAX(minimum, MIN(maximum, (int)((w - gap) * ratio)));
		c = bspregion(c, first, x, y, split, h, gap, 0, edges | EdgeRight);
		return bspregion(c, n - first, x + split + gap, y,
		                 MAX(1, w - split - gap), h, gap, 0, edges | EdgeLeft);
	}
	minimum = tileminimum(c, first, 0);
	gap = MIN(gap, MAX(0, h - minimum - tileminimum(second, n - first, 0)));
	maximum = h - gap - tileminimum(second, n - first, 0);
	split = MAX(minimum, MIN(maximum, (int)((h - gap) * ratio)));
	c = bspregion(c, first, x, y, w, split, gap, 0, edges | EdgeBottom);
	return bspregion(c, n - first, x, y + split + gap,
	                 w, MAX(1, h - split - gap), gap, 0, edges | EdgeTop);
}

void
bsptile(Monitor *m)
{
	Client *c;
	int n, gap;

	for (n = 0, c = nexttiled(m->clients); c; c = nexttiled(c->next), n++);
	if (!n)
		return;
	gap = MIN(m->gappx, MIN(m->ww / 4, m->wh / (n + 1)));
	bspregion(nexttiled(m->clients), n, m->wx + gap, m->wy + gap,
	          MAX(1, m->ww - 2*gap), MAX(1, m->wh - 2*gap), gap, m->nmaster, 0);
}

void
setgaps(const Arg *arg)
{
	selmon->gappx = arg->i ? MAX(0, MIN(100, selmon->gappx + arg->i)) : 0;
	arrange(selmon);
}

void
togglebar(const Arg *arg)
{
	selmon->showbar = !selmon->showbar;
	updatebarpos(selmon);
	resizebarwin(selmon);
	updatesystray();
	arrange(selmon);
}

void
togglefloating(const Arg *arg)
{
	if (!selmon->sel)
		return;
	if (selmon->sel->isfullscreen) /* no support for fullscreen windows */
		return;
	selmon->sel->snapmode = 0;
	selmon->sel->isfloating = !selmon->sel->isfloating || selmon->sel->isfixed;
	if (selmon->sel->isfloating)
		resize(selmon->sel, selmon->sel->x, selmon->sel->y,
		       selmon->sel->w, selmon->sel->h, 0);
	arrange(selmon);
}

void
toggletag(const Arg *arg)
{
	unsigned int newtags;

	if (!selmon->sel)
		return;
	newtags = selmon->sel->tags ^ (arg->ui & TAGMASK);
	if (newtags) {
		selmon->sel->tags = newtags;
		focus(NULL);
		arrange(selmon);
	}
}

void
toggleview(const Arg *arg)
{
	if (selmon->showdesktop) {
		selmon->showdesktop = 0;
		focus(NULL);
		arrange(selmon);
	}
	unsigned int newtagset = selmon->tagset[selmon->seltags] ^ (arg->ui & TAGMASK);

	if (newtagset) {
		selmon->tagset[selmon->seltags] = newtagset;
		focus(NULL);
		arrange(selmon);
	}
}

void
unfocus(Client *c, int setfocus)
{
	if (!c)
		return;
	grabbuttons(c);
	XSetWindowBorder(dpy, c->win, scheme[SchemeNorm][ColBorder].pixel);
	if (setfocus) {
		XSetInputFocus(dpy, root, RevertToPointerRoot, CurrentTime);
		XDeleteProperty(dpy, root, netatom[NetActiveWindow]);
	}
}

void
unmanage(Client *c, int destroyed)
{
	Monitor *m = c->mon;
	XWindowChanges wc;

	endalttab(0);

	destroyhandles(c);
	if (c->titlebar)
		XDestroyWindow(dpy, c->titlebar);
	detach(c);
	detachstack(c);
	if (!destroyed) {
		wc.border_width = c->oldbw;
		XGrabServer(dpy); /* avoid race conditions */
		XSetErrorHandler(xerrordummy);
		XSelectInput(dpy, c->win, NoEventMask);
		XDeleteProperty(dpy, c->win, netatom[NetWMFrameExtents]);
		XConfigureWindow(dpy, c->win, CWBorderWidth, &wc); /* restore border */
		XUngrabButton(dpy, AnyButton, AnyModifier, c->win);
		setclientstate(c, WithdrawnState);
		XSync(dpy, False);
		XSetErrorHandler(xerror);
		XUngrabServer(dpy);
	}
	free(c);
	focus(NULL);
	updateclientlist();
	arrange(m);
}

void
unmapnotify(XEvent *e)
{
	Client *c;
	XUnmapEvent *ev = &e->xunmap;

	if ((c = wintoclient(ev->window))) {
		if (ev->send_event)
			setclientstate(c, WithdrawnState);
		else
			unmanage(c, 0);
	} else if ((c = wintosystrayicon(ev->window))) {
		/* reparenting can unmap a visible icon; hidden icons stay hidden. */
		if (c->tags)
			XMapRaised(dpy, c->win);
	}

}

void
updatebars(void)
{
	unsigned int w;
	Monitor *m;
	XSetWindowAttributes wa = {
		.override_redirect = True,
		.background_pixmap = ParentRelative,
		.event_mask = ButtonPressMask|ExposureMask
	};
	XClassHint ch = {"dwm", "dwm"};
	for (m = mons; m; m = m->next) {
		if (m->barwin)
			continue;
		w = m->ww;
		if (systray && m == systraytomon(NULL))
			w = MAX(1, (int)w - (int)getsystraywidth());
		m->barwin = XCreateWindow(dpy, root, m->wx, m->by, w, bh, 0, DefaultDepth(dpy, screen),
					  CopyFromParent, DefaultVisual(dpy, screen),
					  CWOverrideRedirect|CWBackPixmap|CWEventMask, &wa);
		XDefineCursor(dpy, m->barwin, cursor[CurNormal]->cursor);
		if (systray && m == systraytomon(NULL))
			XMapRaised(dpy, systray->win);
		XMapRaised(dpy, m->barwin);
		XSetClassHint(dpy, m->barwin, &ch);
	}
}

void
updatebarpos(Monitor *m)
{
	m->wy = m->my;
	m->wh = m->mh;
	if (m->showbar) {
		m->wh -= bh;
		m->by = m->topbar ? m->wy : m->wy + m->wh;
		m->wy = m->topbar ? m->wy + bh : m->wy;
	} else
		m->by = -bh;
}

void
updateclientlist(void)
{
	Client *c;
	Monitor *m;

	XDeleteProperty(dpy, root, netatom[NetClientList]);
	for (m = mons; m; m = m->next)
		for (c = m->clients; c; c = c->next)
			XChangeProperty(dpy, root, netatom[NetClientList],
					XA_WINDOW, 32, PropModeAppend,
					(unsigned char *) &(c->win), 1);
}

int
updategeom(void)
{
	int dirty = 0;

#ifdef XINERAMA
	if (XineramaIsActive(dpy)) {
		int i, j, n, nn;
		Client *c;
		Monitor *m;
		XineramaScreenInfo *info = XineramaQueryScreens(dpy, &nn);
		XineramaScreenInfo *unique = NULL;

		for (n = 0, m = mons; m; m = m->next, n++);
		/* only consider unique geometries as separate screens */
		unique = ecalloc(nn, sizeof(XineramaScreenInfo));
		for (i = 0, j = 0; i < nn; i++)
			if (isuniquegeom(unique, j, &info[i]))
				memcpy(&unique[j++], &info[i], sizeof(XineramaScreenInfo));
		XFree(info);
		nn = j;

		/* new monitors if nn > n */
		for (i = n; i < nn; i++) {
			for (m = mons; m && m->next; m = m->next);
			if (m)
				m->next = createmon();
			else
				mons = createmon();
		}
		for (i = 0, m = mons; i < nn && m; m = m->next, i++)
			if (i >= n
			    || unique[i].x_org != m->mx || unique[i].y_org != m->my
			    || unique[i].width != m->mw || unique[i].height != m->mh)
			{
				dirty = 1;
				m->num = i;
				m->mx = m->wx = unique[i].x_org;
				m->my = m->wy = unique[i].y_org;
				m->mw = m->ww = unique[i].width;
				m->mh = m->wh = unique[i].height;
				updatebarpos(m);
			}
		/* removed monitors if n > nn */
		for (i = nn; i < n; i++) {
			for (m = mons; m && m->next; m = m->next);
			while ((c = m->clients)) {
				dirty = 1;
				m->clients = c->next;
				detachstack(c);
				c->mon = mons;
				attach(c);
				attachstack(c);
			}
			if (m == selmon)
				selmon = mons;
			cleanupmon(m);
		}
		free(unique);
	} else
#endif /* XINERAMA */
	{ /* default monitor setup */
		if (!mons)
			mons = createmon();
		if (mons->mw != sw || mons->mh != sh) {
			dirty = 1;
			mons->mw = mons->ww = sw;
			mons->mh = mons->wh = sh;
			updatebarpos(mons);
		}
	}
	if (dirty) {
		selmon = mons;
		selmon = wintomon(root);
	}
	return dirty;
}

void
updatenumlockmask(void)
{
	unsigned int i, j;
	XModifierKeymap *modmap;

	numlockmask = 0;
	modmap = XGetModifierMapping(dpy);
	for (i = 0; i < 8; i++)
		for (j = 0; j < modmap->max_keypermod; j++)
			if (modmap->modifiermap[i * modmap->max_keypermod + j]
			    == XKeysymToKeycode(dpy, XK_Num_Lock))
				numlockmask = (1 << i);
	XFreeModifiermap(modmap);
}

void
updatesizehints(Client *c)
{
	long msize;
	XSizeHints size;

	if (!XGetWMNormalHints(dpy, c->win, &size, &msize))
		/* size is uninitialized, ensure that size.flags aren't used */
		size.flags = PSize;
	if (size.flags & PBaseSize) {
		c->basew = size.base_width;
		c->baseh = size.base_height;
	} else if (size.flags & PMinSize) {
		c->basew = size.min_width;
		c->baseh = size.min_height;
	} else
		c->basew = c->baseh = 0;
	if (size.flags & PResizeInc) {
		c->incw = size.width_inc;
		c->inch = size.height_inc;
	} else
		c->incw = c->inch = 0;
	if (size.flags & PMaxSize) {
		c->maxw = size.max_width;
		c->maxh = size.max_height;
	} else
		c->maxw = c->maxh = 0;
	if (size.flags & PMinSize) {
		c->minw = size.min_width;
		c->minh = size.min_height;
	} else if (size.flags & PBaseSize) {
		c->minw = size.base_width;
		c->minh = size.base_height;
	} else
		c->minw = c->minh = 0;
	if (size.flags & PAspect) {
		c->mina = (float)size.min_aspect.y / size.min_aspect.x;
		c->maxa = (float)size.max_aspect.x / size.max_aspect.y;
	} else
		c->maxa = c->mina = 0.0;
	c->isfixed = (c->maxw && c->maxh && c->maxw == c->minw && c->maxh == c->minh);
	c->hintsvalid = 1;
}

void
updatestatus(void)
{
	if (!gettextprop(root, XA_WM_NAME, stext, sizeof(stext)))
		strcpy(stext, "dwm-"VERSION);
	drawbar(selmon);
	updatesystray();
}

void
initsystray(void)
{
	unsigned long orientation = 0;
	XSetWindowAttributes wa = {
		.override_redirect = True,
		.background_pixel = scheme[SchemeNorm][ColBg].pixel,
		.event_mask = SubstructureNotifyMask
	};

	if (!showsystray)
		return;
	if (XGetSelectionOwner(dpy, netatom[NetSystemTray]) != None) {
		fprintf(stderr, "dwm: another system tray is already running\n");
		return;
	}
	systray = ecalloc(1, sizeof *systray);
	systray->win = XCreateWindow(dpy, root, 0, 0, 1, bh, 0,
		DefaultDepth(dpy, screen), CopyFromParent, DefaultVisual(dpy, screen),
		CWOverrideRedirect|CWBackPixel|CWEventMask, &wa);
	XChangeProperty(dpy, systray->win, netatom[NetSystemTrayOrientation], XA_CARDINAL,
		32, PropModeReplace, (unsigned char *)&orientation, 1);
	XSetSelectionOwner(dpy, netatom[NetSystemTray], systray->win, CurrentTime);
	if (XGetSelectionOwner(dpy, netatom[NetSystemTray]) != systray->win) {
		fprintf(stderr, "dwm: unable to obtain system tray\n");
		XDestroyWindow(dpy, systray->win);
		free(systray);
		systray = NULL;
		return;
	}
	sendevent(root, xatom[Manager], StructureNotifyMask, CurrentTime,
		netatom[NetSystemTray], systray->win, 0, 0);
}

void
cleanupsystray(void)
{
	Client *i;

	if (!systray)
		return;
	while ((i = systray->icons)) {
		XUnmapWindow(dpy, i->win);
		XReparentWindow(dpy, i->win, root, 0, 0);
		XRemoveFromSaveSet(dpy, i->win);
		removesystrayicon(i);
	}
	XDestroyWindow(dpy, systray->win);
	free(systray);
	systray = NULL;
}

void
selectionclear(XEvent *e)
{
	if (systray && e->xselectionclear.selection == netatom[NetSystemTray]) {
		cleanupsystray();
		for (Monitor *m = mons; m; m = m->next)
			resizebarwin(m);
		drawbars();
	}
}

void
reparentnotify(XEvent *e)
{
	Client *i = wintosystrayicon(e->xreparent.window);

	if (i && e->xreparent.parent != systray->win) {
		XRemoveFromSaveSet(dpy, i->win);
		removesystrayicon(i);
		drawbars();
	}
}

void
updatesystrayicongeom(Client *i, int w, int h)
{
	if (!i)
		return;
	w = MAX(1, w);
	h = MAX(1, h);
	i->h = bh;
	i->w = MAX(1, MIN(4 * bh, (int)MIN(4.0 * bh, (double)bh * w / h)));
}

void
updatesystrayiconstate(Client *i, XPropertyEvent *ev)
{
	unsigned int mapped;

	if (!systray || !i || ev->atom != xatom[XembedInfo])
		return;
	mapped = !!(getatomprop(i, xatom[XembedInfo]) & XEMBED_MAPPED);
	if (mapped == i->tags)
		return;
	i->tags = mapped;
	if (mapped)
		XMapRaised(dpy, i->win);
	else
		XUnmapWindow(dpy, i->win);
	setclientstate(i, mapped ? NormalState : WithdrawnState);
	sendevent(i->win, xatom[Xembed], NoEventMask, CurrentTime,
		mapped ? XEMBED_WINDOW_ACTIVATE : XEMBED_WINDOW_DEACTIVATE,
		0, systray->win, XEMBED_EMBEDDED_VERSION);
}

void
updatesystray(void)
{
	Client *i;
	Monitor *m;
	int x = systrayspacing, w;
	XWindowChanges wc;

	if (!systray || !(m = systraytomon(NULL)))
		return;
	w = getsystraywidth();
	if (!m->showbar || !w) {
		XUnmapWindow(dpy, systray->win);
		return;
	}
	for (i = systray->icons; i; i = i->next) {
		i->mon = m;
		if (!i->tags)
			continue;
		i->x = x;
		XMoveResizeWindow(dpy, i->win, x, 0, i->w, i->h);
		XMapRaised(dpy, i->win);
		x += i->w + systrayspacing;
	}
	XSetWindowBackground(dpy, systray->win, scheme[SchemeNorm][ColBg].pixel);
	XMoveResizeWindow(dpy, systray->win, m->wx + m->ww - w, m->by, w, bh);
	wc.stack_mode = Above;
	wc.sibling = m->barwin;
	XConfigureWindow(dpy, systray->win, CWSibling|CWStackMode, &wc);
	XMapWindow(dpy, systray->win);
	XClearWindow(dpy, systray->win);
}

void
updatetitle(Client *c)
{
	if (!gettextprop(c->win, netatom[NetWMName], c->name, sizeof c->name))
		gettextprop(c->win, XA_WM_NAME, c->name, sizeof c->name);
	if (c->name[0] == '\0') /* hack to mark broken clients */
		strcpy(c->name, broken);
	drawtitlebar(c);
}

void
updatewindowtype(Client *c)
{
	Atom state = getatomprop(c, netatom[NetWMState]);
	Atom wtype = getatomprop(c, netatom[NetWMWindowType]);

	if (state == netatom[NetWMFullscreen])
		setfullscreen(c, 1);
	if (wtype == netatom[NetWMWindowTypeDialog])
		c->alwaysfloating = c->isfloating = 1;
}

void
updatewmhints(Client *c)
{
	XWMHints *wmh;

	if ((wmh = XGetWMHints(dpy, c->win))) {
		if (c == selmon->sel && wmh->flags & XUrgencyHint) {
			wmh->flags &= ~XUrgencyHint;
			XSetWMHints(dpy, c->win, wmh);
		} else
			c->isurgent = (wmh->flags & XUrgencyHint) ? 1 : 0;
		if (wmh->flags & InputHint)
			c->neverfocus = !wmh->input;
		else
			c->neverfocus = 0;
		XFree(wmh);
	}
}

void
view(const Arg *arg)
{
	if ((arg->ui & TAGMASK) == selmon->tagset[selmon->seltags])
		return;
	selmon->seltags ^= 1; /* toggle sel tagset */
	if (arg->ui & TAGMASK)
		selmon->tagset[selmon->seltags] = arg->ui & TAGMASK;
	focus(NULL);
	arrange(selmon);
}

Client *
wintoclient(Window w)
{
	Client *c;
	Monitor *m;

	for (m = mons; m; m = m->next)
		for (c = m->clients; c; c = c->next)
			if (c->win == w)
				return c;
	return NULL;
}

Client *
wintosystrayicon(Window w)
{
	Client *i = NULL;

	if (!systray || !w)
		return i;
	for (i = systray->icons; i && i->win != w; i = i->next) ;
	return i;
}

Monitor *
wintomon(Window w)
{
	int x, y;
	Client *c;
	Monitor *m;

	if (w == root && getrootptr(&x, &y))
		return recttomon(x, y, 1, 1);
	for (m = mons; m; m = m->next)
		if (w == m->barwin)
			return m;
	if ((c = wintoclient(w)) || (c = titleclient(w)))
		return c->mon;
	return selmon;
}

/* There's no way to check accesses to destroyed windows, thus those cases are
 * ignored (especially on UnmapNotify's). Other types of errors call Xlibs
 * default error handler, which may call exit. */
int
xerror(Display *dpy, XErrorEvent *ee)
{
	if (ee->error_code == BadWindow
	    || (ee->request_code == X_SetInputFocus && ee->error_code == BadMatch)
	    || (ee->request_code == X_PolyText8 && ee->error_code == BadDrawable)
	    || (ee->request_code == X_PolyFillRectangle && ee->error_code == BadDrawable)
	    || (ee->request_code == X_PolySegment && ee->error_code == BadDrawable)
	    || (ee->request_code == X_ConfigureWindow && ee->error_code == BadMatch)
	    || (ee->request_code == X_GrabButton && ee->error_code == BadAccess)
	    || (ee->request_code == X_GrabKey && ee->error_code == BadAccess)
	    || (ee->request_code == X_CopyArea && ee->error_code == BadDrawable))
		return 0;
	fprintf(stderr, "dwm: fatal error: request code=%d, error code=%d\n",
		ee->request_code, ee->error_code);
	return xerrorxlib(dpy, ee); /* may call exit */
}

int
xerrordummy(Display *dpy, XErrorEvent *ee)
{
	return 0;
}

/* Startup Error handler to check if another window manager
 * is already running. */
int
xerrorstart(Display *dpy, XErrorEvent *ee)
{
	die("dwm: another window manager is already running");
	return -1;
}

Monitor *
systraytomon(Monitor *m)
{
	Monitor *t;
	int i, n;
	if (!systraypinning) {
		if (!m)
			return selmon;
		return m == selmon ? m : NULL;
	}
	for (n = 1, t = mons; t && t->next; n++, t = t->next) ;
	for (i = 1, t = mons; t && t->next && i < systraypinning; i++, t = t->next) ;
	if (systraypinningfailfirst && n < systraypinning)
		return mons;
	return t;
}

void
zoom(const Arg *arg)
{
	Client *c = selmon->sel;

	if (!selmon->lt[selmon->sellt]->arrange || !c || c->isfloating)
		return;
	if (c == nexttiled(selmon->clients) && !(c = nexttiled(c->next)))
		return;
	pop(c);
}

void
update_scheme(void)
{
	Monitor *m;
	for (size_t i = 0; i < LENGTH(colors); i++) {
		drw_scm_free(drw, scheme[i], 3);
		scheme[i] = drw_scm_create(drw, colors[i], 3);
	}
	for (m = mons; m; m = m->next)
		if (m->sel)
			XSetWindowBorder(dpy, m->sel->win, scheme[m == selmon ? SchemeSel : SchemeNorm][ColBorder].pixel);
	drawbars();
}

void
cycle_flycolors(const Arg *arg)
{
	iflycol = (iflycol + LENGTH(flycolors) + arg->i) % LENGTH(flycolors);
	colors[SchemeSel][1] = flycolors[iflycol];
	colors[SchemeSel][2] = flycolors[iflycol];
	update_scheme();
	dmenucmd[10] = flycolors[iflycol];
}

int
main(int argc, char *argv[])
{
	if (argc == 2 && !strcmp("-v", argv[1]))
		die("dwm-"VERSION);
	else if (argc != 1)
		die("usage: dwm [-v]");
	if (!setlocale(LC_CTYPE, "") || !XSupportsLocale())
		fputs("warning: no locale support\n", stderr);
	if (!(dpy = XOpenDisplay(NULL)))
		die("dwm: cannot open display");
	checkotherwm();
	setup();
#ifdef __OpenBSD__
	if (pledge("stdio rpath wpath cpath proc exec", NULL) == -1)
		die("pledge");
#endif /* __OpenBSD__ */
	scan();
	run();
	cleanup();
	XCloseDisplay(dpy);
	return EXIT_SUCCESS;
}
