#include "game.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#define PIECE_COUNT 7
#define SPAWN_X 3
/* Spawn low enough in the buffer that the piece is visible on the frame it
   appears instead of drifting in from off-screen. */
#define SPAWN_Y (BOARD_BUFFER_ROWS - 1)

static const PieceCell PIECE_SHAPES[PIECE_COUNT][4][4] = {
    /* I */
    {
        {{0, 1}, {1, 1}, {2, 1}, {3, 1}},
        {{2, 0}, {2, 1}, {2, 2}, {2, 3}},
        {{0, 2}, {1, 2}, {2, 2}, {3, 2}},
        {{1, 0}, {1, 1}, {1, 2}, {1, 3}},
    },
    /* O */
    {
        {{1, 0}, {2, 0}, {1, 1}, {2, 1}},
        {{1, 0}, {2, 0}, {1, 1}, {2, 1}},
        {{1, 0}, {2, 0}, {1, 1}, {2, 1}},
        {{1, 0}, {2, 0}, {1, 1}, {2, 1}},
    },
    /* T */
    {
        {{1, 0}, {0, 1}, {1, 1}, {2, 1}},
        {{1, 0}, {1, 1}, {2, 1}, {1, 2}},
        {{0, 1}, {1, 1}, {2, 1}, {1, 2}},
        {{1, 0}, {0, 1}, {1, 1}, {1, 2}},
    },
    /* S */
    {
        {{1, 0}, {2, 0}, {0, 1}, {1, 1}},
        {{1, 0}, {1, 1}, {2, 1}, {2, 2}},
        {{1, 1}, {2, 1}, {0, 2}, {1, 2}},
        {{0, 0}, {0, 1}, {1, 1}, {1, 2}},
    },
    /* Z */
    {
        {{0, 0}, {1, 0}, {1, 1}, {2, 1}},
        {{2, 0}, {1, 1}, {2, 1}, {1, 2}},
        {{0, 1}, {1, 1}, {1, 2}, {2, 2}},
        {{1, 0}, {0, 1}, {1, 1}, {0, 2}},
    },
    /* J */
    {
        {{0, 0}, {0, 1}, {1, 1}, {2, 1}},
        {{1, 0}, {2, 0}, {1, 1}, {1, 2}},
        {{0, 1}, {1, 1}, {2, 1}, {2, 2}},
        {{1, 0}, {1, 1}, {0, 2}, {1, 2}},
    },
    /* L */
    {
        {{2, 0}, {0, 1}, {1, 1}, {2, 1}},
        {{1, 0}, {1, 1}, {1, 2}, {2, 2}},
        {{0, 1}, {1, 1}, {2, 1}, {0, 2}},
        {{0, 0}, {1, 0}, {1, 1}, {1, 2}},
    },
};

static int normalize_rotation(int rotation) {
    return ((rotation % 4) + 4) % 4;
}

const PieceCell *game_piece_shape(PieceType piece, int rotation) {
    if (piece == PIECE_NONE) {
        return NULL;
    }

    return PIECE_SHAPES[(int)piece - 1][normalize_rotation(rotation)];
}

const char *game_mode_name(GameMode mode) {
    switch (mode) {
    case GAME_MODE_SPRINT: return "40 LINES";
    case GAME_MODE_ZEN: return "ZEN";
    case GAME_MODE_MARATHON: return "MARATHON";
    default: return "";
    }
}

/* Tetris Guideline speed curve: seconds per row at a given level, as used by
   Tetris Worlds and every guideline game since. */
float game_gravity_for_level(int level) {
    if (level < 1) {
        level = 1;
    }

    if (level > GRAVITY_CURVE_MAX_LEVEL) {
        level = GRAVITY_CURVE_MAX_LEVEL;
    }

    const double base = 0.8 - ((double)(level - 1) * 0.007);
    return (float)pow(base, (double)(level - 1));
}

/* Fixed-goal progression: every 10 lines is a level, and a run started at
   level N still needs N*10 total lines before reaching N+1. */
static int level_for_state(const GameState *game) {
    if (game->mode != GAME_MODE_MARATHON) {
        return game->start_level;
    }

    const int from_lines = 1 + game->lines_cleared / LINES_PER_LEVEL;
    return from_lines > game->start_level ? from_lines : game->start_level;
}

static void apply_level(GameState *game) {
    game->level = level_for_state(game);
    game->gravity_interval = game_gravity_for_level(game->level);
}

static bool collides(const GameState *game, PieceType piece, int rotation, int px, int py) {
    const PieceCell *shape = game_piece_shape(piece, rotation);

    if (shape == NULL) {
        return true;
    }

    for (int i = 0; i < 4; i++) {
        const int x = px + shape[i].x;
        const int y = py + shape[i].y;

        if (x < 0 || x >= BOARD_COLS || y >= BOARD_TOTAL_ROWS) {
            return true;
        }

        if (y >= 0 && game->cells[y][x] != PIECE_NONE) {
            return true;
        }
    }

    return false;
}

static bool piece_is_grounded(const GameState *game) {
    return collides(game, game->current, game->rotation, game->piece_x, game->piece_y + 1);
}

static void refill_bag(GameState *game) {
    for (int i = 0; i < PIECE_COUNT; i++) {
        game->bag[i] = (PieceType)(i + 1);
    }

    for (int i = PIECE_COUNT - 1; i > 0; i--) {
        const int j = rand() % (i + 1);
        const PieceType tmp = game->bag[i];
        game->bag[i] = game->bag[j];
        game->bag[j] = tmp;
    }

    game->bag_index = 0;
}

static PieceType take_from_bag(GameState *game) {
    if (game->bag_index >= PIECE_COUNT) {
        refill_bag(game);
    }

    return game->bag[game->bag_index++];
}

static PieceType pop_next_piece(GameState *game) {
    const PieceType piece = game->next_queue[0];

    for (int i = 0; i < NEXT_QUEUE_SIZE - 1; i++) {
        game->next_queue[i] = game->next_queue[i + 1];
    }

    game->next_queue[NEXT_QUEUE_SIZE - 1] = take_from_bag(game);
    return piece;
}

static void place_piece(GameState *game, PieceType piece) {
    game->current = piece;
    game->rotation = 0;
    game->piece_x = SPAWN_X;
    game->piece_y = SPAWN_Y;
    game->gravity_timer = 0.0f;
    game->lock_timer = 0.0f;
    game->lock_resets = 0;
    game->grounded = false;

    if (!collides(game, game->current, game->rotation, game->piece_x, game->piece_y)) {
        return;
    }

    /* Zen has no fail state: sweep the board and keep the run going. */
    if (game->mode == GAME_MODE_ZEN) {
        memset(game->cells, 0, sizeof(game->cells));
        game->combo = -1;
        game->back_to_back = false;
        return;
    }

    game->game_over = true;
}

static void spawn_next(GameState *game) {
    game->hold_used = false;
    place_piece(game, pop_next_piece(game));
}

static void write_piece_to_board(GameState *game) {
    const PieceCell *shape = game_piece_shape(game->current, game->rotation);

    for (int i = 0; i < 4; i++) {
        const int x = game->piece_x + shape[i].x;
        const int y = game->piece_y + shape[i].y;

        if (y >= 0 && y < BOARD_TOTAL_ROWS && x >= 0 && x < BOARD_COLS) {
            game->cells[y][x] = game->current;
        }
    }
}

/* Single downward compaction pass: copy surviving rows to the bottom, then
   blank whatever is left at the top. */
static int clear_full_lines(GameState *game) {
    int write_row = BOARD_TOTAL_ROWS - 1;

    for (int read_row = BOARD_TOTAL_ROWS - 1; read_row >= 0; read_row--) {
        bool full = true;

        for (int col = 0; col < BOARD_COLS; col++) {
            if (game->cells[read_row][col] == PIECE_NONE) {
                full = false;
                break;
            }
        }

        if (full) {
            continue;
        }

        if (write_row != read_row) {
            memcpy(game->cells[write_row], game->cells[read_row], sizeof(game->cells[0]));
        }

        write_row--;
    }

    const int cleared = write_row + 1;

    for (int row = write_row; row >= 0; row--) {
        memset(game->cells[row], 0, sizeof(game->cells[0]));
    }

    return cleared;
}

/* Guideline scoring: base clear value scaled by level, 1.5x while chaining
   back-to-back tetrises, plus 50 x combo x level for consecutive clears. */
static void award_line_clears(GameState *game, int cleared) {
    static const int BASE_SCORES[] = {0, 100, 300, 500, 800};

    if (cleared <= 0) {
        game->combo = -1;
        return;
    }

    const int level = game->level;
    const bool difficult = cleared == 4;
    int points = BASE_SCORES[cleared];

    if (difficult && game->back_to_back) {
        points = points * 3 / 2;
    }

    game->score += points * level;
    game->combo++;

    if (game->combo > 0) {
        game->score += 50 * game->combo * level;
    }

    game->back_to_back = difficult;
    game->lines_cleared += cleared;
    apply_level(game);

    if (game->mode == GAME_MODE_SPRINT && game->lines_cleared >= SPRINT_LINE_GOAL) {
        game->complete = true;
    }
}

static void commit_lock(GameState *game) {
    write_piece_to_board(game);
    award_line_clears(game, clear_full_lines(game));

    if (game->complete) {
        return;
    }

    spawn_next(game);
}

/* Called after any successful move or rotate: keeps a grounded piece alive a
   little longer so last-moment adjustments feel responsive. */
static void refresh_lock_delay(GameState *game) {
    if (!game->grounded) {
        return;
    }

    if (game->lock_resets < LOCK_RESET_LIMIT) {
        game->lock_timer = 0.0f;
        game->lock_resets++;
    }
}

bool game_is_finished(const GameState *game) {
    return game->game_over || game->complete;
}

int game_lines_remaining(const GameState *game) {
    const int remaining = SPRINT_LINE_GOAL - game->lines_cleared;
    return remaining > 0 ? remaining : 0;
}

void game_start(GameState *game, GameMode mode, int start_level) {
    if (start_level < 1) {
        start_level = 1;
    }

    if (start_level > MAX_START_LEVEL) {
        start_level = MAX_START_LEVEL;
    }

    memset(game, 0, sizeof(*game));

    game->mode = mode;
    game->start_level = mode == GAME_MODE_SPRINT ? 1 : start_level;
    game->hold = PIECE_NONE;
    game->combo = -1;

    apply_level(game);
    refill_bag(game);

    for (int i = 0; i < NEXT_QUEUE_SIZE; i++) {
        game->next_queue[i] = take_from_bag(game);
    }

    spawn_next(game);
}

void game_restart(GameState *game) {
    game_start(game, game->mode, game->start_level);
}

void game_update(GameState *game, float dt) {
    if (game_is_finished(game) || game->paused) {
        return;
    }

    game->elapsed_seconds += dt;
    game->gravity_timer += dt;

    while (game->gravity_timer >= game->gravity_interval) {
        game->gravity_timer -= game->gravity_interval;

        if (!game_move(game, 0, 1)) {
            game->gravity_timer = 0.0f;
            break;
        }
    }

    game->grounded = piece_is_grounded(game);

    if (!game->grounded) {
        game->lock_timer = 0.0f;
        game->lock_resets = 0;
        return;
    }

    game->lock_timer += dt;

    if (game->lock_timer >= LOCK_DELAY_SECONDS) {
        commit_lock(game);
    }
}

bool game_move(GameState *game, int dx, int dy) {
    const int nx = game->piece_x + dx;
    const int ny = game->piece_y + dy;

    if (collides(game, game->current, game->rotation, nx, ny)) {
        return false;
    }

    game->piece_x = nx;
    game->piece_y = ny;
    refresh_lock_delay(game);
    return true;
}

bool game_rotate(GameState *game, int direction) {
    static const int KICKS[5][2] = {{0, 0}, {-1, 0}, {1, 0}, {-2, 0}, {2, 0}};
    const int next = game->rotation + direction;

    for (int i = 0; i < 5; i++) {
        const int px = game->piece_x + KICKS[i][0];
        const int py = game->piece_y + KICKS[i][1];

        if (!collides(game, game->current, next, px, py)) {
            game->rotation = next;
            game->piece_x = px;
            game->piece_y = py;
            refresh_lock_delay(game);
            return true;
        }
    }

    return false;
}

bool game_soft_drop(GameState *game) {
    if (game_is_finished(game) || !game_move(game, 0, 1)) {
        return false;
    }

    game->gravity_timer = 0.0f;
    game->score += 1;
    return true;
}

void game_hard_drop(GameState *game) {
    if (game_is_finished(game)) {
        return;
    }

    while (game_move(game, 0, 1)) {
        game->score += 2;
    }

    commit_lock(game);
}

bool game_hold(GameState *game) {
    if (game->hold_used || game_is_finished(game)) {
        return false;
    }

    const PieceType previous = game->hold;
    game->hold = game->current;

    if (previous == PIECE_NONE) {
        place_piece(game, pop_next_piece(game));
    } else {
        place_piece(game, previous);
    }

    game->hold_used = true;
    return true;
}

PieceType game_board_cell(const GameState *game, int col, int row) {
    if (col < 0 || col >= BOARD_COLS || row < 0 || row >= BOARD_TOTAL_ROWS) {
        return PIECE_NONE;
    }

    return game->cells[row][col];
}

int game_ghost_y(const GameState *game) {
    int y = game->piece_y;

    while (!collides(game, game->current, game->rotation, game->piece_x, y + 1)) {
        y++;
    }

    return y;
}
