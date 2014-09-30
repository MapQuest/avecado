#include "post_process/adminizer.hpp"

#include <mapnik/params.hpp>
#include <mapnik/datasource.hpp>
#include <mapnik/datasource_cache.hpp>
#include <mapnik/featureset.hpp>

#include <boost/geometry.hpp>
#include <boost/geometry/geometries/point.hpp>
#include <boost/geometry/geometries/box.hpp>
#include <boost/geometry/index/rtree.hpp>

// NOTE: this is included only because it's where mapnik::coord2d is
// adapted to work with the boost::geometry stuff. we don't actually
// clip any polygons.
#include <mapnik/polygon_clipper.hpp>

namespace bg = boost::geometry;
namespace bgi = boost::geometry::index;

using point_2d = bg::model::point<double, 2, bg::cs::cartesian>;
using box_2d = bg::model::box<point_2d>;
using polygon_2d = bg::model::polygon<point_2d>;

namespace {

typedef std::pair<box_2d, unsigned int> value;
typedef std::pair<polygon_2d, mapnik::value> entry;

struct param_update_iterator {
  mapnik::feature_ptr m_feature;
  polygon_2d &m_poly;
  const std::string &m_param_name;
  const std::vector<entry> &m_entries;
  bool m_keep_matching;

  param_update_iterator(mapnik::feature_ptr f, polygon_2d &poly,
                        const std::vector<entry> &entries,
                        const std::string &param_name)
    : m_feature(f), m_poly(poly), m_param_name(param_name)
    , m_entries(entries), m_keep_matching(true) {
  }

  param_update_iterator &operator++() { // prefix
    return *this;
  }

  param_update_iterator &operator*() {
    return *this;
  }

  param_update_iterator &operator=(const value &v) {
    const entry &e = m_entries[v.second];

    // do detailed intersection test
    if (bg::intersects(m_poly, e.first)) {
      // at least one result -> some intersection
      m_feature->put_new(m_param_name, e.second);
      m_keep_matching = false;
    }

    return *this;
  }
};

} // anonymous namespace

namespace avecado {
namespace post_process {

/**
 * Post-process that applies administrative region attribution
 * to features, based on geographic location of the geometry.
 */
class adminizer : public izer {
public:
  adminizer(pt::ptree const& config);
  virtual ~adminizer();

  virtual void process(std::vector<mapnik::feature_ptr> &layer) const;

private:
  mapnik::box2d<double> envelope(const std::vector<mapnik::feature_ptr> &layer) const;
  polygon_2d make_boost_polygon(const mapnik::geometry_type &geom) const;

  std::string m_param_name;
  std::shared_ptr<mapnik::datasource> m_datasource;
};

adminizer::adminizer(pt::ptree const& config)
  : m_param_name(config.get<std::string>("param_name")) {
  mapnik::parameters params;

  boost::optional<pt::ptree const &> datasource_config =
    config.get_child_optional("datasource");

  if (datasource_config) {
    for (auto &kv : *datasource_config) {
      params[kv.first] = kv.second.data();
    }
  }

  m_datasource = mapnik::datasource_cache::instance().create(params);
}

adminizer::~adminizer() {
}

void adminizer::process(std::vector<mapnik::feature_ptr> &layer) const {
  typedef bgi::rtree<value, bgi::quadratic<16> > rtree;

  // build extent of all features in layer
  mapnik::box2d<double> env = envelope(layer);

  // query the datasource
  // TODO: do we want to pass more things like scale denominator
  // and resolution type?
  mapnik::featureset_ptr fset = m_datasource->features(mapnik::query(env));

  // construct an index over the bounding boxes of the geometry,
  // first extracting the geometries from mapnik's representation
  // and transforming them too boost::geometry's representation.
  std::vector<entry> entries;

  {
    mapnik::feature_ptr f;
    while (f = fset->next()) {
      mapnik::value param = f->get(m_param_name);

      for (auto const &geom : f->paths()) {
        entries.emplace_back(make_boost_polygon(geom), param);
      }
    }
  }

  // create envelope boxes for entries, as these are needed
  // up-front for the packing algorithm.
  std::vector<value> values;
  values.reserve(entries.size());
  const size_t num_entries = entries.size();
  for (size_t i = 0; i < num_entries; ++i) {
    values.emplace_back(bg::return_envelope<box_2d>(entries[i].first), i);
  }

  // construct index using packing algorithm, which leads to
  // better distribution for querying.
  rtree index(values.begin(), values.end());
  values.clear(); // don't need these any more.

  // loop over features, finding which items from the datasource
  // they intersect with.
  for (mapnik::feature_ptr f : layer) {
    for (auto const &geom : f->paths()) {
      polygon_2d poly = make_boost_polygon(geom);
      param_update_iterator update(f, poly, entries, m_param_name);

      index.query(bgi::intersects(poly), update);

      // quick exit the loop if there's nothing more to do.
      if (!update.m_keep_matching) { break; }
    }
  }
}

mapnik::box2d<double> adminizer::envelope(const std::vector<mapnik::feature_ptr> &layer) const {
  mapnik::box2d<double> result;
  bool first = true;
  for (auto const &feature : layer) {
    if (first) {
      result = feature->envelope();

    } else {
      result.expand_to_include(feature->envelope());
    }
  }
  return result;
}

polygon_2d adminizer::make_boost_polygon(const mapnik::geometry_type &geom) const {
  polygon_2d poly;
  double x = 0, y = 0, prev_x = 0, prev_y = 0;
  unsigned int ring_count = 0;

  geom.rewind(0);

  unsigned int cmd = mapnik::SEG_END;
  while ((cmd = geom.vertex(&x, &y)) != mapnik::SEG_END) {

    if (cmd == mapnik::SEG_MOVETO) {
      prev_x = x;
      prev_y = y;

      if (ring_count == 0) {
        bg::append(poly, bg::make<point_2d>(x, y));
      } else {
        poly.inners().push_back(polygon_2d::inner_container_type::value_type());
        bg::append(poly.inners().back(), bg::make<point_2d>(x, y));
      }
      ++ring_count;

    } else if (cmd == mapnik::SEG_LINETO) {
      if (std::abs(x - prev_x) < 1e-12 && std::abs(y - prev_y) < 1e-12) {
        continue;
      }

      prev_x = x;
      prev_y = y;
      if (ring_count == 1) {
        bg::append(poly, bg::make<point_2d>(x, y));
      } else {
        bg::append(poly.inners().back(), bg::make<point_2d>(x, y));
      }
    }
  }

  return poly;
}

izer_ptr create_adminizer(pt::ptree const& config) {
  return std::make_shared<adminizer>(config);
}

} // namespace post_process
} // namespace avecado
