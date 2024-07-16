#include "conversations.h"

#include <gtk/gtk.h>


GtkTextView* newTextView(const char* message){
    GtkTextBuffer* textBuffer = gtk_text_buffer_new(NULL);
    gtk_text_buffer_set_text(textBuffer, message, strlen(message));

    GtkTextView* textView = (GtkTextView*)gtk_text_view_new_with_buffer(textBuffer);
    gtk_text_view_set_cursor_visible(textView, false);
    gtk_text_view_set_editable(textView, false);

    return textView;
}


GtkTextView* newTextViewEditable(){
    GtkTextBuffer* textBuffer = gtk_text_buffer_new(NULL);
    gtk_text_buffer_set_enable_undo(textBuffer, false);

    GtkTextView* textView = (GtkTextView*)gtk_text_view_new_with_buffer(textBuffer);

    return textView;
}




GtkFrame* newFrame(){
    return (GtkFrame*)gtk_frame_new(NULL);
}


GtkFrame* newMessage(const char* message){
    GtkTextView* textView = newTextView(message);

    GtkFrame* frame = newFrame();
    gtk_frame_set_child(frame, (GtkWidget*)textView);

    return frame;
}



GtkBox* newMessageBox(){
    GtkBox* messageBox = (GtkBox*)gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    return messageBox;
}




GtkScrolledWindow* newMessageWindow(GtkBox* messageBox){
    GtkScrolledWindow* messagesWindow = (GtkScrolledWindow*)gtk_scrolled_window_new();
    gtk_scrolled_window_set_child(messagesWindow, (GtkWidget*) messageBox);

    return messagesWindow;
}

char* getFullText(GtkTextBuffer* buffer){
    GtkTextIter* start = new GtkTextIter;
    GtkTextIter* end = new GtkTextIter;
    gtk_text_buffer_get_bounds(buffer, start, end);

    char* text = gtk_text_buffer_get_text(buffer, start, end, false);
    text[strlen(text)-1] = ' ';
    for(int i = 0; i < strlen(text)-1; i++){
        if(text[i] == ' ') text[i] = '\0';
        else break;
    }
    for(int i = strlen(text)-1; i > 0; i--){
        if(text[i] == ' ') text[i] = '\0';
        else break;
    }


    return text;
}





GtkFrame* newMessageEntry(const char* id, const char* peerId){
    GtkFrame* frame = (GtkFrame*)gtk_frame_new(NULL);

    GtkScrolledWindow* scrollWindow = (GtkScrolledWindow*)gtk_scrolled_window_new();
    gtk_frame_set_child(frame, (GtkWidget*)scrollWindow);

    GtkTextView* messageTextEntry = newTextViewEditable();
    gtk_text_view_set_wrap_mode(messageTextEntry, GTK_WRAP_WORD_CHAR);
    gtk_scrolled_window_set_child(scrollWindow, (GtkWidget*)messageTextEntry);

    GtkTextBuffer* textBuffer = gtk_text_view_get_buffer(messageTextEntry);
    void* parameter = operator new(sizeof(char)*22);
    strcpy((char*)parameter, id);
    strcpy((char*)parameter, peerId);
    g_signal_connect_after(textBuffer, "insert-text", (GCallback)insert_text, parameter);
    
    
    gtk_widget_set_vexpand((GtkWidget*)scrollWindow, true);
    gtk_widget_set_valign((GtkWidget*)scrollWindow, GTK_ALIGN_CENTER);
    gtk_widget_set_valign((GtkWidget*)frame, GTK_ALIGN_END);

    gtk_widget_set_margin_start((GtkWidget*)messageTextEntry, 3);
    gtk_widget_set_margin_end((GtkWidget*)messageTextEntry, 3);
    gtk_widget_set_margin_bottom((GtkWidget*)messageTextEntry, 3);
    

    return frame;
}


GtkBox* newConversation(GtkBox* messageBox, const char* id, const char* peerId){
    GtkBox* mainBox = (GtkBox*)gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    GtkScrolledWindow* messageWindow = newMessageWindow(messageBox);

    gtk_box_append(mainBox, (GtkWidget*)messageWindow);
    
    GtkFrame* messageEntry = newMessageEntry(id, peerId);
    gtk_box_append(mainBox, (GtkWidget*)messageEntry);


    return mainBox;
}




GtkFrame* newIncomingMessage(const char* message){
    GtkFrame* messageFrame = newMessage(message);
    gtk_widget_set_halign((GtkWidget*)messageFrame, GTK_ALIGN_START);
    gtk_widget_set_margin_start((GtkWidget*)messageFrame, 20);

    return messageFrame;
}

GtkFrame* newSentMessage(const char* message){
    GtkFrame* messageFrame = newMessage(message);
    gtk_widget_set_halign((GtkWidget*)messageFrame, GTK_ALIGN_END);
    gtk_widget_set_margin_start((GtkWidget*)messageFrame, 20);

    return messageFrame;
}



void setConversation(GtkStack* stack, GtkBox* messageBox, const char* id, const char* peerId){
    GtkBox* conversation = newConversation(messageBox, id, peerId);

    gtk_stack_add_titled(stack, (GtkWidget*)conversation, peerId, peerId);
}