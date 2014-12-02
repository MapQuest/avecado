#include "config.h"
#include "common.hpp"
#include "fetcher_io.hpp"
#include "fetch/http.hpp"
#include "logging/logger.hpp"
#include "http_server/server.hpp"
#include "vector_tile.pb.h"

#include <mapnik/datasource_cache.hpp>

#include <iostream>

namespace {

struct request_counter : public http::server3::access_logger {
  request_counter() : num_requests(0) {}
  virtual ~request_counter() {}
  virtual void log(const http::server3::request &, const http::server3::reply &) {
    std::unique_lock<std::mutex> lock(mutex);
    ++num_requests;
  }
  std::mutex mutex;
  std::size_t num_requests;
};

server_options default_options(const std::string &map_file, std::shared_ptr<http::server3::access_logger> &logger) {
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
  options.logger = logger;
  options.max_age = 60;
  return options;
}

struct server_guard {
  server_options options;
  http::server3::server server;
  std::string port;

  server_guard(const std::string &map_xml, std::shared_ptr<http::server3::access_logger> logger)
    : options(default_options(map_xml, logger))
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

void test_cache_once() {
  std::shared_ptr<request_counter> counter = std::make_shared<request_counter>();
  {
    server_guard guard("test/empty_map_file.xml", counter);

    avecado::fetch::http fetch(guard.base_url(), "pbf");
    avecado::fetch_response response(fetch(0, 0, 0).get());

    test::assert_equal<bool>(response.is_left(), true, "should fetch tile OK");
    test::assert_equal<int>(response.left()->mapnik_tile().layers_size(), 0, "should have no layers");
  }
  test::assert_equal<std::size_t>(counter->num_requests, 1, "should have one request");
}

void test_cache_twice() {
  std::shared_ptr<request_counter> counter = std::make_shared<request_counter>();
  {
    server_guard guard("test/empty_map_file.xml", counter);
    test::temp_dir dir;

    avecado::fetch::http fetch(guard.base_url(), "pbf");
    fetch.enable_cache((dir.path() / "cache").native());

    {
      avecado::fetch_response response(fetch(0, 0, 0).get());
      test::assert_equal<bool>(response.is_left(), true, "should fetch tile OK");
      test::assert_equal<int>(response.left()->mapnik_tile().layers_size(), 0, "should have no layers");
    }
    {
      avecado::fetch_response response(fetch(0, 0, 0).get());
      test::assert_equal<bool>(response.is_left(), true, "should fetch tile OK");
      test::assert_equal<int>(response.left()->mapnik_tile().layers_size(), 0, "should have no layers");
    }
  }
  test::assert_equal<std::size_t>(counter->num_requests, 1, "should have one request");
}

void test_cache_disable() {
  std::shared_ptr<request_counter> counter = std::make_shared<request_counter>();
  {
    server_guard guard("test/empty_map_file.xml", counter);
    test::temp_dir dir;

    avecado::fetch::http fetch(guard.base_url(), "pbf");
    fetch.enable_cache((dir.path() / "cache").native());

    {
      avecado::fetch_response response(fetch(0, 0, 0).get());
      test::assert_equal<bool>(response.is_left(), true, "should fetch tile OK");
      test::assert_equal<int>(response.left()->mapnik_tile().layers_size(), 0, "should have no layers");
    }

    // disable the cache now and, despite already having the 0/0/0 tile
    // in cache, it should ignore the cache and fetch again.
    fetch.disable_cache();

    {
      avecado::fetch_response response(fetch(0, 0, 0).get());
      test::assert_equal<bool>(response.is_left(), true, "should fetch tile OK");
      test::assert_equal<int>(response.left()->mapnik_tile().layers_size(), 0, "should have no layers");
    }
  }
  test::assert_equal<std::size_t>(counter->num_requests, 2, "should have made two requests");
}

} // anonymous namespace

int main() {
  int tests_failed = 0;

  std::cout << "== Testing HTTP caching ==" << std::endl << std::endl;

  boost::property_tree::ptree conf;
  conf.put("type", "stdout");
  logging::log::configure(conf);

  // need datasource cache set up so that input plugins are available
  // when we parse map XML.
  mapnik::datasource_cache::instance().register_datasources(MAPNIK_DEFAULT_INPUT_PLUGIN_DIR);

#define RUN_TEST(x) { tests_failed += test::run(#x, &(x)); }
  RUN_TEST(test_cache_once);

  // these tests will only work if we have SQLite installed.
#ifdef HAVE_SQLITE3
  RUN_TEST(test_cache_twice);
  RUN_TEST(test_cache_disable);
#endif /* HAVE_SQLITE3 */

  std::cout << " >> Tests failed: " << tests_failed << std::endl << std::endl;

  return (tests_failed > 0) ? 1 : 0;
}
