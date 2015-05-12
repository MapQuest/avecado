#ifndef MAPNIK_SERVER_OPTIONS_HPP
#define MAPNIK_SERVER_OPTIONS_HPP

#include <boost/shared_ptr.hpp>
#include <mapnik/image_scaling.hpp>
#include "post_processor.hpp"
#include "http_server/access_logger.hpp"
#include "http_server/handler_factory.hpp"

namespace http {
namespace server3 {

struct mapnik_server_options {
  unsigned int path_multiplier;
  int buffer_size;
  double scale_factor;
  unsigned int offset_x;
  unsigned int offset_y;
  unsigned int tolerance;
  std::string image_format;
  mapnik::scaling_method_e scaling_method;
  double scale_denominator;
  std::string output_file;
  std::string map_file;
  std::shared_ptr<avecado::post_processor> post_processor;
  std::shared_ptr<http::server3::access_logger> logger;
  unsigned int max_age;
  int compression_level;
};

} } // namespace http::server3

#endif /* MAPNIK_SERVER_OPTIONS_HPP */
