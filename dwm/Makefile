# dwm - dynamic window manager
# See LICENSE file for copyright and license details.

include config.mk

SRC = drw.c dwm.c util.c
OBJ = ${SRC:.c=.o}

all: dwm

.c.o:
	${CC} -c ${CFLAGS} $<

${OBJ}: config.h config.mk drw.h

dwm.o: desktop.c

config.h: config.def.h
	cp config.def.h $@

dwm: ${OBJ}
	${CC} -o $@ ${OBJ} ${LDFLAGS}

clean:
	rm -f dwm ${OBJ} dwm-${VERSION}.tar.gz

dist: clean
	mkdir -p dwm-${VERSION}
	cp -R LICENSE Makefile config.def.h config.mk \
	dwm.1 desktop.c LICENSE.berry drw.h util.h ${SRC} dwm-${VERSION}
	tar -cf dwm-${VERSION}.tar dwm-${VERSION}
	gzip dwm-${VERSION}.tar
	rm -rf dwm-${VERSION}

install: all
	mkdir -p ${DESTDIR}${PREFIX}/bin
	cp -f dwm ${DESTDIR}${PREFIX}/bin
	chmod 755 ${DESTDIR}${PREFIX}/bin/dwm
	mkdir -p ${DESTDIR}${MANPREFIX}/man1
	sed "s/VERSION/${VERSION}/g" < dwm.1 > ${DESTDIR}${MANPREFIX}/man1/dwm.1
	chmod 644 ${DESTDIR}${MANPREFIX}/man1/dwm.1

uninstall:
	rm -f ${DESTDIR}${PREFIX}/bin/dwm \
	${DESTDIR}${MANPREFIX}/man1/dwm.1

check: all
	$(MAKE) -C ../st check
	$(MAKE) -C ../slock check
	$(MAKE) -C ../dmenu
	$(MAKE) -C ../tabbed check
	@check_binary=$$(mktemp); trap 'rm -f "$$check_binary"' EXIT HUP INT TERM; \
	${CC} ${CFLAGS} -g -fsanitize=address,undefined -o "$$check_binary" tests/input.c drw.c util.c ${LDFLAGS} \
	-Wl,--wrap=fork,--wrap=XKeycodeToKeysym \
	-Wl,--wrap=XGetClassHint,--wrap=XInternAtom,--wrap=XSendEvent,--wrap=XGetWMProtocols \
	-Wl,--wrap=XCopyArea,--wrap=XFlush,--wrap=XConfigureWindow,--wrap=XSync \
	-Wl,--wrap=XRaiseWindow,--wrap=XCheckMaskEvent,--wrap=XGrabPointer,--wrap=XUngrabPointer \
	-Wl,--wrap=XQueryPointer,--wrap=XWarpPointer,--wrap=XMaskEvent,--wrap=XAllowEvents \
	-Wl,--wrap=XCreateWindow,--wrap=XMoveWindow,--wrap=XMoveResizeWindow,--wrap=XMapWindow,--wrap=XUnmapWindow \
	-Wl,--wrap=XDestroyWindow,--wrap=XSetWindowBorderWidth,--wrap=XSetWindowBorder,--wrap=XDefineCursor,--wrap=XChangeProperty \
	-Wl,--wrap=XSetWindowBackground,--wrap=XClearWindow \
	-Wl,--wrap=XCreatePixmap,--wrap=XFreePixmap,--wrap=XCreateGC,--wrap=XFreeGC \
	-Wl,--wrap=XSetForeground,--wrap=XFillRectangle,--wrap=XSetWindowBorderPixmap \
	-Wl,--wrap=XGetModifierMapping,--wrap=XUngrabButton,--wrap=XGrabButton \
	-Wl,--wrap=drw_fontset_getwidth,--wrap=drw_setscheme,--wrap=drw_text,--wrap=drw_rect && "$$check_binary"
	@check_binary=$$(mktemp); trap 'rm -f "$$check_binary"' EXIT HUP INT TERM; \
	${CC} ${CFLAGS} -g -fsanitize=address,undefined -o "$$check_binary" tests/systray.c drw.c util.c ${LDFLAGS} \
	-Wl,--wrap=XGetSelectionOwner,--wrap=XSetSelectionOwner,--wrap=XCreateWindow,--wrap=XGetWindowAttributes \
	-Wl,--wrap=XGetWindowProperty,--wrap=XSendEvent,--wrap=XChangeProperty,--wrap=XSetWindowBorderWidth \
	-Wl,--wrap=XSelectInput,--wrap=XAddToSaveSet,--wrap=XRemoveFromSaveSet,--wrap=XReparentWindow \
	-Wl,--wrap=XChangeWindowAttributes,--wrap=XSync,--wrap=XMoveResizeWindow,--wrap=XMapRaised \
	-Wl,--wrap=XMapWindow,--wrap=XUnmapWindow,--wrap=XSetWindowBackground,--wrap=XConfigureWindow \
	-Wl,--wrap=XClearWindow,--wrap=XDestroyWindow,--wrap=drw_fontset_getwidth,--wrap=drw_setscheme \
	-Wl,--wrap=drw_rect,--wrap=drw_map,--wrap=drw_text && "$$check_binary"
	python3 tests/session.py
	python3 tests/deploy.py
	python3 tests/status.py
	python3 tests/helpers.py
	python3 tests/network.py
	python3 tests/bluetooth.py
	python3 tests/vpn.py
	python3 tests/mount.py

check-x11: all
	$(MAKE) -C ../dmenu
	$(MAKE) -C ../tabbed
	python3 tests/x11.py

.PHONY: all clean dist install uninstall check check-x11
