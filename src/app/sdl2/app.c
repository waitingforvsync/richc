#include "richc/app/app.h"
#include <SDL2/SDL.h>
#include <GL/glew.h>


typedef struct thread_context_t {
    SDL_Window *window;
    SDL_sem *frame_ready;
    SDL_mutex *window_size_mutex;
    SDL_atomic_t running;
    int32_t window_width;
    int32_t window_height;
    void *app_context;
    app_callbacks_t app_callbacks;
} thread_context_t;


static int render_thread(void *context) {
    thread_context_t *thread_context = context;
    SDL_GLContext gl_context = SDL_GL_CreateContext(thread_context->window);
    if (!gl_context) {
        return 1;
    }
    glewExperimental = GL_TRUE;
    glewInit();

    SDL_GL_MakeCurrent(thread_context->window, gl_context);

    if (thread_context->app_callbacks.on_init_render) {
        SDL_LockMutex(thread_context->window_size_mutex);
        thread_context->app_callbacks.on_init_render(
            thread_context->app_context,
            thread_context->window_width,
            thread_context->window_height
        );
        SDL_UnlockMutex(thread_context->window_size_mutex);
    }

    while (SDL_AtomicGet(&thread_context->running)) {
        SDL_SemWait(thread_context->frame_ready);
        if (thread_context->app_callbacks.on_render) {
            SDL_LockMutex(thread_context->window_size_mutex);
            thread_context->app_callbacks.on_render(
                thread_context->app_context,
                thread_context->window_width,
                thread_context->window_height
            );
            SDL_UnlockMutex(thread_context->window_size_mutex);
        }
        SDL_GL_SwapWindow(thread_context->window);
    }

    SDL_GL_DeleteContext(gl_context);
    return 0;
}


void app_run(const app_desc_t *desc) {
    SDL_SetHint(SDL_HINT_VIDEODRIVER, "wayland,x11");
    SDL_Init(SDL_INIT_VIDEO);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);

    thread_context_t thread_context = {
        .window = SDL_CreateWindow(
            desc->title,
            SDL_WINDOWPOS_CENTERED,
            SDL_WINDOWPOS_CENTERED,
            desc->width,
            desc->height,
            (desc->resizable ? SDL_WINDOW_RESIZABLE : 0) | SDL_WINDOW_OPENGL
        ),
        .frame_ready = SDL_CreateSemaphore(0),
        .window_size_mutex = SDL_CreateMutex(),
        .running = {1},
        .window_width = desc->width,
        .window_height = desc->height,
        .app_context = desc->context,
        .app_callbacks = desc->callbacks
    };

    SDL_Thread *thread = SDL_CreateThread(render_thread, "RenderThread", &thread_context);

    int64_t time = SDL_GetTicks64();
    while (SDL_AtomicGet(&thread_context.running)) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
                case SDL_QUIT:
                    SDL_AtomicSet(&thread_context.running, 0);
                    break;
                
                case SDL_WINDOWEVENT:
                    switch (event.window.event) {
                        case SDL_WINDOWEVENT_RESIZED:
                            SDL_LockMutex(thread_context.window_size_mutex);
                            thread_context.window_width = event.window.data1;
                            thread_context.window_height = event.window.data2;
                            SDL_UnlockMutex(thread_context.window_size_mutex);
                            if (desc->callbacks.on_resize) {
                                desc->callbacks.on_resize(desc->context, event.window.data1, event.window.data2);
                            }
                    }
            }
        }


        if (desc->callbacks.on_update) {
            desc->callbacks.on_update(desc->context, (SDL_GetTicks64() - time) / 1000.0f);
        }
        time = SDL_GetTicks64();
        SDL_SemPost(thread_context.frame_ready);
    }

    SDL_SemPost(thread_context.frame_ready);
    SDL_WaitThread(thread, 0);
    SDL_DestroyMutex(thread_context.window_size_mutex);
    SDL_DestroySemaphore(thread_context.frame_ready);
    SDL_DestroyWindow(thread_context.window);
    SDL_Quit();
}

