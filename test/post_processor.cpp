#include "common.hpp"
#include "post_processor.hpp"

#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>

#include <iostream>

namespace {

mapnik::feature_ptr mk_line() {
  mapnik::geometry_type *geom = new mapnik::geometry_type(mapnik::geometry_type::LineString);
  geom->push_vertex(0, 0, mapnik::SEG_MOVETO);
  geom->push_vertex(1, 1, mapnik::SEG_LINETO);
  geom->push_vertex(3, 1, mapnik::SEG_LINETO);
  mapnik::context_ptr ctx = std::make_shared<mapnik::context_type>();
  mapnik::feature_ptr feat = std::make_shared<mapnik::feature_impl>(ctx, 0);
  feat->add_geometry(geom);
  return feat;
}

//return true if the izer was run (changed the geometry)
size_t try_zoom(const avecado::post_processor& processor, int z) {
  std::vector<mapnik::feature_ptr> input;
  input.push_back(mk_line());

  mapnik::Map mapnik_map = test::make_map("test/empty_map_file.xml", 256, z, 0, 0);
  return processor.process_layer(input, "test_layer", mapnik_map);
}


//check if the zooms are getting picked up from config properly
void test_zooms() {
  const size_t max_zoom = 22;

  //do a bunch of random iterations
  for(size_t j = 0; j < 100; ++j) {
    //randomly get high and low
    unsigned int high = rand() % max_zoom;
    unsigned int low = rand() % max_zoom;
    if(low > high)
      std::swap(high, low);

    //load the config
    std::ostringstream json;
    json << "{ \"test_layer\": [ { \"minzoom\": ";
    json << low;
    json << ", \"maxzoom\": ";
    json << high;
    json << ", \"process\": [{ \"type\": \"generalizer\", \"tolerance\": 2.001, \"algorithm\": \"visvalingam-whyatt\" }] } ] }";
    std::istringstream is(json.str());
    boost::property_tree::ptree conf;
    boost::property_tree::read_json(is, conf);
    avecado::post_processor processor;
    processor.load(conf);

    //try some zooms, only 4-7 inclusive should do anything
    for(size_t i = 0; i < max_zoom; ++i) {
      //we expect these to return one izer has been run on them
      if(i >= low && i <= high)
        test::assert_equal<size_t>(try_zoom(processor, i), 1);
      //we expect no izer's to be run on these zooms
      else
        test::assert_equal<size_t>(try_zoom(processor, i), 0);
    }
  }
}

} // anonymous namespace

int main() {
  int tests_failed = 0;

  std::cout << "== Testing post processor ==" << std::endl << std::endl;

#define RUN_TEST(x) { tests_failed += test::run(#x, &(x)); }

  RUN_TEST(test_zooms);

  std::cout << " >> Tests failed: " << tests_failed << std::endl << std::endl;

  return (tests_failed > 0) ? 1 : 0;
}
