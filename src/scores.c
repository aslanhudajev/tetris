#include "scores.h"

#include "platform.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define SCORES_FILE_MAGIC "PUZZIE_SCORES"
#define SCORES_FILE_VERSION 1
#define SCORES_FILE "scores.txt"

static void insert_entry(ScoreEntry *entries, int *count, int index, const ScoreEntry *entry) {
    const int limit = *count < SCORE_ENTRY_COUNT ? *count : SCORE_ENTRY_COUNT - 1;

    for (int i = limit; i > index; i--) {
        entries[i] = entries[i - 1];
    }

    entries[index] = *entry;

    if (*count < SCORE_ENTRY_COUNT) {
        (*count)++;
    }
}

void scores_load(ScoreTable *table) {
    memset(table, 0, sizeof(*table));

    char path[1024];
    if (!platform_data_path(SCORES_FILE, path, (int)sizeof(path))) {
        return;
    }

    FILE *file = fopen(path, "r");
    if (file == NULL) {
        return;
    }

    char magic[32] = {0};
    int version = 0;

    if (fscanf(file, "%31s %d", magic, &version) != 2 ||
        strcmp(magic, SCORES_FILE_MAGIC) != 0 ||
        version != SCORES_FILE_VERSION) {
        fclose(file);
        return;
    }

    char kind = 0;

    while (fscanf(file, " %c", &kind) == 1) {
        ScoreEntry entry = {0};

        if (kind == 'S') {
            if (fscanf(file, "%f %d %ld", &entry.time_seconds, &entry.level, &entry.date) != 3) {
                break;
            }

            entry.lines = SPRINT_LINE_GOAL;

            if (table->sprint_count < SCORE_ENTRY_COUNT) {
                table->sprint[table->sprint_count++] = entry;
            }
        } else if (kind == 'M') {
            if (fscanf(file, "%d %d %d %f %ld", &entry.score, &entry.level, &entry.lines,
                       &entry.time_seconds, &entry.date) != 5) {
                break;
            }

            if (table->marathon_count < SCORE_ENTRY_COUNT) {
                table->marathon[table->marathon_count++] = entry;
            }
        } else {
            break;
        }
    }

    fclose(file);
}

void scores_save(const ScoreTable *table) {
    char path[1024];

    if (!platform_data_path(SCORES_FILE, path, (int)sizeof(path))) {
        return;
    }

    FILE *file = fopen(path, "w");
    if (file == NULL) {
        return;
    }

    fprintf(file, "%s %d\n", SCORES_FILE_MAGIC, SCORES_FILE_VERSION);

    for (int i = 0; i < table->sprint_count; i++) {
        const ScoreEntry *entry = &table->sprint[i];
        fprintf(file, "S %.3f %d %ld\n", entry->time_seconds, entry->level, entry->date);
    }

    for (int i = 0; i < table->marathon_count; i++) {
        const ScoreEntry *entry = &table->marathon[i];
        fprintf(file, "M %d %d %d %.3f %ld\n", entry->score, entry->level, entry->lines,
                entry->time_seconds, entry->date);
    }

    fclose(file);
}

int scores_submit(ScoreTable *table, const GameState *game) {
    ScoreEntry entry = {0};
    entry.time_seconds = game->elapsed_seconds;
    entry.score = game->score;
    entry.level = game->level;
    entry.lines = game->lines_cleared;
    entry.date = (long)time(NULL);

    int rank = -1;

    if (game->mode == GAME_MODE_SPRINT) {
        if (!game->complete) {
            return -1;
        }

        for (int i = 0; i < table->sprint_count; i++) {
            if (entry.time_seconds < table->sprint[i].time_seconds) {
                rank = i;
                break;
            }
        }

        if (rank < 0 && table->sprint_count < SCORE_ENTRY_COUNT) {
            rank = table->sprint_count;
        }

        if (rank < 0) {
            return -1;
        }

        insert_entry(table->sprint, &table->sprint_count, rank, &entry);
    } else if (game->mode == GAME_MODE_MARATHON) {
        if (entry.score <= 0) {
            return -1;
        }

        for (int i = 0; i < table->marathon_count; i++) {
            if (entry.score > table->marathon[i].score) {
                rank = i;
                break;
            }
        }

        if (rank < 0 && table->marathon_count < SCORE_ENTRY_COUNT) {
            rank = table->marathon_count;
        }

        if (rank < 0) {
            return -1;
        }

        insert_entry(table->marathon, &table->marathon_count, rank, &entry);
    } else {
        return -1;
    }

    scores_save(table);
    return rank;
}

void scores_format_time(float seconds, char *out, int out_size) {
    if (seconds < 0.0f) {
        seconds = 0.0f;
    }

    const int total_hundredths = (int)(seconds * 100.0f + 0.5f);
    const int minutes = total_hundredths / 6000;
    const int secs = (total_hundredths / 100) % 60;
    const int hundredths = total_hundredths % 100;

    snprintf(out, (size_t)out_size, "%d:%02d.%02d", minutes, secs, hundredths);
}
