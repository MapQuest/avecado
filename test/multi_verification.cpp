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

void test_multiline() {
/* This test uses a map file with a CSV source with a multiline and checks
 * the resulting vector tile at 0/0/0
 */
  mapnik::Map map;
  avecado::tile tile(_z, _x, _y);
  mapnik::load_map(map, "test/single_multiline.xml");
  map.resize(tile_size, tile_size);
  map.zoom_to_box(bbox);
  avecado::make_vector_tile(tile, path_multiplier, map, buffer_size, scale_factor,
                            offset_x, offset_y, tolerance, image_format,
                            scaling_method, scale_denominator, boost::none);
  avecado::tile tile2(_z, _x, _y);
  tile2.from_string(tile.get_data());
  const vector_tile::Tile &result = tile2.mapnik_tile();

  test::assert_equal(result.layers_size(), 1, "Wrong number of layers");
  vector_tile::Tile_Layer layer = result.layers(0);
  test::assert_equal(layer.name(), std::string ("point"), "Wrong layer name");
  test::assert_equal(layer.features_size(), 1, "Wrong number of features");
  vector_tile::Tile_Feature feature = layer.features(0);
  test::assert_equal(int(feature.type()), 2, "Wrong feature type");
  // The geometry should be a move to, lineto, moveto, lineto
  test::assert_equal(feature.geometry_size(), 12, "Wrong feature geometry length");
  // 00001 001
  test::assert_equal(int(feature.geometry(0)), 9, "First command should be MoveTo, length 1");
  // 00001 010
  test::assert_equal(int(feature.geometry(3)), 10, "Second command should be LineTo, length 1");
  test::assert_equal(int(feature.geometry(6)), 9, "Third command should be MoveTo, length 1");
  test::assert_equal(int(feature.geometry(9)), 10, "Third command should be LineTo, length 1");

  /* If we're this far, the PBF checks out */

  // Query the layer with mapnik. See https://github.com/mapbox/mapnik-vector-tile/blob/2e3e2c28/test/vector_tile.cpp#L236
  mapnik::vector_tile_impl::tile_datasource ds(layer, 0, 0, 0, tile_size);

  mapnik::query qq = mapnik::query(bbox);
  qq.add_property_name("name");
  mapnik::featureset_ptr fs;
  fs = ds.features(qq);
  mapnik::feature_ptr feat = fs->next();
  test::assert_equal(feat->num_geometries(),size_t(1),"Wrong feature length");
  test::assert_equal(feat->get_geometry(0).size(),size_t(4),"Wrong feature geometry length");
  // We should have the same moveto lineto moveto lineto as above
  double x, y = -1;
  test::assert_equal<unsigned int>(feat->get_geometry(0).vertex(0, &x, &y),mapnik::SEG_MOVETO, "First command should be SEG_MOVETO");
  test::assert_equal<unsigned int>(feat->get_geometry(0).vertex(1, &x, &y),mapnik::SEG_LINETO, "Second command should be SEG_LINETO");
  test::assert_equal<unsigned int>(feat->get_geometry(0).vertex(2, &x, &y),mapnik::SEG_MOVETO, "Second command should be SEG_MOVETO");
  test::assert_equal<unsigned int>(feat->get_geometry(0).vertex(3, &x, &y),mapnik::SEG_LINETO, "Second command should be SEG_LINETO");
  // MULTILINESTRINGs don't work with GeoJSON, see mapnik/mapnik#2518
}

void test_multipolygon() {
/* This test uses a map file with a CSV source with a multipolygon and checks
 * the resulting vector tile at 0/0/0
 */
  mapnik::Map map;
  avecado::tile tile(_z, _x, _y);
  mapnik::load_map(map, "test/single_multipolygon.xml");
  map.resize(tile_size, tile_size);
  map.zoom_to_box(bbox);
  avecado::make_vector_tile(tile, path_multiplier, map, buffer_size, scale_factor,
                            offset_x, offset_y, tolerance, image_format,
                            scaling_method, scale_denominator, boost::none);
  avecado::tile tile2(_z, _x, _y);
  tile2.from_string(tile.get_data());
  const vector_tile::Tile &result = tile2.mapnik_tile();

  test::assert_equal(result.layers_size(), 1, "Wrong number of layers");
  vector_tile::Tile_Layer layer = result.layers(0);
  test::assert_equal(layer.name(), std::string ("point"), "Wrong layer name");
  test::assert_equal(layer.features_size(), 1, "Wrong number of features");
  vector_tile::Tile_Feature feature = layer.features(0);
  test::assert_equal(int(feature.type()), 3, "Wrong feature type");
  // The geometry should be a move to, lineto, moveto, lineto
  test::assert_equal(feature.geometry_size(), 37, "Wrong feature geometry length");
  // 00001 001
  test::assert_equal(int(feature.geometry(0)), 9, "1st command should be MoveTo, length 1");
  // 2 * 1 entries for coords
  // 00011 010
  test::assert_equal(int(feature.geometry(3)), 26, "2nd command should be LineTo, length 3");
  // 2 * 3 entries for coords
  // 00001 111
  test::assert_equal(int(feature.geometry(10)), 15, "3rd command should be ClosePath, length 1");
  // 00001 001
  test::assert_equal(int(feature.geometry(11)), 9, "4th command should be MoveTo, length 1");
  // 00101 010
  test::assert_equal(int(feature.geometry(14)), 42, "5th command should be LineTo, length 5");
  test::assert_equal(int(feature.geometry(25)), 15, "6th command should be ClosePath, length 1");
  test::assert_equal(int(feature.geometry(26)), 9, "7th command should be MoveTo, length 1");
  test::assert_equal(int(feature.geometry(29)), 26, "8th command should be LineTo, length 3");
  test::assert_equal(int(feature.geometry(36)), 15, "8th command should be ClosePath, length 1");
}

int main() {
  int tests_failed = 0;

  cout << "== Testing make_vector_tile ==" << endl << endl;

#define RUN_TEST(x) { tests_failed += test::run(#x, &(x)); }
  RUN_TEST(setup_mapnik);
  RUN_TEST(test_multiline);
  RUN_TEST(test_multipolygon);
  cout << " >> Tests failed: " << tests_failed << endl << endl;

  return (tests_failed > 0) ? 1 : 0;

}
