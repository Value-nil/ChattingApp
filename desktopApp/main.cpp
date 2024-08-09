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

#include "../common/utilities.h"
#include "../common/constants.h"
#include "desktopTypes.h"

GtkBox* addSubnetPeerButtons;
GtkStack* conversationContainerStack;
int writingFifo;


const char* XML_MAIN_GUI_FILE = "/usr/share/chattingApp/guiFiles/mainGui.xml";
const char* XML_CONVERSATION_GUI_FILE = "/usr/share/chattingApp/guiFiles/conversationContainerGui.xml";
const char* XML_MESSAGE_GUI_FILE = "/usr/share/chattingApp/guiFiles/message.xml";






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
    
    const char* peerId = gtk_button_get_label(self);

    void* toWrite = operator new(sizeof(short)*2+sizeof(char)*22);
    void* message = toWrite;
    
    *(short*)toWrite = 0;
    toWrite = (short*)toWrite+1;
    *(short*)toWrite = 1;
    toWrite = (short*)toWrite+1;
    strcpy((char*)toWrite, (const char*)data);
    toWrite = (char*)toWrite + 11;
    strcpy((char*)toWrite, peerId);
    

    int success = write(writingFifo, message, sizeof(short)*2+sizeof(char)*22);
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


GtkWidget* addNewSubnetPeer(const char* peerId, const char* id){
    char* sendingId = new char[11];
    strcpy(sendingId, id);

    GtkWidget* addContactButton = gtk_button_new_with_label(peerId);
    gtk_widget_set_hexpand(addContactButton, true);
    g_signal_connect(addContactButton, "clicked", (GCallback)connectionClicked, sendingId);

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

    free(toDelete);


    return processedText;
}




void sendMessage(const char* id, const char* peerId, const char* fullText){
    void* message = operator new(MAX_SIZE);
    void* toSend = message;

    *(short*)message = 0;
    message = (short*)message + 1;
    *(short*)message = 2;
    message = (short*)message + 1;
    strcpy((char*)message, id);
    message = (char*)message + 11;
    strcpy((char*)message, peerId);
    message = (char*)message + 11;
    strcpy((char*)message, fullText);

    write(writingFifo, toSend, MAX_SIZE);

    operator delete(toSend);
}

void processCharacterInserted(GtkTextBuffer* self, const GtkTextIter* location, gchar* text, gint len, gpointer user_data){
    if(text[0] == '\n'){
        char* fullText = getFullText(self);
        if(strcmp(fullText, "") && strlen(fullText) <= 100){
            gtk_text_buffer_set_text(self, "", 0);
            
            const char* id = (const char*)user_data;
            const char* peerId = (const char*)id + 11;

			sendMessage(id, peerId, fullText);
        }
    }
}


void setupInsertTextCallback(GtkTextBuffer* inputTextBuffer, const char* id, const char* peerId){
    char* ids = new char[22];
    char* toSend = ids;

    strcpy(ids, id);
    ids += 11;
    strcpy(ids, peerId);

    g_signal_connect_after(inputTextBuffer, "insert-text", (GCallback)processCharacterInserted, (void*)toSend);
}



void processMessage(void* message, const char* id){
    static chToWidg subnetPeers; //includes non-accepted peers ONLY
    static chToBox messageBoxes;

    short method = *(short*)message;
    message = (short*)message+1;

    char* peerId = new char[11];
    strcpy(peerId, (const char*)message);
    message = (char*)message + 11;

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
                char* actualMessage = new char[101];
                strcpy(actualMessage, (const char*)message);

                GtkFrame* incomingMessage = newIncomingMessage((const char*)actualMessage);
                gtk_box_append(messageBoxes[peerId], (GtkWidget*)incomingMessage);

                std::cout << "New message\n";

                break;
            }
            
        case 2:
            //new device on subnet
            {
                subnetPeers[peerId] = addNewSubnetPeer(peerId, id);
                std::cout << "New device on subnet\n";
                break;
            }
        case 3:
            //someone accepted your connection
            {
                //building the conversation from xml file, making sure to setup everything
                GtkBuilder* conversationBuilder = gtk_builder_new_from_file(XML_CONVERSATION_GUI_FILE);

                GtkBox* conversationContainer = (GtkBox*)gtk_builder_get_object(conversationBuilder, "conversationContainer");
                gtk_stack_add_titled(conversationContainerStack, (GtkWidget*)conversationContainer, peerId, peerId);

                GtkBox* messageBox = (GtkBox*)gtk_builder_get_object(conversationBuilder, "messageBox");
                messageBoxes[peerId] = messageBox;

                GtkTextBuffer* inputTextBuffer = (GtkTextBuffer*)gtk_builder_get_object(conversationBuilder, "inputTextBuffer");
                setupInsertTextCallback(inputTextBuffer, id, peerId);


                removeSubnetPeer(subnetPeers[peerId]);
                subnetPeers.erase(peerId);

                std::cout << "Accepted new contact\n";

                break;
            }

        case 4:
            //left device
            {
                removeSubnetPeer(subnetPeers[peerId]);
                subnetPeers.erase(peerId);


                GtkWidget* conversationContainer = gtk_stack_get_child_by_name(conversationContainerStack, peerId);
                if(conversationContainer != NULL){
                    gtk_stack_remove(conversationContainerStack, conversationContainer);
                }

                messageBoxes[peerId] = nullptr;

                std::cout << "A contact has left\n";

                break;
            }
    }
}

void sendOpenedMessage(const char* id){
    void* message = operator new(sizeof(uid_t)+sizeof(short)*2+sizeof(char)*11);
    void* toSend = message;

    *(short*)message = 0;
    message = (short*)message + 1;
    *(short*)message = 0;
    message = (short*)message + 1;
    strcpy((char*)message, id);
    message = (char*)message + 11;
    *(uid_t*)message = getuid();

    int success = write(writingFifo, toSend, sizeof(uid_t)+sizeof(short)*2+sizeof(char)*11);
    handleError(success);

    operator delete(toSend);
}

const char* getHomeDirPath(){
    struct passwd* user = getpwuid(getuid());
    return user->pw_dir;
}

const char* retrieveId(){
    char* id = new char[11];

    const char* homeDirPath = getHomeDirPath();
    char* baseDirPath = buildPath(homeDirPath, INITIAL_DIR_PATH);
    char* idPath = buildPath(baseDirPath, ID_PATH);
    delete[] baseDirPath;

    int fd = open(idPath, O_RDONLY);
    handleError(fd);

    delete[] idPath;

    int readSuccess = read(fd, id, sizeof(char)*11);//created by daemon, should alr come with null terminator
    handleError(readSuccess);

    close(fd);

    return (const char*)id;
}

gboolean fifoCallback(gint fd, GIOCondition condition, gpointer user_data){
    if(condition & G_IO_IN){
        void* message = operator new(sizeof(short)+sizeof(char)*123);
        read(fd, message, sizeof(short)+ sizeof(char)*123);
        processMessage(message, (const char*)user_data);//user_data is the id of the user

        operator delete(message);
    }
    else{//error
        if(condition & (G_IO_ERR | G_IO_HUP)){
            int closingSuccess = close(fd);
            handleError(closingSuccess);
        }
        std::cerr << "Error on fifo fd!";

        return false;
    }
    return true;
}


GSource* setupFifoSource(int readingFd, const char* id){
    char* sendingId = new char[11];
    strcpy(sendingId, id);

    GSource* fifoSource = g_unix_fd_source_new(readingFd, (GIOCondition)(G_IO_IN | G_IO_ERR | G_IO_HUP | G_IO_NVAL));
    g_source_set_callback(fifoSource, G_SOURCE_FUNC(&fifoCallback), sendingId, NULL);
    
    return fifoSource;
}

gboolean stopMainLoop(GtkWindow* self, gpointer user_data){
    g_main_loop_quit((GMainLoop*)user_data);
    return false;
}

void setupClosedWindowCallback(GtkWindow* window, GMainLoop* mainLoop){
    g_signal_connect(window, "close-request", G_CALLBACK(&stopMainLoop), mainLoop);
}


void sendClosingMessage(const char* id){
    void* leavingMessage = operator new(sizeof(short)*2+sizeof(char)*11);
    void* toSend = leavingMessage;

    *(short*)leavingMessage = 0;
    leavingMessage = (short*)leavingMessage + 1;
    *(short*)leavingMessage = 3;
    leavingMessage = (short*)leavingMessage + 1;
    strcpy((char*)leavingMessage, id);

    int finalSuccess = write(writingFifo, toSend, sizeof(short)*2+sizeof(char)*11);
    handleError(finalSuccess);

    operator delete(toSend);
}

int main(){
    const char* id = retrieveId();
    struct passwd* user = getpwuid(getuid());

    const char* readingFifoPath = getFifoPath(user, D_TO_A_PATH);
    const char* writingFifoPath = getFifoPath(user, A_TO_D_PATH);

    
    writingFifo = open(writingFifoPath, O_WRONLY | O_NONBLOCK);
    if(writingFifo == -1){
        //daemon is not active, app is useless
        std::cerr << "Problem opening FIFO with daemon, exiting\n";
        std::cerr << errno << '\n';
        return 1;
    }
    
    int readingFifo = open(readingFifoPath, O_RDONLY | O_NONBLOCK);
    handleError(readingFifo);

    sendOpenedMessage(id);

    GSource* fifoSource = setupFifoSource(readingFifo, id);
    GMainContext* mainContext = g_main_context_default();
    g_source_attach(fifoSource, mainContext);


    GtkWindow* mainWindow = startGtk();

    GMainLoop* mainLoop = g_main_loop_new(mainContext, false);

    setupClosedWindowCallback(mainWindow, mainLoop);

    std::cout << "Entering loop\n";
    g_main_loop_run(mainLoop);

    //this is the end of the program, since g_main_loop_run has returned
    sendClosingMessage(id);

    close(readingFifo);
    close(writingFifo);


    return 0;
}