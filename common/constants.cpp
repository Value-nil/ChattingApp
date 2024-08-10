#include <stddef.h>
#include "constants.h"

const char* ID_PATH = "/id.txt";
const char* D_TO_A_PATH = "/d_to_a_fifo";//daemon to app fifo
const char* A_TO_D_PATH = "/a_to_d_fifo";//app to daemon fifo
const char* INITIAL_DIR_PATH = "/.chatting_app";
const char* MESSAGES_PATH = "/messages";
const size_t MAX_SIZE = sizeof(short)*2+sizeof(char)*123;
const char* FIFO_PATH = "/tmp/chattingApp/";

