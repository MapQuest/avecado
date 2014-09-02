#ifndef AVECADO_IZER_BASE_HPP
#define AVECADO_IZER_BASE_HPP

#include "vector_tile_backend_pbf.hpp"

namespace avecado {
namespace post_process {

/**
 * Base class for post processes, dubbed "izers" (e.g. generalizer)
 * An "izer" modifies the features and geometry after the tile is created.
 */
class izer {
public:
  virtual ~izer() {};
  virtual void process(std::vector<mapnik::feature_ptr> &layer) const = 0;
};

typedef std::shared_ptr<izer> izer_ptr;

} // namespace post_process
} // namespace avecado

#endif // AVECADO_IZER_BASE_HPP
