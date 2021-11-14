#include <stdio.h>
#include <string.h>

#include "value.h"
#include "object.h"
#include "memory.h"

// TODO: Tune these.
// The initial (and minimum) capacity of a non-empty list or map object.
#define TABLE_MIN_CAPACITY 16

// The rate at which a collection's capacity grows when the size exceeds the
// current capacity. The new capacity will be determined by *multiplying* the
// old capacity by this. Growing geometrically is necessary to ensure that
// adding to a collection has O(1) amortized complexity.
#define TABLE_GROW_FACTOR 2

// The maximum percentage of map entries that can be filled before the map is
// grown. A lower load takes more memory but reduces collisions which makes
// lookup faster.
#define TABLE_LOAD_PERCENT 75

DEFINE_BUFFER(Byte, uint8_t);
DEFINE_BUFFER(Int, int);
DEFINE_BUFFER(Value, Value);

void printValue(MochiVM* vm, Value value) {
    if (IS_OBJ(value)) {
        printObject(vm, value);
    } else {
#if MOCHIVM_NAN_TAGGING
        printf("%f", AS_DOUBLE(value));
#elif MOCHIVM_POINTER_TAGGING
        printf("%ju", value);
#else
        printf("%ju", value.as.u64);
#endif
    }
}

Table* mochiNewTable(MochiVM* vm)
{
    Table* table = ALLOCATE(vm, Table);
    table->capacity = 0;
    table->count = 0;
    table->entries = NULL;
    return table;
}

static inline uint32_t hashBits(uint64_t hash)
{
    // From v8's ComputeLongHash() which in turn cites:
    // Thomas Wang, Integer Hash Functions.
    // http://www.concentric.net/~Ttwang/tech/inthash.htm
    hash = ~hash + (hash << 18);  // hash = (hash << 18) - hash - 1;
    hash = hash ^ (hash >> 31);
    hash = hash * 21;  // hash = (hash + (hash << 2)) + (hash << 4);
    hash = hash ^ (hash >> 11);
    hash = hash + (hash << 6);
    hash = hash ^ (hash >> 22);
    return (uint32_t)(hash & 0x3fffffff);
}

// Looks for an entry with [key] in an array of [capacity] [entries].
//
// If found, sets [result] to point to it and returns `true`. Otherwise,
// returns `false` and points [result] to the entry where the key/value pair
// should be inserted.
static bool findEntry(TableEntry* entries, uint32_t capacity, TableKey key, TableEntry** result)
{
    // If there is no entry array (an empty map), we definitely won't find it.
    if (capacity == 0) { return false; }

    // Figure out where to insert it in the table. Use open addressing and
    // basic linear probing.
    uint32_t startIndex = hashBits(key) % capacity;
    uint32_t index = startIndex;

    // If we pass a tombstone and don't end up finding the key, its entry will
    // be re-used for the insert.
    TableEntry* tombstone = NULL;

    // Walk the probe sequence until we've tried every slot.
    do {
        TableEntry* entry = &entries[index];

        // 0 or 1 signifies an empty slot.
        if (entry->key < TABLE_KEY_RANGE_START) {
            // If we found an empty slot, the key is not in the table. If we found a
            // slot that contains a deleted key, we have to keep looking.
            if (entry->key == TABLE_KEY_UNUSED) {
                // We found an empty non-deleted slot, so we've reached the end of the probe
                // sequence without finding the key.
                *result = tombstone != NULL ? tombstone : entry;
                return false;
            } else {
                // We found a tombstone. We need to keep looking in case the key is after it.
                if (tombstone == NULL) { tombstone = entry; }
            }
        } else if (entry->key == key) {
            // We found the key.
            *result = entry;
            return true;
        }

        // Try the next slot.
        index = (index + 1) % capacity;
    }
    while (index != startIndex);

    // If we get here, the table is full of tombstones. Return the first one we found.
    ASSERT(tombstone != NULL, "Map should have tombstones or empty entries.");
    *result = tombstone;
    return false;
}

// Inserts [key] and [value] in the array of [entries] with the given
// [capacity].
//
// Returns `true` if this is the first time [key] was added to the map.
static bool insertEntry(TableEntry* entries, uint32_t capacity, TableKey key, Value value)
{
    ASSERT(entries != NULL, "Should ensure capacity before inserting.");
    
    TableEntry* entry;
    if (findEntry(entries, capacity, key, &entry)) {
        // Already present, so just replace the value.
        entry->value = value;
        return false;
    } else {
        entry->key = key;
        entry->value = value;
        return true;
    }
}

// Updates [table]'s entry array to [capacity].
static void resizeTable(MochiVM* vm, Table* table, uint32_t capacity)
{
    // Create the new empty hash table.
    TableEntry* entries = ALLOCATE_ARRAY(vm, TableEntry, capacity);
    for (uint32_t i = 0; i < capacity; i++) {
        entries[i].key = TABLE_KEY_UNUSED;
        entries[i].value = FALSE_VAL;
    }

    // Re-add the existing entries.
    if (table->capacity > 0) {
        for (uint32_t i = 0; i < table->capacity; i++) {
            TableEntry* entry = &table->entries[i];
            
            // Don't copy empty entries or tombstones.
            if (entry->key < TABLE_KEY_RANGE_START) { continue; }

            insertEntry(entries, capacity, entry->key, entry->value);
        }
    }

    // Replace the array.
    DEALLOCATE(vm, table->entries);
    table->entries = entries;
    table->capacity = capacity;
}

bool mochiTableGet(Table* table, TableKey key, Value* out)
{
    TableEntry* entry;
    bool found = findEntry(table->entries, table->capacity, key, &entry);
    ASSERT(out != NULL, "Cannot pass null pointer for Value out in mochiTableGet.");
    *out = found ? entry->value : FALSE_VAL;
    return found;
}

void mochiTableSet(MochiVM* vm, Table* table, TableKey key, Value value)
{
    // If the map is getting too full, make room first.
    if (table->count + 1 > table->capacity * TABLE_LOAD_PERCENT / 100) {
        // Figure out the new hash table size.
        uint32_t capacity = table->capacity * TABLE_GROW_FACTOR;
        if (capacity < TABLE_MIN_CAPACITY) { capacity = TABLE_MIN_CAPACITY; }

        resizeTable(vm, table, capacity);
    }

    if (insertEntry(table->entries, table->capacity, key, value)) {
        // A new key was added.
        table->count++;
    }
}

void mochiTableClear(MochiVM* vm, Table* table)
{
    DEALLOCATE(vm, table->entries);
    table->entries = NULL;
    table->capacity = 0;
    table->count = 0;
}

bool mochiTableTryRemove(MochiVM* vm, Table* table, TableKey key)
{
    TableEntry* entry;
    if (!findEntry(table->entries, table->capacity, key, &entry)) { return false; }

    // Remove the entry from the table. Set this key to 1, which marks it as a
    // deleted slot. When searching for a key, we will stop on empty slots, but
    // continue past deleted slots.
    entry->key = TABLE_KEY_TOMBSTONE;
    entry->value = FALSE_VAL;

    table->count--;

    if (table->count == TABLE_KEY_UNUSED) {
        // Removed the last item, so free the array.
        mochiTableClear(vm, table);
    }
    else if (table->capacity > TABLE_MIN_CAPACITY &&
            table->count < table->capacity / TABLE_GROW_FACTOR * TABLE_LOAD_PERCENT / 100) {
        uint32_t capacity = table->capacity / TABLE_GROW_FACTOR;
        if (capacity < TABLE_MIN_CAPACITY) { capacity = TABLE_MIN_CAPACITY; }

        // The table is getting empty, so shrink the entry array back down.
        // TODO: Should we do this less aggressively than we grow?
        resizeTable(vm, table, capacity);
    }

    return true;
}