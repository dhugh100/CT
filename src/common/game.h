// Copyright (c) 2026 Dave Hugh. All rights reserved.
// Licensed under the GPL v3.0 License. See README.md for details.
#ifndef GAME_H
#define GAME_H

#include "types.h"

// Bidding functions
int legal_bid(State *s, UC out[4]);
int apply_bid(State *s, UC bid);

// Card play functions
int legal_play(State *s, unsigned char *o);
void apply_play(State *sp, UC action);

// Helper functions for card play
bool is_legal_play(State *s, Card c);
bool match_action(Card c, UC action, UC trump);
UC bind_card_index_to_action(State *s, UC action);
void remove_card(State *sp, char p, char index);

#endif // GAME_H
