#ifndef CHATTING_APP_ALL
#define CHATTING_APP_ALL

#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <iostream>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <vector>
#include <sys/random.h>
#include <new>
#include <poll.h>
#include <pwd.h>
#include <map>

struct cmp_str{
    bool operator()(const char* chr1, const char* chr2) const;
};

struct cmp_addr{
    bool operator()(const sockaddr_in addr1, const sockaddr_in addr2) const;
};

typedef std::map<const char*, int, cmp_str> chToInt;
typedef std::vector<const char*> chVec;
typedef std::vector<pollfd> pollVec;
typedef std::map<sockaddr_in, int, cmp_addr> addrToFd;






#endif