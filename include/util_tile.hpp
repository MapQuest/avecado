#ifndef AVECADO_UTIL_TILE_HPP
#define AVECADO_UTIL_TILE_HPP

#include "tile.hpp"

namespace mapnik { namespace vector { struct tile_layer; } }

namespace avecado { namespace util {

/* returns true if the layer completely covers the tile.
 */
bool is_complete_cover(const mapnik::vector::tile_layer &);

} } // namespace avecado::util

#endif // AVECADO_UTIL_TILE_HPP
