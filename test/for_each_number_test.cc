#include "gtest/gtest.h"

#include "transfers/for_each_number.h"

using transfers::for_each_number;

void check(std::string_view s, std::initializer_list<unsigned> const numbers) {
  auto it = begin(numbers);
  for_each_number(s, [&](unsigned x) {
    ASSERT_NE(it, end(numbers));
    EXPECT_EQ(x, *it);
    ++it;
  });
}

TEST(transfers, for_each_number) {
  check("123", {123});
  check("123 456", {123, 456});
  check("123 456 789", {123, 456, 789});
  check("_123_456_789_", {123, 456, 789});
}