#include "tile.hpp"
#include "vector_tile.pb.h"

namespace avecado {

tile::tile(unsigned int z_, unsigned int x_, unsigned int y_)
  : z(z_), x(x_), y(y_), m_mapnik_tile(new mapnik::vector::tile) {
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

mapnik::vector::tile const &tile::mapnik_tile() const {
  return *m_mapnik_tile;
}

mapnik::vector::tile &tile::mapnik_tile() {
  return *m_mapnik_tile;
}

} // namespace avecado
