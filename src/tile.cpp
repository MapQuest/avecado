#include "tile.hpp"
#include "vector_tile.pb.h"

namespace avecado {

tile::tile(int z, int x, int y, int size)
  : m_mapnik_tile(new mapnik::vector::tile),
    m_z(z),
    m_x(x),
    m_y(y),
    m_size(size) {}

tile::~tile() {}

std::string tile::get_data() const {
  std::string buffer;

  if (m_mapnik_tile->SerializeToString(&buffer)) {
    return buffer;

  } else {
    throw std::runtime_error("Error while serializing protocol buffer tile.");
  }
}

} // namespace avecado
