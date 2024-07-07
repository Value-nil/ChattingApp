#include <stddef.h>
#include <stdint.h>
#include <sys/stat.h>
#include "daemonConstants.h"

const uint16_t SENDING_PORT = 57050;
const uint16_t LISTENING_PORT = 57051;
const size_t SIZE_MULTICAST = sizeof(bool)+sizeof(char)*11;
const size_t MAX_SIZE = sizeof(short)*2+sizeof(char)*123;
const mode_t ACCESS_MODE = S_IRWXU;