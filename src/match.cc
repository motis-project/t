#include "transfers/match.h"

#include "utl/enumerate.h"

#include "nigiri/timetable.h"

#include "transfers/for_each_number.h"
#include "transfers/rtree_index.h"
#include "transfers/types.h"

namespace n = nigiri;

namespace transfers {

bool has_number_match(std::string_view a, std::string_view b) {
  auto match = false;
  for_each_number(a, [&](unsigned const x) {
    for_each_number(b, [&](unsigned const y) { match = (x == y); });
  });
  return match;
}

template <typename Collection>
bool has_number_match(Collection&& a, std::string_view b) {
  return std::any_of(a.begin(), a.end(),
                     [&](auto&& x) { return has_number_match(x.view(), b); });
}

// Assumption: database is already filled with non-redundant OSM entries
void match(n::timetable const& tt, database& db) {
  std::cout << "{\n"
            << "  \"features\": [\n";

  auto platform_rtree = rtree_index<platform_idx_t>{};
  for (auto const& [pos, platform_idx] : db.osm_to_platform_) {
    platform_rtree.add(platform_idx, {pos.lat(), pos.lon()});
  }

  for (auto const& [idx, x] : utl::enumerate(db.platforms_)) {
    std::string name;
    for (auto const& s : db.platform_names_[platform_idx_t{idx}]) {
      name += std::string{s.view()} + ", ";
    }
    std::cout << fmt::format(
        R"(    {{
      "type": "Feature",
      "properties": {{
        "name": "{}",
        "marker-color": "blue",
        "id": "{}/{}"
      }},
      "geometry": {{
        "coordinates": [ {}, {} ],
        "type": "Point"
      }}
    }},)",
        name, x.type_ == ppr::routing::osm_namespace::NODE ? "node" : "way",
        x.id_, x.pos_.lon(), x.pos_.lat());
  }

  for (auto l = n::location_idx_t{0U}; l != tt.n_locations(); ++l) {
    auto const pos = tt.locations_.coordinates_[l];

    std::cout << fmt::format(R"(    {{
      "type": "Feature",
      "properties": {{
        "name": "{}",
        "marker-color": "green",
        "id": "{}"
      }},
      "geometry": {{
        "coordinates": [ {}, {} ],
        "type": "Point"
      }}
    }},)",
                             tt.locations_.names_[l].view(),
                             tt.locations_.ids_[l].view(), pos.lng_, pos.lat_);
  }

  //  std::cout << "N locations = " << tt.n_locations() << "\n";
  auto number_matches = std::vector<bool>{};
  auto results = std::basic_string<platform_idx_t>{};
  for (auto l = n::location_idx_t{0U}; l != tt.n_locations(); ++l) {
    auto const pos = tt.locations_.coordinates_[l];

    platform_rtree.search(pos, 500, results);

    number_matches.resize(db.platforms_.size());
    for (auto r : results) {
      number_matches[to_idx(r)] = has_number_match(
          db.platform_names_[r], tt.locations_.names_[l].view());
    }

    constexpr auto const kNumberMatchBonus = 200.0;
    utl::sort(results, [&](platform_idx_t const a, platform_idx_t const b) {
      return geo::distance(to_geo(db.platforms_[a].pos_), pos) -
                 (number_matches[to_idx(a)] ? kNumberMatchBonus : 0.0) <
             geo::distance(to_geo(db.platforms_[b].pos_), pos) -
                 (number_matches[to_idx(b)] ? kNumberMatchBonus : 0.0);
    });

    if (results.empty()) {
      continue;
    }

    auto const best = to_geo(db.platforms_[results.front()].pos_);

    std::cout << fmt::format(R"(    {{
      "type": "Feature",
      "properties": {{
        "name": "{}/{} VS {}"
      }},
      "geometry": {{
        "coordinates": [
          [ {}, {} ],
          [ {}, {} ]
        ],
        "type": "LineString"
      }}
    }},)",
                             db.platforms_[results.front()].type_ ==
                                     ppr::routing::osm_namespace::NODE
                                 ? "node"
                                 : "way",
                             db.platforms_[results.front()].id_,
                             tt.locations_.names_[l].view(), pos.lng_, pos.lat_,
                             best.lng_, best.lat_);

    //    std::cout << "MATCHES: size=" << results.size()
    //              << ", tt_name=" << tt.locations_.names_[l].view() <<
    //              "\n";
    //    for (auto const& x : results) {
    //      for (auto const& y : db.platform_names_[x]) {
    //        std::cout << "  " << y.view() << " | ";
    //      }
    //      std::cout << "\n";
    //    }

    //    if (results.empty()) {
    //      // No match found.
    //    }
    //
    //    auto const it =
    //        db.tt_to_platform_.find(to_ppr(tt.locations_.coordinates_[l]));
    //    if (it == end(db.tt_to_platform_)) {
    //      db.tt_to_platform_.emplace();
    //    }
  }

  std::cout << "  ],\n"
            << "  \"type\": \"FeatureCollection\"\n"
            << "}\n";
}

}  // namespace transfers
