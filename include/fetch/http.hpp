#ifndef FETCHER_HTTP_HPP
#define FETCHER_HTTP_HPP

#include "fetcher.hpp"

#include <memory>
#include <string>

namespace avecado { namespace fetch {

/* Fetcher which fetches tiles from URLs.
 */
struct http : public fetcher {
  // patterns of the form ${base_url}/${z}/${x}/${y}.${ext}
  http(const std::string &base_url, const std::string &ext);
  // patterns of a more general form
  explicit http(std::vector<std::string> &&patterns);

  virtual ~http();

  std::future<fetch_response> operator()(int z, int x, int y);

private:
  struct impl;
  std::unique_ptr<impl> m_impl;
};

} } // namespace avecado::fetch

#endif /* FETCHER_HTTP_HPP */
