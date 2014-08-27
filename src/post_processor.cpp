#include "avecado.hpp"
#include "post_process/factory.hpp"
#include "post_process/izer_base.hpp"
#include "post_process/adminizer.hpp"
#include "post_process/generalizer.hpp"
#include "post_process/labelizer.hpp"
#include "post_process/mergenizer.hpp"

namespace avecado {
namespace post_process {

typedef std::vector<izer_ptr> izer_vec_t;
typedef std::shared_ptr<izer_vec_t> izer_vec_ptr;
struct zoom_range_t {
  int minzoom;
  int maxzoom;
  izer_vec_ptr processes;
  zoom_range_t () {
    processes = std::make_shared<izer_vec_t>();
  }
};
typedef std::vector<zoom_range_t> zoom_range_vec_t;
typedef std::map<std::string, zoom_range_vec_t> layer_map_t;

class post_processor::process_manager {
public:
  process_manager();
  void load(pt::ptree const& config);
  izer_vec_ptr get_processes(std::string const& layer_name, int zoom_level) const;
private:
  layer_map_t m_layer_processes;
};

post_processor::process_manager::process_manager()
  : m_layer_processes() {}

void post_processor::process_manager::load(pt::ptree const& config) {
  factory<izer> factory;
  factory.register_type("adminizer", create_adminizer)
         .register_type("generalizer", create_generalizer)
         .register_type("labelizer", create_labelizer)
         .register_type("mergenizer", create_mergenizer);

  for (auto layer_child : config) {
    zoom_range_vec_t zoom_ranges;
    for (auto range_child : layer_child.second) {
      zoom_range_t zoom_range;
      zoom_range.minzoom = range_child.second.get<int>("minzoom");
      zoom_range.maxzoom = range_child.second.get<int>("maxzoom");
      pt::ptree const& process_config = range_child.second.get_child("process");
      for (auto izer_child : process_config) {
        std::string const& type = izer_child.second.get<std::string>("type");
        izer_ptr p = factory.create(type, izer_child.second);
        zoom_range.processes->push_back(p);
      }
      zoom_ranges.push_back(std::move(zoom_range));
    }
    m_layer_processes.insert(std::make_pair(layer_child.first, std::move(zoom_ranges)));
  }
}

izer_vec_ptr post_processor::process_manager::get_processes(std::string const& layer_name, int zoom_level) const {
  layer_map_t::const_iterator layer_itr = m_layer_processes.find(layer_name);
  if (layer_itr != m_layer_processes.end()) {
    zoom_range_vec_t const& zoom_ranges = layer_itr->second;
    // TODO: Consider ways to optimize zoom range look up
    for (auto range : zoom_ranges) {
      if (zoom_level >= range.minzoom && zoom_level <= range.maxzoom) {
        return range.processes;
      }
    }
  }
  return izer_vec_ptr();
}

post_processor::post_processor()
  : m_manager(new process_manager()) {}

post_processor::~post_processor() {}

void post_processor::load(pt::ptree const& config) {
  std::unique_ptr<process_manager> mgr(new process_manager());
  mgr->load(config);
  // if we got here without throwing an exception, then
  // we should be OK to use this new configuration.
  m_manager.swap(mgr);
}

void post_processor::process_vector_tile(tile & tile, int zoom_level) const {
  // iterate all layers in the tile
  for (auto layer : tile.m_mapnik_tile->layers()) {
    auto processes = m_manager->get_processes(layer.name(), zoom_level);
    if (processes) {
      // TODO: Unserialize geometry objects to pass through izers
      for (auto p : *processes) {
        p->process(layer);
      }
    }
  }
}

} // namespace post_process
} // namespace avecado
