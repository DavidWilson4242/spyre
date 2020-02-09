#ifndef HASH_H
#define HASH_H

#include <stdlib.h>

#define HASH_INITIAL_CAPACITY 16

typedef struct SpyreEntry {
  char *key;
  void *value;
  struct SpyreEntry *next;
} SpyreEntry_T;

typedef struct SpyreHash {
  SpyreEntry_T **buckets; 
  size_t capacity;
  size_t size;
  size_t (*hash)(const char *);
} SpyreHash_T;

SpyreHash_T *hash_init();
void hash_free(SpyreHash_T **);
void hash_insert(SpyreHash_T *, const char *key, void *value);
void *hash_remove(SpyreHash_T *, const char *key);
void *hash_get(SpyreHash_T *, const char *key);
void hash_foreach(SpyreHash_T *, void (*)(const char *, void *, void *), void *);

#endif
