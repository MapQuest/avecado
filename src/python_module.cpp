#include <boost/python.hpp>

#include "avecado.hpp"

using namespace boost::python;

namespace {

str mk_tile(object py_map,
            int z, int x, int y,
            int tile_size,
            unsigned int path_multiplier,
            int buffer_size,
            double scale_factor,
            unsigned int offset_x,
            unsigned int offset_y,
            unsigned int tolerance,
            std::string image_format,
            std::string scaling_method_str,
            double scale_denominator) {

  mapnik::Map const &map = extract<mapnik::Map const &>(py_map);
  avecado::tile tile(x, y, z, tile_size);

  mapnik::scaling_method_e scaling_method = mapnik::SCALING_NEAR;
  boost::optional<mapnik::scaling_method_e> method =
    mapnik::scaling_method_from_string(scaling_method_str);
  if (!method) {
    std::ostringstream err;
    err << "The string \"" << scaling_method_str << "\" was not recognised as a "
        << "valid scaling method by Mapnik.";
    throw std::runtime_error(err.str());
  }

  avecado::make_vector_tile(tile, path_multiplier, map, buffer_size,
                            scale_factor, offset_x, offset_y,
                            tolerance, image_format, scaling_method,
                            scale_denominator);

  std::string buffer = tile.get_data();
  return str(buffer.data(), buffer.size());
}

} // anonymous namespace

BOOST_PYTHON_MODULE(avecado) {
  def("make_vector_tile", mk_tile,
      (arg("z") = 0,
       arg("x") = 0,
       arg("y") = 0,
       arg("tile_size") = 256,
       arg("path_multiplier") = 16,
       arg("buffer_size") = 0,
       arg("scale_factor") = 1.0,
       arg("offset_x") = 0,
       arg("offset_y") = 0,
       arg("tolerance") = 1,
       arg("image_format") = "jpeg",
       arg("scaling_method") = "near",
       arg("scale_denominator") = 0.0),
      "Make a vector tile from a Mapnik map\n"
      "object and return the serialised PBF.\n"
      "\n"
      "Note that you need to import mapnik\n"
      "before using avecado.\n"
      "\n"
      "Usage:\n"
      ">>> import mapnik\n"
      ">>> import avecado\n"
      ">>> m = mapnik.Map(256, 256)\n"
      ">>> mapnik.load_map(m, 'style.xml')\n"
      ">>> m.zoom_to_box(mapnik.Box2d(-100, -100, 100, 100))\n"
      ">>> t = avecado.make_vector_tile(m, 18, 75501, 99191, 256, path_multiplier = 16, scale_factor = 1.0)\n"
    );
}
