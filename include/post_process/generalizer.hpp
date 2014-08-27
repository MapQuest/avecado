#ifndef AVECADO_GENERALIZER_HPP
#define AVECADO_GENERALIZER_HPP

#include "post_process/izer_base.hpp"

#include <boost/property_tree/ptree.hpp>

namespace pt = boost::property_tree;

namespace avecado {
namespace post_process {

izer_ptr create_generalizer(pt::ptree const& config);

} // namespace post_process
} // namespace avecado

#endif // AVECADO_GENERALIZER_HPP
