#include "post_process/generalizer.hpp"

namespace avecado {
namespace post_process {

/**
 * Post-process that runs a selected generalization algorithm
 * on feature geometries.
 */
class generalizer : public izer {
public:
  generalizer() {};
  virtual ~generalizer() {}

  virtual void process(std::vector<mapnik::feature_ptr> &layer) const;
};

void generalizer::process(std::vector<mapnik::feature_ptr> &layer) const {
  // TODO: generalize!
}

izer_ptr create_generalizer(pt::ptree const& config) {
  return std::make_shared<generalizer>();
}

} // namespace post_process
} // namespace avecado
