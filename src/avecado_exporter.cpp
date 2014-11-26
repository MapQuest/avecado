#include <boost/program_options.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/exceptions.hpp>
#include <boost/utility/typed_in_place_factory.hpp>
#include <boost/format.hpp>
#include <boost/filesystem.hpp>
#include <fstream>
#include <exception>
#include <stdexcept>
#include <future>
#include <atomic>
#include <unordered_set>

#include <mapnik/utils.hpp>
#include <mapnik/load_map.hpp>
#include <mapnik/font_engine_freetype.hpp>
#include <mapnik/datasource_cache.hpp>
#include <mapnik/graphics.hpp>
#include <mapnik/image_util.hpp>

#include "avecado.hpp"
#include "tilejson.hpp"
#include "fetcher.hpp"
#include "fetcher_io.hpp"
#include "util.hpp"
#include "config.h"
#include "vector_tile.pb.h"

namespace bpo = boost::program_options;
namespace bpt = boost::property_tree;
namespace bfs = boost::filesystem;

/**
 * common options for generating vector tiles. this is just a
 * utility class to keep them all in the same place and not need
 * to change many code paths if we add new ones.
 */
struct vector_options {
  unsigned int path_multiplier;
  int buffer_size;
  double scale_factor;
  unsigned int offset_x;
  unsigned int offset_y;
  unsigned int tolerance;
  std::string image_format;
  double scale_denominator;
  std::vector<std::string> ignore_layers;

  void add(bpo::options_description &options) {
    options.add_options()
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
      ("scale-denominator,d", bpo::value<double>(&scale_denominator)->default_value(0.0),
       "Override for scale denominator. A value of 0 means to use the sensible default "
       "which Mapnik will generate from the tile context.")
      ("ignore", bpo::value<std::vector<std::string> >(&ignore_layers),
       "Ignore layers with these names when deciding whether or not to recurse when "
       "bulk generating tiles.")
      ;
  }
};

/**
 * simple locked queue to track the tiles which need to be
 * generated. this is used across multiple threads, so needs to
 * be thread-safe. at high concurrency, it's possible that the
 * simple mutex lock might be contended, but given the amount of
 * time spent fetching data from a database / datasource, it's
 * likely to be a small overhead.
 */
struct tile_queue {
  int min_z, max_z, mask_z, z, x, y;
  std::mutex mutex;

  tile_queue(int min_z_, int max_z_, int mask_z_)
    : min_z(min_z_), max_z(max_z_), mask_z(mask_z_),
      z(min_z), x(0), y(0) {
  }

  // this is stateful and shared between threads, so we don't
  // want it being copied as that would lead to threads
  // duplicating work.
  tile_queue(const tile_queue &) = delete;

  // if there are any tiles remaining to be done, returns true
  // and puts the tile coordinates in `root_z`, `root_x` and
  // `root_y`. the `leaf_z` parameter is filled out with the
  // depth of the tree to recurse down to. this is only done for
  // levels above the mask zoom level, where empty tile children
  // can be omitted.
  //
  // if no tiles remain, returns false.
  bool next(int &root_z, int &root_x, int &root_y, int &leaf_z) {
    std::unique_lock<std::mutex> lock(mutex);

    if (z > mask_z) {
      return false;

    } else {
      root_z = z;
      root_x = x;
      root_y = y;

      leaf_z = (z == mask_z) ? max_z : z;

      ++x;
      if (x >= (1 << z)) {
        x = 0;
        ++y;
      }
      if (y >= (1 << z)) {
        y = 0;
        ++z;
      }

      return true;
    }
  }
};

struct generator_stopped : public std::exception {
  virtual ~generator_stopped() {}
  const char *what() const noexcept {
    return "Generator stopped by exception thrown on a different thread";
  }
};

/**
 * encapsulates logic for tile generation and storage in a
 * conventional z/x/y hierarchy. this holds the "long-lived"
 * and expensive to generate objects such as mapnik::Map
 * which don't need to be re-initialised after each tile is
 * generated.
 */
struct tile_generator {
  mapnik::Map map;
  const std::string output_dir;
  const vector_options &vopt;
  const mapnik::scaling_method_e scaling_method;
  const boost::optional<const avecado::post_processor &> pp;
  const std::unordered_set<std::string> ignore_layers;
  std::atomic<bool> &stop_all_threads;

  tile_generator(const std::string &map_file,
                 const std::string &fonts_dir,
                 const std::string &input_plugins_dir,
                 const std::string &output_dir_,
                 const vector_options &vopt_,
                 mapnik::scaling_method_e scaling_method_,
                 boost::optional<const avecado::post_processor &> pp_,
                 std::atomic<bool> &stop_all_threads_)
    : map(), output_dir(output_dir_), vopt(vopt_),
      scaling_method(scaling_method_), pp(pp_),
      ignore_layers(vopt.ignore_layers.begin(), vopt.ignore_layers.end()),
      stop_all_threads(stop_all_threads_) {

    // try to register fonts and input plugins
    mapnik::freetype_engine::register_fonts(fonts_dir);
    mapnik::datasource_cache::instance().register_datasources(input_plugins_dir);

    // load map config from disk
    mapnik::load_map(map, map_file);
  }

  // generate a tile and, if it's non-empty and max_z > root_z,
  // then generate a whole sub-tree.
  void generate(int root_z, int root_x, int root_y, int max_z) {
    bool make_subtree = make_tile(root_z, root_x, root_y);

    if (make_subtree && (root_z < max_z)) {
      generate_subtree(root_z + 1, 2 * root_x,     2 * root_y,     max_z);
      generate_subtree(root_z + 1, 2 * root_x + 1, 2 * root_y,     max_z);
      generate_subtree(root_z + 1, 2 * root_x + 1, 2 * root_y + 1, max_z);
      generate_subtree(root_z + 1, 2 * root_x,     2 * root_y + 1, max_z);
    }
  }

  // generate a recursive sub-tree starting at the root and ending
  // at `max_z`.
  void generate_subtree(int root_z, int root_x, int root_y, int max_z) {
    make_tile(root_z, root_x, root_y);

    if (root_z < max_z) {
      generate_subtree(root_z + 1, 2 * root_x,     2 * root_y,     max_z);
      generate_subtree(root_z + 1, 2 * root_x + 1, 2 * root_y,     max_z);
      generate_subtree(root_z + 1, 2 * root_x + 1, 2 * root_y + 1, max_z);
      generate_subtree(root_z + 1, 2 * root_x,     2 * root_y + 1, max_z);
    }
  }

  // generate and store a single tile, returning true if the tile
  // had some data in it and false otherwise.
  bool make_tile(int z, int x, int y) {
    if (stop_all_threads.load()) {
      throw generator_stopped();
    }

    avecado::tile tile(z, x, y);

    // setup map parameters
    map.resize(256, 256);
    map.zoom_to_box(avecado::util::box_for_tile(z, x, y));

    // actually make the vector tile
    bool painted = avecado::make_vector_tile(
      tile, vopt.path_multiplier, map, vopt.buffer_size,
      vopt.scale_factor, vopt.offset_x, vopt.offset_y,
      vopt.tolerance, vopt.image_format, scaling_method,
      vopt.scale_denominator, pp);

    // ignore the ignorable layers, if we want to ignore them
    if (painted && !ignore_layers.empty()) {
      bool ignore = true;

      // if there are no layers which aren't ignored, then we
      // can ignore the whole tile, even if it painted something.
      for (const mapnik::vector::tile_layer &layer : tile.mapnik_tile().layers()) {
        if (layer.has_name() && (ignore_layers.count(layer.name()) == 0)) {
          ignore = false;
        }
      }

      if (ignore) {
        painted = false;
      }
    }

    // serialise to file
    bfs::path output_file = (boost::format("%1%/%2%/%3%/%4%.pbf")
                             % output_dir % z % x % y).str();
    bfs::create_directories(output_file.parent_path());
    std::ofstream output(output_file.native());
    output << tile;

    return painted;
  }
};

// thread function for generating a bunch of tiles in parallel.
// this is done by sharing a queue structure and having each thread
// pull 'jobs' off it until all the tiles have been generated.
void make_vector_thread(std::shared_ptr<tile_queue> queue,
                        std::string map_file,
                        std::string fonts_dir,
                        std::string input_plugins_dir,
                        std::string output_dir,
                        vector_options vopt,
                        mapnik::scaling_method_e scaling_method,
                        boost::optional<const avecado::post_processor &> pp,
                        std::atomic<bool> &stop_all_threads) {
  try {
    tile_generator generator(map_file, fonts_dir, input_plugins_dir, output_dir,
                             vopt, scaling_method, pp, stop_all_threads);

    int root_z = 0, root_x = 0, root_y = 0, max_z = 0;
    while (queue->next(root_z, root_x, root_y, max_z)) {
      generator.generate(root_z, root_x, root_y, max_z);
    }

  } catch (const std::exception &e) {
    stop_all_threads.store(true);
    throw;

  } catch (...) {
    stop_all_threads.store(true);
    throw std::runtime_error("Unknown object thrown");
  }
}

int make_vector_bulk(int argc, char *argv[]) {
  std::string output_dir;
  std::string config_file;
  mapnik::scaling_method_e scaling_method = mapnik::SCALING_NEAR;
  vector_options vopt;
  std::string map_file;
  int min_z, max_z, mask_z, num_threads;
  std::string fonts_dir, input_plugins_dir;

  bpo::options_description options(
    "Avecado " VERSION "\n"
    "\n"
    "  Usage: avecado vector-bulk [options] <map-file> <max-z>\n"
    "\n");

  vopt.add(options);

  options.add_options()
    ("help,h", "Print this help message.")
    ("config-file,c", bpo::value<std::string>(&config_file),
     "JSON config file to specify post-processing for data layers.")
    ("output-dir,o", bpo::value<std::string>(&output_dir)->default_value("tiles"),
     "Directory to serialise the vector tiles to.")
    ("fonts", bpo::value<std::string>(&fonts_dir)->default_value(MAPNIK_DEFAULT_FONT_DIR),
     "Directory to tell Mapnik to look in for fonts.")
    ("input-plugins", bpo::value<std::string>(&input_plugins_dir)
     ->default_value(MAPNIK_DEFAULT_INPUT_PLUGIN_DIR),
     "Directory to tell Mapnik to look in for input plugins.")
    ("scaling-method,m", bpo::value<std::string>()->default_value("near"),
     "Method used to re-sample raster layers.")
    ("mask-z", bpo::value<int>(), "Mask value, below which empty tiles are discarded.")
    ("min-z", bpo::value<int>(&min_z)->default_value(0),
     "Minimum zoom level to generate.")
    ("parallel,P", bpo::value<int>(&num_threads)->default_value(1),
     "Number of parallel processes to run when generating tiles.")
    // positional arguments
    ("map-file", bpo::value<std::string>(&map_file), "Mapnik XML input file.")
    ("max-z", bpo::value<int>(&max_z), "Maximum zoom level to generate.")
    ;

  bpo::positional_options_description pos_options;
  pos_options
    .add("map-file", 1)
    .add("max-z", 1)
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
  for (auto arg : {"map-file", "max-z"}) {
    if (vm.count(arg) == 0) {
      std::cerr << "The <" << arg << "> argument was not provided, but is mandatory\n\n";
      std::cerr << options << "\n";
      return EXIT_FAILURE;
    }
  }

  if (vm.count("mask-z")) {
    mask_z = vm["mask-z"].as<int>();

  } else {
    // default is to not mask any zoom levels, which is the same as
    // setting mask = max.
    mask_z = max_z;
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

  // load post processor config if it is provided
  avecado::post_processor post_processor;
  boost::optional<const avecado::post_processor &> pp;
  if (!config_file.empty()) {
    try {
      // parse json config
      bpt::ptree config;
      bpt::read_json(config_file, config);
      // init processor
      post_processor.load(config);
      pp = post_processor;

    } catch (bpt::ptree_error const& e) {
      std::cerr << "Error while parsing config: " << config_file << std::endl;
      std::cerr << e.what() << std::endl;
      return EXIT_FAILURE;
    } catch (std::exception const& e) {
      std::cerr << "Error while loading config: " << config_file << std::endl;
      std::cerr << e.what() << std::endl;
      return EXIT_FAILURE;
    }
  }

  if (num_threads < 1) {
    std::cerr << "Number of parallel threads must be at least one." << std::endl;
    return EXIT_FAILURE;
  }

  try {
    std::shared_ptr<tile_queue> queue =
      std::make_shared<tile_queue>(min_z, max_z, mask_z);
    std::atomic<bool> stop(false);

    std::vector<std::future<void> > threads;
    for (int i = 0; i < num_threads; ++i) {
      threads.emplace_back(std::async(std::launch::async,
                                      &make_vector_thread,
                                      queue, map_file, fonts_dir, input_plugins_dir,
                                      output_dir, vopt, scaling_method, pp,
                                      std::ref(stop)));
    }

    // gather the exceptions from all the threads, but don't
    // stop gathering - we want to harvest all the errors and
    // join all the threads.
    std::exception_ptr error;
    for (auto &fut : threads) {
      try {
        fut.get();

      } catch (const generator_stopped &s) {
        std::cerr << "ERROR: Thread stopped due to exception on other thread.\n";

      } catch (const std::exception &e) {
        error = std::current_exception();
        std::cerr << "ERROR: " << e.what() << "\n";

      } catch (...) {
        error = std::current_exception();
        std::cerr << "UNKNOWN ERROR!\n";
      }
    }

    // if there was an error, re-throw it after the thread
    // resources have been collected.
    if (error) {
      std::rethrow_exception(error);
    }

  } catch (const std::exception &e) {
    std::cerr << "Unable to make vector tile: " << e.what() << "\n";
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}

int make_vector(int argc, char *argv[]) {
  std::string output_file;
  std::string config_file;
  mapnik::scaling_method_e scaling_method = mapnik::SCALING_NEAR;
  vector_options vopt;
  std::string map_file;
  int z, x, y;
  std::string fonts_dir, input_plugins_dir;

  bpo::options_description options(
    "Avecado " VERSION "\n"
    "\n"
    "  Usage: avecado vector [options] <map-file> <tile-z> <tile-x> <tile-y>\n"
    "\n");

  vopt.add(options);

  options.add_options()
    ("help,h", "Print this help message.")
    ("config-file,c", bpo::value<std::string>(&config_file),
     "JSON config file to specify post-processing for data layers.")
    ("output-file,o", bpo::value<std::string>(&output_file)->default_value("tile.pbf"),
     "File to serialise the vector tile to.")
    ("fonts", bpo::value<std::string>(&fonts_dir)->default_value(MAPNIK_DEFAULT_FONT_DIR),
     "Directory to tell Mapnik to look in for fonts.")
    ("input-plugins", bpo::value<std::string>(&input_plugins_dir)
     ->default_value(MAPNIK_DEFAULT_INPUT_PLUGIN_DIR),
     "Directory to tell Mapnik to look in for input plugins.")
    ("scaling-method,m", bpo::value<std::string>()->default_value("near"),
     "Method used to re-sample raster layers.")
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

  // load post processor config if it is provided
  avecado::post_processor post_processor;
  boost::optional<const avecado::post_processor &> pp;
  if (!config_file.empty()) {
    try {
      // parse json config
      bpt::ptree config;
      bpt::read_json(config_file, config);
      // init processor
      post_processor.load(config);
      pp = post_processor;

    } catch (bpt::ptree_error const& e) {
      std::cerr << "Error while parsing config: " << config_file << std::endl;
      std::cerr << e.what() << std::endl;
      return EXIT_FAILURE;
    } catch (std::exception const& e) {
      std::cerr << "Error while loading config: " << config_file << std::endl;
      std::cerr << e.what() << std::endl;
      return EXIT_FAILURE;
    }
  }

  try {
    mapnik::Map map;
    avecado::tile tile(z, x, y);

    // try to register fonts and input plugins
    mapnik::freetype_engine::register_fonts(fonts_dir);
    mapnik::datasource_cache::instance().register_datasources(input_plugins_dir);

    // load map config from disk
    mapnik::load_map(map, map_file);

    // setup map parameters
    map.resize(256, 256);
    map.zoom_to_box(avecado::util::box_for_tile(z, x, y));

    // actually make the vector tile
    avecado::make_vector_tile(tile, vopt.path_multiplier, map, vopt.buffer_size,
                              vopt.scale_factor, vopt.offset_x, vopt.offset_y,
                              vopt.tolerance, vopt.image_format, scaling_method,
                              vopt.scale_denominator, pp);

    // serialise to file
    std::ofstream output(output_file);
    output << tile;

  } catch (const std::exception &e) {
    std::cerr << "Unable to make vector tile: " << e.what() << "\n";
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}

int make_raster(int argc, char *argv[]) {
  unsigned int z = 0, x = 0, y = 0;
  double scale_factor = 1.0;
  unsigned int buffer_size = 0;
  unsigned int width = 256, height = 256;
  std::string tilejson_uri, output_file, map_file;
  std::string fonts_dir, input_plugins_dir;

  bpo::options_description options(
    "Avecado " VERSION "\n"
    "\n"
    "  Usage: avecado raster [options] <tilejson> <map-file> <tile-z> <tile-x> <tile-y>\n"
    "\n");

  options.add_options()
    ("help,h", "Print this help message.")
    ("output-file,o", bpo::value<std::string>(&output_file)->default_value("tile.png"),
     "File to write PNG data to.")
    ("buffer-size,b", bpo::value<unsigned int>(&buffer_size)->default_value(0),
     "Number of pixels around the tile to buffer in order to allow for features "
     "whose rendering effects extend beyond the geometric extent.")
    ("scale_factor,s", bpo::value<double>(&scale_factor)->default_value(1.0),
     "Scale factor to multiply style values by.")
    ("fonts", bpo::value<std::string>(&fonts_dir)->default_value(MAPNIK_DEFAULT_FONT_DIR),
     "Directory to tell Mapnik to look in for fonts.")
    ("input-plugins", bpo::value<std::string>(&input_plugins_dir)
     ->default_value(MAPNIK_DEFAULT_INPUT_PLUGIN_DIR),
     "Directory to tell Mapnik to look in for input plugins.")
    ("width", bpo::value<unsigned int>(&width), "Width of output raster.")
    ("height", bpo::value<unsigned int>(&height), "Height of output raster.")
    // positional arguments
    ("tilejson", bpo::value<std::string>(&tilejson_uri),
     "TileJSON config file URI to specify where to get vector tiles from.")
    ("map-file", bpo::value<std::string>(&map_file), "Mapnik XML input file.")
    ("tile-z", bpo::value<unsigned int>(&z), "Zoom level.")
    ("tile-x", bpo::value<unsigned int>(&x), "Tile x coordinate.")
    ("tile-y", bpo::value<unsigned int>(&y), "Tile x coordinate.")
    ;

  bpo::positional_options_description pos_options;
  pos_options
    .add("tilejson", 1)
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
  for (auto arg : {"tilejson", "map-file", "tile-z", "tile-x", "tile-y"}) {
    if (vm.count(arg) == 0) {
      std::cerr << "The <" << arg << "> argument was not provided, but is mandatory\n\n";
      std::cerr << options << "\n";
      return EXIT_FAILURE;
    }
  }

  try {
    mapnik::Map map;

    // try to register fonts and input plugins
    mapnik::freetype_engine::register_fonts(fonts_dir);
    mapnik::datasource_cache::instance().register_datasources(input_plugins_dir);

    // load map config from disk
    mapnik::load_map(map, map_file);

    // setup map parameters
    map.resize(width, height);
    map.zoom_to_box(avecado::util::box_for_tile(z, x, y));

    bpt::ptree conf = avecado::tilejson(tilejson_uri);
    std::unique_ptr<avecado::fetcher> fetcher = avecado::make_tilejson_fetcher(conf);

    avecado::fetch_response response = (*fetcher)(z, x, y).get();

    if (response.is_left()) {
      mapnik::image_32 image(width, height);

      std::unique_ptr<avecado::tile> tile(std::move(response.left()));
      avecado::render_vector_tile(image, *tile, map, scale_factor, buffer_size);
      mapnik::save_to_file(image, output_file, "png");

    } else {
      throw std::runtime_error((boost::format("Error while fetching tile: %1%")
                                % response.right()).str());
    }

  } catch (const std::exception &e) {
    std::cerr << "Unable to render raster tile: " << e.what() << "\n";
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}

int main(int argc, char *argv[]) {
  if (argc > 1) {
    std::string command = argv[1];

    // urgh, removing a value from argv is a bit of a pain...
    const int new_argc = argc - 1;
    char **new_argv = (char **)alloca(sizeof(*argv) * new_argc);
    new_argv[0] = argv[0];
    for (int i = 1; i < new_argc; ++i) {
      new_argv[i] = argv[i+1];
    }

    if (command == "vector-bulk") {
      return make_vector_bulk(new_argc, new_argv);

    } else if (command == "vector") {
      return make_vector(new_argc, new_argv);

    } else if (command == "raster") {
      return make_raster(new_argc, new_argv);

    } else {
      std::cerr << "Unknown command \"" << command << "\".\n";
    }
  }

  std::cerr <<
    "avecado <command> [command-options]\n"
    "\n"
    "Where command is:\n"
    "  vector-bulk: Avecado will make a range of vector tiles from a\n"
    "               Mapnik XML file and export them as PBFs.\n"
    "  vector: Avecado will make a single vector tile from a Mapnik\n"
    "          XML file and export it as a PBF.\n"
    "  raster: Avecado will make raster tiles from vector tiles\n"
    "          plus a style file.\n"
    "\n"
    "To get more information on the options available for a\n"
    "particular command, run `avecado <command> --help`.\n";
  return EXIT_FAILURE;
}
