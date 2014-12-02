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
  test::assert_equal<bool>(avecado::util::is_interesting(l), false);
}

void test_cover_full() {
  tile_layer l;
  l.set_name("boundingbox");
  mapnik::vector::tile_feature *feat = l.add_features();
  feat->set_id(1);
  feat->set_type(mapnik::vector::tile::Polygon);
  for (uint32_t i : {9, 63, 8256, 26, 0, 8319, 8320, 0, 0, 8320, 15}) {
    feat->add_geometry(i);
  }
  l.set_extent(4096);
  l.set_version(1);
  test::assert_equal<bool>(avecado::util::is_interesting(l), false);
}

// same as the last test, but the geometry is degenerate and
// folds back on itself
void test_cover_full_degenerate() {
  tile_layer l;
  l.set_name("water");
  mapnik::vector::tile_feature *feat = l.add_features();
  feat->set_id(2);
  feat->set_type(mapnik::vector::tile::Polygon);
  for (uint32_t i : {9, 63, 8256, 58, 0, 8319, 8320, 0, 0, 8320, 8319,
        0, 8320, 0, 8319, 0, 8320, 0, 15}) {
    feat->add_geometry(i);
  }
  l.set_extent(4096);
  l.set_version(1);
  test::assert_equal<bool>(avecado::util::is_interesting(l), false);
}

// a layer with more than one feature is interesting
void test_cover_many() {
  tile_layer l;
  l.set_name("boundingbox");
  for (int32_t id : {1, 2}) {
    mapnik::vector::tile_feature *feat = l.add_features();
    feat->set_id(id);
    feat->set_type(mapnik::vector::tile::Polygon);
    for (uint32_t i : {9, 63, 8256, 26, 0, 8319, 8320, 0, 0, 8320, 15}) {
      feat->add_geometry(i);
    }
    l.set_extent(4096);
    l.set_version(1);
  }
  test::assert_equal<bool>(avecado::util::is_interesting(l), true);
}

// something with a shape inside the bbox of the tile is
// interesting.
void test_cover_shape() {
  tile_layer l;
  l.set_name("boundingbox");
  mapnik::vector::tile_feature *feat = l.add_features();
  feat->set_id(1);
  feat->set_type(mapnik::vector::tile::Polygon);
  for (uint32_t i : {9, 63, 8256, 26, 0, 8319, 8320, 0, 0, 8320, 15}) {
    feat->add_geometry(i);
  }
  l.set_extent(8192);
  l.set_version(1);
  test::assert_equal<bool>(avecado::util::is_interesting(l), true);
}

} // anonymous namespace

int main() {
  int tests_failed = 0;

  std::cout << "== Testing tile utilities ==" << std::endl << std::endl;

#define RUN_TEST(x) { tests_failed += test::run(#x, &(x)); }

  RUN_TEST(test_cover_empty);
  RUN_TEST(test_cover_full);
  RUN_TEST(test_cover_full_degenerate);
  RUN_TEST(test_cover_many);
  RUN_TEST(test_cover_shape);

  std::cout << " >> Tests failed: " << tests_failed << std::endl << std::endl;

  return (tests_failed > 0) ? 1 : 0;
}
