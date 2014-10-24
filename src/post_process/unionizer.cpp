#include "post_process/unionizer.hpp"

#include <string>
#include <vector>
#include <set>
#include <unordered_set>
#include <unordered_map>

/*TODO:
 *  rewrite the way we did this. basically the original idea was that
 *  each end point would be a candidate for merging with another end point
 *  however it is pretty much impossible to actually sort them in such a way
 *  that all optimal pairs end up next to each other in the sorted list
 *  so the alternative is to instead generate all pairs (excepting the impossible ones
 *  of course) and score them based on the heuristics, and then sort the set of
 *  pairs based on the score..
 */




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

  //returns true if the given feature has all of the tags
  bool unionable(const mapnik::feature_ptr& feature, const unordered_set<string>& tags) {
    if(!feature)
      return false;

    for(const auto& key : tags) {
      if(!feature->has_key(key))
        return false;
    }
    return true;
  }

  //a struct we can use to sort the end points of linestrings to be used in the
  //match making process. why is this starting to sound like marriage?
  struct candidate{
    //which end of the line is it from
    enum position {FRONT, BACK};
    position m_position;
    //the original geometry objects index within the feature
    size_t m_index;
    //the feature which this geometry belonged to
    mapnik::feature_ptr m_parent;
    //whether or not this feature must maintain its direction
    bool m_directional;
    //the vertex
    double m_x, m_y;
    //TODO: add some vector approximation of the linestring heading away from the vertex
    //so that when sorting you can favor acute vs obtuse angles
    //the difficulty here is how much of the linestring to consider when computing this
    //directional vector. if you take to little it doesn't really represent the angle
    //if you take too much it doesn't really represent the angle. initial thought is
    //given two linestrings you could take up to a

    candidate(position position_, size_t index_, mapnik::feature_ptr feature_, bool directional, double x_, double y_):
      m_position(position_), m_index(index_), m_parent(feature_), m_directional(directional), m_x(x_), m_y(y_) {
    }

    mapnik::feature_ptr do_union(const candidate& c) const {
      return mapnik::feature_ptr();
    }

    //returns true if this candidate can be unioned with the provided candidate
    mapnik::feature_ptr try_union(const candidate& c) const {
      //just in case don't try to merge with oneself
      //should we consider it possible for multies within the same feature though?
      if(m_parent == m_parent)
        return mapnik::feature_ptr();

      //so this if block is a bit insane so instead of anding and oring it together
      //we break it up into separate conditions so as to hopefully make it more readable

      //end points have to match
      if(m_x == c.m_x && m_y == c.m_y){
        //if we must preserve the direction of the geometry
        if(m_directional || c.m_directional){
          //then it has to connect back to front or front to back
          if(m_position == c.m_position)
            return mapnik::feature_ptr();
          //union them
          return do_union(c);
        }//we dont care about the direction of the geometry
        else{
          //union them
          return do_union(c);
        }
      }
      return mapnik::feature_ptr();
    }
  };

  class greedy_comparator {
  public:
    explicit greedy_comparator(const unordered_set<string>& tags):m_tags(tags){}
    bool operator()(const candidate& a, const candidate& b) const {
      //check the endpoint
      if(a.m_x < b.m_x)
        return true;
      if(a.m_x > b.m_x)
        return false;
      if(a.m_y < b.m_y)
        return true;
      if(a.m_y > b.m_y)
        return false;

      //TODO: we haven't checked the direction or positions
      //is there a way to do that for the sort here?

      //check the tags
      for(const auto& tag : m_tags) {
        const mapnik::value& a_val = a.m_parent->get(tag);
        const mapnik::value& b_val = b.m_parent->get(tag);
        if(a_val < b_val)
          return true;
        if(a_val > b_val)
          return false;
      }

      //must be equivalent both in points and tags
      return false;
    }
    const unordered_set<string> m_tags;
  };

  template <class comparator>
  void add_candidates(mapnik::feature_ptr feature, set<candidate, comparator>& candidates,
    const union_heuristic heuristic, const bool preserve_direction) {
    //grab some statistics about the geom so we can play match maker
    for (size_t i = 0; i < feature->num_geometries(); ++i) {
      //grab the geom
      mapnik::geometry_type& geometry = feature->get_geometry(i);
      //we only handle line unioning at present
      if (geometry.type() == mapnik::geometry_type::LineString) {
        //make placeholders for the front and back
        candidate front(candidate::FRONT, i, feature, preserve_direction, NAN, NAN);
        candidate back(candidate::BACK, i, feature, preserve_direction, NAN, NAN);
        //tweak the candidates according to the union heuristic
        switch(heuristic) {
          case ACUTE:
            //not yet implemented
          case OBTUSE:
            //not yet implemented
          case GREEDY:
            geometry.vertex(0, &front.m_x, &front.m_y);
            geometry.vertex(geometry.size(), &back.m_x, &back.m_y);
            break;
        }
        //add on the candidates
        candidates.emplace(front);
        candidates.emplace(back);
      }
    }
  }

  template <class comparator>
  set<candidate, comparator> get_candidates(std::vector<mapnik::feature_ptr> &layer,
    const unordered_set<string>& tags, const unordered_set<string>& directional_tags, const union_heuristic heuristic) {

    set<candidate, comparator> candidates{comparator(tags)};

    //for each feature set
    for (mapnik::feature_ptr feature : layer) {
      //do we care to union this feature
      if (!unionable(feature, tags))
        continue;

      //does it have tags that require it to maintain directionality
      bool preserve_direction = false;
      for (const auto& tag : directional_tags) {
        if((preserve_direction = feature->has_key(tag)))
          break;
      }

      //create some union candidates out of the geom
      add_candidates<comparator>(feature, candidates, heuristic, preserve_direction);
    }

    return candidates;
  }

  template <class comparator>
  size_t union_candidates(set<candidate, comparator>& candidates, const tag_strategy strategy, const boost::optional<string>& ids_tag){
    //a place to hold all the unions we make so we don't
    //try to use the same one twice in one iteration
    unordered_set<mapnik::value_integer> unioned;

    //check all consecutive candidate pairs
    auto candidate = candidates.begin();
    auto next_candidate = next(candidate);
    while(candidate != candidates.end() && next_candidate != candidates.end()) {

      //we've already used this feature in a union
      //note that it may be the result of a union that we
      //could actually union however it could also be an
      //outdated candidate that should be removed due to a union
      //and telling the difference between the two requires quite
      //a lot of bookkeeping (bidirectional lookup)
      if(unioned.find(candidate->m_parent->id()) != unioned.end()) {
        candidate = next(candidate);
        next_candidate = next(candidate);
        continue;
      }//speak now or forever hold your peace
      else {
        //attempt the union
        mapnik::feature_ptr feature = candidate->try_union(*next_candidate);
        //if they are still hitched mark them so as not to hitch them with anyone
        //else in this round. dont worry we'll get polygomous in the next round
        if(feature) {
          unioned.emplace(candidate->m_parent->id());
          unioned.emplace(next(candidate)->m_parent->id());
        }
      }
    }

    //let the caller know how much work we've done
    return unioned.size();
  }

  bool is_empty(const mapnik::feature_ptr& feature) {
    return feature.operator bool() == false;
  }
  void cull(std::vector<mapnik::feature_ptr> &layer) {
    std::remove_if(layer.begin(), layer.end(), is_empty);
  }

  //explicit instantiation
  /*template
  void add_candidates<greedy_comparator>(mapnik::feature_ptr feature, set<candidate, greedy_comparator>& candidates,
    const union_heuristic heuristic, const bool preserve_direction);*/
  /*template
  set<candidate, greedy_comparator> get_candidates<greedy_comparator>(std::vector<mapnik::feature_ptr> &layer,
    const unordered_set<string>& tags, const unordered_set<string>& directional_tags, const union_heuristic heuristic);*/
}

namespace avecado {
namespace post_process {

/**
 * Post-process that merges features which have matching attribution
 * and geometries that are able to be joined or unioned together.
 */
class unionizer : public izer {
public:
  unionizer(const union_heuristic heuristic, const tag_strategy strategy, const boost::optional<string>& keep_ids_tag,
    const size_t max_iterations, const unordered_set<string>& match_tags, const unordered_set<string>& preserve_direction_tags);
  virtual ~unionizer() {}

  virtual void process(std::vector<mapnik::feature_ptr> &layer) const;

private:

  const union_heuristic m_heuristic;
  const tag_strategy m_strategy;
  const boost::optional<string> m_keep_ids_tag;
  const size_t m_max_iterations;
  const unordered_set<string> m_match_tags;
  const unordered_set<string> m_preserve_direction_tags;
};

unionizer::unionizer(const union_heuristic heuristic, const tag_strategy strategy, const boost::optional<string>& keep_ids_tag,
  const size_t max_iterations, const unordered_set<string>& match_tags, const unordered_set<string>& preserve_direction_tags):
  m_heuristic(heuristic), m_strategy(strategy), m_keep_ids_tag(keep_ids_tag), m_max_iterations(max_iterations),
  m_match_tags(match_tags), m_preserve_direction_tags(preserve_direction_tags) {
}

void unionizer::process(std::vector<mapnik::feature_ptr> &layer) const {
  //only do up to as many iterations as the user specified
  for(size_t i = 0; i < m_max_iterations; ++i){
    //templated by the different type of sorting that must be done
    //to make the unioning work easier/faster
    switch(m_heuristic) {
      //TODO: use different comparators
      case ACUTE:
      case OBTUSE:
      case GREEDY:
        //grab all the current adjacent (sorted) tuples of candidates for unioning
        set<candidate, greedy_comparator> candidates =
            get_candidates<greedy_comparator>(layer, m_match_tags, m_preserve_direction_tags, m_heuristic);

        //do the actual union, if none were unioned we quit
        if(!union_candidates<greedy_comparator>(candidates, m_strategy, m_keep_ids_tag))
          return cull(layer);
        break;
    }
  }

  //ran out of iterations
  return cull(layer);
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

  //TODO: add a snap_tolerance option to allow unioning of linestring
  //end points within a specified tolerance from each other
  //NOTE: instead of doing this we could look at the tile info/resolution
  //and use a bitmap to see where features could be union, this would
  //implicitly set the tolerance via the resolution so there would be
  //no way to set it to only do unions on exact matches

  //figure out if they want to keep the original ids or not
  boost::optional<string> keep_ids_tag = config.get_optional<string>("keep_ids_tag");

  //figure out if they want to limit the number of unioning iterations that can happen
  size_t max_iterations = config.get<size_t>("max_iterations", numeric_limits<size_t>::max());

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

  return std::make_shared<unionizer>(heuristic, strategy, keep_ids_tag, max_iterations, match, direction);
}

} // namespace post_process
} // namespace avecado
