#!/bin/bash
# Copyright (c) 2026 Dave Hugh. All rights reserved.
# Licensed under the MIT License. See README.md for details.

# runCT.sh - Train and evaluate CFR models with CSV logging
# Usage: ./runCT.sh <runs> <iterations> <threads> <eval_games> <seed>

# Don't exit on error for grep commands
set +e

# ==============================================================================
# Configuration and Argument Parsing
# ==============================================================================

if [ $# -ne 5 ]; then
    echo "Usage: $0 <runs> <iterations> <threads> <eval_games> <seed>"
    echo ""
    echo "Arguments:"
    echo "  runs        - Number of training runs to perform"
    echo "  iterations  - CFR iterations per run"
    echo "  threads     - Number of threads per run"
    echo "  eval_games  - Number of games for evaluation"
    echo "  seed        - Base random seed (0 for random)"
    echo ""
    echo "Example:"
    echo "  $0 20 250000 1 10000 0"
    exit 1
fi

RUNS=$1
ITERATIONS=$2
THREADS=$3
EVAL_GAMES=$4
BASE_SEED=$5

# Generate base seed if 0
if [ $BASE_SEED -eq 0 ]; then
    BASE_SEED=$RANDOM
fi

# ==============================================================================
# Setup
# ==============================================================================

TIMESTAMP=$(date +%Y%m%d_%H%M%S)
RUN_DIR="runs/run_${TIMESTAMP}"
TEMP_DIR="${RUN_DIR}/temp"
LOG_FILE="${RUN_DIR}/run.log"
CSV_FILE="${RUN_DIR}/results.csv"
FINAL_STRATEGY="${RUN_DIR}/final_strategy.bin"

# Create directories
mkdir -p "$RUN_DIR"
mkdir -p "$TEMP_DIR"

# ==============================================================================
# Logging Functions
# ==============================================================================

log() {
    echo "$(date +'%Y-%m-%d %H:%M:%S') $*" | tee -a "$LOG_FILE"
}

log_error() {
    echo "$(date +'%Y-%m-%d %H:%M:%S') ERROR: $*" | tee -a "$LOG_FILE" >&2
}

# ==============================================================================
# CSV Setup
# ==============================================================================

init_csv() {
    cat > "$CSV_FILE" << EOF
timestamp,runs,iterations,threads,eval_games,base_seed,training_duration,merge_duration,total_duration,nodes_before_merge,nodes_after_merge,prune_percent,policy_games_won,policy_win_percent,policy_hands_won,policy_tricks_won,policy_nodes_found,policy_nodes_not_found,policy_coverage_percent,random_games_won,random_win_percent,random_hands_won,random_tricks_won,improvement_games,improvement_hands,improvement_tricks
EOF
}

# ==============================================================================
# Training Phase
# ==============================================================================

train_models() {
    {
        log "=== Starting Training Phase ==="
        log "Configuration: $RUNS runs, $ITERATIONS iterations, $THREADS threads"
        
        local start_time=$(date +%s)
        local total_nodes=0
        
        for i in $(seq 0 $((RUNS - 1))); do
            local run_seed=$((BASE_SEED + i * 1000))
            local output_file="${TEMP_DIR}/run_${i}.bin"
            
            log "Starting run $((i + 1))/$RUNS with seed $run_seed"
            log "Executing ./bin/ct $THREADS $ITERATIONS $output_file $run_seed"
            
            # Run training
            ./bin/ct $THREADS $ITERATIONS $output_file $run_seed >> "$LOG_FILE" 2>&1
            
            if [ $? -ne 0 ]; then
                log_error "Training run $i failed"
                return 1
            fi
            
            # Get file size and node count
            if [ -f "$output_file" ]; then
                local file_size=$(stat -f%z "$output_file" 2>/dev/null || stat -c%s "$output_file" 2>/dev/null)
                local node_count=$((file_size / 41))  # sizeof(Strat) = 41 bytes
                total_nodes=$((total_nodes + node_count))
                
                log "Completed run $((i + 1))/$RUNS: $node_count nodes"
            else
                log_error "Output file not created for run $i"
                return 1
            fi
        done
        
        local end_time=$(date +%s)
        local duration=$((end_time - start_time))
        
        log "Training completed: $RUNS runs in ${duration}s"
        log "Average duration per run: $((duration / RUNS))s"
        log "Total nodes before merge: $total_nodes"
    } >&2
    
    # Return ONLY the duration to stdout
    echo "$duration"
}

# ==============================================================================
# Merge Phase
# ==============================================================================

merge_strategies() {
    {
        log "=== Starting Merge Phase ==="
        
        local start_time=$(date +%s)
        
        # Build list of input files
        local input_files=()
        for i in $(seq 0 $((RUNS - 1))); do
            input_files+=("${TEMP_DIR}/run_${i}.bin")
        done
        
        # Run merge
        log "Merging ${#input_files[@]} strategy files..."
        log "Executing ./bin/ct-kwayp $FINAL_STRATEGY 0 ${input_files[@]}"
        ./bin/ct-kwayp "$FINAL_STRATEGY" 0 "${input_files[@]}" > "${TEMP_DIR}/merge.log" 2>&1
        
        if [ $? -ne 0 ]; then
            log_error "Merge failed"
            cat "${TEMP_DIR}/merge.log"
            return 1
        fi
        
        local end_time=$(date +%s)
        local duration=$((end_time - start_time))
        
        # Extract statistics from merge log
        local nodes_before=$(grep "Input nodes:" "${TEMP_DIR}/merge.log" | awk '{print $3}')
        local nodes_after=$(grep "Output nodes:" "${TEMP_DIR}/merge.log" | awk '{print $3}')
        local prune_percent=$(grep "Reduction:" "${TEMP_DIR}/merge.log" | awk '{print $2}')
        
        log "Merge completed in ${duration}s"
        log "Nodes before merge: $nodes_before"
        log "Nodes after merge: $nodes_after"
        log "Pruned: $prune_percent"
    } >&2
    
    # Return ONLY the CSV data to stdout
    echo "${duration},${nodes_before},${nodes_after},${prune_percent}"
}

# ==============================================================================
# Validation Phase
# ==============================================================================

validate_strategy() {
    log "=== Validating Final Strategy ==="
    
    ./bin/ct-pbin "$FINAL_STRATEGY" n > "${TEMP_DIR}/validation.log" 2>&1
    
    if [ $? -ne 0 ]; then
        log_error "Validation failed"
        cat "${TEMP_DIR}/validation.log" | tee -a "$LOG_FILE"
        return 1
    fi
    
    log "Strategy validated successfully"
    grep "Total nodes validated:" "${TEMP_DIR}/validation.log" | tee -a "$LOG_FILE"
    return 0
}

# ==============================================================================
# Evaluation Phase
# ==============================================================================
evaluate_strategy() {
    local mode=$1
    local mode_name=$2
    
    {
        log "=== Evaluating Strategy: $mode_name ==="
        log "Executing ./bin/ct-playa $FINAL_STRATEGY $EVAL_GAMES $mode $BASE_SEED"
        
        ./bin/ct-playa "$FINAL_STRATEGY" "$EVAL_GAMES" "$mode" "$BASE_SEED"  > "${TEMP_DIR}/eval_${mode_name}.log" 2>&1
        
        if [ $? -ne 0 ]; then
            log_error "Evaluation failed for $mode_name"
            cat "${TEMP_DIR}/eval_${mode_name}.log"
            return 1
        fi
        
        # Debug: show what we're parsing
        grep "Games won by P0" "${TEMP_DIR}/eval_${mode_name}.log"
        
        # Parse results - extract the number and percentage correctly
        # Line format: "Games won by P0 (machine): 4174 (41.74%)"
        local games_won=$(grep "Games won by P0 (machine):" "${TEMP_DIR}/eval_${mode_name}.log" | awk '{print $6}')
        local win_percent=$(grep "Games won by P0 (machine):" "${TEMP_DIR}/eval_${mode_name}.log" | sed -n 's/.*(\([0-9.]*\)%).*/\1/p')
        local hands_won=$(grep "Hands won by P0:" "${TEMP_DIR}/eval_${mode_name}.log" | awk '{print $NF}')
        local tricks_won=$(grep "Tricks won by P0:" "${TEMP_DIR}/eval_${mode_name}.log" | awk '{print $NF}')
        
        # Set defaults if empty
        games_won=${games_won:-0}
        win_percent=${win_percent:-0.0}
        hands_won=${hands_won:-0}
        tricks_won=${tricks_won:-0}
        
        # Only for policy mode
        local nodes_found="0"
        local nodes_not_found="0"
        local coverage_percent="0.0"
        
        if [ "$mode" -eq 0 ]; then
            # Line format: "Nodes found: 2345 (67.49%)"
            nodes_found=$(grep "Nodes found:" "${TEMP_DIR}/eval_${mode_name}.log" | awk '{print $3}')
            nodes_not_found=$(grep "Nodes not found:" "${TEMP_DIR}/eval_${mode_name}.log" | awk '{print $4}')
            coverage_percent=$(grep "Nodes found:" "${TEMP_DIR}/eval_${mode_name}.log" | sed -n 's/.*(\([0-9.]*\)%).*/\1/p')
            
            # Set defaults
            nodes_found=${nodes_found:-0}
            nodes_not_found=${nodes_not_found:-0}
            coverage_percent=${coverage_percent:-0.0}
        fi
        
        log "$mode_name Results:"
        log "  Games won: $games_won ($win_percent%)"
        log "  Hands won: $hands_won"
        log "  Tricks won: $tricks_won"
        
        if [ "$mode" -eq 0 ]; then
            log "  Coverage: $coverage_percent% ($nodes_found found, $nodes_not_found not found)"
        fi
    } >&2
    
    # Return ONLY the CSV data to stdout
    echo "${games_won},${win_percent},${hands_won},${tricks_won},${nodes_found},${nodes_not_found},${coverage_percent}"
}

# ==============================================================================
# Main Execution
# ==============================================================================

main() {
    log "========================================="
    log "CT Training Pipeline"
    log "========================================="
    log "Run Directory: $RUN_DIR"
    log "Timestamp: $TIMESTAMP"
    log ""
    
    # Initialize CSV
    init_csv
    
    # Check executables exist
    if [ ! -x "./bin/ct" ]; then
        log_error "ct executable not found. Run 'make all' first."
        exit 1
    fi
    
    if [ ! -x "./bin/ct-kwayp" ]; then
        log_error "ct-kwayp executable not found. Run 'make all' first."
        exit 1
    fi
    
    if [ ! -x "./bin/ct-playa" ]; then
        log_error "ct-playa executable not found. Run 'make all' first."
        exit 1
    fi
    
    if [ ! -x "./bin/ct-pbin" ]; then
        log_error "ct-pbin executable not found. Run 'make all' first."
        exit 1
    fi
    
    # Training
    log "Starting training phase..."
    training_duration=$(train_models)
    training_duration=${training_duration:-0}
    if [ "$training_duration" -eq 0 ]; then
        log_error "Training failed"
        exit 1
    fi
    
    # Merge
    log "Starting merge phase..."
    merge_result=$(merge_strategies)
    if [ -z "$merge_result" ]; then
        log_error "Merge failed"
        exit 1
    fi
    
    # Parse merge results
    IFS=',' read -r merge_duration nodes_before nodes_after prune_percent <<< "$merge_result"
    
    # Set defaults
    merge_duration=${merge_duration:-0}
    nodes_before=${nodes_before:-0}
    nodes_after=${nodes_after:-0}
    prune_percent=${prune_percent:-0.0}
    
    # Validate
    validate_strategy
    if [ $? -ne 0 ]; then
        log_error "Validation failed"
        exit 1
    fi
    
    # Evaluate - Policy mode
    log "Starting policy evaluation..."
    policy_result=$(evaluate_strategy 0 "policy")
    if [ -z "$policy_result" ]; then
        log_error "Policy evaluation failed"
        exit 1
    fi
    IFS=',' read -r policy_games_won policy_win_percent policy_hands_won policy_tricks_won \
                    policy_nodes_found policy_nodes_not_found policy_coverage_percent <<< "$policy_result"
    
    # Evaluate - Random baseline
    log "Starting random baseline evaluation..."
    random_result=$(evaluate_strategy 1 "random")
    if [ -z "$random_result" ]; then
        log_error "Random evaluation failed"
        exit 1
    fi
    IFS=',' read -r random_games_won random_win_percent random_hands_won random_tricks_won _ _ _ <<< "$random_result"
    
    # Set defaults for any empty values
    policy_games_won=${policy_games_won:-0}
    policy_win_percent=${policy_win_percent:-0.0}
    policy_hands_won=${policy_hands_won:-0}
    policy_tricks_won=${policy_tricks_won:-0}
    policy_nodes_found=${policy_nodes_found:-0}
    policy_nodes_not_found=${policy_nodes_not_found:-0}
    policy_coverage_percent=${policy_coverage_percent:-0.0}
    
    random_games_won=${random_games_won:-0}
    random_win_percent=${random_win_percent:-0.0}
    random_hands_won=${random_hands_won:-0}
    random_tricks_won=${random_tricks_won:-0}
    
    # Calculate improvements with safe defaults
    improvement_games=$(awk "BEGIN {printf \"%.2f\", ${policy_win_percent} - ${random_win_percent}}")
    improvement_hands=$(awk "BEGIN {printf \"%.2f\", (${policy_hands_won} - ${random_hands_won}) / ${EVAL_GAMES} * 100}")
    improvement_tricks=$(awk "BEGIN {printf \"%.2f\", (${policy_tricks_won} - ${random_tricks_won}) / (${EVAL_GAMES} * 12) * 100}")
    
    # Calculate total duration
    total_duration=$((training_duration + merge_duration))
    
    # Write to CSV
    cat >> "$CSV_FILE" << EOF
$TIMESTAMP,$RUNS,$ITERATIONS,$THREADS,$EVAL_GAMES,$BASE_SEED,$training_duration,$merge_duration,$total_duration,$nodes_before,$nodes_after,$prune_percent,$policy_games_won,$policy_win_percent,$policy_hands_won,$policy_tricks_won,$policy_nodes_found,$policy_nodes_not_found,$policy_coverage_percent,$random_games_won,$random_win_percent,$random_hands_won,$random_tricks_won,$improvement_games,$improvement_hands,$improvement_tricks
EOF
    
    # Summary
    log ""
    log "========================================="
    log "Run Complete!"
    log "========================================="
    log "Total Duration: ${total_duration}s (Training: ${training_duration}s, Merge: ${merge_duration}s)"
    log "Final Strategy: $FINAL_STRATEGY"
    log "Results CSV: $CSV_FILE"
    log ""
    log "Performance Summary:"
    log "  Policy win rate: ${policy_win_percent}%"
    log "  Random win rate: ${random_win_percent}%"
    log "  Improvement: ${improvement_games}% (games)"
    log "  Coverage: ${policy_coverage_percent}%"
    log ""
    log "Clean up temp files with: rm -rf ${TEMP_DIR}"
}

# Run main
main

exit 0
