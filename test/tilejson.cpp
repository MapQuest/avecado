#include "config.h"
#include "common.hpp"
#include "tilejson.hpp"
#include "http_server/server.hpp"
#include "fetcher.hpp"
#include "fetch/overzoom.hpp"

#include <boost/xpressive/xpressive.hpp>

#include <iostream>
#include <fstream>

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

void test_tilejson_fetch_gz() {
  char cwd[PATH_MAX];
  if (getcwd(&cwd[0], PATH_MAX) == NULL) { throw std::runtime_error("getcwd failed"); }
  bpt::ptree conf = avecado::tilejson((boost::format("file://%1%/test/tilejson.json.gz") % cwd).str());

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

// utility function to create TileJSON from Mapnik XML.
std::string tile_json_for_xml(const std::string xml) {
  using test::make_map;
  using avecado::make_tilejson;

  char cwd[PATH_MAX];
  if (getcwd(&cwd[0], PATH_MAX) == NULL) { throw std::runtime_error("getcwd failed"); }

  mapnik::Map map = make_map(xml, 256, 0, 0, 0);
  std::string json = make_tilejson(map, (boost::format("file://%1%/") % cwd).str());

  return json;
}

// https://github.com/MapQuest/avecado/issues/54
//
// maxzoom, minzoom & metatile (masklevel, presumably, too) are all
// supposed to be numeric, but are being generated as strings.
//
// the 2.1.0 tilejson spec doesn't specify metatile or masklevel,
// but we'll just assume they're supposed to be integers too.
void test_tilejson_generate_numeric() {
  using namespace boost::xpressive;

  for (auto &xml : {"test/empty_map_file.xml", "test/tilejson_params.xml"}) {
    try {
      // this test is kinda half-assed, what with the regex and all...
      // but it should work to detect the difference between a JSON
      // string and a number.
      const std::string json = tile_json_for_xml(xml);

      for (auto &param : {"metatile", "maskLevel", "minzoom", "maxzoom"}) {
        // first, need to check if the key is present at all. if not,
        // then it doesn't make sense to check that it's a number.
        // NOTE: this is /"${param}" *:/
        sregex is_present = as_xpr('"') >> param >> '"' >> *space >> as_xpr(':');

        if (regex_search(json.begin(), json.end(), is_present)) {
          // if present, we check if the value begins with a valid number
          // prefix: an optional minus sign and then digits. this might
          // match invalid JSON, but the validity of generated JSON is a
          // test we'll have to do elsewhere.
          sregex is_number = as_xpr('"') >> param >> '"' >> *space >> as_xpr(':')
                                         >> *space >> !as_xpr('-') >> +digit;

          if (!regex_search(json.begin(), json.end(), is_number)) {
            throw std::runtime_error(
              (boost::format("Parameter \"%1%\" should be a number, but is not "
                             "in TileJSON: %2%") % param % json).str());
          }
        }
      }

      // Some params need to be arrays of integers
      for (auto &param : {"center", "bounds"}) {
        // The same check as above for presense
        sregex is_present = as_xpr('"') >> param >> '"' >> *space >> as_xpr(':');
        if (regex_search(json.begin(), json.end(), is_present)) {
          sregex is_array = as_xpr('"') >> param >> '"' >> *space >> as_xpr(':')
                                        >> *space >> as_xpr('[');

          if (!regex_search(json.begin(), json.end(), is_array)) {
            throw std::runtime_error(
              (boost::format("Parameter \"%1%\" should be an array of numbers, but is not "
                             "in TileJSON: %2%") % param % json).str());
          }
        }
      }

    } catch (const std::exception &e) {
      std::throw_with_nested(
        std::runtime_error(
          (boost::format("while processing XML file \"%1%\"") % xml).str()));
    }
  }
}

// same as the above test, but we force Mapnik to store strings
// in its parameter list so that we have to convert to numeric
// before outputting to JSON.
void test_tilejson_generate_numeric_force() {
  using namespace boost::xpressive;
  using test::make_map;
  using avecado::make_tilejson;

  char cwd[PATH_MAX];
  if (getcwd(&cwd[0], PATH_MAX) == NULL) { throw std::runtime_error("getcwd failed"); }

  mapnik::Map map = make_map("test/empty_map_file.xml", 256, 0, 0, 0);
  map.get_extra_parameters().emplace("maxzoom", mapnik::value_holder(std::string("0")));

  std::string json = make_tilejson(map, (boost::format("file://%1%/") % cwd).str());

  sregex is_present = as_xpr("\"maxzoom\"") >> *space >> as_xpr(':');
  if (!regex_search(json.begin(), json.end(), is_present)) {
    throw std::runtime_error(
      (boost::format("maxzoom key not in generated JSON: %1%") % json).str());
  }

  sregex is_number = as_xpr("\"maxzoom\"") >> *space >> as_xpr(':')
                                           >> *space >> !as_xpr('-') >> +digit;
  if (!regex_search(json.begin(), json.end(), is_number)) {
    throw std::runtime_error(
      (boost::format("Parameter \"maxzoom\" should be a number, but is not "
                     "in TileJSON: %1%") % json).str());
  }
}

void test_tilejson_generate_masklevel() {
  using test::make_map;
  using avecado::make_tilejson;

  test::temp_dir tmp;
  std::string base_url = (boost::format("file://%1%/") % tmp.path().native()).str();

  mapnik::Map map = make_map("test/empty_map_file.xml", 256, 0, 0, 0);
  for (int maxzoom = 0; maxzoom < 23; ++maxzoom) {
    map.get_extra_parameters().erase("maxzoom");
    map.get_extra_parameters().emplace("maxzoom", mapnik::value_integer(maxzoom));

    const std::string json = make_tilejson(map, base_url);
    const std::string json_file = (tmp.path() / "tile.json").native();
    std::ofstream out(json_file);
    out.write(json.data(), json.size());
    out.close();

    bpt::ptree conf = avecado::tilejson((boost::format("%1%tile.json") % base_url).str());

    test::assert_equal<mapnik::value_integer>(
      map.get_extra_parameters().find("maxzoom")->second.get<mapnik::value_integer>(),
      mapnik::value_integer(maxzoom), "maxzoom parameter");
    test::assert_equal<size_t>(map.get_extra_parameters().count("maskLevel"), 0,
                               "maskLevel presence in original parameters");
    test::assert_equal<int>(conf.get<int>("maxzoom"), maxzoom, "maxzoom");
    test::assert_equal<int>(conf.get<int>("maskLevel"), maxzoom, "maskLevel");
  }
}

} // anonymous namespace

int main() {
  int tests_failed = 0;

  std::cout << "== Testing TileJSON parsing ==" << std::endl << std::endl;

#define RUN_TEST(x) { tests_failed += test::run(#x, &(x)); }
  RUN_TEST(test_tilejson_fetch);
  RUN_TEST(test_tilejson_fetch_gz);
  RUN_TEST(test_tilejson_parse);
  RUN_TEST(test_tilejson_generate_numeric);
  RUN_TEST(test_tilejson_generate_numeric_force);
  RUN_TEST(test_tilejson_generate_masklevel);

  std::cout << " >> Tests failed: " << tests_failed << std::endl << std::endl;

  return (tests_failed > 0) ? 1 : 0;
}
