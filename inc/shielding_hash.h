#ifndef SHIELDING_HASH_H
#define SHIELDING_HASH_H

#include <stdbool.h>
#include "uthash.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_LOCKS 4
#define MAX_HASH_ENTRIES 10

#define DEBUG_P 0
#if DEBUG_P
    #define DEBUG_PRINT(...) printf(__VA_ARGS__)
#else
    #define DEBUG_PRINT(...) ((void)0)
#endif

typedef enum {
    LS_ACQUIRE_NOW,
    LS_SKIP_ACQUISITION,
    LS_RELEASE_NOW,
    LS_SKIP_RELEASE,
    LS_UNBALANCED_LOCK,
    LS_UNBALANCED_UNLOCK
} LS_Status;

typedef struct {
    void* lock_ptr;
    int rec_count;
} LS_LockEntry;

typedef struct LS_LockHashEntry {
    void* lock_ptr;
    int rec_count;
    UT_hash_handle hh;
    struct LS_LockHashEntry* next;
    bool dynamically_allocated;
} LS_LockHashEntry;

extern __thread LS_LockEntry lock_table[MAX_LOCKS];
extern __thread int lock_count;
extern __thread LS_LockHashEntry* lock_hash;
extern __thread LS_LockHashEntry freelist_pool[MAX_HASH_ENTRIES];
extern __thread LS_LockHashEntry* freelist_head;
extern __thread bool freelist_initialized;

void init_freelist();
LS_LockHashEntry* freelist_pop();
void freelist_push(LS_LockHashEntry* entry);
LS_LockEntry* lookup(void* l);
void IncrementRef(void* l);
int DecrementRef(void* l);

#ifdef __cplusplus
} // extern "C"
#endif

// C++-only: template-based wrappers
#ifdef __cplusplus
#include <utility>

template <typename LockFunc, typename... Args>
inline LS_Status LS_ACQUIRE(void* l, bool reentrant, LockFunc lock_fn, Args&&... args) {
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
    return LS_Status::LS_UNBALANCED_LOCK;
}

template <typename UnlockFunc, typename... Args>
inline LS_Status LS_RELEASE(void* l, bool reentrant, UnlockFunc unlock_fn, Args&&... args) {
    DEBUG_PRINT("In LS_RELEASE\n");
    LS_LockEntry* entry = lookup(l);
    if (!entry) {
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
#endif // __cplusplus

#endif // SHIELDING_HASH_H
