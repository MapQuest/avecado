#include "fetch/http_date_parser.hpp"
#include <time.h>
#include <curl/curl.h>

namespace avecado {

bool parse_http_date(boost::iterator_range<const char *> range, std::time_t &t) {
  // input range is not necessarily null-terminated, so we need to make sure
  // that it is, unfortunately by copying it.
  char *str = static_cast<char *>(alloca(range.size() + 1));
  memcpy(str, range.begin(), range.size());
  str[range.size()] = '\0';

  // now we can use cURL's date parsing function, which is probably pretty well
  // tested.
  t = curl_getdate(str, nullptr);

  // cURL returns -1 on error.
  return t >= 0;
}

} // namespace avecado
