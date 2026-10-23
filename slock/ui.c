struct panel {
	int x, y, width, height;
};

static struct panel
panelrect(struct lock *lock, int monitor)
{
	int x = 0, y = 0, width = lock->width, height = lock->height;
	struct panel panel;

	if (lock->nmonitors) {
		x = lock->monitors[monitor].x;
		y = lock->monitors[monitor].y;
		width = lock->monitors[monitor].width;
		height = lock->monitors[monitor].height;
	}
	panel.width = width > promptwidth + 40 ? promptwidth : width - 40;
	if (panel.width < 40)
		panel.width = 40;
	panel.x = x + (width - panel.width) / 2;
	panel.height = lock->font->ascent + lock->font->descent + 2;
	panel.y = y + (height - panel.height) / 2;
	return panel;
}

static void
resizeui(Display *dpy, struct lock *lock)
{
	int major, minor;

	if (lock->draw)
		XftDrawDestroy(lock->draw);
	if (lock->buffer)
		XFreePixmap(dpy, lock->buffer);
	if (lock->monitors)
		XRRFreeMonitors(lock->monitors);
	lock->nmonitors = 0;
	lock->monitors = NULL;
	if (XRRQueryVersion(dpy, &major, &minor) &&
	    (major > 1 || (major == 1 && minor >= 5)))
		lock->monitors = XRRGetMonitors(dpy, lock->root, True, &lock->nmonitors);
	lock->buffer = XCreatePixmap(dpy, lock->win, lock->width, lock->height,
		DefaultDepth(dpy, lock->screen));
	lock->draw = XftDrawCreate(dpy, lock->buffer, DefaultVisual(dpy, lock->screen),
		DefaultColormap(dpy, lock->screen));
	if (!lock->draw)
		die("slock: cannot create lock drawing surface\n");
}

static void
drawlock(Display *dpy, struct lock *lock, unsigned int count, int state, int caps, int cursor)
{
	struct panel panel;
	XGlyphInfo extents, inputextents;
	unsigned int dots, j;
	int i, baseline, labelwidth, padding, inputx, cursorx;
	char label[sizeof account + 2];
	const char *message;

	snprintf(label, sizeof label, "%s:", account);
	padding = (lock->font->ascent + lock->font->descent) / 2;
	XftTextExtentsUtf8(dpy, lock->font, (const FcChar8 *)label, strlen(label), &extents);
	XSetForeground(dpy, lock->gc, lock->colors[Background].pixel);
	XFillRectangle(dpy, lock->buffer, lock->gc, 0, 0, lock->width, lock->height);
	for (i = 0; i < (lock->nmonitors ? lock->nmonitors : 1); i++) {
		panel = panelrect(lock, i);
		baseline = panel.y + 1 + lock->font->ascent;
		labelwidth = extents.xOff + 2 * padding;
		if (labelwidth > panel.width / 2)
			labelwidth = panel.width / 2;
		XSetForeground(dpy, lock->gc, lock->colors[Field].pixel);
		XFillRectangle(dpy, lock->buffer, lock->gc, panel.x, panel.y, panel.width, panel.height);
		XSetForeground(dpy, lock->gc, lock->colors[Accent].pixel);
		XFillRectangle(dpy, lock->buffer, lock->gc, panel.x, panel.y, labelwidth, panel.height);
		XRectangle clip = {panel.x, panel.y, labelwidth, panel.height};
		XftDrawSetClipRectangles(lock->draw, 0, 0, &clip, 1);
		XftDrawStringUtf8(lock->draw, &lock->colors[SelectedText], lock->font,
			panel.x + padding, baseline, (const FcChar8 *)label, strlen(label));
		inputx = panel.x + labelwidth + padding;
		clip.x = panel.x + labelwidth;
		clip.width = panel.width - labelwidth;
		XftDrawSetClipRectangles(lock->draw, 0, 0, &clip, 1);
		if (count) {
			dots = (panel.width - labelwidth - padding) / 12;
			if (dots > count)
				dots = count;
			XSetForeground(dpy, lock->gc, lock->colors[Text].pixel);
			for (j = 0; j < dots; j++)
				XFillArc(dpy, lock->buffer, lock->gc,
					inputx + j * 12, panel.y + panel.height / 2 - 2, 4, 4, 0, 360 * 64);
			cursorx = inputx + dots * 12;
		} else {
			XftDrawStringUtf8(lock->draw, &lock->colors[Text], lock->font,
				inputx, baseline, (const FcChar8 *)"password", 8);
			XftTextExtentsUtf8(dpy, lock->font, (const FcChar8 *)"password", 8, &inputextents);
			cursorx = inputx + inputextents.xOff;
		}
		if (cursor && count)
			XftDrawStringUtf8(lock->draw, &lock->colors[Text], lock->font,
				cursorx, baseline, (const FcChar8 *)"_", 1);
		XftDrawSetClip(lock->draw, NULL);
		message = state == FAILED ? "incorrect password" :
			state == CHECKING ? "checking..." : caps ? "caps lock" : "";
		if (*message)
			XftDrawStringUtf8(lock->draw, &lock->colors[state == FAILED ? Error : Text], lock->font,
				panel.x, baseline + panel.height + 4, (const FcChar8 *)message, strlen(message));
	}
	XCopyArea(dpy, lock->buffer, lock->win, lock->gc, 0, 0, lock->width, lock->height, 0, 0);
	XFlush(dpy);
}
