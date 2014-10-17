#ifndef AVECADO_HPP
#define AVECADO_HPP

#include "tile.hpp"
#include "post_processor.hpp"

#include <memory>
#include <boost/optional.hpp>
#include <mapnik/map.hpp>
#include <mapnik/image_scaling.hpp>

// forward declaration of mapnik's raster image type
namespace mapnik { class image_32; }

namespace avecado {

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
 *   post_processor
 *     An optional `post_processor` object to handle geometry
 *     operations ("izers") before the tile is serialised.
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
                      double scale_denominator,
                      boost::optional<const post_processor &> post_processor);

/* TODO: document me.
 */
bool render_vector_tile(mapnik::image_32 &image,
                        tile &tile,
                        mapnik::Map const &map,
                        unsigned int z,
                        unsigned int x,
                        unsigned int y,
                        double scale_factor,
                        unsigned int buffer_size);

} // namespace avecado

#endif /* AVECADO_HPP */
