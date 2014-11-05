#include "fetch/http.hpp"
#include "fetch/http_date_parser.hpp"
#include "vector_tile.pb.h"
#include "config.h"

#include <boost/format.hpp>
#include <boost/algorithm/string/find_format.hpp>
#include <boost/xpressive/xpressive.hpp>
#include <boost/optional/optional_io.hpp>
#include <boost/lexical_cast.hpp>

#include <sstream>
#include <list>
#include <queue>
#include <google/protobuf/io/zero_copy_stream.h>
#include <google/protobuf/io/zero_copy_stream_impl.h>

#include <curl/curl.h>

#ifdef HAVE_SQLITE3
#include <sqlite3.h>
#endif

// maximum number of idle HTTP handles/connections to keep alive in
// the handle pool. TODO: make this configurable.
#define MAX_POOL_SIZE (64)

namespace avecado { namespace fetch {

namespace {

size_t write_callback(char *ptr, size_t size, size_t nmemb, void *userdata) {
  std::stringstream *stream = static_cast<std::stringstream*>(userdata);
  size_t total_bytes = size * nmemb;
  stream->write(ptr, total_bytes);
  return stream->good() ? total_bytes : 0;
}

std::vector<std::string> singleton(const std::string &base_url, const std::string &ext) {
  std::vector<std::string> vec;
  vec.push_back((boost::format("%1%/{z}/{x}/{y}.%2%") % base_url % ext).str());
  return vec;
}

struct formatter {
  int z, x, y;

  formatter(int z_, int x_, int y_) : z(z_), x(x_), y(y_) {}

  template<typename Out>
  Out operator()(boost::xpressive::smatch const &what, Out out) const {
    int val = 0;

    char c = what[1].str()[0];
    if      (c == 'z') { val = z; }
    else if (c == 'x') { val = x; }
    else if (c == 'y') { val = y; }
    else { throw std::runtime_error("match failed"); }

    std::string sub = (boost::format("%1%") % val).str();
    out = std::copy(sub.begin(), sub.end(), out);

    return out;
  }
};

struct request {
  request(std::promise<fetch_response> &&p_, int z_, int x_, int y_, std::string url_)
    : promise(std::move(p_)), z(z_), x(x_), y(y_), stream(new std::stringstream), url(url_) {}

  request(request &&r)
    : promise(std::move(r.promise))
    , z(r.z), x(r.x), y(r.y)
    , stream(std::move(r.stream))
    , url(std::move(r.url))
    , base_date(std::move(r.base_date))
    , expires(std::move(r.expires))
    , last_modified(std::move(r.last_modified))
    , etag(std::move(r.etag))
    , max_age(std::move(r.max_age)) {
  }

  bool expired() const {
    if (expires) {
      std::time_t now = time(nullptr);
      return *expires < now;
    }
    return true;
  }

  std::promise<fetch_response> promise;
  int z, x, y;
  std::unique_ptr<std::stringstream> stream;
  std::string url;
  boost::optional<std::time_t> base_date;
  boost::optional<std::time_t> expires;
  boost::optional<std::time_t> last_modified;
  boost::optional<std::string> etag;
  boost::optional<double> max_age;
};

bool parse_header_value(boost::iterator_range<const char *> &range) {
  // skip space following key
  while (bool(range) && (range.front() == ' ')) {
    range.advance_begin(1);
  }

  // skip a colon
  if (bool(range) && (range.front() == ':')) {
    range.advance_begin(1);

    // skip more space
    while (bool(range) && (range.front() == ' ')) {
      range.advance_begin(1);
    }

    // skip any \r\n at the end
    while (bool(range) && ((range.back() == '\r') || (range.back() == '\n'))) {
      range.advance_end(-1);
    }

    return true;
  }

  return false;
}

boost::optional<std::time_t> parse_date(boost::iterator_range<const char *> range) {
  if (parse_header_value(range)) {
    std::time_t t = 0;
    if (parse_http_date(range, t)) {
      return t;
    }
  }
  return boost::none;
}

size_t header_callback(char *ptr, size_t size, size_t nmemb, void *userdata) {
  typedef boost::iterator_range<const char *> string_range;

  static const char header_date[] = "Date";
  static const char header_etag[] = "ETag";
  static const char header_expires[] = "Expires";
  static const char header_last_modified[] = "Last-Modified";
  static const char header_cache_control[] = "Cache-control";

  request *req = static_cast<request *>(userdata);
  const size_t total_bytes = size * nmemb;

#define STRLEN(x) (sizeof(header_ ## x) / sizeof(header_ ## x[0]) - 1)
#define HEADER_MATCH(x) ((total_bytes > STRLEN(x)) && (strncasecmp(ptr, header_ ## x, STRLEN(x)) == 0))

  if (HEADER_MATCH(date)) {
    string_range range(ptr + STRLEN(date), ptr + total_bytes);
    req->base_date = parse_date(range);
 
  } else if (HEADER_MATCH(etag)) {
    string_range range(ptr + STRLEN(etag), ptr + total_bytes);
    if (parse_header_value(range)) {
      req->etag = std::string(range.begin(), range.end());
    }

  } else if (HEADER_MATCH(expires)) {
    string_range range(ptr + STRLEN(expires), ptr + total_bytes);
    req->expires = parse_date(range);

  } else if (HEADER_MATCH(last_modified)) {
    string_range range(ptr + STRLEN(last_modified), ptr + total_bytes);
    req->last_modified = parse_date(range);

  } else if (HEADER_MATCH(cache_control)) {
    string_range range(ptr + STRLEN(cache_control), ptr + total_bytes);
    if (parse_header_value(range)) {
      if ((range.size() > 7) && (strncasecmp(range.begin(), "max-age", 7) == 0)) {
        range.advance_begin(7);
        while (bool(range) && ((range.front() == ' ') || (range.front() == '='))) {
          range.advance_begin(1);
        }
        if (bool(range)) {
          req->max_age = boost::lexical_cast<int>(std::string(range.begin(), range.end()));
        }
      }
    }
  }

#undef STRLEN
#undef HEADER_MATCH

  return total_bytes;
}

#ifdef HAVE_SQLITE3
namespace sqlite {
struct sqlite_db_deleter {
  void operator()(sqlite3 *ptr) const {
    if (ptr != nullptr) {
      int status = sqlite3_close(ptr);
      if (status != SQLITE_OK) {
        // TODO: use logger
        std::cerr << "Unable to close SQLite3 database\n" << std::flush;
      }
    }
  }
};

struct sqlite_statement_finalizer {
  void operator()(sqlite3_stmt *ptr) const {
    if (ptr != nullptr) {
      int status = sqlite3_finalize(ptr);
      if (status != SQLITE_OK) {
        // TODO: use logger
        std::cerr << "Unable to finalize SQLite3 statement\n" << std::flush;
      }
    }
  }
};

struct statement {
  boost::optional<std::time_t> column_time(int i) {
    if (sqlite3_column_type(ptr.get(), i) == SQLITE_NULL) {
      return boost::none;
    } else {
      sqlite3_int64 t = sqlite3_column_int64(ptr.get(), i);
      return std::time_t(t);
    }
  }

  boost::optional<std::string> column_text(int i) {
    if (sqlite3_column_type(ptr.get(), i) == SQLITE_NULL) {
      return boost::none;
    } else {
      const unsigned char *str = sqlite3_column_text(ptr.get(), i);
      int sz = sqlite3_column_bytes(ptr.get(), i);
      return std::string((const char *)str, sz);
    }
  }

  void column_blob(int i, std::stringstream &stream) {
    const char *bytes = static_cast<const char *>(sqlite3_column_blob(ptr.get(), i));
    int sz = sqlite3_column_bytes(ptr.get(), i);
    stream.write(bytes, sz);
  }

  bool step() {
    int status = sqlite3_step(ptr.get());
    if (status == SQLITE_DONE) { return false; }
    if (status != SQLITE_ROW) {
      throw std::runtime_error((boost::format("Unable to step row in query result: %1%") % sqlite3_errmsg(db_for_errors)).str());
    }
    return true;
  }

  void bind_text(int i, const std::string &str) {
    int sz = str.size();
    char *strp = static_cast<char *>(malloc(sz));
    if (strp == nullptr) { throw std::runtime_error("Unable to allocate memory for string copy."); }
    memcpy(strp, str.c_str(), sz);
    int status = sqlite3_bind_text(ptr.get(), i, strp, sz, &free);
    if (status != SQLITE_OK) {
      free(strp);
      throw std::runtime_error((boost::format("Argument bind failed: %1%") % sqlite3_errmsg(db_for_errors)).str());
    }
  }

  void bind_text(int i, const boost::optional<std::string> &str) {
    if (str) {
      bind_text(i, *str);

    } else {
      int status = sqlite3_bind_null(ptr.get(), i);
      if (status != SQLITE_OK) {
        throw std::runtime_error((boost::format("Argument bind failed: %1%") % sqlite3_errmsg(db_for_errors)).str());
      }
    }
  }

  void bind_time(int i, std::time_t t) {
    int status = sqlite3_bind_int64(ptr.get(), i, sqlite3_int64(t));
    if (status != SQLITE_OK) {
      throw std::runtime_error((boost::format("Argument bind failed: %1%") % sqlite3_errmsg(db_for_errors)).str());
    }
  }

  void bind_time(int i, boost::optional<std::time_t> t) {
    if (t) {
      bind_time(i, *t);

    } else {
      int status = sqlite3_bind_null(ptr.get(), i);
      if (status != SQLITE_OK) {
        throw std::runtime_error((boost::format("Argument bind failed: %1%") % sqlite3_errmsg(db_for_errors)).str());
      }
    }
  }

  void bind_blob(int i, std::stringstream &stream) {
    std::string str = stream.str();
    int sz = str.size();
    char *strp = static_cast<char *>(malloc(sz));
    if (strp == nullptr) { throw std::runtime_error("Unable to allocate memory for blob copy."); }
    memcpy(strp, str.c_str(), sz);
    int status = sqlite3_bind_blob(ptr.get(), i, strp, sz, &free);
    if (status != SQLITE_OK) {
      free(strp);
      throw std::runtime_error((boost::format("Argument bind failed: %1%") % sqlite3_errmsg(db_for_errors)).str());
    }
  }

private:
  friend struct db;
  statement(sqlite3 *db, const std::string &sql) 
    : ptr(), db_for_errors(db) {
    const char *tail = nullptr;
    sqlite3_stmt *ptr_ = nullptr;
    int status = sqlite3_prepare_v2(db, sql.c_str(), sql.size(), &ptr_, &tail);
    if (status != SQLITE_OK) {
      throw std::runtime_error((boost::format("Unable to prepare SQLite3 statement \"%1%\": %2%") % sql % sqlite3_errmsg(db_for_errors)).str());
    }
    ptr.reset(ptr_);
  }

  std::unique_ptr<sqlite3_stmt, sqlite_statement_finalizer> ptr;
  sqlite3 *db_for_errors; // use for ERRORS only.
};

struct db {
  db(const std::string &loc) {
    sqlite3 *ptr_;
    int status = sqlite3_open(loc.c_str(), &ptr_);
    if (status != SQLITE_OK) {
      throw std::runtime_error((boost::format("Unable to open SQLite3 database \"%1%\": %2%") % loc % sqlite3_errmsg(ptr_)).str());
    }
    ptr.reset(ptr_);
  }

  statement prepare(const std::string &sql) {
    return statement(ptr.get(), sql);
  }

private:
  std::unique_ptr<sqlite3, sqlite_db_deleter> ptr;
};

} // namespace sqlite

struct cache {
  cache(const std::string &loc) 
    : m_db(new sqlite::db(loc)) {

    // ensure that the table for the cache data exists
    sqlite::statement s(m_db->prepare("SELECT name FROM sqlite_master WHERE type='table' AND name='cache'"));
    if (!s.step()) {
      // table doesn't exist, so create it
      m_db->prepare("CREATE TABLE cache (url TEXT PRIMARY KEY, expires INTEGER, last_modified INTEGER, etag TEXT, body BLOB)").step();
    }
  }

  void lookup(std::unique_ptr<request> &req) {
    sqlite::statement s(m_db->prepare("select expires, last_modified, etag, body from cache where url=?"));
    s.bind_text(1, req->url);

    if (s.step()) {
      req->expires = s.column_time(0);
      req->last_modified = s.column_time(1);
      req->etag = s.column_text(2);
      s.column_blob(3, *(req->stream));
    }
  }

  void write(request *req) {
    // first, normalise the request by collapsing any Cache-control / Expires headers.
    if (req->max_age) {
      req->expires = time(nullptr) + *req->max_age;

    } else if (bool(req->expires) && bool(req->base_date)) {
      req->expires = time(nullptr) + (*req->expires - *req->base_date);

    } else {
      req->expires = boost::none;
    }

    sqlite::statement s(m_db->prepare("insert or replace into cache (url, expires, last_modified, etag, body) values (?, ?, ?, ?, ?)"));
    s.bind_text(1, req->url);
    s.bind_time(2, req->expires);
    s.bind_time(3, req->last_modified);
    s.bind_text(4, req->etag);
    s.bind_blob(5, *(req->stream));
    s.step();
  }

private:
  std::unique_ptr<sqlite::db> m_db;
};

#else /* HAVE_SQLITE3 */
struct cache {
  cache(const std::string &) { not_implemented(); }
  void lookup(std::unique_ptr<request> &req) { not_implemented(); }
  void write(request *req) { not_implemented(); }
  void not_implemented() const { 
    throw std::runtime_error("Caching is not implemented because avecado was built without SQLite3 support.");
  }
};
#endif /* HAVE_SQLITE3 */

} // anonymous namespace

struct http::impl {
  impl(std::vector<std::string> &&patterns);
  ~impl();

  void start_request(std::promise<fetch_response> &&promise, int z, int x, int y);

  void enable_cache(const std::string &cache_location);
  void disable_cache();

private:
  void thread_func();
  void run_curl_multi(CURLM *curl_multi, int *running_handles);
  void perform_multi(CURLM *curl_multi, int *running_handles);
  void handle_response(CURLcode res, CURL *curl);
  void free_handle(CURL *curl);
  CURL *new_handle();
  boost::optional<fetch_error> new_request(CURL *curl, request *r);
  std::string url_for(int z, int x, int y) const;
  bool setup_response_tile(fetch_response &response, std::unique_ptr<std::stringstream> &stream);

  const std::vector<std::string> m_url_patterns;
  std::atomic<bool> m_shutdown;
  std::thread m_thread;
  std::mutex m_mutex;
  std::list<std::unique_ptr<request> > m_new_requests;
  std::queue<CURL*> m_handle_pool;
  // note: m_cache is *shared* between threads, so it *must* be thread-safe
  std::shared_ptr<cache> m_cache;
};

http::impl::impl(std::vector<std::string> &&patterns)
  : m_url_patterns(patterns)
  , m_shutdown(false)
  , m_thread(&impl::thread_func, this) {
}

http::impl::~impl() {
  m_shutdown.store(true);
  m_thread.join();
}

void http::impl::start_request(std::promise<fetch_response> &&promise, int z, int x, int y) {
  std::unique_ptr<request> req(new request(std::move(promise), z, x, y, url_for(z, x, y)));

  if (m_cache) {
    m_cache->lookup(req);
  }

  if (req->expired()) {
    std::unique_lock<std::mutex> lock(m_mutex);
    m_new_requests.emplace_back(std::move(req));

  } else {
    fetch_error err;
    err.status = fetch_status::server_error;
    fetch_response response(err);

    setup_response_tile(response, req->stream);

    req->promise.set_value(std::move(response));
  }
}

void http::impl::thread_func() {
  CURLM *curl_multi = curl_multi_init();
  int running_handles = 0;

  while (m_shutdown.load() == false) {
    std::list<std::unique_ptr<request> > requests;
    {
      std::unique_lock<std::mutex> lock(m_mutex);
      requests.swap(m_new_requests);
    }

    bool added = false;
    for (auto &ptr : requests) {
      request *req = ptr.release();
      CURL *curl = new_handle();
      boost::optional<fetch_error> err = new_request(curl, req);

      if (err) {
        req->promise.set_value(fetch_response(*err));
        delete req;
        free_handle(curl);

      } else {
        curl_multi_add_handle(curl_multi, curl);
        added = true;
      }
    }

    if (added) {
      perform_multi(curl_multi, &running_handles);
    }

    run_curl_multi(curl_multi, &running_handles);
  }

  while (running_handles > 0) {
    run_curl_multi(curl_multi, &running_handles);
  }

  while (!m_handle_pool.empty()) {
    CURL *curl = m_handle_pool.front();
    m_handle_pool.pop();
    curl_easy_cleanup(curl);
  }

  curl_multi_cleanup(curl_multi);
}

void http::impl::run_curl_multi(CURLM *curl_multi, int *running_handles) {
  struct timeval timeout;
  long timeout_ms = 0;

  curl_multi_timeout(curl_multi, &timeout_ms);
  if (timeout_ms < 0) {
    // no curl timeout, so set a default
    timeout.tv_sec = 0L;
    timeout.tv_usec = 100000L;

  } else {
    timeout.tv_sec = timeout_ms / 1000L;
    timeout.tv_usec = (timeout_ms % 1000L) * 1000L;
  }

  int bits_set = 0;
  if (timeout_ms != 0) {
    int max_fd = 0;
    fd_set read_fd_set, write_fd_set, exc_fd_set;

    FD_ZERO(&read_fd_set);
    FD_ZERO(&write_fd_set);
    FD_ZERO(&exc_fd_set);

    CURLMcode res = curl_multi_fdset(curl_multi, &read_fd_set, &write_fd_set, &exc_fd_set, &max_fd);
    if (res != CURLM_OK) {
      // TODO: better error handling & reporting
      std::cerr << "Error in curl_multi_fdset.\n" << std::flush;

    } else {
      if (max_fd > 0) {
        bits_set = select(max_fd + 1, &read_fd_set, &write_fd_set, &exc_fd_set, &timeout);

        // TODO: handle this error better...
        if (bits_set < 0) {
          std::cerr << "Error in select: " << strerror(errno) << "\n" << std::flush;
        }
      }
    }
  }

  perform_multi(curl_multi, running_handles);
}

void http::impl::perform_multi(CURLM *curl_multi, int *running_handles) {
  int msgs_in_queue = 0;
  CURLMcode res = CURLM_OK;
  bool any_done = false;

  do {
    do {
      res = curl_multi_perform(curl_multi, running_handles);
    } while (res == CURLM_CALL_MULTI_PERFORM);

    CURLMsg *msg = nullptr;
    any_done = false;
    while ((msg = curl_multi_info_read(curl_multi, &msgs_in_queue)) != nullptr) {
      if (msg->msg == CURLMSG_DONE) {
        handle_response(msg->data.result, msg->easy_handle);
        curl_multi_remove_handle(curl_multi, msg->easy_handle);
        free_handle(msg->easy_handle);
        any_done = true;
      }
    }
  } while (any_done);
}

void http::impl::handle_response(CURLcode res, CURL *curl) {
  request *req = nullptr;
  CURLcode res2 = curl_easy_getinfo(curl, CURLINFO_PRIVATE, &req);

  fetch_error err;
  err.status = fetch_status::server_error;
  fetch_response response(err);

  if (res != CURLE_OK) {
    if (res == CURLE_REMOTE_FILE_NOT_FOUND) {
      err.status = fetch_status::not_found;
    }
    response = fetch_response(err);

  } else {
    long status_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status_code);

    if (status_code == 200) {
      setup_response_tile(response, req->stream);
      if (m_cache) {
        m_cache->write(req);
      }

    } else {
      switch (status_code) {
      case 400: err.status = fetch_status::bad_request; break;
      case 404: err.status = fetch_status::not_found; break;
      case 501: err.status = fetch_status::not_implemented; break;
      default:
        err.status = fetch_status::server_error;
      }
      response = fetch_response(err);
    }
  }

  req->promise.set_value(std::move(response));
  delete req;
}

bool http::impl::setup_response_tile(fetch_response &response, std::unique_ptr<std::stringstream> &stream) {
  std::unique_ptr<tile> ptr(new tile);

  google::protobuf::io::IstreamInputStream gstream(stream.get());

  bool ok = ptr->mapnik_tile().ParseFromZeroCopyStream(&gstream);
  if (ok) {
    response = fetch_response(std::move(ptr));
  }

  return ok;
}

void http::impl::free_handle(CURL *curl) {
  if (m_handle_pool.size() > MAX_POOL_SIZE) {
    curl_easy_cleanup(curl);

  } else {
    m_handle_pool.push(curl);
  }
}

CURL *http::impl::new_handle() {
  CURL *handle = nullptr;

  if (m_handle_pool.empty()) {
    handle = curl_easy_init();

  } else {
    handle = m_handle_pool.front();
    m_handle_pool.pop();
  }

  return handle;
}

boost::optional<fetch_error> http::impl::new_request(CURL *curl, request *r) {
  fetch_error err;
  err.status = fetch_status::server_error;

  CURLcode res = curl_easy_setopt(curl, CURLOPT_URL, r->url.c_str());
  if (res != CURLE_OK) { return err; }

  res = curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
  if (res != CURLE_OK) { return err; }

  res = curl_easy_setopt(curl, CURLOPT_WRITEDATA, r->stream.get());
  if (res != CURLE_OK) { return err; }

  res = curl_easy_setopt(curl, CURLOPT_PRIVATE, r);
  if (res != CURLE_OK) { return err; }

  res = curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, header_callback);
  if (res != CURLE_OK) { return err; }

  res = curl_easy_setopt(curl, CURLOPT_HEADERDATA, r);
  if (res != CURLE_OK) { return err; }

  return boost::none;
}

std::string http::impl::url_for(int z, int x, int y) const {
  using namespace boost::xpressive;

  if (m_url_patterns.empty()) {
    throw std::runtime_error("no URL patterns in fetcher");
  }

  sregex var = "{" >> (s1 = range('x','z')) >> "}";
  return regex_replace(m_url_patterns[0], var, formatter(z, x, y));
}

void http::impl::enable_cache(const std::string &cache_location) {
  m_cache = std::make_shared<cache>(cache_location);
}

void http::impl::disable_cache() {
  m_cache.reset();
}

http::http(const std::string &base_url, const std::string &ext)
  : m_impl(new impl(singleton(base_url, ext))) {
}

http::http(std::vector<std::string> &&patterns)
  : m_impl(new impl(std::move(patterns))) {
}

http::~http() {
}

std::future<fetch_response> http::operator()(int z, int x, int y) {
  std::promise<fetch_response> promise;
  std::future<fetch_response> future = promise.get_future();
  m_impl->start_request(std::move(promise), z, x, y);
  return future;
}

void http::enable_cache(const std::string &cache_location) {
  m_impl->enable_cache(cache_location);
}

void http::disable_cache() {
  m_impl->disable_cache();
}

} } // namespace avecado::fetch
