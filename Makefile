CC ?= cc
CFLAGS ?= -std=c11 -D_XOPEN_SOURCE=700 -Wall -Wextra -Werror -O2 -g -Iinclude
LDFLAGS ?= -pthread
PORT ?= 8080
THREADS ?= 4
DATA_FILE ?= data/users.csv

BIN_DIR := bin
SERVER := $(BIN_DIR)/week8_dbms
TEST_BIN := $(BIN_DIR)/test_week8_dbms
BUILD_ENV := $(BIN_DIR)/.build-env

COMMON_SRC := \
	src/bptree.c \
	src/db.c \
	src/http.c \
	src/sql.c \
	src/thread_pool.c \
	src/util.c

SERVER_SRC := src/main.c src/server.c $(COMMON_SRC)
TEST_SRC := tests/test_main.c $(COMMON_SRC)

.PHONY: all rebuild rerun mac mac-run mac-test test run clean FORCE

all: $(SERVER)

rebuild:
	@$(MAKE) clean
	@$(MAKE) all

rerun:
	@$(MAKE) rebuild
	@$(MAKE) run

mac: rebuild

mac-run: rerun

mac-test:
	@$(MAKE) rebuild
	@$(MAKE) test

$(BIN_DIR):
	@printf "\n%s\n" "------------------------------------------------------------"
	@printf "%s\n" "Prepare build directory"
	@printf "%s\n" "------------------------------------------------------------"
	@printf "\nCommand\n  $$ mkdir -p $(BIN_DIR)\n\n"
	@mkdir -p $(BIN_DIR)

$(BUILD_ENV): FORCE | $(BIN_DIR)
	@signature="$$(uname -s) $$(uname -m) | CC=$(CC) | CFLAGS=$(CFLAGS) | LDFLAGS=$(LDFLAGS)"; \
	if [ ! -f "$@" ] || [ "$$(cat "$@")" != "$$signature" ]; then \
		printf "\n%s\n" "------------------------------------------------------------"; \
		printf "%s\n" "Check build environment"; \
		printf "%s\n" "------------------------------------------------------------"; \
		printf "\nDetected\n  %s\n\n" "$$signature"; \
		printf "Stale binaries\n  removed $(SERVER) $(TEST_BIN)\n\n"; \
		rm -f "$(SERVER)" "$(TEST_BIN)"; \
		printf "%s\n" "$$signature" > "$@"; \
	fi

$(SERVER): $(SERVER_SRC) $(BUILD_ENV) Makefile | $(BIN_DIR)
	@printf "\n%s\n" "------------------------------------------------------------"
	@printf "%s\n" "Build server"
	@printf "%s\n" "------------------------------------------------------------"
	@printf "\nCommand\n  $$ %s\n\n" "$(CC) $(CFLAGS) $(SERVER_SRC) -o $@ $(LDFLAGS)"
	@$(CC) $(CFLAGS) $(SERVER_SRC) -o $@ $(LDFLAGS)

$(TEST_BIN): $(TEST_SRC) $(BUILD_ENV) Makefile | $(BIN_DIR)
	@printf "\n%s\n" "------------------------------------------------------------"
	@printf "%s\n" "Build test binary"
	@printf "%s\n" "------------------------------------------------------------"
	@printf "\nCommand\n  $$ %s\n\n" "$(CC) $(CFLAGS) $(TEST_SRC) -o $@ $(LDFLAGS)"
	@$(CC) $(CFLAGS) $(TEST_SRC) -o $@ $(LDFLAGS)

test: $(TEST_BIN)
	@printf "\n%s\n" "------------------------------------------------------------"
	@printf "%s\n" "Run tests"
	@printf "%s\n" "------------------------------------------------------------"
	@printf "\nCommand\n  $$ ./$(TEST_BIN)\n\n"
	@./$(TEST_BIN)

run: $(SERVER)
	@printf "\n%s\n" "------------------------------------------------------------"
	@printf "%s\n" "Run server"
	@printf "%s\n" "------------------------------------------------------------"
	@printf "\nCommand\n  $$ ./$(SERVER) $(PORT) $(THREADS) $(DATA_FILE)\n\n"
	@./$(SERVER) $(PORT) $(THREADS) $(DATA_FILE)

clean:
	@printf "\n%s\n" "------------------------------------------------------------"
	@printf "%s\n" "Clean build output"
	@printf "%s\n" "------------------------------------------------------------"
	@printf "\nCommand\n  $$ rm -rf $(BIN_DIR)\n\n"
	@rm -rf $(BIN_DIR)
