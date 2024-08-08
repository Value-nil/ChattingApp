VPATH = daemonApp common desktopApp

DAEMON_TARGETS ::= cmpFuncs.o daemon.o daemonConstants.o processMessage.o socketUtils.o udp.o utilities.o constants.o
MAIN_APP_TARGETS ::= mainApp.o utilities.o constants.o cmpFuncs.o

all: daemon mainApp

daemon: $(DAEMON_TARGETS)
	g++ $(DAEMON_TARGETS) -o daemon

cmpFuncs.o: cmpFuncs.cpp cmpFuncs.h

daemon.o: daemon.cpp cmpFuncs.o udp.o processMessage.o daemonConstants.o socketUtils.o utilities.o constants.o daemonTypes.h

daemonConstants.o: daemonConstants.cpp daemonConstants.h 

processMessage.o: processMessage.cpp processMessage.h cmpFuncs.o utilities.o constants.o daemonConstants.o daemonTypes.h

socketUtils.o: socketUtils.cpp socketUtils.h utilities.o daemonConstants.o

udp.o: udp.cpp udp.h utilities.o cmpFuncs.o socketUtils.o daemonConstants.o daemonTypes.h

utilities.o: utilities.cpp utilities.h constants.o

constants.o: constants.cpp constants.h


mainApp: $(MAIN_APP_TARGETS)
	g++ $(MAIN_APP_TARGETS) -o mainApp `pkg-config --libs gtk4`

mainApp.o: main.cpp
	g++ -c -o mainApp.o desktopApp/main.cpp `pkg-config --cflags gtk4`
