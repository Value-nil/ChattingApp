VPATH = daemonApp common desktopApp desktopApp/guiFiles

DAEMON_TARGETS ::= cmpFuncs.o daemon.o daemonConstants.o processMessage.o socketUtils.o udp.o utilities.o constants.o fifoUtils.o
MAIN_APP_TARGETS ::= mainApp.o utilities.o constants.o cmpFuncs.o
GUI_FILES_TARGETS ::= conversationContainerGui.xml mainGui.xml message.xml

CC ::= g++
CXX ::= g++
CXXFLAGS ::= -Wall -Wextra -O2
LDFLAGS ::= 


PREFIX ::= /usr/local
GUI_FILES_LOCATION ::= $(PREFIX)/share/chattingApp/guiFiles
DAEMON_LOCATION ::= $(PREFIX)/sbin
MAIN_APP_LOCATION ::= $(PREFIX)/bin
UNIT_FILE_LOCATION ::= $(PREFIX)/lib/systemd/system

all: chattingappd chattingApp

chattingappd: $(DAEMON_TARGETS)
	$(CC) $(DAEMON_TARGETS) $(LDFLAGS) -o chattingappd

cmpFuncs.o: cmpFuncs.cpp cmpFuncs.h

daemon.o: daemon.cpp cmpFuncs.o udp.o processMessage.o daemonConstants.o socketUtils.o utilities.o constants.o daemonTypes.h fifoUtils.o

daemonConstants.o: daemonConstants.cpp daemonConstants.h 

processMessage.o: processMessage.cpp processMessage.h cmpFuncs.o utilities.o constants.o daemonConstants.o daemonTypes.h fifoUtils.o

socketUtils.o: socketUtils.cpp socketUtils.h utilities.o daemonConstants.o

udp.o: udp.cpp udp.h utilities.o cmpFuncs.o socketUtils.o daemonConstants.o daemonTypes.h

utilities.o: utilities.cpp utilities.h constants.o

constants.o: constants.cpp constants.h

fifoUtils.o: fifoUtils.cpp fifoUtils.h daemonTypes.h utilities.o daemonConstants.o

chattingApp: LDFLAGS ::= `pkg-config --libs gtk4`
chattingApp: $(MAIN_APP_TARGETS)
	$(CC) $(MAIN_APP_TARGETS) $(LDFLAGS) -o chattingApp


mainApp.o: CXXFLAGS ::= `pkg-config --cflags gtk4` $(CXXFLAGS)
mainApp.o: main.cpp 
	$(CXX) -c desktopApp/main.cpp $(CXXFLAGS) -o mainApp.o


install: gui_files daemon_install main_app_install

gui_files: $(patsubst %.xml,$(GUI_FILES_LOCATION)/%.xml,$(GUI_FILES_TARGETS))

$(GUI_FILES_LOCATION)/%.xml: desktopApp/guiFiles/%.xml
	mkdir -p /usr/local/share/chattingApp/guiFiles
	cp $? -t /usr/local/share/chattingApp/guiFiles -r



daemon_install: $(DAEMON_LOCATION)/chattingappd $(UNIT_FILE_LOCATION)/chattingappd.service

$(DAEMON_LOCATION)/chattingappd : chattingappd
	cp chattingappd $(DAEMON_LOCATION) 
	
$(UNIT_FILE_LOCATION)/chattingappd.service : chattingappd.service $(DAEMON_LOCATION)/chattingappd
	mkdir $(UNIT_FILE_LOCATION) -p
	cp chattingappd.service $(UNIT_FILE_LOCATION)
	systemctl enable chattingappd.service
	systemctl restart chattingappd.service



main_app_install: $(MAIN_APP_LOCATION)/chattingApp

$(MAIN_APP_LOCATION)/chattingApp : chattingApp
	cp chattingApp $(MAIN_APP_LOCATION)

uninstall:
	-rm $(MAIN_APP_LOCATION)/chattingApp
	-systemctl stop chattingappd
	-systemctl disable chattingappd
	-rm $(UNIT_FILE_LOCATION)/chattingappd.service
	-rm $(DAEMON_LOCATION)/chattingappd
	-rm -r $(GUI_FILES_LOCATION)

.PHONY = clean
clean:
	-rm *.o
