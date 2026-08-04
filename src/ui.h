#ifndef UI_H
#define UI_H

#include <raylib.h>

/* Fonts are loaded from the system at runtime, never bundled, so nothing here
   redistributes an Apple typeface. */
void ui_fonts_load(void);
void ui_fonts_unload(void);
const char *ui_font_name(void);

Vector2 ui_measure(const char *text, float size);
Vector2 ui_measure_mono(const char *text, float size);

void ui_text(const char *text, float x, float y, float size, Color color);
void ui_text_mono(const char *text, float x, float y, float size, Color color);
void ui_text_center(const char *text, float center_x, float y, float size, Color color);
void ui_text_shadowed(const char *text, float x, float y, float size, Color color);

/* Letter spaced small caps used for stat labels. */
void ui_label(const char *text, float x, float y, float size, Color color);
Vector2 ui_measure_label(const char *text, float size);

void ui_rounded(Rectangle rect, float radius, Color color);
void ui_rounded_outline(Rectangle rect, float radius, float thickness, Color color);

/* One device pixel, used as a separator instead of boxing content in. */
void ui_hairline(float x, float y, float width, Color color);

/* Colour fading out left to right across the rect. Carries a row's identity
   without putting a border or a stripe on it. */
void ui_wash(Rectangle rect, Color color, float alpha);

/* Pushes the backdrop away from the interface. Themes can install any image or
   shader they like back there, so text needs its own contrast floor. A light
   theme passes white here and the scrim lifts the backdrop instead. */
void ui_scrim(int width, int height, Color color);

/* Frame rate independent approach to a target, for hover transitions. */
float ui_approach(float current, float target, float rate, float dt);

Color ui_mix(Color a, Color b, float t);

#endif
