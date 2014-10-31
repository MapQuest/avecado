#include "post_process/labelizer.hpp"

namespace avecado {
namespace post_process {

/**
 * Post-process that handles shield attribution and concurrencies,
 * and generates label placements along linear geometries.
 */
class labelizer : public izer {
public:
  labelizer() {}
  virtual ~labelizer() {}

  virtual void process(std::vector<mapnik::feature_ptr> &layer, mapnik::Map const& map) const;
};

void labelizer::process(std::vector<mapnik::feature_ptr> &layer, mapnik::Map const& map) const {
  // TODO: labelize!
}

izer_ptr create_labelizer(pt::ptree const& config) {
  return std::make_shared<labelizer>();
}

} // namespace post_process
} // namespace avecado
