#include "tile.hpp"
#include "vector_tile.pb.h"

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

} // namespace avecado
