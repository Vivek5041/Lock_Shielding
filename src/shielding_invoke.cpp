#include "shielding_invoke.h"

// Thread-local variables for tracking locks
thread_local LS_LockEntry lock_table[MAX_LOCKS];
thread_local int lock_count = 0;


thread_local LS_LockHashEntry* lock_hash = NULL;
thread_local LS_LockHashEntry freelist_pool[MAX_HASH_ENTRIES];
thread_local LS_LockHashEntry* freelist_head = NULL;
thread_local bool freelist_initialized = false;

void init_freelist() {
    if (freelist_initialized) return;
    memset(freelist_pool, 0, sizeof(freelist_pool));

    for (int i = 0; i < MAX_HASH_ENTRIES - 1; i++) {
        freelist_pool[i].next = &freelist_pool[i + 1];
        freelist_pool[i].dynamically_allocated = false;
    }
    freelist_pool[MAX_HASH_ENTRIES - 1].next = NULL;
    freelist_pool[MAX_HASH_ENTRIES - 1].dynamically_allocated = false;
    freelist_head = &freelist_pool[0];
    freelist_initialized = true;
}

LS_LockHashEntry* freelist_pop() {
    if (!freelist_initialized) {
        init_freelist();
    }
    LS_LockHashEntry* entry;
    if (freelist_head) {
        entry = freelist_head;
        freelist_head = freelist_head->next;
        entry->dynamically_allocated = false;
        return entry;
    }
    entry = (LS_LockHashEntry*)malloc(sizeof(LS_LockHashEntry));
    if (entry) {
        memset(entry, 0, sizeof(LS_LockHashEntry));
        entry->dynamically_allocated = true;
        entry->next = NULL;
    }
    return entry;
}

void freelist_push(LS_LockHashEntry* entry) {
    if (!entry) return;
    if (entry->dynamically_allocated) {
        free(entry);
        return;
    }
    entry->lock_ptr = NULL;
    entry->rec_count = 0;
    entry->next = freelist_head;
    freelist_head = entry;
}


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

    if (lock_count <= MAX_LOCKS) {
        int val = entry->rec_count;
        if (val > 1) {
            entry->rec_count--;
            return val - 1;
        } else {
            int idx = static_cast<int> (entry - lock_table);
            lock_table[idx] = lock_table[--lock_count];
            return 0;
        }
        
    } else {
        LS_LockHashEntry* hash_entry = (LS_LockHashEntry*) entry;
        int val = hash_entry->rec_count;
        if (val > 1) {
            hash_entry->rec_count--;
            return val - 1;
        } else {
            HASH_DEL(lock_hash, hash_entry);
            freelist_push(hash_entry);
            return 0;
        }
    }
};