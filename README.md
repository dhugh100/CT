# Machine Learning Exercise, CFR coded in C language
## Two Person Setback

---

## Overview

I launched this project to learn more about one aspect of machine learning while having fun coding. It is a training exercise, not a production-quality implementation. Even so it might be of interest to other enthusiasts who want to see how a CFR trainer can be implemented in C, or who want to experiment with training their own strategy for Setback.

The project implements Counterfactual Regret Minimization (CFR), a game-theory algorithm used to compute near-optimal strategies for imperfect-information games, applied to the two-player card game Setback (also known as Pitch). The trainer plays thousands of games against itself, accumulating regret data across each decision point until the resulting strategy converges toward a Nash equilibrium. Multiple independent training runs are merged into a single compact strategy file using a memory-efficient k-way merge, and the final strategy is evaluated by pitting it against both a random opponent and a policy baseline. The entire pipeline — train, merge, validate, evaluate — is orchestrated by a single shell script that logs results to CSV for analysis.

Also included is an interactive, terminal-based game that allows play against the strategy. Very basic, but allows direct experience of the strategy during play.

A companion project at `/home/dhugh/NN` uses the self-play dataset output from this project as a training set for neural network models of bidding and card play.

## Installation

**Prerequisites:** GCC, GNU Make, and a POSIX-compatible shell (Linux or macOS).

```bash
# Clone or download the repository, then build all executables:
cd CT
make all
```

Binaries are written to the `bin/` directory. No external libraries are required beyond the standard C library (`libm`, `libpthread`).

---

## Programs

### Source — Common Library (`src/common/`)

| File | Description |
|---|---|
| `types.h` | Defines all shared types and constants: `Card`, `Hand`, `State`, `Key`, `Node`, `Strat`, `Strat_255`, and all action/history bit-flag macros. |
| `deck.c / deck.h` | Handles card dealing, hand evaluation, and end-of-hand scoring including the set (failed bid) penalty. |
| `game.c / game.h` | Implements game rules: legal bid generation, legal play generation, bid/play application, trick resolution, and card-to-action binding. |
| `abstraction.c / abstraction.h` | Builds the compact 12-byte information-set `Key` from a game state, encoding history counters and cards-in-hand buckets for CFR lookup. |
| `strategy.c / strategy.h` | Loads a merged binary strategy file into a sorted lookup table and provides binary-search retrieval of action probabilities for a given game state key. |
| `util.c / util.h` | Provides debugging helpers: card/hand/state printers, full `Node`, `Strat`, and `Strat_255` dump functions (binary, hex, and decoded key fields), and the LCG random number generator. |

### Executable — `ct` (CFR Trainer, `src/ct/`)

| File | Description |
|---|---|
| `cfr.c / cfr.h` | Core CFR engine: FNV-1a hash table with chaining, regret matching (`update_strategy`), regret accumulation (`update_regrets`), and the recursive game-tree traversal (`recurse`). |
| `main.c` | Entry point for the trainer; spawns pthreads, assigns each thread a private hash-table slice, trains both Player 0 and Player 1 per iteration, and serializes learned strategies to a binary file. Nodes visited fewer than the visit threshold are pruned before saving. |

**Usage:**
```bash
./bin/ct <threads> <iterations> <visit_threshold> <output_file> <seed>
```

| Argument | Description |
|---|---|
| `threads` | Number of parallel training threads. |
| `iterations` | Total CFR game-tree traversals (divided evenly across threads). |
| `visit_threshold` | Nodes visited fewer than this many times are excluded from the output file. |
| `output_file` | Path for the output `Strat` binary file. |
| `seed` | Random seed; pass `0` to use a system-generated seed. |

### Executable — `ct-kwayp` (K-Way Merge, `src/ct-kwayp/`)

| File | Description |
|---|---|
| `merge.c / merge.h` | Sorts each input strategy file individually (one file in memory at a time), then performs a streaming k-way merge across all sorted files, averaging duplicate information-set entries without loading more than one record per file simultaneously. Quantizes averaged float strategies to `Strat_255` format for compact output. |
| `main.c` | Entry point for the merge tool; parses arguments and reports merge statistics. |

**Usage:**
```bash
./bin/ct-kwayp <output_file> <min_visits> <input_file1> [input_file2 ...]
```

### Executable — `ct-playa` (Evaluator & Dataset Generator, `src/ct-playa/`)

| File | Description |
|---|---|
| `eval.c / eval.h` | Runs simulated games between the learned policy and either a random or policy opponent, tracking wins, hands, tricks, and node-lookup coverage. Also supports self-play dataset generation for neural network training. |
| `main.c` | Entry point; parses mode and arguments, dispatches to eval or dataset generation. |

**Usage:**
```bash
./bin/ct-playa <strategy_file> <n_games> <mode> <seed> [output_file [dataset_mode]]
```

| Mode | Description |
|---|---|
| `0` | Policy evaluation: P0 uses strategy, P1 plays randomly. Reports win rates and strategy coverage. |
| `1` | Random baseline: both players random. |
| `2` | Self-play dataset generation: both players use the strategy; records each decision to a binary file for neural network training. Requires `output_file` and `dataset_mode` (0=bid NN, 1=play NN). |

### Executable — `ct-pbin` (Validator, `src/ct-pbin/`)

| File | Description |
|---|---|
| `main.c` | Reads a strategy binary file and validates structural integrity: file alignment, action counts, and probability sums. Reports total node count and any anomalies. |

**Usage:**
```bash
./bin/ct-pbin <strategy_file> <format> <print_nodes>
```

| Argument | Description |
|---|---|
| `strategy_file` | Path to strategy binary. |
| `format` | `S` = `Strat` (float, raw training output); `Q` = `Strat_255` (quantized, merge output). |
| `print_nodes` | `Y` to print per-node detail; `N` for summary only. |

### Executable — `ct-playu` (Interactive Play, `src/ct-playu/`)

A rudimentary interactive version of Setback that lets a human player compete against the trained AI strategy.

| File | Description |
|---|---|
| `play.c / play.h` | Top-level game loop, card display helpers, logging, and input utilities. Runs hands until one player reaches the winning score. |
| `bid.c / bid.h` | Bid phase: prompts the human for a bid and retrieves the AI's bid from the strategy; announces the winning bid. |
| `hand.c / hand.h` | Play phase: handles one player's turn per trick, prompts the human for a card, validates legality, and invokes the AI strategy for machine turns. |

**Usage:**
```bash
./bin/ct-playu <strategy_file> <winning_score> <dealer> <seed>
```

| Argument | Description |
|---|---|
| `strategy_file` | Path to a merged strategy binary (e.g. `runs/.../final_strategy.bin`). |
| `winning_score` | The score at which the game ends (e.g. `7`). |
| `dealer` | `0` for machine deals first, `1` for human deals first. |
| `seed` | Random seed; pass `0` to use a system-generated seed. |

The human player is Player 1; the AI is Player 0. Cards are displayed in suit/rank order. A log of all actions is appended to `playu.log` in the working directory.

### Build System

| File | Description |
|---|---|
| `Makefile` | Builds all executables from source; supports individual targets `ct`, `playa`, `kwayp`, `pbin`, `playu`, and `clean`. Uses wildcard rules — new `.c` files in existing source directories are automatically included. |
| `doRun.sh` | Full training pipeline script — see **Execution** below. |

---

## Execution

`doRun.sh` runs the complete pipeline: train → merge → validate → evaluate, with optional self-play dataset generation.

```bash
./doRun.sh <runs> <iterations> <threads> <visit_threshold> <eval_games> <seed> [dataset_mode]
```

| Argument | Description |
|---|---|
| `runs` | Number of independent training runs to perform (each produces one strategy file). |
| `iterations` | Number of CFR game-tree traversals per run (higher = stronger strategy). |
| `threads` | Number of parallel threads per training run. |
| `visit_threshold` | Nodes visited fewer than this many times are pruned during training. |
| `eval_games` | Number of games used to evaluate the merged strategy. |
| `seed` | Base random seed; pass `0` to use a system-generated seed. |
| `dataset_mode` | Optional: `0` = generate bid NN dataset; `1` = generate play NN dataset. Omit to skip dataset generation. |

**Examples:**
```bash
./doRun.sh 20 250000 1 3 10000 0          # train, merge, evaluate — no dataset
./doRun.sh 20 250000 1 3 10000 0 0        # same, plus bid NN dataset
```

**Output** is written to a timestamped directory under `runs/`:
- `run.log` — full timestamped execution log
- `results.csv` — machine-readable summary (win rates, node counts, durations, improvements)
- `final_strategy.bin` — the merged, quantized strategy file ready for play or evaluation
- `dataset.bin` — self-play decision records for neural network training (only if `dataset_mode` provided)
- `temp/` — intermediate per-run files (can be removed after a successful run)

Convenience symlinks are created (or updated) in the working directory after each run:
- `last_strategy.bin` → most recent `final_strategy.bin`
- `last_results.csv` → most recent `results.csv`
- `last_dataset.bin` → most recent `dataset.bin` (only if dataset was generated)

**Prerequisites:** Run `make all` before executing the script.

---

## Key Data Structures

### Strategy Serialization

Two separate structs are used — one for training output, one for the compact on-disk format after merging:

| Struct | Used by | Format | Size |
|---|---|---|---|
| `Strat` | `ct` output, `ct-kwayp` input | `float strategy[MAX_ACTIONS]` | ~63 bytes/node |
| `Strat_255` | `ct-kwayp` output, `ct-playa` / `ct-pbin` input | `UC s255[MAX_ACTIONS]` (0–255) | ~33 bytes/node |

Quantization: `s255[i] = (UC)(strategy[i] * 255.0f + 0.5f)`. Dequantization on load: `strategy[i] = s255[i] / 255.0f`. Using two separate structs (rather than a union) achieves the full memory savings on disk, since a union's size equals its largest member.

### State Key

The 12-byte `Key` is a compact abstraction of the game state used for CFR node lookup. It encodes dealer, bids, bid flags, trump, trick number, led suit, play history (as rank-bucket counters grouped by led/response × trump/other), and current hand contents (as rank buckets). This abstraction reduces the effective game tree to approximately 1 million distinct information sets.

### CFR Node

During training, each information set is stored as a `Node` containing the key, legal actions, cumulative regret sums, current strategy, cumulative strategy sums, and a visit counter. Regret matching derives the next strategy from positive regrets; the final average strategy is computed from the cumulative strategy sums across all iterations.

---

## License

This project is released under the [GPL v3.0](https://opensource.org/license/gpl-3-0).

---

## Author

Dave Hugh with tutoring, refactoring, documentation, debugging assistance, and some coding by GitHub Copilot and Anthropic AI models.
