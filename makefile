all : mainApp daemon

mainApp : mainApp.o fifoUtilities.o conversations.o constants.o
	g++ fifoUtilities.o mainApp.o conversations.o constants.o `pkg-config --libs gtk4` -o mainApp
mainApp.o : common/fifoUtilities.h desktopApp/main.cpp desktopApp/conversations.h
	g++ desktopApp/main.cpp -c -o mainApp.o `pkg-config --cflags gtk4`
fifoUtilities.o : common/fifoUtilities.h common/fifoUtilities.cpp
	g++ common/fifoUtilities.cpp -c -o fifoUtilities.o
constants.o : common/constants.cpp
	g++ common/constants.cpp -c -o constants.o
conversations.o : desktopApp/conversations.cpp desktopApp/conversations.h
	g++ desktopApp/conversations.cpp -c -o conversations.o `pkg-config --cflags gtk4`

daemon : daemon.o fifoUtilities.o udp.o constants.o processMessage.o daemonApp/all.h
	g++ daemon.o fifoUtilities.o udp.o constants.o processMessage.o -o daemon
daemon.o : daemonApp/daemon.cpp daemonApp/all.h
	g++ daemonApp/daemon.cpp -c -o daemon.o
udp.o : daemonApp/udp.cpp daemonApp/udp.h
	g++ daemonApp/udp.cpp -c -o udp.o
processMessage.o : daemonApp/processMessage.cpp daemonApp/processMessage.h
	g++ daemonApp/processMessage.cpp -c -o processMessage.o