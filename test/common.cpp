/*------------------------------------------------------------------------------
 *
 *  This file is part of avecado
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
#include <iostream>
#include <mapnik/load_map.hpp>

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
#define WORLD_SIZE (40075016.68)

namespace {

void run_with_output_redirected_to(const string &name, function<void ()> test) {
  // create a file for the output to go to.
  string file_name = string("log/") + name + ".testlog";

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

// get the mercator bounding box for a tile coordinate
mapnik::box2d<double> box_for_tile(int z, int x, int y) {
  const double scale = WORLD_SIZE / double(1 << z);
  const double half_world = 0.5 * WORLD_SIZE;

  return mapnik::box2d<double>(
    x * scale - half_world,
    half_world - (y+1) * scale,
    (x+1) * scale - half_world,
    half_world - y * scale);
}

mapnik::Map make_map(std::string style_file, unsigned tile_resolution, int z, int x, int y) {
  // load map config from disk
  mapnik::Map map;
  mapnik::load_map(map, style_file);

  // setup map parameters
  map.resize(tile_resolution, tile_resolution);
  map.zoom_to_box(box_for_tile(z, x, y));
  return map;
}

std::string to_string(const mapnik::geometry_type& a) {
  std::string result = "{";
  //for each vertex
  double ax=0, ay=1;
  for(size_t i = 0; i < a.size(); ++i) {
    a.vertex(i, &ax, &ay);
    result += (boost::format("[%3.1f, %3.1f],") % ax % ay).str();
  }
  if(result.back() == ',')
    result.pop_back();
  return result + "}";
}

std::string to_string(const mapnik::feature_ptr& a) {
  std::string result = "{{";
  //for each tag value of a
  for(auto& kv : *a) {
    result += (boost::format("[%1%, %2%],") % std::get<0>(kv) % std::get<1>(kv)).str();
  }
  if(result.back() == ',')
    result.pop_back();
  result += "}, ";

  //for each geometry of a
  for(auto& ag : a->paths()) {
    result += to_string(ag) + ",";
  }
  if(result.back() == ',')
      result.pop_back();
  return result + "}";
}

std::string to_string(const std::vector<mapnik::feature_ptr>& a) {
  std::string result = "{";
  //for all features in a
  for(const auto& af : a) {
    result += to_string(af) + ",";
  }
  if(result.back() == ',')
      result.pop_back();
  return result + "}";
}

bool equal_tags(const mapnik::feature_ptr& a, const mapnik::feature_ptr& b) {
  //cant be the same if they dont have the same number of items in them
  if(a->size() != b->size())
    return false;

  //NOTE: not sure how this is implemented underneath, but since the keys are
  //in a map, we should be able to just do one pass over them both and as soon
  //as one isn't equal bail. just to be safe we do the check of all pairs though

  //for each key value tuple of a
  for(auto akv : *a) {
    bool found = false;
    //for each key value tuple of b
    for(auto bkv : *b) {
      //if they are equal we are good, tuple equality should work fine
      if(akv == bkv) {
        found = true;
        break;
      }
    }
    if(!found)
      return false;
  }

  return true;
}

bool equal(const mapnik::geometry_type& a, const mapnik::geometry_type& b) {
  //cant be the same if they dont have the same number of vertices
  if(a.size() != b.size())
    return false;

  //check every vertex
  double ax=0, ay=1, bx=2, by=3;
  for(size_t i = 0; i < a.size(); ++i) {
    a.vertex(i, &ax, &ay);
    b.vertex(i, &bx, &by);
    if(ax != bx || ay != by)
      return false;
  }

  return true;
}

bool equal(const mapnik::feature_ptr& a, const mapnik::feature_ptr& b, const bool match_tags) {
  //cant be the same if they dont have the same number of geometries
  if(a->num_geometries() != b->num_geometries())
    return false;

  //check the tags if we are asked to
  if(match_tags && !equal_tags(a, b))
    return false;

  //for each geometry of a
  for(auto& ag : a->paths()) {
    bool found = false;
    //for each geometry of b
    for(auto& bg : b->paths()) {
      //if they are equal we are good
      if(equal(ag, bg)) {
        found = true;
        break;
      }
    }
    if(!found)
      return false;
  }

  return true;
}

bool equal(const std::vector<mapnik::feature_ptr>& a, const std::vector<mapnik::feature_ptr>& b, const bool match_tags) {
  //cant be the same if they dont have the same number of features
  if(a.size() != b.size())
    return false;

  //for all features in a
  for(auto& af : a) {
    bool found = false;
    //for all features in b
    for(auto& bf : b) {
      //if they are equal we are good
      if(equal(af, bf, match_tags)) {
        found = true;
        break;
      }
    }
    if(!found)
      return false;
  }
  return true;
}

}
