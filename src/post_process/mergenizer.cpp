#include "post_process/mergenizer.hpp"

namespace avecado {
namespace post_process {

class mergenizer : public izer {
public:
  mergenizer() {}
  virtual ~mergenizer() {}

  virtual void process(mapnik::vector::tile_layer & layer) const;
};

void mergenizer::process(mapnik::vector::tile_layer & layer) const {
  // TODO: mergenizer!
}

izer_ptr create_mergenizer(pt::ptree const& config) {
  return std::make_shared<mergenizer>();
}

} // namespace post_process
} // namespace avecado
