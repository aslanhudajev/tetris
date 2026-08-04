#ifndef MENU_H
#define MENU_H

#include "game.h"
#include "scores.h"
#include "theme.h"

#include <stdbool.h>

typedef enum {
    APP_SCENE_MENU,
    APP_SCENE_PLAYING,
    APP_SCENE_QUIT,
} AppScene;

typedef enum {
    MENU_SCREEN_MAIN,
    MENU_SCREEN_LEVEL,
    MENU_SCREEN_SCORES,
    MENU_SCREEN_THEMES,
} MenuScreen;

/* Enough for the longest list any screen shows: the theme library. */
#define MENU_MAX_ROWS THEME_MAX_COUNT

typedef struct {
    MenuScreen screen;
    GameMode pending_mode;
    int selected_level;

    /* Eased 0..1 hover weight per row of the current screen, cleared on every
       screen change so a row cannot inherit the glow of whatever it replaced. */
    float hover[MENU_MAX_ROWS];
} MenuState;

typedef struct {
    bool start_game;
    bool quit;
    GameMode mode;
    int start_level;

    /* Index into the theme library the player clicked, or -1. */
    int select_theme;
} MenuAction;

void menu_init(MenuState *menu);
void menu_open_main(MenuState *menu);
MenuAction menu_update(MenuState *menu, const ThemeLibrary *themes, int window_width, int window_height);
void menu_draw(
    const MenuState *menu,
    const ScoreTable *table,
    const ThemeLibrary *themes,
    int window_width,
    int window_height
);

#endif
