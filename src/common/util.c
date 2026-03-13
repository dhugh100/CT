// Copyright (c) 2026 Dave Hugh. All rights reserved.
// Licensed under the GPL v3.0 License. See README.md for details.
#include "util.h"
#include <stddef.h>

// ---------------------------------------------------------------------------
// Internal helpers (not exposed in header)
// ---------------------------------------------------------------------------

static void print_byte_bin(UC b)
{
    for (int i = 7; i >= 0; i--)
        printf("%d", (b >> i) & 1);
}

static const char *action_mnemonic(UC action)
{
    switch (action) {
        case TH: return "TH";   // Trump High (A,K,Q)
        case TJ: return "TJ";   // Trump Jack
        case TL: return "TL";   // Trump Low (2-4)
        case TG: return "TG";   // Trump General (5-10)
        case OP: return "OP";   // Other Point (10-A)
        case ON: return "ON";   // Other Non-point (2-9)
        // Bid actions (raw values 0-3, no overlap with play action bytes)
        case 0:  return "PA";   // Pass
        case 1:  return "B2";   // Bid 2pts
        case 2:  return "B3";   // Bid 3pts
        case 3:  return "B4";   // Bid 4pts
        default: return "??";
    }
}

// trump stored in 3 bits; PRE_TRUMP(0xFF) & 0x7 == 7
static const char *trump_str(UC val)
{
    switch (val) {
        case C: return "C";
        case D: return "D";
        case H: return "H";
        case S: return "S";
        case 7: return "PRE";
        default: return "?";
    }
}

static const char *suit_str(UC suit)
{
    switch (suit & 0x3) {
        case C: return "C";
        case D: return "D";
        case H: return "H";
        case S: return "S";
        default: return "?";
    }
}

// Print raw bytes as binary, groups of 4, wrapping at col 120
// Format per byte: "[NNN]BBBBBBBB " = 14 chars; groups separated by "  "
static void dump_binary(const void *data, size_t len, size_t base_idx)
{
    const UC *p = (const UC *)data;
    int col = 0;
    for (size_t i = 0; i < len; i++) {
        // At the start of every group of 4 (after the first), emit separator or newline
        if (i > 0 && i % 4 == 0) {
            if (col + 4 * 14 > 120) {
                printf("\n");
                col = 0;
            } else {
                printf("  ");
                col += 2;
            }
        }
        printf("[%3zu]", base_idx + i);
        print_byte_bin(p[i]);
        printf(" ");
        col += 14; // "[NNN]" (5) + 8 bits + " " (1) = 14
    }
    if (col > 0) printf("\n");
}

// Print raw bytes as hex, 16 per line
static void dump_hex(const void *data, size_t len)
{
    const UC *p = (const UC *)data;
    for (size_t i = 0; i < len; i++) {
        if (i % 16 == 0) printf("    ");
        printf("%02x ", p[i]);
        if (i % 16 == 15) printf("\n");
    }
    if (len % 16 != 0) printf("\n");
}

// Decode and print the 13-byte key
static void print_key_decoded(const UC *bits)
{
    // --- binary ---
    printf("Key binary:\n");
    for (int i = 0; i < (int)sizeof(Key); i++) {
        printf("  [%2d] ", i);
        print_byte_bin(bits[i]);
        printf("\n");
    }

    // --- hex ---
    printf("Key hex:\n  ");
    for (int i = 0; i < (int)sizeof(Key); i++)
        printf("%02x ", bits[i]);
    printf("\n");

    // --- bytes 0-2: state fields ---
    UC dealer         = (bits[0] >> 7) & 0x1;
    UC bid0           = (bits[0] >> 5) & 0x3;
    UC bid1           = (bits[0] >> 3) & 0x3;
    UC bid_forced     = (bits[0] >> 2) & 0x1;
    UC bid_stolen     = (bits[0] >> 1) & 0x1;
    UC winning_bidder =  bits[0]       & 0x1;
    UC winning_bid    = (bits[1] >> 6) & 0x3;
    UC trump          = (bits[1] >> 3) & 0x7;
    UC leader         = (bits[1] >> 2) & 0x1;
    UC to_act         = (bits[1] >> 1) & 0x1;
    UC stage          =  bits[1]       & 0x1;
    UC trick_num      = (bits[2] >> 5) & 0x7;
    UC led_suit       = (bits[2] >> 3) & 0x3;

    printf("State (bytes 0-2):\n");
    printf("  [0] "); print_byte_bin(bits[0]);
    printf("  dealer=P%d  bid[0]=%d  bid[1]=%d  bid_forced=%d  bid_stolen=%d  winning_bidder=P%d\n",
           dealer, bid0, bid1, bid_forced, bid_stolen, winning_bidder);
    printf("  [1] "); print_byte_bin(bits[1]);
    printf("  winning_bid=%d  trump=%s  leader=P%d  to_act=P%d  stage=%s\n",
           winning_bid, trump_str(trump), leader, to_act, stage == BID ? "BID" : "PLAY");
    printf("  [2] "); print_byte_bin(bits[2]);
    printf("  trick_num=%d  led_suit=%s\n", trick_num, suit_str(led_suit));

    // --- bytes 3-10: history counters; trump byte [TH:2|TJ:1|TL:2|TG:3], other byte [OP:3|ON:4|spare:1] ---
    printf("History (bytes 3-10):\n");
    const char *ctx[] = {"LedTrump ", "RespTrump", "LedOther ", "RespOther"};
    for (int p = 0; p < 2; p++) {
        UC base = 3 + p * 4;
        for (int c = 0; c < 4; c++) {
            UC b = bits[base + c];
            printf("  P%d %s [%2d] ", p, ctx[c], base + c);
            print_byte_bin(b);
            if (c < 2)  // trump byte
                printf("  TH=%d TJ=%d TL=%d TG=%d\n",
                       (b >> 6) & 0x3, (b >> 5) & 0x1, (b >> 3) & 0x3, b & 0x7);
            else        // other byte
                printf("  OP=%d ON=%d\n", (b >> 5) & 0x7, (b >> 1) & 0xF);
        }
    }

    // --- bytes 11-12: in-hand counters ---
    printf("In-hand (bytes 11-12):\n");
    UC t = bits[11], o = bits[12];
    printf("  Trump [11] "); print_byte_bin(t);
    printf("  TH=%d TJ=%d TL=%d TG=%d\n", (t >> 6) & 0x3, (t >> 5) & 0x1, (t >> 3) & 0x3, t & 0x7);
    printf("  Other [12] "); print_byte_bin(o);
    printf("  OP=%d ON=%d\n", (o >> 5) & 0x7, (o >> 1) & 0xF);
}

// Simple LCG random number generator
// Returns a random number between min and max (inclusive)
// Uses upper bits (>> 16) to avoid low-bit cycling artifacts in LCG output
unsigned char get_random(unsigned char min, unsigned char max, unsigned int *seed)
{
    *seed = (*seed * 1103515245 + 12345) & 0x7fffffff;
    return min + ((*seed >> 16) % (max - min + 1));
}

// Print a single card
void print_card(Card c)
{
    char suits[] = {'C', 'D', 'H', 'S'};
    char *ranks[] = {"", "", "2", "3", "4", "5", "6", "7", "8", "9", "10", "J", "Q", "K", "A"};
    
    if (c.rank >= 2 && c.rank <= 14 && c.suit <= 3) {
        printf("%s%c", ranks[c.rank], suits[c.suit]);
    } else {
        printf("--");
    }
}

// Print a hand of cards
void print_hand(Hand *h, int size)
{
    for (int i = 0; i < size; i++) {
        if (h->card[i].rank == 0) break;
        print_card(h->card[i]);
        printf(" ");
    }
    printf("\n");
}

// Print game state (for debugging)
void print_state(State *s)
{
    printf("=== Game State ===\n");
    printf("Stage: %s\n", s->stage == BID ? "BID" : "PLAY");
    printf("Dealer: P%d\n", s->dealer);
    printf("To Act: P%d\n", s->to_act);
    
    if (s->stage == BID) {
        printf("Bids: P0=%d, P1=%d\n", s->bid[0], s->bid[1]);
    } else {
        printf("Trump: ");
        if (s->trump == PRE_TRUMP) printf("Not declared\n");
        else printf("%c\n", "CDHS"[s->trump]);
        
        printf("Leader: P%d\n", s->leader);
        printf("Trick: %d/%d\n", s->trick_num, HAND_SIZE);
        printf("Led suit: %c\n", "CDHS"[s->led_suit]);
    }
    
    printf("\nP0 hand: "); print_hand(&s->hand[0], HAND_SIZE - s->trick_num);
    printf("P1 hand: "); print_hand(&s->hand[1], HAND_SIZE - s->trick_num);
    printf("\n");
}

// Print key in hex
void print_key(Key *k)
{
    printf("Key: ");
    for (int i = 0; i < 9; i++) {
        printf("%02x ", k->bits[i]);
    }
    printf("\n");
}

// Print key in binary
void print_key_binary(Key *k)
{
    printf("Key (binary):\n");
    for (int i = 0; i < 9; i++) {
        printf("Byte %d: ", i);
        for (int b = 7; b >= 0; b--) {
            printf("%d", (k->bits[i] >> b) & 1);
        }
    }
}

// ---------------------------------------------------------------------------
// print_node - full Node dump including regrets
// ---------------------------------------------------------------------------
void print_node(Node *n)
{
    printf("=== NODE ===\n");

    // Raw binary of struct fields
    printf("Node raw bytes (binary):\n");
    printf("  key[%zu]:\n", sizeof(Key));
    dump_binary(&n->key, sizeof(Key), 0);
    printf("  action_count:\n");
    dump_binary(&n->action_count, 1, sizeof(Key));
    printf("  action[%d]:\n", MAX_ACTIONS);
    dump_binary(n->action, MAX_ACTIONS, sizeof(Key) + 1);
    printf("  regret_sum[%d] (floats):\n", MAX_ACTIONS);
    dump_binary(n->regret_sum, MAX_ACTIONS * sizeof(float),
                sizeof(Key) + 1 + MAX_ACTIONS);
    printf("  strategy[%d] (floats):\n", MAX_ACTIONS);
    dump_binary(n->strategy, MAX_ACTIONS * sizeof(float),
                sizeof(Key) + 1 + MAX_ACTIONS + MAX_ACTIONS * sizeof(float));
    printf("  strategy_sum[%d] (floats):\n", MAX_ACTIONS);
    dump_binary(n->strategy_sum, MAX_ACTIONS * sizeof(float),
                sizeof(Key) + 1 + MAX_ACTIONS + 2 * MAX_ACTIONS * sizeof(float));
    printf("  visits (int):\n");
    dump_binary(&n->visits, sizeof(int),
                sizeof(Key) + 1 + MAX_ACTIONS + 3 * MAX_ACTIONS * sizeof(float));

    // Raw hex of struct fields
    printf("Node raw bytes (hex):\n");
    printf("  key:         "); dump_hex(&n->key, sizeof(Key));
    printf("  action_count:"); dump_hex(&n->action_count, 1);
    printf("  action:      "); dump_hex(n->action, MAX_ACTIONS);
    printf("  regret_sum:  "); dump_hex(n->regret_sum, MAX_ACTIONS * sizeof(float));
    printf("  strategy:    "); dump_hex(n->strategy, MAX_ACTIONS * sizeof(float));
    printf("  strategy_sum:"); dump_hex(n->strategy_sum, MAX_ACTIONS * sizeof(float));
    printf("  visits:      "); dump_hex(&n->visits, sizeof(int));

    // Action count and decoded actions
    printf("Action count: %d\n", n->action_count);
    printf("Actions:      ");
    for (int i = 0; i < n->action_count; i++)
        printf("[%02x=%s] ", n->action[i], action_mnemonic(n->action[i]));
    printf("\n");

    // Regrets
    printf("Regret sums:  ");
    for (int i = 0; i < n->action_count; i++)
        printf("[%s: %8.4f] ", action_mnemonic(n->action[i]), n->regret_sum[i]);
    printf("\n");

    // Strategy
    printf("Strategy:     ");
    for (int i = 0; i < n->action_count; i++)
        printf("[%s: %8.4f] ", action_mnemonic(n->action[i]), n->strategy[i]);
    printf("\n");

    // Strategy sum
    printf("Strategy sum: ");
    for (int i = 0; i < n->action_count; i++)
        printf("[%s: %8.4f] ", action_mnemonic(n->action[i]), n->strategy_sum[i]);
    printf("\n");

    printf("Visits: %d\n", n->visits);

    // Key decoded
    print_key_decoded(n->key.bits);
    printf("============\n");
}

// ---------------------------------------------------------------------------
// print_strategy - Strat dump (no regrets)
// ---------------------------------------------------------------------------
void print_strategy(Strat *s)
{
    printf("=== STRATEGY ===\n");

    // Raw binary of struct fields
    printf("Strat raw bytes (binary):\n");
    printf("  bits[%zu]:\n", sizeof(Key));
    dump_binary(s->bits, sizeof(Key), 0);
    printf("  action_count:\n");
    dump_binary(&s->action_count, 1, sizeof(Key));
    printf("  action[%d]:\n", MAX_ACTIONS);
    dump_binary(s->action, MAX_ACTIONS, sizeof(Key) + 1);
    printf("  strategy[%d] (floats):\n", MAX_ACTIONS);
    dump_binary(s->strategy, MAX_ACTIONS * sizeof(float),
                sizeof(Key) + 1 + MAX_ACTIONS);

    // Raw hex of struct fields
    printf("Strat raw bytes (hex):\n");
    printf("  bits:        "); dump_hex(s->bits, sizeof(Key));
    printf("  action_count:"); dump_hex(&s->action_count, 1);
    printf("  action:      "); dump_hex(s->action, MAX_ACTIONS);
    printf("  strategy:    "); dump_hex(s->strategy, MAX_ACTIONS * sizeof(float));

    // Action count and decoded actions
    printf("Action count: %d\n", s->action_count);
    printf("Actions:      ");
    for (int i = 0; i < s->action_count; i++)
        printf("[%02x=%s] ", s->action[i], action_mnemonic(s->action[i]));
    printf("\n");

    // Strategy
    printf("Strategy:     ");
    for (int i = 0; i < s->action_count; i++)
        printf("[%s: %8.4f] ", action_mnemonic(s->action[i]), s->strategy[i]);
    printf("\n");

    // Key decoded
    print_key_decoded(s->bits);
    printf("================\n");
}

// ---------------------------------------------------------------------------
// print_strategy_255 - Strat_255 dump (quantized, on-disk format)
// ---------------------------------------------------------------------------
void print_strategy_255(Strat_255 *s)
{
    printf("=== STRATEGY_255 ===\n");

    // Raw binary of struct fields
    printf("Strat_255 raw bytes (binary):\n");
    printf("  bits[%zu]:\n", sizeof(Key));
    dump_binary(s->bits, sizeof(Key), 0);
    printf("  action_count:\n");
    dump_binary(&s->action_count, 1, sizeof(Key));
    printf("  action[%d]:\n", MAX_ACTIONS);
    dump_binary(s->action, MAX_ACTIONS, sizeof(Key) + 1);
    printf("  s255[%d]:\n", MAX_ACTIONS);
    dump_binary(s->s255, MAX_ACTIONS, sizeof(Key) + 1 + MAX_ACTIONS);

    // Raw hex of struct fields
    printf("Strat_255 raw bytes (hex):\n");
    printf("  bits:        "); dump_hex(s->bits, sizeof(Key));
    printf("  action_count:"); dump_hex(&s->action_count, 1);
    printf("  action:      "); dump_hex(s->action, MAX_ACTIONS);
    printf("  s255:        "); dump_hex(s->s255, MAX_ACTIONS);

    // Action count and decoded actions
    printf("Action count: %d\n", s->action_count);
    printf("Actions:      ");
    for (int i = 0; i < s->action_count; i++)
        printf("[%02x=%s] ", s->action[i], action_mnemonic(s->action[i]));
    printf("\n");

    // Strategy (quantized byte + decoded float)
    printf("Strategy:     ");
    for (int i = 0; i < s->action_count; i++)
        printf("[%s: %3d/255=%.4f] ", action_mnemonic(s->action[i]),
               s->s255[i], s->s255[i] / 255.0f);
    printf("\n");

    // Key decoded
    print_key_decoded(s->bits);
    printf("===================\n");
}
