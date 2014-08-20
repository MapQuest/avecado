#ifndef HTTP_SERVER3_PARSE_PATH_HPP
#define HTTP_SERVER3_PARSE_PATH_HPP

#include <string>

namespace http {
namespace server3 {

bool parse_path(const std::string &path, int &z, int &x, int &y);

} // namespace server3
} // namespace http

#endif // HTTP_SERVER3_PARSE_PATH_HPP
