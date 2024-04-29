#include <stddef.h>
#include "fifoUtilities.h"
const size_t MAX_SIZE = sizeof(short)*2+sizeof(char)*123;
const char* ID_PATH = "/id.txt";

const char* D_TO_A_PATH = "/dToAFifo";
const char* A_TO_D_PATH = "/aToDFifo";
const char* DIRECTORY_PATH = "/chattingApp";
const char* MESSAGES_PATH = "/messages";
const uint16_t SENDING_PORT = 57050;
const uint16_t LISTENING_PORT = 57051;
const size_t SIZE_MULTICAST = sizeof(bool)+sizeof(char)*11;