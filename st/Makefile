# st - simple terminal
# See LICENSE file for copyright and license details.
.POSIX:

include config.mk

SRC = st.c x.c
OBJ = $(SRC:.c=.o)

all: options st

options:
	@echo st build options:
	@echo "CFLAGS  = $(STCFLAGS)"
	@echo "LDFLAGS = $(STLDFLAGS)"
	@echo "CC      = $(CC)"

config.h: config.def.h
	cp config.def.h config.h

.c.o:
	$(CC) $(STCFLAGS) -c $<

st.o: config.h st.h win.h autocomplete.h
x.o: arg.h config.h st.h win.h autocomplete.h xdnd.c

$(OBJ): config.h config.mk

st: $(OBJ)
	$(CC) -o $@ $(OBJ) $(STLDFLAGS)

check: all
	@check_binary=$$(mktemp); trap 'rm -f "$$check_binary"' EXIT HUP INT TERM; \
	$(CC) $(STCPPFLAGS) $(CPPFLAGS) $(CFLAGS) -g -fsanitize=address,undefined \
	-ffunction-sections -fdata-sections tests/resize.c -Wl,--gc-sections -o "$$check_binary" && "$$check_binary"
	@check_binary=$$(mktemp); trap 'rm -f "$$check_binary"' EXIT HUP INT TERM; \
	$(CC) $(STCPPFLAGS) $(CPPFLAGS) $(CFLAGS) -g -fsanitize=address,undefined \
	-ffunction-sections -fdata-sections tests/features.c -Wl,--gc-sections -o "$$check_binary" && PATH="$(CURDIR):$$PATH" "$$check_binary"
	@check_binary=$$(mktemp); trap 'rm -f "$$check_binary"' EXIT HUP INT TERM; \
	$(CC) $(STCFLAGS) -g -fsanitize=address,undefined -ffunction-sections -fdata-sections \
	tests/drop.c st.c -Wl,--gc-sections $(STLDFLAGS) -o "$$check_binary" \
	-Wl,--wrap=XSendEvent,--wrap=XFlush,--wrap=XDeleteProperty,--wrap=XConvertSelection \
	-Wl,--wrap=XGetWindowProperty,--wrap=ttywrite && "$$check_binary"
	python3 tests/autocomplete.py

clean:
	rm -f st $(OBJ) st-$(VERSION).tar.gz

dist: clean
	mkdir -p st-$(VERSION)
	cp -R LICENSE Makefile config.mk\
		config.def.h st.info st.1 arg.h st.h win.h autocomplete.h st-autocomplete xdnd.c tests $(SRC)\
		st-$(VERSION)
	tar -cf - st-$(VERSION) | gzip > st-$(VERSION).tar.gz
	rm -rf st-$(VERSION)

install: st
	mkdir -p $(DESTDIR)$(PREFIX)/bin
	cp -f st $(DESTDIR)$(PREFIX)/bin
	chmod 755 $(DESTDIR)$(PREFIX)/bin/st
	install -m755 st-autocomplete $(DESTDIR)$(PREFIX)/bin/st-autocomplete
	mkdir -p $(DESTDIR)$(MANPREFIX)/man1
	sed "s/VERSION/$(VERSION)/g" < st.1 > $(DESTDIR)$(MANPREFIX)/man1/st.1
	chmod 644 $(DESTDIR)$(MANPREFIX)/man1/st.1
	tic -sx -o "$(DESTDIR)$(PREFIX)/share/terminfo" st.info
	@echo see the jOS README for terminal setup notes.
	mkdir -p $(DESTDIR)$(PREFIX)/share/applications
	cp -f st.desktop $(DESTDIR)$(PREFIX)/share/applications

uninstall:
	rm -f $(DESTDIR)$(PREFIX)/bin/st
	rm -f $(DESTDIR)$(PREFIX)/bin/st-autocomplete
	rm -f $(DESTDIR)$(MANPREFIX)/man1/st.1
	rm -f $(DESTDIR)$(PREFIX)/share/applications/st.desktop

.PHONY: all options check clean dist install uninstall
