VPATH = daemonApp common desktopApp

DAEMON_TARGETS ::= cmpFuncs.o daemon.o daemonConstants.o processMessage.o socketUtils.o udp.o utilities.o constants.o

daemon: $(DAEMON_TARGETS)
	g++ $(DAEMON_TARGETS) -o daemon

cmpFuncs.o: cmpFuncs.cpp types.h

daemon.o: daemon.cpp cmpFuncs.o udp.o processMessage.o daemonConstants.o socketUtils.o utilities.o constants.o

daemonConstants.o: daemonConstants.cpp daemonConstants.h 

processMessage.o: processMessage.cpp processMessage.h cmpFuncs.o utilities.o constants.o daemonConstants.o

socketUtils.o: socketUtils.cpp socketUtils.h utilities.o daemonConstants.o

udp.o: udp.cpp udp.h utilities.o cmpFuncs.o socketUtils.o daemonConstants.o

utilities.o: utilities.cpp utilities.h constants.o

constants.o: constants.cpp constants.h

