#ifndef SERVER_OPTIONS_HPP
#define SERVER_OPTIONS_HPP

#include <mapnik/image_scaling.hpp>
#include "post_processor.hpp"

struct server_options {
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
  std::string port;
  unsigned short thread_hint;
  std::shared_ptr<avecado::post_processor> post_processor;
};

#endif /* SERVER_OPTIONS_HPP */
