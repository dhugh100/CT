// Copyright (c) 2026 Dave Hugh. All rights reserved.
// Licensed under the GPL v3.0 License. See README.md for details.
#include "strategy.h"
#include "abstraction.h"
#include "util.h"
#include <sys/stat.h>

// Load strategy from binary file (Strat_255 on disk, dequantize to Strat in memory)
Strat *load_strategy(const char *filename, long *count)
{
    FILE *fp = fopen(filename, "rb");
    if (!fp) {
        fprintf(stderr, "Error: Cannot open strategy file %s\n", filename);
        return NULL;
    }

    struct stat st;
    stat(filename, &st);
    long file_size = st.st_size;
    long node_count = file_size / sizeof(Strat_255);

    printf("Loading strategy from %s\n", filename);
    printf("File size: %ld bytes\n", file_size);
    printf("Node count: %ld\n", node_count);

    // Read quantized records
    Strat_255 *raw = (Strat_255 *)malloc(node_count * sizeof(Strat_255));
    if (!raw) {
        fprintf(stderr, "Error: Cannot allocate memory for raw strategy\n");
        fclose(fp);
        return NULL;
    }

    size_t nread = fread(raw, sizeof(Strat_255), node_count, fp);
    fclose(fp);
    if ((long)nread != node_count) {
        fprintf(stderr, "Error: Read %zu nodes, expected %ld\n", nread, node_count);
        free(raw);
        return NULL;
    }

    // Dequantize into Strat array
    Strat *strat = (Strat *)malloc(node_count * sizeof(Strat));
    if (!strat) {
        fprintf(stderr, "Error: Cannot allocate memory for strategy\n");
        free(raw);
        return NULL;
    }

    for (long i = 0; i < node_count; i++) {
        memcpy(strat[i].bits,   raw[i].bits,   sizeof(Key));
        strat[i].action_count = raw[i].action_count;
        memcpy(strat[i].action, raw[i].action, raw[i].action_count);
        for (int j = 0; j < raw[i].action_count; j++)
            strat[i].strategy[j] = raw[i].s255[j] / 255.0f;
    }

    free(raw);
    *count = node_count;
    return strat;
}
/*
// Binary search for a node by key
// Returns index if found, -1 if not found
int find_node(Strat *strat, long count, Key *key)
{
    // Linear search for now (could sort and use binary search for optimization)
    for (long i = 0; i < count; i++) {
        if (memcmp(&strat[i].bits, &key->bits, sizeof(Key)) == 0) {
            return i;
        }
    }
    return -1;
}
*/

// Function to perform binary search
int find_node(Strat *strat, long qty, Key *t) {
    int left = 0;
    int right = qty - 1;

    while (left <= right) {
        int mid = left + (right - left) / 2; // Calculate middle index

        // If the target is found at the middle
        if (memcmp(&strat[mid].bits,t,sizeof(Key)) == 0) {
            return mid;
        }
        // If the target is greater than the middle element, search in the right half
        else if (memcmp(&strat[mid].bits,t,sizeof(Key)) < 0) {
            left = mid + 1;
        }
        // If the target is smaller than the middle element, search in the left half
        else {
            right = mid - 1;
        }
    }

    // Target not found
    return -1;
}


bool is_valid_play_action(State *s, UC action)
{
    UC valid_actions[MAX_ACTIONS];
    UC valid_cnt = legal_play(s, valid_actions);
    for (int i = 0; i < valid_cnt; i++) {
        if (valid_actions[i] == action) {
            return true;
        }
    }
    return false;
}

bool is_valid_bid_action(State *s, UC action)
{
    UC valid_actions[MAX_ACTIONS];
    UC valid_cnt = legal_bid(s, valid_actions);
    for (int i = 0; i < valid_cnt; i++) {
        if (valid_actions[i] == action) {
            return true;
        }
    }
    return false;
}

// Get best action from strategy for current state
// Returns action code or 0xff if no valid action found, otherwise returns the action with the highest probability
UC get_best_action(Strat *strat, long count, State *s)
{
    // Build key from current state
    Key k = build_key(s);
    
    // Find node
    int idx = find_node(strat, count, &k);
    if (idx < 0) {
        return 0xff; // Invalid action marker
    }
    
    // Find action with highest probability
    float best_prob = -1.0f;
    UC best_action = 0xff;
    
    for (int i = 0; i < strat[idx].action_count; i++) {
        if ((s->stage == 1 && is_valid_play_action(s, strat[idx].action[i])) ||
            (s->stage == 0 && is_valid_bid_action(s, strat[idx].action[i]))) {
             if (strat[idx].strategy[i] > best_prob) {
                best_prob = strat[idx].strategy[i];
                best_action = strat[idx].action[i];
            }
        }   
    }
    
    return best_action;
}

// Free strategy memory
void free_strategy(Strat *strat)
{
    free(strat);
}
