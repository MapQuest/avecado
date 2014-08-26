#include "common.hpp"

#include <iostream>

using std::cout;
using std::endl;

void test_single_point() {
/* This test creates a CSV source with a single point at 0,0,
 * a map file which renders that point, and checks the resulting
 * vector tile for 0/0/0
 */

}

int main() {
  int tests_failed = 0;

  cout << "== Testing make_vector_tile ==" << endl << endl;

#define RUN_TEST(x) { tests_failed += test::run(#x, &(x)); }

  cout << " >> Tests failed: " << tests_failed << endl << endl;

  return (tests_failed > 0) ? 1 : 0;

}
