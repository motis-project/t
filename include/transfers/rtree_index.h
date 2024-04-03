#pragma once

#include <array>
#include <tuple>

#include "geo/box.h"
#include "geo/latlng.h"

#include "utl/verify.h"

#include "rtree.h"

namespace transfers {

template <typename T>
struct rtree_index {
  rtree_index() : rtree_{rtree_new()} {
    utl::verify(rtree_ != nullptr, "rtree creation failed");
  }

  ~rtree_index() noexcept { rtree_free(rtree_); }

  void add(T const idx, geo::latlng const& pos) {
    auto const min_corner = std::array<double, 2U>{pos.lng_, pos.lat_};
    rtree_insert(rtree_, min_corner.data(), nullptr,
                 reinterpret_cast<void*>(to_idx(idx)));
  }

  void search(geo::latlng const& pos,
              double radius,
              std::basic_string<T>& results) const {
    using udata_t = std::tuple<geo::latlng const* /* search position */,
                               double /* search radius */,
                               std::basic_string<T>* /* results storage */>;

    results.clear();

    auto const b = geo::box{pos, radius};
    auto const min = std::array<double, 2U>{b.min_.lng_, b.min_.lat_};
    auto const max = std::array<double, 2U>{b.max_.lng_, b.max_.lat_};
    auto udata = udata_t{&pos, radius, &results};
    rtree_search(
        rtree_, min.data(), max.data(),
        [](double const* min, double const* /* max */, void const* item,
           void* udata) {
          auto const [p, r, rtree_results_ptr] =
              *reinterpret_cast<udata_t const*>(udata);

          if (geo::distance(*p, {min[1], min[0]}) > r) {
            return true;
          }

          auto const x = T{static_cast<typename T::value_t>(
              reinterpret_cast<std::intptr_t>(item))};

          rtree_results_ptr->push_back(x);

          return true;
        },
        &udata);
  }

  rtree* rtree_;
};

}  // namespace transfers