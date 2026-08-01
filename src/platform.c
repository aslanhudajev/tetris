#include "platform.h"

#include <raylib.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>

#define APP_DATA_FOLDER "Puzzie"

bool platform_data_path(const char *relative, char *out, int out_size) {
    const char *home = getenv("HOME");

    if (home == NULL || home[0] == '\0') {
        return false;
    }

    char dir[1024];
    if (snprintf(dir, sizeof(dir), "%s/Library/Application Support/%s", home, APP_DATA_FOLDER) >=
        (int)sizeof(dir)) {
        return false;
    }

    mkdir(dir, 0755);

    return snprintf(out, (size_t)out_size, "%s/%s", dir, relative) < out_size;
}

/* Assets sit next to the executable when bundled, one level up during a plain
   cmake build, and inside Contents/Resources in a macOS .app. */
bool platform_asset_path(const char *relative, char *out, int out_size) {
    static const char *PATTERNS[] = {
        "%sassets/%s",
        "%s../Resources/assets/%s",
        "%s../assets/%s",
        "%s../../assets/%s",
    };

    const char *app_dir = GetApplicationDirectory();

    for (int i = 0; i < (int)(sizeof(PATTERNS) / sizeof(PATTERNS[0])); i++) {
        if (snprintf(out, (size_t)out_size, PATTERNS[i], app_dir, relative) >= out_size) {
            continue;
        }

        if (FileExists(out)) {
            return true;
        }
    }

    if (snprintf(out, (size_t)out_size, "assets/%s", relative) < out_size && FileExists(out)) {
        return true;
    }

    out[0] = '\0';
    return false;
}
