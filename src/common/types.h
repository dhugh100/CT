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

// Play action codes — bit 7 = trump, bit 6 = other; lower nibble = subcategory
// Trump:  TH=AKQ(3), TJ=Jack(1), TL=Low 2-4(3), TG=General 5-10(6)
// Other:  OP=Point 10-A(5), ON=Non-point 2-9(8)
// Pre-trump cards use OP/ON since all cards are non-trump before trump is declared
#define TH 0b10000001   // Trump High (A,K,Q)
#define TJ 0b10000010   // Trump Jack
#define TL 0b10000011   // Trump Low (2-4)
#define TG 0b10000100   // Trump General (5-10)
#define OP 0b01000001   // Other Point (10-A)
#define ON 0b01000010   // Other Non-point (2-9)

// h_type context flags — upper nibble of s->h_type[p][i]
// Lower nibble stores action & 0x0F: 1=TH/OP, 2=TJ/ON, 3=TL, 4=TG
#define LT 0b10000000   // Led Trump
#define RT 0b01000000   // Resp Trump
#define LO 0b00100000   // Led Other
#define RO 0b00010000   // Resp Other

// Default score values
#define DEFAULT_LOW 15   // Higher than any card rank
#define DEFAULT_HIGH 0   // Lower than any card rank

// Maximum actions per decision point (4 trump + 2 other)
#define MAX_ACTIONS 6

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
    Card cards_won[PLAYERS][HAND_SIZE * 2]; // Cards won by each player (for score calculation)    

    // Scoring
    Score score[PLAYERS];        // Score breakdown
    char t_score[PLAYERS];       // Total score (can be negative if set)
} State;

// Key structure (13 bytes for state abstraction)
typedef struct {
    UC bits[13];
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
// - Output from training, input to kwayp merge
typedef struct {
    UC bits[sizeof(Key)];           // Key
    UC action_count;                // Number of actions
    UC action[MAX_ACTIONS];         // Actions
    float strategy[MAX_ACTIONS];    // Average strategy
} Strat;

// Strategy structure with 255 ranks (for memory-efficient eval loading)
// - Output from kwayp, input to eval
typedef struct {
    UC bits[sizeof(Key)];           // Key
    UC action_count;                // Number of actions
    UC action[MAX_ACTIONS];         // Actions
    UC s255[MAX_ACTIONS];           // Float broken down into 255 ranks to save memory in eval load
} Strat_255;

#endif // TYPES_H
