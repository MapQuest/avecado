#include "config.h"
#include "common.hpp"
#include "avecado.hpp"
#include "logging/logger.hpp"

#include <boost/property_tree/ptree.hpp>

#include <mapnik/datasource_cache.hpp>
#include <mapnik/wkt/wkt_factory.hpp>
#include <mapnik/wkt/wkt_grammar.hpp>
#include <mapnik/wkt/wkt_grammar_impl.hpp>
#include <mapnik/symbolizer.hpp>
#include <mapnik/symbolizer_keys.hpp>
#include <mapnik/symbolizer_utils.hpp>
#include <mapnik/layer.hpp>

#include "vector_tile.pb.h"

#include <iostream>

namespace {

void test_empty() {
  mapnik::color background_colour(0x8c, 0xc6, 0x3f);
  mapnik::image_rgba8 image(256, 256);
  avecado::tile tile(0, 0, 0);
  mapnik::Map map(256, 256);
  map.set_background(background_colour);

  bool status = avecado::render_vector_tile(image, tile, map, 1.0, 0);

  test::assert_equal<bool>(status, true, "should have rendered an image");

  const rgba8_t::type rgba = background_colour.rgba();
  for (unsigned int y = 0; y < image.height(); ++y) {
    for (unsigned int x = 0; x < image.width(); ++x) {
      test::assert_equal<rgba8_t::type>(image(x, y), rgba, "should have set background colour");
    }
  }
}

void test_full() {
  mapnik::color background_colour(0x8c, 0xc6, 0x3f);
  mapnik::color fill_colour(0x51, 0x21, 0x4d);
  mapnik::image_rgba8 image(256, 256);

  mapnik::Map map(256, 256);
  {
    map.set_background(background_colour);
    map.zoom_to_box(mapnik::box2d<double>(-180, -90, 180, 90));

    mapnik::polygon_symbolizer symbolizer;
    mapnik::set_property(symbolizer, mapnik::keys::fill, fill_colour.to_string());

    mapnik::rule rule;
    rule.append(std::move(symbolizer));

    mapnik::feature_type_style style;
    style.add_rule(std::move(rule));
    map.insert_style("style", std::move(style));

    mapnik::layer layer("layer");
    layer.set_srs(map.srs());
    layer.add_style("style");
    map.add_layer(std::move(layer));
  }

  avecado::tile tile(0, 0, 0);
  {
    vector_tile::Tile_Layer *layer = tile.mapnik_tile().add_layers();
    layer->set_version(1);
    layer->set_name("layer");
    vector_tile::Tile_Feature *feature = layer->add_features();
    feature->set_id(1);
    feature->set_type(vector_tile::Tile::POLYGON);
    // this strange sequence of numbers comes from cranking the mapnik vector
    // tile geometry algorithm by hand on a [-180 -90, 180 90] box.
    for (uint32_t g : {9, 0, 128, 26, 512, 0, 0, 256, 511, 0, 7}) {
      feature->add_geometry(g);
    }
    layer->set_extent(256);
  }

  bool status = avecado::render_vector_tile(image, tile, map, 1.0, 0);

  test::assert_equal<bool>(status, true, "should have rendered an image");

  const rgba8_t::type rgba = fill_colour.rgba();
  for (unsigned int y = 0; y < image.height(); ++y) {
    for (unsigned int x = 0; x < image.width(); ++x) {
      test::assert_equal<rgba8_t::type>(image(x, y), rgba, "should have set fill colour");
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
  RUN_TEST(test_full);
  
  std::cout << " >> Tests failed: " << tests_failed << std::endl << std::endl;

  return (tests_failed > 0) ? 1 : 0;
}
