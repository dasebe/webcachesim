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

    void reinit(IdType id, uint64_t size)
    {
        _id = id;
        _size = size;
    }


    // Print request to stdout
    void print() const
    {
        std::cout << "id" << getId() << " size " << getSize() << std::endl;
    }

    // Get request object id
    IdType getId() const
    {
        return _id;
    }

    // Get request size in bytes
    uint64_t getSize() const
    {
        return _size;
    }
};


#endif /* REQUEST_H */



