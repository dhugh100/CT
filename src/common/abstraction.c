// Copyright (c) 2026 Dave Hugh. All rights reserved.
// Licensed under the GPL v3.0 License. See README.md for details.
#include "game.h"
#include "abstraction.h"

// History of led and response cards, split by trump/other (non-trump) with 5 rank buckets.
// h_type encodes both context (upper nibble) and rank bucket (lower nibble):
//   Upper nibble: bit7=Led Trump, bit6=Resp Trump, bit5=Led Other, bit4=Resp Other
//   Lower nibble: 1=H, 2=J, 3=10, 4=M, 5=L
//
// Bit budget per context — Trump (1 suit): H=2, J=1, 10=1, M=3, L=2 → 9 bits
//                          Other (3 suits): H=4, J=2, 10=2, M=4, L=4 → 16 bits
// Total: 2×9 + 2×16 = 50 bits packed into 7 bytes (bytes 3-9), spare bits in byte 5.
//
// Byte 3: [LTH:2|RTH:2|LTJ:1|RTJ:1|LT10:1|RT10:1]   Trump H/J/10 pairs
// Byte 4: [LTM:3|RTM:3|LTL:2]                         Trump M + LTL
// Byte 5: [RTL:2|spare:6]                              Trump RTL
// Byte 6: [LOH:4|LOJ:2|LO10:2]                        Led Other H/J/10
// Byte 7: [LOM:4|LOL:4]                                Led Other M/L
// Byte 8: [ROH:4|ROJ:2|RO10:2]                        Resp Other H/J/10
// Byte 9: [ROM:4|ROL:4]                                Resp Other M/L
void abs_history(State *s, Key *k)
{
    for (UC i = 0; i < HAND_SIZE; i++) {
        Card c = s->hp[s->to_act].card[i];
        UC f = s->h_type[s->to_act][i];

        if (c.rank == 0) break; // Cards are added from index 0; first zero means done

        // Lower nibble of h_type: 1=H, 2=J, 3=10, 4=M, 5=L
        UC rank = f & 0x0F;

        if (f & 0x80) {          // Led Trump (bytes 3-5)
            switch (rank) {
                case 1: k->bits[3] += 0x40; break; // LTH  [7:6]
                case 2: k->bits[3] += 0x08; break; // LTJ  [3]
                case 3: k->bits[3] += 0x02; break; // LT10 [1]
                case 4: k->bits[4] += 0x20; break; // LTM  [7:5]
                case 5: k->bits[4] += 0x01; break; // LTL  [1:0]
            }
        }
        else if (f & 0x40) {     // Response Trump (bytes 3-5)
            switch (rank) {
                case 1: k->bits[3] += 0x10; break; // RTH  [5:4]
                case 2: k->bits[3] += 0x04; break; // RTJ  [2]
                case 3: k->bits[3] += 0x01; break; // RT10 [0]
                case 4: k->bits[4] += 0x04; break; // RTM  [4:2]
                case 5: k->bits[5] += 0x40; break; // RTL  [7:6]
            }
        }
        else if (f & 0x20) {     // Led Other (bytes 6-7)
            switch (rank) {
                case 1: k->bits[6] += 0x10; break; // LOH  [7:4]
                case 2: k->bits[6] += 0x04; break; // LOJ  [3:2]
                case 3: k->bits[6] += 0x01; break; // LO10 [1:0]
                case 4: k->bits[7] += 0x10; break; // LOM  [7:4]
                case 5: k->bits[7] += 0x01; break; // LOL  [3:0]
            }
        }
        else if (f & 0x10) {     // Response Other (bytes 8-9)
            switch (rank) {
                case 1: k->bits[8] += 0x10; break; // ROH  [7:4]
                case 2: k->bits[8] += 0x04; break; // ROJ  [3:2]
                case 3: k->bits[8] += 0x01; break; // RO10 [1:0]
                case 4: k->bits[9] += 0x10; break; // ROM  [7:4]
                case 5: k->bits[9] += 0x01; break; // ROL  [3:0]
            }
        }
    }
}

// Create abstract of cards in hand for the Key
// - Distinguish between trump and non-trump cards, with 5 rank buckets each:
// - High (A,K,Q: 12-14), Jack (J, 11), Ten (T, 10), Medium (5-9), Low (2-4)
// - Special case: pretrump counting dumps into in-hand non-trump category
// === Trump counters
// - Byte 10: TH: 7-6, TJ: 5, T10: 4, TM: 3-0
// - Byte 5: TL: 5-4  (spare from history counters)
// === Other counters
// - Byte 11: OH: 7-6, OJ: 5, O10: 4, OM: 3-0
// - Byte 5: OL: 3-2
void abs_cards_in_hand(State *s, Key *k)
{
    // Loop through hand and update counters
    for (UC i = 0; i < HAND_SIZE; i++) {
        Card c = s->hand[s->to_act].card[i];
        if (c.rank == 0) break; // No more cards in hand

        UC bucket = get_bucket(c);

        // Use non-trump for both pre-trump and not-trump, byte 10, 5
        if (s->trump != c.suit) {
            if (bucket == 0) k->bits[10] += 0x40;      // 10, 7-6, H
            else if (bucket == 1) k->bits[10] += 0x20; // 10, 5, J
            else if (bucket == 2) k->bits[10] += 0x10; // 10, 5, 10
            else if (bucket == 3) k->bits[10] += 0x01; // 10, 3-0, M
            else k->bits[5] += 0x40;                   // 5, 7-6, L
        }
        // Trump cards go in bytes 11,5
        else if (c.suit == s->trump) {
            if (bucket == 0) k->bits[11] += 0x40;      // 10, 7-6, H
            else if (bucket == 1) k->bits[11] += 0x20; // 10, 5, J
            else if (bucket == 2) k->bits[11] += 0x10; // 10, 5, 10
            else if (bucket == 3) k->bits[11] += 0x01; // 10, 3-0, M
            else k->bits[5] += 0x20;                   // 5, 5-4, L
        }
        else {
            assert(false && "Invalid card rank in hand"); // Should never happen due to get_bucket validation
        }                                                   // 
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

    // Bytes 3-9: History counters (byte 10 spare)
    abs_history(sp, &k);
    // Bytes 10-11: Cards in hand counters
    abs_cards_in_hand(sp, &k);

    return k;
}
