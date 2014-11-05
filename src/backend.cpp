#include "backend.hpp"
#include "post_processor.hpp"

namespace avecado {

backend::backend(mapnik::vector::tile & tile,
                 unsigned path_multiplier,
                 mapnik::Map const& map,
                 boost::optional<const post_processor &> pp)
  : m_pbf(tile, path_multiplier),
    m_map(map),
    m_tolerance(1),
    m_post_processor(pp) {}

void backend::start_tile_layer(std::string const& name) {
  m_current_layer_name = name;
  // TODO: Load izers for layer
}

void backend::stop_tile_layer() {
  if (m_post_processor) {
    m_post_processor->process_layer(m_current_layer_features,
                                    m_current_layer_name,
                                    m_map);
  }

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

void backend::start_tile_feature(mapnik::feature_impl const& feature) {
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

void backend::stop_tile_feature() {
  if (m_current_feature && m_current_feature->num_geometries() > 0) {
    m_current_layer_features.push_back(m_current_feature);
  }
  m_current_feature.reset();
}

void backend::add_tile_feature_raster(std::string const& image_buffer) {
  m_current_image_buffer = image_buffer;
}

} // namespace avecado
