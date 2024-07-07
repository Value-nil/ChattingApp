#include "utilities.h"
#include "constants.h"
#include <pwd.h>
#include <string.h>
#include <stdlib.h>
#include <iostream>
#include <errno.h>

void handleError(int returnInt){
    if(returnInt < 0){
        std::cerr << "Error! Code: " << errno << '\n';
        exit(EXIT_FAILURE);
    }
}

const char* getFifoPath(struct passwd* user, const char* fifoPath){
    char* initialDirPath = buildPath(user->pw_dir, INITIAL_DIR_PATH);
    char* fullFifoPath = buildPath(initialDirPath, fifoPath);
    delete[] initialDirPath;

    return (const char*)fullFifoPath;
}

char* buildPath(const char* basePath, const char* relativePath){
    char* fullPath = new char[strlen(basePath) + strlen(relativePath)];
    strcpy(fullPath, basePath);
    strcat(fullPath, relativePath);

    return fullPath;
}