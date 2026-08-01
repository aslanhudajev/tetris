#ifndef PLATFORM_H
#define PLATFORM_H

#include <stdbool.h>

/* Resolves a path inside the writable app data directory, creating the
   directory if needed. `relative` is a bare filename such as "scores.txt". */
bool platform_data_path(const char *relative, char *out, int out_size);

/* Resolves a path inside the bundled assets directory. `relative` is given
   relative to `assets/`, for example "themes/themes.cfg". Returns false when no
   candidate location contains the file. */
bool platform_asset_path(const char *relative, char *out, int out_size);

#endif
