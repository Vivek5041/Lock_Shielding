#ifndef LOCK_TYPE_H
#define LOCK_TYPE_H

// MCS Lock variants
#if defined(MCS_BASELINE) || defined(MCS_LS_ARRAY) || defined(MCS_LS_HYBRID) || defined(MCS_LS_INVOKE)
    #include "mcs.h"
    typedef MCSLock LockType;

// CLH Lock variants
#elif defined(CLH_BASELINE) || defined(CLH_LS_ARRAY) || defined(CLH_LS_HYBRID) || defined(CLH_LS_INVOKE)
    #include "clh.h"
    typedef CLHLock LockType;

// Ticket Lock variants
#elif defined(TICKET_BASELINE) || defined(TICKET_LS_ARRAY) || defined(TICKET_LS_HYBRID) || defined(TICKET_LS_INVOKE)
    #include "ticket.h"
    typedef TicketLock LockType;

// TAS Lock variants
#elif defined(TAS_BASELINE) || defined(TAS_LS_ARRAY) || defined(TAS_LS_HYBRID) || defined(TAS_LS_INVOKE)
    #include "tas.h"
    typedef TASLock LockType;

// ABQL Lock variants
#elif defined(ABQL_BASELINE) || defined(ABQL_LS_ARRAY) || defined(ABQL_LS_HYBRID) || defined(ABQL_LS_INVOKE)
    #include "abql.h"
    typedef ABQLLock LockType;

// K42 Lock variants
#elif defined(K42_BASELINE) || defined(K42_LS_ARRAY) || defined(K42_LS_HYBRID) || defined(K42_LS_INVOKE)
    #include "k42.h"
    typedef K42Lock LockType;

// HEM Lock variants
#elif defined(HEM_BASELINE) || defined(HEM_LS_ARRAY) || defined(HEM_LS_HYBRID) || defined(HEM_LS_INVOKE)
    #include "hem.h"
    typedef HemLock LockType;

// CNA Lock variants
#elif defined(CNA_BASELINE) || defined(CNA_LS_ARRAY) || defined(CNA_LS_HYBRID) || defined(CNA_LS_INVOKE)
    #include "cna.h"
    typedef CNALock LockType;

// ---------- Default fallback ----------
#else
    #pragma message("No lock type defined. Defaulting to TASLock.")
    #include "tas.h"
    typedef TASLock LockType;

#endif

#endif // LOCK_TYPE_H
