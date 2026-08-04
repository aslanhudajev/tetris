#include "render.h"

#include "config.h"
#include "scores.h"
#include "ui.h"

#include <math.h>
#include <raylib.h>
#include <stddef.h>
#include <stdio.h>

/* Everything that is not the board lives in one column down the right: hold,
   the queue, then the run's numbers anchored to the bottom of the well. The
   window is tall and narrow by design, so a second column would cost the board
   width it cannot spare, and splitting the chrome across both sides would leave
   the eye travelling further than the board is wide. */
#define EDGE_MARGIN 18
#define SIDE_COLUMN 100
#define COLUMN_GUTTER 14
#define TOP_MARGIN 26
#define BOTTOM_MARGIN 18

/* Roughly half a board cell at the default window size. Much smaller and the
   queue stops reading as the same pieces that are about to arrive. */
#define PREVIEW_CELL 18
#define CHIP_HEIGHT 18
#define SLOT_HEIGHT 52
#define BOX_GAP 12

/* Label, then value, then the space before whatever follows. */
#define STAT_LABEL 8.5f
#define STAT_LEAD 13.0f
#define STAT_GAP 15.0f

/* Deliberately neutral. Every colour on this screen should come from the
   blocks, so the chrome carries no hue of its own. */
static const Color INK_SOFT = {216, 216, 216, 255};
static const Color COLOR_MUTED = {168, 168, 168, 255};
static const Color COLOR_DIM = {136, 136, 136, 255};
static const Color HAIRLINE = {255, 255, 255, 34};
static const Color CHIP_FILL = {226, 226, 226, 255};
static const Color CHIP_INK = {14, 14, 16, 255};

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

static bool surface_is_visible(const ThemeSurface *surface) {
    return surface->texture_loaded || surface->fill.a > 0 || surface->border_width > 0.0f;
}

static void draw_surface(const ThemeSurface *surface, Rectangle rect, Color border) {
    if (surface->texture_loaded) {
        draw_fitted(&surface->texture, surface->fit, rect, WHITE);
    } else if (surface->fill.a > 0) {
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

    DrawRectangleRec(inner, Fade(color, opacity * 0.10f));
    DrawRectangleLinesEx(inner, 1.0f, Fade(color, opacity));
}

/* Label inverted into a bar across the top of the box. */
static Rectangle draw_labelled_box(const Theme *theme, const char *label, Rectangle box) {
    if (surface_is_visible(&theme->panel)) {
        draw_surface(&theme->panel, box, theme->panel.border);
    }

    DrawRectangleRec((Rectangle){box.x, box.y, box.width, CHIP_HEIGHT}, CHIP_FILL);
    ui_label(label, box.x + 6.0f, box.y + 5.0f, 8.5f, CHIP_INK);

    return (Rectangle){box.x, box.y + CHIP_HEIGHT, box.width, box.height - CHIP_HEIGHT};
}

static void draw_piece_preview(const Theme *theme, PieceType piece, Rectangle slot) {
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
    const float origin_x = slot.x + (slot.width - piece_w) * 0.5f - (float)min_x * PREVIEW_CELL;
    const float origin_y = slot.y + (slot.height - piece_h) * 0.5f - (float)min_y * PREVIEW_CELL;

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

BoardLayout render_board_layout(int window_width, int window_height) {
    const int chrome = EDGE_MARGIN * 2 + COLUMN_GUTTER + SIDE_COLUMN;
    const int available_width = window_width - chrome;
    const int available_height = window_height - TOP_MARGIN - BOTTOM_MARGIN;

    int cell_size = available_width / BOARD_COLS;
    const int height_cell = available_height / BOARD_ROWS;

    if (height_cell < cell_size) {
        cell_size = height_cell;
    }

    if (cell_size < 10) {
        cell_size = 10;
    }

    const int board_width = cell_size * BOARD_COLS;
    const int board_height = cell_size * BOARD_ROWS;

    /* The board and its column are centred as one block, so the board sits
       slightly left of the window centre rather than the whole layout hugging
       an edge. */
    const int group_width = board_width + COLUMN_GUTTER + SIDE_COLUMN;

    return (BoardLayout){
        (window_width - group_width) / 2,
        TOP_MARGIN + (available_height - board_height) / 2,
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

        /* Framebuffer pixels rather than logical points. A backdrop shader has
           to work from gl_FragCoord, which counts real pixels, and on a Retina
           display the two differ by a factor of two. */
        const float resolution[2] = {(float)GetRenderWidth(), (float)GetRenderHeight()};

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
    const Rectangle frame = {
        (float)layout->x,
        (float)layout->y,
        (float)(layout->cell_size * BOARD_COLS),
        (float)(layout->cell_size * BOARD_ROWS),
    };

    if (theme->well.texture_loaded) {
        draw_fitted(&theme->well.texture, theme->well.fit, frame, WHITE);
    } else {
        ui_rounded(frame, theme->well.radius, theme->well.fill);
    }

    /* Faint grid so an empty well still reads as a playfield. */
    const Color grid = Fade(WHITE, 0.045f);

    for (int col = 1; col < BOARD_COLS; col++) {
        const float x = (float)(layout->x + col * layout->cell_size);
        DrawLineEx((Vector2){x, frame.y}, (Vector2){x, frame.y + frame.height}, 1.0f, grid);
    }

    for (int row = 1; row < BOARD_ROWS; row++) {
        const float y = (float)(layout->y + row * layout->cell_size);
        DrawLineEx((Vector2){frame.x, y}, (Vector2){frame.x + frame.width, y}, 1.0f, grid);
    }

    if (theme->well.border_width > 0.0f) {
        ui_rounded_outline(frame, theme->well.radius, theme->well.border_width, theme->well.border);
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

typedef struct {
    const char *label;
    char value[24];
    float size;
    Color tone;
} Stat;

/* Bottom aligned against the well, so the column reads as one thing anchored at
   both ends rather than a stack that trails off. */
static void draw_stats(const RenderContext *context, float x, float bottom) {
    const GameState *game = context->game;
    const Color accent = mode_accent(game->mode);

    Stat stats[3];
    int count = 0;

    char elapsed[24];
    scores_format_time(game->elapsed_seconds, elapsed, (int)sizeof(elapsed));

    switch (game->mode) {
    case GAME_MODE_SPRINT:
        stats[count].label = "lines";
        snprintf(stats[count].value, sizeof(stats[count].value), "%d/%d", game->lines_cleared,
                 SPRINT_LINE_GOAL);
        stats[count].size = 16.0f;
        stats[count++].tone = INK_SOFT;

        stats[count].label = "time";
        snprintf(stats[count].value, sizeof(stats[count].value), "%s", elapsed);
        stats[count].size = 22.0f;
        stats[count++].tone = accent;
        break;

    case GAME_MODE_ZEN:
        stats[count].label = "time";
        snprintf(stats[count].value, sizeof(stats[count].value), "%s", elapsed);
        stats[count].size = 16.0f;
        stats[count++].tone = INK_SOFT;

        stats[count].label = "lines";
        snprintf(stats[count].value, sizeof(stats[count].value), "%d", game->lines_cleared);
        stats[count].size = 22.0f;
        stats[count++].tone = accent;
        break;

    default:
        stats[count].label = "level";
        snprintf(stats[count].value, sizeof(stats[count].value), "%d", game->level);
        stats[count].size = 16.0f;
        stats[count++].tone = INK_SOFT;

        stats[count].label = "lines";
        snprintf(stats[count].value, sizeof(stats[count].value), "%d", game->lines_cleared);
        stats[count].size = 16.0f;
        stats[count++].tone = INK_SOFT;

        stats[count].label = "score";
        snprintf(stats[count].value, sizeof(stats[count].value), "%d", game->score);
        stats[count].size = 22.0f;
        stats[count++].tone = accent;
        break;
    }

    float height = 0.0f;

    for (int i = 0; i < count; i++) {
        height += STAT_LEAD + stats[i].size + (i + 1 < count ? STAT_GAP : 0.0f);
    }

    float y = bottom - height;

    for (int i = 0; i < count; i++) {
        ui_label(stats[i].label, x, y, STAT_LABEL, COLOR_DIM);
        ui_text_mono(stats[i].value, x, y + STAT_LEAD, stats[i].size, stats[i].tone);
        y += STAT_LEAD + stats[i].size + STAT_GAP;
    }
}

static void render_side_column(const RenderContext *context, const BoardLayout *layout) {
    const GameState *game = context->game;
    const Theme *theme = context->theme;
    const float x = (float)(layout->x + layout->cell_size * BOARD_COLS) + COLUMN_GUTTER;

    const Rectangle hold_box = {x, (float)layout->y, SIDE_COLUMN, CHIP_HEIGHT + SLOT_HEIGHT};

    draw_piece_preview(theme, game->hold, draw_labelled_box(theme, "hold", hold_box));

    const Rectangle next_box = {
        x,
        hold_box.y + hold_box.height + BOX_GAP,
        SIDE_COLUMN,
        CHIP_HEIGHT + SLOT_HEIGHT * NEXT_QUEUE_SIZE,
    };
    const Rectangle body = draw_labelled_box(theme, "next", next_box);

    for (int i = 0; i < NEXT_QUEUE_SIZE; i++) {
        const float slot_y = body.y + (float)i * SLOT_HEIGHT;

        if (i > 0) {
            ui_hairline(body.x + 8.0f, slot_y, body.width - 16.0f, HAIRLINE);
        }

        draw_piece_preview(
            theme, game->next_queue[i], (Rectangle){body.x, slot_y, body.width, SLOT_HEIGHT});
    }

    draw_stats(context, x, (float)(layout->y + layout->cell_size * BOARD_ROWS));
}

/* Centred type on a full bleed scrim rather than a panel. A box here would put
   a second frame inside a window that already has one. */
static void render_result_overlay(const RenderContext *context) {
    const GameState *game = context->game;
    const Color accent = mode_accent(game->mode);
    const float center_x = (float)context->window_width * 0.5f;
    const float rule_width = 150.0f;

    DrawRectangle(0, 0, context->window_width, context->window_height, Fade(BLACK, 0.80f));

    float y = (float)context->window_height * 0.5f - 96.0f;

    const char *eyebrow = game_mode_name(game->mode);
    ui_label(eyebrow, center_x - ui_measure_label(eyebrow, 9.5f).x * 0.5f, y, 9.5f, accent);
    y += 22.0f;

    ui_text_center(game->complete ? "Complete" : "Game Over", center_x, y, 30.0f, RAYWHITE);
    y += 42.0f;

    ui_hairline(center_x - rule_width * 0.5f, y, rule_width, HAIRLINE);
    y += 20.0f;

    if (game->complete) {
        char detail[64];
        scores_format_time(game->elapsed_seconds, detail, (int)sizeof(detail));

        ui_text_mono(detail, center_x - ui_measure_mono(detail, 40.0f).x * 0.5f, y, 40.0f, accent);
        y += 50.0f;
    } else {
        const char *score = TextFormat("%d", game->score);

        ui_text_mono(score, center_x - ui_measure_mono(score, 36.0f).x * 0.5f, y, 36.0f, RAYWHITE);
        y += 44.0f;

        const char *detail = TextFormat("%d lines      level %d", game->lines_cleared, game->level);
        ui_label(detail, center_x - ui_measure_label(detail, 9.5f).x * 0.5f, y, 9.5f, COLOR_MUTED);
        y += 22.0f;
    }

    if (context->score_rank >= 0) {
        const char *best = TextFormat("new best      rank %d", context->score_rank + 1);
        ui_label(best, center_x - ui_measure_label(best, 9.5f).x * 0.5f, y, 9.5f,
                 (Color){255, 210, 120, 255});
    }

    ui_label("enter to retry      esc for menu",
             center_x - ui_measure_label("enter to retry      esc for menu", 9.0f).x * 0.5f,
             (float)context->window_height * 0.5f + 104.0f, 9.0f, COLOR_DIM);
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
    DrawRing(center, 40.0f, 44.0f, 0.0f, 360.0f, 64, Fade(WHITE, 0.10f));
    DrawRing(center, 40.0f, 44.0f, -90.0f, -90.0f + 360.0f * progress, 64, (Color){236, 100, 100, 255});

    ui_label("hold esc to quit", center.x - ui_measure_label("hold esc to quit", 9.5f).x * 0.5f,
             center.y + 66.0f, 9.5f, COLOR_MUTED);
}

void render_game(const RenderContext *context) {
    const BoardLayout layout = render_board_layout(context->window_width, context->window_height);

    /* Hung off the top left corner of the well rather than centred over the
       window, which would put it out of line with everything below it. */
    ui_label(game_mode_name(context->game->mode), (float)layout.x, (float)layout.y - 16.0f, 9.0f,
             INK_SOFT);

    render_well(context, &layout);
    render_stack(context, &layout);
    render_active_piece(context, &layout);
    render_side_column(context, &layout);

    if (context->game->paused && !game_is_finished(context->game)) {
        const float center_x = (float)context->window_width * 0.5f;
        const float center_y = (float)context->window_height * 0.5f;

        DrawRectangle(0, 0, context->window_width, context->window_height, Fade(BLACK, 0.72f));
        ui_text_center("Paused", center_x, center_y - 26.0f, 30.0f, RAYWHITE);
        ui_hairline(center_x - 60.0f, center_y + 14.0f, 120.0f, HAIRLINE);
        ui_label("p to resume", center_x - ui_measure_label("p to resume", 9.5f).x * 0.5f,
                 center_y + 28.0f, 9.5f, COLOR_MUTED);
    }

    if (game_is_finished(context->game)) {
        render_result_overlay(context);
    }

    render_quit_progress(context);
}
