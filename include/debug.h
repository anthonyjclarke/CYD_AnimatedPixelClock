#pragma once

#include <Arduino.h>

// =========================
// Debug System
// =========================
/**
 * Leveled debug logging system with runtime control
 *
 * DEBUG LEVELS:
 *   0 = Off      - No debug output
 *   1 = Error    - Critical errors only
 *   2 = Warn     - Warnings + Errors
 *   3 = Info     - General info + Warnings + Errors (default)
 *   4 = Verbose  - All debug output including frequent events
 *
 * USAGE:
 *   DBG_ERROR(fmt, ...)   - Critical errors (level 1+)
 *   DBG_WARN(fmt, ...)    - Warnings (level 2+)
 *   DBG_INFO(fmt, ...)    - General information (level 3+)
 *   DBG_VERBOSE(fmt, ...) - Verbose/frequent output (level 4)
 *
 * RUNTIME CONTROL:
 *   Set debugLevel variable (0-4) to change verbosity at runtime
 *   Web API: GET /api/debug  ->  {"level": N}
 *            POST /api/debug  body: level=N
 *
 * NOTE - differs from the single-file CYD projects on purpose:
 *   `debugLevel` is declared extern here and defined once in main.cpp, rather
 *   than `static` in the header. This project spans ~25 translation units; a
 *   `static` in the header would give each .cpp its own private copy, so a
 *   level set through the web API would silently apply to one file only.
 */

#ifndef DEBUG_LEVEL
#define DEBUG_LEVEL 3  // Default: Info level
#endif

#define DBG_LEVEL_OFF     0
#define DBG_LEVEL_ERROR   1
#define DBG_LEVEL_WARN    2
#define DBG_LEVEL_INFO    3
#define DBG_LEVEL_VERBOSE 4

// Runtime debug level - adjustable via web API. Defined in main.cpp.
extern uint8_t debugLevel;

// clang-format off
#define DBG_ERROR(fmt, ...)   do { if (debugLevel >= DBG_LEVEL_ERROR)   Serial.printf("[ERR ] " fmt "\n", ##__VA_ARGS__); } while(0)
#define DBG_WARN(fmt, ...)    do { if (debugLevel >= DBG_LEVEL_WARN)    Serial.printf("[WARN] " fmt "\n", ##__VA_ARGS__); } while(0)
#define DBG_INFO(fmt, ...)    do { if (debugLevel >= DBG_LEVEL_INFO)    Serial.printf("[INFO] " fmt "\n", ##__VA_ARGS__); } while(0)
#define DBG_VERBOSE(fmt, ...) do { if (debugLevel >= DBG_LEVEL_VERBOSE) Serial.printf("[VERB] " fmt "\n", ##__VA_ARGS__); } while(0)
// clang-format on
