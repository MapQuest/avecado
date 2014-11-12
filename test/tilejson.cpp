#include "config.h"
#include "common.hpp"
#include "tilejson.hpp"
#include "http_server/server.hpp"
#include "fetcher.hpp"
#include "fetch/overzoom.hpp"

#include <iostream>

namespace bpt = boost::property_tree;

namespace {

void test_tilejson_fetch() {
  char cwd[PATH_MAX];
  if (getcwd(&cwd[0], PATH_MAX) == NULL) { throw std::runtime_error("getcwd failed"); }
  bpt::ptree conf = avecado::tilejson((boost::format("file://%1%/test/tilejson.json") % cwd).str());

  test::assert_equal<int>(conf.get<int>("maskLevel"), 8, "maskLevel");
  test::assert_equal<int>(conf.get<int>("maxzoom"), 15, "maxzoom");
  test::assert_equal<int>(conf.get<int>("minzoom"), 0, "minzoom");
}

// TODO: this is a pretty awful test, and isn't able to dig any deeper than
// the overzoom, not even to inspect it. might need an upgrade to how the
// built-in HTTP server works so that we can do more with these tests...
void test_tilejson_parse() {
  using namespace avecado;

  char cwd[PATH_MAX];
  if (getcwd(&cwd[0], PATH_MAX) == NULL) { throw std::runtime_error("getcwd failed"); }
  bpt::ptree conf = tilejson((boost::format("file://%1%/test/tilejson.json") % cwd).str());

  std::unique_ptr<fetcher> f = make_tilejson_fetcher(conf);
  fetch::overzoom *o = dynamic_cast<fetch::overzoom *>(f.get());
  test::assert_equal<bool>(o != nullptr, true, "is overzoom");
}

} // anonymous namespace

int main() {
  int tests_failed = 0;

  std::cout << "== Testing TileJSON parsing ==" << std::endl << std::endl;

#define RUN_TEST(x) { tests_failed += test::run(#x, &(x)); }
  RUN_TEST(test_tilejson_fetch);
  RUN_TEST(test_tilejson_parse);

  std::cout << " >> Tests failed: " << tests_failed << std::endl << std::endl;

  return (tests_failed > 0) ? 1 : 0;
}
