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
idVec requestedPeers;//indicates peers that requested contacting with another peer
idToFd localUsers; //local user id to fifo fd
idToFd remoteDevices; //remote device id to writeable socket fd
deviceid_t deviceId;//an id unique for each device


using namespace std;



void createNewId(){
    int success = getrandom(&deviceId, sizeof(deviceid_t), 0);
    handleError(success);

    deviceId = deviceId & (~USER_PART); //the upper half is used for the device id; the lower part is used for the user id
}

void createFifoDirectory(){
    int success = mkdir(FIFO_PATH, S_IRWXU | S_IRWXG | S_IRWXO);
    handleError(success);
    cout << "Created fifo directory on /tmp\n";
}

void checkIdDirectory(){
    int success = mkdir(ID_DIRECTORY_PATH, S_IRWXU);
    if(success == -1 && errno != EEXIST)
	handleError(-1);
}

void checkForId(){
    checkIdDirectory();
    char* idPath = buildPath(ID_DIRECTORY_PATH, ID_PATH);

    int idFd = open(idPath, O_CREAT | O_EXCL | O_WRONLY, S_IRWXU);
    if(idFd == -1 && errno == 17){
	idFd = open(idPath, O_RDONLY);
	handleError(idFd);

	int success = read(idFd, &deviceId, sizeof(deviceid_t));
	handleError(success);
    }
    else if(idFd == -1) handleError(idFd);
    else{
	createNewId();
        int success = write(idFd, &deviceId, sizeof(deviceid_t));
        handleError(success);
    }
    close(idFd);
    delete[] idPath;
}

void createMsgDir(char* initialDirPath){
    char* messagesDirPath = buildPath(initialDirPath, MESSAGES_PATH);

    int messagesDirSuccess = mkdir(messagesDirPath, S_IRWXU);
    if(messagesDirSuccess == -1 && errno != 17){
        handleError(-1);
    }
    delete[] messagesDirPath;
    cout << "Created messages directory!\n";
}


char* createInitialDir(passwd* user){
    char* path = buildPath(user->pw_dir, INITIAL_DIR_PATH);

    int mkdirSuccess = mkdir(path, S_IRWXU);
    if(mkdirSuccess == -1 && errno != 17){
        handleError(-1);
    }

    return path;
}

void setupUserDirectory(passwd* user){
    seteuid(user->pw_uid);

    char* initialDirPath = createInitialDir(user);

    localUsers[(deviceid_t)user->pw_uid] = 0;
    createUserFifos(user->pw_uid);

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
    deviceid_t remoteDeviceId;
    int success = recvfrom(udpSocket, (void*)&deviceId, sizeof(deviceId), 0, (sockaddr*)address, &size);//0 for flags
    handleError(success);

    processUdpMessage(remoteDeviceId, address);

    delete address;
}

void translateListeningTcp(){
    pollfd newConnection;
    int listeningTcpSocket = toRead[0].fd;    //first fd of polling vector is listening tcp

    newConnection.fd = accept(listeningTcpSocket, nullptr, nullptr);
    newConnection.events = POLLIN;
    handleError(newConnection.fd);
 
    toRead.push_back(newConnection);

    cout << "Accepted new TCP socket: " << newConnection.fd << '\n';
}


void checkUsers(){
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
    if(localUsers.size() == 0){
	cerr << "No users created on the system, exiting\n";
	exit(EXIT_FAILURE);
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
    for(auto iter = remoteDevices.begin(); iter != remoteDevices.end(); iter++){
	if((*iter).second == fd){
	    remoteDevices.erase((*iter).first);
	    break;
	}
    }
}


void processNormalPolling(int index){
    int fd = toRead[index].fd;

    size_t msgSize;
    int success = read(fd, (void*)(&msgSize), sizeof(size_t));
    handleError(success);

    if(success > 0){
        processNewData(fd, msgSize);
    }
    else{//tcp sends empty messages indicating leaving
        cout << "Peer is leaving!\n";
        deleteAddress(fd);
        
        int closingSuccess = close(fd);
        handleError(closingSuccess);
        toRead.erase(toRead.begin()+index);    
    }
}


void sendClosingMessage(int signal){
    size_t sizeOfMsg = sizeof(short)*2+sizeof(deviceid_t);

    void* closingMessage = operator new(sizeof(size_t) + sizeOfMsg);
    void* toSend = closingMessage;

    *(size_t*)closingMessage = sizeOfMsg;
    closingMessage = (size_t*)closingMessage + 1;
    *(short*)closingMessage = 1;
    closingMessage = (short*)closingMessage + 1;
    *(short*)closingMessage = 4;
    closingMessage = (short*)closingMessage + 1;
    *(deviceid_t*)closingMessage = deviceId;

    cout << "Stopping app\n" << flush;

    for(auto j = remoteDevices.begin(); j != remoteDevices.end(); j++){
        int fd = (*j).second;
        if(fd != 0){
            int success = write(fd, toSend, sizeof(size_t)+ sizeOfMsg);
            handleError(success);
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

    checkForId();
    createFifoDirectory();
    checkUsers();
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
    return 0;
}
