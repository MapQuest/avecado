#include "fetcher_io.hpp"

namespace avecado {

std::ostream &operator<<(std::ostream &out, fetch_status status) {
  switch (status) {
  case fetch_status::bad_request:     out << "Bad Request";     break;
  case fetch_status::not_found:       out << "Not Found";       break;
  case fetch_status::server_error:    out << "Server Error";    break;
  case fetch_status::not_implemented: out << "Not Implemented"; break;
  default:
    out << "*** Unknown status ***";
  }
  return out;
}

} // namespace avecado
