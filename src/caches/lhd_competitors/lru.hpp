#pragma once

#include <unordered_map>
#include "repl.hpp"

namespace repl {

  template <typename DataT>
  struct List {
    typedef DataT Data;

    struct Entry {
      Data data;
      Entry* prev;
      Entry* next;

      void remove() {
	assert(prev != nullptr);
	prev->next = next;
	assert(next != nullptr);
	next->prev = prev;
      }
    };

    List()
      : _head(new Entry{ Data(), nullptr, nullptr })
      , _tail(new Entry{ Data(), nullptr, nullptr })
    {
      _head->next = _tail;
      _tail->prev = _head;
    }

    ~List() {
      Entry* entry = _head;

      while (entry) {
	auto* next = entry->next;
	delete entry;
	entry = next;
      }
    }

    void insert_front(Entry* entry) {
      entry->prev = _head;
      entry->next = _head->next;
      _head->next->prev = entry;
      _head->next = entry;
    }

    void insert_back(Entry* entry) {
      entry->prev = _tail->prev;
      entry->next = _tail;
      _tail->prev->next = entry;
      _tail->prev = entry;
    }

    Data& front() {
      return _head->next->data;
    }

    Data& back() {
      return _tail->prev->data;
    }

    bool empty() const {
      return _head->next == _tail;
    }

    Entry *begin() const {
      return _head->next;
    }

    Entry *end() const {
      return _tail;
    }

    Entry *_head, *_tail;
  };

  template <typename Data>
  struct Tags 
    : public std::unordered_map<candidate_t, typename List<Data>::Entry*> {

    typedef typename List<Data>::Entry Entry;

    Entry* lookup(candidate_t id) const {
      auto itr = this->find(id);
      if (itr != this->end()) {
	return itr->second;
      } else {
	return nullptr;
      }
    }

    Entry* allocate(candidate_t id, Data data) {
      auto* entry = new Entry{ data, nullptr, nullptr };
      (*this)[id] = entry;
      return entry;
    }

    Entry* evict(candidate_t id) {
      auto itr = this->find(id);
      assert(itr != this->end());

      auto* entry = itr->second;
      this->erase(itr);
      return entry;
    }
  };

  class LRU : public Policy {
  public:
    void update(candidate_t id, const parser::Request& req) {
      auto* entry = tags.lookup(id);
      if (entry) {
	assert(entry->data == id);
	entry->remove();
      } else {
	entry = tags.allocate(id, id);
      }

      list.insert_front(entry);
    }

    void replaced(candidate_t id) {
      auto* entry = tags.evict(id);
      entry->remove();
      delete entry;
    }

    candidate_t rank(const parser::Request& req) {
      return list.back();
    }

  private:
    List<candidate_t> list;
    Tags<candidate_t> tags;
    typedef typename List<candidate_t>::Entry Entry;
  };

} // namespace repl
