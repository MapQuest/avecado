#ifndef MAPNIK_HANDLER_FACTORY_HPP
#define MAPNIK_HANDLER_FACTORY_HPP

#include <boost/thread/tss.hpp>
#include "http_server/mapnik_server_options.hpp"
#include "http_server/mapnik_request_handler.hpp"

namespace http {
namespace server3 {

/* Creating vector tiles requires access to a mapnik::Map
 * object, which itself contains many resources. Rather than
 * attempt to share resources between threads, this factor
 * creates a `mapnik_request_handler` for each thread, giving
 * each thread its own independent resources.
 */
struct mapnik_handler_factory : public handler_factory {
  explicit mapnik_handler_factory(const mapnik_server_options &options);
  virtual ~mapnik_handler_factory();

  virtual void thread_setup(boost::thread_specific_ptr<request_handler> &tss, const std::string &port);

private:
  mapnik_server_options options_;
};

} } // namespace http::server3

#endif /* HANDLER_FACTORY_HPP */
