#include <stddef.h>
#include "constants.h"

const char* ID_PATH = "/chattingappid"; //path to store the device id in
const char* ID_DIRECTORY_PATH = "/var/local/lib/misc"; //the directory the id file is stored in
const char* INITIAL_DIR_PATH = "/.chatting_app";
const char* MESSAGES_PATH = "/messages/";
const char* FIFO_PATH = "/tmp/chattingApp/";
const int messageLimit = 1000; 
const size_t metadataSize = sizeof(bool) + sizeof(time_t) + sizeof(int);
