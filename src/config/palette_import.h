#pragma once

#include "color_slots.h"
#include <cstring>

constexpr const char* CYD_COLOR_SCHEMA = "cyd-v1";
constexpr const char* UPSTREAM_COLOR_SCHEMA = "upstream-25b1928";

inline bool supportedColorSchema(const char* schema) {
  return !schema || !strcmp(schema, CYD_COLOR_SCHEMA) ||
         !strcmp(schema, UPSTREAM_COLOR_SCHEMA);
}

// Untagged historical backups are CYD palettes. Upstream imports must opt into
// this pinned schema: array length cannot distinguish the two projects safely.
inline int paletteImportSlot(int index, const char* schema) {
  if (index < 0) return -1;
  if (!schema || !strcmp(schema, CYD_COLOR_SCHEMA))
    return index < COL_COUNT ? index : -1;
  if (strcmp(schema, UPSTREAM_COLOR_SCHEMA)) return -1;
  if (index <= COL_SCOPE_PEAK) return index;
  switch (index - (COL_SCOPE_PEAK + 1)) {
    case 0: return COL_DOOM_EMBER;
    case 1: return COL_DOOM_FLAME;
    case 2: return COL_DOOM_CORE;
    case 3: return COL_DIGITS_S17;
    case 4: case 5: return -1; // Upstream external LED strip, absent on CYD
    case 6: return COL_DATE;
    case 7: return COL_WEATHER_DETAIL;
    default: return -1;
  }
}
