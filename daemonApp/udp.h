#ifndef UDP
#define UDP
#include "types.h"

void sendNewUserNotification(int sock, const char* id, bool isFirstMessage);
void processUdpMessage(void* message, sockaddr_in* address);
#endif