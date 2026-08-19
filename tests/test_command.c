#include "command.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static void test_ping(void) {
    struct command *command = parse_command("PING\n");

    assert(command != NULL);
    assert(command->type == COMMAND_PING);

    command_destroy(command);
}

static void test_set(void) {
    struct command *command = parse_command("SET name Alice\n");

    assert(command != NULL);
    assert(command->type == COMMAND_SET);

    assert(command->key != NULL);
    assert(strcmp(command->key, "name") == 0);

    assert(command->value != NULL);
    assert(strcmp(command->value, "Alice") == 0);

    command_destroy(command);
}

static void test_get(void) {
    struct command *command = parse_command("GET name\n");

    assert(command != NULL);
    assert(command->type == COMMAND_GET);

    assert(command->key != NULL);
    assert(strcmp(command->key, "name") == 0);

    command_destroy(command);
}

static void test_del(void) {
    struct command *command = parse_command("DEL name\n");

    assert(command != NULL);
    assert(command->type == COMMAND_DEL);

    assert(command->key != NULL);
    assert(strcmp(command->key, "name") == 0);

    command_destroy(command);
}

static void test_quit(void) {
    struct command *command = parse_command("QUIT\n");

    assert(command != NULL);
    assert(command->type == COMMAND_QUIT);

    command_destroy(command);
}

static void test_invalid_unknown_command(void) {
    struct command *command = parse_command("HELLO\n");

    assert(command != NULL);
    assert(command->type == COMMAND_INVALID);

    command_destroy(command);
}

static void test_invalid_set_missing_arguments(void) {
    struct command *command = parse_command("SET\n");

    assert(command != NULL);
    assert(command->type == COMMAND_INVALID);

    command_destroy(command);
}

static void test_invalid_get_missing_key(void) {
    struct command *command = parse_command("GET\n");

    assert(command != NULL);
    assert(command->type == COMMAND_INVALID);

    command_destroy(command);
}

static void test_invalid_del_missing_key(void) {
    struct command *command = parse_command("DEL\n");

    assert(command != NULL);
    assert(command->type == COMMAND_INVALID);

    command_destroy(command);
}

static void test_invalid_ping_extra_argument(void) {
    struct command *command = parse_command("PING extra\n");

    assert(command != NULL);
    assert(command->type == COMMAND_INVALID);

    command_destroy(command);
}

static void test_invalid_set_extra_argument(void) {
    struct command *command = parse_command("SET a b c\n");

    assert(command != NULL);
    assert(command->type == COMMAND_INVALID);

    command_destroy(command);
}

int main(void) {
    test_ping();
    test_set();
    test_get();
    test_del();
    test_quit();

    test_invalid_unknown_command();
    test_invalid_set_missing_arguments();
    test_invalid_get_missing_key();
    test_invalid_del_missing_key();
    test_invalid_ping_extra_argument();
    test_invalid_set_extra_argument();

    printf("All command parser tests passed.\n");

    return 0;
}
