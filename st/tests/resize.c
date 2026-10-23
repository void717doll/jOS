#include <assert.h>
#include <locale.h>
#include "../st.c"

unsigned int defaultfg = 7, defaultbg = 0, tabspaces = 8;
int allowaltscreen = 1, allowwindowops = 0;
char *vtiden = "\033[?6c";
wchar_t *worddelimiters = L" ";

void xbell(void) {}
void xclipcopy(void) {}
void xloadcols(void) {}
int xsetcolorname(int i, const char *name) { return 0; }
void xseticontitle(char *title) {}
void xsettitle(char *title) {}
int xsetcursor(int cursor) { return 0; }
void xsetmode(int set, unsigned int flags) {}
void xsetpointermotion(int set) {}
void xsetsel(char *text) { free(text); }
void xdrawline(Line line, int x, int y, int end) {}
int xstartdraw(void) { return 1; }
void xdrawcursor(int x, int y, Glyph g, int ox, int oy, Glyph og)
{
	assert(BETWEEN(x, 0, term.col - 1));
	assert(BETWEEN(y, 0, term.row - 1));
}
void xfinishdraw(void) {}
void xximspot(int x, int y) {}

static void
write_text(const char *text)
{
	assert(twrite(text, strlen(text), 0) == (int)strlen(text));
}

static char *
contents(void)
{
	char *text, *p;
	int y;

	p = text = xmalloc((term.histf + term.row) * (term.col * UTF_SIZ + 1) + 1);
	for (y = -term.histf; y <= term.c.y; y++)
		p += tgetline(p, TLINEABS(y));
	*p = '\0';
	return text;
}

static void
check_grid(void)
{
	int x, y;

	assert(BETWEEN(term.c.x, 0, term.col - 1));
	assert(BETWEEN(term.c.y, 0, term.row - 1));
	assert(BETWEEN(term.scr, 0, term.histf));
	draw();
	for (y = -term.histf; y < term.row; y++) {
		Line line = TLINEABS(y);
		for (x = 0; x < term.col; x++) {
			if (line[x].mode & ATTR_WDUMMY)
				assert(x > 0 && (line[x - 1].mode & ATTR_WIDE));
			if ((line[x].mode & ATTR_WIDE) && term.col > 1)
				assert(x + 1 < term.col && (line[x + 1].mode & ATTR_WDUMMY));
		}
	}
}

static void
roundtrip(const char *text, int narrow, int height)
{
	char *before, *after;

	tresize(80, 24);
	treset();
	write_text(text);
	before = contents();
	for (int i = 0; i < 4; i++) {
		tresize(narrow, height);
		check_grid();
		tresize(80, 24);
		check_grid();
	}
	after = contents();
	if (strcmp(before, after)) {
		fprintf(stderr, "before: %s\nafter: %s\n", before, after);
		abort();
	}
	free(before);
	free(after);
}

int
main(void)
{
	char text[8192] = "";
	char *before, *after;
	int i;

	assert(setlocale(LC_CTYPE, "C.UTF-8"));
	tnew(80, 24);
	roundtrip("abcdefghijklmnopqrstuvwxyz0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ", 15, 6);
	roundtrip("first line\r\nsecond line with spaces   \r\n\r\nlast line", 9, 4);
	for (i = 0; i < 60; i++)
		strcat(text, "a long numbered output line: 0123456789abcdefghijklmnopqrstuvwxyz\r\n");
	roundtrip(text, 19, 7);
	roundtrip("abcdefgh界ijklmnop世界qrstuv", 9, 8);
	roundtrip("界世界", 1, 8);
	roundtrip("1234567890", 1, 12);
	roundtrip("12345678", 8, 4);

	tresize(80, 24);
	treset();
	write_text("shell output that must survive fullscreen editor resizes");
	before = contents();
	write_text("\033[?1049h");
	write_text("alternate screen");
	tresize(17, 5);
	tresize(95, 32);
	write_text("\033[?1049l");
	check_grid();
	after = contents();
	assert(!strcmp(before, after));
	free(before);
	free(after);
	/* output written after reflow must continue at the preserved cursor. */
	tresize(80, 24);
	treset();
	write_text("12345678");
	tresize(8, 4);
	write_text("9");
	tresize(80, 24);
	after = contents();
	assert(!strcmp(after, "123456789\n"));
	free(after);

	for (i = 0; i < HISTSIZE + 20; i++)
		write_text("history line\r\n");
	kscrollup(&((Arg){ .i = 12 }));
	for (i = 0; i < 100; i++) {
		tresize(1 + i % 83, 1 + i % 31);
		check_grid();
	}
	kscrolldown(&((Arg){ .i = term.scr }));
	check_grid();
	/* release both screens so leak checking also covers repeated resizes. */
	for (i = 0; i < 2; i++) {
		for (int y = 0; y < term.row; y++)
			free(term.line[y]);
		free(term.line);
		tswapscreen();
	}
	for (i = 0; i < HISTSIZE; i++)
		free(term.hist[i]);
	free(term.tabs);
	free(term.dirty);
	puts("resize checks passed: text, history, hard breaks, wide glyphs, cursor and alternate screen");
	return 0;
}
