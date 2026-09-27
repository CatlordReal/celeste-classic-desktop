#include <windows.h>
#include <process.h>
#include <stdio.h>
#include <wchar.h>

int wmain(int argc, wchar_t **argv) {
    wchar_t executable[MAX_PATH];
    wchar_t game_path[MAX_PATH];
    wchar_t *directory;
    DWORD length;

    (void)argc;
    length = GetModuleFileNameW(NULL, executable, MAX_PATH);
    if (!length || length >= MAX_PATH) {
        fputs("Unable to locate ccleste.exe.\n", stderr);
        return 1;
    }

    directory = wcsrchr(executable, L'\\');
    if (!directory) {
        fputs("Unable to locate release directory.\n", stderr);
        return 1;
    }
    *directory = '\0';
    if (!SetCurrentDirectoryW(executable)) {
        fputs("Unable to open release directory.\n", stderr);
        return 1;
    }
    int written = swprintf(game_path, MAX_PATH, L"%ls\\ccleste-game.exe", executable);
    if (written < 0 || written >= MAX_PATH) {
        fputs("Release path is too long.\n", stderr);
        return 1;
    }
    _wexecv(game_path, (const wchar_t *const *)argv);
    perror("Unable to launch ccleste-game.exe");
    return 1;
}
