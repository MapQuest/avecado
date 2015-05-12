//
// mapnik_handler_factory.cpp
// ~~~~~~~~~~~~~~~~~~~
//

#include "http_server/mapnik_handler_factory.hpp"
#include "http_server/mapnik_request_handler.hpp"

namespace http {
namespace server3 {

mapnik_handler_factory::mapnik_handler_factory(const mapnik_server_options &opts)
  : options_(opts) {
}

mapnik_handler_factory::~mapnik_handler_factory() {
}

void mapnik_handler_factory::thread_setup(boost::thread_specific_ptr<request_handler> &ptr, const std::string &port) {
  ptr.reset(new mapnik_request_handler(options_, port));
}

} // namespace server3
} // namespace http
