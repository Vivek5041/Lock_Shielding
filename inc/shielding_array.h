#ifndef SHIELDING_ARRAY_H
#define SHIELDING_ARRAY_H

#include <cstdio>
#include <utility>
#include <cstdlib>

#define MAX_LOCKS 4
#define MAX_HASH_ENTRIES 10

#define DEBUG_P 0
#if DEBUG_P
    #define DEBUG_PRINT(...) printf(__VA_ARGS__)
#else
    #define DEBUG_PRINT(...) ((void)0)
#endif

// Lock status enum
enum class LS_Status {
    LS_ACQUIRE_NOW = 1,
    LS_SKIP_ACQUISITION,
    LS_UNBALANCED_LOCK,
    LS_RELEASE_NOW,
    LS_SKIP_RELEASE,
    LS_UNBALANCED_UNLOCK,
};

// TLS Entry for array mode
struct LS_LockEntry {
    void* lock_ptr;
    long rec_count;
};

struct LS_LockHashEntry {
    void* lock_ptr;
    int rec_count;
    // UT_hash_handle hh;
    int * dummy[8];
    struct LS_LockHashEntry* next;
    bool dynamically_allocated;
};

LS_LockEntry* lookup(void* l);
void IncrementRef(void* l, LS_LockEntry* entry = nullptr);
int DecrementRef(void* l, LS_LockEntry* entry = nullptr);

template <typename LockType, typename LockFunc, typename... Args>
LS_Status  LS_ACQUIRE(LockType* l, bool reentrant, LockFunc lock_fn, Args&&... args) {
    DEBUG_PRINT("In LS_ACQUIRE\n");
    LS_LockEntry* entry = lookup(l);
    if (!entry) {
        lock_fn(l, std::forward<Args>(args)...);
        IncrementRef(l, entry);
        return LS_Status::LS_ACQUIRE_NOW;
    }
    if (reentrant) {
        IncrementRef(l, entry);
        return LS_Status::LS_SKIP_ACQUISITION;
    }
    fprintf(stderr, "PANIC: LS_ACQUIRE failed due to UNBALANCED_LOCK!\n");
    abort(); /* or exit(EXIT_FAILURE); */
    return LS_Status::LS_UNBALANCED_LOCK;
}

template <typename LockType, typename UnlockFunc, typename... Args>
LS_Status LS_RELEASE(LockType* l, bool reentrant, UnlockFunc unlock_fn, Args&&... args) {
    DEBUG_PRINT("In LS_RELEASE\n");
    LS_LockEntry* entry = lookup(l);
    if (!entry) {
        fprintf(stderr, "PANIC: LS_RELEASE failed due to UNBALANCED_UNLOCK!\n");
        abort(); /* or exit(EXIT_FAILURE); */   
        return LS_Status::LS_UNBALANCED_UNLOCK;
    }
    if (reentrant) {
        int val = DecrementRef(l, entry);
        if (val == 0) {
            unlock_fn(l, std::forward<Args>(args)...);
            return LS_Status::LS_RELEASE_NOW;
        }
        return LS_Status::LS_SKIP_RELEASE;
    }
    unlock_fn(l, std::forward<Args>(args)...);
    DecrementRef(l, entry);
    return LS_Status::LS_RELEASE_NOW;
}


#endif // SHIELDING_ARRAY_H

