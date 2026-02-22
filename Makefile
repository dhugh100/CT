# Copyright (c) 2026 Dave Hugh. All rights reserved.
# Licensed under the MIT License. See README.md for details.
CC = gcc
CFLAGS = -g3 -Isrc/common
LDFLAGS = -pthread -lm

SRC_DIR = src
OBJ_DIR = obj
BIN_DIR = bin

# Common objects (shared by all executables)
COMMON_SRCS = $(wildcard $(SRC_DIR)/common/*.c)
COMMON_OBJS = $(COMMON_SRCS:$(SRC_DIR)/common/%.c=$(OBJ_DIR)/common/%.o)

# CT (training) objects
CT_SRCS = $(wildcard $(SRC_DIR)/ct/*.c)
CT_OBJS = $(CT_SRCS:$(SRC_DIR)/ct/%.c=$(OBJ_DIR)/ct/%.o)

# CT-PLAYA (evaluation) objects
PLAYA_SRCS = $(wildcard $(SRC_DIR)/ct-playa/*.c)
PLAYA_OBJS = $(PLAYA_SRCS:$(SRC_DIR)/ct-playa/%.c=$(OBJ_DIR)/ct-playa/%.o)

# CT-KWAYP (merge) objects
KWAYP_SRCS = $(wildcard $(SRC_DIR)/ct-kwayp/*.c)
KWAYP_OBJS = $(KWAYP_SRCS:$(SRC_DIR)/ct-kwayp/%.c=$(OBJ_DIR)/ct-kwayp/%.o)

# CT-PBIN (validator) objects
PBIN_SRCS = $(wildcard $(SRC_DIR)/ct-pbin/*.c)
PBIN_OBJS = $(PBIN_SRCS:$(SRC_DIR)/ct-pbin/%.c=$(OBJ_DIR)/ct-pbin/%.o)

# All targets
ALL_TARGETS = $(BIN_DIR)/ct $(BIN_DIR)/ct-playa $(BIN_DIR)/ct-kwayp $(BIN_DIR)/ct-pbin

.PHONY: all clean ct playa kwayp pbin

all: $(ALL_TARGETS)

# Build ct (training)
ct: $(BIN_DIR)/ct

$(BIN_DIR)/ct: $(COMMON_OBJS) $(CT_OBJS) | $(BIN_DIR)
	$(CC) $^ -o $@ $(LDFLAGS)

# Build ct-playa (evaluation)
playa: $(BIN_DIR)/ct-playa

$(BIN_DIR)/ct-playa: $(COMMON_OBJS) $(PLAYA_OBJS) | $(BIN_DIR)
	$(CC) $^ -o $@ $(LDFLAGS)

# Build ct-kwayp (merge)
kwayp: $(BIN_DIR)/ct-kwayp

$(BIN_DIR)/ct-kwayp: $(COMMON_OBJS) $(KWAYP_OBJS) | $(BIN_DIR)
	$(CC) $^ -o $@ $(LDFLAGS)

# Build ct-pbin (validator)
pbin: $(BIN_DIR)/ct-pbin

$(BIN_DIR)/ct-pbin: $(COMMON_OBJS) $(PBIN_OBJS) | $(BIN_DIR)
	$(CC) $^ -o $@ $(LDFLAGS)

# Compile common objects
$(OBJ_DIR)/common/%.o: $(SRC_DIR)/common/%.c | $(OBJ_DIR)/common
	$(CC) $(CFLAGS) -c $< -o $@

# Compile ct objects
$(OBJ_DIR)/ct/%.o: $(SRC_DIR)/ct/%.c | $(OBJ_DIR)/ct
	$(CC) $(CFLAGS) -Isrc/ct -c $< -o $@

# Compile ct-playa objects
$(OBJ_DIR)/ct-playa/%.o: $(SRC_DIR)/ct-playa/%.c | $(OBJ_DIR)/ct-playa
	$(CC) $(CFLAGS) -Isrc/ct-playa -c $< -o $@

# Compile ct-kwayp objects
$(OBJ_DIR)/ct-kwayp/%.o: $(SRC_DIR)/ct-kwayp/%.c | $(OBJ_DIR)/ct-kwayp
	$(CC) $(CFLAGS) -Isrc/ct-kwayp -c $< -o $@

# Compile ct-pbin objects
$(OBJ_DIR)/ct-pbin/%.o: $(SRC_DIR)/ct-pbin/%.c | $(OBJ_DIR)/ct-pbin
	$(CC) $(CFLAGS) -Isrc/ct-pbin -c $< -o $@

# Create directories
$(OBJ_DIR)/common $(OBJ_DIR)/ct $(OBJ_DIR)/ct-playa $(OBJ_DIR)/ct-kwayp $(OBJ_DIR)/ct-pbin:
	mkdir -p $@

$(BIN_DIR):
	mkdir -p $@

clean:
	rm -rf $(OBJ_DIR) $(BIN_DIR)
