/*
 * overlay -- show a line of text in a small click-through box on the
 * wlr-layer-shell overlay layer, above every window including fullscreen ones.
 *
 * Text comes from stdin, one line per update: each line replaces the box's
 * contents, an empty line hides the box until the next non-empty one, and EOF
 * quits. What to show is the feeder's business; this only draws.
 *
 * The surface is exactly the size of its text, so the cost is one tiny shm
 * buffer, not a screen-sized one. The font is a Terminus PSF2 bitmap embedded
 * at build time (font.h, see the Makefile), so the only dependency is
 * wayland-client. Glyphs are drawn at the output's real pixel size through
 * fractional-scale + viewporter, which keeps them sharp at dwl's 1.5 scale.
 */
#define _GNU_SOURCE
#include <errno.h>
#include <poll.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#include <wayland-client.h>

#include "font.h"
#include "fractional-scale-v1-client-protocol.h"
#include "viewporter-client-protocol.h"
#include "wlr-layer-shell-unstable-v1-client-protocol.h"

#define MAXGLYPHS 128

/* PSF2 font, parsed once from the embedded blob */
static const uint8_t *glyphs;
static uint32_t nglyph, charsize, fh, fw;
static uint16_t cpmap[0x10000]; /* codepoint -> glyph index + 1, 0 = none */

static struct wl_display *dpy;
static struct wl_compositor *compositor;
static struct wl_shm *shm;
static struct zwlr_layer_shell_v1 *layer_shell;
static struct wp_viewporter *viewporter;
static struct wp_fractional_scale_manager_v1 *fscale_mgr;

/* exists only while there is text to show */
static struct wl_surface *surf;
static struct zwlr_layer_surface_v1 *ls;
static struct wp_viewport *vp;
static struct wp_fractional_scale_v1 *fscale;
static int configured;
static uint32_t lw, lh; /* logical size last requested */

static uint32_t scale120 = 120; /* survives surface re-creation */
static uint32_t cps[MAXGLYPHS];
static int ncp;

/* options */
static uint32_t anchor = ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP | ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT;
static int margin = 8;
static uint32_t fg, bg, ol; /* ol: glyph outline, 0 = none */

static void
die(const char *msg)
{
	fprintf(stderr, "overlay: %s\n", msg);
	exit(1);
}

/* "#rrggbb" or "#rrggbbaa" -> premultiplied ARGB8888 */
static uint32_t
parsecolor(const char *s)
{
	unsigned long v;
	uint32_t r, g, b, a = 0xff;
	char *end;

	if (*s == '#')
		s++;
	v = strtoul(s, &end, 16);
	if (*end || (end - s != 6 && end - s != 8))
		die("colors are #rrggbb or #rrggbbaa");
	if (end - s == 8) {
		a = v & 0xff;
		v >>= 8;
	}
	r = (v >> 16 & 0xff) * a / 255;
	g = (v >> 8 & 0xff) * a / 255;
	b = (v & 0xff) * a / 255;
	return a << 24 | r << 16 | g << 8 | b;
}

/* decode one UTF-8 sequence; malformed input yields '?' and advances a byte */
static uint32_t
utf8next(const unsigned char **p)
{
	const unsigned char *s = *p;
	uint32_t cp;
	int n, i;

	if (s[0] < 0x80) {
		*p += 1;
		return s[0];
	}
	if ((s[0] & 0xe0) == 0xc0)
		cp = s[0] & 0x1f, n = 1;
	else if ((s[0] & 0xf0) == 0xe0)
		cp = s[0] & 0x0f, n = 2;
	else if ((s[0] & 0xf8) == 0xf0)
		cp = s[0] & 0x07, n = 3;
	else
		goto bad;
	for (i = 1; i <= n; i++) {
		if ((s[i] & 0xc0) != 0x80)
			goto bad;
		cp = cp << 6 | (s[i] & 0x3f);
	}
	*p += n + 1;
	return cp;
bad:
	*p += 1;
	return '?';
}

static void
loadfont(void)
{
	const uint8_t *f = font_psf, *p, *end = font_psf + font_psf_len;
	uint32_t hdr[8], g;

	if (font_psf_len < sizeof(hdr))
		die("embedded font is truncated");
	memcpy(hdr, f, sizeof(hdr));
	if (hdr[0] != 0x864ab572)
		die("embedded font is not PSF2");
	nglyph = hdr[4], charsize = hdr[5], fh = hdr[6], fw = hdr[7];
	glyphs = f + hdr[2];
	p = glyphs + (size_t)nglyph * charsize;
	if (p > end)
		die("embedded font is truncated");

	if (!(hdr[3] & 1)) { /* no unicode table: glyph index is the codepoint */
		for (g = 0; g < nglyph && g < 0x10000; g++)
			cpmap[g] = g + 1;
		return;
	}
	/* per glyph: UTF-8 codepoints, then 0xFE-led combining sequences
	 * (ignored), terminated by 0xFF */
	for (g = 0; g < nglyph && p < end; g++, p++) {
		while (p < end && *p != 0xff && *p != 0xfe) {
			uint32_t cp = utf8next(&p);
			if (cp < 0x10000 && !cpmap[cp])
				cpmap[cp] = g + 1;
		}
		while (p < end && *p != 0xff)
			p++;
	}
}

static void
buffer_release(void *data, struct wl_buffer *buf)
{
	wl_buffer_destroy(buf);
}

static const struct wl_buffer_listener buffer_listener = {
	.release = buffer_release,
};

static void
draw(void)
{
	uint32_t padx = fw / 2, pady = fh / 8;
	uint32_t W = ncp * fw + 2 * padx, H = fh + 2 * pady;
	uint32_t stride = W * 4, size = stride * H, rowbytes = (fw + 7) / 8;
	uint32_t *px, x, y, nw, nh;
	struct wl_shm_pool *pool;
	struct wl_buffer *buf;
	int fd, i;

	if (!surf || !configured)
		return;

	if ((fd = memfd_create("overlay", MFD_CLOEXEC)) < 0 || ftruncate(fd, size) < 0)
		die("cannot allocate buffer");
	if ((px = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0)) == MAP_FAILED)
		die("cannot map buffer");

	for (i = 0; i < (int)(W * H); i++)
		px[i] = bg;
	/* pass 0 rings every lit pixel with the outline, pass 1 draws the glyphs
	 * over it, so the outline only survives at the edges. The padding is at
	 * least a pixel, so the ring never leaves the buffer. */
	for (int pass = !ol; pass < 2; pass++)
		for (i = 0; i < ncp; i++) {
			uint32_t gi = cps[i] < 0x10000 ? cpmap[cps[i]] : 0;
			const uint8_t *gl;
			if (!gi && !(gi = cpmap['?']))
				continue;
			gl = glyphs + (size_t)(gi - 1) * charsize;
			for (y = 0; y < fh; y++)
				for (x = 0; x < fw; x++) {
					uint32_t *o = px + (pady + y) * W + padx + i * fw + x;
					if (!(gl[y * rowbytes + x / 8] & (0x80 >> (x % 8))))
						continue;
					if (pass)
						*o = fg;
					else
						for (int dy = -1; dy <= 1; dy++)
							for (int dx = -1; dx <= 1; dx++)
								o[dy * (int)W + dx] = ol;
				}
		}
	munmap(px, size);

	pool = wl_shm_create_pool(shm, fd, size);
	buf = wl_shm_pool_create_buffer(pool, 0, W, H, stride, WL_SHM_FORMAT_ARGB8888);
	wl_shm_pool_destroy(pool);
	close(fd);
	wl_buffer_add_listener(buf, &buffer_listener, NULL);

	/* the buffer is in real pixels; the viewport maps it onto logical size */
	nw = (W * 120 + scale120 / 2) / scale120;
	nh = (H * 120 + scale120 / 2) / scale120;
	if (nw != lw || nh != lh) {
		lw = nw, lh = nh;
		zwlr_layer_surface_v1_set_size(ls, lw, lh);
		wp_viewport_set_destination(vp, lw, lh);
	}
	wl_surface_attach(surf, buf, 0, 0);
	wl_surface_damage_buffer(surf, 0, 0, W, H);
	wl_surface_commit(surf);
}

static void destroysurface(void);

static void
ls_configure(void *data, struct zwlr_layer_surface_v1 *l, uint32_t serial,
		uint32_t w, uint32_t h)
{
	zwlr_layer_surface_v1_ack_configure(l, serial);
	/* only the first configure needs a buffer; later ones echo our size */
	if (!configured) {
		configured = 1;
		draw();
	}
}

static void
ls_closed(void *data, struct zwlr_layer_surface_v1 *l)
{
	destroysurface(); /* output went away; the next line re-creates it */
}

static const struct zwlr_layer_surface_v1_listener ls_listener = {
	.configure = ls_configure,
	.closed = ls_closed,
};

static void
preferred_scale(void *data, struct wp_fractional_scale_v1 *f, uint32_t s)
{
	if (s == scale120)
		return;
	scale120 = s;
	draw();
}

static const struct wp_fractional_scale_v1_listener fscale_listener = {
	.preferred_scale = preferred_scale,
};

static void
createsurface(void)
{
	struct wl_region *empty;

	surf = wl_compositor_create_surface(compositor);
	vp = wp_viewporter_get_viewport(viewporter, surf);
	if (fscale_mgr) {
		fscale = wp_fractional_scale_manager_v1_get_fractional_scale(fscale_mgr, surf);
		wp_fractional_scale_v1_add_listener(fscale, &fscale_listener, NULL);
	}
	/* an empty input region makes every click fall through to the window below */
	empty = wl_compositor_create_region(compositor);
	wl_surface_set_input_region(surf, empty);
	wl_region_destroy(empty);

	ls = zwlr_layer_shell_v1_get_layer_surface(layer_shell, surf, NULL,
			ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY, "overlay");
	zwlr_layer_surface_v1_add_listener(ls, &ls_listener, NULL);
	zwlr_layer_surface_v1_set_anchor(ls, anchor);
	zwlr_layer_surface_v1_set_margin(ls, margin, margin, margin, margin);
	/* a placeholder size; draw() sets the real one with the first buffer */
	lw = lh = 1;
	zwlr_layer_surface_v1_set_size(ls, lw, lh);
	configured = 0;
	wl_surface_commit(surf);
}

static void
destroysurface(void)
{
	if (!surf)
		return;
	zwlr_layer_surface_v1_destroy(ls);
	if (fscale)
		wp_fractional_scale_v1_destroy(fscale);
	wp_viewport_destroy(vp);
	wl_surface_destroy(surf);
	surf = NULL, ls = NULL, vp = NULL, fscale = NULL;
	configured = 0;
}

static void
settext(char *line)
{
	const unsigned char *p = (const unsigned char *)line;

	if (!*line) {
		destroysurface();
		return;
	}
	for (ncp = 0; *p && ncp < MAXGLYPHS; ncp++)
		cps[ncp] = utf8next(&p);
	if (surf)
		draw();
	else
		createsurface(); /* draws on its first configure */
}

static void
global(void *data, struct wl_registry *reg, uint32_t name, const char *iface,
		uint32_t version)
{
	if (!strcmp(iface, wl_compositor_interface.name))
		compositor = wl_registry_bind(reg, name, &wl_compositor_interface, 4);
	else if (!strcmp(iface, wl_shm_interface.name))
		shm = wl_registry_bind(reg, name, &wl_shm_interface, 1);
	else if (!strcmp(iface, zwlr_layer_shell_v1_interface.name))
		layer_shell = wl_registry_bind(reg, name, &zwlr_layer_shell_v1_interface,
				version < 4 ? version : 4);
	else if (!strcmp(iface, wp_viewporter_interface.name))
		viewporter = wl_registry_bind(reg, name, &wp_viewporter_interface, 1);
	else if (!strcmp(iface, wp_fractional_scale_manager_v1_interface.name))
		fscale_mgr = wl_registry_bind(reg, name,
				&wp_fractional_scale_manager_v1_interface, 1);
}

static void
global_remove(void *data, struct wl_registry *reg, uint32_t name)
{
}

static const struct wl_registry_listener registry_listener = {
	.global = global,
	.global_remove = global_remove,
};

/* read what stdin has; act on the last complete line. 0 on EOF. */
static int
readstdin(void)
{
	static char buf[4096];
	static size_t len;
	char *nl, *last = NULL, *start;
	ssize_t n;

	if (len == sizeof(buf) - 1)
		len = 0; /* an absurdly long line: drop it rather than wedge */
	n = read(STDIN_FILENO, buf + len, sizeof(buf) - 1 - len);
	if (n < 0)
		return errno == EINTR || errno == EAGAIN;
	if (n == 0)
		return 0;
	len += n;
	buf[len] = '\0';

	for (start = buf; (nl = strchr(start, '\n')); start = nl + 1) {
		*nl = '\0';
		if (nl > start && nl[-1] == '\r')
			nl[-1] = '\0';
		last = start;
	}
	if (last) {
		settext(last);
		len -= start - buf;
		memmove(buf, start, len);
	}
	return 1;
}

static void
usage(void)
{
	fputs("usage: overlay [-p tl|tr|bl|br] [-m margin] [-f #rrggbb[aa]] [-b #rrggbb[aa]]\n"
		"               [-o #rrggbb[aa]]\n"
		"Shows each stdin line in a click-through box; an empty line hides it.\n"
		"-f text, -b background, -o 1px outline around the text (default none).\n", stderr);
	exit(2);
}

int
main(int argc, char **argv)
{
	struct pollfd fds[2];
	const char *fgs = "#dcdcdc", *bgs = "#1e1e1eeb", *ols = NULL;
	int c;

	while ((c = getopt(argc, argv, "p:m:f:b:o:h")) != -1) {
		switch (c) {
		case 'p':
			if (strlen(optarg) != 2 || !strchr("tb", optarg[0]) || !strchr("lr", optarg[1]))
				usage();
			anchor = (optarg[0] == 't' ? ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP
					: ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM)
				| (optarg[1] == 'l' ? ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT
					: ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT);
			break;
		case 'm': margin = atoi(optarg); break;
		case 'f': fgs = optarg; break;
		case 'b': bgs = optarg; break;
		case 'o': ols = optarg; break;
		default: usage();
		}
	}
	fg = parsecolor(fgs);
	bg = parsecolor(bgs);
	ol = ols ? parsecolor(ols) : 0;
	loadfont();

	if (!(dpy = wl_display_connect(NULL)))
		die("cannot connect to the Wayland display");
	wl_registry_add_listener(wl_display_get_registry(dpy), &registry_listener, NULL);
	wl_display_roundtrip(dpy);
	if (!compositor || !shm || !layer_shell)
		die("compositor lacks wl_compositor, wl_shm or wlr-layer-shell");
	if (!viewporter)
		die("compositor lacks wp_viewporter");

	fds[0] = (struct pollfd){ .fd = wl_display_get_fd(dpy), .events = POLLIN };
	fds[1] = (struct pollfd){ .fd = STDIN_FILENO, .events = POLLIN };
	for (;;) {
		while (wl_display_prepare_read(dpy) != 0)
			wl_display_dispatch_pending(dpy);
		wl_display_flush(dpy);
		if (poll(fds, 2, -1) < 0) {
			wl_display_cancel_read(dpy);
			if (errno == EINTR)
				continue;
			die("poll failed");
		}
		if (fds[0].revents & POLLIN) {
			if (wl_display_read_events(dpy) < 0)
				die("lost the Wayland connection");
		} else {
			wl_display_cancel_read(dpy);
		}
		if (fds[0].revents & (POLLERR | POLLHUP))
			die("lost the Wayland connection");
		wl_display_dispatch_pending(dpy);
		if (fds[1].revents & (POLLIN | POLLHUP) && !readstdin())
			break;
	}
	destroysurface();
	wl_display_roundtrip(dpy);
	wl_display_disconnect(dpy);
	return 0;
}
