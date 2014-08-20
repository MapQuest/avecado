#include "http_server/parse_path.hpp"

#include <boost/lexical_cast.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string/classification.hpp>

namespace http {
namespace server3 {

bool parse_path(const std::string &path, int &z, int &x, int &y)
{
  std::vector<std::string> splits;
  boost::algorithm::split(splits, path, boost::algorithm::is_any_of("/."));
  
  if (splits.size() != 4) {
    return false;
  }

  if (splits[3] != "pbf") {
    return false;
  }

  try {
    z = boost::lexical_cast<int>(splits[0]);
    x = boost::lexical_cast<int>(splits[1]);
    y = boost::lexical_cast<int>(splits[2]);

    return true;

  } catch (...) {
    return false;
  }
}

} }
