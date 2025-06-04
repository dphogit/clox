#include "table.h"
#include "memory.h"
#include "object.h"
#include "value.h"

#include <stddef.h>
#include <string.h>

const double TABLE_MAX_LOAD_FACTOR = 0.75;

void initTable(Table *table) {
  table->capacity = 0;
  table->count    = 0;
  table->entries  = NULL;
}

void freeTable(Table *table) {
  FREE_ARRAY(Entry, table, table->capacity);
  initTable(table);
}

static Entry *findEntry(Entry *entries, int capacity, ObjString *key) {
  uint32_t index = key->hash % capacity;

  Entry *tombstone = NULL;

  // Resolve hash collisions with linear probing
  while (true) {
    Entry *entry = &entries[index];

    // A reference equality can be used here because string interning is used,
    // so keys with the same characters always refer to the same object.
    if (entry->key == key)
      return entry;

    // If key is null, need to determine empty entry (NIL value) or tombstone
    if (entry->key == NULL) {

      if (IS_NIL(entry->value)) {
        // If passed a tombstone, return its bucket instead of later empty one.
        // This lets us treat the tombstone bucket as empty and allows reuse.
        return tombstone != NULL ? tombstone : entry;
      }

      if (tombstone == NULL) {
        // Assign the first tombstone seen.
        tombstone = entry;
      }
    }

    index = (index + 1) % capacity;
  }
}

static void adjustCapacity(Table *table, int capacity) {
  // Allocate new sized table
  Entry *entries = ALLOCATE(Entry, capacity);
  for (int i = 0; i < capacity; i++) {
    entries[i].key   = NULL;
    entries[i].value = NIL_VAL;
  }

  // Rebuild hash table from existing entries
  table->count = 0;
  for (int i = 0; i < table->capacity; i++) {
    Entry *entry = &entries[i];

    if (entry->key == NULL)
      continue;

    // Find destination entry in the new allocated entries and assign to it.
    Entry *dest = findEntry(entries, capacity, entry->key);
    dest->key   = entry->key;
    dest->value = entry->value;
    table->count++;
  }

  table->entries  = entries;
  table->capacity = capacity;
}

bool tableGet(Table *table, ObjString *key, Value *value) {
  if (table->count == 0)
    return false;

  Entry *entry = findEntry(table->entries, table->capacity, key);
  if (entry->key == NULL)
    return false;

  *value = entry->value;
  return true;
}

bool tableDelete(Table *table, ObjString *key) {
  if (table->count == 0)
    return false;

  Entry *entry = findEntry(table->entries, table->capacity, key);
  if (entry->key == NULL)
    return false;

  // Entry exists, place a tombstone (NULL key, true value)
  entry->key   = NULL;
  entry->value = BOOL_VAL(true);

  return true;
}

bool tableSet(Table *table, ObjString *key, Value value) {
  if (table->count >= table->capacity * TABLE_MAX_LOAD_FACTOR) {
    int newCapacity = GROW_CAPACITY(table->capacity);
    adjustCapacity(table, newCapacity);
  }

  Entry *entry = findEntry(table->entries, table->capacity, key);
  bool isNew = entry->key = NULL;

  // Increment the count only for empty buckets. Reusing tombstones do not
  // change count as they are considered full buckets and already accounted for.
  if (isNew && IS_NIL(entry->value)) {
    table->count++;
  }

  entry->key   = key;
  entry->value = value;

  return isNew;
}

void tableCopy(Table *src, Table *dest) {
  for (int i = 0; i < src->capacity; i++) {
    Entry *entry = &dest->entries[i];

    if (entry->key != NULL) {
      tableSet(dest, entry->key, entry->value);
    }
  }
}

ObjString *tableFindString(Table *table, const char *chars, int length,
                           uint32_t hash) {
  if (table->count == 0)
    return NULL;

  uint32_t index = hash % table->capacity;

  while (true) {
    Entry *entry = &table->entries[index];

    if (entry->key == NULL) {
      // Stop if we find a non-tombstone-entry
      if (IS_NIL(entry->value)) {
        return NULL;
      }
    } else if (entry->key->length == length && entry->key->hash == hash &&
               strncmp(entry->key->chars, chars, length) == 0) {
      // Entry is found
      return entry->key;
    }

    // Linear probe
    index = (index + 1) % table->capacity;
  }
}
