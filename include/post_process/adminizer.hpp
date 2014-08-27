#ifndef AVECADO_ADMINIZER_HPP
#define AVECADO_ADMINIZER_HPP

#include "post_process/izer_base.hpp"

#include <boost/property_tree/ptree.hpp>

namespace pt = boost::property_tree;

namespace avecado {
namespace post_process {

izer_ptr create_adminizer(pt::ptree const& config);

} // namespace post_process
} // namespace avecado

#endif // AVECADO_ADMINIZER_HPP
