#include "tilejson.hpp"
#include "fetch/overzoom.hpp"
#include "fetch/http.hpp"

#include <boost/format.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/property_tree/json_parser.hpp>

#include <curl/curl.h>

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

} // namespace avecado
