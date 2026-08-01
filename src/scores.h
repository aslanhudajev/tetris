#ifndef SCORES_H
#define SCORES_H

#include "config.h"
#include "game.h"

#include <stdbool.h>

typedef struct {
    float time_seconds;
    int score;
    int level;
    int lines;
    long date;
} ScoreEntry;

typedef struct {
    ScoreEntry sprint[SCORE_ENTRY_COUNT];
    int sprint_count;
    ScoreEntry marathon[SCORE_ENTRY_COUNT];
    int marathon_count;
} ScoreTable;

void scores_load(ScoreTable *table);
void scores_save(const ScoreTable *table);

/* Insert a finished run. Returns the zero-based rank it landed at, or -1 when
   the run did not make the table. */
int scores_submit(ScoreTable *table, const GameState *game);

void scores_format_time(float seconds, char *out, int out_size);

#endif
