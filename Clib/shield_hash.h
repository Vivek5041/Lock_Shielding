#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>
#include <stdbool.h>
#include "uthash.h"   // You must have uthash.h in the project

#define MAX_LOCKS 4
#define MAX_HASH_ENTRIES 20 

#define DEBUG_P 0
#if DEBUG_P
    #define DEBUG_PRINT(...) printf(__VA_ARGS__)
#else
    #define DEBUG_PRINT(...) ((void)0)
#endif

// Lock status
typedef enum {
    LS_ACQUIRE_NOW,
    LS_SKIP_ACQUISITION,
    LS_RELEASE_NOW,
    LS_SKIP_RELEASE,
    LS_UNBALANCED_LOCK,
    LS_UNBALANCED_UNLOCK
} LS_Status;

// Structure for each array entry
typedef struct {
    void* lock_ptr;
    int rec_count;
} LS_LockEntry;

// Structure for dynamic hash entry
typedef struct LS_LockHashEntry{
    void* lock_ptr;      // Key: lock address
    int rec_count;
    UT_hash_handle hh;   // Makes it hashable
    struct LS_LockHashEntry* next;
    bool dynamically_allocated;  // New flag for malloc
} LS_LockHashEntry;

// Thread-Local Storage
__thread LS_LockEntry lock_table[MAX_LOCKS];
__thread int lock_count = 0;
__thread LS_LockHashEntry* lock_hash = NULL;  // Only created when needed

__thread LS_LockHashEntry freelist_pool[MAX_HASH_ENTRIES];
__thread LS_LockHashEntry* freelist_head = NULL;
__thread bool freelist_initialized = false;

void init_freelist() {
    if (freelist_initialized) return;  // To prevent double initialization

    // Initialize all entries properly
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

inline LS_LockHashEntry* freelist_pop() {
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

    // Fallback to malloc allocation
    entry = (LS_LockHashEntry*)malloc(sizeof(LS_LockHashEntry));
    if (entry) {
        memset(entry, 0, sizeof(LS_LockHashEntry));  // Zero initialize
        entry->dynamically_allocated = true;
        entry->next = NULL;
    }
    return entry;
}

void freelist_push(LS_LockHashEntry* entry) {
    if (!entry) return;  // Guard against dangling pointer

    if (entry->dynamically_allocated) {
        free(entry);  //Free malloc'd entry
        return;
    }
    entry->lock_ptr = NULL;
    entry->rec_count = 0;
    entry->next = freelist_head;
    freelist_head = entry;
}


// --- TLS Lookup ---
inline LS_LockEntry* lookup(void* l) {
    DEBUG_PRINT("In lookup\n");
    if (lock_count <= MAX_LOCKS) {
        for (int i = 0; i < lock_count; i++) {
            if (lock_table[i].lock_ptr == l)
                return &lock_table[i];
        }
        return NULL;
    } else {
        LS_LockHashEntry* entry;
        HASH_FIND_PTR(lock_hash, &l, entry);
        return (LS_LockEntry*)entry;
    }
}

// --- TLS Increment ---
inline void IncrementRef(void* l) {
    DEBUG_PRINT("In IncrementRef\n");
    LS_LockEntry* entry = lookup(l);
    if (!entry) {
        if (lock_count < MAX_LOCKS ) {
            lock_table[lock_count].lock_ptr = l;
            lock_table[lock_count].rec_count = 1;
            lock_count++;
            if (lock_count == MAX_LOCKS) {
                // Convert array to hash
		init_freelist();
                for (int i = 0; i < MAX_LOCKS; i++) {
                    //LS_LockHashEntry* new_entry = (LS_LockHashEntry*)malloc(sizeof(LS_LockHashEntry));
                    LS_LockHashEntry* new_entry = freelist_pop();
                    new_entry->lock_ptr = lock_table[i].lock_ptr;
                    new_entry->rec_count = lock_table[i].rec_count;
                    HASH_ADD_PTR(lock_hash, lock_ptr, new_entry);
                }
		lock_count++;  // Set to MAX_LOCKS + 1 to indicate hash mode
            }
        } else {
            //LS_LockHashEntry* new_entry = (LS_LockHashEntry*)malloc(sizeof(LS_LockHashEntry));
            LS_LockHashEntry* new_entry = freelist_pop();
            if (!new_entry) return;
            new_entry->lock_ptr = l;
            new_entry->rec_count = 1;
            HASH_ADD_PTR(lock_hash, lock_ptr, new_entry);
        }
    } else {
        entry->rec_count++;
    }
}

// --- TLS Decrement ---
inline int DecrementRef(void* l) {
    DEBUG_PRINT("In DecrementRef\n");
    LS_LockEntry* entry = lookup(l);
    if (!entry) return -1;

    if (lock_count <= MAX_LOCKS) {  // We are in array mode
        if (entry->rec_count > 1) {
            entry->rec_count--;
        } else {
            int idx = entry - lock_table;  //Safe only in array mode
            lock_table[idx] = lock_table[--lock_count];
        }
        return entry->rec_count;
    } else {
        LS_LockHashEntry* hash_entry = (LS_LockHashEntry*) entry;
        if (hash_entry->rec_count > 1) {
            hash_entry->rec_count--;
	    return hash_entry->rec_count;
        } else {
            int ret = hash_entry->rec_count - 1;
            HASH_DEL(lock_hash, hash_entry);  //Add key as lock_ptr
            //free(hash_entry);
            //int ret = hash_entry->rec_count;
	    freelist_push(hash_entry);
	    return ret;
        }
    }
}

// Typedef for locking/unlocking function pointer
typedef int (*LockFunc2)(void* l, void* me);
typedef void (*UnlockFunc2)(void* l, void* me);
typedef int (*LockFunc1)(void* l);
typedef void (*UnlockFunc1)(void* l);

// --- Shielding LS Layer ---

LS_Status LS_ACQUIRE1(void* l , bool reentrant, LockFunc1 __lock_fn) {
    DEBUG_PRINT("In LS_ACQ_ENT\n");

    LS_LockEntry* entry = lookup(l);
    if (!entry) {
        __lock_fn(l);
        IncrementRef(l);
        return LS_ACQUIRE_NOW;
    }
    if (reentrant){
        IncrementRef(l);
        return LS_SKIP_ACQUISITION;
    }
    return LS_UNBALANCED_LOCK;
}

LS_Status LS_ACQUIRE2(void* l, void* me, bool reentrant, LockFunc2 __lock_fn) {
    DEBUG_PRINT("In LS_ACQ_ENT\n");

    LS_LockEntry* entry = lookup(l);
    if (!entry) {
        __lock_fn(l, me);
        IncrementRef(l);
        return LS_ACQUIRE_NOW;
    }
    if (reentrant){
        IncrementRef(l);
        return LS_SKIP_ACQUISITION;
    }
    return LS_UNBALANCED_LOCK;
}


LS_Status LS_RELEASE1(void* l,  bool reentrant, UnlockFunc1  __unlock_fn) {
    DEBUG_PRINT("In LS_REL_ENT\n");

    LS_LockEntry* entry = lookup(l);
    if (!entry) {
        return LS_UNBALANCED_UNLOCK;
    }
    if (reentrant) {
        int val = DecrementRef(l);
        if(val == 0) {
            __unlock_fn(l);
            return LS_RELEASE_NOW;
        }
        return LS_SKIP_RELEASE;
    }
    __unlock_fn(l);
    DecrementRef(l);
    return LS_RELEASE_NOW; 
}


LS_Status LS_RELEASE2(void* l, void* me, bool reentrant, UnlockFunc2  __unlock_fn) {
    DEBUG_PRINT("In LS_REL_ENT\n");

    LS_LockEntry* entry = lookup(l);
    if (!entry) {
        return LS_UNBALANCED_UNLOCK;
    }
    if (reentrant) {
        int val = DecrementRef(l);
        if(val == 0) {
            __unlock_fn(l,me);
            return LS_RELEASE_NOW;
        }
        return LS_SKIP_RELEASE;
    }
    __unlock_fn(l, me);
    DecrementRef(l);
    return LS_RELEASE_NOW;
}

