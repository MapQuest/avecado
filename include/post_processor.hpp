#ifndef AVECADO_POST_PROCESSOR_HPP
#define AVECADO_POST_PROCESSOR_HPP

#include "tile.hpp"

namespace avecado {

/**
 * Post processor takes a configuration property tree that defines
 * post-processes, dubbed "izers" (e.g. generalizer, unionizer, etc.)
 * The class has one method for running the "izers" on a vector tile.
 */
class post_processor {
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
   *     layers at which zoom levels.
   *
   * Throws an exception if an unrecoverable error was encountered
   * while reading config options.
   */
  void load(pt::ptree const& config);

  /**
   * Run post-processes on each layer in the given vector tile,
   * according to the loaded configuration.
   *
   * Arguments:
   *
   *   tile
   *     The vector tile to process.
   *
   * Throws an exception if an unrecoverable error was encountered
   * while processing a vector layer.
   */
  void process_vector_tile(tile & tile) const;

private:
  class pimpl;
  std::unique_ptr<pimpl> m_impl;
};

} // namespace avecado

#endif // AVECADO_POST_PROCESSOR_HPP
