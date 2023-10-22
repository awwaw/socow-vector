#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <memory>
#include <new>

template <typename T, size_t SMALL_SIZE>
class socow_vector {

public:
  using value_type = T;

  using reference = T&;
  using const_reference = const T&;

  using pointer = T*;
  using const_pointer = const T*;

  using iterator = pointer;
  using const_iterator = const_pointer;

public:
  // Data Structure

  struct data_storage {
    size_t _reference_counter;
    size_t _capacity;

    T _data[0];

    explicit data_storage(size_t capacity) : _reference_counter(1), _capacity(capacity) {}

    bool was_copied() {
      return _reference_counter > 1;
    }

    void add_copy() {
      _reference_counter++;
    }
  };

public:
  // Constructors

  socow_vector() noexcept : _size(0), _is_small(true) {}

  socow_vector(size_t cap) : _size(0), _is_small(true) {
    reserve(cap);
  }

  socow_vector(const socow_vector& other) : _size(other._size), _is_small(other._is_small) {
    if (other._is_small) {
      copy_data(other.small_storage.begin(), small_storage.begin(), 0, other._size);
    } else {
      big_storage = other.big_storage;
      big_storage->add_copy();
    }
  }

private:
  void delete_storage(data_storage* storage, iterator start, iterator end) {
    if (storage->_reference_counter-- == 1) {
      remove_from_storage(start, end);
      operator delete(storage);
    }
  }

  void destruct() {
    if (_is_small) {
      remove_from_storage(begin(), end());
    } else {
      delete_storage(big_storage, my_begin(), my_end());
    }
  }

public:
  // Destructor

  ~socow_vector() {
    destruct();
  }

public:
  // Ass
  // ignment

  socow_vector& operator=(const socow_vector& other) {
    if (this == &other) {
      return *this;
    }
    if (other._is_small) {
      if (_is_small) {
        socow_vector cpy;
        std::uninitialized_copy_n(other.begin(), std::min(_size, other._size), cpy.begin());
        cpy._size = std::min(_size, other._size);
        std::destroy_n(small_storage.begin() + cpy._size, _size - cpy._size);
        std::uninitialized_copy(other.begin() + cpy._size, other.end(), small_storage.begin() + cpy._size);
        _size = other._size;
        std::swap_ranges(cpy.begin(), cpy.end(), small_storage.begin());
      } else {
        data_storage* cpy = big_storage;
        try {
          copy_data(other.small_storage.begin(), small_storage.begin(), 0, other._size);
        } catch (...) {
          big_storage = cpy;
          throw;
        }

        delete_storage(cpy, cpy->_data, cpy->_data + _size);
      }
    } else {

      destruct();
      big_storage = other.big_storage;
      other.big_storage->add_copy();
    }
    _size = other.size();
    _is_small = other._is_small;
    return *this;
  }

public:
  // Iterator methods

  iterator begin() {
    return data();
  }

  const_iterator begin() const {
    return data();
  }

  iterator end() {
    return begin() + _size;
  }

  const_iterator end() const {
    return begin() + _size;
  }

  iterator insert(const_iterator pos, const T& value) {
    ptrdiff_t idx = pos - my_begin();
    if (size() == capacity() || (!_is_small && big_storage->was_copied())) {
      size_t new_cap = capacity() * (size() == capacity() ? 2 : 1);
      socow_vector tmp(new_cap);
      copy_data(my_begin(), tmp.big_storage->_data, 0, idx);
      tmp._size += idx;
      new (tmp.big_storage->_data + idx) T(value);
      tmp._size++;
      copy_data(my_begin(), tmp.big_storage->_data, idx, _size, idx + 1);
      tmp._size += _size - idx;

      *this = tmp;
      _is_small = false;
    } else {
      new (my_end()) T(value);
      _size++;
      for (size_t i = size() - 1; i > idx; i--) {
        std::swap(operator[](i), operator[](i - 1));
      }
    }
    return my_begin() + idx;
  }

  iterator erase(const_iterator pos) {
    return erase(pos, pos + 1);
  }

  iterator erase(const_iterator first, const_iterator last) {
    size_t segment_length = last - first;
    size_t first_index = first - my_begin();
    size_t remainder = _size - first_index - segment_length;
    size_t last_index = last - my_begin();
    if (segment_length == 0) {
      return my_begin() + first_index;
    }

    if (!_is_small && big_storage->was_copied()) {
      socow_vector cpy(capacity());
      std::uninitialized_copy_n(my_begin(), first_index, cpy.my_begin());
      cpy._size += first_index;
      std::uninitialized_copy_n(my_begin() + last_index, remainder, cpy.my_begin() + first_index);
      cpy._size += remainder;
      *this = cpy;
    } else {
      for (size_t from = first_index, to = from + segment_length; to < _size; from++, to++) {
        std::swap(operator[](from), operator[](to));
      }
      size_t new_size = _size - segment_length;
      std::destroy(begin() + new_size, end());
      _size = new_size;
    }
    return my_begin() + first_index;
  }

public:
  void swap(socow_vector& other) {
    if (&other == this) {
      return;
    }
    if (_size > other._size) {
      other.swap(*this);
      return;
    }
    if (_is_small && other._is_small) {
      copy_data(other.small_storage.begin(), small_storage.begin(), _size, other._size);
      try {
        std::swap_ranges(small_storage.begin(), small_storage.begin() + _size, other.small_storage.begin());
      } catch (...) {
        remove_from_storage(small_storage.begin() + _size, small_storage.begin() + other._size);
        throw;
      }
      remove_from_storage(other.small_storage.begin() + _size, other.small_storage.begin() + other._size);
    } else if (!_is_small && !other._is_small) {
      std::swap(big_storage, other.big_storage);
    } else {
      data_storage* tmp = other.big_storage;
      other.big_storage = nullptr;
      try {
        copy_data(small_storage.begin(), other.small_storage.begin(), 0, _size);
      } catch (...) {
        other.big_storage = tmp;
        throw;
      }
      remove_from_storage(begin(), end());
      big_storage = tmp;
    }
    std::swap(_size, other._size);
    std::swap(_is_small, other._is_small);
  }

  size_t size() const noexcept {
    return _size;
  }

  size_t capacity() const noexcept {
    return _is_small ? SMALL_SIZE : big_storage->_capacity;
  }

  T& operator[](size_t idx) {
    return *(begin() + idx);
  }

  const T& operator[](size_t idx) const {
    return *(begin() + idx);
  }

  T* data() {
    if (_is_small) {
      return small_storage.data();
    }
    if (!_is_small && big_storage->was_copied()) {
      dereference(capacity());
    }
    return big_storage->_data;
  }

  const T* data() const {
    return _is_small ? small_storage.data() : big_storage->_data;
  }

  T& front() {
    return *begin();
  }

  const T& front() const {
    return *begin();
  }

  T& back() {
    return *(end() - 1);
  }

  const T& back() const {
    return *(end() - 1);
  }

  bool empty() const noexcept {
    return size() == 0;
  }

  void push_back(const T& elem) {
    insert(my_end(), elem);
  }

  void pop_back() {
    if (!_is_small && big_storage->was_copied()) {
      dereference(capacity(), _size - 1);
    } else {
      (end() - 1)->~T();
    }
    _size--;
  }

  void clear() {
    if (!_is_small && big_storage->_reference_counter > 1) {
      big_storage->_reference_counter--;
      big_storage = allocate_new_storage(capacity());
      _size = 0;
      return;
    }
    erase(begin(), end());
  }

  void reserve(size_t n) {
    // small -> small: ignore           is_small && n <= SMALL_SIZE     => return
    // small -> big  : reserve n        is_small && n > SMALL_SIZE      => dereference(n)
    // big   -> small: make small       !is_small && n >= size()        => make small storage
    // big   -> big  : reserve n        !is_small && n >= size()        => dereference(n)
    // if capacity > n && is big, uniq => ignore
    if (_is_small && n <= SMALL_SIZE) {
      return; // копировать маленький в маленький не надо (тест reserve_small, reserve_empty)
    }
    if (capacity() > n && !_is_small && big_storage->_reference_counter == 1) {
      return; // если уникальный большой контейнер, и capacity не увеличивается, то игнорим (тест
              // reserve_superfluous_single_user)
    }
    if (n < size()) {
      return; // нельзя зарезервировать меньше, чем уже есть элементов (тест reserve_superfluous_2)
    }
    if (!_is_small && n <= SMALL_SIZE) {
      shrink_to_fit();
      return;
    }
    dereference(n);
  }

  void shrink_to_fit() {
    if (_is_small || size() == capacity()) {
      return;
    }
    if (size() <= SMALL_SIZE) {
      data_storage* cpy = big_storage;
      big_storage = nullptr;
      try {
        copy_data(cpy->_data, small_storage.begin(), 0, _size);
      } catch (...) {
        big_storage = cpy;
        throw;
      }
      delete_storage(cpy, cpy->_data, cpy->_data + _size);
      _is_small = true;
    } else {
      dereference(size());
    }
  }

private:
  iterator my_begin() {
    return _is_small ? small_storage.begin() : big_storage->_data;
  }

  iterator my_end() {
    return my_begin() + _size;
  }

  void dereference(size_t capacity, size_t cnt) {
    data_storage* tmp = make_new_copy(capacity, cnt);
    destruct();
    _is_small = false;
    big_storage = tmp;
  }

  void dereference(size_t capacity) {
    return dereference(capacity, _size);
  }

  void remove_from_storage(iterator start, iterator end) {
    if (start > end) {
      return;
    }
    std::destroy(start, end);
  }

  void copy_data(const T* from, T* to, size_t start, size_t end, size_t start_to) {
    std::uninitialized_copy_n(from + start, end - start, to + start_to);
  }

  void copy_data(const T* from, T* to, size_t start, size_t end) {
    return copy_data(from, to, start, end, start);
  }

private:
  static data_storage* allocate_new_storage(size_t capacity) {
    return new (static_cast<data_storage*>(operator new(sizeof(data_storage) + capacity * sizeof(T))))
        data_storage(capacity);
  }

  data_storage* make_new_copy(size_t capacity, size_t cnt) {
    data_storage* cpy = allocate_new_storage(capacity);
    try {
      copy_data(my_begin(), cpy->_data, 0, cnt);
    } catch (...) {
      operator delete(cpy);
      throw;
    }
    return cpy;
  }

  size_t _size;
  bool _is_small;

  union {
    std::array<T, SMALL_SIZE> small_storage;
    data_storage* big_storage;
  };
};
