#include <stdio.h>
#include <GL/gl.h>
#include <GL/glx.h>
#include <X11/X.h>
#include <X11/keysym.h>

#include <string.h>

#define KS(x,y) keymap[(x)&FT_KEYBOARDSTATE_SIZE]=(y)

static int snglBuf[] = { GLX_RGBA, GLX_DEPTH_SIZE, 16, None };
static int dblBuf[] = { GLX_RGBA, GLX_DEPTH_SIZE, 16, GLX_DOUBLEBUFFER, None };

static int keymap[FT_KEYBOARDSTATE_SIZE] = { 0 };

Display *dpy;
Window win;
GLboolean doubleBuffer = GL_TRUE;

void keyMapSetting()
{
	KS(XK_w, FT_W);
	KS(XK_a, FT_A);
	KS(XK_s, FT_S);
	KS(XK_d, FT_D);
}

void fatalError(const char *s)
{
	printf("%s", s);
}

int main(int argc, char **argv)
{
	fountain::basicSetting();

	keyMapSetting();

	XVisualInfo *vi;
	Colormap cmap;
	XSetWindowAttributes swa;
	GLXContext cx;
	XEvent event;
	int dummy;

	dpy = XOpenDisplay(NULL);

	if (dpy == NULL)
		fatalError("could not open display");

	if (!glXQueryExtension(dpy, &dummy, &dummy))
		fatalError("X server has no OpenGL GLX extension");

	vi = glXChooseVisual(dpy, DefaultScreen(dpy), dblBuf);
	if (vi == NULL) {
		vi = glXChooseVisual(dpy, DefaultScreen(dpy), snglBuf);
		if (vi == NULL)
			fatalError("no RGB visual with depth buffer");
		doubleBuffer = GL_FALSE;
	}
	//if(vi->class != TrueColor)
	//  fatalError("TrueColor visual required for this program");

	cx = glXCreateContext(dpy, vi, /* no shared dlists */ None,
			      /* direct rendering if possible */ GL_TRUE);
	if (cx == NULL)
		fatalError("could not create rendering context");

	cmap =
	    XCreateColormap(dpy, RootWindow(dpy, vi->screen), vi->visual,
			    AllocNone);
	swa.colormap = cmap;
	swa.border_pixel = 0;
	swa.override_redirect = False;
	swa.event_mask = KeyPressMask | KeyReleaseMask | ExposureMask
	    | ButtonPressMask | ButtonReleaseMask | StructureNotifyMask |
	    PointerMotionMask;
	win =
	    XCreateWindow(dpy, RootWindow(dpy, vi->screen), 0, 0,
			  fountain::mainWin.w, fountain::mainWin.h, 0,
			  vi->depth, InputOutput, vi->visual,
			  CWBorderPixel | CWColormap | CWEventMask, &swa);
	XSetStandardProperties(dpy, win, fountain::mainWin.title.c_str(),
			       "main", None, argv, argc, NULL);

	glXMakeCurrent(dpy, win, cx);

	Atom wmDeleteMessage = XInternAtom(dpy, "WM_DELETE_WINDOW", false);

	XMapWindow(dpy, win);

	if (fountain::mainWin.isFullScreen) {
		XWindowAttributes xwa;
		XGetWindowAttributes(dpy, DefaultRootWindow(dpy), &xwa);
		fountain::mainWin.w = xwa.width;
		fountain::mainWin.h = xwa.height;

		XEvent xev;
		Atom wm_state = XInternAtom(dpy, "_NET_WM_STATE", False);
		Atom fullscreen =
		    XInternAtom(dpy, "_NET_WM_STATE_FULLSCREEN", False);

		memset(&xev, 0, sizeof(xev));
		xev.type = ClientMessage;
		xev.xclient.window = win;
		xev.xclient.message_type = wm_state;
		xev.xclient.format = 32;
		xev.xclient.data.l[0] = 1;
		xev.xclient.data.l[1] = fullscreen;
		xev.xclient.data.l[2] = 0;

		XSendEvent(dpy, DefaultRootWindow(dpy), False,
			   SubstructureNotifyMask, &xev);
	}

	if (fountain::mainWin.hideCursor) {
		Display *display = dpy;

		Cursor invisibleCursor;
		Pixmap bitmapNoData;
		XColor black;
		static char noData[] = { 0, 0, 0, 0, 0, 0, 0, 0 };
		black.red = black.green = black.blue = 0;

		bitmapNoData =
		    XCreateBitmapFromData(display, win, noData, 8, 8);
		invisibleCursor =
		    XCreatePixmapCursor(display, bitmapNoData, bitmapNoData,
					&black, &black, 0, 0);
		XDefineCursor(display, win, invisibleCursor);
		XFreeCursor(display, invisibleCursor);
	}

	XSetWMProtocols(dpy, win, &wmDeleteMessage, 1);

	fountain::initAllSystem();
	fountain::gameInit();

	for (;;) {
		while (XPending(dpy) > 0) {
			XNextEvent(dpy, &event);
			switch (event.type) {
			case ClientMessage:
				if (event.xclient.data.l[0] ==
				    (int)wmDeleteMessage) {
					XDestroyWindow(dpy,
						       event.xclient.window);
					XCloseDisplay(dpy);
					return 0;
				}
				break;

			case KeyPress:
				{
					KeySym keysym;
					KeySym keycodeSym;
					XKeyEvent *kevent;
					char buffer[1];
					kevent = (XKeyEvent *) & event;
					if ((XLookupString
					     ((XKeyEvent *) & event, buffer, 1,
					      &keysym, NULL) == 1)
					    && (keysym == (KeySym) XK_Escape)) {
						XDestroyWindow(dpy,
							       event.xclient.
							       window);
						XCloseDisplay(dpy);
						return 0;
					}
					keycodeSym =
					    XLookupKeysym(kevent,
							  0) &
					    FT_KEYBOARDSTATE_SIZE;
					fountain::sysKeyboard.setState(keymap
								       [keycodeSym],
								       1);
				}
				break;

			case KeyRelease:
				{
					unsigned short is_retriggered = 0;
					if (XEventsQueued
					    (dpy, QueuedAfterReading)) {
						XEvent nev;
						XPeekEvent(dpy, &nev);

						if (nev.type == KeyPress
						    && nev.xkey.time ==
						    event.xkey.time
						    && nev.xkey.keycode ==
						    event.xkey.keycode) {
							printf
							    ("keyRetriggered\n");
							//TODO: set the sysKeyboard
							XNextEvent(dpy, &event);
							is_retriggered = 1;
						}
					}
					if (!is_retriggered) {
						XKeyEvent *kevent;
						KeySym keycodeSym;
						kevent = (XKeyEvent *) & event;
						keycodeSym =
						    XLookupKeysym(kevent,
								  0) &
						    FT_KEYBOARDSTATE_SIZE;
						fountain::sysKeyboard.
						    setState(keymap[keycodeSym],
							     0);
					}
				}
				break;

			case ButtonPress:
				fountain::sysMouse.setState(event.xbutton.
							    button, 1);
				break;

			case ButtonRelease:
				fountain::sysMouse.setState(event.xbutton.
							    button, 0);
				break;

			case MotionNotify:
				fountain::sysMouse.update(event.xmotion.x,
							  event.xmotion.y);
				break;

			case ConfigureNotify:
				glViewport(0, 0, event.xconfigure.width,
					   event.xconfigure.height);
				break;

			case Expose:
				break;

			}
		}
		//TODO: move the outside OpenGL word to ft_render
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		glColor3f(1.0, 1.0, 1.0);
		glPushMatrix();
		fountain::singleFrame();
		glPopMatrix();
		glXSwapBuffers(dpy, win);
	}
	return 0;
}