#ifndef ACCESS_LOGGER_HPP
#define ACCESS_LOGGER_HPP

namespace http { namespace server3 {

struct reply;
struct request;

struct access_logger {
  virtual ~access_logger();
  virtual void log(const request &, const reply &) = 0;
};

} } // namespace http::server3

#endif /* ACCESS_LOGGER_HPP */
