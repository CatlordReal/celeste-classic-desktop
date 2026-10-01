#include "../desktop_stats.h"
#include "../tilemap.h"
#include <assert.h>
#include <string.h>

static void test_legacy_layout_and_checksum(void) {
	assert(sizeof(HostTotalsV2) == 48);
	assert(sizeof(HostSaveHeaderV2) == 136);
	assert(sizeof(HostSaveHeaderV3) == 168);
	assert(sizeof(HostSaveHeaderV4) == 184);
	assert(sizeof(HostSaveHeaderV5) == 704);

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

static void test_v4_split_migration(void) {
	HostSaveHeaderV4 old_header;
	memset(&old_header, 0, sizeof old_header);
	memcpy(old_header.magic, HOST_SAVE_V4_MAGIC, 8);
	old_header.version = HOST_SAVE_V4_VERSION;
	old_header.state_size = 42;
	old_header.music_index = 2;
	old_header.run_millis = 654321;
	old_header.run_timer_remainder = 7;
	old_header.run_stats = (HostTotals){1, 2, 3, 4, 5, 6, 7, 8};
	old_header.totals = (HostTotals){11, 12, 13, 14, 15, 700000, 17, 18};
	old_header.best_times = (HostBestTimes){123000, 456000};

	HostSaveHeaderV5 migrated;
	HostMigrateV4ToV5Header(&migrated, &old_header);
	assert(memcmp(migrated.magic, HOST_SAVE_V5_MAGIC, 8) == 0);
	assert(migrated.version == HOST_SAVE_V5_VERSION);
	assert(migrated.state_size == 42 && migrated.music_index == 2);
	assert(migrated.run_millis == 654321 && migrated.run_timer_remainder == 7);
	assert(memcmp(&migrated.run_stats, &old_header.run_stats, sizeof migrated.run_stats) == 0);
	assert(memcmp(&migrated.totals, &old_header.totals, sizeof migrated.totals) == 0);
	assert(memcmp(&migrated.best_times, &old_header.best_times, sizeof migrated.best_times) == 0);
	assert(migrated.run_stage_start_millis == 654321);
	assert(migrated.run_stage_split_valid == 0);
	assert(migrated.stage_splits.best_millis[0] == 0);
	assert(migrated.run_stage_millis[29] == 0);
	assert(migrated.copium.millis == 654321);
	assert(migrated.copium.room_start_millis == 654321);
	assert(migrated.copium.remainder == 7);
	assert(migrated.copium.room_start_remainder == 7);
}

static void test_stage_splits(void) {
	HostStageSplits splits = {{0}};
	HostSplitComparison comparison = {.room = -1};
	assert(HostRecordStageSplit(&splits, 0, 5000, &comparison));
	assert(splits.best_millis[0] == 5000);
	assert(comparison.room == 0 && comparison.elapsed_millis == 5000);
	assert(!comparison.has_baseline && comparison.delta_millis == 0);
	assert(!HostRecordStageSplit(&splits, 0, 5100, &comparison));
	assert(comparison.has_baseline && comparison.baseline_millis == 5000);
	assert(comparison.delta_millis == 100 && splits.best_millis[0] == 5000);
	assert(HostRecordStageSplit(&splits, 0, 4900, &comparison));
	assert(comparison.delta_millis == -100 && splits.best_millis[0] == 4900);
	assert(!HostRecordStageSplit(&splits, -1, 1000, &comparison));
	assert(!HostRecordStageSplit(&splits, 30, 1000, &comparison));

	HostSetSplitComparison(&comparison, 4, 0, 1000);
	uint32_t remainder = 0;
	for (int i = 0; i < 30; i++)
		HostAdvanceSplitComparisonFrame(&comparison, &remainder);
	assert(comparison.elapsed_millis == 1000 && comparison.delta_millis == 0);
}

static void test_copium_timer(void) {
	HostCopiumTimer timer;
	HostResetCopiumTimer(&timer);
	for (int i = 0; i < 30; i++) HostAdvanceCopiumTimerFrame(&timer);
	assert(timer.millis == 1000 && timer.remainder == 0);
	HostStartCopiumRoom(&timer);
	for (int i = 0; i < 15; i++) HostAdvanceCopiumTimerFrame(&timer);
	assert(timer.millis == 1500);
	HostDiscardCopiumRoom(&timer);
	assert(timer.millis == 1000 && timer.remainder == 0);
	for (int i = 0; i < 30; i++) HostAdvanceCopiumTimerFrame(&timer);
	HostStartCopiumRoom(&timer);
	assert(timer.millis == 2000 && timer.room_start_millis == 2000);
}

static void test_v3_stat_migration(void) {
	HostSaveHeaderV3 old_header;
	memset(&old_header, 0, sizeof old_header);
	memcpy(old_header.magic, HOST_SAVE_V3_MAGIC, 8);
	old_header.version = HOST_SAVE_V3_VERSION;
	old_header.state_size = 42;
	old_header.music_index = 2;
	old_header.run_millis = 654321;
	old_header.run_finished = 1;
	old_header.run_stats = (HostTotals){1, 2, 3, 4, 5, 6, 7, 8};
	old_header.totals = (HostTotals){11, 12, 13, 14, 15, 16, 17, 18};

	HostSaveHeaderV4 migrated;
	HostMigrateV3ToV4Header(&migrated, &old_header);
	assert(memcmp(migrated.magic, HOST_SAVE_V4_MAGIC, 8) == 0);
	assert(migrated.version == HOST_SAVE_V4_VERSION);
	assert(migrated.state_size == 42 && migrated.music_index == 2);
	assert(migrated.run_millis == 654321 && migrated.run_finished == 1);
	assert(memcmp(&migrated.run_stats, &old_header.run_stats, sizeof migrated.run_stats) == 0);
	assert(memcmp(&migrated.totals, &old_header.totals, sizeof migrated.totals) == 0);
	assert(migrated.best_times.any_percent_millis == 0);
	assert(migrated.best_times.all_strawberries_millis == 0);

	HostSaveHeaderV5 migrated_v5;
	HostMigrateV3ToV5Header(&migrated_v5, &old_header);
	assert(memcmp(migrated_v5.magic, HOST_SAVE_V5_MAGIC, 8) == 0);
	assert(migrated_v5.version == HOST_SAVE_V5_VERSION);
	assert(migrated_v5.run_millis == 654321);
	assert(migrated_v5.run_stage_start_millis == 654321);
	assert(migrated_v5.run_stage_split_valid == 0);
}

static void test_best_times(void) {
	HostBestTimes best = {0};
	uint32_t map_mask = 0;
	for (int room = 0; room < 30; room++) {
		int room_x = room % 8;
		int room_y = room / 8;
		for (int y = 0; y < 16; y++) for (int x = 0; x < 16; x++) {
			unsigned tile = tilemap_data[room_x * 16 + x + (room_y * 16 + y) * 128];
			if (tile == 26 || tile == 28 || tile == 64 || tile == 8 || tile == 20)
				map_mask |= 1u << room;
		}
	}
	assert(map_mask == HOST_ALL_STRAWBERRIES_MASK);
	assert(HostPopcountFruit(HOST_ALL_STRAWBERRIES_MASK) == 18);
	assert(HostHasAllStrawberries(HOST_ALL_STRAWBERRIES_MASK));
	assert(!HostHasAllStrawberries(HOST_ALL_STRAWBERRIES_MASK & ~(1u << 29)));
	assert(HostRecordBestTime(&best, 120000, 1u << 2));
	assert(best.any_percent_millis == 120000 && best.all_strawberries_millis == 0);
	assert(!HostRecordBestTime(&best, 120001, 0));
	assert(HostRecordBestTime(&best, 119999, 0));
	assert(best.any_percent_millis == 119999);
	assert(HostRecordBestTime(&best, 180000, HOST_ALL_STRAWBERRIES_MASK));
	assert(best.all_strawberries_millis == 180000);
	assert(!HostRecordBestTime(&best, 180000, UINT32_MAX));
	assert(HostRecordBestTime(&best, 179999, UINT32_MAX));
	assert(best.all_strawberries_millis == 179999);
}

static void test_official_run_timer(void) {
	uint64_t millis = 0;
	uint32_t remainder = 0;
	HostAdvanceRunTimerFrame(&millis, &remainder);
	assert(millis == 33 && remainder == 10);
	HostAdvanceRunTimerFrame(&millis, &remainder);
	assert(millis == 66 && remainder == 20);
	HostAdvanceRunTimerFrame(&millis, &remainder);
	assert(millis == 100 && remainder == 0);
	for (int i = 3; i < 30; i++) HostAdvanceRunTimerFrame(&millis, &remainder);
	assert(millis == 1000 && remainder == 0);
	assert(HostReachedSummit(29, 30));
	assert(!HostReachedSummit(28, 30));
	assert(!HostReachedSummit(30, 30));
	assert(!HostReachedSummit(30, 29));
	assert(!HostShouldAdvanceRunTimer(1, 0));
	assert(HostShouldAdvanceRunTimer(0, 0));
	assert(HostShouldAdvanceRunTimer(0, 29));
	assert(!HostShouldAdvanceRunTimer(0, 30));
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

	HostSaveHeaderV4 migrated_v4;
	HostMigrateV2ToV4Header(&migrated_v4, &old_header,
		(1u << 0) | (1u << 5) | (1u << 29), 17);
	assert(memcmp(migrated_v4.magic, HOST_SAVE_V4_MAGIC, 8) == 0);
	assert(migrated_v4.version == HOST_SAVE_V4_VERSION);
	assert(migrated_v4.run_stats.jumps == 11 && migrated_v4.totals.jumps == 101);
	assert(migrated_v4.run_stats.strawberries == 3 && migrated_v4.run_stats.stages == 17);
	assert(migrated_v4.best_times.any_percent_millis == 0);
	assert(migrated_v4.best_times.all_strawberries_millis == 0);

	HostSaveHeaderV5 migrated_v5;
	HostMigrateV2ToV5Header(&migrated_v5, &old_header,
		(1u << 0) | (1u << 5) | (1u << 29), 17);
	assert(memcmp(migrated_v5.magic, HOST_SAVE_V5_MAGIC, 8) == 0);
	assert(migrated_v5.version == HOST_SAVE_V5_VERSION);
	assert(migrated_v5.run_stats.strawberries == 3);
	assert(migrated_v5.run_stage_split_valid == 0);
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
	test_v3_stat_migration();
	test_v4_split_migration();
	test_stat_events();
	test_best_times();
	test_official_run_timer();
	test_stage_splits();
	test_copium_timer();
	return 0;
}
