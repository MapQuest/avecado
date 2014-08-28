/*------------------------------------------------------------------------------
 *
 *  This file is part of rendermq  
 *
 *  Author: matt.amos@mapquest.com
 *
 *  Copyright 2010-1 Mapquest, Inc.  All Rights reserved.
 *
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 *-----------------------------------------------------------------------------*/

#include <string>
#include <sstream>
#include <boost/function.hpp>
#include <boost/format.hpp>
#include <boost/noncopyable.hpp>
#include <boost/filesystem.hpp>

#include <stdexcept>

namespace test{

template <typename T>
void assert_equal(T actual, T expected, std::string message = std::string()) {
   if (actual != expected) {
      throw std::runtime_error((boost::format("%1%: expected=%2%, actual=%3%.")
                                % message % expected % actual).str());
   }
}

template <typename T>
void assert_not_equal(T actual, T expected, std::string message = std::string()) {
   if (actual == expected) {
      throw std::runtime_error((boost::format("%1%: expected=%2%, actual=%3%.")
                                % message % expected % actual).str());
   }
}

template <typename T>
void assert_less_or_equal(T actual, T expected, std::string message = std::string()) {
   if (actual > expected) {
      throw std::runtime_error((boost::format("%1%: expected=%2%, actual=%3%.")
                                % message % expected % actual).str());
   }
}

template <typename T>
void assert_greater_or_equal(T actual, T expected, std::string message = std::string()) {
   if (actual < expected) {
      throw std::runtime_error((boost::format("%1%: expected=%2%, actual=%3%.")
                                % message % expected % actual).str());
   }
}


/* runs the test function, formats the output nicely and returns 1
 * if the test failed.
 */
int run(const std::string &name, boost::function<void ()> test);

/* a DSL to make JSON format files. this is nicer than simply
 * quoting the JSON file because C++ lacks heredoc support and
 * uses the same quote character as JSON, so the quoted strings
 * end up looking really ugly.
 */
struct json {
   enum type { type_NONE, type_DICT, type_LIST };

   json();
   json(const json &j);

   /* use operator() to add dictionary key-value entries.
    */
   template <typename T>
   json &operator()(const std::string &key, const T &t) {
      bool first = false;
      if (m_type == type_NONE) { first = true; m_type = type_DICT; }
      if (m_type != type_DICT) { throw std::runtime_error("Mixed type in JSON: expecting DICT."); }
      if (first) { m_buf << "{"; } else { m_buf << ","; }
      m_buf << "\"" << key << "\":";
      quote(t);
      return *this;
   }

   /* use operator[] to add list entries.
    */
   template <typename T>
   json &operator[](const T &t) {
      bool first = false;
      if (m_type == type_NONE) { first = true; m_type = type_LIST; }
      if (m_type != type_LIST) { throw std::runtime_error("Mixed type in JSON: expecting LIST."); }
      if (first) { m_buf << "["; } else { m_buf << ","; }
      quote(t);
      return *this;
   }

   friend std::ostream &operator<<(std::ostream &, const json &);

private:

   void quote(const json &);
   void quote(const std::string &);
   void quote(int);
   void quote(double);

   type m_type;
   std::ostringstream m_buf;
};

std::ostream &operator<<(std::ostream &, const json &);

/* an RAII temporary directory.
 *
 * on construction, creates a temporary directory. the path to it
 * is available via the path() accessor. upon destruction, it will
 * recursively delete the whole temporary directory tree.
 */
struct temp_dir : boost::noncopyable {
   temp_dir();
   ~temp_dir();
   inline boost::filesystem::path path() const { return m_path; }
private:
   boost::filesystem::path m_path;
};

} // namespace test
