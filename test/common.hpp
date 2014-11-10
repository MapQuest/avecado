#include <string>
#include <vector>
#include <sstream>
#include <boost/function.hpp>
#include <boost/format.hpp>
#include <boost/noncopyable.hpp>
#include <boost/filesystem.hpp>
#include <stdexcept>

#include <mapnik/map.hpp>
#include <mapnik/feature.hpp>

namespace test{

template <typename T>
void assert_equal(T actual, T expected, std::string message = std::string()) {
   if (actual != expected) {
      throw std::runtime_error((boost::format("%1%: expected=%2%, actual=%3%.")
                                % message % expected % actual).str());
   }
}

template <typename T>
void assert_not_equal(T actual, T expected, std::string message = std::string()) {
   if (actual == expected) {
      throw std::runtime_error((boost::format("%1%: expected=%2%, actual=%3%.")
                                % message % expected % actual).str());
   }
}

template <typename T>
void assert_less_or_equal(T actual, T expected, std::string message = std::string()) {
   if (actual > expected) {
      throw std::runtime_error((boost::format("%1%: expected=%2%, actual=%3%.")
                                % message % expected % actual).str());
   }
}

template <typename T>
void assert_greater_or_equal(T actual, T expected, std::string message = std::string()) {
   if (actual < expected) {
      throw std::runtime_error((boost::format("%1%: expected=%2%, actual=%3%.")
                                % message % expected % actual).str());
   }
}

//NOTE: to_string and equals methods below could be passed to assertion methods above but we'd
//still have duplication of code. would be best if mapnik objects had comparison and ostream operators

std::string to_string(const mapnik::geometry_type& a);

std::string to_string(const mapnik::feature_ptr& a);

std::string to_string(const std::vector<mapnik::feature_ptr>& a);

bool equal_tags(const mapnik::feature_ptr& a, const mapnik::feature_ptr& b);

bool equal(const mapnik::geometry_type& a, const mapnik::geometry_type& b);

bool equal(const mapnik::feature_ptr& a, const mapnik::feature_ptr& b, const bool match_tags = false);

bool equal(const std::vector<mapnik::feature_ptr>& a, const std::vector<mapnik::feature_ptr>& b, const bool match_tags = false);


/* runs the test function, formats the output nicely and returns 1
 * if the test failed.
 */
int run(const std::string &name, boost::function<void ()> test);

/* a DSL to make JSON format files. this is nicer than simply
 * quoting the JSON file because C++ lacks heredoc support and
 * uses the same quote character as JSON, so the quoted strings
 * end up looking really ugly.
 */
struct json {
   enum type { type_NONE, type_DICT, type_LIST };

   json();
   json(const json &j);

   /* use operator() to add dictionary key-value entries.
    */
   template <typename T>
   json &operator()(const std::string &key, const T &t) {
      bool first = false;
      if (m_type == type_NONE) { first = true; m_type = type_DICT; }
      if (m_type != type_DICT) { throw std::runtime_error("Mixed type in JSON: expecting DICT."); }
      if (first) { m_buf << "{"; } else { m_buf << ","; }
      m_buf << "\"" << key << "\":";
      quote(t);
      return *this;
   }

   /* use operator[] to add list entries.
    */
   template <typename T>
   json &operator[](const T &t) {
      bool first = false;
      if (m_type == type_NONE) { first = true; m_type = type_LIST; }
      if (m_type != type_LIST) { throw std::runtime_error("Mixed type in JSON: expecting LIST."); }
      if (first) { m_buf << "["; } else { m_buf << ","; }
      quote(t);
      return *this;
   }

   friend std::ostream &operator<<(std::ostream &, const json &);

private:

   void quote(const json &);
   void quote(const std::string &);
   void quote(int);
   void quote(double);

   type m_type;
   std::ostringstream m_buf;
};

std::ostream &operator<<(std::ostream &, const json &);

/* an RAII temporary directory.
 *
 * on construction, creates a temporary directory. the path to it
 * is available via the path() accessor. upon destruction, it will
 * recursively delete the whole temporary directory tree.
 */
struct temp_dir : boost::noncopyable {
   temp_dir();
   ~temp_dir();
   inline boost::filesystem::path path() const { return m_path; }
private:
   boost::filesystem::path m_path;
};

mapnik::box2d<double> box_for_tile(int z, int x, int y);

// often tests will need map objects to call izer::process
mapnik::Map make_map(std::string style_file, unsigned tile_resolution, int z, int x, int y);

} // namespace test
