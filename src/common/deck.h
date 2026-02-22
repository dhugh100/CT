// Copyright (c) 2026 Dave Hugh. All rights reserved.
// Licensed under the GPL v3.0 License. See README.md for details.
#ifndef DECK_H
#define DECK_H

#include "types.h"

// Deck initialization and shuffling
void init_deck(char deck[DECK_SIZE]);
void shuffle_deck(char deck[DECK_SIZE], char n, unsigned int *seed);
void deal(char deck[DECK_SIZE], char raw_hand[PLAYERS][DECK_SIZE]);

// Hand formatting
void make_formatted_hands(State *sp, char raw_hand[PLAYERS][DECK_SIZE]);

// High-level deal function
void make_cards_and_deal(State *sp);

// Scoring
void init_score(Score *s);
char add_game(Card card);
int score(State *sp);

#endif // DECK_H
