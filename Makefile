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

TEST_HASHMAP_BIN = test_hashmap
TEST_COMMAND_BIN = test_command
TEST_DATABASE_BIN = test_database

$(TARGET): $(SOURCES)
	$(CC) $(CFLAGS) $(SOURCES) -o $(TARGET)

$(TEST_HASHMAP_BIN): tests/test_hashmap.c src/hashmap.c
	$(CC) $(CFLAGS) tests/test_hashmap.c src/hashmap.c -o $(TEST_HASHMAP_BIN)

$(TEST_COMMAND_BIN): tests/test_command.c src/command.c
	$(CC) $(CFLAGS) tests/test_command.c src/command.c -o $(TEST_COMMAND_BIN)

$(TEST_DATABASE_BIN): tests/test_database.c \
                     src/database.c \
                     src/hashmap.c \
                     src/aof.c \
                     src/command.c
	$(CC) $(CFLAGS) \
	tests/test_database.c \
	src/database.c \
	src/hashmap.c \
	src/aof.c \
	src/command.c \
	-o $(TEST_DATABASE_BIN)

run-test-hashmap: $(TEST_HASHMAP_BIN)
	./$(TEST_HASHMAP_BIN)

run-test-command: $(TEST_COMMAND_BIN)
	./$(TEST_COMMAND_BIN)

run-test-database: $(TEST_DATABASE_BIN)
	./$(TEST_DATABASE_BIN)

test: run-test-hashmap run-test-command run-test-database

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET) \
	      $(TEST_HASHMAP_BIN) \
	      $(TEST_COMMAND_BIN) \
	      $(TEST_DATABASE_BIN)

.PHONY: run clean test \
        run-test-hashmap \
        run-test-command \
        run-test-database