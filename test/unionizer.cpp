#include "common.hpp"
#include "post_process/unionizer.hpp"

#include <vector>
#include <utility>
#include <iostream>
#include <boost/property_tree/ptree.hpp>
#include <boost/format.hpp>

using namespace std;

namespace {

avecado::post_process::izer_ptr create_unionizer(const string& heuristic, const string& strategy, const size_t iterations,
  const float angle_ratio, const vector<string>& tags, const vector<string>& direction_tags){

  pt::ptree conf;
  conf.put("union_heuristic", heuristic);
  conf.put("tag_strategy", strategy);
  //conf.put("keep_ids_tag", "unioned_ids");
  conf.put("max_iterations", iterations);
  conf.put("angle_union_sample_ratio", angle_ratio);

  pt::ptree tag_array;
  for(const auto& tag : tags) {
    tag_array.add("", tag);
  }
  conf.put_child("match_tags", tag_array);

  pt::ptree directions_array;
  for(const auto& tag : direction_tags) {
    directions_array.add("", tag);
  }
  conf.put_child("preserve_direction_tags", directions_array);

  return avecado::post_process::create_unionizer(conf);
}

void do_test(const avecado::post_process::izer_ptr& izer, vector<mapnik::feature_ptr>& input,
  const vector<mapnik::feature_ptr>& expected, const string& message, const bool show = true){

  //unionize the features in the layer
  izer->process(input, test::make_map("test/empty_map_file.xml", 256, 18, 75344, 98762));
  //check if you got what you paid for
  if(!test::equal(input, expected, true))
  {
    string result;
    if(show) {
      result = test::to_string(input);
    }
    throw runtime_error(message + "\nResult:   " + result + "\nExpected: " + test::to_string(expected));
  }
}

//check if the angle algorithm unions properly
void test_angle() {
  //test that union favors very obtuse angles (low frequency)
  vector<mapnik::feature_ptr> input = {
    test::create_feature({ {-1, 0}, {0, 0} }, {}),
    test::create_feature({ { 0, 0}, {1, 0} }, {}),
    test::create_feature({ {-1, 1}, {0, 0} }, {}),
    test::create_feature({ { 0, 0}, {1, 1} }, {})
  };
  vector<mapnik::feature_ptr> expected = {
    test::create_feature({ {-1, 0}, {0, 0}, {1, 0} }, {}),
    test::create_feature({ {-1, 1}, {0, 0}, {1, 1} }, {})
  };
  avecado::post_process::izer_ptr izer = create_unionizer("obtuse", "drop", 10, .1, {}, {});
  do_test(izer, input, expected, "Obtuse heuristic during union did not produce the expected output");

  //test that union favors very acute angles (high frequency)
  input = {
    test::create_feature({ {-1, 0}, {0, 0} }, {}),
    test::create_feature({ { 0, 0}, {1, 0} }, {}),
    test::create_feature({ {-1, 1}, {0, 0} }, {}),
    test::create_feature({ { 0, 0}, {1, 1} }, {})
  };
  expected = {
    test::create_feature({ {-1, 0}, {0, 0}, {-1, 1} }, {}),
    test::create_feature({ {1, 0}, {0, 0}, {1, 1} }, {})
  };
  izer = create_unionizer("acute", "drop", 10, .1, {}, {});
  do_test(izer, input, expected, "Acute heuristic during union did not produce the expected output");
}

//check if the greedy algorithm unions properly
void test_greedy() {
  //TODO: come up with a case that depends on the order of the input and sorting?
  //perhaps where the angle based heuristics would come up with different answers
}

//check some basic properties of unioning
void test_generic() {
  //check that nothing is unioned
  vector<mapnik::feature_ptr> input = {
    test::create_feature({ {-1, 0}, {0, 0} }, {{"a", "b"}}),
    test::create_feature({ { 0,-1}, {0, 0} }, {{"a", "tunafish"}}),
    test::create_feature({ { 0, 0}, {1, 0} }, {{"a", "c"}}),
    test::create_feature({ { 0, 1}, {0, 0} }, {})
  };
  vector<mapnik::feature_ptr> expected = {
    test::create_feature({ {-1, 0}, {0, 0} }, {{"a", "b"}}),
    test::create_feature({ { 0,-1}, {0, 0} }, {{"a", "tunafish"}}),
    test::create_feature({ { 0, 0}, {1, 0} }, {{"a", "c"}}),
    test::create_feature({ { 0, 1}, {0, 0} }, {})
  };
  avecado::post_process::izer_ptr izer = create_unionizer("greedy", "drop", 1, .1, {"a"}, {});
  do_test(izer, input, expected, "Non-unionable features came out different than when they went in");

  //check that directions are adhered to
  input = {
    test::create_feature({ {-1, 0}, {0, 0} }, {{"oneway", "yes"}}),
    test::create_feature({ { 0,-1}, {0, 0} }, {}),
    test::create_feature({ { 0, 0}, {1, 0} }, {{"oneway", "yes"}}),
    test::create_feature({ { 0, 1}, {0, 0} }, {})
  };
  expected = {
    test::create_feature({ {-1, 0}, {0, 0}, {1, 0} }, {{"oneway", "yes"}}),
    test::create_feature({ { 0,-1}, {0, 0}, { 0, 1} }, {})
  };
  izer = create_unionizer("greedy", "drop", 10, .1, {}, {"oneway"});
  do_test(izer, input, expected, "Direction preserving during union did not produce the expected output");

  //TODO: check multi's make it through unmolested

  //TODO: check self union within a multi

  //check that the tags are dropped on the unioned features
  input = {
    test::create_feature({ {-1, 0}, {0, 0} }, {{"gutes_zeug", "yes"}, {"zusaetzliches_tag", "schrott"}}),
    test::create_feature({ { 0,-1}, {0, 0} }, {{"gutes_zeug", "yes"}})
  };
  expected = {
    test::create_feature({ {-1, 0}, {0, 0}, {0, -1} }, {{"gutes_zeug", "yes"}})
  };
  izer = create_unionizer("greedy", "drop", 10, .1, {"gutes_zeug"}, {});
  do_test(izer, input, expected, "Tag dropping during union did not produce the expected output");

  //check that the right number of unions happen with limited iterations
  input = {
    test::create_feature({ {-1, 0}, {0, 0} }, {}),
    test::create_feature({ { 0,-1}, {0, 0} }, {}),
    test::create_feature({ { 0, 2}, {0, 0} }, {})
  };
  expected = {
    test::create_feature({ {-1, 0}, {0, 0}, {0, -1} }, {}),
    test::create_feature({ { 0, 2}, {0, 0} }, {})
  };
  izer = create_unionizer("greedy", "drop", 10, .1, {}, {});
  do_test(izer, input, expected, "Union was expected to produce 2 features in the layer");

  //TODO: check that the previous ids of all the unioned features are stored within the keep_ids_tag
}

} // anonymous namespace

int main() {
  int tests_failed = 0;

  std::cout << "== Testing unionizer ==" << std::endl << std::endl;

#define RUN_TEST(x) { tests_failed += test::run(#x, &(x)); }

  RUN_TEST(test_generic);

  std::cout << " >> Tests failed: " << tests_failed << std::endl << std::endl;

  return (tests_failed > 0) ? 1 : 0;
}
