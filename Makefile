CC = gcc
CFLAGS = -Wall -Wextra -Wpedantic -std=c11 -Iinclude -pthread -D_DARWIN_C_SOURCE

TARGET = cachecore

SOURCES = src/main.c \
          src/server.c \
          src/command.c \
          src/database.c \
          src/hashmap.c \
          src/queue.c \
          src/thread_pool.c \
          src/aof.c \
          src/client_manager.c

HEADERS = $(wildcard include/*.h)

TEST_HASHMAP_BIN = test_hashmap
TEST_COMMAND_BIN = test_command
TEST_DATABASE_BIN = test_database
STRESS_CLIENT_BIN = stress_client
BENCHMARK_BIN = benchmark_client

$(TARGET): $(SOURCES) $(HEADERS)
	$(CC) $(CFLAGS) $(SOURCES) -o $(TARGET)

$(TEST_HASHMAP_BIN): tests/unit/test_hashmap.c src/hashmap.c include/hashmap.h
	$(CC) $(CFLAGS) \
	tests/unit/test_hashmap.c \
	src/hashmap.c \
	-o $(TEST_HASHMAP_BIN)

$(TEST_COMMAND_BIN): tests/unit/test_command.c src/command.c include/command.h
	$(CC) $(CFLAGS) \
	tests/unit/test_command.c \
	src/command.c \
	-o $(TEST_COMMAND_BIN)

$(TEST_DATABASE_BIN): tests/integration/test_database.c \
                     src/database.c \
                     src/hashmap.c \
                     src/aof.c \
                     src/command.c \
                     include/database.h \
                     include/hashmap.h \
                     include/aof.h \
                     include/command.h
	$(CC) $(CFLAGS) \
	tests/integration/test_database.c \
	src/database.c \
	src/hashmap.c \
	src/aof.c \
	src/command.c \
	-o $(TEST_DATABASE_BIN)

$(STRESS_CLIENT_BIN): tests/stress/stress_client.c
	$(CC) $(CFLAGS) \
	tests/stress/stress_client.c \
	-o $(STRESS_CLIENT_BIN)

$(BENCHMARK_BIN): bench/benchmark_client.c
	$(CC) $(CFLAGS) \
	bench/benchmark_client.c \
	-o $(BENCHMARK_BIN)

run-test-hashmap: $(TEST_HASHMAP_BIN)
	./$(TEST_HASHMAP_BIN)

run-test-command: $(TEST_COMMAND_BIN)
	./$(TEST_COMMAND_BIN)

run-test-database: $(TEST_DATABASE_BIN)
	./$(TEST_DATABASE_BIN)

stress-client: $(STRESS_CLIENT_BIN)
	./$(STRESS_CLIENT_BIN)

benchmark: $(BENCHMARK_BIN)
	./$(BENCHMARK_BIN)

test: run-test-hashmap run-test-command run-test-database

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET) \
	      $(TEST_HASHMAP_BIN) \
	      $(TEST_COMMAND_BIN) \
	      $(TEST_DATABASE_BIN) \
		  $(STRESS_CLIENT_BIN) \
		  $(BENCHMARK_BIN)

.PHONY: run clean test \
        run-test-hashmap \
        run-test-command \
        run-test-database \
		stress-client \
		benchmark