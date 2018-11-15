#ifndef CACHE_HASH_H
#define CACHE_HASH_H

#include "utils.h"
#include "request.h"

// CacheObject is used by caching policies to store a representation of an "object, i.e., the object's id and its size
struct CacheObject
{
    IdType id;
    uint64_t size;

    CacheObject(SimpleRequest &req)
        : id(req.get_id()),
          size(req.get_size())
    {}

    // comparison is based on all three properties
    bool operator==(const CacheObject &rhs) const {
        return (rhs.id == id) && (rhs.size == size);
    }
};


// definition of a hash function on CacheObjects
// required to use unordered_map<CacheObject, >
namespace std
{
    template<> struct hash<CacheObject>
    {
        inline size_t operator()(const CacheObject cobj) const
        {
            size_t seed = 0;
            hash_combine<IdType>(seed, cobj.id);
            hash_combine<uint64_t>(seed, cobj.size);
            return seed;
        }
    };
}
#endif /* CACHE_HASH_H */
