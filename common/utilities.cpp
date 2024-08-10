#include "utilities.h"
#include "constants.h"
#include <pwd.h>
#include <string.h>
#include <stdlib.h>
#include <iostream>
#include <errno.h>
#include <fcntl.h>

void handleError(int returnInt){
    if(returnInt < 0){
        std::cerr << "Error! Code: " << errno << '\n';
        exit(EXIT_FAILURE);
    }
}

const char* getFifoPath(const char* id, bool isAppToD){
    const char* fifoIdPath = buildPath(FIFO_PATH, id);
    const char* fullFifoPath = buildPath(fifoIdPath, isAppToD? "_r" : "_w");
    delete[] fifoIdPath;

    return fullFifoPath;
}

char* buildPath(const char* basePath, const char* relativePath){
    char* fullPath = new char[strlen(basePath) + strlen(relativePath)];
    strcpy(fullPath, basePath);
    strcat(fullPath, relativePath);

    return fullPath;
}

int openFifo(const char* path, int mode){
    int fd = open(path, mode);
    handleError(fd);
    return fd;
}