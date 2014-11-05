#include "fetch/overzoom.hpp"

namespace avecado { namespace fetch {

overzoom::overzoom(std::unique_ptr<fetcher> &&source, int max_zoom, boost::optional<int> mask_zoom)
  : m_source(std::move(source))
  , m_max_zoom(max_zoom)
  , m_mask_zoom(mask_zoom) {
}

overzoom::~overzoom() {
}

std::future<fetch_response> overzoom::operator()(int z, int x, int y) {
  if (z > m_max_zoom) {
    // zoom "out" to max zoom, as we're guaranteed not to find
    // any tiles a z > max zoom.
    x >>= (z - m_max_zoom);
    y >>= (z - m_max_zoom);
    z = m_max_zoom;
  }

  std::future<fetch_response> upstream_future((*m_source)(z, x, y));

  return std::async([this, z, x, y](std::future<fetch_response> &&fut) -> fetch_response {
      fetch_response resp(fut.get());

      // if the tile isn't available, we try again, at the mask
      // zoom level (as long as it's zoomed 'out').
      if (bool(m_mask_zoom) &&
          (z > *m_mask_zoom) &&
          resp.is_right() &&
          (resp.right().status == fetch_status::not_found)) {
        int mask_x = x >> (z - *m_mask_zoom);
        int mask_y = y >> (z - *m_mask_zoom);
        int mask_z = *m_mask_zoom;

        resp = ((*m_source)(mask_z, mask_x, mask_y)).get();
      }

      return resp;
    }, std::move(upstream_future));
}

} } // namespace avecado::fetch
