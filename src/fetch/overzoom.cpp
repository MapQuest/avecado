#include "fetch/overzoom.hpp"

namespace avecado { namespace fetch {

overzoom::overzoom(std::unique_ptr<fetcher> &&source, int max_zoom, boost::optional<int> mask_zoom)
  : m_source(std::move(source))
  , m_max_zoom(max_zoom)
  , m_mask_zoom(mask_zoom) {
}

overzoom::~overzoom() {
}

std::future<fetch_response> overzoom::operator()(const request &r) {
  request req(r);

  if (req.z > m_max_zoom) {
    // zoom "out" to max zoom, as we're guaranteed not to find
    // any tiles a z > max zoom.
    req.x >>= (req.z - m_max_zoom);
    req.y >>= (req.z - m_max_zoom);
    req.z = m_max_zoom;
  }

  std::future<fetch_response> upstream_future((*m_source)(req));

  return std::async([this, req](std::future<fetch_response> &&fut) -> fetch_response {
      fetch_response resp(fut.get());

      // if the tile isn't available, we try again, at the mask
      // zoom level (as long as it's zoomed 'out').
      if (bool(m_mask_zoom) &&
          (req.z > *m_mask_zoom) &&
          resp.is_right() &&
          (resp.right().status == fetch_status::not_found)) {
        const int mask_zoom = *m_mask_zoom;
        request masked(req);
        masked.x >>= masked.z - mask_zoom;
        masked.y >>= masked.z - mask_zoom;
        masked.z = mask_zoom;

        resp = ((*m_source)(masked)).get();
      }

      return resp;
    }, std::move(upstream_future));
}

} } // namespace avecado::fetch
