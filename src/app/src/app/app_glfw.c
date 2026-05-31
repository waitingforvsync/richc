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
} app_;

/* ---- glad proc loader ---- */

static GLADapiproc glad_get_proc_(const char *name)
{
    return (GLADapiproc)glfwGetProcAddress(name);
}

/* ---- GLFW callbacks ---- */

static void key_callback_(GLFWwindow *w, int key, int scancode, int action, int mods)
{
    (void)w; (void)scancode;
    app_.current_mods = (rc_mod)mods;
    if (action == GLFW_PRESS && app_.callbacks.on_key_down)
        app_.callbacks.on_key_down(app_.callbacks.ctx, (rc_scancode)key, (rc_mod)mods);
    else if (action == GLFW_RELEASE && app_.callbacks.on_key_up)
        app_.callbacks.on_key_up(app_.callbacks.ctx, (rc_scancode)key, (rc_mod)mods);
}

static void char_callback_(GLFWwindow *w, unsigned int codepoint)
{
    (void)w;
    if (!app_.callbacks.on_key_char) return;
    app_.callbacks.on_key_char(app_.callbacks.ctx, codepoint, app_.current_mods);
}

static void mouse_button_callback_(GLFWwindow *w, int button, int action, int mods)
{
    (void)w;
    if (action == GLFW_PRESS && app_.callbacks.on_mouse_down)
        app_.callbacks.on_mouse_down(app_.callbacks.ctx, (rc_mouse_button)button, (rc_mod)mods);
    else if (action == GLFW_RELEASE && app_.callbacks.on_mouse_up)
        app_.callbacks.on_mouse_up(app_.callbacks.ctx, (rc_mouse_button)button, (rc_mod)mods);
}

static void cursor_enter_callback_(GLFWwindow *w, int entered)
{
    (void)w;
    if (entered && app_.callbacks.on_mouse_enter)
        app_.callbacks.on_mouse_enter(app_.callbacks.ctx);
    else if (!entered && app_.callbacks.on_mouse_leave)
        app_.callbacks.on_mouse_leave(app_.callbacks.ctx);
}

static void cursor_pos_callback_(GLFWwindow *w, double x, double y)
{
    (void)w;
    if (!app_.callbacks.on_mouse_move) return;
    rc_vec2i pos = { (int32_t)x, (int32_t)y };
    app_.callbacks.on_mouse_move(app_.callbacks.ctx, pos);
}

static void scroll_callback_(GLFWwindow *w, double xoff, double yoff)
{
    (void)w;
    if (!app_.callbacks.on_mouse_wheel) return;
    rc_vec2i delta = { (int32_t)xoff, (int32_t)yoff };
    app_.callbacks.on_mouse_wheel(app_.callbacks.ctx, delta);
}

static void framebuffer_size_callback_(GLFWwindow *w, int width, int height)
{
    (void)w;
    if (!app_.callbacks.on_resize) return;
    rc_vec2i size = { width, height };
    app_.callbacks.on_resize(app_.callbacks.ctx, size);
}

static void window_focus_callback_(GLFWwindow *w, int focused)
{
    (void)w;
    if (focused) {
        if (app_.callbacks.on_focus_gained)
            app_.callbacks.on_focus_gained(app_.callbacks.ctx);
    } else {
        // Clear tracked modifiers: we won't receive key-up events for keys
        // that were held when focus was lost.
        app_.current_mods = (rc_mod)0;
        if (app_.callbacks.on_focus_lost)
            app_.callbacks.on_focus_lost(app_.callbacks.ctx);
    }
}

static void window_iconify_callback_(GLFWwindow *w, int iconified)
{
    (void)w;
    if (iconified && app_.callbacks.on_minimize)
        app_.callbacks.on_minimize(app_.callbacks.ctx);
}

static void window_maximize_callback_(GLFWwindow *w, int maximized)
{
    (void)w;
    if (maximized && app_.callbacks.on_maximize)
        app_.callbacks.on_maximize(app_.callbacks.ctx);
}

static void set_viewport_(void)
{
    int fw, fh;
    glfwGetFramebufferSize(app_.window, &fw, &fh);
    glViewport(0, 0, fw, fh);
}

static void window_refresh_callback_(GLFWwindow *w)
{
    (void)w;
    if (!app_.callbacks.on_render) return;
    set_viewport_();
    app_.callbacks.on_render(app_.callbacks.ctx);
    glfwSwapBuffers(app_.window);
}

/* ---- public API ---- */

void rc_app_init(const rc_app_desc *desc)
{
    RC_PANIC(glfwInit());

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_RESIZABLE, desc->resizable ? GLFW_TRUE : GLFW_FALSE);

    if (desc->srgb)
        glfwWindowHint(GLFW_SRGB_CAPABLE, GLFW_TRUE);
    if (desc->depth_bits > 0)
        glfwWindowHint(GLFW_DEPTH_BITS, desc->depth_bits);
    if (desc->msaa_samples > 1)
        glfwWindowHint(GLFW_SAMPLES, desc->msaa_samples);

    char title_buf[256];
    const char *title_cstr = rc_str_as_cstr(desc->title, title_buf, (uint32_t)sizeof(title_buf));

    app_.window = glfwCreateWindow(desc->size.x, desc->size.y,
                                   title_cstr ? title_cstr : "", NULL, NULL);
    RC_PANIC(app_.window != NULL);

    glfwMakeContextCurrent(app_.window);
    glfwSwapInterval(1);

    RC_PANIC(gladLoadGL(glad_get_proc_));

    if (desc->srgb)
        glEnable(GL_FRAMEBUFFER_SRGB);

    app_.callbacks        = desc->callbacks;
    app_.current_mods     = (rc_mod)0;
    app_.last_update_time = glfwGetTime();

    glfwSetKeyCallback             (app_.window, key_callback_);
    glfwSetCharCallback            (app_.window, char_callback_);
    glfwSetMouseButtonCallback     (app_.window, mouse_button_callback_);
    glfwSetCursorEnterCallback     (app_.window, cursor_enter_callback_);
    glfwSetCursorPosCallback       (app_.window, cursor_pos_callback_);
    glfwSetScrollCallback          (app_.window, scroll_callback_);
    glfwSetFramebufferSizeCallback (app_.window, framebuffer_size_callback_);
    glfwSetWindowFocusCallback     (app_.window, window_focus_callback_);
    glfwSetWindowIconifyCallback   (app_.window, window_iconify_callback_);
    glfwSetWindowMaximizeCallback  (app_.window, window_maximize_callback_);
    glfwSetWindowRefreshCallback   (app_.window, window_refresh_callback_);
}

void rc_app_destroy(void)
{
    glfwDestroyWindow(app_.window);
    glfwTerminate();
    app_.window = NULL;
}

void rc_app_poll(void)
{
    glfwPollEvents();
}

bool rc_app_is_running(void)
{
    return !glfwWindowShouldClose(app_.window);
}

rc_vec2i rc_app_size(void)
{
    int fw, fh;
    glfwGetFramebufferSize(app_.window, &fw, &fh);
    return rc_vec2i_make(fw, fh);
}

void rc_app_request_update(void)
{
    if (!app_.callbacks.on_update) return;
    double now = glfwGetTime();
    double dt  = now - app_.last_update_time;
    app_.last_update_time = now;
    app_.callbacks.on_update(app_.callbacks.ctx, dt);
}

void rc_app_request_render(void)
{
    if (!app_.callbacks.on_render) return;
    set_viewport_();
    app_.callbacks.on_render(app_.callbacks.ctx);
    glfwSwapBuffers(app_.window);
}

void rc_app_swap_buffers(void)
{
    glfwSwapBuffers(app_.window);
}

double rc_app_time(void)
{
    return glfwGetTime();
}
