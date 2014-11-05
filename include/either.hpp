#ifndef EITHER_HPP
#define EITHER_HPP

#include <boost/variant.hpp>

namespace avecado {

/* Simple sum of two types, modelled on Haskell's Either.
 *
 * Useful for modelling errors without needing to pass one or other argument
 * by reference, or using exceptions. This, in turn, is useful when doing
 * asynchronous and/or future stuff, as that can get a little complicated
 * when there are exceptions involved.
 */
template <typename L, typename R>
struct either {
   inline either(const either<L, R> &other) : m_impl(other.m_impl) {}
   inline either(either<L, R> &&other) : m_impl(std::move(other.m_impl)) {}
   inline explicit either(const L &left) : m_impl(left) {}
   inline explicit either(L &&left) : m_impl(std::move(left)) {}
   inline explicit either(const R &right) : m_impl(right) {}
   inline explicit either(R &&right) : m_impl(std::move(right)) {}

   inline either<L, R> &operator=(const either<L, R> &other) { m_impl = other.m_impl; return *this; }
   inline either<L, R> &operator=(either<L, R> &&other) { m_impl = std::move(other.m_impl); return *this; }

   inline bool is_left() const { const L *ptr = boost::get<L>(&m_impl); return ptr != nullptr; }
   inline bool is_right() const { return !is_left(); }

   inline const L &left() const { return *boost::get<L>(&m_impl); }
   inline const R &right() const { return *boost::get<R>(&m_impl); }

private:
   boost::variant<L, R> m_impl;
};

} // namespace avecado

#endif /* FETCHER_HPP */
