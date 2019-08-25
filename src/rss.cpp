//
// Created by zhenyus on 8/24/19.
//

#include "rss.h"
#include <proc/readproc.h>

size_t get_rss() {
    struct proc_t usage;
    look_up_our_self(&usage);
    return usage.rss * getpagesize();
}
