#ifndef AVECADO_FACTORY_HPP
#define AVECADO_FACTORY_HPP

#include <boost/property_tree/ptree.hpp>

namespace pt = boost::property_tree;

namespace avecado {
namespace post_process {

/**
 * Generic factory class for creating objects based on type name and configuration.
 */
template <typename T>
class factory {
public:
  typedef std::shared_ptr<T> ptr_t;
  typedef ptr_t (*factory_func_t)(pt::ptree const& config);
  typedef std::map<std::string, factory_func_t> func_map_t;

  factory() : m_factory_functions() {};

  factory & register_type(std::string const& type, factory_func_t func) {
    m_factory_functions.insert(std::make_pair(type, func));
    return (*this);
  }

  ptr_t create(std::string const& type, pt::ptree const& config) const {
    typename func_map_t::const_iterator f_itr = m_factory_functions.find(type);
    if (f_itr == m_factory_functions.end()) {
      throw std::runtime_error("Unrecognized type: " + type);
    }
    factory_func_t func = f_itr->second;
    ptr_t ptr = func(config);
    return ptr;
  }

private:
  func_map_t m_factory_functions;
};

} // namespace post_process
} // namespace avecado

#endif // AVECADO_FACTORY_HPP
