#ifndef FIFO_UTILITES
#define FIFO_UTILITES
#include <errno.h>
#include <iostream>
extern const size_t MAX_SIZE;
extern const char* ID_PATH;

extern const char* A_TO_D_PATH;
extern const char* D_TO_A_PATH;
extern const char* DIRECTORY_PATH;
extern const char* MESSAGES_PATH;
extern const uint16_t SENDING_PORT;
extern const uint16_t LISTENING_PORT;
extern const size_t SIZE_MULTICAST;
void handleError(int returnInt);
const char* getFifoDir(struct passwd* user, const char* fifoDir);
#endif