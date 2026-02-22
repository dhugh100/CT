// Copyright (c) 2026 Dave Hugh. All rights reserved.
// Licensed under the GPL v3.0 License. See README.md for details.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "types.h"
#include "strategy.h"
#include "eval.h"

int main(int argc, char *argv[])
{
    if (argc != 5) {
        fprintf(stderr, "Usage: %s <strategy_file> <iterations> <mode>\n", argv[0]);
        fprintf(stderr, "  mode: 0=policy (machine vs random), 1=random (both random)\n");
        fprintf(stderr, "  seed: 0 for random, up to MAX_RAND for seed\n");
        return 1;
    }
    
    const char *strategy_file = argv[1];
    int iterations = atoi(argv[2]);
    int mode = atoi(argv[3]);
    int seed = atoi(argv[4]);
    if (seed == 0)  seed = (unsigned int)time(NULL);
    
    printf("=== CT-PLAYA Evaluation ===\n");
    printf("Strategy file: %s\n", strategy_file);
    printf("Iterations: %d\n", iterations);
    printf("Mode: %s\n", mode == 0 ? "POLICY" : "RANDOM");
    printf("Seed: %u\n\n", seed);
    
    // Load strategy
    long strat_count = 0;
    Strat *strat = load_strategy(strategy_file, &strat_count);
    if (!strat) {
        return 1;
    }
    
    printf("\n");
    
    // Run evaluation
    EvalStats stats;
    eval_games(strat, strat_count, iterations, seed, 
               (mode == 0) ? MODE_POLICY : MODE_RANDOM, &stats);
    
    // Print results
    print_eval_stats(&stats);
    
    // Cleanup
    free_strategy(strat);
    
    return 0;
}
