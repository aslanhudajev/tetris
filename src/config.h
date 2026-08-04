#ifndef CONFIG_H
#define CONFIG_H

#define GAME_TITLE "Metris"
#define WINDOW_DEFAULT_WIDTH 540
#define WINDOW_DEFAULT_HEIGHT 760

#define BOARD_COLS 10
#define BOARD_ROWS 20
#define BOARD_BUFFER_ROWS 2
#define BOARD_TOTAL_ROWS (BOARD_ROWS + BOARD_BUFFER_ROWS)

#define NEXT_QUEUE_SIZE 3

/* Guideline marathon: one level per 10 lines, speed curve flattens at 20. */
#define LINES_PER_LEVEL 10
#define MAX_START_LEVEL 15
#define GRAVITY_CURVE_MAX_LEVEL 20
#define SPRINT_LINE_GOAL 40

/* Time a grounded piece rests before locking, and how many times a successful
   move or rotate may restart that timer before the piece locks regardless. */
#define LOCK_DELAY_SECONDS 0.4f
#define LOCK_RESET_LIMIT 12

/* Delayed auto shift: hold time before repeat starts, then repeat interval. */
#define DAS_DELAY_SECONDS 0.14f
#define DAS_REPEAT_SECONDS 0.03f
#define SOFT_DROP_REPEAT_SECONDS 0.03f

/* Frame times above this are treated as a stall (window drag, app switch) and
   clamped so the board never jumps several rows at once. */
#define MAX_FRAME_DELTA 0.1f

#define SCORE_ENTRY_COUNT 10
#define THEME_MAX_COUNT 16

/* Quitting a live run needs a deliberate hold rather than a stray Esc press. */
#define QUIT_HOLD_SECONDS 0.75f

#define CELL_GAP 1

#endif
