#include <gtk/gtk.h>
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
#include "conversations.h"

GtkBox* newConnectionsBox;
GtkStack* conversations;
int writingFifo;


const char* XML_GUI_FILE = "/usr/share/chattingApp/gui.xml";






bool closeSecondaryRequest(GtkWindow* self, gpointer data){
    gtk_widget_set_visible((GtkWidget*)self, false);
    return true;
}

bool closeMainRequest(GtkWindow* self, gpointer data){
    GtkWindow* otherWindow = (GtkWindow*)data;
    gtk_window_close(self);
    gtk_window_close(otherWindow);
    return false;
}

void clicked(GtkButton* self, gpointer data){
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
}




GtkWindow* startGtk(){
    gtk_init();

    GtkBuilder* builder = gtk_builder_new_from_file(XML_GUI_FILE);
    GtkWindow* window = GTK_WINDOW(gtk_builder_get_object(builder, "window"));
    
    GtkStack* stack = (GtkStack*)gtk_builder_get_object(builder, "stack");
    conversations = stack;

    GtkButton* newChatButton = (GtkButton*)gtk_builder_get_object(builder, "chatButton");
    GtkWindow* window2 = GTK_WINDOW(gtk_builder_get_object(builder, "window2"));

    g_signal_connect(window, "close-request", (GCallback)closeMainRequest, window2);    
    g_signal_connect(newChatButton, "clicked", (GCallback)clicked, window2);

    newConnectionsBox = (GtkBox*)gtk_builder_get_object(builder, "newConnectionsBox");

    return window;
}


struct pollfd* newPollFd(int fd){
    struct pollfd* filedes = new struct pollfd;
    filedes->fd = fd;
    filedes->events = POLLIN;
    return filedes;
}

GtkWidget* appendNewConnection(const char* peerId, const char* id){
    GtkWidget* button = gtk_button_new_with_label(peerId);
    gtk_widget_set_hexpand(button, true);
    g_signal_connect(button, "clicked", (GCallback)connectionClicked, (void*)id);

    gtk_box_append(newConnectionsBox, button);
    return button;
}

void removeNewConnection(GtkWidget* toRemove){
    gtk_box_remove(newConnectionsBox, toRemove);
}


void insert_text (GtkTextBuffer* self, const GtkTextIter* location, gchar* text, gint len, gpointer user_data){
    if(text[0] == '\n'){
        char* fullText = getFullText(self);
        if(fullText != "" && strlen(fullText) <= 100){
            gtk_text_buffer_set_text(self, "", 0);
            void* message = operator new(MAX_SIZE);
            void* toSend = message;
            const char* id = (const char*)user_data;
            const char* peerId = (const char*)id + 11;

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
        }
    }
}


void processMessage(void* message, const char* id){
    static std::map<const char*, GtkWidget*> newConnections;
    static std::map<const char*, GtkBox*> messageBoxes;

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
                break;
            }
            
        case 2:
            //new device on subnet
            {
                newConnections[peerId] = appendNewConnection(peerId, id);
                break;
            }
        case 3:
            //someone accepted your connection
            {
                GtkBox* messageBox = newMessageBox();
                messageBoxes[peerId] = messageBox;

                setConversation(conversations, messageBox, id, peerId);

                removeNewConnection(newConnections[peerId]);
                newConnections.erase(peerId);

                break;
            }

        case 4:
            //left device
            {
                removeNewConnection(newConnections[peerId]);
                newConnections.erase(peerId);


                GtkWidget* conversation = gtk_stack_get_child_by_name(conversations, peerId);
                if(conversation != NULL){
                    gtk_stack_remove(conversations, conversation);
                }

                messageBoxes[peerId] = nullptr;
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
        std::err << "Error on fifo fd!";

        return false;
    }
    return true;
}


GSource* setupFifoSource(int readingFd, const char* id){
    GSource* fifoSource = g_unix_fd_source_new(readingFd, G_IO_IN | G_IO_ERR | G_IO_HUP | G_IO_NVAL);
    g_source_set_callback(fifoSource, G_SOURCE_FUNC(&fifoCallback), id, NULL);
    
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