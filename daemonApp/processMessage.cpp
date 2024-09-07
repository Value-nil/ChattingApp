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
#include <time.h>

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
    
    for(unsigned int i = 0; i < remoteIDs.size(); i++){
        std::cout << "Sending ID " << remoteIDs[i] << " for local peer\n";
        strcpy((char*)message, remoteIDs[i]);
        int success = write(localFd, toSend, sizeof(size_t) + sizeOfMsg);
        handleError(success);
    }
    operator delete(toSend);
}


void initializeUser(const char* id){
    const char* fifoDir = getFifoPath(id, false);
    int localFd = openFifo(fifoDir, O_WRONLY);

    localUsers[id] = localFd;

    sendCurrentOnlinePeers(localFd);
}

void sendContactRequest(const char* id, const char* peerId, bool isAccepting){
    size_t sizeOfMsg = sizeof(short)*2+sizeof(char)*22;

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
    std::cout << "Sending request contact message to fd: " << remoteFd << '\n';
    std::cout << "Size of message: " << *(size_t*)message << '\n';

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
    for(unsigned int i = 0; i < requestedPeers.size(); i++){
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


void registerMessage(const char* messageFilePath, const char* message, bool localIsSender){
    int fd = open(messageFilePath, O_WRONLY | O_CREAT | O_APPEND);
    handleError(fd);

    size_t sizeOfEntry = sizeof(bool) + sizeof(time_t) + sizeof(char)*(strlen(message) + 1);
    void* entry = operator new(sizeOfEntry);
    void* toStore = entry;
    
    *(bool*)entry = localIsSender;
    entry = (bool*)entry + 1;
    time((time_t*)entry);
    entry = (time_t*)entry + 1;
    strcpy((char*)entry, message);
    *((char*)entry + strlen(message)) = '\n'; //setting new line for separating from other entries
    
    int success = write(fd, toStore, sizeOfEntry);
    handleError(success);

    operator delete(toStore);
    close(fd);

}

void sendMessage(const char* id, const char* peerId, const char* actualMessage){
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
        *(request+i) = (char)(id[i]^peerId[i]);
    }
    request[10] = '\0';

    return request;
}

const char* getMessageFilePath(uid_t userId, const char* peerId){
    struct passwd* passwdStruct = getpwuid(userId);
    const char* baseDirPath = buildPath(passwdStruct->pw_dir, INITIAL_DIR_PATH);
    const char* messageDirPath = buildPath(baseDirPath, MESSAGES_PATH);
    const char* fullPath = buildPath(messageDirPath, peerId);

    delete[] baseDirPath;
    delete[] messageDirPath;
    return fullPath;
}

void processFifo(void* message){
    short method = *(short*)message;
    message = (short*)message + 1;

    char* id = new char[11];
    strcpy(id, (const char*)message);
    message = (char*)message+11;

    if(method == 0){//app has opened
        initializeUser(id);
        std::cout << "App with id: " << id << " has opened\n";
    }
    else if(method == 1){//local user requesting contact
        //xor
        const char* peerId = (const char*)message;

        char* request = buildRequest(id, peerId);

        checkRequestingPeers(request, id, peerId);
        std::cout << "Local user is requesting contact with another peer\n";
    }
    else if(method == 2){//sending message
        const char* peerId = (const char*)message;
        message = (char*)message+11;

	uid_t userId = *(uid_t*)message;
	message = (uid_t*)message + 1;

	const char* messagePath = getMessageFilePath(userId, peerId);



        const char* actualMessage = (const char*)message;

        sendMessage(id, peerId, actualMessage);
	registerMessage(messagePath, actualMessage, true);
	delete[] messagePath;
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
    message = (size_t*)message + 1;
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

    for(unsigned int i = 0; i < localIDs.size(); i++){
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

    char* peerId = new char[11];
    strcpy(peerId, (const char*)message);

    remoteUsers[peerId] = addressToFd[address->sin_addr];
    remoteIDs.push_back(peerId);
    std::cout << "size of remoteIDs is: " << remoteIDs.size() << '\n';

    std::cout << "The fd for id " << peerId << " is: " << remoteUsers[peerId] << '\n';

    sendNewContactToLocals(peerId);

    delete address;
}

void removeRemoteContact(void* message){
    std::cout << "Removing peer contact\n";
    const char* id = (const char*)message;
    remoteUsers.erase(id);
    for (unsigned int i = 0; i < remoteIDs.size(); i++){
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
