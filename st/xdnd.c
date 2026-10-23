/* xdnd version 5; file drops use shell quoting and never invoke a command. */
static struct {
	Atom aware, enter, position, status, leave, drop, finished;
	Atom selection, typelist, copy, uri, utf8, plain, property;
	Window source;
	Atom target;
	int version;
} xdnd;

static char *
xdndpaths(const unsigned char *data, size_t length)
{
	char *text = xmalloc(length * 8 + 1), *out = text;
	size_t start, end, i;
	int c, a, b, file;
	for (start = 0; start < length; start = end + 1) {
		for (end = start; end < length && data[end] != '\n'; end++) {}
		i = end;
		if (i > start && data[i - 1] == '\r')
			i--;
		if (i == start || data[start] == '#')
			continue;
		file = i - start >= 7 && !memcmp(data + start, "file://", 7);
		if (file) {
			start += 7;
			if (i - start >= 10 && !memcmp(data + start, "localhost/", 10))
				start += 9;
			if (start == i || data[start] != '/')
				goto invalid;
		}
		*out++ = '\'';
		for (; start < i; start++) {
			c = data[start];
			if (file && c == '%') {
				if (start + 2 >= i || !isxdigit(data[start + 1]) || !isxdigit(data[start + 2]))
					goto invalid;
				a = tolower(data[++start]);
				b = tolower(data[++start]);
				c = (a <= '9' ? a - '0' : a - 'a' + 10) * 16 +
				    (b <= '9' ? b - '0' : b - 'a' + 10);
			}
			if (c == 0 || c == 0x1b || c == 0x7f || (c < 32 && c != '\n' && c != '\t'))
				goto invalid;
			if (c == '\'') {
				memcpy(out, "'\\''", 4);
				out += 4;
			} else {
				*out++ = c;
			}
		}
		*out++ = '\'';
		*out++ = ' ';
	}
	*out = '\0';
	return text;
invalid:
	free(text);
	return NULL;
}

static void
xdndsend(Atom message, int accepted)
{
	XEvent reply = {0};
	reply.xclient.type = ClientMessage;
	reply.xclient.display = xw.dpy;
	reply.xclient.window = xdnd.source;
	reply.xclient.message_type = message;
	reply.xclient.format = 32;
	reply.xclient.data.l[0] = xw.win;
	reply.xclient.data.l[1] = accepted;
	reply.xclient.data.l[message == xdnd.status ? 4 : 2] = accepted ? xdnd.copy : None;
	XSendEvent(xw.dpy, xdnd.source, False, NoEventMask, &reply);
	XFlush(xw.dpy);
}

static void
xdndinit(void)
{
	unsigned long version = 5;
	const char *names[] = { "XdndAware", "XdndEnter", "XdndPosition", "XdndStatus",
		"XdndLeave", "XdndDrop", "XdndFinished", "XdndSelection", "XdndTypeList",
		"XdndActionCopy", "text/uri-list", "UTF8_STRING", "text/plain", "ST_XDND" };
	Atom *atoms[] = { &xdnd.aware, &xdnd.enter, &xdnd.position, &xdnd.status,
		&xdnd.leave, &xdnd.drop, &xdnd.finished, &xdnd.selection, &xdnd.typelist,
		&xdnd.copy, &xdnd.uri, &xdnd.utf8, &xdnd.plain, &xdnd.property };
	size_t i;
	for (i = 0; i < LEN(names); i++)
		*atoms[i] = XInternAtom(xw.dpy, names[i], False);
	XChangeProperty(xw.dpy, xw.win, xdnd.aware, XA_ATOM, 32,
		PropModeReplace, (unsigned char *)&version, 1);
}

static int
xdndmessage(XClientMessageEvent *event)
{
	Atom type, types[3], *offered = types;
	unsigned char *data = NULL;
	unsigned long count = 3, remaining, i;
	int format;
	if (event->format != 32)
		return 0;
	if (event->message_type == xdnd.enter) {
		xdnd.source = event->data.l[0];
		xdnd.version = (unsigned long)event->data.l[1] >> 24;
		xdnd.target = None;
		if (xdnd.version < 3 || xdnd.version > 5)
			return 1;
		for (i = 0; i < 3; i++)
			types[i] = event->data.l[i + 2];
		if (event->data.l[1] & 1) {
			if (XGetWindowProperty(xw.dpy, xdnd.source, xdnd.typelist, 0, 256,
			    False, XA_ATOM, &type, &format, &count, &remaining, &data) != Success ||
			    type != XA_ATOM || format != 32 || remaining) {
				if (data) XFree(data);
				return 1;
			}
			offered = (Atom *)data;
		}
		for (i = 0; i < count; i++) {
			if (offered[i] == xdnd.uri) {
				xdnd.target = xdnd.uri;
				break;
			}
			if (offered[i] == xdnd.utf8 || offered[i] == xdnd.plain)
				xdnd.target = offered[i];
		}
		if (data) XFree(data);
		return 1;
	}
	if (!xdnd.source || event->data.l[0] != (long)xdnd.source)
		return 0;
	if (event->message_type == xdnd.position) {
		xdndsend(xdnd.status, xdnd.target != None);
		return 1;
	}
	if (event->message_type == xdnd.leave) {
		xdnd.source = None;
		return 1;
	}
	if (event->message_type == xdnd.drop) {
		if (xdnd.target) {
			XConvertSelection(xw.dpy, xdnd.selection, xdnd.target, xdnd.property,
				xw.win, event->data.l[2]);
		} else {
			xdndsend(xdnd.finished, 0);
			xdnd.source = None;
		}
		return 1;
	}
	return 0;
}

static void
xdndreceive(XSelectionEvent *event)
{
	Atom type;
	unsigned char *data = NULL;
	char *paths = NULL;
	unsigned long count, remaining;
	int format, accepted = 0;
	if (!xdnd.source)
		return;
	if (event->property == xdnd.property &&
	    XGetWindowProperty(xw.dpy, xw.win, xdnd.property, 0, 262144, True,
	    AnyPropertyType, &type, &format, &count, &remaining, &data) == Success &&
	    type == xdnd.target && format == 8 && data && count && !remaining && !memchr(data, 0, count)) {
		if (type == xdnd.uri)
			paths = xdndpaths(data, count);
		if (type != xdnd.uri || paths) {
			autocomplete(&(Arg){.i = ACMPL_DEACTIVATE});
			if (IS_SET(MODE_BRCKTPASTE)) ttywrite("\033[200~", 6, 0);
			ttywrite(paths ? paths : (char *)data, paths ? strlen(paths) : count, 1);
			if (IS_SET(MODE_BRCKTPASTE)) ttywrite("\033[201~", 6, 0);
			accepted = 1;
		}
	}
	free(paths);
	if (data) XFree(data);
	XDeleteProperty(xw.dpy, xw.win, xdnd.property);
	xdndsend(xdnd.finished, accepted);
	xdnd.source = None;
}
