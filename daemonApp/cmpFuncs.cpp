#include <string.h>
#include <netinet/in.h>
#include "types.h"


bool cmp_str::operator()(const char* chr1, const char* chr2)const{
    return strcmp(chr1, chr2) < 0;
}

bool cmp_addr::operator()(const sockaddr_in addr1, const sockaddr_in addr2)const{
    if(addr1.sin_addr.s_addr == addr2.sin_addr.s_addr){
        return addr1.sin_port < addr2.sin_port;
    }
    return addr1.sin_addr.s_addr < addr2.sin_addr.s_addr;
}