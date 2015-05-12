#ifndef SERVER_OPTIONS_HPP
#define SERVER_OPTIONS_HPP

#include <boost/shared_ptr.hpp>
#include "http_server/handler_factory.hpp"

namespace http {
namespace server3 {

struct server_options {
  std::string port;
  unsigned short thread_hint;
  boost::shared_ptr<handler_factory> factory;
};

} } // namespace http::server3

#endif /* SERVER_OPTIONS_HPP */
