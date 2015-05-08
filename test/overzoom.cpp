#include "config.h"
#include "common.hpp"
#include "fetch/overzoom.hpp"
#include "logging/logger.hpp"

#include <iostream>

namespace {

struct test_fetcher : public avecado::fetcher {
  int m_min_zoom, m_max_zoom;
  avecado::fetch_status m_status;

  test_fetcher(int min_zoom, int max_zoom, avecado::fetch_status status) : m_min_zoom(min_zoom), m_max_zoom(max_zoom), m_status(status) {}
  virtual ~test_fetcher() {}

  std::future<avecado::fetch_response> operator()(const avecado::request &r) {
    std::promise<avecado::fetch_response> response;

    if ((r.z >= m_min_zoom) && (r.z <= m_max_zoom)) {
      std::unique_ptr<avecado::tile> tile(new avecado::tile(r.z, r.x, r.y));
      response.set_value(avecado::fetch_response(std::move(tile)));

    } else {
      avecado::fetch_error err;
      err.status = m_status;
      response.set_value(avecado::fetch_response(err));
    }

    return response.get_future();
  }
};

void check_tile(avecado::fetch::overzoom &o, int z, int x, int y, bool expected, const std::string &msg) {
  test::assert_equal<bool>(o(avecado::request(z, x, y)).get().is_left(), expected, msg);
}

void test_fetch_missing() {
  std::unique_ptr<avecado::fetcher> f(new test_fetcher(11, 16, avecado::fetch_status::not_found));
  avecado::fetch::overzoom o(std::move(f), 18, 12);

  // zoom 19 > max, so will be treated as zoom 18
  check_tile(o, 19, 0, 0, true, "z19");
  // zoom 18 & 17 are not present (>16), so will be masked to 12
  check_tile(o, 18, 0, 0, true, "z18");
  check_tile(o, 17, 0, 0, true, "z17");
  // zooms 16 through 11 are present
  check_tile(o, 16, 0, 0, true, "z16");
  check_tile(o, 15, 0, 0, true, "z15");
  check_tile(o, 14, 0, 0, true, "z14");
  check_tile(o, 13, 0, 0, true, "z13");
  check_tile(o, 12, 0, 0, true, "z12");
  check_tile(o, 11, 0, 0, true, "z11");
  // zoom 10 is not present and won't be masked (<12).
  check_tile(o, 10, 0, 0, false, "z10");
}

// like the previous test, except that the fetcher returns an
// error. this should turn off the overzooming behaviour and
// just return the error.
void test_fetch_error() {
  std::unique_ptr<avecado::fetcher> f(new test_fetcher(11, 16, avecado::fetch_status::server_error));
  avecado::fetch::overzoom o(std::move(f), 18, 12);

  // zooms > 16 will all be errors - no matter whether they can be
  // overzoomed or not.
  check_tile(o, 19, 0, 0, false, "z19");
  check_tile(o, 18, 0, 0, false, "z18");
  check_tile(o, 17, 0, 0, false, "z17");
  // zooms 16 through 11 are present
  check_tile(o, 16, 0, 0, true, "z16");
  check_tile(o, 15, 0, 0, true, "z15");
  check_tile(o, 14, 0, 0, true, "z14");
  check_tile(o, 13, 0, 0, true, "z13");
  check_tile(o, 12, 0, 0, true, "z12");
  check_tile(o, 11, 0, 0, true, "z11");
  // zoom 10 is not present and won't be masked (<12).
  check_tile(o, 10, 0, 0, false, "z10");
}

void test_fetch_no_mask() {
  std::unique_ptr<avecado::fetcher> f(new test_fetcher(11, 16, avecado::fetch_status::not_found));
  avecado::fetch::overzoom o(std::move(f), 18, boost::none);

  check_tile(o, 19, 0, 0, false, "z19");
  check_tile(o, 18, 0, 0, false, "z18");
  check_tile(o, 17, 0, 0, false, "z17");
  // zooms 16 through 11 are present
  check_tile(o, 16, 0, 0, true, "z16");
}

void test_fetch_no_mask2() {
  std::unique_ptr<avecado::fetcher> f(new test_fetcher(11, 18, avecado::fetch_status::not_found));
  avecado::fetch::overzoom o(std::move(f), 18, boost::none);

  check_tile(o, 19, 0, 0, true, "z19");
  check_tile(o, 18, 0, 0, true, "z18");
  check_tile(o, 17, 0, 0, true, "z17");
  // zooms 16 through 11 are present
  check_tile(o, 16, 0, 0, true, "z16");
}

} // anonymous namespace

int main() {
  int tests_failed = 0;

  std::cout << "== Testing overzooming ==" << std::endl << std::endl;

#define RUN_TEST(x) { tests_failed += test::run(#x, &(x)); }
  RUN_TEST(test_fetch_missing);
  RUN_TEST(test_fetch_error);
  RUN_TEST(test_fetch_no_mask);
  RUN_TEST(test_fetch_no_mask2);
  
  std::cout << " >> Tests failed: " << tests_failed << std::endl << std::endl;

  return (tests_failed > 0) ? 1 : 0;
}
