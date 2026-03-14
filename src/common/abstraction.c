// Copyright (c) 2026 Dave Hugh. All rights reserved.
// Licensed under the GPL v3.0 License. See README.md for details.
#include "game.h"
#include "abstraction.h"

// History counters for both players, packed one byte per context:
//   Trump byte: [TH:2 | TJ:1 | TL:2 | TG:3]
//   Other byte: [OP:3 | ON:4 | spare:1]
//
// Key layout:
//   Byte  3: P0 Led   Trump
//   Byte  4: P0 Resp  Trump
//   Byte  5: P0 Led   Other
//   Byte  6: P0 Resp  Other
//   Byte  7: P1 Led   Trump
//   Byte  8: P1 Resp  Trump
//   Byte  9: P1 Led   Other
//   Byte 10: P1 Resp  Other

// Trump byte deltas indexed by action & 0x0F (1=TH, 2=TJ, 3=TL, 4=TG)
static const UC trump_delta[] = {0, 0x40, 0x20, 0x08, 0x01};
// Other byte deltas indexed by action & 0x0F (1=OP, 2=ON)
static const UC other_delta[] = {0, 0x20, 0x02};

void abs_history(State *s, Key *k)
{
    for (UC p = 0; p < PLAYERS; p++) {
        UC base = 3 + p * 4;  // P0: bytes 3-6, P1: bytes 7-10
        for (UC i = 0; i < HAND_SIZE; i++) {
            Card c = s->hp[p].card[i];
            UC f = s->h_type[p][i];
            if (c.rank == 0) break;

            UC cat = f & 0x0F;
            if      (f & LT) k->bits[base + 0] += trump_delta[cat];
            else if (f & RT) k->bits[base + 1] += trump_delta[cat];
            else if (f & LO) k->bits[base + 2] += other_delta[cat];
            else if (f & RO) k->bits[base + 3] += other_delta[cat];
        }
    }
}

// In-hand counters for the actor:
//   Byte 11: Trump  [TH:2 | TJ:1 | TL:2 | TG:3]
//   Byte 12: Other  [OP:3 | ON:4 | spare:1]
// Pre-trump cards count as Other since trump is not yet declared
void abs_cards_in_hand(State *s, Key *k)
{
    for (UC i = 0; i < HAND_SIZE; i++) {
        Card c = s->hand[s->to_act].card[i];
        if (c.rank == 0) break;

        if (s->trump != PRE_TRUMP && c.suit == s->trump)
            k->bits[11] += trump_delta[get_trump_cat(c) & 0x0F];
        else
            k->bits[12] += other_delta[get_other_cat(c) & 0x0F];
    }
}

// Build the key for the current state
// - The key is a compact representation of the game state
// - Designed to capture all relevant information for decision-making while minimizing memory usage
// - This includes generic game state information and counters for the player's current hand and history of play
// - Split by trump/non-trump and Face/Rank
Key build_key(State *sp)
{
    Key k = {0};
    memset(&k, 0x00, sizeof(Key));

    // Byte 0 - Game state info
    k.bits[0] |= (sp->dealer & 0b1) << 7;
    k.bits[0] |= (sp->bid[0] & 0b11) << 5;
    k.bits[0] |= (sp->bid[1] & 0b11) << 3;
    k.bits[0] |= (sp->bid_forced & 0b1) << 2;
    k.bits[0] |= (sp->bid_stolen & 0b1) << 1;
    k.bits[0] |= (sp->winning_bidder & 0b1);

    // Byte 1 - More game state
    k.bits[1] |= (sp->winning_bid & 0b11) << 6;
    k.bits[1] |= (sp->trump & 0b111) << 3;
    k.bits[1] |= (sp->leader & 0b1) << 2;
    k.bits[1] |= (sp->to_act & 0b1) << 1;
    k.bits[1] |= (sp->stage & 0b1);

    // Byte 2 - Trick and suit info
    k.bits[2] |= (sp->trick_num & 0b111) << 5;
    k.bits[2] |= (sp->led_suit & 0b11) << 3;
    // Bits 2:2-0 - Led action category (responder only; 0=actor is leader)
    // 1=TH, 2=TJ, 3=TL, 4=TG, 5=OP, 6=ON
    if (sp->stage == PLAY && sp->to_act != sp->leader) {
        Card lc = sp->hp[sp->leader].card[sp->trick_num];
        UC la;
        if (sp->trump != PRE_TRUMP && lc.suit == sp->trump)
            la = get_trump_cat(lc) & 0x0F;          // 1=TH, 2=TJ, 3=TL, 4=TG
        else
            la = 4 + (get_other_cat(lc) & 0x0F);   // 5=OP, 6=ON
        k.bits[2] |= (la & 0b111);
    }

    // Bytes 3-9: History counters (byte 10 spare)
    abs_history(sp, &k);
    // Bytes 11-12: Cards in hand counters (actor only — opponent hand is private)
    abs_cards_in_hand(sp, &k);

    // Byte 13 - Tricks won by actor so far
    k.bits[13] |= (sp->tricks_won[sp->to_act] & 0b111) << 5;

    return k;
}
