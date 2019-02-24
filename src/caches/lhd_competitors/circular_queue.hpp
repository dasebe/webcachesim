#pragma once

namespace misc_competitors {

  // This data structure gives you the view of an infinitely-large
  // queue where only a small region of the queue is active at a
  // time. We use this to track the trace as it grows without any
  // dynamic memory allocation.
  template<class T, size_t SIZE>
  class CircularQueue {
  public:
    CircularQueue(const T& _DEFAULT)
      : DEFAULT(_DEFAULT) {
      for (uint32_t i = 0; i < SIZE; i++) {
        data[i] = DEFAULT;
      }
    }

    void pop_front() {
      data[ head % SIZE ] = DEFAULT;
      ++head;
    }

    void push_back(const T& value) {
      data[ tail % SIZE ] = value;
      ++tail;
    }

    T& front() {
      return data[ head % SIZE ];
    }

    T& back() {
      return data[ tail % SIZE ];
    }

    T& at(off_t idx) {
      assert(head <= idx && idx <= tail);
      return data[ idx % SIZE ];
    }

    T& operator[] (off_t idx) {
      return at(idx);
    }

    size_t size() const {
      return tail - head;
    }

    const T DEFAULT;

  private:
    off_t head, tail;
    T data[SIZE];
  };

  template<class T>
  class CircularNQueue {
  public:
    CircularNQueue(const T& _DEFAULT, const size_t _SIZE)
      : DEFAULT(_DEFAULT) 
      , SIZE(_SIZE)
    {
      data = new T[SIZE];
      for (uint32_t i = 0; i < SIZE; i++) {
        data[i] = DEFAULT;
      }
    }

    void pop_front() {
      data[ head % SIZE ] = DEFAULT;
      ++head;
    }

    void push_back(const T& value) {
      data[ tail % SIZE ] = value;
      ++tail;
    }

    T& front() {
      return data[ head % SIZE ];
    }

    T& back() {
      return data[ tail % SIZE ];
    }

    T& at(off_t idx) {
      // assert(head <= idx && idx <= tail);
      return data[ idx % SIZE ];
    }

    T& operator[] (off_t idx) {
      return at(idx);
    }

    size_t size() const {
      return tail - head;
    }

    const T DEFAULT;

  private:
    off_t head, tail;
    size_t SIZE;
    T *data;
  };

}
