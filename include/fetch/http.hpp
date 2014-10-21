#ifndef FETCHER_HTTP_HPP
#define FETCHER_HTTP_HPP

#include "fetcher.hpp"

#include <memory>
#include <string>

namespace avecado { namespace fetch {

/* Fetcher which fetches tiles from URLs of the form:
 *   ${base_url}/${z}/${x}/${y}.${ext}
 */
struct http : public fetcher {
  http(const std::string &base_url, const std::string &ext);
  virtual ~http();

  fetch_response operator()(int z, int x, int y);

private:
  struct impl;
  std::unique_ptr<impl> m_impl;
};

} } // namespace avecado::fetch

#endif /* FETCHER_HTTP_HPP */
