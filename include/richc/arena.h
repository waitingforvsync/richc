#ifndef RICHC_ARENA_H_
#define RICHC_ARENA_H_

#include "richc/defines.h"


typedef struct arena_t {
    void *base;
    int32_t offset;
    int32_t size;
} arena_t;


arena_t arena_make(void);
arena_t arena_make_with_size(int32_t initial_size);
void arena_init(arena_t *arena, int32_t initial_size);
void arena_deinit(arena_t *arena);
void *arena_alloc(arena_t *arena, int32_t size);
void *arena_calloc(arena_t *arena, int32_t size);
void *arena_realloc(arena_t *arena, void *old_ptr, int32_t old_size, int32_t new_size);
void arena_free(arena_t *arena, void *ptr, int32_t size);
void arena_reset(arena_t *arena);

#define arena_new(type, arena) ((type *)arena_alloc(arena, ssizeof(type)))
#define arena_new_n(type, n, arena) ((type *)arena_alloc(arena, ssizeof(type) * (n)))
#define arena_new_slice(type, n, arena) {.data = arena_new_n(type, n, arena), .num = n}
#define arena_new_zeroed(type, arena) ((type *)arena_calloc(arena, ssizeof(type)))
#define arena_new_zeroed_n(type, n, arena) ((type *)arena_calloc(arena, ssizeof(type) * (n)))
#define arena_destroy(arena, ptr) arena_free(arena, ptr, ssizeof(*ptr))
#define arena_destroy_n(arena, ptr, num) arena_free(arena, ptr, ssizeof(*ptr) * (num))


#endif // ifndef RICHC_ARENA_H_
