include config.mk

all: slock

config.h: config.def.h
	cp config.def.h $@

slock.o: slock.c ui.c config.h config.mk
	$(CC) $(CFLAGS) -c slock.c

slock: slock.o
	$(CC) -o $@ slock.o $(LDFLAGS)

install: all
	install -Dm755 slock "$(DESTDIR)$(PREFIX)/libexec/dwm-slock"
	install -Dm644 slock.1 "$(DESTDIR)$(MANPREFIX)/man1/dwm-slock.1"

clean:
	rm -f slock slock.o

check: all
	@check_binary=$$(mktemp); trap 'rm -f "$$check_binary"' EXIT HUP INT TERM; \
	$(CC) $(CPPFLAGS) $(XFT_CFLAGS) -g -fsanitize=address,undefined -ffunction-sections -fdata-sections \
	tests/lock.c -Wl,--gc-sections -o "$$check_binary" $(LDFLAGS) \
	-Wl,--wrap=clock_gettime,--wrap=poll,--wrap=XPending,--wrap=XNextEvent,--wrap=XLookupString \
	-Wl,--wrap=DPMSCapable,--wrap=DPMSEnable,--wrap=DPMSForceLevel,--wrap=XFlush,--wrap=XBell \
	-Wl,--wrap=XRaiseWindow,--wrap=XResizeWindow,--wrap=XkbGetState \
	-Wl,--wrap=XSetForeground,--wrap=XFillRectangle,--wrap=XFillArc \
	-Wl,--wrap=XCopyArea,--wrap=XftTextExtentsUtf8,--wrap=XftDrawStringUtf8,--wrap=XftDrawSetClip,--wrap=XftDrawSetClipRectangles && "$$check_binary"

.PHONY: all install clean check
