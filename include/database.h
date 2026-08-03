#ifndef DATABASE_H
#define DATABASE_H

#include <stdbool.h>

struct database;

struct database *db_create(void);
void db_destroy(struct database *db);

bool db_set(struct database *db, const char *key, const char *value);
const char *db_get(struct database *db, const char *key);
bool db_del(struct database *db, const char *key);

#endif
