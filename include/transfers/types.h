#pragma once

#include <cinttypes>

#include "osmium/osm/location.hpp"

#include "ankerl/cista_adapter.h"

#include "geo/latlng.h"

#include "cista/containers/vector.h"
#include "cista/containers/vecvec.h"

#include "ppr/routing/input_location.h"

namespace transfers {

inline ppr::location to_ppr(geo::latlng const& l) noexcept {
  return ppr::make_location(l.lng_, l.lat_);
}

inline ppr::location to_ppr(osmium::Location const& l) noexcept {
  return ppr::make_location(l.lon(), l.lat());
}

inline geo::latlng to_geo(ppr::location const& l) noexcept {
  return geo::latlng{l.lat(), l.lon()};
}

template <typename K, typename V>
using hash_map = cista::offset::ankerl_map<K, V>;

template <typename K, typename V>
using vecvec = cista::offset::vecvec<K, V>;

template <typename K, typename V>
using vector_map = cista::offset::vector_map<K, V>;

template <typename K, typename V, std::size_t N>
using nvec = cista::offset::nvec<K, V, N>;

using platform_idx_t = cista::strong<std::uint32_t, struct platform_idx_>;

struct platform {
  CISTA_FRIEND_COMPARABLE(platform)
  ppr::location pos_;
  std::int64_t id_;
  std::int32_t level_;  // stored as level * 10
  ppr::routing::osm_namespace type_;
};

struct database {
  // Location in the timetable (GTFS/HRD/etc.) -> platform index
  hash_map<ppr::location, platform_idx_t> tt_to_platform_;

  // Location in OSM -> platform index
  hash_map<ppr::location, platform_idx_t> osm_to_platform_;

  vector_map<platform_idx_t, platform> platforms_;
  nvec<platform_idx_t, char, 2> platform_names_;
};

}  // namespace transfers