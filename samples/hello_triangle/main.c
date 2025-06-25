#include "richc/app/app.h"

typedef struct app_state_t {
    float angle;
} app_state_t;


void on_paint(void *context) {
    app_state_t *state = (app_state_t *)context;
    state->angle += 0.1f;
}


int main(void) {
    app_state_t state = {
        .angle = 0.0f
    };

    app_init(&(app_desc_t) {
        .width = 1024,
        .height = 800,
        .title = "Sample window",
        .resizable = true,
        .srgb = true,
        .context = &state,
        .callbacks = {
            .on_paint = on_paint
        }
    });

    app_run();
    app_deinit();
    return 0;
}
