#include "util_tile.hpp"
#include "vector_tile.pb.h"

namespace avecado { namespace util {

namespace {

struct minmax {
  uint32_t count;
  bool overflow;
  int32_t coords[2];

  minmax() : count(0), overflow(false) {}

  inline void add(int32_t x) {
    bool found = false;
    for (uint32_t i = 0; i < count; ++i) {
      if (x == coords[i]) {
        found = true;
        break;
      }
    }
    if (!found) {
      if (count < 2) {
        coords[count++] = x;
      } else {
        overflow = true;
      }
    }
  }

  bool inside(int32_t extent) {
    for (uint32_t i = 0; i < count; ++i) {
      if ((coords[i] > 0) && (coords[i] < extent)) {
        return true;
      }
    }
    return false;
  }
};

} // anonymous namespace

bool is_interesting(const vector_tile::Tile_Layer &l) {
  // empty features are not interesting
  if (l.features_size() == 0) {
    return false;
  }

  // however, having more than one feature is interesting
  if (l.features_size() > 1) {
    return true;
  }

  // now we know there's one feature, we can see if the
  // geometry is interesting, which means decoding the tile.
  const vector_tile::Tile_Feature &f = l.features(0);

  const int32_t extent = l.extent();
  const uint32_t geometry_size = f.geometry_size();
  static const uint32_t cmd_bits = 3;

  uint32_t repeat = 0, cmd = 0;
  int32_t x = 0, y = 0;
  minmax xm, ym;

  for (uint32_t i = 0; i < geometry_size; ) {
    // no command repeats left, so the next item must be a
    // command.
    if (repeat == 0) {
      uint32_t entry = f.geometry(i++);
      cmd = entry & ((1 << cmd_bits) - 1);
      repeat = entry >> cmd_bits;

    } else {
      // only defined commands are 1 = move to, 2 = line to and
      // 7 = close path.
      if ((cmd == 1) || (cmd == 2)) {
        int32_t dx = f.geometry(i++);
        int32_t dy = f.geometry(i++);
        dx = ((dx >> 1) ^ (-(dx & 1)));
        dy = ((dy >> 1) ^ (-(dy & 1)));
        x += dx;
        y += dy;
        xm.add(x);
        ym.add(y);
      }
      // else it would be a close, which hits an existing point
      // anyway.
      --repeat;
    }
  }

  // if we only had two coords for both x and y, and they're
  // outside the extent, then yay, it covers the whole of the
  // extent and the layer is uninteresting.
  if (xm.overflow || ym.overflow) {
    // more than 2 coords in either x or y -> interesting
    return true;

  } else {
    // coords inside the extent -> interesting
    return xm.inside(extent) || ym.inside(extent);
  }
}

} } // namespace avecado::util
