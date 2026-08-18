#include "hashmap.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

struct count_context {
    size_t count;
};

static void count_entry(
    const char *key,
    const char *value,
    void *context
) {
    (void)key;
    (void)value;

    struct count_context *ctx = context;
    ctx->count++;
}

static void test_create_and_basic_put_get(void) {
    struct hashmap *map = hashmap_create(4);

    assert(map != NULL);
    assert(hashmap_size(map) == 0);

    assert(hashmap_put(map, "name", "Alice"));
    assert(hashmap_size(map) == 1);

    const char *value = hashmap_get(map, "name");

    assert(value != NULL);
    assert(strcmp(value, "Alice") == 0);

    hashmap_destroy(map);
}

static void test_missing_key(void) {
    struct hashmap *map = hashmap_create(4);

    assert(map != NULL);

    assert(hashmap_get(map, "missing") == NULL);

    hashmap_destroy(map);
}

static void test_overwrite_existing_key(void) {
    struct hashmap *map = hashmap_create(4);

    assert(map != NULL);

    assert(hashmap_put(map, "name", "Alice"));
    assert(hashmap_size(map) == 1);

    assert(hashmap_put(map, "name", "Bob"));

    const char *value = hashmap_get(map, "name");

    assert(value != NULL);
    assert(strcmp(value, "Bob") == 0);
    assert(hashmap_size(map) == 1);

    hashmap_destroy(map);
}

static void test_remove_existing_key(void) {
    struct hashmap *map = hashmap_create(4);

    assert(map != NULL);

    assert(hashmap_put(map, "name", "Alice"));
    assert(hashmap_put(map, "city", "Seoul"));

    assert(hashmap_size(map) == 2);

    assert(hashmap_remove(map, "name"));

    assert(hashmap_get(map, "name") == NULL);
    assert(hashmap_size(map) == 1);

    const char *city = hashmap_get(map, "city");

    assert(city != NULL);
    assert(strcmp(city, "Seoul") == 0);

    hashmap_destroy(map);
}

static void test_remove_missing_key(void) {
    struct hashmap *map = hashmap_create(4);

    assert(map != NULL);

    assert(!hashmap_remove(map, "missing"));
    assert(hashmap_size(map) == 0);

    hashmap_destroy(map);
}

static void test_resize_preserves_entries(void) {
    struct hashmap *map = hashmap_create(2);

    assert(map != NULL);

    assert(hashmap_put(map, "a", "1"));
    assert(hashmap_put(map, "b", "2"));
    assert(hashmap_put(map, "c", "3"));
    assert(hashmap_put(map, "d", "4"));
    assert(hashmap_put(map, "e", "5"));

    assert(hashmap_size(map) == 5);

    assert(strcmp(hashmap_get(map, "a"), "1") == 0);
    assert(strcmp(hashmap_get(map, "b"), "2") == 0);
    assert(strcmp(hashmap_get(map, "c"), "3") == 0);
    assert(strcmp(hashmap_get(map, "d"), "4") == 0);
    assert(strcmp(hashmap_get(map, "e"), "5") == 0);

    hashmap_destroy(map);
}

static void test_foreach_visits_every_entry(void) {
    struct hashmap *map = hashmap_create(4);

    assert(map != NULL);

    assert(hashmap_put(map, "name", "Alice"));
    assert(hashmap_put(map, "age", "23"));
    assert(hashmap_put(map, "city", "Seoul"));

    struct count_context context = {
        .count = 0
    };

    hashmap_foreach(
        map,
        count_entry,
        &context
    );

    assert(context.count == 3);
    assert(context.count == hashmap_size(map));

    hashmap_destroy(map);
}

static void test_invalid_create(void) {
    assert(hashmap_create(0) == NULL);
}

int main(void) {
    test_invalid_create();
    test_create_and_basic_put_get();
    test_missing_key();
    test_overwrite_existing_key();
    test_remove_existing_key();
    test_remove_missing_key();
    test_resize_preserves_entries();
    test_foreach_visits_every_entry();

    printf("All hashmap tests passed.\n");

    return 0;
}
