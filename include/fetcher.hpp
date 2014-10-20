#ifndef FETCHER_HPP
#define FETCHER_HPP

#include "tile.hpp"
#include "either.hpp"

#include <stdexcept>
#include <memory>
#include <boost/optional.hpp>

namespace avecado {

struct fetch_error {
  // HTTP error status code (useful shorthand, even if the source wasn't HTTP)
  int status;
};

typedef either<std::unique_ptr<tile>, fetch_error> fetch_response;

/* Interface for objects which fetch tiles from sources.
 */
struct fetcher {
   virtual ~fetcher();

   // fetches a tile from the source, returning either a
   // tile which contains the (z, x, y) tile or an error.
   virtual fetch_response operator()(int z, int x, int y) = 0;
};

} // namespace avecado

#endif /* FETCHER_HPP */
