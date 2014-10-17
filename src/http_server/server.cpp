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
#include <vector>

// for the Map object destructor
#include <mapnik/map.hpp>
// for mapnik::load_map
#include <mapnik/load_map.hpp>

namespace {
// function to set up the mapnik::Map object on the thread
// immediately after it has been created.
void setup_thread(std::string map_xml,
                  std::string port,
                  boost::thread_specific_ptr<mapnik::Map> &ptr,
                  boost::asio::io_service *service) {
  std::cout << "Loading mapnik map..." << std::endl;
  ptr.reset(new mapnik::Map);
  mapnik::load_map(*ptr, map_xml);
  std::cout << "Mapnik map loaded." << std::endl;
  std::cout << "Server starting on port " << port
            << ". Tiles should be available on URLs like "
            << "http://localhost:" << port << "/0/0/0.pbf" << std::endl;
  service->run();
}
}

namespace http {
namespace server3 {

server::server(const std::string& address, const server_options &options)
  : thread_pool_size_(options.thread_hint),
    signals_(io_service_),
    acceptor_(io_service_),
    new_connection_(),
    map_xml_(options.map_file),
    port_(options.port),
    thread_specific_ptr_(),
    request_handler_(thread_specific_ptr_, options)
{
  // Register to handle the signals that indicate when the server should exit.
  // It is safe to register for the same signal multiple times in a program,
  // provided all registration for the specified signal is made through Asio.
  signals_.add(SIGINT);
  signals_.add(SIGTERM);
#if defined(SIGQUIT)
  signals_.add(SIGQUIT);
#endif // defined(SIGQUIT)
  signals_.async_wait(boost::bind(&server::handle_stop, this));

  // Open the acceptor with the option to reuse the address (i.e. SO_REUSEADDR).
  boost::asio::ip::tcp::resolver resolver(io_service_);
  boost::asio::ip::tcp::resolver::query query(address, port_);
  boost::asio::ip::tcp::endpoint endpoint = *resolver.resolve(query);
  acceptor_.open(endpoint.protocol());
  acceptor_.set_option(boost::asio::ip::tcp::acceptor::reuse_address(true));
  acceptor_.bind(endpoint);
  acceptor_.listen();

  start_accept();
}

server::~server()
{
}

void server::run(bool include_current_thread)
{
  // Create a pool of threads to run all of the io_services.
  for (std::size_t i = (include_current_thread ? 1 : 0);
       i < thread_pool_size_; ++i)
  {
    boost::shared_ptr<boost::thread> thread(
        new boost::thread(
            boost::bind(
                &setup_thread,
                map_xml_,
                port_,
                boost::ref(thread_specific_ptr_),
                &io_service_)));
    threads_.push_back(thread);
  }

  if (include_current_thread) {
    setup_thread(map_xml_, port_, thread_specific_ptr_, &io_service_);
  }
}

void server::stop()
{
   handle_stop();

   // Wait for all threads in the pool to exit.
   for (std::size_t i = 0; i < threads_.size(); ++i)
      threads_[i]->join();
}

void server::start_accept()
{
  new_connection_.reset(new connection(io_service_, request_handler_));
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

} // namespace server3
} // namespace http
