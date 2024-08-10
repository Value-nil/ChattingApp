#ifndef FIFO_UTILS
#define FIFO_UTILS

#include <unistd.h>

void createUserFifos(const char* id, uid_t userId);
void restartUserFifos(const char* id, uid_t userId);




#endif