#ifndef FIFO_UTILITES
#define FIFO_UTILITES

void handleError(int returnInt);
const char* getFifoPath(struct passwd* user, const char* fifoPath);
char* buildPath(const char* basePath, const char* relativePath);
#endif