#ifndef AVECADO_UNIONIZER_HPP
#define AVECADO_UNIONIZER_HPP

#include "post_process/izer_base.hpp"

#include <boost/property_tree/ptree.hpp>

namespace pt = boost::property_tree;

namespace avecado {
namespace post_process {

/**
 * Create a new instance of "unionizer", a post-process that
 * merges features which have matching attribution and
 * geometries that are able to be joined or unioned together.
 */
izer_ptr create_unionizer(pt::ptree const& config);

} // namespace post_process
} // namespace avecado

#endif // AVECADO_UNIONIZER_HPP
