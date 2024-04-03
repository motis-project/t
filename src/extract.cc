#include "transfers/extract.h"

#include <array>
#include <string_view>

#include "utl/parser/arg_parser.h"
#include "utl/progress_tracker.h"
#include "utl/zip.h"

#include "tiles/osm/hybrid_node_idx.h"
#include "tiles/osm/tmp_file.h"
#include "tiles/util_parallel.h"

#include "osmium/handler.hpp"
#include "osmium/io/file.hpp"
#include "osmium/io/pbf_input.hpp"
#include "osmium/osm/area.hpp"
#include "osmium/osm/node.hpp"
#include "osmium/tags/taglist.hpp"
#include "osmium/tags/tags_filter.hpp"
#include "osmium/visitor.hpp"

namespace osm = osmium;
namespace osm_io = osmium::io;
namespace osm_eb = osmium::osm_entity_bits;
namespace osm_mem = osmium::memory;

namespace transfers {

namespace {

std::int32_t level(osm::OSMObject const& x) {
  auto const level = x.tags()["level"];
  return level == nullptr
             ? 0
             : static_cast<std::int32_t>(utl::parse<float>(level) * 10.0F);
}

ppr::location middle(osm::Box const& b) {
  return to_ppr(geo::midpoint({b.bottom_left().lat(), b.bottom_left().lon()},
                              {b.top_right().lat(), b.top_right().lon()}));
}

template <typename A, typename B>
bool strings_equals(A&& a, B&& b) {
  if (a.size() != b.size()) {
    return false;
  }
  for (auto const [x, y] : utl::zip(a, b)) {
    if (x.view() != y.view()) {
      return false;
    }
  }
  return true;
}

struct handler : public osmium::handler::Handler {
  explicit handler(database& db) : db_{db} {}

  void way(osmium::Way const& w) {
    if (skip(w)) {
      return;
    }
    add(w, platform{.pos_ = middle(w.envelope()),
                    .id_ = w.id(),
                    .level_ = level(w),
                    .type_ = ppr::routing::osm_namespace::WAY});
  }

  void node(osmium::Node const& n) {
    if (skip(n)) {
      return;
    }
    add(n, platform{.pos_ = to_ppr(n.location()),
                    .id_ = n.id(),
                    .level_ = level(n),
                    .type_ = ppr::routing::osm_namespace::NODE});
  }

  void area(osmium::Area const& a) {
    if (skip(a)) {
      return;
    }
    add(a, platform{.pos_ = middle(a.envelope()),
                    .id_ = a.id(),
                    .level_ = level(a),
                    .type_ = ppr::routing::osm_namespace::RELATION});
  }

  void add(osm::OSMObject const& x, platform const& p) {
    auto lock = std::scoped_lock{mutex_};

    strings_.clear();
    for (auto const* s :
         {"name", "description", "ref_name", "local_ref", "ref"}) {
      auto const tag = x.tags()[s];
      if (tag != nullptr) {
        strings_.emplace_back(tag);
      }
    }

    auto const it = db_.osm_to_platform_.find(p.pos_);
    if (it != end(db_.osm_to_platform_) && p == db_.platforms_[it->second] &&
        strings_equals(strings_, db_.platform_names_[it->second])) {
      return;
    }

    if (it != end(db_.osm_to_platform_)) {
      db_.osm_to_platform_.erase(it);
    }

    auto const idx = platform_idx_t{db_.platforms_.size()};
    db_.platforms_.emplace_back(p);
    db_.platform_names_.emplace_back(strings_);
    db_.osm_to_platform_.emplace(p.pos_, idx);
  }

  bool skip(osm::OSMObject const& x) const noexcept {
    return !osm::tags::match_any_of(x.tags(), filter_);
  }

  osm::TagsFilter filter_{
      osmium::TagsFilter{}
          .add_rule(true, "public_transport", "platform")
          .add_rule(true, "public_transport", "stop_position")
          .add_rule(true, "railway", "platform")
          .add_rule(true, "railway", "tram_stop")};
  std::mutex mutex_;
  vecvec<std::uint32_t, char> strings_;
  database& db_;
};

}  // namespace

database extract(std::filesystem::path const& in_path,
                 std::filesystem::path const& tmp_dname) {
  auto input_file = osm_io::File{};
  auto file_size = std::size_t{0U};
  try {
    input_file = osm_io::File{in_path};
    file_size =
        osm_io::Reader{input_file, osmium::io::read_meta::no}.file_size();
  } catch (...) {
    fmt::print("load_osm failed [file={}]\n", in_path);
    throw;
  }

  auto pt = utl::get_active_progress_tracker_or_activate("import");
  pt->status("Load OSM").out_mod(3.F).in_high(2 * file_size);

  auto const node_idx_file =
      tiles::tmp_file{(tmp_dname / "idx.bin").generic_string()};
  auto const node_dat_file =
      tiles::tmp_file{(tmp_dname / "dat.bin").generic_string()};
  auto node_idx =
      tiles::hybrid_node_idx{node_idx_file.fileno(), node_dat_file.fileno()};

  {  // Collect node coordinates.
    pt->status("Load OSM / Pass 1");
    auto node_idx_builder = tiles::hybrid_node_idx_builder{node_idx};

    auto reader = osm_io::Reader{input_file, osm_eb::node | osm_eb::relation,
                                 osmium::io::read_meta::no};
    while (auto buffer = reader.read()) {
      pt->update(reader.offset());
      osm::apply(buffer, node_idx_builder);
    }
    reader.close();

    node_idx_builder.finish();
    std::clog << "Hybrid Node Index Statistics:\n";
    node_idx_builder.dump_stats();
  }

  auto areas_mutex = std::mutex{};
  auto mp_queue = tiles::in_order_queue<osm_mem::Buffer>{};
  auto db = database{};
  {  // Extract streets, places, and areas.
    pt->status("Load OSM / Pass 2");
    auto const thread_count =
        std::max(2, static_cast<int>(std::thread::hardware_concurrency()));

    // pool must be destructed before handlers!
    auto pool = osmium::thread::Pool{thread_count,
                                     static_cast<size_t>(thread_count * 8)};

    auto reader = osm_io::Reader{input_file, pool, osmium::io::read_meta::no};
    auto seq_reader = tiles::sequential_until_finish<osm_mem::Buffer>{[&] {
      pt->update(reader.file_size() + reader.offset());
      return reader.read();
    }};

    auto has_exception = std::atomic_bool{false};
    auto workers = std::vector<std::future<void>>{};
    auto h = handler{db};
    workers.reserve(thread_count / 2);
    for (auto i = 0; i < thread_count / 2; ++i) {
      workers.emplace_back(pool.submit([&] {
        try {
          while (true) {
            auto opt = seq_reader.process();
            if (!opt.has_value()) {
              break;
            }

            auto& [idx, buf] = *opt;
            tiles::update_locations(node_idx, buf);
            osm::apply(buf, h);
          }
        } catch (std::exception const& e) {
          fmt::print(std::clog, "EXCEPTION CAUGHT: {} {}\n",
                     std::this_thread::get_id(), e.what());
          has_exception = true;
        } catch (...) {
          fmt::print(std::clog, "UNKNOWN EXCEPTION CAUGHT: {} \n",
                     std::this_thread::get_id());
          has_exception = true;
        }
      }));
    }

    utl::verify(!workers.empty(), "have no workers");
    for (auto& worker : workers) {
      worker.wait();
    }

    utl::verify(!has_exception, "load_osm: exception caught!");
    utl::verify(mp_queue.queue_.empty(), "mp_queue not empty!");

    reader.close();
    pt->update(pt->in_high_);
  }

  return db;
}

}  // namespace transfers