//
// request_handler.hpp
// ~~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2003-2012 Christopher M. Kohlhoff (chris at kohlhoff dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef HTTP_SERVER3_REQUEST_HANDLER_HPP
#define HTTP_SERVER3_REQUEST_HANDLER_HPP

#include <string>
#include <boost/noncopyable.hpp>
#include <boost/optional.hpp>
#include <boost/thread/tss.hpp>

#include "http_server/server_options.hpp"

// forward declaration
namespace mapnik { struct Map; }

namespace http {
namespace server3 {

struct reply;
struct request;

/// The common handler for all incoming requests.
class request_handler
  : private boost::noncopyable
{
public:
  /// Construct with a directory containing files to be served.
  request_handler(const boost::thread_specific_ptr<mapnik::Map> &,
                  const server_options &options);

  /// Handle a request and produce a reply.
  void handle_request(const request& req, reply& rep);

private:
  /// pointer to thread-local copy of the mapnik Map object used to
  /// do the rendering.
  const boost::thread_specific_ptr<mapnik::Map> &map_ptr_;

  /// options, mostly passed to mapnik for making the vector tile
  server_options options_;

  /// Perform URL-decoding on a string. Returns false if the encoding was
  /// invalid.
  static bool url_decode(const std::string& in, std::string& out);
};

} // namespace server3
} // namespace http

#endif // HTTP_SERVER3_REQUEST_HANDLER_HPP
