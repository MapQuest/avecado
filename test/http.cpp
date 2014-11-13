#include "config.h"
#include "common.hpp"
#include "fetcher_io.hpp"
#include "tilejson.hpp"
#include "fetch/http.hpp"
#include "logging/logger.hpp"
#include "http_server/server.hpp"
#include "vector_tile.pb.h"

#include <boost/property_tree/ptree.hpp>

#include <mapnik/datasource_cache.hpp>

#include <iostream>

namespace bpt = boost::property_tree;

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

struct server_guard {
  server_options options;
  http::server3::server server;
  std::string port;

  server_guard(const std::string &map_xml)
    : options(default_options(map_xml))
    , server("localhost", options)
    , port(server.port()) {

    server.run(false);
  }

  ~server_guard() {
    server.stop();
  }

  std::string base_url() {
    return (boost::format("http://localhost:%1%") % port).str();
  }
};

void test_fetch_empty() {
  server_guard guard("test/empty_map_file.xml");

  avecado::fetch::http fetch(guard.base_url(), "pbf");
  avecado::fetch_response response(fetch(0, 0, 0).get());

  test::assert_equal<bool>(response.is_left(), true, "should fetch tile OK");
  test::assert_equal<int>(response.left()->mapnik_tile().layers_size(), 0, "should have no layers");
}

void test_fetch_single_line() {
  server_guard guard("test/single_line.xml");

  avecado::fetch::http fetch(guard.base_url(), "pbf");
  avecado::fetch_response response(fetch(0, 0, 0).get());

  test::assert_equal<bool>(response.is_left(), true, "should fetch tile OK");
  test::assert_equal<int>(response.left()->mapnik_tile().layers_size(), 1, "should have one layer");
}

void assert_is_error(avecado::fetch::http &fetch, int z, int x, int y, avecado::fetch_status status) {
  avecado::fetch_response response(fetch(z, x, y).get());
  test::assert_equal<bool>(response.is_right(), true, (boost::format("(%1%, %2%, %3%): response should be failure") % z % x % y).str());
  test::assert_equal<avecado::fetch_status>(response.right().status, status,
                                            (boost::format("(%1%, %2%, %3%): response status is not what was expected") % z % x % y).str());
}

void test_fetch_error_coordinates() {
  using avecado::fetch_status;

  server_guard guard("test/empty_map_file.xml");

  avecado::fetch::http fetch(guard.base_url(), "pbf");

  assert_is_error(fetch, -1, 0, 0, fetch_status::not_found);
  assert_is_error(fetch, 31, 0, 0, fetch_status::not_found);
  assert_is_error(fetch, 0, 0, 1, fetch_status::not_found);
  assert_is_error(fetch, 0, 1, 0, fetch_status::not_found);
  assert_is_error(fetch, 0, 0, -1, fetch_status::not_found);
  assert_is_error(fetch, 0, -1, 0, fetch_status::not_found);
}
 
void test_fetch_error_extension() {
  using avecado::fetch_status;

  server_guard guard("test/empty_map_file.xml");

  avecado::fetch::http fetch(guard.base_url(), "gif");

  assert_is_error(fetch, 0, 0, 0, fetch_status::not_found);
}

void test_fetch_error_path_segments() {
  using avecado::fetch_status;

  server_guard guard("test/empty_map_file.xml");

  avecado::fetch::http fetch(guard.base_url(), "/0.pbf");

  assert_is_error(fetch, 0, 0, 0, fetch_status::not_found);
}

void test_fetch_error_non_numeric() {
  using avecado::fetch_status;

  server_guard guard("test/empty_map_file.xml");

  std::vector<std::string> patterns;
  patterns.push_back((boost::format("%1%/a/b/c.pbf") % guard.base_url()).str());
  avecado::fetch::http fetch(std::move(patterns));

  assert_is_error(fetch, 0, 0, 0, fetch_status::not_found);
}

void test_no_url_patterns_is_error() {
  std::vector<std::string> patterns;
  bool threw = false;

  avecado::fetch::http fetch(std::move(patterns));

  try {
    avecado::fetch_response response(fetch(0, 0, 0).get());
  } catch (...) {
    threw = true;
  }

  test::assert_equal<bool>(threw, true, "Should have thrown exception when patterns was empty.");
}

void test_fetcher_io() {
  using avecado::fetch_status;

  test::assert_equal<std::string>((boost::format("%1%") % fetch_status::bad_request).str(), "Bad Request");
  test::assert_equal<std::string>((boost::format("%1%") % fetch_status::not_found).str(), "Not Found");
  test::assert_equal<std::string>((boost::format("%1%") % fetch_status::server_error).str(), "Server Error");
  test::assert_equal<std::string>((boost::format("%1%") % fetch_status::not_implemented).str(), "Not Implemented");
}

void test_fetch_tilejson() {
  using avecado::fetch_status;

  server_guard guard("test/single_poly.xml");

  bpt::ptree tilejson = avecado::tilejson(
    (boost::format("%1%/tile.json") % guard.base_url()).str());
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
  RUN_TEST(test_fetch_error_coordinates);
  RUN_TEST(test_fetch_error_extension);
  RUN_TEST(test_fetch_error_path_segments);
  RUN_TEST(test_fetch_error_non_numeric);
  RUN_TEST(test_no_url_patterns_is_error);
  RUN_TEST(test_fetcher_io);
  RUN_TEST(test_fetch_tilejson);

  std::cout << " >> Tests failed: " << tests_failed << std::endl << std::endl;

  return (tests_failed > 0) ? 1 : 0;
}
