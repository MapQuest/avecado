#include "fetcher.hpp"

namespace avecado {

request::request(int z_, int x_, int y_)
  : z(z_), x(x_), y(y_),
    etag(boost::none), if_modified_since(boost::none) {
}

fetcher::~fetcher() {
}

} // namespace avecado
