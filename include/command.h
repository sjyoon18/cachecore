#ifndef COMMAND_H
#define COMMAND_H

enum command_type {
    COMMAND_INVALID,
    COMMAND_PING,
    COMMAND_SET,
    COMMAND_GET,
    COMMAND_DEL,
    COMMAND_QUIT
};

struct command {
    enum command_type type;
    char *key;
    char *value;
};

struct command *parse_command(const char *input);

void command_destroy(struct command *command);

#endif
