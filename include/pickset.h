//
// Created by zhenyus on 11/8/18.
//

#ifndef WEBCACHESIM_PICKSET_H
#define WEBCACHESIM_PICKSET_H

#include <vector>
#include <random>
#include <cstddef>
#include <functional>
#include <unordered_set>

template <typename T, typename H = std::hash<T> >
struct Hasher
{
    std::size_t operator()(const T* const ptr) const
    {
        return H()(*ptr);
    }
};

template <class T, typename H = std::hash<T>>
struct EqualTo
{
    bool operator() (const T* lhs, const T* rhs) const
    {
        return *lhs == *rhs;
    }
};

template <typename T, typename H = std::hash<T> >
struct PickSet
{
    void insert(const T& t)
    {
        if (_unorderedSet.find(&t) == _unorderedSet.cend())
        {
            _vector.push_back(t);

            if (_base == &(*(_vector.begin())))
            {
                _unorderedSet.insert(&(*(_vector.crbegin())));
            }
            else
            {
                _unorderedSet.clear();
                _unorderedSet.reserve(_vector.capacity());
                for (const T& t : _vector) _unorderedSet.insert(&t);
                _base = &(*(_vector.begin()));
            }
        }
    }

    void erase(const T& t)
    {
        auto fit = _unorderedSet.find(&t);

        if (fit != _unorderedSet.end())
        {
            if (*fit != &(*(_vector.crbegin())))
            {
                *(const_cast<T*>(*fit)) = *(_vector.rbegin());
                _unorderedSet.erase(*fit);
                _unorderedSet.erase(&(*(_vector.rbegin())));
                _unorderedSet.insert(*fit);
            }
            else
            {
                _unorderedSet.erase(*fit);
            }

            _vector.pop_back();
        }
    }

    T& pickRandom()
    {
        return _vector[_distribution(_generator) % _vector.size()];
    }

    inline bool exist(const T& t) { return (_unorderedSet.find(&t) != _unorderedSet.cend()); }

    std::unordered_set<const T*, Hasher<T, H>, EqualTo<T, H> > _unorderedSet;
    std::vector<T> _vector;
    T* _base = NULL;

    static std::default_random_engine _generator;
    static std::uniform_int_distribution<std::size_t> _distribution;
};

template <typename T, typename H>
std::default_random_engine PickSet<T, H>::_generator = std::default_random_engine();

template <typename T, typename H>
std::uniform_int_distribution<std::size_t> PickSet<T, H>::_distribution = std::uniform_int_distribution<std::size_t>();



#endif //WEBCACHESIM_PICKSET_H
