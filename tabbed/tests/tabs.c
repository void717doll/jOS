#define main tabbed_main
#include "../tabbed.c"
#undef main
#include <assert.h>

static int painted;
static int hoverpainted;
static unsigned long background;

int __wrap_XSetForeground(Display *display, GC gc, unsigned long pixel) { background = pixel; return 1; }
int __wrap_XFlush(Display *display) { return 1; }
int __wrap_XCopyArea(Display *display, Drawable source, Drawable target, GC gc,
                    int x, int y, unsigned int w, unsigned int h, int dx, int dy) { return 1; }
int __wrap_XFillRectangles(Display *display, Drawable target, GC gc, XRectangle *rects, int count)
{
	for (int i = 0; i < count; i++) {
		assert(rects[i].x >= 0 && rects[i].width > 0);
		assert(rects[i].x + rects[i].width <= ww);
		painted += rects[i].width;
		if (background == dc.hover[ColBG].pixel)
			hoverpainted += rects[i].width;
	}
	return 1;
}
void __wrap_XftTextExtentsUtf8(Display *display, XftFont *font, const FcChar8 *text, int len, XGlyphInfo *extents)
{
	*extents = (XGlyphInfo){.xOff = len * 8};
}
XftDraw *__wrap_XftDrawCreate(Display *display, Drawable target, Visual *visual, Colormap colormap)
{
	return (XftDraw *)&dc;
}
void __wrap_XftDrawStringUtf8(XftDraw *draw, const XftColor *color, XftFont *font,
                            int x, int y, const FcChar8 *text, int len) { }
void __wrap_XftDrawDestroy(XftDraw *draw) { }

int
main(void)
{
	Client tabs[8] = {0};
	Client *windows[LENGTH(tabs)];
	Screen screeninfo = {0};
	_XPrivDisplay display = calloc(1, sizeof *display);
	display->screens = &screeninfo;
	dpy = (Display *)display;
	dc.font.height = dc.font.ascent = 20;
	dc.h = bh = 24;
	dc.norm[ColBG].pixel = 1;
	dc.sel[ColBG].pixel = 2;
	dc.hover[ColBG].pixel = 3;
	dc.urg[ColBG].pixel = 4;
	clients = windows;
	for (size_t i = 0; i < LENGTH(tabs); i++) {
		windows[i] = &tabs[i];
		snprintf(tabs[i].name, sizeof tabs[i].name, "tab %zu", i);
	}
	for (nclients = 1; nclients <= LENGTH(tabs); nclients++) {
		for (sel = 0; sel < nclients; sel++) {
			for (ww = 1; ww <= 900; ww++) {
				for (int i = 0; i < nclients; i++)
					tabs[i].tabx = 2000;
				painted = 0;
				drawbar();
				assert(tabs[sel].tabx > 0 && tabs[sel].tabx <= ww);
				assert(getfirsttab() >= 0 && getfirsttab() <= sel);
				assert(painted == ww);
				for (int i = 0; i < nclients; i++)
					assert(tabs[i].tabx == -1 || (tabs[i].tabx > 0 && tabs[i].tabx <= ww));
			}
		}
	}
	nclients = 3;
	sel = 1;
	ww = 600;
	drawbar();
	XEvent event = {.xmotion = {.type = MotionNotify, .window = win, .x = 10, .y = 4}};
	hoverpainted = 0;
	drawbar();
	assert(hoverpainted == 0);
	groupmode = True;
	hoverpainted = 0;
	hover(&event);
	assert(hoverpainted == 200 && tabat(hoverx, hovery) == 0 && sel == 1);
	painted = 0;
	event.xmotion.x = 20;
	hover(&event);
	assert(painted == 0);
	hoverpainted = 0;
	event.xmotion.x = 210;
	hover(&event);
	assert(hoverpainted == 200 && tabat(hoverx, hovery) == sel);
	hoverpainted = 0;
	event.type = LeaveNotify;
	hover(&event);
	assert(hoverpainted == 0 && hoverx == -1);
	event = (XEvent){.xcrossing = {.type = EnterNotify, .window = win, .x = 450, .y = 4}};
	hover(&event);
	assert(hoverpainted == 200 && tabat(hoverx, hovery) == 2 && sel == 1);
	hoverpainted = 0;
	event = (XEvent){.xmotion = {.type = MotionNotify, .window = win, .x = 450, .y = bh}};
	hover(&event);
	assert(hoverpainted == 0 && tabat(hoverx, hovery) == -1);
	nclients = 8;
	sel = 7;
	ww = 400;
	drawbar();
	assert(tabat(0, 0) == -1);
	ww = 1;
	drawbar();
	assert(tabat(0, 0) == sel && tabat(1, 0) == -1);
	free(display);
	puts("narrow tab bars keep the selected tab visible and clear hidden click targets: passed (x calls mocked)");
	return 0;
}
