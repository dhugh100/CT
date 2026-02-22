# Machine Learning Exercise, CFR coded in C language
## Two Person Setback

---

## Overview

I launched this project to learn more about one aspect of machine learning while having fun coding. It is a training exercise, not a production-quality implementation. Even so it might of interest to other enthusiasts who want to see how a CFR trainer can be implemented in C, or who want to experiment with training their own strategy for Setback.  

The project implements Counterfactual Regret Minimization (CFR), a game-theory algorithm used to compute near-optimal strategies for imperfect-information games, applied to the two-player card game Setback (also known as Pitch). The trainer plays thousands of games against itself, accumulating regret data across each decision point until the resulting strategy converges toward a Nash equilibrium. Multiple independent training runs are merged into a single compact strategy file using a memory-efficient k-way merge, and the final strategy is evaluated by pitting it against both a random opponent and a policy baseline. The entire pipeline — train, merge, validate, evaluate — is orchestrated by a single shell script that logs results to CSV for analysis.

## Installation

**Prerequisites:** GCC, GNU Make, and a POSIX-compatible shell (Linux or macOS).

```bash
# Clone or download the repository, then build all four executables:
cd CT
make all
```

Binaries are written to the `bin/` directory. No external libraries are required beyond the standard C library (`libm`, `libpthread`).

---

## Programs

### Source — Common Library (`src/common/`)

| File | Description |
|---|---|
| `types.h` | Defines all shared types and constants: `Card`, `Hand`, `State`, `Key`, `Node`, `Strat`, and all action/history bit-flag macros. |
| `deck.c / deck.h` | Handles card dealing, hand evaluation, and end-of-hand scoring including the set (failed bid) penalty. |
| `game.c / game.h` | Implements game rules: legal bid generation, legal play generation, bid/play application, trick resolution, and card-to-action binding. |
| `abstraction.c / abstraction.h` | Builds the compact 15-byte information-set `Key` from a game state, encoding history counters and cards-in-hand buckets for CFR lookup. |
| `util.c / util.h` | Provides debugging helpers: card/hand/state printers, full `Node` and `Strat` dump functions (binary, hex, and decoded key fields), and the LCG random number generator. |

### Executable — `ct` (CFR Trainer, `src/ct/`)

| File | Description |
|---|---|
| `cfr.c / cfr.h` | Core CFR engine: FNV-1a hash table with chaining, regret matching (`update_strategy`), regret accumulation (`update_regrets`), and the recursive game-tree traversal (`recurse`). |
| `main.c` | Entry point for the trainer; spawns pthreads, assigns each thread a private hash-table slice, trains both Player 0 and Player 1 per iteration, and serializes learned strategies to a binary file. |

### Executable — `ct-kwayp` (K-Way Merge, `src/ct-kwayp/`)

| File | Description |
|---|---|
| `merge.c / merge.h` | Sorts each input strategy file individually (one file in memory at a time), then performs a streaming k-way merge across all sorted files, averaging duplicate information-set entries without loading more than one record per file simultaneously. |
| `main.c` | Entry point for the merge tool; parses arguments and reports merge statistics. |

### Executable — `ct-playa` (Evaluator, `src/ct-playa/`)

| File | Description |
|---|---|
| `strategy.c / strategy.h` | Loads a merged binary strategy file into a sorted lookup table and provides binary-search retrieval of action probabilities for a given game state key. |
| `eval.c / eval.h` | Runs simulated games between the learned policy and either a random or policy opponent, tracking wins, hands, tricks, and node-lookup coverage. |
| `main.c` | Entry point for the evaluator; parses mode (policy vs. random) and writes results to stdout for pipeline parsing. |

### Executable — `ct-pbin` (Validator, `src/ct-pbin/`)

| File | Description |
|---|---|
| `main.c` | Reads a strategy binary file and validates structural integrity, reporting total node count and any format anomalies. |

### Build & Pipeline

| File | Description |
|---|---|
| `Makefile` | Builds all four executables from source; supports individual targets `ct`, `playa`, `kwayp`, `pbin`, and `clean`. |
| `runCT.sh` | Full training pipeline script — see **Execution** below. |

---

## Execution

`runCT.sh` runs the complete pipeline: train → merge → validate → evaluate.

```bash
./runCT.sh <runs> <iterations> <threads> <eval_games> <seed>
```

| Argument | Description |
|---|---|
| `runs` | Number of independent training runs to perform (each produces one strategy file). |
| `iterations` | Number of CFR game-tree traversals per run (higher = stronger strategy). |
| `threads` | Number of parallel threads per training run. |
| `eval_games` | Number of games used to evaluate the merged strategy. |
| `seed` | Base random seed; pass `0` to use a system-generated seed. |

**Example:**
```bash
./runCT.sh 20 250000 1 10000 0
```
This performs 20 training runs of 250,000 iterations each, merges all resulting strategy files, validates the output, then evaluates it against both a policy opponent and a random baseline over 10,000 games.

**Output** is written to a timestamped directory under `runs/`:
- `run.log` — full timestamped execution log
- `results.csv` — machine-readable summary (win rates, node counts, durations, improvements)
- `final_strategy.bin` — the merged, sorted strategy file ready for play

**Prerequisites:** Run `make all` before executing the script.

---

## License

This project is released under the [GPL v3.0](https://opensource.org/license/gpl-3-0).

---

## Author

Dave Hugh with tutoring, refactoring, documentation, debugging assistance, and some coding by Github ChatGPT and Anthropic AI models.
