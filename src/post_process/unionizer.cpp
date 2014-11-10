#include "post_process/unionizer.hpp"

#include <string>
#include <vector>
#include <list>
#include <set>
#include <map>
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
  enum union_heuristic { GREEDY, OBTUSE, ACUTE, /*LONGEST, SHORTEST, TAG*/ };
  const unordered_map<string, union_heuristic> string_to_heuristic = { {"greedy", GREEDY}, {"obtuse", OBTUSE}, {"acute", ACUTE}, /*{"longest", LONGEST}, {"shortest", SHORTEST}, {"tag", TAG}*/ };

  //we allow the user to specify a strategy for what they want to do with the
  //remaining unreferenced (not in the match_tags or preserve_direction_tags) after
  //the unioning of two geometries. the most straightforward variant is to not
  //include them (DROP). we also support keeping all unreferenced tags that either
  //are equivalent in all features or only present in one of the features (PRESERVE).
  enum tag_strategy { DROP/*, PRESERVE*/ };
  const unordered_map<string, tag_strategy> string_to_strategy = { {"drop", DROP}/*, {"preserve", PRESERVE}*/ };

  //returns true if the given feature has all of the tags
  bool unionable(const mapnik::feature_ptr& feature, const set<string>& tags) {
    if(!feature)
      return false;

    for(const auto& key : tags) {
      if(!feature->has_key(key))
        return false;
    }
    return true;
  }

  //used to approximate a curve with a single directional vector
  struct curve_approximator {
      //pass it the start point of the curve
      curve_approximator(double x, double y, double consume_x, double consume_y):
        m_x(x), m_y(y), m_consume_x(consume_x), m_consume_y(consume_y), m_total_length(0){}

      //pass in the next points on the line which it will consume until it
      //has consumed the limits specified in x/ydist. it will return false if it
      //doesn't want anymore points
      bool consume(double x, double y) {
        //consume this bit
        double x_diff = abs(m_x - x);
        double y_diff = abs(m_y - y);

        //if we've consumed too much x (what could possibly go wrong?)
        if(m_consume_x - x_diff < 0){
          //find the appropriate y_diff (intercept) that makes x_diff == m_consume_x
          y_diff = (y_diff / x_diff) * m_consume_x;
          x_diff = m_consume_x;
        }

        //if we've consumed too much y (surely there is some secrete club drug named 'y')
        if(m_consume_y - y_diff < 0) {
          //find the appropriate x_diff (intercept) that makes y_diff == m_consume_y
          x_diff = (x_diff / y_diff) * m_consume_y;
          y_diff = m_consume_y;
        }

        //update the amount you've consumed
        m_consume_x -= x_diff;
        m_consume_y -= y_diff;

        //keep stats on how far away this point is
        m_points.emplace_back(x_diff, y_diff, x_diff*x_diff + y_diff*y_diff);
        m_total_length += get<2>(m_points.back());

        //do we have length left to consume
        return m_consume_x > 0 && m_consume_y > 0;
      }

      //returns the vector from the origin that follows the general
      //direction of the portion of the curve that was sampled
      void get_approximation(double& x, double& y) {
        //this seems like a reasonable approximation. basically we
        //take all the vectors from the union point to each point along the curve
        //and average them together, but we weight them by their relative distance
        //from the start point. perhaps this strategy has a name but what it is
        //is anyone's guess

        //a place to hold the approximate direction of the curve
        x = y = 0;

        //normalize the length to use as a weight to apply when averaging the vectors
        double scale = 1 / m_total_length;
        for(const auto& weighted_point : m_points) {
          x += get<0>(weighted_point) * get<2>(weighted_point) * scale;
          y += get<1>(weighted_point) * get<2>(weighted_point) * scale;
        }
      }

      const double m_x, m_y;
      double m_consume_x, m_consume_y;
      double m_total_length;
      list<tuple<double, double, double> > m_points;
  };

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
    //normal vector approximating the curve leaving the vertex
    double m_dx, m_dy;

    candidate(position position_, size_t index_, mapnik::feature_ptr feature_, bool directional, union_heuristic heuristic_,
      const pair<double, double>& xy_distance):
      m_position(position_), m_index(index_), m_parent(feature_), m_directional(directional), m_dx(NAN), m_dy(NAN) {
      //grab the geom
      mapnik::geometry_type& geometry = m_parent->get_geometry(m_index);
      //grab the vertex
      geometry.vertex(m_position == FRONT ? 0 : geometry.size() - 1, &m_x, &m_y);

      //tweak the candidate according to the heuristic
      switch(heuristic_) {
        case GREEDY:
          break;
        case OBTUSE:
        case ACUTE:
          //a place to hold each point in the curve
          double x = m_x, y = m_y;
          //object to use to approximate the curve
          curve_approximator appx(m_x, m_y, xy_distance.first, xy_distance.second);
          //pull out the geometry until we've consumed enough
          for(size_t i = 1; i < geometry.size(); ++i) {
            //grab this point in the geom
            if(m_position == FRONT)
              geometry.vertex(i, &x, &y);
            else
              geometry.vertex((geometry.size() - i) - 1, &x, &y);
            //if its done consuming then stop
            if(!appx.consume(x, y))
              break;
          }
          //set the approximate angle of the curve leaving the end point
          appx.get_approximation(m_dx, m_dy);
          break;
      }
    }
  };

  class candidate_comparator {
  public:
    explicit candidate_comparator(const set<string>& tags):m_tags(tags){}
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
    const set<string> m_tags;
  };

  void add_candidates(mapnik::feature_ptr feature, set<candidate, candidate_comparator>& candidates,
    const union_heuristic heuristic, const bool preserve_direction, const pair<double, double>& distance) {
    //grab some statistics about the geom so we can play match maker
    for (size_t i = 0; i < feature->num_geometries(); ++i) {
      //grab the geom
      mapnik::geometry_type& geometry = feature->get_geometry(i);
      //we only handle (nontrivial) linestring unioning at present
      if (geometry.type() == mapnik::geometry_type::LineString && geometry.size() > 1) {
        //make placeholders for the front and back
        candidate front(candidate::FRONT, i, feature, preserve_direction, heuristic, distance);
        candidate back(candidate::BACK, i, feature, preserve_direction, heuristic, distance);
        //add on the candidates
        candidates.emplace(front);
        candidates.emplace(back);
      }
    }
  }

  set<candidate, candidate_comparator> get_candidates(std::vector<mapnik::feature_ptr> &layer,
    const set<string>& tags, const set<string>& directional_tags,
    const union_heuristic heuristic, const pair<double, double>& distance) {

    set<candidate, candidate_comparator> candidates{candidate_comparator(tags)};

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
      add_candidates(feature, candidates, heuristic, preserve_direction, distance);
    }

    return candidates;
  }

  //scores go from 0 to MAX_SCORE
  typedef unsigned char score_t;
  #define MAX_SCORE std::numeric_limits<score_t>::max()
  typedef pair<candidate, candidate> couple_t;

  boost::optional<couple_t> make_couple(const candidate& a, const candidate& b) {
    //if they are the same exact geometry (a ring) we dont want to try to connect it
    //note that we allow the same feature to connect geometries within itself
    if(a.m_index == b.m_index && a.m_parent == b.m_parent)
      return boost::none;
    //they either both care about the direction or they dont
    if(a.m_directional != b.m_directional)
      return boost::none;
    //if they need to maintain direction but they don't
    if(a.m_directional && (a.m_position == b.m_position))
      return boost::none;
    return boost::optional<couple_t>(make_pair(a,b));
  }

  //favor them by ease of union operation
  score_t greedy_score(const couple_t& couple) {
    //front to back is easiest
    if(couple.first.m_position != couple.second.m_position)
      return 0;
    //next easiest is back to back
    if(couple.first.m_position == candidate::BACK)
      return MAX_SCORE / 2;
    //hardest is front to front
    return MAX_SCORE;
  }

  //favor them by smallest cosine similarity
  score_t obtuse_score(const couple_t& couple) {
    //valid interval from -1 to 1 where -1 is opposite directions, 0 is right angle and 1 is same direction
    double dot = couple.first.m_dx*couple.second.m_dx + couple.first.m_dy*couple.second.m_dy;
    //move the dot into the range of 0 - 2, cut it in half to make it a percentage to scale the max score by
    return MAX_SCORE * ((dot + 1) * .5);
  }

  //favor the largest cosine similarity
  score_t acute_score(const couple_t& couple) {
    return MAX_SCORE - obtuse_score(couple);
  }

  map<score_t, couple_t> score_candidates(const set<candidate, candidate_comparator>& candidates, score_t (*scorer)(const couple_t&)){

    //a place to hold all of the scored pairs
    map<score_t, couple_t> pairs;

    //check all consecutive candidate pairs, technically n^2 but practically never that
    auto cmp = candidates.key_comp();
    for(set<candidate, candidate_comparator>::const_iterator candidate = candidates.begin(); candidate != candidates.end(); ++candidate){
      //for all the adjacent candidates (same point and tags)
      //reuse the comparators less than, if one that came after this
      //one isn't less than (meaning equal in this case)
      //then we are done making pairs for the current candidate
      auto next_candidate = next(candidate);
      while(next_candidate != candidates.end() && cmp(*next_candidate, *candidate)){
        //see if they are compatible
        boost::optional<couple_t> couple = make_couple(*candidate, *next_candidate);
        if(couple)
          pairs.emplace(scorer(*couple), *couple);
      }
    }

    //return all the possible unions
    return pairs;
  }

  void do_union(couple_t& couple) {
    //if we are unioning back to front
    if(couple.first.m_position != couple.second.m_position) {
      //make it so its always adding second to first
      if(couple.second.m_position == candidate::BACK) {
        swap(couple.first, couple.second);
      }
      //add the vertices
      double x, y;
      mapnik::geometry_type& dst = couple.first.m_parent->get_geometry(couple.first.m_index);
      mapnik::geometry_type& src = couple.second.m_parent->get_geometry(couple.second.m_index);
      for(size_t i = 0; i < src.size(); ++i) {
        if(src.vertex(i, &x, &y) != mapnik::SEG_END)
          dst.line_to(x, y);
      }
      //remove the src geom
      mapnik::geometry_container::iterator unioned = couple.second.m_parent->paths().begin() + couple.second.m_index;
      couple.second.m_parent->paths().erase(unioned);

      //TODO: if one of them was directional we need to keep that tag
      //TODO: worry about dropping or unioning tags
      //TODO: worry about keeping ids

    }//we have to do front to front or back to back
    else {
      //in this case we can just append vertices in reverse order
      if(couple.first.m_position == candidate::BACK) {
        //add the vertices
        double x, y;
        mapnik::geometry_type& dst = couple.first.m_parent->get_geometry(couple.first.m_index);
        mapnik::geometry_type& src = couple.second.m_parent->get_geometry(couple.second.m_index);
        for(size_t i = 0; i < src.size(); ++i) {
          if(src.vertex((src.size() - i) - 1, &x, &y) != mapnik::SEG_END)
            dst.line_to(x, y);
        }
        //remove the src geom
        mapnik::geometry_container::iterator unioned = couple.second.m_parent->paths().begin() + couple.second.m_index;
        couple.second.m_parent->paths().erase(unioned);
      }//in this case we have to make new geom because there is no front insertion available
      else {
        //add the vertices of the first segment in reverse
        double x, y;
        auto_ptr<mapnik::geometry_type> dst(new mapnik::geometry_type());
        mapnik::geometry_type& src1 = couple.first.m_parent->get_geometry(couple.first.m_index);
        for(size_t i = 0; i < src1.size(); ++i) {
          if(src1.vertex((src1.size() - i) - 1, &x, &y) != mapnik::SEG_END){
            //first point must start with move to or will mess up rendering
            if(i == 0)
              dst->move_to(x, y);
            else
              dst->line_to(x, y);
          }
        }
        //add the vertices of the second segment in normal order
        mapnik::geometry_type& src2 = couple.second.m_parent->get_geometry(couple.second.m_index);
        for(size_t i = 0; i < src2.size(); ++i) {
          if(src2.vertex(i, &x, &y) != mapnik::SEG_END)
            dst->line_to(x, y);
        }
        //remove the src geoms
        mapnik::geometry_container::iterator unioned = couple.first.m_parent->paths().begin() + couple.first.m_index;
        couple.first.m_parent->paths().erase(unioned);
        unioned = couple.second.m_parent->paths().begin() + couple.second.m_index;
        couple.second.m_parent->paths().erase(unioned);
        //add the new geom back on
        couple.first.m_parent->paths().push_back(dst);
      }

      //TODO: worry about dropping or unioning tags
      //TODO: worry about keeping ids
    }
  }

  size_t union_candidates(map<score_t, couple_t>& scored, const tag_strategy strategy, const boost::optional<string>& ids_tag){

    //a place to hold all the unions we make so we don't
    //try to use the same one twice in one iteration
    unordered_set<mapnik::value_integer> unioned;
    for(auto& entry : scored)
    {
      //if we've already used either of these features in a union
      //we can't use them again in this iteration mainly because
      //the bookkeeping to make sure it would work is quite alot
      couple_t& couple = entry.second;
      if(unioned.find(couple.first.m_parent->id()) != unioned.end() ||
        unioned.find(couple.second.m_parent->id()) != unioned.end()) {
        continue;
      }//speak now or forever hold your peace
      else {
        //attempt the union
        do_union(couple);
        //mark them so as not to hitch them with anyone else in this round
        //don't worry we'll get polygamous in the next round
        unioned.emplace(couple.first.m_parent->id());
        unioned.emplace(couple.second.m_parent->id());
      }
    }

    //let the caller know how much work we've done
    return unioned.size();
  }

  void cull(std::vector<mapnik::feature_ptr> &layer) {
    auto empty = [] (const mapnik::feature_ptr& feature) { return feature->num_geometries() == 0; };
    std::remove_if(layer.begin(), layer.end(), empty);
  }
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
    const size_t max_iterations, const set<string>& match_tags, const set<string>& preserve_direction_tags,
    const double angle_union_sample_ratio);
  virtual ~unionizer() {}

  virtual void process(std::vector<mapnik::feature_ptr> &layer, mapnik::Map const& map) const;

private:

  const union_heuristic m_heuristic;
  const tag_strategy m_strategy;
  const boost::optional<string> m_keep_ids_tag;
  const size_t m_max_iterations;
  const set<string> m_match_tags;
  const set<string> m_preserve_direction_tags;
  const double m_angle_union_sample_ratio;
};

unionizer::unionizer(const union_heuristic heuristic, const tag_strategy strategy, const boost::optional<string>& keep_ids_tag,
  const size_t max_iterations, const set<string>& match_tags, const set<string>& preserve_direction_tags,
  const double angle_union_sample_ratio):
  m_heuristic(heuristic), m_strategy(strategy), m_keep_ids_tag(keep_ids_tag), m_max_iterations(max_iterations),
  m_match_tags(match_tags), m_preserve_direction_tags(preserve_direction_tags), m_angle_union_sample_ratio(angle_union_sample_ratio) {
}

void unionizer::process(std::vector<mapnik::feature_ptr> &layer, mapnik::Map const& map) const {
  //if they are using an angle union heuristic they need to know the distance along the feature
  //to use for estimating an angle that represents the curve leaving the union point
  //so we let them say how many units in each axis we should travel before we have enough data
  //to make an approximation. this is rife with assumptions (non constant units per pixel as
  //you vary the x or y coordinates) but hopefully works well enough for commonly used projections
  double width_units = map.get_current_extent().width() * m_angle_union_sample_ratio;
  double height_units = map.get_current_extent().height() * m_angle_union_sample_ratio;

  //only do up to as many iterations as the user specified
  for(size_t i = 0; i < m_max_iterations; ++i){

    //grab all the current adjacent (sorted by endpoint and tags) tuples of candidates for unioning
    set<candidate, candidate_comparator> candidates =
        get_candidates(layer, m_match_tags, m_preserve_direction_tags, m_heuristic, make_pair(width_units, height_units));

    //a place to hold the scored pairs of candidates
    std::map<score_t, couple_t> scored;

    //templated by the different scoring heuristic of which are the best unions
    switch(m_heuristic) {
      case GREEDY:
        //score all the pairs of candidates
        scored = score_candidates(candidates, greedy_score);
        break;
      case OBTUSE:
        //score all the pairs of candidates
        scored = score_candidates(candidates, obtuse_score);
        break;
      case ACUTE:
        //score all the pairs of candidates
        scored = score_candidates(candidates, acute_score);
        break;
    }

    //do the actual unioning, if the count of unions is 0 then we are done
    if(!union_candidates(scored, m_strategy, m_keep_ids_tag))
      return cull(layer);
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
  else
    throw runtime_error(requested_heuristic + " is not supported, try `greedy, obtuse or acute'");

  //figure out what type of union heuristic to use
  string requested_strategy = config.get<string>("tag_strategy", "drop");
  unordered_map<string, tag_strategy>::const_iterator maybe_strategy = string_to_strategy.find(requested_strategy);
  tag_strategy strategy = DROP;
  if(maybe_strategy != string_to_strategy.end())
    strategy = maybe_strategy->second;
  else
    throw runtime_error(requested_strategy + " is not supported, try `drop'");

  //TODO: add a snap_tolerance option to allow unioning of linestring
  //end points within a specified tolerance from each other
  //NOTE: instead of doing this we could look at the tile info/resolution
  //and use a bitmap to see where features could be unioned, this would
  //implicitly set the tolerance via the resolution so there would be
  //no way to set it to only do unions on exact matches

  //figure out if they want to keep the original ids or not
  boost::optional<string> keep_ids_tag = config.get_optional<string>("keep_ids_tag");

  //figure out if they want to limit the number of unioning iterations that can happen
  size_t max_iterations = config.get<size_t>("max_iterations", numeric_limits<size_t>::max());

  //some tags that must match before unioning
  boost::optional<const pt::ptree&> match_tags = config.get_child_optional("match_tags");
  set<string> match;
  if(match_tags) {
    for(const pt::ptree::value_type &v: *match_tags) {
      match.insert(v.first);
    }
  }

  //some tags that, if they occur, must match and make the geometry maintain its original direction
  //this is useful for oneway roads or streams where you want to enforce that the direction of the geometry
  //remains consistent after the union (ie can only union start to end points and vice versa)
  boost::optional<const pt::ptree&> preserve_direction_tags = config.get_child_optional("preserve_direction_tags");
  set<string> direction;
  if(preserve_direction_tags) {
    for(const pt::ptree::value_type &v: *preserve_direction_tags) {
      direction.insert(v.first);
    }
  }

  //if you are using the angle based heuristic for unioning we need to have some measure of length of a feature to use
  //when determining its approximate angle leaving a union point. we allow the user to specify this as a percentage of
  //the resolution of the tiles they are targeting because we have a measure of how many units are encompassed in a given
  //pixel of a given tile. note that we could allow users to specify the number of pixels but this would require them to
  //know the target resolution of their tiles. also note that we default to 10%
  double angle_union_sample_ratio = config.get<double>("angle_union_sample_ratio", .1);
  //we only allow sane values here
  if(angle_union_sample_ratio <= 0 || angle_union_sample_ratio > .5)
    throw runtime_error("Please make sure 0 < angle_union_sample_ratio <= .5");

  return std::make_shared<unionizer>(heuristic, strategy, keep_ids_tag, max_iterations, match, direction, angle_union_sample_ratio);
}

} // namespace post_process
} // namespace avecado

