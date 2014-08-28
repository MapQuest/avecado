#ifndef AVECADO_TILE_HPP
#define AVECADO_TILE_HPP

#include <mapnik/map.hpp>
#include <mapnik/image_scaling.hpp>
#include <boost/property_tree/ptree.hpp>

namespace pt = boost::property_tree;

/* Forward declaration of vector tile type. This type is opaque
 * to users of Avecado, but we expose some methods in the
 * exported vector tile object below. */
namespace mapnik { namespace vector { struct tile; } }

namespace avecado {

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

private:
  std::unique_ptr<mapnik::vector::tile> m_mapnik_tile;

  friend bool make_vector_tile(tile &, unsigned int, mapnik::Map const&,
                               int, double, unsigned int, unsigned int,
                               unsigned int, const std::string &,
                               mapnik::scaling_method_e, double);

  friend std::ostream &operator<<(std::ostream &, const tile &);

  friend class post_processor;
};

} // namespace avecado

#endif // AVECADO_TILE_HPP
