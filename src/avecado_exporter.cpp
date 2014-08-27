#include <boost/program_options.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/exceptions.hpp>
#include <fstream>

#include <mapnik/utils.hpp>
#include <mapnik/load_map.hpp>
#include <mapnik/font_engine_freetype.hpp>
#include <mapnik/datasource_cache.hpp>

#include "avecado.hpp"
#include "config.h"

namespace bpo = boost::program_options;
namespace pt = boost::property_tree;

#define WORLD_SIZE (40075016.68)

// get the mercator bounding box for a tile coordinate
mapnik::box2d<double> box_for_tile(int z, int x, int y) {
  const double scale = WORLD_SIZE / double(1 << z);
  const double half_world = 0.5 * WORLD_SIZE;

  return mapnik::box2d<double>(
    x * scale - half_world,
    half_world - (y+1) * scale,
    (x+1) * scale - half_world,
    half_world - y * scale);
}

int main(int argc, char *argv[]) {
  unsigned int path_multiplier;
  int buffer_size;
  double scale_factor;
  unsigned int offset_x;
  unsigned int offset_y;
  unsigned int tolerance;
  std::string image_format;
  mapnik::scaling_method_e scaling_method = mapnik::SCALING_NEAR;
  double scale_denominator;
  std::string output_file;
  std::string config_file;
  std::string map_file;
  int z, x, y;
  std::string fonts_dir, input_plugins_dir;

  bpo::options_description options(
    "Avecado " VERSION "\n"
    "\n"
    "  Usage: avecado [options] <map-file> <tile-z> <tile-x> <tile-y>\n"
    "\n");

  options.add_options()
    ("help,h", "Print this help message.")
    ("config-file,c", bpo::value<std::string>(&config_file),
     "JSON config file to specify post-processing for data layers.")
    ("output-file,o", bpo::value<std::string>(&output_file)->default_value("tile.pbf"),
     "File to serialise the vector tile to.")
    ("path-multiplier,p", bpo::value<unsigned int>(&path_multiplier)->default_value(16),
     "Create a tile with coordinates multiplied by this constant to get sub-pixel "
     "accuracy.")
    ("buffer-size,b", bpo::value<int>(&buffer_size)->default_value(0),
     "Number of pixels around the tile to buffer in order to allow for features "
     "whose rendering effects extend beyond the geometric extent.")
    ("scale_factor,s", bpo::value<double>(&scale_factor)->default_value(1.0),
     "Scale factor to multiply style values by.")
    ("offset-x", bpo::value<unsigned int>(&offset_x)->default_value(0),
     "Offset added to tile geometry x coordinates.")
    ("offset-y", bpo::value<unsigned int>(&offset_y)->default_value(0),
     "Offset added to tile geometry y coordinates.")
    ("tolerance,t", bpo::value<unsigned int>(&tolerance)->default_value(1),
     "Tolerance used to simplify output geometry.")
    ("image-format,f", bpo::value<std::string>(&image_format)->default_value("jpeg"),
     "Image file format used for embedding raster layers.")
    ("scaling-method,m", bpo::value<std::string>()->default_value("near"),
     "Method used to re-sample raster layers.")
    ("scale-denominator,d", bpo::value<double>(&scale_denominator)->default_value(0.0),
     "Override for scale denominator. A value of 0 means to use the sensible default "
     "which Mapnik will generate from the tile context.")
    ("fonts", bpo::value<std::string>(&fonts_dir)->default_value(MAPNIK_DEFAULT_FONT_DIR),
     "Directory to tell Mapnik to look in for fonts.")
    ("input-plugins", bpo::value<std::string>(&input_plugins_dir)
     ->default_value(MAPNIK_DEFAULT_INPUT_PLUGIN_DIR),
     "Directory to tell Mapnik to look in for input plugins.")
    // positional arguments
    ("map-file", bpo::value<std::string>(&map_file), "Mapnik XML input file.")
    ("tile-z", bpo::value<int>(&z), "Zoom level.")
    ("tile-x", bpo::value<int>(&x), "Tile x coordinate.")
    ("tile-y", bpo::value<int>(&y), "Tile x coordinate.")
    ;

  bpo::positional_options_description pos_options;
  pos_options
    .add("map-file", 1)
    .add("tile-z", 1)
    .add("tile-x", 1)
    .add("tile-y", 1)
    ;

  bpo::variables_map vm;

  try {
    bpo::store(bpo::command_line_parser(argc,argv)
               .options(options)
               .positional(pos_options)
               .run(),
               vm);
    bpo::notify(vm);

  } catch (std::exception & e) {
    std::cerr << "Unable to parse command line options because: " << e.what() << "\n"
              << "This is a bug, please report it at " PACKAGE_BUGREPORT << "\n";
    return EXIT_FAILURE;
  }

  if (vm.count("help")) {
    std::cout << options << "\n";
    return EXIT_SUCCESS;
  }

  // argument checking and verification
  for (auto arg : {"map-file", "tile-z", "tile-x", "tile-y"}) {
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

    scaling_method = *method;
  }

  // load *izer config for layers
  avecado::post_process::post_processor processor;
  if (!config_file.empty()) {
    pt::ptree config;
    try {
      pt::read_json(config_file, config);
    }
    catch (pt::ptree_error & err) {
      std::cerr << "ERROR while parsing: " << config_file << std::endl;
      std::cerr << err.what() << std::endl;
      return EXIT_FAILURE;
    }
    try {
      processor.load(config);
    } catch (const std::exception &e) {
      std::cerr << "Unable to load post processors from config: " << e.what() << "\n";
      return EXIT_FAILURE;
    }
  }

  try {
    mapnik::Map map;
    avecado::tile tile;

    // try to register fonts and input plugins
    mapnik::freetype_engine::register_fonts(fonts_dir);
    mapnik::datasource_cache::instance().register_datasources(input_plugins_dir);

    // load map config from disk
    mapnik::load_map(map, map_file);

    // setup map parameters
    map.resize(256, 256);
    map.zoom_to_box(box_for_tile(z, x, y));

    // actually making the vector tile
    avecado::make_vector_tile(tile, path_multiplier, map, buffer_size, scale_factor,
                              offset_x, offset_y, tolerance, image_format,
                              scaling_method, scale_denominator);

    processor.process_vector_tile(tile, z);

    // serialise to file
    std::ofstream output(output_file);
    output << tile;

  } catch (const std::exception &e) {
    std::cerr << "Unable to make vector tile: " << e.what() << "\n";
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
