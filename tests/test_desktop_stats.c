#include "../desktop_stats.h"
#include <assert.h>
#include <string.h>

static void test_legacy_layout_and_checksum(void) {
	assert(sizeof(HostTotalsV2) == 48);
	assert(sizeof(HostSaveHeaderV2) == 136);
	assert(sizeof(HostSaveHeaderV3) == 168);

	HostSaveHeaderV2 header;
	memset(&header, 0, sizeof header);
	memcpy(header.magic, HOST_SAVE_V2_MAGIC, 8);
	header.version = HOST_SAVE_V2_VERSION;
	header.state_size = 5;
	header.music_index = 3;
	header.run_millis = 123456;
	header.run_finished = 1;
	header.run_stats.jumps = 11;
	header.run_stats.dashes = 12;
	header.totals.jumps = 101;
	header.totals.ingame_millis = 900000;
	const unsigned char state[] = {1, 3, 5, 7, 9};
	uint32_t checksum = HostChecksum(&header, sizeof header, 2166136261u);
	checksum = HostChecksum(state, sizeof state, checksum);
	assert(checksum == 3551210174u);
}

static void test_v2_stat_migration(void) {
	HostSaveHeaderV2 old_header;
	memset(&old_header, 0, sizeof old_header);
	memcpy(old_header.magic, HOST_SAVE_V2_MAGIC, 8);
	old_header.version = HOST_SAVE_V2_VERSION;
	old_header.state_size = 5;
	old_header.music_index = 3;
	old_header.run_millis = 123456;
	old_header.run_finished = 1;
	old_header.run_stats = (HostTotalsV2){11, 12, 13, 14, 15, 16000};
	old_header.totals = (HostTotalsV2){101, 102, 103, 104, 105, 106000};
	HostSaveHeaderV3 migrated;
	HostMigrateV2Header(&migrated, &old_header,
		(1u << 0) | (1u << 5) | (1u << 29), 17);
	assert(memcmp(migrated.magic, HOST_SAVE_V3_MAGIC, 8) == 0);
	assert(migrated.version == HOST_SAVE_V3_VERSION && migrated.state_size == 5);
	assert(migrated.music_index == 3 && migrated.run_millis == 123456);
	assert(migrated.run_finished == 1 && migrated.checksum == 0);
	assert(migrated.run_stats.jumps == 11 && migrated.run_stats.dashes == 12);
	assert(migrated.run_stats.climb_pixels == 13 && migrated.run_stats.deaths == 14);
	assert(migrated.run_stats.completions == 15 && migrated.run_stats.ingame_millis == 16000);
	assert(migrated.totals.jumps == 101 && migrated.totals.dashes == 102);
	assert(migrated.totals.climb_pixels == 103 && migrated.totals.deaths == 104);
	assert(migrated.totals.completions == 105 && migrated.totals.ingame_millis == 106000);
	assert(migrated.run_stats.strawberries == 3 && migrated.totals.strawberries == 3);
	assert(migrated.run_stats.stages == 17 && migrated.totals.stages == 17);
	const unsigned char state[] = {1, 3, 5, 7, 9};
	uint32_t checksum = HostChecksum(&migrated, sizeof migrated, 2166136261u);
	checksum = HostChecksum(state, sizeof state, checksum);
	assert(checksum == 3855453515u);

	HostMigrateV2Header(&migrated, &old_header, 0, 31);
	assert(migrated.run_stats.strawberries == 0 && migrated.run_stats.stages == 0);
}

static void test_stat_events(void) {
	assert(HostCountNewStrawberries(0, (1u << 2) | (1u << 29)) == 2);
	assert(HostCountNewStrawberries(1u << 2, (1u << 2) | (1u << 4)) == 1);
	assert(HostCountNewStrawberries((1u << 2) | (1u << 4), 1u << 2) == 0);
	assert(HostPopcountFruit(UINT32_MAX) == 30);

	assert(HostCompletedStage(0, 1));
	assert(HostCompletedStage(29, 30));
	assert(!HostCompletedStage(30, 31));
	assert(!HostCompletedStage(31, 0));
	assert(!HostCompletedStage(4, 6));
}

int main(void) {
	test_legacy_layout_and_checksum();
	test_v2_stat_migration();
	test_stat_events();
	return 0;
}
