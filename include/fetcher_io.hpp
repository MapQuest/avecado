#ifndef FETCHER_IO_HPP
#define FETCHER_IO_HPP

#include "fetcher.hpp"

#include <ostream>

namespace avecado {

std::ostream &operator<<(std::ostream &out, fetch_status status);

} // namespace avecado

#endif /* FETCHER_IO_HPP */
