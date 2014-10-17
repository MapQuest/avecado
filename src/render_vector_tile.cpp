#include "avecado.hpp"
#include "vector_tile.pb.h"

#include <mapnik/map.hpp>
#include <mapnik/feature.hpp>
#include <mapnik/graphics.hpp>

#include "mapnik3x_compatibility.hpp"

namespace avecado {

bool render_vector_tile(mapnik::image_32 &image,
                        tile &tile,
                        mapnik::Map const &map,
                        unsigned int z,
                        unsigned int x,
                        unsigned int y) {
  return false;
}

} // namespace avecado
