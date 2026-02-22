// Copyright (c) 2026 Dave Hugh. All rights reserved.
// Licensed under the GPL v3.0 License. See README.md for details.
#ifndef EVAL_H
#define EVAL_H

#include "types.h"
#include "strategy.h"

// Evaluation statistics
typedef struct {
    int games_played;
    int games_won[PLAYERS];
    int hands_won[PLAYERS];
    int tricks_won[PLAYERS];
    int nodes_found;
    int nodes_not_found;
} EvalStats;

// Evaluation modes
typedef enum {
    MODE_POLICY,  // Machine uses trained policy
    MODE_RANDOM   // Both players play randomly
} EvalMode;

// Evaluation functions
void init_eval_stats(EvalStats *stats);
void print_eval_stats(EvalStats *stats);
void eval_games(Strat *strat, long strat_count, int iterations, unsigned int seed, EvalMode mode, EvalStats *stats);

#endif // EVAL_H
