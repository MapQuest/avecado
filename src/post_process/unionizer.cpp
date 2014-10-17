#include "post_process/unionizer.hpp"

#include <string>
#include <vector>
#include <unordered_set>
#include <unordered_map>

using namespace std;

namespace {
  //we allow the user to chose between multiple strategies for merging
  //so you can think of a junction where 5 line strings come to the same point
  //you have a potential to union in 10 different ways (from the perspective of
  //a single particular linestring). so you can either just take the first one
  //that occured in the data (GREEDY) or you can favor the union which would
  //result in the steepest (ACUTE) or shallowest (OBTUSE) angle after the union.
  //one could think of another heuristic measuring similarity of tagging between
  //two features but this is not implemented yet
  enum union_heuristic { GREEDY, ACUTE, OBTUSE/*, TAG*/ };
  const unordered_map<string, union_heuristic> string_to_heuristic = { {"greedy", GREEDY}, {"acute", ACUTE}, {"obtuse", OBTUSE}/*, {"tag", TAG}*/ };

  //we allow the user to specify a strategy for what they want to do with the
  //remaining unreferenced (not in the match_tags or preserve_direction_tags) after
  //the unioning of two geometries. the most straightforward variant is to not
  //include them (DROP). we also support keeping all unreferenced tags that either
  //are equivalent in all features or only present in one of the features (PRESERVE).
  enum tag_strategy { DROP, PRESERVE };
  const unordered_map<string, tag_strategy> string_to_strategy = { {"drop", DROP}, {"preserve", PRESERVE} };
}

namespace avecado {
namespace post_process {

/**
 * Post-process that merges features which have matching attribution
 * and geometries that are able to be joined or unioned together.
 */
class unionizer : public izer {
public:
  unionizer(const union_heuristic heuristic, const tag_strategy strategy,
    const unordered_set<string> match_tags, const unordered_set<string> preserve_direction_tags);
  virtual ~unionizer() {}

  virtual void process(std::vector<mapnik::feature_ptr> &layer) const;

private:

  const union_heuristic m_heuristic;
  const tag_strategy m_strategy;
  const unordered_set<string> m_match_tags;
  const unordered_set<string> m_preserve_direction_tags;
};

unionizer::unionizer(const union_heuristic heuristic, const tag_strategy strategy,
  const unordered_set<string> match_tags, const unordered_set<string> preserve_direction_tags):
  m_heuristic(heuristic), m_strategy(strategy), m_match_tags(match_tags), m_preserve_direction_tags(preserve_direction_tags) {
}

void unionizer::process(std::vector<mapnik::feature_ptr> &layer) const {
  // TODO: unionize!
}

izer_ptr create_unionizer(pt::ptree const& config) {

  //figure out what type of union heuristic to use
  string requested_heuristic = config.get<string>("union_heuristic", "greedy");
  unordered_map<string, union_heuristic>::const_iterator maybe_heuristic = string_to_heuristic.find(requested_heuristic);
  union_heuristic heuristic = GREEDY;
  if(maybe_heuristic != string_to_heuristic.end())
    heuristic = maybe_heuristic->second;
  //TODO:
  //else
  //  LOG::WARNING(requested_heurstic + " is not supported, falling back to `greedy'")

  //figure out what type of union heuristic to use
  string requested_strategy = config.get<string>("tag_strategy", "drop");
  unordered_map<string, tag_strategy>::const_iterator maybe_strategy = string_to_strategy.find(requested_strategy);
  tag_strategy strategy = DROP;
  if(maybe_strategy != string_to_strategy.end())
    strategy = maybe_strategy->second;
  //TODO:
  //else
  //  LOG::WARNING(requested_strategy + " is not supported, falling back to `drop'")

  //some tags that must match before unioning
  boost::optional<const pt::ptree&> match_tags = config.get_child_optional("match_tags");
  unordered_set<string> match;
  if(match_tags) {
    for(const pt::ptree::value_type &v: *match_tags) {
      match.insert(v.first);
    }
  }

  //some tags that, if they occur, must match and make the geometry maintain its original direction
  //this is useful for oneway roads or streams where you want to enforce that the direction of the geometry
  //remains consistent after the union (ie can only union start to end points and vice versa)
  boost::optional<const pt::ptree&> preserve_direction_tags = config.get_child_optional("preserve_direction_tags");
  unordered_set<string> direction;
  if(preserve_direction_tags) {
    for(const pt::ptree::value_type &v: *preserve_direction_tags) {
      direction.insert(v.first);
    }
  }

  return std::make_shared<unionizer>(heuristic, strategy, match, direction);
}

} // namespace post_process
} // namespace avecado
