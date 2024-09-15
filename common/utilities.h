#ifndef COMMON_UTILITIES
#define COMMON_UTILITIES

#include "types.h"

char* stringifyId(deviceid_t id);
deviceid_t unstringifyId(char* id);
void handleError(int returnInt);
const char* getFifoPath(deviceid_t id, bool isAppToD);
char* buildPath(const char* basePath, const char* relativePath);
int openFifo(const char* path, int mode);
#endif
