// Copyright (c) 2026 Dave Hugh. All rights reserved.
// Licensed under the GPL v3.0 License. See README.md for details.
#include "eval.h"
#include "game.h"
#include "deck.h"
#include "util.h"

// Initialize evaluation statistics
void init_eval_stats(EvalStats *stats)
{
    memset(stats, 0, sizeof(EvalStats));
}

// Print evaluation statistics
void print_eval_stats(EvalStats *stats)
{
    printf("\n=== Evaluation Results ===\n");
    printf("Games played: %d\n", stats->games_played);
    printf("Games won by P0 (machine): %d (%.2f%%)\n", 
        stats->games_won[0], 
        100.0 * stats->games_won[0] / stats->games_played);
    printf("Games won by P1 (opponent): %d (%.2f%%)\n", 
        stats->games_won[1], 
        100.0 * stats->games_won[1] / stats->games_played);
    printf("Hands won by P0: %d\n", stats->hands_won[0]);
    printf("Hands won by P1: %d\n", stats->hands_won[1]);
    printf("Tricks won by P0: %d\n", stats->tricks_won[0]);
    printf("Tricks won by P1: %d\n", stats->tricks_won[1]);
    
    if (stats->nodes_found + stats->nodes_not_found > 0) {
        printf("\nStrategy coverage:\n");
        printf("Nodes found: %d (%.2f%%)\n", 
            stats->nodes_found,
            100.0 * stats->nodes_found / (stats->nodes_found + stats->nodes_not_found));
        printf("Nodes not found: %d (%.2f%%)\n", 
            stats->nodes_not_found,
            100.0 * stats->nodes_not_found / (stats->nodes_found + stats->nodes_not_found));
    }
}

// Play one hand using strategy for P0
void play_hand_policy(State *s, Strat *strat, long strat_count, EvalStats *stats)
{
    while (!s->hand_done) {
        UC actions[MAX_ACTIONS];
        int num_actions;
        UC chosen_action;
        
       // Get legal actions
       if (s->stage == BID) {
           num_actions = legal_bid(s, actions);
       } else {
           num_actions = legal_play(s, actions);
       }

        // P0 uses strategy, P1 plays randomly
        if (s->to_act == 0) {
            chosen_action = get_best_action(strat, strat_count, s);

            if (chosen_action != 0xff) {
                stats->nodes_found++;
            } else {
                stats->nodes_not_found++;
                // Fall back to random
                if (num_actions == 1) chosen_action = actions[0];
                else chosen_action = actions[get_random(0, num_actions - 1, &s->seed)];
            }
        } else {
            if (num_actions == 1) chosen_action = actions[0];
            else chosen_action = actions[get_random(0, num_actions - 1, &s->seed)];
        }
        
        // Apply action
        if (s->stage == BID) {
            apply_bid(s, chosen_action);
        } else {
            UC card_index = bind_card_index_to_action(s, chosen_action); 
            apply_play(s, card_index);
        }
    }
}

// Play one hand with both players random
void play_hand_random(State *s)
{
    while (!s->hand_done) {
        UC actions[MAX_ACTIONS];
        int num_actions;
        UC chosen_action;
        
        // Get legal actions
        if (s->stage == BID) {
            num_actions = legal_bid(s, actions);
        } else {
            num_actions = legal_play(s, actions);
        }
        
        // Choose random action
        if (num_actions == 1) chosen_action = actions[0];
        else chosen_action = actions[get_random(0, num_actions - 1, &s->seed)];
        
        // Apply action
        if (s->stage == BID) {
            apply_bid(s, chosen_action);
        } else {
            UC card_index = bind_card_index_to_action(s, chosen_action); 
            apply_play(s, card_index);
        }
    }
}

// Run evaluation
void eval_games(Strat *strat, long strat_count, int iterations, unsigned int seed, EvalMode mode, EvalStats *stats)
{
    init_eval_stats(stats);
    
    for (int i = 0; i < iterations; i++) {
        State s = {0};
        s.seed = seed + i;
        s.dealer = get_random(0, 1, &s.seed);
        s.stage = BID;
        s.to_act = 1 - s.dealer;
        
        make_cards_and_deal(&s);
        
        // Play hand
        if (mode == MODE_POLICY) {
            play_hand_policy(&s, strat, strat_count, stats);
        } else {
            play_hand_random(&s);
        }
        
        // Score
        int payoff = score(&s);
        
        stats->games_played++;
        if (payoff > 0) {
            stats->games_won[0]++;
        } else if (payoff < 0) {
            stats->games_won[1]++;
        }
        
        stats->hands_won[0] += (s.t_score[0] > 0) ? 1 : 0;
        stats->hands_won[1] += (s.t_score[1] > 0) ? 1 : 0;
        stats->tricks_won[0] += s.tricks_won[0];
        stats->tricks_won[1] += s.tricks_won[1];
    }
}
