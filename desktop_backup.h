#ifndef CELESTE_DESKTOP_BACKUP_H
#define CELESTE_DESKTOP_BACKUP_H

#include <errno.h>
#include <stdio.h>
#include <string.h>
#ifdef _WIN32
#include <io.h>
#include <windows.h>
#else
#include <unistd.h>
#endif

#ifdef _WIN32
static int DesktopWidePath(const char* path, WCHAR* output, int output_count) {
	return MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path, -1,
		output, output_count) != 0;
}
#endif

static FILE* DesktopOpenFile(const char* path, const char* mode) {
#ifdef _WIN32
	WCHAR wide_path[4096];
	WCHAR wide_mode[8];
	if (!DesktopWidePath(path, wide_path, (int)(sizeof wide_path / sizeof *wide_path))) return NULL;
	if (!MultiByteToWideChar(CP_UTF8, 0, mode, -1,
		wide_mode, (int)(sizeof wide_mode / sizeof *wide_mode))) return NULL;
	return _wfopen(wide_path, wide_mode);
#else
	return fopen(path, mode);
#endif
}

static int DesktopFlushFile(FILE* file) {
	if (fflush(file) != 0) return 0;
#ifdef _WIN32
	return _commit(_fileno(file)) == 0;
#else
	return fsync(fileno(file)) == 0;
#endif
}

static int DesktopRemoveFile(const char* path) {
#ifdef _WIN32
	WCHAR wide_path[4096];
	return DesktopWidePath(path, wide_path, (int)(sizeof wide_path / sizeof *wide_path))
		&& DeleteFileW(wide_path) != 0;
#else
	return unlink(path) == 0;
#endif
}

static int DesktopInstallNoReplace(const char* temporary_path, const char* backup_path) {
#ifdef _WIN32
	WCHAR wide_temporary[4096];
	WCHAR wide_backup[4096];
	if (!DesktopWidePath(temporary_path, wide_temporary,
		(int)(sizeof wide_temporary / sizeof *wide_temporary))) return 0;
	if (!DesktopWidePath(backup_path, wide_backup,
		(int)(sizeof wide_backup / sizeof *wide_backup))) return 0;
	return MoveFileExW(wide_temporary, wide_backup, MOVEFILE_WRITE_THROUGH) != 0;
#else
	return link(temporary_path, backup_path) == 0;
#endif
}

static int DesktopFilesEqual(const char* first_path, const char* second_path) {
	FILE* first = DesktopOpenFile(first_path, "rb");
	FILE* second = DesktopOpenFile(second_path, "rb");
	if (!first || !second) {
		if (first) fclose(first);
		if (second) fclose(second);
		return 0;
	}
	unsigned char first_buffer[4096], second_buffer[4096];
	int equal = 1;
	for (;;) {
		size_t first_count = fread(first_buffer, 1, sizeof first_buffer, first);
		size_t second_count = fread(second_buffer, 1, sizeof second_buffer, second);
		if (first_count != second_count
		 || memcmp(first_buffer, second_buffer, first_count) != 0) {
			equal = 0;
			break;
		}
		if (first_count < sizeof first_buffer) {
			if (ferror(first) || ferror(second)) equal = 0;
			break;
		}
	}
	int first_close = fclose(first);
	int second_close = fclose(second);
	if (first_close != 0 || second_close != 0) equal = 0;
	return equal;
}

static int DesktopCopyFile(const char* source_path, const char* destination_path) {
	FILE* source = DesktopOpenFile(source_path, "rb");
	FILE* destination = DesktopOpenFile(destination_path, "wb");
	if (!source || !destination) {
		if (source) fclose(source);
		if (destination) fclose(destination);
		return 0;
	}
	unsigned char buffer[4096];
	int copied = 1;
	for (;;) {
		size_t count = fread(buffer, 1, sizeof buffer, source);
		if (count && fwrite(buffer, 1, count, destination) != count) copied = 0;
		if (!copied || count < sizeof buffer) {
			if (ferror(source)) copied = 0;
			break;
		}
	}
	if (copied) copied = DesktopFlushFile(destination);
	int source_close = fclose(source);
	int destination_close = fclose(destination);
	if (source_close != 0 || destination_close != 0) copied = 0;
	return copied;
}

static int DesktopPreserveBackup(
	const char* source_path,
	const char* backup_path,
	const char* temporary_path
) {
	FILE* existing = DesktopOpenFile(backup_path, "rb");
	if (existing) {
		fclose(existing);
		return DesktopFilesEqual(source_path, backup_path);
	}
	if (!DesktopCopyFile(source_path, temporary_path)) {
		DesktopRemoveFile(temporary_path);
		return 0;
	}
	int installed = DesktopInstallNoReplace(temporary_path, backup_path);
	DesktopRemoveFile(temporary_path);
	if (!installed && !DesktopFilesEqual(source_path, backup_path)) return 0;
	return DesktopFilesEqual(source_path, backup_path);
}

#endif
