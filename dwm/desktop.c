/* local desktop behavior; included after config.h */
/* client geometry includes the title; x11 application geometry excludes it. */
static int
titleheight(Client *c)
{
	return titles && c->titlebar && !c->isfullscreen ? titlebarheight + 2 * c->bw : 0;
}

static Client *
titleclient(Window window)
{
	Monitor *m;
	Client *c;

	if (!window)
		return NULL;
	for (m = mons; m; m = m->next)
		for (c = m->clients; c; c = c->next)
			if (c->titlebar == window)
				return c;
	return NULL;
}

static void
createtitlebar(Client *c)
{
	XClassHint hint = {0};
	int tabbed = 0;
	XSetWindowAttributes attributes = {
		.override_redirect = True,
		.event_mask = ExposureMask|ButtonPressMask|EnterWindowMask,
		.background_pixel = titlescheme[0][ColBg].pixel
	};

	if (XGetClassHint(dpy, c->win, &hint)) {
		tabbed = hint.res_class && !strcmp(hint.res_class, "tabbed");
		if (hint.res_class)
			XFree(hint.res_class);
		if (hint.res_name)
			XFree(hint.res_name);
	}
	if (tabbed)
		return;
	/* berry keeps a separate decoration window behind the application. */
	c->titlebar = XCreateWindow(dpy, root, c->x, c->y, MAX(1, c->w), MAX(1, c->h),
		c->bw, DefaultDepth(dpy, screen), CopyFromParent, DefaultVisual(dpy, screen),
		CWOverrideRedirect|CWEventMask|CWBackPixel, &attributes);
	XDefineCursor(dpy, c->titlebar, cursor[CurNormal]->cursor);
}

static void
drawtitlebar(Client *c)
{
	int selected, height;
	Pixmap border;
	GC gc;

	if (!titles || !c->titlebar || !c->titlemapped || c->isfullscreen)
		return;
	selected = c->mon == selmon && c == selmon->sel;
	XSetWindowBorder(dpy, c->titlebar, titlescheme[selected][ColBorder].pixel);
	if (c->bw) {
		height = MAX(1, c->h - titleheight(c)) + 2 * c->bw;
		/* the client may already be gone while DestroyNotify is still queued. */
		border = XCreatePixmap(dpy, root, 1, height, c->depth);
		gc = XCreateGC(dpy, border, 0, NULL);
		XSetForeground(dpy, gc, scheme[selected ? SchemeSel : SchemeNorm][ColBorder].pixel);
		XFillRectangle(dpy, border, gc, 0, 0, 1, height);
		/* the top border is at negative y and wraps to the tile's last rows. */
		XSetForeground(dpy, gc, titlescheme[selected][ColBg].pixel);
		XFillRectangle(dpy, border, gc, 0, height - c->bw, 1, c->bw);
		XSetWindowBorderPixmap(dpy, c->win, border);
		XFreeGC(dpy, gc);
		XFreePixmap(dpy, border);
	}
	/* stacked windows use tabbed's labels; ordinary frames stay blank. */
	XSetWindowBackground(dpy, c->titlebar, titlescheme[selected][ColBg].pixel);
	XClearWindow(dpy, c->titlebar);
}

static void
updatetitlebar(Client *c)
{
	XWindowChanges stack = {.sibling = c->win, .stack_mode = Below};

	if (!c->titlebar)
		return;
	if (!titles || !ISVISIBLE(c) || c->isfullscreen) {
		if (c->titlemapped)
			XUnmapWindow(dpy, c->titlebar);
		c->titlemapped = 0;
		return;
	}
	XSetWindowBorderWidth(dpy, c->titlebar, c->bw);
	XMoveResizeWindow(dpy, c->titlebar, c->x, c->y, c->w, c->h);
	XConfigureWindow(dpy, c->titlebar, CWSibling|CWStackMode, &stack);
	if (!c->titlemapped)
		XMapWindow(dpy, c->titlebar);
	c->titlemapped = 1;
	drawtitlebar(c);
}

static void
toggletitles(const Arg *arg)
{
	Monitor *m;
	Client *c;

	titles = !titles;
	for (m = mons; m; m = m->next)
		for (c = m->clients; c; c = c->next) {
			c->h = MAX(c->h, titleheight(c) + 1);
			XSetWindowBorder(dpy, c->win,
				scheme[c == selmon->sel ? SchemeSel : SchemeNorm][ColBorder].pixel);
			updatehandles(c);
			XMoveResizeWindow(dpy, c->win, ISVISIBLE(c) ? c->x : -2 * WIDTH(c),
				c->y + titleheight(c), c->w, MAX(1, c->h - titleheight(c)));
			configure(c);
		}
}

static int
istabgroup(Client *c)
{
	XClassHint hint = {0};
	int group = 0;

	if (XGetClassHint(dpy, c->win, &hint)) {
		group = hint.res_name && !strcmp(hint.res_name, "dwm-tabs")
		     && hint.res_class && !strcmp(hint.res_class, "tabbed");
		if (hint.res_name)
			XFree(hint.res_name);
		if (hint.res_class)
			XFree(hint.res_class);
	}
	return group;
}

static void
tabmessage(Window container, int detach, Window client)
{
	XEvent event = {0};

	event.xclient.type = ClientMessage;
	event.xclient.window = container;
	event.xclient.message_type = XInternAtom(dpy, "_TABBED_GROUP", False);
	event.xclient.format = 32;
	event.xclient.data.l[0] = detach;
	event.xclient.data.l[1] = client;
	XSendEvent(dpy, container, False, NoEventMask, &event);
}

void
grouptabs(const Arg *arg)
{
	Client *selected = selmon->sel, *next;
	char first[32], second[32];
	const char *command[] = { "tabbed", "-c", "-G", first, "-G", second, NULL };
	int grouped, floating;

	if (!selected || selected->isfullscreen)
		return;
	grouped = istabgroup(selected);
	floating = selected->isfloating || !selected->mon->lt[selected->mon->sellt]->arrange;
	for (next = selected->next; next; next = next->next)
		if (ISVISIBLE(next) && (floating || !next->isfloating) && !next->isfullscreen
		    && !(grouped && istabgroup(next)))
			break;
	if (!next)
		for (next = selmon->clients; next && next != selected; next = next->next)
			if (ISVISIBLE(next) && (floating || !next->isfloating) && !next->isfullscreen
			    && !(grouped && istabgroup(next)))
				break;
	if (!next || next == selected)
		return;
	if (grouped)
		tabmessage(selected->win, 0, next->win);
	else if (istabgroup(next))
		tabmessage(next->win, 0, selected->win);
	else {
		snprintf(first, sizeof first, "0x%lx", selected->win);
		snprintf(second, sizeof second, "0x%lx", next->win);
		spawn(&(Arg){.v = command});
	}
}

void
ungrouptab(const Arg *arg)
{
	if (selmon->sel && istabgroup(selmon->sel))
		tabmessage(selmon->sel->win, 1, None);
	else
		resizefloating(&(Arg){.i = -50});
}

void
drawalttab(void)
{
	Client *c;
	int rows = MIN(tabcount, MAX(1, selmon->wh / bh));
	int first = tabindex / rows * rows;
	int width = MIN(600, selmon->ww);
	int height = rows * bh;
	int i;

	XMoveResizeWindow(dpy, tabwin, selmon->wx + (selmon->ww - width) / 2,
	                  selmon->wy + (selmon->wh - height) / 2, width, height);
	drw_resize(drw, MAX(sw, width), MAX(bh, height));
	drw_setscheme(drw, scheme[SchemeNorm]);
	drw_rect(drw, 0, 0, width, height, 1, 1);
	for (i = first; i < MIN(first + rows, tabcount); i++) {
		c = wintoclient(tablist[i]);
		drw_setscheme(drw, scheme[i == tabindex ? SchemeSel : SchemeNorm]);
		drw_text(drw, 0, (i - first) * bh, width, bh, lrpad / 2,
		         c ? c->name : "closed window", 0);
	}
	drw_map(drw, tabwin, 0, 0, width, height);
}

void
endalttab(int accept)
{
	Client *c;
	Window selected;

	if (!tabwin)
		return;
	selected = accept ? tablist[tabindex] : None;
	XUngrabKeyboard(dpy, CurrentTime);
	XDestroyWindow(dpy, tabwin);
	tabwin = None;
	free(tablist);
	tablist = NULL;
	tabcount = 0;
	if ((c = wintoclient(selected))) {
		unfocus(selmon->sel, 0);
		selmon = c->mon;
		c->ishidden = 0;
		selmon->showdesktop = 0;
		setclientstate(c, NormalState);
		if (!ISTAGVISIBLE(c))
			view(&(Arg){.ui = c->tags});
		focus(c);
		arrange(selmon);
	}
	drw_resize(drw, sw, bh);
	drawbars();
}

void
alttab(const Arg *arg)
{
	Client *c;
	Monitor *m;
	XClassHint selected = {0}, candidate = {0};
	XSetWindowAttributes wa = {.override_redirect = True, .event_mask = ExposureMask};
	int sameclass;

	if (tabwin)
		return;
	tabmode = abs(arg->i);
	if (tabmode == 2 && selmon->sel)
		XGetClassHint(dpy, selmon->sel->win, &selected);
	for (m = mons; m; m = m->next)
		for (c = m->stack; c; c = c->snext)
			tabcount++;
	if (!tabcount)
		return;
	tablist = ecalloc(tabcount, sizeof *tablist);
	tabcount = 0;
	for (m = mons; m; m = m->next) {
		for (c = m->stack; c; c = c->snext) {
			if (tabmode != 4 && (m != selmon || !ISTAGVISIBLE(c)))
				continue;
			if (tabmode == 2) {
				XGetClassHint(dpy, c->win, &candidate);
				sameclass = selected.res_class && candidate.res_class &&
				            !strcmp(selected.res_class, candidate.res_class);
				if (candidate.res_class)
					XFree(candidate.res_class);
				if (candidate.res_name)
					XFree(candidate.res_name);
				candidate = (XClassHint){0};
				if (!sameclass)
					continue;
			}
			tablist[tabcount++] = c->win;
		}
	}
	if (selected.res_class)
		XFree(selected.res_class);
	if (selected.res_name)
		XFree(selected.res_name);
	if (!tabcount || XGrabKeyboard(dpy, root, False, GrabModeAsync,
	                              GrabModeAsync, CurrentTime) != GrabSuccess) {
		free(tablist);
		tablist = NULL;
		tabcount = 0;
		return;
	}
	tabindex = tabmode >= 3 ? 0 : (arg->i < 0 ? tabcount - 1 : 1 % tabcount);
	tabwin = XCreateWindow(dpy, root, 0, 0, 1, 1, 0, CopyFromParent,
	                       CopyFromParent, CopyFromParent, CWOverrideRedirect|CWEventMask, &wa);
	XMapRaised(dpy, tabwin);
	drawalttab();
}

void
keyrelease(XEvent *e)
{
	KeySym key = XKeycodeToKeysym(dpy, (KeyCode)e->xkey.keycode, 0);

	if (tabwin && tabmode < 3 && (key == XK_Alt_L || key == XK_Alt_R))
		endalttab(1);
}

void
snapwindow(const Arg *arg)
{
	Client *c = selmon->sel;
	int x, y, w, h;

	if (!c || c->isfullscreen)
		return;
	if (c->snapmode == arg->i) {
		c->snapmode = 0;
		c->isfloating = c->savedfloating;
		resizeclient(c, c->savedx, c->savedy, c->savedw, c->savedh);
		arrange(c->mon);
		return;
	}
	if (!c->snapmode) {
		c->savedfloating = c->isfloating;
		c->savedx = c->x;
		c->savedy = c->y;
		c->savedw = c->w;
		c->savedh = c->h;
	}
	c->snapmode = arg->i;
	c->isfloating = 1;
	x = c->mon->wx;
	y = c->mon->wy;
	w = c->mon->ww;
	h = c->mon->wh;
	if (arg->i == 1 || arg->i == 2) {
		w /= 2;
		if (arg->i == 2)
			x += c->mon->ww - w;
	} else if (arg->i == 3 || arg->i == 4) {
		h /= 2;
		if (arg->i == 4)
			y += c->mon->wh - h;
	}
	resizeclient(c, x, y, MAX(1, w - 2 * c->bw), MAX(1, h - 2 * c->bw));
	arrange(c->mon);
}

static float
tileweight(Client *c)
{
	return c->tileweight > 0 ? c->tileweight : 1.0;
}

static float
tileweights(Client *c, int n)
{
	float total = 0;

	for (; c && n > 0; c = nexttiled(c->next), n--)
		total += tileweight(c);
	return total;
}

static float
bspratio(Client *c, int n, int first, int masters)
{
	float a, b;

	if (masters > 0 && masters < n)
		return c->mon->mfact;
	a = tileweights(c, first);
	b = (tileweights(c, n) - a) / (n - first);
	a /= first;
	return a / (a + b);
}

static void
resizeweights(Client *c, int first, int second, float ratio)
{
	float a = tileweights(c, first), total = tileweights(c, first + second);
	float target = total * ratio * first / (ratio * first + (1.0 - ratio) * second);
	float scale;
	int i;

	for (i = 0; i < first + second; i++, c = nexttiled(c->next)) {
		scale = i < first ? target / a : (total - target) / (total - a);
		c->tileweight = tileweight(c) * scale;
	}
}

static int
tileminimum(Client *c, int n, int horizontal)
{
	int size = 0;

	for (; c && n > 0; c = nexttiled(c->next), n--)
		size += (horizontal ? bh : MAX(bh, titleheight(c) + 1)) + 2 * c->bw + 1;
	return size;
}

static int
resizebspsplit(Client *c, int n, int index, int w, int h, int gap, int masters, int edge, int delta)
{
	Client *second = c;
	int first, split, horizontal, size, changed, minimum, maximum;
	float ratio;

	if (n < 2 || !delta)
		return 0;
	first = masters > 0 && masters < n ? masters : n / 2;
	horizontal = w > h;
	size = horizontal ? w : h;
	gap = MIN(gap, MAX(0, size - 2));
	ratio = bspratio(c, n, first, masters);
	split = MAX(1, MIN(size - gap - 1, (int)((size - gap) * ratio)));
	for (int i = 0; i < first; i++)
		second = nexttiled(second->next);
	if (index < first)
		changed = resizebspsplit(c, first, index, horizontal ? split : w,
			horizontal ? h : split, gap, 0, edge, delta);
	else
		changed = resizebspsplit(second, n - first, index - first,
			horizontal ? MAX(1, size - split - gap) : w,
			horizontal ? h : MAX(1, size - split - gap), gap, 0, edge, delta);
	if (changed)
		return 1;
	if (edge != (horizontal ? (index < first ? EdgeRight : EdgeLeft)
		: (index < first ? EdgeBottom : EdgeTop)))
		return 0;
	minimum = MAX(tileminimum(c, first, horizontal), (size - gap) * 0.05);
	maximum = MIN(size - gap - tileminimum(second, n - first, horizontal), (size - gap) * 0.95);
	if (minimum > maximum)
		return 0;
	ratio = (float)MAX(minimum, MIN(maximum, split + delta)) / MAX(1, size - gap);
	if (masters > 0 && masters < n)
		c->mon->mfact = ratio;
	else
		resizeweights(c, first, n - first, ratio);
	return 1;
}

static void
resizetiled(Client *c, int edges, int dx, int dy)
{
	Monitor *m = c->mon;
	Client *p, *previous = NULL, *upper, *lower;
	int n = 0, index = 0, gap, total, minimum, maximum;
	float ratio;

	edges &= c->tileedges;
	if (!edges || (!dx && !dy))
		return;
	for (p = nexttiled(m->clients); p; p = nexttiled(p->next)) {
		if (p == c)
			index = n;
		n++;
	}
	gap = MIN(m->gappx, MIN(m->ww / 4, m->wh / (n + 1)));
	if (m->lt[m->sellt]->arrange == bsptile) {
		if (edges & (EdgeLeft|EdgeRight))
			resizebspsplit(nexttiled(m->clients), n, index, MAX(1, m->ww - 2*gap),
				MAX(1, m->wh - 2*gap), gap, m->nmaster, edges & (EdgeLeft|EdgeRight), dx);
		if (edges & (EdgeTop|EdgeBottom))
			resizebspsplit(nexttiled(m->clients), n, index, MAX(1, m->ww - 2*gap),
				MAX(1, m->wh - 2*gap), gap, m->nmaster, edges & (EdgeTop|EdgeBottom), dy);
	} else if (m->lt[m->sellt]->arrange == tile) {
		if (edges & (EdgeLeft|EdgeRight))
			m->mfact = MAX(0.05, MIN(0.95, m->mfact + (float)dx / MAX(1, m->ww - gap)));
		if (dy && (edges & (EdgeTop|EdgeBottom))) {
			for (p = nexttiled(m->clients); p && p != c; p = nexttiled(p->next))
				previous = p;
			upper = edges & EdgeTop ? previous : c;
			lower = edges & EdgeTop ? c : nexttiled(c->next);
			if (upper && lower) {
				total = HEIGHT(upper) + HEIGHT(lower);
				minimum = MAX(tileminimum(upper, 1, 0), total * 0.05);
				maximum = MIN(total - tileminimum(lower, 1, 0), total * 0.95);
				if (minimum <= maximum) {
					ratio = (float)MAX(minimum, MIN(maximum, HEIGHT(upper) + dy)) / total;
					resizeweights(upper, 1, 1, ratio);
				}
			}
		}
	}
	arrange(m);
}

Client *
handleclient(Window window, int *edge)
{
	Monitor *m;
	Client *c;
	unsigned int i;

	if (!window)
		return NULL;
	for (m = mons; m; m = m->next)
		for (c = m->clients; c; c = c->next) {
			if (!c->handlesmapped)
				continue;
			for (i = 0; i < LENGTH(c->handles); i++)
				if ((c->handlemask & (1u << i)) && c->handles[i] == window) {
					if (edge)
						*edge = resizeedges[i];
					return c;
				}
		}
	return NULL;
}

void
destroyhandles(Client *c)
{
	unsigned int i;

	for (i = 0; i < LENGTH(c->handles); i++) {
		if (c->handles[i])
			XDestroyWindow(dpy, c->handles[i]);
		c->handles[i] = None;
	}
	c->handlesmapped = 0;
	c->handlemask = 0;
}

void
updatehandles(Client *c)
{
	XSetWindowAttributes attributes = {.override_redirect = True,
		.event_mask = ButtonPressMask|EnterWindowMask};
	XWindowChanges stack = {.sibling = c->win, .stack_mode = Above};
	unsigned int i;
	int floating, edges, border, b, grip, outside;

	if (!c->win)
		return;
	floating = !c->isfullscreen && (c->isfloating || !c->mon->lt[c->mon->sellt]->arrange);
	edges = floating ? EdgeLeft|EdgeRight|EdgeTop|EdgeBottom :
		(c->mon->lt[c->mon->sellt]->arrange == tile || c->mon->lt[c->mon->sellt]->arrange == bsptile)
		? c->tileedges : 0;
	border = c->isfullscreen ? 0 : borderpx;
	if (c->bw != border) {
		c->bw = border;
		XSetWindowBorderWidth(dpy, c->win, border);
		configure(c);
	}
	if (c->frameextent != titleheight(c) + 2 * c->bw) {
		unsigned long extents[] = {c->bw, c->bw, c->bw + titleheight(c), c->bw};
		XChangeProperty(dpy, c->win, netatom[NetWMFrameExtents], XA_CARDINAL, 32,
			PropModeReplace, (unsigned char *)extents, 4);
		c->frameextent = titleheight(c) + 2 * c->bw;
	}
	updatetitlebar(c);
	if (!edges || !ISVISIBLE(c) || !c->bw) {
		if (c->handlesmapped)
			for (i = 0; i < LENGTH(c->handles); i++)
				XUnmapWindow(dpy, c->handles[i]);
		c->handlesmapped = 0;
		c->handlemask = 0;
		return;
	}
	b = c->bw;
	grip = MAX(b, resizegrip);
	outside = grip - b;
	/* extend into the gap instead of covering scrollbars or other app controls. */
	int boxes[8][4] = {
		{c->x - outside, c->y + b, grip, c->h},
		{c->x + b + c->w, c->y + b, grip, c->h},
		{c->x + b, c->y - outside, c->w, grip},
		{c->x + b, c->y + b + c->h, c->w, grip},
		{c->x - outside, c->y - outside, grip, grip},
		{c->x + b + c->w, c->y - outside, grip, grip},
		{c->x - outside, c->y + b + c->h, grip, grip},
		{c->x + b + c->w, c->y + b + c->h, grip, grip}
	};
	for (i = 0; i < LENGTH(c->handles); i++) {
		/* keep a full grip reachable when the window meets a screen edge. */
		boxes[i][0] = MAX(c->mon->wx, MIN(boxes[i][0], c->mon->wx + c->mon->ww - boxes[i][2]));
		boxes[i][1] = MAX(c->mon->wy, MIN(boxes[i][1], c->mon->wy + c->mon->wh - boxes[i][3]));
		if (!c->handles[i]) {
			attributes.cursor = edgecursors[i];
			c->handles[i] = XCreateWindow(dpy, root, boxes[i][0], boxes[i][1], boxes[i][2], boxes[i][3],
				0, 0, InputOnly, CopyFromParent, CWOverrideRedirect|CWEventMask|CWCursor, &attributes);
		} else {
			XMoveResizeWindow(dpy, c->handles[i], boxes[i][0], boxes[i][1], boxes[i][2], boxes[i][3]);
		}
		if ((resizeedges[i] & edges) == resizeedges[i]) {
			if (!(c->handlemask & (1u << i)))
				XMapWindow(dpy, c->handles[i]);
			c->handlemask |= 1u << i;
		} else {
			if (c->handlemask & (1u << i))
				XUnmapWindow(dpy, c->handles[i]);
			c->handlemask &= ~(1u << i);
		}
		XConfigureWindow(dpy, c->handles[i], CWSibling|CWStackMode, &stack);
		stack.sibling = c->handles[i];
	}
	c->handlesmapped = 1;
}

static void
loadlayout(void)
{
	const char *base = getenv("XDG_STATE_HOME"), *home = getenv("HOME");
	FILE *file;
	int current, tiled, length;
	char extra;

	defaultlayout = tiledlayout = 0;
	layoutpath[0] = '\0';
	if (base && base[0] == '/')
		length = snprintf(layoutpath, sizeof layoutpath, "%s/dwm/layout", base);
	else if (home && home[0] == '/')
		length = snprintf(layoutpath, sizeof layoutpath, "%s/.local/state/dwm/layout", home);
	else
		return;
	if (length >= sizeof layoutpath) {
		layoutpath[0] = '\0';
		return;
	}
	if (!(file = fopen(layoutpath, "r")))
		return;
	if (fscanf(file, "%d %d %c", &current, &tiled, &extra) == 2
	    && current >= 0 && current < LENGTH(layouts)
	    && tiled >= 0 && tiled < LENGTH(layouts) && layouts[tiled].arrange) {
		defaultlayout = current;
		tiledlayout = tiled;
	}
	fclose(file);
}

static void
savelayout(void)
{
	char path[PATH_MAX], temporary[PATH_MAX + 16], *p;
	int fd, failed;
	FILE *file;

	if (!layoutpath[0])
		return;
	strcpy(path, layoutpath);
	for (p = path + 1; *p; p++) {
		if (*p != '/')
			continue;
		*p = '\0';
		if (mkdir(path, 0700) < 0 && errno != EEXIST) {
			perror("dwm: cannot create layout state directory");
			return;
		}
		*p = '/';
	}
	snprintf(temporary, sizeof temporary, "%s.XXXXXX", layoutpath);
	if ((fd = mkstemp(temporary)) < 0) {
		perror("dwm: cannot save layout");
		return;
	}
	if (!(file = fdopen(fd, "w"))) {
		close(fd);
		unlink(temporary);
		return;
	}
	failed = fprintf(file, "%d %d\n", defaultlayout, tiledlayout) < 0;
	if (fclose(file) != 0)
		failed = 1;
	if (failed || rename(temporary, layoutpath) < 0) {
		fputs("dwm: cannot save layout\n", stderr);
		unlink(temporary);
	}
}

static void
fitfloating(Client *c)
{
	int title = titleheight(c);
	int maxw = c->mon->ww - 2 * c->bw;
	int maxh = c->mon->wh - 2 * c->bw - title;
	int w = MAX(1, c->w), h = MAX(1, c->h - title);

	if (c->ismpv) {
		maxw = c->mon->ww / 2 - 2 * c->bw;
		maxh = c->mon->wh / 2 - 2 * c->bw - title;
	} else if (!c->isfixed) {
		maxw = c->mon->ww * 3 / 4 - 2 * c->bw;
		maxh = c->mon->wh * 3 / 4 - 2 * c->bw - title;
	}
	maxw = MAX(1, maxw);
	maxh = MAX(1, maxh);
	if (w > maxw) {
		h = MAX(1, (long long)h * maxw / w);
		w = maxw;
	}
	if (h > maxh) {
		w = MAX(1, (long long)w * maxh / h);
		h = maxh;
	}
	c->w = w;
	c->h = h + title;
}

static void
placefloating(Client *c)
{
	Monitor *m = c->mon;
	Client *other;
	int count = 0, i, x, y, right, bottom, dx, dy;
	int step = MAX(32, titleheight(c) + 2 * c->bw);

	c->w = c->floatw ? c->floatw : c->w;
	c->h = c->floath ? c->floath : c->h;
	fitfloating(c);
	right = MAX(m->wx, m->wx + m->ww - WIDTH(c));
	bottom = MAX(m->wy, m->wy + m->wh - HEIGHT(c));
	if (!getrootptr(&x, &y) || x < m->wx || x >= m->wx + m->ww
	    || y < m->wy || y >= m->wy + m->wh) {
		x = m->wx + (right - m->wx) / 2;
		y = m->wy + (bottom - m->wy) / 2;
	}
	x = MAX(m->wx, MIN(x, right));
	y = MAX(m->wy, MIN(y, bottom));
	dx = right - x >= x - m->wx ? step : -step;
	dy = bottom - y >= y - m->wy ? step : -step;
	for (other = m->clients; other; other = other->next)
		if (other != c && ISVISIBLE(other))
			count++;
	/* expose the previous title bar without searching for distant free space. */
	for (i = 0; i <= count; i++) {
		c->x = MAX(m->wx, MIN(x + i * dx, right));
		c->y = MAX(m->wy, MIN(y + i * dy, bottom));
		for (other = m->clients; other; other = other->next)
			if (other != c && ISVISIBLE(other)
			    && abs(c->x - other->x) < step && abs(c->y - other->y) < step)
				break;
		if (!other)
			break;
	}
}

static void
switchlayout(Monitor *m, const Layout *layout)
{
	Client *c;
	int wasfloating = !m->lt[m->sellt]->arrange;
	int floating = !layout->arrange;
	int x, y, w, h;

	if (m->lt[m->sellt] != layout)
		m->sellt ^= 1;
	m->lt[m->sellt] = layout;
	strncpy(m->ltsymbol, layout->symbol, sizeof m->ltsymbol);
	for (c = m->clients; c; c = c->next) {
		if (c->isfullscreen) {
			if (wasfloating && !floating) {
				c->floatx = c->fsx; c->floaty = c->fsy;
				c->floatw = c->fsw; c->floath = c->fsh;
				c->floatvalid = 1;
			}
			if (!floating)
				c->oldstate = c->alwaysfloating || c->isfixed;
			continue;
		}
		if (!floating) {
			if (wasfloating || c->isfloating) {
				c->floatx = c->snapmode ? c->savedx : c->x;
				c->floaty = c->snapmode ? c->savedy : c->y;
				c->floatw = c->snapmode ? c->savedw : c->w;
				c->floath = c->snapmode ? c->savedh : c->h;
				c->floatvalid = 1;
			}
			c->snapmode = 0;
			c->isfloating = c->alwaysfloating || c->isfixed;
		} else if (!wasfloating && !c->isfloating) {
			if (!c->floatvalid) {
				x = c->x; y = c->y; w = c->w; h = c->h;
				placefloating(c);
				c->floatx = c->x; c->floaty = c->y;
				c->floatw = c->w; c->floath = c->h;
				c->x = x; c->y = y; c->w = w; c->h = h;
				c->floatvalid = 1;
			}
			x = MAX(m->wx, MIN(c->floatx, m->wx + m->ww - c->floatw - 2 * c->bw));
			y = MAX(m->wy, MIN(c->floaty, m->wy + m->wh - c->floath - 2 * c->bw));
			resizefloatingclient(c, x, y, c->floatw, c->floath);
		}
	}
}

/* berry-style floating geometry: fixed resize origin, pixel sizes and edge locking. */
void
resizefloatingclient(Client *c, int x, int y, int w, int h)
{
	Monitor *m = c->mon;

	x = MAX(m->wx, MIN(x, m->wx + m->ww - 30 - 2 * c->bw));
	y = MAX(m->wy, MIN(y, m->wy + m->wh - 30 - titleheight(c) - 2 * c->bw));
	w = MAX(1, MIN(MAX(30, w), m->wx + m->ww - x - 2 * c->bw));
	h = MAX(1, MIN(MAX(30 + titleheight(c), h), m->wy + m->wh - y - 2 * c->bw));
	if (x != c->x || y != c->y || w != c->w || h != c->h)
		resizeclient(c, x, y, w, h);
}

void
adjustfloating(const Arg *arg)
{
	Client *c = selmon->sel;
	const int *delta = arg->v;
	int x, y;

	if (!c || c->isfullscreen || (!c->isfloating && c->mon->lt[c->mon->sellt]->arrange))
		return;
	x = c->x;
	y = c->y;
	if (delta[0])
		x = MAX(c->mon->wx, MIN(x + delta[0], c->mon->wx + c->mon->ww - WIDTH(c)));
	if (delta[1])
		y = MAX(c->mon->wy, MIN(y + delta[1], c->mon->wy + c->mon->wh - HEIGHT(c)));
	c->snapmode = 0;
	resizefloatingclient(c, x, y, c->w + delta[2], c->h + delta[3]);
}

void
resizefloating(const Arg *arg)
{
	int delta[] = {0, 0, arg->i, arg->i};

	adjustfloating(&(Arg){.v = delta});
}

void
centerfloating(const Arg *arg)
{
	Client *c = selmon->sel;

	if (!c || c->isfullscreen || (!c->isfloating && c->mon->lt[c->mon->sellt]->arrange))
		return;
	c->snapmode = 0;
	resizefloatingclient(c, c->mon->wx + (c->mon->ww - WIDTH(c)) / 2,
		c->mon->wy + (c->mon->wh - HEIGHT(c)) / 2, c->w, c->h);
}

void
minimize(const Arg *arg)
{
	if (!selmon->sel)
		return;
	selmon->sel->ishidden = 1;
	setclientstate(selmon->sel, IconicState);
	focus(NULL);
	arrange(selmon);
}

void
restore(const Arg *arg)
{
	Client *c;

	for (c = selmon->stack; c; c = c->snext) {
		if (!ISTAGVISIBLE(c) || !c->ishidden)
			continue;
		c->ishidden = 0;
		selmon->showdesktop = 0;
		setclientstate(c, NormalState);
		focus(c);
		arrange(selmon);
		return;
	}
}

void
showdesktop(const Arg *arg)
{
	selmon->showdesktop = !selmon->showdesktop;
	focus(NULL);
	arrange(selmon);
}

void
focusdirection(const Arg *arg)
{
	Client *c, *best = NULL, *selected = selmon->sel;
	Monitor *m;
	long score, bestscore = LONG_MAX;
	int dx, dy, forward, sideways;

	if (!selected)
		return;
	for (m = mons; m; m = m->next) {
		for (c = m->clients; c; c = c->next) {
			if (c == selected || !ISVISIBLE(c))
				continue;
			dx = c->x + c->w / 2 - selected->x - selected->w / 2;
			dy = c->y + c->h / 2 - selected->y - selected->h / 2;
			forward = arg->i == 1 ? -dx : arg->i == 2 ? dx : arg->i == 3 ? -dy : dy;
			sideways = arg->i <= 2 ? abs(dy) : abs(dx);
			if (forward <= 0)
				continue;
			score = (long)forward + 2L * sideways;
			if (score < bestscore) {
				bestscore = score;
				best = c;
			}
		}
	}
	if (best) {
		focus(best);
		restack(selmon);
	}
}

void
movedesktop(const Arg *arg)
{
	unsigned int index;

	if (!selmon->sel)
		return;
	for (index = 0; index < LENGTH(tags); index++)
		if (selmon->tagset[selmon->seltags] & (1U << index))
			break;
	index = (index + LENGTH(tags) + arg->i) % LENGTH(tags);
	tag(&(Arg){.ui = 1U << index});
}

void
fullscreen(const Arg *arg)
{
	if (selmon->sel)
		setfullscreen(selmon->sel, !selmon->sel->isfullscreen);
}
