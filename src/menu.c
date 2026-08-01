#include "menu.h"

#include "config.h"
#include "render.h"
#include "ui.h"

#include <raylib.h>
#include <string.h>

#define MAIN_BUTTON_COUNT 6
#define LEVEL_GRID_COLS 5
#define LEVEL_BUTTON_SIZE 56
#define LEVEL_BUTTON_GAP 9
#define THEME_ROW_WIDTH 380
#define THEME_ROW_HEIGHT 58
#define THEME_ROW_GAP 8

static const Color COLOR_MUTED = {132, 132, 152, 255};
static const Color COLOR_DIM = {96, 96, 116, 255};
static const Color ACCENT = {124, 196, 255, 255};

typedef struct {
    const char *label;
    const char *hint;
    Color accent;
} MainEntry;

static const MainEntry MAIN_ENTRIES[MAIN_BUTTON_COUNT] = {
    {"40 Lines", "Clear 40 lines as fast as you can", {96, 214, 255, 255}},
    {"Zen", "One speed, forever, no game over", {178, 150, 255, 255}},
    {"Marathon", "Speeds up every 10 lines", {255, 186, 92, 255}},
    {"Themes", "Change how the blocks look", {150, 220, 170, 255}},
    {"High Scores", "Best times and scores", {200, 200, 220, 255}},
    {"Quit", NULL, {200, 120, 120, 255}},
};

static Rectangle main_button_rect(int index, int window_width, int window_height) {
    const float width = 280.0f;
    const float height = 46.0f;
    const float gap = 10.0f;
    const float total = MAIN_BUTTON_COUNT * height + (MAIN_BUTTON_COUNT - 1) * gap;
    const float start_y = (float)window_height * 0.5f - total * 0.5f + 52.0f;

    return (Rectangle){
        (float)window_width * 0.5f - width * 0.5f,
        start_y + (float)index * (height + gap),
        width,
        height,
    };
}

static Rectangle level_button_rect(int index, int window_width, int window_height) {
    const int rows = (MAX_START_LEVEL + LEVEL_GRID_COLS - 1) / LEVEL_GRID_COLS;
    const int grid_width = LEVEL_GRID_COLS * LEVEL_BUTTON_SIZE + (LEVEL_GRID_COLS - 1) * LEVEL_BUTTON_GAP;
    const int grid_height = rows * LEVEL_BUTTON_SIZE + (rows - 1) * LEVEL_BUTTON_GAP;

    return (Rectangle){
        (float)(window_width / 2 - grid_width / 2 + (index % LEVEL_GRID_COLS) * (LEVEL_BUTTON_SIZE + LEVEL_BUTTON_GAP)),
        (float)(window_height / 2 - grid_height / 2 + (index / LEVEL_GRID_COLS) * (LEVEL_BUTTON_SIZE + LEVEL_BUTTON_GAP)),
        (float)LEVEL_BUTTON_SIZE,
        (float)LEVEL_BUTTON_SIZE,
    };
}

static Rectangle theme_row_rect(int index, int count, int window_width, int window_height) {
    const int total = count * THEME_ROW_HEIGHT + (count - 1) * THEME_ROW_GAP;

    return (Rectangle){
        (float)(window_width / 2 - THEME_ROW_WIDTH / 2),
        (float)(window_height / 2 - total / 2 + index * (THEME_ROW_HEIGHT + THEME_ROW_GAP)),
        (float)THEME_ROW_WIDTH,
        (float)THEME_ROW_HEIGHT,
    };
}

static Rectangle back_button_rect(int window_width, int window_height) {
    return (Rectangle){
        (float)window_width * 0.5f - 72.0f,
        (float)window_height - 88.0f,
        144.0f,
        40.0f,
    };
}

static bool clicked(Rectangle rect) {
    return CheckCollisionPointRec(GetMousePosition(), rect) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
}

static void draw_surface(Rectangle rect, bool highlighted, Color accent) {
    const bool hovered = CheckCollisionPointRec(GetMousePosition(), rect);
    const bool pressed = hovered && IsMouseButtonDown(MOUSE_BUTTON_LEFT);

    Color fill = (Color){38, 38, 50, 255};
    Color border = (Color){66, 66, 84, 255};

    if (highlighted) {
        fill = ui_mix((Color){38, 38, 50, 255}, accent, 0.18f);
        border = Fade(accent, 0.75f);
    }

    if (hovered) {
        fill = ui_mix(fill, accent, 0.28f);
        border = accent;
    }

    if (pressed) {
        fill = ui_mix(fill, BLACK, 0.2f);
    }

    if (!pressed) {
        ui_shadow(rect, 10.0f, 5.0f, 0.4f);
    }

    ui_rounded(rect, 10.0f, fill);
    ui_rounded_outline(rect, 10.0f, 1.5f, border);

    /* Thin sheen along the top edge keeps the buttons from looking like flat
       grey slabs. */
    DrawRectangleRounded(
        (Rectangle){rect.x + 3.0f, rect.y + 2.0f, rect.width - 6.0f, rect.height * 0.42f},
        0.6f,
        6,
        Fade(WHITE, hovered ? 0.07f : 0.04f)
    );
}

/* Keeps each row identifiable while the mouse is elsewhere, so the list does
   not read as six identical grey slabs. */
static void draw_accent_stripe(Rectangle rect, Color accent) {
    DrawRectangleRounded(
        (Rectangle){rect.x + 8.0f, rect.y + rect.height * 0.3f, 3.0f, rect.height * 0.4f},
        1.0f,
        4,
        accent
    );
}

static void draw_button(Rectangle rect, const char *label, float font_size, bool highlighted, Color accent) {
    draw_surface(rect, highlighted, accent);

    const Vector2 measured = ui_measure(label, font_size);
    ui_text(
        label,
        rect.x + rect.width * 0.5f - measured.x * 0.5f,
        rect.y + rect.height * 0.5f - measured.y * 0.5f,
        font_size,
        RAYWHITE
    );
}

static void draw_screen_title(const char *title, const char *subtitle, int window_width, float y) {
    ui_text_center(title, (float)window_width * 0.5f, y, 36.0f, RAYWHITE);

    if (subtitle != NULL) {
        ui_text_center(subtitle, (float)window_width * 0.5f, y + 46.0f, 15.0f, COLOR_MUTED);
    }
}

static MenuAction empty_action(void) {
    MenuAction action = {0};
    action.select_theme = -1;
    return action;
}

void menu_init(MenuState *menu) {
    memset(menu, 0, sizeof(*menu));
    menu->screen = MENU_SCREEN_MAIN;
    menu->pending_mode = GAME_MODE_MARATHON;
    menu->selected_level = 1;
}

void menu_open_main(MenuState *menu) {
    menu->screen = MENU_SCREEN_MAIN;
}

static MenuAction update_main(MenuState *menu, int window_width, int window_height) {
    MenuAction action = empty_action();

    for (int i = 0; i < MAIN_BUTTON_COUNT; i++) {
        if (!clicked(main_button_rect(i, window_width, window_height))) {
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
            menu->screen = MENU_SCREEN_LEVEL;
            break;
        case 2:
            menu->pending_mode = GAME_MODE_MARATHON;
            menu->screen = MENU_SCREEN_LEVEL;
            break;
        case 3:
            menu->screen = MENU_SCREEN_THEMES;
            break;
        case 4:
            menu->screen = MENU_SCREEN_SCORES;
            break;
        default:
            action.quit = true;
            break;
        }
    }

    return action;
}

static MenuAction update_level(MenuState *menu, int window_width, int window_height) {
    MenuAction action = empty_action();

    for (int i = 0; i < MAX_START_LEVEL; i++) {
        if (clicked(level_button_rect(i, window_width, window_height))) {
            menu->selected_level = i + 1;
            action.start_game = true;
            action.mode = menu->pending_mode;
            action.start_level = i + 1;
        }
    }

    if (clicked(back_button_rect(window_width, window_height)) || IsKeyPressed(KEY_ESCAPE)) {
        menu->screen = MENU_SCREEN_MAIN;
    }

    return action;
}

static MenuAction update_themes(MenuState *menu, const ThemeLibrary *themes, int window_width, int window_height) {
    MenuAction action = empty_action();

    for (int i = 0; i < themes->count; i++) {
        if (clicked(theme_row_rect(i, themes->count, window_width, window_height))) {
            action.select_theme = i;
        }
    }

    if (clicked(back_button_rect(window_width, window_height)) || IsKeyPressed(KEY_ESCAPE)) {
        menu->screen = MENU_SCREEN_MAIN;
    }

    return action;
}

MenuAction menu_update(MenuState *menu, const ThemeLibrary *themes, int window_width, int window_height) {
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
        if (clicked(back_button_rect(window_width, window_height)) || IsKeyPressed(KEY_ESCAPE)) {
            menu->screen = MENU_SCREEN_MAIN;
        }
        break;
    }

    return action;
}

static void draw_main(int window_width, int window_height) {
    const float center_x = (float)window_width * 0.5f;
    const float title_y = (float)window_height * 0.5f - 214.0f;
    const Vector2 title_size = ui_measure(GAME_TITLE, 58.0f);

    ui_text_shadowed(GAME_TITLE, center_x - title_size.x * 0.5f, title_y, 58.0f, RAYWHITE);
    DrawRectangleRounded(
        (Rectangle){center_x - 28.0f, title_y + title_size.y + 12.0f, 56.0f, 3.0f},
        1.0f,
        4,
        Fade(ACCENT, 0.8f)
    );
    ui_label("offline tetris", center_x - ui_measure("OFFLINE TETRIS", 12.0f).x * 0.5f - 10.0f,
             title_y + title_size.y + 26.0f, 12.0f, COLOR_DIM);

    const Vector2 mouse = GetMousePosition();
    const char *hint = NULL;

    for (int i = 0; i < MAIN_BUTTON_COUNT; i++) {
        const Rectangle rect = main_button_rect(i, window_width, window_height);
        draw_button(rect, MAIN_ENTRIES[i].label, 21.0f, false, MAIN_ENTRIES[i].accent);
        draw_accent_stripe(rect, MAIN_ENTRIES[i].accent);

        if (CheckCollisionPointRec(mouse, rect)) {
            hint = MAIN_ENTRIES[i].hint;
        }
    }

    if (hint != NULL) {
        ui_text_center(hint, center_x, (float)window_height - 58.0f, 15.0f, COLOR_MUTED);
    }
}

static void draw_level(const MenuState *menu, int window_width, int window_height) {
    const bool zen = menu->pending_mode == GAME_MODE_ZEN;
    const Color accent = zen ? (Color){178, 150, 255, 255} : (Color){255, 186, 92, 255};

    draw_screen_title(
        zen ? "Zen" : "Marathon",
        zen ? "Pick a speed. It never changes." : "Pick a starting level. It rises every 10 lines.",
        window_width,
        (float)window_height * 0.5f - 196.0f
    );

    for (int i = 0; i < MAX_START_LEVEL; i++) {
        draw_button(
            level_button_rect(i, window_width, window_height),
            TextFormat("%d", i + 1),
            21.0f,
            menu->selected_level == i + 1,
            accent
        );
    }

    draw_button(back_button_rect(window_width, window_height), "Back", 17.0f, false, ACCENT);
}

static void draw_themes(const ThemeLibrary *themes, int window_width, int window_height) {
    const Color accent = (Color){150, 220, 170, 255};

    draw_screen_title("Themes", "Block art is data driven. Drop a folder in and list it in themes.cfg.",
                      window_width, 72.0f);

    for (int i = 0; i < themes->count; i++) {
        const Theme *theme = &themes->entries[i];
        const Rectangle rect = theme_row_rect(i, themes->count, window_width, window_height);
        const bool active = themes->active == i;

        draw_surface(rect, active, accent);
        ui_text(theme->name, rect.x + 20.0f, rect.y + 10.0f, 19.0f, RAYWHITE);

        if (theme->flavor[0] != '\0') {
            ui_text(theme->flavor, rect.x + 20.0f, rect.y + 34.0f, 13.0f, COLOR_MUTED);
        }

        if (active) {
            draw_accent_stripe(rect, accent);
        }

        /* Only the active theme keeps a texture in memory, so preview the
           palette rather than the art. */
        const float swatch = 11.0f;
        float swatch_x = rect.x + rect.width - 20.0f - (swatch + 3.0f) * PIECE_L + 3.0f;

        for (int piece = PIECE_I; piece <= PIECE_L; piece++) {
            DrawRectangleRounded(
                (Rectangle){swatch_x, rect.y + rect.height * 0.5f - swatch * 0.5f, swatch, swatch},
                0.3f,
                4,
                theme->piece_colors[piece]
            );
            swatch_x += swatch + 3.0f;
        }
    }

    draw_button(back_button_rect(window_width, window_height), "Back", 17.0f, false, ACCENT);
}

static void draw_score_column(
    const char *heading,
    Color heading_color,
    const ScoreEntry *entries,
    int count,
    bool is_time,
    float x,
    float y
) {
    ui_label(heading, x, y - 26.0f, 12.0f, heading_color);

    if (count == 0) {
        ui_text("no runs yet", x, y + 4.0f, 14.0f, COLOR_DIM);
        return;
    }

    char buffer[32];

    for (int i = 0; i < count; i++) {
        const float row_y = y + (float)i * 27.0f;

        ui_text_mono(TextFormat("%d", i + 1), x, row_y + 2.0f, 14.0f, COLOR_DIM);

        if (is_time) {
            scores_format_time(entries[i].time_seconds, buffer, (int)sizeof(buffer));
            ui_text_mono(buffer, x + 26.0f, row_y, 18.0f, i == 0 ? heading_color : RAYWHITE);
        } else {
            ui_text_mono(TextFormat("%d", entries[i].score), x + 26.0f, row_y, 18.0f,
                         i == 0 ? heading_color : RAYWHITE);
            ui_text_mono(TextFormat("lv%d", entries[i].level), x + 132.0f, row_y + 3.0f, 13.0f, COLOR_DIM);
        }
    }
}

static void draw_scores(const ScoreTable *table, int window_width, int window_height) {
    draw_screen_title("High Scores", NULL, window_width, 66.0f);

    const float left_x = (float)window_width * 0.5f - 196.0f;
    const float right_x = (float)window_width * 0.5f + 24.0f;
    const float list_y = 164.0f;

    draw_score_column("40 Lines", (Color){96, 214, 255, 255}, table->sprint, table->sprint_count,
                      true, left_x, list_y);
    draw_score_column("Marathon", (Color){255, 186, 92, 255}, table->marathon, table->marathon_count,
                      false, right_x, list_y);

    draw_button(back_button_rect(window_width, window_height), "Back", 17.0f, false, ACCENT);
}

void menu_draw(
    const MenuState *menu,
    const ScoreTable *table,
    const ThemeLibrary *themes,
    int window_width,
    int window_height
) {
    render_background(themes_active(themes), window_width, window_height);

    switch (menu->screen) {
    case MENU_SCREEN_MAIN:
        draw_main(window_width, window_height);
        break;
    case MENU_SCREEN_LEVEL:
        draw_level(menu, window_width, window_height);
        break;
    case MENU_SCREEN_THEMES:
        draw_themes(themes, window_width, window_height);
        break;
    case MENU_SCREEN_SCORES:
        draw_scores(table, window_width, window_height);
        break;
    }
}
