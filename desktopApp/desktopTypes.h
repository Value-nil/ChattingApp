#ifndef DESKTOP_TYPES
#define DESKTOP_TYPES

#include <map>
#include <gtk/gtk.h>

#include "../common/types.h"


typedef std::map<deviceid_t, GtkWidget*> idToWidg; 
typedef std::map<deviceid_t, GtkBox*> idToBox;

#endif
