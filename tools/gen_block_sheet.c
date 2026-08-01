/* Generates the default block sheet: one authored, fully coloured block per
   piece laid out in a row. Not part of the game build.
 *
 *   clang -std=c11 tools/gen_block_sheet.c -o /tmp/gen $(pkg-config --cflags --libs raylib) -lm
 *   /tmp/gen assets/themes/bevel/blocks.png
 *
 * The point of baking colour in rather than tinting a grey tile at runtime is
 * hue shifting: highlights drift warm and shadows drift cool, which is what
 * separates a lit object from a flat square. A multiply tint cannot do that,
 * because it only scales brightness. */

#include <raylib.h>

#include <math.h>
#include <stdio.h>

#define CELL 128
#define PIECES 7
#define MARGIN 3.0f
#define CORNER 17.0f
#define BEVEL 15.0f

/* Warm light, cool ambient fill. */
#define HUE_HIGHLIGHT 48.0f
#define HUE_SHADOW 250.0f

static const Color PIECE_COLORS[PIECES] = {
    {34, 226, 245, 255},  /* I */
    {252, 216, 64, 255},  /* O */
    {186, 100, 252, 255}, /* T */
    {76, 224, 104, 255},  /* S */
    {252, 88, 100, 255},  /* Z */
    {80, 140, 252, 255},  /* J */
    {252, 156, 56, 255},  /* L */
};

static float clampf(float value, float low, float high) {
    return value < low ? low : (value > high ? high : value);
}

static float smoothstep(float edge0, float edge1, float x) {
    const float t = clampf((x - edge0) / (edge1 - edge0), 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

/* Shortest way round the colour wheel, so red does not detour through green. */
static float hue_toward(float from, float to, float t) {
    float delta = to - from;

    while (delta > 180.0f) delta -= 360.0f;
    while (delta < -180.0f) delta += 360.0f;

    float result = from + delta * t;

    while (result < 0.0f) result += 360.0f;
    while (result >= 360.0f) result -= 360.0f;

    return result;
}

/* amount: -1 is deep shadow, 0 is the base colour, +1 is a lit highlight. */
static Vector3 shade(Vector3 hsv, float amount) {
    if (amount >= 0.0f) {
        hsv.x = hue_toward(hsv.x, HUE_HIGHLIGHT, 0.24f * amount);
        hsv.y *= 1.0f - 0.44f * amount;
        hsv.z += (1.0f - hsv.z) * 0.88f * amount;
    } else {
        const float depth = -amount;

        hsv.x = hue_toward(hsv.x, HUE_SHADOW, 0.22f * depth);
        hsv.y = clampf(hsv.y * (1.0f + 0.38f * depth), 0.0f, 1.0f);
        hsv.z *= 1.0f - 0.55f * depth;
    }

    return hsv;
}

static float sdf_round_rect(float px, float py, float half, float radius) {
    const float qx = fabsf(px) - (half - radius);
    const float qy = fabsf(py) - (half - radius);
    const float ax = fmaxf(qx, 0.0f);
    const float ay = fmaxf(qy, 0.0f);

    return sqrtf(ax * ax + ay * ay) + fminf(fmaxf(qx, qy), 0.0f) - radius;
}

int main(int argc, char **argv) {
    const char *out_path = argc > 1 ? argv[1] : "assets/themes/bevel/blocks.png";

    SetTraceLogLevel(LOG_WARNING);

    Image sheet = GenImageColor(CELL * PIECES, CELL, BLANK);
    const float half = (float)CELL * 0.5f - MARGIN;
    /* Light from the top left. */
    const float light_x = -0.6f;
    const float light_y = -0.8f;

    for (int piece = 0; piece < PIECES; piece++) {
        const Vector3 base = ColorToHSV(PIECE_COLORS[piece]);

        for (int y = 0; y < CELL; y++) {
            for (int x = 0; x < CELL; x++) {
                const float px = (float)x + 0.5f - (float)CELL * 0.5f;
                const float py = (float)y + 0.5f - (float)CELL * 0.5f;
                const float sd = sdf_round_rect(px, py, half, CORNER);
                const float coverage = clampf(0.5f - sd, 0.0f, 1.0f);

                if (coverage <= 0.0f) {
                    continue;
                }

                /* Surface normal from the slope of the distance field. */
                const float gx = sdf_round_rect(px + 1.0f, py, half, CORNER) -
                                 sdf_round_rect(px - 1.0f, py, half, CORNER);
                const float gy = sdf_round_rect(px, py + 1.0f, half, CORNER) -
                                 sdf_round_rect(px, py - 1.0f, half, CORNER);
                const float length = sqrtf(gx * gx + gy * gy) + 1e-5f;
                const float lambert = (gx / length) * light_x + (gy / length) * light_y;

                /* 1 along the rim, fading to 0 across the bevel into the face. */
                const float bevel = smoothstep(0.0f, 1.0f, clampf(1.0f + sd / BEVEL, 0.0f, 1.0f));
                const float face = 1.0f - bevel;

                float amount = bevel * lambert * 0.95f;

                /* Gentle top-to-bottom falloff so the face is not dead flat. */
                amount += face * (-py / half) * 0.13f;

                /* Narrow darkening right at the silhouette keeps blocks legible
                   against each other and against the well. */
                amount -= smoothstep(-2.5f, -0.4f, sd) * 0.32f;

                Vector3 hsv = shade(base, clampf(amount, -1.0f, 1.0f));
                Color color = ColorFromHSV(hsv.x, hsv.y, hsv.z);

                /* Soft wet-looking glint on the upper left of the face. */
                const float glint_x = px + 21.0f;
                const float glint_y = py + 23.0f;
                const float glint = expf(-(glint_x * glint_x + glint_y * glint_y) / 620.0f) * face;

                color.r = (unsigned char)clampf(color.r + glint * (255.0f - color.r) * 0.62f, 0.0f, 255.0f);
                color.g = (unsigned char)clampf(color.g + glint * (255.0f - color.g) * 0.62f, 0.0f, 255.0f);
                color.b = (unsigned char)clampf(color.b + glint * (255.0f - color.b) * 0.62f, 0.0f, 255.0f);
                color.a = (unsigned char)(coverage * 255.0f);

                ImageDrawPixel(&sheet, piece * CELL + x, y, color);
            }
        }
    }

    if (!ExportImage(sheet, out_path)) {
        printf("failed to write %s\n", out_path);
        return 1;
    }

    printf("wrote %s (%dx%d, %d cells of %d)\n", out_path, sheet.width, sheet.height, PIECES, CELL);
    UnloadImage(sheet);
    return 0;
}
