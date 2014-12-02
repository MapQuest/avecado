#ifndef AVECADO_UTIL_TILE_HPP
#define AVECADO_UTIL_TILE_HPP

#include "tile.hpp"

namespace mapnik { namespace vector { struct tile_layer; } }

namespace avecado { namespace util {

/* returns false if the layer completely covers the tile, or
 * is completely absent from the tile.
 *
 * this is supposed to help when deciding whether or not to
 * generate a subtree of tiles - if all layers of the parent
 * tile are not interesting, then it is assumed that the same
 * will be true of all descendants and the subtree can be
 * skipped.
 */
bool is_interesting(const mapnik::vector::tile_layer &);

} } // namespace avecado::util

#endif // AVECADO_UTIL_TILE_HPP
