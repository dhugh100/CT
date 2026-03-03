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
int score(State *s)
{
    init_score(&s->score[0]);
    init_score(&s->score[1]);
    memset(&s->cards_won, 0x00, sizeof(s->cards_won));

    // Separate cards won by each player
    UC p0 = 0, p1 = 0;
    
    for (int i = 0; i < HAND_SIZE; i++) {
        if (s->trick_winner[i] == 0) {
            s->cards_won[0][p0++] = s->hp[0].card[i];
            s->cards_won[0][p0++] = s->hp[1].card[i];
        } else {
            s->cards_won[1][p1++] = s->hp[0].card[i];
            s->cards_won[1][p1++] = s->hp[1].card[i];
        }
    }

    // Score P0's cards
    for (UC i = 0; i < p0; i++) {
        if (s->cards_won[0][i].suit == s->trump) {
            if (s->cards_won[0][i].rank < s->score[0].low)
                s->score[0].low = s->cards_won[0][i].rank;
            if (s->cards_won[0][i].rank > s->score[0].high)
                s->score[0].high = s->cards_won[0][i].rank;
            if (s->cards_won[0][i].rank == 11)
                s->score[0].jack = true;
        }
        s->score[0].game += add_game(s->cards_won[0][i]);
    }

    // Score P1's cards
    for (UC i = 0; i < p1; i++) {
        if (s->cards_won[1][i].suit == s->trump) {
            if (s->cards_won[1][i].rank < s->score[1].low)
                s->score[1].low = s->cards_won[1][i].rank;
            if (s->cards_won[1][i].rank > s->score[1].high)
                s->score[1].high = s->cards_won[1][i].rank;
            if (s->cards_won[1][i].rank == 11)
                s->score[1].jack = true;
        }
        s->score[1].game += add_game(s->cards_won[1][i]);
    }

    // Calculate total scores
    s->t_score[0] = 0;
    s->t_score[1] = 0;
    
    if (s->score[0].low < s->score[1].low) s->t_score[0] += 1;
    else s->t_score[1] += 1;
    
    if (s->score[0].high > s->score[1].high) s->t_score[0] += 1;
    else s->t_score[1] += 1;
    
    if (s->score[0].game > s->score[1].game) s->t_score[0] += 1;
    else if (s->score[1].game > s->score[0].game) s->t_score[1] += 1;
    
    if (s->score[0].jack) s->t_score[0] += 1;
    else if (s->score[1].jack) s->t_score[1] += 1;

    // Check for set (didn't make bid)
    // winning_bid is raw (1,2,3) representing actual points (2,3,4)
    UC bid_pts = s->winning_bid + 1;
    if (s->winning_bidder == 0 && s->t_score[0] < bid_pts)
        s->t_score[0] = bid_pts * -1;
    else if (s->winning_bidder == 1 && s->t_score[1] < bid_pts)
        s->t_score[1] = bid_pts * -1;

    // Return utility for P0
    return s->t_score[0] - s->t_score[1];
}
