// When working in C including cimgui.h without the CIMGUI_DEFINE_ENUMS_AND_STRUCTS define is
// worthless since without the define cimgui.h will attempt to use c++ syntax for... reasons...

#define CIMGUI_DEFINE_ENUMS_AND_STRUCTS
#include <cimgui.h>