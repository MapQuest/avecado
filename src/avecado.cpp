#include "avecado.hpp"

#include <mapnik/map.hpp>

#include "mapnik3x_compatibility.hpp"
#include "vector_tile_processor.hpp"
#include "vector_tile_backend_pbf.hpp"

bool foo(mapnik::vector::tile &tile,
         unsigned int path_multiplier,
         mapnik::Map const& map,
         int buffer_size,
         double scale_factor,
         unsigned int offset_x,
         unsigned int offset_y,
         unsigned int tolerance,
         const std::string &image_format,
         mapnik::scaling_method_e scaling_method,
         double scale_denominator) {
  
  typedef mapnik::vector::backend_pbf backend_type;
  typedef mapnik::vector::processor<backend_type> renderer_type;
  
  backend_type backend(tile, path_multiplier);
  
  mapnik::request request(map.width(),
                          map.height(),
                          map.get_current_extent());
  request.set_buffer_size(buffer_size);
  
  renderer_type ren(backend,
                    map,
                    request,
                    scale_factor,
                    offset_x,
                    offset_y,
                    tolerance,
                    image_format,
                    scaling_method);
  ren.apply(scale_denominator);
  
  return ren.painted();
}

