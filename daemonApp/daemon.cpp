#include "all.h"
#include "udp.h"
#include "processMessage.h"
#include "../common/fifoUtilities.h"

bool cmp_str::operator()(const char* chr1, const char* chr2)const{
    return strcmp(chr1, chr2) < 0;
}

bool cmp_addr::operator()(const sockaddr_in addr1, const sockaddr_in addr2)const{
    if(addr1.sin_addr.s_addr == addr2.sin_addr.s_addr){
        return addr1.sin_port < addr2.sin_port;
    }
    return addr1.sin_addr.s_addr < addr2.sin_addr.s_addr;
}


sockaddr_in* getOwnAddress(bool isReceiving){
    sockaddr_in *fullAddress = new sockaddr_in;
    fullAddress->sin_addr.s_addr = htonl(INADDR_ANY);
    fullAddress->sin_family = AF_INET;
    fullAddress->sin_port = htons(isReceiving? LISTENING_PORT : SENDING_PORT);
    
    return fullAddress;
}

void bindSocket(int socket, bool isReceiving, bool isudp){
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



pollfd newtcpSocket(bool isReceiving){
    int sock = socket(PF_INET, SOCK_STREAM, 0);
    handleError(sock);

    bindSocket(sock, isReceiving, false);

    

    pollfd toReturn;
    toReturn.fd = sock;
    toReturn.events = POLLIN;

    return toReturn;
}


char* newId(){
    char* id = new char[11];
    const char* possibleCharacters = "1234567890";

    void* randomNums = operator new(sizeof(unsigned short)*10);
    int success = getrandom(randomNums, sizeof(unsigned short)*10, 0);
    handleError(success);

    for(int i = 0; i < 10; i++){
        *(id+i) = possibleCharacters[(*(unsigned short*)randomNums) % 10];
        randomNums = (unsigned short*)randomNums + 1;
    }
    randomNums = (unsigned short*)randomNums - 10;
    id[10] = '\0';

    operator delete(randomNums);

    return id;
}

char* createFifo(const char* directory, const char* fifoPath, bool shouldReturn){
    char* fifoDirectory = new char[strlen(directory) + strlen(fifoPath)];
    strcpy(fifoDirectory, directory);
    strcat(fifoDirectory, fifoPath);

    std::cout << "Fifo directory is: " << fifoDirectory << '\n';

    int fifoSuccess = mkfifo(fifoDirectory, S_IRWXO | S_IRWXU);
    if(fifoSuccess == -1 && errno != 17){
        handleError(-1);
    }
    if(shouldReturn){
        return fifoDirectory;
    }
    delete[] fifoDirectory;
    return nullptr;
}

char* checkForId(char* directory){
    int mkdirSuccess = mkdir(directory, S_IRWXO | S_IRWXU);
    if(mkdirSuccess == -1 && errno != 17){
        handleError(-1);
    }
    char* idDirectory = new char[strlen(directory)+strlen(ID_PATH)];
    strcpy(idDirectory, directory);
    strcat(idDirectory, ID_PATH);

    int idFd = open(idDirectory, O_CREAT | O_EXCL | O_WRONLY, S_IRWXO | S_IRWXU);
    char* id;
    if(idFd == -1){
        id = new char[11];
        idFd = open(idDirectory, O_RDONLY);
        handleError(idFd);
        int success = read(idFd, id, sizeof(char)*11);
        handleError(success);
    }
    else{
        id = newId();
        int success = write(idFd, id, sizeof(char)*11);
        handleError(success);
    }
    close(idFd);
    delete[] idDirectory;
    return id;
}

void createMessagesDirectory(char* initialDirectory){
    char* messagesDirectory = new char[strlen(initialDirectory)+strlen(MESSAGES_PATH)];
    strcpy(messagesDirectory, initialDirectory);
    strcat(messagesDirectory, MESSAGES_PATH);

    int messagesDirSuccess = mkdir(messagesDirectory, S_IRWXO | S_IRWXU);
    if(messagesDirSuccess == -1 && errno != 17){
        handleError(-1);
    }
    delete[] messagesDirectory;
    std::cout << "Created messages directory!\n";
}


char* buildDirectory(passwd* user){
    char* directory = new char[strlen(user->pw_dir) + strlen(DIRECTORY_PATH)];
    strcpy(directory, user->pw_dir);
    strcat(directory, DIRECTORY_PATH);
    return directory;
}
char* processUser(passwd* user, int udpFd, bool isFirstMessage, chVec &localIDs){
    char* initialDirectory = buildDirectory(user);

    seteuid(user->pw_uid);

    const char* id = checkForId(initialDirectory);
    localIDs.push_back(id);
    createMessagesDirectory(initialDirectory);
    createFifo(initialDirectory, D_TO_A_PATH, false);
    char* fifoDirectory = createFifo(initialDirectory, A_TO_D_PATH, true);

    seteuid(getuid());

    delete[] initialDirectory;


    //send udp
    sendIpMulticast(udpFd, id, isFirstMessage);

    return fifoDirectory;
}

//as I need to retrieve the sending address, I need to translate it to use recvmsg() instead of read()
void translateUdp(pollVec &toRead, addrToFd &addressToFd, chToInt &remote, chVec localIDs, chToInt fifos, chVec &remoteIDs){
    sockaddr_in* address = new sockaddr_in;
    socklen_t* size = new socklen_t;
    *size = sizeof(sockaddr_in);

    void* message = operator new(SIZE_MULTICAST);

    int success = recvfrom(toRead[1].fd, message, SIZE_MULTICAST, 0, (sockaddr*)address, size);
    handleError(success);

    processMulticastMessage(message, address, toRead, addressToFd, remote, localIDs, fifos, remoteIDs);
    operator delete(message);
}

void translateListeningTcp(pollVec &toRead, addrToFd &addressToFd){
    pollfd newConnection;
    sockaddr_in* newPeer = new sockaddr_in;
    socklen_t* size = new socklen_t;
    *size = sizeof(sockaddr_in);

    newConnection.fd = accept(toRead[0].fd, (sockaddr*)newPeer, size);
    newConnection.events = POLLIN;
                    
    toRead.push_back(newConnection);
    addressToFd[*newPeer] = newConnection.fd;

    std::cout << newConnection.fd << '\n';
}





int main(){
    pollVec toRead; //for poll
    chVec requestedPeers;//indicates peers that requested contacting with another peer
    chVec localIDs;//IDs from this device
    chVec remoteIDs;//IDs from peers that are online
    chToInt local; //int for representing file descriptors
    chToInt remote; //int for representing, again, socket file descriptors
    addrToFd addressToFd;//from addresses to (possible)file descriptors
    

    //open udp socket
    pollfd listeningSock = newtcpSocket(true);
    toRead.push_back(listeningSock);
    int listeningSuccess = listen(listeningSock.fd, 0);
    handleError(listeningSuccess);

    pollfd udpSocket = newudpSocket();
    toRead.push_back(udpSocket);

    


    // check for id and create it for every user
    passwd* user = getpwent();
    bool isFirstMessage = true;
    while(user != nullptr){
        if(user->pw_uid >= 1000 && user->pw_uid <= 59999){
            char* fifoDir = processUser(user, udpSocket.fd, isFirstMessage, localIDs);

            int fifoOpenSucc = open(fifoDir, O_RDONLY | O_NONBLOCK);
            handleError(fifoOpenSucc);

            pollfd fifo;
            fifo.fd = fifoOpenSucc;
            fifo.events = POLLIN;

            toRead.push_back(fifo);
            std::cout << "Pushed back!\n";
            isFirstMessage = false;
        }
        errno = 0;
        user = getpwent();
        if(errno != 0){
            handleError(-1);
        }
    }

    std::cout << "Current size: " << toRead.size() << '\n';
    std::cout << "----------------------------\n";

    while(true){
        int success = poll(toRead.data(), toRead.size(), -1);
        handleError(success);

        std::cout << "Current size on polling: " << toRead.size() << '\n';

        


        for(int i = 0; i < toRead.size(); i++){
            if(toRead[i].revents != 0){
                std::cout << i << " polled\n";
            }
            if((toRead[i].revents & POLLIN) == POLLIN){
                //for reading tcp socket
                if(i == 0){
                    std::cout << "Listening TCP polled\n";
                    translateListeningTcp(toRead, addressToFd);
                    continue;
                }
                else if(i == 1){
                    std::cout << "UDP polled\n";
                    translateUdp(toRead, addressToFd, remote, localIDs, local, remoteIDs);
                    continue;
                }


                void* message = operator new(MAX_SIZE);
                int readingSuccess = read(toRead[i].fd, message, MAX_SIZE);
                handleError(readingSuccess);
                if(readingSuccess == 0){
                    int closingSuccess = close(toRead[i].fd);
                    toRead.erase(toRead.begin()+i);
                    handleError(closingSuccess);
                    std::cout << "Peer is leaving!\n";
                }
                if(*(short*)message == 0){
                    //fifo
                    std::cout << "FIFO polled\n";
                    processFifo((short*)message + 1, local, requestedPeers, remote, remoteIDs);
                }
                else if(*(short*)message == 1){
                    std::cout << "TCP polled\n";
                    //socket
                    processTcp((short*)message + 1, requestedPeers, local);
                }
                std::cout << "Data available\n";
                
                operator delete(message);
            }
            if((toRead[i].revents & POLLHUP) == POLLHUP){
                close(toRead[i].fd);
                
                toRead.erase(toRead.begin() + i);
                std::cout << "Hung up!\n";
            }
            if((toRead[i].revents & POLLERR) == POLLERR){
                handleError(-1);
            }
            if((toRead[i].revents & POLLNVAL) == POLLNVAL){
                std::cout << "Invalid fd!\n" << i << '\n';
                toRead.erase(toRead.begin()+i);
            }
        }
        std::cout << "----------------------------\n";
    }


    for(int i = 0; i < toRead.size(); i++){
        int success = close(toRead[i].fd);
        handleError(success);
    }
    
    
    return 0;
}