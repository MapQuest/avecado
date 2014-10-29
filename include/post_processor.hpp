#ifndef AVECADO_POST_PROCESSOR_HPP
#define AVECADO_POST_PROCESSOR_HPP

#include "tile.hpp"
#include <boost/property_tree/ptree.hpp>
#include <boost/noncopyable.hpp>
#include <mapnik/feature.hpp>
#include <memory>
#include <vector>
#include <string>

namespace avecado {

/**
 * Post processor takes a configuration property tree that defines
 * post-processes, dubbed "izers" (e.g. generalizer, unionizer, etc.)
 * The class has one method for running the "izers" on a vector tile.
 */
class post_processor : public boost::noncopyable {
public:
  post_processor();
  ~post_processor();

  /**
   * Parse the configuration and initialize the specified post-processes.
   *
   * Arguments:
   *
   *   config
   *     Configuration tree specifying which processes to run on which
   *     layers at which scales.
   *
   * Throws an exception if an unrecoverable error was encountered
   * while reading config options.
   */
  void load(boost::property_tree::ptree const& config);

  /**
   * Run post-processes on a layer of vector data,
   * according to the loaded configuration.
   *
   * Arguments:
   *
   *   layer
   *     The vector tile layer to process.
   *
   *   layer_name
   *     The name of the layer.
   *
   *   scale
   *     Scale of the tile.
   *
   * Throws an exception if an unrecoverable error was encountered
   * while processing a vector layer.
   */
  void process_layer(std::vector<mapnik::feature_ptr> &layer, 
                     const std::string &layer_name,
                     mapnik::Map const& map) const;

private:
  class pimpl;
  std::unique_ptr<pimpl> m_impl;
};

} // namespace avecado

#endif // AVECADO_POST_PROCESSOR_HPP
