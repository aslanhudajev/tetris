#ifndef GAME_H
#define GAME_H

#include "config.h"

#include <stdbool.h>

typedef enum {
    PIECE_NONE = 0,
    PIECE_I,
    PIECE_O,
    PIECE_T,
    PIECE_S,
    PIECE_Z,
    PIECE_J,
    PIECE_L,
} PieceType;

typedef enum {
    GAME_MODE_SPRINT,
    GAME_MODE_ZEN,
    GAME_MODE_MARATHON,
    GAME_MODE_COUNT,
} GameMode;

typedef struct {
    int x;
    int y;
} PieceCell;

typedef struct {
    PieceType cells[BOARD_TOTAL_ROWS][BOARD_COLS];

    GameMode mode;
    int start_level;

    PieceType current;
    int rotation;
    int piece_x;
    int piece_y;

    PieceType hold;
    bool hold_used;
    PieceType next_queue[NEXT_QUEUE_SIZE];
    PieceType bag[7];
    int bag_index;

    int score;
    int lines_cleared;
    int level;
    int combo;
    bool back_to_back;

    float elapsed_seconds;

    float gravity_timer;
    float gravity_interval;
    float lock_timer;
    int lock_resets;
    bool grounded;

    bool game_over;
    bool complete;
    bool paused;
} GameState;

void game_start(GameState *game, GameMode mode, int start_level);
void game_restart(GameState *game);
void game_update(GameState *game, float dt);

bool game_move(GameState *game, int dx, int dy);
bool game_rotate(GameState *game, int direction);
bool game_soft_drop(GameState *game);
void game_hard_drop(GameState *game);
bool game_hold(GameState *game);

bool game_is_finished(const GameState *game);
int game_lines_remaining(const GameState *game);
const char *game_mode_name(GameMode mode);
float game_gravity_for_level(int level);

PieceType game_board_cell(const GameState *game, int col, int row);
int game_ghost_y(const GameState *game);
const PieceCell *game_piece_shape(PieceType piece, int rotation);

#endif
