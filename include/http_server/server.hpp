//
// server.hpp
// ~~~~~~~~~~
//
// Copyright (c) 2003-2012 Christopher M. Kohlhoff (chris at kohlhoff dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef HTTP_SERVER3_SERVER_HPP
#define HTTP_SERVER3_SERVER_HPP

#include <boost/asio.hpp>
#include <string>
#include <vector>
#include <exception>
#include <boost/noncopyable.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/thread.hpp>
#include <boost/optional.hpp>
#include <boost/thread/tss.hpp>
#include "http_server/connection.hpp"
#include "http_server/request_handler.hpp"
#include "http_server/server_options.hpp"

// forward declaration
namespace mapnik { struct Map; }

namespace http {
namespace server3 {

/// The top-level class of the HTTP server.
class server
  : private boost::noncopyable
{
public:
  /// Construct the server to listen on the specified TCP address and port, and
  /// handle requests using handlers created by the option's factory.
  server(const std::string& address, const server_options &options);

  /// server destructor will need to call the mapnik::Map destructor, so
  /// it needs to know the concrete type, which we don't include here.
  ~server();

  /// Run the server's io_service loop.
  void run(bool include_current_thread);

  /// Stop the server's io_service loop.
  void stop();

  /// Return what port the server is accepting connections on.
  std::string port() const;

private:
  /// Initiate an asynchronous accept operation.
  void start_accept();

  /// Handle completion of an asynchronous accept operation.
  void handle_accept(const boost::system::error_code& e);

  /// Handle a request to stop the server.
  void handle_stop();

  /// The number of threads that will call io_service::run().
  std::size_t thread_pool_size_;

  /// The io_service used to perform asynchronous operations.
  boost::asio::io_service io_service_;

  /// The signal_set is used to register for process termination notifications.
  boost::asio::signal_set signals_;

  /// Acceptor used to listen for incoming connections.
  boost::asio::ip::tcp::acceptor acceptor_;

  /// The next connection to be accepted.
  connection_ptr new_connection_;

  /// the configuration for the request handler - also acts as a factory
  /// for creating per-thread instances of request handlers.
  boost::shared_ptr<handler_factory> factory_;

  /// The port to run on
  std::string port_;

  /// thread local storage, so that we can construct objects and re-use
  /// them on threads without having to worry about locking them or having
  /// any sort of pool of objects.
  boost::thread_specific_ptr<request_handler> thread_specific_ptr_;

   /// The thread pool
   std::vector<boost::shared_ptr<boost::thread> > threads_;

   /// Captured errors for the threads
   std::vector<std::exception_ptr> thread_errors_;
};

} // namespace server3
} // namespace http

#endif // HTTP_SERVER3_SERVER_HPP
