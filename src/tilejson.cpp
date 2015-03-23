#include "tilejson.hpp"
#include "fetch/overzoom.hpp"
#include "fetch/http.hpp"

#include <boost/format.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/property_tree/json_parser.hpp>

#include <curl/curl.h>

#include <mapnik/map.hpp>
#include <mapnik/layer.hpp>
#include <mapnik/feature_layer_desc.hpp>
#include <mapnik/datasource.hpp>

#include <unordered_set>

namespace bpt = boost::property_tree;
namespace bal = boost::algorithm;

namespace {

size_t write_callback(char *ptr, size_t size, size_t nmemb, void *userdata) {
  std::stringstream *stream = static_cast<std::stringstream*>(userdata);
  size_t total_bytes = size * nmemb;
  stream->write(ptr, total_bytes);
  return stream->good() ? total_bytes : 0;
}

#define CURL_SETOPT(curl, opt, arg) { \
  CURLcode res = curl_easy_setopt((curl), (opt), (arg)); \
  if (res != CURLE_OK) { \
    throw std::runtime_error("Unable to set cURL option " #opt); \
  } \
}

struct http_client {
  CURL *m_curl;
  char m_error_buffer[CURL_ERROR_SIZE];

  http_client() : m_curl(curl_easy_init()) {
    if (m_curl == nullptr) {
      throw std::runtime_error("unable to initialise the cURL easy handle");
    }
  }

  ~http_client() {
    if (m_curl != nullptr) {
      curl_easy_cleanup(m_curl);
      m_curl = nullptr;
    }
  }

  long get(const std::string &uri, std::stringstream &stream) {
    CURL_SETOPT(m_curl, CURLOPT_URL, uri.c_str());
    CURL_SETOPT(m_curl, CURLOPT_WRITEFUNCTION, write_callback);
    CURL_SETOPT(m_curl, CURLOPT_WRITEDATA, &stream);
    CURL_SETOPT(m_curl, CURLOPT_ERRORBUFFER, &m_error_buffer[0]);

    // get curl to send the Accept-Encoding header, and also transparently
    // handle decoding before passing the data back to us.
    CURL_SETOPT(m_curl, CURLOPT_ACCEPT_ENCODING, "gzip");

    CURLcode res = curl_easy_perform(m_curl);
    if (res != CURLE_OK) {
      throw std::runtime_error((boost::format("cURL operation failed: %1%") % m_error_buffer).str());
    }

    long status_code = 0;
    curl_easy_getinfo(m_curl, CURLINFO_RESPONSE_CODE, &status_code);
    return status_code;
  }
};

#undef CURL_SETOPT

} // anonymous namespace

namespace avecado {

bpt::ptree tilejson(const std::string &uri) {
  http_client client;
  std::stringstream data;

  long status_code = client.get(uri, data);
  if ((status_code != 200) &&
      // note: this is a bit of a hack to allow local file paths to work - it
      // seems that curl won't synthesize an HTTP status code, so we need to
      // hack around it.
      ((status_code != 0) || (!bal::starts_with(uri, "file:")))) {
    throw std::runtime_error((boost::format("Unable to fetch TileJSON \"%1%\": HTTP status %2%.") % uri % status_code).str());
  }

  bpt::ptree conf;
  bpt::read_json(data, conf);
  return conf;
}

std::unique_ptr<fetcher> make_tilejson_fetcher(const bpt::ptree &conf) {
  // parameters relating to the overzoom functionality
  int max_zoom = conf.get<int>("maxzoom", 22);
  boost::optional<int> mask_zoom = conf.get_optional<int>("maskLevel");

  // patterns for HTTP fetching
  std::vector<std::string> patterns;
  const bpt::ptree &tile_uris = conf.get_child("tiles");
  for (auto const &pattern : tile_uris) {
    patterns.push_back(pattern.second.data());
  }

  // construct fetchers
  std::unique_ptr<fetcher> http(new fetch::http(std::move(patterns)));
  std::unique_ptr<fetcher> overzoom(new fetch::overzoom(std::move(http), max_zoom, mask_zoom));

  return overzoom;
}

namespace {

struct json_converter : public mapnik::util::static_visitor<> {
  std::ostream &out;

  json_converter(std::ostream &o) : out(o) {}

  void operator()(const mapnik::value_null &) const {
    out << "null";
  }

  void operator()(const mapnik::value_bool &b) const {
    if (b) {
      out << "true";
    } else {
      out << "false";
    }
  }

  void operator()(const mapnik::value_integer &i) const {
    out << i;
  }

  void operator()(const mapnik::value_double &f) const {
    out << std::setprecision(16) << f;
  }

  void operator()(const std::string &s) const {
    out << "\"" << s << "\"";
  }
};

struct force_integer : public mapnik::util::static_visitor<mapnik::value_integer> {
  mapnik::value_integer operator()(const mapnik::value_null &) const {
    return mapnik::value_integer(0);
  }

  mapnik::value_integer operator()(const mapnik::value_bool &b) const {
    return mapnik::value_integer(b ? 1 : 0);
  }

  mapnik::value_integer operator()(const mapnik::value_integer &i) const {
    return i;
  }

  mapnik::value_integer operator()(const mapnik::value_double &f) const {
    return mapnik::value_integer(f);
  }

  mapnik::value_integer operator()(const std::string &s) const {
    int result = 0;
    if (mapnik::util::string2int(s, result)) {
      return mapnik::value_integer(result);

    } else {
      throw std::runtime_error((boost::format("Could not parse \"%1%\" as integer")
                                % s).str());
    }
  }
};

mapnik::parameters make_default_parameters() {
  mapnik::parameters defaults;

  defaults.emplace(std::string("minzoom"), mapnik::value_integer(0));
  defaults.emplace(std::string("maxzoom"), mapnik::value_integer(0));
  defaults.emplace(std::string("format"), std::string("pbf"));
  defaults.emplace(std::string("name"), std::string("Avecado Development Server"));
  defaults.emplace(std::string("private"), mapnik::value_bool(true));
  defaults.emplace(std::string("scheme"), std::string("xyz"));
  defaults.emplace(std::string("tilejson"), std::string("2.0.0"));

  return defaults;
}

} // anonymous namespace

std::string make_tilejson(const mapnik::Map &map,
                          const std::string &base_url) {
  static const mapnik::parameters defaults = make_default_parameters();
  static const std::unordered_set<std::string> array_keys = {"center", "bounds"};

  // TODO: remove super-hacky hard-coded 'JSON' serialiser and use
  // a proper one.
  std::ostringstream out;
  out << "{";

  // copy Mapnik's parameters to make some changes and perhaps add
  // some default values if they're not already present.
  mapnik::parameters params = map.get_extra_parameters();

  // force some parameters to be integers.
  for (auto const &key : {"metatile", "maskLevel", "maxzoom", "minzoom"}) {
    mapnik::parameters::iterator itr = params.find(key);
    if (itr != params.end()) {
      itr->second = mapnik::util::apply_visitor(force_integer(), itr->second);
    }
  }

  // apply some defaults
  for (auto const &row : defaults) {
    mapnik::parameters::iterator itr = params.find(row.first);
    if (itr == params.end()) {
      params.emplace(row);
    }
  }

  // maskLevel is a bit special - we want to default that to maxzoom
  // if it's not already specified.
  {
    mapnik::parameters::iterator itr = params.find("maskLevel");
    if (itr == params.end()) {
      // we're guaranteed maxzoom exists, as it's inserted as a
      // default if it wasn't provided by the Map.
      params.emplace("maskLevel", params.find("maxzoom")->second);
    }
  }

  for (auto const &row : params) {
    out << "\"" << row.first << "\":";
    if (array_keys.count(row.first) > 0) {
      out << "[" << row.second.get<std::string>() << "]";
    } else {
      mapnik::util::apply_visitor(json_converter(out), row.second);
    }
    out << ",";
  }

  out << "\"tiles\": [";
  out << "\"" << base_url << "/{z}/{x}/{y}.pbf\"";
  out << "],";

  out << "\"vector_layers\":[";
  bool first = true;
  for (auto const &layer : map.layers()) {
    if (!layer.active()) {
      continue;
    }

    if (first) {
      first = false;
    } else {
      out << ",";
    }

    out << "{";
    out << "\"id\": \"" << layer.name() << "\",";
    out << "\"description\": \"\","; // layer description

    bool field_first = true;
    mapnik::layer_descriptor desc = layer.datasource()->get_descriptor();
    out << "\"fields\":{";
    for (auto const &attr : desc.get_descriptors()) {
      if (field_first) {
        field_first = false;
      } else {
        out << ",";
      }

      out << "\"" << attr.get_name() << "\": \"\"";
    }
    out << "}}";
  }
  out << "]";

  out << "}";
  return out.str();
}

} // namespace avecado
