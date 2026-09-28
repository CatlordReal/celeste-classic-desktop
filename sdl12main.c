#include <SDL.h>
#include <SDL_mixer.h>
#if SDL_MAJOR_VERSION >= 2
#include "sdl20compat.inc.c"
#endif
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include <stdint.h>
#include <time.h>
#ifdef _WIN32
#include <io.h>
#include <windows.h>
#else
#include <unistd.h>
#endif
#ifdef _3DS
#include <3ds.h>
#endif
#include "celeste.h"
#include "desktop_backup.h"
#include "desktop_stats.h"

static void ErrLog(char* fmt, ...) {
#ifdef _3DS
	/*FILE* f = fopen("sdmc:/ccleste.txt", "a");
	if (!f) return;
	fprintf(f, "%li \t", (long int)time(NULL));*/
	FILE* f = stdout; //bottom screen console
#else
	FILE* f = stderr;
#endif

	va_list ap;
	va_start(ap, fmt);
	vfprintf(f, fmt, ap);
	va_end(ap);

	if (f != stderr && f != stdout) fclose(f);
}

SDL_Surface* screen = NULL;
SDL_Surface* gfx = NULL;
SDL_Surface* font = NULL;
Mix_Chunk* snd[64] = {NULL};
Mix_Music* mus[6] = {NULL};

#define PICO8_W 128
#define PICO8_H 128
#if !defined(_3DS) && !defined(EMSCRIPTEN)
#define HOST_PANEL_W 96
#else
#define HOST_PANEL_W 0
#endif
#define HOST_W (PICO8_W + HOST_PANEL_W)

#ifdef _3DS
static const int scale = 2;
#else
static int scale = 4;
#endif

static const SDL_Color base_palette[16] = {
	{0x00, 0x00, 0x00},
	{0x1d, 0x2b, 0x53},
	{0x7e, 0x25, 0x53},
	{0x00, 0x87, 0x51},
	{0xab, 0x52, 0x36},
	{0x5f, 0x57, 0x4f},
	{0xc2, 0xc3, 0xc7},
	{0xff, 0xf1, 0xe8},
	{0xff, 0x00, 0x4d},
	{0xff, 0xa3, 0x00},
	{0xff, 0xec, 0x27},
	{0x00, 0xe4, 0x36},
	{0x29, 0xad, 0xff},
	{0x83, 0x76, 0x9c},
	{0xff, 0x77, 0xa8},
	{0xff, 0xcc, 0xaa}
};
static SDL_Color palette[16];

static inline Uint32 getcolor(char idx) {
	SDL_Color c = palette[idx%16];
	return SDL_MapRGB(screen->format, c.r,c.g,c.b);
}

static void ResetPalette(void) {
	//SDL_SetPalette(surf, SDL_PHYSPAL|SDL_LOGPAL, (SDL_Color*)base_palette, 0, 16);
	//memcpy(screen->format->palette->colors, base_palette, 16*sizeof(SDL_Color));
	memcpy(palette, base_palette, sizeof palette);
}

static char* GetDataPath(char* path, int n, const char* fname) {
#ifdef _3DS
	snprintf(path, n, "romfs:/%s", fname);
#else
#ifdef _WIN32
	char pathsep = '\\';
#else
	char pathsep = '/';
#endif //_WIN32
	snprintf(path, n, "data%c%s", pathsep, fname);
#endif //_3DS

	return path;
}

static Uint32 getpixel(SDL_Surface *surface, int x, int y) {
	int bpp = surface->format->BytesPerPixel;
	/* Here p is the address to the pixel we want to retrieve */
	Uint8 *p = (Uint8 *)surface->pixels + y * surface->pitch + x * bpp;

	switch(bpp) {
		case 1:
			return *p;

		case 2:
			return *(Uint16 *)p;

		case 3:
			if(SDL_BYTEORDER == SDL_BIG_ENDIAN)
				return p[0] << 16 | p[1] << 8 | p[2];
			else
				return p[0] | p[1] << 8 | p[2] << 16;

		case 4:
			return *(Uint32 *)p;
	}
	return 0;
}

static void loadbmpscale(char* filename, SDL_Surface** s) {
	SDL_Surface* surf = *s;
	if (surf) SDL_FreeSurface(surf), surf = *s = NULL;

	char tmpath[4096];
	SDL_Surface* bmp = SDL_LoadBMP(GetDataPath(tmpath, sizeof tmpath, filename));
	if (!bmp) {
		ErrLog("error loading bmp '%s': %s\n", filename, SDL_GetError());
		return;
	}

	int w = bmp->w, h = bmp->h;

	surf = SDL_CreateRGBSurface(SDL_SWSURFACE, w*scale, h*scale, 8, 0,0,0,0);
	assert(surf != NULL);
	unsigned char* data = surf->pixels;
	/*memcpy((_S)->format->palette->colors, base_palette, 16*sizeof(SDL_Color));*/
	for (int y = 0; y < h; y++) for (int x = 0; x < w; x++) {
		unsigned char pix = getpixel(bmp, x, y);
		for (int i = 0; i < scale; i++) for (int j = 0; j < scale; j++) {
			data[x*scale+i + (y*scale+j)*w*scale] = pix;
		}
	}
	SDL_FreeSurface(bmp);
	SDL_SetPalette(surf, SDL_PHYSPAL | SDL_LOGPAL, (SDL_Color*)base_palette, 0, 16);
	SDL_SetColorKey(surf, SDL_SRCCOLORKEY, 0);

	*s = surf;
}

#define LOGLOAD(w) printf("loading %s...", w)
#define LOGDONE() printf("done\n")

static void LoadData(void) {
	LOGLOAD("gfx.bmp");
	loadbmpscale("gfx.bmp", &gfx);
	LOGDONE();
	
	LOGLOAD("font.bmp");
	loadbmpscale("font.bmp", &font);
	LOGDONE();

	static const char sndids[] = {0,1,2,3,4,5,6,7,8,9,13,14,15,16,23,35,37,38,40,50,51,54,55};
	for (int iid = 0; iid < sizeof sndids; iid++) {
		int id = sndids[iid];
		char fname[20];
		sprintf(fname, "snd%i.wav", id);
		char path[4096];
		LOGLOAD(fname);
		GetDataPath(path, sizeof path, fname);
		snd[id] = Mix_LoadWAV(path);
		if (!snd[id]) {
			ErrLog("snd%i: Mix_LoadWAV: %s\n", id, Mix_GetError());
		}
		LOGDONE();
	}
	static const char musids[] = {0,10,20,30,40};
	for (int iid = 0; iid < sizeof musids; iid++) {
		int id = musids[iid];
		char fname[20];
		sprintf(fname, "mus%i.ogg", id);
		LOGLOAD(fname);
		char path[4096];
		GetDataPath(path, sizeof path, fname);
		mus[id/10] = Mix_LoadMUS(path);
		if (!mus[id/10]) {
			ErrLog("mus%i: Mix_LoadMUS: %s\n", id, Mix_GetError());
		}
		LOGDONE();
	}
}
#include "tilemap.h"

static Uint16 buttons_state = 0;

#define SDL_CHECK(r) do {                               \
	if (!(r)) {                                           \
		ErrLog("%s:%i, fatal error: `%s` -> %s\n", \
		        __FILE__, __LINE__, #r, SDL_GetError());    \
		exit(2);                                            \
	}                                                     \
} while(0)

static void p8_rectfill(int x0, int y0, int x1, int y1, int col);
static void p8_print(const char* str, int x, int y, int col);

//on-screen display (for info, such as loading a state, toggling screenshake, toggling fullscreen, etc)
static char osd_text[200] = "";
static int osd_timer = 0;
static void OSDset(const char* fmt, ...) {
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(osd_text, sizeof osd_text, fmt, ap);
	osd_text[sizeof osd_text - 1] = '\0'; //make sure to add NUL terminator in case of truncation
	printf("%s\n", osd_text);
	osd_timer = 30;
	va_end(ap);
}
static void OSDdraw(void) {
	if (osd_timer > 0) {
		--osd_timer;
		const int x = 4;
		const int y = 120 + (osd_timer < 10 ? 10-osd_timer : 0); //disappear by going below the screen
		p8_rectfill(x-2, y-2, x+4*strlen(osd_text), y+6, 6); //outline
		p8_rectfill(x-1, y-1, x+4*strlen(osd_text)-1, y+5, 0);
		p8_print(osd_text, x, y, 7);
	}
}
	
static Mix_Music* current_music = NULL;
static _Bool enable_screenshake = 1;
static _Bool paused = 0;
static _Bool running = 1;
static void* initial_game_state = NULL;
static void* game_state = NULL;
static Mix_Music* game_state_music = NULL;
static void mainLoop(void);
static FILE* TAS = NULL;

enum HostMenu {
	HOST_MENU_NONE,
	HOST_MENU_MAIN,
	HOST_MENU_STATS,
};

static enum HostMenu host_menu = HOST_MENU_NONE;
static int host_menu_selection = 0;
static HostTotals host_totals;
static HostTotals host_run_stats;
static HostTotals game_state_run_stats;
static uint64_t host_run_millis = 0;
static uint64_t game_state_run_millis = 0;
static _Bool host_run_finished = 0;
static _Bool game_state_run_finished = 0;
static _Bool host_has_focus = 1;
static _Bool host_consume_game_input = 0;
static Celeste_P8_Telemetry host_previous_telemetry;
static _Bool host_telemetry_ready = 0;
static Uint32 host_previous_tick = 0;
static Uint32 host_last_save_tick = 0;
static int host_last_room = -1;
static char host_save_path[4096] = "";
static char host_save_tmp_path[4096] = "";
static char host_backup_path[4096] = "";
static char host_backup_tmp_path[4096] = "";
static _Bool host_save_enabled = 1;

static void HostInitPersistence(void);
static void HostUpdateStats(void);
static void HostSaveProgress(void);
static void HostResetRun(void);
static void HostDrawTimer(void);
static void HostDrawMenu(void);
static void HostSetMenu(enum HostMenu menu);
static void HostActivateMenuSelection(void);

#ifdef _3DS
// hack: newer SDL versions remove SDL_N3DSKeyBind, but I'm too lazy to change the
// code to properly use SDL_Joystick inputs on 3DS so work around it ...
static short n3ds_key_map[32];

static void SDL_N3DSKeyBind(int n3dskey, int kbkey) {
	for (int i = 0; i < 32; i++)
		if (n3dskey & (1u << i))
			n3ds_key_map[i] = kbkey;
}
#define SDL_GetKeyState n3ds_get_fake_key_state
static Uint8 *n3ds_get_fake_key_state(int *numkeys) {
	static Uint8 st[SDLK_LAST];
	if (numkeys) *numkeys = SDLK_LAST;

	memset(st, 0, sizeof st);
	hidScanInput();
	Uint32 down = hidKeysDown();
	Uint32 held = hidKeysHeld();
	for (int i = 0; i < 32; i++) {
		st[n3ds_key_map[i]] |= (held & (1u << i)) != 0;
		if (down & (1u << i)) {
			SDL_Event ev;
			ev.type = SDL_KEYDOWN;
			ev.key.keysym.sym = n3ds_key_map[i];
			SDL_PushEvent(&ev);
		}
	}

	return st;
}
#endif

#if !defined(_3DS) && !defined(EMSCRIPTEN)
static _Bool HostReplaceFile(const char* source, const char* destination) {
#ifdef _WIN32
	WCHAR wide_source[4096];
	WCHAR wide_destination[4096];
	if (!DesktopWidePath(source, wide_source,
		(int)(sizeof wide_source / sizeof *wide_source))) return 0;
	if (!DesktopWidePath(destination, wide_destination,
		(int)(sizeof wide_destination / sizeof *wide_destination))) return 0;
	return MoveFileExW(wide_source, wide_destination,
		MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
#else
	return rename(source, destination) == 0;
#endif
}

static int HostCurrentMusicIndex(void) {
	for (int i = 0; i < (int)(sizeof mus / sizeof *mus); i++) {
		if (current_music == mus[i]) return i;
	}
	return -1;
}

static _Bool HostV2StatsValid(const HostSaveHeaderV2* header) {
	return header->run_millis <= header->totals.ingame_millis
		&& header->run_stats.jumps <= header->totals.jumps
		&& header->run_stats.dashes <= header->totals.dashes
		&& header->run_stats.climb_pixels <= header->totals.climb_pixels
		&& header->run_stats.deaths <= header->totals.deaths
		&& header->run_stats.completions <= header->totals.completions;
}

static _Bool HostV3StatsValid(const HostSaveHeaderV3* header) {
	return header->run_millis <= header->totals.ingame_millis
		&& header->run_stats.jumps <= header->totals.jumps
		&& header->run_stats.dashes <= header->totals.dashes
		&& header->run_stats.climb_pixels <= header->totals.climb_pixels
		&& header->run_stats.deaths <= header->totals.deaths
		&& header->run_stats.completions <= header->totals.completions
		&& header->run_stats.strawberries <= header->totals.strawberries
		&& header->run_stats.stages <= header->totals.stages;
}
#endif

static void HostFormatDuration(char* output, size_t output_size, uint64_t millis, _Bool centiseconds) {
	uint64_t hours = millis / 3600000u;
	unsigned minutes = (unsigned)((millis / 60000u) % 60u);
	unsigned seconds = (unsigned)((millis / 1000u) % 60u);
	if (centiseconds) {
		unsigned hundredths = (unsigned)((millis / 10u) % 100u);
		snprintf(output, output_size, "%02llu:%02u:%02u.%02u",
			(unsigned long long)hours, minutes, seconds, hundredths);
	} else {
		snprintf(output, output_size, "%lluh %02um %02us",
			(unsigned long long)hours, minutes, seconds);
	}
}

static void HostFormatCount(char* output, size_t output_size, uint64_t value) {
	if (value < 10000u)
		snprintf(output, output_size, "%llu", (unsigned long long)value);
	else if (value < 1000000u)
		snprintf(output, output_size, "%lluK", (unsigned long long)(value / 1000u));
	else if (value < 1000000000u)
		snprintf(output, output_size, "%lluM", (unsigned long long)(value / 1000000u));
	else if (value < 1000000000000u)
		snprintf(output, output_size, "%lluB", (unsigned long long)(value / 1000000000u));
	else
		snprintf(output, output_size, "999+");
}

static void HostSetMenu(enum HostMenu menu) {
	if (menu == HOST_MENU_NONE) {
		host_menu = menu;
		paused = 0;
		if (host_has_focus) {
			Mix_Resume(-1);
			Mix_ResumeMusic();
		}
		return;
	}
	if (host_menu == HOST_MENU_NONE) {
		Mix_Pause(-1);
		Mix_PauseMusic();
	}
	host_menu = menu;
	paused = 1;
}

static void HostResetRun(void) {
	if (!initial_game_state) return;
	Celeste_P8_load_state(initial_game_state);
	Celeste_P8_set_rndseed((unsigned)(time(NULL) + SDL_GetTicks()));
	Mix_HaltChannel(-1);
	Mix_HaltMusic();
	current_music = NULL;
	Celeste_P8_init();
	host_run_millis = 0;
	host_run_finished = 0;
	memset(&host_run_stats, 0, sizeof host_run_stats);
	Celeste_P8_get_telemetry(&host_previous_telemetry);
	host_telemetry_ready = 1;
	host_last_room = host_previous_telemetry.room;
	host_previous_tick = SDL_GetTicks();
	HostSetMenu(HOST_MENU_NONE);
	HostSaveProgress();
	OSDset("run reset");
}

static void HostActivateMenuSelection(void) {
	if (host_menu == HOST_MENU_NONE) return;
	if (host_menu == HOST_MENU_STATS) {
		HostSetMenu(HOST_MENU_MAIN);
		return;
	}
	switch (host_menu_selection) {
		case 0: HostSetMenu(HOST_MENU_NONE); break;
		case 1: host_menu = HOST_MENU_STATS; break;
		case 2: HostResetRun(); break;
		case 3: HostSaveProgress(); running = 0; break;
	}
}

static void HostInitPersistence(void) {
	host_previous_tick = host_last_save_tick = SDL_GetTicks();
	Celeste_P8_get_telemetry(&host_previous_telemetry);
	host_telemetry_ready = 1;
	host_last_room = host_previous_telemetry.room;
	if (TAS) return;

#if !defined(_3DS) && !defined(EMSCRIPTEN) && SDL_MAJOR_VERSION >= 2
	char* preference_path = SDL_GetPrefPath("celeste-classic", "celeste-classic");
	if (!preference_path) {
		ErrLog("SDL_GetPrefPath: %s\n", SDL_GetError());
		return;
	}
	snprintf(host_save_path, sizeof host_save_path, "%sprogress.dat", preference_path);
	snprintf(host_save_tmp_path, sizeof host_save_tmp_path, "%sprogress.tmp", preference_path);
	snprintf(host_backup_path, sizeof host_backup_path, "%sprogress.v2.bak", preference_path);
	snprintf(host_backup_tmp_path, sizeof host_backup_tmp_path, "%sprogress.v2.bak.tmp", preference_path);
	SDL_free(preference_path);

	FILE* file = DesktopOpenFile(host_save_path, "rb");
	if (!file) return;
	HostSaveHeaderV2 header_v2;
	HostSaveHeaderV3 header_v3;
	memset(&header_v2, 0, sizeof header_v2);
	memset(&header_v3, 0, sizeof header_v3);
	_Bool valid = fread(&header_v2, sizeof header_v2, 1, file) == 1;
	_Bool migrate_v2 = valid
		&& memcmp(header_v2.magic, HOST_SAVE_V2_MAGIC, 8) == 0
		&& header_v2.version == HOST_SAVE_V2_VERSION;
	_Bool load_v3 = valid
		&& memcmp(header_v2.magic, HOST_SAVE_V3_MAGIC, 8) == 0
		&& header_v2.version == HOST_SAVE_V3_VERSION;
	if (load_v3) {
		valid = fseek(file, 0, SEEK_SET) == 0
			&& fread(&header_v3, sizeof header_v3, 1, file) == 1;
	} else if (!migrate_v2) {
		valid = 0;
	}
	size_t state_size = Celeste_P8_get_state_size();
	uint32_t saved_state_size = migrate_v2 ? header_v2.state_size : header_v3.state_size;
	uint32_t run_finished = migrate_v2 ? header_v2.run_finished : header_v3.run_finished;
	int music_index = migrate_v2 ? header_v2.music_index : header_v3.music_index;
	valid = valid && saved_state_size == state_size;
	valid = valid && run_finished <= 1;
	valid = valid && music_index >= -1 && music_index < (int)(sizeof mus / sizeof *mus);
	valid = valid && (migrate_v2 ? HostV2StatsValid(&header_v2) : HostV3StatsValid(&header_v3));
	void* state = valid ? SDL_malloc(state_size) : NULL;
	valid = valid && state && fread(state, state_size, 1, file) == 1;
	valid = valid && fgetc(file) == EOF && !ferror(file);
	uint32_t expected_checksum = migrate_v2 ? header_v2.checksum : header_v3.checksum;
	uint32_t checksum;
	if (migrate_v2) {
		header_v2.checksum = 0;
		checksum = HostChecksum(&header_v2, sizeof header_v2, 2166136261u);
	} else {
		header_v3.checksum = 0;
		checksum = HostChecksum(&header_v3, sizeof header_v3, 2166136261u);
	}
	if (valid) checksum = HostChecksum(state, state_size, checksum);
	valid = valid && checksum == expected_checksum;
	fclose(file);

	if (!valid) {
		ErrLog("ignored invalid save: %s\n", host_save_path);
		if (state) SDL_free(state);
		return;
	}
	_Bool migration_backup_ready = !migrate_v2 || DesktopPreserveBackup(
		host_save_path, host_backup_path, host_backup_tmp_path);
	if (!migration_backup_ready) {
		host_save_enabled = 0;
		ErrLog("couldn't preserve V2 backup; progress remains read-only: %s\n", host_backup_path);
	}

	Celeste_P8_load_state(state);
	SDL_free(state);
	if (migrate_v2) {
		HostSaveHeaderV3 migrated_header;
		Celeste_P8_get_telemetry(&host_previous_telemetry);
		HostMigrateV2Header(
			&migrated_header, &header_v2,
			host_previous_telemetry.fruit_mask,
			host_previous_telemetry.room);
		host_totals = migrated_header.totals;
		host_run_stats = migrated_header.run_stats;
	} else {
		host_totals = header_v3.totals;
		host_run_stats = header_v3.run_stats;
	}
	host_run_millis = migrate_v2 ? header_v2.run_millis : header_v3.run_millis;
	host_run_finished = (_Bool)run_finished;
	Mix_HaltMusic();
	current_music = NULL;
	if (music_index >= 0 && mus[music_index]) {
		current_music = mus[music_index];
		Mix_PlayMusic(current_music, -1);
	}
	Celeste_P8_get_telemetry(&host_previous_telemetry);
	host_last_room = host_previous_telemetry.room;
	if (migrate_v2 && migration_backup_ready) {
		HostSaveProgress();
		OSDset("progress upgraded");
	} else if (migrate_v2) {
		OSDset("backup failed; save off");
	} else {
		OSDset("progress loaded");
	}
#endif
}

static void HostSaveProgress(void) {
	if (TAS || !host_save_enabled || !host_telemetry_ready || !host_save_path[0]) return;
#if !defined(_3DS) && !defined(EMSCRIPTEN) && SDL_MAJOR_VERSION >= 2
	size_t state_size = Celeste_P8_get_state_size();
	if (state_size > UINT32_MAX) return;
	void* state = SDL_malloc(state_size);
	if (!state) return;
	Celeste_P8_save_state(state);

	HostSaveHeaderV3 header;
	memset(&header, 0, sizeof header);
	memcpy(header.magic, HOST_SAVE_V3_MAGIC, 8);
	header.version = HOST_SAVE_V3_VERSION;
	header.state_size = (uint32_t)state_size;
	header.music_index = HostCurrentMusicIndex();
	header.run_millis = host_run_millis;
	header.run_finished = host_run_finished;
	header.run_stats = host_run_stats;
	header.totals = host_totals;
	header.checksum = HostChecksum(&header, sizeof header, 2166136261u);
	header.checksum = HostChecksum(state, state_size, header.checksum);

	FILE* file = DesktopOpenFile(host_save_tmp_path, "wb");
	_Bool saved = file != NULL;
	saved = saved && fwrite(&header, sizeof header, 1, file) == 1;
	saved = saved && fwrite(state, state_size, 1, file) == 1;
	saved = saved && DesktopFlushFile(file);
	if (file && fclose(file) != 0) saved = 0;
	if (saved) saved = HostReplaceFile(host_save_tmp_path, host_save_path);
	if (!saved) {
#ifdef _WIN32
		ErrLog("couldn't save progress '%s': errno %d, Win32 %lu\n",
			host_save_path, errno, (unsigned long)GetLastError());
#else
		ErrLog("couldn't save progress '%s': %s\n", host_save_path, strerror(errno));
#endif
	}
	SDL_free(state);
	host_last_save_tick = SDL_GetTicks();
#endif
}

static void HostUpdateStats(void) {
	Celeste_P8_Telemetry telemetry;
	Celeste_P8_get_telemetry(&telemetry);
	Uint32 now = SDL_GetTicks();
	Uint32 elapsed = now - host_previous_tick;
	host_previous_tick = now;
	_Bool new_run = host_telemetry_ready && host_previous_telemetry.is_title && !telemetry.is_title;
	if (new_run) {
		host_run_millis = 0;
		host_run_finished = 0;
		memset(&host_run_stats, 0, sizeof host_run_stats);
	}

	if (host_telemetry_ready) {
#define ADD_TELEMETRY_TOTAL(field) \
		if (telemetry.field >= host_previous_telemetry.field) { \
			uint64_t delta = telemetry.field - host_previous_telemetry.field; \
			host_run_stats.field += delta; \
			host_totals.field += delta; \
		}
		ADD_TELEMETRY_TOTAL(jumps);
		ADD_TELEMETRY_TOTAL(dashes);
		ADD_TELEMETRY_TOTAL(climb_pixels);
		ADD_TELEMETRY_TOTAL(completions);
#undef ADD_TELEMETRY_TOTAL
		if (telemetry.deaths >= host_previous_telemetry.deaths) {
			uint64_t delta = (uint64_t)(telemetry.deaths - host_previous_telemetry.deaths);
			host_run_stats.deaths += delta;
			host_totals.deaths += delta;
		}
		uint64_t strawberries = HostCountNewStrawberries(
			host_previous_telemetry.fruit_mask, telemetry.fruit_mask);
		host_run_stats.strawberries += strawberries;
		host_totals.strawberries += strawberries;
		uint64_t stages = HostCompletedStage(
			host_previous_telemetry.room, telemetry.room);
		host_run_stats.stages += stages;
		host_totals.stages += stages;
	}

	if (!paused && host_has_focus && !telemetry.is_title) {
		if (!host_run_finished) host_run_millis += elapsed;
		host_totals.ingame_millis += elapsed;
	}
	if (host_telemetry_ready
	 && telemetry.completions > host_previous_telemetry.completions)
		host_run_finished = 1;
	host_previous_telemetry = telemetry;
	host_telemetry_ready = 1;

	if (!TAS && telemetry.room != host_last_room) {
		host_last_room = telemetry.room;
		HostSaveProgress();
	} else if (!TAS && now - host_last_save_tick >= 10000u) {
		HostSaveProgress();
	}
}

static void HostDrawTimer(void) {
#if HOST_PANEL_W > 0
	Celeste_P8_Telemetry telemetry;
	Celeste_P8_get_telemetry(&telemetry);
	char time_text[32];
	HostFormatDuration(time_text, sizeof time_text, host_run_millis, 1);
	p8_rectfill(PICO8_W, 0, HOST_W - 1, PICO8_H - 1, 0);
	p8_rectfill(PICO8_W, 0, PICO8_W + 1, PICO8_H - 1, 1);
	p8_print("speedrun", PICO8_W + 9, 10, 6);
	p8_print(telemetry.is_title ? "--:--:--.--" : time_text, PICO8_W + 9, 20, 7);
	if (!telemetry.is_title) {
		char room_text[24];
		snprintf(room_text, sizeof room_text, "stage %d", telemetry.room + 1);
		if (host_run_finished) p8_print("finished", PICO8_W + 9, 33, 11);
		p8_print(room_text, PICO8_W + 9, 44, 6);
	}
	p8_print("esc: menu", PICO8_W + 9, 110, 5);
#endif
}

static void HostDrawMenu(void) {
	if (host_menu == HOST_MENU_NONE) return;
#if HOST_PANEL_W > 0
	const int panel_x = PICO8_W + 6;
	p8_rectfill(PICO8_W, 0, HOST_W - 1, PICO8_H - 1, 0);
	p8_rectfill(PICO8_W, 0, PICO8_W + 1, PICO8_H - 1, 1);

	if (host_menu == HOST_MENU_MAIN) {
		static const char* items[] = {"resume", "stats", "reset to start", "save and quit"};
		p8_print("menu", panel_x, 10, 7);
		for (int i = 0; i < (int)(sizeof items / sizeof *items); i++) {
			char line[32];
			snprintf(line, sizeof line, "%c %s", i == host_menu_selection ? '>' : ' ', items[i]);
			p8_print(line, panel_x, 31 + i * 14, i == host_menu_selection ? 10 : 6);
		}
		if (host_menu_selection == 2) p8_print("keeps all-time stats", panel_x, 95, 5);
		p8_print("arrows + z/enter", panel_x, 110, 5);
		return;
	}

	char line[64], run_time[32], total_time[32];
	HostFormatDuration(run_time, sizeof run_time, host_run_millis, 0);
	HostFormatDuration(total_time, sizeof total_time, host_totals.ingame_millis, 0);
	p8_print("stats", panel_x, 4, 7);
	p8_print("current / all time", panel_x, 14, 6);
#define DRAW_STAT(y, label, current, total) do { \
		char current_text[8], total_text[8]; \
		HostFormatCount(current_text, sizeof current_text, (uint64_t)(current)); \
		HostFormatCount(total_text, sizeof total_text, (uint64_t)(total)); \
		snprintf(line, sizeof line, "%s %s/%s", label, current_text, total_text); \
		p8_print(line, panel_x, y, 7); \
	} while (0)
	DRAW_STAT(25, "jumps", host_run_stats.jumps, host_totals.jumps);
	DRAW_STAT(34, "dashes", host_run_stats.dashes, host_totals.dashes);
	DRAW_STAT(43, "berries", host_run_stats.strawberries, host_totals.strawberries);
	DRAW_STAT(52, "stages", host_run_stats.stages, host_totals.stages);
	DRAW_STAT(61, "climbed m", host_run_stats.climb_pixels * 100u / 128u,
		host_totals.climb_pixels * 100u / 128u);
	DRAW_STAT(70, "deaths", host_run_stats.deaths, host_totals.deaths);
	DRAW_STAT(79, "completions", host_run_stats.completions, host_totals.completions);
#undef DRAW_STAT
	snprintf(line, sizeof line, "time %s", run_time);
	p8_print(line, panel_x, 90, 7);
	snprintf(line, sizeof line, "all  %s", total_time);
	p8_print(line, panel_x, 99, 7);
	p8_print("z/enter/esc: back", panel_x, 113, 5);
#endif
}

int main(int argc, char** argv) {
	SDL_CHECK(SDL_Init(SDL_INIT_AUDIO | SDL_INIT_VIDEO) == 0);
#if SDL_MAJOR_VERSION >= 2
	SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER);
	SDL_GameControllerAddMappingsFromRW(SDL_RWFromFile("gamecontrollerdb.txt", "rb"), 1);
#endif
	int videoflag = SDL_SWSURFACE | SDL_HWPALETTE;
#ifdef _3DS
	fsInit();
	romfsInit();
	videoflag = SDL_DOUBLEBUF | SDL_HWSURFACE | SDL_CONSOLEBOTTOM | SDL_TOPSCR;
	SDL_N3DSKeyBind(KEY_A, SDLK_z);
	SDL_N3DSKeyBind(KEY_X|KEY_B, SDLK_x);
	SDL_N3DSKeyBind(KEY_CPAD_UP|KEY_CSTICK_UP|KEY_DUP, SDLK_UP);
	SDL_N3DSKeyBind(KEY_CPAD_DOWN|KEY_CSTICK_DOWN|KEY_DDOWN, SDLK_DOWN);
	SDL_N3DSKeyBind(KEY_CPAD_LEFT|KEY_CSTICK_LEFT|KEY_DLEFT, SDLK_LEFT);
	SDL_N3DSKeyBind(KEY_CPAD_RIGHT|KEY_CSTICK_RIGHT|KEY_DRIGHT, SDLK_RIGHT);
	SDL_N3DSKeyBind(KEY_SELECT, SDLK_F11); //to switch full screen
	SDL_N3DSKeyBind(KEY_START, SDLK_ESCAPE); //to pause
	
	SDL_N3DSKeyBind(KEY_Y, SDLK_LSHIFT); //hold to reset / load/save state
	SDL_N3DSKeyBind(KEY_L, SDLK_d); //load state
	SDL_N3DSKeyBind(KEY_R, SDLK_s); //save state
#endif
	SDL_CHECK(screen = SDL_SetVideoMode(HOST_W*scale, PICO8_H*scale, 32, videoflag));
	SDL_WM_SetCaption("Celeste", NULL);
	int mixflag = MIX_INIT_OGG;
	if (Mix_Init(mixflag) != mixflag) {
		ErrLog("Mix_Init: %s\n", Mix_GetError());
	}
	if (Mix_OpenAudio(22050, AUDIO_S16SYS, 1, 1024) < 0) {
		ErrLog("Mix_Init: %s\n", Mix_GetError());
	}
	ResetPalette();
	SDL_ShowCursor(0);

	if (argc > 1) {
		TAS = fopen(argv[1], "r");
		if (!TAS) {
			printf("couldn't open TAS file '%s': %s\n", argv[1], strerror(errno));
		}
	}

	printf("game state size %gkb\n", Celeste_P8_get_state_size()/1024.);

	printf("now loading...\n");

	{
		const unsigned char loading_bmp[] = {
			0x42,0x4d,0xca,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x82,0x00,
			0x00,0x00,0x6c,0x00,0x00,0x00,0x24,0x00,0x00,0x00,0x09,0x00,
			0x00,0x00,0x01,0x00,0x01,0x00,0x00,0x00,0x00,0x00,0x48,0x00,
			0x00,0x00,0x23,0x2e,0x00,0x00,0x23,0x2e,0x00,0x00,0x02,0x00,
			0x00,0x00,0x02,0x00,0x00,0x00,0x42,0x47,0x52,0x73,0x00,0x00,
			0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
			0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
			0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
			0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x02,0x00,
			0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
			0x00,0x00,0x00,0x00,0x00,0x00,0xff,0xff,0xff,0x00,0x00,0x00,
			0x00,0x00,0xe0,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x10,0x00,
			0x00,0x00,0x66,0x3e,0xf1,0x24,0xf0,0x00,0x00,0x00,0x49,0x44,
			0x92,0x24,0x90,0x00,0x00,0x00,0x49,0x3c,0x92,0x24,0x90,0x00,
			0x00,0x00,0x49,0x04,0x92,0x24,0x90,0x00,0x00,0x00,0x46,0x38,
			0xf0,0x3c,0xf0,0x00,0x00,0x00,0x40,0x00,0x12,0x00,0x00,0x00,
			0x00,0x00,0xc0,0x00,0x10,0x00,0x00,0x00,0x00,0x00
		};
		SDL_RWops* rw = SDL_RWFromConstMem(loading_bmp, sizeof loading_bmp);
		SDL_Surface* loading = SDL_LoadBMP_RW(rw, 1);
		if (!loading) goto skip_load;

		SDL_Rect rc = {60, 60};
		SDL_BlitSurface(loading,NULL,screen,&rc);
		
		SDL_Flip(screen);
		SDL_FreeSurface(loading);
	} skip_load:

	LoadData();

	int pico8emu(CELESTE_P8_CALLBACK_TYPE call, ...);
	Celeste_P8_set_call_func(pico8emu);

	//for reset
	initial_game_state = SDL_malloc(Celeste_P8_get_state_size());
	if (initial_game_state) Celeste_P8_save_state(initial_game_state);

	if (TAS) {
		// a consistent seed for tas playback
		Celeste_P8_set_rndseed(8);
	} else {
		Celeste_P8_set_rndseed((unsigned)(time(NULL) + SDL_GetTicks()));
	}

	Celeste_P8_init();
	HostInitPersistence();

	printf("ready\n");
	{
		FILE* start_fullscreen_f = fopen("ccleste-start-fullscreen.txt", "r");
		const char* start_fullscreen_v = getenv("CCLESTE_START_FULLSCREEN");
		if (start_fullscreen_f || (start_fullscreen_v && *start_fullscreen_v)) {
			SDL_WM_ToggleFullScreen(screen);
		}
		if (start_fullscreen_f) fclose(start_fullscreen_f);
	}

#ifdef _3DS
	while (aptMainLoop()) mainLoop();
#elif !defined(EMSCRIPTEN)
	while (running) mainLoop();
#else
#include <emscripten.h>
	//FIXME: this assumes that the display refreshes at 60Hz
	emscripten_set_main_loop(mainLoop, 0, 0);
	emscripten_set_main_loop_timing(EM_TIMING_RAF, 2);
	return 0;
#endif

	HostSaveProgress();
	if (game_state) SDL_free(game_state);
	if (initial_game_state) SDL_free(initial_game_state);

	SDL_FreeSurface(gfx);
	SDL_FreeSurface(font);
	for (int i = 0; i < (sizeof snd)/(sizeof *snd); i++) {
		if (snd[i]) Mix_FreeChunk(snd[i]);
	}
	for (int i = 0; i < (sizeof mus)/(sizeof *mus); i++) {
		if (mus[i]) Mix_FreeMusic(mus[i]);
	}

	Mix_CloseAudio();
	Mix_Quit();
	SDL_Quit();
	return 0;
}

#if SDL_MAJOR_VERSION >= 2
/* These inputs aren't sent to the game. */
enum {
	PSEUDO_BTN_SAVE_STATE = 6,
	PSEUDO_BTN_LOAD_STATE = 7,
	PSEUDO_BTN_EXIT = 8,
	PSEUDO_BTN_PAUSE = 9,
};
static void ReadGamepadInput(Uint16* out_buttons);
#endif

static void mainLoop(void) {
	const Uint8* kbstate = SDL_GetKeyState(NULL);
	_Bool menu_was_open = host_menu != HOST_MENU_NONE;
		
	static int reset_input_timer = 0;
	//hold F9 (select+start+y) to reset
	if (initial_game_state != NULL
#ifdef _3DS
			&& kbstate[SDLK_LSHIFT] && kbstate[SDLK_ESCAPE] && kbstate[SDLK_F11]
#else
			&& kbstate[SDLK_F9]
#endif
	) {
		reset_input_timer++;
		if (reset_input_timer >= 30) {
			reset_input_timer=0;
			HostResetRun();
		}
	} else reset_input_timer = 0;

	Uint16 prev_buttons_state = buttons_state;
	buttons_state = 0;

#if SDL_MAJOR_VERSION >= 2
	SDL_GameControllerUpdate();
	ReadGamepadInput(&buttons_state);

	if (host_menu != HOST_MENU_NONE) {
		if (!((prev_buttons_state >> PSEUDO_BTN_PAUSE) & 1)
		 && (buttons_state >> PSEUDO_BTN_PAUSE) & 1) {
			if (host_menu == HOST_MENU_STATS) HostSetMenu(HOST_MENU_MAIN);
			else HostSetMenu(HOST_MENU_NONE);
		}
		if (host_menu == HOST_MENU_MAIN
		 && !((prev_buttons_state >> 2) & 1) && ((buttons_state >> 2) & 1))
			host_menu_selection = (host_menu_selection + 3) % 4;
		if (host_menu == HOST_MENU_MAIN
		 && !((prev_buttons_state >> 3) & 1) && ((buttons_state >> 3) & 1))
			host_menu_selection = (host_menu_selection + 1) % 4;
		if (!((prev_buttons_state >> 4) & 1) && ((buttons_state >> 4) & 1))
			HostActivateMenuSelection();
		if (host_menu == HOST_MENU_STATS
		 && !((prev_buttons_state >> 5) & 1) && ((buttons_state >> 5) & 1))
			HostSetMenu(HOST_MENU_MAIN);
	} else if (!((prev_buttons_state >> PSEUDO_BTN_PAUSE) & 1)
	 && (buttons_state >> PSEUDO_BTN_PAUSE) & 1) {
		goto toggle_pause;
	}

	if (host_menu == HOST_MENU_NONE
	 && !((prev_buttons_state >> PSEUDO_BTN_EXIT) & 1)
	 && (buttons_state >> PSEUDO_BTN_EXIT) & 1) {
		goto press_exit;
	}

	if (host_menu == HOST_MENU_NONE
	 && !((prev_buttons_state >> PSEUDO_BTN_SAVE_STATE) & 1)
	 && (buttons_state >> PSEUDO_BTN_SAVE_STATE) & 1) {
		goto save_state;
	}

	if (host_menu == HOST_MENU_NONE
	 && !((prev_buttons_state >> PSEUDO_BTN_LOAD_STATE) & 1)
	 && (buttons_state >> PSEUDO_BTN_LOAD_STATE) & 1) {
		goto load_state;
	}
#endif

	SDL_Event ev;
	while (SDL_PollEvent(&ev)) switch (ev.type) {
		case SDL_QUIT: running = 0; break;
#if SDL_MAJOR_VERSION >= 2
		case SDL_WINDOWEVENT:
			if (ev.window.event == SDL_WINDOWEVENT_FOCUS_LOST
			 || ev.window.event == SDL_WINDOWEVENT_MINIMIZED) {
				host_has_focus = 0;
				Mix_Pause(-1);
				Mix_PauseMusic();
			} else if (ev.window.event == SDL_WINDOWEVENT_FOCUS_GAINED) {
				host_has_focus = 1;
				host_previous_tick = SDL_GetTicks();
				if (!paused) {
					Mix_Resume(-1);
					Mix_ResumeMusic();
				}
			}
			break;
#else
		case SDL_ACTIVEEVENT:
			if (ev.active.state & SDL_APPINPUTFOCUS) {
				host_has_focus = ev.active.gain != 0;
				if (!host_has_focus) {
					Mix_Pause(-1);
					Mix_PauseMusic();
				} else if (!paused) {
					host_previous_tick = SDL_GetTicks();
					Mix_Resume(-1);
					Mix_ResumeMusic();
				}
			}
			break;
#endif
		case SDL_KEYDOWN: {
#if SDL_MAJOR_VERSION >= 2
			if (ev.key.repeat) break; //no key repeat
#endif
			if (host_menu != HOST_MENU_NONE) {
				if (ev.key.keysym.sym == SDLK_ESCAPE) {
					if (host_menu == HOST_MENU_STATS) HostSetMenu(HOST_MENU_MAIN);
					else HostSetMenu(HOST_MENU_NONE);
				} else if (host_menu == HOST_MENU_MAIN && ev.key.keysym.sym == SDLK_UP) {
					host_menu_selection = (host_menu_selection + 3) % 4;
				} else if (host_menu == HOST_MENU_MAIN && ev.key.keysym.sym == SDLK_DOWN) {
					host_menu_selection = (host_menu_selection + 1) % 4;
				} else if (ev.key.keysym.sym == SDLK_z
#if SDL_MAJOR_VERSION >= 2
					|| ev.key.keysym.sym == SDL_SCANCODE_RETURN
#else
					|| ev.key.keysym.sym == SDLK_RETURN
#endif
				) {
					HostActivateMenuSelection();
				}
				break;
			}
			if (ev.key.keysym.sym == SDLK_ESCAPE) { //do pause
				toggle_pause:
			#if HOST_PANEL_W > 0
				HostSetMenu(HOST_MENU_MAIN);
			#else
				if (paused) Mix_Resume(-1), Mix_ResumeMusic(); else Mix_Pause(-1), Mix_PauseMusic();
				paused = !paused;
			#endif
				break;
			} else if (ev.key.keysym.sym == SDLK_DELETE) { //exit
				press_exit:
				running = 0;
				break;
			} else if (ev.key.keysym.sym == SDLK_F11 && !(kbstate[SDLK_LSHIFT] || kbstate[SDLK_ESCAPE])) {
				if (SDL_WM_ToggleFullScreen(screen)) { //this doesn't work on windows..
					OSDset("toggle fullscreen");
				}
				screen = SDL_GetVideoSurface();
				break;
			} else if (0 && ev.key.keysym.sym == SDLK_5) {
				Celeste_P8__DEBUG();
				break;
			} else if (ev.key.keysym.sym == SDLK_s && kbstate[SDLK_LSHIFT]) { //save state
				save_state:
				game_state = game_state ? game_state : SDL_malloc(Celeste_P8_get_state_size());
				if (game_state) {
					OSDset("save state");
					Celeste_P8_save_state(game_state);
					game_state_music = current_music;
					game_state_run_stats = host_run_stats;
					game_state_run_millis = host_run_millis;
					game_state_run_finished = host_run_finished;
				}
				break;
			} else if (ev.key.keysym.sym == SDLK_d && kbstate[SDLK_LSHIFT]) { //load state
				load_state:
				if (game_state) {
					OSDset("load state");
					if (paused) paused = 0, Mix_Resume(-1), Mix_ResumeMusic();
					Celeste_P8_load_state(game_state);
					host_run_stats = game_state_run_stats;
					host_run_millis = game_state_run_millis;
					host_run_finished = game_state_run_finished;
					Celeste_P8_get_telemetry(&host_previous_telemetry);
					host_telemetry_ready = 1;
					host_previous_tick = SDL_GetTicks();
					host_last_room = host_previous_telemetry.room;
					if (current_music != game_state_music) {
						Mix_HaltMusic();
						current_music = game_state_music;
						if (game_state_music) Mix_PlayMusic(game_state_music, -1);
					}
				}
				break;
			} else if ( //toggle screenshake (e / L+R)
#ifdef _3DS
					(ev.key.keysym.sym == SDLK_d && kbstate[SDLK_s]) || (ev.key.keysym.sym == SDLK_s && kbstate[SDLK_d])
#else
					ev.key.keysym.sym == SDLK_e
#endif
					) {
				enable_screenshake = !enable_screenshake;
				OSDset("screenshake: %s", enable_screenshake ? "on" : "off");
			} break;
		}
	}
	if (menu_was_open && host_menu == HOST_MENU_NONE) host_consume_game_input = 1;

	if (!TAS && host_menu == HOST_MENU_NONE) {
		if (kbstate[SDLK_LEFT])  buttons_state |= (1<<0);
		if (kbstate[SDLK_RIGHT]) buttons_state |= (1<<1);
		if (kbstate[SDLK_UP])    buttons_state |= (1<<2);
		if (kbstate[SDLK_DOWN])  buttons_state |= (1<<3);
		if (kbstate[SDLK_z] || kbstate[SDLK_c] || kbstate[SDLK_n]) buttons_state |= (1<<4);
		if (kbstate[SDLK_x] || kbstate[SDLK_v] || kbstate[SDLK_m]) buttons_state |= (1<<5);
	} else if (TAS && !paused) {
		static int t = 0;
		t++;
		if (t==1) buttons_state = 1<<4;
		else if (t > 80) {
			int btn;
			fscanf(TAS, "%d,", &btn);
			buttons_state = btn;
		} else buttons_state = 0;
	}
	if (host_consume_game_input) {
		if (buttons_state & 0x3f) buttons_state &= (Uint16)~0x3f;
		else host_consume_game_input = 0;
	}

	if (paused) {
#if HOST_PANEL_W == 0
		const int x0 = PICO8_W/2-3*4, y0 = 8;

		p8_rectfill(x0-1,y0-1, 6*4+x0+1,6+y0+1, 6);
		p8_rectfill(x0,y0, 6*4+x0,6+y0, 0);
		p8_print("paused", x0+1, y0+1, 7);
#endif
	} else if (host_has_focus) {
		Celeste_P8_update();
		Celeste_P8_draw();
	}
	HostUpdateStats();
	OSDdraw();
	HostDrawTimer();
	HostDrawMenu();

	SDL_Flip(screen);

#ifdef EMSCRIPTEN //emscripten_set_main_loop already sets the fps
	SDL_Delay(1);
#elif defined(_3DS)
	gspWaitForVBlank(), gspWaitForVBlank();
#else
	static int t = 0;
	static unsigned frame_start = 0;
	unsigned frame_end = SDL_GetTicks();
	unsigned frame_time = frame_end-frame_start;
	unsigned target_millis;
	// frame timing for 30fps is 33.333... ms, but we only have integer granularity
	// so alternate between 33 and 34 ms, like [33,33,34,33,33,34,...] which averages out to 33.333...
	if (t < 2) target_millis = 33;
	else       target_millis = 34;

	if (++t == 3) t = 0;

	if (frame_time < target_millis) {
		SDL_Delay(target_millis - frame_time);
	}
	frame_start = SDL_GetTicks();
#endif
}

static int gettileflag(int, int);
static void p8_line(int,int,int,int,unsigned char);

//lots of code from https://github.com/SDL-mirror/SDL/blob/bc59d0d4a2c814900a506d097a381077b9310509/src/video/SDL_surface.c#L625
//coordinates should be scaled already
static inline void Xblit(SDL_Surface* src, SDL_Rect* srcrect, SDL_Surface* dst, SDL_Rect* dstrect, int color, int flipx, int flipy) {
	assert(src && dst && !src->locked && !dst->locked);
	assert(dst->format->BitsPerPixel == 32 && src->format->BitsPerPixel == 8);
	SDL_Rect fulldst;
	/* If the destination rectangle is NULL, use the entire dest surface */
	if (!dstrect)
		dstrect = (fulldst = (SDL_Rect){0,0,dst->w,dst->h}, &fulldst);

	int srcx, srcy, w, h;
	
	/* clip the source rectangle to the source surface */
	if (srcrect) {
		int maxw, maxh;

		srcx = srcrect->x;
		w = srcrect->w;
		if (srcx < 0) {
			w += srcx;
			dstrect->x -= srcx;
			srcx = 0;
		}
		maxw = src->w - srcx;
		if (maxw < w)
			w = maxw;

		srcy = srcrect->y;
		h = srcrect->h;
		if (srcy < 0) {
			h += srcy;
			dstrect->y -= srcy;
			srcy = 0;
		}
		maxh = src->h - srcy;
		if (maxh < h)
			h = maxh;

	} else {
		srcx = srcy = 0;
		w = src->w;
		h = src->h;
	}

	/* clip the destination rectangle against the clip rectangle */
	{
		SDL_Rect *clip = &dst->clip_rect;
		int dx, dy;

		dx = clip->x - dstrect->x;
		if (dx > 0) {
			w -= dx;
			dstrect->x += dx;
			srcx += dx;
		}
		dx = dstrect->x + w - clip->x - clip->w;
		if (dx > 0)
			w -= dx;

		dy = clip->y - dstrect->y;
		if (dy > 0) {
			h -= dy;
			dstrect->y += dy;
			srcy += dy;
		}
		dy = dstrect->y + h - clip->y - clip->h;
		if (dy > 0)
			h -= dy;
	}

	if (w && h) {
		unsigned char* srcpix = src->pixels;
		int srcpitch = src->pitch;
		Uint32* dstpix = dst->pixels;
    #define _blitter(dp, xflip) do                                                                  \
    for (int y = 0; y < h; y++) for (int x = 0; x < w; x++) {                                       \
      unsigned char p = srcpix[!xflip ? srcx+x+(srcy+y)*srcpitch : srcx+(w-x-1)+(srcy+y)*srcpitch]; \
      if (p) dstpix[dstrect->x+x + (dstrect->y+y)*dst->w] = getcolor(dp);                           \
    } while(0)
		if (color && flipx) _blitter(color, 1);
		else if (!color && flipx) _blitter(p, 1);
		else if (color && !flipx) _blitter(color, 0);
		else if (!color && !flipx) _blitter(p, 0);
		#undef _blitter
	}
}

static void p8_rectfill(int x0, int y0, int x1, int y1, int col) {
	int w = (x1 - x0 + 1)*scale;
	int h = (y1 - y0 + 1)*scale;
	if (w > 0 && h > 0) {
		SDL_Rect rc = {x0*scale,y0*scale, w,h};
		SDL_FillRect(screen, &rc, getcolor(col));
	}
}

static void p8_print(const char* str, int x, int y, int col) {
	for (char c = *str; c; c = *(++str)) {
		c &= 0x7F;
		SDL_Rect srcrc = {8*(c%16), 8*(c/16)};
		srcrc.x *= scale;
		srcrc.y *= scale;
		srcrc.w = srcrc.h = 8*scale;
		
		SDL_Rect dstrc = {x*scale, y*scale, scale, scale};
		Xblit(font, &srcrc, screen, &dstrc, col, 0,0);
		x += 4;
	}
}

int pico8emu(CELESTE_P8_CALLBACK_TYPE call, ...) {
	static int camera_x = 0, camera_y = 0;
	if (!enable_screenshake) {
		camera_x = camera_y = 0;
	}

	va_list args;
	int ret = 0;
	va_start(args, call);
	
	#define   INT_ARG() va_arg(args, int)
	#define  BOOL_ARG() (Celeste_P8_bool_t)va_arg(args, int)
	#define RET_INT(_i)   do {ret = (_i); goto end;} while (0)
	#define RET_BOOL(_b) RET_INT(!!(_b))

	switch (call) {
		case CELESTE_P8_MUSIC: { //music(idx,fade,mask)
			int index = INT_ARG();
			int fade = INT_ARG();
			int mask = INT_ARG();

			(void)mask; //we do not care about this since sdl mixer keeps sounds and music separate
			
			if (index == -1) { //stop playing
				Mix_FadeOutMusic(fade);
				current_music = NULL;
			} else if (mus[index/10]) {
				Mix_Music* musi = mus[index/10];
				current_music = musi;
				Mix_FadeInMusic(musi, -1, fade);
			}
		} break;
		case CELESTE_P8_SPR: { //spr(sprite,x,y,cols,rows,flipx,flipy)
			int sprite = INT_ARG();
			int x = INT_ARG();
			int y = INT_ARG();
			int cols = INT_ARG();
			int rows = INT_ARG();
			int flipx = BOOL_ARG();
			int flipy = BOOL_ARG();

			(void)cols;
			(void)rows;

			assert(rows == 1 && cols == 1);

			if (sprite >= 0) {
				SDL_Rect srcrc = {
					8*(sprite % 16),
					8*(sprite / 16)
				};
				srcrc.x *= scale;
				srcrc.y *= scale;
				srcrc.w = srcrc.h = scale*8;
				SDL_Rect dstrc = {
					(x - camera_x)*scale, (y - camera_y)*scale,
					scale, scale
				};
				Xblit(gfx, &srcrc, screen, &dstrc, 0,flipx,flipy);
			}
		} break;
		case CELESTE_P8_BTN: { //btn(b)
			int b = INT_ARG();
			assert(b >= 0 && b <= 5); 
			RET_BOOL(buttons_state & (1 << b));
		} break;
		case CELESTE_P8_SFX: { //sfx(id)
			int id = INT_ARG();
		
			if (id < (sizeof snd) / (sizeof*snd) && snd[id])
				Mix_PlayChannel(-1, snd[id], 0);
		} break;
		case CELESTE_P8_PAL: { //pal(a,b)
			int a = INT_ARG();
			int b = INT_ARG();
			if (a >= 0 && a < 16 && b >= 0 && b < 16) {
				//swap palette colors
				palette[a] = base_palette[b];
			}
		} break;
		case CELESTE_P8_PAL_RESET: { //pal()
			ResetPalette();
		} break;
		case CELESTE_P8_CIRCFILL: { //circfill(x,y,r,col)
			int cx = INT_ARG() - camera_x;
			int cy = INT_ARG() - camera_y;
			int r = INT_ARG();
			int col = INT_ARG();

			int realcolor = getcolor(col);

			if (r <= 1) {
				SDL_FillRect(screen, &(SDL_Rect){scale*(cx-1), scale*cy, scale*3, scale}, realcolor);
				SDL_FillRect(screen, &(SDL_Rect){scale*cx, scale*(cy-1), scale, scale*3}, realcolor);
			} else if (r <= 2) {
				SDL_FillRect(screen, &(SDL_Rect){scale*(cx-2), scale*(cy-1), scale*5, scale*3}, realcolor);
				SDL_FillRect(screen, &(SDL_Rect){scale*(cx-1), scale*(cy-2), scale*3, scale*5}, realcolor);
			} else if (r <= 3) {
				SDL_FillRect(screen, &(SDL_Rect){scale*(cx-3), scale*(cy-1), scale*7, scale*3}, realcolor);
				SDL_FillRect(screen, &(SDL_Rect){scale*(cx-1), scale*(cy-3), scale*3, scale*7}, realcolor);
				SDL_FillRect(screen, &(SDL_Rect){scale*(cx-2), scale*(cy-2), scale*5, scale*5}, realcolor);
			} else { //i dont think the game uses this
				int f = 1 - r; //used to track the progress of the drawn circle (since its semi-recursive)
				int ddFx = 1; //step x
				int ddFy = -2 * r; //step y
				int x = 0;
				int y = r;

				//this algorithm doesn't account for the diameters
				//so we have to set them manually
				p8_line(cx,cy-y, cx,cy+r, col);
				p8_line(cx+r,cy, cx-r,cy, col);

				while (x < y) {
					if (f >= 0) {
						y--;
						ddFy += 2;
						f += ddFy;
					}
					x++;
					ddFx += 2;
					f += ddFx;

					//build our current arc
					p8_line(cx+x,cy+y, cx-x,cy+y, col);
					p8_line(cx+x,cy-y, cx-x,cy-y, col);
					p8_line(cx+y,cy+x, cx-y,cy+x, col);
					p8_line(cx+y,cy-x, cx-y,cy-x, col);
				}
			}
		} break;
		case CELESTE_P8_PRINT: { //print(str,x,y,col)
			const char* str = va_arg(args, const char*);
			int x = INT_ARG() - camera_x;
			int y = INT_ARG() - camera_y;
			int col = INT_ARG() % 16;

#ifdef _3DS
			if (!strcmp(str, "x+c")) {
				//this is confusing, as 3DS uses a+b button, so use this hack to make it more appropiate
				str = "a+b";
			}
#endif

			p8_print(str,x,y,col);
		} break;
		case CELESTE_P8_RECTFILL: { //rectfill(x0,y0,x1,y1,col)
			int x0 = INT_ARG() - camera_x;
			int y0 = INT_ARG() - camera_y;
			int x1 = INT_ARG() - camera_x;
			int y1 = INT_ARG() - camera_y;
			int col = INT_ARG();

			p8_rectfill(x0,y0,x1,y1,col);
		} break;
		case CELESTE_P8_LINE: { //line(x0,y0,x1,y1,col)
			int x0 = INT_ARG() - camera_x;
			int y0 = INT_ARG() - camera_y;
			int x1 = INT_ARG() - camera_x;
			int y1 = INT_ARG() - camera_y;
			int col = INT_ARG();

			p8_line(x0,y0,x1,y1,col);
		} break;
		case CELESTE_P8_MGET: { //mget(tx,ty)
			int tx = INT_ARG();
			int ty = INT_ARG();

			RET_INT(tilemap_data[tx+ty*128]);
		} break;
		case CELESTE_P8_CAMERA: { //camera(x,y)
			if (enable_screenshake) {
				camera_x = INT_ARG();
				camera_y = INT_ARG();
			}
		} break;
		case CELESTE_P8_FGET: { //fget(tile,flag)
			int tile = INT_ARG();
			int flag = INT_ARG();

			RET_INT(gettileflag(tile, flag));
		} break;
		case CELESTE_P8_MAP: { //map(mx,my,tx,ty,mw,mh,mask)
			int mx = INT_ARG(), my = INT_ARG();
			int tx = INT_ARG(), ty = INT_ARG();
			int mw = INT_ARG(), mh = INT_ARG();
			int mask = INT_ARG();
			
			for (int x = 0; x < mw; x++) {
				for (int y = 0; y < mh; y++) {
					int tile = tilemap_data[x + mx + (y + my)*128];
					//hack
					if (mask == 0 || (mask == 4 && tile_flags[tile] == 4) || gettileflag(tile, mask != 4 ? mask-1 : mask)) {
						SDL_Rect srcrc = {
							8*(tile % 16),
							8*(tile / 16)
						};
						srcrc.x *= scale;
						srcrc.y *= scale;
						srcrc.w = srcrc.h = scale*8;
						SDL_Rect dstrc = {
							(tx+x*8 - camera_x)*scale, (ty+y*8 - camera_y)*scale,
							scale*8, scale*8
						};

						if (0) {
							srcrc.x = srcrc.y = 0;
							srcrc.w = srcrc.h = 8;
							dstrc.x = x*8, dstrc.y = y*8;
							dstrc.w = dstrc.h = 8;
						}

						Xblit(gfx, &srcrc, screen, &dstrc, 0, 0, 0);
					}
				}
			}
		} break;
	}

	end:
	va_end(args);
	return ret;
}

static int gettileflag(int tile, int flag) {
	return tile < sizeof(tile_flags)/sizeof(*tile_flags) && (tile_flags[tile] & (1 << flag)) != 0;
}

//coordinates should NOT be scaled before calling this
static void p8_line(int x0, int y0, int x1, int y1, unsigned char color) {
	#define CLAMP(v,min,max) v = v < min ? min : v >= max ? max-1 : v;
	CLAMP(x0,0,screen->w);
	CLAMP(y0,0,screen->h);
	CLAMP(x1,0,screen->w);
	CLAMP(y1,0,screen->h);

	Uint32 realcolor = getcolor(color);

	#undef CLAMP
  #define PLOT(x,y) do {                                                        \
     SDL_FillRect(screen, &(SDL_Rect){x*scale,y*scale,scale,scale}, realcolor); \
	} while (0)
	int sx, sy, dx, dy, err, e2;
	dx = abs(x1 - x0);
	dy = abs(y1 - y0);
	if (!dx && !dy) return;

	if (x0 < x1) sx = 1; else sx = -1;
	if (y0 < y1) sy = 1; else sy = -1;
	err = dx - dy;
	if (!dy && !dx) return;
	else if (!dx) { //vertical line
		for (int y = y0; y != y1; y += sy) PLOT(x0,y);
	} else if (!dy) { //horizontal line
		for (int x = x0; x != x1; x += sx) PLOT(x,y0);
	} while (x0 != x1 || y0 != y1) {
		PLOT(x0, y0);
		e2 = 2 * err;
		if (e2 > -dy) {
			err -= dy;
			x0 += sx;
		}
		if (e2 < dx) {
			err += dx;
			y0 += sy;
		}
	}
	#undef PLOT
}

#if SDL_MAJOR_VERSION >= 2
//SDL2: read input from connected gamepad

struct mapping {
	SDL_GameControllerButton sdl_btn;
	Uint16 pico8_btn;
};
static const char* pico8_btn_names[] = {
	"left", "right", "up", "down", "jump", "dash", "save", "load", "exit", "pause"
};

// initialized with default mapping
static struct mapping controller_mappings[30] = {
	{SDL_CONTROLLER_BUTTON_DPAD_LEFT,  0}, //left
	{SDL_CONTROLLER_BUTTON_DPAD_RIGHT, 1}, //right
	{SDL_CONTROLLER_BUTTON_DPAD_UP,    2}, //up
	{SDL_CONTROLLER_BUTTON_DPAD_DOWN,  3}, //down
	{SDL_CONTROLLER_BUTTON_A,          4}, //jump
	{SDL_CONTROLLER_BUTTON_B,          5}, //dash

	{SDL_CONTROLLER_BUTTON_LEFTSHOULDER,  PSEUDO_BTN_SAVE_STATE}, //save
	{SDL_CONTROLLER_BUTTON_RIGHTSHOULDER, PSEUDO_BTN_LOAD_STATE}, //load
	{SDL_CONTROLLER_BUTTON_GUIDE,         PSEUDO_BTN_EXIT}, //exit
	{SDL_CONTROLLER_BUTTON_START,         PSEUDO_BTN_PAUSE}, //pause
	{0xff, 0xff}
};
static const Uint16 stick_deadzone = 32767 / 2; //about half

static void ReadGamepadInput(Uint16* out_buttons) {
	static _Bool read_config = 0;
	if (!read_config) {
		read_config = 1;
		const char* cfg_file_path = getenv("CCLESTE_INPUT_CFG_PATH");
		if (!cfg_file_path) {
			cfg_file_path  = "ccleste-input-cfg.txt";
		}
		FILE* cfg = fopen(cfg_file_path, "r");
		if (cfg) {
			int i;
			for (i = 0; i < sizeof controller_mappings / sizeof *controller_mappings - 1;) {
				char line[150], p8btn[31], sdlbtn[31];
				fgets(line, sizeof line - 1, cfg);
				if (feof(cfg) || ferror(cfg)) break;
				line[sizeof line - 1] = 0;
				if (*line == '#') {
					//comment
				} else if (sscanf(line, "%30s %30s", p8btn, sdlbtn) == 2) {
					p8btn[sizeof p8btn - 1] = sdlbtn[sizeof sdlbtn - 1] = 0;
					for (int btn = 0; btn < sizeof pico8_btn_names / sizeof *pico8_btn_names; btn++) {
						if (!SDL_strcasecmp(pico8_btn_names[btn], p8btn)) {
							printf("input cfg: %s -> %s\n", p8btn, sdlbtn);
							controller_mappings[i].sdl_btn = SDL_GameControllerGetButtonFromString(sdlbtn);
							controller_mappings[i].pico8_btn = btn;
							i++;
						}
					}
				}
			}
			controller_mappings[i].pico8_btn = 0xFF;
			fclose(cfg);
		} else {
			cfg = fopen(cfg_file_path, "w");
			if (cfg) {
				printf("creating ccleste-input-cfg.txt with default button mappings\n");
				fprintf(cfg, "# in-game \tcontroller\n");
				for (struct mapping* mapping = controller_mappings; mapping->pico8_btn != 0xFF; mapping++) {
					fprintf(cfg, "%s \t%s\n", pico8_btn_names[mapping->pico8_btn], SDL_GameControllerGetStringForButton(mapping->sdl_btn));
				}
				fclose(cfg);
			}
		}
	}

	static SDL_GameController* controller = NULL;
	if (!controller) {
		static int tries_left = 30;
		if (!tries_left) return;
		tries_left--;

		//use first available controller
		int count = SDL_NumJoysticks();
		printf("sdl reports %i controllers\n", count);
		for (int i = 0; i < count; i++) {
			if (SDL_IsGameController(i)) {
				controller = SDL_GameControllerOpen(i);
				if (!controller) {
					fprintf(stderr, "error opening controller: %s\n", SDL_GetError());
					return;
				}
				printf("picked controller: '%s'\n", SDL_GameControllerName(controller));
				break;
			}
		}
	}

	//pico 8 buttons and pseudo buttons
	for (int i = 0; i < sizeof controller_mappings / sizeof *controller_mappings; i++) {
		struct mapping mapping = controller_mappings[i];
		if (mapping.pico8_btn == 0xFF) break;
		_Bool pressed = SDL_GameControllerGetButton(controller, mapping.sdl_btn);
		Uint16 mask = ~(1 << mapping.pico8_btn);
		*out_buttons = (*out_buttons & mask) | (pressed << mapping.pico8_btn);
	}

	//joystick -> dpad input
	Sint16 x_axis = SDL_GameControllerGetAxis(controller, SDL_CONTROLLER_AXIS_LEFTX);
	Sint16 y_axis = SDL_GameControllerGetAxis(controller, SDL_CONTROLLER_AXIS_LEFTY);
	if (x_axis < -stick_deadzone) *out_buttons |= (1 << 0); //left
	if (x_axis >  stick_deadzone) *out_buttons |= (1 << 1); //right
	if (y_axis < -stick_deadzone) *out_buttons |= (1 << 2); //up
	if (y_axis >  stick_deadzone) *out_buttons |= (1 << 3); //down
}
#endif


// vim: ts=2 sw=2 noexpandtab
