#include "tile.hpp"
#include "vector_tile.pb.h"

#include <google/protobuf/io/zero_copy_stream_impl.h>
#include <google/protobuf/io/gzip_stream.h>

namespace avecado {

tile::tile(unsigned int z_, unsigned int x_, unsigned int y_)
  : z(z_), x(x_), y(y_), m_mapnik_tile(new mapnik::vector::tile) {
}

tile::~tile() {
}

std::string tile::get_data(int compression_level) const {
  std::ostringstream buffer;
  buffer << tile_gzip(*this, compression_level);
  return buffer.str();
}

void tile::from_string(const std::string &str) {
  tile t(z, x, y);
  std::istringstream buffer(str);
  buffer >> t;
  m_mapnik_tile.swap(t.m_mapnik_tile);
}

mapnik::vector::tile const &tile::mapnik_tile() const {
  return *m_mapnik_tile;
}

mapnik::vector::tile &tile::mapnik_tile() {
  return *m_mapnik_tile;
}

std::istream &operator>>(std::istream &in, tile &t) {
  google::protobuf::io::IstreamInputStream stream(&in);
  google::protobuf::io::GzipInputStream gz_stream(&stream);
  bool read_ok = t.mapnik_tile().ParseFromZeroCopyStream(&gz_stream);

  if (!read_ok) {
    throw std::runtime_error("Unable to read tile from input stream.");
  }

  return in;
}

std::ostream &operator<<(std::ostream &out, const tile_gzip &t) {
  google::protobuf::io::OstreamOutputStream stream(&out);

  google::protobuf::io::GzipOutputStream::Options options;
  if (t.compression_level_ >= 0) {
    options.compression_level = t.compression_level_;
  }
  options.format = google::protobuf::io::GzipOutputStream::ZLIB;
  google::protobuf::io::GzipOutputStream gz_stream(&stream, options);

  bool write_ok = t.tile_.mapnik_tile().SerializeToZeroCopyStream(&gz_stream);

  if (!write_ok) {
    throw std::runtime_error("Unable to write tile to output stream.");
  }

  return out;
}

} // namespace avecado
