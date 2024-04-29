#ifndef PROCESS_MESSAGE
#define PROCESS_MESSAGE
#include "all.h"
void processFifo(void* message, chToInt &fifos, chVec &requestedPeers, chToInt &remote, chVec remoteIDs);
void processTcp(void* message, chVec &requestedPeers, chToInt &fifos);

#endif