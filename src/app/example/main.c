#include <glad/gl.h>

#include "richc/app/app.h"

static void render(void *ctx)
{
    (void)ctx;
    // perceptual mid-grey clear, straight to GL for now (a gfx abstraction comes
    // later). 0.216 linear encodes to ~0.5 sRGB given the srgb framebuffer.
    glClearColor(0.216f, 0.216f, 0.216f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
}

int main(void)
{
    rc_app_init(&(rc_app_desc) {
        .title     = RC_STR("richc-app test"),
        .size      = {1280, 720},
        .resizable = true,
        .srgb      = true,
        .callbacks = {
            .on_render = render,
        },
    });

    while (rc_app_is_running()) {
        rc_app_poll();
        rc_app_request_render();
    }

    rc_app_destroy();
    return 0;
}
