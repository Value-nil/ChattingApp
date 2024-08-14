#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <new>
#include <iostream>
#include <string.h>
#include <unistd.h>

#include "../common/utilities.h"
#include "udp.h"
#include "daemonTypes.h"
#include "socketUtils.h"
#include "daemonConstants.h"

extern chVec localIDs;
extern pollVec toRead;
extern addrToFd addressToFd;
extern chToInt remoteUsers;
extern chToInt localUsers;
extern chVec remoteIDs;



void sendUdpMulticastMessage(int sock){
    sockaddr_in* multicastAddress = newMulticastSendingAddress();

    int success1 = connect(sock, (sockaddr*)multicastAddress, sizeof(sockaddr_in));
    handleError(success1);

    int success2 = write(sock, nullptr, 0);
    handleError(success2);

    resetPeer(sock);
}

void sendNewDeviceNotification(int sock){
    sendUdpMulticastMessage(sock);
}

void sendLocalContacts(int socket){
    size_t sizeOfMsg = sizeof(short)*2 + sizeof(char)*11;

    void* message = operator new(sizeof(size_t) + sizeOfMsg);
    void* toSend = message;

    *(size_t*)message = sizeOfMsg;
    message = (size_t*)message + 1;
    *(short*)message = 1;
    message = (short*)message + 1;
    *(short*)message = 3;
    message = (short*)message + 1;

    for(int i = 0; i < localIDs.size(); i++){
        strcpy((char*)message, localIDs[i]);
        int writingSuccess = write(socket, toSend, sizeof(size_t)+ sizeOfMsg);
        handleError(writingSuccess);
    }

    operator delete(toSend);
}



void connectToPeerListeningTcp(int fd, sockaddr_in* address){
    address->sin_port = htons(LISTENING_PORT); // connect to the listening socket of the peer

    int success = connect(fd, (sockaddr*)address, sizeof(sockaddr_in));
    handleError(success);

    address->sin_port = htons(SENDING_PORT); //revert port changes
}

void makeNewDeviceConnection(sockaddr_in* address){
    //each device has its own socket, so a new socket is needed for every device(address)
    pollfd newtcp = newConnectingTcpSocket();
    connectToPeerListeningTcp(newtcp.fd, address);

    addressToFd[address->sin_addr] = newtcp.fd;

    toRead.push_back(newtcp);

    sendLocalContacts(newtcp.fd);

    std::cout << "New TCP socket created with fd: " << newtcp.fd << '\n';
}

void processUdpMessage(sockaddr_in* address){
    std::cout << "New device on subnet\n";
    makeNewDeviceConnection(address);
}