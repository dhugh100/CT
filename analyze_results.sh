#!/bin/bash

# analyze_results.sh - Analyze CSV results from multiple runs
# Usage: ./analyze_results.sh <csv_files...>

if [ $# -eq 0 ]; then
    echo "Usage: $0 <csv_file1> [csv_file2] ..."
    echo ""
    echo "Example:"
    echo "  $0 runs/*/results.csv"
    exit 1
fi

# Combine all CSVs (skip headers after first)
combined=$(mktemp)
head -1 "$1" > "$combined"
for f in "$@"; do
    tail -n +2 "$f" >> "$combined"
done

echo "=== CT Training Results Analysis ==="
echo ""
echo "Total runs analyzed: $(( $(wc -l < "$combined") - 1 ))"
echo ""

# Calculate statistics
awk -F',' 'NR>1 {
    # Policy win percentage
    policy_sum += $14
    policy_count++
    
    # Random win percentage
    random_sum += $21
    
    # Improvement
    improvement_sum += $25
    
    # Coverage
    coverage_sum += $19
    
    # Duration
    duration_sum += $10
    
    # Best improvement
    if ($25 > best_improvement || best_improvement == "") {
        best_improvement = $25
        best_config = $2 "runs/" $3 "iter/" $4 "threads"
    }
}
END {
    print "Average Policy Win Rate: " policy_sum/policy_count "%"
    print "Average Random Win Rate: " random_sum/policy_count "%"
    print "Average Improvement: " improvement_sum/policy_count "%"
    print "Average Coverage: " coverage_sum/policy_count "%"
    print "Average Total Duration: " duration_sum/policy_count "s"
    print ""
    print "Best Improvement: " best_improvement "% (" best_config ")"
}' "$combined"

rm "$combined"
