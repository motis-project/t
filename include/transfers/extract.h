#pragma once

#include <filesystem>

#include "transfers/types.h"

namespace transfers {

database parse(std::filesystem::path const&);

}  // namespace transfers