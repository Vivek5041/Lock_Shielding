#ifndef LOCK_TYPE_H
#define LOCK_TYPE_H

#if defined(MCS1)
    #include "mcs1.h"
    typedef MCSLock LockType;

#elif defined(MCS3)
    #include "mcs3.h"
    typedef MCSLock LockType;

#elif defined(MCS4)
    #include "mcs4.h"
    typedef MCSLock LockType;    

#elif defined(CLH1)
    #include "clh1.h"
    typedef CLHLock LockType;

#elif defined(CLH3)
    #include "clh3.h"
    typedef CLHLock LockType;    

#elif defined(CLH4)
    #include "clh4.h"
    typedef CLHLock LockType;

#elif defined(TICKET1)
    #include "ticket1.h"
    typedef TicketLock LockType;

#elif defined(TICKET3)
    #include "ticket3.h"
    typedef TicketLock LockType;

#elif defined(TICKET4)
    #include "ticket4.h"
    typedef TicketLock LockType;    

#elif defined(ABQL1)
    #include "abql1.h"
    typedef ABQLLock LockType;

#elif defined(ABQL3)
    #include "abql3.h"
    typedef ABQLLock LockType;

#elif defined(ABQL4)
    #include "abql4.h"
    typedef ABQLLock LockType;

#elif defined(TAS1)
    #include "tas1.h"
    typedef TASLock LockType;

#elif defined(TAS3)
    #include "tas3.h"
    typedef TASLock LockType;
    
#elif defined(TAS4)
    #include "tas4.h"
    typedef TASLock LockType;
    
        
// ---------- Default fallback ----------
#else
    #pragma message("No lock type defined. Defaulting to TASLock.")
    #include "tas1.h"
    typedef TASLock LockType;

#endif

#endif // LOCK_TYPE_H
