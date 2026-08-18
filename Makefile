CC = gcc
CFLAGS = -Wall -Wextra -Wpedantic -std=c11 -Iinclude -pthread

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

TEST_HASHMAP = test_hashmap
TEST_COMMAND = test_command

$(TARGET): $(SOURCES)
	$(CC) $(CFLAGS) $(SOURCES) -o $(TARGET)

$(TEST_HASHMAP): tests/test_hashmap.c src/hashmap.c
	$(CC) $(CFLAGS) tests/test_hashmap.c src/hashmap.c -o $(TEST_HASHMAP)

$(TEST_COMMAND): tests/test_command.c src/command.c
	$(CC) $(CFLAGS) tests/test_command.c src/command.c -o $(TEST_COMMAND)

test_hashmap: $(TEST_HASHMAP)
	./$(TEST_HASHMAP)

test_command: $(TEST_COMMAND)
	./$(TEST_COMMAND)

test: test_hashmap test_command

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET) $(TEST_HASHMAP) $(TEST_COMMAND)

.PHONY: run clean test test_hashmap test_command