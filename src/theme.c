#include "theme.h"

#include "platform.h"

#include <math.h>
#include <rlgl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define THEME_MANIFEST "themes/themes.cfg"
#define THEME_SETTINGS_FILE "settings.txt"

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

/* How far apart in luminance the ghost and the well have to end up once the
   ghost's own transparency has been applied. Roughly the point where a one
   pixel outline still reads without competing with a real piece. */
#define GHOST_MIN_CONTRAST 0.22f

#define DEFAULT_SHEET_ORDER "IOTSZJL"

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

    const size_t length = strlen(value);

    if (length < 6) {
        return false;
    }

    char *end = NULL;
    const long rgba = strtol(value, &end, 16);

    if (end == value) {
        return false;
    }

    if (length >= 8) {
        out->r = (unsigned char)((rgba >> 24) & 0xFF);
        out->g = (unsigned char)((rgba >> 16) & 0xFF);
        out->b = (unsigned char)((rgba >> 8) & 0xFF);
        out->a = (unsigned char)(rgba & 0xFF);
        return true;
    }

    out->r = (unsigned char)((rgba >> 16) & 0xFF);
    out->g = (unsigned char)((rgba >> 8) & 0xFF);
    out->b = (unsigned char)(rgba & 0xFF);
    out->a = 255;
    return true;
}

static bool parse_fit(const char *value, ThemeFit *out) {
    if (strcmp(value, "cover") == 0) {
        *out = THEME_FIT_COVER;
    } else if (strcmp(value, "tile") == 0) {
        *out = THEME_FIT_TILE;
    } else {
        *out = THEME_FIT_STRETCH;
    }

    return true;
}

static PieceType piece_from_letter(char letter) {
    switch (letter) {
    case 'i': case 'I': return PIECE_I;
    case 'o': case 'O': return PIECE_O;
    case 't': case 'T': return PIECE_T;
    case 's': case 'S': return PIECE_S;
    case 'z': case 'Z': return PIECE_Z;
    case 'j': case 'J': return PIECE_J;
    case 'l': case 'L': return PIECE_L;
    default: return PIECE_NONE;
    }
}

/* Maps grid cells to pieces by walking the letters left to right. Separators
   are ignored; any other character (`-` by convention) burns a slot so sheets
   can carry cells the game does not use. */
static void apply_sheet_order(Theme *theme, const char *order) {
    for (int i = 0; i <= PIECE_L; i++) {
        theme->blocks.slot[i] = -1;
    }

    int slot = 0;

    for (const char *cursor = order; *cursor != '\0'; cursor++) {
        if (*cursor == ' ' || *cursor == '\t' || *cursor == ',') {
            continue;
        }

        const PieceType piece = piece_from_letter(*cursor);

        if (piece != PIECE_NONE) {
            theme->blocks.slot[piece] = slot;
        }

        slot++;
    }
}

/* Light on dark. Secondary tones sit at roughly 4.5:1 against a dark backdrop,
   which is the floor for text this small. */
static void ink_preset_dark(ThemeInk *ink) {
    ink->ink = (Color){244, 244, 244, 255};
    ink->muted = (Color){184, 184, 184, 255};
    ink->dim = (Color){148, 148, 148, 255};
    ink->hairline = (Color){255, 255, 255, 34};
    ink->chip = (Color){226, 226, 226, 255};
    ink->chip_ink = (Color){14, 14, 16, 255};
    ink->scrim = (Color){0, 0, 0, 255};
}

/* The same relationships inverted, including the scrim, which lightens the
   backdrop here rather than darkening it. */
static void ink_preset_light(ThemeInk *ink) {
    ink->ink = (Color){18, 18, 24, 255};
    ink->muted = (Color){78, 78, 88, 255};
    ink->dim = (Color){110, 110, 122, 255};
    ink->hairline = (Color){0, 0, 0, 38};
    ink->chip = (Color){26, 26, 32, 255};
    ink->chip_ink = (Color){246, 246, 248, 255};
    ink->scrim = (Color){255, 255, 255, 255};
}

static void theme_set_defaults(Theme *theme) {
    memset(theme, 0, sizeof(*theme));

    for (int i = 0; i <= PIECE_L; i++) {
        theme->piece_colors[i] = DEFAULT_PIECE_COLORS[i];
    }

    theme->blocks.mode = THEME_BLOCK_SOLID;
    theme->blocks.columns = 7;
    theme->blocks.rows = 1;
    theme->blocks.radius = 0.18f;
    theme->blocks.inset = (float)CELL_GAP;
    theme->blocks.border_width = 1.0f;
    apply_sheet_order(theme, DEFAULT_SHEET_ORDER);

    theme->backdrop.mode = THEME_BACKDROP_GRADIENT;
    theme->backdrop.top = (Color){18, 18, 24, 255};
    theme->backdrop.fit = THEME_FIT_COVER;

    /* Square, hard edged and neutral. Rounded corners and tinted frames both
       pull attention away from the blocks, which are the only thing on this
       screen that should be carrying colour. */
    theme->panel.fill = (Color){9, 9, 10, 235};
    theme->panel.border = (Color){255, 255, 255, 58};
    theme->panel.border_width = 1.0f;
    theme->panel.radius = 0.0f;
    theme->panel.fit = THEME_FIT_STRETCH;

    /* Slightly translucent so a shader or image backdrop still carries through
       the playfield instead of being punched out by it. */
    theme->well.fill = (Color){7, 7, 8, 238};
    theme->well.border = (Color){255, 255, 255, 58};
    theme->well.border_width = 1.0f;
    theme->well.radius = 0.0f;
    theme->well.fit = THEME_FIT_STRETCH;

    ink_preset_dark(&theme->ink);

    theme->ghost_style = THEME_GHOST_TILE;
    theme->ghost_opacity = GHOST_OPACITY_AUTO;
}

static PieceType piece_from_key(const char *key) {
    if (strncmp(key, "color_", 6) != 0 || key[7] != '\0') {
        return PIECE_NONE;
    }

    return piece_from_letter(key[6]);
}

static bool apply_surface_field(
    ThemeSurface *surface,
    const char *key,
    const char *prefix,
    const char *value
) {
    const size_t length = strlen(prefix);

    if (strncmp(key, prefix, length) != 0) {
        return false;
    }

    const char *suffix = key + length;

    if (suffix[0] == '\0') {
        parse_color(value, &surface->fill);
    } else if (strcmp(suffix, "_border") == 0) {
        parse_color(value, &surface->border);
    } else if (strcmp(suffix, "_border_width") == 0) {
        surface->border_width = strtof(value, NULL);
    } else if (strcmp(suffix, "_radius") == 0) {
        surface->radius = strtof(value, NULL);
    } else if (strcmp(suffix, "_image") == 0) {
        copy_field(surface->image, THEME_PATH_MAX, value);
    } else if (strcmp(suffix, "_fit") == 0) {
        parse_fit(value, &surface->fit);
    } else {
        return false;
    }

    return true;
}

static bool apply_backdrop_field(Theme *theme, const char *key, const char *value) {
    ThemeBackdrop *backdrop = &theme->backdrop;

    if (strcmp(key, "background") == 0) {
        parse_color(value, &backdrop->top);
    } else if (strcmp(key, "background_bottom") == 0) {
        parse_color(value, &backdrop->bottom);
        backdrop->mode = THEME_BACKDROP_GRADIENT;
    } else if (strcmp(key, "background_flat") == 0) {
        backdrop->mode = THEME_BACKDROP_SOLID;
    } else if (strcmp(key, "background_image") == 0) {
        copy_field(backdrop->image, THEME_PATH_MAX, value);
    } else if (strcmp(key, "background_shader") == 0) {
        copy_field(backdrop->shader, THEME_PATH_MAX, value);
    } else if (strcmp(key, "background_fit") == 0) {
        parse_fit(value, &backdrop->fit);
    } else {
        return false;
    }

    return true;
}

static bool apply_block_field(Theme *theme, const char *key, const char *value) {
    ThemeBlocks *blocks = &theme->blocks;

    if (strcmp(key, "sheet") == 0) {
        copy_field(blocks->path, THEME_PATH_MAX, value);
        blocks->mode = THEME_BLOCK_SHEET;
    } else if (strcmp(key, "tile") == 0) {
        copy_field(blocks->path, THEME_PATH_MAX, value);
        blocks->mode = THEME_BLOCK_TINTED;
    } else if (strcmp(key, "columns") == 0) {
        const int columns = (int)strtol(value, NULL, 10);
        if (columns > 0) blocks->columns = columns;
    } else if (strcmp(key, "rows") == 0) {
        const int rows = (int)strtol(value, NULL, 10);
        if (rows > 0) blocks->rows = rows;
    } else if (strcmp(key, "order") == 0) {
        apply_sheet_order(theme, value);
    } else if (strcmp(key, "block_radius") == 0) {
        const float radius = strtof(value, NULL);
        blocks->radius = radius < 0.0f ? 0.0f : (radius > 1.0f ? 1.0f : radius);
    } else if (strcmp(key, "block_inset") == 0) {
        blocks->inset = strtof(value, NULL);
    } else if (strcmp(key, "block_border_width") == 0) {
        blocks->border_width = strtof(value, NULL);
    } else if (strcmp(key, "block_border") == 0) {
        blocks->border_is_override = parse_color(value, &blocks->border_override);
    } else {
        return false;
    }

    return true;
}

static bool apply_ink_field(ThemeInk *ink, const char *key, const char *value) {
    if (strcmp(key, "ui") == 0) {
        /* A preset, so a theme only has to name its overrides. It overwrites
           the whole set, which is why it has to come before them. */
        if (strcmp(value, "light") == 0) {
            ink_preset_light(ink);
        } else {
            ink_preset_dark(ink);
        }
    } else if (strcmp(key, "ui_ink") == 0) {
        parse_color(value, &ink->ink);
    } else if (strcmp(key, "ui_ink_soft") == 0) {
        ink->ink_soft_is_override = parse_color(value, &ink->ink_soft);
    } else if (strcmp(key, "ui_muted") == 0) {
        parse_color(value, &ink->muted);
    } else if (strcmp(key, "ui_dim") == 0) {
        parse_color(value, &ink->dim);
    } else if (strcmp(key, "ui_hairline") == 0) {
        parse_color(value, &ink->hairline);
    } else if (strcmp(key, "ui_chip") == 0) {
        parse_color(value, &ink->chip);
    } else if (strcmp(key, "ui_chip_ink") == 0) {
        parse_color(value, &ink->chip_ink);
    } else if (strcmp(key, "ui_scrim") == 0) {
        parse_color(value, &ink->scrim);
    } else if (strcmp(key, "ui_accent") == 0) {
        ink->accent_is_override = parse_color(value, &ink->accent);
    } else {
        return false;
    }

    return true;
}

static void apply_field(Theme *theme, const char *key, const char *value) {
    const PieceType piece = piece_from_key(key);

    if (piece != PIECE_NONE) {
        parse_color(value, &theme->piece_colors[piece]);
        return;
    }

    if (apply_block_field(theme, key, value) ||
        apply_backdrop_field(theme, key, value) ||
        apply_ink_field(&theme->ink, key, value) ||
        apply_surface_field(&theme->panel, key, "panel", value) ||
        apply_surface_field(&theme->well, key, "well", value)) {
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
    } else if (strcmp(key, "outline") == 0) {
        /* Older manifests used one key for every edge. */
        parse_color(value, &theme->panel.border);
    } else if (strcmp(key, "ghost") == 0) {
        theme->ghost_style = strcmp(value, "outline") == 0 ? THEME_GHOST_OUTLINE : THEME_GHOST_TILE;
    } else if (strcmp(key, "ghost_opacity") == 0) {
        const float opacity = strtof(value, NULL);

        if (opacity >= 0.0f && opacity <= 1.0f) {
            theme->ghost_opacity = opacity;
        }
    } else {
        TraceLog(LOG_WARNING, "THEME: '%s' has unknown key '%s'", theme->id, key);
    }
}

static Color mix_color(Color a, Color b, float t) {
    return (Color){
        (unsigned char)(a.r + (b.r - a.r) * t),
        (unsigned char)(a.g + (b.g - a.g) * t),
        (unsigned char)(a.b + (b.b - a.b) * t),
        (unsigned char)(a.a + (b.a - a.a) * t),
    };
}

/* sRGB weights without the gamma step. This only has to rank colours against
   one another, not match a perceptual standard. */
static float luminance(Color color) {
    return (0.2126f * color.r + 0.7152f * color.g + 0.0722f * color.b) / 255.0f;
}

/* The landing preview is drawn in the piece's own colour, which disappears
   whenever a piece sits at the same brightness as the well behind it. A palette
   built from one hue guarantees at least one that does — Game Boy's O piece was
   the exact green of the well — so each ghost colour is pushed away from the
   well until it clears a minimum separation.

   Resolved here rather than while drawing, so the render loop still only reads
   a colour out of a table. */
static void resolve_ghost_colors(Theme *theme) {
    /* A tile ghost is the block art drawn faintly, so it carries its own
       shading and never reduces to a single colour. Only the outline style
       reads this table. */
    if (theme_ghost_style(theme) != THEME_GHOST_OUTLINE) {
        for (int piece = PIECE_NONE; piece <= PIECE_L; piece++) {
            theme->blocks.ghost[piece] = theme->piece_colors[piece];
        }

        return;
    }

    /* Assumes the well is close to opaque. A theme that makes it mostly
       transparent is choosing whatever the backdrop does behind it. */
    const float well = luminance(theme->well.fill);

    /* Away from whichever end of the range the well is nearer. That leaves the
       most room to move, and keeps a light theme's ghosts dark rather than
       washing them out to white. */
    const bool darken = well > 0.5f;
    const Color pole = darken ? (Color){0, 0, 0, 255} : (Color){255, 255, 255, 255};
    const float pole_lum = darken ? 0.0f : 1.0f;

    /* Transparency scales the contrast down, so ask for proportionally more of
       it than we need to end up with. */
    const float opacity = theme_ghost_opacity(theme);
    const float required = GHOST_MIN_CONTRAST / (opacity > 0.05f ? opacity : 0.05f);
    const float target = darken ? well - required : well + required;

    for (int piece = PIECE_NONE; piece <= PIECE_L; piece++) {
        const Color color = theme->piece_colors[piece];
        const float lum = luminance(color);

        if (fabsf(lum - well) >= required) {
            theme->blocks.ghost[piece] = color;
            continue;
        }

        /* Blending towards the pole moves luminance linearly, so the blend that
           just reaches the target can be solved for rather than searched. */
        const float span = pole_lum - lum;
        float t = (fabsf(span) < 0.001f) ? 1.0f : (target - lum) / span;

        t = t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t);
        theme->blocks.ghost[piece] = mix_color(color, pole, t);
    }
}

/* Folds every manifest choice into plain values. Nothing below this point is
   recomputed while the game is running. */
static void theme_finalize(Theme *theme) {
    ThemeBlocks *blocks = &theme->blocks;

    if (blocks->path[0] == '\0') {
        blocks->mode = THEME_BLOCK_SOLID;
    }

    for (int piece = PIECE_NONE; piece <= PIECE_L; piece++) {
        const Color color = theme->piece_colors[piece];

        switch (blocks->mode) {
        case THEME_BLOCK_SHEET:
            /* Art carries its own colour, so the tint is a no-op white. */
            blocks->fill[piece] = WHITE;
            break;
        default:
            blocks->fill[piece] = color;
            break;
        }

        blocks->border[piece] = blocks->border_is_override
                                    ? blocks->border_override
                                    : ColorBrightness(color, -0.35f);
    }

    if (theme->backdrop.shader[0] != '\0') {
        theme->backdrop.mode = THEME_BACKDROP_SHADER;
    } else if (theme->backdrop.image[0] != '\0') {
        theme->backdrop.mode = THEME_BACKDROP_IMAGE;
    } else if (theme->backdrop.mode == THEME_BACKDROP_GRADIENT &&
               theme->backdrop.bottom.a == 0) {
        theme->backdrop.bottom = mix_color(theme->backdrop.top, (Color){0, 0, 0, 255}, 0.35f);
        theme->backdrop.top = mix_color(theme->backdrop.top, (Color){255, 255, 255, 255}, 0.06f);
    }

    /* Body text sits between the headings and the secondary tone, so a theme
       that sets only `ui_ink` and `ui_muted` still gets a coherent third step. */
    if (!theme->ink.ink_soft_is_override) {
        theme->ink.ink_soft = mix_color(theme->ink.ink, theme->ink.muted, 0.5f);
    }

    /* Last, because it reads both the resolved block mode and the well. */
    resolve_ghost_colors(theme);
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

        if (current == NULL) {
            continue;
        }

        /* A few keys are switches rather than settings. background_flat is
           documented without a value, so a bare key has to reach apply_field
           instead of being dropped as a malformed line. */
        if (separator == NULL) {
            apply_field(current, text, "");
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

static bool resolve_asset(const char *relative_to_themes, char *out, int out_size) {
    char relative[THEME_PATH_MAX + 16];

    snprintf(relative, sizeof(relative), "themes/%s", relative_to_themes);
    return platform_asset_path(relative, out, out_size);
}

static bool load_image_texture(const Theme *theme, const char *asset, ThemeFit fit, Texture2D *out) {
    char path[1024];

    if (!resolve_asset(asset, path, (int)sizeof(path))) {
        TraceLog(LOG_WARNING, "THEME: '%s' cannot find %s", theme->id, asset);
        return false;
    }

    *out = LoadTexture(path);

    if (out->id == 0) {
        TraceLog(LOG_WARNING, "THEME: '%s' failed to load %s", theme->id, asset);
        return false;
    }

    /* Repeat wrapping lets a tiled backdrop cover the window in one quad
       instead of a loop of draws. */
    SetTextureWrap(*out, fit == THEME_FIT_TILE ? TEXTURE_WRAP_REPEAT : TEXTURE_WRAP_CLAMP);
    SetTextureFilter(*out, TEXTURE_FILTER_BILINEAR);
    return true;
}

static void acquire_surface(const Theme *theme, ThemeSurface *surface) {
    if (surface->image[0] == '\0') {
        return;
    }

    surface->texture_loaded = load_image_texture(theme, surface->image, surface->fit, &surface->texture);
}

static void report_sheet(const Theme *theme) {
    const ThemeBlocks *blocks = &theme->blocks;
    const int slots = blocks->columns * blocks->rows;

    TraceLog(
        LOG_INFO,
        "THEME: '%s' sheet %dx%d, %dx%d grid, cells of %dx%d",
        theme->id,
        blocks->texture.width,
        blocks->texture.height,
        blocks->columns,
        blocks->rows,
        blocks->texture.width / blocks->columns,
        blocks->texture.height / blocks->rows
    );

    if (blocks->texture.width % blocks->columns != 0 ||
        blocks->texture.height % blocks->rows != 0) {
        TraceLog(
            LOG_WARNING,
            "THEME: '%s' sheet %dx%d does not divide evenly into %d x %d cells, "
            "so slices will drift. Crop the sheet to a multiple of the grid.",
            theme->id,
            blocks->texture.width,
            blocks->texture.height,
            blocks->columns,
            blocks->rows
        );
    }

    for (int piece = PIECE_I; piece <= PIECE_L; piece++) {
        if (blocks->slot[piece] >= slots) {
            TraceLog(
                LOG_WARNING,
                "THEME: '%s' maps piece %c to cell %d but the grid only holds %d. "
                "Check `order`, `columns` and `rows`.",
                theme->id,
                "?IOTSZJL"[piece],
                blocks->slot[piece],
                slots
            );
        }
    }
}

static void acquire_blocks(Theme *theme) {
    ThemeBlocks *blocks = &theme->blocks;

    if (blocks->mode == THEME_BLOCK_SOLID) {
        return;
    }

    char path[1024];

    if (!resolve_asset(blocks->path, path, (int)sizeof(path))) {
        TraceLog(LOG_WARNING, "THEME: '%s' cannot find %s", theme->id, blocks->path);
        blocks->mode = THEME_BLOCK_SOLID;
        return;
    }

    blocks->texture = LoadTexture(path);

    if (blocks->texture.id == 0) {
        TraceLog(LOG_WARNING, "THEME: '%s' failed to load %s", theme->id, blocks->path);
        blocks->mode = THEME_BLOCK_SOLID;
        return;
    }

    blocks->texture_loaded = true;

    if (blocks->mode == THEME_BLOCK_TINTED) {
        GenTextureMipmaps(&blocks->texture);
        SetTextureFilter(blocks->texture, TEXTURE_FILTER_TRILINEAR);

        const Rectangle full = {
            0.0f,
            0.0f,
            (float)blocks->texture.width,
            (float)blocks->texture.height,
        };

        for (int piece = PIECE_NONE; piece <= PIECE_L; piece++) {
            blocks->source[piece] = full;
        }

        return;
    }

    /* No mipmaps for sheets: at small mip levels neighbouring cells average
       together and bleed each other's colour across the seams. Blocks sit on
       whole-pixel grid positions at a fixed size, so there is no sub-pixel
       motion for mipmapping to smooth out anyway. */
    SetTextureFilter(blocks->texture, TEXTURE_FILTER_BILINEAR);
    report_sheet(theme);

    const float cell_width = (float)blocks->texture.width / (float)blocks->columns;
    const float cell_height = (float)blocks->texture.height / (float)blocks->rows;
    const int slots = blocks->columns * blocks->rows;

    for (int piece = PIECE_NONE; piece <= PIECE_L; piece++) {
        const int slot = blocks->slot[piece];

        if (slot < 0 || slot >= slots) {
            /* Falls back to a flat square for this piece only. */
            blocks->source[piece] = (Rectangle){0.0f, 0.0f, 0.0f, 0.0f};
            continue;
        }

        /* Half a texel of inset stops bilinear sampling from reaching into the
           neighbouring cell along the seams. */
        blocks->source[piece] = (Rectangle){
            (float)(slot % blocks->columns) * cell_width + 0.5f,
            (float)(slot / blocks->columns) * cell_height + 0.5f,
            cell_width - 1.0f,
            cell_height - 1.0f,
        };
    }
}

static void acquire_backdrop(Theme *theme) {
    ThemeBackdrop *backdrop = &theme->backdrop;

    if (backdrop->mode == THEME_BACKDROP_IMAGE) {
        backdrop->texture_loaded =
            load_image_texture(theme, backdrop->image, backdrop->fit, &backdrop->texture);

        if (!backdrop->texture_loaded) {
            backdrop->mode = THEME_BACKDROP_GRADIENT;
        }

        return;
    }

    if (backdrop->mode != THEME_BACKDROP_SHADER) {
        return;
    }

    char path[1024];

    if (!resolve_asset(backdrop->shader, path, (int)sizeof(path))) {
        TraceLog(LOG_WARNING, "THEME: '%s' cannot find %s", theme->id, backdrop->shader);
        backdrop->mode = THEME_BACKDROP_GRADIENT;
        return;
    }

    backdrop->program = LoadShader(NULL, path);

    /* raylib hands back the default shader when compilation fails. */
    if (backdrop->program.id == rlGetShaderIdDefault()) {
        TraceLog(LOG_WARNING, "THEME: '%s' shader %s did not compile", theme->id, backdrop->shader);
        backdrop->mode = THEME_BACKDROP_GRADIENT;
        return;
    }

    backdrop->shader_loaded = true;
    backdrop->loc_time = GetShaderLocation(backdrop->program, "time");
    backdrop->loc_resolution = GetShaderLocation(backdrop->program, "resolution");
    TraceLog(LOG_INFO, "THEME: '%s' backdrop shader %s ready", theme->id, backdrop->shader);
}

/* Only the active theme holds GPU resources, so memory stays flat no matter
   how many themes are installed. */
static void theme_acquire(Theme *theme) {
    acquire_blocks(theme);
    acquire_backdrop(theme);
    acquire_surface(theme, &theme->panel);
    acquire_surface(theme, &theme->well);
}

static void release_surface(ThemeSurface *surface) {
    if (!surface->texture_loaded) {
        return;
    }

    UnloadTexture(surface->texture);
    surface->texture_loaded = false;
}

static void theme_release(Theme *theme) {
    if (theme->blocks.texture_loaded) {
        UnloadTexture(theme->blocks.texture);
        theme->blocks.texture_loaded = false;
    }

    if (theme->backdrop.texture_loaded) {
        UnloadTexture(theme->backdrop.texture);
        theme->backdrop.texture_loaded = false;
    }

    if (theme->backdrop.shader_loaded) {
        UnloadShader(theme->backdrop.program);
        theme->backdrop.shader_loaded = false;
    }

    release_surface(&theme->panel);
    release_surface(&theme->well);
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

    for (int i = 0; i < library->count; i++) {
        theme_finalize(&library->entries[i]);
    }

    restore_selection(library);
    theme_acquire(&library->entries[library->active]);
}

void themes_unload(ThemeLibrary *library) {
    for (int i = 0; i < library->count; i++) {
        theme_release(&library->entries[i]);
    }
}

void themes_set_active(ThemeLibrary *library, int index) {
    if (index < 0 || index >= library->count || index == library->active) {
        return;
    }

    theme_release(&library->entries[library->active]);
    library->active = index;
    theme_acquire(&library->entries[index]);
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

ThemeGhostStyle theme_ghost_style(const Theme *theme) {
    if (theme->blocks.mode == THEME_BLOCK_SOLID) {
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
