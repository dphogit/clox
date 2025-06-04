#ifndef CLOX_TABLE_H
#define CLOX_TABLE_H

#include "value.h"
#include <stdint.h>

// An associated key-value pair, representing an entry to a table.
typedef struct entry {
  ObjString *key;
  Value value;
} Entry;

typedef struct table {
  int capacity;
  int count; // Number of entries plus tombstones (which count as full buckets)
  Entry *entries;
} Table;

void initTable(Table *table);
void freeTable(Table *table);

/*
 * Retrieves an entry for the given key.
 *
 * If the entry with the key exists, it returns true as well as assigning the
 * value pointer output parameter. Otherwise, returns false.
 */
bool tableGet(Table *table, ObjString *key, Value *value);

/*
 * Deletes an entry from the table, replacing the entry's value with a special
 * entry known as a "tombstone", represented with a `NULL` key and `true` value.
 *
 * Returns true if an existing entry was deleted, otherwise false.
 */
bool tableDelete(Table *table, ObjString *key);

/*
 * Adds the given key/value pair to the given hash table, overriding the value
 * if there is an already existing entry.
 *
 * Returns true if a new entry was added, otherwise returns false.
 */
bool tableSet(Table *table, ObjString *key, Value value);

void tableCopy(Table *src, Table *dest);
ObjString *tableFindString(Table *table, const char *chars, int length,
                           uint32_t hash);

#endif
