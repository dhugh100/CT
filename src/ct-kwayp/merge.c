// Copyright (c) 2026 Dave Hugh. All rights reserved.
// Licensed under the GPL v3.0 License. See README.md for details.
#include "merge.h"

// One open stream per input file during k-way merge
typedef struct {
    FILE *fp;
    Strat current;   // Current head record from this file
    bool exhausted;  // No more records in this file
} Stream;

// Compare two Strat records by key (for qsort and merge comparison)
// - If key is equal, check action count and then actions
// - Need to include to separate nodes with different actions
static int compare_keys(const void *a, const void *b)
{
    const Strat *sa = (const Strat *)a;
    const Strat *sb = (const Strat *)b;
    int _1st = memcmp(sa->bits, sb->bits, sizeof(Key));
    if (_1st != 0) return _1st;
    if (sa->action_count != sb->action_count)
         return (int)sa->action_count - (int)sb->action_count;
     return memcmp(sa->action, sb->action, sa->action_count);
}

// Load one file into memory, sort it, write it back sorted.
// Only one file is ever in memory at a time.
static int sort_file(const char *filename)
{
    FILE *fp = fopen(filename, "rb");
    if (!fp) {
        fprintf(stderr, "Error: Cannot open %s\n", filename);
        return -1;
    }

    fseek(fp, 0, SEEK_END);
    long fsize = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    long count = fsize / sizeof(Strat);
    if (count == 0) {
        fclose(fp);
        printf("  %s: empty, skipping\n", filename);
        return 0;
    }

    Strat *buf = malloc(count * sizeof(Strat));
    if (!buf) {
        fprintf(stderr, "Error: Cannot allocate %ld nodes for %s\n", count, filename);
        fclose(fp);
        return -1;
    }

    size_t n = fread(buf, sizeof(Strat), count, fp);
    fclose(fp);

    if ((long)n != count) {
        fprintf(stderr, "Error: Read %zu of %ld nodes from %s\n", n, count, filename);
        free(buf);
        return -1;
    }

    qsort(buf, count, sizeof(Strat), compare_keys);

    fp = fopen(filename, "wb");
    if (!fp) {
        fprintf(stderr, "Error: Cannot open %s for writing\n", filename);
        free(buf);
        return -1;
    }

    size_t written = fwrite(buf, sizeof(Strat), count, fp);
    fclose(fp);
    free(buf);

    if ((long)written != count) {
        fprintf(stderr, "Error: Wrote %zu of %ld nodes to %s\n", written, count, filename);
        return -1;
    }

    printf("  %s: sorted %ld nodes\n", filename, count);
    return 0;
}

// Read the next record into a stream's current slot; mark exhausted on EOF
static void advance_stream(Stream *s)
{
    if (s->exhausted) return;
    if (fread(&s->current, sizeof(Strat), 1, s->fp) != 1)
        s->exhausted = true;
}

// Return the index of the stream with the smallest key, or -1 if all exhausted
static int find_min(Stream *streams, int n)
{
    int min = -1;
    for (int i = 0; i < n; i++) {
        if (streams[i].exhausted) continue;
        if (min == -1 ||
            memcmp(streams[i].current.bits, streams[min].current.bits, sizeof(Key)) < 0)
            min = i;
    }
    return min;
}

// Perform k-way merge of pre-sorted streams, averaging duplicate keys on the fly
static int kway_merge(Stream *streams, int n, const char *output_file,
                      long *input_count, long *output_count)
{
    FILE *ofp = fopen(output_file, "wb");
    if (!ofp) {
        fprintf(stderr, "Error: Cannot open output file %s\n", output_file);
        return -1;
    }

    *input_count = 0;
    *output_count = 0;

    // Accumulator for the current key group
    Strat accum;
    float strat_sums[MAX_ACTIONS];
    long dup_count = 0;

    while (true) {
        int idx = find_min(streams, n);
        if (idx == -1) break;   // All streams exhausted

        Strat *s = &streams[idx].current;
        (*input_count)++;

        if (dup_count == 0 || memcmp(s->bits, accum.bits, sizeof(Key)) != 0 ||
            s->action_count != accum.action_count || 
            memcmp(s->action,accum.action,accum.action_count != 0)) {
            // Write completed group (if any) before starting a new one
            if (dup_count > 0) {
                for (int j = 0; j < accum.action_count; j++)
                    accum.strategy[j] = strat_sums[j] / (float)dup_count;
                if (fwrite(&accum, sizeof(Strat), 1, ofp) != 1) {
                    fprintf(stderr, "Error: Write failed on output file\n");
                    fclose(ofp);
                    return -1;
                }
                (*output_count)++;
            }
            // Begin new group
            accum = *s;
            memset(strat_sums, 0, sizeof(strat_sums));
            for (int j = 0; j < accum.action_count; j++)
                strat_sums[j] = s->strategy[j];
            dup_count = 1;
        } else {
            // Same key: accumulate strategy values for averaging
            for (int j = 0; j < accum.action_count; j++)
                strat_sums[j] += s->strategy[j];
            dup_count++;
        }

        advance_stream(&streams[idx]);
    }

    // Flush the final group
    if (dup_count > 0) {
        for (int j = 0; j < accum.action_count; j++)
            accum.strategy[j] = strat_sums[j] / (float)dup_count;
        if (fwrite(&accum, sizeof(Strat), 1, ofp) != 1) {
            fprintf(stderr, "Error: Write failed on final node\n");
            fclose(ofp);
            return -1;
        }
        (*output_count)++;
    }

    fclose(ofp);
    return 0;
}

// Main merge entry point
int merge_strategies(MergeConfig *config, MergeStats *stats)
{
    memset(stats, 0, sizeof(MergeStats));
    int n = config->num_files;

    // Phase 1: sort each input file individually (one file in memory at a time)
    printf("Phase 1: Sorting %d input file(s)...\n", n);
    for (int i = 0; i < n; i++) {
        if (sort_file(config->input_files[i]) != 0)
            return -1;
    }

    // Phase 2: open all sorted files and k-way merge into output
    printf("Phase 2: K-way merge...\n");

    Stream *streams = malloc(n * sizeof(Stream));
    if (!streams) {
        fprintf(stderr, "Error: Cannot allocate stream array\n");
        return -1;
    }

    for (int i = 0; i < n; i++) {
        streams[i].fp = fopen(config->input_files[i], "rb");
        if (!streams[i].fp) {
            fprintf(stderr, "Error: Cannot open %s for merge\n", config->input_files[i]);
            for (int j = 0; j < i; j++) fclose(streams[j].fp);
            free(streams);
            return -1;
        }
        streams[i].exhausted = false;
        advance_stream(&streams[i]);
    }

    long input_count = 0, output_count = 0;
    int rc = kway_merge(streams, n, config->output_file, &input_count, &output_count);

    for (int i = 0; i < n; i++) {
        if (streams[i].fp) fclose(streams[i].fp);
    }
    free(streams);

    if (rc != 0) return -1;

    stats->total_nodes_input  = input_count;
    stats->total_nodes_output = output_count;
    stats->nodes_pruned       = input_count - output_count;

    return 0;
}

// Print merge statistics
void print_merge_stats(MergeStats *stats)
{
    printf("\n=== Merge Statistics ===\n");
    printf("Input nodes:  %ld\n", stats->total_nodes_input);
    printf("Output nodes: %ld\n", stats->total_nodes_output);
    printf("Nodes pruned: %ld\n", stats->nodes_pruned);
    if (stats->total_nodes_input > 0)
        printf("Reduction:    %.2f%%\n",
            100.0 * stats->nodes_pruned / stats->total_nodes_input);
}
