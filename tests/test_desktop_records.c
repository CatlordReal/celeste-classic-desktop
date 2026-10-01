#include "../desktop_records.h"

#include <assert.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

static void close_enough(double actual, double expected) {
	assert(fabs(actual - expected) < 0.000001);
}

int main(void) {
	char url[256];
	char tiny[8];

#ifdef _WIN32
	_putenv_s("CCLESTE_RECORD_DIR", "");
#else
	unsetenv("CCLESTE_RECORD_DIR");
#endif
	DesktopRecordInit();
#ifdef _WIN32
	assert(strcmp(DesktopRecordLocalPath(DESKTOP_RECORD_ANY_PERCENT),
		"records\\any.mp4") == 0);
	assert(strcmp(DesktopRecordLocalPath(DESKTOP_RECORD_ALL_STRAWBERRIES),
		"records\\all-berries.mp4") == 0);
#else
	assert(strcmp(DesktopRecordLocalPath(DESKTOP_RECORD_ANY_PERCENT),
		"records/any.mp4") == 0);
	assert(strcmp(DesktopRecordLocalPath(DESKTOP_RECORD_ALL_STRAWBERRIES),
		"records/all-berries.mp4") == 0);
#endif

	for (int category = 0; category < 2; category++) {
		for (int room = 0; room < DESKTOP_RECORD_ROOM_COUNT; room++) {
			double start = DesktopRecordRoomStart(category, room);
			double end = DesktopRecordRoomEnd(category, room);
			assert(start >= 0.0);
			assert(end > start);
			if (room + 1 < DESKTOP_RECORD_ROOM_COUNT)
				close_enough(end, DesktopRecordRoomStart(category, room + 1));
		}
	}

	assert(strcmp(DesktopRecordRunner(DESKTOP_RECORD_ANY_PERCENT),
		"Associatedfeetballer") == 0);
	assert(strcmp(DesktopRecordRunner(DESKTOP_RECORD_ALL_STRAWBERRIES),
		"LordSNEK") == 0);
	assert(strcmp(DesktopRecordTimeLabel(DESKTOP_RECORD_ANY_PERCENT), "1:37.60") == 0);
	assert(strcmp(DesktopRecordTimeLabel(DESKTOP_RECORD_ALL_STRAWBERRIES), "2:00.77") == 0);
	assert(strcmp(DesktopRecordURL(DESKTOP_RECORD_ANY_PERCENT),
		"https://youtube.com/watch?v=7yhApvCUDF8") == 0);
	assert(strcmp(DesktopRecordURL(DESKTOP_RECORD_ALL_STRAWBERRIES),
		"https://youtube.com/watch?v=gbbnHZSVDuM") == 0);

	close_enough(DesktopRecordRoomStart(DESKTOP_RECORD_ANY_PERCENT, 0), 2.300000);
	close_enough(DesktopRecordRoomStart(DESKTOP_RECORD_ANY_PERCENT, 21), 66.133333);
	close_enough(DesktopRecordRoomStart(DESKTOP_RECORD_ANY_PERCENT, 22), 71.166667);
	close_enough(DesktopRecordRoomStart(DESKTOP_RECORD_ANY_PERCENT, 30), 99.933333);
	close_enough(DesktopRecordRoomEnd(DESKTOP_RECORD_ANY_PERCENT, 21), 71.166667);
	close_enough(DesktopRecordRoomEnd(DESKTOP_RECORD_ANY_PERCENT, 30), 105.882993);
	close_enough(DesktopRecordRoomStart(DESKTOP_RECORD_ALL_STRAWBERRIES, 0), 6.473133);
	close_enough(DesktopRecordRoomStart(DESKTOP_RECORD_ALL_STRAWBERRIES, 30), 127.427422);
	close_enough(DesktopRecordRoomEnd(DESKTOP_RECORD_ALL_STRAWBERRIES, 30), 135.790295);

	assert(DesktopRecordBuildRoomURL(DESKTOP_RECORD_ANY_PERCENT, 22, url, sizeof url));
	assert(strcmp(url, "https://youtube.com/watch?v=7yhApvCUDF8&t=71.167s") == 0);
	assert(DesktopRecordBuildRoomURL(DESKTOP_RECORD_ALL_STRAWBERRIES, 0, url, sizeof url));
	assert(strcmp(url, "https://youtube.com/watch?v=gbbnHZSVDuM&t=6.473s") == 0);
	assert(!DesktopRecordBuildRoomURL(2, 0, url, sizeof url));
	assert(!DesktopRecordBuildRoomURL(0, -1, url, sizeof url));
	assert(!DesktopRecordBuildRoomURL(0, DESKTOP_RECORD_ROOM_COUNT, url, sizeof url));
	assert(!DesktopRecordBuildRoomURL(0, 0, tiny, sizeof tiny));
	assert(DesktopRecordURL(2) == NULL);
	assert(DesktopRecordRoomStart(2, 0) < 0.0);

#ifdef _WIN32
	_putenv_s("CCLESTE_RECORD_DIR", "C:\\celeste-recordings");
#else
	setenv("CCLESTE_RECORD_DIR", "/private/tmp/celeste-recordings", 1);
#endif
	DesktopRecordInit();
#ifdef _WIN32
	assert(strcmp(DesktopRecordLocalPath(DESKTOP_RECORD_ANY_PERCENT),
		"C:\\celeste-recordings\\any.mp4") == 0);
	assert(strcmp(DesktopRecordLocalPath(DESKTOP_RECORD_ALL_STRAWBERRIES),
		"C:\\celeste-recordings\\all-berries.mp4") == 0);
#else
	assert(strcmp(DesktopRecordLocalPath(DESKTOP_RECORD_ANY_PERCENT),
		"/private/tmp/celeste-recordings/any.mp4") == 0);
	assert(strcmp(DesktopRecordLocalPath(DESKTOP_RECORD_ALL_STRAWBERRIES),
		"/private/tmp/celeste-recordings/all-berries.mp4") == 0);
#endif
	return 0;
}
