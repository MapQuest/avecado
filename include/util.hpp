#ifndef AVECADO_UTIL_HPP
#define AVECADO_UTIL_HPP

#include <mapnik/box2d.hpp>

namespace avecado { namespace util {

// returns the bounding box in mercator coordinates for a
// conventional z/x/y tile.
mapnik::box2d<double> box_for_tile(int z, int x, int y);

} } // namespace avecado::util

#endif // AVECADO_UTIL_HPP
