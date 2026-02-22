// Copyright (c) 2026 Dave Hugh. All rights reserved.
// Licensed under the GPL v3.0 License. See README.md for details.
#ifndef CFR_H
#define CFR_H

#include "types.h"

// Hash table configuration
#ifndef NODE_QTY
#define NODE_QTY 10000000  // Default: 10M nodes per thread
#endif

// Hash functions
unsigned int hash_key(Key *k);
unsigned int idx_hash(Key *k, int size);

// Node management
Node *get_or_create(Node **pa, Key *key, UC actions[], UC legal_n, int thread_num);

// CFR algorithm
float recurse(State *sp, Node **hash_table, int p, int thread_num);

// Regret matching
void update_strategy(Node *node);
void update_regrets(Node *node, float *action_utilities, float node_utility);

#endif // CFR_H
