#include "avecado.hpp"
#include "vector_tile.pb.h"

#include <mapnik/map.hpp>
#include <mapnik/feature.hpp>

#include <google/protobuf/io/zero_copy_stream_impl.h>

#include "mapnik3x_compatibility.hpp"
#include "vector_tile_processor.hpp"
#include "backend.hpp"

namespace avecado {

std::ostream &operator<<(std::ostream &out, const tile &t) {
  google::protobuf::io::OstreamOutputStream stream(&out);
  bool write_ok = t.mapnik_tile().SerializeToZeroCopyStream(&stream);

  if (!write_ok) {
    throw std::runtime_error("Unable to write tile to output stream.");
  }

  return out;
}

bool make_vector_tile(tile &tile,
                      unsigned int path_multiplier,
                      mapnik::Map const& map,
                      int buffer_size,
                      double scale_factor,
                      unsigned int offset_x,
                      unsigned int offset_y,
                      unsigned int tolerance,
                      const std::string &image_format,
                      mapnik::scaling_method_e scaling_method,
                      double scale_denominator,
                      boost::optional<const post_processor &> pp) {
  
  typedef backend backend_type;
  typedef mapnik::vector::processor<backend_type> renderer_type;
  
  backend_type backend(tile.mapnik_tile(), path_multiplier, pp);
  
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

} // namespace avecado
