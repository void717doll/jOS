/* See LICENSE file for license details. */
#define _XOPEN_SOURCE 500
#if HAVE_SHADOW_H
#include <shadow.h>
#endif

#include <ctype.h>
#include <errno.h>
#include <crypt.h>
#include <poll.h>
#include <time.h>
#include <grp.h>
#include <pwd.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <X11/extensions/Xrandr.h>
#include <X11/keysym.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/XKBlib.h>
#include <X11/Xft/Xft.h>
#include <X11/cursorfont.h>

#include <X11/extensions/dpms.h>

char *argv0;

enum { INIT, INPUT, FAILED, CHECKING };
enum { Background, Field, Text, Accent, SelectedText, Error, NUMCOLS };

struct lock {
	int screen, width, height;
	Window root, win;
	Pixmap buffer;
	GC gc;
	Cursor cursor;
	XftFont *font;
	XftDraw *draw;
	XftColor colors[NUMCOLS];
	XRRMonitorInfo *monitors;
	int nmonitors;
};

static char account[256];

struct xrandr {
	int active;
	int evbase;
	int errbase;
};

#include "config.h"
static void die(const char *errstr, ...);
#include "ui.c"

	static void
die(const char *errstr, ...)
{
	va_list ap;

	va_start(ap, errstr);
	vfprintf(stderr, errstr, ap);
	va_end(ap);
	exit(1);
}

#ifdef __linux__
#include <fcntl.h>
#include <linux/oom.h>

	static void
dontkillme(void)
{
	FILE *f;
	const char oomfile[] = "/proc/self/oom_score_adj";

	if (!(f = fopen(oomfile, "w"))) {
		if (errno == ENOENT)
			return;
		die("slock: fopen %s: %s\n", oomfile, strerror(errno));
	}
	fprintf(f, "%d", OOM_SCORE_ADJ_MIN);
	if (fclose(f)) {
		if (errno == EACCES)
			die("slock: unable to disable OOM killer. "
					"Make sure to suid or sgid slock.\n");
		else
			die("slock: fclose %s: %s\n", oomfile, strerror(errno));
	}
}
#endif

	static const char *
gethash(void)
{
	const char *hash;
	struct passwd *pw;

	/* Check if the current user has a password entry */
	errno = 0;
	if (!(pw = getpwuid(getuid()))) {
		if (errno)
			die("slock: getpwuid: %s\n", strerror(errno));
		else
			die("slock: cannot retrieve password entry\n");
	}
	hash = pw->pw_passwd;

#if HAVE_SHADOW_H
	if (!strcmp(hash, "x")) {
		struct spwd *sp;
		if (!(sp = getspnam(pw->pw_name)))
			die("slock: getspnam: cannot retrieve shadow entry. "
					"Make sure to suid or sgid slock.\n");
		hash = sp->sp_pwdp;
	}
#else
	if (!strcmp(hash, "*")) {
#ifdef __OpenBSD__
		if (!(pw = getpwuid_shadow(getuid())))
			die("slock: getpwnam_shadow: cannot retrieve shadow entry. "
					"Make sure to suid or sgid slock.\n");
		hash = pw->pw_passwd;
#else
		die("slock: getpwuid: cannot retrieve shadow entry. "
				"Make sure to suid or sgid slock.\n");
#endif /* __OpenBSD__ */
	}
#endif /* HAVE_SHADOW_H */

	return hash;
}

	static void
readpw(Display *dpy, struct xrandr *rr, struct lock **locks, int nscreens,
		const char *hash)
{
	XRRScreenChangeNotifyEvent *rre;
	char buf[32], passwd[256] = {0}, *inputhash;
	int num, screen, running, failure, submit, redraw, caps = 0, cursor = 0;
	unsigned int len, start;
	XkbStateRec keyboard;
	KeySym ksym;
	XEvent ev;
	struct pollfd input = { ConnectionNumber(dpy), POLLIN, 0 };
	struct timespec last, now, cursor_until;
	int screenoff = 0, waitms, cursor_waitms, ready;

	len = 0;
	running = 1;
	failure = 0;
	if (XkbGetState(dpy, XkbUseCoreKbd, &keyboard) == Success)
		caps = !!(keyboard.locked_mods & LockMask);
	for (screen = 0; screen < nscreens; screen++)
		drawlock(dpy, locks[screen], 0, INIT, caps, 0);
	clock_gettime(CLOCK_MONOTONIC, &last);

	while (running) {
		clock_gettime(CLOCK_MONOTONIC, &now);
		waitms = timeoff > 0 && !screenoff ?
			timeoff * 1000 - (now.tv_sec - last.tv_sec) * 1000 -
			(now.tv_nsec - last.tv_nsec) / 1000000 : -1;
		if (cursor) {
			cursor_waitms = (cursor_until.tv_sec - now.tv_sec) * 1000 +
				(cursor_until.tv_nsec - now.tv_nsec) / 1000000;
			if (cursor_waitms <= 0) {
				unsigned int count = 0;
				cursor = 0;
				for (start = 0; start < len; start++)
					if (((unsigned char)passwd[start] & 0xc0) != 0x80)
						count++;
				for (screen = 0; screen < nscreens; screen++)
					drawlock(dpy, locks[screen], count, failure ? FAILED : len ? INPUT : INIT, caps, 0);
			} else if (waitms < 0 || cursor_waitms < waitms) {
				waitms = cursor_waitms;
			}
		}
		if (timeoff > 0 && !screenoff && waitms <= 0) {
			if (DPMSCapable(dpy)) {
				DPMSEnable(dpy);
				DPMSForceLevel(dpy, DPMSModeOff);
				XFlush(dpy);
			}
			screenoff = 1;
			waitms = -1;
		}
		if (!XPending(dpy)) {
			ready = poll(&input, 1, waitms);
			if (ready == 0 || (ready < 0 && errno == EINTR))
				continue;
			if (ready < 0)
				die("slock: poll: %s\n", strerror(errno));
		}
		XNextEvent(dpy, &ev);
		if (ev.type == KeyPress || ev.type == KeyRelease ||
		    ev.type == ButtonPress || ev.type == ButtonRelease || ev.type == MotionNotify) {
			clock_gettime(CLOCK_MONOTONIC, &last);
			if (screenoff && DPMSCapable(dpy)) {
				DPMSForceLevel(dpy, DPMSModeOn);
				XFlush(dpy);
			}
			screenoff = 0;
		}
		submit = redraw = 0;
		if (XkbGetState(dpy, XkbUseCoreKbd, &keyboard) == Success)
			caps = !!(keyboard.locked_mods & LockMask);
		if (ev.type == KeyPress) {
			explicit_bzero(&buf, sizeof(buf));
			num = XLookupString(&ev.xkey, buf, sizeof(buf), &ksym, 0);
			if (IsKeypadKey(ksym)) {
				if (ksym == XK_KP_Enter)
					ksym = XK_Return;
				else if (ksym >= XK_KP_0 && ksym <= XK_KP_9)
					ksym = (ksym - XK_KP_0) + XK_0;
			}
			if (IsFunctionKey(ksym) ||
					IsKeypadKey(ksym) ||
					IsMiscFunctionKey(ksym) ||
					IsPFKey(ksym) ||
					IsPrivateKeypadKey(ksym))
				continue;
			cursor = 1;
			switch (ksym) {
				case XK_Return:
					submit = 1;
					break;
				case XK_Escape:
					explicit_bzero(&passwd, sizeof(passwd));
					len = 0;
					failure = 0;
					break;
				case XK_BackSpace:
					if (ev.xkey.state & ControlMask) {
						explicit_bzero(passwd, sizeof passwd);
						len = 0;
					} else if (len) {
						start = len - 1;
						while (start && ((unsigned char)passwd[start] & 0xc0) == 0x80)
							start--;
						explicit_bzero(passwd + start, len - start);
						len = start;
					}
					failure = 0;
					break;
				default:
					if (num && !iscntrl((unsigned char)buf[0]) &&
							(len + num < sizeof(passwd))) {
						memcpy(passwd + len, buf, num);
						len += num;
						failure = 0;
					} else if (buf[0] == '\025') { /* ctrl-u clears input */
						explicit_bzero(&passwd, sizeof(passwd));
						len = 0;
						failure = 0;
					}
					break;
			}
			clock_gettime(CLOCK_MONOTONIC, &cursor_until);
			cursor_until.tv_nsec += 700000000;
			if (cursor_until.tv_nsec >= 1000000000) {
				cursor_until.tv_sec++;
				cursor_until.tv_nsec -= 1000000000;
			}
			explicit_bzero(buf, sizeof buf);
			redraw = 1;
		} else if (ev.type == Expose || ev.type == VisibilityNotify) {
			if (ev.type == VisibilityNotify && ev.xvisibility.state != VisibilityUnobscured)
				for (screen = 0; screen < nscreens; screen++)
					XRaiseWindow(dpy, locks[screen]->win);
			redraw = 1;
		} else if (rr->active && ev.type == rr->evbase + RRScreenChangeNotify) {
			rre = (XRRScreenChangeNotifyEvent*)&ev;
			for (screen = 0; screen < nscreens; screen++) {
				if (locks[screen]->win == rre->window) {
					if (rre->rotation == RR_Rotate_90 || rre->rotation == RR_Rotate_270) {
						locks[screen]->width = rre->height;
						locks[screen]->height = rre->width;
					} else {
						locks[screen]->width = rre->width;
						locks[screen]->height = rre->height;
					}
					XResizeWindow(dpy, locks[screen]->win, locks[screen]->width, locks[screen]->height);
					resizeui(dpy, locks[screen]);
					redraw = 1;
					break;
				}
			}
		} else {
			for (screen = 0; screen < nscreens; screen++)
				XRaiseWindow(dpy, locks[screen]->win);
		}
		if (submit) {
			cursor = 0;
			for (screen = 0; screen < nscreens; screen++)
				drawlock(dpy, locks[screen], 0, CHECKING, caps, 0);
			passwd[len] = '\0';
			errno = 0;
			inputhash = crypt(passwd, hash);
			if (!inputhash)
				fprintf(stderr, "slock: crypt: %s\n", strerror(errno));
			running = !inputhash || strcmp(inputhash, hash);
			if (running) {
				XBell(dpy, 100);
				failure = 1;
			}
			explicit_bzero(passwd, sizeof passwd);
			len = 0;
			redraw = 1;
		}
		if (running && redraw) {
			unsigned int count = 0;
			for (start = 0; start < len; start++)
				if (((unsigned char)passwd[start] & 0xc0) != 0x80)
					count++;
			for (screen = 0; screen < nscreens; screen++)
				drawlock(dpy, locks[screen], count, failure ? FAILED : len ? INPUT : INIT, caps, cursor);
		}
	}
}

	static struct lock *
lockscreen(Display *dpy, struct xrandr *rr, int screen)
{
	int i, ptgrab, kbgrab;
	struct lock *lock;
	XSetWindowAttributes wa;

	if (dpy == NULL || screen < 0 || !(lock = calloc(1, sizeof(struct lock))))
		return NULL;

	lock->screen = screen;
	lock->root = RootWindow(dpy, lock->screen);

	lock->width = DisplayWidth(dpy, screen);
	lock->height = DisplayHeight(dpy, screen);
	for (i = 0; i < NUMCOLS; i++)
		if (!XftColorAllocName(dpy, DefaultVisual(dpy, screen), DefaultColormap(dpy, screen),
		    colorname[i], &lock->colors[i]))
			die("slock: cannot allocate ui color\n");
	if (!(lock->font = XftFontOpenName(dpy, screen, fontname)))
		die("slock: cannot load lock font\n");
	wa.override_redirect = 1;
	wa.background_pixel = lock->colors[Background].pixel;
	lock->win = XCreateWindow(dpy, lock->root, 0, 0, lock->width, lock->height,
		0, DefaultDepth(dpy, screen), CopyFromParent, DefaultVisual(dpy, screen),
		CWOverrideRedirect | CWBackPixel, &wa);
	lock->gc = XCreateGC(dpy, lock->win, 0, NULL);
	lock->cursor = XCreateFontCursor(dpy, XC_left_ptr);
	XDefineCursor(dpy, lock->win, lock->cursor);
	XSelectInput(dpy, lock->win, ExposureMask|VisibilityChangeMask);
	resizeui(dpy, lock);

	/* Try to grab mouse pointer *and* keyboard for 600ms, else fail the lock */
	for (i = 0, ptgrab = kbgrab = -1; i < 6; i++) {
		if (ptgrab != GrabSuccess) {
			ptgrab = XGrabPointer(dpy, lock->root, False,
					ButtonPressMask | ButtonReleaseMask |
					PointerMotionMask, GrabModeAsync,
					GrabModeAsync, None, lock->cursor, CurrentTime);
		}
		if (kbgrab != GrabSuccess) {
			kbgrab = XGrabKeyboard(dpy, lock->root, True,
					GrabModeAsync, GrabModeAsync, CurrentTime);
		}

		/* input is grabbed: we can lock the screen */
		if (ptgrab == GrabSuccess && kbgrab == GrabSuccess) {
			XMapRaised(dpy, lock->win);
			if (rr->active)
				XRRSelectInput(dpy, lock->win, RRScreenChangeNotifyMask);

			XSelectInput(dpy, lock->root, SubstructureNotifyMask);
			return lock;
		}

		/* retry on AlreadyGrabbed but fail on other errors */
		if ((ptgrab != AlreadyGrabbed && ptgrab != GrabSuccess) ||
				(kbgrab != AlreadyGrabbed && kbgrab != GrabSuccess))
			break;

		usleep(100000);
	}

	/* we couldn't grab all input: fail out */
	if (ptgrab != GrabSuccess)
		fprintf(stderr, "slock: unable to grab mouse pointer for screen %d\n",
				screen);
	if (kbgrab != GrabSuccess)
		fprintf(stderr, "slock: unable to grab keyboard for screen %d\n",
				screen);
	return NULL;
}

	static void
usage(void)
{
	die("usage: slock [-v]\n");
}

int
main(int argc, char **argv) {
	struct xrandr rr;
	struct lock **locks;
	struct passwd *pwd;
	struct group *grp;
	uid_t duid;
	gid_t dgid;
	const char *hash;
	Display *dpy;
	int s, nlocks, nscreens;

	if (argc > 1 && !strcmp(argv[1], "-v")) {
		puts("slock-"VERSION);
		return 0;
	} else if (argc > 1) {
		usage();
	}

	if (!(pwd = getpwuid(getuid())))
		die("slock: cannot find current account\n");
	snprintf(account, sizeof account, "%s", pwd->pw_name);

	/* validate drop-user and -group */
	errno = 0;
	if (!(pwd = getpwnam(user)))
		die("slock: getpwnam %s: %s\n", user,
				errno ? strerror(errno) : "user entry not found");
	duid = pwd->pw_uid;
	errno = 0;
	if (!(grp = getgrnam(group)))
		die("slock: getgrnam %s: %s\n", group,
				errno ? strerror(errno) : "group entry not found");
	dgid = grp->gr_gid;

#ifdef __linux__
	dontkillme();
#endif

	hash = gethash();
	errno = 0;
	if (!crypt("", hash))
		die("slock: crypt: %s\n", strerror(errno));

	if (!(dpy = XOpenDisplay(NULL)))
		die("slock: cannot open display\n");

	/* drop privileges */
	if (setgroups(0, NULL) < 0)
		die("slock: setgroups: %s\n", strerror(errno));
	if (setgid(dgid) < 0)
		die("slock: setgid: %s\n", strerror(errno));
	if (setuid(duid) < 0)
		die("slock: setuid: %s\n", strerror(errno));

	/* check for Xrandr support */
	rr.active = XRRQueryExtension(dpy, &rr.evbase, &rr.errbase);

	/* get number of screens in display "dpy" and blank them */
	nscreens = ScreenCount(dpy);
	if (!(locks = calloc(nscreens, sizeof(struct lock *))))
		die("slock: out of memory\n");
	for (nlocks = 0, s = 0; s < nscreens; s++) {
		if ((locks[s] = lockscreen(dpy, &rr, s)) != NULL)
			nlocks++;
		else
			break;
	}
	XSync(dpy, 0);

	/* did we manage to lock everything? */
	if (nlocks != nscreens)
		return 1;

	/* everything is now blank. Wait for the correct password */
	readpw(dpy, &rr, locks, nscreens, hash);

	return 0;
}
