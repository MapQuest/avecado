#include "util.hpp"

#define WORLD_SIZE (40075016.68)

namespace avecado { namespace util {

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

} } // namespace avecado::util

