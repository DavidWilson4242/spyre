#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "hash.h"

/* default hash function that is used.  can be overwritten
 * by reassigning to SpyreHash_T->hash */
/* credit: http://www.cse.yorku.ca/~oz/hash.html */
static size_t default_hash(const char *key) {
  size_t h = 5381;
  int c;

  while (c = *key++) {
    h = ((h << 5) + h) + c;
  }

  return h;
}

static size_t condense_hash(SpyreHash_T *table, const char *key) {
  return table->hash(key) % table->capacity;
}

void hash_insert(SpyreHash_T *table, const char *key, void *value) {
  size_t index = condense_hash(table, key);
  SpyreEntry_T *entry = malloc(sizeof(SpyreEntry_T));
  assert(entry);
  entry->value = value;
  entry->key = malloc(strlen(key) + 1);
  assert(entry->key);
  strcpy(entry->key, key);

  if (!table->buckets[index]) {
    entry->next = NULL;
    table->buckets[index] = entry;
  } else {
    entry->next = table->buckets[index];
    table->buckets[index] = entry;
  }

}

void *hash_get(SpyreHash_T *table, const char *key) {
  size_t index = condense_hash(table, key);
  for (SpyreEntry_T *e = table->buckets[index]; e != NULL; e = e->next) {
    if (!strcmp(e->key, key)) {
      return e->value;
    }
  }
  return NULL;
}

SpyreHash_T *hash_init() {
  
  SpyreHash_T *table = malloc(sizeof(SpyreHash_T));
  assert(table);
  table->hash = default_hash;
  table->capacity = HASH_INITIAL_CAPACITY;
  table->buckets = calloc(HASH_INITIAL_CAPACITY, sizeof(SpyreEntry_T *));

  return table;

}
