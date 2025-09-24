#ifndef SANITY_CHECK_H
#define SANITY_CHECK_H

// Sanity check to ensure proper shielding headers are included
// and define the appropriate version flags

#if defined(MCS_BASELINE) || defined(CLH_BASELINE) || defined(TICKET_BASELINE) || defined(ABQL_BASELINE) || \
    defined(TAS_BASELINE) || defined(K42_BASELINE) || defined(HEM_BASELINE) || defined(CNA_BASELINE)
    #define SHIELD_VERSION_BASELINE
    // No shielding required for baseline version

#elif defined(MCS_LS_ARRAY) || defined(CLH_LS_ARRAY) || defined(TICKET_LS_ARRAY) || defined(ABQL_LS_ARRAY) || \
      defined(TAS_LS_ARRAY) || defined(K42_LS_ARRAY) || defined(HEM_LS_ARRAY) || defined(CNA_LS_ARRAY)
    #define SHIELD_VERSION_LS_ARRAY
    #ifndef SHIELDING_ARRAY_H
        #include "shielding_array.h"
    #endif

#elif defined(MCS_LS_HYBRID) || defined(CLH_LS_HYBRID) || defined(TICKET_LS_HYBRID) || defined(ABQL_LS_HYBRID) || \
      defined(TAS_LS_HYBRID) || defined(K42_LS_HYBRID) || defined(HEM_LS_HYBRID) || defined(CNA_LS_HYBRID)
    #define SHIELD_VERSION_LS_HYBRID
    #ifndef SHIELDING_HASH_H
        #include "shielding_hash.h"
    #endif

#elif defined(MCS_LS_INVOKE) || defined(CLH_LS_INVOKE) || defined(TICKET_LS_INVOKE) || defined(ABQL_LS_INVOKE) || \
      defined(TAS_LS_INVOKE) || defined(K42_LS_INVOKE) || defined(HEM_LS_INVOKE) || defined(CNA_LS_INVOKE)
    #define SHIELD_VERSION_LS_INVOKE
    #ifndef SHIELDING_INVOKE_H
        #include "shielding_invoke.h"
    #endif

#else
    // Default to baseline (no shielding)
    #define SHIELD_VERSION_BASELINE
    #pragma message("No specific lock version defined. Defaulting to baseline (no shielding).")
#endif

// Verification that appropriate headers are included
#ifdef SHIELD_VERSION_LS_ARRAY
    #ifndef SHIELDING_ARRAY_H
        #error "LS_ARRAY locks require shielding_array.h"
    #endif
#endif

#ifdef SHIELD_VERSION_LS_HYBRID
    #ifndef SHIELDING_HASH_H
        #error "LS_HYBRID locks require shielding_hash.h"
    #endif
#endif

#ifdef SHIELD_VERSION_LS_INVOKE
    #ifndef SHIELDING_INVOKE_H
        #error "LS_INVOKE locks require shielding_invoke.h"
    #endif
#endif

// Diagnostic information (can be disabled with -DSILENT_SANITY_CHECK)
#ifndef SILENT_SANITY_CHECK
    #ifdef SHIELD_VERSION_BASELINE
        #pragma message("Using Shield Version: Baseline (no shielding)")
    #elif defined(SHIELD_VERSION_LS_ARRAY)
        #pragma message("Using Shield Version: LS_Array (array-based shielding)")
    #elif defined(SHIELD_VERSION_LS_HYBRID)
        #pragma message("Using Shield Version: LS_Hybrid (hash-based shielding)")
    #elif defined(SHIELD_VERSION_LS_INVOKE)
        #pragma message("Using Shield Version: LS_Invoke (invoke-based shielding)")
    #endif
#endif

#endif // SANITY_CHECK_H