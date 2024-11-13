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
#include <dirent.h>

extern idToFd localUsers;
extern idVec requestedPeers;
extern idToFd remoteDevices;
extern deviceid_t deviceId;
idToFd syncFds;
idToSyncBuffer syncBuffers;




static void sendPeerToLocal(int localFd, deviceid_t peerId){
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
static inline void requestRemoteContacts(deviceid_t id){
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

static inline void initializeUser(void* message){
    deviceid_t id = *(deviceid_t*)message;//this is only the user id part!
    message = (deviceid_t*)message+1;

    const char* fifoDir = getFifoPath(id, false);
    int localFd = openFifo(fifoDir, O_WRONLY);

    localUsers[id] = localFd;

    requestRemoteContacts(id);

    std::cout << "App with id: " << id << " has opened\n";
}

static void sendContactRequest(deviceid_t id, deviceid_t peerId, bool isAccepting){
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

static void processAcceptContact(deviceid_t peerId, deviceid_t id){
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

static inline void checkRequestingPeers(deviceid_t request, deviceid_t id, deviceid_t peerId){
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


static void registerMessage(const char* messageFilePath, const char* message, bool localIsSender){
    int fd = open(messageFilePath, O_WRONLY | O_CREAT | O_APPEND, S_IRUSR | S_IWUSR);
    handleError(fd);

    int findSuccess = lseek(fd, 0, SEEK_END);
    handleError(findSuccess);

    size_t sizeOfEntry = metadataSize + sizeof(char)*(strlen(message));
    void* entry = operator new(sizeOfEntry);
    void* toStore = entry;
    
    *(bool*)entry = localIsSender;
    entry = (bool*)entry + 1;
    time((time_t*)entry);
    entry = (time_t*)entry + 1;
    *(int*)entry = strlen(message);
    entry = (int*)entry + 1;
    strncpy((char*)entry, message, strlen(message));
    
    int success = write(fd, toStore, sizeOfEntry);
    handleError(success);

    operator delete(toStore);
    close(fd);

    std::cout << "Registered message\n";
}

static inline void sendMessage(deviceid_t id, deviceid_t peerId, const char* actualMessage){
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

static inline deviceid_t buildRequest(deviceid_t id, deviceid_t peerId){
    return (id & USER_PART)^peerId;
}


static inline void processLocalRequest(void* message){
    deviceid_t id = *(deviceid_t*)message;//this is only the user id part!
    message = (deviceid_t*)message+1;
    deviceid_t peerId = *(deviceid_t*)message;

    deviceid_t request = buildRequest(id, peerId);

    checkRequestingPeers(request, id, peerId);
    std::cout << "Local user is requesting contact with another peer\n";
}

static inline void sendMessageToRemote(void* message){
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

static inline void closeApp(void* message){
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

static inline void sendIncomingMessage(int localFd, deviceid_t peerId, const char* actualMessage){
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


static inline void processIncomingMessage(void* message){
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

static inline void processPeerAcceptedContact(void* message){
    deviceid_t peerId = *(deviceid_t*)message;
    message = (deviceid_t*)message+1;

    deviceid_t id = *(deviceid_t*)message;

    int localFd = localUsers[id & USER_PART];
    if(localFd <= 0) return; // the user is offline
    processAcceptContact(peerId, id);

    std::cout << "Peer TCP has accepted contact\n";
}

static inline void processRemoteRequest(void* message){
    deviceid_t peerId = *(deviceid_t*)message;
    message = (deviceid_t*)message+1;

    deviceid_t id = *(deviceid_t*)message;

    deviceid_t request = buildRequest(id, peerId);
    requestedPeers.push_back(request);
    //TELL FIFO ABOUT THIS(future)
    std::cout << "Remote TCP is requesting contact!\n";
}

static inline void sendNewContactToLocals(deviceid_t peerId){
    
    for(auto iter = localUsers.begin(); iter != localUsers.end(); iter++){
        int fd = (*iter).second;
        if(fd != 0)
            sendPeerToLocal(fd, peerId);

        std::cout << "File descriptor for FIFO is: " << fd << '\n';
        std::cout << "The ID it is reffering to: " << (*iter).first << '\n';
    }
}



static inline void addNewRemoteContact(void* message){
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

static inline void removeRemoteContact(void* message){
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

static void openMessageFileForSync(deviceid_t localId, deviceid_t peerId){//both ids given in full format
    const char* messageFilePath = getMessageFilePath((localId & USER_PART), peerId);

    int fd = open(messageFilePath, O_RDWR);
    handleError(fd);

    delete[] messageFilePath;

    deviceid_t idXor = localId^peerId;
    syncFds[idXor] = fd;
    std::cout << "Sync starting for local id " << localId << " and peer id " << peerId << '\n';
}

static void* readRegisteredMessage(deviceid_t xorIds){
    int fileFd = syncFds[xorIds];

    void* metadata = operator new(metadataSize);
    void* toDelete = metadata;

    int success = read(fileFd, metadata, metadataSize);
    handleError(success);
    if(success == 0) return nullptr;

    metadata += sizeof(bool) + sizeof(time_t);
    int sizeOfActualMessage = *(int*)metadata;
    metadata = toDelete;

    char* actualMessage = new char[sizeOfActualMessage];
    int success2 = read(fileFd, actualMessage, sizeOfActualMessage);
    handleError(success2);

    void* fullRegister = operator new(metadataSize + sizeof(char)*sizeOfActualMessage);
    memcpy(fullRegister, metadata, metadataSize);
    fullRegister += metadataSize;
    strncpy((char*)fullRegister, actualMessage, sizeOfActualMessage);

    operator delete(toDelete);
    delete[] actualMessage;

    return fullRegister;
}
static void sendEndSyncMessage(deviceid_t localId, deviceid_t peerId){
    size_t sizeOfMsg = sizeof(short)*2 + sizeof(deviceid_t)*2;
    int fd = remoteDevices[peerId & ~USER_PART];

    void* message = operator new(sizeof(size_t) + sizeOfMsg);
    void* toSend = message;

    *(size_t*)message = sizeOfMsg;
    message += sizeof(size_t);
    *(short*)message = 1;
    message += sizeof(short);
    *(short*)message = 9;
    message += sizeof(short);
    *(deviceid_t*)message = localId;
    message += sizeof(deviceid_t);
    *(deviceid_t*)message = peerId;

    int success = write(fd, toSend, sizeof(size_t) + sizeOfMsg);
    handleError(success);
}

static void sendSyncMessage(void* payload, deviceid_t xorIds, int fd){
    int sizeOfActualMessage = *(int*)(payload + sizeof(bool) + sizeof(time_t));
    size_t sizeOfMsg = sizeof(short)*2 + metadataSize + sizeof(deviceid_t) + sizeOfActualMessage*sizeof(char);

    void* message = operator new(sizeof(size_t) + sizeOfMsg);
    void* toSend = message;

    *(size_t*)message = sizeOfMsg;
    message += sizeof(size_t);
    *(short*)message = 1;
    message += sizeof(short);
    *(short*)message = 8;
    message += sizeof(short);
    *(deviceid_t*)message = xorIds;
    message += sizeof(deviceid_t);
    memcpy(message, payload, metadataSize + sizeOfActualMessage);
    
    int success = write(fd, toSend, sizeof(size_t) + sizeOfMsg);
    handleError(success);
}

static inline void sendSyncronizationStartMessage(deviceid_t peerDeviceId){
    size_t sizeOfMsg = sizeof(short)*2 + sizeof(deviceid_t)*2;
    int fd = remoteDevices[peerDeviceId];

    void* message = operator new(sizeof(size_t) + sizeOfMsg);
    void* toSend = message;

    *(size_t*)message = sizeOfMsg;
    message = (size_t*)message + 1;
    *(short*)message = 1;
    message = (short*)message + 1;
    *(short*)message = 7;
    message = (short*)message + 1;

    for(auto iter = localUsers.begin(); iter != localUsers.end(); iter++){
	deviceid_t id = (*iter).first;
	deviceid_t fullId = id | deviceId;

	*(deviceid_t*)message = fullId;
	message = (deviceid_t*)message + 1;

	const char* messageDirPath = getMessageDirectoryPath(id);
	DIR* dirptr = opendir(messageDirPath);
	struct dirent* direcEnt = readdir(dirptr);

	delete[] messageDirPath;

	while(direcEnt != nullptr){
	    deviceid_t peerId = unstringifyId(direcEnt->d_name);

	    if((peerId & ~USER_PART) == peerDeviceId){
		*(deviceid_t*)message = peerId;
		int success2 = write(fd, toSend, sizeof(size_t) + sizeOfMsg);
		handleError(success2);

		openMessageFileForSync(fullId, peerId);
		void* firstMessage = readRegisteredMessage(fullId^peerId);
		if(firstMessage != nullptr)
		    sendSyncMessage(firstMessage, fullId | peerId, remoteDevices[peerId & ~USER_PART]);
		else
		    sendEndSyncMessage(fullId, peerId);

	    }
	    direcEnt = readdir(dirptr);
	}
	int success3 = closedir(dirptr);
	handleError(success3);

	message = (deviceid_t*)message - 1;
    }
    operator delete(toSend);
}

static inline void addRemoteDevice(void* message, int fd){
    deviceid_t remoteId = *(deviceid_t*)message;
    remoteDevices[remoteId] = fd;
    sendLocalContacts(fd, 0); //0 to represent sending for all remote peers
    sendSyncronizationStartMessage(remoteId);
}

static inline void sendLocalContactsToSpecificId(void* message, int fd){
    deviceid_t peerId = *(deviceid_t*)message;
    sendLocalContacts(fd, peerId);
}

static inline void processSyncStartRequest(void* message){
    deviceid_t peerId = *(deviceid_t*)message;
    message = (deviceid_t*)message + 1;
    deviceid_t localId = *(deviceid_t*)message;

    openMessageFileForSync(localId, peerId);
}

static void writeToTempFile(void* message, deviceid_t xorIds){
    message += sizeof(bool) + sizeof(time_t);
    int sizeOfActualMessage = *(int*)message;
    message -= sizeof(bool) + sizeof(time_t);

    void* record = operator new(metadataSize + sizeof(char)*sizeOfActualMessage);
    memcpy(record, message, metadataSize + sizeof(char)*sizeOfActualMessage);

    syncBuffers[xorIds].push_back(record);
}

static void writeOlderMessage(void* message, deviceid_t xorIds, int currentOffset){
    writeToTempFile(message, xorIds);
    int success = lseek(syncFds[xorIds], currentOffset, SEEK_SET);
    handleError(success);
}

static short compareMessages(void* localRecord, void* remoteRecord){
    bool localSentM1 = *(bool*)remoteRecord;
    remoteRecord += sizeof(bool);
    time_t timeSentM1 = *(time_t*)remoteRecord;

    bool localSentM2 = *(bool*)localRecord;
    localRecord += sizeof(bool);
    time_t timeSentM2 = *(time_t*)localRecord;

    long long difference = timeSentM1 - timeSentM2;
    if(difference > 0)//remote record time is higher than local
	return 1;
    else if(difference < 0)//remote record time is lower than local
	return -1;
    else return 0;
}
static void processEqualMessage(deviceid_t xorIds, void* currentRecord, int fd){
    writeToTempFile(currentRecord, xorIds);
    operator delete(currentRecord);

    int currentOffset = lseek(fileFd, 0, SEEK_CUR);
    handleError(currentOffset);

    currentRecord = readRegisteredMessage(xorIds);
    sendSyncMessage(currentRecord, xorIds, fd);

    int success = lseek(syncFds[xorIds], currentOffset, SEEK_SET);
    handleError(success);

    operator delete(currentRecord);
}


static inline void processSyncRequest(void* message, int fd){
    deviceid_t xorIds = *(deviceid_t*)message;
    int fileFd = syncFds[xorIds];

    message += sizeof(deviceid_t);
    *(bool*)message = !(*(bool*)message);//locally sent variable depends on the device; since it's being sent by a peer, it must be negated

    int currentOffset = lseek(fileFd, 0, SEEK_CUR);
    handleError(currentOffset);

    void* currentRecord = readRegisteredMessage(xorIds);
    short difference = compareMessages(currentRecord, message);
 
    if(difference == -1){
	writeOlderMessage(message, xorIds, currentOffset);
	operator delete(currentRecord);
    }
    else if(difference == 1){
	while(difference == 1){
	    writeToTempFile(currentRecord, xorIds);
	    sendSyncMessage(currentRecord, xorIds, fd);
	    operator delete(currentRecord);

	    int currentOffset = lseek(fileFd, 0, SEEK_CUR);
	    handleError(currentOffset);

	    currentRecord = readRegisteredMessage(xorIds);
	    difference = compareMessages(currentRecord, message);
	}
	if(difference == -1){
	    writeOlderMessage(message, xorIds, currentOffset);
	    operator delete(currentRecord);
	}
	else
	    processEqualMessage(xorIds, currentRecord, fd);
    }
    else
	processEqualMessage(xorIds, currentRecord, fd);
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
	case 7: processSyncStartRequest(message); break; //remote device is requesting sync
	case 8: processSyncRequest(message, fd); break; // remote device is sending message for sync
    }

}
