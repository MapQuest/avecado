#include "post_process/generalizer.hpp"

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

  virtual void process(vector<mapnik::feature_ptr> &layer) const;

private:
  //TODO: mapnik simplify_converter shared ptr
};

generalizer::generalizer(const string& algorithm, const double& tolerance) {
  //TODO: make a mapnik simplfy_converter shared ptr passing in the algorithm type
  boost::optional<mapnik::simplify_algorithm_e> algo = mapnik::simplify_algorithm_from_string(algorithm);
}


void generalizer::process(vector<mapnik::feature_ptr> &layer) const {
  // TODO: generalize!
}

izer_ptr create_generalizer(pt::ptree const& config) {
  //NOTE: there is no peucker in mapnik yet..
  string algorithm = config.get<string>("algorithm", "douglasl-peucker");
  double tolerance = config.get<double>("tolerance");
  return std::make_shared<generalizer>(algorithm, tolerance);
}

} // namespace post_process
} // namespace avecado
