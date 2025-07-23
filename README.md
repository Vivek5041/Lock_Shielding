# Shield

**LockShielding** is a tool for making locks resilient to common misuse in concurrent programming environments.

---

## Library Components

- **`libshielding_array.so`**:  
  Handles **UNBALANCED-LOCK** and **UNBALANCED-UNLOCK** misuse using array implementation along with **lock reeentracy**.

- **`libshielding_hash.so`**:  
  Handles unbalanced lock/unlock protection with hybrid approach.
  
- **`C version`**:  
  C library files are provided in **[`Clib`](Clib/)** directory.

---

## Lock Versions

- **`{LOCK_ALGO}1`**:  
  Simple, **unmodified** lock implementation (baseline).

- **`{LOCK_ALGO}3`**:  
  Modified lock implementation that detects and handles **UNBALANCED-LOCK** and **UNBALANCED-UNLOCK** misuse using array.

- **`{LOCK_ALGO}4`**:  
  Modified lock implementation that detects and handles **UNBALANCED-LOCK** and **UNBALANCED-UNLOCK** misuse using hybrid approach.

---
