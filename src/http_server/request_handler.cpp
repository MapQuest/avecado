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
#include <boost/lexical_cast.hpp>
#include <boost/algorithm/string/predicate.hpp>

#include "http_server/request_handler.hpp"
#include "http_server/parse_path.hpp"
#include "http_server/mime_types.hpp"
#include "http_server/reply.hpp"
#include "http_server/request.hpp"

// for vector tile rendering
#include "avecado.hpp"

#define WORLD_SIZE (40075016.68)

namespace {
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
} // anonymous namespace

namespace http {
namespace server3 {

request_handler::request_handler(const boost::thread_specific_ptr<mapnik::Map> &map_ptr,
                                 const server_options &options)
  : map_ptr_(map_ptr),
    options_(options)
{
}

void request_handler::handle_request(const request& req, reply& rep)
{
  // Decode url to path.
  std::string request_path;
  if (!url_decode(req.uri, request_path))
  {
    rep = reply::stock_reply(reply::bad_request);
    return;
  }

  // simple hierarchy is just $z/$x/$y.pbf, in spherical mercator
  // and we don't take account of anything fancy.
  int z, x, y;
  if (!parse_path(request_path, z, x, y)) {
    rep = reply::stock_reply(reply::bad_request);
    return;
  }

  // some sanity checking for z, x, y ranges
  if ((z < 0) || (z > 30)) {
    rep = reply::stock_reply(reply::bad_request);
    return;
  }
  const int max_coord = 1 << z;
  if ((x < 0) || (x >= max_coord)) {
    rep = reply::stock_reply(reply::bad_request);
    return;
  }
  if ((y < 0) || (y >= max_coord)) {
    rep = reply::stock_reply(reply::bad_request);
    return;
  }

  try {
    // setup map parameters
    map_ptr_->resize(256, 256);
    map_ptr_->zoom_to_box(box_for_tile(z, x, y));

    avecado::tile tile;

    // actually making the vector tile
    bool ok = avecado::make_vector_tile(
      tile, options_.path_multiplier, *map_ptr_, options_.buffer_size,
      options_.scale_factor, options_.offset_x, options_.offset_y,
      options_.tolerance, options_.image_format, options_.scaling_method,
      options_.scale_denominator);

    if (!ok) {
      throw std::runtime_error("Unable to make vector tile.");
    }

    // Fill out the reply to be sent to the client.
    rep.status = reply::ok;
    rep.is_hard_error = false;
    rep.content = tile.get_data();
    rep.headers.resize(2);
    rep.headers[0].name = "Content-Length";
    rep.headers[0].value = boost::lexical_cast<std::string>(rep.content.size());
    rep.headers[1].name = "Content-Type";
    rep.headers[1].value = "application/octet-stream";

  } catch (...) {
    rep = reply::stock_reply(reply::internal_server_error);
  }
}

bool request_handler::url_decode(const std::string& in, std::string& out)
{
  out.clear();
  out.reserve(in.size());
  for (std::size_t i = 0; i < in.size(); ++i)
  {
    if (in[i] == '%')
    {
      if (i + 3 <= in.size())
      {
        int value = 0;
        std::istringstream is(in.substr(i + 1, 2));
        if (is >> std::hex >> value)
        {
          out += static_cast<char>(value);
          i += 2;
        }
        else
        {
          return false;
        }
      }
      else
      {
        return false;
      }
    }
    else if (in[i] == '+')
    {
      out += ' ';
    }
    else
    {
      out += in[i];
    }
  }
  return true;
}

} // namespace server3
} // namespace http
