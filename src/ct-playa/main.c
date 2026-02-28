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
    if (argc < 5 || argc > 6) {
        fprintf(stderr, "Usage: %s <strategy_file> <iterations> <mode> <seed> [output_csv]\n", argv[0]);
        fprintf(stderr, "  mode: 0=policy (P0 strategy vs random)\n");
        fprintf(stderr, "        1=random (both random)\n");
        fprintf(stderr, "        2=self-play (strategy vs strategy, requires output_csv)\n");
        fprintf(stderr, "  seed: 0 for random\n");
        fprintf(stderr, "  output_csv: dataset file path (required for mode 2)\n");
        return 1;
    }

    const char *strategy_file = argv[1];
    int iterations = atoi(argv[2]);
    int mode = atoi(argv[3]);
    int seed = atoi(argv[4]);
    const char *output_csv = (argc == 6) ? argv[5] : NULL;

    if (seed == 0) seed = (unsigned int)time(NULL);

    if (mode == 2 && !output_csv) {
        fprintf(stderr, "Error: mode 2 (self-play) requires an output_csv argument\n");
        return 1;
    }

    const char *mode_name = (mode == 0) ? "POLICY" : (mode == 1) ? "RANDOM" : "SELF-PLAY";
    printf("=== CT-PLAYA Evaluation ===\n");
    printf("Strategy file: %s\n", strategy_file);
    printf("Iterations: %d\n", iterations);
    printf("Mode: %s\n", mode_name);
    printf("Seed: %u\n", seed);
    if (output_csv) printf("Dataset output: %s\n", output_csv);
    printf("\n");

    // Load strategy
    long strat_count = 0;
    Strat *strat = load_strategy(strategy_file, &strat_count);
    if (!strat) {
        return 1;
    }

    printf("\n");

    // Run evaluation
    EvalStats stats;

    if (mode == 2) {
        FILE *fp = fopen(output_csv, "w");
        if (!fp) {
            fprintf(stderr, "Error: Cannot open output file %s\n", output_csv);
            free_strategy(strat);
            return 1;
        }
        eval_games_selfplay(strat, strat_count, iterations, seed, &stats, fp);
        fclose(fp);
        printf("Dataset written to %s\n", output_csv);
    } else {
        eval_games(strat, strat_count, iterations, seed,
                   (mode == 0) ? MODE_POLICY : MODE_RANDOM, &stats);
    }

    // Print results
    print_eval_stats(&stats);

    // Cleanup
    free_strategy(strat);

    return 0;
}
