#include "config.h"
#include "common.hpp"
#include "fetch/overzoom.hpp"
#include "logging/logger.hpp"

#include <iostream>

namespace {

struct test_fetcher : public avecado::fetcher {
  int m_min_zoom, m_max_zoom, m_status;

  test_fetcher(int min_zoom, int max_zoom, int status) : m_min_zoom(min_zoom), m_max_zoom(max_zoom), m_status(status) {}
  virtual ~test_fetcher() {}

  avecado::fetch_response operator()(int z, int, int) {
    if ((z >= m_min_zoom) && (z <= m_max_zoom)) {
      std::unique_ptr<avecado::tile> tile(new avecado::tile);
      return avecado::fetch_response(std::move(tile));

    } else {
      avecado::fetch_error err;
      err.status = m_status;
      return avecado::fetch_response(err);
    }
  }
};

void test_fetch_missing() {
  std::unique_ptr<avecado::fetcher> f(new test_fetcher(11, 16, 404));
  avecado::fetch::overzoom o(std::move(f), 18, 12);

  // zoom 19 > max, so will be treated as zoom 18
  test::assert_equal<bool>(o(19, 0, 0).is_left(), true, "z19");
  // zoom 18 & 17 are not present (>16), so will be masked to 12
  test::assert_equal<bool>(o(18, 0, 0).is_left(), true, "z18");
  test::assert_equal<bool>(o(17, 0, 0).is_left(), true, "z17");
  // zooms 16 through 11 are present
  test::assert_equal<bool>(o(16, 0, 0).is_left(), true, "z16");
  test::assert_equal<bool>(o(15, 0, 0).is_left(), true, "z15");
  test::assert_equal<bool>(o(14, 0, 0).is_left(), true, "z14");
  test::assert_equal<bool>(o(13, 0, 0).is_left(), true, "z13");
  test::assert_equal<bool>(o(12, 0, 0).is_left(), true, "z12");
  test::assert_equal<bool>(o(11, 0, 0).is_left(), true, "z11");
  // zoom 10 is not present and won't be masked (<12).
  test::assert_equal<bool>(o(10, 0, 0).is_left(), false, "z10");
}

// like the previous test, except that the fetcher returns an
// error. this should turn off the overzooming behaviour and
// just return the error.
void test_fetch_error() {
  std::unique_ptr<avecado::fetcher> f(new test_fetcher(11, 16, 500));
  avecado::fetch::overzoom o(std::move(f), 18, 12);

  // zooms > 16 will all be errors - no matter whether they can be
  // overzoomed or not.
  test::assert_equal<bool>(o(19, 0, 0).is_left(), false, "z19");
  test::assert_equal<bool>(o(18, 0, 0).is_left(), false, "z18");
  test::assert_equal<bool>(o(17, 0, 0).is_left(), false, "z17");
  // zooms 16 through 11 are present
  test::assert_equal<bool>(o(16, 0, 0).is_left(), true, "z16");
  test::assert_equal<bool>(o(15, 0, 0).is_left(), true, "z15");
  test::assert_equal<bool>(o(14, 0, 0).is_left(), true, "z14");
  test::assert_equal<bool>(o(13, 0, 0).is_left(), true, "z13");
  test::assert_equal<bool>(o(12, 0, 0).is_left(), true, "z12");
  test::assert_equal<bool>(o(11, 0, 0).is_left(), true, "z11");
  // zoom 10 is not present and won't be masked (<12).
  test::assert_equal<bool>(o(10, 0, 0).is_left(), false, "z10");
}

void test_fetch_no_mask() {
  std::unique_ptr<avecado::fetcher> f(new test_fetcher(11, 16, 404));
  avecado::fetch::overzoom o(std::move(f), 18, boost::none);

  test::assert_equal<bool>(o(19, 0, 0).is_left(), false, "z19");
  test::assert_equal<bool>(o(18, 0, 0).is_left(), false, "z18");
  test::assert_equal<bool>(o(17, 0, 0).is_left(), false, "z17");
  // zooms 16 through 11 are present
  test::assert_equal<bool>(o(16, 0, 0).is_left(), true, "z16");
}

void test_fetch_no_mask2() {
  std::unique_ptr<avecado::fetcher> f(new test_fetcher(11, 18, 404));
  avecado::fetch::overzoom o(std::move(f), 18, boost::none);

  test::assert_equal<bool>(o(19, 0, 0).is_left(), true, "z19");
  test::assert_equal<bool>(o(18, 0, 0).is_left(), true, "z18");
  test::assert_equal<bool>(o(17, 0, 0).is_left(), true, "z17");
  // zooms 16 through 11 are present
  test::assert_equal<bool>(o(16, 0, 0).is_left(), true, "z16");
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
