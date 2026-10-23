#define main tabbed_main
#include "../tabbed.c"
#undef main
#include <assert.h>

static int configured, notified;
static XWindowAttributes actual;

Status __wrap_XGetWindowAttributes(Display *display, Window window, XWindowAttributes *attributes)
{
	*attributes = actual;
	return 1;
}

int __wrap_XConfigureWindow(Display *display, Window window, unsigned int mask, XWindowChanges *changes)
{
	if (mask & CWX) actual.x = changes->x;
	if (mask & CWY) actual.y = changes->y;
	if (mask & CWWidth) actual.width = changes->width;
	if (mask & CWHeight) actual.height = changes->height;
	if (mask & CWBorderWidth) actual.border_width = changes->border_width;
	configured++;
	return 1;
}

Status __wrap_XSendEvent(Display *display, Window window, Bool propagate, long mask, XEvent *event)
{
	assert(event->type == ConfigureNotify);
	notified++;
	return 1;
}

int
main(void)
{
	Client client = {.win = 10};
	Client *windows[] = {&client};
	XEvent request = {.xconfigurerequest = {.window = 10, .value_mask = CWWidth | CWHeight | CWY}};

	clients = windows;
	nclients = 1;
	bh = 24;
	ww = 800;
	wh = 624;
	resize(0, 800, 600);
	for (int i = 0; i < 100; i++)
		resize(0, 800, 600);
	assert(configured == 1 && notified == 1);
	resize(0, 900, 600);
	assert(configured == 2 && notified == 2);
	bh = 0;
	resize(0, 900, 600);
	assert(configured == 3 && notified == 3);
	configurerequest(&request);
	resize(0, ww, wh - bh);
	assert(configured == 4 && notified == 3);
	/* a client can drift even when the last requested size still matches. */
	actual.x = 70;
	actual.y = 80;
	actual.width = 400;
	actual.border_width = 2;
	resize(0, ww, wh - bh);
	assert(actual.x == 0 && actual.y == bh && actual.width == ww);
	assert(actual.height == wh - bh && actual.border_width == 0);
	assert(configured == 5 && notified == 4);
	resize(0, ww, wh - bh);
	assert(configured == 5 && notified == 4);
	resize(0, 400, 300);
	resize(0, ww, wh - bh);
	assert(actual.width == ww && actual.height == wh - bh);
	puts("tab geometry restores after resizing and repairs drift without redundant configure requests: passed (x calls mocked)");
	return 0;
}
