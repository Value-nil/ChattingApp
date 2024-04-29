#include "fifoUtilities.h"
#include <pwd.h>
#include <string.h>
#include <stdlib.h>

void handleError(int returnInt){
    if(returnInt < 0){
        std::cerr << "Error! Code: " << errno << '\n';
        exit(EXIT_FAILURE);
    }
}

const char* getFifoDir(struct passwd* user, const char* fifoDir){
    char* fifoFullDir = new char[sizeof(char)*(strlen(user->pw_dir)+strlen(fifoDir)+strlen(DIRECTORY_PATH))];
    strcpy(fifoFullDir, user->pw_dir);
    strcat(fifoFullDir, DIRECTORY_PATH);
    strcat(fifoFullDir, fifoDir);

    std::cout << "Fifo dir is: " << fifoFullDir << '\n';

    return (const char*)fifoFullDir;
}