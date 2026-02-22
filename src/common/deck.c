// Copyright (c) 2026 Dave Hugh. All rights reserved.
// Licensed under the GPL v3.0 License. See README.md for details.
#include "deck.h"
#include "util.h"

// Initialize deck with values 0-51
void init_deck(char deck[DECK_SIZE])
{
    for (char i = 0; i < DECK_SIZE; i++) {
        deck[i] = i;
    }
}

// Swap two elements
void swap(char *a, char *b)
{
    char temp = *a;
    *a = *b;
    *b = temp;
}

// Fisher-Yates shuffle algorithm
void shuffle_deck(char deck[DECK_SIZE], char n, unsigned int *seed)
{
    for (char i = n - 1; i > 0; i--) {
        char j = get_random(0, i, seed);
        swap(&deck[i], &deck[j]);
    }
}

// Deal cards from deck to raw hands
void deal(char deck[DECK_SIZE], char raw_hand[PLAYERS][DECK_SIZE])
{
    char k = 0;
    for (char i = 0; i < PLAYERS; i++) {
        for (char j = 0; j < HAND_SIZE; j++) {
            raw_hand[i][j] = deck[k];
            k++;
        }
    }
}

// Convert raw hands (0-51) to suit/rank format
void make_formatted_hands(State *sp, char raw_hand[PLAYERS][DECK_SIZE])
{
    for (char i = 0; i < PLAYERS; i++) {
        for (char j = 0; j < HAND_SIZE; j++) {
            if (raw_hand[i][j] < 13) {
                sp->hand[i].card[j].suit = C;
                sp->hand[i].card[j].rank = raw_hand[i][j] + 2;
            } else if (raw_hand[i][j] >= 13 && raw_hand[i][j] < 26) {
                sp->hand[i].card[j].suit = D;
                sp->hand[i].card[j].rank = raw_hand[i][j] - 11;
            } else if (raw_hand[i][j] >= 26 && raw_hand[i][j] < 39) {
                sp->hand[i].card[j].suit = H;
                sp->hand[i].card[j].rank = raw_hand[i][j] - 24;
            } else if (raw_hand[i][j] >= 39 && raw_hand[i][j] < 52) {
                sp->hand[i].card[j].suit = S;
                sp->hand[i].card[j].rank = raw_hand[i][j] - 37;
            } else {
                assert(false && "Invalid card value in deck");
            }
        }
    }
}

// High-level function: deal and format hands
void make_cards_and_deal(State *sp)
{
    char deck[DECK_SIZE] = {0};
    char raw_hand[PLAYERS][DECK_SIZE] = {0};
    
    init_deck(deck);
    shuffle_deck(deck, DECK_SIZE, &sp->seed);
    deal(deck, raw_hand);
    make_formatted_hands(sp, raw_hand);
}

// Initialize score structure
void init_score(Score *s)
{
    s->low = DEFAULT_LOW;
    s->high = DEFAULT_HIGH;
    s->game = 0;
    s->jack = false;
}

// Return game points for a card
char add_game(Card card)
{
    if (card.rank == 14) return 4;       // Ace
    else if (card.rank == 13) return 3;  // King
    else if (card.rank == 12) return 2;  // Queen
    else if (card.rank == 11) return 1;  // Jack
    else if (card.rank == 10) return 10; // Ten
    return 0;
}

// Score the hand and return utility (P0 score - P1 score)
int score(State *sp)
{
    init_score(&sp->score[0]);
    init_score(&sp->score[1]);

    // Separate cards won by each player
    Card cp0[HAND_SIZE * 2] = {0};
    Card cp1[HAND_SIZE * 2] = {0};
    UC cp0c = 0, cp1c = 0;
    
    for (int i = 0; i < HAND_SIZE; i++) {
        if (sp->trick_winner[i] == 0) {
            cp0[cp0c++] = sp->hp[0].card[i];
            cp0[cp0c++] = sp->hp[1].card[i];
        } else {
            cp1[cp1c++] = sp->hp[0].card[i];
            cp1[cp1c++] = sp->hp[1].card[i];
        }
    }

    // Score P0's cards
    for (UC i = 0; i < cp0c; i++) {
        if (cp0[i].suit == sp->trump) {
            if (cp0[i].rank < sp->score[0].low)
                sp->score[0].low = cp0[i].rank;
            if (cp0[i].rank > sp->score[0].high)
                sp->score[0].high = cp0[i].rank;
            if (cp0[i].rank == 11)
                sp->score[0].jack = true;
        }
        sp->score[0].game += add_game(cp0[i]);
    }

    // Score P1's cards
    for (UC i = 0; i < cp1c; i++) {
        if (cp1[i].suit == sp->trump) {
            if (cp1[i].rank < sp->score[1].low)
                sp->score[1].low = cp1[i].rank;
            if (cp1[i].rank > sp->score[1].high)
                sp->score[1].high = cp1[i].rank;
            if (cp1[i].rank == 11)
                sp->score[1].jack = true;
        }
        sp->score[1].game += add_game(cp1[i]);
    }

    // Calculate total scores
    sp->t_score[0] = 0;
    sp->t_score[1] = 0;
    
    if (sp->score[0].low < sp->score[1].low) sp->t_score[0] += 1;
    else sp->t_score[1] += 1;
    
    if (sp->score[0].high > sp->score[1].high) sp->t_score[0] += 1;
    else sp->t_score[1] += 1;
    
    if (sp->score[0].game > sp->score[1].game) sp->t_score[0] += 1;
    else if (sp->score[1].game > sp->score[0].game) sp->t_score[1] += 1;
    
    if (sp->score[0].jack) sp->t_score[0] += 1;
    else if (sp->score[1].jack) sp->t_score[1] += 1;

    // Check for set (didn't make bid)
    // winning_bid is raw (1,2,3) representing actual points (2,3,4)
    UC bid_pts = sp->winning_bid + 1;
    if (sp->winning_bidder == 0 && sp->t_score[0] < bid_pts)
        sp->t_score[0] = bid_pts * -1;
    else if (sp->winning_bidder == 1 && sp->t_score[1] < bid_pts)
        sp->t_score[1] = bid_pts * -1;

    // Return utility for P0
    return sp->t_score[0] - sp->t_score[1];
}
