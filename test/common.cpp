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

#include "common.hpp"
#include "../logging/logger.hpp"
#include <boost/property_tree/ptree.hpp>
#include <boost/filesystem.hpp>
#include <boost/system/error_code.hpp>
#include <boost/system/system_error.hpp>
#include <stdexcept>
#include <iomanip>


#ifdef HAVE_BLACKBOX
#include <logging/bb_logger.hpp>
#endif

using boost::function;
using std::runtime_error;
using std::exception;
using std::cout;
using std::clog;
using std::cerr;
using std::endl;
using std::setw;
using std::flush;
using std::string;
using std::vector;
namespace fs = boost::filesystem;

#define TEST_NAME_WIDTH (45)

namespace {

void run_with_output_redirected_to(const string &name, function<void ()> test) {
  // create a file for the output to go to.
  string file_name = string("log/") + name + ".testlog";
  boost::property_tree::ptree conf;
  conf.put("type", "file");
  conf.put("location", file_name);
  rendermq::log::configure(conf);

#ifdef HAVE_BLACKBOX
  ::blackbox::logging::log::configure(conf);  
#endif

  // run the test
  test();
}

}

namespace test {

int run(const string &name, function<void ()> test) {
  cout << setw(TEST_NAME_WIDTH) << name << flush;
  try {
    run_with_output_redirected_to(name, test);
    cout << "  [PASS]" << endl;
    return 0;

  } catch (const exception &ex) {
    cout << "  [FAIL: " << ex.what() << "]" << endl;
    return 1;

  } catch (...) {
    cerr << "  [FAIL: Unexpected error]" << endl;
    throw;
  }
}

json::json() : m_type(json::type_NONE) {}
json::json(const json &j) : m_type(j.m_type), m_buf(j.m_buf.str()) {}

std::ostream &operator<<(std::ostream &out, const json &j) {
   if (j.m_type == json::type_NONE) {
      out << "null";

   } else {
      out << j.m_buf.str();
      if (j.m_type == json::type_DICT) {
         out << "}";
      } else {
         out << "]";
      }
   }
   return out;
}

void json::quote(const json &j) { m_buf << j; }
void json::quote(const std::string &s) { m_buf << "\"" << s << "\""; }
void json::quote(int i) { m_buf << i; }
void json::quote(double d) { m_buf << d; }

temp_dir::temp_dir()
   : m_path(fs::temp_directory_path() / fs::unique_path("nms-test-%%%%-%%%%-%%%%-%%%%")) {
   fs::create_directories(m_path);
}

temp_dir::~temp_dir() {
   // files might be deleted by other things while this is running
   // (0MQ context thread, i'm looking at you), and this isn't
   // necessarily an error as far as this code is concerned - it
   // just wants to delete everything underneath the temporary
   // directory.
   boost::system::error_code err;

   // catch all errors - we don't want to throw in the destructor
   try {
      // but loop while the path exists and the errors are
      // ignorable.
      while (fs::exists(m_path)) {
         fs::remove_all(m_path, err);

         // for any non-ignorable error, there's not much we can
         // do from the destructor except complain loudly.
         if (err && (err != boost::system::errc::no_such_file_or_directory)) {
            LOG_WARNING(boost::format("Unable to remove temporary "
                                      "directory %1%: %2%")
                        % m_path % err.message());
            break;
         }
      }

   } catch (const std::exception &e) {
      LOG_ERROR(boost::format("Exception caught while trying to remove "
                              "temporary directory %1%: %2%")
                % m_path % e.what());

   } catch (...) {
      LOG_ERROR(boost::format("Unknown exception caught while trying to "
                              "remove temporary directory %1%")
                % m_path);
   }
}

}
