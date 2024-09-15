#ifndef FIFO_UTILS
#define FIFO_UTILS

#include <unistd.h>

void createUserFifos(uid_t userId);
void restartUserFifos(uid_t userId);




#endif
