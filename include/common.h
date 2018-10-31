//
// Created by Zhenyu Song on 10/31/18.
//

#ifndef WEBCACHESIM_COMMON_H
#define WEBCACHESIM_COMMON_H

// uncomment to enable cache debugging:
// #define CDEBUG 1

// util for debug
#ifdef CDEBUG
inline void logMessage(std::string m, double x, double y, double z) {
    std::cerr << m << "," << x << "," << y  << "," << z << "\n";
}
#define LOG(m,x,y,z) logMessage(m,x,y,z)
#else
#define LOG(m,x,y,z)
#endif




#endif //WEBCACHESIM_COMMON_H
