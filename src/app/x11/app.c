#include "richc/app/app.h"
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>


static Display *display;
static Window window;
static Atom delete_message;

static void *app_context;
static app_callbacks_t app_callbacks;


void app_init(app_desc_t *desc) {
    display = XOpenDisplay(0);
    require(display);

    window = XCreateWindow(
        display,
        RootWindow(display, 0),
        0, 0,
        desc->width, desc->height,
        0,
        CopyFromParent,
        InputOutput,
        CopyFromParent,
        CWEventMask,
        &(XSetWindowAttributes) {
            .event_mask =
                KeyPressMask |
                KeyReleaseMask |
                PointerMotionMask
        }
    );

    XStoreName(display, window, desc->title);

    if (!desc->resizable) {
        XSetWMNormalHints(display, window, &(XSizeHints) {
            .flags = PMinSize | PMaxSize,
            .min_width = desc->width,
            .min_height = desc->height,
            .max_width = desc->width,
            .max_height = desc->height
        });
    }

    delete_message = XInternAtom(display, "WM_DELETE_WINDOW", False);
    XSetWMProtocols(display, window, &delete_message, 1);
    XMapWindow(display, window);

    app_context = desc->context;
    app_callbacks = desc->callbacks;
}


void app_run(void) {
    bool running = true;
    while (running) {
        while (XPending(display)) {
            XEvent event;
            XNextEvent(display, &event);
            check(event.xany.window == window);
            switch (event.type) {
                case ConfigureNotify:
                    break;
                case Expose:
                    break;
                case ClientMessage:
                    if ((Atom)event.xclient.data.l[0] == delete_message) {
                        running = false;
                    }
                    break;
                case FocusIn:
                    break;
                case FocusOut:
                    break;
                case MotionNotify:
                    break;
                case ButtonPress:
                    break;
                case ButtonRelease:
                    break;
                case KeyPress:
                    if (app_callbacks.on_key_press) {
                        app_callbacks.on_key_press(app_context, event.xkey.keycode); // @todo translate
                    }
                    break;
                case KeyRelease:
                    if (app_callbacks.on_key_release) {
                        app_callbacks.on_key_release(app_context, event.xkey.keycode); // @todo translate
                    }
                    break;
            }
        }

        if (app_callbacks.on_paint) {
            app_callbacks.on_paint(app_context);
        }
    }
}


void app_deinit(void) {
    XDestroyWindow(display, window);
    XCloseDisplay(display);
}


