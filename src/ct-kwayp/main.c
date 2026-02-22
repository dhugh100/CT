// Copyright (c) 2026 Dave Hugh. All rights reserved.
// Licensed under the GPL v3.0 License. See README.md for details.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "types.h"
#include "merge.h"

int main(int argc, char *argv[])
{
    if (argc < 4) {
        fprintf(stderr, "Usage: %s <output_file> <min_visits> <input_file1> [input_file2] ...\n", argv[0]);
        fprintf(stderr, "  Merges multiple strategy files into one\n");
        fprintf(stderr, "  min_visits: currently unused (for future pruning by visit count)\n");
        return 1;
    }
    
    MergeConfig config;
    config.output_file = argv[1];
    config.min_visits = atoi(argv[2]);
    config.num_files = argc - 3;
    config.input_files = &argv[3];
    
    printf("=== CT-KWAYP K-Way Merge ===\n");
    printf("Output file: %s\n", config.output_file);
    printf("Min visits: %d\n", config.min_visits);
    printf("Input files: %d\n", config.num_files);
    for (int i = 0; i < config.num_files; i++) {
        printf("  %d: %s\n", i + 1, config.input_files[i]);
    }
    printf("\n");
    
    time_t start = time(NULL);
    
    // Merge strategies
    MergeStats stats;
    if (merge_strategies(&config, &stats) != 0) {
        fprintf(stderr, "Error: Merge failed\n");
        return 1;
    }
    
    time_t end = time(NULL);
    
    // Print results
    print_merge_stats(&stats);
    printf("\nMerge completed in %ld seconds\n", end - start);
    
    return 0;
}
