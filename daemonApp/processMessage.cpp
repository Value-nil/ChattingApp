#include "all.h"
#include "processMessage.h"
#include "../common/fifoUtilities.h"

void onAppOpened(const char* id, uid_t userId, chToInt &fifos, chVec remoteIDs){
    passwd* user = getpwuid(userId);
    const char* fifoDir = getFifoDir(user, D_TO_A_PATH);

    int fd = open(fifoDir, O_WRONLY);
    handleError(fd);

    fifos[id] = fd;

    void* message = operator new(sizeof(short) + sizeof(char)*11);
    void* toSend = message;
    *(short*)message = 2;
    message = (short*)message + 1;

    for(int i = 0; i < remoteIDs.size(); i++){
        strcpy((char*)message, remoteIDs[i]);
        int success = write(fd, toSend, sizeof(short) + sizeof(char)*11);
        handleError(success);
    }

    operator delete(toSend);
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


    int success = write(fd, toSend, sizeof(short)*2+sizeof(char)*22);
    handleError(success);

    operator delete(toSend);
}

void processAcceptContact(const char* peerId, int fd){
    void* message = operator new(sizeof(short)+sizeof(char)*22);
    void* toSend = message;
    *(short*)message = 3;
    message = (short*)message + 1;
    strcpy((char*)message, peerId);

    int success = write(fd, toSend, sizeof(short)+sizeof(char)*22);
    handleError(success);

    operator delete(toSend);
}

void onFifoRequestContact(const char* request, const char* id, const char* peerId, chVec &requestedPeers, int fd, int localFd){
    bool hasBeenRequested = false;
    for(int i = 0; i < requestedPeers.size(); i++){
        if(strcmp(request, requestedPeers[i]) == 0){
            sendContactRequest(id, peerId, fd, true);
            processAcceptContact(peerId, localFd);

            requestedPeers.erase(requestedPeers.begin()+i);
            hasBeenRequested = true;
        }
    }
    if(!hasBeenRequested){
        sendContactRequest(id, peerId, fd, false);
    }

}

void onFifoMessage(void* message, int fd){
    *(short*)message = 1;

    int success = write(fd, message, MAX_SIZE);
    handleError(success);
}

//this function will be to get all the variables from the message. For actually doing somehthing, there's another function
void processFifo(void* message, chToInt &fifos, chVec &requestedPeers, chToInt &remote, chVec remoteIDs){
    short method = *(short*)message;
    message = (short*)message + 1;

    char* id = new char[11];
    strcpy(id, (const char*)message);
    message = (char*)message+11;

    if(method == 0){//0 for indicating app has opened
        uid_t userId = *(uid_t*)message;
        
        onAppOpened(id, userId, fifos, remoteIDs);


        std::cout << id << '\n';
        std::cout << *(uid_t*)message << '\n';
    }
    else if(method == 1){//1 for requesting contact
        //xor
        char* peerId = new char[11];
        strcpy(peerId, (const char*)message);

        char* request = new char[11];
        for(int i = 0; i < 10; i++){
            request[i] = (char)((int)(id[i])^(int)(peerId[i]));
        }
        request[10] = '\0';
        int fd = remote[peerId];
        onFifoRequestContact(request, id, peerId, requestedPeers, fd, fifos[id]);
        std::cout << "User is requesting contact!\n";
    }
    else if(method == 2){//2 for sending message
        char* peerId = new char[11];
        strcpy(peerId, (const char*)message);

        int fd = remote[peerId];

        message = (char*)message - 11;
        message = (short*)message - 2;

        onFifoMessage(message, fd);
        std::cout << "User is sending message!\n";
    }
    else if(method == 3){//app is being closed
        int success = close(fifos[id]);
        handleError(success);
        fifos[id] = 0;
        std::cout << "User is closing app!\n";
    }
}




void processNewIncomingM(int fd, const char* peerId, const char* actualMessage){
    void* message = operator new(sizeof(short)+sizeof(char)*112);
    void* toSend = message;
    *(short*)message = 1;
    message = (short*)message + 1;
    strcpy((char*)message, peerId);
    message = (char*)message + 11;
    strcpy((char*)message, actualMessage);

    int success = write(fd, toSend, sizeof(short)+sizeof(char)*112);
    handleError(success);

    operator delete(toSend);
}

//keep in mind IDs are reversed!
void processTcp(void* message, chVec &requestedPeers, chToInt &fifos){
    short method = *(short*)message;
    message = (short*)message + 1;

    char* peerId = new char[11];
    strcpy(peerId, (const char*)message);
    message = (char*)message+11;

    char* id = new char[11];
    strcpy(id, (const char*)message);
    message = (char*)message + 11;

    if(method == 0){//requestContact
        std::cout << "Peer TCP is requesting contact!\n";
        char* request = new char[11];
        for(int i = 0; i < 10; i++){
            request[i] = (char)((int)(id[i])^(int)(peerId[i]));
        }
        request[10] = '\0';
        //TELL FIFO ABOUT THIS
        requestedPeers.push_back(request);
    }
    else if(method == 1){//acceptContact
        std::cout << "Peer TCP has accepted contact!\n";
        if(fifos[id] <= 0) return;
        processAcceptContact(peerId, fifos[id]);
    }
    else if(method == 2){//newMessage
        std::cout << "New incoming message\n";
        if(fifos[id] <= 0) return;
        processNewIncomingM(fifos[id], peerId, (const char*)message);
    }
}