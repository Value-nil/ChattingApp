#ifndef TYPES
#define TYPES

#include <netinet/in.h>
#include <map>
#include <vector>
#include <poll.h>

#include "../common/cmpFuncs.h"




typedef std::map<const char*, int, cmp_str> chToInt;
typedef std::vector<const char*> chVec;
typedef std::vector<pollfd> pollVec;
typedef std::map<in_addr, int, cmp_addr> addrToFd;






#endif