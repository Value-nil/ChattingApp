#ifndef SOCK_UTILS
#define SOCK_UTILS

#include <poll.h>
#include <netinet/in.h>

pollfd newListeningTcpSocket();
pollfd newConnectingTcpSocket();
void resetPeer(int sock);
pollfd newudpSocket();
sockaddr_in* newMulticastSendingAddress();

extern const char* MULTICASTIP;






#endif