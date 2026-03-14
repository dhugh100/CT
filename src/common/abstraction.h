// Copyright (c) 2026 Dave Hugh. All rights reserved.
// Licensed under the GPL v3.0 License. See README.md for details.
#ifndef ABSTRACTION_H
#define ABSTRACTION_H

#include "types.h"

// Key building functions
Key build_key(State *sp);
void abs_history(State *s, Key *k);
void abs_cards_in_hand(State *s, Key *k);

#endif // ABSTRACTION_H
