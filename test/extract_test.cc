#include "gtest/gtest.h"

#include "utl/zip.h"

#include "fmt/core.h"
#include "fmt/ostream.h"

#include "nigiri/loader/dir.h"
#include "nigiri/loader/gtfs/load_timetable.h"
#include "nigiri/timetable.h"

#include "transfers/extract.h"
#include "transfers/match.h"

using namespace date;

constexpr auto const stations = R"(
# stops.txt
"stop_id","stop_code","stop_name","stop_desc","stop_lat","stop_lon","location_type","parent_station","wheelchair_boarding","platform_code","level_id"
"de:06411:4734","","Darmstadt Hauptbahnhof",,"49.871937000000","8.631143000000",1,,0,"",""
"de:06411:4734:41:41","","Darmstadt Hauptbahnhof","Schiene 6","49.873037000000","8.629159000000",0,"de:06411:4734",0,"6","1"
"de:06411:4734:42:42","","Darmstadt Hauptbahnhof","Schiene 10","49.873037000000","8.629159000000",0,"de:06411:4734",0,"10","1"
"de:06411:4734:43:43","","Darmstadt Hauptbahnhof","Schiene 12","49.873037000000","8.629159000000",0,"de:06411:4734",0,"12","1"
"de:06411:4734:44:44","","Darmstadt Hauptbahnhof","Schiene 8","49.873037000000","8.629159000000",0,"de:06411:4734",0,"8","1"
"de:06411:4734:46:46","","Darmstadt Hauptbahnhof","Tram Platz 1","49.872397000000","8.631640000000",0,,0,"1 Ost","2"
"de:06411:4734:47:47","","Darmstadt Hauptbahnhof","Bus Platz 2","49.872865000000","8.631553000000",0,,0,"2 Ost","2"
"de:06411:4734:48:48","","Darmstadt Hauptbahnhof","Tram Platz 3","49.872792000000","8.631401000000",0,,0,"3 Ost","2"
"de:06411:4734:49:49","","Darmstadt Hauptbahnhof","Bus Platz 4","49.872298000000","8.631474000000",0,,0,"4 Ost","2"
"de:06411:4734:50:51","","Darmstadt Hauptbahnhof","Bus Platz 11","49.871957000000","8.631546000000",0,,0,"11 Ost","2"
"de:06411:4734:50:52","","Darmstadt Hauptbahnhof","Bus Platz 12","49.871696000000","8.631715000000",0,,0,"12 Ost","2"
"de:06411:4734:51:53","","Darmstadt Hauptbahnhof","Bus Platz 13","49.871508000000","8.631884000000",0,,0,"13 Ost","2"
"de:06411:4734:51:54","","Darmstadt Hauptbahnhof","Bus Platz 14","49.871347000000","8.632024000000",0,,0,"14 Ost","2"
"de:06411:4734:52:55","","Darmstadt Hauptbahnhof","Bus Platz 15","49.871283000000","8.631858000000",0,,0,"15 Ost","2"
"de:06411:4734:52:56","","Darmstadt Hauptbahnhof","Bus Platz 16","49.871201000000","8.631524000000",0,,0,"16 Ost","2"
"de:06411:4734:53:57","","Darmstadt Hauptbahnhof","Bus Platz 17","49.871381000000","8.631384000000",0,,0,"17 Ost","2"
"de:06411:4734:50:59","","Darmstadt Hauptbahnhof","Bus Platz 19","49.871758000000","8.631339000000",0,,0,"19 Ost","2"
"de:06411:4734:50:60","","Darmstadt Hauptbahnhof","Bus Platz 20","49.871938000000","8.631296000000",0,,0,"20 Ost","2"
"de:06411:4734:62:62","","Darmstadt Hauptbahnhof","Gleis 1","49.873037000000","8.629159000000",0,"de:06411:4734",0,"1","1"
"de:06411:4734:64:64","","Darmstadt Hauptbahnhof","Schiene 4","49.873037000000","8.629159000000",0,"de:06411:4734",0,"4","1"
"de:06411:4734:57:67","","Darmstadt Hauptbahnhof","Bus Platz 21","49.872333000000","8.628288000000",0,,0,"21 West","2"
"de:06411:4734:58:68","","Darmstadt Hauptbahnhof","Bus Platz 22","49.872359000000","8.628121000000",0,,0,"22 West","2"
"de:06411:4734:57","","Darmstadt Hauptbahnhof",,"49.872333000000","8.628288000000",0,,0,"",""
"de:06411:4734:64:63","","Darmstadt Hauptbahnhof","Schiene 3","49.873037000000","8.629159000000",0,"de:06411:4734",0,"3","1"
"de:06411:4734:41:65","","Darmstadt Hauptbahnhof","Schiene 5","49.873037000000","8.629159000000",0,"de:06411:4734",0,"5","1"
"de:06411:4734:44:66","","Darmstadt Hauptbahnhof","Schiene 7","49.873037000000","8.629159000000",0,"de:06411:4734",0,"7","1"
"de:06411:4734:42:69","","Darmstadt Hauptbahnhof","Schiene 9","49.873037000000","8.629159000000",0,"de:06411:4734",0,"9","1"
"de:06411:4734:43:70","","Darmstadt Hauptbahnhof","Schiene 11","49.873037000000","8.629159000000",0,"de:06411:4734",0,"11","1"
"000010473462","","Darmstadt Hauptbahnhof",,"49.871937000000","8.631143000000",0,,0,"",""
"000010473463","","Darmstadt Hauptbahnhof",,"49.871937000000","8.631143000000",0,,0,"",""
"de:06411:4734_G","","Darmstadt Hauptbahnhof",,"49.872516000000","8.629379000000",0,,0,"",""
"000320473402","","Darmstadt Hauptbahnhof","Zugang E","49.871748000000","8.630839000000",2,"de:06411:4734",0,"","2"
"000320473405","","Darmstadt Hauptbahnhof","Zugang A","49.872818000000","8.630970000000",2,"de:06411:4734",0,"","2"
"000320473406","","Darmstadt Hauptbahnhof","Zugang B","49.872242000000","8.630877000000",2,"de:06411:4734",0,"","2"
"000320473407","","Darmstadt Hauptbahnhof","Zugang C","49.871685000000","8.628140000000",2,"de:06411:4734",0,"","2"
"000320473427","","Darmstadt Hauptbahnhof","Zugang D","49.871511000000","8.627168000000",2,"de:06411:4734",0,"","2"
"000320473471","","Darmstadt Hauptbahnhof","Zugang F","49.871289000000","8.630800000000",2,"de:06411:4734",0,"","2"
"000320473446","","Darmstadt Hauptbahnhof","Tram Platz 1","49.872397000000","8.631640000000",2,"de:06411:4734",0,"1 Ost","2"
"000320473447","","Darmstadt Hauptbahnhof","Bus Platz 2","49.872865000000","8.631553000000",2,"de:06411:4734",0,"2 Ost","2"
"000320473448","","Darmstadt Hauptbahnhof","Tram Platz 3","49.872792000000","8.631401000000",2,"de:06411:4734",0,"3 Ost","2"
"000320473449","","Darmstadt Hauptbahnhof","Bus Platz 4","49.872298000000","8.631474000000",2,"de:06411:4734",0,"4 Ost","2"
"000320473451","","Darmstadt Hauptbahnhof","Bus Platz 11","49.871957000000","8.631546000000",2,"de:06411:4734",0,"11 Ost","2"
"000320473452","","Darmstadt Hauptbahnhof","Bus Platz 12","49.871696000000","8.631715000000",2,"de:06411:4734",0,"12 Ost","2"
"000320473453","","Darmstadt Hauptbahnhof","Bus Platz 13","49.871508000000","8.631884000000",2,"de:06411:4734",0,"13 Ost","2"
"000320473454","","Darmstadt Hauptbahnhof","Bus Platz 14","49.871347000000","8.632024000000",2,"de:06411:4734",0,"14 Ost","2"
"000320473455","","Darmstadt Hauptbahnhof","Bus Platz 15","49.871283000000","8.631858000000",2,"de:06411:4734",0,"15 Ost","2"
"000320473456","","Darmstadt Hauptbahnhof","Bus Platz 16","49.871201000000","8.631524000000",2,"de:06411:4734",0,"16 Ost","2"
"000320473457","","Darmstadt Hauptbahnhof","Bus Platz 17","49.871381000000","8.631384000000",2,"de:06411:4734",0,"17 Ost","2"
"000320473459","","Darmstadt Hauptbahnhof","Bus Platz 19","49.871758000000","8.631339000000",2,"de:06411:4734",0,"19 Ost","2"
"000320473460","","Darmstadt Hauptbahnhof","Bus Platz 20","49.871938000000","8.631296000000",2,"de:06411:4734",0,"20 Ost","2"
"000320473467","","Darmstadt Hauptbahnhof","Bus Platz 21","49.872333000000","8.628288000000",2,"de:06411:4734",0,"21 West","2"
"000320473468","","Darmstadt Hauptbahnhof","Bus Platz 22","49.872359000000","8.628121000000",2,"de:06411:4734",0,"22 West","2"

)";

TEST(transfers, extract) {
  auto tt = nigiri::timetable{};
  tt.date_range_ = {2023_y / December / 20, 2023_y / December / 21};
  nigiri::loader::gtfs::load_timetable(
      {}, nigiri::source_idx_t{0}, nigiri::loader::mem_dir::read(stations), tt);

  auto db = transfers::extract("test/da_hbf.osm.pbf", "/tmp");
  transfers::match(tt, db);

  //  for (auto const& [x, names] : utl::zip(db.platforms_, db.platform_names_))
  //  {
  //    fmt::print("id={}:{}, pos={}, level={}, names: ", x.type_, x.id_,
  //    x.pos_,
  //               x.level_ / 10.0);
  //    for (auto const& n : names) {
  //      fmt::print("{} ", n.view());
  //    }
  //    fmt::print("\n");
  //  }
}