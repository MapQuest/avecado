#include "post_process/generalizer.hpp"

#include <vector>
#include <string>
#include <mapnik/simplify_converter.hpp>

using namespace std;

namespace avecado {
namespace post_process {

/**
 * Post-process that runs a selected generalization algorithm
 * on feature geometries.
 */
class generalizer : public izer {
public:
  generalizer(const string& algorithm, const double& tolerance);
  virtual ~generalizer() {}

  virtual void process(vector<mapnik::feature_ptr> &layer, mapnik::Map const& map) const;

private:

  boost::optional<mapnik::simplify_algorithm_e> m_algorithm;
  double m_tolerance;
};

generalizer::generalizer(const string& algorithm, const double& tolerance):
  m_algorithm(mapnik::simplify_algorithm_from_string(algorithm)),
  m_tolerance(tolerance) {

}


void generalizer::process(vector<mapnik::feature_ptr> &layer, mapnik::Map const& map) const {
  //for each feature set
  for(auto& feat : layer) {
    //for each geometry
    for(size_t i = 0; i < feat->num_geometries(); ++i) {
      //grab the geom
      const mapnik::geometry_type &geom = feat->get_geometry(i);
      // adapt it as a path
      mapnik::vertex_adapter path(geom);
      //setup the generalization
      mapnik::simplify_converter<mapnik::vertex_adapter> generalizer(path);
      if(m_algorithm)
        generalizer.set_simplify_algorithm(*m_algorithm);
      generalizer.set_simplify_tolerance(m_tolerance);
      // suck the vertices back out of it. note: keep the geometry in a smart
      // pointer type in case ::push_vertex fails below and we get an exception.
      std::unique_ptr<mapnik::geometry_type> output(new mapnik::geometry_type(geom.type()));
      mapnik::CommandType cmd;
      double x, y;
      while((cmd = (mapnik::CommandType)generalizer.vertex(&x, &y)) != mapnik::SEG_END)
      {
        output->push_vertex(x, y, cmd);
      }
      // and put it back, note that this returns a pointer of unspecified
      // smart type to the old one, which means it will get safely deallocated
      // when it goes out of scope
      feat->paths().replace(i, output.release());
    }
  }
}

izer_ptr create_generalizer(pt::ptree const& config) {
  //NOTE: there is no peucker in mapnik yet..
  string algorithm = config.get<string>("algorithm", "douglas-peucker");
  double tolerance = config.get<double>("tolerance");
  return std::make_shared<generalizer>(algorithm, tolerance);
}

} // namespace post_process
} // namespace avecado
