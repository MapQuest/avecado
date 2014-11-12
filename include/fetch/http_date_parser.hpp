#ifndef HTTP_DATE_PARSER_HPP
#define HTTP_DATE_PARSER_HPP

#include <ctime>
#include <boost/range/iterator_range_core.hpp>

namespace avecado {

/* parse an RFC 2616-compliant date string into an epoch time.
 */
bool parse_http_date(boost::iterator_range<const char *> range, std::time_t &t);
 
} // namespace avecado

#endif // HTTP_DATE_PARSER_HPP
