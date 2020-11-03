#ifndef DEBUG_FLAGS_H
#define DEBUG_FLAGS_H

/* the --debug flag prints out a debug message
 * whenever debug() is called.
 *
 * by default this is only for a small number of special events
 *
 * this file has #defines for compile-time directives for enabling/disabling
 * debugging info for other runtime events too
 *
 * e.g. for info about allocations, and synchronization
 * that are too verbose for usual --debug output.
 */

/**
 * if -DENABLE_DEBUG_BWAITS=1 then --debug will
 * print out BWAIT() calls
 */
#ifndef ENABLE_DEBUG_BWAITS
   #define ENABLE_DEBUG_BWAITS 0
#endif

/**
 * if -DENABLE_DEBUG_ALLOCS=1 then --debug
 * will print out ALLOC_MANY() and ALLOC_ONE() calls
 *
 * this is enabled by default
 */
#ifndef ENABLE_DEBUG_ALLOCS
   #define ENABLE_DEBUG_ALLOCS 1
#endif

/**
 * if -DENABLE_DEBUG_CONCRETIZATION=1 then --debug
 * will output information during concretization process
 * for debugging errors during concretization
 */
#ifndef ENABLE_DEBUG_CONCRETIZATION
   #define ENABLE_DEBUG_CONCRETIZATION 0
#endif

/**
 * if -DENABLE_DEBUG_LOCKS=1 then --debug
 * will output information when taking a lock
 */
#ifndef ENABLE_DEBUG_LOCKS
   #define ENABLE_DEBUG_LOCKS 0
#endif

/**
 * if -DENABLE_DEBUG_DISABLE_WFE=1 then nop wfe/sev
 */
#ifndef ENABLE_DEBUG_DISABLE_WFE
   #define ENABLE_DEBUG_DISABLE_WFE 0
#endif

/**
 * if -DENABLE_DEBUG_PTABLE_SET_RANGE=1 then
 * pagetable range set functions will
 * dump more information
 */
#ifndef ENABLE_DEBUG_PTABLE_SET_RANGE
   #define ENABLE_DEBUG_PTABLE_SET_RANGE 0
#endif

#endif /* DEBUG_FLAGS_H */