#pragma once
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <unistd.h>

namespace parser_competitors {

    typedef float float32_t;

    struct Request {
        int32_t appId;
        int64_t valueSize;
        int64_t id;

        inline int64_t size() const { return valueSize; }
    } __attribute__((packed));


}
