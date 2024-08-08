#ifndef DESKTOP_TYPES
#define DESKTOP_TYPES

#include <map>
#include <gtk/gtk.h>

#include "../common/cmpFuncs.h"


typedef std::map<const char*, GtkWidget*, cmp_str> chToWidg; 
typedef std::map<const char*, GtkBox*, cmp_str> chToBox;

#endif