#include "config.h"
#include "common.hpp"
#include "fetch/http.hpp"
#include "logging/logger.hpp"
#include "http_server/server.hpp"
#include "vector_tile.pb.h"

#include <mapnik/datasource_cache.hpp>

#include <iostream>

namespace {

server_options default_options(const std::string &map_file) {
  server_options options;
  options.path_multiplier = 16;
  options.buffer_size = 0;
  options.scale_factor = 1.0;
  options.offset_x = 0;
  options.offset_y = 0;
  options.tolerance = 1;
  options.image_format = "jpeg";
  options.scaling_method = mapnik::SCALING_NEAR;
  options.scale_denominator = 0.0;
  options.thread_hint = 1;
  options.map_file = map_file;
  options.port = "";
  return options;
}

void test_fetch_empty() {
  server_options options = default_options("test/empty_map_file.xml");
  http::server3::server server("localhost", options);
  std::string port = server.port();

  server.run(false);

  avecado::fetch::http fetch((boost::format("http://localhost:%1%") % port).str(), "pbf");
  avecado::fetch_response response(fetch(0, 0, 0));

  server.stop();

  test::assert_equal<bool>(response.is_left(), true, "should fetch tile OK");
  test::assert_equal<int>(response.left()->mapnik_tile().layers_size(), 0, "should have no layers");
}

void test_fetch_single_line() {
  server_options options = default_options("test/single_line.xml");
  http::server3::server server("localhost", options);
  std::string port = server.port();

  server.run(false);

  avecado::fetch::http fetch((boost::format("http://localhost:%1%") % port).str(), "pbf");
  avecado::fetch_response response(fetch(0, 0, 0));

  server.stop();

  test::assert_equal<bool>(response.is_left(), true, "should fetch tile OK");
  test::assert_equal<int>(response.left()->mapnik_tile().layers_size(), 1, "should have one layer");
}

} // anonymous namespace

int main() {
  int tests_failed = 0;

  std::cout << "== Testing HTTP fetching ==" << std::endl << std::endl;

  // need datasource cache set up so that input plugins are available
  // when we parse map XML.
  mapnik::datasource_cache::instance().register_datasources(MAPNIK_DEFAULT_INPUT_PLUGIN_DIR);

#define RUN_TEST(x) { tests_failed += test::run(#x, &(x)); }
  RUN_TEST(test_fetch_empty);
  RUN_TEST(test_fetch_single_line);
  
  std::cout << " >> Tests failed: " << tests_failed << std::endl << std::endl;

  return (tests_failed > 0) ? 1 : 0;
}
