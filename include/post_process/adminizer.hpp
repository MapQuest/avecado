#ifndef AVECADO_ADMINIZER_HPP
#define AVECADO_ADMINIZER_HPP

#include "post_process/izer_base.hpp"

#include <boost/property_tree/ptree.hpp>

namespace pt = boost::property_tree;

namespace avecado {
namespace post_process {

/**
 * Create a new instance of "adminizer", a post-process that
 * applies administrative region attribution to features,
 * based on geographic location of the geometry.
 */
izer_ptr create_adminizer(pt::ptree const& config);

} // namespace post_process
} // namespace avecado

#endif // AVECADO_ADMINIZER_HPP
