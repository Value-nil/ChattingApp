#include "daemonTypes.h"
#include "udp.h"
#include "processMessage.h"
#include "daemonConstants.h"
#include "socketUtils.h"
#include "fifoUtils.h"

#include "../common/utilities.h"
#include "../common/constants.h"



#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/random.h>
#include <sys/stat.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <new>
#include <pwd.h>
#include <errno.h>
#include <iostream>
#include <poll.h>
#include <signal.h>
#include <stdlib.h>


pollVec toRead; //for poll; includes readable socket fds
chVec requestedPeers;//indicates peers that requested contacting with another peer
chVec localIDs;//IDs from this device
chVec remoteIDs;//IDs from peers that are online
chToInt localUsers; //local user id to fifo fd
chToInt remoteUsers; //remote user id to writeable socket fd
addrToFd addressToFd;//from addresses to (possible)file descriptors


using namespace std;



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

char* retrieveId(char* idPath){
    char* id = new char[11];

    int idFd = open(idPath, O_RDONLY);
    handleError(idFd);

    int success = read(idFd, id, sizeof(char)*11);
    handleError(success);

    return id;
}

void createFifoDirectory(){
    int success = mkdir(FIFO_PATH, 0777);
    cout << "Created fifo directory on /tmp\n";
    handleError(success);
}



char* checkForId(char* initialDirPath){
    char* idPath = buildPath(initialDirPath, ID_PATH);

    int idFd = open(idPath, O_CREAT | O_EXCL | O_WRONLY, ACCESS_MODE);
    char* id;
    if(idFd == -1 && errno == 17){
        id = retrieveId(idPath);
    }
    else if(idFd == -1) handleError(idFd);
    else{
        id = newId();
        int success = write(idFd, id, sizeof(char)*11);
        handleError(success);
    }
    close(idFd);
    delete[] idPath;
    return id;
}

void createMsgDir(char* initialDirPath){
    char* messagesDirPath = buildPath(initialDirPath, MESSAGES_PATH);

    int messagesDirSuccess = mkdir(messagesDirPath, ACCESS_MODE);
    if(messagesDirSuccess == -1 && errno != 17){
        handleError(-1);
    }
    delete[] messagesDirPath;
    cout << "Created messages directory!\n";
}


char* createInitialDir(passwd* user){
    char* path = buildPath(user->pw_dir, INITIAL_DIR_PATH);

    int mkdirSuccess = mkdir(path, ACCESS_MODE);
    if(mkdirSuccess == -1 && errno != 17){
        handleError(-1);
    }

    return path;
}

void setupUserDirectory(passwd* user){
    seteuid(user->pw_uid);

    char* initialDirPath = createInitialDir(user);

    const char* id = checkForId(initialDirPath);
    localIDs.push_back(id);
    std::cout << "Initializing " << id << " directory\n";
    createUserFifos(id, user->pw_uid);

    createMsgDir(initialDirPath);

    seteuid(getuid());

    delete[] initialDirPath;
}

void processUser(passwd* user){
    setupUserDirectory(user);
}

//as I need to retrieve the sending address, I need to translate it to use recvfrom() instead of read()
void translateUdp(){
    sockaddr_in* address = new sockaddr_in;
    socklen_t size = sizeof(sockaddr_in);

    int udpSocket = toRead[1].fd; //second fd of polling vector is udp socket
    char a;
    int success = recvfrom(udpSocket, (void*)&a, sizeof(char), 0, (sockaddr*)address, &size);//0 for flags
    handleError(success);

    processUdpMessage(address);

    delete address;
}

void translateListeningTcp(){
    pollfd newConnection;
    int listeningTcpSocket = toRead[0].fd;    //first fd of polling vector is listening tcp

    sockaddr_in* newPeer = new sockaddr_in;
    socklen_t size = sizeof(sockaddr_in);
    newConnection.fd = accept(listeningTcpSocket, (sockaddr*)newPeer, &size);
    newConnection.events = POLLIN;

    cout << "Address of peer: " << inet_ntoa(newPeer->sin_addr) << '\n';

    handleError(newConnection.fd);
                    
    toRead.push_back(newConnection);
    addressToFd[newPeer->sin_addr] = newConnection.fd;//for other users a device might have

    sendLocalContacts(newConnection.fd);

    cout << "Accepted new TCP socket: " << newConnection.fd;

    delete newPeer;
}


void checkUsers(pollfd udpSocket){
    passwd* user = getpwent();
    while(user != nullptr){
        if(user->pw_uid >= 1000 && user->pw_uid <= 59999){//checking for actually created users
            processUser(user);
        }
        //error handling for every user(making sure error handling is independent from every other user)
        errno = 0;
        user = getpwent();
        if(errno != 0){
            handleError(-1);
        }
    }
}

void processNewData(int fd, size_t msgSize){
    void* message = operator new(msgSize);
    size_t readBytes = read(fd, message, msgSize);
    cout << "The message intended size is " << msgSize << '\n';
    cout << "Read bytes: " << readBytes << '\n';
    handleError(readBytes);

    if(*(short*)message == 0){
        cout << "FIFO polled\n";
        processFifo((short*)message + 1);
    }
    else if(*(short*)message == 1){
        cout << "TCP polled\n";
        processTcp((short*)message + 1, fd);
    }
    operator delete(message);
}

void deleteAddress(int fd){
    sockaddr_in* leavingAddr = new sockaddr_in;
    socklen_t size = sizeof(sockaddr_in);

    int peerNameSuccess = getpeername(fd, (sockaddr*)leavingAddr, &size);
    handleError(peerNameSuccess);

    addressToFd.erase(leavingAddr->sin_addr);
        
    cout << "Leaving address: " << inet_ntoa(leavingAddr->sin_addr) << '\n';

    delete leavingAddr;
}


void processNormalPolling(int index){
    int fd = toRead[index].fd;

    size_t msgSize;
    int success = read(fd, (void*)(&msgSize), sizeof(size_t));
    handleError(success);

    if(success > 0){
        processNewData(fd, msgSize);
    }
    else{
        cout << "Peer is leaving!\n";

        deleteAddress(fd);
        
        int closingSuccess = close(fd);
        handleError(closingSuccess);
        toRead.erase(toRead.begin()+index);    
    }
}


void sendClosingMessage(int signal){
    size_t sizeOfMsg = sizeof(short)*2+sizeof(char)*11;

    void* closingMessage = operator new(sizeof(size_t) + sizeOfMsg);
    void* toSend = closingMessage;

    *(size_t*)closingMessage = sizeOfMsg;
    closingMessage = (size_t*)closingMessage + 1;
    *(short*)closingMessage = 1;
    closingMessage = (short*)closingMessage + 1;
    *(short*)closingMessage = 4;
    closingMessage = (short*)closingMessage + 1;

    cout << "Stopping app\n" << flush;

    for(unsigned int i = 0; i < localIDs.size(); i++){
        strcpy((char*)closingMessage, localIDs[i]);
        for(auto j = addressToFd.begin(); j != addressToFd.end(); j++){
            int fd = (*j).second;
            if(fd != 0){
                int success = write(fd, toSend, sizeof(size_t)+ sizeOfMsg);
                handleError(success);
            }
        }
    }

    cout << "Finished stopping, exiting\n" << flush;

    exit(EXIT_SUCCESS);
}

void connectTermSignal(){
    struct sigaction* action = new struct sigaction;
    action->sa_handler = &sendClosingMessage;
    action->sa_flags = 0;

    int success = sigaction(SIGTERM, action, nullptr);
    handleError(success);

}

int main(){
    pollfd listeningSock = newListeningTcpSocket();
    toRead.push_back(listeningSock);

    pollfd udpSocket = newudpSocket();
    toRead.push_back(udpSocket);


    createFifoDirectory();
    checkUsers(udpSocket);
    sendNewDeviceNotification(udpSocket.fd);
    connectTermSignal();

    cout << "finished initialization\n" << std::flush;
    //main loop of polling
    while(true){
        int success = poll(toRead.data(), toRead.size(), -1);
        handleError(success);

        cout << "Current size on polling: " << toRead.size() << '\n';

        for(unsigned int i = 0; i < toRead.size(); i++){
            short revents = toRead[i].revents;
            if(revents != 0){
                cout << toRead[i].fd << " fd polled\n";
            }


            if((revents & POLLIN) == POLLIN){
                //for detecting listening tcp and udp, respectively
                if(i == 0){
                    cout << "Listening TCP polled\n";
                    translateListeningTcp();
                }
                else if(i == 1){
                    cout << "UDP polled\n";
                    translateUdp();
                }
                else{
                    processNormalPolling(i);
                }


                
            }
            if((revents & POLLHUP) == POLLHUP){
                close(toRead[i].fd);
                
                toRead.erase(toRead.begin() + i);
                cout << "Hung up!\n";
            }
            if((revents & POLLERR) == POLLERR){
                handleError(-1);
            }
            if((revents & POLLNVAL) == POLLNVAL){
                cout << "Invalid fd!\n" << i << '\n';
                toRead.erase(toRead.begin()+i);
            }
        }
        cout << "----------------------------\n";
        cout << std::flush;
    }


    for(unsigned int i = 0; i < toRead.size(); i++){
        int success = close(toRead[i].fd);
        handleError(success);
    }
    
    
    return 0;
}
