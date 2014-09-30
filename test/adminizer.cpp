#include "config.h"
#include "common.hpp"
#include "post_process/adminizer.hpp"

#include <boost/property_tree/ptree.hpp>

#include <mapnik/datasource_cache.hpp>

#include <iostream>

namespace {

mapnik::feature_ptr mk_line(const std::initializer_list<double> &coords) {
  auto itr = coords.begin();
  auto end = coords.end();
  auto cmd = mapnik::SEG_MOVETO;
  mapnik::geometry_type *geom = new mapnik::geometry_type(mapnik::geometry_type::LineString);

  while (itr != end) {
    double x = *itr++;
    if (itr == end) { break; }
    double y = *itr++;

    geom->push_vertex(x, y, cmd);
    cmd = mapnik::SEG_LINETO;
  }

  mapnik::context_ptr ctx = std::make_shared<mapnik::context_type>();
  mapnik::feature_ptr feat = std::make_shared<mapnik::feature_impl>(ctx, 0);
  feat->add_geometry(geom);
  return feat;
}

// test simple assignment of an object's parameter by inclusion
// in an area.
void test_simple_inclusion_param() {
  namespace pp = avecado::post_process;

  pt::ptree conf;
  conf.put("param_name", "foo");
  conf.put("datasource.type", "csv");
  conf.put("datasource.inline",
           "wkt|foo\n"
           "Polygon((-10.0 -10.0, -10.0 10.0, 10.0 10.0, 10.0 -10.0, -10.0 -10.0))|foo_value\n");
  pp::izer_ptr izer = pp::create_adminizer(conf);

  std::vector<mapnik::feature_ptr> features;
  features.push_back(mk_line({0, 0, 1, 1, 2, 0, 3, 1, 4, 0}));

  izer->process(features);

  test::assert_equal<size_t>(features.size(), 1);

  // being adminized should have added (or overwritten) the 'foo'
  // parameter from the admin polygon.
  test::assert_equal<bool>(features[0]->has_key("foo"), true,
                           "feature should have parameter key \"foo\" after adminizing");
  test::assert_equal<std::string>(features[0]->get("foo").to_string(), "foo_value",
                                  "feature should have parameter from adminizing polygon");

  // being adminized shouldn't have affected this geometry because it's
  // entirely within the admin polygon
  test::assert_equal<size_t>(features[0]->num_geometries(), 1);
  const mapnik::geometry_type &geom = features[0]->get_geometry(0);
  test::assert_equal<mapnik::geometry_type::types>(geom.type(), mapnik::geometry_type::LineString);
  test::assert_equal<size_t>(geom.size(), 5);
  double x = -1, y = -1;

  test::assert_equal<unsigned int>(geom.vertex(0, &x, &y), mapnik::SEG_MOVETO);
  test::assert_equal<double>(x, 0);
  test::assert_equal<double>(y, 0);

  test::assert_equal<unsigned int>(geom.vertex(1, &x, &y), mapnik::SEG_LINETO);
  test::assert_equal<double>(x, 1);
  test::assert_equal<double>(y, 1);

  test::assert_equal<unsigned int>(geom.vertex(2, &x, &y), mapnik::SEG_LINETO);
  test::assert_equal<double>(x, 2);
  test::assert_equal<double>(y, 0);

  test::assert_equal<unsigned int>(geom.vertex(3, &x, &y), mapnik::SEG_LINETO);
  test::assert_equal<double>(x, 3);
  test::assert_equal<double>(y, 1);

  test::assert_equal<unsigned int>(geom.vertex(4, &x, &y), mapnik::SEG_LINETO);
  test::assert_equal<double>(x, 4);
  test::assert_equal<double>(y, 0);
}

} // anonymous namespace

int main() {
  int tests_failed = 0;

  std::cout << "== Testing adminizer ==" << std::endl << std::endl;

  // set up mapnik's datasources, as we'll be using them in the
  // tests.
  mapnik::datasource_cache::instance().register_datasources(MAPNIK_DEFAULT_INPUT_PLUGIN_DIR);

#define RUN_TEST(x) { tests_failed += test::run(#x, &(x)); }
  RUN_TEST(test_simple_inclusion_param);
  
  std::cout << " >> Tests failed: " << tests_failed << std::endl << std::endl;

  return (tests_failed > 0) ? 1 : 0;
}