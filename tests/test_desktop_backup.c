#include "../desktop_backup.h"
#include <assert.h>
#include <stdlib.h>
#ifdef _WIN32
#include <process.h>
#define getpid _getpid
#endif

static void write_bytes(const char* path, const unsigned char* bytes, size_t size) {
	FILE* file = DesktopOpenFile(path, "wb");
	assert(file);
	assert(fwrite(bytes, 1, size, file) == size);
	assert(DesktopFlushFile(file));
	assert(fclose(file) == 0);
}

int main(void) {
	const char* temporary_directory = getenv("TMPDIR");
	if (!temporary_directory || !*temporary_directory) temporary_directory = "/tmp";
	char source[1024], backup[1024], temporary[1024], impossible[1024];
	snprintf(source, sizeof source, "%s/ccleste-v2-%ld.dat", temporary_directory, (long)getpid());
	snprintf(backup, sizeof backup, "%s/ccleste-v2-%ld.bak", temporary_directory, (long)getpid());
	snprintf(temporary, sizeof temporary, "%s/ccleste-v2-%ld.tmp", temporary_directory, (long)getpid());
	snprintf(impossible, sizeof impossible, "%s/ccleste-v2-missing-%ld/backup", temporary_directory, (long)getpid());
	DesktopRemoveFile(source);
	DesktopRemoveFile(backup);
	DesktopRemoveFile(temporary);

	const unsigned char original[] = {2, 4, 6, 8, 10};
	const unsigned char changed[] = {1, 3, 5, 7, 9};
	write_bytes(source, original, sizeof original);
	assert(DesktopPreserveBackup(source, backup, temporary));
	assert(DesktopFilesEqual(source, backup));

	write_bytes(source, changed, sizeof changed);
	assert(!DesktopPreserveBackup(source, backup, temporary));
	write_bytes(temporary, original, sizeof original);
	assert(DesktopFilesEqual(temporary, backup));
	DesktopRemoveFile(temporary);

	assert(!DesktopPreserveBackup(source, impossible, temporary));
	write_bytes(temporary, changed, sizeof changed);
	assert(DesktopFilesEqual(source, temporary));

	DesktopRemoveFile(source);
	DesktopRemoveFile(backup);
	DesktopRemoveFile(temporary);
	return 0;
}
