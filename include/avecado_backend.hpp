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

class avecado_backend {
public:
  avecado_backend(mapnik::vector::tile & tile,
                  unsigned path_multiplier)
    : m_pbf(tile, path_multiplier),
      m_tolerance(1) {}

  void start_tile_layer(std::string const& name) {
    m_current_layer_name = name;
    // TODO: Load izers for layer
  }

  inline void stop_tile_layer() {
    // TODO: run geometries through izers before writing to pbf
    // write layer to pbf
    m_pbf.start_tile_layer(m_current_layer_name);
    for (auto feature : m_current_layer_features) {
      m_pbf.start_tile_feature(*feature);
      if (m_current_image_buffer) {
        m_pbf.add_tile_feature_raster(*m_current_image_buffer);
        m_current_image_buffer.reset();
      }
      for (size_t i = 0; i < feature->num_geometries(); i++) {
        mapnik::geometry_type const& path = feature->get_geometry(i);
        // See hack note about tolerance below in add_path(...)
        m_pbf.add_path(path, m_tolerance, path.type());
      }
      m_pbf.stop_tile_feature();
    }
    m_pbf.stop_tile_layer();
    // clear the current feature vector
    m_current_layer_features.clear();
  }

  void start_tile_feature(mapnik::feature_impl const& feature) {
    // new current feature object
    m_current_feature.reset(new mapnik::feature_impl(feature.context(), feature.id()));
    m_current_feature->set_id(feature.id());
    // copy kvp to new feature
    mapnik::feature_kv_iterator itr = feature.begin();
    mapnik::feature_kv_iterator end = feature.end();
    for ( ;itr!=end; ++itr) {
      std::string const& name = MAPNIK_GET<0>(*itr);
      mapnik::value const& val = MAPNIK_GET<1>(*itr);
      m_current_feature->put_new(name, val);
    }
  }

  void stop_tile_feature() {
    if (m_current_feature && m_current_feature->num_geometries() > 0) {
      m_current_layer_features.push_back(m_current_feature);
    }
    m_current_feature.reset();
  }

  void add_tile_feature_raster(std::string const& image_buffer) {
    m_current_image_buffer = image_buffer;
  }

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
  std::string m_current_layer_name;
  std::vector<mapnik::feature_ptr> m_current_layer_features;
  mapnik::feature_ptr m_current_feature;
  boost::optional<std::string> m_current_image_buffer;
  unsigned m_tolerance;
};

} // namespace avecado

#endif // AVECADO_BACKEND_HPP
