#include "ui.h"

#include <stddef.h>
#include <string.h>

/* Two atlases so small labels and large headings are both sampled near their
   native size instead of being scaled a long way in either direction. */
#define ATLAS_SMALL 30
#define ATLAS_LARGE 64
#define ATLAS_MONO 44
#define LARGE_THRESHOLD 29.0f

/* Only plain .ttf files: raylib bundles stb_truetype, which cannot read the
   .ttc collections that most of the older macOS families ship as. */
static const char *UI_FONT_CANDIDATES[] = {
    "/System/Library/Fonts/SFNSRounded.ttf",
    "/System/Library/Fonts/SFCompactRounded.ttf",
    "/System/Library/Fonts/SFNS.ttf",
    "/System/Library/Fonts/Supplemental/Arial Rounded Bold.ttf",
};

static const char *MONO_FONT_CANDIDATES[] = {
    "/System/Library/Fonts/SFNSMono.ttf",
    "/System/Library/Fonts/Monaco.ttf",
    "/System/Library/Fonts/Supplemental/Andale Mono.ttf",
};

static Font font_small;
static Font font_large;
static Font font_mono;
static bool fonts_loaded;
static char font_label[128] = "raylib default";

static float spacing_for(float size) {
    return size * 0.02f;
}

static bool is_default_font(Font font) {
    return font.texture.id == GetFontDefault().texture.id;
}

/* raylib hands back the built-in bitmap font when a face fails to parse, so a
   successful-looking return still has to be checked against it. */
static Font load_first_available(const char **candidates, int count, int atlas_size, char *chosen) {
    for (int i = 0; i < count; i++) {
        if (!FileExists(candidates[i])) {
            continue;
        }

        const Font font = LoadFontEx(candidates[i], atlas_size, NULL, 0);

        if (is_default_font(font) || font.glyphCount == 0) {
            continue;
        }

        SetTextureFilter(font.texture, TEXTURE_FILTER_BILINEAR);

        if (chosen != NULL) {
            TextCopy(chosen, GetFileName(candidates[i]));
        }

        return font;
    }

    return GetFontDefault();
}

void ui_fonts_load(void) {
    const int ui_count = (int)(sizeof(UI_FONT_CANDIDATES) / sizeof(UI_FONT_CANDIDATES[0]));
    const int mono_count = (int)(sizeof(MONO_FONT_CANDIDATES) / sizeof(MONO_FONT_CANDIDATES[0]));

    /* Sizes passed to the draw calls are logical points, and on a Retina
       display raylib draws them through a 2x transform. Rasterising the atlas
       at 1x would mean every glyph is magnified on the way to the screen, so
       the atlas has to match the backing scale. */
    const float dpi = GetWindowScaleDPI().y;
    const int scale = dpi >= 1.5f ? 2 : 1;

    font_small = load_first_available(UI_FONT_CANDIDATES, ui_count, ATLAS_SMALL * scale, font_label);
    font_large = load_first_available(UI_FONT_CANDIDATES, ui_count, ATLAS_LARGE * scale, NULL);
    font_mono = load_first_available(MONO_FONT_CANDIDATES, mono_count, ATLAS_MONO * scale, NULL);

    fonts_loaded = true;
    TraceLog(LOG_INFO, "UI: interface font is %s, atlas at %dx", font_label, scale);
}

void ui_fonts_unload(void) {
    if (!fonts_loaded) {
        return;
    }

    if (!is_default_font(font_small)) UnloadFont(font_small);
    if (!is_default_font(font_large)) UnloadFont(font_large);
    if (!is_default_font(font_mono)) UnloadFont(font_mono);

    fonts_loaded = false;
}

const char *ui_font_name(void) {
    return font_label;
}

static Font pick_font(float size) {
    return size >= LARGE_THRESHOLD ? font_large : font_small;
}

Vector2 ui_measure(const char *text, float size) {
    return MeasureTextEx(pick_font(size), text, size, spacing_for(size));
}

Vector2 ui_measure_mono(const char *text, float size) {
    return MeasureTextEx(font_mono, text, size, spacing_for(size));
}

void ui_text(const char *text, float x, float y, float size, Color color) {
    DrawTextEx(pick_font(size), text, (Vector2){x, y}, size, spacing_for(size), color);
}

void ui_text_mono(const char *text, float x, float y, float size, Color color) {
    DrawTextEx(font_mono, text, (Vector2){x, y}, size, spacing_for(size), color);
}

void ui_text_center(const char *text, float center_x, float y, float size, Color color) {
    const Vector2 measured = ui_measure(text, size);
    ui_text(text, center_x - measured.x * 0.5f, y, size, color);
}

void ui_text_shadowed(const char *text, float x, float y, float size, Color color) {
    ui_text(text, x, y + size * 0.045f, size, Fade(BLACK, 0.45f));
    ui_text(text, x, y, size, color);
}

#define LABEL_MAX 128

/* Bounded on purpose: raylib's TextCopy runs to the terminator, and some of
   this text comes out of theme files, so the length is not ours to trust. */
static const char *to_upper(const char *text, char *buffer, int size) {
    int i = 0;

    for (; text[i] != '\0' && i < size - 1; i++) {
        buffer[i] = (text[i] >= 'a' && text[i] <= 'z') ? (char)(text[i] - 'a' + 'A') : text[i];
    }

    buffer[i] = '\0';
    return buffer;
}

static float label_spacing(float size) {
    return size * 0.16f;
}

void ui_label(const char *text, float x, float y, float size, Color color) {
    char upper[LABEL_MAX];

    DrawTextEx(
        pick_font(size),
        to_upper(text, upper, LABEL_MAX),
        (Vector2){x, y},
        size,
        label_spacing(size),
        color
    );
}

Vector2 ui_measure_label(const char *text, float size) {
    char upper[LABEL_MAX];
    return MeasureTextEx(pick_font(size), to_upper(text, upper, LABEL_MAX), size, label_spacing(size));
}

/* raylib expresses corner rounding as a fraction of the shorter side, so
   convert from a pixel radius to keep corners consistent across panel sizes. */
static float roundness_for(Rectangle rect, float radius) {
    const float shorter = rect.width < rect.height ? rect.width : rect.height;

    if (shorter <= 0.0f) {
        return 0.0f;
    }

    const float value = radius / (shorter * 0.5f);
    return value > 1.0f ? 1.0f : value;
}

void ui_rounded(Rectangle rect, float radius, Color color) {
    DrawRectangleRounded(rect, roundness_for(rect, radius), 8, color);
}

void ui_rounded_outline(Rectangle rect, float radius, float thickness, Color color) {
    DrawRectangleRoundedLinesEx(rect, roundness_for(rect, radius), 8, thickness, color);
}

void ui_hairline(float x, float y, float width, Color color) {
    DrawRectangleRec((Rectangle){x, y, width, 1.0f}, color);
}

/* Vertex order here is topLeft, bottomLeft, bottomRight, topRight. */
void ui_wash(Rectangle rect, Color color, float alpha) {
    const Color near_edge = Fade(color, alpha);
    const Color far_edge = Fade(color, 0.0f);

    DrawRectangleGradientEx(rect, near_edge, near_edge, far_edge, far_edge);
}

void ui_scrim(int width, int height, float strength) {
    const float w = (float)width;
    const float h = (float)height;

    DrawRectangleRec((Rectangle){0.0f, 0.0f, w, h}, Fade(BLACK, 0.26f * strength));

    /* Heavier at the edges than the middle, so the header and footer type sits
       on something solid while the backdrop still reads in the centre. */
    DrawRectangleGradientV(0, 0, width, (int)(h * 0.32f), Fade(BLACK, 0.40f * strength), BLANK);
    DrawRectangleGradientV(
        0, (int)(h * 0.62f), width, (int)(h * 0.38f) + 1, BLANK, Fade(BLACK, 0.46f * strength));
}

float ui_approach(float current, float target, float rate, float dt) {
    float step = rate * dt;

    if (step > 1.0f) {
        step = 1.0f;
    }

    return current + (target - current) * step;
}

Color ui_mix(Color a, Color b, float t) {
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;

    return (Color){
        (unsigned char)(a.r + (b.r - a.r) * t),
        (unsigned char)(a.g + (b.g - a.g) * t),
        (unsigned char)(a.b + (b.b - a.b) * t),
        (unsigned char)(a.a + (b.a - a.a) * t),
    };
}
