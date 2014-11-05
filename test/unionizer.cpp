#include "common.hpp"
#include "post_process/unionizer.hpp"

#include <vector>
#include <utility>
#include <iostream>
#include <boost/property_tree/ptree.hpp>

using namespace std;

namespace {

bool assert_equal(const mapnik::feature_ptr& a, const mapnik::feature_ptr& b) {



  return true;
}

mapnik::feature_ptr mk_line(const vector<pair<double, double> >& line, const vector<pair<string, string> >& tags) {
  //make the geom
  mapnik::geometry_type *geom = new mapnik::geometry_type(mapnik::geometry_type::LineString);
  mapnik::CommandType cmd = mapnik::SEG_MOVETO;
  for(const auto& p : line) {
    geom->push_vertex(p.first, p.second, cmd);
    cmd = mapnik::SEG_LINETO;
  }

  //turn it into a feature
  mapnik::context_ptr ctx = std::make_shared<mapnik::context_type>();
  mapnik::feature_ptr feat = std::make_shared<mapnik::feature_impl>(ctx, 0);
  feat->add_geometry(geom);

  //add the tagging to it
  for(const auto& kv: tags) {
    feat->put_new(kv.first, mapnik::value_unicode_string::fromUTF8(kv.second));
  }

  //hand it back
  return feat;
}

void simple_greedy_union(vector<mapnik::feature_ptr>& features, const vector<string>& tags, const vector<string>& direction_tags, const float angle_ratio){
  pt::ptree conf;
  conf.put("union_heuristic", "greedy");
  conf.put("tag_strategy", "drop");
  conf.put("keep_ids_tag", "unioned_ids");
  conf.put("max_iterations", 10);
  conf.put("angle_union_sample_ratio", angle_ratio);

  pt::ptree tag_array;
  for(const auto& tag : tags) {
    tag_array.put_value(tag);
  }
  conf.put_child("match_tags", tag_array);

  pt::ptree directions_array;
  for(const auto& tag : direction_tags) {
    directions_array.put_value(tag);
  }
  conf.put_child("preserve_direction_tags", directions_array);

  avecado::post_process::izer_ptr processor = avecado::post_process::create_unionizer(conf);
}

//check if the angle algorithm unions properly
void test_angle() {
  //TODO: test no unions occur

  //TODO: test correct number of unions occur
}

//check if the greedy algorithm unions properly
void test_greedy() {
  //TODO: test no unions occur

  //TODO: test correct number of unions occur
}

//check some basic properties of unioning
void test_generic() {
  //TODO: check that directions are adhered to

  //TODO: check that the tags are dropped on the unioned features

  //TODO: check that the right number of unions happen with limited iterations

  //TODO: check that the previous ids of all the unioned features are stored within the keep_ids_tag
}

} // anonymous namespace

int main() {
  int tests_failed = 0;

  std::cout << "== Testing unionizer ==" << std::endl << std::endl;

#define RUN_TEST(x) { tests_failed += test::run(#x, &(x)); }

  RUN_TEST(test_greedy);

  std::cout << " >> Tests failed: " << tests_failed << std::endl << std::endl;

  return (tests_failed > 0) ? 1 : 0;
}
