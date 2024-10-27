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



void sendPeerToLocal(int localFd, deviceid_t peerId){
    size_t sizeOfMsg = sizeof(short) + sizeof(deviceid_t);

    void* message = operator new(sizeof(size_t) + sizeOfMsg);
    void* toSend = message;

    *(size_t*)message = sizeOfMsg;
    message = (size_t*)message + 1;
    *(short*)message = 2;
    message = (short*)message + 1;
    *(deviceid_t*)message = peerId;

    std::cout << "Sending ID " << peerId << " to local peer\n";
    int success = write(localFd, toSend, sizeof(size_t) + sizeOfMsg);
    handleError(success);

    operator delete(toSend);
}
void requestRemoteContacts(deviceid_t id){
    std::cout << "Requesting remote contacts!\n";

    size_t sizeOfMsg = sizeof(short)*2+sizeof(deviceid_t);

    void* message = operator new(sizeof(size_t)+sizeOfMsg);
    void* toSend = message;

    *(size_t*)message = sizeOfMsg;
    message = (size_t*)message + 1;
    *(short*)message = 1;
    message = (short*)message + 1;
    *(short*)message = 6;
    message = (short*)message + 1;
    *(deviceid_t*)message = id; //to resend and know what id you have to router to

    for(auto iter = remoteDevices.begin(); iter != remoteDevices.end(); iter++){
	int success = write((*iter).second, toSend, sizeof(size_t)+sizeOfMsg);
	handleError(success);
    }

    operator delete(toSend);
}

void initializeUser(void* message){
    deviceid_t id = *(deviceid_t*)message;//this is only the user id part!
    message = (deviceid_t*)message+1;

    const char* fifoDir = getFifoPath(id, false);
    int localFd = openFifo(fifoDir, O_WRONLY);

    localUsers[id] = localFd;

    requestRemoteContacts(id);

    std::cout << "App with id: " << id << " has opened\n";
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
    const char* messageFilePath = getMessageFilePath(id, peerId);
    int success2 = creat(messageFilePath, S_IRWXU | S_IROTH);
    handleError(success2);

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
    int fd = open(messageFilePath, O_WRONLY | O_CREAT | O_APPEND, S_IRUSR | S_IWUSR);
    handleError(fd);

    size_t sizeOfEntry = metadataSize + sizeof(char)*(strlen(message));
    void* entry = operator new(sizeOfEntry);
    void* toStore = entry;
    
    *(bool*)entry = localIsSender;
    entry = (bool*)entry + 1;
    time((time_t*)entry);
    entry = (time_t*)entry + 1;
    *(int*)entry = strlen(message);
    strcpy((char*)entry, message);
    //*((char*)entry + strlen(message)) = '\0'; //setting new line for separating from other entries
    
    int success = write(fd, toStore, sizeOfEntry);
    handleError(success);

    operator delete(toStore);
    close(fd);

}

void sendMessage(deviceid_t id, deviceid_t peerId, const char* actualMessage){
    size_t sizeOfMsg = sizeof(short)*2+sizeof(deviceid_t)*2+sizeof(char)*(messageLimit+1);

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


void processLocalRequest(void* message){
    deviceid_t id = *(deviceid_t*)message;//this is only the user id part!
    message = (deviceid_t*)message+1;
    deviceid_t peerId = *(deviceid_t*)message;

    deviceid_t request = buildRequest(id, peerId);

    checkRequestingPeers(request, id, peerId);
    std::cout << "Local user is requesting contact with another peer\n";
}

void sendMessageToRemote(void* message){
    deviceid_t id = *(deviceid_t*)message;//this is only the user id part!
    message = (deviceid_t*)message+1;
    deviceid_t peerId = *(deviceid_t*)message;
    message = (deviceid_t*)message+1;

    const char* messagePath = getMessageFilePath(id, peerId);

    const char* actualMessage = (const char*)message;

    sendMessage(id, peerId, actualMessage);
    registerMessage(messagePath, actualMessage, true);

    delete[] messagePath;
    std::cout << "Local user is sending message!\n";
}

void closeApp(void* message){
    deviceid_t id = *(deviceid_t*)message;//this is only the user id part!
    message = (deviceid_t*)message+1;

    int success = close(localUsers[id]);
    handleError(success);
    localUsers[id] = 0;

    restartUserFifos((uid_t)id);
    std::cout << "Local user is closing app!\n";
}



void processFifo(void* message){
    short method = *(short*)message;
    message = (short*)message + 1;

    switch(method){
	case 0: initializeUser(message); break; //app has opened
	case 1: processLocalRequest(message); break; //local contact requesting contact
	case 2: sendMessageToRemote(message); break; //local contact sending message
	case 3: closeApp(message); break; //local contact is closing app
    }
}

void sendIncomingMessage(int localFd, deviceid_t peerId, const char* actualMessage){
    size_t sizeOfMsg = sizeof(short)+sizeof(deviceid_t)+sizeof(char)*(messageLimit+1);

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
    std::cout << "New incoming message\n";
    deviceid_t peerId = *(deviceid_t*)message;
    message = (deviceid_t*)message+1;

    deviceid_t id = *(deviceid_t*)message & USER_PART;//whenever local IDs come from the peer device, they come in full format(deviceid+userid)
    message = (deviceid_t*)message+1;

    const char* actualMessage = (const char*)message;

    const char* messageFilePath = getMessageFilePath(id, peerId);
    registerMessage(messageFilePath, actualMessage, false);
    delete[] messageFilePath;

    int localFd = localUsers[id];
    if(localFd <= 0) return; //the user is offline

    sendIncomingMessage(localFd, peerId, actualMessage);
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
    
    for(auto iter = localUsers.begin(); iter != localUsers.end(); iter++){
        int fd = (*iter).second;
        if(fd != 0)
            sendPeerToLocal(fd, peerId);

        std::cout << "File descriptor for FIFO is: " << fd << '\n';
        std::cout << "The ID it is reffering to: " << (*iter).first << '\n';
    }
}

void addNewRemoteContact(void* message){
    deviceid_t userThatWasRequesting = *(deviceid_t*)message;
    message = (deviceid_t*)message + 1;
    deviceid_t peerId = *(deviceid_t*)message;
    std::cout << "The user that was requesting is " << userThatWasRequesting << '\n';
    std::cout << "The remote contact to add is " << peerId << '\n';

    if(userThatWasRequesting == 0)
        sendNewContactToLocals(peerId);
    else
    	sendPeerToLocal(localUsers[userThatWasRequesting], peerId);
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
    sendLocalContacts(fd, 0); //0 to represent sending for all remote peers
}

void sendLocalContactsToSpecificId(void* message, int fd){
    deviceid_t peerId = *(deviceid_t*)message;
    sendLocalContacts(fd, peerId);
}


//keep in mind IDs are reversed!
void processTcp(void* message, int fd){
    short method = *(short*)message;
    message = (short*)message + 1;

    switch(method){
	case 0: processRemoteRequest(message); break; //peer contact requesting contact
	case 1: processPeerAcceptedContact(message); break; //peer contact accepted contact
	case 2: processIncomingMessage(message); break; //new message from a peer contact
	case 3: addNewRemoteContact(message); break; //new remote contact on subnet
	case 4: removeRemoteContact(message); break; //remote contact is leaving
	case 5: addRemoteDevice(message, fd); break; //remote device is sending its device id
	case 6: sendLocalContactsToSpecificId(message, fd); break; //remote contact has opened and is requesting device
    }

}
