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
#include "util.hpp"

#include "config.h"

using std::cout;
using std::endl;

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
const mapnik::box2d<double> bbox = avecado::util::box_for_tile(_z, _x, _y);

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

/* JSON strings for map layers. These have a loss of precisision from the
 * vector tile conversion, and the best way to generate them is just to take
 * the actual output and then check that it's sensible */
const std::string single_point_json (R"({"type":"Feature","id":1,"geometry":{"type":"Point","coordinates":[0,0]},"properties":{"name":"null island"}})");
const std::string single_line_json (R"({"type":"Feature","id":1,"geometry":{"type":"LineString","coordinates":[[-2035059.44106453,0],[-939258.203568246,1252344.27142433],[939258.203568246,939258.203568246],[2035059.44106453,-0.0000000001164153]]},"properties":{"name":"null highway"}})");
const std::string single_poly_json (R"({"type":"Feature","id":1,"geometry":{"type":"Polygon","coordinates":[[[-2035059.44106453,0],[-939258.203568246,1095801.23749629],[939258.203568246,939258.203568246],[2035059.44106453,0],[-2035059.44106453,0],[-2035059.44106453,0]],[[-156543.033928041,0],[0.0000000000873115,156543.033928041],[156543.033928041,0],[-156543.033928041,0],[-156543.033928041,0]]]},"properties":{"name":"null lake"}})");
// intersected with a z1 tile
const std::string single_line_z1_json (R"({"type":"Feature","id":1,"geometry":{"type":"LineString","coordinates":[[-2035059.44106453,0],[-1017529.72053227,1252344.27142433],[-0.0000000002328306,1095801.23749629]]},"properties":{"name":"null highway"}})");

void test_single_point() {
/* This test uses a map file with a CSV source single point at 0,0 and checks
 * the resulting vector tile at 0/0/0
 */

  avecado::tile tile(_z, _x, _y);
  mapnik::Map map = test::make_map("test/single_point.xml", tile_size, _z, _x, _y);
  avecado::make_vector_tile(tile, path_multiplier, map, buffer_size, scale_factor,
                            offset_x, offset_y, tolerance, image_format,
                            scaling_method, scale_denominator, boost::none);
  avecado::tile tile2(_z, _x, _y);
  tile2.from_string(tile.get_data());
  const vector_tile::Tile &result = tile2.mapnik_tile();

  test::assert_equal(result.layers_size(), 1, "Wrong number of layers");
  vector_tile::Tile_Layer layer = result.layers(0);

  // Query the layer with mapnik. See https://github.com/mapbox/mapnik-vector-tile/blob/2e3e2c28/test/vector_tile.cpp#L236
  mapnik::vector_tile_impl::tile_datasource ds(layer, 0, 0, 0, tile_size);

  mapnik::query qq = mapnik::query(bbox);
  qq.add_property_name("name");
  mapnik::featureset_ptr fs;
  fs = ds.features(qq);
  mapnik::feature_ptr feat = fs->next();
  std::string json = feature_to_geojson(*feat);
  test::assert_equal(json, single_point_json, "Wrong JSON");
}

void test_single_line() {
/* This test uses a map file with a CSV source line and checks the resulting
 * vector tile at 0/0/0
 */
  avecado::tile tile(_z, _x, _y);
  mapnik::Map map = test::make_map("test/single_line.xml", tile_size, _z, _x, _y);
  avecado::make_vector_tile(tile, path_multiplier, map, buffer_size, scale_factor,
                            offset_x, offset_y, tolerance, image_format,
                            scaling_method, scale_denominator, boost::none);
  avecado::tile tile2(_z, _x, _y);
  tile2.from_string(tile.get_data());
  const vector_tile::Tile &result = tile2.mapnik_tile();

  test::assert_equal(result.layers_size(), 1, "Wrong number of layers");
  vector_tile::Tile_Layer layer = result.layers(0);

  // Query the layer with mapnik. See https://github.com/mapbox/mapnik-vector-tile/blob/2e3e2c28/test/vector_tile.cpp#L236
  mapnik::vector_tile_impl::tile_datasource ds(layer, 0, 0, 0, tile_size);

  mapnik::query qq = mapnik::query(bbox);
  qq.add_property_name("name");
  mapnik::featureset_ptr fs;
  fs = ds.features(qq);
  mapnik::feature_ptr feat = fs->next();
  std::string json = feature_to_geojson(*feat);
  test::assert_equal(json, single_line_json, "Wrong JSON");
}

void test_single_polygon() {
/* This test uses a map file with a CSV source polygon and checks the
 * resulting vector tile at 0/0/0
 */
  avecado::tile tile(_z, _x, _y);
  mapnik::Map map = test::make_map("test/single_poly.xml", tile_size, _z, _x, _y);
  avecado::make_vector_tile(tile, path_multiplier, map, buffer_size, scale_factor,
                            offset_x, offset_y, tolerance, image_format,
                            scaling_method, scale_denominator, boost::none);
  avecado::tile tile2(_z, _x, _y);
  tile2.from_string(tile.get_data());
  const vector_tile::Tile &result = tile2.mapnik_tile();

  test::assert_equal(result.layers_size(), 1, "Wrong number of layers");
  vector_tile::Tile_Layer layer = result.layers(0);

  // Query the layer with mapnik. See https://github.com/mapbox/mapnik-vector-tile/blob/2e3e2c28/test/vector_tile.cpp#L236
  mapnik::vector_tile_impl::tile_datasource ds(layer, 0, 0, 0, tile_size);

  mapnik::query qq = mapnik::query(bbox);
  qq.add_property_name("name");
  mapnik::featureset_ptr fs;
  fs = ds.features(qq);
  mapnik::feature_ptr feat = fs->next();
  std::string json = feature_to_geojson(*feat);
  test::assert_equal(json, single_poly_json, "Wrong JSON");
}

void test_intersected_line() {
/* This test uses a map file with a CSV source line and checks the resulting
 * vector tile for 1/0/0, while the line extends outside that tile
 */
  avecado::tile tile(_z, _x, _y);
  mapnik::Map map = test::make_map("test/single_line.xml", tile_size, 1, 0, 0);
  avecado::make_vector_tile(tile, path_multiplier, map, buffer_size, scale_factor,
                            offset_x, offset_y, tolerance, image_format,
                            scaling_method, scale_denominator, boost::none);
  avecado::tile tile2(_z, _x, _y);
  tile2.from_string(tile.get_data());
  const vector_tile::Tile &result = tile2.mapnik_tile();

  test::assert_equal(result.layers_size(), 1, "Wrong number of layers");
  vector_tile::Tile_Layer layer = result.layers(0);

  // Query the layer with mapnik. See https://github.com/mapbox/mapnik-vector-tile/blob/2e3e2c28/test/vector_tile.cpp#L236
  // note tile_datasource has x, y, z
  mapnik::vector_tile_impl::tile_datasource ds(layer, 0, 0, 1, tile_size);

  mapnik::query qq = mapnik::query(avecado::util::box_for_tile(1, 0, 0));
  qq.add_property_name("name");
  mapnik::featureset_ptr fs;
  fs = ds.features(qq);
  mapnik::feature_ptr feat = fs->next();
  std::string json = feature_to_geojson(*feat);
  test::assert_equal(json, single_line_z1_json, "Wrong JSON");
}


int main() {
  int tests_failed = 0;

  cout << "== Testing make_vector_tile ==" << endl << endl;

#define RUN_TEST(x) { tests_failed += test::run(#x, &(x)); }
  RUN_TEST(setup_mapnik);
  RUN_TEST(test_single_point);
  RUN_TEST(test_single_line);
  RUN_TEST(test_single_polygon);
  RUN_TEST(test_intersected_line);
  cout << " >> Tests failed: " << tests_failed << endl << endl;

  return (tests_failed > 0) ? 1 : 0;

}
