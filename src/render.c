#include "render.h"

#include "config.h"
#include "scores.h"

#include <raylib.h>
#include <stddef.h>

#define SIDE_PANEL_WIDTH 160
#define PREVIEW_CELL 14
#define PREVIEW_BOX_WIDTH 96
#define PREVIEW_BOX_HEIGHT 58

static const Color COLOR_MUTED = {120, 120, 140, 255};

/* Theme art is always drawn axis aligned. Rotating a piece must never rotate
   its tile, otherwise baked-in lighting would point in four directions at
   once. */
static void draw_tile(const Theme *theme, int x, int y, int size, Color tint) {
    const float source_size = (float)theme->texture.width;

    DrawTexturePro(
        theme->texture,
        (Rectangle){0.0f, 0.0f, source_size, source_size},
        (Rectangle){(float)x, (float)y, (float)size, (float)size},
        (Vector2){0.0f, 0.0f},
        0.0f,
        tint
    );
}

static void draw_flat_cell(int x, int y, int size, Color fill) {
    const int inner = size - CELL_GAP * 2;

    DrawRectangle(x + CELL_GAP, y + CELL_GAP, inner, inner, fill);
    DrawRectangleLinesEx(
        (Rectangle){(float)(x + CELL_GAP), (float)(y + CELL_GAP), (float)inner, (float)inner},
        1.0f,
        ColorBrightness(fill, -0.25f)
    );
}

static void draw_cell(const Theme *theme, int x, int y, int size, Color color) {
    if (theme_has_texture(theme)) {
        draw_tile(theme, x, y, size, color);
    } else {
        draw_flat_cell(x, y, size, color);
    }
}

static void draw_ghost_cell(const Theme *theme, int x, int y, int size, Color color) {
    const float opacity = theme_ghost_opacity(theme);

    if (theme_ghost_style(theme) == THEME_GHOST_TILE) {
        draw_tile(theme, x, y, size, Fade(color, opacity));
        return;
    }

    const int inner = size - CELL_GAP * 2;

    DrawRectangleLinesEx(
        (Rectangle){(float)(x + CELL_GAP), (float)(y + CELL_GAP), (float)inner, (float)inner},
        2.0f,
        Fade(color, opacity)
    );
}

/* Draws a piece centred inside a fixed preview box, used for hold and next. */
static void draw_piece_preview(const Theme *theme, PieceType piece, int box_x, int box_y) {
    DrawRectangle(box_x, box_y, PREVIEW_BOX_WIDTH, PREVIEW_BOX_HEIGHT, theme->panel);
    DrawRectangleLines(box_x, box_y, PREVIEW_BOX_WIDTH, PREVIEW_BOX_HEIGHT, theme->outline);

    const PieceCell *shape = game_piece_shape(piece, 0);
    if (shape == NULL) {
        return;
    }

    int min_x = shape[0].x, max_x = shape[0].x;
    int min_y = shape[0].y, max_y = shape[0].y;

    for (int i = 1; i < 4; i++) {
        if (shape[i].x < min_x) min_x = shape[i].x;
        if (shape[i].x > max_x) max_x = shape[i].x;
        if (shape[i].y < min_y) min_y = shape[i].y;
        if (shape[i].y > max_y) max_y = shape[i].y;
    }

    const int piece_w = (max_x - min_x + 1) * PREVIEW_CELL;
    const int piece_h = (max_y - min_y + 1) * PREVIEW_CELL;
    const int origin_x = box_x + (PREVIEW_BOX_WIDTH - piece_w) / 2 - min_x * PREVIEW_CELL;
    const int origin_y = box_y + (PREVIEW_BOX_HEIGHT - piece_h) / 2 - min_y * PREVIEW_CELL;
    const Color color = theme_piece_color(theme, piece);

    for (int i = 0; i < 4; i++) {
        draw_cell(
            theme,
            origin_x + shape[i].x * PREVIEW_CELL,
            origin_y + shape[i].y * PREVIEW_CELL,
            PREVIEW_CELL,
            color
        );
    }
}

static void draw_stat(const char *label, const char *value, int x, int y) {
    DrawText(label, x, y, 13, COLOR_MUTED);
    DrawText(value, x, y + 16, 20, RAYWHITE);
}

BoardLayout render_board_layout(int window_width, int window_height) {
    const int available_width = window_width - SIDE_PANEL_WIDTH - 40;
    const int available_height = window_height - 40;

    int cell_size = available_width / BOARD_COLS;
    const int height_cell = available_height / BOARD_ROWS;

    if (height_cell < cell_size) {
        cell_size = height_cell;
    }

    if (cell_size < 12) {
        cell_size = 12;
    }

    const int board_width = cell_size * BOARD_COLS;
    const int board_height = cell_size * BOARD_ROWS;

    return (BoardLayout){
        (window_width - SIDE_PANEL_WIDTH - board_width) / 2,
        (window_height - board_height) / 2,
        cell_size,
    };
}

void render_background(const Theme *theme, int window_width, int window_height) {
    ClearBackground(theme->background);
    DrawRectangle(0, 0, window_width, window_height, theme->background);
}

static void render_side_panel(const RenderContext *context, int panel_x, int panel_y) {
    const GameState *game = context->game;
    const Theme *theme = context->theme;

    char time_text[32];
    scores_format_time(game->elapsed_seconds, time_text, (int)sizeof(time_text));

    DrawText(game_mode_name(game->mode), panel_x, panel_y, 16, (Color){150, 150, 190, 255});

    int cursor_y = panel_y + 28;
    DrawText("HOLD", panel_x, cursor_y, 13, COLOR_MUTED);
    draw_piece_preview(theme, game->hold, panel_x, cursor_y + 18);

    cursor_y += 18 + PREVIEW_BOX_HEIGHT + 14;
    DrawText("NEXT", panel_x, cursor_y, 13, COLOR_MUTED);
    cursor_y += 18;

    for (int i = 0; i < NEXT_QUEUE_SIZE; i++) {
        draw_piece_preview(theme, game->next_queue[i], panel_x, cursor_y);
        cursor_y += PREVIEW_BOX_HEIGHT + 6;
    }

    cursor_y += 12;

    switch (game->mode) {
    case GAME_MODE_SPRINT:
        draw_stat("TIME", time_text, panel_x, cursor_y);
        draw_stat("LINES LEFT", TextFormat("%d", game_lines_remaining(game)), panel_x, cursor_y + 46);
        break;
    case GAME_MODE_ZEN:
        draw_stat("LINES", TextFormat("%d", game->lines_cleared), panel_x, cursor_y);
        draw_stat("SPEED", TextFormat("%d", game->level), panel_x, cursor_y + 46);
        draw_stat("TIME", time_text, panel_x, cursor_y + 92);
        break;
    default:
        draw_stat("SCORE", TextFormat("%d", game->score), panel_x, cursor_y);
        draw_stat("LINES", TextFormat("%d", game->lines_cleared), panel_x, cursor_y + 46);
        draw_stat("LEVEL", TextFormat("%d", game->level), panel_x, cursor_y + 92);
        break;
    }
}

static void render_board(const RenderContext *context, const BoardLayout *layout) {
    const Theme *theme = context->theme;
    const int width = layout->cell_size * BOARD_COLS;
    const int height = layout->cell_size * BOARD_ROWS;

    DrawRectangle(layout->x - 8, layout->y - 8, width + 16, height + 16, theme->panel);
    DrawRectangleLinesEx(
        (Rectangle){(float)(layout->x - 8), (float)(layout->y - 8), (float)(width + 16), (float)(height + 16)},
        2.0f,
        theme->outline
    );

    for (int row = 0; row < BOARD_ROWS; row++) {
        for (int col = 0; col < BOARD_COLS; col++) {
            const PieceType piece = game_board_cell(context->game, col, row + BOARD_BUFFER_ROWS);

            if (piece == PIECE_NONE) {
                continue;
            }

            draw_cell(
                theme,
                layout->x + col * layout->cell_size,
                layout->y + row * layout->cell_size,
                layout->cell_size,
                theme_piece_color(theme, piece)
            );
        }
    }
}

static void render_active_piece(const RenderContext *context, const BoardLayout *layout) {
    const GameState *game = context->game;
    const Theme *theme = context->theme;
    const PieceCell *shape = game_piece_shape(game->current, game->rotation);

    if (shape == NULL || game_is_finished(game)) {
        return;
    }

    const Color color = theme_piece_color(theme, game->current);
    const int ghost_y = game_ghost_y(game);

    for (int i = 0; i < 4; i++) {
        const int col = game->piece_x + shape[i].x;
        const int row = ghost_y + shape[i].y - BOARD_BUFFER_ROWS;

        if (row < 0 || row >= BOARD_ROWS) {
            continue;
        }

        draw_ghost_cell(
            theme,
            layout->x + col * layout->cell_size,
            layout->y + row * layout->cell_size,
            layout->cell_size,
            color
        );
    }

    for (int i = 0; i < 4; i++) {
        const int col = game->piece_x + shape[i].x;
        const int row = game->piece_y + shape[i].y - BOARD_BUFFER_ROWS;

        if (row < 0 || row >= BOARD_ROWS) {
            continue;
        }

        draw_cell(
            theme,
            layout->x + col * layout->cell_size,
            layout->y + row * layout->cell_size,
            layout->cell_size,
            color
        );
    }
}

static void render_result_overlay(const RenderContext *context) {
    const GameState *game = context->game;
    const int box_w = 300;
    const int box_h = 170;
    const int box_x = (context->window_width - box_w) / 2;
    const int box_y = (context->window_height - box_h) / 2;

    DrawRectangle(0, 0, context->window_width, context->window_height, (Color){0, 0, 0, 150});
    DrawRectangle(box_x, box_y, box_w, box_h, (Color){24, 24, 32, 245});
    DrawRectangleLines(box_x, box_y, box_w, box_h, context->theme->outline);

    const char *title = game->complete ? "40 Lines Complete" : "Game Over";
    DrawText(title, box_x + box_w / 2 - MeasureText(title, 24) / 2, box_y + 22, 24, RAYWHITE);

    if (game->complete) {
        char detail[64];
        scores_format_time(game->elapsed_seconds, detail, (int)sizeof(detail));
        DrawText(detail, box_x + box_w / 2 - MeasureText(detail, 36) / 2, box_y + 58, 36, (Color){140, 200, 255, 255});
    } else {
        const char *summary = TextFormat("Score %d   Lines %d", game->score, game->lines_cleared);
        DrawText(summary, box_x + box_w / 2 - MeasureText(summary, 18) / 2, box_y + 66, 18, LIGHTGRAY);
    }

    if (context->score_rank >= 0) {
        const char *rank_text = TextFormat("New high score - rank %d", context->score_rank + 1);
        DrawText(
            rank_text,
            box_x + box_w / 2 - MeasureText(rank_text, 15) / 2,
            box_y + 106,
            15,
            (Color){255, 210, 120, 255}
        );
    }

    const char *footer = "Enter to retry    Esc for menu";
    DrawText(footer, box_x + box_w / 2 - MeasureText(footer, 14) / 2, box_y + box_h - 32, 14, COLOR_MUTED);
}

/* Ring that fills clockwise while Esc is held, so a stray tap cannot end a run. */
static void render_quit_progress(const RenderContext *context) {
    const float progress = context->quit_progress;

    if (progress <= 0.0f) {
        return;
    }

    const Vector2 center = {
        (float)context->window_width * 0.5f,
        (float)context->window_height * 0.5f,
    };
    const float inner = 42.0f;
    const float outer = 52.0f;

    DrawRectangle(
        0,
        0,
        context->window_width,
        context->window_height,
        Fade(BLACK, 0.45f + 0.25f * progress)
    );

    DrawRing(center, inner, outer, 0.0f, 360.0f, 64, (Color){50, 50, 62, 255});
    DrawRing(center, inner, outer, -90.0f, -90.0f + 360.0f * progress, 64, (Color){235, 90, 90, 255});

    const char *label = "Hold Esc to quit";
    DrawText(
        label,
        (int)center.x - MeasureText(label, 18) / 2,
        (int)center.y + (int)outer + 22,
        18,
        RAYWHITE
    );
}

void render_game(const RenderContext *context) {
    const BoardLayout layout = render_board_layout(context->window_width, context->window_height);

    render_board(context, &layout);
    render_active_piece(context, &layout);
    render_side_panel(context, layout.x + layout.cell_size * BOARD_COLS + 24, layout.y);

    if (context->game->paused && !game_is_finished(context->game)) {
        DrawRectangle(0, 0, context->window_width, context->window_height, (Color){0, 0, 0, 150});
        DrawText(
            "Paused",
            context->window_width / 2 - MeasureText("Paused", 30) / 2,
            context->window_height / 2 - 15,
            30,
            RAYWHITE
        );
    }

    if (game_is_finished(context->game)) {
        render_result_overlay(context);
    }

    render_quit_progress(context);
}
