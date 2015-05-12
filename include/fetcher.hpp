#ifndef FETCHER_HPP
#define FETCHER_HPP

#include "tile.hpp"
#include "either.hpp"

#include <stdexcept>
#include <memory>
#include <future>
#include <boost/optional.hpp>
#include <boost/date_time/posix_time/posix_time_types.hpp>

namespace avecado {

/* Status codes for fetches. Because they're an easy short-hand, we
 * model them on HTTP status codes. This means it should be pretty easy
 * to figure out at a glance what the status is when debugging.
 */
enum class fetch_status : std::uint16_t {
  /* for when the requested tile has not been modified. this will only
   * be returned for requests which set either ETag or If-Modified-Since
   */
  not_modified = 304,
  /* for when the request was malformed in some way, e.g: x or y out of
   * range for the given z. */
  bad_request = 400,
  /* requested tile could not be found, possibly it does not exist. */
  not_found = 404,
  /* an unspecified and unexpected kind of error occurred. it may, or
   * may not, be temporary. */
  server_error = 500,
  /* something along the way didn't implement something that was
   * required to complete the request. */
  not_implemented = 501,
};

/* Describes the non-content status encountered while fetching a tile.
 * This isn't necessarily an error: it could be a 304 Not Modified
 * response.
 */
struct fetch_result {
  fetch_status status;
};

typedef either<std::unique_ptr<tile>, fetch_result> fetch_response;

/* Request objects collect together the parameters needed
 * to specify a tile request, such as its (z, x, y)
 * location.
 */
struct request {
  request(int z_, int x_, int y_);

  // Mandatory tile coordinates. z = zoom level, typically from 0
  // to 18 or higher. x & y are coordinates ranging from 0 to 2^z
  // where x=0 is west-most and increases heading east and y=0
  // is north-most and increases heading south.
  int z, x, y;

  // Optional "ETag" header value. If this is present, then it
  // will be checked against ETags stored for the tile and may
  // result in a response with code `fetch_status::not_modified`
  boost::optional<std::string> etag;

  // Optional time of last modificatin. If this value is present
  // then it will be checked against the last modification time
  // of the tile and, if the tile has not been modified since
  // this date, a response code `fetch_status::not_modified`
  // will be returned. Note that ETag is preferred, as the time
  // value here only has granularity to the second, and so may
  // miss updates.
  boost::optional<boost::posix_time::ptime> if_modified_since;
};

/* Interface for objects which fetch tiles from sources.
 */
struct fetcher {
   virtual ~fetcher();

   // fetches a tile from the source, returning either a
   // tile which contains the (z, x, y) tile or an error.
  virtual std::future<fetch_response> operator()(const request &) = 0;
};

} // namespace avecado

#endif /* FETCHER_HPP */
