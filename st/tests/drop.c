#define main st_xmain
#include "../x.c"
#undef main
#include <assert.h>

static XEvent reply;
static Atom converted;
static char pasted[2048];
static const char *payload;

int __wrap_XSendEvent(Display *display, Window window, Bool propagate, long mask, XEvent *event)
{
	reply = *event;
	return 1;
}
int __wrap_XFlush(Display *display) { return 1; }
int __wrap_XDeleteProperty(Display *display, Window window, Atom property) { return 1; }
int __wrap_XConvertSelection(Display *display, Atom selection, Atom target, Atom property, Window requestor, Time time)
{
	assert(selection == xdnd.selection && property == xdnd.property);
	converted = target;
	return 1;
}
int __wrap_XGetWindowProperty(Display *display, Window window, Atom property, long offset, long length,
                             Bool delete, Atom requested, Atom *type, int *format,
                             unsigned long *count, unsigned long *remaining, unsigned char **data)
{
	*type = xdnd.uri;
	*format = 8;
	*count = strlen(payload);
	*remaining = 0;
	*data = (unsigned char *)strdup(payload);
	return Success;
}
void __wrap_ttywrite(const char *text, size_t length, int echo)
{
	assert(strlen(pasted) + length < sizeof pasted);
	strncat(pasted, text, length);
}

int
main(void)
{
	const char *uris = "# files\r\nfile:///tmp/a%20b.txt\r\nfile://localhost/tmp/it%27s%24.txt\r\n";
	char *paths = xdndpaths((const unsigned char *)uris, strlen(uris));
	assert(paths && !strcmp(paths, "'/tmp/a b.txt' '/tmp/it'\\''s$.txt' "));
	free(paths);
	uris = "https://example.test/a%20b?q=1&x=2\r\n";
	paths = xdndpaths((const unsigned char *)uris, strlen(uris));
	assert(paths && !strcmp(paths, "'https://example.test/a%20b?q=1&x=2' "));
	free(paths);
	uris = "file:///tmp/bad%00name\n";
	assert(!xdndpaths((const unsigned char *)uris, strlen(uris)));
	uris = "file:///tmp/bad%XYname\n";
	assert(!xdndpaths((const unsigned char *)uris, strlen(uris)));
	uris = "file://remote/tmp/file\n";
	assert(!xdndpaths((const unsigned char *)uris, strlen(uris)));
	xdnd.enter = 1;
	xdnd.position = 2;
	xdnd.drop = 3;
	xdnd.status = 4;
	xdnd.finished = 5;
	xdnd.selection = 6;
	xdnd.uri = 7;
	xdnd.property = 8;
	xdnd.copy = 9;
	xw.win = 100;
	XClientMessageEvent event = {.format = 32, .message_type = xdnd.enter,
		.data.l = {200, 5L << 24, 7, 0, 0}};
	assert(xdndmessage(&event) && xdnd.target == xdnd.uri);
	event.message_type = xdnd.position;
	assert(xdndmessage(&event));
	assert(reply.xclient.message_type == xdnd.status && reply.xclient.data.l[1] == 1);
	event.message_type = xdnd.drop;
	assert(xdndmessage(&event) && converted == xdnd.uri);
	payload = "file:///tmp/a%20b.txt\r\n";
	win.mode = MODE_BRCKTPASTE;
	XSelectionEvent selection = {.property = xdnd.property};
	xdndreceive(&selection);
	assert(!strcmp(pasted, "\033[200~'/tmp/a b.txt' \033[201~"));
	assert(reply.xclient.message_type == xdnd.finished && reply.xclient.data.l[1] == 1);
	assert(xdnd.source == None);
	/* a rejected transfer must still tell the file manager it has finished. */
	xdnd.source = 200;
	selection.property = None;
	xdndreceive(&selection);
	assert(reply.xclient.data.l[1] == 0 && xdnd.source == None);
	puts("file drop decoding, multiple files, quoting and invalid uri rejection: passed");
	return 0;
}
