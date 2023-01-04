#ifndef SMALL_VECTOR_HPP
#define SMALL_VECTOR_HPP

#include <cassert>
#include <cstdint>
#include <initializer_list>
#include <iterator>
#include <limits>
#include <memory>
#include <new>
#include <stdexcept>

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

  ~SmallVectorImpl() {
    clear();
    free_storage();
    set_internal_storage();
  }

  SmallVectorImpl &operator=(const SmallVectorImpl &RHS) {
    clear();
    assign_sequence(RHS.begin(), RHS.end());
    return *this;
  }

  SmallVectorImpl &operator=(SmallVectorImpl &&RHS) {
    clear();
    if (RHS.is_internal_storage())
      assign_sequence_by_move(RHS.begin(), RHS.end());
    else {
      free_storage();
      data_ = RHS.data_;
      size_ = RHS.size_;
      capacity_ = RHS.capacity_;
      RHS.set_internal_storage();
      RHS.size_ = 0;
    }
    return *this;
  }

  SmallVectorImpl &operator=(std::initializer_list<T> ilist) {
    assign(ilist);
    return *this;
  }

  void assign(size_type count, const T &value) {
    clear();
    construct_until_size_is(count, value);
  };

  template <typename InputIt> void assign(InputIt first, InputIt last) {
    clear();
    assign_sequence(std::move(first), std::move(last));
  }

  void assign(std::initializer_list<T> ilist) {
    clear();
    assign_sequence_by_move(ilist.begin(), ilist.end());
  }

  T *data() noexcept { return data_; }
  const T *data() const noexcept { return data_; }
  size_type size() const noexcept { return size_; }
  bool empty() const noexcept { return !size(); }
  size_type max_size() const noexcept { return std::numeric_limits<internal_size_type>::max(); }
  size_type capacity() const noexcept { return capacity_; }

  void reserve(size_type capacity) {
    if (capacity > capacity_)
      move_to_new_storage(capacity);
  };

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

  void clear() noexcept { delete_until_size_is(0); };

  void resize(size_type count) {
    delete_until_size_is(count);
    construct_until_size_is(count);
  };

  void resize(size_type count, const T &value) {
    delete_until_size_is(count);
    construct_until_size_is(count, value);
  };

  void push_back(const T &value) { emplace_back(value); };
  void push_back(T &&value) { emplace_back(std::move(value)); };

  template <typename... Args> void emplace_back(Args &&...args) {
    if (size_ == capacity_)
      move_to_new_storage(capacity_ * 2);
    unchecked_emplace_back(std::forward<Args>(args)...);
  };

  void pop_back() noexcept {
    std::destroy_at(std::addressof(back()));
    --size_;
  }

  // TODO: insert/emplace/erase

  void swap(SmallVectorImpl &RHS) {
    if (!is_internal_storage() && RHS.is_internal_storage()) {
      std::swap(data_, RHS.data_);
      std::swap(size_, RHS.size_);
      std::swap(capacity_, RHS.capacity_);
    }
    // TODO: swap for internal storage
  };

protected:
  SmallVectorImpl(size_t internal_capacity) noexcept
      : data_(get_internal_storage()), size_(0), capacity_(internal_capacity){};

private:
  T *data_;
  internal_size_type size_, capacity_;

  T *get_internal_storage() noexcept;
  const T *get_internal_storage() const noexcept;

  template <typename Iterator> void assign_sequence(Iterator first, Iterator last) {
    assert(empty());
    assign_sequence_impl(first, last, typename std::iterator_traits<Iterator>::iterator_category{});
  };

  template <typename Iterator>
  void assign_sequence_impl(Iterator first, Iterator last, std::input_iterator_tag) {
    for (; first != last; ++first)
      emplace_back(*first);
  }

  template <typename Iterator>
  void assign_sequence_impl(Iterator first, Iterator last, std::random_access_iterator_tag) {
    reserve(last - first);
    for (; first != last; ++first)
      unchecked_emplace_back(*first);
  }

  template <typename Iterator> void assign_sequence_by_move(Iterator first, Iterator last) {
    assert(empty());
    assign_sequence_by_move_impl(first, last,
                                 typename std::iterator_traits<Iterator>::iterator_category{});
  }

  template <typename Iterator>
  void assign_sequence_by_move_impl(Iterator first, Iterator last, std::input_iterator_tag) {
    for (; first != last; ++first)
      emplace_back(std::move(*first));
  }

  template <typename Iterator>
  void assign_sequence_by_move_impl(Iterator first, Iterator last,
                                    std::random_access_iterator_tag) {
    reserve(last - first);
    for (; first != last; ++first)
      unchecked_emplace_back(std::move(*first));
  }

  bool range_check(size_type pos) const noexcept { return pos < size_; }

  bool is_internal_storage() const noexcept { return data_ == get_internal_storage(); }

  void set_internal_storage() noexcept { data_ = get_internal_storage(); }

  void move_to_new_storage(internal_size_type new_capacity) {
    assert(size_ < new_capacity);
    T *new_data =
        static_cast<T *>(::operator new(new_capacity * sizeof(T), std::align_val_t(alignof(T))));

    for (internal_size_type i = 0; i < size_; ++i)
      std::construct_at(new_data + i, std::move(data_[i]));
    std::destroy_n(data_, size_);

    free_storage();
    data_ = new_data;
    capacity_ = new_capacity;
  }

  void free_storage() const noexcept {
    if (!is_internal_storage())
      ::operator delete(static_cast<void *>(data_), std::align_val_t(alignof(T)));
  }

  template <typename... Args> void unchecked_emplace_back(Args &&...args) {
    assert(size_ < capacity_);
    std::construct_at(std::addressof(*end()), std::forward<Args>(args)...);
    ++size_;
  }

  void construct_until_size_is(internal_size_type count) {
    reserve(count);
    while (size_ < count)
      unchecked_emplace_back();
  }

  void construct_until_size_is(internal_size_type count, const T &value) {
    reserve(count);
    while (size_ < count)
      unchecked_emplace_back(value);
  }

  void delete_until_size_is(internal_size_type count) noexcept {
    while (size_ > count)
      pop_back();
  }
};

template <typename T, size_t N> class SmallVector : public SmallVectorImpl<T> {
private:
  static_assert(N != 0, "SmallVector must have non-zero internal storage size");
  friend class SmallVectorImpl<T>;
  using Base = SmallVectorImpl<T>;

public:
  using size_type = typename Base::size_type;

  SmallVector() : Base(N){};

  explicit SmallVector(size_type count, const T &value = T()) : Base(N) {
    this->assign(count, value);
  }

  template <typename InputIt> SmallVector(InputIt first, InputIt last) : Base(N) {
    this->assign(first, last);
  }

  SmallVector(const SmallVector &RHS) : Base(N) {
    if (!RHS.empty())
      Base::operator=(RHS);
  }

  SmallVector(SmallVector &&RHS) : Base(N) {
    if (!RHS.empty())
      Base::operator=(std::move(RHS));
  }

  SmallVector(SmallVectorImpl<T> &&RHS) : Base(N) {
    if (!RHS.empty())
      Base::operator=(std::move(RHS));
  }

  SmallVector(std::initializer_list<T> ilist) : Base(N) {
    if (!std::empty(ilist))
      Base::operator=(ilist);
  }

  SmallVector &operator=(const SmallVector &RHS) {
    Base::operator=(RHS);
    return *this;
  }

  SmallVector &operator=(SmallVector &&RHS) {
    Base::operator=(std::move(RHS));
    return *this;
  }

  SmallVector &operator=(SmallVectorImpl<T> &&RHS) {
    Base::operator=(std::move(RHS));
    return *this;
  }

  SmallVector &operator=(std::initializer_list<T> ilist) {
    Base::operator=(ilist);
    return *this;
  }

private:
  std::aligned_storage_t<N * sizeof(T), alignof(T)> internal_storage_;
};

template <typename T> T *SmallVectorImpl<T>::get_internal_storage() noexcept {
  return reinterpret_cast<T *>(&static_cast<SmallVector<T, 1> *>(this)->internal_storage_);
}

template <typename T> const T *SmallVectorImpl<T>::get_internal_storage() const noexcept {
  return reinterpret_cast<const T *>(
      &static_cast<const SmallVector<T, 1> *>(this)->internal_storage_);
}

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
