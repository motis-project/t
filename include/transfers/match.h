#pragma once

namespace nigiri {
struct timetable;
}

namespace transfers {

struct database;

void match(nigiri::timetable const&, database&);

}  // namespace transfers