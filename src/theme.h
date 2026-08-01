#ifndef THEME_H
#define THEME_H

#include "config.h"
#include "game.h"

#include <raylib.h>
#include <stdbool.h>

#define THEME_ID_MAX 32
#define THEME_NAME_MAX 48
#define THEME_FLAVOR_MAX 96
#define THEME_PATH_MAX 192

typedef enum {
    /* Faded copy of the block art. */
    THEME_GHOST_TILE,
    /* Hollow rectangle, the way the untextured default draws it. */
    THEME_GHOST_OUTLINE,
} ThemeGhostStyle;

typedef struct {
    char id[THEME_ID_MAX];
    char name[THEME_NAME_MAX];
    char flavor[THEME_FLAVOR_MAX];
    char author[THEME_NAME_MAX];

    /* Path to the grayscale tile, relative to assets/themes/. Empty means the
       theme draws flat colored squares instead of textured ones. */
    char tile[THEME_PATH_MAX];

    Color piece_colors[PIECE_L + 1];
    Color background;
    Color panel;
    Color outline;

    ThemeGhostStyle ghost_style;
    /* Negative means "pick a sensible default for the ghost style". */
    float ghost_opacity;

    Texture2D texture;
    bool texture_loaded;
} Theme;

typedef struct {
    Theme entries[THEME_MAX_COUNT];
    int count;
    int active;
} ThemeLibrary;

/* Builds the built-in "No Theme" entry, appends everything declared in
   assets/themes/themes.cfg, then activates the theme saved on disk. */
void themes_load(ThemeLibrary *library);
void themes_unload(ThemeLibrary *library);

void themes_set_active(ThemeLibrary *library, int index);
const Theme *themes_active(const ThemeLibrary *library);

Color theme_piece_color(const Theme *theme, PieceType piece);
bool theme_has_texture(const Theme *theme);

/* A theme without art can only draw outlines, whatever it asked for. */
ThemeGhostStyle theme_ghost_style(const Theme *theme);
float theme_ghost_opacity(const Theme *theme);

#endif
