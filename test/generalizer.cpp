#include "common.hpp"
#include "post_process/generalizer.hpp"

#include <boost/property_tree/ptree.hpp>

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

// if you generalize something enough, it should become straight.
void test_generalize_to_straight() {
  namespace pp = avecado::post_process;

  pt::ptree conf;
  // NOTE: i was slightly surprised that this tolerance (basically 2, but
  // somewhere must be < rather than <=) rather than 1.0 was what worked,
  // but in practice it should make little difference.
  conf.put("tolerance", 2.001);
  conf.put("algorithm", "visvalingam-whyatt");
  pp::izer_ptr izer = pp::create_generalizer(conf);

  std::vector<mapnik::feature_ptr> features;
  features.push_back(mk_line({0, 0, 1, 1, 2, 0, 3, 1, 4, 0}));

  izer->process(features);

  test::assert_equal<size_t>(features.size(), 1);
  test::assert_equal<size_t>(features[0]->num_geometries(), 1);
  const mapnik::geometry_type &geom = features[0]->get_geometry(0);
  test::assert_equal<mapnik::geometry_type::types>(geom.type(), mapnik::geometry_type::LineString);
  test::assert_equal<size_t>(geom.size(), 2);
  double x = -1, y = -1;

  test::assert_equal<unsigned int>(geom.vertex(0, &x, &y), mapnik::SEG_MOVETO);
  test::assert_equal<double>(x, 0);
  test::assert_equal<double>(y, 0);

  test::assert_equal<unsigned int>(geom.vertex(1, &x, &y), mapnik::SEG_LINETO);
  test::assert_equal<double>(x, 4);
  test::assert_equal<double>(y, 0);
}

} // anonymous namespace

int main() {
  int tests_failed = 0;

  std::cout << "== Testing generalizer ==" << std::endl << std::endl;

#define RUN_TEST(x) { tests_failed += test::run(#x, &(x)); }
  RUN_TEST(test_generalize_to_straight);

  std::cout << " >> Tests failed: " << tests_failed << std::endl << std::endl;

  return (tests_failed > 0) ? 1 : 0;
}
