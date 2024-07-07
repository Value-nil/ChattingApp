#include "types.h"
#include "udp.h"
#include "processMessage.h"
#include "daemonConstants.h"
#include "socketUtils.h"

#include "../common/utilities.h"
#include "../common/constants.h"



#include <netinet/in.h>
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

char* createFifo(const char* initialDirPath, const char* fifoPath, bool shouldReturn){
    char* fullFifoPath = buildPath(initialDirPath, fifoPath);

    cout << "Fifo path is: " << fullFifoPath << '\n';

    int fifoSuccess = mkfifo(fullFifoPath, ACCESS_MODE);
    if(fifoSuccess == -1 && errno != 17)
        handleError(-1);


    if(shouldReturn)
        return fullFifoPath;
    
    delete[] fullFifoPath;
    return nullptr;
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

char* setupUserDirectory(passwd* user){
    char* initialDirPath = createInitialDir(user);

    const char* id = checkForId(initialDirPath);
    localIDs.push_back(id);
    createMsgDir(initialDirPath);
    createFifo(initialDirPath, D_TO_A_PATH, false);
    char* fifoPath = createFifo(initialDirPath, A_TO_D_PATH, true);

    delete[] initialDirPath;

    return fifoPath;
}


int processUser(passwd* user, int udpFd, bool isFirstMessage){
    
    char* fifoDir = setupUserDirectory(user);

    //need to send isFirstMessage because when a peer receives just one udp message from a new peer, it will send all of its local users
    sendNewUserNotification(udpFd, *(localIDs.end()-1), isFirstMessage);

    int fifoFd = open(fifoDir, O_RDONLY | O_NONBLOCK);
    handleError(fifoFd);

    return fifoFd;
}

//as I need to retrieve the sending address, I need to translate it to use recvmsg() instead of read()
void translateUdp(){
    sockaddr_in* address = new sockaddr_in;
    socklen_t* size = new socklen_t;
    *size = sizeof(sockaddr_in);

    int udpSocket = toRead[1].fd; //second fd of polling vector is udp socket

    void* message = operator new(SIZE_MULTICAST);

    int success = recvfrom(udpSocket, message, SIZE_MULTICAST, 0, (sockaddr*)address, size);//0 for flags
    handleError(success);

    processUdpMessage(message, address);
    operator delete(message);
}

void translateListeningTcp(){
    pollfd newConnection;
    int listeningTcpSocket = toRead[0].fd;    //first fd of polling vector is listening tcp

    sockaddr_in* newPeer = new sockaddr_in;
    socklen_t* size = new socklen_t;
    *size = sizeof(sockaddr_in);

    newConnection.fd = accept(listeningTcpSocket, (sockaddr*)newPeer, size);
    newConnection.events = POLLIN;

    handleError(newConnection.fd);
                    
    toRead.push_back(newConnection);
    addressToFd[*newPeer] = newConnection.fd;//for other users a device might have

    cout << newConnection.fd << '\n';
}


void checkUsers(pollfd udpSocket){
    passwd* user = getpwent();
    bool isFirstMessage = true;
    while(user != nullptr){
        if(user->pw_uid >= 1000 && user->pw_uid <= 59999){//checking for actually created users
            int fifoFd = processUser(user, udpSocket.fd, isFirstMessage);

            pollfd fifo;
            fifo.fd = fifoFd;
            fifo.events = POLLIN;

            toRead.push_back(fifo);
            isFirstMessage = false;
        }
        //error handling for every user(making sure error handling is independent from every other user)
        errno = 0;
        user = getpwent();
        if(errno != 0){
            handleError(-1);
        }
    }
}


void processNormalPolling(int index){
    int fd = toRead[index].fd;

    void* message = operator new(MAX_SIZE);
    int readBytes = read(fd, message, MAX_SIZE);
    handleError(readBytes);

    if(readBytes == 0){
        int closingSuccess = close(fd);
        toRead.erase(toRead.begin()+index);

        handleError(closingSuccess);

        cout << "Peer is leaving!\n";
    }
    if(*(short*)message == 0){
        //fifo
        cout << "FIFO polled\n";
        processFifo((short*)message + 1);
    }
    else if(*(short*)message == 1){
        cout << "TCP polled\n";
        //socket
        processTcp((short*)message + 1);
    }
    cout << "Data available\n";
                
    operator delete(message);
}



int main(){
    pollfd listeningSock = newListeningTcpSocket();
    toRead.push_back(listeningSock);

    pollfd udpSocket = newudpSocket();
    toRead.push_back(udpSocket);



    checkUsers(udpSocket);

    cout << "Current size: " << toRead.size() << '\n';
    cout << "----------------------------\n";


    //main loop of polling
    while(true){
        int success = poll(toRead.data(), toRead.size(), -1);
        handleError(success);

        cout << "Current size on polling: " << toRead.size() << '\n';

        


        for(int i = 0; i < toRead.size(); i++){
            short revents = toRead[i].revents;
            if(revents != 0){
                cout << i << " polled\n";
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
    }


    for(int i = 0; i < toRead.size(); i++){
        int success = close(toRead[i].fd);
        handleError(success);
    }
    
    
    return 0;
}