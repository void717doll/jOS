#define main tabbed_main
#include "../tabbed.c"
#undef main
#include <assert.h>

static Window closedwindow, killedwindow;
static Bool supportsdelete = True;

Status
__wrap_XGetWMProtocols(Display *display, Window window, Atom **protocols, int *count)
{
	if (!supportsdelete)
		return 0;
	*protocols = malloc(sizeof(Atom));
	(*protocols)[0] = wmatom[WMDelete];
	*count = 1;
	return 1;
}

Status
__wrap_XSendEvent(Display *display, Window window, Bool propagate, long mask, XEvent *event)
{
	assert(event->type == ClientMessage);
	assert(event->xclient.message_type == wmatom[WMProtocols]);
	assert(event->xclient.data.l[0] == wmatom[WMDelete]);
	closedwindow = window;
	return 1;
}

int
__wrap_XKillClient(Display *display, XID resource)
{
	killedwindow = resource;
	return 1;
}

int
main(void)
{
	Client first = {.win = 10}, second = {.win = 11}, third = {.win = 12};
	Client *windows[] = {&first, &second, &third};
	XEvent event = {.xclient = {.type = ClientMessage, .window = 20, .format = 32}};

	clients = windows;
	nclients = 3;
	sel = 1;
	win = 20;
	wmatom[WMProtocols] = event.xclient.message_type = 30;
	wmatom[WMDelete] = event.xclient.data.l[0] = 31;
	groupmode = running = True;
	clientmessage(&event);
	assert(running && nclients == 3 && closedwindow == second.win);
	assert(second.closed && !first.closed && !third.closed && !killedwindow);
	clientmessage(&event);
	assert(running && killedwindow == second.win);

	/* the final tab must close before its container exits. */
	nclients = 1;
	sel = 0;
	closedwindow = killedwindow = None;
	clientmessage(&event);
	assert(running && closedwindow == first.win && !killedwindow);
	first.closed = False;
	supportsdelete = False;
	clientmessage(&event);
	assert(running && killedwindow == first.win);
	nclients = 0;
	sel = -1;
	clientmessage(&event);
	assert(!running);

	/* ordinary tabbed containers keep their existing close policy. */
	groupmode = False;
	nclients = 3;
	sel = 2;
	running = True;
	clientmessage(&event);
	assert(!running);
	running = killclientsfirst = True;
	clientmessage(&event);
	assert(running && killedwindow == third.win);
	puts("closing a group targets its selected tab, including the final tab: passed (x calls mocked)");
	return 0;
}
