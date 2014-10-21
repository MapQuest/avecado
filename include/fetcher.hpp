#ifndef FETCHER_HPP
#define FETCHER_HPP

#include "tile.hpp"
#include "either.hpp"

#include <stdexcept>
#include <memory>
#include <boost/optional.hpp>

namespace avecado {

/* Status codes for fetch errors. Because they're an easy short-hand, we
 * model them on HTTP status codes. This means it should be pretty easy
 * to figure out at a glance what the status is when debugging.
 */
enum class fetch_status : std::uint16_t {
  /* for when the request was malformed in some way, e.g: x or y out of
   * range for the given z. */
  bad_request = 400,
  /* requested tile could not be found, possibly it does not exist. */
  not_found = 404,
  /* an unspecified and unexpected kind of error occurred. it may, or
   * may not, be temporary. */
  server_error = 500,
};

/* Describes the error encountered while fetching a tile.
 */
struct fetch_error {
  fetch_status status;
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
