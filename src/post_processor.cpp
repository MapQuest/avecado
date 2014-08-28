#include "avecado.hpp"
#include "post_process/factory.hpp"
#include "post_process/izer_base.hpp"
#include "post_process/adminizer.hpp"
#include "post_process/generalizer.hpp"
#include "post_process/labelizer.hpp"
#include "post_process/unionizer.hpp"

namespace avecado {

typedef std::vector<post_process::izer_ptr> izer_vec_t;
typedef struct {
  int minzoom;
  int maxzoom;
  izer_vec_t processes;
} zoom_range_t;
typedef std::vector<zoom_range_t> zoom_range_vec_t;
typedef std::map<std::string, zoom_range_vec_t> layer_map_t;

class post_processor::pimpl {
public:
  pimpl();
  void load(pt::ptree const& config);
  void process_layer(mapnik::vector::tile_layer & layer, int zoom_level) const;
private:
  layer_map_t m_layer_processes;
};

post_processor::pimpl::pimpl()
  : m_layer_processes() {}

// Parse config tree and initialize izers
void post_processor::pimpl::load(pt::ptree const& config) {
  post_process::factory<post_process::izer> factory;
  factory.register_type("adminizer", post_process::create_adminizer)
         .register_type("generalizer", post_process::create_generalizer)
         .register_type("labelizer", post_process::create_labelizer)
         .register_type("unionizer", post_process::create_unionizer);

  for (auto layer_child : config) {
    zoom_range_vec_t zoom_ranges;
    for (auto range_child : layer_child.second) {
      zoom_range_t zoom_range;
      zoom_range.minzoom = range_child.second.get<int>("minzoom");
      zoom_range.maxzoom = range_child.second.get<int>("maxzoom");
      pt::ptree const& process_config = range_child.second.get_child("process");
      for (auto izer_child : process_config) {
        std::string const& type = izer_child.second.get<std::string>("type");
        post_process::izer_ptr p = factory.create(type, izer_child.second);
        zoom_range.processes.push_back(p);
      }
      zoom_ranges.push_back(std::move(zoom_range));
    }
    m_layer_processes.insert(std::make_pair(layer_child.first, std::move(zoom_ranges)));
  }
}

// Find post-processes for given layer at zoom level and run them
void post_processor::pimpl::process_layer(mapnik::vector::tile_layer & layer, int zoom_level) const {
  layer_map_t::const_iterator layer_itr = m_layer_processes.find(layer.name());
  if (layer_itr != m_layer_processes.end()) {
    zoom_range_vec_t const& zoom_ranges = layer_itr->second;
    // TODO: Consider ways to optimize zoom range look up
    for (auto range : zoom_ranges) {
      if (zoom_level >= range.minzoom && zoom_level <= range.maxzoom) {
        // TODO: unserialize geometry objects to pass through izers
        for (auto p : range.processes) {
          p->process(layer);
        }
        break;
      }
    }
  }
}

post_processor::post_processor()
  : m_impl(new pimpl()) {}

post_processor::~post_processor() {}

void post_processor::load(pt::ptree const& config) {
  std::unique_ptr<pimpl> impl(new pimpl());
  impl->load(config);
  // if we got here without throwing an exception, then
  // we should be OK to use this new configuration.
  m_impl.swap(impl);
}

void post_processor::process_vector_tile(tile & tile) const {
  for (auto layer : tile.m_mapnik_tile->layers()) {
    m_impl->process_layer(layer, tile.m_z);
  }
}

} // namespace avecado
