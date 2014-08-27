#include "post_process/labelizer.hpp"

namespace avecado {
namespace post_process {

class labelizer : public izer {
public:
  labelizer() {}
  virtual ~labelizer() {}

  virtual void process(mapnik::vector::tile_layer & layer) const;
};

void labelizer::process(mapnik::vector::tile_layer & layer) const {
  // TODO: labelize!
}

izer_ptr create_labelizer(pt::ptree const& config) {
  return std::make_shared<labelizer>();
}

} // namespace post_process
} // namespace avecado
