#include "config.h"
#include "common.hpp"
#include "fetch/http.hpp"
#include "logging/logger.hpp"
#include "http_server/server.hpp"

#include <iostream>

namespace {

void test_fetch() {
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
  options.map_file = "test/simple_map_file.xml";
  options.port = "";

  http::server3::server server("localhost", options);
  std::string port = server.port();

  server.run(false);

  avecado::fetch::http fetch((boost::format("http://localhost:%1%") % port).str(), "pbf");
  avecado::fetch_response response(fetch(0, 0, 0));

  server.stop();

  test::assert_equal<bool>(response.is_left(), true);
}

} // anonymous namespace

int main() {
  int tests_failed = 0;

  std::cout << "== Testing HTTP fetching ==" << std::endl << std::endl;

#define RUN_TEST(x) { tests_failed += test::run(#x, &(x)); }
  RUN_TEST(test_fetch);
  
  std::cout << " >> Tests failed: " << tests_failed << std::endl << std::endl;

  return (tests_failed > 0) ? 1 : 0;
}
