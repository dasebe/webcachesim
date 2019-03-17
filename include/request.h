#ifndef REQUEST_H
#define REQUEST_H

#include <cstdint>
#include <iostream>
#include <vector>

using namespace std;

typedef uint64_t IdType;

// Request information
class SimpleRequest
{
public:
    IdType _id; // request object id
    uint64_t _size; // request size in bytes
    u_int64_t _t;
    vector<uint64_t > _extra_features;

    SimpleRequest()
    {
    }
    virtual ~SimpleRequest()
    {
    }

    // Create request
    SimpleRequest(IdType id, uint64_t size)
        : _id(id), _size(size) {
    }

    SimpleRequest(IdType id, uint64_t size, uint64_t t, vector<uint64_t >* extra_features = nullptr)
        : _id(id), _size(size), _t(t) {
        if (extra_features)
            _extra_features = *extra_features;
    };

    inline void reinit(IdType id, uint64_t size) {
        _id = id;
        _size = size;
    }

    inline void reinit(IdType id, uint64_t size, uint64_t t, vector<uint64_t >* extra_features = nullptr) {
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
    inline IdType get_id() const {
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
    AnnotatedRequest(IdType id, uint64_t size, uint64_t t, uint64_t next_seq,
            vector<uint64_t >* extra_features = nullptr)
            : SimpleRequest(id, size),
              _next_seq(next_seq) {
        _t = t;
        if (extra_features)
            _extra_features = *extra_features;
    }

    inline void reinit(IdType id, uint64_t size, uint64_t t, uint64_t next_seq,
                       vector<uint64_t >* extra_features = nullptr) {
        SimpleRequest::reinit(id, size);
        _t = t;
        _next_seq = next_seq;
        if (extra_features)
            _extra_features = *extra_features;
    }
};


class ClassifiedRequest: public SimpleRequest
{
public:
    double rehit_probability;

    // Create request
    ClassifiedRequest(IdType id, uint64_t size, double _rehit_probability)
            : SimpleRequest(id, size),
              rehit_probability(_rehit_probability)
    {
    }

    inline void reinit(IdType id, uint64_t size, double _rehit_probability) {
        SimpleRequest::reinit(id, size);
        rehit_probability = _rehit_probability;
    }
};


#endif /* REQUEST_H */



