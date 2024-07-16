#ifndef CONVERSATION
#define CONVERSATION

#include <gtk/gtk.h>

void setConversation(GtkStack* stack, GtkBox* messageBox, const char* id, const char* peerId);
GtkFrame* newSentMessage(const char* message);
GtkFrame* newIncomingMessage(const char* message);
GtkBox* newMessageBox();
GtkScrolledWindow* newMessageWindow(GtkBox* messageBox);
void insert_text (GtkTextBuffer* self, const GtkTextIter* location, gchar* text, gint len, gpointer user_data);
char* getFullText(GtkTextBuffer* buffer);




#endif