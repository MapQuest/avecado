#include "fetch/http.hpp"
#include "vector_tile.pb.h"

#include <boost/format.hpp>
#include <boost/algorithm/string/find_format.hpp>
#include <boost/xpressive/xpressive.hpp>

#include <sstream>
#include <list>
#include <queue>
#include <google/protobuf/io/zero_copy_stream.h>
#include <google/protobuf/io/zero_copy_stream_impl.h>

#include <curl/curl.h>

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
  request(std::promise<fetch_response> &&p_, int z_, int x_, int y_) 
    : promise(std::move(p_)), z(z_), x(x_), y(y_), stream(new std::stringstream) {}

  request(request &&r)
    : promise(std::move(r.promise))
    , z(r.z), x(r.x), y(r.y)
    , stream(std::move(r.stream)) {
  }
    
  std::promise<fetch_response> promise;
  int z, x, y;
  std::unique_ptr<std::stringstream> stream;
};

} // anonymous namespace

struct http::impl {
  impl(std::vector<std::string> &&patterns);
  ~impl();

  void start_request(std::promise<fetch_response> &&promise, int z, int x, int y);

private:
  void thread_func();
  void run_curl_multi(CURLM *curl_multi, int *running_handles);
  void perform_multi(CURLM *curl_multi, int *running_handles);
  void handle_response(CURLcode res, CURL *curl);
  void free_handle(CURL *curl);
  CURL *new_handle();
  boost::optional<fetch_error> new_request(CURL *curl, request *r);
  std::string url_for(int z, int x, int y) const;

  const std::vector<std::string> m_url_patterns;
  std::atomic<bool> m_shutdown;
  std::thread m_thread;
  std::mutex m_mutex;
  std::list<request*> m_new_requests;
  std::queue<CURL*> m_handle_pool;
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
  std::unique_lock<std::mutex> lock(m_mutex);
  m_new_requests.emplace_back(new request(std::move(promise), z, x, y));
}

void http::impl::thread_func() {
  CURLM *curl_multi = curl_multi_init();
  int running_handles = 0;

  while (m_shutdown.load() == false) {
    std::list<request*> requests;
    {
      std::unique_lock<std::mutex> lock(m_mutex);
      requests.swap(m_new_requests);
    }

    bool added = false;
    for (request *req : requests) {
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
    timeout.tv_sec = 0;
    timeout.tv_usec = 100000L;

  } else {
    timeout.tv_sec = timeout_ms / 1000L;
    timeout.tv_usec = (timeout_ms % 1000L) * 1000L;
  }

  int bits_set = 0;
  if (timeout_ms != 0) {
    int max_fd = -1;
    fd_set read_fd_set, write_fd_set, exc_fd_set;

    FD_ZERO(&read_fd_set);
    FD_ZERO(&write_fd_set);
    FD_ZERO(&exc_fd_set);

    curl_multi_fdset(curl_multi, &read_fd_set, &write_fd_set, &exc_fd_set, &max_fd);

    if (max_fd > 0) {
      bits_set = select(max_fd, &read_fd_set, &write_fd_set, &exc_fd_set, &timeout);

      // TODO: handle this error better...
      if (bits_set < 0) {
        std::cerr << "Error in select: " << strerror(errno) << "\n" << std::flush;
      }
    }
  }

  perform_multi(curl_multi, running_handles);
}

void http::impl::perform_multi(CURLM *curl_multi, int *running_handles) {
  int msgs_in_queue = 0;
  CURLMcode res = CURLM_OK;
  bool any_done = false;

  res = curl_multi_perform(curl_multi, running_handles);

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
      std::unique_ptr<tile> ptr(new tile);

      google::protobuf::io::IstreamInputStream gstream(req->stream.get());
      if (ptr->mapnik_tile().ParseFromZeroCopyStream(&gstream)) {
        response = fetch_response(std::move(ptr));
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

  std::string url = url_for(r->z, r->x, r->y);

  CURLcode res = curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
  if (res != CURLE_OK) { return err; }
 
  res = curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
  if (res != CURLE_OK) { return err; }

  res = curl_easy_setopt(curl, CURLOPT_WRITEDATA, r->stream.get());
  if (res != CURLE_OK) { return err; }

  res = curl_easy_setopt(curl, CURLOPT_PRIVATE, r);
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

} } // namespace avecado::fetch
