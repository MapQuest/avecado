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

  //turn a zoom level into mapnik scale which is units per pixel:
  //https://github.com/mapnik/mapnik/wiki/ScaleAndPpi
  const double WORLD_CIRCUMFERENCE_METERS = 40075016.68;
  double meters_per_pixel(const mapnik::Map& m, int z) {
    //if we fit the whole world into a tile this size
    //this is how many meters per pixel per axis we would have
    //most often width and height will be 256 pixels
    const double world_meters_per_pixel_x = WORLD_CIRCUMFERENCE_METERS / m.width();
    const double world_meters_per_pixel_y = WORLD_CIRCUMFERENCE_METERS / m.height();
    //ASSUMPTION: lets hope the tile is basically square in terms of pixels
    const double world_meters_per_pixel = (world_meters_per_pixel_x + world_meters_per_pixel_y) * .5;
    //this is how many tiles per axis we have at this zoom level
    const int tiles_per_axis = 1 << z;
    //this is how many meters fit in a pixel at a tile from this zoom level
    return world_meters_per_pixel / tiles_per_axis;
  }

  //approximate equivalence using an epsilon
  const double EPSILON = .0005;
  bool appx_equal(const double a, const double b) {
    return fabs(a-b) < EPSILON;
  }
}

namespace avecado {

typedef std::vector<post_process::izer_ptr> izer_vec_t;
typedef struct {
  double minzoom;
  double maxzoom;
  izer_vec_t processes;
} scale_range_t;
typedef std::vector<scale_range_t> scale_range_vec_t;
typedef std::map<std::string, scale_range_vec_t> layer_map_t;

class post_processor::pimpl {
public:
  pimpl();
  void load(pt::ptree const& config);
  size_t process_layer(std::vector<mapnik::feature_ptr> & layer,
                     const std::string &layer_name,
                     mapnik::Map const& map) const;
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
      scale_range.minzoom = range_child.second.get<int>("minzoom") - .5;
      scale_range.maxzoom = range_child.second.get<int>("maxzoom") + .5;
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

// Find post-processes for given layer at scale and run them
size_t post_processor::pimpl::process_layer(std::vector<mapnik::feature_ptr> & layer,
                                          const std::string &layer_name,
                                          mapnik::Map const& map) const {
  size_t ran = 0;
  layer_map_t::const_iterator layer_itr = m_layer_processes.find(layer_name);
  if (layer_itr != m_layer_processes.end()) {
    scale_range_vec_t const& scale_ranges = layer_itr->second;
    // TODO: Consider ways to optimize scale range look up
    for (auto range : scale_ranges) {
      double min_scale = meters_per_pixel(map, range.maxzoom);
      double max_scale = meters_per_pixel(map, range.minzoom);
      if ((map.scale() >= min_scale && map.scale() <= max_scale)/* ||
          appx_equal(map.scale(), min_scale) ||
          appx_equal(map.scale(), max_scale)*/) {
        // TODO: unserialize geometry objects to pass through izers
        for (auto p : range.processes) {
          p->process(layer, map);
          ++ran;
        }
        break;
      }
    }
  }
  return ran;
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

size_t post_processor::process_layer(std::vector<mapnik::feature_ptr> &layer,
                                   const std::string &layer_name,
                                   mapnik::Map const& map) const {
  return m_impl->process_layer(layer, layer_name, map);
}

} // namespace avecado
