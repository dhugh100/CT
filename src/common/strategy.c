// Copyright (c) 2026 Dave Hugh. All rights reserved.
// Licensed under the GPL v3.0 License. See README.md for details.
#include "strategy.h"
#include "abstraction.h"
#include "util.h"
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>

// Load strategy from binary file via mmap — zero-copy, no malloc.
// Supports files larger than available RAM; OS pages in only what's needed.
Strat_255 *load_strategy(const char *filename, long *count)
{
    int fd = open(filename, O_RDONLY);
    if (fd < 0) {
        fprintf(stderr, "Error: Cannot open strategy file %s\n", filename);
        return NULL;
    }

    struct stat st;
    fstat(fd, &st);
    long file_size = st.st_size;
    long node_count = file_size / sizeof(Strat_255);
    size_t map_size = (size_t)node_count * sizeof(Strat_255);

    printf("Loading strategy from %s\n", filename);
    printf("File size: %ld bytes\n", file_size);
    printf("Node count: %ld\n", node_count);

    Strat_255 *strat = (Strat_255 *)mmap(NULL, map_size, PROT_READ, MAP_SHARED, fd, 0);
    close(fd);
    if (strat == MAP_FAILED) {
        fprintf(stderr, "Error: mmap failed for strategy file %s\n", filename);
        return NULL;
    }

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
int find_node(Strat_255 *strat, long qty, Key *t) {
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
UC get_best_action(Strat_255 *strat, long count, State *s)
{
    // Build key from current state
    Key k = build_key(s);

    // Find node
    int idx = find_node(strat, count, &k);
    if (idx < 0) {
        return 0xff; // Invalid action marker
    }

    // Find action with highest probability — dequantize only for this node
    float best_prob = -1.0f;
    UC best_action = 0xff;

    for (int i = 0; i < strat[idx].action_count; i++) {
        if ((s->stage == 1 && is_valid_play_action(s, strat[idx].action[i])) ||
            (s->stage == 0 && is_valid_bid_action(s, strat[idx].action[i]))) {
            float prob = strat[idx].s255[i] / 255.0f;
            if (prob > best_prob) {
                best_prob = prob;
                best_action = strat[idx].action[i];
            }
        }
    }

    return best_action;
}

// Unmap strategy — count must match the value returned by load_strategy
void free_strategy(Strat_255 *strat, long count)
{
    munmap(strat, (size_t)count * sizeof(Strat_255));
}
