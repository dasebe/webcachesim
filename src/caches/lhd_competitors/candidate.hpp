#pragma once

#include <iostream>
#include <cassert>
#include <unordered_map>
#include "parser.hpp"

//#include "growing_vector.hpp"

namespace repl_competitors {

struct candidate_t {
  int64_t size;
  int64_t id;

  static candidate_t make(const parser_competitors::Request& req) {
    return candidate_t{req.size(), req.id};
  }

  inline bool operator==(const candidate_t& that) const { 
    return (id == that.id) && (size == that.size); 
  }

  inline bool operator!=(const candidate_t& that) const { 
    return !(operator==(that)); 
  }

  inline bool operator<(const candidate_t& that) const {
    if (this->size == that.size) {
      return this->id < that.id;
    } else {
      return this->size < that.size;
    }
  }
};

const candidate_t INVALID_CANDIDATE{-1, -1};

template <typename T>
class CandidateMap : public std::unordered_map<candidate_t, T> {
public:
  typedef std::unordered_map<candidate_t, T> Base;
  const T DEFAULT;

  CandidateMap(const T& _DEFAULT)
    : Base()
    , DEFAULT(_DEFAULT) {}

  using typename Base::reference;
  using typename Base::const_reference;

  T& operator[] (candidate_t c) { 
    auto itr = Base::find(c);
    if (itr == Base::end()) { 
      auto ret = Base::insert({c, DEFAULT});
      assert(ret.second);
      return ret.first->second;
    }
    else {return itr->second;} 
  }

  const T& operator[] (candidate_t c) const { 
    auto itr = Base::find(c);
    if (itr == Base::end()) { 
      return DEFAULT;
    }
    else {return itr->second;} 
  }
};

}

// candidate_t specializations
namespace std {

  template <>
  struct hash<repl_competitors::candidate_t> {
    size_t operator() (const repl_competitors::candidate_t& x) const {
      return x.id;
    }
  };

}

namespace {

  inline std::ostream& operator<< (std::ostream& os, const repl_competitors::candidate_t& x) {
    return os << "(" << x.size << ", " << x.id << ")";
  }

}
