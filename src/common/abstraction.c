// Copyright (c) 2026 Dave Hugh. All rights reserved.
// Licensed under the GPL v3.0 License. See README.md for details.
#include "abstraction.h"

// History of led and response cards, split by trump/Other(non-trump) and 4 rank buckets
// - Led cards are the cards that the player to act has led
// - 4 rank buckets: High (A,K,Q: 13-14), Special (J,10: 10-12), Medium (9-5: 5-9), Low (4-2: 2-4)
// - Each counter gets 4 bits (max count = 15, sufficient for max 6 cards in history)
// - If rank is 0 of card slot, that means empty, and should stop counting (cards added 0 -> 5)
// - Constants LT, LO, RT, RO are used to determine if the card was led or a response and if trump or not
// - The history type is set during play
// - Bytes 3-4: Led Trump - LTH (bits 7-4 of byte 2), LTS (bits 3-0 of byte 2),
//              LTM (bits 7-4 of byte 3), LTL (bits 3-0 of byte 3)
// - Bytes 5-6: Led Other - LOH (bits 7-4 of byte 4), LOS (bits 3-0 of byte 4),
//              LOM (bits 7-4 of byte 5), LOL (bits 3-0 of byte 5)
// - Bytes 7-8: Response Trump - RTH (bits 7-4 of byte 6), RTS (bits 3-0 of byte 6),
//              RTM (bits 7-4 of byte 7), RTL (bits 3-0 of byte 7)
// - Bytes 9-10: Response Other - ROH (bits 7-4 of byte 8), ROS (bits 3-0 of byte 8),
//              ROM (bits 7-4 of byte 9), ROL (bits 3-0 of byte 9)
void abs_history(State *s, Key *k)
{
    // Loop through played cards and update counters
    for (UC i = 0; i < HAND_SIZE; i++)
    {
        Card c = s->hp[s->to_act].card[i];
        UC f = s->h_type[s->to_act][i];

        if (c.rank == 0) break; // Stop loop, as cards are added from index 0 to 5

        // Determine which bucket this card belongs to
        UC bucket = 0;
        if (c.rank >= 12 && c.rank <= 14) bucket = 0;       // High: A, K, Q
        else if (c.rank == 10 || c.rank == 11) bucket = 1; // Special: J, 10
        else if (c.rank >= 5 && c.rank <= 9)  bucket = 2;   // Medium: 9-5
        else if (c.rank >= 2 && c.rank <= 4) bucket = 3;    // Low: 4-2
        else assert(false && "Invalid card rank");

        // Update appropriate bytes based on led/response and trump/other
        // Test only the type bit in the upper nibble to avoid rank-bit overlap.
        // Stored h_type values use: bit 7 = Led Trump, bit 5 = Led Other,
        // bit 6 = Response Trump, bit 4 = Response Other.
        if (f & 0x80) {  // Led Trump (bytes 3-4)
            if (bucket == 0) k->bits[3] += 0x10;      // LTH in upper nibble
            else if (bucket == 1) k->bits[3] += 0x01; // LTS in lower nibble
            else if (bucket == 2) k->bits[4] += 0x10; // LTM in upper nibble
            else k->bits[4] += 0x01;                  // LTL in lower nibble
        }
        else if (f & 0x20) {  // Led Other (bytes 5-6)
            if (bucket == 0) k->bits[5] += 0x10;      // LOH in upper nibble
            else if (bucket == 1) k->bits[5] += 0x01; // LOS in lower nibble
            else if (bucket == 2) k->bits[6] += 0x10; // LOM in upper nibble
            else k->bits[6] += 0x01;                  // LOL in lower nibble
        }
        else if (f & 0x40) {  // Response Trump (bytes 7-8)
            if (bucket == 0) k->bits[7] += 0x10;      // RTH in upper nibble
            else if (bucket == 1) k->bits[7] += 0x01; // RTS in lower nibble
            else if (bucket == 2) k->bits[8] += 0x10; // RTM in upper nibble
            else k->bits[8] += 0x01;                  // RTL in lower nibble
        }
        else if (f & 0x10) {  // Response Other (bytes 9-10)
            if (bucket == 0) k->bits[9] += 0x10;      // ROH in upper nibble
            else if (bucket == 1) k->bits[9] += 0x01; // ROS in lower nibble
            else if (bucket == 2) k->bits[10] += 0x10; // ROM in upper nibble
            else k->bits[10] += 0x01;                  // ROL in lower nibble
        }
    }
}
// Create abstract of cards in hand for the Key
// - Distinguish between trump and non-trump cards, with 4 rank buckets each:
//   High (A,K,Q: 13-14), Special (J,10: 10-12), Medium (9-5: 5-9), Low (4-2: 2-4)
// - Special case for pre-trump, where trump not declared
// -- Pretrump counting dumps into in-hand non-trump categories - trump distinction not meaningful
// - Bytes 11-12: Trump counters - TH (bits 7-4 of byte 11), TS (bits 3-0 of byte 11), 
//              TM (bits 7-4 of byte 12), TL (bits 3-0 of byte 12)
// - Bytes 13-14: Other counters - OH (bits 7-4 of byte 13), OS (bits 3-0 of byte 13),
//              OM (bits 7-4 of byte 14), OL (bits 3-0 of byte 14)
// - Each counter gets 4 bits (max count = 15, way more than needed but clean)
void abs_cards_in_hand(State *s, Key *k)
{
    // Loop through hand and update counters
    for (UC i = 0; i < HAND_SIZE; i++) {
        Card c = s->hand[s->to_act].card[i];
        if (c.rank == 0) break; // No more cards in hand

        UC bucket = 0; // Which rank bucket
        if (c.rank >= 12 && c.rank <= 14) bucket = 0;       // High: A, K, Q
        else if (c.rank == 10 || c.rank == 11) bucket = 1; // Special: J, 10
        else if (c.rank >= 5 && c.rank <= 9)  bucket = 2;   // Medium: 9-5
        else if (c.rank >= 2 && c.rank <= 4) bucket = 3;    // Low: 4-2

        // Use non-trump (bytes123-14) for pre-trump
        if (s->trump == PRE_TRUMP) {
            if (bucket == 0) k->bits[13] += 0x10;      // OH in upper nibble
            else if (bucket == 1) k->bits[13] += 0x01; // OS in lower nibble
            else if (bucket == 2) k->bits[14] += 0x10; // OM in upper nibble
            else k->bits[14] += 0x01;                  // OL in lower nibble
        }
        // Trump cards go in bytes 11-12
        else if (c.suit == s->trump) {
            if (bucket == 0) k->bits[11] += 0x10;      // TH in upper nibble
            else if (bucket == 1) k->bits[11] += 0x01; // TS in lower nibble
            else if (bucket == 2) k->bits[12] += 0x10; // TM in upper nibble
            else k->bits[12] += 0x01;                  // TL in lower nibble
        }
        // Non-trump cards go in bytes 13-14
        else {
            if (bucket == 0) k->bits[13] += 0x10;      // OH in upper nibble
            else if (bucket == 1) k->bits[13] += 0x01; // OS in lower nibble
            else if (bucket == 2) k->bits[14] += 0x10; // OM in upper nibble
            else k->bits[14] += 0x01;                  // OL in lower nibble
        }
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

    // Bytes 3-10: History counters
    abs_history(sp, &k);
    // Bytes 11-14: Cards in hand counters
    abs_cards_in_hand(sp, &k);

    return k;
}
