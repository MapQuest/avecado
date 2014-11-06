#include "common.hpp"
#include "post_process/unionizer.hpp"

#include <vector>
#include <utility>
#include <iostream>
#include <boost/property_tree/ptree.hpp>

using namespace std;

namespace {

bool assert_equal_tags(const mapnik::feature_ptr& a, const mapnik::feature_ptr& b) {
  //cant be the same if they dont have the same number of items in them
  if(a->size() != b->size())
    return false;

  //NOTE: not sure how this is implemented underneath, but since the keys are
  //in a map, we should be able to just do one pass over them both and as soon
  //as one isn't equal bail. just to be safe we do the check of all pairs though

  //for each key value tuple of a
  for(auto akv : *a) {
    bool found = false;
    //for each key value tuple of b
    for(auto bkv : *b) {
      //if they are equal we are good, tuple equality should work fine
      if(akv == bkv) {
        found = true;
        break;
      }
    }
    if(!found)
      return false;
  }

  return true;
}

bool assert_equal(const mapnik::geometry_type& a, const mapnik::geometry_type& b) {
  //cant be the same if they dont have the same number of vertices
  if(a.size() != b.size())
    return false;

  //check every vertex
  double ax=0, ay=1, bx=2, by=3;
  for(size_t i = 0; i < a.size(); ++i) {
    a.vertex(i, &ax, &ay);
    b.vertex(i, &bx, &by);
    if(ax != bx || ay != by)
      return false;
  }

  return true;
}

bool assert_equal(const mapnik::feature_ptr& a, const mapnik::feature_ptr& b, const bool match_tags = false) {
  //cant be the same if they dont have the same number of geometries
  if(a->num_geometries() != b->num_geometries())
    return false;

  //check the tags if we are asked to
  if(match_tags && !assert_equal_tags(a, b))
    return false;

  //for each geometry of a
  for(auto& ag : a->paths()) {
    bool found = false;
    //for each geometry of b
    for(auto& bg : b->paths()) {
      //if they are equal we are good
      if(assert_equal(ag, bg)) {
        found = true;
        break;
      }
    }
    if(!found)
      return false;
  }

  return true;
}

bool assert_equal(const vector<mapnik::feature_ptr>& a, const vector<mapnik::feature_ptr>& b, const bool match_tags = false) {
  //cant be the same if they dont have the same number of features
  if(a.size() != b.size())
    return false;

  //for all features in a
  for(auto af : a) {
    bool found = false;
    //for all features in b
    for(auto bf : b) {
      //if they are equal we are good
      if(assert_equal(af, bf, match_tags)) {
        found = true;
        break;
      }
    }
    if(!found)
      return false;
  }
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

avecado::post_process::izer_ptr creat_unionizer(const string& heuristic, const string& strategy, const size_t iterations,
  const float angle_ratio, const vector<string>& tags, const vector<string>& direction_tags){

  pt::ptree conf;
  conf.put("union_heuristic", heuristic);
  conf.put("tag_strategy", strategy);
  //conf.put("keep_ids_tag", "unioned_ids");
  conf.put("max_iterations", iterations);
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

  return avecado::post_process::create_unionizer(conf);
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
