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
        fprintf(stderr, "Usage: %s <strategy_file> <iterations> <mode> <seed> [output_file]\n", argv[0]);
        fprintf(stderr, "  mode: 0=policy (P0 strategy vs random)\n");
        fprintf(stderr, "        1=random (both random)\n");
        fprintf(stderr, "        2=self-play eval (strategy vs strategy, stats only)\n");
        fprintf(stderr, "        3=bid dataset (strategy vs strategy, writes bid NN records)\n");
        fprintf(stderr, "        4=play dataset (strategy vs strategy, writes play NN records)\n");
        fprintf(stderr, "  output_file: required for modes 3 and 4\n");
        fprintf(stderr, "  seed: 0 for random\n");
        return 1;
    }

    const char *strategy_file = argv[1];
    int iterations = atoi(argv[2]);
    int mode = atoi(argv[3]);
    int seed = atoi(argv[4]);
    const char *output_file = (argc >= 6) ? argv[5] : NULL;

    if (seed == 0) seed = (unsigned int)time(NULL);

    if ((mode == 3 || mode == 4) && !output_file) {
        fprintf(stderr, "Error: mode %d requires an output_file argument\n", mode);
        return 1;
    }

    const char *mode_names[] = { "POLICY", "RANDOM", "SELF-PLAY EVAL", "BID DATASET", "PLAY DATASET" };
    const char *mode_name = (mode >= 0 && mode <= 4) ? mode_names[mode] : "UNKNOWN";
    printf("=== CT-PLAYA Evaluation ===\n");
    printf("Strategy file: %s\n", strategy_file);
    printf("Iterations: %d\n", iterations);
    printf("Mode: %s\n", mode_name);
    printf("Seed: %u\n", seed);
    if (output_file) printf("Output: %s\n", output_file);
    printf("\n");

    // Load strategy
    long strat_count = 0;
    Strat_255 *strat = load_strategy(strategy_file, &strat_count);
    if (!strat) {
        return 1;
    }

    printf("\n");

    // Run evaluation
    EvalStats stats;

    if (mode == 0 || mode == 1) {
        eval_games(strat, strat_count, iterations, seed,
                   (mode == 0) ? MODE_POLICY : MODE_RANDOM, &stats);
    } else if (mode == 2) {
        eval_games_selfplay(strat, strat_count, iterations, seed, &stats, NULL, 0);
    } else if (mode == 3 || mode == 4) {
        FILE *fp = fopen(output_file, "wb");
        if (!fp) {
            fprintf(stderr, "Error: Cannot open output file %s\n", output_file);
            free_strategy(strat, strat_count);
            return 1;
        }
        eval_games_selfplay(strat, strat_count, iterations, seed, &stats, fp, mode - 3);
        fclose(fp);
        printf("Dataset written to %s\n", output_file);
    } else {
        fprintf(stderr, "Error: unknown mode %d\n", mode);
        free_strategy(strat, strat_count);
        return 1;
    }

    // Print results
    print_eval_stats(&stats);

    // Cleanup
    free_strategy(strat, strat_count);

    return 0;
}
