/*
 * Copyright (C) 2012 The Android Open Source Project
 * Copyright (c) 2012 The Linux Foundation. All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
/*
 * General purpose hash table, used for finding classes, methods, etc.
 *
 * When the number of elements reaches a certain percentage of the table's
 * capacity, the table will be resized.
 */
#ifndef _TESTFRAMEWORK_HASH
#define _TESTFRAMEWORK_HASH

#include <pthread.h>

#define HASH_TOMBSTONE ((void*) 0xcbcacccd)     // invalid ptr value

/* compute the hash of an item with a specific type */
typedef unsigned int (*HashCompute)(const void* item);

/*
 * Compare a hash entry with a "loose" item after their hash values match.
 * Returns { <0, 0, >0 } depending on ordering of items (same semantics
 * as strcmp()).
 */
typedef int (*HashCompareFunc)(const void* tableItem, const void* looseItem);

/*
 * This function will be used to free entries in the table.  This can be
 * NULL if no free is required, free(), or a custom function.
 */
typedef void (*HashFreeFunc)(void* ptr);

/*
 * Expandable hash table.
 *
 * This structure should be considered opaque.
 */
class TfHashTable {
public:
    /*
     * Create and initialize a HashTable structure, using "initialSize" as
     * a basis for the initial capacity of the table.  (The actual initial
     * table size may be adjusted upward.)  If you know exactly how many
     * elements the table will hold, pass the result from TfHashSize() in.)
     *
     * Returns "false" if unable to allocate the table.
     */
    TfHashTable(size_t initialSize, HashFreeFunc freeFunc,
                HashCompareFunc cmpFunc, HashCompute computeFunc);

    /*
     * Free a hash table.  Performs a "clear" first.
     */
    ~TfHashTable();

    /*
     * Compute the capacity needed for a table to hold "size" elements.  Use
     * this when you know ahead of time how many elements the table will hold.
     * Pass this value into TfHashTableCreate() to ensure that you can add
     * all elements without needing to reallocate the table.
     */
    size_t TfHashSize(size_t size);

    /*
     * Clear out a hash table, freeing the contents of any used entries.
     */
    void TfHashTableClear();

    /*
     * Look up an entry in the table, possibly adding it if it's not there.
     *
     * If "item" is not found, and "doAdd" is false, NULL is returned.
     * Otherwise, a pointer to the found or added item is returned.  (You can
     * tell the difference by seeing if return value == item.)
     *
     * An "add" operation may cause the entire table to be reallocated.  Don't
     * forget to lock the table before calling this.
     */
    void* TfHashTableLookup(void* item, int len = -1, bool doAdd = false);

    /*
     * Remove an item from the hash table, given its "data" pointer.  Does not
     * invoke the "free" function; just detaches it from the table.
     */
    bool TfHashTableRemove(void* item);

    static unsigned int ComputeUtf8Hash(const char* str);

private:
   /*
     * Exclusive access.  Important when adding items to a table, or when
     * doing any operations on a table that could be added to by another thread.
     */
    void TfHashTableLock() {
        pthread_mutex_lock(&lock);
    }
    void TfHashTableUnlock() {
        pthread_mutex_unlock(&lock);
    }
    bool resizeHash(int newSize);
    unsigned int roundUpPower2(unsigned int val);
    int countTombStones();
private:
  /*
   * One entry in the hash table.  "data" values are expected to be (or have
   * the same characteristics as) valid pointers.  In particular, a NULL
   * value for "data" indicates an empty slot, and HASH_TOMBSTONE indicates
   * a no-longer-used slot that must be stepped over during probing.
   *
   * Attempting to add a NULL or tombstone value is an error.
   *
   * When an entry is released, we will call (HashFreeFunc)(entry->data).
   */
  struct HashEntry {
      unsigned int hashValue;
      void* data;
  };

private:
    int tableSize;          /* must be power of 2 */
    int numEntries;         /* current #of "live" entries */
    int numDeadEntries;     /* current #of tombstone entries */
    HashEntry* pEntries;           /* array on heap */
    HashFreeFunc freeFunc;
    HashCompareFunc cmpFunc;
    HashCompute computeFunc;
    pthread_mutex_t lock;
};


#endif /*_TESTFRAMEWORK_HASH*/
