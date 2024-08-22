#ifndef COMMON_UTILITIES
#define COMMON_UTILITIES

void handleError(int returnInt);
const char* getFifoPath(const char* id, bool isAppToD);
char* buildPath(const char* basePath, const char* relativePath);
int openFifo(const char* path, int mode);
#endif