#ifndef DATABASE_H
#define DATABASE_H

struct database;

enum db_result {
    DB_OK,
    DB_NOT_FOUND,
    DB_ERROR,
    DB_FATAL
};

struct database *db_create(void);
void db_destroy(struct database *db);

enum db_result db_set(struct database *db, const char *key, const char *value);
char *db_get(struct database *db, const char *key);
enum db_result db_del(struct database *db, const char *key);

#endif
