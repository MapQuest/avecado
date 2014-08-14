#include "avecado.hpp"
#include "vector_tile.pb.h"

#include <mapnik/map.hpp>

#include <google/protobuf/io/zero_copy_stream_impl.h>

#include "mapnik3x_compatibility.hpp"
#include "vector_tile_processor.hpp"
#include "vector_tile_backend_pbf.hpp"

namespace avecado {

tile::tile() 
  : m_mapnik_tile(new mapnik::vector::tile) {
}

tile::~tile() {
}

std::string tile::get_data() const {
  std::string buffer;

  if (m_mapnik_tile->SerializeToString(&buffer)) {
    return buffer;

  } else {
    throw std::runtime_error("Error while serializing protocol buffer tile.");
  }
}

std::ostream &operator<<(std::ostream &out, const tile &t) {
  google::protobuf::io::OstreamOutputStream stream(&out);
  bool write_ok = t.m_mapnik_tile->SerializeToZeroCopyStream(&stream);

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
                      double scale_denominator) {
  
  typedef mapnik::vector::backend_pbf backend_type;
  typedef mapnik::vector::processor<backend_type> renderer_type;
  
  backend_type backend(*tile.m_mapnik_tile, path_multiplier);
  
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
