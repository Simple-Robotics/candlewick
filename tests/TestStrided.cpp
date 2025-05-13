#include "candlewick/utils/StridedView.h"
#include <gtest/gtest.h>
#include <vector>
#include <numeric>

using candlewick::strided_view;

GTEST_TEST(TestStridedView, c_array) {
  using T = std::tuple<uint, float>;
  T data[5] = {{0, 0.1f}, {10, 2.2f}, {0, 0.3f}, {12, -0.3f}, {0, -13.4f}};

  strided_view<T> view(data, 2 * sizeof(T));
  EXPECT_EQ(view.size(), sizeof(data) / sizeof(T));
  EXPECT_EQ(view.stride_bytes(), 2 * sizeof(T));
  EXPECT_EQ(view.max_index(), 3);
  EXPECT_FALSE(view.empty());
}

GTEST_TEST(TestStridedView, vector_int) {
  std::vector<int> data;
  data.resize(11);
  std::iota(data.begin(), data.end(), 0);

  auto stride = 2 * sizeof(int);
  auto view = strided_view(data.data(), data.size(), stride);
  EXPECT_EQ(view.size(), data.size());
  EXPECT_EQ(view.stride_bytes(), stride);
  EXPECT_EQ(view.max_index(), 6);
  EXPECT_FALSE(view.empty());

  EXPECT_EQ(view.front(), 0);
  EXPECT_EQ(view[1], 2);
  EXPECT_EQ(view[2], 4);
  EXPECT_EQ(view.at(3), 6);
  EXPECT_EQ(view.at(4), 8);
  EXPECT_EQ(view.at(5), 10);
  EXPECT_THROW((void)view.at(6), std::out_of_range);
}

struct test_data {
  int a;
  double b;
};

bool operator==(const test_data &x, const test_data &y) {
  return x.a == y.a && x.b == y.b;
}

GTEST_TEST(TestStridedView, span) {
  std::vector<test_data> data;
  data.resize(11);

  for (uint i = 0; i < 11; i++) {
    data[i].a = int(i);
    data[i].b = 3.f * float(i);
  }

  std::span<test_data> view0{data};

  auto stride = 3 * sizeof(test_data);
  strided_view view{view0, stride};
  EXPECT_EQ(view.size(), data.size());
  EXPECT_EQ(view.stride_bytes(), stride);
  EXPECT_FALSE(view.empty());

  EXPECT_EQ(view[0], view.front());
  EXPECT_EQ(view[0], data[0]);
  EXPECT_EQ(view[1], data[3]);
  EXPECT_EQ(view[2], data[6]);
  EXPECT_EQ(view.at(3), data.at(9));
  EXPECT_THROW((void)view.at(4), std::out_of_range);
}
