#include "theme.h"

#include "platform.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define THEME_MANIFEST "themes/themes.cfg"
#define THEME_SETTINGS_FILE "settings.txt"

/* Bright and saturated on purpose: block art is applied as a multiply tint, so
   anything mid-toned here reads as muddy once it is drawn through a texture. */
static const Color DEFAULT_PIECE_COLORS[PIECE_L + 1] = {
    {40, 40, 48, 255},    /* PIECE_NONE */
    {34, 226, 245, 255},  /* I */
    {252, 216, 64, 255},  /* O */
    {186, 100, 252, 255}, /* T */
    {76, 224, 104, 255},  /* S */
    {252, 88, 100, 255},  /* Z */
    {80, 140, 252, 255},  /* J */
    {252, 156, 56, 255},  /* L */
};

#define GHOST_OPACITY_AUTO (-1.0f)
#define GHOST_OPACITY_TILE 0.13f
#define GHOST_OPACITY_OUTLINE 0.5f

static char *trim(char *text) {
    while (*text == ' ' || *text == '\t' || *text == '\r' || *text == '\n') {
        text++;
    }

    char *end = text + strlen(text);

    while (end > text) {
        const char previous = *(end - 1);

        if (previous != ' ' && previous != '\t' && previous != '\r' && previous != '\n') {
            break;
        }

        end--;
    }

    *end = '\0';
    return text;
}

static void copy_field(char *destination, int size, const char *value) {
    snprintf(destination, (size_t)size, "%s", value);
}

static bool parse_color(const char *value, Color *out) {
    while (*value == '#' || *value == ' ') {
        value++;
    }

    if (strlen(value) < 6) {
        return false;
    }

    char *end = NULL;
    const long rgb = strtol(value, &end, 16);

    if (end == value) {
        return false;
    }

    out->r = (unsigned char)((rgb >> 16) & 0xFF);
    out->g = (unsigned char)((rgb >> 8) & 0xFF);
    out->b = (unsigned char)(rgb & 0xFF);
    out->a = 255;
    return true;
}

static void theme_set_defaults(Theme *theme) {
    memset(theme, 0, sizeof(*theme));

    for (int i = 0; i <= PIECE_L; i++) {
        theme->piece_colors[i] = DEFAULT_PIECE_COLORS[i];
    }

    theme->background = (Color){18, 18, 24, 255};
    theme->panel = (Color){28, 28, 36, 255};
    theme->outline = (Color){60, 60, 72, 255};
    theme->ghost_style = THEME_GHOST_TILE;
    theme->ghost_opacity = GHOST_OPACITY_AUTO;
}

static PieceType piece_from_key(const char *key) {
    if (strcmp(key, "color_i") == 0) return PIECE_I;
    if (strcmp(key, "color_o") == 0) return PIECE_O;
    if (strcmp(key, "color_t") == 0) return PIECE_T;
    if (strcmp(key, "color_s") == 0) return PIECE_S;
    if (strcmp(key, "color_z") == 0) return PIECE_Z;
    if (strcmp(key, "color_j") == 0) return PIECE_J;
    if (strcmp(key, "color_l") == 0) return PIECE_L;
    return PIECE_NONE;
}

static void apply_field(Theme *theme, const char *key, const char *value) {
    const PieceType piece = piece_from_key(key);

    if (piece != PIECE_NONE) {
        parse_color(value, &theme->piece_colors[piece]);
        return;
    }

    if (strcmp(key, "id") == 0) {
        copy_field(theme->id, THEME_ID_MAX, value);
    } else if (strcmp(key, "name") == 0) {
        copy_field(theme->name, THEME_NAME_MAX, value);
    } else if (strcmp(key, "flavor") == 0) {
        copy_field(theme->flavor, THEME_FLAVOR_MAX, value);
    } else if (strcmp(key, "author") == 0) {
        copy_field(theme->author, THEME_NAME_MAX, value);
    } else if (strcmp(key, "tile") == 0) {
        copy_field(theme->tile, THEME_PATH_MAX, value);
    } else if (strcmp(key, "background") == 0) {
        parse_color(value, &theme->background);
    } else if (strcmp(key, "panel") == 0) {
        parse_color(value, &theme->panel);
    } else if (strcmp(key, "outline") == 0) {
        parse_color(value, &theme->outline);
    } else if (strcmp(key, "ghost") == 0) {
        theme->ghost_style = strcmp(value, "outline") == 0 ? THEME_GHOST_OUTLINE : THEME_GHOST_TILE;
    } else if (strcmp(key, "ghost_opacity") == 0) {
        const float opacity = strtof(value, NULL);

        if (opacity >= 0.0f && opacity <= 1.0f) {
            theme->ghost_opacity = opacity;
        }
    }
}

static void add_builtin_theme(ThemeLibrary *library) {
    Theme *theme = &library->entries[library->count++];

    theme_set_defaults(theme);
    copy_field(theme->id, THEME_ID_MAX, "none");
    copy_field(theme->name, THEME_NAME_MAX, "No Theme");
    copy_field(theme->flavor, THEME_FLAVOR_MAX, "Plain colored blocks. Always available.");
}

static void parse_manifest(ThemeLibrary *library) {
    char path[1024];

    if (!platform_asset_path(THEME_MANIFEST, path, (int)sizeof(path))) {
        return;
    }

    FILE *file = fopen(path, "r");
    if (file == NULL) {
        return;
    }

    Theme *current = NULL;
    char line[512];

    while (fgets(line, sizeof(line), file) != NULL) {
        char *text = trim(line);

        if (text[0] == '\0' || text[0] == '#') {
            continue;
        }

        if (text[0] == '[') {
            if (library->count >= THEME_MAX_COUNT) {
                break;
            }

            current = &library->entries[library->count++];
            theme_set_defaults(current);
            continue;
        }

        char *separator = strchr(text, '=');

        if (current == NULL || separator == NULL) {
            continue;
        }

        *separator = '\0';
        apply_field(current, trim(text), trim(separator + 1));
    }

    fclose(file);

    /* Drop trailing entries that never received an id. */
    while (library->count > 1 && library->entries[library->count - 1].id[0] == '\0') {
        library->count--;
    }
}

static void load_texture(Theme *theme) {
    if (theme->texture_loaded || theme->tile[0] == '\0') {
        return;
    }

    char relative[THEME_PATH_MAX + 16];
    char path[1024];

    snprintf(relative, sizeof(relative), "themes/%s", theme->tile);

    if (!platform_asset_path(relative, path, (int)sizeof(path))) {
        TraceLog(LOG_WARNING, "THEME: tile not found for '%s': %s", theme->id, relative);
        return;
    }

    theme->texture = LoadTexture(path);

    if (theme->texture.id == 0) {
        return;
    }

    GenTextureMipmaps(&theme->texture);
    SetTextureFilter(theme->texture, TEXTURE_FILTER_TRILINEAR);
    theme->texture_loaded = true;
}

static void unload_texture(Theme *theme) {
    if (!theme->texture_loaded) {
        return;
    }

    UnloadTexture(theme->texture);
    theme->texture_loaded = false;
}

static void save_selection(const ThemeLibrary *library) {
    char path[1024];

    if (!platform_data_path(THEME_SETTINGS_FILE, path, (int)sizeof(path))) {
        return;
    }

    FILE *file = fopen(path, "w");
    if (file == NULL) {
        return;
    }

    fprintf(file, "theme = %s\n", library->entries[library->active].id);
    fclose(file);
}

static void restore_selection(ThemeLibrary *library) {
    char path[1024];

    if (!platform_data_path(THEME_SETTINGS_FILE, path, (int)sizeof(path))) {
        return;
    }

    FILE *file = fopen(path, "r");
    if (file == NULL) {
        return;
    }

    char line[256];

    while (fgets(line, sizeof(line), file) != NULL) {
        char *separator = strchr(line, '=');

        if (separator == NULL) {
            continue;
        }

        *separator = '\0';

        if (strcmp(trim(line), "theme") != 0) {
            continue;
        }

        const char *id = trim(separator + 1);

        for (int i = 0; i < library->count; i++) {
            if (strcmp(library->entries[i].id, id) == 0) {
                library->active = i;
                break;
            }
        }
    }

    fclose(file);
}

void themes_load(ThemeLibrary *library) {
    memset(library, 0, sizeof(*library));

    add_builtin_theme(library);
    parse_manifest(library);
    restore_selection(library);
    load_texture(&library->entries[library->active]);
}

void themes_unload(ThemeLibrary *library) {
    for (int i = 0; i < library->count; i++) {
        unload_texture(&library->entries[i]);
    }
}

/* Only the active theme keeps a texture resident, which keeps memory flat no
   matter how many themes are installed. */
void themes_set_active(ThemeLibrary *library, int index) {
    if (index < 0 || index >= library->count || index == library->active) {
        return;
    }

    unload_texture(&library->entries[library->active]);
    library->active = index;
    load_texture(&library->entries[index]);
    save_selection(library);
}

const Theme *themes_active(const ThemeLibrary *library) {
    return &library->entries[library->active];
}

Color theme_piece_color(const Theme *theme, PieceType piece) {
    if (piece < PIECE_NONE || piece > PIECE_L) {
        return theme->piece_colors[PIECE_NONE];
    }

    return theme->piece_colors[piece];
}

bool theme_has_texture(const Theme *theme) {
    return theme->texture_loaded;
}

ThemeGhostStyle theme_ghost_style(const Theme *theme) {
    if (!theme->texture_loaded) {
        return THEME_GHOST_OUTLINE;
    }

    return theme->ghost_style;
}

float theme_ghost_opacity(const Theme *theme) {
    if (theme->ghost_opacity >= 0.0f) {
        return theme->ghost_opacity;
    }

    return theme_ghost_style(theme) == THEME_GHOST_OUTLINE ? GHOST_OPACITY_OUTLINE : GHOST_OPACITY_TILE;
}
