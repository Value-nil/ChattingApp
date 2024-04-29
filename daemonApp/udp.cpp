#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <new>

#include "udp.h"
#include "all.h"
#include "../common/fifoUtilities.h"

const char* MULTICASTIP = "239.255.139.53";

void resetPeer(int sock){
    sockaddr* unspec = new sockaddr;
    unspec->sa_family = AF_UNSPEC;

    int success = connect(sock, unspec, sizeof(sockaddr));
    handleError(success);
}



void setMulticast(int socket){
    ip_mreqn *multicastAddress = new ip_mreqn;
    multicastAddress->imr_multiaddr.s_addr = inet_addr(MULTICASTIP);
    multicastAddress->imr_address.s_addr = htonl(INADDR_ANY);
    multicastAddress->imr_ifindex = 0;

    int success = setsockopt(socket, IPPROTO_IP, IP_ADD_MEMBERSHIP, multicastAddress, sizeof(*multicastAddress));
    handleError(success);
}


void sendUdpMulticast(int sock, void* message){
    sockaddr_in* multicastAddress = new sockaddr_in;
    multicastAddress->sin_family = AF_INET;
    multicastAddress->sin_addr.s_addr = inet_addr(MULTICASTIP);
    multicastAddress->sin_port = htons(SENDING_PORT);

    int success1 = connect(sock, (sockaddr*)multicastAddress, sizeof(sockaddr_in));
    handleError(success1);

    std::cout << "Came to writing" << '\n';
    int success2 = write(sock, message, SIZE_MULTICAST);
    operator delete(message);
    handleError(success2);



    resetPeer(sock);
}
void* newMulticastMessage(const char* id, bool isNewDevice){
    void* message = operator new(SIZE_MULTICAST);
    void* toReturn = message;
    *(bool*)message = isNewDevice;
    message = (bool*)message + 1;
    strcpy((char*)message, id);
    return toReturn;
}

pollfd newudpSocket(){
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    handleError(sock);

    setMulticast(sock);
    bindSocket(sock, false, true);

    
    

    pollfd toReturn;
    toReturn.fd = sock;
    toReturn.events = POLLIN;

    return toReturn;
}

void sendIpMulticast(int sock, const char* id, bool isFirstMessage){
    void* message = newMulticastMessage(id, isFirstMessage);
    sendUdpMulticast(sock, message);
}

void sendTargetedUdp(int udpSock, sockaddr_in* address, chVec localIDs){
    void* message = operator new(SIZE_MULTICAST);
    void* toSend = message;
    *(bool*)message = false;
    message = (bool*)message + 1;
    
    address->sin_port = htons(SENDING_PORT);
    int success1 = connect(udpSock, (sockaddr*)address, sizeof(sockaddr_in));
    handleError(success1);
    

    for(int i = 0; i < localIDs.size(); i++){
        strcpy((char*)message, localIDs[i]);
        int writingSuccess = write(udpSock, toSend, SIZE_MULTICAST);
        handleError(writingSuccess);
    }
    address->sin_port = htons(LISTENING_PORT);

    resetPeer(udpSock);

    operator delete(toSend);
}

void sendNewContactToLocal(chVec localIDs, chToInt fifos, const char* peerId){

    void* message = operator new(sizeof(short) + sizeof(char)*11);
    void* toSend = message;

    *(short*)message = 2;
    message = (short*)message + 1;
    strcpy((char*)message, peerId);

    for(int i = 0; i < localIDs.size(); i++){
        int fd = fifos[localIDs[i]];
        std::cout << "File descriptor for FIFO is: " << fd << '\n';
        std::cout << "The ID it is reffering to: " << localIDs[i] << '\n';

        if(fd != 0){
            int success = write(fd, toSend, sizeof(short) + sizeof(char)*11);
            handleError(success);
        } 
    }

    operator delete(toSend);
}

void processMulticastMessage(void* message, sockaddr_in* address, pollVec &toRead, addrToFd &addressToFd, chToInt &remote, chVec localIDs, chToInt fifos, chVec &remoteIDs){
    int udpSocket = toRead[1].fd;
    bool isNewDevice = *(bool*)message;
    message = (bool*)message + 1;
    char* peerId = new char[11];

    strcpy(peerId, (const char*)message);

    address->sin_port = htons(LISTENING_PORT);

    std::cout << "Multicast processing\n";
    if(isNewDevice){
        pollfd newtcp = newtcpSocket(false);
        toRead.push_back(newtcp);

        int success = connect(newtcp.fd, (sockaddr*)address, sizeof(sockaddr_in));
        handleError(success);
        addressToFd[*address] = newtcp.fd;

        sendTargetedUdp(udpSocket, address, localIDs);

        std::cout << "Is new device!\n";
    }

    //set id to fd
    remote[peerId] = addressToFd[*address];
    remoteIDs.push_back(peerId);

    sendNewContactToLocal(localIDs, fifos, peerId);
}