#include "processMessage.h"
#include "../common/utilities.h"
#include "../common/constants.h"
#include "daemonTypes.h"
#include "daemonConstants.h"
#include "fifoUtils.h"
#include "udp.h"


#include <pwd.h>
#include <unistd.h>
#include <new>
#include <string.h>
#include <vector>
#include <map>
#include <iostream>
#include <fcntl.h>
#include <time.h>

extern idToFd localUsers;
extern idVec requestedPeers;
extern idToFd remoteDevices;
extern deviceid_t deviceId;


void sendCurrentOnlinePeers(int localFd){
    size_t sizeOfMsg = sizeof(short) + sizeof(deviceid_t);

    void* message = operator new(sizeof(size_t) + sizeOfMsg);
    void* toSend = message;

    *(size_t*)message = sizeOfMsg;
    message = (size_t*)message + 1;
    *(short*)message = 2;
    message = (short*)message + 1;
    
    for(auto iter = remoteDevices.begin(); iter != remoteDevices.end(); iter++){
        std::cout << "Sending ID " << (*iter).first << " to local peer\n";

	*(deviceid_t*)message = (*iter).first;
        int success = write(localFd, toSend, sizeof(size_t) + sizeOfMsg);
        handleError(success);
    }
    operator delete(toSend);
}


void initializeUser(deviceid_t id){
    const char* fifoDir = getFifoPath(id, false);
    int localFd = openFifo(fifoDir, O_WRONLY);

    localUsers[id] = localFd;

    sendCurrentOnlinePeers(localFd);
}

void sendContactRequest(deviceid_t id, deviceid_t peerId, bool isAccepting){
    size_t sizeOfMsg = sizeof(short)*2+sizeof(deviceid_t)*2;

    void* message = operator new(sizeof(size_t) + sizeOfMsg);
    void* toSend = message;

    *(size_t*)message = sizeOfMsg;
    message = (size_t*)message + 1;
    *(short*)message = 1;
    message = (short*)message + 1;
    *(short*)message = isAccepting? 1 : 0;
    message = (short*)message + 1;
    *(deviceid_t*)message = (id | deviceId);
    message = (deviceid_t*)message + 1;
    *(deviceid_t*)message = peerId;

    int remoteFd = remoteDevices[peerId & (~USER_PART)];
    std::cout << "Detected device id is " << (peerId & (~USER_PART)) << '\n';

    int success = write(remoteFd, toSend, sizeof(size_t) + sizeOfMsg);
    handleError(success);

    operator delete(toSend);
}

void processAcceptContact(deviceid_t peerId, deviceid_t id){
    size_t sizeOfMsg = sizeof(short) + sizeof(deviceid_t);

    void* message = operator new(sizeof(size_t)+sizeOfMsg);
    void* toSend = message;

    *(size_t*)message = sizeOfMsg;
    message = (size_t*)message + 1;
    *(short*)message = 3;
    message = (short*)message + 1;
    *(deviceid_t*)message = peerId;

    std::cout << "User accepted contact\n";

    int localFd = localUsers[id & USER_PART];

    int success = write(localFd, toSend, sizeof(size_t) + sizeOfMsg);
    handleError(success);

    operator delete(toSend);
}

void checkRequestingPeers(deviceid_t request, deviceid_t id, deviceid_t peerId){
    bool hasBeenRequested = false;
    for(unsigned int i = 0; i < requestedPeers.size(); i++){
        if(request == requestedPeers[i]){
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

void sendMessage(deviceid_t id, deviceid_t peerId, const char* actualMessage){
    size_t sizeOfMsg = sizeof(short)*2+sizeof(deviceid_t)*2+sizeof(char)*101;

    void* message = operator new(sizeof(size_t) + sizeOfMsg);
    void* toSend = message;

    *(size_t*)message = sizeOfMsg;
    message = (size_t*)message + 1;
    *(short*)message = 1;
    message = (short*)message + 1;
    *(short*)message = 2;
    message = (short*)message + 1;
    *(deviceid_t*)message = (id | deviceId);
    message = (deviceid_t*) message + 1;
    *(deviceid_t*)message = peerId;
    message = (deviceid_t*)message + 1;
    strcpy((char*)message, actualMessage);

    int remoteFd = remoteDevices[peerId & ~USER_PART];

    int success = write(remoteFd, toSend, sizeof(size_t) + sizeOfMsg);
    handleError(success);

    operator delete(toSend);
}

deviceid_t buildRequest(deviceid_t id, deviceid_t peerId){
    return (id & USER_PART)^peerId;
}

const char* getMessageFilePath(deviceid_t userId, deviceid_t peerId){
    struct passwd* passwdStruct = getpwuid((uid_t)userId);
    const char* baseDirPath = buildPath(passwdStruct->pw_dir, INITIAL_DIR_PATH);
    const char* messageDirPath = buildPath(baseDirPath, MESSAGES_PATH);
    const char* stringifiedPeerId = stringifyId(peerId);
    const char* fullPath = buildPath(messageDirPath, stringifiedPeerId);

    delete[] baseDirPath;
    delete[] messageDirPath;
    delete[] stringifiedPeerId;
    return fullPath;
}

void processFifo(void* message){
    short method = *(short*)message;
    message = (short*)message + 1;

    deviceid_t id = *(deviceid_t*)message;//this is only the user id part!
    message = (deviceid_t*)message+1;

    if(method == 0){//app has opened
        initializeUser(id);
        std::cout << "App with id: " << id << " has opened\n";
    }
    else if(method == 1){//local user requesting contact
        //xor
        deviceid_t peerId = *(deviceid_t*)message;

        deviceid_t request = buildRequest(id, peerId);

        checkRequestingPeers(request, id, peerId);
        std::cout << "Local user is requesting contact with another peer\n";
    }
    else if(method == 2){//sending message
	deviceid_t peerId = *(deviceid_t*)message;
        message = (deviceid_t*)message+1;

	const char* messagePath = getMessageFilePath(id, peerId);

        const char* actualMessage = (const char*)message;

        sendMessage(id, peerId, actualMessage);
	registerMessage(messagePath, actualMessage, true);

	delete[] messagePath;
        std::cout << "Local user is sending message!\n";
    }
    else if(method == 3){//app is being closed
        int success = close(localUsers[id]);
        handleError(success);
        localUsers[id] = 0;

        restartUserFifos((uid_t)id);
        std::cout << "Local user is closing app!\n";
    }
}

void sendIncomingMessage(int localFd, deviceid_t peerId, const char* actualMessage){
    size_t sizeOfMsg = sizeof(short)+sizeof(deviceid_t)+sizeof(char)*101;

    void* message = operator new(sizeof(size_t)+ sizeOfMsg);
    void* toSend = message;

    *(size_t*)message = sizeOfMsg;
    message = (size_t*)message + 1;
    *(short*)message = 1;
    message = (short*)message + 1;
    *(deviceid_t*)message = peerId;
    message = (deviceid_t*)message + 1;
    strcpy((char*)message, actualMessage);
    
    int success = write(localFd, toSend, sizeof(size_t)+ sizeOfMsg);
    handleError(success);

    operator delete(toSend);
}


void processIncomingMessage(void* message){
    deviceid_t peerId = *(deviceid_t*)message;
    message = (deviceid_t*)message+1;

    deviceid_t id = *(deviceid_t*)message;
    message = (deviceid_t*)message+1;

    int localFd = localUsers[id & USER_PART];
    if(localFd <= 0) return; //the user is offline

    const char* actualMessage = (const char*)message;

    sendIncomingMessage(localFd, peerId, actualMessage);
    std::cout << "New incoming message\n";
}

void processPeerAcceptedContact(void* message){
    deviceid_t peerId = *(deviceid_t*)message;
    message = (deviceid_t*)message+1;

    deviceid_t id = *(deviceid_t*)message;

    int localFd = localUsers[id & USER_PART];
    if(localFd <= 0) return; // the user is offline
    processAcceptContact(peerId, id);

    std::cout << "Peer TCP has accepted contact\n";
}

void processRemoteRequest(void* message){
    deviceid_t peerId = *(deviceid_t*)message;
    message = (deviceid_t*)message+1;

    deviceid_t id = *(deviceid_t*)message;

    deviceid_t request = buildRequest(id, peerId);
    requestedPeers.push_back(request);
    //TELL FIFO ABOUT THIS(future)
    std::cout << "Remote TCP is requesting contact!\n";
}

void sendNewContactToLocals(deviceid_t peerId){
    size_t sizeOfMsg = sizeof(short) + sizeof(deviceid_t);

    void* message = operator new(sizeof(size_t) + sizeOfMsg);
    void* toSend = message;

    *(size_t*)message = sizeOfMsg;
    message = (size_t*)message + 1;
    *(short*)message = 2;
    message = (short*)message + 1;
    *(deviceid_t*)message = peerId;

    for(auto iter = localUsers.begin(); iter != localUsers.end(); iter++){
        int fd = (*iter).second;
        if(fd != 0){
            int success = write(fd, toSend, sizeof(size_t) + sizeOfMsg);
            handleError(success);
        } 

        std::cout << "File descriptor for FIFO is: " << fd << '\n';
        std::cout << "The ID it is reffering to: " << (*iter).first << '\n';
    }

    operator delete(toSend);
}

void addNewRemoteContact(void* message){
    deviceid_t peerId = *(deviceid_t*)message;
    sendNewContactToLocals(peerId);
}

void removeRemoteContact(void* message){
    deviceid_t peerId = *(deviceid_t*)message;
    std::cout << "Contact " << peerId << " is closing\n";

    size_t sizeOfMsg = sizeof(short) + sizeof(deviceid_t);

    void* closingMessage = operator new(sizeof(size_t) + sizeOfMsg);
    void* toSend = closingMessage;

    *(size_t*)closingMessage = sizeOfMsg;
    closingMessage = (size_t*)closingMessage + 1;
    *(short*)closingMessage = 4;
    closingMessage = (short*)closingMessage + 1;
    *(deviceid_t*)closingMessage = peerId;

    for(auto iter = localUsers.begin(); iter != localUsers.end(); iter++){
        int fd = (*iter).second;
        if(fd != 0){
            int success = write(fd, toSend, sizeof(size_t) + sizeOfMsg);
            handleError(success);
        }
    }
    operator delete(toSend);
}

void addRemoteDevice(void* message, int fd){
    deviceid_t remoteId = *(deviceid_t*)message;
    remoteDevices[remoteId] = fd;
    sendLocalContacts(fd);
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
    }
    else if(method == 3){//add new contact (the device just joined)
        addNewRemoteContact(message);
    }
    else if(method == 4){//remove remote contact(the device is going to leave)
        removeRemoteContact(message);
    }
    else if(method == 5){//remote device is sending their deviceId
	addRemoteDevice(message, fd);
    }
}
