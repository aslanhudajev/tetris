#ifndef RENDER_H
#define RENDER_H

#include "game.h"
#include "theme.h"

typedef struct {
    int x;
    int y;
    int cell_size;
} BoardLayout;

typedef struct {
    const GameState *game;
    const Theme *theme;
    int window_width;
    int window_height;

    /* Zero-based high score position a finished run earned, or -1. */
    int score_rank;

    /* How far through the hold-to-quit gesture the player is, 0 to 1. */
    float quit_progress;
} RenderContext;

BoardLayout render_board_layout(int window_width, int window_height);
void render_background(const Theme *theme, int window_width, int window_height);
void render_game(const RenderContext *context);

#endif
