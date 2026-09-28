#ifndef CELESTE_DESKTOP_STATS_H
#define CELESTE_DESKTOP_STATS_H

#include <stddef.h>
#include <stdint.h>

#define HOST_SAVE_V2_MAGIC "CCLSTSV2"
#define HOST_SAVE_V3_MAGIC "CCLSTSV3"
#define HOST_SAVE_V2_VERSION 2u
#define HOST_SAVE_V3_VERSION 3u
#define HOST_FRUIT_MASK 0x3fffffffu

typedef struct {
	uint64_t jumps;
	uint64_t dashes;
	uint64_t climb_pixels;
	uint64_t deaths;
	uint64_t completions;
	uint64_t ingame_millis;
} HostTotalsV2;

typedef struct {
	uint64_t jumps;
	uint64_t dashes;
	uint64_t climb_pixels;
	uint64_t deaths;
	uint64_t completions;
	uint64_t ingame_millis;
	uint64_t strawberries;
	uint64_t stages;
} HostTotals;

/* Frozen v1.0.1 layout. Changing this breaks V2 progress migration. */
typedef struct {
	char magic[8];
	uint32_t version;
	uint32_t state_size;
	uint32_t checksum;
	int32_t music_index;
	uint64_t run_millis;
	uint32_t run_finished;
	uint32_t reserved;
	HostTotalsV2 run_stats;
	HostTotalsV2 totals;
} HostSaveHeaderV2;

typedef struct {
	char magic[8];
	uint32_t version;
	uint32_t state_size;
	uint32_t checksum;
	int32_t music_index;
	uint64_t run_millis;
	uint32_t run_finished;
	uint32_t reserved;
	HostTotals run_stats;
	HostTotals totals;
} HostSaveHeaderV3;

static uint32_t HostChecksum(const void* data, size_t size, uint32_t hash) {
	const unsigned char* bytes = data;
	for (size_t i = 0; i < size; i++) {
		hash ^= bytes[i];
		hash *= 16777619u;
	}
	return hash;
}

static unsigned HostPopcountFruit(uint32_t fruit_mask) {
	fruit_mask &= HOST_FRUIT_MASK;
	unsigned count = 0;
	while (fruit_mask) {
		fruit_mask &= fruit_mask - 1u;
		count++;
	}
	return count;
}

static unsigned HostCountNewStrawberries(uint32_t previous, uint32_t current) {
	return HostPopcountFruit(current & ~previous);
}

static unsigned HostCompletedStage(int previous_room, int current_room) {
	return previous_room >= 0 && previous_room < 30 && current_room == previous_room + 1;
}

static unsigned HostSeedStages(int room) {
	return room >= 0 && room <= 30 ? (unsigned)room : 0u;
}

static void HostCopyV2Totals(HostTotals* destination, const HostTotalsV2* source) {
	destination->jumps = source->jumps;
	destination->dashes = source->dashes;
	destination->climb_pixels = source->climb_pixels;
	destination->deaths = source->deaths;
	destination->completions = source->completions;
	destination->ingame_millis = source->ingame_millis;
	destination->strawberries = 0;
	destination->stages = 0;
}

static void HostMigrateV2Stats(
	HostTotals* run_stats,
	HostTotals* totals,
	const HostTotalsV2* old_run_stats,
	const HostTotalsV2* old_totals,
	uint32_t fruit_mask,
	int room
) {
	HostCopyV2Totals(run_stats, old_run_stats);
	HostCopyV2Totals(totals, old_totals);
	run_stats->strawberries = HostPopcountFruit(fruit_mask);
	run_stats->stages = HostSeedStages(room);
	totals->strawberries = run_stats->strawberries;
	totals->stages = run_stats->stages;
}

static void HostMigrateV2Header(
	HostSaveHeaderV3* destination,
	const HostSaveHeaderV2* source,
	uint32_t fruit_mask,
	int room
) {
	for (size_t i = 0; i < sizeof *destination; i++)
		((unsigned char*)destination)[i] = 0;
	for (size_t i = 0; i < 8; i++)
		destination->magic[i] = HOST_SAVE_V3_MAGIC[i];
	destination->version = HOST_SAVE_V3_VERSION;
	destination->state_size = source->state_size;
	destination->music_index = source->music_index;
	destination->run_millis = source->run_millis;
	destination->run_finished = source->run_finished;
	HostMigrateV2Stats(
		&destination->run_stats, &destination->totals,
		&source->run_stats, &source->totals,
		fruit_mask, room);
}

#endif
