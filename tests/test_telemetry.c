#include "../celeste.h"
#include "../tilemap.h"
#include <assert.h>
#include <stdarg.h>
#include <stdlib.h>

static unsigned buttons;

static int callback(CELESTE_P8_CALLBACK_TYPE type, ...) {
    va_list args;
    va_start(args, type);
    int value = 0;
    if (type == CELESTE_P8_BTN) {
        value = (buttons & (1u << va_arg(args, int))) != 0;
    } else if (type == CELESTE_P8_MGET) {
        int x = va_arg(args, int), y = va_arg(args, int);
        if (x >= 0 && x < 128 && y >= 0 && y < 64) value = tilemap_data[x + y * 128];
    } else if (type == CELESTE_P8_FGET) {
        int tile = va_arg(args, int), flag = va_arg(args, int);
        if (tile >= 0 && tile < (int)sizeof(tile_flags) && flag >= 0 && flag < 8)
            value = (tile_flags[tile] & (1 << flag)) != 0;
    }
    va_end(args);
    return value;
}

int main(void) {
    Celeste_P8_Telemetry t = {0};
    Celeste_P8_set_call_func(callback);
    Celeste_P8_set_rndseed(8);
    Celeste_P8_init();
    Celeste_P8_get_telemetry(&t);
    assert(t.is_title && t.jumps == 0 && t.dashes == 0);

    Celeste_P8__DEBUG();
    for (int i = 0; i < 100; i++) Celeste_P8_update();
    Celeste_P8_get_telemetry(&t);
    assert(!t.is_title && t.room == 0);

    buttons = 1u << 4;
    Celeste_P8_update();
    buttons = 0;
    for (int i = 0; i < 15; i++) Celeste_P8_update();
    buttons = 1u << 5;
    Celeste_P8_update();
    buttons = 0;
    Celeste_P8_get_telemetry(&t);
    assert(t.jumps == 1 && t.dashes == 1 && t.climb_pixels > 0);
    Celeste_P8_Telemetry saved = t;

    Celeste_P8_update();
    buttons = 1u << 5;
    Celeste_P8_update();
    buttons = 0;
    Celeste_P8_get_telemetry(&t);
    assert(t.dashes == saved.dashes); // midair dash cannot repeat without a refill
    saved = t;

    void *state = malloc(Celeste_P8_get_state_size());
    assert(state);
    Celeste_P8_save_state(state);
    Celeste_P8_init();
    Celeste_P8_get_telemetry(&t);
    assert(t.is_title && t.jumps == 0 && t.dashes == 0);
    Celeste_P8_load_state(state);
    Celeste_P8_get_telemetry(&t);
    assert(!t.is_title && t.jumps == saved.jumps && t.dashes == saved.dashes);
    assert(t.climb_pixels == saved.climb_pixels);
    free(state);
    return 0;
}
