#include "config.h"
#include "common.hpp"
#include "post_process/adminizer.hpp"

#include <boost/property_tree/ptree.hpp>

#include <mapnik/datasource_cache.hpp>
#include <mapnik/wkt/wkt_factory.hpp>
#include <mapnik/wkt/wkt_grammar.hpp>
#include <mapnik/wkt/wkt_grammar_impl.hpp>

#include <iostream>

using std::make_pair;
namespace pp = avecado::post_process;

namespace {

pp::izer_ptr mk_10x10_poly_izer() {
  pt::ptree conf;
  conf.put("param_name", "foo");
  conf.put("datasource.type", "csv");
  // Square going to +/- 10
  conf.put("datasource.inline",
           "wkt|foo\n"
           "Polygon((-10.0 -10.0, -10.0 10.0, 10.0 10.0, 10.0 -10.0, -10.0 -10.0))|foo_value\n");
  return pp::create_adminizer(conf);
}

mapnik::feature_ptr mk_feat_wkt(const std::string &wkt) {
  mapnik::context_ptr ctx = std::make_shared<mapnik::context_type>();
  mapnik::feature_ptr feat = std::make_shared<mapnik::feature_impl>(ctx, 0);
  if (!mapnik::from_wkt(wkt, feat->paths())) {
    throw std::runtime_error((boost::format("Unable to parse WKT geometry from "
                                            "string \"%1%\"") % wkt).str());
  }
  return feat;
}

void assert_line_geom_equal(mapnik::feature_ptr &feat,
                            const std::initializer_list<double> &coords) {
  using mapnik::geometry_type;

  test::assert_equal<size_t>(feat->num_geometries(), 1,
                             "Feature should only have one geometry");
  const geometry_type &geom = feat->get_geometry(0);
  test::assert_equal<geometry_type::types>(geom.type(), geometry_type::LineString,
                                           "geometry should be LineString");
  test::assert_equal<size_t>(geom.size(), coords.size() / 2,
                             "should have the expected number of coordinates");

  auto itr = coords.begin();
  auto end = coords.end();
  auto expect_cmd = mapnik::SEG_MOVETO;
  geom.rewind(0);

  double expect_x = -1, expect_y = -1, actual_x = -1, actual_y = -1;

  while (itr != end) {
    expect_x = *itr++;
    if (itr == end) { break; }
    expect_y = *itr++;

    auto actual_cmd = geom.vertex(&actual_x, &actual_y);

    test::assert_equal<unsigned int>(actual_cmd, expect_cmd, "command");
    test::assert_equal<double>(actual_x, expect_x, "x");
    test::assert_equal<double>(actual_y, expect_y, "y");

    expect_cmd = mapnik::SEG_LINETO;
  }
}

void assert_points_geom_equal(mapnik::feature_ptr &feat,
                              const std::initializer_list<std::pair<double, double> > &coords) {
  using mapnik::geometry_type;

  test::assert_equal<size_t>(feat->num_geometries(), 1,
                             "Feature should only have one geometry");
  const geometry_type &geom = feat->get_geometry(0);
  test::assert_equal<geometry_type::types>(geom.type(), geometry_type::Point,
                                           "geometry should be Point");
  test::assert_equal<size_t>(geom.size(), coords.size(),
                             "should have the expected number of coordinates");

  auto itr = coords.begin();
  auto end = coords.end();
  geom.rewind(0);

  double expect_x = -1, expect_y = -1, actual_x = -1, actual_y = -1;

  while (itr != end) {
    boost::tie(expect_x, expect_y) = *itr++;

    unsigned int actual_cmd = geom.vertex(&actual_x, &actual_y);

    test::assert_equal<unsigned int>(actual_cmd, mapnik::SEG_MOVETO, "command");
    test::assert_equal<double>(actual_x, expect_x, "x");
    test::assert_equal<double>(actual_y, expect_y, "y");
  }
}

void assert_wkt_geom_equal(mapnik::feature_ptr &feat, const std::string &wkt) {
  using mapnik::geometry_type;
  using mapnik::geometry_container;

  geometry_container expected_paths;
  test::assert_equal<bool>(mapnik::from_wkt(wkt, expected_paths), true, "valid WKT");

  const geometry_container &actual_paths = feat->paths();

  const size_t num_paths = expected_paths.size();
  test::assert_equal<size_t>(actual_paths.size(), num_paths,
                             "same number of paths in geometry");

  // NOTE: this doesn't take account of potential re-ordering of the paths
  // within the geometry container, which would be valid. it's not clear
  // whether anything which processes the geometry would re-order them, so
  // until the test has a spurious failure because of re-ordering, let's
  // just assume the ordering remains the same.
  for (size_t i = 0; i < num_paths; ++i) {
    const geometry_type &actual = actual_paths[i];
    const geometry_type &expected = expected_paths[i];

    test::assert_equal<geometry_type::types>(actual.type(), expected.type(),
                                             "geometry types");
    test::assert_equal<size_t>(actual.size(), expected.size(),
                               "number of coordinates");

    // NOTE: this doesn't account for rotation of any polygon rings,
    // which means the test might give a false negative. however, until
    // we have an example of that, let's just assume it doesn't happen.
    actual.rewind(0);
    expected.rewind(0);
    while (true) {
      double actual_x = 0, actual_y = 0, expected_x = 0, expected_y = 0;
      unsigned int actual_cmd = actual.vertex(&actual_x, &actual_y);
      unsigned int expected_cmd = expected.vertex(&expected_x, &expected_y);

      test::assert_equal<unsigned int>(actual_cmd, expected_cmd, "command");
      if (actual_cmd == mapnik::SEG_END) {
        break;
      }

      test::assert_equal<double>(actual_x, expected_x, "x");
      test::assert_equal<double>(actual_y, expected_y, "y");
    }
  }
}

void assert_has_new_param(const mapnik::feature_ptr &feat) {
  test::assert_equal<bool>(feat->has_key("foo"), true,
                           "feature should have parameter key \"foo\" after adminizing");
  test::assert_equal<std::string>(feat->get("foo").to_string(), "foo_value",
                                  "feature should have parameter from adminizing polygon");
}

void assert_izer_include(const std::string &wkt) {
  pp::izer_ptr izer = mk_10x10_poly_izer();
  std::vector<mapnik::feature_ptr> features;
  features.push_back(mk_feat_wkt(wkt));

  izer->process(features);

  test::assert_equal<size_t>(features.size(), 1, "should be only one feature");

  // being adminized should have added (or overwritten) the 'foo'
  // parameter from the admin polygon.
  assert_has_new_param(features[0]);

  // being adminized shouldn't have affected this geometry because it's
  // entirely within the admin polygon
  assert_wkt_geom_equal(features[0], wkt);
}

void assert_izer_exclude(const std::string &wkt) {
  pp::izer_ptr izer = mk_10x10_poly_izer();
  std::vector<mapnik::feature_ptr> features;
  features.push_back(mk_feat_wkt(wkt));

  izer->process(features);

  test::assert_equal<size_t>(features.size(), 1, "should be only one feature");

  // since the geometry is outside the admin polygon, no parameter
  // should have been written.
  test::assert_equal<bool>(features[0]->has_key("foo"), false,
                           "feature should not have been affected by adminizer.");

  // being adminized shouldn't have affected this geometry because it's
  // entirely outside the admin polygon
  assert_wkt_geom_equal(features[0], wkt);
}

// test simple assignment of an object's parameter by inclusion
// in an area.
void test_point_simple_inclusion_param() {
  assert_izer_include("POINT(0 0)");
}

// test that an object outside of the polygon is not modified.
void test_point_simple_exclusion_param() {
  assert_izer_exclude("POINT(11 11)");
}

void test_multipoint_simple_inclusion_param() {
  assert_izer_include("MULTIPOINT((0 0))");
  assert_izer_include("MULTIPOINT((0 0),(1 1))");
}

// test that an object outside of the polygon is not modified.
void test_multipoint_simple_exclusion_param() {
  assert_izer_exclude("MULTIPOINT((11 11))");
  assert_izer_exclude("MULTIPOINT((11 11), (12 12))");
}

void test_line_simple_inclusion_param() {
  assert_izer_include("LINESTRING(0 0, 1 1, 2 0, 3 1, 4 0)");
}

void test_line_simple_exclusion_param() {
  assert_izer_exclude("LINESTRING(0 11, 11 11, 11 -11, 0 -11)");
}

void test_poly_simple_inclusion_param() {
  assert_izer_include("POLYGON((0 0, 1 0, 1 1, 0 1, 0 0))");
}

void test_poly_simple_exclusion_param() {
  assert_izer_exclude("POLYGON((20 0, 21 0, 21 1, 20 1, 20 0))");
}

void test_multipoly_simple_inclusion_param() {
  assert_izer_include("MULTIPOLYGON(((0 0, 1 0, 1 1, 0 1, 0 0)))");
  assert_izer_include("MULTIPOLYGON(((0 0, 1 0, 1 1, 0 1, 0 0)),((5 5, 6 5, 6 6, 5 6, 5 5)))");
}

void test_multipoly_simple_exclusion_param() {
  assert_izer_exclude("MULTIPOLYGON(((20 0, 21 0, 21 1, 20 1, 20 0)))");
  assert_izer_exclude("MULTIPOLYGON(((20 0, 21 0, 21 1, 20 1, 20 0)),((-20 0, -21 0, -21 1, -20 1, -20 0)))");
}

// test that a polygon with a hole in it, but still intersecting the
// adminizer box gets included.
void test_poly_inner_inclusion_param() {
  assert_izer_include(
    "POLYGON("
    "  (-10 -20, 30 -20, 30 20, -10 20, -10 -20),"
    "  ( -1 -11, 21 -11, 21 11,  -1 11,  -1 -11)"
    ")");
}

// test that a polygon with a hole in it, where the hole means the
// adminizer polygon is completely outside the polygon, is not
// included.
void test_poly_inner_exclusion_param() {
  /** NOTE: this currently does not work due to a bug upstream which
   ** is being worked on: https://github.com/boostorg/geometry/pull/159
   ** and, until we pull in a fixed version of boost::geometry, this
   ** test will fail. **/
  // assert_izer_exclude(
  //   "POLYGON("
  //   "  (-20 -20, 20 -20, 20 20, -20 20, -20 -20),"
  //   "  (-11 -11, 11 -11, 11 11, -11 11, -11 -11)"
  //   ")");
}

} // anonymous namespace

int main() {
  int tests_failed = 0;

  std::cout << "== Testing adminizer ==" << std::endl << std::endl;

  // set up mapnik's datasources, as we'll be using them in the
  // tests.
  mapnik::datasource_cache::instance().register_datasources(MAPNIK_DEFAULT_INPUT_PLUGIN_DIR);

#define RUN_TEST(x) { tests_failed += test::run(#x, &(x)); }
  RUN_TEST(test_point_simple_inclusion_param);
  RUN_TEST(test_point_simple_exclusion_param);

  // mapbox/mapnik-vector-tile#62 prevents multipoints from working properly through the entire toolchain
  RUN_TEST(test_multipoint_simple_inclusion_param);
  RUN_TEST(test_multipoint_simple_exclusion_param);

  RUN_TEST(test_line_simple_inclusion_param);
  RUN_TEST(test_line_simple_exclusion_param);

  RUN_TEST(test_poly_simple_inclusion_param);
  RUN_TEST(test_poly_simple_exclusion_param);

  RUN_TEST(test_multipoly_simple_inclusion_param);
  RUN_TEST(test_multipoly_simple_exclusion_param);

  RUN_TEST(test_poly_inner_inclusion_param);
  RUN_TEST(test_poly_inner_exclusion_param);
  
  std::cout << " >> Tests failed: " << tests_failed << std::endl << std::endl;

  return (tests_failed > 0) ? 1 : 0;
}
