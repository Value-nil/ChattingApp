#ifndef UDP
#define UDP
#include "all.h"

void sendIpMulticast(int sock, const char* id, bool isFirstMessage);
void bindSocket(int socket, bool isReceiving, bool);
pollfd newudpSocket();
void processMulticastMessage(void* message, sockaddr_in* address, pollVec &toRead, addrToFd &addressToFd, chToInt &remote, chVec localIDs, chToInt fifos, chVec &remoteIDs);
pollfd newtcpSocket(bool isReceiving);

#endif