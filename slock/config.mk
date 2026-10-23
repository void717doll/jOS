VERSION = 1.7
PREFIX = /usr/local
MANPREFIX = ${PREFIX}/share/man
CPPFLAGS = -DVERSION=\"${VERSION}\" -D_DEFAULT_SOURCE -DHAVE_SHADOW_H
XFT_CFLAGS = $(shell pkg-config --cflags xft)
CFLAGS = -std=c99 -pedantic -Wall -Wextra -Os ${CPPFLAGS} ${XFT_CFLAGS}
LDFLAGS = -lcrypt -lX11 -lXext -lXrandr $(shell pkg-config --libs xft)
