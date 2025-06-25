#include "gfx/opengl/opengl.h"

glfns_t gl = {
    // Fill out any base OpenGL functions directly
    // The rest will be queried and loaded at runtime.
    .Clear = glClear,
    .ClearColor = glClearColor,
    .Disable = glDisable,
    .Enable = glEnable,
    .Viewport = glViewport
};
