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

extern pollVec toRead;
extern idToFd remoteDevices;
extern idToFd localUsers;
extern deviceid_t deviceId;


void sendNewDeviceNotification(int sock){//sends a multicast message
    sockaddr_in* multicastAddress = newMulticastSendingAddress();

    int success1 = connect(sock, (sockaddr*)multicastAddress, sizeof(sockaddr_in));
    handleError(success1);

    int success2 = write(sock, &deviceId, sizeof(deviceid_t));
    handleError(success2);

    resetPeer(sock);
}

void sendLocalContacts(int socket, deviceid_t idToSend){
    size_t sizeOfMsg = sizeof(short)*2 + sizeof(deviceid_t)*2;

    void* message = operator new(sizeof(size_t) + sizeOfMsg);
    void* toSend = message;

    *(size_t*)message = sizeOfMsg;
    message = (size_t*)message + 1;
    *(short*)message = 1;
    message = (short*)message + 1;
    *(short*)message = 3;
    message = (short*)message + 1;
    *(deviceid_t*)message = idToSend;
    message = (deviceid_t*)message + 1;

    for(auto iter = localUsers.begin(); iter != localUsers.end(); iter++){
	*(deviceid_t*)message = deviceId | (deviceid_t)((*iter).first);

        int writingSuccess = write(socket, toSend, sizeof(size_t)+ sizeOfMsg);
        handleError(writingSuccess);
    }

    operator delete(toSend);
}



static inline void connectToPeerListeningTcp(int fd, sockaddr_in* address){
    address->sin_port = htons(LISTENING_PORT); // connect to the listening socket of the peer

    int success = connect(fd, (sockaddr*)address, sizeof(sockaddr_in));
    handleError(success);

    address->sin_port = htons(SENDING_PORT); //revert port changes
}

static inline void sendDeviceIdToPeer(int fd){
    size_t sizeOfMsg = sizeof(short)*2 + sizeof(deviceid_t);
    void* message = operator new(sizeof(size_t) + sizeOfMsg);
    void* toSend = message;

    *(size_t*)message = sizeOfMsg;
    message = (size_t*)message + 1;
    *(short*)message = 1;
    message = (short*)message + 1;
    *(short*)message = 5;
    message = (short*)message + 1;
    *(deviceid_t*)message = deviceId;

    int success = write(fd, toSend, sizeof(size_t)+sizeOfMsg);
    handleError(success);
}


static inline void makeNewDeviceConnection(deviceid_t peerDeviceId, sockaddr_in* address){
    //each device has its own socket, so a new socket is needed for every device(address)
    pollfd newtcp = newConnectingTcpSocket();
    connectToPeerListeningTcp(newtcp.fd, address);
    sendDeviceIdToPeer(newtcp.fd);

    remoteDevices[peerDeviceId] = newtcp.fd;
    toRead.push_back(newtcp);

    sendLocalContacts(newtcp.fd, 0);

    std::cout << "New TCP socket created with id: " << peerDeviceId << " and fd " << newtcp.fd << '\n';
}

void processUdpMessage(deviceid_t peerDeviceId, sockaddr_in* address){
    std::cout << "New device on subnet\n";
    makeNewDeviceConnection(peerDeviceId, address);
}
