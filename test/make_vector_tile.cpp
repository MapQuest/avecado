#include "common.hpp"
#include "avecado.hpp"

#include <iostream>

#include <mapnik/utils.hpp>
#include <mapnik/load_map.hpp>
#include <mapnik/font_engine_freetype.hpp>
#include <mapnik/datasource_cache.hpp>
#include <mapnik/json/feature_generator.hpp>
#include <mapnik/json/feature_generator_grammar_impl.hpp>
#include <mapnik/json/geometry_generator_grammar_impl.hpp>

#include "vector_tile.pb.h"
#include "vector_tile_datasource.hpp"

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

const unsigned tile_size = 256;
const unsigned _x=0,_y=0,_z=0; 
const mapnik::box2d<double> bbox = box_for_tile(_z, _x, _y);

void setup_mapnik() {
  mapnik::freetype_engine::register_fonts(MAPNIK_DEFAULT_FONT_DIR);
  mapnik::datasource_cache::instance().register_datasources(MAPNIK_DEFAULT_INPUT_PLUGIN_DIR);
}

/* See bindings/python/mapnik_feature.cpp from Mapnik */
std::string feature_to_geojson(mapnik::feature_impl const& feature)
{
  std::string json;
  test::assert_equal(mapnik::json::to_geojson(json,feature), true, "Failed to convert to GeoJSON");
  return json;
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
  map.resize(tile_size, tile_size);
  map.zoom_to_box(bbox);
  avecado::make_vector_tile(tile, path_multiplier, map, buffer_size, scale_factor,
                            offset_x, offset_y, tolerance, image_format,
                            scaling_method, scale_denominator, boost::none);
  mapnik::vector::tile result;
  result.ParseFromString(tile.get_data());
  test::assert_equal(result.layers_size(), 1, "Wrong number of layers");
  mapnik::vector::tile_layer layer = result.layers(0);

  // Query the layer with mapnik. See https://github.com/mapbox/mapnik-vector-tile/blob/2e3e2c28/test/vector_tile.cpp#L236
  mapnik::vector::tile_datasource ds(layer, 0, 0, 0, tile_size);

  mapnik::query qq = mapnik::query(bbox);
  qq.add_property_name("name");
  mapnik::featureset_ptr fs;
  fs = ds.features(qq);
  mapnik::feature_ptr feat = fs->next();
  std::string json = feature_to_geojson(*feat);
  test::assert_equal(json,
                     std::string(R"({"type":"Feature","id":1,"geometry":{"type":"Point","coordinates":[0,0]},"properties":{"name":"null island"}})"),
                     "Wrong JSON");
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
