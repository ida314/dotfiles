# overlay -- see overlay.c. `make` builds in place; `make install` copies the
# binary to $(PREFIX)/bin.

PREFIX ?= /usr/local
FONT ?= /usr/lib/kbd/consolefonts/ter-u24b.psf.gz
WLPROTO = $(shell pkg-config --variable=pkgdatadir wayland-protocols)

CFLAGS ?= -O2 -Wall -Wextra -Wno-unused-parameter
CFLAGS += $(shell pkg-config --cflags wayland-client) -I.
LDLIBS += $(shell pkg-config --libs wayland-client)

PROTOS = wlr-layer-shell-unstable-v1 xdg-shell viewporter fractional-scale-v1
GEN = $(PROTOS:%=%-client-protocol.h) $(PROTOS:%=%-protocol.c)

all: overlay

overlay: overlay.c font.h $(GEN)
	$(CC) $(CFLAGS) -o $@ overlay.c $(PROTOS:%=%-protocol.c) $(LDFLAGS) $(LDLIBS)

# the font is embedded so the binary reads nothing at runtime
font.h: $(FONT)
	gzip -dc $< > font.psf
	xxd -i -n font_psf font.psf > $@
	rm -f font.psf

wlr-layer-shell-unstable-v1.xml: protocols/wlr-layer-shell-unstable-v1.xml
	cp $< $@
xdg-shell.xml:
	cp $(WLPROTO)/stable/xdg-shell/xdg-shell.xml $@
viewporter.xml:
	cp $(WLPROTO)/stable/viewporter/viewporter.xml $@
fractional-scale-v1.xml:
	cp $(WLPROTO)/staging/fractional-scale/fractional-scale-v1.xml $@

%-client-protocol.h: %.xml
	wayland-scanner client-header $< $@
%-protocol.c: %.xml
	wayland-scanner private-code $< $@

install: overlay
	install -Dm755 overlay $(DESTDIR)$(PREFIX)/bin/overlay

clean:
	rm -f overlay font.h font.psf $(GEN) $(PROTOS:%=%.xml)

.PHONY: all install clean
.INTERMEDIATE: $(PROTOS:%=%.xml)
