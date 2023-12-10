#pragma once

#include <cinttypes>

#include "ankerl/cista_adapter.h"

#include "cista/containers/vector.h"
#include "cista/containers/vecvec.h"

#include "ppr/routing/input_location.h"

namespace transfers {

template <typename K, typename V>
using hash_map = cista::offset::ankerl_map<K, V>;

template <typename K, typename V>
using vecvec = cista::offset::vecvec<K, V>;

template <typename K, typename V>
using vector_map = cista::offset::vector_map<K, V>;

using platform_idx_t = cista::strong<std::uint32_t, struct platform_idx_>;

/**
 * Rationale:
 *    - Timetables are updated more often than OSM.
 *    - OSM updates invalidate the cache.
 *    - Timetables do NOT invalidate the cache.
 *
 *  First time (after OSM change):
 *    - Extract all platforms from OSM.
 *    - Map timetable locations to OSM platforms.
 *    - Route one-to-many for each platform to platforms in radius
 *      (using OSM objects as starts/destinations).
 *    - Store routing results to database
 *    - Update nigiri timetable.
 *
 *  Update (after timetable change):
 *    - For each new timetable location:
 *     - Map timetable locations to OSM platforms.
 *     - Route one-to-many for each platform to platforms in radius
 *       (using OSM objects as starts/destinations).
 */
struct database {
  // Location in the timetable (GTFS/HRD/etc.) -> platform index
  hash_map<ppr::location, platform_idx_t> tt_to_platform_;

  vector_map<platform_idx_t, ppr::routing::osm_element> platform_osm_;
  vecvec<platform_idx_t, platform_idx_t>
};

}  // namespace transfers