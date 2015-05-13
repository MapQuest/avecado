//
// mapnik_request_handler.hpp
// ~~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2003-2012 Christopher M. Kohlhoff (chris at kohlhoff dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef HTTP_SERVER3_MAPNIK_REQUEST_HANDLER_HPP
#define HTTP_SERVER3_MAPNIK_REQUEST_HANDLER_HPP

#include <string>
#include <boost/noncopyable.hpp>
#include <boost/optional.hpp>
#include <boost/thread/tss.hpp>

#include "http_server/mapnik_server_options.hpp"
#include <mapnik/map.hpp>

namespace http {
namespace server3 {

struct reply;
struct request;

/// The handler for mapnik vector tile creation & tilejson
class mapnik_request_handler
  : public request_handler
{
public:
  /// Construct with a directory containing files to be served.
  mapnik_request_handler(const mapnik_server_options &options, std::string port);

  /// Handle a request and produce a reply.
  void handle_request(const request& req, reply& rep);

private:
  /// pointer to thread-local copy of the mapnik Map object used to
  /// do the rendering.
  mapnik::Map map_;

  /// options, mostly passed to mapnik for making the vector tile
  mapnik_server_options options_;

  /// the port that the server is running on. used for inserting the
  /// URL of the server into the TileJSON.
  std::string port_;
  
  /// max-age header directive to use. pre-rendered to a string.
  std::string max_age_value_;

  /// Implementation detail of handling a request and producing a reply.
  void handle_request_impl(const request& req, reply& rep);

  /// Handle request for TileJSON.
  void handle_request_json(const request &req, reply &rep);

  /// Handle request for a tile.
  void handle_request_tile(const request &req, reply &rep,
                           const std::string &request_path);
};

} // namespace server3
} // namespace http

#endif // HTTP_SERVER3_MAPNIK_REQUEST_HANDLER_HPP
