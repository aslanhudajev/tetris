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

void ui_rounded(Rectangle rect, float radius, Color color);
void ui_rounded_outline(Rectangle rect, float radius, float thickness, Color color);
void ui_shadow(Rectangle rect, float radius, float spread, float alpha);
void ui_panel(Rectangle rect, float radius, Color fill, Color border);

Color ui_mix(Color a, Color b, float t);

#endif
