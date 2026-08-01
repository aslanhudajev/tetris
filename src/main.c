#include "config.h"
#include "game.h"
#include "menu.h"
#include "render.h"
#include "scores.h"
#include "theme.h"

#include <raylib.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

typedef struct {
    int direction;
    int last_pressed_direction;
    float das_timer;
    float arr_timer;
    float soft_drop_timer;
    bool soft_drop_held;
} InputState;

static bool key_down_any(int a, int b) {
    return IsKeyDown(a) || IsKeyDown(b);
}

static bool key_pressed_any(int a, int b) {
    return IsKeyPressed(a) || IsKeyPressed(b);
}

static int held_direction(InputState *input) {
    const bool left = key_down_any(KEY_LEFT, KEY_A);
    const bool right = key_down_any(KEY_RIGHT, KEY_D);

    if (key_pressed_any(KEY_LEFT, KEY_A)) {
        input->last_pressed_direction = -1;
    }

    if (key_pressed_any(KEY_RIGHT, KEY_D)) {
        input->last_pressed_direction = 1;
    }

    if (left && right) {
        return input->last_pressed_direction;
    }

    if (left) {
        return -1;
    }

    if (right) {
        return 1;
    }

    return 0;
}

static void handle_horizontal(GameState *game, InputState *input, float dt) {
    const int direction = held_direction(input);

    if (direction != input->direction) {
        input->direction = direction;
        input->das_timer = 0.0f;
        input->arr_timer = 0.0f;

        if (direction != 0) {
            game_move(game, direction, 0);
        }

        return;
    }

    if (direction == 0) {
        return;
    }

    input->das_timer += dt;

    if (input->das_timer < DAS_DELAY_SECONDS) {
        return;
    }

    input->arr_timer += dt;

    while (input->arr_timer >= DAS_REPEAT_SECONDS) {
        input->arr_timer -= DAS_REPEAT_SECONDS;

        if (!game_move(game, direction, 0)) {
            break;
        }
    }
}

static void handle_soft_drop(GameState *game, InputState *input, float dt) {
    if (!key_down_any(KEY_DOWN, KEY_S)) {
        input->soft_drop_held = false;
        input->soft_drop_timer = 0.0f;
        return;
    }

    if (!input->soft_drop_held) {
        input->soft_drop_held = true;
        input->soft_drop_timer = 0.0f;
        game_soft_drop(game);
        return;
    }

    input->soft_drop_timer += dt;

    while (input->soft_drop_timer >= SOFT_DROP_REPEAT_SECONDS) {
        input->soft_drop_timer -= SOFT_DROP_REPEAT_SECONDS;

        if (!game_soft_drop(game)) {
            break;
        }
    }
}

static void handle_play_input(GameState *game, InputState *input, float dt) {
    if (IsKeyPressed(KEY_P)) {
        game->paused = !game->paused;
    }

    if (game->paused) {
        return;
    }

    handle_horizontal(game, input, dt);
    handle_soft_drop(game, input, dt);

    if (IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_X) || IsKeyPressed(KEY_W)) {
        game_rotate(game, 1);
    }

    if (IsKeyPressed(KEY_Z)) {
        game_rotate(game, -1);
    }

    if (IsKeyPressed(KEY_C) || IsKeyPressed(KEY_LEFT_SHIFT) || IsKeyPressed(KEY_RIGHT_SHIFT)) {
        game_hold(game);
    }

    if (IsKeyPressed(KEY_SPACE)) {
        game_hard_drop(game);
    }
}

int main(void) {
    /* VSync alone paces the loop; adding SetTargetFPS on top makes raylib wait
       twice per frame, which shows up as periodic stutter. */
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT);
    InitWindow(WINDOW_DEFAULT_WIDTH, WINDOW_DEFAULT_HEIGHT, GAME_TITLE);
    SetWindowMinSize(460, 620);
    SetExitKey(KEY_NULL);
    srand((unsigned int)time(NULL));

    MenuState menu;
    GameState game;
    InputState input;
    ScoreTable scores;
    ThemeLibrary themes;
    AppScene scene = APP_SCENE_MENU;
    int score_rank = -1;
    bool score_submitted = false;
    float quit_hold = 0.0f;

    menu_init(&menu);
    game_start(&game, GAME_MODE_MARATHON, 1);
    memset(&input, 0, sizeof(input));
    scores_load(&scores);
    themes_load(&themes);

    while (!WindowShouldClose() && scene != APP_SCENE_QUIT) {
        float dt = GetFrameTime();
        if (dt > MAX_FRAME_DELTA) {
            dt = MAX_FRAME_DELTA;
        }

        const int window_width = GetScreenWidth();
        const int window_height = GetScreenHeight();

        if (scene == APP_SCENE_MENU) {
            const MenuAction action = menu_update(&menu, &themes, window_width, window_height);

            if (action.select_theme >= 0) {
                themes_set_active(&themes, action.select_theme);
            }

            if (action.quit) {
                scene = APP_SCENE_QUIT;
            } else if (action.start_game) {
                game_start(&game, action.mode, action.start_level);
                memset(&input, 0, sizeof(input));
                score_rank = -1;
                score_submitted = false;
                quit_hold = 0.0f;
                scene = APP_SCENE_PLAYING;
            }
        } else if (scene == APP_SCENE_PLAYING) {
            if (game_is_finished(&game) && !score_submitted) {
                score_rank = scores_submit(&scores, &game);
                score_submitted = true;
            }

            bool leave = false;

            /* A finished run has nothing left to lose, so Esc leaves at once.
               A live run must be held to avoid throwing away progress. */
            if (game_is_finished(&game)) {
                leave = IsKeyPressed(KEY_ESCAPE);

                if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER)) {
                    game_restart(&game);
                    memset(&input, 0, sizeof(input));
                    score_rank = -1;
                    score_submitted = false;
                }
            } else {
                quit_hold = IsKeyDown(KEY_ESCAPE) ? quit_hold + dt : 0.0f;
                leave = quit_hold >= QUIT_HOLD_SECONDS;

                handle_play_input(&game, &input, dt);
                game_update(&game, dt);
            }

            if (leave) {
                quit_hold = 0.0f;
                menu_open_main(&menu);
                scene = APP_SCENE_MENU;
            }
        }

        BeginDrawing();
        if (scene == APP_SCENE_MENU) {
            menu_draw(&menu, &scores, &themes, window_width, window_height);
        } else {
            const Theme *theme = themes_active(&themes);
            const RenderContext context = {
                .game = &game,
                .theme = theme,
                .window_width = window_width,
                .window_height = window_height,
                .score_rank = score_rank,
                .quit_progress = quit_hold / QUIT_HOLD_SECONDS,
            };

            render_background(theme, window_width, window_height);
            render_game(&context);
        }
        EndDrawing();
    }

    themes_unload(&themes);
    CloseWindow();
    return 0;
}
