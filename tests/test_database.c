#include "database.h"

#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

static void assert_value(
    struct database *db,
    const char *key,
    const char *expected
) {
    char *value = db_get(db, key);

    assert(value != NULL);
    assert(strcmp(value, expected) == 0);

    free(value);
}

static void assert_missing(
    struct database *db,
    const char *key
) {
    char *value = db_get(db, key);

    assert(value == NULL);
}

static void test_basic_set_get_del(void) {
    struct database *db = db_create();

    assert(db != NULL);

    assert(db_set(db, "name", "Alice") == DB_OK);
    assert_value(db, "name", "Alice");

    assert(db_del(db, "name") == DB_OK);
    assert_missing(db, "name");

    assert(db_del(db, "name") == DB_NOT_FOUND);

    db_destroy(db);
}

static void test_overwrite(void) {
    struct database *db = db_create();

    assert(db != NULL);

    assert(db_set(db, "age", "20") == DB_OK);
    assert_value(db, "age", "20");

    assert(db_set(db, "age", "21") == DB_OK);
    assert_value(db, "age", "21");

    db_destroy(db);
}

static void test_persistence_after_restart(void) {
    struct database *db = db_create();

    assert(db != NULL);

    assert(db_set(db, "name", "Alice") == DB_OK);
    assert(db_set(db, "city", "Seoul") == DB_OK);

    db_destroy(db);

    db = db_create();

    assert(db != NULL);

    assert_value(db, "name", "Alice");
    assert_value(db, "city", "Seoul");

    db_destroy(db);
}

static void test_delete_persists_after_restart(void) {
    struct database *db = db_create();

    assert(db != NULL);

    assert(db_set(db, "language", "C") == DB_OK);
    assert_value(db, "language", "C");

    assert(db_del(db, "language") == DB_OK);

    db_destroy(db);

    db = db_create();

    assert(db != NULL);
    assert_missing(db, "language");

    db_destroy(db);
}

static void test_overwrite_persists_after_restart(void) {
    struct database *db = db_create();

    assert(db != NULL);

    assert(db_set(db, "version", "1") == DB_OK);
    assert(db_set(db, "version", "2") == DB_OK);
    assert(db_set(db, "version", "3") == DB_OK);

    db_destroy(db);

    db = db_create();

    assert(db != NULL);
    assert_value(db, "version", "3");

    db_destroy(db);
}

static void test_torn_aof_recovery(void) {
    struct database *db = db_create();

    assert(db != NULL);

    assert(db_set(db, "name", "Alice") == DB_OK);
    assert(db_set(db, "city", "Seoul") == DB_OK);

    db_destroy(db);

    int fd = open(
        "cachecore.aof",
        O_WRONLY | O_APPEND
    );

    assert(fd != -1);

    const char *partial = "SET broken";

    ssize_t written = write(fd, partial, strlen(partial));

    assert(written == (ssize_t)strlen(partial));

    close(fd);

    db = db_create();

    assert(db != NULL);
    
    assert_value(db, "name", "Alice");
    assert_value(db, "city", "Seoul");

    db_destroy(db);

    fd = open("cachecore.aof", O_RDONLY);

    assert(fd != -1);

    char buffer[256];

    ssize_t bytes_read = read(
        fd,
        buffer,
        sizeof(buffer) - 1
    );

    assert(bytes_read >= 0);

    buffer[bytes_read] = '\0';

    close(fd);

    assert(strstr(buffer, "SET broken") == NULL);

    assert(strstr(buffer, "SET name Alice\n") != NULL);
    assert(strstr(buffer, "SET city Seoul\n") != NULL);
}

static void test_automatic_aof_compaction(void) {
    struct database *db = db_create();

    assert(db != NULL);

    char value[32];

    for (int i = 0; i < 1100; i++) {
        snprintf(
            value,
            sizeof(value),
            "%d",
            i
        );

        assert(db_set(db, "counter", value) == DB_OK);
    }

    assert_value(db, "counter", "1099");

    db_destroy(db);

    int fd = open("cachecore.aof", O_RDONLY);

    assert(fd != -1);
    
    char buffer[4096];

    ssize_t bytes_read = read(
        fd,
        buffer,
        sizeof(buffer) - 1
    );

    assert(bytes_read >= 0);

    buffer[bytes_read] = '\0';

    close(fd);

    assert(strstr(buffer, "SET counter 1099\n") != NULL);

    assert(strstr(buffer, "SET counter 0\n") == NULL);
    assert(strstr(buffer, "SET counter 500\n") == NULL);

    db = db_create();

    assert(db != NULL);
    assert_value(db, "counter", "1099");

    db_destroy(db);
}

int main(void) {
    char original_directory[PATH_MAX];

    assert(getcwd(
        original_directory,
        sizeof(original_directory)
    ) != NULL);

    char temp_directory[] = "/tmp/cachecore-test-XXXXXX";

    assert(mkdtemp(temp_directory) != NULL);
    assert(chdir(temp_directory) == 0);

    test_basic_set_get_del();

    assert(unlink("cachecore.aof") == 0);

    test_overwrite();
    assert(unlink("cachecore.aof") == 0);

    test_persistence_after_restart();
    assert(unlink("cachecore.aof") == 0);

    test_delete_persists_after_restart();
    assert(unlink("cachecore.aof") == 0);

    test_overwrite_persists_after_restart();
    assert(unlink("cachecore.aof") == 0);

    test_torn_aof_recovery();
    assert(unlink("cachecore.aof") == 0);

    test_automatic_aof_compaction();
    assert(unlink("cachecore.aof") == 0);

    assert(chdir(original_directory) == 0);
    assert(rmdir(temp_directory) == 0);

    printf("All database integration tests passed.\n");

    return 0;
}
