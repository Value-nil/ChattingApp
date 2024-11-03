#include "socketUtils.h"
#include "../common/utilities.h"
#include "daemonConstants.h"

#include <netinet/in.h>
#include <sys/socket.h>
#include <poll.h>
#include <arpa/inet.h>



const char* MULTICASTIP = "239.255.139.53";


static inline sockaddr_in* getOwnAddress(bool isReceiving){
    sockaddr_in *fullAddress = new sockaddr_in;
    fullAddress->sin_addr.s_addr = htonl(INADDR_ANY);
    fullAddress->sin_family = AF_INET;
    fullAddress->sin_port = htons(isReceiving? LISTENING_PORT : SENDING_PORT);
    
    return fullAddress;
}




static void bindSocket(int socket, bool isReceiving, bool isudp){
    sockaddr_in* fullAddress = getOwnAddress(isReceiving);
    if(!isReceiving && !isudp){
        int True = 1;
        int success2 = setsockopt(socket, SOL_SOCKET, SO_REUSEADDR, (void*)&True, sizeof(int));
        handleError(success2);

        int success = setsockopt(socket, SOL_SOCKET, SO_REUSEPORT, (void*)&True, sizeof(int));
        handleError(success);
    }
    int success1 = bind(socket, (const sockaddr*)fullAddress, sizeof(sockaddr_in));
    handleError(success1);
}






static pollfd newtcpSocket(bool isReceiving){
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    handleError(sock);

    bindSocket(sock, isReceiving, false);

    if(isReceiving){
        int listeningSuccess = listen(sock, 0);
        handleError(listeningSuccess);
    }

    pollfd toReturn;
    toReturn.fd = sock;
    toReturn.events = POLLIN;

    return toReturn;
}


pollfd newListeningTcpSocket(){
    return newtcpSocket(true);
}

pollfd newConnectingTcpSocket(){
    return newtcpSocket(false);
}




void resetPeer(int sock){ // to allow socket to receive from all peer sockets
    sockaddr* unspec = new sockaddr;
    unspec->sa_family = AF_UNSPEC;

    int success = connect(sock, unspec, sizeof(sockaddr));
    handleError(success);
}



static inline void setMulticast(int socket){
    ip_mreqn *multicastAddress = new ip_mreqn;
    multicastAddress->imr_multiaddr.s_addr = inet_addr(MULTICASTIP);
    multicastAddress->imr_address.s_addr = htonl(INADDR_ANY);
    multicastAddress->imr_ifindex = 0;

    int success = setsockopt(socket, IPPROTO_IP, IP_ADD_MEMBERSHIP, multicastAddress, sizeof(*multicastAddress));
    handleError(success);
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


sockaddr_in* newMulticastSendingAddress(){
    sockaddr_in* multicastAddress = new sockaddr_in;
    multicastAddress->sin_family = AF_INET;
    multicastAddress->sin_addr.s_addr = inet_addr(MULTICASTIP);
    multicastAddress->sin_port = htons(SENDING_PORT);

    return multicastAddress;
}
