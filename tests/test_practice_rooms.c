#include "../celeste.h"
#include "../tilemap.h"

#include <assert.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

static unsigned buttons;

static int callback(CELESTE_P8_CALLBACK_TYPE type, ...) {
	va_list args;
	va_start(args, type);
	int value = 0;
	if (type == CELESTE_P8_BTN) {
		value = (buttons & (1u << va_arg(args, int))) != 0;
	} else if (type == CELESTE_P8_MGET) {
		int x = va_arg(args, int);
		int y = va_arg(args, int);
		if (x >= 0 && x < 128 && y >= 0 && y < 64)
			value = tilemap_data[x + y * 128];
	} else if (type == CELESTE_P8_FGET) {
		int tile = va_arg(args, int);
		int flag = va_arg(args, int);
		if (tile >= 0 && tile < (int)sizeof tile_flags && flag >= 0 && flag < 8)
			value = (tile_flags[tile] & (1 << flag)) != 0;
	}
	va_end(args);
	return value;
}

static void assert_room(int room, int is_title) {
	Celeste_P8_Telemetry telemetry = {0};
	Celeste_P8_get_telemetry(&telemetry);
	assert(telemetry.room == room);
	assert(telemetry.is_title == is_title);
}

static size_t max_djump_offset(void) {
	return 2 * sizeof(unsigned) + 2 * sizeof(int) + 2 * sizeof(int)
		+ sizeof(Celeste_P8_bool_t) + sizeof(int)
		+ 30 * sizeof(Celeste_P8_bool_t) + sizeof(Celeste_P8_bool_t)
		+ sizeof(int) + 3 * sizeof(Celeste_P8_bool_t) + sizeof(int)
		+ sizeof(Celeste_P8_bool_t) + 2 * sizeof(int) + sizeof(short)
		+ sizeof(int);
}

static int saved_max_djump(const void* state, size_t state_size) {
	size_t offset = max_djump_offset();
	assert(offset <= state_size && state_size - offset >= sizeof(int));
	int value;
	memcpy(&value, (const char*)state + offset, sizeof value);
	return value;
}

static int set_practice_double_dash(void* state, size_t state_size, int room) {
	if (room <= 21) return 1;
	size_t offset = max_djump_offset();
	if (offset > state_size || state_size - offset < sizeof(int)) return 0;
	int value = saved_max_djump(state, state_size);
	if (value != 1 && value != 2) return 0;
	value = 2;
	memcpy((char*)state + offset, &value, sizeof value);
	return 1;
}

static void enter_practice_room(const void* pre_practice_state, int room) {
	Celeste_P8_load_state(pre_practice_state);
	assert_room(31, 1);

	Celeste_P8__DEBUG();
	for (int frame = 0; frame < 30; frame++) Celeste_P8_update();
	assert_room(31, 1);
	Celeste_P8_update();
	assert_room(0, 0);

	for (int current = 0; current < room; current++) Celeste_P8__DEBUG();
	assert_room(room, 0);
}

int main(void) {
	Celeste_P8_set_call_func(callback);
	Celeste_P8_set_rndseed(8);
	Celeste_P8_init();
	assert_room(31, 1);

	size_t state_size = Celeste_P8_get_state_size();
	void* pre_practice_state = malloc(state_size);
	void* room_state = malloc(state_size);
	void* restored_state = malloc(state_size);
	assert(pre_practice_state && room_state && restored_state);
	Celeste_P8_save_state(pre_practice_state);

	for (int room = 0; room <= 30; room++)
		enter_practice_room(pre_practice_state, room);

	/* Debug room skips bypass room 21's orb, so post-unlock practice seeds it. */
	enter_practice_room(pre_practice_state, 21);
	Celeste_P8_save_state(room_state);
	assert(set_practice_double_dash(room_state, state_size, 21));
	assert(saved_max_djump(room_state, state_size) == 1);
	enter_practice_room(pre_practice_state, 22);
	Celeste_P8_save_state(room_state);
	assert(saved_max_djump(room_state, state_size) == 1);
	assert(set_practice_double_dash(room_state, state_size, 22));
	assert(saved_max_djump(room_state, state_size) == 2);
	Celeste_P8_load_state(room_state);
	Celeste_P8_save_state(restored_state);
	assert(saved_max_djump(restored_state, state_size) == 2);
	Celeste_P8_Telemetry before_jump = {0};
	Celeste_P8_get_telemetry(&before_jump);
	Celeste_P8_Telemetry after_jump = before_jump;
	for (int frame = 0; frame < 240 && after_jump.jumps == before_jump.jumps; frame++) {
		buttons = (frame & 1) ? 0 : 1u << 4;
		Celeste_P8_update();
		Celeste_P8_get_telemetry(&after_jump);
	}
	buttons = 0;
	assert(after_jump.jumps == before_jump.jumps + 1);
	Celeste_P8_Telemetry before_dashes = after_jump;
	buttons = (1u << 2) | (1u << 5);
	Celeste_P8_update();
	buttons = 0;
	for (int frame = 0; frame < 6; frame++) Celeste_P8_update();
	buttons = (1u << 2) | (1u << 5);
	Celeste_P8_update();
	buttons = 0;
	Celeste_P8_update();
	Celeste_P8_Telemetry after_dashes = {0};
	Celeste_P8_get_telemetry(&after_dashes);
	assert(after_dashes.dashes == before_dashes.dashes + 2);

	enter_practice_room(pre_practice_state, 17);
	Celeste_P8_save_state(room_state);
	Celeste_P8__DEBUG();
	for (int frame = 0; frame < 12; frame++) Celeste_P8_update();
	assert_room(18, 0);
	Celeste_P8_load_state(room_state);
	assert_room(17, 0);
	Celeste_P8_save_state(restored_state);
	assert(memcmp(room_state, restored_state, state_size) == 0);

	Celeste_P8_load_state(pre_practice_state);
	assert_room(31, 1);
	Celeste_P8_save_state(restored_state);
	assert(memcmp(pre_practice_state, restored_state, state_size) == 0);

	free(restored_state);
	free(room_state);
	free(pre_practice_state);
	return 0;
}
