#include "fetch/http.hpp"
#include "vector_tile.pb.h"

#include <boost/format.hpp>
#include <boost/algorithm/string/find_format.hpp>
#include <boost/xpressive/xpressive.hpp>

#include <sstream>
#include <google/protobuf/io/zero_copy_stream.h>
#include <google/protobuf/io/zero_copy_stream_impl.h>

#include <curl/curl.h>

namespace avecado { namespace fetch {

namespace {

size_t write_callback(char *ptr, size_t size, size_t nmemb, void *userdata) {
  std::stringstream *stream = static_cast<std::stringstream*>(userdata);
  size_t total_bytes = size * nmemb;
  stream->write(ptr, total_bytes);
  return stream->good() ? total_bytes : 0;
}

std::vector<std::string> singleton(const std::string &base_url, const std::string &ext) {
  std::vector<std::string> vec;
  vec.push_back((boost::format("%1%/{z}/{x}/{y}.%2%") % base_url % ext).str());
  return vec;
}

struct formatter {
  int z, x, y;

  formatter(int z_, int x_, int y_) : z(z_), x(x_), y(y_) {}

  template<typename Out>
  Out operator()(boost::xpressive::smatch const &what, Out out) const {
    int val = 0;

    char c = what[1].str()[0];
    if      (c == 'z') { val = z; }
    else if (c == 'x') { val = x; }
    else if (c == 'y') { val = y; }
    else { throw std::runtime_error("match failed"); }

    std::string sub = (boost::format("%1%") % val).str();
    out = std::copy(sub.begin(), sub.end(), out);

    return out;
  }
};

} // anonymous namespace

struct http::impl {
  impl(std::vector<std::string> &&patterns);

  fetch_response fetch(int z, int x, int y);

  std::string url_for(int z, int x, int y) const;

  std::vector<std::string> m_url_patterns;
};

http::impl::impl(std::vector<std::string> &&patterns)
  : m_url_patterns(patterns) {
}

fetch_response http::impl::fetch(int z, int x, int y) {
  std::string url = url_for(z, x, y);

  fetch_error err;
  err.status = fetch_status::server_error;
  fetch_response response(err);

  CURL *curl = curl_easy_init();
  if (curl != nullptr) {
    std::stringstream stream;

    CURLcode res = curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    if (res != CURLE_OK) { return response; }
 
    res = curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    if (res != CURLE_OK) { return response; }

    res = curl_easy_setopt(curl, CURLOPT_WRITEDATA, &stream);
    if (res != CURLE_OK) { return response; }

    res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
      if (res == CURLE_REMOTE_FILE_NOT_FOUND) {
        err.status = fetch_status::not_found;
      }
      response = fetch_response(err);

    } else {
      long status_code = 0;
      curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status_code);
      
      if (status_code == 200) {
        std::unique_ptr<tile> ptr(new tile);
        google::protobuf::io::IstreamInputStream gstream(&stream);
        if (!ptr->mapnik_tile().ParseFromZeroCopyStream(&gstream)) {
          return response;
        }
        response = fetch_response(std::move(ptr));

      } else {
        switch (status_code) {
        case 400: err.status = fetch_status::bad_request; break;
        case 404: err.status = fetch_status::not_found; break;
        case 501: err.status = fetch_status::not_implemented; break;
        default:
          err.status = fetch_status::server_error;
        }
        response = fetch_response(err);
      }
    }
 
    curl_easy_cleanup(curl);
  }

  return response;
}

std::string http::impl::url_for(int z, int x, int y) const {
  using namespace boost::xpressive;

  if (m_url_patterns.empty()) {
    throw std::runtime_error("no URL patterns in fetcher");
  }

  sregex var = "{" >> (s1 = range('x','z')) >> "}";
  return regex_replace(m_url_patterns[0], var, formatter(z, x, y));
}

http::http(const std::string &base_url, const std::string &ext)
  : m_impl(new impl(singleton(base_url, ext))) {
}

http::http(std::vector<std::string> &&patterns)
  : m_impl(new impl(std::move(patterns))) {
}

http::~http() {
}

fetch_response http::operator()(int z, int x, int y) {
  return m_impl->fetch(z, x, y);
}

} } // namespace avecado::fetch
