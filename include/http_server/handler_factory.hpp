#ifndef HANDLER_FACTORY_HPP
#define HANDLER_FACTORY_HPP

#include <boost/thread/tss.hpp>
#include "http_server/request_handler.hpp"

namespace http {
namespace server3 {

/* Creates `request_handler` objects.
 *
 * The `request_handler` objects are the ones doing the real work,
 * but since the server supports having multiple threads, sharing
 * resources between them gets to be a real pain. Instead, we
 * create new resouces for each thread by calling the factory's
 * `thread_setup` method.
 */
struct handler_factory : public boost::noncopyable {
  virtual ~handler_factory();

  /// create whatever resources the specific `request_handler`
  /// implementation needs, and assign it to the thread-specific
  /// pointer.
  virtual void thread_setup(boost::thread_specific_ptr<request_handler> &tss, const std::string &port) = 0;
};

} } // namespace http::server3

#endif /* HANDLER_FACTORY_HPP */
