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
    /* Flat coloured rectangles. No texture, no sampling. */
    THEME_BLOCK_SOLID,
    /* One grayscale image multiplied by the piece colour. */
    THEME_BLOCK_TINTED,
    /* Authored cells on a grid, drawn exactly as painted. */
    THEME_BLOCK_SHEET,
} ThemeBlockMode;

typedef enum {
    THEME_BACKDROP_SOLID,
    THEME_BACKDROP_GRADIENT,
    THEME_BACKDROP_IMAGE,
    THEME_BACKDROP_SHADER,
} ThemeBackdropMode;

typedef enum {
    THEME_FIT_STRETCH,
    THEME_FIT_COVER,
    THEME_FIT_TILE,
} ThemeFit;

typedef enum {
    /* Faded copy of the block art. */
    THEME_GHOST_TILE,
    /* Hollow rectangle, the way an untextured theme draws it. */
    THEME_GHOST_OUTLINE,
} ThemeGhostStyle;

/* Every field a block needs, flattened at load time. The per-cell draw path is
   an array index and one branch, whatever the theme is doing. */
typedef struct {
    ThemeBlockMode mode;

    char path[THEME_PATH_MAX];
    int columns;
    int rows;
    int slot[PIECE_L + 1];

    float radius;
    float inset;
    float border_width;
    Color border_override;
    bool border_is_override;

    /* Resolved once: sheet cell rectangles, and the tint that turns the shared
       texture into this piece. Solid mode leaves `source` unused. */
    Rectangle source[PIECE_L + 1];
    Color fill[PIECE_L + 1];
    Color border[PIECE_L + 1];

    Texture2D texture;
    bool texture_loaded;
} ThemeBlocks;

/* A panel, the board well, or anything else with a fill and an edge. */
typedef struct {
    Color fill;
    Color border;
    float border_width;
    float radius;

    char image[THEME_PATH_MAX];
    ThemeFit fit;
    Texture2D texture;
    bool texture_loaded;

    /* The board well tints its edge with the mode colour unless a theme says
       otherwise. */
    bool border_is_override;
} ThemeSurface;

typedef struct {
    ThemeBackdropMode mode;
    Color top;
    Color bottom;

    char image[THEME_PATH_MAX];
    ThemeFit fit;
    Texture2D texture;
    bool texture_loaded;

    char shader[THEME_PATH_MAX];
    Shader program;
    bool shader_loaded;
    int loc_time;
    int loc_resolution;
} ThemeBackdrop;

typedef struct {
    char id[THEME_ID_MAX];
    char name[THEME_NAME_MAX];
    char flavor[THEME_FLAVOR_MAX];
    char author[THEME_NAME_MAX];

    Color piece_colors[PIECE_L + 1];

    ThemeBlocks blocks;
    ThemeBackdrop backdrop;
    ThemeSurface panel;
    ThemeSurface well;

    ThemeGhostStyle ghost_style;
    /* Negative means "pick a sensible default for the ghost style". */
    float ghost_opacity;
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

/* A theme without block art can only draw outlines, whatever it asked for. */
ThemeGhostStyle theme_ghost_style(const Theme *theme);
float theme_ghost_opacity(const Theme *theme);

#endif
