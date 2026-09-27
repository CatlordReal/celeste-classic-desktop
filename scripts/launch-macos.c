#include <limits.h>
#include <mach-o/dyld.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int main(int argc, char **argv) {
    char executable[PATH_MAX];
    char resolved[PATH_MAX];
    uint32_t executable_size = sizeof executable;
    char *macos_directory;

    (void)argc;
    if (_NSGetExecutablePath(executable, &executable_size) != 0 ||
        !realpath(executable, resolved)) {
        fputs("Unable to locate Celeste Classic.app.\n", stderr);
        return 1;
    }
    macos_directory = strrchr(resolved, '/');
    if (!macos_directory) {
        fputs("Unable to locate app resources.\n", stderr);
        return 1;
    }
    *macos_directory = '\0';
    if (chdir(macos_directory) != 0 || chdir("../Resources") != 0) {
        perror("Unable to open app resources");
        return 1;
    }
    execv("./ccleste-bin", argv);
    perror("Unable to launch ccleste-bin");
    return 1;
}
