// Copyright (c) 2026 Dave Hugh. All rights reserved.
// Licensed under the GPL v3.0 License. See README.md for details.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include "types.h"
#include "util.h"

// Validate and print info about a strategy binary file
int main(int argc, char *argv[])
{
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <strategy_file> <print strategy nodes Yy/Nn\n", argv[0]);
        fprintf(stderr, "  Validates and displays information about a strategy file\n");
        return 1;
    }
    
    const char *filename = argv[1];
    
    printf("=== CT-PBIN Strategy Validator ===\n");
    printf("File: %s\n\n", filename);
    
    // Check file exists and get size
    struct stat st;
    if (stat(filename, &st) != 0) {
        fprintf(stderr, "Error: Cannot stat file %s\n", filename);
        return 1;
    }

    // Check print request specified
    char print_nodes = argv[2][0];
    if (print_nodes != 'Y' && print_nodes != 'y' && print_nodes != 'N' && print_nodes != 'n') {
        fprintf(stderr, "Error: Invalid print strategy nodes option '%c'. Use Y/y or N/n.\n", print_nodes);
        return 1;
    }
    
    long file_size = st.st_size;
    long expected_nodes = file_size / sizeof(Strat);
    long remainder = file_size % sizeof(Strat);
    
    printf("File size: %ld bytes\n", file_size);
    printf("Expected Strat size: %zu bytes\n", sizeof(Strat));
    printf("Node count: %ld\n", expected_nodes);
    
    if (remainder != 0) {
        printf("WARNING: File size not aligned to Strat size (remainder: %ld bytes)\n", remainder);
        return 1;
    }
    
    // Open and read file
    FILE *fp = fopen(filename, "rb");
    if (!fp) {
        fprintf(stderr, "Error: Cannot open file %s\n", filename);
        return 1;
    }
    
    // Statistics
    long total_nodes = 0;
    long action_counts[MAX_ACTIONS + 1] = {0};
    
    // Read and validate each node
    Strat s;
    while (fread(&s, sizeof(Strat), 1, fp) == 1) {
        total_nodes++;
        
        // Validate action count
        if (s.action_count > MAX_ACTIONS) {
            fprintf(stderr, "Error: Invalid action count %d at node %ld\n", 
                s.action_count, total_nodes);
            fclose(fp);
            return 1;
        }
        
        action_counts[s.action_count]++;
        
        // Validate strategy probabilities sum to ~1.0
        float sum = 0.0f;
        for (int i = 0; i < s.action_count; i++) {
            sum += s.strategy[i];
        }
        
        if (sum < 0.99f || sum > 1.01f) {
            fprintf(stderr, "Warning: Strategy probabilities sum to %.4f at node %ld\n", 
                sum, total_nodes);
        }
        if (print_nodes == 'Y' || print_nodes == 'y') {
            printf("Strategy Node %ld\n", total_nodes);
            print_strategy(&s);
        }
    }
    
    fclose(fp);
    
    // Print statistics
    printf("\n=== Validation Results ===\n");
    printf("Total nodes validated: %ld\n", total_nodes);
    printf("\nAction count distribution:\n");
    for (int i = 0; i <= MAX_ACTIONS; i++) {
        if (action_counts[i] > 0) {
            printf("  %d actions: %ld nodes (%.2f%%)\n", 
                i, action_counts[i], 
                100.0 * action_counts[i] / total_nodes);
        }
    }
    
    printf("\n✅ File is valid!\n");
    
    return 0;
}
