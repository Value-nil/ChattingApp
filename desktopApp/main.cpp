#include <gtk/gtk.h>
#include <glib-unix.h>

#include <iostream>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <new>
#include <string.h>
#include <errno.h>
#include <thread>
#include <poll.h>
#include <vector>
#include <map>
#include <mutex>
#include <pwd.h>
#include <dirent.h>
#include <time.h>

#include "../common/utilities.h"
#include "../common/constants.h"
#include "desktopTypes.h"

GtkBox* addSubnetPeerButtons;
GtkStack* conversationContainerStack;
int writingFifo;
idToBox messageBoxes;


const char* XML_MAIN_GUI_FILE = "/usr/local/share/chattingApp/guiFiles/mainGui.xml";
const char* XML_CONVERSATION_GUI_FILE = "/usr/local/share/chattingApp/guiFiles/conversationContainerGui.xml";
const char* XML_MESSAGE_GUI_FILE = "/usr/local/share/chattingApp/guiFiles/message.xml";





bool closeSecondaryRequest(GtkWindow* self, gpointer data){
    gtk_widget_set_visible((GtkWidget*)self, false);
    return true;
}

bool closeMainRequest(GtkWindow* self, gpointer data){
    GtkWindow* otherWindow = (GtkWindow*)data;
    gtk_window_close(otherWindow);
    return false;
}

void openSecondaryWindow(GtkButton* self, gpointer data){
    GtkWindow* window2 = (GtkWindow*)data;
    gtk_widget_set_visible((GtkWidget*)window2, true);
    g_signal_connect(window2, "close-request", (GCallback)closeSecondaryRequest, NULL);
}

void connectionClicked(GtkButton* self, gpointer data){
    deviceid_t peerId = *(deviceid_t*)data;
    deviceid_t id = (deviceid_t)getuid();
    std::cout << "Peer id is: " << peerId << '\n';

    size_t sizeOfMsg = sizeof(short)*2+sizeof(deviceid_t)*2;

    void* toWrite = operator new(sizeof(size_t) + sizeOfMsg);
    void* message = toWrite;

    *(size_t*)toWrite = sizeOfMsg;
    toWrite = (size_t*)toWrite + 1;
    *(short*)toWrite = 0;
    toWrite = (short*)toWrite+1;
    *(short*)toWrite = 1;
    toWrite = (short*)toWrite+1;
    *(deviceid_t*)toWrite = id;
    toWrite = (deviceid_t*)toWrite + 1;
    *(deviceid_t*)toWrite = peerId;
    

    int success = write(writingFifo, message, sizeof(size_t) + sizeOfMsg);
    handleError(success);

    operator delete(message);
}

GtkFrame* newMessage(const char* message){
    GtkBuilder* messageBuilder = gtk_builder_new_from_file(XML_MESSAGE_GUI_FILE);
    GtkTextBuffer* textBuffer = (GtkTextBuffer*)gtk_builder_get_object(messageBuilder, "textBuffer");
    gtk_text_buffer_set_text(textBuffer, message, -1);

    GtkFrame* messageFrame = (GtkFrame*)gtk_builder_get_object(messageBuilder, "message");
    return messageFrame;
}

GtkFrame* newIncomingMessage(const char* message){
    GtkFrame* messageFrame = newMessage(message);
    gtk_widget_set_margin_end((GtkWidget*)messageFrame, 30);

    return messageFrame;
}

GtkFrame* newSentMessage(const char* message){
    GtkFrame* messageFrame = newMessage(message);
    gtk_widget_set_margin_start((GtkWidget*)messageFrame, 30);

    return messageFrame;
}


GtkWindow* startGtk(){
    gtk_init();

    GtkBuilder* builder = gtk_builder_new_from_file(XML_MAIN_GUI_FILE);
    GtkWindow* window = GTK_WINDOW(gtk_builder_get_object(builder, "window"));
    
    GtkStack* stack = (GtkStack*)gtk_builder_get_object(builder, "stack");
    conversationContainerStack = stack;

    GtkButton* newChatButton = (GtkButton*)gtk_builder_get_object(builder, "chatButton");
    GtkWindow* window2 = GTK_WINDOW(gtk_builder_get_object(builder, "window2"));

    g_signal_connect(window, "close-request", (GCallback)closeMainRequest, window2);    
    g_signal_connect(newChatButton, "clicked", (GCallback)openSecondaryWindow, window2);

    addSubnetPeerButtons = (GtkBox*)gtk_builder_get_object(builder, "addSubnetPeerButtons");

    return window;
}


GtkWidget* addNewSubnetPeer(deviceid_t peerId){
    deviceid_t* peerIdAlloc = new deviceid_t;
    *peerIdAlloc = peerId;

    const char* stringifiedPeerId = stringifyId(peerId);
    GtkWidget* addContactButton = gtk_button_new_with_label(stringifiedPeerId);
    gtk_widget_set_hexpand(addContactButton, true);
    g_signal_connect(addContactButton, "clicked", (GCallback)connectionClicked, peerIdAlloc);

    gtk_box_append(addSubnetPeerButtons, addContactButton);
    return addContactButton;
}

void removeSubnetPeer(GtkWidget* toRemove){
    gtk_box_remove(addSubnetPeerButtons, toRemove);
}

char* getFullText(GtkTextBuffer* buffer){
    GtkTextIter* start = new GtkTextIter;
    GtkTextIter* end = new GtkTextIter;
    gtk_text_buffer_get_bounds(buffer, start, end);

    char* text = gtk_text_buffer_get_text(buffer, start, end, false);
    char* toDelete = text;
    text[strlen(text)-1] = '\0';

    char* temp = text;
    while(*temp == ' '){
        temp += 1;
    }
    text = temp;

    temp = text + strlen(text);
    while(temp != text && *(temp-1) == ' '){
        temp -= 1;
    }
    *temp = '\0';

    char* processedText = new char[strlen(text)+1];
    strcpy(processedText, text);

    std::cout << "Processed text: " <<  processedText << '\n';

    free(toDelete);


    return processedText;
}




void sendMessage(deviceid_t id, deviceid_t peerId, const char* fullText){
    size_t sizeOfMsg = sizeof(short)*2+sizeof(deviceid_t)*2+sizeof(char)*(strlen(fullText)+1);

    void* message = operator new(sizeof(size_t) + sizeOfMsg);
    void* toSend = message;

    *(size_t*)message = sizeOfMsg;
    message = (size_t*)message + 1;
    *(short*)message = 0;
    message = (short*)message + 1;
    *(short*)message = 2;
    message = (short*)message + 1;
    *(deviceid_t*)message = id;
    message = (deviceid_t*)message + 1;
    *(deviceid_t*)message = peerId;
    message = (deviceid_t*)message + 1;
    strcpy((char*)message, fullText);

    write(writingFifo, toSend, sizeof(size_t) + sizeOfMsg);

    operator delete(toSend);
}
void displaySentMessage(deviceid_t peerId, const char* message){
    GtkFrame* messageFrame = newSentMessage((const char*)message);
    gtk_box_append(messageBoxes[peerId], (GtkWidget*)messageFrame);
}

void processCharacterInserted(GtkTextBuffer* self, const GtkTextIter* location, gchar* text, gint len, gpointer user_data){
    std::cout << "length: " << len << '\n';
    if(text[0] == '\n'){
        char* fullText = getFullText(self);
        if(strcmp(fullText, "") && strlen(fullText) <= messageLimit){
            gtk_text_buffer_set_text(self, "", 0);
            
	    deviceid_t id = (deviceid_t)getuid();
            deviceid_t peerId = *(deviceid_t*)user_data;

	    sendMessage(id, peerId, fullText);
	    displaySentMessage(peerId, fullText);
	}
    }
}


void setupInsertTextCallback(GtkTextBuffer* inputTextBuffer, deviceid_t peerId){
    deviceid_t* peerIdAlloc = new deviceid_t;
    *peerIdAlloc = peerId;
    g_signal_connect_after(inputTextBuffer, "insert-text", (GCallback)processCharacterInserted, peerIdAlloc);
}

void displayIncomingMessage(deviceid_t peerId, const char* actualMessage){
    GtkFrame* incomingMessage = newIncomingMessage((const char*)actualMessage);
    gtk_box_append(messageBoxes[peerId], (GtkWidget*)incomingMessage);
}

void retrieveMessages(deviceid_t peerId){
    const char* messageFilePath = getMessageFilePath((deviceid_t)getuid(), peerId);
    int fd = open(messageFilePath, O_RDONLY);
    handleError(fd);

    void* metadata = operator new(metadataSize);
    void* toRemove = metadata;
    int bytesRead = read(fd, metadata, metadataSize);
    handleError(bytesRead);
    while(bytesRead > 0){
	bool localSentMessage = *(bool*)metadata;
	metadata = (bool*)metadata + 1;
	//the time won't be use for now
	metadata = (time_t*)metadata + 1;
	int messageSize = *(int*)metadata;

	char* message = new char[messageSize+1];
	int success = read(fd, message, messageSize);
	handleError(success);

	message[messageSize] = '\0';
	std::cout << message << '\n';
	if(localSentMessage)
	    displaySentMessage(peerId, (const char*)message);
	else
	    displayIncomingMessage(peerId, (const char*)message);
	delete[] message;

	metadata = toRemove;
	bytesRead = read(fd, metadata, metadataSize);
        handleError(bytesRead);
	std::cout << bytesRead << '\n';
    }
    operator delete(toRemove);
}

void addNewContact(idToWidg &subnetPeers, deviceid_t peerId){
    const char* stringifiedPeerId = stringifyId(peerId);
    //building the conversation from xml file, making sure to setup everything
    GtkBuilder* conversationBuilder = gtk_builder_new_from_file(XML_CONVERSATION_GUI_FILE);

    GtkBox* conversationContainer = (GtkBox*)gtk_builder_get_object(conversationBuilder, "conversationContainer");
    gtk_stack_add_titled(conversationContainerStack, (GtkWidget*)conversationContainer, stringifiedPeerId, stringifiedPeerId);

    GtkBox* messageBox = (GtkBox*)gtk_builder_get_object(conversationBuilder, "messageBox");
    messageBoxes[peerId] = messageBox;

    GtkTextBuffer* inputTextBuffer = (GtkTextBuffer*)gtk_builder_get_object(conversationBuilder, "inputTextBuffer");
    setupInsertTextCallback(inputTextBuffer, peerId);


    removeSubnetPeer(subnetPeers[peerId]);
    retrieveMessages(peerId);
    subnetPeers.erase(peerId);

    std::cout << "Accepted new contact\n";
}



bool checkForMessages(deviceid_t peerId){
    deviceid_t userId = (deviceid_t)getuid();
    const char* stringifiedPeerId = stringifyId(peerId);

    const char* messageDirPath = getMessageDirectoryPath(userId);
    DIR* messageDir = opendir(messageDirPath);
    if(messageDir == nullptr) handleError(-1);

    bool fileExists = false;
    errno = 0;
    struct dirent* direc_ent = readdir(messageDir);
    while(direc_ent != nullptr){
	if(strcmp(direc_ent->d_name, stringifiedPeerId) == 0){
	    fileExists = true;
	    break;
	}
	direc_ent = readdir(messageDir);
    }
    if(errno != 0) handleError(-1);

    return fileExists;
}

void processMessage(void* message){
    static idToWidg subnetPeers; //includes non-accepted peers ONLY

    short method = *(short*)message;
    message = (short*)message+1;

    deviceid_t peerId = *(deviceid_t*)message;
    std::cout << "Peer id is: " << peerId << '\n';
    message = (deviceid_t*)message + 1;

    std::cout << method << " came in!\n";

    switch(method){
        case 0://requested contact
            {
                //will be implemented later on
                break;
            }
        case 1:
            //new message
            {
                char* actualMessage = new char[messageLimit+1];
                strcpy(actualMessage, (const char*)message);
		displayIncomingMessage(peerId, actualMessage);

                std::cout << "New message\n";

                break;
            }
            
        case 2:
            //new device on subnet
            {
                subnetPeers[peerId] = addNewSubnetPeer(peerId);
		bool isContact = checkForMessages(peerId);
		if(isContact)
		    addNewContact(subnetPeers, peerId);
		
                std::cout << "New device on subnet\n";
                break;
            }
        case 3:
            //someone accepted your connection
            {
		addNewContact(subnetPeers, peerId);
                break;
            }

        case 4:
            //left device
            {
                removeSubnetPeer(subnetPeers[peerId]);
                subnetPeers.erase(peerId);

		const char* stringifiedPeerId = stringifyId(peerId);

                GtkWidget* conversationContainer = gtk_stack_get_child_by_name(conversationContainerStack, stringifiedPeerId);
                if(conversationContainer != NULL){
                    gtk_stack_remove(conversationContainerStack, conversationContainer);
                }

		messageBoxes.erase(peerId);

                std::cout << "A contact has left\n";

                break;
            }
    }
}

void sendOpenedMessage(){
    size_t sizeOfMsg = sizeof(short)*2+sizeof(deviceid_t);

    void* message = operator new(sizeof(size_t) + sizeOfMsg);
    void* toSend = message;

    *(size_t*)message = sizeOfMsg;
    message = (size_t*)message + 1;
    *(short*)message = 0;
    message = (short*)message + 1;
    *(short*)message = 0;
    message = (short*)message + 1;
    *(deviceid_t*)message = (deviceid_t)getuid();

    int success = write(writingFifo, toSend, sizeof(size_t) + sizeOfMsg);
    handleError(success);

    operator delete(toSend);
}

const char* getHomeDirPath(){
    struct passwd* user = getpwuid(getuid());
    return user->pw_dir;
}


gboolean fifoCallback(gint fd, GIOCondition condition, gpointer user_data){
    if(condition & G_IO_IN){
        size_t sizeOfMsg;
        int success = read(fd, (void*)(&sizeOfMsg), sizeof(size_t));
        std::cout << "Bytes intended to be read: " << sizeOfMsg << '\n';
        if(success > 0){
            void* message = operator new(sizeOfMsg);
            int readingSuccess = read(fd, message, sizeOfMsg);
            handleError(readingSuccess);
            std::cout << "Bytes read: " << readingSuccess << '\n';
            processMessage(message);
            operator delete(message);
        }
        else{
            std::cerr << "The fifo has read with 0 bytes\n";
        }
    }
    else{//error
        if(condition & (G_IO_ERR | G_IO_HUP)){
            int closingSuccess = close(fd);
            handleError(closingSuccess);
        }
        std::cerr << "Error on fifo fd!";

        return false;
    }
    std::cout << "--------------------------\n";
    return true;
}


GSource* setupFifoSource(int readingFd){
    GSource* fifoSource = g_unix_fd_source_new(readingFd, (GIOCondition)(G_IO_IN | G_IO_ERR | G_IO_HUP | G_IO_NVAL));
    g_source_set_callback(fifoSource, G_SOURCE_FUNC(&fifoCallback), NULL, NULL);
    
    return fifoSource;
}

gboolean stopMainLoop(GtkWindow* self, gpointer user_data){
    g_main_loop_quit((GMainLoop*)user_data);
    return false;
}

void setupClosedWindowCallback(GtkWindow* window, GMainLoop* mainLoop){
    g_signal_connect(window, "close-request", G_CALLBACK(&stopMainLoop), mainLoop);
}


void sendClosingMessage(){
    size_t sizeOfMsg = sizeof(short)*2+sizeof(deviceid_t);

    void* leavingMessage = operator new(sizeof(size_t) + sizeOfMsg);
    void* toSend = leavingMessage;

    *(size_t*)leavingMessage = sizeOfMsg;
    leavingMessage = (size_t*)leavingMessage + 1;
    *(short*)leavingMessage = 0;
    leavingMessage = (short*)leavingMessage + 1;
    *(short*)leavingMessage = 3;
    leavingMessage = (short*)leavingMessage + 1;
    *(deviceid_t*)leavingMessage = (deviceid_t)getuid();

    int finalSuccess = write(writingFifo, toSend, sizeof(size_t) + sizeOfMsg);
    handleError(finalSuccess);

    operator delete(toSend);
}

void retrieveSavedMessages(){
    //for future commits
}

const char* getOwnFifoPath(bool isReading){
    deviceid_t id = (deviceid_t)getuid();
    return getFifoPath(id, isReading);
}

int main(){
    const char* readingFifoPath = getOwnFifoPath(false);
    const char* writingFifoPath = getOwnFifoPath(true);

    
    writingFifo = open(writingFifoPath, O_WRONLY | O_NONBLOCK);
    if(writingFifo == -1){
        //daemon is not active, app is useless
        std::cerr << "Problem opening FIFO with daemon, exiting\n";
        std::cerr << errno << '\n';
        return 1;
    }
    
    int readingFifo = openFifo(readingFifoPath, O_RDONLY | O_NONBLOCK);

    sendOpenedMessage();

    GSource* fifoSource = setupFifoSource(readingFifo);
    GMainContext* mainContext = g_main_context_default();
    g_source_attach(fifoSource, mainContext);

    GtkWindow* mainWindow = startGtk();

    GMainLoop* mainLoop = g_main_loop_new(mainContext, false);

    setupClosedWindowCallback(mainWindow, mainLoop);

    std::cout << "Entering loop\n";
    g_main_loop_run(mainLoop);
    //this is the end of the program, since g_main_loop_run has returned

    sendClosingMessage();

    close(readingFifo);
    close(writingFifo);


    return 0;
}
