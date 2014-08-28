#include "post_process/unionizer.hpp"

namespace avecado {
namespace post_process {

/**
 * Post-process that merges features which have matching attribution
 * and geometries that are able to be joined or unioned together.
 */
class unionizer : public izer {
public:
  unionizer() {}
  virtual ~unionizer() {}

  virtual void process(mapnik::vector::tile_layer & layer) const;
};

void unionizer::process(mapnik::vector::tile_layer & layer) const {
  // TODO: unionize!
}

izer_ptr create_unionizer(pt::ptree const& config) {
  return std::make_shared<unionizer>();
}

} // namespace post_process
} // namespace avecado
