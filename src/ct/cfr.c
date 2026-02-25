// Copyright (c) 2026 Dave Hugh. All rights reserved.
// Licensed under the GPL v3.0 License. See README.md for details.
#include "cfr.h"
#include "game.h"
#include "abstraction.h"
#include "deck.h"
#include <math.h>

// FNV-1a 32-bit hash
unsigned int hash_key(Key *k)
{
    char *ptr = (char *) k;
    unsigned int h = 2166136261u;
    for (size_t i = 0; i < sizeof(Key); i++) {
        h ^= ptr[i];
        h *= 16777619u;
    }
    return h;
}

// Apply modulus operator to retrieved hash
// - Size is the number of buckets for the thread in the hash table
unsigned int idx_hash(Key *k, int size)
{
    return hash_key(k) % size;
}
// Get or create node in hash table
// - Hash table with chaining (linked lists)
// - Handles collisions where same key has different legal actions
Node *get_or_create(Node **pa, Key *key, UC actions[], UC legal_n, int thread_num)
{
    long idx = idx_hash(key, NODE_QTY) + (NODE_QTY * thread_num);
    Node *cur = pa[idx];

    // Loop through the bucket if a collision
    while (cur) {
        if (memcmp(&cur->key, key, sizeof(Key)) == 0) {
            // Key matches - check if actions also match
            if (cur->action_count != legal_n) {
                // Same key, different number of actions - keep searching
                cur = cur->next;
                continue;
            }
            
            // Check if action arrays match (order may differ)
            bool actions_match = true;
            for (int i = 0; i < legal_n; i++) {
                bool found = false;
                for (int j = 0; j < cur->action_count; j++) {
                    if (cur->action[j] == actions[i]) {
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    actions_match = false;
                    break;
                }
            }
            
            if (actions_match) {
                return cur;  // Exact match found
            } else {
                // Same key, different actions - keep searching
                cur = cur->next;
                continue;
            }
        }
        cur = cur->next;
    }

    // No exact match found - create new node
    Node *node = (Node *) calloc(1, sizeof(Node));
    memcpy(&node->key, key, sizeof(Key));
    node->action_count = legal_n;
    memcpy(node->action, actions, legal_n * sizeof(UC));
    
    // Initialize with uniform strategy
    for (int i = 0; i < legal_n; i++) {
        node->strategy[i] = 1.0f / legal_n;
    }
    
    node->next = pa[idx];
    pa[idx] = node;
    
    return node;
}

// Update strategy using regret matching
void update_strategy(Node *node)
{
    float normalizing_sum = 0.0f;
    
    // Calculate sum of positive regrets
    for (int i = 0; i < node->action_count; i++) {
        if (node->regret_sum[i] > 0) {
            normalizing_sum += node->regret_sum[i];
        }
    }
    
    // Update strategy
    for (int i = 0; i < node->action_count; i++) {
        if (normalizing_sum > 0) {
            node->strategy[i] = (node->regret_sum[i] > 0) ? 
                node->regret_sum[i] / normalizing_sum : 0.0f;
        } else {
            node->strategy[i] = 1.0f / node->action_count;
        }
        
        // Accumulate strategy sum for final average
        node->strategy_sum[i] += node->strategy[i];
    }
    
    node->visits++;
}

// Update regrets after action utilities are calculated
void update_regrets(Node *node, float *action_utilities, float node_utility)
{
    for (int i = 0; i < node->action_count; i++) {
        float regret = action_utilities[i] - node_utility;
        node->regret_sum[i] += regret;
    }
}

// CFR recursion
// - Traverse game tree, calculate utilities, update regrets and strategies
float recurse(State *sp, Node **hash_table, int p, int thread_num)
{
    // Terminal node - return payoff
    if (sp->hand_done) {
        int payoff = score(sp);
        return (p == 0) ? payoff : -payoff;
    }
    
    // Get legal actions
    UC actions[MAX_ACTIONS];
    int num_actions;
    
    if (sp->stage == BID) {
        num_actions = legal_bid(sp, actions);
    } else {
        num_actions = legal_play(sp, actions);
    }
    
    // Build key and get/create node
    Key k = build_key(sp);
    Node *node = get_or_create(hash_table, &k, actions, num_actions, thread_num);
    
    // Update strategy using regret matching
    update_strategy(node);
    
    // Calculate action utilities
    float action_utilities[MAX_ACTIONS] = {0};
    float node_utility = 0.0f;
    
    for (int i = 0; i < num_actions; i++) {
        State next_state = *sp;
        
        // Apply action
        if (sp->stage == BID) {
            apply_bid(&next_state, actions[i]);
        } else {
            UC card_index = bind_card_index_to_action(&next_state, actions[i]);
            apply_play(&next_state, card_index);
        }
        
        // Recurse
        action_utilities[i] = recurse(&next_state, hash_table, p, thread_num);
        
        // Weight by strategy
        node_utility += node->strategy[i] * action_utilities[i];
    }
    
    // Update regrets (only for current player's nodes)
    if (sp->to_act == p) {
        update_regrets(node, action_utilities, node_utility);
    }
    
    return node_utility;
}
