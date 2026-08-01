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

typedef struct {
    MenuScreen screen;
    GameMode pending_mode;
    int selected_level;
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
