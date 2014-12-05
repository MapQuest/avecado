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
  tile(unsigned int z_, unsigned int x_, unsigned int y_);

  ~tile();

  // Return the tile contents as PBF
  std::string get_data() const;

  // parse the string as PBF to get a tile.
  void from_string(const std::string &str);

  // Return the in-memory structure of the tile.
  mapnik::vector::tile const &mapnik_tile() const;
  mapnik::vector::tile &mapnik_tile();

  // coordinates of this tile
  const unsigned int z, x, y;

private:
  std::unique_ptr<mapnik::vector::tile> m_mapnik_tile;
};

// read the tile from a zero-copy input stream
std::istream &operator>>(std::istream &, tile &);

// more efficient output function for zero-copy streams
std::ostream &operator<<(std::ostream &, const tile &);

} // namespace avecado

#endif // AVECADO_TILE_HPP
