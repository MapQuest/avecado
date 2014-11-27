#include "common.hpp"
#include "util_tile.hpp"
#include "vector_tile.pb.h"

#include <vector>
#include <utility>
#include <iostream>
#include <boost/property_tree/ptree.hpp>
#include <boost/format.hpp>

using mapnik::vector::tile_layer;

namespace {

void test_cover_empty() {
  tile_layer l;
  test::assert_equal<bool>(avecado::util::is_complete_cover(l), false);
}

} // anonymous namespace

int main() {
  int tests_failed = 0;

  std::cout << "== Testing tile utilities ==" << std::endl << std::endl;

#define RUN_TEST(x) { tests_failed += test::run(#x, &(x)); }

  RUN_TEST(test_cover_empty);

  std::cout << " >> Tests failed: " << tests_failed << std::endl << std::endl;

  return (tests_failed > 0) ? 1 : 0;
}
