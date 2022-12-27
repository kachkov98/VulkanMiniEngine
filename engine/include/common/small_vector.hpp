#ifndef SMALL_VECTOR_HPP
#define SMALL_VECTOR_HPP

#include <cassert>
#include <cstdint>
#include <initializer_list>
#include <iterator>
#include <limits>
#include <memory>
#include <stdexcept>
//#include <type_traits>
//#include <utility>

namespace vme {

template <typename T> class SmallVectorImpl {
private:
  using internal_size_type = uint32_t;

public:
  using value_type = T;
  using size_type = std::size_t;
  using difference_type = std::ptrdiff_t;
  using reference = T &;
  using const_reference = const T &;
  using pointer = T *;
  using const_pointer = const T *;
  using iterator = T *;
  using const_iterator = const T *;
  using reverse_iterator = std::reverse_iterator<iterator>;
  using const_reverse_iterator = std::reverse_iterator<const_iterator>;

  SmallVectorImpl() = delete;
  SmallVectorImpl(const SmallVectorImpl &) = delete;
  SmallVectorImpl(SmallVectorImpl &&) = delete;

  SmallVectorImpl &operator=(const SmallVectorImpl &RHS) { return *this; }
  SmallVectorImpl &operator=(SmallVectorImpl &&RHS) { return *this; }
  SmallVectorImpl &operator=(std::initializer_list<T> ilist) { return *this; }

  void assign(size_type count, const T &value);
  template <typename InputIt> void assign(InputIt first, InputIt last);
  void assign(std::initializer_list<T> ilist);

  T *data() noexcept { return data_; }
  const T *data() const noexcept { return data_; }
  size_type size() const noexcept { return size_; }
  bool empty() const noexcept { return !size(); }
  size_type max_size() const noexcept { return std::numeric_limits<internal_size_type>::max(); }
  size_type capacity() const noexcept { return capacity_; }
  void reserve(size_type capacity);
  void shrink_to_fit();

  iterator begin() noexcept { return data(); }
  const_iterator begin() const noexcept { return data(); }
  iterator end() noexcept { return data() + size(); }
  const_iterator end() const noexcept { return data() + size(); }
  reverse_iterator rbegin() noexcept { return reverse_iterator(begin()); }
  const_reverse_iterator rbegin() const noexcept { return const_reverse_iterator(begin()); }
  reverse_iterator rend() noexcept { return reverse_iterator(end()); }
  const_reverse_iterator rend() const noexcept { return const_reverse_iterator(end()); }

  reference at(size_type pos) {
    if (!range_check(pos))
      throw std::out_of_range("SmallVectorImpl::at()");
    return begin()[pos];
  }
  const_reference at(size_type pos) const {
    if (!range_check(pos))
      throw std::out_of_range("SmallVectorImpl::at()");
    return begin()[pos];
  }
  reference operator[](size_type pos) noexcept {
    assert(range_check(pos));
    return begin()[pos];
  }
  const_reference operator[](size_type pos) const noexcept {
    assert(range_check(pos));
    return begin()[pos];
  }
  reference front() noexcept {
    assert(!empty());
    return begin()[0];
  }
  const_reference front() const noexcept {
    assert(!empty());
    return begin()[0];
  }
  reference back() noexcept {
    assert(!empty());
    return end()[-1];
  }
  const_reference back() const noexcept {
    assert(!empty());
    return end()[-1];
  }

  void clear() noexcept;
  void resize(size_type count){};
  void resize(size_type count, const T &value){};
  void push_back(const T &value){};
  template <typename... Args> void emplace_back(Args &&...args){};
  void swap(SmallVectorImpl &RHS){};

protected:
  SmallVectorImpl(size_t internal_capacity) noexcept {};

private:
  T *data_;
  internal_size_type size_, capacity_;

  bool range_check(size_type pos) { return pos < size(); }
};

template <typename T, size_t N> class SmallVector : public SmallVectorImpl<T> {
public:
  using Base = SmallVectorImpl<T>;
  using size_type = Base::size_type;

  SmallVector() : Base(N){};
  explicit SmallVector(size_type count, const T &value = T()) : Base(N) {
    this->assign(count, value);
  }
  template <typename InputIt> SmallVector(InputIt first, InputIt last) : Base(N) {
    this->assign(first, last);
  }
  SmallVector(std::initializer_list<T> ilist) : Base(N) { this->assign(ilist); }

  SmallVector(const SmallVector &RHS) : Base(N) {
    if (!RHS.empty())
      Base::operator=(RHS);
  }

  SmallVector(SmallVector &&RHS) : Base(N) {
    if (!RHS.empty())
      Base::operator=(std::move(RHS));
  }

  SmallVector(Base &&RHS) : Base(N) {
    if (!RHS.empty())
      Base::operator=(std::move(RHS));
  }

  SmallVector &operator=(const SmallVector &RHS) {
    Base::operator=(RHS);
    return *this;
  }

  SmallVector &operator=(SmallVector &&RHS) {
    Base::operator=(std::move(RHS));
    return *this;
  }

  SmallVector &operator=(Base &&RHS) {
    Base::operator=(std::move(RHS));
    return *this;
  }

private:
  static_assert(N != 0, "SmallVector must have non-zero internal storage size");
  std::aligned_storage<N * sizeof(T), alignof(T)> internal_storage_;
};
} // namespace vme

namespace std {
template <typename T> inline void swap(vme::SmallVectorImpl<T> &LHS, vme::SmallVectorImpl<T> &RHS) {
  LHS.swap(RHS);
}

template <typename T, unsigned N>
inline void swap(vme::SmallVector<T, N> &LHS, vme::SmallVector<T, N> &RHS) {
  LHS.swap(RHS);
}
} // namespace std

#endif
