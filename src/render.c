#include "render.h"

#include "config.h"
#include "scores.h"
#include "ui.h"

#include <math.h>
#include <raylib.h>
#include <stddef.h>

#define SIDE_PANEL_WIDTH 172
#define PREVIEW_CELL 16
#define PREVIEW_BOX_WIDTH 108
#define PREVIEW_BOX_HEIGHT 64
#define WELL_PADDING 10

static const Color COLOR_MUTED = {132, 132, 152, 255};
static const Color COLOR_DIM = {96, 96, 116, 255};

static Color mode_accent(GameMode mode) {
    switch (mode) {
    case GAME_MODE_SPRINT: return (Color){96, 214, 255, 255};
    case GAME_MODE_ZEN: return (Color){178, 150, 255, 255};
    default: return (Color){255, 186, 92, 255};
    }
}

static Rectangle cell_rect(const BoardLayout *layout, int col, int row) {
    return (Rectangle){
        (float)(layout->x + col * layout->cell_size),
        (float)(layout->y + row * layout->cell_size),
        (float)layout->cell_size,
        (float)layout->cell_size,
    };
}

/* Theme art is always drawn axis aligned. Rotating a piece must never rotate
   its tile, otherwise baked-in lighting would point in four directions at
   once. */
/* Every value here was resolved when the theme was activated, so this stays a
   table lookup and one well-predicted branch no matter how a theme is
   configured. Solid, tinted and sheet art all land in the same shape of work. */
static void draw_block(const Theme *theme, PieceType piece, Rectangle dest, float alpha) {
    const ThemeBlocks *blocks = &theme->blocks;
    const float inset = blocks->inset;
    const Rectangle inner = {
        dest.x + inset,
        dest.y + inset,
        dest.width - inset * 2.0f,
        dest.height - inset * 2.0f,
    };

    if (blocks->source[piece].width > 0.0f) {
        DrawTexturePro(
            blocks->texture,
            blocks->source[piece],
            dest,
            (Vector2){0.0f, 0.0f},
            0.0f,
            Fade(blocks->fill[piece], alpha)
        );
        return;
    }

    /* Roundness is a fraction of the block rather than a pixel radius, so a
       block keeps its shape whether it is drawn in the well or in a preview
       box a third of the size. */
    if (blocks->radius > 0.0f) {
        DrawRectangleRounded(inner, blocks->radius, 6, Fade(blocks->fill[piece], alpha));

        if (blocks->border_width > 0.0f) {
            DrawRectangleRoundedLinesEx(
                inner, blocks->radius, 6, blocks->border_width, Fade(blocks->border[piece], alpha));
        }

        return;
    }

    DrawRectangleRec(inner, Fade(blocks->fill[piece], alpha));

    if (blocks->border_width > 0.0f) {
        DrawRectangleLinesEx(inner, blocks->border_width, Fade(blocks->border[piece], alpha));
    }
}

/* Fits an image to a rectangle with a single quad in every mode: tiling relies
   on repeat wrapping rather than a loop of draws, and cover crops through the
   source rectangle rather than a scissor pass. */
static void draw_fitted(const Texture2D *texture, ThemeFit fit, Rectangle dest, Color tint) {
    Rectangle source = {0.0f, 0.0f, (float)texture->width, (float)texture->height};

    if (fit == THEME_FIT_TILE) {
        source.width = dest.width;
        source.height = dest.height;
    } else if (fit == THEME_FIT_COVER) {
        const float scale = fminf(source.width / dest.width, source.height / dest.height);
        const float crop_width = dest.width * scale;
        const float crop_height = dest.height * scale;

        source.x = (source.width - crop_width) * 0.5f;
        source.y = (source.height - crop_height) * 0.5f;
        source.width = crop_width;
        source.height = crop_height;
    }

    DrawTexturePro(*texture, source, dest, (Vector2){0.0f, 0.0f}, 0.0f, tint);
}

static void draw_surface(const ThemeSurface *surface, Rectangle rect, Color border) {
    if (surface->texture_loaded) {
        draw_fitted(&surface->texture, surface->fit, rect, WHITE);
    } else {
        ui_rounded(rect, surface->radius, surface->fill);
    }

    if (surface->border_width > 0.0f) {
        ui_rounded_outline(rect, surface->radius, surface->border_width, border);
    }
}

static void draw_ghost_block(const Theme *theme, PieceType piece, Rectangle dest) {
    const Color color = theme_piece_color(theme, piece);
    const float opacity = theme_ghost_opacity(theme);

    if (theme_ghost_style(theme) == THEME_GHOST_TILE) {
        draw_block(theme, piece, dest, opacity);
        return;
    }

    const Rectangle inner = {
        dest.x + CELL_GAP,
        dest.y + CELL_GAP,
        dest.width - CELL_GAP * 2.0f,
        dest.height - CELL_GAP * 2.0f,
    };

    DrawRectangleRounded(inner, 0.18f, 6, Fade(color, opacity * 0.12f));
    DrawRectangleRoundedLinesEx(inner, 0.18f, 6, 2.0f, Fade(color, opacity));
}

static void draw_piece_preview(const Theme *theme, PieceType piece, Rectangle box) {
    draw_surface(&theme->panel, box, Fade(theme->panel.border, 0.8f));

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

    const float piece_w = (float)(max_x - min_x + 1) * PREVIEW_CELL;
    const float piece_h = (float)(max_y - min_y + 1) * PREVIEW_CELL;
    const float origin_x = box.x + (box.width - piece_w) * 0.5f - (float)min_x * PREVIEW_CELL;
    const float origin_y = box.y + (box.height - piece_h) * 0.5f - (float)min_y * PREVIEW_CELL;

    for (int i = 0; i < 4; i++) {
        const Rectangle dest = {
            origin_x + (float)shape[i].x * PREVIEW_CELL,
            origin_y + (float)shape[i].y * PREVIEW_CELL,
            PREVIEW_CELL,
            PREVIEW_CELL,
        };

        draw_block(theme, piece, dest, 1.0f);
    }
}

static void draw_stat(const char *label, const char *value, float x, float y, Color value_color) {
    ui_label(label, x, y, 11.0f, COLOR_DIM);
    ui_text_mono(value, x, y + 15.0f, 25.0f, value_color);
}

BoardLayout render_board_layout(int window_width, int window_height) {
    const int available_width = window_width - SIDE_PANEL_WIDTH - 48;
    const int available_height = window_height - 56;

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

/* Exactly one of these runs per frame. Nothing is layered on top of anything
   else, so a shader backdrop costs one quad and a solid backdrop costs a
   clear. */
void render_background(const Theme *theme, int window_width, int window_height) {
    const ThemeBackdrop *backdrop = &theme->backdrop;
    const Rectangle screen = {0.0f, 0.0f, (float)window_width, (float)window_height};

    switch (backdrop->mode) {
    case THEME_BACKDROP_SOLID:
        ClearBackground(backdrop->top);
        return;

    case THEME_BACKDROP_IMAGE:
        ClearBackground(backdrop->top);
        draw_fitted(&backdrop->texture, backdrop->fit, screen, WHITE);
        return;

    case THEME_BACKDROP_SHADER: {
        const float time = (float)GetTime();
        const float resolution[2] = {(float)window_width, (float)window_height};

        SetShaderValue(backdrop->program, backdrop->loc_time, &time, SHADER_UNIFORM_FLOAT);
        SetShaderValue(backdrop->program, backdrop->loc_resolution, resolution, SHADER_UNIFORM_VEC2);

        BeginShaderMode(backdrop->program);
        DrawRectangleRec(screen, WHITE);
        EndShaderMode();
        return;
    }

    default:
        DrawRectangleGradientV(0, 0, window_width, window_height, backdrop->top, backdrop->bottom);
        return;
    }
}

static void render_well(const RenderContext *context, const BoardLayout *layout) {
    const Theme *theme = context->theme;
    const Color accent = mode_accent(context->game->mode);
    const Rectangle frame = {
        (float)(layout->x - WELL_PADDING),
        (float)(layout->y - WELL_PADDING),
        (float)(layout->cell_size * BOARD_COLS + WELL_PADDING * 2),
        (float)(layout->cell_size * BOARD_ROWS + WELL_PADDING * 2),
    };
    const Color border =
        theme->well.border_is_override ? theme->well.border : Fade(accent, 0.35f);

    ui_shadow(frame, theme->well.radius, 10.0f, 0.55f);

    if (theme->well.texture_loaded) {
        draw_fitted(&theme->well.texture, theme->well.fit, frame, WHITE);
    } else {
        ui_rounded(frame, theme->well.radius, theme->well.fill);
    }

    /* Faint grid so an empty well still reads as a playfield. */
    const Color grid = Fade(WHITE, 0.035f);

    for (int col = 1; col < BOARD_COLS; col++) {
        const float x = (float)(layout->x + col * layout->cell_size);
        DrawLineEx(
            (Vector2){x, (float)layout->y},
            (Vector2){x, (float)(layout->y + layout->cell_size * BOARD_ROWS)},
            1.0f,
            grid
        );
    }

    for (int row = 1; row < BOARD_ROWS; row++) {
        const float y = (float)(layout->y + row * layout->cell_size);
        DrawLineEx(
            (Vector2){(float)layout->x, y},
            (Vector2){(float)(layout->x + layout->cell_size * BOARD_COLS), y},
            1.0f,
            grid
        );
    }

    if (theme->well.border_width > 0.0f) {
        ui_rounded_outline(frame, theme->well.radius, theme->well.border_width, border);
    }
}

static void render_stack(const RenderContext *context, const BoardLayout *layout) {
    const GameState *game = context->game;
    const Theme *theme = context->theme;

    for (int row = 0; row < BOARD_ROWS; row++) {
        for (int col = 0; col < BOARD_COLS; col++) {
            const PieceType piece = game_board_cell(game, col, row + BOARD_BUFFER_ROWS);

            if (piece != PIECE_NONE) {
                draw_block(theme, piece, cell_rect(layout, col, row), 1.0f);
            }
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

    const int ghost_y = game_ghost_y(game);

    for (int i = 0; i < 4; i++) {
        const int col = game->piece_x + shape[i].x;
        const int row = ghost_y + shape[i].y - BOARD_BUFFER_ROWS;

        if (row >= 0 && row < BOARD_ROWS) {
            draw_ghost_block(theme, game->current, cell_rect(layout, col, row));
        }
    }

    for (int i = 0; i < 4; i++) {
        const int col = game->piece_x + shape[i].x;
        const int row = game->piece_y + shape[i].y - BOARD_BUFFER_ROWS;

        if (row >= 0 && row < BOARD_ROWS) {
            draw_block(theme, game->current, cell_rect(layout, col, row), 1.0f);
        }
    }
}

static void render_side_panel(const RenderContext *context, float panel_x, float panel_y) {
    const GameState *game = context->game;
    const Theme *theme = context->theme;
    const Color accent = mode_accent(game->mode);

    char time_text[32];
    scores_format_time(game->elapsed_seconds, time_text, (int)sizeof(time_text));

    ui_label(game_mode_name(game->mode), panel_x, panel_y, 12.0f, accent);

    float cursor_y = panel_y + 24.0f;
    ui_label("Hold", panel_x, cursor_y, 11.0f, COLOR_DIM);
    draw_piece_preview(
        theme,
        game->hold,
        (Rectangle){panel_x, cursor_y + 15.0f, PREVIEW_BOX_WIDTH, PREVIEW_BOX_HEIGHT}
    );

    cursor_y += 15.0f + PREVIEW_BOX_HEIGHT + 16.0f;
    ui_label("Next", panel_x, cursor_y, 11.0f, COLOR_DIM);
    cursor_y += 15.0f;

    for (int i = 0; i < NEXT_QUEUE_SIZE; i++) {
        draw_piece_preview(
            theme,
            game->next_queue[i],
            (Rectangle){panel_x, cursor_y, PREVIEW_BOX_WIDTH, PREVIEW_BOX_HEIGHT}
        );
        cursor_y += PREVIEW_BOX_HEIGHT + 6.0f;
    }

    cursor_y += 16.0f;

    switch (game->mode) {
    case GAME_MODE_SPRINT:
        draw_stat("Time", time_text, panel_x, cursor_y, accent);
        draw_stat("Lines left", TextFormat("%d", game_lines_remaining(game)), panel_x, cursor_y + 50.0f, RAYWHITE);
        break;
    case GAME_MODE_ZEN:
        draw_stat("Lines", TextFormat("%d", game->lines_cleared), panel_x, cursor_y, RAYWHITE);
        draw_stat("Speed", TextFormat("%d", game->level), panel_x, cursor_y + 50.0f, accent);
        draw_stat("Time", time_text, panel_x, cursor_y + 100.0f, COLOR_MUTED);
        break;
    default:
        draw_stat("Score", TextFormat("%d", game->score), panel_x, cursor_y, RAYWHITE);
        draw_stat("Lines", TextFormat("%d", game->lines_cleared), panel_x, cursor_y + 50.0f, COLOR_MUTED);
        draw_stat("Level", TextFormat("%d", game->level), panel_x, cursor_y + 100.0f, accent);
        break;
    }
}

static void render_result_overlay(const RenderContext *context) {
    const GameState *game = context->game;
    const Color accent = mode_accent(game->mode);
    const Rectangle box = {
        (float)(context->window_width - 320) * 0.5f,
        (float)(context->window_height - 190) * 0.5f,
        320.0f,
        190.0f,
    };

    DrawRectangle(0, 0, context->window_width, context->window_height, Fade(BLACK, 0.62f));
    ui_shadow(box, 14.0f, 12.0f, 0.7f);
    ui_panel(box, 14.0f, (Color){26, 26, 34, 250}, Fade(accent, 0.5f));

    const float center_x = box.x + box.width * 0.5f;

    ui_text_center(game->complete ? "40 Lines Complete" : "Game Over", center_x, box.y + 26.0f, 26.0f, RAYWHITE);

    if (game->complete) {
        char detail[64];
        scores_format_time(game->elapsed_seconds, detail, (int)sizeof(detail));

        const Vector2 measured = ui_measure_mono(detail, 44.0f);
        ui_text_mono(detail, center_x - measured.x * 0.5f, box.y + 66.0f, 44.0f, accent);
    } else {
        ui_text_center(TextFormat("Score %d", game->score), center_x, box.y + 70.0f, 22.0f, RAYWHITE);
        ui_text_center(TextFormat("%d lines - level %d", game->lines_cleared, game->level),
                       center_x, box.y + 98.0f, 15.0f, COLOR_MUTED);
    }

    if (context->score_rank >= 0) {
        ui_text_center(
            TextFormat("New best - rank %d", context->score_rank + 1),
            center_x,
            box.y + 122.0f,
            15.0f,
            (Color){255, 210, 120, 255}
        );
    }

    ui_text_center("Enter to retry     Esc for menu", center_x, box.y + box.height - 30.0f, 14.0f, COLOR_DIM);
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

    DrawRectangle(0, 0, context->window_width, context->window_height, Fade(BLACK, 0.4f + 0.3f * progress));
    DrawRing(center, 44.0f, 54.0f, 0.0f, 360.0f, 64, Fade(WHITE, 0.12f));
    DrawRing(center, 44.0f, 54.0f, -90.0f, -90.0f + 360.0f * progress, 64, (Color){240, 96, 96, 255});

    ui_text_center("Hold Esc to quit", center.x, center.y + 74.0f, 17.0f, RAYWHITE);
}

void render_game(const RenderContext *context) {
    const BoardLayout layout = render_board_layout(context->window_width, context->window_height);

    render_well(context, &layout);
    render_stack(context, &layout);
    render_active_piece(context, &layout);
    render_side_panel(
        context,
        (float)(layout.x + layout.cell_size * BOARD_COLS + 26),
        (float)layout.y
    );

    if (context->game->paused && !game_is_finished(context->game)) {
        DrawRectangle(0, 0, context->window_width, context->window_height, Fade(BLACK, 0.6f));
        ui_text_center(
            "Paused",
            (float)context->window_width * 0.5f,
            (float)context->window_height * 0.5f - 20.0f,
            34.0f,
            RAYWHITE
        );
        ui_text_center(
            "P to resume",
            (float)context->window_width * 0.5f,
            (float)context->window_height * 0.5f + 22.0f,
            15.0f,
            COLOR_MUTED
        );
    }

    if (game_is_finished(context->game)) {
        render_result_overlay(context);
    }

    render_quit_progress(context);
}
