#include "common.hpp"
#include "post_process/generalizer.hpp"

#include <iostream>

namespace {

} // anonymous namespace

int main() {
  int tests_failed = 0;

  std::cout << "== Testing generalizer ==" << std::endl << std::endl;

#define RUN_TEST(x) { tests_failed += test::run(#x, &(x)); }
  std::cout << " >> Tests failed: " << tests_failed << std::endl << std::endl;

  return (tests_failed > 0) ? 1 : 0;
}
