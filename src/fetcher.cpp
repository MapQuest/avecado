#include "fetcher.hpp"

namespace avecado {

request::request(int z_, int x_, int y_)
  : z(z_), x(x_), y(y_) {
}

fetcher::~fetcher() {
}

} // namespace avecado
