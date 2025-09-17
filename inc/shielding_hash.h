#ifndef SHIELDING_HASH_H
#define SHIELDING_HASH_H

#include <utility>
#include <stdbool.h>
#include <cstdio>
#include <cstdlib>
#include "uthash.h"

#define MAX_LOCKS 4
#define MAX_HASH_ENTRIES 10

#define DEBUG_P 0
#if DEBUG_P
    #define DEBUG_PRINT(...) printf(__VA_ARGS__)
#else
    #define DEBUG_PRINT(...) ((void)0)
#endif

#undef HASH_FUNCTION
#define HASH_FUNCTION(keyptr, keylen, hashv) \
    do { \
        uintptr_t k = (uintptr_t)(*(keyptr)); \
        hashv = (unsigned int)(k >> 4) ^ (unsigned int)(k); \
    } while (0)


// Lock status enum
enum class LS_Status{
    LS_ACQUIRE_NOW = 1,
    LS_SKIP_ACQUISITION,
    LS_UNBALANCED_LOCK,
    LS_RELEASE_NOW,
    LS_SKIP_RELEASE,
    LS_UNBALANCED_UNLOCK,
} ;

struct LS_LockEntry {
    void* lock_ptr;
    int rec_count;
} ;

struct LS_LockHashEntry {
    void* lock_ptr;
    int rec_count;
    UT_hash_handle hh;
    struct LS_LockHashEntry* next;
    bool dynamically_allocated;
};

void init_freelist();
LS_LockHashEntry* freelist_pop();
void freelist_push(LS_LockHashEntry* entry);
LS_LockEntry* lookup(void* l);
void IncrementRef(void* l);
int DecrementRef(void* l);


template <typename LockFunc, typename... Args>
LS_Status  LS_ACQUIRE(void* l, bool reentrant, LockFunc lock_fn, Args&&... args) {
    DEBUG_PRINT("In LS_ACQUIRE\n");
    LS_LockEntry* entry = lookup(l);
    if (!entry) {
        lock_fn(l, std::forward<Args>(args)...);
        IncrementRef(l);
        return LS_Status::LS_ACQUIRE_NOW;
    }
    if (reentrant) {
        IncrementRef(l);
        return LS_Status::LS_SKIP_ACQUISITION;
    }
    fprintf(stderr, "PANIC: LS_ACQUIRE failed due to UNBALANCED_LOCK!\n");
    abort(); /* or exit(EXIT_FAILURE); */
    return LS_Status::LS_UNBALANCED_LOCK;
}

template <typename UnlockFunc, typename... Args>
LS_Status LS_RELEASE(void* l, bool reentrant, UnlockFunc unlock_fn, Args&&... args) {
    DEBUG_PRINT("In LS_RELEASE\n");
    LS_LockEntry* entry = lookup(l);
    if (!entry) {
        fprintf(stderr, "PANIC: LS_RELEASE failed due to UNBALANCED_UNLOCK!\n");
        abort(); /* or exit(EXIT_FAILURE); */   
        return LS_Status::LS_UNBALANCED_UNLOCK;
    }
    if (reentrant) {
        int val = DecrementRef(l);
        if (val == 0) {
            unlock_fn(l, std::forward<Args>(args)...);
            return LS_Status::LS_RELEASE_NOW;
        }
        return LS_Status::LS_SKIP_RELEASE;
    }
    unlock_fn(l, std::forward<Args>(args)...);
    DecrementRef(l);
    return LS_Status::LS_RELEASE_NOW;
}

#endif // SHIELDING_HASH_H
