#include <boost/program_options.hpp>
#include <boost/bind.hpp>
#include <boost/asio.hpp>

#include <mapnik/utils.hpp>
#include <mapnik/load_map.hpp>
#include <mapnik/font_engine_freetype.hpp>
#include <mapnik/datasource_cache.hpp>

#include "avecado.hpp"
#include "config.h"


namespace bpo = boost::program_options;
using boost::asio::ip::tcp;

class session
{
public:
  session(boost::asio::io_service& io_service)
    : socket_(io_service)
  {
  }

  tcp::socket& socket()
  {
    return socket_;
  }

  void start()
  {
    socket_.async_read_some(boost::asio::buffer(data_, max_length),
        boost::bind(&session::handle_read, this,
          boost::asio::placeholders::error,
          boost::asio::placeholders::bytes_transferred));
  }

private:
  void handle_read(const boost::system::error_code& error,
      size_t bytes_transferred)
  {
    if (!error)
    {
      boost::asio::async_write(socket_,
          boost::asio::buffer(data_, bytes_transferred),
          boost::bind(&session::handle_write, this,
            boost::asio::placeholders::error));
    }
    else
    {
      delete this;
    }
  }

  void handle_write(const boost::system::error_code& error)
  {
    if (!error)
    {
      socket_.async_read_some(boost::asio::buffer(data_, max_length),
          boost::bind(&session::handle_read, this,
            boost::asio::placeholders::error,
            boost::asio::placeholders::bytes_transferred));
    }
    else
    {
      delete this;
    }
  }

  tcp::socket socket_;
  enum { max_length = 1024 };
  char data_[max_length];
};

class server
{
public:
  server(boost::asio::io_service& io_service, short port)
    : io_service_(io_service),
      acceptor_(io_service, tcp::endpoint(tcp::v4(), port))
  {
    start_accept();
  }

private:
  void start_accept()
  {
    session* new_session = new session(io_service_);
    acceptor_.async_accept(new_session->socket(),
        boost::bind(&server::handle_accept, this, new_session,
          boost::asio::placeholders::error));
  }

  void handle_accept(session* new_session,
      const boost::system::error_code& error)
  {
    if (!error)
    {
      new_session->start();
    }
    else
    {
      delete new_session;
    }

    start_accept();
  }

  boost::asio::io_service& io_service_;
  tcp::acceptor acceptor_;
};

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
  std::string map_file;
  short port;
  unsigned short thread_hint;
  std::string fonts_dir, input_plugins_dir;

  bpo::options_description options(
    "Avecado " VERSION "\n"
    "\n"
    "  Usage: avecado_server [options] <map-file> <port>\n"
    "\n");

  options.add_options()
	("help,h", "Print this help message.")
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
	("port", bpo::value<short>(&port), "Port upon which the server will listen.")
	("thread-hint", bpo::value<unsigned short>(&thread_hint), "Hint at the number of asynchronous "
	"requests the server should be able to service.")
	;

  bpo::positional_options_description pos_options;
  pos_options
    .add("map-file", 1)
    .add("port", 1)
    .add("thread-hint", 1)
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
  for (auto arg : {"map-file", "port", "thread-hint"}) {
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


  //start up the server
  try {
    boost::asio::io_service io_service(thread_hint);
    server s(io_service, atoi(argv[1]));
    io_service.run();
  }
  catch (std::exception& e)
  {
    std::cerr << "Exception: " << e.what() << "\n";
  }

  return EXIT_SUCCESS;
}
