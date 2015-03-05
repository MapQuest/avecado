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
#include "util.hpp"
#include <boost/property_tree/ptree.hpp>
#include <boost/filesystem.hpp>
#include <boost/system/error_code.hpp>
#include <boost/system/system_error.hpp>
#include <exception>
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

namespace {

void run_with_output_redirected_to(const string &name, function<void ()> test) {
  // create a file for the output to go to.
  string file_name = string("log/") + name + ".testlog";

  // run the test
  test();
}

void unwind_nested_exception(std::ostream &out, const std::exception &e) {
  out << e.what();
  try {
    std::rethrow_if_nested(e);

  } catch (const std::exception &nested) {
    out << ". Caused by: ";
    unwind_nested_exception(out, nested);

  } catch (...) {
    out << ". Caused by UNKNOWN EXCEPTION";
  }
}

} // anonymous namespace

namespace test {

int run(const string &name, function<void ()> test) {
  cout << setw(TEST_NAME_WIDTH) << name << flush;
  try {
    run_with_output_redirected_to(name, test);
    cout << "  [PASS]" << endl;
    return 0;

  } catch (const exception &ex) {
    cout << "  [FAIL: ";
    unwind_nested_exception(cout, ex);
    cout << "]" << endl;
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

mapnik::Map make_map(std::string style_file, unsigned tile_resolution, int z, int x, int y) {
  // load map config from disk
  mapnik::Map map;
  mapnik::load_map(map, style_file);

  // setup map parameters
  map.resize(tile_resolution, tile_resolution);
  map.zoom_to_box(avecado::util::box_for_tile(z, x, y));
  return map;
}

std::string to_string(const mapnik::geometry_type& a) {
  std::string result = "{";
  //for each vertex
  double ax=0, ay=1;
  mapnik::vertex_adapter va(a);
  for(size_t i = 0; i < va.size(); ++i) {
    va.vertex(i, &ax, &ay);
    result += (boost::format("[%3.1f, %3.1f],") % ax % ay).str();
  }
  if(result.back() == ',')
    result.pop_back();
  return result + "}";
}

std::string to_string(const mapnik::feature_ptr& a) {
  std::string result = "{{";
  //for each tag value of a
  for(const auto& kv : *a) {
    const mapnik::value& val = std::get<1>(kv);
    //skip any mapnik::value_null
    if(!val.is_null())
      result += (boost::format("[%1%, %2%],") % std::get<0>(kv) % val).str();
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
  //NOTE: we don't care about kv pairs whose values are mapnik::value_null
  //to avoid having worrying about it we simply copy all the non nulls first
  size_t a_count = 0;
  for(const auto& kv : *a) {
    //if we have a non null value here
    const mapnik::value& v = std::get<1>(kv);
    if(!v.is_null()){
      //if b doesnt have it we are done
      if(b->get(std::get<0>(kv)) != v)
        return false;
      //b had it so we remember that
      else
        ++a_count;
    }
  }

  //at this point b had everything a had, but we need to make sure it didn't have extra
  for(const auto& kv : *b) {
    //if we have a non null value here
    const mapnik::value& v = std::get<1>(kv);
    if(!v.is_null()){
      --a_count;
    }
  }

  //if a_count isn't 0 then b had more entries than a and therefore isn't equal
  return a_count == 0;
}

bool equal(const mapnik::geometry_type& a, const mapnik::geometry_type& b) {
  //cant be the same if they dont have the same number of vertices
  if(a.size() != b.size())
    return false;

  //check every vertex
  mapnik::vertex_adapter va(a), vb(b);
  double ax=0, ay=1, bx=2, by=3;
  for(size_t i = 0; i < va.size(); ++i) {
    int cmda = va.vertex(i, &ax, &ay);
    int cmdb = vb.vertex(i, &bx, &by);
    if(cmda != cmdb || ax != bx || ay != by)
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

mapnik::feature_ptr create_multi_feature(const std::vector<std::vector<std::pair<double, double> > >& lines, const std::vector<std::pair<std::string, std::string> >& tags) {
  //a feature to hold the geometries and attribution
  mapnik::context_ptr ctx = std::make_shared<mapnik::context_type>();
  mapnik::feature_ptr feat = std::make_shared<mapnik::feature_impl>(ctx, 0);

  //add the tagging to it
  for(const auto& kv: tags) {
    feat->put_new(kv.first, mapnik::value_unicode_string::fromUTF8(kv.second));
  }

  //make the geom
  for(const auto& line : lines) {
    mapnik::geometry_type *geom = new mapnik::geometry_type(mapnik::geometry_type::LineString);
    mapnik::CommandType cmd = mapnik::SEG_MOVETO;
    for(const auto& p : line) {
      geom->push_vertex(p.first, p.second, cmd);
      cmd = mapnik::SEG_LINETO;
    }
    feat->add_geometry(geom);
  }

  //hand it back
  return feat;
}

mapnik::feature_ptr create_feature(const std::vector<std::pair<double, double> >& line, const std::vector<std::pair<std::string, std::string> >& tags) {
  //a feature to hold the geometries and attribution
  mapnik::context_ptr ctx = std::make_shared<mapnik::context_type>();
  mapnik::feature_ptr feat = std::make_shared<mapnik::feature_impl>(ctx, 0);

  //add the tagging to it
  for(const auto& kv: tags) {
    feat->put_new(kv.first, mapnik::value_unicode_string::fromUTF8(kv.second));
  }

  //make the geom
  mapnik::geometry_type *geom = new mapnik::geometry_type(mapnik::geometry_type::LineString);
  mapnik::CommandType cmd = mapnik::SEG_MOVETO;
  for(const auto& p : line) {
    geom->push_vertex(p.first, p.second, cmd);
    cmd = mapnik::SEG_LINETO;
  }
  feat->add_geometry(geom);

  //hand it back
  return feat;
}

}
