// Copyright (c) 2026 Dave Hugh. All rights reserved.
// Licensed under the GPL v3.0 License. See README.md for details.
#include "eval.h"
#include "game.h"
#include "deck.h"
#include "util.h"
#include "abstraction.h"
#include <stddef.h>

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
void play_hand_policy(State *s, Strat_255 *strat, long strat_count, EvalStats *stats)
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
    UC key[12];
    UC action;
    int strategy_hit;
} DecisionRecord;

#define MAX_DECISIONS_PER_HAND 16

static void write_dataset_header(FILE *fp)
{
    fprintf(fp, "game_id,player,stage,trick_num,trump,dealer,"
                "winning_bidder,winning_bid,"
                "k00,k01,k02,k03,k04,k05,k06,k07,k08,k09,k10,k11,"
                "action,strategy_hit,payoff\n");
}

// Play one hand with both players using the strategy, buffering decisions
static int play_hand_selfplay_record(State *s, Strat_255 *strat, long strat_count,
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
        for (int j = 0; j < 12; j++) {
            fprintf(fp, "%d,", r->key[j]);
        }
        fprintf(fp, "%d,%d,%d\n", r->action, r->strategy_hit, payoff);
    }
}

// ============================================================
// Bid NN dataset generation
// ============================================================

#define MAX_BID_DECISIONS_PER_HAND 2
#define BID_DATA_MAGIC 0x42494442u  // 'B','I','D','B'

// File header (12 bytes); count is filled in by finalize_bid_data_header()
typedef struct {
    uint32_t magic;        // BID_DATA_MAGIC
    uint32_t record_size;  // sizeof(BidRecord) — reader can validate layout
    uint32_t count;        // total BidRecords in file
} BidDataHeader;

// One record per bid decision where CFR strategy was found.
// Layout: 1+1+7+3+16 = 28 bytes. The 3-byte pad makes float alignment explicit
// so sizeof(BidRecord) == 28 with no hidden compiler padding.
// hand_packed: 52 bits, one per card slot (suit*13+(rank-2)), LSB-first within each byte.
typedef struct {
    uint8_t am_i_dealer;      // 1 if current bidder is dealer, 0 otherwise
    uint8_t opp_bid;          // opponent's raw bid 0-3; only meaningful when am_i_dealer=1
    uint8_t hand_packed[7];   // 52 bits bitpacked; bit i = card at suit*13+(rank-2)
    uint8_t _pad[3];          // explicit alignment padding before float
    float   strategy[4];      // CFR average strategy for bid actions 0,1,2,3
} BidRecord;

static void write_bid_data_header(FILE *fp)
{
    BidDataHeader h = { BID_DATA_MAGIC, sizeof(BidRecord), 0 };
    fwrite(&h, sizeof(BidDataHeader), 1, fp);
}

// Seek back to fill in the final record count
static void finalize_bid_data_header(FILE *fp, uint32_t count)
{
    fseek(fp, offsetof(BidDataHeader, count), SEEK_SET);
    fwrite(&count, sizeof(uint32_t), 1, fp);
    fseek(fp, 0, SEEK_END);
}

// Play one hand recording only bid decisions that have a strategy hit.
// Returns the number of bid records written into buf.
static int play_hand_selfplay_bid_record(State *s, Strat_255 *strat, long strat_count,
                                         BidRecord *buf, EvalStats *stats)
{
    int count = 0;
    while (!s->hand_done) {
        UC actions[MAX_ACTIONS];
        int num_actions;
        UC chosen_action;

        if (s->stage == BID)
            num_actions = legal_bid(s, actions);
        else
            num_actions = legal_play(s, actions);

        Key k = build_key(s);
        int sidx = find_node(strat, strat_count, &k);
        int strategy_hit = 0;

        if (sidx >= 0) {
            // Select best action (argmax over legal actions in strategy)
            float best_prob = -1.0f;
            chosen_action = 0xff;
            for (int i = 0; i < strat[sidx].action_count; i++) {
                UC a = strat[sidx].action[i];
                for (int j = 0; j < num_actions; j++) {
                    float prob = strat[sidx].s255[i] / 255.0f;
                    if (actions[j] == a && prob > best_prob) {
                        best_prob = prob;
                        chosen_action = a;
                        break;
                    }
                }
            }
            if (chosen_action != 0xff)
                strategy_hit = 1;
        }

        if (!strategy_hit) {
            if (s->to_act == 0) stats->nodes_not_found++;
            chosen_action = (num_actions == 1)
                ? actions[0]
                : actions[get_random(0, num_actions - 1, &s->seed)];
        } else {
            if (s->to_act == 0) stats->nodes_found++;
        }

        // Record bid decisions with strategy hits only
        if (s->stage == BID && strategy_hit && count < MAX_BID_DECISIONS_PER_HAND) {
            BidRecord *rec = &buf[count++];
            rec->am_i_dealer = (s->dealer == s->to_act) ? 1 : 0;
            rec->opp_bid     = s->bid[1 - s->to_act];

            memset(rec->hand_packed, 0, sizeof(rec->hand_packed));
            for (int i = 0; i < HAND_SIZE; i++) {
                Card c = s->hand[s->to_act].card[i];
                if (c.rank == 0) break;
                int bit = c.suit * 13 + (c.rank - 2);
                rec->hand_packed[bit / 8] |= (uint8_t)(1u << (bit % 8));
            }

            memset(rec->strategy, 0, sizeof(rec->strategy));
            for (int i = 0; i < strat[sidx].action_count; i++) {
                UC a = strat[sidx].action[i];
                if (a < 4) rec->strategy[a] = strat[sidx].s255[i] / 255.0f;
            }
        }

        if (s->stage == BID)
            apply_bid(s, chosen_action);
        else {
            UC card_index = bind_card_index_to_action(s, chosen_action);
            apply_play(s, card_index);
        }
    }
    return count;
}

static void flush_bid_records_to_bin(FILE *fp, BidRecord *buf, int count)
{
    fwrite(buf, sizeof(BidRecord), count, fp);
}

// Run evaluation
void eval_games(Strat_255 *strat, long strat_count, int iterations, unsigned int seed, EvalMode mode, EvalStats *stats)
{
    init_eval_stats(stats);
    
    for (int i = 0; i < iterations; i++) {
        State s = {0};
        s.seed = seed + i;
        s.dealer = get_random(0, 1, &s.seed);
        s.stage = BID;
        s.to_act = 1 - s.dealer;
        s.trump = PRE_TRUMP;
        
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

// Strategy vs strategy: play N games, record decisions to file.
// dataset_mode: 0 = bid NN — binary BidRecord per bid decision, strategy_hit=1 only
//               1 = play NN — CSV of all decisions with payoff [future]
void eval_games_selfplay(Strat_255 *strat, long strat_count, int iterations,
                         unsigned int seed, EvalStats *stats, FILE *dataset_fp,
                         int dataset_mode)
{
    init_eval_stats(stats);

    if (dataset_fp) {
        if (dataset_mode == 0) write_bid_data_header(dataset_fp);
        else                   write_dataset_header(dataset_fp);
    }

    BidRecord      bid_buf[MAX_BID_DECISIONS_PER_HAND];
    DecisionRecord dec_buf[MAX_DECISIONS_PER_HAND];
    uint32_t total_bid_records = 0;

    for (int i = 0; i < iterations; i++) {
        State s = {0};
        s.seed = seed + i;
        s.dealer = get_random(0, 1, &s.seed);
        s.stage = BID;
        s.to_act = 1 - s.dealer;
        s.trump = PRE_TRUMP;

        make_cards_and_deal(&s);

        int ndecisions;
        if (dataset_mode == 0)
            ndecisions = play_hand_selfplay_bid_record(&s, strat, strat_count, bid_buf, stats);
        else
            ndecisions = play_hand_selfplay_record(&s, strat, strat_count, dec_buf, stats);

        int payoff = score(&s);

        stats->games_played++;
        if (payoff > 0)      stats->games_won[0]++;
        else if (payoff < 0) stats->games_won[1]++;
        stats->hands_won[0] += (s.t_score[0] > 0) ? 1 : 0;
        stats->hands_won[1] += (s.t_score[1] > 0) ? 1 : 0;
        stats->tricks_won[0] += s.tricks_won[0];
        stats->tricks_won[1] += s.tricks_won[1];

        if (dataset_fp) {
            if (dataset_mode == 0) {
                flush_bid_records_to_bin(dataset_fp, bid_buf, ndecisions);
                total_bid_records += ndecisions;
            } else {
                flush_decisions_to_csv(dataset_fp, i, dec_buf, ndecisions, payoff);
            }
        }
    }

    if (dataset_fp && dataset_mode == 0)
        finalize_bid_data_header(dataset_fp, total_bid_records);
}
