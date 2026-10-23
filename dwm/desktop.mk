PREFIX ?= $(HOME)/.local
DESTDIR ?=

all:
	$(MAKE) -C ../slock
	$(MAKE) -C ../st
	$(MAKE) -C ../dmenu
	$(MAKE) -C ../tabbed
	$(MAKE)

install: all
	$(MAKE) -C ../slock install PREFIX="$(PREFIX)" DESTDIR="$(DESTDIR)"
	$(MAKE) -C ../st install PREFIX="$(PREFIX)" DESTDIR="$(DESTDIR)"
	$(MAKE) -C ../dmenu install PREFIX="$(PREFIX)" DESTDIR="$(DESTDIR)"
	$(MAKE) -C ../tabbed install PREFIX="$(PREFIX)" DESTDIR="$(DESTDIR)"
	$(MAKE) install PREFIX="$(PREFIX)" DESTDIR="$(DESTDIR)"
	install -m 755 scripts/dwm-action scripts/dwm-session scripts/dwm-check scripts/dwm-background scripts/dwm-bluetooth scripts/dwm-calendar scripts/dwm-network scripts/dwm-status scripts/dwm-wallpaper scripts/dwm-weather scripts/dwm-wifi scripts/dwm-vpn scripts/dwm-mount "$(DESTDIR)$(PREFIX)/bin/"
	install -m 755 scripts/lock "$(DESTDIR)$(PREFIX)/bin/dwm-lock"
	mkdir -p "$(DESTDIR)$(PREFIX)/share/dwm"
	install -m 644 dunstrc wallpaper picom.conf redshift.conf "$(DESTDIR)$(PREFIX)/share/dwm/"
	mkdir -p "$(DESTDIR)$(PREFIX)/share/xsessions"
	sed 's|@PREFIX@|$(PREFIX)|g' jos.desktop > "$(DESTDIR)$(PREFIX)/share/xsessions/jos.desktop"

clean:
	$(MAKE) -C ../slock clean
	$(MAKE) -C ../st clean
	$(MAKE) -C ../dmenu clean
	$(MAKE) -C ../tabbed clean
	$(MAKE) clean

.PHONY: all install clean
