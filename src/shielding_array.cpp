#include "shielding_array.h"

thread_local LS_LockEntry lock_table[MAX_LOCKS];
thread_local int lock_count = 0;
thread_local LS_LockHashEntry* lock_hash = NULL;
thread_local LS_LockHashEntry freelist_pool[MAX_HASH_ENTRIES];
thread_local LS_LockHashEntry* freelist_head = NULL;
thread_local bool freelist_initialized = false;

LS_LockEntry* lookup(void* l) {
    if (lock_count <= MAX_LOCKS) {
        for (int i = 0; i < lock_count; ++i) {
            if (lock_table[i].lock_ptr == l)
                return &lock_table[i];
        }
    }
    return nullptr;
}

void IncrementRef(void* l, LS_LockEntry* entry) {
    if (!entry) {
        entry = lookup(l);
    }
    if (!entry) {
        if (lock_count < MAX_LOCKS) {
            lock_table[lock_count].lock_ptr = l;
            lock_table[lock_count].rec_count = 1;
            ++lock_count;
        }
    } else {
        entry->rec_count++;
    }
}

int DecrementRef(void* l, LS_LockEntry* entry) {
    if (!entry) {
        entry = lookup(l);
    }
    if (!entry) return -1;

    if (entry->rec_count > 1) {
        entry->rec_count--;
    } else {
        int idx = static_cast<int>(entry - lock_table);
        lock_table[idx] = lock_table[--lock_count];
        return 0;
    }
    return entry->rec_count;
}
