#ifndef REQUEST_H
#define REQUEST_H

#include <cstdint>
#include <iostream>
#include <vector>

using namespace std;

typedef uint64_t KeyT;
typedef uint64_t SizeT;

// Request information
class SimpleRequest
{
public:
    KeyT _id; // request object id
    uint64_t _size; // request size in bytes
    uint64_t _t;
    //category feature. unsigned int. ideally not exceed 2k
    vector<uint16_t > _extra_features;

    SimpleRequest()
    {
    }
    virtual ~SimpleRequest()
    {
    }

    // Create request
    SimpleRequest(KeyT id, uint64_t size)
        : _id(id), _size(size) {
    }

    SimpleRequest(KeyT id, uint64_t size, uint64_t t, vector<uint16_t> *extra_features = nullptr)
        : _id(id), _size(size), _t(t) {
        if (extra_features)
            _extra_features = *extra_features;
    };

    inline void reinit(KeyT id, uint64_t size) {
        _id = id;
        _size = size;
    }

    inline void reinit(KeyT id, uint64_t size, uint64_t t, vector<uint16_t> *extra_features = nullptr) {
        _id = id;
        _size = size;
        _t = t;
        if (extra_features)
            _extra_features = *extra_features;
    }

    // Print request to stdout
    void print() const
    {
        std::cout << "id" << get_id() << " size " << get_size() << std::endl;
    }

    // Get request object id
    inline KeyT get_id() const {
        return _id;
    }

    // Get request size in bytes
    inline uint64_t get_size() const {
        return _size;
    }
};


class AnnotatedRequest: public SimpleRequest
{
public:
    u_int64_t _next_seq;

    // Create request
    AnnotatedRequest(KeyT id, uint64_t size, uint64_t t, uint64_t next_seq,
                     vector<uint16_t >* extra_features = nullptr)
            : SimpleRequest(id, size),
              _next_seq(next_seq) {
        _t = t;
        if (extra_features)
            _extra_features = *extra_features;
    }

    inline void reinit(KeyT id, uint64_t size, uint64_t t, uint64_t next_seq,
                       vector<uint16_t >* extra_features = nullptr) {
        SimpleRequest::reinit(id, size);
        _t = t;
        _next_seq = next_seq;
        if (extra_features)
            _extra_features = *extra_features;
    }
};


#endif /* REQUEST_H */



