#include "richc/app/app.h"


int main(void) {
    app_init(&(app_desc_t) {
        .width = 1024,
        .height = 800,
        .title = "Sample window",
        .resizable = true
    });
    app_run();
    app_deinit();
    return 0;
}
