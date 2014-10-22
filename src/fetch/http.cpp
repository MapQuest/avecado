#include "fetch/http.hpp"
#include "vector_tile.pb.h"
#include <boost/format.hpp>
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

}

struct http::impl {
  impl(const std::string &base_url, const std::string &ext);

  fetch_response fetch(int z, int x, int y);

  std::string m_base_url, m_ext;
};

http::impl::impl(const std::string &base_url, const std::string &ext)
  : m_base_url(base_url), m_ext(ext) {
}

fetch_response http::impl::fetch(int z, int x, int y) {
  std::string url = (boost::format("%1%/%2%/%3%/%4%.%5%")
                     % m_base_url % z % x % y % m_ext).str();

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

http::http(const std::string &base_url, const std::string &ext)
  : m_impl(new impl(base_url, ext)) {
}

http::~http() {
}

fetch_response http::operator()(int z, int x, int y) {
  return m_impl->fetch(z, x, y);
}

} } // namespace avecado::fetch
