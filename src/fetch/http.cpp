#include "fetch/http.hpp"
#include <boost/format.hpp>
#include <curl/curl.h>

namespace avecado { namespace fetch {

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
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
 
    CURLcode res = curl_easy_perform(curl);

    if (res != CURLE_OK) {
      if (res == CURLE_REMOTE_FILE_NOT_FOUND) {
        err.status = fetch_status::not_found;
      }
      response = fetch_response(err);

    } else {
      std::unique_ptr<tile> ptr(new tile);
      response = fetch_response(std::move(ptr));
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
