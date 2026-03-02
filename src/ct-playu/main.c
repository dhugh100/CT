// Copyright (c) 2026 Dave Hugh. All rights reserved.
// Licensed under the GPL v3.0 License. See README.md for details.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "types.h"
#include "strategy.h"
#include "play.h"

int main(int argc, char *argv[])
{
    if (argc != 5) {
        fprintf(stderr, "Usage: %s <strategy_file> <winning score> <dealer> <seed>\n", argv[0]);
        fprintf(stderr, "  strategy file: relative strategy file name\n");
        fprintf(stderr, "  score at which the game is won\n");
        fprintf(stderr, "  dealer: 0 for machine, 1 for you\n");
        fprintf(stderr, "  seed: 0 for random, up to MAX_RAND for seed\n");
        return 1;
    }

    // Capture command line arguments
    const char *strategy_file = argv[1];
    UC winning_score = atoi(argv[2]);
    UC dealer = atoi(argv[3]);
    unsigned int seed = atoi(argv[4]);
    if (seed == 0)  seed = (unsigned int)time(NULL);
    
    printf("=== CT-PLAYU ===\n");
    printf("Strategy file: %s\n", strategy_file);
    printf("Winning Score: %u\n", winning_score);
    printf("Dealer: %u\n", dealer);
    printf("Seed: %u\n\n", seed);
    
    // Load strategy
    long strat_count = 0;
    Strat *strat = load_strategy(strategy_file, &strat_count);
    if (!strat) {
        return 1;
    }
    
    printf("\n");
   
    play_game(strat, strat_count, winning_score, dealer, seed);

    // Cleanup
    free_strategy(strat);
    return 0;
}
