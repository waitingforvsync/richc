/*
 * app_glfw.c - GLFW + glad backend for rc_app.
 *
 * This is the only file that includes GLFW and glad headers; all
 * GLFW/OpenGL types are kept out of the public headers.
 *
 * GLFW key constants, modifier flags, and mouse button values are
 * intentionally chosen to match the rc_* enums in keys.h exactly so
 * callbacks can cast without a lookup table.
 */

#include "richc/app/app.h"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <glad/gl.h>

#include "richc/macros.h"

/* ---- global state ---- */

static struct {
    GLFWwindow       *window;
    rc_app_callbacks  callbacks;
    rc_mod            current_mods;     /* tracked for on_key_char              */
    double            last_update_time; /* glfwGetTime() at last request_update */
} app;

/* ---- glad proc loader ---- */

static GLADapiproc glad_get_proc(const char *name)
{
    return (GLADapiproc)glfwGetProcAddress(name);
}

/* ---- GLFW callbacks ---- */

static void key_callback(GLFWwindow *w, int key, int scancode, int action, int mods)
{
    (void)w; (void)scancode;
    app.current_mods = (rc_mod)mods;
    if (action == GLFW_PRESS && app.callbacks.on_key_down)
        app.callbacks.on_key_down(app.callbacks.ctx, (rc_scancode)key, (rc_mod)mods);
    else if (action == GLFW_RELEASE && app.callbacks.on_key_up)
        app.callbacks.on_key_up(app.callbacks.ctx, (rc_scancode)key, (rc_mod)mods);
}

static void char_callback(GLFWwindow *w, unsigned int codepoint)
{
    (void)w;
    if (!app.callbacks.on_key_char) return;
    app.callbacks.on_key_char(app.callbacks.ctx, codepoint, app.current_mods);
}

static void mouse_button_callback(GLFWwindow *w, int button, int action, int mods)
{
    (void)w;
    if (action == GLFW_PRESS && app.callbacks.on_mouse_down)
        app.callbacks.on_mouse_down(app.callbacks.ctx, (rc_mouse_button)button, (rc_mod)mods);
    else if (action == GLFW_RELEASE && app.callbacks.on_mouse_up)
        app.callbacks.on_mouse_up(app.callbacks.ctx, (rc_mouse_button)button, (rc_mod)mods);
}

static void cursor_enter_callback(GLFWwindow *w, int entered)
{
    (void)w;
    if (entered && app.callbacks.on_mouse_enter)
        app.callbacks.on_mouse_enter(app.callbacks.ctx);
    else if (!entered && app.callbacks.on_mouse_leave)
        app.callbacks.on_mouse_leave(app.callbacks.ctx);
}

static void cursor_pos_callback(GLFWwindow *w, double x, double y)
{
    (void)w;
    if (!app.callbacks.on_mouse_move) return;
    rc_vec2i pos = { (int32_t)x, (int32_t)y };
    app.callbacks.on_mouse_move(app.callbacks.ctx, pos);
}

static void scroll_callback(GLFWwindow *w, double xoff, double yoff)
{
    (void)w;
    if (!app.callbacks.on_mouse_wheel) return;
    rc_vec2i delta = { (int32_t)xoff, (int32_t)yoff };
    app.callbacks.on_mouse_wheel(app.callbacks.ctx, delta);
}

static void framebuffer_size_callback(GLFWwindow *w, int width, int height)
{
    (void)w;
    if (!app.callbacks.on_resize) return;
    rc_vec2i size = { width, height };
    app.callbacks.on_resize(app.callbacks.ctx, size);
}

static void window_focus_callback(GLFWwindow *w, int focused)
{
    (void)w;
    if (focused) {
        if (app.callbacks.on_focus_gained)
            app.callbacks.on_focus_gained(app.callbacks.ctx);
    } else {
        // Clear tracked modifiers: we won't receive key-up events for keys
        // that were held when focus was lost.
        app.current_mods = (rc_mod)0;
        if (app.callbacks.on_focus_lost)
            app.callbacks.on_focus_lost(app.callbacks.ctx);
    }
}

static void window_iconify_callback(GLFWwindow *w, int iconified)
{
    (void)w;
    if (iconified && app.callbacks.on_minimize)
        app.callbacks.on_minimize(app.callbacks.ctx);
}

static void window_maximize_callback(GLFWwindow *w, int maximized)
{
    (void)w;
    if (maximized && app.callbacks.on_maximize)
        app.callbacks.on_maximize(app.callbacks.ctx);
}

/* Invoke on_render(ctx, size) and swap, skipping both entirely while the
 * framebuffer is zero-sized (minimised): there is nothing to render into. */
static void render_and_swap(void)
{
    if (!app.callbacks.on_render) return;
    int width = 0;
    int height = 0;
    glfwGetFramebufferSize(app.window, &width, &height);
    if (width <= 0 || height <= 0) return;
    app.callbacks.on_render(app.callbacks.ctx, rc_vec2i_make(width, height));
    glfwSwapBuffers(app.window);
}

static void window_refresh_callback(GLFWwindow *w)
{
    (void)w;
    render_and_swap();
}

/* ---- public API ---- */

void rc_app_init(const rc_app_desc *desc)
{
    RC_PANIC(glfwInit());

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_RESIZABLE, desc->resizable ? GLFW_TRUE : GLFW_FALSE);
    if (desc->hidden)
        glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);

    // always request an sRGB-capable default framebuffer: capability does not
    // force encoding (GL_FRAMEBUFFER_SRGB and the format do), and gfx wants
    // the hardware-encode present path whenever the driver grants it
    glfwWindowHint(GLFW_SRGB_CAPABLE, GLFW_TRUE);

    char title_buf[256];
    const char *title_cstr = rc_str_as_cstr(desc->title, title_buf, (uint32_t)sizeof(title_buf));

    app.window = glfwCreateWindow(desc->size.x, desc->size.y,
                                  title_cstr ? title_cstr : "", NULL, NULL);
    RC_PANIC(app.window != NULL);

    glfwMakeContextCurrent(app.window);
    glfwSwapInterval(1);

    RC_PANIC(gladLoadGL(glad_get_proc));

    app.callbacks        = desc->callbacks;
    app.current_mods     = (rc_mod)0;
    app.last_update_time = glfwGetTime();

    glfwSetKeyCallback             (app.window, key_callback);
    glfwSetCharCallback            (app.window, char_callback);
    glfwSetMouseButtonCallback     (app.window, mouse_button_callback);
    glfwSetCursorEnterCallback     (app.window, cursor_enter_callback);
    glfwSetCursorPosCallback       (app.window, cursor_pos_callback);
    glfwSetScrollCallback          (app.window, scroll_callback);
    glfwSetFramebufferSizeCallback (app.window, framebuffer_size_callback);
    glfwSetWindowFocusCallback     (app.window, window_focus_callback);
    glfwSetWindowIconifyCallback   (app.window, window_iconify_callback);
    glfwSetWindowMaximizeCallback  (app.window, window_maximize_callback);
    glfwSetWindowRefreshCallback   (app.window, window_refresh_callback);
}

void rc_app_destroy(void)
{
    glfwDestroyWindow(app.window);
    glfwTerminate();
    app.window = NULL;
}

void rc_app_poll(void)
{
    glfwPollEvents();
}

bool rc_app_is_running(void)
{
    return !glfwWindowShouldClose(app.window);
}

rc_vec2i rc_app_size(void)
{
    int fw, fh;
    glfwGetFramebufferSize(app.window, &fw, &fh);
    return rc_vec2i_make(fw, fh);
}

void rc_app_request_update(void)
{
    if (!app.callbacks.on_update) return;
    double now = glfwGetTime();
    double dt  = now - app.last_update_time;
    app.last_update_time = now;
    app.callbacks.on_update(app.callbacks.ctx, dt);
}

void rc_app_request_render(void)
{
    render_and_swap();
}

void rc_app_swap_buffers(void)
{
    glfwSwapBuffers(app.window);
}

double rc_app_time(void)
{
    return glfwGetTime();
}
