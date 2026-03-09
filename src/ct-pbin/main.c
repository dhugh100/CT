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
    if (argc != 4) {
        fprintf(stderr, "Usage: %s <strategy_file> <format: S|Q> <print nodes: Y|N>\n", argv[0]);
        fprintf(stderr, "  S = Strat (float, training output)  Q = Strat_255 (quantized, kwayp output)\n");
        return 1;
    }

    const char *filename = argv[1];

    char fmt = argv[2][0];
    if (fmt != 'S' && fmt != 's' && fmt != 'Q' && fmt != 'q') {
        fprintf(stderr, "Error: Invalid format '%c'. Use S (Strat) or Q (Strat_255).\n", fmt);
        return 1;
    }
    int quantized = (fmt == 'Q' || fmt == 'q');

    char print_nodes = argv[3][0];
    if (print_nodes != 'Y' && print_nodes != 'y' && print_nodes != 'N' && print_nodes != 'n') {
        fprintf(stderr, "Error: Invalid print option '%c'. Use Y/y or N/n.\n", print_nodes);
        return 1;
    }

    printf("=== CT-PBIN Strategy Validator ===\n");
    printf("File:   %s\n", filename);
    printf("Format: %s\n\n", quantized ? "Strat_255 (quantized)" : "Strat (float)");

    struct stat st;
    if (stat(filename, &st) != 0) {
        fprintf(stderr, "Error: Cannot stat file %s\n", filename);
        return 1;
    }

    long file_size = st.st_size;
    size_t record_size = quantized ? sizeof(Strat_255) : sizeof(Strat);
    long expected_nodes = file_size / (long)record_size;
    long remainder = file_size % (long)record_size;

    printf("File size:    %ld bytes\n", file_size);
    printf("Record size:  %zu bytes (%s)\n", record_size,
           quantized ? "Strat_255" : "Strat");
    printf("Node count:   %ld\n", expected_nodes);

    if (remainder != 0) {
        printf("WARNING: File size not aligned to record size (remainder: %ld bytes)\n", remainder);
        return 1;
    }

    FILE *fp = fopen(filename, "rb");
    if (!fp) {
        fprintf(stderr, "Error: Cannot open file %s\n", filename);
        return 1;
    }

    long total_nodes = 0;
    long action_counts[MAX_ACTIONS + 1] = {0};
    int do_print = (print_nodes == 'Y' || print_nodes == 'y');

    if (quantized) {
        Strat_255 s;
        while (fread(&s, sizeof(Strat_255), 1, fp) == 1) {
            total_nodes++;

            if (s.action_count > MAX_ACTIONS) {
                fprintf(stderr, "Error: Invalid action count %d at node %ld\n",
                        s.action_count, total_nodes);
                fclose(fp);
                return 1;
            }
            action_counts[s.action_count]++;

            float sum = 0.0f;
            for (int i = 0; i < s.action_count; i++)
                sum += s.s255[i] / 255.0f;
            if (sum < 0.99f || sum > 1.01f)
                fprintf(stderr, "Warning: Strategy probs sum to %.4f at node %ld\n",
                        sum, total_nodes);

            if (do_print) {
                printf("Strategy Node %ld\n", total_nodes);
                print_strategy_255(&s);
            }
        }
    } else {
        Strat s;
        while (fread(&s, sizeof(Strat), 1, fp) == 1) {
            total_nodes++;

            if (s.action_count > MAX_ACTIONS) {
                fprintf(stderr, "Error: Invalid action count %d at node %ld\n",
                        s.action_count, total_nodes);
                fclose(fp);
                return 1;
            }
            action_counts[s.action_count]++;

            float sum = 0.0f;
            for (int i = 0; i < s.action_count; i++)
                sum += s.strategy[i];
            if (sum < 0.99f || sum > 1.01f)
                fprintf(stderr, "Warning: Strategy probs sum to %.4f at node %ld\n",
                        sum, total_nodes);

            if (do_print) {
                printf("Strategy Node %ld\n", total_nodes);
                print_strategy(&s);
            }
        }
    }

    fclose(fp);

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
