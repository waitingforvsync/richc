#ifndef RICHC_APP_APP_H_
#define RICHC_APP_APP_H_

#include "richc/defines.h"


typedef struct app_callbacks_t {
    void (*on_update)(void *app_context, float delta_time);
    void (*on_init_render)(void *app_context, int32_t width, int32_t height);
    void (*on_render)(void *app_context, int32_t width, int32_t height);
    void (*on_resize)(void *app_context, int32_t width, int32_t height);
    void (*on_key_down)(void *app_context, int32_t keycode);
    void (*on_key_up)(void *app_context, int32_t keycode);
    void (*on_key_type)(void *app_context, int32_t charcode);
    void (*on_mouse_move)(void *app_context, int32_t x, int32_t y);
    void (*on_mouse_button_down)(void *app_context, int32_t);
    void (*on_mouse_button_up)(void *app_context, int32_t);
} app_callbacks_t;


typedef struct app_desc_t {
    int32_t width;
    int32_t height;
    const char *title;
    bool resizable;
    bool srgb;
    void *context;
    app_callbacks_t callbacks;
} app_desc_t;


void app_run(const app_desc_t *desc);



#endif // ifndef RICHC_APP_APP_H_
