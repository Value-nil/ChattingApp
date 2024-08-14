#ifndef UDP
#define UDP
#include "daemonTypes.h"

void sendNewDeviceNotification(int sock);
void processUdpMessage(sockaddr_in* address);
void sendLocalContacts(int socket);
#endif