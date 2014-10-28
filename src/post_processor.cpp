#include "avecado.hpp"
#include "post_processor.hpp"
#include "post_process/factory.hpp"
#include "post_process/izer_base.hpp"
#include "post_process/adminizer.hpp"
#include "post_process/generalizer.hpp"
#include "post_process/labelizer.hpp"
#include "post_process/unionizer.hpp"

namespace pt = boost::property_tree;

namespace {
  const double WORLD_METERS = 40075016.68;
  double zoom_to_scale(int zoom) {
    return WORLD_METERS / double(1 << zoom);
  }
}

namespace avecado {

typedef std::vector<post_process::izer_ptr> izer_vec_t;
typedef struct {
  int minscale;
  int maxscale;
  izer_vec_t processes;
} scale_range_t;
typedef std::vector<scale_range_t> scale_range_vec_t;
typedef std::map<std::string, scale_range_vec_t> layer_map_t;

class post_processor::pimpl {
public:
  pimpl();
  void load(pt::ptree const& config);
  void process_layer(std::vector<mapnik::feature_ptr> & layer,
                     const std::string &layer_name,
                     double scale) const;
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
    scale_range_vec_t scale_ranges;
    for (auto range_child : layer_child.second) {
      scale_range_t scale_range;
      scale_range.minscale = zoom_to_scale(range_child.second.get<int>("minzoom"));
      scale_range.maxscale = zoom_to_scale(range_child.second.get<int>("maxzoom"));
      pt::ptree const& process_config = range_child.second.get_child("process");
      for (auto izer_child : process_config) {
        std::string const& type = izer_child.second.get<std::string>("type");
        post_process::izer_ptr p = factory.create(type, izer_child.second);
        scale_range.processes.push_back(p);
      }
      scale_ranges.push_back(std::move(scale_range));
    }
    m_layer_processes.insert(std::make_pair(layer_child.first, std::move(scale_ranges)));
  }
}

// Find post-processes for given layer at scale level and run them
void post_processor::pimpl::process_layer(std::vector<mapnik::feature_ptr> & layer,
                                          const std::string &layer_name,
                                          double scale) const {
  layer_map_t::const_iterator layer_itr = m_layer_processes.find(layer_name);
  if (layer_itr != m_layer_processes.end()) {
    scale_range_vec_t const& scale_ranges = layer_itr->second;
    // TODO: Consider ways to optimize scale range look up
    for (auto range : scale_ranges) {
      if (scale >= range.minscale && scale <= range.maxscale) {
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

void post_processor::process_layer(std::vector<mapnik::feature_ptr> &layer, 
                                   const std::string &layer_name,
                                   double scale) const {
  m_impl->process_layer(layer, layer_name, scale);
}

} // namespace avecado
