// Copyright (c) 2026 Dave Hugh. All rights reserved.
// Licensed under the GPL v3.0 License. See README.md for details.
#include "eval.h"
#include "game.h"
#include "deck.h"
#include "util.h"
#include "abstraction.h"

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

// ============================================================
// Self-play dataset generation (strategy vs strategy)
// ============================================================

// Per-decision record buffered before payoff is known
typedef struct {
    int player, stage, trick_num, trump, dealer;
    int winning_bidder, winning_bid;
    UC key[15];
    UC action;
    int strategy_hit;
} DecisionRecord;

#define MAX_DECISIONS_PER_HAND 16

static void write_dataset_header(FILE *fp)
{
    fprintf(fp, "game_id,player,stage,trick_num,trump,dealer,"
                "winning_bidder,winning_bid,"
                "k00,k01,k02,k03,k04,k05,k06,k07,k08,k09,k10,k11,k12,k13,k14,"
                "action,strategy_hit,payoff\n");
}

// Play one hand with both players using the strategy, buffering decisions
static int play_hand_selfplay_record(State *s, Strat *strat, long strat_count,
                                     DecisionRecord *buf, EvalStats *stats)
{
    int count = 0;
    while (!s->hand_done) {
        UC actions[MAX_ACTIONS];
        int num_actions;
        UC chosen_action;
        int strategy_hit = 0;

        if (s->stage == BID) {
            num_actions = legal_bid(s, actions);
        } else {
            num_actions = legal_play(s, actions);
        }

        // Build key now so we record pre-action state
        Key k = build_key(s);

        // Both players use strategy
        chosen_action = get_best_action(strat, strat_count, s);
        if (chosen_action != 0xff) {
            strategy_hit = 1;
            if (s->to_act == 0) stats->nodes_found++;
        } else {
            if (s->to_act == 0) stats->nodes_not_found++;
            if (num_actions == 1) chosen_action = actions[0];
            else chosen_action = actions[get_random(0, num_actions - 1, &s->seed)];
        }

        // Buffer the decision record
        DecisionRecord *rec = &buf[count++];
        rec->player        = s->to_act;
        rec->stage         = s->stage;
        rec->trick_num     = s->trick_num;
        rec->trump         = s->trump;
        rec->dealer        = s->dealer;
        rec->winning_bidder = 0; // filled in retroactively
        rec->winning_bid    = 0; // filled in retroactively
        memcpy(rec->key, k.bits, sizeof(k.bits));
        rec->action        = chosen_action;
        rec->strategy_hit  = strategy_hit;

        // Apply action
        if (s->stage == BID) {
            apply_bid(s, chosen_action);
        } else {
            UC card_index = bind_card_index_to_action(s, chosen_action);
            apply_play(s, card_index);
        }
    }

    // Retroactively fill in winning bid info from completed state
    for (int i = 0; i < count; i++) {
        buf[i].winning_bidder = s->winning_bidder;
        buf[i].winning_bid    = s->winning_bid;
    }

    return count;
}

static void flush_decisions_to_csv(FILE *fp, int game_id,
                                   DecisionRecord *buf, int count, int payoff)
{
    for (int i = 0; i < count; i++) {
        DecisionRecord *r = &buf[i];
        fprintf(fp, "%d,%d,%d,%d,%d,%d,%d,%d,",
                game_id, r->player, r->stage, r->trick_num, r->trump,
                r->dealer, r->winning_bidder, r->winning_bid);
        for (int j = 0; j < 15; j++) {
            fprintf(fp, "%d,", r->key[j]);
        }
        fprintf(fp, "%d,%d,%d\n", r->action, r->strategy_hit, payoff);
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

// Strategy vs strategy: play N games, record every decision to CSV
void eval_games_selfplay(Strat *strat, long strat_count, int iterations,
                         unsigned int seed, EvalStats *stats, FILE *dataset_fp)
{
    init_eval_stats(stats);
    if (dataset_fp) write_dataset_header(dataset_fp);

    DecisionRecord buf[MAX_DECISIONS_PER_HAND];

    for (int i = 0; i < iterations; i++) {
        State s = {0};
        s.seed = seed + i;
        s.dealer = get_random(0, 1, &s.seed);
        s.stage = BID;
        s.to_act = 1 - s.dealer;

        make_cards_and_deal(&s);

        int ndecisions = play_hand_selfplay_record(&s, strat, strat_count, buf, stats);

        int payoff = score(&s);

        stats->games_played++;
        if (payoff > 0)      stats->games_won[0]++;
        else if (payoff < 0) stats->games_won[1]++;
        stats->hands_won[0] += (s.t_score[0] > 0) ? 1 : 0;
        stats->hands_won[1] += (s.t_score[1] > 0) ? 1 : 0;
        stats->tricks_won[0] += s.tricks_won[0];
        stats->tricks_won[1] += s.tricks_won[1];

        if (dataset_fp) {
            flush_decisions_to_csv(dataset_fp, i, buf, ndecisions, payoff);
        }
    }
}
