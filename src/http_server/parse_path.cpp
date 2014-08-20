#include "http_server/parse_path.hpp"

#include <boost/spirit/include/qi.hpp>
#include <boost/spirit/home/qi/numeric/int.hpp>
#include <boost/spirit/include/qi_int.hpp>
#include <boost/spirit/include/phoenix_core.hpp>
#include <boost/spirit/include/phoenix_operator.hpp>

namespace http {
namespace server3 {

bool parse_path(const std::string &path, int &z, int &x, int &y)
{
  typedef std::string::const_iterator iterator;
  using boost::spirit::int_;
  using boost::spirit::_1;
  using boost::phoenix::ref;

  iterator first = path.begin();
  iterator last = path.end();

  bool result = boost::spirit::qi::phrase_parse(
    first, last,
    ( boost::spirit::qi::int_[ref(z) = _1] >> '/'
      int_[ref(x) = _1] >> '/'
      int_[ref(y) = _1] >> ".pbf"
    ));

  return result && (first == last);
}

} }
