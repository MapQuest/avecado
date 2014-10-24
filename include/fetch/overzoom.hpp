#ifndef FETCHER_OVERZOOM_HPP
#define FETCHER_OVERZOOM_HPP

#include "fetcher.hpp"

#include <memory>

namespace avecado { namespace fetch {

/* Fetcher which supports 'overzoom', that is using tiles from
 * a lower zoom level when tiles at the desired zoom level are
 * missing.
 */
struct overzoom : public fetcher {
  overzoom(std::unique_ptr<fetcher> &&source, int max_zoom, boost::optional<int> mask_zoom);
  virtual ~overzoom();

  std::future<fetch_response> operator()(int z, int x, int y);

private:
  std::unique_ptr<fetcher> m_source;
  int m_max_zoom;
  boost::optional<int> m_mask_zoom;
};

} } // namespace avecado::fetch

#endif /* FETCHER_OVERZOOM_HPP */
