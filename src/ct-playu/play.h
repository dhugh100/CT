// Copyright (c) 2026 Dave Hugh. All rights reserved.
// Licensed under the GPL v3.0 License. See README.md for details.
#ifndef PLAY_GAME_H
#define PLAY_GAME_H

#include "types.h"

// Forward declarations
int play_game(Strat_255 *, long, UC, UC, unsigned int);
void flush_input_buffer(void);
void flush_if_needed(const char *buf);
void card_string(const Card *, int, char *, int);
void doc_state(State *, UC, UC);
void log_msg(const char *);

#endif // PLAY_GAME_H
