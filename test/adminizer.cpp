#include "config.h"
#include "common.hpp"
#include "post_process/adminizer.hpp"
#include "logging/logger.hpp"

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

std::string intersection_param(pp::izer_ptr &izer, const std::string &wkt) {
  std::vector<mapnik::feature_ptr> features;
  features.push_back(mk_feat_wkt(wkt));

  izer->process(features);

  test::assert_equal<size_t>(features.size(), 1, "should be only one feature");
  mapnik::feature_ptr feat = features[0];

  test::assert_equal<bool>(feat->has_key("foo"), true,
                           "feature should have parameter key \"foo\" after adminizing");

  return feat->get("foo").to_string();
}

void test_intersection_mode_first() {
  pt::ptree conf;
  conf.put("param_name", "foo");
  conf.put("datasource.type", "csv");
  conf.put("datasource.inline",
           "wkt|foo\n"
           "POLYGON((0 0, 3 0, 3 3, 0 3, 0 0))|first_value\n"
           "POLYGON((1 1, 4 1, 4 4, 1 4, 1 1))|second_value\n");
  pp::izer_ptr izer = pp::create_adminizer(conf);

  std::string adminized_param = intersection_param(izer, "POINT(2 2)");

  test::assert_equal<std::string>(adminized_param, "first_value",
                                  "when intersection mode is first, should have the "
                                  "first admin polygon's parameter");
}

void test_intersection_mode_collect() {
  pt::ptree conf;
  conf.put("param_name", "foo");
  conf.put("collect", "true");
  conf.put("delimiter", "|");
  conf.put("datasource.type", "csv");
  conf.put("datasource.inline",
           "wkt|foo\n"
           "POLYGON((0 0, 3 0, 3 3, 0 3, 0 0))|first_value\n"
           "POLYGON((1 1, 4 1, 4 4, 1 4, 1 1))|second_value\n");
  pp::izer_ptr izer = pp::create_adminizer(conf);

  std::string adminized_param = intersection_param(izer, "POINT(2 2)");

  test::assert_equal<std::string>(adminized_param, "first_value|second_value",
                                  "when intersection mode is collect, should have "
                                  "all the admin polygons' parameters");
}

void test_intersection_mode_split() {
  using mapnik::feature_ptr;
  using mapnik::geometry_type;

  pt::ptree conf;
  conf.put("param_name", "foo");
  conf.put("split", "true");
  conf.put("datasource.type", "csv");
  conf.put("datasource.inline",
           "wkt|foo\n"
           "POLYGON((0 0, 3 0, 3 3, 0 3, 0 0))|first_value\n");
  pp::izer_ptr izer = pp::create_adminizer(conf);

  std::vector<feature_ptr> features;
  features.push_back(mk_feat_wkt("LINESTRING(-1 2, 5 2)"));

  izer->process(features);

  int segments[] = {-1, -1, -1};
  int num_segments = 0;

  for (size_t i = 0; i < features.size(); ++i) {
    feature_ptr feat = features[i];

    const size_t num_geoms = feat->num_geometries();
    for (size_t j = 0; j < num_geoms; ++j) {
      const geometry_type &geom = feat->get_geometry(j);
      test::assert_equal<geometry_type::types>(geom.type(), geometry_type::LineString,
                                               "geometry should be LineString");

      geom.rewind(0);
      double x = 0, y = 0;
      unsigned int cmd = 0;

      while ((cmd = geom.vertex(&x, &y)) != mapnik::SEG_END) {
        // we're expecting segments running L->R starting at x = -1, 0 & 3
        if (cmd == mapnik::SEG_MOVETO) {
          if (std::abs(x + 1.0) < 1.0e-6) {
            segments[0] = i;

          } else if (std::abs(x) < 1.0e-6) {
            segments[1] = i;

          } else if (std::abs(x - 3.0) < 1.0e-6) {
            segments[2] = i;

          } else {
            throw std::runtime_error((boost::format("Unexpected x=%1% coordinate") % x).str());
          }

          ++num_segments;
        }
      }
    }
  }

  test::assert_equal<int>(num_segments, 3, "should be 3 segments");

  for (int i = 0; i < 3; ++i) {
    if (segments[i] == -1) {
      throw std::runtime_error((boost::format("Segment %1% not found") % i).str());
    }
  }

  // segments outside of the admin polygon should not be set
  test::assert_equal<bool>(features[segments[0]]->has_key("foo"), false,
                           "segment 0 outside of hit area should not have \"foo\" set");
  test::assert_equal<bool>(features[segments[2]]->has_key("foo"), false,
                           "segment 3 outside of hit area should not have \"foo\" set");

  // the segments inside of the admin polygons should have been adminized
  test::assert_equal<bool>(features[segments[1]]->has_key("foo"), true,
                           "segment 1 inside of hit area should have \"foo\" set");
  test::assert_equal<std::string>(features[segments[1]]->get("foo").to_string(),
                                  "first_value", "segment 1 should be adminized");
}

void test_intersection_mode_split_first() {
  using mapnik::feature_ptr;
  using mapnik::geometry_type;

  pt::ptree conf;
  conf.put("param_name", "foo");
  conf.put("split", "true");
  conf.put("datasource.type", "csv");
  conf.put("datasource.inline",
           "wkt|foo\n"
           "POLYGON((0 0, 3 0, 3 3, 0 3, 0 0))|first_value\n"
           "POLYGON((1 1, 4 1, 4 4, 1 4, 1 1))|second_value\n");
  pp::izer_ptr izer = pp::create_adminizer(conf);

  std::vector<feature_ptr> features;
  features.push_back(mk_feat_wkt("LINESTRING(-1 2, 5 2)"));

  izer->process(features);

  int segments[] = {-1, -1, -1, -1};
  int num_segments = 0;

  for (size_t i = 0; i < features.size(); ++i) {
    feature_ptr feat = features[i];

    const size_t num_geoms = feat->num_geometries();
    for (size_t j = 0; j < num_geoms; ++j) {
      const geometry_type &geom = feat->get_geometry(j);
      test::assert_equal<geometry_type::types>(geom.type(), geometry_type::LineString,
                                               "geometry should be LineString");

      geom.rewind(0);
      double x = 0, y = 0;
      unsigned int cmd = 0;

      while ((cmd = geom.vertex(&x, &y)) != mapnik::SEG_END) {
        // we're expecting segments running L->R starting at x = -1, 0, 3 & 4
        if (cmd == mapnik::SEG_MOVETO) {
          if (std::abs(x + 1.0) < 1.0e-6) {
            segments[0] = i;

          } else if (std::abs(x) < 1.0e-6) {
            segments[1] = i;

          } else if (std::abs(x - 3.0) < 1.0e-6) {
            segments[2] = i;

          } else if (std::abs(x - 4.0) < 1.0e-6) {
            segments[3] = i;

          } else {
            throw std::runtime_error((boost::format("Unexpected x=%1% coordinate") % x).str());
          }

          ++num_segments;
        }
      }
    }
  }

  test::assert_equal<int>(num_segments, 4, "should be 4 segments");

  for (int i = 0; i < 4; ++i) {
    if (segments[i] == -1) {
      throw std::runtime_error((boost::format("Segment %1% not found") % i).str());
    }
  }

  // the segments outside of the admin polygons should not be affected
  test::assert_equal<bool>(features[segments[0]]->has_key("foo"), false,
                           "segment 0 outside of hit area should not have \"foo\" set");
  test::assert_equal<bool>(features[segments[3]]->has_key("foo"), false,
                           "segment 3 outside of hit area should not have \"foo\" set");

  // the segments inside of the admin polygons should have been adminized
  test::assert_equal<bool>(features[segments[1]]->has_key("foo"), true,
                           "segment 1 inside of hit area should have \"foo\" set");
  test::assert_equal<bool>(features[segments[2]]->has_key("foo"), true,
                           "segment 2 inside of hit area should have \"foo\" set");

  // and they should be set to the right values...
  test::assert_equal<std::string>(features[segments[1]]->get("foo").to_string(),
                                  "first_value", "segment 1 should have first value");
  test::assert_equal<std::string>(features[segments[2]]->get("foo").to_string(),
                                  "second_value", "segment 2 should have second value");
}


void test_intersection_mode_split_collect() {
  using mapnik::feature_ptr;
  using mapnik::geometry_type;

  pt::ptree conf;
  conf.put("param_name", "foo");
  conf.put("split", "true");
  conf.put("collect", "true");
  conf.put("delimiter", "|");
  conf.put("datasource.type", "csv");
  conf.put("datasource.inline",
           "wkt|foo\n"
           "POLYGON((0 0, 3 0, 3 3, 0 3, 0 0))|first_value\n"
           "POLYGON((1 1, 4 1, 4 4, 1 4, 1 1))|second_value\n");
  pp::izer_ptr izer = pp::create_adminizer(conf);

  std::vector<feature_ptr> features;
  features.push_back(mk_feat_wkt("LINESTRING(-1 2, 5 2)"));

  izer->process(features);

  int segments[] = {-1, -1, -1, -1, -1};
  int num_segments = 0;

  for (size_t i = 0; i < features.size(); ++i) {
    feature_ptr feat = features[i];

    const size_t num_geoms = feat->num_geometries();
    for (size_t j = 0; j < num_geoms; ++j) {
      const geometry_type &geom = feat->get_geometry(j);
      test::assert_equal<geometry_type::types>(geom.type(), geometry_type::LineString,
                                               "geometry should be LineString");

      geom.rewind(0);
      double x = 0, y = 0;
      unsigned int cmd = 0;

      while ((cmd = geom.vertex(&x, &y)) != mapnik::SEG_END) {
        // we're expecting segments running L->R starting at x = -1, 0, 1, 3 & 4
        if (cmd == mapnik::SEG_MOVETO) {
          if (std::abs(x + 1.0) < 1.0e-6) {
            segments[0] = i;

          } else if (std::abs(x) < 1.0e-6) {
            segments[1] = i;

          } else if (std::abs(x - 1.0) < 1.0e-6) {
            segments[2] = i;

          } else if (std::abs(x - 3.0) < 1.0e-6) {
            segments[3] = i;

          } else if (std::abs(x - 4.0) < 1.0e-6) {
            segments[4] = i;

          } else {
            throw std::runtime_error((boost::format("Unexpected x=%1% coordinate") % x).str());
          }

          ++num_segments;
        }
      }
    }
  }

  test::assert_equal<int>(num_segments, 5, "should be 5 segments");

  for (int i = 0; i < 5; ++i) {
    if (segments[i] == -1) {
      throw std::runtime_error((boost::format("Segment %1% not found") % i).str());
    }
  }

  // the segments outside of the admin polygons should not be affected
  test::assert_equal<bool>(features[segments[0]]->has_key("foo"), false,
                           "segment 0 outside of hit area should not have \"foo\" set");
  test::assert_equal<bool>(features[segments[4]]->has_key("foo"), false,
                           "segment 3 outside of hit area should not have \"foo\" set");

  // the segments inside of the admin polygons should have been adminized
  test::assert_equal<bool>(features[segments[1]]->has_key("foo"), true,
                           "segment 1 inside of hit area should have \"foo\" set");
  test::assert_equal<bool>(features[segments[2]]->has_key("foo"), true,
                           "segment 2 inside of hit area should have \"foo\" set");
  test::assert_equal<bool>(features[segments[3]]->has_key("foo"), true,
                           "segment 3 inside of hit area should have \"foo\" set");

  // and they should be set to the right values...
  test::assert_equal<std::string>(features[segments[1]]->get("foo").to_string(),
                                  "first_value", "segment 1 should have first value");
  test::assert_equal<std::string>(features[segments[2]]->get("foo").to_string(),
                                  "first_value|second_value",
                                  "segment 2 should have both values");
  test::assert_equal<std::string>(features[segments[3]]->get("foo").to_string(),
                                  "second_value", "segment 3 should have second value");
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

  RUN_TEST(test_intersection_mode_first);
  RUN_TEST(test_intersection_mode_collect);
  RUN_TEST(test_intersection_mode_split);
  RUN_TEST(test_intersection_mode_split_first);
  RUN_TEST(test_intersection_mode_split_collect);

  RUN_TEST(test_poly_inner_inclusion_param);
  RUN_TEST(test_poly_inner_exclusion_param);
  
  std::cout << " >> Tests failed: " << tests_failed << std::endl << std::endl;

  return (tests_failed > 0) ? 1 : 0;
}
