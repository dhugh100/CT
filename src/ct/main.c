// Copyright (c) 2026 Dave Hugh. All rights reserved.
// Licensed under the GPL v3.0 License. See README.md for details.
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <pthread.h>
#include "types.h"
#include "cfr.h"
#include "deck.h"
#include "util.h"

// Global configuration
typedef struct {
    int iterations;
    int threads;
    char *output_file;
    unsigned int base_seed;
} Config;

// Thread data
typedef struct {
    int thread_id;
    int iterations_per_thread;
    Node **hash_table;
    unsigned int seed;
} ThreadData;

// Thread function for CFR training
void *train_thread(void *arg)
{
    ThreadData *data = (ThreadData *)arg;
    
    for (int i = 0; i < data->iterations_per_thread; i++) {
        State s = {0};
        s.seed = data->seed + i;
        s.dealer = get_random(0, 1, &s.seed);
        s.stage = BID;
        s.to_act = 1 - s.dealer; // Non-dealer bids first
        
        make_cards_and_deal(&s);
        
        recurse(&s, data->hash_table, 0, data->thread_id);
        recurse(&s, data->hash_table, 1, data->thread_id);
    }
    
    return NULL;
}

// Save strategy to binary file
void save_strategy_file(Node **hash_table, int threads, const char *filename)
{
    FILE *fp = fopen(filename, "wb");
    if (!fp) {
        fprintf(stderr, "Error: Cannot open file %s for writing\n", filename);
        return;
    }
    
    long total_nodes = 0;
    
    // Count and write nodes
    for (int t = 0; t < threads; t++) {
        for (long i = 0; i < NODE_QTY; i++) {
            Node *cur = hash_table[t * NODE_QTY + i];
            while (cur) {
                // Calculate average strategy
                Strat strat = {0};
                memcpy(&strat.bits, &cur->key.bits, 15);
                strat.action_count = cur->action_count;
                memcpy(strat.action, cur->action, MAX_ACTIONS);
                
                float strategy_sum = 0.0f;
                for (int j = 0; j < cur->action_count; j++) {
                    strategy_sum += cur->strategy_sum[j];
                }
                
                for (int j = 0; j < cur->action_count; j++) {
                    if (strategy_sum > 0) {
                        strat.strategy[j] = cur->strategy_sum[j] / strategy_sum;
                    } else {
                        strat.strategy[j] = 1.0f / cur->action_count;
                    }
                }
                
                fwrite(&strat, sizeof(Strat), 1, fp);
                total_nodes++;
                cur = cur->next;
            }
        }
    }
    
    fclose(fp);
    printf("Saved %ld nodes to %s\n", total_nodes, filename);
}

int main(int argc, char *argv[])
{
    if (argc != 5) {
        fprintf(stderr, "Usage: %s <threads> <iterations> <output_file> <seed>\n", argv[0]);
        return 1;
    }
    
    Config config;
    config.threads = atoi(argv[1]);
    config.iterations = atoi(argv[2]);
    config.output_file = argv[3];
    config.base_seed = atoi(argv[4]);
    if (config.base_seed == 0) config.base_seed = (unsigned int)time(NULL);
    
    printf("=== CFR Training ===\n");
    printf("Threads: %d\n", config.threads);
    printf("Iterations: %d\n", config.iterations);
    printf("Output: %s\n", config.output_file);
    printf("Base seed: %u\n", config.base_seed);
    
    // Allocate hash table
    long total_buckets = NODE_QTY * config.threads;
    Node **hash_table = (Node **)calloc(total_buckets, sizeof(Node *));
    if (!hash_table) {
        fprintf(stderr, "Error: Cannot allocate hash table\n");
        return 1;
    }
    
    printf("Hash table allocated: %ld buckets\n", total_buckets);
    
    // Create threads
    pthread_t *threads = malloc(config.threads * sizeof(pthread_t));
    ThreadData *thread_data = malloc(config.threads * sizeof(ThreadData));
    
    int iterations_per_thread = config.iterations / config.threads;
    
    printf("Starting training...\n");
    time_t start_time = time(NULL);
    
    for (int i = 0; i < config.threads; i++) {
        thread_data[i].thread_id = i;
        thread_data[i].iterations_per_thread = iterations_per_thread;
        thread_data[i].hash_table = hash_table;
        thread_data[i].seed = config.base_seed + (i * 10000);
        
        pthread_create(&threads[i], NULL, train_thread, &thread_data[i]);
    }
    
    // Wait for threads
    for (int i = 0; i < config.threads; i++) {
        pthread_join(threads[i], NULL);
    }
    
    time_t end_time = time(NULL);
    printf("Training completed in %ld seconds\n", end_time - start_time);
    
    // Save strategy
    printf("Saving strategy...\n");
    save_strategy_file(hash_table, config.threads, config.output_file);
    
    // Cleanup
    free(threads);
    free(thread_data);
    
    // Free hash table nodes
    for (long i = 0; i < total_buckets; i++) {
        Node *cur = hash_table[i];
        while (cur) {
            Node *next = cur->next;
            free(cur);
            cur = next;
        }
    }
    free(hash_table);
    
    printf("Done!\n");
    return 0;
}
