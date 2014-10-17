#include "config.h"
#include "common.hpp"
#include "avecado.hpp"
#include "logging/logger.hpp"

#include <boost/property_tree/ptree.hpp>

#include <mapnik/datasource_cache.hpp>
#include <mapnik/wkt/wkt_factory.hpp>
#include <mapnik/wkt/wkt_grammar.hpp>
#include <mapnik/wkt/wkt_grammar_impl.hpp>
#include <mapnik/graphics.hpp>

#include "vector_tile.pb.h"

#include <iostream>

namespace {

void test_empty() {
  mapnik::color background_colour(0x8c, 0xc6, 0x3f);
  mapnik::image_32 image(256, 256);
  avecado::tile tile;
  mapnik::Map map(256, 256);
  map.set_background(background_colour);

  bool status = avecado::render_vector_tile(image, tile, map, 0, 0, 0, 1.0, 0);

  test::assert_equal<bool>(status, true, "should have rendered an image");

  const unsigned int rgba = background_colour.rgba();
  for (unsigned int y = 0; y < image.height(); ++y) {
    for (unsigned int x = 0; x < image.width(); ++x) {
      test::assert_equal<unsigned int>(image.data()(x, y), rgba, "should have set background colour");
    }
  }
}

} // anonymous namespace

int main() {
  int tests_failed = 0;

  std::cout << "== Testing render_vector_tile ==" << std::endl << std::endl;

  // set up mapnik's datasources, as we'll be using them in the
  // tests.
  mapnik::datasource_cache::instance().register_datasources(MAPNIK_DEFAULT_INPUT_PLUGIN_DIR);

#define RUN_TEST(x) { tests_failed += test::run(#x, &(x)); }
  RUN_TEST(test_empty);
  
  std::cout << " >> Tests failed: " << tests_failed << std::endl << std::endl;

  return (tests_failed > 0) ? 1 : 0;
}
