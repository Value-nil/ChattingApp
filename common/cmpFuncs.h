#ifndef COMPARE_FUNCTIONS
#define COMPARE_FUNCTIONS

#include <netinet/in.h>

struct cmp_str{
    bool operator()(const char* chr1, const char* chr2) const;
};

struct cmp_addr{
    bool operator()(const in_addr addr1, const in_addr addr2) const;
};


#endif