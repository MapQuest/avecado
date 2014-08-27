#include "common.hpp"
#include "avecado.hpp"

#include <iostream>

#include <mapnik/utils.hpp>
#include <mapnik/load_map.hpp>
#include <mapnik/font_engine_freetype.hpp>
#include <mapnik/datasource_cache.hpp>

#include "vector_tile.pb.h"

#include "config.h"

using std::cout;
using std::endl;

#define WORLD_SIZE (40075016.68)

mapnik::box2d<double> box_for_tile(int z, int x, int y) {
  const double scale = WORLD_SIZE / double(1 << z);
  const double half_world = 0.5 * WORLD_SIZE;

  return mapnik::box2d<double>(
    x * scale - half_world,
    half_world - (y+1) * scale,
    (x+1) * scale - half_world,
    half_world - y * scale);
}

// Default constants

const unsigned int path_multiplier = 1;
const int buffer_size = 0;
const double scale_factor = 1.0;
const unsigned int offset_x = 0;
const unsigned int offset_y = 0;
const unsigned int tolerance = 1;
const std::string image_format = "jpeg";
const mapnik::scaling_method_e scaling_method = mapnik::SCALING_NEAR;
const double scale_denominator = 0;


void setup_mapnik() {
  mapnik::freetype_engine::register_fonts(MAPNIK_DEFAULT_FONT_DIR);
  mapnik::datasource_cache::instance().register_datasources(MAPNIK_DEFAULT_INPUT_PLUGIN_DIR);
}

void test_single_point() {
/* This test creates a CSV source with a single point at 0,0,
 * a map file which renders that point, and checks the resulting
 * vector tile for 0/0/0
 */
  mapnik::Map map;
  avecado::tile tile;
  // to-do: can we do this completely inline?
  mapnik::load_map(map, "test/single_point.xml");
  map.resize(256, 256);
  map.zoom_to_box(box_for_tile(0, 0, 0));
  avecado::make_vector_tile(tile, path_multiplier, map, buffer_size, scale_factor,
                            offset_x, offset_y, tolerance, image_format,
                            scaling_method, scale_denominator);
  mapnik::vector::tile result;
  result.ParseFromString(tile.get_data());
  mapnik::vector::tile_layer layer = result.layers(0);
  test::assert_equal(layer.version(), (unsigned int)(1), "Unknown layer version number");
}

int main() {
  int tests_failed = 0;

  cout << "== Testing make_vector_tile ==" << endl << endl;

#define RUN_TEST(x) { tests_failed += test::run(#x, &(x)); }
  RUN_TEST(setup_mapnik);
  RUN_TEST(test_single_point);
  cout << " >> Tests failed: " << tests_failed << endl << endl;

  return (tests_failed > 0) ? 1 : 0;

}
