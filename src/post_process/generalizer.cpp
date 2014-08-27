#include "post_process/generalizer.hpp"

namespace avecado {
namespace post_process {

class generalizer : public izer {
public:
  generalizer() {};
  virtual ~generalizer() {}

  virtual void process(mapnik::vector::tile_layer & layer) const;
};

void generalizer::process(mapnik::vector::tile_layer & layer) const {
  // TODO: generalize!
}

izer_ptr create_generalizer(pt::ptree const& config) {
  return std::make_shared<generalizer>();
}

} // namespace post_process
} // namespace avecado
