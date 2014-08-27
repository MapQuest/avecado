#include "avecado.hpp"
#include "vector_tile.pb.h"

#include <mapnik/map.hpp>

#include <boost/property_tree/ptree.hpp>

#include <google/protobuf/io/zero_copy_stream_impl.h>

#include "mapnik3x_compatibility.hpp"
#include "vector_tile_processor.hpp"
#include "vector_tile_backend_pbf.hpp"

namespace avecado {

std::ostream &operator<<(std::ostream &out, const tile &t) {
  google::protobuf::io::OstreamOutputStream stream(&out);
  bool write_ok = t.m_mapnik_tile->SerializeToZeroCopyStream(&stream);

  if (!write_ok) {
    throw std::runtime_error("Unable to write tile to output stream.");
  }

  return out;
}

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
                      double scale_denominator) {
  
  typedef mapnik::vector::backend_pbf backend_type;
  typedef mapnik::vector::processor<backend_type> renderer_type;
  
  backend_type backend(*tile.m_mapnik_tile, path_multiplier);
  
  mapnik::request request(map.width(),
                          map.height(),
                          map.get_current_extent());
  request.set_buffer_size(buffer_size);
  
  renderer_type ren(backend,
                    map,
                    request,
                    scale_factor,
                    offset_x,
                    offset_y,
                    tolerance,
                    image_format,
                    scaling_method);
  ren.apply(scale_denominator);
  
  return ren.painted();
}

/**
 * Processes "izers" on the given layer, based on the configuration
 */
void process_vector_layer(mapnik::vector::tile_layer const& layer,
                          boost::optional<pt::ptree const&> mergonalize,
                          boost::optional<pt::ptree const&> generalize,
                          boost::optional<pt::ptree const&> labelize) {

  if (mergonalize) {
    pt::ptree const& mergonalizer_conf = mergonalize.get();
    // TODO: Merganolize!
  }
  if (generalize) {
    pt::ptree const& generalizer_conf = generalize.get();
    // TODO: Generalize!
  }
  if (labelize) {
    pt::ptree const& labelizer_conf = labelize.get();
    // TODO: Labelize!
  }
}

/**
 * Find the appropriate config for a given "izer" at a given zoom level, if it exists
 */
boost::optional<pt::ptree const&> izer_at_zoom(pt::ptree const& layer_conf,
                                               std::string const& izer_name,
                                               int zoom_level) {

  auto optional_conf = layer_conf.get_child_optional(izer_name);
  if (optional_conf) {
    pt::ptree const& izer_conf = optional_conf.get();
    // iterate and check each zoom level range
    for (auto range : izer_conf) {
      int min = range.second.get<int>("minzoom");
      int max = range.second.get<int>("maxzoom");
      if (zoom_level >= min && zoom_level <= max) {
        return boost::optional<pt::ptree const&>(range.second);
      }
    }
  }
  return boost::none;
}

/**
 * Processes "izers" on the given tile, based on the configuration
 */
void process_vector_tile(tile & tile, pt::ptree const& config, int zoom_level) {
  // iterate all layers in the tile
  for (auto layer : tile.m_mapnik_tile->layers()) {
    // check for config corresponding to this layer
    auto optional_conf = config.get_child_optional(layer.name());
    if (optional_conf) {
      pt::ptree const& layer_conf = optional_conf.get();
      auto mergonalize = izer_at_zoom(layer_conf, "mergonalize", zoom_level);
      auto generalize = izer_at_zoom(layer_conf, "generalize", zoom_level);
      auto labelize = izer_at_zoom(layer_conf, "labelize", zoom_level);
      process_vector_layer(layer, mergonalize, generalize, labelize);
    }
  }
}

} // namespace avecado
