#ifndef SHIELDING_ARRAY_H
#define SHIELDING_ARRAY_H

#include <cstdio>
#include <utility>

#define MAX_LOCKS 8

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

extern thread_local LS_LockEntry lock_table[MAX_LOCKS];
extern thread_local int lock_count;

LS_LockEntry* lookup(void* l);
void IncrementRef(void* l);
int DecrementRef(void* l);

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

#endif // SHIELDING_ARRAY_H

