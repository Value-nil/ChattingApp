#include "fifoUtils.h"
#include "daemonTypes.h"
#include "../common/utilities.h"
#include "daemonConstants.h"


#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <iostream>

extern pollVec toRead;


const char* createFifo(const char* id, bool isReading){
    const char* fullFifoPath = getFifoPath(id, isReading);
    std::cout << "Creating fifo on: " << fullFifoPath << '\n';
    int success = mkfifo(fullFifoPath, ACCESS_MODE);
    handleError(success);
    return fullFifoPath;
}

void createUserFifos(const char* id, uid_t userId){
    seteuid(userId);

    //reading fifo
    const char* rFifoPath = createFifo(id, true);
    int readingFd = openFifo(rFifoPath, O_RDONLY | O_NONBLOCK);

    pollfd fifo;
    fifo.fd = readingFd;
    fifo.events = POLLIN;

    toRead.push_back(fifo);

    //writing fifo
    createFifo(id, false);

    seteuid(getuid());
}

void removeUserFifos(const char* id){
    const char* rFifoPath = getFifoPath(id, true);
    std::cout << "unlinking " << rFifoPath << '\n';
    unlink(rFifoPath);

    const char* wFifoPath = getFifoPath(id, false);
    std::cout << "unlinking " << wFifoPath << '\n';
    unlink(wFifoPath);
}


void restartUserFifos(const char* id, uid_t userId){
    std::cout << "Restarting fifos for id " << id << '\n';
    removeUserFifos(id);
    createUserFifos(id, userId);
}