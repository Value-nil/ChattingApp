#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <new>
#include <iostream>
#include <string.h>
#include <unistd.h>

#include "../common/utilities.h"
#include "udp.h"
#include "types.h"
#include "socketUtils.h"
#include "daemonConstants.h"

extern chVec localIDs;
extern pollVec toRead;
extern addrToFd addressToFd;
extern chToInt remoteUsers;
extern chToInt localUsers;
extern chVec remoteIDs;



void sendUdpMulticastMessage(int sock, void* message){
    sockaddr_in* multicastAddress = newMulticastSendingAddress();

    int success1 = connect(sock, (sockaddr*)multicastAddress, sizeof(sockaddr_in));
    handleError(success1);

    int success2 = write(sock, message, SIZE_MULTICAST);
    operator delete(message);
    handleError(success2);

    resetPeer(sock);
}


void* newMulticastMessage(const char* id, bool isNewDevice){
    void* message = operator new(SIZE_MULTICAST);
    void* toReturn = message;

    std::cout << strlen(id) << '\n';

    *(bool*)message = isNewDevice;
    message = (bool*)message + 1;
    strcpy((char*)message, id);

    return toReturn;
}



void sendNewUserNotification(int sock, const char* id, bool isFirstMessage){
    void* message = newMulticastMessage(id, isFirstMessage);
    sendUdpMulticastMessage(sock, message);
}

void sendLocalContacts(int udpSock, sockaddr_in* address){
    void* message = operator new(SIZE_MULTICAST);
    void* toSend = message;

    *(bool*)message = false;
    message = (bool*)message + 1;
    
    int success1 = connect(udpSock, (sockaddr*)address, sizeof(sockaddr_in));
    handleError(success1);
    

    for(int i = 0; i < localIDs.size(); i++){
        strcpy((char*)message, localIDs[i]);
        int writingSuccess = write(udpSock, toSend, SIZE_MULTICAST);
        handleError(writingSuccess);
    }

    resetPeer(udpSock);

    operator delete(toSend);
}

void sendNewContactToLocals(const char* peerId){

    void* message = operator new(sizeof(short) + sizeof(char)*11);
    void* toSend = message;

    *(short*)message = 2;
    message = (short*)message + 1;
    strcpy((char*)message, peerId);

    for(int i = 0; i < localIDs.size(); i++){
        int fd = localUsers[localIDs[i]];
        if(fd != 0){
            int success = write(fd, toSend, sizeof(short) + sizeof(char)*11);
            handleError(success);
        } 

        std::cout << "File descriptor for FIFO is: " << fd << '\n';
        std::cout << "The ID it is reffering to: " << localIDs[i] << '\n';
    }

    operator delete(toSend);
}

void connectToPeerListeningTcp(int fd, sockaddr_in* address){
    address->sin_port = htons(LISTENING_PORT); // connect to the listening socket of the peer

    int success = connect(fd, (sockaddr*)address, sizeof(sockaddr_in));
    handleError(success);

    address->sin_port = htons(SENDING_PORT); //revert port changes
}

void makeNewDeviceConnection(sockaddr_in* address, int udpSocket){
    //each device has its own socket, so a new socket is needed for every device(address)
    pollfd newtcp = newConnectingTcpSocket();
    connectToPeerListeningTcp(newtcp.fd, address);

    addressToFd[address->sin_addr] = newtcp.fd;

    toRead.push_back(newtcp);

    sendLocalContacts(udpSocket, address);

    std::cout << "New TCP socket created with fd: " << newtcp.fd << '\n';
}

void processUdpMessage(void* message, sockaddr_in* address){
    std::cout << "New contact on subnet!\n"; 
    int udpSocket = toRead[1].fd;

    bool isNewDevice = *(bool*)message;
    message = (bool*)message + 1;

    char* peerId = new char[11];
    strcpy(peerId, (const char*)message);


    if(isNewDevice){
        std::cout << "New device on subnet\n";
        makeNewDeviceConnection(address, udpSocket);
    }

    remoteUsers[peerId] = addressToFd[address->sin_addr];
    remoteIDs.push_back(peerId);

    std::cout << "The fd for id " << peerId << " is: " << remoteUsers[peerId] << '\n';

    sendNewContactToLocals(peerId);
}