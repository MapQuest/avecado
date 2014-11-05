#include <boost/program_options.hpp>
#include <boost/bind.hpp>
#include <boost/asio.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/exceptions.hpp>

#include <mapnik/utils.hpp>
#include <mapnik/load_map.hpp>
#include <mapnik/font_engine_freetype.hpp>
#include <mapnik/datasource_cache.hpp>

#include "avecado.hpp"
#include "http_server/server.hpp"
#include "config.h"

namespace bpo = boost::program_options;
namespace pt = boost::property_tree;
using boost::asio::ip::tcp;

int main(int argc, char *argv[]) {
  server_options srv_opts;
  std::string fonts_dir, input_plugins_dir, config_file;

  bpo::options_description options(
    "Avecado " VERSION "\n"
    "\n"
    "  Usage: avecado_server [options] <map-file> <port>\n"
    "\n"
    "The server will serve PBF vector tiles on the port which you specify, using "
    "the common Google Maps numbering scheme /$z/$x/$y.pbf. For example, the "
    "tile with coordinates z=2, x=1, y=0 would be available at "
    "http://localhost:8080/2/1/0/pbf if the port parameter is given as 8080."
    "\n"
    "\n");

  options.add_options()
    ("help,h", "Print this help message.")
    ("path-multiplier,p", bpo::value<unsigned int>(&srv_opts.path_multiplier)->default_value(16),
     "Create a tile with coordinates multiplied by this constant to get sub-pixel "
     "accuracy.")
    ("buffer-size,b", bpo::value<int>(&srv_opts.buffer_size)->default_value(0),
     "Number of pixels around the tile to buffer in order to allow for features "
     "whose rendering effects extend beyond the geometric extent.")
    ("scale_factor,s", bpo::value<double>(&srv_opts.scale_factor)->default_value(1.0),
     "Scale factor to multiply style values by.")
    ("offset-x", bpo::value<unsigned int>(&srv_opts.offset_x)->default_value(0),
     "Offset added to tile geometry x coordinates.")
    ("offset-y", bpo::value<unsigned int>(&srv_opts.offset_y)->default_value(0),
     "Offset added to tile geometry y coordinates.")
    ("tolerance,t", bpo::value<unsigned int>(&srv_opts.tolerance)->default_value(1),
     "Tolerance used to simplify output geometry.")
    ("image-format,f", bpo::value<std::string>(&srv_opts.image_format)->default_value("jpeg"),
     "Image file format used for embedding raster layers.")
    ("scaling-method,m", bpo::value<std::string>()->default_value("near"),
     "Method used to re-sample raster layers.")
    ("scale-denominator,d", bpo::value<double>(&srv_opts.scale_denominator)->default_value(0.0),
     "Override for scale denominator. A value of 0 means to use the sensible default "
     "which Mapnik will generate from the tile context.")
    ("fonts", bpo::value<std::string>(&fonts_dir)->default_value(MAPNIK_DEFAULT_FONT_DIR),
     "Directory to tell Mapnik to look in for fonts.")
    ("input-plugins", bpo::value<std::string>(&input_plugins_dir)
     ->default_value(MAPNIK_DEFAULT_INPUT_PLUGIN_DIR),
     "Directory to tell Mapnik to look in for input plugins.")
    ("thread-hint", bpo::value<unsigned short>(&srv_opts.thread_hint)->default_value(1),
     "Hint at the number of asynchronous "
     "requests the server should be able to service.")
    ("config-file,c", bpo::value<std::string>(&config_file),
     "JSON config file to specify post-processing for data layers.")
    // positional arguments
    ("map-file", bpo::value<std::string>(&srv_opts.map_file), "Mapnik XML input file.")
    ("port", bpo::value<std::string>(&srv_opts.port), "Port upon which the server will listen.")
    ;

  bpo::positional_options_description pos_options;
  pos_options
    .add("map-file", 1)
    .add("port", 1)
    ;

  bpo::variables_map vm;

  try {
    bpo::store(bpo::command_line_parser(argc,argv)
           .options(options)
           .positional(pos_options)
           .run(),
           vm);
    bpo::notify(vm);

  } catch (std::exception &e) {
    std::cerr << "Unable to parse command line options because: " << e.what() << "\n"
			  << "This is a bug, please report it at " PACKAGE_BUGREPORT << "\n";
    return EXIT_FAILURE;
  }

  if (vm.count("help")) {
    std::cout << options << "\n";
    return EXIT_SUCCESS;
  }

  // argument checking and verification
  for (auto arg : {"map-file", "port"}) {
    if (vm.count(arg) == 0) {
      std::cerr << "The <" << arg << "> argument was not provided, but is mandatory\n\n";
      std::cerr << options << "\n";
      return EXIT_FAILURE;
    }
  }

  if (vm.count("scaling-method")) {
    std::string method_str(vm["scaling-method"].as<std::string>());

    boost::optional<mapnik::scaling_method_e> method =
      mapnik::scaling_method_from_string(method_str);

    if (!method) {
      std::cerr << "The string \"" << method_str << "\" was not recognised as a "
          << "valid scaling method by Mapnik.\n";
      return EXIT_FAILURE;
    }

    srv_opts.scaling_method = *method;

  } else {
    // default option
    srv_opts.scaling_method = mapnik::SCALING_NEAR;
  }

  if (vm.count("config-file")) {
    try {
      // parse json config
      pt::ptree config;
      pt::read_json(config_file, config);

      // init processor
      srv_opts.post_processor.reset(new avecado::post_processor);
      srv_opts.post_processor->load(config);

    } catch (pt::ptree_error const& e) {
      std::cerr << "Error while parsing config: " << config_file << std::endl;
      std::cerr << e.what() << std::endl;
      return EXIT_FAILURE;

    } catch (std::exception const& e) {
      std::cerr << "Error while loading config: " << config_file << std::endl;
      std::cerr << e.what() << std::endl;
      return EXIT_FAILURE;
    }
  }

  //start up the server
  try {
    // try to register fonts and input plugins
    mapnik::freetype_engine::register_fonts(fonts_dir);
    mapnik::datasource_cache::instance().register_datasources(input_plugins_dir);

    // start the server running
    http::server3::server server("0.0.0.0", srv_opts);
    server.run(true);

  } catch (std::exception& e) {
    std::cerr << "Exception: " << e.what() << "\n";
  }

  return EXIT_SUCCESS;
}
