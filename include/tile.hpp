#ifndef AVECADO_TILE_HPP
#define AVECADO_TILE_HPP

#include <mapnik/map.hpp>
#include <mapnik/image_scaling.hpp>

/* Forward declaration of vector tile type. This type is opaque
 * to users of Avecado, but we expose some methods in the
 * exported vector tile object below. */
namespace mapnik { namespace vector { struct tile; } }

namespace avecado {

class post_processor;

/**
 * Wrapper around the vector tile type, exposing some useful
 * methods but not needing the inclusion of the protobuf header.
 */
class tile {
public:
  // Construct an empty vector tile
  tile();

  ~tile();

  // Return the tile contents as PBF
  std::string get_data() const;

  // Return the in-memory structure of the tile.
  mapnik::vector::tile const &mapnik_tile() const;
  mapnik::vector::tile &mapnik_tile();

private:
  std::unique_ptr<mapnik::vector::tile> m_mapnik_tile;
};

} // namespace avecado

#endif // AVECADO_TILE_HPP
