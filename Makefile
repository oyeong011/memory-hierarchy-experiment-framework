CC ?= cc
CFLAGS ?= -O2 -std=c11 -Wall -Wextra -Wpedantic
LDFLAGS ?=

BIN_DIR := bin
SRC_DIR := src

PROGRAMS := \
	$(BIN_DIR)/pointer_chase \
	$(BIN_DIR)/stream_lite \
	$(BIN_DIR)/stride_access \
	$(BIN_DIR)/row_col_traversal \
	$(BIN_DIR)/matmul_tiled

.PHONY: all clean

all: $(PROGRAMS)

$(BIN_DIR):
	mkdir -p $(BIN_DIR)

$(BIN_DIR)/pointer_chase: $(SRC_DIR)/pointer_chase.c $(SRC_DIR)/bench_util.h | $(BIN_DIR)
	$(CC) $(CFLAGS) -o $@ $(SRC_DIR)/pointer_chase.c $(LDFLAGS)

$(BIN_DIR)/stream_lite: $(SRC_DIR)/stream_lite.c $(SRC_DIR)/bench_util.h | $(BIN_DIR)
	$(CC) $(CFLAGS) -o $@ $(SRC_DIR)/stream_lite.c $(LDFLAGS)

$(BIN_DIR)/stride_access: $(SRC_DIR)/stride_access.c $(SRC_DIR)/bench_util.h | $(BIN_DIR)
	$(CC) $(CFLAGS) -o $@ $(SRC_DIR)/stride_access.c $(LDFLAGS)

$(BIN_DIR)/row_col_traversal: $(SRC_DIR)/row_col_traversal.c $(SRC_DIR)/bench_util.h | $(BIN_DIR)
	$(CC) $(CFLAGS) -o $@ $(SRC_DIR)/row_col_traversal.c $(LDFLAGS)

$(BIN_DIR)/matmul_tiled: $(SRC_DIR)/matmul_tiled.c | $(BIN_DIR)
	$(CC) $(CFLAGS) -o $@ $(SRC_DIR)/matmul_tiled.c $(LDFLAGS)

clean:
	rm -f $(PROGRAMS)
