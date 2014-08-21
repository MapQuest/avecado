#ifndef AVECADO_HPP
#define AVECADO_HPP

#include <memory>
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
  // construct an empty vector tile
  tile();

  ~tile();

  // return the tile contents as PBF
  std::string get_data() const;

private:
  std::unique_ptr<mapnik::vector::tile> m_mapnik_tile;

  friend bool make_vector_tile(tile &, unsigned int, mapnik::Map const&,
                               int, double, unsigned int, unsigned int,
                               unsigned int, const std::string &,
                               mapnik::scaling_method_e, double);

  friend void process_vector_tile(tile &, pt::ptree const&, int);

  friend std::ostream &operator<<(std::ostream &, const tile &);
};

// more efficient output function for zero-copy streams
std::ostream &operator<<(std::ostream &, const tile &);

/**
 * make_vector_tile adds geometry from a mapnik query to a vector
 * tile object.
 *
 * Arguments:
 *
 *   tile
 *     The vector tile to which geometry from the query will be
 *     added.
 *
 *   path_multiplier
 *     Multiplier for pixel coordinates within the vector tile. If
 *     this is set to 1, then coordinates will correspond to integer
 *     pixels. However, a larger number (e.g: 16) is recommended to
 *     improve the visual appearance of vector tiles, especially when
 *     over-zoomed.
 *
 *   map
 *     The mapnik object which contains the settings for the queries
 *     and styles used to build the vector tile.
 *
 *   buffer_size
 *     The size of the buffer in pixels which adds a "border" around
 *     the vector tile for which data is extracted, but is not
 *     intended to be part of the visible area. This exists to handle
 *     the rendering of objects whose visual effects extend beyond
 *     their geometric extent (e.g: lines rendered with a width, but
 *     the geometry has zero width).
 *
 *   scale_factor
 *     Scale factor by which to increase the pixel sizes of rendered items.
 *
 *   offset_x & offset_y
 *     Offset in pixels to add to transformed coordinates. This can be
 *     used to shift the origin of the vector tile to a different
 *     position.
 *
 *   tolerance
 *     If a geometry path has successive points which are closer than
 *     this tolerance in both the x and y directions, then the point
 *     will be dropped. Note that the units are pixels multiplied by
 *     the `path_multiplier`.
 *
 *   image_format
 *     File format used when encoding raster features in the tile
 *     object.
 *
 *   scaling_method
 *     Which method to use when scaling pixels from raster sources.
 *
 *   scale_denominator
 *     Scale denominator to use when rendering features. If <= 0, then
 *     mapnik will choose an appropriate scale based on the request
 *     size.
 *
 * Returns true if the renderer painted, which means that it added
 * some geometry to the vector tile. Returns false if no geometry
 * was added. This can be used to detect empty tiles, which can be
 * used to accelerate hierarchical rendering of tiles by pruning
 * empty branches.
 *
 * Throws an exception if an unrecoverable error was encountered
 * while building the vector tile.
 */
bool make_vector_tile(tile &tile,
                      unsigned int path_multiplier,
                      mapnik::Map const& map,
                      int buffer_size,
                      double scale_factor,
                      unsigned int offset_x,
                      unsigned int offset_y,
                      unsigned int tolerance,
                      const std::string &image_format,
                      mapnik::scaling_method_e scaling_method,
                      double scale_denominator);

/**
 * process_vector_tile looks for post-processing options in the config
 * and runs mergonalizer, generalizer, and/or labelizer on each layer
 * where they are specified
 *
 * Arguments:
 *
 *   tile
 *     The vector tile to process.
 *
 *   config
 *     Configuration tree specifying which processes to run on which
 *     layers at which zoom levels.
 *
 *   zoom_level
 *     Zoom level of the tile.
 *
 * Throws an exception if an unrecoverable error was encountered
 * while reading config options or processing a vector layer.
 */
void process_vector_tile(tile & tile, pt::ptree const& config, int zoom_level);

} // namespace avecado

#endif /* AVECADO_HPP */
