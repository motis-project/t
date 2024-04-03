#pragma once

#include <filesystem>

#include "transfers/types.h"

namespace transfers {

database extract(std::filesystem::path const& in_path,
                 std::filesystem::path const& tmp_path);

}  // namespace transfers