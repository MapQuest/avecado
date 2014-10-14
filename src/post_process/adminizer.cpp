#include "post_process/adminizer.hpp"

#include <mapnik/params.hpp>
#include <mapnik/datasource.hpp>
#include <mapnik/datasource_cache.hpp>
#include <mapnik/featureset.hpp>

#include <boost/geometry.hpp>
#include <boost/geometry/geometries/point.hpp>
#include <boost/geometry/geometries/box.hpp>
#include <boost/geometry/geometries/polygon.hpp>
#include <boost/geometry/geometries/multi_point.hpp>
#include <boost/geometry/multi/geometries/multi_linestring.hpp>
#include <boost/geometry/index/rtree.hpp>
#include <boost/geometry/io/wkt/write.hpp>

#include <queue> // for std::priority_queue<>

// NOTE: this is included only because it's where mapnik::coord2d is
// adapted to work with the boost::geometry stuff. we don't actually
// clip any polygons.
#include <mapnik/polygon_clipper.hpp>

namespace bg = boost::geometry;
namespace bgi = boost::geometry::index;

using point_2d = bg::model::point<double, 2, bg::cs::cartesian>;
using box_2d = bg::model::box<point_2d>;
using linestring_2d = bg::model::linestring<point_2d>;
using multi_point_2d = bg::model::multi_point<point_2d>;
using multi_linestring_2d = bg::model::multi_linestring<linestring_2d>;
// see PR #22
using polygon_2d = bg::model::polygon<point_2d, false, true>;
using priority_queue = std::priority_queue<unsigned int, std::vector<unsigned int>,
                                           std::greater<unsigned int> >;

namespace {

typedef std::pair<box_2d, unsigned int> value;
struct entry {
  entry(polygon_2d &&p, mapnik::value &&v, unsigned int i)
    : polygon(p), value(v), index(i) {
  }
  polygon_2d polygon;
  mapnik::value value;
  unsigned int index;
};

struct param_updater {
  bool m_collect;
  std::set<unsigned int> m_indices;
  bool m_finished;

  explicit param_updater(bool collect)
    : m_collect(collect)
    , m_indices()
    , m_finished(false) {
  }

  void operator()(const entry &e) {
    m_indices.insert(e.index);
    // early termination only if we're looking for the first admin
    // area and just found it.
    m_finished = (!m_collect) && (e.index == 0);
  }
};

void update_feature_params(const std::set<unsigned int> &indices,
                           bool collect,
                           const std::vector<entry> &entries,
                           mapnik::feature_ptr &&feat,
                           const std::string &param_name,
                           const mapnik::value_unicode_string &delimiter,
                           std::vector<mapnik::feature_ptr> &append_to) {
  append_to.emplace_back(feat);
  mapnik::feature_ptr &feature = append_to.back();

  if (!indices.empty()) {
    if (collect) {
      mapnik::value_unicode_string buffer;
      bool first = true;
      for (unsigned int i : indices) {
        if (first) {
          first = false;
        } else {
          buffer.append(delimiter);
        }
        buffer.append(entries[i].value.to_unicode());
      }
      feature->put_new(param_name, buffer);

    } else {
      const entry &e = entries[*indices.begin()];
      feature->put_new(param_name, e.value);
    }
  }
}

inline void update_feature_params(const param_updater &updater,
                                  const std::vector<entry> &entries,
                                  mapnik::feature_ptr &&feat,
                                  const std::string &param_name,
                                  const mapnik::value_unicode_string &delimiter,
                                  std::vector<mapnik::feature_ptr> &append_to) {
  update_feature_params(updater.m_indices, updater.m_collect,
                        entries, std::move(feat), param_name,
                        delimiter, append_to);
}

template <typename GeomType>
mapnik::geometry_type *to_mapnik_geom(const GeomType &g) {
  throw std::runtime_error("Unimplemented geometry type.");
}

template <>
mapnik::geometry_type *to_mapnik_geom<multi_point_2d>(const multi_point_2d &g) {
  mapnik::geometry_type *mg = new mapnik::geometry_type(mapnik::geometry_type::Point);
  for (auto const &point : g) {
    mg->move_to(point.get<0>(), point.get<1>());
  }
  return mg;
}

template <>
mapnik::geometry_type *to_mapnik_geom<multi_linestring_2d>(const multi_linestring_2d &g) {
  mapnik::geometry_type *mg = new mapnik::geometry_type(mapnik::geometry_type::LineString);
  for (auto const &line : g) {
    const size_t n_points = line.size();
    if (n_points > 0) {
      mg->move_to(line[0].get<0>(), line[0].get<1>());
      for (size_t i = 1; i < n_points; ++i) {
        mg->line_to(line[i].get<0>(), line[i].get<1>());
      }
    }
  }
  return mg;
}

mapnik::box2d<double> envelope(const std::vector<mapnik::feature_ptr> &layer) {
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

multi_point_2d make_boost_point(const mapnik::geometry_type &geom) {
/* Takes a mapnik geometry and makes a multi_point_2d from it. It has to be a
 * multipoint, since we don't know from geom.type() if it's a point or multipoint?
 */
  multi_point_2d points;
  double x = 0, y = 0;

  geom.rewind(0);

  unsigned int cmd = mapnik::SEG_END;

  while ((cmd = geom.vertex(&x, &y)) != mapnik::SEG_END) {
    points.push_back(bg::make<point_2d>(x, y));
  }
  return points;
}

multi_linestring_2d make_boost_linestring(const mapnik::geometry_type &geom) {
  multi_linestring_2d line;
  double x = 0, y = 0, prev_x = 0, prev_y = 0;

  geom.rewind(0);

  unsigned int cmd = mapnik::SEG_END;
  while ((cmd = geom.vertex(&x, &y)) != mapnik::SEG_END) {

    if (cmd == mapnik::SEG_MOVETO) {
      line.push_back(linestring_2d());
      line.back().push_back(bg::make<point_2d>(x, y));

    } else if (cmd == mapnik::SEG_LINETO) {
      if (std::abs(x - prev_x) < 1e-12 && std::abs(y - prev_y) < 1e-12) {
        continue;
      }

      line.back().push_back(bg::make<point_2d>(x, y));
    }

    prev_x = x;
    prev_y = y;
  }

  return line;
}

polygon_2d make_boost_polygon(const mapnik::geometry_type &geom) {
  polygon_2d poly;
  double x = 0, y = 0, prev_x = 0, prev_y = 0;
  unsigned int ring_count = 0;

  geom.rewind(0);

  unsigned int cmd = mapnik::SEG_END;
  while ((cmd = geom.vertex(&x, &y)) != mapnik::SEG_END) {

    if (cmd == mapnik::SEG_MOVETO) {
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

      if (ring_count == 1) {
        bg::append(poly, bg::make<point_2d>(x, y));

      } else {
        bg::append(poly.inners().back(), bg::make<point_2d>(x, y));
      }
    }

    prev_x = x;
    prev_y = y;
  }

  return poly;
}

template <typename GeomType>
inline void split_hack(const GeomType &geom, const polygon_2d &poly,
                       std::list<GeomType> &inside_out,
                       std::list<GeomType> &outside_out) {
  bg::intersection(geom, poly, inside_out);
  bg::difference(geom, poly, outside_out);
}

template <>
inline void split_hack<multi_point_2d>(
  const multi_point_2d &multipoint,
  const polygon_2d &poly,
  std::list<multi_point_2d> &inside_out,
  std::list<multi_point_2d> &outside_out) {

  multi_point_2d inside_multi, outside_multi;
  for (auto const &point : multipoint) {
    if (bg::intersects(point, poly)) {
      inside_multi.push_back(point);
    } else {
      outside_multi.push_back(point);
    }
  }
  inside_out.emplace_back(std::move(inside_multi));
  outside_out.emplace_back(std::move(outside_multi));
}

template <>
inline void split_hack<multi_linestring_2d>(
  const multi_linestring_2d &multilinestring,
  const polygon_2d &poly,
  std::list<multi_linestring_2d> &inside_out,
  std::list<multi_linestring_2d> &outside_out) {

  multi_linestring_2d inside_multi, outside_multi;
  for (auto const &line : multilinestring) {
    bg::intersection(line, poly, inside_multi);
    bg::difference(line, poly, outside_multi);
  }

  inside_out.emplace_back(std::move(inside_multi));
  outside_out.emplace_back(std::move(outside_multi));
}

template <typename GeomType>
inline bool within(const GeomType &geom, const polygon_2d &poly) {
  return bg::within(geom, poly);
}

template <>
inline bool within<multi_point_2d>(const multi_point_2d &geom, const polygon_2d &poly) {
  for (auto const &point : geom) {
    if (!bg::within(point, poly)) {
      return false;
    }
  }
  return true;
}

template <>
inline bool within<multi_linestring_2d>(const multi_linestring_2d &geom, const polygon_2d &poly) {
  for (auto const &line : geom) {
    if (!bg::within(line, poly)) {
      return false;
    }
  }
  return true;
}

template <typename GeomType>
inline bool disjoint(const GeomType &geom, const polygon_2d &poly) {
  return bg::disjoint(geom, poly);
}

template <>
inline bool disjoint<multi_point_2d>(const multi_point_2d &geom, const polygon_2d &poly) {
  for (auto const &point : geom) {
    if (!bg::disjoint(point, poly)) {
      return false;
    }
  }
  return true;
}

template <>
inline bool disjoint<multi_linestring_2d>(const multi_linestring_2d &geom, const polygon_2d &poly) {
  for (auto const &line : geom) {
    if (!bg::disjoint(line, poly)) {
      return false;
    }
  }
  return true;
}

template <typename GeomType>
void split_impl(const GeomType &geom, const polygon_2d &poly,
                mapnik::feature_ptr &inside, mapnik::feature_ptr &outside) {
  if (within(geom, poly)) {
    inside->add_geometry(to_mapnik_geom(geom));

  } else if (disjoint(geom, poly)) {
    outside->add_geometry(to_mapnik_geom(geom));

  } else {
    // must be some part of the geometry which is inside and some
    // which is outside, so we'll need to split it.
    std::list<GeomType> inside_out, outside_out;

    split_hack<GeomType>(geom, poly, inside_out, outside_out);

    for (const GeomType &g : inside_out) {
      inside->add_geometry(to_mapnik_geom(g));
    }
    for (const GeomType &g : outside_out) {
      outside->add_geometry(to_mapnik_geom(g));
    }
  }
}

void split(const mapnik::geometry_type &geom, const polygon_2d &poly,
           mapnik::feature_ptr &inside, mapnik::feature_ptr &outside) {
  if (geom.type() == mapnik::geometry_type::types::Point) {
    multi_point_2d multi_point = make_boost_point(geom);
    split_impl(multi_point, poly, inside, outside);

  } else if (geom.type() == mapnik::geometry_type::types::LineString) {
    multi_linestring_2d multi_line = make_boost_linestring(geom);
    split_impl(multi_line, poly, inside, outside);

  } else if (geom.type() == mapnik::geometry_type::types::Polygon) {
    polygon_2d poly2 = make_boost_polygon(geom);
    split_impl(poly2, poly, inside, outside);
  }
}

void split_and_update(const std::set<unsigned int> &indices,
                      priority_queue remaining_indices,
                      bool collect,
                      const std::vector<entry> &entries,
                      mapnik::feature_ptr &&feat,
                      const std::string &param_name,
                      const mapnik::value_unicode_string &delimiter,
                      std::vector<mapnik::feature_ptr> &append_to) {
  if (remaining_indices.empty()) {
    update_feature_params(indices, collect, entries, std::move(feat),
                          param_name, delimiter, append_to);

  } else {
    unsigned int index = remaining_indices.top();
    remaining_indices.pop();
    const entry &e = entries[index];

    // TODO: copying the mapnik feature like this is very wasteful.
    // could either try instantiating lazily only when either is
    // non-empty, or do the whole thing with geometries and only
    // handle the features later?
    mapnik::feature_ptr inside =
      std::make_shared<mapnik::feature_impl>(
        std::make_shared<mapnik::context_type>(), feat->id());
    mapnik::feature_ptr outside =
      std::make_shared<mapnik::feature_impl>(
        std::make_shared<mapnik::context_type>(), feat->id());
    for (auto const &kv : *(feat->context())) {
      inside->put_new(kv.first, kv.second);
      outside->put_new(kv.first, kv.second);
    }

    for (auto const &geom : feat->paths()) {
      split(geom, e.polygon, inside, outside);
    }

    if (!inside->paths().empty()) {
      std::set<unsigned int> inside_indices(indices);
      inside_indices.insert(index);

      if (collect) {
        // if collecting, then we need to recurse, as later
        // polygons could add further parameter values.
        split_and_update(inside_indices, remaining_indices, collect,
                         entries, std::move(inside), param_name,
                         delimiter, append_to);

      } else {
        // if not collecting, then we have hit the first already
        // since we went through the indices in order, so there
        // is no need to recurse.
        update_feature_params(inside_indices, collect, entries,
                              std::move(inside), param_name,
                              delimiter, append_to);
      }
    }

    if (!outside->paths().empty()) {
      // always recurse on the outside, as we don't yet know the
      // relation between the geometries and the other admin
      // polygons.
      split_and_update(indices, remaining_indices, collect,
                       entries, std::move(outside), param_name,
                       delimiter, append_to);
    }
  }
}

template <typename GeomType>
struct intersects_iterator {
  const GeomType &m_geom;
  const std::vector<entry> &m_entries;
  param_updater &m_updater;

  intersects_iterator(const GeomType &geom,
                      const std::vector<entry> &entries,
                      param_updater &updater)
    : m_geom(geom), m_entries(entries), m_updater(updater) {
  }

  intersects_iterator &operator++() { // prefix
    return *this;
  }

  intersects_iterator &operator*() {
    return *this;
  }

  intersects_iterator &operator=(const value &v) {
    const entry &e = m_entries[v.second];

    // do detailed intersection test, as the index only does bounding
    // box intersection tests.
    if (intersects(e.polygon)) {
      m_updater(e);
    }

    return *this;
  }

  bool intersects(const polygon_2d &) const;
};

template <>
bool intersects_iterator<multi_point_2d>::intersects(const polygon_2d &poly) const {
  // TODO: remove this hack when/if bg::intersects supports
  // intersection on multi type.
  for (auto const &point : m_geom) {
    if (bg::intersects(point, poly)) {
      return true;
    }
  }
  return false;
}

template <>
bool intersects_iterator<multi_linestring_2d>::intersects(const polygon_2d &poly) const {
  // TODO: remove this hack when/if bg::intersects supports
  // intersection on multi type.
  for (auto const &line : m_geom) {
    if (bg::intersects(line, poly)) {
      return true;
    }
  }
  return false;
}

template <>
bool intersects_iterator<polygon_2d>::intersects(const polygon_2d &poly) const {
  return bg::intersects(m_geom, poly);
}

template <typename RTreeType, typename GeomType>
void try_update(RTreeType &index,
                const GeomType &geom,
                const std::vector<entry> &entries,
                param_updater &updater) {

  intersects_iterator<GeomType> itr(geom, entries, updater);
  index.query(bgi::intersects(bg::return_envelope<box_2d>(geom)), itr);
}
  
} // anonymous namespace

namespace avecado {
namespace post_process {

using rtree = bgi::rtree<value, bgi::quadratic<16> >;

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
  std::vector<entry> make_entries(const mapnik::box2d<double> &env) const;
  rtree make_index(const std::vector<entry> &entries) const;
  void adminize_feature(mapnik::feature_ptr &&f,
                        const rtree &index,
                        const std::vector<entry> &entries,
                        std::vector<mapnik::feature_ptr> &append_to) const;

  // the name of the parameter to take from the admin polygon and set
  // on the feature being adminized.
  std::string m_param_name;

  // if true, split geometries at admin polygon boundaries. if false,
  // do not modify the geometries.
  bool m_split;

  // if true, collect all matching admin parameters. if false, use the
  // first admin parameter only.
  bool m_collect;

  // string to use to separate parameter values when m_collect == true.
  mapnik::value_unicode_string m_delimiter;

  // data source to fetch matching admin boundaries from.
  std::shared_ptr<mapnik::datasource> m_datasource;
};

adminizer::adminizer(pt::ptree const& config)
  : m_param_name(config.get<std::string>("param_name"))
  , m_split(false)
  , m_collect(false)
  , m_delimiter(icu::UnicodeString::fromUTF8(",")) {
  mapnik::parameters params;

  boost::optional<pt::ptree const &> datasource_config =
    config.get_child_optional("datasource");

  if (datasource_config) {
    for (auto &kv : *datasource_config) {
      params[kv.first] = kv.second.data();
    }
  }

  m_datasource = mapnik::datasource_cache::instance().create(params);

  boost::optional<std::string> split = config.get_optional<std::string>("split");
  if (split) {
    m_split = *split == "true";
  }

  boost::optional<std::string> collect = config.get_optional<std::string>("collect");
  if (collect) {
    m_collect = *collect == "true";
  }

  boost::optional<std::string> delimiter = config.get_optional<std::string>("delimiter");
  if (delimiter) {
    m_delimiter = mapnik::value_unicode_string(icu::UnicodeString::fromUTF8(*delimiter));
  }
}

adminizer::~adminizer() {
}

std::vector<entry> adminizer::make_entries(const mapnik::box2d<double> &env) const {
  // query the datasource
  // TODO: do we want to pass more things like scale denominator
  // and resolution type?
  mapnik::featureset_ptr fset = m_datasource->features(mapnik::query(env));

  std::vector<entry> entries;
  unsigned int index = 0;

  mapnik::feature_ptr f;
  while (f = fset->next()) {
    mapnik::value param = f->get(m_param_name);

    for (auto const &geom : f->paths()) {
      // ignore all non-polygon types
      if (geom.type() == mapnik::geometry_type::types::Polygon) {
        entries.emplace_back(make_boost_polygon(geom), std::move(param), index++);
      }
    }
  }

  return entries;
}

rtree adminizer::make_index(const std::vector<entry> &entries) const {
  // create envelope boxes for entries, as these are needed
  // up-front for the packing algorithm.
  std::vector<value> values;
  values.reserve(entries.size());
  const size_t num_entries = entries.size();
  for (size_t i = 0; i < num_entries; ++i) {
    values.emplace_back(bg::return_envelope<box_2d>(entries[i].polygon), i);
  }

  // construct index using packing algorithm, which leads to
  // better distribution for querying.
  return rtree(values.begin(), values.end());
}

void adminizer::adminize_feature(mapnik::feature_ptr &&f,
                                 const rtree &index,
                                 const std::vector<entry> &entries,
                                 std::vector<mapnik::feature_ptr> &append_to) const {
  param_updater updater(m_collect);

  for (auto const &geom : f->paths()) {
    if (geom.type() == mapnik::geometry_type::types::Point) {
      multi_point_2d multi_point = make_boost_point(geom);
      try_update(index, multi_point, entries, updater);

    } else if (geom.type() == mapnik::geometry_type::types::LineString) {
      multi_linestring_2d multi_line = make_boost_linestring(geom);
      try_update(index, multi_line, entries, updater);

    } else if (geom.type() == mapnik::geometry_type::types::Polygon) {
      polygon_2d poly = make_boost_polygon(geom);
      try_update(index, poly, entries, updater);
    }

    // quick exit the loop if there's nothing more to do.
    if (updater.m_finished) { break; }
  }

  if (m_split) {
    priority_queue remaining_indices(
      updater.m_indices.begin(), updater.m_indices.end());
    std::set<unsigned int> empty;

    split_and_update(empty, remaining_indices, m_collect, entries,
                     std::move(f), m_param_name, m_delimiter, append_to);

  } else {
    update_feature_params(updater, entries, std::move(f), m_param_name,
                          m_delimiter, append_to);
  }
}

void adminizer::process(std::vector<mapnik::feature_ptr> &layer) const {
  // build extent of all features in layer
  mapnik::box2d<double> env = envelope(layer);

  // construct an index over the bounding boxes of the geometry,
  // first extracting the geometries from mapnik's representation
  // and transforming them too boost::geometry's representation.
  std::vector<entry> entries = make_entries(env);

  rtree index = make_index(entries);

  // loop over features, finding which items from the datasource
  // they intersect with.
  std::vector<mapnik::feature_ptr> new_features;
  for (mapnik::feature_ptr &f : layer) {
    adminize_feature(std::move(f), index, entries, new_features);
  }

  // move new features into the same array that we were passed.
  // this is so that we can add new features (e.g: when split).
  layer.clear();
  layer.swap(new_features);
}

izer_ptr create_adminizer(pt::ptree const& config) {
  return std::make_shared<adminizer>(config);
}

} // namespace post_process
} // namespace avecado
