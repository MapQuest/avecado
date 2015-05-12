//
// server.cpp
// ~~~~~~~~~~
//
// Copyright (c) 2003-2012 Christopher M. Kohlhoff (chris at kohlhoff dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "http_server/server.hpp"
#include <boost/thread/thread.hpp>
#include <boost/bind.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/format.hpp>
#include <vector>

// for the Map object destructor
#include <mapnik/map.hpp>
// for mapnik::load_map
#include <mapnik/load_map.hpp>

namespace {
// function to set up the mapnik::Map object on the thread
// immediately after it has been created.
void setup_thread(const boost::shared_ptr<http::server3::handler_factory> &factory,
                  std::string port,
                  boost::thread_specific_ptr<http::server3::request_handler> &ptr,
                  boost::asio::io_service *service,
                  std::exception_ptr &error) {
  try {
    factory->thread_setup(ptr, port);
    service->run();

  } catch (const std::exception &e) {
    std::cerr << "ERROR: Thread terminating due to: " << e.what() << "\n";
    error = std::current_exception();

  } catch (...) {
    std::cerr << "ERROR: Thread terminating due to UNKNOWN ERROR\n";
    error = std::current_exception();
  }
}
}

namespace http {
namespace server3 {

server::server(const std::string& address, const server_options &options)
  : thread_pool_size_(options.thread_hint),
    signals_(io_service_),
    acceptor_(io_service_),
    new_connection_(),
    factory_(options.factory),
    port_(options.port)
{
  using boost::asio::ip::tcp;

  // Register to handle the signals that indicate when the server should exit.
  // It is safe to register for the same signal multiple times in a program,
  // provided all registration for the specified signal is made through Asio.
  signals_.add(SIGINT);
  signals_.add(SIGTERM);
#if defined(SIGQUIT)
  signals_.add(SIGQUIT);
#endif // defined(SIGQUIT)
  signals_.async_wait(boost::bind(&server::handle_stop, this));

  tcp::resolver resolver(io_service_);
  tcp::resolver::query query(address, port_);
  tcp::endpoint endpoint = *resolver.resolve(query);

  // Open the acceptor with the option to reuse the address (i.e. SO_REUSEADDR).
  acceptor_.open(endpoint.protocol());
  acceptor_.set_option(tcp::acceptor::reuse_address(true));
  acceptor_.bind(endpoint);

  // get the actual port bound
  endpoint = acceptor_.local_endpoint();
  port_ = (boost::format("%1%") % endpoint.port()).str();

  // listen on the socket
  acceptor_.listen();

  start_accept();
}

server::~server()
{
}

void server::run(bool include_current_thread)
{
  // Create a set of exception pointers for threads to pass
  // back their errors to the main thread.
  thread_errors_.resize(thread_pool_size_);

  // Create a pool of threads to run all of the io_services.
  for (std::size_t i = (include_current_thread ? 1 : 0);
       i < thread_pool_size_; ++i)
  {
    boost::shared_ptr<boost::thread> thread(
        new boost::thread(
            boost::bind(
                &setup_thread,
                factory_,
                port_,
                boost::ref(thread_specific_ptr_),
                &io_service_,
                boost::ref(thread_errors_[i]))));
    threads_.push_back(thread);
  }

  if (include_current_thread) {
    setup_thread(factory_, port_, boost::ref(thread_specific_ptr_),
                 &io_service_, boost::ref(thread_errors_[0]));
  }

  std::cout << "Server starting on port " << port_
            << ". Tiles should be available on URLs like "
            << "http://localhost:" << port_ << "/0/0/0.pbf" << std::endl;
}

void server::stop()
{
   handle_stop();

   // Wait for all threads in the pool to exit.
   for (std::size_t i = 0; i < threads_.size(); ++i) {
     try {
       threads_[i]->join();

       // NOTE: these don't re-throw because we want to join
       // all the threads, not bail half way through because
       // one of them had an error.
     } catch (const std::exception &e) {
       std::cerr << "ERROR: Failed to join thread due to: " << e.what() << "\n";

     } catch (...) {
       std::cerr << "ERROR: Failed to join thread due to UNKNOWN EXCEPTION.\n";
     }
   }

   // if any thread had an error, re-throw it now.
   for (auto &ptr : thread_errors_) {
     if (ptr) {
       // this ignores subsequent errors, but they should have
       // been printed out in the per-thread catch sections
       // anyway.
       std::rethrow_exception(ptr);
     }
   }
}

void server::start_accept()
{
  new_connection_.reset(new connection(io_service_, thread_specific_ptr_));
  acceptor_.async_accept(new_connection_->socket(),
      boost::bind(&server::handle_accept, this,
        boost::asio::placeholders::error));
}

void server::handle_accept(const boost::system::error_code& e)
{
  if (!e)
  {
    new_connection_->start();
  }

  start_accept();
}

void server::handle_stop()
{
  io_service_.stop();
}

std::string server::port() const {
  return port_;
}

} // namespace server3
} // namespace http
