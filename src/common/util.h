// Copyright (c) 2026 Dave Hugh. All rights reserved.
// Licensed under the GPL v3.0 License. See README.md for details.
#ifndef UTIL_H
#define UTIL_H

#include "types.h"

// Random number generation
unsigned char get_random(unsigned char min, unsigned char max, unsigned int *seed);

// Debugging/logging helpers
void print_card(Card c);
void print_hand(Hand *h, int size);
void print_state(State *s);
void print_key(Key *k);
void print_key_binary(Key *k);
void print_node(Node *n);
void print_strategy(Strat *s);

// Logging macro (can be disabled in production)
#define LOG 1
#define MSG(level, ...) do { if (level) fprintf(stderr, __VA_ARGS__); } while(0)

#endif // UTIL_H
