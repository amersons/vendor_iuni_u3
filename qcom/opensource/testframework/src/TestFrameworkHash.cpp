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
 * Hash table.  The dominant calls are add and lookup, with removals
 * happening very infrequently.  We use probing, and don't worry much
 * about tombstone removal.
 */
#define LOG_TAG "TestFrameworkHash"

#include <stdlib.h>
#include <utils/Log.h>
#include "TestFrameworkHash.h"
#include "TestFramework.h"

/* table load factor, i.e. how full can it get before we resize */
//#define LOAD_NUMER  3       // 75%
//#define LOAD_DENOM  4
#define LOAD_NUMER  5       // 62.5%
#define LOAD_DENOM  8
//#define LOAD_NUMER  1       // 50%
//#define LOAD_DENOM  2

/*
 * Create and initialize a hash table.
 */
TfHashTable::TfHashTable(size_t initialSize, HashFreeFunc freeFunc,
                         HashCompareFunc cmpFunc, HashCompute computeFunc)
{
    pEntries = NULL;
    numEntries = numDeadEntries = 0;
    pthread_mutex_init(&lock, NULL);

    if (initialSize <= 0) {
        return;
    }

    initialSize = TfHashSize(initialSize);
    tableSize = roundUpPower2(initialSize);
    this->freeFunc = freeFunc;
    this->cmpFunc = cmpFunc;
    this->computeFunc = computeFunc;
    pEntries = (HashEntry*) malloc(tableSize * sizeof(HashEntry));
    if (pEntries == NULL) {
        return;
    }

    memset(pEntries, 0, tableSize * sizeof(HashEntry));
}

/*
 * Free the table.
 */
TfHashTable::~TfHashTable()
{
    TfHashTableClear();
    free(pEntries);
}

/*
 * Clear out all entries.
 */
void TfHashTable::TfHashTableClear()
{
    HashEntry* pEnt;
    int i;

    TfHashTableLock();
    pEnt = pEntries;
    for (i = 0; i < tableSize; i++, pEnt++) {
        if (pEnt->data == HASH_TOMBSTONE) {
            // nuke entry
            pEnt->data = NULL;
        } else if (pEnt->data != NULL) {
            // call free func then nuke entry
            if (freeFunc != NULL)
                (*freeFunc)(pEnt->data);
            pEnt->data = NULL;
        }
    }

    numEntries = 0;
    numDeadEntries = 0;
    TfHashTableUnlock();
}

/*
 * Compute the capacity needed for a table to hold "size" elements.
 */
size_t TfHashTable::TfHashSize(size_t size) {
    return (size * LOAD_DENOM) / LOAD_NUMER +1;
}

/*
 * Resize a hash table.  We do this when adding an entry increased the
 * size of the table beyond its comfy limit.
 *
 * This essentially requires re-inserting all elements into the new storage.
 *
 * If multiple threads can access the hash table, the table's lock should
 * have been grabbed before issuing the "lookup+add" call that led to the
 * resize, so we don't have a synchronization problem here.
 */
bool TfHashTable::resizeHash(int newSize)
{
    HashEntry* pNewEntries;
    int i;

    if (countTombStones() != numDeadEntries)
        return false;

    pNewEntries = (HashEntry*) calloc(newSize, sizeof(HashEntry));
    if (pNewEntries == NULL)
        return false;

    for (i = 0; i < tableSize; i++) {
        void* data = pEntries[i].data;
        if (data != NULL && data != HASH_TOMBSTONE) {
            int hashValue = pEntries[i].hashValue;
            int newIdx;

            /* probe for new spot, wrapping around */
            newIdx = hashValue & (newSize-1);
            while (pNewEntries[newIdx].data != NULL)
                newIdx = (newIdx + 1) & (newSize-1);

            pNewEntries[newIdx].hashValue = hashValue;
            pNewEntries[newIdx].data = data;
        }
    }

    free(pEntries);
    pEntries = pNewEntries;
    tableSize = newSize;
    numDeadEntries = 0;

    if(countTombStones() != 0)
        return false;

    return true;
}

/*
 * Look up an entry.
 *
 * We probe on collisions, wrapping around the table.
 */
void* TfHashTable::TfHashTableLookup(void* item, int len, bool doAdd)
{
    HashEntry* pEntry;
    HashEntry* pEnd;
    void* result = NULL;

    if ((tableSize <= 0) || (item == HASH_TOMBSTONE) || (item == NULL))
        return NULL;

    unsigned int itemHash = 0;

    if (computeFunc) {
        itemHash = (*computeFunc)(item);
    }

    TfHashTableLock();
    /* jump to the first entry and probe for a match */
    pEntry = &pEntries[itemHash & (tableSize-1)];
    pEnd = &pEntries[tableSize];
    while (pEntry->data != NULL) {
        if (pEntry->data != HASH_TOMBSTONE &&
            pEntry->hashValue == itemHash &&
            (*cmpFunc)(pEntry->data, item) == 0)
        {
            /* match */
            break;
        }

        pEntry++;
        if (pEntry == pEnd) {     /* wrap around to start */
            if (tableSize == 1)
                break;      /* edge case - single-entry table */
            pEntry = pEntries;
        }
    }

    if (pEntry->data == NULL) {
        if (doAdd) {
            pEntry->hashValue = itemHash;
            pEntry->data = malloc(len);
            if (pEntry->data != NULL) {
                memmove(pEntry->data, item, len);
                numEntries++;
            }

            //We've added an entry.  See if this brings us too close to full.
            if ((numEntries+numDeadEntries) * LOAD_DENOM
                > tableSize * LOAD_NUMER)
            {
                if (!resizeHash(tableSize * 2)) {
                    /* don't really have a way to indicate failure */
                    TF_LOGE("TF hash resize failure\n");
                }
            }

            //make sure table is not bad
            if (numEntries < tableSize)
                result = item;
        }
    } else {
        result = pEntry->data;
    }
    TfHashTableUnlock();

    return result;
}

/*
 * Remove an entry from the table.
 *
 */
bool TfHashTable::TfHashTableRemove(void* item)
{
    HashEntry* pEntry;
    HashEntry* pEnd;

    if (tableSize <= 0)
        return false;

    unsigned int itemHash = 0;

    if (computeFunc) {
        itemHash = (*computeFunc)(item);
    }

    TfHashTableLock();
    /* jump to the first entry and probe for a match */
    pEntry = &pEntries[itemHash & (tableSize-1)];
    pEnd = &pEntries[tableSize];
    while (pEntry->data != NULL) {
        if (pEntry->data == item) {
            if (freeFunc != NULL)
                (*freeFunc)(pEntry->data);
            pEntry->data = HASH_TOMBSTONE;
            numEntries--;
            numDeadEntries++;
            return true;
        }

        pEntry++;
        if (pEntry == pEnd) {     /* wrap around to start */
            if (tableSize == 1)
                break;      /* edge case - single-entry table */
            pEntry = pEntries;
        }
    }
    TfHashTableUnlock();

    return false;
}

/*
 * Round up to the next highest power of 2.
 *
 */
unsigned int TfHashTable::roundUpPower2(unsigned int val)
{
    val--;
    val |= val >> 1;
    val |= val >> 2;
    val |= val >> 4;
    val |= val >> 8;
    val |= val >> 16;
    val++;

    return val;
}

unsigned int TfHashTable::ComputeUtf8Hash(const char* utf8Str)
{
    unsigned int hash = 1;

    while (*utf8Str != '\0')
        hash = hash * 31 + *utf8Str++;

    return hash;
}

/*
 * Count up the number of tombstone entries in the hash table.
 */
int TfHashTable::countTombStones()
{
    int i, count;

    for (count = i = 0; i < tableSize; i++) {
        if (pEntries[i].data == HASH_TOMBSTONE)
            count++;
    }
    return count;
}
