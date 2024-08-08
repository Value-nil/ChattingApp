#include <string.h>
#include <netinet/in.h>
#include "cmpFuncs.h"


bool cmp_str::operator()(const char* chr1, const char* chr2)const{
    return strcmp(chr1, chr2) < 0;
}

bool cmp_addr::operator()(const in_addr addr1, const in_addr addr2)const{
    return addr1.s_addr < addr2.s_addr;
}