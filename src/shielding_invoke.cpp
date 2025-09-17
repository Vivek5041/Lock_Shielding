#include "shielding_invoke.h"

// Thread-local variables for tracking locks
thread_local LS_LockEntry lock_table[MAX_LOCKS];
thread_local int lock_count = 0;

// Note: The hash and freelist variables are defined but not used by the functions below.
thread_local LS_LockHashEntry* lock_hash = NULL;
thread_local LS_LockHashEntry freelist_pool[MAX_HASH_ENTRIES];
thread_local LS_LockHashEntry* freelist_head = NULL;
thread_local bool freelist_initialized = false;

LS_LockEntry* lookup(void* l) {
    // This implementation only uses the array mode.
    if (lock_count <= MAX_LOCKS) {
        for (int i = 0; i < lock_count; ++i) {
            if (lock_table[i].lock_ptr == l)
                return &lock_table[i];
        }
    }
    return nullptr;
}

void IncrementRef(void* l, LS_LockEntry* entry) {
    // LS_LockEntry* entry = lookup(l);
    if (!entry) {
        // Add a new entry if there's space
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
    // LS_LockEntry* entry = lookup(l);
    if (!entry) return -1; // Lock not found

    if (entry->rec_count > 1) {
        entry->rec_count--;
    } else {
        // Last reference, remove the entry from the table.
        // This is done by swapping with the last element for O(1) removal.
        int idx = static_cast<int>(entry - lock_table);
        lock_table[idx] = lock_table[--lock_count];
        return 0; // Return 0 to indicate the lock was fully released
    }
    return entry->rec_count;
};