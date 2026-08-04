#include "menu.h"

#include "config.h"
#include "render.h"
#include "ui.h"

#include <raylib.h>
#include <string.h>

/* The menu is a left aligned index rather than a stack of buttons: one margin
   for everything to line up against, rows that run the full width, and
   hairlines instead of boxes. Nothing draws a border, a bevel or a drop
   shadow, which leaves colour free to mean something on its own. */
#define MARGIN 30.0f
#define HEADER_HEIGHT 96.0f
#define FOOTER_HEIGHT 54.0f
#define ROW_HEIGHT 62.0f

/* Shortest a row goes before the label itself would be cramped. Below the
   threshold in draw_row the hint is dropped rather than overflowing. */
#define ROW_COMPACT 42.0f
#define ROW_HINT_MIN 56.0f

/* Label column, clear of the mark chip in the left margin. */
#define GUTTER 50.0f
#define CHIP_WIDTH 36.0f
#define CHIP_HEIGHT 24.0f

#define MAIN_ROW_COUNT 6
#define LEVEL_GRID_COLS 5
#define LEVEL_CELL 52.0f
#define LEVEL_GAP 8.0f
#define SWATCH 9.0f
#define SWATCH_GAP 3.0f

/* The active theme's interface colours, refreshed at the top of menu_draw. Row
   accents are the only hue here; everything else is a step on the theme's own
   scale, so a light theme reads as dark type on a pale backdrop without any of
   the drawing below needing to know which it is. */
static const ThemeInk *ink = NULL;

/* Every screen indexes hover state by row, so the longest list has to fit. */
_Static_assert(MAIN_ROW_COUNT <= MENU_MAX_ROWS, "main menu needs a hover slot per row");
_Static_assert(MAX_START_LEVEL <= MENU_MAX_ROWS, "level grid needs a hover slot per cell");

typedef struct {
    /* Short tag for the chip on the left. The reference marks each row with an
       abbreviation rather than an index, which gives the column something with
       shape in it instead of a row of numerals. */
    const char *mark;
    const char *label;
    const char *hint;
    Color accent;
} MainEntry;

static const MainEntry MAIN_ENTRIES[MAIN_ROW_COUNT] = {
    {"40L", "40 Lines", "Clear forty lines against the clock", {96, 214, 255, 255}},
    {"ZEN", "Zen", "One speed, no game over", {178, 150, 255, 255}},
    {"MAR", "Marathon", "Gets faster every ten lines", {255, 186, 92, 255}},
    {"THM", "Themes", "Change how the blocks look", {150, 220, 170, 255}},
    {"REC", "High Scores", "Best times and scores", {214, 214, 214, 255}},
    {"EXT", "Quit", "Close the game", {228, 122, 122, 255}},
};

/* A monochrome theme collapses all of these onto its one accent. */
static Color accent_of(Color preferred) {
    return ink->accent_is_override ? ink->accent : preferred;
}

/* Layout ------------------------------------------------------------------ */

static float content_top(void) {
    return HEADER_HEIGHT;
}

static float content_bottom(int window_height) {
    return (float)window_height - FOOTER_HEIGHT;
}

/* Rows give up height before they give up fitting. Past the point where the
   hint line no longer has room they drop it and keep only the label, which is
   what lets the theme list grow without needing to scroll. */
static float row_height(int count, int window_height) {
    const float span = content_bottom(window_height) - content_top();
    const float fitted = span / (float)(count > 0 ? count : 1);

    if (fitted > ROW_HEIGHT) {
        return ROW_HEIGHT;
    }

    return fitted < ROW_COMPACT ? ROW_COMPACT : fitted;
}

static Rectangle row_rect(int index, int count, int window_width, int window_height) {
    const float top = content_top();
    const float span = content_bottom(window_height) - top;
    const float height = row_height(count, window_height);
    float start = top + (span - (float)count * height) * 0.5f;

    if (start < top) {
        start = top;
    }

    return (Rectangle){
        MARGIN,
        start + (float)index * height,
        (float)window_width - MARGIN * 2.0f,
        height,
    };
}

static Rectangle level_rect(int index, int window_width, int window_height) {
    const int rows = (MAX_START_LEVEL + LEVEL_GRID_COLS - 1) / LEVEL_GRID_COLS;
    const float grid_width = LEVEL_GRID_COLS * LEVEL_CELL + (LEVEL_GRID_COLS - 1) * LEVEL_GAP;
    const float grid_height = (float)rows * LEVEL_CELL + (float)(rows - 1) * LEVEL_GAP;
    const float top =
        content_top() + (content_bottom(window_height) - content_top() - grid_height) * 0.5f;

    return (Rectangle){
        (float)window_width * 0.5f - grid_width * 0.5f +
            (float)(index % LEVEL_GRID_COLS) * (LEVEL_CELL + LEVEL_GAP),
        top + (float)(index / LEVEL_GRID_COLS) * (LEVEL_CELL + LEVEL_GAP),
        LEVEL_CELL,
        LEVEL_CELL,
    };
}

/* Sits in the header where the wordmark is on the home screen. */
static Rectangle back_rect(void) {
    return (Rectangle){MARGIN - 10.0f, 24.0f, 112.0f, 30.0f};
}

/* Input ------------------------------------------------------------------- */

static bool hovering(Rectangle rect) {
    return CheckCollisionPointRec(GetMousePosition(), rect);
}

static bool clicked(Rectangle rect) {
    return hovering(rect) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
}

static float menu_delta(void) {
    const float dt = GetFrameTime();
    return dt > MAX_FRAME_DELTA ? MAX_FRAME_DELTA : dt;
}

static void set_screen(MenuState *menu, MenuScreen screen) {
    menu->screen = screen;
    memset(menu->hover, 0, sizeof(menu->hover));
}

static void track_hover(MenuState *menu, int index, bool is_hovered) {
    if (index < 0 || index >= MENU_MAX_ROWS) {
        return;
    }

    menu->hover[index] =
        ui_approach(menu->hover[index], is_hovered ? 1.0f : 0.0f, 15.0f, menu_delta());
}

static float hover_of(const MenuState *menu, int index) {
    return (index >= 0 && index < MENU_MAX_ROWS) ? menu->hover[index] : 0.0f;
}

/* Drawing ----------------------------------------------------------------- */

static void draw_chevron(float x, float y, float half, float thickness, Color color) {
    DrawLineEx((Vector2){x, y - half}, (Vector2){x + half * 0.72f, y}, thickness, color);
    DrawLineEx((Vector2){x + half * 0.72f, y}, (Vector2){x, y + half}, thickness, color);
}

static void draw_chevron_left(float x, float y, float half, float thickness, Color color) {
    DrawLineEx((Vector2){x + half * 0.72f, y - half}, (Vector2){x, y}, thickness, color);
    DrawLineEx((Vector2){x, y}, (Vector2){x + half * 0.72f, y + half}, thickness, color);
}

static void draw_home_header(int window_width) {
    ui_label(GAME_TITLE, MARGIN, 46.0f, 25.0f, ink->ink);
    ui_hairline(MARGIN, HEADER_HEIGHT - 8.0f, (float)window_width - MARGIN * 2.0f, ink->hairline);
}

static void draw_screen_header(int window_width, const char *title) {
    const Color tone = hovering(back_rect()) ? ink->ink : ink->muted;

    draw_chevron_left(MARGIN + 1.0f, 38.0f, 4.0f, 1.6f, tone);
    ui_label("back", MARGIN + 13.0f, 33.0f, 9.0f, tone);
    ui_label(title, MARGIN, 56.0f, 21.0f, ink->ink);
    ui_hairline(MARGIN, HEADER_HEIGHT - 8.0f, (float)window_width - MARGIN * 2.0f, ink->hairline);
}

static void draw_footer(int window_width, int window_height, const char *left, const char *right) {
    const float y = content_bottom(window_height);

    ui_hairline(MARGIN, y, (float)window_width - MARGIN * 2.0f, ink->hairline);

    if (left != NULL) {
        ui_label(left, MARGIN, y + 20.0f, 9.0f, ink->dim);
    }

    if (right != NULL) {
        ui_label(right, (float)window_width - MARGIN - ui_measure_label(right, 9.0f).x, y + 20.0f,
                 9.0f, ink->dim);
    }
}

/* One row of the index. `weight` is the eased hover value and `marked` holds a
   row lit when the pointer is elsewhere, for the current selection. */
static void draw_row(
    Rectangle rect,
    const char *mark,
    const char *label,
    const char *hint,
    Color accent,
    float weight,
    bool marked
) {
    const float lit = (marked && weight < 0.55f) ? 0.55f : weight;

    /* Each row keeps a trace of its colour with the pointer elsewhere, so the
       list reads as six distinct things rather than six identical lines. It
       decays well before the right edge: a bar filled end to end is what made
       the old buttons look heavy. */
    Rectangle wash = rect;
    wash.width *= 0.72f;
    ui_wash(wash, accent, 0.045f + 0.19f * lit);

    ui_hairline(rect.x, rect.y + rect.height, rect.width, ink->hairline);

    const float indent = 7.0f * weight;

    /* Without a mark the label runs to the left edge, flush with the header
       above it. */
    const float lead = mark != NULL ? GUTTER : 0.0f;

    if (mark != NULL) {
        const Rectangle chip = {
            rect.x + indent,
            rect.y + rect.height * 0.5f - CHIP_HEIGHT * 0.5f,
            CHIP_WIDTH,
            CHIP_HEIGHT,
        };

        /* Square, and it fills as the row lights: the mark inverts to dark on
           the accent rather than the accent brightening in place. */
        DrawRectangleRec(chip, Fade(accent, 0.14f + 0.80f * lit));

        const Vector2 mark_size = ui_measure_mono(mark, 12.0f);

        ui_text_mono(
            mark,
            chip.x + chip.width * 0.5f - mark_size.x * 0.5f,
            chip.y + chip.height * 0.5f - mark_size.y * 0.5f,
            12.0f,
            ui_mix(accent, ink->chip_ink, lit)
        );
    }

    const bool room_for_hint = hint != NULL && rect.height >= ROW_HINT_MIN;
    const Color label_tone = ui_mix(ink->ink_soft, ink->ink, weight);

    if (room_for_hint) {
        ui_text(label, rect.x + lead + indent, rect.y + 13.0f, 23.0f, label_tone);
        ui_label(hint, rect.x + lead + 1.0f + indent, rect.y + 43.0f, 9.0f,
                 ui_mix(ink->dim, ink->muted, lit));
    } else {
        /* Nothing below it, so the label centres on the row instead of sitting
           where it would if a hint were coming. */
        ui_text(label, rect.x + lead + indent, rect.y + rect.height * 0.5f - 11.0f, 21.0f,
                label_tone);
    }

    if (weight > 0.01f) {
        draw_chevron(rect.x + rect.width - 14.0f + 4.0f * weight, rect.y + rect.height * 0.5f, 4.5f,
                     1.7f, Fade(accent, weight));
    }
}

static void draw_level_cell(Rectangle rect, int level, bool selected, float weight, Color accent) {
    Color fill = Fade(ink->ink, 0.05f + 0.06f * weight);
    Color text = ui_mix(ink->ink_soft, ink->ink, weight);

    if (selected) {
        fill = Fade(accent, 0.92f);
        text = ink->chip_ink;
    }

    DrawRectangleRec(rect, fill);

    if (!selected && weight > 0.01f) {
        DrawRectangleLinesEx(rect, 1.0f, Fade(accent, 0.5f * weight));
    }

    const char *value = TextFormat("%d", level);
    const Vector2 size = ui_measure(value, 18.0f);

    ui_text(value, rect.x + rect.width * 0.5f - size.x * 0.5f,
            rect.y + rect.height * 0.5f - size.y * 0.5f, 18.0f, text);
}

static MenuAction empty_action(void) {
    MenuAction action = {0};
    action.select_theme = -1;
    return action;
}

/* State ------------------------------------------------------------------- */

void menu_init(MenuState *menu) {
    memset(menu, 0, sizeof(*menu));
    menu->screen = MENU_SCREEN_MAIN;
    menu->pending_mode = GAME_MODE_MARATHON;
    menu->selected_level = 1;
}

void menu_open_main(MenuState *menu) {
    set_screen(menu, MENU_SCREEN_MAIN);
}

static MenuAction update_main(MenuState *menu, int window_width, int window_height) {
    MenuAction action = empty_action();

    for (int i = 0; i < MAIN_ROW_COUNT; i++) {
        const Rectangle rect = row_rect(i, MAIN_ROW_COUNT, window_width, window_height);
        track_hover(menu, i, hovering(rect));

        if (!clicked(rect)) {
            continue;
        }

        switch (i) {
        case 0:
            action.start_game = true;
            action.mode = GAME_MODE_SPRINT;
            action.start_level = 1;
            break;
        case 1:
            menu->pending_mode = GAME_MODE_ZEN;
            set_screen(menu, MENU_SCREEN_LEVEL);
            break;
        case 2:
            menu->pending_mode = GAME_MODE_MARATHON;
            set_screen(menu, MENU_SCREEN_LEVEL);
            break;
        case 3:
            set_screen(menu, MENU_SCREEN_THEMES);
            break;
        case 4:
            set_screen(menu, MENU_SCREEN_SCORES);
            break;
        default:
            action.quit = true;
            break;
        }

        /* The screen may have changed, so the remaining rects are stale. */
        break;
    }

    return action;
}

static bool leaving(MenuState *menu) {
    if (clicked(back_rect()) || IsKeyPressed(KEY_ESCAPE)) {
        set_screen(menu, MENU_SCREEN_MAIN);
        return true;
    }

    return false;
}

static MenuAction update_level(MenuState *menu, int window_width, int window_height) {
    MenuAction action = empty_action();

    if (leaving(menu)) {
        return action;
    }

    for (int i = 0; i < MAX_START_LEVEL; i++) {
        const Rectangle rect = level_rect(i, window_width, window_height);
        track_hover(menu, i, hovering(rect));

        if (clicked(rect)) {
            menu->selected_level = i + 1;
            action.start_game = true;
            action.mode = menu->pending_mode;
            action.start_level = i + 1;
        }
    }

    return action;
}

static MenuAction update_themes(
    MenuState *menu,
    const ThemeLibrary *themes,
    int window_width,
    int window_height
) {
    MenuAction action = empty_action();

    if (leaving(menu)) {
        return action;
    }

    for (int i = 0; i < themes->count; i++) {
        const Rectangle rect = row_rect(i, themes->count, window_width, window_height);
        track_hover(menu, i, hovering(rect));

        if (clicked(rect)) {
            action.select_theme = i;
        }
    }

    return action;
}

MenuAction menu_update(
    MenuState *menu,
    const ThemeLibrary *themes,
    int window_width,
    int window_height
) {
    MenuAction action = empty_action();

    switch (menu->screen) {
    case MENU_SCREEN_MAIN:
        action = update_main(menu, window_width, window_height);
        break;
    case MENU_SCREEN_LEVEL:
        action = update_level(menu, window_width, window_height);
        break;
    case MENU_SCREEN_THEMES:
        action = update_themes(menu, themes, window_width, window_height);
        break;
    case MENU_SCREEN_SCORES:
        leaving(menu);
        break;
    }

    return action;
}

/* Screens ----------------------------------------------------------------- */

static void draw_main(const MenuState *menu, int window_width, int window_height) {
    draw_home_header(window_width);

    for (int i = 0; i < MAIN_ROW_COUNT; i++) {
        const Rectangle rect = row_rect(i, MAIN_ROW_COUNT, window_width, window_height);

        if (i == 0) {
            ui_hairline(rect.x, rect.y, rect.width, ink->hairline);
        }

        draw_row(rect, MAIN_ENTRIES[i].mark, MAIN_ENTRIES[i].label, MAIN_ENTRIES[i].hint,
                 accent_of(MAIN_ENTRIES[i].accent), hover_of(menu, i), false);
    }

    /* No footer here. The rows are the whole screen, and the only thing that
       ever sat down there was a caption for a setting two rows above it. */
}

static void draw_level(const MenuState *menu, int window_width, int window_height) {
    const bool zen = menu->pending_mode == GAME_MODE_ZEN;
    const Color accent =
        accent_of(zen ? (Color){178, 150, 255, 255} : (Color){255, 186, 92, 255});

    draw_screen_header(window_width, zen ? "Zen" : "Marathon");

    for (int i = 0; i < MAX_START_LEVEL; i++) {
        draw_level_cell(level_rect(i, window_width, window_height), i + 1,
                        menu->selected_level == i + 1, hover_of(menu, i), accent);
    }

    /* The main menu row already explains what each mode does. */
    draw_footer(window_width, window_height, NULL, "esc to go back");
}

static void draw_themes(
    const MenuState *menu,
    const ThemeLibrary *themes,
    int window_width,
    int window_height
) {
    draw_screen_header(window_width, "Themes");

    for (int i = 0; i < themes->count; i++) {
        const Theme *theme = &themes->entries[i];
        const Rectangle rect = row_rect(i, themes->count, window_width, window_height);
        const bool active = themes->active == i;
        const float weight = hover_of(menu, i);

        if (i == 0) {
            ui_hairline(rect.x, rect.y, rect.width, ink->hairline);
        }

        /* A theme's own I piece colour tints the row that selects it. */
        const Color accent = theme->piece_colors[PIECE_I];
        const char *hint = theme->flavor;

        if (active) {
            hint = theme->flavor[0] != '\0' ? TextFormat("active  -  %s", theme->flavor) : "active";
        }

        /* No mark: an abbreviation of a theme's own name says nothing the name
           beside it does not, and the swatches already carry its colour. */
        draw_row(rect, NULL, theme->name, hint[0] != '\0' ? hint : NULL, accent, weight, active);

        /* Only the active theme keeps its art in memory, so preview the
           palette rather than the block sheet. */
        const float pieces = (float)(PIECE_L - PIECE_I + 1);
        const float strip = pieces * SWATCH + (pieces - 1.0f) * SWATCH_GAP;
        float x = rect.x + rect.width - 30.0f - strip;

        for (int piece = PIECE_I; piece <= PIECE_L; piece++) {
            const float alpha = active ? 1.0f : 0.45f + 0.4f * weight;

            DrawRectangleRec((Rectangle){x, rect.y + rect.height * 0.5f - SWATCH * 0.5f, SWATCH, SWATCH},
                             Fade(theme->piece_colors[piece], alpha));
            x += SWATCH + SWATCH_GAP;
        }
    }

    draw_footer(window_width, window_height, NULL, "esc to go back");
}

static void draw_score_column(
    const char *heading,
    Color accent,
    const ScoreEntry *entries,
    int count,
    bool is_time,
    float x,
    float width,
    float y
) {
    ui_label(heading, x, y, 9.5f, accent);
    ui_hairline(x, y + 18.0f, width, ink->hairline);

    if (count == 0) {
        ui_label("no runs yet", x, y + 32.0f, 9.0f, ink->dim);
        return;
    }

    char buffer[32];

    for (int i = 0; i < count; i++) {
        const float row_y = y + 32.0f + (float)i * 26.0f;
        const Color tone = i == 0 ? accent : ink->ink_soft;

        ui_text_mono(TextFormat("%d", i + 1), x, row_y + 3.0f, 11.0f, ink->dim);

        if (is_time) {
            scores_format_time(entries[i].time_seconds, buffer, (int)sizeof(buffer));
            ui_text_mono(buffer, x + 22.0f, row_y, 17.0f, tone);
        } else {
            ui_text_mono(TextFormat("%d", entries[i].score), x + 22.0f, row_y, 17.0f, tone);

            const char *level = TextFormat("lv%d", entries[i].level);
            ui_text_mono(level, x + width - ui_measure_mono(level, 11.0f).x, row_y + 4.0f, 11.0f, ink->dim);
        }
    }
}

static void draw_scores(const ScoreTable *table, int window_width, int window_height) {
    draw_screen_header(window_width, "High Scores");

    const float full = (float)window_width - MARGIN * 2.0f;
    const float column = full * 0.44f;
    const float top = content_top() + 26.0f;

    draw_score_column("40 Lines", accent_of((Color){96, 214, 255, 255}), table->sprint, table->sprint_count,
                      true, MARGIN, column, top);
    draw_score_column("Marathon", accent_of((Color){255, 186, 92, 255}), table->marathon, table->marathon_count,
                      false, MARGIN + full - column, column, top);

    draw_footer(window_width, window_height, NULL, "esc to go back");
}

void menu_draw(
    const MenuState *menu,
    const ScoreTable *table,
    const ThemeLibrary *themes,
    int window_width,
    int window_height
) {
    const Theme *active = themes_active(themes);

    ink = &active->ink;

    render_background(active, window_width, window_height);
    ui_scrim(window_width, window_height, ink->scrim);

    switch (menu->screen) {
    case MENU_SCREEN_MAIN:
        draw_main(menu, window_width, window_height);
        break;
    case MENU_SCREEN_LEVEL:
        draw_level(menu, window_width, window_height);
        break;
    case MENU_SCREEN_THEMES:
        draw_themes(menu, themes, window_width, window_height);
        break;
    case MENU_SCREEN_SCORES:
        draw_scores(table, window_width, window_height);
        break;
    }
}
