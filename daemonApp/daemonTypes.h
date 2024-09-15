#ifndef DAEMON_TYPES
#define DAEMON_TYPES

#include <netinet/in.h>
#include <map>
#include <vector>
#include <poll.h>

#include "../common/types.h"




typedef std::map<deviceid_t, int> idToFd;
typedef std::vector<deviceid_t> idVec;
typedef std::vector<pollfd> pollVec;
typedef std::map<in_addr, int> addrToFd;






#endif
