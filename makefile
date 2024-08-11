VPATH = daemonApp common desktopApp desktopApp/guiFiles

DAEMON_TARGETS ::= cmpFuncs.o daemon.o daemonConstants.o processMessage.o socketUtils.o udp.o utilities.o constants.o fifoUtils.o
MAIN_APP_TARGETS ::= mainApp.o utilities.o constants.o cmpFuncs.o
GUI_FILES_LOCATION ::= /usr/local/share/chattingApp/guiFiles
GUI_FILES_TARGETS ::= conversationContainerGui.xml mainGui.xml message.xml

all: chattingappd chattingApp

chattingappd: $(DAEMON_TARGETS)
	g++ $(DAEMON_TARGETS) -o chattingappd

cmpFuncs.o: cmpFuncs.cpp cmpFuncs.h

daemon.o: daemon.cpp cmpFuncs.o udp.o processMessage.o daemonConstants.o socketUtils.o utilities.o constants.o daemonTypes.h fifoUtils.o

daemonConstants.o: daemonConstants.cpp daemonConstants.h 

processMessage.o: processMessage.cpp processMessage.h cmpFuncs.o utilities.o constants.o daemonConstants.o daemonTypes.h fifoUtils.o

socketUtils.o: socketUtils.cpp socketUtils.h utilities.o daemonConstants.o

udp.o: udp.cpp udp.h utilities.o cmpFuncs.o socketUtils.o daemonConstants.o daemonTypes.h

utilities.o: utilities.cpp utilities.h constants.o

constants.o: constants.cpp constants.h

fifoUtils.o: fifoUtils.cpp fifoUtils.h daemonTypes.h utilities.o daemonConstants.o


chattingApp: $(MAIN_APP_TARGETS)
	g++ $(MAIN_APP_TARGETS) -o chattingApp `pkg-config --libs gtk4`

mainApp.o: main.cpp
	g++ -c -o mainApp.o desktopApp/main.cpp `pkg-config --cflags gtk4`


install: gui_files daemon_install main_app_install

gui_files: $(patsubst %.xml,$(GUI_FILES_LOCATION)/%.xml,$(GUI_FILES_TARGETS))

$(GUI_FILES_LOCATION)/%.xml: desktopApp/guiFiles/%.xml
	mkdir -p /usr/local/share/chattingApp/guiFiles
	cp $? -t /usr/local/share/chattingApp/guiFiles -r

daemon_install: chattingappd chattingappd.service
	cp chattingappd /usr/local/sbin
	mkdir /usr/local/lib/systemd/system -p
	cp chattingappd.service /usr/local/lib/systemd/system
	systemctl enable /usr/local/lib/systemd/system/chattingappd.service
	systemctl restart chattingappd.service

main_app_install: chattingApp
	cp chattingApp /usr/local/bin