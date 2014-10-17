#include "config.h"
#include "common.hpp"
#include "avecado.hpp"
#include "logging/logger.hpp"

#include <boost/property_tree/ptree.hpp>

#include <mapnik/datasource_cache.hpp>
#include <mapnik/wkt/wkt_factory.hpp>
#include <mapnik/wkt/wkt_grammar.hpp>
#include <mapnik/wkt/wkt_grammar_impl.hpp>

#include "vector_tile.pb.h"

#include <iostream>

namespace {

} // anonymous namespace

int main() {
  int tests_failed = 0;

  std::cout << "== Testing render_vector_tile ==" << std::endl << std::endl;

  // set up mapnik's datasources, as we'll be using them in the
  // tests.
  mapnik::datasource_cache::instance().register_datasources(MAPNIK_DEFAULT_INPUT_PLUGIN_DIR);

#define RUN_TEST(x) { tests_failed += test::run(#x, &(x)); }
  // RUN_TEST(test_);
  
  std::cout << " >> Tests failed: " << tests_failed << std::endl << std::endl;

  return (tests_failed > 0) ? 1 : 0;
}
