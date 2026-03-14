// Copyright (c) 2026 Dave Hugh. All rights reserved.
// Licensed under the GPL v3.0 License. See README.md for details.
#include "game.h"
#include "abstraction.h"

// History counters packed per player into a 7-byte block.
// Caller provides player index and block base byte offset into the key.
//   Actor history:    base=3  → key bytes  3-9
//   Opponent history: base=10 → key bytes 10-16
//
// h_type encodes both context (upper nibble) and rank bucket (lower nibble):
//   Upper nibble: bit7=Led Trump, bit6=Resp Trump, bit5=Led Other, bit4=Resp Other
//   Lower nibble: 1=H, 2=J, 3=10, 4=M, 5=L
//
// Bit budget per context — Trump (1 suit): H=2, J=1, 10=1, M=3, L=2 → 9 bits
//                          Other (3 suits): H=4, J=2, 10=2, M=4, L=4 → 16 bits
// Total: 2×9 + 2×16 = 50 bits packed into 7 bytes
//
// Block-relative byte layout:
//   +0: [LTH:2|RTH:2|LTJ:1|RTJ:1|LT10:1|RT10:1]   Trump H/J/10 pairs
//   +1: [LTM:3|RTM:3|LTL:2]                         Trump M + LTL
//   +2: [RTL:2|spare:6]                              Trump RTL
//   +3: [LOH:4|LOJ:2|LO10:2]                        Led Other H/J/10
//   +4: [LOM:4|LOL:4]                                Led Other M/L
//   +5: [ROH:4|ROJ:2|RO10:2]                        Resp Other H/J/10
//   +6: [ROM:4|ROL:4]                                Resp Other M/L
static void abs_history(State *s, Key *k, UC player, UC base)
{
    for (UC i = 0; i < HAND_SIZE; i++) {
        Card c = s->hp[player].card[i];
        UC f = s->h_type[player][i];

        if (c.rank == 0) break; // Cards are added from index 0; first zero means done

        // Lower nibble of h_type: 1=H, 2=J, 3=10, 4=M, 5=L
        UC rank = f & 0x0F;

        if (f & 0x80) {          // Led Trump (block +0, +1)
            switch (rank) {
                case 1: k->bits[base + 0] += 0x40; break; // LTH  [7:6]
                case 2: k->bits[base + 0] += 0x08; break; // LTJ  [3]
                case 3: k->bits[base + 0] += 0x02; break; // LT10 [1]
                case 4: k->bits[base + 1] += 0x20; break; // LTM  [7:5]
                case 5: k->bits[base + 1] += 0x01; break; // LTL  [1:0]
            }
        }
        else if (f & 0x40) {     // Response Trump (block +0, +1, +2)
            switch (rank) {
                case 1: k->bits[base + 0] += 0x10; break; // RTH  [5:4]
                case 2: k->bits[base + 0] += 0x04; break; // RTJ  [2]
                case 3: k->bits[base + 0] += 0x01; break; // RT10 [0]
                case 4: k->bits[base + 1] += 0x04; break; // RTM  [4:2]
                case 5: k->bits[base + 2] += 0x40; break; // RTL  [7:6]
            }
        }
        else if (f & 0x20) {     // Led Other (block +3, +4)
            switch (rank) {
                case 1: k->bits[base + 3] += 0x10; break; // LOH  [7:4]
                case 2: k->bits[base + 3] += 0x04; break; // LOJ  [3:2]
                case 3: k->bits[base + 3] += 0x01; break; // LO10 [1:0]
                case 4: k->bits[base + 4] += 0x10; break; // LOM  [7:4]
                case 5: k->bits[base + 4] += 0x01; break; // LOL  [3:0]
            }
        }
        else if (f & 0x10) {     // Response Other (block +5, +6)
            switch (rank) {
                case 1: k->bits[base + 5] += 0x10; break; // ROH  [7:4]
                case 2: k->bits[base + 5] += 0x04; break; // ROJ  [3:2]
                case 3: k->bits[base + 5] += 0x01; break; // RO10 [1:0]
                case 4: k->bits[base + 6] += 0x10; break; // ROM  [7:4]
                case 5: k->bits[base + 6] += 0x01; break; // ROL  [3:0]
            }
        }
    }
}

// In-hand counters for the actor (opponent hand is private).
// Pre-trump cards count as Other since trump is not yet declared.
//
//   Byte 17: [TH:2|TJ:1|T10:1|TM:4]   Trump H/J/10/M
//   Byte 18: [OH:2|OJ:1|O10:1|OM:4]   Other H/J/10/M
//   Byte  5: [RTL:2|TL:2|OL:2|spare:2] Low counts packed into actor RTL spare bits
static void abs_cards_in_hand(State *s, Key *k)
{
    for (UC i = 0; i < HAND_SIZE; i++) {
        Card c = s->hand[s->to_act].card[i];
        if (c.rank == 0) break;

        UC bucket = get_bucket(c);

        if (s->trump != c.suit) {  // Other or pre-trump (PRE_TRUMP=0xFF never matches a suit)
            if      (bucket == 0) k->bits[18] += 0x40; // OH  [7:6]
            else if (bucket == 1) k->bits[18] += 0x20; // OJ  [5]
            else if (bucket == 2) k->bits[18] += 0x10; // O10 [4]
            else if (bucket == 3) k->bits[18] += 0x01; // OM  [3:0]
            else                  k->bits[5]  += 0x04; // OL  byte 5 [3:2]
        } else {                   // Trump
            if      (bucket == 0) k->bits[17] += 0x40; // TH  [7:6]
            else if (bucket == 1) k->bits[17] += 0x20; // TJ  [5]
            else if (bucket == 2) k->bits[17] += 0x10; // T10 [4]
            else if (bucket == 3) k->bits[17] += 0x01; // TM  [3:0]
            else                  k->bits[5]  += 0x10; // TL  byte 5 [5:4]
        }
    }
}

// Build the key for the current state
// - The key is a compact representation of the game state
// - Designed to capture all relevant information for decision-making while minimizing memory usage
// - This includes generic game state information and counters for the player's current hand and history of play
// - Split by trump/non-trump and 5 rank buckets (H/J/10/M/L)
//
// Key layout (19 bytes):
//   Byte  0: [dealer:1|bid0:2|bid1:2|bid_forced:1|bid_stolen:1|winning_bidder:1]
//   Byte  1: [winning_bid:2|trump:3|leader:1|to_act:1|stage:1]
//   Byte  2: [trick_num:3|led_suit:2|spare:3]
//   Bytes 3-9:   Actor history    — 7-byte block (see abs_history layout)
//   Bytes 10-16: Opponent history — 7-byte block (same layout)
//   Byte 17: [TH:2|TJ:1|T10:1|TM:4]   In-hand Trump H/J/10/M
//   Byte 18: [OH:2|OJ:1|O10:1|OM:4]   In-hand Other H/J/10/M
//   Note: In-hand Low (TL/OL) packed into byte 5 spare bits [5:4]/[3:2]
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

    // Bytes 3-9:   Actor history
    abs_history(sp, &k, sp->to_act, 3);
    // Bytes 10-16: Opponent history
    abs_history(sp, &k, 1 - sp->to_act, 10);
    // Bytes 17-18: In-hand counters; Low in byte 5 [5:4]/[3:2]
    abs_cards_in_hand(sp, &k);

    return k;
}
