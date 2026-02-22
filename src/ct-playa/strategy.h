// Copyright (c) 2026 Dave Hugh. All rights reserved.
// Licensed under the GPL v3.0 License. See README.md for details.
#ifndef STRATEGY_H
#define STRATEGY_H

#include "types.h"
#include "game.h"

// Strategy loading and querying
Strat *load_strategy(const char *filename, long *count);
int find_node(Strat *strat, long count, Key *key);
UC get_best_action(Strat *strat, long count, State *s);
void free_strategy(Strat *strat);

#endif // STRATEGY_H
