#ifndef TILEJSON_HPP
#define TILEJSON_HPP

#include "fetcher.hpp"
#include <memory>
#include <boost/property_tree/ptree.hpp>

namespace avecado {

/* Fetches a URI and parses it as TileJSON.
 */
boost::property_tree::ptree tilejson(const std::string &uri);

/* Constructs a fetcher based on a TileJSON config.
 *
 * This is a convenience method which constructs a sequence of nested
 * fetchers such that they return tiles specified by the TileJSON
 * config given as input. In turn, this means that often a single
 * parameter is all that's necessary to specify a vector tile source.
 */
std::unique_ptr<fetcher> make_tilejson_fetcher(const boost::property_tree::ptree &conf);

} // namespace avecado

#endif /* TILEJSON_HPP */
