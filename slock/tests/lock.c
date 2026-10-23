#define main slock_main
#include "../slock.c"
#undef main
#include <assert.h>

static const char *keys;
static size_t keyindex;
static int bells, off, on, polls, failedtext, checkingtext, capstext, masks, frames, cursors;
static unsigned long foreground;
static time_t seconds;

int __wrap_clock_gettime(clockid_t clock, struct timespec *value)
{
	value->tv_sec = seconds;
	value->tv_nsec = 0;
	return 0;
}
int __wrap_poll(struct pollfd *fds, nfds_t count, int timeout)
{
	assert(timeout == timeoff * 1000 && count == 1);
	seconds = timeoff + 1;
	polls++;
	return 0;
}
int __wrap_XPending(Display *dpy) { return seconds != 0; }
int __wrap_XNextEvent(Display *dpy, XEvent *event)
{
	assert(keyindex < strlen(keys));
	memset(event, 0, sizeof *event);
	if (keys[keyindex] == '#') {
		event->type = ButtonPress;
		event->xbutton.button = Button1;
		event->xbutton.x_root = 400;
		event->xbutton.y_root = 380;
		keyindex++;
	} else {
		event->type = KeyPress;
		event->xkey.keycode = keys[keyindex] == '^' ? '\b' : keys[keyindex];
		event->xkey.state = keys[keyindex] == '^' ? ControlMask : 0;
		keyindex++;
	}
	return 0;
}
int __wrap_XLookupString(XKeyEvent *event, char *buf, int size, KeySym *key, XComposeStatus *status)
{
	buf[0] = event->keycode;
	*key = buf[0] == '\r' ? XK_Return : buf[0] == '\b' ? XK_BackSpace :
		buf[0] == '\033' ? XK_Escape : (KeySym)buf[0];
	return 1;
}
Status __wrap_XkbGetState(Display *dpy, unsigned int device, XkbStatePtr state)
{
	memset(state, 0, sizeof *state);
	state->locked_mods = LockMask;
	return Success;
}
Bool __wrap_DPMSCapable(Display *dpy) { return True; }
Status __wrap_DPMSEnable(Display *dpy) { return 1; }
Status __wrap_DPMSForceLevel(Display *dpy, CARD16 level)
{
	if (level == DPMSModeOff) off++;
	if (level == DPMSModeOn) on++;
	return 1;
}
int __wrap_XFlush(Display *dpy) { return 1; }
int __wrap_XBell(Display *dpy, int percent) { bells++; return 1; }
int __wrap_XRaiseWindow(Display *dpy, Window window) { return 1; }
int __wrap_XResizeWindow(Display *dpy, Window window, unsigned int width, unsigned int height) { return 1; }
int __wrap_XSetForeground(Display *dpy, GC gc, unsigned long pixel)
{
	foreground = pixel;
	return 1;
}
int __wrap_XFillRectangle(Display *dpy, Drawable draw, GC gc, int x, int y,
	unsigned int width, unsigned int height)
{
	if (foreground == Background) {
		assert(x == 0 && y == 0 && width == 800 && height == 600);
		frames++;
	} else {
		assert(foreground == Field || foreground == Accent);
		assert(height == 22 && width <= 300);
	}
	return 1;
}
int __wrap_XFillArc(Display *dpy, Drawable draw, GC gc, int x, int y,
	unsigned int width, unsigned int height, int a, int b)
{
	assert(width == 4 && height == 4 && foreground == Text);
	masks++;
	return 1;
}
int __wrap_XCopyArea(Display *dpy, Drawable from, Drawable to, GC gc,
	int x, int y, unsigned int width, unsigned int height, int dx, int dy) { return 1; }
Bool __wrap_XftDrawSetClip(XftDraw *draw, Region region) { return True; }
Bool __wrap_XftDrawSetClipRectangles(XftDraw *draw, int x, int y, const XRectangle *rectangles, int count)
{
	assert(count == 1 && rectangles[0].height == 22);
	return True;
}
void __wrap_XftTextExtentsUtf8(Display *dpy, XftFont *font, const FcChar8 *text,
	int len, XGlyphInfo *extents)
{
	memset(extents, 0, sizeof *extents);
	extents->xOff = len * 10;
}
void __wrap_XftDrawStringUtf8(XftDraw *draw, const XftColor *color, XftFont *font,
	int x, int y, const FcChar8 *text, int len)
{
	const char *value = (const char *)text;
	assert(!strcmp(value, "test:") || !strcmp(value, "password") || !strcmp(value, "_") ||
		!strcmp(value, "incorrect password") || !strcmp(value, "checking...") ||
		!strcmp(value, "caps lock"));
	if (!strcmp(value, "_")) cursors++;
	if (!strcmp(value, "incorrect password")) {
		assert(color->pixel == Error);
		failedtext++;
	}
	if (!strcmp(value, "checking...")) checkingtext++;
	if (!strcmp(value, "caps lock")) capstext++;
}

int main(void)
{
	_XPrivDisplay display = calloc(1, sizeof *display);
	struct xrandr rr = {0};
	XftFont font = {.ascent = 16, .descent = 4};
	struct lock lock = {.width = 800, .height = 600, .font = &font}, *locks[] = {&lock};
	XRRMonitorInfo monitors[2] = {{.width = 800, .height = 600},
		{.x = 800, .width = 800, .height = 600}};
	char *hash = strdup(crypt("secret", "$6$test$"));
	int i;

	assert(display && hash);
	strcpy(account, "test");
	for (i = 0; i < NUMCOLS; i++)
		lock.colors[i].pixel = i;
	drawlock((Display *)display, &lock, 0, INIT, 0, 0);
	assert(cursors == 0);
	drawlock((Display *)display, &lock, 0, INIT, 0, 1);
	assert(cursors == 0);
	drawlock((Display *)display, &lock, 1, INPUT, 0, 1);
	assert(cursors == 1);
	keys = "x\rsecret\r";
	readpw((Display *)display, &rr, locks, 1, hash);
	assert(polls == 1 && off == 1 && on == 1);
	assert(bells == 1 && keyindex == strlen(keys));
	assert(failedtext == 1 && checkingtext == 2 && capstext > 0 && masks > 0 && frames > 5);

	/* ctrl+backspace clears the input before enter authenticates. */
	keys = "wrong^secret\r";
	keyindex = 0;
	readpw((Display *)display, &rr, locks, 1, hash);
	assert(bells == 1 && checkingtext == 3 && keyindex == strlen(keys));
	lock.monitors = monitors;
	lock.nmonitors = 2;
	assert(panelrect(&lock, 0).x == 250 && panelrect(&lock, 1).x == 1050);
	assert(panelrect(&lock, 0).height == font.ascent + font.descent + 2);
	free(hash);
	free(display);
	puts("compact themed ui, masked input, feedback, editing, monitors, timeout and authentication: passed (x11 mocked)");
	return 0;
}
