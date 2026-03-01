// Copyright (c) 2026 Dave Hugh. All rights reserved.
// Licensed under the GPL v3.0 License. See README.md for details.
#ifndef TYPES_H
#define TYPES_H

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

// Unsigned char shorthand
typedef unsigned char UC;

// Game constants
#define HAND_SIZE 6
#define PLAYERS 2
#define DECK_SIZE 52

// Suits
#define C 0  // Clubs
#define D 1  // Diamonds
#define H 2  // Hearts
#define S 3  // Spades

// Pre-trump indicator (trump not yet declared)
#define PRE_TRUMP 0xff

// Game stages
#define BID 0
#define PLAY 1

// Different action types
// Bits 567 = T = Trump, O = Other (non-trump), P = pre-trump,
// Bits 0-3 = H = High (KQA), S =Special (J10), M = Medium (5-9) L = Low (2-4)

#define TH 0b10000001
#define TJ 0b10000010
#define TT 0b10000011
#define TM 0b10000100
#define TL 0b10000101
#define OH 0b01000001
#define OJ 0b01000010
#define OT 0b01000011
#define OM 0b01000100
#define OL 0b01000101
#define PH 0b00100001
#define PJ 0b00100010
#define PT 0b00100011
#define PM 0b00100100
#define PL 0b00100101

// History types for tracking how cards were played (for state abstraction)
#define LT 0b10000000
#define LO 0b01000000
#define RT 0b00100000
#define RO 0b00010000

// Different played card types
// Bits 4567 - L = led, R = response, T = trump, O = other (non-trump
// Bits 0-3 = H = High (KQA), J = J. T=10, M = Medium (5-9) L = Low (2-4)
#define LTH 0b10000001
#define LTJ 0b10000010
#define LTT 0b10000011
#define LTM 0b10000100
#define LTL 0b10000101
#define RTH 0b01000001
#define RTJ 0b01000010
#define RTT 0b01000011
#define RTM 0b01000100
#define RTL 0b01000101
#define LOH 0b00100001
#define LOJ 0b00100010
#define LOT 0b00100011
#define LOM 0b00100100
#define LOL 0b00100101
#define ROH 0b00010001
#define ROJ 0b00010010
#define ROT 0b00010011
#define ROM 0b00010100
#define ROL 0b00010101

// Default score values
#define DEFAULT_LOW 15   // Higher than any card rank
#define DEFAULT_HIGH 0   // Lower than any card rank

// Maximum actions per decision point (5 trump + 5 other, or 5 pre-trump)
#define MAX_ACTIONS 10

// Card structure
typedef struct {
    UC suit;  // C, D, H, S
    UC rank;  // 2-14 (Jack=11, Queen=12, King=13, Ace=14)
} Card;

// Hand structure (collection of cards)
typedef struct {
    Card card[HAND_SIZE];
} Hand;

// History type structure (tracks how card was played)
typedef struct {
    bool led_trump;
    bool led_other;
    bool resp_trump;
    bool resp_other;
} HistoryType;

// Score structure
typedef struct {
    UC low;      // Lowest trump card won
    UC high;     // Highest trump card won
    UC game;     // Game points (face cards and 10s)
    bool jack;   // Did we win the jack of trump?
} Score;

// Complete game state
typedef struct {
    // Game setup
    UC dealer;              // 0 or 1
    unsigned int seed;      // RNG seed
    
    // Bidding state
    UC bid[PLAYERS];        // Bids for each player (0=pass, 1=2pts, 2=3pts, 3=4pts)
    bool bid_forced;        // Was bid forced (both passed)?
    bool bid_stolen;        // Did dealer steal the bid?
    UC winning_bidder;      // Which player won the bid
    UC winning_bid;         // The winning bid amount
    
    // Play state
    UC stage;               // BID or PLAY
    UC trump;               // Trump suit (C, D, H, S) or PRE_TRUMP (0xff)
    UC leader;              // Who leads this trick
    UC to_act;              // Whose turn is it
    UC trick_num;           // Current trick number (0-5)
    UC led_suit;            // Suit led this trick
    bool hand_done;         // Is the hand complete?
    
    // Cards
    Hand hand[PLAYERS];     // Current hands
    Hand hp[PLAYERS];       // Played cards history
    
    // History tracking 
    UC h_type[PLAYERS][HAND_SIZE];
    
    // Trick results
    UC trick_winner[HAND_SIZE];  // Who won each trick
    UC tricks_won[PLAYERS];      // Total tricks won by each player
    
    // Scoring
    Score score[PLAYERS];        // Score breakdown
    char t_score[PLAYERS];       // Total score (can be negative if set)
} State;

// Key structure (12 bytes for state abstraction)
typedef struct {
    UC bits[12];
} Key;

// CFR Node structure
typedef struct Node {
    Key key;                        // State abstraction key
    UC action_count;                // Number of legal actions
    UC action[MAX_ACTIONS];         // Legal actions
    float regret_sum[MAX_ACTIONS];  // Cumulative regrets
    float strategy[MAX_ACTIONS];    // Current strategy
    float strategy_sum[MAX_ACTIONS]; // Cumulative strategy (for averaging)
    int visits;                     // Number of times visited
    struct Node *next;              // For hash table chaining
} Node;

// Strategy structure (for serialization/loading)
typedef struct {
    UC bits[sizeof(Key)];           // Key
    UC action_count;                // Number of actions
    UC action[MAX_ACTIONS];         // Actions
    float strategy[MAX_ACTIONS];    // Average strategy
} Strat;

#endif // TYPES_H
