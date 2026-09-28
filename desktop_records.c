#include "desktop_records.h"

#include <SDL.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <io.h>
#include <process.h>
#include <shellapi.h>
#include <windows.h>
#define ACCESS _access
#define EXECUTABLE_ACCESS 0
#define PATH_SEPARATOR '\\'
#define PATH_LIST_SEPARATOR ';'
#else
#include <fcntl.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#define ACCESS access
#define EXECUTABLE_ACCESS X_OK
#define PATH_SEPARATOR '/'
#define PATH_LIST_SEPARATOR ':'
#endif

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

typedef struct DesktopRecordManifest {
	const char* runner;
	const char* time_label;
	const char* url;
	const char* filename;
	double video_end;
	double room_starts[DESKTOP_RECORD_ROOM_COUNT];
} DesktopRecordManifest;

static const DesktopRecordManifest records[] = {
	{
		"Associatedfeetballer",
		"1:37.60",
		"https://youtube.com/watch?v=7yhApvCUDF8",
		"any.mp4",
		105.882993,
		{
			2.300000, 5.433333, 8.733333, 11.933333, 14.933333,
			17.466667, 20.166667, 22.466667, 25.733333, 29.066667,
			32.233333, 34.966667, 37.300000, 40.733333, 44.333333,
			47.366667, 50.333333, 53.133333, 56.733333, 59.200000,
			63.733333, 66.133333, 71.166667, 73.666667, 76.766667,
			80.166667, 84.766667, 88.466667, 94.300000, 97.200000,
			99.933333
		}
	},
	{
		"LordSNEK",
		"2:00.77",
		"https://youtube.com/watch?v=gbbnHZSVDuM",
		"all-berries.mp4",
		135.790295,
		{
			6.473133, 10.710700, 14.014011, 18.451778, 21.421411,
			26.960289, 29.529522, 32.999656, 36.236233, 40.440433,
			43.610267, 46.413078, 49.149144, 54.754744, 59.292622,
			63.363356, 66.332989, 70.236900, 73.807133, 78.378378,
			83.016344, 85.418744, 90.457122, 93.626956, 96.730056,
			101.801800, 107.774433, 111.478144, 118.718711, 123.323322,
			127.427422
		}
	}
};

static char record_directory[PATH_MAX];
static char record_paths[2][PATH_MAX];
#ifdef _WIN32
static intptr_t practice_process = -1;
#else
static pid_t practice_process = -1;
#endif

static int valid_category(int category) {
	return category == DESKTOP_RECORD_ANY_PERCENT ||
		category == DESKTOP_RECORD_ALL_STRAWBERRIES;
}

static int executable_exists(const char* executable) {
	const char* path;
	const char* cursor;
	char candidate[PATH_MAX];
	size_t name_length;
	if (!executable || !*executable) return 0;
	if (strchr(executable, PATH_SEPARATOR)
#ifdef _WIN32
		|| strchr(executable, '/')
#endif
	)
		return ACCESS(executable, EXECUTABLE_ACCESS) == 0;
	path = getenv("PATH");
	if (!path) return 0;
	name_length = strlen(executable);
	for (cursor = path; *cursor;) {
		const char* end = strchr(cursor, PATH_LIST_SEPARATOR);
		size_t directory_length = end ? (size_t)(end - cursor) : strlen(cursor);
		if (directory_length == 0) {
			candidate[0] = '.';
			directory_length = 1;
		} else if (directory_length < sizeof candidate) {
			memcpy(candidate, cursor, directory_length);
		}
		if (directory_length + name_length + 2 <= sizeof candidate) {
			candidate[directory_length] = PATH_SEPARATOR;
			memcpy(candidate + directory_length + 1, executable, name_length + 1);
			if (ACCESS(candidate, EXECUTABLE_ACCESS) == 0) return 1;
#ifdef _WIN32
			if (!strchr(executable, '.') && directory_length + name_length + 6 <= sizeof candidate) {
				memcpy(candidate + directory_length + name_length + 1, ".exe", 5);
				if (ACCESS(candidate, EXECUTABLE_ACCESS) == 0) return 1;
			}
#endif
		}
		if (!end) break;
		cursor = end + 1;
	}
	return 0;
}

static const char* mpv_executable(void) {
	const char* configured = getenv("CCLESTE_MPV_PATH");
	if (configured && *configured && executable_exists(configured)) return configured;
	return executable_exists("mpv") ? "mpv" : NULL;
}

static int local_record_exists(int category) {
	return valid_category(category) && record_paths[category][0] &&
		ACCESS(record_paths[category], 4) == 0;
}

static int launch_process(const char* executable, const char* const arguments[], int practice) {
#ifdef _WIN32
	intptr_t process = _spawnvp(_P_NOWAIT, executable, arguments);
	if (process == -1) return 0;
	if (practice)
		practice_process = process;
	else
		CloseHandle((HANDLE)process);
	return 1;
#else
	int error_pipe[2];
	int child_error = 0;
	ssize_t error_bytes;
	if (pipe(error_pipe) != 0) return 0;
	if (fcntl(error_pipe[1], F_SETFD, FD_CLOEXEC) == -1) {
		close(error_pipe[0]);
		close(error_pipe[1]);
		return 0;
	}
	pid_t process = fork();
	if (process < 0) {
		close(error_pipe[0]);
		close(error_pipe[1]);
		return 0;
	}
	if (process == 0) {
		close(error_pipe[0]);
		if (!practice) {
			pid_t detached = fork();
			if (detached < 0) {
				child_error = errno;
				(void)write(error_pipe[1], &child_error, sizeof child_error);
				_exit(127);
			}
			if (detached > 0) _exit(0);
		} else {
			(void)setpgid(0, 0);
		}
		execvp(executable, (char* const*)arguments);
		child_error = errno;
		(void)write(error_pipe[1], &child_error, sizeof child_error);
		_exit(127);
	}
	close(error_pipe[1]);
	if (!practice) waitpid(process, NULL, 0);
	do {
		error_bytes = read(error_pipe[0], &child_error, sizeof child_error);
	} while (error_bytes < 0 && errno == EINTR);
	close(error_pipe[0]);
	if (error_bytes > 0) {
		if (practice) waitpid(process, NULL, 0);
		return 0;
	}
	if (practice) practice_process = process;
	return 1;
#endif
}

static int open_url(const char* url) {
#if SDL_MAJOR_VERSION >= 2 && SDL_VERSION_ATLEAST(2, 0, 14)
	return SDL_OpenURL(url) == 0;
#else
#ifdef _WIN32
	return (INT_PTR)ShellExecuteA(NULL, "open", url, NULL, NULL, SW_SHOWNORMAL) > 32;
#elif defined(__APPLE__)
	const char* arguments[3];
	arguments[0] = "open";
#else
	const char* arguments[3];
	arguments[0] = "xdg-open";
#endif
#ifndef _WIN32
	arguments[1] = url;
	arguments[2] = NULL;
	return executable_exists(arguments[0]) && launch_process(arguments[0], arguments, 0);
#endif
#endif
}

static int build_record_path(char* output, size_t output_size,
	const char* directory, const char* filename) {
	size_t length;
	int written;
	if (!directory || !*directory) return 0;
	length = strlen(directory);
	written = snprintf(output, output_size, "%s%s%s", directory,
		directory[length - 1] == '/' || directory[length - 1] == '\\' ? "" :
#ifdef _WIN32
		"\\",
#else
		"/",
#endif
		filename);
	if (written < 0 || (size_t)written >= output_size) {
		output[0] = '\0';
		return 0;
	}
	return 1;
}

void DesktopRecordInit(void) {
	const char* configured = getenv("CCLESTE_RECORD_DIR");
	record_directory[0] = '\0';
	if (configured && *configured) {
		int written = snprintf(record_directory, sizeof record_directory, "%s", configured);
		if (written < 0 || (size_t)written >= sizeof record_directory)
			record_directory[0] = '\0';
	} else {
#if SDL_MAJOR_VERSION >= 2
		char* preference_path = SDL_GetPrefPath("celeste-classic", "celeste-classic");
		if (preference_path) {
			int written = snprintf(record_directory, sizeof record_directory,
				"%srecords", preference_path);
			if (written < 0 || (size_t)written >= sizeof record_directory)
				record_directory[0] = '\0';
			SDL_free(preference_path);
		}
#endif
	}
	for (int category = 0; category < 2; category++) {
		if (!record_directory[0]) {
			record_paths[category][0] = '\0';
			continue;
		}
		(void)build_record_path(record_paths[category], sizeof record_paths[category],
			record_directory, records[category].filename);
	}
}

void DesktopRecordStopPractice(void) {
#ifdef _WIN32
	if (practice_process != -1) {
		HANDLE process = (HANDLE)practice_process;
		if (WaitForSingleObject(process, 0) == WAIT_TIMEOUT)
			TerminateProcess(process, 0);
		WaitForSingleObject(process, INFINITE);
		CloseHandle(process);
		practice_process = -1;
	}
#else
	if (practice_process > 0) {
		pid_t result = waitpid(practice_process, NULL, WNOHANG);
		if (result == 0) {
			if (kill(-practice_process, SIGTERM) != 0)
				kill(practice_process, SIGTERM);
			for (int attempt = 0; attempt < 50; attempt++) {
				result = waitpid(practice_process, NULL, WNOHANG);
				if (result != 0) break;
				SDL_Delay(10);
			}
			if (result == 0) {
				if (kill(-practice_process, SIGKILL) != 0)
					kill(practice_process, SIGKILL);
				waitpid(practice_process, NULL, 0);
			}
		}
		practice_process = -1;
	}
#endif
}

int DesktopRecordStartPractice(int category, int room, int allow_web_fallback) {
	const char* mpv;
	char start[48], loop_start[48], loop_end[48], url[256];
	const char* arguments[12];
	if (!valid_category(category) || room < 0 || room >= DESKTOP_RECORD_ROOM_COUNT)
		return 0;
	DesktopRecordStopPractice();
	mpv = mpv_executable();
	if (mpv && local_record_exists(category)) {
		snprintf(start, sizeof start, "--start=%.6f", DesktopRecordRoomStart(category, room));
		snprintf(loop_start, sizeof loop_start, "--ab-loop-a=%.6f", DesktopRecordRoomStart(category, room));
		snprintf(loop_end, sizeof loop_end, "--ab-loop-b=%.6f", DesktopRecordRoomEnd(category, room));
		arguments[0] = mpv;
		arguments[1] = "--no-terminal";
		arguments[2] = "--force-window=yes";
		arguments[3] = "--keep-open=no";
		arguments[4] = "--autofit=50%x100%";
		arguments[5] = "--geometry=100%:0%";
		arguments[6] = "--title=Celeste Classic World Record Practice";
		arguments[7] = start;
		arguments[8] = loop_start;
		arguments[9] = loop_end;
		arguments[10] = record_paths[category];
		arguments[11] = NULL;
		if (launch_process(mpv, arguments, 1)) return 1;
	}
	if (!allow_web_fallback) return 0;
	if (!DesktopRecordBuildRoomURL(category, room, url, sizeof url)) return 0;
	return open_url(url);
}

int DesktopRecordWatch(int category) {
	const char* mpv;
	const char* arguments[8];
	if (!valid_category(category)) return 0;
	mpv = mpv_executable();
	if (mpv && local_record_exists(category)) {
		arguments[0] = mpv;
		arguments[1] = "--no-terminal";
		arguments[2] = "--force-window=yes";
		arguments[3] = "--keep-open=no";
		arguments[4] = "--title=Celeste Classic World Record";
		arguments[5] = record_paths[category];
		arguments[6] = NULL;
		if (launch_process(mpv, arguments, 0)) return 1;
	}
	return open_url(records[category].url);
}

const char* DesktopRecordURL(int category) {
	return valid_category(category) ? records[category].url : NULL;
}

const char* DesktopRecordRunner(int category) {
	return valid_category(category) ? records[category].runner : NULL;
}

const char* DesktopRecordTimeLabel(int category) {
	return valid_category(category) ? records[category].time_label : NULL;
}

const char* DesktopRecordLocalPath(int category) {
	return valid_category(category) ? record_paths[category] : NULL;
}

double DesktopRecordRoomStart(int category, int room) {
	if (!valid_category(category) || room < 0 || room >= DESKTOP_RECORD_ROOM_COUNT)
		return -1.0;
	return records[category].room_starts[room];
}

double DesktopRecordRoomEnd(int category, int room) {
	if (!valid_category(category) || room < 0 || room >= DESKTOP_RECORD_ROOM_COUNT)
		return -1.0;
	if (room + 1 < DESKTOP_RECORD_ROOM_COUNT)
		return records[category].room_starts[room + 1];
	return records[category].video_end;
}

int DesktopRecordBuildRoomURL(int category, int room, char* output, size_t output_size) {
	int written;
	if (!output || output_size == 0 || !valid_category(category) ||
		room < 0 || room >= DESKTOP_RECORD_ROOM_COUNT)
		return 0;
	written = snprintf(output, output_size, "%s&t=%.3fs", records[category].url,
		records[category].room_starts[room]);
	return written >= 0 && (size_t)written < output_size;
}
