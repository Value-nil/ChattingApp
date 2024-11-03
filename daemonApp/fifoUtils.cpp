#include "fifoUtils.h"
#include "daemonTypes.h"
#include "../common/utilities.h"
#include "daemonConstants.h"


#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <iostream>
#include <stdio.h>

extern pollVec toRead;


static const char* createFifo(deviceid_t id, bool isReading){
    const char* fullFifoPath = getFifoPath(id, isReading);
    std::cout << "Creating fifo on: " << fullFifoPath << '\n';

    int success = mkfifo(fullFifoPath, isReading? S_IWUSR | S_IRUSR: S_IRUSR);
    handleError(success);
    int success2 = chown(fullFifoPath, (uid_t)id, (gid_t)-1);
    handleError(success2);

    return fullFifoPath;
}


void createUserFifos(uid_t userId){
    //reading fifo
    const char* rFifoPath = createFifo((deviceid_t)userId, true);
    int readingFd = openFifo(rFifoPath, O_RDONLY | O_NONBLOCK);

    pollfd fifo;
    fifo.fd = readingFd;
    fifo.events = POLLIN;

    toRead.push_back(fifo);

    //writing fifo
    createFifo((deviceid_t)userId, false);
}

static inline void removeUserFifos(uid_t id){
    const char* stringifiedId = stringifyId(id);

    const char* rFifoPath = getFifoPath(id, true);
    std::cout << "unlinking " << rFifoPath << '\n';
    unlink(rFifoPath);

    const char* wFifoPath = getFifoPath(id, false);
    std::cout << "unlinking " << wFifoPath << '\n';
    unlink(wFifoPath);

    delete[] stringifiedId;
}


void restartUserFifos(uid_t userId){
    std::cout << "Restarting fifos for id " << userId << '\n';
    removeUserFifos(userId);
    createUserFifos(userId);
}
