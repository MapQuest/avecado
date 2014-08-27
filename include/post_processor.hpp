#ifndef AVECADO_LAYER_PROCESSOR_MANAGER_HPP
#define AVECADO_LAYER_PROCESSOR_MANAGER_HPP

#include "tile.hpp"

namespace avecado {
namespace post_process {

class post_processor {
public:
  post_processor();
  ~post_processor();

  void load(pt::ptree const& config);

  void process_vector_tile(tile & tile, int zoom_level) const;

private:
  class process_manager;
  std::unique_ptr<process_manager> m_manager;
};

} // namespace post_process
} // namespace avecado

#endif // AVECADO_LAYER_PROCESSOR_MANAGER_HPP
