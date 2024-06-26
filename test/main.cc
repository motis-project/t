#include <filesystem>

#include "gtest/gtest.h"

#include "utl/progress_tracker.h"

#include "test_dir.h"

namespace fs = std::filesystem;

int main(int argc, char** argv) {
  std::clog.rdbuf(std::cout.rdbuf());

  auto const progress_tracker = utl::activate_progress_tracker("test");
  auto const silencer = utl::global_progress_bars{true};
  fs::current_path(NIGIRI_TEST_EXECUTION_DIR);

  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}