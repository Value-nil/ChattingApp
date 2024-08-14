#include "processMessage.h"
#include "../common/utilities.h"
#include "../common/constants.h"
#include "daemonTypes.h"
#include "daemonConstants.h"
#include "fifoUtils.h"


#include <pwd.h>
#include <unistd.h>
#include <new>
#include <string.h>
#include <vector>
#include <map>
#include <iostream>
#include <fcntl.h>

extern chToInt localUsers;
extern chVec remoteIDs;
extern chVec localIDs;
extern chVec requestedPeers;
extern chToInt remoteUsers;
extern addrToFd addressToFd;


void sendCurrentOnlinePeers(int localFd){
    size_t sizeOfMsg = sizeof(short) + sizeof(char)*11;

    void* message = operator new(sizeof(size_t) + sizeOfMsg);
    void* toSend = message;

    *(size_t*)message = sizeOfMsg;
    message = (size_t*)message + 1;
    *(short*)message = 2;
    message = (short*)message + 1;
    
    for(int i = 0; i < remoteIDs.size(); i++){
        strcpy((char*)message, remoteIDs[i]);
        int success = write(localFd, toSend, sizeof(size_t) + sizeOfMsg);
        handleError(success);
    }
    operator delete(toSend);
}


void initializeUser(const char* id, uid_t userId){
    const char* fifoDir = getFifoPath(id, false);
    int localFd = openFifo(fifoDir, O_WRONLY);

    localUsers[id] = localFd;

    sendCurrentOnlinePeers(localFd);
}

void sendContactRequest(const char* id, const char* peerId, bool isAccepting){
    size_t sizeOfMsg = sizeof(sizeof(short)*2+sizeof(char)*22);

    void* message = operator new(sizeof(size_t) + sizeOfMsg);
    void* toSend = message;

    *(size_t*)message = sizeOfMsg;
    message = (size_t*)message + 1;
    *(short*)message = 1;
    message = (short*)message + 1;
    *(short*)message = isAccepting? 1 : 0;
    message = (short*)message + 1;
    strcpy((char*)message, id);
    message = (char*)message + 11;
    strcpy((char*)message, peerId);

    int remoteFd = remoteUsers[peerId];
    std::cout << "Sending request contact messsage to fd: " << remoteFd << '\n';

    int success = write(remoteFd, toSend, sizeof(size_t) + sizeOfMsg);
    handleError(success);

    operator delete(toSend);
}

void processAcceptContact(const char* peerId, const char* id){
    size_t sizeOfMsg = sizeof(short) + sizeof(char)*22;

    void* message = operator new(sizeof(size_t)+sizeOfMsg);
    void* toSend = message;

    *(size_t*)message = sizeOfMsg;
    message = (size_t*)message + 1;
    *(short*)message = 3;
    message = (short*)message + 1;
    strcpy((char*)message, peerId);

    std::cout << "User accepted contact\n";

    int localFd = localUsers[id];

    int success = write(localFd, toSend, sizeof(size_t) + sizeOfMsg);
    handleError(success);

    operator delete(toSend);
}

void checkRequestingPeers(const char* request, const char* id, const char* peerId){
    bool hasBeenRequested = false;
    for(int i = 0; i < requestedPeers.size(); i++){
        if(strcmp(request, requestedPeers[i]) == 0){
            sendContactRequest(id, peerId, true);
            processAcceptContact(peerId, id);

            requestedPeers.erase(requestedPeers.begin()+i);
            hasBeenRequested = true;
        }
    }
    if(!hasBeenRequested){
        sendContactRequest(id, peerId, false);
    }

}

void sendMessage(char* id, char* peerId, char* actualMessage){
    size_t sizeOfMsg = sizeof(short)*2+sizeof(char)*123;

    void* message = operator new(sizeof(size_t) + sizeOfMsg);
    void* toSend = message;

    *(size_t*)message = sizeOfMsg;
    message = (size_t*)message + 1;
    *(short*)message = 1;
    message = (short*)message + 1;
    *(short*)message = 2;
    message = (short*)message + 1;
    strcpy((char*)message, id);
    message = (char*) message + 11;
    strcpy((char*)message, peerId);
    message = (char*)message + 11;
    strcpy((char*)message, actualMessage);

    int remoteFd = remoteUsers[peerId];

    int success = write(remoteFd, toSend, sizeof(size_t) + sizeOfMsg);
    handleError(success);

    operator delete(toSend);
}

char* buildRequest(const char* id, const char* peerId){
    char* request = new char[11];
    for(int i = 0; i < 10; i++){
        request[i] = (char)((const int)(id[i])^(const int)(peerId[i]));
    }
    request[10] = '\0';

    return request;
}

void processFifo(void* message){
    short method = *(short*)message;
    message = (short*)message + 1;

    char* id = new char[11];
    strcpy(id, (const char*)message);
    message = (char*)message+11;

    if(method == 0){//app has opened
        uid_t userId = *(uid_t*)message;
        
        initializeUser(id, userId);


        std::cout << "App with id: " << id << " has opened\n";
    }
    else if(method == 1){//local user requesting contact
        //xor
        char* peerId = new char[11];
        strcpy(peerId, (const char*)message);

        char* request = buildRequest(id, peerId);

        checkRequestingPeers(request, id, peerId);
        std::cout << "Local user is requesting contact with another peer\n";
    }
    else if(method == 2){//sending message
        char* peerId = new char[11];
        strcpy(peerId, (const char*)message);
        message = (char*)message+11;

        char* actualMessage = new char[101];
        strcpy(actualMessage, (const char*)message);

        sendMessage(id, peerId, actualMessage);
        std::cout << "Local user is sending message!\n";
    }
    else if(method == 3){//app is being closed
        uid_t userId = *(uid_t*)message;

        int success = close(localUsers[id]);
        handleError(success);
        localUsers[id] = 0;

        restartUserFifos(id, userId);
        std::cout << "Local user is closing app!\n";
    }
}

void sendIncomingMessage(int localFd, const char* peerId, const char* actualMessage){
    size_t sizeOfMsg = sizeof(short)+sizeof(char)*112;

    void* message = operator new(sizeof(size_t)+ sizeOfMsg);
    void* toSend = message;

    *(size_t*)message = sizeOfMsg;
    message = (size_t*)message;
    *(short*)message = 1;
    message = (short*)message + 1;
    strcpy((char*)message, peerId);
    message = (char*)message + 11;
    strcpy((char*)message, actualMessage);

    int success = write(localFd, toSend, sizeof(size_t)+ sizeOfMsg);
    handleError(success);

    operator delete(toSend);
}


void processIncomingMessage(void* message){
    const char* peerId = (const char*)message;
    message = (char*)message+11;

    const char* id = (const char*)message;
    message = (char*)message + 11;
    int localFd = localUsers[id];
    if(localFd <= 0) return; //the user is offline

    const char* actualMessage = (const char*)message;

    sendIncomingMessage(localFd, peerId, actualMessage);
}

void processPeerAcceptedContact(void* message){
    const char* peerId = (const char*)message;
    message = (char*)message+11;

    const char* id = (const char*)message;


    int localFd = localUsers[id];
    if(localFd <= 0) return; // the user is offline
    processAcceptContact(peerId, id);

    std::cout << "Peer TCP has accepted contact\n";
}

void processRemoteRequest(void* message){
    const char* peerId = (const char*)message;
    message = (char*)message+11;

    const char* id = (const char*)message;


    char* request = buildRequest(id, peerId);
    //TELL FIFO ABOUT THIS(future)
    requestedPeers.push_back(request);
    std::cout << "Remote TCP is requesting contact!\n";
}

void sendNewContactToLocals(const char* peerId){
    size_t sizeOfMsg = sizeof(short) + sizeof(char)*11;

    void* message = operator new(sizeof(size_t) + sizeOfMsg);
    void* toSend = message;

    *(size_t*)message = sizeOfMsg;
    message = (size_t*)message + 1;
    *(short*)message = 2;
    message = (short*)message + 1;
    strcpy((char*)message, peerId);

    for(int i = 0; i < localIDs.size(); i++){
        int fd = localUsers[localIDs[i]];
        if(fd != 0){
            int success = write(fd, toSend, sizeof(size_t) + sizeOfMsg);
            handleError(success);
        } 

        std::cout << "File descriptor for FIFO is: " << fd << '\n';
        std::cout << "The ID it is reffering to: " << localIDs[i] << '\n';
    }

    operator delete(toSend);
}

void addNewRemoteContact(void* message, int fd){
    sockaddr_in* address = new sockaddr_in;
    socklen_t size = sizeof(sockaddr_in);

    int peerNameSuccess = getpeername(fd, (sockaddr*)address, &size);
    handleError(peerNameSuccess);

    const char* peerId = (const char*)message;

    remoteUsers[peerId] = addressToFd[address->sin_addr];
    remoteIDs.push_back(peerId);

    std::cout << "The fd for id " << peerId << " is: " << remoteUsers[peerId] << '\n';

    sendNewContactToLocals(peerId);

    delete address;
}

void removeRemoteContact(void* message){
    const char* id = (const char*)message;
    remoteUsers.erase(id);
    for (int i = 0; i < remoteIDs.size(); i++){
        if(!strcmp(remoteIDs[i], id)){
            remoteIDs.erase(remoteIDs.begin()+i);
        }
    }
}

//keep in mind IDs are reversed!
void processTcp(void* message, int fd){
    short method = *(short*)message;
    message = (short*)message + 1;

    if(method == 0){//peer tcp requesting contact
        processRemoteRequest(message);
    }
    else if(method == 1){//peer tcp accepting contact
        processPeerAcceptedContact(message);
    }
    else if(method == 2){//new message incoming
        processIncomingMessage(message);
        std::cout << "New incoming message\n";
    }
    else if(method == 3){//add new contact (the device just joined)
        addNewRemoteContact(message, fd);
    }
    else if(method == 4){//remove remote contact(the device is going to leave)
        removeRemoteContact(message);
    }
    else if(method == 5){//the remote daemon is stopping(may not be used)

    }
}