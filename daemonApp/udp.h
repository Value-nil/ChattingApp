#ifndef UDP
#define UDP
#include "daemonTypes.h"

void sendNewDeviceNotification(int sock);
void processUdpMessage(deviceid_t peerDeviceId, sockaddr_in* address);
void sendLocalContacts(int socket);
#endif
