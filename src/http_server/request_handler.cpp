//
// request_handler.cpp
// ~~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2003-2012 Christopher M. Kohlhoff (chris at kohlhoff dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <sstream>
#include <string>
#include <ctime>
#include <chrono>
#include <iomanip>
#include <boost/lexical_cast.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/format.hpp>

#include "http_server/request_handler.hpp"
#include "http_server/parse_path.hpp"
#include "http_server/reply.hpp"
#include "http_server/request.hpp"

// for vector tile creation
#include "avecado.hpp"
#include "tilejson.hpp"
#include "util.hpp"

namespace {
std::string make_http_date() {
  std::stringstream out;
  std::time_t t = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
  struct tm tt;
  gmtime_r(&t, &tt);
  char *oldlocale = setlocale(LC_TIME, NULL);
  setlocale(LC_TIME, "C");
  char buf[30];
  strftime(buf, 30, "%a, %d %b %Y %H:%M:%S GMT", &tt);
  setlocale(LC_TIME, oldlocale);
  return std::string(buf);
}

std::string strip_query_params(const std::string &str) {
  return str.substr(0, str.find('?'));
}
} // anonymous namespace

namespace http {
namespace server3 {

request_handler::~request_handler() {
}

bool request_handler::url_decode(const std::string& in, std::string& out) {
  out.clear();
  out.reserve(in.size());
  for (std::size_t i = 0; i < in.size(); ++i) {
    if (in[i] == '%') {
      if (i + 3 <= in.size()) {
        int value = 0;
        std::istringstream is(in.substr(i + 1, 2));
        if (is >> std::hex >> value) {
          out += static_cast<char>(value);
          i += 2;

        } else {
          return false;
        }

      } else {
        return false;
      }

    } else if (in[i] == '+') {
      out += ' ';

    } else {
      out += in[i];
    }
  }
  return true;
}

} // namespace server3
} // namespace http
