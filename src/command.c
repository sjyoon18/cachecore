#include "command.h"

#include <stdlib.h>
#include <string.h>

struct command *parse_command(const char *input) {
    if (input == NULL) {
        return NULL;
    }

    struct command *command = malloc(sizeof(*command));

    if (command == NULL) {
        return NULL;
    }

    command->type = COMMAND_INVALID;
    command->key = NULL;
    command->value = NULL;

    char *copy = malloc(strlen(input) + 1);

    if (copy == NULL) {
        free(command);
        return NULL;
    }

    strcpy(copy, input);

    char *token = strtok(copy, " \n");

    if (token == NULL) {
        free(copy);
        return command;
    }

    if (strcmp(token, "PING") == 0) {
        char *extra = strtok(NULL, " \n");
        if (extra == NULL) {
            command->type = COMMAND_PING;
        }
    } else if (strcmp(token, "SET") == 0) {
        char *key = strtok(NULL, " \n");
        char *value = strtok(NULL, " \n");
        char *extra = strtok(NULL, " \n");

        if (key != NULL && value != NULL && extra == NULL) {
            command->key = malloc(strlen(key) + 1);
            if (command->key == NULL) {
                free(command);
                free(copy);
                return NULL;
            }
            strcpy(command->key, key);

            command->value = malloc(strlen(value) + 1);
            
            if (command->value == NULL) {
                free(command->key);
                free(command);
                free(copy);
                return NULL;
            }
            strcpy(command->value, value);

            command->type = COMMAND_SET;
        }
    } else if (strcmp(token, "GET") == 0) {
        char *key = strtok(NULL, " \n");
        char *extra = strtok(NULL, " \n");

        if (key != NULL && extra == NULL) {
            command->key = malloc(strlen(key) + 1);
            if (command->key == NULL) {
                free(command);
                free(copy);
                return NULL;
            }
            strcpy(command->key, key);

            command->type = COMMAND_GET;
        }
    } else if (strcmp(token, "DEL") == 0) {
        char *key = strtok(NULL, " \n");
        char *extra = strtok(NULL, " \n");

        if (key != NULL && extra == NULL) {
            command->key = malloc(strlen(key) + 1);
            if (command->key == NULL) {
                free(command);
                free(copy);
                return NULL;
            }
            strcpy(command->key, key);

            command->type = COMMAND_DEL;
        }
    } else if (strcmp(token, "QUIT") == 0) {
        char *extra = strtok(NULL, " \n");

        if (extra == NULL) {
            command->type = COMMAND_QUIT;
        }
    }

    free(copy);

    return command;
}

void command_destroy(struct command *command) {
    if (command == NULL) {
        return;
    }

    free(command->key);
    free(command->value);
    free(command);
}
