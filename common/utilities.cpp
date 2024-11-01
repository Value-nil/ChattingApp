#include "utilities.h"
#include "constants.h"

#include <pwd.h>
#include <string.h>
#include <stdlib.h>
#include <iostream>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>


char* stringifyId(deviceid_t id){
    char* buffer = new char[21];
    snprintf(buffer, 21, "%llu", id);
    std::cout << "Stringified ID is " << buffer << '\n';
    return buffer;
}

deviceid_t unstringifyId(const char* id){
    return strtoull(id, nullptr, 10);
}

void handleError(int returnInt){
    if(returnInt < 0){
        std::cerr << "Error! Code: " << errno << '\n';
        exit(EXIT_FAILURE);
    }
}

const char* getFifoPath(deviceid_t id, bool isAppToD){
    const char* stringifiedId = stringifyId(id);
    const char* fifoIdPath = buildPath(FIFO_PATH, stringifiedId);
    const char* fullFifoPath = buildPath(fifoIdPath, isAppToD? "_r" : "_w");
    delete[] stringifiedId;
    delete[] fifoIdPath;

    return fullFifoPath;
}

char* buildPath(const char* basePath, const char* relativePath){
    char* fullPath = new char[strlen(basePath) + strlen(relativePath) + 1];//1 for the null pointer
    strcpy(fullPath, basePath);
    strcat(fullPath, relativePath);

    return fullPath;
}

int openFifo(const char* path, int mode){
    int fd = open(path, mode);
    handleError(fd);
    return fd;
}

const char* getMessageDirectoryPath(deviceid_t userId){
    struct passwd* passwdStruct = getpwuid((uid_t)userId);
    const char* baseDirPath = buildPath(passwdStruct->pw_dir, INITIAL_DIR_PATH);
    const char* messageDirPath = buildPath(baseDirPath, MESSAGES_PATH);
    delete[] baseDirPath;

    return messageDirPath;
}
const char* getMessageFilePath(deviceid_t userId, deviceid_t peerId){
    const char* messageDirPath = getMessageDirectoryPath(userId);
    const char* stringifiedPeerId = stringifyId(peerId);
    const char* fullPath = buildPath(messageDirPath, stringifiedPeerId);

    delete[] messageDirPath;
    delete[] stringifiedPeerId;

    return fullPath;
}
