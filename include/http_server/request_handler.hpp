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

namespace http {
namespace server3 {

struct reply;
struct request;

/* The common handler interface for all incoming requests.
 * This should be implemented for each type of request you'd like to
 * support. This is currently used for test mocking and vector tiles.
 * See `mapnik_request_handler` for more information. */
struct request_handler : private boost::noncopyable {
  virtual ~request_handler();

  /// Handle a request and produce a reply.
  virtual void handle_request(const request& req, reply& rep) = 0;

  /// Perform URL-decoding on a string. Returns false if the encoding was
  /// invalid.
  static bool url_decode(const std::string& in, std::string& out);
};

} // namespace server3
} // namespace http

#endif // HTTP_SERVER3_REQUEST_HANDLER_HPP
