CC ?= cc
CFLAGS ?= -std=c11 -D_XOPEN_SOURCE=700 -Wall -Wextra -Werror -O2 -g -Iinclude
LDFLAGS ?= -pthread

BIN_DIR := bin
SERVER := $(BIN_DIR)/week8_dbms
TEST_BIN := $(BIN_DIR)/test_week8_dbms

COMMON_SRC := \
	src/bptree.c \
	src/db.c \
	src/http.c \
	src/sql.c \
	src/thread_pool.c \
	src/util.c

SERVER_SRC := src/main.c src/server.c $(COMMON_SRC)
TEST_SRC := tests/test_main.c $(COMMON_SRC)

.PHONY: all test run clean

all: $(SERVER)

$(BIN_DIR):
	mkdir -p $(BIN_DIR)

$(SERVER): $(SERVER_SRC) | $(BIN_DIR)
	$(CC) $(CFLAGS) $(SERVER_SRC) -o $@ $(LDFLAGS)

$(TEST_BIN): $(TEST_SRC) | $(BIN_DIR)
	$(CC) $(CFLAGS) $(TEST_SRC) -o $@ $(LDFLAGS)

test: $(TEST_BIN)
	./$(TEST_BIN)

run: $(SERVER)
	./$(SERVER)

clean:
	rm -rf $(BIN_DIR)
