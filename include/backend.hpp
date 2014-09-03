#ifndef AVECADO_BACKEND_HPP
#define AVECADO_BACKEND_HPP

#include "mapnik3x_compatibility.hpp"
#include MAPNIK_VARIANT_INCLUDE

// mapnik
#include <mapnik/feature.hpp>
#include <mapnik/value_types.hpp>
#include <mapnik/vertex.hpp>

// vector tile
#include "vector_tile_backend_pbf.hpp"

// boost
#include <boost/optional.hpp>

namespace avecado {

class post_processor;

class backend {
public:
  backend(mapnik::vector::tile & tile,
          unsigned path_multiplier,
          boost::optional<const post_processor &> pp);

  void start_tile_layer(std::string const& name);

  void stop_tile_layer();

  void start_tile_feature(mapnik::feature_impl const& feature);

  void stop_tile_feature();

  void add_tile_feature_raster(std::string const& image_buffer);

  template <typename T>
  inline unsigned add_path(T & path, unsigned tolerance, MAPNIK_GEOM_TYPE type) {
    mapnik::geometry_type * geom = new mapnik::geometry_type(type);
    double x, y;
    unsigned command, count = 0;
    path.rewind(0);
    while ((command = path.vertex(&x, &y)) != mapnik::SEG_END) {
      geom->push_vertex(x, y, (mapnik::CommandType)command);
      count++;
    }
    m_current_feature->add_geometry(geom);
    // TODO: This is just modifying a member variable for each add_path call,
    //       which will then be used at the end of the layer for writing all
    //       paths into protobuf. It works for now, because vector_tile_processor
    //       uses the same tolerance value throughout the whole tile, but this
    //       is still a hack way of doing it...
    m_tolerance = tolerance;
    return count;
  }

private:
  mapnik::vector::backend_pbf m_pbf;
  unsigned int m_tolerance;
  boost::optional<const post_processor &> m_post_processor;
  std::string m_current_layer_name;
  std::vector<mapnik::feature_ptr> m_current_layer_features;
  mapnik::feature_ptr m_current_feature;
  boost::optional<std::string> m_current_image_buffer;
};

} // namespace avecado

#endif // AVECADO_BACKEND_HPP
