#include "menu.h"

#include "config.h"

#include <raylib.h>
#include <string.h>

#define MAIN_BUTTON_COUNT 6
#define LEVEL_GRID_COLS 5
#define LEVEL_BUTTON_SIZE 56
#define LEVEL_BUTTON_GAP 8
#define THEME_ROW_WIDTH 360
#define THEME_ROW_HEIGHT 54
#define THEME_ROW_GAP 8

typedef struct {
    const char *label;
    const char *hint;
} MainEntry;

static const MainEntry MAIN_ENTRIES[MAIN_BUTTON_COUNT] = {
    {"40 Lines", "Clear 40 lines as fast as you can"},
    {"Zen", "One speed, forever, no game over"},
    {"Marathon", "Speeds up every 10 lines"},
    {"Themes", "Change how the blocks look"},
    {"High Scores", "Best times and scores"},
    {"Quit", NULL},
};

static Rectangle main_button_rect(int index, int window_width, int window_height) {
    const float width = 260.0f;
    const float height = 44.0f;
    const float gap = 10.0f;
    const float total = MAIN_BUTTON_COUNT * height + (MAIN_BUTTON_COUNT - 1) * gap;
    const float start_y = (float)window_height * 0.5f - total * 0.5f + 50.0f;

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
    const int origin_x = window_width / 2 - grid_width / 2;
    const int origin_y = window_height / 2 - grid_height / 2;

    return (Rectangle){
        (float)(origin_x + (index % LEVEL_GRID_COLS) * (LEVEL_BUTTON_SIZE + LEVEL_BUTTON_GAP)),
        (float)(origin_y + (index / LEVEL_GRID_COLS) * (LEVEL_BUTTON_SIZE + LEVEL_BUTTON_GAP)),
        (float)LEVEL_BUTTON_SIZE,
        (float)LEVEL_BUTTON_SIZE,
    };
}

static Rectangle theme_row_rect(int index, int count, int window_width, int window_height) {
    const int total = count * THEME_ROW_HEIGHT + (count - 1) * THEME_ROW_GAP;
    const int origin_y = window_height / 2 - total / 2;

    return (Rectangle){
        (float)(window_width / 2 - THEME_ROW_WIDTH / 2),
        (float)(origin_y + index * (THEME_ROW_HEIGHT + THEME_ROW_GAP)),
        (float)THEME_ROW_WIDTH,
        (float)THEME_ROW_HEIGHT,
    };
}

static Rectangle back_button_rect(int window_width, int window_height) {
    return (Rectangle){
        (float)window_width * 0.5f - 70.0f,
        (float)window_height - 90.0f,
        140.0f,
        40.0f,
    };
}

static bool clicked(Rectangle rect) {
    return CheckCollisionPointRec(GetMousePosition(), rect) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
}

static void draw_panel(Rectangle rect, bool highlighted) {
    const bool hovered = CheckCollisionPointRec(GetMousePosition(), rect);
    Color fill = (Color){45, 45, 58, 255};
    Color border = (Color){90, 90, 110, 255};

    if (highlighted) {
        fill = (Color){58, 58, 84, 255};
        border = (Color){130, 130, 170, 255};
    }

    if (hovered) {
        fill = (Color){74, 74, 96, 255};
        border = RAYWHITE;
    }

    DrawRectangleRec(rect, fill);
    DrawRectangleLinesEx(rect, 2.0f, border);
}

static void draw_button(Rectangle rect, const char *label, int font_size, bool highlighted) {
    draw_panel(rect, highlighted);

    const int text_width = MeasureText(label, font_size);
    DrawText(
        label,
        (int)(rect.x + rect.width * 0.5f) - text_width / 2,
        (int)(rect.y + rect.height * 0.5f) - font_size / 2,
        font_size,
        RAYWHITE
    );
}

static void draw_centered(const char *text, int y, int font_size, Color color, int window_width) {
    DrawText(text, window_width / 2 - MeasureText(text, font_size) / 2, y, font_size, color);
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

static MenuAction update_themes(
    MenuState *menu,
    const ThemeLibrary *themes,
    int window_width,
    int window_height
) {
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
    draw_centered(GAME_TITLE, window_height / 2 - 200, 52, RAYWHITE, window_width);
    draw_centered("offline tetris", window_height / 2 - 158, 16, (Color){120, 120, 140, 255}, window_width);

    const Vector2 mouse = GetMousePosition();
    const char *hint = NULL;

    for (int i = 0; i < MAIN_BUTTON_COUNT; i++) {
        const Rectangle rect = main_button_rect(i, window_width, window_height);
        draw_button(rect, MAIN_ENTRIES[i].label, 21, false);

        if (CheckCollisionPointRec(mouse, rect)) {
            hint = MAIN_ENTRIES[i].hint;
        }
    }

    if (hint != NULL) {
        draw_centered(hint, window_height - 60, 15, (Color){130, 130, 150, 255}, window_width);
    }
}

static void draw_level(const MenuState *menu, int window_width, int window_height) {
    const bool zen = menu->pending_mode == GAME_MODE_ZEN;

    draw_centered(zen ? "Zen" : "Marathon", window_height / 2 - 190, 40, RAYWHITE, window_width);
    draw_centered(
        zen ? "Pick a speed. It never changes." : "Pick a starting level. It rises every 10 lines.",
        window_height / 2 - 140,
        16,
        (Color){130, 130, 150, 255},
        window_width
    );

    for (int i = 0; i < MAX_START_LEVEL; i++) {
        draw_button(
            level_button_rect(i, window_width, window_height),
            TextFormat("%d", i + 1),
            20,
            menu->selected_level == i + 1
        );
    }

    draw_button(back_button_rect(window_width, window_height), "Back", 18, false);
}

static void draw_themes(const ThemeLibrary *themes, int window_width, int window_height) {
    draw_centered("Themes", 80, 38, RAYWHITE, window_width);
    draw_centered(
        "Block art is one grayscale image, tinted per piece.",
        128,
        15,
        (Color){130, 130, 150, 255},
        window_width
    );

    for (int i = 0; i < themes->count; i++) {
        const Theme *theme = &themes->entries[i];
        const Rectangle rect = theme_row_rect(i, themes->count, window_width, window_height);
        const bool active = themes->active == i;

        draw_panel(rect, active);
        DrawText(theme->name, (int)rect.x + 16, (int)rect.y + 11, 19, RAYWHITE);

        if (theme->flavor[0] != '\0') {
            DrawText(theme->flavor, (int)rect.x + 16, (int)rect.y + 33, 12, (Color){160, 160, 180, 255});
        }

        if (active) {
            const int tag_width = MeasureText("ACTIVE", 12);
            DrawText(
                "ACTIVE",
                (int)(rect.x + rect.width) - tag_width - 16,
                (int)rect.y + 21,
                12,
                (Color){255, 210, 120, 255}
            );
        }
    }

    draw_button(back_button_rect(window_width, window_height), "Back", 18, false);
}

static void draw_scores(const ScoreTable *table, int window_width, int window_height) {
    char buffer[32];

    draw_centered("High Scores", 70, 38, RAYWHITE, window_width);

    const int left_x = window_width / 2 - 190;
    const int right_x = window_width / 2 + 20;
    const int list_y = 150;

    DrawText("40 LINES", left_x, list_y - 30, 18, (Color){140, 200, 255, 255});
    DrawText("MARATHON", right_x, list_y - 30, 18, (Color){255, 190, 130, 255});

    if (table->sprint_count == 0) {
        DrawText("no runs yet", left_x, list_y, 14, (Color){110, 110, 130, 255});
    }

    for (int i = 0; i < table->sprint_count; i++) {
        scores_format_time(table->sprint[i].time_seconds, buffer, (int)sizeof(buffer));
        DrawText(TextFormat("%2d.", i + 1), left_x, list_y + i * 26, 16, (Color){110, 110, 130, 255});
        DrawText(buffer, left_x + 36, list_y + i * 26, 16, RAYWHITE);
    }

    if (table->marathon_count == 0) {
        DrawText("no runs yet", right_x, list_y, 14, (Color){110, 110, 130, 255});
    }

    for (int i = 0; i < table->marathon_count; i++) {
        const ScoreEntry *entry = &table->marathon[i];
        DrawText(TextFormat("%2d.", i + 1), right_x, list_y + i * 26, 16, (Color){110, 110, 130, 255});
        DrawText(TextFormat("%d", entry->score), right_x + 36, list_y + i * 26, 16, RAYWHITE);
        DrawText(
            TextFormat("lv%d", entry->level),
            right_x + 130,
            list_y + i * 26 + 2,
            13,
            (Color){110, 110, 130, 255}
        );
    }

    draw_button(back_button_rect(window_width, window_height), "Back", 18, false);
}

void menu_draw(
    const MenuState *menu,
    const ScoreTable *table,
    const ThemeLibrary *themes,
    int window_width,
    int window_height
) {
    ClearBackground(themes_active(themes)->background);

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
