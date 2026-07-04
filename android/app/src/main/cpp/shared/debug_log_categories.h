/*
 * debug_log_categories.h -- shared log category IDs for debug logging
 *
 * These integer values are duplicated in DebugLogCategory.kt so the
 * Kotlin UI and C logging code agree on category numbering.
 * Keep both files in sync when adding new categories.
 */
#ifndef DEBUG_LOG_CATEGORIES_H
#define DEBUG_LOG_CATEGORIES_H

#define DLOG_NETWORK     0
#define DLOG_GRAPHICS    1
#define DLOG_TEXTURE     2
#define DLOG_GAME        3
#define DLOG_LAUNCHER    4
#define DLOG_PROFILING   5
#define DLOG_COOP_DESYNC 6
#define DLOG_COUNT       7

#endif /* DEBUG_LOG_CATEGORIES_H */
