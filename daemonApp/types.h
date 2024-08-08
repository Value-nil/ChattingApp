#ifndef TYPES
#define TYPES

#include <netinet/in.h>
#include <map>
#include <vector>
#include <poll.h>


struct cmp_str{
    bool operator()(const char* chr1, const char* chr2) const;
};

struct cmp_addr{
    bool operator()(const in_addr addr1, const in_addr addr2) const;
};

typedef std::map<const char*, int, cmp_str> chToInt;
typedef std::vector<const char*> chVec;
typedef std::vector<pollfd> pollVec;
typedef std::map<in_addr, int, cmp_addr> addrToFd;






#endif