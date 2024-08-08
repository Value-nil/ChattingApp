#include "processMessage.h"
#include "../common/utilities.h"
#include "../common/constants.h"
#include "types.h"
#include "daemonConstants.h"


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
extern chVec requestedPeers;
extern chToInt remoteUsers;


void sendCurrentOnlinePeers(int localFd){
    void* message = operator new(sizeof(short) + sizeof(char)*11);
    void* toSend = message;

    *(short*)message = 2;
    message = (short*)message + 1;
    
    for(int i = 0; i < remoteIDs.size(); i++){
        strcpy((char*)message, remoteIDs[i]);
        int success = write(localFd, toSend, sizeof(short) + sizeof(char)*11);
        handleError(success);
    }
    operator delete(toSend);
}


void initializeUser(const char* id, uid_t userId){
    passwd* user = getpwuid(userId);
    const char* fifoDir = getFifoPath(user, D_TO_A_PATH);

    int localFd = open(fifoDir, O_WRONLY);
    handleError(localFd);

    localUsers[id] = localFd;

    sendCurrentOnlinePeers(localFd);
}

void sendContactRequest(const char* id, const char* peerId, int fd, bool isAccepting){
    void* message = operator new(sizeof(short)*2+sizeof(char)*22);
    void* toSend = message;

    *(short*)message = 1;
    message = (short*)message + 1;
    *(short*)message = isAccepting? 1 : 0;
    message = (short*)message + 1;
    strcpy((char*)message, id);
    message = (char*)message + 11;
    strcpy((char*)message, peerId);

    std::cout << "Sending request contact messsage to fd: " << fd << '\n';

    int success = write(fd, toSend, sizeof(short)*2+sizeof(char)*22);
    handleError(success);

    operator delete(toSend);
}

void processAcceptContact(const char* peerId, int localFd){
    void* message = operator new(sizeof(short)+sizeof(char)*22);
    void* toSend = message;

    *(short*)message = 3;
    message = (short*)message + 1;
    strcpy((char*)message, peerId);

    std::cout << "User accepted contact\n";

    int success = write(localFd, toSend, sizeof(short)+sizeof(char)*22);
    handleError(success);

    operator delete(toSend);
}

void checkRequestingPeers(const char* request, const char* id, const char* peerId, int remoteFd, int localFd){
    bool hasBeenRequested = false;
    for(int i = 0; i < requestedPeers.size(); i++){
        if(strcmp(request, requestedPeers[i]) == 0){
            sendContactRequest(id, peerId, remoteFd, true);
            processAcceptContact(peerId, localFd);

            requestedPeers.erase(requestedPeers.begin()+i);
            hasBeenRequested = true;
        }
    }
    if(!hasBeenRequested){
        sendContactRequest(id, peerId, remoteFd, false);
    }

}

void sendMessage(void* message, int remoteFd){
    *(short*)message = 1;

    int success = write(remoteFd, message, MAX_SIZE);
    handleError(success);
}

char* buildRequest(char* id, char* peerId){
    char* request = new char[11];
    for(int i = 0; i < 10; i++){
        request[i] = (char)((int)(id[i])^(int)(peerId[i]));
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
        
        int remoteFd = remoteUsers[peerId];
        int localFd = localUsers[id];
        checkRequestingPeers(request, id, peerId, remoteFd, localFd);
        std::cout << "Local user is requesting contact with another peer\n";
    }
    else if(method == 2){//sending message
        char* peerId = new char[11];
        strcpy(peerId, (const char*)message);

        int remoteFd = remoteUsers[peerId];

        message = (char*)message - 11;
        message = (short*)message - 2;

        sendMessage(message, remoteFd);
        std::cout << "Local user is sending message!\n";
    }
    else if(method == 3){//app is being closed
        int success = close(localUsers[id]);
        handleError(success);
        localUsers[id] = 0;
        std::cout << "Local user is closing app!\n";
    }
}




void processIncomingMessage(int localFd, const char* peerId, const char* actualMessage){
    void* message = operator new(sizeof(short)+sizeof(char)*112);
    void* toSend = message;
    *(short*)message = 1;
    message = (short*)message + 1;
    strcpy((char*)message, peerId);
    message = (char*)message + 11;
    strcpy((char*)message, actualMessage);

    int success = write(localFd, toSend, sizeof(short)+sizeof(char)*112);
    handleError(success);

    operator delete(toSend);
}

void processRemoteRequest(char* id, char* peerId){
    char* request = buildRequest(id, peerId);
    //TELL FIFO ABOUT THIS(future)
    requestedPeers.push_back(request);
    std::cout << "Peer TCP is requesting contact!\n";
}

//keep in mind IDs are reversed!
void processTcp(void* message){
    short method = *(short*)message;
    message = (short*)message + 1;

    char* peerId = new char[11];
    strcpy(peerId, (const char*)message);
    message = (char*)message+11;

    char* id = new char[11];
    strcpy(id, (const char*)message);
    message = (char*)message + 11;

    if(method == 0){//peer tcp requesting contact
        std::cout << "Remote TCP is requesting contact\n";
        processRemoteRequest(id, peerId);
    }
    else if(method == 1){//peer tcp accepting contact
        int localFd = localUsers[id];
        if(localFd <= 0) return; // the user is offline
        processAcceptContact(peerId, localFd);

        std::cout << "Peer TCP has accepted contact\n";
    }
    else if(method == 2){//new message incoming
        
        
        const char* actualMessage = (const char*)message;
        int localFd = localUsers[id];
        if(localFd <= 0) return; //the user is offline
        processIncomingMessage(localFd, peerId, actualMessage);

        std::cout << "New incoming message\n";
    }
}