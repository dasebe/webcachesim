#ifndef REQUEST_H
#define REQUEST_H

#include <cstdint>
#include <iostream>

typedef uint64_t IdType;

// Request information
class SimpleRequest
{
private:
    IdType _id; // request object id
    uint64_t _size; // request size in bytes

public:
    SimpleRequest()
    {
    }
    virtual ~SimpleRequest()
    {
    }

    // Create request
    SimpleRequest(IdType id, uint64_t size)
        : _id(id),
          _size(size)
    {
    }

    inline void reinit(IdType id, uint64_t size) {
        _id = id;
        _size = size;
    }


    // Print request to stdout
    void print() const
    {
        std::cout << "id" << getId() << " size " << getSize() << std::endl;
    }

    // Get request object id
    inline IdType getId() const {
        return _id;
    }

    // Get request size in bytes
    inline uint64_t getSize() const {
        return _size;
    }
};


class AnnotatedRequest: public SimpleRequest
{
private:
    u_int64_t _next_t;

public:
    // Create request
    AnnotatedRequest(IdType id, uint64_t size, uint64_t next_t)
            : SimpleRequest(id, size),
              _next_t(next_t)
    {
    }

    inline void reinit(IdType id, uint64_t size, uint64_t next_t) {
        SimpleRequest::reinit(id, size);
        _next_t = next_t;
    }
    // Get request size in bytes
    inline uint64_t get_next_t() const {
        return _next_t;
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



