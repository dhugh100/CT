// Copyright (c) 2026 Dave Hugh. All rights reserved.
// Licensed under the GPL v3.0 License. See README.md for details.
#ifndef MERGE_H
#define MERGE_H

#include "types.h"

// Merge configuration
typedef struct {
    int num_files;
    char **input_files;
    char *output_file;
    int min_visits;  // Minimum visits to keep a node
} MergeConfig;

// Merge statistics
typedef struct {
    long total_nodes_input;
    long total_nodes_output;
    long nodes_pruned;
} MergeStats;

// Merge functions
int merge_strategies(MergeConfig *config, MergeStats *stats);
void print_merge_stats(MergeStats *stats);

#endif // MERGE_H
