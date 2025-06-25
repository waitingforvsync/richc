#include "richc/app/app.h"
#include "app/x11/opengl.h"
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <GL/glx.h>
#include <GL/glxext.h>


static Display *display;
static Window window;
static Atom delete_message;
static GLXContext context;

static void *app_context;
static app_callbacks_t app_callbacks;


void app_init(app_desc_t *desc) {
    display = XOpenDisplay(0);
    require(display);

    int fb_attribs[] = {
        GLX_X_RENDERABLE,                   True,
        GLX_DRAWABLE_TYPE,                  GLX_WINDOW_BIT,
        GLX_RENDER_TYPE,                    GLX_RGBA_BIT,
        GLX_X_VISUAL_TYPE,                  GLX_TRUE_COLOR,
        GLX_RED_SIZE,                       8,
        GLX_GREEN_SIZE,                     8,
        GLX_BLUE_SIZE,                      8,
        GLX_ALPHA_SIZE,                     8,
        GLX_DEPTH_SIZE,                     24,
        GLX_STENCIL_SIZE,                   8,
        GLX_DOUBLEBUFFER,                   True,
        GLX_FRAMEBUFFER_SRGB_CAPABLE_ARB,   desc->srgb ? True : False,
        None
    };

    int fb_count = 0;
    GLXFBConfig *fb_configs = glXChooseFBConfig(
        display,
        DefaultScreen(display),
        fb_attribs,
        &fb_count
    );
    require(fb_configs);
    GLXFBConfig fb_config = fb_configs[0];
    XFree(fb_configs);

    XVisualInfo *visual_info = glXGetVisualFromFBConfig(display, fb_config);
    require(visual_info);

    Colormap colormap = XCreateColormap(
        display,
        RootWindow(display, visual_info->screen),
        visual_info->visual,
        AllocNone
    );

    window = XCreateWindow(
        display,
        RootWindow(display, visual_info->screen),
        0, 0,
        desc->width, desc->height,
        0,
        visual_info->depth,
        InputOutput,
        visual_info->visual,
        CWColormap | CWEventMask,
        &(XSetWindowAttributes) {
            .colormap = colormap,
            .event_mask =
                KeyPressMask |
                KeyReleaseMask |
                PointerMotionMask |
                ExposureMask
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

    typedef GLXContext (*CreateContextAttribsARB)(Display *, GLXFBConfig, GLXContext, Bool, const int *);
    CreateContextAttribsARB glXCreateContextAttribsARB = (CreateContextAttribsARB)glXGetProcAddressARB((const GLubyte *)"glXCreateContextAttribsARB");
    require(glXCreateContextAttribsARB);

    static int ctx_attribs[] = {
        GLX_CONTEXT_MAJOR_VERSION_ARB, 3,
        GLX_CONTEXT_MINOR_VERSION_ARB, 2,
        GLX_CONTEXT_PROFILE_MASK_ARB,  GLX_CONTEXT_CORE_PROFILE_BIT_ARB,
        None
    };

    context = glXCreateContextAttribsARB(display, fb_config, 0, True, ctx_attribs);
    require(context);
    glXMakeCurrent(display, window, context);
    opengl_init();

    XFree(visual_info);

    app_context = desc->context;
    app_callbacks = desc->callbacks;
}


void app_set_window_title(const char *title) {
    XStoreName(display, window, title);
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
                // https://stackoverflow.com/questions/2100654/ignore-auto-repeat-in-x11-applications
                    if (app_callbacks.on_key_down) {
                        app_callbacks.on_key_down(app_context, event.xkey.keycode); // @todo translate
                    }
                    break;
                case KeyRelease:
                    if (app_callbacks.on_key_up) {
                        app_callbacks.on_key_up(app_context, event.xkey.keycode); // @todo translate
                    }
                    break;
            }
        }

        if (app_callbacks.on_paint) {
            app_callbacks.on_paint(app_context);
            glXSwapBuffers(display, window);
        }
    }
}


void app_deinit(void) {
    glXMakeCurrent(display, None, 0);
    glXDestroyContext(display, context);
    XDestroyWindow(display, window);
    XCloseDisplay(display);
}


